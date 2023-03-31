/*
 * Copyright (C) 2015 Heiko Schocher <hs@denx.de>
 *
 * from:
 * drivers/gpu/drm/panel/panel-ld9040.c
 * ld9040 AMOLED LCD drm_panel driver.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 * Derived from drivers/video/backlight/ld9040.c
 *
 * Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>
#include <drm/drm_panel.h>

#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <linux/gpio.h>

struct kpanel {
	struct device *dev;
	struct drm_panel panel;
	struct videomode vm;
	struct gpio_desc *gpio_lcd_power_en;
	struct gpio_desc *gpio_lcd_rst;
	struct gpio_desc *gpio_backlight_en;
};

static inline struct gpio_desc *m_request_gpio(struct device *dev, const char *str)
{
	struct gpio_desc *gpiod;

	gpiod = devm_gpiod_get_optional(dev, str, GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(gpiod)) {
		DRM_WARN("failed to request %s (gpio%ld)\n", str, PTR_ERR(gpiod));
		return NULL;
	}

	return gpiod;
}

static inline void m_gpio_direction_output(struct gpio_desc *gpiod, int value)
{
	if (gpiod)
		gpiod_direction_output(gpiod, value);
}

static inline struct kpanel *panel_to_kpanel(struct drm_panel *panel)
{
	return container_of(panel, struct kpanel, panel);
}

static int kpanel_display_on(struct kpanel *ctx)
{
	m_gpio_direction_output(ctx->gpio_lcd_power_en, 1);
	mdelay(500);   /* avoid screen shark */
	m_gpio_direction_output(ctx->gpio_backlight_en, 1);

	return 0;
}

static int kpanel_display_off(struct kpanel *ctx)
{
	m_gpio_direction_output(ctx->gpio_backlight_en, 0);
	m_gpio_direction_output(ctx->gpio_lcd_power_en, 0);

	return 0;
}

static int kpanel_power_on(struct kpanel *ctx)
{
	return kpanel_display_on(ctx);
}

static int kpanel_disable(struct drm_panel *panel)
{
	struct kpanel *ctx = panel_to_kpanel(panel);

	return kpanel_display_off(ctx);
}

static int kpanel_enable(struct drm_panel *panel)
{
	struct kpanel *ctx = panel_to_kpanel(panel);

	return kpanel_power_on(ctx);
}

static const struct drm_display_mode default_mode = {
	.clock = 74250,
	.hdisplay = 1280,
	.hsync_start = 1280 + 70,
	.hsync_end = 1280 + 70 + 40,
	.htotal = 1280 + 70 + 40 + 260,
	.vdisplay = 720,
	.vsync_start = 720 + 20,
	.vsync_end = 720 + 20 + 5,
	.vtotal = 720 + 20 + 5 + 5,
	.vrefresh = 60,
};

static int kpanel_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct kpanel *ctx = panel_to_kpanel(panel);
	struct drm_display_mode *mode;
	struct drm_display_info *disp_info = &connector->display_info;

	struct device_node *np = panel->dev->of_node;
	struct device_node *timings_np;
	struct device_node *entry;

	timings_np = of_get_child_by_name(np, "display-timings");
	if (!timings_np) {
		pr_err("%pOF: could not find display-timings node\n", np);
		return -EINVAL;
	}

	entry = of_parse_phandle(timings_np, "native-mode", 0);
	if (!entry) {
		pr_err("%pOF: no timing specifications given\n", timings_np);
		return -EINVAL;
	}

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		pr_err("failed to create a new display mode\n");
		return -ENOMEM;
	}

	drm_display_mode_from_videomode(&ctx->vm, mode);
	drm_bus_flags_from_videomode(&ctx->vm, &connector->display_info.bus_flags);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

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

	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs kpanel_drm_funcs = {
	.disable = kpanel_disable,
	.enable = kpanel_enable,
	.get_modes = kpanel_get_modes,
};

static int parse_gpio_resource(struct device_node *np, struct kpanel *ctx)
{
	ctx->gpio_lcd_power_en = m_request_gpio(ctx->dev, "lcd-power-en");

	ctx->gpio_lcd_rst = m_request_gpio(ctx->dev, "lcd-rst");

	ctx->gpio_backlight_en = m_request_gpio(ctx->dev, "backlight-en");

	return 0;
}

static int kpanel_probe(struct platform_device *pdev)
{
	struct kpanel *ctx;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ret = of_get_videomode(np, &ctx->vm, OF_USE_NATIVE_MODE);
	if(ret < 0)
		return ret;

	ctx->dev = dev;
	ret = parse_gpio_resource(np, ctx);
	if (ret)
		return ret;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &kpanel_drm_funcs;

	platform_set_drvdata(pdev, ctx);
	return drm_panel_add(&ctx->panel);
}

static int kpanel_remove(struct platform_device *pdev)
{
	struct kpanel *ctx = platform_get_drvdata(pdev);

	kpanel_display_off(ctx);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id kpanel_of_match[] = {
	{ .compatible = "semidrive,panel-simple" },
	{ }
};
MODULE_DEVICE_TABLE(of, kpanel_of_match);

static struct platform_driver panel_lvds_driver = {
	.probe		= kpanel_probe,
	.remove		= kpanel_remove,
	.driver		= {
		.name	= "kunlun-panel-simple",
		.of_match_table = kpanel_of_match,
	},
};

module_platform_driver(panel_lvds_driver);

MODULE_AUTHOR("Heiko Schocher <hs@denx.de>");
MODULE_DESCRIPTION("kpanel LCD Driver");
MODULE_LICENSE("GPL v2");
