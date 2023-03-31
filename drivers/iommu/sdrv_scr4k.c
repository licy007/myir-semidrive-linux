/*
 * Semidrive X9 platform SMMU auxiliary driver
 *
 * Copyright (C) 2019, Semidrive Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/dmaengine.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/platform_data/mmp_dma.h>
#include <linux/dmapool.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/of.h>
#include <linux/wait.h>
#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/debugfs.h>

#define SCR4K_SID_BASE (0x35600000u)

static init_scr4k_sid(void)
{
	void __iomem *scr4k_base = ioremap(SCR4K_SID_BASE, 0x68 << 12);
	u32 i = 0;
	u32 ret = 0;

	for (i = 0; i <= 5; i++) {
		writel_relaxed(i, scr4k_base  + (i << 12));
	}

	for (i = 7; i <= 39; i++) {
		writel_relaxed(i, scr4k_base  + (i << 12));
	}

	for (i = 47; i <= 73; i++) {
		writel_relaxed(i, scr4k_base  + (i << 12));
	}

	for (i = 80; i <= 103; i++) {
		writel_relaxed(i, scr4k_base  + (i << 12));
	}

	iounmap(scr4k_base);
}

subsys_initcall(init_scr4k_sid);
