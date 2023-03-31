/*
 * ALSA SoC Texas Instruments TAS5404 Quad-Channel Audio Amplifier
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "tas5404.h"
//#define TAS5404_DEBUG 1 // used at debug mode

#ifdef TAS5404_DEBUG
#define dev_prt dev_info
#define x9dbgprt printk
#else
#define dev_prt dev_dbg
#define x9dbgprt(format, arg...)                                               \
	do {                                                                   \
	} while (0)
#endif
/* Define how often to check (and clear) the fault status register (in ms) */
#define TAS5404_FAULT_CHECK_INTERVAL 300

static const char *const tas5404_supply_names[] = {
    /* "pvdd", Class-D amp output FETs supply. */
};
#define TAS5404_NUM_SUPPLIES ARRAY_SIZE(tas5404_supply_names)

struct tas5404_data {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_bulk_data supplies[TAS5404_NUM_SUPPLIES];
	struct delayed_work fault_check_work;
	unsigned int last_fault1;
	unsigned int last_fault2;
	unsigned int last_warn1;
	unsigned int last_warn2;
	struct gpio_desc *standby_gpio;
	struct gpio_desc *mute_gpio;
	bool HIZ_flag;
	ktime_t unmute_start;
};

/*
 * Analog channel gain select
 */
static const char *tas5404_gain_txt[] = {"12dB", "20dB", "26dB", "32dB"};

static const struct soc_enum tas5404_gain_txt_enum[] = {
    SOC_ENUM_SINGLE(TAS5404_CTRL1, 0, ARRAY_SIZE(tas5404_gain_txt),
		    tas5404_gain_txt),
    SOC_ENUM_SINGLE(TAS5404_CTRL1, 2, ARRAY_SIZE(tas5404_gain_txt),
		    tas5404_gain_txt),
    SOC_ENUM_SINGLE(TAS5404_CTRL1, 4, ARRAY_SIZE(tas5404_gain_txt),
		    tas5404_gain_txt),
    SOC_ENUM_SINGLE(TAS5404_CTRL1, 6, ARRAY_SIZE(tas5404_gain_txt),
		    tas5404_gain_txt),
};

static const struct snd_kcontrol_new tas5404_snd_controls[] = {
    SOC_ENUM("SPK Analog CH1 Playback Vol", tas5404_gain_txt_enum[0]),
    SOC_ENUM("SPK Analog CH2 Playback Vol", tas5404_gain_txt_enum[1]),
    SOC_ENUM("SPK Analog CH3 Playback Vol", tas5404_gain_txt_enum[2]),
    SOC_ENUM("SPK Analog CH4 Playback Vol", tas5404_gain_txt_enum[3]),
};

static const struct snd_soc_dapm_widget tas5404_dapm_widgets[] = {
    SND_SOC_DAPM_AIF_IN("ANA_AMP Playback", "Playback", 0, SND_SOC_NOPM, 0, 0),
    SND_SOC_DAPM_OUTPUT("ANA_AMP_OUT")
};

static const struct snd_soc_dapm_route tas5404_audio_map[] = {
    {"ANA_AMP_OUT", NULL, "ANA_AMP Playback"},
};

static int tas5404_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_component *component = dai->component;
	struct tas5404_data *tas5404 = snd_soc_component_get_drvdata(component);
	int ret;

	dev_prt(component->dev, "%s() mute=%d\n", __func__, mute);

	if (tas5404->mute_gpio) {
		gpiod_set_value_cansleep(tas5404->mute_gpio, mute);
		return 0;
	}
	/* Unmute(0) disable low-low mute(1) set low-low channels */
	if(mute == 1){
		ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL6,
					TAS5404_CTRL6_ALL_CH_BIT,
					TAS5404_CTRL6_ALL_CH_LOW_LOW);
	}
	else{
		ret = regmap_update_bits(
					tas5404->regmap, TAS5404_CTRL6, TAS5404_CTRL6_ALL_CH_BIT,
					TAS5404_CTRL6_ALL_CH_LOW_LOW_DISABLED);
	}
	if (ret < 0) {
		dev_err(component->dev,
			"Cannot change channels low-low states for "
			"TAS5404_CTRL6: %d\n",
			ret);
	}
	/* Unmute(0) mute(1) channels  */
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL5,
				 TAS5404_CTRL5_UNMUTE_BIT,
				 mute << TAS5404_CTRL5_UNMUTE_OFFSET);

	if (ret < 0){
		dev_err(component->dev, "Cannot mute/un-mute channels TAS5404_CTRL5: %d\n", ret);
	}

	return 0;
}

static int tas5404_power_off(struct snd_soc_codec *codec)
{
	struct tas5404_data *tas5404 = snd_soc_codec_get_drvdata(codec);
	int ret;
	dev_prt(codec->dev, "%s \n",__func__);
	/*mute by I2C*/
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL5,
				 TAS5404_CTRL5_UNMUTE_BIT,
				 1 << TAS5404_CTRL5_UNMUTE_OFFSET);
	if (ret < 0){
		dev_err(codec->dev, "Cannot mute channels TAS5404_CTRL5: %d\n", ret);
	}
	/*Write HIZ state to all channels*/
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL5,
				 TAS5404_CTRL5_ALL_CH_BIT,
				 TAS5404_CTRL5_ALL_CH_HIZ);
	if (ret < 0){
		dev_err(codec->dev, "Cannot set channels to HIZ for TAS5404_CTRL5: %d\n", ret);
	}
	/*Set to low-low state to all channels*/
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL6,
				 TAS5404_CTRL6_ALL_CH_BIT,
				 TAS5404_CTRL6_ALL_CH_LOW_LOW);
	if (ret < 0) {
		dev_err(codec->dev,
			"Cannot set channels to low-low for TAS5404_CTRL6: %d\n",
			ret);
	}
	tas5404->HIZ_flag = false;
	cancel_delayed_work_sync(&tas5404->fault_check_work);
	regcache_cache_only(tas5404->regmap, false);
	regcache_mark_dirty(tas5404->regmap);

/* 	ret = regulator_bulk_disable(ARRAY_SIZE(tas5404->supplies),
				     tas5404->supplies);
	if (ret < 0) {
		dev_err(codec->dev, "failed to disable supplies: %d\n",
			ret);
		return ret;
	} */

	return 0;
}

static int tas5404_power_on(struct snd_soc_codec *codec)
{
	struct tas5404_data *tas5404 = snd_soc_codec_get_drvdata(codec);
	int ret;

	unsigned int reg_val;
	dev_prt(codec->dev, "%s \n",__func__);
	regcache_cache_only(tas5404->regmap, false);

	ret = regcache_sync(tas5404->regmap);
	if (ret < 0) {
		dev_err(codec->dev, "failed to sync regcache: %d\n", ret);
		return ret;
	}
	/* Read fault register 1 */
	ret = regmap_read(tas5404->regmap, TAS5404_FAULT_REG1, &reg_val);
	if (ret < 0) {
		dev_err(codec->dev, "Cannot read register TAS5404_FAULT_REG1: %d\n", ret);
	}

	if (tas5404->mute_gpio) {
		gpiod_set_value_cansleep(tas5404->mute_gpio, 0);
		/*
		 * channels are muted via the mute pin.  Don't also mute
		 * them via the registers so that subsequent register
		 * access is not necessary to un-mute the channels
		 * chan_states = TAS5404_ALL_STATE_PLAY; */
	} else {
		/* chan_states = TAS5404_ALL_STATE_MUTE; */
	}
	/*Un-mute by I2C*/
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL5,
				 TAS5404_CTRL5_UNMUTE_BIT,
				 0 << TAS5404_CTRL5_UNMUTE_OFFSET);
	if (ret < 0){
		dev_err(codec->dev, "Cannot un-mute channels TAS5404_CTRL5: %d\n", ret);
	}

	/*Clear low-low state to all channels*/
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL6,
				 TAS5404_CTRL6_ALL_CH_BIT,
				 TAS5404_CTRL6_ALL_CH_LOW_LOW_DISABLED);
	if (ret < 0) {
		dev_err(
		    codec->dev,
		    "Cannot disable channels low-low states for TAS5404_CTRL6: %d\n",
		    ret);
	}
	tas5404->unmute_start = ktime_get();
	dev_dbg(codec->dev, "tas5404 --set HIZ %lli\n", tas5404->unmute_start);
	mdelay(20);
	tas5404->HIZ_flag = true;
	schedule_delayed_work(&tas5404->fault_check_work,
				      msecs_to_jiffies(TAS5404_FAULT_CHECK_INTERVAL));
	/* any time we come out of HIZ, the output channels automatically run DC
	 * load diagnostics if auto diagnostics are enabled. wait here until this
	 * completes.
	 */

	return 0;
}

static int tas5404_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	dev_prt(codec->dev, "%s() level=%d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (snd_soc_codec_get_bias_level(codec) ==
		    SND_SOC_BIAS_OFF)
			tas5404_power_on(codec);
		break;
	case SND_SOC_BIAS_OFF:
		tas5404_power_off(codec);
		break;
	}

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_tas5404 = {
    .set_bias_level = tas5404_set_bias_level,
	.idle_bias_off = true,
	.component_driver = {
		.controls = tas5404_snd_controls,
		.num_controls = ARRAY_SIZE(tas5404_snd_controls),
		.dapm_widgets = tas5404_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(tas5404_dapm_widgets),
		.dapm_routes = tas5404_audio_map,
		.num_dapm_routes = ARRAY_SIZE(tas5404_audio_map),
	},
};

static struct snd_soc_dai_ops tas5404_speaker_dai_ops = {
//    .hw_params = tas5404_hw_params,
//    .set_fmt = tas5404_set_dai_fmt,
//    .set_tdm_slot = tas5404_set_dai_tdm_slot,
    .digital_mute = tas5404_mute,
};

static struct snd_soc_dai_driver tas5404_dai[] = {
    {
	.name = "tas5404-amplifier",
	.playback =
	    {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 4,
	    },
	.ops = &tas5404_speaker_dai_ops,
    },
};

static void tas5404_fault_check_work(struct work_struct *work)
{
	struct tas5404_data *tas5404 =
	    container_of(work, struct tas5404_data, fault_check_work.work);
	struct device *dev = tas5404->dev;
	unsigned int reg;
	int ret;
	if(tas5404->HIZ_flag == true){
		/* Unmute channels after delay*/
		ktime_t diff = ktime_sub(ktime_get(), tas5404->unmute_start);
		dev_dbg(dev, "tas5404 --clear HIZ diff %lli\n", ktime_to_ms(diff));
		if (ktime_to_ms(diff) < TAS5404_FAULT_CHECK_INTERVAL) {
			goto out;
		}

		ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL5,
				 TAS5404_CTRL5_ALL_CH_BIT,
				 0x0 << TAS5404_CTRL5_UNMUTE_CH1);
		if (ret < 0){
			dev_err(dev, "Cannot un-mute channels TAS5404_CTRL5: %d\n", ret);
		}
		tas5404->HIZ_flag = false;

	}
	ret = regmap_read(tas5404->regmap, TAS5404_FAULT_REG1, &reg);
	if (ret < 0) {
		dev_err(dev, "failed to read FAULT1 register: %d\n", ret);
		goto out;
	}

	/*
	 * Ignore any clock faults as there is no clean way to check for them.
	 * We would need to start checking for those faults *after* the SAIF
	 * stream has been setup, and stop checking *before* the stream is
	 * stopped to avoid any false-positives. However there are no
	 * appropriate hooks to monitor these events.
	 */
	reg &= TAS5404_FAULT_OTW | TAS5404_FAULT_DC_OFFSET |
	       TAS5404_FAULT_OCSD | TAS5404_FAULT_OTSD | TAS5404_FAULT_CP_UV |
	       TAS5404_FAULT_AVDD_UV | TAS5404_FAULT_PVDD_UV |
	       TAS5404_FAULT_AVDD_OV;

	if (!reg) {
		tas5404->last_fault1 = reg;
		goto check_global_fault2_reg;
	}

	/*
	 * Only flag errors once for a given occurrence. This is needed as
	 * the TAS5404 will take time clearing the fault condition internally
	 * during which we don't want to bombard the system with the same
	 * error message over and over.
	 */
	if ((reg & TAS5404_FAULT_OTW) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_OTW))
		dev_crit(dev, "experienced aOvertemperature fault\n");

	if ((reg & TAS5404_FAULT_DC_OFFSET) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_DC_OFFSET))
		dev_crit(dev, "experienced a DC offset fault\n");

	if ((reg & TAS5404_FAULT_OCSD) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_OCSD))
		dev_crit(dev, "experienced a Overcurrent shutdown fault\n");

	if ((reg & TAS5404_FAULT_OTSD) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_OTSD))
		dev_crit(dev, "experienced a Overtemperature shutdown fault\n");

	if ((reg & TAS5404_FAULT_CP_UV) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_CP_UV))
		dev_crit(dev, "experienced a Charge-pump undervoltage fault\n");

	if ((reg & TAS5404_FAULT_AVDD_UV) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_AVDD_UV))
		dev_crit(dev, "experienced a AVDD, analog voltage, "
			      "undervoltage  fault\n");

	if ((reg & TAS5404_FAULT_PVDD_UV) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_PVDD_UV))
		dev_crit(dev, "experienced a PVDD undervoltage  fault\n");

	if ((reg & TAS5404_FAULT_AVDD_OV) &&
	    !(tas5404->last_fault1 & TAS5404_FAULT_AVDD_OV))
		dev_crit(dev, "experienced a PVDD overvoltagefault\n");

	/* Store current fault1 value so we can detect any changes next time */
	tas5404->last_fault1 = reg;

check_global_fault2_reg:
	ret = regmap_read(tas5404->regmap, TAS5404_FAULT_REG2, &reg);
	if (ret < 0) {
		dev_err(dev, "failed to read FAULT2 register: %d\n", ret);
		goto out;
	}

	reg &= TAS5404_FAULT_OTSD_CH1 | TAS5404_FAULT_OTSD_CH2 |
	       TAS5404_FAULT_OTSD_CH3 | TAS5404_FAULT_OTSD_CH4 |
	       TAS5404_FAULT_DC_OFFSET_CH1 | TAS5404_FAULT_DC_OFFSET_CH2 |
	       TAS5404_FAULT_DC_OFFSET_CH3 | TAS5404_FAULT_DC_OFFSET_CH4;

	if (!reg) {
		tas5404->last_fault2 = reg;
		goto check_warn_reg;
	}


	if ((reg & TAS5404_FAULT_OTSD_CH1) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_OTSD_CH1))
		dev_crit(dev, "experienced an overtemp shutdown on CH1\n");

	if ((reg & TAS5404_FAULT_OTSD_CH2) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_OTSD_CH2))
		dev_crit(dev, "experienced an overtemp shutdown on CH2\n");

	if ((reg & TAS5404_FAULT_OTSD_CH3) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_OTSD_CH3))
		dev_crit(dev, "experienced an overtemp shutdown on CH3\n");

	if ((reg & TAS5404_FAULT_OTSD_CH4) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_OTSD_CH4))
		dev_crit(dev, "experienced an overtemp shutdown on CH4\n");

	if ((reg & TAS5404_FAULT_DC_OFFSET_CH1) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_DC_OFFSET_CH1))
		dev_crit(dev, "experienced a DC offset on CH1\n");

	if ((reg & TAS5404_FAULT_DC_OFFSET_CH2) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_DC_OFFSET_CH2))
		dev_crit(dev, "experienced a DC offset  on CH2\n");

	if ((reg & TAS5404_FAULT_DC_OFFSET_CH3) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_DC_OFFSET_CH3))
		dev_crit(dev, "experienced a DC offset  on CH3\n");

	if ((reg & TAS5404_FAULT_DC_OFFSET_CH4) &&
	    !(tas5404->last_fault2 & TAS5404_FAULT_DC_OFFSET_CH4))
		dev_crit(dev, "experienced a DC offset on CH4\n");

	/* Store current fault2 value so we can detect any changes next time */
	tas5404->last_fault2 = reg;

check_warn_reg:
	ret = regmap_read(tas5404->regmap, TAS5404_DIAG_REG1, &reg);
	if (ret < 0) {
		dev_err(dev, "failed to read WARN register: %d\n", ret);
		goto out;
	}

/* 	reg &= TAS5404_WARN_REG; */

	if (!reg) {
		tas5404->last_warn1 = reg;
		goto out;
	}

	/* Store current warn value so we can detect any changes next time */
	tas5404->last_warn1 = reg;

	/* Clear any faults by toggling the CLEAR_FAULT control bit */

out:
	/* Schedule the next fault check at the specified interval */
	schedule_delayed_work(&tas5404->fault_check_work,
			      msecs_to_jiffies(TAS5404_FAULT_CHECK_INTERVAL));
}

static const struct reg_default tas5404_reg_defaults[] = {
    {TAS5404_FAULT_REG1, 0x00},
    {TAS5404_FAULT_REG2, 0x00},
    {TAS5404_DIAG_REG1, 0x00},
    {TAS5404_DIAG_REG2, 0x00},
    {TAS5404_STAT_REG1, 0x00},
    {TAS5404_STAT_REG2, 0x0F},
    {TAS5404_STAT_REG3, 0x00},
    {TAS5404_STAT_REG4, 0x00},
    {TAS5404_CTRL1, 0xAA}, /* 0x80, 0xAA*/
    {TAS5404_CTRL2, 0xF0},
    {TAS5404_CTRL3, 0x0D},
    {TAS5404_CTRL4, 0x50},
    {TAS5404_CTRL5, 0x1F},
    {TAS5404_CTRL6, 0x00},
    {TAS5404_CTRL7, 0x01},
    {TAS5404_STAT_REG5, 0x00},
};

static bool tas5404_is_writable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TAS5404_CTRL1:
	case TAS5404_CTRL2:
	case TAS5404_CTRL3:
	case TAS5404_CTRL4:
	case TAS5404_CTRL5:
	case TAS5404_CTRL6:
	case TAS5404_CTRL7:
		return true;
	default:
		return false;
	}
}

static bool tas5404_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TAS5404_FAULT_REG1:
	case TAS5404_FAULT_REG2:
	case TAS5404_DIAG_REG1:
	case TAS5404_DIAG_REG2:
	case TAS5404_STAT_REG1:
	case TAS5404_STAT_REG2:
	case TAS5404_STAT_REG3:
	case TAS5404_STAT_REG4:
	case TAS5404_CTRL1:
	case TAS5404_CTRL2:
	case TAS5404_CTRL3:
	case TAS5404_CTRL4:
	case TAS5404_CTRL5:
	case TAS5404_CTRL6:
	case TAS5404_CTRL7:
	case TAS5404_STAT_REG5:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config tas5404_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    .writeable_reg = tas5404_is_writable_reg,
    .volatile_reg = tas5404_is_volatile_reg,

    .max_register = TAS5404_MAX,
    .reg_defaults = tas5404_reg_defaults,
    .num_reg_defaults = ARRAY_SIZE(tas5404_reg_defaults),
    .cache_type = REGCACHE_RBTREE,
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id tas5404_of_ids[] = {
    {
	.compatible = "ti,tas5404",
    },
    {},
};
MODULE_DEVICE_TABLE(of, tas5404_of_ids);
#endif

static int tas5404_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct tas5404_data *tas5404;
	int ret;
	int i;
	unsigned int reg_val;
	x9dbgprt("\t[tas5404] probed %s(%d)\n", __FUNCTION__, __LINE__);
	tas5404 = devm_kzalloc(dev, sizeof(*tas5404), GFP_KERNEL);
	if (!tas5404)
		return -ENOMEM;

	dev_set_drvdata(dev, tas5404);

	tas5404->dev = dev;

	tas5404->regmap = devm_regmap_init_i2c(client, &tas5404_regmap_config);
	if (IS_ERR(tas5404->regmap)) {
		ret = PTR_ERR(tas5404->regmap);
		dev_dbg(dev, "unable to allocate register map: %d\n", ret);
		return ret;
	}

	/*
	 * Get control of the standby pin and set it LOW to take the codec
	 * out of the stand-by mode.
	 * Note: The actual pin polarity is taken care of in the GPIO lib
	 * according the polarity specified in the DTS.
	 */
	tas5404->standby_gpio =
	    devm_gpiod_get_optional(dev, "standby", GPIOD_OUT_LOW);
	if (IS_ERR(tas5404->standby_gpio)) {
		dev_prt(dev, "failed to get standby GPIO: %ld\n",
			 PTR_ERR(tas5404->standby_gpio));
		tas5404->standby_gpio = NULL;
	}
	x9dbgprt("\t[tas5404] %s(%d)\n", __FUNCTION__,__LINE__);
	/*
	 * Get control of the mute pin and set it HIGH in order to start with
	 * all the output muted.
	 * Note: The actual pin polarity is taken care of in the GPIO lib
	 * according the polarity specified in the DTS.
	 */
 	tas5404->mute_gpio =
	    devm_gpiod_get_optional(dev, "mute", GPIOD_OUT_HIGH);
	if (IS_ERR(tas5404->mute_gpio)) {
		dev_prt(dev, "failed to get nmute GPIO: %ld\n",
			 PTR_ERR(tas5404->mute_gpio));
		tas5404->mute_gpio = NULL;
	}
	x9dbgprt("\t[tas5404] %s(%d)\n", __FUNCTION__,__LINE__);
	for (i = 0; i < ARRAY_SIZE(tas5404->supplies); i++)
		tas5404->supplies[i].supply = tas5404_supply_names[i];
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(tas5404->supplies),
				      tas5404->supplies);
	if (ret) {
		dev_err(dev, "unable to request supplies: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(tas5404->supplies),
				    tas5404->supplies);
	if (ret) {
		dev_err(dev, "unable to enable supplies: %d\n", ret);
		return ret;
	}

	/* Reset device to establish well-defined startup state */
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL5,
				 TAS5404_CTRL5_RESET_BIT,
				 0 << TAS5404_CTRL5_RESET_OFFSET);
	if (ret) {
		dev_err(dev, "unable to reset device: %d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&tas5404->fault_check_work, tas5404_fault_check_work);

	ret = snd_soc_register_codec(
	    dev, &soc_codec_dev_tas5404, tas5404_dai, ARRAY_SIZE(tas5404_dai));
	if (ret < 0) {
		dev_err(dev, "unable to register codec: %d\n", ret);
		return ret;
	}

	ret = regmap_read(tas5404->regmap, TAS5404_FAULT_REG1, &reg_val);
	if (ret < 0) {
		dev_err(dev, "Cannot read register TAS5404_FAULT_REG1: %d\n", ret);
	}
	x9dbgprt("\t[tas5404] %s(%d)\n", __FUNCTION__,__LINE__);

	/*Set to low-low state to all channels*/
	ret = regmap_update_bits(tas5404->regmap, TAS5404_CTRL6,
				 TAS5404_CTRL6_ALL_CH_BIT,
				 TAS5404_CTRL6_ALL_CH_LOW_LOW);
	if (ret < 0) {
		dev_err(dev,
			"Cannot set channels to low-low for TAS5404_CTRL6: "
			"%d\n",
			ret);
	}
	return 0;
}

static int tas5404_i2c_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct tas5404_data *tas5404 = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&tas5404->fault_check_work);

	/* put the codec in stand-by */
	if (tas5404->standby_gpio)
		gpiod_set_value_cansleep(tas5404->standby_gpio, 1);

/* 	ret = regulator_bulk_disable(ARRAY_SIZE(tas5404->supplies),
				     tas5404->supplies);
	if (ret < 0) {
		dev_err(dev, "unable to disable supplies: %d\n", ret);
		return ret;
	} */

	return 0;
}

static const struct i2c_device_id tas5404_i2c_ids[] = {{"tas5404", 0}, {}};
MODULE_DEVICE_TABLE(i2c, tas5404_i2c_ids);

static struct i2c_driver tas5404_i2c_driver = {
    .driver =
	{
	    .name = "tas5404",
	    .of_match_table = of_match_ptr(tas5404_of_ids),
	},
    .probe = tas5404_i2c_probe,
    .remove = tas5404_i2c_remove,
    .id_table = tas5404_i2c_ids,
};
module_i2c_driver(tas5404_i2c_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("TAS5404 Audio amplifier driver");
MODULE_LICENSE("GPL");