/*
 * sdrv_regctl.c
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
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/regmap.h>

struct sdrv_regctl_entry {
	unsigned int reg_index;
	void __iomem *reg;
	struct list_head list;
};

struct sdrv_regctl {
	struct device *dev;
	int reg_num;
	struct list_head list;
};


static int sdrv_regctl_reg_write(void *context, unsigned int reg,
		unsigned int val)
{
	struct sdrv_regctl *regctl = context;
	struct sdrv_regctl_entry *entry;
	void __iomem *reg_addr = NULL;
	list_for_each_entry(entry, &regctl->list, list) {
		if (entry->reg_index == reg) {
			reg_addr = entry->reg;
			break;
		}
	}
	if (reg_addr) {
		writel(val, reg_addr);
		return 0;
	}
	return -ENODATA;
}

static int sdrv_regctl_reg_read(void *context, unsigned int reg,
		unsigned int *val)
{
	struct sdrv_regctl *regctl = context;
	struct sdrv_regctl_entry *entry;
	void __iomem *reg_addr = NULL;
	list_for_each_entry(entry, &regctl->list, list) {
		if (entry->reg_index == reg) {
			reg_addr = entry->reg;
			break;
		}
	}
	if (reg_addr) {
		*val = readl(reg_addr);
		return 0;
	}
	return -ENODATA;
}

static int sdrv_regctl_probe(struct platform_device *pdev);

static const struct of_device_id sdrv_regctl_dt_ids[] = {
	{ .compatible = "semidrive,reg-ctl", },
	{ /* sentinel */  },
};

static struct platform_driver sdrv_regctl_driver = {
	.probe =  sdrv_regctl_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table	= sdrv_regctl_dt_ids,
	},
};

static unsigned int regctl_reg_to_index(phys_addr_t reg_base, phys_addr_t reg)
{
	if (reg < reg_base)
		return -1;
	return (reg - reg_base)>>12;
}

static int sdrv_regctl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	void __iomem *reg_addr;
	struct regmap *regmap;
	struct resource res;
	struct regmap_config *regctl_regmap_config;
	int i;
	unsigned int index;
	struct sdrv_regctl *regctl;
	struct sdrv_regctl_entry *entry;

	const __be32 *base;
	phys_addr_t reg_base;
	int prop_size;

	base = of_get_property(dev->of_node, "common_reg_base", &prop_size);
	if (!base) {
		dev_err(dev, "can't get common reset base address\n");
	}
	prop_size /= sizeof(u32);
	reg_base = of_read_number(base, prop_size);

	i = 0;
	while (of_get_address(dev->of_node, i++, NULL, NULL));

	if (i == 1) {
		dev_err(dev, "regctl reg address not specified\n");
		return -ENODATA;
	}
	regctl = devm_kzalloc(dev, sizeof(*regctl), GFP_KERNEL);
	if (!regctl)
		return -ENOMEM;

	regctl->reg_num = i - 1;
	regctl->dev = dev;
	INIT_LIST_HEAD(&regctl->list);

	for (i = 0; i < regctl->reg_num; i++) {
		of_address_to_resource(dev->of_node, i, &res);
		reg_addr = devm_ioremap_resource(dev, &res);

		index = regctl_reg_to_index(reg_base, res.start);

		if (!IS_ERR(reg_addr)) {
			entry = devm_kzalloc(dev, sizeof(*entry), GFP_KERNEL);
			if (!entry)
				return -ENOMEM;
			entry->reg_index = index;
			entry->reg = reg_addr;
			list_add(&entry->list, &regctl->list);
		} else {
			dev_err(dev, "regctl ioremap reg failed %ld\n", PTR_ERR(reg_addr));
		}
	}

	regctl_regmap_config = devm_kzalloc(dev, sizeof(*regctl_regmap_config),
			GFP_KERNEL);
	if (!regctl_regmap_config)
		return -ENOMEM;

	regctl_regmap_config->reg_bits = 32;
	regctl_regmap_config->val_bits = 32;
	regctl_regmap_config->reg_stride = 0;
	regctl_regmap_config->max_register = 8;
	regctl_regmap_config->reg_write = sdrv_regctl_reg_write;
	regctl_regmap_config->reg_read = sdrv_regctl_reg_read;
	regctl_regmap_config->fast_io = true;
	regctl_regmap_config->cache_type = REGCACHE_NONE;

	regmap = devm_regmap_init(dev, NULL, (void*)regctl,
			regctl_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	dev->of_node->data = dev;
	devm_kfree(dev, regctl_regmap_config);
	return 0;
}

static int __init sdrv_regctl_init(void)
{
	int ret;

	ret = platform_driver_register(&sdrv_regctl_driver);
	if (ret) {
		pr_err("register sdrv regctl driver failed\n");
		return ret;
	}

	return 0;
}
subsys_initcall(sdrv_regctl_init);
