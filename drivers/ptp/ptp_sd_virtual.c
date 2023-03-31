/*
 * Copyright (c) 2021, Semidriver Semiconductor
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

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/types.h>


/*  sd virtual ptp priv structure */
struct virtual_ptp {
	void __iomem *regs;
	struct ptp_clock *ptp_clk;
	struct ptp_clock_info caps;
	struct device *dev;
	u32 ts_ovf_last;
	u32 ts_wrap_cnt;
	spinlock_t lock;
};

static int virtual_ptp_gettime(struct ptp_clock_info *ptp,
		struct timespec64 *ts)
{
	unsigned long flags;
	struct virtual_ptp *virtual_ptp = container_of(ptp,
			struct virtual_ptp, caps);
	u64 s;
	u64 ns;

	spin_lock_irqsave(&virtual_ptp->lock, flags);

	s = readl(virtual_ptp->regs + 0xB10);
	ns = readl(virtual_ptp->regs + 0xB14);
	if (ns != 0 || s != 0) {
		/* Get the TSSS value */
		ns = readl(virtual_ptp->regs + 0xB0C);
		/* Get the TSS and convert sec time value to nanosecond */
		ns += readl(virtual_ptp->regs + 0xB08) * 1000000000ULL;
	}

	*ts = ns_to_timespec64(ns);

	spin_unlock_irqrestore(&virtual_ptp->lock, flags);



	return 0;
}



static const struct ptp_clock_info virtual_ptp_caps = {
	.owner		= THIS_MODULE,
	.name		= "SD VIRTUAL PTP timer",
	.max_adj	= 50000000,
	.n_ext_ts	= 0,
	.n_pins		= 0,
	.pps		= 0,
	.adjfreq	= NULL,
	.adjtime	= NULL,
	.gettime64	= virtual_ptp_gettime,
	.settime64	= NULL,
	.enable		= NULL,
};

static int virtual_ptp_probe(struct platform_device *pdev)
{
	struct virtual_ptp *virtual_ptp;
	struct device *dev = &pdev->dev;
	struct resource *res;

	virtual_ptp = devm_kzalloc(dev, sizeof(struct virtual_ptp), GFP_KERNEL);
	if (!virtual_ptp)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	virtual_ptp->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(virtual_ptp->regs))
		return PTR_ERR(virtual_ptp->regs);

	spin_lock_init(&virtual_ptp->lock);

	virtual_ptp->dev = dev;
	virtual_ptp->caps = virtual_ptp_caps;
	virtual_ptp->ptp_clk = ptp_clock_register(&virtual_ptp->caps,
			&pdev->dev);
	if (IS_ERR(virtual_ptp->ptp_clk)) {
		dev_err(dev,
			"%s: Failed to register ptp clock\n", __func__);
		return PTR_ERR(virtual_ptp->ptp_clk);
	}

	platform_set_drvdata(pdev, virtual_ptp);

	dev_info(dev, "ptp clk probe done\n");

	return 0;
}

static int virtual_ptp_remove(struct platform_device *pdev)
{
	struct virtual_ptp *virtual_ptp = platform_get_drvdata(pdev);

	ptp_clock_unregister(virtual_ptp->ptp_clk);


	return 0;
}

static const struct of_device_id virtual_ptp_of_match[] = {
	{ .compatible = "semidrive, virtual_ptp", },
	{},
};
MODULE_DEVICE_TABLE(of, virtual_ptp_of_match);

static struct platform_driver ptp_sd_virtual_driver = {
	.driver = {
		.name = "sd_virtual",
		.pm = NULL,
		.of_match_table = virtual_ptp_of_match,
	},
	.probe    = virtual_ptp_probe,
	.remove   = virtual_ptp_remove,
};
module_platform_driver(ptp_sd_virtual_driver);

MODULE_AUTHOR("semidrive");
MODULE_DESCRIPTION("semidrive virtual PTP Clock driver");
MODULE_LICENSE("GPL v2");


