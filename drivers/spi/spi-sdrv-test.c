/*
 * Simple SPI devices test driver
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#define SPI_BUFLEN 256

static ssize_t spi_test_w_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int num;
	unsigned int cnt;
	int err;
	struct spi_device *spi = to_spi_device(dev);

	uint8_t tx[SPI_BUFLEN] = {0};
	for (cnt = 0; cnt < SPI_BUFLEN; cnt++)
		tx[cnt] = cnt + 1;

	err = kstrtouint(buf, 10, &num);
	if (err)
		return err;

	if (num > SPI_BUFLEN) {
		num = SPI_BUFLEN;
		printk(KERN_ERR "max len %d\n", SPI_BUFLEN);
	}

	spi_write(spi, tx, num);

	return size;
}

static ssize_t spi_test_r_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int num;
	unsigned int cnt;
	int err;
	struct spi_device *spi = to_spi_device(dev);
	uint8_t rx[SPI_BUFLEN] = {0};
	for (cnt = 0; cnt < SPI_BUFLEN; cnt++)
		rx[cnt] = 0x55;

	err = kstrtouint(buf, 10, &num);
	if (err)
		return err;

	if (num > SPI_BUFLEN) {
		num = SPI_BUFLEN;
		printk(KERN_ERR "max len %d\n", SPI_BUFLEN);
	}

	spi_read(spi, rx, num);

	for(cnt = 0; cnt < num; cnt++)
		printk(KERN_ERR "rx[%u] = %#x\n", cnt, rx[cnt]);

	return size;
}

static ssize_t spi_test_rw_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int num;
	unsigned int cnt;
	int err;
	struct spi_device *spi = to_spi_device(dev);
	uint8_t rx[SPI_BUFLEN] = {0x0};
	uint8_t tx[SPI_BUFLEN] = {0x0};
	for (cnt = 0; cnt < SPI_BUFLEN; cnt++) {
		rx[cnt] = 0x55;
		tx[cnt] = cnt + 1;
	}

	err = kstrtouint(buf, 10, &num);
	if (err)
		return err;

	if (num > SPI_BUFLEN) {
		num = SPI_BUFLEN;
		printk(KERN_ERR "max len %d\n", SPI_BUFLEN);
	}

	struct spi_transfer t = {
		.rx_buf = rx,
		.tx_buf	= tx,
		.len = num,
	};

	spi_sync_transfer(spi, &t, 1);

	for(cnt = 0; cnt < num; cnt++)
		printk(KERN_ERR "cntttt=%d, tx=%#x, rx=%#x\n", cnt, tx[cnt], rx[cnt]);

	return size;
}

static ssize_t spi_test_wr_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int num;
	unsigned int cnt;
	int err;
	struct spi_device *spi = to_spi_device(dev);
	uint8_t rx[SPI_BUFLEN] = {0x0};
	uint8_t tx[SPI_BUFLEN] = {0x0};
	for (cnt = 0; cnt < SPI_BUFLEN; cnt++) {
		rx[cnt] = 0x55;
		tx[cnt] = cnt + 1;
	}

	err = kstrtouint(buf, 10, &num);
	if (err)
		return err;

	if (num > SPI_BUFLEN) {
		num = SPI_BUFLEN;
		printk(KERN_ERR "max len %d\n", SPI_BUFLEN);
	}

	spi_write_then_read(spi, tx, num, rx, num);

	for(cnt = 0; cnt < num; cnt++)
		printk(KERN_ERR "cnt=%d, tx=%#x, rx=%#x\n", cnt, tx[cnt], rx[cnt]);

	return size;
}

static ssize_t spi_test_flash_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int num;
	unsigned int cnt;
	int err;
	struct spi_device *spi = to_spi_device(dev);
	uint8_t rx[SPI_BUFLEN] = {0x0};
	for (cnt = 0; cnt < SPI_BUFLEN; cnt++)
		rx[cnt] = 0x55;

	err = kstrtouint(buf, 10, &num);
	if (err)
		return err;

	if (num > SPI_BUFLEN) {
		num = SPI_BUFLEN;
		printk(KERN_ERR "max len %d\n", SPI_BUFLEN);
	}

	if (spi_flash_read_supported(spi)) {
		struct spi_flash_read_message msg = {
			.buf = rx,
			.len = num,
			.from = 0x5678,
			.read_opcode = 1,
			.addr_width = 2,
			.dummy_bytes = 0,
		};

		err = spi_flash_read(spi, &msg);
		if (err < 0)
			printk(KERN_ERR "test spi flash read fail %d\n", err);
		else {
			for(cnt = 0; cnt < num; cnt++)
				printk(KERN_ERR "cnt=%d, rx=%#x\n", cnt, rx[cnt]);
		}
	}

	return size;
}

static DEVICE_ATTR(spi_test_w, 0664, NULL, spi_test_w_store);
static DEVICE_ATTR(spi_test_r, 0664, NULL, spi_test_r_store);
static DEVICE_ATTR(spi_test_rw, 0664, NULL, spi_test_rw_store);
static DEVICE_ATTR(spi_test_wr, 0664, NULL, spi_test_wr_store);
static DEVICE_ATTR(spi_test_flash, 0664, NULL, spi_test_flash_store);

static int spi_test_probe(struct spi_device *spi)
{
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi_setup(spi);

	device_create_file(&spi->dev, &dev_attr_spi_test_w);
	device_create_file(&spi->dev, &dev_attr_spi_test_r);
	device_create_file(&spi->dev, &dev_attr_spi_test_rw);
	device_create_file(&spi->dev, &dev_attr_spi_test_wr);
	device_create_file(&spi->dev, &dev_attr_spi_test_flash);
	return 0;
}

static int spi_test_remove(struct spi_device *spi)
{
	device_remove_file(&spi->dev, &dev_attr_spi_test_w);
	device_remove_file(&spi->dev, &dev_attr_spi_test_r);
	device_remove_file(&spi->dev, &dev_attr_spi_test_rw);
	device_remove_file(&spi->dev, &dev_attr_spi_test_wr);
	device_remove_file(&spi->dev, &dev_attr_spi_test_flash);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spi_test_dt_ids[] = {
	{ .compatible = "sd,spi_test" },
	{},
};
MODULE_DEVICE_TABLE(of, spi_test_dt_ids);
#endif

static struct spi_driver spi_test_driver = {
	.driver = {
		.name = "spi_test",
		.of_match_table = of_match_ptr(spi_test_dt_ids),
	},
	.probe = spi_test_probe,
	.remove = spi_test_remove,
};

module_spi_driver(spi_test_driver);

