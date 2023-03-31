/*
 * sdrv-spdif.h
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

#ifndef SDRV_SPDIF_H__
#define SDRV_SPDIF_H__
#include <linux/bitops.h>

#define SPDIF_FIFO_ADDR_OFFSET 0x0010U

/* --------------------------------------------------------------------------
	Register Name   : REG_CDN_SPDIF_REGS_SPDIF_CTRL
	Register Offset : 0x0
	Description     : The SPDIF control register is a read and
			write special function register.
-------------------------------------------------------------------------- */

#define REG_CDN_SPDIF_REGS_SPDIF_CTRL (0x00)
#define SPDIF_CTRL_TSAMPLE_RATE_SHIFT 0
#define SPDIF_CTRL_SFR_ENABLE_SHIFT 8
#define SPDIF_CTRL_SPDIF_ENABLE_SHIFT 9
#define SPDIF_CTRL_FIFO_ENABLE_SHIFT 10
#define SPDIF_CTRL_CLK_ENABLE_SHIFT 11
#define SPDIF_CTRL_TR_MODE_SHIFT 12
#define SPDIF_CTRL_PARITY_CHECK_SHIFT 13
#define SPDIF_CTRL_PARITY_GEN_SHIFT 14
#define SPDIF_CTRL_VALIDITY_CHECK_SHIFT 15
#define SPDIF_CTRL_CHANNEL_MODE_SHIFT 16
#define SPDIF_CTRL_DUPLICATE_SHIFT 17
#define SPDIF_CTRL_SET_PREAMB_B_SHIFT 18
#define SPDIF_CTRL_USE_FIFO_IF_SHIFT 19
#define SPDIF_CTRL_PARITY_MASK_SHIFT 21
#define SPDIF_CTRL_UNDERR_MASK_SHIFT 22
#define SPDIF_CTRL_OVERR_MASK_SHIFT 23
#define SPDIF_CTRL_EMPTY_MASK_SHIFT 24
#define SPDIF_CTRL_AEMPTY_MASK_SHIFT 25
#define SPDIF_CTRL_FULL_MASK_SHIFT 26
#define SPDIF_CTRL_AFULL_MASK_SHIFT 27
#define SPDIF_CTRL_SYNCERR_MASK_SHIFT 28
#define SPDIF_CTRL_LOCK_MASK_SHIFT 29
#define SPDIF_CTRL_BEGIN_MASK_SHIFT 30
#define SPDIF_CTRL_INTREQ_MASK_SHIFT 31

#define BIT_SPDIF_CTRL_TSAMPLE_RATE GENMASK(7, 0)
#define BIT_SPDIF_CTRL_SFR_ENABLE BIT(8)
#define BIT_SPDIF_CTRL_SPDIF_ENABLE BIT(9)
#define BIT_SPDIF_CTRL_FIFO_ENABLE BIT(10)
#define BIT_SPDIF_CTRL_CLK_ENABLE BIT(11)
#define BIT_SPDIF_CTRL_TR_MODE BIT(12)
#define BIT_SPDIF_CTRL_PARITY_CHECK BIT(13)
#define BIT_SPDIF_CTRL_PARITY_GEN BIT(14)
#define BIT_SPDIF_CTRL_VALIDITY_CHECK BIT(15)
#define BIT_SPDIF_CTRL_CHANNEL_MODE BIT(16)
#define BIT_SPDIF_CTRL_DUPLICATE BIT(17)
#define BIT_SPDIF_CTRL_SET_PREAMB_B BIT(18)
#define BIT_SPDIF_CTRL_USE_FIFO_IF BIT(19)
#define BIT_SPDIF_CTRL_PARITY_MASK BIT(21)
#define BIT_SPDIF_CTRL_UNDERR_MASK BIT(22)
#define BIT_SPDIF_CTRL_OVERR_MASK BIT(23)
#define BIT_SPDIF_CTRL_EMPTY_MASK BIT(24)
#define BIT_SPDIF_CTRL_AEMPTY_MASK BIT(25)
#define BIT_SPDIF_CTRL_FULL_MASK BIT(26)
#define BIT_SPDIF_CTRL_AFULL_MASK BIT(27)
#define BIT_SPDIF_CTRL_SYNCERR_MASK BIT(28)
#define BIT_SPDIF_CTRL_LOCK_MASK BIT(29)
#define BIT_SPDIF_CTRL_BEGIN_MASK BIT(30)
#define BIT_SPDIF_CTRL_INTREQ_MASK BIT(31)

/* --------------------------------------------------------------------------
	Register Name   : REG_CDN_SPDIF_REGS_SPDIF_INT_REG
	Register Offset : 0x1
	Description     : The SPDIF control register is a read/write
			special function register.
-------------------------------------------------------------------------- */

#define REG_CDN_SPDIF_REGS_SPDIF_INT_REG (0x4)
/* rsample rate bits is read-only */
#define SPDIF_INT_RSAMPLE_RATE_SHIFT 0
#define SPDIF_INT_PREAMBLE_DEL_SHIFT 8
#define SPDIF_INT_PARITYO_SHIFT 21
#define SPDIF_INT_TDATA_UNDERR_SHIFT 22
#define SPDIF_INT_RDATA_OVERR_SHIFT 23
#define SPDIF_INT_FIFO_EMPTY_SHIFT 24
#define SPDIF_INT_FIFO_AEMPTY_SHIFT 25
#define SPDIF_INT_FIFO_FULL_SHIFT 26
#define SPDIF_INT_FIFO_AFULL_SHIFT 27
#define SPDIF_INT_SYNCERR_SHIFT 28
#define SPDIF_INT_LOCK_SHIFT 29
#define SPDIF_INT_BLOCK_BEGIN_SHIFT 30

#define BIT_SPDIF_INT_RSAMPLE_RATE GENMASK(7, 0)
#define BIT_SPDIF_INT_PREAMBLE_DEL GENMASK(20,8)
#define BIT_SPDIF_INT_PARITYO BIT(21)
#define BIT_SPDIF_INT_TDATA_UNDERR BIT(22)
#define BIT_SPDIF_INT_RDATA_OVERR BIT(23)
#define BIT_SPDIF_INT_FIFO_EMPTY BIT(24)
#define BIT_SPDIF_INT_FIFO_AEMPTY BIT(25)
#define BIT_SPDIF_INT_FIFO_FULL BIT(26)
#define BIT_SPDIF_INT_FIFO_AFULL BIT(27)
#define BIT_SPDIF_INT_SYNCERR BIT(28)
#define BIT_SPDIF_INT_LOCK BIT(29)
#define BIT_SPDIF_INT_BLOCK_BEGIN BIT(30)

/* --------------------------------------------------------------------------
	Register Name   : REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL
	Register Offset : 0x10
	Description     : The SPDIF FIFO control register is a read
		    write special function register.
-------------------------------------------------------------------------- */

#define REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL (0x08)
#define SPDIF_FIFO_CTRL_AEMPTY_THRESHOLD_SHIFT 0
#define SPDIF_FIFO_CTRL_AFULL_THRESHOLD_SHIFT 16

/* TODO: check spdif fifo depth */
#define BIT_SPDIF_FIFO_CTRL_AEMPTY_THRESHOLD GENMASK(7,0)
#define BIT_SPDIF_FIFO_CTRL_AFULL_THRESHOLD GENMASK(23, 16)

/* --------------------------------------------------------------------------
	Register Name   : REG_CDN_SPDIF_REGS_SPDIF_STAT_REG
	Register Offset : 0x11
	Description     : The SPDIF FIFO status register is a read only
			special function register.
-------------------------------------------------------------------------- */

#define REG_CDN_SPDIF_REGS_SPDIF_STAT_REG (0x0c)
#define SPDIF_STAT_FIFO_LEVEL_SHIFT 0
#define SPDIF_STAT_PARITY_FLAG_SHIFT 21
#define SPDIF_STAT_UNDERR_FLAG_SHIFT 22
#define SPDIF_STAT_OVRERR_FLAG_SHIFT 23
#define SPDIF_STAT_EMPTY_FLAG_SHIFT 24
#define SPDIF_STAT_AEMPTY_FLAG_SHIFT 25
#define SPDIF_STAT_FULL_FLAG_SHIFT 26
#define SPDIF_STAT_AFULL_FLAG_SHIFT 27
#define SPDIF_STAT_SYNCERR_FLAG_SHIFT 28
#define SPDIF_STAT_LOCK_FLAG_SHIFT 29
#define SPDIF_STAT_BEGIN_FLAG_SHIFT 30
#define SPDIF_STAT_RIGHT_FLAG_SHIFT 31

#define BIT_SPDIF_STAT_FIFO_LEVEL GENMASK(7,0)
#define BIT_SPDIF_STAT_PARITY_FLAG BIT(21)
#define BIT_SPDIF_STAT_UNDERR_FLAG BIT(22)
#define BIT_SPDIF_STAT_OVRERR_FLAG BIT(23)
#define BIT_SPDIF_STAT_EMPTY_FLAG BIT(24)
#define BIT_SPDIF_STAT_AEMPTY_FLAG BIT(25)
#define BIT_SPDIF_STAT_FULL_FLAG BIT(26)
#define BIT_SPDIF_STAT_AFULL_FLAG BIT(27)
#define BIT_SPDIF_STAT_SYNCERR_FLAG BIT(28)
#define BIT_SPDIF_STAT_LOCK_FLAG BIT(29)
#define BIT_SPDIF_STAT_BEGIN_FLAG BIT(30)
#define BIT_SPDIF_STAT_RIGHT_FLAG BIT(31)

#endif