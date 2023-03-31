/*
 * sdrv-cam-video.h
 *
 * Semidrive camera video header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __SDRV_CAM_VIDEO_H__
#define __SDRV_CAM_VIDEO_H__

#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <linux/device.h>

struct sdrv_cam_buffer {
	struct vb2_v4l2_buffer vbuf;
	dma_addr_t paddr[3];
	struct list_head queue;
};

struct sdrv_cam_queue {
	int qlen;
	struct list_head qhead;
};

struct sdrv_cam_video {
	struct device *dev;
	struct csi_core *core;
	struct vb2_queue queue;
	struct video_device vdev;
	struct media_pad pad;
	struct v4l2_format active_fmt;
	struct list_head buf_list;
	unsigned int sequence;
	u8 id;	/* stream id */
	struct mutex lock;
	struct mutex q_lock;
	spinlock_t buf_lock;

	struct vb2_v4l2_buffer *vbuf_ready; /* reg set */
	struct sdrv_cam_queue active_q;  /* data writing */

	struct vb2_mem_ops mem_ops;
	atomic_t at_user;

	int (*hotplug_callback)(struct sdrv_cam_video *video);
};


int sdrv_cam_video_register(struct sdrv_cam_video *video,
		struct v4l2_device *v4l2_dev);

void sdrv_cam_video_unregister(struct sdrv_cam_video *video);


#endif /* __SDRV_CAM_VIDEO_H__ */
