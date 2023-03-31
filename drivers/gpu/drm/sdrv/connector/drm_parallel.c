/*
 * kunlun_drm_parallel.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc_helper.h>
#include <video/of_display_timing.h>

#include "drm_drv.h"
#include "sdrv_dpc.h"

#define KUNLUN_DRM_PARALLEL		"sdrv-drm-parallel"

struct kunlun_drm_parallel {
	struct device *dev;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_display_mode mode;
	struct drm_panel *panel;
	struct drm_bridge *bridge;

	u32 bus_format;
	u32 bus_flags;
	u32 connector_type;
};

#define enc_to_kpar(enc) container_of(enc, struct kunlun_drm_parallel, encoder)
#define con_to_kpar(con) container_of(con, struct kunlun_drm_parallel, connector)

static int kunlun_get_display_info(struct device_node *np,  struct drm_display_info *disp_info, struct drm_display_mode *mode)
{
	// = &connector->display_info;
	int width_mm, height_mm, vrefresh;
	struct device_node *timings_np;
	struct device_node *entry;
	struct device_node *native_mode;

	if (!np)
		return NULL;

	timings_np = of_get_child_by_name(np, "display-timings");
	if (!timings_np) {
		pr_err("%pOF: could not find display-timings node\n", np);
		return NULL;
	}

	entry = of_parse_phandle(timings_np, "native-mode", 0);
	if (!entry) {
		pr_err("%pOF: no timing specifications given\n", np);
		return -1;
	}

	pr_debug("%pOF: using %s as default timing\n", np, entry->name);

	if (of_property_read_u32(entry, "width-mm", &disp_info->width_mm)) {
		DRM_INFO("get width-mm failed\n");
		disp_info->width_mm = 0;
	}

	if (of_property_read_u32(entry, "height-mm", &disp_info->height_mm)) {
		DRM_INFO("get height-mm failed\n");
		disp_info->height_mm = 0;
	}

	if (of_property_read_u32(entry, "vrefresh", &mode->vrefresh)) {
		DRM_INFO("can't get vrefresh\n");
		mode->vrefresh = drm_mode_vrefresh(mode);
	}

	return 0;
}

static int kunlun_drm_parallel_connector_get_modes(
		struct drm_connector *connector)
{
	struct kunlun_drm_parallel *kpar = con_to_kpar(connector);
	struct device_node *np = kpar->dev->of_node;
	struct drm_display_mode *mode;
	int ret, num_modes = 0;

	num_modes = drm_panel_get_modes(kpar->panel);
	if(num_modes > 0)
		return num_modes;

	num_modes = 0;

	if(np) {
		ret = of_get_drm_display_mode(np, &kpar->mode, &kpar->bus_flags,
				OF_USE_NATIVE_MODE);
		if(ret)
			return ret;

		mode = drm_mode_create(connector->dev);
		if(!mode)
			return -ENOMEM;

		drm_mode_copy(mode, &kpar->mode);
		kunlun_get_display_info(np, &connector->display_info, mode);
		mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
		num_modes++;
	}

	return num_modes;
}

static void kunlun_drm_parallel_connector_destroy(
		struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_helper_funcs
kunlun_parallel_connector_helper_funcs = {
	.get_modes = kunlun_drm_parallel_connector_get_modes,
};

static const struct drm_connector_funcs kunlun_parallel_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = kunlun_drm_parallel_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static void kunlun_drm_parallel_encoder_enable(struct drm_encoder *encoder)
{
	struct kunlun_drm_parallel *kpar = enc_to_kpar(encoder);

	drm_panel_prepare(kpar->panel);
	drm_panel_enable(kpar->panel);
}

static void kunlun_drm_parallel_encoder_disable(struct drm_encoder *encoder)
{
	struct kunlun_drm_parallel *kpar = enc_to_kpar(encoder);

	drm_panel_disable(kpar->panel);
	drm_panel_unprepare(kpar->panel);
}

static int kunlun_drm_parallel_atomic_check(struct drm_encoder *encoder,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct kunlun_crtc_state *kunlun_state = to_kunlun_crtc_state(crtc_state);
	struct drm_display_info *di = &conn_state->connector->display_info;
	struct kunlun_drm_parallel *kpar = enc_to_kpar(encoder);

	if(!kpar->bus_format && di->num_bus_formats) {
		kunlun_state->bus_format = di->bus_formats[0];
		kunlun_state->bus_flags = di->bus_flags;
	}	else {
		kunlun_state->bus_format = kpar->bus_format;
		kunlun_state->bus_flags = kpar->bus_flags;
	}

	return 0;
}

static const struct drm_encoder_funcs kunlun_parallel_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs
kunlun_parallel_encoder_helper_funcs = {
	.enable = kunlun_drm_parallel_encoder_enable,
	.disable = kunlun_drm_parallel_encoder_disable,
	.atomic_check = kunlun_drm_parallel_atomic_check,
};

/**
 * drm_crtc_port_mask - find the mask of a registered CRTC by port OF node
 * @dev: DRM device
 * @port: port OF node
 *
 * Given a port OF node, return the possible mask of the corresponding
 * CRTC within a device's list of CRTCs.  Returns zero if not found.
 */
static uint32_t drm_crtc_port_mask(struct drm_device *dev,
				   struct device_node *port)
{
	unsigned int index = 0;
	struct drm_crtc *tmp;

	drm_for_each_crtc(tmp, dev) {
		if (tmp->port == port)
			return 1 << index;

		index++;
	}

	return 0;
}


static uint32_t kunlun_drm_find_possible_crtcs(struct drm_device *dev,
		struct device_node *port)
{
	struct device_node *remote, *ep, *parent, *child;
	uint32_t possible_crtcs = 0;

	for_each_endpoint_of_node(port, ep) {
		remote = of_graph_get_remote_port(ep);
		if(!remote) {
			of_node_put(ep);
			return 0;
		}
		parent = of_get_next_parent(remote);
		for_each_child_of_node(parent, child) {
			possible_crtcs |= drm_crtc_port_mask(dev, child);
		}

		of_node_put(parent);
		of_node_put(remote);
	}

	return possible_crtcs;
}

static int kunlun_drm_parallel_register(struct drm_device *drm,
		struct kunlun_drm_parallel *kpar)
{
	struct drm_encoder *encoder = &kpar->encoder;
	struct drm_connector *connector = &kpar->connector;
	struct device *dev = kpar->dev;
	int ret;

	encoder->possible_crtcs = kunlun_drm_find_possible_crtcs(drm,
			dev->of_node);
	if(!encoder->possible_crtcs)
		return -EPROBE_DEFER;

	drm_encoder_helper_add(encoder, &kunlun_parallel_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &kunlun_parallel_encoder_funcs,
			DRM_MODE_ENCODER_NONE, NULL);
	if(ret) {
		DRM_DEV_ERROR(dev, "Failed to initialize encoder with drm\n");
		return ret;
	}

	if(kpar->bridge) {
		ret = drm_bridge_attach(encoder, kpar->bridge, NULL);
		if(ret < 0) {
			DRM_DEV_ERROR(dev, "Failed to attach bridge\n");
			goto err_encoder_cleanup;
		}
	} else {
		drm_connector_helper_add(connector,
				&kunlun_parallel_connector_helper_funcs);
		drm_connector_init(drm, connector,
				&kunlun_parallel_connector_funcs,
				kpar->connector_type ? kpar->connector_type: DRM_MODE_CONNECTOR_DPI);
		drm_mode_connector_attach_encoder(connector, encoder);
	}

	if(kpar->panel){
		drm_panel_attach(kpar->panel, connector);
	}

	return 0;
err_encoder_cleanup:
	drm_encoder_cleanup(encoder);
	return ret;
}

static int kunlun_drm_parallel_bind(struct device *dev,
		struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct device_node *np = dev->of_node;
	struct kunlun_drm_parallel *kpar;
	const char *fmt;
	int ret;

	kpar = devm_kzalloc(dev, sizeof(*kpar), GFP_KERNEL);
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
	ret = drm_of_find_panel_or_bridge(np, 1, 0,
			&kpar->panel, &kpar->bridge);
	if(ret && ret != -ENODEV) {
		DRM_DEV_ERROR(dev, "Cannot find effective panel or bridge\n");
		return ret;
	}

	kpar->dev = dev;
	ret = kunlun_drm_parallel_register(drm, kpar);
	if(ret)
		return ret;

	dev_set_drvdata(dev, kpar);

	return 0;
}

static void kunlun_drm_parallel_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct kunlun_drm_parallel *kpar = dev_get_drvdata(dev);

	if(kpar->panel)
		drm_panel_detach(kpar->panel);
}

static const struct component_ops kunlun_drm_parallel_ops = {
	.bind = kunlun_drm_parallel_bind,
	.unbind = kunlun_drm_parallel_unbind,
};

static int kunlun_drm_parallel_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &kunlun_drm_parallel_ops);
}

static int kunlun_drm_parallel_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &kunlun_drm_parallel_ops);
	return 0;
}

static const struct of_device_id kunlun_drm_parallel_ids[] = {
	{ .compatible = "semidrive, kunlun-display-parallel", },
	{  },
};
MODULE_DEVICE_TABLE(of, kunlun_drm_parallel_ids);

static struct platform_driver kunlun_drm_parallel_driver = {
	.probe = kunlun_drm_parallel_probe,
	.remove = kunlun_drm_parallel_remove,
	.driver = {
		.of_match_table = kunlun_drm_parallel_ids,
		.name = KUNLUN_DRM_PARALLEL
	},
};
module_platform_driver(kunlun_drm_parallel_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive kunlun drm parallel");
MODULE_LICENSE("GPL");

