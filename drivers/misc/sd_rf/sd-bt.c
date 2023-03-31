/*
 * Bluetooth Power Enable
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/rfkill.h>
#include <linux/of_gpio.h>
#include "sd_bt.h"

static int sd_bt_on(struct sd_bt_data *data, bool on_off)
{
	if (!on_off && gpio_is_valid(data->gpio_bt_en))
		gpio_set_value(data->gpio_bt_en, 0);
	if (on_off && gpio_is_valid(data->gpio_bt_en)) {
		mdelay(10);
		gpio_set_value(data->gpio_bt_en, 1);
	}
	data->power_state = on_off;
	return 0;
}

static int sd_bt_set_block(void *data, bool blocked)
{
	struct sd_bt_data *platdata = data;
	struct platform_device *pdev = platdata->pdev;
	int ret;

	if (blocked != platdata->power_state) {
		dev_warn(&pdev->dev, "bt block state already is %d\n", blocked);
		return 0;
	}
	ret = sd_bt_on(platdata, !blocked);
	if (ret) {
		dev_err(&pdev->dev, "set block failed\n");
		return ret;
	}
	return 0;
}

static const struct rfkill_ops sd_bt_rfkill_ops = {
	.set_block = sd_bt_set_block,
};

static int sd_bt_probe(struct platform_device *pdev)
{
	int rc, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct sd_bt_data *data;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	data->pdev = pdev;
	data->gpio_bt_en = of_get_named_gpio(np, "bt_en", 0);
	if (data->gpio_bt_en < 0) {
		data->gpio_bt_en = -1;
		return -1;
	}
	if (data->gpio_bt_en != -1) {
		ret = gpio_request(data->gpio_bt_en, "bt_en");
		gpio_direction_output(data->gpio_bt_en, 0);
	}
	data->gps_reset = of_get_named_gpio(np, "gps_reset", 0);
	if (data->gps_reset < 0) {
		data->gps_reset = -1;
	}
	if (data->gps_reset != -1) {
		ret = gpio_request(data->gps_reset, "gps_reset");
		gpio_set_value_cansleep(data->gps_reset, 1);
		msleep(20);
		gpio_set_value_cansleep(data->gps_reset, 0);
		msleep(20);
		gpio_set_value_cansleep(data->gps_reset, 1);
	}
	data->rfkill = rfkill_alloc("sd-rfkill", dev, RFKILL_TYPE_BLUETOOTH,
				&sd_bt_rfkill_ops, data);
	if (!data->rfkill) {
		rc = -ENOMEM;
		goto err_rfkill;
	}
	rfkill_set_states(data->rfkill, true, false);
	rc = rfkill_register(data->rfkill);
	if (rc)
		goto err_rfkill;
	platform_set_drvdata(pdev, data);
	data->power_state = 0;
	return 0;
err_rfkill:
	rfkill_destroy(data->rfkill);
	gpio_free(data->gpio_bt_en);
	gpio_free(data->gps_reset);
	return rc;
}

static int sd_bt_remove(struct platform_device *dev)
{
	struct sd_bt_data *data = dev->dev.platform_data;
	struct rfkill *rfk = platform_get_drvdata(dev);
	platform_set_drvdata(dev, NULL);
	if (rfk) {
		rfkill_unregister(rfk);
		rfkill_destroy(rfk);
	}
	rfk = NULL;
	gpio_free(data->gpio_bt_en);
	return 0;
}

static const struct of_device_id sd_bt_ids[] = {
	{ .compatible = "semidrive,sd-rfkill" },
	{}
};
static struct platform_driver sd_bt_driver = {
	.probe = sd_bt_probe,
	.remove = sd_bt_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name = "sd-rfkill",
		.of_match_table	= sd_bt_ids,
	},
};
module_platform_driver(sd_bt_driver);
MODULE_DESCRIPTION("semidrive bluetooth enable driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David");

