/*
* xenbus.c
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System Configuration functions.
*
* Revision History:
* -----------------
* 011, 11/10/2020 Lili create this file
*/

#include <linux/of_device.h>
#include <xen/xenbus.h>
#include <xen/page.h>
#include <xen/events.h>
#include <linux/vmalloc.h>

#include "xengpu.h"
#include "xengpuback.h"

static int gpuback_remove(struct xenbus_device *dev);

static int xengpu_connect_ring(struct backend_info *be, grant_ref_t ring_ref,
				unsigned int evtchn)
{
	struct xenbus_device *dev = be->dev;
	struct xen_gpuif *vgpuif = be->gpuif;
	void *addr;
	struct gpuif_sring *shared;
	int err;

	err = xenbus_map_ring_valloc(dev, &ring_ref, 1, &addr);
	if (err)
		goto err;

	shared = (struct gpuif_sring *)addr;
	BACK_RING_INIT(&vgpuif->back_ring, shared, XEN_PAGE_SIZE);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	err = bind_interdomain_evtchn_to_irq_lateeoi(vgpuif->domid, evtchn);
#else
	err = bind_interdomain_evtchn_to_irq(vgpuif->domid, evtchn);
#endif
	if (err < 0)
		goto err_unmap;

	vgpuif->gpuif_irq = err;

	err = request_threaded_irq(vgpuif->gpuif_irq, NULL, xen_gpuif_irq_fn,
				   IRQF_ONESHOT, "xen-gpuback", vgpuif);
	if (err) {
		pr_warn("Could not setup irq handler for gpu back\n");
		goto err_deinit;
	}

	return 0;

err_deinit:
	unbind_from_irqhandler(vgpuif->gpuif_irq, vgpuif);
	vgpuif->gpuif_irq = 0;
err_unmap:
	xenbus_unmap_ring_vfree(dev, shared);
err:
	return err;
}

static int backend_connect(struct backend_info *be)
{
	struct xenbus_device *dev = be->dev;
	unsigned int val;
	grant_ref_t ring_ref;
	unsigned int evtchn;
	int err;

	err = xenbus_scanf(XBT_NIL, dev->otherend,
			"gpu-ring-ref", "%u", &val);
	if (err < 0)
		goto done; /* The frontend does not have a control ring */

	ring_ref = val;

	err = xenbus_scanf(XBT_NIL, dev->otherend,
			"event-channel-gpu", "%u", &val);
	if (err < 0) {
		xenbus_dev_fatal(dev, err,
				"reading %s/event-channel-gpu",
				dev->otherend);
		goto fail;
	}

	evtchn = val;

	err = xengpu_connect_ring(be, ring_ref, evtchn);
	if (err) {
		xenbus_dev_fatal(dev, err,
				"mapping shared-frame %u port %u",
				ring_ref, evtchn);
		goto fail;
	}

done:
	return 0;

fail:
	return err;
}

static void backend_disconnect(struct backend_info *be)
{
	struct xen_gpuif *vgpuif = be->gpuif;

	if (vgpuif) {
		if (vgpuif->gpuif_irq) {
			unbind_from_irqhandler(vgpuif->gpuif_irq, vgpuif);
			vgpuif->gpuif_irq = 0;
		}

		if (vgpuif->back_ring.sring) {
			xenbus_unmap_ring_vfree(be->dev, vgpuif->back_ring.sring);
		}
	}
}


/*
 * Callback received when the frontend's state changes.
 */
static void gpu_frontend_changed(struct xenbus_device *dev,
				enum xenbus_state frontend_state)
{
	struct backend_info *be = dev_get_drvdata(&dev->dev);
	int err;

	pr_err("in %s %p front(%s) back(%s)\n",
		__func__,
		dev,
		xenbus_strstate(frontend_state),
		xenbus_strstate(dev->state));

	switch (frontend_state) {
		case XenbusStateInitialising:
			if (dev->state == XenbusStateClosed) {
				pr_info("%s: prepare for reconnect\n", dev->nodename);
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

			//backend_disconnect(be);

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

	pr_err("out %s %p front(%s) back(%s)\n",
		__func__,
		dev,
	xenbus_strstate(frontend_state),
	xenbus_strstate(dev->state));
}


static void xen_gpuif_destroy(struct xen_gpuif *gpuif)
{
	if (gpuif) {
		kfree(gpuif);
	}
	return;
}


static struct xen_gpuif *xen_gpuif_alloc(domid_t domid)
{
	struct xen_gpuif *gpuif;

	//gpuif = kmem_cache_zalloc(xen_gpuif_cachep, GFP_KERNEL);
	gpuif = kzalloc(sizeof(struct xen_gpuif), GFP_KERNEL);
	if (!gpuif)
		return ERR_PTR(-ENOMEM);

	gpuif->domid = domid;
	return gpuif;
}


/**
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures and switch to InitWait.
 */
static int gpuback_probe(struct xenbus_device *dev,
			 const struct xenbus_device_id *id)
{
	int err;
	struct backend_info *be = kzalloc(sizeof(struct backend_info),
					GFP_KERNEL);
	if (!be) {
		xenbus_dev_fatal(dev, -ENOMEM,
				 "allocating backend structure");
		VMM_DEBUG_PRINT("%s, allocating backend structure\n", __func__);
		return -ENOMEM;
	}

	be->dev = dev;
	be->gpuif = xen_gpuif_alloc(dev->otherend_id);
	if (IS_ERR(be->gpuif)) {
		err = PTR_ERR(be->gpuif);
		be->gpuif = NULL;
		xenbus_dev_fatal(dev, err, "creating gpu interface");
		goto fail;
	}

	be->gpuif->be = be;
	dev_set_drvdata(&dev->dev, be);

	err = xenbus_switch_state(dev, XenbusStateInitWait);
	if (err)
		goto fail;

	VMM_DEBUG_PRINT("%s:%d, finished\n", __func__, __LINE__);

	return 0;

fail:
	VMM_DEBUG_PRINT("%s:%d, failed\n", __func__, __LINE__);
	gpuback_remove(dev);
	return err;
}


static int gpuback_remove(struct xenbus_device *dev)
{
	struct backend_info *be = dev_get_drvdata(&dev->dev);

	xenbus_switch_state(dev, XenbusStateClosed);
	if (be) {
		backend_disconnect(be);
	}
	xen_gpuif_destroy(be->gpuif);
	kfree(be);
	dev_set_drvdata(&dev->dev, NULL);
	VMM_DEBUG_PRINT("%s:%d, finished\n", __func__, __LINE__);

	return 0;
}



static const struct xenbus_device_id gpuback_ids[] = {
	{ "vgpu" },
	{ "" }
};

static struct xenbus_driver gpuback_driver = {
	.ids = gpuback_ids,
	.probe = gpuback_probe,
	.remove = gpuback_remove,
	.otherend_changed = gpu_frontend_changed,
};

int xengpu_xenbus_init(void)
{
	VMM_DEBUG_PRINT("%s:%d, will call xenbus_register_backend",
		__FUNCTION__,
		__LINE__);

	return xenbus_register_backend(&gpuback_driver);
}

void xengpu_xenbus_fini(void)
{
	return xenbus_unregister_driver(&gpuback_driver);
}

