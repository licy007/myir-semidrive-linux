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
#include <linux/io.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ulink.h>
#include <linux/workqueue.h>

#define UC_BLOCK_SIZE    0x100

struct ulink_test_device {
	struct device *dev;
	struct ulink_channel *ch;
	struct ulink_channel *ap;
	struct ulink_channel *ap2;
	char	rev_data[UC_BLOCK_SIZE];
	char	ap_data[UC_BLOCK_SIZE];
	char	ap2_data[UC_BLOCK_SIZE];
};

static void test_recive(struct ulink_channel *ch, uint8_t *data, uint32_t size)
{
	struct ulink_test_device *tdev = (struct ulink_test_device *)ch->user_data;

	memcpy(tdev->rev_data, data, size);
	tdev->rev_data[size] = 0;
}

static void test_ap(struct ulink_channel *ch, uint8_t *data, uint32_t size)
{
	struct ulink_test_device *tdev = (struct ulink_test_device *)ch->user_data;

	memcpy(tdev->ap_data, data, size);
	tdev->ap_data[size] = 0;
}

static void test_ap2(struct ulink_channel *ch, uint8_t *data, uint32_t size)
{
	struct ulink_test_device *tdev = (struct ulink_test_device *)ch->user_data;

	memcpy(tdev->ap2_data, data, size);
	tdev->ap2_data[size] = 0;
}

static ssize_t send_msg_store(struct device *dev,
				struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct ulink_test_device *tdev = dev_get_drvdata(dev);

	if (tdev->ch == NULL)
		return len;
	ulink_channel_send(tdev->ch, buf, len);

	return len;
}
static DEVICE_ATTR_WO(send_msg);

static ssize_t get_msg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ulink_test_device *tdev = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", tdev->rev_data);
}
static DEVICE_ATTR_RO(get_msg);

static ssize_t send_ap_store(struct device *dev,
				struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct ulink_test_device *tdev = dev_get_drvdata(dev);

	if (tdev->ap == NULL)
		return len;
	ulink_channel_send(tdev->ap, buf, len);

	return len;
}
static DEVICE_ATTR_WO(send_ap);

static ssize_t get_ap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ulink_test_device *tdev = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", tdev->ap_data);
}
static DEVICE_ATTR_RO(get_ap);

static ssize_t send_ap2_store(struct device *dev,
				struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct ulink_test_device *tdev = dev_get_drvdata(dev);

	if (tdev->ap2 == NULL)
		return len;
	ulink_channel_send(tdev->ap2, buf, len);

	return len;
}
static DEVICE_ATTR_WO(send_ap2);

static ssize_t get_ap2_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ulink_test_device *tdev = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", tdev->ap2_data);
}
static DEVICE_ATTR_RO(get_ap2);

static int ulink_test_probe(struct platform_device *pdev)
{
	struct ulink_test_device *tdev;

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	tdev->ch = ulink_request_channel(DP_CR5_SAF, 0xdef, test_recive);
	if (tdev->ch)
		tdev->ch->user_data = tdev;
	tdev->ap = ulink_request_channel(DP_CA_AP1, 0xddd, test_ap);
	if (tdev->ap)
		tdev->ap->user_data = tdev;
	tdev->ap2 = ulink_request_channel(DP_CA_AP2, 0xddd, test_ap2);
	if (tdev->ap2)
		tdev->ap2->user_data = tdev;

	device_create_file(&pdev->dev, &dev_attr_send_msg);
	device_create_file(&pdev->dev, &dev_attr_get_msg);
	device_create_file(&pdev->dev, &dev_attr_send_ap);
	device_create_file(&pdev->dev, &dev_attr_get_ap);
	device_create_file(&pdev->dev, &dev_attr_send_ap2);
	device_create_file(&pdev->dev, &dev_attr_get_ap2);

	tdev->dev = &pdev->dev;
	platform_set_drvdata(pdev, tdev);
	return 0;
}

static int ulink_test_remove(struct platform_device *pdev)
{
	struct ulink_test_device *tdev = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_get_ap2);
	device_remove_file(&pdev->dev, &dev_attr_send_ap2);
	device_remove_file(&pdev->dev, &dev_attr_get_ap);
	device_remove_file(&pdev->dev, &dev_attr_send_ap);
	device_remove_file(&pdev->dev, &dev_attr_get_msg);
	device_remove_file(&pdev->dev, &dev_attr_send_msg);

	if (tdev->ch)
		ulink_release_channel(tdev->ch);
	if (tdev->ap)
		ulink_release_channel(tdev->ap);
	if (tdev->ap2)
		ulink_release_channel(tdev->ap2);
	return 0;
}

static const struct of_device_id ulink_test_match[] = {
	{ .compatible = "sd,ulink-test" },
	{},
};
MODULE_DEVICE_TABLE(of, ulink_test_match);

static struct platform_driver ulink_test_driver = {
	.driver = {
		.name = "ulink_test",
		.of_match_table = ulink_test_match,
	},
	.probe  = ulink_test_probe,
	.remove = ulink_test_remove,
};
module_platform_driver(ulink_test_driver);

MODULE_DESCRIPTION("ulink test driver");
MODULE_AUTHOR("mingmin.ling <mingmin.ling@semidrive.com");
MODULE_LICENSE("GPL v2");
