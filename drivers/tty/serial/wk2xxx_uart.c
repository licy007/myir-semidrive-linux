/*
 *   FILE NAME  : wk2xxx_spi.c
 *
 *   WKIC Ltd.
 *   By  Xu XunWei Tech
 *   DEMO Version :2.0 Data:2018-8-08
 *
 *   DESCRIPTION: Implements an interface for the wk2xxx of spi interface
 *
 *   1. compiler warnings all changes
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/spi/spi.h>
#include <linux/timer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>


#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <linux/io.h>
#include "wk2xxx_uart.h"


//#define _DEBUG_WK2XXX
//#define _DEBUG_WK2XXX1
//#define _DEBUG_WK2XXX2
//#define _DEBUG_WK2XXX4
//#define _DEBUG_WK2XXX5

#define WK2XXX_STATUS_PE    1
#define WK2XXX_STATUS_FE    2
#define WK2XXX_STATUS_BRK   4
#define WK2XXX_STATUS_OE    8

#define MAX_RFCOUNT_SIZE    256
#define NAME_LEN            30

static unsigned int chip_num;

struct wk2xxx_port {
	struct wk2xxx_chip *chip;
	struct uart_port port;
	struct spi_device *spi_wk;
	spinlock_t conf_lock;   /* shared data */
	struct workqueue_struct *workqueue;
	struct work_struct work;
	int suspending;
	void (*wk2xxx_hw_suspend)(int suspend);
	int tx_done;

	int force_end_work;
	int irq;
	int minor;      /* minor number */
	int tx_empty;
	int tx_empty_flag;

	int start_tx_flag;
	int stop_tx_flag;
	int stop_rx_flag;
	int irq_flag;
	int conf_flag;

	int tx_empty_fail;
	int start_tx_fail;
	int stop_tx_fail;
	int stop_rx_fail;
	int irq_fail;
	int conf_fail;

	uint8_t new_lcr;
	uint8_t new_fwcr;
	uint8_t new_scr;
	/*set baud 0f register*/
	uint8_t new_baud1;
	uint8_t new_baud0;
	uint8_t new_pres;
};

struct wk2xxx_chip {
	struct wk2xxx_port wk2xxxs[NR_PORTS];
	struct uart_driver wk2xxx_uart_driver;
	int chip_num;

	struct mutex wk2xxxs_lock;
	struct mutex wk2xxxs_reg_lock;
	struct mutex wk2xxs_work_lock;
};

/*
 * This function read wk2xxx of Global register:
 */
static int wk2xxx_read_global_reg(struct spi_device *spi, uint8_t reg,
				  uint8_t *dat)
{
	struct spi_message msg;
	uint8_t buf_wdat[2];
	uint8_t buf_rdat[2];
	int status;
	struct wk2xxx_chip *chip = dev_get_drvdata(&spi->dev);
	struct spi_transfer index_xfer = {
		.len            = 2,
		//.cs_change      = 1,
		.speed_hz	= wk2xxx_spi_speed,
	};

	mutex_lock(&chip->wk2xxxs_reg_lock);
	status = 0;
	spi_message_init(&msg);
	buf_wdat[0] = 0x40|reg;
	buf_wdat[1] = 0x00;
	buf_rdat[0] = 0x00;
	buf_rdat[1] = 0x00;
	index_xfer.tx_buf = buf_wdat;
	index_xfer.rx_buf = (void *)buf_rdat;
	spi_message_add_tail(&index_xfer, &msg);
	status = spi_sync(spi, &msg);

	mutex_unlock(&chip->wk2xxxs_reg_lock);
	if (status)
		return status;
	*dat = buf_rdat[1];

	return 0;
}

/*
 * This function write wk2xxx of Global register:
 */
static int wk2xxx_write_global_reg(struct spi_device *spi,
				   uint8_t reg, uint8_t dat)
{
	struct spi_message msg;
	uint8_t buf_reg[2];
	int status;
	struct wk2xxx_chip *chip = dev_get_drvdata(&spi->dev);
	struct spi_transfer index_xfer = {
		.len            = 2,
		//.cs_change      = 1,
		.speed_hz	= wk2xxx_spi_speed,
	};

	mutex_lock(&chip->wk2xxxs_reg_lock);
	spi_message_init(&msg);
	/* register index */
	buf_reg[0] = 0x00 | reg;
	buf_reg[1] = dat;
	index_xfer.tx_buf = buf_reg;
	spi_message_add_tail(&index_xfer, &msg);
	status = spi_sync(spi, &msg);
	mutex_unlock(&chip->wk2xxxs_reg_lock);

	return status;
}
/*
 * This function read wk2xxx of slave register:
 */
static int wk2xxx_read_slave_reg(struct spi_device *spi, uint8_t port,
				 uint8_t reg, uint8_t *dat)
{
	struct spi_message msg;
	uint8_t buf_wdat[2];
	uint8_t buf_rdat[2];
	int status;
	struct wk2xxx_chip *chip = dev_get_drvdata(&spi->dev);
	struct spi_transfer index_xfer = {
		.len            = 2,
		//.cs_change      = 1,
		.speed_hz	= wk2xxx_spi_speed,
	};

	mutex_lock(&chip->wk2xxxs_reg_lock);
	status = 0;
	spi_message_init(&msg);
	buf_wdat[0] = 0x40 | (((port-1) << 4) | reg);
	buf_wdat[1] = 0x00;
	buf_rdat[0] = 0x00;
	buf_rdat[1] = 0x00;
	index_xfer.tx_buf = buf_wdat;
	index_xfer.rx_buf = (void *) buf_rdat;
	spi_message_add_tail(&index_xfer, &msg);
	status = spi_sync(spi, &msg);
	mutex_unlock(&chip->wk2xxxs_reg_lock);
	if (status)
		return status;

	*dat = buf_rdat[1];

	return 0;
}

/*
 * This function write wk2xxx of Slave register:
 */
static int wk2xxx_write_slave_reg(struct spi_device *spi, uint8_t port,
				  uint8_t reg, uint8_t dat)
{
	struct spi_message msg;
	uint8_t buf_reg[2];
	int status;
	struct wk2xxx_chip *chip = dev_get_drvdata(&spi->dev);
	struct spi_transfer index_xfer = {
		.len            = 2,
		//.cs_change      = 1,
		.speed_hz	= wk2xxx_spi_speed,
	};

	mutex_lock(&chip->wk2xxxs_reg_lock);
	spi_message_init(&msg);
	/* register index */
	buf_reg[0] = ((port-1)<<4) | reg;
	buf_reg[1] = dat;
	index_xfer.tx_buf = buf_reg;
	spi_message_add_tail(&index_xfer, &msg);
	status = spi_sync(spi, &msg);
	mutex_unlock(&chip->wk2xxxs_reg_lock);

	return status;
}

/*
 * This function read wk2xxx of fifo:
 */
#if 0
static int wk2xxx_read_fifo(struct spi_device *spi, uint8_t port,
			    uint8_t fifolen, uint8_t *dat)
{
	struct spi_message msg;
	int status, i;
	uint8_t recive_fifo_data[MAX_RFCOUNT_SIZE+1] = {0};
	uint8_t transmit_fifo_data[MAX_RFCOUNT_SIZE+1] = {0};
	struct wk2xxx_chip *chip = dev_get_drvdata(&spi->dev);
	struct spi_transfer index_xfer = {
		.len            = fifolen+1,
		//.cs_change      = 1,
		.speed_hz	= wk2xxx_spi_speed,
	};

	if (!(fifolen > 0)) {
		pr_info("%s, fifolen error!!\n", __func__);
		return 1;
	}

	mutex_lock(&chip->wk2xxxs_reg_lock);
	spi_message_init(&msg);
	/* register index */
	transmit_fifo_data[0] = ((port-1) << 4) | 0xc0;
	index_xfer.tx_buf = transmit_fifo_data;
	index_xfer.rx_buf = (void *) recive_fifo_data;
	spi_message_add_tail(&index_xfer, &msg);

	status = spi_sync(spi, &msg);
	udelay(1);
	for (i = 0; i < fifolen; i++)
		*(dat+i) = recive_fifo_data[i+1];
	mutex_unlock(&chip->wk2xxxs_reg_lock);

	return status;
}
#endif

/*
 * This function write wk2xxx of fifo:
 */
#if 0
static int wk2xxx_write_fifo(struct spi_device *spi, uint8_t port,
			     uint8_t fifolen, uint8_t *dat)
{
	struct spi_message msg;
	int status, i;
	uint8_t recive_fifo_data[MAX_RFCOUNT_SIZE+1] = {0};
	uint8_t transmit_fifo_data[MAX_RFCOUNT_SIZE+1] = {0};
	struct wk2xxx_chip *chip = dev_get_drvdata(&spi->dev);
	struct spi_transfer index_xfer = {
		.len            = fifolen+1,
		//.cs_change      = 1,
		.speed_hz	= wk2xxx_spi_speed,
	};

	if (!(fifolen > 0)) {
		pr_info("%s,fifolen error!!\n", __func__);
		return 1;
	}

	mutex_lock(&chip->wk2xxxs_reg_lock);
	spi_message_init(&msg);
	/* register index */
	transmit_fifo_data[0] = ((port - 1) << 4) | 0x80;
	for (i = 0; i < fifolen; i++)
		transmit_fifo_data[i+1] =  *(dat+i);

	index_xfer.tx_buf = transmit_fifo_data;
	index_xfer.rx_buf = (void *) recive_fifo_data;
	spi_message_add_tail(&index_xfer, &msg);
	status = spi_sync(spi, &msg);
	mutex_unlock(&chip->wk2xxxs_reg_lock);

	return status;

}
#endif

static void wk2xxxirq_app(struct uart_port *port);
static void conf_wk2xxx_subport(struct uart_port *port);
static void wk2xxx_work(struct work_struct *w);
static void wk2xxx_stop_tx(struct uart_port *port);
static u_int wk2xxx_tx_empty(struct uart_port *port);

static int wk2xxx_dowork(struct wk2xxx_port *s)
{
#ifdef _DEBUG_WK2XXX
	pr_info("--%s---\n", __func__);
#endif
	if (!s->force_end_work && !work_pending(&s->work) &&
	    !freezing(current) && !s->suspending) {
		queue_work(s->workqueue, &s->work);
#ifdef _DEBUG_WK2XXX
		pr_info("--queue_work---ok---\n");
#endif
		return 1;
	}
	return 0;
}

static void wk2xxx_work(struct work_struct *w)
{
	struct wk2xxx_port *s = container_of(w, struct wk2xxx_port, work);
	struct wk2xxx_chip *chip = s->chip;
	uint8_t rx;
	int work_start_tx_flag;
	int work_stop_rx_flag;
	int work_irq_flag;
	int work_conf_flag;

#ifdef _DEBUG_WK2XXX
	pr_info("--%s---in---\n", __func__);
#endif
	do {
		mutex_lock(&chip->wk2xxs_work_lock);

		work_start_tx_flag = s->start_tx_flag;
		if (work_start_tx_flag)
			s->start_tx_flag = 0;

		work_stop_rx_flag = s->stop_rx_flag;
		if (work_stop_rx_flag)
			s->stop_rx_flag = 0;
		work_conf_flag = s->conf_flag;
		if (work_conf_flag)
			s->conf_flag = 0;

		work_irq_flag = s->irq_flag;
		if (work_irq_flag)
			s->irq_flag = 0;
		mutex_unlock(&chip->wk2xxs_work_lock);

		if (work_start_tx_flag) {
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					      WK2XXX_SIER, &rx);
			rx |= WK2XXX_TFTRIG_IEN | WK2XXX_RFTRIG_IEN |
				WK2XXX_RXOUT_IEN;
			wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
					       WK2XXX_SIER, rx);
		}

		if (work_stop_rx_flag) {
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					      WK2XXX_SIER, &rx);
			rx &=  ~WK2XXX_RFTRIG_IEN;
			rx &=  ~WK2XXX_RXOUT_IEN;
			wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
					       WK2XXX_SIER, rx);
		}

		if (work_irq_flag) {
			wk2xxxirq_app(&s->port);
			s->irq_fail = 1;
		}

	} while (!s->force_end_work && !freezing(current) &&
		 (work_irq_flag || work_stop_rx_flag));

	if (s->start_tx_fail) {
		wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
				      WK2XXX_SIER, &rx);
		rx |= WK2XXX_TFTRIG_IEN | WK2XXX_RFTRIG_IEN |
			WK2XXX_RXOUT_IEN;
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_SIER, rx);
		s->start_tx_fail = 0;
	}

	if (s->stop_rx_fail) {
		wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
				      WK2XXX_SIER, &rx);
		rx &=  ~WK2XXX_RFTRIG_IEN;
		rx &=  ~WK2XXX_RXOUT_IEN;
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_SIER, rx);
		s->stop_rx_fail = 0;
	}

	if (s->irq_fail) {
		s->irq_fail = 0;
		enable_irq(s->port.irq);
	}

#ifdef _DEBUG_WK2XXX
	pr_info("--%s---exit---\n", __func__);
#endif
}

static void wk2xxx_rx_chars(struct uart_port *port)
{
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	uint8_t fsr, lsr, dat[1], rx_dat[256] = {0};
	unsigned int ch, flg, sifr, ignored = 0, status = 0, rx_count = 0;
	int rfcnt = 0, rx_num = 0;

#ifdef _DEBUG_WK2XXX5
	pr_info("%s---------in---\n", __func__);
#endif
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SPAGE,
			       WK2XXX_PAGE0);
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_FSR, dat);
	fsr = dat[0];
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_LSR, dat);
	lsr = dat[0];
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SIFR, dat);
	sifr = dat[0];
#ifdef _DEBUG_WK2XXX
	pr_info("rx_chars()-port:%lx--fsr:0x%x--lsr:0x%x--\n",
		s->port.iobase, fsr, lsr);
#endif

	if (!(sifr & 0x80)) {
		flg = TTY_NORMAL;
		if (fsr & WK2XXX_RDAT) {
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					      WK2XXX_RFCNT, dat);
			rfcnt = dat[0];
			if (rfcnt == 0)
				rfcnt = 255;

			for (rx_num = 0; rx_num < rfcnt; rx_num++) {
				wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
						      WK2XXX_FDAT, dat);
				rx_dat[rx_num] = dat[0];
			}

			s->port.icount.rx += rfcnt;
			for (rx_num = 0; rx_num < rfcnt; rx_num++) {
				if (uart_handle_sysrq_char(&s->port,
							   rx_dat[rx_num]))
					break;
#ifdef _DEBUG_WK2XXX5
				pr_info("rx_chars:0x%x----\n", rx_dat[rx_num]);
#endif
				uart_insert_char(&s->port, status,
						 WK2XXX_STATUS_OE,
						 rx_dat[rx_num], flg);
				rx_count++;

				if ((rx_count >= 64) &&
				    (s->port.state->port.tty->port != NULL)) {
					tty_flip_buffer_push(
						s->port.state->port.tty->port);
					rx_count = 0;
				}
			}

			if ((rx_count > 0) &&
			    (s->port.state->port.tty->port != NULL)) {
#ifdef _DEBUG_WK2XXX
				pr_info("push buffer tty flip port = 0x%lx count =%d\n",
						s->port.iobase, rx_count);
#endif
				tty_flip_buffer_push(
						s->port.state->port.tty->port);
				rx_count = 0;
			}
		}
	} else {
		while (fsr & WK2XXX_RDAT) {
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					      WK2XXX_FDAT, dat);
			ch = (int)dat[0];
#ifdef _DEBUG_WK2XXX
			pr_info("%s: port:%lx--RXDAT:0x%x----\n",
				__func__, s->port.iobase, ch);
#endif
			s->port.icount.rx++;
			//rx_count++;
#ifdef _DEBUG_WK2XXX1
			pr_info("%s: ----port:%lx error\n", __func__,
				s->port.iobase);
#endif
			flg = TTY_NORMAL;
			if (lsr&(WK2XXX_OE | WK2XXX_FE|WK2XXX_PE|WK2XXX_BI)) {
				pr_info("%s: ----port:%lx error,lsr:%x!\n",
					__func__, s->port.iobase, lsr);
				//goto handle_error;
				if (lsr & WK2XXX_PE) {
					s->port.icount.parity++;
					status |= WK2XXX_STATUS_PE;
					flg = TTY_PARITY;
				}

				if (lsr & WK2XXX_FE) {
					s->port.icount.frame++;
					status |= WK2XXX_STATUS_FE;
					flg = TTY_FRAME;
				}

				if (lsr & WK2XXX_OE) {
					s->port.icount.overrun++;
					status |= WK2XXX_STATUS_OE;
					flg = TTY_OVERRUN;
				}

				if (lsr&fsr & WK2XXX_BI) {
					s->port.icount.brk++;
					status |= WK2XXX_STATUS_BRK;
					flg = TTY_BREAK;
				}

				if (++ignored > 100)
					goto out;

				goto ignore_char;
			}
error_return:
			if (uart_handle_sysrq_char(&s->port, ch))
				goto ignore_char;

			uart_insert_char(&s->port, status, WK2XXX_STATUS_OE,
					 ch, flg);
			rx_count++;

			if ((rx_count >= 64) &&
			    (s->port.state->port.tty->port != NULL)) {
				tty_flip_buffer_push(
					s->port.state->port.tty->port);
				rx_count = 0;
			}
#ifdef _DEBUG_WK2XXX1
			pr_info("s->port.icount.rx = 0x%x char = 0x%x flg = 0x%X port = 0x%lx rx_count = %d\n",
				s->port.icount.rx, ch, flg, s->port.iobase,
				rx_count);
#endif
ignore_char:
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					      WK2XXX_FSR, dat);
			fsr = dat[0];
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					      WK2XXX_LSR, dat);
			lsr = dat[0];
		}
out:
		if ((rx_count > 0) && (s->port.state->port.tty->port != NULL)) {
#ifdef _DEBUG_WK2XXX1
			pr_info("push buffer tty flip port = %lx count = %d\n",
				s->port.iobase, rx_count);
#endif
			tty_flip_buffer_push(s->port.state->port.tty->port);
			rx_count = 0;
		}

	}

#if 0
	pr_info(" rx_num = :%d\n", s->port.icount.rx);
#endif

#ifdef _DEBUG_WK2XXX
	pr_info("%s---------out---\n", __func__);
#endif
	return;
#ifdef SUPPORT_SYSRQ
	s->port.state->sysrq = 0;
#endif
	goto error_return;
#ifdef _DEBUG_WK2XXX
	pr_info("--%s---exit---\n", __func__);
#endif
}

static void wk2xxx_tx_chars(struct uart_port *port)
{
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	uint8_t fsr, tfcnt, dat[1], txbuf[255] = {0};
	int count, tx_count, i;

#ifdef _DEBUG_WK2XXX5
	pr_info("--%s---in---\n", __func__);
#endif
	if (s->port.x_char) {
#ifdef _DEBUG_WK2XXX
		pr_info("s->port.x_char:%x,port = %lx\n",
		       s->port.x_char, s->port.iobase);
#endif
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_FDAT,
				       s->port.x_char);
		s->port.icount.tx++;
		s->port.x_char = 0;
		goto out;
	}

	if (uart_circ_empty(&s->port.state->xmit) ||
	    uart_tx_stopped(&s->port))
		goto out;

	/*
	 * Tried using FIFO (not checking TNF) for fifo fill:
	 * still had the '1 bytes repeated' problem.
	 */
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_FSR, dat);
	fsr = dat[0];

	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_TFCNT, dat);
	tfcnt = dat[0];
#ifdef _DEBUG_WK2XXX
	pr_info("fsr:0x%x,rfcnt:0x%x,port = %lx\n", fsr, tfcnt, s->port.iobase);
#endif
	if (tfcnt == 0) {
		tx_count = (fsr & WK2XXX_TFULL)?0:255;
#ifdef _DEBUG_WK2XXX
		pr_info("tx_count:%x,port = %lx\n", tx_count, s->port.iobase);
#endif
	} else {
		tx_count = 255-tfcnt;
#ifdef _DEBUG_WK2XXX
		pr_info("tx_count:%x,port = %lx\n", tx_count, s->port.iobase);
#endif
	}

#ifdef _DEBUG_WK2XXX
	pr_info("fsr:%x\n", fsr);
#endif
	count = tx_count;
	i = 0;
	do {
		if (uart_circ_empty(&s->port.state->xmit))
			break;
		txbuf[i] = s->port.state->xmit.buf[s->port.state->xmit.tail];
		s->port.state->xmit.tail =
			(s->port.state->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
		s->port.icount.tx++;
		i++;
#ifdef _DEBUG_WK2XXX
		pr_info("tx_chars:0x%x--\n", txbuf[i-1]);
#endif
	} while (--count > 0);

#if 0
	wk2xxx_write_fifo(s->spi_wk, s->port.iobase, i, txbuf);
#else
	for (count = 0; count < i; count++)
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_FDAT, txbuf[count]);
#endif


#ifdef _DEBUG_WK2XXX
	pr_info("icount.tx:%d,xmit.head1:%d,xmit.tail:%d,UART_XMIT_SIZE::%lx, char:%x,fsr:0x%x,port = %lx\n",
		s->port.icount.tx, s->port.state->xmit.head,
		s->port.state->xmit.tail, UART_XMIT_SIZE,
		s->port.state->xmit.buf[s->port.state->xmit.tail], fsr,
		s->port.iobase);
#endif
out:
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_FSR, dat);
	fsr = dat[0];
	if (((fsr&WK2XXX_TDAT) == 0) && ((fsr&WK2XXX_TBUSY) == 0)) {
		if (uart_circ_chars_pending(&s->port.state->xmit)
			< WAKEUP_CHARS)
			uart_write_wakeup(&s->port);

		if (uart_circ_empty(&s->port.state->xmit))
			wk2xxx_stop_tx(&s->port);
	}

#ifdef _DEBUG_WK2XXX
	pr_info("--%s---exit---\n", __func__);
#endif
}

static irqreturn_t wk2xxx_irq(int irq, void *dev_id)
{
	struct wk2xxx_port *s = dev_id;

	disable_irq_nosync(s->port.irq);
	s->irq_flag = 1;
	if (wk2xxx_dowork(s)) {
		;
	} else {
		s->irq_flag = 0;
		s->irq_fail = 1;
	}

	return IRQ_HANDLED;
}

static void wk2xxxirq_app(struct uart_port *port)//
{
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	unsigned int  pass_counter = 0;
	uint8_t sifr, gifr, sier, dat[1];

#ifdef _DEBUG_WK2XXX
	pr_info("%s: ------port:%lx--------------\n", __func__, s->port.iobase);
#endif
	wk2xxx_read_global_reg(s->spi_wk, WK2XXX_GIFR, dat);
	gifr = dat[0];
	switch (s->port.iobase) {
	case 1:
		if (!(gifr & WK2XXX_UT1INT))
			return;
		break;
	case 2:
		if (!(gifr & WK2XXX_UT2INT))
			return;
		break;
	case 3:
		if (!(gifr & WK2XXX_UT3INT))
			return;
		break;
	case 4:
		if (!(gifr & WK2XXX_UT4INT))
			return;
		break;
	default:
		break;

	}

	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SIFR, dat);
	sifr = dat[0];
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SIER, dat);
	sier = dat[0];
#ifdef _DEBUG_WK2XXX1
	pr_info("irq_app..........sifr:%x sier:%x\n", sifr, sier);
#endif
	do {
		if ((sifr&WK2XXX_RFTRIG_INT) || (sifr&WK2XXX_RXOVT_INT))
			wk2xxx_rx_chars(&s->port);

		if ((sifr & WK2XXX_TFTRIG_INT) && (sier & WK2XXX_TFTRIG_IEN)) {
			wk2xxx_tx_chars(&s->port);
			return;
		}

		if (pass_counter++ > WK2XXX_ISR_PASS_LIMIT)
			break;

		wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
				      WK2XXX_SIFR, dat);
		sifr = dat[0];
		wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
				      WK2XXX_SIER, dat);
		sier = dat[0];
#ifdef _DEBUG_WK2XXX1
		pr_info(".........rx..........tx  sifr:%x sier:%x port:%lx\n",
			sifr, sier, s->port.iobase);
#endif
	} while ((sifr&WK2XXX_RXOVT_INT) || (sifr & WK2XXX_RFTRIG_INT) ||
		 ((sifr & WK2XXX_TFTRIG_INT) && (sier & WK2XXX_TFTRIG_IEN)));
#ifdef _DEBUG_WK2XXX
	pr_info("%s: ---------exit---\n", __func__);
#endif
}

/*
 *   Return TIOCSER_TEMT when transmitter is not busy.
 */
static u_int wk2xxx_tx_empty(struct uart_port *port)
{
	uint8_t tx;
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	//struct wk2xxx_chip *chip = s->chip;

	pr_debug("%s: ---------in---\n", __func__);

	//mutex_lock(&chip->wk2xxxs_lock);
	if (!(s->tx_empty_flag || s->tx_empty_fail)) {
		wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
				      WK2XXX_FSR, &tx);
		while ((tx & WK2XXX_TDAT)|(tx&WK2XXX_TBUSY)) {
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					      WK2XXX_FSR, &tx);
		}
		s->tx_empty = ((tx & WK2XXX_TDAT)|(tx&WK2XXX_TBUSY)) <= 0;
		if (s->tx_empty) {
			s->tx_empty_flag = 0;
			s->tx_empty_fail = 0;
		} else {
			s->tx_empty_fail = 0;
			s->tx_empty_flag = 0;
		}
	}
	//mutex_unlock(&chip->wk2xxxs_lock);

#ifdef _DEBUG_WK2XXX5
	pr_info("s->tx_empty_fail----FSR:%d--s->tx_empty:%d--\n",
		tx, s->tx_empty);
#endif
#ifdef _DEBUG_WK2XXX
	pr_info("%s: ----------exit---\n", __func__);
#endif
	return s->tx_empty;
}

static void wk2xxx_set_mctrl(struct uart_port *port, u_int mctrl)
{
#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif
}
static u_int wk2xxx_get_mctrl(struct uart_port *port)
{
#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

/*
 *  interrupts disabled on entry
 */
static void wk2xxx_stop_tx(struct uart_port *port)
{

	uint8_t dat[1], sier, sifr;
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	//struct wk2xxx_chip *chip = s->chip;

#ifdef _DEBUG_WK2XXX
	pr_info("%s: ------in---\n", __func__);
#endif
	//mutex_lock(&chip->wk2xxxs_lock);
	if (!(s->stop_tx_flag || s->stop_tx_fail)) {
		wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
				      WK2XXX_SIER, dat);
		sier = dat[0];
		s->stop_tx_fail = (sier&WK2XXX_TFTRIG_IEN) > 0;
		if (s->stop_tx_fail) {
			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					WK2XXX_SIER, dat);
			sier = dat[0];
			sier &=  ~WK2XXX_TFTRIG_IEN;
			wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
					WK2XXX_SIER, sier);


			wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
					WK2XXX_SIFR, dat);
			sifr = dat[0];
			sifr &= ~WK2XXX_TFTRIG_INT;
			wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
					WK2XXX_SIFR, sifr);
			s->stop_tx_fail = 0;
			s->stop_tx_flag = 0;
		} else {
			s->stop_tx_fail = 0;
			s->stop_tx_flag = 0;
		}
	}
	//mutex_unlock(&chip->wk2xxxs_lock);
#ifdef _DEBUG_WK2XXX4
	pr_info("%s: ------exit---\n", __func__);
#endif
}

/*
 * interrupts may not be disabled on entry
 */
static void wk2xxx_start_tx(struct uart_port *port)
{

	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------in---\n", __func__);
#endif
	if (!(s->start_tx_flag || s->start_tx_fail)) {
		s->start_tx_flag = 1;
		if (wk2xxx_dowork(s)) {
			;
		} else {
			s->start_tx_fail = 1;
			s->start_tx_flag = 0;
		}
	}

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------exit---\n", __func__);
#endif

}

/*
 * Interrupts enabled
 */
static void wk2xxx_stop_rx(struct uart_port *port)
{
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);

#ifdef _DEBUG_WK2XXX
	pr_info("--%s------in---\n", __func__);
#endif
	if (!(s->stop_rx_flag || s->stop_rx_fail)) {
		s->stop_rx_flag = 1;
		if (wk2xxx_dowork(s)) {
			;
		} else {
			s->stop_rx_flag = 0;
			s->stop_rx_fail = 1;
		}
	}
#ifdef _DEBUG_WK2XXX
	pr_info("-%s------exit---\n", __func__);
#endif
}


/*
 * No modem control lines
 */
static void wk2xxx_enable_ms(struct uart_port *port)
{
#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif
}

/*
 * Interrupts always disabled.
 */
static void wk2xxx_break_ctl(struct uart_port *port, int break_state)
{
#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif
}

static int wk2xxx_startup(struct uart_port *port)//i
{
	uint8_t gena, grst, gier, sier, scr, dat[1];
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	struct wk2xxx_chip *chip = s->chip;
	char b[12];
	char *irq_name;

#ifdef _DEBUG_WK2XXX5
	pr_info("-%s------in---\n", __func__);
#endif
	if (s->suspending)
		return 0;

	s->force_end_work = 0;
	sprintf(b, "wk2xxx-%d", (uint8_t)s->port.iobase);
	s->workqueue = create_workqueue(b);

	if (!s->workqueue) {
		dev_warn(&s->spi_wk->dev, "cannot create workqueue\n");
		return -EBUSY;
	}

	INIT_WORK(&s->work, wk2xxx_work);

	if (s->wk2xxx_hw_suspend)
		s->wk2xxx_hw_suspend(0);

	wk2xxx_read_global_reg(s->spi_wk, WK2XXX_GENA, dat);
	gena = dat[0];
	switch (s->port.iobase) {
	case 1:
		gena |= WK2XXX_UT1EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	case 2:
		gena |= WK2XXX_UT2EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	case 3:
		gena |= WK2XXX_UT3EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	case 4:
		gena |= WK2XXX_UT4EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	default:
		pr_info(":con_wk2xxx_subport bad iobase %d\n",
			(uint8_t)s->port.iobase);
		break;
	}

	wk2xxx_read_global_reg(s->spi_wk, WK2XXX_GRST, dat);
	grst = dat[0];
	switch (s->port.iobase) {
	case 1:
		grst |= WK2XXX_UT1RST;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GRST, grst);
		break;
	case 2:
		grst |= WK2XXX_UT2RST;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GRST, grst);
		break;
	case 3:
		grst |= WK2XXX_UT3RST;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GRST, grst);
		break;
	case 4:
		grst |= WK2XXX_UT4RST;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GRST, grst);
		break;
	default:
		pr_info(":con_wk2xxx_subport bad iobase %d\n",
			(uint8_t)s->port.iobase);
		break;
	}

	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SIER, dat);
	sier = dat[0];
	sier &= ~WK2XXX_TFTRIG_IEN;
	sier |= WK2XXX_RFTRIG_IEN;
	sier |= WK2XXX_RXOUT_IEN;
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SIER, sier);

	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SCR, dat);
	scr = dat[0] | WK2XXX_TXEN|WK2XXX_RXEN;
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SCR, scr);

	//initiate the fifos
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_FCR, 0xff);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_FCR, 0xfc);
	//set rx/tx interrupt
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SPAGE, 1);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_RFTL, 0x40);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_TFTL, 0X20);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SPAGE, 0);
	//enable the sub port interrupt
	wk2xxx_read_global_reg(s->spi_wk, WK2XXX_GIER, dat);
	gier = dat[0];

	switch (s->port.iobase) {
	case 1:
		gier |= WK2XXX_UT1IE;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GIER, gier);
		break;
	case 2:
		gier |= WK2XXX_UT2IE;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GIER, gier);
		break;
	case 3:
		gier |= WK2XXX_UT3IE;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GIER, gier);
		break;
	case 4:
		gier |= WK2XXX_UT4IE;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GIER, gier);
		break;
	default:
		pr_info(": bad iobase %d\n", (uint8_t)s->port.iobase);
		break;
	}

	if (s->wk2xxx_hw_suspend)
		s->wk2xxx_hw_suspend(0);
	msleep(50);

	uart_circ_clear(&s->port.state->xmit);
	wk2xxx_enable_ms(&s->port);

	// request irq
	irq_name = kzalloc(NAME_LEN, GFP_KERNEL);
	if (!irq_name)
		return -ENOMEM;

	sprintf(irq_name, "wk2xxx%d_irq_gpio", chip->chip_num);
	if (request_irq(s->port.irq, wk2xxx_irq, IRQF_SHARED|IRQF_TRIGGER_LOW,
	    irq_name, s) < 0) {
		dev_warn(&s->spi_wk->dev, "cannot allocate irq %d\n",
			 s->port.irq);
		s->port.irq = 0;
		destroy_workqueue(s->workqueue);
		s->workqueue = NULL;
		return -EBUSY;
	}       udelay(100);
	udelay(100);

#ifdef _DEBUG_WK2XXX5
	pr_info("-%s------exit---\n", __func__);
#endif

	return 0;
}

/* Power down all displays on reboot, poweroff or halt */
static void wk2xxx_shutdown(struct uart_port *port)
{
	uint8_t gena, dat[1];
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);

#ifdef _DEBUG_WK2XXX
	pr_info("--%s------in---\n", __func__);
#endif
	if (s->suspending)
		return;
	s->force_end_work = 1;
	if (s->workqueue) {
		flush_workqueue(s->workqueue);
		destroy_workqueue(s->workqueue);
		s->workqueue = NULL;
	}

	if (s->port.irq)
		free_irq(s->port.irq, s);

	wk2xxx_read_global_reg(s->spi_wk, WK2XXX_GENA, dat);
	gena = dat[0];
	switch (s->port.iobase) {
	case 1:
		gena &=  ~WK2XXX_UT1EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	case 2:
		gena &=  ~WK2XXX_UT2EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	case 3:
		gena &=  ~WK2XXX_UT3EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	case 4:
		gena &=  ~WK2XXX_UT4EN;
		wk2xxx_write_global_reg(s->spi_wk, WK2XXX_GENA, gena);
		break;
	default:
		pr_info(":con_wk2xxx_subport bad iobase %d\n",
			(uint8_t)s->port.iobase);
		break;
	}

#ifdef _DEBUG_WK2XXX
	pr_info("-%s-----exit---\n", __func__);
#endif
}

static void conf_wk2xxx_subport(struct uart_port *port)//i
{
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	uint8_t old_sier, fwcr, lcr, scr, scr_ss, dat[1];
	uint8_t baud0_ss, baud1_ss, pres_ss;

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------in---\n", __func__);
#endif
	lcr = s->new_lcr;
	scr_ss = s->new_scr;
	baud0_ss = s->new_baud0;
	baud1_ss = s->new_baud1;
	pres_ss = s->new_pres;
	fwcr = s->new_fwcr;
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SIER, dat);
	old_sier = dat[0];
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SIER,
			       old_sier&(~(WK2XXX_TFTRIG_IEN |
					 WK2XXX_RFTRIG_IEN |
					 WK2XXX_RXOUT_IEN)));
	//local_irq_restore(flags);
	do {
		wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase,
				      WK2XXX_FSR, dat);
	} while (dat[0] & WK2XXX_TBUSY);
	// then, disable tx and rx
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SCR, dat);
	scr = dat[0];
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SCR,
			       scr&(~(WK2XXX_RXEN|WK2XXX_TXEN)));
	// set the parity, stop bits and data size //
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_LCR, lcr);
	/*set cts  and rst*/
	if (fwcr > 0) {
#ifdef _DEBUG_WK2XXX
		pr_info("-set ctsrts--fwcr=0x%X\n", fwcr);
#endif
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_FWCR, fwcr);
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_SPAGE, 1);
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_FWTH, 0XF0);
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_FWTL, 0X80);
		wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
				       WK2XXX_SPAGE, 0);
	}

	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
			       WK2XXX_SIER, old_sier);
	// set the baud rate //
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
			       WK2XXX_SPAGE, 1);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
			       WK2XXX_BAUD0, baud0_ss);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
			       WK2XXX_BAUD1, baud1_ss);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase,
			       WK2XXX_PRES, pres_ss);
#ifdef _DEBUG_WK2XXX2
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_BAUD0, dat);
	pr_info(":WK2XXX_BAUD0=0x%X\n", dat[0]);
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_BAUD1, dat);
	pr_info(":WK2XXX_BAUD1=0x%X\n", dat[0]);
	wk2xxx_read_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_PRES, dat);
	pr_info(":WK2XXX_PRES=0x%X\n", dat[0]);
#endif
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SPAGE, 0);
	wk2xxx_write_slave_reg(s->spi_wk, s->port.iobase, WK2XXX_SCR,
			       scr | (WK2XXX_RXEN | WK2XXX_TXEN));

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------exit---\n", __func__);
#endif
}

static void wk2xxx_termios(struct uart_port *port, struct ktermios *termios,
	    struct ktermios *old)
{
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);
	int baud = 0;
	uint8_t lcr = 0, fwcr, baud1, baud0, pres;
	unsigned short cflag;
	unsigned short lflag;

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------in---\n", __func__);
#endif

	cflag = termios->c_cflag;
	lflag = termios->c_lflag;
#ifdef _DEBUG_WK2XXX1
	pr_info("cflag := 0x%X  lflag : = 0x%X\n", cflag, lflag);
#endif
	baud1 = 0;
	baud0 = 0;
	pres = 0;
	baud = tty_termios_baud_rate(termios);

	switch (baud) {
	case 600:
		baud1 = 0x4;
		baud0 = 0x7f;
		pres = 0;
		break;
	case 1200:
		baud1 = 0x2;
		baud0 = 0x3F;
		pres = 0;
		break;
	case 2400:
		baud1 = 0x1;
		baud0 = 0x1f;
		pres = 0;
		break;
	case 4800:
		baud1 = 0x00;
		baud0 = 0x8f;
		pres = 0;
		break;
	case 9600:
		baud1 = 0x00;
		baud0 = 0x47;
		pres = 0;
		break;
	case 19200:
		baud1 = 0x00;
		baud0 = 0x23;
		pres = 0;
		break;
	case 38400:
		baud1 = 0x00;
		baud0 = 0x11;
		pres = 0;
		break;
	case 76800:
		baud1 = 0x00;
		baud0 = 0x08;
		pres = 0;
		break;
	case 1800:
		baud1 = 0x01;
		baud0 = 0x7f;
		pres = 0;
		break;
	case 3600:
		baud1 = 0x00;
		baud0 = 0xbf;
		pres = 0;
		break;
	case 7200:
		baud1 = 0x00;
		baud0 = 0x5f;
		pres = 0;
		break;
	case 14400:
		baud1 = 0x00;
		baud0 = 0x2f;
		pres = 0;
		break;
	case 28800:
		baud1 = 0x00;
		baud0 = 0x17;
		pres = 0;
		break;
	case 57600:
		baud1 = 0x00;
		baud0 = 0x0b;
		pres = 0;
		break;
	case 115200:
		baud1 = 0x00;
		baud0 = 0x05;
		pres = 0;
		break;
	case 230400:
		baud1 = 0x00;
		baud0 = 0x02;
		pres = 0;
		break;
	default:
		baud1 = 0x00;
		baud0 = 0x00;
		pres = 0;
		break;
	}
	tty_termios_encode_baud_rate(termios, baud, baud);

	/* we are sending char from a workqueue so enable */
#ifdef _DEBUG_WK2XXX
	pr_info("port:%lx--lcr:0x%x- cflag:0x%x-CSTOPB:0x%x,PARENB:0x%x,PARODD:0x%x--\n",
		s->port.iobase, lcr, cflag, CSTOPB, PARENB, PARODD);
#endif

	lcr = 0;
	if (cflag & CSTOPB)
		lcr |= WK2XXX_STPL;//two  stop_bits
	else
		lcr &=  ~WK2XXX_STPL;//one  stop_bits

	if (cflag & PARENB) {
		lcr |= WK2XXX_PAEN;//enbale spa
		if (!(cflag & PARODD)) {
			lcr |= WK2XXX_PAM1;
			lcr &= ~WK2XXX_PAM0;
		} else {
			lcr |= WK2XXX_PAM0;//PAM0=1
			lcr &= ~WK2XXX_PAM1;//PAM1=0
		}
	} else {
		lcr &=  ~WK2XXX_PAEN;
	}

	/*set rts and cts*/
	fwcr = (termios->c_cflag&CRTSCTS)?0X30:0;

#ifdef _DEBUG_WK2XXX
	pr_info("port:%lx--lcr:0x%x- cflag:0x%x-CSTOPB:0x%x,PARENB:0x%x,PARODD:0x%x--\n",
		s->port.iobase, lcr, cflag, CSTOPB, PARENB, PARODD);
#endif
	s->new_baud1 = baud1;
	s->new_baud0 = baud0;
	s->new_pres = pres;
	s->new_lcr = lcr;
	s->new_fwcr = fwcr;

#ifdef _DEBUG_WK2XXX
	pr_info("port:%lx--NEW_FWCR-\n", s->port.iobase, s->new_fwcr);
#endif
	conf_wk2xxx_subport(&s->port);

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------exit---\n", __func__);
#endif
}

static const char *wk2xxx_type(struct uart_port *port)
{
#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif
	return port->type == PORT_WK2XXX ? "wk2xxx" : NULL;
}

static void wk2xxx_release_port(struct uart_port *port)
{
  #ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
  #endif
}

static int wk2xxx_request_port(struct uart_port *port)
{
#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif
	return 0;
}

static void wk2xxx_config_port(struct uart_port *port, int flags)
{
	struct wk2xxx_port *s = container_of(port, struct wk2xxx_port, port);

#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif

	if (flags & UART_CONFIG_TYPE && wk2xxx_request_port(port) == 0)
		s->port.type = PORT_WK2XXX;
}

static int wk2xxx_verify_port(struct uart_port *port, struct serial_struct *ser)
{

	int ret = 0;

#ifdef _DEBUG_WK2XXX
	pr_info("%s!!\n", __func__);
#endif

	if (ser->type != PORT_UNKNOWN && ser->type != PORT_WK2XXX)
		ret = -EINVAL;
	if (port->irq != ser->irq)
		ret = -EINVAL;
	if (ser->io_type != SERIAL_IO_PORT)
		ret = -EINVAL;
	//if (port->uartclk / 16 != ser->baud_base)
	//      ret = -EINVAL;
	if (port->iobase != ser->port)
		ret = -EINVAL;
	if (ser->hub6 != 0)
		ret = -EINVAL;

	return ret;
}

const static struct uart_ops wk2xxx_pops = {
	.tx_empty = wk2xxx_tx_empty,
	.set_mctrl = wk2xxx_set_mctrl,
	.get_mctrl = wk2xxx_get_mctrl,
	.stop_tx = wk2xxx_stop_tx,
	.start_tx = wk2xxx_start_tx,
	.stop_rx = wk2xxx_stop_rx,
	.enable_ms = wk2xxx_enable_ms,
	.break_ctl = wk2xxx_break_ctl,
	.startup = wk2xxx_startup,
	.shutdown = wk2xxx_shutdown,
	.set_termios = wk2xxx_termios,
	.type = wk2xxx_type,
	.release_port = wk2xxx_release_port,
	.request_port = wk2xxx_request_port,
	.config_port = wk2xxx_config_port,
	.verify_port = wk2xxx_verify_port,

};

static int semidrive_spi_parse_dt(struct device *dev)
{

	int irq_gpio, irq_flags, irq, rst_gpio;
	enum of_gpio_flags flags;

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------in---\n", __func__);
#endif
	irq_gpio = of_get_named_gpio_flags(dev->of_node, "irq_gpio", 0,
					   (enum of_gpio_flags *)&irq_flags);
	if (!gpio_is_valid(irq_gpio)) {
		pr_info("invalid wk2xxx_irq_gpio: %d\n", irq_gpio);
		return -1;
	}

	gpio_direction_input(irq_gpio);
	irq = gpio_to_irq(irq_gpio);
	if (!irq) {
		pr_info("wk2xxx_irqGPIO: %d get irq failed!\n", irq);
		return -1;
	}
	pr_info("wk2xxx_irq_gpio: %d, irq: %d", irq_gpio, irq);

	rst_gpio = of_get_named_gpio_flags(dev->of_node, "reset-gpios",
					   0, &flags);
	if (!gpio_is_valid(rst_gpio)) {
		pr_info("invalid wk2xxx_rst_gpio: %d\n", rst_gpio);
		return -1;
	}

	gpio_direction_output(rst_gpio, 0);
	usleep_range(10000, 12000);
	gpio_set_value(rst_gpio, 1);
	usleep_range(5000, 6000);

#ifdef _DEBUG_WK2XXX
	pr_info("-%s------out---\n", __func__);
#endif
	return irq;
}

static int wk2xxx_probe(struct spi_device *spi)
{
	uint8_t i;
	int status, irq;
	uint8_t dat[1];
	struct wk2xxx_chip *chip;
	char *driver_name;
	char *dev_name;
	int ret = -1;

	chip = kzalloc(sizeof(struct wk2xxx_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mutex_init(&chip->wk2xxxs_lock);
	mutex_init(&chip->wk2xxxs_reg_lock);
	mutex_init(&chip->wk2xxs_work_lock);
	chip->chip_num = chip_num++;
	dev_set_drvdata(&spi->dev, chip);

	ret = wk2xxx_read_global_reg(spi, WK2XXX_GENA, dat);
	if (ret < 0)
		pr_info("read wk2xxx reg error,ret=%d\n", ret);
	else
		pr_info("GENA = 0x%X\n", dat[0]);

	ret = wk2xxx_write_global_reg(spi, WK2XXX_GENA, 0xf5);
	if (ret < 0)
		pr_err("write wk2xxx 0xf5 error,ret=%d\n", ret);

	ret = wk2xxx_read_global_reg(spi, WK2XXX_GENA, dat);
	if (ret < 0)
		pr_err("read wk2xxx reg error,ret=%d\n", ret);
	else
		pr_info("GENA = 0x%X\n", dat[0]);

	ret = wk2xxx_write_global_reg(spi, WK2XXX_GENA, 0xf0);
	if (ret < 0)
		pr_err("write wk2xxx 0xf0 error,ret=%d\n", ret);

	ret = wk2xxx_read_global_reg(spi, WK2XXX_GENA, dat);
	if (ret < 0)
		pr_err("read wk2xxx reg error,ret=%d\n", ret);
	else
		pr_info("GENA = 0x%X\n", dat[0]);

	irq = semidrive_spi_parse_dt(&spi->dev);
	if (irq < 0)
		return 1;

	ret = wk2xxx_read_global_reg(spi, WK2XXX_GENA, dat);
	if (ret < 0)
		pr_err("read wk2xxx reg error,ret=%d\n", ret);

	if ((dat[0] & 0xf0) != 0x30) {
		pr_info("error GENA = 0x%X\n", dat[0]);
		pr_info("spi driver error!!!!\n");
		return 1;
	}

	driver_name = kzalloc(NAME_LEN, GFP_KERNEL);
	dev_name = kzalloc(NAME_LEN, GFP_KERNEL);
	if (!driver_name || !dev_name) {
		pr_err("%s kzalloc failed at %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	pr_info("chip_num is %d\n", chip->chip_num);
	sprintf(driver_name, "ttySWK-SPI%d", chip->chip_num);
	sprintf(dev_name, "ttys-spi%d-WK", chip->chip_num);
	chip->wk2xxx_uart_driver.driver_name = driver_name;
	chip->wk2xxx_uart_driver.dev_name = dev_name;
	chip->wk2xxx_uart_driver.owner = THIS_MODULE;
	chip->wk2xxx_uart_driver.major = SERIAL_WK2XXX_MAJOR;
	chip->wk2xxx_uart_driver.minor =
		MINOR_START + chip->chip_num * NR_PORTS;
	chip->wk2xxx_uart_driver.nr = NR_PORTS;
	chip->wk2xxx_uart_driver.cons = NULL;
	status = uart_register_driver(&chip->wk2xxx_uart_driver);
	if (status) {
		pr_info("Couldn't register wk2xxx uart driver\n");
		return status;
	}

	for (i = 0; i < NR_PORTS; i++) {
		struct wk2xxx_port *s = &chip->wk2xxxs[i];

		s->chip          = chip;
		s->tx_done       = 0;
		s->spi_wk        = spi;
		s->port.line     = i;
		s->port.ops      = &wk2xxx_pops;
		s->port.uartclk  = WK_CRASTAL_CLK;
		s->port.fifosize = 256;
		s->port.iobase   = i + 1;
		s->port.irq      = irq;
		s->port.iotype   = SERIAL_IO_PORT;
		s->port.flags    = ASYNC_BOOT_AUTOCONF;
		status = uart_add_one_port(&chip->wk2xxx_uart_driver, &s->port);
		if (status < 0) {
			pr_info("failed for line i:= %d with error %d\n",
				i, status);
			return status;
		}
	}
	pr_err("%s: end status=%d\n", __func__, status);
	return status;
}

static int wk2xxx_remove(struct spi_device *spi)
{
	int i;
	struct wk2xxx_chip *chip = dev_get_drvdata(&spi->dev);
	struct wk2xxx_port *s;

	for (i = 0; i < NR_PORTS; i++) {
		s = &chip->wk2xxxs[i];
		uart_remove_one_port(&chip->wk2xxx_uart_driver, &s->port);
	}

	uart_unregister_driver(&chip->wk2xxx_uart_driver);
	return 0;
}

static const struct of_device_id semidrive_spi_wk2xxx_dt_match[] = {
	{ .compatible = "wkmic,wk2124_uart", },
	{ },
};
MODULE_DEVICE_TABLE(of, semidrive_spi_wk2xxx_dt_match);

static struct spi_driver wk2xxx_driver = {
	.driver = {
		.name           = "wk2124_uart",
		.bus            = &spi_bus_type,
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(semidrive_spi_wk2xxx_dt_match),
	},

	.probe          = wk2xxx_probe,
	.remove         = wk2xxx_remove,
};

static int __init wk2xxx_init_spi0(void)
{

	int retval;

	retval = spi_register_driver(&wk2xxx_driver);
	return retval;
}

static void __exit wk2xxx_exit_spi0(void)
{
	return spi_unregister_driver(&wk2xxx_driver);
}

module_init(wk2xxx_init_spi0);
module_exit(wk2xxx_exit_spi0);

MODULE_AUTHOR("WKMIC Ltd");
MODULE_DESCRIPTION("wk2xxx generic serial port driver");
MODULE_LICENSE("GPL");
