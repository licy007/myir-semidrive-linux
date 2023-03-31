
/*
 * sdrv-i2s-sc.h
 * Copyright (C) 2019 semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef SDRV_I2S_SC_H__
#define SDRV_I2S_SC_H__
#include <linux/bitops.h>
#define BIT_(nr) (1UL << (nr))

/*  For FPGA I2S clock is set to 75000000 */
#define FPGA_I2S_CLOCK (75000000)

enum { CDN_CHN_WIDTH_8BITS = 0,
       CDN_CHN_WIDTH_12BITS,
       CDN_CHN_WIDTH_16BITS,
       CDN_CHN_WIDTH_18BITS,
       CDN_CHN_WIDTH_20BITS,
       CDN_CHN_WIDTH_24BITS,
       CDN_CHN_WIDTH_28BITS,
       CDN_CHN_WIDTH_32BITS,
       CDN_CHN_WIDTH_NUMB,
};

#define AFE_I2S_CHN_WIDTH CDN_CHN_WIDTH_32BITS
#define APB_I2S_SC8_BASE (0x30650000u)
#define APB_I2S_SC7_BASE (0x30640000u)
#define APB_I2S_SC6_BASE (0x30630000u)
#define APB_I2S_SC5_BASE (0x30620000u)
#define APB_I2S_SC4_BASE (0x30610000u)
#define APB_I2S_SC3_BASE (0x30600000u)

const static uint8_t ChnWidthTable[] = {8, 12, 16, 18, 20, 24, 28, 32};
#define I2S_SC_SAMPLE_RATE_CALC(fclk, fsample, chn_num, chn_width)             \
	DIV_ROUND_CLOSEST(fclk , (fsample * chn_num * chn_width))

/* Register offsets from SDRV_I2S*_BASE */
#define I2S_SC_FIFO_OFFSET 0x040
#define I2S_SC_TX_DEPTH 128
#define I2S_SC_RX_DEPTH 128

#define I2S_SC_FIFO_DEPTH_PARA 7
//--------------------------------------------------------------------------
// IP Ref Info     : REG_CDN_I2SSC_REGS
// RTL version     :
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// Address Block Name : BITSC_REGS_BITSC_ADDR_MAP_MEMORY_MAP_BITSC_ADDR_MAP
// Description        :
//--------------------------------------------------------------------------
#define I2S_SC_BASE 0x0
//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_I2S_CTRL
// Register Offset : 0x0
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_I2S_CTRL (I2S_SC_BASE + 0x0)
#define I2S_CTRL_LR_PACK_FIELD_OFFSET 31
#define I2S_CTRL_LR_PACK_FIELD_SIZE 1
#define I2S_CTRL_FIFO_AFULL_MASK_FIELD_OFFSET 30
#define I2S_CTRL_FIFO_AFULL_MASK_FIELD_SIZE 1
#define I2S_CTRL_FIFO_FULL_MASK_FIELD_OFFSET 29
#define I2S_CTRL_FIFO_FULL_MASK_FIELD_SIZE 1
#define I2S_CTRL_FIFO_AEMPTY_MASK_FIELD_OFFSET 28
#define I2S_CTRL_FIFO_AEMPTY_MASK_FIELD_SIZE 1
#define I2S_CTRL_FIFO_EMPTY_MASK_FIELD_OFFSET 27
#define I2S_CTRL_FIFO_EMPTY_MASK_FIELD_SIZE 1
#define I2S_CTRL_I2S_MASK_FIELD_OFFSET 26
#define I2S_CTRL_I2S_MASK_FIELD_SIZE 1
#define I2S_CTRL_INTREQ_MASK_FIELD_OFFSET 25
#define I2S_CTRL_INTREQ_MASK_FIELD_SIZE 1
#define I2S_CTRL_I2S_STB_FIELD_OFFSET 24
#define I2S_CTRL_I2S_STB_FIELD_SIZE 1
#define I2S_CTRL_HOST_DATA_ALIGN_FIELD_OFFSET 23
#define I2S_CTRL_HOST_DATA_ALIGN_FIELD_SIZE 1
#define I2S_CTRL_DATA_ORDER_FIELD_OFFSET 22
#define I2S_CTRL_DATA_ORDER_FIELD_SIZE 1
#define I2S_CTRL_DATA_ALIGN_FIELD_OFFSET 21
#define I2S_CTRL_DATA_ALIGN_FIELD_SIZE 1
#define I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET 16
#define I2S_CTRL_DATA_WS_DEL_FIELD_SIZE 5
#define I2S_CTRL_WS_POLAR_FIELD_OFFSET 15
#define I2S_CTRL_WS_POLAR_FIELD_SIZE 1
#define I2S_CTRL_SCK_POLAR_FIELD_OFFSET 14
#define I2S_CTRL_SCK_POLAR_FIELD_SIZE 1
#define I2S_CTRL_AUDIO_MODE_FIELD_OFFSET 13
#define I2S_CTRL_AUDIO_MODE_FIELD_SIZE 1
#define I2S_CTRL_MONO_MODE_FIELD_OFFSET 12
#define I2S_CTRL_MONO_MODE_FIELD_SIZE 1
#define I2S_CTRL_WS_MODE_FIELD_OFFSET 8
#define I2S_CTRL_WS_MODE_FIELD_SIZE 4
#define I2S_CTRL_CHN_WIDTH_FIELD_OFFSET 5
#define I2S_CTRL_CHN_WIDTH_FIELD_SIZE 3
#define I2S_CTRL_FIFO_RST_FIELD_OFFSET 4
#define I2S_CTRL_FIFO_RST_FIELD_SIZE 1
#define I2S_CTRL_SFR_RST_FIELD_OFFSET 3
#define I2S_CTRL_SFR_RST_FIELD_SIZE 1
#define I2S_CTRL_MS_CFG_FIELD_OFFSET 2
#define I2S_CTRL_MS_CFG_FIELD_SIZE 1
#define I2S_CTRL_DIR_CFG_FIELD_OFFSET 1
#define I2S_CTRL_DIR_CFG_FIELD_SIZE 1
#define I2S_CTRL_I2S_EN_FIELD_OFFSET 0
#define I2S_CTRL_I2S_EN_FIELD_SIZE 1

#define BIT_CTRL_LR_PACK (BIT_(31))
#define BIT_CTRL_FIFO_AFULL_MASK (BIT_(30))
#define BIT_CTRL_FIFO_FULL_MASK (BIT_(29))
#define BIT_CTRL_FIFO_AEMPTY_MASK (BIT_(28))
#define BIT_CTRL_FIFO_EMPTY_MASK (BIT_(27))
#define BIT_CTRL_I2S_MASK (BIT_(26))
#define BIT_CTRL_INTREQ_MASK (BIT_(25))
#define BIT_CTRL_I2S_STB (BIT_(24))
#define BIT_CTRL_HOST_DATA_ALIGN (BIT_(23))
#define BIT_CTRL_DATA_ORDER (BIT_(22))
#define BIT_CTRL_DATA_ALIGN (BIT_(21))
#define BIT_CTRL_DATA_WS_DEL_4 (BIT_(20))
#define BIT_CTRL_DATA_WS_DEL_3 (BIT_(19))
#define BIT_CTRL_DATA_WS_DEL_2 (BIT_(18))
#define BIT_CTRL_DATA_WS_DEL_1 (BIT_(17))
#define BIT_CTRL_DATA_WS_DEL_0 (BIT_(16))
#define BIT_CTRL_DATA_WS_DEL                                                   \
	(BIT_(16) | BIT_(17) | BIT_(18) | BIT_(19) | BIT_(20))
#define BIT_CTRL_WS_POLAR (BIT_(15))
#define BIT_CTRL_SCK_POLAR (BIT_(14))
#define BIT_CTRL_AUDIO_MODE (BIT_(13))
#define BIT_CTRL_MONO_MODE (BIT_(12))
#define BIT_CTRL_WS_MODE_3 (BIT_(11))
#define BIT_CTRL_WS_MODE_2 (BIT_(10))
#define BIT_CTRL_WS_MODE_1 (BIT_(9))
#define BIT_CTRL_WS_MODE_0 (BIT_(8))
#define BIT_CTRL_WS_MODE (BIT_(8) | BIT_(9) | BIT_(10) | BIT_(11))
#define BIT_CTRL_CHN_WIDTH_2 (BIT_(7))
#define BIT_CTRL_CHN_WIDTH_1 (BIT_(6))
#define BIT_CTRL_CHN_WIDTH_0 (BIT_(5))
#define BIT_CTRL_CHN_WIDTH (BIT_(5) | BIT_(6) | BIT_(7))
#define BIT_CTRL_FIFO_RST (BIT_(4))
#define BIT_CTRL_SFR_RST (BIT_(3))
#define BIT_CTRL_MS_CFG (BIT_(2))
#define BIT_CTRL_DIR_CFG (BIT_(1))
#define BIT_CTRL_I2S_EN (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_I2S_CTRL_FDX
// Register Offset : 0x4
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_I2S_CTRL_FDX (I2S_SC_BASE + 0x4)
#define I2S_CTRL_FDX_RFIFO_AFULL_MASK_FIELD_OFFSET 30
#define I2S_CTRL_FDX_RFIFO_AFULL_MASK_FIELD_SIZE 1
#define I2S_CTRL_FDX_RFIFO_FULL_MASK_FIELD_OFFSET 29
#define I2S_CTRL_FDX_RFIFO_FULL_MASK_FIELD_SIZE 1
#define I2S_CTRL_FDX_RFIFO_AEMPTY_MASK_FIELD_OFFSET 28
#define I2S_CTRL_FDX_RFIFO_AEMPTY_MASK_FIELD_SIZE 1
#define I2S_CTRL_FDX_RFIFO_EMPTY_MASK_FIELD_OFFSET 27
#define I2S_CTRL_FDX_RFIFO_EMPTY_MASK_FIELD_SIZE 1
#define I2S_CTRL_FDX_RI2S_MASK_FIELD_OFFSET 26
#define I2S_CTRL_FDX_RI2S_MASK_FIELD_SIZE 1
#define I2S_CTRL_FDX_FIFO_RST_FIELD_OFFSET 4
#define I2S_CTRL_FDX_FIFO_RST_FIELD_SIZE 1
#define I2S_CTRL_FDX_I2S_FRX_EN_FIELD_OFFSET 2
#define I2S_CTRL_FDX_I2S_FRX_EN_FIELD_SIZE 1
#define I2S_CTRL_FDX_I2S_FTX_EN_FIELD_OFFSET 1
#define I2S_CTRL_FDX_I2S_FTX_EN_FIELD_SIZE 1
#define I2S_CTRL_FDX_FULL_DUPLEX_FIELD_OFFSET 0
#define I2S_CTRL_FDX_FULL_DUPLEX_FIELD_SIZE 1

#define BIT_CTRL_FDX_RFIFO_AFULL_MASK (BIT_(30))
#define BIT_CTRL_FDX_RFIFO_FULL_MASK (BIT_(29))
#define BIT_CTRL_FDX_RFIFO_AEMPTY_MASK (BIT_(28))
#define BIT_CTRL_FDX_RFIFO_EMPTY_MASK (BIT_(27))
#define BIT_CTRL_FDX_RI2S_MASK (BIT_(26))
#define BIT_CTRL_FDX_FIFO_RST (BIT_(4))
#define BIT_CTRL_FDX_I2S_FRX_EN (BIT_(2))
#define BIT_CTRL_FDX_I2S_FTX_EN (BIT_(1))
#define BIT_CTRL_FDX_FULL_DUPLEX (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_I2S_SRES
// Register Offset : 0x8
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_I2S_SRES (I2S_SC_BASE + 0x8)
#define I2S_SRES_RESOLUTION_FIELD_OFFSET 0
#define I2S_SRES_RESOLUTION_FIELD_SIZE 5

#define BIT_SRES_RESOLUTION_4 (BIT_(4))
#define BIT_SRES_RESOLUTION_3 (BIT_(3))
#define BIT_SRES_RESOLUTION_2 (BIT_(2))
#define BIT_SRES_RESOLUTION_1 (BIT_(1))
#define BIT_SRES_RESOLUTION_0 (BIT_(0))
#define BIT_SRES_RESOLUTION (BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3) | BIT_(4))
//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_I2S_SRES_FDR
// Register Offset : 0xc
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_I2S_SRES_FDR (I2S_SC_BASE + 0xc)
#define I2S_SRES_FDR_RRESOLUTION_FIELD_OFFSET 0
#define I2S_SRES_FDR_RRESOLUTION_FIELD_SIZE 5

#define BIT_SRES_FDR_RRESOLUTION_4 (BIT_(4))
#define BIT_SRES_FDR_RRESOLUTION_3 (BIT_(3))
#define BIT_SRES_FDR_RRESOLUTION_2 (BIT_(2))
#define BIT_SRES_FDR_RRESOLUTION_1 (BIT_(1))
#define BIT_SRES_FDR_RRESOLUTION_0 (BIT_(0))
#define BIT_SRES_FDR_RRESOLUTION                                               \
	(BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3) | BIT_(4))
//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_I2S_SRATE
// Register Offset : 0x10
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_I2S_SRATE (I2S_SC_BASE + 0x10)
#define I2S_SRATE_SAMPLE_RATE_FIELD_OFFSET 0
#define I2S_SRATE_SAMPLE_RATE_FIELD_SIZE 20

#define BIT_SRATE_SAMPLE_RATE_19 (BIT_(19))
#define BIT_SRATE_SAMPLE_RATE_18 (BIT_(18))
#define BIT_SRATE_SAMPLE_RATE_17 (BIT_(17))
#define BIT_SRATE_SAMPLE_RATE_16 (BIT_(16))
#define BIT_SRATE_SAMPLE_RATE_15 (BIT_(15))
#define BIT_SRATE_SAMPLE_RATE_14 (BIT_(14))
#define BIT_SRATE_SAMPLE_RATE_13 (BIT_(13))
#define BIT_SRATE_SAMPLE_RATE_12 (BIT_(12))
#define BIT_SRATE_SAMPLE_RATE_11 (BIT_(11))
#define BIT_SRATE_SAMPLE_RATE_10 (BIT_(10))
#define BIT_SRATE_SAMPLE_RATE_9 (BIT_(9))
#define BIT_SRATE_SAMPLE_RATE_8 (BIT_(8))
#define BIT_SRATE_SAMPLE_RATE_7 (BIT_(7))
#define BIT_SRATE_SAMPLE_RATE_6 (BIT_(6))
#define BIT_SRATE_SAMPLE_RATE_5 (BIT_(5))
#define BIT_SRATE_SAMPLE_RATE_4 (BIT_(4))
#define BIT_SRATE_SAMPLE_RATE_3 (BIT_(3))
#define BIT_SRATE_SAMPLE_RATE_2 (BIT_(2))
#define BIT_SRATE_SAMPLE_RATE_1 (BIT_(1))
#define BIT_SRATE_SAMPLE_RATE_0 (BIT_(0))
#define BIT_SRATE_SAMPLE_RATE (GENMASK(19, 0))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_I2S_STAT
// Register Offset : 0x14
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_I2S_STAT (I2S_SC_BASE + 0x14)
#define I2S_STAT_RFIFO_AFULL_FIELD_OFFSET 19
#define I2S_STAT_RFIFO_AFULL_FIELD_SIZE 1
#define I2S_STAT_RFIFO_FULL_FIELD_OFFSET 18
#define I2S_STAT_RFIFO_FULL_FIELD_SIZE 1
#define I2S_STAT_RFIFO_AEMPTY_FIELD_OFFSET 17
#define I2S_STAT_RFIFO_AEMPTY_FIELD_SIZE 1
#define I2S_STAT_RFIFO_EMPTY_FIELD_OFFSET 16
#define I2S_STAT_RFIFO_EMPTY_FIELD_SIZE 1
#define I2S_STAT_FIFO_AFULL_FIELD_OFFSET 5
#define I2S_STAT_FIFO_AFULL_FIELD_SIZE 1
#define I2S_STAT_FIFO_FULL_FIELD_OFFSET 4
#define I2S_STAT_FIFO_FULL_FIELD_SIZE 1
#define I2S_STAT_FIFO_AEMPTY_FIELD_OFFSET 3
#define I2S_STAT_FIFO_AEMPTY_FIELD_SIZE 1
#define I2S_STAT_FIFO_EMPTY_FIELD_OFFSET 2
#define I2S_STAT_FIFO_EMPTY_FIELD_SIZE 1
#define I2S_STAT_RDATA_OVERR_FIELD_OFFSET 1
#define I2S_STAT_RDATA_OVERR_FIELD_SIZE 1
#define I2S_STAT_TDATA_UNDERR_FIELD_OFFSET 0
#define I2S_STAT_TDATA_UNDERR_FIELD_SIZE 1

#define BIT_STAT_RFIFO_AFULL (BIT_(19))
#define BIT_STAT_RFIFO_FULL (BIT_(18))
#define BIT_STAT_RFIFO_AEMPTY (BIT_(17))
#define BIT_STAT_RFIFO_EMPTY (BIT_(16))
#define BIT_STAT_FIFO_AFULL (BIT_(5))
#define BIT_STAT_FIFO_FULL (BIT_(4))
#define BIT_STAT_FIFO_AEMPTY (BIT_(3))
#define BIT_STAT_FIFO_EMPTY (BIT_(2))
#define BIT_STAT_RDATA_OVERR (BIT_(1))
#define BIT_STAT_TDATA_UNDERR (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_FIFO_LEVEL
// Register Offset : 0x18
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_FIFO_LEVEL (I2S_SC_BASE + 0x18)
#define FIFO_LEVEL_FIFO_LEVEL_FIELD_OFFSET 0
#define FIFO_LEVEL_FIFO_LEVEL_FIELD_SIZE (I2S_SC_FIFO_DEPTH_PARA + 1)

#define CDN_BIT_FIFO_LEVEL_FIFO_LEVEL_3 (BIT_(3))
#define _FIFO_LEVEL_FIFO_LEVEL_2 (BIT_(2))
#define _FIFO_LEVEL_FIFO_LEVEL_1 (BIT_(1))
#define _FIFO_LEVEL_FIFO_LEVEL_0 (BIT_(0))
#define _FIFO_LEVEL_FIFO_LEVEL (BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_FIFO_AEMPTY
// Register Offset : 0x1c
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_FIFO_AEMPTY (I2S_SC_BASE + 0x1c)
#define FIFO_AEMPTY_AEMPTY_THRESHOLD_FIELD_OFFSET 0
#define FIFO_AEMPTY_AEMPTY_THRESHOLD_FIELD_SIZE (I2S_SC_FIFO_DEPTH_PARA)

#define _FIFO_AEMPTY_AEMPTY_THRESHOLD_3 (BIT_(3))
#define _FIFO_AEMPTY_AEMPTY_THRESHOLD_2 (BIT_(2))
#define _FIFO_AEMPTY_AEMPTY_THRESHOLD_1 (BIT_(1))
#define _FIFO_AEMPTY_AEMPTY_THRESHOLD_0 (BIT_(0))
#define _FIFO_AEMPTY_AEMPTY_THRESHOLD (BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3))
//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_FIFO_AFULL
// Register Offset : 0x20
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_FIFO_AFULL (I2S_SC_BASE + 0x20)
#define FIFO_AFULL_AFULL_THRESHOLD_FIELD_OFFSET 0
#define FIFO_AFULL_AFULL_THRESHOLD_FIELD_SIZE (I2S_SC_FIFO_DEPTH_PARA)

#define _FIFO_AFULL_AFULL_THRESHOLD_3 (BIT_(3))
#define _FIFO_AFULL_AFULL_THRESHOLD_2 (BIT_(2))
#define _FIFO_AFULL_AFULL_THRESHOLD_1 (BIT_(1))
#define _FIFO_AFULL_AFULL_THRESHOLD_0 (BIT_(0))
#define _FIFO_AFULL_AFULL_THRESHOLD (BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR
// Register Offset : 0x24
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR (I2S_SC_BASE + 0x24)
#define FIFO_LEVEL_FDR_FIFO_LEVEL_FIELD_OFFSET 0
#define FIFO_LEVEL_FDR_FIFO_LEVEL_FIELD_SIZE (I2S_SC_FIFO_DEPTH_PARA + 1)

#define _FIFO_LEVEL_FDR_FIFO_LEVEL_3 (BIT_(3))
#define _FIFO_LEVEL_FDR_FIFO_LEVEL_2 (BIT_(2))
#define _FIFO_LEVEL_FDR_FIFO_LEVEL_1 (BIT_(1))
#define _FIFO_LEVEL_FDR_FIFO_LEVEL_0 (BIT_(0))
#define _FIFO_LEVEL_FDR_FIFO_LEVEL (BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_FIFO_AEMPTY_FDR
// Register Offset : 0x28
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_FIFO_AEMPTY_FDR (I2S_SC_BASE + 0x28)
#define FIFO_AEMPTY_FDR_AEMPTY_THRESHOLD_FIELD_OFFSET 0
#define FIFO_AEMPTY_FDR_AEMPTY_THRESHOLD_FIELD_SIZE (I2S_SC_FIFO_DEPTH_PARA)

#define _FIFO_AEMPTY_FDR_AEMPTY_THRESHOLD_3 (BIT_(3))
#define _FIFO_AEMPTY_FDR_AEMPTY_THRESHOLD_2 (BIT_(2))
#define _FIFO_AEMPTY_FDR_AEMPTY_THRESHOLD_1 (BIT_(1))
#define _FIFO_AEMPTY_FDR_AEMPTY_THRESHOLD_0 (BIT_(0))
#define _FIFO_AEMPTY_FDR_AEMPTY_THRESHOLD                                      \
	(BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_FIFO_AFULL_FDR
// Register Offset : 0x2c
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_FIFO_AFULL_FDR (I2S_SC_BASE + 0x2c)
#define FIFO_AFULL_FDR_AFULL_THRESHOLD_FIELD_OFFSET 0
#define FIFO_AFULL_FDR_AFULL_THRESHOLD_FIELD_SIZE (I2S_SC_FIFO_DEPTH_PARA)

#define _FIFO_AFULL_FDR_AFULL_THRESHOLD_3 (BIT_(3))
#define _FIFO_AFULL_FDR_AFULL_THRESHOLD_2 (BIT_(2))
#define _FIFO_AFULL_FDR_AFULL_THRESHOLD_1 (BIT_(1))
#define _FIFO_AFULL_FDR_AFULL_THRESHOLD_0 (BIT_(0))
#define _FIFO_AFULL_FDR_AFULL_THRESHOLD (BIT_(0) | BIT_(1) | BIT_(2) | BIT_(3))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_TDM_CTRL
// Register Offset : 0x30
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_TDM_CTRL (I2S_SC_BASE + 0x30)
#define TDM_CTRL_CH15_EN_FIELD_OFFSET 31
#define TDM_CTRL_CH15_EN_FIELD_SIZE 1
#define TDM_CTRL_CH14_EN_FIELD_OFFSET 30
#define TDM_CTRL_CH14_EN_FIELD_SIZE 1
#define TDM_CTRL_CH13_EN_FIELD_OFFSET 29
#define TDM_CTRL_CH13_EN_FIELD_SIZE 1
#define TDM_CTRL_CH12_EN_FIELD_OFFSET 28
#define TDM_CTRL_CH12_EN_FIELD_SIZE 1
#define TDM_CTRL_CH11_EN_FIELD_OFFSET 27
#define TDM_CTRL_CH11_EN_FIELD_SIZE 1
#define TDM_CTRL_CH10_EN_FIELD_OFFSET 26
#define TDM_CTRL_CH10_EN_FIELD_SIZE 1
#define TDM_CTRL_CH9_EN_FIELD_OFFSET 25
#define TDM_CTRL_CH9_EN_FIELD_SIZE 1
#define TDM_CTRL_CH8_EN_FIELD_OFFSET 24
#define TDM_CTRL_CH8_EN_FIELD_SIZE 1
#define TDM_CTRL_CH7_EN_FIELD_OFFSET 23
#define TDM_CTRL_CH7_EN_FIELD_SIZE 1
#define TDM_CTRL_CH6_EN_FIELD_OFFSET 22
#define TDM_CTRL_CH6_EN_FIELD_SIZE 1
#define TDM_CTRL_CH5_EN_FIELD_OFFSET 21
#define TDM_CTRL_CH5_EN_FIELD_SIZE 1
#define TDM_CTRL_CH4_EN_FIELD_OFFSET 20
#define TDM_CTRL_CH4_EN_FIELD_SIZE 1
#define TDM_CTRL_CH3_EN_FIELD_OFFSET 19
#define TDM_CTRL_CH3_EN_FIELD_SIZE 1
#define TDM_CTRL_CH2_EN_FIELD_OFFSET 18
#define TDM_CTRL_CH2_EN_FIELD_SIZE 1
#define TDM_CTRL_CH1_EN_FIELD_OFFSET 17
#define TDM_CTRL_CH1_EN_FIELD_SIZE 1
#define TDM_CTRL_CH0_EN_FIELD_OFFSET 16
#define TDM_CTRL_CH0_EN_FIELD_SIZE 1
#define TDM_CTRL_CHN_NO_FIELD_OFFSET 1
#define TDM_CTRL_CHN_NO_FIELD_SIZE 4
#define TDM_CTRL_TDM_EN_FIELD_OFFSET 0
#define TDM_CTRL_TDM_EN_FIELD_SIZE 1

#define BIT_TDM_CTRL_CH15_EN (BIT_(31))
#define BIT_TDM_CTRL_CH14_EN (BIT_(30))
#define BIT_TDM_CTRL_CH13_EN (BIT_(29))
#define BIT_TDM_CTRL_CH12_EN (BIT_(28))
#define BIT_TDM_CTRL_CH11_EN (BIT_(27))
#define BIT_TDM_CTRL_CH10_EN (BIT_(26))
#define BIT_TDM_CTRL_CH9_EN (BIT_(25))
#define BIT_TDM_CTRL_CH8_EN (BIT_(24))
#define BIT_TDM_CTRL_CH7_EN (BIT_(23))
#define BIT_TDM_CTRL_CH6_EN (BIT_(22))
#define BIT_TDM_CTRL_CH5_EN (BIT_(21))
#define BIT_TDM_CTRL_CH4_EN (BIT_(20))
#define BIT_TDM_CTRL_CH3_EN (BIT_(19))
#define BIT_TDM_CTRL_CH2_EN (BIT_(18))
#define BIT_TDM_CTRL_CH1_EN (BIT_(17))
#define BIT_TDM_CTRL_CH0_EN (BIT_(16))
#define BIT_TDM_CTRL_CH_EN (0xFFFF0000)
#define BIT_TDM_CTRL_CHN_NO_3 (BIT_(4))
#define BIT_TDM_CTRL_CHN_NO_2 (BIT_(3))
#define BIT_TDM_CTRL_CHN_NO_1 (BIT_(2))
#define BIT_TDM_CTRL_CHN_NO_0 (BIT_(1))
#define BIT_TDM_CTRL_CHN_NUMB (BIT_(1)|BIT_(2)|BIT_(3)|BIT_(4))
#define BIT_TDM_CTRL_TDM_EN (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_CDN_I2SSC_REGS_TDM_FD_DIR
// Register Offset : 0x34
// Description     :
//--------------------------------------------------------------------------
#define REG_CDN_I2SSC_REGS_TDM_FD_DIR (I2S_SC_BASE + 0x34)
#define TDM_FD_DIR_CH15_RXEN_FIELD_OFFSET 31
#define TDM_FD_DIR_CH15_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH14_RXEN_FIELD_OFFSET 30
#define TDM_FD_DIR_CH14_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH13_RXEN_FIELD_OFFSET 29
#define TDM_FD_DIR_CH13_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH12_RXEN_FIELD_OFFSET 28
#define TDM_FD_DIR_CH12_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH11_RXEN_FIELD_OFFSET 27
#define TDM_FD_DIR_CH11_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH10_RXEN_FIELD_OFFSET 26
#define TDM_FD_DIR_CH10_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH9_RXEN_FIELD_OFFSET 25
#define TDM_FD_DIR_CH9_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH8_RXEN_FIELD_OFFSET 24
#define TDM_FD_DIR_CH8_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH7_RXEN_FIELD_OFFSET 23
#define TDM_FD_DIR_CH7_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH6_RXEN_FIELD_OFFSET 22
#define TDM_FD_DIR_CH6_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH5_RXEN_FIELD_OFFSET 21
#define TDM_FD_DIR_CH5_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH4_RXEN_FIELD_OFFSET 20
#define TDM_FD_DIR_CH4_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH3_RXEN_FIELD_OFFSET 19
#define TDM_FD_DIR_CH3_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH2_RXEN_FIELD_OFFSET 18
#define TDM_FD_DIR_CH2_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH1_RXEN_FIELD_OFFSET 17
#define TDM_FD_DIR_CH1_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH0_RXEN_FIELD_OFFSET 16
#define TDM_FD_DIR_CH0_RXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH15_TXEN_FIELD_OFFSET 15
#define TDM_FD_DIR_CH15_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH14_TXEN_FIELD_OFFSET 14
#define TDM_FD_DIR_CH14_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH13_TXEN_FIELD_OFFSET 13
#define TDM_FD_DIR_CH13_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH12_TXEN_FIELD_OFFSET 12
#define TDM_FD_DIR_CH12_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH11_TXEN_FIELD_OFFSET 11
#define TDM_FD_DIR_CH11_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH10_TXEN_FIELD_OFFSET 10
#define TDM_FD_DIR_CH10_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH9_TXEN_FIELD_OFFSET 9
#define TDM_FD_DIR_CH9_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH8_TXEN_FIELD_OFFSET 8
#define TDM_FD_DIR_CH8_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH7_TXEN_FIELD_OFFSET 7
#define TDM_FD_DIR_CH7_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH6_TXEN_FIELD_OFFSET 6
#define TDM_FD_DIR_CH6_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH5_TXEN_FIELD_OFFSET 5
#define TDM_FD_DIR_CH5_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH4_TXEN_FIELD_OFFSET 4
#define TDM_FD_DIR_CH4_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH3_TXEN_FIELD_OFFSET 3
#define TDM_FD_DIR_CH3_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH2_TXEN_FIELD_OFFSET 2
#define TDM_FD_DIR_CH2_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH1_TXEN_FIELD_OFFSET 1
#define TDM_FD_DIR_CH1_TXEN_FIELD_SIZE 1
#define TDM_FD_DIR_CH0_TXEN_FIELD_OFFSET 0
#define TDM_FD_DIR_CH0_TXEN_FIELD_SIZE 1

#define BIT_TDM_FD_DIR_CH15_RXEN (BIT_(31))
#define BIT_TDM_FD_DIR_CH14_RXEN (BIT_(30))
#define BIT_TDM_FD_DIR_CH13_RXEN (BIT_(29))
#define BIT_TDM_FD_DIR_CH12_RXEN (BIT_(28))
#define BIT_TDM_FD_DIR_CH11_RXEN (BIT_(27))
#define BIT_TDM_FD_DIR_CH10_RXEN (BIT_(26))
#define BIT_TDM_FD_DIR_CH9_RXEN (BIT_(25))
#define BIT_TDM_FD_DIR_CH8_RXEN (BIT_(24))
#define BIT_TDM_FD_DIR_CH7_RXEN (BIT_(23))
#define BIT_TDM_FD_DIR_CH6_RXEN (BIT_(22))
#define BIT_TDM_FD_DIR_CH5_RXEN (BIT_(21))
#define BIT_TDM_FD_DIR_CH4_RXEN (BIT_(20))
#define BIT_TDM_FD_DIR_CH3_RXEN (BIT_(19))
#define BIT_TDM_FD_DIR_CH2_RXEN (BIT_(18))
#define BIT_TDM_FD_DIR_CH1_RXEN (BIT_(17))
#define BIT_TDM_FD_DIR_CH0_RXEN (BIT_(16))
#define BIT_TDM_FD_DIR_CH15_TXEN (BIT_(15))
#define BIT_TDM_FD_DIR_CH14_TXEN (BIT_(14))
#define BIT_TDM_FD_DIR_CH13_TXEN (BIT_(13))
#define BIT_TDM_FD_DIR_CH12_TXEN (BIT_(12))
#define BIT_TDM_FD_DIR_CH11_TXEN (BIT_(11))
#define BIT_TDM_FD_DIR_CH10_TXEN (BIT_(10))
#define BIT_TDM_FD_DIR_CH9_TXEN (BIT_(9))
#define BIT_TDM_FD_DIR_CH8_TXEN (BIT_(8))
#define BIT_TDM_FD_DIR_CH7_TXEN (BIT_(7))
#define BIT_TDM_FD_DIR_CH6_TXEN (BIT_(6))
#define BIT_TDM_FD_DIR_CH5_TXEN (BIT_(5))
#define BIT_TDM_FD_DIR_CH4_TXEN (BIT_(4))
#define BIT_TDM_FD_DIR_CH3_TXEN (BIT_(3))
#define BIT_TDM_FD_DIR_CH2_TXEN (BIT_(2))
#define BIT_TDM_FD_DIR_CH1_TXEN (BIT_(1))
#define BIT_TDM_FD_DIR_CH0_TXEN (BIT_(0))
#define BIT_TDM_FD_DIR_CH_RX_EN (0xFFFF0000)
#define BIT_TDM_FD_DIR_CH_TX_EN (0xFFFF)

#endif /* SDRV_I2S_SC_H__ */
