/*
 * clk-pll.h
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


#ifndef __CLK_PLL_H
#define       __CLK_PLL_H
#include "clk.h"
#include "sc_pfpll.h"

struct sdrv_pll_rate_table {
	unsigned int refdiv;
	unsigned int fbdiv;
	unsigned int postdiv[2];
	unsigned long frac;
	int div[4];
	int isint;
};
struct sdrv_cgu_pll_clk {
	const char *name;
	int type;/*0: root; 1~4: div a,b,c,d*/
	struct clk_hw hw;
	void __iomem *reg_base;
	const struct sdrv_pll_rate_table *rate_table;
	struct sdrv_pll_rate_table params;
	bool isreadonly;
	unsigned int rate_count;
	spinlock_t *lock;
	const struct clk_ops *pll_ops;
	unsigned long min_rate;
	unsigned long max_rate;
	struct notifier_block clk_nb;
};
#define nb_to_sdrv_cgu_pll_clk(nb) container_of(nb, struct sdrv_cgu_pll_clk, clk_nb)
#endif

