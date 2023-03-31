/*
 * ARM/ARM64 generic CPU idle driver.
 *
 * Copyright (C) 2014 ARM Ltd.
 * Author: Lorenzo Pieralisi <lorenzo.pieralisi@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "CPUidle arm: " fmt

#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/topology.h>

#include <asm/cpuidle.h>
#include <linux/sched.h>
#include <linux/sched/idle.h>

#include "dt_idle_states.h"

static int poll_idle(struct cpuidle_device *dev, struct cpuidle_driver *drv,
		     int index)
{
	local_irq_enable();
	if (!current_set_polling_and_test()) {
		while (!need_resched())
			cpu_relax();
	}
	current_clr_polling();

	return index;
}

/*
 * arm_enter_idle_state - Programs CPU to enter the specified state
 *
 * dev: cpuidle device
 * drv: cpuidle driver
 * idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int arm_enter_idle_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	if (idx == 0)
		return poll_idle(dev, drv, idx);
	/* currently only support idx <=1 */
	if (idx >= 1) {
		cpu_do_idle();
		return 1;
	}
	//CPU_PM_CPU_IDLE_ENTER(arm_cpuidle_suspend, idx);
	return 1;
}

static struct cpuidle_driver arm_idle_driver __initdata = {
	.name = "arm_idle",
	.owner = THIS_MODULE,
	.states[0] = {
		.enter                  = arm_enter_idle_state,
		.exit_latency           = 0,
		.target_residency       = 0,
		.power_usage            = -1,
		.flags                  = CPUIDLE_FLAG_POLLING,
		.name                   = "POLL",
		.desc                   = "SDRV POLL",
	},

	.states[1] = {
		.enter                  = arm_enter_idle_state,
		.exit_latency           = 20,
		.target_residency       = 20,
		.power_usage            = UINT_MAX/5,
		.name                   = "SDWFI",
		.desc                   = "ARM WFI",
	}
};

static const struct of_device_id arm_idle_state_match[] __initconst = {
	{ .compatible = "sdrv,idle-state",
	  .data = arm_enter_idle_state },
	{ },
};

/*
 * arm_idle_init
 *
 * Registers the arm specific cpuidle driver with the cpuidle
 * framework. It relies on core code to parse the idle states
 * and initialize them using driver data structures accordingly.
 */
static int __init sdrv_idle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

	for_each_possible_cpu(cpu) {

		drv = kmemdup(&arm_idle_driver, sizeof(*drv), GFP_KERNEL);
		if (!drv) {
			ret = -ENOMEM;
			goto out_fail;
		}

		drv->cpumask = (struct cpumask *)cpumask_of(cpu);

		drv->state_count = 2;
		ret = dt_init_idle_driver(drv, arm_idle_state_match, 2);
		if (ret <= 0) {
			// ret = ret ? : -ENODEV;
			// goto out_kfree_drv;
			pr_info("no valid dt idle\n");
		}

		ret = cpuidle_register_driver(drv);
		if (ret) {
			pr_err("Failed to register cpuidle driver\n");
			goto out_kfree_drv;
		}
		if (drv->state_count <= 2)
			goto only_default;
		/*
		 * Call arch CPU operations in order to initialize
		 * idle states suspend back-end specific data
		 */
		ret = arm_cpuidle_init(cpu);

		/*
		 * Skip the cpuidle device initialization if the reported
		 * failure is a HW misconfiguration/breakage (-ENXIO).
		 */
		if (ret == -ENXIO)
			continue;

		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops ret %d\n", cpu, ret);
			goto out_unregister_drv;
		}
only_default:
		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev) {
			pr_err("Failed to allocate cpuidle device\n");
			ret = -ENOMEM;
			goto out_unregister_drv;
		}
		dev->cpu = cpu;

		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("Failed to register cpuidle device for CPU %d\n",
			       cpu);
			goto out_kfree_dev;
		}
	}
	pr_info("register sdrvcpuidle ok\n");
	return 0;

out_kfree_dev:
	kfree(dev);
out_unregister_drv:
	cpuidle_unregister_driver(drv);
out_kfree_drv:
	kfree(drv);
out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		drv = cpuidle_get_cpu_driver(dev);
		cpuidle_unregister_device(dev);
		cpuidle_unregister_driver(drv);
		kfree(dev);
		kfree(drv);
	}

	return ret;
}
device_initcall(sdrv_idle_init);
