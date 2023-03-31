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

#define ARK7116_WIDTH  720
#define ARK7116_HEIGHT  480

enum ark7116_mode_id {
	ARK7116_MODE_480P_720_576 = 0,
    ARK7116_MODE_480P_720_480 = 1,
	ARK7116_NUM_MODES,
};

enum ark7116_frame_rate {
    ARK7116_30_FPS = 0,
	ARK7116_25_FPS = 1,
	ARK7116_NUM_FRAMERATES,
};

struct ark7116_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct ark7116_pixfmt ark7116_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static const int ark7116_framerates[] = {
    [ARK7116_30_FPS] = 30,
	[ARK7116_25_FPS] = 25,
};

struct ark7116_ctrls {
	struct v4l2_ctrl_handler handler;
	struct {
		struct v4l2_ctrl *auto_exp;
		struct v4l2_ctrl *exposure;
	};
	struct {
		struct v4l2_ctrl *auto_wb;
		struct v4l2_ctrl *blue_balance;
		struct v4l2_ctrl *red_balance;
	};
	struct {
		struct v4l2_ctrl *auto_gain;
		struct v4l2_ctrl *gain;
	};
	struct v4l2_ctrl *brightness;
	struct v4l2_ctrl *light_freq;
	struct v4l2_ctrl *saturation;
	struct v4l2_ctrl *contrast;
	struct v4l2_ctrl *hue;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
};

enum ark7116_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct ark7116_mode_info {
	enum ark7116_mode_id id;
	enum ark7116_downsize_mode dn_mode;
	u32 hact;
	u32 htot;
	u32 vact;
	u32 vtot;
};

struct ark7116_dev {
	struct i2c_client *i2c_client;
	struct v4l2_subdev sd;

	struct media_pad pad;

	struct v4l2_fwnode_endpoint ep;	/* the parsed DT endpoint info */

	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *gpi_gpio;

	bool upside_down;
	uint32_t link_count;
	/* lock to protect all members below */
	struct mutex lock;

	int power_count;

	struct v4l2_mbus_framefmt fmt;
	bool pending_fmt_change;

	const struct ark7116_mode_info *current_mode;
	const struct ark7116_mode_info *last_mode;
	enum ark7116_frame_rate current_fr;

	struct v4l2_fract frame_interval;

	struct ark7116_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
};

static const struct ark7116_mode_info
	ark7116_mode_data[ARK7116_NUM_FRAMERATES][ARK7116_NUM_MODES] = {
	{
		{
			ARK7116_MODE_480P_720_480, SUBSAMPLING,
			720, 720+200, 480, 480+160,
		},
		{
			ARK7116_MODE_480P_720_480, SUBSAMPLING,
			720, 720+200, 480, 480+160,
		},
	},
};

typedef struct _PanlstaticPara
{
	u16 addr;
	u8 data;
} PanlstaticPara;

PanlstaticPara  AV1_staticPara[] =
{
//GLOBAL
    {0XFD02,0X00},
    {0XFD0A,0X30},
    {0XFD0B,0X27},
    {0XFD0C,0X33},
    {0XFD0D,0XF0},
    {0XFD0E,0X2C},
    {0XFD0F,0X03},
    {0XFD10,0X04},
    {0XFD11,0XFF},
    {0XFD12,0XFF},
    {0XFD13,0XFF},
    {0XFD14,0X02},
    {0XFD15,0X02},
    {0XFD16,0X0A},
    {0XFD1A,0X40},
    {0XFD19,0X02},
//PWM
//DECODER
    {0XFE01,0X06},
    {0XFE06,0X02},
    {0XFE07,0X80},
    {0XFE0F,0X07},
    {0XFE13,0X16},
    {0XFE14,0X22},
    {0XFE15,0X05},
    {0XFE26,0X0E},
    {0XFE27,0X00},
    {0XFE48,0X07},
    {0XFE54,0X00},
    {0XFE55,0X0A},
    {0XFE63,0XC0},
    {0XFE83,0X7F},
    {0XFED5,0XB1},
    {0XFED7,0XF7},
    {0XFEDC,0X00},
    {0XFE44,0X20},
    {0XFE45,0X80},
    {0XFE43,0X80},
    {0XFECB,0X00},
    {0XFE56,0X07},
    {0XFEC9,0X00},
    {0XFE46,0X00},
    {0XFE42,0X00},
//VP
    {0XFFB0,0X67},
    {0XFFB1,0X0F},
    {0XFFB2,0X10},
    {0XFFB3,0X10},
    {0XFFB4,0X10},
    {0XFFB7,0X10},
    {0XFFB8,0X10},
    {0XFFB9,0X22},
    {0XFFBA,0X20},
    {0XFFBB,0X22},
    {0XFFBC,0X20},
    {0XFFC7,0X31},
    {0XFFC8,0X05},
    {0XFFC9,0X00},
    {0XFFCB,0X80},
    {0XFFCC,0X80},
    {0XFFCD,0X2D},
    {0XFFCE,0X00},
    {0XFFCF,0X80},
    {0XFFD0,0X80},
    {0XFFD2,0X4F},
    {0XFFD3,0X80},
    {0XFFD4,0X7A},
    {0XFFD7,0X10},
    {0XFFD8,0X80},
    {0XFFE7,0X50},
    {0XFFE8,0X10},
    {0XFFE9,0X22},
    {0XFFEA,0X20},
    {0XFFF0,0X42},
    {0XFFF1,0XE0},
    {0XFFF2,0XDE},
    {0XFFF3,0XE1},
    {0XFFF4,0XFD},
    {0XFFF5,0X2E},
    {0XFFF6,0XF0},
    {0XFFF7,0XDD},
    {0XFFF8,0XF0},
    {0XFFF9,0XFD},
    {0XFFFA,0X33},
    {0XFFFB,0X81},
    {0XFFD5,0X00},
    {0XFFD6,0X38},
//TCON
    {0XFC00,0X40},
    {0XFC01,0X00},
    {0XFC02,0X00},
    {0XFC09,0X0E},
    {0XFC0A,0X33},
//SCALE
    {0XFC90,0X02},
    {0XFC91,0X00},
    {0XFC92,0X00},
    {0XFC93,0X0C},
    {0XFC94,0X00},
    {0XFC95,0X00},
    {0XFC96,0XD8},
    {0XFC97,0X03},
    {0XFC98,0X00},
    {0XFC99,0X04},
    {0XFC9A,0X59},
    {0XFC9B,0X03},
    {0XFC9C,0X01},
    {0XFC9D,0X00},
    {0XFC9E,0X06},
    {0XFC9F,0X00},
    {0XFCA0,0X23},
    {0XFCA1,0X00},
    {0XFCA2,0XF5},
    {0XFCA3,0X02},
    {0XFCA4,0X03},
    {0XFCA5,0X00},
    {0XFCA6,0X05},
    {0XFCA7,0X00},
    {0XFCA8,0X0E},
    {0XFCA9,0X00},
    {0XFCAA,0X06},
    {0XFCAB,0X01},
    {0XFCAC,0X1A},
    {0XFCAD,0X00},
    {0XFCAE,0X00},
    {0XFCAF,0X00},
    {0XFCB0,0X00},
    {0XFCB1,0X14},
    {0XFCB2,0X00},
    {0XFCB3,0X00},
    {0XFCB4,0X00},
    {0XFCB5,0X00},
    {0XFCB7,0X07},
    {0XFCB8,0X01},
    {0XFCBB,0X37},
    {0XFCBC,0X01},
    {0XFCBD,0X01},
    {0XFCBE,0X00},
    {0XFCBF,0X0C},
    {0XFCC0,0X00},
    {0XFCC1,0X00},
    {0XFCC2,0XDF},
    {0XFCC3,0X03},
    {0XFCC4,0X00},
    {0XFCC5,0X04},
    {0XFCC6,0X62},
    {0XFCC7,0X03},
    {0XFCC8,0X01},
    {0XFCC9,0X00},
    {0XFCCA,0X06},
    {0XFCCB,0X00},
    {0XFCCC,0X20},
    {0XFCCD,0X00},
    {0XFCCE,0XF2},
    {0XFCCF,0X02},
    {0XFCD1,0X00},
    {0XFCD2,0X08},
    {0XFCD3,0X00},
    {0XFCD4,0X0C},
    {0XFCD5,0X00},
    {0XFCD6,0X2C},
    {0XFCD7,0X01},
    {0XFCD8,0X06},
    {0XFCD9,0X00},
    {0XFCDA,0X00},
    {0XFCDB,0X00},
    {0XFCDC,0X00},
    {0XFCDD,0X14},
    {0XFCDE,0X00},
    {0XFCDF,0X00},
    {0XFCE0,0X00},
    {0XFCE1,0X00},
    {0XFCE3,0XC1},
    {0XFCE4,0X02},
    {0XFCE7,0X03},
    {0XFCD0,0X03},
    {0XFCE2,0X00},
    {0XFCB6,0X00},
    {0XFB35,0X00},
    {0XFB89,0X00},
//GAMMA
    {0XFF00,0X03},
    {0XFF01,0X05},
    {0XFF02,0X0B},
    {0XFF03,0X11},
    {0XFF04,0X18},
    {0XFF05,0X20},
    {0XFF06,0X29},
    {0XFF07,0X33},
    {0XFF08,0X3D},
    {0XFF09,0X48},
    {0XFF0A,0X54},
    {0XFF0B,0X5F},
    {0XFF0C,0X6A},
    {0XFF0D,0X75},
    {0XFF0E,0X7F},
    {0XFF0F,0X89},
    {0XFF10,0X94},
    {0XFF11,0X9F},
    {0XFF12,0XA9},
    {0XFF13,0XB3},
    {0XFF14,0XBD},
    {0XFF15,0XC5},
    {0XFF16,0XCD},
    {0XFF17,0XD5},
    {0XFF18,0XDB},
    {0XFF19,0XE1},
    {0XFF1A,0XE6},
    {0XFF1B,0XEB},
    {0XFF1C,0XEF},
    {0XFF1D,0XF3},
    {0XFF1E,0XF7},
    {0XFF1F,0XFB},
    {0XFF20,0X05},
    {0XFF21,0X0B},
    {0XFF22,0X11},
    {0XFF23,0X18},
    {0XFF24,0X20},
    {0XFF25,0X29},
    {0XFF26,0X33},
    {0XFF27,0X3D},
    {0XFF28,0X48},
    {0XFF29,0X54},
    {0XFF2A,0X5F},
    {0XFF2B,0X6A},
    {0XFF2C,0X75},
    {0XFF2D,0X7F},
    {0XFF2E,0X89},
    {0XFF2F,0X94},
    {0XFF30,0X9F},
    {0XFF31,0XA9},
    {0XFF32,0XB3},
    {0XFF33,0XBD},
    {0XFF34,0XC5},
    {0XFF35,0XCD},
    {0XFF36,0XD5},
    {0XFF37,0XDB},
    {0XFF38,0XE1},
    {0XFF39,0XE6},
    {0XFF3A,0XEB},
    {0XFF3B,0XEF},
    {0XFF3C,0XF3},
    {0XFF3D,0XF7},
    {0XFF3E,0XFB},
    {0XFF3F,0X05},
    {0XFF40,0X0B},
    {0XFF41,0X11},
    {0XFF42,0X18},
    {0XFF43,0X20},
    {0XFF44,0X29},
    {0XFF45,0X33},
    {0XFF46,0X3D},
    {0XFF47,0X48},
    {0XFF48,0X54},
    {0XFF49,0X5F},
    {0XFF4A,0X6A},
    {0XFF4B,0X75},
    {0XFF4C,0X7F},
    {0XFF4D,0X89},
    {0XFF4E,0X94},
    {0XFF4F,0X9F},
    {0XFF50,0XA9},
    {0XFF51,0XB3},
    {0XFF52,0XBD},
    {0XFF53,0XC5},
    {0XFF54,0XCD},
    {0XFF55,0XD5},
    {0XFF56,0XDB},
    {0XFF57,0XE1},
    {0XFF58,0XE6},
    {0XFF59,0XEB},
    {0XFF5A,0XEF},
    {0XFF5B,0XF3},
    {0XFF5C,0XF7},
    {0XFF5D,0XFB},
    {0XFF5E,0XFF},
    {0XFF5F,0XFF},
    {0XFF60,0XFF},
};;


PanlstaticPara AMT_PadMuxStaticPara[]=
{
//PAD MUX
    {0XFD30,0X22},
    {0XFD32,0X11},
    {0XFD33,0X11},
    {0XFD34,0X44},
    {0XFD35,0X40},
    {0XFD36,0X44},
    {0XFD37,0X44},
    {0XFD38,0X44},
    {0XFD39,0X44},
    {0XFD3A,0X11},
    {0XFD3B,0X11},
    {0XFD3C,0X44},
    {0XFD3D,0X44},
    {0XFD3E,0X44},
    {0XFD3F,0X44},
    {0XFD40,0X44},
    {0XFD41,0X44},
    {0XFD44,0X01},
    {0XFD45,0X00},
    {0XFD46,0X00},
    {0XFD47,0X00},
    {0XFD48,0X00},
    {0XFD49,0X00},
    {0XFD4A,0X00},
    {0XFD4B,0X00},
    {0XFD50,0X0B},
    {0XFD51,0X00},
    {0XFD52,0X00},
    {0XFD53,0X00},
    {0XFD54,0X00},
    {0XFD55,0X00},
    {0XFD56,0X00},
    {0XFD57,0X00},
    {0XFD58,0X00},
    {0XFD59,0X00},
    {0XFD5A,0X00},
};

static inline struct ark7116_dev *to_ark7116_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ark7116_dev, sd);
}

static int ark7116_amt_calc_subaddr(u8 tmp_addr)
{
    u8 sub_addr = 0x00;
    switch(tmp_addr)
	{
	    case 0XF9:
	    case 0XFD:
			 sub_addr= 0XB0;
			 break;

		case 0XF3:
			 sub_addr= 0XE6;
			  break;

		case 0XFA:
			 sub_addr= 0XBE;
			 break;

		case 0XFB:
			 sub_addr= 0XB6;
			 break;

	    case 0XFC:
			 sub_addr= 0XB8;
			 break;

		case 0XFE:
			 sub_addr= 0XB2;
			 break;

		case 0XFF:
			 sub_addr= 0XB4;
			 break;

		case 0X00:
			sub_addr = 0XBE;
			break;

		default:
			 sub_addr= 0XB0;
			 break;
	}

    return sub_addr;
}

static int ark7116_amt_write_reg(struct ark7116_dev *sensor, u16 reg, u8 val)
{
    int ret = 0;
    u8 addr = 0;
    u8 tmp_addr = 0;
    u8 sub_addr = 0;
    u8 buf[2];
    struct i2c_msg msg;
    struct i2c_client *client = sensor->i2c_client;

    addr = (u8)(reg&0xFF);
    tmp_addr = (u8)((reg>>8)&0xFF);
    sub_addr = ark7116_amt_calc_subaddr(tmp_addr);
    sub_addr = (sub_addr>>1);

    buf[0] = addr;
	buf[1] = val;

    msg.addr = sub_addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

    ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret < 0) {
		//dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	return 0;
}

static int ark7116_amt_read_reg(struct ark7116_dev *sensor, u16 reg, u8 *val)
{
    int ret = 0;
    u8 addr = 0;
    u8 tmp_addr = 0;
    u8 sub_addr = 0;
    u8 buf[1];
    u8 read_buf[1];
    struct i2c_msg msg[2];
    struct i2c_client *client = sensor->i2c_client;

    addr = (u8)(reg&0xFF);
    tmp_addr = (u8)((reg>>8)&0xFF);
    sub_addr = ark7116_amt_calc_subaddr(tmp_addr);
    sub_addr = (sub_addr>>1);

    buf[0] = addr;

	msg[0].addr = sub_addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = sub_addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = read_buf;
	msg[1].len = sizeof(read_buf);

    ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		//dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	*val = read_buf[0];

	return 0;
}

#if 0
static int ap0101_read_reg8(struct ark7116_dev *sensor, u16 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

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
		dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	dev_err(&client->dev, "%s: reg=0x%x, 0x%x, 0x%x\n",
		__func__, reg, buf[0], buf[1]);
	*val = ((u16) buf[0] << 8) | (u16) buf[1];

	return 0;

}

static int ap0101_write_reg8(struct ark7116_dev *sensor, u16 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

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

static int ap0101_read_reg16(struct ark7116_dev *sensor, u16 reg, u16 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

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
		dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	dev_err(&client->dev, "%s: reg=0x%x, 0x%x, 0x%x\n",
		__func__, reg, buf[0], buf[1]);
	*val = ((u16) buf[0] << 8) | (u16) buf[1];

	return 0;

}

static int ap0101_write_reg16(struct ark7116_dev *sensor, u16 reg, u16 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

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

static int ap0101_read_reg32(struct ark7116_dev *sensor, u16 reg, u32 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[4];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = 2;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 4;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	dev_err(&client->dev, "%s: reg=0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, reg, buf[0], buf[1], buf[2], buf[3]);
	*val =
	    ((u32) buf[0] << 24) | ((u32) buf[1] << 16) | ((u32) buf[2] << 8) |
	    (u32) buf[3];

	return 0;

}

static int ap0101_write_reg32(struct ark7116_dev *sensor, u16 reg, u32 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[6];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 24;
	buf[3] = val >> 16;
	buf[4] = val >> 8;
	buf[5] = val & 0xff;

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
#endif

/*
static struct v4l2_subdev *ark7116_get_interface_subdev(struct v4l2_subdev *sd)
{
	struct media_pad *source_pad, *remote_pad;
	int ret;

	ret = sdrv_find_pad(sd, MEDIA_PAD_FL_SOURCE);

	if (ret < 0)
		return NULL;

	source_pad = &sd->entity.pads[ret];

	remote_pad = media_entity_remote_pad(source_pad);

	if (!remote_pad || !is_media_entity_v4l2_subdev(remote_pad->entity))
		return NULL;

	return media_entity_to_v4l2_subdev(remote_pad->entity);
}
*/

static int ark7116_s_power(struct v4l2_subdev *sd, int on)
{
	struct ark7116_dev *sensor = to_ark7116_dev(sd);
	dev_err(&sensor->i2c_client->dev, "%s: on=%d\n", __func__, on);
	return 0;
}

static int ark7116_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->pad != 0)
		return -EINVAL;

	if (fse->index >= ARK7116_NUM_MODES)
		return -EINVAL;

	fse->min_width = ark7116_mode_data[0][fse->index].hact;
	fse->max_width = fse->min_width;
	fse->min_height = ark7116_mode_data[0][fse->index].vact;
	fse->max_height = fse->min_height;
	return 0;
}

static int ark7116_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg,
				      struct v4l2_subdev_frame_interval_enum
				      *fie)
{
	struct v4l2_fract tpf;

	if (fie->pad != 0)
		return -EINVAL;

	if (fie->index >= ARK7116_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = ark7116_framerates[fie->index];

	fie->interval = tpf;

	return 0;
}

static int ark7116_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct ark7116_dev *sensor = to_ark7116_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ark7116_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

static int ark7116_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	if (code->index >= ARRAY_SIZE(ark7116_formats))
		return -EINVAL;

	code->code = ark7116_formats[code->index].code;

	return 0;
}

static int ark7116_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ark7116_dev *sensor = to_ark7116_dev(sd);

	dev_err(&sensor->i2c_client->dev, "%s: enable=%d\n", __func__, enable);

	return 0;
}

static int ark7116_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct ark7116_dev *sensor = to_ark7116_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	fmt = &sensor->fmt;

	format->format = *fmt;

	mutex_unlock(&sensor->lock);

	return 0;
}

static int ark7116_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct ark7116_dev *sensor = to_ark7116_dev(sd);

	if(format->format.code != sensor->fmt.code)
		return -EINVAL;

    format->format.field = V4L2_FIELD_INTERLACED;

	return 0;
}

static int ark7116_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops ark7116_core_ops = {
	.s_power = ark7116_s_power,
};

static const struct v4l2_subdev_video_ops ark7116_video_ops = {
	.g_frame_interval = ark7116_g_frame_interval,
	.s_frame_interval = ark7116_s_frame_interval,
	.s_stream = ark7116_s_stream,
};

static const struct v4l2_subdev_pad_ops ark7116_pad_ops = {
	.enum_mbus_code = ark7116_enum_mbus_code,
	.get_fmt = ark7116_get_fmt,
	.set_fmt = ark7116_set_fmt,
	.enum_frame_size = ark7116_enum_frame_size,
	.enum_frame_interval = ark7116_enum_frame_interval,
};

static const struct v4l2_subdev_ops ark7116_subdev_ops = {
	.core = &ark7116_core_ops,
	.video = &ark7116_video_ops,
	.pad = &ark7116_pad_ops,
};

static const struct v4l2_ctrl_ops ark7116_ctrl_ops = {
	.s_ctrl = ark7116_s_ctrl,
};

static const char *const test_pattern_menu[] = {
	"Disabled",
	"Color bars",
};

#if 0
static int ark7116_init_controls(struct ark7116_dev *sensor)
{
	const struct v4l2_ctrl_ops *ops = &ark7116_ctrl_ops;
	struct ark7116_ctrls *ctrls = &sensor->ctrls;
	struct v4l2_ctrl_handler *hdl = &ctrls->handler;
	int ret;

	v4l2_ctrl_handler_init(hdl, 32);

	/* we can use our own mutex for the ctrl lock */
	hdl->lock = &sensor->lock;

	/* Auto/manual white balance */
	ctrls->auto_wb = v4l2_ctrl_new_std(hdl, ops,
					   V4L2_CID_AUTO_WHITE_BALANCE,
					   0, 1, 1, 1);
	ctrls->blue_balance = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_BLUE_BALANCE,
						0, 4095, 1, 0);
	ctrls->red_balance = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_RED_BALANCE,
					       0, 4095, 1, 0);
	/* Auto/manual exposure */
	ctrls->auto_exp = v4l2_ctrl_new_std_menu(hdl, ops,
						 V4L2_CID_EXPOSURE_AUTO,
						 V4L2_EXPOSURE_MANUAL, 0,
						 V4L2_EXPOSURE_AUTO);
	ctrls->exposure = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_EXPOSURE,
					    0, 65535, 1, 0);
	/* Auto/manual gain */
	ctrls->auto_gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_AUTOGAIN,
					     0, 1, 1, 1);
	ctrls->gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_GAIN, 0, 1023, 1, 0);

	ctrls->saturation = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_SATURATION,
					      0, 255, 1, 64);
	ctrls->hue = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_HUE, 0, 359, 1, 0);
	ctrls->contrast = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_CONTRAST,
					    0, 255, 1, 0);
	ctrls->test_pattern =
	    v4l2_ctrl_new_std_menu_items(hdl, ops, V4L2_CID_TEST_PATTERN,
					 ARRAY_SIZE(test_pattern_menu) - 1,
					 0, 0, test_pattern_menu);
	ctrls->hflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	ctrls->vflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	ctrls->light_freq =
	    v4l2_ctrl_new_std_menu(hdl, ops,
				   V4L2_CID_POWER_LINE_FREQUENCY,
				   V4L2_CID_POWER_LINE_FREQUENCY_AUTO, 0,
				   V4L2_CID_POWER_LINE_FREQUENCY_50HZ);

	if (hdl->error) {
		ret = hdl->error;
		goto free_ctrls;
	}

	ctrls->gain->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->exposure->flags |= V4L2_CTRL_FLAG_VOLATILE;

	v4l2_ctrl_auto_cluster(3, &ctrls->auto_wb, 0, false);
	v4l2_ctrl_auto_cluster(2, &ctrls->auto_gain, 0, true);
	v4l2_ctrl_auto_cluster(2, &ctrls->auto_exp, 1, true);

	sensor->sd.ctrl_handler = hdl;
	return 0;

free_ctrls:
	v4l2_ctrl_handler_free(hdl);
	return ret;
}
#endif

static int ark7116_amt_write_block(struct ark7116_dev *sensor, PanlstaticPara *para, u16 size)
{
    int i;

    for (i=0; i<size; i++) {
        ark7116_amt_write_reg(sensor, para[i].addr, para[i].data);
    }

    return 0;
}

/*
static int ark7116_amt_set_slave_mode(struct ark7116_dev *sensor)
{
    u8 i;
    u8 read_data = 0;
    u8 AddrBuff[6] = {0xa1,0xa2,0xa3,0xa4,0xa5,0xa6};
    u8 DataBuff[6] = {0x00,0x00,0x00,0x00,0x00,0x00};


    DataBuff[0] = 0X55;
    DataBuff[1] = 0xAA;
    DataBuff[2] = 0X03;
    DataBuff[3] = 0X50;  //slave mode
    DataBuff[4] = 0;     // crc val
    DataBuff[5] = DataBuff[2]^DataBuff[3]^DataBuff[4];

    ark7116_amt_write_reg(sensor, 0XFAC6, 0X40);
    ark7116_amt_write_reg(sensor, 0XFD00, 0X00);
    usleep_range(20000, 20100);
    ark7116_amt_write_reg(sensor, 0XFD00, 0X5A);
    usleep_range(20000, 20100);
	ark7116_amt_write_reg(sensor, 0XFAAF, 0x00); //I2c Write Start  BUS_STATUS_ADDR = 0XFAAF

	for(i =0;i < 6;i++)
	{
	   ark7116_amt_write_reg(sensor, AddrBuff[i], DataBuff[i]);
	}

	ark7116_amt_write_reg(sensor, 0XFAAF, 0x11);  //I2c Write End
    //这个200MS延时非常重要，移植时一定 要注意，否则SLAVE有数据出错
    usleep_range(200000, 200100);

    return 0;
}
*/

/*
static int ark7116_amt_set_slave_mode_2(struct ark7116_dev *sensor)
{
    u8 i;
    u8 read_data = 0;
    u8 AddrBuff[6] = {0xa1,0xa2,0xa3,0xa4,0xa5,0xa6};
    u8 DataBuff[6] = {0x00,0x00,0x00,0x00,0x00,0x00};


    DataBuff[0] = 0X55;
    DataBuff[1] = 0xAA;
    DataBuff[2] = 0X03;
    DataBuff[3] = 0X50;  //slave mode
    DataBuff[4] = 0;     // crc val
    DataBuff[5] = DataBuff[2]^DataBuff[3]^DataBuff[4];

    ark7116_amt_write_reg(sensor, 0XFAC6, 0X40);
	ark7116_amt_write_reg(sensor, 0XFAAF, 0x00); //I2c Write Start  BUS_STATUS_ADDR = 0XFAAF

	for(i =0;i < 6;i++)
	{
	   ark7116_amt_write_reg(sensor, AddrBuff[i], DataBuff[i]);
	}

	ark7116_amt_write_reg(sensor, 0XFAAF, 0x11);  //I2c Write End
    //这个200MS延时非常重要，移植时一定 要注意，否则SLAVE有数据出错
    usleep_range(200000, 200100);
    return 0;
}
*/

static int ark7116_amt_set_slave_mode(struct ark7116_dev *sensor)
{
    u8 i;
    u8 read_data = 0;

    u8 AddrBuff[6] = {0xa1,0xa2,0xa3,0xa4,0xa5,0xa6};
    u8 DataBuff[6] = {0x00,0x00,0x00,0x00,0x00,0x00};

    DataBuff[0] = 0X55;
    DataBuff[1] = 0xAA;
    DataBuff[2] = 0X03;
    DataBuff[3] = 0X50;  //slave mode
    DataBuff[4] = 0;     // crc val
    DataBuff[5] = DataBuff[2]^DataBuff[3]^DataBuff[4];

    ark7116_amt_write_reg(sensor, 0XFAC6, 0X40);
    ark7116_amt_write_reg(sensor, 0XFD00, 0X00);
    ark7116_amt_write_reg(sensor, 0XFD00, 0X5A);

    i = 0;
    while(i++ < 50) {
        ark7116_amt_read_reg(sensor, 0XFAC6, &read_data);
        if(read_data == 0xc0)
            break;
    }

    ark7116_amt_write_reg(sensor, 0XFAC6, 0X40);
	ark7116_amt_write_reg(sensor, 0XFAAF, 0x00); //I2c Write Start  BUS_STATUS_ADDR = 0XFAAF

	for(i =0;i < 6;i++)
	{
	   ark7116_amt_write_reg(sensor, AddrBuff[i], DataBuff[i]);
	}

	ark7116_amt_write_reg(sensor, 0XFAAF, 0x11);  //I2c Write End
    usleep_range(200000, 200100);  //这个200MS延时非常重要，移植时一定 要注意，否则SLAVE有数据出错
    return 0;
}

static int ark7116_initialization(struct ark7116_dev *sensor)
{
    int size;
    u8 read_data = 0;
    struct i2c_client *client = sensor->i2c_client;

    ark7116_amt_set_slave_mode(sensor);
    ark7116_amt_write_reg(sensor, 0XFAC6, 0x40);
    ark7116_amt_read_reg(sensor, 0XFAC6, &read_data);
    read_data &= 0x80;
	if(0 != read_data) {

		return -1;
	}

    ark7116_amt_write_reg(sensor, 0XFD0E, 0X20);
    size = sizeof(AV1_staticPara) / sizeof(AV1_staticPara[0]);
    ark7116_amt_write_block(sensor, AV1_staticPara, size);
    size = sizeof(AMT_PadMuxStaticPara) / sizeof(AMT_PadMuxStaticPara[0]);
    ark7116_amt_write_block(sensor, AMT_PadMuxStaticPara, size);
    ark7116_amt_write_reg(sensor, 0XFD0E, 0X2C);

    // option cvbs0
    ark7116_amt_write_reg(sensor, 0XFEDC, 0x00);

    // contarst
    ark7116_amt_read_reg(sensor, 0xFFD3, &read_data);

    // signel 0xe is ok
    read_data = 0;
    ark7116_amt_read_reg(sensor, 0xFE26, &read_data);
    dev_err(&client->dev, "<%d, %s>:read_data=0x%x", __LINE__, __func__, read_data);

    // 1:ntsc, 0:pal
    read_data = 0;
    ark7116_amt_read_reg(sensor, 0xFE2A, &read_data);
    dev_err(&client->dev, "<%d, %s>:read_data=0x%x", __LINE__, __func__, read_data);

    return 0;
}

static int ark7116_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	struct ark7116_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	int ret;

	dev_info(dev, "%s++\n", __func__);
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;

	fmt = &sensor->fmt;
	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = ARK7116_WIDTH;
	fmt->height = ARK7116_HEIGHT;
	fmt->field = V4L2_FIELD_INTERLACED;
	sensor->frame_interval.numerator = 1;

	sensor->frame_interval.denominator = ark7116_framerates[ARK7116_30_FPS];
	sensor->current_fr = ARK7116_30_FPS;
	sensor->current_mode = &ark7116_mode_data[ARK7116_30_FPS][0];
	sensor->ae_target = 52;

	/* optional indication of physical rotation of sensor */
	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "rotation",
				       &rotation);

	if (!ret) {
		switch (rotation) {
		case 180:
			sensor->upside_down = true;

			/* fall through */
		case 0:
			break;

		default:
			dev_warn(dev,
				 "%u degrees rotation is not supported, ignoring...\n",
				 rotation);
		}
	}

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(&client->dev),
						  NULL);

	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(endpoint, &sensor->ep);
	fwnode_handle_put(endpoint);

	if (ret) {
		dev_err(dev, "Could not parse endpoint\n");
		return ret;
	}

	v4l2_i2c_subdev_init(&sensor->sd, client, &ark7116_subdev_ops);

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);

	if (ret)
		return ret;

	mutex_init(&sensor->lock);
	ark7116_initialization(sensor);
	ret = v4l2_async_register_subdev(&sensor->sd);
	if (ret)
		goto free_ctrls;

	return 0;

free_ctrls:
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static int ark7116_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ark7116_dev *sensor = to_ark7116_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);

	return 0;
}

static const struct i2c_device_id ark7116_id[] = {
	{"ark7116", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ark7116_id);

static const struct of_device_id ark7116_dt_ids[] = {
	{.compatible = "ar,ark7116"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, ark7116_dt_ids);

static struct i2c_driver ark7116_i2c_driver = {
	.driver = {
		   .name = "ark7116",
		   .of_match_table = ark7116_dt_ids,
		   },
	.id_table = ark7116_id,
	.probe = ark7116_probe,
	.remove = ark7116_remove,
};

module_i2c_driver(ark7116_i2c_driver);

MODULE_DESCRIPTION("ARK7116 Parallel Camera Subdev Driver");
MODULE_LICENSE("GPL");
