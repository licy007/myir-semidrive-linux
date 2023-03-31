/*
 * Copyright (c) 2021, Semidriver Semiconductor
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
#include <linux/input.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>

#define STR_MB_ADDR 0x49
#define STR_RESUME_MAGIC 0x8765
/*str sync ap12ap2*/
struct str_sync_device {
	struct device *dev;
	struct mbox_chan *chan;
	struct workqueue_struct *str_queue;
	struct work_struct str_work;
	uint32_t host;
};

static int str_sync_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct str_sync_device *tdev = platform_get_drvdata(pdev);
	sd_msghdr_t msg;
	int ret;

	if (tdev->host == 0)
		return 0;

	mb_msg_init_head(&msg, 4, 0, 0, MB_MSG_HDR_SZ, STR_MB_ADDR);

	ret = mbox_send_message(tdev->chan, &msg);
	if (ret < 0)
		return ret;
	return 0;
}

void str_queue_work(struct work_struct *work)
{
	pm_suspend(PM_SUSPEND_MEM);
}

static void str_sync_receive(struct mbox_client *client, void *message)
{
	sd_msghdr_t *msg = message;
	struct device *dev = client->dev;
	struct str_sync_device *tdev = dev_get_drvdata(dev);

	if (msg->addr != STR_MB_ADDR)
		return;

	if (tdev->str_queue)
		queue_work(tdev->str_queue, &tdev->str_work);
}

static struct mbox_chan *str_sync_mbox_request(struct platform_device *pdev,
					       uint32_t host)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	if (host == 1)
		client->rx_callback = NULL;
	else
		client->rx_callback = str_sync_receive;
	client->tx_prepare = NULL;
	client->tx_done = NULL;
	client->tx_block = false;
	client->knows_txdone = false;

	channel = mbox_request_channel(client, 0);
	if (IS_ERR(channel)) {
		dev_warn(&pdev->dev, "Failed to request channel\n");
		return NULL;
	}

	return channel;
}

static int str_sync_probe(struct platform_device *pdev)
{
	struct str_sync_device *tdev;

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	if (of_property_read_u32(pdev->dev.of_node, "host", &tdev->host))
		tdev->host = 0;

	tdev->chan = str_sync_mbox_request(pdev, tdev->host);
	if (!tdev->chan)
		return -EPROBE_DEFER;

	tdev->dev = &pdev->dev;

	if (tdev->host == 0) {
		tdev->str_queue = create_singlethread_workqueue("str");
		INIT_WORK(&tdev->str_work, str_queue_work);
	}

	platform_set_drvdata(pdev, tdev);

	return 0;
}

static int str_sync_remove(struct platform_device *pdev)
{
	struct str_sync_device *tdev = platform_get_drvdata(pdev);

	if (tdev->chan)
		mbox_free_channel(tdev->chan);
	if (tdev->str_queue)
		destroy_workqueue(tdev->str_queue);

	return 0;
}

static const struct of_device_id str_sync_match[] = {
	{ .compatible = "sd,str-sync" },
	{},
};
MODULE_DEVICE_TABLE(of, str_sync_match);

static SIMPLE_DEV_PM_OPS(str_sync_ops, str_sync_suspend, NULL);

static struct platform_driver str_sync_driver = {
	.driver = {
			.name = "str_sync",
			.of_match_table = str_sync_match,
			.pm = &str_sync_ops,
		},
	.probe = str_sync_probe,
	.remove = str_sync_remove,
};
module_platform_driver(str_sync_driver);
/*end str sync ap12ap2*/

/*v power key*/
static struct input_dev *str_power_key;
static void __iomem *str_addr;

static int str_power_key_resume(struct device *dev)
{
	unsigned int value;

	if (!str_power_key)
		return 0;

	value = readl(str_addr);

	if ((value & 0xffff) == STR_RESUME_MAGIC) { // str resume
		input_report_key(str_power_key, KEY_POWER, 1);
		input_report_key(str_power_key, KEY_POWER, 0);
		input_sync(str_power_key);
	}
	return 0;
}

static int str_power_key_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	str_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(str_addr))
		return PTR_ERR(str_addr);

	str_power_key = input_allocate_device();
	if (!str_power_key) {
		ret = -ENOMEM;
		goto error;
	}

	str_power_key->name = "str-power-key";
	input_set_capability(str_power_key, EV_KEY, KEY_POWER);
	ret = input_register_device(str_power_key);
	if (ret) {
		input_free_device(str_power_key);
		str_power_key = NULL;
		dev_err(&pdev->dev, "input_register_device(str_power_key)");
		goto error;
	}

	return 0;
error:

	return ret;
}

static int str_power_key_remove(struct platform_device *pdev)
{
	if (str_power_key) {
		input_unregister_device(str_power_key);
		input_free_device(str_power_key);
	}
	return 0;
}

static const struct of_device_id str_power_key_match[] = {
	{ .compatible = "sd,str-power-key" },
	{},
};
MODULE_DEVICE_TABLE(of, str_power_key_match);

static SIMPLE_DEV_PM_OPS(str_power_key_ops, NULL, str_power_key_resume);

static struct platform_driver str_power_key_driver = {
	.driver = {
			.name = "str_power_key",
			.of_match_table = str_power_key_match,
			.pm = &str_power_key_ops,
		},
	.probe = str_power_key_probe,
	.remove = str_power_key_remove,
};
module_platform_driver(str_power_key_driver);
/*end v power key*/

MODULE_DESCRIPTION("STR misc tools driver");
MODULE_AUTHOR("mingmin.ling <mingmin.ling@semidrive.com");
MODULE_LICENSE("GPL v2");
