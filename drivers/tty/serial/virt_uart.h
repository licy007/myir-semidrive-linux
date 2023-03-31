#ifndef _VIRT_UART_H_
#define _VIRT_UART_H_
#include <linux/serial_core.h>
#include <linux/soc/semidrive/logbuf.h>

#define PORT_SDRV_VUART			(160)

#define SDRV_VUART_TTY_NAME		"ttyV"
#define SDRV_VUART_PORT_NUM		2

struct sdrv_virt_uart_port {
	struct uart_port port;
	struct work_struct work;
	struct device *dev;
	logbuf_bk_desc_t  *k_hdr;
	logbuf_bk_desc_t *u_hdr;
	atomic_t tx_flag;
};

#endif /* _VIRT_UART_H_ */
