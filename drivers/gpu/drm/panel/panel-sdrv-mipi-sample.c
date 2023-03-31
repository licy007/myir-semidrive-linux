/*
 * panel-sdrv-mipi-sample.c
 *
 * Semidrive platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <drm/drm_atomic_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

enum {
	DSI_MODE_CMD = 0,
	DSI_MODE_VIDEO_BURST,
	DSI_MODE_VIDEO_SYNC_PULSE,
	DSI_MODE_VIDEO_SYNC_EVENT
};

enum {
	CMD_CODE_INIT = 0,
	CMD_CODE_SLEEP_IN,
	CMD_CODE_SLEEP_OUT,
	CMD_CODE_RESERVED0,
	CMD_CODE_RESERVED1,
	CMD_CODE_RESERVED2,
	CMD_CODE_RESERVED3,
	CMD_CODE_MAX
};

struct dsi_cmd_desc {
	u8 data_type;
	u8 wait;
	u8 wc_h;
	u8 wc_l;
	u8 payload[];
};

struct gpio_timing {
	u32 level;
	u32 delay;
};

struct reset_sequence {
	u32 items;
	struct gpio_timing *timing;
};

struct panel_info {
	struct device_node *of_node;
	struct drm_display_mode mode;
	struct drm_display_mode *buildin_modes;
	int num_buildin_modes;
	struct gpio_desc *avdd;
	struct gpio_desc *avee;
	struct gpio_desc *reset;
	struct reset_sequence rst_on_seq;
	struct reset_sequence rst_off_seq;
	const void *cmds[CMD_CODE_MAX];
	int cmds_len[CMD_CODE_MAX];

	uint32_t format;
	uint32_t lanes;
	uint32_t mode_flags;
	bool use_dcs;
};
struct sdrv_panel {
	struct device dev;
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct panel_info info;
	struct backlight_device *backlight;
};

const char *lcd_name;
static int __init lcd_name_get(char *str)
{
	if (str != NULL)
		lcd_name = str;
	DRM_INFO("lcd name from lk: %s\n", lcd_name);
	return 0;
}

__setup("lcd_name=", lcd_name_get);

static struct sdrv_panel *to_sdrv_panel(struct drm_panel *panel)
{
	return container_of(panel, struct sdrv_panel, panel);
}

static int sdrv_panel_send_cmds(struct mipi_dsi_device *dsi,
			const void *data, int size)
{
	struct sdrv_panel *spanel = mipi_dsi_get_drvdata(dsi);
	const struct dsi_cmd_desc *cmds = data;
	u16 len;

	if ((cmds == NULL) || (dsi == NULL))
		return -EINVAL;

	while (size > 0) {
		len = (cmds->wc_h << 8) | cmds->wc_l;

		if (spanel->info.use_dcs)
			mipi_dsi_dcs_write_buffer(dsi, cmds->payload, len);
		else
			mipi_dsi_generic_write(dsi, cmds->payload, len);

		if (cmds->wait)
			msleep(cmds->wait);
		cmds = (const struct dsi_cmd_desc *)(cmds->payload + len);
		size -= (len + 4);
	}

	return 0;
}

static int sdrv_panel_disable(struct drm_panel *panel)
{
	struct sdrv_panel *spanel = to_sdrv_panel(panel);

	DRM_INFO("%s()\n", __func__);

	if (spanel->backlight) {
		spanel->backlight->props.power = FB_BLANK_POWERDOWN;
		spanel->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(spanel->backlight);
	}

	sdrv_panel_send_cmds(spanel->dsi,
			spanel->info.cmds[CMD_CODE_SLEEP_IN],
			spanel->info.cmds_len[CMD_CODE_SLEEP_IN]);

	return 0;
}

static int sdrv_panel_enable(struct drm_panel *panel)
{
	struct sdrv_panel *spanel = to_sdrv_panel(panel);

	DRM_INFO("%s()\n", __func__);

	sdrv_panel_send_cmds(spanel->dsi,
			spanel->info.cmds[CMD_CODE_INIT],
			spanel->info.cmds_len[CMD_CODE_INIT]);

	if (spanel->backlight) {
		spanel->backlight->props.power = FB_BLANK_UNBLANK;
		spanel->backlight->props.state &= ~BL_CORE_FBBLANK;
		backlight_update_status(spanel->backlight);
	}

	return 0;
}

static int sdrv_panel_unprepare(struct drm_panel *panel)
{
	struct sdrv_panel *spanel = to_sdrv_panel(panel);
	struct gpio_timing *timing;
	int items, i;

	DRM_INFO("%s()\n", __func__);

	if (spanel->info.avdd) {
		gpiod_direction_output(spanel->info.avdd, 0);
		mdelay(5);
	}

	if (spanel->info.avee) {
		gpiod_direction_output(spanel->info.avee, 0);
		mdelay(5);
	}

	if (spanel->info.reset) {
		items = spanel->info.rst_off_seq.items;
		timing = spanel->info.rst_off_seq.timing;
		for (i = 0; i < items; i++) {
			gpiod_direction_output(spanel->info.reset, timing[i].level);
			mdelay(timing[i].delay);
		}
	}

	return 0;
}

static int sdrv_panel_prepare(struct drm_panel *panel)
{
	struct sdrv_panel *spanel = to_sdrv_panel(panel);
	struct gpio_timing *timing;
	int items, i;

	DRM_INFO("%s()\n", __func__);

	if (spanel->info.avdd) {
		gpiod_direction_output(spanel->info.avdd, 1);
		mdelay(5);
	}

	if (spanel->info.avee) {
		gpiod_direction_output(spanel->info.avee, 1);
		mdelay(5);
	}

	if (spanel->info.reset) {
		items = spanel->info.rst_on_seq.items;
		timing = spanel->info.rst_on_seq.timing;
		for (i = 0; i < items; i++) {
			gpiod_direction_output(spanel->info.reset, timing[i].level);
			mdelay(timing[i].delay);
		}
	}

	return 0;
}

static int sdrv_panel_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct sdrv_panel *spanel = to_sdrv_panel(panel);
	struct drm_display_mode *mode;
	int mode_count = 0;

	DRM_INFO("%s()\n", __func__);
	mode = drm_mode_duplicate(panel->drm, &spanel->info.mode);
	if (!mode) {
		DRM_ERROR("failed to alloc mode %s\n", spanel->info.mode.name);
		return 0;
	}
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);
	mode_count++;

	panel->connector->display_info.width_mm = spanel->info.mode.width_mm;
	panel->connector->display_info.height_mm = spanel->info.mode.height_mm;

	return mode_count;
}

static const struct drm_panel_funcs sdrv_panel_funcs = {
	.disable = sdrv_panel_disable,
	.unprepare = sdrv_panel_unprepare,
	.prepare = sdrv_panel_prepare,
	.enable = sdrv_panel_enable,
	.get_modes = sdrv_panel_get_modes,
};

static int panel_reset_get_resource(struct device_node *np,
			struct panel_info *info)
{
	return 0;
}

static int get_buildin_modes(struct device_node *lcd_node,
			struct panel_info *info)
{
	struct drm_display_mode *buildin_modes = info->buildin_modes;
	struct device_node *timings_np;
	int num_timings, i, ret;

	timings_np = of_get_child_by_name(lcd_node, "display-timings");
	if (!timings_np) {
		DRM_ERROR("%s: could not find display-timings node\n");
		return -ENODEV;
	}

	num_timings = of_get_child_count(timings_np);
	if (!num_timings) {
		DRM_ERROR("Get num_timings failed\n");
		goto done;
	}

	buildin_modes = kzalloc(sizeof(struct drm_display_mode) * num_timings,
				GFP_KERNEL);

	for (i = 0; i < num_timings; i++) {
		ret = of_get_drm_display_mode(lcd_node,
					&buildin_modes[i], NULL, i);
		if (ret) {
			DRM_ERROR("get display timing failed\n");
			goto entryfail;
		}

		buildin_modes[i].width_mm = info->mode.width_mm;
		buildin_modes[i].height_mm = info->mode.height_mm;
		buildin_modes[i].vrefresh = info->mode.vrefresh;
	}
	info->num_buildin_modes = num_timings;
	DRM_INFO("info->num_buildin_modes = %d\n", num_timings);
	goto done;

entryfail:
	kfree(buildin_modes);
done:
	of_node_put(timings_np);

	return 0;
}

static int sdrv_panel_get_resource(struct mipi_dsi_device *dsi,
			struct sdrv_panel *spanel)
{
	struct device_node *of_node = dsi->dev.of_node;
	struct panel_info *info = &spanel->info;
	struct device *dev = &dsi->dev;
	struct device_node *lcd_node;
	const char *name, *fmt;
	uint32_t mode, val;
	char lcd_path[64];
	int bytes, ret;
	const void *p;

	ret = of_property_read_string(of_node, "semidrive,lcd-attached", &name);
	if (!ret)
		lcd_name = name;

	sprintf(lcd_path, "/lcds/%s", lcd_name);
	lcd_node = of_find_node_by_path(lcd_path);
	if (!lcd_node) {
		DRM_ERROR("%pOF: could not find %s node\n", of_node, lcd_name);
		return -ENODEV;
	}
	info->of_node = lcd_node;

	ret = of_property_read_u32(lcd_node, "semidrive,dsi-work-mode", &mode);
	if (!ret) {
		switch (mode) {
		case DSI_MODE_CMD:
			info->mode_flags = 0;
			break;
		case DSI_MODE_VIDEO_BURST:
			info->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;
			break;
		case DSI_MODE_VIDEO_SYNC_PULSE:
			info->mode_flags = MIPI_DSI_MODE_VIDEO |
					MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
			break;
		case DSI_MODE_VIDEO_SYNC_EVENT:
			info->mode_flags = MIPI_DSI_MODE_VIDEO;
			break;
		default:
			info->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;
			break;
		}
	}else {
		DRM_ERROR("dsi work mode is not found, we use video mode for default\n");
		info->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;
	}

	ret = of_property_read_bool(lcd_node,
		"semidrive,dsi-clock-non-continuous-support");
	if (ret)
		info->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(lcd_node, "semidrive,dsi-lanes-number", &val);
	if (!ret)
		info->lanes = val;
	else
		info->lanes = 4;

	ret = of_property_read_string(lcd_node, "semidrive,dsi-color-format", &fmt);
	if (ret)
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(fmt, "rgb888"))
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(fmt, "rgb666"))
		info->format = MIPI_DSI_FMT_RGB666;
	else if (!strcmp(fmt, "rgb666_packed"))
		info->format = MIPI_DSI_FMT_RGB666_PACKED;
	else if (!strcmp(fmt, "rgb565"))
		info->format = MIPI_DSI_FMT_RGB565;
	else
		DRM_ERROR("dsi-color-format (%s) is not supported\n");

	ret = of_property_read_u32(lcd_node, "semidrive,physical_width", &val);
	if (!ret)
		info->mode.width_mm = val;

	ret = of_property_read_u32(lcd_node, "semidrive,physical_height", &val);
	if (!ret)
		info->mode.height_mm = val;

	ret = panel_reset_get_resource(lcd_node, info);
	if (ret)
		DRM_ERROR("panel_reset_get_resource() failed\n");

	p = of_get_property(lcd_node, "semidrive,initial-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_INIT] = p;
		info->cmds_len[CMD_CODE_INIT] = bytes;
	} else {
		DRM_ERROR("parse init-command property failed\n");
	}

	p = of_get_property(lcd_node, "semidrive,sleep-in-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_IN] = p;
		info->cmds_len[CMD_CODE_SLEEP_IN] = bytes;
	} else {
		DRM_ERROR("parse sleep-in-command property failed\n");
	}

	p = of_get_property(lcd_node, "semidrive,sleep-out-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_OUT] = p;
		info->cmds_len[CMD_CODE_SLEEP_OUT] = bytes;
	} else {
		DRM_ERROR("parse sleep-out-command property failed\n");
	}

	ret = of_get_drm_display_mode(lcd_node, &info->mode, 0, OF_USE_NATIVE_MODE);
	if (ret) {
		DRM_ERROR("of_get_drm_display_mode() failed\n");
		return ret;
	}

	info->mode.vrefresh = drm_mode_vrefresh(&info->mode);

	get_buildin_modes(lcd_node, info);

	spanel->info.avdd = devm_gpiod_get_optional(dev, "avdd", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(spanel->info.avdd))
		DRM_WARN("can't get panel avdd gpio: %ld\n",
					PTR_ERR(spanel->info.avdd));

	spanel->info.avee = devm_gpiod_get_optional(dev, "avee", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(spanel->info.avee))
		DRM_WARN("can't get panel avee gpio: %ld\n",
					PTR_ERR(spanel->info.avee));

	spanel->info.reset = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(spanel->info.reset))
		DRM_WARN("can't get panel reset gpio: %ld\n",
					PTR_ERR(spanel->info.reset));

	return 0;
}

static int sdrv_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device_node *backlight_node;
	struct sdrv_panel *spanel;
	int ret;

	DRM_INFO("%s()\n", __func__);

	spanel = devm_kzalloc(&dsi->dev, sizeof(*spanel), GFP_KERNEL);
	if (!spanel) {
		DRM_ERROR("spanel kzalloc failed\n");
		return -ENOMEM;
	}

	mipi_dsi_set_drvdata(dsi, spanel);

	backlight_node = of_parse_phandle(dsi->dev.of_node,
				"semidrive,backlight", 0);
	if (backlight_node) {
		spanel->backlight = of_find_backlight_by_node(backlight_node);
		of_node_put(backlight_node);
		if (spanel->backlight) {
			spanel->backlight->props.state &= ~BL_CORE_FBBLANK;
			spanel->backlight->props.power = FB_BLANK_UNBLANK;
			backlight_update_status(spanel->backlight);
		} else {
			DRM_ERROR("backlight is not ready\n");
			return -EPROBE_DEFER;
		}
	} else {
		DRM_WARN("backlight node is not found\n");
	}

	ret = sdrv_panel_get_resource(dsi, spanel);
	if (ret) {
		DRM_ERROR("sdrv_panel_get_resource() failed\n");
		return -EPROBE_DEFER;
	}

	spanel->dev.of_node = spanel->info.of_node;
	spanel->panel.dev = &spanel->dev;
	spanel->panel.funcs = &sdrv_panel_funcs;
	drm_panel_init(&spanel->panel);

	ret = drm_panel_add(&spanel->panel);
	if (ret) {
		DRM_ERROR("drm_panel_add() failed\n");
		return ret;
	}

	dsi->lanes = spanel->info.lanes;
	dsi->format = spanel->info.format;
	dsi->mode_flags = spanel->info.mode_flags;

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		DRM_ERROR("mipi_dsi_attach() failed\n");
		drm_panel_remove(&spanel->panel);
		return ret;
	}
	spanel->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, spanel);

	DRM_INFO("panel driver probe success\n");

	return 0;
}

static int sdrv_panel_remove(struct mipi_dsi_device *dsi)
{
	struct sdrv_panel *spanel = mipi_dsi_get_drvdata(dsi);
	struct drm_panel *panel = &spanel->panel;
	int ret;

	DRM_INFO("%s()\n", __func__);

	sdrv_panel_disable(panel);
	sdrv_panel_unprepare(panel);

	ret = mipi_dsi_detach(dsi);
	if (ret)
		DRM_ERROR("failed to detach from DSI host: %d\n", ret);

	drm_panel_detach(panel);
	drm_panel_remove(panel);

	return 0;
}

static const struct of_device_id panel_of_match[] = {
	{.compatible = "semidrive,mipi-panel",},
	{ }
};

MODULE_DEVICE_TABLE(of, panel_of_match);

static struct mipi_dsi_driver sdrv_panel_driver = {
	.driver = {
		.name = "sdrv-mipi-panel-drv",
		.of_match_table = panel_of_match,
	},
	.probe = sdrv_panel_probe,
	.remove = sdrv_panel_remove,
};

module_mipi_dsi_driver(sdrv_panel_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive DRM DSI");
MODULE_LICENSE("GPL");
