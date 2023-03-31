/*
 * x9-ref-mach
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
#include <sound/pcm_params.h>
#include <sound/tlv.h>
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

	/*Next is for tdm debug*/
	bool set_tdm_slot;  ///< false: don't set tdm slot parameter. true: update tdm slot
	int tdm_tx_mask;    ///< tx mask, bitmap format.
	int tdm_rx_mask;    ///< rx mask, bitmap format.
	int tdm_slots;	    ///< tdm slot number.
	int tdm_slot_width; ///< tdm slot width. 16bits 20bits 24bits 28bits
			    ///< 32bits.

};

/*Controls
 * ------------------------------------------------------------------------------*/
enum snd_x9_ctrl {
	PCM_PLAYBACK_VOLUME,
	PCM_PLAYBACK_MUTE,
	PCM_PLAYBACK_DEVICE,
};

static int x9_ref_late_probe(struct snd_soc_card *card) { return 0; }

static int x9_ref_tdm_tx_mask_controls_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = chip->tdm_tx_mask;

	return 0;
}
static int x9_ref_tdm_tx_mask_controls_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);

	chip->tdm_tx_mask = ucontrol->value.integer.value[0];
	chip->set_tdm_slot = true;
	return 0;
}

static int x9_ref_tdm_rx_mask_controls_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = chip->tdm_rx_mask;

	return 0;
}
static int x9_ref_tdm_rx_mask_controls_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);

	chip->tdm_rx_mask = ucontrol->value.integer.value[0];
	chip->set_tdm_slot = true;
	return 0;
}
static int x9_ref_tdm_slot_controls_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = chip->tdm_slots;

	return 0;
}
static int x9_ref_tdm_slot_controls_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);
	unsigned int val = ucontrol->value.integer.value[0];
	if (val > 16) {
		return -EINVAL;
	}
	chip->tdm_slots = val;
	chip->set_tdm_slot = true;
	return 0;
}

static const char *i2s_sc_slot_width_texts[] = {
    "16bit", "20bit", "24bit", "28bit", "32bit",
};

static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_slot_width, i2s_sc_slot_width_texts);

static int
x9_ref_tdm_slot_width_control_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);

	/* 	ucontrol->value.enumerated.item[0] = drvdata->mclk_sel; */
	switch (chip->tdm_slot_width) {
	case 16:
		ucontrol->value.enumerated.item[0] = 0;
		break;
	case 20:
		ucontrol->value.enumerated.item[0] = 1;
		break;
	case 24:
		ucontrol->value.enumerated.item[0] = 2;
		break;
	case 28:
		ucontrol->value.enumerated.item[0] = 3;
		break;
	case 32:
		ucontrol->value.enumerated.item[0] = 4;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int
x9_ref_tdm_slot_width_control_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(card);
	unsigned int val = ucontrol->value.enumerated.item[0];
	switch (val) {
	case 0:
		chip->tdm_slot_width = 16;
		break;
	case 1:
		chip->tdm_slot_width = 20;
		break;
	case 2:
		chip->tdm_slot_width = 24;
		break;
	case 3:
		chip->tdm_slot_width = 28;
		break;
	case 4:
		chip->tdm_slot_width = 32;
		break;
	default:
		return -EINVAL;
	}
	chip->set_tdm_slot = true;
	return 0;
}

static const struct snd_kcontrol_new snd_x9_ref_mach_controls[] = {
    SOC_SINGLE_EXT("X9Mach TDM Tx Mask", 0, 0, 0xFFFF, 0,
		   x9_ref_tdm_tx_mask_controls_get,
		   x9_ref_tdm_tx_mask_controls_put),
    SOC_SINGLE_EXT("X9Mach TDM Rx Mask", 1, 0, 0xFFFF, 0,
		   x9_ref_tdm_rx_mask_controls_get,
		   x9_ref_tdm_rx_mask_controls_put),
    SOC_SINGLE_EXT("X9Mach TDM Slot", 3, 0, 0x20, 0,
		   x9_ref_tdm_slot_controls_get, x9_ref_tdm_slot_controls_put),
    SOC_ENUM_EXT("X9Mach TDM SlotWidth", soc_enum_slot_width,
		 x9_ref_tdm_slot_width_control_get,
		 x9_ref_tdm_slot_width_control_put),
};
static int x9_ref_rtd_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(rtd->card);
	dev_dbg(rtd->dev,
		"[i2s sc] cpu dai -----%s -- %p %p "
		"%p----------------------- \n",
		rtd->cpu_dai->name, rtd->cpu_dai, rtd->cpu_dai->driver,
		rtd->cpu_dai->driver->ops->set_tdm_slot);
	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0xFF, 0xF, 8, 32);
	if (ret < 0) {
		dev_err(rtd->dev,
			"can't set cpu snd_soc_dai_set_tdm_slot err %d\n", ret);
		return ret;
	}
	/*update this function for card interface*/
	chip->tdm_tx_mask = 0xff;
	chip->tdm_rx_mask = 0xf;
	chip->tdm_slots = 8;
	chip->tdm_slot_width = 32;
	return 0;
}
static int x9_ref_mach_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_x9_chip *chip = snd_soc_card_get_drvdata(rtd->card);
	if (chip->set_tdm_slot == false)
		return 0;
	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, chip->tdm_tx_mask,
				       chip->tdm_rx_mask, chip->tdm_slots,
				       chip->tdm_slot_width);
	if (ret < 0) {
		dev_err(rtd->dev,
			"can't set cpu snd_soc_dai_set_tdm_slot err %d\n", ret);
		return ret;
	}
	chip->set_tdm_slot = false;
	dev_info(rtd->dev,
		"set tdm slot tx(0x%x) rx(0x%x) slots(%d) width(%d)\n",
		chip->tdm_tx_mask, chip->tdm_rx_mask, chip->tdm_slots,
		chip->tdm_slot_width);
	return 0;
}

static const struct snd_soc_ops x9_ref_mach_snd_ops = {
    .hw_params = x9_ref_mach_hw_params,
};

static struct snd_soc_dai_link snd_x9_ref_soc_dai_links[] = {
    /* Front End DAI links */
    {
	.name = "x9_hifi",
	.stream_name = "x9 hifi",
	.cpu_name = "30650000.i2s",
	.cpu_dai_name = "snd-afe-sc-i2s-dai0",
	.platform_name = "30650000.i2s",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.init = x9_ref_rtd_init,
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM,
	.ops = &x9_ref_mach_snd_ops,
    },
};
/*-Init Machine Driver
 * ---------------------------------------------------------------------*/
#define SND_X9_REF_MACH_DRIVER "x9-ref-mach"
#define SND_REF_CARD_NAME SND_X9_REF_MACH_DRIVER
/*Sound Card Driver
 * ------------------------------------------------------------------------*/
static struct snd_soc_card x9_ref_card = {
    .name = SND_REF_CARD_NAME,
    .dai_link = snd_x9_ref_soc_dai_links,
    .num_links = ARRAY_SIZE(snd_x9_ref_soc_dai_links),
    .late_probe = x9_ref_late_probe,
};

/*ALSA machine driver probe functions.*/
static int x9_ref_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &x9_ref_card;
	struct device *dev = &pdev->dev;

	int ret;
	struct snd_x9_chip *chip;
	dev_info(&pdev->dev, ": dev name =  %s %s\n", pdev->name, __func__);
	card->dev = dev;

	/* Allocate chip data */
	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}
	chip->card = card;
	/*Next section is for tdm debug*/
	snd_soc_card_set_drvdata(card, chip);
	chip->set_tdm_slot = false;
	chip->tdm_tx_mask = 0xff;
	chip->tdm_rx_mask = 0xf;
	chip->tdm_slots = 8;
	chip->tdm_slot_width = 32;
	/*tdm debug section*/

	ret = devm_snd_soc_register_card(dev, card);

	if (ret) {
		dev_err(dev, "%s snd_soc_register_card fail %d\n", __func__,
			ret);
		return ret;
	}
	/* Add controls */
	ret = snd_soc_add_card_controls(card, snd_x9_ref_mach_controls,
			ARRAY_SIZE(snd_x9_ref_mach_controls));
	if (ret < 0) {
		dev_err(dev, "%s snd_soc_register_card control fail %d\n", __func__,
			ret);
	}
	return ret;
}

static int x9_ref_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));
	dev_info(&pdev->dev, "%s x9_ref_removed\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
/*pm suspend operation */
static int snd_x9_ref_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	dev_info(&pdev->dev, "%s snd_x9_suspend\n", __func__);
	return 0;
}
/*pm resume operation */
static int snd_x9_ref_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s snd_x9_resume\n", __func__);
	return 0;
}
#endif
static const struct of_device_id x9_ref_dt_match[] = {
    {
	.compatible = "semidrive,x9-ref-mach",
    },
    {},
};

MODULE_DEVICE_TABLE(of, x9_ref_dt_match);

static struct platform_driver x9_ref_mach_driver = {
    .driver =
	{
	    .name = SND_X9_REF_MACH_DRIVER,
	    .of_match_table = x9_ref_dt_match,
#ifdef CONFIG_PM
	    .pm = &snd_soc_pm_ops,
#endif
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},

    .probe = x9_ref_probe,
    .remove = x9_ref_remove,
#ifdef CONFIG_PM
    .suspend = snd_x9_ref_suspend,
    .resume = snd_x9_ref_resume,
#endif
};

module_platform_driver(x9_ref_mach_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC ref machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 ref alsa mach");
