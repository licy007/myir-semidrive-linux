/**
 * @file xf6020.c
 * @author shao yi (yi.shao@semidrive.com)
 * @brief xf6020 module dsp driver.
 * @version 0.1
 * @date 2020-08-11
 *
 * @copyright Copyright (c) 2020 semdrive
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include "xf6020.h"

#define XF6020_DEBUG 1

#ifdef XF6020_DEBUG
#define x9dbgprt printk
#else
#define x9dbgprt(format, arg...) do {} while (0)
#endif

struct xf6020_data {
	struct device *dev; /*!<  device*/
	struct i2c_client *i2c;/*!<  i2c adaptor*/
	struct gpio_desc *reset_gpio;/*!< low active reset pin */
	struct regmap *regmap; /*!<  registers mapping */
};

/*
 * Register (0x0) Voice Mode Register
 */
static const char * const xf6020_voice_mode[] = {
	"FourArea", "TwoArea"
};

/*
 * Register (0x1) Parameter Register
 */
static const char * const xf6020_param_mode[] = {
	"Tel", /*0: Bluetooth EC mode  */
	"Music", /*1: Music EC mode */
	"Beam0", /*2:  Front Left Beam */
	"Beam1", /*3:  Front Right Beam */
	"Beam2", /*4:  Rear Left Beam*/
	"Beam3", /*5:  Rear Right Beam*/
	"Beam Off", /*6: Beam Off*/
	"Wakeup Two",/*7: Two Way Wakeup Mode*/
	"Wakeup Four",/*8: Four Way Wakeup Mode*/
	"Wakeup Off",/*9: Wakeup off mode*/
	"Rec Beam",/*10: Recognize Narrow Beam Mode*/
	"Rec MAE",/*11: Recognize Omnidirection Mode*/
	"Rec Off",/*12: Recognize off mode*/
	"Dual MAE",/*13 dual area Omnidirection*/
	"Dual MAB",/*14 dual area Narrow Beam*/
	"Dual MAE MICL",
	"Dual MAE MICR",
	"Dual MAE REFL",
	"Dual MAE REFR",
	"Dual MAE REFMIX",
	"Dual Phone"
};

static const struct soc_enum xf6020_enum[] = {
	SOC_ENUM_SINGLE(XF6020_REG_VOICE_MODE, 0, ARRAY_SIZE(xf6020_voice_mode), xf6020_voice_mode),
	SOC_ENUM_SINGLE(XF6020_REG_PARAM, 0, ARRAY_SIZE(xf6020_param_mode), xf6020_param_mode),
};


static const struct snd_kcontrol_new xf6020_snd_controls[] = {
	SOC_ENUM("Voice Mode", xf6020_enum[0]),
	SOC_ENUM("Param", xf6020_enum[1]),
	SOC_SINGLE("Bypass Mode", XF6020_REG_BYPASS, 0, 1, 0),
};

static const struct snd_soc_dapm_widget xf6020_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("SAI1_IN"),
	SND_SOC_DAPM_AIF_OUT("SAI1_OUT", "XF Capture", 0, SND_SOC_NOPM, 0, 0),
	//SND_SOC_DAPM_OUTPUT("SAI1_OUT"),
};

static const struct snd_soc_dapm_route xf6020_audio_routes[] = {

	{ "SAI1_OUT", NULL, "SAI1_IN" },
	//{ "SAI1_OUT",NULL,"XF Capture" },
};

static int xf6020_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	unsigned int rate = params_rate(params);
	unsigned int width = params_width(params);

	dev_dbg(component->dev, "%s() rate=%u width=%u\n", __func__, rate, width);

	return 0;
}

static int xf6020_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

static struct snd_soc_dai_ops xf6020_dai_ops = {
	.hw_params	= xf6020_hw_params,
	.set_fmt = xf6020_set_dai_fmt,
};

static struct snd_soc_dai_driver xf6020_dai[] = {
	{
		.name = "xf6020-codec",
		.capture = {
			.stream_name = "XF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = XF6020_RATES,
			.formats = XF6020_FORMATS,
		},
		.ops = &xf6020_dai_ops,
	},
};

static unsigned int xf6020_i2c_read(
	struct i2c_client *client, u8 *reg, int reglen, void *data,int datalen)
{
	struct i2c_msg xfer[2];
	int ret;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = data;

	ret = i2c_transfer(client->adapter, xfer, 2);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return -ret;
	else
		return -EIO;
}

/**
 * @brief 16 bit reg write command.
 *
 * @param codec
 * @param reg: register address
 * @param value:
 * @return int : a negative value on error, 0 on success
 */
static int xf6020_write_register(struct snd_soc_codec *codec,  unsigned int reg,  unsigned int value)
{
	struct xf6020_data *xf6020 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	printk("FUNC %s %d \n,", __func__, __LINE__);
	if(reg != XF6020_REG_VERSION){
		ret = regmap_write(xf6020->regmap, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}
	return ret;
}

/**
 * @brief read codec register
 *
 * @param codec
 * @param reg
 * @return unsigned int result of register
 */
unsigned int xf6020_read_register(struct snd_soc_codec *codec,  unsigned int reg)
{
	struct xf6020_data *xf6020 = snd_soc_codec_get_drvdata(codec);
	int ret;
	u32 rdata = 0;
	unsigned char tx[2];
	int	wlen, rlen;
	printk("FUNC %s %d \n,", __func__, __LINE__);
	if (reg != XF6020_REG_VERSION) {
		ret = regmap_read(xf6020->regmap, reg, &rdata);
		if (ret != 0)
			dev_err(codec->dev, "Cache read to %x failed: %d\n",
				reg, ret);
		return rdata;
	}
	/* xf6020 reg version will return 4 bytes data */
	wlen = 2;
	rlen = 4;
	tx[0] = (unsigned char)(0xFF & (reg >> 8));
	tx[1] = (unsigned char)(0xFF & reg);
	ret = xf6020_i2c_read(xf6020->i2c, tx, wlen, (void *)&rdata, rlen);
	if (ret < 0) {
		x9dbgprt("[xf6020] %s error ret = %d \n", __FUNCTION__,__LINE__);
		rdata = -EIO;
	}
	return rdata;
}

/* static int xf6020_probe(struct snd_soc_codec *codec)
{

	int ver;
	// Need 100ms delay after reset to get version
	msleep(100);
	ver= snd_soc_read(codec, XF6020_REG_VERSION);
	dev_info(codec->dev, "xf6020 version(%d 0x%x)\n",
				ver,ver);
	return 0;
} */

static struct snd_soc_codec_driver soc_codec_dev_xf6020 = {
	/* xf6020_probe could get xf6020's version.
	.probe			= xf6020_probe,  */
	.write = xf6020_write_register,
	.read = xf6020_read_register,
	.idle_bias_off = true,
	.component_driver = {
		.controls		= xf6020_snd_controls,
		.num_controls		= ARRAY_SIZE(xf6020_snd_controls),
		.dapm_widgets		= xf6020_dapm_widgets,
		.num_dapm_widgets	= ARRAY_SIZE(xf6020_dapm_widgets),
		.dapm_routes		= xf6020_audio_routes,
		.num_dapm_routes	= ARRAY_SIZE(xf6020_audio_routes),
	},
};

static const struct reg_default tas5404_reg_defaults[] = {
	{ XF6020_REG_VOICE_MODE,			0x01 },
	{ XF6020_REG_PARAM,					0x0E },
	{ XF6020_REG_BYPASS,				0x00 },
	{ XF6020_REG_VERSION,				0x22 }, /*0x3122 12578*/
};

static bool xf6020_is_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case XF6020_REG_VOICE_MODE:
		case XF6020_REG_PARAM:
		case XF6020_REG_BYPASS:
		case XF6020_REG_VERSION:
		return true;
	default:
		return false;
	}
}

static bool xf6020_is_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case XF6020_REG_VOICE_MODE:
		case XF6020_REG_PARAM:
		case XF6020_REG_BYPASS:
		return true;
	default:
		return false;
	}
}

static bool xf6020_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case XF6020_REG_VOICE_MODE:
		case XF6020_REG_PARAM:
		case XF6020_REG_BYPASS:
		case XF6020_REG_VERSION:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config xf6020_regmap = {
	.reg_bits = 16,
	.val_bits = 8,

	.max_register = XF6020_REG_VERSION,
	.readable_reg = xf6020_is_readable_reg,
	.volatile_reg = xf6020_is_volatile_reg,
	.writeable_reg = xf6020_is_writeable_reg,

	.reg_defaults = tas5404_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tas5404_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id xf6020_of_ids[] = {
	{ .compatible = "xf6020", },
	{ },
};
MODULE_DEVICE_TABLE(of, xf6020_of_ids);
#endif

/**
 * @brief xf6020 chip initialization
 *
 * @param codec
 * @return int
 */
static int xf6020_init_reg(struct device *dev)
{
	struct xf6020_data *xf6020 = dev_get_drvdata(dev);
	int ret = 0;
	/* reset xf6020 chip */
	if(xf6020->reset_gpio != NULL){
		x9dbgprt("[xf6020] %s(%d) reset chip \n", __FUNCTION__,__LINE__);
		/*Reset active keep low 10ms*/
		gpiod_set_value_cansleep(xf6020->reset_gpio, 1);
		msleep(10);
		/*Reset de-active and need 400ms to enter normal mode.*/
		gpiod_set_value_cansleep(xf6020->reset_gpio, 0);
		msleep(10);
	}
	return ret;
}

/**
 * @brief xf6020 probe function from i2c device.
 *
 * @param client
 * @param id
 * @return int
 */
static int xf6020_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct xf6020_data *xf6020;
	int ret;

	x9dbgprt("[xf6020] %s(%d)\n", __FUNCTION__, __LINE__);
	/* Alloc driver data */
	xf6020 = devm_kzalloc(dev, sizeof(*xf6020), GFP_KERNEL);
	if (!xf6020)
		return -ENOMEM;

	dev_set_drvdata(dev, xf6020);

	xf6020->dev = dev;
	xf6020->i2c	= client;
	/* Init xf6020 registers */

	xf6020->regmap = devm_regmap_init_i2c(client, &xf6020_regmap);
	if (IS_ERR(xf6020->regmap)) {
		ret = PTR_ERR(xf6020->regmap);
		dev_err(dev, "unable to allocate register map: %d \n", ret);
		return ret;
	}

	/* Reset device to establish well-defined startup state write 1 to reset*/
 	xf6020->reset_gpio  = devm_gpiod_get_optional(dev, "reset",
						      GPIOD_OUT_HIGH);

	if (IS_ERR(xf6020->reset_gpio)) {
		if (PTR_ERR(xf6020->reset_gpio) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_info(dev, "failed to get reset GPIO: %ld\n",
			PTR_ERR(xf6020->reset_gpio));
		xf6020->reset_gpio = NULL;
	}
	/* Init xf6020 register */
	xf6020_init_reg(dev);

	ret = snd_soc_register_codec(dev, &soc_codec_dev_xf6020, xf6020_dai,ARRAY_SIZE(xf6020_dai));
	if(ret < 0){
		dev_err(dev,"unable register codec:%d \n",ret);
		return ret;
	}
	return 0;
}

static int xf6020_i2c_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct xf6020_data *xf6020 = dev_get_drvdata(dev);
	/* put the dsp in reset status */
	if (xf6020->reset_gpio)
		gpiod_set_value_cansleep(xf6020->reset_gpio, 0);
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id xf6020_i2c_ids[] = {
	{ "iflytek,xf6020", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, xf6020_i2c_ids);

static struct i2c_driver xf6020_i2c_driver = {
	.driver = {
		.name = "xf6020",
		.of_match_table = of_match_ptr(xf6020_of_ids),
	},
	.probe = xf6020_i2c_probe,
	.remove = xf6020_i2c_remove,
	.id_table = xf6020_i2c_ids,
};
module_i2c_driver(xf6020_i2c_driver);

MODULE_AUTHOR("Shao Yi <yi.shao@semidrive.com>");
MODULE_DESCRIPTION("dsp xf6020 driver");
MODULE_AUTHOR("yi.shao@semidrive.com");
