/*
 * sdrv_connector.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef __CONNECTOR_DRM_H
#define __CONNECTOR_DRM_H
#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc_helper.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <video/of_display_timing.h>


struct sdrv_connector;
struct sdrv_conn_ops {
	int (*init)(struct sdrv_connector *conn);
	int (*uinit)(struct sdrv_connector *conn);
	int (*enable)(struct sdrv_connector *conn);
	int (*disable)(struct sdrv_connector *conn);
	int (*atomic_check)(struct sdrv_connector *conn);
};

struct sdrv_connector {
	const char *name;
	struct device_node *np;
	u32 connector_type;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct sdrv_conn_ops *ops;
	void *priv;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_display_mode mode;
};

struct connector_type {
	const char name[64];
	struct sdrv_conn_ops *ops;
};

#define enc_to_sdrv_conn(enc) container_of(enc, struct sdrv_connector, encoder)
#define con_to_sdrv_conn(con) container_of(con, struct sdrv_connector, connector)

extern struct sdrv_conn_ops parallel_ops;
extern struct sdrv_conn_ops lvds_ops;
#endif //__CONNECTOR_DRM_H