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


static int max2008x_read_reg(struct i2c_client *client, u8 addr, u8 reg,
							 u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;
	u32 ori_addr;

	ori_addr = client->addr;
	client->addr = addr;
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
	client->addr = ori_addr;
	if (ret < 0) {
		dev_err(&client->dev, "%s: chip 0x%02x error: reg=0x%02x\n",
				__func__, msg[0].addr, reg);
		return ret;
	}
	*val = buf[0];
	return 0;
}

static int max2008x_write_reg(struct i2c_client *client, u8 addr, u8 reg,
							  u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;
	u32 ori_addr;

	ori_addr = client->addr;
	client->addr = addr;
	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 2;
	pr_debug("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = i2c_transfer(client->adapter, &msg, 1);
	client->addr = ori_addr;
	if (ret < 0) {
		dev_err(&client->dev, "%s: chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
				__func__, msg.addr, reg, val);
		return ret;
	}
	return 0;
}


int poc_power(struct i2c_client *client, struct gpio_desc *gpiod, u8 addr,
			  int chan, int enable)
{
	u8 val;
	int ret = -1;

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"en", ret);
		return -ENODEV;
	}
	if (gpiod && (enable == 1)) {
		gpiod_direction_output(gpiod, enable);
	}
	if (gpiod && (enable == 0) && (chan == 0xf)) {
		gpiod_direction_output(gpiod, enable);
	}
	max2008x_read_reg(client, addr, MAX2008X_CMD_CONFIG, &val);
	if (enable == 1) {
		if (chan != 0xf)
			val = val | (1<<chan);
		else
			val = val | chan;
		dev_info(&client->dev, "chan 0x%x set value 0x%x\n", chan, val);
		//currently for some daughter board(DB5071) the four channels need to be all opened to avoid reverse channel iic failure
		//Will revise later
		ret = max2008x_write_reg(client, addr, MAX2008X_CMD_CONFIG, /*val*/0x1f);
	} else if (enable == 0) {
		if (chan != 0xf)
			val = val & (~(1<<chan));
		else
			val = val & (~chan);
		ret = max2008x_write_reg(client, addr, MAX2008X_CMD_CONFIG, /*val*/0x10);
	}
	max2008x_read_reg(client, addr, MAX2008X_CMD_CONFIG, &val);
	dev_info(&client->dev, "MAX208X_CMD_CONFIG = 0x%x\n", val);
	return ret;
}
EXPORT_SYMBOL(poc_power);

#if 0 // defined_but_no_used
static int max2008x_device_init(struct i2c_client *client, u8 addr)
{
	//Disable camera module power in default.
	max2008x_write_reg(client, addr, MAX2008X_CMD_CONFIG,
					   DISABLE_ALL_LINK_POWER);
	return 0;
}
#endif // defined_but_no_used

MODULE_AUTHOR("michael chen");
MODULE_DESCRIPTION("Camera POC driver for max2008x");
MODULE_LICENSE("GPL");
