/*
 * sdrv-mipi-csi2.h
 *
 * Semidrive platform mipi csi2 header file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef SDRV_MIPI_CSI2_H
#define SDRV_MIPI_CSI2_H

#include <linux/clk.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>
#include <linux/phy/phy.h>
//#include <media/v4l2-dv-timings.h>
//#include <linux/io.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include "sdrv-csi.h"
#define SDRV_MIPI_CSI2_NAME           "sdrv-mipi-csi2"
#define SDRV_MIPI_CSI2_NAME_SIDEB           "sdrv-mipi-csi2-sideb"


#define SDRV_MIPI_CSI2_VC0_PAD_SINK       0
#define SDRV_MIPI_CSI2_VC1_PAD_SINK       1
#define SDRV_MIPI_CSI2_VC2_PAD_SINK       2
#define SDRV_MIPI_CSI2_VC3_PAD_SINK       3
#define SDRV_MIPI_CSI2_VC0_PAD_SRC        4
#define SDRV_MIPI_CSI2_VC1_PAD_SRC        5
#define SDRV_MIPI_CSI2_VC2_PAD_SRC        6
#define SDRV_MIPI_CSI2_VC3_PAD_SRC        7
#define SDRV_MIPI_CSI2_PAD_NUM        8


#define CSI_MAX_ENTITIES    2

#define SDRV_MIPI_CSI2_IPI_NUM        4


/* data type */
#define DT_YUV420_LE_8_O 0x18
#define DT_YUV420_10_O 0x19
#define DT_YUV420_LE_8_E 0x1C
#define DT_YUV420_10_E 0x1D
#define DT_YUV422_8 0x1E
#define DT_YUV422_10 0x1F
#define DT_RGB444 0x20
#define DT_RGB555 0x21
#define DT_RGB565 0x22
#define DT_RGB666 0x23
#define DT_RGB888 0x24
#define DT_RAW8 0x2A
#define DT_RAW10 0x2B

/* ipi mode */
#define IPI_MODE_ENABLE 1<<24
#define IPI_MODE_CUT_THROUGH 1<<16
#define IPI_MODE_AUTO_TIMING 1<<0

/* ipi advance features */
#define IPI_ADV_SYNC_LEGACCY 1<<24
#define IPI_ADV_USE_EMBEDDED 1<<21
#define IPI_ADV_USE_BLANKING 1<<20
#define IPI_ADV_USE_NULL 1<<19
#define IPI_ADV_USE_LINE_START 1<<18
#define IPI_ADV_USE_VIDEO 1<<17
#define IPI_ADV_SEL_LINE_EVENT 1<<16

/* ipi soft reset*/
#define IPI4_SOFTRSTN (1 << 12)
#define IPI3_SOFTRSTN (1 << 8)
#define IPI2_SOFTRSTN (1 << 4)
#define IPI_SOFTRSTN  (1 << 0)

/** @short DWC MIPI CSI-2 register addresses*/
enum register_addresses {
	R_CSI2_VERSION = 0x00,
	R_CSI2_N_LANES = 0x04,
	R_CSI2_CTRL_RESETN = 0x08,
	R_CSI2_INTERRUPT = 0x0C,
	R_CSI2_DATA_IDS_1 = 0x10,
	R_CSI2_DATA_IDS_2 = 0x14,
	R_CSI2_INT_ST_AP_MAIN = 0x2C,

	R_CSI2_DPHY_SHUTDOWNZ = 0x40,
	R_CSI2_DPHY_RSTZ = 0x44,
	R_CSI2_DPHY_RX = 0x48,
	R_CSI2_DPHY_STOPSTATE = 0x4C,
	R_CSI2_DPHY_TST_CTRL0 = 0x50,
	R_CSI2_DPHY_TST_CTRL1 = 0x54,

	R_CSI2_IPI_MODE = 0x80,
	R_CSI2_IPI_VCID = 0x84,
	R_CSI2_IPI_DATA_TYPE = 0x88,
	R_CSI2_IPI_MEM_FLUSH = 0x8C,
	R_CSI2_IPI_HSA_TIME = 0x90,
	R_CSI2_IPI_HBP_TIME = 0x94,
	R_CSI2_IPI_HSD_TIME = 0x98,
	R_CSI2_IPI_HLINE_TIME = 0x9C,
	R_CSI2_IPI_SOFTRSTN = 0xA0,
	R_CSI2_IPI_ADV_FEATURES = 0xAC,
	R_CSI2_IPI_VSA_LINES = 0xB0,
	R_CSI2_IPI_VBP_LINES = 0xB4,
	R_CSI2_IPI_VFP_LINES = 0xB8,
	R_CSI2_IPI_VACTIVE_LINES = 0xBC,
	R_CSI2_PHY_CAL = 0Xcc,
	R_CSI2_INT_PHY_FATAL = 0xe0,
	R_CSI2_MASK_INT_PHY_FATAL = 0xe4,
	R_CSI2_FORCE_INT_PHY_FATAL = 0xe8,
	R_CSI2_INT_PKT_FATAL = 0xf0,
	R_CSI2_MASK_INT_PKT_FATAL = 0xf4,
	R_CSI2_FORCE_INT_PKT_FATAL = 0xf8,
	R_CSI2_INT_FRAME_FATAL = 0x100,
	R_CSI2_MASK_INT_FRAME_FATAL = 0x104,
	R_CSI2_FORCE_INT_FRAME_FATAL = 0x108,
	R_CSI2_INT_PHY = 0x110,
	R_CSI2_MASK_INT_PHY = 0x114,
	R_CSI2_FORCE_INT_PHY = 0x118,
	R_CSI2_INT_PKT = 0x120,
	R_CSI2_MASK_INT_PKT = 0x124,
	R_CSI2_FORCE_INT_PKT = 0x128,
	R_CSI2_INT_LINE = 0x130,
	R_CSI2_MASK_INT_LINE = 0x134,
	R_CSI2_FORCE_INT_LINE = 0x138,
	R_CSI2_INT_IPI = 0x140,
	R_CSI2_MASK_INT_IPI = 0x144,
	R_CSI2_FORCE_INT_IPI = 0x148,
	R_CSI2_INT_IPI2 = 0x150,
	R_CSI2_MASK_INT_IPI2 = 0x154,
	R_CSI2_FORCE_INT_IPI2 = 0x158,
	R_CSI2_INT_IPI3 = 0x160,
	R_CSI2_MASK_INT_IPI3 = 0x164,
	R_CSI2_FORCE_INT_IPI3 = 0x168,
	R_CSI2_INT_IPI4 = 0x170,
	R_CSI2_MASK_INT_IPI4 = 0x174,
	R_CSI2_FORCE_INT_IPI4 = 0x178,

	R_CSI2_IPI2_MODE = 0x200,
	R_CSI2_IPI2_VCID = 0x204,
	R_CSI2_IPI2_DATA_TYPE = 0X208,
	R_CSI2_IPI2_MEM_FLUSH = 0x20c,
	R_CSI2_IPI2_HSA_TIME = 0X210,
	R_CSI2_IPI2_HBP_TIME = 0X214,
	R_CSI2_IPI2_HSD_TIME = 0X218,
	R_CSI2_IPI2_ADV_FEATURES = 0X21C,

	R_CSI2_IPI3_MODE = 0X220,
	R_CSI2_IPI3_VCID = 0X224,
	R_CSI2_IPI3_DATA_TYPE = 0X228,
	R_CSI2_IPI3_MEM_FLUSH = 0X22C,
	R_CSI2_IPI3_HSA_TIME = 0X230,
	R_CSI2_IPI3_HBP_TIME = 0X234,
	R_CSI2_IPI3_HSD_TIME = 0X238,
	R_CSI2_IPI3_ADV_FEATURES = 0X23C,

	R_CSI2_IPI4_MODE = 0X240,
	R_CSI2_IPI4_VCID = 0X244,
	R_CSI2_IPI4_DATA_TYPE = 0X248,
	R_CSI2_IPI4_MEM_FLUSH = 0X24C,
	R_CSI2_IPI4_HSA_TIME = 0X250,
	R_CSI2_IPI4_HBP_TIME = 0X254,
	R_CSI2_IPI4_HSD_TIME = 0X258,
	R_CSI2_IPI4_ADV_FEATURES = 0X25C,

	R_CSI2_IPI_RAM_ERR_LOG_AP = 0X2E0,
	R_CSI2_IPI_RAM_ERR_ADDR_AP = 0X2E4,
	R_CSI2_IPI2_RAM_ERR_LOG_AP = 0X2E8,
	R_CSI2_IPI2_RAM_ERR_ADDR_AP = 0X2EC,
	R_CSI2_IPI3_RAM_ERR_LOG_AP = 0X2F0,
	R_CSI2_IPI3_RAM_ERR_ADDR_AP = 0X2F4,
	R_CSI2_IPI4_RAM_ERR_LOG_AP = 0X2F8,
	R_CSI2_IPI4_RAM_ERR_ADDR_AP = 0X2FC,

	R_CSI2_SCRAMBLING = 0X300,
	R_CSI2_SCRAMBLING_SEED1 = 0X304,
	R_CSI2_SCRAMBLING_SEED2 = 0X308,
	R_CSI2_SCRAMBLING_SEED3 = 0X30C,
	R_CSI2_SCRAMBLING_SEED4 = 0x310,
	R_CSI2_N_SYNC = 0X340,
	R_CSI2_ERR_INJ_CTRL_AP = 0X344,
	R_CSI2_ERR_INJ_CHK_MSK_AP = 0X348,
	R_CSI2_ERR_INJ_DH32_MSK_AP = 0X34C,
	R_CSI2_ERR_INJ_DL32_MSK_AP = 0X350,
	R_CSI2_ERR_INJ_ST_AP = 0X354,
	R_CSI2_ST_FAP_PHY_FATAL = 0X360,
	R_CSI2_INT_MSK_FAP_PHY_FATAL = 0X364,
	R_CSI2_INT_FORCE_FAP_PHY_FATAL = 0X368,
	R_CSI2_INT_ST_FAP_PKT_FATAL = 0X370,
	R_CSI2_INT_MSK_FAP_PKT_FATAL = 0X374,
	R_CSI2_INT_FORCE_FAP_PKT_FATAL = 0X378,
	R_CSI2_INT_ST_FAP_FRAME_FATAL = 0X380,
	R_CSI2_INT_MSK_FAP_FRAME_FATAL = 0X384,
	R_CSI2_INT_FORCE_FAP_FRAME_FATAL = 0X388,
	R_CSI2_INT_ST_FAP_PHY = 0X390,
	R_CSI2_INT_MSK_FAP_PHY = 0X394,
	R_CSI2_INT_FORCE_FAP_PHY = 0X398,
	R_CSI2_INT_ST_FAP_PKT = 0X3A0,
	R_CSI2_INT_MSK_FAP_PKT = 0X3A4,
	R_CSI2_INT_FORCE_FAP_PKT = 0X3A8,
	R_CSI2_INT_ST_FAP_LINE = 0X3B0,
	R_CSI2_INT_MSK_FAP_LINE = 0X3B4,
	R_CSI2_INT_FORCE_FAP_LINE = 0X3B8,
	R_CSI2_INT_ST_FAP_IPI = 0X3C0,
	R_CSI2_INT_MSK_FAP_IPI = 0X3C4,
	R_CSI2_INT_FORCE_FAP_IPI = 0X3C8,
	R_CSI2_INT_ST_FAP_IPI2 = 0X3D0,
	R_CSI2_INT_MSK_FAP_IPI2 = 0X3D4,
	R_CSI2_INT_FORCE_FAP_IPI2 = 0X3D8,
	R_CSI2_INT_ST_FAP_IPI3 = 0X3E0,
	R_CSI2_INT_MSK_FAP_IPI3 = 0X3E4,
	R_CSI2_INT_FORCE_FAP_IPI3 = 0X3E8,
	R_CSI2_INT_ST_FAP_IPI4 = 0X3F0,
	R_CSI2_INT_MSK_FAP_IPI4 = 0X3F4,
	R_CSI2_INT_FORCE_FAP_IPI4 = 0X3F8,
};

enum phy_fatal_interrupt_type {
	PHY_ERRSOTSYNCCHS_3 = 1 << 3,
	PHY_ERRSOTSYNCCHS_2 = 1 << 2,
	PHY_ERRSOTSYNCCHS_1 = 1 << 1,
	PHY_ERRSOTSYNCCHS_0 = 1 << 0,
};

enum pkt_fatal_interrupt_type {
	ERR_ECC_DOUBLE = 1 << 16,
	VC3_ERR_CRC = 1 << 3,
	VC2_ERR_CRC = 1 << 2,
	VC1_ERR_CRC = 1 << 1,
	VC0_ERR_CRC = 1 << 0,
};

enum frame_fatal_type {
	ERR_FRAME_DATA_VC3 = 1 << 19,
	ERR_FRAME_DATA_VC2 = 1 << 18,
	ERR_FRAME_DATA_VC1 = 1 << 17,
	ERR_FRAME_DATA_VC0 = 1 << 16,
	ERR_F_SEQ_VC3 = 1 << 11,
	ERR_F_SEQ_VC2 = 1 << 10,
	ERR_F_SEQ_VC1 = 1 << 9,
	ERR_F_SEQ_VC0 = 1 << 8,
	ERR_F_BNDRY_MATCH_VC3 = 1 << 3,
	ERR_F_BNDRY_MATCH_VC2 = 1 << 2,
	ERR_F_BNDRY_MATCH_VC1 = 1 << 1,
	ERR_F_BNDRY_MATCH_VC0 = 1 << 0,
};

enum phy_fatal_interrupt_type_2 {
	PHY_ERRESC_3 = 1 << 19,
	PHY_ERRESC_2 = 1 << 18,
	PHY_ERRESC_1 = 1 << 17,
	PHY_ERRESC_0 = 1 << 16,
	PHY_ERRSOTHS_3 = 1 << 3,
	PHY_ERRSOTHS_2 = 1 << 2,
	PHY_ERRSOTHS_1 = 1 << 1,
	PHY_ERRSOTHS_0 = 1 << 0,
};

enum pkt_fatal_interrupt_type_2 {
	VC3_ERR_ECC_CORRECTED = 1 << 19,
	VC2_ERR_ECC_CORRECTED = 1 << 18,
	VC1_ERR_ECC_CORRECTED = 1 << 17,
	VC0_ERR_ECC_CORRECTED = 1 << 16,
	ERR_ID_VC3 = 1 << 3,
	ERR_ID_VC2 = 1 << 2,
	ERR_ID_VC1 = 1 << 1,
	ERR_ID_VC0 = 1 << 0,
};

enum ipi_interrupt_type {
	INT_EVENT_FIFO_OVERFLOW = 1 << 5,
	PIXEL_IF_HLINE_ERR = 1 << 4,
	PIXEL_IF_FIFO_NEMPTY_FS = 1 << 3,
	FRAME_SYNC_ERR = 1 << 2,
	PIXEL_IF_FIFO_OVERFLOW = 1 << 1,
	PIXEL_IF_FIFO_UNDERFLOW = 1 << 0,
};

enum ap_generic_type {
	SYNCHRONIZER_PIXCLK_4IF_AP_ERR = 1 << 19,
	SYNCHRONIZER_PIXCLK_3IF_AP_ERR = 1 << 18,
	SYNCHRONIZER_PIXCLK_2IF_AP_ERR = 1 << 17,
	SYNCHRONIZER_PIXCLK_AP_ERR = 1 << 16,
	SYNCHRONIZER_RXBYTECLKHS_AP_ERR = 1 << 15,
	SYNCHRONIZER_FPCLK_AP_ERR = 1 << 14,
	ERR_MSGR_AP_ERR = 1 << 12,
	PREP_OUTS_AP_ERR = 1 << 10,
	PACKET_ANALYZER_AP_ERR = 0x3 << 8,
	PHY_ADAPTER_AP_ERR = 1 << 7,
	DESCREAMBLER_AP_ERR = 1 << 6,
	DE_SKEW_AP_ERR = 1 << 4,
	REG_BANK_AP_ERR = 0x3 << 2,
	APB_AP_ERR = 0x3 << 0,
};

enum ap_ipi_type {
	REDUNDANCY_ERR = 1 << 5,
	CRC_ERR = 1 << 4,
	ECC_MULTIPLE_ERR = 1 << 3,
	ECC_SINGLE_ERR = 1 << 2,
	PARITY_RX_ERR = 1 << 1,
	PARITY_TX_ERR = 1 << 0,
};

/** @short Interrupt Masks */
enum interrupt_type {
	CSI2_INT_PHY_FATAL = 1 << 0,
	CSI2_INT_PKT_FATAL = 1 << 1,
	CSI2_INT_FRAME_FATAL = 1 << 2,
	CSI2_INT_PHY = 1 << 16,
	CSI2_INT_PKT = 1 << 17,
	CSI2_INT_LINE = 1 << 18,
	CSI2_INT_IPI = 1 << 19,
	CSI2_INT_IPI2 = 1 << 20,
	CSI2_INT_IPI3 = 1 << 21,
	CSI2_INT_IPI4 = 1 << 22,
};

/**
 * @short Format template
 */
struct mipi_fmt {
	const char *name;
	u32 code;
	u8 depth;
};

struct csi_hw {

	uint32_t num_lanes;
	uint32_t output_type;   //IPI = 0; IDI = 1; BOTH = 2
	/*IPI Info */
	uint32_t ipi_mode;
	uint32_t ipi_color_mode;
	uint32_t ipi_auto_flush;
	uint32_t virtual_ch;

	uint32_t hsa;
	uint32_t hbp;
	uint32_t hsd;
	uint32_t htotal;

	uint32_t vsa;
	uint32_t vbp;
	uint32_t vfp;
	uint32_t vactive;

	uint32_t adv_feature;
};

struct sdrv_csi_mipi_csi2 {
	u32 id;
	//void __iomem *base;
	struct device *dev;
	struct v4l2_subdev subdev;

	struct mutex lock;
	spinlock_t slock;

	struct media_pad pads[SDRV_MIPI_CSI2_PAD_NUM];
	struct platform_device *pdev;
	u8 index;


	/** Store current format */
	const struct mipi_fmt *fmt;
	struct v4l2_mbus_framefmt format;

	/** Device Tree Information */
	void __iomem *base_address;
	void __iomem *dispmux_address;
	struct resource *dispmux_res;

	uint32_t ctrl_irq_number;

	uint32_t lanerate;
	uint32_t lane_count;

	u32 active_stream_num;

	struct csi_hw hw;

	struct phy *phy;
};


struct phy_freqrange {
	int index;
	int range_l;
	int range_h;
};

#endif
