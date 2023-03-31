#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "serdes_9xx.h"

//#define SERDES_DEBUG

static int du90ub94x_i2c_read(struct i2c_client *client,
	uint8_t addr, u8 reg, u8 *buf, int len)
{
    struct i2c_msg msgs[2];
    int ret;

    msgs[0].flags = 0;
    msgs[0].addr  = addr;
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = addr;
    msgs[1].len   = len;
    msgs[1].buf   = buf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

static int du90ub94x_i2c_write(struct i2c_client *client,
	uint8_t addr, u8 reg, u8 *buf, int len)
{
    u8 *addr_buf;
    struct i2c_msg msg;
    int ret;

    addr_buf = kmalloc(len + 1, GFP_KERNEL);

    if (!addr_buf){
		printk(KERN_ERR "%s, alloc mem fail on addr=%#x, reg=%#x\n",
			__func__, addr, reg);
        return -ENOMEM;
    }

    addr_buf[0] = reg;
    memcpy(&addr_buf[1], buf, len);

    msg.flags = 0;
    msg.addr = addr;
    msg.buf = addr_buf;
    msg.len = len + 1;

    ret = i2c_transfer(client->adapter, &msg, 1);
    kfree(addr_buf);
    return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

int du90ub941_or_947_enable_port(struct i2c_client *client,
	uint8_t addr, enum serdes_9xx_port port_flag)
{
	int ret;
	u8 sreg = 0x1e, sval = 0;

    if (port_flag == PRIMARY_PORT)
        sval = 0x1;
    else if (port_flag == SECOND_PORT)
        sval = 0x2;
    else if (port_flag == DOUBLE_PORT)
		sval = 0x4;
	else{
		dev_err(&client->dev, "%s, port_falg invalid %d\n", __func__, port_flag);
		return -1;
	}

    ret = du90ub94x_i2c_write(client, addr, sreg, &sval, 1);

#ifdef SERDES_DEBUG
	int ret1;
	u8 ret_val = 0;

	ret1 = du90ub94x_i2c_read(client, addr, sreg, &ret_val, 1);
	dev_err(&client->dev, "%s, ret1=%d, ret_val=%#x\n", __func__, ret1, ret_val);
#endif

	return ret;
}
EXPORT_SYMBOL(du90ub941_or_947_enable_port);

int du90ub941_or_947_enable_i2c_passthrough(struct i2c_client *client, uint8_t addr)
{
	int ret;
	u8 sreg = 0x17, sval = 0x9e;

	ret = du90ub94x_i2c_write(client, addr, sreg, &sval, 1);

#ifdef SERDES_DEBUG
	int ret1;
	u8 ret_val = 0;

	ret1 = du90ub94x_i2c_read(client, addr, sreg, &ret_val, 1);
	dev_err(&client->dev, "%s, ret1=%d, ret_val=%#x\n", __func__, ret1, ret_val);
#endif

	return ret;
}
EXPORT_SYMBOL(du90ub941_or_947_enable_i2c_passthrough);

int du90ub941_or_947_enable_int(struct i2c_client *client, uint8_t addr)
{
	int ret;
	u8 sreg = 0xc6, sval = 0x21;

	ret = du90ub94x_i2c_write(client, addr, sreg, &sval, 1);

#ifdef SERDES_DEBUG
	int ret1;
	u8 ret_val = 0;

	ret1 = du90ub94x_i2c_read(client, addr, sreg, &ret_val, 1);
	dev_err(&client->dev, "%s, ret1=%d, ret_val=%#x\n", __func__, ret1, ret_val);
#endif

	return ret;
}
EXPORT_SYMBOL(du90ub941_or_947_enable_int);

int du90ub948_gpio_output(struct i2c_client *client,
	uint8_t addr, int gpio, int val)
{
	int ret;
	u8 dreg = 0, dval = 0;

	switch (gpio) {
	case 0:
		dreg = 0x1d;
		break;
	case 1:
	case 2:
		dreg = 0x1e;
		break;
	case 3:
		dreg = 0x1f;
		break;
	case 5:
	case 6:
		dreg = 0x20;
		break;
	case 7:
	case 8:
		dreg = 0x21;
		break;
	default:
		dev_err(&client->dev, "%s, 948(0) gpio num %d invalid\n", __func__, gpio);
		return -1;
    }

	ret = du90ub94x_i2c_read(client, addr, dreg, &dval, 1);
	if(ret) {
		dev_err(&client->dev, "%s, i2c read gpio(%d) fail\n", __func__, gpio);
	}

	switch (gpio) {
	case 0:
	case 1:
	case 3:
	case 5:
	case 7:
		dval &= 0xf0;
		if (val == 1)
			dval |= 0x09;
		else
			dval |= 0x01;
		break;
	case 2:
	case 6:
	case 8:
		dval &= 0x0f;
		if (val == 1)
			dval |= 0x90;
		else
			dval |= 0x10;
		break;
	default:
		dev_err(&client->dev, "%s, 948(1) gpio num %d invalid\n", __func__, gpio);
		return -1;
    }

	ret = du90ub94x_i2c_write(client, addr, dreg, &dval, 1);

#ifdef SERDES_DEBUG
	int ret1;
	u8 ret_val = 0;

	ret1 = du90ub94x_i2c_read(client, addr, dreg, &ret_val, 1);
	dev_err(&client->dev, "%s, gpio(%d), ret1=%d, ret_val=%#x\n", __func__, gpio, ret1, ret_val);
#endif

    return ret;
}
EXPORT_SYMBOL(du90ub948_gpio_output);

int du90ub948_gpio_input(struct i2c_client *client, uint8_t addr, int gpio)
{
	int ret;
	u8 dreg = 0, dval = 0;

	switch (gpio) {
	case 0:
		dreg = 0x1d;
		break;
	case 1:
	case 2:
		dreg = 0x1e;
		break;
	case 3:
		dreg = 0x1f;
		break;
	case 5:
	case 6:
		dreg = 0x20;
		break;
	case 7:
	case 8:
		dreg = 0x21;
		break;
	default:
		dev_err(&client->dev, "%s, 948(0) gpio num %d invalid\n", __func__, gpio);
		return -1;
    }

	ret = du90ub94x_i2c_read(client, addr, dreg, &dval, 1);
	if(ret) {
		dev_err(&client->dev, "%s, i2c read gpio(%d) fail\n", __func__, gpio);
	}

	switch (gpio) {
	case 0:
	case 1:
	case 3:
	case 5:
	case 7:
		dval &= 0xf0;
		dval |= 0x03;
		break;
	case 2:
	case 6:
	case 8:
		dval &= 0x0f;
		dval |= 0x30;
		break;
	default:
		dev_err(&client->dev, "%s, 948(1) gpio num %d invalid\n", __func__, gpio);
		return -1;
    }

	ret = du90ub94x_i2c_write(client, addr, dreg, &dval, 1);

#ifdef SERDES_DEBUG
	int ret1;
	u8 ret_val = 0;

	ret1 = du90ub94x_i2c_read(client, addr, dreg, &ret_val, 1);
	dev_err(&client->dev, "%s, gpio(%d), ret1=%d, ret_val=%#x\n", __func__, gpio, ret1, ret_val);
#endif

    return ret;
}
EXPORT_SYMBOL(du90ub948_gpio_input);

int du90ub941_or_947_gpio_input(struct i2c_client *client, uint8_t addr, int gpio)
{
	int ret;
	u8 sreg = 0, sval = 0;

	switch (gpio) {
	case 0:
		sreg = 0x0d;
		break;
	case 1:
	case 2:
		sreg = 0x0e;
		break;
	case 3:
		sreg = 0x0f;
		break;
	case 5:
	case 6:
		sreg = 0x10;
		break;
	case 7:
	case 8:
		sreg = 0x11;
		break;
	default:
		dev_err(&client->dev, "%s, --gpio num %d invalid\n", __func__, gpio);
		return -1;
    }

	ret = du90ub94x_i2c_read(client, addr, sreg, &sval, 1);
	if(ret) {
		dev_err(&client->dev, "%s, i2c read gpio(%d) fail\n", __func__, gpio);
	}

	switch (gpio) {
	case 0:
	case 1:
	case 3:
		sval &= 0xf0;
		sval |= 0x05;
		break;
	case 2:
		sval &= 0x0f;
		sval |= 0x50;
		break;
	case 5:
	case 7:
		sval &= 0xf0;
		sval |= 0x03;
		break;
	case 6:
	case 8:
		sval &= 0x0f;
		sval |= 0x30;
		break;
	default:
		dev_err(&client->dev, "%s, ++gpio num %d invalid\n", __func__, gpio);
		return -1;
    }

	ret = du90ub94x_i2c_write(client, addr, sreg, &sval, 1);

#ifdef SERDES_DEBUG
	int ret1;
	u8 ret_val = 0;

	ret1 = du90ub94x_i2c_read(client, addr, sreg, &ret_val, 1);
	dev_err(&client->dev, "%s, gpio(%d), ret1=%d, ret_val=%#x\n", __func__, gpio, ret1, ret_val);
#endif

    return ret;
}
EXPORT_SYMBOL(du90ub941_or_947_gpio_input);

