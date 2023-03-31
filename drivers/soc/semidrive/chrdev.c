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

#include <linux/soc/semidrive/ipcc.h>

#include "uapi/dcf-ioctl.h"

union dcf_ioctl_arg {
	struct dcf_usercontext	context;
	struct dcf_ioc_setproperty setprop;
};

struct dcf_device {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
	fmode_t fmode;
	int myaddr;

	int minor;
	struct device *dev;
	struct rpmsg_device *rpdev;
	struct mutex ept_lock;
	struct list_head users;
	atomic_t user_count;
};

struct dcf_user {
	struct list_head list_node;
	struct dcf_device *eptdev;
	u8 user_id;
	u8 user_id_other;
	spinlock_t queue_lock;
	struct sk_buff_head queue;
	wait_queue_head_t readq;
	int pid;

};

static int validate_ioctl_arg(unsigned int cmd, union dcf_ioctl_arg *arg)
{
	int ret = 0;

	switch (cmd) {
	case DCF_IOC_SET_USER_CTX:

		break;
	default:
		break;
	}

	return ret ? -EINVAL : 0;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int dcf_ioctl_dir(unsigned int cmd)
{
	return _IOC_DIR(cmd);
}

long dcf_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct dcf_user *user = filp->private_data;
	union dcf_ioctl_arg data;
	unsigned int dir;
	int ret = 0;

	dir = dcf_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	ret = validate_ioctl_arg(cmd, &data);
	if (ret) {
		pr_warn_once("%s: ioctl validate failed\n", __func__);
		return ret;
	}

	if (!(dir & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case DCF_IOC_SET_USER_CTX:
	{
		struct dcf_usercontext *ctx = &data.context;
		if (ctx->user_id >= DCF_USER_ANY)
			return -ERANGE;

		if (user->user_id != DCF_USER_ANY)
			return -EEXIST;

		user->user_id = ctx->user_id;
		user->user_id_other = ctx->user_id_other;

		break;
	}
	case DCF_IOC_SET_PROPERTY:
	{
		struct dcf_ioc_setproperty *setprop = &data.setprop;

#ifndef SDRV_NEW_PROPERTY
		semidrive_set_property(setprop->property_id,
			setprop->property_value);
#else
		semidrive_set_property_new(setprop->property_id,
			setprop->property_value);
#endif
		break;
	}
	case DCF_IOC_GET_PROPERTY:
	{
		struct dcf_ioc_setproperty *setprop = &data.setprop;

#ifndef SDRV_NEW_PROPERTY
		semidrive_get_property(setprop->property_id,
			&setprop->property_value);
#else
		semidrive_get_property_new(setprop->property_id,
			&setprop->property_value);
#endif
		break;
	}

	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}

static ssize_t dcf_dev_read(struct file *filp, char __user *buf,
				 size_t len, loff_t *f_pos)
{
	struct dcf_user *user = filp->private_data;
	struct dcf_device *eptdev = user->eptdev;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	unsigned long flags;
	struct sk_buff *skb;
	int use;

	if (!rpdev->ept)
		return -EPIPE;

	spin_lock_irqsave(&user->queue_lock, flags);

	/* Wait for data in the queue */
	if (skb_queue_empty(&user->queue)) {
		spin_unlock_irqrestore(&user->queue_lock, flags);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* Wait until we get data or the endpoint goes away */
		if (wait_event_interruptible(user->readq,
						 !skb_queue_empty(&user->queue) ||
						 !rpdev->ept))
			return -ERESTARTSYS;

		/* We lost the endpoint while waiting */
		if (!rpdev->ept)
			return -EPIPE;

		spin_lock_irqsave(&user->queue_lock, flags);
	}

	skb = skb_dequeue(&user->queue);
	spin_unlock_irqrestore(&user->queue_lock, flags);
	if (!skb)
		return -EFAULT;

	use = min_t(size_t, len, skb->len);
	if (copy_to_user(buf, skb->data, use))
		use = -EFAULT;

	kfree_skb(skb);

	return use;
}

static ssize_t dcf_dev_write(struct file *filp, const char __user *buf,
				  size_t len, loff_t *f_pos)
{
	struct dcf_user *user = filp->private_data;
	struct dcf_device *eptdev = user->eptdev;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	void *kbuf;
	int ret;

	kbuf = memdup_user(buf, len);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	if (mutex_lock_interruptible(&eptdev->ept_lock)) {
		ret = -ERESTARTSYS;
		goto free_kbuf;
	}

	if (!rpdev->ept) {
		ret = -EPIPE;
		goto unlock_eptdev;
	}

	if (filp->f_flags & O_NONBLOCK)
		ret = rpmsg_trysend(rpdev->ept, kbuf, len);
	else
		ret = rpmsg_send(rpdev->ept, kbuf, len);

unlock_eptdev:
	mutex_unlock(&eptdev->ept_lock);

free_kbuf:
	kfree(kbuf);
	return ret < 0 ? ret : len;
}

static unsigned int dcf_dev_poll(struct file *filp, poll_table *wait)
{
	struct dcf_user *user = filp->private_data;
	struct dcf_device *eptdev = user->eptdev;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	unsigned int mask = 0;

	if (!rpdev->ept)
		return POLLERR;

	poll_wait(filp, &user->readq, wait);

	if (!skb_queue_empty(&user->queue))
		mask |= POLLIN | POLLRDNORM;

	mask |= rpmsg_poll(rpdev->ept, filp, wait);

	return mask;
}

static int dcf_eptdev_cb(struct rpmsg_device *rpdev, void *buf, int len,
			void *priv, u32 addr)
{
	struct dcf_device *eptdev = priv;
	struct dcf_user *user;
	struct sk_buff *skb;

	/* No recepient any more, just drop this message */
	if (!atomic_read(&eptdev->user_count)) {
		return 0;
	}

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	skb_put_data(skb, buf, len);

	/*TODO: dispatch to a dcf_user according to src/dst address */
	list_for_each_entry(user, &eptdev->users, list_node) {
		spin_lock(&user->queue_lock);
		skb_queue_tail(&user->queue, skb);
		spin_unlock(&user->queue_lock);

		/* wake up any blocking processes, waiting for new data */
		wake_up_interruptible(&user->readq);

		break;
	}

	return 0;
}

static int dcf_dev_open(struct inode *inode, struct file *filp)
{
	struct dcf_device *eptdev = filp->private_data;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	struct device *dev = eptdev->dev;
	struct rpmsg_channel_info chinfo;
	struct dcf_user *user;

	get_device(dev);

	if (!atomic_read(&eptdev->user_count)) {
		mutex_init(&eptdev->ept_lock);
		INIT_LIST_HEAD(&eptdev->users);

		if (rpdev->ept) {
			dev_err(dev, "Already exist\n");
			return -EEXIST;
		}

		strncpy(chinfo.name, eptdev->name, RPMSG_NAME_SIZE);
		chinfo.src = eptdev->myaddr;
		chinfo.dst = RPMSG_ADDR_ANY;

		rpdev->ept = rpmsg_create_ept(rpdev, dcf_eptdev_cb, eptdev, chinfo);
		if (!rpdev->ept) {
			dev_err(dev, "failed to create endpoint\n");
			put_device(dev);
			return -ENOMEM;
		}
		rpdev->src = rpdev->ept->addr;

		dev_info(dev, "first user, creating ept %s %d->%d \n", chinfo.name, chinfo.src, chinfo.dst);
	}

	user = kzalloc(sizeof(struct dcf_user), GFP_KERNEL);
	if (!user) {
		dev_err(dev, "failed to create endpoint\n");
		put_device(dev);
		return -ENOMEM;
	}

	user->user_id = DCF_USER_ANY;	/* Not assigned */
	user->user_id_other = DCF_USER_ANY;
	user->eptdev = eptdev;
	user->pid = current->group_leader->pid;
	spin_lock_init(&user->queue_lock);
	skb_queue_head_init(&user->queue);
	init_waitqueue_head(&user->readq);
	list_add(&user->list_node, &eptdev->users);
	filp->private_data = user;
	atomic_inc(&eptdev->user_count);
	dev_info(dev, "user=%d pid=%d opened\n", user->user_id, user->pid);

	return 0;
}

static int dcf_dev_release(struct inode *inode, struct file *filp)
{
	struct dcf_user *user = filp->private_data;
	struct dcf_device *eptdev = user->eptdev;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	struct device *dev = eptdev->dev;
	struct sk_buff *skb;

	list_del(&user->list_node);

	/* Discard all SKBs */
	spin_lock(&user->queue_lock);
	while (!skb_queue_empty(&user->queue)) {
		skb = skb_dequeue(&user->queue);
		kfree_skb(skb);
	}
	spin_unlock(&user->queue_lock);
	dev_info(dev, "user=%d pid=%d released!\n", user->user_id, user->pid);

	if (atomic_dec_and_test(&eptdev->user_count)) {
		/* Close the endpoint, if it's not already destroyed by the parent */
		mutex_lock(&eptdev->ept_lock);
		if (rpdev->ept) {
				rpmsg_destroy_ept(rpdev->ept);
				rpdev->ept = NULL;
		}
		mutex_unlock(&eptdev->ept_lock);

		dev_info(dev, "last user=%d pid=%d released!\n", user->user_id, user->pid);
	}

	kfree(user);
	put_device(dev);

	return 0;
}

static const struct file_operations __maybe_unused cluster_fops = {
	.owner   = THIS_MODULE,
	.read    = dcf_dev_read,
	.write   = dcf_dev_write,
	.unlocked_ioctl = dcf_dev_ioctl,
	.poll    = dcf_dev_poll,
	.open    = dcf_dev_open,
	.release = dcf_dev_release,
	.llseek	 = no_llseek,
};

static const struct file_operations __maybe_unused avm_fops = {
	.llseek		= no_llseek,
	.read		= dcf_dev_read,
	.write		= dcf_dev_write,
	.unlocked_ioctl = dcf_dev_ioctl,
	.open		= dcf_dev_open,
	.release	= dcf_dev_release,
};

static const struct file_operations __maybe_unused sec_fops = {
	.llseek		= no_llseek,
	.read		= dcf_dev_read,
	.write		= dcf_dev_write,
	.unlocked_ioctl = dcf_dev_ioctl,
	.open		= dcf_dev_open,
	.release	= dcf_dev_release,
};

static const struct file_operations property_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl = dcf_dev_ioctl,
	.open    = dcf_dev_open,
	.release = dcf_dev_release,
	.llseek	 = no_llseek,
};

static struct dcf_device devlist[] = {
	[1] = { "cluster",  0666, &cluster_fops, FMODE_UNSIGNED_OFFSET, SD_CLUSTER_EPT},
	[2] = { "earlyapp", 0666, &avm_fops,     FMODE_UNSIGNED_OFFSET, SD_EARLYAPP_EPT},
	[3] = { "ssystem",  0600, &sec_fops,     FMODE_UNSIGNED_OFFSET, SD_SSYSTEM_EPT},
	[4] = { "ivi",      0666, &cluster_fops, FMODE_UNSIGNED_OFFSET, SD_IVI_EPT},
	[5] = { "loopback", 0666, &cluster_fops, FMODE_UNSIGNED_OFFSET, SD_LOOPBACK_EPT},
	[6] = { "property", 0600, &property_fops,FMODE_UNSIGNED_OFFSET, SD_PROPERTY_EPT},
};

static struct rpmsg_device_id rpmsg_bridge_id_table[] = {
	{.name = "rpmsg-cluster"},
	{.name = "rpmsg-earlyapp"},
	{.name = "rpmsg-ssystem"},
	{.name = "rpmsg-ivi"},
	{.name = "rpmsg-loopback"},
	{.name = "rpmsg-property"},
	{},
};

static dev_t dcf_major;
static struct class *dcf_class;

static int dcf_chrdev_open(struct inode *inode, struct file *filp)
{
	int minor;
	const struct dcf_device *dev;

	minor = iminor(inode);
	if (minor >= ARRAY_SIZE(devlist))
		return -ENXIO;

	dev = &devlist[minor];
	if (!dev->fops)
		return -ENXIO;

	filp->f_mode |= dev->fmode;
	filp->private_data = (void*)dev;
	replace_fops(filp, dev->fops);

	if (dev->fops->open)
		return dev->fops->open(inode, filp);

	return 0;
}

static const struct file_operations dcf_chrdev_fops = {
	.open = dcf_chrdev_open,
	.llseek = noop_llseek,
};

static char *dcf_devnode(struct device *dev, umode_t *mode)
{
	if (mode && devlist[MINOR(dev->devt)].mode)
		*mode = devlist[MINOR(dev->devt)].mode;
	return NULL;
}

static int rpmsg_bridge_probe(struct rpmsg_device *rpdev)
{
	struct dcf_device *dev = NULL;
	int minor;
	char *pstr;

	for (minor = 1; minor < ARRAY_SIZE(devlist); minor++) {
		pstr = strnstr(rpdev->id.name, devlist[minor].name, RPMSG_NAME_SIZE);
		if (pstr) {
			dev = &devlist[minor];
			if (dev->dev) {
				pr_err("Error already open device %s\n", dev->name);
				return -EALREADY;
			}
			dev->rpdev = rpdev;
			dev->minor = minor;
			dev->dev = device_create(dcf_class, &rpdev->dev, MKDEV(MAJOR(dcf_major), minor),
						  dev, devlist[minor].name);

			if (IS_ERR(dev->dev)) {
				pr_err("Error creating device %s\n", dev->name);
				return PTR_ERR(dev->dev);
			}
			dev_set_drvdata(&rpdev->dev, dev);
			dev_info(&rpdev->dev, "device %s created!\n", dev->name);
			return 0;
		}
	}

	return -ENODEV;
}

static void rpmsg_bridge_remove(struct rpmsg_device *rpdev)
{
	struct dcf_device *dev = dev_get_drvdata(&rpdev->dev);

	if (dev->dev)
		device_destroy(dcf_class, dev->dev->devt);

	dev_set_drvdata(&rpdev->dev, NULL);
	dev->rpdev = NULL;
}

static struct rpmsg_driver rpmsg_bridge_driver = {
	.probe = rpmsg_bridge_probe,
	.remove = rpmsg_bridge_remove,
	.id_table = rpmsg_bridge_id_table,
	.drv = {
		.name = "rpmsg_dcf"
	},
};

static int __init dcf_char_init(void)
{
	int major;
	int ret;

	/* Not support this in DomU */
	if (xen_domain() && !xen_initial_domain())
		return -ENODEV;

	pr_info("initialize dcf platform driver!\n");
	major = register_chrdev(0, "dcf", &dcf_chrdev_fops);
	if (major < 0) {
		pr_err("dcf: failed to register char dev region\n");
		return major;
	}

	dcf_major = MKDEV(major, 0);
	dcf_class = class_create(THIS_MODULE, "dcf");
	if (IS_ERR(dcf_class)) {
		unregister_chrdev(major, "dcf");
		return PTR_ERR(dcf_class);
	}

	dcf_class->devnode = dcf_devnode;
	/* Native environment use rpmsg channel as transportation */
	ret = register_rpmsg_driver(&rpmsg_bridge_driver);
	if (ret < 0) {
		pr_err("dcf: failed to register rpmsg driver\n");
		class_destroy(dcf_class);
		unregister_chrdev(major, "dcf");
	}

	return 0;
}

static void __exit dcf_char_exit(void)
{
	if (xen_domain() && !xen_initial_domain())
		return;

	pr_info("unregister dcf platform driver!\n");
	unregister_rpmsg_driver(&rpmsg_bridge_driver);
	class_destroy(dcf_class);
	unregister_chrdev(dcf_major, "dcf");
}

module_init(dcf_char_init);
module_exit(dcf_char_exit);

MODULE_AUTHOR("<ye.liu@semidrive.com>");
MODULE_ALIAS("dcf:chrdev");
MODULE_DESCRIPTION("Semidrive DCF User Interface device");
MODULE_LICENSE("GPL v2");

