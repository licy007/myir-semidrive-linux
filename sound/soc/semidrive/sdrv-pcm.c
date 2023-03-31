
/*
 * sdrv-pcm.c
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
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pxa2xx-lib.h>
#include <sound/dmaengine_pcm.h>

#include "sdrv-snd-common.h"
/* X9 Audio Front End PCM probe function */
static int snd_afe_pcm_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct sdrv_afe_pcm *afe;
    struct resource *res;
    dev_info(dev, "Probed.\n");
    afe = devm_kzalloc(dev, sizeof(struct sdrv_afe_pcm),
                       GFP_KERNEL);
    if (afe == NULL)
        return -ENOMEM;
    return ret;
}
/* X9 Audio Front End PCM remove function */
static int snd_afe_pcm_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct sdrv_afe_pcm *afe = platform_get_drvdata(dev);
    dev_info(dev, "Removed.\n");
    /* regmap_exit(afe->regmap); */
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

static const struct of_device_id sdrv_i2s_sc_of_match[] = {
    {
        .compatible = "semidrive,x9-afe-pcm",
    },
    {},
};
/*  Define afe pcm platform driver name.  */
static struct platform_driver snd_afe_i2s_sc_driver = {
    .driver = {
        .name = "x9-afe-pcm",
        .of_match_table = sdrv_afe_pcm_of_match,
    },
    .probe = snd_afe_pcm_probe,
    .remove = snd_afe_pcm_remove,
};

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("X9 ALSA SoC audio front end pcm driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9-afe-pcm");