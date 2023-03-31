/*
 * x9-ref-mach-hs
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
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/pcm.h>
#include <sound/tlv.h>
#include "sdrv-common.h"
#include <linux/i2c.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <soc/semidrive/sdrv_boardinfo_hwid.h>
/* -------------------------------- */

/* definition of the chip-specific record  dma related ops*/
struct snd_x9u_chip_hs {
	struct snd_soc_card *card;
	struct x9u_dummy_model *model;
	struct snd_pcm *pcm;
	struct snd_pcm *pcm_spdif;
	struct snd_pcm_hardware pcm_hw;
	/* Bitmat for valid reg_base and irq numbers
	unsigned int avail_substreams;*/
	struct device *dev;

	int volume;
	int old_volume; /* stores the volume value whist muted */
	int dest;
	int mute;
	int jack_gpio;

	unsigned int opened;
	unsigned int spdif_status;
	struct mutex audio_mutex;
};
static int delay_probe_cnt = 1;
/*Controls
 * ------------------------------------------------------------------------------*/
/* head set jack GPIO Controller  */
static struct snd_soc_jack hs_jack;
static struct snd_soc_jack_gpio hs_jack_gpios[] = {
    {
	.gpio = -1,
	.name = "headset-gpio",
	.report = SND_JACK_HEADSET,
	.invert = 0,
	.debounce_time = 200,
    },
};

static struct snd_soc_jack_pin hs_jack_pin[] = {
    {
	.pin = "Headphones",
	.mask = SND_JACK_HEADSET,

    },
    {
	.pin = "LINE OUT",
	.mask = SND_JACK_HEADPHONE,
	.invert = 1,
    },
};

/*
 * Logic for a tlv320aic23 as connected on a x9u_ms_hs
 */
static int x9u_msb1_tlv320aic23_init(struct snd_soc_pcm_runtime *rtd)
{
	int err = 0;
	DEBUG_FUNC_PRT;

	/* Jack detection API stuff */
	/*TODO: add damp &hs_jack, hs_jack_pin, ARRAY_SIZE(hs_jack_pin));*/
	if (gpio_is_valid(hs_jack_gpios[0].gpio)) {
		err = snd_soc_card_jack_new(
		    rtd->card, "Headphones", SND_JACK_HEADSET, &hs_jack,
		    hs_jack_pin, ARRAY_SIZE(hs_jack_pin));

		if (err)
			return err;
		DEBUG_ITEM_PRT(hs_jack_gpios[0].gpio);
		err = snd_soc_jack_add_gpios(
		    &hs_jack, ARRAY_SIZE(hs_jack_gpios), hs_jack_gpios);
		DEBUG_ITEM_PRT(err);
	}
	DEBUG_FUNC_PRT;
	return err;
}

/*Dapm widtets*/
static const struct snd_soc_dapm_widget sd_x9u_msb1_tlv320aic23_dapm_widgets[] =
    {
	/* Outputs */
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_LINE("LINE OUT", NULL),
	/* Inputs */
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_MIC("Mic In", NULL),
};

static const struct snd_soc_dapm_route sd_x9u_msb1_tlv320aic23_dapm_map[] = {
    /* Line Out connected to LLOUT, RLOUT */
    {"Headphones", NULL, "LHPOUT"},
    {"Headphones", NULL, "RHPOUT"},

    {"LINE OUT", NULL, "LOUT"},
    {"LINE OUT", NULL, "ROUT"},

    {"DAC", NULL, "PCM0 OUT"},

    /* Line in connected to LLINEIN, RLINEIN */
    {"LLINEIN", NULL, "Line In"},
    {"RLINEIN", NULL, "Line In"},
    /*  Mic in*/
    {"MICIN", NULL, "Mic In"},
};

/*DAI
 * Link-------------------------------------------------------------------------------*/
static int x9u_tlv320ai23_soc_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;

	int srate, mclk, err;

	srate = params_rate(params);
	mclk = 256 * srate;
	/*WARNNING: need rearch mclk already be set to 12288000
	If you config mclk here, need check its impact for others.
	*/

	DEBUG_ITEM_PRT(mclk);
	err = snd_soc_dai_set_sysclk(cpu_dai, I2S_SCLK_MCLK, mclk,
				     SND_SOC_CLOCK_OUT);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, I2S_SCLK_MCLK, mclk,
				     SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}

	return 0;
}

static const struct snd_soc_ops x9u_msb1_tlv320aic23_ops = {
    .hw_params = x9u_tlv320ai23_soc_hw_params,
};

static struct snd_soc_dai_link snd_x9u_msb1_soc_dai_links[] = {
    {
	.name = "x9_hifi_hs",
	.stream_name = "x9 hifi hs",
	.cpu_name = "30650000.i2s",
	.cpu_dai_name = "snd-afe-sc-i2s-dai0",
	.platform_name = "30650000.i2s",
	.codec_dai_name = "tlv320aic23-hifi",
	.init = x9u_msb1_tlv320aic23_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS,
	.ops = &x9u_msb1_tlv320aic23_ops,
    },
};
/*-Init Machine Driver
 * ---------------------------------------------------------------------*/
#define SND_X9_MACH_DRIVER "x9u-ms_b1-tlv320aic23"
#define SND_CARD_NAME SND_X9_MACH_DRIVER
/*Sound Card Driver
 * ------------------------------------------------------------------------*/
static struct snd_soc_card x9u_msb1_tlv320aic23_card = {
    .name = SND_X9_MACH_DRIVER,
    .owner = THIS_MODULE,
    .dai_link = snd_x9u_msb1_soc_dai_links,
    .num_links = ARRAY_SIZE(snd_x9u_msb1_soc_dai_links),
    .dapm_widgets = sd_x9u_msb1_tlv320aic23_dapm_widgets,
    .num_dapm_widgets = ARRAY_SIZE(sd_x9u_msb1_tlv320aic23_dapm_widgets),
    .dapm_routes = sd_x9u_msb1_tlv320aic23_dapm_map,
    .num_dapm_routes = ARRAY_SIZE(sd_x9u_msb1_tlv320aic23_dapm_map),
    .fully_routed = true,
};


/*ALSA machine driver probe functions.*/
static int x9u_msb1_tlv320aic23_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &x9u_msb1_tlv320aic23_card;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np;

	int ret;
	struct snd_x9u_chip_hs *chip;

	dev_info(&pdev->dev, ": dev name = %s %s %d\n", pdev->name, __func__,
		 get_part_id(PART_BOARD_ID_MIN));

	if (delay_probe_cnt > 0) {
		delay_probe_cnt--;
		return -EPROBE_DEFER;
	}

	DEBUG_FUNC_PRT;
	card->dev = dev;

	codec_np = of_parse_phandle(np, "semidrive,audio-codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "failed to get codec info\n");
		return -EINVAL;
	}
	snd_x9u_msb1_soc_dai_links[0].codec_of_node = codec_np;

	hs_jack_gpios[0].gpio =
	    of_get_named_gpio(np, "semidrive,jack-gpio-b1", 0);
	if (hs_jack_gpios[0].gpio < 0) {
		hs_jack_gpios[0].gpio = -1;
	}

	/* Allocate chip data */
	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}
	chip->card = card;

	ret = devm_snd_soc_register_card(dev, card);

	if (ret) {
		dev_err(dev, "%s snd_soc_register_card fail %d\n", __func__,
			ret);
	}

	return ret;
}

static int x9u_msb1_tlv320aic23_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));
	dev_info(&pdev->dev, "%s x9u_msb1_tlv320aic23_removed\n", __func__);
	delay_probe_cnt = 1;
	return 0;
}
#ifdef CONFIG_PM
/*pm suspend operation */
static int snd_x9u_msb1_tlv320aic23_suspend(struct platform_device *pdev,
					    pm_message_t state)
{
	dev_info(&pdev->dev, "%s snd_x9u_suspend\n", __func__);
	return 0;
}
/*pm resume operation */
static int snd_x9u_msb1_tlv320aic23_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s snd_x9u_resume\n", __func__);
	return 0;
}
#endif

static const struct of_device_id x9u_msb1_tlv320aic23_dt_match[] = {
    {
	.compatible = "semidrive,x9-msb1-tlv320aic23",
    },
    {},
};
MODULE_DEVICE_TABLE(of, x9u_msb1_tlv320aic23_dt_match);

static struct platform_driver x9u_msb1_tlv320aic23_mach_driver = {
    .driver =
	{
	    .name = SND_X9_MACH_DRIVER,
	    .of_match_table = x9u_msb1_tlv320aic23_dt_match,
#ifdef CONFIG_PM
	    .pm = &snd_soc_pm_ops,
#endif
	    .probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},

    .probe = x9u_msb1_tlv320aic23_probe,
    .remove = x9u_msb1_tlv320aic23_remove,
#ifdef CONFIG_PM
    .suspend = snd_x9u_msb1_tlv320aic23_suspend,
    .resume = snd_x9u_msb1_tlv320aic23_resume,
#endif
};

module_platform_driver(x9u_msb1_tlv320aic23_mach_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC ref machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 ref tlv320aic23 alsa mach");
