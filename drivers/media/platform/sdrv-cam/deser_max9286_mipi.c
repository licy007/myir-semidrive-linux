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

#define MAX9286_DEVICE_ID 0x40
#define MAX9286_SLAVE_ID 0x2c


#define MAX96705_SLAVE_ID 0x40
#define MAX96705_DEV_INDEX -5
#define MAX96705_DEVICE_ID 0x41

#define MAX20087_SLAVE_ID 0x28
#define MAX20087_SLAVE_ID_2 0x29



#define AP0101_SLAVE_ID 0x5d
#define AP0101_DEV_INDEX -4

#define MAX20086_SLAVE_ID 0x28

enum MAX9286_CHANNEL {
	MAX9286_CH_A = 0,
	MAX9286_CH_B = 1,
	MAX9286_CH_C = 2,
	MAX9286_CH_D = 3,
};

enum MAX96705_I2C_ADDR {
	MAX96705_DEF = 0x80,
	MAX96705_CH_A = 0x90,
	MAX96705_CH_B = 0x92,
	MAX96705_CH_C = 0x94,
	MAX96705_CH_D = 0x96,
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


struct max9286_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct max9286_pixfmt max9286_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};


reg_param_t max9286_reg[] = {
};

static int max9286_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	if (code->index >= ARRAY_SIZE(max9286_formats))
		return -EINVAL;

	code->code = max9286_formats[code->index].code;

	return 0;
}

static int max9286_write_reg(deser_dev_t *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_deser;
	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
			__func__, reg, val);
		return ret;
	}

	return 0;
}

static int max9286_read_reg(deser_dev_t *sensor, u8 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_deser;
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
		dev_err(&client->dev, "%s: error: reg=0x%x\n", __func__, reg);
		return ret;
	}

	*val = buf[0];
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
		dev_err(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
			__func__, channel, msg.addr, reg, val);
		return ret;
	}

	return 0;
}

#if 0 // defined_but_no_used
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
		dev_err(&client->dev, "%s: channel %d chip 0x%02x error: reg=0x%02x\n",
				__func__, channel,  msg[0].addr, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}

static int max96705_access_sync(deser_dev_t *dev,  int channel, int write, u16 reg, u8 *val_r, u8 val_w)
{
	int ret = -1;

	if (channel < 0 || channel > 3) {
		dev_err(&dev->i2c_client->dev, "%s: chip max96705 ch=%d, invalid channel.\n", __FUNCTION__, channel);
		return ret;
	}

	mutex_lock(&dev->serer_lock);

	if (write == MAX96705_WRITE)
		ret = max96705_write_reg(dev, channel, reg, val_w);
	else if (write == MAX96705_READ)
		ret = max96705_read_reg(dev, channel, reg, val_r);
	else
		dev_err(&dev->i2c_client->dev, "%s: chip max96705 Invalid parameter.\n", __FUNCTION__);

	mutex_unlock(&dev->serer_lock);
	return ret;
}

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
static int max9286_power(deser_dev_t *dev, bool enable)
{
	dev_info(&dev->i2c_client->dev, "%s: enable=%d\n", __func__, enable);
	if (dev->pwdn_gpio)
		gpiod_direction_output(dev->pwdn_gpio, enable ? 1 : 0);
	return 0;
}

//Power for camera module
static int max20087_power(deser_dev_t *dev, bool enable)
{
	int ret = -EINVAL;
#ifdef CONFIG_POWER_POC_DRIVER
	struct i2c_client *client;
	u8 reg;
#endif

	dev_info(&dev->i2c_client->dev, "%s: enable=%d\n", __func__, enable);

	if (dev->poc_gpio){
		dev_err(&dev->i2c_client->dev, "%s: dev->poc_gpio=%p.\n", __func__, dev->poc_gpio);
		gpiod_direction_output(dev->poc_gpio, 1);
		msleep(20);
	}

#ifdef CONFIG_POWER_POC_DRIVER
	client = dev->i2c_client;
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
	//For different camera subboard max9286's 'POC i2c addr.
	//There is only a POC chip on one i2c bus.
	u8 poc_addr[4] = {0x28, 0x29, 0x2a, 0x2b};
	int i;
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
	if (en == true) {
		msleep(20);
		max9286_write_reg(dev, 0x15, 0x9b);
	} else if (en == false) {
		max9286_write_reg(dev, 0x15, 0x13);
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

	for (i = 0; i < 4; i++) {
		ret = max96705_read_reg(dev, i, 0x1E, &chip_id);
		if (ret < 0) {
			dev_err(&client->dev,
						"%s ch %d wrong chip identifier,  expected 0x%x(max96705), got 0x%x\n",
						__func__, i, MAX96705_DEVICE_ID, chip_id);
			usleep_range(10000, 11000);
			continue;
		} else{
			dev_info(&client->dev, "max96705 dev chipid = 0x%02x\n", chip_id);
			break;
		}
	}

	return ret;
}
#endif // defined_but_no_used

//Should implement this interface
int max9286_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
//	int i = 0;

	ret = max9286_read_reg(dev, 0x1e, &chip_id);
	dev_info(&client->dev, "%s: chip_id=0x%x\n", __func__, chip_id);
	if (chip_id != MAX9286_DEVICE_ID) {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(max9286) got 0x%x\n",
			__func__, MAX9286_DEVICE_ID, chip_id);
		return -EIO;
	}

	return ret;
}

//Should implement this interface
int max9286_initialization(deser_dev_t *dev)
{
	deser_dev_t *sensor = dev;
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 reg, val;
//	u8 link_status = 0, link_count = 0;
	int i = 0, j;

	dev_info(&client->dev, "%s: sensor->addr_deser=0x%x\n", __func__, sensor->addr_deser);
	if (sensor->pmu_gpio){
		dev_err(&sensor->i2c_client->dev, "%s: sensor->pmu_gpio=%p.\n", __func__, sensor->pmu_gpio);
		gpiod_direction_output(sensor->pmu_gpio, 1);
		msleep(20);
	}

	//csienable=0
	ret = max9286_write_reg(sensor, 0x15, DES_REG15_CSIOUT_DISABLE);
	if (ret) {
		dev_err(&client->dev, "max9286 write fail!\n");
		return -ENODEV;
	}
	//disable fs
	max9286_write_reg(sensor, 0x1, DES_REG1_FS_DISABLE);
#ifndef V9TS_CSI
	dev_info(&client->dev, "set him\n");
	//him enable
	max9286_write_reg(sensor, 0x1c, DES_REG1C_HIM_ENABLE);

	max20087_power(sensor, false);
	usleep_range(500, 600);
	max20087_power(sensor, true);
#endif
	for (i = 0; i < 120; i++) {

		val = 0;
		max9286_read_reg(sensor, 0x49, &val);

		if ((val & 0x0f) == 0x0f)
			break;

		usleep_range(1000, 1100);
	}

	dev_info(&client->dev, "0x49=0x%x, i=%d\n", val, i);

	if (val == 0) {

		dev_err(&client->dev, "max96705 detect fail!\n");
		return -ENODEV;
	}


	//config link
	max96705_write_reg(sensor, 0, 0x04, SER_REG4_CFGLINK_EN);
	usleep_range(5000, 5100);

	//dbl, hven
	max96705_write_reg(sensor, 0, 0x07, SER_REG7_DBL_HV_EN);

	//dbl align
	max96705_write_reg(sensor, 0, 0x67, SER_REG67_DBLALIGN);

	for (j = 0; j < 50; j++) {

		max9286_read_reg(sensor,0x49, &val);
		if ((val & 0xf0) == 0xf0)
			break;

		usleep_range(100, 200);
	}

	dev_info(&client->dev, "reg=0x%x, val=0x%x, j=%d\n", reg, val, j);

	for (i = 0; i < MAX_CAMERA_NUM; i++) {

		max9286_write_reg(sensor, 0x0A, 0x01<<i);
		for (j = 0; j < 50; j++) {

			max9286_read_reg(sensor, 0x49, &val);

			if ((val & 0x10<<i))
				break;

			usleep_range(100, 200);
		}
		dev_info(&client->dev, "reg=0x%x, val=0x%x, [0x%x], i=%d, j=%d\n", reg, val,
			(MAX96705_SLAVE_ID + MAX96705_DEV_INDEX + sensor->sec_9286_shift + i), i, j);
		max96705_write_reg(sensor, 0, 0x00,
				   (MAX96705_SLAVE_ID + MAX96705_DEV_INDEX +
					sensor->sec_9286_shift + i) << 1);

		usleep_range(900, 1000);
	}
	//Enable all Link control channel
	max9286_write_reg(sensor, 0x0A, DES_REGA_ALLLINK_EN);

	//yuv10 to yuv8
	//enable dbl
	max9286_write_reg(sensor, 0x12, DES_REG12_YUV8_DBL);

	//polarity
	max9286_write_reg(sensor, 0x0c, DES_REGC_HVEN);

	//63, 64 for csi data output
	max9286_write_reg(sensor, 0x63, 0x0);
	max9286_write_reg(sensor, 0x64, 0x0);
	//auto mask
	max9286_write_reg(sensor, 0x69, DES_REG69_AUTO_MASK);

	//period
	max9286_write_reg(sensor, 0x19, DES_REG_PERIOD);

	val = 0;
	for (i = 0; i < MAX_CAMERA_NUM; i++) {
		//vs delay
		max96705_write_reg(sensor,
					   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
					   i, 0x43, 0x25);
		max96705_write_reg(sensor,
					   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
					   i, 0x45, 0x01);
		if (sensor->device_type == SDRV1_ICL02) {
			max96705_write_reg(sensor,
					   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
					   i, 0x47, 0x29);
		} else {
			max96705_write_reg(sensor,
					   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
					   i, 0x47, 0x26);
		}

		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x20, 0x07);

		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x21, 0x06);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x22, 0x05);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x23, 0x04);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x24, 0x03);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x25, 0x02);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x26, 0x01);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x27, 0x00);

		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x30, 0x17);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x31, 0x16);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x32, 0x15);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x33, 0x14);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x34, 0x13);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x35, 0x12);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x36, 0x11);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x37, 0x10);

		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x9,
				   (AP0101_SLAVE_ID + AP0101_DEV_INDEX +
					sensor->sec_9286_shift + i) << 1);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0xa, AP0101_SLAVE_ID << 1);

		usleep_range(500, 600);
	}

	max9286_write_reg(sensor, 0x0, DES_REG0_INPUTLINK_EN);

	//fs, manual pclk
	if (sensor->device_type == SDRV1_ICL02) {
		max9286_write_reg(sensor, 0x06, 0xa0);
		max9286_write_reg(sensor, 0x07, 0x5b);
		max9286_write_reg(sensor, 0x08, 0x32);
	} else {
		max9286_write_reg(sensor, 0x06, 0x00);
		max9286_write_reg(sensor, 0x07, 0xf2);
		max9286_write_reg(sensor, 0x08, 0x2b);
	}

	for (i = 0; i < MAX_CAMERA_NUM; i++) {
		//enable channel
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x04, 0x83);
	}
	//internal fs, debug for output
	max9286_write_reg(sensor, 0x1, DES_REG1_FS_EN);
	max9286_write_reg(dev, 0x15, 0x9b);
	max9286_write_reg(dev, 0x15, 0x13);

	return 0;
}


deser_para_t max9286_para = {
	.name = "max9286",
	.addr_deser = MAX9286_SLAVE_ID,
	.addr_serer = MAX96705_DEF>>1,
	.addr_poc	= MAX20087_SLAVE_ID,
	.pclk		= 288*1000*1000,
	.mipi_bps	= 576*1000*1000,
	.width		= 1280,
	.htot		= 1320,
	.height		= 720,
	.vtot		= 740,
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps		= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.denominator = 25,
	.used		= DESER_NOT_USED, // use two max9286 desers on one camera subboard. If you don't have this satuation, please set DESER_NOT_USED
	.reset_gpio = NULL,	//if dts can't config reset and pwdn,
	.pwdn_gpio	= NULL, //use these two members.
	.power_deser = max9286_power,
	.power_module = max20087_power,
	.detect_deser = max9286_check_chip_id,
	.init_deser = max9286_initialization,
	.start_deser = start_deser,
	.dump_deser	= NULL,
	.deser_enum_mbus_code = max9286_enum_mbus_code,
};

deser_para_t max9286_para_2 = {
	.name = "max9286_2",
	.addr_deser = MAX9286_SLAVE_ID,
	.addr_serer = MAX96705_DEF>>1,
	.addr_poc	= MAX20087_SLAVE_ID_2,
	.pclk		= 288*1000*1000,
	.mipi_bps	= 576*1000*1000,
	.width		= 1280,
	.htot		= 1320,
	.height		= 720,
	.vtot		= 740,
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps		= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.denominator = 25,
	.used		= DESER_NOT_USED, // use two max9286 desers on one camera subboard. If you don't have this satuation, please set DESER_NOT_USED
	.reset_gpio = NULL,	//if dts can't config reset and pwdn,
	.pwdn_gpio	= NULL, //use these two members.
	.power_deser = max9286_power,
	.power_module = max20087_power,
	.detect_deser = max9286_check_chip_id,
	.init_deser = max9286_initialization,
	.start_deser = start_deser,
	.dump_deser	= NULL,
	.deser_enum_mbus_code = max9286_enum_mbus_code,
};


