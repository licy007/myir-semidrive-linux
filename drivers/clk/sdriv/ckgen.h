 /*
 * ckgen.h
 *
 *
 * Copyright(c); 2018 Semidrive
 *
 * Author: Alex Chen <qing.chen@semidrive.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CKGEN_H__
#define __CKGEN_H__

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "ckgen_reg.h"

#define SLICE_ID(x) (x)
#define PREDIV(x)   (x)
#define POSTDIV(x)  (x)
#define SLICE_SRC(x)    (x)
#define UUU_ID(x)   (x)
#define SOC_CKGEN_REG_MAP(x)(x<<10)

#define DIV_MNPQ(m, n, p, q)    \
	(FV_CKGEN_UUU_SLICE_M_DIV_NUM(m) \
	| FV_CKGEN_UUU_SLICE_N_DIV_NUM(n)\
	| FV_CKGEN_UUU_SLICE_P_DIV_NUM(p)\
	| FV_CKGEN_UUU_SLICE_Q_DIV_NUM(q))

typedef enum {
	PATH_A = 0,
	PATH_B,
} slice_path_e;

typedef enum {
	UUU_SEL_CKGEN_SOC = 0,
	UUU_SEL_PLL = 3,
} uuu_src_sel_e;

void ckgen_ip_slice_cfg(void __iomem *base,
		u32 src_sel, u32 pre_div, u32 post_div);
void ckgen_bus_slice_cfg(void __iomem *base,
	u32 path, u32 src_sel, u32 pre_div);
void ckgen_bus_slice_postdiv_update(void __iomem *base, u32 post_div);
void ckgen_bus_slice_switch(void __iomem *base);
void ckgen_core_slice_cfg(void __iomem *base, u32 path, u32 src_sel);
void ckgen_core_slice_postdiv_update(void __iomem *base, u32 post_div);
void ckgen_core_slice_switch(void __iomem *base);
void ckgen_cg_en(void __iomem *base);
void ckgen_cg_dis(void __iomem *base);

#endif  /* __CKGEN_H__ */
