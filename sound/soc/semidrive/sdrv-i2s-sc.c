/*
 * sdrv-i2s-sc.c
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
#include "sdrv-i2s-sc.h"
#include "sdrv-common.h"
#include <linux/clk.h>
#include <linux/delay.h>
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

#define DRV_NAME "snd-afe-i2s-sc"
#define SDRV_I2S_FIFO_SIZE 2048

/* Define a fack buffer for dummy data copy */
static bool fake_buffer = 0;
module_param(fake_buffer, bool, 0444);
MODULE_PARM_DESC(fake_buffer, "Fake buffer allocations.");

static bool full_duplex = true;
module_param(full_duplex, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(full_duplex, "Set to full duplex mode (default: true)");

/**
 * @brief input slot_width such as 32.
 * 		  return enum CDN_CHN_WIDTH_32BITS
 *
 */
static int check_and_get_slot_width_enum(int slot_width){
	int i;
	for (i = 0; i < CDN_CHN_WIDTH_NUMB; i++) {
		if (slot_width == ChnWidthTable[i]) {
			return i;
		}
	}
	printk(KERN_ERR "can't support this slot_width %d use 32 CDN_CHN_WIDTH_32BITS\n", slot_width);
	return AFE_I2S_CHN_WIDTH;
}


/* misc operation */

static void afe_i2s_sc_config(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;
	/*Disable i2s*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (0 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	/*SFR reset*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_SFR_RST,
			   (0 << I2S_CTRL_SFR_RST_FIELD_OFFSET));

	/*FIFO reset*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_RST,
			   (0 << I2S_CTRL_FIFO_RST_FIELD_OFFSET));

	if (true == afe->is_full_duplex) {

		/*Reset full duplex FIFO reset*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_FIFO_RST,
				   (0 << I2S_CTRL_FDX_FIFO_RST_FIELD_OFFSET));
		/*Enable full-duplex*/
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_FULL_DUPLEX,
		    (1 << I2S_CTRL_FDX_FULL_DUPLEX_FIELD_OFFSET));
		/*Disable tx sdo*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_I2S_FTX_EN,
				   (0 << I2S_CTRL_FDX_I2S_FTX_EN_FIELD_OFFSET));
		/*Disable rx sdi*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_I2S_FRX_EN,
				   (0 << I2S_CTRL_FDX_I2S_FRX_EN_FIELD_OFFSET));

		/*Unmask full-duplex mode interrupt request.*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
				   BIT_CTRL_FDX_RI2S_MASK,
				   (1 << I2S_CTRL_FDX_RI2S_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes empty. */
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_EMPTY_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_EMPTY_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes almost empty.*/
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_AEMPTY_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_AEMPTY_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes full. */
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_FULL_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_FULL_MASK_FIELD_OFFSET));
		/*Mask full-duplex receive FIFO becomes almost full. */
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		    BIT_CTRL_FDX_RFIFO_AFULL_MASK,
		    (0 << I2S_CTRL_FDX_RFIFO_AFULL_MASK_FIELD_OFFSET));
	}
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (0 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	/*config direction of transmission:Tx*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_DIR_CFG,
			   (1 << I2S_CTRL_DIR_CFG_FIELD_OFFSET));

	/*config total data width*/
	regmap_update_bits(
	    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, BIT_CTRL_CHN_WIDTH,
	    (afe->slot_width << I2S_CTRL_CHN_WIDTH_FIELD_OFFSET));

	/*config almost empty fifo level:tx fifo level */
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, 0x30);
	/*config almost full fifo level:defualt value*/
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL, 0x60);

	/*config resolution,valid number of data bit*/
	if (true == afe->is_full_duplex) {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR, 0xf);
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, 0xf);
		/*config almost empty fifo level:tx fifo level */
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY_FDR,
			     0x30);
		/*config almost full fifo level:defualt value*/
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL_FDR,
			     0x60);
	} else {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, 0xf);
	}


	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL 0x%x:(0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_I2S_CTRL, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, &val);
	dev_dbg(afe->dev,
		"DUMP %d REG_CDN_I2SSC_REGS_FIFO_AEMPTY 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_FIFO_AEMPTY, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL, &val);
	dev_dbg(afe->dev,
		"DUMP %d REG_CDN_I2SSC_REGS_FIFO_AFULL 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_FIFO_AFULL, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_SRES 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_I2S_SRES, val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR, &val);
	dev_dbg(afe->dev,
		"DUMP %d REG_CDN_I2SSC_REGS_TDM_FD_DIR 0x%x: (0x%x)\n",
		__LINE__, REG_CDN_I2SSC_REGS_TDM_FD_DIR, val);
}

static void afe_i2s_sc_start_capture(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, cnt, val = 0;
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT, 0);
	/*Enable rx sdi*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_I2S_FRX_EN,
			   (1 << I2S_CTRL_FDX_I2S_FRX_EN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (1 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	/*Set interrupt request i2s fdx */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RI2S_MASK,
			   (1 << I2S_CTRL_FDX_RI2S_MASK_FIELD_OFFSET));

	/*Set all interrupt request generation */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_MASK,
			   (1 << I2S_CTRL_I2S_MASK_FIELD_OFFSET));


	/*Clear fifo almost full mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_AFULL_MASK,
			   (0 << I2S_CTRL_FIFO_AFULL_MASK_FIELD_OFFSET));

	/*Clear fifo afull mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RFIFO_AFULL_MASK,
			   (0 << I2S_CTRL_FDX_RFIFO_AFULL_MASK_FIELD_OFFSET));

	/*Clear I2S Overrun status*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			   BIT_STAT_RDATA_OVERR,
			   (0 << I2S_STAT_RDATA_OVERR_FIELD_OFFSET));

	/*Clear I2S UNDERR status*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			   BIT_STAT_TDATA_UNDERR,
			   (0 << I2S_STAT_TDATA_UNDERR_FIELD_OFFSET));


	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_INTREQ_MASK,
			   (1 << I2S_CTRL_INTREQ_MASK_FIELD_OFFSET));

	if ((afe->tdm_initialized == true) &&
	    (afe->tdm_use_rx_last_slot == true)) {
		cnt = 0;
		if (atomic_read(&afe->playing) && (afe->is_slave == true) &&
		    (BIT_(afe->slots - 1) & afe->rx_slot_mask)) {
			while (cnt < 500) {
				cnt++;
				ret = regmap_read(
					afe->regmap,
					REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR,
					&val);
				if (val > 0) {
					/* Only read one time */
					ioread32((afe->regs +
						  I2S_SC_FIFO_OFFSET));
					break;
				}
			}
		}
		if (cnt > 300) {
			dev_warn(
				afe->dev,
				"DUMP %d REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR(0x%x) times(0x%x)\n",
				__LINE__, val, cnt);
		}
	}

	return;
}

static void afe_i2s_sc_stop_capture(struct sdrv_afe_i2s_sc *afe)
{
	/*Enable rx sdi*/
	u32 ret, val, cnt;

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_I2S_FRX_EN,
			   (0 << I2S_CTRL_FDX_I2S_FRX_EN_FIELD_OFFSET));
	udelay(30);

 	cnt = 0;
	while (cnt < I2S_SC_RX_DEPTH * 2) {
		if (true == afe->is_full_duplex) {
			ret = regmap_read(afe->regmap,
					  REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR,
					  &val);
		} else {
			ret = regmap_read(afe->regmap,
					  REG_CDN_I2SSC_REGS_FIFO_LEVEL, &val);
		}
		if (ret == 0) {
			if (val == 0) {
				break;
			}
		}
		ioread32((afe->regs + I2S_SC_FIFO_OFFSET));
		cnt++;
	}
	/*Reset full duplex FIFO reset*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_FIFO_RST,
			   (0 << I2S_CTRL_FDX_FIFO_RST_FIELD_OFFSET));
	return;

}

static void afe_i2s_sc_start_playback(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;

	/*Set interrupt request i2s fdx */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_RI2S_MASK,
			   (1 << I2S_CTRL_FDX_RI2S_MASK_FIELD_OFFSET));

	/*Set all interrupt request generation */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_MASK,
			   (1 << I2S_CTRL_I2S_MASK_FIELD_OFFSET));

	/*Set fifo almost empty mask */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_FIFO_AEMPTY_MASK,
			   (0 << I2S_CTRL_FIFO_AEMPTY_MASK_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			   BIT_STAT_FIFO_EMPTY,
			   (0 << I2S_STAT_TDATA_UNDERR_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			   BIT_STAT_FIFO_AEMPTY,
			   (0 << I2S_STAT_TDATA_UNDERR_FIELD_OFFSET));
	/*Clear I2S UNDERR status*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			   BIT_STAT_TDATA_UNDERR,
			   (0 << I2S_STAT_TDATA_UNDERR_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_INTREQ_MASK,
			   (1 << I2S_CTRL_INTREQ_MASK_FIELD_OFFSET));

	/*Enable tx sdo*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_I2S_FTX_EN,
			   (1 << I2S_CTRL_FDX_I2S_FTX_EN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (1 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);

}

static void afe_i2s_sc_stop_playback(struct sdrv_afe_i2s_sc *afe)
{

	u32 ret, val, cnt, delay_cnt;
	cnt = 0;

	/* (1000000/10)  * I2S_SC_TX_DEPTH / (afe->srate * afe->tx_channels) + 1*/
	delay_cnt = 100000 * I2S_SC_TX_DEPTH / (afe->srate * afe->tx_channels) + 1;
	while (cnt < delay_cnt) {
		ret =
		    regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT, &val);
		if (ret >= 0) {
			if (BIT_STAT_FIFO_EMPTY & val) {
				break;
			}
		}
		udelay(10);
		cnt++;
	}
	ret = regmap_read(afe->regmap,
					  REG_CDN_I2SSC_REGS_FIFO_LEVEL, &val);
	if (val > 0){
		dev_err(afe->dev, "delay %d , afe->tx_channels %d level %d \n", delay_cnt,afe->tx_channels,val);
	}
	/*Disable tx sdi*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			   BIT_CTRL_FDX_I2S_FTX_EN,
			   (0 << I2S_CTRL_FDX_I2S_FTX_EN_FIELD_OFFSET));
	udelay(30);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT, 0);

}

static void afe_i2s_sc_stop(struct sdrv_afe_i2s_sc *afe)
{
	u32 ret, val;

	/*Clear I2S status*/
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT, 0);
	/*Disable i2s*/
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			   BIT_CTRL_I2S_EN,
			   (0 << I2S_CTRL_I2S_EN_FIELD_OFFSET));

	DEBUG_FUNC_PRT
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);
}

/*
 * dummy buffer handling
 */

static void *dummy_page[2];

static void free_fake_buffer(void)
{
	if (fake_buffer) {
		int i;
		for (i = 0; i < 2; i++)
			if (dummy_page[i]) {
				free_page((unsigned long)dummy_page[i]);
				dummy_page[i] = NULL;
			}
	}
}

static int alloc_fake_buffer(void)
{
	int i;

	if (!fake_buffer)
		return 0;
	for (i = 0; i < 2; i++) {
		dummy_page[i] = (void *)get_zeroed_page(GFP_KERNEL);
		if (!dummy_page[i]) {
			free_fake_buffer();
			return -ENOMEM;
		}
	}
	return 0;
}

/* x9 pcm hardware definition.  */

static const struct snd_pcm_hardware sdrv_pcm_hardware = {

    .info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
	     SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER),
    .formats = SND_FORMATS,
    .rates = SND_RATE,
    .rate_min = SND_RATE_MIN,
    .rate_max = SND_RATE_MAX,
    .channels_min = SND_CHANNELS_MIN,
    .channels_max = SND_CHANNELS_MAX,
    .period_bytes_min = MIN_PERIOD_SIZE,
    .period_bytes_max = MAX_PERIOD_SIZE,
    .periods_min = MIN_PERIODS,
    .periods_max = MAX_PERIODS,
    .buffer_bytes_max = MAX_ABUF_SIZE,
    .fifo_size = 0,
};

static int sdrv_snd_pcm_open(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	DEBUG_FUNC_PRT;
	snd_soc_set_runtime_hwparams(substream, &sdrv_pcm_hardware);
	runtime->private_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	return 0;
}

static int sdrv_snd_pcm_close(struct snd_pcm_substream *substream)
{
	DEBUG_FUNC_PRT;
	synchronize_rcu();
	return 0;
}
static int sdrv_snd_pcm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{

	struct snd_pcm_runtime *runtime;
	runtime = substream->runtime;

	DEBUG_FUNC_PRT;
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int sdrv_snd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	DEBUG_FUNC_PRT;
	return snd_pcm_lib_free_pages(substream);
}

static int sdrv_snd_pcm_prepare(struct snd_pcm_substream *substream)
{

	int ret = 0;
	DEBUG_FUNC_PRT;
	return ret;
}
/*This function for pcm */
static int sdrv_snd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sdrv_afe_i2s_sc *afe = runtime->private_data;
	DEBUG_FUNC_PRT;

	/* 	printk("pcm:buffer_size = %ld,"
		"dma_area = %p, dma_bytes = %zu reg = %p PeriodsSZ(%d)(%d)\n ",
		runtime->buffer_size, runtime->dma_area, runtime->dma_bytes,
	   afe->regs,runtime->periods,runtime->period_size);
 	*/
	printk(KERN_INFO "%s:%i ------cmd(%d)--------------\n", __func__,
	       __LINE__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		DEBUG_FUNC_PRT;
		ACCESS_ONCE(afe->tx_ptr) = 0;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_i2s_sc_start_playback(afe);
			atomic_set(&afe->playing, 1);
		} else {
			afe_i2s_sc_start_capture(afe);
			atomic_set(&afe->capturing, 1);
		}
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			atomic_set(&afe->playing, 0);
			afe_i2s_sc_stop_playback(afe);
		} else {
			atomic_set(&afe->capturing, 0);
			afe_i2s_sc_stop_capture(afe);
		}
		if (!atomic_read(&afe->playing) &&
		    !atomic_read(&afe->capturing)) {
			 afe_i2s_sc_stop(afe);
		}
		return 0;
	}
	return -EINVAL;
}

static snd_pcm_uframes_t
sdrv_snd_pcm_pointer(struct snd_pcm_substream *substream)
{
	// DEBUG_FUNC_PRT;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sdrv_afe_i2s_sc *afe = runtime->private_data;
	snd_pcm_uframes_t pos = ACCESS_ONCE(afe->tx_ptr);

	return pos < runtime->buffer_size ? pos : 0;
}

static struct snd_pcm_ops sdrv_snd_pcm_ops = {
    .open = sdrv_snd_pcm_open,
    .close = sdrv_snd_pcm_close,
    .ioctl = snd_pcm_lib_ioctl,
    .hw_params = sdrv_snd_pcm_hw_params,
    .hw_free = sdrv_snd_pcm_hw_free,
    .prepare = sdrv_snd_pcm_prepare,
    .trigger = sdrv_snd_pcm_trigger,
    .pointer = sdrv_snd_pcm_pointer,
};

/* 	TODO: here setup dai */
/* Set up irq handler
 * ------------------------------------------------------------ */
static irqreturn_t sdrv_i2s_sc_irq_handler(int irq, void *dev_id)
{
	struct sdrv_afe_i2s_sc *afe = dev_id;
	unsigned int i2s_sc_status;
	struct snd_pcm_runtime *runtime;
	int ret;

	runtime = afe->tx_substream->runtime;
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			  &i2s_sc_status);
	/* clear irq here*/
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT, 0);

	/* printk("pcm:buffer_size = %ld,"
		"dma_area = %p, dma_bytes = %zu reg = %p PeriodsSZ(%d)(%d)
	   I2S_STAT(0x%x)\n ", runtime->buffer_size, runtime->dma_area,
	   runtime->dma_bytes,
	   afe->regs,runtime->periods,runtime->period_size,i2s_sc_status);
    */
	if (ret) {
		dev_err(afe->dev, "%s irq status err\n", __func__);
		return IRQ_HANDLED;
	}

	if (i2s_sc_status & BIT_STAT_FIFO_AEMPTY) {
		dev_info(afe->dev, "i2s sc almost empty! \n");
	}

	if (i2s_sc_status & BIT_STAT_FIFO_EMPTY) {
	}
	if (i2s_sc_status & BIT_STAT_FIFO_AFULL) {
		dev_info(afe->dev, "i2s sc almost full! \n");
	}

	if (i2s_sc_status & BIT_STAT_RDATA_OVERR) {
		if (atomic_read(&afe->capturing)) {
			dev_dbg(afe->dev, "i2s sc overrun! \n");
			snd_pcm_period_elapsed(afe->rx_substream);
		}
	}

	if (i2s_sc_status & BIT_STAT_TDATA_UNDERR) {
		if (atomic_read(&afe->playing)){
			dev_dbg(afe->dev, "i2s sc underrun! \n");
			snd_pcm_period_elapsed(afe->tx_substream);
		}
	}

	return IRQ_HANDLED;
}

/* -Set up dai regmap
 * here----------------------------------------------------------- */
static bool sdrv_i2s_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_CDN_I2SSC_REGS_I2S_CTRL:
	case REG_CDN_I2SSC_REGS_I2S_CTRL_FDX:
	case REG_CDN_I2SSC_REGS_I2S_SRES:
	case REG_CDN_I2SSC_REGS_I2S_SRES_FDR:
	case REG_CDN_I2SSC_REGS_I2S_SRATE:
	case REG_CDN_I2SSC_REGS_I2S_STAT:
	case REG_CDN_I2SSC_REGS_FIFO_LEVEL:
	case REG_CDN_I2SSC_REGS_FIFO_AEMPTY:
	case REG_CDN_I2SSC_REGS_FIFO_AFULL:
	case REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR:
	case REG_CDN_I2SSC_REGS_FIFO_AEMPTY_FDR:
	case REG_CDN_I2SSC_REGS_FIFO_AFULL_FDR:
	case REG_CDN_I2SSC_REGS_TDM_CTRL:
	case REG_CDN_I2SSC_REGS_TDM_FD_DIR:
		return true;
	default:
		return false;
	}
}

/*  Setup i2s regmap */
static const struct regmap_config sdrv_i2s_sc_regmap_config = {
    .reg_bits = 32,
    .reg_stride = 4,
    .val_bits = 32,
    .max_register = REG_CDN_I2SSC_REGS_TDM_FD_DIR + 0x39,
    .writeable_reg = sdrv_i2s_wr_rd_reg,
    .readable_reg = sdrv_i2s_wr_rd_reg,
    .volatile_reg = sdrv_i2s_wr_rd_reg,
    .cache_type = REGCACHE_NONE,
};

int snd_afe_dai_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{

	/* struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai; */
	/* TODO: enable main clk */
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);

	DEBUG_FUNC_PRT;

	snd_soc_dai_set_dma_data(dai, substream, afe);

	return 0;
}

static int ch_mask[]={0x0,0x1,0x3,0x7,0xf,0x1f,0x3f,0x7f,0xff};

static void snd_afe_dai_ch_cfg(struct sdrv_afe_i2s_sc *afe, int stream, unsigned ch_numb)
{


	afe->slots = max(ch_numb,afe->slots);
	dev_info(afe->dev, "%s:%i : afe->slots (0x%x) ch_numb(0x%x) afe->slot_width(0x%x) \n", __func__,
	       __LINE__,afe->slots , ch_numb, afe->slot_width);
	/* Enable tdm mode */


	if(stream == SNDRV_PCM_STREAM_PLAYBACK){

		afe->tx_slot_mask = ch_mask[ch_numb];
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
		    BIT_TDM_FD_DIR_CH_TX_EN,
		    (afe->tx_slot_mask << TDM_FD_DIR_CH0_TXEN_FIELD_OFFSET));

	}else if(stream == SNDRV_PCM_STREAM_CAPTURE){

		afe->rx_slot_mask = ch_mask[ch_numb];
		regmap_update_bits(
		    afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
		    BIT_TDM_FD_DIR_CH_RX_EN,
		    (afe->rx_slot_mask << TDM_FD_DIR_CH0_RXEN_FIELD_OFFSET));

	}else{
		dev_err(afe->dev, "%s(%d) Don't support this stream: %d\n",
				__func__,__LINE__,stream);
	}

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CHN_NUMB,
				   ((afe->slots-1) << TDM_CTRL_CHN_NO_FIELD_OFFSET));

	/* Set enabled number  */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_CH_EN,
				   (ch_mask[afe->slots] << TDM_CTRL_CH0_EN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (1 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

	//ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL, &val);
	//dev_dbg(afe->dev, "DUMP REG_CDN_I2SSC_REGS_TDM_CTRL(0x%x)\n", val);
}

int snd_afe_dai_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *hwparam,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	unsigned srate = params_rate(hwparam);
	unsigned channels = params_channels(hwparam);
	unsigned period_size = params_period_size(hwparam);
	unsigned sample_size = snd_pcm_format_width(params_format(hwparam));

/* 	dev_info(afe->dev, "%s(%d) afe_reg(0x%llx) afe(0x%llx) rtd(0x%llx) dai(0x%llx) %d ,cpu_dai name %s.\n",
				__func__,__LINE__, afe->regs, afe, rtd, dai, afe->tdm_initialized, rtd->cpu_dai->name); */
	/* 	unsigned freq, ratio, level;
		int err; */
	/*Filter TDM setting*/
	afe->srate = srate;
	if(afe->tdm_initialized == false){
		if (params_channels(hwparam) == 2) {
			/*config data channel stereo*/
			/* DEBUG_FUNC_PRT */
			/* Disable tdm mode */
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (0 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_AUDIO_MODE,
					(0 << I2S_CTRL_AUDIO_MODE_FIELD_OFFSET));
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_MONO_MODE,
					(0 << I2S_CTRL_MONO_MODE_FIELD_OFFSET));
			/*set to pack mode*/
			if(afe->pack_en == true){
				regmap_update_bits(
						afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
						BIT_CTRL_LR_PACK,
						(1 << I2S_CTRL_LR_PACK_FIELD_OFFSET));
			}else{
				regmap_update_bits(
						afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
						BIT_CTRL_LR_PACK,
						(0 << I2S_CTRL_LR_PACK_FIELD_OFFSET));
			}

		} else if (params_channels(hwparam) == 1) {
			/*config data channel mono
			DEBUG_FUNC_PRT*/

			/* Disable tdm mode */
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (0 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_AUDIO_MODE,
					(1 << I2S_CTRL_AUDIO_MODE_FIELD_OFFSET));
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_MONO_MODE,
					(0 << I2S_CTRL_MONO_MODE_FIELD_OFFSET));
			/*clear pack mode*/
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_LR_PACK,
					(0 << I2S_CTRL_LR_PACK_FIELD_OFFSET));

		} else {
			/* Multi channels */
			DEBUG_FUNC_PRT
			/*clear pack mode*/
			regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_LR_PACK,
					(0 << I2S_CTRL_LR_PACK_FIELD_OFFSET));
			snd_afe_dai_ch_cfg(afe, substream->stream, params_channels(hwparam));
		}
	}else{
		/* Already set to tdm by snd_afe_set_dai_tdm_slot */
		/*clear pack mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
					BIT_CTRL_LR_PACK,
					(0 << I2S_CTRL_LR_PACK_FIELD_OFFSET));

		if(afe->tdm_dbg_mode == true)
		{
			snd_afe_dai_ch_cfg(afe, substream->stream, params_channels(hwparam));
		}
		DEBUG_FUNC_PRT

	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		DEBUG_FUNC_PRT
		if (false == afe->is_full_duplex) {
			regmap_update_bits(
			    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			    BIT_CTRL_DIR_CFG,
			    (1 << I2S_CTRL_DIR_CFG_FIELD_OFFSET));
		}
		/*set tx substream*/
		afe->tx_substream = substream;
		afe->tx_channels = channels;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		DEBUG_FUNC_PRT
		if (false == afe->is_full_duplex) {
			regmap_update_bits(
			    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			    BIT_CTRL_DIR_CFG,
			    (0 << I2S_CTRL_DIR_CFG_FIELD_OFFSET));
		}
		/*set rx substream */
		afe->rx_substream = substream;
		afe->rx_channels = channels;
	} else {
		DEBUG_FUNC_PRT
		return -EINVAL;
	}

	switch (params_format(hwparam)) {
		case SNDRV_PCM_FORMAT_S8:
			dev_err(afe->dev, "%s(%d) Don't support this function for S8 is low dynamic range.\n",
				__func__,__LINE__);
			return -EINVAL;
		case SNDRV_PCM_FORMAT_S16_LE:
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S32_LE:
			/* Don't support in LR Pack mode */
			if ((params_channels(hwparam) == 2)&&(afe->pack_en == true)&& (afe->tdm_initialized == false))
			{
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
	}

	/* Here calculate and fill sample rate.  */
	if (true == afe->is_full_duplex) {

		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR,
			     sample_size - 1);

		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES,
			     sample_size - 1);

	} else {
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES,
			     sample_size - 1);
	}

	/*Channel width fix to 32 */
	if(afe->tdm_initialized == false){
		regmap_write(
		    afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRATE,
		    I2S_SC_SAMPLE_RATE_CALC(clk_get_rate(afe->clk_i2s), srate,
					    max((int)channels, 2),
					    ChnWidthTable[afe->slot_width]));
	}
	else{
		regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRATE,
			     I2S_SC_SAMPLE_RATE_CALC(clk_get_rate(afe->clk_i2s), srate,
						     afe->slots,
						     ChnWidthTable[afe->slot_width]));
	}

	dev_info(
	    afe->dev,
	    "%s clk%lu srate: %u, channels: %u, sample_size: %u, period_size: %u, stream %u\n",
	    __func__,clk_get_rate(afe->clk_i2s), srate, channels, sample_size, period_size,substream->stream);

	return snd_soc_runtime_set_dai_fmt(rtd, rtd->dai_link->dai_fmt);
}

int snd_afe_dai_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{

	DEBUG_FUNC_PRT
	return 0;
}

/* snd_afe_dai_trigger */
int snd_afe_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			struct snd_soc_dai *dai)
{

	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	printk(KERN_INFO "%s:%i ---- cmd(%d)stream(%d)--------------\n", __func__,
	       __LINE__, cmd,substream->stream );
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		DEBUG_FUNC_PRT;
		ACCESS_ONCE(afe->tx_ptr) = 0;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_i2s_sc_start_playback(afe);
			atomic_set(&afe->playing, 1);
		} else {
			afe_i2s_sc_start_capture(afe);
			atomic_set(&afe->capturing, 1);
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			atomic_set(&afe->playing, 0);
			afe_i2s_sc_stop_playback(afe);
		} else {
			atomic_set(&afe->capturing, 0);
			afe_i2s_sc_stop_capture(afe);
		}
		if (!atomic_read(&afe->playing) &&
		    !atomic_read(&afe->capturing)) {
			printk(KERN_INFO "%s:%i -----i2s stop----------\n", __func__,__LINE__);
			afe_i2s_sc_stop(afe);
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}
int snd_afe_dai_hw_free(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	DEBUG_ITEM_PRT(substream->stream);

	if((afe->tx_slot_mask == 0 )&&
		(afe->rx_slot_mask == 0)){
		DEBUG_FUNC_PRT
	}
	return snd_pcm_lib_free_pages(substream);
}
void snd_afe_dai_shutdown(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	DEBUG_ITEM_PRT(substream->stream);
	/* disable main clk  end of playback or capture*/
}

static int snd_afe_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int val = 0, sck_polar = 1, ret;
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	/* codec clk & FRM slave i2s master*/
	case SND_SOC_DAIFMT_CBS_CFS:
		DEBUG_FUNC_PRT
		/*config master mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_MS_CFG,
				   (1 << I2S_CTRL_MS_CFG_FIELD_OFFSET));
		/*config sck polar*/
		sck_polar = afe->master_sck_polar;
		afe->is_slave = false;
		break;
	/* codec clk & FRM master i2s slave*/
	case SND_SOC_DAIFMT_CBM_CFM:
		DEBUG_FUNC_PRT
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_MS_CFG,
				   (0 << I2S_CTRL_MS_CFG_FIELD_OFFSET));
		/*config sck polar*/
		sck_polar = afe->slave_sck_polar;
		afe->is_slave = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
		DEBUG_FUNC_PRT
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (sck_polar << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));
		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (0 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));
		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (1 << I2S_CTRL_WS_MODE_FIELD_OFFSET));
		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (1 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));
		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));
		/*config data order:fisrt send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		DEBUG_FUNC_PRT
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (sck_polar << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));
		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (1 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));
		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (1 << I2S_CTRL_WS_MODE_FIELD_OFFSET));
		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (0 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));
		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));
		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		DEBUG_FUNC_PRT
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (sck_polar << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));
		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (1 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));
		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (1 << I2S_CTRL_WS_MODE_FIELD_OFFSET));
		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (0 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));
		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (1 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));
		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));
		break;
	case SND_SOC_DAIFMT_DSP_A:
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (sck_polar << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));

		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (0 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));

		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (0 << I2S_CTRL_WS_MODE_FIELD_OFFSET));

		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (1 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));

		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));

		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));

		break;
	case SND_SOC_DAIFMT_DSP_B:
		/*config sck polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_SCK_POLAR,
				   (sck_polar << I2S_CTRL_SCK_POLAR_FIELD_OFFSET));

		/*config ws polar*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_POLAR,
				   (0 << I2S_CTRL_WS_POLAR_FIELD_OFFSET));

		/*config word select mode*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_WS_MODE,
				   (0 << I2S_CTRL_WS_MODE_FIELD_OFFSET));

		/*config ws singal delay*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_WS_DEL,
				   (0 << I2S_CTRL_DATA_WS_DEL_FIELD_OFFSET));

		/*config data align:MSB*/
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ALIGN,
				   (0 << I2S_CTRL_DATA_ALIGN_FIELD_OFFSET));

		/*config data order:first send MSB */
		regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
				   BIT_CTRL_DATA_ORDER,
				   (0 << I2S_CTRL_DATA_ORDER_FIELD_OFFSET));

		break;
	default:
		return -EINVAL;
	}

	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, &val);
	dev_dbg(afe->dev, "DUMP %d REG_CDN_I2SSC_REGS_I2S_CTRL(0x%x)\n",
		__LINE__, val);
	return 0;
}

static int snd_afe_set_dai_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);
	int ret, val;
	unsigned long slots_mask;

	dev_info(afe->dev, "%s:%i : tx_mask(0x%x) rx_mask(0x%x) slots(%d) slot_width(%d) \n", __func__,
	       __LINE__,tx_mask,rx_mask,slots,slot_width);
	/* Enable tdm mode */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
				   BIT_TDM_CTRL_TDM_EN,
				   (1 << TDM_CTRL_TDM_EN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
			   BIT_TDM_FD_DIR_CH_TX_EN,
			   (tx_mask << TDM_FD_DIR_CH0_TXEN_FIELD_OFFSET));

	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
			   BIT_TDM_FD_DIR_CH_RX_EN,
			   (rx_mask << TDM_FD_DIR_CH0_RXEN_FIELD_OFFSET));
	if ((slots < 1) || (slots > 16 )){
		return -EINVAL;
	}
	bitmap_fill(&slots_mask,slots);


	afe->slot_width = check_and_get_slot_width_enum(slot_width);
	afe->slots = slots;
	afe->slots_mask = slots_mask;
	afe->tx_slot_mask = tx_mask;
	afe->rx_slot_mask = rx_mask;
	/*Audio will use tdm */
	afe->tdm_initialized = true;

	/* Enable tdm channel */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
			   BIT_TDM_CTRL_CHN_NUMB,
			   ((afe->slots-1) << TDM_CTRL_CHN_NO_FIELD_OFFSET));
	/* Set enabled number  */
	regmap_update_bits(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
			   BIT_TDM_CTRL_CH_EN,
			   (afe->slots_mask << TDM_CTRL_CH0_EN_FIELD_OFFSET));
	/*config total data width*/
	regmap_update_bits(
	    afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL, BIT_CTRL_CHN_WIDTH,
	    (afe->slot_width << I2S_CTRL_CHN_WIDTH_FIELD_OFFSET));
	dev_dbg(afe->dev, "%d:0x%x \n",afe->slots,afe->slots_mask);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL, &val);
	dev_info(afe->dev, "DUMP REG_CDN_I2SSC_REGS_TDM_CTRL(0x%x)\n", val);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR, &val);
	dev_info(afe->dev, "DUMP REG_CDN_I2SSC_REGS_TDM_FD_DIR(0x%x)\n", val);
	return 0;
}

static int snd_afe_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				  unsigned int freq, int dir)
{
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(dai);

	if (dir == SND_SOC_CLOCK_IN || clk_id != I2S_SCLK_MCLK)
		return -EINVAL;
	return clk_set_rate(afe->mclk, freq);
}

static struct snd_soc_dai_ops snd_afe_dai_ops = {
    .startup = snd_afe_dai_startup,
    .shutdown = snd_afe_dai_shutdown,
    .hw_params = snd_afe_dai_hw_params,
    .set_tdm_slot = snd_afe_set_dai_tdm_slot,
    .hw_free = snd_afe_dai_hw_free,
    .prepare = snd_afe_dai_prepare,
    .trigger = snd_afe_dai_trigger,
    .set_fmt = snd_afe_dai_set_fmt,
    .set_sysclk = snd_afe_dai_set_sysclk,
};

int snd_soc_dai_probe(struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(dai);
	struct device *dev = d->dev;

	dev_info(dev, "DAI probe.----------vaddr(0x%p)------------------\n",d->regs);


	dai->capture_dma_data = &d->capture_dma_data;
	dai->playback_dma_data = &d->playback_dma_data;

	return 0;
}

int snd_soc_dai_remove(struct snd_soc_dai *dai)
{
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(dai);
	struct device *dev = d->dev;

	dev_info(dev, "DAI remove.\n");

	return 0;
}

/* i2s afe dais */


static  struct snd_soc_dai_driver snd_afe_dai_template = {
	.name = "snd-afe-sc-i2s-dai0",
	.probe = snd_soc_dai_probe,
	.remove = snd_soc_dai_remove,
	.playback =
	    {
		.stream_name = "I2S Playback",
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = SND_CHANNELS_MIN,
		.channels_max = SND_CHANNELS_MAX,
	    },
	.capture =
	    {
		.stream_name = "I2S Capture",
		.formats = SND_FORMATS,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.channels_min = SND_CHANNELS_MIN,
		.channels_max = SND_CHANNELS_MAX,
	    },
	.ops = &snd_afe_dai_ops,
	.symmetric_rates = 1,
};

static const struct snd_soc_component_driver snd_sample_soc_component = {
    .name = DRV_NAME "-i2s-component",
};

int snd_afe_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = d->dev;
	struct snd_pcm *pcm = rtd->pcm;

	dev_info(dev, "New PCM.\n");
	if (!fake_buffer) {
		snd_pcm_lib_preallocate_pages_for_all(
		    pcm, SNDRV_DMA_TYPE_CONTINUOUS,
		    snd_dma_continuous_data(GFP_KERNEL), 0,
		    SDRV_I2S_FIFO_SIZE * 8);
	}
	return 0;
}

void snd_afe_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_chip(pcm);
	struct sdrv_afe_i2s_sc *d = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = d->dev;

	dev_info(dev, "Free.\n");
}

/*-----------PCM
 * OPS-----------------------------------------------------------------*/
static const struct snd_soc_platform_driver snd_afe_pcm_soc_platform = {
    .pcm_new = snd_afe_pcm_new,
    .pcm_free = snd_afe_pcm_free,
    .ops = &sdrv_snd_pcm_ops,
};

static int sdrv_pcm_prepare_slave_config(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params,
					 struct dma_slave_config *slave_config)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdrv_afe_i2s_sc *afe = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	unsigned channels = params_channels(params);
	int ret;


	ret = snd_hwparams_to_dma_slave_config(substream, params, slave_config);
	if (ret)
		return ret;

	slave_config->dst_maxburst = channels;
	slave_config->src_maxburst = channels;
	/*Change width by audio format */

	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S8:
			dev_err(afe->dev, "%s(%d) Don't support this function for S8 is low dynamic range.\n",
				__func__,__LINE__);
			return -EINVAL;
		case SNDRV_PCM_FORMAT_S16_LE:
			DEBUG_FUNC_PRT;
			if((channels == 2)&&(afe->pack_en == true)&&(afe->tdm_initialized == false)){
				/* Set to L/R pack mode. */
				slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
				slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			}else{
				slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
				slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			}
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S32_LE:
			DEBUG_FUNC_PRT;
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			break;
		default:
			return -EINVAL;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		slave_config->dst_addr = afe->playback_dma_data.addr;

	}else{
		slave_config->src_addr = afe->capture_dma_data.addr;
	}
	DEBUG_FUNC_PRT;
	return 0;
}

static const struct snd_dmaengine_pcm_config sdrv_dmaengine_pcm_config = {
    .pcm_hardware = &sdrv_pcm_hardware,
    .prepare_slave_config = sdrv_pcm_prepare_slave_config,
    .prealloc_buffer_size = MAX_ABUF_SIZE,
};

static int snd_afe_soc_platform_probe(struct snd_soc_component *pdev)
{
	struct device *dev = pdev->dev;
	/* struct sdrv_afe_i2s_sc *afe; */
	int ret = 0;
	dev_info(dev, "Added.----------------\n");

	DEBUG_FUNC_PRT
	/* TODO: Add afe controls here */

	return ret;
}

static void snd_afe_soc_platform_remove(struct snd_soc_component *pdev)
{
	struct device *dev = pdev->dev;
	dev_info(dev, "Removed.\n");
	DEBUG_FUNC_PRT
	return;
}

static const struct snd_soc_dapm_widget afe_widgets[] = {
    /* Outputs */
	SND_SOC_DAPM_AIF_OUT("PCM0 OUT", NULL,  0,
			SND_SOC_NOPM, 0, 0),
	/* Inputs */
	SND_SOC_DAPM_AIF_IN("PCM0 IN", NULL,  0,
			SND_SOC_NOPM, 0, 0),

};

static const struct snd_soc_dapm_route afe_dapm_map[] = {
    /* Line Out connected to LLOUT, RLOUT */
   /* Outputs */
   { "PCM0 OUT", NULL, "I2S Playback" },
   /* Inputs */
   { "I2S Capture", NULL, "PCM0 IN" },

};
static struct snd_soc_component_driver snd_afe_soc_component = {
	.name = "afe-dai",
    .probe = snd_afe_soc_platform_probe,
    .remove = snd_afe_soc_platform_remove,
	.dapm_widgets = afe_widgets,
	.num_dapm_widgets = ARRAY_SIZE(afe_widgets),
	.dapm_routes = afe_dapm_map,
	.num_dapm_routes = ARRAY_SIZE(afe_dapm_map),
};

static int snd_afe_i2s_sc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_afe_i2s_sc *afe;
	int ret;
	struct resource *res;
	unsigned int irq_id, value;

	dev_info(dev, "Probed.\n");

	afe = devm_kzalloc(dev, sizeof(struct sdrv_afe_i2s_sc), GFP_KERNEL);
	if (afe == NULL){
		return -ENOMEM;
	}
	afe->pdev = pdev;
	afe->dev = dev;
	platform_set_drvdata(pdev, afe);

	afe->clk_i2s = devm_clk_get(&pdev->dev, "i2s-clk");
	if (IS_ERR(afe->clk_i2s))
		return PTR_ERR(afe->clk_i2s);
	//Only this freq could also set MCLK to 12.288M.
	/* 	ret = clk_set_rate(afe->clk_i2s, 294912000);
	if (ret)
		return ret; */

	afe->mclk = devm_clk_get(&pdev->dev, "i2s-mclk");
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

	/* Get register and print */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->regs = devm_ioremap_resource(afe->dev, res);
	if (IS_ERR(afe->regs)) {
		ret = PTR_ERR(afe->regs);
		goto err_disable;
	}

	dev_info(&pdev->dev,
		 "ALSA X9"
		 "num(%d) vaddr(0x%p) paddr(0x%llx) \n",
		  pdev->num_resources, afe->regs, res->start);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->regs,
					    &sdrv_i2s_sc_regmap_config);

	if (IS_ERR(afe->regmap)) {
		ret = PTR_ERR(afe->regmap);
		goto err_disable;
	}

	/* Get irq and setting */
	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		regmap_exit(afe->regmap);
		dev_err(afe->dev, "%s no irq\n", dev->of_node->name);
		return -ENXIO;
	}
	DEBUG_ITEM_PRT(irq_id);

	ret = devm_request_irq(afe->dev, irq_id, sdrv_i2s_sc_irq_handler, 0,
			       pdev->name, (void *)afe);

	/* TODO: Set i2s sc default afet to full duplex mode. Next change by dts
	 * setting. Change scr setting before, need check SCR_SEC_BASE + (0xC <<
	 * 10) 's value should be 0x3F*/

	afe->is_full_duplex = true;

	ret = device_property_read_u32(dev, "semidrive,full-duplex", &value);
	if (!ret) {
		/* semidrive,full-duplex is 0 set to half duplex mode.  */
		if (value == 0) {
			/* D9 set half duplex mode */
			afe->is_full_duplex = false;
		}
	}

	if (fake_buffer) {
		ret = alloc_fake_buffer();
	}

	afe->playback_dma_data.addr = res->start + I2S_SC_FIFO_OFFSET;
	afe->playback_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	afe->playback_dma_data.maxburst = 4;

	afe->capture_dma_data.addr = res->start + I2S_SC_FIFO_OFFSET;
	afe->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	afe->capture_dma_data.maxburst = 4;

	ret =
	    devm_snd_dmaengine_pcm_register(dev, &sdrv_dmaengine_pcm_config, 0);
	if (ret)
		goto err_disable;

	memcpy(&afe->dai_drv, &snd_afe_dai_template,
	       sizeof(struct snd_soc_dai_driver));

	ret = device_property_read_string(&pdev->dev, "sdrv,afe-dai-name", &afe->dai_drv.name);
	if (ret < 0) {
		dev_info(&pdev->dev, "snd_afe_dais '%s'\n",afe->dai_drv.name);
	}else{
		dev_info(&pdev->dev, "set snd_afe_dais to '%s'\n",afe->dai_drv.name);
	}

	ret = devm_snd_soc_register_component(dev, &snd_afe_soc_component,
					      &afe->dai_drv,
					      1);
	if (ret < 0) {
		regmap_exit(afe->regmap);
		dev_err(&pdev->dev, "couldn't register component\n");
		goto err_disable;
	}
	/*init afe*/
	afe->tdm_initialized = false;
	afe->tdm_dbg_mode = false;
	afe->tdm_use_rx_last_slot = false;
	afe->rx_channels = 1;
	afe->tx_channels = 1;
	afe->master_sck_polar = 1;
	afe->slave_sck_polar = 1;
	afe->srate = 8000;
	afe->pack_en = true;
	afe->slot_width = AFE_I2S_CHN_WIDTH;
	if (of_find_property(dev->of_node, "sdrv,disable-pack-mode", NULL)){
		dev_info(&pdev->dev, "disable pack mode.\n");
		afe->pack_en = false;
	}
	ret = device_property_read_u32(dev, "sdrv,master-sck-polar", &value);
	if (!ret) {
		/* sdrv,master-sck-polar  */
		if (value == 0) {
			afe->master_sck_polar = 0;
		}else {
			afe->master_sck_polar = 1;
		}
	}
	ret = device_property_read_u32(dev, "sdrv,slave-sck-polar", &value);
	if (!ret) {
		/* sdrv,slave-sck-polar */
		if (value == 0) {
			afe->slave_sck_polar = 0;
		}else {
			afe->slave_sck_polar = 1;
		}
	}
	ret = device_property_read_u32(dev, "sdrv,slot-width", &value);
	if (!ret) {
		/* ssdrv,slot-width  */
		afe->slot_width = check_and_get_slot_width_enum(value);
	}
	if (of_find_property(dev->of_node, "sdrv,tdm-dbg", NULL)){
		dev_info(&pdev->dev, "enable tdm debug mode.\n");
		afe->tdm_dbg_mode = true;
	}

	if (of_find_property(dev->of_node, "sdrv,tdm-use-rx-last-slot", NULL)) {
		dev_info(&pdev->dev, "set tdm-use-rx-last-slot flag.\n");
		afe->tdm_use_rx_last_slot = true;
	}
	afe_i2s_sc_config(afe);
	return 0;
err_disable:
	/* pm_runtime_disable(&pdev->dev); */
	return ret;
}

static int snd_afe_i2s_sc_remove(struct platform_device *pdev)
{

	struct device *dev = &pdev->dev;
	struct sdrv_afe_i2s_sc *afe = platform_get_drvdata(pdev);
	dev_info(dev, "Removed.\n");
	regmap_exit(afe->regmap);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id sdrv_i2s_sc_of_match[] = {
    {
	.compatible = "semidrive,x9-i2s-sc",
    },
    {},
};

#ifdef CONFIG_PM_SLEEP
static int sdrv_i2s_sc_suspend(struct device *dev)
{
	int ret;
	struct sdrv_afe_i2s_sc *afe = dev_get_drvdata(dev);
	/* dev_info(dev, "sdrv_i2s_sc_suspend.\n"); */
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
			  &afe->reg_i2s_ctrl);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
			  &afe->reg_i2s_ctrl_fdx);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES,
			  &afe->reg_i2s_sres);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR,
			  &afe->reg_i2s_sres_fdr);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRATE,
			  &afe->reg_i2s_srate);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
			  &afe->reg_i2s_stat);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_LEVEL,
			  &afe->reg_i2s_fifo_level);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY,
			  &afe->reg_i2s_fifo_aempty);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL,
			  &afe->reg_i2s_fifo_afull);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_LEVEL_FDR,
			  &afe->reg_i2s_fifo_level_fdr);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY_FDR,
			  &afe->reg_i2s_fifo_aempty_fdr);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL_FDR,
			  &afe->reg_i2s_fifo_afull_fdr);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
			  &afe->reg_i2s_tdm_ctrl);
	ret = regmap_read(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
			  &afe->reg_i2s_tdm_fd_dir);

	clk_disable_unprepare(afe->mclk);
	clk_disable_unprepare(afe->clk_i2s);
	return 0;
}

static int sdrv_i2s_sc_resume(struct device *dev)
{
	int ret;
	struct sdrv_afe_i2s_sc *afe = dev_get_drvdata(dev);
	/* dev_info(dev, "sdrv_i2s_sc_resume.\n"); */
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

	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL_FDX,
		     afe->reg_i2s_ctrl_fdx);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES,
		     afe->reg_i2s_sres);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRES_FDR,
		     afe->reg_i2s_sres_fdr);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_SRATE,
		     afe->reg_i2s_srate);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_STAT,
		     afe->reg_i2s_stat);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY,
		     afe->reg_i2s_fifo_aempty);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL,
		     afe->reg_i2s_fifo_afull);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AEMPTY_FDR,
		     afe->reg_i2s_fifo_aempty_fdr);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_FIFO_AFULL_FDR,
		     afe->reg_i2s_fifo_afull_fdr);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_TDM_CTRL,
		     afe->reg_i2s_tdm_ctrl);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_TDM_FD_DIR,
		     afe->reg_i2s_tdm_fd_dir);
	regmap_write(afe->regmap, REG_CDN_I2SSC_REGS_I2S_CTRL,
		     afe->reg_i2s_ctrl);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */
static const struct dev_pm_ops i2s_sc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdrv_i2s_sc_suspend, sdrv_i2s_sc_resume)
};

static struct platform_driver snd_afe_i2s_sc_driver = {
    .driver =
	{
	    .name = DRV_NAME "-i2s",
	    .of_match_table = sdrv_i2s_sc_of_match,
	    .pm = &i2s_sc_pm_ops,
	},
    .probe = snd_afe_i2s_sc_probe,
    .remove = snd_afe_i2s_sc_remove,
};

module_platform_driver(snd_afe_i2s_sc_driver);
MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 I2S SC ALSA SoC device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 i2s asoc device");
