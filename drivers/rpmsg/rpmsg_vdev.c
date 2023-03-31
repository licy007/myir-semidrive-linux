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
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/mailbox_client.h>
#include <linux/sd_rpmsg.h>
#include <linux/soc/semidrive/mb_msg.h>

#define CONFIG_RPMSG_DUMP_HEX		(0)
#define MAX_MSG_NUM 10	/* enlarge it if overflow happen */
#define MAX_VDEV_NUMS	4

/*
 * For now, allocate 256 buffers of 512 bytes for each side. each buffer
 * will then have 16B for the msg header and 496B for the payload.
 * This will require a total space of 256KB for the buffers themselves, and
 * 3 pages for every vring (the size of the vring depends on the number of
 * buffers it supports).
 */
#define RPMSG_NUM_BUFS		(512)
#define RPMSG_BUF_SIZE		(512)
#define RPMSG_BUFS_SPACE	(RPMSG_NUM_BUFS * RPMSG_BUF_SIZE)

/*
 * The alignment between the consumer and producer parts of the vring.
 * Note: this is part of the "wire" protocol. If you change this, you need
 * to update your BIOS image as well
 */
#define RPMSG_VRING_ALIGN	(4096)

/* With 256 buffers, our vring will occupy 3 pages */
#define RPMSG_RING_SIZE	((DIV_ROUND_UP(vring_size(RPMSG_NUM_BUFS / 2, \
				RPMSG_VRING_ALIGN), PAGE_SIZE)) * PAGE_SIZE)


/* contains pool of descriptors and two circular buffers */
#ifndef VRING_SIZE
#define VRING_SIZE (0x8000)
#endif

struct sd_virtdev {
	struct virtio_device vdev;
	unsigned int vring[2];
	struct virtqueue *vq[2];
	int base_vq_id;
	int num_of_vqs;
	struct notifier_block nb;
};

struct sd_notify_msg {
	sd_msghdr_t msghead;
	__u8 device_type;
	__u8 cc;
	__u8 vq_id;
	__u8 flags;
} __attribute__((packed));

struct sd_rpmsg_virt_device {
	struct device *dev;
	enum sd_rpmsg_rprocs	variant;
	struct delayed_work delay_work;
	spinlock_t rpmsg_mb_lock;
	struct blocking_notifier_head notifier;

	__u64 rproc_message[MAX_MSG_NUM];
	__u32 in_idx, out_idx;
	__u8 counter;	/* tx continuity counter */

	int rproc_id;
	struct mbox_chan *mbox;
	struct mbox_client client;
	struct mutex lock;
	int vdev_nums;
	struct sd_virtdev ivdev[MAX_VDEV_NUMS];
};

#define to_sd_virtdev(vd) container_of(vd, struct sd_virtdev, vdev)
#define to_sd_rvdev(vd, id) container_of(vd, struct sd_rpmsg_virt_device, ivdev[id])

struct sd_rpmsg_vq_info {
	__u16 num;	/* number of entries in the virtio_ring */
	__u16 vq_id;	/* a globaly unique index of this virtqueue */
	void *addr;	/* address where we mapped the virtio ring */
	struct sd_rpmsg_virt_device *rvdev;
};

static u64 sd_rpmsg_get_features(struct virtio_device *vdev)
{
	/* VIRTIO_RPMSG_F_NS has been made private */
	/* VIRTIO_RPMSG_F_ECHO */
	return (1 << 0) | (1 << 1);
}

static int sd_rpmsg_finalize_features(struct virtio_device *vdev)
{
	/* Give virtio_ring a chance to accept features */
	vring_transport_features(vdev);
	return 0;
}

/* kick the remote processor, and let it know which virtqueue to poke at */
static bool sd_rpmsg_notify(struct virtqueue *vq)
{
	struct sd_rpmsg_vq_info *rpvq = vq->priv;
	struct sd_notify_msg msg;
	struct sd_rpmsg_virt_device *rvdev = rpvq->rvdev;
	int ret;

	mutex_lock(&rvdev->lock);

	MB_MSG_INIT_RPMSG_HEAD(&msg.msghead, rvdev->rproc_id,
			sizeof(struct sd_notify_msg), IPCC_ADDR_RPMSG);
	msg.vq_id = rpvq->vq_id;
	msg.cc = rvdev->counter++;
	msg.device_type = 0x90;

	/* send the index of the triggered virtqueue as the mbox payload */
	ret = mbox_send_message(rvdev->mbox, &msg);
	mutex_unlock(&rvdev->lock);

	/* in case of TIMEOUT because remote proc may be off */
	if (ret < 0) {
		dev_info(rvdev->dev, "rproc %d may be off\n", rvdev->rproc_id);
		return false;
	}
	return true;
}

static int sd_rpmsg_callback(struct notifier_block *this,
					unsigned long index, void *data)
{
	struct sd_virtdev *svdev;
	__u64 msg_data = (__u64) data;
	struct sd_notify_msg *msg = (struct sd_notify_msg*) &msg_data;
	__u16 vq_id;

#if CONFIG_RPMSG_DUMP_HEX
 	print_hex_dump_bytes("rpmsg : cb: ", DUMP_PREFIX_ADDRESS,
 				msg, 8);
#endif
	svdev = container_of(this, struct sd_virtdev, nb);
	/* ignore vq indices which are clearly not for us */
	vq_id = msg->vq_id;
	if (vq_id < svdev->base_vq_id || vq_id > svdev->base_vq_id + 1) {
		pr_err("vq_id: 0x%x is invalid\n", vq_id);
		return NOTIFY_DONE;
	}

	vq_id -= svdev->base_vq_id;

	pr_debug("%s vqid: %d rproc: %d nvqs: %d\n", __func__, msg->vq_id,
				msg->msghead.rproc, svdev->num_of_vqs);

	/*
	 * Currently both PENDING_MSG and explicit-virtqueue-index
	 * messaging are supported.
	 * Whatever approach is taken, at this point 'mu_msg' contains
	 * the index of the vring which was just triggered.
	 */
	if (vq_id < svdev->num_of_vqs) {
		vring_interrupt(vq_id, svdev->vq[vq_id]);
	}
	return NOTIFY_DONE;
}

static struct virtqueue *rp_find_vq(struct virtio_device *vdev,
				    unsigned int index,
				    void (*callback)(struct virtqueue *vq),
				    const char *name, bool ctx)
{
	struct sd_virtdev *svdev = to_sd_virtdev(vdev);
	struct sd_rpmsg_virt_device *rvdev = to_sd_rvdev(svdev,
						     svdev->base_vq_id / 2);
	struct sd_rpmsg_vq_info *rpvq;
	struct virtqueue *vq;
	struct device *dev = &vdev->dev;
	int err;

	rpvq = kzalloc(sizeof(*rpvq), GFP_KERNEL);
	if (!rpvq)
		return ERR_PTR(-ENOMEM);

	/* ioremap'ing normal memory, so we cast away sparse's complaints */
	rpvq->addr = (__force void *) ioremap_nocache(svdev->vring[index],
							RPMSG_RING_SIZE);
	if (!rpvq->addr) {
		err = -ENOMEM;
		goto free_rpvq;
	}

	memset_io(rpvq->addr, 0, RPMSG_RING_SIZE);

	dev_dbg(dev, "vring%d: phys 0x%x, virt 0x%p\n", index, svdev->vring[index],
					rpvq->addr);

	vq = vring_new_virtqueue(index, RPMSG_NUM_BUFS / 2, RPMSG_VRING_ALIGN,
			vdev, true, ctx, rpvq->addr, sd_rpmsg_notify, callback,
			name);
	if (!vq) {
		dev_err(dev, "vring_new_virtqueue failed\n");
		err = -ENOMEM;
		goto unmap_vring;
	}

	svdev->vq[index] = vq;
	vq->priv = rpvq;
	/* system-wide unique id for this virtqueue */
	rpvq->vq_id = svdev->base_vq_id + index;
	rpvq->rvdev = rvdev;
	mutex_init(&rvdev->lock);

	return vq;

unmap_vring:
	/* iounmap normal memory, so make sparse happy */
	iounmap((__force void __iomem *) rpvq->addr);
free_rpvq:
	kfree(rpvq);
	return ERR_PTR(err);
}

static void sd_rpmsg_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;
	struct sd_virtdev *svdev = to_sd_virtdev(vdev);
	struct sd_rpmsg_virt_device *rvdev = to_sd_rvdev(svdev,
						     svdev->base_vq_id / 2);

	list_for_each_entry_safe(vq, n, &vdev->vqs, list) {
		struct sd_rpmsg_vq_info *rpvq = vq->priv;

		iounmap(rpvq->addr);
		vring_del_virtqueue(vq);
		kfree(rpvq);
	}

	if (&svdev->nb)
		blocking_notifier_chain_unregister(&rvdev->notifier, &svdev->nb);
}

static int sd_rpmsg_find_vqs(struct virtio_device *vdev, unsigned int nvqs,
				struct virtqueue *vqs[],
				vq_callback_t *callbacks[],
				const char * const names[],
				const bool * ctx,
				struct irq_affinity *desc)
{
	struct sd_virtdev *svdev = to_sd_virtdev(vdev);
	struct sd_rpmsg_virt_device *rvdev = to_sd_rvdev(svdev,
						     svdev->base_vq_id / 2);
	int i, err;

	/* we maintain two virtqueues per remote processor (for RX and TX) */
	if (nvqs != 2)
		return -EINVAL;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = rp_find_vq(vdev, i, callbacks[i], names[i], ctx ? ctx[i] : false);
		if (IS_ERR(vqs[i])) {
			err = PTR_ERR(vqs[i]);
			goto error;
		}
	}

	svdev->num_of_vqs = nvqs;

	svdev->nb.notifier_call = sd_rpmsg_callback;
	blocking_notifier_chain_register(&rvdev->notifier, &svdev->nb);

	return 0;

error:
	sd_rpmsg_del_vqs(vdev);
	return err;
}

static void sd_rpmsg_reset(struct virtio_device *vdev)
{
	dev_dbg(&vdev->dev, "reset !\n");
}

static u8 sd_rpmsg_get_status(struct virtio_device *vdev)
{
	return 0;
}

static void sd_rpmsg_set_status(struct virtio_device *vdev, u8 status)
{
	dev_dbg(&vdev->dev, "%s new status: %d\n", __func__, status);
}

static void sd_rpmsg_virt_device_release(struct device *dev)
{
	/* this handler is provided so driver core doesn't yell at us */
}

static struct virtio_config_ops sd_rpmsg_config_ops = {
	.get_features	= sd_rpmsg_get_features,
	.finalize_features = sd_rpmsg_finalize_features,
	.find_vqs	= sd_rpmsg_find_vqs,
	.del_vqs	= sd_rpmsg_del_vqs,
	.reset		= sd_rpmsg_reset,
	.set_status	= sd_rpmsg_set_status,
	.get_status	= sd_rpmsg_get_status,
};

static int set_vring_phy_buf(struct platform_device *pdev,
		       struct sd_rpmsg_virt_device *rvdev, int index)
{
	struct resource *res;
	resource_size_t size;
	unsigned int start, end;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		size = resource_size(res);
		start = res->start;
		end = res->start + size;
		rvdev->ivdev[0].vring[0] = start; /* rx vring first */
		rvdev->ivdev[0].vring[1] = start + VRING_SIZE; /* tx vring */
	} else {
		dev_err(dev, "no rpmsg vring resource!\n");
		return -ENOMEM;
	}

	return 0;
}

static void rpmsg_rx_work_handler(struct work_struct *work)
{
	struct sd_rpmsg_virt_device *rvdev = container_of(to_delayed_work(work),
					 struct sd_rpmsg_virt_device, delay_work);
	__u64 message;
	unsigned long flags;

	spin_lock_irqsave(&rvdev->rpmsg_mb_lock, flags);
	/* handle all incoming mbox message */
	while (rvdev->in_idx != rvdev->out_idx) {
		message = rvdev->rproc_message[rvdev->out_idx % MAX_MSG_NUM];
		spin_unlock_irqrestore(&rvdev->rpmsg_mb_lock, flags);

		blocking_notifier_call_chain(&(rvdev->notifier), 8, (void *)message);

		spin_lock_irqsave(&rvdev->rpmsg_mb_lock, flags);
		rvdev->rproc_message[rvdev->out_idx % MAX_MSG_NUM] = 0;
		rvdev->out_idx++;
	}
	spin_unlock_irqrestore(&rvdev->rpmsg_mb_lock, flags);
}

static void sd_rpmsg_mbox_callback(struct mbox_client *client, void *mssg)
{
	struct sd_rpmsg_virt_device *rvdev = container_of(client,
					 struct sd_rpmsg_virt_device, client);
	struct device *dev = client->dev;
	__u64 message;
	unsigned long flags;

	dev_dbg(dev, "rpmsg cb msg from: %d\n", rvdev->rproc_id);

	memcpy(&message, mssg, 8);
	spin_lock_irqsave(&rvdev->rpmsg_mb_lock, flags);
	/* get message from receive buffer */
	rvdev->rproc_message[rvdev->in_idx % MAX_MSG_NUM] = message;
	rvdev->in_idx++;
	/*
	 * Too many mu message not be handled in timely, can enlarge
	 * MAX_MSG_NUM
	 */
	if (rvdev->in_idx == rvdev->out_idx) {
		spin_unlock_irqrestore(&rvdev->rpmsg_mb_lock, flags);
		dev_err(dev, "mbox msg overflow!\n");
		return;
	}
	spin_unlock_irqrestore(&rvdev->rpmsg_mb_lock, flags);

	queue_delayed_work(system_unbound_wq, &rvdev->delay_work, 0);
	//schedule_delayed_work(&rvdev->delay_work, 0);

	return;
}

static int sd_rpmsg_virt_probe(struct platform_device *pdev)
{
	int j, ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct sd_rpmsg_virt_device *rvdev;
	struct mbox_client *client;

	rvdev = kzalloc(sizeof(struct sd_rpmsg_virt_device), GFP_KERNEL);
	if (!rvdev)
		return -ENOMEM;

	rvdev->dev = dev;
	rvdev->variant = (enum sd_rpmsg_rprocs)of_device_get_match_data(dev);
	rvdev->rproc_id = rvdev->variant;
	INIT_DELAYED_WORK(&rvdev->delay_work, rpmsg_rx_work_handler);
	spin_lock_init(&rvdev->rpmsg_mb_lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&(rvdev->notifier));

	dev_info(dev, "Semidrive rpmsg is initializing!\n");

	client = &rvdev->client;
	/* Initialize the mbox unit used by rpmsg */
	client->dev = dev;
	client->tx_done = NULL;
	client->rx_callback = sd_rpmsg_mbox_callback;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;

	rvdev->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(rvdev->mbox)) {
		ret = -EBUSY;
		dev_err(dev, "failed to request mbox channel\n");
		return ret;
	}

	ret = set_vring_phy_buf(pdev, rvdev, 0);
	if (ret) {
		dev_err(dev, "No vring buffer.\n");
		return -ENOMEM;
	}

	if (of_reserved_mem_device_init_by_idx(dev, np, 0)) {
		dev_err(dev, "No specific DMA pool.\n");
		ret = -ENOMEM;
		return ret;
	}

	rvdev->vdev_nums = 1;
	for (j = 0; j < rvdev->vdev_nums; j++) {
		dev_err(dev, "rvdev%d: vring0 0x%x, vring1 0x%x\n",
			 rvdev->rproc_id,
			 rvdev->ivdev[j].vring[0],
			 rvdev->ivdev[j].vring[1]);
		rvdev->ivdev[j].vdev.id.device = VIRTIO_ID_RPMSG;
		rvdev->ivdev[j].vdev.config = &sd_rpmsg_config_ops;
		rvdev->ivdev[j].vdev.dev.parent = &pdev->dev;
		rvdev->ivdev[j].vdev.dev.release = sd_rpmsg_virt_device_release;
		rvdev->ivdev[j].base_vq_id = j * 2;

		ret = register_virtio_device(&rvdev->ivdev[j].vdev);
		if (ret) {
			dev_err(dev, "failed to register rvdev: %d\n", ret);
			return ret;
		}
	}

	platform_set_drvdata(pdev, rvdev);

	return ret;
}

static const struct of_device_id sd_rpmsg_dt_ids[] = {
	{ .compatible = "sd,rpmsg-vq,rp_saf",  .data = (void *)SD_RPROC_SAF, },
	{ .compatible = "sd,rpmsg-vq,rp_sec",  .data = (void *)SD_RPROC_SEC, },
	{ .compatible = "sd,rpmsg-vq,rp_mp",   .data = (void *)SD_RPROC_MPC, },
	{ .compatible = "sd,rpmsg-vq,rp_ap",   .data = (void *)SD_RPROC_AP2, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sd_rpmsg_dt_ids);

#ifdef CONFIG_PM_SLEEP
static int sd_rpmsg_virtio_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sd_rpmsg_virt_device *rvdev = platform_get_drvdata(pdev);
	int i;
	struct virtio_device *vdev;
	int ret = 0;

	for (i = 0; i < rvdev->vdev_nums; i++) {
		vdev = &rvdev->ivdev[i].vdev;
		ret |= virtio_device_freeze(vdev);
	}

	return ret;
}

static int sd_rpmsg_virtio_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sd_rpmsg_virt_device *rvdev = platform_get_drvdata(pdev);
	int i;
	struct virtio_device *vdev;
	int ret = 0;

	for (i = 0; i < rvdev->vdev_nums; i++) {
		vdev = &rvdev->ivdev[i].vdev;
		ret |= virtio_device_restore(vdev);
	}

	return ret;
}

static struct dev_pm_ops sd_rpmsg_virtio_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sd_rpmsg_virtio_freeze, sd_rpmsg_virtio_restore)
};
#endif

static struct platform_driver sd_rpmsg_virt_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sd-rpmsg",
		   .of_match_table = sd_rpmsg_dt_ids,
		#ifdef CONFIG_PM_SLEEP
			.pm = &sd_rpmsg_virtio_pm_ops,
		#endif
		   },
	.probe = sd_rpmsg_virt_probe,
};

static int __init sd_rpmsg_init(void)
{
	int ret;

	ret = platform_driver_register(&sd_rpmsg_virt_driver);
	if (ret)
		pr_err("Unable to initialize rpmsg driver\n");
	else
		pr_info("Semidrive rpmsg virtio is registered.\n");

	return ret;
}

arch_initcall(sd_rpmsg_init);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive rpmsg virtio device");
MODULE_LICENSE("GPL v2");
