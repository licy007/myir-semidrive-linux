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
#include "lvds_interface.h"

static int lvds_get_resource(struct lvds_data *klvds,
		struct device_node *np)
{
	struct resource res;
	uint32_t tmp;

	if (of_address_to_resource(np, 0, &res)) {
		DRM_ERROR("parse dt base address failed\n");
		return -ENODEV;
	}
	DRM_INFO("got lvds res 0x%lx\n", (unsigned long)res.start);

	klvds->base = ioremap_nocache(res.start, resource_size(&res));
	if (!klvds->base) {
		DRM_ERROR("Cannot find lvds base regs.\n");
		return -EADDRNOTAVAIL;
	}
	if (!of_property_read_u32(np, "lvds-combine", &tmp))
		klvds->lvds_combine = tmp;
	else
		klvds->lvds_combine = 0;

	if (!of_property_read_u32(np, "lvds-select", &tmp))
		klvds->lvds_select = tmp;
	else
		klvds->lvds_select = 0;

	if (!of_property_read_u32(np, "dual-mode", &tmp))
		klvds->dual_mode = tmp;
	else
		klvds->dual_mode = 0;

	if (!of_property_read_u32(np, "map-format", &tmp))
		klvds->map_format = tmp;
	else
		klvds->map_format = 0;

	if (!of_property_read_u32(np, "pixel-format", &tmp))
		klvds->pixel_format = tmp;
	else
		klvds->pixel_format = 0;

	if (!of_property_read_u32(np, "vsync-pol", &tmp))
		klvds->vsync_pol = tmp;
	else
		klvds->vsync_pol = 0;

	return 0;
}


static int lvds_init(struct sdrv_connector *conn)
{
	struct device_node *np;
	struct lvds_data *klvds;
	int connector_type;
	int ret;

	if (!conn->np) {
		return -ENODEV;
	}
	np = conn->np;
	klvds = kzalloc(sizeof(*klvds), GFP_KERNEL);
	if(!klvds)
		return -ENOMEM;

	ret = lvds_get_resource(klvds, conn->np);
	if (ret) {
		DRM_ERROR("lvds get resouce failed.\n");
		return ret;
	}

	if (of_property_read_u32(np, "connector-mode", &connector_type))
		connector_type = 0;

	DRM_INFO("read connector %s connector mode = %d\n", np->name, connector_type);

	conn->connector_type = connector_type ? connector_type : DRM_MODE_CONNECTOR_LVDS;
	conn->priv = klvds;
	return 0;
}

static int lvds_uinit(struct sdrv_connector *conn)
{
	struct lvds_data *klvds = conn->priv;

	if (klvds->base)
		iounmap(klvds->base);
	kfree(klvds);

	return 0;
}

static int lvds_enable(struct sdrv_connector *conn)
{
	struct lvds_data *klvds = conn->priv;

	lvds_config(klvds);
	return 0;
}

static int lvds_disable(struct sdrv_connector *conn)
{

	return 0;
}

struct sdrv_conn_ops lvds_ops = {
	.init = lvds_init,
	.uinit = lvds_uinit,
	.enable = lvds_enable,
	.disable = lvds_disable,
};
