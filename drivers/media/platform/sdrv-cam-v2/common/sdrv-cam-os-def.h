/*
 * sdrv-cam-os-def.h
 *
 * Semidrive platform camera util header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#ifndef __SDRV_CAM_OS_DEF_H__
#define __SDRV_CAM_OS_DEF_H__

#ifdef __LINUX__

#include <linux/types.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#ifndef reg_addr_t
typedef void __iomem * reg_addr_t;
#endif

#define csi_err_ratelimited pr_err_ratelimited
#define csi_err pr_err
#define csi_warn pr_warn
#define csi_info pr_info
#define csi_debug pr_debug

static inline uint32_t reg_read(reg_addr_t base, uint32_t addr)
{
	return readl_relaxed(base + addr);
}

static inline void reg_write(reg_addr_t base, uint32_t addr, uint32_t val)
{
	writel_relaxed(val, base + addr);
}

static inline void reg_clr(reg_addr_t base, uint32_t addr, uint32_t clr)
{
	reg_write(base, addr, reg_read(base, addr) & ~clr);
}

static inline void reg_set(reg_addr_t base, uint32_t addr, uint32_t set)
{
	reg_write(base, addr, reg_read(base, addr) | set);
}

static inline void reg_clr_and_set(reg_addr_t base, uint32_t addr,
										uint32_t clr, uint32_t set)
{
	uint32_t reg;

	reg = reg_read(base, addr);
	reg &= ~clr;
	reg |= set;
	reg_write(base, addr, reg);
}
#endif /* __LINUX__ */

#endif /* __SDRV_CAM_OS_DEF_H__ */
