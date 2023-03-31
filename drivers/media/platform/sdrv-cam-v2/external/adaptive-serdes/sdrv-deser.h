/*
 * sdrv-cam-core.h
 *
 * Semidrive platform csi header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef SDRV_DESER_H
#define SDRV_DESER_H

#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <linux/device.h>

#define MIPI_CSI2_SENS_VCX_PADS_NUM     4

//If you don't want necessary requirement, DON'T CHANGE THESE NAMES.
#define SDRV_DESER_NAME  "sdrv,mcsi0_deser"
#define SDRV2_DESER_NAME "sdrv,mcsi1_deser"
#define SDRV3_DESER_NAME "sdrv,dvp_deser"

#define SDRV_DESER_NAME_I2C  "mcsi0_deser"
#define SDRV2_DESER_NAME_I2C "mcsi1_deser"
#define SDRV3_DESER_NAME_I2C "dvp_deser"

#if defined(CONFIG_ARCH_SEMIDRIVE_V9)
#define SDRV_DESER_NAME_B  "sdrv,mcsi0_deser_sideb"
#define SDRV2_DESER_NAME_B "sdrv,mcsi1_deser_sideb"
#define SDRV3_DESER_NAME_B "sdrv,dvp_deser_sideb"
#define SDRV_DESER_NAME_I2C_B  "mcsi0_deser_sideb"
#define SDRV2_DESER_NAME_I2C_B "mcsi1_deser_sideb"
#define SDRV3_DESER_NAME_I2C_B "dvp_deser_sideb"
#endif


typedef struct __deser_param deser_para_t;


typedef struct __sdrv_deser_dev {
	struct i2c_client *i2c_client;
	unsigned short addr_deser;
	unsigned short addr_serer;
	unsigned short addr_poc;
	unsigned short addr_isp;
	unsigned short addr_gpioext;

	struct v4l2_subdev sd;
	struct media_pad pads[MIPI_CSI2_SENS_VCX_PADS_NUM];
	struct v4l2_fwnode_endpoint ep;	/* the parsed DT endpoint info */

	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *poc_gpio;
	struct gpio_desc *gpi_gpio;
	struct gpio_desc *pmu_gpio;

	bool upside_down;
	uint32_t link_count;
	struct mutex serer_lock;
	/* lock to protect all members below */
	struct mutex lock;

	int power_count;

	struct v4l2_mbus_framefmt fmt;
	bool pending_fmt_change;

	struct v4l2_fract frame_interval;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
	int sec_9286_shift;
	u32 sync;
	u32 device_type;
	u32 type;
	deser_para_t *priv;
} deser_dev_t;

/*
 * Deser rx chip dev structure
 */
struct __deser_param {
	char *name;

	u16 addr_deser;
	u16 addr_serer;
	u16 addr_poc;
	u16 addr_isp;
	u16 addr_gpioext;

	u32 pclk;
	u32 mipi_bps;
	u32 width;
	u32 height;
	u32 htot;
	u32 vtot;

	u32 mbus_code;
	u32 colorspace;
	u32 fps;

	u16	ycbcr_enc;
	u16	quantization;
	u32 field;
	u32 numerator;
	u32 denominator;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;

	int used;  //Can't use the detected deser variable.

	int (*power_deser) (deser_dev_t *dev, bool on);
	int (*power_module) (deser_dev_t *dev, bool on);
	int (*detect_deser) (deser_dev_t *dev);			//Must implement this interface
	int (*init_deser) (deser_dev_t *dev);			//Must implement this interface
	int (*start_deser) (deser_dev_t *dev, bool on); //Must implement this interface
	void(*dump_deser)(deser_dev_t *dev);
	int (*deser_enum_mbus_code)(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_mbus_code_enum *code);
};


/**
*  \brief Register address and value pair, with delay.
*/
typedef struct{
	u16 nRegAddr;
	/**< Register Address */
	u8 nRegValue;
	/**< data */
	unsigned int nDelay;
	/**< Delay to be applied, after the register is programmed */
} reg_param_t;


/**
*	deser state machine.
*	Add this feature for decreasing deser scan time and decrasing system boot time.
*/
enum DESER_MACHINE {
	DESER_NOT_USED = 0,		/** don't use this deser at one csi interface when not detected*/
	DESER_USE_ONCE = 1,		/** use one deser at one csi interface detected*/
	DESER_USE_TWICE = 2,		/** use one deser at one csi interface detected*/
};


enum support_device {
	SDRV_SD = 0x0,	//default cameras
	SDRV1_ICL02 = 0x0101,	//csi1 cameras
	SDRV3_ICL02 = 0x0301,	//csi3 cameras
	SDRV3_MINIEYE,
};


#endif
