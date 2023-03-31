/*
 * Semidrive hardware semaphore driver
 * Copyright (C) Semidrive Semiconductor Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include "hwspinlock_internal.h"

#define SEMAG_TOKEN(v)      (0x200 + 0x0 + 0x8 * (v))             /* hw semaphore gate */
#define SEMAG_PC(v)         (0x200 + 0x4 + 0x8 * (v))             /* permissin control */

#define SEMAG0_SG0C         0x1                           /* bit0: 0 is unlock, 1 is locked */
#define SEMAG0_SG0LS        (0xff << 16)                  /* lock status, indicate who locks */
#define SEMAG0_MASTERID(v)  (((v) & SEMAG0_SG0LS) >> 16)

/* hwsemaphore number */
#define SD_HWLOCKS_NUM		64

struct sd_hwspinlock_dev {
	void __iomem *base;
	struct hwspinlock_device bank;
};

static int sd_hwspinlock_trylock(struct hwspinlock *lock)
{
	struct sd_hwspinlock_dev *sd_hwlock =
		dev_get_drvdata(lock->bank->dev);
	void __iomem *addr = lock->priv;
	int user_id, lock_id;
	uint32_t semag;

	semag = readl(addr);
	if (!(semag & SEMAG0_SG0C)) {
		writel(1, addr); /* get lock, write lock bit */
		return 1;
	}

	lock_id = hwlock_to_id(lock);

	/* get the master/user id */
	user_id = SEMAG0_MASTERID(semag);
	dev_warn(sd_hwlock->bank.dev,
		 "semaphore [%d] lock failed and master/user id = 0x%x!\n",
		 lock_id, user_id);
	return 0;
}

static void sd_hwspinlock_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	writel(0, lock_addr);
}

/* The specs recommended below number as the retry delay time */
static void sd_hwspinlock_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

static const struct hwspinlock_ops sd_hwspinlock_ops = {
	.trylock = sd_hwspinlock_trylock,
	.unlock  = sd_hwspinlock_unlock,
	.relax   = sd_hwspinlock_relax,
};

static int sd_hwspinlock_probe(struct platform_device *pdev)
{
	struct sd_hwspinlock_dev *sd_hwlock;
	struct hwspinlock *lock;
	struct resource *res;
	int i, ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	sd_hwlock = devm_kzalloc(&pdev->dev,
				   sizeof(struct sd_hwspinlock_dev) +
				   SD_HWLOCKS_NUM * sizeof(*lock),
				   GFP_KERNEL);
	if (!sd_hwlock)
		return -ENOMEM;

	/* shared with mailbox io addr, devm_ioremap_shared_resource is not availalbe */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sd_hwlock->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!sd_hwlock->base) {
		dev_err(&pdev->dev, "ioremap failed for resource %pR\n", res);
		return -EADDRNOTAVAIL;
	}

	for (i = 0; i < SD_HWLOCKS_NUM; i++) {
		lock = &sd_hwlock->bank.lock[i];
		lock->priv = sd_hwlock->base + SEMAG_TOKEN(i);
	}

	platform_set_drvdata(pdev, sd_hwlock);
	pm_runtime_enable(&pdev->dev);

	ret = hwspin_lock_register(&sd_hwlock->bank, &pdev->dev,
				   &sd_hwspinlock_ops, 0, SD_HWLOCKS_NUM);
	if (ret) {
		pm_runtime_disable(&pdev->dev);
		return ret;
	}

	return 0;
}

static int sd_hwspinlock_remove(struct platform_device *pdev)
{
	struct sd_hwspinlock_dev *sd_hwlock = platform_get_drvdata(pdev);

	hwspin_lock_unregister(&sd_hwlock->bank);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id sd_hwspinlock_of_match[] = {
	{ .compatible = "semidrive,hwsemaphore", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sd_hwspinlock_of_match);

static struct platform_driver sd_hwspinlock_driver = {
	.probe  = sd_hwspinlock_probe,
	.remove = sd_hwspinlock_remove,
	.driver = {
		.name = "sd_hwsemaphore",
		.of_match_table = of_match_ptr(sd_hwspinlock_of_match),
	},
};

static int __init sd_hwspinlock_init(void)
{
	return platform_driver_register(&sd_hwspinlock_driver);
}
postcore_initcall(sd_hwspinlock_init);

static void __exit sd_hwspinlock_exit(void)
{
	platform_driver_unregister(&sd_hwspinlock_driver);
}
module_exit(sd_hwspinlock_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware semaphore driver for Semidrive");
MODULE_AUTHOR("bin.wu <bin.wu@semidrive.com>");
