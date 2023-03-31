/*
 * Copyright (c) 2021, Semidriver Semiconductor
 *
 * This program is free software; you can rediulinkibute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is diulinkibuted in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/sys_counter.h>
#include <linux/debugfs.h>

#define MBOX_MAX_MSG_LEN	(128)

enum {
	TS_DP_CR5_SAF,
	TS_DP_CR5_SEC,
	TS_DP_CR5_MPC,
	TS_DP_CA_AP1,
	TS_DP_CA_AP2,
	TS_DP_CPU_MAX,
};


struct time_sync_device {
	struct device			*dev;
	uint32_t			rproc;
	uint32_t			master;
	uint64_t			realtime;
	uint64_t			syscnt;
	struct mbox_chan		*chan[TS_DP_CPU_MAX];
	uint32_t			channum;
	uint8_t				message[MBOX_MAX_MSG_LEN];
	spinlock_t		lock;
	int (*tx_func)(struct time_sync_device *tdev,
			char *data, uint32_t size);
};


static struct dentry *root_debugfs_dir;


static ssize_t time_sync_rtc_write(struct file *filp,
				       const char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	struct time_sync_device *tdev = filp->private_data;
	void *data;
	int ret;
	sd_msghdr_t *msg;
	uint64_t syscnt;
	uint32_t magic = 0x01; /* RTC */

	if (count != sizeof(uint32_t)) {
		dev_err(tdev->dev, "input data error !\n");
		return -EINVAL;
	}

	if (count > MBOX_MAX_MSG_LEN - MB_MSG_HDR_SZ) {
		dev_err(tdev->dev,
			"Message length %zd greater than max allowed %ld\n",
			count, (int)MBOX_MAX_MSG_LEN - MB_MSG_HDR_SZ);
		return -EINVAL;
	}

	memset(tdev->message, 0x00, MBOX_MAX_MSG_LEN);
	if (tdev->master == 1) {
		msg = (sd_msghdr_t *)tdev->message;
		mb_msg_init_head(msg, tdev->rproc, 0, 0,
				count + MB_MSG_HDR_SZ + sizeof(syscnt) +
				sizeof(magic), 0x41);
		memcpy(msg->data, &magic, sizeof(magic));
		ret = copy_from_user(msg->data+sizeof(magic), userbuf, count);
		syscnt = semidrive_get_systime_us();
		memcpy(msg->data+sizeof(magic)+count, &syscnt, sizeof(syscnt));
		data = tdev->message;
		if (tdev->chan[0])
			ret = mbox_send_message(tdev->chan[0],
					data); /* send rtc time to saf only */
	}
	return count;
}


static ssize_t time_sync_realtime_write(struct file *filp,
				       const char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	struct time_sync_device *tdev = filp->private_data;
	void *data;
	int ret;
	sd_msghdr_t *msg;
	uint64_t syscnt;
	uint64_t realtime;
	uint32_t magic = 0x02; /* realtime */
	uint32_t pos;

	if (count != sizeof(uint64_t)) {
		dev_err(tdev->dev, "input data error !\n");
		return -EINVAL;
	}
	if (count > MBOX_MAX_MSG_LEN - MB_MSG_HDR_SZ) {
		dev_err(tdev->dev,
			"Message length %zd greater than max allowed %ld\n",
			count, (int)MBOX_MAX_MSG_LEN - MB_MSG_HDR_SZ);
		return -EINVAL;
	}

	ret = copy_from_user(&realtime, userbuf, count);
	syscnt = semidrive_get_systime_us();
	tdev->realtime = realtime;
	tdev->syscnt = syscnt;
	if (tdev->master == 1) {
		int i;

		for (i = 0; i < TS_DP_CPU_MAX; i++) {
			if (tdev->chan[i]) {
				memset(tdev->message, 0x00, MBOX_MAX_MSG_LEN);
				msg = (sd_msghdr_t *)tdev->message;
				data = tdev->message;

				mb_msg_init_head(msg, tdev->rproc, 0, 0,
						count + MB_MSG_HDR_SZ +
						sizeof(syscnt) + sizeof(magic),
						0x41);
				memcpy(msg->data, &magic, sizeof(magic));
				pos = sizeof(magic);
				memcpy(msg->data + pos,
						&realtime, sizeof(realtime));
				pos += sizeof(realtime);
				memcpy(msg->data+pos, &syscnt, sizeof(syscnt));
				ret = mbox_send_message(tdev->chan[i], data);
			}
		}
	}
	return count;
}

static ssize_t time_sync_realtime_read(struct file *filp, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	struct time_sync_device *tdev = filp->private_data;
	unsigned char *touser;
	uint64_t syscnt;
	uint64_t realtime;
	unsigned long flags;
	int ret;


	touser = kzalloc(sizeof(realtime), GFP_KERNEL);
	if (!touser)
		return -ENOMEM;

	spin_lock_irqsave(&tdev->lock, flags);
	realtime = tdev->realtime;
	syscnt = semidrive_get_systime_us();
	if (realtime != 0)
		realtime += (syscnt - tdev->syscnt);

	memcpy(touser, &realtime, sizeof(realtime));

	ret = simple_read_from_buffer(userbuf, count,
			ppos, touser, sizeof(realtime));
	spin_unlock_irqrestore(&tdev->lock, flags);

	kfree(touser);
	return ret;
}



static const struct file_operations time_sync_rtc_ops = {
	.write	= time_sync_rtc_write,
	.open	= simple_open,
	.llseek	= generic_file_llseek,
};

static const struct file_operations time_sync_realtime_ops = {
	.write	= time_sync_realtime_write,
	.read	= time_sync_realtime_read,
	.open	= simple_open,
	.llseek	= generic_file_llseek,
};

static void time_sync_receive_message(struct mbox_client *client, void *message)
{
	struct time_sync_device *tdev = dev_get_drvdata(client->dev);

	if (message) {
		sd_msghdr_t *msg = message;
		uint64_t syscnt_R;
		uint64_t syscnt_D;
		uint64_t realtime;
		uint32_t magic;
		uint32_t pos;

		memcpy(&magic, msg->data, sizeof(magic));
		pos = sizeof(magic);
		if (magic == 0x02) {
			memcpy(&realtime, msg->data+pos, sizeof(realtime));
			pos += sizeof(realtime);
			memcpy(&syscnt_R, msg->data+pos, sizeof(syscnt_R));
			syscnt_D = semidrive_get_systime_us();
			if (syscnt_D >= syscnt_R) {
				realtime += (syscnt_D - syscnt_R);
				tdev->realtime = realtime;
				tdev->syscnt = syscnt_D;
			}
		}
	}
}

static int time_sync_add_debugfs(struct platform_device *pdev,
				 struct time_sync_device *tdev)
{
	if (!debugfs_initialized())
		return 0;

	root_debugfs_dir = debugfs_create_dir("timesync", NULL);
	if (!root_debugfs_dir) {
		dev_err(&pdev->dev, "Failed to create Mailbox debugfs\n");
		return -EINVAL;
	}

	debugfs_create_file("rtc", 0600, root_debugfs_dir,
			    tdev, &time_sync_rtc_ops);

	debugfs_create_file("realtime", 0600, root_debugfs_dir,
			    tdev, &time_sync_realtime_ops);
	return 0;
}


static int time_sync_probe(struct platform_device *pdev)
{
	struct time_sync_device *tdev;
	struct mbox_client *client;
	uint32_t  sendto[TS_DP_CPU_MAX];
	int i;

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	if (of_property_read_u32(pdev->dev.of_node,
				"rproc", &tdev->rproc))
		return -ENOMEM;

	if (of_property_read_u32(pdev->dev.of_node,
				"master", &tdev->master))
		return -ENOMEM;

	if (of_property_read_u32_array(pdev->dev.of_node,
				"sendto", sendto, TS_DP_CPU_MAX))
		return -ENOMEM;


	spin_lock_init(&tdev->lock);
	tdev->tx_func = NULL;
	tdev->dev = &pdev->dev;
	tdev->realtime  = 0;
	tdev->syscnt = 0;
	tdev->channum = 0;
	memset(tdev->message, 0x00, MBOX_MAX_MSG_LEN);
	platform_set_drvdata(pdev, tdev);

	for (i = 0; i < TS_DP_CPU_MAX; i++) {
		tdev->chan[i] = 0;
		if (i != tdev->rproc  && tdev->master == 1 && sendto[i]) {
			client = devm_kzalloc(&pdev->dev,
					sizeof(*client), GFP_KERNEL);
			if (!client)
				return ERR_PTR(-ENOMEM);

			client->dev = &pdev->dev;
			client->tx_prepare	= NULL;
			client->tx_done		= NULL;
			client->tx_block	= true;
			client->tx_tout		= 20000;
			client->knows_txdone	= false;
			client->rx_callback = NULL;
			tdev->chan[i] = mbox_request_channel(client, i);
			tdev->channum++;

		}
		if (i == tdev->rproc && tdev->master == 0) {
			client = devm_kzalloc(&pdev->dev,
					sizeof(*client), GFP_KERNEL);
			if (!client)
				return ERR_PTR(-ENOMEM);

			client->dev = &pdev->dev;
			client->tx_prepare	= NULL;
			client->tx_done		= NULL;
			client->tx_block	= true;
			client->tx_tout		= 20000;
			client->knows_txdone	= false;
			client->rx_callback = time_sync_receive_message;
			tdev->chan[i] = mbox_request_channel(client, i);
			tdev->channum++;
		}
	}
	time_sync_add_debugfs(pdev, tdev);
	return 0;
}

static int time_sync_remove(struct platform_device *pdev)
{
#if 0
/*TBD*/
#endif
	return 0;
}

static const struct of_device_id time_sync_match[] = {
	{ .compatible = "sd,time-sync" },
	{},
};
MODULE_DEVICE_TABLE(of, time_sync_match);

static struct platform_driver time_sync_driver = {
	.driver = {
		.name = "time_sync",
		.of_match_table = time_sync_match,
	},
	.probe  = time_sync_probe,
	.remove = time_sync_remove,
};
#if 0
static int __init time_sync_init(void)
{
	return platform_driver_register(&time_sync_driver);
}

static void __exit time_sync_exit(void)
{
	return platform_driver_unregister(&time_sync_driver);
}

late_initcall(time_sync_init);
module_exit(time_sync_exit);
#endif
module_platform_driver(time_sync_driver);


MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("time sync(RTC) Driver");
MODULE_LICENSE("GPL v2");

