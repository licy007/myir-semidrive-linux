/*
 * sdrv_connector.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include "drm_connector.h"
#include "drm_drv.h"
#include "sdrv_dpc.h"

struct para_data {
	u32 bus_format;
	u32 bus_flags;
	u32 connector_type;
};

static int parallel_init(struct sdrv_connector *conn)
{
	struct device_node *np;
	struct para_data *kpar;
	const char *fmt;
	int ret;

	if (!conn->np) {
		return -ENODEV;
	}
	np = conn->np;
	kpar = kzalloc(sizeof(*kpar), GFP_KERNEL);
	if(!kpar)
		return -ENOMEM;
	/* parallel out default RGB888 */
	ret = of_property_read_string(np, "pix-format", &fmt);
	if(!ret) {
		if(!strncmp(fmt, "RGB24", 5) || !strncmp(fmt, "rgb24", 5))
			kpar->bus_format = MEDIA_BUS_FMT_RGB888_1X24;
		if(!strncmp(fmt, "RGB666", 6) || !strncmp(fmt, "rgb666", 6))
			kpar->bus_format = MEDIA_BUS_FMT_RGB666_1X18;
		else if(!strncmp(fmt, "RGB565", 6) || !strncmp(fmt, "rgb565", 6))
			kpar->bus_format = MEDIA_BUS_FMT_RGB565_1X16;
	}
	of_property_read_u32(np, "connector-mode", &kpar->connector_type);
	DRM_INFO("read connector %s connector mode = %d\n", np->name, kpar->connector_type);

	conn->connector_type = kpar->connector_type ? kpar->connector_type : DRM_MODE_CONNECTOR_DPI;
	conn->priv = kpar;

	return 0;
}

static int parallel_uinit(struct sdrv_connector *conn)
{
	struct para_data *kpar = conn->priv;

	kfree(kpar);
	return 0;
}

static int parallel_enable(struct sdrv_connector *conn)
{
	return 0;
}

static int parallel_disable(struct sdrv_connector *conn)
{
	return 0;
}

struct sdrv_conn_ops parallel_ops = {
	.init = parallel_init,
	.uinit = parallel_uinit,
	.enable = parallel_enable,
	.disable = parallel_disable,
};
