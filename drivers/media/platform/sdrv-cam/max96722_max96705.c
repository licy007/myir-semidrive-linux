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

#define MAX_CAMERA_NUM 4

#define MAX96712_DEVICE_ID 0xa0
#define MAX96722_DEVICE_ID 0xa1
#define MAX96705_DEVICE_ID 0x41
#define MAX9295A_DEVICE_ID 0x91
#define MAX9295B_DEVICE_ID 0x93

#define AP0101_SLAVE_ID (0x5d)
#define MAX20087_SLAVE_ID (0x29)

#define MAX96722_MCSI0_NAME "max,max96722"
#define MAX96722_MCSI1_NAME "max,max96722s"
#define MAX96722_RMCSI0_NAME "max,max96722_sideb"
#define MAX96722_RMCSI1_NAME "max,max96722s_sideb"

#define MAX96722_MCSI0_NAME_I2C "max96722"
#define MAX96722_MCSI1_NAME_I2C "max96722s"
#define MAX96722_RMCSI0_NAME_I2C "max96722_sideb"
#define MAX96722_RMCSI1_NAME_I2C "max96722s_sideb"


enum max96722_mode_id {
	MAX96722_MODE_720P_1280_720 = 0,
	MAX96722_NUM_MODES,
};

enum max96722_frame_rate {
	MAX96722_25_FPS = 0,
	MAX96722_NUM_FRAMERATES,
};

enum MAX96722_CHANNEL {
	MAX96722_CH_A = 0,
	MAX96722_CH_B = 1,
	MAX96722_CH_C = 2,
	MAX96722_CH_D = 3,
};

enum MAX96705_I2C_ADDR {
	MAX96705_DEF = 0x80,
	MAX96705_CH_A = 0x90,
	MAX96705_CH_B = 0x92,
	MAX96705_CH_C = 0x94,
	MAX96705_CH_D = 0x96,
};
enum MAX9295_I2C_ADDR {
	MAX9295_DEF = 0x80,
	MAX9295_CH_A = 0x90,
	MAX9295_CH_B = 0x92,
	MAX9295_CH_C = 0x94,
	MAX9295_CH_D = 0x96,
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


struct max96722_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct max96722_pixfmt max96722_formats[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB,},
};

static const int max96722_framerates[] = {
	[MAX96722_25_FPS] = 25,
};



struct max96722_ctrls {
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

enum max96722_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct max96722_mode_info {
	enum max96722_mode_id id;
	enum max96722_downsize_mode dn_mode;
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

struct max96722_dev {
	struct i2c_client *i2c_client;
	unsigned short addr_96722;
	unsigned short addr_96705;
	unsigned short addr_max20087;
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
	struct mutex max96705_lock;
	/* lock to protect all members below */
	struct mutex lock;

	int power_count;

	struct v4l2_mbus_framefmt fmt;
	bool pending_fmt_change;

	const struct max96722_mode_info *current_mode;
	const struct max96722_mode_info *last_mode;
	enum max96722_frame_rate current_fr;

	struct v4l2_fract frame_interval;

	struct max96722_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
	int sec_9286_shift;
	u32 sync;
};

static const struct max96722_mode_info
	max96722_mode_data[MAX96722_NUM_FRAMERATES][MAX96722_NUM_MODES] = {
	{
		{
			MAX96722_MODE_720P_1280_720, SUBSAMPLING,
			1280, 1892, 720, 740,
		},
	},
};

/**
*  \brief Register address and value pair, with delay.
*/
typedef struct {
	u16 nRegAddr;
	/**< Register Address */
	u8 nRegValue;
	/**< data */
	unsigned int nDelay;
	/**< Delay to be applied, after the register is programmed */
} max96722_param_t;

//#define ONE_PORT
#define FRAMESYNC_USE
//#define PROBE_INIT_MAX96722

static max96722_param_t max96722_reg[] = {
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
	{0x040B, 0x80, 0x0},
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
	{0x09CE, 0xDE, 0x0},
	{0x09CF, 0x00, 0x0},
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
	//{0x040b,0x42, 50},
};
static max96722_param_t max96722_reg_max9295[] = {
	{0x0006, 0xf0, 0x0},
	{0x0010, 0x11, 0x0},
	{0x0011, 0x11, 0x0},
	{0x08A0, 0x04, 0x0}, /*PHY 2x4 mode*/
	{0x08A2, 0x30, 0x0}, //enable all the phys
	{0x08A3, 0xE4, 0x0}, //phy1 D0->D2 D1->D3 phy0 D0->D0 D1->D1
	{0x040B, 0x80, 0x0}, //pipe0 BPP 0x10: datatype 0x1e csi output disable
	{0x040E, 0x5E, 0x0}, //bit0~5 pipe0:0x1e bit 7:6 pipe1 data type highest 2 bits
	{0x040F, 0x7E, 0x0}, //bit7:4 pipe2 highest 4 bits bit3:0 pipe1 low bits
	{0x0410, 0x7A, 0x0}, //bit7:2 pipe3 6 bits bit1:0 pipe2 low 2 bits
	{0x0411, 0x48, 0x0}, //bit7:5 pipe2 high bits bit4:0 pipe1 BPP 0x10:datatype 0x1e
	{0x0412, 0x20, 0x0}, //bit6:2 pipe3 BPP 0x10:0x1e bit1:0 pipe 2 BPP lower bits
	{0x0418, 0xEF, 0x0}, //phy1 lane speed set bit0-4 for lane speed. 0xEF for 1.5G
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
static max96722_param_t max9295_reg[] = {
	{0x02be, 0x10, 0x0},
	{0x0318, 0x5e, 0x0},
	{0x02d3, 0x84, 0x0},
};
static void max96722_link_enable(struct max96722_dev *sensor, int en);
static void max96722_mipi_enable(struct max96722_dev *sensor, int en);
static int max96722_initialization(struct max96722_dev *sensor);
static void max96722_power(struct max96722_dev *sensor, bool enable);
static int max20087_power(struct max96722_dev *sensor, bool enable);


static inline struct max96722_dev *to_max96722_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96722_dev, sd);
}

static int max96722_write_reg(struct max96722_dev *sensor, u16 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	client->addr = sensor->addr_96722;
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
				__func__, sensor->addr_96722, reg, val);
		return ret;
	}
	return 0;
}

static int max96722_read_reg(struct max96722_dev *sensor, u16 reg,
							 u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[3];
	int ret;

	client->addr = sensor->addr_96722;
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
				__func__, sensor->addr_96722, reg, ret);
		return ret;
	}
	*val = buf[2];
	return 0;
}

static int max96705_write_reg(struct max96722_dev *sensor, int channel,
							  u8 reg,
							  u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_96705;
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

static int max96705_read_reg(struct max96722_dev *sensor, int channel,
							 u8 reg,
							 u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_96705;
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

static int max96705_access_sync(struct max96722_dev *sensor,  int channel,
								int write, u16 reg, u8 *val_r, u8 val_w)
{
	int ret = -1;

	if (channel < 0 || channel > 3) {
		dev_err(&sensor->i2c_client->dev,
				"%s: chip max96705 ch=%d, invalid channel.\n", __func__, channel);
		return ret;
	}
	mutex_lock(&sensor->max96705_lock);
	if (write == MAX96705_WRITE)
		ret = max96705_write_reg(sensor, channel, reg, val_w);
	else if (write == MAX96705_READ)
		ret = max96705_read_reg(sensor, channel, reg, val_r);
	else
		dev_err(&sensor->i2c_client->dev, "%s: chip max96705 Invalid parameter.\n",
				__func__);
	mutex_unlock(&sensor->max96705_lock);
	return ret;
}

#if 0 // defined_but_not_used
static int max20087_write_reg(struct max96722_dev *sensor, u8 reg,
							  u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	client->addr = sensor->addr_max20087;
	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = 2;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "%s: chip %x02x error: reg=0x%02x, val=0x%02x\n",
				__func__, client->addr, reg, val);
		return ret;
	}
	return 0;
}

static int max20087_read_reg(struct max96722_dev *sensor, u8 reg,
							 u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	client->addr = sensor->addr_max20087;
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
#endif // defined_but_not_used

static struct v4l2_subdev *max96722_get_interface_subdev(
	struct v4l2_subdev *sd)
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

static int max96722_s_power(struct v4l2_subdev *sd, int on)
{
	struct max96722_dev *sensor = to_max96722_dev(sd);
	struct v4l2_subdev *interface_sd = max96722_get_interface_subdev(sd);
	struct sdrv_csi_mipi_csi2 *kcmc = v4l2_get_subdevdata(interface_sd);

	printk("%s on = %d +\n", __func__, on);
	kcmc->lane_count = 4;
#ifndef PROBE_INIT_MAX96722
	if (on == 1) {
		max96722_power(sensor, 1);
		msleep(30);
		max96722_initialization(sensor);
		usleep_range(10000, 11000);
	} else if (on == 0) {
		max96722_power(sensor, 0);
		max20087_power(sensor, 0);
	}
#endif
	printk("%s -\n", __func__);
	return 0;
}

static int max96722_enum_frame_size(struct v4l2_subdev *sd,
									struct v4l2_subdev_pad_config *cfg,
									struct v4l2_subdev_frame_size_enum *fse)
{
	struct max96722_dev *sensor = to_max96722_dev(sd);

	if (fse->pad != 0)
		return -EINVAL;
	if (fse->index >= 1)
		return -EINVAL;
	fse->min_width = sensor->fmt.width;
	fse->max_width = fse->min_width;
	if (sensor->sync == 0)
		fse->min_height = sensor->fmt.height;
	else
		fse->min_height = sensor->fmt.height * 4;
	fse->max_height = fse->min_height;
	return 0;
}

static int max96722_enum_frame_interval(struct v4l2_subdev *sd,
										struct v4l2_subdev_pad_config *cfg,
										struct v4l2_subdev_frame_interval_enum
										*fie)
{
	struct v4l2_fract tpf;

	if (fie->pad != 0)
		return -EINVAL;
	if (fie->index >= MAX96722_NUM_FRAMERATES)
		return -EINVAL;
	tpf.numerator = 1;
	tpf.denominator = max96722_framerates[fie->index];
	fie->interval = tpf;
	return 0;
}

static int max96722_g_frame_interval(struct v4l2_subdev *sd,
									 struct v4l2_subdev_frame_interval *fi)
{
	struct max96722_dev *sensor = to_max96722_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);
	return 0;
}

static int max96722_s_frame_interval(struct v4l2_subdev *sd,
									 struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

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

static int max96722_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max96722_dev *sensor = to_max96722_dev(sd);

	printk("%s +\n", __func__);
	if (enable == 1) {
		msleep(20);
		max96722_mipi_enable(sensor, 1);
	} else if (enable == 0) {
		max96722_link_enable(sensor, 0);
		usleep_range(2000, 3000);
		max96722_mipi_enable(sensor, 0);
	}
	printk("%s -\n", __func__);
	return 0;
}

static int max96722_get_fmt(struct v4l2_subdev *sd,
							struct v4l2_subdev_pad_config *cfg,
							struct v4l2_subdev_format *format)
{
	struct max96722_dev *sensor = to_max96722_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;
	mutex_lock(&sensor->lock);
	fmt = &sensor->fmt;
	format->format = *fmt;
	mutex_unlock(&sensor->lock);
	return 0;
}

static int max96722_set_fmt(struct v4l2_subdev *sd,
							struct v4l2_subdev_pad_config *cfg,
							struct v4l2_subdev_format *format)
{
	struct max96722_dev *sensor = to_max96722_dev(sd);

	if (format->format.code != sensor->fmt.code)
		return -EINVAL;
	return 0;
}

static int max96722_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops max96722_core_ops = {
	.s_power = max96722_s_power,
};

static const struct v4l2_subdev_video_ops max96722_video_ops = {
	.g_frame_interval = max96722_g_frame_interval,
	.s_frame_interval = max96722_s_frame_interval,
	.s_stream = max96722_s_stream,
};

static const struct v4l2_subdev_pad_ops max96722_pad_ops = {
	.enum_mbus_code = max96722_enum_mbus_code,
	.get_fmt = max96722_get_fmt,
	.set_fmt = max96722_set_fmt,
	.enum_frame_size = max96722_enum_frame_size,
	.enum_frame_interval = max96722_enum_frame_interval,
};

static const struct v4l2_subdev_ops max96722_subdev_ops = {
	.core = &max96722_core_ops,
	.video = &max96722_video_ops,
	.pad = &max96722_pad_ops,
};

static const struct v4l2_ctrl_ops max96722_ctrl_ops = {
	.s_ctrl = max96722_s_ctrl,
};

//Power for Rx max96722/712
static void max96722_power(struct max96722_dev *sensor, bool enable)
{
	gpiod_direction_output(sensor->pwdn_gpio, enable ? 1 : 0);
}


//Power for camera module
static int max20087_power(struct max96722_dev *sensor, bool enable)
{
	int ret;
#ifdef CONFIG_POWER_POC_DRIVER
	struct i2c_client *client = sensor->i2c_client;
	u8 reg = 0x1;

	if (!strcmp(client->name, MAX96722_MCSI0_NAME_I2C)) {
		ret = poc_mdeser0_power(0xf, enable, reg,  0);
	} else if (!strcmp(client->name, MAX96722_MCSI1_NAME_I2C)) {
		ret = poc_mdeser1_power(0xf, enable, reg,  0);
#if defined(CONFIG_ARCH_SEMIDRIVE_V9)
	} else if (!strcmp(client->name, MAX96722_RMCSI0_NAME_I2C)) {
		ret = poc_r_mdeser0_power(0xf, enable, reg,  0);
	} else if (!strcmp(client->name, MAX96722_RMCSI1_NAME_I2C)) {
		ret = poc_r_mdeser1_power(0xf, enable, reg,  0);
#endif
	} else {
		dev_err(&client->dev, "Can't support POC %s.\n", client->name);
		ret = -EINVAL;
	}
#else
	if (enable == 1)
		ret = max20087_write_reg(sensor, 0x01, 0x1f);
	else
		ret = max20087_write_reg(sensor, 0x01, 0x00);
#endif
	return ret;
}

#define MAX_WAIT_COUNT (20)

static int max9295_read_reg(struct max96722_dev  *dev, int channel,
							u16 reg,
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
static int max9295_write_reg(struct max96722_dev  *dev, int channel,
							 u16 reg,
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
static int max9295_check_chip_id(struct max96722_dev  *dev, int channel)
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

static int max96722_check_chip_id(struct max96722_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
//	int i = 0;

	ret = max96722_read_reg(sensor, 0x000d, &chip_id);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to read max96722 chipid.\n",
				__func__);
		return ret;
	}
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

#undef MAX_WAIT_COUNT
#define MAX_WAIT_COUNT (10)

static int max96705_check_chip_id(struct max96722_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 chip_id = 0;
	int i = 0;

	while (i < MAX_WAIT_COUNT) {
		msleep(20);
		ret = max96705_read_reg(sensor, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x1E,
								&chip_id);
		if (ret < 0) {
			i++;
		} else {
			printk(KERN_DEBUG "max96705 dev chipid = 0x%02x\n", chip_id);
			break;
		}
	}
	if (i == MAX_WAIT_COUNT) {
		dev_err(&client->dev,
				"%s  wrong chip identifier,  expected 0x%x(max96705), got 0x%x\n",
				__func__, MAX96705_DEVICE_ID, chip_id);
	}
	return ret;
}

static void max96722_preset(struct max96722_dev *sensor)
{
	//0x00,0x13,0x75,
	max96722_write_reg(sensor, 0x0013, 0x75);
	usleep_range(10000, 11000);
}

static void max96722_mipi_enable(struct max96722_dev *sensor, int en)
{
	//0x04,0x0B,0x42
	if (en == 1)
		max96722_write_reg(sensor, 0x040b, 0x42);
	else
		max96722_write_reg(sensor, 0x040b, 0x00);
}

static void max96722_link_enable(struct max96722_dev *sensor, int en)
{
	if (en == 1)
		max96722_write_reg(sensor, 0x0006, 0xF);
	else
		max96722_write_reg(sensor, 0x0006, 0x00);
}

static int max96722_initialization(struct max96722_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	struct v4l2_subdev sd = sensor->sd;
	struct v4l2_subdev *interface_sd = max96722_get_interface_subdev(&sd);
	struct sdrv_csi_mipi_csi2 *kcmc = v4l2_get_subdevdata(interface_sd);
	struct v4l2_mbus_framefmt *fmt;
	u8 value;
	int ret;
	int reglen = ARRAY_SIZE(max96722_reg);
	int i = 0;
	int j = 0;
	u8 reg_value;
	//printk("%s reglen = %d +\n", __FUNCTION__, reglen);
	fmt = &sensor->fmt;
	//Check if deser exists.
	ret = max96722_read_reg(sensor, 0x000d, &value);
	if (ret < 0) {
		dev_err(&client->dev, "max96722 fail to read 0x0006=%d\n", ret);
		return ret;
	}
	max96722_preset(sensor);
	max96722_mipi_enable(sensor, 0);
	max20087_power(sensor, 0);
	usleep_range(10000, 11000);
	max20087_power(sensor, 1);
	msleep(120);
	max96722_write_reg(sensor, 0x0006, 0xff);//enable GMSL2 and all the links
	msleep(50);
	max96722_write_reg(sensor, 0x0010, 0x11);//set phyA&B speed to 3G
	max96722_write_reg(sensor, 0x0011, 0x11);//set phyC&D speed to 3G
	msleep(50);
	if (max9295_check_chip_id(sensor, 0) == 0) {
		kcmc->lanerate = 1500*1000*1000/8;
		fmt->width = 1920;
		fmt->height = 1080;
		reg_value = 0xf0;
		reglen = ARRAY_SIZE(max96722_reg_max9295);
		for (i = 0; i < reglen; i++) {
			max96722_write_reg(sensor, max96722_reg_max9295[i].nRegAddr,
							   max96722_reg_max9295[i].nRegValue);
			msleep(max96722_reg_max9295[i].nDelay);
		}
		reglen = ARRAY_SIZE(max9295_reg);
		for (i = 0; i < MAX_CAMERA_NUM; i++) {
			value = 0xf0 | (1<<i);
			max96722_write_reg(sensor, 0x6, value);
			if (max9295_check_chip_id(sensor, i) != 0) {
				dev_err(&client->dev,
						"%s: could not find 9295 on channel %d for 96722\n",
						__func__, i);
				continue;
			} else {
				max9295_write_reg(sensor, (MAX9295_DEF-MAX9295_CH_A)>>1, 0x00,
								  MAX9295_CH_A + i * 2);
				reg_value = reg_value|(1<<i);
			}
			for (j = 0; j < reglen; j++) {
				max9295_write_reg(sensor, i, max9295_reg[j].nRegAddr,
								  max9295_reg[j].nRegValue);
				msleep(max9295_reg[j].nDelay);
			}
		}
		max96722_write_reg(sensor, 0x6, reg_value);
		msleep(50);
	} else {
		max96722_preset(sensor);
		max96722_mipi_enable(sensor, 0);
		//Init max96722
		for (i = 0; i < reglen; i++) {
			max96722_write_reg(sensor, max96722_reg[i].nRegAddr,
							   max96722_reg[i].nRegValue);
			msleep(max96722_reg[i].nDelay);
		}
		//Camera Module Power On/Off
		max20087_power(sensor, 0);
		usleep_range(10000, 11000);
		max20087_power(sensor, 1);
		msleep(120);
		for (i = 0; i < MAX_CAMERA_NUM; i++) {
			//Modify serializer i2c address
			max96722_write_reg(sensor, 0x0006, 1<<i);
			if (max96705_check_chip_id(sensor) != 0) {
				dev_err(&client->dev,
						"%s  ch %d wrong chip identifier\n",
						__func__, i);
				continue;
			}
			max96705_write_reg(sensor, (MAX96705_DEF-MAX96705_CH_A)>>1, 0x00,
							   MAX96705_CH_A + i * 2);
			usleep_range(10000, 11000);
			max96705_access_sync(sensor, i, MAX96705_WRITE, 0x07, NULL, 0x84);
			usleep_range(1000, 1100);
			max96705_access_sync(sensor, i, MAX96705_WRITE, 0x43, NULL, 0x25);
			max96705_access_sync(sensor, i, MAX96705_WRITE, 0x45, NULL, 0x01);
			max96705_access_sync(sensor, i, MAX96705_WRITE, 0x47, NULL, 0x26);
			usleep_range(1000, 1100);
			max96705_access_sync(sensor, i, MAX96705_WRITE, 0x67, NULL, 0xc4);
		}
		kcmc->lanerate = 1000*1000*1000/8;
		//For enable data
		usleep_range(2000, 3000);
		max96722_mipi_enable(sensor, 1);
		usleep_range(2000, 3000);
		max96722_link_enable(sensor, 1);
		msleep(30);
		max96722_mipi_enable(sensor, 0);
	}
	//printk("%s  -\n", __FUNCTION__);
	return 0;
}

static int max96722_probe(struct i2c_client *client,
						  const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	struct max96722_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	u32 sync;
	int ret;
	struct gpio_desc *gpiod;
	//printk("%s %s\n", __FUNCTION__, client->name);
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	sensor->i2c_client = client;
	sensor->addr_96722 = client->addr;
	sensor->addr_96705 = MAX96705_CH_A>>1;
	sensor->addr_max20087 = MAX20087_SLAVE_ID;
	gpiod = devm_gpiod_get_optional(&client->dev, "pwdn", GPIOD_IN);
	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"pwdn", ret);
		return ret;
	}
	sensor->pwdn_gpio = gpiod;
	max96722_power(sensor, 0);
	usleep_range(1000, 1100);
	max96722_power(sensor, 1);
	msleep(30);
	ret = max96722_check_chip_id(sensor);
	if (ret < 0) {
		dev_err(&client->dev, "fail to init max96722.\n");
		return ret;
	}
	max96722_power(sensor, 0);
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
	sensor->frame_interval.denominator = max96722_framerates[MAX96722_25_FPS];
	sensor->current_fr = MAX96722_25_FPS;
	sensor->current_mode =
		&max96722_mode_data[MAX96722_25_FPS][MAX96722_MODE_720P_1280_720];
	sensor->ae_target = 52;
	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "sync", &sync);
	dev_info(&client->dev, "sync: %d, ret=%d\n", sync, ret);
	if (ret < 0) {
		sync = 0;
	}
	sensor->sync = sync;
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
	v4l2_i2c_subdev_init(&sensor->sd, client, &max96722_subdev_ops);
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->pads[MIPI_CSI2_SENS_VC0_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC1_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC3_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity,
								 MIPI_CSI2_SENS_VCX_PADS_NUM, sensor->pads);
	if (ret)
		return ret;
#ifdef PROBE_INIT_MAX96722
	max96722_initialization(sensor);
#endif
	ret = v4l2_async_register_subdev(&sensor->sd);
	if (ret)
		goto free_ctrls;
	mutex_init(&sensor->max96705_lock);
	mutex_init(&sensor->lock);
	return 0;
free_ctrls:
	//v4l2_ctrl_handler_free(&sensor->ctrls.handler);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static int max96722_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct max96722_dev *sensor = to_max96722_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);
	return 0;
}

static const struct i2c_device_id max96722_id[] = {
	{"max96722", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, max96722_id);

static const struct of_device_id max96722_dt_ids[] = {
	{.compatible = "max,max96722"},
	{.compatible = "max,max96722s"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, max96722_dt_ids);

static struct i2c_driver max96722_i2c_driver = {
	.driver = {
		.name = "max96722",
		.of_match_table = max96722_dt_ids,
	},
	.id_table = max96722_id,
	.probe = max96722_probe,
	.remove = max96722_remove,
};

module_i2c_driver(max96722_i2c_driver);

#ifdef CONFIG_ARCH_SEMIDRIVE_V9
static const struct i2c_device_id max96722_id_sideb[] = {
	{"max96722", 0},
	{},
};

static const struct of_device_id max96722_dt_ids_sideb[] = {
	{.compatible = MAX96722_RMCSI0_NAME},
	{.compatible = MAX96722_RMCSI1_NAME},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, max96722_dt_ids_sideb);

static struct i2c_driver max96722_i2c_driver_sideb = {
	.driver = {
		.name = "max96722_sideb",
		.of_match_table = max96722_dt_ids_sideb,
	},
	.id_table = max96722_id_sideb,
	.probe = max96722_probe,
	.remove = max96722_remove,
};

static int __init sdrv_max96722_sideb_init(void)
{
	int ret;

	ret = i2c_add_driver(&max96722_i2c_driver_sideb);
	if (ret < 0)
		printk("fail to register max96722 i2c driver.\n");
	return ret;
}

late_initcall(sdrv_max96722_sideb_init);
#endif


MODULE_DESCRIPTION("MAX96722 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
