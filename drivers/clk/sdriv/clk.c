/*
 * clk.c
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

#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/reboot.h>
#include <linux/rational.h>
#include "clk.h"
#include "ckgen.h"

struct clk *sd_clk_register_composite(
	struct device *dev, const char *name, const char *const *parent_names,
	int num_parents, struct clk_hw *mux_hw, const struct clk_ops *mux_ops,
	struct clk_hw *rate_hw, const struct clk_ops *rate_ops,
	struct clk_hw *gate_hw, const struct clk_ops *gate_ops,
	unsigned long flags);

typedef union {
	struct {
		u32 cg_en_a : 1;
		u32 src_sel_a : 3;
		u32 reserved1 : 5;
		u32 a_b_sel : 1;
		u32 post_div_num : 6;
		u32 cg_en_b : 1;
		u32 src_sel_b : 3;
		u32 reserved2 : 7;
		u32 cg_en_b_status : 1;
		u32 cg_en_a_status : 1;
		u32 reserved3 : 2;
		u32 post_busy : 1;
	};

	u32 val;
} soc_core_ctl;
typedef union {
	struct {
		u32 cg_en_a : 1;
		u32 src_sel_a : 3;
		u32 pre_div_num_a : 3;
		u32 reserved1 : 2;
		u32 a_b_sel : 1;
		u32 post_div_num : 6;
		u32 cg_en_b : 1;
		u32 src_sel_b : 3;
		u32 pre_div_num_b : 3;
		u32 reserved2 : 4;
		u32 cg_en_b_status : 1;
		u32 cg_en_a_status : 1;
		u32 pre_busy_b : 1;
		u32 pre_busy_a : 1;
		u32 post_busy : 1;
	};
	u32 val;
} soc_bus_ctl;
typedef union {
	struct {
		u32 cg_en : 1;
		u32 src_sel : 3;
		u32 pre_div_num : 3;
		u32 reserved1 : 3;
		u32 post_div_num : 6;
		u32 reserved2 : 12;
		u32 cg_en_status : 1;
		u32 reserved3 : 1;
		u32 pre_busy : 1;
		u32 post_busy : 1;
	};
	u32 val;
} soc_ip_ctl;

typedef union {
	struct {
		u32 q_div : 4;
		u32 p_div : 4;
		u32 n_div : 4;
		u32 m_div : 4;
		u32 uuu_sel0 : 1;
		u32 uuu_sel1 : 1;
		u32 reserved1 : 2;
		u32 uuu_gating_ack : 1;
		u32 reserved2 : 6;
		u32 dbg_div : 4;
		u32 dbg_gating_en : 1;
	};

	u32 val;
} soc_uuu;

typedef union {
	struct {
		u32 sw_gating_en : 1;
		u32 sw_gating_dis : 1; //0, means enable, 1 means disable.
		u32 reserved : 29;
		u32 ckgen_gating_lock : 1;
	};
	u32 val;
} soc_lp_gate_en;

static u32 get_soc_core_parent(void __iomem *base)
{
	u32 val;
	void __iomem *reg = base;
	soc_core_ctl ctl;

	ctl.val = clk_readl(reg);
	if (ctl.a_b_sel == 0)
		val = ctl.src_sel_a;
	else
		val = ctl.src_sel_b;
	pr_debug("%s index %d\n", __func__, val);
	return val;
}

static u32 set_soc_core_parent(void __iomem *base, u32 index)
{
	void __iomem *reg = base;
	soc_core_ctl ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s index %d\n", __func__, index);
	ckgen_core_slice_cfg(base, !ctl.a_b_sel, index);

	ckgen_core_slice_switch(base);
	return 0;
}

static u32 get_soc_bus_parent(void __iomem *base)
{
	u32 val;
	void __iomem *reg = base;
	soc_bus_ctl ctl;

	ctl.val = clk_readl(reg);

	if (ctl.a_b_sel == 0)
		val = ctl.src_sel_a;
	else
		val = ctl.src_sel_b;
	pr_debug("%s index %d\n", __func__, val);
	return val;
}

static u32 set_soc_bus_parent(void __iomem *base, u32 index)
{
	void __iomem *reg = base;
	soc_bus_ctl ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s index %d\n", __func__, index);

	if (ctl.a_b_sel)
		ckgen_bus_slice_cfg(base, !ctl.a_b_sel, index,
				    ctl.pre_div_num_b);
	else
		ckgen_bus_slice_cfg(base, !ctl.a_b_sel, index,
				    ctl.pre_div_num_a);
	ckgen_bus_slice_switch(base);
	return 0;
}

static u32 get_soc_ip_parent(void __iomem *base)
{
	u32 val;
	void __iomem *reg = base;
	soc_ip_ctl ctl;

	ctl.val = clk_readl(reg);

	val = ctl.src_sel;
	pr_debug("%s index %d\n", __func__, val);
	return val;
}

static u32 set_soc_ip_parent(void __iomem *base, u32 index)
{
	void __iomem *reg = base;
	soc_ip_ctl ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s index %d\n", __func__, index);
	ckgen_ip_slice_cfg(base, index, ctl.pre_div_num, ctl.post_div_num);
	return 0;
}

static u32 get_soc_uuu_sel0_parent(void __iomem *base)
{
	u32 val;
	void __iomem *reg = base;
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	val = ctl.uuu_sel0;
	pr_debug("%s index %d\n", __func__, val);
	return val;
}

static u32 set_soc_uuu_sel0_parent(void __iomem *base, u32 index)
{
	void __iomem *reg = base;
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s index %d\n", __func__, index);
	ctl.uuu_sel0 = index;
	clk_writel(ctl.val, reg);
	return 0;
}

static u32 get_soc_uuu_sel1_parent(void __iomem *base)
{
	u32 val;
	void __iomem *reg = base;
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	val = ctl.uuu_sel1;
	pr_debug("%s index %d\n", __func__, val);
	return val;
}

static u32 set_soc_uuu_sel1_parent(void __iomem *base, u32 index)
{
	void __iomem *reg = base;
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s index %d\n", __func__, index);
	ctl.uuu_sel1 = index;
	clk_writel(ctl.val, reg);
	return 0;
}

static u8 sdclk_mux_get_parent(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = mux_to_sdrv_cgu_out_clk(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 val;

	if (clk->type == CLK_TYPE_CORE) {
		val = get_soc_core_parent(clk->base);
	} else if (clk->type == CLK_TYPE_BUS) {
		val = get_soc_bus_parent(clk->base);
	} else if (clk->type == CLK_TYPE_IP || clk->type == CLK_TYPE_IP_COM) {
		val = get_soc_ip_parent(clk->base);
	} else if (clk->type == CLK_TYPE_UUU_MUX) { //UUU MUX
		val = get_soc_uuu_sel0_parent(clk->base);
	} else if (clk->type == CLK_TYPE_UUU_MUX2) { //UUU MUX
		val = get_soc_uuu_sel1_parent(clk->base);
	} else { //UUU n/p/q , ip post div and gate only have one parent
		return 0;
	}

	if (val >= num_parents)
		return -EINVAL;
	return val;
}

static int sdclk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct sdrv_cgu_out_clk *clk = mux_to_sdrv_cgu_out_clk(hw);
	u32 val;
	unsigned long flags = 0;

	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);

	if (clk->type == CLK_TYPE_CORE) {
		val = set_soc_core_parent(clk->base, index);
	} else if (clk->type == CLK_TYPE_BUS) {
		val = set_soc_bus_parent(clk->base, index);
	} else if (clk->type == CLK_TYPE_IP || clk->type == CLK_TYPE_IP_COM) {
		val = set_soc_ip_parent(clk->base, index);
	} else if (clk->type == CLK_TYPE_UUU_MUX) { //UUU MUX
		val = set_soc_uuu_sel0_parent(clk->base, index);
	} else if (clk->type == CLK_TYPE_UUU_MUX2) { //UUU MUX2
		val = set_soc_uuu_sel1_parent(clk->base, index);
	} else { //UUU , ip post div and gate only have one parent
		//return 0;
	}

	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);

	return 0;
}
static bool mux_is_better_rate(unsigned long rate, unsigned long now,
			       unsigned long best, unsigned long flags)
{
	if (flags & CLK_MUX_ROUND_CLOSEST)
		return abs_diff(now, rate) < abs_diff(best, rate);

	return now <= rate && now > best;
}

int sdclk_mux_determine_rate_flags(struct clk_hw *hw,
				   struct clk_rate_request *req,
				   unsigned long flags)
{
	struct clk_hw *parent, *best_parent = NULL;
	int i, num_parents, ret;
	unsigned long best = 0;
	struct clk_rate_request parent_req = *req;

	/* if NO_REPARENT flag set, pass through to current parent */
	if (clk_hw_get_flags(hw) & CLK_SET_RATE_NO_REPARENT) {
		parent = clk_hw_get_parent(hw);
		if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT) {
			ret = __clk_determine_rate(parent ? parent : NULL,
						   &parent_req);
			if (ret)
				return ret;

			best = parent_req.rate;
		} else if (parent) {
			best = clk_hw_get_rate(parent);
		} else {
			best = clk_hw_get_rate(hw);
		}

		goto out;
	}

	/* find the parent that can provide the fastest rate <= rate */
	num_parents = clk_hw_get_num_parents(hw);
	for (i = 0; i < num_parents; i++) {
		parent = clk_hw_get_parent_by_index(hw, i);
		if (!parent)
			continue;

		if (clk_hw_get_flags(hw) &
		    CLK_SET_RATE_PARENT /* && clk_hw_get_flags(parent) & CLK_SET_RATE_PARENT*/) {
			parent_req = *req;
			ret = __clk_determine_rate(parent, &parent_req);
			if (ret)
				continue;
		} else {
			parent_req.rate = clk_hw_get_rate(parent);
		}
		if (parent_req.rate != 0 &&
		    (best == 0 || mux_is_better_rate(req->rate, parent_req.rate,
						     best, flags))) {
			best_parent = parent;
			best = parent_req.rate;
		}
	}

	if (!best_parent)
		return -EINVAL;

out:
	if (best_parent)
		req->best_parent_hw = best_parent;
	req->best_parent_rate = best;
	req->rate = best;

	return 0;
}
static int sdclk_mux_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	return sdclk_mux_determine_rate_flags(hw, req, CLK_MUX_ROUND_CLOSEST);
}

static const struct clk_ops sdclk_mux_ops = {
	.get_parent = sdclk_mux_get_parent,
	.set_parent = sdclk_mux_set_parent,
	.determine_rate = sdclk_mux_determine_rate,
};

static int sdclk_gate_enable(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = gate_to_sdrv_cgu_out_clk(hw);
	u32 __iomem *reg = 0;
	unsigned long uninitialized_var(flags);

	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);

	pr_debug("%s name %s\n", __func__, clk->name);
	if (clk->type == CLK_TYPE_CORE) {
		soc_core_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 1;
		else
			ctl.cg_en_b = 1;
		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_BUS) {
		soc_bus_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 1;
		else
			ctl.cg_en_b = 1;

		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_IP || clk->type == CLK_TYPE_IP_POST ||
		   clk->type == CLK_TYPE_IP_COM) {
		soc_ip_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		ctl.cg_en = 1;
		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_GATE) {
		ckgen_cg_en(clk->base);
	}

	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);

	return 0;
}

static void sdclk_gate_disable(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = gate_to_sdrv_cgu_out_clk(hw);
	u32 __iomem *reg = 0;
	unsigned long uninitialized_var(flags);

	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);

	pr_debug("%s name %s\n", __func__, clk->name);
	if (clk->type == CLK_TYPE_CORE) {
		soc_core_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 0;
		else
			ctl.cg_en_b = 0;

		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_BUS) {
		soc_bus_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 0;
		else
			ctl.cg_en_b = 0;

		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_IP || clk->type == CLK_TYPE_IP_POST ||
		   clk->type == CLK_TYPE_IP_COM) {
		soc_ip_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		ctl.cg_en = 0;
		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_GATE) {
		ckgen_cg_dis(clk->base);
	}

	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);
}

static int sdclk_gate_is_enabled(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = gate_to_sdrv_cgu_out_clk(hw);
	void __iomem *reg = 0;
	int ret = 0;
	unsigned long uninitialized_var(flags);

	pr_debug("%s name %s\n", __func__, clk->name);
	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);
	if (clk->type == CLK_TYPE_CORE) {
		soc_core_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ret = ctl.cg_en_a;
		else
			ret = ctl.cg_en_b;
	} else if (clk->type == CLK_TYPE_BUS) {
		soc_bus_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ret = ctl.cg_en_a;
		else
			ret = ctl.cg_en_b;
	} else if (clk->type == CLK_TYPE_IP || clk->type == CLK_TYPE_IP_POST ||
		   clk->type == CLK_TYPE_IP_COM) {
		soc_ip_ctl ctl;

		reg = clk->base;
		ctl.val = clk_readl(reg);
		ret = ctl.cg_en;
	} else if (clk->type == CLK_TYPE_GATE) {
		soc_lp_gate_en gate;

		reg = clk->base; //get gating id?
		gate.val = clk_readl(reg);
		ret = !gate.sw_gating_dis;
	}

	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);
	return ret;
}

static const struct clk_ops sdclk_gate_ops = {
	.enable = sdclk_gate_enable,
	.disable = sdclk_gate_disable,
	.is_enabled = sdclk_gate_is_enabled,
};

static const struct clk_ops sdclk_gate_readonly_ops = {
	.enable = NULL,
	.disable = NULL,
	.is_enabled = sdclk_gate_is_enabled,
};

#define div_mask(width) ((1 << (width)) - 1)

static inline u32 clk_div_readl(struct clk_divider *divider)
{
	return readl(divider->reg);
}

static inline void clk_div_writel(struct clk_divider *divider, u32 val)
{
	writel(val, divider->reg);
}

static unsigned long _sdclk_divider_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct clk_divider *divider = to_clk_divider(hw);
	unsigned int val;
	unsigned int div, prediv = 0, postdiv = 0;

	val = clk_div_readl(divider);
	prediv = (val >> IP_PREDIV_SHIFT & div_mask(IP_PREDIV_WIDTH)) + 1;
	postdiv = (val >> IP_POSTDIV_SHIFT & div_mask(IP_POSTDIV_WIDTH)) + 1;
	div = prediv * postdiv;
	return DIV_ROUND_UP_ULL((u64)parent_rate, div);
	;
}
static unsigned long sdclk_divider_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct sdrv_cgu_out_clk *clk = div_to_sdrv_cgu_out_clk(hw);
	if (clk->type == CLK_TYPE_IP_COM) {
		return _sdclk_divider_recalc_rate(hw, parent_rate);
	} else
		return clk_divider_ops.recalc_rate(hw, parent_rate);
}

static unsigned int _get_table_maxdiv(const struct clk_div_table *table,
				      u8 width)
{
	unsigned int maxdiv = 0, mask = div_mask(width);
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->div > maxdiv && clkt->val <= mask)
			maxdiv = clkt->div;
	return maxdiv;
}

static unsigned int _get_table_mindiv(const struct clk_div_table *table)
{
	unsigned int mindiv = UINT_MAX;
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->div < mindiv)
			mindiv = clkt->div;
	return mindiv;
}

static unsigned int _get_maxdiv(const struct clk_div_table *table, u8 width,
				unsigned long flags)
{
	if (flags & CLK_DIVIDER_ONE_BASED)
		return div_mask(width);
	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		return 1 << div_mask(width);
	if (table)
		return _get_table_maxdiv(table, width);
	return div_mask(width) + 1;
}

static int _round_up_table(const struct clk_div_table *table, int div)
{
	const struct clk_div_table *clkt;
	int up = INT_MAX;

	for (clkt = table; clkt->div; clkt++) {
		if (clkt->div == div)
			return clkt->div;
		else if (clkt->div < div)
			continue;

		if ((clkt->div - div) < (up - div))
			up = clkt->div;
	}

	return up;
}

static int _round_down_table(const struct clk_div_table *table, int div)
{
	const struct clk_div_table *clkt;
	int down = _get_table_mindiv(table);

	for (clkt = table; clkt->div; clkt++) {
		if (clkt->div == div)
			return clkt->div;
		else if (clkt->div > div)
			continue;

		if ((div - clkt->div) < (div - down))
			down = clkt->div;
	}

	return down;
}
static int _div_round_up(const struct clk_div_table *table,
			 unsigned long parent_rate, unsigned long rate,
			 unsigned long flags)
{
	int div = DIV_ROUND_UP_ULL((u64)parent_rate, rate);

	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		div = __roundup_pow_of_two(div);
	if (table)
		div = _round_up_table(table, div);

	return div;
}

static int _div_round_closest(const struct clk_div_table *table,
			      unsigned long parent_rate, unsigned long rate,
			      unsigned long flags)
{
	int up, down;
	unsigned long up_rate, down_rate;

	up = DIV_ROUND_UP_ULL((u64)parent_rate, rate);
	down = parent_rate / rate;

	if (flags & CLK_DIVIDER_POWER_OF_TWO) {
		up = __roundup_pow_of_two(up);
		down = __rounddown_pow_of_two(down);
	} else if (table) {
		up = _round_up_table(table, up);
		down = _round_down_table(table, down);
	}

	up_rate = DIV_ROUND_UP_ULL((u64)parent_rate, up);
	down_rate = DIV_ROUND_UP_ULL((u64)parent_rate, down);

	return (rate - up_rate) <= (down_rate - rate) ? up : down;
}

static int _div_round(const struct clk_div_table *table,
		      unsigned long parent_rate, unsigned long rate,
		      unsigned long flags)
{
	if (flags & CLK_DIVIDER_ROUND_CLOSEST)
		return _div_round_closest(table, parent_rate, rate, flags);

	return _div_round_up(table, parent_rate, rate, flags);
}

static bool _is_best_div(unsigned long rate, unsigned long now,
			 unsigned long best, unsigned long flags)
{
	if (flags & CLK_DIVIDER_ROUND_CLOSEST)
		return abs_diff(rate, now) < abs_diff(rate, best);

	return now <= rate && now > best;
}

static int _next_div(const struct clk_div_table *table, int div,
		     unsigned long flags)
{
	div++;

	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		return __roundup_pow_of_two(div);
	if (table)
		return _round_up_table(table, div);

	return div;
}
static unsigned int _get_table_div(const struct clk_div_table *table,
		                                                        unsigned int val)
{
	        const struct clk_div_table *clkt;

		        for (clkt = table; clkt->div; clkt++)
				                if (clkt->val == val)
							                        return clkt->div;
			        return 0;
}

static unsigned int _get_div(const struct clk_div_table *table,
		                             unsigned int val, unsigned long flags, u8 width)
{
	        if (flags & CLK_DIVIDER_ONE_BASED)
			                return val;
		        if (flags & CLK_DIVIDER_POWER_OF_TWO)
				                return 1 << val;
			        if (flags & CLK_DIVIDER_MAX_AT_ZERO)
					                return val ? val : div_mask(width) + 1;
				        if (table)
						                return _get_table_div(table, val);
					        return val + 1;
}
static int sdclk_divider_bestdiv(struct clk_hw *hw, struct clk_hw *parent,
				 unsigned long rate,
				 unsigned long *best_parent_rate,
				 const struct clk_div_table *table, u8 width,
				 unsigned long flags)
{
	int i, bestdiv = 0;
	unsigned long parent_rate, best = 0, now, maxdiv;
	unsigned long parent_rate_saved = *best_parent_rate;

	if (!rate)
		rate = 1;

	maxdiv = _get_maxdiv(table, width, flags);

	if (!(clk_hw_get_flags(hw) &
	      CLK_SET_RATE_PARENT) /* || !(clk_hw_get_flags(parent) & CLK_SET_RATE_PARENT)*/) {
		parent_rate = *best_parent_rate;
		bestdiv = _div_round(table, parent_rate, rate, flags);
		bestdiv = bestdiv == 0 ? 1 : bestdiv;
		bestdiv = bestdiv > maxdiv ? maxdiv : bestdiv;
		return bestdiv;
	}

	/*
         * The maximum divider we can use without overflowing
         * unsigned long in rate * i below
         */
	maxdiv = min(U32_MAX / rate, maxdiv);

	for (i = _next_div(table, 0, flags); i <= maxdiv;
	     i = _next_div(table, i, flags)) {
		if (rate * i == parent_rate_saved) {
			/*
                         * It's the most ideal case if the requested rate can be
                         * divided from parent clock without needing to change
                         * parent rate, so return the divider immediately.
                         */
			*best_parent_rate = parent_rate_saved;
			return i;
		}
		parent_rate = clk_hw_round_rate(parent, rate * i);
		now = DIV_ROUND_UP_ULL((u64)parent_rate, i);
		if (parent_rate != 0 &&
		    (best == 0 || _is_best_div(rate, now, best, flags))) {
			bestdiv = i;
			best = now;
			*best_parent_rate = parent_rate;
		}
	}

	if (!bestdiv) {
		bestdiv = _get_maxdiv(table, width, flags);
		*best_parent_rate = clk_hw_round_rate(parent, 1);
	}

	return bestdiv;
}

long sddivider_round_rate_parent(struct clk_hw *hw, struct clk_hw *parent,
				 unsigned long rate, unsigned long *prate,
				 const struct clk_div_table *table, u8 width,
				 unsigned long flags)
{
	int div;

	div = sdclk_divider_bestdiv(hw, parent, rate, prate, table, width,
				    flags);

	return DIV_ROUND_UP_ULL((u64)*prate, div);
}
static struct clk_hw *parent_hw[5] = { NULL };
static int hw_i = 0;
int sdrv_set_dummy_parent_hw(struct clk_hw *hw)
{
	if (parent_hw[hw_i] != NULL && hw != NULL) {
		hw_i++;
		hw_i = hw_i % 5;
	}
	BUG_ON(parent_hw[hw_i] != NULL && hw != NULL);
	parent_hw[hw_i] = hw;
	if (hw == NULL && hw_i != 0)
		hw_i--;
	return 0;
}
static long sdclk_divider_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *prate)
{
	struct clk_divider *divider = to_clk_divider(hw);

	/* if read only, just return current value */
	if (divider->flags & CLK_DIVIDER_READ_ONLY) {
		u32 val;

		val = clk_div_readl(divider) >> divider->shift;
		val &= div_mask(divider->width);
		val = _get_div(divider->table, val, divider->flags,
							divider->width);
		return DIV_ROUND_UP_ULL((u64)*prate, val);
	}

	return sddivider_round_rate_parent(
		hw, parent_hw[hw_i] ? parent_hw[hw_i] : clk_hw_get_parent(hw),
		rate, prate, divider->table, divider->width, divider->flags);
}

static int sdclk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long parent_rate)
{
	struct clk_divider *divider = to_clk_divider(hw);
	struct sdrv_cgu_out_clk *clk = div_to_sdrv_cgu_out_clk(hw);
	int value;
	unsigned long flags = 0;
	u32 val;

	value = divider_get_val(rate, parent_rate, divider->table,
				divider->width, divider->flags);
	if (value < 0)
		return value;

	if (divider->lock)
		spin_lock_irqsave(divider->lock, flags);
	else
		__acquire(divider->lock);
	if (clk->type == CLK_TYPE_IP_COM) {
		int i, k;
		int bestdivdiff =
			    (1 << IP_PREDIV_WIDTH) * (1 << IP_POSTDIV_WIDTH),
		    ni=0, nk=0;
		for (k = 1; k <= (1 << IP_POSTDIV_WIDTH); k++) {
			for (i = 1; i <= (1 << IP_PREDIV_WIDTH); i++) {
				int divdiff;
				if ((i * k) == (value + 1)) {
					bestdivdiff = 0;
					ni = i - 1;
					nk = k - 1;
					break;
				}
				divdiff = abs_diff((i * k), (value + 1));
				if (divdiff < bestdivdiff) {
					bestdivdiff = divdiff;
					ni = i - 1;
					nk = k - 1;
				}
			}
		}
		//pre
		val = clk_readl(divider->reg);
		val &= ~(div_mask(IP_PREDIV_WIDTH) << IP_PREDIV_SHIFT);

		val |= (u32)ni << IP_PREDIV_SHIFT;
		//post
		val &= ~(div_mask(IP_POSTDIV_WIDTH) << IP_POSTDIV_SHIFT);

		val |= (u32)nk << IP_POSTDIV_SHIFT;
		clk_writel(val, divider->reg);
		/* polling busy bit */
		{
			int count = 100;

			do {
				udelay(1);
				val = clk_readl(divider->reg);
				val &= (div_mask(IP_PREDIV_BUSYWIDTH)
					<< IP_PREDIV_BUSYSHIFT);
				val = val >> IP_PREDIV_BUSYSHIFT;

			} while (val != IP_PREDIV_EXPECT && count--);
			if (count == 0)
				pr_err("polling prediv busyfail: %s busy bit\n",
				       clk->name);
			count = 100;
			do {
				udelay(1);
				val = clk_readl(divider->reg);
				val &= (div_mask(IP_POSTDIV_BUSYWIDTH)
					<< IP_POSTDIV_BUSYSHIFT);
				val = val >> IP_POSTDIV_BUSYSHIFT;

			} while (val != IP_POSTDIV_EXPECT && count--);
			if (count == 0)
				pr_err("polling postdiv busy fail: %s busy bit\n",
				       clk->name);
		}
	} else {
		if (divider->flags & CLK_DIVIDER_HIWORD_MASK) {
			val = div_mask(divider->width) << (divider->shift + 16);
		} else {
			val = clk_readl(divider->reg);
			val &= ~(div_mask(divider->width) << divider->shift);
		}
		val |= (u32)value << divider->shift;
		clk_writel(val, divider->reg);
		/* polling busy bit */
		if (clk->busywidth) {
			int count = 100;

			do {
				udelay(1);
				val = clk_readl(divider->reg);
				val &= (div_mask(clk->busywidth)
					<< clk->busyshift);
				val = val >> clk->busyshift;

			} while (val != clk->expect && count--);
			if (count == 0)
				pr_err("polling fail: %s busy bit\n",
				       clk->name);
		}
	}
	if (divider->lock)
		spin_unlock_irqrestore(divider->lock, flags);
	else
		__release(divider->lock);
	return 0;
}

static const struct clk_ops sdclk_divider_ops = {
	.recalc_rate = sdclk_divider_recalc_rate,
	.round_rate = sdclk_divider_round_rate,
	.set_rate = sdclk_divider_set_rate,
};

static const struct clk_ops sdclk_divider_readonly_ops = {
	.recalc_rate = sdclk_divider_recalc_rate,
	.round_rate = sdclk_divider_round_rate,
};

static int sdrv_clk_pre_rate_change(struct sdrv_cgu_out_clk *clk,
				    struct clk_notifier_data *ndata)
{
	int ret = 0;

	if (ndata->new_rate < clk->min_rate ||
	    ndata->new_rate > clk->max_rate) {
		pr_err("want change clk %s freq from %ld to %ld, not allowd, min %ld max %ld\n",
		       clk->name, ndata->old_rate, ndata->new_rate,
		       clk->min_rate, clk->max_rate);
		ret = -EINVAL;
	}
	return ret;
}

static int sdrv_clk_post_rate_change(struct sdrv_cgu_out_clk *clk,
				     struct clk_notifier_data *ndata)
{
	return 0;
}

static int sdrv_clk_notifier_cb(struct notifier_block *nb, unsigned long event,
				void *data)
{
	struct clk_notifier_data *ndata = data;
	struct sdrv_cgu_out_clk *clk = nb_to_sdrv_cgu_out_clk(nb);
	int ret = 0;

	pr_debug("%s: event %lu, old_rate %lu, new_rate: %lu\n", __func__,
		 event, ndata->old_rate, ndata->new_rate);
	if (event == PRE_RATE_CHANGE)
		ret = sdrv_clk_pre_rate_change(clk, ndata);
	else if (event == POST_RATE_CHANGE)
		ret = sdrv_clk_post_rate_change(clk, ndata);

	return notifier_from_errno(ret);
}
/**
 * sdrv_of_clk_parent_fill() - Fill @parents with names of @np's parents and return
 * number of parents
 * @np: Device node pointer associated with clock provider
 * @parents: pointer to char array that hold the parents' names
 * @size: size of the @parents array
 *
 * Return: number of parents for the clock node.
 */
static int sdrv_of_clk_parent_fill(struct device_node *np, const char **parents,
			unsigned int size)
{
	unsigned int i = 0;

	while (i < size) {
		parents[i] = of_clk_get_parent_name(np, i);
		if (!parents[i])
			parents[i]="null";
		i++;
	}

	return i;
}

static struct clk *sdrv_register_out_composite(struct device_node *np,
					       void __iomem *base,
					       struct sdrv_cgu_out_clk *clk,
					       const char *clk_name)
{
	const char *parents[MAX_PARENT_NUM];
	struct clk *ret = NULL;
	u32 min_freq = 0, max_freq = 0, readonly = 0;
	char pname[32] = { "" };
	bool havegate = false;
	int n_parents = 0;
	bool nochg_prate;

	of_property_read_u32(np, "sdrv,clk-readonly", &readonly);
	nochg_prate = of_property_read_bool(np, "sdrv,nochg-parent-rate");

	clk->name = clk_name;
	clk->base = base;
	clk->lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (clk->lock)
		spin_lock_init(clk->lock);

	n_parents = of_clk_get_parent_count(np);
	if (n_parents < 1) {
		pr_err("clk %s must have parents\n", np->name);
		return NULL;
	}

	if (clk->type == CLK_TYPE_UUU_MUX || clk->type == CLK_TYPE_UUU_MUX2 ||
	    clk->type == CLK_TYPE_GATE) {
		clk->div.reg = NULL;
	} else {
		//clk->div.reg = base + ctl_offset;
		clk->div.reg = base;
		clk->div.lock = clk->lock;
		clk->div.flags = CLK_DIVIDER_ROUND_CLOSEST;
	}
	if (clk->type == CLK_TYPE_IP || clk->type == CLK_TYPE_IP_POST ||
	    clk->type == CLK_TYPE_IP_COM || clk->type == CLK_TYPE_BUS ||
	    clk->type == CLK_TYPE_CORE || clk->type == CLK_TYPE_GATE) {
		havegate = true;
	}
	//pr_info("register clk:%s\n", clk->name);
	if (clk->type == CLK_TYPE_IP_POST) {
		sprintf(pname, "%s_PRE", np->name);
		parents[0] = pname;
		n_parents = 1;
	} else
		sdrv_of_clk_parent_fill(np, parents, n_parents);

	if (readonly) {
		if (clk->div.reg != NULL)
			clk->div.flags = CLK_DIVIDER_READ_ONLY;
		ret = sd_clk_register_composite(
			NULL, clk->name, parents, n_parents,
			(n_parents != 0) ? (&clk->mux_hw) : NULL,
			(n_parents != 0) ? (&sdclk_mux_ops) : NULL,
			(clk->div.reg != NULL) ? (&clk->div.hw) : NULL,
			(clk->div.reg != NULL) ? (&sdclk_divider_readonly_ops) :
						 NULL,
			(havegate == true) ? (&clk->gate_hw) : NULL,
			(havegate == true) ? (&sdclk_gate_readonly_ops) : NULL,
			((n_parents != 0) && (!nochg_prate)) ?
				(CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT |
				 CLK_SET_RATE_NO_REPARENT) :
				CLK_IGNORE_UNUSED);
		if (IS_ERR(ret)) {
			pr_err("register clk:%s fail\n", clk->name);
			return ret;
		}
	} else {
		ret = sd_clk_register_composite(
			NULL, clk->name, parents, n_parents,
			(n_parents != 0) ? (&clk->mux_hw) : NULL,
			(n_parents != 0) ? (&sdclk_mux_ops) : NULL,
			(clk->div.reg != NULL) ? (&clk->div.hw) : NULL,
			(clk->div.reg != NULL) ? (&sdclk_divider_ops) : NULL,
			(havegate == true) ? (&clk->gate_hw) : NULL,
			(havegate == true) ? (&sdclk_gate_ops) : NULL,
			((n_parents != 0) && (!nochg_prate)) ?
				(CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT) :
				CLK_IGNORE_UNUSED);
		if (IS_ERR(ret)) {
			pr_err("register clk:%s fail\n", clk->name);
			return ret;
		}
		if (sdrv_get_clk_min_rate(clk->name, &min_freq) == 0) {
			clk_set_min_rate(ret, min_freq);
			clk->min_rate = min_freq;
		} else {
			clk->min_rate = 0;
		}
		if (sdrv_get_clk_max_rate(clk->name, &max_freq) == 0) {
			clk_set_max_rate(ret, max_freq);
			clk->max_rate = max_freq;
		} else {
			clk->max_rate = U32_MAX;
		}
		clk->clk_nb.notifier_call = sdrv_clk_notifier_cb;
		if (clk_notifier_register(ret, &clk->clk_nb)) {
			pr_err("%s: failed to register clock notifier for %s\n",
			       __func__, clk->name);
		}
	}

	return ret;
}

int sdrv_get_clk_min_rate(const char *name, u32 *min)
{
	struct device_node *np = NULL;
	int ret = -1;

	np = of_find_node_by_name(NULL, name);
	if (np != NULL) {
		if (of_device_is_available(np) &&
		    of_property_read_u32(np, "sdrv,min-freq", min) == 0)
			ret = 0;
		of_node_put(np);
		return ret;
	}
	return ret;
}

int sdrv_get_clk_max_rate(const char *name, u32 *max)
{
	struct device_node *np = NULL;
	int ret = -1;

	np = of_find_node_by_name(NULL, name);
	if (np != NULL) {
		if (of_device_is_available(np) &&
		    of_property_read_u32(np, "sdrv,max-freq", max) == 0)
			ret = 0;
		of_node_put(np);
		return ret;
	}
	return ret;
}
bool sdrv_clk_of_device_is_available(const struct device_node *device)
{
	const char *status;
	int statlen;

	if (!device)
		return false;

	status = of_get_property(device, "status", &statlen);
	if (status == NULL) {
		return sdrv_clk_of_device_is_available(device->parent);
	}

	if (statlen > 0) {
		if (!strcmp(status, "okay") || !strcmp(status, "ok"))
			return true;
	}

	return false;
}

static void __init sdrv_ckgen_clk_init(struct device_node *np)
{
	void __iomem *base;
	int ret;
	struct sdrv_cgu_out_clk *cgu_clk;
	struct clk *clk;
	int type;
	const char *clk_name = np->name;

	if (!sdrv_clk_of_device_is_available(np))
		return;

	base = of_iomap(np, 0);
	if (!base) {
		pr_warn("%s: failed to map address range\n", __func__);
		return;
	}
	pr_debug("reg base %p name %s\n", base, clk_name);
	cgu_clk = kzalloc(sizeof(*cgu_clk), GFP_KERNEL);
	if (!cgu_clk)
		return;

	of_property_read_string(np, "clock-output-names", &clk_name);

	if (of_property_read_u32(np, "sdrv,type", &type) < 0) {
		kfree(cgu_clk);
		return;
	}

	if (type == CLK_TYPE_CORE) {
		//sdrv,postdiv
		cgu_clk->type = CLK_TYPE_CORE;
		cgu_clk->div.shift = CORE_DIV_SHIFT;
		cgu_clk->div.width = CORE_DIV_WIDTH;
		cgu_clk->busywidth = CORE_DIV_BUSYWIDTH;
		cgu_clk->busyshift = CORE_DIV_BUSYSHIFT;
		cgu_clk->expect = CORE_DIV_EXPECT;
	} else if (type == CLK_TYPE_BUS) {
		//sdrv,prediv
		//sdrv,postdiv
		cgu_clk->type = CLK_TYPE_BUS;
		cgu_clk->div.shift = BUS_POSTDIV_SHIFT;
		cgu_clk->div.width = BUS_POSTDIV_WIDTH;
		cgu_clk->busywidth = BUS_POSTDIV_BUSYWIDTH;
		cgu_clk->busyshift = BUS_POSTDIV_BUSYSHIFT;
		cgu_clk->expect = BUS_POSTDIV_EXPECT;
	} else if (type == CLK_TYPE_IP) {
		char name[32] = { "" };
		//sdrv,prediv
		cgu_clk->type = CLK_TYPE_IP;
		cgu_clk->div.shift = IP_PREDIV_SHIFT;
		cgu_clk->div.width = IP_PREDIV_WIDTH;
		cgu_clk->busywidth = IP_PREDIV_BUSYWIDTH;
		cgu_clk->busyshift = IP_PREDIV_BUSYSHIFT;
		cgu_clk->expect = IP_PREDIV_EXPECT;
		sprintf(name, "%s_PRE", clk_name);
		clk = sdrv_register_out_composite(np, base, cgu_clk, name);
		if (IS_ERR_OR_NULL(clk))
			return;
		ret = of_clk_add_provider(np, of_clk_src_simple_get, clk);
		if (ret) {
			clk_unregister(clk);
			return;
		}
		//post
		cgu_clk = kzalloc(sizeof(*cgu_clk), GFP_KERNEL);
		if (!cgu_clk)
			return;
		//sdrv,postdiv
		cgu_clk->type = CLK_TYPE_IP_POST;
		cgu_clk->div.shift = IP_POSTDIV_SHIFT;
		cgu_clk->div.width = IP_POSTDIV_WIDTH;
		cgu_clk->busywidth = IP_POSTDIV_BUSYWIDTH;
		cgu_clk->busyshift = IP_POSTDIV_BUSYSHIFT;
		cgu_clk->expect = IP_POSTDIV_EXPECT;
	} else if (type == CLK_TYPE_IP_COM) {
		//char name[32]={""};
		cgu_clk->type = CLK_TYPE_IP_COM;
		//cgu_clk->div.shift = IP_COMDIV_SHIFT;
		cgu_clk->div.width = IP_COMDIV_WIDTH;
		//cgu_clk->busywidth = IP_COMDIV_BUSYWIDTH;
		//cgu_clk->busyshift = IP_COMDIV_BUSYSHIFT;
		//cgu_clk->expect = IP_COMDIV_EXPECT;
		//sprintf(name, "%s_COM", clk_name);
	} else if (type == CLK_TYPE_UUU_MUX) {
		cgu_clk->type = CLK_TYPE_UUU_MUX;
		cgu_clk->busywidth = 0;
	} else if (type == CLK_TYPE_UUU_DIVIDER) {
		u32 shift, width;
		cgu_clk->type = CLK_TYPE_UUU_DIVIDER;
		if (of_property_read_u32(np, "sdrv,div-shift", &shift) < 0 ||
		    of_property_read_u32(np, "sdrv,div-width", &width) < 0) {
			BUG_ON(1);
			return;
		}
		cgu_clk->div.shift = shift;
		cgu_clk->div.width = width;
		cgu_clk->busywidth = 0;
	} else if (type == CLK_TYPE_UUU_MUX2) {
		cgu_clk->type = CLK_TYPE_UUU_MUX2;
		cgu_clk->busywidth = 0;
	} else if (type == CLK_TYPE_GATE) {
		cgu_clk->type = CLK_TYPE_GATE;
		cgu_clk->busywidth = 0;
	}

	clk = sdrv_register_out_composite(np, base, cgu_clk, clk_name);
	if (IS_ERR_OR_NULL(clk))
		return;
	ret = of_clk_add_provider(np, of_clk_src_simple_get, clk);
	if (ret) {
		clk_unregister(clk);
		return;
	}
}

CLK_OF_DECLARE(sdrv_ckgen_clk, "semidrive,clkgen-composite",
	       sdrv_ckgen_clk_init);
#ifdef CONFIG_ARCH_SEMIDRIVE_V9
static const struct of_device_id remote_clk_match[] __initconst = {
	{
		.compatible = "semidrive,clkgen-composite-remote",
		.data = sdrv_ckgen_clk_init,
	},
	{
		.compatible = "semidrive,pll-remote",
		.data = sdrv_pll_of_init,
	},
	{}
};

int __init remote_clk_init(void)
{
	pr_err("init remote_clk_init\n");
	of_clk_init(remote_clk_match);
	return 0;
}
late_initcall(remote_clk_init);
#endif
