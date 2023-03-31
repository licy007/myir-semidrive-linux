/*
 * reset-sdriv.c
 *
 *
 * Copyright(c); 2020 Semidrive
 *
 * Author: Yujin <jin.yu@semidrive.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <asm/io.h>
#include <dt-bindings/reset/sdriv-rstgen.h>
#include <linux/of_address.h>
#include <linux/printk.h>

#define RSTGEN_RST_SIGNAL_MASK  0x1U
#define RSTGEN_RST_EN_MASK 0x1U << 1U
#define RSTGEN_RST_STATUS_MASK 0x1U << 30U
#define RSTGEN_RST_LOCK_MASK 0x1U << 31U

#define RSTGEN_MEM_RANGE 0x1000

struct sdriv_reset_entry {
	unsigned long id;
	void __iomem *rstgen_en;
	void __iomem *rstgen_status;
	struct list_head list;
};

struct sdriv_rstgen {
	struct reset_controller_dev rcdev;
	struct list_head list;
};

static int rstgen_is_module(unsigned long id)
{
	int ret = (id < RSTGEN_CORE_BASE)?1:0;
	return ret;
}

static int rstgen_is_core(unsigned long id)
{
	int ret = (id >= RSTGEN_CORE_BASE)?1:0;
	return ret;
}

static int sdriv_rstgen_assert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	void __iomem *rstgen_en = NULL, *rstgen_status = NULL;
	unsigned int value;
	struct sdriv_rstgen *rstgen = container_of(rcdev, struct sdriv_rstgen, rcdev);
	struct sdriv_reset_entry *reset_entry;
	if (list_empty(&rstgen->list)){
		pr_err("reset is not available.\n");
		return -1;
	}

	list_for_each_entry(reset_entry, &rstgen->list, list) {
		if (reset_entry->id == id) {
			rstgen_en =  reset_entry->rstgen_en;
			rstgen_status = reset_entry->rstgen_status;
		}
	}

	if (rstgen_is_module(id) && rstgen_en) {
		/* Module assert */
		value = readl(rstgen_en);

		/* Check reset lock */
		if (value & RSTGEN_RST_LOCK_MASK)
		{
			pr_err("reset %lu is locked.\n", id);
			return -1;
		}
		/* Check reset status */
		if (!(value & RSTGEN_RST_STATUS_MASK))
		{
			pr_err("reset %lu already in assert status\n", id);
		}

		/* Enable reset configure */
		writel(value | RSTGEN_RST_EN_MASK, rstgen_en);
		/* Polling enable status */
		while(!(readl(rstgen_en) & RSTGEN_RST_EN_MASK));

		/* Assert reset */
		writel(RSTGEN_RST_EN_MASK, rstgen_en);

		/* Polling reset status */
		while((readl(rstgen_en) & RSTGEN_RST_STATUS_MASK));

		return 0;
	}

	if (rstgen_is_core(id) && rstgen_en && rstgen_status) {
		/* Core assert */
		value = readl(rstgen_en);
		/* Check reset lock */
		if (value & RSTGEN_RST_LOCK_MASK)
		{
			pr_err("reset %lu is locked.\n", id + RSTGEN_CORE_BASE);
			return -1;
		}
		/* Check reset status */
		if (!(readl(rstgen_status) & RSTGEN_RST_STATUS_MASK))
		{
			pr_err("reset %lu already in assert status\n", id + RSTGEN_CORE_BASE);
			return -1;
		}
		/* Enable reset configure */
		writel(RSTGEN_RST_SIGNAL_MASK, rstgen_en);
		/* Polling enable status */
		while(!(readl(rstgen_en) & RSTGEN_RST_STATUS_MASK));

		/* Assert reset */
		writel(0, rstgen_status);

		/* Polling reset status */
		while(readl(rstgen_status) & RSTGEN_RST_STATUS_MASK);

		return 0;
	}

	pr_err("failed to assert %lu.\n", id);
	return -1;
}

static int sdriv_rstgen_deassert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	void __iomem *rstgen_en = NULL, *rstgen_status = NULL;
	unsigned int value;
	struct sdriv_rstgen *rstgen = container_of(rcdev, struct sdriv_rstgen, rcdev);
	struct sdriv_reset_entry *reset_entry;
	if (list_empty(&rstgen->list)){
		pr_err("reset is not available.\n");
		return -1;
	}
	list_for_each_entry(reset_entry, &rstgen->list, list) {
		if (reset_entry->id == id) {
			rstgen_en =  reset_entry->rstgen_en;
			rstgen_status = reset_entry->rstgen_status;
		}
	}

	if (rstgen_is_module(id) && rstgen_en) {
		/* Module deassert */
		value = readl(rstgen_en);

		/* Check reset lock */
		if (value & RSTGEN_RST_LOCK_MASK)
		{
			pr_err("reset %lu is locked.\n", id);
			return -1;
		}
		/* Check reset status */
		if ((value & RSTGEN_RST_STATUS_MASK))
		{
			pr_err("reset  %lu already in deassert status\n", id);
		}
		/* Enable reset configure */
		writel(value | RSTGEN_RST_EN_MASK, rstgen_en);
		/* Polling enable status */
		while(!(readl(rstgen_en) & RSTGEN_RST_EN_MASK));
		/* Deassert reset */
		writel(RSTGEN_RST_EN_MASK | RSTGEN_RST_SIGNAL_MASK, rstgen_en);
		/* Polling reset status */
		while(!(readl(rstgen_en) & RSTGEN_RST_STATUS_MASK));

		return 0;
	}
	if (rstgen_is_core(id) && rstgen_en && rstgen_status) {
		/* Core deassert */
		value = readl(rstgen_en);
		/* Check reset lock */
		if (value & RSTGEN_RST_LOCK_MASK)
		{
			pr_err("reset %lu is locked.\n", id + RSTGEN_CORE_BASE);
			return -1;
		}
		/* Check reset status */
		if (readl(rstgen_status) & RSTGEN_RST_STATUS_MASK)
		{
			pr_err("reset %lu already in deassert status\n", id + RSTGEN_CORE_BASE);
			return -1;
		}
		/* Enable reset configure */
		writel(RSTGEN_RST_SIGNAL_MASK, rstgen_en);
		/* Polling enable status */
		while(!(readl(rstgen_en) & RSTGEN_RST_STATUS_MASK));

		/* Deassert reset */
		writel(RSTGEN_RST_SIGNAL_MASK, rstgen_status);
		/* Polling reset status */
		while(!(readl(rstgen_status) & RSTGEN_RST_STATUS_MASK));

		return 0;
	}

	pr_err("failed to deassert %lu.\n", id);
	return -1;
}

/* 0 = reset, 1 = release, -1 unknown */
static int sdriv_rstgen_status(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	void __iomem *rstgen_en = NULL, *rstgen_status = NULL;
	unsigned int value;
	struct sdriv_reset_entry *reset_entry;
	struct sdriv_rstgen *rstgen = container_of(rcdev, struct sdriv_rstgen, rcdev);

	if (list_empty(&rstgen->list)){
		pr_err("reset is not available.\n");
		return -1;
	}
	list_for_each_entry(reset_entry, &rstgen->list, list) {
		if (reset_entry->id == id) {
			rstgen_en =  reset_entry->rstgen_en;
			rstgen_status = reset_entry->rstgen_status;
		}
	}

	if (rstgen_is_module(id) && rstgen_en) {
		/* Module status */
		value = readl(rstgen_en);

		return !!(value & RSTGEN_RST_STATUS_MASK);
	}

	if (rstgen_is_core(id) && rstgen_en && rstgen_status) {
		/* Core status*/

		value = readl(rstgen_status);
		return !!(value & RSTGEN_RST_STATUS_MASK);
	}

	pr_err("failed to get reset %lu status.\n", id);
	return -1;
}

static int sdriv_rstgen_reset(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	void __iomem *rstgen_en = NULL, *rstgen_status = NULL;
	unsigned int value;
	struct sdriv_rstgen *rstgen = container_of(rcdev, struct sdriv_rstgen, rcdev);
	struct sdriv_reset_entry *reset_entry;

	if (list_empty(&rstgen->list)){
		pr_err("reset is not available.\n");
		return -1;
	}
	list_for_each_entry(reset_entry, &rstgen->list, list) {
		if (reset_entry->id == id) {
			rstgen_en =  reset_entry->rstgen_en;
			rstgen_status = reset_entry->rstgen_status;
		}
	}

	if (rstgen_is_module(id) && rstgen_en) {
		/* Module reset */
		value = readl(rstgen_en);

		/* Check reset lock */
		if (value & RSTGEN_RST_LOCK_MASK)
		{
			pr_err("reset %lu is locked.\n", id);
			return -1;
		}

		/* Enalbe reset configure */
		writel(value | RSTGEN_RST_EN_MASK, rstgen_en);
		/* Polling enable status*/
		while(!(readl(rstgen_en) & RSTGEN_RST_EN_MASK));
		/* Assert reset*/
		writel(RSTGEN_RST_EN_MASK, rstgen_en);
		/* Polling reset status */
		while((readl(rstgen_en) & RSTGEN_RST_STATUS_MASK));
		/* Deassert reset */
		writel(RSTGEN_RST_EN_MASK | RSTGEN_RST_SIGNAL_MASK, rstgen_en);
		/* Polling reset status */
		while(!(readl(rstgen_en) & RSTGEN_RST_STATUS_MASK));
		return 0;
	}

	if (rstgen_is_core(id) && rstgen_en && rstgen_status) {
		/* Core self reset */
		value = readl(rstgen_en);
		/* Check lock */
		if (value & RSTGEN_RST_LOCK_MASK)
		{
			pr_err("reset %lu is locked.\n", id + RSTGEN_CORE_BASE);
			return -1;
		}
		/* Enable reset configure */
		writel(RSTGEN_RST_SIGNAL_MASK, rstgen_en);
		while(!(readl(rstgen_en) & RSTGEN_RST_STATUS_MASK));
		/* Trigger auto clear reset */
		writel(RSTGEN_RST_EN_MASK, rstgen_status);
		writel(RSTGEN_RST_SIGNAL_MASK, rstgen_status);
		/* Polling reset status */
		while(!(readl(rstgen_status) & RSTGEN_RST_STATUS_MASK));
		return 0;
	}
	pr_err("failed to reset %lu.\n", id);
	return -1;
}

static const struct reset_control_ops sdriv_reset_ops = {
	.reset = sdriv_rstgen_reset,
	.assert = sdriv_rstgen_assert,
	.deassert = sdriv_rstgen_deassert,
	.status = sdriv_rstgen_status,
};

static int reset_fill_entry(phys_addr_t start, resource_size_t size, unsigned long id,
		struct sdriv_reset_entry *entry)
{
	if (size < RSTGEN_MEM_RANGE || size > 2 * RSTGEN_MEM_RANGE)
		return -1;
	entry->id = id;
	entry->rstgen_en = ioremap(start, size);
	if (!entry->rstgen_en) {
		return -1;
	}
	if (rstgen_is_module(id)) {
		entry->rstgen_status = NULL;
	} else {
		entry->rstgen_status = entry->rstgen_en +  RSTGEN_MEM_RANGE;
	}
	return 0;
}

static phys_addr_t reset_get_base_addr(const struct device_node *np, const char *name)
{
	const __be32 *base;
	int prop_size;
	base = of_get_property(np, name, &prop_size);

	if (!base){
		return 0;
	}
	prop_size /= sizeof(u32);
	return of_read_number(base, prop_size);
}

static unsigned long reset_mem_to_index(phys_addr_t addr, phys_addr_t core, phys_addr_t module)
{
	unsigned long ret = -1;
	if (addr < core)
		return ret;
	if (addr > core && addr < module) {
		ret = (addr - core) >> 13;
		return ret + RSTGEN_CORE_BASE;
	} else {
		ret = (addr - module) >> 12;
		return ret;
	}
}

static int sdriv_reset_probe(struct platform_device *pdev)
{
	struct sdriv_rstgen *rstgen;
	int i, ret, size;
	u32 *res_array, res_nr, reg_nr, id;
	struct sdriv_reset_entry *entry;
	struct resource res;

	struct device *dev = &pdev->dev;

	/* Get rstgen module/core base address */
	phys_addr_t rstgen_core_p = reset_get_base_addr(dev->of_node,
			"rstgen_core_base");
	phys_addr_t rstgen_module_p = reset_get_base_addr(dev->of_node,
			"rstgen_module_base");

	if (!rstgen_core_p || !rstgen_module_p) {
		dev_err(dev, "rstgen base address not specified\n");
		return -ENODATA;
	}

	/* Get rstgen resource from dts */
	if (!of_get_property(dev->of_node, "rstgen_resource", &size)) {
		dev_err(dev, "no rstgen_resource property\n");
		return -ENODATA;
	}

	res_nr = size/sizeof(u32);

	res_array = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!res_array)
		return -ENOMEM;

	/* Index from dts */
	ret = device_property_read_u32_array(dev, "rstgen_resource", res_array, res_nr);
	if (ret) {
		dev_err(dev, "failed to parse reset resource %d\n", ret);
		return ret;
	}

	/* Get rstgen regs from dts */
	i = 0;
	reg_nr = 0;
	while (of_get_address(dev->of_node, i++, NULL, NULL)) {
		reg_nr++;
	}
	if (reg_nr == 0) {
		dev_err(dev, "rstgen module/core address not specified\n");
		return -ENODATA;
	}

	/* Check nums of rstgen regs and resource */
	if (reg_nr != res_nr) {
		dev_err(dev, "regs not match resource, check dts\n");
		return -EINVAL;
	}
	rstgen = devm_kzalloc(dev, sizeof(*rstgen), GFP_KERNEL);

	if (!rstgen)
		return -ENOMEM;
	INIT_LIST_HEAD(&rstgen->list);

	/* Check each rstgen reg */
	for (i = 0; i < reg_nr; i++)
	{
		if (of_address_to_resource(dev->of_node, i, &res)) {
			dev_err(dev, "can't get reset module/core address\n");
			continue;
		}
		/* Index calculated from reg */
		id = reset_mem_to_index(res.start, rstgen_core_p, rstgen_module_p);
		if (id > RSTGEN_MAX)
		{
			dev_err(dev, "invalied reset address\n");
			continue;
		}

		/* Check the index */
		if (id != res_array[i]) {
			dev_err(dev, "index from dts not match the reg addr\n");
			continue;
		}

		entry = devm_kzalloc(dev, sizeof(*entry), GFP_KERNEL);
		if (!entry)
			return -ENOMEM;
		if (reset_fill_entry(res.start, resource_size(&res), id, entry)) {
			dev_err(dev, "failed to fill reset entry %d\n", id);
			devm_kfree(dev, entry);
			continue;
		}
		list_add(&entry->list, &rstgen->list);
	}

	devm_kfree(dev, res_array);

	rstgen->rcdev.owner = THIS_MODULE;
	rstgen->rcdev.nr_resets = RSTGEN_MAX;
	rstgen->rcdev.ops = &sdriv_reset_ops;
	rstgen->rcdev.of_node = dev->of_node;

	return devm_reset_controller_register(dev, &rstgen->rcdev);
}

static const struct of_device_id sdriv_reset_dt_ids[] = {
	{ .compatible = "semidrive,rstgen", },
	{ /* sentinel */  },
};

static struct platform_driver sdriv_reset_driver = {
	.probe =  sdriv_reset_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table	= sdriv_reset_dt_ids,
	},
};

static int __init sdrv_rstgen_init(void)
{
	return platform_driver_register(&sdriv_reset_driver);
}
arch_initcall(sdrv_rstgen_init);
