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

#include "sdrv-csi.h"
#include "sdrv-mipi-csi2.h"

#define MAX_SENSOR_NUM 1

#define MAX96706_DEVICE_ID 0x4A

#define MAX96705_SLAVE_ID 0x40
#define MAX96705_DEV_INDEX -9
#define MAX96705_DEVICE_ID 0x41

#define AP0101_SLAVE_ID 0x5d
#define AP0101_DEV_INDEX -9

enum max96706_mode_id {
	MAX96706_MODE_720P_1280_720 = 0,
	MAX96706_NUM_MODES,
};

enum max96706_frame_rate {
	MAX96706_25_FPS = 0,
	MAX96706_NUM_FRAMERATES,
};

struct max96706_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct max96706_pixfmt max96706_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static const int max96706_framerates[] = {
	[MAX96706_25_FPS] = 25,
};

struct max96706_ctrls {
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

enum max96706_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct max96706_mode_info {
	enum max96706_mode_id id;
	enum max96706_downsize_mode dn_mode;
	u32 hact;
	u32 htot;
	u32 vact;
	u32 vtot;
};

struct max96706_dev {
	struct i2c_client *i2c_client;
	unsigned short addr_96706;
	unsigned short addr_96705;
	unsigned short addr_0101;
	struct v4l2_subdev sd;

	struct media_pad pad;

	struct v4l2_fwnode_endpoint ep;	/* the parsed DT endpoint info */

	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *gpi_gpio;
	struct gpio_desc *poc_gpio;

	bool upside_down;
	uint32_t link_count;
	/* lock to protect all members below */
	struct mutex lock;

	int power_count;
	int device_exist;

	struct v4l2_mbus_framefmt fmt;
	bool pending_fmt_change;

	const struct max96706_mode_info *current_mode;
	const struct max96706_mode_info *last_mode;
	enum max96706_frame_rate current_fr;

	struct v4l2_fract frame_interval;

	struct max96706_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
};

static const struct max96706_mode_info
max96706_mode_data[MAX96706_NUM_FRAMERATES][MAX96706_NUM_MODES] = {
	{
		{
			MAX96706_MODE_720P_1280_720, SUBSAMPLING,
			1280, 1892, 720, 740,
		},
	},
};

static inline struct max96706_dev *to_max96706_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96706_dev, sd);
}

static int max96706_write_reg(struct max96706_dev *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_96706;
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

static int max96706_read_reg(struct max96706_dev *sensor, u8 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_96706;
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

static int max96705_write_reg(struct max96706_dev *sensor, u8 idx, u8 reg,
			      u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_96705;
	buf[0] = reg;
	buf[1] = val;

	msg.addr = client->addr + idx;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
			__func__, reg, val);
		return ret;
	}

	return 0;
}

static int max96705_read_reg(struct max96706_dev *sensor, u8 idx, u8 reg,
			     u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_96705;
	buf[0] = reg;

	msg[0].addr = client->addr + idx;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr + idx;
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

static int ap0101_read_reg8(struct max96706_dev *sensor, u8 idx, u16 reg,
			    u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_0101;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr + idx;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr + idx;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	*val = ((u16) buf[0] << 8) | (u16) buf[1];

	return 0;

}

static int ap0101_write_reg8(struct max96706_dev *sensor, u8 idx, u16 reg,
			     u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	client->addr = sensor->addr_0101;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

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

static int ap0101_read_reg16(struct max96706_dev *sensor, u8 idx, u16 reg,
			     u16 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_0101;
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
		dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	*val = ((u16) buf[0] << 8) | (u16) buf[1];

	return 0;

}

static int ap0101_write_reg16(struct max96706_dev *sensor, u8 idx, u16 reg,
			      u16 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	client->addr = sensor->addr_0101;
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

static int ap0101_read_reg32(struct max96706_dev *sensor, u8 idx, u16 reg,
			     u32 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[4];
	int ret;

	client->addr = sensor->addr_0101;
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
		dev_err(&client->dev, "%s: error: reg=%x\n", __func__, reg);
		return ret;
	}

	*val =
	    ((u32) buf[0] << 24) | ((u32) buf[1] << 16) | ((u32) buf[2] << 8) |
	    (u32) buf[3];

	return 0;

}

static int ap0101_write_reg32(struct max96706_dev *sensor, u8 idx, u16 reg,
			      u32 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[6];
	int ret;

	client->addr = sensor->addr_0101;
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

static struct v4l2_subdev *max96706_get_interface_subdev(struct v4l2_subdev *sd)
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

static int max96706_s_power(struct v4l2_subdev *sd, int on)
{

	return 0;
}

static int max96706_enum_frame_size(struct v4l2_subdev *sd,
				    struct v4l2_subdev_pad_config *cfg,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->pad != 0)
		return -EINVAL;

	if (fse->index >= MAX96706_NUM_MODES)
		return -EINVAL;

	fse->min_width = max96706_mode_data[0][fse->index].hact;
	fse->max_width = fse->min_width;
	fse->min_height = max96706_mode_data[0][fse->index].vact;
	fse->max_height = fse->min_height;
	return 0;
}

static int max96706_enum_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_frame_interval_enum
					*fie)
{
	struct max96706_dev *sensor = to_max96706_dev(sd);
	struct v4l2_fract tpf;
	int ret;

	if (fie->pad != 0)
		return -EINVAL;

	if (fie->index >= MAX96706_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = max96706_framerates[fie->index];

	fie->interval = tpf;

	return 0;
}

static int max96706_g_frame_interval(struct v4l2_subdev *sd,
				     struct v4l2_subdev_frame_interval *fi)
{
	struct max96706_dev *sensor = to_max96706_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int max96706_s_frame_interval(struct v4l2_subdev *sd,
				     struct v4l2_subdev_frame_interval *fi)
{

	return 0;
}

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

static int max96706_s_stream(struct v4l2_subdev *sd, int enable)
{

	int i, idx;
	u8 val;
	struct max96706_dev *sensor = to_max96706_dev(sd);

	if (enable == 1) {
		max96706_read_reg(sensor, 0x04, &val);
		val &= ~(0x40);
		max96706_write_reg(sensor, 0x04, val);
		usleep_range(1000, 1100);
	} else if (enable == 0) {
		max96706_read_reg(sensor, 0x04, &val);
		val |= 0x40;
		max96706_write_reg(sensor, 0x04, val);
		usleep_range(1000, 1100);
	}

	return 0;
}

static int max96706_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct max96706_dev *sensor = to_max96706_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	fmt = &sensor->fmt;

	format->format = *fmt;

	mutex_unlock(&sensor->lock);

	return 0;
}

static int max96706_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct max96706_dev *sensor = to_max96706_dev(sd);

	if(format->format.code != sensor->fmt.code)
		return -EINVAL;

	return 0;
}

static int max96706_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops max96706_core_ops = {
	.s_power = max96706_s_power,
};

static const struct v4l2_subdev_video_ops max96706_video_ops = {
	.g_frame_interval = max96706_g_frame_interval,
	.s_frame_interval = max96706_s_frame_interval,
	.s_stream = max96706_s_stream,
};

static const struct v4l2_subdev_pad_ops max96706_pad_ops = {
	.enum_mbus_code = max96706_enum_mbus_code,
	.get_fmt = max96706_get_fmt,
	.set_fmt = max96706_set_fmt,
	.enum_frame_size = max96706_enum_frame_size,
	.enum_frame_interval = max96706_enum_frame_interval,
};

static const struct v4l2_subdev_ops max96706_subdev_ops = {
	.core = &max96706_core_ops,
	.video = &max96706_video_ops,
	.pad = &max96706_pad_ops,
};

static const struct v4l2_ctrl_ops max96706_ctrl_ops = {
	.s_ctrl = max96706_s_ctrl,
};

static const char *const test_pattern_menu[] = {
	"Disabled",
	"Color bars",
};

static int max96706_init_controls(struct max96706_dev *sensor)
{
	const struct v4l2_ctrl_ops *ops = &max96706_ctrl_ops;
	struct max96706_ctrls *ctrls = &sensor->ctrls;
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

static void max96706_power(struct max96706_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->pwdn_gpio, enable ? 1 : 0);
	usleep_range(10000, 10100);
	gpiod_direction_output(sensor->poc_gpio, enable ? 1 : 0);
	usleep_range(10000, 10100);
}

static int max96706_check_chip_id(struct max96706_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	ret = max96706_read_reg(sensor, 0x1e, &chip_id);

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

static int max96705_check_chip_id(struct max96706_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	ret = max96705_read_reg(sensor, 0, 0x1e, &chip_id);

	if (ret) {
		dev_err(&client->dev, "%s: failed to read chip identifier\n",
			__func__);
	}

	if (chip_id != MAX96705_DEVICE_ID) {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(max96705), got 0x%x\n",
			__func__, MAX96705_DEVICE_ID, chip_id);
		ret = -ENXIO;
	}

	return ret;
}

static int ap0101_change_config(struct max96706_dev *sensor, u8 idx)
{
	u16 reg;
	u16 val16;
	int i;

	reg = 0xfc00;
	val16 = 0x2800;
	ap0101_write_reg16(sensor, AP0101_DEV_INDEX, reg, val16);
	usleep_range(10000, 10100);

	reg = 0x0040;
	val16 = 0x8100;
	ap0101_write_reg16(sensor, AP0101_DEV_INDEX, reg, val16);
	usleep_range(10000, 10100);

	for (i = 0; i < 10; i++) {
		reg = 0x0040;
		val16 = 0;
		ap0101_read_reg16(sensor, AP0101_DEV_INDEX, reg, &val16);

		if (val16 == 0x0)
			return 0;

		usleep_range(10000, 10100);
	}

	return -1;
}

static int max96706_initialization(struct max96706_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 val;
	u8 link_status = 0, link_count = 0;
	int i = 0;

	dev_info(&client->dev, "%s()\n", __func__);

	//set him
	val = 0;
	max96706_read_reg(sensor, 0x06, &val);
	val |= 0x80;
	max96706_write_reg(sensor, 0x06, val);
	usleep_range(1000, 1100);

	//invert hsync
	val = 0;
	max96706_read_reg(sensor, 0x02, &val);
	val |= 0x80;
	max96706_write_reg(sensor, 0x02, val);
	usleep_range(1000, 1100);

	//disable output
	val = 0;
	max96706_read_reg(sensor, 0x04, &val);
	val |= 0x40;
	max96706_write_reg(sensor, 0x04, val);
	usleep_range(1000, 1100);

	msleep(500);
	ret = max96705_check_chip_id(sensor);

	if (ret != 0) {
		dev_err(&client->dev, "%s: not found 96705, ret=%d\n", __func__,
			ret);
		sensor->device_exist = -1;
		return -1;
	}

	sensor->device_exist = 1;

	max96705_write_reg(sensor, 0, 0x01, (sensor->addr_96706) << 1);
	usleep_range(5000, 5100);
	max96705_write_reg(sensor, 0, 0x00,
			   (sensor->addr_96705 + MAX96705_DEV_INDEX) << 1);
	usleep_range(5000, 5100);
	max96706_write_reg(sensor, 0x00,
			   (sensor->addr_96705 + MAX96705_DEV_INDEX) << 1);
	usleep_range(5000, 5100);

	if (sensor->gpi_gpio) {
		gpiod_direction_output(sensor->gpi_gpio, 1);
		usleep_range(10000, 10100);
		gpiod_direction_output(sensor->gpi_gpio, 0);
		usleep_range(1000, 1100);
		gpiod_direction_output(sensor->gpi_gpio, 1);
	} else {
		dev_err(&sensor->i2c_client->dev, "%s: no gpi_gpio\n",
			__func__);
		val = 0;
		max96705_read_reg(sensor, MAX96705_DEV_INDEX, 0x0f, &val);
		dev_info(&sensor->i2c_client->dev, "%s: val=0x%x\n", __func__,
			 val);
		val |= 0x81;
		max96705_write_reg(sensor, MAX96705_DEV_INDEX, 0x0f, val);
		usleep_range(100, 200);
	}

	//enable dbl, hven
	max96705_write_reg(sensor, MAX96705_DEV_INDEX, 0x07, 0x84);
	usleep_range(10000, 10100);
	//[7]dbl, [2]hven, [1]cxtp
	max96706_write_reg(sensor, 0x07, 0x86);
	usleep_range(10000, 10100);

	max96705_write_reg(sensor, MAX96705_DEV_INDEX, 0x9,
			   (AP0101_SLAVE_ID + AP0101_DEV_INDEX) << 1);
	max96705_write_reg(sensor, MAX96705_DEV_INDEX, 0xa,
			   AP0101_SLAVE_ID << 1);
	usleep_range(10000, 10100);

	u16 reg;
	u16 val16;

	reg = 0x0;		//chip version, 0x0160
	val16 = 0;
	ap0101_read_reg16(sensor, AP0101_DEV_INDEX, reg, &val16);
	dev_info(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);

#if 0
	reg = 0xca9c;		//
	val16 = 0;
	ap0101_read_reg16(sensor, AP0101_DEV_INDEX, reg, &val16);
	dev_info(&client->dev, "0101, reg=0x%x, val=0x%x\n", reg, val16);
	if ((val16 & (0x1 << 10)) == 0) {
		val16 = 0x405;
		ap0101_write_reg16(sensor, AP0101_DEV_INDEX, reg, val16);
		usleep_range(10000, 10100);

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
	if ((val16 & (0x1 << 9)) == 0) {
		val16 = 0x605;
		ap0101_write_reg16(sensor, AP0101_DEV_INDEX, reg, val16);
		usleep_range(10000, 10100);

		for (i = 0; i < 10; i++) {
			ret = ap0101_change_config(sensor, 0);

			if (ret == 0)
				break;

			msleep(100);
		}
	}
	dev_info(&client->dev, "0101, end msb. i=%d\n", i);
#endif
	val = 0;
	max96706_read_reg(sensor, 0x5, &val);
	dev_info(&client->dev, "96706, reg=0x5, val=0x%x\n", val);
	val |= 0x40;
	max96706_write_reg(sensor, 0x5, val);
	val = 0;
	max96706_read_reg(sensor, 0x5, &val);
	dev_info(&client->dev, "96706, reg=0x5, val=0x%x\n", val);

	sensor->link_count = link_count;
	return 0;
}

static int max96706_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	struct max96706_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	int ret;
	struct gpio_desc *gpiod;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;

	sensor->addr_96706 = client->addr;
	sensor->addr_96705 = MAX96705_SLAVE_ID;
	sensor->addr_0101 = AP0101_SLAVE_ID;

	/*
	 * default init sequence initialize sensor to
	 * YUV422 UYVY VGA@30fps
	 */
	fmt = &sensor->fmt;
	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = 1280;
	fmt->height = 720;
	fmt->field = V4L2_FIELD_NONE;
	sensor->frame_interval.numerator = 1;

	sensor->frame_interval.denominator =
	    max96706_framerates[MAX96706_25_FPS];
	sensor->current_fr = MAX96706_25_FPS;
	sensor->current_mode =
	    &max96706_mode_data[MAX96706_25_FPS][MAX96706_MODE_720P_1280_720];

//  sensor->last_mode = sensor->current_mode;

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

	v4l2_i2c_subdev_init(&sensor->sd, client, &max96706_subdev_ops);

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);

	if (ret)
		return ret;

	gpiod = devm_gpiod_get_optional(&client->dev, "powerdown", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		dev_err(&client->dev, "get %s GPIO: %d\n", "pwdn", ret);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"pwdn", ret);

		//return ret;
		gpiod = NULL;
	}

	sensor->pwdn_gpio = gpiod;

	gpiod = devm_gpiod_get_optional(&client->dev, "gpi", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"pwdn", ret);

		gpiod = NULL;
	}

	sensor->gpi_gpio = gpiod;

	gpiod = devm_gpiod_get_optional(&client->dev, "poc", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"pwdn", ret);

		//return ret;
		gpiod = NULL;
	}

	sensor->poc_gpio = gpiod;

	mutex_init(&sensor->lock);

	dev_info(dev, "%s(): call max96706_power()-100-500\n", __func__);
	max96706_power(sensor, 1);
	ret = max96706_check_chip_id(sensor);

	if (ret == 0)
		max96706_initialization(sensor);

	ret = v4l2_async_register_subdev(&sensor->sd);

	if (ret)
		goto free_ctrls;

	return 0;

free_ctrls:
//  v4l2_ctrl_handler_free(&sensor->ctrls.handler);
entity_cleanup:
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static int max96706_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct max96706_dev *sensor = to_max96706_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);

	return 0;
}

static const struct i2c_device_id max96706_id[] = {
	{"max96706", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, max96706_id);

static const struct of_device_id max96706_dt_ids[] = {
	{.compatible = "max,max96706"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, max96706_dt_ids);

static struct i2c_driver max96706_i2c_driver = {
	.driver = {
		   .name = "max96706",
		   .of_match_table = max96706_dt_ids,
		   },
	.id_table = max96706_id,
	.probe = max96706_probe,
	.remove = max96706_remove,
};

module_i2c_driver(max96706_i2c_driver);

MODULE_DESCRIPTION("MAX96706 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
