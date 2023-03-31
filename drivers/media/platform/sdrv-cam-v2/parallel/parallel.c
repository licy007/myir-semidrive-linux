/*
 * sdrv-port-parallel.c
 *
 * Semidrive platform parallel subdev operation
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
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

#include "sdrv-cam-video.h"
#include "sdrv-cam-core.h"

#define SDRV_PARALLEL_NAME			"sdrv-csi-parallel"

#define SDRV_PARALLEL_PAD_SINK		0
#define SDRV_PARALLEL_PAD_SRC			1
#define SDRV_PARALLEL_PAD_NUM			2

struct sdrv_csi_parallel {
	struct device *dev;
	struct v4l2_subdev subdev;
	struct media_pad pads[SDRV_PARALLEL_PAD_NUM];
};

static int parallel_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static const struct v4l2_subdev_core_ops sdrv_parallel_core_ops = {
	.s_power = parallel_s_power,
};

static const struct v4l2_subdev_ops sdrv_parallel_v4l2_ops = {
	.core = &sdrv_parallel_core_ops,
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
	sd->grp_id = CAM_SUBDEV_GRP_ID_MIPICSI_PARALLEL;
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

int parallel_pm_suspend(struct device *dev)
{
	pr_err("parallel: suspend\n");
	return 0;
}

int parallel_pm_resume(struct device *dev)
{
	pr_err("parallel: resume\n");
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops pm_ops = {
	.suspend_late = parallel_pm_suspend,
	.resume_early = parallel_pm_resume,
};
#else
int parallel_dev_suspend(struct device *dev, pm_message_t state)
{
	parallel_pm_suspend(dev);
	return 0;
}
int parallel_dev_resume(struct device *dev)
{
	parallel_pm_resume(dev);
	return 0;
}
#endif // CONFIG_PM



static struct platform_driver sdrv_parallel_driver = {
	.probe = sdrv_parallel_probe,
	.remove = sdrv_parallel_remove,
	.driver = {
		.name = SDRV_PARALLEL_NAME,
		.of_match_table = sdrv_parallel_dt_match,
#ifdef CONFIG_PM
		.pm = &pm_ops,
#else
		.pm = NULL,
		.suspend = camv2_dev_suspend,
		.resume = camv2_dev_resume,
#endif // CONFIG_PM
	},
};

module_platform_driver(sdrv_parallel_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive CSI parallel driver");
MODULE_LICENSE("GPL v2");
