/*
 * max9295.h
 *
 * Semidrive platform csi header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef SDRV_SERDES_H
#define SDRV_SERDES_H

#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define MAX_SENSOR_NUM  4
#define MIPI_CSI2_SENS_VC0_PAD_SOURCE   0
#define MIPI_CSI2_SENS_VC1_PAD_SOURCE   1
#define MIPI_CSI2_SENS_VC2_PAD_SOURCE   2
#define MIPI_CSI2_SENS_VC3_PAD_SOURCE   3
#define MIPI_CSI2_SENS_VCX_PADS_NUM     4

#define PROBE_INIT

enum serdes_status {
	SERDES_IDLE = 0,
	SERDES_PROBED,
	SERDES_DONE,
};

typedef struct __serdes_param serdes_para_t;


struct serdes_init_status {
	int   init_status;
	int   stream_cnt;
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
} serdes_param_reg16_t;

/**
*  \brief Register address and value pair, with delay.
*/
typedef struct {
	u8 nRegAddr;
	/**< Register Address */
	u8 nRegValue;
	/**< data */
	unsigned int nDelay;
	/**< Delay to be applied, after the register is programmed */
} serdes_param_reg8_t;

struct serdes_dev_t {
	struct i2c_client *i2c_client;
	const char *module_name;
	int width[MAX_SENSOR_NUM];//for cameras that have different size within one serdesializer
	int height[MAX_SENSOR_NUM];
	int remap_addr_ser[MAX_SENSOR_NUM];
	unsigned int addr_des;
	unsigned int addr_poc;
	unsigned int addr_ser;
	struct v4l2_subdev sd[MAX_SENSOR_NUM];
	struct media_pad pads[MIPI_CSI2_SENS_VCX_PADS_NUM];

	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *poc_gpio;
	struct gpio_desc *gpi_gpio;

	struct mutex lock;
	struct mutex lock_hotplug;
	struct delayed_work init_work;//init is done as a delayed work to accelarate the open speed
	struct delayed_work hotplug_work;

	int current_channel_lock_status;
	int last_channel_lock_status;

	int ser_bitmap; //the ser bit map uses 4 bits. If all the port is connected,the bit map should be 0xf.It is the serializer that the dts supported
	int ser_online_bitmap;//the ser online bit map is the actual online serializer map
	int serdes_streamon_bitmap; //indicate which channel is stream on

	struct serdes_init_status serdes_status;
	struct v4l2_mbus_framefmt fmt;

	struct v4l2_fract frame_interval;
	u32 sync;

	serdes_para_t *priv;
};

struct __serdes_param {
	const char *module_name;
	u32 mbus_code;
	u32 colorspace;
	u32 fps;

	u32 field;
	u32 numerator;
	u16	quantization;

	u32 delay_time; //need to define this variable because the delay work may cause serializer address conflict(0x40)

	int  (*serdes_init)(struct serdes_dev_t *dev);
	void (*serdes_start)(struct serdes_dev_t *dev, bool on);
	void (*serdes_power)(struct serdes_dev_t *dev,
						 bool on); //power on serdesializer
	int  (*serdes_check_link_status)(struct serdes_dev_t *serdes);
	int  (*serdes_check_link_status_ex)(struct serdes_dev_t
								 *serdes, int channel);//check link lock for specific channel
	bool  (*serdes_check_video_lock_status)(struct serdes_dev_t *serdes);
	int  (*serdes_enum_mbus_code)(struct v4l2_subdev *sd,
								 struct v4l2_subdev_pad_config *cfg,
								 struct v4l2_subdev_mbus_code_enum *code);
};

int serdes_parse_dt(struct serdes_dev_t *serdes,
				   struct i2c_client *client);

int serdes_subdev_init(struct serdes_dev_t *serdes);
int serdes_remove(struct i2c_client *client);
int serdes_set_id(struct v4l2_subdev *sd, int id);

int serdes_read_reg_16(struct i2c_client *client,
					   u16 reg,
					   u8 *val);
int serdes_write_reg_16(struct i2c_client *client,
						u16 reg,
						u8 val);
int serdes_read_reg_8(struct i2c_client *client,
					  u8 reg,
					  u8 *val);
int serdes_write_reg_8(struct i2c_client *client,
					   u8 reg,
					   u8 val);



#endif
