/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/sys_counter.h>

#define SYS_CNT_CNTCVLREAD	(0x0)
#define SYS_CNT_CNTCVUREAD	(0x4)

#define MILLION                 (1000000)

static void __iomem *sys_cntr_base;
static int freq_hz;
static int freq_mhz;

inline uint32_t semidrive_get_syscntr(void)
{
	if (sys_cntr_base)
		return readl(sys_cntr_base);

	return 0;
}
EXPORT_SYMBOL(semidrive_get_syscntr);

inline uint64_t semidrive_get_syscntr_64(void)
{
	uint64_t syscnt = 0;

	if (sys_cntr_base) {
		syscnt = readl(sys_cntr_base);
		syscnt |= ((uint64_t)readl(sys_cntr_base + SYS_CNT_CNTCVUREAD)) << 32;
		return syscnt;
	}

	return 0;
}
EXPORT_SYMBOL(semidrive_get_syscntr_64);

inline uint32_t semidrive_get_syscntr_freq(void)
{
	return freq_hz;
}
EXPORT_SYMBOL(semidrive_get_syscntr_freq);

inline uint64_t semidrive_get_systime_us(void)
{
	if (likely(freq_mhz))
		return (semidrive_get_syscntr_64() / freq_mhz);

	return 0;
}
EXPORT_SYMBOL(semidrive_get_systime_us);

static int semidrive_syscntr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *mem;
	struct device_node *np;
	int freq;

	if (!pdev->dev.of_node)
		return -ENODEV;

	np = pdev->dev.of_node;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sys_cntr_base = devm_ioremap_resource(dev, mem);
	if (IS_ERR(sys_cntr_base)) {
		dev_err(dev, "ioremap failed\n");
		return PTR_ERR(sys_cntr_base);
	}

	if (of_property_read_u32(np, "clock-frequency", &freq)) {
		dev_err(dev, "get clock frequency failed\n");
	} else {
		freq_hz = freq;
		freq_mhz = freq_hz / MILLION;
		dev_info(dev, "sys counter frequency: %d (HZ)\n", freq_hz);
	}

	return 0;
}

static int semidrive_syscntr_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id semidrive_syscntr_of_ids[] = {
	{ .compatible = "semidrive,syscntr"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, semidrive_syscntr_of_ids);

static struct platform_driver semidrive_syscntr_driver = {
	.probe = semidrive_syscntr_probe,
	.remove = semidrive_syscntr_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "semidrive,sys-cntr",
		.of_match_table = semidrive_syscntr_of_ids,
	},
};

static int __init semidrive_syscntr_init(void)
{
	int ret;

	ret = platform_driver_register(&semidrive_syscntr_driver);
	if (ret)
		pr_err("Unable to initialize sys counter\n");
	else
		pr_info("Semidrive sys counter is registered.\n");

	return ret;
}
arch_initcall(semidrive_syscntr_init);

static void __exit semidrive_syscntr_fini(void)
{
	platform_driver_unregister(&semidrive_syscntr_driver);
}
module_exit(semidrive_syscntr_fini);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive System Counter Driver");
MODULE_LICENSE("GPL v2");

