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
#include "sdrv-cam-core.h"
#include "mipicsi-os.h"


static struct i2c_client *n4_i2c_client = NULL;
static unsigned char n4_i2c_addr;
unsigned int mclk = 3; // 0:1242M 1:1188 2:756M 3:594M
unsigned int i2c = 1; //1:non-continue 0:continue

#define MAX_SENSOR_NUM 4

#define n4_DEVICE_ID 0x31

#define N4_MCSI0_NAME "nextchip,n4"

#define N4_MCSI0_NAME_I2C "n4"

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

enum n4_mode_id {
	n4_MODE_720P_1280_720 = 0,
	n4_NUM_MODES,
};

enum n4_frame_rate {
	n4_25_FPS = 0,
	n4_NUM_FRAMERATES,
};

struct n4_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct n4_pixfmt n4_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static const int n4_framerates[] = {
	[n4_25_FPS] = 25,
};

struct n4_ctrls {
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

enum n4_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct n4_mode_info {
	enum n4_mode_id id;
	enum n4_downsize_mode dn_mode;
	u32 hact;
	u32 htot;
	u32 vact;
	u32 vtot;
	//const struct reg_value *reg_data;
	//u32 reg_data_size;
};

#define MIPI_CSI2_SENS_VC0_PAD_SOURCE   0
#define MIPI_CSI2_SENS_VC1_PAD_SOURCE   1
#define MIPI_CSI2_SENS_VC2_PAD_SOURCE   2
#define MIPI_CSI2_SENS_VC3_PAD_SOURCE   3
#define MIPI_CSI2_SENS_VCX_PADS_NUM     4

struct n4_dev {
	struct i2c_client *i2c_client;
	unsigned short addr_n4;
	struct i2c_adapter *adap;
	struct v4l2_subdev sd[MIPI_CSI2_SENS_VCX_PADS_NUM];
#if 1
	struct media_pad pads[MIPI_CSI2_SENS_VCX_PADS_NUM];
#else
	struct media_pad pad;
#endif
	struct v4l2_fwnode_endpoint ep;	/* the parsed DT endpoint info */

	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *pw_gpio;
	bool upside_down;
	uint32_t link_count;
	/* lock to protect all members below */
	struct mutex lock;

	int power_count;

	struct v4l2_mbus_framefmt fmt;
	bool pending_fmt_change;

	const struct n4_mode_info *current_mode;
	const struct n4_mode_info *last_mode;
	enum n4_frame_rate current_fr;

	struct v4l2_fract frame_interval;

	struct n4_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
	int sec_9286_shift;
	u32 sync;
};

static const struct n4_mode_info
	n4_mode_data[n4_NUM_FRAMERATES][n4_NUM_MODES] = {
	{
		{
			n4_MODE_720P_1280_720, SUBSAMPLING,
			1280, 1892, 720, 740,
		},
	},
};

static inline struct n4_dev *to_n4_dev(struct v4l2_subdev *sd)
{
	return sd->host_priv;
}

static int n4_i2c_write(u8 reg, u8 val)
{
	struct i2c_client *client = n4_i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = n4_i2c_addr;
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

static int n4_i2c_read(u8 reg, u8 *val)
{
	struct i2c_client *client = n4_i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = n4_i2c_addr;
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


static int n4_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static int n4_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct n4_dev *sensor = to_n4_dev(sd);

	if (fse->pad != 0)
		return -EINVAL;

	if (fse->index >= n4_NUM_MODES)
		return -EINVAL;

	fse->min_width = n4_mode_data[0][fse->index].hact;
	fse->max_width = fse->min_width;

	if (sensor->sync == 0)
		fse->min_height = n4_mode_data[0][fse->index].vact;
	else
		fse->min_height = n4_mode_data[0][fse->index].vact * 4;

	fse->max_height = fse->min_height;
	return 0;
}

static int n4_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum
				       *fie)
{
	struct v4l2_fract tpf;

	if (fie->pad != 0)
		return -EINVAL;

	if (fie->index >= n4_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = n4_framerates[fie->index];

	fie->interval = tpf;

	return 0;
}

static int n4_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct n4_dev *sensor = to_n4_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int n4_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

static int n4_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	if (code->index >= ARRAY_SIZE(n4_formats))
		return -EINVAL;

	code->code = n4_formats[code->index].code;

	return 0;
}

static int n4_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct n4_dev *sensor = to_n4_dev(sd);
	struct i2c_client *client = sensor->i2c_client;
	//int ret = 0;
	//sensor->i2c_client->addr = sensor->addr_n4;

	if (enable == 1) {
		//csienable=1
		//n4_write_reg(sensor, 0x15, 0x9b);
		dev_info(&client->dev, "%s(): enable \n", __func__);
	} else if (enable == 0) {
		//csienable=0
		//n4_write_reg(sensor, 0x15, 0x13);
		dev_info(&client->dev, "%s(): disabled \n", __func__);
	}

	return 0;
}

static int n4_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct n4_dev *sensor = to_n4_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	fmt = &sensor->fmt;

	format->format = *fmt;

	mutex_unlock(&sensor->lock);

	return 0;
}

static int n4_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	return 0;
}

#if 0
static int n4_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops n4_core_ops = {
	.s_power = n4_s_power,
};

static const struct v4l2_subdev_video_ops n4_video_ops = {
	.g_frame_interval = n4_g_frame_interval,
	.s_frame_interval = n4_s_frame_interval,
	.s_stream = n4_s_stream,
};

static const struct v4l2_subdev_pad_ops n4_pad_ops = {
	.enum_mbus_code = n4_enum_mbus_code,
	.get_fmt = n4_get_fmt,
	.set_fmt = n4_set_fmt,
	.enum_frame_size = n4_enum_frame_size,
	.enum_frame_interval = n4_enum_frame_interval,
};

static const struct v4l2_subdev_ops n4_subdev_ops = {
	.core = &n4_core_ops,
	.video = &n4_video_ops,
	.pad = &n4_pad_ops,
};

static void n4_reset(struct n4_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->reset_gpio, enable ? 1 : 0);
}

static void n4_power(struct n4_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->pwdn_gpio, enable ? 1 : 0);
}
static void n4_power_cvbs(struct n4_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->pw_gpio, enable ? 1 : 0);
}
static int n4_check_chip_id(struct n4_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	//n4_power(sensor, 1);

	ret = n4_i2c_read(0x1e, &chip_id);

	if (ret) {
		dev_err(&client->dev, "%s: failed to read chip identifier\n",
			__func__);
		//goto power_off;
	} else {
		dev_info(&client->dev,
			 "%s(): check_chip id succsed chip_id=%x\n", chip_id);
	}
/*
	if (chip_id != n4_DEVICE_ID) {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(n4), got 0x%x\n",
			__func__, n4_DEVICE_ID, chip_id);
		ret = -ENXIO;
	}
*/
//power_off:
//  n4_power(sensor, 0);

	return ret;
}

static void n4_common_settings(void)
{
	dev_info(&n4_i2c_client->dev, "n4_common_settings ENTER\n");

	#if 1 //add for reinitial when not power off
	n4_i2c_write(0xff, 0x20);
	n4_i2c_write(0x00, 0x00);
	n4_i2c_write(0x40, 0x11);
	n4_i2c_write(0x40, 0x00);

	n4_i2c_write(0xff, 0x01);
	n4_i2c_write(0x81, 0x41);
	n4_i2c_write(0x80, 0x61);
	n4_i2c_write(0x80, 0x60);
	n4_i2c_write(0x81, 0x40);
	#endif

	n4_i2c_write(0xff, 0x04);
	n4_i2c_write(0xa0, 0x24);
	n4_i2c_write(0xa1, 0x24);
	n4_i2c_write(0xa2, 0x24);
	n4_i2c_write(0xa3, 0x24);
	n4_i2c_write(0xa4, 0x24);
	n4_i2c_write(0xa5, 0x24);
	n4_i2c_write(0xa6, 0x24);
	n4_i2c_write(0xa7, 0x24);
	n4_i2c_write(0xa8, 0x24);
	n4_i2c_write(0xa9, 0x24);
	n4_i2c_write(0xaa, 0x24);
	n4_i2c_write(0xab, 0x24);
	n4_i2c_write(0xac, 0x24);
	n4_i2c_write(0xad, 0x24);
	n4_i2c_write(0xae, 0x24);
	n4_i2c_write(0xaf, 0x24);
	n4_i2c_write(0xb0, 0x24);
	n4_i2c_write(0xb1, 0x24);
	n4_i2c_write(0xb2, 0x24);
	n4_i2c_write(0xb3, 0x24);
	n4_i2c_write(0xb4, 0x24);
	n4_i2c_write(0xb5, 0x24);
	n4_i2c_write(0xb6, 0x24);
	n4_i2c_write(0xb7, 0x24);
	n4_i2c_write(0xb8, 0x24);
	n4_i2c_write(0xb9, 0x24);
	n4_i2c_write(0xba, 0x24);
	n4_i2c_write(0xbb, 0x24);
	n4_i2c_write(0xbc, 0x24);
	n4_i2c_write(0xbd, 0x24);
	n4_i2c_write(0xbe, 0x24);
	n4_i2c_write(0xbf, 0x24);
	n4_i2c_write(0xc0, 0x24);
	n4_i2c_write(0xc1, 0x24);
	n4_i2c_write(0xc2, 0x24);
	n4_i2c_write(0xc3, 0x24);

	n4_i2c_write(0xff, 0x21);
	n4_i2c_write(0x07, 0x80);
	n4_i2c_write(0x07, 0x00);

	n4_i2c_write(0xff, 0x0A);
	n4_i2c_write(0x77, 0x8F);
	n4_i2c_write(0xF7, 0x8F);
	n4_i2c_write(0xff, 0x0B);
	n4_i2c_write(0x77, 0x8F);
	n4_i2c_write(0xF7, 0x8F);

	n4_i2c_write(0xFF, 0x01);
	n4_i2c_write(0xCC, 0x64);
	n4_i2c_write(0xCD, 0x64);
	n4_i2c_write(0xCE, 0x64);
	n4_i2c_write(0xCF, 0x64);
//	n4_i2c_write(0xC8, 0x00);
//	n4_i2c_write(0xC9, 0x00);
//	n4_i2c_write(0xCA, 0x00);
//	n4_i2c_write(0xCB, 0x00);

	n4_i2c_write(0xFF, 0x21);
	if(mclk == 3)
	{
		n4_i2c_write(0x40, 0xAC);
		n4_i2c_write(0x41, 0x10);
		n4_i2c_write(0x42, 0x03);
		n4_i2c_write(0x43, 0x43);
		n4_i2c_write(0x11, 0x04);
		n4_i2c_write(0x10, 0x0A);
		n4_i2c_write(0x12, 0x06);
		n4_i2c_write(0x13, 0x09);
		n4_i2c_write(0x17, 0x01);
		n4_i2c_write(0x18, 0x0D);
		n4_i2c_write(0x15, 0x04);
		n4_i2c_write(0x14, 0x16);
		n4_i2c_write(0x16, 0x05);
		n4_i2c_write(0x19, 0x05);
		n4_i2c_write(0x1A, 0x0A);
		n4_i2c_write(0x1B, 0x08);
		n4_i2c_write(0x1C, 0x07);
	}
	else if(mclk == 1)
	{	n4_i2c_write(0x40, 0xd8);
		n4_i2c_write(0x41, 0x00);
		n4_i2c_write(0x42, 0x04);
		n4_i2c_write(0x43, 0x43);
		n4_i2c_write(0x11, 0x08);
		n4_i2c_write(0x10, 0x12);
		n4_i2c_write(0x12, 0x0b);
		n4_i2c_write(0x13, 0x11);
		n4_i2c_write(0x17, 0x02);
		n4_i2c_write(0x18, 0x11);
		n4_i2c_write(0x15, 0x07);
		n4_i2c_write(0x14, 0x2b);
		n4_i2c_write(0x16, 0x0a);
		n4_i2c_write(0x19, 0x09);
		n4_i2c_write(0x1A, 0x14);
		n4_i2c_write(0x1B, 0x10);
		n4_i2c_write(0x1C, 0x0d);
	}
	else
	{
		n4_i2c_write(0x40, 0xB4);
		n4_i2c_write(0x41, 0x00);
		n4_i2c_write(0x42, 0x03);
		n4_i2c_write(0x43, 0x43);
		n4_i2c_write(0x11, 0x08);
		n4_i2c_write(0x10, 0x13);
		n4_i2c_write(0x12, 0x0B);
		n4_i2c_write(0x13, 0x12);
		n4_i2c_write(0x17, 0x02);
		n4_i2c_write(0x18, 0x12);
		n4_i2c_write(0x15, 0x07);
		n4_i2c_write(0x14, 0x2D);
		n4_i2c_write(0x16, 0x0B);
		n4_i2c_write(0x19, 0x09);
		n4_i2c_write(0x1A, 0x15);
		n4_i2c_write(0x1B, 0x11);
		n4_i2c_write(0x1C, 0x0E);
	}
	n4_i2c_write(0x44, 0x00);
	n4_i2c_write(0x49, 0xF3);
	n4_i2c_write(0x49, 0xF0);
	n4_i2c_write(0x44, 0x02);
	if(i2c==0)
		n4_i2c_write(0x08, 0x48); //0x40:non-continue;0x48:continuous
	else
		n4_i2c_write(0x08, 0x40); //0x40:non-continue;0x48:continuous
	n4_i2c_write(0x0F, 0x01);
	n4_i2c_write(0x38, 0x1E);
	n4_i2c_write(0x39, 0x1E);
	n4_i2c_write(0x3A, 0x1E);
	n4_i2c_write(0x3B, 0x1E);
	n4_i2c_write(0x07, 0x0F); //0x0f:4lane  0x07:2lane
	n4_i2c_write(0x2D, 0x01); //0x01:4lane  0x00:2lane
	n4_i2c_write(0x45, 0x02);

	n4_i2c_write(0xFF, 0x13);
	n4_i2c_write(0x30, 0x00);
	n4_i2c_write(0x31, 0x00);
	n4_i2c_write(0x32, 0x00);

	n4_i2c_write(0xFF, 0x09);
	n4_i2c_write(0x40, 0x00);
	n4_i2c_write(0x41, 0x00);
	n4_i2c_write(0x42, 0x00);
	n4_i2c_write(0x43, 0x00);
	n4_i2c_write(0x44, 0x00);
	n4_i2c_write(0x45, 0x00);
	n4_i2c_write(0x46, 0x00);
	n4_i2c_write(0x47, 0x00);
	n4_i2c_write(0x50, 0x30);
	n4_i2c_write(0x51, 0x6f);
	n4_i2c_write(0x52, 0x67);
	n4_i2c_write(0x53, 0x48);
	n4_i2c_write(0x54, 0x30);
	n4_i2c_write(0x55, 0x6f);
	n4_i2c_write(0x56, 0x67);
	n4_i2c_write(0x57, 0x48);
	n4_i2c_write(0x58, 0x30);
	n4_i2c_write(0x59, 0x6f);
	n4_i2c_write(0x5a, 0x67);
	n4_i2c_write(0x5b, 0x48);
	n4_i2c_write(0x5c, 0x30);
	n4_i2c_write(0x5d, 0x6f);
	n4_i2c_write(0x5e, 0x67);
	n4_i2c_write(0x5f, 0x48);
	n4_i2c_write(0x96, 0x00);
	n4_i2c_write(0x97, 0x00);
	n4_i2c_write(0x98, 0x00);
	n4_i2c_write(0x99, 0x00);
	n4_i2c_write(0x9a, 0x00);
	n4_i2c_write(0x9b, 0x00);
	n4_i2c_write(0x9c, 0x00);
	n4_i2c_write(0x9d, 0x00);
	n4_i2c_write(0x9e, 0x00);
	n4_i2c_write(0xb6, 0x00);
	n4_i2c_write(0xb7, 0x00);
	n4_i2c_write(0xb8, 0x00);
	n4_i2c_write(0xb9, 0x00);
	n4_i2c_write(0xba, 0x00);
	n4_i2c_write(0xbb, 0x00);
	n4_i2c_write(0xbc, 0x00);
	n4_i2c_write(0xbd, 0x00);
	n4_i2c_write(0xbe, 0x00);
	n4_i2c_write(0xd6, 0x00);
	n4_i2c_write(0xd7, 0x00);
	n4_i2c_write(0xd8, 0x00);
	n4_i2c_write(0xd9, 0x00);
	n4_i2c_write(0xda, 0x00);
	n4_i2c_write(0xdb, 0x00);
	n4_i2c_write(0xdc, 0x00);
	n4_i2c_write(0xdd, 0x00);
	n4_i2c_write(0xde, 0x00);
	n4_i2c_write(0xf6, 0x00);
	n4_i2c_write(0xf7, 0x00);
	n4_i2c_write(0xf8, 0x00);
	n4_i2c_write(0xf9, 0x00);
	n4_i2c_write(0xfa, 0x00);
	n4_i2c_write(0xfb, 0x00);
	n4_i2c_write(0xfc, 0x00);
	n4_i2c_write(0xfd, 0x00);
	n4_i2c_write(0xfe, 0x00);

	n4_i2c_write(0xff, 0x0a);
	n4_i2c_write(0x3d, 0x00);
	n4_i2c_write(0x3c, 0x00);
	n4_i2c_write(0x30, 0xac);
	n4_i2c_write(0x31, 0x78);
	n4_i2c_write(0x32, 0x17);
	n4_i2c_write(0x33, 0xc1);
	n4_i2c_write(0x34, 0x40);
	n4_i2c_write(0x35, 0x00);
	n4_i2c_write(0x36, 0xc3);
	n4_i2c_write(0x37, 0x0a);
	n4_i2c_write(0x38, 0x00);
	n4_i2c_write(0x39, 0x02);
	n4_i2c_write(0x3a, 0x00);
	n4_i2c_write(0x3b, 0xb2);
	n4_i2c_write(0x25, 0x10);
	n4_i2c_write(0x27, 0x1e);
	n4_i2c_write(0xbd, 0x00);
	n4_i2c_write(0xbc, 0x00);
	n4_i2c_write(0xb0, 0xac);
	n4_i2c_write(0xb1, 0x78);
	n4_i2c_write(0xb2, 0x17);
	n4_i2c_write(0xb3, 0xc1);
	n4_i2c_write(0xb4, 0x40);
	n4_i2c_write(0xb5, 0x00);
	n4_i2c_write(0xb6, 0xc3);
	n4_i2c_write(0xb7, 0x0a);
	n4_i2c_write(0xb8, 0x00);
	n4_i2c_write(0xb9, 0x02);
	n4_i2c_write(0xba, 0x00);
	n4_i2c_write(0xbb, 0xb2);
	n4_i2c_write(0xa5, 0x10);
	n4_i2c_write(0xa7, 0x1e);

	n4_i2c_write(0xff, 0x0b);
	n4_i2c_write(0x3d, 0x00);
	n4_i2c_write(0x3c, 0x00);
	n4_i2c_write(0x30, 0xac);
	n4_i2c_write(0x31, 0x78);
	n4_i2c_write(0x32, 0x17);
	n4_i2c_write(0x33, 0xc1);
	n4_i2c_write(0x34, 0x40);
	n4_i2c_write(0x35, 0x00);
	n4_i2c_write(0x36, 0xc3);
	n4_i2c_write(0x37, 0x0a);
	n4_i2c_write(0x38, 0x00);
	n4_i2c_write(0x39, 0x02);
	n4_i2c_write(0x3a, 0x00);
	n4_i2c_write(0x3b, 0xb2);
	n4_i2c_write(0x25, 0x10);
	n4_i2c_write(0x27, 0x1e);
	n4_i2c_write(0xbd, 0x00);
	n4_i2c_write(0xbc, 0x00);
	n4_i2c_write(0xb0, 0xac);
	n4_i2c_write(0xb1, 0x78);
	n4_i2c_write(0xb2, 0x17);
	n4_i2c_write(0xb3, 0xc1);
	n4_i2c_write(0xb4, 0x40);
	n4_i2c_write(0xb5, 0x00);
	n4_i2c_write(0xb6, 0xc3);
	n4_i2c_write(0xb7, 0x0a);
	n4_i2c_write(0xb8, 0x00);
	n4_i2c_write(0xb9, 0x02);
	n4_i2c_write(0xba, 0x00);
	n4_i2c_write(0xbb, 0xb2);
	n4_i2c_write(0xa5, 0x10);
	n4_i2c_write(0xa7, 0x1e);

	n4_i2c_write(0xFF, 0x21);
	n4_i2c_write(0x3E, 0x00);
	n4_i2c_write(0x3F, 0x00);
}


#define CH_VALUE_NTSC				0x00 //720*480i@cvbs-ntsc
#define CH_VALUE_PAL				0x10 //720*576i@cbvs-pal
#define CH_VALUE_HD25				0x21 //1280*720p@25
#define CH_VALUE_HD30				0x20 //1280*720p@30
#define CH_VALUE_FHD25				0x31 //1920*1080p@25
#define CH_VALUE_FHD30				0x30 //1920*1080p@30

static int ch_fhd_mode[4]={0,0,0,0};

static void n4_init_ch(unsigned char ch, unsigned char vfmt)
{
	dev_info(&n4_i2c_client->dev,"int ch = %d,vfmt = 0x%x\n",ch,vfmt);
	n4_i2c_write(0xFF, 0x00);
	n4_i2c_write(0x00+ch, 0x10);
	if ((CH_VALUE_HD25 == vfmt) || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25 == vfmt) || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x04+ch, 0x00);
	} else if (CH_VALUE_PAL==vfmt) {
		n4_i2c_write(0x04+ch, 0x05);
	} else if (CH_VALUE_NTSC==vfmt) {
		n4_i2c_write(0x04+ch, 0x04);
	}
	if (CH_VALUE_HD25 == vfmt) {
		n4_i2c_write(0x08+ch, 0x0D);
	} else if (CH_VALUE_HD30==vfmt) {
		n4_i2c_write(0x08+ch, 0x0C);
	} else if (CH_VALUE_FHD25 == vfmt) {
		n4_i2c_write(0x08+ch, 0x03);
	} else if (CH_VALUE_FHD30==vfmt) {
		n4_i2c_write(0x08+ch, 0x02);
	} else {
		n4_i2c_write(0x08+ch, 0x00);
	}

	n4_i2c_write(0x0c+ch, 0x00);
	if ((CH_VALUE_HD25 == vfmt) || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25 == vfmt) || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x10+ch, 0x20);
	} else if (CH_VALUE_PAL == vfmt) {
		n4_i2c_write(0x10+ch, 0xdd);
	} else if (CH_VALUE_NTSC== vfmt) {
		n4_i2c_write(0x10+ch, 0xa0);
	}
	n4_i2c_write(0x14+ch, 0x00);
	n4_i2c_write(0x18+ch, 0x11);
	n4_i2c_write(0x1c+ch, 0x1a);
	n4_i2c_write(0x20+ch, 0x00);
	if ((CH_VALUE_HD25 == vfmt) || (CH_VALUE_HD30 == vfmt)){
		n4_i2c_write(0x24+ch, 0x88);
		n4_i2c_write(0x28+ch, 0x84);
		n4_i2c_write(0x30+ch, 0x03);
		n4_i2c_write(0x34+ch, 0x0f);
	} else if ((CH_VALUE_FHD25 == vfmt) || (CH_VALUE_FHD30 == vfmt)){
		n4_i2c_write(0x24+ch, 0x86);
		n4_i2c_write(0x28+ch, 0x80);
		n4_i2c_write(0x30+ch, 0x00);
		n4_i2c_write(0x34+ch, 0x00);
	} else {
		n4_i2c_write(0x24+ch, 0x90);
		n4_i2c_write(0x28+ch, 0x90);
		n4_i2c_write(0x30+ch, 0x00);
		n4_i2c_write(0x34+ch, 0x00);
	}
	n4_i2c_write(0x40+ch, 0x00);
	n4_i2c_write(0x44+ch, 0x00);
	n4_i2c_write(0x48+ch, 0x00);
	if ((CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x4c+ch, 0xfe);
		n4_i2c_write(0x50+ch, 0xfb);
	} else {
		n4_i2c_write(0x4c+ch, 0x00);
		n4_i2c_write(0x50+ch, 0x00);
	}
	n4_i2c_write(0x58+ch, 0x80);
	n4_i2c_write(0x5c+ch, 0x82);
	n4_i2c_write(0x60+ch, 0x10);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x64+ch, 0x05);
	} else if (CH_VALUE_PAL == vfmt) {
		n4_i2c_write(0x64+ch, 0x0a);
	} else if (CH_VALUE_NTSC== vfmt) {
		n4_i2c_write(0x64+ch, 0x1c);
	}
	if ((CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x68+ch, 0x48);
	} else if (CH_VALUE_HD25 == vfmt) {
		n4_i2c_write(0x68+ch, 0x38);
	} else if (CH_VALUE_HD30== vfmt) {
		n4_i2c_write(0x68+ch, 0x3e);
	} else if (CH_VALUE_PAL == vfmt) {
		n4_i2c_write(0x68+ch, 0xd8);
	} else if (CH_VALUE_NTSC == vfmt) {
		n4_i2c_write(0x68+ch, 0xd4);
	}
	n4_i2c_write(0x6c+ch, 0x00);
	if ((CH_VALUE_NTSC== vfmt)  || (CH_VALUE_PAL == vfmt)) {
		n4_i2c_write(0x70+ch, 0x1e);
	}
	n4_i2c_write(0x78+ch, 0x21);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x7c+ch, 0x00);
	} else {
		n4_i2c_write(0x7c+ch, 0x8f);
	}
	n4_i2c_write(0xFF, 0x01);
	n4_i2c_write(0x7C, 0x00);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x84+ch, 0x04);
		n4_i2c_write(0x88+ch, 0x01);
		n4_i2c_write(0x8c+ch, 0x02);
		n4_i2c_write(0xa0+ch, 0x00);
	} else {
		n4_i2c_write(0x84+ch, 0x06);
		n4_i2c_write(0x88+ch, 0x07);
		n4_i2c_write(0x8c+ch, 0x01);
		n4_i2c_write(0xa0+ch, 0x10);
		n4_i2c_write(0xcc+ch, 0x40);
	}
	n4_i2c_write(0xEC+ch, 0x00);

	n4_i2c_write(0xFF, 0x05+ch);
	n4_i2c_write(0x00, 0xd0);
	n4_i2c_write(0x01, 0x2c);
	if (CH_VALUE_PAL == vfmt || CH_VALUE_NTSC == vfmt) {
		n4_i2c_write(0x05, 0x24); //2021-01-22
		n4_i2c_write(0x08, 0x50);
		n4_i2c_write(0x10, 0x00);
		n4_i2c_write(0x11, 0x00);
	}
	else{
		n4_i2c_write(0x05, 0x24);
		n4_i2c_write(0x08, 0x5A);
		n4_i2c_write(0x10, 0x06);
		n4_i2c_write(0x11, 0x06);
	}
	n4_i2c_write(0x1d, 0x0c);
	n4_i2c_write(0x24, 0x2a);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x25, 0xdc);
	} else if (CH_VALUE_PAL == vfmt) {
		n4_i2c_write(0x25, 0xcc);
	} else if (CH_VALUE_NTSC == vfmt) {
		n4_i2c_write(0x25, 0xdc);
	}
	n4_i2c_write(0x26, 0x40);
	n4_i2c_write(0x27, 0x57);
	n4_i2c_write(0x28, 0x80);
	n4_i2c_write(0x2b, 0xa8);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x31, 0x82);
	} else if (CH_VALUE_PAL == vfmt) {
		n4_i2c_write(0x31, 0x02);
	} else if (CH_VALUE_NTSC == vfmt) {
		n4_i2c_write(0x31, 0x82);
	}
	n4_i2c_write(0x32, 0x10);
	if ((CH_VALUE_FHD25 == vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x38, 0x13);
		n4_i2c_write(0x47, 0xEE);
		n4_i2c_write(0x50, 0xc6);
	} else if ((CH_VALUE_HD25 == vfmt)  || (CH_VALUE_HD30 == vfmt)) {
		n4_i2c_write(0x38, 0x10);
		n4_i2c_write(0x47, 0xEE);
		n4_i2c_write(0x50, 0xc6);
	} else if (CH_VALUE_PAL == vfmt) {
		n4_i2c_write(0x38, 0x1f);
		n4_i2c_write(0x47, 0x04);
		n4_i2c_write(0x50, 0x84);
	} else {
		n4_i2c_write(0x38, 0x1d);
		n4_i2c_write(0x47, 0x04);
		n4_i2c_write(0x50, 0x84);
	}
	n4_i2c_write(0x53, 0x00);
	n4_i2c_write(0x57, 0x00);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x58, 0x77);
	} else {
		n4_i2c_write(0x58, 0x70);
	}
	n4_i2c_write(0x59, 0x00);
	n4_i2c_write(0x5C, 0x78);
	n4_i2c_write(0x5F, 0x00);
	n4_i2c_write(0x62, 0x20);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x64, 0x00);
	} else {
		n4_i2c_write(0x64, 0x01);
	}
	n4_i2c_write(0x65, 0x00);
	n4_i2c_write(0x69, 0x00);
	n4_i2c_write(0x82, 0x00);  //important!!!!
	if ((CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x6E, 0x00);
		n4_i2c_write(0x6F, 0x00);
		n4_i2c_write(0x90, 0x01);
	} else if (CH_VALUE_HD30 == vfmt) {
		n4_i2c_write(0x6E, 0x10);
		n4_i2c_write(0x6F, 0x1C);
		n4_i2c_write(0x90, 0x01);
	} else if (CH_VALUE_HD25 == vfmt) {
		n4_i2c_write(0x6E, 0x00);
		n4_i2c_write(0x6F, 0x00);
		n4_i2c_write(0x90, 0x01);
	} else {
		n4_i2c_write(0x6E, 0x00);
		n4_i2c_write(0x6F, 0x00);
		if ( CH_VALUE_PAL== vfmt) {
			n4_i2c_write(0x90, 0x0d);
		} else {
			n4_i2c_write(0x90, 0x01);
		}
	}
	n4_i2c_write(0x92, 0x00);
	n4_i2c_write(0x94, 0x00);
	n4_i2c_write(0x95, 0x00);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0xa9, 0x00);
		n4_i2c_write(0xb5, 0x80);
	} else if (CH_VALUE_PAL == vfmt) {
		n4_i2c_write(0xa9, 0x0a);
		n4_i2c_write(0xb5, 0x00);
	} else {
		n4_i2c_write(0xa9, 0x1c);
		n4_i2c_write(0xb5, 0x00);
	}
	n4_i2c_write(0xb7, 0xfc);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0xb8, 0x39);
	}else{
		n4_i2c_write(0xb8, 0xb9);
	}
	n4_i2c_write(0xb9, 0x72);
	n4_i2c_write(0xbb, 0x0f);
	n4_i2c_write(0xd1, 0x30);
	n4_i2c_write(0xd5, 0x80);

	n4_i2c_write(0xFF, 0x09);
	if ((CH_VALUE_HD25== vfmt)  || (CH_VALUE_HD30 == vfmt) || (CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		n4_i2c_write(0x96+ch*0x20, 0x00);
		n4_i2c_write(0x97+ch*0x20, 0x00);
	} else {
		n4_i2c_write(0x96+ch*0x20, 0x10);
		n4_i2c_write(0x97+ch*0x20, 0x10);
	}

	if ((CH_VALUE_FHD25== vfmt)  || (CH_VALUE_FHD30 == vfmt)) {
		ch_fhd_mode[ch] = 1;
	} else {
		ch_fhd_mode[ch] = 0;
	}
}


static void n4_mipi_outputs(int mipi_ch_en)
{
	int ch;
	unsigned char reg_value;
	int ret;
	dev_info(&n4_i2c_client->dev,"n4_mipi_outputs mipi_ch_en = 0x%x\n",mipi_ch_en);
	n4_i2c_write(0xFF, 0x20);
	ret = n4_i2c_read(0x01, &reg_value);
	if(ret < 0) {
		dev_err(&n4_i2c_client->dev, "read reg[0x01] failed \n");
	}
	for(ch=0;ch<4;ch++){
		if( 1 == ch_fhd_mode[ch] ) {
			reg_value &= (~(0x03<<(ch*2)));
		} else {
			reg_value |= (0x01<<(ch*2));
		}
	}
	n4_i2c_write(0x01, reg_value); //0x55
	n4_i2c_write(0x00, 0x00);
	//n4_i2c_write(0x40, 0x01);
	n4_i2c_write(0x0F, 0x00);
	n4_i2c_write(0x0D, 0x01);  //4lane mode
	//n4_i2c_write(0x40, 0x00);
	n4_i2c_write(0x00, mipi_ch_en);  //ch1/2/3/4 enabled FF
	n4_i2c_write(0x40, 0x01);
	n4_i2c_write(0x40, 0x00);

	n4_i2c_write(0xFF, 0x01);
	n4_i2c_write(0xC8, 0x00);
	n4_i2c_write(0xC9, 0x00);
	n4_i2c_write(0xCA, 0x00);
	n4_i2c_write(0xCB, 0x00);
}

static int n4_init(void)
{
    n4_common_settings();

    n4_init_ch(0,CH_VALUE_HD25);
    n4_init_ch(1,CH_VALUE_HD25);
    n4_init_ch(2,CH_VALUE_HD25);
    n4_init_ch(3,CH_VALUE_HD25);

    n4_mipi_outputs(0xFF);
	return 0;
}

static int n4_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	struct n4_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	u32 sync;
	int ret;
	int i,j;
	struct gpio_desc *gpiod;
	struct device_node *poc_np;
	struct device_node *subdev_of_node;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;
	n4_i2c_client = sensor->i2c_client;
	sensor->addr_n4 = client->addr;
	n4_i2c_addr = sensor->addr_n4;
	dev_err(&client->dev, "N4 n4_i2c_addr=0x%x\n", n4_i2c_addr);
	mutex_init(&sensor->lock);
	/*
	 * default init sequence initialize sensor to
	 * YUV422 UYVY VGA@30fps
	 */
	fmt = &sensor->fmt;
	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	//fmt->code = MEDIA_BUS_FMT_RGB565_2X8_LE;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = 1280;
	fmt->height = 720;
	fmt->field = V4L2_FIELD_NONE;
	sensor->frame_interval.numerator = 1;

	sensor->frame_interval.denominator = n4_framerates[n4_25_FPS];
	sensor->current_fr = n4_25_FPS;
	sensor->current_mode =
	    &n4_mode_data[n4_25_FPS][n4_MODE_720P_1280_720];

//  sensor->last_mode = sensor->current_mode;

	sensor->ae_target = 52;

	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "sync", &sync);
	dev_info(&client->dev, "sync: %d, ret=%d\n", sync, ret);

	if (ret < 0)
		sync = 0;

	sensor->sync = sync;


	poc_np = of_parse_phandle(client->dev.of_node, "semidrive,poc", 0);
	if (!poc_np) {
		dev_info(&client->dev, "no poc device node\n");
		sensor->adap = NULL;
	} else
		sensor->adap = of_find_i2c_adapter_by_node(poc_np);
	dev_info(&client->dev, "sensor->adap=%p\n", sensor->adap);


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

	sensor->pads[MIPI_CSI2_SENS_VC0_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC1_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC3_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	for (i = 0; i < (sync == 0 ? MIPI_CSI2_SENS_VCX_PADS_NUM : sync); i++) {
		v4l2_i2c_subdev_init(&sensor->sd[i], client,  &n4_subdev_ops);
		subdev_of_node = of_graph_get_port_by_id(client->dev.of_node,i);
		sensor->sd[i].fwnode = of_fwnode_handle(subdev_of_node);
		v4l2_set_subdev_hostdata(&sensor->sd[i], sensor);
		sensor->sd[i].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		sensor->sd[i].entity.function = MEDIA_ENT_F_CAM_SENSOR;
		sensor->pads[i].flags = MEDIA_PAD_FL_SOURCE;
		ret = media_entity_pads_init(&sensor->sd[i].entity,
					     4,sensor->pads);
		if (ret)
			goto mutex_des;
	}


	for (i = 0; i < (sync == 0 ? MIPI_CSI2_SENS_VCX_PADS_NUM : sync); i++) {
		ret = v4l2_async_register_subdev(&sensor->sd[i]);
		if (ret)
			goto entity_cleanup;
	}

	if (ret) {
		dev_info(dev, "media_entity_pads_init faild \n");
		return ret;
	}

	//power gpio GPIO_H1 index(123)
	gpiod = devm_gpiod_get_optional(&client->dev, "pwdn", GPIOD_OUT_HIGH);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"pwdn", ret);

		//return ret;
		gpiod = NULL;
	}
	sensor->pwdn_gpio = gpiod;

	//rst gpio index(79) gpio D15
	gpiod = devm_gpiod_get_optional(&client->dev, "rst", GPIOD_OUT_HIGH);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"rst", ret);

		gpiod = NULL;
	}
	sensor->reset_gpio = gpiod;

	//power gpio gpio B4 index(16)
	gpiod = devm_gpiod_get_optional(&client->dev, "pw", GPIOD_OUT_HIGH);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"pw", ret);

		//return ret;
		gpiod = NULL;
	}
	sensor->pw_gpio = gpiod;

	if (sensor->pwdn_gpio != NULL) {
		n4_power(sensor, true);
		usleep_range(10000, 11000);
	}
	if (sensor->pw_gpio != NULL) {
		n4_power_cvbs(sensor, true);
		//usleep_range(10000, 11000);
	}

	n4_reset(sensor, false);
	usleep_range(10000, 11000);
	n4_reset(sensor, true);
	usleep_range(10000, 11000);

	ret = n4_check_chip_id(sensor);
	if (ret == 0) {
		n4_init();
		dev_err(&client->dev, "N4 init....\n");
	}
	return 0;

entity_cleanup:
	for (j = 0; j < i; j++) {
		media_entity_cleanup(&sensor->sd[j].entity);
	}
mutex_des:

	mutex_destroy(&sensor->lock);
	return ret;
}

static int n4_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct n4_dev *sensor = to_n4_dev(sd);
	int i;

	for (i = 0; i < (sensor->sync == 0 ? MIPI_CSI2_SENS_VCX_PADS_NUM : sensor->sync); i++) {

		v4l2_async_unregister_subdev(&sensor->sd[i]);

		media_entity_cleanup(&sensor->sd[i].entity);
	}
	mutex_destroy(&sensor->lock);


	return 0;
}

static const struct i2c_device_id n4_id[] = {
	{"n4", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, n4_id);

static const struct of_device_id n4_dt_ids[] = {
	{.compatible = N4_MCSI0_NAME},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, n4_dt_ids);

static struct i2c_driver n4_i2c_driver = {
	.driver = {
		   .name = "n4",
		   .of_match_table = n4_dt_ids,
		   },
	.id_table = n4_id,
	.probe = n4_probe,
	.remove = n4_remove,
};

module_i2c_driver(n4_i2c_driver);

MODULE_DESCRIPTION("N4 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
