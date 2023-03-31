/*
 * ak4556.c
 *
 * Add ak4556 codec
 */

#include <linux/module.h>
#include <sound/soc.h>

/*
 * ak4556 is very simple DA/AD converter which has no setting register.
 *
 * CAUTION
 *
 * ak4556 playback format is SND_SOC_DAIFMT_I2S
 * on same bit clock, LR clock.
 * But, this driver doesn't have snd_soc_dai_ops :: set_fmt
 *
 * CPU/Codec DAI image

 * Configure by hardware.
 */

static const struct snd_soc_dapm_widget ak4556_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("AINL"),
SND_SOC_DAPM_INPUT("AINR"),

SND_SOC_DAPM_OUTPUT("AOUTL"),
SND_SOC_DAPM_OUTPUT("AOUTR"),
};

static const struct snd_soc_dapm_route ak4556_dapm_routes[] = {
	{ "Capture", NULL, "AINL" },
	{ "Capture", NULL, "AINR" },

	{ "AOUTL", NULL, "Playback" },
	{ "AOUTR", NULL, "Playback" },
};

static struct snd_soc_dai_driver ak4556_dai = {
	.name = "ak4556-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		/* FIXME: This chip only support SNDRV_PCM_FMTBIT_S24_LE*/
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE ,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		/* FIXME: This chip only support SNDRV_PCM_FMTBIT_S24_LE*/
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE ,
	},
	.symmetric_rates = 1,
};

static const struct snd_soc_codec_driver soc_codec_dev_ak4556 = {
	.component_driver = {
		.dapm_widgets		= ak4556_dapm_widgets,
		.num_dapm_widgets	= ARRAY_SIZE(ak4556_dapm_widgets),
		.dapm_routes		= ak4556_dapm_routes,
		.num_dapm_routes	= ARRAY_SIZE(ak4556_dapm_routes),
	},
};

static int ak4556_soc_probe(struct platform_device *pdev)
{
	/* pr_info("AK4556 Probed!"); */
	return snd_soc_register_codec(&pdev->dev,
				      &soc_codec_dev_ak4556,
				      &ak4556_dai, 1);
}

static int ak4556_soc_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

/* static const struct of_device_id ak4556_of_match[] = {
	{ .compatible = "asahi-kasei,ak4556" },
	{},
};
MODULE_DEVICE_TABLE(of, ak4556_of_match); */

static struct platform_driver ak4556_driver = {
	.driver = {
		.name = "ak4556-adc-dac",
		/* .of_match_table = ak4556_of_match, */
	},
	.probe	= ak4556_soc_probe,
	.remove	= ak4556_soc_remove,
};
//module_platform_driver(ak4556_driver);

static struct platform_device *ak4556_soc_dev;

static int __init snd_soc_ak4556_init(void)
{
	int ret;
	/* pr_info("AK4556 init!"); */
	ak4556_soc_dev =
		platform_device_register_simple("ak4556-adc-dac", -1, NULL, 0);
	if (IS_ERR(ak4556_soc_dev))
		return PTR_ERR(ak4556_soc_dev);

	ret = platform_driver_register(&ak4556_driver);
	if (ret != 0)
		platform_device_unregister(ak4556_soc_dev);

	return ret;
}
module_init(snd_soc_ak4556_init);

void __exit snd_soc_ak4556_exit(void)
{
	platform_device_unregister(ak4556_soc_dev);
	platform_driver_unregister(&ak4556_driver);
}
module_exit(snd_soc_ak4556_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SoC ak4556 driver");
MODULE_AUTHOR("yi.shao@semidrive.com");
