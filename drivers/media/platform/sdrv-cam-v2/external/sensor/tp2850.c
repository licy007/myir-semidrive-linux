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


static struct i2c_client *tp2850_i2c_client = NULL;
static unsigned char tp2850_i2c_addr;

#define tp2850_DEVICE_ID 0x2850

#define tp2850_NAME "tp,tp2850"

#define tp2850_NAME_I2C "tp2850"

#define I2C_ADDR_TP2850 0x44

enum{
	DEC_PAGE=0,
    MIPI_PAGE=8,
};
enum{
    STD_TVI,
    STD_HDA, //AHD
};
enum{
    PAL,
    NTSC,
    HD25,
    HD30,
    FHD25,
    FHD30,
};

enum tp2850_mode_id {
	tp2850_MODE_720P_1280_720 = 0,
	tp2850_NUM_MODES,
};

enum tp2850_frame_rate {
	tp2850_25_FPS = 0,
	tp2850_NUM_FRAMERATES,
};

struct tp2850_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct tp2850_pixfmt tp2850_formats[] = {
	{MEDIA_BUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_SRGB,},
};

static const int tp2850_framerates[] = {
	[tp2850_25_FPS] = 25,
};

struct tp2850_ctrls {
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

enum tp2850_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct tp2850_mode_info {
	enum tp2850_mode_id id;
	enum tp2850_downsize_mode dn_mode;
	u32 hact;
	u32 htot;
	u32 vact;
	u32 vtot;
};

struct tp2850_dev {
	struct i2c_client *i2c_client;
	unsigned short addr_tp2850;
	struct i2c_adapter *adap;
	struct v4l2_subdev sd;

	struct media_pad pad;

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

	const struct tp2850_mode_info *current_mode;
	const struct tp2850_mode_info *last_mode;
	enum tp2850_frame_rate current_fr;

	struct v4l2_fract frame_interval;

	struct tp2850_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
	int sec_9286_shift;
	u32 sync;
};

static const struct tp2850_mode_info
	tp2850_mode_data[tp2850_NUM_FRAMERATES][tp2850_NUM_MODES] = {
	{
		{
			tp2850_MODE_720P_1280_720, SUBSAMPLING,
			1280, 1892, 720, 740,
		},
	},
};

static inline struct tp2850_dev *to_tp2850_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct tp2850_dev, sd);
}

static int tp2850_i2c_write(u8 reg, u8 val)
{
	struct i2c_client *client = tp2850_i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = tp2850_i2c_addr;
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

static int tp2850_i2c_read(u8 reg, u8 *val)
{
	struct i2c_client *client = tp2850_i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = tp2850_i2c_addr;
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


static int tp2850_s_power(struct v4l2_subdev *sd, int on)
{
	//struct tp2850_dev *sensor = to_tp2850_dev(sd);

	return 0;
}

static int tp2850_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct tp2850_dev *sensor = to_tp2850_dev(sd);

	if (fse->pad != 0)
		return -EINVAL;

	if (fse->index >= tp2850_NUM_MODES)
		return -EINVAL;

	fse->min_width = tp2850_mode_data[0][fse->index].hact;
	fse->max_width = fse->min_width;

	if (sensor->sync == 0)
		fse->min_height = tp2850_mode_data[0][fse->index].vact;
	else
		fse->min_height = tp2850_mode_data[0][fse->index].vact * 4;

	fse->max_height = fse->min_height;
	return 0;
}

static int tp2850_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum
				       *fie)
{
	struct v4l2_fract tpf;

	if (fie->pad != 0)
		return -EINVAL;

	if (fie->index >= tp2850_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = tp2850_framerates[fie->index];

	fie->interval = tpf;

	return 0;
}

static int tp2850_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct tp2850_dev *sensor = to_tp2850_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int tp2850_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

static int tp2850_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	if (code->index >= ARRAY_SIZE(tp2850_formats))
		return -EINVAL;

	code->code = tp2850_formats[code->index].code;

	return 0;
}

static int tp2850_check_chip_id(struct tp2850_dev *sensor);

static int tp2850_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tp2850_dev *sensor = to_tp2850_dev(sd);
	struct i2c_client *client = sensor->i2c_client;

	if (enable == 1) {
		dev_info(&client->dev, "%s(): enable \n", __func__);
	} else if (enable == 0) {
		dev_info(&client->dev, "%s(): disabled \n", __func__);
	}
	return 0;
}

static int tp2850_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct tp2850_dev *sensor = to_tp2850_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	fmt = &sensor->fmt;

	format->format = *fmt;

	mutex_unlock(&sensor->lock);

	return 0;
}

static int tp2850_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	return 0;
}

static int tp2850_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops tp2850_core_ops = {
	.s_power = tp2850_s_power,
};

static const struct v4l2_subdev_video_ops tp2850_video_ops = {
	.g_frame_interval = tp2850_g_frame_interval,
	.s_frame_interval = tp2850_s_frame_interval,
	.s_stream = tp2850_s_stream,
};

static const struct v4l2_subdev_pad_ops tp2850_pad_ops = {
	.enum_mbus_code = tp2850_enum_mbus_code,
	.get_fmt = tp2850_get_fmt,
	.set_fmt = tp2850_set_fmt,
	.enum_frame_size = tp2850_enum_frame_size,
	.enum_frame_interval = tp2850_enum_frame_interval,
};

static const struct v4l2_subdev_ops tp2850_subdev_ops = {
	.core = &tp2850_core_ops,
	.video = &tp2850_video_ops,
	.pad = &tp2850_pad_ops,
};

static void tp2850_reset(struct tp2850_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->reset_gpio, enable ? 1 : 0);
}

static void tp2850_power(struct tp2850_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->pwdn_gpio, enable ? 1 : 0);
}
static void tp2850_power_pw(struct tp2850_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->pw_gpio, enable ? 1 : 0);
}
static int tp2850_check_chip_id(struct tp2850_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 chip_lid = 0;
	u8 chip_hid = 0;
	u16 chip_id = 0;

	//tp2850_power(sensor, 1);

	ret = tp2850_i2c_read(0xfe, &chip_hid);
	if (ret) {
		dev_err(&client->dev, "%s: failed to read chip identifier\n",
			__func__);
		goto power_off;
	} else {
		dev_info(&client->dev, "%s(): check_chip id succsed \n");
	}
	ret = tp2850_i2c_read(0xff, &chip_lid);
	if (ret) {
		dev_err(&client->dev, "%s: failed to read chip identifier\n",
			__func__);
		goto power_off;
	} else {
		dev_info(&client->dev, "%s(): check_chip id succsed \n");
	}
	chip_id = (chip_hid << 8) | chip_lid;
	if (chip_id != tp2850_DEVICE_ID) {
		dev_err(&client->dev,
			"%s: wrong chip identifier, expected 0x%x(tp2850), got 0x%x\n",
			__func__, tp2850_DEVICE_ID, chip_id);
		ret = -ENXIO;
	}
	return ret;
power_off:
	//tp2850_power(sensor, 0);
	return ret;
}

enum{
	VIN1=0,
    VIN2=1,
    VIN3=2,
    VIN4=3,
};

void tp2850_common_init(void)
{
	tp2850_i2c_write(0x40, MIPI_PAGE); //MIPI page
	tp2850_i2c_write(0x00, 0x00);
	tp2850_i2c_write(0x08, 0xf0);
	tp2850_i2c_write(0x14, 0x73);

	tp2850_i2c_write(0x40, DEC_PAGE);
	tp2850_i2c_write(0x4c, 0x43);
	tp2850_i2c_write(0x4e, 0x15);
	tp2850_i2c_write(0xf3, 0x00);
	tp2850_i2c_write(0xf6, 0x00);
	tp2850_i2c_write(0xfa, 0x00);
	tp2850_i2c_write(0xfd, 0x80);
}

void tp2850_clock_set(unsigned char fmt)
{
	tp2850_i2c_write(0x40, MIPI_PAGE); //MIPI page

	if(FHD30 == fmt || FHD25 == fmt) {
		tp2850_i2c_write(0x13, 0x04);
	} else {
		tp2850_i2c_write(0x13, 0x24);
	}

	tp2850_i2c_write(0x40, DEC_PAGE);
}

/////////////////////////////////
//ch: video channel
//fmt: PAL/NTSC/HD25/HD30
//std: STD_TVI/STD_HDA
//sample: tp2850_sensor_init(VIN1,HD30,STD_TVI); //video is TVI 720p30 from Vin1
////////////////////////////////
void tp2850_sensor_init(unsigned char ch,unsigned char fmt,unsigned char std)
{
	printk("%s, %d,ch:%d, fmt:%d, std:%d\n", __func__, __LINE__, ch, fmt, std);
	tp2850_common_init();
	tp2850_clock_set(fmt);

	tp2850_i2c_write(0x40, DEC_PAGE);
	tp2850_i2c_write(0x41, ch);		//video MUX select

	if(PAL == fmt) {
		tp2850_i2c_write(0x02, 0xcf);
		tp2850_i2c_write(0x07, 0x80);
		tp2850_i2c_write(0x0b, 0x80);
		tp2850_i2c_write(0x0c, 0x13);
		tp2850_i2c_write(0x0d, 0x51);

		tp2850_i2c_write(0x15, 0x13);
		tp2850_i2c_write(0x16, 0x76);
		tp2850_i2c_write(0x17, 0x80);
		tp2850_i2c_write(0x18, 0x17);
		tp2850_i2c_write(0x19, 0x20);
		tp2850_i2c_write(0x1a, 0x17);
		tp2850_i2c_write(0x1b, 0x01);
		tp2850_i2c_write(0x1c, 0x09);
		tp2850_i2c_write(0x1d, 0x48);

		tp2850_i2c_write(0x20, 0x48);
		tp2850_i2c_write(0x21, 0x84);
		tp2850_i2c_write(0x22, 0x37);
		tp2850_i2c_write(0x23, 0x3f);

		tp2850_i2c_write(0x2b, 0x70);
		tp2850_i2c_write(0x2c, 0x2a);
		tp2850_i2c_write(0x2d, 0x64);
		tp2850_i2c_write(0x2e, 0x56);

		tp2850_i2c_write(0x30, 0x7a);
		tp2850_i2c_write(0x31, 0x4a);
		tp2850_i2c_write(0x32, 0x4d);
		tp2850_i2c_write(0x33, 0xf0);

		tp2850_i2c_write(0x35, 0x65);
		tp2850_i2c_write(0x38, 0x00);
		tp2850_i2c_write(0x39, 0x04);
	} else if(NTSC == fmt) {
		tp2850_i2c_write(0x02, 0xcf);
		tp2850_i2c_write(0x07, 0x80);
		tp2850_i2c_write(0x0b, 0x80);
		tp2850_i2c_write(0x0c, 0x13);
		tp2850_i2c_write(0x0d, 0x50);

		tp2850_i2c_write(0x15, 0x13);
		tp2850_i2c_write(0x16, 0x60);
		tp2850_i2c_write(0x17, 0x80);
		tp2850_i2c_write(0x18, 0x12);
		tp2850_i2c_write(0x19, 0xf0);
		tp2850_i2c_write(0x1a, 0x07);
		tp2850_i2c_write(0x1b, 0x01);
		tp2850_i2c_write(0x1c, 0x09);
		tp2850_i2c_write(0x1d, 0x38);

		tp2850_i2c_write(0x20, 0x40);
		tp2850_i2c_write(0x21, 0x84);
		tp2850_i2c_write(0x22, 0x36);
		tp2850_i2c_write(0x23, 0x3c);

		tp2850_i2c_write(0x2b, 0x70);
		tp2850_i2c_write(0x2c, 0x2a);
		tp2850_i2c_write(0x2d, 0x68);
		tp2850_i2c_write(0x2e, 0x57);

		tp2850_i2c_write(0x30, 0x62);
		tp2850_i2c_write(0x31, 0xbb);
		tp2850_i2c_write(0x32, 0x96);
		tp2850_i2c_write(0x33, 0xc0);

		tp2850_i2c_write(0x35, 0x65);
		tp2850_i2c_write(0x38, 0x00);
		tp2850_i2c_write(0x39, 0x04);
	} else if(HD25 == fmt) {
		tp2850_i2c_write(0x02, 0x8a);
		tp2850_i2c_write(0x07, 0xc0);
		tp2850_i2c_write(0x0b, 0xc0);
		tp2850_i2c_write(0x0c, 0x13);
		tp2850_i2c_write(0x0d, 0x50);

		tp2850_i2c_write(0x15, 0x13);
		tp2850_i2c_write(0x16, 0x15);
		tp2850_i2c_write(0x17, 0x00);
		tp2850_i2c_write(0x18, 0x19);
		tp2850_i2c_write(0x19, 0xd0);
		tp2850_i2c_write(0x1a, 0x25);
		tp2850_i2c_write(0x1b, 0x01);
		tp2850_i2c_write(0x1c, 0x07);  //1280*720, 25fps
		tp2850_i2c_write(0x1d, 0xbc);  //1280*720, 25fps

		tp2850_i2c_write(0x20, 0x30);
		tp2850_i2c_write(0x21, 0x84);
		tp2850_i2c_write(0x22, 0x36);
		tp2850_i2c_write(0x23, 0x3c);

		tp2850_i2c_write(0x2b, 0x60);
		tp2850_i2c_write(0x2c, 0x0a);
		tp2850_i2c_write(0x2d, 0x30);
		tp2850_i2c_write(0x2e, 0x70);

		tp2850_i2c_write(0x30, 0x48);
		tp2850_i2c_write(0x31, 0xbb);
		tp2850_i2c_write(0x32, 0x2e);
		tp2850_i2c_write(0x33, 0x90);

		tp2850_i2c_write(0x35, 0x25);
		tp2850_i2c_write(0x38, 0x00);
		tp2850_i2c_write(0x39, 0x18);

		if(STD_HDA == std) {
        	tp2850_i2c_write(0x02, 0xce);//y/c

        	tp2850_i2c_write(0x0d, 0x71);

        	tp2850_i2c_write(0x20, 0x40);
        	tp2850_i2c_write(0x21, 0x46);

        	tp2850_i2c_write(0x25, 0xfe);
        	tp2850_i2c_write(0x26, 0x01);

        	tp2850_i2c_write(0x2c, 0x3a);
        	tp2850_i2c_write(0x2d, 0x5a);
        	tp2850_i2c_write(0x2e, 0x40);

        	tp2850_i2c_write(0x30, 0x9e);
        	tp2850_i2c_write(0x31, 0x20);
        	tp2850_i2c_write(0x32, 0x10);
        	tp2850_i2c_write(0x33, 0x90);
		}
	} else if(HD30 == fmt) {
		tp2850_i2c_write(0x02, 0xca);
		tp2850_i2c_write(0x07, 0xc0);
		tp2850_i2c_write(0x0b, 0xc0);
		tp2850_i2c_write(0x0c, 0x13);
		tp2850_i2c_write(0x0d, 0x50);

		tp2850_i2c_write(0x15, 0x13);
		tp2850_i2c_write(0x16, 0x15);
		tp2850_i2c_write(0x17, 0x00);
		tp2850_i2c_write(0x18, 0x19);
		tp2850_i2c_write(0x19, 0xd0);
		tp2850_i2c_write(0x1a, 0x25);
		tp2850_i2c_write(0x1b, 0x01);
		tp2850_i2c_write(0x1c, 0x06);  //1280*720, 30fps
		tp2850_i2c_write(0x1d, 0x72);  //1280*720, 30fps

		tp2850_i2c_write(0x20, 0x30);
		tp2850_i2c_write(0x21, 0x84);
		tp2850_i2c_write(0x22, 0x36);
		tp2850_i2c_write(0x23, 0x3c);

		tp2850_i2c_write(0x2b, 0x60);
		tp2850_i2c_write(0x2c, 0x0a);
		tp2850_i2c_write(0x2d, 0x30);
		tp2850_i2c_write(0x2e, 0x70);

		tp2850_i2c_write(0x30, 0x48);
		tp2850_i2c_write(0x31, 0xbb);
		tp2850_i2c_write(0x32, 0x2e);
		tp2850_i2c_write(0x33, 0x90);

		tp2850_i2c_write(0x35, 0x25);
		tp2850_i2c_write(0x38, 0x00);
		tp2850_i2c_write(0x39, 0x18);

		if(STD_HDA == std) {
        	tp2850_i2c_write(0x02, 0xce);

        	tp2850_i2c_write(0x0d, 0x70);

        	tp2850_i2c_write(0x20, 0x40);
        	tp2850_i2c_write(0x21, 0x46);

        	tp2850_i2c_write(0x25, 0xfe);
        	tp2850_i2c_write(0x26, 0x01);

        	tp2850_i2c_write(0x2c, 0x3a);
        	tp2850_i2c_write(0x2d, 0x5a);
        	tp2850_i2c_write(0x2e, 0x40);

        	tp2850_i2c_write(0x30, 0x9d);
        	tp2850_i2c_write(0x31, 0xca);
        	tp2850_i2c_write(0x32, 0x01);
        	tp2850_i2c_write(0x33, 0xd0);
		}
	} else if(FHD30 == fmt) {
		tp2850_i2c_write(0x02, 0xc8);
		tp2850_i2c_write(0x07, 0xc0);
		tp2850_i2c_write(0x0b, 0xc0);
		tp2850_i2c_write(0x0c, 0x03);
		tp2850_i2c_write(0x0d, 0x50);

		tp2850_i2c_write(0x15, 0x03);
		tp2850_i2c_write(0x16, 0xd2);
		tp2850_i2c_write(0x17, 0x80);
		tp2850_i2c_write(0x18, 0x29);
		tp2850_i2c_write(0x19, 0x38);
	    tp2850_i2c_write(0x1b, 0x01);
		tp2850_i2c_write(0x1a, 0x47);
		tp2850_i2c_write(0x1c, 0x08);  //1920*1080, 30fps
		tp2850_i2c_write(0x1d, 0x98);  //

		tp2850_i2c_write(0x20, 0x30);
		tp2850_i2c_write(0x21, 0x84);
		tp2850_i2c_write(0x22, 0x36);
		tp2850_i2c_write(0x23, 0x3c);

		tp2850_i2c_write(0x2b, 0x60);
		tp2850_i2c_write(0x2c, 0x0a);
		tp2850_i2c_write(0x2d, 0x30);
		tp2850_i2c_write(0x2e, 0x70);

		tp2850_i2c_write(0x30, 0x48);
		tp2850_i2c_write(0x31, 0xbb);
		tp2850_i2c_write(0x32, 0x2e);
		tp2850_i2c_write(0x33, 0x90);

		tp2850_i2c_write(0x35, 0x05);
		tp2850_i2c_write(0x38, 0x00);
		tp2850_i2c_write(0x39, 0x1C);

		if(STD_HDA == std) {
    		tp2850_i2c_write(0x02, 0xcc);

    		tp2850_i2c_write(0x0d, 0x72);

    		tp2850_i2c_write(0x15, 0x01);
    		tp2850_i2c_write(0x16, 0xf0);

    		tp2850_i2c_write(0x20, 0x38);
    		tp2850_i2c_write(0x21, 0x46);

    		tp2850_i2c_write(0x25, 0xfe);
    		tp2850_i2c_write(0x26, 0x0d);

    		tp2850_i2c_write(0x2c, 0x3a);
    		tp2850_i2c_write(0x2d, 0x54);
    		tp2850_i2c_write(0x2e, 0x40);

    		tp2850_i2c_write(0x30, 0xa5);
    		tp2850_i2c_write(0x31, 0x95);
    		tp2850_i2c_write(0x32, 0xe0);
    		tp2850_i2c_write(0x33, 0x60);
		}
	} else if(FHD25 == fmt) {
		tp2850_i2c_write(0x02, 0xc8);
		tp2850_i2c_write(0x07, 0xc0);
		tp2850_i2c_write(0x0b, 0xc0);
		tp2850_i2c_write(0x0c, 0x03);
		tp2850_i2c_write(0x0d, 0x50);

		tp2850_i2c_write(0x15, 0x03);
		tp2850_i2c_write(0x16, 0xd2);
		tp2850_i2c_write(0x17, 0x80);
		tp2850_i2c_write(0x18, 0x29);
		tp2850_i2c_write(0x19, 0x38);
		tp2850_i2c_write(0x1a, 0x47);
	    tp2850_i2c_write(0x1b, 0x01);
		tp2850_i2c_write(0x1c, 0x0a);  //1920*1080, 25fps
		tp2850_i2c_write(0x1d, 0x50);  //

		tp2850_i2c_write(0x20, 0x30);
		tp2850_i2c_write(0x21, 0x84);
		tp2850_i2c_write(0x22, 0x36);
		tp2850_i2c_write(0x23, 0x3c);

		tp2850_i2c_write(0x2b, 0x60);
		tp2850_i2c_write(0x2c, 0x0a);
		tp2850_i2c_write(0x2d, 0x30);
		tp2850_i2c_write(0x2e, 0x70);

		tp2850_i2c_write(0x30, 0x48);
		tp2850_i2c_write(0x31, 0xbb);
		tp2850_i2c_write(0x32, 0x2e);
		tp2850_i2c_write(0x33, 0x90);

		tp2850_i2c_write(0x35, 0x05);
		tp2850_i2c_write(0x38, 0x00);
		tp2850_i2c_write(0x39, 0x1C);

		if (STD_HDA == std) {
   			tp2850_i2c_write(0x02, 0xcc);

   			tp2850_i2c_write(0x0d, 0x73);

   			tp2850_i2c_write(0x15, 0x01);
   			tp2850_i2c_write(0x16, 0xf0);

   			tp2850_i2c_write(0x20, 0x3c);
   			tp2850_i2c_write(0x21, 0x46);

   			tp2850_i2c_write(0x25, 0xfe);
   			tp2850_i2c_write(0x26, 0x0d);

   			tp2850_i2c_write(0x2c, 0x3a);
   			tp2850_i2c_write(0x2d, 0x54);
   			tp2850_i2c_write(0x2e, 0x40);

   			tp2850_i2c_write(0x30, 0xa5);
   			tp2850_i2c_write(0x31, 0x86);
   			tp2850_i2c_write(0x32, 0xfb);
   			tp2850_i2c_write(0x33, 0x60);
		}
	}
}

static int tp2850_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	//struct fwnode_handle *endpoint;
	struct tp2850_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	u32 sec_9286;
	u32 sync;
	int ret;
	struct device_node *subdev_of_node;
	struct gpio_desc *gpiod;
	struct device_node *poc_np;
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;
	tp2850_i2c_client = sensor->i2c_client;
	sensor->addr_tp2850 = client->addr;
	tp2850_i2c_addr = sensor->addr_tp2850;
	mutex_init(&sensor->lock);
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

	sensor->frame_interval.denominator = tp2850_framerates[tp2850_25_FPS];
	sensor->current_fr = tp2850_25_FPS;
	sensor->current_mode =
	    &tp2850_mode_data[tp2850_25_FPS][tp2850_MODE_720P_1280_720];

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

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	v4l2_i2c_subdev_init(&sensor->sd, client, &tp2850_subdev_ops);
	subdev_of_node = of_graph_get_port_by_id(client->dev.of_node, 0);
	sensor->sd.fwnode = of_fwnode_handle(subdev_of_node);
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);


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

		//return ret;
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
		tp2850_power(sensor, true);
		usleep_range(10000, 11000);
	}
	if (sensor->pw_gpio != NULL) {
		tp2850_power_pw(sensor, true);
		usleep_range(10000, 11000);
	}
	tp2850_reset(sensor, false);
	usleep_range(10000, 11000);
	tp2850_reset(sensor, true);
	usleep_range(10000, 11000);

	ret = tp2850_check_chip_id(sensor);

	if (ret == 0)
	    tp2850_sensor_init(VIN1, HD25, STD_HDA);

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

static int tp2850_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tp2850_dev *sensor = to_tp2850_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);

	return 0;
}

static const struct i2c_device_id tp2850_id[] = {
	{"tp2850", 0},
	{},
};

//MODULE_DEVICE_TABLE(i2c, tp2850_id);

static const struct of_device_id tp2850_dt_ids[] = {
	{.compatible = tp2850_NAME},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, tp2850_dt_ids);

static struct i2c_driver tp2850_i2c_driver = {
	.driver = {
		   .name = tp2850_NAME_I2C,
		   .of_match_table = tp2850_dt_ids,
		   },
	.id_table = tp2850_id,
	.probe = tp2850_probe,
	.remove = tp2850_remove,
};

module_i2c_driver(tp2850_i2c_driver);

MODULE_DESCRIPTION("tp2850 csi Camera Subdev Driver");
MODULE_LICENSE("GPL");
