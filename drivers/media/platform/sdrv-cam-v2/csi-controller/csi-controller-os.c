/*
 * csi-controller-os.c
 *
 * Semidrive platform kstream subdev operation
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/clk.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>

#include "sdrv-cam-os-def.h"
#include "sdrv-cam-util.h"
#include "sdrv-cam-video.h"
#include "sdrv-cam-core.h"
#include "csi-controller-os.h"
#include "cam_isr_log.h"

/*
 * set 1 if frame done irq mask
 */
#define MAX_ACTIVE_BUF 1

struct kstream_mbus_format {
	u32 code;
	u8 pix_odd;
	u8 pix_even;
	bool fc_dual;
};

static const struct kstream_mbus_format mbus_fmts[] = {
	{
		.code = MEDIA_BUS_FMT_YUYV8_1_5X8, /* YUV420 YYUYYV... */
		.pix_odd = 0x01,
		.pix_even = 0x01,
		.fc_dual = true,
	},
	{
		.code = MEDIA_BUS_FMT_UYVY8_2X8, /* YUV422 UYVY */
		.pix_odd = 0x01,
		.pix_even = 0x01,
		.fc_dual = false,
	},
	{
		.code = MEDIA_BUS_FMT_YUYV8_2X8, /* YUV422 YUYV */
		.pix_odd = 0x01,
		.pix_even = 0x01,
		.fc_dual = false,
	},
	{
		.code = MEDIA_BUS_FMT_RGB565_2X8_BE, /* RGB565 BE */
		.pix_odd = 0x01,
		.pix_even = 0x01,
		.fc_dual = false,
	},
	{
		.code = MEDIA_BUS_FMT_RGB888_1X24, /* RGB888 */
		.pix_odd = 0x00,
		.pix_even = 0x00,
		.fc_dual = false,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR8_1X8, /* RAW8 */
		.pix_odd = 0x03,
		.pix_even = 0x03,
		.fc_dual = false,
	},
	{
		.code = MEDIA_BUS_FMT_Y8_1X8, /* Grep */
		.pix_odd = 0x03,
		.pix_even = 0x03,
		.fc_dual = false,
	},
};

const struct kstream_pix_format pix_fmts[] = {

	{
		.pixfmt = V4L2_PIX_FMT_YUV420, /* YUV420 3 planes*/
		.mbus_code = MEDIA_BUS_FMT_YUYV8_1_5X8,
		.planes = 1,
		.bpp = {8, 4, 4},
		.pack_uv_odd = 0x01,
		.pack_uv_even = 0x10,
		.pack_pix_odd = 0x02,
		.pack_pix_even = 0x02,
		.split = {0x0FD3, 0x39},
		.pack = {0x108, 0x08},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_YUYV, /* YUYV */
		.mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x21a, 0x3F},
		.pack = {0x108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_YUYV, /* YUYV */
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x13, 0x3F},
		.pack = {0x108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_UYVY, /* UYVY */
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x21a, 0x3F},
		.pack = {0x108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_UYVY, /* UYVY */
		.mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x13, 0x3F},
		.pack = {0x108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_RGB565, /* RGB565 */
		.mbus_code = MEDIA_BUS_FMT_RGB565_2X8_BE,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_RGB24, /* RGB24 */
		.mbus_code = MEDIA_BUS_FMT_RGB888_1X24,
		.planes = 1,
		.bpp = {24},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x02,
		.pack_pix_even = 0x02,
		.split = {0x0E53, 0x3F},
		.pack = {0x2108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_SBGGR8, /* RAW8 */
		.mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.planes = 1,
		.bpp = {8},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_GREY, /* Grep */
		.mbus_code = MEDIA_BUS_FMT_Y8_1X8,
		.planes = 1,
		.bpp = {8},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 0,
	},
	{
		.pixfmt = V4L2_PIX_FMT_YUV420, /* YUV420 3 planes*/
		.mbus_code = MEDIA_BUS_FMT_YUYV8_1_5X8,
		.planes = 1,
		.bpp = {8, 4, 4},
		.pack_uv_odd = 0x01,
		.pack_uv_even = 0x10,
		.pack_pix_odd = 0x02,
		.pack_pix_even = 0x02,
		.split = {0x0FD3, 0x39},
		.pack = {0x108, 0x08},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_YUYV, /* YUYV */
		.mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_YUYV, /* YUYV */
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x21a, 0x3F},
		.pack = {0x42108, 0},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_UYVY, /* UYVY */
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_UYVY, /* UYVY */
		.mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x21a, 0x3F},
		.pack = {0x42108, 0},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_RGB565, /* RGB565 */
		.mbus_code = MEDIA_BUS_FMT_RGB565_2X8_BE,
		.planes = 1,
		.bpp = {16},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_RGB24, /* RGB24 */
		.mbus_code = MEDIA_BUS_FMT_RGB888_1X24,
		.planes = 1,
		.bpp = {24},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x02,
		.pack_pix_even = 0x02,
		.split = {0x0E53, 0x3F},
		.pack = {0x2108, 0},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_SBGGR8, /* RAW8 */
		.mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.planes = 1,
		.bpp = {8},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 1,
	},
	{
		.pixfmt = V4L2_PIX_FMT_GREY, /* Grep */
		.mbus_code = MEDIA_BUS_FMT_Y8_1X8,
		.planes = 1,
		.bpp = {8},
		.pack_uv_odd = 0x03,
		.pack_uv_even = 0x03,
		.pack_pix_odd = 0x03,
		.pack_pix_even = 0x03,
		.split = {0x53, 0x3F},
		.pack = {0x42108, 0},
		.bt = 1,
	},

};

static uint32_t to_v4l2_pixfmt(uint32_t csi_pix_fmt)
{
	switch (csi_pix_fmt) {
	case CSI_FMT_YUYV:
		return V4L2_PIX_FMT_YUYV;

	case CSI_FMT_UYVY:
		return V4L2_PIX_FMT_UYVY;

	case CSI_FMT_YUV422SP:
		return V4L2_PIX_FMT_NV16;

	case CSI_FMT_RGB24:
		return V4L2_PIX_FMT_RGB24;

	case CSI_FMT_BGR24:
		return V4L2_PIX_FMT_BGR24;

	default:
		return V4L2_PIX_FMT_UYVY;
	}

	return V4L2_PIX_FMT_UYVY;
}

static uint32_t to_csi_pixfmt(uint32_t v4l2_pix_fmt)
{
	switch (v4l2_pix_fmt) {
	case V4L2_PIX_FMT_YUYV:
		return CSI_FMT_YUYV;

	case V4L2_PIX_FMT_UYVY:
		return CSI_FMT_UYVY;

	case V4L2_PIX_FMT_NV16:
		return CSI_FMT_YUV422SP;

	case V4L2_PIX_FMT_RGB24:
		return CSI_FMT_RGB24;

	case V4L2_PIX_FMT_BGR24:
		return CSI_FMT_BGR24;

	default:
		return CSI_FMT_UYVY;
	}

	return CSI_FMT_UYVY;
}


const struct kstream_pix_format *kstream_get_kpfmt_by_mbus_code(unsigned int
								mbus_code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pix_fmts); i++)
		if (mbus_code == pix_fmts[i].mbus_code)
			break;

	if (i >= ARRAY_SIZE(pix_fmts))
		i = 2;		/* default UYVY */

	return &pix_fmts[i];
}

const struct kstream_pix_format *kstream_get_kpfmt_by_fmt(unsigned int
							  pixelformat)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pix_fmts); i++)
		if (pixelformat == pix_fmts[i].pixfmt)
			break;

	if (i >= ARRAY_SIZE(pix_fmts))
		i = 2;		/* default UYVY */

	return &pix_fmts[i];
}

const struct kstream_pix_format *kstream_get_kpfmt_by_index(unsigned int index)
{
	if (index >= ARRAY_SIZE(pix_fmts))
		return NULL;

	return &pix_fmts[index];
}

const int kstream_get_kpfmt_count(void)
{
	return ARRAY_SIZE(pix_fmts);
}

const u32 *kstream_get_support_formats_by_index(struct kstream_device *kstream, unsigned int index)
{
	if (index >= kstream->support_formats_num)
		return NULL;
	return kstream->support_formats+index;
}

static int kstream_update_support_formats(struct kstream_device *kstream, u32 pixfmt)
{
	int i;

	for(i=0; i<kstream->support_formats_num; i++){
		if (*(kstream->support_formats +i) == pixfmt)
			return 0;
	}

	kstream->support_formats_num++;
	kstream->support_formats = krealloc(kstream->support_formats, sizeof(kstream->support_formats[0])*kstream->support_formats_num, GFP_KERNEL);

	if (!kstream->support_formats)
		dev_err(kstream->dev, "%s: realloc error\n", __func__);

	*(kstream->support_formats + i) = pixfmt;

	return 0;
}

const int kstream_init_format_by_mbus_code(struct kstream_device *kstream, unsigned int mbus_code)
{
	unsigned int i;

	int ret;
	struct csi_bus_info bus_info;
	struct csi_outfmt_info res;

	if (kstream->core->mbus_type == V4L2_MBUS_CSI2)
		bus_info.bus_type = CSI_BUS_MIPICSI2;
	else if (kstream->core->mbus_type == V4L2_MBUS_PARALLEL)
		bus_info.bus_type = CSI_BUS_PARALLEL2;
	else if (kstream->core->mbus_type == V4L2_MBUS_BT656)
		bus_info.bus_type = CSI_BUS_BT656;
	else
		pr_err("error: unknown mbus_type %d\n", kstream->core->mbus_type);

	if (mbus_code == MEDIA_BUS_FMT_UYVY8_2X8)
		bus_info.mbus_fmt = CSI_MBUS_UYVY8_2X8;
	else if (mbus_code == MEDIA_BUS_FMT_YUYV8_2X8)
		bus_info.mbus_fmt = CSI_MBUS_YUYV8_2X8;
	else if (mbus_code == MEDIA_BUS_FMT_RGB888_1X24)
		bus_info.mbus_fmt = CSI_MBUS_RGB888_1X24;
	else
		pr_err("error: unknown mbus_fmt %d\n", mbus_code);

	bus_info.bus_flag = 0;
	ret = csi_cfg_bus(kstream->core->csi_hw, kstream->id, &bus_info);
	ret |= csi_query_outfmts(kstream->core->csi_hw, kstream->id, &res);
	if (ret == 0 && res.count > 0) {
		for (i = 0; i < res.count; i++) {
			u32 pixfmt;
			pixfmt = to_v4l2_pixfmt(res.fmts[i]);
			kstream_update_support_formats(kstream, pixfmt);
		}
	}

	return ret;

}

static inline int kstream_get_pix_fmt(struct kstream_device *kstream,
				      struct v4l2_pix_format_mplane **pix_fmt)
{
	struct v4l2_pix_format_mplane *fmt;
	int i;
	struct v4l2_mbus_framefmt *in_fmt, *out_fmt;

	in_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	out_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SRC];

	fmt = &kstream->video.active_fmt.fmt.pix_mp;

	for (i = 0; i < ARRAY_SIZE(pix_fmts); i++)
		if ((fmt->pixelformat == pix_fmts[i].pixfmt) && (in_fmt->code == pix_fmts[i].mbus_code) && (kstream->core->bt == pix_fmts[i].bt))
			break;

	if (i >= ARRAY_SIZE(pix_fmts))
		return -EINVAL;

	if (fmt->num_planes > IMG_MAX_NUM_PLANES)
		return -EINVAL;

	*pix_fmt = fmt;

	return i;
}

static void kstream_flush_all_buffers(struct kstream_device *kstream,
				      enum vb2_buffer_state state)
{
	struct sdrv_cam_video *video = &kstream->video;
	struct sdrv_cam_buffer *kbuf, *node;
	struct vb2_v4l2_buffer *vbuf;
	unsigned long flags;

	spin_lock_irqsave(&video->buf_lock, flags);
	vbuf = video->vbuf_ready;

	if (vbuf) {
		video->vbuf_ready = NULL;
		kbuf = container_of(vbuf, struct sdrv_cam_buffer, vbuf);
		list_add_tail(&kbuf->queue, &video->buf_list);
	}

	while (!list_empty(&video->active_q.qhead)) {
		kbuf = list_first_entry(&video->active_q.qhead, struct sdrv_cam_buffer, queue);
		list_del(&kbuf->queue);
		video->active_q.qlen--;
		list_add_tail(&kbuf->queue, &video->buf_list);
	}

	spin_unlock_irqrestore(&video->buf_lock, flags);

	list_for_each_entry_safe(kbuf, node, &video->buf_list, queue) {
		vb2_buffer_done(&kbuf->vbuf.vb2_buf, state);
		list_del(&kbuf->queue);
	}
}

static int kstream_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	if (flags & MEDIA_LNK_FL_ENABLED)
		if (media_entity_remote_pad(local))
			return -EBUSY;

	return 0;
}

static int kstream_set_power(struct v4l2_subdev *sd, int on)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);

	int ret = 0;

	pr_info("cam:%s: stream %d on %d\n", __func__, kstream->id, on);

	if (on)
		ret = csi_reset_img(kstream->core->csi_hw, (uint32_t)kstream->id, CSI_RESET_IMG_HW);
	return ret;

}

static int kstream_frame_done(void *caller, uint32_t img_id)
{
	int ret = 0;
	struct kstream_device *kstream = (struct kstream_device *)caller;
	struct sdrv_cam_video *video = &kstream->video;
	struct vb2_v4l2_buffer *vbuf;
	struct sdrv_cam_buffer *kbuf = NULL;
	struct csi_core *csi = kstream->core;
	unsigned long flags;

	if (kstream->state == STOPPED)
		return 0;

	if (list_empty(&video->active_q.qhead)) {
		if (kstream->state == RUNNING)
			cam_isr_log_err("stream%d active_q empty\n", kstream->id);
		return 0;
	}

	if (video->active_q.qlen > MAX_ACTIVE_BUF) {
		cam_isr_log_debug("stream%d done. active_q full\n", kstream->id);
	}

	spin_lock_irqsave(&video->buf_lock, flags);

	kbuf = list_first_entry(&video->active_q.qhead, struct sdrv_cam_buffer, queue);
	list_del(&kbuf->queue);
	video->active_q.qlen--;

	vbuf = &kbuf->vbuf;
	vbuf->vb2_buf.timestamp = ktime_get_ns();

	kstream->frame_interval = vbuf->vb2_buf.timestamp - kstream->last_timestamp;
	kstream->last_timestamp = vbuf->vb2_buf.timestamp;
	kstream->frame_cnt++;

	//skip the first frame  buffers by not sending the buffer to the app and send back to the buffer list directly
	if (video->sequence < csi->skip_frame) {
		video->sequence++;
		list_add_tail(&kbuf->queue, &video->buf_list);
		spin_unlock_irqrestore(&video->buf_lock, flags);
		return 0;
	}

	vbuf->sequence = video->sequence++;

#if (IS_ENABLED(CONFIG_ARM64))
	/*
	 * To speed up CPU access in V4L2_MEMORY_MMAP + dma-contig case, we
	 * declare outself to be a DMA-coherent device. Assure cache line
	 * consistency before returning buffer to video core.
	 */
	if (is_device_dma_coherent(kstream->dev)) {
		int i;

		/* mark incoherent to let arch do right cache ops */
		kstream->dev->archdata.dma_coherent = false;

		for (i = 0; i < vbuf->vb2_buf.num_planes; i++) {
			dma_sync_single_for_cpu(kstream->dev,
					kbuf->paddr[i],
					vbuf->vb2_buf.planes[i].length,
					DMA_FROM_DEVICE);
		}

		/* restore to map page cacheable */
		kstream->dev->archdata.dma_coherent = true;
	}
#endif

	vb2_buffer_done(&vbuf->vb2_buf, VB2_BUF_STATE_DONE);
	spin_unlock_irqrestore(&video->buf_lock, flags);

	camera_heartbeat_inc();
	cam_isr_log_trace("frm_done: csi%d img %d, pop vbuf %p, %x, %x, %x; ts 0x%llx; %x %x %x %x\n",
		kstream->core->host_id, img_id, vbuf, vbuf->flags, vbuf->field, vbuf->sequence,
		vbuf->vb2_buf.timestamp, vbuf->vb2_buf.index, vbuf->vb2_buf.type,
		vbuf->vb2_buf.memory, vbuf->vb2_buf.num_planes);

	return ret;
}

static int kstream_update_buf(void *caller, uint32_t img_id)
{
	int ret = 0;
	struct csi_img_buf buf;
	struct kstream_device *kstream = (struct kstream_device *)caller;
	struct sdrv_cam_video *video = &kstream->video;
	struct sdrv_cam_buffer *kbuf;
	unsigned long flags;

	if (kstream->state == STOPPED)
		return 0;
	spin_lock_irqsave(&video->buf_lock, flags);
	if (list_empty(&video->buf_list)) {
		spin_unlock_irqrestore(&video->buf_lock, flags);
		kstream->frame_loss_cnt++;
		if (kstream->state == RUNNING) {
			cam_isr_log_err("stream%d, vbuf_list is empty\n", kstream->id);
			kstream->state = IDLE;
		}
		return 0;
	}
	kstream->state = RUNNING;

	if (video->active_q.qlen >= MAX_ACTIVE_BUF) {
		spin_unlock_irqrestore(&video->buf_lock, flags);
		cam_isr_log_warn("stream%d, active_q has set\n", kstream->id);
		return 0;
	}

	if (video->vbuf_ready) {
		kbuf = container_of(video->vbuf_ready, struct sdrv_cam_buffer, vbuf);
		list_add_tail(&kbuf->queue, &video->active_q.qhead);
		video->active_q.qlen++;
		video->vbuf_ready = NULL;
	}

	kbuf = list_first_entry(&video->buf_list, struct sdrv_cam_buffer, queue);
	list_del(&kbuf->queue);
	video->vbuf_ready = &kbuf->vbuf;

	spin_unlock_irqrestore(&video->buf_lock, flags);

	buf.paddr[0] = kbuf->paddr[0];
	buf.paddr[1] = kbuf->paddr[1];
	buf.paddr[2] = kbuf->paddr[2];

	ret = csi_cfg_img_buf(kstream->core->csi_hw, kstream->id, &buf);
	cam_isr_log_trace("update_buf: csi%d img %d, push vbuf %p\n",
			kstream->core->host_id, img_id, video->vbuf_ready);

	return ret;
}

static int kstream_error_handle(void *caller, uint32_t img_id)
{
	int ret = 0;
	struct kstream_device *kstream = (struct kstream_device *)caller;

	if (kstream->state == STOPPED)
		return 0;

	cam_isr_log_err("%s, csi%d img%d, error\n", __func__, kstream->core->host_id, img_id);
	return ret;
}

static int kstream_stream_on(struct kstream_device *kstream)
{
	int ret = 0;
	uint32_t outfmt = CSI_FMT_YUYV;
	struct csi_callback_t cb;
	struct csi_bus_info bus_info;
	struct csi_field_info field;
	struct csi_img_size size[2];
	//struct csi_img_crop crop[2];

	struct v4l2_pix_format_mplane *fmt;
	struct v4l2_mbus_framefmt *in_fmt, *out_fmt;
	in_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	out_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SRC];
	fmt = &kstream->video.active_fmt.fmt.pix_mp;
	kstream->state = IDLE;

	memset(&cb, 0, sizeof(struct csi_callback_t));
	cb.caller = (void *)kstream;
	cb.csi_frame_done = kstream_frame_done;
	cb.csi_update_buf = kstream_update_buf;
	cb.csi_error_handle = kstream_error_handle;
	pr_debug("csi_register_callback: caller %p, done %p\n", cb.caller, cb.csi_frame_done);
	ret = csi_register_callback(kstream->core->csi_hw, kstream->id, &cb);

	memset(&bus_info, 0, sizeof(struct csi_bus_info));
	if (kstream->core->mbus_type == V4L2_MBUS_CSI2)
		bus_info.bus_type = CSI_BUS_MIPICSI2;
	else if (kstream->core->mbus_type == V4L2_MBUS_PARALLEL)
		bus_info.bus_type = CSI_BUS_PARALLEL2;
	else if (kstream->core->mbus_type == V4L2_MBUS_BT656)
		bus_info.bus_type = CSI_BUS_BT656;
	else
		pr_err("error: unknown mbus_type %d\n", kstream->core->mbus_type);

	if (in_fmt->code == MEDIA_BUS_FMT_UYVY8_2X8)
		bus_info.mbus_fmt = CSI_MBUS_UYVY8_2X8;
	else if (in_fmt->code == MEDIA_BUS_FMT_YUYV8_2X8)
		bus_info.mbus_fmt = CSI_MBUS_YUYV8_2X8;
	else if (in_fmt->code == MEDIA_BUS_FMT_RGB888_1X24)
		bus_info.mbus_fmt = CSI_MBUS_RGB888_1X24;
	else
		pr_err("error: unknown mbus_fmt %d\n", in_fmt->code);

	ret = csi_cfg_bus(kstream->core->csi_hw, kstream->id, &bus_info);

	outfmt = to_csi_pixfmt(fmt->pixelformat);
	ret = csi_cfg_outfmt(kstream->core->csi_hw, kstream->id, outfmt);

	field.field_type = CSI_FIELD_NONE;
	ret = csi_cfg_field(kstream->core->csi_hw, kstream->id, &field);

	memset(&size[0], 0, sizeof(size));
	size[0].w = out_fmt->width;
	size[0].h = out_fmt->height;
	if(kstream->core->sync == 1)
	{
		size[0].h = size[0].h/4;
	}
	ret = csi_cfg_size(kstream->core->csi_hw, kstream->id, &size[0]);

	ret = kstream_update_buf(kstream, kstream->id);

	ret = csi_stream_on(kstream->core->csi_hw, kstream->id);
	pr_err("csi%d img %d,sync %d, stream on done\n",
			kstream->core->host_id, kstream->id, kstream->core->sync);
	camera_heartbeat_clear();
	kstream->state = RUNNING;
	kstream->video.sequence = 0;
	kstream->frame_cnt = 0;
	kstream->frame_loss_cnt = 0;

	return ret;
}

static int kstream_stream_off(struct kstream_device *kstream)
{
	int ret = 0;

	kstream->state = STOPPED;

	pr_err("csi%d img %d, stream off start\n", kstream->core->host_id, kstream->id);
	ret = csi_stream_off(kstream->core->csi_hw, kstream->id);
	pr_err("csi%d img %d, stream off done\n", kstream->core->host_id, kstream->id);

	kstream_flush_all_buffers(kstream, VB2_BUF_STATE_ERROR);

	return ret;
}

static int kstream_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	int ret;

	if (enable)
		ret = kstream_stream_on(kstream);
	else
		ret = kstream_stream_off(kstream);
	return ret;

}

static int kstream_set_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *fmt);

static struct v4l2_mbus_framefmt *__kstream_get_format(struct kstream_device
														   *kstream,
													   struct
													   v4l2_subdev_pad_config
														   *cfg,
													   unsigned int pad,
													   enum v4l2_subdev_format_whence
														   which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(&kstream->subdev, cfg, pad);

	return &kstream->mbus_fmt[pad];
}

static struct v4l2_rect *__kstream_get_crop(struct kstream_device *kstream,
					    struct v4l2_subdev_pad_config *cfg,
					    enum v4l2_subdev_format_whence
					    which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_crop(&kstream->subdev, cfg,
						SDRV_IMG_PAD_SRC);

	return &kstream->crop;
}

static int kstream_try_format(struct kstream_device *kstream,
			      struct v4l2_subdev_pad_config *cfg,
			      unsigned int pad, struct v4l2_mbus_framefmt *fmt,
			      enum v4l2_subdev_format_whence which)
{
	unsigned int i;

	switch (pad) {
	case SDRV_IMG_PAD_SINK:

		for (i = 0; i < ARRAY_SIZE(mbus_fmts); i++)
			if (fmt->code == mbus_fmts[i].code)
				break;

		if (i >= ARRAY_SIZE(mbus_fmts))
			fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;

		if (kstream->core->sync == 0) {
			fmt->width = clamp_t(u32, fmt->width, 1, SDRV_IMG_X_MAX);
			fmt->height = clamp_t(u32, fmt->height, 1, SDRV_IMG_Y_MAX);
		}
		if (kstream->core->sync == 1) {
			fmt->width = clamp_t(u32, fmt->width, 1, SDRV_IMG_X_MAX);
			fmt->height = clamp_t(u32, fmt->height, 1, SDRV_IMG_Y_MAX*4);
		}
		fmt->field = fmt->field;

		break;
	}

	fmt->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static void kstream_try_crop(struct kstream_device *kstream,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_rect *rect,
			     enum v4l2_subdev_format_whence which)
{
	struct v4l2_mbus_framefmt *fmt;

	fmt = __kstream_get_format(kstream, cfg, SDRV_IMG_PAD_SINK, which);

	if (!fmt)
		return;

	if (rect->width > fmt->width)
		rect->width = fmt->width;

	if (rect->width + rect->left > fmt->width)
		rect->left = fmt->width - rect->width;

	rect->top = 0;
	rect->height = fmt->height;
}

static int kstream_set_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_selection *sel)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_rect *rect;
	int ret = 0;

	if (sel->target == V4L2_SEL_TGT_CROP && sel->pad == SDRV_IMG_PAD_SRC) {
		rect = __kstream_get_crop(kstream, cfg, sel->which);

		if (!rect)
			return -EINVAL;

		kstream_try_crop(kstream, cfg, &sel->r, sel->which);
		*rect = sel->r;

		return 0;

	} else
		ret = -EINVAL;

	return ret;
}

static int kstream_set_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *fmt)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mbus_fmt, tmp_fmt;
	struct v4l2_subdev_selection sel = { 0 };
	struct v4l2_subdev_mbus_code_enum mbus_code = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};

	int ret = 0;

	ret = kstream_try_format(kstream, cfg, fmt->pad,
				 &fmt->format, fmt->which);

	if (ret < 0)
		return ret;

	tmp_fmt = fmt->format;

	mbus_fmt = __kstream_get_format(kstream, cfg, fmt->pad, fmt->which);

	if (!mbus_fmt)
		return -EINVAL;

	ret = kstream_try_format(kstream, cfg, fmt->pad,
				 &fmt->format, fmt->which);

	if (ret < 0)
		return ret;

	if (memcmp(&tmp_fmt, &fmt->format, sizeof(tmp_fmt))) {
		dev_err(kstream->dev, "CSI cannot support: %dx%d:%d\n",
			tmp_fmt.width, tmp_fmt.height, tmp_fmt.code);
		return -EINVAL;
	}

	*mbus_fmt = fmt->format;

	v4l2_subdev_call(kstream->sensor.subdev, pad, enum_mbus_code,
							 NULL, &mbus_code);

	mbus_fmt->code = mbus_code.code;

	kstream->crop.width = fmt->format.width;
	kstream->crop.height = fmt->format.height;

	if (fmt->pad == SDRV_IMG_PAD_SINK) {

		mbus_fmt = __kstream_get_format(kstream, cfg, SDRV_IMG_PAD_SRC,
						fmt->which);

		*mbus_fmt = fmt->format;

		kstream_try_format(kstream, cfg, SDRV_IMG_PAD_SRC, mbus_fmt,
				   fmt->which);

		sel.which = fmt->which;
		sel.pad = SDRV_IMG_PAD_SRC;
		sel.target = V4L2_SEL_TGT_CROP;
		sel.r.width = fmt->format.width;
		sel.r.height = fmt->format.height;
		ret = kstream_set_selection(sd, cfg, &sel);

		if (ret < 0)
			return ret;
	}

	return ret;
}

static int kstream_init_formats(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format = {
		.pad = SDRV_IMG_PAD_SINK,
		.which = fh ? V4L2_SUBDEV_FORMAT_TRY :
		    V4L2_SUBDEV_FORMAT_ACTIVE,
		.format = {
			   .code = MEDIA_BUS_FMT_UYVY8_2X8,
			   .width = 640,
			   .height = 480,
			   },
	};

	return kstream_set_format(sd, fh ? fh->pad : NULL, &format);
}

static const struct v4l2_subdev_core_ops kstream_core_ops = {
	.s_power = kstream_set_power,
};

static const struct v4l2_subdev_video_ops sdrv_cam_video_ops = {
	.s_stream = kstream_set_stream,
};

static const struct v4l2_subdev_pad_ops kstream_pad_ops = {
	.set_fmt = kstream_set_format, //TBD MOVE PART
};

static const struct v4l2_subdev_ops sdrv_stream_v4l2_ops = {
	.core = &kstream_core_ops,
	.video = &sdrv_cam_video_ops,
	.pad = &kstream_pad_ops,
};

static const struct v4l2_subdev_internal_ops sdrv_stream_internal_ops = {
	.open = kstream_init_formats,
};

static const struct media_entity_operations sdrv_stream_media_ops = {
	.link_setup = kstream_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};


static int kstream_init_device(struct kstream_device *kstream)
{
	kstream->state = INITING;

	return kstream_init_formats(&kstream->subdev, NULL);
}

int sdrv_stream_register_entities(struct kstream_device *kstream,
				  struct v4l2_device *v4l2_dev)
{
	struct v4l2_subdev *sd = &kstream->subdev;
	struct media_pad *pads = kstream->pads;
	struct device *dev = kstream->dev;
	struct sdrv_cam_video *video = &kstream->video;
	int ret;

	v4l2_subdev_init(sd, &sdrv_stream_v4l2_ops);
	sd->internal_ops = &sdrv_stream_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, ARRAY_SIZE(sd->name), "%s%d",
		 SDRV_IMG_NAME, kstream->id);
	v4l2_set_subdevdata(sd, kstream);

	ret = kstream_init_device(kstream);

	if (ret < 0) {
		dev_err(dev, "Stream %d: failed to init format %d\n",
			kstream->id, ret);
		return ret;
	}

	pads[SDRV_IMG_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[SDRV_IMG_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;

	sd->entity.function = MEDIA_ENT_F_IO_V4L;
	sd->entity.ops = &sdrv_stream_media_ops;
	ret = media_entity_pads_init(&sd->entity, SDRV_IMG_PAD_NUM, pads);

	if (ret < 0) {
		dev_err(dev, "Failed to init media entity: %d\n", ret);
		return ret;
	}

	ret = v4l2_device_register_subdev(v4l2_dev, sd);

	if (ret < 0) {
		dev_err(dev, "Failed to register subdev: %d\n", ret);
		goto err_reg_subdev;
	}

	video->core = kstream->core;
	video->dev = kstream->dev;
	video->id = kstream->id;
	ret = sdrv_cam_video_register(video, v4l2_dev);

	if (ret < 0) {
		dev_err(dev, "Failed to register video device: %d\n", ret);
		goto err_reg_video;
	}

	ret = media_create_pad_link(&sd->entity, SDRV_IMG_PAD_SRC,
				    &video->vdev.entity, 0,
				    MEDIA_LNK_FL_IMMUTABLE |
				    MEDIA_LNK_FL_ENABLED);

	if (ret < 0) {
		dev_err(dev, "Failed to link %s->%s entities: %d\n",
			sd->entity.name, video->vdev.entity.name, ret);
		goto err_link;
	}

	kstream->state = INITIALED;
	dev_info(dev, "Camera Stream %d initialized", kstream->id);

	return 0;
 err_link:
	sdrv_cam_video_unregister(video);
 err_reg_video:
	v4l2_device_unregister_subdev(sd);
 err_reg_subdev:
	media_entity_cleanup(&sd->entity);
	return ret;
}

int sdrv_stream_unregister_entities(struct kstream_device *kstream)
{
	struct v4l2_subdev *sd = &kstream->subdev;

	kstream_stream_off(kstream);

	sdrv_cam_video_unregister(&kstream->video);

	v4l2_device_unregister_subdev(sd);

	media_entity_cleanup(&sd->entity);

	return 0;
}
