/*
 * sc_pfpll.c
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
#include "sc_pfpll.h"
#include "sc_pfpll_reg.h"
#include <linux/clk.h>
#include <linux/clk-provider.h>

#define FBDIV_MIN 16
#define FBDIV_MAX 640
#define FBDIV_MIN_IN_FRAC_MODE 20
#define FBDIV_MAX_IN_FRAC_MODE 320
#define REFDIV_MIN 1
#define REFDIV_MAX 63
#define POSTDIV1_MIN 1
#define POSTDIV1_MAX 7
#define POSTDIV2_MIN 1
#define POSTDIV2_MAX 7

#define VCO_FREQ_MIN (800 * 1000 * 1000U)
#define VCO_FREQ_MAX (3200 * 1000 * 1000U)

/*
 * PLLs sourced from external OSC (24MHz) in ATB
 *
 * The PLLs involved in ATB are in integer mode.
 */

/*
 * Fout = (Fref/RefDiv)*(FbDiv+(Frac/2^24))/PostDiv
 *  PostDiv = PostDiv1 * PostDiv2
 *
 * Something shall be followed:
 *  1) PostDiv1 >= PostDiv2
 *  2) 800MHz <= VCO_FREQ <= 3200MHz
 *  3) 16 <= FBDIV <= 640 for INT mode
 *  4) 20 <= FBDIV <= 320 for Frac mode
 *
 *  There are four divs (A/B/C/D, in pll wrapper). Their default values are
 *  properly set per integration requirement. Usually, there is no need for
 *  ATB to change them.
 */
/**
 * @para
 *      freq    the frequency on which PLL to be programmed to run, in Hz.
 */
void sc_pfpll_setparams(void __iomem *base, unsigned int fbdiv,
			unsigned int refdiv, unsigned int postdiv1,
			unsigned int postdiv2, unsigned long frac, int diva,
			int divb, int divc, int divd, int isint)
{
	void __iomem *b = base;
	u32 ctrl = clk_readl(b + PLL_CTRL_OFF);
	u32 div = clk_readl(b + PLL_DIV_OFF);
	u32 v = 0;
	u32 freq_vco = 0;

	WARN_ON(!((refdiv >= REFDIV_MIN) && (refdiv <= REFDIV_MAX)));
	WARN_ON(!((fbdiv >= FBDIV_MIN) && (fbdiv <= FBDIV_MAX)));

	freq_vco = (SOC_PLL_REFCLK_FREQ / refdiv) * fbdiv;
	WARN_ON(!((freq_vco >= VCO_FREQ_MIN) && (freq_vco <= VCO_FREQ_MAX)));

	/* disable pll, gate-off outputs, power-down postdiv */
	/* the clock gates need to be off to protect the divs behind them from
 * being malfunctioned by glitch.
 * Once div been feed (i.e, the vco is stable), the div value can be
 * updated on the fly.
 */
	if ((ctrl & BM_PLL_CTRL_PLLEN) == BM_PLL_CTRL_PLLEN) {
		ctrl &= ~(BM_PLL_CTRL_PLL_DIVD_CG_EN | BM_PLL_CTRL_PLL_DIVC_CG_EN |
			  BM_PLL_CTRL_PLL_DIVB_CG_EN | BM_PLL_CTRL_PLL_DIVA_CG_EN |
			  BM_PLL_CTRL_PLLPOSTCG_EN);
		clk_writel(ctrl, b + PLL_CTRL_OFF);
		ctrl = clk_readl(b + PLL_CTRL_OFF);
		ctrl &= ~BM_PLL_CTRL_PLLEN;
		clk_writel(ctrl, b + PLL_CTRL_OFF);
	}
	clk_writel(fbdiv, b + PLL_FBDIV_OFF);
	/* update refdiv. */
	div &= ~FM_PLL_DIV_REFDIV;
	div |= FV_PLL_DIV_REFDIV(refdiv);
	clk_writel(div, b + PLL_DIV_OFF);
	/*  frac */
	if (isint) {
		/* Integer mode. */
		ctrl = clk_readl(b + PLL_CTRL_OFF);
		ctrl &= ~BM_PLL_CTRL_INT_MODE;
		ctrl |= 1 << BS_PLL_CTRL_INT_MODE;
		clk_writel(ctrl, b + PLL_CTRL_OFF);
	} else {
		/* Fractional mode. */
		ctrl = clk_readl(b + PLL_CTRL_OFF);
		ctrl &= ~BM_PLL_CTRL_INT_MODE;
		clk_writel(ctrl, b + PLL_CTRL_OFF);

		/* Configure fbdiv fractional part.*/
		v = clk_readl(b + PLL_FRAC_OFF);
		v &= ~FM_PLL_FRAC_FRAC;
		v |= FV_PLL_FRAC_FRAC(frac);
		clk_writel(v, b + PLL_FRAC_OFF);
	}

	/* enable PLL, VCO starting... */
	ctrl |= BM_PLL_CTRL_PLLEN;
	clk_writel(ctrl, b + PLL_CTRL_OFF);
	/* max lock time: 250 input clock cycles for freqency lock and 500 cycles
 * for phase lock. For fref=25MHz, REFDIV=1, locktime is 5.0us and 10us
 */
	while (!(clk_readl(b + PLL_CTRL_OFF) & BM_PLL_CTRL_LOCK))
		;
	/* power up and update post div */
	ctrl |= BM_PLL_CTRL_FOUTPOSTDIVEN;
	clk_writel(ctrl, b + PLL_CTRL_OFF);
	div &= ~(FM_PLL_DIV_POSTDIV1 | FM_PLL_DIV_POSTDIV2);
	div |= FV_PLL_DIV_POSTDIV1(postdiv1) | FV_PLL_DIV_POSTDIV2(postdiv2);
	clk_writel(div, b + PLL_DIV_OFF);

	v = clk_readl(b + PLL_OUT_DIV_1_OFF);
	v &= ~(FM_PLL_OUT_DIV_1_DIV_NUM_A | FM_PLL_OUT_DIV_1_DIV_NUM_B);
	v |= FV_PLL_OUT_DIV_1_DIV_NUM_A(diva) |
	     FV_PLL_OUT_DIV_1_DIV_NUM_B(divb);
	clk_writel(v, b + PLL_OUT_DIV_1_OFF);

	v = clk_readl(b + PLL_OUT_DIV_2_OFF);
	v &= ~(FM_PLL_OUT_DIV_2_DIV_NUM_C | FM_PLL_OUT_DIV_2_DIV_NUM_D);
	v |= FV_PLL_OUT_DIV_2_DIV_NUM_C(divc) |
	     FV_PLL_OUT_DIV_2_DIV_NUM_D(divd);
	clk_writel(v, b + PLL_OUT_DIV_2_OFF);

	/* these DIVs and CGs is outside sc pll IP, they are in a wrapper */
	ctrl |= BM_PLL_CTRL_PLLPOSTCG_EN;
	if (diva != -1)
		ctrl |= BM_PLL_CTRL_PLL_DIVA_CG_EN;
	if (divb != -1)
		ctrl |= BM_PLL_CTRL_PLL_DIVB_CG_EN;
	if (divc != -1)
		ctrl |= BM_PLL_CTRL_PLL_DIVC_CG_EN;
	if (divd != -1)
		ctrl |= BM_PLL_CTRL_PLL_DIVD_CG_EN;

	clk_writel(ctrl, b + PLL_CTRL_OFF);
}

void sc_pfpll_getparams(void __iomem *base, unsigned int *fbdiv,
			unsigned int *refdiv, unsigned int *postdiv1,
			unsigned int *postdiv2, unsigned long *frac, int *diva,
			int *divb, int *divc, int *divd, int *isint)
{
	void __iomem *b = base;
	u32 v = clk_readl(b + PLL_CTRL_OFF);
	int a_isenable = v >> BS_PLL_CTRL_PLL_DIVA_CG_EN & 1;
	int b_isenable = v >> BS_PLL_CTRL_PLL_DIVB_CG_EN & 1;
	int c_isenable = v >> BS_PLL_CTRL_PLL_DIVC_CG_EN & 1;
	int d_isenable = v >> BS_PLL_CTRL_PLL_DIVD_CG_EN & 1;

	*isint = ((v & BM_PLL_CTRL_INT_MODE) >> BS_PLL_CTRL_INT_MODE);

	v = clk_readl(b + PLL_FBDIV_OFF);
	*fbdiv = GFV_PLL_FBDIV_FBDIV(v);

	v = clk_readl(b + PLL_DIV_OFF);
	*refdiv = GFV_PLL_DIV_REFDIV(v);
	*postdiv1 = GFV_PLL_DIV_POSTDIV1(v);
	*postdiv2 = GFV_PLL_DIV_POSTDIV2(v);

	v = clk_readl(b + PLL_FRAC_OFF);
	*frac = GFV_PLL_FRAC_FRAC(v);

	v = clk_readl(b + PLL_OUT_DIV_1_OFF);
	if (a_isenable)
		*diva = GFV_PLL_OUT_DIV_1_DIV_NUM_A(v);
	else
		*diva = -1;
	if (b_isenable)
		*divb = GFV_PLL_OUT_DIV_1_DIV_NUM_B(v);
	else
		*divb = -1;

	v = clk_readl(b + PLL_OUT_DIV_2_OFF);
	if (c_isenable)
		*divc = GFV_PLL_OUT_DIV_2_DIV_NUM_C(v);
	else
		*divc = -1;
	if (d_isenable)
		*divd = GFV_PLL_OUT_DIV_2_DIV_NUM_D(v);
	else
		*divd = -1;
}

void sc_pfpll_set_ss(void __iomem *base, bool enable, bool down,
		     uint32_t spread)
{
	uint32_t v = clk_readl(base + PLL_SSMOD);

	/* Enable SS modulator if necessary. */
	/* Modulators may be required for display modules to reduce EMI. */
	if (enable) {
		uint32_t ctrl = clk_readl(base + PLL_CTRL_OFF);
		ctrl &= ~BM_PLL_CTRL_INT_MODE;
		clk_writel(ctrl, base + PLL_CTRL_OFF);

		v &= ~(1 << PLL_SSMOD_DISABLE_SSCG);
		v &= ~(1 << PLL_SSMOD_RESETPTR);
		if (down)
			v |= (1 << PLL_SSMOD_DOWNSPREAD);
		else
			v &= ~(1 << PLL_SSMOD_DOWNSPREAD);

		v &= ~(0x1f << PLL_SSMOD_SPREAD);
		v |= ((spread & 0x1f) << PLL_SSMOD_SPREAD);
		v &= ~(0x1f << PLL_SSMOD_DIVVAL);
		v |= ((15 & 0x1f) << PLL_SSMOD_DIVVAL);
		v &= ~(1 << PLL_SSMOD_MOD_RESET);
	} else {
		v |= (1 << PLL_SSMOD_DISABLE_SSCG);
		v |= (1 << PLL_SSMOD_RESETPTR);
		v |= (1 << PLL_SSMOD_MOD_RESET);
	}
	clk_writel(v, base + PLL_SSMOD);
}
void sc_pfpll_get_ss(void __iomem *base, bool *isint, bool *enable, bool *down,
		     uint32_t *spread)
{
	uint32_t v = clk_readl(base + PLL_CTRL_OFF);

	*isint = ((v & BM_PLL_CTRL_INT_MODE) >> BS_PLL_CTRL_INT_MODE);
	v = clk_readl(base + PLL_SSMOD);
	if (v & (1 << PLL_SSMOD_DISABLE_SSCG))
		*enable = false;
	else
		*enable = true;
	if (v & (1 << PLL_SSMOD_DOWNSPREAD))
		*down = true;
	else
		*down = false;
	*spread = ((v >> PLL_SSMOD_SPREAD) & 0x1f);
	return;
}
