/*
 * x9-ref-mach-remote
 * Copyright (C) 2022 semidrive
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
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "sdrv-common.h"

static int x9_ref_late_probe(struct snd_soc_card *card)
{
	return 0;
}

static int x9_ref_mach_remote_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	return 0;
}

static const struct snd_soc_ops x9_ref_mach_remote_snd_ops = {
	.hw_params = x9_ref_mach_remote_hw_params,
};

static struct snd_soc_dai_link snd_x9_ref_remote_dai_links[3];

/* use unilink for virtual i2s in x9u */
static struct snd_soc_dai_link snd_unilink_i2s_dai_links[] = {
	{
		.name = "x9_unilink_pcm0",
		.stream_name = "x9 unilink pcm0",
		.cpu_name = "soc:unilink@i2s0",
		.cpu_dai_name = "snd-unilink-pcm-dai0",
		.platform_name = "soc:unilink@i2s0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = NULL,
		.ops = &x9_ref_mach_remote_snd_ops,
	},
};

/* use rpmsg for virtual i2s in x9h */
static struct snd_soc_dai_link snd_rpmsg_i2s_dai_links[] = {
	{
		.name = "x9_rpmsg_pcm0",
		.stream_name = "x9 rpmsg pcm0",
		.cpu_name = "soc:rpmsg@i2s0",
		.cpu_dai_name = "snd-rpmsg-pcm-dai0",
		.platform_name = "soc:rpmsg@i2s0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = NULL,
		.ops = &x9_ref_mach_remote_snd_ops,
	},

	{
		.name = "x9_rpmsg_pcm1",
		.stream_name = "x9 rpmsg pcm1",
		.cpu_name = "soc:rpmsg@i2s1",
		.cpu_dai_name = "snd-rpmsg-pcm-dai1",
		.platform_name = "soc:rpmsg@i2s1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = NULL,
		.ops = &x9_ref_mach_remote_snd_ops,
	},
};

/* Init Machine Driver */
#define SND_X9_REF_MACH_REMOTE_DRIVER "x9-ref-mach-remote-pcm"
#define SND_REF_REMOTE_CARD_NAME SND_X9_REF_MACH_REMOTE_DRIVER

static struct snd_soc_card x9_ref_remote_card = {
	.name = SND_REF_REMOTE_CARD_NAME,
	.dai_link = snd_x9_ref_remote_dai_links,
	.num_links = ARRAY_SIZE(snd_x9_ref_remote_dai_links),
	.late_probe = x9_ref_late_probe,
};

/* ALSA machine driver probe functions. */
static int x9_ref_remote_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &x9_ref_remote_card;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int link_count = 0; /* default has one link */

	card->dev = dev;

	//use sound card device 1 as virtual i2s device node (unilink)
	if (of_find_property(np, "semidrive,unilink_i2s", NULL)){
		dev_info(&pdev->dev, "add unilink_i2s device node\n");
		memcpy(&snd_x9_ref_remote_dai_links[link_count],&snd_unilink_i2s_dai_links[0],
				sizeof(snd_unilink_i2s_dai_links));
		link_count += ARRAY_SIZE(snd_unilink_i2s_dai_links);
	}

	//use sound card device 1 as virtual i2s device node (rpmsg)
	if (of_find_property(np, "semidrive,rpmsg_i2s", NULL)){
		dev_info(&pdev->dev, "add rpmsg_i2s device node\n");
		memcpy(&snd_x9_ref_remote_dai_links[link_count],&snd_rpmsg_i2s_dai_links[0],
				sizeof(snd_rpmsg_i2s_dai_links));
		link_count += ARRAY_SIZE(snd_rpmsg_i2s_dai_links);
	}

	x9_ref_remote_card.num_links = link_count;

	if (link_count > 0) {
		ret = devm_snd_soc_register_card(dev, card);
	} else {
		ret = -1;
	}

	if (ret) {
		dev_err(dev, "%s snd_soc_register_card fail %d.\n", __func__,
			ret);
	}

	return ret;
}

static int x9_ref_remote_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));

	dev_info(&pdev->dev, "%s x9_ref_remote_removed.\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
/* pm suspend operation */
static int snd_x9_ref_remote_suspend(struct platform_device *pdev,
				     pm_message_t state)
{
	dev_info(&pdev->dev, "%s snd_x9_ref_remote_suspend.\n", __func__);
	return 0;
}

/* pm resume operation */
static int snd_x9_ref_remote_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s snd_x9_ref_remote_resume.\n", __func__);
	return 0;
}
#endif

static const struct of_device_id x9_ref_remote_dt_match[] = {
	{
		.compatible = "semidrive,x9-ref-mach-remote",
	},
	{},
};

MODULE_DEVICE_TABLE(of, x9_ref_remote_dt_match);

static struct platform_driver x9_ref_mach_remote_driver = {
    .driver =
        {
            .name = SND_X9_REF_MACH_REMOTE_DRIVER,
            .of_match_table = x9_ref_remote_dt_match,
#ifdef CONFIG_PM
            .pm = &snd_soc_pm_ops,
#endif
            .probe_type = PROBE_PREFER_ASYNCHRONOUS,
        },

    .probe = x9_ref_remote_probe,
    .remove = x9_ref_remote_remove,
#ifdef CONFIG_PM
    .suspend = snd_x9_ref_remote_suspend,
    .resume = snd_x9_ref_remote_resume,
#endif
};

module_platform_driver(x9_ref_mach_remote_driver);

MODULE_AUTHOR("Yuansong Jiao <yuansong.jiao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC ref machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 ref remote alsa mach");