/*
 * Copyright (C) Semidrive Semiconductor Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/semidrive-mailbox.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/page.h>
#include <xen/events.h>
#include <xen/grant_table.h>

#define MBOX_FRONT_MAX_SPIN 5000
#define MBOX_RING_ORDER 1
#define MBOX_RING_SIZE	\
        __CONST_RING_SIZE(mbox, XEN_PAGE_SIZE)

struct mbox_chan_front {
	unsigned target;
	bool is_run;
	u8 my_addr;
	u8 dest_addr;
	u8 osid;
};

struct mbox_front_channel {
	spinlock_t ring_lock;
	struct mbox_front_ring front_ring;
	grant_ref_t ring_ref;
	int evtchn, data_evtchn;
	int irq;

	int data_irq;
	grant_ref_t data_ref;
	struct mbox_data_intf *ring;
	struct mbox_data data;
	struct mutex in_mutex;
	struct mutex out_mutex;
	void *rxbuf_linear;
	wait_queue_head_t inflight_conn_req;
};

struct mbox_call_info {
	struct mbox_request_data *req;
	struct mbox_respond_data respond;
	struct mbox_call_info *next;
	struct completion call_completed;
};

struct mbox_frontend {
	struct xenbus_device *xb_dev;
	struct mbox_front_channel *channel;
	struct completion call_completion;
	struct mbox_call_info *call_head;
	spinlock_t calls_lock;
	bool inited;
	struct mbox_chan_front mlink[MB_MAX_CHANS];
	struct mbox_chan chan[MB_MAX_CHANS];
	struct mbox_controller controller;
};

static struct class mbox_front_class = { .name = "mailbox", };

static int mbox_write_ring(struct mbox_data_intf *intf,
			struct mbox_data *data,
			void *buffer,
			int len)
{
	RING_IDX cons, prod, size, masked_prod, masked_cons;
	RING_IDX array_size = XEN_FLEX_RING_SIZE(MBOX_RING_ORDER);
	int32_t error;

	error = intf->out_error;
	if (error < 0)
		return error;
	cons = intf->out_cons;
	prod = intf->out_prod;
	/* read indexes before continuing */
	virt_mb();

	size = mbox_queued(prod, cons, array_size);
	if (size >= array_size)
		return -EINVAL;

	if (len > array_size - size)
		len = array_size - size;

	masked_prod = mbox_mask(prod, array_size);
	masked_cons = mbox_mask(cons, array_size);

	if (masked_prod < masked_cons) {
		memcpy(data->out + masked_prod, buffer, len);
	} else {
		int end_size = array_size - masked_prod;

		if (len > end_size) {
			memcpy(data->out + masked_prod, buffer, end_size);
			memcpy(data->out, buffer + end_size, len - end_size);
		} else {
			memcpy(data->out + masked_prod, buffer, len);
		}
	}

	/* write to ring before updating pointer */
	virt_wmb();
	intf->out_prod += len;

	return len;
}
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static int mbox_data_read(struct mbox_data_intf *intf,
		       struct mbox_data *data, void *buffer, size_t maxlen, bool peek)
{
	RING_IDX cons, prod, size, masked_prod, masked_cons;
	RING_IDX array_size = XEN_FLEX_RING_SIZE(MBOX_RING_ORDER);
	size_t data_len;
	int32_t error;

	cons = intf->in_cons;
	prod = intf->in_prod;
	error = intf->in_error;
	/* get pointers before reading from the ring */
	virt_rmb();
	if (error < 0)
		return error;

	size = mbox_queued(prod, cons, array_size);
	masked_prod = mbox_mask(prod, array_size);
	masked_cons = mbox_mask(cons, array_size);

	if (size == 0)
		return 0;

	data_len = MIN(maxlen, size);

	if (masked_prod > masked_cons) {
		memcpy(buffer, data->in + masked_cons, data_len);
	} else {
		int len = array_size - masked_cons;

		if (data_len > len) {
			memcpy(buffer, data->in + masked_cons, len);
			memcpy(buffer + len, data->in, data_len - len);
		} else {
			memcpy(buffer, data->in + masked_cons, data_len);
		}
	}

	/* read data from the ring before increasing the index */
	virt_mb();

	if (!peek)
		intf->in_cons += data_len;

	return data_len;
}

static bool mbox_data_writeable(struct mbox_front_channel *chan)
{
	struct mbox_data_intf *intf = chan->ring;
	RING_IDX cons, prod, size = XEN_FLEX_RING_SIZE(MBOX_RING_ORDER);
	int32_t error;

	error = intf->out_error;
	if (error == -ENOTCONN)
		return false;

	if (error != 0)
		return true;

	cons = intf->out_cons;
	prod = intf->out_prod;

	return !!(size - mbox_queued(prod, cons, size));
}

static int mbox_cmd_enqueue(struct mbox_frontend *mbdev, struct mbox_request_data *req)
{
	struct mbox_front_channel *chan = mbdev->channel;
	struct mbox_front_ring *ring = &(chan->front_ring);
	RING_IDX req_prod = ring->req_prod_pvt;
	struct mbox_request_data *ring_req;
	int notify;
	uint32_t id = req_prod & (MBOX_RING_SIZE - 1);;

	if (RING_FULL(ring))
	{
		return -EBUSY;
	}

	ring_req = RING_GET_REQUEST(ring, ring->req_prod_pvt);
	ring->req_prod_pvt++;

	memcpy(ring_req, req, sizeof(struct mbox_request_data));
	ring_req->req_id = id;
	req->req_id      = id;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
	if (notify)
		notify_remote_via_irq(chan->irq);

	return 0;
}

static int mbox_cmd_call(struct mbox_frontend *mbdev, struct mbox_request_data *req)
{
	struct mbox_call_info *call;
	struct mbox_call_info *current_call;
	struct device *dev = &mbdev->xb_dev->dev;
	unsigned long flags;
	int ret = 0;

	if (!mbdev || mbdev->inited == false) {
		dev_err(dev, "device is not initialized yet");
		return -EINVAL;
	}

	dev = &mbdev->xb_dev->dev;
	call = kzalloc((sizeof(struct mbox_call_info)), GFP_KERNEL);
	if (!call) {
		dev_err(dev, "no memory\n");
		return -ENOMEM;
	}

	spin_lock_irqsave(&mbdev->calls_lock, flags);
	init_completion(&call->call_completed);
	current_call = mbdev->call_head;
	if (!current_call) {
		mbdev->call_head = call;
	} else {
		while (current_call->next){
			current_call = current_call->next;
		}
		current_call->next = call;
	}
	call->req = req;

	ret = mbox_cmd_enqueue(mbdev, req);
	if (ret) {
		spin_unlock_irqrestore(&mbdev->calls_lock, flags);
		dev_err(dev, "fail to call\n");
		return ret;
	}
	spin_unlock_irqrestore(&mbdev->calls_lock, flags);

	/* wait for the backend to connect */
	if (wait_for_completion_interruptible(&call->call_completed) < 0)
		return -ETIMEDOUT;

	spin_lock_irqsave(&mbdev->calls_lock, flags);
	current_call = mbdev->call_head;
	if (current_call == NULL) {
		// do nothing
	} else if (current_call == call) {
		mbdev->call_head = current_call->next;
	} else {
		do {
			if (current_call->next == call)
				break;
			current_call = current_call->next;
		} while (current_call != NULL);

		if (current_call->next == call)
			current_call->next = current_call->next->next;
	}
	spin_unlock_irqrestore(&mbdev->calls_lock, flags);
	ret = (int)call->respond.result;
	kfree(call);

	return ret;
}

static int mbox_front_send_data(struct mbox_chan *mchan, void *data)
{
	struct mbox_controller *mbox = mchan->mbox;
	struct mbox_frontend *mbdev =
		container_of(mbox, struct mbox_frontend, controller);
	struct mbox_front_channel *chan = mbdev->channel;
	struct mbox_chan_front *mlink = mchan->con_priv;
	sd_msghdr_t *msg = data;
	int sent, len, tot_sent = 0;
	int count = 0;

	dev_dbg(mbox->dev, "tx in p=%d c=%d a=%d l=%d",
			msg->rproc, msg->protocol, msg->addr, msg->dat_len);
	mutex_lock(&chan->out_mutex);
	if (!mbox_data_writeable(chan)) {
		mutex_unlock(&chan->out_mutex);
		return -EAGAIN;
	}

	msg->rproc = mlink->target;
	msg->addr = mlink->dest_addr;
	len = msg->dat_len;
again:
	count++;
	sent = mbox_write_ring(chan->ring, &chan->data, data, len);
	if (sent > 0) {
		len -= sent;
		tot_sent += sent;
		notify_remote_via_irq(chan->data_irq);
	}
	if (sent >= 0 && len > 0 && count < MBOX_FRONT_MAX_SPIN)
		goto again;

	if (sent < 0)
		tot_sent = sent;

	mutex_unlock(&chan->out_mutex);

	dev_dbg(mbox->dev, "send tot=%d bytes", tot_sent);

	return tot_sent > 0 ? 0 : tot_sent;
}

static int mbox_front_startup(struct mbox_chan *mchan)
{
	struct mbox_chan_front *mlink = mchan->con_priv;

	mlink->is_run = true;

	return 0;
}

static void mbox_front_shutdown(struct mbox_chan *mchan)
{
	struct mbox_frontend *mbdev =
		container_of(mchan->mbox, struct mbox_frontend, controller);
	struct mbox_chan_front *mlink = mchan->con_priv;
	struct mbox_request_data request;

	request.cmd  = MBC_DEL_CHANNEL;
	request.osid = mlink->osid;
	request.mba  = mlink->my_addr;
	mbox_cmd_call(mbdev, &request);
	mlink->is_run = false;
}

static bool mbox_front_last_tx_done(struct mbox_chan *mchan)
{
	return true;
}

static const struct mbox_chan_ops mbox_front_ops = {
	.send_data = mbox_front_send_data,
	.startup = mbox_front_startup,
	.shutdown = mbox_front_shutdown,
	.last_tx_done = mbox_front_last_tx_done,
};

static struct mbox_chan *mbox_front_of_xlate(struct mbox_controller *mbox,
					    const struct of_phandle_args *p)
{
	struct mbox_frontend *mbdev =
		container_of(mbox, struct mbox_frontend, controller);
	struct mbox_request_data request;
	int local_addr, remote_link;
	int i;

	/* #mbox-cells is 2 */
	if (p->args_count != 2) {
		dev_err(mbox->dev, "Invalid arguments in dt[%d] instead of 2\n",
			p->args_count);
		return ERR_PTR(-EINVAL);
	}

	local_addr = p->args[0];
	remote_link = p->args[1];

	for (i = 0;i < mbox->num_chans;i++) {
		struct mbox_chan *mchan = &mbox->chans[i];
		struct mbox_chan_front *mlink = mchan->con_priv;
		if (0xff == mlink->target) {
			mlink->target = MB_RPROC_ID(remote_link);
			mlink->my_addr = local_addr;
			mlink->dest_addr = MB_DST_ADDR(remote_link);
			mlink->osid = MB_OS_ID(remote_link);

			request.cmd = MBC_NEW_CHANNEL;
			request.osid = mlink->osid;
			request.mba = local_addr;
			mbox_cmd_call(mbdev, &request);

			return mchan;
		}
	}

	dev_info(mbox->dev, "myaddr %d, remote %d is wrong on %s\n",
		local_addr, remote_link, p->np->name);
	return ERR_PTR(-ENOENT);
}

static struct mbox_chan *find_used_chan_atomic(
			struct mbox_frontend *mbdev, u32 rproc, u32 dest)
{
	u32 idx;
	struct mbox_chan *chan;
	struct mbox_chan_front *mlink;
	struct mbox_controller *mbox = &mbdev->controller;

	if (!mbox->dev || mbdev->inited == false)
		return NULL;

	if (dest) {
		/* match exact reciever address */
		for (idx = 0;idx < MB_MAX_CHANS; idx++) {
			chan = &mbdev->chan[idx];
			mlink = &mbdev->mlink[idx];
			if (chan->cl && (dest == mlink->my_addr))
				return chan;
		}
		dev_info(mbox->dev, "mu: channel addr %d not found\n", dest);
	} else
		dev_info(mbox->dev, "mu: invalid addr from rproc%d\n", rproc);

	/* if addr not matched, try to match processor */
	for (idx = 0;idx < MB_MAX_CHANS; idx++) {
		chan = &mbdev->chan[idx];
		mlink = &mbdev->mlink[idx];
		if (chan->cl && (rproc == mlink->target))
			return chan;
	}

	dev_warn(mbox->dev, "mu: channel addr %d not found for rproc %d\n",
			dest, rproc);

    return NULL;
}

static irqreturn_t mbox_front_cmd_interrupt(int irq, void *dev_id)
{
	struct xenbus_device *xbdev = (struct xenbus_device *)dev_id;
	struct mbox_frontend *fe = dev_get_drvdata(&xbdev->dev);
	struct mbox_front_channel *chan = fe->channel;
	struct mbox_call_info *current_call = NULL;
	struct mbox_respond_data *respond;
	unsigned long flags;
	RING_IDX i, rp;

	spin_lock_irqsave(&chan->ring_lock, flags);
again:
	rp = chan->front_ring.sring->rsp_prod;
	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = chan->front_ring.rsp_cons; i != rp; i++) {

		respond = RING_GET_RESPONSE(&chan->front_ring, i);

		//spin_lock(&info->calls_lock);
		current_call = fe->call_head;
		do {
			if (!current_call)
				break;
			if (current_call->req->req_id == respond->req_id)
				break;
			current_call = current_call->next;
		} while(current_call);

		if (current_call && (current_call->req->req_id == respond->req_id)) {
			memcpy(&current_call->respond, respond, sizeof(struct mbox_respond_data));
			complete(&current_call->call_completed);
		}
		//spin_unlock(&info->calls_lock);
	}

	chan->front_ring.rsp_cons = i;

	if (i != chan->front_ring.req_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_RESPONSES(&chan->front_ring, more_to_do);
		if (more_to_do)
			goto again;
	} else
		chan->front_ring.sring->rsp_event = i + 1;

	spin_unlock_irqrestore(&chan->ring_lock, flags);

	return IRQ_HANDLED;
}

static irqreturn_t mbox_front_data_interrupt(int irq, void *dev_id)
{
	struct xenbus_device *xbdev = (struct xenbus_device *)dev_id;
	struct mbox_frontend *mbdev = dev_get_drvdata(&xbdev->dev);
	struct mbox_front_channel *chan = mbdev->channel;
	unsigned char *buffer = chan->rxbuf_linear;
	struct mbox_data_head *ring_recv;
	struct mbox_chan *mbchan = NULL;
	unsigned long flags;
	sd_msghdr_t *msg;
	int event_len, head_len;
	int ret;

	head_len = sizeof(sd_msghdr_t) + sizeof(struct mbox_data_head);
	spin_lock_irqsave(&chan->ring_lock, flags);

	/* process events until none left */
	for (;;) {
		/* Peek message head to calculate length */
		ret = mbox_data_read(chan->ring, &chan->data, buffer, head_len, true);
		if (ret <= 0) {
			spin_unlock_irqrestore(&chan->ring_lock, flags);
			return IRQ_HANDLED;
		}

		/* Peek entire message with the length */
		ring_recv = (struct mbox_data_head *) buffer;
		msg = (sd_msghdr_t *) ring_recv->data;
		event_len = msg->dat_len + sizeof(struct mbox_data_head);
		WARN_ON(event_len > MB_BANK_LEN);

		ret = mbox_data_read(chan->ring, &chan->data, buffer, event_len, false);
		if (ret <= 0) {
			spin_unlock_irqrestore(&chan->ring_lock, flags);
			return IRQ_HANDLED;
		}
		WARN_ON(event_len != ret);

		dev_dbg(&xbdev->dev, "rx t=%d p=%d o=%d s=%d d=%d l=%d b=%p\n",
				ring_recv->type, ring_recv->origin_proc,
				msg->osid, ring_recv->src_addr,
				ring_recv->dst_addr, msg->dat_len, buffer);

		mbchan = find_used_chan_atomic(mbdev, msg->rproc, msg->addr);
		if (mbchan)
			mbox_chan_received_data(mbchan, (void *)msg);
	}

	spin_unlock_irqrestore(&chan->ring_lock, flags);

	return IRQ_HANDLED;
}

static int mbox_create_active(struct xenbus_device *xbdev, struct mbox_front_channel *chan, int *evtchn)
{
	void *bytes;
	int ret = -ENOMEM, irq = -1, i;

	dev_err(&xbdev->dev, "creating active");

	*evtchn = -1;
	init_waitqueue_head(&chan->inflight_conn_req);

	chan->ring = (struct mbox_data_intf *) __get_free_page(GFP_KERNEL | __GFP_ZERO);
	if (chan->ring == NULL)
		goto out_error;

	chan->ring->ring_order = MBOX_RING_ORDER;
	bytes = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, MBOX_RING_ORDER);
	if (bytes == NULL)
		goto out_error;

	for (i = 0; i < (1 << MBOX_RING_ORDER); i++)
		chan->ring->ref[i] = gnttab_grant_foreign_access(
			xbdev->otherend_id,
			pfn_to_gfn(virt_to_pfn(bytes) + i), 0);

	chan->data_ref = gnttab_grant_foreign_access(
		xbdev->otherend_id,
		pfn_to_gfn(virt_to_pfn((void *)chan->ring)), 0);

	chan->data.in = bytes;
	chan->data.out = bytes + XEN_FLEX_RING_SIZE(MBOX_RING_ORDER);

	ret = xenbus_alloc_evtchn(xbdev, evtchn);
	if (ret)
		goto out_error;

	irq = bind_evtchn_to_irqhandler(*evtchn, mbox_front_data_interrupt,
					0, "mbox-frontend", xbdev);
	if (irq < 0) {
		ret = irq;
		goto out_error;
	}

	chan->rxbuf_linear = kmalloc(MB_BANK_LEN, GFP_KERNEL);
	if (chan->rxbuf_linear == NULL) {
		ret = irq;
		goto out_error;
	}

	chan->data_irq = irq;
	mutex_init(&chan->in_mutex);
	mutex_init(&chan->out_mutex);

	dev_err(&xbdev->dev, "created active");

	return 0;

out_error:

	if (chan->rxbuf_linear)
		kfree(chan->rxbuf_linear);
	if (*evtchn >= 0)
		xenbus_free_evtchn(xbdev, *evtchn);
	kfree(chan->data.in);
	kfree(chan->ring);
	return ret;
}

static int mbox_connect_backend(struct xenbus_device *xbdev, struct mbox_front_channel *chan)
{
	struct xenbus_transaction xbt;
	const char *message = NULL;
	int ret;

again:
	ret = xenbus_transaction_start(&xbt);
	if (ret) {
		xenbus_dev_fatal(xbdev, ret, "starting transaction");
		return ret;
	}

	ret = xenbus_printf(xbt, xbdev->nodename,
				"cmd-ring-ref", "%u", chan->ring_ref);
	if (ret) {
		message = "writing ring-ref";
		goto abort_transaction;
	}

	ret = xenbus_printf(xbt, xbdev->nodename, "cmd-evt-chn", "%u",
				chan->evtchn);
	if (ret) {
		message = "writing event-channel";
		goto abort_transaction;
	}

	ret = xenbus_printf(xbt, xbdev->nodename,
				"data-ring-ref", "%u", chan->data_ref);
	if (ret) {
		message = "writing ring-ref";
		goto abort_transaction;
	}

	ret = xenbus_printf(xbt, xbdev->nodename, "data-evt-chn", "%u",
				chan->data_evtchn);
	if (ret) {
		message = "writing event-channel";
		goto abort_transaction;
	}

	ret = xenbus_transaction_end(xbt, 0);
	if (ret == -EAGAIN)
		goto again;

	if (ret) {
		xenbus_dev_fatal(xbdev, ret, "completing transaction");
		return ret;
	}

	return 0;

abort_transaction:
	xenbus_transaction_end(xbt, 1);
	if (message)
	        xenbus_dev_error(xbdev, ret, "%s", message);

	return ret;
}

static int mbox_front_connect(struct xenbus_device *xbdev)
{
	struct mbox_frontend *mbdev = dev_get_drvdata(&xbdev->dev);
	struct mbox_front_channel *chan;
	struct mbox_sring *sring;
	grant_ref_t gref;
	int err;

	dev_err(&xbdev->dev, "connecting backend");

	chan = devm_kzalloc(&xbdev->dev, sizeof(struct mbox_front_channel), GFP_KERNEL);
	if (!chan) {
		err = -ENOMEM;
		xenbus_dev_fatal(xbdev, err, "allocating mbox channel");
		goto fail;
	}

	sring = (struct mbox_sring *)get_zeroed_page(GFP_NOIO | __GFP_HIGH);
	if (!sring) {
		err = -ENOMEM;
		xenbus_dev_fatal(xbdev, err, "allocating mbox ring page");
		goto fail;
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&chan->front_ring, sring, XEN_PAGE_SIZE);

	err = xenbus_grant_ring(xbdev, sring, 1, &gref);
	if (err < 0)
		goto grant_ring_fail;
	chan->ring_ref = gref;

	err = xenbus_alloc_evtchn(xbdev, &chan->evtchn);
	if (err < 0)
		goto alloc_evtchn_fail;

	err = bind_evtchn_to_irqhandler(chan->evtchn, mbox_front_cmd_interrupt,
					0, "mb-front", xbdev);
	if (err <= 0)
		goto bind_fail;

	chan->irq = err;
	spin_lock_init(&chan->ring_lock);
	mbdev->channel = chan;

	err = mbox_create_active(xbdev, chan, &chan->data_evtchn);
	if (err < 0)
		goto bind_fail;

	err = mbox_connect_backend(xbdev, chan);
	if (err < 0)
		goto bind_fail;

	dev_err(&xbdev->dev, "connected to backend");

	return 0;

bind_fail:
	xenbus_free_evtchn(xbdev, chan->evtchn);
alloc_evtchn_fail:
	gnttab_end_foreign_access_ref(chan->ring_ref, 0);
grant_ring_fail:
	free_page((unsigned long)sring);
fail:

	if (chan)
		kfree(chan);

	dev_info(&xbdev->dev, "out %s, err = %d", __FUNCTION__, err);

	return err;
}

static void mbox_disconnect_backend(struct mbox_front_channel *chan)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&chan->ring_lock, flags);

	if (chan->irq)
		unbind_from_irqhandler(chan->irq, chan);
	chan->evtchn = 0;
	chan->irq = 0;

	gnttab_free_grant_references(chan->ring_ref);

	/* End access and free the pages */
	gnttab_end_foreign_access(chan->ring_ref, 0, (unsigned long)chan->front_ring.sring);

	chan->ring_ref = GRANT_INVALID_REF;

	if (chan->data_irq)
		unbind_from_irqhandler(chan->data_irq, chan);
	chan->data_evtchn = 0;
	chan->data_irq = 0;

	for (i = 0; i < (1 << MBOX_RING_ORDER); i++)
		gnttab_end_foreign_access(chan->ring->ref[i], 0, 0);

	gnttab_end_foreign_access(chan->data_ref, 0, 0);
	free_page((unsigned long)chan->ring);

	spin_unlock_irqrestore(&chan->ring_lock, flags);
}

// The function is called on activation of the device
static int mbox_front_probe(struct xenbus_device *xbdev,
              const struct xenbus_device_id *id)
{
	struct mbox_frontend *mbdev;
	struct device_node *np;
	int err, i;

	dev_info(&xbdev->dev, "Probing device...\n");

	/* Allocate memory for device */
	mbdev = devm_kzalloc(&xbdev->dev, sizeof(*mbdev), GFP_KERNEL);
	if (!mbdev)
		return -ENOMEM;

	init_completion(&mbdev->call_completion);
	spin_lock_init(&mbdev->calls_lock);
	mbdev->xb_dev = xbdev;

	for (i = 0; i < MB_MAX_CHANS; i++) {
		mbdev->chan[i].con_priv = &mbdev->mlink[i];
		mbdev->mlink[i].is_run = 0;
		mbdev->mlink[i].target = 0xff; /* assigned during request channel */
	}

	np = of_find_compatible_node(NULL, NULL, "semidrive,mbox-front");
	if (!np) {
		dev_err(&xbdev->dev, "no mbox front of_node\n");
		return -ENODEV;
	}

	xbdev->dev.of_node = np;
	mbdev->controller.dev = &xbdev->dev;
	mbdev->controller.chans = &mbdev->chan[0];
	mbdev->controller.num_chans = MB_MAX_CHANS;
	mbdev->controller.ops = &mbox_front_ops;
	mbdev->controller.txdone_irq = false;
	mbdev->controller.txdone_poll = true;
	mbdev->controller.txpoll_period = 1;
	mbdev->controller.of_xlate = mbox_front_of_xlate;
	mbdev->inited = true;
	dev_set_drvdata(&xbdev->dev, mbdev);

	err = mbox_controller_register(&mbdev->controller);
	if (err) {
		dev_err(&xbdev->dev, "Failed to register mailbox cntl %d\n", err);
		return err;
	}

	xenbus_switch_state(xbdev, XenbusStateInitialising);

	return 0;
}

static int mbox_front_remove(struct xenbus_device *xbdev)
{
	struct mbox_frontend *mbdev = dev_get_drvdata(&xbdev->dev);
	struct mbox_front_channel *chan = mbdev->channel;

	dev_info(&xbdev->dev, "%s removed", xbdev->nodename);
	if (mbdev == NULL)
	    return 0;

	mbox_controller_unregister(&mbdev->controller);
	mbdev->inited = false;
	mbox_disconnect_backend(chan);
	kfree(chan);
	kfree(mbdev);
	dev_set_drvdata(&xbdev->dev, NULL);

	return 0;
}

/**
 * Callback received when the backend's state changes.
 */
static void mbox_backend_changed(struct xenbus_device *xbdev,
				enum xenbus_state backend_state)
{
	dev_info(&xbdev->dev, "%s backend(%s) this end(%s)\n", __func__,
			xenbus_strstate(backend_state), xenbus_strstate(xbdev->state));

	switch (backend_state) {
	case XenbusStateInitialising:
	case XenbusStateInitialised:
	case XenbusStateReconfiguring:
	case XenbusStateReconfigured:
	case XenbusStateUnknown:
		break;
	case XenbusStateInitWait:
		if (xbdev->state != XenbusStateInitialising)
			break;

		mbox_front_connect(xbdev);
		xenbus_switch_state(xbdev, XenbusStateInitialised);
    	break;
	case XenbusStateConnected:
		xenbus_switch_state(xbdev, XenbusStateConnected);
		break;
	case XenbusStateClosed:
		if (xbdev->state == XenbusStateClosed)
			break;
		/* fall through */
	case XenbusStateClosing:
		xenbus_frontend_closed(xbdev);
		break;
	}
}

static const struct xenbus_device_id mbox_front_ids[] = {
	{ XEN_MBOX_DEVICE_ID },
	{ ""  }
};

static struct xenbus_driver mbox_front_driver = {
	.name = "mbox",
	.ids  = mbox_front_ids,
	.probe = mbox_front_probe,
	.remove = mbox_front_remove,
	.otherend_changed = mbox_backend_changed,
};

static int __init semidrive_mbox_init(void)
{
	int err;

	/* Only used in guest OS */
	if (!xen_domain() || xen_initial_domain())
		return -ENODEV;

	err = class_register(&mbox_front_class);
	if (err)
		return err;

	err = xenbus_register_frontend(&mbox_front_driver);
	if (err < 0)
		pr_err("Failed to initialize frontend driver, ret %d", err);

	return err;
}
subsys_initcall(semidrive_mbox_init);

static void __exit semidrive_mbox_exit(void)
{
	/* Only used in guest OS */
	if (!xen_domain() || xen_initial_domain())
		return;

	xenbus_unregister_driver(&mbox_front_driver);

	class_unregister(&mbox_front_class);
}
module_exit(semidrive_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Semidrive Mailbox Frontend Driver");
MODULE_AUTHOR("ye.liu@semidrive.com");
