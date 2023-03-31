/*
 * kunlun_drm_lvds.c
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
#include <linux/of_address.h>
#include <video/of_display_timing.h>

#include "drm_drv.h"
#include "sdrv_dpc.h"
#include "drm_lvds_reg.h"

#define KUNLUN_DRM_LVDS		"sdrv-drm-lvds"

typedef enum {
    PIXEL_FORMAT_18_BIT = 0b00,
    PIXEL_FORMAT_24_BIT = 0b01,
    PIXEL_FORMAT_30_BIT = 0b10
} LVDS_PIXEL_FORMAT;

typedef enum {
    MAP_FORMAT_JEIDA = 0b0,
    MAP_FORMAT_SWPG = 0b01
} LVDS_MAP_FORMAT;

enum {
	DUALLODD_ODD = 0b0,
	DUALLODD_EVEN = 0b1
};

enum {
	SRC_SEL_7_CLOCK   = 0b0,
	SRC_SEL_3P5_CLOCK = 0b1
};

enum {
	LVDS_CLK_EN 	 = 0b00001,
	LVDS_CLK_X7_EN	 = 0b00010,
	LVDS_CLK_X3P5_EN = 0b00100,
	LVDS_DBG_CLK_EN  = 0b01000,
	LVDS_DSP_CLK_EN  = 0b10000
};

enum {
	LVDS1 = 1,
	LVDS2,
	LVDS3,
	LVDS4,
	LVDS1_LVDS2,
	LVDS3_LVDS4,
	LVDS1_4
};

enum LVDS_CHANNEL {
	LVDS_CH1 = 0,
	LVDS_CH2,
	LVDS_CH3,
	LVDS_CH4,
	LVDS_CH_MAX_NUM
};

struct lvds_chx_info {
    int ch_idx;
    int ch_mode;
    int pixel_fmt;
    int map_format;
    int vsync_pol;
};

struct lvds_context {
	void __iomem *base;
	uint8_t lvds_select;
	uint8_t map_format;
	uint8_t pixel_format;
	uint8_t lvds_combine;
	uint8_t dual_mode;
	uint8_t vsync_pol;
};

struct kunlun_drm_lvds {
	struct device *dev;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_display_mode mode;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct lvds_context ctx;
};

#define enc_to_klvds(enc) container_of(enc, struct kunlun_drm_lvds, encoder)
#define con_to_klvds(con) container_of(con, struct kunlun_drm_lvds, connector)

static inline void
reg_writel(void __iomem *base, uint32_t offset, uint32_t val)
{
	writel(val, base + offset);
	/*pr_info("val = 0x%08x, read:0x%08x = 0x%08x\n",
			val, base + offset, readl(base+offset));*/
}

static inline uint32_t reg_readl(void __iomem *base, uint32_t offset)
{
	return readl(base + offset);
}

static inline uint32_t reg_value(unsigned int val,
	unsigned int src, unsigned int shift, unsigned int mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

static int kunlun_drm_lvds_connector_get_modes(
		struct drm_connector *connector)
{
	struct kunlun_drm_lvds *klvds = con_to_klvds(connector);
	struct device_node *np = klvds->dev->of_node;
	struct drm_display_mode *mode;
	int ret, num_modes = 0;

	num_modes = drm_panel_get_modes(klvds->panel);
	if(num_modes > 0)
		return num_modes;

	num_modes = 0;

	if(np) {
		ret = of_get_drm_display_mode(np, &klvds->mode, NULL,
			OF_USE_NATIVE_MODE);
		if(ret)
			return ret;

		mode = drm_mode_create(connector->dev);
		if(!mode)
			return -ENOMEM;

		drm_mode_copy(mode, &klvds->mode);
		mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
		num_modes++;
	}

	return num_modes;
}

static void kunlun_drm_lvds_connector_destroy(
		struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_helper_funcs
kunlun_lvds_connector_helper_funcs = {
	.get_modes = kunlun_drm_lvds_connector_get_modes,
};

static const struct drm_connector_funcs kunlun_lvds_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = kunlun_drm_lvds_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static void lvds_channel_config(struct lvds_context *ctx, uint8_t chidx)
{
	int val, mux, i;

	val = reg_readl(ctx->base, LVDS_CHN_CTRL_(chidx));
	val = reg_value(ctx->dual_mode, val, CHN_DUALMODE_SHIFT, CHN_DUALMODE_MASK);
	val = reg_value(ctx->pixel_format, val, CHN_BPP_SHIFT, CHN_BPP_MASK);
	val = reg_value(ctx->map_format, val, CHN_FORMAT_SHIFT, CHN_FORMAT_MASK);
	val = reg_value(ctx->vsync_pol, val,
		CHN_VSYNC_POL_SHIFT, CHN_VSYNC_POL_MASK);

	if ((ctx->pixel_format == PIXEL_FORMAT_30_BIT) &&
		ctx->dual_mode) {
		if ((chidx == LVDS_CH1) || (chidx == LVDS_CH2))
			val = reg_value(DUALLODD_ODD, val,
				CHN_DUALODD_SHIFT, CHN_DUALODD_MASK);
		else
			val = reg_value(DUALLODD_EVEN, val,
				CHN_DUALODD_SHIFT, CHN_DUALODD_MASK);
	}

	if (ctx->pixel_format == PIXEL_FORMAT_30_BIT) {
		if ((chidx == LVDS_CH1) || (chidx == LVDS_CH3))
			mux = 0;
		else
			mux = 1;
	} else {
		if (ctx->dual_mode) {
			if ((chidx == LVDS_CH1) || (chidx == LVDS_CH3))
				mux = 0;
			else
				mux = 1;
		} else {
			if ((chidx == LVDS_CH1) || (chidx == LVDS_CH3))
				mux = 0;
			else
				mux = 2;
		}
	}

	val = reg_value(mux, val, CHN_MUX_SHIFT, CHN_MUX_MASK);

	val = reg_value(1, val, CHN_EN_SHIFT, CHN_EN_MASK);
	reg_writel(ctx->base, LVDS_CHN_CTRL_(chidx), val);

	/*config 5 lanes base vdd*/
	for (i = 0; i < 5; i++) {
		reg_writel(ctx->base, LVDS_CHN_PAD_SET_(chidx) + 0x4 * i, 0x9CF);
	}
}

static int kunlun_lvds_init(struct kunlun_drm_lvds *klvds)
{
	int val;
	struct lvds_context *ctx = &klvds->ctx;

	val = reg_readl(ctx->base, LVDS_BIAS_SET);
	val = reg_value(1, val, BIAS_EN_SHIFT, BIAS_EN_MASK);
	reg_writel(ctx->base, LVDS_BIAS_SET, val);

	if (ctx->pixel_format == PIXEL_FORMAT_30_BIT) {
		if (!ctx->dual_mode) {
			switch (ctx->lvds_select) {
			case LVDS1_LVDS2:
				lvds_channel_config(ctx, LVDS_CH1);
				lvds_channel_config(ctx, LVDS_CH2);
				break;
			case LVDS3_LVDS4:
				lvds_channel_config(ctx, LVDS_CH3);
				lvds_channel_config(ctx, LVDS_CH4);
				break;
			default:
				DRM_ERROR("%s() error select for 30bit separate mode.\n",
					__func__);
				return -EINVAL;
			}
		} else {
			if (ctx->lvds_select == LVDS1_4) {
				lvds_channel_config(ctx, LVDS_CH1);
				lvds_channel_config(ctx, LVDS_CH2);
				lvds_channel_config(ctx, LVDS_CH3);
				lvds_channel_config(ctx, LVDS_CH4);
			} else {
				DRM_ERROR("%s() error select for 30bit dual mode.\n",
					__func__);
				return -EINVAL;
			}
		}
	}else {
		if (!ctx->dual_mode) {
			switch (ctx->lvds_select) {
			case LVDS1:
				lvds_channel_config(ctx, LVDS_CH1);
				break;
			case LVDS2:
				lvds_channel_config(ctx, LVDS_CH2);
				break;
			case LVDS3:
				lvds_channel_config(ctx, LVDS_CH3);
				break;
			case LVDS4:
				lvds_channel_config(ctx, LVDS_CH4);
				break;
			default:
				DRM_ERROR("%s() error select for separate mode.\n",
					__func__);
				return -EINVAL;
			}
		}else {
			switch (ctx->lvds_select) {
			case LVDS1_LVDS2:
				lvds_channel_config(ctx, LVDS_CH1);
				lvds_channel_config(ctx, LVDS_CH2);
				break;
			case LVDS3_LVDS4:
				lvds_channel_config(ctx, LVDS_CH3);
				lvds_channel_config(ctx, LVDS_CH4);
				break;
			case LVDS1_4:
				val = reg_readl(ctx->base, LVDS_COMBINE);
				DDBG(klvds->mode.hdisplay);
				val = reg_value(1920 - 1, val, CMB_VLD_HEIGHT_SHIFT, CMB_VLD_HEIGHT_MASK);
				reg_writel(ctx->base, LVDS_COMBINE, val);

				lvds_channel_config(ctx, LVDS_CH1);
				lvds_channel_config(ctx, LVDS_CH2);
				lvds_channel_config(ctx, LVDS_CH3);
				lvds_channel_config(ctx, LVDS_CH4);
				break;
			default:
				DRM_ERROR("%s() error select for dual mode :%d\n",
					__func__, ctx->lvds_select);
				return -EINVAL;
			}
		}
	}

	DRM_INFO("lvds init ok\n");
	return 0;
}

static int kunlun_lvds_uninit(struct kunlun_drm_lvds *klvds)
{
	if (!klvds)
		return -1;

	return 0;
}

static void kunlun_drm_lvds_encoder_enable(struct drm_encoder *encoder)
{
	struct kunlun_drm_lvds *klvds = enc_to_klvds(encoder);

	kunlun_lvds_init(klvds);

	if (klvds->panel) {
		drm_panel_prepare(klvds->panel);
		drm_panel_enable(klvds->panel);
	}
}

static void kunlun_drm_lvds_encoder_disable(struct drm_encoder *encoder)
{
	struct kunlun_drm_lvds *klvds = enc_to_klvds(encoder);

	DRM_INFO("%s()\n", __func__);

	if (klvds->panel) {
		drm_panel_disable(klvds->panel);
		drm_panel_unprepare(klvds->panel);
	}

	kunlun_lvds_uninit(klvds);
}

static void kunlun_update_dpc_pllclk_div(struct drm_crtc_state *crtc_state, struct drm_encoder *encoder)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc_state->crtc);
	struct kunlun_drm_lvds *klvds = enc_to_klvds(encoder);
	struct sdrv_dpc *dpc;

	if (!kcrtc || !klvds) {
		DRM_ERROR("crtc or lvds is null\n");
		return;
	}

	dpc = kcrtc->master;

	while(dpc && dpc->dpc_type != DPC_TYPE_DC)
		dpc = dpc->next;

	if(dpc == NULL)
		return;

	if(klvds->ctx.dual_mode == 1)
		dpc->pclk_div = 1;

	DRM_DEBUG_ATOMIC("%s dpc:%s pclk_div:%d\n", __func__, dpc->name, dpc->pclk_div);
}

static int kunlun_drm_lvds_atomic_check(struct drm_encoder *encoder,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	kunlun_update_dpc_pllclk_div(crtc_state, encoder);

	return 0;
}

static const struct drm_encoder_funcs kunlun_lvds_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs
kunlun_lvds_encoder_helper_funcs = {
	.enable = kunlun_drm_lvds_encoder_enable,
	.disable = kunlun_drm_lvds_encoder_disable,
	.atomic_check = kunlun_drm_lvds_atomic_check,
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

static int kunlun_lvds_get_resource(struct kunlun_drm_lvds *klvds,
		struct device_node *np)
{
	struct device *dev = klvds->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct lvds_context *ctx = &klvds->ctx;
	struct resource *regs;
	uint32_t tmp;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctx->base = devm_ioremap(dev, regs->start, resource_size(regs));
	if (!ctx->base) {
		DRM_ERROR("Cannot find lvds base regs.\n");
		return -EADDRNOTAVAIL;
	}

	if (!of_property_read_u32(np, "lvds-combine", &tmp))
		ctx->lvds_combine = tmp;
	else
		ctx->lvds_combine = 0;

	if (!of_property_read_u32(np, "lvds-select", &tmp))
		ctx->lvds_select = tmp;
	else
		ctx->lvds_select = 0;

	if (!of_property_read_u32(np, "dual-mode", &tmp))
		ctx->dual_mode = tmp;
	else
		ctx->dual_mode = 0;

	if (!of_property_read_u32(np, "map-format", &tmp))
		ctx->map_format = tmp;
	else
		ctx->map_format = 0;

	if (!of_property_read_u32(np, "pixel-format", &tmp))
		ctx->pixel_format = tmp;
	else
		ctx->pixel_format = 0;

	if (!of_property_read_u32(np, "vsync-pol", &tmp))
		ctx->vsync_pol = tmp;
	else
		ctx->vsync_pol = 0;

	return 0;
}

static int kunlun_drm_lvds_register(struct drm_device *drm,
		struct kunlun_drm_lvds *klvds)
{
	struct drm_encoder *encoder = &klvds->encoder;
	struct drm_connector *connector = &klvds->connector;
	struct device *dev = klvds->dev;
	int ret;

	encoder->possible_crtcs = kunlun_drm_find_possible_crtcs(drm,
			dev->of_node);
	if(!encoder->possible_crtcs)
		return -EPROBE_DEFER;

	drm_encoder_helper_add(encoder, &kunlun_lvds_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &kunlun_lvds_encoder_funcs,
			DRM_MODE_ENCODER_NONE, NULL);
	if(ret) {
		DRM_DEV_ERROR(dev, "Failed to initialize encoder with drm\n");
		return ret;
	}

	if(klvds->bridge) {
		ret = drm_bridge_attach(encoder, klvds->bridge, NULL);
		if(ret < 0) {
			DRM_DEV_ERROR(dev, "Failed to attach bridge\n");
			goto err_encoder_cleanup;
		}
	} else {
		drm_connector_helper_add(connector,
				&kunlun_lvds_connector_helper_funcs);
		drm_connector_init(drm, connector,
				&kunlun_lvds_connector_funcs,
				DRM_MODE_CONNECTOR_LVDS);
		drm_mode_connector_attach_encoder(connector, encoder);
	}

	if(klvds->panel){
		drm_panel_attach(klvds->panel, connector);
	}

	return 0;
err_encoder_cleanup:
	drm_encoder_cleanup(encoder);
	return ret;
}

static int kunlun_drm_lvds_bind(struct device *dev,
		struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct device_node *np = dev->of_node;
	struct kunlun_drm_lvds *klvds;
	int ret;

	klvds = devm_kzalloc(dev, sizeof(*klvds), GFP_KERNEL);
	if(!klvds)
		return -ENOMEM;

	klvds->dev = dev;

	ret = kunlun_lvds_get_resource(klvds, np);
	if (ret) {
		DRM_ERROR("lvds get resouce failed.\n");
		return ret;
	}

	ret = drm_of_find_panel_or_bridge(np, 1, 0,
			&klvds->panel, &klvds->bridge);
	if(ret && ret != -ENODEV) {
		DRM_DEV_ERROR(dev, "Cannot find effective panel or bridge\n");
		return ret;
	}

	ret = kunlun_drm_lvds_register(drm, klvds);
	if(ret)
		return ret;

	dev_set_drvdata(dev, klvds);

	return 0;
}

static void kunlun_drm_lvds_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct kunlun_drm_lvds *klvds = dev_get_drvdata(dev);

	if(klvds->panel)
		drm_panel_detach(klvds->panel);
}

static const struct component_ops kunlun_drm_lvds_ops = {
	.bind = kunlun_drm_lvds_bind,
	.unbind = kunlun_drm_lvds_unbind,
};

static int kunlun_drm_lvds_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &kunlun_drm_lvds_ops);
}

static int kunlun_drm_lvds_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &kunlun_drm_lvds_ops);
	return 0;
}

static const struct of_device_id kunlun_drm_lvds_ids[] = {
	{ .compatible = "semidrive, kunlun-display-lvds", },
	{  },
};
MODULE_DEVICE_TABLE(of, kunlun_drm_lvds_ids);

static struct platform_driver kunlun_drm_lvds_driver = {
	.probe = kunlun_drm_lvds_probe,
	.remove = kunlun_drm_lvds_remove,
	.driver = {
		.of_match_table = kunlun_drm_lvds_ids,
		.name = KUNLUN_DRM_LVDS
	},
};
module_platform_driver(kunlun_drm_lvds_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive kunlun drm lvds");
MODULE_LICENSE("GPL");

