/*
 * clk.h
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

#ifndef __CLK__H__
#define __CLK__H__

#define MAX_PARENT_NUM 10

#include <dt-bindings/clk/x9-clk.h>
#include <linux/of_address.h>

#define IP_PREDIV_SHIFT 4
#define IP_PREDIV_WIDTH 3
#define IP_PREDIV_BUSYSHIFT 30
#define IP_PREDIV_BUSYWIDTH 1
#define IP_PREDIV_EXPECT 0
#define IP_POSTDIV_SHIFT 10
#define IP_POSTDIV_WIDTH 6
#define IP_POSTDIV_BUSYSHIFT 31
#define IP_POSTDIV_BUSYWIDTH 1
#define IP_POSTDIV_EXPECT 0

#define IP_COMDIV_WIDTH 9

#define CORE_DIV_SHIFT 10
#define CORE_DIV_WIDTH 6
#define CORE_DIV_BUSYSHIFT 31
#define CORE_DIV_BUSYWIDTH 1
#define CORE_DIV_EXPECT 0

#define BUS_POSTDIV_SHIFT 10
#define BUS_POSTDIV_WIDTH 6
#define BUS_POSTDIV_BUSYSHIFT 31
#define BUS_POSTDIV_BUSYWIDTH 1
#define BUS_POSTDIV_EXPECT 0

struct sdrv_cgu_out_clk {
	const char *name;
	int type;
	void __iomem	*base;
	spinlock_t	*lock;
	//mux
	u8 mux_shift;
	u8 mux_width;
	//composite
	u32 *mux_table;
	struct clk_divider div;
	struct clk_hw mux_hw;
	struct clk_hw gate_hw;
	unsigned long min_rate;
	unsigned long max_rate;
	struct notifier_block clk_nb;
	int busywidth;	//0 means no busy bit
	int busyshift;
	int expect;
};
#define mux_to_sdrv_cgu_out_clk(_hw) container_of(_hw, struct sdrv_cgu_out_clk, mux_hw)
#define gate_to_sdrv_cgu_out_clk(_hw) container_of(_hw, struct sdrv_cgu_out_clk, gate_hw)
#define div_to_sdrv_cgu_out_clk(_hw) container_of(_hw, struct sdrv_cgu_out_clk, div.hw)
#define nb_to_sdrv_cgu_out_clk(nb) container_of(nb, struct sdrv_cgu_out_clk, clk_nb)
int sdrv_get_clk_min_rate(const char *name, u32 *min);
int sdrv_get_clk_max_rate(const char *name, u32 *max);
bool sdrv_clk_of_device_is_available(const struct device_node *device);
int sdrv_set_dummy_parent_hw(struct clk_hw *hw);
#ifdef CONFIG_ARCH_SEMIDRIVE_V9
extern void sdrv_pll_of_init(struct device_node *np);
#endif
#endif

