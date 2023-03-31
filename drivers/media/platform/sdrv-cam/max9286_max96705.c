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
#include "poc-max2008x.h"

#define MAX_SENSOR_NUM 4

#define MAX9286_DEVICE_ID 0x40

#define MAX96705_SLAVE_ID 0x40
#define MAX96705_DEV_INDEX -4
#define MAX96705_DEVICE_ID 0x41

#define AP0101_SLAVE_ID 0x5d
#define AP0101_DEV_INDEX -4
#define MAX20086_SLAVE_ID 0x28


#define MAX9286_MCSI0_NAME "max,max9286"
#define MAX9286_MCSI1_NAME "max,max9286s"
#define MAX9286_RMCSI0_NAME "max,max9286_sideb"
#define MAX9286_RMCSI1_NAME "max,max9286s_sideb"

#define MAX9286_MCSI0_NAME_I2C "max9286"
#define MAX9286_MCSI1_NAME_I2C "max9286s"
#define MAX9286_RMCSI0_NAME_I2C "max9286_sideb"
#define MAX9286_RMCSI1_NAME_I2C "max9286s_sideb"

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

enum max9286_mode_id {
	MAX9286_MODE_720P_1280_720 = 0,
	MAX9286_NUM_MODES,
};

enum max9286_frame_rate {
	MAX9286_25_FPS = 0,
	MAX9286_NUM_FRAMERATES,
};

struct max9286_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct max9286_pixfmt max9286_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static const int max9286_framerates[] = {
	[MAX9286_25_FPS] = 25,
};

struct max9286_ctrls {
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

enum max9286_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct max9286_mode_info {
	enum max9286_mode_id id;
	enum max9286_downsize_mode dn_mode;
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

struct max9286_dev {
	struct i2c_client *i2c_client;
	unsigned short addr_9286;
	unsigned short addr_96705;
	unsigned short addr_0101;
	unsigned short addr_20086;
	unsigned short addr_20086_;
	struct i2c_adapter *adap;
	struct v4l2_subdev sd;
#if 1
	struct media_pad pads[MIPI_CSI2_SENS_VCX_PADS_NUM];
#else
	struct media_pad pad;
#endif
	struct v4l2_fwnode_endpoint ep;	/* the parsed DT endpoint info */

	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	bool upside_down;
	uint32_t link_count;
	/* lock to protect all members below */
	struct mutex lock;

	int power_count;

	struct v4l2_mbus_framefmt fmt;
	bool pending_fmt_change;

	const struct max9286_mode_info *current_mode;
	const struct max9286_mode_info *last_mode;
	enum max9286_frame_rate current_fr;

	struct v4l2_fract frame_interval;

	struct max9286_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
	int sec_9286_shift;
	u32 sync;
};

static const struct max9286_mode_info
	max9286_mode_data[MAX9286_NUM_FRAMERATES][MAX9286_NUM_MODES] = {
	{
		{
			MAX9286_MODE_720P_1280_720, SUBSAMPLING,
			1280, 1892, 720, 740,
		},
	},
};

static inline struct max9286_dev *to_max9286_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9286_dev, sd);
}

static int max9286_write_reg(struct max9286_dev *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_9286;
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

static int max9286_read_reg(struct max9286_dev *sensor, u8 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_9286;
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

static int max96705_write_reg(struct max9286_dev *sensor, u8 idx, u8 reg,
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

static int max96705_read_reg(struct max9286_dev *sensor, u8 idx, u8 reg,
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

static int max20086_write_reg(struct max9286_dev *sensor, u8 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_20086;
	buf[0] = reg;
	buf[1] = val;
	//printk("%s: reg=0x%x, val=0x%x\n", __func__, reg, val);
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(sensor->adap ? sensor->adap : client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x, val=0x%x\n",
			__func__, reg, val);
		if (sensor->addr_20086_) {
			client->addr = sensor->addr_20086_;
			buf[0] = reg;
			buf[1] = val;
			msg.addr = client->addr;
			msg.flags = client->flags;
			msg.buf = buf;
			msg.len = sizeof(buf);
			ret = i2c_transfer(sensor->adap ? sensor->adap : client->adapter, &msg, 1);
			if (ret < 0) {
				dev_err(&client->dev, "%s_: error: reg=0x%x, val=0x%x\n", __func__, reg, val);
				return ret;
			}
		} else {
			return ret;
		}
	}

	return 0;
}

static int max20086_read_reg(struct max9286_dev *sensor, u8 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_20086;
	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(sensor->adap ? sensor->adap : client->adapter, msg, 2);

	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=0x%x\n", __func__, reg);
		if (sensor->addr_20086_) {
			client->addr = sensor->addr_20086_;
			buf[0] = reg;
			msg[0].addr = client->addr;
			msg[0].flags = client->flags;
			msg[0].buf = buf;
			msg[0].len = sizeof(buf);
			msg[1].addr = client->addr;
			msg[1].flags = client->flags | I2C_M_RD;
			msg[1].buf = buf;
			msg[1].len = 1;
			ret = i2c_transfer(sensor->adap ? sensor->adap : client->adapter, msg, 2);
			if (ret < 0) {
				dev_err(&client->dev, "%s_: error: reg=0x%x\n", __func__, reg);
				return ret;
			}
		} else {
			return ret;
		}
	}

	*val = buf[0];
	return 0;
}

static struct v4l2_subdev *max9286_get_interface_subdev(struct v4l2_subdev *sd)
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

static int max9286_s_power(struct v4l2_subdev *sd, int on)
{
	struct max9286_dev *sensor = to_max9286_dev(sd);
	struct v4l2_subdev *interface_sd = max9286_get_interface_subdev(sd);
	struct sdrv_csi_mipi_csi2 *kcmc = v4l2_get_subdevdata(interface_sd);

	kcmc->lane_count = sensor->link_count;

	return 0;
}

static int max9286_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct max9286_dev *sensor = to_max9286_dev(sd);

	if (fse->pad != 0)
		return -EINVAL;

	if (fse->index >= MAX9286_NUM_MODES)
		return -EINVAL;

	fse->min_width = max9286_mode_data[0][fse->index].hact;
	fse->max_width = fse->min_width;

	if (sensor->sync == 0)
		fse->min_height = max9286_mode_data[0][fse->index].vact;
	else
		fse->min_height = max9286_mode_data[0][fse->index].vact * 4;

	fse->max_height = fse->min_height;
	return 0;
}

static int max9286_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum
				       *fie)
{
	struct max9286_dev *sensor = to_max9286_dev(sd);
	struct v4l2_fract tpf;
	int ret;

	if (fie->pad != 0)
		return -EINVAL;

	if (fie->index >= MAX9286_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = max9286_framerates[fie->index];

	fie->interval = tpf;

	return 0;
}

static int max9286_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct max9286_dev *sensor = to_max9286_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int max9286_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

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

static int max9286_s_stream(struct v4l2_subdev *sd, int enable)
{
	int i, idx;
	u8 val;

	struct max9286_dev *sensor = to_max9286_dev(sd);
	//int ret = 0;
	//sensor->i2c_client->addr = sensor->addr_9286;

	if (enable == 1) {
		//csienable=1
		max9286_write_reg(sensor, 0x15, 0x9b);
	} else if (enable == 0) {
		//csienable=0
		max9286_write_reg(sensor, 0x15, 0x13);
	}

	return 0;
}

static int max9286_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct max9286_dev *sensor = to_max9286_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	fmt = &sensor->fmt;

	format->format = *fmt;

	mutex_unlock(&sensor->lock);

	return 0;
}

static int max9286_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct max9286_dev *sensor = to_max9286_dev(sd);

	if(format->format.code != sensor->fmt.code)
		return -EINVAL;

	return 0;
}

static int max9286_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops max9286_core_ops = {
	.s_power = max9286_s_power,
};

static const struct v4l2_subdev_video_ops max9286_video_ops = {
	.g_frame_interval = max9286_g_frame_interval,
	.s_frame_interval = max9286_s_frame_interval,
	.s_stream = max9286_s_stream,
};

static const struct v4l2_subdev_pad_ops max9286_pad_ops = {
	.enum_mbus_code = max9286_enum_mbus_code,
	.get_fmt = max9286_get_fmt,
	.set_fmt = max9286_set_fmt,
	.enum_frame_size = max9286_enum_frame_size,
	.enum_frame_interval = max9286_enum_frame_interval,
};

static const struct v4l2_subdev_ops max9286_subdev_ops = {
	.core = &max9286_core_ops,
	.video = &max9286_video_ops,
	.pad = &max9286_pad_ops,
};

static const struct v4l2_ctrl_ops max9286_ctrl_ops = {
	.s_ctrl = max9286_s_ctrl,
};

static const char *const test_pattern_menu[] = {
	"Disabled",
	"Color bars",
};

static int max9286_init_controls(struct max9286_dev *sensor)
{
	const struct v4l2_ctrl_ops *ops = &max9286_ctrl_ops;
	struct max9286_ctrls *ctrls = &sensor->ctrls;
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

static void max9286_power(struct max9286_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->pwdn_gpio, enable ? 1 : 0);
}

static void max20086_power(struct max9286_dev *sensor, bool enable)
{
	u8 reg, val;

#ifdef CONFIG_POWER_POC_DRIVER
		struct i2c_client *client = sensor->i2c_client;

		reg = 0x1;
		if (!strcmp(client->name, MAX9286_MCSI0_NAME_I2C)) {
			poc_mdeser0_power(0xf, enable, reg,  0);
		} else if (!strcmp(client->name, MAX9286_MCSI1_NAME_I2C)) {
			poc_mdeser1_power(0xf, enable, reg,  0);
#if defined(CONFIG_ARCH_SEMIDRIVE_V9)
		} else if (!strcmp(client->name, MAX9286_RMCSI0_NAME_I2C)) {
			poc_r_mdeser0_power(0xf, enable, reg,  0);
		} else if (!strcmp(client->name, MAX9286_RMCSI1_NAME_I2C)) {
			poc_r_mdeser1_power(0xf, enable, reg,  0);
#endif
		} else {
			dev_err(&client->dev, "Can't support POC %s.\n", client->name);
		}
#else
	reg = 0x1;
	if (enable) {
		val = 0x1f;
		max20086_write_reg(sensor, reg, val);
	} else {
		val = 0x10;
		max20086_write_reg(sensor, reg, val);
	}

	val = 0;
	max20086_read_reg(sensor, reg, &val);
	dev_info(&sensor->i2c_client->dev, "20086: reg=%d, val=0x%x\n", reg,
		val);
#endif

}

static int max9286_check_chip_id(struct max9286_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	//max9286_power(sensor, 1);

	ret = max9286_read_reg(sensor, 0x1e, &chip_id);

	if (ret) {
		dev_err(&client->dev, "%s: failed to read chip identifier\n",
			__func__);
		//goto power_off;
	}

	if (chip_id != MAX9286_DEVICE_ID) {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(max9286), got 0x%x\n",
			__func__, MAX9286_DEVICE_ID, chip_id);
		ret = -ENXIO;
	}
//power_off:
//  max9286_power(sensor, 0);

	return ret;
}

static int max9286_initialization(struct max9286_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 reg, val;
	u8 link_status = 0, link_count = 0;
	int i = 0, j;

	//csienable=0
	ret = max9286_write_reg(sensor, 0x15, DES_REG15_CSIOUT_DISABLE);
	if (ret) {
		dev_err(&client->dev, "max9286 write fail!\n");
		return 0;
	}
	usleep_range(5000, 6000);

	//disable fs
	max9286_write_reg(sensor, 0x1, DES_REG1_FS_DISABLE);


	dev_info(&client->dev, "set him\n");
	//him enable
	max9286_write_reg(sensor, 0x1c, DES_REG1C_HIM_ENABLE);
	usleep_range(5000, 6000);


	max20086_power(sensor, true);
	msleep(100);


	for (i = 0; i < 20; i++) {
		val = 0;
		max9286_read_reg(sensor, 0x49, &val);

		if ((val & 0x0f) == 0x0f)
			break;
		msleep(10);
	}
	dev_info(&client->dev, "0x49=0x%x, i=%d\n", val, i);
	if (val == 0) {
		dev_err(&client->dev, "max96705 detect fail!\n");
		return 0;
	}
	//config link
	max96705_write_reg(sensor, 0, 0x04, SER_REG4_CFGLINK_EN);
	usleep_range(5000, 6000);

	//dbl, hven
	max96705_write_reg(sensor, 0, 0x07, SER_REG7_DBL_HV_EN);
	usleep_range(5000, 6000);

	//dbl align
	max96705_write_reg(sensor, 0, 0x67, SER_REG67_DBLALIGN);

	for (j = 0; j < 20; j++) {
		reg = 0x49;
		val = 0;
		max9286_read_reg(sensor, reg, &val);

		if ((val & 0xf0) == 0xf0)
			break;
		msleep(20);
	}
	dev_info(&client->dev, "reg=0x%x, val=0x%x, j=%d\n", reg, val, j);

	for (i = 0; i < MAX_SENSOR_NUM; i++) {

		//max9286_write_reg(sensor, 0x0A, 0x11<<i);
		max9286_write_reg(sensor, 0x0A, 0x01 << i);
		usleep_range(5000, 6000);

		link_count++;

		for (j = 0; j < 20; j++) {
			reg = 0x49;
			val = 0;
			max9286_read_reg(sensor, reg, &val);

			if ((val & 0x10 << i))
				break;
			msleep(20);
		}
		dev_info(&client->dev,
			"reg=0x%x, val=0x%x, [0x%x], i=%d, j=%d\n", reg, val,
			(MAX96705_SLAVE_ID + MAX96705_DEV_INDEX +
			 sensor->sec_9286_shift + i), i, j);

		max96705_write_reg(sensor, 0, 0x00,
				   (MAX96705_SLAVE_ID + MAX96705_DEV_INDEX +
				    sensor->sec_9286_shift + i) << 1);
		usleep_range(5000, 6000);
	}

	//Enable all Link control channel
	max9286_write_reg(sensor, 0x0A, DES_REGA_ALLLINK_EN);
	usleep_range(10000, 11000);

	reg = 0x49;
	val = 0;
	max9286_read_reg(sensor, 0x49, &val);
	dev_info(&client->dev, "all reg=0x%x, val=0x%x\n", reg, val);

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
	usleep_range(5000, 6000);

	val = 0;

	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		//vs delay
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x43, 0x25);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x45, 0x01);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x47, 0x26);
		usleep_range(5000, 6000);

		//max96705_write_reg(sensor, i, 0x4d, 0xcc);

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
		//msleep(10);

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
		usleep_range(5000, 6000);

		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x9,
				   (AP0101_SLAVE_ID + AP0101_DEV_INDEX +
				    sensor->sec_9286_shift + i) << 1);
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0xa, AP0101_SLAVE_ID << 1);
		usleep_range(5000, 6000);
	}

	max9286_write_reg(sensor, 0x0, DES_REG0_INPUTLINK_EN);
	usleep_range(5000, 6000);

	//fs, manual pclk
	max9286_write_reg(sensor, 0x06, 0x00);
	max9286_write_reg(sensor, 0x07, 0xf2);
	max9286_write_reg(sensor, 0x08, 0x2b);
	usleep_range(10000, 11000);

	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		//enable channel
		max96705_write_reg(sensor,
				   MAX96705_DEV_INDEX + sensor->sec_9286_shift +
				   i, 0x04, 0x83);
		usleep_range(5000, 6000);
	}

	reg = 0x49;
	val = 0;
	max9286_read_reg(sensor, 0x49, &val);
	dev_info(&client->dev, "end reg=0x%x, val=0x%x\n", reg, val);

	//internal fs, debug for output
	max9286_write_reg(sensor, 0x1, DES_REG1_FS_EN);
	usleep_range(10000, 11000);

	sensor->link_count = link_count;
	return 0;
}

static int max9286_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	struct max9286_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	u32 sec_9286;
	u32 sync;
	int ret;
	struct gpio_desc *gpiod;
	struct device_node *poc_np;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;
	sensor->addr_9286 = client->addr;

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

	sensor->frame_interval.denominator = max9286_framerates[MAX9286_25_FPS];
	sensor->current_fr = MAX9286_25_FPS;
	sensor->current_mode =
	    &max9286_mode_data[MAX9286_25_FPS][MAX9286_MODE_720P_1280_720];

//  sensor->last_mode = sensor->current_mode;

	sensor->ae_target = 52;

	ret =
	    fwnode_property_read_u16(dev_fwnode(&client->dev), "addr_20086",
				     &sensor->addr_20086);
	dev_info(&client->dev, "addr_20086: 0x%x, ret=%d\n", sensor->addr_20086,
		 ret);
	if (ret < 0)
		sensor->addr_20086 = MAX20086_SLAVE_ID;
	ret =
	    fwnode_property_read_u16(dev_fwnode(&client->dev), "addr_20086_",
				     &sensor->addr_20086_);
	dev_info(&client->dev, "addr_20086_: 0x%x, ret=%d\n", sensor->addr_20086_,
		 ret);
	if (ret < 0)
		sensor->addr_20086_ = 0;

	ret =
	    fwnode_property_read_u16(dev_fwnode(&client->dev), "addr_96705",
				     &sensor->addr_96705);
	dev_info(&client->dev, "addr_96705: 0x%x, ret=%d\n", sensor->addr_96705,
		 ret);
	if (ret < 0)
		sensor->addr_96705 = MAX96705_SLAVE_ID;

	ret =
	    fwnode_property_read_u16(dev_fwnode(&client->dev), "addr_0101",
				     &sensor->addr_0101);
	dev_info(&client->dev, "addr_0101: 0x%x, ret=%d\n", sensor->addr_0101,
		 ret);
	if (ret < 0)
		sensor->addr_0101 = AP0101_SLAVE_ID;

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

	v4l2_i2c_subdev_init(&sensor->sd, client, &max9286_subdev_ops);

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
#if 1
	sensor->pads[MIPI_CSI2_SENS_VC0_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC1_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC3_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity,
				     MIPI_CSI2_SENS_VCX_PADS_NUM, sensor->pads);
#else
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
#endif

	if (ret)
		return ret;

	//struct gpio_desc *gpiod;
	gpiod = devm_gpiod_get_optional(&client->dev, "pwdn", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"pwdn", ret);

		//return ret;
		gpiod = NULL;
	}

	sensor->pwdn_gpio = gpiod;
	//gpiod_direction_output(gpiod, 1);
	//msleep(1);

	//struct gpio_desc *gpiod;
	gpiod = devm_gpiod_get_optional(&client->dev, "vin", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"vin", ret);

		dev_err(&client->dev, "%s: get vin gpio fail\n", __func__);
		//return ret;
	} else {
		gpiod_direction_output(gpiod, 1);
		usleep_range(1000, 2000);
	}

	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "sec_9286",
				       &sec_9286);
	dev_info(&client->dev, "sec_9286: %d, ret=%d\n", sec_9286, ret);

	if (!ret) {
		if (sec_9286 == 1)
			sensor->sec_9286_shift = -4;
		else
			sensor->sec_9286_shift = 0;
	} else {
		sensor->sec_9286_shift = 0;
	}

	max9286_power(sensor, false);
	max20086_power(sensor, false);

	usleep_range(10000, 11000);

	max9286_power(sensor, true);
	usleep_range(10000, 11000);


	ret = max9286_check_chip_id(sensor);

	if (ret == 0)
		max9286_initialization(sensor);

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

static int max9286_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct max9286_dev *sensor = to_max9286_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);

	return 0;
}

static const struct i2c_device_id max9286_id[] = {
	{"max9286", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, max9286_id);

static const struct of_device_id max9286_dt_ids[] = {
	{.compatible = MAX9286_MCSI0_NAME},
	{.compatible = MAX9286_MCSI1_NAME},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, max9286_dt_ids);

static struct i2c_driver max9286_i2c_driver = {
	.driver = {
		   .name = "max9286",
		   .of_match_table = max9286_dt_ids,
		   },
	.id_table = max9286_id,
	.probe = max9286_probe,
	.remove = max9286_remove,
};

module_i2c_driver(max9286_i2c_driver);

#ifdef CONFIG_ARCH_SEMIDRIVE_V9
static const struct i2c_device_id max9286_id_sideb[] = {
	{"max9286", 0},
	{},
};

static const struct of_device_id max9286_dt_ids_sideb[] = {
	{.compatible = MAX9286_RMCSI0_NAME},
	{.compatible = MAX9286_RMCSI1_NAME},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, max9286_dt_ids_sideb);

static struct i2c_driver max9286_i2c_driver_sideb = {
	.driver = {
		   .name = "max9286_sideb",
		   .of_match_table = max9286_dt_ids_sideb,
		   },
	.id_table = max9286_id_sideb,
	.probe = max9286_probe,
	.remove = max9286_remove,
};

static int __init sdrv_max9286_sideb_init(void)
{
	int ret;

	ret = i2c_add_driver(&max9286_i2c_driver_sideb);
	if (ret < 0)
		printk("fail to register max9286 i2c driver.\n");

	return ret;
}

late_initcall(sdrv_max9286_sideb_init);
#endif

MODULE_DESCRIPTION("MAX9286 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
