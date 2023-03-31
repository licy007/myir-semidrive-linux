/*
 * Copyright (C) 2020-2030 Semidrive, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include "sdrv_serdes_core.h"

#define MAX96706_CHECK_LINK_COUNT	(100)

#define MAX96706_DEVICE_ID 0x4A
#define MAX96705_DEVICE_ID 0x41

static int max96705_read_reg(struct serdes_dev_t *serdes,
							 u8 addr,
							 u8 reg,
							 u8 *val);
static int max96706_fengjing_initialization(struct serdes_dev_t *serdes);
void max96706_output_enable(struct serdes_dev_t *serdes, bool enable);

static int max96706_write_reg(struct serdes_dev_t *serdes, u8 reg, u8 val)
{
	struct i2c_client *client = serdes->i2c_client;

	return serdes_write_reg_8(client, reg, val);
}

static int max96706_read_reg(struct serdes_dev_t *serdes, u8 reg, u8 *val)
{
	struct i2c_client *client = serdes->i2c_client;

	return serdes_read_reg_8(client, reg, val);
}

static void max96706_power(struct serdes_dev_t *serdes, bool enable)
{
	gpiod_direction_output(serdes->pwdn_gpio, enable ? 1 : 0);
	usleep_range(5000, 6000);
}

static void max96706_poc_power(struct serdes_dev_t *serdes, bool enable)
{
	gpiod_direction_output(serdes->poc_gpio, enable ? 1 : 0);
	usleep_range(5000, 6000);
}

#if 0
static int max96706_check_chip_id(struct serdes_dev_t *serdes)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	max96706_power(serdes, true);
	ret = max96706_read_reg(serdes, 0x1e, &chip_id);
	if (ret) {
		dev_err(&client->dev, "%s: failed to read chip identifier\n",
				__func__);
	}
	if (chip_id != MAX96706_DEVICE_ID) {
		dev_err(&client->dev,
				"%s: wrong chip identifier, expected 0x%x(max96706), got 0x%x\n",
				__func__, MAX96706_DEVICE_ID, chip_id);
		ret = -ENXIO;
	}
	return ret;
}
#endif

static int max96706_get_lock_status(struct serdes_dev_t *serdes)
{
	u8 val = 0;

	max96706_read_reg(serdes, 0x04, &val);
	val = (val>>7)&0x1;
	return val&serdes->serdes_streamon_bitmap;
}

static int max96706_get_lock_status_ex(struct serdes_dev_t *serdes, int channel)
{
	u8 val = 0;
	u8 link_status = 0;

	max96705_read_reg(serdes, serdes->remap_addr_ser[0], 0x1e, &val);
	if (val != MAX96705_DEVICE_ID) {
		max96706_read_reg(serdes, 0x04, &val);
		link_status = (val>>7)&0x1;
		if (link_status == 1) {
			max96706_fengjing_initialization(serdes);
			if (serdes->serdes_status.stream_cnt > 0) {
				max96706_output_enable(serdes, 1);
			}
		}
	} else {
		link_status = 1;
	}
	return link_status;
}

void max96706_output_enable(struct serdes_dev_t *serdes, bool enable)
{
	u8 val = 0;

	max96706_read_reg(serdes, 0x04, &val);
	if (enable == 1) {
		val &= ~(0x40);//enable output. bit 6 set 0 to enable
	} else if (enable == 0) {
		val |= 0x40;
	}
	max96706_write_reg(serdes, 0x04, val);
	usleep_range(1000, 1100);
}



#define MAX96706_CHECK_LINK_COUNT	(100)

//addr is for the remapped i2c address
static int max96705_read_reg(struct serdes_dev_t *serdes,
							 u8 addr,
							 u8 reg,
							 u8 *val)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u32 ori_addr;

	ori_addr = client->addr;
	client->addr = addr;
	ret = serdes_read_reg_8(client, reg, val);
	client->addr = ori_addr;
	return ret;
}

/*This is a function for i2c address remapping The original address is 0x40 and needs to be ramapped
  for different channel access from soc*/
/* for avm normally the
   channel 0 0x40 -->0x48
   channel 1 0x40 -->0x49
   channel 2 0x40 -->0x4a
   channel 3 0x40 -->0x4b */
static int max96705_map_i2c_addr(struct serdes_dev_t *serdes, u8 addr)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u16 reg = 0x00;
	u32 ori_addr;

	if (serdes == NULL) {
		printk("no client %s %d", __func__, __LINE__);
		return -ENODEV;
	}
	ori_addr = client->addr;
	client->addr = serdes->addr_ser;
	ret = serdes_write_reg_8(client, reg, addr<<1);
	client->addr = ori_addr;
	return ret;
}


static int max96705_write_reg(struct serdes_dev_t *serdes,
							  u8 addr,
							  u8 reg,
							  u8 val)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u32 ori_addr;

	ori_addr = client->addr;
	client->addr = addr;
	ret = serdes_write_reg_8(client, reg, val);
	client->addr = ori_addr;
	return ret;
}


static void max96706_serdes_power(struct serdes_dev_t *serdes, bool enable)
{
    if (enable) {
        max96706_power(serdes, false);
        usleep_range(10000, 10100);
        max96706_power(serdes, true);
        usleep_range(10000, 10100);
    }
    else {
        max96706_power(serdes, false);
    }
}

static int max96706_fengjing_initialization(struct serdes_dev_t *serdes)
{
    int ret;
    u8 chip_id;
	struct i2c_client *client = serdes->i2c_client;
	int loop = 0;
	u8 val;

    ret = max96706_read_reg(serdes, 0x1e, &chip_id);
	if (chip_id != MAX96706_DEVICE_ID) {
		dev_err(&serdes->i2c_client->dev,
				"%s: wrong chip identifier, expected 0x%x(max96706), got 0x%x\n",
				__func__, MAX96706_DEVICE_ID, chip_id);
		return ENXIO;
	}

	dev_info(&client->dev, "%s()\n", __func__);
	max96706_poc_power(serdes, 0);
	//set him
	max96706_read_reg(serdes, 0x06, &val);
	val |= 0x80;
	max96706_write_reg(serdes, 0x06, val);
	//[7]dbl, [2]hven, [1]cxtp
	max96706_write_reg(serdes, 0x07, 0x86);
	//invert hsync
	max96706_read_reg(serdes, 0x02, &val);
	val |= 0x80;
	max96706_write_reg(serdes, 0x02, val);
	//disable output enable forward/reverse control channel
	max96706_read_reg(serdes, 0x04, &val);
	val |= 0x47;
	max96706_write_reg(serdes, 0x04, val);
	max96706_poc_power(serdes, 1);
	for (loop = 0; loop < MAX96706_CHECK_LINK_COUNT; loop++) {
		max96706_read_reg(serdes, 0x04, &val);
		val = (val>>7)&0x1;
		if (val == 1)
			break;
		usleep_range(1000, 1100);
	}
	//driver current selection
	max96706_read_reg(serdes, 0x5, &val);
	dev_info(&client->dev, "96706, reg=0x5, val=0x%x\n", val);
	val |= 0x40;
	max96706_write_reg(serdes, 0x5, val);
	max96705_map_i2c_addr(serdes, serdes->remap_addr_ser[0]);
	usleep_range(2000, 3000);
	max96705_write_reg(serdes, serdes->remap_addr_ser[0], 0x01,
					   (serdes->i2c_client->addr) << 1);
	usleep_range(5000, 5100);
	if (serdes->gpi_gpio) {
		gpiod_direction_output(serdes->gpi_gpio, 1);
		usleep_range(2000, 2100);
		gpiod_direction_output(serdes->gpi_gpio, 0);
		usleep_range(1000, 1100);
		gpiod_direction_output(serdes->gpi_gpio, 1);
	} else {
		dev_err(&serdes->i2c_client->dev, "%s: no gpi_gpio\n",
				__func__);
		max96705_read_reg(serdes, serdes->remap_addr_ser[0], 0x0f, &val);
		dev_info(&serdes->i2c_client->dev, "%s: val=0x%x\n", __func__,
				 val);
		val |= 0x81;
		max96705_write_reg(serdes, serdes->remap_addr_ser[0], 0x0f, val);
		usleep_range(100, 200);
	}
	//enable dbl, hven
	max96705_write_reg(serdes, serdes->remap_addr_ser[0], 0x07, 0x84);
	return 0;
}

static const int fengjing_formats[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

static int fengjing_enum_mbus_code(struct v4l2_subdev *sd,
								   struct v4l2_subdev_pad_config *cfg,
								   struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;
	if (code->index >= ARRAY_SIZE(fengjing_formats))
		return -EINVAL;
	code->code = fengjing_formats[code->index];
	return 0;
}

serdes_para_t max96706_fengjing_MC163_MAX96705_720p = {
	.module_name = "max96706_fengjing_MC163_MAX96705_720p",
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps	= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.serdes_power = max96706_serdes_power,
	.serdes_start = max96706_output_enable,
	.serdes_init = max96706_fengjing_initialization,
	.serdes_check_link_status = max96706_get_lock_status,
	.serdes_check_link_status_ex = max96706_get_lock_status_ex,
	.serdes_enum_mbus_code = fengjing_enum_mbus_code,
	.delay_time = 200,
};

MODULE_DESCRIPTION("MAX96706 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
