/*
 * sdrv-cam-main.c
 *
 * Semidrive platform camera v4l2 core file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>

#include "sdrv-cam-os-def.h"
#include "sdrv-cam-video.h"
#include "sdrv-cam-core.h"
#include "csi-controller-os.h"
#include "sdrv-cam-debug.h"

static int kstream_subdev_notifier_complete(struct v4l2_async_notifier
		*async)
{
	struct csi_core *csi = container_of(async, struct csi_core, notifier);
	struct v4l2_subdev *sd, *source_sd;
	struct kstream_device *kstream;
	struct media_entity *source, *sink;
	struct kstream_interface *interface;
	struct kstream_sensor *sensor;
	int ret;

	pr_debug("cam:kstream_subdev_notifier_complete host_id[%d]\n",
			 csi->host_id);
	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		interface = &kstream->interface;
		sensor = &kstream->sensor;
		sd = &kstream->subdev;
		source_sd = interface->subdev;
		sink = source;
		source_sd = sensor->subdev;
		source = &source_sd->entity;
	}
	ret = v4l2_device_register_subdev_nodes(&csi->v4l2_dev);
	return ret;
}

static int kstream_interface_subdev_bound(struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
	struct kstream_device *kstream =
		container_of(asd, struct kstream_device, interface.asd);
	struct csi_core *csi;

	csi = kstream->core;
	pr_debug("cam:<%s> subdev[%s] host_id[%d]\n", __func__, subdev->name,
			 csi->host_id);
	if (csi->mbus_type == V4L2_MBUS_CSI2) {
		list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
			kstream->interface.subdev = subdev;
			kstream->interface.mbus_type = kstream->core->mbus_type;
		}
	} else {
		kstream->interface.subdev = subdev;
		kstream->interface.mbus_type = kstream->core->mbus_type;
	}
	return 0;
}

static int kstream_sensor_subdev_bound(struct v4l2_subdev *subdev,
									   struct v4l2_async_subdev *asd)
{
	struct kstream_device *kstream =
		container_of(asd, struct kstream_device, sensor.asd);
	struct csi_core *csi;
	csi = kstream->core;
	pr_debug("cam:<%s> subdev[%s] host_id[%d]\n", __func__, subdev->name,
			 csi->host_id);
	kstream->sensor.subdev = subdev;
	v4l2_set_subdevdata(subdev, kstream);
	return 0;
}


static int kstream_subdev_notifier_bound(struct v4l2_async_notifier *async,
		struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
	pr_debug("cam:<%s> subdev[%s] grp_id[%d]\n", __func__, subdev->name,
			 subdev->grp_id);
	switch (subdev->grp_id) {
	case CAM_SUBDEV_GRP_ID_MIPICSI_PARALLEL:
		return kstream_interface_subdev_bound(subdev, asd);
		break;
	case CAM_SUBDEV_GRP_ID_SENSOR:
		return kstream_sensor_subdev_bound(subdev, asd);
		break;
	default:
		if (subdev->entity.flags)
			return kstream_sensor_subdev_bound(subdev, asd);
		break;
	}
	return 0;
}

static int sdrv_stream_register_device(struct csi_core *csi)
{
	struct kstream_device *kstream;
	int ret = 0;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		ret = sdrv_stream_register_entities(kstream, &csi->v4l2_dev);
		if (ret < 0)
			return ret;
	}
	return ret;
}

static int sdrv_stream_unregister_device(struct csi_core *csi)
{
	struct kstream_device *kstream;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		sdrv_stream_unregister_entities(kstream);
	}
	return 0;
}

static int sdrv_of_parse_hcrop(struct kstream_device *kstream,
							   struct device_node *node)
{
	uint32_t val;
	int ret;
	struct device_node *port_node = of_get_parent(node);

	val = 0;
	ret = of_property_read_u32(port_node, "hcrop_back", &val);
	kstream->hcrop_back = val;
	val = 0;
	ret = of_property_read_u32(port_node, "hcrop_front", &val);
	kstream->hcrop_front = val;
	val = 0;
	ret = of_property_read_u32(port_node, "hcrop_top_back", &val);
	kstream->hcrop_top_back = val;
	val = 0;
	ret = of_property_read_u32(port_node, "hcrop_top_front", &val);
	kstream->hcrop_top_front = val;
	val = 0;
	ret = of_property_read_u32(port_node, "hcrop_bottom_back", &val);
	kstream->hcrop_bottom_back = val;
	val = 0;
	ret = of_property_read_u32(port_node, "hcrop_bottom_front", &val);
	kstream->hcrop_bottom_front = val;
	if (kstream->hcrop_top_back || kstream->hcrop_bottom_back)
		kstream->hcrop_back = kstream->hcrop_top_back + kstream->hcrop_bottom_back;
	if (kstream->hcrop_top_front || kstream->hcrop_bottom_front)
		kstream->hcrop_front = kstream->hcrop_top_front +
							   kstream->hcrop_bottom_front;
	of_node_put(port_node);
	dev_info(kstream->dev, "%d, %d, %d, %d, %d, %d\n", kstream->hcrop_back,
			 kstream->hcrop_front,
			 kstream->hcrop_top_back, kstream->hcrop_top_front,
			 kstream->hcrop_bottom_back, kstream->hcrop_bottom_front);
	return ret;
}

static int sdrv_of_parse_ports(struct csi_core *csi)
{
	struct device_node *img_ep, *interface, *sensor, *intf_ep, *sensor_ep;
	struct device *dev = csi->dev;
	struct v4l2_async_notifier *notifier = &csi->notifier;
	struct of_endpoint ep;
	struct kstream_device *kstream;
	unsigned int size, i;
	int ret;

	for_each_endpoint_of_node(dev->of_node, img_ep) {
		if (csi->kstream_nums > SDRV_IMG_NUM) {
			dev_warn(dev, "Max support %d ips\n", SDRV_IMG_NUM);
			break;
		}
		if ((csi->sync == 1) && (csi->kstream_nums > 0))
			break;
		kstream = devm_kzalloc(dev, sizeof(*kstream), GFP_KERNEL);
		if (!kstream) {
			dev_err(dev,
					"Failed to allocate memory for kstream device!\n");
			return -ENOMEM;
		}
		ret = of_graph_parse_endpoint(img_ep, &ep);
		if ((ret < 0) || (ep.port >= SDRV_IMG_NUM)) {
			dev_err(dev, "Can't get port id\n");
			return ret;
		}
		kstream->id = ep.port;
		kstream->dev = csi->dev;
		kstream->core = csi;
		kstream->iommu_enable = false;
		csi->kstream_nums++;
		csi->kstream_dev[kstream->id] = kstream;
		sdrv_of_parse_hcrop(kstream, img_ep);
		interface = of_graph_get_remote_port_parent(img_ep);
		if (!interface) {
			dev_dbg(dev, "Cannot get remote parent\n");
			continue;
		}
		kstream->interface.asd.match_type = V4L2_ASYNC_MATCH_FWNODE;
		kstream->interface.asd.match.fwnode.fwnode =
			of_fwnode_handle(interface);
		if (csi->mbus_type == V4L2_MBUS_CSI2) {
			if (csi->mipi_stream_num < 1) {
				notifier->num_subdevs++;
			}
		} else {
			notifier->num_subdevs++;
		}
		for_each_endpoint_of_node(interface, intf_ep) {
			ret = of_graph_parse_endpoint(intf_ep, &ep);
			if ((ret < 0) || (ep.port >= SDRV_IMG_NUM)) {
				dev_err(dev, "Can't get port id\n");
				return ret;
			}
			if (kstream->id != ep.port) {
				//dev_info(dev, "port not match\n");
				of_node_put(intf_ep);
				continue;
			}
			sensor = of_graph_get_remote_port_parent(intf_ep);
			if (!sensor || sensor == dev->of_node) {
				of_node_put(sensor);
				continue;
			}
			kstream->sensor.asd.match_type = V4L2_ASYNC_MATCH_FWNODE;
			if (csi->sync == 1)
				kstream->sensor.asd.match.fwnode.fwnode = of_fwnode_handle(sensor);
			else{
				sensor_ep = of_graph_get_port_by_id(sensor, kstream->id);
				kstream->sensor.asd.match.fwnode.fwnode = of_fwnode_handle(sensor_ep);
			}
			of_node_put(sensor);
			if (csi->mbus_type == V4L2_MBUS_CSI2) {
				if ((csi->sync == 1) && (csi->mipi_stream_num < 1)) {
					notifier->num_subdevs++;
				} else if (csi->sync == 0)
					notifier->num_subdevs++;
				csi->mipi_stream_num++;
			} else {
				notifier->num_subdevs++;
			}
			kstream->enabled = true;
			list_add_tail(&kstream->csi_entry, &csi->kstreams);
		}
		of_node_put(interface);
	}
	if (!notifier->num_subdevs) {
		dev_err(dev, "No subdev found in %s", dev->of_node->name);
		return -EINVAL;
	}
	size = sizeof(*notifier->subdevs) * notifier->num_subdevs;
	notifier->subdevs = devm_kzalloc(dev, size, GFP_KERNEL);
	i = 0;
	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		if (((csi->mbus_type == V4L2_MBUS_CSI2)
			 && (i == 0)) || (csi->mbus_type == V4L2_MBUS_PARALLEL)
			 || (csi->mbus_type == V4L2_MBUS_BT656))
			notifier->subdevs[i++] = &kstream->interface.asd;
		notifier->subdevs[i++] = &kstream->sensor.asd;
		if ((csi->mbus_type == V4L2_MBUS_CSI2)
			&& (csi->sync == 1))
			break;
	}
	pr_debug("cam:v4l2_name[%s] host_id[%d] num_subdevs[%d] mipi_stream_num[%d] i[%d]\n",
			 csi->v4l2_dev.name, csi->host_id, notifier->num_subdevs,
			 csi->mipi_stream_num, i);
	return notifier->num_subdevs;
}

static void translate_mbus_type_to_core_type(struct csi_core *csi)
{
	switch (csi->mbus_type) {
	case V4L2_MBUS_CSI2:
	case V4L2_MBUS_CSI1:
		csi->bt = BT_MIPI;
		break;
	default:
		csi->bt = BT_PARA;
	}
}

static int sdrv_of_parse_core(struct platform_device *pdev,
							  struct csi_core *csi)
{
	struct resource *res;
	struct device *dev = csi->dev;
	struct fwnode_handle *fwnode;
	int ret;
	const char *mbus;
	u32 host_id;
	u32 sync;
	u32 skip_frame;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	csi->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(csi->base)) {
		dev_err(dev, "Could not get csi reg map\n");
		return PTR_ERR(csi->base);
	}
	csi->irq = platform_get_irq(pdev, 0);
	if (csi->irq <= 0) {
		dev_err(dev, "Missing IRQ\n");
		return -EINVAL;
	}
	//dev_info(dev, "csi->irq = %d\n", csi->irq);
	ret = of_property_read_string(dev->of_node, "mbus-type", &mbus);
	if (ret < 0) {
		dev_err(dev, "Missing mbus-type\n");
		return -EINVAL;
	}
	if (strcmp(mbus, "parallel") == 0)
		csi->mbus_type = V4L2_MBUS_PARALLEL;
	else if (strcmp(mbus, "bt656") == 0)
		csi->mbus_type = V4L2_MBUS_BT656;
	else if (strcmp(mbus, "mipi-csi2") == 0)
		csi->mbus_type = V4L2_MBUS_CSI2;
	else if (strcmp(mbus, "dc2csi-1") == 0)
		csi->mbus_type = SDRV_MBUS_DC2CSI1;
	else if (strcmp(mbus, "dc2csi-2") == 0)
		csi->mbus_type = SDRV_MBUS_DC2CSI2;
	else
		dev_err(dev, "Unknow mbus-type\n");
	ret = of_property_read_u32(dev->of_node, "host_id", &host_id);
	if (ret < 0) {
		dev_err(dev, "Missing host id\n");
		return -EINVAL;
	}
	csi->host_id = host_id;
	translate_mbus_type_to_core_type(csi);
	ret = of_property_read_u32(dev->of_node, "sync", &sync);
	if (ret < 0) {
		sync = 0;
	}
	dev_info(dev, "sync=%d\n", sync);
	csi->sync = sync;
	ret = of_property_read_u32(dev->of_node, "skip_frame", &skip_frame);
	if (ret < 0) {
		skip_frame = 0;
	}
	dev_info(dev, "skip_frame=%d\n", skip_frame);
	csi->skip_frame = skip_frame;
	fwnode = dev_fwnode(dev);
	ret = v4l2_fwnode_endpoint_parse(fwnode, &csi->vep);
	fwnode_handle_put(fwnode);
	if (ret) {
		dev_info(dev, "cam:v4l2 fwnode not found!\n");
	} else if (csi->mbus_type == V4L2_MBUS_BT656
			   || csi->mbus_type == V4L2_MBUS_PARALLEL) {
		dev_info(dev, "cam:bus_flags: %08X\n", csi->vep.bus.parallel.flags);
	}
	return ret;
}

static const struct media_device_ops sdrv_media_ops = {
	.link_notify = v4l2_pipeline_link_notify,
};


static irqreturn_t sdrv_csi_isr(int irq, void *dev)
{
	struct csi_core *csi = dev;

	csi_irq_handle(csi->csi_hw);
	return IRQ_HANDLED;
}

static int sdrv_cam_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct csi_core *csi;
	int ret;

	csi = devm_kzalloc(dev, sizeof(*csi), GFP_KERNEL);
	if (!csi)
		return -ENOMEM;
	csi->dev = dev;
	atomic_set(&csi->ref_count, 0);
	atomic_set(&csi->active_stream_num, 0);
	platform_set_drvdata(pdev, csi);
	INIT_LIST_HEAD(&csi->kstreams);
	mutex_init(&csi->lock);
	ret = sdrv_of_parse_core(pdev, csi);
	if (ret < 0)
		return ret;
	ret = sdrv_of_parse_ports(csi);
	if (ret < 0)
		return ret;
	csi->media_dev.dev = csi->dev;
	strlcpy(csi->media_dev.model, "Kunlun camera module",
			sizeof(csi->media_dev.model));
	csi->media_dev.ops = &sdrv_media_ops;
	media_device_init(&csi->media_dev);
	csi->v4l2_dev.mdev = &csi->media_dev;
	ret = v4l2_device_register(csi->dev, &csi->v4l2_dev);
	if (ret < 0) {
		dev_err(dev, "Failed to register V4L2 device: %d\n", ret);
		return ret;
	}
	ret = sdrv_stream_register_device(csi);
	if (ret < 0)
		goto err_register_entities;
	csi->notifier.bound = kstream_subdev_notifier_bound;
	csi->notifier.complete = kstream_subdev_notifier_complete;
	ret = v4l2_async_notifier_register(&csi->v4l2_dev, &csi->notifier);
	if (ret < 0) {
		dev_err(dev, "Failed to register async subdev node: %d\n", ret);
		goto err_register_subdevs;
	}
	csi->csi_hw = (void *)csi_hw_init(csi->base, csi->host_id);
	if (csi->csi_hw == NULL) {
		dev_err(dev, "csi hw init failed\n");
		goto err_register_subdevs;
	}
	ret = csi_cfg_sync(csi->csi_hw, csi->sync);
	ret = devm_request_irq(dev, csi->irq, sdrv_csi_isr,
						   0, dev_name(dev), csi);
	if (ret < 0) {
		dev_err(dev, "Request IRQ failed: %d\n", ret);
		goto err_register_subdevs;
	}
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(40));
	if (ret)
		goto err_register_subdevs;
	cam_debug_init(pdev);
	dev_info(dev, "CSI driver probed.\n");
	return 0;
err_register_subdevs:
	sdrv_stream_unregister_device(csi);
err_register_entities:
	v4l2_device_unregister(&csi->v4l2_dev);
	return ret;
}

static int sdrv_cam_remove(struct platform_device *pdev)
{
	struct csi_core *csi;

	csi = platform_get_drvdata(pdev);
	cam_debug_exit(pdev);
	v4l2_async_notifier_unregister(&csi->notifier);
	sdrv_stream_unregister_device(csi);
	WARN_ON(atomic_read(&csi->ref_count));
	v4l2_device_unregister(&csi->v4l2_dev);
	media_device_unregister(&csi->media_dev);
	media_device_cleanup(&csi->media_dev);
	return 0;
}

static const struct of_device_id sdrv_csi_dt_match[] = {
	{.compatible = "semidrive,sdrv-csi"},
	{},
};

int sdrv_csi_pm_suspend(struct device *dev)
{
	pr_err("camv2: suspend\n");
	return 0;
}

int sdrv_csi_pm_resume(struct device *dev)
{
	pr_err("camv2: resume\n");
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops pm_ops = {
	.suspend_late = sdrv_csi_pm_suspend,
	.resume_early = sdrv_csi_pm_resume,
};
#else
int sdrv_csi_dev_suspend(struct device *dev, pm_message_t state)
{
	sdrv_csi_pm_suspend(dev);
	return 0;
}
int sdrv_csi_dev_resume(struct device *dev)
{
	sdrv_csi_pm_resume(dev);
	return 0;
}
#endif // CONFIG_PM

static struct platform_driver sdrv_cam_driver = {
	.probe = sdrv_cam_probe,
	.remove = sdrv_cam_remove,
	.driver = {
		.name = "sdrv-csi",
		.of_match_table = sdrv_csi_dt_match,
#ifdef CONFIG_PM
		.pm = &pm_ops,
#else
		.pm = NULL,
		.suspend = sdrv_csi_dev_suspend,
		.resume = sdrv_csi_dev_resume,
#endif // CONFIG_PM
	},
};

module_platform_driver(sdrv_cam_driver);

MODULE_ALIAS("platform:sdrv-csi");
MODULE_DESCRIPTION("Semidrive Camera driver");
MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_LICENSE("GPL v2");
