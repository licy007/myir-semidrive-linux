
/*
 * sdrv-i2s-mc.h
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

#ifndef SDRV_I2S_MC_H__
#define SDRV_I2S_MC_H__
#include <linux/bitops.h>
#define BIT_(nr) (1UL << (nr))
#define I2S_MC_SAMPLE_RATE_CALC(fclk, fsample, chn_width)                      \
	(( fclk + fsample * chn_width) / (2 * fsample * chn_width))

#define I2S_MC_SAMPLE_RESOLUTION_TO_CONFIG(bit_format) (bit_format - 1)

enum { I2S_CHAN_0 = 0,
       I2S_CHAN_1 = 1,
       I2S_CHAN_2 = 2,
       I2S_CHAN_3 = 3,
       I2S_CHAN_4 = 4,
       I2S_CHAN_5 = 5,
       I2S_CHAN_6 = 6,
       I2S_CHAN_7 = 7,
};

/* configurable parameter
    CDNSI2SMC_WIDTH:32
    CDNSI2SMC_DEPTH_TX:9  2^9 = 512
    CDNSI2SMC_DEPTH_RX:9  2^9 = 512
    CDNSI2SMC_RATEW:11  */
#define I2S_MC_FIFO_OFFSET 0x3c

#define I2S_MC_TX_DEPTH 512
#define I2S_MC_RX_DEPTH 512

#define I2S_MC_DATA_WIDTH 32

/* --------------------------------------------------------------------------
	Register Name   : CDN I2SMC REGS I2S_CTRL
	Register Offset : 0x0
	Description     : The i2s transceiver control register is a read and
		    write special function register.
-------------------------------------------------------------------------- */

#define MC_I2S_CTRL (0x000)
#define CTRL_EN_SHIFT 0
#define CTRL_TR_CFG_SHIFT 8
#define CTRL_LOOP_BACK_0_1_SHIFT 16
#define CTRL_LOOP_BACK_2_3_SHIFT 17
#define CTRL_LOOP_BACK_4_5_SHIFT 18
#define CTRL_LOOP_BACK_7_8_SHIFT 19
#define CTRL_SFR_RST_SHIFT 20
#define CTRL_T_MS_SHIFT 21
#define CTRL_R_MS_SHIFT 22
#define CTRL_TFIFO_RST_SHIFT 23
#define CTRL_RFIFO_RST_SHIFT 24
#define CTRL_TSYNC_RST_SHIFT 25
#define CTRL_RSYNC_RST_SHIFT 26
#define CTRL_TSYNC_LOOP_BACK_SHIFT 27
#define CTRL_RSYNC_LOOP_BACK_SHIFT 28

#define CTRL_EN_MASK GENMASK(7, 0)
#define CTRL_TR_CFG_MASK GENMASK(15, 8)

#define CTRL_LOOP_BACK_0_1_MASK BIT(16)
#define CTRL_LOOP_BACK_2_3_MASK BIT(17)
#define CTRL_LOOP_BACK_4_5_MASK BIT(18)
#define CTRL_LOOP_BACK_7_8_MASK BIT(19)

#define CTRL_SFR_RST_MASK BIT(20)
#define CTRL_T_MS_MASK BIT(21)
#define CTRL_R_MS_MASK BIT(22)
#define CTRL_TFIFO_RST_MASK BIT(23)
#define CTRL_RFIFO_RST_MASK BIT(24)
#define CTRL_TSYNC_RST_MASK BIT(25)
#define CTRL_RSYNC_RST_MASK BIT(26)
#define CTRL_TSYNC_LOOP_BACK_MASK BIT(27)
#define CTRL_RSYNC_LOOP_BACK_MASK BIT(28)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_I2S_INTR_STAT
 Register Offset : 0x4
 Description     : Interrupt status register is read or write special function
		register.
-------------------------------------------------------------------------- */
#define MC_I2S_INTR_STAT (0x004)
#define INTR_TDATA_UNDERR_SHIFT 0
/* The code is a binary notation of the channels's number. 1~3 */
#define INTR_UNDERR_CODE_SHIFT 1
#define INTR_RDATA_OVRERR_SHIFT 4
/* The code is a binary notation of the channels's number. 5~7 */
#define INTR_OVRERR_CODE_SHIFT 5
#define INTR_TFIFO_EMPTY_SHIFT 8
#define INTR_TFIFO_AEMPTY_SHIFT 9
#define INTR_TFIFO_FULL_SHIFT 10
#define INTR_TFIFO_AFULL_SHIFT 11
#define INTR_RFIFO_EMPTY_SHIFT 12
#define INTR_RFIFO_AEMPTY_SHIFT 13
#define INTR_RFIFO_FULL_SHIFT 14
#define INTR_RFIFO_AFULL_SHIFT 15

#define INTR_TDATA_UNDERR_MASK BIT(0)
/* The code is a binary notation of the channels's number. 1~3 */
#define INTR_UNDERR_CODE_MASK GENMASK(3, 1)
#define INTR_RDATA_OVRERR_MASK BIT(4)
/* The code is a binary notation of the channels's number. 5~7 */
#define INTR_OVRERR_CODE_MASK GENMASK(7, 5)
#define INTR_TFIFO_EMPTY_MASK BIT(8)
#define INTR_TFIFO_AEMPTY_MASK BIT(9)
#define INTR_TFIFO_FULL_MASK BIT(10)
#define INTR_TFIFO_AFULL_MASK BIT(11)
#define INTR_RFIFO_EMPTY_MASK BIT(12)
#define INTR_RFIFO_AEMPTY_MASK BIT(13)
#define INTR_RFIFO_FULL_MASK BIT(14)
#define INTR_RFIFO_AFULL_MASK BIT(15)

/* --------------------------------------------------------------------------
 Register Name   : CDN I2SMC REGS I2S_SRR
 Register Offset : 0x8
 Description     : I2s sample rate and resolution
-------------------------------------------------------------------------- */

#define MC_I2S_SRR (0x008)
/*  CDNSI2SMC_RATEW:11 rx sample rate is 16+ CDNSI2SMC_RATEW -1 */
#define TSAMPLE_RATE_SHIFT 0
#define TRESOLUTION_SHIFT 11
#define RSAMPLE_RATE_SHIFT 16
#define RRESOLUTION_SHIFT 27

#define TSAMPLE_RATE_MASK GENMASK(10, 0)
#define TRESOLUTION_MASK GENMASK(15, 11)
#define RSAMPLE_RATE_MASK GENMASK(26, 16)
#define RRESOLUTION_MASK GENMASK(31, 27)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_CID_CTRL
 Register Offset : 0xc
 Description     : The clock strobes and interrupt masks controls register is
		    a read/write special function register
-------------------------------------------------------------------------- */
#define MC_I2S_CID_CTRL (0x00c)
/* i2s clock strobe from 0 to 7 */
#define CLK_STROBE_SHIFT 0
#define STROBE_TS_SHIFT 8
#define STROBE_RS_SHIFT 9
#define INTREQ_MASK_SHIFT 15
/* Bit masking for interrupt request generation after underrun /overrun. from 0
 * to 7 */
#define I2S_MASK_SHIFT 16
#define TFIFO_EMPTY_MASK_SHIFT 24
#define TFIFO_AEMPTY_MASK_SHIFT 25
#define TFIFO_FULL_MASK_SHIFT 26
#define TFIFO_AFULL_MASK_SHIFT 27
#define RFIFO_EMPTY_MASK_SHIFT 28
#define RFIFO_AEMPTY_MASK_SHIFT 29
#define RFIFO_FULL_MASK_SHIFT 30
#define RFIFO_AFULL_MASK_SHIFT 31

#define CLK_STROBE_MASK GENMASK(7, 0)
#define STROBE_TS_MASK BIT(8)
#define STROBE_RS_MASK BIT(9)
#define INTREQ_MASK_MASK BIT(15)
/* Bit masking for interrupt request generation after underrun /overrun. from 0
 * to 7 */
#define I2S_MASK_MASK GENMASK(23, 16)
#define TFIFO_EMPTY_MASK_MASK BIT(24)
#define TFIFO_AEMPTY_MASK_MASK BIT(25)
#define TFIFO_FULL_MASK_MASK BIT(26)
#define TFIFO_AFULL_MASK_MASK BIT(27)
#define RFIFO_EMPTY_MASK_MASK BIT(28)
#define RFIFO_AEMPTY_MASK_MASK BIT(29)
#define RFIFO_FULL_MASK_MASK BIT(30)
#define RFIFO_AFULL_MASK_MASK BIT(31)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_TFIFO_STAT
 Register Offset : 0x10
 Description     :The Transmit FIFO Level status register is read only special
			    function register.
			    tlevel CDNSI2SMC_DEPTH_TX-0
-------------------------------------------------------------------------- */
#define MC_I2S_TFIFO_STAT (0x010)
#define I2S_TFIFO_SHIFT 0
#define I2S_TFIFO_MASK GENMASK(8, 0)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_RFIFO_STAT
 Register Offset : 0x14
 Description     :The receive FIFO Level status register is read only special
			    function register.
			    rlevel CDNSI2SMC_DEPTH_RX-0
-------------------------------------------------------------------------- */
#define MC_I2S_RFIFO_STAT (0x014)

#define I2S_RFIFO_SHIFT 0
#define I2S_RFIFO_MASK GENMASK(8, 0)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_TFIFO_CTRL
 Register Offset : 0x1c
 Description     : The transmit FIFO thresholds control register is a read and
			write special function register
-------------------------------------------------------------------------- */
#define MC_I2S_TFIFO_CTRL (0x018)

#define TAEMPTY_THRESHOLD_SHIFT 0
#define TAFULL_THRESHOLD_SHIFT 16
#define TAEMPTY_THRESHOLD_MASK GENMASK(7, 0)
#define TAFULL_THRESHOLD_MASK GENMASK(23, 16)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_RFIFO_CTRL
 Register Offset : 0x1c
 Description     :
-------------------------------------------------------------------------- */
#define MC_I2S_RFIFO_CTRL (0x01c)

#define RAEMPTY_THRESHOLD_SHIFT 0
#define RAFULL_THRESHOLD_SHIFT 16
#define RAEMPTY_THRESHOLD_MASK GENMASK(7, 0)
#define RAFULL_THRESHOLD_MASK GENMASK(23, 16)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_DEV_CONF
 Register Offset : 0x20
 Description     : The device configuration register is read/write programming
		register.
-------------------------------------------------------------------------- */
#define MC_I2S_DEV_CONF (0x020)

#define TRAN_SCK_POLAR_SHIFT 0
#define TRAN_WS_POLAR_SHIFT 1
#define TRAN_APB_ALIGN_LR_SHIFT 2
#define TRAN_I2S_ALIGN_LR_SHIFT 3
#define TRAN_DATA_WS_DEL_SHIFT 4
#define TRAN_WS_DSP_MODE_SHIFT 5
#define REC_SCK_POLAR_SHIFT 6
#define REC_WS_POLAR_SHIFT 7
#define REC_APB_ALIGN_LR_SHIFT 8
#define I2S_ALIGN_LR_SHIFT 9
#define REC_DATA_WS_DEL_SHIFT 10
#define REC_WS_DSP_MODE_SHIFT 11

#define TRAN_SCK_POLAR_MASK BIT(0)
#define TRAN_WS_POLAR_MASK BIT(1)
#define TRAN_APB_ALIGN_LR_MASK BIT(2)
#define TRAN_I2S_ALIGN_LR_MASK BIT(3)
#define TRAN_DATA_WS_DEL_MASK BIT(4)
#define TRAN_WS_DSP_MODE_MASK BIT(5)
#define REC_SCK_POLAR_MASK BIT(6)
#define REC_WS_POLAR_MASK BIT(7）
#define REC_APB_ALIGN_LR_MASK BIT(8）
#define I2S_ALIGN_LR_MASK BIT(9)
#define REC_DATA_WS_DEL_MASK BIT(10)
#define REC_WS_DSP_MODE_MASK BIT(11)

/* --------------------------------------------------------------------------
 Register Name   : CDN_I2SMC_REGS_I2S_POLL_STAT
 Register Offset : 0x24
 Description     : The i2s status register for controlling the IP by polling is
		read only special function regitster.
-------------------------------------------------------------------------- */
#define MC_I2S_POLL_STAT (0x024)

#define STAT_TX_EMPTY_SHIFT 0
#define STAT_TX_AEMPTY_SHIFT 1
#define STAT_TX_UNDERRUN_SHIFT 2
#define STAT_RX_FULL_SHIFT 4
#define STAT_RX_AFULL_SHIFT 5
#define STAT_RX_OVERRUN_SHIFT 6

#define STAT_TX_EMPTY_MASK BIT(0)
#define STAT_TX_AEMPTY_MASK BIT(1)
#define STAT_TX_UNDERRUN_MASK BIT(2)
#define STAT_RX_FULL_MASK BIT(4)
#define STAT_RX_AFULL_MASK BIT(5)
#define STAT_RX_OVERRUN_MASK BIT(6)

#endif /* SDRV_MC_I2S_H__ */
