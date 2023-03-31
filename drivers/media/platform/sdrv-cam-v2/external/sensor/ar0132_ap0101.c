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

enum ar0132_mode_id {
	AR0132_MODE_720P_1280_720 = 0,
	AR0132_NUM_MODES,
};

enum ar0132_frame_rate {
	AR0132_25_FPS = 0,
	AR0132_NUM_FRAMERATES,
};

struct ar0132_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct ar0132_pixfmt ar0132_formats[] = {
	{MEDIA_BUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_SRGB,},
};

static const int ar0132_framerates[] = {
	[AR0132_25_FPS] = 25,
};

struct ar0132_ctrls {
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

enum ar0132_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct ar0132_mode_info {
	enum ar0132_mode_id id;
	enum ar0132_downsize_mode dn_mode;
	u32 hact;
	u32 htot;
	u32 vact;
	u32 vtot;
};

struct ar0132_dev {
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

	const struct ar0132_mode_info *current_mode;
	const struct ar0132_mode_info *last_mode;
	enum ar0132_frame_rate current_fr;

	struct v4l2_fract frame_interval;

	struct ar0132_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
};

static const struct ar0132_mode_info
	ar0132_mode_data[AR0132_NUM_FRAMERATES][AR0132_NUM_MODES] = {
	{
		{
			AR0132_MODE_720P_1280_720, SUBSAMPLING,
			1280, 1892, 720, 740,
		},
	},
};

static inline struct ar0132_dev *to_ar0132_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0132_dev, sd);
}

static int ap0101_read_reg16(struct ar0132_dev *sensor, u16 reg, u16 *val)
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

static int ap0101_write_reg16(struct ar0132_dev *sensor, u16 reg, u16 val)
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

static int ar0132_s_power(struct v4l2_subdev *sd, int on)
{
	struct ar0132_dev *sensor = to_ar0132_dev(sd);
	dev_err(&sensor->i2c_client->dev, "%s: on=%d\n", __func__, on);
	return 0;
}

static int ar0132_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->pad != 0)
		return -EINVAL;

	if (fse->index >= AR0132_NUM_MODES)
		return -EINVAL;

	fse->min_width = ar0132_mode_data[0][fse->index].hact;
	fse->max_width = fse->min_width;
	fse->min_height = ar0132_mode_data[0][fse->index].vact;
	fse->max_height = fse->min_height;
	return 0;
}

static int ar0132_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg,
				      struct v4l2_subdev_frame_interval_enum
				      *fie)
{
	struct v4l2_fract tpf;

	if (fie->pad != 0)
		return -EINVAL;

	if (fie->index >= AR0132_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = ar0132_framerates[fie->index];

	fie->interval = tpf;

	return 0;
}

static int ar0132_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct ar0132_dev *sensor = to_ar0132_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ar0132_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{

	return 0;
}

static int ar0132_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	if (code->index >= ARRAY_SIZE(ar0132_formats))
		return -EINVAL;

	code->code = ar0132_formats[code->index].code;

	return 0;
}

static int ar0132_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0132_dev *sensor = to_ar0132_dev(sd);

	dev_err(&sensor->i2c_client->dev, "%s: enable=%d\n", __func__, enable);
	return 0;
}

static int ar0132_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct ar0132_dev *sensor = to_ar0132_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	fmt = &sensor->fmt;

	format->format = *fmt;

	mutex_unlock(&sensor->lock);

	return 0;
}

static int ar0132_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct ar0132_dev *sensor = to_ar0132_dev(sd);

	if(format->format.code != sensor->fmt.code)
		return -EINVAL;

	return 0;
}

static int ar0132_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops ar0132_core_ops = {
	.s_power = ar0132_s_power,
};

static const struct v4l2_subdev_video_ops ar0132_video_ops = {
	.g_frame_interval = ar0132_g_frame_interval,
	.s_frame_interval = ar0132_s_frame_interval,
	.s_stream = ar0132_s_stream,
};

static const struct v4l2_subdev_pad_ops ar0132_pad_ops = {
	.enum_mbus_code = ar0132_enum_mbus_code,
	.get_fmt = ar0132_get_fmt,
	.set_fmt = ar0132_set_fmt,
	.enum_frame_size = ar0132_enum_frame_size,
	.enum_frame_interval = ar0132_enum_frame_interval,
};

static const struct v4l2_subdev_ops ar0132_subdev_ops = {
	.core = &ar0132_core_ops,
	.video = &ar0132_video_ops,
	.pad = &ar0132_pad_ops,
};

static const struct v4l2_ctrl_ops ar0132_ctrl_ops = {
	.s_ctrl = ar0132_s_ctrl,
};

static const char *const test_pattern_menu[] = {
	"Disabled",
	"Color bars",
};

#if 0
static int ar0132_init_controls(struct ar0132_dev *sensor)
{
	const struct v4l2_ctrl_ops *ops = &ar0132_ctrl_ops;
	struct ar0132_ctrls *ctrls = &sensor->ctrls;
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

static int ap0101_change_config(struct ar0132_dev *sensor)
{
	u16 reg;
	u16 val16;
	int i;

	reg = 0xfc00;
	val16 = 0x2800;
	ap0101_write_reg16(sensor, reg, val16);
	usleep_range(10000, 11000);

	reg = 0x0040;
	val16 = 0x8100;
	ap0101_write_reg16(sensor, reg, val16);
	usleep_range(10000, 11000);

	for (i = 0; i < 10; i++) {
		reg = 0x0040;
		val16 = 0;
		ap0101_read_reg16(sensor, reg, &val16);

		if (val16 == 0x0)
			return 0;

		usleep_range(10000, 11000);
	}

	return -1;
}

static int ar0132_initialization(struct ar0132_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u16 val16;
	u16 reg;
	int j;

	//u8 link_status = 0, link_count = 0;
	//struct gpio_desc *gpiod;

	dev_err(&client->dev, "%s()\n", __func__);

	reg = 0x0;		//chip version, 0x0160
	val16 = 0;
	ap0101_read_reg16(sensor, reg, &val16);
	dev_err(&client->dev, "96706 0101, reg=0x%x, val=0x%x\n", reg, val16);

	reg = 0xc846;		//
	val16 = 0;
	ap0101_read_reg16(sensor, reg, &val16);
	dev_err(&client->dev, "1-reg=0x%x, val=0x%x\n", reg, val16);
	val16 |= 0x1;
	ap0101_write_reg16(sensor, reg, val16);
	usleep_range(10000, 11000);

	for (j = 0; j < 10; j++) {
		ret = ap0101_change_config(sensor);

		if (ret == 0)
			break;

		msleep(100);
	}
	reg = 0xc846;		//
	val16 = 0;
	ap0101_read_reg16(sensor, reg, &val16);
	dev_err(&client->dev, "2-reg=0x%x, val=0x%x\n", reg, val16);

	return 0;
}

static int ar0132_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	struct ar0132_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	int ret;

	dev_info(dev, "%s++\n", __func__);
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;

	/*
	 * default init sequence initialize sensor to
	 * YUV422 UYVY VGA@30fps
	 */
	fmt = &sensor->fmt;
	fmt->code = MEDIA_BUS_FMT_YUYV8_2X8;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = 1280;
	fmt->height = 720;
	fmt->field = V4L2_FIELD_NONE;
	sensor->frame_interval.numerator = 1;

	sensor->frame_interval.denominator = ar0132_framerates[AR0132_25_FPS];
	sensor->current_fr = AR0132_25_FPS;
	sensor->current_mode =
	&ar0132_mode_data[AR0132_25_FPS][AR0132_MODE_720P_1280_720];

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

	v4l2_i2c_subdev_init(&sensor->sd, client, &ar0132_subdev_ops);

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);

	if (ret)
		return ret;

	mutex_init(&sensor->lock);

	ar0132_initialization(sensor);
	ret = v4l2_async_register_subdev(&sensor->sd);

	if (ret)
		goto free_ctrls;

	return 0;

free_ctrls:
//  v4l2_ctrl_handler_free(&sensor->ctrls.handler);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static int ar0132_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ar0132_dev *sensor = to_ar0132_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);

	return 0;
}

static const struct i2c_device_id ar0132_id[] = {
	{"ar0132", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ar0132_id);

static const struct of_device_id ar0132_dt_ids[] = {
	{.compatible = "ar,ar0132"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, ar0132_dt_ids);

static struct i2c_driver ar0132_i2c_driver = {
	.driver = {
		   .name = "ar0132",
		   .of_match_table = ar0132_dt_ids,
		   },
	.id_table = ar0132_id,
	.probe = ar0132_probe,
	.remove = ar0132_remove,
};

module_i2c_driver(ar0132_i2c_driver);

MODULE_DESCRIPTION("AR0132 Parallel Camera Subdev Driver");
MODULE_LICENSE("GPL");
