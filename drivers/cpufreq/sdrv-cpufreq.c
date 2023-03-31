/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define DEFAULT_MIN_FREQ		100000000
#define DEFAULT_MAX_FREQ		(U32_MAX)
#define DEFAULT_DELAY_TIME		30         /* second */
#define PLL1_DIVA_FIX_FREQ		500000000

struct sdrv_cpufreq_data {
	bool static_freqtable;
	unsigned int min_freq;
	unsigned int max_freq;
	unsigned int transition_delay_us;
	unsigned int start_time;
	unsigned long dvfs_start_time;
	struct cpufreq_frequency_table *freq_table;
	struct device_node *pdev_np;
	struct clk *cpu_clk;
	struct clk *pll_clk;
	struct clk *ckgen_clk;
	struct clk *sel_clk;
};
static struct sdrv_cpufreq_data cpufreq_data;

static int sdrv_set_target(struct cpufreq_policy *policy, unsigned int index)
{
	unsigned long new_freq;
	struct sdrv_cpufreq_data *data = policy->driver_data;

	if (data->start_time && time_before(jiffies, data->dvfs_start_time))
		return 0;

	new_freq = data->freq_table[index].frequency;

	if (data->static_freqtable) {
		/* switch to backup clock */
		clk_set_parent(data->sel_clk, data->ckgen_clk);
		/* update the pll clock */
		clk_set_rate(data->pll_clk, new_freq * 1000);
		/* switch to pll clock again */
		clk_set_parent(data->sel_clk, data->pll_clk);
	}

	/* update cpu clock freq */
	clk_set_rate(policy->clk, new_freq * 1000);

	return 0;
}

static int sdrv_cpufreq_create_dynamic_freqtable(struct sdrv_cpufreq_data *data)
{
	unsigned int freq, rate, minfreq;
	unsigned int i, steps;

	minfreq = (clk_round_rate(data->cpu_clk, data->min_freq)) / 1000;
	freq = (clk_round_rate(data->cpu_clk, data->max_freq)) / 1000;

	pr_info("get clk: min %d, max %d\n", minfreq, freq);

	steps = fls(freq / minfreq) + 1;
	data->freq_table = kcalloc(steps, sizeof(*(data->freq_table)),
				   GFP_KERNEL);
	if (!data->freq_table)
		return -ENOMEM;

	for (i = 0; i < (steps - 1); i++) {
		rate = clk_round_rate(data->cpu_clk, freq * 1000) / 1000;

		data->freq_table[i].frequency = rate;

		freq /= 2;
	}

	data->freq_table[steps - 1].frequency = CPUFREQ_TABLE_END;

	return 0;
}

static int sdrv_cpufreq_parse_static_freqtable(struct sdrv_cpufreq_data *data)
{
	struct device_node *np = data->pdev_np;
	unsigned int i, steps;
	int rate;

	steps = of_property_count_u32_elems(np, "frequency-list");
	if (steps <= 0) {
		pr_err("frequency list is invalid\n");
		return -EINVAL;
	}
	steps += 1;

	data->freq_table = kcalloc(steps, sizeof(*(data->freq_table)),
				   GFP_KERNEL);
	if (!data->freq_table)
		return -ENOMEM;

	for (i = 0; i < (steps - 1); i++) {
		of_property_read_u32_index(np, "frequency-list", i, &rate);
		data->freq_table[i].frequency = rate / 1000;
	}

	data->freq_table[steps - 1].frequency = CPUFREQ_TABLE_END;

	return 0;
}

static int sdrv_cpufreq_extra_clk_init(struct sdrv_cpufreq_data *data,
				      struct device *dev)
{
	int retval = 0;

	data->pll_clk = clk_get(dev, "pll");
	if (IS_ERR_OR_NULL(data->pll_clk)) {
		pr_err("could not get pll clk\n");
		return PTR_ERR(data->pll_clk);
	}

	data->ckgen_clk = clk_get(dev, "ckgen");
	if (IS_ERR_OR_NULL(data->ckgen_clk)) {
		pr_err("could not get ckgen core clk\n");
		retval = PTR_ERR(data->ckgen_clk);
		goto free_pll_clk;
	}
	clk_set_rate(data->ckgen_clk, PLL1_DIVA_FIX_FREQ);

	data->sel_clk = clk_get(dev, "sel0");
	if (IS_ERR_OR_NULL(data->sel_clk)) {
		pr_err("could not get sel0 clk\n");
		retval = PTR_ERR(data->sel_clk);
		goto free_ckgen_clk;
	}

	return 0;

free_ckgen_clk:
	clk_put(data->ckgen_clk);
free_pll_clk:
	clk_put(data->pll_clk);

	return retval;
}

static int sdrv_cpufreq_extra_clk_deinit(struct sdrv_cpufreq_data *data)
{
	if (data->pll_clk)
		clk_put(data->pll_clk);
	if (data->ckgen_clk)
		clk_put(data->ckgen_clk);
	if (data->sel_clk)
		clk_put(data->sel_clk);

	return 0;
}

static int sdrv_cpufreq_driver_init(struct cpufreq_policy *policy)
{
	struct sdrv_cpufreq_data *data = &cpufreq_data;
	struct device *cpu_dev;
	int retval = 0;

	if (policy->cpu != 0)
		return -EINVAL;

	cpumask_setall(policy->cpus);

	cpu_dev = get_cpu_device(policy->cpu);
	if (IS_ERR_OR_NULL(cpu_dev)) {
		pr_err("failed to get cpu%d device\n", policy->cpu);
		return -ENODEV;
	}

	data->cpu_clk = clk_get(cpu_dev, "cpu0");
	if (IS_ERR_OR_NULL(data->cpu_clk)) {
		pr_err("could not get cpu clk\n");
		return PTR_ERR(data->cpu_clk);
	}

	if (data->static_freqtable) {
		retval = sdrv_cpufreq_extra_clk_init(data, cpu_dev);
		if (retval)
			goto err;

		retval = sdrv_cpufreq_parse_static_freqtable(data);
		if (retval)
			goto err;
	} else {
		retval = sdrv_cpufreq_create_dynamic_freqtable(data);
		if (retval)
			goto err;
	}

	policy->clk = data->cpu_clk;
	policy->cpuinfo.transition_latency = 0;
	policy->transition_delay_us = data->transition_delay_us;
	policy->driver_data = data;

	if (cpufreq_table_validate_and_show(policy, data->freq_table)) {
		retval = -EINVAL;
		goto err;
	}

	pr_info("semidrive CPU frequency driver\n");
	data->dvfs_start_time = jiffies + (data->start_time * HZ);

	return 0;

err:
	kfree(data->freq_table);

	if (data->static_freqtable)
		sdrv_cpufreq_extra_clk_deinit(data);
	if (data->cpu_clk)
		clk_put(data->cpu_clk);

	return retval;
}

static struct cpufreq_driver sdrv_cpufreq_driver = {
	.name		= "sdrv cpufreq",
	.init		= sdrv_cpufreq_driver_init,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= sdrv_set_target,
	.get		= cpufreq_generic_get,
	.flags		= CPUFREQ_STICKY,
	.attr		= cpufreq_generic_attr,
};

static int sdrv_cpufreq_probe(struct platform_device *pdev)
{
	struct sdrv_cpufreq_data *data = &cpufreq_data;
	struct device_node *np;
	int ret;

	np = pdev->dev.of_node;
	if (!np)
		return -ENODEV;

	if (of_property_read_u32(np, "min-freq", &data->min_freq) < 0) {
		pr_err("no valid min-freq, set as default\n");
		data->min_freq = DEFAULT_MIN_FREQ;
	}

	if (of_property_read_u32(np, "max-freq", &data->max_freq) < 0) {
		pr_err("no valid max-freq, set as default\n");
		data->max_freq = DEFAULT_MAX_FREQ;
	}

	if (of_property_read_u32(np, "trans-delay-us",
				 &data->transition_delay_us) < 0) {
		pr_err("no valid trans-delay-us, set as default\n");
		data->transition_delay_us = 0;
	}

	if (of_property_read_u32(np, "start-time", &data->start_time) < 0) {
		pr_err("no valid start-time, set as default\n");
		data->start_time = DEFAULT_DELAY_TIME;
	}

	ret = of_property_count_u32_elems(np, "frequency-list");
	if (ret > 0)
		data->static_freqtable = true;
	else
		data->static_freqtable = false;

	data->pdev_np = np;

	pr_info("get prop min %d, max %d, period %d us start delay %d s\n",
		data->min_freq, data->max_freq, data->transition_delay_us,
		data->start_time);

	return cpufreq_register_driver(&sdrv_cpufreq_driver);
}

static const struct of_device_id sdrv_cpufreq_dt_match[] = {
	{ .compatible = "semidrive,sdrv-cpufreq" },
	{}
};

static struct platform_driver sdrv_cpufreq_platform_driver = {
	.driver = {
		.name = "sdrv-cpufreq",
		.owner = THIS_MODULE,
		.of_match_table = sdrv_cpufreq_dt_match,
	},
	.probe = sdrv_cpufreq_probe,
};

static int __init sdrv_cpufreq_init(void)
{
	return platform_driver_register(&sdrv_cpufreq_platform_driver);
}

static void __exit sdrv_cpufreq_exit(void)
{
	platform_driver_unregister(&sdrv_cpufreq_platform_driver);
}

late_initcall(sdrv_cpufreq_init);
module_exit(sdrv_cpufreq_exit);

MODULE_AUTHOR("Semidrive Corporation");
MODULE_DESCRIPTION("Cpufreq driver for Semidrive Series SoCs");
MODULE_LICENSE("GPL v2");
