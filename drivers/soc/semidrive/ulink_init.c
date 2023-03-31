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
#include <linux/module.h>
#include <linux/soc/semidrive/ulink.h>

static int ulink_init(void)
{
	int ret;

	ret = ulink_pcie_init();
	if (ret)
		return ret;

	ret = ulink_irq_init();
	if (ret)
		return ret;

	ret = ulink_channel_init();
	return ret;
}
device_initcall_sync(ulink_init);

static void ulink_exit(void)
{
	ulink_channel_exit();
	ulink_irq_exit();
	ulink_pcie_exit();
}
module_exit(ulink_exit);

MODULE_DESCRIPTION("ulink driver");
MODULE_AUTHOR("mingmin.ling <mingmin.ling@semidrive.com");
MODULE_LICENSE("GPL v2");
