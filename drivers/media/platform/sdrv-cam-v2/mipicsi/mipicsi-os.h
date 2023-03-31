/*
 * mipicsi-os.h
 *
 * Semidrive platform mipi csi2 header file
 *
 * Copyright (C) 2022, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __SDRV_MIPI_CSI_OS_H__
#define __SDRV_MIPI_CSI_OS_H__

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include "mipicsi-ip.h"
#include "sdrv-cam-core.h"
#include "sdrv-cam-video.h"

#define SDRV_MIPI_CSI2_NAME "sdrv-mipi-csi2"

struct sdrv_csi_mipi_csi2 {
	struct device *dev;
	struct platform_device *pdev;
	struct v4l2_subdev subdev;

	struct mutex lock;

	/* Device Tree Information */
	void __iomem *base_address;
	void __iomem *dispmux_address;
	uint32_t ctrl_irq_number;

	struct csi_mipi_dev core;
};

#endif
