/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

struct semidrive_vce_device{
	u32 ce_id;
	int irq;
	void __iomem *base;
    void __iomem *sram_base;
};

static int sdrv_vce_probe(struct platform_device *pdev)
{
	int ret;
	//creat rng device
	struct platform_device *rng_pdev;
	struct platform_device *ce_pdev;
    u32 val;
	char* rng_drive_name = "semidrive-rng";
	char* ce_drive_name = "semidrive-ce";
	struct semidrive_vce_device *rng_device;
	struct semidrive_vce_device *ce_device;
	struct resource *ctrl_reg;
	struct resource *sram_base;
	u32 ce_id;
	int irq;
	void __iomem *base_temp;
    void __iomem *sram_base_temp;

	pr_info("sdrv_vce_probe enter");

	val = 0;
    ret = of_property_read_u32(pdev->dev.of_node, "op-mode", &val);

	if (ret < 0) {
  		dev_err(&pdev->dev, "No op-mode optinos configed in dts\n");
  		return ret;
  	}else{

		ctrl_reg = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctrl_reg");
		base_temp = devm_ioremap_resource(&pdev->dev, ctrl_reg);

		if (IS_ERR(base_temp))
			return PTR_ERR(base_temp);

		sram_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram_base");
		sram_base_temp = devm_ioremap_resource(&pdev->dev, sram_base);

		if (IS_ERR(sram_base_temp))
			return PTR_ERR(sram_base_temp);

		of_property_read_u32(pdev->dev.of_node, "ce_id", &ce_id);

		irq = platform_get_irq(pdev, 0);

		//bit 0 define rng function open
		if((val&0x1) == 0x1){
			rng_pdev = platform_device_alloc(rng_drive_name, -1);
			if (!rng_pdev){
				return -ENOMEM;
			}
			rng_device = kzalloc(sizeof(*rng_device), GFP_KERNEL);
  			if (!rng_device){
				  return -ENOMEM;
			}

			//rng_device->np = np;
			rng_device->base = base_temp;

			//copy vce device node to rng device
			platform_set_drvdata(rng_pdev, (void *)rng_device);

			ret = platform_device_add(rng_pdev);
			if (ret){
				platform_device_put(rng_pdev);
			}
		}
		//bit 1 define ce function open
		if((val&0x2) == 0x2){
			ce_pdev = platform_device_alloc(ce_drive_name, -1);
			if (!ce_pdev){
				return -ENOMEM;
			}

			ce_device = kzalloc(sizeof(*ce_device), GFP_KERNEL);
  			if (!ce_device){
				  return -ENOMEM;
			}

			ce_device->ce_id = ce_id;
			ce_device->irq = irq;
			ce_device->base = base_temp;
			ce_device->sram_base = sram_base_temp;

			//copy vce device node to ce device
			platform_set_drvdata(ce_pdev, (void *)ce_device);

			ret = platform_device_add(ce_pdev);
			if (ret){
				platform_device_put(ce_pdev);
			}
		}
	}

	return 0;
}

static const struct of_device_id sdrv_vce_match[] = {
	{ .compatible = "semidrive,vce" },
	{ }
};

MODULE_DEVICE_TABLE(of, sdrv_vce_match);

static struct platform_driver sdrv_vce_driver = {
	.probe		= sdrv_vce_probe,
	.driver		= {
		.name	= "semidrive-vce",
		.of_match_table = of_match_ptr(sdrv_vce_match),
	},
};

module_platform_driver(sdrv_vce_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
MODULE_DESCRIPTION("semidrive vce driver");
