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

#define MAX9296_DEVICE_ID 0x94
#define MAX9296_SLAVE_ID 0x48


#define MAX96705_SLAVE_ID 0x40
#define MAX96705_DEV_INDEX -5
#define MAX96705_DEVICE_ID 0x41

#define MAX20087_SLAVE_ID 0x28
#define MAX20087_SLAVE_ID_2 0x29


#define MAX20086_SLAVE_ID 0x28

enum MAX9296_CHANNEL {
	MAX9296_CH_A = 0,
	MAX9296_CH_B = 1,
};

enum MAX9295_I2C_ADDR {
	MAX9295_DEF = 0x80,
	MAX96705_CH_A = 0x90,
	MAX96705_CH_B = 0x92,
};


enum MAX96705_READ_WRITE {
	MAX96705_READ,
	MAX96705_WRITE,
};


//#define V9TS_CSI

#define DES_REG15_CSIOUT_DISABLE 0x03
#define DES_REG1_FS_DISABLE 0xc3
#define DES_REG1_FS_EN 0x40
#define DES_REG1C_HIM_ENABLE 0xf4
#define DES_REGA_ALLLINK_EN 0xff
#define DES_REG12_YUV8_DBL 0xf3
#define DES_REGC_HVEN 0x91
#define DES_REG69_AUTO_MASK 0x30
#define DES_REG_PERIOD 0x80
#define DES_REG0_INPUTLINK_EN 0xef
#define DES_REG

#define SER_REG4_CFGLINK_EN 0x43
#define SER_REG4_VIDEOLINK_EN 0x83
#define SER_REG7_DBL_HV_EN	0x84
#define SER_REG67_DBLALIGN 0xe7



#if 0 // defined_but_no_used
static reg_param_t max9296_linka_setting[] = {
	//Disable MIPI CSI output
	{0x0007, 0x07, 0x0},
	//Coax mode
	{0x0011, 0x0F, 0x0},
};

static reg_param_t max9296_setting[] = {
	//Disable MIPI CSI output
	{0x0332, 0x00, 0x0},
	//Coax mode
	{0x0011, 0x0F, 0x0},
	//----------Set BPP, VC, DT for X pipe and Y pipe----------------------------------------------//
	{0x031D, 0xEF, 0x0}, //enable override x,y pipe BPP/VC/DT    bit7,bit6
	//DT
	{0x0316, 0x5E, 0x0},
	{0x0317, 0x0E, 0x0},
	//VC
	{0x0314, 0x10, 0x0},
	//BPP
	{0x0313, 0x82, 0x0},
	{0x0319, 0x10, 0x0},
	//Enable YUV422 mux mode
	{0x0322, 0xF0, 0x0},
	//--------------Mapping control-----------------------------------------------------//
	{0x040B, 0x07, 0x0},
	{0x044B, 0x07, 0x0},
	//Map to MIPI B port
	{0x042D, 0x2A, 0x0},
	{0x046D, 0x2A, 0x0},
	{0x040D, 0x1E, 0x0},
	{0x040E, 0x1E, 0x0},
	{0x040F, 0x00, 0x0},
	{0x0410, 0x00, 0x0},
	{0x0411, 0x01, 0x0},
	{0x0412, 0x01, 0x0},
	{0x044D, 0x5E, 0x0},
	{0x044E, 0x5E, 0x0},
	{0x044F, 0x40, 0x0},
	{0x0450, 0x40, 0x0},
	{0x0451, 0x41, 0x0},
	{0x0452, 0x41, 0x0},
	//Set MIPI-CSI B port
	{0x0330, 0x04, 0x0}, //Set 2*4 lane mode
	{0x1558, 0x28, 0x0}, //PHY B optimize
	{0x1559, 0x68, 0x0}, //PHY B optimize
	{0x0334, 0xE4, 0x0}, //D0, D1, D2, D3 map for PHY B (PHY2, PHY3)
	{0x0322, 0x38, 0x0}, //MIPI Lane Rate high 4 bit[0:3], [4:7]=YUV8bits mux setting
	{0x0323, 0x2F, 0x0}, //MIPI Lane Rate low 5 bit[0:4]
	{0x0332, 0xC0, 0x0}, //enable PHY2 and PHY3
};
#endif // defined_but_no_used

struct max9296_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct max9296_pixfmt max9296_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static int max9296_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	if (code->index >= ARRAY_SIZE(max9296_formats))
		return -EINVAL;

	code->code = max9296_formats[code->index].code;

	return 0;
}

static int max9296_write_reg(deser_dev_t *sensor, u16 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	client->addr = sensor->addr_deser;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	//dev_err(&client->dev, "%s: sensor->addr_deser=0x%x\n", __func__, sensor->addr_deser);
	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
			__func__, reg, val);
		return ret;
	}

	return 0;
}


static int max9296_read_reg(deser_dev_t *sensor, u16 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_deser;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;
	//dev_err(&client->dev, "%s: sensor->addr_deser=0x%x\n", __func__, sensor->addr_deser);
	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x\n", __func__, reg);
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

	client->addr = dev->addr_serer;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = client->addr  + channel;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	dev_info(&client->dev, "%s: msg.addr = 0x%x\n", __func__, msg.addr);
	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
			__func__, channel, msg.addr, reg, val);
		return ret;
	}

	return 0;
}

static int max9295_read_reg(deser_dev_t *dev, int channel, u16 reg,
			     u8 *val)
{
	struct i2c_client *client = dev->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	client->addr = dev->addr_serer;
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
	dev_info(&client->dev, "%s: msg[0].addr = 0x%x\n", __func__, msg[0].addr);

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x\n",
				__func__, channel,  msg[0].addr, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
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
		dev_err(&client->dev, "%s: chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
			__func__, client->addr, reg, val);
		return ret;
	}

	return 0;
}
#endif

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
		dev_err(&client->dev, "%s: chip %02x error: reg=0x%02x\n",  __func__, client->addr, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}
#endif // defined_but_no_used

//Power on/off deser
static int max9296_power(deser_dev_t *dev, bool enable)
{
	dev_info(&dev->i2c_client->dev, "%s: enable=%d, dev->pwdn_gpio=%p\n", __func__, enable, dev->pwdn_gpio);
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

	dev_info(&dev->i2c_client->dev, "%s: enable=%d\n", __func__, enable);

	reg = 0x1;
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
	//For different camera subboard max9296's 'POC i2c addr.
	//There is only a POC chip on one i2c bus.
	u8 poc_addr[4] = {0x28, 0x29, 0x2a, 0x2b};
	int i;

	dev_info(&dev->i2c_client->dev, "%s: enable=%d\n", __func__, enable);
	if (enable == 1)
		ret = max20087_write_reg(dev, 0x01, 0x1f);
	else
		ret = max20087_write_reg(dev, 0x01, 0x10);

	//For different POC address
	if (ret < 0) {
		for (i = 0; i < 4; i++) {
			dev->addr_poc = poc_addr[i];
			ret = max20087_write_reg(dev, 0x01, enable?0x1f:0x10);
			if (ret == 0)
				break;
		}
	}
#endif
	return ret;
}

//Should implement this interface
static int start_deser(deser_dev_t *dev, bool en)
{
	dev_info(&dev->i2c_client->dev, "%s: en=%d\n", __func__, en);
	if (en == true) {
		msleep(20);
		max9296_write_reg(dev, 0x15, 0x9b);
	} else if (en == false) {
		max9296_write_reg(dev, 0x15, 0x13);
	}

	return 0;
}

#if 0 // defined_but_no_used
//Should implement this interface
static int max96705_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
	int i = 0;

	dev_info(&dev->i2c_client->dev, "%s\n", __func__);
	for (i = 0; i < 4; i++) {
		//ret = max96705_read_reg(dev, i, 0x1E, &chip_id);
		if (ret < 0) {
			dev_err(&client->dev,
						"%s ch %d wrong chip identifier,  expected 0x%x(max96705), got 0x%x\n",
						__func__, i, MAX96705_DEVICE_ID, chip_id);
			usleep_range(10000, 11000);
			continue;
		} else
			break;
	}

	return ret;
}
#endif // defined_but_no_used

//Should implement this interface
int max9296_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
//	int i = 0;

	dev_info(&dev->i2c_client->dev, "%s\n", __func__);
	ret = max9296_read_reg(dev, 0x0d, &chip_id);

	if (chip_id != MAX9296_DEVICE_ID) {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(max9296) got 0x%x\n",
			__func__, MAX9296_DEVICE_ID, chip_id);
		return -EIO;
	}

	return ret;
}

//Should implement this interface
int max9296_initialization(deser_dev_t *dev)
{
//	int kk;
	u16 reg;
	u8 val;

	dev_info(&dev->i2c_client->dev, "%s()\n", __func__);
	if (dev->pmu_gpio) {
		dev_info(&dev->i2c_client->dev, "%s: dev->pmu_gpio=%p.\n", __func__, dev->pmu_gpio);
		gpiod_direction_output(dev->pmu_gpio, 1);
	}
	msleep(20);

	if (dev->poc_gpio) {
		dev_info(&dev->i2c_client->dev, "%s: dev->poc_gpio=%p.\n", __func__, dev->poc_gpio);
		gpiod_direction_output(dev->poc_gpio, 1);
	}
	msleep(100);


	reg = 0x02be;
	val = 0;
	max9295_read_reg(dev, 0, reg, &val);
	dev_info(&dev->i2c_client->dev, "%s: reg=0x%x, val=0x%x.\n", __func__, reg, val);
	val = 0x10;
	max9295_write_reg(dev, 0, reg, val);	//camera reset

	reg = 0x0057;
	val = 0;
	max9295_read_reg(dev, 0, reg, &val);
	dev_info(&dev->i2c_client->dev, "%s: reg=0x%x, val=0x%x.\n", __func__, reg, val);
	val = 0x12;
	max9295_write_reg(dev, 0, reg, val);

	reg = 0x005b;
	val = 0;
	max9295_read_reg(dev, 0, reg, &val);
	dev_info(&dev->i2c_client->dev, "%s: reg=0x%x, val=0x%x.\n", __func__, reg, val);
	val = 0x11;
	max9295_write_reg(dev, 0, reg, val);

	reg = 0x0318;
	val = 0;
	max9295_read_reg(dev, 0, reg, &val);
	dev_info(&dev->i2c_client->dev, "%s: reg=0x%x, val=0x%x.\n", __func__, reg, val);
	val = 0x5e;
	max9295_write_reg(dev, 0, reg, val);


	reg = 0x0320;
	val = 0;
	max9296_read_reg(dev, reg, &val);
	dev_info(&dev->i2c_client->dev, "%s: reg=0x%x, val=0x%x.\n", __func__, reg, val);
	val = 0x28;
	max9296_write_reg(dev, reg, val);

	reg = 0x0313;
	val = 0;
	max9296_read_reg(dev, reg, &val);
	dev_info(&dev->i2c_client->dev, "%s: reg=0x%x, val=0x%x.\n", __func__, reg, val);
	val = 0x02;
	max9296_write_reg(dev, reg, val);

	return 0;
}


deser_para_t max9296_para = {
	.name = "max9296",
	.addr_deser = MAX9296_SLAVE_ID,
	.addr_serer = MAX9295_DEF>>1,
	.addr_poc	= MAX20087_SLAVE_ID,
	.pclk		= 288*1000*1000,
	.mipi_bps	= 576*1000*1000,
	.width		= 1920,
	.htot		= 1940,
	.height		= 1080,
	.vtot		= 1100,
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps		= 30,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.denominator = 30,
	.used		= DESER_NOT_USED, // use two max9296 desers on one camera subboard. If you don't have this satuation, please set DESER_NOT_USED
	.reset_gpio = NULL,	//if dts can't config reset and pwdn,
	.pwdn_gpio	= NULL, //use these two members.
	.power_deser = max9296_power,
	.power_module = max20087_power,
	.detect_deser = max9296_check_chip_id,
	.init_deser = max9296_initialization,
	.start_deser = start_deser,
	.dump_deser	= NULL,
	.deser_enum_mbus_code = max9296_enum_mbus_code,
};




