/*
 * semidrive rfkill module
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
#include <linux/gpio/consumer.h>

struct sd_rfkill_data {
	const char              *name;
	enum rfkill_type        type;
	struct gpio_desc        *reset_gpio;
	struct gpio_desc        *en_gpio;
	int                     delay;
	struct rfkill           *rfkill_dev;
	bool                    isblocked;
};

static int sd_rfkill_set_power(void *data, bool blocked)
{
	struct sd_rfkill_data *rfkill = data;

	if (blocked == rfkill->isblocked) {
		pr_warn("%s block state already is %d\n", rfkill->name, blocked);
		return 0;
	}
	if (blocked) {
		gpiod_set_value_cansleep(rfkill->en_gpio, false);
		gpiod_set_value_cansleep(rfkill->reset_gpio, false);
	} else {
		if (rfkill->reset_gpio) {
			gpiod_set_value_cansleep(rfkill->reset_gpio, true);
			msleep(rfkill->delay);
			gpiod_set_value_cansleep(rfkill->reset_gpio, false);
			msleep(rfkill->delay);
			gpiod_set_value_cansleep(rfkill->reset_gpio, true);
		}
		if (rfkill->en_gpio) {
			msleep(rfkill->delay);
			gpiod_set_value_cansleep(rfkill->en_gpio, true);
		}
	}
	rfkill->isblocked = blocked;
	return 0;
}

static const struct rfkill_ops sd_rfkill_ops = {
	.set_block = sd_rfkill_set_power,
};
static int sd_rfkill_probe_item(struct platform_device *pdev,
					const char *type_name, struct sd_rfkill_data **rfk)
{
	struct sd_rfkill_data *rfkill;
	struct gpio_desc *gpio;
	char prop[50] = {""};
	int ret;

	rfkill = devm_kzalloc(&pdev->dev, sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;
	sprintf(prop, "%s-name", type_name);
	device_property_read_string(&pdev->dev, prop, &rfkill->name);
	sprintf(prop, "%s-delay", type_name);
	device_property_read_u32_array(&pdev->dev, prop, &rfkill->delay, 1);
	if (!rfkill->name) {
		rfkill->name = devm_kstrdup(&pdev->dev, type_name, GFP_KERNEL);
		if (!rfkill->name) {
			//devm_kfree(&pdev->dev, rfkill);
			return -ENOMEM;
		}
	}
	rfkill->type = rfkill_find_type(type_name);
	sprintf(prop, "%s-reset", type_name);
	gpio = devm_gpiod_get_optional(&pdev->dev, prop, GPIOD_OUT_LOW);
	if (IS_ERR(gpio)) {
		//devm_kfree(&pdev->dev, rfkill->name);
		//devm_kfree(&pdev->dev, rfkill);
		return PTR_ERR(gpio);
	}
	rfkill->reset_gpio = gpio;
	sprintf(prop, "%s-en", type_name);
	gpio = devm_gpiod_get_optional(&pdev->dev, prop, GPIOD_OUT_LOW);
	if (IS_ERR(gpio)) {
		//devm_kfree(&pdev->dev, rfkill->name);
		//devm_kfree(&pdev->dev, rfkill);
		return PTR_ERR(gpio);
	}
	rfkill->en_gpio = gpio;
	if (!rfkill->reset_gpio && !rfkill->en_gpio) {
		//devm_kfree(&pdev->dev, rfkill->name);
		//devm_kfree(&pdev->dev, rfkill);
		dev_err(&pdev->dev, "%s invalid platform data\n", type_name);
		return -EINVAL;
	}
	rfkill->rfkill_dev = rfkill_alloc(rfkill->name, &pdev->dev, rfkill->type,
						&sd_rfkill_ops, rfkill);
	if (!rfkill->rfkill_dev) {
		//devm_kfree(&pdev->dev, rfkill->name);
		//devm_kfree(&pdev->dev, rfkill);
		return -ENOMEM;
	}
	rfkill_set_states(rfkill->rfkill_dev, true, false);
	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		goto err_destroy;
	//platform_set_drvdata(pdev, rfkill);
	*rfk = rfkill;
	// init to block state
	sd_rfkill_set_power(rfkill, true);
	dev_info(&pdev->dev, "%s device registered.\n", rfkill->name);
	return 0;
err_destroy:
	rfkill_destroy(rfkill->rfkill_dev);
	//devm_kfree(&pdev->dev, rfkill->name);
	//devm_kfree(&pdev->dev, rfkill);
	return ret;
}
static int sd_rfkill_remove_item(struct platform_device *pdev, struct sd_rfkill_data *rfkill)
{
	if (rfkill) {
		rfkill_unregister(rfkill->rfkill_dev);
		rfkill_destroy(rfkill->rfkill_dev);
	}
	return 0;
}

static struct sd_rfkill_data *rfkill_bluetooth;
static struct sd_rfkill_data *rfkill_wlan;
static struct sd_rfkill_data *rfkill_gps;
static int sd_rfkill_probe(struct platform_device *pdev)
{
	int cnt = 0;

	cnt++;
	if (sd_rfkill_probe_item(pdev, "bluetooth", &rfkill_bluetooth) != 0)
		cnt--;
	cnt++;
	if (sd_rfkill_probe_item(pdev, "wlan", &rfkill_wlan) != 0)
		cnt--;
	cnt++;
	if (sd_rfkill_probe_item(pdev, "gps", &rfkill_gps) != 0)
		cnt--;
	return cnt == 0 ? -1 : 0;
}

static int sd_rfkill_remove(struct platform_device *pdev)
{
	sd_rfkill_remove_item(pdev, rfkill_bluetooth);
	sd_rfkill_remove_item(pdev, rfkill_wlan);
	sd_rfkill_remove_item(pdev, rfkill_gps);
	return 0;
}

static const struct of_device_id sd_rfkill_ids[] = {
	{ .compatible = "semidrive,sd-rfkill-v2" },
	{}
};
static struct platform_driver sd_rfkill_driver = {
	.probe = sd_rfkill_probe,
	.remove = sd_rfkill_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name = "sd-rfkill-v2",
		.of_match_table	= sd_rfkill_ids,
	},
};
module_platform_driver(sd_rfkill_driver);
MODULE_DESCRIPTION("semidrive rfkill driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David");
