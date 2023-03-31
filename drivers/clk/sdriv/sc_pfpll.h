/*
 * sc_pfpll.h
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
#ifndef __SC_PFPLL_H__
#define __SC_PFPLL_H__

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/notifier.h>

#define PLL_FBDIV(x) (x)
#define PLL_REFDIV(x) (x)
#define PLL_POSTDIV1(x) (x)

#define SOC_PLL_REFCLK_FREQ 24000000

void sc_pfpll_setparams(void __iomem *base, unsigned int fbdiv,
			unsigned int refdiv, unsigned int postdiv1,
			unsigned int postdiv2, unsigned long frac, int diva,
			int divb, int divc, int divd, int isint);

void sc_pfpll_getparams(void __iomem *base, unsigned int *fbdiv,
			unsigned int *refdiv, unsigned int *postdiv1,
			unsigned int *postdiv2, unsigned long *frac, int *diva,
			int *divb, int *divc, int *divd, int *isint);
void sc_pfpll_set_ss(void __iomem *base, bool enable, bool down,
		     uint32_t spread);
void sc_pfpll_get_ss(void __iomem *base, bool *isint, bool *enable, bool *down,
		     uint32_t *spread);
#endif
