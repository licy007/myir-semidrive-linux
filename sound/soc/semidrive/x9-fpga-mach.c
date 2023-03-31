/*
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
#include <sound/rawmidi.h>
#include <sound/tlv.h>
/*FIXME: remove i2c later*/
#include "x9-common.h"
#include <linux/i2c.h>
#include <sound/initval.h>
#include <sound/soc.h>

/* -------------------------------- */
static bool fake_buffer = 0;
module_param(fake_buffer, bool, 0444);
MODULE_PARM_DESC(fake_buffer, "Fake buffer allocations.");
/* -------------------------------- */

/*Controls
 * ------------------------------------------------------------------------------*/
enum snd_x9_ctrl {
	PCM_PLAYBACK_VOLUME,
	PCM_PLAYBACK_MUTE,
	PCM_PLAYBACK_DEVICE,
};
static int snd_x9_cs42888_controls_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	/*Set value here*/
	/* 	struct evb_priv *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = card_data->ext_spk_amp_en; */

	return 0;
}
static int snd_x9_cs42888_controls_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
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

static const struct snd_kcontrol_new snd_x9_fpga_controls[] = {
    SOC_SINGLE_BOOL_EXT("X9 Switch", 0, snd_x9_cs42888_controls_get,
			snd_x9_cs42888_controls_put),
    /* 	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
     .name = "PCM Playback Volume",
     .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
     .private_value = PCM_PLAYBACK_VOLUME,
     .info = snd_x9_ctl_info,
     .get = snd_x9_ctl_get,
     .put = snd_x9_ctl_put,
     .tlv = {.p = snd_codec_db_scale}}, */
};

/*DAI
 * Link-------------------------------------------------------------------------------*/

static struct snd_soc_dai_link snd_x9_cs42888_soc_dai_links[] = {
    {
	.name = "x9_cs42888 Playback",
	.stream_name = "x9 Playback",
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
	//.cpu_dai_name   = "x9_cs42888-i2s-dai0",
	.cpu_dai_name = "snd-afe-sc-i2s-dai0",
	.platform_name = "30580000.i2s",
	.codec_dai_name = "cs42888",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM,
    },
    {
	.name = "x9_cs42888 Capture",
	.stream_name = "x9 Capture",
	.cpu_dai_name = "snd-afe-sc-i2s-dai1",
	.platform_name = "30580000.i2s",
	.codec_dai_name = "cs42888",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM,
    },
    /* Front End DAI links */
    {
	.name = "x9 hifi Playback",
	.stream_name = "x9 hifi Playback",
	.cpu_dai_name = "snd-afe-sc-i2s-dai0",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
	.dynamic = 1,
	.dpcm_playback = 1,
    },
};
/*Sound Card Driver
 * ------------------------------------------------------------------------*/
static struct snd_soc_card x9_cs42888_card = {
    .name = "sd-x9-snd-card",

    .dai_link = snd_x9_cs42888_soc_dai_links,
    .num_links = ARRAY_SIZE(snd_x9_cs42888_soc_dai_links),
    .controls = snd_x9_fpga_controls,
    .num_controls = ARRAY_SIZE(snd_x9_fpga_controls),
    //.dapm_widgets = sd_x9_fpga_dapm_widgets,
    //.num_dapm_widgets = ARRAY_SIZE(sd_x9_fpga_dapm_widgets),
    //.dapm_routes = sd_x9_fpga_audio_map,
    //.num_dapm_routes = ARRAY_SIZE(sd_x9_fpga_audio_map),

};

/*GPIO probe use this function to set gpio. */
static int x9_gpio_probe(struct snd_soc_card *card)
{
	int ret = 0;
	return ret;
}

/*-Init Machine Driver
 * ---------------------------------------------------------------------*/
#define SND_X9_MACH_DRIVER "x9-cs42888-fpga"

/*ALSA machine driver probe functions.*/
static int x9_fpga_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &x9_cs42888_card;
	struct device *dev = &pdev->dev;
	struct device_node *codec_node;
	int ret, i;

	dev_info(&pdev->dev, ": dev name =%s %s\n", pdev->name, __func__);
	codec_node =
	    of_parse_phandle(pdev->dev.of_node, "semidrive,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
		DEBUG_FUNC_PRT;
		return -EINVAL;
	}
	snd_x9_cs42888_soc_dai_links[0].codec_of_node = codec_node;
	snd_x9_cs42888_soc_dai_links[1].codec_of_node = codec_node;
	DEBUG_FUNC_PRT;
	card->dev = dev;

	ret = devm_snd_soc_register_card(dev, card);

	if (ret) {
		dev_err(dev, "%s snd_soc_register_card fail %d\n", __func__,
			ret);
	}

	return ret;
}

static int x9_fpga_remove(struct platform_device *pdev)
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

static const struct of_device_id x9_cs42888_dt_match[] = {
    {
	.compatible = "semidrive,x9-cs42888",
    },
    {},
};
MODULE_DEVICE_TABLE(of, x9_cs42888_dt_match);

static struct platform_driver x9_fpga_mach_driver = {
    .driver =
	{
	    .name = SND_X9_MACH_DRIVER,
	    .of_match_table = x9_cs42888_dt_match,
#ifdef CONFIG_PM
	    .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = x9_fpga_probe,
	.remove = x9_fpga_remove,
#ifdef CONFIG_PM
	.suspend = snd_x9_suspend,
	.resume = snd_x9_resume,
#endif
};

module_platform_driver(x9_fpga_mach_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 alsa device");
