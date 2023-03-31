/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/acpi.h>
#include <linux/gpio/driver.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "gpiolib.h"

/* port operation registers */
#define GPIO_DIR_PORT_1		0x2000
#define GPIO_DIR_PORT_2		0x2010
#define GPIO_DIR_PORT_SIZE \
	(GPIO_DIR_PORT_2 - GPIO_DIR_PORT_1)
#define GPIO_DIR_PORT_X(n) \
	(GPIO_DIR_PORT_1 + ((n) * GPIO_DIR_PORT_SIZE))

#define GPIO_DATA_IN_PORT_1		0x2200
#define GPIO_DATA_IN_PORT_2		0x2210
#define GPIO_DATA_IN_PORT_SIZE \
	(GPIO_DATA_IN_PORT_2 - GPIO_DATA_IN_PORT_1)
#define GPIO_DATA_IN_PORT_X(n) \
	(GPIO_DATA_IN_PORT_1 + ((n) * GPIO_DATA_IN_PORT_SIZE))

#define GPIO_DATA_OUT_PORT_1		0x2400
#define GPIO_DATA_OUT_PORT_2		0x2410
#define GPIO_DATA_OUT_PORT_SIZE \
	(GPIO_DATA_OUT_PORT_2 - GPIO_DATA_OUT_PORT_1)
#define GPIO_DATA_OUT_PORT_X(n) \
	(GPIO_DATA_OUT_PORT_1 + ((n) * GPIO_DATA_OUT_PORT_SIZE))

#define GPIO_INT_EN_PORT_1		0x2600
#define GPIO_INT_EN_PORT_2		0x2610
#define GPIO_INT_EN_PORT_SIZE \
	(GPIO_INT_EN_PORT_2 - GPIO_INT_EN_PORT_1)
#define GPIO_INT_EN_PORT_X(n) \
	(GPIO_INT_EN_PORT_1 + ((n) * GPIO_INT_EN_PORT_SIZE))

#define GPIO_INT_MASK_PORT_1		0x2800
#define GPIO_INT_MASK_PORT_2		0x2810
#define GPIO_INT_MASK_PORT_SIZE \
	(GPIO_INT_MASK_PORT_2 - GPIO_INT_MASK_PORT_1)
#define GPIO_INT_MASK_PORT_X(n) \
	(GPIO_INT_MASK_PORT_1 + ((n) * GPIO_INT_MASK_PORT_SIZE))

#define GPIO_INT_TYPE_PORT_1		0x2a00
#define GPIO_INT_TYPE_PORT_2		0x2a10
#define GPIO_INT_TYPE_PORT_SIZE \
	(GPIO_INT_TYPE_PORT_2 - GPIO_INT_TYPE_PORT_1)
#define GPIO_INT_TYPE_PORT_X(n) \
	(GPIO_INT_TYPE_PORT_1 + ((n) * GPIO_INT_TYPE_PORT_SIZE))

#define GPIO_INT_POL_PORT_1		0x2c00
#define GPIO_INT_POL_PORT_2		0x2c10
#define GPIO_INT_POL_PORT_SIZE \
	(GPIO_INT_POL_PORT_2 - GPIO_INT_POL_PORT_1)
#define GPIO_INT_POL_PORT_X(n) \
	(GPIO_INT_POL_PORT_1 + ((n) * GPIO_INT_POL_PORT_SIZE))

#define GPIO_INT_BOTH_EDGE_PORT_1		0x2e00
#define GPIO_INT_BOTH_EDGE_PORT_2		0x2e10
#define GPIO_INT_BOTH_EDGE_PORT_SIZE \
	(GPIO_INT_BOTH_EDGE_PORT_2 - GPIO_INT_BOTH_EDGE_PORT_1)
#define GPIO_INT_BOTH_EDGE_PORT_X(n) \
	(GPIO_INT_BOTH_EDGE_PORT_1 + ((n) * GPIO_INT_BOTH_EDGE_PORT_SIZE))

#define GPIO_INT_STATUS_PORT_1		0x3000
#define GPIO_INT_STATUS_PORT_2		0x3010
#define GPIO_INT_STATUS_PORT_SIZE \
	(GPIO_INT_STATUS_PORT_2 - GPIO_INT_STATUS_PORT_1)
#define GPIO_INT_STATUS_PORT_X(n) \
	(GPIO_INT_STATUS_PORT_1 + ((n) * GPIO_INT_STATUS_PORT_SIZE))

#define GPIO_INT_STATUS_UNMASK_PORT_1		0x3200
#define GPIO_INT_STATUS_UNMASK_PORT_2		0x3210
#define GPIO_INT_STATUS_UNMASK_PORT_SIZE \
	(GPIO_INT_STATUS_UNMASK_PORT_2 - GPIO_INT_STATUS_UNMASK_PORT_1)
#define GPIO_INT_STATUS_UNMASK_PORT_X(n) \
	(GPIO_INT_STATUS_UNMASK_PORT_1 + \
	((n) * GPIO_INT_STATUS_UNMASK_PORT_SIZE))

#define GPIO_INT_EDGE_CLR_PORT_1		0x3400
#define GPIO_INT_EDGE_CLR_PORT_2		0x3410
#define GPIO_INT_EDGE_CLR_PORT_SIZE \
	(GPIO_INT_EDGE_CLR_PORT_2 - GPIO_INT_EDGE_CLR_PORT_1)
#define GPIO_INT_EDGE_CLR_PORT_X(n) \
	(GPIO_INT_EDGE_CLR_PORT_1 + ((n) * GPIO_INT_EDGE_CLR_PORT_SIZE))

#define GPIO_INT_LEV_SYNC_PORT_1		0x3600
#define GPIO_INT_LEV_SYNC_PORT_2		0x3610
#define GPIO_INT_LEV_SYNC_PORT_SIZE \
	(GPIO_INT_LEV_SYNC_PORT_2 - GPIO_INT_LEV_SYNC_PORT_1)
#define GPIO_INT_LEV_SYNC_PORT_X(n) \
	(GPIO_INT_LEV_SYNC_PORT_1 + ((n) * GPIO_INT_LEV_SYNC_PORT_SIZE))

#define GPIO_INT_DEB_EN_PORT_1		0x3800
#define GPIO_INT_DEB_EN_PORT_2		0x3810
#define GPIO_INT_DEB_EN_PORT_SIZE \
	(GPIO_INT_DEB_EN_PORT_2 - GPIO_INT_DEB_EN_PORT_1)
#define GPIO_INT_DEB_EN_PORT_X(n) \
	(GPIO_INT_DEB_EN_PORT_1 + ((n) * GPIO_INT_DEB_EN_PORT_SIZE))

#define GPIO_INT_CLK_SEL_PORT_1		0x3a00
#define GPIO_INT_CLK_SEL_PORT_2		0x3a10
#define GPIO_INT_CLK_SEL_PORT_SIZE \
	(GPIO_INT_CLK_SEL_PORT_2 - GPIO_INT_CLK_SEL_PORT_1)
#define GPIO_INT_CLK_SEL_PORT_X(n) \
	(GPIO_INT_CLK_SEL_PORT_1 + ((n) * GPIO_INT_CLK_SEL_PORT_SIZE))

#define GPIO_INT_PCLK_ACTIVE_PORT_1		0x3c00
#define GPIO_INT_PCLK_ACTIVE_PORT_2		0x3c10
#define GPIO_INT_PCLK_ACTIVE_PORT_SIZE \
	(GPIO_INT_PCLK_ACTIVE_PORT_2 - GPIO_INT_PCLK_ACTIVE_PORT_1)
#define GPIO_INT_PCLK_ACTIVE_PORT_X(n) \
	(GPIO_INT_PCLK_ACTIVE_PORT_1 + ((n) * GPIO_INT_PCLK_ACTIVE_PORT_SIZE))

#define SEMIDRIVE_MAX_PORTS		6

struct sdrv_port_property {
	struct fwnode_handle *fwnode;
	unsigned int	idx;
	unsigned int	ngpio;
	unsigned int	gpio_base;
	unsigned int	irq;
	bool		irq_shared;
	unsigned int	gpio_ranges[4];
};

struct sdrv_gpio;

#ifdef CONFIG_PM_SLEEP
/* Store GPIO context across system-wide suspend/resume transitions */
struct sdrv_context {
	u32 data;
	u32 dir;
	u32 ext;
	u32 int_en;
	u32 int_mask;
	u32 int_type;
	u32 int_pol;
	u32 int_edg;
	u32 int_deb;
};
#endif

struct sdrv_gpio_port {
	struct gpio_chip	gc;
	bool			is_registered;
	struct sdrv_gpio	*gpio;
#ifdef CONFIG_PM_SLEEP
	struct sdrv_context	*ctx;
#endif
	unsigned int		idx;
	struct irq_domain	*domain;  /* each port has one domain */
};

struct sdrv_gpio {
	struct	device		*dev;
	void __iomem		*regs;
	struct sdrv_gpio_port	*ports;
	unsigned int		nr_ports;
	raw_spinlock_t		lock;
};

static inline u32 sdrv_read(struct sdrv_gpio *gpio, unsigned int offset)
{
	void __iomem *reg_base	= gpio->regs;
	return readl(reg_base + offset);
}

static inline void sdrv_write(struct sdrv_gpio *gpio, unsigned int offset,
			       u32 val)
{
	void __iomem *reg_base	= gpio->regs;
	writel(val, reg_base + offset);
}

static int sdrv_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
	struct sdrv_gpio_port *port = gpiochip_get_data(gc);
	int irq;

	irq = irq_find_mapping(port->domain, offset);

	return irq;
}

static u32 sdrv_do_irq(struct sdrv_gpio_port *port)
{
	unsigned int port_idx = 0;
	int bit = 0;
	unsigned long irq_status = 0;
	unsigned long clear_int, reg_mask_read, ret = 1;
	int gpio_irq;
	struct sdrv_gpio *gpio = port->gpio;

	port_idx = port->idx;

	irq_status = sdrv_read(gpio, GPIO_INT_STATUS_PORT_X(port_idx));
	reg_mask_read = sdrv_read(gpio, GPIO_INT_MASK_PORT_X(port_idx));

	/* mask the port INT */
	/* can not mask here, irq_gc_mask_set_bit will do */
	//sdrv_write(gpio,  GPIO_INT_MASK_PORT_X(port_idx), 0xffffffff);

	for_each_set_bit(bit, &irq_status, 32) {
		gpio_irq = irq_find_mapping(port->domain, bit);
		clear_int = sdrv_read(gpio, GPIO_INT_EDGE_CLR_PORT_X(port_idx));
		/* clear INT */
		sdrv_write(gpio, GPIO_INT_EDGE_CLR_PORT_X(port_idx), clear_int | BIT(bit));
		/* Invoke interrupt handler */
		if (generic_handle_irq(gpio_irq))
			ret = 0;
	}

	/* restore irq mask */
	/* can not unmask here, irq_gc_mask_clr_bit will do */
	//sdrv_write(gpio,  GPIO_INT_MASK_PORT_X(port_idx), reg_mask_read);

	return ret;
}

static void sdrv_irq_handler(struct irq_desc *desc)
{
	struct sdrv_gpio_port *port = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	sdrv_do_irq(port);
	if (chip->irq_eoi)
		chip->irq_eoi(irq_desc_get_irq_data(desc));
}

static void sdrv_irq_enable(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	unsigned long flags;
	u32 val;
	unsigned int reg_idx;
	unsigned int bit_idx;

	reg_idx = port->idx;
	bit_idx = d->hwirq;


	raw_spin_lock_irqsave(&gpio->lock, flags);

	/* unmask */
	val = sdrv_read(gpio, GPIO_INT_MASK_PORT_X(reg_idx));
	val &= ~BIT(bit_idx);
	sdrv_write(gpio,  GPIO_INT_MASK_PORT_X(reg_idx), val);

	/* INT EN */
	val = sdrv_read(gpio, GPIO_INT_EN_PORT_X(reg_idx));
	val |= BIT(bit_idx);
	sdrv_write(gpio,  GPIO_INT_EN_PORT_X(reg_idx), val);

	raw_spin_unlock_irqrestore(&gpio->lock, flags);
}

static void sdrv_irq_disable(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	unsigned long flags;
	u32 val;
	unsigned int reg_idx = 0;
	unsigned int bit_idx;

	reg_idx = port->idx;
	bit_idx = d->hwirq;


	raw_spin_lock_irqsave(&gpio->lock, flags);
	val = sdrv_read(gpio, GPIO_INT_EN_PORT_X(reg_idx));
	val &= ~BIT(bit_idx);
	sdrv_write(gpio, GPIO_INT_EN_PORT_X(reg_idx), val);
	raw_spin_unlock_irqrestore(&gpio->lock, flags);
}

static int sdrv_irq_reqres(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	struct gpio_chip *gc;

	gc = &port->gc;

	if (gpiochip_lock_as_irq(gc, irqd_to_hwirq(d))) {
		dev_err(gpio->dev, "unable to lock HW IRQ %lu for IRQ\n",
			irqd_to_hwirq(d));
		return -EINVAL;
	}
	return 0;
}

static void sdrv_irq_relres(struct irq_data *d)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct gpio_chip *gc;

	gc = &port->gc;

	gpiochip_unlock_as_irq(gc, irqd_to_hwirq(d));
}

static int sdrv_irq_set_type(struct irq_data *d, u32 type)
{
	struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
	struct sdrv_gpio_port *port = igc->private;
	struct sdrv_gpio *gpio = port->gpio;
	unsigned long flags;
	unsigned int reg_idx = 0, val;
	unsigned int bit_idx;

	reg_idx = port->idx;

	if (type & ~(IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING |
		     IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_BOTH))
		return -EINVAL;

	raw_spin_lock_irqsave(&gpio->lock, flags);

	bit_idx = d->hwirq;

	switch (type) {
	case IRQ_TYPE_EDGE_BOTH:
		/* EDGE_BOTH */
		val = sdrv_read(gpio, GPIO_INT_TYPE_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_TYPE_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_POL_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_POL_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx));
		val |= BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx), val);
		break;

	case IRQ_TYPE_EDGE_RISING:
		/* EDGE + HIGH active */
		val = sdrv_read(gpio, GPIO_INT_TYPE_PORT_X(reg_idx));
		val |= BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_TYPE_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_POL_PORT_X(reg_idx));
		val |= BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_POL_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx), val);
		break;

	case IRQ_TYPE_EDGE_FALLING:
		/* EDGE + LOW active */
		val = sdrv_read(gpio, GPIO_INT_TYPE_PORT_X(reg_idx));
		val |= BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_TYPE_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_POL_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_POL_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx), val);
		break;

	case IRQ_TYPE_LEVEL_HIGH:
		/* LEVEL + HIGH active */
		val = sdrv_read(gpio, GPIO_INT_TYPE_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_TYPE_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_POL_PORT_X(reg_idx));
		val |= BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_POL_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx), val);
		break;

	case IRQ_TYPE_LEVEL_LOW:
		/* LEVEL + LOW active */
		val = sdrv_read(gpio, GPIO_INT_TYPE_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_TYPE_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_POL_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_POL_PORT_X(reg_idx), val);
		val = sdrv_read(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx));
		val &= ~BIT(bit_idx);
		sdrv_write(gpio, GPIO_INT_BOTH_EDGE_PORT_X(reg_idx), val);
		break;
	}

	irq_setup_alt_chip(d, type);

	raw_spin_unlock_irqrestore(&gpio->lock, flags);

	return 0;
}

static int sdrv_gpio_set_debounce(struct gpio_chip *gc,
				   unsigned offset, unsigned debounce)
{
	struct sdrv_gpio_port *port = gpiochip_get_data(gc);
	struct sdrv_gpio *gpio = port->gpio;
	unsigned long flags, val_deb, mask;
	unsigned int reg_idx = port->idx;

	raw_spin_lock_irqsave(&gpio->lock, flags);

	mask = BIT(offset);

	val_deb = sdrv_read(gpio, GPIO_INT_DEB_EN_PORT_X(reg_idx));
	if (debounce)
		sdrv_write(gpio,
			GPIO_INT_DEB_EN_PORT_X(reg_idx), val_deb | mask);
	else
		sdrv_write(gpio,
			GPIO_INT_DEB_EN_PORT_X(reg_idx), val_deb & ~mask);

	raw_spin_unlock_irqrestore(&gpio->lock, flags);

	return 0;
}

static int sdrv_gpio_set_config(struct gpio_chip *gc, unsigned offset,
				 unsigned long config)
{
	u32 debounce;

	if (pinconf_to_config_param(config) != PIN_CONFIG_INPUT_DEBOUNCE) {
		return -ENOTSUPP;
	}

	debounce = pinconf_to_config_argument(config);
	return sdrv_gpio_set_debounce(gc, offset, debounce);
}

static irqreturn_t sdrv_irq_handler_mfd(int irq, void *dev_id)
{
	u32 worked = 0;
	struct sdrv_gpio_port *port = dev_id;

	worked = sdrv_do_irq(port);

	return worked ? IRQ_HANDLED : IRQ_NONE;
}

static void sdrv_configure_irqs(struct sdrv_gpio *gpio,
				 struct sdrv_gpio_port *port,
				 struct sdrv_port_property *pp)
{
	struct gpio_chip *gc = &port->gc;
	struct fwnode_handle  *fwnode = pp->fwnode;
	struct irq_chip_generic	*irq_gc = NULL;
	unsigned int hwirq, ngpio = gc->ngpio;
	struct irq_chip_type *ct;
	int err, i;

	port->domain = irq_domain_create_linear(fwnode, ngpio,
						 &irq_generic_chip_ops, gpio);
	if (!port->domain)
		return;

	err = irq_alloc_domain_generic_chips(port->domain, ngpio, 2,
					     "gpio-sdrv", handle_level_irq,
					     IRQ_NOREQUEST, 0,
					     IRQ_GC_INIT_NESTED_LOCK);
	if (err) {
		dev_err(gpio->dev, "irq_alloc_domain_generic_chips failed\n");
		irq_domain_remove(port->domain);
		port->domain = NULL;
		return;
	}

	irq_gc = irq_get_domain_generic_chip(port->domain, 0);
	if (!irq_gc) {
		irq_domain_remove(port->domain);
		port->domain = NULL;
		return;
	}

	irq_gc->reg_base = gpio->regs;
	irq_gc->private = port;

	for (i = 0; i < 2; i++) {
		ct = &irq_gc->chip_types[i];
		ct->chip.irq_ack = irq_gc_ack_set_bit;
		ct->chip.irq_mask = irq_gc_mask_set_bit;
		ct->chip.irq_unmask = irq_gc_mask_clr_bit;
		ct->chip.irq_set_type = sdrv_irq_set_type;
		ct->chip.irq_enable = sdrv_irq_enable;
		ct->chip.irq_disable = sdrv_irq_disable;
		ct->chip.irq_request_resources = sdrv_irq_reqres;
		ct->chip.irq_release_resources = sdrv_irq_relres;
		ct->chip.flags |= IRQCHIP_SKIP_SET_WAKE;

		ct->regs.ack = GPIO_INT_EDGE_CLR_PORT_X(port->idx);
		ct->regs.mask = GPIO_INT_MASK_PORT_X(port->idx);
		ct->type = IRQ_TYPE_LEVEL_MASK;
	}

	irq_gc->chip_types[0].type = IRQ_TYPE_LEVEL_MASK;
	irq_gc->chip_types[1].type = IRQ_TYPE_EDGE_BOTH;
	irq_gc->chip_types[1].handler = handle_edge_irq;

	if (!pp->irq_shared) {
		dev_info(gpio->dev, "sdrv_gpio irq Not shared...\n");
		irq_set_chained_handler_and_data(pp->irq,
			sdrv_irq_handler, port);
	} else {
		/*
		 * Request a shared IRQ since where MFD would have devices
		 * using the same irq pin
		 */
		dev_info(gpio->dev, "sdrv_gpio irq shared...\n");
		err = devm_request_irq(gpio->dev, pp->irq,
			sdrv_irq_handler_mfd,
			IRQF_SHARED, "gpio-sdrv-mfd", port);
		if (err) {
			dev_err(gpio->dev, "error requesting IRQ\n");
			irq_domain_remove(port->domain);
			port->domain = NULL;
			return;
		}
	}

	for (hwirq = 0 ; hwirq < ngpio ; hwirq++)
		irq_create_mapping(port->domain, hwirq);

	port->gc.to_irq = sdrv_gpio_to_irq;
}

static void sdrv_irq_teardown(struct sdrv_gpio *gpio)
{
	struct sdrv_gpio_port *port = &gpio->ports[0];
	struct gpio_chip *gc = &port->gc;
	unsigned int ngpio = gc->ngpio;
	irq_hw_number_t hwirq;
	int idx = 0;


	for (idx = 0; idx < gpio->nr_ports; idx++) {
		dev_info(gpio->dev, "port idx[%d] removed!\n", idx);
		port = &gpio->ports[idx];
		gc = &port->gc;
		ngpio = gc->ngpio;

		if (!port->domain)
			return;

		for (hwirq = 0 ; hwirq < ngpio ; hwirq++)
			irq_dispose_mapping(
				irq_find_mapping(port->domain, hwirq));

		irq_domain_remove(port->domain);
		port->domain = NULL;
	}

}

static int sdrv_gpio_add_port(struct sdrv_gpio *gpio,
			       struct sdrv_port_property *pp,
			       unsigned int offs)
{
	struct sdrv_gpio_port *port;
	void __iomem *dat, *set, *dirout;
	int err;

	port = &gpio->ports[offs];
	port->gpio = gpio;
	port->idx = pp->idx;

#ifdef CONFIG_PM_SLEEP
	port->ctx = devm_kzalloc(gpio->dev, sizeof(*port->ctx), GFP_KERNEL);
	if (!port->ctx)
		return -ENOMEM;
#endif


	/* using dts "reg" for Register Port idx. */

	/* data: registers reflects the signals value on port */
	dat = gpio->regs + GPIO_DATA_IN_PORT_X(pp->idx);

	/* set: values written to this register are output on port */
	set = gpio->regs + GPIO_DATA_OUT_PORT_X(pp->idx);

	/* Data Direction */
	dirout = gpio->regs + GPIO_DIR_PORT_X(pp->idx);

	err = bgpio_init(&port->gc, gpio->dev, 4, dat, set, NULL, dirout,
			 NULL, BGPIOF_READ_OUTPUT_REG_SET);
	if (err) {
		dev_err(gpio->dev, "failed to init gpio chip for port%d\n",
			port->idx);
		return err;
	}

#ifdef CONFIG_OF_GPIO
	port->gc.of_node = to_of_node(pp->fwnode);
#endif
	port->gc.ngpio = pp->ngpio;
	port->gc.base = pp->gpio_base;

	port->gc.set_config = sdrv_gpio_set_config;

	if (pp->irq)
		sdrv_configure_irqs(gpio, port, pp);

	err = gpiochip_add_data(&port->gc, port);
	if (err)
		dev_err(gpio->dev, "failed to register gpiochip for port%d\n",
			port->idx);
	else
		port->is_registered = true;

	return err;
}

static void sdrv_gpio_unregister(struct sdrv_gpio *gpio)
{
	unsigned int m;

	for (m = 0; m < gpio->nr_ports; ++m)
		if (gpio->ports[m].is_registered)
			gpiochip_remove(&gpio->ports[m].gc);
}

static struct sdrv_port_property *
sdrv_gpio_get_pdata(struct device *dev, struct sdrv_gpio *gpio)
{
	struct fwnode_handle *fwnode;
	struct sdrv_port_property *properties;
	struct sdrv_port_property *pp;
	int nports;
	int i;

	nports = device_get_child_node_count(dev);
	if (nports == 0)
		return ERR_PTR(-ENODEV);

	properties = devm_kcalloc(dev, nports, sizeof(*pp), GFP_KERNEL);
	if (!properties)
		return ERR_PTR(-ENOMEM);

	gpio->nr_ports = nports;

	i = 0;
	device_for_each_child_node(dev, fwnode) {
		pp = &properties[i];
		pp->fwnode = fwnode;

		if (fwnode_property_read_u32(fwnode, "reg", &pp->idx) ||
		    pp->idx >= SEMIDRIVE_MAX_PORTS) {
			dev_err(dev,
				"missing/invalid port index for port%d\n", i);
			fwnode_handle_put(fwnode);
			return ERR_PTR(-EINVAL);
		}

		if (fwnode_property_read_u32_array(fwnode, "gpio-ranges",
			pp->gpio_ranges, ARRAY_SIZE(pp->gpio_ranges))) {
			dev_err(dev,
				 "failed to get gpio-ranges for port%d\n", i);
			fwnode_handle_put(fwnode);
			return ERR_PTR(-EINVAL);
		} else {
			dev_info(dev,
				 "Got gpio-ranges[%d, %d, %d, %d] for port%d\n",
				pp->gpio_ranges[0], pp->gpio_ranges[1],
				pp->gpio_ranges[2], pp->gpio_ranges[3], i);
		}

		pp->ngpio = pp->gpio_ranges[3];

		if (fwnode_property_read_bool(fwnode,
						  "interrupt-controller")) {
			pp->irq = irq_of_parse_and_map(to_of_node(fwnode), 0);
			if (!pp->irq)
				dev_warn(dev, "no irq for port%d\n", pp->idx);
			dev_info(dev, "sdrv_gpio: irq [%d]\n", pp->irq);
		}

		pp->irq_shared	= false;
		pp->gpio_base	= pp->gpio_ranges[2];

		/*
		 * dts "reg" is regsiter port index(0~4),
		 */
		i++;
	}

	return properties;
}

static const struct of_device_id sdrv_of_match[] = {
	{ .compatible = "semidrive,sdrv-gpio",},
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, sdrv_of_match);


static int sdrv_gpio_probe(struct platform_device *pdev)
{
	unsigned int i;
	struct resource *res;
	struct sdrv_gpio *gpio;
	int err;
	struct device *dev = &pdev->dev;
	struct sdrv_port_property *properties;

	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	raw_spin_lock_init(&gpio->lock);

	properties = sdrv_gpio_get_pdata(dev, gpio);
	if (!properties) {
		dev_err(dev, "sdrv_gpio sdrv_gpio_get_pdata error!\n");
		return PTR_ERR(properties);
	}


	if (!gpio->nr_ports) {
		dev_err(dev, "sdrv_gpio No ports found!\n");
		return -ENODEV;
	}

	gpio->dev = &pdev->dev;

	gpio->ports = devm_kcalloc(&pdev->dev, gpio->nr_ports,
				   sizeof(*gpio->ports), GFP_KERNEL);
	if (!gpio->ports)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gpio->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gpio->regs)) {
		dev_err(dev, "sdrv_gpio regs devm_ioremap_resource failed !\n");
		return PTR_ERR(gpio->regs);
	}

	for (i = 0; i < gpio->nr_ports; i++) {
		err = sdrv_gpio_add_port(gpio, &properties[i], i);
		if (err) {
			dev_err(dev, "sdrv_gpio sdrv_gpio_add_port failed !\n");
			goto out_unregister;
		}
	}
	platform_set_drvdata(pdev, gpio);

	return 0;

out_unregister:
	sdrv_gpio_unregister(gpio);
	sdrv_irq_teardown(gpio);

	return err;
}

static int sdrv_gpio_remove(struct platform_device *pdev)
{
	struct sdrv_gpio *gpio = platform_get_drvdata(pdev);

	sdrv_gpio_unregister(gpio);
	sdrv_irq_teardown(gpio);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sdrv_gpio_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdrv_gpio *gpio = platform_get_drvdata(pdev);
	unsigned long flags;
	int i;

	raw_spin_lock_irqsave(&gpio->lock, flags);
	for (i = 0; i < gpio->nr_ports; i++) {
		unsigned int offset;
		unsigned int idx = gpio->ports[i].idx;
		struct sdrv_context *ctx = gpio->ports[i].ctx;

		BUG_ON(!ctx);

		offset = GPIO_DIR_PORT_X(idx);
		ctx->dir = sdrv_read(gpio, offset);

		offset = GPIO_DATA_IN_PORT_X(idx);
		ctx->data = sdrv_read(gpio, offset);

		offset = GPIO_DATA_OUT_PORT_X(idx);
		ctx->ext = sdrv_read(gpio, offset);

		/* interrupts */
		ctx->int_mask = sdrv_read(gpio, GPIO_INT_MASK_PORT_X(idx));
		ctx->int_pol = sdrv_read(gpio, GPIO_INT_POL_PORT_X(idx));
		ctx->int_type = sdrv_read(gpio, GPIO_INT_TYPE_PORT_X(idx));
		ctx->int_edg = sdrv_read(gpio, GPIO_INT_BOTH_EDGE_PORT_X(idx));
		ctx->int_deb = sdrv_read(gpio, GPIO_INT_DEB_EN_PORT_X(idx));

		/* Mask out interrupts */
		sdrv_write(gpio, GPIO_INT_MASK_PORT_X(idx), 0xffffffff);

	}
	raw_spin_unlock_irqrestore(&gpio->lock, flags);

	return 0;
}

static int sdrv_gpio_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdrv_gpio *gpio = platform_get_drvdata(pdev);
	unsigned long flags;
	int i;

	raw_spin_lock_irqsave(&gpio->lock, flags);
	for (i = 0; i < gpio->nr_ports; i++) {
		unsigned int offset;
		unsigned int idx = gpio->ports[i].idx;
		struct sdrv_context *ctx = gpio->ports[i].ctx;

		BUG_ON(!ctx);

		offset = GPIO_DATA_IN_PORT_X(idx);
		sdrv_write(gpio, offset, ctx->data);

		offset = GPIO_DIR_PORT_X(idx);
		sdrv_write(gpio, offset, ctx->dir);

		offset = GPIO_DATA_OUT_PORT_X(idx);
		sdrv_write(gpio, offset, ctx->ext);

		/* interrupts */
		sdrv_write(gpio, GPIO_INT_TYPE_PORT_X(idx), ctx->int_type);
		sdrv_write(gpio, GPIO_INT_POL_PORT_X(idx), ctx->int_pol);
		sdrv_write(gpio, GPIO_INT_BOTH_EDGE_PORT_X(idx), ctx->int_edg);
		sdrv_write(gpio, GPIO_INT_DEB_EN_PORT_X(idx), ctx->int_deb);
		sdrv_write(gpio, GPIO_INT_MASK_PORT_X(idx), ctx->int_mask);

		/* Clear out spurious interrupts */
		sdrv_write(gpio, GPIO_INT_EDGE_CLR_PORT_X(idx), 0xffffffff);

	}
	raw_spin_unlock_irqrestore(&gpio->lock, flags);

	return 0;
}
#endif

static struct dev_pm_ops sdrv_gpio_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS (sdrv_gpio_suspend,
			sdrv_gpio_resume)
};

static struct platform_driver sdrv_gpio_driver = {
	.driver		= {
		.name	= "semidrive,sdrv-gpio",
		.pm	= &sdrv_gpio_pm_ops,
		.of_match_table = of_match_ptr(sdrv_of_match),
	},
	.probe		= sdrv_gpio_probe,
	.remove		= sdrv_gpio_remove,
};

static int __init sdrv_gpio_init(void)
{
	return platform_driver_register(&sdrv_gpio_driver);
}

//module_platform_driver(sdrv_gpio_driver);
static void __exit sdrv_gpio_exit(void)
{
	return platform_driver_unregister(&sdrv_gpio_driver);
}

subsys_initcall(sdrv_gpio_init);
module_exit(sdrv_gpio_exit);

#ifdef CONFIG_ARCH_SEMIDRIVE_V9
static const struct of_device_id sdrv_of_match_sideb[] = {
	{.compatible = "semidrive,sdrv-gpio-sideb"},
	{},
};

static struct platform_driver sdrv_gpio_driver_sideb = {
	.probe = sdrv_gpio_probe,
	.remove = sdrv_gpio_remove,
	.driver = {
		   .name = "semidrive,sdrv-gpio-sideb",
		   .of_match_table = sdrv_of_match_sideb,
		   },
};

static int __init sdrv_gpio_sideb_init(void)
{
	int ret;

	ret = platform_driver_register(&sdrv_gpio_driver_sideb);
	if (ret < 0) {
		printk("fail to register sideb gpio driver ret = %d.\n", ret);
	}
	return ret;
}

static void __exit sdrv_gpio_sideb_exit(void)
{
	return platform_driver_unregister(&sdrv_gpio_driver_sideb);
}

late_initcall(sdrv_gpio_sideb_init);
module_exit(sdrv_gpio_sideb_exit);
#endif


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Semidrive");
MODULE_DESCRIPTION("Semidrive GPIO driver");
