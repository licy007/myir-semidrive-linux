/*
 * Copyright (C) 2020 Semidrive Semiconductor, Inc.
 *
 * derived from the imx-rpmsg implementation.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/semidrive-mailbox.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/sys_counter.h>
#include "rpmsg_internal.h"

/* The feature bitmap for virtio rpmsg */
#define RPMSG_F_NS 0 /* RP supports name service notifications */
#define RPMSG_F_ECHO 1 /* RP supports echo test */
#define RPMSG_RESERVED_ADDRESSES (1024)
#define RPMSG_NS_ADDR (53)
#define RPMSG_ECHO_ADDR (30)
#define RPMSG_DEFAULT_HW_MTU (512)
#define RPMSG_DEFAULT_TXD_NUM (16) /* Not exceed mbox chan tq length */

/* DEBUG purpose */
#define CONFIG_IPCC_DUMP_HEX (0)

#define RPMSG_IPCC_DOWN_INTERVAL msecs_to_jiffies(2000)
#define RPMSG_IPCC_SYNC_INTERVAL msecs_to_jiffies(100)
#define RPMSG_IPCC_SYNA_FALLBACK msecs_to_jiffies(200)

struct rpmsg_ipcc_hdr {
	sd_msghdr_t mboxhdr;
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __packed;

struct rpmsg_ipcc_nsm {
	char name[RPMSG_NAME_SIZE];
	u32 addr;
	u32 flags;
} __packed;

enum rpmsg_ns_flags {
	RPMSG_NS_CREATE = 0,
	RPMSG_NS_DESTROY = 1,
};

typedef struct {
	uint32_t des0;
	uint32_t des1;
	uint32_t des2;
	uint32_t des3;
	void *pbuf;
	char pad[16 - sizeof(void *)];
} rpmsg_desc_t;

struct rpmsg_ipcc_device {
	struct device *dev;
	int rproc;
	struct mbox_client client;
	struct list_head node;
	struct mbox_chan *mbox;
	struct rpmsg_endpoint *ns_ept;
	struct delayed_work ns_work;

#define RPMSG_IPCC_LINK_DOWN (0)
#define RPMSG_IPCC_LINK_SYNC (1)
#define RPMSG_IPCC_LINK_SYNA (2)
#define RPMSG_IPCC_LINK_UP (3)
#define RPMSG_IPCC_LINK_ERRSTATE (4)
	int link_status; /* for medium link establish and destroy */
#define RPMSG_IPCC_LINK_RETRY_NUM (100)
	int link_retry; /* retry counter before give up */

	struct rpmsg_endpoint *echo_ept;
	struct idr endpoints;
	struct mutex endpoints_lock;
	unsigned int hw_mtu; /* the mbox mtu */
	unsigned int mtu; /* the rpmsg mtu */

	/* TX */
	wait_queue_head_t sendq;
	spinlock_t txlock;
	void *tx_buf_base;
	rpmsg_desc_t *tx_desc;
	uint32_t tx_desc_num;
	uint32_t tx_desc_avail;

	/* RX */
	struct sk_buff_head rxq;
	struct work_struct rx_work;
};

struct rpmsg_ipcc_channel {
	struct rpmsg_device rpdev;

	struct rpmsg_ipcc_device *vrp;
};

#define to_rpmsg_ipcc_channel(_rpdev)                                          \
	container_of(_rpdev, struct rpmsg_ipcc_channel, rpdev)

#define to_rpmsg_ipcc_device(_rpdev)                                           \
	container_of(_rpdev, struct rpmsg_ipcc_device, rpdev)

static void rpmsg_ipcc_destroy_ept(struct rpmsg_endpoint *ept);
static int rpmsg_ipcc_send(struct rpmsg_endpoint *ept, void *data, int len);
static int rpmsg_ipcc_sendto(struct rpmsg_endpoint *ept, void *data, int len,
			     u32 dst);
static int rpmsg_ipcc_send_offchannel(struct rpmsg_endpoint *ept, u32 src,
				      u32 dst, void *data, int len);
static int rpmsg_ipcc_trysend(struct rpmsg_endpoint *ept, void *data, int len);
static int rpmsg_ipcc_trysendto(struct rpmsg_endpoint *ept, void *data, int len,
				u32 dst);
static int rpmsg_ipcc_trysend_offchannel(struct rpmsg_endpoint *ept, u32 src,
					 u32 dst, void *data, int len);
static unsigned int rpmsg_ipcc_poll(struct rpmsg_endpoint *ept,
				    struct file *filp, poll_table *wait);
static ssize_t rpmsg_ipcc_get_mtu(struct rpmsg_endpoint *ept);

static const struct rpmsg_endpoint_ops rpmsg_ipcc_endpoint_ops = {
	.destroy_ept = rpmsg_ipcc_destroy_ept,
	.send = rpmsg_ipcc_send,
	.sendto = rpmsg_ipcc_sendto,
	.send_offchannel = rpmsg_ipcc_send_offchannel,
	.trysend = rpmsg_ipcc_trysend,
	.trysendto = rpmsg_ipcc_trysendto,
	.trysend_offchannel = rpmsg_ipcc_trysend_offchannel,
	.poll = rpmsg_ipcc_poll,
	.get_mtu = rpmsg_ipcc_get_mtu,
};

static LIST_HEAD(ipcc_cons);
static DEFINE_MUTEX(con_mutex);

static int ipcc_has_feature(struct rpmsg_ipcc_device *vrp, int feature)
{
	return 1;
}

static void __ept_release(struct kref *kref)
{
	struct rpmsg_endpoint *ept =
		container_of(kref, struct rpmsg_endpoint, refcount);
	/*
	 * At this point no one holds a reference to ept anymore,
	 * so we can directly free it
	 */
	kfree(ept);
}

/* for more info, see below documentation of rpmsg_create_ept() */
static struct rpmsg_endpoint *__rpmsg_create_ept(struct rpmsg_ipcc_device *vrp,
						 struct rpmsg_device *rpdev,
						 rpmsg_rx_cb_t cb, void *priv,
						 u32 addr)
{
	int id_min, id_max, id;
	struct rpmsg_endpoint *ept;
	struct device *dev = &rpdev->dev;

	ept = kzalloc(sizeof(*ept), GFP_KERNEL);
	if (!ept)
		return NULL;

	kref_init(&ept->refcount);
	mutex_init(&ept->cb_lock);

	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &rpmsg_ipcc_endpoint_ops;

	/* do we need to allocate a local address ? */
	if (addr == RPMSG_ADDR_ANY) {
		id_min = RPMSG_RESERVED_ADDRESSES;
		id_max = 0;
	} else {
		id_min = addr;
		id_max = addr + 1;
	}

	mutex_lock(&vrp->endpoints_lock);

	/* bind the endpoint to an rpmsg address (and allocate one if needed) */
	id = idr_alloc(&vrp->endpoints, ept, id_min, id_max, GFP_KERNEL);
	if (id < 0) {
		dev_err(dev, "idr_alloc failed: %d\n", id);
		goto free_ept;
	}
	ept->addr = id;

	mutex_unlock(&vrp->endpoints_lock);

	return ept;

free_ept:
	mutex_unlock(&vrp->endpoints_lock);
	kref_put(&ept->refcount, __ept_release);
	return NULL;
}

static struct rpmsg_endpoint *
rpmsg_ipcc_create_ept(struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		      struct rpmsg_channel_info chinfo)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	return __rpmsg_create_ept(vrp, rpdev, cb, priv, chinfo.src);
}

/**
 * __rpmsg_destroy_ept() - destroy an existing rpmsg endpoint
 * @vrp: virtproc which owns this ept
 * @ept: endpoing to destroy
 *
 * An internal function which destroy an ept without assuming it is
 * bound to an rpmsg channel. This is needed for handling the internal
 * name service endpoint, which isn't bound to an rpmsg channel.
 * See also __rpmsg_create_ept().
 */
static void __rpmsg_destroy_ept(struct rpmsg_ipcc_device *vrp,
				struct rpmsg_endpoint *ept)
{
	/* make sure new inbound messages can't find this ept anymore */
	mutex_lock(&vrp->endpoints_lock);
	idr_remove(&vrp->endpoints, ept->addr);
	mutex_unlock(&vrp->endpoints_lock);

	/* make sure callbacks are exclusive with us */
	mutex_lock(&ept->cb_lock);
	ept->cb = NULL;
	mutex_unlock(&ept->cb_lock);

	kref_put(&ept->refcount, __ept_release);
}

static void rpmsg_ipcc_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(ept->rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	__rpmsg_destroy_ept(vrp, ept);
}

static int rpmsg_ipcc_announce_create(struct rpmsg_device *rpdev)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;
	struct device *dev = &rpdev->dev;
	int err = 0;

	/* need to tell remote processor's name service about this channel ? */
	if (rpdev->announce && rpdev->ept &&
	    ipcc_has_feature(vrp, RPMSG_F_NS)) {
		struct rpmsg_ipcc_nsm nsm;

		strncpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
		nsm.addr = rpdev->ept->addr;
		nsm.flags = RPMSG_NS_CREATE;

		err = rpmsg_sendto(rpdev->ept, &nsm, sizeof(nsm),
				   RPMSG_NS_ADDR);
		if (err)
			dev_err(dev, "failed to announce service %d\n", err);
	}

	return err;
}

static int rpmsg_ipcc_announce_destroy(struct rpmsg_device *rpdev)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;
	struct device *dev = &rpdev->dev;
	int err = 0;

	/* tell remote processor's name service we're removing this channel */
	if (rpdev->announce && rpdev->ept &&
	    ipcc_has_feature(vrp, RPMSG_F_NS)) {
		struct rpmsg_ipcc_nsm nsm;

		strncpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
		nsm.addr = rpdev->ept->addr;
		nsm.flags = RPMSG_NS_DESTROY;

		err = rpmsg_sendto(rpdev->ept, &nsm, sizeof(nsm),
				   RPMSG_NS_ADDR);
		if (err)
			dev_err(dev, "failed to announce service %d\n", err);
	}

	return err;
}

static const struct rpmsg_device_ops rpmsg_ipcc_devops = {
	.create_ept = rpmsg_ipcc_create_ept,
	.announce_create = rpmsg_ipcc_announce_create,
	.announce_destroy = rpmsg_ipcc_announce_destroy,
};

static void rpmsg_ipcc_release_device(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);

	kfree(vch);
}

/*
 * create an rpmsg channel using its name and address info.
 * this function will be used to create both static and dynamic
 * channels.
 */
static struct rpmsg_device *
rpmsg_create_channel(struct rpmsg_ipcc_device *vrp,
		     struct rpmsg_channel_info *chinfo)
{
	struct rpmsg_ipcc_channel *vch;
	struct rpmsg_device *rpdev;
	struct device *tmp, *dev = vrp->dev;
	int ret;

	/* make sure a similar channel doesn't already exist */
	tmp = rpmsg_find_device(dev, chinfo);
	if (tmp) {
		/* decrement the matched device's refcount back */
		put_device(tmp);
		dev_err(dev, "channel %s:%x:%x already exist\n", chinfo->name,
			chinfo->src, chinfo->dst);
		return NULL;
	}

	vch = kzalloc(sizeof(*vch), GFP_KERNEL);
	if (!vch)
		return NULL;

	/* Link the channel to our vrp */
	vch->vrp = vrp;

	/* Assign public information to the rpmsg_device */
	rpdev = &vch->rpdev;
	rpdev->src = chinfo->src;
	rpdev->dst = chinfo->dst;
	rpdev->ops = &rpmsg_ipcc_devops;

	/*
	 * rpmsg server channels has predefined local address (for now),
	 * and their existence needs to be announced remotely
	 */
	rpdev->announce = rpdev->src != RPMSG_ADDR_ANY;

	strncpy(rpdev->id.name, chinfo->name, RPMSG_NAME_SIZE);

	rpdev->dev.parent = vrp->dev;
	rpdev->dev.release = rpmsg_ipcc_release_device;
	ret = rpmsg_register_device(rpdev);
	if (ret)
		return NULL;

	return rpdev;
}

static rpmsg_desc_t *acquire_tx_desc(struct rpmsg_ipcc_device *vrp)
{
	rpmsg_desc_t *tdesc;
	unsigned long flags;
	int i;

	/* support multiple concurrent senders */
	spin_lock_irqsave(&vrp->txlock, flags);
	if (vrp->tx_desc_avail == 0) {
		spin_unlock_irqrestore(&vrp->txlock, flags);
		return NULL;
	}

	for (i = 0; i < vrp->tx_desc_num; i++) {
		tdesc = &(vrp->tx_desc[i]);
		if (tdesc->des1 == 0) {
			tdesc->des1 = 1; // clear once tx acknowledge
			tdesc->des2 = i;
			vrp->tx_desc_avail--;
			spin_unlock_irqrestore(&vrp->txlock, flags);
			return tdesc;
		}
	}
	spin_unlock_irqrestore(&vrp->txlock, flags);

	return NULL;
}

static int tx_desc_available(struct rpmsg_ipcc_device *vrp)
{
	unsigned long flags;
	uint32_t avail;

	spin_lock_irqsave(&vrp->txlock, flags);
	avail = vrp->tx_desc_avail;
	spin_unlock_irqrestore(&vrp->txlock, flags);
	return avail;
}
static void __release_tx_desc(struct rpmsg_ipcc_device *vrp,
			      rpmsg_desc_t *tdesc)
{
	/* Only reap used tdesc */
	if (tdesc->des1) {
		tdesc->des1 = 0;
		tdesc->des2 = 0;
		vrp->tx_desc_avail++;
		//dev_err(vrp->dev, "tx_desc release avail=%d\n", vrp->tx_desc_avail);
	}
}

static int release_tx_desc(struct rpmsg_ipcc_device *vrp, void *mssg)
{
	rpmsg_desc_t *tdesc;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&vrp->txlock, flags);
	for (i = 0; i < vrp->tx_desc_num; i++) {
		tdesc = &(vrp->tx_desc[i]);
		if (tdesc->pbuf == mssg) {
			__release_tx_desc(vrp, tdesc);
			spin_unlock_irqrestore(&vrp->txlock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&vrp->txlock, flags);

	return -ENOENT;
}

static void send_mailbox_trace(struct rpmsg_ipcc_device *vrp,
			       struct rpmsg_ipcc_hdr *msg)
{
	struct device *dev = vrp->dev;

	dev_dbg(dev, "TX From 0x%x, To 0x%x, Len %d, Flags %d, Reserved %d\n",
		msg->src, msg->dst, msg->len, msg->flags, msg->reserved);

#if CONFIG_IPCC_DUMP_HEX
	print_hex_dump_bytes("rpmsg_ipcc TX: ", DUMP_PREFIX_NONE, msg,
			     sizeof(*msg) + msg->len);
#endif
}

static int __send_to_mailbox(struct rpmsg_ipcc_device *vrp,
			     struct rpmsg_ipcc_hdr *msg)
{
	int ret = 0;

	send_mailbox_trace(vrp, msg);

	ret = mbox_send_message(vrp->mbox, msg);
	ret = (ret < 0) ? ret : 0;

	return ret;
}

inline static void init_mbox_msg_hdr(struct rpmsg_ipcc_hdr *msg, int rp,
				     u32 src, u32 dst, void *data, int len)
{
	MB_MSG_INIT_RPMSG_HEAD(&msg->mboxhdr, rp, len + sizeof(*msg), 0);
	msg->len = len;
	msg->flags = 0;
	msg->src = src;
	msg->dst = dst;
	msg->reserved = 0;

	if (data)
		memcpy(msg->data, data, len);
}

static int __send_offchannel_raw(struct rpmsg_ipcc_device *vrp, u32 src,
				 u32 dst, void *data, int len, bool wait)
{
	struct device *dev = vrp->dev;
	struct rpmsg_ipcc_hdr *msg;
	rpmsg_desc_t *tdesc;
	int ret = 0;

	/* bcasting isn't allowed */
	if (src == RPMSG_ADDR_ANY || dst == RPMSG_ADDR_ANY) {
		dev_err(dev, "invalid addr (src 0x%x, dst 0x%x)\n", src, dst);
		return -EINVAL;
	}

	/*
	* We currently use fixed-sized buffers, and therefore the payload
	* length is limited.
	*
	* One of the possible improvements here is either to support
	* user-provided buffers (and then we can also support zero-copy
	* messaging), or to improve the buffer allocator, to support
	* variable-length buffer sizes.
	*/
	if (len > vrp->mtu) {
		dev_err(dev, "message is too big (%d>%d)\n", len, vrp->mtu);
		return -EMSGSIZE;
	}

	/* grab a buffer */
	tdesc = acquire_tx_desc(vrp);
	if (!tdesc && !wait)
		return -ENOMEM;

	/* no free buffer ? wait for one (but bail after 15 seconds) */
	while (!tdesc) {
		/*
		* sleep until a free buffer is available or 15 secs elapse.
		* the timeout period is not configurable because there's
		* little point in asking drivers to specify that.
		* if later this happens to be required, it'd be easy to add.
		*/
		ret = wait_event_interruptible_timeout(
			vrp->sendq, (tdesc = acquire_tx_desc(vrp)),
			msecs_to_jiffies(15000));

		/* timeout ? */
		if (!ret) {
			dev_err(dev, "timeout waiting for a tx buffer\n");
			return -ERESTARTSYS;
		}
	}

	msg = (struct rpmsg_ipcc_hdr *)tdesc->pbuf;
	init_mbox_msg_hdr(msg, vrp->rproc, src, dst, data, len);

	ret = __send_to_mailbox(vrp, msg);

	return ret;
}

static int rpmsg_ipcc_send_offchannel_raw(struct rpmsg_device *rpdev, u32 src,
					  u32 dst, void *data, int len,
					  bool wait)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	return __send_offchannel_raw(vrp, src, dst, data, len, wait);
}

static int rpmsg_ipcc_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct rpmsg_device *rpdev = ept->rpdev;
	u32 src = ept->addr, dst = rpdev->dst;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static int rpmsg_ipcc_sendto(struct rpmsg_endpoint *ept, void *data, int len,
			     u32 dst)
{
	struct rpmsg_device *rpdev = ept->rpdev;
	u32 src = ept->addr;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static int rpmsg_ipcc_send_offchannel(struct rpmsg_endpoint *ept, u32 src,
				      u32 dst, void *data, int len)
{
	struct rpmsg_device *rpdev = ept->rpdev;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static int rpmsg_ipcc_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct rpmsg_device *rpdev = ept->rpdev;
	u32 src = ept->addr, dst = rpdev->dst;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len,
					      false);
}

static int rpmsg_ipcc_trysendto(struct rpmsg_endpoint *ept, void *data, int len,
				u32 dst)
{
	struct rpmsg_device *rpdev = ept->rpdev;
	u32 src = ept->addr;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len,
					      false);
}

static int rpmsg_ipcc_trysend_offchannel(struct rpmsg_endpoint *ept, u32 src,
					 u32 dst, void *data, int len)
{
	struct rpmsg_device *rpdev = ept->rpdev;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len,
					      false);
}

static unsigned int rpmsg_ipcc_poll(struct rpmsg_endpoint *ept,
				    struct file *filp, poll_table *wait)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(ept->rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;
	unsigned int mask = 0;

	poll_wait(filp, &vrp->sendq, wait);

	if (tx_desc_available(vrp))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static ssize_t rpmsg_ipcc_get_mtu(struct rpmsg_endpoint *ept)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(ept->rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	return vrp->mtu;
}
static int rpmsg_ipcc_recv(struct rpmsg_ipcc_device *vrp, unsigned char *data,
			   unsigned int len)
{
	struct device *dev = vrp->dev;
	struct rpmsg_endpoint *ept;
	struct rpmsg_ipcc_hdr *msg = (struct rpmsg_ipcc_hdr *)data;

	dev_dbg(dev, "From: 0x%x, To: 0x%x, Len: %d, Flags: %d, Reserved: %d\n",
		msg->src, msg->dst, msg->len, msg->flags, msg->reserved);
#if CONFIG_IPCC_DUMP_HEX
	print_hex_dump_bytes("rpmsg_ipcc RX: ", DUMP_PREFIX_ADDRESS, msg,
			     sizeof(*msg) + msg->len);
#endif
	/*
	 * We currently use fixed-sized buffers, so trivially sanitize
	 * the reported payload length.
	 */
	if (len > vrp->hw_mtu ||
	    msg->len > (len - sizeof(struct rpmsg_ipcc_hdr))) {
		dev_warn(dev, "inbound msg too big: (%d, %d)\n", len, msg->len);
		return -EINVAL;
	}

	/* use the dst addr to fetch the callback of the appropriate user */
	mutex_lock(&vrp->endpoints_lock);

	ept = idr_find(&vrp->endpoints, msg->dst);

	/* let's make sure no one deallocates ept while we use it */
	if (likely(ept))
		kref_get(&ept->refcount);

	mutex_unlock(&vrp->endpoints_lock);

	if (likely(ept)) {
		/* make sure ept->cb doesn't go away while we use it */
		mutex_lock(&ept->cb_lock);

		if (ept->cb)
			ept->cb(ept->rpdev, msg->data, msg->len, ept->priv,
				msg->src);

		mutex_unlock(&ept->cb_lock);

		/* farewell, ept, we don't need you anymore */
		kref_put(&ept->refcount, __ept_release);
	} else
		dev_warn_ratelimited(
			dev, "msg (%d->%d) received with no recipient\n",
			msg->src, msg->dst);

	/* TODO: return the buffer back to memory pool */

	return 0;
}

static void _init_ns_announce_delayed(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rpmsg_ipcc_device *vrp =
		container_of(dwork, struct rpmsg_ipcc_device, ns_work);
	struct rpmsg_channel_info chinfo;
	struct rpmsg_ipcc_nsm nsm;
	struct device *tmp;
	int ret;

	if (vrp->link_status == RPMSG_IPCC_LINK_UP) {
		dev_info(vrp->dev, "confirm link up\n");
		return;
	}

	if (vrp->link_status == RPMSG_IPCC_LINK_SYNA) {
		vrp->link_status = RPMSG_IPCC_LINK_UP;
		dev_info(vrp->dev, "fallback link up\n");
		return;
	}

	strncpy(nsm.name, "ipcc-echo", RPMSG_NAME_SIZE);
	nsm.addr = RPMSG_ECHO_ADDR;
	nsm.flags = RPMSG_NS_CREATE;
	ret = __send_offchannel_raw(vrp, RPMSG_ECHO_ADDR, RPMSG_NS_ADDR, &nsm,
				    sizeof(nsm), true);
	if (ret) {
		dev_warn(vrp->dev, "failed to announce %d, retry\n", ret);
		schedule_delayed_work(&vrp->ns_work, RPMSG_IPCC_DOWN_INTERVAL);
		return;
	}

	vrp->link_status = RPMSG_IPCC_LINK_SYNC;
	vrp->link_retry++;
	dev_info(vrp->dev, "[%u] link sync c:%d\n", semidrive_get_syscntr(),
		 vrp->link_retry);

	/* make sure p2p rpmsg channel is created
	 * announce ns shake hand
	 */
	strncpy(chinfo.name, nsm.name, RPMSG_NAME_SIZE);
	chinfo.src = RPMSG_ADDR_ANY;
	chinfo.dst = RPMSG_ECHO_ADDR;
	tmp = rpmsg_find_device(vrp->dev, &chinfo);
	if (tmp) {
		put_device(tmp);

		dev_info(vrp->dev, "[%u] link %d -> SYNA\n",
			 semidrive_get_syscntr(), vrp->link_status);
		vrp->link_status = RPMSG_IPCC_LINK_SYNA;

		/* wait a moment for nsm comming */
		schedule_delayed_work(&vrp->ns_work, RPMSG_IPCC_SYNA_FALLBACK);
		vrp->link_retry = 0; /* reset retry counter */
		return;
	}

	if (vrp->link_retry < RPMSG_IPCC_LINK_RETRY_NUM)
		schedule_delayed_work(&vrp->ns_work, RPMSG_IPCC_SYNC_INTERVAL);
	else
		dev_err(vrp->dev, "[%u] link fail after retry %d\n",
			semidrive_get_syscntr(), vrp->link_retry);
}

static void rpmsg_ipcc_recv_task(struct work_struct *work)
{
	struct rpmsg_ipcc_device *vrp =
		container_of(work, struct rpmsg_ipcc_device, rx_work);
	struct sk_buff *skb;

	/* exhaust the rx skb queue for schedule work may lost */
	for (;;) {
		/* check for data in the queue */
		if (skb_queue_empty(&vrp->rxq)) {
			return;
		}

		skb = skb_dequeue(&vrp->rxq);
		if (unlikely(!skb))
			return;

		rpmsg_ipcc_recv(vrp, skb->data, skb->len);

		kfree_skb(skb);
	}
}

static void __mbox_rx_cb(struct mbox_client *client, void *mssg)
{
	struct rpmsg_ipcc_device *vrp =
		container_of(client, struct rpmsg_ipcc_device, client);
	struct device *dev = client->dev;
	struct sk_buff *skb;
	sd_msghdr_t *msghdr = mssg;
	int len;
	void *tmp;

	if (unlikely(!msghdr)) {
		dev_err(dev, "%s NULL mssg\n", __func__);
		return;
	}

	len = msghdr->dat_len;
	dev_dbg(dev, "msghdr %p (%x %d %d %d)\n", msghdr, msghdr->addr, len,
		msghdr->protocol, msghdr->rproc);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (unlikely(!skb))
		return;

	tmp = skb_put(skb, len);
	copy_from_mbox(tmp, mssg, len);

	skb_queue_tail(&vrp->rxq, skb);

	queue_work(system_unbound_wq, &vrp->rx_work);
	//schedule_work(&vrp->rx_work);

	return;
}

/*
 * This is invoked whenever the remote processor completed processing
 * a TX msg we just sent it, and the buffer is put back to the used ring.
 *
 * Normally, though, we suppress this "tx complete" interrupt in order to
 * avoid the incurred overhead.
 */
static void __mbox_tx_done(struct mbox_client *client, void *mssg, int r)
{
	struct rpmsg_ipcc_device *vrp =
		container_of(client, struct rpmsg_ipcc_device, client);

	release_tx_desc(vrp, mssg);
	/* wake up potential senders that are waiting for a tx buffer */
	wake_up_interruptible(&vrp->sendq);
}

static int rpmsg_ipcc_recover_announce(struct device *dev, void *data)
{
	struct rpmsg_device *rpdev =
		container_of(dev, struct rpmsg_device, dev);

	return rpmsg_ipcc_announce_create(rpdev);
}

/* This is called when link with remote peer is reset for eg. reboot */
static void rpmsg_ipcc_device_recovery(struct rpmsg_ipcc_device *vrp)
{
	schedule_delayed_work(&vrp->ns_work, 0);

	device_for_each_child(vrp->dev, NULL, rpmsg_ipcc_recover_announce);
}

/* invoked when a name service announcement arrives */
static int rpmsg_ipcc_ns_cb(struct rpmsg_device *rpdev, void *data, int len,
			    void *priv, u32 src)
{
	struct rpmsg_ipcc_nsm *msg = data;
	struct rpmsg_device *newch;
	struct rpmsg_channel_info chinfo;
	struct rpmsg_ipcc_device *vrp = priv;
	struct device *dev = vrp->dev;
	int ret;

#if defined(CONFIG_DYNAMIC_DEBUG)
	dynamic_hex_dump("NS announcement: ", DUMP_PREFIX_NONE, 16, 1, data,
			 len, true);
#endif

	dev_dbg(dev, "rpmsg_ipcc_ns_cb in\n");

	if (len != sizeof(*msg)) {
		dev_err(dev, "malformed ns msg (%d)\n", len);
		return -EINVAL;
	}

	/*
	 * the name service ept does _not_ belong to a real rpmsg channel,
	 * and is handled by the rpmsg bus itself.
	 * for sanity reasons, make sure a valid rpdev has _not_ sneaked
	 * in somehow.
	 */
	if (rpdev) {
		dev_err(dev, "anomaly: ns ept has an rpdev handle\n");
		return -EINVAL;
	}

	/* don't trust the remote processor for null terminating the name */
	msg->name[RPMSG_NAME_SIZE - 1] = '\0';

	dev_info(dev, "nsm in, %sing %s addr 0x%x\n",
		 msg->flags & RPMSG_NS_DESTROY ? "destroy" : "creat", msg->name,
		 msg->addr);

	strncpy(chinfo.name, msg->name, sizeof(chinfo.name));
	chinfo.src = RPMSG_ADDR_ANY;
	chinfo.dst = msg->addr;

	if (msg->flags & RPMSG_NS_DESTROY) {
		ret = rpmsg_unregister_device(vrp->dev, &chinfo);
		if (ret)
			dev_err(dev, "rpmsg_destroy_channel failed: %d\n", ret);
	} else {
		struct device *tmp;

		tmp = rpmsg_find_device(dev, &chinfo);
		if (!tmp) {
			newch = rpmsg_create_channel(vrp, &chinfo);
			if (!newch)
				dev_err(dev, "rpmsg_create_channel failed\n");
		} else {
			/* decrement the matched device's refcount back */
			put_device(tmp);
		}

		/* ignore if it's not a echo_endpoint create nsm */
		if (msg->addr != RPMSG_ECHO_ADDR) {
			return 0;
		}

		switch (vrp->link_status) {
		case RPMSG_IPCC_LINK_DOWN:
			/*
			 * This rare case happens when current OS boot later
			 * than remote OS and first nsm is not scheduled yet.
			 */
			dev_warn(dev, "receive nsm in link down, reply\n");
			schedule_delayed_work(&vrp->ns_work, 0);
			break;

		case RPMSG_IPCC_LINK_SYNC:
			/*
			 * Nothing to do, wait for next delayed nsm annouce
			 */
			break;

		case RPMSG_IPCC_LINK_SYNA:
			dev_info(dev, "[%u] link SYNA -> up\n",
				 semidrive_get_syscntr());
			vrp->link_status = RPMSG_IPCC_LINK_UP;
			break;

		case RPMSG_IPCC_LINK_UP:
			/*
			 * This is an exception, peer likely reboot
			 */
			dev_warn(dev, "nsm after linkup, peer maybe reboot\n");
			vrp->link_status = RPMSG_IPCC_LINK_DOWN;
			rpmsg_ipcc_device_recovery(vrp);

			break;

		default:
			dev_err(dev, "wrong link status %d\n",
				vrp->link_status);
			break;
		}
	}

	return 0;
}

struct rpmsg_device *rpmsg_ipcc_request_channel(struct platform_device *client,
						int index)
{
	struct rpmsg_channel_info chinfo;
	struct rpmsg_ipcc_device *vrp;
	struct rpmsg_device *rpmsg_dev = NULL;
	struct of_phandle_args spec;
	const char *dev_name;
	u32 src, dst;
	int ret;

	mutex_lock(&con_mutex);

	if (of_parse_phandle_with_args(client->dev.of_node, "rpmsg-dev",
				       "#rpmsg-cells", index, &spec)) {
		dev_info(&client->dev,
			 "%s: can't parse \"rpmsg-dev\" property\n", __func__);
		mutex_unlock(&con_mutex);
		return NULL;
	}

	ret = device_property_read_string(&client->dev, "name", &dev_name);
	if (ret < 0)
		return NULL;

	list_for_each_entry(vrp, &ipcc_cons, node) {
		if (vrp->dev->of_node == spec.np) {
			if (vrp->link_status != RPMSG_IPCC_LINK_UP)
				break;

			src = spec.args[0];
			dst = spec.args[1];
			dev_info(vrp->dev,
				 "Creating device %s addr 0x%x->0x%x\n",
				 dev_name, src, dst);

			strncpy(chinfo.name, dev_name, sizeof(chinfo.name));
			chinfo.src = src;
			chinfo.dst = dst;
			rpmsg_dev = rpmsg_create_channel(vrp, &chinfo);
		}
	}

	of_node_put(spec.np);

	mutex_unlock(&con_mutex);
	return rpmsg_dev;
}
EXPORT_SYMBOL(rpmsg_ipcc_request_channel);

/* invoked when a echo request arrives */
static int rpmsg_ipcc_echo_cb(struct rpmsg_device *rpdev, void *data, int len,
			      void *priv, u32 src)
{
	struct rpmsg_ipcc_device *vrp = priv;
	struct device *dev = vrp->dev;
	struct dcf_message *dmsg;
	struct dcf_ccm_hdr *hdr;
	int err = 0;

	dev_dbg(dev, "%s in\n", __func__);
	if (len == 0) {
		dev_err(dev, "malformed echo msg (%d)\n", len);
		return -EINVAL;
	}

	if (rpdev) {
		dev_err(dev, "anomaly: echo ept has an rpdev handle\n");
		return -EINVAL;
	}

	dmsg = (struct dcf_message *)data;
	switch (dmsg->msg_type) {
	case COMM_MSG_CCM_ECHO:
		/* Add timestamp in the time[2] */
		hdr = (struct dcf_ccm_hdr *)dmsg;
		hdr->time[2] = semidrive_get_syscntr();
		err = __send_offchannel_raw(vrp, RPMSG_ECHO_ADDR, src, data,
					    len, true);
		if (err)
			dev_err(dev, "failed to echo service %d\n", err);

		break;
	case COMM_MSG_CCM_ACK:
		err = __send_offchannel_raw(vrp, RPMSG_ECHO_ADDR, src, "ACK", 4,
					    true);
		if (err)
			dev_err(dev, "failed to echo service %d\n", err);

		break;
	default:
		/* No more action, just drop the packet */
		break;
	}

	return 0;
}

static int rpmsg_ipcc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct rpmsg_ipcc_device *vrp;
	struct mbox_client *client;
	int ret = 0;
	int i;

	vrp = kzalloc(sizeof(struct rpmsg_ipcc_device), GFP_KERNEL);
	if (!vrp) {
		dev_err(&pdev->dev, "no memory to probe\n");
		return -ENOMEM;
	}

	if (of_property_read_u32(np, "rpmsg-mtu", &vrp->hw_mtu)) {
		vrp->hw_mtu = RPMSG_DEFAULT_HW_MTU;
	}

	if (of_property_read_u32(np, "tx-desc-num", &vrp->tx_desc_num)) {
		vrp->tx_desc_num = RPMSG_DEFAULT_TXD_NUM;
	}

	vrp->mtu = vrp->hw_mtu - sizeof(struct rpmsg_ipcc_hdr);
	vrp->tx_desc = kzalloc(sizeof(rpmsg_desc_t) * vrp->tx_desc_num +
				       vrp->hw_mtu * vrp->tx_desc_num,
			       GFP_KERNEL);
	if (IS_ERR(vrp->tx_desc)) {
		dev_err(&pdev->dev, "tx buf malloc failed: %ld\n",
			PTR_ERR(vrp->tx_desc));
		ret = -ENOMEM;
		goto fail_out;
	}
	vrp->tx_desc_avail = vrp->tx_desc_num;
	vrp->tx_buf_base = (void *)&vrp->tx_desc[vrp->tx_desc_num];
	for (i = 0; i < vrp->tx_desc_num; i++) {
		vrp->tx_desc[i].pbuf = vrp->tx_buf_base + vrp->hw_mtu * i;
	}
	dev_info(&pdev->dev, "txdesc=(%d %p %p)\n", vrp->tx_desc_num,
		 vrp->tx_desc, vrp->tx_buf_base);

	idr_init(&vrp->endpoints);
	mutex_init(&vrp->endpoints_lock);
	spin_lock_init(&vrp->txlock);
	init_waitqueue_head(&vrp->sendq);
	INIT_WORK(&vrp->rx_work, rpmsg_ipcc_recv_task);
	INIT_DELAYED_WORK(&vrp->ns_work, _init_ns_announce_delayed);

	vrp->rproc = -1; /* This is not used */
	vrp->link_status = RPMSG_IPCC_LINK_DOWN;

	skb_queue_head_init(&vrp->rxq);

	client = &vrp->client;
	/* Initialize the mbox unit used by rpmsg */
	client->dev = &pdev->dev;
	client->tx_done = __mbox_tx_done;
	client->rx_callback = __mbox_rx_cb;
	client->tx_block = false;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	vrp->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(vrp->mbox)) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "mbox_request_channel failed: %ld\n",
			PTR_ERR(vrp->mbox));
		goto fail_out;
	}
	vrp->dev = &pdev->dev;
	platform_set_drvdata(pdev, vrp);

	/* register the ipcc device into database */
	mutex_lock(&con_mutex);
	list_add_tail(&vrp->node, &ipcc_cons);
	mutex_unlock(&con_mutex);

	/* if supported by the remote processor, enable the name service */
	if (ipcc_has_feature(vrp, RPMSG_F_NS)) {
		/* a dedicated endpoint handles the name service msgs */
		vrp->ns_ept = __rpmsg_create_ept(vrp, NULL, rpmsg_ipcc_ns_cb,
						 vrp, RPMSG_NS_ADDR);
		if (!vrp->ns_ept) {
			dev_err(&pdev->dev, "failed to create the ns ept\n");
			ret = -ENOMEM;
		}
	}

	/* if supported by the remote processor, enable the echo service */
	if (ipcc_has_feature(vrp, RPMSG_F_ECHO)) {
		/* a dedicated endpoint handles the echo msgs */
		vrp->echo_ept = __rpmsg_create_ept(
			vrp, NULL, rpmsg_ipcc_echo_cb, vrp, RPMSG_ECHO_ADDR);
		if (!vrp->echo_ept) {
			dev_err(&pdev->dev, "failed to create the echo ept\n");
			ret = -ENOMEM;
		}
	}

	/* need to tell remote processor's name service about this channel ? */
	if (ipcc_has_feature(vrp, RPMSG_F_NS)) {
		schedule_delayed_work(&vrp->ns_work, 0);
	}
	dev_info(&pdev->dev, "%s mtu=(%d %d)\n", __func__, vrp->hw_mtu,
		 vrp->mtu);

	return ret;

fail_out:
	if (!IS_ERR_OR_NULL(vrp->mbox))
		mbox_free_channel(vrp->mbox);

	if (vrp->tx_desc)
		kfree(vrp->tx_desc);

	if (vrp)
		kfree(vrp);

	return ret;
}

static int rpmsg_ipcc_remove_device(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

/*
 * Shut down all clients by making sure that each edge stops processing
 * events and scanning for new channels, then call destroy on the devices.
 */
static int rpmsg_ipcc_remove(struct platform_device *pdev)
{
	int ret;
	struct rpmsg_ipcc_device *vrp = platform_get_drvdata(pdev);
	struct sk_buff *skb;

	if (!vrp)
		return -ENODEV;

	ret = device_for_each_child(vrp->dev, NULL, rpmsg_ipcc_remove_device);
	if (ret)
		dev_warn(vrp->dev, "can't remove rpmsg device: %d\n", ret);

	if (vrp->ns_ept)
		__rpmsg_destroy_ept(vrp, vrp->ns_ept);

	/* Discard all SKBs */
	while (!skb_queue_empty(&vrp->rxq)) {
		skb = skb_dequeue(&vrp->rxq);
		kfree_skb(skb);
	}

	mutex_lock(&con_mutex);

	list_del(&vrp->node);

	mutex_unlock(&con_mutex);

	if (vrp->mbox)
		mbox_free_channel(vrp->mbox);

	idr_destroy(&vrp->endpoints);

	kfree(vrp);

	return ret;
}

static const struct of_device_id rpmsg_ipcc_of_ids[] = {
	{ .compatible = "sd,rpmsg-ipcc" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rpmsg_ipcc_of_ids);

static struct platform_driver rpmsg_ipcc_driver = {
	.probe = rpmsg_ipcc_probe,
	.remove = rpmsg_ipcc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sd,rpmsg-ipcc",
		.of_match_table = rpmsg_ipcc_of_ids,
	},
};

static int __init rpmsg_ipcc_init(void)
{
	int ret;

	ret = platform_driver_register(&rpmsg_ipcc_driver);
	if (ret)
		pr_err("Unable to initialize rpmsg ipcc\n");
	else
		pr_info("Semidrive rpmsg ipcc is registered.\n");

	return ret;
}

arch_initcall(rpmsg_ipcc_init);

static void __exit rpmsg_ipcc_exit(void)
{
	platform_driver_unregister(&rpmsg_ipcc_driver);
}
module_exit(rpmsg_ipcc_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive rpmsg IPCC Driver");
MODULE_LICENSE("GPL v2");
