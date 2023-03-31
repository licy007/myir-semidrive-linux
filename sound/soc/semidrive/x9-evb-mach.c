/*
 * x9-evb-mach
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
/*FIXME: remove i2c later*/
#include "sdrv-common.h"
#include <linux/i2c.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>

/* -------------------------------- */
/* definition of the chip-specific record  dma related ops*/
struct snd_x9_chip {
	struct snd_soc_card *card;
	struct x9_dummy_model *model;
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
	struct mutex audio_mutex;
};


/*Controls
 * ------------------------------------------------------------------------------*/
enum snd_x9_ctrl {
	PCM_PLAYBACK_VOLUME,
	PCM_PLAYBACK_MUTE,
	PCM_PLAYBACK_DEVICE,
};
static int snd_x9_evb_controls_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_card *card = snd_kcontrol_chip(kcontrol); */
	/*Set value here*/
	/* 	struct evb_priv *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = card_data->ext_spk_amp_en; */

	return 0;
}
static int snd_x9_evb_controls_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_card *card = snd_kcontrol_chip(kcontrol); */
	/* 	struct evb_priv *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = card_data->ext_spk_amp_en; */

	return 0;
}
static int snd_x9_ctl_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	if (kcontrol->private_value == PCM_PLAYBACK_VOLUME) {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = -10239;
		uinfo->value.integer.max = 400; /* 2303 */
	}
	return 0;
}

static int snd_x9_ctl_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_x9_chip *chip = snd_kcontrol_chip(kcontrol);

	mutex_lock(&chip->audio_mutex);

	if (kcontrol->private_value == PCM_PLAYBACK_VOLUME)
		ucontrol->value.integer.value[0] = chip->volume;
	else if (kcontrol->private_value == PCM_PLAYBACK_MUTE)
		ucontrol->value.integer.value[0] = chip->mute;
	else if (kcontrol->private_value == PCM_PLAYBACK_DEVICE)
		ucontrol->value.integer.value[0] = chip->dest;

	mutex_unlock(&chip->audio_mutex);
	return 0;
}

static int snd_x9_ctl_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	return changed;
}
SNDRV_CTL_TLVD_DECLARE_DB_SCALE(snd_codec_db_scale, -10239, 1, 1);

static const struct snd_kcontrol_new snd_x9_cs42888_controls[] = {
    SOC_SINGLE_BOOL_EXT("X9 Switch", 0, snd_x9_evb_controls_get,
			snd_x9_evb_controls_put),
    {.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
     .name = "PCM Playback Volume",
     .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
     .private_value = PCM_PLAYBACK_VOLUME,
     .info = snd_x9_ctl_info,
     .get = snd_x9_ctl_get,
     .put = snd_x9_ctl_put,
     .tlv = {.p = snd_codec_db_scale}},
};

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

/* Headphones jack detection for DAPM pin */
static struct snd_soc_jack_pin hs_jack_pin[] = {
    {
	.pin = "Mic In",
	.mask = SND_JACK_HEADSET,
    },
    {
	.pin = "Line In",
	/* disable speaker when hp jack is inserted */
	.mask = SND_JACK_HEADPHONE,
	.invert = 1,
    },
};

/*
 * Logic for a tlv320aic23 as connected on a x9_evb
 */
static int x9_evb_tlv320aic23_init(struct snd_soc_pcm_runtime *rtd)
{
	int err = 0;
	DEBUG_FUNC_PRT;
	/* Jack detection API stuff */
	/*TODO: add damp &hs_jack, hs_jack_pin, ARRAY_SIZE(hs_jack_pin));*/
	if (gpio_is_valid(hs_jack_gpios[0].gpio)) {
	err = snd_soc_card_jack_new(rtd->card, "Headset Jack", SND_JACK_HEADSET,
				    &hs_jack, hs_jack_pin, ARRAY_SIZE(hs_jack_pin));

	if (err)
		return err;
	DEBUG_ITEM_PRT(hs_jack_gpios[0].gpio);
/* 	err = snd_soc_jack_add_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios),
				     hs_jack_gpios); */
	DEBUG_ITEM_PRT(err);
	}
	DEBUG_FUNC_PRT;
	return err;
}

/*Dapm widtets*/
static const struct snd_soc_dapm_widget sd_x9_evb_dapm_widgets[] = {
    /* Outputs */
    SND_SOC_DAPM_HP("Headphone Jack", NULL),

    /* Inputs */
    SND_SOC_DAPM_LINE("Line In", NULL),
    SND_SOC_DAPM_MIC("Mic In", NULL),
};

static const struct snd_soc_dapm_route sd_x9_evb_dapm_map[] = {
    /* Line Out connected to LLOUT, RLOUT */
    {"Headphone Jack", NULL, "LOUT"},
    {"Headphone Jack", NULL, "ROUT"},
    /* Line in connected to LLINEIN, RLINEIN */
    {"LLINEIN", NULL, "Line In"},
    {"RLINEIN", NULL, "Line In"},
    /*  Mic in*/
    {"MICIN", NULL, "Mic In"},
};
/*DAI
 * Link-------------------------------------------------------------------------------*/
static int x9_tlv320ai23_soc_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;

	int srate, mclk, err;

	srate = params_rate(params);
	mclk = 256 * srate;
	/*TODO: need rearch mclk already be set to 12288000
	Configure clk here.
	*/

	DEBUG_ITEM_PRT(mclk);
	err = snd_soc_dai_set_sysclk(codec_dai, I2S_SCLK_MCLK, mclk,
				     SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}

	return 0;
}

static const struct snd_soc_ops x9_tlv320ai23_ops = {
    .hw_params = x9_tlv320ai23_soc_hw_params,
};

static struct snd_soc_dai_link snd_x9_evb_soc_dai_links[] = {
    {
	.name = "x9_tlv320ai23 hifi",
	.stream_name = "x9 hifi",
	/*
     * You MAY specify the link's CPU-side device, either by device name,
     * or by DT/OF node, but not both. If this information is omitted,
     * the CPU-side DAI is matched using .cpu_dai_name only, which hence
     * must be globally unique. These fields are currently typically used
     * only for codec to codec links, or systems using device tree.
     * You MAY specify the DAI name of the CPU DAI. If this information is
     * omitted, the CPU-side DAI is matched using .cpu_name/.cpu_of_node
     * only, which only works well when that device exposes a single DAI.
     */
	.cpu_dai_name = "snd-afe-sc-i2s-dai0",
	.platform_name = "30650000.i2s",
	.codec_dai_name = "tlv320aic23-hifi",
	.ops = &x9_tlv320ai23_ops,
	//.codec_dai_name = "snd-soc-dummy-dai",//"tlv320aic23-hifi",
	.codec_name = "tlv320aic23-codec.0-001a",
	.init = x9_evb_tlv320aic23_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS,
    },
    {
	.name = "x9_i2s_mc1",
	.stream_name = "x9_i2s_mc",
	.cpu_dai_name = "snd-afe-mc-i2s-dai0",
	.cpu_name = "305c0000.i2s_mc",
	.platform_name = "305c0000.i2s_mc",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS,
    },
    {
	.name = "x9_i2s_mc2",
	.stream_name = "x9_i2s_mc",
	.cpu_dai_name = "snd-afe-mc-i2s-dai0",
	.platform_name = "305d0000.i2s_mc",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS,
    },
#if 0 /* open if enable SPDIF device in dts */
	{
	.name = "x9_spdif1",
	.stream_name = "x9 spdif",
	.cpu_name = "30580000.spdif",
	.platform_name = "30580000.spdif",
	.cpu_dai_name = "snd_afe_spdif_dai0",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
    },
    {
	.name = "x9_spdif2",
	.stream_name = "x9 spdif",
	.cpu_name = "30590000.spdif",
	.platform_name = "30590000.spdif",
	.cpu_dai_name = "snd_afe_spdif_dai0",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
    }
#endif
};

/*Sound Card Driver
 * ------------------------------------------------------------------------*/
static struct snd_soc_card x9_evb_card = {
    .name = "sd-x9-snd-card",

    .dai_link = snd_x9_evb_soc_dai_links,
    .num_links = ARRAY_SIZE(snd_x9_evb_soc_dai_links),
    /*.controls = snd_x9_evb_controls,
    .num_controls = ARRAY_SIZE(snd_x9_evb_controls),*/
    .dapm_widgets = sd_x9_evb_dapm_widgets,
    .num_dapm_widgets = ARRAY_SIZE(sd_x9_evb_dapm_widgets),
    .dapm_routes = sd_x9_evb_dapm_map,
    .num_dapm_routes = ARRAY_SIZE(sd_x9_evb_dapm_map),

};

/*GPIO probe use this function to set gpio. */
static int x9_gpio_probe(struct snd_soc_card *card)
{
	int ret = 0;
	return ret;
}

/*-Init Machine Driver
 * ---------------------------------------------------------------------*/
#define SND_X9_MACH_DRIVER "x9-tlv320-evb"

/*ALSA machine driver probe functions.*/
static int x9_evb_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &x9_evb_card;
	struct device *dev = &pdev->dev;

	int ret;
	struct snd_x9_chip *chip;

	dev_info(&pdev->dev, ": dev name =%s %s\n", pdev->name, __func__);
	/*
	struct device_node *codec_node;
	codec_node =
	    of_parse_phandle(pdev->dev.of_node, "semidrive,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
		DEBUG_FUNC_PRT;
		return -EINVAL;
	}
	snd_x9_evb_soc_dai_links[0].codec_of_node = codec_node;
	snd_x9_evb_soc_dai_links[1].codec_of_node = codec_node; */
	DEBUG_FUNC_PRT;
	card->dev = dev;

	/* Allocate chip data */
	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}
	chip->card = card;
	chip->jack_gpio = of_get_named_gpio(pdev->dev.of_node, "jack-gpio", 0);
	hs_jack_gpios[0].gpio = chip->jack_gpio;

	ret = devm_snd_soc_register_card(dev, card);

	if (ret) {
		dev_err(dev, "%s snd_soc_register_card fail %d\n", __func__,
			ret);
	}

	return ret;
}

static int x9_evb_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));
	dev_info(&pdev->dev, "%s x9_evb_removed\n", __func__);
	return 0;
}
#ifdef CONFIG_PM
/*pm suspend operation */
static int snd_x9_suspend(struct platform_device *pdev, pm_message_t state)
{
	dev_info(&pdev->dev, "%s snd_x9_suspend\n", __func__);
	return 0;
}
/*pm resume operation */
static int snd_x9_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s snd_x9_resume\n", __func__);
	return 0;
}
#endif

static const struct of_device_id x9_evb_dt_match[] = {
    {
	.compatible = "semidrive,x9-tlv320aic23",
    },
    {},
};
MODULE_DEVICE_TABLE(of, x9_evb_dt_match);

static struct platform_driver x9_evb_mach_driver = {
    .driver =
	{
	    .name = SND_X9_MACH_DRIVER,
	    .of_match_table = x9_evb_dt_match,
#ifdef CONFIG_PM
	    .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = x9_evb_probe,
	.remove = x9_evb_remove,
#ifdef CONFIG_PM
	.suspend = snd_x9_suspend,
	.resume = snd_x9_resume,
#endif

};

module_platform_driver(x9_evb_mach_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 alsa mach");
