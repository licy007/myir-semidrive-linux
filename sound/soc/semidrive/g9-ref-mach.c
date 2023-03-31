/*
 * g9-evb-mach
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
struct snd_g9_chip {
	struct snd_soc_card *card;
	struct x9_dummy_model *model;
	struct snd_pcm *pcm;
	struct snd_pcm *pcm_spdif;
	struct snd_pcm_hardware pcm_hw;
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


/*Controls
 * ------------------------------------------------------------------------------*/
enum snd_g9_ctrl {
	PCM_PLAYBACK_VOLUME,
	PCM_PLAYBACK_MUTE,
	PCM_PLAYBACK_DEVICE,
};

static int snd_g9_ref_controls_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/*Add machine driver controller here */
	return 0;
}

static int snd_g9_ref_controls_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/*Add machine driver controller here */
	return 0;
}

static int snd_g9_ctl_info(struct snd_kcontrol *kcontrol,
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

static int snd_g9_ctl_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_g9_chip *chip = snd_kcontrol_chip(kcontrol);

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

static int snd_g9_ctl_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	return changed;
}

SNDRV_CTL_TLVD_DECLARE_DB_SCALE(snd_codec_db_scale, -10239, 1, 1);

static const struct snd_kcontrol_new snd_g9_mach_controls[] = {
    SOC_SINGLE_BOOL_EXT("g9 Switch", 0, snd_g9_ref_controls_get,
			snd_g9_ref_controls_put),
    {.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
     .name = "PCM Playback Volume",
     .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
     .private_value = PCM_PLAYBACK_VOLUME,
     .info = snd_g9_ctl_info,
     .get = snd_g9_ctl_get,
     .put = snd_g9_ctl_put,
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
 * Logic for a tlv320aic3104 as connected on a g9_evb
 */
static int g9_evb_tlv320aic3104_init(struct snd_soc_pcm_runtime *rtd)
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
static const struct snd_soc_dapm_widget sd_g9_evb_dapm_widgets[] = {
    /* Outputs */
	SND_SOC_DAPM_SPK("HS L", NULL),
	SND_SOC_DAPM_SPK("HS R", NULL),

    /* Inputs */
    SND_SOC_DAPM_LINE("Line In", NULL),
    SND_SOC_DAPM_MIC("Mic In", NULL),
};

static const struct snd_soc_dapm_route sd_g9_evb_dapm_map[] = {
    /* Line Out connected to LLOUT, RLOUT */
    {"HS L", NULL, "HPLOUT"},
    {"HS R", NULL, "HPROUT"},

    /*  Mic in*/

    {"LINE1R", NULL, "Mic Bias"},
	{"LINE1L", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic In"},

};
/*DAI
 * Link-------------------------------------------------------------------------------*/
static int g9_tlv320ai3x_soc_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;

	int srate, mclk, err;

	srate = params_rate(params);
	/*TODO: need rearch mclk already be set to 12000000
	Configure clk here.
	*/
	mclk = 12000000;
	DEBUG_ITEM_PRT(mclk);
	err = snd_soc_dai_set_sysclk(codec_dai, I2S_SCLK_MCLK, mclk,
				     SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}

	return 0;
}

static const struct snd_soc_ops g9_tlv320ai3x_ops = {
    .hw_params = g9_tlv320ai3x_soc_hw_params,
};

static struct snd_soc_dai_link snd_g9_evb_soc_dai_links[] = {
    {
	.name = "g9_tlv320ai23 hifi",
	.stream_name = "g9 hifi",
	.cpu_dai_name = "snd-afe-sc-i2s-dai0",
	.platform_name = "30610000.i2s",
	.codec_dai_name = "tlv320aic3x-hifi",
	.ops = &g9_tlv320ai3x_ops,
	.codec_name = "tlv320aic3x-codec.4-0018",
	.init = g9_evb_tlv320aic3104_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS,
    },
};

/*Sound Card Driver
 * ------------------------------------------------------------------------*/
static struct snd_soc_card g9_ref_card = {
    .name = "sdrv-g9-snd-card",

    .dai_link = snd_g9_evb_soc_dai_links,
    .num_links = ARRAY_SIZE(snd_g9_evb_soc_dai_links),
    .dapm_widgets = sd_g9_evb_dapm_widgets,
    .num_dapm_widgets = ARRAY_SIZE(sd_g9_evb_dapm_widgets),
    .dapm_routes = sd_g9_evb_dapm_map,
    .num_dapm_routes = ARRAY_SIZE(sd_g9_evb_dapm_map),

};

/*GPIO probe use this function to set gpio. */
static int g9_gpio_probe(struct snd_soc_card *card)
{
	int ret = 0;
	return ret;
}

/*-Init Machine Driver
 * ---------------------------------------------------------------------*/
#define SND_G9_MACH_DRIVER "g9-tlv320-ref"

/*ALSA machine driver probe functions.*/
static int g9_ref_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &g9_ref_card;
	struct device *dev = &pdev->dev;

	int ret;
	struct snd_g9_chip *chip;

	dev_info(&pdev->dev, ": dev name =%s %s\n", pdev->name, __func__);

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

static int g9_ref_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));
	dev_info(&pdev->dev, "%s g9_evb_removed\n", __func__);
	return 0;
}
#ifdef CONFIG_PM
/*pm suspend operation */
static int snd_g9_suspend(struct platform_device *pdev, pm_message_t state)
{
	dev_info(&pdev->dev, "%s snd_g9_suspend\n", __func__);
	return 0;
}
/*pm resume operation */
static int snd_g9_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s snd_g9_resume\n", __func__);
	return 0;
}
#endif

static const struct of_device_id g9_evb_dt_match[] = {
    {
	.compatible = "semidrive,g9-tlv320aic3104",
    },
};
MODULE_DEVICE_TABLE(of, g9_evb_dt_match);

static struct platform_driver g9_evb_mach_driver = {
    .driver =
	{
	    .name = SND_G9_MACH_DRIVER,
	    .of_match_table = g9_evb_dt_match,
#ifdef CONFIG_PM
	    .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = g9_ref_probe,
	.remove = g9_ref_remove,
#ifdef CONFIG_PM
	.suspend = snd_g9_suspend,
	.resume = snd_g9_resume,
#endif

};

module_platform_driver(g9_evb_mach_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("g9 ALSA SoC machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:g9 alsa mach");
