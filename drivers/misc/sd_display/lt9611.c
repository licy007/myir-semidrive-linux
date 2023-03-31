/*
 * Copyright (C) 2020 semidrive Semiconductor Co., Ltd.
 *
 * LT9611 MIPI to HDMI driver
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "bridge.h"

#define LT9611_DEVICE_NAME     "bridge lt9611"
#define LT9611_DEVICE_I2C_ADDR 0x39

#define LT9611_CHIP_ID_H       (0x17)
#define LT9611_CHIP_ID_L       (0x02)

#define LT9611_REG_END         0xffff
#define LT9611_REG_DELAY       0xfffe

#define HPD_INTERRUPT_ENABLE   0x01
#define VID_INTERRUPT_ENABLE   0x02
#define CEC_INTERRUPT_ENABLE   0x03

#define AR_ND                  0x00
#define AR_4_3                 0x01
#define AR_16_9                0x02

#define MPEG_PKT_EN            0x01
#define AIF_PKT_EN             0x02
#define SPD_PKT_EN             0x04
#define AVI_PKT_EN             0x08
#define UD1_PKT_EN             0x10
#define UD0_PKT_EN             0x20

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

enum video_time_type {
	video_640x480_60Hz_vic1 = 1,
	video_720x480_60Hz_vic3,
	video_720x576_50Hz_vic,

	video_1280x720_60Hz_vic4,
	video_1280x720_50Hz_vic,
	video_1280x720_30Hz_vic,

	video_3840x2160_30Hz_vic,
	video_3840x2160_25Hz_vic,
	video_3840x2160_24Hz_vic,
	video_3840x1080_60Hz_vic,

	video_2560x1600_60Hz_vic,
	video_2560x1440_60Hz_vic,
	video_2560x1080_60Hz_vic,

	video_1920x1080_60Hz_vic16,
	video_1920x1080_50Hz_vic,
	video_1920x1080_30Hz_vic,
	video_1920x1080_25Hz_vic,
	video_1920x1080_24Hz_vic,

	video_other,
	video_none
};

struct video_timing {
	unsigned int hfp;
	unsigned int hs;
	unsigned int hbp;
	unsigned int hact;
	unsigned int htotal;
	unsigned int vfp;
	unsigned int vs;
	unsigned int vbp;
	unsigned int vact;
	unsigned int vtotal;
	unsigned char h_polarity;
	unsigned char v_polarity;
	unsigned int vic;
	unsigned char aspact_ratio;  // 0=no data, 1=4:3, 2=16:9, 3=no data.
	unsigned int pclk_khz;
	unsigned int sysclk;
	enum video_time_type format;
};

static struct i2c_client *i2c_dev;
static struct bridge_attr lt9611_bridge_attr;
static struct gpio_desc *gpio_power_en = NULL;
static struct gpio_desc *gpio_rst = NULL;
struct video_timing *video;
static unsigned char pcr_m;
static unsigned char edid_array[512];
enum video_time_type time_type;

static int lt9611_write(struct i2c_client *i2c, unsigned char reg, unsigned char value)
{
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr   = i2c->addr,
		.flags  = 0,
		.len    = 2,
		.buf    = buf,
	};

	int ret = i2c_transfer(i2c->adapter, &msg, 1);
	if (ret < 0)
		printk(KERN_ERR "lt9611: failed to write reg: %x\n", (int)reg);

	return ret;
}

static int lt9611_read(struct i2c_client *i2c, unsigned char reg, unsigned char *value)
{
	unsigned char buf[1] = {reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr   = i2c->addr,
			.flags  = 0,
			.len    = 1,
			.buf    = buf,
		},
		[1] = {
			.addr   = i2c->addr,
			.flags  = I2C_M_RD,
			.len    = 1,
			.buf    = value,
		}
	};

	int ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0)
		printk(KERN_ERR "lt9611(%x): failed to read reg: %x\n", i2c->addr, (int)reg);

	return ret;
}

static int lt9611_write_array(struct i2c_client *i2c, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != LT9611_REG_END) {
		if (vals->reg_num == LT9611_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = lt9611_write(i2c, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		// printk(KERN_ERR "vals->reg_num:%x, vals->value:%x\n", vals->reg_num, vals->value);
		vals++;
	}

	return 0;
}

static int lt9611_detect(struct i2c_client *i2c)
{
	unsigned char h, l;
	int ret;

	lt9611_write(i2c, 0xff, 0x80);
	lt9611_write(i2c, 0xee, 0x01);

	ret = lt9611_read(i2c, 0x00, &h);
	if (ret < 0)
		return ret;

	if (h != LT9611_CHIP_ID_H) {
		printk(KERN_ERR "lt9611 read chip id high failed:0x%x\n", h);
		return -ENODEV;
	}

	ret = lt9611_read(i2c, 0x01, &l);
	if (ret < 0)
		return ret;

	if (l != LT9611_CHIP_ID_L) {
		printk(KERN_ERR "lt9611 read chip id high failed:0x%x\n", l);
		return -ENODEV;
	}
	printk(KERN_ERR "lt9611 chip id is 0x%02x%02x\n", h, l);

	return 0;
}

/*
 * only hpd irq is working for low power consumption
 */
static int lt9611_lowpower_mode(struct i2c_client *i2c, char enable)
{
	int ret = 0;

	if (enable) {
		struct regval_list list[] = {
			{0xFF, 0x81},
			{0x02, 0x49},
			{0x23, 0x80},
			{0x30, 0x00},
			{0xff, 0x80},
			{0x11, 0x0a},
			{LT9611_REG_END, 0x00},
		};
		ret = lt9611_write_array(i2c, list);
		if (ret)
			printk(KERN_ERR "lt9611: failed to enter low power mode\n");
		printk(KERN_ERR  "lt9611: enter low power mode\n");
	}

	if (!enable) {
		struct regval_list list[] = {
			{0xFF, 0x81},
			{0x02, 0x12},
			{0x23, 0x40},
			{0x30, 0xea},
			{0xff, 0x80},
			{0x11, 0xfa},
			{LT9611_REG_END, 0x00},
		};
		ret = lt9611_write_array(i2c, list);
		if (ret)
			printk(KERN_ERR "lt9611: failed to exit low power mode\n");
		printk(KERN_ERR  "lt9611: exit low power moden");
	}

	return 0;
}

static int lt9611_system_init(struct i2c_client *i2c)
{
	int ret;
	unsigned char reg_lane;

	if (lt9611_bridge_attr.mipi.lanes == 4)
		reg_lane = 0;

	struct regval_list list[] = {
		{0xff, 0x82},
		{0x51, 0x11},
		/* Timer for Frequency meter */
		{0xff, 0x82},
		{0x1b, 0x69}, // Timer 2
		{0x1c, 0x78},
		{0xcb, 0x69}, // Timer 1
		{0xcc, 0x78},
		/* power consumption for work */
		{0xff, 0x80},
		{0x04, 0xf0},
		{0x06, 0xf0},
		{0x0a, 0x80},
		{0x0b, 0x46}, //csc clk
		{0x0d, 0xef},
		{0x11, 0xfa},
		{0xFF, 0x81},
		{0x01, 0x18}, //sel xtal clock
		{0xFF, 0x80},
		{0xff, 0x81},    // mipi mode
		{0x06, 0x60},    // port A rx current
		{0x07, 0x3f},    // eq
		{0x08, 0x3f},    // eq
		{0x0a, 0xfe},    // port A ldo voltage set
		{0x0b, 0xbf},    // enable port A lprx
		{0x11, 0x60},    // port B rx current
		{0x12, 0x3f},    // eq
		{0x13, 0x3f},    // eq
		{0x15, 0xfe},    // port B ldo voltage set
		{0x16, 0xbf},    // enable port B lprx
		{0x1c, 0x03},    // PortA clk lane no-LP mode.
		{0x20, 0x03},    // PortB clk lane no-LP mode.
		{0xff, 0x82},
		{0x4f, 0x80},    //[7] = Select ad_txpll_d_clk.
		{0x50, 0x10},
		{0xff, 0x83},
		{0x00, reg_lane},
		{0x02, 0x0A},    // settle
		{0x06, 0x0A},    // settle
		{0x0a, 0x00},    // 1=dual_lr, 0=dual_en
		{LT9611_REG_END, 0x00},
	};
	ret = lt9611_write_array(i2c, list);
	if (ret)
		printk(KERN_ERR "lt9611: failed to init system\n");

	return ret;
}

static struct video_timing timing_list[] = {
	{16, 96, 48, 640, 800, 10, 2, 33, 480, 525, 0, 0, 1, AR_4_3, 25000, 0, video_640x480_60Hz_vic1},
	{16, 62, 60, 720, 858, 9, 6, 30, 480, 525, 0, 0, 2, AR_4_3, 27000, 0, video_720x480_60Hz_vic3},
	{12, 64, 68, 720, 864, 5, 5, 39, 576, 625, 0, 0, 17, AR_4_3, 27000, 0, video_720x576_50Hz_vic},

	{110, 40, 220, 1280, 1650, 5, 5, 20, 720, 750, 1, 1, 4, AR_16_9, 74250, 630, video_1280x720_60Hz_vic4},
	{440, 40, 220, 1280, 1980, 5, 5, 20, 720, 750, 1, 1, 19, AR_16_9, 74250, 750, video_1280x720_50Hz_vic},
	{1760, 40, 220, 1280, 3300, 5, 5, 20, 720, 750, 1, 1, 0, AR_16_9, 74250, 1230, video_1280x720_30Hz_vic},

	{88, 44, 148, 1920, 2200, 4, 5, 36, 1080, 1125, 1, 1, 16, AR_16_9, 148500, 430, video_1920x1080_60Hz_vic16},
	{528, 44, 148, 1920, 2640, 4, 5, 36, 1080, 1125, 1, 1, 31, AR_16_9, 148500, 510, video_1920x1080_50Hz_vic},
	{88, 44, 148, 1920, 2200, 4, 5, 36, 1080, 1125, 1, 1, 34, AR_16_9, 74250, 830, video_1920x1080_30Hz_vic},
	{528, 44, 148, 1920, 2640, 4, 5, 36, 1080, 1125, 1, 1, 33, AR_16_9, 74250, 980, video_1920x1080_25Hz_vic},
	{638, 44, 148, 1920, 2750, 4, 5, 36, 1080, 1125, 1, 1, 32, AR_16_9, 74250, 1030, video_1920x1080_24Hz_vic},

	{176, 88, 296, 3840, 4400, 8, 10, 72, 2160, 2250, 1, 1, 95, AR_16_9, 297000, 430, video_3840x2160_30Hz_vic},
	{1056, 88, 296, 3840, 5280, 8, 10, 72, 2160, 2250, 1, 1, 94, AR_16_9, 297000, 490, video_3840x2160_25Hz_vic},
	{1276, 88, 296, 3840, 5500, 8, 10, 72, 2160, 2250, 1, 1, 93, AR_16_9, 297000, 520, video_3840x2160_24Hz_vic},

	{180,60, 160, 1200, 1600, 35, 10, 25, 1920, 1990, 1, 1, 0, AR_16_9, 191040, 0, video_other},
	{88, 44, 148, 1920, 2200, 5, 5, 20, 720,  750, 1, 1, 0, AR_16_9, 100000, 0, video_other},
};

static int lt9611_check_video_timing(struct i2c_client *i2c)
{
	unsigned char mipi_video_format;
	unsigned char h, l;
	unsigned int h_act, h_act_a ,h_act_b, v_act, v_tal;
	unsigned int h_total_sysclk;
	struct video_timing *timing;
	int i;

	mdelay(100);
	lt9611_write(i2c, 0xff,0x82); // top video check module
	lt9611_read(i2c, 0x86, &h);
	lt9611_read(i2c, 0x87, &l);
	h_total_sysclk = (h << 8) + l;

	lt9611_read(i2c, 0x82, &h);
	lt9611_read(i2c, 0x83, &l);
	v_act = (h << 8) + l;

	lt9611_read(i2c, 0x6c, &h);
	lt9611_read(i2c, 0x6d, &l);
	v_tal = (h << 8) + l;

	lt9611_write(i2c, 0xff,0x83);
	lt9611_read(i2c, 0x82, &h);
	lt9611_read(i2c, 0x83, &l);
	h_act_a = (h << 8) + l;

	lt9611_read(i2c, 0x86, &h);
	lt9611_read(i2c, 0x87, &l);
	h_act_b = (h << 8) + l;

	if (lt9611_bridge_attr.mipi.data_fmt == MIPI_YUV422) {
		h_act_a /= 2;
		h_act_b /= 2;
	}

	if (lt9611_bridge_attr.mipi.data_fmt == MIPI_RGB888) {
		h_act_a /= 3;
		h_act_b /= 3;
	}

	lt9611_read(i2c, 0x88, &mipi_video_format);
	printk(KERN_ERR "h_act_a, h_act_b, v_act, v_tal: %d, %d, %d, %d\n", h_act_a, h_act_b, v_act, v_tal);

	if (0) // dual port
		h_act = h_act_a + h_act_b;
	else
		h_act = h_act_a;

	lt9611_write(i2c, 0xFF, 0x81);
	lt9611_write(i2c, 0x01, 0x18); //sel xtal clock
	lt9611_write(i2c, 0xFF, 0x80);

	for (i = 0; i < sizeof(timing_list)/sizeof(timing_list[0]); i++) {
		timing = &timing_list[i];
		if (h_act != timing->hact || v_act != timing->vact)
			continue;

		if (timing->sysclk == 0 || h_total_sysclk < timing->sysclk) {
				time_type = timing->format;
				video = timing;
				return 0;
		}
	}

	return -1;
}

static int lt9611_mipi_video_timing(struct i2c_client *i2c, struct video_timing *timing)
{
	int ret = 0;
	struct regval_list list[] = {
		{0xff, 0x83},
		{0x0d, timing->vtotal/256},
		{0x0e, timing->vtotal%256},                  // vtotal
		{0x0f, timing->vact/256},
		{0x10, timing->vact%256},                    // vactive
		{0x11, timing->htotal/256},
		{0x12, timing->htotal%256},                  // htotal
		{0x13, timing->hact/256},
		{0x14, timing->hact%256},                    // hactive
		{0x15, timing->vs%256},                      // vsa
		{0x16, timing->hs%256},                      // hsa
		{0x17, timing->vfp%256},                     // vfp
		{0x18, (timing->vs+timing->vbp)%256},  // vss
		{0x19, timing->hfp%256},                     // hfp
		{0x1a, ((timing->hfp/256)<<4)+(timing->hs+timing->hbp)/256},
		{0x1b, (timing->hs+timing->hbp)%256},  // hss
		{LT9611_REG_END, 0x00},
	};
	ret = lt9611_write_array(i2c, list);
	if (ret)
		printk(KERN_ERR "lt9611: failed to set video timing\n");

	return ret;
}

static void lt9611_mipi_pcr(struct i2c_client *i2c, struct video_timing *timing)
{
	unsigned char POL;

	POL = (timing-> h_polarity)*0x02 + (timing-> v_polarity);
	POL = ~POL;
	POL &= 0x03;

	lt9611_write(i2c, 0xff, 0x83);
	lt9611_write(i2c, 0x0b, 0x01); //vsync read delay(reference value)
	lt9611_write(i2c, 0x0c, 0x10); //
	lt9611_write(i2c, 0x48, 0x00); //de mode delay
	lt9611_write(i2c, 0x49, 0x81); //=1/4 hact

	/* stage 1 */
	lt9611_write(i2c, 0x21, 0x4a); //bit[3:0] step[11:8]
	lt9611_write(i2c, 0x24, 0x71); //bit[7:4]v/h/de mode; line for clk stb[11:8]
	lt9611_write(i2c, 0x25, 0x30); //line for clk stb[7:0]
	lt9611_write(i2c, 0x2a, 0x01); //clk stable in

	/* stage 2 */
	lt9611_write(i2c, 0x4a, 0x40); //offset //0x10
	lt9611_write(i2c, 0x1d, (0x10|POL)); //PCR de mode step setting.

	switch (time_type) {
	case video_3840x1080_60Hz_vic:
	case video_3840x2160_30Hz_vic:
	case video_3840x2160_25Hz_vic:
	case video_3840x2160_24Hz_vic:
	case video_2560x1600_60Hz_vic:
	case video_2560x1440_60Hz_vic:
	case video_2560x1080_60Hz_vic:
		break;
	case video_1920x1080_60Hz_vic16:
	case video_1920x1080_30Hz_vic:
	case video_1280x720_60Hz_vic4:
	case video_1280x720_30Hz_vic:
		break;
	case video_720x480_60Hz_vic3:
	case video_640x480_60Hz_vic1:
		lt9611_write(i2c, 0xff, 0x83);
		lt9611_write(i2c, 0x0b, 0x02);
		lt9611_write(i2c, 0x0c, 0x40);
		lt9611_write(i2c, 0x48, 0x01);
		lt9611_write(i2c, 0x49, 0x10);
		lt9611_write(i2c, 0x24, 0x70);
		lt9611_write(i2c, 0x25, 0x80);
		lt9611_write(i2c, 0x2a, 0x10);
		lt9611_write(i2c, 0x2b, 0x80);
		lt9611_write(i2c, 0x23, 0x28);
		lt9611_write(i2c, 0x4a, 0x10);
		lt9611_write(i2c, 0x1d, 0xf3);
		break;
	default: break;
	}

	lt9611_mipi_video_timing(i2c, video);
	lt9611_write(i2c, 0xff, 0x83);
	lt9611_write(i2c, 0x26, pcr_m);
	lt9611_write(i2c, 0xff, 0x80);
	lt9611_write(i2c, 0x11, 0x5a); //Pcr reset
	lt9611_write(i2c, 0x11, 0xfa);
}

static void lt9611_set_pll(struct i2c_client *i2c, struct video_timing *video_format)
{
	unsigned int pclk, pclk_div;
	unsigned char pll_lock_flag, cal_done_flag, band_out;
	unsigned char post_div;
	// unsigned char pcr_m;
	unsigned char reg_2d;
	unsigned char i;

	pclk = video_format->pclk_khz;
	if (pclk > 150000) {
		reg_2d = 0x88;
		post_div = 0x01;
	} else if (pclk > 80000) {
		post_div = 0x02;
		reg_2d = 0x99;
	} else {
		post_div = 0x04;
		reg_2d = 0xaa;
	}

	pcr_m = (unsigned char)((pclk * 5 * post_div) / 27000);
	pcr_m--;
	pclk_div = pclk / 2;

	struct regval_list list[] = {
		{0xff, 0x81},
		{0x23, 0x40}, /* Enable LDO and disable PD */
		{0x24, 0x62}, /* 0x62, LG25UM58 issue, 20180824 */
		{0x25, 0x80},
		{0x26, 0x55},
		{0x2c, 0x37},
		{0x2f, 0x01},
		{0x27, 0x66},
		{0x28, 0x88},
		{0x2a, 0x20},
		{0x2d, reg_2d},
		{0xff, 0x83},
		{0x2d, 0x40},         /* M up limit */
		{0x31, 0x08},         /* M down limit */
		{0x26, 0x80 | pcr_m}, /* fixed M is to let pll locked*/
		{0xff, 0x82},
		{0xe3, pclk_div / 65536},
		{0xe4, pclk_div % 65536 / 256},
		{0xe5, pclk_div % 65536 % 256},
		{0xde, 0x20}, /* pll cal en, start calibration */
		{0xde, 0xe0},
		{0xff, 0x80},
		{0x11, 0x5a}, /* Pcr clk reset */
		{0x11, 0xfa},
		{0x16, 0xf2}, /* pll cal digital reset */
		{0x18, 0xdc}, /* pll analog reset */
		{0x18, 0xfc},
		{0x16, 0xf3}, /* start calibration */
		{LT9611_REG_END, 0x00},
	};

	lt9611_write_array(i2c, list);

	/* pll lock status */
	for (i = 0; i < 6 ; i++) {
		lt9611_write(i2c, 0xff, 0x80);
		lt9611_write(i2c, 0x16, 0xe3); /* pll lock logic reset */
		lt9611_write(i2c, 0x16, 0xf3);
		lt9611_write(i2c, 0xff, 0x82);
		lt9611_read(i2c, 0xe7, &cal_done_flag);
		lt9611_read(i2c, 0xe6, &band_out);
		lt9611_read(i2c, 0x15, &pll_lock_flag);

		if ((pll_lock_flag & 0x80)&&(cal_done_flag & 0x80)&&(band_out != 0xff))
			break;
		else {
			lt9611_write(i2c, 0xff, 0x80);
			lt9611_write(i2c, 0x11, 0x5a); /* Pcr clk reset */
			lt9611_write(i2c, 0x11, 0xfa);
			lt9611_write(i2c, 0x16, 0xf2); /* pll cal digital reset */
			lt9611_write(i2c, 0x18, 0xdc); /* pll analog reset */
			lt9611_write(i2c, 0x18, 0xfc);
			lt9611_write(i2c, 0x16, 0xf3); /*start calibration*/
			printk(KERN_ERR "HDMI pll unlocked, reset pll");
		}
	}
}

static int lt9611_hdmi_tx_phy(struct i2c_client *i2c)
{
	unsigned char couple_mode = 0x73;

	if (lt9611_bridge_attr.hdmi.couple == mode_AC)
		couple_mode = 0x73;

	if (lt9611_bridge_attr.hdmi.couple == mode_DC)
		couple_mode = 0x31;

	struct regval_list list[] = {
		{0xff, 0x08},
		{0x30, 0x06},
		{0x31, couple_mode},
		{0x32, 0x04},
		{0x33, 0x00},
		{0x34, 0x00},
		{0x35, 0x00},
		{0x36, 0x00},
		{0x37, 0x04},
		{0x3f, 0x00},
		{0x40, 0x09}, // clk swing
		{0x41, 0x09}, // D0 swing
		{0x42, 0x09}, // D1 swing
		{0x43, 0x09}, // D2 swing
		{0x44, 0x00},
		{LT9611_REG_END, 0x00},
	};

	return lt9611_write_array(i2c, list);
}

static int lt9611_hdcp_init(struct i2c_client *i2c)
{
	int ret;
	struct regval_list list[] = {
		{0xff, 0x85},
		{0x07, 0x1f},
		{0x13, 0xfc},
		{0x17, 0x0f},
		{0x15, 0x05},
		// {0x15, 0x65},
		{LT9611_REG_END, 0x00},
	};

	ret = lt9611_write_array(i2c, list);
	if (ret)
		printk(KERN_ERR "lt9611: failed to init hdcp\n");

	return ret;
}

static int lt9611_hdcp_enable(struct i2c_client *i2c)
{
	int ret;
	struct regval_list list[] = {
		{0xff, 0x80},
		{0x14, 0x7f},
		{0x14, 0xff},
		{0xff, 0x85},
		{0x15, 0x01},    // disable HDCP
		{0x15, 0x71},    // enable HDCP
		{0x15, 0x65},    // enable HDCP
		{LT9611_REG_END, 0x00},
	};

	ret = lt9611_write_array(i2c, list);
	if (ret)
		printk(KERN_ERR "lt9611: failed to enable hdcp\n");

	return 0;
}

static int lt9611_hdcp_disable(struct i2c_client *i2c)
{
	int ret;

	ret = lt9611_write(i2c, 0xff, 0x85);
	ret += lt9611_write(i2c, 0x15 ,0x45);
	if (ret)
		printk(KERN_ERR "lt9611: failed to disable hdcp\n");

	return ret;
}

static int lt9611_hdmi_out_enable(struct i2c_client *i2c)
{
	int ret;
	struct regval_list list[] = {
		{0xff, 0x81},
		{0x23, 0x40},
		{0xff, 0x82},
		{0xde, 0x20},
		{0xde, 0xe0},
		{0xff, 0x80},
		{0x18, 0xdc},    /* txpll sw rst */
		{0x18, 0xfc},
		{0x16, 0xf1},    /* txpll calibration rest */
		{0x16, 0xf3},
		{0x11, 0x5a},    /* Pcr reset */
		{0x11, 0xfa},
		{0xff, 0x81},
		{0x30, 0xea},
		{LT9611_REG_END, 0x00},
	};
	ret = lt9611_write_array(i2c, list);
	if (ret) {
		printk(KERN_ERR "lt9611: failed to enable hdmi out\n");
		return ret;
	}

	if (lt9611_bridge_attr.hdmi.en_hdcp == 1)
		lt9611_hdcp_enable(i2c);
	else
		lt9611_hdcp_disable(i2c);

	return 0;
}

static int lt9611_hdmi_out_disable(struct i2c_client *i2c)
{
	int ret;

	ret = lt9611_write(i2c, 0xff, 0x81);
	ret += lt9611_write(i2c, 0x30, 0x00); /* Txphy PD */
	ret += lt9611_write(i2c, 0x23, 0x80); /* Txpll PD */
	if (ret)
		printk(KERN_ERR "lt9611: failed to disable hdmi out\n");

	lt9611_hdcp_disable(i2c);
	return ret;
}

void lt9611_hdmi_tx(struct i2c_client *i2c, struct video_timing *video_format)
{
	unsigned char VIC = video_format->vic;
	unsigned char AR = video_format->aspact_ratio;
	unsigned char pb0,pb2,pb4;
	unsigned char infoFrame_en;

	infoFrame_en = (AIF_PKT_EN|AVI_PKT_EN|SPD_PKT_EN);
	pb2 = (AR<<4) + 0x08;
	pb4 = VIC;
	pb0 = ((pb2 + pb4) <= 0x5f)?(0x5f - pb2 - pb4):(0x15f - pb2 - pb4);

	lt9611_write(i2c, 0xff,0x82);

	if (lt9611_bridge_attr.hdmi.port == port_HDMI)
		lt9611_write(i2c, 0xd6, 0x8e); //sync polarity
	else if(lt9611_bridge_attr.hdmi.port == port_DVI)
		lt9611_write(i2c, 0xd6, 0x0e); //sync polarity

	if (lt9611_bridge_attr.hdmi.audio.port == port_IIS)
		lt9611_write(i2c, 0xd7, 0x04);

	if (lt9611_bridge_attr.hdmi.audio.port == port_SPD)
		lt9611_write(i2c, 0xd7, 0x80);

	lt9611_write(i2c, 0xff, 0x84);
	lt9611_write(i2c, 0x43, pb0);   //AVI_PB0

	lt9611_write(i2c, 0x45, pb2);  //AVI_PB2
	lt9611_write(i2c, 0x47, pb4);   //AVI_PB4

	lt9611_write(i2c, 0xff, 0x84);
	lt9611_write(i2c, 0x10, 0x02); //data iland
	lt9611_write(i2c, 0x12, 0x64); //act_h_blank

	/* VS_IF, 4k 30hz need send VS_IF packet. */
	if (VIC == 95) {
		lt9611_write(i2c, 0xff, 0x84);
		lt9611_write(i2c, 0x3d, infoFrame_en|UD0_PKT_EN);

		lt9611_write(i2c, 0x74, 0x81);  //HB0
		lt9611_write(i2c, 0x75, 0x01);  //HB1
		lt9611_write(i2c, 0x76, 0x05);  //HB2
		lt9611_write(i2c, 0x77, 0x49);  //PB0
		lt9611_write(i2c, 0x78, 0x03);  //PB1
		lt9611_write(i2c, 0x79, 0x0c);  //PB2
		lt9611_write(i2c, 0x7a, 0x00);  //PB3
		lt9611_write(i2c, 0x7b, 0x20);  //PB4
		lt9611_write(i2c, 0x7c, 0x01);  //PB5
	} else {
		lt9611_write(i2c, 0xff,0x84);
		lt9611_write(i2c, 0x3d,infoFrame_en); //UD1 infoframe enable
	}

	if (infoFrame_en&&SPD_PKT_EN) {
		lt9611_write(i2c, 0xff, 0x84);
		lt9611_write(i2c, 0xc0, 0x83);  //HB0
		lt9611_write(i2c, 0xc1, 0x01);  //HB1
		lt9611_write(i2c, 0xc2, 0x19);  //HB2

		lt9611_write(i2c, 0xc3, 0x00);  //PB0
		lt9611_write(i2c, 0xc4, 0x01);  //PB1
		lt9611_write(i2c, 0xc5, 0x02);  //PB2
		lt9611_write(i2c, 0xc6, 0x03);  //PB3
		lt9611_write(i2c, 0xc7, 0x04);  //PB4
		lt9611_write(i2c, 0xc8, 0x00);  //PB5
	}
}

static int lt9611_audio_init(struct i2c_client *i2c)
{
	int ret;

	if (lt9611_bridge_attr.hdmi.audio.port == port_IIS) {
		struct regval_list list[] = {
			{0xff, 0x84},
			{0x06, 0x08},
			{0x07, 0x10},
			{0x0f, 0x2b}, /* 48K sampling frequency */
			{0x34, 0xd4}, /* CTS_N 20180823 0xd5: sclk = 32fs, 0xd4: sclk = 64fs */
			{0x35, 0x00},
			{0x36, 0x18},
			{0x37, 0x00},
			{LT9611_REG_END, 0x00},
		};
		ret = lt9611_write_array(i2c, list);
	}

	if (lt9611_bridge_attr.hdmi.audio.port == port_SPD) {
		struct regval_list list[] = {
			{0xff, 0x84},
			{0x06, 0x0c},
			{0x07, 0x10},
			{0x34, 0xd4}, //CTS_N
			{LT9611_REG_END, 0x00},
		};
		ret = lt9611_write_array(i2c, list);
	}

	if (ret)
		printk(KERN_ERR "lt9611: failed to init audio\n");

	return ret;
}

static __maybe_unused int lt9611_read_edid(struct i2c_client *i2c)
{
	unsigned int i, j;
	unsigned char extern_flag = 0;
	unsigned char val;
	int size = 0;

	memset(edid_array, 0, sizeof(edid_array));

	lt9611_write(i2c, 0xff, 0x85);
	lt9611_write(i2c, 0x03, 0xc9);
	lt9611_write(i2c, 0x04, 0xa0); //0xA0 is EDID device address
	lt9611_write(i2c, 0x05, 0x00); //0x00 is EDID offset address
	lt9611_write(i2c, 0x06, 0x20); //length for read
	lt9611_write(i2c, 0x14, 0x7f);

	/* block 0 & 1 */
	for (i = 0; i < 8; i++) {
		lt9611_write(i2c, 0x05, i*32);
		lt9611_write(i2c, 0x07, 0x36);
		lt9611_write(i2c, 0x07, 0x34);
		lt9611_write(i2c, 0x07, 0x37);

		mdelay(5); /* wait 5ms */
		lt9611_read(i2c, 0x40, &val);
		if (!(val&0x02))
			goto err_edid;

		lt9611_read(i2c, 0x40, &val);
		if (val&0x50)
			goto err_edid;

		for (j = 0; j < 32; j++) {
			lt9611_read(i2c, 0x83, &val);
			edid_array[i*32+j] = val;
			size++;
			if ((i == 3) && (j == 30))
				extern_flag = val & 0x03;
		}

		if((i == 3) && (extern_flag < 1)) // no block 1
			goto err_edid;
	}

	if(extern_flag < 2) /* no block 2 */
		goto err_edid;

	/* block 2 & 3 */
	for (i = 0; i < 8; i++) {
		lt9611_write(i2c, 0x05, i*32);
		lt9611_write(i2c, 0x07, 0x76);
		lt9611_write(i2c, 0x07, 0x74);
		lt9611_write(i2c, 0x07, 0x77);

		mdelay(5);
		lt9611_read(i2c, 0x40, &val);
		if(!(val&0x02))
			goto err_edid;

		lt9611_read(i2c, 0x40, &val);
		if (val&0x50)
			goto err_edid;

		for (j = 0; j < 32; j++) {
			lt9611_read(i2c, 0x83, &val);
			edid_array[256+i*32+j] = val;
			size++;
		}

		if ((i == 3) && (extern_flag < 3)) // no block 3
			goto err_edid;
	}

	for (i = 0; i < 512; i++) {
		printk(KERN_ERR "0x%02x ", edid_array[i]);
		if (i % 32 == 0)
			printk(KERN_ERR "\n");
	}

err_edid:
	lt9611_write(i2c, 0x03, 0xc2);
	lt9611_write(i2c, 0x07, 0x1f);
	return size;
}

static __maybe_unused void dump_edid(int size)
{
	int i;

	printk(KERN_ERR "edid size %d\n", size);
	for (i = 0; i < size; i++) {
		if (i % 32 == 0)
			printk(KERN_ERR "\n");
		printk(KERN_ERR "0x%02x ", edid_array[i]);
	}
}

static int lt9611_get_hpd_status(struct i2c_client *i2c)
{
	unsigned char reg_5e;

	lt9611_write(i2c, 0xff,0x82);
	lt9611_read(i2c, 0x5e, &reg_5e);
	if ((reg_5e & 0x04) == 0x04)
		return 1;

	return 0;
}

static void lt9611_freq_meter(struct i2c_client *i2c)
{
	unsigned char temp;
	unsigned int reg=0x00;

	/* port A byte clk meter */
	lt9611_write(i2c, 0xff,0x82);
	lt9611_write(i2c, 0xc7,0x03); //PortA
	mdelay(5);
	lt9611_read(i2c, 0xcd, &temp);
	if ((temp&0x60) == 0x60) {
		reg = (unsigned int)(temp&0x0f)*65536;
		lt9611_read(i2c, 0xce, &temp);
		reg = reg + (unsigned short)temp*256;
		lt9611_read(i2c, 0xcf, &temp);
		reg = reg + temp;
		printk(KERN_ERR  "port A byte clk = %d\n",reg);
	}
	else
		printk(KERN_ERR  "port A byte clk unstable\n");

	/* port B byte clk meter */
	lt9611_write(i2c, 0xff, 0x82);
	lt9611_write(i2c, 0xc7, 0x04);
	mdelay(5);
	lt9611_read(i2c, 0xcd, &temp);
	if ((temp&0x60)==0x60) {
		reg = (unsigned int)(temp&0x0f)*65536;
		lt9611_read(i2c, 0xce, &temp);
		reg = reg + (unsigned short)temp*256;
		lt9611_read(i2c, 0xcf, &temp);
		reg = reg + temp;
		printk(KERN_ERR  "port B byte clk = %d\n",reg);
	}
	else
		printk(KERN_ERR  "port B byte clk unstable\n");
}

static int lt9611_init_irq(struct i2c_client *i2c)
{
	int ret = 0;
	struct regval_list list[] = {
		{0xff, 0x82},
		{0x58, 0x0a},   // Det HPD 0x0a
		{0x59, 0x80},   // HPD debounce width
		{0x9e, 0xf7},
		{LT9611_REG_END, 0x00},
	};
	ret = lt9611_write_array(i2c, list);
	if (ret)
		printk(KERN_ERR "lt9611: failed to init irq\n");

	return ret;
}

static void lt9611_set_interrupt(struct i2c_client *i2c, char int_type, char enable)
{
	switch (int_type) {
	case HPD_INTERRUPT_ENABLE:
		if (enable) {
			lt9611_write(i2c, 0xff, 0x82);
			lt9611_write(i2c, 0x07, 0xff); //clear3
			lt9611_write(i2c, 0x07, 0x3f); //clear3
			lt9611_write(i2c, 0x03, 0x3f); //mask3  //Tx_det
		}

		if (!enable) {
			lt9611_write(i2c, 0xff, 0x82);
			lt9611_write(i2c, 0x07, 0xff); //clear3
			lt9611_write(i2c, 0x03, 0xff); //mask3  //Tx_det
		}
		break;
	case VID_INTERRUPT_ENABLE:
		if (enable) {
			lt9611_write(i2c, 0xff, 0x82);
			lt9611_write(i2c, 0x9e, 0xff); //clear vid chk irq
			lt9611_write(i2c, 0x9e, 0xf7);
			lt9611_write(i2c, 0x04, 0xff); //clear0
			lt9611_write(i2c, 0x04, 0xfe); //clear0
			lt9611_write(i2c, 0x00, 0xfe); //mask0 vid_chk_IRQ
		}

		if (!enable) {
			lt9611_write(i2c, 0xff, 0x82);
			lt9611_write(i2c, 0x04, 0xff); //clear0
			lt9611_write(i2c, 0x00, 0xff); //mask0 vid_chk_IRQ
		}
		break;
	case CEC_INTERRUPT_ENABLE:
		if (enable) {
			lt9611_write(i2c, 0xff, 0x86);
			lt9611_write(i2c, 0xfa, 0x00); //cec interrup mask
			lt9611_write(i2c, 0xfc, 0x7f); //cec irq clr
			lt9611_write(i2c, 0xfc, 0x00);

			/* cec irq init */
			lt9611_write(i2c, 0xff, 0x82);
			lt9611_write(i2c, 0x01, 0x7f); //mask bit[7]
			lt9611_write(i2c, 0x05, 0xff); //clr bit[7]
			lt9611_write(i2c, 0x05, 0x7f);
		}

		if (!enable) {
			lt9611_write(i2c, 0xff, 0x86);
			lt9611_write(i2c, 0xfa, 0xff); //cec interrup mask
			lt9611_write(i2c, 0xfc, 0x7f); //cec irq clr

			/* cec irq init */
			lt9611_write(i2c, 0xff, 0x82);
			lt9611_write(i2c, 0x01, 0xff); //mask bit[7]
			lt9611_write(i2c, 0x05, 0xff); //clr bit[7]
		}
		break;
		default: break;
	}
}

static int lt9611_hpd_inter_handle(struct i2c_client *i2c)
{
	int status = lt9611_get_hpd_status(i2c_dev);

	lt9611_write(i2c, 0xff, 0x82);
	lt9611_write(i2c, 0x07, 0xff);    // clear3
	lt9611_write(i2c, 0x07, 0x3f);    // clear3

	if (status) {
		printk(KERN_DEBUG  "lt9611: hdmi connected");
		lt9611_lowpower_mode(i2c, 0);
		lt9611_set_interrupt(i2c, VID_INTERRUPT_ENABLE, 1);
		// mdelay(10);
		// dump_edid(lt9611_read_edid(i2c_dev));

		lt9611_check_video_timing(i2c);
		if (time_type != video_none) {
			lt9611_set_pll(i2c, video);
			lt9611_mipi_pcr(i2c, video);
			lt9611_hdmi_tx(i2c, video);
			lt9611_hdmi_out_enable(i2c);
		} else {
			lt9611_hdmi_out_disable(i2c);
			printk(KERN_DEBUG  "no mipi video, disable hdmi output\n");
		}
	} else {
		printk(KERN_DEBUG  "lt9611: hdmi disconnected\n");
		lt9611_set_interrupt(i2c, VID_INTERRUPT_ENABLE, 0);
		lt9611_lowpower_mode(i2c, 1);
	}
	return 0;
}

static int lt9611_init(struct i2c_client *i2c)
{
	int ret;

	ret = lt9611_system_init(i2c_dev);
	if (ret)
		return -1;

	ret = lt9611_check_video_timing(i2c);
	if (ret)
		return -1;

	lt9611_set_pll(i2c, video);
	lt9611_mipi_pcr(i2c, video);

	lt9611_audio_init(i2c);

	lt9611_hdcp_init(i2c);
	lt9611_hdmi_tx(i2c, video);
	lt9611_hdmi_tx_phy(i2c);

	lt9611_init_irq(i2c);;

	lt9611_set_interrupt(i2c, HPD_INTERRUPT_ENABLE, 1);
	lt9611_set_interrupt(i2c, VID_INTERRUPT_ENABLE, 0);
	lt9611_set_interrupt(i2c, CEC_INTERRUPT_ENABLE, 1);

	lt9611_freq_meter(i2c);

	mdelay(10); /* HPD have debounce, wait HPD irq. */
	lt9611_hpd_inter_handle(i2c);

	return 0;
}

static inline struct gpio_desc *m_request_gpio(struct device *dev, const char *str)
{
	struct gpio_desc *gpiod;

	gpiod = devm_gpiod_get_optional(dev, str, GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(gpiod)) {
		printk(KERN_ERR "failed to request %s (gpio%ld)\n", str, PTR_ERR(gpiod));
		return NULL;
	}

	return gpiod;
}

static inline void m_gpio_direction_output(struct gpio_desc *gpiod, int value)
{
	if (gpiod)
		gpiod_direction_output(gpiod, value);
}

static int init_gpio(void)
{
	/* maybe do not have power pin */
	gpio_power_en = m_request_gpio(&i2c_dev->dev, "lt9611-power-en");

	gpio_rst = m_request_gpio(&i2c_dev->dev, "lt9611-rst");
	if (!gpio_rst)
		return -1;

	return 0;
}

static void lt9611_power_off(void)
{
	m_gpio_direction_output(gpio_power_en, 0);

	m_gpio_direction_output(gpio_rst, 0);
}

static int lt9611_power_on(void)
{
	int ret;

	m_gpio_direction_output(gpio_power_en, 0);
	mdelay(5);
	m_gpio_direction_output(gpio_power_en, 1);
	mdelay(5);

	m_gpio_direction_output(gpio_rst, 0);
	mdelay(5);
	m_gpio_direction_output(gpio_rst, 1);
	mdelay(5);

	ret = lt9611_detect(i2c_dev);
	if (ret)
		printk(KERN_ERR "lt9611: failed to detect\n");

	return ret;
}

static struct bridge_attr lt9611_bridge_attr = {
	.device_name          = LT9611_DEVICE_NAME,
	.cbus_addr            = LT9611_DEVICE_I2C_ADDR,
	.mipi = {
		.data_fmt         = MIPI_RGB888,
		.tx_mode          = tx_Burst,
		.lanes            = 4,
		.clk_settle_time  = 120,   /* ns */
		.data_settle_time = 130,   /* ns */
	},
	.hdmi = {
		.couple           = mode_AC,
		.port             = port_HDMI,
		.en_hdcp          = 0,
		.audio = {
			.port         = port_IIS,
		},
	},

	.ops = {
		.power_on         = lt9611_power_on,
		.power_off        = lt9611_power_off,
	},
};

static int lt9611_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;

	i2c_dev = client;

	ret = init_gpio();
	if (ret)
		return ret;

	ret = lt9611_power_on();
	if (ret)
		goto err_power_on;

	mdelay(2000);  /* wait mipi host init ok, need to optimize */

	ret = lt9611_init(client);
	if (ret)
		goto err_power_on;

	return 0;

err_power_on:
	lt9611_power_off();
	return ret;
}

static int lt9611_remove(struct i2c_client *client)
{
	lt9611_power_off();

	return 0;
}

static const struct of_device_id lt9611_dt_ids[] = {
	{.compatible = "semidrive,lt9611",},
	{ }
};

static struct i2c_driver lt9611_driver = {
	.probe              = lt9611_probe,
	.remove             = lt9611_remove,
	.driver = {
		.name           = "lt9611",
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(lt9611_dt_ids),
	},
};

module_i2c_driver(lt9611_driver);

MODULE_DESCRIPTION("LT9611 Driver");
MODULE_LICENSE("GPL");
