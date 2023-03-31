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
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "poc-max2008x.h"
#include "sdrv_serdes_core.h"

#define MAX_SENSOR_NUM 4

#define MAX9286_DEVICE_ID 0x40
#define MAX96705_DEVICE_ID 0x41

#define DES_1C_HIM_ENABLE 0xf0
/* Register 0x49 */
#define MAX9286_VIDEO_DETECT_MASK	0x0f
/* Register 0x27 */
#define MAX9286_LOCKED			(1<<7)
// Register 0x31
#define MAX9286_FSYNC_LOCKED    (1<<6)

//0x15 disable csi output
#define DES_REG15_CSIOUT_DISABLE 0x03
//0x01 disable fs
#define DES_REG1_FS_DISABLE 0xc3
#define DES_REG1_FS_EN 0x40
//0x1c enable him
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

int max9286_fengjing_initialization(struct serdes_dev_t *serdes);
void max9286_out_enable(struct serdes_dev_t *serdes, bool enable);
int max9286_write_reg(struct serdes_dev_t *serdes, u8 reg, u8 val)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u32 addr;

	addr = client->addr;
	client->addr = serdes->addr_des;
	ret =  serdes_write_reg_8(client, reg, val);
	client->addr = addr;
	return ret;
}

int max9286_read_reg(struct serdes_dev_t *serdes, u8 reg, u8 *val)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u32 addr;

	addr = client->addr;
	client->addr = serdes->addr_des;
	ret =  serdes_read_reg_8(client, reg, val);
	client->addr = addr;
	return ret;
}

int max9286_get_video_links(struct serdes_dev_t *serdes)
{
	u8 val;
	int ret;

	ret = max9286_read_reg(serdes, 0x49, &val);
	if (ret < 0)
		return -EIO;
	return (val&0xf)&serdes->serdes_streamon_bitmap;
}

static bool max9286_get_video_lock(struct serdes_dev_t *serdes)
{
	u8 video_lock_status = 0;
	u8 val = 0;
	int ret;

	ret = max9286_read_reg(serdes, 0x27, &val);
	if (ret < 0)
		return false;
	video_lock_status = val&0xf;
	if ((video_lock_status&serdes->serdes_streamon_bitmap) !=
		serdes->current_channel_lock_status) {
		dev_err(&serdes->i2c_client->dev,
				"video_lock_status 0x%x current_channel_lock_status 0x%x\n",
				video_lock_status, serdes->current_channel_lock_status);
		return true;
	}
	return false;
}

int max9286_get_vsync_link_lock(struct serdes_dev_t *serdes)
{
	u8 val;
	int ret;
	int lock_status;

	ret = max9286_read_reg(serdes, 0x27, &val);
	if (ret < 0)
		return -EIO;
	lock_status = val&0xf;
	return lock_status;
}

static int max9286_get_link_lock_ex(struct serdes_dev_t *serdes, int channel)
{
	u8 val;
	int link_lock = 0;
	int vsync_lock = max9286_get_vsync_link_lock(serdes);

	if (vsync_lock != 0xf) { //check if link lock is ok
		max9286_read_reg(serdes, 0x49, &val);
		link_lock = val&0xf;
		if (link_lock == 0xf) { //all links online,reinit
			max9286_fengjing_initialization(serdes);
			if (serdes->serdes_status.stream_cnt > 0) {
				max9286_out_enable(serdes, 1);
			}
			//check link status after  reinit
			max9286_read_reg(serdes, 0x49, &val);
			link_lock = val&0xf;
		}
	} else {
		link_lock = 0xf;
	}
	return link_lock;
}

#define MAX_CHECK_COUNT (10)
#define MAX_CHECK_VIDEO_COUNT (250)
/*
 * max9286_check_video_links() - Make sure video links are detected and locked
 *
 * Performs safety checks on video link status. Make sure they are detected
 * and all enabled links are locked.
 *
 * Returns 0 for success, -EIO for errors.
 */
int max9286_check_video_links(struct serdes_dev_t *serdes, u8 channel_mask)
{
	unsigned int i;
	int ret;
	u8 val;
	/*
	 * Make sure valid video links are detected.
	 * The delay is not characterized in de-serializer manual, wait up
	 * to 5 ms.
	 */
	for (i = 0; i < MAX_CHECK_VIDEO_COUNT; i++) {
		ret = max9286_read_reg(serdes, 0x49, &val);
		if (ret < 0)
			return -EIO;
		if ((val & MAX9286_VIDEO_DETECT_MASK) == channel_mask)
			break;
		usleep_range(350, 500);
	}
	if (i == MAX_CHECK_VIDEO_COUNT) {
		if (val == 0) {
			dev_err(&serdes->i2c_client->dev,
					"Unable to detect video links: 0x%02x\n", val);
			return -EIO;
		}
	}
	/* Make sure all enabled links are locked (4ms max). */
	if (channel_mask == 0xf) {
		for (i = 0; i < MAX_CHECK_COUNT; i++) {
			ret = max9286_read_reg(serdes, 0x27, &val);
			if (ret < 0)
				return -EIO;
			if (val & MAX9286_LOCKED)
				break;
			usleep_range(350, 450);
		}
		if (i == MAX_CHECK_COUNT) {
			dev_err(&serdes->i2c_client->dev,
					"Not all enabled links locked: 0x27 val 0x%x\n", val);
			return -EIO;
		}
		dev_info(&serdes->i2c_client->dev,
				 "Successfully detected video links after %u loops\n",
				 i);
	}
	return 0;
}
EXPORT_SYMBOL(max9286_check_video_links);
/*
 * max9286_check_config_link() - Detect and wait for configuration links
 *
 * Determine if the configuration channel is up and settled for a link.
 *
 * Returns 0 for success, -EIO for errors.
 */
int max9286_check_config_link(struct serdes_dev_t *serdes)
{
	unsigned int config_mask =  0x0f << 4;
	unsigned int i;
	u8 val;
	int ret;
	/*
	 * Make sure requested configuration links are detected.
	 * The delay is not characterized in the chip manual: wait up
	 * to 5 milliseconds.
	 */
	for (i = 0; i < MAX_CHECK_COUNT; i++) {
		ret = max9286_read_reg(serdes, 0x49, &val);
		if (ret < 0)
			return -EIO;
		val &= 0xf0;
		if (val == config_mask)
			break;
		usleep_range(350, 500);
	}
	if (val != config_mask) {
		dev_err(&serdes->i2c_client->dev,
				"Unable to detect configuration links: 0x%02x expected 0x%02x\n",
				ret, config_mask);
		if (val == 0)
			return -EIO;
	}
	dev_info(&serdes->i2c_client->dev,
			 "Successfully detected configuration links after %u loops: 0x%02x\n",
			 i, config_mask);
	return 0;
}

/*
 * Detect for frame sync lock
 */
int max9286_check_fsync_locked(struct serdes_dev_t *serdes)
{
	u8 val;
	unsigned int i;
	int ret;

	for (i = 0; i < MAX_CHECK_COUNT; i++) {
		ret = max9286_read_reg(serdes, 0x31, &val);
		if (ret < 0)
			return -EIO;
		if (val & MAX9286_FSYNC_LOCKED)
			break;
		usleep_range(350, 500);
	}
	if (i == MAX_CHECK_COUNT) {
		dev_err(&serdes->i2c_client->dev, "Fsync not locked reg 0x%02x\n", val);
		return -EIO;
	}
	dev_info(&serdes->i2c_client->dev,
			 "Successfully detected frame sync after %u loops\n",
			 i);
	return 0;
}

void max9286_out_enable(struct serdes_dev_t *serdes, bool enable)
{
	if (enable == 1)
		max9286_write_reg(serdes, 0x15, 0x9b);
	if (enable == 0)
		max9286_write_reg(serdes, 0x15, 0x13);
}


void max9286_power(struct serdes_dev_t *serdes, bool enable)
{
	gpiod_direction_output(serdes->pwdn_gpio, enable ? 1 : 0);
	usleep_range(10000, 11000);
}

void max20086_power(struct serdes_dev_t *serdes, bool enable)
{
	dev_info(&serdes->i2c_client->dev, "20086: channel  enable %d\n",
			 enable);
	poc_power(serdes->i2c_client, serdes->poc_gpio, serdes->addr_poc,
			  0xf, enable);
}

/*This is a function for i2c address remapping The original address is 0x40 and needs to be ramapped
  for different channel access from soc*/
/* for avm normally the
   channel 0 0x40 -->0x48
   channel 1 0x40 -->0x49
   channel 2 0x40 -->0x4a
   channel 3 0x40 -->0x4b */
static int max96705_map_i2c_addr(struct serdes_dev_t *serdes, u8 addr)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u16 reg = 0x00;
	u32 ori_addr;

	if (serdes == NULL) {
		printk("no client %s %d", __func__, __LINE__);
		return -ENODEV;
	}
	ori_addr = client->addr;
	client->addr = serdes->addr_ser;
	ret = serdes_write_reg_8(client, reg, addr<<1);
	client->addr = ori_addr;
	return ret;
}


static int max96705_write_reg(struct serdes_dev_t *serdes,
							  u8 addr,
							  u8 reg,
							  u8 val)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret;
	u32 ori_addr;

	ori_addr = client->addr;
	client->addr = addr;
	ret = serdes_write_reg_8(client, reg, val);
	client->addr = ori_addr;
	return ret;
}

//write to max96705 with the address before addr remap
static int max96705_write_reg_ori(struct serdes_dev_t *serdes,
								  u8 reg,
								  u8 val)
{
	struct i2c_msg msg;
	struct i2c_client *client = serdes->i2c_client;
	u8 buf[2];
	int ret;
	u8 addr;

	addr = serdes->addr_ser;
	buf[0] = reg;
	buf[1] = val;
	msg.addr = addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev,
				"%s:  chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
				__func__, msg.addr, reg, val);
		return ret;
	}
	return 0;
}

int max9286_reverse_channel_setup(struct serdes_dev_t *serdes)
{
	int i = 0;
	/*enable forward/reverse control channel */
	max9286_write_reg(serdes, 0x0A, DES_REGA_ALLLINK_EN);
	max9286_check_video_links(serdes, 0xf);
	//configuration link enable
	//this will enable all the conf link of serializers with addr 0x40
	//it is required to set the gmsl1 serializer to CLINKEN and back to
	//SEREN when a valid PCLK is available to avoid iic read/write failure
	//In this case it should be 0x43(CLINKEN)->0x83(SEREN)
	max96705_write_reg_ori(serdes, 0x04,
						   SER_REG4_CFGLINK_EN);
	usleep_range(5000, 6000);
	//dbl, hven should be match with the serdesializer
	max96705_write_reg_ori(serdes, 0x07,
						   SER_REG7_DBL_HV_EN);
	//dbl align
	max96705_write_reg_ori(serdes, 0x67,
						   SER_REG67_DBLALIGN);
	max9286_check_config_link(serdes);
	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		//enable each forward control channel at one time
		//that is to make sure only one forward channel is available at one time
		//to make the i2c addr remapping
		max9286_write_reg(serdes, 0x0A, 0x01 << i);
		max9286_check_config_link(serdes);
		max96705_map_i2c_addr(serdes, serdes->remap_addr_ser[i]);
	}
	//at last enable all the forward/reverse control channel
	max9286_write_reg(serdes, 0x0A, DES_REGA_ALLLINK_EN);
	return 0;
}

static serdes_param_reg8_t max9286_init_reg[] = {
	{0x0A, 0x0,  0},/*disable forward/reverse control channel */
	{0x15, 0x03, 2000},//disable csi output
	{0x01, 0xc3, 0},//disable fs
	{0x1c, 0xf4, 2000},//him enable
	{0x12, 0xf3, 0},//enable dbl double double input mode(2 bits for one cycle)csi-2 output yuv422 8bit
	{0x0c, 0x91, 0},//use HS/VS encoding
	{0x63, 0x0,  0},//63, 64 for csi data output
	{0x64, 0x0,  0},
	{0x69, 0x30, 0},//auto mask enabled
	{0x19, 0x80, 0},//timing period
};
int max9286_initial_setup(struct serdes_dev_t *serdes)
{
	int ret = 0;
	int i = 0;
	int reglen = ARRAY_SIZE(max9286_init_reg);
	//disable poc power during 9286 initial setup
	max20086_power(serdes, false);
	for (i = 0; i < reglen; i++) {
		ret = max9286_write_reg(serdes, max9286_init_reg[i].nRegAddr,
								max9286_init_reg[i].nRegValue);
		if (ret) {
			dev_err(&serdes->i2c_client->dev, "max9286 write fail!\n");
			return -EIO;
		}
		if (max9286_init_reg[i].nDelay > 0)
			usleep_range(max9286_init_reg[i].nDelay, max9286_init_reg[i].nDelay+1000);
	}
	max20086_power(serdes, true);
	return 0;
}

static serdes_param_reg8_t max96705_gmsl_set_reg[] = {
	{0x43, 0x25, 0},//set vs delay
	{0x45, 0x01, 0},
	{0x47, 0x26, 5000},
	{0x20, 0x07, 0},//set crossbar
	{0x21, 0x06, 0},
	{0x22, 0x05, 0},
	{0x23, 0x04, 0},
	{0x24, 0x03, 0},
	{0x25, 0x02, 0},
	{0x26, 0x01, 0},
	{0x27, 0x00, 0},
	{0x30, 0x17, 0},
	{0x31, 0x16, 0},
	{0x32, 0x15, 0},
	{0x33, 0x14, 0},
	{0x34, 0x13, 0},
	{0x35, 0x12, 0},
	{0x36, 0x11, 0},
	{0x37, 0x10, 1000},
};
int max9286_max96705_gmsl_link_setup(struct serdes_dev_t *serdes)
{
	int i = 0;
	int j = 0;
	int reglen = ARRAY_SIZE(max96705_gmsl_set_reg);

	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		for (j = 0; j < reglen; j++) {
			max96705_write_reg(serdes, serdes->remap_addr_ser[i],
							   max96705_gmsl_set_reg[j].nRegAddr,
							   max96705_gmsl_set_reg[j].nRegValue);
			if (max96705_gmsl_set_reg[j].nDelay > 0)
				usleep_range(max96705_gmsl_set_reg[j].nDelay,
							 max96705_gmsl_set_reg[j].nDelay+100);
		}
	}
	//enable input link/vsync from camera
	max9286_write_reg(serdes, 0x0, DES_REG0_INPUTLINK_EN);
	//fs, manual pclk
	//pclk 72MHz fps 25 the number of pclk in one frame = 72MHz/25 = 2880,000=0x2bf200
	max9286_write_reg(serdes, 0x06, 0x00);
	max9286_write_reg(serdes, 0x07, 0xf2);
	max9286_write_reg(serdes, 0x08, 0x2b);
	usleep_range(10000, 11000);
	for (i = 0; i < MAX_SENSOR_NUM; i++) {
		//enable all serial links
		max96705_write_reg(serdes, serdes->remap_addr_ser[i], 0x04, 0x83);
		usleep_range(5000, 6000);
	}
	usleep_range(5000, 6000);
	//internal fs, debug for output
	max9286_write_reg(serdes, 0x1, DES_REG1_FS_EN);
	return 0;
}

static void max9286_serdes_power(struct serdes_dev_t *serdes, bool enable)
{
    if (enable) {
        max9286_power(serdes, false);
        usleep_range(10000, 10100);
        max9286_power(serdes, true);
        usleep_range(10000, 10100);
    }
    else {
        max9286_power(serdes, false);
    }
}

int max9286_fengjing_initialization(struct serdes_dev_t *serdes)
{
	int ret = 0;
    u8 chip_id;

    ret = max9286_read_reg(serdes, 0x1e, &chip_id);
	if (chip_id != MAX9286_DEVICE_ID) {
		dev_err(&serdes->i2c_client->dev,
				"%s: wrong chip identifier, expected 0x%x(max9286), got 0x%x\n",
				__func__, MAX9286_DEVICE_ID, chip_id);
		return -ENXIO;
	}

	ret = max9286_initial_setup(serdes);
	if (ret < 0)
		return ret;
	max9286_reverse_channel_setup(serdes);
	max9286_max96705_gmsl_link_setup(serdes);
	return ret;
}
EXPORT_SYMBOL(max9286_fengjing_initialization);

static const int fengjing_formats[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

static int fengjing_enum_mbus_code(struct v4l2_subdev *sd,
								   struct v4l2_subdev_pad_config *cfg,
								   struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;
	if (code->index >= ARRAY_SIZE(fengjing_formats))
		return -EINVAL;
	code->code = fengjing_formats[code->index];
	return 0;
}



int max9286_check_chip_id(struct serdes_dev_t *serdes)
{
	struct i2c_client *client = serdes->i2c_client;
	int ret = 0;
	u8 chip_id = 0;

	max9286_power(serdes, true);
	ret = max9286_read_reg(serdes, 0x1e, &chip_id);
	if (ret) {
		dev_err(&client->dev, "%s: failed to read chip identifier\n",
				__func__);
	}
	if (chip_id != MAX9286_DEVICE_ID) {
		dev_err(&client->dev,
				"%s: wrong chip identifier, expected 0x%x(max9286), got 0x%x\n",
				__func__, MAX9286_DEVICE_ID, chip_id);
		ret = -ENXIO;
	}
	return ret;
}
EXPORT_SYMBOL(max9286_check_chip_id);

serdes_para_t max9286_fengjing_MC163_MAX96705_720p = {
	.module_name = "max9286_fengjing_MC163_MAX96705_720p",
	.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.fps	= 25,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.field		= V4L2_FIELD_NONE,
	.numerator	= 1,
	.serdes_power = max9286_serdes_power,
	.serdes_start = max9286_out_enable,
	.serdes_init = max9286_fengjing_initialization,
	.serdes_check_link_status = max9286_get_video_links,
	.serdes_check_link_status_ex = max9286_get_link_lock_ex,
	.serdes_enum_mbus_code = fengjing_enum_mbus_code,
	.serdes_check_video_lock_status = max9286_get_video_lock,
	.delay_time = 200,
};


MODULE_DESCRIPTION("MAX9286 reg init Driver");
MODULE_LICENSE("GPL");
