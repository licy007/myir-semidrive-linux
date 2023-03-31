/*
 * dpc.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#ifndef _sdrv_plane_H_
#define _sdrv_plane_H_

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <video/videomode.h>

#include "sdrv_dpc.h"

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420, DRM_FORMAT_YVU420,
};
/*Kunlun DC&DP layer IDs*/
enum {
	DC_SPIPE = BIT(0),
	DC_GPIPE = BIT(1),
	DP_GPIPE0 = BIT(2),
	DP_GPIPE1 = BIT(3),
	DP_SPIPE0 = BIT(4),
	DP_SPIPE1 = BIT(5),
};

/*Kunlun DC&DP layer comp swip*/
enum {
	SWAP_A_RGB			= 0b0000,
	SWAP_A_RBG			= 0b0001,
	SWAP_A_GBR			= 0b0010,
	SWAP_A_GRB			= 0b0011,
	SWAP_A_BGR			= 0b0100,
	SWAP_A_BRG			= 0b0101,
	SWAP_B_ARG			= 0b1000,
	SWAP_B_AGR			= 0b1001,
	SWAP_B_RGA			= 0b1010,
	SWAP_B_RAG			= 0b1011,
	SWAP_B_GRA			= 0b1100,
	SWAP_B_GAR			= 0b1101
};

/*Kunlun DC&DP layer format uv mode*/
enum {
	UV_YUV444_RGB       = 0b00,
	UV_YUV422           = 0b01,
	UV_YUV440           = 0b10,
	UV_YUV420           = 0b11
};

/*Kunlun DC&DP layer format memory mode*/
enum {
	LINEAR_MODE             = 0b000,
	RLE_COMPR_MODE          = 0b001,
	GPU_RAW_TILE_MODE       = 0b010,
	GPU_CPS_TILE_MODE       = 0b011,
	VPU_RAW_TILE_MODE       = 0b100,
	VPU_CPS_TILE_MODE       = 0b101,
	VPU_RAW_TILE_988_MODE   = 0b110,
};

/*Kunlun DC&DP layer format planar mode*/
enum {
	FMT_INTERLEAVED     = 0b00,
	FMT_MONOTONIC       = 0b01,
	FMT_SEMI_PLANAR     = 0b10,
	FMT_PLANAR          = 0b11
};

/*Kunlun DC&DP layer format rotation mode*/
enum {
	ROT_DEFAULT         = 0b000,
	ROT_ROT             = 0b001,
	ROT_VFLIP           = 0b010,
	ROT_HFLIP           = 0b100
};

/*Kunlun DP layer format TILE vsize*/
enum {
	TILE_VSIZE_1       = 0b000,
	TILE_VSIZE_2       = 0b001,
	TILE_VSIZE_4       = 0b010,
	TILE_VSIZE_8       = 0b011,
	TILE_VSIZE_16      = 0b100,
};

/*Kunlun DP layer format TILE hsize*/
enum {
	TILE_HSIZE_1       = 0b000,
	TILE_HSIZE_8       = 0b001,
	TILE_HSIZE_16      = 0b010,
	TILE_HSIZE_32      = 0b011,
	TILE_HSIZE_64      = 0b100,
	TILE_HSIZE_128     = 0b101,
};

/**/
enum {
	FBDC_U8U8U8U8      = 0xc,
	FBDC_U16           = 0x9,
	FBDC_R5G6B5        = 0x5,
	FBDC_U8            = 0x0,
	FBDC_NV21          = 0x37,
	FBDC_YUV420_16PACK = 0x65
};

enum {
	PLANE_DISABLE,
	PLANE_ENABLE
};

enum {
	PROP_PLANE_CAP_RGB = 0,
	PROP_PLANE_CAP_YUV,
	PROP_PLANE_CAP_XFBC,
	PROP_PLANE_CAP_YUV_FBC,
	PROP_PLANE_CAP_ROTATION,
	PROP_PLANE_CAP_SCALING,
};

typedef enum {
    DATASPACE_UNKNOWN = 0,
    DATASPACE_ARBITRARY = 1,
    DATASPACE_STANDARD_SHIFT = 16,
    DATASPACE_STANDARD_MASK = 4128768,                      // (63 << STANDARD_SHIFT)
    DATASPACE_STANDARD_UNSPECIFIED = 0,                     // (0 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT709 = 65536,                       // (1 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT601_625 = 131072,                  // (2 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT601_625_UNADJUSTED = 196608,       // (3 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT601_525 = 262144,                  // (4 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT601_525_UNADJUSTED = 327680,       // (5 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT2020 = 393216,                     // (6 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE = 458752,  // (7 << STANDARD_SHIFT)
    DATASPACE_STANDARD_BT470M = 524288,                     // (8 << STANDARD_SHIFT)
    DATASPACE_STANDARD_FILM = 589824,                       // (9 << STANDARD_SHIFT)
    DATASPACE_STANDARD_DCI_P3 = 655360,                     // (10 << STANDARD_SHIFT)
    DATASPACE_STANDARD_ADOBE_RGB = 720896,                  // (11 << STANDARD_SHIFT)
    DATASPACE_TRANSFER_SHIFT = 22,
    DATASPACE_TRANSFER_MASK = 130023424,       // (31 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_UNSPECIFIED = 0,        // (0 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_LINEAR = 4194304,       // (1 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_SRGB = 8388608,         // (2 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_SMPTE_170M = 12582912,  // (3 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_GAMMA2_2 = 16777216,    // (4 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_GAMMA2_6 = 20971520,    // (5 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_GAMMA2_8 = 25165824,    // (6 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_ST2084 = 29360128,      // (7 << TRANSFER_SHIFT)
    DATASPACE_TRANSFER_HLG = 33554432,         // (8 << TRANSFER_SHIFT)
    DATASPACE_RANGE_SHIFT = 27,
    DATASPACE_RANGE_MASK = 939524096,      // (7 << RANGE_SHIFT)
    DATASPACE_RANGE_UNSPECIFIED = 0,       // (0 << RANGE_SHIFT)
    DATASPACE_RANGE_FULL = 134217728,      // (1 << RANGE_SHIFT)
    DATASPACE_RANGE_LIMITED = 268435456,   // (2 << RANGE_SHIFT)
    DATASPACE_RANGE_EXTENDED = 402653184,  // (3 << RANGE_SHIFT)
    DATASPACE_SRGB_LINEAR = 512,
    DATASPACE_V0_SRGB_LINEAR = 138477568,  // ((STANDARD_BT709 | TRANSFER_LINEAR) | RANGE_FULL)
    DATASPACE_V0_SCRGB_LINEAR =
        406913024,  // ((STANDARD_BT709 | TRANSFER_LINEAR) | RANGE_EXTENDED)
    DATASPACE_SRGB = 513,
    DATASPACE_V0_SRGB = 142671872,   // ((STANDARD_BT709 | TRANSFER_SRGB) | RANGE_FULL)
    DATASPACE_V0_SCRGB = 411107328,  // ((STANDARD_BT709 | TRANSFER_SRGB) | RANGE_EXTENDED)
    DATASPACE_JFIF = 257,
    DATASPACE_V0_JFIF = 146931712,  // ((STANDARD_BT601_625 | TRANSFER_SMPTE_170M) | RANGE_FULL)
    DATASPACE_BT601_625 = 258,
    DATASPACE_V0_BT601_625 =
        281149440,  // ((STANDARD_BT601_625 | TRANSFER_SMPTE_170M) | RANGE_LIMITED)
    DATASPACE_BT601_525 = 259,
    DATASPACE_V0_BT601_525 =
        281280512,  // ((STANDARD_BT601_525 | TRANSFER_SMPTE_170M) | RANGE_LIMITED)
    DATASPACE_BT709 = 260,
    DATASPACE_V0_BT709 = 281083904,  // ((STANDARD_BT709 | TRANSFER_SMPTE_170M) | RANGE_LIMITED)
    DATASPACE_DCI_P3_LINEAR = 139067392,  // ((STANDARD_DCI_P3 | TRANSFER_LINEAR) | RANGE_FULL)
    DATASPACE_DCI_P3 = 155844608,  // ((STANDARD_DCI_P3 | TRANSFER_GAMMA2_6) | RANGE_FULL)
    DATASPACE_DISPLAY_P3_LINEAR =
        139067392,                         // ((STANDARD_DCI_P3 | TRANSFER_LINEAR) | RANGE_FULL)
    DATASPACE_DISPLAY_P3 = 143261696,  // ((STANDARD_DCI_P3 | TRANSFER_SRGB) | RANGE_FULL)
    DATASPACE_ADOBE_RGB = 151715840,  // ((STANDARD_ADOBE_RGB | TRANSFER_GAMMA2_2) | RANGE_FULL)
    DATASPACE_BT2020_LINEAR = 138805248,  // ((STANDARD_BT2020 | TRANSFER_LINEAR) | RANGE_FULL)
    DATASPACE_BT2020 = 147193856,     // ((STANDARD_BT2020 | TRANSFER_SMPTE_170M) | RANGE_FULL)
    DATASPACE_BT2020_PQ = 163971072,  // ((STANDARD_BT2020 | TRANSFER_ST2084) | RANGE_FULL)
    DATASPACE_DEPTH = 4096,
    DATASPACE_SENSOR = 4097,
} sdrv_dataspace_t;

typedef enum {
    COLOR_MODE_NATIVE = 0,
    COLOR_MODE_STANDARD_BT601_625 = 1,
    COLOR_MODE_STANDARD_BT601_625_UNADJUSTED = 2,
    COLOR_MODE_STANDARD_BT601_525 = 3,
    COLOR_MODE_STANDARD_BT601_525_UNADJUSTED = 4,
    COLOR_MODE_STANDARD_BT709 = 5,
    COLOR_MODE_DCI_P3 = 6,
    COLOR_MODE_SRGB = 7,
    COLOR_MODE_ADOBE_RGB = 8,
    COLOR_MODE_DISPLAY_P3 = 9,
} sdrv_color_mode_t;

typedef enum {
    COLOR_TRANSFORM_IDENTITY = 0,
    COLOR_TRANSFORM_ARBITRARY_MATRIX = 1,
    COLOR_TRANSFORM_VALUE_INVERSE = 2,
    COLOR_TRANSFORM_GRAYSCALE = 3,
    COLOR_TRANSFORM_CORRECT_PROTANOPIA = 4,
    COLOR_TRANSFORM_CORRECT_DEUTERANOPIA = 5,
    COLOR_TRANSFORM_CORRECT_TRITANOPIA = 6,
} sdrv_color_transform_t;

enum {
	DRM_MODE_BLEND_PIXEL_NONE = 0,
	DRM_MODE_BLEND_PREMULTI,
	DRM_MODE_BLEND_COVERAGE
};

typedef enum {
    HDR_DOLBY_VISION = 1,
    HDR_HDR10 = 2,
    HDR_HLG = 3,
} sdrv_hdr_t;

struct pipe_capability {
	const u32 *formats;
	int nformats;
	int layer_type;
	int rotation;
	int scaling;
	int yuv;
	int yuv_fbc;
	int xfbc;
	const uint32_t *dataspaces;
	int num_dataspaces;
};

struct sdrv_pipe;
struct pipe_operation {
	int (*init)(struct sdrv_pipe *pipe);
	int (*update)(struct sdrv_pipe *pipe, struct dpc_layer *layer);
	int (*check)(struct sdrv_pipe *pipe, struct dpc_layer *layer);
	int (*disable)(struct sdrv_pipe *pipe);
	int (*sw_reset)(struct sdrv_pipe *pipe);
	int (*trigger)(struct sdrv_pipe *pipe);
};

struct sdrv_pipe {
	int id; // the ordered id from 0
	struct sdrv_dpc *dpc;
	enum drm_plane_type type;
	void __iomem *regs;
	const char *name;
	struct pipe_operation *ops;
	struct pipe_capability *cap;
	struct dpc_layer layer_data;
	struct sdrv_pipe *next;
	bool enable_status;
	bool fbdc_status;
	int current_dataspace;
};

struct plane_config {
	struct drm_property *alpha_property;
	struct drm_property *blend_mode_property;
	struct drm_property *fbdc_hsize_y_property;
	struct drm_property *fbdc_hsize_uv_property;
	struct drm_property *cap_mask_property;
	struct drm_property *pd_type_property;
	struct drm_property *pd_mode_property;
	struct drm_property *dataspace_property;
	struct drm_property_blob *dataspace_blob;
};

struct kunlun_plane {
	struct pipe_capability *cap;
	struct kunlun_crtc *kcrtc;
	uint8_t plane_status;
	struct plane_config config;
	uint8_t alpha;
	uint8_t blend_mode;
	uint32_t fbdc_hsize_y;
	uint32_t fbdc_hsize_uv;
	uint32_t cap_mask;
	uint32_t dataspace;
	int pd_type;
	int pd_mode;
	struct sdrv_pipe *pipes[2];
	int num_pipe;
	enum drm_plane_type type;
	struct drm_plane base;
};

struct ops_entry {
	const char *ver;
	void *ops;
};

struct ops_list {
	struct list_head head;
	struct ops_entry *entry;
};

extern struct list_head pipe_list_head;
int ops_register(struct ops_entry *entry, struct list_head *head);
void *ops_attach(const char *str, struct list_head *head);

#define pipe_ops_register(entry)	ops_register(entry, &pipe_list_head)
#define pipe_ops_attach(str)	ops_attach(str, &pipe_list_head)

extern struct ops_entry apipe_entry;
extern struct ops_entry spipe_entry;
extern struct ops_entry gpipe_dc_entry;
extern struct ops_entry gpipe1_dp_entry;
extern struct ops_entry gpipe0_dp_entry;

int kunlun_planes_init_primary(struct kunlun_crtc *kcrtc,
		struct drm_plane **primary, struct drm_plane **cursor);
int kunlun_planes_init_overlay(struct kunlun_crtc *kcrtc);
void kunlun_planes_fini(struct kunlun_crtc *kcrtc);
#endif //_sdrv_plane_H_

