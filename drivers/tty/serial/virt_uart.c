#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <linux/tty.h>
#include <linux/termios.h>
#include <linux/of_address.h>
#include <linux/console.h>
#include <asm/io.h>
#include "virt_uart.h"

static struct sdrv_virt_uart_port sdpt;
static struct uart_driver sdrv_virt_uart_driver;

static struct sdrv_virt_uart_port *sdrv_to_vuart_port(struct uart_port *p)
{
	return container_of(p, struct sdrv_virt_uart_port, port);
}

static void sdrv_virt_uart_set_mctrl(struct uart_port *port, unsigned int mctl)
{
	return;
}

static unsigned int sdrv_virt_uart_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS;
}

static unsigned int sdrv_virt_uart_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT; /* always empty */
}

static void sdrv_virt_uart_stop_tx(struct uart_port *port)
{
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);

	atomic_set(&p->tx_flag, 0);
}

/* support only tx for current virtual uart */
static void sdrv_virt_uart_stop_rx(struct uart_port *port)
{
	return;
}

static void sdrv_virt_uart_start_tx(struct uart_port *port)
{
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);
	struct circ_buf *xmit = &port->state->xmit;

	if (uart_tx_stopped(port))
		return;

	if (!uart_circ_empty(xmit)) {
		atomic_set(&p->tx_flag, 1);
		schedule_work(&p->work);
	}
}

static int sdrv_virt_uart_startup(struct uart_port *port)
{
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);

	atomic_set(&p->tx_flag, 1);
	return 0;
}

static void sdrv_virt_uart_shutdown(struct uart_port *port)
{
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);

	atomic_set(&p->tx_flag, 0);
	return;
}

static void sdrv_virt_uart_set_termios(struct uart_port *port,
				   struct ktermios *termios,
				   struct ktermios *old)
{
	return;
}

static int sdrv_virt_uart_verify_port(struct uart_port *port,
				  struct serial_struct *ser)
{
	int ret = 0;

	if (port->type != PORT_SDRV_VUART)
		ret = -EINVAL;

	if (ser->baud_base < 9600)
		ret = -EINVAL;

	return ret;
}

static int sdrv_virt_uart_request_port(struct uart_port *port)
{
	return 0;
}

static void sdrv_virt_uart_release_port(struct uart_port *port)
{
	return;
}

static const char *sdrv_virt_uart_type(struct uart_port *port)
{
	return (port->type == PORT_SDRV_VUART) ? "sdrv_virt_uart" : NULL;
}

static void sdrv_virt_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_SDRV_VUART;
		sdrv_virt_uart_request_port(port);
	}
}

static const struct uart_ops sdrv_virt_uart_ops = {
	.set_mctrl      = sdrv_virt_uart_set_mctrl,
	.get_mctrl      = sdrv_virt_uart_get_mctrl,
	.tx_empty	= sdrv_virt_uart_tx_empty,
	.start_tx	= sdrv_virt_uart_start_tx,
	.stop_tx	= sdrv_virt_uart_stop_tx,
	.stop_rx	= sdrv_virt_uart_stop_rx,
	.startup	= sdrv_virt_uart_startup,
	.shutdown	= sdrv_virt_uart_shutdown,
	.set_termios	= sdrv_virt_uart_set_termios,
	.type		= sdrv_virt_uart_type,
	.config_port	= sdrv_virt_uart_config_port,
	.request_port	= sdrv_virt_uart_request_port,
	.release_port	= sdrv_virt_uart_release_port,
	.verify_port	= sdrv_virt_uart_verify_port,
};

static int sdrv_virt_uart_init_buf(struct uart_port *port)
{
	int ret;
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);

	ret = sdrv_logbuf_init();
	if (ret)
		return ret;

	p->k_hdr = sdrv_logbuf_get_bank_desc(AP2_A55);
	if (!p->k_hdr)
		return -EINVAL;
	p->u_hdr = sdrv_logbuf_get_bank_desc(AP2_USER);
	if (!p->u_hdr)
		return -EINVAL;

	return 0;
}

static int sdrv_virt_uart_early_init_buf(struct uart_port *port)
{
	int ret;
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);

	ret = sdrv_logbuf_early_init();
	if (ret)
		return ret;

	p->k_hdr = sdrv_logbuf_get_bank_desc(AP2_A55);
	if (!p->k_hdr)
		return -EINVAL;

	return 0;
}

static void sdrv_virt_uart_port_write(struct uart_port *port, int ch)
{
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);

	sdrv_logbuf_putc_with_timestamp(p->k_hdr, ch, true);
}

static void sdrv_virt_uart_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = &sdpt.port;
	struct sdrv_virt_uart_port *p = sdrv_to_vuart_port(port);

	uart_console_write(port, s, count, sdrv_virt_uart_port_write);
}

static void sdrv_virt_uart_early_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = &sdpt.port;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	uart_console_write(port, s, count, sdrv_virt_uart_port_write);
	spin_unlock_irqrestore(&port->lock, flags);
}

static int sdrv_virt_uart_console_setup(struct console *co, char *options)
{
	struct uart_port *port = &sdpt.port;

	return sdrv_virt_uart_init_buf(port);
}

static struct console sdrv_virt_uart_console = {
	.name		= SDRV_VUART_TTY_NAME,
	.write		= sdrv_virt_uart_console_write,
	.device		= uart_console_device,
	.setup		= sdrv_virt_uart_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data           = &sdrv_virt_uart_driver,
};

static int __init sdrv_virt_uart_console_init(void)
{
	register_console(&sdrv_virt_uart_console);

	return 0;
}
console_initcall(sdrv_virt_uart_console_init);

static struct uart_driver sdrv_virt_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "sdrv_virt_uart",
	.dev_name	= SDRV_VUART_TTY_NAME,
	.nr		= SDRV_VUART_PORT_NUM,
	.cons		= &sdrv_virt_uart_console,
};

static int __init
sdrv_virt_uart_early_console_setup(struct earlycon_device *device, const char *opt)
{
	struct uart_port *port = &device->port;
	struct device_node *np;
	int ret;

	ret = sdrv_virt_uart_early_init_buf(port);
	if (ret)
		return ret;

	device->con->write = sdrv_virt_uart_early_console_write;

	return 0;
}
OF_EARLYCON_DECLARE(sdrv, "semidrive, virtual-uart", sdrv_virt_uart_early_console_setup);

static void sdrv_virt_uart_work(struct work_struct *work)
{
	struct sdrv_virt_uart_port *p = container_of(work, struct sdrv_virt_uart_port, work);
	struct circ_buf *xmit = &p->port.state->xmit;
	unsigned long flags;
	int count;

again:
	if (p->port.x_char) {
		p->port.icount.tx++;
		p->port.x_char = 0;
		return;
	}

	if (!atomic_read(&p->tx_flag))
		return;

	spin_lock_irqsave(&p->port.lock, flags);
	count = p->port.fifosize;
	while (count--) {
		sdrv_logbuf_putc_with_timestamp(p->u_hdr, xmit->buf[xmit->tail], true);
		xmit->tail = (xmit->tail+1) & (SERIAL_XMIT_SIZE - 1);
		p->port.icount.tx++;

		if (uart_circ_empty(xmit))
			break;
	}
	spin_unlock_irqrestore(&p->port.lock, flags);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&p->port);

	if (!uart_circ_empty(xmit))
		goto again;

	return;
}

static int sdrv_virt_uart_probe(struct platform_device *pdev)
{
	struct sdrv_virt_uart_port *p = &sdpt;
	int ret;

	platform_set_drvdata(pdev, p);
	p->dev = &pdev->dev;

	if (pdev->dev.of_node)
		pdev->id = of_alias_get_id(pdev->dev.of_node, "serial");

	if ((!p->u_hdr) || (!p->k_hdr)) {
		ret = sdrv_virt_uart_init_buf(&p->port);
		if (ret)
			return ret;
	}

	INIT_WORK(&p->work, sdrv_virt_uart_work);

	p->port.iotype = UPIO_MEM32;
	p->port.dev   = &pdev->dev;
	p->port.line  = pdev->id;
	p->port.fifosize = 64;
	p->port.type = PORT_SDRV_VUART;
	p->port.ops   = &sdrv_virt_uart_ops;

	return uart_add_one_port(&sdrv_virt_uart_driver, &p->port);
}

static int sdrv_virt_uart_remove(struct platform_device *pdev)
{
	struct sdrv_virt_uart_port *p;
	p = platform_get_drvdata(pdev);

	uart_remove_one_port(&sdrv_virt_uart_driver, &p->port);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id sdrv_virt_uart_of_match[] = {
	{.compatible = "semidrive, virtual-uart"},
	{}
};
MODULE_DEVICE_TABLE(of, sdrv_virt_uart_of_match);
#endif

static struct platform_driver sdrv_virt_uart_platform_driver = {
	.probe = sdrv_virt_uart_probe,
	.remove = sdrv_virt_uart_remove,
	.driver = {
		.name  = "sdrv_virt_uart",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sdrv_virt_uart_of_match),
#endif
	},
};

static int __init sdrv_virt_uart_init(void)
{
	int ret;

	ret = uart_register_driver(&sdrv_virt_uart_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&sdrv_virt_uart_platform_driver);
	if (ret) {
		uart_unregister_driver(&sdrv_virt_uart_driver);
		return ret;
	}

	return 0;
}

static void __exit sdrv_virt_uart_exit(void)
{
	platform_driver_unregister(&sdrv_virt_uart_platform_driver);
	uart_unregister_driver(&sdrv_virt_uart_driver);
}

module_init(sdrv_virt_uart_init);
module_exit(sdrv_virt_uart_exit);

MODULE_DESCRIPTION("semidrive virtual uart driver");
MODULE_AUTHOR("Xingyu Chen <xingyu.chen@semidrive.com>");
MODULE_AUTHOR("Fuzheng Meng <fuzheng.meng@semidrive.com>");
MODULE_LICENSE("GPL v2");
