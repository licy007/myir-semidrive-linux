/*
 * sdrv-i2s-mc.c
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
#include "sdrv-i2s-mc.h"
#include "sdrv-common.h"
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#define DRV_NAME "snd-afe-i2s-mc"
#define X9_I2S_MC_FIFO_SIZE 2048

static bool fake_buffer = 0;
module_param(fake_buffer, bool, 0444);
MODULE_PARM_DESC(fake_buffer, "Fake buffer allocations.");
//#define SND_I2S_MC_REG_DEBUG

#ifdef SND_I2S_MC_REG_DEBUG
static void dump_afe_i2s_mc_config(struct sdrv_afe_i2s_mc *afe)
{
	u32 ret, val;

	dev_info(afe->dev, "\n<func: %s>--line %d\n", __func__, __LINE__);

	ret = regmap_read(afe->regmap, MC_I2S_CTRL, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_CTRL 0x%x:(0x%x)\n", __LINE__,
		MC_I2S_CTRL, val);

	ret = regmap_read(afe->regmap, MC_I2S_INTR_STAT, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_INTR_STAT 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_INTR_STAT, val);

	ret = regmap_read(afe->regmap, MC_I2S_SRR, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_SRR 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_SRR, val);

	ret = regmap_read(afe->regmap, MC_I2S_CID_CTRL, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_CID_CTRL 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_CID_CTRL, val);

	ret = regmap_read(afe->regmap, MC_I2S_TFIFO_CTRL, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_TFIFO_CTRL 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_TFIFO_CTRL, val);

	ret = regmap_read(afe->regmap, MC_I2S_RFIFO_CTRL, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_RFIFO_CTRL 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_RFIFO_CTRL, val);

	ret = regmap_read(afe->regmap, MC_I2S_TFIFO_STAT, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_TFIFO_STAT 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_TFIFO_STAT, val);

	ret = regmap_read(afe->regmap, MC_I2S_RFIFO_STAT, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_RFIFO_STAT 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_RFIFO_STAT, val);

	ret = regmap_read(afe->regmap, MC_I2S_DEV_CONF, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_DEV_CONF 0x%x: (0x%x)\n", __LINE__,
		MC_I2S_DEV_CONF, val);
	dev_info(afe->dev, "<func: %s>--line %d-------end.\n\n", __func__, __LINE__);
}
#endif
/* misc operation */
/*DAI start up config*/
static void afe_i2s_mc_config(struct sdrv_afe_i2s_mc *afe)
{

	/*Disable chn0~chn7*/
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_EN_MASK,
			   (0 << CTRL_EN_SHIFT));

	/*sfr reset*/
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_SFR_RST_MASK,
			   (0 << CTRL_SFR_RST_SHIFT));

	/*fifo reset*/
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_TFIFO_RST_MASK,
			   (0 << CTRL_TFIFO_RST_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_RFIFO_RST_MASK,
			   (0 << CTRL_RFIFO_RST_SHIFT));

	/*sync reset*/
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_TSYNC_RST_MASK,
			   (0 << CTRL_TSYNC_RST_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_RSYNC_RST_MASK,
			   (0 << CTRL_RSYNC_RST_SHIFT));


	if (true == afe->loopback_mode) {
		/* TODO:	Here set up afe loop back config. */
	} else {
		/* 	Here set up normal mode config. */
		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_LOOP_BACK_0_1_MASK,
				   (0 << CTRL_LOOP_BACK_0_1_SHIFT));

		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_LOOP_BACK_2_3_MASK,
				   (0 << CTRL_LOOP_BACK_2_3_SHIFT));

		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_LOOP_BACK_4_5_MASK,
				   (0 << CTRL_LOOP_BACK_4_5_SHIFT));

		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_LOOP_BACK_7_8_MASK,
				   (0 << CTRL_LOOP_BACK_7_8_SHIFT));

		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_TSYNC_LOOP_BACK_MASK,
				   (0 << CTRL_TSYNC_LOOP_BACK_SHIFT));

		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_RSYNC_LOOP_BACK_MASK,
				   (0 << CTRL_RSYNC_LOOP_BACK_SHIFT));
	}

	/*config FIFO threshold
	Tx FIFO almost empty*/
	regmap_update_bits(
		afe->regmap, MC_I2S_TFIFO_CTRL, TAEMPTY_THRESHOLD_MASK,
		((I2S_MC_TX_DEPTH - 128) << TAEMPTY_THRESHOLD_SHIFT));
	/*Tx FIFO almost full*/
	regmap_update_bits(afe->regmap, MC_I2S_TFIFO_CTRL,
			   TAFULL_THRESHOLD_MASK,
			   ((I2S_MC_TX_DEPTH - 1) << TAFULL_THRESHOLD_SHIFT));
	/*Rx FIFO almost empty*/
	regmap_update_bits(afe->regmap, MC_I2S_RFIFO_CTRL,
			   RAEMPTY_THRESHOLD_MASK,
			   (0 << RAEMPTY_THRESHOLD_SHIFT));
	/*Rx FIFO almost full*/
	regmap_update_bits(afe->regmap, MC_I2S_RFIFO_CTRL,
			   RAFULL_THRESHOLD_MASK,
			   ((I2S_MC_RX_DEPTH - 32) << RAFULL_THRESHOLD_SHIFT));

	regcache_sync(afe->regmap);

#ifdef SND_I2S_MC_REG_DEBUG
	dump_afe_i2s_mc_config(afe);
#endif

}

static void afe_i2s_mc_start_capture(struct sdrv_afe_i2s_mc *afe)
{

	/*Firstly,Enable i2s channel*/
	if (true == afe->loopback_mode) {
		/* In loopback mode enable all of channel */
		regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_EN_MASK,
					(0xff << CTRL_EN_SHIFT));
	} else {

		regmap_update_bits(afe->regmap, MC_I2S_CTRL, (afe->rx_slot_mask << CTRL_EN_SHIFT),
					(afe->rx_slot_mask << CTRL_EN_SHIFT));
	};

	afe->capture_status = 1;

	/*sync */
	if (afe->clk_sync_loop == 0x02) { //rx_clk_loop
		regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_TSYNC_RST_MASK,
			   (0x1 << CTRL_TSYNC_RST_SHIFT));
	}

	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_RSYNC_RST_MASK,
			   (0x1 << CTRL_RSYNC_RST_SHIFT));

	/* CID setting		 */
	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, (afe->rx_slot_mask << I2S_MASK_SHIFT),
			   (afe->rx_slot_mask << I2S_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_EMPTY_MASK_MASK,
			   (0x0 << TFIFO_EMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_AEMPTY_MASK_MASK,
			   (0x0 << TFIFO_AEMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_FULL_MASK_MASK,
			   (0x0 << TFIFO_FULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_AFULL_MASK_MASK,
			   (0x0 << TFIFO_AFULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_EMPTY_MASK_MASK,
			   (0x0 << RFIFO_EMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_AEMPTY_MASK_MASK,
			   (0x0 << RFIFO_AEMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_FULL_MASK_MASK,
			   (0x0 << RFIFO_FULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_AFULL_MASK_MASK,
			   (0x0 << RFIFO_AFULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, INTREQ_MASK_MASK,
			   (0x1 << INTREQ_MASK_SHIFT));

	regcache_sync(afe->regmap);

	dev_info(afe->dev, "\n<func: %s>--line %d\n", __func__, __LINE__);

}

static void afe_i2s_mc_stop_capture(struct sdrv_afe_i2s_mc *afe)
{

	/* Disable rx_slot_mask */
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, (afe->rx_slot_mask << CTRL_EN_SHIFT),
				((~afe->rx_slot_mask) << CTRL_EN_SHIFT));
	/* Then disable interrupt */
	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, (afe->rx_slot_mask << INTREQ_MASK_SHIFT),
				((~afe->rx_slot_mask) << INTREQ_MASK_SHIFT));

	afe->capture_status = 0;

	if (afe->playback_status == 0) {//playback no runnig
		if (afe->clk_sync_loop == 0x02) { // rx_clk_loop
			regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_TSYNC_RST_MASK,
			    (0x0 << CTRL_TSYNC_RST_SHIFT));
		}
		regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_RSYNC_RST_MASK,
			   (0x0 << CTRL_RSYNC_RST_SHIFT));
		afe_i2s_mc_config(afe); //set default
	} else {//playback is running
		if (afe->clk_sync_loop != 0x01) {// no need receive provid clk
			regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_RSYNC_RST_MASK,
			   (0x0 << CTRL_RSYNC_RST_SHIFT));
		}
	}

	regcache_sync(afe->regmap);
	dev_info(afe->dev, "\n<func: %s>--line %d\n", __func__, __LINE__);

}

static void afe_i2s_mc_start_playback(struct sdrv_afe_i2s_mc *afe)
{

	/*Firstly,Enable i2s channel*/
	if (true == afe->loopback_mode) {
		/* In loopback mode enable all of channel */
		regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_EN_MASK,
				   (0xff << CTRL_EN_SHIFT));
	} else {
		regmap_update_bits(afe->regmap, MC_I2S_CTRL, (afe->tx_slot_mask << CTRL_EN_SHIFT),
				   (afe->tx_slot_mask << CTRL_EN_SHIFT));
	};

	afe->playback_status = 1;

	/*sync */
	if (afe->clk_sync_loop == 0x01) { //tx_clk_loop
		regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_RSYNC_RST_MASK,
			   (0x1 << CTRL_RSYNC_RST_SHIFT));
	}

	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_TSYNC_RST_MASK,
			   (0x1 << CTRL_TSYNC_RST_SHIFT));

	/* CID setting		 */
	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, (afe->tx_slot_mask << I2S_MASK_SHIFT),
			   (afe->tx_slot_mask << I2S_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_EMPTY_MASK_MASK,
			   (0x0 << TFIFO_EMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_AEMPTY_MASK_MASK,
			   (0x0 << TFIFO_AEMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_FULL_MASK_MASK,
			   (0x0 << TFIFO_FULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, TFIFO_AFULL_MASK_MASK,
			   (0x0 << TFIFO_AFULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_EMPTY_MASK_MASK,
			   (0x0 << RFIFO_EMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_AEMPTY_MASK_MASK,
			   (0x0 << RFIFO_AEMPTY_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_FULL_MASK_MASK,
			   (0x0 << RFIFO_FULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, RFIFO_AFULL_MASK_MASK,
			   (0x0 << RFIFO_AFULL_MASK_SHIFT));

	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, INTREQ_MASK_MASK,
			   (0x1 << INTREQ_MASK_SHIFT));

	regcache_sync(afe->regmap);


	dev_info(afe->dev, "<func: %s>--line %d.\n\n", __func__, __LINE__);

}

static void afe_i2s_mc_stop_playback(struct sdrv_afe_i2s_mc *afe)
{

	/*Disable tx_slot_mask*/
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, (afe->tx_slot_mask << CTRL_EN_SHIFT),
			   ((~afe->tx_slot_mask) << CTRL_EN_SHIFT));
	/* Then disable interrupt */
	regmap_update_bits(afe->regmap, MC_I2S_CID_CTRL, (afe->tx_slot_mask << INTREQ_MASK_SHIFT),
			   ((~afe->tx_slot_mask) << INTREQ_MASK_SHIFT));

	afe->playback_status = 0;

	if (afe->capture_status == 0) {//capture no runnig
		if (afe->clk_sync_loop == 0x01) { // tx_clk_loop
			regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_RSYNC_RST_MASK,
			    (0x0 << CTRL_RSYNC_RST_SHIFT));
		}
		regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_TSYNC_RST_MASK,
			   (0x0 << CTRL_TSYNC_RST_SHIFT));
		afe_i2s_mc_config(afe); //set default
	} else {//capture is running
		if (afe->clk_sync_loop != 0x02) {// no need transmit provid clk
			regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_TSYNC_RST_MASK,
			   (0x0 << CTRL_TSYNC_RST_SHIFT));
		}
	}

	regcache_sync(afe->regmap);
	dev_info(afe->dev, "\n<func: %s>--line %d\n", __func__, __LINE__);

}

static const struct snd_pcm_hardware sdrv_pcm_hardware = {

	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SND_FORMATS,
	.rates = SND_RATE,
	.rate_min = SND_RATE_MIN,
	.rate_max = SND_RATE_MAX,
	.channels_min = SND_CHANNELS_MIN,
	.channels_max = SND_CHANNELS_MAX,
	.period_bytes_min = MIN_I2S_MC_PERIOD_SIZE,
	.period_bytes_max = MAX_I2S_MC_PERIOD_SIZE,
	.periods_min = MIN_I2S_MC_PERIODS,
	.periods_max = MAX_I2S_MC_PERIODS,
	.buffer_bytes_max = MAX_I2S_MC_ABUF_SIZE,
	.fifo_size = 0,
};


/* Set up irq handler
 * ------------------------------------------------------------ */
static irqreturn_t sdrv_i2s_mc_irq_handler(int irq, void *dev_id)
{
	struct sdrv_afe_i2s_mc *afe = dev_id;
	unsigned int i2s_mc_status;
	/* struct snd_pcm_runtime *runtime = afe->tx_substream->runtime; */

	int ret = 0, chan_code;
	/* Read status to i2s_mc_status and then clear the register */
	ret = regmap_read(afe->regmap, MC_I2S_INTR_STAT, &i2s_mc_status);
	/* clear irq here*/

	if (ret) {
		dev_err(afe->dev, "%s irq status err !\n", __func__);
		return IRQ_HANDLED;
	}

	if (i2s_mc_status & INTR_TDATA_UNDERR_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		chan_code = (i2s_mc_status & INTR_UNDERR_CODE_MASK) >>
				INTR_UNDERR_CODE_SHIFT;
		dev_err(afe->dev, "i2s mc channel(0x%x) underrun !\n",
			chan_code);
	}

	if (i2s_mc_status & INTR_RDATA_OVRERR_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x0);
		chan_code = (i2s_mc_status & INTR_OVRERR_CODE_MASK) >>
				INTR_OVRERR_CODE_SHIFT;
		dev_err(afe->dev, "i2s mc channel(0x%x) overrun !\n",
			chan_code);
	}

	if (i2s_mc_status & INTR_TFIFO_EMPTY_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc tx fifo empty !\n");
	}

	if (i2s_mc_status & INTR_TFIFO_AEMPTY_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc tx fifo almost empty !\n");
	}

	if (i2s_mc_status & INTR_TFIFO_FULL_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc tx fifo full !\n");
	}

	if (i2s_mc_status & INTR_TFIFO_AFULL_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc tx fifo almost full !\n");
	}
	if (i2s_mc_status & INTR_RFIFO_EMPTY_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc fifo rx fifo empty !\n");
	}
	if (i2s_mc_status & INTR_RFIFO_AEMPTY_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc rx fifo almost empty !\n");
	}

	if (i2s_mc_status & INTR_RFIFO_FULL_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc rx fifo full !\n");
	}
	if (i2s_mc_status & INTR_RFIFO_AFULL_MASK) {
		regmap_write(afe->regmap, MC_I2S_INTR_STAT, 0x10);
		dev_err(afe->dev, "i2s mc fifo almost full !\n");
	}

	return IRQ_HANDLED;
}

/* -Set up dai regmap
 * here----------------------------------------------------------- */
static bool sdrv_i2s_mc_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MC_I2S_CTRL:
	case MC_I2S_INTR_STAT:
	case MC_I2S_SRR:
	case MC_I2S_CID_CTRL:
	case MC_I2S_TFIFO_STAT:
	case MC_I2S_RFIFO_STAT:
	case MC_I2S_TFIFO_CTRL:
	case MC_I2S_RFIFO_CTRL:
	case MC_I2S_DEV_CONF:
	case MC_I2S_POLL_STAT:
		return true;
	default:
		return false;
	}
}


static const struct regmap_config sdrv_i2s_mc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = MC_I2S_POLL_STAT,
	.writeable_reg = sdrv_i2s_mc_wr_rd_reg,
	.readable_reg = sdrv_i2s_mc_wr_rd_reg,
	.volatile_reg = sdrv_i2s_mc_wr_rd_reg,
	.cache_type = REGCACHE_FLAT,
};

int snd_afe_i2s_mc_dai_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *cpu_dai;
	struct sdrv_afe_i2s_mc *afe;

	DEBUG_FUNC_PRT;
	rtd = substream->private_data;
	cpu_dai = rtd->cpu_dai;
	/* TODO: enable main clk */
	afe = snd_soc_dai_get_drvdata(dai);
	snd_soc_dai_set_dma_data(dai, substream, afe);

	return 0;
}

int snd_afe_i2s_mc_dai_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hwparam,
				 struct snd_soc_dai *dai)
{
	u32 ret, val;
	unsigned mc_sr;
	unsigned long clk_rate;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdrv_afe_i2s_mc *afe = snd_soc_dai_get_drvdata(dai);

	unsigned srate = params_rate(hwparam);
	unsigned channels = params_channels(hwparam);
	unsigned period_size = params_period_size(hwparam);
	unsigned sample_size = snd_pcm_format_width(params_format(hwparam));

	DEBUG_FUNC_PRT
	/* set tdm slot */
	afe->slot_width = sample_size;
	afe->slots = channels;

	dev_info(afe->dev, "%s:%i : tx_mask(0x%x) rx_mask(0x%x) slots(%d) slot_width(%d) \n", __func__,
		   __LINE__, afe->tx_slot_mask, afe->rx_slot_mask, channels, sample_size);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* Here calculate and fill sample rate.  */
		clk_rate = clk_get_rate(afe->clk_i2s);
		mc_sr =
			I2S_MC_SAMPLE_RATE_CALC(clk_rate, srate, I2S_MC_DATA_WIDTH);

		DEBUG_ITEM_PRT(mc_sr);
		regmap_update_bits(afe->regmap, MC_I2S_SRR, TSAMPLE_RATE_MASK,
				   (mc_sr << TSAMPLE_RATE_SHIFT));

		/* fill sample resolution.  */
		regmap_update_bits(afe->regmap, MC_I2S_SRR, TRESOLUTION_MASK,
					(I2S_MC_SAMPLE_RESOLUTION_TO_CONFIG(sample_size)
					<< TRESOLUTION_SHIFT));
		/* set tx substream */
		afe->tx_substream = substream;

		if (afe->clk_sync_loop == 0x00) { //0x00=normal,0x01-tx_loop,0x02-rx_loop
			if (true == afe->is_slave) {
			// SND_SOC_DAIFMT_CBM_CFM slave mode
			DEBUG_FUNC_PRT
			regmap_update_bits(afe->regmap, MC_I2S_CTRL,
					   CTRL_T_MS_MASK,
					   (0 << CTRL_T_MS_SHIFT));

			} else {
				// SND_SOC_DAIFMT_CBS_CFS master mode
				DEBUG_FUNC_PRT
				regmap_update_bits(afe->regmap, MC_I2S_CTRL,
						CTRL_T_MS_MASK,
						(1 << CTRL_T_MS_SHIFT));
			}
		} else {
			if (true == afe->is_slave) { // no support slave
				dev_err(afe->dev, "Could not support slave in clk_sync_loop mode.\n");
			} else { //master support two mode

				regmap_update_bits(afe->regmap, MC_I2S_SRR, RSAMPLE_RATE_MASK,
                        (mc_sr << RSAMPLE_RATE_SHIFT));

				/* receive fill sample resolution.  */
				regmap_update_bits(afe->regmap, MC_I2S_SRR, RRESOLUTION_MASK,
						(I2S_MC_SAMPLE_RESOLUTION_TO_CONFIG(sample_size)
						<< RRESOLUTION_SHIFT));

				if(afe->clk_sync_loop == 0x01) {
					DEBUG_FUNC_PRT
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_T_MS_MASK,
							(0 << CTRL_T_MS_SHIFT));
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_R_MS_MASK,
							(1 << CTRL_R_MS_SHIFT));
				} else if(afe->clk_sync_loop == 0x02) {
					DEBUG_FUNC_PRT
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_T_MS_MASK,
							(1 << CTRL_T_MS_SHIFT));
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_R_MS_MASK,
							(0 << CTRL_R_MS_SHIFT));
				}
			}
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {

		/* Here calculate and fill sample rate.  */
		clk_rate = clk_get_rate(afe->clk_i2s);
		mc_sr =
			I2S_MC_SAMPLE_RATE_CALC(clk_rate, srate, I2S_MC_DATA_WIDTH);

		DEBUG_ITEM_PRT(mc_sr);
		regmap_update_bits(afe->regmap, MC_I2S_SRR, RSAMPLE_RATE_MASK,
					(mc_sr << RSAMPLE_RATE_SHIFT));

		/* fill sample resolution.  */
		regmap_update_bits(afe->regmap, MC_I2S_SRR, RRESOLUTION_MASK,
					(I2S_MC_SAMPLE_RESOLUTION_TO_CONFIG(sample_size)
					<< RRESOLUTION_SHIFT));

		/* set rx substream */
		afe->rx_substream = substream;

		if (afe->clk_sync_loop == 0x00) {//0x00=normal,0x01-tx_loop,0x02-rx_loop
			if (true == afe->is_slave) {
				// SND_SOC_DAIFMT_CBM_CFM slave mode
				DEBUG_FUNC_PRT
				regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_R_MS_MASK,
							(0 << CTRL_R_MS_SHIFT));

			} else {
				// SND_SOC_DAIFMT_CBS_CFS master mode
				DEBUG_FUNC_PRT
				regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_R_MS_MASK,
							(1 << CTRL_R_MS_SHIFT));
			}
		} else {
			if (true == afe->is_slave) { // no support slave
				dev_err(afe->dev, "Could not support slave in clk_sync_loop mode.\n");
			} else { //master support two mode

				regmap_update_bits(afe->regmap, MC_I2S_SRR, TSAMPLE_RATE_MASK,
                    (mc_sr << TSAMPLE_RATE_SHIFT));

				/* transmit fill sample resolution.  */
				regmap_update_bits(afe->regmap, MC_I2S_SRR, TRESOLUTION_MASK,
						(I2S_MC_SAMPLE_RESOLUTION_TO_CONFIG(sample_size)
						<< TRESOLUTION_SHIFT));

				if(afe->clk_sync_loop == 0x01) {
					DEBUG_FUNC_PRT
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_T_MS_MASK,
							(0 << CTRL_T_MS_SHIFT));
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_R_MS_MASK,
							(1 << CTRL_R_MS_SHIFT));
				} else if(afe->clk_sync_loop == 0x02) {
					DEBUG_FUNC_PRT
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_T_MS_MASK,
							(1 << CTRL_T_MS_SHIFT));
					regmap_update_bits(afe->regmap, MC_I2S_CTRL,
							CTRL_R_MS_MASK,
							(0 << CTRL_R_MS_SHIFT));
				}
			}
		}
	} else {
		return -EINVAL;
	}

	/* set dir */
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, (afe->tx_slot_mask << CTRL_TR_CFG_SHIFT),
				((afe->tx_slot_mask) << CTRL_TR_CFG_SHIFT));
	regmap_update_bits(afe->regmap, MC_I2S_CTRL, (afe->rx_slot_mask << CTRL_TR_CFG_SHIFT),
				((~afe->rx_slot_mask) << CTRL_TR_CFG_SHIFT));

	regcache_sync(afe->regmap);

	ret = regmap_read(afe->regmap, MC_I2S_SRR, &val);
	dev_info(afe->dev, "DUMP %d MC_I2S_SRR(0x%x)\n", __LINE__, val);

	dev_info(
		afe->dev,
		"%s srate: %u, channels: %u, sample_size: %u, period_size: %u\n",
		__func__, srate, channels, sample_size, period_size);

	return snd_soc_runtime_set_dai_fmt(rtd, rtd->dai_link->dai_fmt);
}

int snd_afe_i2s_mc_dai_prepare(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{

	DEBUG_FUNC_PRT
	// afe_i2s_mc_config(afe);
	return 0;
}

/* snd_afe_dai_trigger */
int snd_afe_i2s_mc_dai_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{

	struct sdrv_afe_i2s_mc *afe = snd_soc_dai_get_drvdata(dai);
	printk(KERN_INFO "%s:%i ------cmd(%d)--------------\n", __func__,
			__LINE__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			afe_i2s_mc_start_playback(afe);
		else
			afe_i2s_mc_start_capture(afe);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			afe_i2s_mc_stop_playback(afe);
		else
			afe_i2s_mc_stop_capture(afe);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}
int snd_afe_i2s_mc_dai_hw_free(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	DEBUG_FUNC_PRT
	return 0;
}
void snd_afe_i2s_mc_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	DEBUG_FUNC_PRT
	/* disable main clk  end of playback or capture*/
}

static int snd_afe_i2s_mc_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct sdrv_afe_i2s_mc *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int val = 0, ret;
	DEBUG_FUNC_PRT
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	if (afe->clk_sync_loop == 0x01) {
		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_TSYNC_LOOP_BACK_MASK,
				   (1 << CTRL_TSYNC_LOOP_BACK_SHIFT));
	} else if (afe->clk_sync_loop == 0x02) {
		regmap_update_bits(afe->regmap, MC_I2S_CTRL,
				   CTRL_RSYNC_LOOP_BACK_MASK,
				   (1 << CTRL_RSYNC_LOOP_BACK_SHIFT));
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	/* codec clk & FRM slave i2s master*/
	case SND_SOC_DAIFMT_CBS_CFS:
		/*config master mode*/
		DEBUG_ITEM_PRT(dai->capture_active);
		DEBUG_ITEM_PRT(dai->playback_active);
		afe->is_slave = false;
		break;
	/* codec clk & FRM master i2s slave*/
	case SND_SOC_DAIFMT_CBM_CFM:
		DEBUG_ITEM_PRT(dai->capture_active);
		DEBUG_ITEM_PRT(dai->playback_active);
		/*config slave mode*/
		afe->is_slave = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
		DEBUG_FUNC_PRT

		regmap_write(afe->regmap, MC_I2S_DEV_CONF, 0x208 | afe->clk_edge);
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		DEBUG_FUNC_PRT
		regmap_write(afe->regmap, MC_I2S_DEV_CONF, 0x69a | afe->clk_edge);
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		DEBUG_FUNC_PRT
		regmap_write(afe->regmap, MC_I2S_DEV_CONF, 0x492 | afe->clk_edge);
		break;

	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		DEBUG_FUNC_PRT
		break;

	default:
		return -EINVAL;
	}
	regcache_sync(afe->regmap);

	ret = regmap_read(afe->regmap, MC_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d MC_I2S_CTRL(0x%x)\n", __LINE__, val);

	ret = regmap_read(afe->regmap, MC_I2S_DEV_CONF, &val);
	dev_dbg(afe->dev, "DUMP %d MC_I2S_DEV_CONF(0x%x)\n", __LINE__, val);
	return 0;
}

#if 0
static int snd_afe_i2s_mc_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
				unsigned int rx_mask, int slots, int slot_width)
{
	struct sdrv_afe_i2s_mc *afe = snd_soc_dai_get_drvdata(dai);

	/* config slot width */
	/* i2s mc has a fixed slot width which is 32-bit wide */
	afe->slot_width = slot_width;

	/* config channel mask*/
	afe->slots = slots;

	DEBUG_FUNC_PRT
	return 0;
}
#endif

static struct snd_soc_dai_ops snd_afe_i2s_mc_dai_ops = {
	.startup = snd_afe_i2s_mc_dai_startup,
	.shutdown = snd_afe_i2s_mc_dai_shutdown,
	.hw_params = snd_afe_i2s_mc_dai_hw_params,
	.hw_free = snd_afe_i2s_mc_dai_hw_free,
	.prepare = snd_afe_i2s_mc_dai_prepare,
	.trigger = snd_afe_i2s_mc_dai_trigger,
	.set_fmt = snd_afe_i2s_mc_dai_set_fmt,
//	.set_tdm_slot = snd_afe_i2s_mc_set_tdm_slot,
};

int snd_soc_i2s_mc_dai_probe(struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_mc *d = snd_soc_dai_get_drvdata(dai);
	struct device *dev = d->dev;

	dev_info(dev, "DAI probe.----------------------------\n");

	/* 	struct zx_i2s_info *zx_i2s = dev_get_drvdata(dai->dev);
	snd_soc_dai_init_dma_data(dai, &d->playback_dma_data,
		&d->capture_dma_data); */

	dai->capture_dma_data = &d->capture_dma_data;
	dai->playback_dma_data = &d->playback_dma_data;

	return 0;
}

int snd_soc_i2s_mc_dai_remove(struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_mc *d = snd_soc_dai_get_drvdata(dai);
	struct device *dev = d->dev;

	dev_info(dev, "DAI remove.\n");

	return 0;
}

static struct snd_soc_dai_driver snd_afe_dais[] = {
	{
	.name = "snd-afe-mc-i2s-dai0",
	.probe = snd_soc_i2s_mc_dai_probe,
	.remove = snd_soc_i2s_mc_dai_remove,
	.playback =
		{
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = 1,
		.channels_max = 8,
		},
	.capture =
	{
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = 1,
		.channels_max = 4,
	},
	.ops = &snd_afe_i2s_mc_dai_ops,
	},
	{
	.name = "snd-afe-mc-i2s-dai1",
	.probe = snd_soc_i2s_mc_dai_probe,
	.remove = snd_soc_i2s_mc_dai_remove,
	.capture =
		{
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = 1,
		.channels_max = 4,
		},
	.ops = &snd_afe_i2s_mc_dai_ops,
	},
};

static const struct snd_soc_component_driver snd_sample_soc_component = {
	.name = DRV_NAME "-i2s-component",
};

static int sdrv_pcm_prepare_slave_config(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params,
					   struct dma_slave_config *slave_config)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdrv_afe_i2s_mc *afe = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	int ret;
	DEBUG_FUNC_PRT;

	ret = snd_hwparams_to_dma_slave_config(substream, params, slave_config);
	if (ret)
		return ret;

	slave_config->dst_maxburst = 4;
	slave_config->src_maxburst = 4;

	slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		slave_config->dst_addr = afe->playback_dma_data.addr;
	else
		slave_config->src_addr = afe->capture_dma_data.addr;
	DEBUG_FUNC_PRT;
	return 0;
}

static const struct snd_dmaengine_pcm_config sdrv_dmaengine_pcm_config = {
	.pcm_hardware = &sdrv_pcm_hardware,
	.prepare_slave_config = sdrv_pcm_prepare_slave_config,
	.prealloc_buffer_size = MAX_I2S_MC_ABUF_SIZE,
};

static int snd_i2s_mc_soc_platform_probe(struct snd_soc_component *pdev)
{
	struct device *dev = pdev->dev;
	dev_info(dev, "Added-----------%s:%i \n", __func__, __LINE__);

	return 0;
}

static void snd_i2s_mc_soc_platform_remove(struct snd_soc_component *pdev)
{
	struct device *dev = pdev->dev;
	dev_info(dev, "Removed-----------%s:%i \n", __func__, __LINE__);
	// snd_soc_unregister_platform(&pdev->dev);
	return;
}

static struct snd_soc_component_driver snd_afe_soc_component = {

	.probe = snd_i2s_mc_soc_platform_probe,
	.remove = snd_i2s_mc_soc_platform_remove,
	/*   .pcm_new  = snd_afe_pcm_new,
	.pcm_free = snd_afe_pcm_free,
	.ops = &dummy_pcm_ops,
	.name = DRV_NAME, */
};

static int snd_afe_i2s_mc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_afe_i2s_mc *afe;
	int ret;
	unsigned int value;
	struct resource *res;
	unsigned int irq_id;
	dev_info(dev, "Probed.\n");

	afe = devm_kzalloc(dev, sizeof(struct sdrv_afe_i2s_mc), GFP_KERNEL);
	if (afe == NULL)
		return -ENOMEM;

	afe->pdev = pdev;
	afe->dev = dev;
	afe->loopback_mode = false;

	platform_set_drvdata(pdev, afe);

	/* get clk and mclk */
	afe->clk_i2s = devm_clk_get(&pdev->dev, "i2s-mc-clk");
	if (IS_ERR(afe->clk_i2s))
		return PTR_ERR(afe->clk_i2s);
	DEBUG_ITEM_PRT(clk_get_rate(afe->clk_i2s));

	afe->mclk = devm_clk_get(&pdev->dev, "i2s-mc-mclk");
	if (IS_ERR(afe->mclk))
		return PTR_ERR(afe->mclk);
	DEBUG_ITEM_PRT(clk_get_rate(afe->mclk));

	ret = clk_prepare_enable(afe->mclk);
	if (ret) {
		dev_err(dev, "mclk clk_prepare_enable failed: %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(afe->clk_i2s);
	if (ret) {
		dev_err(dev, "clk_i2s clk_prepare_enable failed: %d\n", ret);
		return ret;
	}

	/* Get irq and setting */
	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_err(afe->dev, "%s no irq\n", dev->of_node->name);
		return -ENXIO;
	}
	DEBUG_ITEM_PRT(irq_id);

	ret = devm_request_irq(afe->dev, irq_id, sdrv_i2s_mc_irq_handler, 0,
				   pdev->name, (void *)afe);
	/*  ret = devm_request_threaded_irq(&pdev->dev, irq_id, NULL,
									sdrv_i2s_mc_irq_handler,
									IRQF_SHARED
	| IRQF_ONESHOT, pdev->name, afe); if (ret < 0)
	{
		dev_err(afe->dev, "Could not request irq.\n");
		return ret;
	} */
	/* Get register and print */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->regs = devm_ioremap_resource(afe->dev, res);
	if (IS_ERR(afe->regs)) {
		ret = PTR_ERR(afe->regs);
		goto err_disable;
	}
	DEBUG_ITEM_PRT(afe->regs);
	printk(KERN_INFO "ALSA X9"
			" irq(%d) num(%d) vaddr(0x%llx) paddr(0x%llx) \n",
			irq_id, pdev->num_resources, afe->regs, res->start);
	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->regs,
						&sdrv_i2s_mc_regmap_config);

	if (IS_ERR(afe->regmap)) {
		ret = PTR_ERR(afe->regmap);
		goto err_disable;
	}

	afe->tx_slot_mask  = 0;
	afe->rx_slot_mask  = 0;
	afe->clk_sync_loop = 0;
	afe->clk_edge      = 1;
	afe->slot_width    = 16;
	afe->slots         = 2;

	ret = device_property_read_u32(dev, "sdrv,rx-tx-slot-mask", &value);
	if(!ret) {
		afe->tx_slot_mask =  value & 0xFF;
		afe->rx_slot_mask =  (value >> 8) & 0xFF;
	}

	ret = device_property_read_u32(dev, "sdrv,tx-rx-sck-loop", &value);
	if(!ret) {
		afe->clk_sync_loop = value;
	}

	ret = device_property_read_u32(dev, "sdrv,master-sck-polar", &value);
	if(!ret) {
		afe->clk_edge = value;
	}

	/* 	Set register map to cache bypass mode. */
	/* regcache_cache_bypass(afe->regmap, true); */

	/*FIXME: Need change addr_width by different type. */
	afe->playback_dma_data.addr = res->start + I2S_MC_FIFO_OFFSET;
	afe->playback_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	afe->playback_dma_data.maxburst = 4;

	afe->capture_dma_data.addr = res->start + I2S_MC_FIFO_OFFSET;
	afe->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	afe->capture_dma_data.maxburst = 4;

	ret = devm_snd_dmaengine_pcm_register(dev, &sdrv_dmaengine_pcm_config, 0);
	if (ret)
		goto err_disable;

	ret = devm_snd_soc_register_component(dev, &snd_afe_soc_component,
						  snd_afe_dais,
						  ARRAY_SIZE(snd_afe_dais));
	if (ret < 0) {
		dev_err(&pdev->dev, "couldn't register component\n");
		goto err_disable;
	}

	afe_i2s_mc_config(afe);

	return 0;
err_disable:
	/* pm_runtime_disable(&pdev->dev); */
	return ret;
}

static int snd_afe_i2s_mc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_afe_i2s_mc *afe = platform_get_drvdata(pdev);
	dev_info(dev, "Removed.\n");
	regmap_exit(afe->regmap);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id sdrv_i2s_mc_of_match[] = {
    {
	.compatible = "semidrive,x9-i2s-mc",
    },
    {},
};

#ifdef CONFIG_PM_SLEEP
static int sdrv_i2s_mc_suspend(struct device *dev)
{
	int ret;
	struct sdrv_afe_i2s_mc *afe = dev_get_drvdata(dev);

	dev_info(afe->dev, "%s\n", __func__);

	ret = regmap_read(afe->regmap, MC_I2S_CTRL,
			  &afe->reg_mc_i2s_ctrl);
	ret = regmap_read(afe->regmap, MC_I2S_INTR_STAT,
			  &afe->reg_mc_i2s_intr_stat);
	ret = regmap_read(afe->regmap, MC_I2S_SRR,
			  &afe->reg_mc_i2s_srr);
	ret = regmap_read(afe->regmap, MC_I2S_CID_CTRL,
			  &afe->reg_mc_i2s_cid_ctrl);
	ret = regmap_read(afe->regmap, MC_I2S_TFIFO_STAT,
			  &afe->reg_mc_i2s_tfifo_stat);
	ret = regmap_read(afe->regmap, MC_I2S_RFIFO_STAT,
			  &afe->reg_mc_i2s_rfifo_stat);
	ret = regmap_read(afe->regmap, MC_I2S_TFIFO_CTRL,
			  &afe->reg_mc_i2s_tfifo_ctrl);
	ret = regmap_read(afe->regmap, MC_I2S_RFIFO_CTRL,
			  &afe->reg_mc_i2s_rfifo_ctrl);
	ret = regmap_read(afe->regmap, MC_I2S_DEV_CONF,
			  &afe->reg_mc_i2s_dev_conf);
	ret = regmap_read(afe->regmap, MC_I2S_POLL_STAT,
			  &afe->reg_mc_i2s_poll_stat);

	clk_disable_unprepare(afe->mclk);
	clk_disable_unprepare(afe->clk_i2s);

	return 0;
}

static int sdrv_i2s_mc_resume(struct device *dev)
{
	int ret;
	struct sdrv_afe_i2s_mc *afe = dev_get_drvdata(dev);

	dev_info(afe->dev, "%s\n", __func__);

	ret = clk_prepare_enable(afe->mclk);
	if (ret) {
		dev_err(dev, "i2s mc mclk clk_prepare_enable failed: %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(afe->clk_i2s);
	if (ret) {
		dev_err(dev, "i2s mc clk_i2s clk_prepare_enable failed: %d\n", ret);
		return ret;
	}

	regmap_update_bits(afe->regmap, MC_I2S_CTRL, CTRL_SFR_RST_MASK,
			   (0 << CTRL_SFR_RST_SHIFT));

	ret = regmap_write(afe->regmap, MC_I2S_INTR_STAT,
			  afe->reg_mc_i2s_intr_stat);
	ret = regmap_write(afe->regmap, MC_I2S_SRR,
			  afe->reg_mc_i2s_srr);
	ret = regmap_write(afe->regmap, MC_I2S_CID_CTRL,
			  afe->reg_mc_i2s_cid_ctrl);
	ret = regmap_write(afe->regmap, MC_I2S_TFIFO_STAT,
			  afe->reg_mc_i2s_tfifo_stat);
	ret = regmap_write(afe->regmap, MC_I2S_RFIFO_STAT,
			  afe->reg_mc_i2s_rfifo_stat);
	ret = regmap_write(afe->regmap, MC_I2S_TFIFO_CTRL,
			  afe->reg_mc_i2s_tfifo_ctrl);
	ret = regmap_write(afe->regmap, MC_I2S_RFIFO_CTRL,
			  afe->reg_mc_i2s_rfifo_ctrl);
	ret = regmap_write(afe->regmap, MC_I2S_DEV_CONF,
			  afe->reg_mc_i2s_dev_conf);
	ret = regmap_write(afe->regmap, MC_I2S_POLL_STAT,
			  afe->reg_mc_i2s_poll_stat);
	ret = regmap_write(afe->regmap, MC_I2S_CTRL,
			  afe->reg_mc_i2s_ctrl);

	return 0;
}

static const struct dev_pm_ops i2s_mc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdrv_i2s_mc_suspend, sdrv_i2s_mc_resume)
};

#endif /* CONFIG_PM_SLEEP */

static struct platform_driver snd_afe_i2s_mc_driver = {
	.driver =
	{
		.name = DRV_NAME "-i2s",
		.of_match_table = sdrv_i2s_mc_of_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &i2s_mc_pm_ops,
#endif
	},
	.probe = snd_afe_i2s_mc_probe,
	.remove = snd_afe_i2s_mc_remove,
};

module_platform_driver(snd_afe_i2s_mc_driver);
MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 I2S SC ALSA SoC device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 i2s asoc device");
