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
#include <linux/mutex.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/semidrive-mailbox.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/page.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>
#include "mb_regs.h"

#define MAX_PV_MBOXES	4
#define MAX_RING_ORDER XENBUS_MAX_RING_GRANT_ORDER

struct mbox_ioworker {
	struct work_struct register_work;
	struct workqueue_struct *wq;
};

struct mbox_back_channel {
	/* Unique identifier for this interface. */
	domid_t domid;
	struct mbox_backend *be;
	struct mbox_back_ring back_ring;
	unsigned int cmd_irq;

	uint64_t id;
	grant_ref_t ref;
	struct mbox_data_intf *intf;
	void *bytes;
	struct mbox_data data;
	uint32_t ring_order;
	int io_irq;
	atomic_t read;
	atomic_t write;
	atomic_t io;
	atomic_t release;
	atomic_t connected;

	struct mbox_ioworker ioworker;
};

struct mbox_back_chan {
	bool used;
	int osid;
	int mba;
};

struct mbox_backend {
	struct xenbus_device *xb_dev;
	struct sd_mbox_device *mbdev;;
	struct mbox_back_channel *channel;
	bool inited;
	struct mbox_back_chan mchan[MB_MAX_CHANS];
};

static struct mbox_backend *mbox_backends[MAX_PV_MBOXES];
static DEFINE_SPINLOCK(mbox_be_lock);
static struct mbox_backend *
mbox_find_backend(unsigned int osid, unsigned int mba);
static int send_to_hardware(struct mbox_backend *be, sd_msghdr_t *data);

/* read data from MU hardware to frontend */
static void
mbox_copy_to_frontend(struct mbox_back_channel *chan, void *msg, int len)
{
	RING_IDX cons, prod, size, wanted, array_size, masked_prod, masked_cons;
	int32_t error;
	struct mbox_data_intf *intf = chan->intf;
	struct device *dev = &chan->be->xb_dev->dev;
	struct mbox_data *data = &chan->data;
	unsigned int end_size;
	int ret = 0;

	array_size = XEN_FLEX_RING_SIZE(chan->ring_order);
	cons = intf->in_cons;
	prod = intf->in_prod;
	error = intf->in_error;
	/* read the indexes first, then deal with the data */
	virt_mb();

	if (error)
		return;

	size = mbox_queued(prod, cons, array_size);
	if (size >= array_size)
		return;

	wanted = array_size - size;
	masked_prod = mbox_mask(prod, array_size);
	masked_cons = mbox_mask(cons, array_size);
	if (wanted < len) {
		ret = -EAGAIN;
		goto done;
	}

	dev_dbg(dev, "read size=%d prod=%d cons=%d",
				len, masked_prod, masked_cons);
	if (masked_prod < masked_cons) {
		dev_dbg(dev, "linear copy");
		memcpy(data->in + masked_prod, msg, len);
	} else {
		end_size = array_size - masked_prod;
		dev_dbg(dev, "split end_size=%d", end_size);

		if (len < end_size) {
			memcpy(data->in + masked_prod, msg, len);
		} else {
			memcpy(data->in + masked_prod, msg, end_size);
			memcpy(data->in, msg + end_size, len - end_size);
		}
	}
	atomic_set(&chan->read, 0);
	atomic_dec(&chan->io);
	ret = len;

done:
	/* write the data, then modify the indexes */
	virt_wmb();
	if (ret < 0)
		intf->in_error = ret;
	else
		intf->in_prod = prod + ret;
	/* update the indexes, then notify the other end */
	virt_wmb();
	notify_remote_via_irq(chan->io_irq);

	dev_dbg(dev, "update rd prod=%d cons=%d", intf->in_prod, intf->in_cons);

	return;
}

/* write from frontend to MU hardware MU */
static void mbox_conn_back_write(struct mbox_back_channel *chan)
{
	struct mbox_data_intf *intf = chan->intf;
	struct mbox_data *data = &chan->data;
	struct device *dev = &chan->be->xb_dev->dev;
	RING_IDX cons, prod, size, array_size, masked_prod, masked_cons;
	int ret;
	void *buf, *mem = NULL;
	unsigned int end_size;

	cons = intf->out_cons;
	prod = intf->out_prod;
	/* read the indexes before dealing with the data */
	virt_mb();

	array_size = XEN_FLEX_RING_SIZE(chan->ring_order);
	size = mbox_queued(prod, cons, array_size);
	if (size == 0)
		return;

	masked_prod = mbox_mask(prod, array_size);
	masked_cons = mbox_mask(cons, array_size);

	if (masked_prod > masked_cons) {
		buf = data->out + masked_cons;
	} else {
		mem = buf = kzalloc(size, GFP_KERNEL);
		if (!buf)
			return;

		end_size = array_size - masked_cons;
		memcpy(buf, data->out + masked_cons, end_size);
		memcpy(buf + end_size, data->out, size - end_size);
	}

	dev_dbg(dev, "write size=%d prod=%d cons=%d",
			size, masked_prod, masked_cons);

	atomic_set(&chan->write, 0);
	ret = send_to_hardware(chan->be, buf);
	if (ret == -EAGAIN || (ret >= 0 && ret < size)) {
		atomic_inc(&chan->write);
		atomic_inc(&chan->io);
		dev_dbg(dev, " write again ret=%d\n", ret);
	}
	if (ret == -EAGAIN)
		goto out;

	/* write the data, then update the indexes */
	virt_wmb();
	if (ret < 0) {
		intf->out_error = ret;
	} else {
		intf->out_error = 0;
		intf->out_cons = cons + ret;
		prod = intf->out_prod;
	}
	/* update the indexes, then notify the other end */
	virt_wmb();
	if (prod != cons + ret)
		atomic_inc(&chan->write);
	notify_remote_via_irq(chan->io_irq);

	dev_dbg(dev, "update wr prod=%d cons=%d", intf->out_prod, intf->out_cons);

out:
	if (mem)
		kfree(mem);
}

static void mbox_back_ioworker(struct work_struct *work)
{
	struct mbox_ioworker *ioworker = container_of(work,
		struct mbox_ioworker, register_work);
	struct mbox_back_channel *chan = container_of(ioworker, struct mbox_back_channel,
		ioworker);

	while (atomic_read(&chan->io) > 0) {
		if (atomic_read(&chan->release) > 0) {
			atomic_set(&chan->release, 0);
			return;
		}

		if (atomic_read(&chan->write) > 0)
			mbox_conn_back_write(chan);

		atomic_dec(&chan->io);
	}
}

static irqreturn_t mbox_back_write_event(int irq, void *opaque)
{
	struct mbox_back_channel *chan = opaque;
	struct mbox_ioworker *iow;

	if (chan == NULL)
		return IRQ_HANDLED;

	iow = &chan->ioworker;
	atomic_inc(&chan->write);
	atomic_inc(&chan->io);
	queue_work(iow->wq, &iow->register_work);

	return IRQ_HANDLED;
}

void mbox_back_received_data(sd_msghdr_t *data, int remote_proc, int src, int dest)
{
	struct mbox_back_channel *chan;
	struct mbox_backend *be;
	unsigned int osid;
	struct mbox_data_head *hdr;
	sd_msghdr_t msg_body;
	sd_msghdr_t *msg = &msg_body;
	void *rx_buf;
	int size;

	if (!xen_domain())
		return;

	/* from VDSP, reserved a special mba without data */
	if (remote_proc == MASTER_VDSP) {
		be = mbox_find_backend(0, IPCC_ADDR_VDSP_ANN);
		if (!be) {
			pr_err("mbox-irq: backend for mba-%d is not found\n",
					IPCC_ADDR_VDSP_ANN);
			return;
		}
		pr_info("mbox-irq: backend for VDSP\n");
		/* do not copy any data, src, dst is not used */
		MB_MSG_INIT_VDSP_HEAD(msg, MB_MSG_HDR_SZ);
		src = IPCC_ADDR_VDSP_ANN;
		dest = IPCC_ADDR_VDSP_ANN;
	} else {
		/* otherwise, to find a frontend */
		msg = data;
		osid = msg->osid;
		pr_debug("mbox-irq: rx p=%d o=%d s=%d d=%d l=%d\n",
				remote_proc, osid, src, dest, msg->dat_len);

		if (osid == 0)
			osid = msg->rproc;
		be = mbox_find_backend(osid, dest);
		if (!be) {
			be = mbox_find_backend(0, dest);
			if (!be) {
				pr_err("mbox-irq: %d backend with osid-%d mba-%d not found\n",
						remote_proc, osid, dest);
				return;
			}
		}
	}
	chan = be->channel;

	BUG_ON(!chan);

	/* the backend channel is disconnected */
	if (!atomic_read(&chan->connected))
		return;

	size = sizeof(struct mbox_data_head) + msg->dat_len;
	rx_buf = kmalloc(size, GFP_ATOMIC);
	if (rx_buf == NULL)
		return;

	/* Prefix a header in front of received message data */
	hdr = rx_buf;
	hdr->origin_proc = remote_proc;
	hdr->src_addr = src;
	hdr->dst_addr = dest;
	hdr->type = MU_TYPE_NEWDATA;
	memcpy_fromio(hdr->data, msg, msg->dat_len);

	atomic_inc(&chan->read);
	atomic_inc(&chan->io);
	mbox_copy_to_frontend(chan, rx_buf, size);
	pr_debug("mbox-irq: copy to frontend=%d\n", chan->domid);

	kfree(rx_buf);
}
EXPORT_SYMBOL(mbox_back_received_data);

static int fill_tmh(struct sd_mbox_tx_msg *txm, u8 len, u8 target, u8 src, u8 dst)
{
	if (txm) {
		u32 tmh0 = FV_TMH0_TXMES_LEN((ALIGN(len, 2))/2)
							| FV_TMH0_MID(txm->msg_id);
		u32 reset_check;

		tmh0 |= FV_TMH0_MDP(1 << target);
		tmh0 |= BM_TMH0_TXUSE_MB | FV_TMH0_MBM(1 << txm->msg_id);

		writel(tmh0, txm->tmh);
		/* use tmh1 as source addr */
		writel(src, txm->tmh + TMH1_OFF);
		/* use tmh2 as destination addr */
		writel(dst, txm->tmh + TMH2_OFF);

		reset_check = readl(txm->tmh);
		if (reset_check != tmh0) {
			return -ENODEV;
		}

		return 0;
	}

	return -EINVAL;
}

static int send_to_hardware(struct mbox_backend *be, sd_msghdr_t *data)
{
	struct sd_mbox_device *mbdev = be->mbdev;
	struct sd_mbox_tx_msg *txm;
	struct device *dev;
	unsigned int actual_size, priority, protocol, target;
	unsigned long flags;
	int ret;

	dev = mbdev->dev;
	spin_lock_irqsave(&mbdev->msg_lock, flags);

	actual_size = mb_msg_parse_packet_len(data);
	priority = mb_msg_parse_packet_prio(data);
	protocol = mb_msg_parse_packet_proto(data);
	target = mb_msg_parse_rproc(data);

	if (actual_size > MB_BANK_LEN) {
		dev_warn(dev, "msg len %d > expected %d, trim\n",
					actual_size, MB_BANK_LEN);
		actual_size = MB_BANK_LEN;
	}

	txm = sd_mu_alloc_msg(mbdev, 0, priority);
	if (!txm) {
		spin_unlock_irqrestore(&mbdev->msg_lock, flags);
		dev_err(dev, "No msg available\n");
		return -EAGAIN;
	}
	txm->remote = target;

	dev_dbg(dev, "tx to p=%d c=%d l=%d msg: %d\n",
				target, protocol, actual_size, txm->msg_id);
	ret = fill_tmh(txm, actual_size, target, data->addr, data->addr);

	memcpy_toio((void *)txm->tx_buf, data, actual_size);

	if (ret < 0) {
		dev_err(dev, "sendto rp%d unexpected ret: %d\n", target, ret);
		sd_mu_free_msg(mbdev, txm);
		spin_unlock_irqrestore(&mbdev->msg_lock, flags);

		return ret;
	}

	sd_mu_send_msg(txm);
	spin_unlock_irqrestore(&mbdev->msg_lock, flags);

	while(sd_mu_is_msg_sending(txm));
	sd_mu_free_msg(mbdev, txm);

	return actual_size;
}

int mbox_back_request_channel(struct mbox_backend *be, const struct mbox_request_data *req)
{
	struct xenbus_device *xbdev = be->xb_dev;
	struct mbox_back_chan *mchan;
	int i;

	for (i = 0; i < MB_MAX_CHANS;i++) {
		mchan = &be->mchan[i];
		if (mchan->used == false) {
			mchan->used = true;
			mchan->osid = req->osid;
			mchan->mba  = req->mba;
			dev_info(&xbdev->dev, "request chan os=%d a=%x", mchan->osid, mchan->mba);
			return 0;
		}
	}

	return -ENOENT;
}

int mbox_back_free_channel(struct mbox_backend *be, const int mba)
{
	struct mbox_back_chan *mchan;
	int i;

	for (i = 0; i < MB_MAX_CHANS;i++) {
		mchan = &be->mchan[i];
		if (mchan->used && mchan->mba == mba) {
			mchan->used = false;
			mchan->osid = 0;
			mchan->mba  = 0;
			return 0;
		}
	}

	return -ENOENT;
}

static void process_front_request(struct mbox_back_channel *chan, const struct mbox_request_data *req)
{
	struct mbox_respond_data *rsp;
	struct mbox_backend *be = chan->be;
	struct xenbus_device *xbdev = be->xb_dev;
	RING_IDX idx = chan->back_ring.rsp_prod_pvt;
	int notify;
	int ret;

	if (!be) {
		dev_err(&xbdev->dev, "backend device is not initialized yet");
		return;
	}
	rsp = RING_GET_RESPONSE(&chan->back_ring, idx);
	rsp->req_id = req->req_id;

	switch (req->cmd) {
		case MBC_NEW_CHANNEL:
			ret = mbox_back_request_channel(be, req);
			rsp->result = ret;
			break;
		case MBC_DEL_CHANNEL:
			ret = mbox_back_free_channel(be, req->mba);
			rsp->result = ret;
			break;
		default:
			rsp->result = -ENOTSUPP;
	}
	chan->back_ring.rsp_prod_pvt = ++idx;
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&chan->back_ring, notify);
	if (notify)
		notify_remote_via_irq(chan->cmd_irq);
}

static bool mbox_has_request(struct mbox_back_channel *chan)
{
 	if (likely(RING_HAS_UNCONSUMED_REQUESTS(&chan->back_ring)))
 		return true;

 	return false;
}

static void mbox_work_handler(struct mbox_back_channel *chan)
{
	struct mbox_request_data *req;

	for (;;) {
		RING_IDX req_prod, req_cons;

		req_prod = chan->back_ring.sring->req_prod;
		req_cons = chan->back_ring.req_cons;

		/* Make sure we can see requests before we process them. */
		rmb();

		if (req_cons == req_prod)
			break;

		while (req_cons != req_prod) {

			req = RING_GET_REQUEST(&chan->back_ring, req_cons);
			req_cons++;

			process_front_request(chan, req);
		}

		chan->back_ring.req_cons = req_cons;
		chan->back_ring.sring->req_event = req_cons + 1;
	}
}

irqreturn_t mbox_back_cmd_interrupt(int irq, void *data)
{
	struct mbox_back_channel *chan = data;

	while (mbox_has_request(chan))
		mbox_work_handler(chan);

	return IRQ_HANDLED;
}

int mbox_connect_cmd(struct xenbus_device *xbdev, grant_ref_t ring_ref,
			unsigned int evtchn)
{
	struct mbox_backend *be = dev_get_drvdata(&xbdev->dev);
	struct mbox_back_channel *chan = be->channel;
	void *page;
	struct mbox_sring *shared;
	int err;

	err = xenbus_map_ring_valloc(xbdev, &ring_ref, 1, &page);
	if (err)
		goto err;

	shared = (struct mbox_sring *)page;
	BACK_RING_INIT(&chan->back_ring, shared, XEN_PAGE_SIZE);

	err = bind_interdomain_evtchn_to_irq(chan->domid, evtchn);
	if (err < 0)
		goto err_unmap;

	chan->cmd_irq = err;

	err = request_threaded_irq(chan->cmd_irq, NULL, mbox_back_cmd_interrupt,
	                           IRQF_ONESHOT, "mb-back", chan);
	if (err) {
		dev_err(&xbdev->dev, "Could not setup irq handler for ipc back\n");
		goto err_deinit;
	}

	return 0;

err_deinit:
	unbind_from_irqhandler(chan->cmd_irq, chan);
	chan->cmd_irq = 0;
err_unmap:
	xenbus_unmap_ring_vfree(xbdev, chan->back_ring.sring);
	chan->back_ring.sring = NULL;
err:

	return err;
}

int mbox_connect_data(struct xenbus_device *xbdev, grant_ref_t ring_ref,
			unsigned int evtchn)
{
	struct mbox_backend *be = dev_get_drvdata(&xbdev->dev);
	struct mbox_back_channel *chan = be->channel;
	void *page;
	int ret;

	chan->ref = ring_ref;
	ret = xenbus_map_ring_valloc(xbdev, &ring_ref, 1, &page);
	if (ret < 0)
		goto out;

	chan->intf = page;
	chan->ring_order = chan->intf->ring_order;
	/* first read the order, then chan the data ring */
	virt_rmb();
	if (chan->ring_order > MAX_RING_ORDER) {
		pr_warn("%s frontend requested ring_order %u, which is > MAX (%u)\n",
				__func__, chan->ring_order, MAX_RING_ORDER);
		goto out;
	}
	ret = xenbus_map_ring_valloc(xbdev, chan->intf->ref,
					 (1 << chan->ring_order), &page);
	if (ret < 0)
		goto out;
	chan->bytes = page;
	chan->data.in = chan->bytes;
	chan->data.out = chan->bytes + XEN_FLEX_RING_SIZE(chan->ring_order);

	chan->ioworker.wq = alloc_workqueue("mbox_io", WQ_HIGHPRI, 1);
	if (!chan->ioworker.wq)
		goto out;

	atomic_set(&chan->io, 1);
	INIT_WORK(&chan->ioworker.register_work, mbox_back_ioworker);

	ret = bind_interdomain_evtchn_to_irqhandler(xbdev->otherend_id,
							evtchn, mbox_back_write_event,
							0, "mbox-backend", chan);
	if (ret < 0)
		goto out;

	chan->io_irq = ret;
	return 0;

out:
	return ret;
}

static int mbox_backend_connect(struct xenbus_device *xbdev)
{
	struct mbox_backend *be = dev_get_drvdata(&xbdev->dev);
	struct mbox_back_channel *chan = be->channel;
	unsigned int val;
	grant_ref_t ring_ref;
	unsigned int evtchn;
	int err;

	err = xenbus_scanf(XBT_NIL, xbdev->otherend,
	                   "cmd-ring-ref", "%u", &val);
	if (err < 0) {
		xenbus_dev_fatal(xbdev, err, "reading %s/cmd-ring-ref",
				xbdev->otherend);
		goto fail;
	}

	ring_ref = val;
	err = xenbus_scanf(XBT_NIL, xbdev->otherend,
	                   "cmd-evt-chn", "%u", &val);
	if (err < 0) {
		xenbus_dev_fatal(xbdev, err, "reading %s/cmd-evt-chn",
				xbdev->otherend);
		goto fail;
	}

	evtchn = val;
	err = mbox_connect_cmd(xbdev, ring_ref, evtchn);
	if (err < 0) {
		xenbus_dev_fatal(xbdev, err,  "mapping cmd ring %u port %u",
				ring_ref, evtchn);
		goto fail;
	}

	err = xenbus_scanf(XBT_NIL, xbdev->otherend,
	                   "data-ring-ref", "%u", &val);
	if (err < 0) {
		xenbus_dev_fatal(xbdev, err, "reading %s/data-ring-ref",
				xbdev->otherend);
		goto fail;
	}

	ring_ref = val;
	err = xenbus_scanf(XBT_NIL, xbdev->otherend,
	                   "data-evt-chn", "%u", &val);
	if (err < 0) {
		xenbus_dev_fatal(xbdev, err, "reading %s/data-evt-chn",
				xbdev->otherend);
		goto fail;
	}

	evtchn = val;
	err = mbox_connect_data(xbdev, ring_ref, evtchn);
	if (err < 0) {
		xenbus_dev_fatal(xbdev, err,  "mapping data ring %u port %u",
				ring_ref, evtchn);
		goto fail;
	}

	/* indicate the backend channel is active for receiving data */
	atomic_set(&chan->connected, 1);

	return 0;

fail:
	return err;
}

static void mbox_backend_disconnect(struct xenbus_device *xbdev)
{
	struct mbox_backend *be = dev_get_drvdata(&xbdev->dev);
	struct mbox_back_channel *chan = be->channel;

	if (chan) {
		/* indicate the backend channel is inactive */
		atomic_set(&chan->connected, 0);
		if (chan->io_irq) {
			disable_irq(chan->io_irq);
			unbind_from_irqhandler(chan->io_irq, chan);
			chan->io_irq = 0;
		}
		atomic_set(&chan->release, 1);
		flush_work(&chan->ioworker.register_work);
		if (chan->bytes) {
			xenbus_unmap_ring_vfree(xbdev, chan->bytes);
			chan->bytes = 0;
		}
		if (chan->intf) {
			xenbus_unmap_ring_vfree(xbdev, (void *)chan->intf);
			chan->intf = 0;
		}
		if (chan->cmd_irq) {
			unbind_from_irqhandler(chan->cmd_irq, chan);
			chan->cmd_irq = 0;
		}
		if (chan->back_ring.sring) {
			xenbus_unmap_ring_vfree(xbdev, chan->back_ring.sring);
			chan->back_ring.sring = NULL;
		}
	}
}

static void mbox_free_channel(struct mbox_back_channel *chan)
{
	return;
}

static struct mbox_back_channel *
mbox_alloc_channel(struct xenbus_device *xbdev, domid_t domid)
{
	struct mbox_back_channel *chan;

	chan = devm_kzalloc(&xbdev->dev, sizeof(struct mbox_back_channel), GFP_KERNEL);
	if (!chan)
		return ERR_PTR(-ENOMEM);

	chan->domid = domid;

	return chan;
}

int mbox_register_backend(struct mbox_backend *be)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&mbox_be_lock, flags);
	for (i = 0; i < MAX_PV_MBOXES;i++) {
		if (mbox_backends[i] == NULL) {
			mbox_backends[i] = be;
			spin_unlock_irqrestore(&mbox_be_lock, flags);
			return i;
		}
	}
	spin_unlock_irqrestore(&mbox_be_lock, flags);

	return -ENOENT;
}

void mbox_unregister_backend(struct mbox_backend *be)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&mbox_be_lock, flags);
	for (i = 0; i < MAX_PV_MBOXES;i++) {
		if (mbox_backends[i] == be) {
			mbox_backends[i] = NULL;
			break;
		}
	}
	spin_unlock_irqrestore(&mbox_be_lock, flags);
}

struct mbox_backend *mbox_find_backend(unsigned int osid, unsigned int mba)
{
	struct mbox_backend *be = NULL;
	struct mbox_back_chan *mchan;
	unsigned long flags;
	int i, j;

	spin_lock_irqsave(&mbox_be_lock, flags);
	for (i = 0;i < MAX_PV_MBOXES;i++) {
		be = mbox_backends[i];
		if (!be)
			continue;

		for (j = 0; j < MB_MAX_CHANS;j++) {
			mchan = &be->mchan[j];
			if (mchan->used) {
				if ((osid && (mchan->osid == osid || mchan->mba == mba)) ||
					/* osid == 0 is for legacy support */
					(!osid && mchan->mba == mba)) {
					spin_unlock_irqrestore(&mbox_be_lock, flags);
					return be;
				}
			}
		}
	}
	spin_unlock_irqrestore(&mbox_be_lock, flags);

	return NULL;
}

static int mbox_back_remove(struct xenbus_device *xbdev)
{
	struct mbox_backend *be = dev_get_drvdata(&xbdev->dev);

	xenbus_switch_state(xbdev, XenbusStateClosed);

	mbox_unregister_backend(be);

	if (be)
		mbox_backend_disconnect(xbdev);

	if (be->channel)
		mbox_free_channel(be->channel);

	be->inited = false;
	dev_set_drvdata(&xbdev->dev, NULL);

	return 0;
}

/**
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures and switch to InitWait.
 */
extern struct sd_mbox_device *g_mbdev;
static int mbox_back_probe(struct xenbus_device *xbdev,
                         const struct xenbus_device_id *id)
{
	int err;
	struct mbox_backend *be;

	be = devm_kzalloc(&xbdev->dev, sizeof(struct mbox_backend), GFP_KERNEL);
	if (!be) {
		xenbus_dev_fatal(xbdev, -ENOMEM, "allocating backend structure");
		dev_err(&xbdev->dev, "allocating backend structure\n");
		return -ENOMEM;
	}

	be->xb_dev = xbdev;
	be->channel = mbox_alloc_channel(xbdev, xbdev->otherend_id);
	if (IS_ERR(be->channel)) {
		err = PTR_ERR(be->channel);
		be->channel = NULL;
		xenbus_dev_fatal(xbdev, err, "creating mbox channel");
		goto fail;
	}

	be->channel->be = be;
	dev_set_drvdata(&xbdev->dev, be);
	be->inited = true;
	be->mbdev = g_mbdev;

	BUG_ON(!be->mbdev);

	err = mbox_register_backend(be);
	if (err < 0)
		goto fail;

	err = xenbus_switch_state(xbdev, XenbusStateInitWait);
	if (err)
		goto fail;

	dev_info(&xbdev->dev, "%s done from domid: %d\n", __func__, xbdev->otherend_id);

	return 0;

fail:
	dev_err(&xbdev->dev, "%s: failed\n", __func__);
	mbox_back_remove(xbdev);

	return err;
}

/*
 * Callback received when the frontend's state changes.
 */
static void mbox_front_changed(struct xenbus_device *xbdev,
                             enum xenbus_state frontend_state)
{
	int err;

	dev_info(&xbdev->dev, "in %s front(%s) back(%s)\n", __func__,
			xenbus_strstate(frontend_state),xenbus_strstate(xbdev->state));

	switch (frontend_state) {
	case XenbusStateInitialising:
		if (xbdev->state == XenbusStateClosed) {
		    dev_info(&xbdev->dev, "%s: prepare for reconnect\n", xbdev->nodename);
		    xenbus_switch_state(xbdev, XenbusStateInitWait);
		}
		break;

	case XenbusStateInitialised:
	case XenbusStateConnected:
		/*
		 * Ensure we connect even when two watches fire in
		 * close succession and we miss the intermediate value
		 * of frontend_state.
		 */
		if (xbdev->state == XenbusStateConnected)
			break;

		err = mbox_backend_connect(xbdev);
		if (err) {
			/*
			 * Clean up so that memory resources can be used by
			 * other devices. connect_ring reported already error.
			 */
			mbox_backend_disconnect(xbdev);
			break;
		}
		xenbus_switch_state(xbdev, XenbusStateConnected);
		break;

	case XenbusStateClosing:
		xenbus_switch_state(xbdev, XenbusStateClosing);
		break;

	case XenbusStateClosed:
		mbox_backend_disconnect(xbdev);
		xenbus_switch_state(xbdev, XenbusStateClosed);
		if (xenbus_dev_is_online(xbdev))
			break;
		/* fall through */
		/* if not online */
	case XenbusStateUnknown:
		device_unregister(&xbdev->dev);
		break;

	default:
		xenbus_dev_fatal(xbdev, -EINVAL, "saw state %d at frontend",
				frontend_state);
	    break;
	}
}


static const struct xenbus_device_id mbox_back_ids[] = {
	{ XEN_MBOX_DEVICE_ID },
	{ "" }
};

static struct xenbus_driver mbox_back_driver = {
	.ids = mbox_back_ids,
	.probe = mbox_back_probe,
	.remove = mbox_back_remove,
	.otherend_changed = mbox_front_changed,
};

static int __init sd_mbox_back_init(void)
{
	if (!xen_domain() || !xen_initial_domain())
		return -ENODEV;

	pr_info("register mailbox backend\n");
	return xenbus_register_backend(&mbox_back_driver);
}
module_init(sd_mbox_back_init);

static void __exit sd_mbox_back_exit(void)
{
	if (!xen_domain() || !xen_initial_domain())
		return;

	xenbus_unregister_driver(&mbox_back_driver);

	return;
}
module_exit(sd_mbox_back_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Semidrive Mailbox Backend Driver");
MODULE_AUTHOR("ye.liu@semidrive.com");
