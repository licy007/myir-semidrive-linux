/*
 * mipicsi-ip.h
 *
 * Semidrive platform mipi csi2 header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __SDRV_MIPICSI_IP_H__
#define __SDRV_MIPICSI_IP_H__

#include "sdrv-cam-os-def.h"

/* ipi mode */
#define IPI_CONTROLLER_TIMING (1 << 0)
#define IPI_MODE_ENABLE (1 << 24)
#define IPI_MODE_CUT_THROUGH (1 << 16)

/* ipi advance features */
#define IPI_ADV_SYNC_LEGACCY (1 << 24)
#define IPI_ADV_USE_EMBEDDED (1 << 21)
#define IPI_ADV_USE_BLANKING (1<<20）
#define IPI_ADV_USE_NULL (1 << 19)
#define IPI_ADV_USE_LINE_START (1 << 18)
#define IPI_ADV_USE_VIDEO (1 << 17)
#define IPI_ADV_SEL_LINE_EVENT (1 << 16)

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

struct csi_vch_info {
	uint32_t output_type; /* IPI = 0; IDI = 1; BOTH = 2 */
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

struct csi_mipi_dev {
	uint32_t id;
	reg_addr_t base;
	reg_addr_t dispmux_base;

	uint32_t lanerate;
	uint32_t lane_num;

	struct csi_vch_info vch_hw[4];
};

int mipi_csi_irq(struct csi_mipi_dev *core);
void mipi_csi2_enable(struct csi_mipi_dev *core, uint32_t enable);
int mipi_csi2_initialization(struct csi_mipi_dev *core);

#endif /* __SDRV_MIPICSI_IP_H__ */