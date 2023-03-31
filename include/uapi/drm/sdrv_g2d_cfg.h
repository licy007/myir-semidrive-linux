#ifndef __SDRV_G2D_CFG_H
#define __SDRV_G2D_CFG_H

#include "sdrv_drm.h"

#ifdef __YOCTO_G2D_TEST__
typedef    __u8    uint8_t;
typedef    __u16    uint16_t;
typedef    __u32    uint32_t;
typedef    unsigned long    uint64_t;
#endif

#define G2D_LAYER_MAX_NUM       6

#ifndef G2DLITE_API_USE
typedef enum {
	SWAP_A_RGB          = 0b0000,
	SWAP_A_RBG          = 0b0001,
	SWAP_A_GBR          = 0b0010,
	SWAP_A_GRB          = 0b0011,
	SWAP_A_BGR          = 0b0100,
	SWAP_A_BRG          = 0b0101,
	SWAP_B_ARG          = 0b1000,
	SWAP_B_AGR          = 0b1001,
	SWAP_B_RGA          = 0b1010,
	SWAP_B_RAG          = 0b1011,
	SWAP_B_GRA          = 0b1100,
	SWAP_B_GAR          = 0b1101
} COMP_SWAP_MODE;

typedef enum {
	UV_YUV444_RGB       = 0b00,
	UV_YUV422           = 0b01,
	UV_YUV440           = 0b10,
	UV_YUV420           = 0b11
} DATA_UV_MODE;

typedef enum {
	LINEAR_MODE             = 0b000,
	RLE_COMPR_MODE          = 0b001,
	GPU_RAW_TILE_MODE       = 0b010,
	GPU_CPS_TILE_MODE       = 0b011,
	VPU_RAW_TILE_MODE       = 0b100,
	VPU_CPS_TILE_MODE       = 0b101,
	VPU_RAW_TILE_988_MODE   = 0b110,
} DATA_MODE;

typedef enum {
	FMT_INTERLEAVED     = 0b00,
	FMT_MONOTONIC       = 0b01,
	FMT_SEMI_PLANAR     = 0b10,
	FMT_PLANAR          = 0b11
} FRM_BUF_STR_FMT;

typedef enum {
	ROT_DEFAULT         = 0b000,
	ROT_ROT             = 0b001,
	ROT_VFLIP           = 0b010,
	ROT_HFLIP           = 0b100
} ROT_TYPE;
#endif

#ifndef G2DLITE_API_USE
enum {
	BLEND_PIXEL_NONE = 0,
	BLEND_PIXEL_PREMULTI,
	BLEND_PIXEL_COVERAGE
};

typedef enum {
	ROTATION_TYPE_NONE    = 0b000,
	ROTATION_TYPE_ROT_90  = 0b001,
	ROTATION_TYPE_HFLIP   = 0b010,
	ROTATION_TYPE_VFLIP   = 0b100,
	ROTATION_TYPE_ROT_180 = ROTATION_TYPE_VFLIP | ROTATION_TYPE_HFLIP,
	ROTATION_TYPE_ROT_270 = ROTATION_TYPE_ROT_90 | ROTATION_TYPE_VFLIP | ROTATION_TYPE_HFLIP,
	ROTATION_TYPE_VF_90   = ROTATION_TYPE_VFLIP | ROTATION_TYPE_ROT_90,
	ROTATION_TYPE_HF_90   = ROTATION_TYPE_HFLIP | ROTATION_TYPE_ROT_90,
} rotation_type;
#endif

typedef enum {
    PD_NONE = 0,
    PD_SRC = 0x1,
    PD_DST = 0x2
} PD_LAYER_TYPE;

struct g2d_output_cfg{
	uint32_t width;
	uint32_t height;
	uint32_t fmt;
	uint64_t addr[4];
	uint32_t stride[4];
	uint32_t rotation;
	uint32_t nplanes;
	uint32_t offsets[4];
	struct tile_ctx out_ctx;
	struct g2d_buf_info out_buf[4];
	struct g2d_buf bufs[4];
};

struct g2d_bg_cfg {
	uint32_t en;
	uint32_t color;
	uint8_t g_alpha;
	uint8_t zorder;
	uint64_t aaddr;
	uint8_t bpa;
	uint32_t astride;
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	PD_LAYER_TYPE pd_type;
	struct g2d_buf_info cfg_buf;
	struct g2d_buf abufs;
};

struct g2d_coeff_table {
	int set_tables;
	int hcoef_set;
	int hcoef[33][5];
	int vcoef_set;
	int vcoef[33][4];
	int csc_coef_set;
	int csc_coef[15];
};

struct g2d_input{
	unsigned char layer_num;
	struct g2d_bg_cfg bg_layer;
	struct g2d_layer layer[G2D_LAYER_MAX_NUM];
	struct g2d_output_cfg output;
	struct g2d_coeff_table tables;
};

struct g2d_pipe_capability {
	uint32_t formats[100];
	int nformats;
	int layer_type;
	int rotation;
	int scaling;
	int yuv;
	int yuv_fbc;
	int xfbc;
};

struct g2d_capability {
	int num_pipe;
	struct g2d_pipe_capability pipe_caps[G2D_LAYER_MAX_NUM];
};

struct g2d_layer_x {
	__u8 index; //plane index
	__u8 enable;
	__u8 nplanes;
	__u32 addr_l[4];
	__u32 addr_h[4];
	__u32 pitch[4];
	__u32 offsets[4];
	__s16 src_x;
	__s16 src_y;
	__s16 src_w;
	__s16 src_h;
	__s16 dst_x;
	__s16 dst_y;
	__u16 dst_w;
	__u16 dst_h;
	__u32 format;
	struct pix_g2dcomp comp;
	struct tile_ctx ctx;
	__u32 alpha;
	__u32 blend_mode;
	__u32 rotation;
	__u32 zpos;
	__u32 xfbc;
	__u64 modifier;
	__u32 width;
	__u32 height;
	struct g2d_buf_info in_buf[4];
};

struct g2d_output_cfg_x{
	uint32_t width;
	uint32_t height;
	uint32_t fmt;
	uint64_t addr[4];
	uint32_t stride[4];
	uint32_t rotation;
	uint32_t nplanes;
	uint32_t offsets[4];
	struct tile_ctx out_ctx;
	struct g2d_buf_info out_buf[4];
};

struct g2d_bg_cfg_x {
	uint32_t en;
	uint32_t color;
	uint8_t g_alpha;
	uint8_t zorder;
	uint64_t aaddr;
	uint8_t bpa;
	uint32_t astride;
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	PD_LAYER_TYPE pd_type;
	struct g2d_buf_info cfg_buf;
};

struct g2d_inputx{
	unsigned char layer_num;
	struct g2d_bg_cfg_x bg_layer;
	struct g2d_layer_x layer[G2D_LAYER_MAX_NUM];
	struct g2d_output_cfg_x output;
	struct g2d_coeff_table tables;
};
#define G2D_COMMAND_BASE 0x00
#define G2D_IOCTL_BASE			'g'
#define G2D_IO(nr)			_IO(G2D_IOCTL_BASE,nr)
#define G2D_IOR(nr,type)		_IOR(G2D_IOCTL_BASE,nr,type)
#define G2D_IOW(nr,type)		_IOW(G2D_IOCTL_BASE,nr,type)
#define G2D_IOWR(nr,type)		_IOWR(G2D_IOCTL_BASE,nr,type)

#define G2D_IOCTL_GET_CAPABILITIES 		G2D_IOWR(G2D_COMMAND_BASE + 1, struct g2d_capability)
#define G2D_IOCTL_POST_CONFIG 		G2D_IOWR(G2D_COMMAND_BASE + 2, struct g2d_inputx)
#define G2D_IOCTL_FAST_COPY 		G2D_IOWR(G2D_COMMAND_BASE + 3, struct g2d_inputx)
#define G2D_IOCTL_FILL_RECT 		G2D_IOWR(G2D_COMMAND_BASE + 4, struct g2d_inputx)

#endif //__SDRV_G2D_CFG_H
