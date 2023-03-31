/*
*   FILE NAME  : spi-is62w.c
*
*   IRAM Ltd.
*	spi-is62w.c
*   By Li Nan Tech
*   DEMO Version: 1.0 Data: 2021-8-20
*
*   DESCRIPTION: Implements an interface for the is62w of spi interface
*
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/spi/spi.h>
#include <linux/timer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include "spi-is62w.h"

int is62w_spi_reset(struct is62w_spi_dev *is62w)
{
	struct spi_message m;
	struct spi_transfer t;
	int ret;
	u8 cmd_addr = IS62W_RSTDQI;

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = &cmd_addr;
	t.len = 1;
	spi_message_add_tail(&t, &m);

	mutex_lock(&is62w->lock);

	ret = spi_sync(is62w->spi, &m);
	/* have to wait at least Tcsl ns */
	ndelay(250);
	if (ret)
		dev_err(&is62w->spi->dev, "is62w reset error: %d\n", ret);

	mutex_unlock(&is62w->lock);
	return ret;
}

static int is62w_spi_esdi(struct is62w_spi_dev *is62w)
{
	struct spi_message m;
	struct spi_transfer t;
	int ret;
	u8 cmd_addr = IS62W_ESDI;

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = &cmd_addr;
	t.len = 1;
	spi_message_add_tail(&t, &m);

	mutex_lock(&is62w->lock);

	ret = spi_sync(is62w->spi, &m);
	/* have to wait at least Tcsl ns */
	ndelay(250);
	if (ret)
		dev_err(&is62w->spi->dev, "is62w esdi error: %d\n", ret);

	mutex_unlock(&is62w->lock);
	return ret;
}

static int is62w_spi_esqi(struct is62w_spi_dev *is62w)
{
	struct spi_message m;
	struct spi_transfer t;
	int ret;
	u8 cmd_addr = IS62W_ESQI;

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = &cmd_addr;
	t.len = 1;
	spi_message_add_tail(&t, &m);

	mutex_lock(&is62w->lock);

	ret = spi_sync(is62w->spi, &m);
	/* have to wait at least Tcsl ns */
	ndelay(250);
	if (ret)
		dev_err(&is62w->spi->dev, "is62w esqi error: %d\n", ret);

	mutex_unlock(&is62w->lock);
	return ret;
}

static int is62w_spi_rdmr(struct is62w_spi_dev *is62w, u8 *reg)
{
	struct spi_message m;
	struct spi_transfer t;
	int ret;
	u8 cmd_addr = IS62W_RDMR;
	u8 buf[2] = {0};

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = &cmd_addr;
	t.rx_buf = buf;
	t.len = 3;
	spi_message_add_tail(&t, &m);

	mutex_lock(&is62w->lock);

	ret = spi_sync(is62w->spi, &m);
	/* have to wait at least Tcsl ns */
	ndelay(250);
	if (ret)
		dev_err(&is62w->spi->dev, "is62w rdmr error: %d\n", ret);

	*reg = buf[1];
	mutex_unlock(&is62w->lock);
	return ret;
}

static int is62w_spi_wrmr(struct is62w_spi_dev *is62w, enum is62w_op_mode opmode)
{
	struct spi_message m;
	struct spi_transfer t;
	u8 reg;
	int ret;
	u8 cmd_addr[2] = {0};

	if (is62w->opmode == opmode) {
		pr_info("is62w has been this mode\n");
		return 0;
	}

	reg = opmode << 6;
	cmd_addr[0] = IS62W_WRMR;
	cmd_addr[1] = (reg << 8);

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = cmd_addr;
	t.len = 2;
	spi_message_add_tail(&t, &m);

	mutex_lock(&is62w->lock);

	ret = spi_sync(is62w->spi, &m);
	/* have to wait at least Tcsl ns */
	ndelay(250);
	if (ret)
		dev_err(&is62w->spi->dev, "is62w wrmr error: %d\n", ret);

	is62w->opmode = opmode;
	mutex_unlock(&is62w->lock);
	return ret;
}

static int is62w_spi_write_4B(struct is62w_spi_dev *is62w, u32 off,
				   void *val, u32 len)
{
	struct spi_message m;
	struct spi_transfer t;
	char *buf = val;
	int ret = 0;
	u32 i;
	u8 *w_buf = is62w->rw_buf;

	if (len > IS62W_WR_LEN)
		len = IS62W_WR_LEN;

	memset(w_buf, 0, sizeof(u8) * IS62W_MSG_LEN);
	w_buf[0] = IS62W_WRITE;
	w_buf[1] = (off >> 16) & 0xff;
	w_buf[2] = (off >> 8) & 0xff;
	w_buf[3] = off & 0xff;
	for (i = 0; i < len; i++) {
		w_buf[i + IS62W_WR_OFF] = *(u8 *)(buf + i);
	}

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = w_buf;
	t.len = IS62W_WR_OFF + len;
	spi_message_add_tail(&t, &m);

	ret = spi_sync(is62w->spi, &m);
	if (ret) {
		dev_err(&is62w->spi->dev, "write 4B failed at %d: %d\n",
				off, ret);
	}

	/* have to wait program cycle time Twc ms */
	mdelay(6);
	return ret;
}

int is62w_spi_write(struct is62w_spi_dev *is62w, u32 off,
				   void *val, u32 len)
{
	char *buf = val;
	int ret = 0;
	u32 sz, die_num;

	if ((off + len) >= is62w->capacity) {
		pr_err("off/len is invalid\n");
		return -1;
	}
	if (is62w->opmode != OP_SEQUENTIAL) {
		pr_err("the current opmode is not OP_SEQUENTIAL\n");
		return -1;
	}

	mutex_lock(&is62w->lock);
	while (len) {
		if (len > IS62W_WR_LEN)
			sz = IS62W_WR_LEN;
		else
			sz = len;

		die_num = off / is62w->die_sz;
		if ((off + sz) > (is62w->die_sz * (die_num + 1)))
			sz = is62w->die_sz * (die_num + 1) - off;

		ret = is62w_spi_write_4B(is62w, off, buf, sz);
		if (ret)
			goto out;

		len -= sz;
		off += sz;
		buf += sz;
	}

out:
	if (ret)
		pr_err("write data fail\n");
	mutex_unlock(&is62w->lock);
	return ret;
}

static int is62w_spi_read_3B(struct is62w_spi_dev *is62w, u32 off,
				   void *val, u32 len)
{
	struct spi_message m;
	struct spi_transfer t;
	char *buf = val;
	int ret = 0;
	u32 i;
	u8 cmd_addr[4] = {0};
	u8 *r_buf = is62w->rw_buf;

	if (len > IS62W_RD_LEN)
		len = IS62W_RD_LEN;

	cmd_addr[0] = IS62W_READ;
	cmd_addr[1] = (off >> 16) & 0xff;
	cmd_addr[2] = (off >> 8) & 0xff;
	cmd_addr[3] = off & 0xff;

	memset(r_buf, 0, IS62W_MSG_LEN);
	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = cmd_addr;
	t.rx_buf = r_buf;
	t.len = IS62W_RD_OFF + len;
	spi_message_add_tail(&t, &m);

	ret = spi_sync(is62w->spi, &m);
	if (ret) {
		dev_err(&is62w->spi->dev, "read 3B failed at %d: %d\n",
				off, ret);
		return ret;
	}

	/* have to wait program cycle time Twc ms */
	mdelay(6);
	for(i = 0; i < len; i++) {
		buf[i] = r_buf[i + IS62W_RD_OFF];
	}
	return 0;
}
int is62w_spi_read(struct is62w_spi_dev *is62w, u32 off,
				   void *val, u32 len)
{
	char *buf = val;
	int ret = 0;
	u32 sz, die_num;

	if ((off + len) >= is62w->capacity) {
		pr_err("off/len is invalid\n");
		return -1;
	}
	if (is62w->opmode != OP_SEQUENTIAL) {
		pr_err("the current opmode is not OP_SEQUENTIAL\n");
		return -1;
	}

	mutex_lock(&is62w->lock);
	while (len) {
		if (len > IS62W_RD_LEN)
			sz = IS62W_RD_LEN;
		else
			sz = len;

		die_num = off / is62w->die_sz;
		if ((off + sz) > (is62w->die_sz * (die_num + 1)))
			sz = is62w->die_sz * (die_num + 1) - off;

		ret = is62w_spi_read_3B(is62w, off, buf, sz);
		if (ret)
			goto out;

		len -= sz;
		off += sz;
		buf += sz;
	}

out:
	if (ret)
		pr_err("read data fail\n");
	mutex_unlock(&is62w->lock);
	return ret;
}

static int sdrv_spi_parse_dt(struct is62w_spi_dev *is62w)
{
	struct spi_device *spi = is62w->spi;
	struct device *dev = &spi->dev;
	struct device_node *np = dev->of_node;

	of_property_read_u32(np, "spi-max-frequency", &(spi->max_speed_hz));
	of_property_read_u32(np, "page_size", &(is62w->page_sz));
	of_property_read_u32(np, "page_num", &(is62w->page_num));
	of_property_read_u32(np, "die_num", &(is62w->die_num));

	return 0;
}

static int is62w_probe(struct spi_device *spi)
{
	int err = 0;
	struct is62w_spi_dev *is62w;
	u8 w_buf[32] = {0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                    0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                    0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
                    0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a};
	u8 r_buf[32] = {0};
	u8 reg = 0;
	int i = 0;

	is62w = kzalloc(sizeof(struct is62w_spi_dev), GFP_KERNEL);
	if (!is62w)
		return -ENOMEM;

	mutex_init(&is62w->lock);
	is62w->spi = spi;

	if (is62w->spi->dev.of_node) {
		err = sdrv_spi_parse_dt(is62w);
		if(err < 0)
			return err;
	}

	is62w->capacity = is62w->page_sz * is62w->page_num;
	is62w->die_sz = is62w->capacity / is62w->die_num;
	is62w->opmode = OP_SEQUENTIAL;

	is62w->rw_buf = kzalloc(IS62W_MSG_LEN * sizeof(u8), GFP_KERNEL);
	if (!is62w->rw_buf) {
		err = -ENOMEM;
		goto fail;
	}

	err = is62w_spi_reset(is62w);
	if (err)
		goto fail_all;

	err = is62w_spi_wrmr(is62w, OP_SEQUENTIAL);
	if (err)
		goto fail_all;

	err = is62w_spi_rdmr(is62w, &reg);
	if (err)
		goto fail_all;

	err = is62w_spi_write(is62w, 0x104, w_buf, 32);
	if (err)
		goto fail_all;

	err = is62w_spi_read(is62w, 0x104, r_buf, 32);
	if (err)
		goto fail_all;

	for (i = 0; i < 32; i++) {
		if (w_buf[i] == r_buf[i]) {
			if (i == 31)
				pr_info("r/w data are the same, test ok!\n");
		}
	}

	spi_set_drvdata(spi, is62w);

	pr_info("is62w_probe ok!\n");
	return 0;
fail_all:
	if (is62w->rw_buf)
		kfree(is62w->rw_buf);
fail:
	if (is62w)
		kfree(is62w);
	pr_err("is62w probe fail!\n");
	return err;
}

static int is62w_remove(struct spi_device *spi)
{
	struct is62w_spi_dev *is62w = spi_get_drvdata(spi);

	if (is62w->rw_buf)
		kfree(is62w->rw_buf);
	if (is62w)
		kfree(is62w);
	return 0;
}

static const struct of_device_id sdrv_spi_is62w_dt_match[] = {
	{ .compatible = "semidrive,is62w-spi-0", },
	{ },
};
MODULE_DEVICE_TABLE(of, sdrv_spi_is62w_dt_match);

static struct spi_driver is62w_driver = {
    .driver = {
        .name           = "is62w-spi-0",
        .bus            = &spi_bus_type,
        .owner          = THIS_MODULE,
        .of_match_table = of_match_ptr(sdrv_spi_is62w_dt_match),
    },
    .probe          = is62w_probe,
    .remove         = is62w_remove,
};

module_spi_driver(is62w_driver);

MODULE_AUTHOR("IS62W Ltd");
MODULE_DESCRIPTION("Driver for is62w IRAM");
MODULE_LICENSE("GPL");
