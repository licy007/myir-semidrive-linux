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

static int sdrv_connector_connector_get_modes(
		struct drm_connector *connector)
{
	struct sdrv_connector *sdrv_conn = con_to_sdrv_conn(connector);
	int num_modes = 0;

	num_modes = drm_panel_get_modes(sdrv_conn->panel);
	if(num_modes > 0)
		return num_modes;

	return num_modes;
}

static void sdrv_connector_connector_destroy(
		struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_helper_funcs
sdrv_connector_helper_funcs = {
	.get_modes = sdrv_connector_connector_get_modes,
};

static const struct drm_connector_funcs sdrv_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = sdrv_connector_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int sdrv_connector_init(struct sdrv_connector *sdrv_conn)
{
	int ret;
	if (!sdrv_conn->ops)
		return -EINVAL;

	ret = sdrv_conn->ops->init(sdrv_conn);
	if (ret) {
		DRM_ERROR("connector get resouce failed.\n");
		return ret;
	}

	DRM_INFO("sdrv_connector_init init ok\n");
	return 0;
}

static int sdrv_connector_uninit(struct sdrv_connector *sdrv_conn)
{
	if (!sdrv_conn)
		return -1;

	return 0;
}

static void sdrv_connector_encoder_enable(struct drm_encoder *encoder)
{
	struct sdrv_connector *sdrv_conn = enc_to_sdrv_conn(encoder);

	if (sdrv_conn->panel) {
		drm_panel_prepare(sdrv_conn->panel);
		drm_panel_enable(sdrv_conn->panel);
	}

	if (sdrv_conn->ops->enable)
		sdrv_conn->ops->enable(sdrv_conn);
}

static void sdrv_connector_encoder_disable(struct drm_encoder *encoder)
{
	struct sdrv_connector *sdrv_conn = enc_to_sdrv_conn(encoder);

	DRM_INFO("%s()\n", __func__);

	if (sdrv_conn->panel) {
		drm_panel_disable(sdrv_conn->panel);
		drm_panel_unprepare(sdrv_conn->panel);
	}

	if (sdrv_conn->ops->disable)
		sdrv_conn->ops->disable(sdrv_conn);

	sdrv_connector_uninit(sdrv_conn);
}

static void sdrv_dpc_pllclk_div(struct drm_crtc_state *crtc_state, struct sdrv_connector *sdrv_conn)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc_state->crtc);
	struct lvds_data *klvds;
	struct sdrv_dpc *dpc;

	if (!kcrtc || !sdrv_conn) {
		DRM_ERROR("crtc or lvds is null\n");
		return;
	}

	dpc = kcrtc->master;
	klvds = (struct lvds_data *)sdrv_conn->priv;

	while(dpc && dpc->dpc_type != DPC_TYPE_DC)
		dpc = dpc->next;

	if(dpc == NULL)
		return;

	if(klvds->dual_mode == 1)
		dpc->pclk_div = 1;

	DRM_DEBUG_ATOMIC("%s dpc:%s pclk_div:%d\n", __func__, dpc->name, dpc->pclk_div);
}


static int sdrv_conn_atomic_check(struct drm_encoder *encoder,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{

	struct sdrv_connector *sdrv_conn = enc_to_sdrv_conn(encoder);
	if (sdrv_conn->ops->atomic_check)
		sdrv_conn->ops->atomic_check(sdrv_conn);

	if (sdrv_conn->name) {
		if (strstr(sdrv_conn->name, "lvds")) {
			sdrv_dpc_pllclk_div(crtc_state, sdrv_conn);
		}
	}

	return 0;
}
static const struct drm_encoder_funcs sdrv_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs
sdrv_encoder_helper_funcs = {
	.enable = sdrv_connector_encoder_enable,
	.disable = sdrv_connector_encoder_disable,
	.atomic_check = sdrv_conn_atomic_check,
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

static uint32_t sdrv_drm_find_possible_crtcs(struct drm_device *dev,
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

static int sdrv_connector_register(struct drm_device *drm,
		struct sdrv_connector *sdrv_conn)
{
	struct drm_encoder *encoder = &sdrv_conn->encoder;
	struct drm_connector *connector = &sdrv_conn->connector;
	int ret;

	encoder->possible_crtcs = sdrv_drm_find_possible_crtcs(drm,
			sdrv_conn->np);
	if(!encoder->possible_crtcs)
		return -EPROBE_DEFER;

	drm_encoder_helper_add(encoder, &sdrv_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &sdrv_encoder_funcs,
			DRM_MODE_ENCODER_NONE, NULL);
	if(ret) {
		DRM_ERROR("Failed to initialize encoder with drm\n");
		return ret;
	}

	if(sdrv_conn->bridge) {
		ret = drm_bridge_attach(encoder, sdrv_conn->bridge, NULL);
		if(ret < 0) {
			DRM_ERROR("Failed to attach bridge\n");
			goto err_encoder_cleanup;
		}
	} else {
		drm_connector_helper_add(connector,
				&sdrv_connector_helper_funcs);
		drm_connector_init(drm, connector,
				&sdrv_connector_funcs,
				sdrv_conn->connector_type);
		drm_mode_connector_attach_encoder(connector, encoder);
	}

	if(sdrv_conn->panel){
		drm_panel_attach(sdrv_conn->panel, connector);
	}

	return 0;
err_encoder_cleanup:
	drm_encoder_cleanup(encoder);
	return ret;
}

static int sdrv_connector_bind(struct device *dev,
		struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct device_node *np = dev->of_node;
	struct sdrv_connector *sdrv_conn;
	const struct connector_type *match_data;
	int ret;

	match_data = of_device_get_match_data(dev);
	if (!match_data) {
		DRM_ERROR("sdrv_conn get match data failed\n");
		return -ENODEV;
	}
	DRM_INFO("connector type %s\n", match_data->name);

	sdrv_conn = devm_kzalloc(dev, sizeof(*sdrv_conn), GFP_KERNEL);
	if(!sdrv_conn)
		return -ENOMEM;

	sdrv_conn->np = np;
	sdrv_conn->ops = match_data->ops;
	sdrv_conn->name = match_data->name;

	if (!sdrv_conn->ops) {
		DRM_ERROR("sdrv_conn get ops failed.\n");
		return -EINVAL;
	}
	if (sdrv_connector_init(sdrv_conn))
		return -ENODEV;
	ret = drm_of_find_panel_or_bridge(np, 1, 0,
			&sdrv_conn->panel, &sdrv_conn->bridge);
	if(ret && ret != -ENODEV) {
		DRM_DEV_ERROR(dev, "Cannot find effective panel or bridge\n");
		return ret;
	}

	ret = sdrv_connector_register(drm, sdrv_conn);
	if(ret)
		return ret;

	dev_set_drvdata(dev, sdrv_conn);

	return 0;
}

static void sdrv_connector_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct sdrv_connector *sdrv_conn = dev_get_drvdata(dev);

	if(sdrv_conn->panel)
		drm_panel_detach(sdrv_conn->panel);
}

static const struct component_ops sdrv_connector_ops = {
	.bind = sdrv_connector_bind,
	.unbind = sdrv_connector_unbind,
};

static int sdrv_connector_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &sdrv_connector_ops);
}

static int sdrv_connector_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &sdrv_connector_ops);
	return 0;
}

struct connector_type sdrv_conn_types[] = {
	{
		.name = "display lvds",
		.ops = &lvds_ops,
	},
	{
		.name = "display parallel",
		.ops = &parallel_ops,
	},
};

static const struct of_device_id sdrv_connector_ids[] = {
	{ .compatible = "semidrive, kunlun-display-lvds", &sdrv_conn_types[0]},
	{ .compatible = "semidrive, kunlun-display-parallel", &sdrv_conn_types[1]},
	{  },
};
MODULE_DEVICE_TABLE(of, sdrv_connector_ids);

static struct platform_driver sdrv_connector_driver = {
	.probe = sdrv_connector_probe,
	.remove = sdrv_connector_remove,
	.driver = {
		.of_match_table = sdrv_connector_ids,
		.name = "semidrive drm connector",
	},
};
module_platform_driver(sdrv_connector_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive kunlun drm lvds");
MODULE_LICENSE("GPL");

