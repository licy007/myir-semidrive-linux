/*
 * sdrv-spdif.c
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
#include "sdrv-spdif.h"
#include "sdrv-common.h"
#include <linux/clk.h>
#include <linux/compiler.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm_params.h>
#include <sound/pcm.h>
#include <stdbool.h>

#define SPDIF_INT_STATUS_REG_BITS 0xffc00000

#define CAL_SPDIF_SAMPLERATE_CODE(SPDIF_CLK,SAMPLE_RATE)  ((2 * SPDIF_CLK + 128 * SAMPLE_RATE) / (2 * 128 * SAMPLE_RATE) - 1)
//#define SND_I2S_SPDIF_REG_DEBUG

#ifdef SND_I2S_SPDIF_REG_DEBUG
static void dump_afe_i2s_spdif_config(struct sdrv_afe_spdif *afe)
{
	u32 ret, val;
	/* register value print */
	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL, &val);
	dev_info(afe->dev, "DUMP %d REG_CDN_SPDIF_REGS_SPDIF_CTRL 0x%x: (0x%x)\n",
			__LINE__, REG_CDN_SPDIF_REGS_SPDIF_CTRL, val);

	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_INT_REG, &val);
	dev_info(afe->dev, "DUMP %d REG_CDN_SPDIF_REGS_SPDIF_INT_REG 0x%x: (0x%x)\n",
			__LINE__, REG_CDN_SPDIF_REGS_SPDIF_INT_REG, val);

	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL, &val);
	dev_info(afe->dev, "DUMP %d REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL 0x%x: (0x%x)\n",
			__LINE__, REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL, val);

	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_STAT_REG, &val);
	dev_info(afe->dev, "DUMP %d REG_CDN_SPDIF_REGS_SPDIF_STAT_REG 0x%x: (0x%x)\n",
			__LINE__, REG_CDN_SPDIF_REGS_SPDIF_STAT_REG, val);
}
#endif

static void afe_spdif_config(struct sdrv_afe_spdif *afe)
{


	/* disable spdif core */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_SPDIF_ENABLE,
			(0 << SPDIF_CTRL_SPDIF_ENABLE_SHIFT));
	/* reset sfr */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_SFR_ENABLE,
			(0 << SPDIF_CTRL_SFR_ENABLE_SHIFT));
	/* reset fifo */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_FIFO_ENABLE,
			(0 << SPDIF_CTRL_FIFO_ENABLE_SHIFT));
	/* disable spdif core clock */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_CLK_ENABLE,
			(1 << SPDIF_CTRL_CLK_ENABLE_SHIFT));
	/* disable fifo interface */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_USE_FIFO_IF,
			(0 << SPDIF_CTRL_USE_FIFO_IF_SHIFT));
	/* set spdif transfer preamble B num */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_SET_PREAMB_B,
			(0 << SPDIF_CTRL_SET_PREAMB_B_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_INT_REG,
			BIT_SPDIF_INT_PREAMBLE_DEL,
			(0 << SPDIF_INT_PREAMBLE_DEL_SHIFT));
	/* set parity-gen active */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_PARITY_GEN,
			(1 << SPDIF_CTRL_PARITY_GEN_SHIFT));
	/* enalbe spdif rx parity check */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_PARITY_CHECK,
			(1 << SPDIF_CTRL_PARITY_CHECK_SHIFT));
	/* enalbe spdif rx validty check */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_VALIDITY_CHECK,
			(1 << SPDIF_CTRL_VALIDITY_CHECK_SHIFT));
	/* interrupt config */
	/* set interrupt mask */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_UNDERR_MASK,
			(1 << SPDIF_CTRL_UNDERR_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_OVERR_MASK,
			(1 << SPDIF_CTRL_OVERR_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_PARITY_MASK,
			(0 << SPDIF_CTRL_PARITY_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_SYNCERR_MASK,
			(0 << SPDIF_CTRL_SYNCERR_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_LOCK_MASK,
			(1 << SPDIF_CTRL_LOCK_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_BEGIN_MASK,
			(0 << SPDIF_CTRL_BEGIN_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_FULL_MASK,
			(0 << SPDIF_CTRL_FULL_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_AFULL_MASK,
			(0 << SPDIF_CTRL_AFULL_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_EMPTY_MASK,
			(0 << SPDIF_CTRL_EMPTY_MASK_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_AEMPTY_MASK,
			(0 << SPDIF_CTRL_AEMPTY_MASK_SHIFT));
	/* set fifo level */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL,
			BIT_SPDIF_FIFO_CTRL_AEMPTY_THRESHOLD,
			(0x30 << SPDIF_FIFO_CTRL_AEMPTY_THRESHOLD_SHIFT));
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL,
			BIT_SPDIF_FIFO_CTRL_AFULL_THRESHOLD,
			(0x20 << SPDIF_FIFO_CTRL_AFULL_THRESHOLD_SHIFT));
	/* set transfer dir: tx */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_TR_MODE,
			(1 << SPDIF_CTRL_TR_MODE_SHIFT));
	/* enable spdif core clk */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_CLK_ENABLE,
			(0 << SPDIF_CTRL_CLK_ENABLE_SHIFT));
	regcache_sync(afe->regmap);

#ifdef SND_I2S_SPDIF_REG_DEBUG
	dump_afe_i2s_spdif_config(afe);
#endif
}

static int snd_afe_spdif_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct sdrv_afe_spdif *afe = snd_soc_dai_get_drvdata(dai);

	/* clk_enable(afe->clk); */
	/* judge if spdif in progress, or return error code*/
	if (afe->spdif_status) {
		dev_err(afe->dev, "device busy! spdif status: %d\n", afe->spdif_status);
		return -EINPROGRESS;
	}
	snd_soc_dai_set_dma_data(dai, substream, afe);
	afe_spdif_config(afe);
	return 0;
}

static void snd_afe_spdif_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct sdrv_afe_spdif *afe = snd_soc_dai_get_drvdata(dai);
	afe->spdif_status = SDRV_SPDIF_STATUS_FREE;
	/* clk_disable(afe->clk); */
}

static int snd_afe_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{

	struct sdrv_afe_spdif *afe = snd_soc_dai_get_drvdata(dai);

	unsigned srate = params_rate(params);
	unsigned tsrate_code = CAL_SPDIF_SAMPLERATE_CODE(clk_get_rate(afe->clk),srate);
	unsigned channels = params_channels(params);
	dev_info(afe->dev, "snd_afe_spdif_hw_params: srate = %d.\n", srate);
	dev_info(afe->dev, "snd_afe_spdif_hw_params: spdif tsrate_code = %d.\n", tsrate_code);


	/* generate spdif status code by hw params */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch(srate) {
			case 16000: afe->spdif_status = SDRV_SPDIF_STATUS_16_OUT;
						break;
			case 44100: afe->spdif_status = SDRV_SPDIF_STATUS_44_OUT;
						break;
			case 48000: afe->spdif_status = SDRV_SPDIF_STATUS_48_OUT;
						break;
			default: afe->spdif_status = SDRV_SPDFI_STATUS_UNDEFINED;
		}
	}

	if (channels == 2) {
		/* cfg spdif to stereo */
		regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
				BIT_SPDIF_CTRL_CHANNEL_MODE,
				(0 << SPDIF_CTRL_CHANNEL_MODE_SHIFT));
	} else if(channels == 1){
		/* cfg spdif to stereo */
		regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
				BIT_SPDIF_CTRL_CHANNEL_MODE,
				(1 << SPDIF_CTRL_CHANNEL_MODE_SHIFT));
	} else {
		/* do not support multi-channels */
		DEBUG_FUNC_PRT
			return -EINVAL;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		DEBUG_FUNC_PRT
			regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
					BIT_SPDIF_CTRL_TR_MODE,
					(1 << SPDIF_CTRL_TR_MODE_SHIFT));

		/*set tx substream*/
		afe->tx_substream = substream;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		DEBUG_FUNC_PRT
			regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
					BIT_SPDIF_CTRL_TR_MODE,
					(0 << SPDIF_CTRL_TR_MODE_SHIFT));

		/*set rx substream */
		afe->rx_substream = substream;
	} else {
		DEBUG_FUNC_PRT
			return -EINVAL;
	}

	/* cfg sample rate */
	regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			BIT_SPDIF_CTRL_TSAMPLE_RATE,
			(tsrate_code << SPDIF_CTRL_TSAMPLE_RATE_SHIFT));

	regcache_sync(afe->regmap);

#ifdef SND_I2S_SPDIF_REG_DEBUG
	dump_afe_i2s_spdif_config(afe);
#endif
	return 0;
}

static int snd_afe_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{

	struct sdrv_afe_spdif *afe = snd_soc_dai_get_drvdata(dai);
	DEBUG_FUNC_PRT
	printk(KERN_INFO "%s:%i ------cmd(%d)--------------\n", __func__,
			__LINE__, cmd);
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		case SNDRV_PCM_TRIGGER_RESUME:
			DEBUG_FUNC_PRT;
			regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
					BIT_SPDIF_CTRL_SPDIF_ENABLE,
					(1 << SPDIF_CTRL_SPDIF_ENABLE_SHIFT));
			regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
					BIT_SPDIF_CTRL_INTREQ_MASK,
					(1 << SPDIF_CTRL_INTREQ_MASK_SHIFT));
			if (!afe->spdif_status)
				afe->spdif_status = SDRV_SPDFI_STATUS_UNDEFINED;
			break;

		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		case SNDRV_PCM_TRIGGER_SUSPEND:
			regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
					BIT_SPDIF_CTRL_SPDIF_ENABLE,
					(0 << SPDIF_CTRL_SPDIF_ENABLE_SHIFT));
			regmap_update_bits(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
					BIT_SPDIF_CTRL_INTREQ_MASK,
					(0 << SPDIF_CTRL_INTREQ_MASK_SHIFT));
			/* set spdif status free */
			afe->spdif_status = SDRV_SPDIF_STATUS_FREE;
			break;

		default:
			return -EINVAL;
	}
	regcache_sync(afe->regmap);
	return 0;
}

static struct snd_soc_dai_ops snd_afe_spdif_dai_ops = {
	.startup = snd_afe_spdif_startup,
	.shutdown = snd_afe_spdif_shutdown,
	.hw_params = snd_afe_spdif_hw_params,
	.trigger = snd_afe_spdif_trigger,
};

static int snd_soc_spdif_dai_probe(struct snd_soc_dai *dai)
{
	struct sdrv_afe_spdif *afe = snd_soc_dai_get_drvdata(dai);

	dev_info(afe->dev, "DAI probe.");
	dai->capture_dma_data = &afe->capture_dma_data;
	dai->playback_dma_data = &afe->playback_dma_data;

	return 0;
}

static struct snd_soc_dai_driver snd_afe_spdif_dais[] = {
	{
		.name = "snd_afe_spdif_dai0",
		.probe = snd_soc_spdif_dai_probe,
		.playback = {
			.stream_name = "SPDIF Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "SPDIF Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &snd_afe_spdif_dai_ops,
	}
};

static struct snd_soc_component_driver snd_afe_spdif_soc_component = {
	.name = "afe-spdif"
};

static bool sdrv_spdif_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case REG_CDN_SPDIF_REGS_SPDIF_CTRL:
		case REG_CDN_SPDIF_REGS_SPDIF_INT_REG:
		case REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL:
		case REG_CDN_SPDIF_REGS_SPDIF_STAT_REG:
			return true;
		default:
			return false;
	}
}

static const struct regmap_config sdrv_spdif_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = SPDIF_FIFO_ADDR_OFFSET + 1,
	.writeable_reg = sdrv_spdif_wr_rd_reg,
	.readable_reg = sdrv_spdif_wr_rd_reg,
	.volatile_reg = sdrv_spdif_wr_rd_reg,
	.cache_type = REGCACHE_FLAT,
};

static const struct snd_pcm_hardware sdrv_spdif_pcm_hardware = {

	.info = (SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	.rates = SNDRV_PCM_RATE_8000_96000,
	.rate_min = SNDRV_PCM_RATE_8000,
	.rate_max = SNDRV_PCM_RATE_96000,
	.channels_min = 1,
	.channels_max = 2,
	.period_bytes_min = MIN_PERIOD_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = MIN_PERIODS,
	.periods_max = MAX_PERIODS,
	.buffer_bytes_max = MAX_ABUF_SIZE,
	.fifo_size = 0,
};

static int sdrv_pcm_prepare_slave_config(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct dma_slave_config *slave_config)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdrv_afe_spdif *afe = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	int ret;

	ret = snd_hwparams_to_dma_slave_config(substream, params, slave_config);
	if (ret)
		return ret;

	slave_config->dst_maxburst = 4;
	slave_config->src_maxburst = 4;

	/*Change width by audio format */

	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S8:
			dev_err(afe->dev, "%s(%d) Don't support this function for S8 is low dynamic range.\n",
					__func__,__LINE__);
			return -EINVAL;
		case SNDRV_PCM_FORMAT_S16_LE:
			DEBUG_FUNC_PRT;
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S24_3LE:
		case SNDRV_PCM_FORMAT_S32_LE:
			DEBUG_FUNC_PRT;
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			break;
		default:
			return -EINVAL;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		slave_config->dst_addr = afe->playback_dma_data.addr;
	else
		slave_config->src_addr = afe->capture_dma_data.addr;
	DEBUG_FUNC_PRT;
	return 0;
}

static const struct snd_dmaengine_pcm_config sdrv_dmaengine_spdif_pcm_config = {
	.pcm_hardware = &sdrv_spdif_pcm_hardware,
	.prepare_slave_config = sdrv_pcm_prepare_slave_config,
	.prealloc_buffer_size = MAX_ABUF_SIZE,
};

static irqreturn_t sdrv_spdif_irq_handler(int irq, void *dev_id)
{
	struct sdrv_afe_spdif *afe = dev_id;
	unsigned int spdif_status, rx_rate = 0;
	struct snd_pcm_runtime *runtime;
	int ret;
	unsigned long spdif_clk;

	runtime = afe->tx_substream->runtime;
	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_INT_REG,
			&spdif_status);
	regmap_write(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_INT_REG,
			spdif_status & (~SPDIF_INT_STATUS_REG_BITS));

	if (ret) {
		dev_err(afe->dev, "%s irq status err\n", __func__);
		return IRQ_HANDLED;
	}

	if (spdif_status & BIT_SPDIF_INT_TDATA_UNDERR) {
		dev_err(afe->dev, "SPDIF underrun! \n");
	}

	if (spdif_status & BIT_SPDIF_INT_RDATA_OVERR) {
		dev_err(afe->dev, "SPDIF overrun! \n");
	}

	if (spdif_status & BIT_SPDIF_INT_LOCK) {
		rx_rate = spdif_status & BIT_SPDIF_INT_RSAMPLE_RATE;
		spdif_clk = clk_get_rate(afe->clk);
		if(rx_rate == CAL_SPDIF_SAMPLERATE_CODE(spdif_clk,16000)) {
			afe->spdif_status = SDRV_SPDIF_STATUS_16_IN;
		} else if(rx_rate == CAL_SPDIF_SAMPLERATE_CODE(spdif_clk,48000)) {
			afe->spdif_status = SDRV_SPDIF_STATUS_48_IN;
		} else {
			afe->spdif_status = SDRV_SPDFI_STATUS_UNDEFINED;
		}
		dev_info(afe->dev, "spdif rx sample rate: %d.\n", afe->spdif_status);
	}

	return IRQ_HANDLED;
}

static int snd_afe_spdif_probe(struct  platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_afe_spdif *afe;
	struct resource *res;
	unsigned int irq_id;
	unsigned int value;
	int ret;

	dev_info(dev, "Probed.\n");
	afe = devm_kzalloc(&pdev->dev, sizeof(struct sdrv_afe_spdif), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	afe->pdev = pdev;
	afe->dev = dev;
	afe->spdif_status = SDRV_SPDIF_STATUS_FREE;
	platform_set_drvdata(pdev, afe);

	/* get clock */
	afe->clk = devm_clk_get(&pdev->dev, "spdif-clk");
	if (IS_ERR(afe->clk))
		return PTR_ERR(afe->clk);
	DEBUG_ITEM_PRT(clk_get_rate(afe->clk));

	ret = device_property_read_u32(dev, "sdrv,spdif-clk", &value);
	if(!ret) {
		ret = clk_set_rate(afe->clk,value);
		if (ret) {
			dev_err(dev, "spdif set_clk failed: %d\n", ret);
			return ret;
		}
	}

	ret = clk_prepare_enable(afe->clk);
	if (ret) {
		dev_err(dev, "clk clk_prepare_enable failed: %d\n", ret);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->regs = devm_ioremap_resource(afe->dev, res);
	if (IS_ERR(afe->regs)) {
		ret = PTR_ERR(afe->regs);
		goto err_disable;
	}

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->regs,
			&sdrv_spdif_regmap_config);

	if (IS_ERR(afe->regmap)) {
		ret = PTR_ERR(afe->regmap);
		goto err_disable;
	}

	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_err(afe->dev, "%s no irq\n", dev->of_node->name);
		return -ENXIO;
	}

	dev_info(&pdev->dev,
			"ALSA X9 SPDIF"
			" irq(%d) num(%d) vaddr(0x%llx) paddr(0x%llx) \n",
			irq_id, pdev->num_resources, afe->regs, res->start);

	ret = devm_request_irq(afe->dev, irq_id, sdrv_spdif_irq_handler, 0,
			pdev->name, (void*)afe);

	/* cfg dma relate info */
	afe->playback_dma_data.addr = res->start + SPDIF_FIFO_ADDR_OFFSET;
	afe->playback_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	afe->playback_dma_data.maxburst = 4;

	afe->capture_dma_data.addr = res->start + SPDIF_FIFO_ADDR_OFFSET;
	afe->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	afe->capture_dma_data.maxburst = 4;

	ret = devm_snd_dmaengine_pcm_register(dev,&sdrv_dmaengine_spdif_pcm_config, 0);
	if (ret)
		goto err_disable;

	ret = devm_snd_soc_register_component(dev, &snd_afe_spdif_soc_component,
			snd_afe_spdif_dais,
			ARRAY_SIZE(snd_afe_spdif_dais));
	if (ret < 0) {
		dev_err(&pdev->dev, "couldn't register component\n");
		goto err_disable;
	}

	return 0;

err_disable:
	return ret;
}

static int snd_afe_spdif_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_afe_spdif *afe = platform_get_drvdata(pdev);
	dev_info(dev, "Remove.\n");
	regmap_exit(afe->regmap);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id sdrv_spdif_of_match[] = {
    {
	.compatible = "semidrive,x9-spdif",
    },
    {},
};

#ifdef CONFIG_PM_SLEEP
static int sdrv_spdif_suspend(struct device *dev)
{
	int ret;
	struct sdrv_afe_spdif *afe = dev_get_drvdata(dev);

	dev_info(afe->dev, "%s\n", __func__);

	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			  &afe->reg_spdif_ctrl);
	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_INT_REG,
			  &afe->reg_spdif_int);
	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL,
			  &afe->reg_spdif_fifo_ctrl);
	ret = regmap_read(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_STAT_REG,
			  &afe->reg_spdif_stat);

	clk_disable_unprepare(afe->clk);

	return 0;
}

static int sdrv_spdif_resume(struct device *dev)
{
	int ret;
	struct sdrv_afe_spdif *afe = dev_get_drvdata(dev);

	dev_info(afe->dev, "%s\n", __func__);

	ret = clk_prepare_enable(afe->clk);
	if (ret) {
		dev_err(dev, "spdif clk_prepare_enable failed: %d\n", ret);
		return ret;
	}

	ret = regmap_write(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_INT_REG,
			  afe->reg_spdif_int);
	ret = regmap_write(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_FIFO_CTRL,
			  afe->reg_spdif_fifo_ctrl);
	ret = regmap_write(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_STAT_REG,
			  afe->reg_spdif_stat);
	ret = regmap_write(afe->regmap, REG_CDN_SPDIF_REGS_SPDIF_CTRL,
			  afe->reg_spdif_ctrl);

	return 0;
}

static const struct dev_pm_ops spdif_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdrv_spdif_suspend, sdrv_spdif_resume)
};

#endif /* CONFIG_PM_SLEEP */

static struct platform_driver snd_afe_spdif_driver = {
	.driver = {
		.name = "snd-afe-spdif",
		.of_match_table = sdrv_spdif_of_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &spdif_pm_ops,
#endif
	},
	.probe = snd_afe_spdif_probe,
	.remove = snd_afe_spdif_remove,
};

module_platform_driver(snd_afe_spdif_driver);
MODULE_AUTHOR("Su Zhenxiu <zhenxiu.su@semidrive.com>");
MODULE_DESCRIPTION("X9 SPDIF ALSA SoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 spdif asoc device");
