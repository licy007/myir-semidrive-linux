/*
 * Copyright (c) 2020, Semidriver Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/rpmsg.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define RPMSG_HEADER_LEN 		16
#define MAX_RPMSG_BUFF_SIZE		(512 - RPMSG_HEADER_LEN)

#define SF                              0xF0                                       /* Single frame */
#define MF(nr)                          ((nr) & 0x0F)                              /* Multiple frame, nr: frame number */
#define EF(nr)                          (((nr) & 0x0F) | 0x10)                     /* Ending frame */
#define IS_SF(payload)                  ((payload)->flow_ctrl == 0xF0)             /* Is single frame? */
#define GET_PDU_SN(payload)             (((payload)->flow_ctrl) & 0x0F)
#define IS_EF(payload)                  ((((payload)->flow_ctrl) & 0xF0) == 0x10)  /* Is ending frame? */
#define FC_SIZE                         1
#define REMAIN_PACKET_SIZE(size, pos)   ((size) - (pos) + FC_SIZE)

#define ETH1_RPMSG_EPT			1024
#define ETH2_RPMSG_EPT			1025
#define DOIP_RPMSG_EPT			1026
#define ETH3_RPMSG_EPT			1027
#define ETH4_RPMSG_EPT			1028

struct sdpe_rpc_pdu_head {
	unsigned char flow_ctrl;
	unsigned char data[0];
} __attribute__((packed));

struct sdpe_rpc_rx_sdu {
	int sdu_len;
	int pdu_sn;
	struct sk_buff_head pdu_queue;
};

struct sdpe_rpc_device {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
	fmode_t fmode;
	int myaddr;

	int minor;
	struct device *dev;
	struct rpmsg_device *rpdev;
	struct mutex ept_lock;

	spinlock_t rx_sdu_queue_lock;
	struct sk_buff_head rx_sdu_queue;
	struct sk_buff *current_rx_sdu;
	wait_queue_head_t readq;
};

static dev_t sdpe_rpc_major;
static struct class *sdpe_rpc_class;
static int eth_pkt_num_quota = 1000;

static int sdpe_rpc_open(struct inode *inode, struct file *filp);
static int sdpe_rpc_release(struct inode *inode, struct file *filp);
static ssize_t sdpe_rpc_read(struct file *filp, char __user *buf,
		size_t len, loff_t *f_pos);
static ssize_t sdpe_rpc_write(struct file *filp, const char __user *buf,
		size_t len, loff_t *f_pos);
static int sdpe_rpc_rpmsg_rx_cb(struct rpmsg_device *rpdev, void *buf, int len,
		void *priv, u32 addr);

static int sdpe_rpc_chrdev_open(struct inode *inode, struct file *filp);
static void sdpe_rpc_rpmsg_remove(struct rpmsg_device *rpdev);
static int sdpe_rpc_rpmsg_probe(struct rpmsg_device *rpdev);
static char *sdpe_rpc_devnode(struct device *dev, umode_t *mode);

static const struct file_operations sdpe_rpc_fops = {
	.owner   = THIS_MODULE,
	.read    = sdpe_rpc_read,
	.write   = sdpe_rpc_write,
	.open    = sdpe_rpc_open,
	.release = sdpe_rpc_release,
	.llseek	 = no_llseek,
};

static struct sdpe_rpc_device sdpe_rpc_devlist[] = {
	[1] = { "sdpe-eth0",  0666, &sdpe_rpc_fops, FMODE_UNSIGNED_OFFSET, ETH1_RPMSG_EPT},
	[2] = { "sdpe-eth1",  0666, &sdpe_rpc_fops, FMODE_UNSIGNED_OFFSET, ETH2_RPMSG_EPT},
	[3] = { "sdpe-doip",  0666, &sdpe_rpc_fops, FMODE_UNSIGNED_OFFSET, DOIP_RPMSG_EPT},
	[4] = { "sdpe-eth2",  0666, &sdpe_rpc_fops, FMODE_UNSIGNED_OFFSET, ETH3_RPMSG_EPT},
	[5] = { "sdpe-eth3",  0666, &sdpe_rpc_fops, FMODE_UNSIGNED_OFFSET, ETH4_RPMSG_EPT},
};

static struct rpmsg_device_id sdpe_rpc_rpmsg_id_table[] = {
	{.name = "sdpe-eth0"},
	{.name = "sdpe-eth1"},
	{.name = "sdpe-doip"},
	{.name = "sdpe-eth2"},
	{.name = "sdpe-eth3"},
	{},
};

static const struct file_operations sdpe_rpc_chrdev_fops = {
	.open = sdpe_rpc_chrdev_open,
	.llseek = noop_llseek,
};

static struct sk_buff *sdpe_rpc_alloc_rx_sdu(void)
{
	struct sk_buff *sdu_skb;
	struct sdpe_rpc_rx_sdu *sdu;

	sdu_skb = alloc_skb(sizeof(struct sdpe_rpc_rx_sdu),
			GFP_ATOMIC);
	if (!sdu_skb)
		return NULL;

	sdu = (struct sdpe_rpc_rx_sdu *)sdu_skb->data;
	sdu->sdu_len = 0;
	sdu->pdu_sn = -1;
	skb_queue_head_init(&sdu->pdu_queue);

	return sdu_skb;
}

static void sdpe_rpc_free_rx_sdu(struct sk_buff *sdu_skb)
{
	struct sdpe_rpc_rx_sdu *sdu;

	sdu = (struct sdpe_rpc_rx_sdu *)sdu_skb->data;
	skb_queue_purge(&sdu->pdu_queue);
	kfree_skb(sdu_skb);
}

static ssize_t sdpe_rpc_read(struct file *filp, char __user *buf,
		size_t len, loff_t *f_pos)
{
	struct sdpe_rpc_device *rpc_dev = filp->private_data;
	struct rpmsg_device *rpdev = rpc_dev->rpdev;
	struct sdpe_rpc_rx_sdu *sdu;
	unsigned long flags;
	struct sk_buff *skb, *pdu_skb;
	int use = 0;

	if (!rpdev->ept)
		return -EPIPE;

	spin_lock_irqsave(&rpc_dev->rx_sdu_queue_lock, flags);

	/* Wait for data in the queue */
	if (skb_queue_empty(&rpc_dev->rx_sdu_queue)) {
		spin_unlock_irqrestore(&rpc_dev->rx_sdu_queue_lock, flags);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* Wait until we get data or the endpoint goes away */
		if (wait_event_interruptible(rpc_dev->readq,
					!skb_queue_empty(&rpc_dev->rx_sdu_queue) ||
					!rpdev->ept))
			return -ERESTARTSYS;

		/* We lost the endpoint while waiting */
		if (!rpdev->ept)
			return -EPIPE;

		spin_lock_irqsave(&rpc_dev->rx_sdu_queue_lock, flags);
	}

	skb = skb_dequeue(&rpc_dev->rx_sdu_queue);
	spin_unlock_irqrestore(&rpc_dev->rx_sdu_queue_lock, flags);
	if (!skb)
		return -EFAULT;

	sdu = (struct sdpe_rpc_rx_sdu *)skb->data;

	if (sdu->sdu_len > len) {
		use = -EINVAL;
		goto free_rx_sdu;
	}

	while ((pdu_skb = skb_dequeue(&sdu->pdu_queue)) != NULL) {
		copy_to_user(buf + use, pdu_skb->data, pdu_skb->len);
		use += pdu_skb->len;
		kfree_skb(pdu_skb);
	}

free_rx_sdu:

	sdpe_rpc_free_rx_sdu(skb);

	return use;
}


static ssize_t sdpe_rpc_write(struct file *filp, const char __user *buf,
		size_t len, loff_t *f_pos)
{
	struct sdpe_rpc_device *rpc_dev = filp->private_data;
	struct rpmsg_device *rpdev = rpc_dev->rpdev;
	struct sdpe_rpc_pdu_head *pdu;
	int pdu_sn = 0;
	int offset = 0;
	int send_size;
	int ret;

	pdu = kvmalloc(MAX_RPMSG_BUFF_SIZE, GFP_USER);
	if (IS_ERR(pdu))
		return PTR_ERR(pdu);

	if (mutex_lock_interruptible(&rpc_dev->ept_lock)) {
		ret = -ERESTARTSYS;
		goto free_kbuf;
	}

	if (!rpdev->ept) {
		ret = -EPIPE;
		goto unlock_eptdev;
	}

	while (offset < len) {
		if (REMAIN_PACKET_SIZE(len, offset) > MAX_RPMSG_BUFF_SIZE) {
			pdu->flow_ctrl = MF(pdu_sn++);
			send_size = MAX_RPMSG_BUFF_SIZE;
		} else {
			if (pdu_sn == 0) {
				pdu->flow_ctrl = SF;
			} else {
				pdu->flow_ctrl = EF(pdu_sn);
			}
			send_size = REMAIN_PACKET_SIZE(len, offset);
		}

		copy_from_user(pdu->data, buf + offset, send_size - FC_SIZE);
		offset += send_size - FC_SIZE;

		if (filp->f_flags & O_NONBLOCK)
			ret = rpmsg_trysend(rpdev->ept, pdu, send_size);
		else
			ret = rpmsg_send(rpdev->ept, pdu, send_size);

		if (ret < 0) {
			break;
		}
	}

unlock_eptdev:
	mutex_unlock(&rpc_dev->ept_lock);

free_kbuf:
	kvfree(pdu);
	return ret < 0 ? ret : len;
}

static int sdpe_rpc_rpmsg_rx_cb(struct rpmsg_device *rpdev, void *buf, int len,
		void *priv, u32 addr)
{
	struct sdpe_rpc_device *rpc_dev = priv;
	struct sk_buff *sdu_skb, *pdu_skb;
	struct sdpe_rpc_rx_sdu *sdu;
	struct sdpe_rpc_pdu_head *pdu_head;

	sdu_skb = rpc_dev->current_rx_sdu;
	if (!sdu_skb) {
		if (skb_queue_len(&rpc_dev->rx_sdu_queue) < eth_pkt_num_quota)
			sdu_skb = sdpe_rpc_alloc_rx_sdu();
		if (!sdu_skb)
			return -ENOMEM;

		rpc_dev->current_rx_sdu = sdu_skb;
	}
	sdu = (struct sdpe_rpc_rx_sdu *)sdu_skb->data;

	pdu_head = (struct sdpe_rpc_pdu_head *)buf;
	if (GET_PDU_SN(pdu_head) != (sdu->pdu_sn + 1)) {
		sdpe_rpc_free_rx_sdu(sdu_skb);
		rpc_dev->current_rx_sdu = NULL;
		return -EINVAL;
	}

	pdu_skb = alloc_skb(len, GFP_ATOMIC);
	if (!pdu_skb) {
		sdpe_rpc_free_rx_sdu(sdu_skb);
		rpc_dev->current_rx_sdu = NULL;
		return -ENOMEM;
	}
	skb_put_data(pdu_skb, pdu_head->data, len - FC_SIZE);

	skb_queue_tail(&sdu->pdu_queue, pdu_skb);
	sdu->sdu_len += (len - FC_SIZE);
	sdu->pdu_sn = GET_PDU_SN(pdu_head);

	if (IS_SF(pdu_head) || IS_EF(pdu_head)) {
		spin_lock(&rpc_dev->rx_sdu_queue_lock);
		skb_queue_tail(&rpc_dev->rx_sdu_queue, sdu_skb);
		spin_unlock(&rpc_dev->rx_sdu_queue_lock);

		rpc_dev->current_rx_sdu = NULL;
		/* wake up any blocking processes, waiting for new data */
		wake_up_interruptible(&rpc_dev->readq);
	}

	return 0;
}

static int sdpe_rpc_open(struct inode *inode, struct file *filp)
{
	struct sdpe_rpc_device *rpc_dev = filp->private_data;
	struct rpmsg_device *rpdev = rpc_dev->rpdev;
	struct device *dev = rpc_dev->dev;
	struct rpmsg_channel_info chinfo;

	get_device(dev);

	mutex_init(&rpc_dev->ept_lock);
	spin_lock_init(&rpc_dev->rx_sdu_queue_lock);
	skb_queue_head_init(&rpc_dev->rx_sdu_queue);
	rpc_dev->current_rx_sdu = NULL;

	init_waitqueue_head(&rpc_dev->readq);

	if (rpdev->ept) {
		dev_err(dev, "Already exist\n");
		return -EEXIST;
	}

	strncpy(chinfo.name, rpc_dev->name, RPMSG_NAME_SIZE);
	chinfo.src = rpc_dev->myaddr;
	chinfo.dst = RPMSG_ADDR_ANY;

	rpdev->ept = rpmsg_create_ept(rpdev, sdpe_rpc_rpmsg_rx_cb, rpc_dev, chinfo);
	if (!rpdev->ept) {
		dev_err(dev, "failed to create endpoint\n");
		put_device(dev);
		return -ENOMEM;
	}
	rpdev->src = rpdev->ept->addr;

	dev_info(dev, "open %s %d->%d \n", chinfo.name, chinfo.src, chinfo.dst);

	return 0;
}

static int sdpe_rpc_release(struct inode *inode, struct file *filp)
{
	struct sdpe_rpc_device *rpc_dev = filp->private_data;
	struct rpmsg_device *rpdev = rpc_dev->rpdev;
	struct device *dev = rpc_dev->dev;
	struct sk_buff *skb;

	/* Close the endpoint, if it's not already destroyed by the parent */
	mutex_lock(&rpc_dev->ept_lock);
	if (rpdev->ept) {
		rpmsg_destroy_ept(rpdev->ept);
		rpdev->ept = NULL;
	}
	mutex_unlock(&rpc_dev->ept_lock);

	/* Discard all SKBs */
	while (!skb_queue_empty(&rpc_dev->rx_sdu_queue)) {
		skb = skb_dequeue(&rpc_dev->rx_sdu_queue);
		sdpe_rpc_free_rx_sdu(skb);
	}

	if (rpc_dev->current_rx_sdu) {
		sdpe_rpc_free_rx_sdu(rpc_dev->current_rx_sdu);
	}

	put_device(dev);

	dev_info(dev, "released!\n");

	return 0;
}

static int sdpe_rpc_chrdev_open(struct inode *inode, struct file *filp)
{
	int minor;
	const struct sdpe_rpc_device *rpc_dev;

	minor = iminor(inode);
	if (minor >= ARRAY_SIZE(sdpe_rpc_devlist))
		return -ENXIO;

	rpc_dev = &sdpe_rpc_devlist[minor];
	if (!rpc_dev->fops)
		return -ENXIO;

	filp->f_mode |= rpc_dev->fmode;
	filp->private_data = (void*)rpc_dev;
	replace_fops(filp, rpc_dev->fops);

	if (rpc_dev->fops->open)
		return rpc_dev->fops->open(inode, filp);

	return 0;
}

static char *sdpe_rpc_devnode(struct device *dev, umode_t *mode)
{
	if (mode && sdpe_rpc_devlist[MINOR(dev->devt)].mode)
		*mode = sdpe_rpc_devlist[MINOR(dev->devt)].mode;
	return NULL;
}

static int sdpe_rpc_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct sdpe_rpc_device *rpc_dev = NULL;
	int minor;
	char *pstr;

	for (minor = 1; minor < ARRAY_SIZE(sdpe_rpc_devlist); minor++) {
		pstr = strnstr(rpdev->id.name, sdpe_rpc_devlist[minor].name, RPMSG_NAME_SIZE);
		if (pstr) {
			rpc_dev = &sdpe_rpc_devlist[minor];
			rpc_dev->rpdev = rpdev;
			rpc_dev->minor = minor;
			rpc_dev->dev = device_create(sdpe_rpc_class, &rpdev->dev, MKDEV(MAJOR(sdpe_rpc_major), minor),
					rpc_dev, sdpe_rpc_devlist[minor].name);

			dev_set_drvdata(&rpdev->dev, rpc_dev);
			dev_info(rpc_dev->dev, "sdpe rpc device created!\n");
			return 0;
		}
	}

	return -ENODEV;
}

static void sdpe_rpc_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct sdpe_rpc_device *rpc_dev = dev_get_drvdata(&rpdev->dev);

	if (rpc_dev->dev)
		device_destroy(sdpe_rpc_class, rpc_dev->dev->devt);

	dev_set_drvdata(&rpdev->dev, NULL);
	rpc_dev->rpdev = NULL;
}

static struct rpmsg_driver sdpe_rpc_rpmsg_driver = {
	.probe = sdpe_rpc_rpmsg_probe,
	.remove = sdpe_rpc_rpmsg_remove,
	.id_table = sdpe_rpc_rpmsg_id_table,
	.drv = {
		.name = "sd,sdpe_rpc_rpmsg"
	},
};

static int __init sdpe_rpc_init(void)
{
	int major;
	int ret;

	major = register_chrdev(0, "sdpe_rpc", &sdpe_rpc_chrdev_fops);
	if (major < 0) {
		pr_err("sdpe_rpc: failed to register char dev region\n");
		goto err_out;
	}

	sdpe_rpc_major = MKDEV(major, 0);
	sdpe_rpc_class = class_create(THIS_MODULE, "sdpe_rpc");
	if (IS_ERR(sdpe_rpc_class)) {
		pr_err("sdpe_rpc: failed to create class\n");
		goto err_out_with_chr_dev;
	}

	sdpe_rpc_class->devnode = sdpe_rpc_devnode;

	ret = register_rpmsg_driver(&sdpe_rpc_rpmsg_driver);
	if (ret < 0) {
		pr_err("sdpe_rpc: failed to register rpmsg driver\n");
		goto err_out_with_class;
	}

	return 0;

err_out_with_class:
	class_destroy(sdpe_rpc_class);

err_out_with_chr_dev:

	unregister_chrdev(sdpe_rpc_major, "sdpe_rpc");

err_out:
	return -1;
}

static void __exit sdpe_rpc_exit(void)
{
	unregister_rpmsg_driver(&sdpe_rpc_rpmsg_driver);
	class_destroy(sdpe_rpc_class);
	unregister_chrdev(sdpe_rpc_major, "sdpe_rpc");
}

module_init(sdpe_rpc_init);
module_exit(sdpe_rpc_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive sdpe rpc driver");
MODULE_LICENSE("GPL v2");

static int __init sdpe_setup(char *str)
{
	eth_pkt_num_quota = simple_strtoul(str, 0, NULL);
	return 0;
}
early_param("sdpe_quota", sdpe_setup);

