/*
 * pwm-sd.c
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

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/math64.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/mfd/sd-timers.h>
#include <linux/mfd/pwm-sd.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>

#define DEBUG_FUNC_PRT                                                         \
	{                                                                      \
		/* pr_err(" %s:%i <-- \n", __func__, __LINE__); */             \
	}

#define CPT_TMR_DIV 0
#define CPT_CHN_TOTAL 4
#define OP_MODE_PWM 1
uint32_t int_count = 0;

typedef enum pwm_direct { PWM_CAPTURE, PWM_PLAYBACK, PWM_ALL } pwm_direct_t;

struct sd_pwm_ip {
	u32 direction;
};

static const struct sd_pwm_ip pwm_ip[] = {
    {.direction = PWM_CAPTURE},
    {.direction = PWM_PLAYBACK},
};
/* Regfield IDs */
enum
{
	/* Bits in PWM_CTRL*/

	TIM_CPT_INT_STAT,
	TIM_STA_EN,
	TIM_SIG_EN,
	TIM_CPT_C_EN, //local channel D
	TIM_CPT_D_EN, //local channel D
	TIM_CLK_CONFIG,
	TIM_CNT_CONFIG,
	TIM_CNT_G0_CONFIG,
	/* Keep last */
	MAX_REGFIELDS
};

struct sd_cpt_channel
{
	unsigned int index;
	struct mutex lock;
	wait_queue_head_t wait;
};

struct sd_play_channel
{
	unsigned int index;
	struct mutex lock;
	wait_queue_head_t wait;
};

struct sd_pwm_compat_data
{
	const struct reg_field *reg_fields;
	unsigned int cpt_num_devs;
	unsigned int play_num_devs;
	unsigned int max_pwm_cnt;
	unsigned int max_prescale;
	// unsigned int pwm_num_devs;
};
struct sd_pwm_chip
{
	struct device *dev;
	u32 cpt_rate;
	struct clk *capt_clk;
	struct clk *plby_clk;
	struct regmap *regmap;
	struct sd_pwm_compat_data *cdata;
	struct regmap_field *tim_clk_config;
	struct regmap_field *tim_cnt_config;
	struct regmap_field *tim_sta_en;
	struct regmap_field *tim_sig_en;
	struct regmap_field *tim_cpt_en;
	struct regmap_field *tim_cpt_c_en;
	struct regmap_field *tim_cpt_d_en;
	struct regmap_field *tim_cpt_int_stat;
	struct regmap_field *tim_cnt_g0_config;
	struct pwm_chip chip;
	struct pwm_device *cur;
	unsigned long configured;
	unsigned int en_count;
	struct mutex sd_pwm_lock; /* To sync between enable/disable calls */
	u32 map_base;
	u32 enable_interrupt;
	uint32_t cnt_per_ms;
	uint32_t cnt_per_us;
	int32_t irq_err_no;
	pwm_direct_t direct;
	drv_pwm_simple_cfg_t play_cfg;
	drv_pwm_simple_context_t pwm_ctx;
	void __iomem *mmio;
};

static const struct reg_field sd_pwm_regfields[MAX_REGFIELDS] = {
	[TIM_CLK_CONFIG] = REG_FIELD(TIM_CLK_CONFIG_OFF, 0, 31),
	[TIM_CNT_CONFIG] = REG_FIELD(CNT_CONFIG_OFF, 6, 6),
	[TIM_CNT_G0_CONFIG] = REG_FIELD(CNT_CONFIG_OFF, 0, 0),
	[TIM_CPT_INT_STAT] = REG_FIELD(INT_STA_OFF, 22, 23),
	[TIM_STA_EN] = REG_FIELD(INT_STA_EN_OFF, 22, 23),	//chan FIFO C,D
	[TIM_SIG_EN] = REG_FIELD(INT_SIG_EN_OFF, 22, 23),	//chan FIFO C,D
	[TIM_CPT_C_EN] = REG_FIELD(CPT_C_CONFIG_OFF, 0, 0), //chan C
	[TIM_CPT_D_EN] = REG_FIELD(CPT_D_CONFIG_OFF, 0, 0), //chan D
};

static inline struct sd_pwm_chip *to_sd_pwmchip(struct pwm_chip *chip)
{
	return container_of(chip, struct sd_pwm_chip, chip);
}

static int32_t timer_drv_clk_init(struct sd_pwm_chip *pc, uint32_t clk_sel,
				  uint32_t clk_div)
{
	u32 value = 0;

	if (!pc->tim_clk_config) {
		printk("pc->tim_clk_config is null\n");
		return -1;
	}
	pc->cnt_per_ms = (pc->cpt_rate / (clk_div + 1)) / 1000;
	pc->cnt_per_us = pc->cnt_per_ms / 1000;
	regmap_read(pc->regmap, TIM_CLK_CONFIG_OFF, &value);
	value &= ~(FM_TIM_CLK_CONFIG_SRC_CLK_SEL | FM_TIM_CLK_CONFIG_DIV_NUM);
	value |= (FV_TIM_CLK_CONFIG_SRC_CLK_SEL(clk_sel) |
		  FV_TIM_CLK_CONFIG_DIV_NUM(clk_div));
	return regmap_write(pc->regmap, TIM_CLK_CONFIG_OFF, value);
}

static int timer_drv_func_cpt_set(struct sd_pwm_chip *pc,
				  timer_drv_func_cpt_t *cpt_cfg)
{
	uint32_t value = 0;
	int32_t ret = 0;
	int32_t i = 0;
	u32 reg_offset = CPT_C_CONFIG_OFF;
	for (i = 0; i < 2; i++) {
		regmap_read(pc->regmap, reg_offset, &value);

		if (1 == i) // Chann D
			cpt_cfg->trig_mode = TIMER_DRV_CPT_RISING_EDGE;
		else // Chann C
			cpt_cfg->trig_mode = TIMER_DRV_CPT_FALLING_EDGE;

		value &=
		    ~(FM_CPT_A_CONFIG_CNT_SEL | BM_CPT_A_CONFIG_DUAL_CPT_MODE |
		      BM_CPT_A_CONFIG_SINGLE_MODE |
		      FM_CPT_A_CONFIG_CPT_TRIG_MODE);

		value |= ((cpt_cfg->cpt_cnt_sel << 3) |
			  (cpt_cfg->dual_cpt_mode << 2) |
			  (cpt_cfg->single_mode << 1) |
			  FV_CPT_A_CONFIG_CPT_TRIG_MODE(cpt_cfg->trig_mode));

		regmap_write(pc->regmap, reg_offset, value);
		regmap_read(pc->regmap, reg_offset, &value);

		reg_offset += 0x4;
	}
	/*
	Hardware input D, share to C.
	*/
	value = 0;
	regmap_read(pc->regmap, SSE_CTRL_OFF, &value);
	// 1. SSE_CTRL register bit2 is enable.
	value |= BM_SSE_CTRL_CPT_C_SYS_EN;
	// 2. SSE_CTRL register bit3 is clear.
	value &= ~(BM_SSE_CTRL_CPT_D_SYS_EN);
	ret = regmap_write(pc->regmap, SSE_CTRL_OFF, value);
	if (ret)
		dev_err(pc->dev, "reg:SSE_CPT_C_OFF write error \n");
	// 3. SSE_CPT_C register is set to 0x0xff00ff00.
	ret = regmap_write(pc->regmap, SSE_CPT_C_OFF, 0xff00ff00);
	if (ret)
		dev_err(pc->dev, "reg:SSE_CPT_C_OFF write error \n");
	return ret;
}

static int32_t
timer_drv_func_disable_cmp(struct sd_pwm_chip *pc) // disable compare mode
{
	int32_t ret = 0;
	uint32_t value = 0;
	uint32_t reg_offset = CMP_A_CONFIG_OFF;
	int i = 0;
	for (i = 0; i < pc->cdata->cpt_num_devs; i++) {
		ret = regmap_read(pc->regmap, reg_offset, &value);
		if (ret)
			break;

		value &= (~BM_CMP_A_CONFIG_EN);
		ret = regmap_write(pc->regmap, reg_offset, value);
		if (ret)
			break;
		regmap_read(pc->regmap, reg_offset, &value);
		reg_offset += 0x4;
	}

	return ret;
}

static int32_t timer_drv_func_dma_init(struct sd_pwm_chip *pc, uint32_t dma_sel,
				       uint32_t fifo_wml)
{
	int32_t ret = 0;
	uint32_t value = 0;
	uint32_t reg_offset = DMA_CTRL_OFF;
	regmap_read(pc->regmap, reg_offset, &value);
	value &= ~(FM_DMA_CTRL_CHN_C_WML | FM_DMA_CTRL_CHN_C_SEL |
		   BM_DMA_CTRL_CHN_C_EN | FM_DMA_CTRL_CHN_D_WML |
		   FM_DMA_CTRL_CHN_D_SEL | BM_DMA_CTRL_CHN_D_EN);
	value |= (FV_DMA_CTRL_CHN_C_WML(fifo_wml) |
		  (FV_DMA_CTRL_CHN_C_SEL(dma_sel)) | BM_DMA_CTRL_CHN_C_EN |
		  FV_DMA_CTRL_CHN_D_WML(fifo_wml) |
		  (FV_DMA_CTRL_CHN_D_SEL(dma_sel)) | BM_DMA_CTRL_CHN_D_EN);
	ret = regmap_write(pc->regmap, reg_offset, value);
	if (ret) {
		dev_err(pc->dev, "reg write error, reg:%#x, value: %#x",
			reg_offset, value);
		return ret;
	}
	regmap_read(pc->regmap, reg_offset, &value);
	return ret;
}

static void drv_pwm_force_output(sdrv_pwm_t *pwm_dev, drv_pwm_chn_t chn,
			  drv_pwm_force_out_t force_out)
{
	sdrv_pwm_cmp_ch_config1_t cmp_cfg1_temp;
	uint32_t *cmp_cfg1_ptr;

	if (chn == DRV_PWM_CHN_A) {
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_a_config1.val);
	} else if (chn == DRV_PWM_CHN_B) {
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_b_config1.val);
	} else if (chn == DRV_PWM_CHN_C) {
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_c_config1.val);
	} else if (chn == DRV_PWM_CHN_D) {
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_d_config1.val);
	} else {
		return;
	}

	cmp_cfg1_temp.val = *cmp_cfg1_ptr;
	if (force_out == DRV_PWM_FORCE_OUT_DISABLE) {
		cmp_cfg1_temp.frc_high = 0;
		cmp_cfg1_temp.frc_low = 0;
	} else if (force_out == DRV_PWM_FORCE_OUT_HIGH) {
		cmp_cfg1_temp.frc_high = 1;
		cmp_cfg1_temp.frc_low = 0;
	} else if (force_out == DRV_PWM_FORCE_OUT_LOW) {
		cmp_cfg1_temp.frc_high = 0;
		cmp_cfg1_temp.frc_low = 1;
	}

	*cmp_cfg1_ptr = cmp_cfg1_temp.val;
}

static void drv_pwm_cmp_out_start(sdrv_pwm_t *pwm_dev)
{
	sdrv_pwm_cmp_ctrl_t cmp_ctrl;

	drv_pwm_force_output(pwm_dev, DRV_PWM_CHN_A, DRV_PWM_FORCE_OUT_DISABLE);
	drv_pwm_force_output(pwm_dev, DRV_PWM_CHN_B, DRV_PWM_FORCE_OUT_DISABLE);
	drv_pwm_force_output(pwm_dev, DRV_PWM_CHN_C, DRV_PWM_FORCE_OUT_DISABLE);
	drv_pwm_force_output(pwm_dev, DRV_PWM_CHN_D, DRV_PWM_FORCE_OUT_DISABLE);

	cmp_ctrl.val = pwm_dev->cmp_ctrl.val;
	cmp_ctrl.cmp_en = 1;
	pwm_dev->cmp_ctrl.val = cmp_ctrl.val;
}

static int sd_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct sd_pwm_chip *pc = to_sd_pwmchip(chip);

	drv_pwm_cmp_out_start((sdrv_pwm_t *)pc->mmio);

	return 0;
}

static void sd_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	DEBUG_FUNC_PRT
	return;
}

static void sd_pwm_set_polarity(struct pwm_chip *chip, struct pwm_device *pwm,
				enum pwm_polarity polarity)
{
	struct sd_pwm_chip *pc = to_sd_pwmchip(chip);
	drv_pwm_simple_cfg_t *play_cfg = &pc->play_cfg;
	if (polarity == PWM_POLARITY_INVERSED) {
		play_cfg->cmp_cfg[pwm->hwpwm].phase =
		    DRV_PWM_PHASE_POLARITY_NEG;
		dev_info(pc->dev, "pwm->hwpwm: %d, PWM_POLARITY_INVERSED",
			pwm->hwpwm);
	} else {
		play_cfg->cmp_cfg[pwm->hwpwm].phase =
		    DRV_PWM_PHASE_POLARITY_POS;
		dev_info(pc->dev, "pwm->hwpwm: %d, PWM_POLARITY_NORMAL",
			pwm->hwpwm);
	}
}

static void drv_pwm_reset(sdrv_pwm_t *pwm_dev)
{
	sdrv_pwm_cmp_ctrl_t cmp_ctrl;

	cmp_ctrl.val = pwm_dev->cmp_ctrl.val;
	cmp_ctrl.sw_rst = 1;
	pwm_dev->cmp_ctrl.val = cmp_ctrl.val;
}

static void drv_pwm_clk_init(sdrv_pwm_t *pwm_dev, drv_pwm_src_clk_t clk_src,
			     uint16_t clk_div)
{
	sdrv_pwm_cnt_g0_config_t cnt_g0_config;

	cnt_g0_config.val = pwm_dev->cnt_g0_config.val;
	cnt_g0_config.src_clk_sel = clk_src;
	cnt_g0_config.div_num = clk_div;
	pwm_dev->cnt_g0_config.val = cnt_g0_config.val;
}
static void drv_pwm_single_mode_set(sdrv_pwm_t *pwm_dev,
				    drv_pwm_single_mode_t mode)
{
	sdrv_pwm_cmp_ctrl_t cmp_ctrl;

	cmp_ctrl.val = pwm_dev->cmp_ctrl.val;
	cmp_ctrl.sw_rst = 0;
	cmp_ctrl.single_mode = mode;
	pwm_dev->cmp_ctrl.val = cmp_ctrl.val;
}
static void drv_pwm_group_num_set(sdrv_pwm_t *pwm_dev,
				  drv_pwm_group_num_t grp_num)
{
	sdrv_pwm_cmp_config_t cmp_config;

	cmp_config.val = pwm_dev->cmp_config.val;
	cmp_config.grp_num = grp_num;
	pwm_dev->cmp_config.val = cmp_config.val;
}

static void drv_pwm_count_reset(sdrv_pwm_t *pwm_dev)
{
	sdrv_pwm_cnt_g0_config_t cnt_g0_config;

	cnt_g0_config.val = pwm_dev->cnt_g0_config.val;
	cnt_g0_config.frc_rld = 1;
	cnt_g0_config.int_clr = 1;
	pwm_dev->cnt_g0_config.val = cnt_g0_config.val;

	// while(pwm_dev->cnt_g0_config.frc_rld);
}
static uint32_t drv_pwm_none_zero_bits_index(uint32_t num)
{
	uint32_t index = 0;

	while (num) {
		index++;
		num >>= 1;
	}
	if (index >= 1) {
		index--;
	}

	return index;
}
static void drv_pwm_dual_cmp_mode_set(sdrv_pwm_t *pwm_dev, bool enable)
{
	sdrv_pwm_cmp_config_t cmp_config;

	cmp_config.val = pwm_dev->cmp_config.val;
	cmp_config.dual_cmp_mode = enable;
	pwm_dev->cmp_config.val = cmp_config.val;
}
static void drv_pwm_edge_align_chn_set(sdrv_pwm_t *pwm_dev, drv_pwm_chn_t chn,
				       drv_pwm_phase_polarity_t phase)
{
	sdrv_pwm_cmp_ch_config0_t cmp_cfg0_temp;
	sdrv_pwm_cmp_ch_config1_t cmp_cfg1_temp;
	uint32_t *cmp_cfg0_ptr;
	uint32_t *cmp_cfg1_ptr;

	if (chn == DRV_PWM_CHN_A) {
		cmp_cfg0_ptr = (uint32_t *)&(pwm_dev->cmp_a_config0.val);
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_a_config1.val);
	} else if (chn == DRV_PWM_CHN_B) {
		cmp_cfg0_ptr = (uint32_t *)&(pwm_dev->cmp_b_config0.val);
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_b_config1.val);
	} else if (chn == DRV_PWM_CHN_C) {
		cmp_cfg0_ptr = (uint32_t *)&(pwm_dev->cmp_c_config0.val);
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_c_config1.val);
	} else if (chn == DRV_PWM_CHN_D) {
		cmp_cfg0_ptr = (uint32_t *)&(pwm_dev->cmp_d_config0.val);
		cmp_cfg1_ptr = (uint32_t *)&(pwm_dev->cmp_d_config1.val);
	} else {
		return;
	}

	cmp_cfg0_temp.val = *cmp_cfg0_ptr;
	cmp_cfg1_temp.val = *cmp_cfg1_ptr;

	if (phase == DRV_PWM_PHASE_POLARITY_POS) {
		cmp_cfg0_temp.cmp0_out_mode = DRV_CMP_OUT_LEVEL_LOW;
		cmp_cfg1_temp.ovf_out_mode = DRV_CMP_OUT_LEVEL_HIGH;
	} else {
		cmp_cfg0_temp.cmp0_out_mode = DRV_CMP_OUT_LEVEL_HIGH;
		cmp_cfg1_temp.ovf_out_mode = DRV_CMP_OUT_LEVEL_LOW;
	}

	*cmp_cfg0_ptr = cmp_cfg0_temp.val;
	*cmp_cfg1_ptr = cmp_cfg1_temp.val;
}

static void drv_pwm_edge_align_duty_set(sdrv_pwm_t *pwm_dev, drv_pwm_chn_t chn,
					uint32_t ovf_val,
					drv_pwm_phase_polarity_t phase,
					uint8_t duty)
{
	uint32_t cmp_val;
	DEBUG_FUNC_PRT
	if (phase == DRV_PWM_PHASE_POLARITY_POS) {
		cmp_val = duty * ((uint64_t)ovf_val + 1) / 100;
	} else {
		cmp_val = (100 - duty) * ((uint64_t)ovf_val + 1) / 100;
	}

	if (chn == DRV_PWM_CHN_A) {
		pwm_dev->cmp0_a_val = cmp_val;
	} else if (chn == DRV_PWM_CHN_B) {
		pwm_dev->cmp0_b_val = cmp_val;
	} else if (chn == DRV_PWM_CHN_C) {
		pwm_dev->cmp0_c_val = cmp_val;
	} else if (chn == DRV_PWM_CHN_D) {
		pwm_dev->cmp0_d_val = cmp_val;
	} else {
		return;
	}

	/* updater cmp register */
	pwm_dev->cmp_val_upt = 1;
	// while(pwm_dev->cmp_val_upt & 0x00000001);
}

#if 0
static void drv_pwm_center_align_duty_set(sdrv_pwm_t *pwm_dev,
					  drv_pwm_chn_t chn, uint32_t ovf_val,
					  drv_pwm_phase_polarity_t phase,
					  uint8_t duty)
{
	uint32_t cmp0_val;
	uint32_t cmp1_val;
	DEBUG_FUNC_PRT

	if (phase == DRV_PWM_PHASE_POLARITY_POS) {
		cmp0_val = (100 - duty) * ((uint64_t)ovf_val + 1) / (2 * 100);
		cmp1_val = (ovf_val + 1) - cmp0_val;
	} else {
		cmp0_val = duty * ((uint64_t)ovf_val + 1) / (2 * 100);
		cmp1_val = (ovf_val + 1) - cmp0_val;
	}
	if (chn == DRV_PWM_CHN_A) {
		pwm_dev->cmp0_a_val = cmp0_val;
		pwm_dev->cmp1_a_val = cmp1_val;
	} else if (chn == DRV_PWM_CHN_B) {
		pwm_dev->cmp0_b_val = cmp0_val;
		pwm_dev->cmp1_b_val = cmp1_val;
	} else if (chn == DRV_PWM_CHN_C) {
		pwm_dev->cmp0_c_val = cmp0_val;
		pwm_dev->cmp1_c_val = cmp1_val;
	} else if (chn == DRV_PWM_CHN_D) {
		pwm_dev->cmp0_d_val = cmp0_val;
		pwm_dev->cmp1_d_val = cmp1_val;
	} else {
		return;
	}

	/* updater cmp register */
	pwm_dev->cmp_val_upt = 1;
	// while(pwm_dev->cmp_val_upt & 0x00000001);
}
#endif

static void drv_pwm_simple_duty_all_set(sdrv_pwm_t *pwm_dev,
					drv_pwm_simple_context_t *ctx,
					drv_pwm_simple_cfg_t *play_cfg)
{
	int32_t i = 0;
	for (; i < DRV_PWM_CHN_TOTAL; i++) {
		// drv_pwm_simple_duty_set(pwm_dev, i, ctx,
		// play_cfg->cmp_cfg[i].duty);
		drv_pwm_edge_align_duty_set(pwm_dev, i, pwm_dev->cnt_g0_ovf,
					    play_cfg->cmp_cfg[i].phase,
					    play_cfg->cmp_cfg[i].duty);
	}
}
void drv_pwm_int_enable(sdrv_pwm_t *pwm_dev, drv_pwm_int_src_t int_src)
{
	uint32_t int_sta_en;
	uint32_t int_sig_en;

	int_sta_en = pwm_dev->int_sta_en;
	int_sig_en = pwm_dev->int_sig_en;

	int_sta_en |= (1 << int_src);
	int_sig_en |= (1 << int_src);

	pwm_dev->int_sta_en = int_sta_en;
	pwm_dev->int_sig_en = int_sig_en;
}

void drv_pwm_int_disable(sdrv_pwm_t *pwm_dev, drv_pwm_int_src_t int_src)
{
	uint32_t int_sta_en;
	uint32_t int_sig_en;

	int_sta_en = pwm_dev->int_sta_en;
	int_sig_en = pwm_dev->int_sig_en;

	int_sta_en &= ~(1 << int_src);
	int_sig_en &= ~(1 << int_src);

	pwm_dev->int_sta_en = int_sta_en;
	pwm_dev->int_sig_en = int_sig_en;
}

void drv_pwm_int_clear_pending(sdrv_pwm_t *pwm_dev, drv_pwm_int_src_t int_src)
{
	uint32_t int_sta;

	int_sta = pwm_dev->int_sta;

	int_sta |= (1 << int_src);

	pwm_dev->int_sta =  int_sta;

}
// static void drv_pwm_simple_freq_set(struct sd_pwm_chip *pc,
static void drv_pwm_simple_freq_set(sdrv_pwm_t *pwm_dev, drv_pwm_chn_t chn,
				    drv_pwm_simple_context_t *ctx,
				    uint64_t freq)
{
	uint64_t ovf_val;
	uint32_t ovf_bits;
	sdrv_pwm_cmp_config_t cmp_config;

	/* Frequency: overflow value */
	ovf_val = (DRV_PWM_HF_CLOCK_FREQ) * 100000ULL;
	ovf_val = ovf_val / (DRV_PWM_SIMPLE_CLOCK_DIV_NUM + 1) / freq;
	if (ovf_val > 0) {
		ovf_val -= 1;
	}
	pwm_dev->cnt_g0_ovf = ovf_val;
	ctx->ovf_val = ovf_val;

	ovf_bits = drv_pwm_none_zero_bits_index(ovf_val) + 1;
	cmp_config.val = pwm_dev->cmp_config.val;
	if (ovf_bits <= 8) {
		cmp_config.data_format = DRV_PWM_CMP_DATA_8BITS;
	} else if (ovf_bits <= 16) {
		cmp_config.data_format = DRV_PWM_CMP_DATA_16BITS;
	} else {
		cmp_config.data_format = DRV_PWM_CMP_DATA_32BITS;
	}
	pwm_dev->cmp_config.val = cmp_config.val;
}

static void drv_pwm_simple_init(struct sd_pwm_chip *pc,
				drv_pwm_simple_cfg_t *pwm_cfg,
				drv_pwm_simple_context_t *pwm_simple_ctx)
{
	uint32_t loop;
	uint32_t ovf_val;
	uint32_t ovf_bits;
	drv_pwm_clk_cfg_t clk_cfg;
	sdrv_pwm_cmp_config_t cmp_config;
	sdrv_pwm_t *pwm_dev = (sdrv_pwm_t *)pc->mmio;
	/* Software reset whole IP */
	drv_pwm_reset(pwm_dev);

	/* Select soucre clock and clock div */
	clk_cfg.clk_src = DRV_PWM_SRC_CLK_HF;
	clk_cfg.clk_div = DRV_PWM_SIMPLE_CLOCK_DIV_NUM;
	drv_pwm_clk_init(pwm_dev, clk_cfg.clk_src, clk_cfg.clk_div);

	/* Single mode/Consecutive mode */
	drv_pwm_single_mode_set(pwm_dev, pwm_cfg->single_mode);

	/* config group number */
	drv_pwm_group_num_set(pwm_dev, pwm_cfg->grp_num);

	/* Frequency: overflow value */
	ovf_val =
	    ((DRV_PWM_HF_CLOCK_FREQ) / (clk_cfg.clk_div + 1)) / (pwm_cfg->freq);
	if (ovf_val > 0) {
		ovf_val -= 1;
	}
	pwm_dev->cnt_g0_ovf = ovf_val;

	/* reload cnount g0 */
	drv_pwm_count_reset(pwm_dev);

	ovf_bits = drv_pwm_none_zero_bits_index(ovf_val) + 1;
	cmp_config.val = pwm_dev->cmp_config.val;
	if (ovf_bits <= 8) {
		cmp_config.data_format = DRV_PWM_CMP_DATA_8BITS;
	} else if (ovf_bits <= 16) {
		cmp_config.data_format = DRV_PWM_CMP_DATA_16BITS;
	} else {
		cmp_config.data_format = DRV_PWM_CMP_DATA_32BITS;
	}
	pwm_dev->cmp_config.val = cmp_config.val;

	/* check the duty range */
	for (loop = 0; loop < DRV_PWM_CHN_TOTAL; loop++) {
		if (pwm_cfg->cmp_cfg[loop].duty > 100) {
			pwm_cfg->cmp_cfg[loop].duty = 100;
		}
	}

	/* save to context */
	pwm_simple_ctx->ovf_val = ovf_val;
	pwm_simple_ctx->align_mode = pwm_cfg->align_mode;

	/* config align mode */
	if (pwm_cfg->align_mode == DRV_PWM_EDGE_ALIGN_MODE) {

		/* single compare register */
		drv_pwm_dual_cmp_mode_set(pwm_dev, false);

		/* compare mode config */
		drv_pwm_edge_align_chn_set(
		    pwm_dev, DRV_PWM_CHN_A,
		    pwm_cfg->cmp_cfg[DRV_PWM_CHN_A].phase);
		/* save to context */
		pwm_simple_ctx->phase[DRV_PWM_CHN_A] =
		    pwm_cfg->cmp_cfg[DRV_PWM_CHN_A].phase;
		/* duty: compare value */
		drv_pwm_edge_align_duty_set(
		    pwm_dev, DRV_PWM_CHN_A, ovf_val,
		    pwm_cfg->cmp_cfg[DRV_PWM_CHN_A].phase,
		    pwm_cfg->cmp_cfg[DRV_PWM_CHN_A].duty);

		if ((pwm_cfg->grp_num == DRV_PWM_CHN_A_B_WORK) ||
		    (pwm_cfg->grp_num == DRV_PWM_CHN_A_B_C_D_WORK)) {
			/* compare mode config */
			drv_pwm_edge_align_chn_set(
			    pwm_dev, DRV_PWM_CHN_B,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_B].phase);
			/* save to context */
			pwm_simple_ctx->phase[DRV_PWM_CHN_B] =
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_B].phase;
			/* duty: compare value */
			drv_pwm_edge_align_duty_set(
			    pwm_dev, DRV_PWM_CHN_B, ovf_val,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_B].phase,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_B].duty);
		}

		if (pwm_cfg->grp_num == DRV_PWM_CHN_A_B_C_D_WORK) {
			/* compare mode config */
			drv_pwm_edge_align_chn_set(
			    pwm_dev, DRV_PWM_CHN_C,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_C].phase);
			drv_pwm_edge_align_chn_set(
			    pwm_dev, DRV_PWM_CHN_D,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_D].phase);
			/* save to context */
			pwm_simple_ctx->phase[DRV_PWM_CHN_C] =
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_C].phase;
			pwm_simple_ctx->phase[DRV_PWM_CHN_D] =
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_D].phase;
			/* duty: compare value */
			drv_pwm_edge_align_duty_set(
			    pwm_dev, DRV_PWM_CHN_C, ovf_val,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_C].phase,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_C].duty);
			drv_pwm_edge_align_duty_set(
			    pwm_dev, DRV_PWM_CHN_D, ovf_val,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_D].phase,
			    pwm_cfg->cmp_cfg[DRV_PWM_CHN_D].duty);
		}
	}
}

static int sd_pwm_init(struct pwm_chip *chip)
{
	struct sd_pwm_chip *pc = to_sd_pwmchip(chip);
	drv_pwm_simple_cfg_t *play_cfg = &pc->play_cfg;

	play_cfg->freq = 1000;
	play_cfg->cmp_cfg[DRV_PWM_CHN_A].duty = 70;
	play_cfg->cmp_cfg[DRV_PWM_CHN_B].duty = 70;
	play_cfg->cmp_cfg[DRV_PWM_CHN_C].duty = 70;
	play_cfg->cmp_cfg[DRV_PWM_CHN_D].duty = 70;
	play_cfg->single_mode = DRV_PWM_CONTINUE_CMP;

	drv_pwm_simple_init(pc, play_cfg, &pc->pwm_ctx);
	return 0;
}

static int sd_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			 int duty_ns, int period_ns)
{
	struct sd_pwm_chip *pc = to_sd_pwmchip(chip);
	drv_pwm_simple_cfg_t *play_cfg = &pc->play_cfg;
	uint64_t new_duty = 0;
	uint64_t new_frep = 0;
	new_duty = duty_ns * 100ULL;
	do_div(new_duty, period_ns);
	new_frep = NSEC_PER_SEC * 100000ULL;
	do_div(new_frep, period_ns);

	dev_info(pc->dev,
		"%s %d: map_base: %#x, pwm->hwpwm: %d, new_duty: %llu, new_frep: "
		"%llu\n",
		__func__, __LINE__, pc->map_base, pwm->hwpwm, new_duty,
		new_frep);

	play_cfg->freq = new_frep;

	play_cfg->cmp_cfg[pwm->hwpwm].duty = new_duty;
	drv_pwm_simple_freq_set((sdrv_pwm_t *)pc->mmio, pwm->hwpwm,
				&pc->pwm_ctx, play_cfg->freq);
	drv_pwm_simple_duty_all_set((sdrv_pwm_t *)pc->mmio, &pc->pwm_ctx,
				    play_cfg);

	return 0;
}

static int sd_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			struct pwm_state *state)
{
	DEBUG_FUNC_PRT

	if (state->polarity != pwm->state.polarity)
		sd_pwm_set_polarity(chip, pwm, state->polarity);

	if (state->period != pwm->state.period ||
	    state->duty_cycle != pwm->state.duty_cycle)
		sd_pwm_config(chip, pwm, state->duty_cycle, state->period);

	if (state->enabled != pwm->state.enabled) {
		if (state->enabled)
			sd_pwm_enable(chip, pwm);
		else
			sd_pwm_disable(chip, pwm);
	}

	return 0;
}

static irqreturn_t sd_pwm_capture_interrupt(int irq, void *data)
{
	struct sd_pwm_chip *pc = data;
	int ret = IRQ_NONE;
	uint32_t value = 0;
	regmap_read(pc->regmap, INT_STA_OFF, &value);
	if (value >> 23 & 0x01) {
		int_count++;
		regmap_field_write(pc->tim_cpt_d_en, 0x0);
	}

	if (value >> 22 & 0x01) {
		int_count++;
		regmap_field_write(pc->tim_cpt_c_en, 0x0);
	}

	if (int_count == 2) {
		regmap_field_write(pc->tim_sta_en, 0x0);
		regmap_field_write(pc->tim_sig_en, 0x0);
	}
	return ret;
}

static irqreturn_t sd_pwm_rise_edge_interrupt(int irq, void *data)
{
	struct sd_pwm_chip *pc = (struct sd_pwm_chip *)data;

	drv_pwm_int_clear_pending(pc->mmio, DRV_PWM_INT_SRC_CNT_G0_OVF);

	return IRQ_HANDLED;
}

static int sd_pwd_cpt_ret(uint32_t *rise, uint32_t rise_len, uint32_t *fall,
			  uint32_t fall_len, struct sd_pwm_chip *pc,
			  struct pwm_capture *result)
{
	int ret = 0;
	int i;
	int j = 0;
	uint32_t cycle = 0;
	uint32_t high = 0;

	if (!rise || !fall || !result || rise_len <= 0 || fall_len <= 0)
		return -EINVAL;

	i = rise_len / 2;

	for (j = 0; j < fall_len; j++) {
		if (fall[j] > rise[i - 1] && fall[j] < rise[i])
			break;
	}

	if (j == fall_len){
		result->period = 0;
		result->duty_cycle = 0;

		return ret;
	}

	high = fall[j] - rise[i - 1];
	cycle = rise[i] - rise[i - 1];
	dev_info(pc->dev, "high:%#08x, cycle:%#08x \n", high, cycle);
	result->period = (pc->cpt_rate + (cycle >> 1)) / cycle;
	result->duty_cycle = (high * 100 + (cycle >> 1)) / cycle;

	return ret;
}
/*
 * Capture using PWM input mode:
 *                              ___          ___          ___
 * TI[1, 2, 3 or 4]: ........._|   |________|   |________|   |___........
 *                             ^0  ^1       ^2  ^3       ^4  ^5  ........
 *                              .   .        .   .       .  XXXXX........
 *                              .   .        .   .      XXXXX .   .    .
 *                              .   .        .  XXXXX    .    .   .    .
 *                              .   .      XXXXX .       .    .   .    .
 *                              .  XXXXX     .   .       .    .   .    .
 * COUNTER G0:    ____________XXXXX .        .   .       .    .   .    ._XXX
 *                        start^.   .        .   .       .    .    ^stop
 *                              .   .        .   .       .    .
 *                              v   .        v   .       v    .
 *                                  v            v            v
 * Chann D  :       tx..........t0...........t2..........t4......t(n) rise_data
 * Chann C  :       tx..............t1...........t3..........t5..t(n) fall_data
 *
 * 0: Chann D snapchot on rising edge: counter value save in FIFO D.
 * 1: Chann C snapchot on falling edge: counter value save in FIFO C.
 * 2: There are 16 storage spaces in fifo c/d,
 * 	  once the storage is full,
 *    stop storage and report interruption.
 * 3: Read FIFO C/D to rise_data and fall_data array.
 * 4: Calculation freq and duty cycle
 *
 */
static int sd_pwm_capture(struct pwm_chip *chip, struct pwm_device *pwm,
			  struct pwm_capture *result, unsigned long timeout)
{
	struct sd_pwm_chip *pc = to_sd_pwmchip(chip);
	struct sd_cpt_channel *ddata = pwm_get_chip_data(pwm);
	struct device *dev = pc->dev;
	timer_drv_func_cpt_t cpt_cfg;

	uint32_t rise_data[16];
	uint32_t fall_data[16];
	uint32_t clear_fifo_limit = 2;

	int ret = 0;
	int i;
	uint32_t value = 0;
	uint32_t fifo_sta = 0;
	DEBUG_FUNC_PRT

	mutex_lock(&ddata->lock);
	if (pc->direct != PWM_CAPTURE){
		result->period = 0;
		result->duty_cycle = 0;
		goto exit;
	}
	REG_DUMP(pc, INT_STA_OFF, value);
	REG_DUMP(pc, FIFO_STA_OFF, value);

	/* PWM Capture support 24000000 or 400000000*/
	pc->cpt_rate = 24000000;

	ret = clk_set_rate(pc->capt_clk, pc->cpt_rate);
	if (ret) {
		dev_err(dev, "failed to clock set rate\n");
		return ret;
	}

	dev_info(dev, "clock:%ld\n", clk_get_rate(pc->capt_clk));

	/* Prepare capture measurement */
	timer_drv_clk_init(pc, TIMER_DRV_SEL_LF_CLK, CPT_TMR_DIV);

	/* cascade set, G1 cascaded G0 or not. */
	regmap_field_write(pc->tim_cnt_config, 1);

	regmap_write(pc->regmap, CNT_LOCAL_C_OVF_OFF,
		     FM_CNT_LOCAL_C_OVF_OVF_VAL);
	regmap_write(pc->regmap, CNT_LOCAL_D_OVF_OFF,
		     FM_CNT_LOCAL_D_OVF_OVF_VAL);

	regmap_write(pc->regmap, CNT_LOCAL_C_INIT_OFF, 0);
	regmap_write(pc->regmap, CNT_LOCAL_D_INIT_OFF, 0);

	regmap_write(pc->regmap, CNT_G0_INIT_OFF,
		     FM_CNT_G0_INIT_VALUE); // 0xffffffff
	/* Enale DMA */
	timer_drv_func_dma_init(pc, TIMER_DRV_DMA_SEL_CPT, 1);

	/* Config capture */
	cpt_cfg.cpt_cnt_sel = TIMER_DRV_CPT_CNT_G0;
	cpt_cfg.trig_mode = TIMER_DRV_CPT_RISING_EDGE;
	cpt_cfg.dual_cpt_mode = 0;
	cpt_cfg.single_mode = 0;
	cpt_cfg.filter_dis = 1;
	cpt_cfg.filter_width = 0;
	cpt_cfg.first_cpt_rst_en = 0;
	timer_drv_func_cpt_set(pc, &cpt_cfg); // cpt chann set

	/*	Disable compare*/
	timer_drv_func_disable_cmp(pc);

	regmap_field_write(pc->tim_sta_en, 0x3);
	regmap_field_write(pc->tim_sig_en, 0x3);
	/* Enable capture*/
	regmap_field_write(pc->tim_cpt_d_en, 0x1);
	regmap_field_write(pc->tim_cpt_c_en, 0x1);

	wait_event_interruptible_timeout(ddata->wait, int_count > 1,
					 msecs_to_jiffies(timeout));

	int_count = 0;
	regmap_field_write(pc->tim_cpt_d_en, 0x0);
	regmap_field_write(pc->tim_cpt_c_en, 0x0);

	regmap_read(pc->regmap, FIFO_STA_OFF, &fifo_sta);

	for (i = 0; i < ((fifo_sta >> (8u * 3 /*D*/ + 2u)) & 0x1fu); i++) {
		regmap_read(pc->regmap, FIFO_D_OFF, &value);
		rise_data[i] = value;
		dev_info(dev, "FIFO_D_OFF: %#x\n", rise_data[i]);
	}
	for (i = 0; i < ((fifo_sta >> (8u * 2 /*C*/ + 2u)) & 0x1fu); i++) {
		regmap_read(pc->regmap, FIFO_C_OFF, &value);
		fall_data[i] = value;
		dev_info(dev, "FIFO_C_OFF: %#x\n", fall_data[i]);
	}

	while (clear_fifo_limit) {
		regmap_read(
		    pc->regmap, FIFO_STA_OFF,
		    &fifo_sta); /*Drop do not needed data, for next fifo read*/
		if (((fifo_sta >> (8u * 3 /*D*/ + 2u)) & 0x1fu) == 0 &&
		    ((fifo_sta >> (8u * 2 /*C*/ + 2u)) & 0x1fu) == 0) {
			break;
		} else {
			for (i = 0;
			     i < ((fifo_sta >> (8u * 2 /*C*/ + 2u)) & 0x1fu);
			     i++) {
				regmap_read(pc->regmap, FIFO_C_OFF, &value);
				dev_info(dev, "Drop C: %#x\n", value);
			}
			for (i = 0;
			     i < ((fifo_sta >> (8u * 3 /*D*/ + 2u)) & 0x1fu);
			     i++) {
				regmap_read(pc->regmap, FIFO_D_OFF, &value);
				dev_info(dev, "Drop D: %#x\n", value);
			}
			/*
			Because of Linux scheduling,
			a lot of data is written into fifo when closing the
			register. So, when clear no need data will close reg
			again to workaround this case.
			*/
			regmap_field_write(pc->tim_cpt_c_en, 0x0);
			regmap_field_write(pc->tim_cpt_d_en, 0x0);
			regmap_field_write(pc->tim_sta_en, 0x0);
			regmap_field_write(pc->tim_sig_en, 0x0);
		}
		clear_fifo_limit--;
	}
	if (!clear_fifo_limit) {
		regmap_read(pc->regmap, FIFO_STA_OFF, &fifo_sta);
		dev_err(pc->dev, "FIFO no been clear, fifo_state:%#8x\n",
			fifo_sta);
	}
	/* clear intterrupt flag */
	regmap_read(pc->regmap, INT_STA_OFF, &value);
	regmap_write(pc->regmap, INT_STA_OFF, value);

	/*Calculation results*/
	ret = sd_pwd_cpt_ret(rise_data, sizeof(rise_data) / sizeof(uint32_t),
			     fall_data, sizeof(fall_data) / sizeof(uint32_t),
			     pc, result);

exit:
	mutex_unlock(&ddata->lock);
	return ret;
}
static int sd_pwm_capture_probe_dt(struct sd_pwm_chip *pc)
{
	struct device *dev = pc->dev;
	const struct reg_field *reg_fields;
	struct device_node *np = dev->of_node;
	struct sd_pwm_compat_data *cdata = pc->cdata;
	u32 num_devs, op_mode;
	int ret = 0;
	DEBUG_FUNC_PRT
	ret = of_property_read_u32(np, "op-mode", &op_mode);
	if (ret < 0) {
		dev_err(dev, "No op-mode optinos configed in dtsi\n");
		return ret;
	}
	if (OP_MODE_PWM != op_mode) {
		dev_err(dev, "op-mode(%d) options unsupport\n", op_mode);
		return -EPERM;
	}
	ret = of_property_read_u32(np, "sd,capture-num-chan", &num_devs);
	if (!ret)
		cdata->cpt_num_devs = num_devs;

	pc->capt_clk = devm_clk_get(dev, "timer");
	if (IS_ERR(pc->capt_clk)) {
		dev_err(dev, "failed to get PWM capture clock\n");
		return PTR_ERR(pc->capt_clk);
	}

	reg_fields = cdata->reg_fields;

	pc->tim_cpt_int_stat = devm_regmap_field_alloc(
	    dev, pc->regmap, reg_fields[TIM_CPT_INT_STAT]);
	if (IS_ERR(pc->tim_cpt_int_stat))
		return PTR_ERR(pc->tim_cpt_int_stat);

	pc->tim_cpt_c_en =
	    devm_regmap_field_alloc(dev, pc->regmap, reg_fields[TIM_CPT_C_EN]);
	if (IS_ERR(pc->tim_cpt_c_en))
		return PTR_ERR(pc->tim_cpt_c_en);

	pc->tim_cpt_d_en =
	    devm_regmap_field_alloc(dev, pc->regmap, reg_fields[TIM_CPT_D_EN]);
	if (IS_ERR(pc->tim_cpt_d_en))
		return PTR_ERR(pc->tim_cpt_d_en);

	pc->tim_clk_config = devm_regmap_field_alloc(
	    dev, pc->regmap, reg_fields[TIM_CLK_CONFIG]);
	if (IS_ERR(pc->tim_clk_config))
		return PTR_ERR(pc->tim_clk_config);

	pc->tim_cnt_config = devm_regmap_field_alloc(
	    dev, pc->regmap, reg_fields[TIM_CNT_CONFIG]);
	if (IS_ERR(pc->tim_cnt_config))
		return PTR_ERR(pc->tim_cnt_config);

	pc->tim_cnt_g0_config = devm_regmap_field_alloc(
	    dev, pc->regmap, reg_fields[TIM_CNT_G0_CONFIG]);
	if (IS_ERR(pc->tim_cnt_g0_config))
		return PTR_ERR(pc->tim_cnt_g0_config);

	pc->tim_sta_en =
	    devm_regmap_field_alloc(dev, pc->regmap, reg_fields[TIM_STA_EN]);
	if (IS_ERR(pc->tim_sta_en))
		return PTR_ERR(pc->tim_sta_en);

	pc->tim_sig_en =
	    devm_regmap_field_alloc(dev, pc->regmap, reg_fields[TIM_SIG_EN]);
	if (IS_ERR(pc->tim_sig_en))
		return PTR_ERR(pc->tim_sig_en);
	pc->direct = PWM_CAPTURE;
	return ret;
}
static int sd_pwm_playback_probe_dt(struct sd_pwm_chip *pc)
{
	struct device *dev = pc->dev;
	struct device_node *np = dev->of_node;
	struct sd_pwm_compat_data *cdata = pc->cdata;
	drv_pwm_simple_cfg_t *play_cfg = &pc->play_cfg;
	u32 num_devs;
	int ret = 0;
	DEBUG_FUNC_PRT

	ret = of_property_read_u32(np, "sd,playback-num-chan", &num_devs);
	if (!ret)
		cdata->play_num_devs = num_devs;
	else
		cdata->play_num_devs = 2;

	switch (cdata->play_num_devs) {
	case 1:
		play_cfg->grp_num = DRV_PWM_CHN_A_WORK;
		break;
	case 2:
		play_cfg->grp_num = DRV_PWM_CHN_A_B_WORK;
		break;
	case 4:
		play_cfg->grp_num = DRV_PWM_CHN_A_B_C_D_WORK;
		break;
	default:
		pr_err(
		    "unsupport playback num channels, set default work mode\n");
		play_cfg->grp_num = DRV_PWM_CHN_A_B_WORK;
		break;
	}

	if (of_property_read_u32(np, "sd,enable_interrupt", &pc->enable_interrupt))
		pc->enable_interrupt = 0;

	pc->direct = PWM_PLAYBACK;

	return ret;
}

static bool sd_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}
static const struct regmap_config sd_timer_regmap_config = {
    .reg_bits = 32,
    .val_bits = 32,
    .reg_stride = 4,
    .volatile_reg = sd_volatile_reg,
    .max_register = CMP_D_CONFIG_OFF,
};
static const struct regmap_config sd_pwm_regmap_config = {
    .reg_bits = 32,
    .val_bits = 32,
    .reg_stride = 4,
    .volatile_reg = sd_volatile_reg,
    .max_register = 0x90,
};
static const struct pwm_ops sd_pwm_ops = {
    .capture = sd_pwm_capture,
	.apply = sd_pwm_apply,
    .owner = THIS_MODULE,
};
static int sd_pwm_probe_capture(struct platform_device *pdev) //timer ip
{
	struct device *dev = &pdev->dev;
	struct sd_pwm_compat_data *cdata;
	struct sd_pwm_chip *pc;
	struct resource *res;
	int i, irq, ret;
	DEBUG_FUNC_PRT
	pc = devm_kzalloc(dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	cdata = devm_kzalloc(dev, sizeof(*cdata), GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -1;
	pc->map_base = res->start;

	pc->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(pc->mmio))
		return PTR_ERR(pc->mmio);
	// pc->mmio = of_iomap(np, 0);
	pc->regmap =
	    devm_regmap_init_mmio(dev, pc->mmio, &sd_timer_regmap_config);
	if (IS_ERR(pc->regmap))
		return PTR_ERR(pc->regmap);

	/*
	 * Setup PWM data with default values: some values could be replaced
	 * with specific ones provided from Device Tree.
	 */
	cdata->reg_fields = sd_pwm_regfields;
	cdata->cpt_num_devs = 0;
	pc->cdata = cdata;
	pc->dev = dev;

	mutex_init(&pc->sd_pwm_lock);

	ret = sd_pwm_capture_probe_dt(pc);
	if (ret) {
		dev_err(dev, "failed to probe device tree\n");
		return ret;
	}
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to obtain IRQ\n");
		return irq;
	}

	ret = devm_request_irq(&pdev->dev, irq, sd_pwm_capture_interrupt, 0, pdev->name,
			       pc);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		return ret;
	}

	pc->chip.dev = dev;
	pc->chip.ops = &sd_pwm_ops;
	pc->chip.base = -1;
	pc->chip.npwm = pc->cdata->cpt_num_devs;
	ret = pwmchip_add(&pc->chip);
	if (ret < 0) {
		dev_err(dev, "pwmchip_add error!\n");
		return ret;
	}
	dev_info(dev, "cdata->cpt_num_devs: %d", cdata->cpt_num_devs);
	for (i = 0; i < cdata->cpt_num_devs; i++) {
		struct sd_cpt_channel *ddata;

		ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
		if (!ddata)
			return -ENOMEM;

		init_waitqueue_head(&ddata->wait);
		mutex_init(&ddata->lock);

		pwm_set_chip_data(&pc->chip.pwms[i], ddata);
	}

	platform_set_drvdata(pdev, pc);

	return ret;
}

static int sd_pwm_probe_palyback(struct platform_device *pdev) // pwm ip
{
	struct device *dev = &pdev->dev;
	struct sd_pwm_compat_data *cdata;
	struct sd_pwm_chip *pc;
	struct resource *res;
	int i, irq, ret;
	DEBUG_FUNC_PRT

	pc = devm_kzalloc(dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	cdata = devm_kzalloc(dev, sizeof(*cdata), GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -1;
	pc->map_base = res->start;

	pc->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(pc->mmio))
		return PTR_ERR(pc->mmio);

	pc->regmap =
	    devm_regmap_init_mmio(dev, pc->mmio, &sd_pwm_regmap_config);
	if (IS_ERR(pc->regmap))
		return PTR_ERR(pc->regmap);

	pc->plby_clk = devm_clk_get(dev, "plbkclk");
	if (IS_ERR(pc->plby_clk)) {
			dev_err(dev, "pc->plby_clk error \n");
			return PTR_ERR(pc->plby_clk);
	}

	clk_set_rate(pc->plby_clk, DRV_PWM_HF_CLOCK_FREQ);

	pc->cdata = cdata;
	pc->dev = dev;

	mutex_init(&pc->sd_pwm_lock);

	ret = sd_pwm_playback_probe_dt(pc);
	if (ret) {
		dev_err(dev, "failed to probe device tree\n");
		return ret;
	}

	if (pc->enable_interrupt) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0) {
			dev_err(&pdev->dev, "Failed to obtain IRQ\n");
			return irq;
		}

		ret = devm_request_irq(&pdev->dev, irq, sd_pwm_rise_edge_interrupt, 0,
				       pdev->name, pc);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request IRQ\n");
			return ret;
		}
	}

	pc->chip.dev = dev;
	pc->chip.ops = &sd_pwm_ops;
	pc->chip.base = -1;
	pc->chip.npwm = pc->cdata->play_num_devs;
	pc->chip.of_xlate = of_pwm_xlate_with_flags;
	pc->chip.of_pwm_n_cells = 3;
	ret = pwmchip_add(&pc->chip);
	if (ret < 0) {
		dev_err(dev, "pwmchip_add error!\n");
		return ret;
	}
	dev_info(dev, "cdata->play_num_devs: %d", cdata->play_num_devs);
	for (i = 0; i < cdata->play_num_devs; i++) {
		struct sd_play_channel *channel;

		channel = devm_kzalloc(dev, sizeof(*channel), GFP_KERNEL);
		if (!channel)
			return -ENOMEM;
		channel->index = i;
		init_waitqueue_head(&channel->wait);
		mutex_init(&channel->lock);

		pwm_set_chip_data(&pc->chip.pwms[i], channel);
	}

	platform_set_drvdata(pdev, pc);
	sd_pwm_init(&pc->chip);

	if (pc->enable_interrupt)
		drv_pwm_int_enable((sdrv_pwm_t *)pc->mmio, DRV_PWM_INT_SRC_CNT_G0_OVF);

	return ret;
}

static int sd_pwm_probe(struct platform_device *pdev) // main probe
{
	struct device *dev = &pdev->dev;
	const struct sd_pwm_ip *pwm_ip = of_device_get_match_data(&pdev->dev);
	int32_t ret = 0;
	dev_info(dev, "%s, %d: direction: %d", __func__, __LINE__,
		pwm_ip->direction);
	switch (pwm_ip->direction) {
	case PWM_CAPTURE:
		ret = sd_pwm_probe_capture(pdev);
		break;
	case PWM_PLAYBACK:
		ret = sd_pwm_probe_palyback(pdev);
		break;
	}
	return ret;
}
static int sd_pwm_remove(struct platform_device *pdev)
{
	struct sd_pwm_chip *pc = platform_get_drvdata(pdev);
	unsigned int i;
	DEBUG_FUNC_PRT

	regmap_exit(pc->regmap);

	if (pc->enable_interrupt)
		drv_pwm_int_disable((sdrv_pwm_t *)pc->mmio, DRV_PWM_INT_SRC_CNT_G0_OVF);

	for (i = 0; i < pc->cdata->cpt_num_devs; i++)
		pwm_disable(&pc->chip.pwms[i]);

	return pwmchip_remove(&pc->chip);
}

static const struct of_device_id sd_pwm_of_match[] = {
    {
	.compatible = "sd,sd-pwm-capture", .data = &pwm_ip[0],
    },
    {
	.compatible = "sd,sd-pwm-playback", .data = &pwm_ip[1],
    },
    {/* sentinel */}};
MODULE_DEVICE_TABLE(of, sd_pwm_of_match);

static struct platform_driver sd_pwm_driver = {
    .driver =
	{
	    .name = "sd-pwm",
	    .of_match_table = sd_pwm_of_match,
	},
    .probe = sd_pwm_probe,
    .remove = sd_pwm_remove,
};
module_platform_driver(sd_pwm_driver);

MODULE_AUTHOR("mengmeng.chang <mengmeng.chang@semidrive.com>");
MODULE_DESCRIPTION("SemiDrive SD PWM driver");
MODULE_LICENSE("GPL");
