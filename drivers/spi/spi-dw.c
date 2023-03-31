/*
 * Designware SPI core controller driver (refer pxa2xx_spi.c)
 *
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>

#include <soc/semidrive/sdrv_scr.h>
#include "spi-dw.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

/* Slave spi_dev related */
struct chip_data {
	u8 cs;			/* chip select pin */
	u8 tmode;		/* TR/TO/RO/EEPROM */
	u8 type;		/* SPI/SSP/MicroWire */

	u8 poll_mode;		/* 1 means use poll mode */

	u16 clk_div;		/* baud rate divider */
	u32 speed_hz;		/* baud rate */
	void (*cs_control)(u32 command);
};

#ifdef CONFIG_DEBUG_FS
#define SPI_REGS_BUFSIZE	1024
static ssize_t dw_spi_show_regs(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct dw_spi *dws = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(SPI_REGS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"%s registers:\n", dev_name(&dws->master->dev));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"CTRL0: \t\t0x%08x\n", dw_readl(dws, DW_SPI_CTRL0));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"CTRL1: \t\t0x%08x\n", dw_readl(dws, DW_SPI_CTRL1));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"SSIENR: \t0x%08x\n", dw_readl(dws, DW_SPI_SSIENR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"SER: \t\t0x%08x\n", dw_readl(dws, DW_SPI_SER));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"BAUDR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_BAUDR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"TXFTLR: \t0x%08x\n", dw_readl(dws, DW_SPI_TXFLTR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"RXFTLR: \t0x%08x\n", dw_readl(dws, DW_SPI_RXFLTR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"TXFLR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_TXFLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"RXFLR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_RXFLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"SR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_SR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"IMR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_IMR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"ISR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_ISR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"DMACR: \t\t0x%08x\n", dw_readl(dws, DW_SPI_DMACR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"DMATDLR: \t0x%08x\n", dw_readl(dws, DW_SPI_DMATDLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"DMARDLR: \t0x%08x\n", dw_readl(dws, DW_SPI_DMARDLR));
	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
			"=================================\n");

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations dw_spi_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dw_spi_show_regs,
	.llseek		= default_llseek,
};

static ssize_t dw_spi_loop_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[8];
	struct dw_spi *dws = filp->private_data;
	size_t size = sprintf(buf, "%u\n", dws->loop_enable);
	return simple_read_from_buffer(buffer, count, ppos, buf, size);
}

static ssize_t dw_spi_loop_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	int ret = 0;
	struct dw_spi *dws = filp->private_data;
	unsigned int flag;

	ret = kstrtouint_from_user(buffer, count, 0, &flag);
	if (ret)
		return -EFAULT;

	dws->loop_enable = !!flag;
	printk(KERN_ERR "spi%u loop=%u\n", dws->bus_num, dws->loop_enable);
	return count;
}

static const struct file_operations dw_spi_loop_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = dw_spi_loop_read,
	.write = dw_spi_loop_write,
};

static int dw_spi_debugfs_init(struct dw_spi *dws)
{
	char name[32];

	snprintf(name, 32, "dw_spi%d", dws->master->bus_num);
	dws->debugfs = debugfs_create_dir(name, NULL);
	if (!dws->debugfs)
		return -ENOMEM;

	debugfs_create_file("registers", S_IFREG | S_IRUGO,
		dws->debugfs, (void *)dws, &dw_spi_regs_ops);

	if (!dws->master->slave)
		debugfs_create_file("loop", S_IWUGO | S_IRUGO,
			dws->debugfs, (void *)dws, &dw_spi_loop_ops);

	return 0;
}

static void dw_spi_debugfs_remove(struct dw_spi *dws)
{
	debugfs_remove_recursive(dws->debugfs);
}

#else
static inline int dw_spi_debugfs_init(struct dw_spi *dws)
{
	return 0;
}

static inline void dw_spi_debugfs_remove(struct dw_spi *dws)
{
}
#endif /* CONFIG_DEBUG_FS */

static void dw_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct dw_spi *dws = spi_master_get_devdata(spi->master);
	struct chip_data *chip = spi_get_ctldata(spi);

	/* Chip select logic is inverted from spi_set_cs() */
	if (chip && chip->cs_control)
		chip->cs_control(!enable);

	if (!enable)
		dw_writel(dws, DW_SPI_SER, BIT(spi->chip_select));
}

/* Return the max entries we can fill into tx fifo */
static inline u32 tx_max(struct dw_spi *dws)
{
	u32 tx_left, tx_room, rxtx_gap;

	tx_left = (dws->tx_end - dws->tx) / dws->n_bytes;
	tx_room = dws->fifo_len - dw_readl(dws, DW_SPI_TXFLR);

	/*
	 * Another concern is about the tx/rx mismatch, we
	 * though to use (dws->fifo_len - rxflr - txflr) as
	 * one maximum value for tx, but it doesn't cover the
	 * data which is out of tx/rx fifo and inside the
	 * shift registers. So a control from sw point of
	 * view is taken.
	 */
	rxtx_gap =  ((dws->rx_end - dws->rx) - (dws->tx_end - dws->tx))
			/ dws->n_bytes;

	if (dws->tmode == SPI_TMOD_EPROMREAD)
		return min(tx_left, tx_room);
	else
		return min3(tx_left, tx_room, (u32) (dws->fifo_len - rxtx_gap));
}

/* Return the max entries we should read out of rx fifo */
static inline u32 rx_max(struct dw_spi *dws)
{
	u32 rx_left = (dws->rx_end - dws->rx) / dws->n_bytes;

	return min_t(u32, rx_left, dw_readl(dws, DW_SPI_RXFLR));
}

static void dw_writer(struct dw_spi *dws)
{
	u32 max = tx_max(dws);
	u32 txw = 0;

	while (max--) {
		/* Set the tx word if the transfer's original "tx" is not null */
		if (dws->tx_end - dws->len) {
			if (dws->n_bytes == 1)
				txw = *(u8 *)(dws->tx);
			else if (dws->n_bytes == 2)
				txw = *(u16 *)(dws->tx);
			else
				txw = *(u32 *)(dws->tx);
		}
		dw_write_io_reg(dws, DW_SPI_DR, txw);
		dws->tx += dws->n_bytes;
	}
}

static void dw_reader(struct dw_spi *dws)
{
	u32 max = rx_max(dws);
	u32 rxw;

	while (max--) {
		rxw = dw_read_io_reg(dws, DW_SPI_DR);
		/* Care rx only if the transfer's original "rx" is not null */
		if (dws->rx_end - dws->len) {
			if (dws->n_bytes == 1)
				*(u8 *)(dws->rx) = rxw;
			else if (dws->n_bytes == 2)
				*(u16 *)(dws->rx) = rxw;
			else
				*(u32 *)(dws->rx) = rxw;
		}
		dws->rx += dws->n_bytes;
	}
}

static void int_error_stop(struct dw_spi *dws, const char *msg)
{
	spi_reset_chip(dws);

	dev_err(&dws->master->dev, "%s\n", msg);
	if (dws->tmode == SPI_TMOD_EPROMREAD)
		dws->eep_stat = -EIO;
	else
		dws->master->cur_msg->status = -EIO;
	spi_finalize_current_transfer(dws->master);
}

static irqreturn_t interrupt_transfer(struct dw_spi *dws)
{
	u16 irq_status = dw_readl(dws, DW_SPI_ISR);
	unsigned int rx_len;

	/* Error handling */
	if (irq_status & (SPI_INT_TXOI | SPI_INT_RXOI | SPI_INT_RXUI)) {
		dw_readl(dws, DW_SPI_ICR);
		int_error_stop(dws, "interrupt_transfer: fifo overrun/underrun");
		return IRQ_HANDLED;
	}

	if (spi_controller_is_slave(dws->master)) {
		if (dws->slave_aborted) {
			int_error_stop(dws, "slave aborted");
			return IRQ_HANDLED;
		}
	}

	dw_reader(dws);
	rx_len = (dws->rx_end - dws->rx) / dws->n_bytes;
	if (!rx_len) {
		spi_mask_intr(dws, 0xff);
		spi_finalize_current_transfer(dws->master);
	} else if (rx_len <= dw_readl(dws, DW_SPI_RXFLTR)) {
		dw_writel(dws, DW_SPI_RXFLTR, rx_len - 1);
	}

	if (dws->tmode != SPI_TMOD_EPROMREAD) {
		if (irq_status & SPI_INT_TXEI) {
			dw_writer(dws);
			if (dws->tx_end == dws->tx)
				spi_mask_intr(dws, SPI_INT_TXEI);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t dw_spi_irq(int irq, void *dev_id)
{
	struct spi_master *master = dev_id;
	struct dw_spi *dws = spi_master_get_devdata(master);
	u16 irq_status = dw_readl(dws, DW_SPI_ISR) & 0x3f;

	if (!irq_status)
		return IRQ_NONE;

	if ((dws->tmode != SPI_TMOD_EPROMREAD) && !master->cur_msg) {
		spi_mask_intr(dws, 0xff);
		return IRQ_HANDLED;
	}

	return dws->transfer_handler(dws);
}

/* Must be called inside pump_transfers() */
static int poll_transfer(struct dw_spi *dws)
{
	do {
		dw_writer(dws);
		dw_reader(dws);
		cpu_relax();
	} while (dws->rx_end > dws->rx);

	return 0;
}

static inline void wait_for_idle(struct dw_spi *dws)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(5);

	do {
		if (!(readl_relaxed(dws->regs + DW_SPI_SR) & SR_BUSY))
			return;
	} while (!time_after(jiffies, timeout));

	pr_info("spi controller is in busy state!\n");
}

static void dw_spi_dma_rxcb(void *data)
{
	unsigned long flags;
	struct dw_spi *dws = data;

	spin_lock_irqsave(&dws->lock, flags);

	dws->state &= ~RXBUSY;
	if (!(dws->state & TXBUSY)) {
		spi_enable_chip(dws, 0);
		spi_finalize_current_transfer(dws->master);
	}

	spin_unlock_irqrestore(&dws->lock, flags);
}

static void dw_spi_dma_txcb(void *data)
{
	unsigned long flags;
	struct dw_spi *dws = data;

	/* Wait until the FIFO data completely. */
	wait_for_idle(dws);

	spin_lock_irqsave(&dws->lock, flags);

	dws->state &= ~TXBUSY;
	if (!(dws->state & RXBUSY)) {
		spi_enable_chip(dws, 0);
		spi_finalize_current_transfer(dws->master);
	}

	spin_unlock_irqrestore(&dws->lock, flags);
}

static int dw_spi_prepare_dma(struct dw_spi *dws)
{
	unsigned long flags;
	struct dma_slave_config rxconf, txconf;
	struct dma_async_tx_descriptor *rxdesc, *txdesc;

	spin_lock_irqsave(&dws->lock, flags);
	dws->state &= ~RXBUSY;
	dws->state &= ~TXBUSY;
	spin_unlock_irqrestore(&dws->lock, flags);

	rxdesc = NULL;
	if (dws->rx) {
		rxconf.direction = dws->dma_rx.direction;
		rxconf.src_addr = dws->dma_rx.addr;
		rxconf.src_addr_width = dws->n_bytes;
		if (dws->dma_caps.max_burst > 4)
			rxconf.src_maxburst = 4;
		else
			rxconf.src_maxburst = 1;
		dmaengine_slave_config(dws->dma_rx.ch, &rxconf);

		rxdesc = dmaengine_prep_slave_sg(
				dws->dma_rx.ch,
				dws->rx_sg.sgl, dws->rx_sg.nents,
				dws->dma_rx.direction, DMA_PREP_INTERRUPT);
		if (!rxdesc)
			return -EINVAL;

		rxdesc->callback = dw_spi_dma_rxcb;
		rxdesc->callback_param = dws;
	}

	txdesc = NULL;
	if (dws->tx && (dws->tmode != SPI_TMOD_EPROMREAD)) {
		txconf.direction = dws->dma_tx.direction;
		txconf.dst_addr = dws->dma_tx.addr;
		txconf.dst_addr_width = dws->n_bytes;
		if (dws->dma_caps.max_burst > 4)
			txconf.dst_maxburst = 4;
		else
			txconf.dst_maxburst = 1;
		dmaengine_slave_config(dws->dma_tx.ch, &txconf);

		txdesc = dmaengine_prep_slave_sg(
				dws->dma_tx.ch,
				dws->tx_sg.sgl, dws->tx_sg.nents,
				dws->dma_tx.direction, DMA_PREP_INTERRUPT);
		if (!txdesc) {
			if (rxdesc)
				dmaengine_terminate_sync(dws->dma_rx.ch);
			return -EINVAL;
		}

		txdesc->callback = dw_spi_dma_txcb;
		txdesc->callback_param = dws;
	}


	/* rx must be started before tx due to spi instinct */
	if (rxdesc) {
		spin_lock_irqsave(&dws->lock, flags);
		dws->state |= RXBUSY;
		spin_unlock_irqrestore(&dws->lock, flags);
		dmaengine_submit(rxdesc);
		dma_async_issue_pending(dws->dma_rx.ch);
	}

	if (txdesc && (dws->tmode != SPI_TMOD_EPROMREAD)) {
		spin_lock_irqsave(&dws->lock, flags);
		dws->state |= TXBUSY;
		spin_unlock_irqrestore(&dws->lock, flags);
		dmaengine_submit(txdesc);
		dma_async_issue_pending(dws->dma_tx.ch);
	}

	return 0;
}

static void dw_flash_set_cs(struct spi_device *spi, bool enable)
{
	if (spi->mode & SPI_CS_HIGH)
		enable = !enable;

	if (gpio_is_valid(spi->cs_gpio)) {
		gpio_set_value(spi->cs_gpio, !enable);
		/* Some SPI masters need both GPIO CS & slave_select */
		if ((spi->controller->flags & SPI_MASTER_GPIO_SS) &&
		    spi->controller->set_cs)
			spi->controller->set_cs(spi, !enable);
	} else if (spi->controller->set_cs) {
		spi->controller->set_cs(spi, !enable);
	}
}

static int dw_flash_read(struct spi_device *spi, struct spi_flash_read_message *msg)
{
	struct dw_spi *dws = spi_master_get_devdata(spi->master);
	struct chip_data *chip = spi_get_ctldata(spi);
	u8 cmd[5] = {0};
	int ret = 0;
	u32 cr0 = 0;
	u16 rxlevel;
	u16 cmd_len;
	unsigned long long ms = 0;
	unsigned long flags;

	cmd[0] = msg->read_opcode;
	cmd[1] = msg->from >> (msg->addr_width * 8 -  8);
	cmd[2] = msg->from >> (msg->addr_width * 8 - 16);
	cmd[3] = msg->from >> (msg->addr_width * 8 - 24);
	cmd[4] = msg->from >> (msg->addr_width * 8 - 32);
	cmd_len = msg->addr_width + msg->dummy_bytes + 1;

	dws->tx = (void *)cmd;
	dws->tx_end = dws->tx + cmd_len;
	dws->rx = msg->buf;
	dws->rx_end = dws->rx + msg->len;
	dws->len = cmd_len;
	dws->eep_stat = 0;

	if (!spi->max_speed_hz)
		return -EINVAL;

	if (spi->bits_per_word == 8)
		dws->n_bytes = 1;
	else
		return -EINVAL;

	spi_enable_chip(dws, 0);
	dw_flash_set_cs(spi, false);

	chip->clk_div = (DIV_ROUND_UP(dws->max_freq, spi->max_speed_hz) + 1) & 0xfffe;
	spi_set_clk(dws, chip->clk_div);

	/* Default SPI mode is SCPOL = 0, SCPH = 0 */
	chip->tmode = SPI_TMOD_EPROMREAD;
	cr0 = (spi->bits_per_word - 1) << SPI_DFS32_OFFSET
		| (chip->type << SPI_FRF_OFFSET)
		| (spi->mode << SPI_MODE_OFFSET)
		| (chip->tmode << SPI_TMOD_OFFSET);
	if (dws->loop_enable)
		cr0 = cr0 | (1 << SPI_SRL_OFFSET);

	dws->tmode = chip->tmode;
	dw_writel(dws, DW_SPI_CTRL0, cr0);
	dw_writel(dws, DW_SPI_CTRL1, msg->len - 1);

	if (dws->enable_dma) {
		dws->rx_sg = msg->rx_sg;
		dw_writel(dws, DW_SPI_DMATDLR, 0);
		dw_writel(dws, DW_SPI_DMARDLR, 0);
		dw_writel(dws, DW_SPI_DMACR, SPI_DMA_RDMAE);
	}

	if (!chip->poll_mode) {
		rxlevel = min_t(u16, dws->fifo_len / 2, msg->len / dws->n_bytes);
		dw_writel(dws, DW_SPI_RXFLTR, rxlevel - 1);

		if (dws->enable_dma) {
			if (dw_spi_prepare_dma(dws) < 0)
				dev_err(&dws->master->dev, "err:flash prepare dma fail\n");
			spi_umask_intr(dws, SPI_INT_TXOI | SPI_INT_RXUI | SPI_INT_RXOI);
		} else
			spi_umask_intr(dws, SPI_INT_TXOI | SPI_INT_RXUI | SPI_INT_RXOI | SPI_INT_RXFI);
		dws->transfer_handler = interrupt_transfer;

		dw_flash_set_cs(spi, true);
		spi_enable_chip(dws, 1);

		spin_lock_irqsave(&dws->lock, flags);
		while (dws->tx_end > dws->tx)
			dw_writer(dws);
		dws->len = msg->len;
		spin_unlock_irqrestore(&dws->lock, flags);

		reinit_completion(&dws->master->xfer_completion);

		ms = 8LL * 1000LL * msg->len;
		do_div(ms, spi->max_speed_hz);
		ms += ms + 1000; /* some tolerance */

		if (ms > UINT_MAX)
			ms = UINT_MAX;

		ms = wait_for_completion_timeout(&dws->master->xfer_completion,
						 msecs_to_jiffies(ms));
	}

	if(dws->eep_stat < 0)
		ret = dws->eep_stat;
	else if (ms)
		msg->retlen = msg->len;
	else
		ret = -ETIMEDOUT;

	return ret;
}

static int dw_spi_transfer_one(struct spi_master *master,
		struct spi_device *spi, struct spi_transfer *transfer)
{
	struct dw_spi *dws = spi_master_get_devdata(master);
	struct chip_data *chip = spi_get_ctldata(spi);
	u8 imask = 0;
	u16 txlevel = 0;
	u32 cr0;
	u32 dmacr = 0;
	int ret;

	dws->tx = (void *)transfer->tx_buf;
	dws->tx_end = dws->tx + transfer->len;
	dws->rx = transfer->rx_buf;
	dws->rx_end = dws->rx + transfer->len;
	dws->len = transfer->len;

	dws->slave_aborted = false;

	spi_enable_chip(dws, 0);

	/* Handle per transfer options for bpw and speed */
	if (transfer->speed_hz != dws->current_freq) {
		if (transfer->speed_hz != chip->speed_hz) {
			/* clk_div doesn't support odd number */
			chip->clk_div = (DIV_ROUND_UP(dws->max_freq, transfer->speed_hz) + 1) & 0xfffe;
			chip->speed_hz = transfer->speed_hz;
		}
		dws->current_freq = transfer->speed_hz;
		spi_set_clk(dws, chip->clk_div);
	}

	if (transfer->bits_per_word == 8) {
		dws->n_bytes = 1;
	} else if (transfer->bits_per_word == 16) {
		dws->n_bytes = 2;
	} else if (transfer->bits_per_word == 32) {
		dws->n_bytes = 4;
	} else {
		return -EINVAL;
	}
	/* Default SPI mode is SCPOL = 0, SCPH = 0 */
	chip->tmode = SPI_TMOD_TR;
	cr0 = (transfer->bits_per_word - 1) << SPI_DFS32_OFFSET
		| (chip->type << SPI_FRF_OFFSET)
		| (spi->mode << SPI_MODE_OFFSET)
		| (chip->tmode << SPI_TMOD_OFFSET);
	if(dws->loop_enable)
		cr0 = cr0 | (1 << SPI_SRL_OFFSET);

	/*
	 * Adjust transfer mode if necessary. Requires platform dependent
	 * chipselect mechanism.
	 */
	if (chip->cs_control) {
		if (dws->rx && dws->tx)
			chip->tmode = SPI_TMOD_TR;
		else if (dws->rx)
			chip->tmode = SPI_TMOD_RO;
		else
			chip->tmode = SPI_TMOD_TO;

		cr0 &= ~SPI_TMOD_MASK;
		cr0 |= (chip->tmode << SPI_TMOD_OFFSET);
	}

	dws->tmode = chip->tmode;
	dw_writel(dws, DW_SPI_CTRL0, cr0);

	/* For poll mode just disable all interrupts */
	spi_mask_intr(dws, 0xff);

	if (dws->enable_dma) {
		dws->tx_sg = transfer->tx_sg;
		dws->rx_sg = transfer->rx_sg;

		if (dws->tx)
			dmacr |= SPI_DMA_TDMAE;
		if (dws->rx)
			dmacr |= SPI_DMA_RDMAE;

		dw_writel(dws, DW_SPI_DMATDLR, 0);
		dw_writel(dws, DW_SPI_DMARDLR, 0);
		dw_writel(dws, DW_SPI_DMACR, dmacr);

		if (!dws->tx) {
			dev_err(&dws->master->dev, "err:this dma need tx\n");
		} else {
			ret = dw_spi_prepare_dma(dws);
			if (ret < 0)
				dev_err(&dws->master->dev, "err:prepare dma fail\n");
			else
				spi_enable_chip(dws, 1);
		}
	} else if (!chip->poll_mode) {
		txlevel = min_t(u16, dws->fifo_len / 2, dws->len / dws->n_bytes);
		dw_writel(dws, DW_SPI_TXFLTR, txlevel);
		dw_writel(dws, DW_SPI_RXFLTR, txlevel - 1);

		/* Set the interrupt mask */
		imask |= SPI_INT_TXEI | SPI_INT_TXOI |
			 SPI_INT_RXUI | SPI_INT_RXOI | SPI_INT_RXFI;
		spi_umask_intr(dws, imask);

		dws->transfer_handler = interrupt_transfer;

		spi_enable_chip(dws, 1);
	}

	if (chip->poll_mode)
		return poll_transfer(dws);

	return 1;
}

static int dw_spi_slave_abort(struct spi_master *master)
{
	struct dw_spi *dws = spi_master_get_devdata(master);

	dws->slave_aborted = true;
	spi_finalize_current_transfer(master);

	return 0;
}

static void dw_spi_handle_err(struct spi_master *master,
		struct spi_message *msg)
{
	struct dw_spi *dws = spi_master_get_devdata(master);

	spi_reset_chip(dws);
}

/* This may be called twice for each spi dev */
static int dw_spi_setup(struct spi_device *spi)
{
	struct dw_spi_chip *chip_info = NULL;
	struct chip_data *chip;
	int ret;

	/* Only alloc on first setup */
	chip = spi_get_ctldata(spi);
	if (!chip) {
		chip = kzalloc(sizeof(struct chip_data), GFP_KERNEL);
		if (!chip)
			return -ENOMEM;
		spi_set_ctldata(spi, chip);
	}

	/*
	 * Protocol drivers may change the chip settings, so...
	 * if chip_info exists, use it
	 */
	chip_info = spi->controller_data;

	/* chip_info doesn't always exist */
	if (chip_info) {
		if (chip_info->cs_control)
			chip->cs_control = chip_info->cs_control;

		chip->poll_mode = chip_info->poll_mode;
		chip->type = chip_info->type;
	}

	chip->tmode = SPI_TMOD_TR;

	if (gpio_is_valid(spi->cs_gpio)) {
		ret = gpio_direction_output(spi->cs_gpio,
				!(spi->mode & SPI_CS_HIGH));
		if (ret)
			return ret;
	}

	return 0;
}

static void dw_spi_cleanup(struct spi_device *spi)
{
	struct chip_data *chip = spi_get_ctldata(spi);

	kfree(chip);
	spi_set_ctldata(spi, NULL);
}

/* Restart the controller, disable all interrupts, clean rx fifo */
static void spi_hw_init(struct device *dev, struct dw_spi *dws)
{
	spi_reset_chip(dws);

	/*
	 * Try to detect the FIFO depth if not set by interface driver,
	 * the depth could be from 2 to 256 from HW spec
	 */
	if (!dws->fifo_len) {
		u32 fifo;

		for (fifo = 1; fifo < 256; fifo++) {
			dw_writel(dws, DW_SPI_TXFLTR, fifo);
			if (fifo != dw_readl(dws, DW_SPI_TXFLTR))
				break;
		}
		dw_writel(dws, DW_SPI_TXFLTR, 0);

		dws->fifo_len = (fifo == 1) ? 0 : fifo;
		dev_dbg(dev, "Detected FIFO size: %u bytes\n", dws->fifo_len);
	}
}

static bool dw_spi_can_dma(struct spi_master *master,
			   struct spi_device *spi, struct spi_transfer *xfer)
{
	return true;
}

static bool dw_spi_flash_can_dma(struct spi_device *spi,
				  struct spi_flash_read_message *msg)
{
	return true;
}

int dw_spi_add_host(struct device *dev, struct dw_spi *dws)
{
	struct spi_master *master;
	int ret;
	bool slave_mode;
	struct device_node *np = dev->of_node;

	BUG_ON(dws == NULL);

	slave_mode = of_property_read_bool(np, "spi-slave");
	if (slave_mode)
		master = spi_alloc_slave(dev, 0);
	else
		master = spi_alloc_master(dev, 0);

	if (!master)
		return -ENOMEM;

	dws->master = master;
	dws->type = SSI_MOTO_SPI;

	ret = request_irq(dws->irq, dw_spi_irq, IRQF_SHARED, dev_name(dev),
			  master);
	if (ret < 0) {
		dev_err(dev, "can not get IRQ\n");
		goto err_free_master;
	}

	spin_lock_init(&dws->lock);

	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_LOOP;
	master->bits_per_word_mask = SPI_BPW_MASK(8) | SPI_BPW_MASK(16)
		| SPI_BPW_MASK(32);
	master->bus_num = dws->bus_num;

	if (slave_mode) {
		master->mode_bits |= SPI_NO_CS;
		master->slave_abort = dw_spi_slave_abort;
	} else {
		master->flags = SPI_MASTER_GPIO_SS;
		master->num_chipselect = dws->num_cs;
		master->spi_flash_read = dw_flash_read;
	}
	master->setup = dw_spi_setup;
	master->cleanup = dw_spi_cleanup;
	master->set_cs = dw_spi_set_cs;
	master->transfer_one = dw_spi_transfer_one;
	master->handle_err = dw_spi_handle_err;
	master->max_speed_hz = dws->max_freq;
	master->dev.of_node = dev->of_node;
	master->slave = slave_mode;

	/* Basic HW init */
	spi_hw_init(dev, dws);

	/* Setup SCR opmode as master or slave in according to the config */
	if (dws->scr_opmode) {
		if (slave_mode)
			sdrv_scr_set(SCR_SEC, dws->scr_opmode, 1);
		else
			sdrv_scr_set(SCR_SEC, dws->scr_opmode, 0);
	}

	if (dws->enable_dma) {
		dws->dma_tx.ch = dma_request_chan(dev, "tx");
		if (IS_ERR(dws->dma_tx.ch)) {
			dev_err(dev, "Failed to request TX DMA channel\n");
			dws->dma_tx.ch = NULL;
			goto err_free_irq;
		}

		dws->dma_rx.ch = dma_request_chan(dev, "rx");
		if (IS_ERR(dws->dma_rx.ch)) {
			dev_err(dev, "Failed to request RX DMA channel\n");
			dws->dma_rx.ch = NULL;
			goto err_free_dma_tx;
		}

		if (dws->dma_tx.ch && dws->dma_rx.ch) {
			dma_get_slave_caps(dws->dma_rx.ch, &dws->dma_caps);
			dws->dma_tx.addr = (dma_addr_t)(dws->paddr + DW_SPI_DR);
			dws->dma_rx.addr = (dma_addr_t)(dws->paddr + DW_SPI_DR);
			dws->dma_tx.direction = DMA_MEM_TO_DEV;
			dws->dma_rx.direction = DMA_DEV_TO_MEM;

			master->spi_flash_can_dma = dw_spi_flash_can_dma;
			master->can_dma = dw_spi_can_dma;
			master->dma_tx = dws->dma_tx.ch;
			master->dma_rx = dws->dma_rx.ch;
		}
	}

	spi_master_set_devdata(master, dws);
	ret = devm_spi_register_master(dev, master);
	if (ret) {
		dev_err(&master->dev, "problem registering spi master\n");
		goto err_free_dma_rx;
	}

	dw_spi_debugfs_init(dws);
	return 0;

	if (dws->enable_dma) {
err_free_dma_rx:
		dma_release_channel(dws->dma_rx.ch);
err_free_dma_tx:
		dma_release_channel(dws->dma_tx.ch);
 err_free_irq:
	free_irq(dws->irq, master);
}

err_free_master:
	spi_master_put(master);
	return ret;
}
EXPORT_SYMBOL_GPL(dw_spi_add_host);

void dw_spi_remove_host(struct dw_spi *dws)
{
	dw_spi_debugfs_remove(dws);

	if (dws->enable_dma) {
		if (dws->dma_tx.ch)
			dma_release_channel(dws->dma_tx.ch);
		if (dws->dma_rx.ch)
			dma_release_channel(dws->dma_rx.ch);
	}

	spi_shutdown_chip(dws);

	free_irq(dws->irq, dws->master);
}
EXPORT_SYMBOL_GPL(dw_spi_remove_host);

int dw_spi_suspend_host(struct dw_spi *dws)
{
	int ret;

	ret = spi_master_suspend(dws->master);
	if (ret)
		return ret;

	spi_shutdown_chip(dws);
	return 0;
}
EXPORT_SYMBOL_GPL(dw_spi_suspend_host);

int dw_spi_resume_host(struct dw_spi *dws)
{
	int ret;

	spi_hw_init(&dws->master->dev, dws);
	ret = spi_master_resume(dws->master);
	if (ret)
		dev_err(&dws->master->dev, "fail to start queue (%d)\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(dw_spi_resume_host);

MODULE_AUTHOR("Feng Tang <feng.tang@intel.com>");
MODULE_DESCRIPTION("Driver for DesignWare SPI controller core");
MODULE_LICENSE("GPL v2");
