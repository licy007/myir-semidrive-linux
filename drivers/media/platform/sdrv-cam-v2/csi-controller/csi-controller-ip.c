/*
 * csi-controller-ip.c
 *
 * Semidrive platform csi ip driver core file
 *
 * Copyright (C) 2022, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include "sdrv-cam-os-def.h"
#include "csi-controller-hwreg-v1.h"
#include "csi-controller-ip.h"
#include "cam_isr_log.h"

#define REMAPPING_WORDS ((64 * 6 + 31) / 32)
#define IMG_STREAM_NUM 2
#define DEFAULT_SKIP_FRAMES 1

#define IMG_MAX_WIDTH   ((1 << 16) - 1)
#define IMG_MAX_HEIGHT  ((1 << 16) - 1)

#define FIELD_HAS_BOTH(field) (field == CSI_FIELD_INTERLACED)

#define CSI_IMG_ERR_RETURN(csi_dev, img_id, param) \
	do { \
		if ((csi_dev == NULL) || (img_id >= CSI_IMG_NUM) || (param == NULL)) { \
			csi_err("%s: failed to get csi_dev %p, img_id %d, param %p\n", \
				__func__, csi_dev, img_id, param); \
			return -1; \
		} \
	} while(0);

enum csi_ip_version {
	CSI_V1 = 0,
	CSI_V2,
};

enum frame_buffer_mode {
	FB_MODE_NORMAL = 0,
	FB_MODE_ROUND,
};

/* Please define CSI_V1 for Kunlun, CSI_V2 for Taishan(E3) */
#define CONFIG_CSI_VERSION CSI_V1

static int s_test_cnt;
int is_addr_64bit = (sizeof(dma_addr_t) / 8);

enum image_status {
	IMG_STATUS_IDLE = 0,
	IMG_STATUS_STREAM_ON,
	IMG_STATUS_MAX,
};

int csi_cfg_img_buf(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_buf *buf);

struct csi_core_internal;

struct csi_format_pack {
	uint32_t output_fmt;
	uint32_t bus_fmt;
	uint8_t is_parallel;
	uint8_t bpp[3];
	uint8_t planes;
	uint8_t fc_dualline;
	uint8_t valid_pix_odd;  /* valid pixel count in one pack(48 bits) for odd line */
	uint8_t valid_pix_even; /* valid pixel count in one pack(48 bits) for even line */
	uint8_t pack_cycle_odd;  /* parallel: package how many bytes(one byte per cycle) in one pack(48 bits) for odd line */
	uint8_t pack_cycle_even; /* parallel: package how many bytes(one byte per cycle) in one pack(48 bits) for even line */
	uint8_t pack_cycle_uv_odd;  /* parallel: U/V exists in pack for odd line */
	uint8_t pack_cycle_uv_even; /* parallel: U/V exists in pack for even line */
	uint32_t split[2]; /* data_sel, [0] for stream0(plane1), [1] for stream1(plane2) */
	uint32_t pack[2];  /* color_depth, [0] for stream0(plane1), [1] for stream1(plane2) */
	const uint32_t *bitmap;  /* parallel: remapping pack(48bits) to 64bits( 4-ch x 16bits) */
};

struct csi_img_err_cnt {
	uint32_t bt_err;
	uint32_t bt_fatal;
	uint32_t crop_err;
	uint32_t pix_err;
	uint32_t overflow;
	uint32_t stream0_err;
	uint32_t stream1_err;
	uint32_t stream2_err;
};

struct csi_img_internal {
	uint32_t id;
	uint32_t frm_cnt;
	uint32_t bus_type; /* should be enum csi_bus_type */
	uint32_t bus_fmt;  /* should be enum csi_mbus_fmt */
	uint32_t bus_flag;
	uint32_t output_fmt;  /* should be enum csi_out_fmt */
	uint32_t field;
	uint32_t second_field;
	uint32_t skip_frame;
	uint32_t stream_start;
	uint32_t skip_shadow;
	uint32_t status;
	spinlock_t status_lock;
	struct csi_img_err_cnt err_cnt;
	const struct csi_format_pack *pfmt;
	struct csi_img_buf next_buf;
	struct csi_img_size size[2];  /* [0] for frame and odd field, [1] for even field */
	struct csi_img_crop crop[2];  /* [0] for frame and odd field, [1] for even field */
	uint32_t crop_update;
	spinlock_t crop_lock;

	uint32_t fb_mode;
	struct mem_range round_mem[3];

	struct csi_core_internal *core;
	struct csi_callback_t irq_cb;
};

struct csi_core_internal {
	uint32_t host_id;
	uint32_t inited;
	uint32_t sync;
	uint32_t sync_irq;
	uint32_t ip_version;
	reg_addr_t base;
	spinlock_t glb_reg_lock;
	struct csi_hw_dev ex_dev;
	struct csi_img_internal images[CSI_IMG_NUM];
};

struct wdma_fifo_t {
	uint32_t dfifo;
	uint32_t cfifo;
};

static struct csi_core_internal csi_cores[CSI_HOST_NUM] = {
	{
		.host_id = CSI_HOST_0,
		.ip_version = CONFIG_CSI_VERSION,
		.ex_dev = {
			.host_id = CSI_HOST_0,
		},
	},
	{
		.host_id = CSI_HOST_1,
		.ip_version = CONFIG_CSI_VERSION,
		.ex_dev = {
			.host_id = CSI_HOST_1,
		},
	},
	{
		.host_id = CSI_HOST_2,
		.ip_version = CONFIG_CSI_VERSION,
		.ex_dev = {
			.host_id = CSI_HOST_2,
		},
	},
};


/*
 * formats pack for mipi-csi input
 */
const struct csi_format_pack csi_mipi_fmts[] = {
	/* idx = 0 */
	{
		.output_fmt = CSI_FMT_UYVY,
		.bus_fmt = CSI_MBUS_UYVY8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,   /* 1: 2 pixels one pack, 0: 1 pixels one pack */
		.valid_pix_even = 1,
		.split = { 0x1A },    /* 3bits-per-sec: 0 | 0 | 3 | 2, */
		.pack = { 0x108 },    /* 5bits-per-sec: 0 | 0 | 8 | 8, */
	},
	/* idx = 1 */
	{
		.output_fmt = CSI_FMT_YUYV,
		.bus_fmt = CSI_MBUS_UYVY8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.split = { 0x13 },    /* 3bits-per-sec: 0 | 0 | 2 | 3, */
		.pack = { 0x108 },    /* 5bits-per-sec: 0 | 0 | 8 | 8, */
	},
	/* idx = 2 */
	{
		.output_fmt = CSI_FMT_YUYV,
		.bus_fmt = CSI_MBUS_YUYV8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,   /* 1: 2 pixels one pack, 0: 1 pixels one pack */
		.valid_pix_even = 1,
		.split = { 0x1A },	  /* 3bits-per-sec: 0 | 0 | 3 | 2, */
		.pack = { 0x108 },	  /* 5bits-per-sec: 0 | 0 | 8 | 8, */
	},
	/* idx = 3 */
	{
		.output_fmt = CSI_FMT_UYVY,
		.bus_fmt = CSI_MBUS_YUYV8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.split = { 0x13 },	  /* 3bits-per-sec: 0 | 0 | 2 | 3, */
		.pack = { 0x108 },	  /* 5bits-per-sec: 0 | 0 | 8 | 8, */
	},
	/* idx = 4 */
	{
		.output_fmt = CSI_FMT_UYVY,
		.bus_fmt = CSI_MBUS_UYVY10_2X10,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.split = { 0x1A },    /* 3bits-per-sec: 0 | 0 | 3 | 2, */
		.pack = { 0x108 },    /* 5bits-per-sec: 0 | 0 | 8 | 8, */
	},
	/* idx = 5 */
	{
		.output_fmt = CSI_FMT_YUYV,
		.bus_fmt = CSI_MBUS_UYVY10_2X10,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.split = { 0x13 },    /* 3bits-per-sec: 0 | 0 | 2 | 3, */
		.pack = { 0x108 },    /* 5bits-per-sec: 0 | 0 | 8 | 8, */
	},
	/* idx = 6 */
	{
		.output_fmt = CSI_FMT_RGB24,
		.bus_fmt = CSI_MBUS_RGB888_1X24,
		.bpp = { 24 },
		.planes = 1,
		.valid_pix_odd = 0,
		.valid_pix_even = 0,
		.split = { 0xE53 },     /* 3bits-per-sec: x | 1 | 2 | 3, x=16bit(0) */
		.pack = { 0x2108 },     /* 5bits-per-sec: 0 | 8 | 8 | 8, */
	},
	/* idx = 7 */
	{
		.output_fmt = CSI_FMT_BGR24,
		.bus_fmt = CSI_MBUS_RGB888_1X24,
		.bpp = { 24 },
		.planes = 1,
		.valid_pix_odd = 0,   /* 0: 1 pixels one pack */
		.valid_pix_even = 0,
		.split = { 0xED1 },     /* 3bits-per-sec: x | 3 | 2 | 1, x=16bit(0) */
		.pack = { 0x2108 },     /* 5bits-per-sec: 0 | 8 | 8 | 8, */
	},
};

/*
 * bit 00-15: 4 5 6 7 8 9 0 1,  2 3 4 5 6 7 8 9
 * bit 16-31: 14 15 16 17 18 19 10 11,  12 13 14 15 16 17 18 19
 * bit 32-47: 24 25 26 27 28 29 20 21,  22 23 24 25 26 27 28 29
 * bit 48-63: 34 35 36 37 38 39 30 31,  32 33 34 35 36 37 38 39
 */
const uint32_t mapping_default[REMAPPING_WORDS] = {
	0x481c6144, 0x40c20402, 0x2481c614, 0xd24503ce,
	0xe34c2ca4, 0x4d24503c, 0x5c6da658, 0x85d65547,
	0x75c6da65, 0xe69648e2, 0x28607de9, 0x9e69648e
};

/*
 * formats pack for parallel input
 */
const struct csi_format_pack csi_parallel_fmts[] = {
	/* idx = 0 */
	{
		.output_fmt = CSI_FMT_UYVY,
		.bus_fmt = CSI_MBUS_UYVY8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.pack_cycle_odd = 3,   /* 3: pack 4bytes in 48bits pack for odd line */
		.pack_cycle_even = 3,  /* 3: pack 4bytes in 48bits pack for even line */
		.pack_cycle_uv_odd = 3,
		.pack_cycle_uv_even = 3,
		.split = { 0x53 },     /* 3bits-per-sec: 0 | 1 | 2 | 3, */
		.pack = { 0x42108, 0x3F },   /* 5bits-per-sec: 8 | 8 | 8 | 8, */
		.bitmap = mapping_default,
	},
	/* idx = 1 */
	{
		.output_fmt = CSI_FMT_YUYV,
		.bus_fmt = CSI_MBUS_UYVY8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.pack_cycle_odd = 3,   /* 3: pack 4bytes in 48bits pack for odd line */
		.pack_cycle_even = 3,  /* 3: pack 4bytes in 48bits pack for even line */
		.pack_cycle_uv_odd = 3,
		.pack_cycle_uv_even = 3,
		.split = { 0x21A },    /* 3bits-per-sec: 1 | 0 | 3 | 2, */
		.pack = { 0x42108, 0x3F },   /* 5bits-per-sec: 8 | 8 | 8 | 8, */
		.bitmap = mapping_default,
	},
	/* idx = 2 */
	{
		.output_fmt = CSI_FMT_UYVY,
		.bus_fmt = CSI_MBUS_YUYV8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.pack_cycle_odd = 3,   /* 3: pack 4bytes in 48bits pack for odd line */
		.pack_cycle_even = 3,  /* 3: pack 4bytes in 48bits pack for even line */
		.pack_cycle_uv_odd = 3,
		.pack_cycle_uv_even = 3,
		.split = { 0x21A },    /* 3bits-per-sec: 1 | 0 | 3 | 2, */
		.pack = { 0x42108 },   /* 5bits-per-sec: 8 | 8 | 8 | 8, */
		.bitmap = mapping_default,
	},
	/* idx = 3 */
	{
		.output_fmt = CSI_FMT_YUYV,
		.bus_fmt = CSI_MBUS_YUYV8_2X8,
		.bpp = { 16 },
		.planes = 1,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.pack_cycle_odd = 3,   /* 3: pack 4bytes in 48bits pack for odd line */
		.pack_cycle_even = 3,  /* 3: pack 4bytes in 48bits pack for even line */
		.pack_cycle_uv_odd = 3,
		.pack_cycle_uv_even = 3,
		.split = { 0x53 },     /* 3bits-per-sec: 0 | 1 | 2 | 3, */
		.pack = { 0x42108 },   /* 5bits-per-sec: 8 | 8 | 8 | 8, */
		.bitmap = mapping_default,
	},
	/* idx = 4 */
	{
		.output_fmt = CSI_FMT_RGB24,
		.bus_fmt = CSI_MBUS_RGB888_1X24,
		.bpp = { 24 },
		.planes = 1,
		.valid_pix_odd = 0,
		.valid_pix_even = 0,
		.pack_cycle_odd = 2,   /* 2: pack 3bytes in 48bits pack for odd line */
		.pack_cycle_even = 2,  /* 2: pack 3bytes in 48bits pack for even line */
		.split = { 0xE53 },     /* 3bits-per-sec: x | 1 | 2 | 3, x=16bit(0) */
		.pack = { 0x2108 },     /* 5bits-per-sec: 0 | 8 | 8 | 8, */
		.bitmap = mapping_default,
	},
	/* idx = 5 */
	{
		.output_fmt = CSI_FMT_BGR24,
		.bus_fmt = CSI_MBUS_RGB888_1X24,
		.bpp = { 24 },
		.planes = 1,
		.valid_pix_odd = 0,
		.valid_pix_even = 0,
		.pack_cycle_odd = 2,   /* 2: pack 3bytes in 48bits pack for odd line */
		.pack_cycle_even = 2,  /* 2: pack 3bytes in 48bits pack for even line */
		.split = { 0xED1 },     /* 3bits-per-sec: x | 3 | 2 | 1, x=16bit(0) */
		.pack = { 0x2108 },     /* 5bits-per-sec: 0 | 8 | 8 | 8, */
		.bitmap = mapping_default,
	},
	/* idx = 6 */
	{
		.output_fmt = CSI_FMT_YUV422SP,
		.bus_fmt = CSI_MBUS_UYVY8_2X8,
		.bpp = { 8, 8 },
		.planes = 2,
		.valid_pix_odd = 1,
		.valid_pix_even = 1,
		.pack_cycle_odd = 3,   /* 3: pack 4bytes in 48bits pack for odd line */
		.pack_cycle_even = 3,  /* 3: pack 4bytes in 48bits pack for even line */
		.pack_cycle_uv_odd = 3,
		.pack_cycle_uv_even = 3,
		.split = { 0x02, 0x23 },   /* 3bits-per-sec: 0 | 0 | 0 | 2,  0 | 0 | 1 | 3 */
		.pack = { 0x108, 0x108 },  /* 5bits-per-sec: 0 | 0 | 8 | 8,  0 | 0 | 8 | 8 */
		.bitmap = mapping_default,
	},
};


static inline void csi_set(struct csi_core_internal *csi_core,
		uint32_t addr, uint32_t mask, uint32_t set_val)
{
	unsigned long flags;

	spin_lock_irqsave(&csi_core->glb_reg_lock, flags);
	reg_write(csi_core->base, addr, (reg_read(csi_core->base, addr) & ~mask) | set_val);
	spin_unlock_irqrestore(&csi_core->glb_reg_lock, flags);
}

/* set reg to default value */
static void csi_img_set_default(struct csi_img_internal *img)
{
	struct csi_core_internal *csi_core = img->core;
	int i, n = img->id;

	reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_H(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_L(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_U_BADDR_H(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_U_BADDR_L(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_V_BADDR_H(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_V_BADDR_L(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_RGBY_STRIDE(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_U_STRIDE(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_V_STRIDE(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_SIZE(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_IPI_CTRL(n), 0x4000);
	reg_write(csi_core->base, CSI_REG_IMG_IF_PIXEL_MASK0(n), 0xFFFFFFFF);
	reg_write(csi_core->base, CSI_REG_IMG_IF_PIXEL_MASK1(n), 0xFFFFFFFF);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_CTRL(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_SPLIT0(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_SPLIT1(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_CROP0(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_CROP1(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_PACK0(n), 0x00);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_PACK1(n), 0x00);

	i = n * 2;
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_DFIFO(i), 0x00400008);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_CFIFO(i), 0x04);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL0(i), 0x3C8);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL1(i), 0x00);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL2(i), 0x0B);
	i++;
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_DFIFO(i), 0x00400008);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_CFIFO(i), 0x04);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL0(i), 0x3C8);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL1(i), 0x00);
	reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL2(i), 0x0B);

	reg_write(csi_core->base, CSI_REG_PARA_BT_CTRL0(n), 0x00);
	reg_write(csi_core->base, CSI_REG_PARA_BT_CTRL1(n), 0x00);
	reg_write(csi_core->base, CSI_REG_PARA_BT_CTRL2(n), 0x00);

	if (csi_core->ip_version < CSI_V2)
		return;

	reg_write(csi_core->base, CSI_REG_IMG_FB_CTRL(n), 0x00);
}

/* set ipi ctrl, channel ctrl */
static int csi_img_set_channel(struct csi_img_internal *img)
{
	struct csi_core_internal *csi_core = img->core;
	const struct csi_format_pack *fmt = img->pfmt;
	int i, n, stream_num;
	uint32_t chn_ctrl, ipi_ctrl;

	n = img->id;
	stream_num = (fmt->planes > IMG_STREAM_NUM) ? IMG_STREAM_NUM : fmt->planes;

	ipi_ctrl = reg_read(csi_core->base, CSI_REG_IMG_IPI_CTRL(n));
	ipi_ctrl &= ~(0x7);
	ipi_ctrl |= (img->bus_type & 7);

	if (img->bus_type == CSI_BUS_MIPICSI2) {
		switch (fmt->bus_fmt) {
		case CSI_MBUS_UYVY8_2X8:
		case CSI_MBUS_YUYV8_2X8:
		case CSI_MBUS_UYVY10_2X10:
			ipi_ctrl |= (1 << 6);
			break;
		case CSI_MBUS_UYVY8_1_5X8:
			ipi_ctrl |= (1 << 4);
			break;
		case CSI_MBUS_VYUY8_1_5X8:
			ipi_ctrl |= (1 << 3);
			break;
		default:
			break;
		}
	} else {
		ipi_ctrl &= ~(0x1F << 9);
		ipi_ctrl |= ((fmt->fc_dualline & 1) << 13);
		ipi_ctrl |= ((fmt->valid_pix_even & 3) << 11) | ((fmt->valid_pix_odd & 3) << 9);
	}

	s_test_cnt++;
	//img->skip_frame = (s_test_cnt>>3)&3;
	csi_err("s_test_cnt %d, skip_frame: %d\n", s_test_cnt, img->skip_frame);

	/* vsync_mask_count */
	ipi_ctrl &= ~(3 << 7);
	ipi_ctrl |= ((img->skip_frame & 3) << 7);

	reg_write(csi_core->base, CSI_REG_IMG_IPI_CTRL(n), ipi_ctrl);

	/* set channel ctrl: stream enable */
	chn_ctrl = reg_read(csi_core->base, CSI_REG_IMG_CHN_CTRL(n));
	chn_ctrl &= ~(0x7);
	chn_ctrl |= ((1 << stream_num) - 1);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_CTRL(n), chn_ctrl);

	for (i = 0; i < stream_num; i++) {
		int split[4], color_depth[4];
		split[0] = (fmt->split[i] & 0x7);
		split[1] = ((fmt->split[i] >> 3) & 0x7);
		split[2] = ((fmt->split[i] >> 6) & 0x7);
		split[3] = ((fmt->split[i] >> 9) & 0x7);
		color_depth[0] = (fmt->pack[i] & 0x1F);
		color_depth[1] = ((fmt->pack[i] >> 5) & 0x1F);
		color_depth[2] = ((fmt->pack[i] >> 10) & 0x1F);
		color_depth[3] = ((fmt->pack[i] >> 15) & 0x1F);

		csi_info("%s: pix_fmt: 0x%08x, bus_type 0x%x\n",
			__func__, fmt->output_fmt, fmt->bus_fmt);
		csi_info("plane.%d, split= [%d, %d, %d, %d], color_depth = [%d, %d, %d, %d]\n",
			i, split[3], split[2], split[1], split[0],
			color_depth[3], color_depth[2], color_depth[1], color_depth[0]);

		reg_write(csi_core->base, CSI_REG_IMG_CHN_SPLIT0(n) + i * 4, fmt->split[i]);
		reg_write(csi_core->base, CSI_REG_IMG_CHN_PACK0(n) + i * 4,  fmt->pack[i]);
	}

	return 0;
}

/* set crop */
static int csi_img_set_crop(struct csi_img_internal *img)
{
	struct csi_core_internal *csi_core = img->core;
	const struct csi_format_pack *fmt = img->pfmt;
	int i, n, stream_num;
	uint32_t crop_en, start, length, val;
	uint32_t chn_ctrl;

	n = img->id;
	stream_num = (fmt->planes > IMG_STREAM_NUM) ? IMG_STREAM_NUM : fmt->planes;

	/* set hcrop: width crop */
	crop_en = (img->crop[0].w == img->size[0].w) ? 0 : 1;
	if (!crop_en) {
		csi_set(csi_core, CSI_REG_IMG_CHN_CTRL(n), 0x38, 0);
		goto set_vcrop;
	}

	csi_info("%s: csi%d img%d width crop enable\n", __func__, csi_core->host_id, img->id);
	chn_ctrl = reg_read(csi_core->base, CSI_REG_IMG_CHN_CTRL(n));
	chn_ctrl &= ~(0x38); /* bit[5:3] */
	crop_en = (1 << stream_num) - 1;
	chn_ctrl |= (crop_en << 3);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_CTRL(n), chn_ctrl);

	for (i = 0; i < stream_num; i++) {
		start = img->crop[0].x / (fmt->valid_pix_odd + 1) - 1;
		length = img->crop[0].w / (fmt->valid_pix_odd + 1);
		val = ((length & 0xFFFF) << 16) | (start & 0xFFFF);
		reg_write(csi_core->base, CSI_REG_IMG_CHN_CROP0(n) + i * 4, val);
	}

set_vcrop:
	/* height crop is not supported by Kunlun */
	if (csi_core->ip_version < CSI_V2)
		return 0;

	/* set vcrop: height crop, height crop is supported by Taishan(E3) */
	crop_en = (img->crop[0].h == img->size[0].h) ? 0 : 1;
	if (!crop_en) {
		csi_set(csi_core, CSI_REG_IMG_CHN_CTRL(n), 0x1C00, 0);
		return 0;
	}

	csi_info("%s: csi%d img%d height crop enable\n", __func__, csi_core->host_id, img->id);
	chn_ctrl = reg_read(csi_core->base, CSI_REG_IMG_CHN_CTRL(n));
	chn_ctrl &= ~(0x1C00); /* bit[12:10] */
	crop_en = (1 << stream_num) - 1;
	chn_ctrl |= (crop_en << 10);
	reg_write(csi_core->base, CSI_REG_IMG_CHN_CTRL(n), chn_ctrl);

	for (i = 0; i < stream_num; i++) {
		start = img->crop[0].y;
		length = img->crop[0].h;
		val = ((length & 0xFFFF) << 16) | (start & 0xFFFF);
		reg_write(csi_core->base, CSI_REG_IMG_CHN_VCROP0(n) + i * 4, val);
	}

	return 0;
}

/* set size, stride */
static int csi_img_set_size(struct csi_img_internal *img)
{
	struct csi_core_internal *csi_core = img->core;
	const struct csi_format_pack *fmt = img->pfmt;
	int i, n = img->id;
	uint32_t width, height, out_width, stride;

	/* set size */
	if (img->bus_type == CSI_BUS_MIPICSI2)
		width = img->size[0].w - 1;
	else
		width = img->size[0].w - (1 + fmt->valid_pix_odd);

	height = img->size[0].h - 1;

	reg_write(csi_core->base, CSI_REG_IMG_SIZE(n), ((width & 0xFFFF) << 16) | (height & 0xFFFF));

	/* set stride */
	out_width = img->crop[0].w;
	for (i = 0; i < fmt->planes; i++) {
		if (FIELD_HAS_BOTH(img->field))
			stride = 2 * out_width * fmt->bpp[i] / 8;
		else
			stride = out_width * fmt->bpp[i] / 8;
		reg_write(csi_core->base, CSI_REG_IMG_RGBY_STRIDE(n) + i * 4, stride);
	}

	csi_img_set_crop(img);

	return 0;
}

static int csi_img_set_wdma(struct csi_img_internal *img)
{
	struct csi_core_internal *csi_core = img->core;
	int i, k, n;
	uint32_t axi0 = 0x3C8;
	uint32_t axi1 = 0x00;
	uint32_t axi2 = 0x0B;
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

	k = (img->pfmt->planes == 1) ? 0 : 1;
	n = img->id * 2;
	for (i = 0; i < IMG_STREAM_NUM; i++, n++) {
		reg_write(csi_core->base, CSI_REG_WDMA_CHN_DFIFO(n), wfifo[k][i].dfifo);
		reg_write(csi_core->base, CSI_REG_WDMA_CHN_CFIFO(n), wfifo[k][i].cfifo);
		reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL0(n), axi0);
		reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL1(n), axi1);
		reg_write(csi_core->base, CSI_REG_WDMA_CHN_AXI_CTRL2(n), axi2);
	}

	return 0;
}

static int csi_img_set_para_bt(struct csi_img_internal *img)
{
	struct csi_core_internal *csi_core = img->core;
	int i;
	uint32_t value0 = 0, value1 = 0, value2 = 0, height;
	const struct csi_format_pack *fmt = img->pfmt;
	const uint32_t *map = fmt->bitmap;

	if (img->bus_type == CSI_BUS_MIPICSI2) {
		reg_write(csi_core->base, CSI_REG_PARA_BT_CTRL0(img->id), value0);
		return 0;
	}

	value0 |= (1 << 0) |
			((fmt->pack_cycle_odd & 3) << 1) |
			((fmt->pack_cycle_even & 3) << 3) |
			((fmt->pack_cycle_uv_odd & 3) << 6) |
			((fmt->pack_cycle_uv_even & 3) << 8);

	/* data enable pol */
	if (img->bus_flag & MBUS_DE_ACTIVE_LOW)
		value0 |= (1 << 10);

	/* vsync pol */
	if (img->bus_flag & MBUS_VSYNC_ACTIVE_LOW)
		value0 |= (1 << 11);

	/* hsync pol */
	if (img->bus_flag & MBUS_HSYNC_ACTIVE_HIGH)
		value0 |= (1 << 12);

	/* pclk pol */
	if (img->bus_flag & MBUS_PCLK_SAMPLE_RISING)
		value0 |= (1 << 13);

	if (img->bus_type == CSI_BUS_BT656 || img->bus_type == CSI_BUS_BT1120_SDR) {
		if (FIELD_HAS_BOTH(img->field)) {
			height = img->size[1].h - 1;
			value0 |= (1 << 14) | (height << 15);
			value1 = (1 << 2);
		} else {
			value1 = (1 << 0) | (1 << 2) | (img->size[0].w << 16);
		}
	}

	if (FIELD_HAS_BOTH(img->field))
		value2 = 0;
	else
		value2 = (1 << 17);

	reg_write(csi_core->base, CSI_REG_PARA_BT_CTRL0(img->id), value0);
	reg_write(csi_core->base, CSI_REG_PARA_BT_CTRL1(img->id), value1);
	reg_write(csi_core->base, CSI_REG_PARA_BT_CTRL2(img->id), value2);

	for (i = 0; i < REMAPPING_WORDS; i++)
		reg_write(csi_core->base, CSI_REG_PIXEL_MAP(img->id) + i * 4, map[i]);

	if (1) {
		uint32_t imap[3], bit_map[16];
		int j;
		for (j = 0; j < 4; j++) {
			imap[0] = map[j * 3 + 0];
			imap[1] = map[j * 3 + 1];
			imap[2] = map[j * 3 + 2];
			bit_map[0] = ((imap[0] >> 0) & 0x3F);
			bit_map[1] = ((imap[0] >> 6) & 0x3F);
			bit_map[2] = ((imap[0] >> 12) & 0x3F);
			bit_map[3] = ((imap[0] >> 18) & 0x3F);
			bit_map[4] = ((imap[0] >> 24) & 0x3F);
			bit_map[5] = ((imap[0] >> 30) & 0x3);
			bit_map[5] |= ((imap[1] << 2) & 0x3C);
			bit_map[6] = ((imap[1] >> 4) & 0x3F);
			bit_map[7] = ((imap[1] >> 10) & 0x3F);
			bit_map[8] = ((imap[1] >> 16) & 0x3F);
			bit_map[9] = ((imap[1] >> 22) & 0x3F);
			bit_map[10] = ((imap[1] >> 28) & 0xF);
			bit_map[10] |= ((imap[2] << 4) & 0x30);
			bit_map[11] = ((imap[2] >> 2) & 0x3F);
			bit_map[12] = ((imap[2] >> 8) & 0x3F);
			bit_map[13] = ((imap[2] >> 14) & 0x3F);
			bit_map[14] = ((imap[2] >> 20) & 0x3F);
			bit_map[15] = ((imap[2] >> 26) & 0x3F);
			csi_info("map idx %d, data %d %d %d %d %d %d %d %d,  %d %d %d %d %d %d %d %d\n", j * 16,
				bit_map[0], bit_map[1], bit_map[2], bit_map[3], bit_map[4], bit_map[5], bit_map[6], bit_map[7],
				bit_map[8], bit_map[9], bit_map[10], bit_map[11], bit_map[12], bit_map[13], bit_map[14], bit_map[15]);
		}
	}

	return 0;
}

static int csi_img_set_memrange(struct csi_img_internal *img)
{
	struct csi_core_internal *csi_core = img->core;
	int n = img->id;
	uint32_t mask = (1 << 28) - 1;

	if (csi_core->ip_version < CSI_V2)
		return 0;

	n = img->id;
	reg_write(csi_core->base, CSI_REG_IMG_FB_CTRL(n), (img->fb_mode & 1));
	if (img->fb_mode == FB_MODE_NORMAL)
		return 0;

	reg_write(csi_core->base, CSI_REG_IMG_UP_RGBY_ADDR(n),  (img->round_mem[0].up & mask));
	reg_write(csi_core->base, CSI_REG_IMG_LOW_RGBY_ADDR(n), (img->round_mem[0].low & mask));
	reg_write(csi_core->base, CSI_REG_IMG_UP_U_ADDR(n),  (img->round_mem[1].up & mask));
	reg_write(csi_core->base, CSI_REG_IMG_LOW_U_ADDR(n), (img->round_mem[1].low & mask));
	reg_write(csi_core->base, CSI_REG_IMG_UP_V_ADDR(n),  (img->round_mem[2].up & mask));
	reg_write(csi_core->base, CSI_REG_IMG_LOW_V_ADDR(n), (img->round_mem[2].low & mask));

	return 0;
}

/* set img 1~3 buffer address base on buffer of img0 for sync mode */
static void csi_img_set_sync_buf(struct csi_img_internal *img, struct csi_img_buf *buf)
{
	int i, n = img->id;
	struct csi_core_internal *csi_core = img->core;
	const struct csi_format_pack *fmt = img->pfmt;
	dma_addr_t paddr[3];
	uint32_t *ptr_u32;
	uint32_t addr_l[3], addr_h[3], stride[3], height[3], offset[3];

	for (i = 0; i < (int)fmt->planes; i++) {
		stride[i] = reg_read(csi_core->base, CSI_REG_IMG_RGBY_STRIDE(n) + i * 4);
		height[i] = img->size[0].h; /* todo: U/V height may be 1/2, such as YUV420 */
		offset[i] = n * stride[i] * height[i];
		paddr[i] = buf->paddr[i] + offset[i];

		// check addr: shouldn't IS_ERR_OR_NULL
		if (IS_ERR_OR_NULL((void *)paddr[i])) {
			cam_isr_log_err("addr err,sync_buf:[%d]0x%llx\n", i, paddr[i]);
			return;
		}
		ptr_u32 = (uint32_t *)&paddr[i];
		addr_l[i] = ptr_u32[0];
		addr_h[i] = (is_addr_64bit ? ptr_u32[1]: 0);
		reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_H(n) + i * 8, addr_h[i]);
		reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_L(n) + i * 8, addr_l[i]);
	}

	csi_set(csi_core, CSI_REG_REG_LOAD, (1 << n), (1 << n));
}

/* update buffer addr for second field */
static int csi_img_set_field_buf(struct csi_img_internal *img, struct csi_img_buf *buf)
{
	struct csi_core_internal *csi_core = img->core;
	const struct csi_format_pack *fmt = img->pfmt;
	int i, n = img->id;
	dma_addr_t paddr[3];
	uint32_t *ptr_u32;
	uint32_t addr_l[3], addr_h[3], stride[3];

	/* default set bottom field as second */
	for (i = 0; i < (int)fmt->planes; i++) {
		stride[i] = reg_read(csi_core->base, CSI_REG_IMG_RGBY_STRIDE(n) + i * 4);
		paddr[i] = buf->paddr[i] + (stride[i] / 2);

		// check addr: shouldn't IS_ERR_OR_NULL
		if (IS_ERR_OR_NULL((void *)paddr[i])) {
			cam_isr_log_err("addr err,field_buf:[%d]0x%llx\n", i, paddr[i]);
			return -1;
		}
		ptr_u32 = (uint32_t *)&paddr[i];
		addr_l[i] = ptr_u32[0];
		addr_h[i] = (is_addr_64bit ? ptr_u32[1]: 0);
		reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_H(n) + i * 8, addr_h[i]);
		reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_L(n) + i * 8, addr_l[i]);
	}
    csi_set(csi_core, CSI_REG_REG_LOAD, (1 << n), (1 << n));
	return 0;
}

static int csi_img_set_regs(struct csi_img_internal *img)
{
	int ret = 0, n = img->id;
	uint32_t set_val;
	struct csi_core_internal *csi_core = img->core;
	struct csi_img_buf *base_buf = &img->next_buf;

	csi_img_set_default(img);
	ret |= csi_cfg_img_buf(&csi_core->ex_dev, n, &img->next_buf);
	ret |= csi_img_set_channel(img);
	ret |= csi_img_set_size(img);
	ret |= csi_img_set_para_bt(img);
	ret |= csi_img_set_wdma(img);
	ret |= csi_img_set_memrange(img);
	if (ret) {
		csi_err("%s, csi%d, img %d, failed to set regs\n",
			__func__, csi_core->host_id, img->id);
		return ret;
	}

	if (csi_core->sync == 0) {
		/* enalbe irq */
		if (img->bus_type != CSI_BUS_BT656)
			csi_set(csi_core, CSI_REG_INT_MASK0, INT0_MASK(n), INT0_MASK(n));
		else
			csi_set(csi_core, CSI_REG_INT_MASK0, INT0_BT_MASK(n), INT0_BT_MASK(n));
		csi_set(csi_core, CSI_REG_INT_MASK1, INT1_ERR_MASK(n), INT1_ERR_MASK(n));
		/* [17]: WDMA_CFG_LOAD,  [15:8]: update mask_n, [7:4]: shadow_update_n by software(force shashow) */
		set_val = ((1 << 17)) | 0xFF00 | (0x10 << n);
		csi_set(csi_core, CSI_REG_REG_LOAD, set_val, set_val);
		/* enable stream */
		csi_set(csi_core, CSI_REG_ENABLE, (1 << n), (1 << n));
		return 0;
	}

	for (n = CSI_IMG1; n <= CSI_IMG3; n++) {
		img = &csi_core->images[n];
		csi_img_set_default(img);
		ret |= csi_img_set_channel(img);
		ret |= csi_img_set_size(img);
		ret |= csi_img_set_para_bt(img);
		ret |= csi_img_set_wdma(img);
		if (ret) {
			csi_err("%s, csi%d, img %d, failed to set regs\n",
				__func__, csi_core->host_id, img->id);
			return ret;
		}
		csi_img_set_sync_buf(&csi_core->images[n], base_buf);

	}
		/* enalbe irq */
		csi_set(csi_core, CSI_REG_INT_MASK0, 0xffffffff, 0xffffffff);
		csi_set(csi_core, CSI_REG_INT_MASK1, 0xffffffff, 0xffffffff);

	set_val = ((1 << 17)) | 0xFFF0;
	csi_set(csi_core, CSI_REG_REG_LOAD, set_val, set_val);

	/* enable all stream */
	csi_set(csi_core, CSI_REG_ENABLE, 0xF, 0xF);
	return 0;
}

/* setup parameter base on input configuration */
static int csi_img_setup_param(struct csi_img_internal *img)
{
	int i, total;
	uint32_t n;
	struct csi_core_internal *csi_core = img->core;
	const struct csi_format_pack *fmt_pack;

	if ((img->bus_type >= CSI_BUS_TYPE_MAX) ||
		(img->bus_type == CSI_BUS_PARALLEL1) ||
		(img->bus_type == CSI_BUS_BT1120_DDR)) {
		csi_err("%s, csi%d, img %d, unsupported bus_type %d\n",
			__func__, csi_core->host_id, img->id, img->bus_type);
		return -1;
	}

	if (img->field != CSI_FIELD_NONE && img->field != CSI_FIELD_INTERLACED) {
		csi_err("%s, csi%d, img %d, unsupported field_type %d\n",
			__func__, csi_core->host_id, img->id, img->field);
		return -1;
	}

	total = FIELD_HAS_BOTH(img->field) ? 2: 1;
	for (i = 0; i < total; i++) {
		if ((img->size[i].w > IMG_MAX_WIDTH) || (img->size[i].h > IMG_MAX_HEIGHT)) {
			csi_err("%s, csi%d, img %d, unsupported size %d %d\n",
				__func__, csi_core->host_id, img->id, img->size[i].w, img->size[i].h);
			return -1;
		}
		if (img->crop[i].x == 0 && img->crop[i].y == 0 &&
			img->crop[i].w == 0 && img->crop[i].h == 0) {
			img->crop[i].w = img->size[i].w;
			img->crop[i].h = img->size[i].h;
		}
		if (((img->crop[i].x + img->crop[i].w) > img->size[i].w) ||
			((img->crop[i].y + img->crop[i].h) > img->size[i].h)) {
			csi_err("%s, csi%d, img %d, error crop (%d %d %d %d) for size %d %d\n",
				__func__, csi_core->host_id, img->id,
				img->crop[i].x, img->crop[i].y, img->crop[i].w, img->crop[i].h,
				img->size[i].w, img->size[i].h);
			return -1;
		}
	}

	if (img->bus_type == CSI_BUS_MIPICSI2) {
		fmt_pack = &csi_mipi_fmts[0];
		total = sizeof(csi_mipi_fmts) / sizeof(csi_mipi_fmts[0]);
	} else {
		fmt_pack = &csi_parallel_fmts[0];
		total = sizeof(csi_parallel_fmts) / sizeof(csi_parallel_fmts[0]);
	}

	for (i = 0; i < total; i++, fmt_pack++) {
		if (fmt_pack->bus_fmt != img->bus_fmt || fmt_pack->output_fmt != img->output_fmt)
			continue;
		img->pfmt = fmt_pack;
		csi_info("%s, csi%d, img%d, bus type %d bus_fmt %d, outfmt: 0x%08x, index %d\n",
			__func__, csi_core->host_id, img->id, img->bus_type,
			img->bus_fmt, fmt_pack->output_fmt, i);
		break;
	}

	if (img->pfmt == NULL) {
		if (img->bus_type == CSI_BUS_MIPICSI2) {
			fmt_pack = &csi_mipi_fmts[0];
		} else {
			fmt_pack = &csi_parallel_fmts[0];
		}
		img->pfmt = fmt_pack;
		csi_err("%s, csi%d, img%d, No matching format found, Use the first, bus type %d\n",
			__func__, csi_core->host_id, img->id, img->bus_type);
		csi_err("%s: img bus_fmt:%d, fmt_pack bus_fmt:%d, img output_fmt:%d, fmt_pack output_fmt:%d\n",__func__,
			img->bus_fmt, fmt_pack->bus_fmt,
			img->output_fmt, fmt_pack->output_fmt);
	}

	if (img->bus_type == CSI_BUS_MIPICSI2)
		img->skip_frame = DEFAULT_SKIP_FRAMES;
	else
		img->skip_frame = DEFAULT_SKIP_FRAMES;

	csi_info("%s, csi%d img%d setup params done\n", __func__,
		img->core->host_id, img->id);

	if (csi_core->sync == 0)
		return 0;

	for (n = CSI_IMG1; n <= CSI_IMG3; n++) {
		img = &csi_core->images[n];
		memcpy(img, &csi_core->images[0], sizeof(struct csi_img_internal));
		spin_lock_init(&img->status_lock);
		img->id = n;
	}

	return 0;
}

static void csi_err_handle(struct csi_img_internal *img, uint32_t sync, uint32_t value0, uint32_t value1)
{
	int n = img->id;

	cam_isr_log_err("%s, csi%d img%d error. int0=0x%x, int1=0x%x, frm_cnt %d\n",
					__func__, img->core->host_id, img->id, value0, value1,
					img->frm_cnt);
	if (sync) {
		if (value0 & CSI_INT0_BT_ERR) {
			img->err_cnt.bt_err++;
			cam_isr_log_err("bt_err:%d\n", img->err_cnt.bt_err);
		}

		if (value0 & CSI_INT0_BT_FATAL) {
			img->err_cnt.bt_fatal++;
			cam_isr_log_err("bt_fatal:%d\n", img->err_cnt.bt_fatal);
		}

		if (value1 & (CSI_INT1_CROP_ERR0 | CSI_INT1_CROP_ERR1 | CSI_INT1_CROP_ERR2 | CSI_INT1_CROP_ERR3)) {
			img->err_cnt.crop_err++;
			cam_isr_log_err("crop_err:%d\n", img->err_cnt.crop_err);
		}

		if (value1 & (CSI_INT1_PIXEL_ERR0 | CSI_INT1_PIXEL_ERR1 | CSI_INT1_PIXEL_ERR2 | CSI_INT1_PIXEL_ERR3)) {
			img->err_cnt.pix_err++;
			cam_isr_log_err("pix_err:%d\n", img->err_cnt.pix_err);
		}

		if (value1 & (CSI_INT1_OVERFLOW0 | CSI_INT1_OVERFLOW1 | CSI_INT1_OVERFLOW2 | CSI_INT1_OVERFLOW3)) {
			img->err_cnt.overflow++;
			cam_isr_log_err("overflow:%d\n", img->err_cnt.overflow);
		}

		if (value1 & (CSI_INT1_IMG0_BUSERR0 | CSI_INT1_IMG1_BUSERR0 | CSI_INT1_IMG2_BUSERR0 | CSI_INT1_IMG3_BUSERR0)) {
			img->err_cnt.stream0_err++;
			// log: 1/32
			if ((img->err_cnt.stream0_err & 0x1F) == 0x01) {
				cam_isr_log_err("[0]:0x%lx,0x%lx;[1]:0x%lx,0x%lx;[2]:0x%lx,0x%lx;[3]:0x%lx,0x%lx",
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_H(0)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_L(0)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_H(1)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_L(1)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_H(2)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_L(2)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_H(3)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_L(3)));
			}
		}

		if (value1 & (CSI_INT1_IMG0_BUSERR1 | CSI_INT1_IMG1_BUSERR1 | CSI_INT1_IMG2_BUSERR1 | CSI_INT1_IMG3_BUSERR1)) {
			img->err_cnt.stream1_err++;
			// log: 1/32
			if ((img->err_cnt.stream1_err & 0x1F) == 0x01) {
				cam_isr_log_err("[0]:0x%lx,0x%lx;[1]:0x%lx,0x%lx;[2]:0x%lx,0x%lx;[3]:0x%lx,0x%lx",
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_H(0)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_L(0)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_H(1)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_L(1)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_H(2)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_L(2)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_H(3)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_L(3)));
			}
		}

		if (value1 & (CSI_INT1_IMG0_BUSERR2 | CSI_INT1_IMG1_BUSERR2 | CSI_INT1_IMG2_BUSERR2 | CSI_INT1_IMG3_BUSERR2)) {
			img->err_cnt.stream2_err++;
			// log: 1/32
			if ((img->err_cnt.stream1_err & 0x1F) == 0x01) {
				cam_isr_log_err("[0]:0x%lx,0x%lx;[1]:0x%lx,0x%lx;[2]:0x%lx,0x%lx;[3]:0x%lx,0x%lx",
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_H(0)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_L(0)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_H(1)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_L(1)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_H(2)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_L(2)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_H(3)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_L(3)));
			}
		}

	} else {

		if (value0 & (CSI_INT0_BT_ERR << n)) {
			img->err_cnt.bt_err++;
			cam_isr_log_err("bt_err:%d\n", img->err_cnt.bt_err);
		}

		if (value0 & (CSI_INT0_BT_FATAL << n)) {
			img->err_cnt.bt_fatal++;
			cam_isr_log_err("bt_fatal:%d\n", img->err_cnt.bt_fatal);
		}

		if (value1 & (CSI_INT1_CROP_ERR0 << n)) {
			img->err_cnt.crop_err++;
			cam_isr_log_err("crop_err:%d\n", img->err_cnt.crop_err);
		}

		if (value1 & (CSI_INT1_PIXEL_ERR0 << n)) {
			img->err_cnt.pix_err++;
			cam_isr_log_err("pix_err:%d\n", img->err_cnt.pix_err);
		}

		if (value1 & (CSI_INT1_OVERFLOW0 << n)) {
			img->err_cnt.overflow++;
			cam_isr_log_err("overflow:%d\n", img->err_cnt.overflow);
		}

		if (value1 & (CSI_INT1_IMG0_BUSERR0 << n)) {
			img->err_cnt.stream0_err++;
			// log: 1 / 32
			if ((img->err_cnt.stream0_err & 0x1F) == 0x1)
				cam_isr_log_err("[0x%lx]:0x%lx,0x%lx\n",
					img->core->base + CSI_REG_IMG_RGBY_BADDR_H(n),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_H(n)),
					reg_read(img->core->base, CSI_REG_IMG_RGBY_BADDR_L(n)));
		}

		if (value1 & (CSI_INT1_IMG0_BUSERR1 << n)) {
			img->err_cnt.stream1_err++;
			// log: 1 / 32
			if ((img->err_cnt.stream1_err & 0x1F) == 0x1)
				cam_isr_log_err("[0x%lx]:0x%lx,0x%lx\n",
					img->core->base + CSI_REG_IMG_U_BADDR_H(n),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_H(n)),
					reg_read(img->core->base, CSI_REG_IMG_U_BADDR_L(n)));
		}

		if (value1 & (CSI_INT1_IMG0_BUSERR2 << n)) {
			img->err_cnt.stream2_err++;
			// log: 1 / 32
			if ((img->err_cnt.stream2_err & 0x1F) == 0x1)
				cam_isr_log_err("[0x%lx]:0x%lx,0x%lx\n",
					img->core->base + CSI_REG_IMG_V_BADDR_H(n),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_H(n)),
					reg_read(img->core->base, CSI_REG_IMG_V_BADDR_L(n)));
		}
	}
}

/* ============================== external API below ===================================== */

/* ============== following interface for csi_image ================== */

int csi_reset_img(struct csi_hw_dev *csi_dev, uint32_t img_id, uint32_t cmd)
{
	int n = img_id;
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, &cmd);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];

	if (cmd & CSI_RESET_IMG_HW) {
		//b4-b7:soft reset
		csi_set(csi_core, CSI_REG_ENABLE, (0x10 << n), (0x10 << n));
		usleep_range(2, 10);
		csi_set(csi_core, CSI_REG_ENABLE, (0x10 << n), 0);
	}

	if (cmd & CSI_RESET_IMG_CTX) {
		memset(img, 0, sizeof(struct csi_img_internal));
		spin_lock_init(&img->status_lock);
		spin_lock_init(&img->crop_lock);
		img->core = csi_core;
		img->id = n;
	}

	csi_info("%s, csi%d img%d reset cmd 0x%x done\n", __func__,
		img->core->host_id, img->id, cmd);
	return 0;
}


int csi_register_callback(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_callback_t *cb)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, cb);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	memcpy(&img->irq_cb, cb, sizeof(struct csi_callback_t));

	csi_info("%s, csi%d img%d done\n", __func__,
		img->core->host_id, img->id);
	return 0;
}

int csi_cfg_bus(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_bus_info *bus)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, bus);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	img->bus_type = bus->bus_type;
	img->bus_fmt = bus->mbus_fmt;
	img->bus_flag = bus->bus_flag;

	csi_info("%s, csi%d, img%d, bus type %d bus_fmt %d, bus_flag 0x%x\n",
		__func__, csi_core->host_id, img_id,
		img->bus_type, img->bus_fmt, img->bus_flag);

	return 0;
}

int csi_query_outfmts(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_outfmt_info *fmt)
{
	int i, total;
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;
	const struct csi_format_pack *fmt_pack;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, fmt);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	if (img->bus_type == CSI_BUS_MIPICSI2) {
		fmt_pack = &csi_mipi_fmts[0];
		total = sizeof(csi_mipi_fmts) / sizeof(csi_mipi_fmts[0]);
	} else {
		fmt_pack = &csi_parallel_fmts[0];
		total = sizeof(csi_parallel_fmts) / sizeof(csi_parallel_fmts[0]);
	}

	fmt->count = 0;
	for (i = 0; i < total; i++, fmt_pack++) {
		if (fmt_pack->bus_fmt != img->bus_fmt)
			continue;
		fmt->fmts[fmt->count] = fmt_pack->output_fmt;
		fmt->count++;
		csi_info("%s, csi%d, img%d, bus type %d bus_fmt %d, outfmt%d: 0x%08x\n",
			__func__, csi_core->host_id, img_id,
			img->bus_type, img->bus_fmt, fmt->count, fmt_pack->output_fmt);
	}

	return 0;
}

int csi_cfg_outfmt(struct csi_hw_dev *csi_dev, uint32_t img_id, uint32_t outfmt)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, &outfmt);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	img->output_fmt = outfmt;

	csi_info("%s, csi%d, img%d, output fmt %d\n",
		__func__, csi_core->host_id, img_id, img->output_fmt);

	return 0;
}

int csi_cfg_field(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_field_info *field)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, field);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	img->field = field->field_type;

	csi_info("%s, csi%d, img%d, field_type %d\n",
		__func__, csi_core->host_id, img_id, img->field);

	return 0;
}

int csi_cfg_size(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_size *size)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, size);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	img->size[0] = size[0];
	img->size[1] = size[1];

	csi_info("%s, csi%d, img%d, size0[ %d x %d ], size1[ %d x %d ]\n",
		__func__, csi_core->host_id, img_id,
		size[0].w, size[0].h, size[1].w, size[1].h);

	return 0;
}

int csi_cfg_crop(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_crop *crop)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;
	unsigned long flags;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, crop);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];

	csi_info("%s, csi%d, img%d, crop0[%d %d %d %d], crop1[%d %d %d %d]\n",
		__func__, csi_core->host_id, img_id,
		crop[0].x, crop[0].y, crop[0].w, crop[0].h,
		crop[1].x, crop[1].y, crop[1].w, crop[1].h);

	if (img->status != IMG_STATUS_STREAM_ON) {
		img->crop[0] = crop[0];
		img->crop[1] = crop[1];
		return 0;
	}

	if (((crop[0].x + crop[0].w) > img->size[0].w) ||
		((crop[0].y + crop[0].h) > img->size[0].h) ||
		(crop[0].w == 0) || (crop[0].w == 0)) {
		csi_info("%s, csi%d, img%d, error crop0[%d %d %d %d]\n",
			__func__, csi_core->host_id, img_id,
			crop[0].x, crop[0].y, crop[0].w, crop[0].h);
		return -1;
	}

	spin_lock_irqsave(&img->crop_lock, flags);
	if (img->crop_update == 0) {
		/* make sure previous setting is update to register.  */
		img->crop_update = (img->crop[0].x ^ crop[0].x);
		img->crop_update |= (img->crop[0].y ^ crop[0].y);
		img->crop_update |= (img->crop[0].w ^ crop[0].w);
		img->crop_update |= (img->crop[0].h ^ crop[0].h);
		if (img->crop_update)
			img->crop[0] = crop[0];
	}
	spin_unlock_irqrestore(&img->crop_lock, flags);

	return 0;
}

int csi_cfg_img_buf(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_buf *buf)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;
	int i, n = img_id;
	uint32_t addr_l[3], addr_h[3];
	unsigned long flags;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, buf);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	img->next_buf = *buf;

	if (csi_core->sync && img_id != CSI_IMG0) {
		cam_isr_log_err("%s: invalid img_id %d for sync mode\n\n",
				__func__, img_id);
		return -1;
	}

	if (img->pfmt == NULL) {
		cam_isr_log_err("csi img%d is not stream on\n", img_id);
		return 0;
	}

	for (i = 0; i < img->pfmt->planes; i++) {
		uint32_t *ptr_u32 = (uint32_t *)&buf->paddr[i];

		// check addr: shouldn't IS_ERR_OR_NULL
		if (IS_ERR_OR_NULL((void *)buf->paddr[i])) {
			cam_isr_log_err("addr err:[%d]0x%llx\n", i, buf->paddr[i]);
			return -1;
		}
		addr_l[i] = ptr_u32[0];
		addr_h[i] = (is_addr_64bit ? ptr_u32[1]: 0);
		reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_H(n) + i * 8, addr_h[i]);
		reg_write(csi_core->base, CSI_REG_IMG_RGBY_BADDR_L(n) + i * 8, addr_l[i]);
	}
	spin_lock_irqsave(&img->crop_lock, flags);
	if (img->crop_update) {
		csi_img_set_crop(img);
		img->crop_update = 0;
	}
	spin_unlock_irqrestore(&img->crop_lock, flags);
	/* shadow_set_n*/
	csi_set(csi_core, CSI_REG_REG_LOAD, (1 << n), (1 << n));

	if (!csi_core->sync || img->status != IMG_STATUS_STREAM_ON)
		return 0;

	/* if sync mode and is stream on, img1~3 buffer should be updated as well */
	for (n = CSI_IMG1; n <= CSI_IMG3; n++)
		csi_img_set_sync_buf(&csi_core->images[n], buf);

	return 0;
}

int csi_cfg_mem_range(struct csi_hw_dev *csi_dev, uint32_t img_id, struct mem_range mem[3])
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	CSI_IMG_ERR_RETURN(csi_dev, img_id, mem);

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];
	img->fb_mode = FB_MODE_ROUND;
	memcpy(img->round_mem, mem, sizeof(img->round_mem));

	csi_info("%s, csi%d, img%d, mem [0x%08x 0x%08x], [0x%08x 0x%08x], [0x%08x 0x%08x]\n",
		__func__, csi_core->host_id, img_id,
		mem[0].low, mem[0].up, mem[1].low, mem[1].up, mem[2].low, mem[2].up);

	return 0;
}

int csi_stream_on(struct csi_hw_dev *csi_dev, uint32_t img_id)
{
	int ret;
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;

	if ((csi_dev == NULL) || (img_id >= CSI_IMG_NUM)) {
		csi_err("%s: failed to get csi_dev %p, img_id %d\n",
				__func__, csi_dev, img_id);
		return -1;
	}

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	img = &csi_core->images[img_id];

	ret = csi_img_setup_param(img);
	if (ret) {
		csi_err("%s: csi%d, img_id %d failed to setup param\n",
				__func__, csi_core->host_id, img_id);
		return -1;
	}

	ret = csi_img_set_regs(img);
	if (ret) {
		csi_err("%s: csi%d, img_id %d failed to stream on\n",
				__func__, csi_core->host_id, img_id);
		return -1;
	}

	memset(&img->err_cnt, 0, sizeof(struct csi_img_err_cnt));
	img->frm_cnt = 0;
	img->stream_start = 0;
	img->skip_shadow = 0;
	img->status = IMG_STATUS_STREAM_ON;
	return 0;
}

int csi_stream_off(struct csi_hw_dev *csi_dev, uint32_t img_id)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;
	int n, img_s, img_e;
	unsigned long flags;

	if ((csi_dev == NULL) || (img_id >= CSI_IMG_NUM)) {
		csi_err("%s: failed to get csi_dev %p, img_id %d\n",
				__func__, csi_dev, img_id);
		return -1;
	}

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	if (csi_core->sync == 0) {
		img_s = img_id;
		img_e = img_id;
	} else {
		img_s = CSI_IMG0;
		img_e = CSI_IMG3;
	}

	for (n = img_s; n <= img_e; n++) {
		img = &csi_core->images[n];
		spin_lock_irqsave(&img->status_lock, flags);

		/* disable stream */
		csi_set(csi_core, CSI_REG_ENABLE, (1 << n), 0);

		/* disable irq */
		csi_set(csi_core, CSI_REG_INT_MASK0, INT0_MASK(n), 0);
		csi_set(csi_core, CSI_REG_INT_MASK1, INT1_ERR_MASK(n), 0);
		csi_set(csi_core, CSI_REG_REG_LOAD, (1 << (4 + n)), (1 << (4 + n)));

		img->status = IMG_STATUS_IDLE;
		spin_unlock_irqrestore(&img->status_lock, flags);
	}

	return 0;
}



/* ================ following interface for csi_core =============== */
int csi_cfg_sync(struct csi_hw_dev *csi_dev, uint32_t sync)
{
	struct csi_core_internal *csi_core;

	if (csi_dev == NULL || csi_dev->host_id >= CSI_HOST_NUM) {
		csi_err("%s: fail to get input csi dev %p\n", __func__, csi_dev);
		return -1;
	}

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	csi_core->sync = !!sync;
	csi_info("%s: csi%d sync_mode %d\n", __func__, csi_core->host_id, csi_core->sync);

	return 0;
}

int csi_irq_handle(struct csi_hw_dev *csi_dev)
{
	struct csi_core_internal *csi_core;
	struct csi_img_internal *img;
	int i;
	uint32_t value0, value1;
	unsigned long flags;

	if (csi_dev == NULL) {
		cam_isr_log_err("%s: failed to get csi_dev %p\n", __func__, csi_dev);
		return -1;
	}

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;

	value0 = reg_read(csi_core->base, CSI_REG_INT_STAT0);
	value1 = reg_read(csi_core->base, CSI_REG_INT_STAT1);
	reg_write(csi_core->base, CSI_REG_INT_STAT0, value0);
	reg_write(csi_core->base, CSI_REG_INT_STAT1, value1);
	cam_isr_log_trace("int:0x%x 0x%x\n", value0, value1);

	if (csi_core->sync) {
		img = &csi_core->images[0];

		spin_lock_irqsave(&img->status_lock, flags);
		if (img->status != IMG_STATUS_STREAM_ON) {
			spin_unlock_irqrestore(&img->status_lock, flags);
			return 0;
		}

		/* [0:3] - img store done, [4:7] - img shadow done */
		csi_core->sync_irq |= (value0 & 0xff);

		if ((csi_core->sync_irq & 0x0f) == 0x0f) {
			if (img->stream_start && img->irq_cb.csi_frame_done)
				img->irq_cb.csi_frame_done(img->irq_cb.caller, img->id);
			csi_core->sync_irq &= 0xf0;
			img->frm_cnt++;
		}

		if ((csi_core->sync_irq & 0xf0) == 0xf0) {
			if (img->irq_cb.csi_update_buf)
				img->irq_cb.csi_update_buf(img->irq_cb.caller, img->id);
			csi_core->sync_irq &= 0x0f;
			img->stream_start = 1;
		}
		if (img->stream_start &&
			((value0 & INT0_ERR_MASK_ALL) || (value1 & INT1_ERR_MASK_ALL))) {
			if (img->irq_cb.csi_error_handle)
				img->irq_cb.csi_error_handle(img->irq_cb.caller, img->id);
			csi_err_handle(img, 1, value0, value1);
		}
		spin_unlock_irqrestore(&img->status_lock, flags);

	} else {
		for (i = CSI_IMG0; i <= CSI_IMG3; i++) {
			if ((value0 & ((CSI_INT0_STORE_DONE0 | CSI_INT0_SHADOW_SET0) << i)) == 0 && value1 == 0)
				continue;
			img = &csi_core->images[i];
			spin_lock_irqsave(&img->status_lock, flags);
			if (img->status != IMG_STATUS_STREAM_ON) {
				spin_unlock_irqrestore(&img->status_lock, flags);
				continue;
			}
			cam_isr_log_trace("int: img%d status %d proc \n", img->id, img->status);

			if (value0 & (CSI_INT0_STORE_DONE0 << i)) {
				cam_isr_log_trace("int: img%d status %d csi_frame_done %p\n", img->id, img->status,
					img->irq_cb.csi_frame_done);
				if (img->stream_start && img->irq_cb.csi_frame_done) {
					cam_isr_log_trace("int: img%d caller %p\n", img->id, img->irq_cb.caller);
					img->irq_cb.csi_frame_done(img->irq_cb.caller, img->id);
				}
				if (img->frm_cnt < 5)
					cam_isr_log_warn("frame_done: %d\n", img->frm_cnt);
				img->frm_cnt++;
			}
			if (value0 & (CSI_INT0_SHADOW_SET0 << i)) {
				if (FIELD_HAS_BOTH(img->field) && img->second_field == 0) {
					img->second_field = 1;
					csi_img_set_field_buf(img, &img->next_buf);
				} else if (!img->skip_shadow && img->irq_cb.csi_update_buf) {
					img->second_field = 0;
					cam_isr_log_trace("int: img%d updata buf caller %p\n", img->id, img->irq_cb.caller);
					img->irq_cb.csi_update_buf(img->irq_cb.caller, img->id);
				}
				img->stream_start = 1;
				img->skip_shadow = 0;
				if (img->frm_cnt < 5)
					cam_isr_log_warn("shadow_done: %d\n", img->frm_cnt);
			}

			if (img->stream_start &&
				((value0 & INT0_ERR_MASK(i)) || (value1 & INT1_ERR_MASK(i)))) {
				if (img->irq_cb.csi_error_handle)
					img->irq_cb.csi_error_handle(img->irq_cb.caller, img->id);
				csi_err_handle(img, 0, value0, value1);
			}
			spin_unlock_irqrestore(&img->status_lock, flags);
		}
	}

	return 0;
}

const struct csi_hw_operations csi_ops = {
	.csi_irq_handle = csi_irq_handle,
	.csi_cfg_sync = csi_cfg_sync,

	.csi_reset_img = csi_reset_img,
	.csi_register_callback = csi_register_callback,
	.csi_cfg_bus = csi_cfg_bus,
	.csi_query_outfmts = csi_query_outfmts,
	.csi_cfg_outfmt = csi_cfg_outfmt,
	.csi_cfg_field = csi_cfg_field,
	.csi_cfg_size = csi_cfg_size,
	.csi_cfg_crop = csi_cfg_crop,
	.csi_cfg_img_buf = csi_cfg_img_buf,

	.csi_stream_on = csi_stream_on,
	.csi_stream_off = csi_stream_off,
};

struct csi_hw_dev *csi_hw_init(reg_addr_t base, uint32_t host_id)
{
	struct csi_core_internal *csi_core;
	int i;

	if (host_id >= CSI_HOST_NUM) {
		csi_err("%s: error input csi host id %d, should be 0~2\n", __func__, host_id);
		return NULL;
	}

	csi_core = &csi_cores[host_id];
	if (csi_core->inited) {
		csi_warn("%s: csi%d is already inited\n", __func__, host_id);
		return &csi_core->ex_dev;
	}

	for (i = 0; i < CSI_IMG_NUM; i++) {
		csi_core->images[i].id = i;
		csi_core->images[i].core = csi_core;
		spin_lock_init(&csi_core->images[i].status_lock);
		spin_lock_init(&csi_core->images[i].crop_lock);
	}

	spin_lock_init(&csi_core->glb_reg_lock);
	csi_core->base = base;
	/* global reset */
	reg_write(csi_core->base, CSI_REG_ENABLE, (1 << 8));
	usleep_range(2, 10);
	reg_write(csi_core->base, CSI_REG_ENABLE, 0);

	csi_core->ex_dev.priv_data = (void *)csi_core;
	csi_core->ex_dev.ops = csi_ops;
	csi_core->inited = 1;
	csi_info("%s: csi%d init done\n", __func__, host_id);

	return &csi_core->ex_dev;
}

int csi_hw_deinit(struct csi_hw_dev *csi_dev)
{
	struct csi_core_internal *csi_core;

	if (csi_dev == NULL) {
		csi_err("%s: NULL input csi dev\n", __func__);
		return -1;
	}
	if (csi_dev->host_id >= CSI_HOST_NUM) {
		csi_err("%s: error input csi dev host_id %d\n", __func__, csi_dev->host_id);
		return -1;
	}

	csi_core = (struct csi_core_internal *)csi_dev->priv_data;
	/* global reset */
	reg_write(csi_core->base, CSI_REG_ENABLE, (1 << 8));
	usleep_range(2, 10);
	reg_write(csi_core->base, CSI_REG_ENABLE, 0);

	csi_core->inited = 0;
	csi_info("%s: csi%d deinit done\n", __func__, csi_core->host_id);

	return 0;
}
