/*
 * sdrv_connector.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __LVDS_INTERFACE_H
#define __LVDS_INTERFACE_H

struct lvds_data {
	void __iomem *base;
	uint8_t lvds_select;
	uint8_t map_format;
	uint8_t pixel_format;
	uint8_t lvds_combine;
	uint8_t dual_mode;
	uint8_t vsync_pol;
};


int lvds_config(struct lvds_data *lvds);

#endif

