#include <linux/of_device.h>
#include <linux/module.h>
#include <xen/xenbus.h>
#include <xen/page.h>
#include <xen/events.h>
#include <linux/vmalloc.h>
#include <linux/soc/semidrive/xenipc.h>

struct xenipc_be_channel {
	/* Unique identifier for this interface. */
	domid_t                 domid;
	/* Back pointer to the xenipc_backend. */
	struct xenipc_backend   *be;
	struct xenipc_back_ring back_ring;
	unsigned int xirq;
};

struct xenipc_backend {
	struct xenbus_device    *dev;
	struct xenipc_be_channel *bechan;
	unsigned                major;
	unsigned                minor;
	char                    *mode;
};

static int xenipc_back_remove(struct xenbus_device *dev);
int rpmsg_rpc_call_trace(int dev, struct rpc_req_msg *, struct rpc_ret_msg *);

static void process_front_request(struct xenipc_be_channel *chan, const struct xenipc_request *req)
{
	RING_IDX idx = chan->back_ring.rsp_prod_pvt;
	int notify;
	struct xenipc_respond * rsp = RING_GET_RESPONSE(&chan->back_ring, idx);
	struct rpc_ret_msg result;
	struct rpc_req_msg request;
	int ret = 0;

	DCF_INIT_RPC_REQ(request, req->cmd);
	memcpy(request.param, req->param, sizeof(req->param));
	ret = rpmsg_rpc_call_trace(0, &request, &result);

	rsp->ack = request.cmd;
	rsp->retcode = ret;
	rsp->id = req->id;
	memcpy(rsp->result, result.result, sizeof(rsp->result));

	chan->back_ring.rsp_prod_pvt = ++idx;

	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&chan->back_ring, notify);
	if (notify)
		notify_remote_via_irq(chan->xirq);
}

static bool xenipc_work_todo(struct xenipc_be_channel *chan)
{
 	if (likely(RING_HAS_UNCONSUMED_REQUESTS(&chan->back_ring)))
 		return 1;

 	return 0;
}

static void xenipc_work_handler(struct xenipc_be_channel *chan)
{
	for (;;) {
		RING_IDX req_prod, req_cons;

		req_prod = chan->back_ring.sring->req_prod;
		req_cons = chan->back_ring.req_cons;

		/* Make sure we can see requests before we process them. */
		rmb();

		if (req_cons == req_prod)
			break;

		while (req_cons != req_prod) {
			struct xenipc_request req;

			RING_COPY_REQUEST(&chan->back_ring, req_cons, &req);
			req_cons++;

			process_front_request(chan, &req);
		}

		chan->back_ring.req_cons = req_cons;
		chan->back_ring.sring->req_event = req_cons + 1;
	}
}

irqreturn_t xenipc_irq_handler(int irq, void *data)
{
	struct xenipc_be_channel *chan = data;

	while (xenipc_work_todo(chan))
		xenipc_work_handler(chan);

	return IRQ_HANDLED;
}

int xenipc_connect_ring(struct xenipc_backend *be, grant_ref_t ring_ref,
			unsigned int evtchn)
{
	struct xenbus_device *dev = be->dev;
	struct xenipc_be_channel *chan = be->bechan;
	void *addr;
	struct xenipc_sring *shared;
	int err;

	err = xenbus_map_ring_valloc(dev, &ring_ref, 1, &addr);
	if (err)
		goto err;

	shared = (struct xenipc_sring *)addr;
	BACK_RING_INIT(&chan->back_ring, shared, XEN_PAGE_SIZE);

	err = bind_interdomain_evtchn_to_irq(chan->domid, evtchn);
	if (err < 0)
		goto err_unmap;

	chan->xirq = err;

	err = request_threaded_irq(chan->xirq, NULL, xenipc_irq_handler,
	                           IRQF_ONESHOT, "xenipc-back", chan);
	if (err) {
		dev_err(&dev->dev, "Could not setup irq handler for ipc back\n");
		goto err_deinit;
	}

	return 0;

err_deinit:
	unbind_from_irqhandler(chan->xirq, chan);
	chan->xirq = 0;
err_unmap:
	xenbus_unmap_ring_vfree(dev, chan->back_ring.sring);
err:
	return err;
}

static int backend_connect(struct xenipc_backend *be)
{
	struct xenbus_device *dev = be->dev;
	unsigned int val;
	grant_ref_t ring_ref;
	unsigned int evtchn;
	int err;

	err = xenbus_scanf(XBT_NIL, dev->otherend,
	                   "ipc-ring-ref", "%u", &val);
	if (err < 0)
		goto done; /* The frontend does not have a control ring */

	ring_ref = val;

	err = xenbus_scanf(XBT_NIL, dev->otherend,
	                   "event-channel-ipc", "%u", &val);
	if (err < 0) {
		xenbus_dev_fatal(dev, err, "reading %s/event-channel-ipc",
				dev->otherend);
		goto fail;
	}

	evtchn = val;

	err = xenipc_connect_ring(be, ring_ref, evtchn);
	if (err) {
		xenbus_dev_fatal(dev, err,  "mapping shared-frame %u port %u",
				ring_ref, evtchn);
		goto fail;
	}

done:
	return 0;

fail:
	return err;
}

static void backend_disconnect(struct xenipc_backend *be)
{
	struct xenipc_be_channel *chan = be->bechan;

	if (chan) {
		if (chan->xirq) {
			unbind_from_irqhandler(chan->xirq, chan);
			chan->xirq = 0;
		}

		if (chan->back_ring.sring) {
			xenbus_unmap_ring_vfree(be->dev, chan->back_ring.sring);
			chan->back_ring.sring = NULL;
		}
	}
}

/*
 * Callback received when the frontend's state changes.
 */
static void xenipc_front_changed(struct xenbus_device *dev,
                             enum xenbus_state frontend_state)
{
	struct xenipc_backend *be = dev_get_drvdata(&dev->dev);
	int err;

	dev_info(&dev->dev, "in %s %p front(%s) back(%s)\n",
		 __func__,
		 dev,
		xenbus_strstate(frontend_state),
		xenbus_strstate(dev->state));

	switch (frontend_state) {
	case XenbusStateInitialising:
		if (dev->state == XenbusStateClosed) {
		    dev_info(&dev->dev, "%s: prepare for reconnect\n", dev->nodename);
		    xenbus_switch_state(dev, XenbusStateInitWait);
		}
		break;

	case XenbusStateInitialised:
	case XenbusStateConnected:
		/*
		 * Ensure we connect even when two watches fire in
		 * close succession and we miss the intermediate value
		 * of frontend_state.
		 */
		if (dev->state == XenbusStateConnected)
			break;

		err = backend_connect(be);
		if (err) {
			/*
			 * Clean up so that memory resources can be used by
			 * other devices. connect_ring reported already error.
			 */
			backend_disconnect(be);
			break;
		}
		xenbus_switch_state(dev, XenbusStateConnected);
		break;

	case XenbusStateClosing:
		xenbus_switch_state(dev, XenbusStateClosing);
		break;

	case XenbusStateClosed:
		backend_disconnect(be);
		xenbus_switch_state(dev, XenbusStateClosed);
		if (xenbus_dev_is_online(dev))
			break;
		/* fall through */
		/* if not online */
	case XenbusStateUnknown:
		device_unregister(&dev->dev);
		break;

	default:
		xenbus_dev_fatal(dev, -EINVAL, "saw state %d at frontend",
				frontend_state);
	    break;
	}
}

static void xenipc_free_channel(struct xenipc_be_channel *bechan)
{
	kfree(bechan);
	return;
}


static struct xenipc_be_channel *xenipc_alloc_channel(domid_t domid)
{
	struct xenipc_be_channel *bechan;

	bechan = kzalloc(sizeof(struct xenipc_be_channel), GFP_KERNEL);
	if (!bechan)
		return ERR_PTR(-ENOMEM);

	bechan->domid = domid;
	return bechan;
}

/**
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures and switch to InitWait.
 */
static int xenipc_back_probe(struct xenbus_device *dev,
                         const struct xenbus_device_id *id)
{
	int err;
	struct xenipc_backend *be = kzalloc(sizeof(struct xenipc_backend),
									GFP_KERNEL);
	if (!be) {
		xenbus_dev_fatal(dev, -ENOMEM, "allocating backend structure");
		dev_err(&dev->dev, "allocating backend structure\n");
		return -ENOMEM;
	}

	be->dev = dev;
	be->bechan = xenipc_alloc_channel(dev->otherend_id);
	if (IS_ERR(be->bechan)) {
		err = PTR_ERR(be->bechan);
		be->bechan = NULL;
		xenbus_dev_fatal(dev, err, "creating ipc interface");
		goto fail;
	}

	be->bechan->be = be;
	dev_set_drvdata(&dev->dev, be);

	err = xenbus_switch_state(dev, XenbusStateInitWait);
	if (err)
		goto fail;

	dev_info(&dev->dev, "%s done from domid: %d\n", __func__, dev->otherend_id);

	return 0;

fail:
	dev_err(&dev->dev, "%s: failed\n", __func__);
	xenipc_back_remove(dev);
	return err;
}

static int xenipc_back_remove(struct xenbus_device *dev)
{
	struct xenipc_backend *be = dev_get_drvdata(&dev->dev);

	xenbus_switch_state(dev, XenbusStateClosed);
	if (be) {
		backend_disconnect(be);

		if (be->bechan)
			xenipc_free_channel(be->bechan);

		be->bechan = NULL;
		dev_set_drvdata(&dev->dev, NULL);
		kfree(be);
	}
	return 0;
}

static const struct xenbus_device_id xenipc_be_ids[] = {
	{ "vrpc" },
	{ "" }
};

static struct xenbus_driver xenipc_be_driver = {
	.ids = xenipc_be_ids,
	.probe = xenipc_back_probe,
	.remove = xenipc_back_remove,
	.otherend_changed = xenipc_front_changed,
};

static int __init xenipc_be_init(void)
{
	if (!xen_domain() || !xen_initial_domain())
		return -ENODEV;

	pr_info("register xen ipc backend\n");

	return xenbus_register_backend(&xenipc_be_driver);
}
module_init(xenipc_be_init);

static void __exit xenipc_be_fini(void)
{
	if (!xen_domain() || !xen_initial_domain())
		return;

	xenbus_unregister_driver(&xenipc_be_driver);
	return;
}

module_exit(xenipc_be_fini);

