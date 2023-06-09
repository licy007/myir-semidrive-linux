/*
 * sc_pfpll_reg.h
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

/* Generated by tool. Do not modify manually. */

#ifndef __SC_PFPLL_REG_H__
#define __SC_PFPLL_REG_H__

#define PLL_CTRL_OFF	0x0U

#define BS_PLL_CTRL_LOCK	31U
#define BM_PLL_CTRL_LOCK	(0x01U << BS_PLL_CTRL_LOCK)

#define BS_PLL_CTRL_PLL_DIVD_CG_EN	21U
#define BM_PLL_CTRL_PLL_DIVD_CG_EN	(0x01U << BS_PLL_CTRL_PLL_DIVD_CG_EN)

#define BS_PLL_CTRL_PLL_DIVC_CG_EN	20U
#define BM_PLL_CTRL_PLL_DIVC_CG_EN	(0x01U << BS_PLL_CTRL_PLL_DIVC_CG_EN)

#define BS_PLL_CTRL_PLL_DIVB_CG_EN	19U
#define BM_PLL_CTRL_PLL_DIVB_CG_EN	(0x01U << BS_PLL_CTRL_PLL_DIVB_CG_EN)

#define BS_PLL_CTRL_PLL_DIVA_CG_EN	18U
#define BM_PLL_CTRL_PLL_DIVA_CG_EN	(0x01U << BS_PLL_CTRL_PLL_DIVA_CG_EN)

#define BS_PLL_CTRL_BYPASS	17U
#define BM_PLL_CTRL_BYPASS	(0x01U << BS_PLL_CTRL_BYPASS)

#define BS_PLL_CTRL_PLLPOSTCG_EN	16U
#define BM_PLL_CTRL_PLLPOSTCG_EN	(0x01U << BS_PLL_CTRL_PLLPOSTCG_EN)

#define BS_PLL_CTRL_FOUTPOSTDIVEN	4U
#define BM_PLL_CTRL_FOUTPOSTDIVEN	(0x01U << BS_PLL_CTRL_FOUTPOSTDIVEN)

#define BS_PLL_CTRL_FOUT4PHASEEN	3U
#define BM_PLL_CTRL_FOUT4PHASEEN	(0x01U << BS_PLL_CTRL_FOUT4PHASEEN)

#define BS_PLL_CTRL_DACEN	2U
#define BM_PLL_CTRL_DACEN	(0x01U << BS_PLL_CTRL_DACEN)

#define BS_PLL_CTRL_INT_MODE	1U
#define BM_PLL_CTRL_INT_MODE	(0x01U << BS_PLL_CTRL_INT_MODE)

#define BS_PLL_CTRL_PLLEN	0U
#define BM_PLL_CTRL_PLLEN	(0x01U << BS_PLL_CTRL_PLLEN)

#define PLL_DIV_OFF	0x4U

#define FS_PLL_DIV_POSTDIV2	9U
#define FM_PLL_DIV_POSTDIV2	(0x7U << FS_PLL_DIV_POSTDIV2)
#define FV_PLL_DIV_POSTDIV2(v) \
	(((v) << FS_PLL_DIV_POSTDIV2) & FM_PLL_DIV_POSTDIV2)
#define GFV_PLL_DIV_POSTDIV2(v) \
	(((v) & FM_PLL_DIV_POSTDIV2) >> FS_PLL_DIV_POSTDIV2)

#define FS_PLL_DIV_POSTDIV1	6U
#define FM_PLL_DIV_POSTDIV1	(0x7U << FS_PLL_DIV_POSTDIV1)
#define FV_PLL_DIV_POSTDIV1(v) \
	(((v) << FS_PLL_DIV_POSTDIV1) & FM_PLL_DIV_POSTDIV1)
#define GFV_PLL_DIV_POSTDIV1(v) \
	(((v) & FM_PLL_DIV_POSTDIV1) >> FS_PLL_DIV_POSTDIV1)

#define FS_PLL_DIV_REFDIV	0U
#define FM_PLL_DIV_REFDIV	(0x3fU << FS_PLL_DIV_REFDIV)
#define FV_PLL_DIV_REFDIV(v) \
	(((v) << FS_PLL_DIV_REFDIV) & FM_PLL_DIV_REFDIV)
#define GFV_PLL_DIV_REFDIV(v) \
	(((v) & FM_PLL_DIV_REFDIV) >> FS_PLL_DIV_REFDIV)

#define PLL_FBDIV_OFF	0x8U

#define FS_PLL_FBDIV_FBDIV	0U
#define FM_PLL_FBDIV_FBDIV	(0xfffU << FS_PLL_FBDIV_FBDIV)
#define FV_PLL_FBDIV_FBDIV(v) \
	(((v) << FS_PLL_FBDIV_FBDIV) & FM_PLL_FBDIV_FBDIV)
#define GFV_PLL_FBDIV_FBDIV(v) \
	(((v) & FM_PLL_FBDIV_FBDIV) >> FS_PLL_FBDIV_FBDIV)

#define PLL_FRAC_OFF	0xcU

#define FS_PLL_FRAC_FRAC	0U
#define FM_PLL_FRAC_FRAC	(0xffffffU << FS_PLL_FRAC_FRAC)
#define FV_PLL_FRAC_FRAC(v) \
	(((v) << FS_PLL_FRAC_FRAC) & FM_PLL_FRAC_FRAC)
#define GFV_PLL_FRAC_FRAC(v) \
	(((v) & FM_PLL_FRAC_FRAC) >> FS_PLL_FRAC_FRAC)

#define PLL_DESKEW_OFF	0x10U

#define FS_PLL_DESKEW_DSKEWCALIN	6U
#define FM_PLL_DESKEW_DSKEWCALIN	(0xfffU << FS_PLL_DESKEW_DSKEWCALIN)
#define FV_PLL_DESKEW_DSKEWCALIN(v) \
	(((v) << FS_PLL_DESKEW_DSKEWCALIN) & FM_PLL_DESKEW_DSKEWCALIN)
#define GFV_PLL_DESKEW_DSKEWCALIN(v) \
	(((v) & FM_PLL_DESKEW_DSKEWCALIN) >> FS_PLL_DESKEW_DSKEWCALIN)

#define FS_PLL_DESKEW_DSKEWCALCNT	3U
#define FM_PLL_DESKEW_DSKEWCALCNT	(0x7U << FS_PLL_DESKEW_DSKEWCALCNT)
#define FV_PLL_DESKEW_DSKEWCALCNT(v) \
	(((v) << FS_PLL_DESKEW_DSKEWCALCNT) & FM_PLL_DESKEW_DSKEWCALCNT)
#define GFV_PLL_DESKEW_DSKEWCALCNT(v) \
	(((v) & FM_PLL_DESKEW_DSKEWCALCNT) >> FS_PLL_DESKEW_DSKEWCALCNT)

#define BS_PLL_DESKEW_DSKEWCALBYP	2U
#define BM_PLL_DESKEW_DSKEWCALBYP	(0x01U << BS_PLL_DESKEW_DSKEWCALBYP)

#define BS_PLL_DESKEW_DSKEWFASTCAL	1U
#define BM_PLL_DESKEW_DSKEWFASTCAL	(0x01U << BS_PLL_DESKEW_DSKEWFASTCAL)

#define BS_PLL_DESKEW_DSKEWCALEN	0U
#define BM_PLL_DESKEW_DSKEWCALEN	(0x01U << BS_PLL_DESKEW_DSKEWCALEN)

#define PLL_DESKEW_STA_OFF	0x14U

#define FS_PLL_DESKEW_STA_DSKEWCALOUT	5U
#define FM_PLL_DESKEW_STA_DSKEWCALOUT	(0xfffU << FS_PLL_DESKEW_STA_DSKEWCALOUT)
#define FV_PLL_DESKEW_STA_DSKEWCALOUT(v) \
	(((v) << FS_PLL_DESKEW_STA_DSKEWCALOUT) & FM_PLL_DESKEW_STA_DSKEWCALOUT)
#define GFV_PLL_DESKEW_STA_DSKEWCALOUT(v) \
	(((v) & FM_PLL_DESKEW_STA_DSKEWCALOUT) >> FS_PLL_DESKEW_STA_DSKEWCALOUT)

#define FS_PLL_DESKEW_STA_DSKEWCALLOCKCNT	1U
#define FM_PLL_DESKEW_STA_DSKEWCALLOCKCNT	(0xfU << FS_PLL_DESKEW_STA_DSKEWCALLOCKCNT)
#define FV_PLL_DESKEW_STA_DSKEWCALLOCKCNT(v) \
	(((v) << FS_PLL_DESKEW_STA_DSKEWCALLOCKCNT) & FM_PLL_DESKEW_STA_DSKEWCALLOCKCNT)
#define GFV_PLL_DESKEW_STA_DSKEWCALLOCKCNT(v) \
	(((v) & FM_PLL_DESKEW_STA_DSKEWCALLOCKCNT) >> FS_PLL_DESKEW_STA_DSKEWCALLOCKCNT)

#define BS_PLL_DESKEW_STA_DSKEWCALLOCK	0U
#define BM_PLL_DESKEW_STA_DSKEWCALLOCK	(0x01U << BS_PLL_DESKEW_STA_DSKEWCALLOCK)

#define PLL_OUT_DIV_1_OFF	0x20U

#define BS_PLL_OUT_DIV_1_DIV_BUSY_B	31U
#define BM_PLL_OUT_DIV_1_DIV_BUSY_B	(0x01U << BS_PLL_OUT_DIV_1_DIV_BUSY_B)

#define FS_PLL_OUT_DIV_1_DIV_NUM_B	16U
#define FM_PLL_OUT_DIV_1_DIV_NUM_B	(0x7fffU << FS_PLL_OUT_DIV_1_DIV_NUM_B)
#define FV_PLL_OUT_DIV_1_DIV_NUM_B(v) \
	(((v) << FS_PLL_OUT_DIV_1_DIV_NUM_B) & FM_PLL_OUT_DIV_1_DIV_NUM_B)
#define GFV_PLL_OUT_DIV_1_DIV_NUM_B(v) \
	(((v) & FM_PLL_OUT_DIV_1_DIV_NUM_B) >> FS_PLL_OUT_DIV_1_DIV_NUM_B)

#define BS_PLL_OUT_DIV_1_DIV_BUSY_A	15U
#define BM_PLL_OUT_DIV_1_DIV_BUSY_A	(0x01U << BS_PLL_OUT_DIV_1_DIV_BUSY_A)

#define FS_PLL_OUT_DIV_1_DIV_NUM_A	0U
#define FM_PLL_OUT_DIV_1_DIV_NUM_A	(0x7fffU << FS_PLL_OUT_DIV_1_DIV_NUM_A)
#define FV_PLL_OUT_DIV_1_DIV_NUM_A(v) \
	(((v) << FS_PLL_OUT_DIV_1_DIV_NUM_A) & FM_PLL_OUT_DIV_1_DIV_NUM_A)
#define GFV_PLL_OUT_DIV_1_DIV_NUM_A(v) \
	(((v) & FM_PLL_OUT_DIV_1_DIV_NUM_A) >> FS_PLL_OUT_DIV_1_DIV_NUM_A)

#define PLL_OUT_DIV_2_OFF	0x24U

#define BS_PLL_OUT_DIV_2_DIV_BUSY_D	31U
#define BM_PLL_OUT_DIV_2_DIV_BUSY_D	(0x01U << BS_PLL_OUT_DIV_2_DIV_BUSY_D)

#define FS_PLL_OUT_DIV_2_DIV_NUM_D	16U
#define FM_PLL_OUT_DIV_2_DIV_NUM_D	(0x7fffU << FS_PLL_OUT_DIV_2_DIV_NUM_D)
#define FV_PLL_OUT_DIV_2_DIV_NUM_D(v) \
	(((v) << FS_PLL_OUT_DIV_2_DIV_NUM_D) & FM_PLL_OUT_DIV_2_DIV_NUM_D)
#define GFV_PLL_OUT_DIV_2_DIV_NUM_D(v) \
	(((v) & FM_PLL_OUT_DIV_2_DIV_NUM_D) >> FS_PLL_OUT_DIV_2_DIV_NUM_D)

#define BS_PLL_OUT_DIV_2_DIV_BUSY_C	15U
#define BM_PLL_OUT_DIV_2_DIV_BUSY_C	(0x01U << BS_PLL_OUT_DIV_2_DIV_BUSY_C)

#define FS_PLL_OUT_DIV_2_DIV_NUM_C	0U
#define FM_PLL_OUT_DIV_2_DIV_NUM_C	(0x7fffU << FS_PLL_OUT_DIV_2_DIV_NUM_C)
#define FV_PLL_OUT_DIV_2_DIV_NUM_C(v) \
	(((v) << FS_PLL_OUT_DIV_2_DIV_NUM_C) & FM_PLL_OUT_DIV_2_DIV_NUM_C)
#define GFV_PLL_OUT_DIV_2_DIV_NUM_C(v) \
	(((v) & FM_PLL_OUT_DIV_2_DIV_NUM_C) >> FS_PLL_OUT_DIV_2_DIV_NUM_C)

/* Spread spectrum mode setting register */
#define PLL_SSMOD                       0x18
#  define PLL_SSMOD_MOD_RESET           0
#  define PLL_SSMOD_DISABLE_SSCG    1
#  define PLL_SSMOD_RESETPTR        2
#  define PLL_SSMOD_DOWNSPREAD      3
#  define PLL_SSMOD_SPREAD      4   /*4 ~ 8*/
#  define PLL_SSMOD_DIVVAL      9   /*9 ~ 14*/

#endif
