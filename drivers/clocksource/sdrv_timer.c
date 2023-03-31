/*
 * sdrv_timer.c
 *
 * Copyright (c) 2018 Semidrive Semiconductor.
 * All rights reserved.
 *
 * Description: implement semidrive timer driver
 *
 * Revision History:
 * -----------------
 * 011, 01/24/2019 chenqing implement this
 */


#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/sched_clock.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <clocksource/sdrv_timer.h>
#include "sdrv_timer_reg.h"

#define TIMER_MAX_NUM 5
#define PLAT_TIMER_INST 0 /*instance 0 as platform timer*/
/*for op_mode*/
#define OP_MODE_TIMER 0

struct sdrv_timer_context_t {
	sdrv_timer_t *tmr;
	int irq;
	u32 freq;
	int div;
	u64 interval; /*cnt*/
	void *arg;
	bool isperiodic;
	bool inited;
	u32 cnt_per_ms;
	u32 cnt_per_us;
	struct clocksource cs;
	struct clock_event_device ced;
	/*WA*/
	u32 g0_tick_saved;
	u32 g1_tick_saved;
	u32 a_tick_saved;
};

static struct sdrv_timer_context_t g_tmr[TIMER_MAX_NUM];
static inline struct sdrv_timer_context_t *
clocksource_to_sdrv_ctx(struct clocksource *cs)
{
	return container_of(cs, struct sdrv_timer_context_t, cs);
}
struct sdrv_timer_context_t *soc_get_module_inst(int id)
{
	WARN_ON(id >= TIMER_MAX_NUM);
	return &g_tmr[id];
}
u32 sdrv_tmr_init(int id, void __iomem *iobase, int irq, u32 freq)
{
	u32 val;
	struct sdrv_timer_context_t *tmr_ctx = soc_get_module_inst(id);
	sdrv_timer_t *tmr = (sdrv_timer_t *)iobase;

	tmr_ctx->tmr = tmr;
	tmr_ctx->irq = irq;
	tmr_ctx->freq = freq;
	tmr_ctx->div = 2;

	tmr_ctx->g0_tick_saved = tmr->cnt_g0;
	tmr_ctx->g1_tick_saved = tmr->cnt_g1;
	pr_info("g0 %d, g1 %d\n", tmr_ctx->g0_tick_saved,
				tmr_ctx->g1_tick_saved);

	val = tmr->tim_clk_config;
	val &= ~(FM_TIM_CLK_CONFIG_SRC_CLK_SEL | FM_TIM_CLK_CONFIG_DIV_NUM);
	val |= (FV_TIM_CLK_CONFIG_SRC_CLK_SEL(2)
		| FV_TIM_CLK_CONFIG_DIV_NUM(tmr_ctx->div));
	tmr->tim_clk_config = val;

	tmr->cnt_g0_ovf = 0xFFFFFFFF;
	tmr->cnt_g1_ovf = 0xFFFFFFFF;

	tmr->cnt_config |= (BM_CNT_CONFIG_CASCADE_MODE
		| BM_CNT_CONFIG_CNT_G1_RLD | BM_CNT_CONFIG_CNT_G0_RLD);

	while ((BM_CNT_CONFIG_CNT_G1_RLD | BM_CNT_CONFIG_CNT_G0_RLD)
		& tmr->cnt_config);

	pr_info("reload finished\n");
	/* TODO: see if we have chance to remove this workaround */
	/* it takes 3 refclk until the reloading being completed.
	 * Polling the tick until it is smaller than previous saving
	 */
	pr_info("c g0 %d, g1 %d\n", tmr->cnt_g0, tmr->cnt_g1);
	while (!((tmr->cnt_g0 == 0 || tmr->cnt_g0 < tmr_ctx->g0_tick_saved)
		&& (tmr->cnt_g1 == 0
			|| tmr->cnt_g1 < tmr_ctx->g1_tick_saved)));

	tmr_ctx->inited = true;

	tmr_ctx->cnt_per_ms = (tmr_ctx->freq / (tmr_ctx->div + 1)) / 1000;
	tmr_ctx->cnt_per_us = tmr_ctx->cnt_per_ms / 1000;
	pr_info("init finished cnt per ms %d, cnt per us %d\n",
		tmr_ctx->cnt_per_ms, tmr_ctx->cnt_per_us);
	return 0;
}

u64 sdrv_tmr_tick(int id)
{
	struct sdrv_timer_context_t *tmr_ctx = soc_get_module_inst(id);
	sdrv_timer_t *tmr = tmr_ctx->tmr;

	return ((u64)tmr->cnt_g0 | (((u64)tmr->cnt_g1) << 32U));
}

static int set_sdrv_timer(int id, void *callback, void *arg, ulong interval,
	bool isperiodic)
{
	u32 config;
	struct sdrv_timer_context_t *tmr_ctx = soc_get_module_inst(id);
	sdrv_timer_t *tmr = tmr_ctx->tmr;

	if (!tmr_ctx->inited)
		return -1;
	tmr_ctx->interval = interval;  //cnt
	tmr_ctx->arg = arg;
	tmr_ctx->isperiodic = isperiodic;

	/* enable timer */
	tmr->cnt_local_a_init = 0;
	tmr->cnt_local_a_ovf = tmr_ctx->interval;
	tmr_ctx->a_tick_saved = tmr->cnt_local_a;
	config = tmr->cnt_config;
	config |= BM_CNT_CONFIG_CNT_LOCAL_A_RLD;
	tmr->cnt_config = config;

	while (BM_CNT_CONFIG_CNT_LOCAL_A_RLD & tmr->cnt_config);
	//WA
	while (!(tmr->cnt_local_a < tmr_ctx->a_tick_saved));
	/* enable irq */
	tmr->int_sta_en |= BM_INT_STA_EN_CNT_LOCAL_A_OVF;
	tmr->int_sig_en |= BM_INT_SIG_EN_CNT_LOCAL_A_OVF;

	return 0;
}

static irqreturn_t sd_timer_handler(int irq, void *dev_id)
{

	struct sdrv_timer_context_t *tmr_ctx = (struct sdrv_timer_context_t *)dev_id;
	sdrv_timer_t *tmr = tmr_ctx->tmr;
	u32 int_sta = tmr->int_sta;
	struct clock_event_device *ced = &tmr_ctx->ced;

	if (int_sta & BM_INT_STA_CNT_LOCAL_A_OVF) {
		//disable irq
		tmr->int_sig_en &= ~(BM_INT_SIG_EN_CNT_LOCAL_A_OVF);
		tmr->int_sta_en &= ~(BM_INT_STA_EN_CNT_LOCAL_A_OVF);
		//clear irq
		tmr->int_sta |= BM_INT_STA_CNT_LOCAL_A_OVF;

		//reset the timer?
		if (tmr_ctx->isperiodic) {
			u32 config;
			//enable timer
			tmr->cnt_local_a_init = 0;
			tmr->cnt_local_a_ovf = tmr_ctx->interval;
			tmr_ctx->a_tick_saved = tmr->cnt_local_a;
			config = tmr->cnt_config;
			config |= BM_CNT_CONFIG_CNT_LOCAL_A_RLD;
			tmr->cnt_config = config;
			while (BM_CNT_CONFIG_CNT_LOCAL_A_RLD & tmr->cnt_config);
			//WA
			while (!(tmr->cnt_local_a < tmr_ctx->a_tick_saved));
			//enable irq
			tmr->int_sta_en |= BM_INT_STA_EN_CNT_LOCAL_A_OVF;
			tmr->int_sig_en |= BM_INT_SIG_EN_CNT_LOCAL_A_OVF;
		}
		ced->event_handler(ced);
		return IRQ_HANDLED;
	} else {
		WARN_ON(1);
	}
	return IRQ_NONE;
}
static void __init timer_get_base_and_rate(struct device_node *np,
				void __iomem **base, u32 *rate)
{
	struct clk *timer_clk;
	struct clk *pclk;

	*base = of_iomap(np, 0);

	if (!*base)
		panic("Unable to map regs for %s", np->name);

	/*
	 * Not all implementations use a periphal clock, so don't panic
	 * if it's not present
	 */
	pclk = of_clk_get_by_name(np, "pclk");
	if (!IS_ERR(pclk))
		if (clk_prepare_enable(pclk))
			pr_warn("pclk for %s is present, but could not be activated\n",
				np->name);

	timer_clk = of_clk_get_by_name(np, "timer");
	if (IS_ERR(timer_clk))
		goto try_clock_freq;

	if (!clk_prepare_enable(timer_clk)) {
		*rate = clk_get_rate(timer_clk);
		return;
	}

try_clock_freq:
	if (of_property_read_u32(np, "clock-freq", rate) &&
		of_property_read_u32(np, "clock-frequency", rate))
		panic("No clock nor clock-frequency property for %s", np->name);
}

static u64 sdrv_read_clocksource(struct clocksource *cs)
{
	u64 current_count;
	struct sdrv_timer_context_t *tmr_ctx =
		clocksource_to_sdrv_ctx(cs);
	sdrv_timer_t *tmr = tmr_ctx->tmr;

	current_count = ((u64)tmr->cnt_g0 | (((u64)tmr->cnt_g1) << 32U));

	return current_count;
}
static void sdrv_restart_clocksource(struct clocksource *cs)
{
	//sdrv_timer_context_t *tmr_ctx =
	//        clocksource_to_sdrv_ctx(cs);
	//start timer
}
static u32 sched_rate = 0;
u32 kunlun_arch_timer_get_rate(void)
{
	if (sched_rate == 0)
		return arch_timer_get_rate();
	else
		return sched_rate;
}
static u64 notrace read_sched_clock(void);
u64 sdrv_timer_get_cnt(void)
{
	if (sched_rate == 0)
		return arch_timer_reg_read_stable(cntvct_el0);
	else
		return read_sched_clock();
}
#if defined(CONFIG_GK20A_PCI)
EXPORT_SYMBOL(sdrv_timer_get_cnt);
#endif
static int sd_timer_set_next_event(unsigned long evt,
					struct clock_event_device *clk)
{
	set_sdrv_timer(PLAT_TIMER_INST, NULL, clk, evt, false);
	return 0;
}
static int sd_timer_shutdown(struct clock_event_device *clk)
{
	struct sdrv_timer_context_t *tmr_ctx = soc_get_module_inst(PLAT_TIMER_INST);
	sdrv_timer_t *tmr = tmr_ctx->tmr;

	if (!tmr_ctx->inited)
		return -1;

	//disable irq
	tmr->int_sig_en &= ~(BM_INT_SIG_EN_CNT_LOCAL_A_OVF);
	tmr->int_sta_en &= ~(BM_INT_STA_EN_CNT_LOCAL_A_OVF);
	//clear irq
	tmr->int_sta |= BM_INT_STA_CNT_LOCAL_A_OVF;
	return 0;
}
static void __init add_clockevent(struct device_node *event_timer)
{
	struct sdrv_timer_context_t *tmr_ctx = soc_get_module_inst(PLAT_TIMER_INST);
	int ret;

	tmr_ctx->ced.irq = tmr_ctx->irq;
	tmr_ctx->ced.features = CLOCK_EVT_FEAT_ONESHOT;
	tmr_ctx->ced.features |= CLOCK_EVT_FEAT_DYNIRQ;
	tmr_ctx->ced.name = "sd_timer";
	tmr_ctx->ced.rating = 300;
	tmr_ctx->ced.cpumask = cpu_all_mask;
	tmr_ctx->ced.set_state_shutdown = sd_timer_shutdown;
	tmr_ctx->ced.set_state_oneshot_stopped = sd_timer_shutdown;
	tmr_ctx->ced.set_next_event = sd_timer_set_next_event;
	tmr_ctx->ced.set_state_shutdown(&tmr_ctx->ced);

	clockevents_config_and_register(&tmr_ctx->ced, sched_rate, 0xf, 0x7fffffff);

	ret = request_irq(tmr_ctx->irq, sd_timer_handler, IRQF_TIMER, "sd_timer", tmr_ctx);
	if (ret)
		pr_err("Failed to request timer irq %d\n", tmr_ctx->irq);
}
static void __init add_clocksource(struct device_node *source_timer)
{
	void __iomem *iobase;
	struct sdrv_timer_context_t *tmr_ctx = soc_get_module_inst(PLAT_TIMER_INST);
	u32 irq, rate;

	timer_get_base_and_rate(source_timer, &iobase, &rate);
	pr_info("rate %d\n", rate);
	irq = irq_of_parse_and_map(source_timer, 0);
	sdrv_tmr_init(0, iobase, irq, rate);
	tmr_ctx->cs.name = source_timer->name;
	tmr_ctx->cs.rating = 300;
	tmr_ctx->cs.read = sdrv_read_clocksource;
	tmr_ctx->cs.mask = CLOCKSOURCE_MASK(64);
	tmr_ctx->cs.flags = CLOCK_SOURCE_IS_CONTINUOUS;
	tmr_ctx->cs.resume = sdrv_restart_clocksource;

	//clocksource_register_hz(&tmr_ctx->cs, tmr_ctx->freq/tmr_ctx->div);

	/*
	 * Fallback to use the clocksource as sched_clock if no separate
	 * timer is found. sched_io_base then points to the current_value
	 * register of the clocksource timer.
	 */
	//sched_io_base = iobase + 0x04;
	sched_rate = rate / (tmr_ctx->div + 1);
}
static u64 notrace read_sched_clock(void)
{
	u64 current_count;
	struct sdrv_timer_context_t *tmr_ctx = soc_get_module_inst(PLAT_TIMER_INST);
	sdrv_timer_t *tmr = tmr_ctx->tmr;

	current_count = ((u64)tmr->cnt_g0 | (((u64)tmr->cnt_g1) << 32U));

	return current_count;
}
static int __init sd_timer_init(struct device_node *timer)
{
	u32 val = 0;
	int ret;
	ret = of_property_read_u32(timer, "op-mode", &val);
	if (ret < 0) {
		pr_info("op-mode is not config, default enable timer\n");
		val = OP_MODE_TIMER;
	}
	if (OP_MODE_TIMER == val) //timer op-mode
	{
		pr_info("%s: found clocksource timer\n", __func__);
		add_clocksource(timer);
		sched_clock_register(read_sched_clock, 64, sched_rate);
		add_clockevent(timer);
	}
	return 0;
}

TIMER_OF_DECLARE(sd_timer, "sd,sd-timer", sd_timer_init);
