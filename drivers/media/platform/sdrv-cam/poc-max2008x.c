/*
 * Copyright (C) 2021 Semidrive, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/of_platform.h>
#include <linux/acpi.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>


enum POC_CHIP_INDEX {
	POC_CHIP_INDEX_MDESER0 = 0,
	POC_CHIP_INDEX_MDESER1 = 1,
	POC_CHIP_INDEX_R_MDESER0 = 2,
	POC_CHIP_INDEX_R_MDESER1 = 3,
	POC_CHIP_INDEX_MDESER_RESER = 4,
};

enum __MAX2008X_REG_LIST {
	MAX2008X_CMD_MASK = 0x00,
	MAX2008X_CMD_CONFIG = 0x01,
	MAX2008X_CMD_ID = 0x02,
	MAX2008X_CMD_STAT1 = 0x03,
	MAX2008X_CMD_STAT2_0 = 0x04,
	MAX2008X_CMD_STAT2_1 = 0x05,
	MAX2008X_CMD_ADC1 = 0x06,
	MAX2008X_CMD_ADC2 = 0x07,
	MAX2008X_CMD_ADC3 = 0x08,
	MAX2008X_CMD_ADC4 = 0x09,
};

#define MAX_POC_CHIP_NUM (5)

#define DISABLE_ALL_LINK_POWER 0x10
#define ENABLE_ALL_LINK_POWER 0x1f


struct max2008x_chip {
	int index;
	char *names;
	struct mutex i2c_lock;
	struct i2c_client *client;
	struct gpio_desc *poc_gpiod;
	unsigned long driver_data;
};

static struct max2008x_chip *poc_max2008x[MAX_POC_CHIP_NUM];


static int max2008x_read_8b(struct max2008x_chip *dev, u8 reg, u8 *val)
{
	struct i2c_client *client = dev->client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: chip 0x%02x error: reg=0x%02x\n",
			__func__, msg[0].addr, reg);
		return ret;
	}

	*val = buf[0];

	return 0;
}

static int max2008x_write_8b(struct max2008x_chip *dev, u8 reg, u8 val)
{
	struct i2c_client *client = dev->client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 2;
	pr_debug("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
			__func__, msg.addr, reg, val);
		return ret;
	}

	return 0;
}

int poc_mdeser0_power(int chan, int enable, u8 reg, u8 val)
{
	int ret = -1;
	u8 value;
	u8 addr;
	u8 sd507_5071_poc_addr[2] = {0x28, 0x2a}; //For sd507 db5071subboard
	struct max2008x_chip *ppoc = poc_max2008x[POC_CHIP_INDEX_MDESER0];

	if (NULL == ppoc) {
		pr_err("There is no poc for this deser. Don't change deser's name.\n");
		return -EINVAL;
	}

	if (ppoc->poc_gpiod) {
		gpiod_direction_output(ppoc->poc_gpiod, enable);
	}

	addr = ppoc->client->addr;

	ret = max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	if (ret < 0) {
		if (ppoc->client->addr == sd507_5071_poc_addr[0]) { // Try another poc address.
			ppoc->client->addr = sd507_5071_poc_addr[1];
		} else {
			ppoc->client->addr = sd507_5071_poc_addr[0];
		}
		dev_err(&ppoc->client->dev, "Try different poc address 0x%x\n", ppoc->client->addr);
		max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	}
	pr_debug("MAX208X_CMD_CONFIG = 0x%x en = %d\n", value, enable);
	switch (chan) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 0xf:
		value = 0x1f;
		break;
	case 0xa:  //Support other POC chips
		value = val;
		break;
	default:
		pr_err("Not support %d channel.\n", chan);
		return -EINVAL;
	}

	if (enable == 1)
		ret = max2008x_write_8b(ppoc, reg, ENABLE_ALL_LINK_POWER);
	else if (enable == 0)
		ret = max2008x_write_8b(ppoc, reg, DISABLE_ALL_LINK_POWER);


	max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	pr_debug("MAX208X_CMD_CONFIG = 0x%x\n", value);

	ppoc->client->addr = addr;

	return ret;
}
EXPORT_SYMBOL(poc_mdeser0_power);

int poc_mdeser1_power(int chan, int enable, int reg, u32 val)
{
	int ret = -1;
	u8 value;
	u8 addr;
	u8 sd507_5071_poc_addr[2] = {0x29, 0x2b}; //For sd507 db5071subboard

	struct max2008x_chip *ppoc = poc_max2008x[POC_CHIP_INDEX_MDESER1];

	if (NULL == ppoc) {
		pr_err("There is no poc for this deser. Don't change deser's name.\n");
		return -EINVAL;
	}

	if (ppoc->poc_gpiod) {
		gpiod_direction_output(ppoc->poc_gpiod, enable);
	}


	addr = ppoc->client->addr;

	ret = max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	if (ret < 0) {
		if (ppoc->client->addr == sd507_5071_poc_addr[0]) { // Try another poc address.
			ppoc->client->addr = sd507_5071_poc_addr[1];
		} else {
			ppoc->client->addr = sd507_5071_poc_addr[0];
		}
		dev_err(&ppoc->client->dev, "Try different poc address 0x%x\n", ppoc->client->addr);
		max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	}

	pr_debug("MAX208X_CMD_CONFIG = 0x%x enable = %d\n", value, enable);
	switch (chan) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 0xf:
		value = 0x1f;
		break;
	case 0xa:  //Support other POC chips
		value = val;
		break;
	default:
		pr_err("Not support %d channel.", chan);
		return -EINVAL;
	}

	if (enable == 1)
		ret = max2008x_write_8b(ppoc, reg, ENABLE_ALL_LINK_POWER);
	else if (enable == 0)
		ret = max2008x_write_8b(ppoc, reg, DISABLE_ALL_LINK_POWER);

	max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	pr_debug("MAX208X_CMD_CONFIG = 0x%x\n", value);

	return ret;

}
EXPORT_SYMBOL(poc_mdeser1_power);

#if defined(CONFIG_ARCH_SEMIDRIVE_V9)
int poc_r_mdeser0_power(int chan, int enable,  u8 reg, u8 val)
{
	int ret = -1;
	u8 value;
	struct max2008x_chip *ppoc = poc_max2008x[POC_CHIP_INDEX_R_MDESER0];

	if (NULL == ppoc) {
		pr_err("There is no poc for this deser. Don't change deser's name.\n");
		return -EINVAL;
	}

	if (ppoc->poc_gpiod) {
		gpiod_direction_output(ppoc->poc_gpiod, enable);
	}

	max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	pr_debug("MAX208X_CMD_CONFIG = 0x%x en = %d\n", value, enable);
	switch (chan) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 0xf:
		value = 0x1f;
		break;
	case 0xa:  //Support other POC chips
		value = val;
		break;
	default:
		pr_err("Not support %d channel.", chan);
		return -EINVAL;
	}

	if (enable == 1)
		ret = max2008x_write_8b(ppoc, reg, ENABLE_ALL_LINK_POWER);
	else if (enable == 0)
		ret = max2008x_write_8b(ppoc, reg, DISABLE_ALL_LINK_POWER);


	max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	pr_debug("MAX208X_CMD_CONFIG = 0x%x\n", value);

	return ret;
}
EXPORT_SYMBOL(poc_r_mdeser0_power);

int poc_r_mdeser1_power(int chan, int enable, int reg, u32 val)
{
	int ret = -1;
	u8 value;
	struct max2008x_chip *ppoc = poc_max2008x[POC_CHIP_INDEX_R_MDESER1];

	if (NULL == ppoc) {
		pr_err("There is no poc for this deser. Don't change deser's name.\n");
		return -EINVAL;
	}

	if (ppoc->poc_gpiod) {
		gpiod_direction_output(ppoc->poc_gpiod, enable);
	}

	max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	pr_debug("MAX208X_CMD_CONFIG = 0x%x en = %d\n", value, enable);
	switch (chan) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 0xf:
		value = 0x1f;
		break;
	case 0xa:  //Support other POC chips
		value = val;
		break;
	default:
		pr_err("Not support %d channel.", chan);
		return -EINVAL;
	}

	if (enable == 1)
		ret = max2008x_write_8b(ppoc, reg, ENABLE_ALL_LINK_POWER);
	else if (enable == 0)
		ret = max2008x_write_8b(ppoc, reg, DISABLE_ALL_LINK_POWER);

	max2008x_read_8b(ppoc, MAX2008X_CMD_CONFIG, &value);
	pr_debug("MAX208X_CMD_CONFIG = 0x%x en = %d\n", value, enable);

	return ret;

}
EXPORT_SYMBOL(poc_r_mdeser1_power);
#endif

static int max2008x_device_init(struct max2008x_chip *chip)
{
	//Disable camera module power in default.
	max2008x_write_8b(chip, MAX2008X_CMD_CONFIG, DISABLE_ALL_LINK_POWER);
	//if (chip->poc_gpiod)
	//	gpiod_direction_output(chip->poc_gpiod, 0);
	return 0;
}

static int max2008x_probe(struct i2c_client *client,
				   const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
	struct max2008x_chip *dev_max2008x;
	struct gpio_desc *gpiod;
	u8 val;
	int ret;

	pr_debug("%s name = %s +\n", __FUNCTION__, client->name);
	dev_max2008x = devm_kzalloc(dev, sizeof(*dev_max2008x), GFP_KERNEL);
	if (!dev_max2008x) {
		dev_err(&client->dev, "No memory!");
		return -ENOMEM;
	}

	dev_max2008x->client = client;

	ret = max2008x_read_8b(dev_max2008x, MAX2008X_CMD_ID, &val);
	if (ret < 0) {
		dev_err(&client->dev, "Can't detect poc device!");
		return -ENODEV;
	}

	if (!strcmp(client->name, "deser0_poc"))
		poc_max2008x[POC_CHIP_INDEX_MDESER0] = dev_max2008x;
	else if (!strcmp(client->name, "deser1_poc"))
		poc_max2008x[POC_CHIP_INDEX_MDESER1] = dev_max2008x;
	else if (!strcmp(client->name, "rdeser0_poc"))
		poc_max2008x[POC_CHIP_INDEX_R_MDESER0] = dev_max2008x;
	else if (!strcmp(client->name, "rdeser1_poc"))
		poc_max2008x[POC_CHIP_INDEX_R_MDESER1] = dev_max2008x;
	else
		poc_max2008x[POC_CHIP_INDEX_MDESER_RESER] = dev_max2008x;

	dev_max2008x->index++;
	gpiod = devm_gpiod_get_optional(&client->dev, "en", GPIOD_OUT_LOW);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"en", ret);
	} else {
		dev_max2008x->poc_gpiod = gpiod;
	}

	max2008x_device_init(dev_max2008x);

	return 0;
}

static int max2008x_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id max2008x_dt_ids[] = {
	{ .compatible = "max2008x,deser0_poc"},
	{ .compatible = "max2008x,deser1_poc"},
	{}
};

MODULE_DEVICE_TABLE(of, max2008x_dt_ids);

static struct i2c_driver max2008x_driver = {
	.driver = {
		.name	= "max2008x",
		.of_match_table = max2008x_dt_ids,
	},
	.probe		= max2008x_probe,
	.remove		= max2008x_remove,
};

static int __init max2008x_i2c_init(void)
{
	return i2c_add_driver(&max2008x_driver);
}
subsys_initcall(max2008x_i2c_init);

static void __exit max2008x_exit(void)
{
	return i2c_del_driver(&max2008x_driver);
}
module_exit(max2008x_exit);

#if defined(CONFIG_ARCH_SEMIDRIVE_V9)
static const struct of_device_id max2008x_dt_ids_sideb[] = {
	{ .compatible = "max2008x,rdeser0_poc"},
	{ .compatible = "max2008x,rdeser1_poc"},
	{}
};

MODULE_DEVICE_TABLE(of, max2008x_dt_ids_sideb);

static struct i2c_driver max2008x_driver_sideb = {
	.driver = {
		.name	= "max2008x_sideb",
		.of_match_table = max2008x_dt_ids_sideb,
	},
	.probe		= max2008x_probe,
	.remove		= max2008x_remove,
};

int register_sideb_poc_driver(void)
{
	int ret;

	ret = i2c_add_driver(&max2008x_driver_sideb);
	if (ret < 0)
		pr_err("fail to register sideb poc driver.\n");
	return ret;
}
EXPORT_SYMBOL(register_sideb_poc_driver);
#endif


MODULE_AUTHOR("michael chen");
MODULE_DESCRIPTION("Camera POC driver for max2008x");
MODULE_LICENSE("GPL");
