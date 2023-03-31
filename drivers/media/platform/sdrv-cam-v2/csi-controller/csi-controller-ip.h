/*
 * csi_reg.h
 *
 * Semidrive platform csi header file
 *
 * Copyright (C) 2022, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __CSI_HW_H__
#define __CSI_HW_H__

#include "sdrv-cam-os-def.h"

/* max output format for one specified input bus fmt */
#define CSI_OUTFMT_MAX 8


/* csi hardware id */
enum csi_host_id {
	CSI_HOST_0 = 0,
	CSI_HOST_1,
	CSI_HOST_2,
	CSI_HOST_NUM,
};

/* image channel id in one csi hardware device */
enum csi_img_id {
	CSI_IMG0 = 0,
	CSI_IMG1,
	CSI_IMG2,
	CSI_IMG3,
	CSI_IMG_NUM
};

/*
 * for cmd of: int csi_reset_img(struct csi_hw_dev *csi_dev, uint32_t img_id, uint32_t cmd);
 * CSI_RESET_IMG_HW: reset hardware (csi virtual channel)
 * CSI_RESET_IMG_CTX: reset image software context.
 * (CSI_RESET_IMG_HW | CSI_RESET_IMG_CTX) should be applied every time video open
 * ”CSI_RESET_IMG_HW only“ can be applied for error recovering
 */
enum csi_img_reset_type {
	CSI_RESET_IMG_HW = (1 << 0),
	CSI_RESET_IMG_CTX = (1 << 1),
};

/* input camera signal interface type */
enum csi_bus_type {
	CSI_BUS_MIPICSI2 = 0, /* mipi-csi2 */
	CSI_BUS_PARALLEL1, /* continuous clk, VSYNC & HSYNC, gate mode1  -- NOT SUPPORTED */
	CSI_BUS_PARALLEL2, /* continuous clk, VSYNC * HSYNC, gate mode2 */
	CSI_BUS_PARALLEL3, /* non-continuous clk, VSYNC */
	CSI_BUS_PARALLEL4, /* continuous clk, VSYNC & HSYNC, DE(data-enable), same as MIPICSI2 */
	CSI_BUS_BT656, /* clk, data */
	CSI_BUS_BT1120_SDR, /* clk, single data one cycle */
	CSI_BUS_BT1120_DDR, /* clk, double data one cycle  -- NOT SUPPORTED */
	CSI_BUS_TYPE_MAX
};

/* input media format */
enum csi_mbus_fmt {
	CSI_MBUS_UYVY8_2X8 = 1,  /* for mipi-csi YUV422-8bit, for parallel UYVY-8bit */
	CSI_MBUS_YUYV8_2X8,      /*                           for parallel YUYV-8bit*/
	CSI_MBUS_UYVY10_2X10,    /* for mipi-csi YUV422-10bit */
	CSI_MBUS_UYVY8_1_5X8,    /* for mipi-csi YUV420-legacy, for parallel UYY-VYY-8bit */
	CSI_MBUS_VYUY8_1_5X8,    /* for mipi-csi YUV420,      for parallel VYY-UYY-8bit */
	CSI_MBUS_RGB888_1X24,    /* for mipi-csi RGB-8bit,    for parallel RGB-8bit */
	/* More types to be added ....*/
	CSI_MBUS_FMT_MAX
};

enum csi_bus_flag {
	MBUS_DE_ACTIVE_LOW = (1 << 0),
	MBUS_VSYNC_ACTIVE_LOW = (1 << 1),
	MBUS_HSYNC_ACTIVE_HIGH = (1 << 2),
	MBUS_PCLK_SAMPLE_RISING = (1 << 3),
};

/* csi output format (stored in memory) */
enum csi_out_fmt {
	CSI_FMT_YUYV = 1, /* Y-U-Y-V, 8-8-8-8 */
	CSI_FMT_UYVY,     /* U-Y-V-Y, 8-8-8-8 */
	CSI_FMT_YUV422SP, /* Y-Y-Y-Y, U-V-U-V, two planes*/
	CSI_FMT_RGB24,  /* R-G-B, 8-8-8*/ 
	CSI_FMT_BGR24,  /* B-G-R, 8-8-8*/
	/* More types to be added ....*/	
	CSI_FMT_MAX,
};

/* input image type. For BT656, interlaced field is supportted. Other cases should be CSI_FIELD_NONE */
enum csi_field_type {
	CSI_FIELD_NONE,  /* no field, progressive */
	CSI_FIELD_TOP,
	CSI_FIELD_BOTTOM,
	CSI_FIELD_INTERLACED, /* both top and bottom field and interlaced into one frame */
	CSI_FIELD_MAX,
};

struct csi_img_crop {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct csi_img_size {
	uint32_t w;
	uint32_t h;
};

struct csi_img_buf {
	dma_addr_t paddr[3];
};

struct mem_range {
	uint32_t up;
	uint32_t low;
};

struct csi_bus_info {
	uint32_t bus_type; /* should be enum csi_bus_type */
	uint32_t mbus_fmt; /* should be enum csi_mbus_fmt */
	uint32_t bus_flag;
};

/* supported output formats for one specified bus_types and mbus_fmt */
struct csi_outfmt_info {
	uint32_t count;    
	uint32_t fmts[CSI_OUTFMT_MAX];
};

struct csi_field_info {
	uint32_t field_type;  /* should be enum csi_field_type */
};

struct csi_callback_t {
	void *caller;
	int (*csi_frame_done)(void *caller, uint32_t img_id);
	int (*csi_update_buf)(void *caller, uint32_t img_id);
	int (*csi_error_handle)(void *caller, uint32_t img_id);
};

struct csi_hw_dev;

struct csi_hw_operations {
	/* ==== following interface for csi_core ==== */
	int (*csi_irq_handle)(struct csi_hw_dev *csi_dev);
	int (*csi_cfg_sync)(struct csi_hw_dev *csi_dev, uint32_t sync);

	/* ==== following interface for csi_image (one of virtual channel) ==== */
	int (*csi_reset_img)(struct csi_hw_dev *csi_dev, uint32_t img_id, uint32_t cmd);
	int (*csi_register_callback)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_callback_t *cb);
	int (*csi_cfg_bus)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_bus_info *bus);
	int (*csi_query_outfmts)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_outfmt_info *fmt);
	int (*csi_cfg_outfmt)(struct csi_hw_dev *csi_dev, uint32_t img_id, uint32_t outfmt);
	int (*csi_cfg_field)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_field_info *field);
	int (*csi_cfg_size)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_size *size);
	int (*csi_cfg_crop)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_crop *crop);
	int (*csi_cfg_img_buf)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_buf *buf);
	int (*csi_cfg_mem_range)(struct csi_hw_dev *csi_dev, uint32_t img_id, struct mem_range mem[3]);

	int (*csi_stream_on)(struct csi_hw_dev *csi_dev, uint32_t img_id);
	int (*csi_stream_off)(struct csi_hw_dev *csi_dev, uint32_t img_id);
};

struct csi_hw_dev {
	void *priv_data;
	uint32_t host_id;
	struct csi_hw_operations ops;
};

/* ==== following interface for csi_core ==== */
struct csi_hw_dev *csi_hw_init(reg_addr_t base, uint32_t host_id);
int csi_hw_deinit(struct csi_hw_dev *csi_dev);
int csi_irq_handle(struct csi_hw_dev *csi_dev);
int csi_cfg_sync(struct csi_hw_dev *csi_dev, uint32_t sync);

/* ==== following interface for csi_image (one of virtual channel) ==== */
int csi_reset_img(struct csi_hw_dev *csi_dev, uint32_t img_id, uint32_t cmd);
int csi_register_callback(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_callback_t *cb);
int csi_cfg_bus(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_bus_info *bus);
int csi_query_outfmts(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_outfmt_info *fmt);
int csi_cfg_outfmt(struct csi_hw_dev *csi_dev, uint32_t img_id, uint32_t outfmt);
int csi_cfg_field(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_field_info *field);
int csi_cfg_size(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_size *size);
int csi_cfg_crop(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_crop *crop);
int csi_cfg_img_buf(struct csi_hw_dev *csi_dev, uint32_t img_id, struct csi_img_buf *buf);

int csi_stream_on(struct csi_hw_dev *csi_dev, uint32_t img_id);
int csi_stream_off(struct csi_hw_dev *csi_dev, uint32_t img_id);
#endif
