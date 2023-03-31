/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/random.h>

#define RNG_CE_TRNG_VALUE_OFFSET       (0x114<<0)
#define REG_CE_TRNG_CONTROL_OFFSET     (0x8100<<0)
#define TRNG_CONTROL_TRNG_SOFTRESET_FIELD_OFFSET 8

#define TRNG_SOFTRESET_SHIFT                TRNG_CONTROL_TRNG_SOFTRESET_FIELD_OFFSET
#define TRNG_SOFTRESET_MASK                 1UL << TRNG_SOFTRESET_SHIFT

#define to_semidrive_rng(p)    container_of(p, struct semidrive_rng_data, rng)

struct semidrive_rng_data {
	void __iomem *base;
	struct hwrng rng;
};

struct semidrive_vce_device{
	u32 ce_id;
	int irq;
	void __iomem *base;
    void __iomem *sram_base;
};

static uint32_t reg_value(uint32_t val, uint32_t src, uint32_t shift, uint32_t mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

void trng_soft_reset(void __iomem *ce_ctrl_base)
{
	uint32_t read_value, value;

	pr_info("trng_soft_reset enter ");

	read_value = readl(ce_ctrl_base);
	value = reg_value(1, read_value, TRNG_SOFTRESET_SHIFT,
						TRNG_SOFTRESET_MASK);
	writel(value, ce_ctrl_base);

	value = reg_value(0, read_value, TRNG_SOFTRESET_SHIFT,
						TRNG_SOFTRESET_MASK);
	writel(value, ce_ctrl_base);

}

uint32_t rng_get_trng(void __iomem *ce_base, uint8_t* dst, uint32_t size)
{
	uint32_t ret = 0;
	uint32_t rng_value = 0;
	uint32_t i = 0;
	uint32_t index = 0;
	uint32_t index_max = 0;
	uint32_t cp_left = 0;

	index = 0;
	index_max = size>>2; //4 byte one cp
	cp_left = size&0x3;

	for (i = 0; i < index_max; i++) {
			rng_value = readl(ce_base + RNG_CE_TRNG_VALUE_OFFSET);
			memcpy(dst+index, (void *)(&rng_value), sizeof(rng_value));
			index = index+4;
			ret = index;
	}

	if(cp_left > 0){
		rng_value = readl(ce_base + RNG_CE_TRNG_VALUE_OFFSET);
		memcpy(dst+index, (void *)(&rng_value), cp_left);
		ret = index+cp_left;
	}

	return ret;
}

static int semidrive_rng_init(struct hwrng *rng)
{
	pr_info("semidrive_rng_init enter ");
	return 0;
}

static void semidrive_rng_cleanup(struct hwrng *rng)
{
	struct semidrive_rng_data *priv = to_semidrive_rng(rng);

	pr_info("semidrive_rng_cleanup enter ");
	trng_soft_reset(priv->base+REG_CE_TRNG_CONTROL_OFFSET);
}

static int semidrive_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	int ret = 0;
	struct semidrive_rng_data *priv = to_semidrive_rng(rng);

	ret = rng_get_trng(priv->base, buf, max);

	return ret;
}

static int semidrive_rng_probe(struct platform_device *pdev)
{
	struct semidrive_rng_data *rng;
	int ret;
	u32 val;
	struct device_node *np;
	struct semidrive_vce_device *rng_device;

	val = 0;
	np = pdev->dev.of_node;
	rng_device = (struct semidrive_vce_device *)platform_get_drvdata(pdev);
	pr_info("semidrive_rng_probe enter ");

	if (!rng_device)
		return -ENOMEM;

	rng = devm_kzalloc(&pdev->dev, sizeof(*rng), GFP_KERNEL);
	if (!rng)
		return -ENOMEM;

	platform_set_drvdata(pdev, rng);

	rng->base = rng_device->base;

	if (IS_ERR(rng->base))
		return PTR_ERR(rng->base);

	rng->rng.name = pdev->name;
	rng->rng.init = semidrive_rng_init;
	rng->rng.cleanup = semidrive_rng_cleanup;
	rng->rng.read = semidrive_rng_read;
	rng->rng.quality = 1000;

	ret = devm_hwrng_register(&pdev->dev, &rng->rng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id semidrive_rng_match[] = {
	{ .compatible = "semidrive,rngunuse" },
	{ }
};

MODULE_DEVICE_TABLE(of, semidrive_rng_match);

static struct platform_driver semidrive_rng_driver = {
	.probe		= semidrive_rng_probe,
	.driver		= {
		.name	= "semidrive-rng",
		.of_match_table = of_match_ptr(semidrive_rng_match),
	},
};

module_platform_driver(semidrive_rng_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
MODULE_DESCRIPTION("semidrive random number generator driver");
