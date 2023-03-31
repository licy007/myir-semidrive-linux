/*
* xengpufront.c
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System abstraction layer .
*
* Revision History:
* -----------------
* 01, 07/25/2019 Lili create this file
*/
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_ALERT */
#include <linux/semaphore.h>
#include <linux/sched.h>

#include <xen/xen.h>		/* We are doing something with Xen */
#include <xen/xenbus.h>
#include <xen/page.h>
#include <xen/events.h>
#include <xen/grant_table.h>

#include "xengpu.h"
#include "xengpufront.h"
#include "sysconfig.h"

struct xengpu_info gXenGpuInfo={0};

DMA_ALLOC * getDMAallocinfo(void)
{
	return gXenGpuInfo.psDmaAlloc;
}

int gsx_front_unmap(uint32_t func_id, uint32_t dev_id, uint32_t os_id)
{
	struct xengpu_info *drv_info = &gXenGpuInfo;
	struct gpuif_request request;
	struct gpuif_response *rsp = NULL;
	int ret;

	XENGPU_REQUEST_INIT(&request);

	request.operation  = GPUIF_OP_UNMAP_DEVPHYSHEAPS;
	request.ui32FuncID = func_id;
	request.ui32DevID = dev_id;
	request.ui32OsID = os_id;

	ret = xengpu_do_request(&request, &rsp);
	if ((!ret) && (rsp != NULL)) {
		ret = rsp->status;
	}
	kfree(rsp);

	xen_pvz_shbuf_free(drv_info->shbuf);
	drv_info->shbuf = NULL;
	drv_info->mapped = true;

	return 0;
}


int gsx_front_map(uint32_t func_id, uint32_t dev_id, uint32_t os_id,
	uint64_t size, uint64_t addr)
{
	struct xengpu_info *drv_info = &gXenGpuInfo;
	struct gpuif_request request;
	struct gpuif_response *rsp = NULL;
	int ret;
	struct xen_pvz_shbuf_cfg buf_cfg;

	memset(&buf_cfg, 0, sizeof(buf_cfg));
	buf_cfg.xb_dev = drv_info->xb_dev;
	buf_cfg.size = size;
	buf_cfg.addr = addr;
	buf_cfg.pages = NULL;

	drv_info->shbuf = xen_pvz_shbuf_alloc(&buf_cfg);
	if (IS_ERR(drv_info->shbuf))
		return -ENOMEM;

	XENGPU_REQUEST_INIT(&request);

	request.operation  = GPUIF_OP_MAP_DEVPHYSHEAPS;
	request.ui32FuncID = func_id;
	request.ui32DevID = dev_id;
	request.ui32OsID = os_id;
	request.op.map_dev_heap.gref_directory =
			xen_pvz_shbuf_get_dir_start(drv_info->shbuf);
	request.op.map_dev_heap.buffer_sz = size;

	ret = xengpu_do_request(&request, &rsp);
	if ((!ret) && (rsp != NULL)) {
		ret = rsp->status;
	}
	kfree(rsp);

	if (ret) {
		goto fail;
	}

	drv_info->mapped = true;

	return 0;

fail:
	if (drv_info->shbuf) {
		xen_pvz_shbuf_free(drv_info->shbuf);
		drv_info->shbuf = NULL;
	}

	return ret;
}


static int gpuif_request(struct xengpu_info *info, struct gpuif_request *pReq)
{
	struct gpufront_info *rinfo = info->rinfo;
	struct gpuif_front_ring *ring = &(rinfo->front_ring);
	RING_IDX req_prod = ring->req_prod_pvt;
	struct gpuif_request *ring_req;
	int notify;
	uint32_t id = req_prod & (GPU_RING_SIZE - 1);;

	if (RING_FULL(ring))
	{
		return -EBUSY;
	}

	ring_req = RING_GET_REQUEST(ring, ring->req_prod_pvt);
	ring->req_prod_pvt++;

	memcpy(ring_req, pReq, sizeof(struct gpuif_request));
	ring_req->id = id;
	pReq->id     = id;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
	if (notify)
		notify_remote_via_irq(rinfo->irq);

	return 0;
}

static irqreturn_t gpuif_interrupt(int irq, void *dev_id)
{
	struct gpuif_response *bret;
	RING_IDX i, rp;
	struct xenbus_device *xbdev = (struct xenbus_device *)dev_id;
	struct xengpu_info *info = dev_get_drvdata(&xbdev->dev);
	struct gpufront_info *rinfo = info->rinfo;
	struct xengpu_call_info *current_call = NULL;

	spin_lock(&rinfo->ring_lock);
again:
	rp = rinfo->front_ring.sring->rsp_prod;
	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = rinfo->front_ring.rsp_cons; i != rp; i++) {
		bret = RING_GET_RESPONSE(&rinfo->front_ring, i);

		//spin_lock(&info->calls_lock);
		current_call = info->call_head;
		do {
			if (!current_call)
				break;
			if (current_call->req->id == bret->id)
				break;
			current_call = current_call->next;
		} while(current_call);

		if (current_call && (current_call->req->id == bret->id)) {
			memcpy(current_call->rsp, bret, sizeof(struct gpuif_response));
			complete(&current_call->call_completed);
		}
		//spin_unlock(&info->calls_lock);
	}

	rinfo->front_ring.rsp_cons = i;

	if (i != rinfo->front_ring.req_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_RESPONSES(&rinfo->front_ring, more_to_do);
		if (more_to_do)
			goto again;
	} else
		rinfo->front_ring.sring->rsp_event = i + 1;

	spin_unlock(&rinfo->ring_lock);

	return IRQ_HANDLED;
}

int xengpu_do_request(struct gpuif_request *pReq, struct gpuif_response **rsp)
{
	int ret = 0;
	struct xengpu_call_info *call = kzalloc((sizeof(struct xengpu_call_info)), GFP_KERNEL);
	struct xengpu_call_info *current_call;
	unsigned long flags;
	VMM_DEBUG_PRINT("%s:%d, enter!", __FUNCTION__, __LINE__);

	if (gXenGpuInfo.inited == false)
	{
		pr_err("xen gpu is not initialized correctly");
		return -EINVAL;
	}

	spin_lock_irqsave(&gXenGpuInfo.calls_lock, flags);
	init_completion(&call->call_completed);
	current_call = gXenGpuInfo.call_head;

	if (!current_call) {
		gXenGpuInfo.call_head = call;
	} else {
		while (current_call->next){
			current_call = current_call->next;
		}
		current_call->next = call;
	}
	call->req = pReq;
	call->rsp = kzalloc((sizeof(struct gpuif_response)), GFP_KERNEL);
	VMM_DEBUG_PRINT("%s:%d, send quest(id = %d)", __FUNCTION__, __LINE__, pReq->operation);

	ret = gpuif_request(&gXenGpuInfo, pReq);
	if (ret) {
		spin_unlock_irqrestore(&gXenGpuInfo.calls_lock, flags);
		pr_err("xen gpu is not initialized correctly");
		return ret;
	}
	VMM_DEBUG_PRINT("%s:%d, xen gpu is initialized correctly, pReq->id = %llu!", __FUNCTION__, __LINE__, pReq->id);
	spin_unlock_irqrestore(&gXenGpuInfo.calls_lock, flags);

	/* wait for the backend to connect */
	if (wait_for_completion_interruptible(&call->call_completed) < 0)
		return -ETIMEDOUT;

	VMM_DEBUG_PRINT("%s:%d, get rsp!", __FUNCTION__, __LINE__);

	spin_lock_irqsave(&gXenGpuInfo.calls_lock, flags);
	current_call = gXenGpuInfo.call_head;
	if ( current_call == NULL ) {
		// do nothing
	} else if ( current_call == call ){
		gXenGpuInfo.call_head = current_call->next;
	} else {
		do{
			if (current_call->next == call) break;
			current_call = current_call->next;
		} while (current_call != NULL);

		if (current_call->next == call)
			current_call->next = current_call->next->next;
	}

	spin_unlock_irqrestore(&gXenGpuInfo.calls_lock, flags);

	*rsp = call->rsp;
	kfree(call);
	return ret;
}
EXPORT_SYMBOL(xengpu_do_request);

static int talk_to_backend_ring(struct xenbus_device *dev, struct gpufront_info *priv)
{
	struct xenbus_transaction xbt;
	const char *message = NULL;
	int rv;

 again:
	rv = xenbus_transaction_start(&xbt);
	if (rv) {
		xenbus_dev_fatal(dev, rv, "starting transaction");
			return rv;
	}

	rv = xenbus_printf(xbt, dev->nodename,
			"gpu-ring-ref", "%u", priv->ring_ref);
	if (rv) {
		message = "writing ring-ref";
		goto abort_transaction;
	}

	rv = xenbus_printf(xbt, dev->nodename, "event-channel-gpu", "%u",
			priv->evtchn);
	if (rv) {
		message = "writing event-channel";
		goto abort_transaction;
	}

	rv = xenbus_transaction_end(xbt, 0);
	if (rv == -EAGAIN)
		goto again;
	if (rv) {
		xenbus_dev_fatal(dev, rv, "completing transaction");
		return rv;
	}

	return 0;

 abort_transaction:
	xenbus_transaction_end(xbt, 1);
	if (message)
		xenbus_dev_error(dev, rv, "%s", message);

	return rv;
}



static int gpufront_connect(struct xenbus_device *dev)
{
	struct gpuif_sring *sring;
	grant_ref_t gref;
	int err;
	struct gpufront_info *rinfo = kzalloc(sizeof(struct gpufront_info), GFP_KERNEL);
	struct xengpu_info *info = &gXenGpuInfo;

	sring= (struct gpuif_sring *)get_zeroed_page(GFP_NOIO | __GFP_HIGH);
	if (!sring) {
		err = -ENOMEM;
		xenbus_dev_fatal(dev, err, "allocating gpu ring page");
		goto fail;
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&rinfo->front_ring, sring, XEN_PAGE_SIZE);

	err = xenbus_grant_ring(dev, sring, 1, &gref);
	if (err < 0)
		goto grant_ring_fail;
	rinfo->ring_ref = gref;

	err = xenbus_alloc_evtchn(dev, &rinfo->evtchn);
	if (err < 0)
		goto alloc_evtchn_fail;

	err = bind_evtchn_to_irqhandler(rinfo->evtchn,
					gpuif_interrupt,
					0, "gpufront", dev);
	if (err <= 0)
		goto bind_fail;
	rinfo->irq = err;
	spin_lock_init(&rinfo->ring_lock);

	spin_lock_init(&info->calls_lock);
	info->rinfo = rinfo;

	err = talk_to_backend_ring(dev, rinfo);
	if (err <0)
		goto bind_fail;

	dev_set_drvdata(&dev->dev, info);

	VMM_DEBUG_PRINT("out %s:%d", __FUNCTION__, __LINE__);
	return 0;

bind_fail:
	xenbus_free_evtchn(dev, rinfo->evtchn);
alloc_evtchn_fail:
	gnttab_end_foreign_access_ref(rinfo->ring_ref, 0);
grant_ring_fail:
	free_page((unsigned long)sring);
fail:
	kfree(rinfo);
	kfree(info);
	VMM_DEBUG_PRINT("out %s:%d, err = %d", __FUNCTION__, __LINE__, err);
	return err;
}

static void xengpu_disconnect_backend(struct gpufront_info *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->ring_lock, flags);

	if (info->irq)
		unbind_from_irqhandler(info->irq, info);
	info->evtchn = 0;
	info->irq = 0;

	gnttab_free_grant_references(info->ring_ref);

	/* End access and free the pages */
	gnttab_end_foreign_access(info->ring_ref, 0, (unsigned long)info->front_ring.sring);

	info->ring_ref = GRANT_INVALID_REF;
	spin_unlock_irqrestore(&info->ring_lock, flags);
}


/**
 * Callback received when the backend's state changes.
 */
static void gpu_backend_changed(struct xenbus_device *dev,
				enum xenbus_state backend_state)
{
	pr_err("%s %p backend(%s) this end(%s)\n",
		__func__,
		dev,
		xenbus_strstate(backend_state),
		xenbus_strstate(dev->state));

	switch (backend_state) {
		case XenbusStateInitialising:
		case XenbusStateInitialised:
		case XenbusStateReconfiguring:
		case XenbusStateReconfigured:
		case XenbusStateUnknown:
			break;
		case XenbusStateInitWait:
			if (dev->state != XenbusStateInitialising)
				break;
			gpufront_connect(dev);
			xenbus_switch_state(dev, XenbusStateInitialised);
			break;
		case XenbusStateConnected:
			xenbus_switch_state(dev, XenbusStateConnected);
			gXenGpuInfo.inited = true;
			complete(&gXenGpuInfo.pvz_completion);
			break;

		case XenbusStateClosed:
			if (dev->state == XenbusStateClosed)
				break;
			/* fall through */
		case XenbusStateClosing:
			xenbus_frontend_closed(dev);
			break;
		}
}

static int xengpufront_remove(struct xenbus_device *xbdev)
{
	struct xengpu_info *info = dev_get_drvdata(&xbdev->dev);
	struct gpufront_info *rinfo = info->rinfo;
	dev_dbg(&xbdev->dev, "%s removed", xbdev->nodename);
	if (info == NULL)
		return 0;
	gXenGpuInfo.inited = false;
	xengpu_disconnect_backend(rinfo);
	kfree(rinfo);
	kfree(info);
	return 0;
}


// The function is called on activation of the device
static int xengpufront_probe(struct xenbus_device *dev,
			  const struct xenbus_device_id *id)
{
	printk(KERN_NOTICE "Probe called. We are good to go.\n");
	gXenGpuInfo.xb_dev = dev;
	xenbus_switch_state(dev, XenbusStateInitialising);
	return 0;
}

// This defines the name of the devices the driver reacts to
static const struct xenbus_device_id xengpufront_ids[] = {
	{ "vgpu"  },
	{ ""	}
};

// We set up the callback functions
static struct xenbus_driver xengpufront_driver = {
	.name = "gpufront",
	.ids	= xengpufront_ids,
	.probe = xengpufront_probe,
	.remove = xengpufront_remove,
	.otherend_changed = gpu_backend_changed,
};

// On loading this kernel module, we register as a frontend driver
int xengpu_init(void)
{
	int ret;

	printk(KERN_NOTICE "enter xen gpu front driver!\n");
	if (!xen_domain())
		return -ENODEV;

	init_completion(&gXenGpuInfo.pvz_completion);
	ret = xenbus_register_frontend(&xengpufront_driver);
	if (ret < 0)
		pr_err("Failed to initialize frontend driver, ret %d", ret);

	/* wait for the backend to connect */
	if (wait_for_completion_interruptible(&gXenGpuInfo.pvz_completion) < 0)
		return -ETIMEDOUT;

	return 0;
}

// ...and on unload we unregister
void xengpu_exit(void)
{
	printk(KERN_ALERT "unregister xen gpu front driver!\n");
	xenbus_unregister_driver(&xengpufront_driver);
}
