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
#include <linux/kthread.h>

#include "sdrv-deser.h"

#define MAX_SENSOR_NUM 1

#define MAX96706_DEVICE_ID 0x4A
#define MAX96706_SLAVE_ID 0x78


#define MAX96705_SLAVE_ID 0x40
#define MAX96705_DEV_INDEX -9
#define MAX96705_DEVICE_ID 0x41

enum AP0101_I2C_ADDR {
	AP0101_DEF = 0xBA,
	AP0101_CH_A = 0xA8,
};


enum MAX96722_CHANNEL {
	MAX96722_CH_A = 0,
	MAX96722_CH_B = 1,
	MAX96722_CH_C = 2,
	MAX96722_CH_D = 3,
};

enum MAX96705_I2C_ADDR {
	MAX96705_DEF = 0x80,
	MAX96705_CH_A = 0x98,
};

enum MAX96705_READ_WRITE {
	MAX96705_READ,
	MAX96705_WRITE,
};

enum SUBBOARD_TYPE {
	BOARD_SD507_A02P  = 0x00,
	BOARD_SD507_A02  = 0x10,
	BOARD_DB5071 = 0x20,
};

struct max96706_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct max96706_pixfmt max96706_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static int max96706_enum_mbus_code(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	if (code->index >= ARRAY_SIZE(max96706_formats))
		return -EINVAL;

	code->code = max96706_formats[code->index].code;

	return 0;
}

static int max96706_write_reg(deser_dev_t *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_deser;
	buf[0] = reg;
	buf[1] = val;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
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

static int max96706_read_reg(deser_dev_t *sensor, u8 reg, u8 *val)
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
		dev_err(&client->dev, "%s: error: reg=0x%x\n",
				__func__, reg);
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

#if 0 // defined_but_not_used
static int max96705_access_sync(deser_dev_t *dev,  int channel, int write, u16 reg, u8 *val_r, u8 val_w)
{
	int ret = -1;

	if (channel < 0 || channel > 3) {
		dev_err(&dev->i2c_client->dev, "%s: chip max96705 ch=%d, invalid channel.\n", __func__, channel);
		return ret;
	}

	mutex_lock(&dev->serer_lock);

	if (write == MAX96705_WRITE)
		ret = max96705_write_reg(dev, channel, reg, val_w);
	else if (write == MAX96705_READ)
		ret = max96705_read_reg(dev, channel, reg, val_r);
	else
		dev_err(&dev->i2c_client->dev, "%s: chip max96705 Invalid parameter.\n", __func__);

	mutex_unlock(&dev->serer_lock);
	return ret;
}

static int ap0101_read_reg16(deser_dev_t *sensor, int idx, u16 reg,
							 u16 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr + idx;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr + idx;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n",
				__func__, reg);
		return ret;
	}

	*val = ((u16)buf[0] << 8) | (u16)buf[1];

	return 0;

}

static int ap0101_write_reg16(deser_dev_t *sensor, int idx, u16 reg,
							  u16 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.addr = client->addr + idx;
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
#endif // defined_but_not_used

struct reg_value_ov {
	u16 reg_addr;
	u8 val;
	u32 delay_ms;
};

static const struct reg_value_ov OV9284_RegTable[] = {
	{0x0103, 0x01, 500},
	{0x0302, 0x30, 0},
	{0x030D, 0x60, 0},
	{0x030E, 0x06, 0},
	{0x3001, 0x62, 0},
	{0x3004, 0x01, 0},
	{0x3005, 0xFF, 0},
	{0x3006, 0xE2, 0},
	{0x3011, 0x0A, 0},
	{0x3013, 0x18, 0},
	{0x301C, 0xF0, 0},
	{0x3022, 0x07, 0},
	{0x3030, 0x10, 0},
	{0x3039, 0x2E, 0},
	{0x303A, 0xF0, 0},
	{0x3500, 0x00, 0},
	{0x3501, 0x2A, 0},
	{0x3502, 0x90, 0},
	{0x3503, 0x08, 0},
	{0x3505, 0x8C, 0},
	{0x3507, 0x03, 0},
	{0x3508, 0x00, 0},
	{0x3509, 0x10, 0},
	{0x3610, 0x80, 0},
	{0x3611, 0xA0, 0},
	{0x3620, 0x6E, 0},
	{0x3632, 0x56, 0},
	{0x3633, 0x78, 0},
	{0x3662, 0x05, 0},
	{0x3666, 0x5A, 0},
	{0x366F, 0x7E, 0},
	{0x3680, 0x84, 0},
	{0x3712, 0x80, 0},
	{0x372D, 0x22, 0},
	{0x3731, 0x80, 0},
	{0x3732, 0x30, 0},
	{0x3778, 0x00, 0},
	{0x377D, 0x22, 0},
	{0x3788, 0x02, 0},
	{0x3789, 0xA4, 0},
	{0x378A, 0x00, 0},
	{0x378B, 0x4A, 0},
	{0x3799, 0x20, 0},
	{0x3800, 0x00, 0},
	{0x3801, 0x00, 0},
	{0x3802, 0x00, 0},
	{0x3803, 0x00, 0},
	{0x3804, 0x05, 0},
	{0x3805, 0x0F, 0},
	{0x3806, 0x03, 0},
	{0x3807, 0x2F, 0},
	{0x3808, 0x05, 0},
	{0x3809, 0x00, 0},
	{0x380A, 0x02, 0},
	{0x380B, 0xD0, 0},
	{0x380C, 0x03, 0},
	{0x380D, 0x6B, 0},
	{0x380E, 0x05, 0},
	{0x380F, 0x5C, 0},
	{0x3810, 0x00, 0},
	{0x3811, 0x08, 0},
	{0x3812, 0x00, 0},
	{0x3813, 0x08, 0},
	{0x3814, 0x11, 0},
	{0x3815, 0x11, 0},
	{0x3820, 0x40, 0},
	{0x3821, 0x00, 0},
	{0x382C, 0x05, 0},
	{0x382D, 0xB0, 0},
	{0x389D, 0x00, 0},
	{0x3881, 0x42, 0},
	{0x3882, 0x01, 0},
	{0x3883, 0x00, 0},
	{0x3885, 0x02, 0},
	{0x38A8, 0x02, 0},
	{0x38A9, 0x80, 0},
	{0x38B1, 0x00, 0},
	{0x38B3, 0x02, 0},
	{0x38C4, 0x00, 0},
	{0x38C5, 0xC0, 0},
	{0x38C6, 0x04, 0},
	{0x38C7, 0x80, 0},
	{0x3920, 0xFF, 0},
	{0x4003, 0x40, 0},
	{0x4008, 0x04, 0},
	{0x4009, 0x0B, 0},
	{0x400C, 0x00, 0},
	{0x400D, 0x07, 0},
	{0x4010, 0x40, 0},
	{0x4043, 0x40, 0},
	{0x4307, 0x30, 0},
	{0x4317, 0x01, 0},
	{0x4501, 0x00, 0},
	{0x4507, 0x00, 0},
	{0x4509, 0x00, 0},
	{0x450A, 0x08, 0},
	{0x4601, 0x04, 0},
	{0x470F, 0xE0, 0},
	{0x4F07, 0x00, 0},
	{0x4800, 0x00, 0},
	{0x5000, 0x9F, 0},
	{0x5001, 0x00, 0},
	{0x5E00, 0x00, 0},
	{0x5D00, 0x0B, 0},
	{0x5D01, 0x02, 0},
	{0x4837, 0x1C, 0},
	{0x3006, 0xEA, 0},
	{0x3210, 0x04, 0},
	{0x3007, 0x02, 0},
	{0x301C, 0xF0, 0},
	{0x3020, 0x20, 0},
	{0x3025, 0x02, 0},
	{0x3920, 0xFF, 0},
	{0x3923, 0x00, 0},
	{0x3924, 0x00, 0},
	{0x3925, 0x00, 0},
	{0x3926, 0x00, 0},
	{0x3927, 0x00, 0},
	{0x3928, 0x80, 0},
	{0x392B, 0x00, 0},
	{0x392C, 0x00, 0},
	{0x392D, 0x02, 0},
	{0x392E, 0xD8, 0},
	{0x392F, 0xCB, 0},
	{0x38E3, 0x07, 0},
	{0x3885, 0x07, 0},
	{0x382B, 0x3A, 0},
	{0x3670, 0x68, 0},
	{0x3500, 0x00, 0},
	{0x3501, 0x51, 0},
	{0x3502, 0x40, 0},
	{0x3509, 0x20, 0},
	{0x380E, 0x07, 0},
	{0x380F, 0x28, 0},
	{0x3927, 0x01, 0},
	{0x3928, 0x88, 0},
	{0x3929, 0x00, 0},
	{0x392A, 0x41, 100},
	{0x0100, 0x01, 100},
	{0x3006, 0xEC, 0},
	{0x3666, 0x50, 0},
	{0x3667, 0xDA, 0},
	{0x38B3, 0x07, 0},
	{0x3885, 0x07, 0},
	{0x382B, 0x3A, 0},
	{0x3670, 0x68, 0},
	{0x3823, 0x30, 0},
	{0x3824, 0x00, 0},
	{0x3825, 0x08, 0},
	{0x3826, 0x03, 0},
	{0x3827, 0x8A, 0},
	{0x3740, 0x01, 0},
	{0x3741, 0x00, 0},
	{0x3742, 0x08, 0},
};

bool g_9284_t;
bool g_9284_stat;

#if 0 // defined_but_not_used
static int ov9284_read_reg(deser_dev_t *sensor, u16 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	//client->addr = sensor->addr_isp;
	client->addr = 0x70;

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

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n",
				__func__, reg);
		return ret;
	}

	*val = buf[0];

	return 0;

}
#endif // defined_but_not_used

static int ov9284_write_reg(deser_dev_t *sensor, u16 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	//client->addr = sensor->addr_isp;
	client->addr = 0x60;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

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
//#endif

//#if CUSTOM_AR0144
struct reg_value_ar {
	u16 reg_addr;
	u16 val;
};

static const struct reg_value_ar AR0144_RegTable[] = {
	/* (AR0144 1280x800 27MHz input 60fps parallel) */
	{0x301A, 0x00D9},
	{0xFFFF, 0x000A},
	{0x301A, 0x30D8},
	{0x3088, 0x8000},
	{0x3086, 0x327F},
	{0x3086, 0x5780},
	{0x3086, 0x2730},
	{0x3086, 0x7E13},
	{0x3086, 0x8000},
	{0x3086, 0x157E},
	{0x3086, 0x1380},
	{0x3086, 0x000F},
	{0x3086, 0x8190},
	{0x3086, 0x1643},
	{0x3086, 0x163E},
	{0x3086, 0x4522},
	{0x3086, 0x0937},
	{0x3086, 0x8190},
	{0x3086, 0x1643},
	{0x3086, 0x167F},
	{0x3086, 0x9080},
	{0x3086, 0x0038},
	{0x3086, 0x7F13},
	{0x3086, 0x8023},
	{0x3086, 0x3B7F},
	{0x3086, 0x9345},
	{0x3086, 0x0280},
	{0x3086, 0x007F},
	{0x3086, 0xB08D},
	{0x3086, 0x667F},
	{0x3086, 0x9081},
	{0x3086, 0x923C},
	{0x3086, 0x1635},
	{0x3086, 0x7F93},
	{0x3086, 0x4502},
	{0x3086, 0x8000},
	{0x3086, 0x7FB0},
	{0x3086, 0x8D66},
	{0x3086, 0x7F90},
	{0x3086, 0x8182},
	{0x3086, 0x3745},
	{0x3086, 0x0236},
	{0x3086, 0x8180},
	{0x3086, 0x4416},
	{0x3086, 0x3143},
	{0x3086, 0x7416},
	{0x3086, 0x787B},
	{0x3086, 0x7D45},
	{0x3086, 0x023D},
	{0x3086, 0x6445},
	{0x3086, 0x0A3D},
	{0x3086, 0x647E},
	{0x3086, 0x1281},
	{0x3086, 0x8037},
	{0x3086, 0x7F10},
	{0x3086, 0x450A},
	{0x3086, 0x3F74},
	{0x3086, 0x7E10},
	{0x3086, 0x7E12},
	{0x3086, 0x0F3D},
	{0x3086, 0xD27F},
	{0x3086, 0xD480},
	{0x3086, 0x2482},
	{0x3086, 0x9C03},
	{0x3086, 0x430D},
	{0x3086, 0x2D46},
	{0x3086, 0x4316},
	{0x3086, 0x5F16},
	{0x3086, 0x532D},
	{0x3086, 0x1660},
	{0x3086, 0x404C},
	{0x3086, 0x2904},
	{0x3086, 0x2984},
	{0x3086, 0x81E7},
	{0x3086, 0x816F},
	{0x3086, 0x170A},
	{0x3086, 0x81E7},
	{0x3086, 0x7F81},
	{0x3086, 0x5C0D},
	{0x3086, 0x5749},
	{0x3086, 0x5F53},
	{0x3086, 0x2553},
	{0x3086, 0x274D},
	{0x3086, 0x2BF8},
	{0x3086, 0x1016},
	{0x3086, 0x4C09},
	{0x3086, 0x2BB8},
	{0x3086, 0x2B98},
	{0x3086, 0x4E11},
	{0x3086, 0x5367},
	{0x3086, 0x4001},
	{0x3086, 0x605C},
	{0x3086, 0x095C},
	{0x3086, 0x1B40},
	{0x3086, 0x0245},
	{0x3086, 0x0045},
	{0x3086, 0x8029},
	{0x3086, 0xB67F},
	{0x3086, 0x8040},
	{0x3086, 0x047F},
	{0x3086, 0x8841},
	{0x3086, 0x095C},
	{0x3086, 0x0B29},
	{0x3086, 0xB241},
	{0x3086, 0x0C40},
	{0x3086, 0x0340},
	{0x3086, 0x135C},
	{0x3086, 0x0341},
	{0x3086, 0x1117},
	{0x3086, 0x125F},
	{0x3086, 0x2B90},
	{0x3086, 0x2B80},
	{0x3086, 0x816F},
	{0x3086, 0x4010},
	{0x3086, 0x4101},
	{0x3086, 0x5327},
	{0x3086, 0x4001},
	{0x3086, 0x6029},
	{0x3086, 0xA35F},
	{0x3086, 0x4D1C},
	{0x3086, 0x1702},
	{0x3086, 0x81E7},
	{0x3086, 0x2983},
	{0x3086, 0x4588},
	{0x3086, 0x4021},
	{0x3086, 0x7F8A},
	{0x3086, 0x4039},
	{0x3086, 0x4580},
	{0x3086, 0x2440},
	{0x3086, 0x087F},
	{0x3086, 0x885D},
	{0x3086, 0x5367},
	{0x3086, 0x2992},
	{0x3086, 0x8810},
	{0x3086, 0x2B04},
	{0x3086, 0x8916},
	{0x3086, 0x5C43},
	{0x3086, 0x8617},
	{0x3086, 0x0B5C},
	{0x3086, 0x038A},
	{0x3086, 0x484D},
	{0x3086, 0x4E2B},
	{0x3086, 0x804C},
	{0x3086, 0x0B41},
	{0x3086, 0x9F81},
	{0x3086, 0x6F41},
	{0x3086, 0x1040},
	{0x3086, 0x0153},
	{0x3086, 0x2740},
	{0x3086, 0x0160},
	{0x3086, 0x2983},
	{0x3086, 0x2943},
	{0x3086, 0x5C05},
	{0x3086, 0x5F4D},
	{0x3086, 0x1C81},
	{0x3086, 0xE745},
	{0x3086, 0x0281},
	{0x3086, 0x807F},
	{0x3086, 0x8041},
	{0x3086, 0x0A91},
	{0x3086, 0x4416},
	{0x3086, 0x092F},
	{0x3086, 0x7E37},
	{0x3086, 0x8020},
	{0x3086, 0x307E},
	{0x3086, 0x3780},
	{0x3086, 0x2015},
	{0x3086, 0x7E37},
	{0x3086, 0x8020},
	{0x3086, 0x0343},
	{0x3086, 0x164A},
	{0x3086, 0x0A43},
	{0x3086, 0x160B},
	{0x3086, 0x4316},
	{0x3086, 0x8F43},
	{0x3086, 0x1690},
	{0x3086, 0x4316},
	{0x3086, 0x7F81},
	{0x3086, 0x450A},
	{0x3086, 0x4130},
	{0x3086, 0x7F83},
	{0x3086, 0x5D29},
	{0x3086, 0x4488},
	{0x3086, 0x102B},
	{0x3086, 0x0453},
	{0x3086, 0x2D40},
	{0x3086, 0x3045},
	{0x3086, 0x0240},
	{0x3086, 0x087F},
	{0x3086, 0x8053},
	{0x3086, 0x2D89},
	{0x3086, 0x165C},
	{0x3086, 0x4586},
	{0x3086, 0x170B},
	{0x3086, 0x5C05},
	{0x3086, 0x8A60},
	{0x3086, 0x4B91},
	{0x3086, 0x4416},
	{0x3086, 0x0915},
	{0x3086, 0x3DFF},
	{0x3086, 0x3D87},
	{0x3086, 0x7E3D},
	{0x3086, 0x7E19},
	{0x3086, 0x8000},
	{0x3086, 0x8B1F},
	{0x3086, 0x2A1F},
	{0x3086, 0x83A2},
	{0x3086, 0x7E11},
	{0x3086, 0x7516},
	{0x3086, 0x3345},
	{0x3086, 0x0A7F},
	{0x3086, 0x5380},
	{0x3086, 0x238C},
	{0x3086, 0x667F},
	{0x3086, 0x1381},
	{0x3086, 0x8414},
	{0x3086, 0x8180},
	{0x3086, 0x313D},
	{0x3086, 0x6445},
	{0x3086, 0x2A3D},
	{0x3086, 0xD27F},
	{0x3086, 0x4480},
	{0x3086, 0x2494},
	{0x3086, 0x3DFF},
	{0x3086, 0x3D4D},
	{0x3086, 0x4502},
	{0x3086, 0x7FD0},
	{0x3086, 0x8000},
	{0x3086, 0x8C66},
	{0x3086, 0x7F90},
	{0x3086, 0x8194},
	{0x3086, 0x3F44},
	{0x3086, 0x1681},
	{0x3086, 0x8416},
	{0x3086, 0x2C2C},
	{0x3086, 0x2C2C},
	{0x3F00, 0x0005},
	{0x3ED6, 0x3CB1},
	{0x3EDA, 0xBADE},
	{0x3EDA, 0xBAEE},
	{0x3ED6, 0x3CB5},
	{0x3F00, 0x0A05},
	{0x3F00, 0xAA05},
	{0x3F00, 0xAA05},
	{0x3EDA, 0xBCEE},
	{0x3EDA, 0xCCEE},
	{0x3EF8, 0x6542},
	{0x3EF8, 0x6522},
	{0x3EFA, 0x4442},
	{0x3EFA, 0x4422},
	{0x3EFA, 0x4222},
	{0x3EFA, 0x2222},
	{0x3EFC, 0x4446},
	{0x3EFC, 0x4466},
	{0x3EFC, 0x4666},
	{0x3EFC, 0x6666},
	{0x3EEA, 0xAA09},
	{0x3EE2, 0x180E},
	{0x3EE4, 0x0808},
	{0x3060, 0x000E},
	{0x3EEA, 0x2A09},
	{0x3268, 0x0037},
	{0x3092, 0x00CF},
	{0x3786, 0x0006},
	{0x3F4A, 0x0F70},
	{0x3092, 0x00CF},
	{0x3786, 0x0006},
	{0x3268, 0x0036},
	{0x3268, 0x0034},
	{0x3268, 0x0030},
	{0x3064, 0x1802},
	{0x306E, 0x5010},
	{0x306E, 0x4810},
	{0x3EF6, 0x8001},
	{0x3EF6, 0x8041},
	{0x3180, 0xC08F},
	{0x302A, 0x0006},
	{0x302C, 0x0001},
	{0x302E, 0x0004},
	{0x3030, 0x0042},
	{0x3036, 0x000C},
	{0x3038, 0x0001},
	{0x30B0, 0x0038},
	{0xFFFF, 0x000A},
	{0x3002, 0x0000},
	{0x3004, 0x0004},
	{0x3006, 0x031F},
	{0x3008, 0x0503},
	{0x300A, 0x0339},
	{0x300C, 0x05D0},
	{0x3012, 0x0064},
	{0x30A2, 0x0001},
	{0x30A6, 0x0001},
	{0x3040, 0x0000},
	{0x31AE, 0x0200},
	{0x3040, 0x0400},
	{0x30A8, 0x0003},
	{0x3040, 0x0C00},
	{0x30AE, 0x0003},
	{0x3064, 0x1882},
	{0x3064, 0x1982},
	{0x3028, 0x0010},
	{0x3064, 0x1902},
	{0x3064, 0x1802},
	{0x30CE, 0x0040}, /* Disable SHUTTER Pin */
	{0x301A, 0x30DC},
	{0x3064, 0x1802},
};

static int ar0144_read_reg(deser_dev_t *sensor, u16 reg, u16 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	//client->addr = sensor->addr_isp;
	client->addr = 0x10;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n",
				__func__, reg);
		return ret;
	}

	*val = ((u16)buf[0] << 8) | (u16)buf[1];

	return 0;

}
static int ar0144_write_reg(deser_dev_t *sensor, u16 reg, u16 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	//client->addr = sensor->addr_isp;
	client->addr = 0x10;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

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
//#endif

#if 0 // defined_but_not_used
static int ap0101_read_reg32(deser_dev_t *sensor, u8 idx, u16 reg,
							 u32 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[4];
	int ret;

	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr + idx;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = 2;

	msg[1].addr = client->addr + idx;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 4;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n",
				__func__, reg);
		return ret;
	}

	*val = ((u32)buf[0] << 24) | ((u32)buf[1] << 16) | ((u32)buf[2] << 8) |
		   (u32)buf[3];

	return 0;

}

static int ap0101_write_reg32(deser_dev_t *sensor, u8 idx, u16 reg,
							  u32 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[6];
	int ret;

	client->addr = sensor->addr_isp;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 24;
	buf[3] = val >> 16;
	buf[4] = val >> 8;
	buf[5] = val & 0xff;

	msg.addr = client->addr + idx;
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

static int ap0101_change_config(deser_dev_t *sensor, u8 idx)
{
	u16 reg;
	u16 val16;
	int i;

	reg = 0xfc00;
	val16 = 0x2800;
	ap0101_write_reg16(sensor, 0, reg, val16);
	usleep_range(10000, 11000);

	reg = 0x0040;
	val16 = 0x8100;
	ap0101_write_reg16(sensor, 0, reg, val16);
	usleep_range(10000, 11000);

	for (i = 0; i < 10; i++) {
		reg = 0x0040;
		val16 = 0;
		ap0101_read_reg16(sensor, 0, reg, &val16);

		if (val16 == 0x0)
			return 0;

		usleep_range(10000, 11000);
	}

	return -1;
}
#endif // defined_but_not_used

static int tca9539_write_reg(deser_dev_t *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_gpioext;
	buf[0] = reg;
	buf[1] = val;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
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

static int tca9539_read_reg(deser_dev_t *sensor, u8 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_gpioext;
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
		dev_err(&client->dev, "%s: error: reg=0x%x\n",
				__func__, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}
//Power for Rx max96706/712
static int max96706_power(deser_dev_t *dev, bool enable)
{
	//printk("max96706 power +\n");
	dev_info(&dev->i2c_client->dev, "%s: enable=%d.\n", __func__, enable);

	if (dev->pwdn_gpio)
		gpiod_direction_output(dev->pwdn_gpio, enable ? 1 : 0);
	//printk("max96706 power -\n");

	return 0;
}


int check_camera_subboard(deser_dev_t *sensor)
{
	u8  val[2];
	int ret;
	int board_type;
	struct i2c_client *client = sensor->i2c_client;

	#if 0
	ret = tca9539_read_reg(sensor, 0x00, &val[0]);
	if (ret != 0)
		dev_err(&client->dev, "Fail to get tca9539 reg.");

	//printk("TCA9539: 0x0=0x%x\n", val[0]);
	#endif
	ret = tca9539_read_reg(sensor, 0x01, &val[1]);
	if (ret != 0) {
		dev_err(&client->dev, "Fail to get tca9539 reg.");
		return ret;
	}
	board_type = val[1]&0x30;

	//printk("TCA9539: 0x1=0x%x, board_type = 0x%x\n", val[1], board_type);

	return board_type;
}


//Power for camera module
static int poc_power(deser_dev_t *dev, bool enable)
{
	dev_info(&dev->i2c_client->dev, "%s: enable=%d.\n", __func__, enable);
	if (dev->poc_gpio) {
		dev_info(&dev->i2c_client->dev, "%s: dev->poc_gpio=%p.\n", __func__, dev->poc_gpio);
		gpiod_direction_output(dev->poc_gpio, enable ? 1 : 0);
	}
	return 0;
}

//db5071 and sd507 use different gpi gpio.
//tca9539 extent gpio chip
static int gpi_power(deser_dev_t *dev, int gpio, bool enable)
{
	int board_type;
	u8 val;
	u8 reg;
	int shift;

	dev_info(&dev->i2c_client->dev, "%s: enable=%d.\n", __func__, enable);
	//printk("gpi power +\n");

	if (gpio > 15 || gpio < 0)
		return -EINVAL;


	board_type = check_camera_subboard(dev);
	//printk("check board 0x%x +\n", board_type);

	if (board_type < 0)
		board_type = BOARD_DB5071;

	if (gpio > 7) {
		shift = gpio - 8;
		reg = 0x7;
	} else if (gpio < 7) {
		shift = gpio;
		reg = 0x6;
	}

	if (board_type == BOARD_SD507_A02 || board_type == BOARD_SD507_A02P) {
		pr_debug("gpi sd507 a02/a02+ board.\n");
		if (dev->gpi_gpio) {
			//dev_err(&dev->i2c_client->dev, "has gpi_gpio\n", __func__);
			gpiod_direction_output(dev->gpi_gpio, enable ? 1 : 0);
		}
	} else {  //For BOARD_DB5071 PIN9
		pr_debug("gpi db5071 board.\n");
		if (enable == true) {
			tca9539_read_reg(dev, reg, &val);
			val &= ~(1 << shift);
			tca9539_write_reg(dev, reg, val);
		} else if (enable == false) {
			tca9539_read_reg(dev, reg, &val);
			val |= (1 << shift);
			tca9539_write_reg(dev, reg, val);
		}
	}

	//printk("gpi power -\n");
	return 0;

}

//Should implement this interface
static int start_deser(deser_dev_t *sensor, bool en)
{
	u8 val;

	dev_info(&sensor->i2c_client->dev, "%s: %d\n", __func__, en);

	if (en == true) {
		msleep(100);
		max96706_read_reg(sensor, 0x04, &val);
		val &= ~(0x40);
		max96706_write_reg(sensor, 0x04, val);
		usleep_range(1000, 1100);
	} else if (en == false) {
		max96706_read_reg(sensor, 0x04, &val);
		val |= 0x40;
		max96706_write_reg(sensor, 0x04, val);
		usleep_range(1000, 1100);
	}
	g_9284_stat = en;
	return 0;
}

//Should implement this interface
static int max96705_check_chip_id(deser_dev_t *dev)
{
	int ret = 0;
	u8 chip_id = 0;

	ret = max96705_read_reg(dev, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x1E, &chip_id);

	return ret;
}

//Should implement this interface
static int max96706_check_chip_id(deser_dev_t *dev)
{
	struct i2c_client *client = dev->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
//	int i = 0;

	ret = max96706_read_reg(dev, 0x1E, &chip_id);

	if (chip_id !=  MAX96706_DEVICE_ID) {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(max96706), got 0x%x\n",
			__func__, MAX96706_DEVICE_ID, chip_id);
		return -EIO;
	}

	return ret;
}

void max96706_reg_dump(deser_dev_t *sensor)
{
}

static int device_icl02_init(deser_dev_t *sensor)
{
	int i;
//	u16 reg;
	u8 val;
	u16 ttt;

	dev_info(&sensor->i2c_client->dev, "%s():\n", __func__);

	//dbl
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x7, 0x84);
	usleep_range(5000, 6000);

	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x20, 0x2);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x21, 0x3);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x22, 0x4);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x23, 0x5);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x24, 0x6);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x25, 0x7);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x26, 0x8);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x27, 0x9);

	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x30, 0x12);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x31, 0x13);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x32, 0x14);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x33, 0x15);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x34, 0x16);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x35, 0x17);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x36, 0x18);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x37, 0x19);

	usleep_range(5000, 6000);

	//reset 0144
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0xe, 0x12);
	usleep_range(1000, 2000);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0xf, 0x2e);
	usleep_range(5000, 6000);
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0xf, 0x3e);
	usleep_range(10000, 11000);

	//0144 soft reset
	ar0144_write_reg(sensor, 0x301a, 0x0001);
	usleep_range(10000, 11000);

	ar0144_write_reg(sensor, 0x301a, 0x10d8);
	usleep_range(10000, 11000);

	for (i = 0; i < (sizeof(AR0144_RegTable)/sizeof(struct reg_value_ar)); i++) {
		ar0144_write_reg(sensor, AR0144_RegTable[i].reg_addr, AR0144_RegTable[i].val);
		if (AR0144_RegTable[i].reg_addr == 0x301a)
			usleep_range(10000, 11000);
	}

	//trun on led
	ar0144_write_reg(sensor, 0x3270, 0x0100);

	//start stream
	ttt = 0;

	ar0144_read_reg(sensor, 0x301a, &ttt);
	dev_info(&sensor->i2c_client->dev, "%s: ar0144: reg 0x301a=0x%x\n", __func__, ttt);
	ttt |= 0x4;
	ar0144_write_reg(sensor, 0x301a, ttt);

	//trig
	val = 0;
	max96705_read_reg(sensor, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x0f, &val);
	val |= 0x81;
	max96705_write_reg(sensor, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x0f, val);
	usleep_range(100, 200);

	max96705_write_reg(sensor, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x4, 0x87); //video link
	usleep_range(5000, 6000);

	return 0;
}

static int device_minieye_thread(void *data)
{
	deser_dev_t *sensor = data;
//	int flag;
	u8 val;

	val = 0;
	max96705_read_reg(sensor, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x0f, &val);
	while (1) {
		msleep(23);
		if (!g_9284_stat)
			continue;
		if (sensor->gpi_gpio) {
			if (val) {
				//gpiod_direction_output(sensor->gpi_gpio, 0);
				gpi_power(sensor, 9, 0);
				val = 0;
			} else {
				//gpiod_direction_output(sensor->gpi_gpio, 1);
				gpi_power(sensor, 9, 1);
				val = 1;
			}
		} else {
			if (val&0x1)
				val &= ~(0x1);
			else
				val |= 0x81;
			max96705_write_reg(sensor, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x0f, val);
		}
	}
	return 0;
}

static int device_minieye_init(deser_dev_t *sensor)
{
	int i;

	//dbl
	max96705_write_reg(sensor,	(MAX96705_DEF-MAX96705_CH_A)>>1, 0x7, 0x84);
	usleep_range(5000, 6000);


	dev_err(&sensor->i2c_client->dev, "%s-new: sizeof(OV9284_RegTable)=%ld\n",
			__func__, sizeof(OV9284_RegTable)/sizeof(struct reg_value_ov));
	for (i = 0; i < (sizeof(OV9284_RegTable)/sizeof(struct reg_value_ov)); i++) {
		ov9284_write_reg(sensor, OV9284_RegTable[i].reg_addr, OV9284_RegTable[i].val);
		if (OV9284_RegTable[i].delay_ms)
			msleep(OV9284_RegTable[i].delay_ms);
	}


	max96705_write_reg(sensor,  (MAX96705_DEF-MAX96705_CH_A)>>1, 0x4, 0x87);	//config link
	usleep_range(5000, 6000);

	dev_err(&sensor->i2c_client->dev, "%s: end\n", __func__);

	if (g_9284_t == 0) {
		g_9284_t = 1;
		kthread_run(device_minieye_thread, sensor, "ov9284 thread");
	}

	return 0;
}

//Should implement this interface
static int max96706_initialization(deser_dev_t *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 val;
	int i = 0;
//	u16 reg;
//	u16 val16;

	dev_info(&client->dev, "%s()\n", __func__);
	//printk("max96706 init %s pid %d +\n", current->comm, current->pid);
	//set him
	val = 0;
	ret = max96706_read_reg(sensor, 0x06, &val);
	if (ret < 0) {
		dev_err(&client->dev, "max96706 fail to read 0x06=%d\n", ret);
		return ret;
	}

	val |= 0x80;
	max96706_write_reg(sensor, 0x06, val);
	usleep_range(1000, 1100);

	//[7]dbl, [2]hven, [1]cxtp
	val = 0;
	max96706_read_reg(sensor, 0x07, &val);
	dev_info(&client->dev, "96706 0x7=0x%x\n", val);
	if (val != 0x86) {
		max96706_write_reg(sensor, 0x07, 0x86);
		usleep_range(10000, 11000);
	}

	//invert hsync
	val = 0;
	max96706_read_reg(sensor, 0x02, &val);
	val |= 0x80;
	max96706_write_reg(sensor, 0x02, val);
	usleep_range(3000, 3100);

	//disable output
	val = 0;
	max96706_read_reg(sensor, 0x04, &val);
	dev_info(&client->dev, "max96706 read 0x4 = 0x%x\n",  val);
	val = 0x47;
	max96706_write_reg(sensor, 0x04, val);
	msleep(20);

	if (sensor->pmu_gpio) {
		dev_info(&sensor->i2c_client->dev, "%s: sensor->pmu_gpio=%p.\n", __func__, sensor->pmu_gpio);
		gpiod_direction_output(sensor->pmu_gpio, 1);
	}
	msleep(20);


	poc_power(sensor, 0);
	usleep_range(1000, 2000);
	poc_power(sensor, 1);
	msleep(100);

	//dcs
	val = 0;
	max96706_read_reg(sensor, 0x5, &val);
	dev_info(&client->dev, "96706, reg=0x5, val=0x%x\n", val);
	val |= 0x40;
	max96706_write_reg(sensor, 0x5, val);
	val = 0;
	max96706_read_reg(sensor, 0x5, &val);

	if ((sensor->device_type == SDRV3_ICL02) || (sensor->device_type == SDRV3_MINIEYE)) {
		val = 0xb6;	//loc ack
		max96706_write_reg(sensor, 0x0d, val);
		usleep_range(5000, 6000);

		max96705_write_reg(sensor,  (MAX96705_DEF-MAX96705_CH_A)>>1, 0x4, 0x47);	//config link
		usleep_range(5000, 6000);
	}

	//Retry for i2c address comflict in defaul 7b i2c address 0x40.
	for (i = 0; i < 3; i++) {
		ret = max96705_check_chip_id(sensor);

		if (ret != 0) {
			//dev_err(&client->dev, "%s: times %d not found 96705, ret=%d\n", __func__, i, ret);
			msleep(20);
		} else {
			//dev_err(&client->dev, "%s: found 96705, ret=%d\n", __func__, ret);
			break;
		}
	}

	dev_info(&client->dev, "%s: scan 96705, ret=%d\n", __func__, ret);
	if (ret < 0)
		return -ENODEV;

	if (sensor->device_type == SDRV3_ICL02) {
		device_icl02_init(sensor);
		return 0;
	}

	if (sensor->device_type == SDRV3_MINIEYE) {
		device_minieye_init(sensor);
		return 0;
	}


	//Change I2C
	usleep_range(1000, 2000);
	max96705_write_reg(sensor,  (MAX96705_DEF-MAX96705_CH_A)>>1, 0x00, sensor->addr_serer << 1);
	usleep_range(5000, 6000);
	max96705_write_reg(sensor, 0, 0x01, (sensor->addr_deser) << 1);
	usleep_range(10000, 11000);

	if (sensor->gpi_gpio) {
		#if 0
		gpiod_direction_output(sensor->gpi_gpio, 1);
		usleep_range(100, 200);
		gpiod_direction_output(sensor->gpi_gpio, 0);
		usleep_range(100, 200);
		gpiod_direction_output(sensor->gpi_gpio, 1);
		msleep(50);
		#endif

		gpi_power(sensor, 9, 1);
		usleep_range(100, 200);
		gpi_power(sensor, 9, 0);
		usleep_range(100, 200);
		gpi_power(sensor, 9, 1);
		;
	} else {
		//dev_err(&sensor->i2c_client->dev, "%s: no gpi_gpio\n", __func__);
		val = 0;
		max96705_read_reg(sensor, 0, 0x0f, &val);
		//dev_info(&sensor->i2c_client->dev, "%s: val=0x%x\n", __func__, val);
		val |= 0x81;
		max96705_write_reg(sensor, 0, 0x0f, val);
		usleep_range(100, 200);
	}

	//enable dbl, hven
	max96705_write_reg(sensor, 0, 0x07, 0x84);
	usleep_range(10000, 11000);


	max96705_write_reg(sensor, 0, 0x9, sensor->addr_isp << 1);
	max96705_write_reg(sensor, 0, 0xa, AP0101_DEF);
	msleep(20);

#if 0
	reg = 0x0;	//chip version, 0x0160
	val16 = 0;
	ap0101_read_reg16(sensor, 0, reg, &val16);
	dev_info(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);


	reg = 0xca9c;	//
	val16 = 0;
	ap0101_read_reg16(sensor, 0, reg, &val16);
	dev_info(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);
	if ((val16 & (0x1<<10)) == 0) {
		val16 = 0x405;
		ap0101_write_reg16(sensor, 0, reg, val16);
		usleep_range(10000, 11000);

		for (i = 0; i < 10; i++) {
			ret = ap0101_change_config(sensor, 0);
			if (ret == 0)
				break;
			msleep(100);
		}
	}
	val16 = 0;
	ap0101_read_reg16(sensor, 0, reg, &val16);
	dev_info(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);
	if ((val16 & (0x1<<9)) == 0) {
		val16 = 0x605;
		ap0101_write_reg16(sensor, 0, reg, val16);
		usleep_range(10000, 11000);

		for (i = 0; i < 10; i++) {
			ret = ap0101_change_config(sensor, 0);
			if (ret == 0)
				break;
			msleep(100);
		}
	}
#endif

	//dev_err(&client->dev, "0101, end msb. i=%d\n", i);
	dev_info(&client->dev, "%s: end.\n", __func__);

	return 0;
}

deser_para_t max96706_para = {
	.name = "max96706",
	.addr_deser = MAX96706_SLAVE_ID,
	.addr_serer = MAX96705_CH_A>>1,
	.addr_isp	= AP0101_CH_A>>1,
	.addr_gpioext = 0x74,
	.pclk		= 72*1000*1000,
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
	.used		= DESER_NOT_USED, // NOT USED IN DEFAULT
	.reset_gpio = NULL,	//if dts can't config reset and pwdn, use these two members.
	.pwdn_gpio	= NULL,
	.power_deser = max96706_power,
	.power_module = poc_power,
	.detect_deser = max96706_check_chip_id,
	.init_deser = max96706_initialization,
	.start_deser = start_deser,
	.dump_deser	= max96706_reg_dump,
	.deser_enum_mbus_code = max96706_enum_mbus_code,
};
