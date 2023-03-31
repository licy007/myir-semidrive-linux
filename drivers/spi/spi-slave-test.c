/*
 * SPI slave-test driver
 *
 * This SPI slave-test handler sends the time of reception of the last SPI message
 * as two 32-bit unsigned integers in binary format and in network byte order,
 * representing the number of seconds and fractional seconds (in microseconds)
 * since boot up.
 *
 * Copyright (C) 2016-2017 Glider bvba
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Usage (assuming /dev/spidev2.0 corresponds to the SPI master on the remote
 * system):
 *
 *   # spidev_test -D /dev/spidev2.0 -p dummy-8B -v
 *   spi mode: 0x0
 *   bits per word: 8
 *   max speed: 500000 Hz (500 KHz)
 *   RX | 00 00 04 6D 00 09 5B BB ...
 *		^^^^^    ^^^^^^^^
 *		seconds  microseconds
 *
 *   # reboot='\x7c\x50'
 *   # poweroff='\x71\x3f'
 *   # halt='\x38\x76'
 *   # suspend='\x1b\x1b'
 *   # spidev_test -D /dev/spidev2.0 -p $suspend # or $reboot, $poweroff, $halt
 */

#include <linux/completion.h>
#include <linux/module.h>
#include <linux/sched/clock.h>
#include <linux/spi/spi.h>
#include <linux/reboot.h>
#include <linux/suspend.h>

/*
 * The numbers are chosen to display something human-readable on two 7-segment
 * displays connected to two 74HC595 shift registers
 */
#define CMD_REBOOT	    0x7c50	/* rb */
#define CMD_POWEROFF	0x713f	/* OF */
#define CMD_HALT	    0x3876	/* HL */
#define CMD_SUSPEND	    0x1b1b	/* ZZ */

struct spi_slave_test_priv {
	struct spi_device *spi;
	struct completion finished;
	struct spi_transfer xfer[2];
	struct spi_message msg[2];
	__be32 buf[2];
	__be16 cmd;
};

extern void msleep(unsigned int msecs);

static int spi_slave_test_submit(struct spi_slave_test_priv *priv);
static int spi_slave_test_read_submit(struct spi_slave_test_priv *priv);
static int spi_slave_test_write_submit(struct spi_slave_test_priv *priv);

static void spi_slave_test_read_complete(void *arg)
{
	struct spi_slave_test_priv *priv = arg;
	int ret;
	u16 cmd;

	cmd = be16_to_cpu(priv->cmd);
	switch (cmd) {
	case CMD_REBOOT:
		dev_err(&priv->spi->dev, "Rebooting system...\n");
		kernel_restart(NULL);

	case CMD_POWEROFF:
		dev_err(&priv->spi->dev, "Powering off system...\n");
		kernel_power_off();
		break;

	case CMD_HALT:
		dev_err(&priv->spi->dev, "Halting system...\n");
		kernel_halt();
		break;

	case CMD_SUSPEND:
		dev_err(&priv->spi->dev, "Suspending system...\n");
		pm_suspend(PM_SUSPEND_MEM);
		break;

	default:
		dev_warn(&priv->spi->dev, "Unknown command 0x%x\n", cmd);
		break;
	}

	msleep(50);
	ret = spi_slave_test_read_submit(priv);
	if (ret)
		goto terminate;

	return;

terminate:
	dev_info(&priv->spi->dev, "Terminating\n");
	complete(&priv->finished);
}

static void spi_slave_test_write_complete(void *arg)
{
	struct spi_slave_test_priv *priv = arg;
	int ret;

	msleep(50);
	ret = spi_slave_test_write_submit(priv);
	if (ret)
		goto terminate;

	return;

terminate:
	dev_info(&priv->spi->dev, "Terminating\n");
	complete(&priv->finished);
}

static int spi_slave_test_read_submit(struct spi_slave_test_priv *priv)
{
	int ret;

 	spi_message_init_with_transfers(&priv->msg[1], &priv->xfer[1], 1);

	priv->msg[1].complete = spi_slave_test_read_complete;
	priv->msg[1].context = priv;

	ret = spi_async(priv->spi, &priv->msg[1]);
	if (ret)
		dev_err(&priv->spi->dev, "spi_async() failed %d\n", ret);

	return ret;
}

static int spi_slave_test_write_submit(struct spi_slave_test_priv *priv)
{
	u32 rem_us;
	int ret;
	u64 ts;

	ts = local_clock();
	rem_us = do_div(ts, 1000000000) / 1000;

	priv->buf[0] = cpu_to_be32(ts);
	priv->buf[1] = cpu_to_be32(rem_us);

	spi_message_init_with_transfers(&priv->msg[0], &priv->xfer[0], 1);

	priv->msg[0].complete = spi_slave_test_write_complete;
	priv->msg[0].context = priv;

	ret = spi_async(priv->spi, &priv->msg[0]);
	if (ret)
		dev_err(&priv->spi->dev, "spi_async() failed %d\n", ret);

	return ret;
}

static int spi_slave_test_submit(struct spi_slave_test_priv *priv)
{
	spi_slave_test_read_submit(priv);
	spi_slave_test_write_submit(priv);

	return 0;
}

static int spi_slave_test_probe(struct spi_device *spi)
{
	struct spi_slave_test_priv *priv;
	int ret;

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->spi = spi;
	init_completion(&priv->finished);
	priv->xfer[0].tx_buf = priv->buf;
	priv->xfer[0].len = sizeof(priv->buf);

	priv->xfer[1].rx_buf = &priv->cmd;
	priv->xfer[1].len = sizeof(priv->cmd);

	ret = spi_slave_test_submit(priv);
	if (ret)
		return ret;

	spi_set_drvdata(spi, priv);
	return 0;
}

static int spi_slave_test_remove(struct spi_device *spi)
{
	struct spi_slave_test_priv *priv = spi_get_drvdata(spi);

	spi_slave_abort(spi);
	wait_for_completion(&priv->finished);
	return 0;
}

static struct spi_driver spi_slave_test_driver = {
	.driver = {
		.name	= "spi-slave-test",
	},
	.probe		= spi_slave_test_probe,
	.remove		= spi_slave_test_remove,
};
module_spi_driver(spi_slave_test_driver);

MODULE_AUTHOR("Geert Uytterhoeven <geert+renesas@glider.be>");
MODULE_DESCRIPTION("SPI slave read/write test");
MODULE_LICENSE("GPL v2");
