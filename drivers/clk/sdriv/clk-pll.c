/*
 * clk-pll.c
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
#include <linux/of_address.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include "clk-pll.h"

#define DEBUG_PLL 0
#define PLL_DUMMY_ROOT 5

#define to_sdrv_cgu_pll(_hw) container_of(_hw, struct sdrv_cgu_pll_clk, hw)

/*2^24*/
#define FRACM (16777216)
static unsigned long sdrv_pll_calcurate_pll_cfg(const struct sdrv_pll_rate_table *rate)
{
	unsigned long tmp = 0;
	unsigned long freq = 0;

	if (rate->isint == 0)
		tmp = rate->frac*(1000000)/(FRACM);

	freq = (tmp + (rate->fbdiv*(1000000)))*(SOC_PLL_REFCLK_FREQ/(1000000))/rate->refdiv;
	return freq;
}
static void stuff_valid_value(struct sdrv_pll_rate_table *config)
{
	int i = 0;
	if (!(config->refdiv >= 1 && config->refdiv <= 63))
		config->refdiv = 1;

	for (i = 0; i < 2; i++) {
		if (!(config->postdiv[i] >= 1 && config->postdiv[i] <= 7))
			config->postdiv[i] = 1;
	}

	if (!(config->postdiv[0] >= config->postdiv[1]))
		config->postdiv[0] = config->postdiv[1];

	if (config->isint) {
		if (!(config->fbdiv >= 16 && config->fbdiv <= 640))
			config->fbdiv = 16;
	} else {
		if (!(config->fbdiv >= 20 && config->fbdiv <= 320))
			config->fbdiv = 20;

		if (!(config->frac >= 0 && config->frac <= FRACM))
			config->frac = FRACM;
	}
}

static unsigned long sdrv_pll_get_freq(int plltype,
		const struct sdrv_pll_rate_table *cur)
{
	unsigned long pll_cfg = sdrv_pll_calcurate_pll_cfg(cur);

	if (plltype == PLL_DIVA && cur->div[0] != -1)
		return pll_cfg/(cur->div[0] + 1);
	else if (plltype == PLL_DIVB && cur->div[1] != -1)
		return pll_cfg/(cur->div[1] + 1);
	else if (plltype == PLL_DIVC && cur->div[2] != -1)
		return pll_cfg/(cur->div[2] + 1);
	else if (plltype == PLL_DIVD && cur->div[3] != -1)
		return pll_cfg/(cur->div[3] + 1);
	else if (plltype == PLL_ROOT) //root
		return pll_cfg/cur->postdiv[0]/cur->postdiv[1];
	else if (plltype == PLL_DUMMY_ROOT)
		return pll_cfg;
	return 0;
}
static unsigned long sdrv_pll_get_freq_with_prate(int plltype,
		const struct sdrv_pll_rate_table *cur, const unsigned long prate)
{
	if (plltype == PLL_DIVA && cur->div[0] != -1)
		return prate/(cur->div[0] + 1);
	else if (plltype == PLL_DIVB && cur->div[1] != -1)
		return prate/(cur->div[1] + 1);
	else if (plltype == PLL_DIVC && cur->div[2] != -1)
		return prate/(cur->div[2] + 1);
	else if (plltype == PLL_DIVD && cur->div[3] != -1)
		return prate/(cur->div[3] + 1);
	else if (plltype == PLL_ROOT) //root
		return prate/cur->postdiv[0]/cur->postdiv[1];
	else if (plltype == PLL_DUMMY_ROOT)
		return sdrv_pll_get_freq(PLL_DUMMY_ROOT, cur);
	return 0;
}

static void sdrv_pll_get_params(struct sdrv_cgu_pll_clk *pll,
					struct sdrv_pll_rate_table *rate)
{
	sc_pfpll_getparams(pll->reg_base, &rate->fbdiv, &rate->refdiv,
		&rate->postdiv[0], &rate->postdiv[1], &rate->frac,
		&rate->div[0], &rate->div[1], &rate->div[2], &rate->div[3], &rate->isint);
#if DEBUG_PLL
	pr_err("pll %s reg_base 0x%lx fbdiv %d, refdiv %d, postdiv1 %d postdiv2 %d frac %ld, diva %d, divb %d divc %d divd %d, isint %d\n",
		clk_hw_get_name(&pll->hw), pll->reg_base,
		rate->fbdiv, rate->refdiv, rate->postdiv[0], rate->postdiv[1], rate->frac,
		rate->div[0], rate->div[1], rate->div[2], rate->div[3], rate->isint);
#endif
}

static void sdrv_pll_set_params(struct sdrv_cgu_pll_clk *pll,
					const struct sdrv_pll_rate_table *rate)
{
	sc_pfpll_setparams(pll->reg_base, rate->fbdiv, rate->refdiv,
		rate->postdiv[0], rate->postdiv[1], rate->frac,
		rate->div[0], rate->div[1], rate->div[2], rate->div[3], rate->isint);
#if DEBUG_PLL
	pr_err("set pll %s reg_base 0x%lx fbdiv %d, refdiv %d, postdiv1 %d postdiv2 %d frac %ld, diva %d, divb %d divc %d divd %d, isint %d\n",
		clk_hw_get_name(&pll->hw), pll->reg_base,
		rate->fbdiv, rate->refdiv, rate->postdiv[0], rate->postdiv[1],
		rate->frac, rate->div[0], rate->div[1], rate->div[2],
		rate->div[3], rate->isint);
#endif
}


static unsigned long sdrv_pll_recalc_rate(struct clk_hw *hw,
						     unsigned long prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	struct sdrv_pll_rate_table cur;
	u64 rate64 = prate;

	sdrv_pll_get_params(pll, &cur);
	rate64 = sdrv_pll_get_freq(pll->type, &cur);

	return (unsigned long)rate64;
}

static int sdrv_pll_enable(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	//enable
	//wait pll locked
	return 0;
}

static void sdrv_pll_disable(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	//disable
}

static int sdrv_pll_is_enabled(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	return true;
}
#if 0
static long sdrv_pll_round_rate(struct clk_hw *hw,
		unsigned long drate, unsigned long *prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	const struct sdrv_pll_rate_table *rate_table = pll->rate_table;
	int i;
	unsigned long rate = 0;

	for (i = 0; i < pll->rate_count; i++) {
		rate = sdrv_pll_get_freq(pll->type, &rate_table[i]);
		if (drate <= rate)
			return rate;
	}

	return rate;
}
#endif
static bool is_valid_pll_config(struct sdrv_pll_rate_table *conf)
{
	// check the pll limit
	//fvco 800MHZ~3200MHZ
	unsigned long fvco;
	unsigned int pfd;

	fvco = sdrv_pll_get_freq(PLL_DUMMY_ROOT, conf);

	if (fvco < 800000000 || fvco > 3200000000)
		return false;

	//pfd >10Mhz in fac mode. and >5Mhz in int mode
	pfd = 24000000 / conf->refdiv;

	if (conf->isint && pfd <= 5000000)
		return false;

	if (conf->isint == 0 && pfd <= 10000000)
		return false;

	return true;
}

unsigned long pll_translate_freq_to_config(unsigned long freq, int plldiv,
		struct sdrv_pll_rate_table *config, unsigned long *prate)
{
	unsigned long bestrate = 0, bestratediff = ULONG_MAX, rate;
	unsigned long diff;
	unsigned int maxdiv = 0, mindiv, bestdiv;
	struct sdrv_pll_rate_table conf;

	conf = *config;
	if (plldiv == PLL_DUMMY_ROOT) {
		int fbdiv = 1, refdiv = 1;
		int refdiv_max;
		int fbdiv_max, fbdiv_min;
		config->isint = 1;
		config->frac = 0;
		stuff_valid_value(config);
		conf = *config;
		if (conf.isint) {
			fbdiv_min = 16;
			fbdiv_max = 640;
			refdiv_max = 4;
		} else {
			fbdiv_min = 20;
			fbdiv_max = 320;
			refdiv_max = 2;
		}

		freq = min(freq, 3200000000UL);
		freq = max(freq, 800000000UL);

		for (refdiv = 1; refdiv <= refdiv_max; refdiv++) {
			int fbdivmin, fbdivmax;
			int fbtmp = ((freq * refdiv / 24) -
			(conf.frac * 1000000) / 16777216) / 1000000;
			fbdivmin = max(fbtmp - 2, fbdiv_min);
			fbdivmax = min(fbtmp + 2, fbdiv_max);

			for (fbdiv = fbdivmin; fbdiv <= fbdivmax; fbdiv++) {
				conf.fbdiv = fbdiv;
				conf.refdiv = refdiv;

				if (!is_valid_pll_config(&conf))
					continue;

				rate = sdrv_pll_get_freq(plldiv, &conf);
				diff = abs_diff(freq, rate);
				if (diff == 0) {
					config->fbdiv = conf.fbdiv;
					config->refdiv = conf.refdiv;
					return freq;
				}

				if (diff < bestratediff) {
					bestratediff = diff;
					bestrate = rate;
					config->fbdiv = conf.fbdiv;
					config->refdiv = conf.refdiv;
				}
			}
		}
		conf = *config;
		if (bestrate > freq && conf.isint == 1)
			conf.fbdiv--;

		conf.frac = ((freq / 1000 * conf.refdiv / 24) - conf.fbdiv * 1000) *
					16777216 / 1000;

		if (conf.frac)
			conf.isint = 0;

		if (!is_valid_pll_config(&conf))
			return bestrate;

		rate = sdrv_pll_get_freq(plldiv, &conf);
		*config = conf;
		//bestrate = rate;
		bestrate = freq;
	} else if (plldiv == PLL_ROOT) {
		int i, j;
		int postdiv0div_max = 7;
		int postdiv1div_max = 7;

		maxdiv = postdiv0div_max * postdiv1div_max;
		maxdiv = min((unsigned int)(ULONG_MAX / freq), maxdiv);
		mindiv = 1;
		//initial value of bestdiv is the fix div or current div.
		bestdiv = mindiv;

		for (i = 1; i <= postdiv0div_max; i++) {
			for (j = 1; j <= postdiv1div_max; j++) {
				if (((i * j) < (int)mindiv) ||
					((i * j) > (int)maxdiv))
					break;

				if (i < j) //post0 >= post1
					break;

				rate = pll_translate_freq_to_config(i * j * freq,
					PLL_DUMMY_ROOT, &conf, NULL);
				diff = abs_diff(freq, rate / (i * j));

				if (diff == 0) {
					*config = conf;
					config->postdiv[0] = i;
					config->postdiv[1] = j;
					*prate = i * j * freq;
					return freq;
				}

				if (diff < bestratediff) {
					bestratediff = diff;
					bestdiv = (i * j);
					bestrate = rate / bestdiv;
					*config = conf;
					config->postdiv[0] = i;
					config->postdiv[1] = j;
					*prate = i * j * freq;
				}
			}
		}
	} else if (plldiv >= PLL_DIVA && plldiv <= PLL_DIVD) {
		int div_max  = 16;
		int i;

		maxdiv = div_max;
		maxdiv = min((unsigned int)(ULONG_MAX / freq), maxdiv);
		mindiv = 1;
		maxdiv = max(maxdiv, mindiv);
		//initial value of bestdiv is the fix div or current div.
		bestdiv = mindiv;

		for (i = mindiv; i <= (int)maxdiv; i++) {
			rate = pll_translate_freq_to_config(i * freq,
				PLL_DUMMY_ROOT, &conf, NULL);
			diff = abs_diff(freq, rate / i);

			if (diff == 0) {
				*config = conf;
				config->div[plldiv - 1] = i - 1;
				*prate = i * freq;
				return freq;
			}

			if (diff < bestratediff) {
				bestratediff = diff;
				bestdiv = i;
				bestrate = rate / bestdiv;
				*config = conf;
				config->div[plldiv - 1] = i - 1;
				*prate = i * freq;
			}
		}
	}
	return bestrate;
}

unsigned long pll_translate_freq_to_config_with_prate(unsigned long freq, int plldiv,
		struct sdrv_pll_rate_table *config, unsigned long prate)
{
	unsigned long bestrate = 0, bestratediff = ULONG_MAX, rate;
	unsigned long diff;
	unsigned int maxdiv = 0, mindiv, bestdiv;
	struct sdrv_pll_rate_table conf;

	conf = *config;

	if (plldiv == PLL_DUMMY_ROOT) {
		int fbdiv = 1, refdiv = 1;
		int refdiv_max;
		int fbdiv_max, fbdiv_min;

		config->isint = 1;
		config->frac = 0;
		stuff_valid_value(config);
		conf = *config;
		if (conf.isint) {
			fbdiv_min = 16;
			fbdiv_max = 640;
			refdiv_max = 4;
		} else {
			fbdiv_min = 20;
			fbdiv_max = 320;
			refdiv_max = 2;
		}

		freq = min(freq, 3200000000UL);
		freq = max(freq, 800000000UL);

		for (refdiv = 1; refdiv <= refdiv_max; refdiv++) {
			int fbdivmin, fbdivmax;
			int fbtmp = ((freq * refdiv / 24) -
			(conf.frac * 1000000) / 16777216) / 1000000;
			fbdivmin = max(fbtmp - 2, fbdiv_min);
			fbdivmax = min(fbtmp + 2, fbdiv_max);

			for (fbdiv = fbdivmin; fbdiv <= fbdivmax; fbdiv++) {
				conf.fbdiv = fbdiv;
				conf.refdiv = refdiv;

				if (!is_valid_pll_config(&conf))
					continue;

				rate = sdrv_pll_get_freq(plldiv, &conf);
				diff = abs_diff(freq, rate);
				if (diff == 0) {
					config->fbdiv = conf.fbdiv;
					config->refdiv = conf.refdiv;
					return freq;
				}

				if (diff < bestratediff) {
					bestratediff = diff;
					bestrate = rate;
					config->fbdiv = conf.fbdiv;
					config->refdiv = conf.refdiv;
				}
			}
		}
		conf = *config;

		if (bestrate > freq && conf.isint == 1)
			conf.fbdiv--;

		conf.frac = ((freq / 1000 * conf.refdiv / 24) - conf.fbdiv * 1000) *
					16777216 / 1000;

		if (conf.frac)
			conf.isint = 0;

		if (!is_valid_pll_config(&conf))
			return bestrate;

		rate = sdrv_pll_get_freq(plldiv, &conf);
		*config = conf;
		bestrate = rate;
	} else if (plldiv == PLL_ROOT) {
		int i, j;
		int postdiv0div_max = 7;
		int postdiv1div_max = 7;

		maxdiv = postdiv0div_max * postdiv1div_max;
		maxdiv = min((unsigned int)(ULONG_MAX / freq), maxdiv);
		mindiv = 1;
		//initial value of bestdiv is the fix div or current div.
		bestdiv = mindiv;
		rate = prate;
		for (i = 1; i <= postdiv0div_max; i++) {
			for (j = 1; j <= postdiv1div_max; j++) {
				if (((i * j) < (int)mindiv) ||
					((i * j) > (int)maxdiv))
					break;

				if (i < j) //post0 >= post1
					break;

				//rate = pll_translate_freq_to_config(i * j * freq,
				//	PLL_DUMMY_ROOT, &conf);
				diff = abs_diff(freq, rate / (i * j));

				if (diff == 0) {
					*config = conf;
					config->postdiv[0] = i;
					config->postdiv[1] = j;
					return freq;
				}

				if (diff < bestratediff) {
					bestratediff = diff;
					bestdiv = (i * j);
					bestrate = rate / bestdiv;
					*config = conf;
					config->postdiv[0] = i;
					config->postdiv[1] = j;
				}
			}
		}
	} else if (plldiv >= PLL_DIVA && plldiv <= PLL_DIVD) {
		int div_max  = 16;
		int i;

		maxdiv = div_max;
		maxdiv = min((unsigned int)(ULONG_MAX / freq), maxdiv);
		mindiv = 1;
		maxdiv = max(maxdiv, mindiv);
		//initial value of bestdiv is the fix div or current div.
		bestdiv = mindiv;
		rate = prate;
		for (i = mindiv; i <= (int)maxdiv; i++) {
			//rate = pll_translate_freq_to_config(i * freq,
			//	PLL_DUMMY_ROOT, &conf);
			diff = abs_diff(freq, rate / i);

			if (diff == 0) {
				*config = conf;
				config->div[plldiv - 1] = i - 1;
				return freq;
			}

			if (diff < bestratediff) {
				bestratediff = diff;
				bestdiv = i;
				bestrate = rate / bestdiv;
				*config = conf;
				config->div[plldiv - 1] = i - 1;
			}
		}
	}
	return bestrate;
}

static long sdrv_pll_round_rate_moreprecise(struct clk_hw *hw,
		unsigned long drate, unsigned long *prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	struct sdrv_pll_rate_table *params = &pll->params;
	unsigned long rate = 0;
	int plltype = pll->type;

	sdrv_pll_get_params(pll, &pll->params);

	rate = pll_translate_freq_to_config(drate, plltype, params, prate);
	return rate;
}
static int sdrv_pll_set_rate_moreprecise(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	unsigned long rate, flags = 0;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);
	else
		__acquire(pll->lock);

	sdrv_pll_get_params(pll, &pll->params);
	//calculate
	rate = pll_translate_freq_to_config_with_prate(drate, pll->type, &pll->params, prate);

	// check prate
	//BUG_ON(prate != sdrv_pll_get_prate(pll->type, pll->params));

	sdrv_pll_set_params(pll, &pll->params);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);
	else
		__release(pll->lock);

	return 0;
}
static unsigned long sdrv_pll_recalc_rate_moreprecise(struct clk_hw *hw,
							 unsigned long prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	struct sdrv_pll_rate_table cur;
	u64 rate64 = prate;

	sdrv_pll_get_params(pll, &cur);
	rate64 = sdrv_pll_get_freq_with_prate(pll->type, &cur, prate);

	return (unsigned long)rate64;
}
static void sdrv_pll_init(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
}
#if 0

static int sdrv_pll_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	const struct sdrv_pll_rate_table *rate_table = pll->rate_table;
	int i;
	unsigned long rate, flags = 0;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);
	else
		__acquire(pll->lock);


	for (i = 0; i < pll->rate_count; i++) {
		rate = sdrv_pll_get_freq(pll->type, &rate_table[i]);
		if (drate <= rate) {
			sdrv_pll_set_params(pll, &rate_table[i]);
				if (pll->lock)
					spin_unlock_irqrestore(pll->lock, flags);
				else
					__release(pll->lock);
			return 0;
		}
	}

	sdrv_pll_set_params(pll, &rate_table[i-1]);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);
	else
		__release(pll->lock);

	return 0;
}


static const struct clk_ops sdrv_pll_clk_norate_ops = {
	.recalc_rate = sdrv_pll_recalc_rate,
	.enable = sdrv_pll_enable,
	.disable = sdrv_pll_disable,
	.is_enabled = sdrv_pll_is_enabled,
};

static const struct clk_ops sdrv_pll_clk_ops = {
	.recalc_rate = sdrv_pll_recalc_rate,
	.round_rate = sdrv_pll_round_rate,
	.set_rate = sdrv_pll_set_rate,
	.enable = sdrv_pll_enable,
	.disable = sdrv_pll_disable,
	.is_enabled = sdrv_pll_is_enabled,
	.init = sdrv_pll_init,
};
#endif
static ssize_t sdrv_pllss_read(struct file *filp, char __user *buffer,
			       size_t count, loff_t *ppos)
{
	struct sdrv_cgu_pll_clk *pll = filp->private_data;
	char tmp[100];
	size_t size;
	bool isint = false, enable = false, down = false;
	unsigned int spread = 0;

	sc_pfpll_get_ss(pll->reg_base, &isint, &enable, &down, &spread);
	size = sprintf(tmp, "isint %d, enable %d, down %d, spread %d\n", isint,
		       enable, down, spread);

	return simple_read_from_buffer(buffer, count, ppos, tmp, size);
}

static ssize_t sdrv_pllss_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct sdrv_cgu_pll_clk *pll = filp->private_data;
	unsigned int buf[100];
	bool enable = false, down = false;
	unsigned int spread = 0;
	int err;

	err = copy_from_user((char *)buf, (char __user *)buffer, count);
	if (err < 0)
		return -EFAULT;
	sscanf((const char *)buf, "%d %d %d", (int*)&enable, (int*)&down, &spread);
	if (spread > 31) {
		pr_err("spread should be 0~31, but input is %d\n", spread);
		return -EFAULT;
	}
	sc_pfpll_set_ss(pll->reg_base, enable, down, spread);
	return count;
}

static const struct file_operations sdrv_pllss_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = sdrv_pllss_read,
	.write = sdrv_pllss_write,
};
static int sdrv_pll_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);

	debugfs_create_file("spread", S_IRUSR | S_IWUSR, dentry, pll,
			    &sdrv_pllss_ops);
	return 0;
}
static const struct clk_ops sdrv_pll_clk_moreprecise_ops = {
	.recalc_rate = sdrv_pll_recalc_rate_moreprecise,
	.round_rate = sdrv_pll_round_rate_moreprecise,
	.set_rate = sdrv_pll_set_rate_moreprecise,
	.enable = sdrv_pll_enable,
	.disable = sdrv_pll_disable,
	.is_enabled = sdrv_pll_is_enabled,
	.init = sdrv_pll_init,
	.debug_init = sdrv_pll_debug_init,
};

static const struct clk_ops sdrv_pll_clk_readonly_ops = {
	.recalc_rate = sdrv_pll_recalc_rate,
	.enable = NULL,
	.disable = NULL,
	.is_enabled = sdrv_pll_is_enabled,
	.debug_init = sdrv_pll_debug_init,
};

static int sdrv_pll_pre_rate_change(struct sdrv_cgu_pll_clk *clk,
					   struct clk_notifier_data *ndata)
{
	int ret = 0;

	if (ndata->new_rate < clk->min_rate
			|| ndata->new_rate > clk->max_rate) {
		pr_err("want change clk %s freq from %ld to %ld, not allowd, min %ld max %ld\n",
			clk->name, ndata->old_rate, ndata->new_rate,
			clk->min_rate, clk->max_rate);
		ret = -EINVAL;
	}
	return ret;
}

static int sdrv_pll_post_rate_change(struct sdrv_cgu_pll_clk *clk,
					  struct clk_notifier_data *ndata)
{
	return 0;
}

static int sdrv_pll_notifier_cb(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct sdrv_cgu_pll_clk *clk = nb_to_sdrv_cgu_pll_clk(nb);
	int ret = 0;

	pr_debug("%s: event %lu, old_rate %lu, new_rate: %lu\n",
		 __func__, event, ndata->old_rate, ndata->new_rate);
	if (event == PRE_RATE_CHANGE)
		ret = sdrv_pll_pre_rate_change(clk, ndata);
	else if (event == POST_RATE_CHANGE)
		ret = sdrv_pll_post_rate_change(clk, ndata);

	return notifier_from_errno(ret);
}

static struct clk * __init get_clk_by_name(const char *name)
{
	struct of_phandle_args phandle;
	struct clk *clk = ERR_PTR(-ENODEV);

	phandle.np = of_find_node_by_name(NULL, name);

	if (phandle.np) {
		clk = of_clk_get_from_provider(&phandle);
		of_node_put(phandle.np);
	}
	return clk;
}

static struct clk *sdrv_register_pll(struct device_node *np, void __iomem *base, int type,
	const char *clk_name, struct sdrv_pll_rate_table *rate_table, bool readonly)
{
	const char *pll_parents[1];
	struct clk_init_data init;
	struct sdrv_cgu_pll_clk *pll;
	struct clk *pll_clk;
	u32 min_freq = 0, max_freq = 0;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	//pr_info("register pll type %d name %s\n", type, clk_name);
	pll->name = clk_name;
	pll->type = type;
	pll->isreadonly = readonly;
	if (base)
		pll->reg_base = base;
	/* now create the actual pll */
	init.name = clk_name;

	/* keep all plls untouched for now */
	init.flags = CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE;
	if (type == PLL_DUMMY_ROOT)
		init.num_parents = 0;
	else
		init.num_parents = 1;

	if(init.num_parents == 0) {
		if (pll->reg_base) {
			pll->lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
			if (pll->lock)
				spin_lock_init(pll->lock);
		} else {
			pll->lock = NULL;
		}
	} else {
		struct sdrv_cgu_pll_clk *p_clk = NULL;
		struct clk *pclk = get_clk_by_name(np->name);/*parent*/
		pll_parents[0] = np->name;
		if(IS_ERR(pclk)) {
			pr_err("parent name %s no exist, need register parent first\n", pll_parents[0]);
			BUG_ON(1);
		}
		p_clk = to_sdrv_cgu_pll(__clk_get_hw(pclk));
		
		init.parent_names = pll_parents;
		if (pll->reg_base && pll->reg_base == p_clk->reg_base)
			pll->lock = p_clk->lock;
	}

	if (rate_table) {
		int len;

		/* find count of rates in rate_table */
		for (len = 0; rate_table[len].refdiv != 0; )
			len++;

		pll->rate_count = len;
		pll->rate_table = kmemdup(rate_table,
					pll->rate_count *
					sizeof(struct sdrv_pll_rate_table),
					GFP_KERNEL);
		WARN(!pll->rate_table,
			"%s: could not allocate rate table for %s\n",
			__func__, clk_name);
	}

	if (pll->isreadonly) {
		init.ops = &sdrv_pll_clk_readonly_ops;
	} else {
		init.ops = &sdrv_pll_clk_moreprecise_ops;
		init.flags |= CLK_SET_RATE_PARENT;
	}

	pll->hw.init = &init;
#if DEBUG_PLL
	pr_err("pll %s\n", clk_name);
#endif
	pll_clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(pll_clk)) {
		pr_err("%s: failed to register pll clock %s : %ld\n",
			__func__, clk_name, PTR_ERR(pll_clk));
		return NULL;
	}

	if (sdrv_get_clk_min_rate(clk_name, &min_freq) == 0) {
		clk_set_min_rate(pll_clk, min_freq);
		pll->min_rate = min_freq;
	} else {
		pll->min_rate = 0;
	}
	if (sdrv_get_clk_max_rate(clk_name, &max_freq) == 0) {
		clk_set_max_rate(pll_clk, max_freq);
		pll->max_rate = max_freq;
	} else {
		pll->max_rate = ULONG_MAX;
	}
	pll->clk_nb.notifier_call = sdrv_pll_notifier_cb;
	if (clk_notifier_register(pll_clk, &pll->clk_nb)) {
		pr_err("%s: failed to register clock notifier for %s\n",
				__func__, pll->name);
	}
	return pll_clk;
}

void __init sdrv_pll_of_init(struct device_node *np)
{
	struct clk *clk;
	void __iomem *reg_base;
	int i = 0;
	char clk_name[32]={""};
	int ret;
	u32 div=-1, readonly = 0;
	struct sdrv_pll_rate_table *rate_table = NULL;
	struct clk_onecell_data *clk_data;
	struct clk **clks;

	if (!sdrv_clk_of_device_is_available(np))
		return;

	clks = kzalloc(sizeof(struct clk *)*(PLL_DUMMY_ROOT+1), GFP_KERNEL);
	if (!clks)
		return;
	clk_data = kzalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
	if (!clk_data) {
		kfree(clks);
		return;
	}
	clk_data->clks = clks;
	clk_data->clk_num = PLL_DUMMY_ROOT+1;

	reg_base = of_iomap(np, 0);
	if (!reg_base) {
		pr_err("%s: failed to map address range %s\n", __func__, np->name);
		goto err;
	}
	pr_debug("reg base %p name %s\n", reg_base, np->name);

	if (!of_property_read_u32(np, "sdrv,clk-readonly", &readonly)) {
		if(!readonly) {
			//init rate_table, TODO
			rate_table = NULL;
		}
	}
	for (i = PLL_DUMMY_ROOT; i >= 0; i--) {
		if (i == PLL_DUMMY_ROOT) {
			sprintf(clk_name, "%s", np->name);
		} else if (i == PLL_ROOT) {
			sprintf(clk_name, "%s_ROOT", np->name);
		} else if (i == PLL_DIVA) {
			if(!of_property_read_u32(np, "sdrv,pll-diva", &div))
				sprintf(clk_name, "%s_DIVA", np->name);
			else
				continue;
		} else if (i == PLL_DIVB) {
			if(!of_property_read_u32(np, "sdrv,pll-divb", &div))
				sprintf(clk_name, "%s_DIVB", np->name);
			else
				continue;
		} else if (i == PLL_DIVC) {
			if(!of_property_read_u32(np, "sdrv,pll-divc", &div))
				sprintf(clk_name, "%s_DIVC", np->name);
			else
				continue;
		} else if (i == PLL_DIVD) {
			if(!of_property_read_u32(np, "sdrv,pll-divd", &div))
				sprintf(clk_name, "%s_DIVD", np->name);
			else
				continue;
		}

		clk = sdrv_register_pll(np, reg_base, i, clk_name,
					rate_table, readonly);

		if (IS_ERR(clk))
			goto err;

		clks[i] = clk;
		ret = of_clk_add_provider(np, of_clk_src_simple_get, clk);
		if (ret) {
			clk_unregister(clk);
			goto err;
		}
	}

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	return;
err:
	kfree(clks);
	kfree(clk_data);
}

CLK_OF_DECLARE(sdrv_pll, "semidrive,pll", sdrv_pll_of_init);

