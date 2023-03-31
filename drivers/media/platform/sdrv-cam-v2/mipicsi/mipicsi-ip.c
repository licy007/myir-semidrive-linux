/*
 * csimipi-ip.c
 *
 * Semidrive platform mipi csi2 header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
*/

#include "mipicsi-ip.h"
#include "cam_isr_log.h"

#define SDRV_MIPI_CSI2_IPI_NUM 4

/* ipi soft reset*/
#define IPI4_SOFTRSTN (1 << 12)
#define IPI3_SOFTRSTN (1 << 8)
#define IPI2_SOFTRSTN (1 << 4)
#define IPI_SOFTRSTN (1 << 0)

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
	PHY_ERRSOTSYNCCHS_3 = (1 << 3),
	PHY_ERRSOTSYNCCHS_2 = (1 << 2),
	PHY_ERRSOTSYNCCHS_1 = (1 << 1),
	PHY_ERRSOTSYNCCHS_0 = (1 << 0),
};

enum pkt_fatal_interrupt_type {
	ERR_ECC_DOUBLE = (1 << 16),
	VC3_ERR_CRC = (1 << 3),
	VC2_ERR_CRC = (1 << 2),
	VC1_ERR_CRC = (1 << 1),
	VC0_ERR_CRC = (1 << 0),
};

enum frame_fatal_type {
	ERR_FRAME_DATA_VC3 = (1 << 19),
	ERR_FRAME_DATA_VC2 = (1 << 18),
	ERR_FRAME_DATA_VC1 = (1 << 17),
	ERR_FRAME_DATA_VC0 = (1 << 16),
	ERR_F_SEQ_VC3 = (1 << 11),
	ERR_F_SEQ_VC2 = (1 << 10),
	ERR_F_SEQ_VC1 = (1 << 9),
	ERR_F_SEQ_VC0 = (1 << 8),
	ERR_F_BNDRY_MATCH_VC3 = (1 << 3),
	ERR_F_BNDRY_MATCH_VC2 = (1 << 2),
	ERR_F_BNDRY_MATCH_VC1 = (1 << 1),
	ERR_F_BNDRY_MATCH_VC0 = (1 << 0),
};

enum phy_fatal_interrupt_type_2 {
	PHY_ERRESC_3 = (1 << 19),
	PHY_ERRESC_2 = (1 << 18),
	PHY_ERRESC_1 = (1 << 17),
	PHY_ERRESC_0 = (1 << 16),
	PHY_ERRSOTHS_3 = (1 << 3),
	PHY_ERRSOTHS_2 = (1 << 2),
	PHY_ERRSOTHS_1 = (1 << 1),
	PHY_ERRSOTHS_0 = (1 << 0),
};

enum pkt_fatal_interrupt_type_2 {
	VC3_ERR_ECC_CORRECTED = (1 << 19),
	VC2_ERR_ECC_CORRECTED = (1 << 18),
	VC1_ERR_ECC_CORRECTED = (1 << 17),
	VC0_ERR_ECC_CORRECTED = (1 << 16),
	ERR_ID_VC3 = (1 << 3),
	ERR_ID_VC2 = (1 << 2),
	ERR_ID_VC1 = (1 << 1),
	ERR_ID_VC0 = (1 << 0),
};

enum ipi_interrupt_type {
	INT_EVENT_FIFO_OVERFLOW = (1 << 5),
	PIXEL_IF_HLINE_ERR = (1 << 4),
	PIXEL_IF_FIFO_NEMPTY_FS = (1 << 3),
	FRAME_SYNC_ERR = (1 << 2),
	PIXEL_IF_FIFO_OVERFLOW = (1 << 1),
	PIXEL_IF_FIFO_UNDERFLOW = (1 << 0),
};

enum ap_generic_type {
	SYNCHRONIZER_PIXCLK_4IF_AP_ERR = (1 << 19),
	SYNCHRONIZER_PIXCLK_3IF_AP_ERR = (1 << 18),
	SYNCHRONIZER_PIXCLK_2IF_AP_ERR = (1 << 17),
	SYNCHRONIZER_PIXCLK_AP_ERR = (1 << 16),
	SYNCHRONIZER_RXBYTECLKHS_AP_ERR = (1 << 15),
	SYNCHRONIZER_FPCLK_AP_ERR = (1 << 14),
	ERR_MSGR_AP_ERR = (1 << 12),
	PREP_OUTS_AP_ERR = (1 << 10),
	PACKET_ANALYZER_AP_ERR = (0x3 << 8),
	PHY_ADAPTER_AP_ERR = (1 << 7),
	DESCREAMBLER_AP_ERR = (1 << 6),
	DE_SKEW_AP_ERR = (1 << 4),
	REG_BANK_AP_ERR = (0x3 << 2),
	APB_AP_ERR = (0x3 << 0),
};

enum ap_ipi_type {
	REDUNDANCY_ERR = (1 << 5),
	CRC_ERR = (1 << 4),
	ECC_MULTIPLE_ERR = (1 << 3),
	ECC_SINGLE_ERR = (1 << 2),
	PARITY_RX_ERR = (1 << 1),
	PARITY_TX_ERR = (1 << 0),
};

/** @short Interrupt Masks */
enum interrupt_type {
	CSI2_INT_PHY_FATAL = (1 << 0),
	CSI2_INT_PKT_FATAL = (1 << 1),
	CSI2_INT_FRAME_FATAL = (1 << 2),
	CSI2_INT_PHY = (1 << 16),
	CSI2_INT_PKT = (1 << 17),
	CSI2_INT_LINE = (1 << 18),
	CSI2_INT_IPI = (1 << 19),
	CSI2_INT_IPI2 = (1 << 20),
	CSI2_INT_IPI3 = (1 << 21),
	CSI2_INT_IPI4 = (1 << 22),
};

struct phy_freqrange {
	int index;
	int range_l;
	int range_h;
};

static const struct phy_freqrange g_phy_freqs[63] = {
	{0x00, 80, 88},     // 80
	{0x10, 88, 95},     // 90
	{0x20, 95, 105},    // 100
	{0x30, 105, 115},   // 110
	{0x01, 115, 125},   // 120
	{0x11, 125, 135},   // 130
	{0x21, 135, 145},   // 140
	{0x31, 145, 155},   // 150
	{0x02, 155, 165},   // 160
	{0x12, 165, 175},   // 170
	{0x22, 175, 185},   // 180
	{0x32, 185, 198},   // 190
	{0x03, 198, 212},   // 205
	{0x13, 212, 230},   // 220
	{0x23, 230, 240},   // 235
	{0x33, 240, 260},   // 250
	{0x04, 260, 290},   // 275
	{0x14, 290, 310},   // 300
	{0x25, 310, 340},   // 325
	{0x35, 340, 375},   // 350
	{0x05, 375, 425},   // 400
	{0x16, 425, 275},   // 450
	{0x26, 475, 525},   // 500
	{0x37, 525, 575},   // 550
	{0x07, 575, 625},   // 600
	{0x18, 625, 675},   // 650
	{0x28, 675, 725},   // 700
	{0x39, 725, 775},   // 750
	{0x09, 775, 825},   // 800
	{0x19, 825, 875},   // 850
	{0x29, 875, 925},   // 900
	{0x3a, 925, 975},   // 950
	{0x0a, 975, 1025},  // 1000
	{0x1a, 1025, 1075}, // 1050
	{0x2a, 1075, 1125}, // 1100
	{0x3b, 1125, 1175}, // 1150
	{0x0b, 1175, 1225}, // 1200
	{0x1b, 1225, 1275}, // 1250
	{0x2b, 1275, 1325}, // 1300
	{0x3c, 1325, 1375}, // 1350
	{0x0c, 1375, 1425}, // 1400
	{0x1c, 1425, 1475}, // 1450
	{0x2c, 1475, 1525}, // 1500
	{0x3d, 1525, 1575}, // 1550
	{0x0d, 1575, 1625}, // 1600
	{0x1d, 1625, 1675}, // 1650
	{0x2d, 1675, 1725}, // 1700
	{0x3e, 1725, 1775}, // 1750
	{0x0e, 1775, 1825}, // 1800
	{0x1e, 1825, 1875}, // 1850
	{0x2f, 1875, 1925}, // 1900
	{0x3f, 1925, 1975}, // 1950
	{0x0f, 1975, 2025}, // 2000
	{0x40, 2025, 2075}, // 2050
	{0x41, 2075, 2125}, // 2100
	{0x42, 2125, 2175}, // 2150
	{0x43, 2175, 2225}, // 2200
	{0x44, 2225, 2275}, // 2250
	{0x45, 2275, 2325}, // 2300
	{0x46, 2325, 2375}, // 2350
	{0x47, 2375, 2425}, // 2400
	{0x48, 2425, 2475}, // 2450
	{0x49, 2475, 2500}, // 2500
};

static void dw_mipi_csi_write(struct csi_mipi_dev *core, uint32_t reg,
			uint32_t data)
{
	reg_write(core->base, reg, data);
}

static uint32_t dw_mipi_csi_read(struct csi_mipi_dev *core, uint32_t reg)
{
	return reg_read(core->base, reg);
}

static uint32_t check_version(struct csi_mipi_dev *core)
{
	uint32_t version;

	version = dw_mipi_csi_read(core, R_CSI2_VERSION);

	if (version != 0x3133302A) {
		csi_err("mipi_csi2 version error: val=0x%08x, should be 0x3133302A\n",
					version);
		return -1;
	}

	return 0;
}

static int set_phy_freq(struct csi_mipi_dev *core, uint32_t freq,
				uint32_t lanes)
{
	int phyrate;
	int i;

	phyrate = freq / 1000 / 1000;

	csi_info("%s(): lanerate=%d MBps\n", __func__, phyrate);

	if ((phyrate < g_phy_freqs[0].range_l) ||
		(phyrate > g_phy_freqs[62].range_h)) {
		csi_err("err phy freq\n");
		return -1;
	}

	for (i = 0; i < 63; i++) {
		if ((phyrate >= g_phy_freqs[i].range_l) &&
			(phyrate < g_phy_freqs[i].range_h))
			break;
	}

	csi_info("%s(): i=%d, index=%d, range[%d, %d]\n", __func__, i,
	g_phy_freqs[i].index, g_phy_freqs[i].range_l,
	g_phy_freqs[i].range_h);

	if (core->id == 0 || core->id == 2)
		reg_write(core->dispmux_base, 0x60, g_phy_freqs[i].index);
	else
		reg_write(core->dispmux_base, 0x68, g_phy_freqs[i].index);

	return 0;
}

static int dw_enable_mipi_csi_irq_common(struct csi_mipi_dev *core)
{
	uint32_t value;

	/*PHY FATAL*/
	value = PHY_ERRSOTSYNCCHS_3 | PHY_ERRSOTSYNCCHS_2;
	value |= PHY_ERRSOTSYNCCHS_1 | PHY_ERRSOTSYNCCHS_0;
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_PHY_FATAL, value);

	/*PKT FATAL*/
	value = ERR_ECC_DOUBLE | VC3_ERR_CRC;
	value |= VC2_ERR_CRC | VC1_ERR_CRC | VC0_ERR_CRC;
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_PKT_FATAL, value);

	/*FRAME FATAL*/
	value = ERR_FRAME_DATA_VC3;
	value |= ERR_FRAME_DATA_VC2;
	value |= ERR_FRAME_DATA_VC1;
	value |= ERR_FRAME_DATA_VC0;
	value |= ERR_F_SEQ_VC3;
	value |= ERR_F_SEQ_VC2;
	value |= ERR_F_SEQ_VC1;
	value |= ERR_F_SEQ_VC0;
	value |= ERR_F_BNDRY_MATCH_VC3;
	value |= ERR_F_BNDRY_MATCH_VC2;
	value |= ERR_F_BNDRY_MATCH_VC1;
	value |= ERR_F_BNDRY_MATCH_VC0;
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_FRAME_FATAL, value);

	/*PHY FATAL*/
	value = PHY_ERRESC_3;
	value |= PHY_ERRESC_2;
	value |= PHY_ERRESC_1;
	value |= PHY_ERRESC_0;
	value |= PHY_ERRSOTHS_3;
	value |= PHY_ERRSOTHS_2;
	value |= PHY_ERRSOTHS_1;
	value |= PHY_ERRSOTHS_0;
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_PHY, value);

	/*IPI FATAL*/
	value = INT_EVENT_FIFO_OVERFLOW;
	value |= PIXEL_IF_HLINE_ERR;
	value |= PIXEL_IF_FIFO_NEMPTY_FS;
	value |= FRAME_SYNC_ERR;
	value |= PIXEL_IF_FIFO_UNDERFLOW;
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI, value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI2, value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI3, value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI4, value);

	return 0;
}

static int dw_disable_mipi_csi_irq_common(struct csi_mipi_dev *core)
{
	uint32_t value;

	/*PHY FATAL*/
	value = PHY_ERRSOTSYNCCHS_3 | PHY_ERRSOTSYNCCHS_2;
	value |= PHY_ERRSOTSYNCCHS_1 | PHY_ERRSOTSYNCCHS_0;
	value = dw_mipi_csi_read(core, R_CSI2_MASK_INT_PHY_FATAL) & (~value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_PHY_FATAL, value);

	/*PKT FATAL*/
	value = ERR_ECC_DOUBLE | VC3_ERR_CRC;
	value |= VC2_ERR_CRC | VC1_ERR_CRC | VC0_ERR_CRC;
	value = dw_mipi_csi_read(core, R_CSI2_MASK_INT_PHY_FATAL) & (~value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_PKT_FATAL, value);

	/*FRAME FATAL*/
	value = ERR_FRAME_DATA_VC3;
	value |= ERR_FRAME_DATA_VC2;
	value |= ERR_FRAME_DATA_VC1;
	value |= ERR_FRAME_DATA_VC0;
	value |= ERR_F_SEQ_VC3;
	value |= ERR_F_SEQ_VC2;
	value |= ERR_F_SEQ_VC1;
	value |= ERR_F_SEQ_VC0;
	value |= ERR_F_BNDRY_MATCH_VC3;
	value |= ERR_F_BNDRY_MATCH_VC2;
	value |= ERR_F_BNDRY_MATCH_VC1;
	value |= ERR_F_BNDRY_MATCH_VC0;
	value = dw_mipi_csi_read(core, R_CSI2_MASK_INT_FRAME_FATAL) & (~value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_FRAME_FATAL, value);

	/*PHY FATAL*/
	value = PHY_ERRESC_3;
	value |= PHY_ERRESC_2;
	value |= PHY_ERRESC_1;
	value |= PHY_ERRESC_0;
	value |= PHY_ERRSOTHS_3;
	value |= PHY_ERRSOTHS_2;
	value |= PHY_ERRSOTHS_1;
	value |= PHY_ERRSOTHS_0;
	value = dw_mipi_csi_read(core, R_CSI2_MASK_INT_PHY) & (~value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_PHY, value);

	/*IPI FATAL*/
	value = INT_EVENT_FIFO_OVERFLOW;
	value |= PIXEL_IF_HLINE_ERR;
	value |= PIXEL_IF_FIFO_NEMPTY_FS;
	value |= FRAME_SYNC_ERR;
	value |= PIXEL_IF_FIFO_UNDERFLOW;
	value = dw_mipi_csi_read(core, R_CSI2_MASK_INT_IPI) & (~value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI, value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI2, value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI3, value);
	dw_mipi_csi_write(core, R_CSI2_MASK_INT_IPI4, value);

	return 0;
}

static void set_lane_number(struct csi_mipi_dev *core, int cnt)
{
	dw_mipi_csi_write(core, R_CSI2_N_LANES, cnt - 1);
}

static void set_ipi_mode(struct csi_mipi_dev *core, int index, int val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(core, R_CSI2_IPI_MODE, val);
		break;

	case 2:
		dw_mipi_csi_write(core, R_CSI2_IPI2_MODE, val);
		break;

	case 3:
		dw_mipi_csi_write(core, R_CSI2_IPI3_MODE, val);
		break;

	case 4:
		dw_mipi_csi_write(core, R_CSI2_IPI4_MODE, val);
		break;

	default:
		break;
	}
}

static void set_ipi_vcid(struct csi_mipi_dev *core, int index, uint32_t vch)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(core, R_CSI2_IPI_VCID, vch);
		break;

	case 2:
		dw_mipi_csi_write(core, R_CSI2_IPI2_VCID, vch);
		break;

	case 3:
		dw_mipi_csi_write(core, R_CSI2_IPI3_VCID, vch);
		break;

	case 4:
		dw_mipi_csi_write(core, R_CSI2_IPI4_VCID, vch);
		break;

	default:
		break;
	}
}

static void set_ipi_data_type(struct csi_mipi_dev *core, int index,
				uint8_t type)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(core, R_CSI2_IPI_DATA_TYPE, type);
		break;

	case 2:
		dw_mipi_csi_write(core, R_CSI2_IPI2_DATA_TYPE, type);
		break;

	case 3:
		dw_mipi_csi_write(core, R_CSI2_IPI3_DATA_TYPE, type);
		break;

	case 4:
		dw_mipi_csi_write(core, R_CSI2_IPI4_DATA_TYPE, type);
		break;

	default:
		break;
	}
}

static void set_ipi_hsa(struct csi_mipi_dev *core, int index, uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(core, R_CSI2_IPI_HSA_TIME, val);
		break;

	case 2:
		dw_mipi_csi_write(core, R_CSI2_IPI2_HSA_TIME, val);
		break;

	case 3:
		dw_mipi_csi_write(core, R_CSI2_IPI3_HSA_TIME, val);
		break;

	case 4:
		dw_mipi_csi_write(core, R_CSI2_IPI4_HSA_TIME, val);
		break;

	default:
		break;
	}
}

static void set_ipi_hbp(struct csi_mipi_dev *core, int index, uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(core, R_CSI2_IPI_HBP_TIME, val);
		break;

	case 2:
		dw_mipi_csi_write(core, R_CSI2_IPI2_HBP_TIME, val);
		break;

	case 3:
		dw_mipi_csi_write(core, R_CSI2_IPI3_HBP_TIME, val);
		break;

	case 4:
		dw_mipi_csi_write(core, R_CSI2_IPI4_HBP_TIME, val);
		break;

	default:
		break;
	}
}

static void set_ipi_hsd(struct csi_mipi_dev *core, int index, uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(core, R_CSI2_IPI_HSD_TIME, val);
		break;

	case 2:
		dw_mipi_csi_write(core, R_CSI2_IPI2_HSD_TIME, val);
		break;

	case 3:
		dw_mipi_csi_write(core, R_CSI2_IPI3_HSD_TIME, val);
		break;

	case 4:
		dw_mipi_csi_write(core, R_CSI2_IPI4_HSD_TIME, val);
		break;

	default:
		break;
	}
}

static void set_ipi_adv_features(struct csi_mipi_dev *core, int index,
				uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(core, R_CSI2_IPI_ADV_FEATURES, val);
		break;

	case 2:
		dw_mipi_csi_write(core, R_CSI2_IPI2_ADV_FEATURES, val);
		break;

	case 3:
		dw_mipi_csi_write(core, R_CSI2_IPI3_ADV_FEATURES, val);
		break;

	case 4:
		dw_mipi_csi_write(core, R_CSI2_IPI4_ADV_FEATURES, val);
		break;

	default:
		break;
	}
}

int mipi_csi2_initialization(struct csi_mipi_dev *core)
{
	int i;

	if (check_version(core) < 0)
		return -ENODEV;

	set_lane_number(core, core->lane_num);

	for (i = 0; i < SDRV_MIPI_CSI2_IPI_NUM; i++) {
		set_ipi_mode(core, i + 1, core->vch_hw[i].ipi_mode);
		set_ipi_vcid(core, i + 1, core->vch_hw[i].virtual_ch);

		set_ipi_data_type(core, i + 1, core->vch_hw[i].output_type);

		set_ipi_hsa(core, i + 1, core->vch_hw[i].hsa);
		set_ipi_hbp(core, i + 1, core->vch_hw[i].hbp);
		set_ipi_hsd(core, i + 1, core->vch_hw[i].hsd);

		set_ipi_adv_features(core, i + 1, core->vch_hw[i].adv_feature);
	}

	set_phy_freq(core, core->lanerate, core->lane_num);
	return 0;
}

void mipi_csi2_enable(struct csi_mipi_dev *core, uint32_t enable)
{
	csi_info("%s: enable[%d]\n", __func__, enable);

	if (enable) {
		// phy_shutdownz high, not shutdown
		dw_mipi_csi_write(core, R_CSI2_DPHY_SHUTDOWNZ, 1);
		// dphy_rstz high, not reset
		dw_mipi_csi_write(core, R_CSI2_DPHY_RSTZ, 1);

		// resetn high, not reset
		dw_mipi_csi_write(core, R_CSI2_CTRL_RESETN, 1);
		dw_enable_mipi_csi_irq_common(core);
		usleep_range(1000, 2000);
	} else {
		dw_mipi_csi_write(core, R_CSI2_DPHY_SHUTDOWNZ, 0);
		dw_mipi_csi_write(core, R_CSI2_DPHY_RSTZ, 0);
		dw_mipi_csi_write(core, R_CSI2_CTRL_RESETN, 0);
		dw_disable_mipi_csi_irq_common(core);
		usleep_range(1000, 2000);
	}
}

int mipi_csi_irq(struct csi_mipi_dev *core)
{
	uint32_t int_status, ind_status;
	uint32_t ipi_softrst;

	int_status = dw_mipi_csi_read(core, R_CSI2_INTERRUPT);

	if (int_status & CSI2_INT_PHY_FATAL) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_PHY_FATAL);
		cam_isr_log_err("mipicsi%d: INT PHY FATAL: 0x%08x\n", core->id,
							ind_status);
	}

	if (int_status & CSI2_INT_PKT_FATAL) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_PKT_FATAL);
		cam_isr_log_err("mipicsi%d: INT PKT FATAL: 0x%08x\n", core->id,
							ind_status);
	}

	if (int_status & CSI2_INT_FRAME_FATAL) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_FRAME_FATAL);
		cam_isr_log_err("mipicsi%d: INT FRAME FATAL: 0x%08x\n", core->id,
							ind_status);
	}

	if (int_status & CSI2_INT_PHY) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_PHY);
		cam_isr_log_err("mipicsi%d: INT PHY: 0x%08x\n", core->id,
							ind_status);
	}

	if (int_status & CSI2_INT_PKT) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_PKT);
		cam_isr_log_err("mipicsi%d: INT PKT: 0x%08x\n", core->id,
							ind_status);
	}

	if (int_status & CSI2_INT_LINE) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_LINE);
		cam_isr_log_err("mipicsi%d: INT LINE: 0x%08x\n", core->id,
							ind_status);
	}

	if (int_status & CSI2_INT_IPI) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_IPI);
		ipi_softrst = dw_mipi_csi_read(core, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI_SOFTRSTN);
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI_SOFTRSTN;
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		cam_isr_log_err("mipicsi%d: INT IPI: 0x%08x\n", core->id,
							ind_status);
	}
	if (int_status & CSI2_INT_IPI2) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_IPI2);
		ipi_softrst = dw_mipi_csi_read(core, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI2_SOFTRSTN);
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI2_SOFTRSTN;
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		cam_isr_log_err("mipicsi%d: INT IPI2: 0x%08x\n", core->id,
							ind_status);
	}
	if (int_status & CSI2_INT_IPI3) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_IPI3);
		ipi_softrst = dw_mipi_csi_read(core, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI3_SOFTRSTN);
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI3_SOFTRSTN;
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		cam_isr_log_err("mipicsi%d: INT IPI3: 0x%08x\n", core->id,
							ind_status);
	}
	if (int_status & CSI2_INT_IPI4) {
		ind_status = dw_mipi_csi_read(core, R_CSI2_INT_IPI4);
		ipi_softrst = dw_mipi_csi_read(core, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI4_SOFTRSTN);
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI4_SOFTRSTN;
		dw_mipi_csi_write(core, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		cam_isr_log_err("mipicsi%d: INT IPI4: 0x%08x\n", core->id,
							ind_status);
	}

	return 0;
}