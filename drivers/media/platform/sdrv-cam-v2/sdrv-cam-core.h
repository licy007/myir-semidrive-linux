/*
 * sdrv-cam-core.h
 *
 * Semidrive platform csi header file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __SDRV_CAM_MAIN_H__
#define __SDRV_CAM_MAIN_H__

#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <linux/device.h>

#include "csi-controller-ip.h"


#define SDRV_IMG_NUM	4

#define SDRV_IMG_PAD_SINK		0
#define SDRV_IMG_PAD_SRC		1
#define SDRV_IMG_PAD_NUM		2

#define SDRV_IMG_NAME "sdrv-kstream"

#define SDRV_IMG_X_MAX		4096
#define SDRV_IMG_Y_MAX		4096

#define IMG_MAX_NUM_PLANES		3
#define IMG_HW_NUM_PLANES		(IMG_MAX_NUM_PLANES - 1)

#define SDRV_MBUS_DC2CSI1		0x101
#define SDRV_MBUS_DC2CSI2		0x102

#define BT_MIPI		0
#define BT_PARA		1

#define CAM_SUBDEV_GRP_ID_SENSOR 0
#define CAM_SUBDEV_GRP_ID_MIPICSI_PARALLEL 1

struct csi_core;
struct kstream_device;

struct csi_int_stat {
	u64 frm_cnt;
	u32 bt_err;
	u32 bt_fatal;
	u32 bt_cof;
	u32 crop_err;
	u32 pixel_err;
	u32 overflow;
	u32 bus_err;
};


struct csi_core {
	struct device *dev;
	struct mutex lock;

	/* v4l2 info */
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct v4l2_async_notifier notifier;
	struct v4l2_fwnode_endpoint vep;
	char type[16]; /* EVS,DMS,DVR,FRONT,BACK */

	unsigned int  mbus_type;

	struct v4l2_format active_fmt;
	struct video_device vdev;

	/* stream info */
	struct list_head kstreams;
	unsigned int kstream_nums;
	unsigned int mipi_stream_num;
	struct kstream_device* kstream_dev[SDRV_IMG_NUM];
	atomic_t active_stream_num; /* active stream number, reference count*/

	/* csi ip info */
	void *csi_hw;
	void __iomem *base;
	u32 irq;
	u32 host_id;	/* host ip id */
	u32 bt;		/* bus type */
	u32 sync;
	u32 skip_frame; //the frame count that software needs to skip
	u32 img_sync;
	u32 img_cnt;
	struct csi_int_stat ints;
	atomic_t ref_count;
	struct dentry *p_dentry;
};

#endif
