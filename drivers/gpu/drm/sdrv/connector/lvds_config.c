/*
* SEMIDRIVE Copyright Statement
* Copyright (c) SEMIDRIVE. All rights reserved
* This software and all rights therein are owned by SEMIDRIVE,
* and are protected by copyright law and other relevant laws, regulations and protection.
* Without SEMIDRIVE's prior written consent and /or related rights,
* please do not use this software or any potion thereof in any form or by any means.
* You may not reproduce, modify or distribute this software except in compliance with the License.
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* You should have received a copy of the License along with this program.
* If not, see <http://www.semidrive.com/licenses/>.
*/

#include <linux/types.h>

#include "lvds_reg.h"
#include "lvds_interface.h"

typedef enum {
    PIXEL_FORMAT_18_BIT = 0b00,
    PIXEL_FORMAT_24_BIT = 0b01,
    PIXEL_FORMAT_30_BIT = 0b10
} LVDS_PIXEL_FORMAT;

typedef enum {
    MAP_FORMAT_JEIDA = 0b0,
    MAP_FORMAT_SWPG = 0b01
} LVDS_MAP_FORMAT;

enum {
	DUALLODD_ODD = 0b0,
	DUALLODD_EVEN = 0b1
};

enum {
	SRC_SEL_7_CLOCK   = 0b0,
	SRC_SEL_3P5_CLOCK = 0b1
};

enum {
	LVDS_CLK_EN 	 = 0b00001,
	LVDS_CLK_X7_EN	 = 0b00010,
	LVDS_CLK_X3P5_EN = 0b00100,
	LVDS_DBG_CLK_EN  = 0b01000,
	LVDS_DSP_CLK_EN  = 0b10000
};

enum {
	LVDS1 = 1,
	LVDS2,
	LVDS3,
	LVDS4,
	LVDS1_LVDS2,
	LVDS3_LVDS4,
	LVDS1_4
};

enum LVDS_CHANNEL {
	LVDS_CH1 = 0,
	LVDS_CH2,
	LVDS_CH3,
	LVDS_CH4,
	LVDS_CH_MAX_NUM
};

static inline void
reg_writel(void __iomem *base, u32 offset, uint32_t val)
{
	//writel(val, base + offset);
	*((volatile uint32_t *)(base + offset)) = (val);
	/*pr_info("val = 0x%08x, read:0x%08x = 0x%08x\n",
			val, base + offset, readl(base+offset));*/
}

static inline uint32_t reg_readl(void __iomem *base, uint32_t offset)
{
	return *((volatile uint32_t *)(base + offset));
}

static inline uint32_t reg_value(unsigned int val,
	unsigned int src, unsigned int shift, unsigned int mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

static void lvds_channel_config(struct lvds_data *lvds, uint8_t chidx)
{
	int val, mux, i;

	val = reg_readl(lvds->base, LVDS_CHN_CTRL_(chidx));
	val = reg_value(lvds->dual_mode, val, CHN_DUALMODE_SHIFT, CHN_DUALMODE_MASK);
	val = reg_value(lvds->pixel_format, val, CHN_BPP_SHIFT, CHN_BPP_MASK);
	val = reg_value(lvds->map_format, val, CHN_FORMAT_SHIFT, CHN_FORMAT_MASK);
	val = reg_value(lvds->vsync_pol, val,
		CHN_VSYNC_POL_SHIFT, CHN_VSYNC_POL_MASK);

	if ((lvds->pixel_format == PIXEL_FORMAT_30_BIT) &&
		lvds->dual_mode) {
		if ((chidx == LVDS_CH1) || (chidx == LVDS_CH2))
			val = reg_value(DUALLODD_ODD, val,
				CHN_DUALODD_SHIFT, CHN_DUALODD_MASK);
		else
			val = reg_value(DUALLODD_EVEN, val,
				CHN_DUALODD_SHIFT, CHN_DUALODD_MASK);
	}

	if (lvds->pixel_format == PIXEL_FORMAT_30_BIT) {
		if ((chidx == LVDS_CH1) || (chidx == LVDS_CH3))
			mux = 0;
		else
			mux = 1;
	} else {
		if (lvds->dual_mode) {
			if ((chidx == LVDS_CH1) || (chidx == LVDS_CH3))
				mux = 0;
			else
				mux = 1;
		} else {
			if ((chidx == LVDS_CH1) || (chidx == LVDS_CH3))
				mux = 0;
			else
				mux = 2;
		}
	}

	val = reg_value(mux, val, CHN_MUX_SHIFT, CHN_MUX_MASK);

	val = reg_value(1, val, CHN_EN_SHIFT, CHN_EN_MASK);
	reg_writel(lvds->base, LVDS_CHN_CTRL_(chidx), val);

	/*config 5 lanes base vdd*/
	for (i = 0; i < 5; i++) {
		reg_writel(lvds->base, LVDS_CHN_PAD_SET_(chidx) + 0x4 * i, 0x9CF);
	}
}

int lvds_config(struct lvds_data *lvds)
{
	int val;

	val = reg_readl(lvds->base, LVDS_BIAS_SET);
	val = reg_value(1, val, BIAS_EN_SHIFT, BIAS_EN_MASK);
	reg_writel(lvds->base, LVDS_BIAS_SET, val);

	if (lvds->pixel_format == PIXEL_FORMAT_30_BIT) {
		if (!lvds->dual_mode) {
			switch (lvds->lvds_select) {
			case LVDS1_LVDS2:
				lvds_channel_config(lvds, LVDS_CH1);
				lvds_channel_config(lvds, LVDS_CH2);
				break;
			case LVDS3_LVDS4:
				lvds_channel_config(lvds, LVDS_CH3);
				lvds_channel_config(lvds, LVDS_CH4);
				break;
			default:
				return -22;
			}
		} else {
			if (lvds->lvds_select == LVDS1_4) {
				lvds_channel_config(lvds, LVDS_CH1);
				lvds_channel_config(lvds, LVDS_CH2);
				lvds_channel_config(lvds, LVDS_CH3);
				lvds_channel_config(lvds, LVDS_CH4);
			} else {
				return -22;
			}
		}
	}else {
		if (!lvds->dual_mode) {
			switch (lvds->lvds_select) {
			case LVDS1:
				lvds_channel_config(lvds, LVDS_CH1);
				break;
			case LVDS2:
				lvds_channel_config(lvds, LVDS_CH2);
				break;
			case LVDS3:
				lvds_channel_config(lvds, LVDS_CH3);
				break;
			case LVDS4:
				lvds_channel_config(lvds, LVDS_CH4);
				break;
			default:
				return -22;
			}
		}else {
			switch (lvds->lvds_select) {
			case LVDS1_LVDS2:
				lvds_channel_config(lvds, LVDS_CH1);
				lvds_channel_config(lvds, LVDS_CH2);
				break;
			case LVDS3_LVDS4:
				lvds_channel_config(lvds, LVDS_CH3);
				lvds_channel_config(lvds, LVDS_CH4);
				break;
			case LVDS1_4:
				val = reg_readl(lvds->base, LVDS_COMBINE);
				val = reg_value(1920 - 1, val, CMB_VLD_HEIGHT_SHIFT, CMB_VLD_HEIGHT_MASK);
				reg_writel(lvds->base, LVDS_COMBINE, val);

				lvds_channel_config(lvds, LVDS_CH1);
				lvds_channel_config(lvds, LVDS_CH2);
				lvds_channel_config(lvds, LVDS_CH3);
				lvds_channel_config(lvds, LVDS_CH4);
				break;
			default:
				return -22;
			}
		}
	}

	return 0;
}

