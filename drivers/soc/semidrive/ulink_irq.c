/*
 * Copyright (c) 2022, Semidriver Semiconductor
 *
 * This program is free software; you can rediulinkibute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is diulinkibuted in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ulink.h>
#include <linux/workqueue.h>

#define IRQ_MAX_HANDLE 0x100
#define IRQ_INIT_MAGIC 0xDEADBEEF

typedef struct {
    uint32_t irq_num[IRQ_MAX_HANDLE];
    uint32_t irq_tri[IRQ_MAX_HANDLE];
    uint32_t irq_clr[IRQ_MAX_HANDLE];
    uint32_t irq_reg_nums;
    uint32_t irq_magic;
}ulink_irq_status_t;

typedef struct {
    void(*handle)(void *);
    void *data;
} ulink_irq_desc_t;

struct ulink_irq_device {
	struct device			*dev;
	ulink_irq_status_t __iomem	*irq_status_tx;
	ulink_irq_status_t __iomem	*irq_status_rx;
	ulink_irq_desc_t		ulink_irq_desc[IRQ_MAX_HANDLE];
	struct task_struct		*tx_thread;
	struct completion 		tx_event;
	uint32_t			tx;
	uint32_t			rx;
	uint32_t			irq_type;
	bool				tx_state;
	spinlock_t			lock;
};

static struct ulink_irq_device *g_ulink_irq;

static void route_irq(uint32_t irq_num, struct ulink_irq_device *tdev)
{
    if ((irq_num < IRQ_MAX_HANDLE) &&
            (tdev->ulink_irq_desc[irq_num].handle != NULL)) {
        tdev->ulink_irq_desc[irq_num].handle(tdev->ulink_irq_desc[irq_num].data);
    }
}

static irqreturn_t do_rx_irq(int irq, void *data)
{
	struct ulink_irq_device *tdev = (struct ulink_irq_device *)data;
	uint32_t i;

	for (i = 0; i < tdev->irq_status_rx->irq_reg_nums; i++) {
		if (tdev->irq_status_rx->irq_clr[i] != tdev->irq_status_rx->irq_tri[i]) {
			tdev->irq_status_rx->irq_clr[i] = tdev->irq_status_rx->irq_tri[i];
			route_irq(tdev->irq_status_rx->irq_num[i], tdev);
		}
	}
	return IRQ_HANDLED;
}

static uint32_t ulink_irq_gpio_request(struct ulink_irq_device *tdev,
				struct platform_device *pdev)
{
	uint32_t irq_flag;

	tdev->tx = of_get_named_gpio(pdev->dev.of_node, "tx", 0);
	if (!gpio_is_valid(tdev->tx))
		return -ENODEV;
	tdev->rx = of_get_named_gpio(pdev->dev.of_node, "rx", 0);
	if (!gpio_is_valid(tdev->rx))
		return -ENODEV;
	if (of_property_read_u32(pdev->dev.of_node, "irq-type", &tdev->irq_type))
		return -ENOMEM;

	if (devm_gpio_request(&pdev->dev, tdev->tx, "ulink_tx_gpio"))
		return -ENODEV;

	if (devm_gpio_request(&pdev->dev, tdev->rx, "ulink_rx_gpio"))
		return -ENODEV;

	irq_flag = tdev->irq_type? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
	irq_flag |= IRQF_ONESHOT;

	gpio_direction_output(tdev->tx, !tdev->irq_type);
	gpio_direction_input(tdev->rx);

	if (request_threaded_irq(gpio_to_irq(tdev->rx), NULL, do_rx_irq,
			irq_flag, "ulink_rx_irq", tdev))
		return -ENODEV;

	return 0;
}

static bool check_tx_done(struct ulink_irq_device *tdev)
{
	int ret = false;
	int i;

	if (!tdev)
		return true;

	if (tdev->irq_status_tx->irq_magic == IRQ_INIT_MAGIC) {
		for (i = 0; i < tdev->irq_status_tx->irq_reg_nums; i++) {
			if (tdev->irq_status_tx->irq_clr[i] != tdev->irq_status_tx->irq_tri[i])
				break;
		}
		if (i == tdev->irq_status_tx->irq_reg_nums) 
			ret = true;
		else
			ret = false;
	} else {
		ret = true;
	}

	if (ret) {
		if (tdev->tx_state == true) {
			gpio_set_value(tdev->tx, !tdev->irq_type);
			tdev->tx_state = false;
		}
	} else {
		if (tdev->tx_state == false) {
			gpio_set_value(tdev->tx, !!tdev->irq_type);
			tdev->tx_state = true;
		}
	}
	return ret;
}

static int do_tx_irq(void *data)
{
	struct ulink_irq_device *tdev = (struct ulink_irq_device *)data;
	while(1) {
		if (kthread_should_stop())
			break;

		if (check_tx_done(tdev)) {
			wait_for_completion(&tdev->tx_event);
		} else {
			msleep(1);
		}
	}
	return 0;
}

static int ulink_irq_probe(struct platform_device *pdev)
{
	struct ulink_irq_device *tdev;
	struct resource *res;
	void __iomem *addr;
	struct resource *res1;
	void __iomem *addr1;

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	addr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1)
		return -ENOMEM;

	addr1 = devm_ioremap(&pdev->dev, res1->start, resource_size(res1));
	if (IS_ERR(addr1))
		return PTR_ERR(addr1);

	tdev->tx_state = false;
	init_completion(&tdev->tx_event);
	tdev->irq_status_tx = addr;
	tdev->irq_status_rx = addr1;
	if (tdev->irq_status_rx->irq_reg_nums >= 0xff)
		tdev->irq_status_rx->irq_reg_nums = 0;

	tdev->irq_status_rx->irq_magic = IRQ_INIT_MAGIC;

	if (ulink_irq_gpio_request(tdev, pdev))
		return -ENODEV;

	spin_lock_init(&tdev->lock);

	tdev->tx_thread = kthread_run(do_tx_irq, tdev, "uch-tx");
	tdev->dev = &pdev->dev;
	g_ulink_irq = tdev;
	platform_set_drvdata(pdev, tdev);

	return 0;
}

static int ulink_irq_remove(struct platform_device *pdev)
{
	struct ulink_irq_device *tdev = platform_get_drvdata(pdev);

	if (tdev->tx_thread) {
		kthread_stop(tdev->tx_thread);
		complete(&tdev->tx_event);
	}

	return 0;
}

static const struct of_device_id ulink_irq_match[] = {
	{ .compatible = "sd,ulink-irq"},
	{},
};
MODULE_DEVICE_TABLE(of, ulink_irq_match);

static struct platform_driver ulink_irq_driver = {
	.driver = {
		.name = "ulink_irq",
		.of_match_table = ulink_irq_match,
	},
	.probe  = ulink_irq_probe,
	.remove = ulink_irq_remove,
};

int ulink_irq_init(void)
{
	return  platform_driver_register(&ulink_irq_driver);
}

void ulink_irq_exit(void)
{
	platform_driver_unregister(&ulink_irq_driver);
}

void ulink_trigger_irq(uint32_t irq_num)
{
	struct ulink_irq_device *tdev = g_ulink_irq;
	unsigned long flags;
	int i;

	if (!tdev)
		return;

	if (tdev->irq_status_tx->irq_reg_nums == (uint32_t)-1)
		return;

	if (tdev->irq_status_tx->irq_magic != IRQ_INIT_MAGIC)
		return;

	spin_lock_irqsave(&tdev->lock, flags);
	for (i = 0; i < tdev->irq_status_tx->irq_reg_nums; i++) {
		if (irq_num == tdev->irq_status_tx->irq_num[i]) {
			tdev->irq_status_tx->irq_tri[i]++;
			break;
		}
	}
	spin_unlock_irqrestore(&tdev->lock, flags);

	if (i < tdev->irq_status_tx->irq_reg_nums)
		complete(&tdev->tx_event);
}
EXPORT_SYMBOL(ulink_trigger_irq);

void ulink_request_irq(uint32_t irq_num, void(*func)(void *), void *data)
{
	struct ulink_irq_device *tdev = g_ulink_irq;
	uint32_t irq_reg_nums;

	if (!tdev)
		return;
	tdev->ulink_irq_desc[irq_num].handle = func;
	tdev->ulink_irq_desc[irq_num].data = data;
	irq_reg_nums = tdev->irq_status_rx->irq_reg_nums;
	tdev->irq_status_rx->irq_num[irq_reg_nums] = irq_num;
	tdev->irq_status_rx->irq_tri[irq_reg_nums] = 0;
	tdev->irq_status_rx->irq_clr[irq_reg_nums] = 0;
	irq_reg_nums++;
	tdev->irq_status_rx->irq_reg_nums = irq_reg_nums;
}
EXPORT_SYMBOL(ulink_request_irq);
