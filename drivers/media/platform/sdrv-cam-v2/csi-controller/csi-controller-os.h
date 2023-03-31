/*
 * csi-controller-os.h
 *
 * Semidrive platform mipi csi2 header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __SDRV_CSI_CONTROLLER_OS_H__
#define __SDRV_CSI_CONTROLLER_OS_H__

#include "sdrv-cam-video.h"
#include "sdrv-cam-core.h"

enum kstream_state {
	STOPPED = 0,
	INITING,
	INITIALED,
	RUNNING,
	IDLE,
	STOPPING,
};

struct kstream_sensor {
	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
	unsigned int source_pad;
};

struct kstream_interface {
	unsigned int mbus_type;
	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
	unsigned int sink_pad;
	unsigned int source_pad;
};

struct kstream_device {
	u8 id;
	struct device *dev;
	struct list_head csi_entry;
	struct csi_core *core;
	struct v4l2_subdev subdev;
	struct media_pad pads[SDRV_IMG_PAD_NUM];

	struct v4l2_mbus_framefmt mbus_fmt[SDRV_IMG_PAD_NUM];
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_rect crop;

	struct sdrv_cam_video video;
	struct kstream_interface interface;
	struct kstream_sensor sensor;
	enum kstream_state state;
	bool enabled;
	bool iommu_enable;
	int field;

	u32 *support_formats;
	int support_formats_num;

	u32 sd_hcrop_b;
	u32 sd_hcrop_f;

	uint32_t hcrop_back;
	uint32_t hcrop_front;
	uint32_t hcrop_top_back;
	uint32_t hcrop_top_front;
	uint32_t hcrop_bottom_back;
	uint32_t hcrop_bottom_front;

	u64 last_timestamp;
	u64 frame_interval;
	u32 frame_cnt;
	u32 frame_loss_cnt;

};

struct kstream_pix_format {
	u32 pixfmt; /*pixfmt pixel format,  little endian four character code. */
	u32 mbus_code; /* media bus code */
	u8 planes; /* plane num */
	u8 bpp[3];
	u8 pack_uv_odd;
	u8 pack_uv_even;
	u8 pack_pix_odd;
	u8 pack_pix_even;
	u32 split[2];
	u32 pack[2];
	u32 bt;
};

const struct kstream_pix_format *
kstream_get_kpfmt_by_mbus_code(unsigned int mbus_code);

const struct kstream_pix_format *
kstream_get_kpfmt_by_index(unsigned int index);

const int kstream_get_kpfmt_count(void);

const int kstream_init_format_by_mbus_code(struct kstream_device *kstream, unsigned int mbus_code);

const u32 *kstream_get_support_formats_by_index(struct kstream_device *kstream, unsigned int index);

const struct kstream_pix_format *
kstream_get_kpfmt_by_fmt(unsigned int pixelformat);

int sdrv_stream_register_entities(struct kstream_device *kstream,
				  struct v4l2_device *v4l2_dev);

int sdrv_stream_unregister_entities(struct kstream_device *kstream);

#endif /* __SDRV_CSI_CONTROLLER_OS_H__ */
