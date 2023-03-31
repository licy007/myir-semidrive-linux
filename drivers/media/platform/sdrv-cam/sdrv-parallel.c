/*
 * sdrv-parallel.c
 *
 * Semidrive platform kstream subdev operation
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include <linux/clk.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>

#include "sdrv-csi.h"

#define SDRV_PARALLEL_NAME			"sdrv-csi-parallel"

#define SDRV_PARALLEL_PAD_SINK		0
#define SDRV_PARALLEL_PAD_SRC			1
#define SDRV_PARALLEL_PAD_NUM			2

struct sdrv_csi_parallel {
	struct device *dev;
	struct v4l2_subdev subdev;
	struct media_pad pads[SDRV_PARALLEL_PAD_NUM];
};

static struct v4l2_subdev *parallel_get_sensor_subdev(struct v4l2_subdev *sd)
{
	struct media_pad *sink_pad, *remote_pad;
	int ret;

	ret = sdrv_find_pad(sd, MEDIA_PAD_FL_SINK);
	if(ret < 0)
		return NULL;
	sink_pad = &sd->entity.pads[ret];

	remote_pad = media_entity_remote_pad(sink_pad);
	if(!remote_pad || !is_media_entity_v4l2_subdev(remote_pad->entity))
		return NULL;

	return media_entity_to_v4l2_subdev(remote_pad->entity);
}

static int parallel_s_power(struct v4l2_subdev *sd, int on)
{
#if 0
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, core, s_power, on);
#else
	return 0;
#endif
}

static int parallel_g_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *fi)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, video, g_frame_interval, fi);
}

static int parallel_s_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *fi)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, video, s_frame_interval, fi);
}

static int parallel_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, video, s_stream, enable);
}

static int parallel_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_mbus_code_enum *code)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, pad, enum_mbus_code, cfg, code);
}

static int parallel_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, pad, set_fmt, cfg, format);
}

static int parallel_enum_frame_size(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_size_enum *fse)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, pad, enum_frame_size, cfg, fse);
}

static int parallel_enum_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, pad, enum_frame_interval, cfg, fie);
}

static int parallel_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_subdev *sensor_sd = parallel_get_sensor_subdev(sd);
	if(!sensor_sd)
		return -EINVAL;

	return v4l2_subdev_call(sensor_sd, pad, get_fmt, cfg, format);
}

static const struct v4l2_subdev_core_ops sdrv_parallel_core_ops = {
	.s_power = parallel_s_power,
};

static const struct v4l2_subdev_video_ops sdrv_parallel_video_ops = {
	.g_frame_interval = parallel_g_frame_interval,
	.s_frame_interval = parallel_s_frame_interval,
	.s_stream = parallel_s_stream,
};

static const struct v4l2_subdev_pad_ops sdrv_parallel_pad_ops = {
	.enum_mbus_code = parallel_enum_mbus_code,
	.get_fmt = parallel_get_fmt,
	.set_fmt = parallel_set_fmt,
	.enum_frame_size = parallel_enum_frame_size,
	.enum_frame_interval = parallel_enum_frame_interval,
};

static const struct v4l2_subdev_ops sdrv_parallel_v4l2_ops = {
	.core = &sdrv_parallel_core_ops,
	.video = &sdrv_parallel_video_ops,
	.pad = &sdrv_parallel_pad_ops,
};

static int parallel_link_setup(struct media_entity *entity,
		const struct media_pad *local,
		const struct media_pad *remote, u32 flags)
{
	if(flags & MEDIA_LNK_FL_ENABLED)
		if(media_entity_remote_pad(local))
			return -EBUSY;

	return 0;
}

static const struct media_entity_operations sdrv_parallel_media_ops = {
	.link_setup = parallel_link_setup,
};

static int sdrv_parallel_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_csi_parallel *kcp;
	struct v4l2_subdev *sd;
	int ret;

	kcp = devm_kzalloc(dev, sizeof(*kcp), GFP_KERNEL);
	if(!kcp)
		return -ENOMEM;

	kcp->dev = dev;
	sd = &kcp->subdev;

	v4l2_subdev_init(sd, &sdrv_parallel_v4l2_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sprintf(sd->name, "%s", SDRV_PARALLEL_NAME);
	v4l2_set_subdevdata(sd, kcp);

	kcp->pads[SDRV_PARALLEL_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	kcp->pads[SDRV_PARALLEL_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;

	sd->dev = dev;
	sd->entity.function = MEDIA_ENT_F_IO_V4L;
	sd->entity.ops = &sdrv_parallel_media_ops;
	ret = media_entity_pads_init(&sd->entity,
			SDRV_PARALLEL_PAD_NUM, kcp->pads);
	if(ret < 0) {
		dev_err(dev, "Failed to init media entity:%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, kcp);

	ret = v4l2_async_register_subdev(sd);
	if(ret < 0) {
		dev_err(dev, "Failed to register async subdev");
		goto error_entity_cleanup;
	}

	dev_info(dev, "CSI parallel probed.\n");
	return 0;

error_entity_cleanup:
	media_entity_cleanup(&sd->entity);
	return ret;
}

static int sdrv_parallel_remove(struct platform_device *pdev)
{
	struct sdrv_csi_parallel *kcp = platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = &kcp->subdev;

	media_entity_cleanup(&sd->entity);
	return 0;
}

static const struct of_device_id sdrv_parallel_dt_match[] = {
	{.compatible = "semidrive,sdrv-csi-parallel"},
	{},
};

static struct platform_driver sdrv_parallel_driver = {
	.probe = sdrv_parallel_probe,
	.remove = sdrv_parallel_remove,
	.driver = {
		.name = SDRV_PARALLEL_NAME,
		.of_match_table = sdrv_parallel_dt_match,
	},
};

module_platform_driver(sdrv_parallel_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive CSI parallel driver");
MODULE_LICENSE("GPL v2");
