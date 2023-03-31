/*
 * SemiDrive X9 ADC driver
 *
 * Copyright (C) 2021 SemiDrive, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/iio/iio.h>
#include <linux/iio/driver.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "sdrv_adc_reg.h"


#define SDRV_REG_ADC_CLK_CFG_PRE_DIV_4			((u32)((4-1) << ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET))
#define SDRV_REG_ADC_CLK_CFG_PRE_DIV_8			((u32)((8-1) << ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET))
#define SDRV_REG_ADC_CLK_CFG_PRE_DIV_16			((u32)((16-1) << ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET))
#define SDRV_REG_ADC_CLK_CFG_PRE_DIV_32			((u32)((32-1) << ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET))
#define SDRV_REG_ADC_CLK_CFG_PRE_DIV_64			((u32)((64-1) << ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET))
#define SDRV_REG_ADC_CLK_CFG_PRE_DIV_128		((u32)((128-1) << ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET))
#define SDRV_ADC_TIMEOUT		msecs_to_jiffies(100)

#define IS_ENABLE_FIFO 1

typedef enum
{
	ADC_SINGLE_CH_SINGLE_E0 = 0x00,
	ADC_SINGLE_CH_BACK2BACK_E1,
	ADC_SINGLE_CH_INTERVAL_E2,
	ADC_MULTIPLE_CH_E3,
} adc_convert_mode_t;

typedef enum
{
	ADC_EOC_INT_FLAG_E1 = 0x01,
	ADC_WML_INT_FLAG_E2 = 0x02,
	ADC_CTC_INT_FLAG_E4 = 0x04,
	ADC_EOL_INT_FLAG_E8 = 0x08,
	ADC_OVF_INT_FLAG_E16 = 0x10,
	ADC_DUMMY_INT_FLAG_E32 = 0x20,
} adc_int_flag_e_t;

enum sdrv_adc_clk_pre_div {
	SDRV_ADC_ANALOG_CLK_PRE_DIV_4,
	SDRV_ADC_ANALOG_CLK_PRE_DIV_8,
	SDRV_ADC_ANALOG_CLK_PRE_DIV_16,
	SDRV_ADC_ANALOG_CLK_PRE_DIV_32,
	SDRV_ADC_ANALOG_CLK_PRE_DIV_64,
	SDRV_ADC_ANALOG_CLK_PRE_DIV_128,
};

enum sdrv_adc_average_num {
	SDRV_ADC_AVERAGE_NUM_4,
	SDRV_ADC_AVERAGE_NUM_8,
	SDRV_ADC_AVERAGE_NUM_16,
	SDRV_ADC_AVERAGE_NUM_32,
};

struct sdrv_adc_feature {
	enum sdrv_adc_clk_pre_div clk_pre_div;

	u32 core_time_unit;	/* impact the sample rate */
};

struct sdrv_adc {
	struct device *dev;
	void __iomem *regs;
	struct clk *clk;
	bool use_adc_fifo;
	bool adc_clk_disable;
	int irq;
	spinlock_t lock;

	u32 resolution;
	u32 clksrc_sel;
	u32 vref_uv;
	u32 value;
	u32 channel;
	u32 channel_num;
	u32 pre_div_num;
	u32 clk_rate;

	struct regulator *vref;
	struct sdrv_adc_feature adc_feature;

	struct completion completion;
};

struct sdrv_adc_analogue_core_clk {
	u32 pre_div;
	u32 reg_config;
};

#define SDRV_ADC_ANALOGUE_CLK_CONFIG(_pre_div, _reg_conf) {	\
	.pre_div = (_pre_div),					\
	.reg_config = (_reg_conf),				\
}

static const struct sdrv_adc_analogue_core_clk sdrv_adc_analogue_clk[] = {
	SDRV_ADC_ANALOGUE_CLK_CONFIG(4, SDRV_REG_ADC_CLK_CFG_PRE_DIV_4),
	SDRV_ADC_ANALOGUE_CLK_CONFIG(8, SDRV_REG_ADC_CLK_CFG_PRE_DIV_8),
	SDRV_ADC_ANALOGUE_CLK_CONFIG(16, SDRV_REG_ADC_CLK_CFG_PRE_DIV_16),
	SDRV_ADC_ANALOGUE_CLK_CONFIG(32, SDRV_REG_ADC_CLK_CFG_PRE_DIV_32),
	SDRV_ADC_ANALOGUE_CLK_CONFIG(64, SDRV_REG_ADC_CLK_CFG_PRE_DIV_64),
	SDRV_ADC_ANALOGUE_CLK_CONFIG(128, SDRV_REG_ADC_CLK_CFG_PRE_DIV_128),
};

#define SDRV_ADC_CHAN(_idx) {					\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.channel = (_idx),					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |	\
	BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
}

static const struct iio_chan_spec sdrv_adc_iio_channels[] = {
	SDRV_ADC_CHAN(0),
	SDRV_ADC_CHAN(1),
	SDRV_ADC_CHAN(2),
	SDRV_ADC_CHAN(3),
	SDRV_ADC_CHAN(4),
	SDRV_ADC_CHAN(5),
	SDRV_ADC_CHAN(6),
	SDRV_ADC_CHAN(7),
};

static void sdrv_adc_feature_config(struct sdrv_adc *info)
{
	info->adc_feature.clk_pre_div = SDRV_ADC_ANALOG_CLK_PRE_DIV_8;
	if (info->resolution == 0x00) {
		info->adc_feature.core_time_unit = 8;
	} else if (info->resolution == 0x01) {
		info->adc_feature.core_time_unit = 10;
	} else if (info->resolution == 0x02) {
		info->adc_feature.core_time_unit = 12;
	} else if (info->resolution == 0x03) {
		info->adc_feature.core_time_unit = 14;
	} else {
		info->adc_feature.core_time_unit = 8;
	}
}

/*
 * adc SELRES bits set
 * ADC resolution set
 */
static void sdrv_adc_set_resolution_bits(struct sdrv_adc *info)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	write_v &= ~(u32)(0x3 << ADC_CTRL_SELRES_FIELD_OFFSET);
	write_v |= (u32)(info->resolution << ADC_CTRL_SELRES_FIELD_OFFSET);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc clk src select bits set
 */
static void sdrv_adc_set_clk_src_bits(struct sdrv_adc *info)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CLK_CFG);
	write_v &= ~(u32)(0x3 << ADC_CLK_CFG_SRC_SEL_FIELD_OFFSET);
	write_v |= (u32)(info->clksrc_sel << ADC_CLK_CFG_SRC_SEL_FIELD_OFFSET);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CLK_CFG);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc SELBG bit set to 1 or 0
 */
static void sdrv_adc_set_SELBG_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_CTRL_SELBG) : (write_v &= ~BIT_AP_APB_ADC_ADC_CTRL_SELBG);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc SELREF bit set to 1 or 0
 */
static void sdrv_adc_set_SELREF_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_CTRL_SELREF) : (write_v &= ~BIT_AP_APB_ADC_ADC_CTRL_SELREF);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}


/*
 * adc ctrl reset bit set to 1 or 0
 * function:ctrl reset
 */
static void sdrv_adc_set_rst_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_CTRL_SW_RST) : (write_v &= ~BIT_AP_APB_ADC_ADC_CTRL_SW_RST);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * reset adc ctrl
 */
static void sdrv_adc_reset(struct sdrv_adc *info)
{
	sdrv_adc_set_rst_bit(info, true);
	sdrv_adc_set_rst_bit(info, false);
}

/*
 * analog IP SW reset bit set to 1 or 0
 * function:ctrl rst analog ip
 */
static void sdrv_adc_set_dwc_rst_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_CTRL_DWC_SW_RST) : (write_v &= ~BIT_AP_APB_ADC_ADC_CTRL_DWC_SW_RST);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc differential mode bit set to 1 or 0
 */
static void sdrv_adc_set_delta_mode_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_CTRL_DIFF_EN) : (write_v &= ~BIT_AP_APB_ADC_ADC_CTRL_DIFF_EN);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc multiple ch repeat set
 */
static void sdrv_adc_set_repeat(struct sdrv_adc *info, u32 repeat)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_MCHC);
	write_v &= ~(u32)(0xf << ADC_MCHC_REPEAT_FIELD_OFFSET);
	write_v |= (u32)((repeat & 0xf) << ADC_MCHC_REPEAT_FIELD_OFFSET);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_MCHC);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc enable bit set to 1 or 0
 * should be set to 1 allround the adc job period
 */
static void sdrv_adc_set_en_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_CTRL_ENADC) : (write_v &= ~BIT_AP_APB_ADC_ADC_CTRL_ENADC);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc converting mode set
 * convert_mode is an enum element
 */
static void sdrv_adc_set_convert_mode(struct sdrv_adc *info, adc_convert_mode_t convert_mode)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_SSC);
	write_v &= ~(u32)(0xff << ADC_SSC_CONV_MODE_FIELD_OFFSET);
	write_v |= (u32)(convert_mode << ADC_SSC_CONV_MODE_FIELD_OFFSET);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_SSC);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc converting interval set
 */
static void sdrv_adc_set_interval(struct sdrv_adc *info, u8 value)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_SSC);
	write_v &= ~(u32)(0xff << ADC_SSC_INTERVAL_FIELD_OFFSET);
	write_v |= (u32)(value << ADC_SSC_INTERVAL_FIELD_OFFSET);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_SSC);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc start converting
 */
static void sdrv_adc_start_convert(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_SSC);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_SSC_CONV_START) : (write_v &= ~BIT_AP_APB_ADC_ADC_SSC_CONV_START);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_SSC);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc stop continuous convert
 */
static void sdrv_adc_stop_continuous(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_SSC);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_SSC_CONV_STOP) : (write_v &= ~BIT_AP_APB_ADC_ADC_SSC_CONV_STOP);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_SSC);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc init conversion
 * use this init before really convert to make sure the first convert is right
 */
static void sdrv_adc_init_convert(struct sdrv_adc *info)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_SSC);
	write_v |= BIT_AP_APB_ADC_ADC_SSC_CONV_INIT;
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_SSC);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc timer over int mask bit set to 1 or 0
 */
static void sdrv_adc_set_tmr_int_mask_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_IMASK);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_IMASK_TMR_OVF) : (write_v &= ~BIT_AP_APB_ADC_ADC_IMASK_TMR_OVF);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_IMASK);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc end of loop int mask bit set to 1 or 0
 */
static void sdrv_adc_set_EOL_int_mask_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_IMASK);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_IMASK_EOL) : (write_v &= ~BIT_AP_APB_ADC_ADC_IMASK_EOL);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_IMASK);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc CTC int mask bit set to 1 or 0
 */
static void sdrv_adc_set_CTC_int_mask_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_IMASK);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_IMASK_CTC) : (write_v &= ~BIT_AP_APB_ADC_ADC_IMASK_CTC);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_IMASK);
	spin_unlock_irqrestore(&info->lock, flags);
}
/*
 * adc fifo int mask bit set to 1 or 0
 */
static void sdrv_adc_set_fifo_int_mask_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_IMASK);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_IMASK_WML) : (write_v &= ~BIT_AP_APB_ADC_ADC_IMASK_WML);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_IMASK);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc conversion end int mask bit set to 1 or 0
 */
static void sdrv_adc_set_conv_end_int_mask_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_IMASK);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_IMASK_EOC) : (write_v &= ~BIT_AP_APB_ADC_ADC_IMASK_EOC);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_IMASK);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc timertamp en bit set to 1 or 0
 * start timer
 */
static void sdrv_adc_set_timertamp_en_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_TIMER);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_TIMER_TS_EN) : (write_v &= ~BIT_AP_APB_ADC_ADC_TIMER_TS_EN);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_TIMER);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc get fifo WML
 * WML is the num of data avalible
 */
static u32 sdrv_adc_get_fifo_WML(struct sdrv_adc *info)
{
	return (readl(info->regs + REG_AP_APB_ADC_ADC_FIFO) & 0x7f);
}

/*
 * adc fifo threshold for interrupt
 */
static void sdrv_adc_set_fifo_threashold_int(struct sdrv_adc *info, u32 threshold)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_FIFO);
	write_v &= ~(u32)(0xff << ADC_FIFO_THRE_FIELD_OFFSET);
	write_v |= (u32)((threshold & 0xff) << ADC_FIFO_THRE_FIELD_OFFSET);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_FIFO);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc fifo en set to 1 or 0
 */
static void sdrv_adc_set_fifo_en_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_FIFO);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_FIFO_ENABLE) : (write_v &= ~BIT_AP_APB_ADC_ADC_FIFO_ENABLE);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_FIFO);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc flush fifo
 */
static void sdrv_adc_flush_fifo(struct sdrv_adc *info)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_FIFO);
	write_v |= BIT_AP_APB_ADC_ADC_FIFO_FLUSH;
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_FIFO);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc dma single en set to 1 or 0
 */
static void sdrv_adc_set_dma_single_en_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_DMA_CFG);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_DMA_CFG_SINGLE_EN) : (write_v &= ~BIT_AP_APB_ADC_ADC_DMA_CFG_SINGLE_EN);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_DMA_CFG);
	spin_unlock_irqrestore(&info->lock, flags);
}

/*
 * adc dma en set to 1 or 0
 */
static void sdrv_adc_set_dma_en_bit(struct sdrv_adc *info, bool is_to_1)
{
	unsigned long flags;
	u32 write_v = 0;

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_DMA_CFG);
	is_to_1 ? (write_v |= BIT_AP_APB_ADC_ADC_DMA_CFG_DMA_EN) : (write_v &= ~BIT_AP_APB_ADC_ADC_DMA_CFG_DMA_EN);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_DMA_CFG);
	spin_unlock_irqrestore(&info->lock, flags);
}

static void sdrv_adc_power_down(struct sdrv_adc *info)
{
	u32 adc_cfg;
	unsigned long flags;

	spin_lock_irqsave(&info->lock, flags);
	adc_cfg = readl(info->regs + REG_AP_APB_ADC_ADC_CTRL);
	adc_cfg |= BIT_AP_APB_ADC_ADC_CTRL_POWERDOWN;
	adc_cfg &= ~BIT_AP_APB_ADC_ADC_CTRL_ENADC;
	writel(adc_cfg, info->regs + REG_AP_APB_ADC_ADC_CTRL);
	spin_unlock_irqrestore(&info->lock, flags);
}

static int sdrv_adc_read_data(struct sdrv_adc *info)
{
	u32 value;

	value = readl(info->regs + REG_AP_APB_ADC_ADC_RSLT);
	return value;
}

static void sdrv_adc_sample_rate_set(struct sdrv_adc *info)
{
	struct sdrv_adc_feature *adc_feature = &info->adc_feature;
	struct sdrv_adc_analogue_core_clk adc_analogure_clk;
	u32 sample_rate = 0;
	unsigned long flags;

	sdrv_adc_set_clk_src_bits(info);

	spin_lock_irqsave(&info->lock, flags);
	sample_rate = readl(info->regs + REG_AP_APB_ADC_ADC_CLK_CFG);
	sample_rate &= ~(u32)(0xff << ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET);
	adc_analogure_clk = sdrv_adc_analogue_clk[adc_feature->clk_pre_div];
	sample_rate |= adc_analogure_clk.reg_config;
	info->pre_div_num = adc_analogure_clk.pre_div;
	writel(sample_rate, info->regs + REG_AP_APB_ADC_ADC_CLK_CFG);
	spin_unlock_irqrestore(&info->lock, flags);

	sdrv_adc_set_resolution_bits(info);
}

static void sdrv_adc_hw_init(struct sdrv_adc *info, bool extref, bool differential)
{
	/* rst adcc */
	sdrv_adc_reset(info);
	/* is use external refh(VDDA1.8) */
	sdrv_adc_set_SELREF_bit(info, !extref);
	if (!extref) {
		sdrv_adc_set_SELBG_bit(info, true);
	}

	sdrv_adc_sample_rate_set(info);
	sdrv_adc_set_delta_mode_bit(info, differential);
	sdrv_adc_set_en_bit(info, true);
	sdrv_adc_set_dwc_rst_bit(info, true);
	mdelay(10);
	sdrv_adc_set_dwc_rst_bit(info, false);

	sdrv_adc_set_interval(info, 0x20);
	sdrv_adc_set_convert_mode(info, ADC_SINGLE_CH_SINGLE_E0);
	sdrv_adc_init_convert(info);
	sdrv_adc_stop_continuous(info, true);
}

static void sdrv_adc_channel_set(struct sdrv_adc *info)
{
	u32 write_v = 0;
	u32 channel;
	unsigned long flags;

	channel = info->channel;

	if (channel > 0x7f)
	{
		printk("selected ch:%d is not surpported\n", channel);
		return;
	}

	spin_lock_irqsave(&info->lock, flags);
	write_v = readl(info->regs + REG_AP_APB_ADC_ADC_SCHC);
	write_v &= ~(u32)(0x7f << ADC_SCHC_CSEL_FIELD_OFFSET);
	write_v |= (u32)((channel & 0x7f) << ADC_SCHC_CSEL_FIELD_OFFSET);
	writel(write_v, info->regs + REG_AP_APB_ADC_ADC_SCHC);
	spin_unlock_irqrestore(&info->lock, flags);

	sdrv_adc_set_repeat(info, !0);
}

static void sdrv_adc_clear_intflag(struct sdrv_adc *info, u32 status)
{
	unsigned long flags;

	spin_lock_irqsave(&info->lock, flags);
	writel(status, info->regs + REG_AP_APB_ADC_ADC_IFLAG);
	spin_unlock_irqrestore(&info->lock, flags);
}

static u32 sdrv_adc_get_sample_rate(struct sdrv_adc *info)
{
	unsigned long input_clk;
	u32 analogue_core_clk;
	u32 core_time_unit;

	if (info->adc_clk_disable) {
		input_clk = info->clk_rate;
	} else {
		input_clk = clk_get_rate(info->clk);
	}

	core_time_unit = info->adc_feature.core_time_unit;
	analogue_core_clk = input_clk / info->pre_div_num;
//	printk("analogue_core_clk is %d,core_time_unit is %d\n", analogue_core_clk, core_time_unit);
	return analogue_core_clk / core_time_unit;
}

static irqreturn_t sdrv_adc_isr(int irq, void *dev_id)
{
	struct sdrv_adc *info = dev_id;
	u32 status;

	status = readl(info->regs + REG_AP_APB_ADC_ADC_IFLAG);
	if(info->use_adc_fifo) {
		if(status & (1 << ADC_IFLAG_WML_FIELD_OFFSET)){
			//fifo is not empty,single convert end
			while (!sdrv_adc_get_fifo_WML(info));
			info->value = sdrv_adc_read_data(info);
			sdrv_adc_stop_continuous(info, true);
			complete(&info->completion);
		}
	} else {
		if (status & (1 << ADC_IFLAG_EOC_FIELD_OFFSET)) {
			info->value = sdrv_adc_read_data(info);
			if(info->value & 0x80000000){
				info->value = sdrv_adc_read_data(info);
			}
			complete(&info->completion);
		}
	}

	/*
	 * adc clear all int flag
	 */
	status |= (ADC_EOC_INT_FLAG_E1 | ADC_WML_INT_FLAG_E2 | ADC_CTC_INT_FLAG_E4 | ADC_EOL_INT_FLAG_E8 | ADC_OVF_INT_FLAG_E16 | ADC_DUMMY_INT_FLAG_E32);
	sdrv_adc_clear_intflag(info, status);
	return IRQ_HANDLED;
}

static int sdrv_adc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct sdrv_adc *info = iio_priv(indio_dev);
	u32 channel;
	long ret;

	switch (mask) {
		case IIO_CHAN_INFO_RAW:
			mutex_lock(&indio_dev->mlock);
			reinit_completion(&info->completion);

			channel = chan->channel & 0x07;
			info->channel = channel;
			sdrv_adc_channel_set(info);
			if(info->use_adc_fifo) {
				//fifo en
				sdrv_adc_set_fifo_en_bit(info, true);
				sdrv_adc_set_timertamp_en_bit(info, false);

				sdrv_adc_set_fifo_threashold_int(info, 1);
				//disable dma
				sdrv_adc_set_dma_en_bit(info, false);
				sdrv_adc_set_dma_single_en_bit(info, false);
				//fulsh fifo
				sdrv_adc_flush_fifo(info);
				sdrv_adc_stop_continuous(info, false);
			}

			sdrv_adc_set_tmr_int_mask_bit(info, true);
			sdrv_adc_set_EOL_int_mask_bit(info, true);
			sdrv_adc_set_CTC_int_mask_bit(info, true);
			sdrv_adc_set_fifo_int_mask_bit(info, false);
			sdrv_adc_set_conv_end_int_mask_bit(info, false);
			sdrv_adc_start_convert(info, true);

			ret = wait_for_completion_interruptible_timeout(&info->completion, SDRV_ADC_TIMEOUT);
			if (ret == 0) {
				mutex_unlock(&indio_dev->mlock);
				return -ETIMEDOUT;
			}
			if (ret < 0) {
				mutex_unlock(&indio_dev->mlock);
				return ret;
			}

			*val = info->value;
			mutex_unlock(&indio_dev->mlock);
			return IIO_VAL_INT;

		case IIO_CHAN_INFO_SCALE:
			info->vref_uv = regulator_get_voltage(info->vref);
			*val = info->vref_uv / 1000;
			*val2 = 12;
			return IIO_VAL_FRACTIONAL_LOG2;

		case IIO_CHAN_INFO_SAMP_FREQ:
			*val = sdrv_adc_get_sample_rate(info);
			return IIO_VAL_INT;

		default:
			return -EINVAL;
	}
}

static int sdrv_adc_reg_access(struct iio_dev *indio_dev, unsigned reg, unsigned writeval, unsigned *readval)
{
	struct sdrv_adc *info = iio_priv(indio_dev);

	if (!readval || reg % 4 || reg > REG_AP_APB_ADC_ADC_MON_THRE)
		return -EINVAL;

	*readval = readl(info->regs + reg);

	return 0;
}

static const struct iio_info sdrv_adc_iio_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &sdrv_adc_read_raw,
	.debugfs_reg_access = &sdrv_adc_reg_access,
};

static const struct of_device_id sdrv_adc_match[] = {
	{ .compatible = "sdrv,sdrv-adc", },
	{},
};
MODULE_DEVICE_TABLE(of, sdrv_adc_match);

static int sdrv_adc_probe(struct platform_device *pdev)
{
	struct sdrv_adc *info;
	struct iio_dev *indio_dev;
	struct device_node *node = pdev->dev.of_node;
	struct resource *mem;
	int ret;
	u32 prop;

	if (!node)
		return -EINVAL;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*info));
	if (!indio_dev) {
		dev_err(&pdev->dev, "Failed allocating iio device\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);
	spin_lock_init(&info->lock);
	info->dev = &pdev->dev;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(info->regs)) {
		ret = PTR_ERR(info->regs);
		dev_err(&pdev->dev, "Failed to remap adc memory, err = %d\n", ret);
		return ret;
	}

	info->irq = platform_get_irq(pdev, 0);
	if (info->irq < 0) {
		dev_err(&pdev->dev, "No irq resource?\n");
		return info->irq;
	}

	info->vref = devm_regulator_get(&pdev->dev, "vref");
	if (IS_ERR(info->vref)) {
		ret = PTR_ERR(info->vref);
		dev_err(&pdev->dev, "Failed getting reference voltage, err = %d\n", ret);
		return ret;
	}

	ret = regulator_enable(info->vref);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable adc reference top voltage, err = %d\n", ret);
		return ret;
	}

	if (of_property_read_u32(node, "resolution_value", &prop)) {
		dev_err(&pdev->dev, "Missing resolution_value property in the DT.\n");
		ret = -EINVAL;
		return ret;
	}

	info->resolution = prop;
	prop = 0;

	if (of_property_read_u32(node, "clksrc_sel", &prop)) {
		dev_err(&pdev->dev, "Missing clksrc_sel property in the DT.\n");
		ret = -EINVAL;
		return ret;
	}

	info->clksrc_sel = prop;
	prop = 0;

	if (of_property_read_u32(node, "clk_value", &prop)) {
		dev_err(&pdev->dev, "Missing clk_value property in the DT.\n");
		ret = -EINVAL;
		return ret;
	}
	info->clk_rate = prop;
	prop = 0;

	if (of_property_read_u32(node, "channel_num", &prop)) {
		dev_err(&pdev->dev, "Missing channel_num property in the DT.\n");
		info->channel_num = 4;
	} else {
		info->channel_num = prop;
		prop = 0;
	}

	info->use_adc_fifo = of_property_read_bool(node, "use-adc-fifo");

	platform_set_drvdata(pdev, indio_dev);

	init_completion(&info->completion);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &sdrv_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = sdrv_adc_iio_channels;
	if ((info->channel_num > 0) && (info->channel_num  <= 8)) {
		indio_dev->num_channels = info->channel_num;
	} else {
		indio_dev->num_channels = 4;
	}

	info->adc_clk_disable = of_property_read_bool(node, "adc-clk-disable");
	if (info->adc_clk_disable) {
		printk("ap adc clk disable, adc clk use safety\n");
	} else {
		info->clk = devm_clk_get(&pdev->dev, "adc-clk");
		if (IS_ERR(info->clk)) {
			ret = PTR_ERR(info->clk);
			dev_err(&pdev->dev, "Failed getting clock, err = %d\n", ret);
			return ret;
		}

		ret = clk_set_rate(info->clk, info->clk_rate);
		if (ret) {
			dev_err(&pdev->dev, "fail to clock set rate.\n");
		}

		ret = clk_prepare_enable(info->clk);
		if (ret) {
			dev_err(&pdev->dev, "Could not prepare or enable the clock.\n");
			goto error_iio_device_register;
		}
	}

	sdrv_adc_feature_config(info);
	sdrv_adc_hw_init(info,true, false);

	ret = devm_request_irq(info->dev, info->irq, sdrv_adc_isr, 0, dev_name(&pdev->dev), info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed requesting irq, irq = %d\n", info->irq);
		goto error_iio_device_register;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		sdrv_adc_power_down(info);
		dev_err(&pdev->dev, "Couldn't register the device.\n");
		goto error_iio_device_register;
	}

	return 0;

error_iio_device_register:
	if (!info->adc_clk_disable) {
		clk_disable_unprepare(info->clk);
	}

	regulator_disable(info->vref);

	return ret;
}

static int sdrv_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct sdrv_adc *info = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);

	sdrv_adc_power_down(info);

	if (!info->adc_clk_disable) {
		clk_disable_unprepare(info->clk);
	}

	regulator_disable(info->vref);

	return 0;
}

static int __maybe_unused sdrv_adc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct sdrv_adc *info = iio_priv(indio_dev);

	sdrv_adc_power_down(info);

	if (!info->adc_clk_disable) {
		clk_disable_unprepare(info->clk);
	}

	regulator_disable(info->vref);

	return 0;
}

static int __maybe_unused sdrv_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct sdrv_adc *info = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(info->vref);
	if (ret) {
		dev_err(info->dev, "Can't enable adc reference top voltage, err = %d\n", ret);
		return ret;
	}

	if (!info->adc_clk_disable) {
		ret = clk_prepare_enable(info->clk);
		if (ret) {
			dev_err(info->dev, "Could not prepare or enable clock.\n");
			regulator_disable(info->vref);
			return ret;
		}
	}

	sdrv_adc_hw_init(info, true, false);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sdrv_adc_pm_ops, sdrv_adc_suspend, sdrv_adc_resume);

static struct platform_driver sdrv_adc_driver = {
	.probe		= sdrv_adc_probe,
	.remove		= sdrv_adc_remove,
	.driver		= {
		.name	= "sdrv_adc",
		.of_match_table = sdrv_adc_match,
		.pm	= &sdrv_adc_pm_ops,
	},
};

module_platform_driver(sdrv_adc_driver);

MODULE_AUTHOR("Peng Wan <peng.wan@semidrive.com>");
MODULE_DESCRIPTION("SemiDrive X9 ADC driver");
MODULE_LICENSE("GPL v2");
