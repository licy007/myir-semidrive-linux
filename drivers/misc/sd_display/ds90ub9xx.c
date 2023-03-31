#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/time.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>

#define CHECK_LINK_CONT 20
#define CHECK_LINK_DELAY 200

enum ds90ub9xx_ic {
	DS90UB941 = 0,
	DS90UB948,
	DS90UB9XX_MAX
};

enum ds_gpio_direct {
	INPUT,
	OUTPUT,
};

enum ds90ub9xx_gpio {
	GPIO0 = 0,
	GPIO1,
	GPIO2,
	GPIO3,
	GPIO_MAX,
};

struct ds90ub9xx_data {
	struct i2c_client *client;
	u8 addr_ds9xx[DS90UB9XX_MAX];
	u32 remote_gpio;
	struct delayed_work dwork;
};


struct ds_gpio_config {
	enum ds90ub9xx_gpio gpio;
	uint8_t ser_reg;
	uint8_t des_reg;
	uint8_t value_mask;
	uint8_t ser_val;
	uint8_t des_val;
};

static int ds90ub9xx_i2c_read(enum ds90ub9xx_ic ic ,struct ds90ub9xx_data *data,
	u8 reg, u8 *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].flags = 0;
	msgs[0].addr  = data->addr_ds9xx[ic];
	msgs[0].len   = 1;
	msgs[0].buf   = &reg;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = data->addr_ds9xx[ic];
	msgs[1].len   = len;
	msgs[1].buf   = buf;

	ret = i2c_transfer(data->client->adapter, msgs, 2);
	ret = ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
	if (ret) {
		pr_err("error: %s: read reg = 0x%x, val = 0x%x\n", __func__, reg, *buf);
	}
	return ret;
}

static int ds90ub9xx_i2c_write(enum ds90ub9xx_ic ic, struct ds90ub9xx_data *data,
	u8 reg, u8 *buf, int len)
{
	u8 *addr_buf;
	struct i2c_msg msg;
	int ret;

	addr_buf = kmalloc(len + 1, GFP_KERNEL);

	if (!addr_buf)
		return -ENOMEM;

	addr_buf[0] = reg;
	memcpy(&addr_buf[1], buf, len);

	msg.flags = 0;
	msg.addr  = data->addr_ds9xx[ic];
	msg.buf   = addr_buf;
	msg.len   = len + 1;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	kfree(addr_buf);
	ret = ret < 0 ? ret : (ret != 1 ? -EIO : 0);
	if (ret) {
		pr_err("error: %s: write reg = 0x%x, val = 0x%x\n", __func__, reg, *buf);
	}
	return ret;
}

static int ds90ub9xx_i2c_writebyte_mask(enum ds90ub9xx_ic ic, struct ds90ub9xx_data *data,	uint8_t reg, uint8_t buf, uint8_t mask)
{
	uint8_t val;
	int ret;

	ret = ds90ub9xx_i2c_read(ic, data, reg, &val, 1);
	if(ret) {
		pr_err("error: %s: read reg = 0x%x, val = 0x%x\n", __func__, reg, val);
		return ret;
	}

	val &= ~mask;
	val |= buf;

	ret = ds90ub9xx_i2c_write(ic, data, reg, &val,1);
	if(ret) {
		pr_err("error: %s: write reg = 0x%x, val = 0x%x\n", __func__, reg, val);
		return ret;
	}
	return 0;
}

static int ds90ub94x_948_gpio_config(struct ds90ub9xx_data *data, uint8_t direct)
{
	uint8_t i;
	int ret;
	enum ds90ub9xx_ic ic = DS90UB941;

	struct ds_gpio_config gpc[] = {
		{GPIO0, 0x0D, 0x1D, 0x0F, 0, 0},
		{GPIO1, 0x0E, 0x1E, 0x0F, 0, 0},
		{GPIO2, 0x0E, 0x1E, 0xF0, 0, 0},
		{GPIO3, 0x0F, 0x1F, 0x0F, 0, 0},
	};

	if (data->remote_gpio == GPIO_MAX)
		return 0;

	for (i = 0; i < sizeof(gpc)/sizeof(gpc[0]); i++) {
		if(data->remote_gpio == gpc[i].gpio) {
			if (direct == INPUT) {
				gpc[i].ser_val = 0x5;
				gpc[i].des_val = 0x3;
			} else if (direct == OUTPUT){
				gpc[i].ser_val = 0x3;
				gpc[i].des_val = 0x5;
			} else {
				pr_err("[err] %s :config ds90ub941/948 direct[%d] value is out of value ! \n",
				__func__, direct);
				return -1;
			}

			ret = ds90ub9xx_i2c_writebyte_mask(ic, data, gpc[i].ser_reg, gpc[i].ser_val, gpc[i].value_mask);

			ic = DS90UB948;
			ret |= ds90ub9xx_i2c_writebyte_mask(ic, data, gpc[i].des_reg, gpc[i].des_val, gpc[i].value_mask);
			if (ret)
				break;

			pr_info("%s:config ds90ub941/948 gpio[%d] direct[%d] succesed !\n", __func__, data->remote_gpio, direct);
			return 0;
		}
	}

	pr_err("[err] %s:config ds90ub941/948 gpio[%d] direct[%d] failed !\n", __func__, data->remote_gpio, direct);
	return -1;
}

static void ds90ub941_init(struct ds90ub9xx_data *data)
{
	u8 dreg, dval;
	enum ds90ub9xx_ic ic = DS90UB941;
	pr_info("%s enter\n", __func__);

	//dreg = 0x01; dval = 0x8;
	//ds90ub9xx_i2c_write(ic, data, dreg, &dval,1); //Disable DSI
	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x1E; dval = 0x04;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Use I2D ID+1 for FPD-Link III Port 1 register access
	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x03; dval = 0x9A;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Enable I2C_PASSTHROUGH, FPD-Link III Port 0
	dreg = 0x1E; dval = 0x02;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 1
	dreg = 0x03; dval = 0x9A;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Enable I2C_PASSTHROUGH, FPD-Link III Port 1

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x40; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 0 digital registers
	dreg = 0x41; dval = 0x21;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI_CONFIG_1 register
	dreg = 0x42; dval = 0x60;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_VS_POLARITY=DSI_HS_POLARITY=1(active low)

	dreg = 0x1E; dval = 0x02;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 1
	dreg = 0x40; dval = 0x09;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 1 digital registers
	dreg = 0x41; dval = 0x21;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI_CONFIG_1 register
	dreg = 0x42; dval = 0x60;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_VS_POLARITY=DSI_HS_POLARITY=1(active low)

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x5B; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Force Independent 2:2 mode
	//dreg = 0x5B; dval = 0x01;
	//ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Forced Single FPD-Link III Transmitter mode (Port 1 disabled)
	dreg = 0x4F; dval = 0x8C;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_CONTINUOUS_CLOCK, 4 lanes, DSI Port 0

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x40; dval = 0x04;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 0 digital registers
	dreg = 0x41; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DPHY_SKIP_TIMING register
#if defined(MIPI_KD070_SERDES_1024X600_LCD)
	dreg = 0x42; dval = 0x10;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 200 MHz DSI clock frequency (1920x720, Round(65x0.2) -5)
#else
	dreg = 0x42; dval = 0x15;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 300 MHz DSI clock frequency (1920x720, Round(65x0.3) -5)
#endif

	dreg = 0x1E; dval = 0x02;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 1
	dreg = 0x4F; dval = 0x8C;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Set DSI_CONTINUOUS_CLOCK, 4 lanes, DSI Port 1

	dreg = 0x1E; dval = 0x01;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select FPD-Link III Port 0
	dreg = 0x40; dval = 0x08;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DSI Port 1 digital registers
	dreg = 0x41; dval = 0x05;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Select DPHY_SKIP_TIMING register
#if defined(MIPI_KD070_SERDES_1024X600_LCD)
	dreg = 0x42; dval = 0x10;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 200 MHz DSI clock frequency (1920x720, Round(65x0.2) -5)
#else
	dreg = 0x42; dval = 0x15;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Write TSKIP_CNT value for 300 MHz DSI clock frequency (1920x720, Round(65x0.3) -5)
#endif
	dreg = 0x10; dval = 0x00;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//Enable DSI

	pr_info("%s done\n", __func__);
}

static void ds90ub948_init(struct ds90ub9xx_data *data)
{
	u8 dreg, dval;
	enum ds90ub9xx_ic ic = DS90UB948;
	pr_info("%s enter\n", __func__);

#if defined(MIPI_KD070_SERDES_1024X600_LCD)
	if (0) {/*lastest hardware this not need cfg*/
		dreg = 0x1E; dval = 0x09;
		ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);
		ds90ub9xx_i2c_read(ic, data, dreg, &dval, 1);
		pr_info("%s: read: dreg=0x1D, dval=0x%x\n", __func__, dval);

		dreg = 0x20; dval = 0x09;
		ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);
		ds90ub9xx_i2c_read(ic, data, dreg, &dval, 1);
		pr_info("%s: read: dreg=0x20, dval=0x%x\n", __func__, dval);
	}
#else
	dreg = 0x49; dval = 0x60;
	ds90ub9xx_i2c_write(ic, data, dreg, &dval,1);//set MAPSEL high
	ds90ub9xx_i2c_read(ic, data, dreg, &dval, 1);
#endif

	ds90ub94x_948_gpio_config(data, OUTPUT);

	pr_info("%s done\n", __func__);
}

static void ds90ub9xx_deserdes_init(struct work_struct *work)
{
	struct ds90ub9xx_data *data = NULL;
	static u8 entry_cont = 0;
	u8 dreg, dval;
	enum ds90ub9xx_ic ic = DS90UB941;

	data = container_of(work, struct ds90ub9xx_data, dwork.work);
	if (!data) {
		pr_err("data is null\n");
		return;
	};

	dreg = 0x5a; dval = 0x00;
	ds90ub9xx_i2c_read(ic, data, dreg, &dval, 1);
	printk_ratelimited(KERN_INFO "%s: read: dreg=0x5a, dval=0x%x\n", __func__, dval);

	if (dval & 0x80) {
		ds90ub948_init(data);
		return;
	}

	entry_cont++;

	if (entry_cont < CHECK_LINK_CONT)
		schedule_delayed_work(&data->dwork, msecs_to_jiffies(CHECK_LINK_DELAY));
	else
		pr_info("%s: deserdes link timeout\n", __func__);
}

static int ds90ub9xx_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ds90ub9xx_data *data;
	int ret = 0;
	u32 val;

	pr_info("%s : I2C Address: 0x%02x\n", __func__, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("I2C check functionality failed.\n");
		return -ENXIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	data->client = client;
	i2c_set_clientdata(client, data);

	/*get resource from dts*/
	ret = of_property_read_u32(client->dev.of_node, "addr_ds941", &val);
	if (ret < 0) {
		pr_err("Missing addr_ds941\n");
	}

	data->addr_ds9xx[DS90UB941] = (u8)val;

	ret = of_property_read_u32(client->dev.of_node, "addr_ds948", &val);

	if (ret < 0) {
		pr_err("Missing addr_ds948\n");
	}

	data->addr_ds9xx[DS90UB948] = (u8)val;

	pr_info("data->addr_ds941=0x%x, data->addr_ds948=0x%x\n",
		data->addr_ds9xx[DS90UB941], data->addr_ds9xx[DS90UB948]);

	ret = of_property_read_u32(client->dev.of_node, "remote-gpio", &val);
	if (ret < 0) {
		val = GPIO_MAX;
		ret = 0;
		pr_info("remote gpio don't config\n");
	}

	data->remote_gpio = val;
	pr_info("ds90ub9xx remoute gpio:%d\n", data->remote_gpio);

	ds90ub941_init(data);

	INIT_DELAYED_WORK(&data->dwork, ds90ub9xx_deserdes_init);

	schedule_delayed_work(&data->dwork, 0);
	return ret;
}

static int ds90ub9xx_remove(struct i2c_client *pdev)
{
	return 0;
}

static const struct of_device_id ds90ub9xx_dt_ids[] = {
	{.compatible = "semidrive,ds90ub9xx",},
	{}
};

static struct i2c_driver ds90ub9xx_driver = {
	.probe = ds90ub9xx_probe,
	.remove = ds90ub9xx_remove,
	.driver = {
		.name = "ds90ub9xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ds90ub9xx_dt_ids),
	},
};

module_i2c_driver(ds90ub9xx_driver);

MODULE_DESCRIPTION("DS90U9XX Driver");
MODULE_LICENSE("GPL");
