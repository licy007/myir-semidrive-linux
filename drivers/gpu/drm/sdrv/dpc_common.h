#ifndef __DPC_TYPE_H
#define __DPC_TYPE_H

#include <stdbool.h>
#include <uapi/drm/sdrv_drm.h>

#define SDRV_RDMA_MASK			BIT(0)
#define SDRV_RLE_MASK			BIT(1)
#define SDRV_MLC_MASK			BIT(2)
#define SDRV_RLE1_MASK			BIT(3)
#define SDRV_SDMA_DONE_MASK		BIT(4)
#define SDRV_TCON_SOF_MASK		BIT(5)
#define SDRV_TCON_EOF_MASK 		BIT(6)
#define SDRV_TCON_UNDERRUN_MASK	BIT(7)
#define	SDRV_DC_UNDERRUN_MASK	BIT(8)
#define	SDRV_LAYER_KICK_MASK	BIT(9)

#define MLC_JUMP            0x7000
#define S_MLC_JUMP          0x8000

enum {
	DPC_TYPE_DP = 0,
	DPC_TYPE_DC,
	DPC_TYPE_DUMMY,
	DPC_TYPE_RPCALL,
	DPC_TYPE_UNILINK,
	DPC_TYPE_VIDEO,
	DPC_TYPE_MAX
};

enum {
	DPC_SEPARATE_NONE = 0,
	DPC_SEPARATE_MASTER = 1,
	DPC_SEPARATE_SLAVE
};

enum {
	DELAY_POST_NONE = 0,
	DELAY_POST_START = 1,
	DELAY_POST_END
};

enum {
	DPC_MODE_BLEND_PIXEL_NONE = 0,
	DPC_MODE_BLEND_PREMULTI,
	DPC_MODE_BLEND_COVERAGE
};

typedef enum {
    PD_NONE = 0,
    PD_SRC = 0b01,
    PD_DST = 0b10
} PD_LAYER_TYPE;

typedef enum {
    CLEAR = 0,
    SRC,
    DST,
    SRC_OVER,
    DST_OVER,
    SRC_IN,
    DST_IN,
    SRC_OUT,
    DST_OUT,
    SRC_ATOP,
    DST_ATOP,
    XOR,
    DARKEN,
    LIGHTEN,
    MULTIPLY,
    SCREEN,
    ADD,
    OVERLAY,
    SRC_SUB,
    DES_SUB
} pd_mode_t;

struct pd_t {
    u32 en;
    u32 zorder;
    pd_mode_t mode;
    u8 alpha_need;
};

struct sdrv_dpc_mode {
	int clock;		/* in kHz */
	int hdisplay;
	int hsync_start;
	int hsync_end;
	int htotal;
	int hskew;
	int vdisplay;
	int vsync_start;
	int vsync_end;
	int vtotal;
	int vscan;
	int vrefresh;
	unsigned int flags;
};

#endif

