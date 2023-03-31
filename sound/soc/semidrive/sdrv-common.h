/* sdrv-common.h
 * Copyright (C) 2020 semidrive
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

#ifndef SDRV_COMMON_H__
#define SDRV_COMMON_H__
#include "sdrv-abuf.h"
#include <sound/dmaengine_pcm.h>

/* scr physical address */
#define APB_SCR_SEC_BASE (0x38200000u)

#define DEBUG_FUNC_PRT                                                         \
	printk(KERN_INFO "SDRV_ALSA: %s:%-120i\n", __func__,    \
	       __LINE__);

#define DEBUG_FUNC_PRT_INFO(str)                                               \
	printk(KERN_INFO "SDRV_ALSA: %s:%-120i %s\n", __func__, __LINE__, str);

#define DEBUG_DEV_PRT(dev)                                                     \
	dev_info(dev, "SDRV_ALSA: %s:%-120i \n", __func__,       \
		 __LINE__);

#define DEBUG_ITEM_PRT(item)                                                   \
	printk(KERN_INFO "SDRV_ALSA: %s:%-20i [%s]:0x%x(%d)\n", __func__,      \
	       __LINE__, #item, (unsigned int)item, (int)item);

#define DEBUG_ITEM_PTR_PRT(item)                                                   \
	printk(KERN_INFO "SDRV_ALSA: %s:%-120i [%s]:0x%llx(%p)\n", __func__,   \
	       __LINE__, #item, (long long unsigned int)item, item);

#define SDRV_I2S_SC_FIFO_SIZE (2048)

#define SND_CHANNEL_MONO 1  /* 1.0 */
#define SND_CHANNEL_FOUR 2  /* 2.0 */
#define SND_CHANNEL_TWO 4   /* 3.1 */
#define SND_CHANNEL_SIX 6   /* 5.1 */
#define SND_CHANNEL_EIGHT 8 /* 7.1 */
#define SND_CHANNEL_TEN  10 /* 7.1.2 */
#define SND_CHANNEL_TWELVE  12 /* 7.1.4 */
#define SND_CHANNEL_SIXTEEN  16 /* 16*/
#define SND_CHANNELS_MIN SND_CHANNEL_MONO
#define SND_CHANNELS_MAX SND_CHANNEL_SIXTEEN

/* defaults x9 sound card setting*/
#define MAX_ABUF_SIZE (SND_CHANNELS_MAX * 2 * 2 * SDRV_I2S_SC_FIFO_SIZE)
#define MIN_ABUF_SIZE (256)
#define MAX_PERIOD_SIZE (SND_CHANNELS_MAX * 2 * 2 * SDRV_I2S_SC_FIFO_SIZE)
#define MIN_PERIOD_SIZE (2)
#define MAX_PERIODS (4 * SDRV_I2S_SC_FIFO_SIZE)
#define MIN_PERIODS (2)
#define MAX_ABUF MAX_ABUF_SIZE

/* i2s mc config */
#define MIN_I2S_MC_PERIOD_SIZE (2)
#define MAX_I2S_MC_PERIOD_SIZE (8 * 2 * 2 * X9_I2S_MC_FIFO_SIZE)
#define MIN_I2S_MC_PERIODS (2)
#define MAX_I2S_MC_PERIODS (4 * X9_I2S_MC_FIFO_SIZE)
#define MAX_I2S_MC_ABUF_SIZE (8 * 2 * 2 * X9_I2S_MC_FIFO_SIZE)

#define SND_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |SNDRV_PCM_FMTBIT_S32_LE)

#define SND_RATE SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000
#define SND_RATE_MIN 8000
#define SND_RATE_MAX 48000


#define SDRV_SPDIF_STATUS_FREE 0x0
#define SDRV_SPDIF_STATUS_16_IN 0x01
#define SDRV_SPDIF_STATUS_44_IN 0x02
#define SDRV_SPDIF_STATUS_48_IN SDRV_SPDIF_STATUS_44_IN
#define SDRV_SPDIF_STATUS_16_OUT 0x03
#define SDRV_SPDIF_STATUS_44_OUT 0x04
#define SDRV_SPDIF_STATUS_48_OUT 0x05
#define SDRV_SPDFI_STATUS_UNDEFINED 0xff

enum {
	I2S_SCLK_MCLK,
};

/* x9 afe common structure */
struct sdrv_afe {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *base_addr;
	void __iomem *sram_address;
};

/* x9 i2s sc audio front end structure */
struct sdrv_afe_i2s_sc {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;

	/* TODO: Alloc a sram address, use this to for dma transfer  */
	void __iomem *sram_address;

	/* this is clk for i2s clock */
	struct clk *clk_i2s;

	/* mclk for output  */
	struct clk *mclk;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct snd_dmaengine_dai_dma_data playback_dma_data;
	char capture_dma_chan[8];
	char playback_dma_chan[8];
	struct snd_dmaengine_pcm_config dma_config;
	struct regmap *regmap;
	struct snd_pcm_substream __rcu *tx_substream;
	struct snd_pcm_substream __rcu *rx_substream;

	unsigned int tx_ptr; /* next frame index in the sample buffer */

	unsigned int periods;
	/* current fifo level estimate.
	 * Doesn't have to be perfectly accurate, but must be not less than
	 * the actual FIFO level in order to avoid stall on push attempt.
	 */
	unsigned tx_fifo_level;

	/* FIFO level at which level interrupt occurs */
	unsigned tx_fifo_low;

	/* maximal FIFO level */
	unsigned tx_fifo_high;

	/* Full Duplex mode true is full duplex*/
	bool is_full_duplex;

	/* In full-duplex playing and capturing flag */
	atomic_t playing;
	atomic_t capturing;
	/*TDM Related code*/
	unsigned slot_width;
	unsigned slots;
	unsigned slots_mask;
	unsigned tx_slot_mask;
	unsigned rx_slot_mask;
	bool tdm_initialized;
	/*support tdm debug mode, it auto mapping channel from first channel, support play different format, user debug need enable sdrv,tdm_dbg_mode*/
	bool tdm_dbg_mode;
	/* stream is slave mode  */
	bool is_slave;
	/* when last tdm rx slot enable, need use this flag to avoid capture channel swap*/
	bool tdm_use_rx_last_slot;

	int tx_channels;
	int rx_channels;
	/*Only one sample rate for i2s ws*/
	int srate;

	/* i2s l/r pack mode*/
	bool pack_en;
	/* i2s sck polar mode*/
	unsigned master_sck_polar;
	unsigned slave_sck_polar;

	/* afe dai*/
	struct snd_soc_dai_driver dai_drv;
	/*PM Cache*/
	unsigned int reg_i2s_ctrl;
	unsigned int reg_i2s_ctrl_fdx;
	unsigned int reg_i2s_sres;
	unsigned int reg_i2s_sres_fdr;
	unsigned int reg_i2s_srate;
	unsigned int reg_i2s_stat;
	unsigned int reg_i2s_fifo_level;
	unsigned int reg_i2s_fifo_aempty;
	unsigned int reg_i2s_fifo_afull;
	unsigned int reg_i2s_fifo_level_fdr;
	unsigned int reg_i2s_fifo_aempty_fdr;
	unsigned int reg_i2s_fifo_afull_fdr;
	unsigned int reg_i2s_tdm_ctrl;
	unsigned int reg_i2s_tdm_fd_dir;

};

/* x9 i2s mc audio front end structure */
struct sdrv_afe_i2s_mc {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;
	/* TODO: Alloc a sram address, use this to for dma transfer  */
	void __iomem *sram_address;

	struct clk *clk_i2s;
	struct clk *mclk;

	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct snd_dmaengine_dai_dma_data playback_dma_data;
	char capture_dma_chan[8];
	char playback_dma_chan[8];
	struct snd_dmaengine_pcm_config dma_config;
	struct regmap *regmap;
	struct snd_pcm_substream __rcu *tx_substream;
	struct snd_pcm_substream __rcu *rx_substream;

	unsigned int tx_ptr; /* next frame index in the sample buffer */
	unsigned int periods;
	/* current fifo level estimate.
	 * Doesn't have to be perfectly accurate, but must be not less than
	 * the actual FIFO level in order to avoid stall on push attempt.
	 */
	unsigned tx_fifo_level;

	/* FIFO level at which level interrupt occurs */
	unsigned tx_fifo_low;

	/* maximal FIFO level */
	unsigned tx_fifo_high;

	/* loop back test  */
	bool loopback_mode;

	/* stream is slav mode  */
	bool is_slave;

	/* multi channel config */
	unsigned slot_width;
	unsigned slots;
	unsigned tx_slot_mask;
	unsigned rx_slot_mask;

	/* clk sync loop */
	unsigned clk_sync_loop;

	/* fmt clk edge */
	unsigned clk_edge;

	/* playback stream status */
	unsigned playback_status;

	/* capture stream status */
	unsigned capture_status;

	/* PM Cache */
	unsigned int reg_mc_i2s_ctrl;
	unsigned int reg_mc_i2s_intr_stat;
	unsigned int reg_mc_i2s_srr;
	unsigned int reg_mc_i2s_cid_ctrl;
	unsigned int reg_mc_i2s_tfifo_stat;
	unsigned int reg_mc_i2s_rfifo_stat;
	unsigned int reg_mc_i2s_tfifo_ctrl;
	unsigned int reg_mc_i2s_rfifo_ctrl;
	unsigned int reg_mc_i2s_dev_conf;
	unsigned int reg_mc_i2s_poll_stat;
};

struct sdrv_afe_spdif {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;
	/* TODO: Alloc a sram address, use this to for dma transfer  */
	void __iomem *sram_address;

	struct clk *clk;
	struct clk *mclk;

	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct snd_dmaengine_dai_dma_data playback_dma_data;
	char capture_dma_chan[4];
	char playback_dma_chan[4];
	struct snd_dmaengine_pcm_config dma_config;
	struct regmap *regmap;
	struct snd_pcm_substream __rcu *tx_substream;
	struct snd_pcm_substream __rcu *rx_substream;
	unsigned int tx_ptr; /* next frame index in the sample buffer */

	/* FIFO level at which level interrupt occurs */
	unsigned tx_fifo_low;

	/* maximal FIFO level */
	unsigned tx_fifo_high;
	/* status code prevent flow conflict */
	unsigned int spdif_status;

	/* PM Cache */
	unsigned int reg_spdif_ctrl;
	unsigned int reg_spdif_int;
	unsigned int reg_spdif_fifo_ctrl;
	unsigned int reg_spdif_stat;
};

/* Define audio front end model */
struct sdrv_afe_pcm {
	struct platform_device *pdev;
	struct device *dev;

	/* address for ioremap audio hardware register */
	void __iomem *regs;
	/* TODO: Alloc a sram address, use this to for dma transfer  */
	// void __iomem *sram_address;
	// struct regmap *regmap;
	struct sdrv_audio_buf abuf[SDRV_ABUF_NUMB];
	/*Back end Data status*/
	/*Audio front end*/
	bool suspended;

	int afe_pcm_ref_cnt;
	int adda_afe_pcm_ref_cnt;
	int i2s_out_on_ref_cnt;
};


/* x9 dummy model for debug*/
struct sdrv_dummy_model {
	const char *name;
	int (*playback_constraints)(struct snd_pcm_runtime *runtime);
	int (*capture_constraints)(struct snd_pcm_runtime *runtime);
	u64 formats;
	size_t buffer_bytes_max;
	size_t period_bytes_min;
	size_t period_bytes_max;
	unsigned int periods_min;
	unsigned int periods_max;
	unsigned int rates;
	unsigned int rate_min;
	unsigned int rate_max;
	unsigned int channels_min;
	unsigned int channels_max;
};

#endif /* SDRV_COMMON_H__ */
