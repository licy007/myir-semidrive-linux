/*
 * sdrv-kstream.c
 *
 * Semidrive platform kstream subdev operation
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
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

#include "sdrv-csi.h"

#define IMG_ENABLE					0x0
#define IMG_RST_(i)					BIT(i + 4)
#define IMG_IMG_EN_SHIFT			0

#define IMG_STRIDE_(n)				(0x18 + (n) * 4)

#define IMG_IMG_SIZE				(0x24)

#define IMG_SIZE_WIDTH_SHIFT		16
#define IMG_SIZE_HEIGHT_SHIFT		0

#define IMG_CTRL					(0x28)
#define IMG_FC_DUAL					BIT(13)
#define IMG_MIMG_YUV422				BIT(6)
#define IMG_MIMG_YUV420_LEGACY		BIT(4)
#define IMG_INTERFACE_MASK			0x07
#define IMG_MIMG_CSI2_SEL			0x00
#define IMG_PARALLEL_SEL			0x02
#define IMG_PARALLEL_SEL_OTHER		0x04
#define IMG_BT656_SEL				0x05
#define IMG_PIXEL_CTRL_MASK			0x3FF8
#define IMG_PIXEL_VLD_EVEN_SHIFT	11
#define IMG_PIXEL_VLD_ODD_SHIFT		9
#define IMG_VSYNC_MASK_SHIFT		7

#define IMG_MASK_0				(0x2C)
#define IMG_MASK_1				(0x30)

#define IMG_CROP_(n)				(0x44 + (n) * 4)
#define IMG_CROP_MASK				0xFFFFFFFF
#define IMG_CROP_LEN_SHIFT			16
#define IMG_CROP_POS_SHIFT			0

#define IMG_CHN_CTRL				(0x34)

#define IMG_PIXEL_ENDIAN_SHIFT		7
#define IMG_PIXEL_ROUNDING			BIT(6)
#define IMG_PIXEL_CROP_MASK			0x38
#define IMG_PIXEL_CROP_SHIFT		3
#define IMG_PIXEL_STREAM_EN_SHIFT	0
#define IMG_PIXEL_STREAM_EN_MASK	0x07

#define IMG_CHN_SPLIT_(n)			(0x38 + (n) * 4)
#define IMG_CHN_PACK_(n)			(0x50 + (n) * 4)

#define IMG_INT_ST0					0x04
#define IMG_INT_MSK0				0x0C
#define IMG_INT_DONE_SHIFT			0
#define IMG_INT_SDW_SHIFT			4

#define IMG_INT_ST1					0x08
#define IMG_INT_MSK1				0x10
#define IMG_CROP_ERR_SHIFT			0
#define IMG_PIX_ERR_SHIFT			4
#define IMG_OVERFLOW_SHIFT			8
#define IMG_BUS_ERR_SHIFT			12

#define IMG_REG_LOAD				0x14
#define REG_LOAD_WDMA_CFG			BIT(17)
#define REG_LOAD_WDMA_ARB			BIT(16)
#define REG_UPDATE_SHIFT			8
#define IMG_SHADOW_UPDATE_SHIFT		4
#define IMG_SHADOW_SET_SHIFT		0

#define IMG_BADDR_H_(n)			(0x00 + (n) * 8)
#define IMG_BADDR_L_(n)			(0x04 + (n) * 8)
#define IMG_BADDR_H_MASK			0xFF00000000
#define IMG_BADDR_H_SHIFT			32
#define IMG_BADDR_L_MASK			0xFFFFFFFF
#define IMG_BADDR_L_SHIFT			0

#define IMG_PARA_BT0_(n)			(0x500 + (n) * 0x40)
#define IMG_PARA_BT1_(n)			(0x504 + (n) * 0x40)
#define IMG_PARA_BT2_(n)			(0x508 + (n) * 0x40)
#define IMG_PARA_MAP_(n, i)			(0x50C + (n) * 0x40 + (i) * 0x04)

#define PARA_EXIT_SHIFT				0
#define PARA_PACK_ODD_SHIFT			1
#define PARA_PACK_EVEN_SHIFT		3
#define PARA_UV_ODD_SHIFT			6
#define PARA_UV_EVEN_SHIFT			8
#define PARA_DE_POL					BIT(10)
#define PARA_VS_POL					BIT(11)
#define PARA_HS_POL					BIT(12)
#define PARA_CLK_POL				BIT(13)
#define PARA_EVEN_EN				BIT(14)
#define PARA_EVEN_HEIGHT_SHIFT		15

#define BT_PROGRESSIVE_SHIFT		0
#define BT_VSYNC_SHIFT				1
#define BT_FIELD_SHIFT				2
#define BT_VSYNC_POSTPONE_SHIF		16

#define BT_FILTER_EN_SHIFT			0
#define BT_VSYNC_EDGE_SHIFT			17
#define PARA_PACK_SWAP				18

#define PIXEL_MAP_COUNT				12

#define WDMA_JUMP					0x20
#define WDMA_DFIFO(n, i)			(0x300 + ((n) * 2 + i) * WDMA_JUMP)
#define WDMA_CFIFO(n, i)			(0x304 + ((n) * 2 + i) * WDMA_JUMP)
#define WDMA_AXI0(n, i)				(0x308 + ((n) * 2 + i) * WDMA_JUMP)
#define WDMA_AXI1(n, i)				(0x30C + ((n) * 2 + i) * WDMA_JUMP)
#define WDMA_AXI2(n, i)				(0x310 + ((n) * 2 + i) * WDMA_JUMP)

#define IMG_REG_LEN		0x80
#define IMG_CNT 4

struct wdma_fifo_t {
	uint32_t dfifo;
	uint32_t cfifo;
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
#if 0
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
	},
#else
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

#endif
};


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

	for (i = 0; i < ARRAY_SIZE(pix_fmts); i++) {
		if ((mbus_code == pix_fmts[i].mbus_code) && (kstream->core->bt == pix_fmts[i].bt))
			kstream_update_support_formats(kstream, pix_fmts[i].pixfmt);
	}

	return 0;
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

static int kstream_set_stride(struct kstream_device *kstream)
{
	struct v4l2_pix_format_mplane *pix_fmt;
	struct v4l2_mbus_framefmt *in_fmt, *out_fmt;
	int pos_i, pos_o, n;
	u32 val;
	s32 width, height, stride;
	u32 j;

	pos_o = kstream_get_pix_fmt(kstream, &pix_fmt);

	if (pos_o < 0)
		return pos_o;

	in_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	out_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SRC];

	for (pos_i = 0; pos_i < ARRAY_SIZE(mbus_fmts); pos_i++)
		if (mbus_fmts[pos_i].code == in_fmt->code)
			break;

	if (pos_i >= ARRAY_SIZE(mbus_fmts))
		return -EINVAL;

	if ((kstream->core->host_id == 0) || (kstream->core->host_id == 1)
			|| (kstream->core->host_id == 3) || (kstream->core->host_id == 4))
		width = in_fmt->width - 1;
	else
		width = in_fmt->width - (1 + mbus_fmts[pos_i].pix_odd);

	if (kstream->core->sync == 0) {
		if (V4L2_FIELD_HAS_TOP(in_fmt->field) || V4L2_FIELD_HAS_BOTTOM(in_fmt->field)) {
			height = (in_fmt->height + 1) / 2 - 1;
			height += (kstream->hcrop_top_back + kstream->hcrop_top_front);
		} else {
			height = in_fmt->height - 1;
			height += kstream->hcrop_back + kstream->hcrop_front;
		}
	} else {
		height = in_fmt->height / 4 - 1;
	}

	WARN_ON(width <= 0 || height <= 0);

	val = (width << IMG_SIZE_WIDTH_SHIFT) |
	    (height << IMG_SIZE_HEIGHT_SHIFT);

	if (kstream->core->sync == 0) {
		kcsi_writel(kstream->base, IMG_IMG_SIZE, val);
	} else {
		for (j = 0; j < IMG_CNT; j++)
			kcsi_writel(kstream->base + IMG_REG_LEN * j,
				    IMG_IMG_SIZE, val);
	}

	for (n = 0; n < pix_fmt->num_planes; n++) {
		if (V4L2_FIELD_HAS_BOTH(in_fmt->field))
			stride = 2 * out_fmt->width * pix_fmts[pos_o].bpp[n] / 8;
		else
			stride = out_fmt->width * pix_fmts[pos_o].bpp[n] / 8;

		if (stride) {
			if (kstream->core->sync == 0) {
				kcsi_writel(kstream->base, IMG_STRIDE_(n),
					    stride);
			} else {
				for (j = 0; j < IMG_CNT; j++)
					kcsi_writel(kstream->base +
						    IMG_REG_LEN * j,
						    IMG_STRIDE_(n), stride);
			}
		}
	}

	return 0;
}

static int kstream_set_wdma(struct kstream_device *kstream)
{
	struct v4l2_pix_format_mplane *pix_fmt;
	int pos, i;
	u32 axi0 = 0x3C8;
	u32 axi1 = 0x00;
	u32 axi2 = 0x0B;
	int j;
	int n;
	struct wdma_fifo_t wfifo[2][2] = {
		{
			/* for one plane output, assign all fifo to stream0 */
			{ 0x00400010, 0x08 },
			{ 0x00400000, 0x00 },
		},
		{
			/* for two planes output */
			{ 0x00400008, 0x04 },
			{ 0x00400008, 0x04 },
		},
	};

	pos = kstream_get_pix_fmt(kstream, &pix_fmt);

	if (pos < 0)
		return pos;

	n = (pix_fmt->num_planes == 1) ? 0 : 1;
	if (kstream->core->sync == 0) {
		for (i = 0; i < IMG_HW_NUM_PLANES; i++) {
			kcsi_writel(kstream->core->base,
				    WDMA_DFIFO(kstream->id, i), wfifo[n][i].dfifo);
			kcsi_writel(kstream->core->base,
				    WDMA_CFIFO(kstream->id, i), wfifo[n][i].cfifo);
			kcsi_writel(kstream->core->base,
				    WDMA_AXI0(kstream->id, i), axi0);
			kcsi_writel(kstream->core->base,
				    WDMA_AXI1(kstream->id, i), axi1);
			kcsi_writel(kstream->core->base,
				    WDMA_AXI2(kstream->id, i), axi2);
		}

		kcsi_set(kstream->core->base, IMG_REG_LOAD,
			 REG_LOAD_WDMA_CFG /* | REG_LOAD_WDMA_ARB */);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			for (i = 0; i < IMG_HW_NUM_PLANES; i++) {
				kcsi_writel(kstream->core->base,
					    WDMA_DFIFO(j, i), wfifo[n][i].dfifo);
				kcsi_writel(kstream->core->base,
					    WDMA_CFIFO(j, i), wfifo[n][i].cfifo);
				kcsi_writel(kstream->core->base,
					    WDMA_AXI0(j, i), axi0);
				kcsi_writel(kstream->core->base,
					    WDMA_AXI1(j, i), axi1);
				kcsi_writel(kstream->core->base,
					    WDMA_AXI2(j, i), axi2);
			}

			kcsi_set(kstream->core->base, IMG_REG_LOAD,
				 REG_LOAD_WDMA_CFG /* | REG_LOAD_WDMA_ARB */);
		}
	}

	return 0;
}

static int kstream_set_para_bt(struct kstream_device *kstream)
{
	struct v4l2_pix_format_mplane *pix_fmt;
	struct v4l2_fwnode_bus_parallel *bus;
	int pos, i, height;
	u32 value0 = 0, value1 = 0, value2 = 0;
	u32 map[PIXEL_MAP_COUNT] = {
		0x481c6144, 0x40c20402, 0x2481c614, 0xd24503ce,
		0xe34c2ca4, 0x4d24503c, 0x5c6da658, 0x85d65547,
		0x75c6da65, 0xe69648e2, 0x28607de9, 0x9e69648e
	};
	struct v4l2_mbus_framefmt *in_fmt;

	in_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	pos = kstream_get_pix_fmt(kstream, &pix_fmt);

	if (pos < 0)
		return pos;

	switch (kstream->interface.mbus_type) {
	case V4L2_MBUS_CSI2:
	case SDRV_MBUS_DC2CSI2:
		value0 |= 0 << PARA_EXIT_SHIFT;
		break;

	case V4L2_MBUS_BT656:
	case V4L2_MBUS_PARALLEL:
	case SDRV_MBUS_DC2CSI1:
		bus = &kstream->core->vep.bus.parallel;
		value0 |= (1 << PARA_EXIT_SHIFT) |
		    (pix_fmts[pos].pack_uv_odd << PARA_UV_ODD_SHIFT) |
		    (pix_fmts[pos].pack_uv_even << PARA_UV_EVEN_SHIFT) |
		    (pix_fmts[pos].pack_pix_odd << PARA_PACK_ODD_SHIFT) |
		    (pix_fmts[pos].pack_pix_even << PARA_PACK_EVEN_SHIFT);

		if (kstream->interface.mbus_type == V4L2_MBUS_BT656) {
			if (V4L2_FIELD_HAS_BOTH(in_fmt->field))
				value1 = (1 << BT_FIELD_SHIFT);
			else
				value1 |= (1 << BT_PROGRESSIVE_SHIFT) |
				    (pix_fmt->width << BT_VSYNC_POSTPONE_SHIF) |
				    (1 << BT_FIELD_SHIFT);
		}

		if (bus->flags & V4L2_MBUS_PCLK_SAMPLE_RISING)
			value0 |= PARA_CLK_POL;

		if (bus->flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH)
			value0 |= PARA_HS_POL;

		if (bus->flags & V4L2_MBUS_VSYNC_ACTIVE_LOW)
			value0 |= PARA_VS_POL;

		if (V4L2_FIELD_HAS_BOTH(in_fmt->field)) {
			height = (in_fmt->height + 1) / 2 - 1;
			height += (kstream->hcrop_bottom_back + kstream->hcrop_bottom_front);
			value0 |= ((height << PARA_EVEN_HEIGHT_SHIFT) | PARA_EVEN_EN);
        }
		break;

	default:
		break;
	}

	if (V4L2_FIELD_HAS_BOTH(in_fmt->field))
		value2 = 0;
	else
		value2 = 1 << BT_VSYNC_EDGE_SHIFT;

	if (kstream->interface.mbus_type == V4L2_MBUS_CSI2) {
		kcsi_writel(kstream->core->base, IMG_PARA_BT0_(0), value0);
		//value1 = 0x2800005;
		kcsi_writel(kstream->core->base, IMG_PARA_BT1_(0), value1);
		kcsi_writel(kstream->core->base, IMG_PARA_BT2_(0), value2);

		for (i = 0; i < PIXEL_MAP_COUNT; i++)
			kcsi_writel(kstream->core->base,
				    IMG_PARA_MAP_(0, i), map[i]);
	} else {
		kcsi_writel(kstream->core->base, IMG_PARA_BT0_(kstream->id),
			    value0);
		//value1 = 0x2800005;
		kcsi_writel(kstream->core->base, IMG_PARA_BT1_(kstream->id),
			    value1);
		kcsi_writel(kstream->core->base, IMG_PARA_BT2_(kstream->id),
			    value2);

		for (i = 0; i < PIXEL_MAP_COUNT; i++)
			kcsi_writel(kstream->core->base,
				    IMG_PARA_MAP_(kstream->id, i), map[i]);
	}

	return 0;
}

static int kstream_set_channel(struct kstream_device *kstream)
{
	struct v4l2_pix_format_mplane *active_fmt;
	int pos, n;
	u32 ctrl = 0;
	u32 j;

	pos = kstream_get_pix_fmt(kstream, &active_fmt);
	if (pos < 0)
		return pos;

	dev_info(kstream->dev, "%s: pix_fmts[pos=%d]: 0x%08x, mbus_fmt 0x%x\n",
		__func__, pos, pix_fmts[pos].pixfmt, pix_fmts[pos].mbus_code);

	for (n = 0; n < active_fmt->num_planes; n++) {
		dev_info(kstream->dev, "plane.%d, split= [%d, %d, %d, %d], color_depth = [%d, %d, %d, %d]\n",
			n, ((pix_fmts[pos].split[0] >> 9) & 0x7), ((pix_fmts[pos].split[0] >> 6) & 0x7),
			((pix_fmts[pos].split[0] >> 3) & 0x7), (pix_fmts[pos].split[0] & 0x7),
			((pix_fmts[pos].pack[0] >> 15) & 0x1F), ((pix_fmts[pos].pack[0] >> 10) & 0x1F),
			((pix_fmts[pos].pack[0] >> 5) & 0x1F), (pix_fmts[pos].pack[0] & 0x1F));

		if (kstream->core->sync == 0) {
			kcsi_writel(kstream->base, IMG_CHN_SPLIT_(n),
					pix_fmts[pos].split[n]);
			kcsi_writel(kstream->base, IMG_CHN_PACK_(n),
					pix_fmts[pos].pack[n]);
		} else {
			for (j = 0; j < IMG_CNT; j++) {
				kcsi_writel(kstream->base +
						IMG_REG_LEN * j,
						IMG_CHN_SPLIT_(n), pix_fmts[pos].split[n]);
				kcsi_writel(kstream->base +
						IMG_REG_LEN * j,
						IMG_CHN_PACK_(n), pix_fmts[pos].pack[n]);
			}
		}

		ctrl |= ((1 << n) << IMG_PIXEL_STREAM_EN_SHIFT);
	}

	if (kstream->core->sync == 0) {
		kcsi_clr_and_set(kstream->base, IMG_CHN_CTRL,
				 IMG_PIXEL_STREAM_EN_MASK, ctrl);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			kcsi_clr_and_set(kstream->base + IMG_REG_LEN * j,
					 IMG_CHN_CTRL, IMG_PIXEL_STREAM_EN_MASK,
					 ctrl);
		}
	}

	return 0;
}

static int kstream_set_crop(struct kstream_device *kstream)
{
	struct v4l2_rect *rect = &kstream->crop;
	struct v4l2_mbus_framefmt *mbus_fmt;
	struct v4l2_pix_format_mplane *pix_fmt;
	int pos, n;
	u32 val = 0, chnl_ctrl = 0, div;
	s32 width, left;
	u32 j;

	mbus_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	pix_fmt = &kstream->video.active_fmt.fmt.pix_mp;

	if (rect->width >= mbus_fmt->width)
		return 0;

	pos = kstream_get_pix_fmt(kstream, &pix_fmt);

	if (pos < 0)
		return pos;

	if (pix_fmt->pixelformat == V4L2_PIX_FMT_RGB24 ||
	    pix_fmt->pixelformat == V4L2_PIX_FMT_YUV32)
		div = 1;
	else
		div = 2;

	width = rect->width / div;
	left = rect->left / div - 1;

	WARN_ON(width == 0);

	if (left < 0)
		left = 0;

	val = (width << IMG_CROP_LEN_SHIFT) | (left << IMG_CROP_POS_SHIFT);

	if (kstream->core->sync == 0) {
		for (n = 0; n < pix_fmt->num_planes && n < IMG_HW_NUM_PLANES;
		     n++) {
			kcsi_clr_and_set(kstream->base, IMG_CROP_(n),
					 IMG_CROP_MASK, val);
			chnl_ctrl |= ((1 << n) << IMG_PIXEL_CROP_SHIFT);
		}

		kcsi_clr_and_set(kstream->base, IMG_CHN_CTRL,
				 IMG_PIXEL_CROP_MASK, chnl_ctrl);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			for (n = 0;
			     n < pix_fmt->num_planes && n < IMG_HW_NUM_PLANES;
			     n++) {
				kcsi_clr_and_set(kstream->base +
						 IMG_REG_LEN * j, IMG_CROP_(n),
						 IMG_CROP_MASK, val);
				chnl_ctrl |= ((1 << n) << IMG_PIXEL_CROP_SHIFT);
			}

			kcsi_clr_and_set(kstream->base + IMG_REG_LEN * j,
					 IMG_CHN_CTRL, IMG_PIXEL_CROP_MASK,
					 chnl_ctrl);
		}
	}

	return 0;
}

static int kstream_set_pixel_ctrl(struct kstream_device *kstream)
{
	struct kstream_interface *interface = &kstream->interface;
	struct v4l2_mbus_framefmt *fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	u32 val = 0;
	int i;
	u32 j;

	for (i = 0; i < ARRAY_SIZE(mbus_fmts); i++)
		if (mbus_fmts[i].code == fmt->code)
			break;

	if (i >= ARRAY_SIZE(mbus_fmts))
		return -EINVAL;

	if (interface->mbus_type == V4L2_MBUS_CSI2) {
		switch (fmt->code) {
		case MEDIA_BUS_FMT_UYVY8_2X8:
		case MEDIA_BUS_FMT_YUYV8_2X8:
			val |= IMG_MIMG_YUV422;
			break;

		case MEDIA_BUS_FMT_UYVY8_1_5X8:
		case MEDIA_BUS_FMT_YVYU8_1_5X8:
			val |= IMG_MIMG_YUV420_LEGACY;
			break;

		default:
			break;
		}
	} else {
		if (mbus_fmts[i].fc_dual)
			val |= IMG_FC_DUAL;
	}

	val |= (0x01 << IMG_VSYNC_MASK_SHIFT);

	if (kstream->core->host_id == 2 || kstream->core->host_id == 5) {
		val |= (mbus_fmts[i].pix_even << IMG_PIXEL_VLD_EVEN_SHIFT);
		val |= (mbus_fmts[i].pix_odd << IMG_PIXEL_VLD_ODD_SHIFT);
	}

	if (kstream->core->sync == 0) {
		//val = 0x4a82;
		kcsi_clr_and_set(kstream->base, IMG_CTRL, IMG_PIXEL_CTRL_MASK,
				 val);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			kcsi_clr_and_set(kstream->base + IMG_REG_LEN * j,
					 IMG_CTRL, IMG_PIXEL_CTRL_MASK, val);
		}
	}

	//val = 0x55 << REG_UPDATE_SHIFT;
	val = 0xff << REG_UPDATE_SHIFT;
	kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
	return 0;
}

static int kstream_set_pixel_mask(struct kstream_device *kstream)
{
	u32 val = 0xFFFFFFFF;
	u32 j;

	if (kstream->core->sync == 0) {
		kcsi_set(kstream->base, IMG_MASK_0, val);
		kcsi_set(kstream->base, IMG_MASK_1, val);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			kcsi_set(kstream->base + IMG_REG_LEN * j, IMG_MASK_0,
				 val);
			kcsi_set(kstream->base + IMG_REG_LEN * j, IMG_MASK_1,
				 val);
		}
	}

	return 0;
}

static void kstream_flush_all_buffers(struct kstream_device *kstream,
				      enum vb2_buffer_state state)
{
	struct kstream_video *video = &kstream->video;
	struct kstream_buffer *kbuf, *node;
	struct vb2_v4l2_buffer *vbuf;
	unsigned long flags;

	spin_lock_irqsave(&video->buf_lock, flags);
	vbuf = video->vbuf_ready;

	if (vbuf) {
		video->vbuf_ready = NULL;
		kbuf = container_of(vbuf, struct kstream_buffer, vbuf);
		list_add_tail(&kbuf->queue, &video->buf_list);
	}

	vbuf = video->vbuf_active;

	if (vbuf) {
		video->vbuf_active = NULL;
		kbuf = container_of(vbuf, struct kstream_buffer, vbuf);
		list_add_tail(&kbuf->queue, &video->buf_list);
	}

	spin_unlock_irqrestore(&video->buf_lock, flags);

	list_for_each_entry_safe(kbuf, node, &video->buf_list, queue) {
		vb2_buffer_done(&kbuf->vbuf.vb2_buf, state);
		list_del(&kbuf->queue);
	}
}

static int kstream_set_baddr(struct kstream_device *kstream)
{
	struct kstream_video *video = &kstream->video;
	const struct v4l2_pix_format_mplane *format =
	    &video->active_fmt.fmt.pix_mp;
	struct kstream_buffer *kbuf;
	unsigned long flags;
	unsigned int i;
	u32 val, addr_h, addr_l;
	u32 length = 0;
	u8  bpp;
	u32 j;
	dma_addr_t paddr;
	struct v4l2_mbus_framefmt *in_fmt;

	in_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	if (video->core->sync == 1) {
		switch (in_fmt->code) {
		case MEDIA_BUS_FMT_UYVY8_2X8:
		case MEDIA_BUS_FMT_YUYV8_2X8:
			bpp = 16;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
			bpp = 24;
			break;
		default:
			printk("Not supported format 0x%x yet.\n", in_fmt->code);
			return -EIO;
		}
		//for sync mode, a continuous dma buffer is allocated for the 4 IPI forwarding data stream
		//the buffer length for each IPI is (image_width* image_height*bpp/8)/4
		length = (in_fmt->width*in_fmt->height*bpp/8)/4;
	}
	if (list_empty(&video->buf_list)) {
		if (kstream->state == RUNNING)
			kstream->state = IDLE;

		return 0;
	} else if (kstream->state == IDLE)
		kstream->state = RUNNING;

	if (video->vbuf_active)
		return -EFAULT;

	if (V4L2_FIELD_HAS_BOTH(in_fmt->field)) {
		if (kstream->field == 1) {
			kstream->field = 0;
			for (i = 0; i < format->num_planes; i++) {
				addr_h = kcsi_readl(kstream->base, IMG_BADDR_H_(i));
				addr_l = kcsi_readl(kstream->base, IMG_BADDR_L_(i));
				val = kcsi_readl(kstream->base, IMG_STRIDE_(i));
				if ((in_fmt->field == V4L2_FIELD_INTERLACED) || (in_fmt->field == V4L2_FIELD_INTERLACED_BT) || (in_fmt->field == V4L2_FIELD_SEQ_BT))
					paddr = ((dma_addr_t)(addr_h) << IMG_BADDR_H_SHIFT) + addr_l - val/2;
				else
					paddr = ((dma_addr_t)(addr_h) << IMG_BADDR_H_SHIFT) + addr_l + val/2;
				addr_h = (paddr & IMG_BADDR_H_MASK) >> IMG_BADDR_H_SHIFT;
				addr_l = (paddr & IMG_BADDR_L_MASK) >> IMG_BADDR_L_SHIFT;
				kcsi_writel(kstream->base, IMG_BADDR_H_(i), addr_h);
				kcsi_writel(kstream->base, IMG_BADDR_L_(i), addr_l);
			}

			val = (1 << kstream->id) << IMG_SHADOW_SET_SHIFT;
			kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
			return 0;
		} else if (kstream->field == 0)
			kstream->field = 1;
	}
	spin_lock_irqsave(&video->buf_lock, flags);
	kbuf = list_first_entry(&video->buf_list, struct kstream_buffer, queue);
	list_del(&kbuf->queue);
	video->vbuf_active = video->vbuf_ready;
	video->vbuf_ready = &kbuf->vbuf;
	spin_unlock_irqrestore(&video->buf_lock, flags);

	if (video->core->sync == 0) {
		for (i = 0; i < format->num_planes; i++) {
			addr_h =
			    (kbuf->paddr[i] & IMG_BADDR_H_MASK) >> IMG_BADDR_H_SHIFT;
			addr_l =
			    (kbuf->paddr[i] & IMG_BADDR_L_MASK) >> IMG_BADDR_L_SHIFT;
			if ((in_fmt->field == V4L2_FIELD_INTERLACED) || (in_fmt->field == V4L2_FIELD_INTERLACED_BT) || (in_fmt->field == V4L2_FIELD_SEQ_BT)) {
				val = kcsi_readl(kstream->base, IMG_STRIDE_(i));
				paddr = kbuf->paddr[i] + val/2;
				addr_h = (paddr & IMG_BADDR_H_MASK) >> IMG_BADDR_H_SHIFT;
				addr_l = (paddr & IMG_BADDR_L_MASK) >> IMG_BADDR_L_SHIFT;
			}
			kcsi_writel(kstream->base, IMG_BADDR_H_(i), addr_h);
			kcsi_writel(kstream->base, IMG_BADDR_L_(i), addr_l);
		}

		val = (1 << kstream->id) << IMG_SHADOW_SET_SHIFT;
		kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			for (i = 0; i < format->num_planes; i++) {
				addr_h =
					((kbuf->paddr[i] +
					  length *
					  j) & IMG_BADDR_H_MASK) >>
					IMG_BADDR_H_SHIFT;
				addr_l =
				    ((kbuf->paddr[i] +
					  length *
				      j) & IMG_BADDR_L_MASK) >>
				    IMG_BADDR_L_SHIFT;
				kcsi_writel(kstream->base + IMG_REG_LEN * j,
					    IMG_BADDR_H_(i), addr_h);
				kcsi_writel(kstream->base + IMG_REG_LEN * j,
					    IMG_BADDR_L_(i), addr_l);
			}
		}

		val = 0xf << IMG_SHADOW_SET_SHIFT;
		kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
	}

	return 0;
}

static int kstream_setup_regs(struct kstream_device *kstream)
{
	int ret;

	ret = kstream_set_wdma(kstream);

	if (ret < 0)
		return ret;

	ret = kstream_set_pixel_mask(kstream);

	if (ret < 0)
		return ret;

	ret = kstream_set_pixel_ctrl(kstream);

	if (ret < 0)
		return ret;

	ret = kstream_set_para_bt(kstream);

	if (ret < 0)
		return ret;

	ret = kstream_set_crop(kstream);

	if (ret < 0)
		return ret;

	ret = kstream_set_channel(kstream);

	if (ret < 0)
		return ret;

	ret = kstream_set_stride(kstream);

	if (ret < 0)
		return ret;

	ret = kstream_set_baddr(kstream);

	if (ret < 0)
		return ret;

	return ret;
}

static int kstream_enable_irq_common(struct kstream_device *kstream)
{
	struct v4l2_pix_format_mplane *pix_fmt;
	struct v4l2_rect *rect = &kstream->crop;
	u8 id = kstream->id;
	u32 val, n;

	pix_fmt = &kstream->video.active_fmt.fmt.pix_mp;

	if (kstream->core->sync == 0) {
		val = (1 << id) << IMG_INT_SDW_SHIFT;
		kcsi_set(kstream->core->base, IMG_INT_MSK0, val);

		val = (1 << id) << IMG_PIX_ERR_SHIFT;
		val |= (1 << id) << IMG_OVERFLOW_SHIFT;

		if (rect->width)
			val |= (1 << id) << IMG_CROP_ERR_SHIFT;

		for (n = 0; n < pix_fmt->num_planes && n < IMG_HW_NUM_PLANES;
		     n++)
			val |=
			    (1 << (id * IMG_HW_NUM_PLANES + n)) <<
			    IMG_BUS_ERR_SHIFT;

		kcsi_set(kstream->core->base, IMG_INT_MSK1, val);

		val = (1 << id) << IMG_SHADOW_UPDATE_SHIFT;
		kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
	} else {
		val = 0xf << IMG_INT_SDW_SHIFT;
		kcsi_set(kstream->core->base, IMG_INT_MSK0, val);

		val = 0xf << IMG_PIX_ERR_SHIFT;
		val |= 0xf << IMG_OVERFLOW_SHIFT;

		if (rect->width)
			val |= 0xf << IMG_CROP_ERR_SHIFT;

		val |= 0xfff << IMG_BUS_ERR_SHIFT;

		kcsi_set(kstream->core->base, IMG_INT_MSK1, val);

		val = 0xf << IMG_SHADOW_UPDATE_SHIFT;
		kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
	}

	return 0;
}

static int kstream_enable_stream(struct kstream_device *kstream)
{
	u8 id = kstream->id;

	if (kstream->core->sync == 0) {
		kcsi_set(kstream->core->base, IMG_ENABLE,
			 (1 << id) << IMG_IMG_EN_SHIFT);
	} else {
		kcsi_set(kstream->core->base, IMG_ENABLE,
			 (0xf) << IMG_IMG_EN_SHIFT);
	}

	return 0;
}

static int kstream_disable_irq_common(struct kstream_device *kstream)
{
	struct v4l2_pix_format_mplane *pix_fmt;
	u8 id = kstream->id;
	u32 val, n;

	pix_fmt = &kstream->video.active_fmt.fmt.pix_mp;

	if (kstream->core->sync == 0) {
		val = (1 << id) << IMG_CROP_ERR_SHIFT;
		val |= (1 << id) << IMG_PIX_ERR_SHIFT;
		val |= (1 << id) << IMG_OVERFLOW_SHIFT;

		for (n = 0; n < pix_fmt->num_planes && n < IMG_HW_NUM_PLANES;
		     n++)
			val |=
			    (1 << (id * IMG_HW_NUM_PLANES + n)) <<
			    IMG_BUS_ERR_SHIFT;

		kcsi_clr(kstream->core->base, IMG_INT_MSK1, val);

		val = (1 << id) << IMG_INT_SDW_SHIFT;
		kcsi_clr(kstream->core->base, IMG_INT_MSK0, val);

		val = (1 << id) << IMG_SHADOW_UPDATE_SHIFT;
		kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
	} else {
		val = 0xf << IMG_CROP_ERR_SHIFT;
		val |= 0xf << IMG_PIX_ERR_SHIFT;
		val |= 0xf << IMG_OVERFLOW_SHIFT;

		val |= 0xfff << IMG_BUS_ERR_SHIFT;

		kcsi_clr(kstream->core->base, IMG_INT_MSK1, val);

		val = 0xf << IMG_INT_SDW_SHIFT;
		kcsi_clr(kstream->core->base, IMG_INT_MSK0, val);

		val = 0xf << IMG_SHADOW_UPDATE_SHIFT;
		kcsi_set(kstream->core->base, IMG_REG_LOAD, val);
	}

	return 0;
}

static int kstream_disable_stream(struct kstream_device *kstream)
{
	struct v4l2_pix_format_mplane *pix_fmt;
	int pos, n;
	u32 ctrl = 0;
	u8 id = kstream->id;
	int j;

	pos = kstream_get_pix_fmt(kstream, &pix_fmt);

	if (pos < 0)
		return pos;

	for (n = 0; n < pix_fmt->num_planes && n < IMG_HW_NUM_PLANES; n++) {
		ctrl |= ((1 << n) << IMG_PIXEL_CROP_SHIFT);
		ctrl |= ((1 << n) << IMG_PIXEL_STREAM_EN_SHIFT);
	}

	if (kstream->core->sync == 0) {
		kcsi_clr(kstream->base, IMG_CHN_CTRL, ctrl);

		kcsi_clr(kstream->core->base, IMG_ENABLE,
			 (1 << id) << IMG_IMG_EN_SHIFT);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			kcsi_clr(kstream->base + IMG_REG_LEN * j, IMG_CHN_CTRL,
				 ctrl);
		}

		kcsi_clr(kstream->core->base, IMG_ENABLE,
			 (0xf) << IMG_IMG_EN_SHIFT);
	}

	return 0;
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

static int kstream_enable(struct kstream_device *kstream)
{
	struct kstream_video *video = &kstream->video;
	struct v4l2_subdev *if_sd = kstream->interface.subdev;
	int ret;

	if (kstream->state == RUNNING || kstream->state == IDLE)
		return 0;

	video->sequence = 0;

	kstream->field = 0;
	ret = kstream_setup_regs(kstream);

	if (ret < 0)
		return ret;

	ret = v4l2_subdev_call(if_sd, video, s_stream, 1);

	if (ret < 0)
		goto error;

	kstream_enable_irq_common(kstream);
	kstream_enable_stream(kstream);

	kstream->state = RUNNING;

	return 0;
 error:
	kstream->state = STOPPED;
	kstream_flush_all_buffers(kstream, VB2_BUF_STATE_ERROR);
	return ret;
}

static int kstream_disable(struct kstream_device *kstream)
{
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (kstream->state < RUNNING) {
		kstream->state = STOPPED;
		return 0;
	}

	kstream_disable_stream(kstream);

	kstream_disable_irq_common(kstream);

	v4l2_subdev_call(if_sd, video, s_stream, 0);

	kstream_flush_all_buffers(kstream, VB2_BUF_STATE_ERROR);
	kstream->state = STOPPED;
	return 0;
}

static void kstream_reset_device(struct kstream_device *kstream)
{
	int j;

	if (kstream->core->sync == 0)
		kcsi_set(kstream->core->base, IMG_ENABLE,
			 IMG_RST_(kstream->id));
	else
		for (j = 0; j < 4; j++)
			kcsi_set(kstream->core->base, IMG_ENABLE, 0xf << 4);


	usleep_range(2, 10);

	if (kstream->core->sync == 0) {
		kcsi_clr(kstream->core->base, IMG_ENABLE,
			 IMG_RST_(kstream->id));
	} else {
		kcsi_clr(kstream->core->base, IMG_ENABLE, 0xf << 4);
	}
}

static int kstream_set_power(struct v4l2_subdev *sd, int on)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);

	if (on)
		kstream_reset_device(kstream);

	return 0;
}

static int kstream_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	int ret;

	if (enable) {
		ret = kstream_enable(kstream);

		if (ret < 0)
			dev_err(kstream->dev,
				"Failed to enable kstream stream\n");
	} else {
		ret = kstream_disable(kstream);

		if (ret < 0)
			dev_err(kstream->dev,
				"Failed to disable kstream stream\n");
	}

	return ret;
}

static int kstream_get_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *fmt);
static int kstream_set_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *fmt);

static struct v4l2_mbus_framefmt *__kstream_get_format(struct kstream_device
						       *kstream,
						       struct
						       v4l2_subdev_pad_config
						       *cfg, unsigned int pad,
						       enum
						       v4l2_subdev_format_whence
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
	struct v4l2_rect *rect;
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

	case SDRV_IMG_PAD_SRC:
		*fmt =
		    *__kstream_get_format(kstream, cfg, SDRV_IMG_PAD_SINK,
					  which);

		rect = __kstream_get_crop(kstream, cfg, which);

		fmt->width = rect->width;

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

static int kstream_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

#if 0
	struct v4l2_mbus_framefmt *format;
	unsigned int i;

	if (!v4l2_subdev_call(if_sd, pad, enum_mbus_code, cfg, code)) {
		for (i = 0; i < ARRAY_SIZE(mbus_fmts); i++) {
			if (code->code == mbus_fmts[i].code)
				return 0;
		}

		return -EINVAL;
	}

	if (code->pad == SDRV_IMG_PAD_SINK) {
		if (code->index >= ARRAY_SIZE(mbus_fmts))
			return -EINVAL;

		code->code = mbus_fmts[code->index].code;
	} else {
		if (code->index > 0)
			return -EINVAL;

		format = __kstream_get_format(kstream, cfg, SDRV_IMG_PAD_SINK,
					      code->which);

		code->code = format->code;
	}

	return 0;
#else
	if (!v4l2_subdev_call(if_sd, pad, enum_mbus_code, cfg, code)) {
		return 0;
	}

	return -EINVAL;

#endif
}

static int kstream_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;
	struct v4l2_mbus_framefmt format;

	if (!v4l2_subdev_call(if_sd, pad, enum_frame_size, cfg, fse))
		return 0;

	if (fse->index != 0)
		return -EINVAL;

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	kstream_try_format(kstream, cfg, fse->pad, &format, fse->which);
	fse->min_width = format.width;
	fse->min_height = format.height;

	if (format.code != fse->code)
		return -EINVAL;

	format.code = fse->code;
	format.width = -1;
	format.height = -1;
	kstream_try_format(kstream, cfg, fse->pad, &format, fse->which);
	fse->max_width = format.width;
	fse->max_height = format.height;

	return 0;
}

static int kstream_set_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_selection *sel)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev_format fmt = { 0 };
	struct v4l2_rect *rect;
	int ret = 0;

	if (sel->target == V4L2_SEL_TGT_CROP && sel->pad == SDRV_IMG_PAD_SRC) {
		rect = __kstream_get_crop(kstream, cfg, sel->which);

		if (!rect)
			return -EINVAL;

		kstream_try_crop(kstream, cfg, &sel->r, sel->which);
		*rect = sel->r;

		fmt.which = sel->which;
		fmt.pad = SDRV_IMG_PAD_SRC;
		ret = kstream_get_format(sd, cfg, &fmt);

		if (ret < 0)
			return ret;

		fmt.format.width = rect->width;
		fmt.format.height = rect->height;
		ret = kstream_set_format(sd, cfg, &fmt);
	} else
		ret = -EINVAL;

	return ret;
}

static int kstream_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_selection *sel)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *fmt;
	struct v4l2_rect *rect;

	if (sel->pad == SDRV_IMG_PAD_SRC) {
		switch (sel->target) {
		case V4L2_SEL_TGT_CROP_BOUNDS:
			fmt =
			    __kstream_get_format(kstream, cfg, sel->pad,
						 sel->which);

			if (!fmt)
				return -EINVAL;

			sel->r.left = 0;
			sel->r.top = 0;
			sel->r.width = fmt->width;
			sel->r.height = fmt->height;
			break;

		case V4L2_SEL_TGT_CROP:
			rect = __kstream_get_crop(kstream, cfg, sel->which);

			if (!rect)
				return -EINVAL;

			sel->r = *rect;
			break;

		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int kstream_get_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *fmt)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;
	struct v4l2_mbus_framefmt *mbus_fmt, tmp_fmt;

	mbus_fmt = __kstream_get_format(kstream, cfg, fmt->pad, fmt->which);

	if (!mbus_fmt)
		return -EINVAL;

	if (kstream->state == INITING)
		return 0;

	if (fmt->pad == SDRV_IMG_PAD_SRC) {
		fmt->format = *mbus_fmt;
		return 0;
	}

	if (v4l2_subdev_call(if_sd, pad, get_fmt, cfg, fmt))
		return -EINVAL;

	if (!memcmp(mbus_fmt, &fmt->format, sizeof(*mbus_fmt)))
		return 0;

	tmp_fmt = fmt->format;

	if (kstream_try_format(kstream, cfg, fmt->pad,
			       &fmt->format, fmt->which))
		return -EINVAL;

	if (memcmp(&tmp_fmt, &fmt->format, sizeof(tmp_fmt)))
		return -EINVAL;

	return 0;
}

static int kstream_set_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *fmt)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;
	struct v4l2_mbus_framefmt *mbus_fmt, tmp_fmt;
	struct v4l2_subdev_selection sel = { 0 };
	int ret = 0;

	ret = kstream_try_format(kstream, cfg, fmt->pad,
				 &fmt->format, fmt->which);

	if (ret < 0)
		return ret;

	if (fmt->pad == SDRV_IMG_PAD_SINK && kstream->state != INITING) {
		if (v4l2_subdev_call(if_sd, pad, set_fmt, cfg, fmt))
			return -EINVAL;
	}

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

static int kstream_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum
				       *fie)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, pad, enum_frame_interval, cfg, fie);
}

static int kstream_get_pixelaspect(struct v4l2_subdev *sd,
				   struct v4l2_fract *aspect)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, g_pixelaspect, aspect);
}

static int kstream_get_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, g_input_status, status);
}

static int kstream_query_std(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, querystd, std);
}

static int kstream_get_std(struct v4l2_subdev *sd, v4l2_std_id *norm)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, g_std, norm);
}

static int kstream_set_std(struct v4l2_subdev *sd, v4l2_std_id norm)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, s_std, norm);
}

static int kstream_get_parm(struct v4l2_subdev *sd,
			    struct v4l2_streamparm *parm)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, g_parm, parm);
}

static int kstream_set_parm(struct v4l2_subdev *sd,
			    struct v4l2_streamparm *parm)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, s_parm, parm);
}

static int kstream_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, g_frame_interval, fi);
}

static int kstream_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct kstream_device *kstream = v4l2_get_subdevdata(sd);
	struct v4l2_subdev *if_sd = kstream->interface.subdev;

	if (if_sd == NULL)
		return -EINVAL;

	return v4l2_subdev_call(if_sd, video, s_frame_interval, fi);
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

static const struct v4l2_subdev_video_ops kstream_video_ops = {
	.s_stream = kstream_set_stream,
	.g_pixelaspect = kstream_get_pixelaspect,
	.g_input_status = kstream_get_input_status,
	.querystd = kstream_query_std,
	.g_std = kstream_get_std,
	.s_std = kstream_set_std,
	.g_parm = kstream_get_parm,
	.s_parm = kstream_set_parm,
	.g_frame_interval = kstream_g_frame_interval,
	.s_frame_interval = kstream_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops kstream_pad_ops = {
	.enum_mbus_code = kstream_enum_mbus_code,
	.enum_frame_size = kstream_enum_frame_size,
	.get_fmt = kstream_get_format,
	.set_fmt = kstream_set_format,
	.get_selection = kstream_get_selection,
	.set_selection = kstream_set_selection,
	.enum_frame_interval = kstream_enum_frame_interval,
};

static const struct v4l2_subdev_ops sdrv_stream_v4l2_ops = {
	.core = &kstream_core_ops,
	.video = &kstream_video_ops,
	.pad = &kstream_pad_ops,
};

static const struct v4l2_subdev_internal_ops sdrv_stream_internal_ops = {
	.open = kstream_init_formats,
};

static const struct media_entity_operations sdrv_stream_media_ops = {
	.link_setup = kstream_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static void sdrv_stream_frm_done_isr(struct kstream_device *kstream)
{
	struct kstream_video *video = &kstream->video;
	struct vb2_v4l2_buffer *vbuf;
	unsigned long flags;
	struct v4l2_mbus_framefmt *in_fmt;
	struct kstream_buffer *kbuf;
	unsigned int i;

	in_fmt = &kstream->mbus_fmt[SDRV_IMG_PAD_SINK];
	if (video->vbuf_active == NULL) {
		if (kstream->state == RUNNING)
			pr_err("wrong state machine! Maybe app can't get the data in time.\n");
			//WARN_ON(video->vbuf_active == NULL);
		return;
	}
	if (V4L2_FIELD_HAS_BOTH(in_fmt->field)) {
		if (kstream->field == 0)
			return;
	}
	spin_lock_irqsave(&video->buf_lock, flags);
	vbuf = video->vbuf_active;

#if (IS_ENABLED(CONFIG_ARM64))
	/*
	 * When user IO is V4L2_MEMORY_MMAP and driver queue ops is
	 * dma-contig, video core won't invalidate cache for us, as we're
	 * assumed to be a dma-incoherent device. In this case we must
	 * assure cache consistency before getting buffer ready.
	 */
	if (is_device_dma_coherent(video->dev)) {
		/* mark incoherent to let arch do right cache ops */
		video->dev->archdata.dma_coherent = false;

		kbuf = container_of(vbuf, struct kstream_buffer, vbuf);
		for (i = 0; i < vbuf->vb2_buf.num_planes; i++) {
			dma_sync_single_for_cpu(video->dev, kbuf->paddr[i],
					vbuf->vb2_buf.planes[i].length,
					DMA_FROM_DEVICE);
		}

		/* restore to map page cacheable */
		video->dev->archdata.dma_coherent = true;
	}
#endif

	vbuf->sequence = video->sequence++;
	vbuf->vb2_buf.timestamp = ktime_get_ns();
	vb2_buffer_done(&vbuf->vb2_buf, VB2_BUF_STATE_DONE);
	video->vbuf_active = NULL;
	spin_unlock_irqrestore(&video->buf_lock, flags);
}

static void sdrv_stream_img_update_isr(struct kstream_device *kstream)
{
	struct kstream_video *video = &kstream->video;

	WARN_ON(video->vbuf_ready == NULL);

	kstream_set_baddr(kstream);
}

static void sdrv_stream_init_interface(struct kstream_device *kstream)
{
	u32 val;
	int j;

	val = kcsi_readl(kstream->base, IMG_CTRL);
	val &= ~IMG_INTERFACE_MASK;

	switch (kstream->interface.mbus_type) {
	case V4L2_MBUS_PARALLEL:
	case SDRV_MBUS_DC2CSI1:
		val |= IMG_PARALLEL_SEL;
		break;

	case V4L2_MBUS_CSI2:
	case SDRV_MBUS_DC2CSI2:
		val |= IMG_MIMG_CSI2_SEL;
		break;

	case V4L2_MBUS_BT656:
		val |= IMG_BT656_SEL;
		break;

	default:
		val |= IMG_PARALLEL_SEL_OTHER;
		break;
	}

	if (kstream->core->sync == 0) {
		kcsi_writel(kstream->base, IMG_CTRL, val);
	} else {
		for (j = 0; j < IMG_CNT; j++) {
			kcsi_writel(kstream->base + IMG_REG_LEN * j, IMG_CTRL,
				    val);
		}
	}

}

static struct kstream_ops sdrv_stream_ops = {
	.init_interface = sdrv_stream_init_interface,
	.frm_done_isr = sdrv_stream_frm_done_isr,
	.img_update_isr = sdrv_stream_img_update_isr,
};

static int kstream_init_device(struct kstream_device *kstream)
{
	kstream->ops = &sdrv_stream_ops;
	kstream->state = INITING;

	return kstream_init_formats(&kstream->subdev, NULL);
}

int sdrv_stream_register_entities(struct kstream_device *kstream,
				  struct v4l2_device *v4l2_dev)
{
	struct v4l2_subdev *sd = &kstream->subdev;
	struct media_pad *pads = kstream->pads;
	struct device *dev = kstream->dev;
	struct kstream_video *video = &kstream->video;
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
	ret = sdrv_stream_video_register(video, v4l2_dev);

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
	sdrv_stream_video_unregister(video);
 err_reg_video:
	v4l2_device_unregister_subdev(sd);
 err_reg_subdev:
	media_entity_cleanup(&sd->entity);
	return ret;
}

int sdrv_stream_unregister_entities(struct kstream_device *kstream)
{
	struct v4l2_subdev *sd = &kstream->subdev;

	kstream_disable(kstream);

	sdrv_stream_video_unregister(&kstream->video);

	v4l2_device_unregister_subdev(sd);

	media_entity_cleanup(&sd->entity);

	return 0;
}
