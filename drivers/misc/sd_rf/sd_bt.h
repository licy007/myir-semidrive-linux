/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef SEMIDRIVE_BT_H
#define SEMIDRIVE_BT_H

struct sd_bt_data {
	int gpio_bt_en;
	int gps_reset;
	int power_state;
	struct rfkill *rfkill;
	struct platform_device *pdev;
};

#endif

