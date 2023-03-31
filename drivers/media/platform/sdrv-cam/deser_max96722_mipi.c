/*
 * Copyright (C) 2021 Semidrive, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>

#include "sdrv-deser.h"
#include "poc-max2008x.h"


#define MAX_CAMERA_NUM 4

#define MAX96712_DEVICE_ID 0xa0
#define MAX96722_DEVICE_ID 0xa1
#define MAX96705_DEVICE_ID 0x41
#define MAX9295A_DEVICE_ID 0x91
#define MAX9295B_DEVICE_ID 0x93

#define MAX96722_SLAVE_ID (0x6b)
#define AP0101_SLAVE_ID (0x5d)
#define MAX20087_SLAVE_ID (0x29)

enum MAX96722_CHANNEL {
	MAX96722_CH_A = 0,
	MAX96722_CH_B = 1,
	MAX96722_CH_C = 2,
	MAX96722_CH_D = 3,
};
enum MAX9295_I2C_ADDR {
	MAX9295_DEF = 0x80,
	MAX9295_CH_A = 0x90,
	MAX9295_CH_B = 0x92,
	MAX9295_CH_C = 0x94,
	MAX9295_CH_D = 0x96,
};

enum MAX96705_I2C_ADDR {
	MAX96705_DEF = 0x80,
	MAX96705_CH_A = 0x90,
	MAX96705_CH_B = 0x92,
	MAX96705_CH_C = 0x94,
	MAX96705_CH_D = 0x96,
};

enum AP0101_I2C_ADDR {
	AP0101_DEF  = 0xAB,
	AP0101_CH_A = 0xA0,
	AP0101_CH_B = 0xA2,
	AP0101_CH_C = 0xA4,
	AP0101_CH_D = 0xA6,
};


enum MAX96705_READ_WRITE {
	MAX96705_READ,
	MAX96705_WRITE,
};

//#define ONE_PORT
#define FRAMESYNC_USE
//#define DEBUG_MAX96722
//#define SYSFS_REG_DEBUG
static reg_param_t max9295_reg[] = {
	{0x2be,  0x10, 0x0},
	{0x0318, 0x5e, 0x0},
	{0x02d3, 0x84, 0x0},
};

static reg_param_t max96722_reg[] = {
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
	{0x040B, 0x80, 0x0}, //0x82
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
	{0x09CE, 0xDE, 0x0},//
	{0x09CF, 0x00, 0x0},//
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

	//Enable mipi
//	{0x040b,0x42, 50},
};

static reg_param_t max96722_reg_max9295[] = {
	// MAX96712 MIPI settings
	{0x0006, 0xf0, 0x0},
	{0x0010, 0x11, 0x0},
	{0x0011, 0x11, 0x0},
	{0x08A0, 0x04, 0x0}, /*PHY 2x4 mode*/
	{0x08A2, 0x30, 0x0}, //enable all the phys
	{0x08A3, 0xE4, 0x0}, //phy1 D0->D2 D1->D3 phy0 D0->D0 D1->D1

	//BPP for pipe lane 0 set as 1E(YUV422)
	{0x040B, 0x80, 0x0}, //pipe0 BPP 0x10: datatype 0x1e csi output disable
	{0x040E, 0x5E, 0x0}, //bit0~5 pipe0:0x1e bit 7:6 pipe1 data type highest 2 bits
	{0x040F, 0x7E, 0x0}, //bit7:4 pipe2 highest 4 bits bit3:0 pipe1 low bits
	{0x0410, 0x7A, 0x0}, //bit7:2 pipe3 6 bits bit1:0 pipe2 low 2 bits
	{0x0411, 0x48, 0x0}, //bit7:5 pipe2 high bits bit4:0 pipe1 BPP 0x10:datatype 0x1e
	{0x0412, 0x20, 0x0}, //bit6:2 pipe3 BPP 0x10:0x1e bit1:0 pipe 2 BPP lower bits

	//lane speed set
	{0x0418, 0xEF, 0x0}, //phy1 lane speed set bit0-4 for lane speed. 0xEF for 1.5G

	//Mapping settings
	{0x090B, 0x07, 0x0},
	{0x092D, 0x15, 0x0},
	{0x090D, 0x1E, 0x0},
	{0x090E, 0x1E, 0x0},
	{0x090F, 0x00, 0x0},
	{0x0910, 0x00, 0x0},
	{0x0911, 0x01, 0x0},
	{0x0912, 0x01, 0x0},

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
	{0x04A0, 0x04, 0x0},//manual frame sync on MPF2
	{0x04A2, 0x00, 0x0},//Turn off auto master link selection
	{0x04AA, 0x00, 0x0},//Disable overlap window
	{0x04AB, 0x00, 0x0},
	{0x04AF, 0xC0, 0x0},//AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0
	{0x04A7, 0x0C, 0x0},//set FSYNC period to 25M/30 CLK cycles. PCLK at 25MHz
	{0x04A6, 0xB7, 0x0},
	{0x04A5, 0x35, 0x0},
	{0x04B1, 0x38, 0x0},//FSYNC TX ID is 7
};
struct max96722_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct max96722_pixfmt max96722_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static int max96722_enum_mbus_code(struct v4l2_subdev *sd,
								   struct v4l2_subdev_pad_config *cfg,
								   struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;
	if (code->index >= ARRAY_SIZE(max96722_formats))
		return -EINVAL;
	code->code = max96722_formats[code->index].code;
	return 0;
}

static int max96722_write_reg(deser_dev_t *dev, u16 reg, u8 val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	client->addr = dev->addr_deser;
	buf[0] = (reg & 0xff00) >> 8;
	buf[1] = (reg & 0x00ff);
	buf[2] = val;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 3;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "%s: error: chip %02x  reg=0x%04x, val=0x%02x\n",
				__func__, dev->addr_deser, reg, val);
		return ret;
	}
	return 0;
}

static int max96722_read_reg(deser_dev_t *dev, u16 reg, u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[3];
	int ret;

	client->addr = dev->addr_deser;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = 0xee;
	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = 2;
	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = &buf[2];
	msg[1].len = 1;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: error: chip %02x  reg=0x%04x, ret=%d\n",
				__func__, dev->addr_deser, reg, ret);
		return ret;
	}
	*val = buf[2];
	return 0;
}

static int max96705_write_reg(deser_dev_t *dev, int channel, u8 reg,
							  u8 val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = dev->addr_serer;
	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr  + channel;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 2;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev,
				"%s: channel %d chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
				__func__, channel, msg.addr, reg, val);
		return ret;
	}
	return 0;
}

static int max96705_read_reg(deser_dev_t *dev, int channel, u8 reg,
							 u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = dev->addr_serer;
	buf[0] = reg;
	msg[0].addr = client->addr + channel;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);
	msg[1].addr = client->addr + channel;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x\n",
				 __func__, channel,  msg[0].addr, reg);
		return ret;
	}
	*val = buf[0];
	return 0;
}

static int max96705_access_sync(deser_dev_t *dev,  int channel, int write,
								u16 reg, u8 *val_r, u8 val_w)
{
	int ret = -1;

	if (channel < 0 || channel > 3) {
		dev_err(&dev->i2c_client->dev,
				"%s: chip max96705 ch=%d, invalid channel.\n", __func__, channel);
		return ret;
	}
	mutex_lock(&dev->serer_lock);
	if (write == MAX96705_WRITE)
		ret = max96705_write_reg(dev, channel, reg, val_w);
	else if (write == MAX96705_READ)
		ret = max96705_read_reg(dev, channel, reg, val_r);
	else
		dev_err(&dev->i2c_client->dev, "%s: chip max96705 Invalid parameter.\n",
				__func__);
	mutex_unlock(&dev->serer_lock);
	return ret;
}

static int max9295_read_reg(deser_dev_t *dev, int channel, u16 reg,
							u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	client->addr = MAX9295_CH_A>>1;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	msg[0].addr = client->addr + channel;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);
	msg[1].addr = client->addr + channel;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x\n",
				 __func__, channel,  msg[0].addr, reg);
		return ret;
	}
	*val = buf[0];
	return 0;
}
static int max9295_write_reg(deser_dev_t *dev, int channel, u16 reg,
							 u8 val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	client->addr = MAX9295_CH_A>>1;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;
	msg.addr = client->addr  + channel;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev,
				"%s: channel %d chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
				__func__, channel, msg.addr, reg, val);
		return ret;
	}
	dev_info(&client->dev,
			 "%s: channel %d chip 0x%02x : reg=0x%02x, val=0x%02x\n",
			 __func__, channel, msg.addr, reg, val);
	return 0;
}
#define MAX_WAIT_COUNT (4)
static int max9295_check_chip_id(deser_dev_t *dev, int channel)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
	int i = 0;

	while (i < MAX_WAIT_COUNT) {
		msleep(20);
		ret = max9295_read_reg(dev, (MAX9295_DEF-MAX9295_CH_A)>>1, 0x0d, &chip_id);
		if (ret < 0) {
			i++;
		} else {
			dev_info(&client->dev, "max9295 dev chipid = 0x%02x\n", chip_id);
			return 0;
		}
	}
	return -ENODEV;
}
#ifndef CONFIG_POWER_POC_DRIVER
static int max20087_write_reg(deser_dev_t *dev, u8 reg,
							  u8 val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = dev->addr_poc;
	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 2;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_info(&client->dev, "%s: chip %x02x error: reg=0x%02x, val=0x%02x\n",
				 __func__, client->addr, reg, val);
		return ret;
	}
	return 0;
}
#endif // CONFIG_POWER_POC_DRIVER

#if 0 // defined_but_no_used
static int max20087_read_reg(deser_dev_t *dev, u8 reg,
							 u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = dev->addr_poc;
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
		dev_err(&client->dev, "%s: chip %02x error: reg=0x%02x\n",  __func__,
				client->addr, reg);
		return ret;
	}
	*val = buf[0];
	return 0;
}
#endif // defined_but_no_used

//Power for Rx max96722/712
static int max96722_power(deser_dev_t *dev, bool enable)
{
	if (dev->pwdn_gpio)
		gpiod_direction_output(dev->pwdn_gpio, enable ? 1 : 0);
	return 0;
}

//Power for camera module
static int max20087_power(deser_dev_t *dev, bool enable)
{
	int ret = -EINVAL;
#ifdef CONFIG_POWER_POC_DRIVER
	struct i2c_client *client = dev->i2c_client;
	u8 reg;

	reg = 0x1;
	//pr_debug("sensor->name = %s\n", client->name);
	if (!strcmp(client->name, SDRV_DESER_NAME_I2C)) {
		ret = poc_mdeser0_power(0xf, enable, reg,  0);
	} else if (!strcmp(client->name, SDRV2_DESER_NAME_I2C)) {
		ret = poc_mdeser1_power(0xf, enable, reg,  0);
#if defined(CONFIG_ARCH_SEMIDRIVE_V9)
	} else if (!strcmp(client->name, SDRV_DESER_NAME_I2C_B)) {
		ret = poc_r_mdeser0_power(0xf, enable, reg,  0);
	} else if (!strcmp(client->name, SDRV2_DESER_NAME_I2C_B)) {
		ret = poc_r_mdeser1_power(0xf, enable, reg,  0);
#endif
	} else {
		dev_err(&client->dev, "Can't support POC %s.\n", client->name);
		return ret;
	}
#else
	if (enable == 1)
		ret = max20087_write_reg(dev, 0x01, 0x1f);
	else
		ret = max20087_write_reg(dev, 0x01, 0x10);
#endif
	return ret;
}

static void max96722_preset(deser_dev_t *dev)
{
	//0x00,0x13,0x75,
	max96722_write_reg(dev, 0x0013, 0x75);
	msleep(20);
}

static void max96722_mipi_enable(deser_dev_t *dev, int en)
{
	//0x04,0x0B,0x42
	if (en == 1)
		max96722_write_reg(dev, 0x040b, 0x42);
	else
		max96722_write_reg(dev, 0x040b, 0x00);
}

static void max96722_link_enable(deser_dev_t *dev, int en)
{
	if (en == 1)
		max96722_write_reg(dev, 0x0006, 0xF);
	else
		max96722_write_reg(dev, 0x0006, 0x00);
}

//Should implement this interface
static int start_deser(deser_dev_t *dev, bool en)
{
	if (en == true) {
		//msleep(20);
		max96722_mipi_enable(dev, 1);
	} else if (en == false) {
		max96722_link_enable(dev, 0);
		usleep_range(2000, 3000);
		max96722_mipi_enable(dev, 0);
	}
	return 0;
}

//Should implement this interface
static int max96705_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
	int i = 0;

	while (i < MAX_WAIT_COUNT) {
		msleep(20);
		ret = max96705_read_reg(dev, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x1E,
								&chip_id);
		if (ret < 0) {
			i++;
			usleep_range(10000, 11000);
			continue;
		} else {
			dev_info(&client->dev, "max96705 dev chipid = 0x%02x\n", chip_id);
			break;
		}
	}
	if (i == MAX_WAIT_COUNT) {
		dev_info(&client->dev,
				 "%s wrong chip identifier,  expected 0x%x(max96705), got %d\n",
				 __func__, MAX96705_DEVICE_ID, chip_id);
	}
	return ret;
}

//Should implement this interface
int max96722_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	ret = max96722_read_reg(dev, 0x000d, &chip_id);
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

void max96722_reg_dump(deser_dev_t *sensor)
{
}


//Should implement this interface
int max96722_initialization(deser_dev_t *dev);


deser_para_t max96722_para = {
	.name = "max96722-712",
	.addr_deser = MAX96722_SLAVE_ID,
	.addr_serer = MAX96705_CH_A >> 1,
	.addr_poc	= MAX20087_SLAVE_ID,
	.addr_gpioext = 0x74,
	.addr_isp	= AP0101_CH_A >> 1,
	.pclk		= 288*1000*1000,
	.mipi_bps	= 1000*1000*1000,
	.width		= 1280,
	.htot		= 1320,
	.height		= 720,
	.vtot		= 740,
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps	= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.denominator = 25,
	.used		= DESER_NOT_USED,	// NOT USED IN DEFAULT
	.reset_gpio = NULL,	//if dts can't config reset and pwdn, use these two members.
	.pwdn_gpio	= NULL,
	.power_deser = max96722_power,
	.power_module = max20087_power,
	.detect_deser = max96722_check_chip_id,
	.init_deser = max96722_initialization,
	.start_deser = start_deser,
	.dump_deser	= max96722_reg_dump,
	.deser_enum_mbus_code = max96722_enum_mbus_code,
};

#ifdef SYSFS_REG_DEBUG

#define MAX_INPUT_ELEM_SZ (10)
#define MAX_CONFIG_BYTES  (256)
typedef struct {
	u8  channel;
	u16 regaddr;
	u8	regval;
} max96722_debug;
max96722_debug t_max96722_debug;

static inline deser_dev_t *to_deser_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, deser_dev_t, sd);
}
static ssize_t des_reg_store(struct device *dev,
							 struct device_attribute *attr, const char *buf, size_t size);
static ssize_t des_reg_show(struct device *dev,
							struct device_attribute *attr, char *buf);
static ssize_t des_reg_write_store(struct device *dev,
								   struct device_attribute *attr, const char *buf, size_t size);
static ssize_t des_reg_write_show(struct device *dev,
								  struct device_attribute *attr, char *buf);
static ssize_t ser_reg_show(struct device *dev,
							struct device_attribute *attr, char *buf);
static ssize_t ser_reg_store(struct device *dev,
							 struct device_attribute *attr, const char *buf, size_t size);
static ssize_t ser_reg_write_store(struct device *dev,
								   struct device_attribute *attr, const char *buf, size_t size);
static ssize_t ser_reg_write_show(struct device *dev,
								  struct device_attribute *attr, char *buf);
static DEVICE_ATTR(reg_dump, 0600,
				   des_reg_show, des_reg_store);
static DEVICE_ATTR(reg_write, 0600,
				   des_reg_write_show, des_reg_write_store);
static DEVICE_ATTR(ser_reg_dump, 0600,
				   ser_reg_show, ser_reg_store);
static DEVICE_ATTR(ser_reg_write, 0600,
				   ser_reg_write_show, ser_reg_write_store);
static int des_parse_input(struct device *dev, const char *buf,
						   size_t buf_size, u16 *ic_buf, size_t ic_buf_size)
{
	const char *pbuf = buf;
	int value;
	char scan_buf[MAX_INPUT_ELEM_SZ];
	int i = 0;
	int j;
	int last = 0;
	int ret;

	dev_info(dev, "%s: pbuf=%p buf=%p size=%d %s=%d buf=%s\n", __func__,
			 pbuf, buf, (int) buf_size, "scan buf size",
			 MAX_INPUT_ELEM_SZ, buf);
	while (pbuf <= (buf + buf_size)) {
		if (i >= MAX_CONFIG_BYTES) {
			dev_err(dev, "%s: %s size=%d max=%d\n", __func__,
					"Max cmd size exceeded", i,
					MAX_CONFIG_BYTES);
			return -EINVAL;
		}
		if (i >= ic_buf_size) {
			dev_err(dev, "%s: %s size=%d buf_size=%zu\n", __func__,
					"Buffer size exceeded", i, ic_buf_size);
			return -EINVAL;
		}
		while (((*pbuf == ' ') || (*pbuf == ','))
			   && (pbuf < (buf + buf_size))) {
			last = *pbuf;
			pbuf++;
		}
		if (pbuf >= (buf + buf_size))
			break;
		memset(scan_buf, 0, MAX_INPUT_ELEM_SZ);
		if ((last == ',') && (*pbuf == ',')) {
			dev_err(dev, "%s: %s \",,\" not allowed.\n", __func__,
					"Invalid data format.");
			return -EINVAL;
		}
		for (j = 0; j < (MAX_INPUT_ELEM_SZ - 1)
			 && (pbuf < (buf + buf_size))
			 && (*pbuf != ' ')
			 && (*pbuf != ','); j++) {
			last = *pbuf;
			scan_buf[j] = *pbuf++;
		}
		dev_info(dev, "%s %s %d\n", __func__, scan_buf, __LINE__);
		ret = kstrtoint(scan_buf, 0, &value);
		if (ret < 0) {
			dev_err(dev, "%s: %s '%s' %s%s i=%d r=%d\n", __func__,
					"Invalid data format. ", scan_buf,
					"Use \"0xHH,...,0xHH\"", " instead.",
					i, ret);
			return ret;
		}
		ic_buf[i] = value;
		dev_info(dev, "%s 0x%x %d\n", __func__, ic_buf[i], __LINE__);
		i++;
	}
	return i;
}
static ssize_t des_reg_store(struct device *dev,
							 struct device_attribute *attr, const char *buf, size_t size)
{
	int rc = 0;
	int value;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}
	t_max96722_debug.regaddr = value;
	return size;
}
static ssize_t des_reg_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	deser_dev_t *sensor = to_deser_dev(sd);
	u8 val = 0;

	max96722_read_reg(sensor, t_max96722_debug.regaddr, &val);
	return sprintf(buf, "reg 0x%x value 0x%x\n", t_max96722_debug.regaddr,
				   val);
}
static ssize_t des_reg_write_store(struct device *dev,
								   struct device_attribute *attr, const char *buf, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	deser_dev_t *sensor = to_deser_dev(sd);
	int length;
	u16 ic_buf[MAX_CONFIG_BYTES];

	length = des_parse_input(dev, buf, size, ic_buf,
							 MAX_CONFIG_BYTES);
	if (length <= 0) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}
	dev_info(dev, "%s buf 0 0x%x buf 1 0x%x %d\n", __func__, ic_buf[0],
			 ic_buf[1], __LINE__);
	t_max96722_debug.regaddr = ic_buf[0];
	t_max96722_debug.regval = ic_buf[1];
	max96722_write_reg(sensor, t_max96722_debug.regaddr,
					   t_max96722_debug.regval);
	return size;
}
static ssize_t des_reg_write_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	deser_dev_t *sensor = to_deser_dev(sd);
	u8 val = 0;

	max96722_read_reg(sensor, t_max96722_debug.regaddr, &val);
	return sprintf(buf, "reg 0x%x value 0x%x\n", t_max96722_debug.regaddr,
				   val);
}
ssize_t ser_reg_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	int length;
	u16 ic_buf[MAX_CONFIG_BYTES];

	length = des_parse_input(dev, buf, size, ic_buf,
							 MAX_CONFIG_BYTES);
	if (length <= 0) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}
	t_max96722_debug.channel = ic_buf[0];
	t_max96722_debug.regaddr = ic_buf[1];
	if (t_max96722_debug.channel > MAX96722_CH_D) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}
	return size;
}
static ssize_t ser_reg_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	deser_dev_t *sensor = to_deser_dev(sd);
	u8 val = 0;

	max9295_read_reg(sensor, t_max96722_debug.channel,
					 t_max96722_debug.regaddr, &val);
	return sprintf(buf, "reg 0x%x value 0x%x\n", t_max96722_debug.regaddr,
				   val);
}
static ssize_t ser_reg_write_store(struct device *dev,
								   struct device_attribute *attr, const char *buf, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	deser_dev_t *sensor = to_deser_dev(sd);
	int length;
	u16 ic_buf[MAX_CONFIG_BYTES];

	length = des_parse_input(dev, buf, size, ic_buf,
							 MAX_CONFIG_BYTES);
	if (length <= 0) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}
	dev_info(dev, "%s buf 0 0x%x buf 1 0x%x %d\n", __func__, ic_buf[0],
			 ic_buf[1], __LINE__);
	t_max96722_debug.channel = ic_buf[0];
	t_max96722_debug.regaddr = ic_buf[1];
	t_max96722_debug.regval = ic_buf[2];
	if (t_max96722_debug.channel > MAX96722_CH_D) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}
	max9295_write_reg(sensor, t_max96722_debug.channel,
					  t_max96722_debug.regaddr, t_max96722_debug.regval);
	return size;
}
static ssize_t ser_reg_write_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	deser_dev_t *sensor = to_deser_dev(sd);
	u8 val = 0;

	max9295_read_reg(sensor, t_max96722_debug.channel,
					 t_max96722_debug.regaddr, &val);
	return sprintf(buf, "reg 0x%x value 0x%x\n", t_max96722_debug.regaddr,
				   val);
}
#endif

int max96722_initialization(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	u8 value;
	int ret;
	int reglen = ARRAY_SIZE(max96722_reg);
	int i = 0;
	int j = 0;
	u8 reg_value;
	u8 err_cnt = 0;

	ret = max96722_read_reg(dev, 0x000d, &value);
	if (ret < 0) {
		dev_err(&client->dev, "max96722 fail to read 0x000d=%d\n", ret);
		return -ENODEV;
	}
	max96722_preset(dev);
	max96722_mipi_enable(dev, 0);

	if (dev->hml == 0){/*no camera traverse for hml test,will revise in sdrv-cam 2.0*/
		max20087_power(dev, 0);
		usleep_range(10000, 11000);
		max20087_power(dev, 1);
		msleep(120);
		max96722_write_reg(dev, 0x0006, 0xff);//enable GMSL2 and all the links
		msleep(50);
		max96722_write_reg(dev, 0x0010, 0x11);//set phyA&B speed to 3G
		max96722_write_reg(dev, 0x0011, 0x11);//set phyC&D speed to 3G
		msleep(50);
	}
        if ((dev->hml == 0)&&(max9295_check_chip_id(dev, 0) == 0)) {
		max96722_para.mipi_bps = 1500000000;
		max96722_para.fps = 30;
		max96722_para.denominator = 30;
		max96722_para.width = 1920;
		max96722_para.height = 1080;
		dev->fmt.width = 1920;
		dev->fmt.height = 1080;
		reg_value = 0xf0;
		reglen = ARRAY_SIZE(max96722_reg_max9295);
		for (i = 0; i < reglen; i++) {
			max96722_write_reg(dev, max96722_reg_max9295[i].nRegAddr,
							   max96722_reg_max9295[i].nRegValue);
			msleep(max96722_reg_max9295[i].nDelay);
		}
		reglen = ARRAY_SIZE(max9295_reg);
		for (i = 0; i < MAX_CAMERA_NUM; i++) {
			value = 0xf0 | (1<<i);
			max96722_write_reg(dev, 0x6, value);
			if (max9295_check_chip_id(dev, i) != 0) {
				dev_err(&client->dev,
						"%s: could not find 9295 on channel %d for 96722\n",
						__func__, i);
				continue;
			} else {
				max9295_write_reg(dev, (MAX9295_DEF-MAX9295_CH_A)>>1, 0x00,
								  MAX9295_CH_A + i * 2);
				reg_value = reg_value|(1<<i);
			}
			for (j = 0; j < reglen; j++) {
				max9295_write_reg(dev, i, max9295_reg[j].nRegAddr,
								  max9295_reg[j].nRegValue);
				msleep(max9295_reg[j].nDelay);
			}
		}
		max96722_write_reg(dev, 0x6, reg_value);
		msleep(50);
#ifdef SYSFS_REG_DEBUG
		ret = device_create_file(&client->dev, &dev_attr_reg_dump);
		if (ret) {
			dev_info(&client->dev,
					 "%s: Error, could not create debug sysfs for 96722\n",
					 __func__);
			return 0;
		}
		ret = device_create_file(&client->dev, &dev_attr_reg_write);
		if (ret) {
			dev_info(&client->dev,
					 "%s: Error, could not create debug sysfs for 96722\n",
					 __func__);
			return 0;
		}
		ret = device_create_file(&client->dev, &dev_attr_ser_reg_dump);
		if (ret) {
			dev_info(&client->dev,
					 "%s: Error, could not create debug sysfs for 96722\n",
					 __func__);
			return 0;
		}
		ret = device_create_file(&client->dev, &dev_attr_ser_reg_write);
		if (ret) {
			dev_info(&client->dev,
					 "%s: Error, could not create debug sysfs for 96722\n",
					 __func__);
			return 0;
		}
#endif
		return 0;
	} else {
		reglen = ARRAY_SIZE(max96722_reg);
		for (i = 0; i < reglen; i++) {
			max96722_write_reg(dev, max96722_reg[i].nRegAddr,
							   max96722_reg[i].nRegValue);
			msleep(max96722_reg[i].nDelay);
		}
		max20087_power(dev, 0);
		usleep_range(10000, 11000);
		max20087_power(dev, 1);
		msleep(120);
		for (i = 0; i < MAX_CAMERA_NUM; i++) {
			max96722_write_reg(dev, 0x0006, 1<<i);
			usleep_range(20000, 21000);
			if (max96705_check_chip_id(dev) != 0) {
				err_cnt++;
				if (err_cnt == MAX_CAMERA_NUM)
					goto no_camera;
				else
					continue;
			}
			max96705_write_reg(dev, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x00,
							   MAX96705_CH_A + i * 2);
			usleep_range(10000, 11000);
			max96705_access_sync(dev, i, MAX96705_WRITE, 0x07, NULL, 0x84);
			usleep_range(1000, 1100);
			max96705_access_sync(dev, i, MAX96705_WRITE, 0x43, NULL, 0x25);
			max96705_access_sync(dev, i, MAX96705_WRITE, 0x45, NULL, 0x01);
			max96705_access_sync(dev, i, MAX96705_WRITE, 0x47, NULL, 0x26);
			usleep_range(10000, 11000);
			max96705_access_sync(dev, i, MAX96705_WRITE, 0x67, NULL, 0xc4);
			max96705_write_reg(dev, i, 0x9, (dev->addr_isp << 1) + i * 2);
			max96705_write_reg(dev, i, 0xa, AP0101_DEF);
		}
		usleep_range(2000, 3000);
		max96722_mipi_enable(dev, 1);
		usleep_range(2000, 3000);
		max96722_link_enable(dev, 1);
		msleep(30);
		max96722_mipi_enable(dev, 0);
		return 0;
	}
no_camera:
	dev_info(&client->dev,
			 "%s: Error, no camera attached\n",
			 __func__);
	return -EIO;
}
