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

#include "poc-max2008x.h"
#include "sdrv_serdes_core.h"

#define MAX96712_DEVICE_ID 0xa0
#define MAX96722_DEVICE_ID 0xa1
#define MAX96705_DEVICE_ID 0x41
#define MAX9295A_DEVICE_ID 0x91
#define MAX9295B_DEVICE_ID 0x93

static int max96722_initialization_fengjing(struct serdes_dev_t *serdes);
//static int max96705_check_chip_id(struct serdes_dev_t *serdes, u8 address);
static int max96722_write_reg(struct serdes_dev_t *serdes, u16 reg, u8 val)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u32 addr;

	addr = client->addr;
	client->addr = serdes->addr_des;
	ret = serdes_write_reg_16(client, reg, val);
	client->addr = addr;//set client addr back to the virtual addr set in dts
	return ret;
}

static int max96722_read_reg(struct serdes_dev_t *serdes, u16 reg,
							 u8 *val)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u32 addr;

	addr = client->addr;
	client->addr = serdes->addr_des;
	ret = serdes_read_reg_16(client, reg, val);
	client->addr = addr;//set client addr back to the virtual addr set in dts
	return ret;
}

//Power for Rx max96722/712
static void max96722_power(struct serdes_dev_t *serdes, bool enable)
{
	gpiod_direction_output(serdes->pwdn_gpio, enable ? 1 : 0);
	usleep_range(20000, 21000);
}


//Power for camera module
static int max20087_power(struct serdes_dev_t *serdes, bool enable)
{
	int ret;

	ret = poc_power(serdes->i2c_client, serdes->poc_gpio, serdes->addr_poc,
					0xf, enable);
	return ret;
}

#if 0
static int max96722_check_chip_id(struct serdes_dev_t *serdes)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	max96722_power(serdes, 1);
	usleep_range(20000, 21000);
	ret = max96722_read_reg(serdes, 0x000d, &chip_id);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to read max96722 chipid.\n",
				__func__);
		return ret;
	}
	if (chip_id == MAX96722_DEVICE_ID || chip_id == MAX96712_DEVICE_ID) {
		dev_info(&client->dev, "max96722/12 chipid = 0x%02x\n", chip_id);
	} else {
		dev_err(&client->dev,
				"%s: wrong chip identifier, expected 0x%x(max96722), 0x%x(max96712) got 0x%x\n",
				__func__, MAX96722_DEVICE_ID, MAX96712_DEVICE_ID, chip_id);
		return -EIO;
	}
	return ret;
}
#endif

//enable mipi output to soc
static void max96722_mipi_enable(struct serdes_dev_t *serdes, bool en)
{
	//0x04,0x0B,0x42
	if (en == 1){
		max96722_write_reg(serdes, 0x08A0, 0x04);
		max96722_write_reg(serdes, 0x040b, 0x40);
		msleep(30);
		max96722_write_reg(serdes, 0x08A0, 0x84);
		max96722_write_reg(serdes, 0x040b, 0x42);
	}
	else{
		max96722_write_reg(serdes, 0x08A0, 0x04);
		max96722_write_reg(serdes, 0x040b, 0x40);
	}
}

static int max96722_gmsl1_get_link_lock(struct serdes_dev_t *serdes)
{
	int i = 0;
	u8 value = 0;
	u8 lock_status = 0;

	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		max96722_read_reg(serdes, 0xbcb + i * 0x100, &value);
		value = value & 0x1;
		lock_status |= (value << i);
	}
	return lock_status&serdes->serdes_streamon_bitmap;
}

static bool max96722_get_video_lock(struct serdes_dev_t *serdes)
{
	int i = 0;
	u8 value = 0;
	u8 video_lock_status = 0;

	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		max96722_read_reg(serdes, 0x1dc+i*0x20, &value);
		value = value & 0x1;
		video_lock_status |= (value << i);
	}
	if ((video_lock_status&serdes->serdes_streamon_bitmap) !=
		serdes->current_channel_lock_status)
		return true;
	if (video_lock_status == 0 && (serdes->serdes_streamon_bitmap != 0)) {
		max96722_read_reg(serdes, 0x6, &value);
		value = value&0xf;
		if (value ==
			0) { //this is a corner case that the stream on is not zero but all the links are disabled,probably a plug out happened during a reinit. We need a reinit here
			return true;
		}
	}
	return false;
}

//for both gmsl1 and gmsl2
//0x1dc 0x1fc 0x21c 0x23c 0x25c 0x27c 0x29c 0x2bc bit0 video lock
static int max96722_video_channel_lock(struct serdes_dev_t *serdes, int channel)
{
	u8 value;
	u8 video_lock_status = 0;
	int i = 0;

	if (serdes->sync == 1) {
		for (i = 0; i < MAX_SENSOR_NUM; i++) {
			max96722_read_reg(serdes, 0x1dc+i*0x20, &value);
			value = value & 0x1;
			video_lock_status |= (value << i);
		}
	} else {
		max96722_read_reg(serdes, 0x1dc+channel*0x20, &value);
		video_lock_status = value & 0x1;
	}
	//dev_info(&serdes->i2c_client->dev, "addr 0x%x value 0x%x\n", 0x1dc+channel*0x20,value);
	return video_lock_status;
}

//check if video lock is on,if not,reinit and then check the link status
static int max96722_gmsl1_get_link_lock_ex(struct serdes_dev_t *serdes, int channel)
{
	int i = 0;
	u8 value = 0;
	u8 lock_status = 0;
	u8 video_lock_status = 0;

	video_lock_status = max96722_video_channel_lock(serdes, channel);
	if (serdes->sync == 1) {
		if (video_lock_status != 0xf) { //reinit when video link lock is lost
			//do reinit when the lock status is not zero
			max96722_initialization_fengjing(serdes);
			if (serdes->serdes_status.stream_cnt > 0) {
				max96722_mipi_enable(serdes, 1);
			}
			msleep(50);//wait link locked
			//check link status after  reinit
			for (i = 0; i < MAX_SENSOR_NUM; i++) {
				max96722_read_reg(serdes, 0xbcb + i * 0x100, &value);
				value = value & 0x1;
				lock_status |= (value << i);
			}
		} else {
			lock_status = 0xf;
		}
	} else {
		if (video_lock_status == 0) { //reinit when video link lock is lost
			max96722_initialization_fengjing(serdes);
			if (serdes->serdes_status.stream_cnt > 0) {
				max96722_mipi_enable(serdes, 1);
			}
			msleep(50);//wait link locked
			//check link status after  reinit
			max96722_read_reg(serdes, 0xbcb+channel*0x100, &value);
			lock_status = value&0x1;
		} else {
			lock_status = 1;
		}
	}
	return lock_status;
}

//0xbcb 0xccb 0xdcb 0xecb
static int max96722_gmsl1_link_lock(struct serdes_dev_t *serdes, int channel)
{
	u8 value = 0;

	max96722_read_reg(serdes, 0xbcb+channel*0x100, &value);
	//dev_info(&serdes->i2c_client->dev,"value 0x%x",value);
	return value&0x1;
}

#define FRAMESYNC_USE

static serdes_param_reg16_t max96722_reg[] = {
	// Begin of Script
	// MAX96712 Link Initialization to pair with GMSL1 Serializers
	{0x0006, 0x00, 0x0}, // Disable all links

	//Tx/Rx rate Selection
	{0x0010, 0x11, 0x00},// 3Gbps

	// Power up sequence for GMSL1 HIM capable serializers; Also apply to Ser with power up status unknown
	// Turn on HIM on MAX96712
	{0x0C06, 0xEF, 0x0},

#ifndef ONE_PORT
	{0x0B06, 0xEF, 0x0},
	//{0x0C06,0xEF, 0x0},
	{0x0D06, 0xEF, 0x0},
	{0x0E06, 0xEF, 0x0},
#endif
	// disable HS/VS processing
	{0x0C0F, 0x01, 0x0},
#ifndef ONE_PORT
	{0x0B0F, 0x01, 0x0},
	{0x0D0F, 0x01, 0x0},
	{0x0E0F, 0x01, 0x0},
#endif
	{0x0C07, 0x84, 0xa},

#ifndef ONE_PORT
	{0x0B07, 0x84, 0x0},
	{0x0D07, 0x84, 0x0},
	{0x0E07, 0x84, 0x0},
#endif

	// YUV MUX mode
	{0x041A, 0xF0, 0x0},

	//Set i2c path
	//{0x0B04,0x03, 0x0},
	//{0x0B0D,0x81, 0x0},// 设置96706的I2C通讯

	// MAX96712 MIPI settings
	{0x08A0, 0x04, 0x0},
	{0x08A2, 0x30, 0x0},
	{0x00F4, 0x01, 0x0}, //enable 4 pipeline 0xF, enable pipeline 0 0x1
	{0x094A, 0xc0, 0x0},  // Mipi data lane count b6-7 1/2/4lane
	{0x08A3, 0xE4, 0x0}, //0x44 4 lanes data output, 0x4E lane2/3 data output. 0xE4 lane0/1 data output.

	//BPP for pipe lane 0 set as 1E(YUV422)
	{0x040B, 0x80, 0x0},
	{0x040C, 0x00, 0x0},
	{0x040D, 0x00, 0x0},
	{0x040E, 0x5E, 0x0},
	{0x040F, 0x7E, 0x0},
	{0x0410, 0x7A, 0x0},
	{0x0411, 0x48, 0x0},
	{0x0412, 0x20, 0x0},

	//lane speed set
	{0x0415, 0xEA, 0x0}, //phy0 lane speed set 0xEF for 1.5g
	{0x0418, 0xEA, 0x0}, //phy1 lane speed set bit0-4 for lane speed. 10001 0xf1 for 1.5g


	//Mapping settings
	{0x090B, 0x07, 0x0},
	{0x092D, 0x15, 0x0},
	{0x090D, 0x1E, 0x0},
	{0x090E, 0x1E, 0x0},
	{0x090F, 0x00, 0x0},
	{0x0910, 0x00, 0x0}, //frame sync frame start map
	{0x0911, 0x01, 0x0},
	{0x0912, 0x01, 0x0},

	//{0x0903,0x80, 0x0},//bigger than 1.5Gbps， Deskew can works.

	{0x094B, 0x07, 0x0},
	{0x096D, 0x15, 0x0},
	{0x094D, 0x1E, 0x0},
	{0x094E, 0x5E, 0x0},
	{0x094F, 0x00, 0x0},
	{0x0950, 0x40, 0x0},
	{0x0951, 0x01, 0x0},
	{0x0952, 0x41, 0x0},

	{0x098B, 0x07, 0x0},
	{0x09AD, 0x15, 0x0},
	{0x098D, 0x1E, 0x0},
	{0x098E, 0x9E, 0x0},
	{0x098F, 0x00, 0x0},
	{0x0990, 0x80, 0x0},
	{0x0991, 0x01, 0x0},
	{0x0992, 0x81, 0x0},

	{0x09CB, 0x07, 0x0},
	{0x09ED, 0x15, 0x0},
	{0x09CD, 0x1E, 0x0},
	{0x09CE, 0xDE, 0x0},
	{0x09CF, 0x00, 0x0},
	{0x09D0, 0xC0, 0x0},
	{0x09D1, 0x01, 0x0},
	{0x09D2, 0xC1, 0x0},


#ifdef FRAMESYNC_USE
	//-------------- Frame Sync --------------/
	// set FSYNC period to 25M/30 CLK cycles. PCLK at 25MHz

#ifdef ONE_PORT
	{0x04A2, 0x20, 0x0},
#else
	{0x04A2, 0x00, 0x0},
#endif

	{0x04A7, 0x0F, 0x0},
	{0x04A6, 0x42, 0x0},
	{0x04A5, 0x40, 0x0},

	{0x04AA, 0x00, 0x0},
	{0x04AB, 0x00, 0x0},


	// AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0? GMSL1
#ifdef ONE_PORT
	{0x04AF, 0x42, 0x0},
#else
	{0x04AF, 0x4F, 0x0},
#endif
	{0x04A0, 0x00, 0x0},


	{0x0B08, 0x10, 0x0},
	{0x0C08, 0x10, 0x0},
	{0x0D08, 0x10, 0x0},
	{0x0E08, 0x10, 0x0},
#endif

	//For debug
	{0x0001, 0xcc, 0x0},
	{0x00FA, 0x10, 0x0},
};

static serdes_param_reg8_t max96705_reg[] = {
	{0x07, 0x84, 1000},
	{0x43, 0x25, 0},
	{0x45, 0x01, 0},
	{0x47, 0x26, 1000},
	{0x67, 0xc4, 2000},
};

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

#if 0
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

static int max96705_check_chip_id(struct serdes_dev_t *serdes, u8 address)
{
	int ret = 0;
	u8 chip_id = 0;
	u8 reg = 0x1E;

	ret = max96705_read_reg(serdes, address, reg, &chip_id);
	if (ret < 0) {
		dev_err(&serdes->i2c_client->dev,
				"%s:chip 0x%02x error: reg=0x%02x ret %d\n", __func__,
				address, reg, ret);
		return ret;
	}
	if (chip_id != MAX96705_DEVICE_ID) {
		dev_err(&serdes->i2c_client->dev,
				"max96705 dev chipid = 0x%02x expected 0x%x\n",
				chip_id, MAX96705_DEVICE_ID);
		return -ENODEV;
	}
	return 0;
}
#endif


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

static int max96722_max96705_init_des(struct serdes_dev_t *serdes)
{
	int reglen = 0;
	int i = 0;

	reglen = ARRAY_SIZE(max96722_reg);
	for (i = 0; i < reglen; i++) {
		max96722_write_reg(serdes, max96722_reg[i].nRegAddr,
						   max96722_reg[i].nRegValue);
	}
	return 0;
}

static int max96722_max96705_init_ser(struct serdes_dev_t *serdes)
{
	//Camera Module Power On/Off
	u8 value;
	int i = 0;
	int j = 0;
	int reglen = 0;
	int lock = 0;
	struct i2c_client *client = serdes->i2c_client;

	reglen = ARRAY_SIZE(max96705_reg);
	max20087_power(serdes, 0);
	usleep_range(10000, 11000);
	max20087_power(serdes, 1);
	msleep(120);
	max96722_write_reg(serdes, 0x6, 0x0f);
	usleep_range(10000, 11000);//wait link to be stable
	serdes->ser_online_bitmap = 0;
	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		if (((serdes->ser_bitmap>>i) & 0x1) == 1) {
			lock = max96722_gmsl1_link_lock(serdes, i);
			if (lock == 0) {
				dev_info(&client->dev, "channel %d not locked\n", i);
				continue;
			}
			max96722_read_reg(serdes, 0xb04+i*0x100,&value);
			max96722_write_reg(serdes, 0xb04+i*0x100, value|(1<<1));//open current reverse channel
			usleep_range(100,110);
			//close other reverse channel
			for (j = 0; j < MAX_SENSOR_NUM; j++){
				if (j != i){
					max96722_read_reg(serdes, 0xb04+j*0x100,&value);
					max96722_write_reg(serdes, 0xb04+j*0x100, value&(0xfd));
					usleep_range(100,110);
				}
			}
			max96705_map_i2c_addr(serdes, serdes->remap_addr_ser[i]);
			serdes->ser_online_bitmap |= (1<<i);
			usleep_range(10000, 11000);
			for (j = 0; j < reglen; j++) {
				max96705_write_reg(serdes, serdes->remap_addr_ser[i],
								   max96705_reg[j].nRegAddr,
								   max96705_reg[j].nRegValue);
				if (max96705_reg[j].nDelay > 0)
					usleep_range(max96705_reg[j].nDelay, max96705_reg[j].nDelay+100);
			}
		}
	}
	dev_info(&client->dev, "online bitmap 0x%x\n", serdes->ser_online_bitmap);
	for (i = 0; i < MAX_SENSOR_NUM; i++){//open all the reverse channel
		max96722_read_reg(serdes, 0xb04+i*0x100,&value);
		max96722_write_reg(serdes, 0xb04+i*0x100, value|(1<<1));
		usleep_range(100,110);
	}
	usleep_range(30000, 31000);//wait to get video link stable
	return 0;
}

static void max96722_serdes_power(struct serdes_dev_t *serdes, bool enable)
{
    if (enable) {
        max96722_power(serdes, false);
        usleep_range(10000, 10100);
        max96722_power(serdes, true);
        usleep_range(10000, 10100);
    }
    else {
        max96722_power(serdes, false);
    }
}

static int max96722_initialization_fengjing(struct serdes_dev_t *serdes)
{
    int ret;
    u8 chip_id = 0;

    ret = max96722_read_reg(serdes, 0x000d, &chip_id);
	if (chip_id == MAX96722_DEVICE_ID || chip_id == MAX96712_DEVICE_ID) {
		dev_info(&serdes->i2c_client->dev, "max96722/12 chipid = 0x%02x\n", chip_id);
	} else {
		dev_err(&serdes->i2c_client->dev,
				"%s: wrong chip identifier, expected 0x%x(max96722), 0x%x(max96712) got 0x%x\n",
				__func__, MAX96722_DEVICE_ID, MAX96712_DEVICE_ID, chip_id);
		return -EIO;
	}

	max96722_max96705_init_des(serdes);
	max96722_max96705_init_ser(serdes);
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

serdes_para_t max96722_fengjing_MC163_MAX96705_720p = {
	.module_name = "max96722_fengjing_MC163_MAX96705_720p",
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps	= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.serdes_power = max96722_serdes_power,
	.serdes_init = max96722_initialization_fengjing,
	.serdes_start = max96722_mipi_enable,
	.serdes_check_link_status = max96722_gmsl1_get_link_lock,
	.serdes_check_link_status_ex = max96722_gmsl1_get_link_lock_ex,
	.serdes_check_video_lock_status = max96722_get_video_lock,
	.serdes_enum_mbus_code = fengjing_enum_mbus_code,
	.delay_time = 2000,
};

MODULE_DESCRIPTION("MAX96722 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
