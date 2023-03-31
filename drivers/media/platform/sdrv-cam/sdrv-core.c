/*
 * sdrv-core.c
 *
 * Semidrive platform v4l2 core file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
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

#include "sdrv-csi.h"

#define SDRV_IMG_OFFSET				0x100
#define SDRV_IMG_REG_LEN			0x80

#define CSI_CORE_ENABLE				0x00
#define CSI_RESET				BIT(8)

#define CSI_INT_0				0x04
#define CSI_INT_1				0x08
#define CSI_STORE_DONE_MASK			0x0F
#define CSI_STORE_DONE_SHIFT			0
#define CSI_SDW_SET_MASK			0xF0
#define CSI_SDW_SET_SHIFT			4
#define CSI_CROP_ERR_MASK			0x0F
#define CSI_CROP_ERR_SHIFT			0
#define CSI_PIXEL_ERR_MASK			0xF0
#define CSI_PIXEL_ERR_SHIFT			4
#define CSI_OVERFLOW_MASK			0xF00
#define CSI_OVERFLOW_SHIFT			8
#define CSI_BUS_ERR_MASK			0xFF000
#define CSI_BUS_ERR_SHIFT			12
#define CSI_BT_ERR_MASK				0x100
#define CSI_BT_FATAL_MASK			0x200
#define CSI_BT_COF_MASK				0x400

#define SDRV_IMG_REG_BASE(i)        \
    (SDRV_IMG_OFFSET + (i) * (SDRV_IMG_REG_LEN))

static int kstream_subdev_notifier_complete(struct v4l2_async_notifier
					    *async)
{
	struct csi_core *csi = container_of(async, struct csi_core, notifier);
	struct v4l2_subdev *sd, *source_sd;
	struct kstream_device *kstream;
	struct media_entity *source, *sink;
	struct kstream_interface *interface;
	struct kstream_sensor *sensor;
	unsigned int kstream_sink_pad;
	int ret;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		interface = &kstream->interface;
		sensor = &kstream->sensor;
		sd = &kstream->subdev;
		source_sd = interface->subdev;

		source = &source_sd->entity;
		sink = &sd->entity;

		ret = sdrv_find_pad(sd, MEDIA_PAD_FL_SINK);
		kstream_sink_pad = ret < 0 ? 0 : ret;

		kstream->ops->init_interface(kstream);

		/* link interface --> csi stream module */
		ret = media_create_pad_link(source, interface->source_pad,
					    sink, kstream_sink_pad,
					    MEDIA_LNK_FL_IMMUTABLE |
					    MEDIA_LNK_FL_ENABLED);

		if (ret < 0) {
			dev_err(kstream->dev,
				"Failed to link %s - %s entities: %d\n",
				source->name, sink->name, ret);
			return ret;
		}

		dev_info(kstream->dev, "Link %s -> %s\n", source->name,
			 sink->name);

		sink = source;
		source_sd = sensor->subdev;
		source = &source_sd->entity;

		/* link sensor --> interface */
		ret = media_create_pad_link(source, sensor->source_pad,
					    sink, interface->sink_pad,
					    MEDIA_LNK_FL_IMMUTABLE |
					    MEDIA_LNK_FL_ENABLED);

		if (ret < 0) {
			dev_err(kstream->dev,
				"Failed to link %s - %s entities: %d\n",
				source->name, sink->name, ret);
			return ret;
		}

		dev_info(kstream->dev, "Link %s -> %s\n", source->name,
			 sink->name);
	}

	ret = v4l2_device_register_subdev_nodes(&csi->v4l2_dev);

	if (ret < 0)
		return ret;

	return media_device_register(&csi->media_dev);
}

static int kstream_interface_subdev_bound(struct v4l2_subdev *subdev,
					  struct v4l2_async_subdev *asd)
{
	struct kstream_device *kstream =
	    container_of(asd, struct kstream_device, interface.asd);
	int ret;
	struct csi_core *csi;

	csi = kstream->core;

	if (csi->mbus_type == V4L2_MBUS_CSI2) {

		list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
			kstream->interface.subdev = subdev;
			kstream->interface.mbus_type = kstream->core->mbus_type;
			ret = sdrv_find_pad(subdev, MEDIA_PAD_FL_SOURCE);

			if (ret < 0)
				return ret;

			kstream->interface.source_pad = ret + kstream->id;

			ret = sdrv_find_pad(subdev, MEDIA_PAD_FL_SINK);
			kstream->interface.sink_pad =
			    (ret < 0 ? 0 : ret) + kstream->id;

			dev_info(kstream->dev,
				 "Bound subdev %s source pad: %u sink pad: %u\n",
				 subdev->name, kstream->interface.source_pad,
				 kstream->interface.sink_pad);
		}
	} else {
		kstream->interface.subdev = subdev;
		kstream->interface.mbus_type = kstream->core->mbus_type;
		ret = sdrv_find_pad(subdev, MEDIA_PAD_FL_SOURCE);

		if (ret < 0)
			return ret;

		kstream->interface.source_pad = ret;

		ret = sdrv_find_pad(subdev, MEDIA_PAD_FL_SINK);
		kstream->interface.sink_pad = ret < 0 ? 0 : ret;

		dev_dbg(kstream->dev,
			"Bound subdev %s source pad: %u sink pad: %u\n",
			subdev->name, kstream->interface.source_pad,
			kstream->interface.sink_pad);
	}

	return 0;
}

static int kstream_sensor_subdev_bound(struct v4l2_subdev *subdev,
				       struct v4l2_async_subdev *asd)
{
	struct kstream_device *kstream =
	    container_of(asd, struct kstream_device, sensor.asd);
	int ret;
	struct csi_core *csi;

	csi = kstream->core;

	if (csi->mbus_type == V4L2_MBUS_CSI2) {
		list_for_each_entry(kstream, &csi->kstreams, csi_entry) {

			kstream->sensor.subdev = subdev;
			ret = sdrv_find_pad(subdev, MEDIA_PAD_FL_SOURCE);

			if (ret < 0)
				return ret;

			kstream->sensor.source_pad = ret;

			dev_info(kstream->dev,
				 "Bound subdev %s source pad: %u\n",
				 subdev->name, kstream->sensor.source_pad);
		}
	} else {
		kstream->sensor.subdev = subdev;
		ret = sdrv_find_pad(subdev, MEDIA_PAD_FL_SOURCE);

		if (ret < 0)
			return ret;

		kstream->sensor.source_pad = ret;

		dev_dbg(kstream->dev, "Bound subdev %s source pad: %u\n",
			subdev->name, kstream->sensor.source_pad);
	}

	return 0;
}

static int kstream_subdev_notifier_bound(struct v4l2_async_notifier *async,
					 struct v4l2_subdev *subdev,
					 struct v4l2_async_subdev *asd)
{
	switch (subdev->entity.function) {
	case MEDIA_ENT_F_IO_V4L:
		return kstream_interface_subdev_bound(subdev, asd);
		break;

	case MEDIA_ENT_F_CAM_SENSOR:
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

static int sdrv_of_parse_hcrop(struct kstream_device *kstream, struct device_node *node)
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
		kstream->hcrop_front = kstream->hcrop_top_front + kstream->hcrop_bottom_front;

	of_node_put(port_node);
	dev_info(kstream->dev, "%d, %d, %d, %d, %d, %d\n", kstream->hcrop_back, kstream->hcrop_front,
		kstream->hcrop_top_back, kstream->hcrop_top_front, kstream->hcrop_bottom_back, kstream->hcrop_bottom_front);
	return ret;
}

static int sdrv_of_parse_ports(struct csi_core *csi)
{
	struct device_node *img_ep, *interface, *sensor, *intf_ep;
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
		kstream->base = csi->base + SDRV_IMG_REG_BASE(kstream->id);
		csi->kstream_nums++;

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

			kstream->sensor.asd.match_type =
			    V4L2_ASYNC_MATCH_FWNODE;
			kstream->sensor.asd.match.fwnode.fwnode =
			    of_fwnode_handle(sensor);
			of_node_put(sensor);

			if (csi->mbus_type == V4L2_MBUS_CSI2) {
				if (csi->mipi_stream_num < 1) {
					notifier->num_subdevs++;
				}

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
		notifier->subdevs[i++] = &kstream->interface.asd;
		notifier->subdevs[i++] = &kstream->sensor.asd;

		if ((csi->mbus_type == V4L2_MBUS_CSI2)
		    && (csi->mipi_stream_num > 1))
			break;
	}

	return notifier->num_subdevs;
}

static void translate_mbus_type_to_core_type(struct csi_core *csi)
{
	switch (csi->mbus_type){
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
	fwnode = dev_fwnode(dev);
	ret = v4l2_fwnode_endpoint_parse(fwnode, &csi->vep);
	fwnode_handle_put(fwnode);

	if (ret) {
		dev_dbg(dev, "v4l2 fwnode not found!\n");
	} else if (csi->mbus_type == V4L2_MBUS_BT656
		   || csi->mbus_type == V4L2_MBUS_PARALLEL) {
		dev_dbg(dev, "bus_flags: %08X\n", csi->vep.bus.parallel.flags);
	}

	return ret;
}

static void sdrv_init_core(struct csi_core *csi)
{
	kcsi_writel(csi->base, CSI_CORE_ENABLE, CSI_RESET);
	usleep_range(2, 10);
	kcsi_clr(csi->base, CSI_CORE_ENABLE, CSI_RESET);
}

static const struct media_device_ops sdrv_media_ops = {
	.link_notify = v4l2_pipeline_link_notify,
};

static void sdrv_csi_frm_done_isr(struct csi_core *csi, u32 val)
{
	struct kstream_device *kstream;
	unsigned int ids = (val & CSI_STORE_DONE_MASK) >> CSI_STORE_DONE_SHIFT;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		if (kstream->state != RUNNING && kstream->state != IDLE)
			continue;

		if (!((1 << kstream->id) & ids))
			continue;

		kstream->ops->frm_done_isr(kstream);
		csi->ints.frm_cnt++;
	}
}

static void sdrv_csi_img_update_isr(struct csi_core *csi, u32 val)
{
	struct kstream_device *kstream;
	unsigned int ids = (val & CSI_SDW_SET_MASK) >> CSI_SDW_SET_SHIFT;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		if (kstream->state != RUNNING && kstream->state != IDLE)
			continue;

		if (!((1 << kstream->id) & ids))
			continue;

		kstream->ops->img_update_isr(kstream);
	}
}

static void sdrv_csi_bt_err_isr(struct csi_core *csi, u32 val)
{
	if (csi->mbus_type != V4L2_MBUS_BT656)
		return;

	if (val & CSI_BT_ERR_MASK)
		csi->ints.bt_err++;

	if (val & CSI_BT_FATAL_MASK)
		csi->ints.bt_fatal++;

	if (val & CSI_BT_COF_MASK)
		csi->ints.bt_cof++;
}

static void sdrv_csi_int1_isr(struct csi_core *csi, u32 val)
{
	struct kstream_device *kstream;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		if (kstream->state != RUNNING && kstream->state != IDLE)
			continue;

		if ((1 << kstream->id) &
		    ((val & CSI_CROP_ERR_MASK) >> CSI_CROP_ERR_SHIFT))
			csi->ints.crop_err++;

		if ((1 << kstream->id) &
		    ((val & CSI_PIXEL_ERR_MASK) >> CSI_PIXEL_ERR_SHIFT))
			csi->ints.pixel_err++;

		if ((1 << kstream->id) &
		    ((val & CSI_OVERFLOW_MASK) >> CSI_OVERFLOW_SHIFT))
			csi->ints.overflow++;

		if ((3 << kstream->id) &
		    ((val & CSI_BUS_ERR_MASK) >> CSI_BUS_ERR_SHIFT))
			csi->ints.bus_err++;
	}
}

static irqreturn_t sdrv_csi_isr(int irq, void *dev)
{
	struct csi_core *csi = dev;
	u32 value0, value1;

	value0 = kcsi_readl(csi->base, CSI_INT_0);
	value1 = kcsi_readl(csi->base, CSI_INT_1);

	kcsi_writel(csi->base, CSI_INT_0, value0);
	kcsi_writel(csi->base, CSI_INT_1, value1);

	if (csi->sync == 0) {
		if (value0 & CSI_STORE_DONE_MASK)
			sdrv_csi_frm_done_isr(csi, value0);

		if (value0 & CSI_SDW_SET_MASK)
			sdrv_csi_img_update_isr(csi, value0);
	} else {
		csi->img_sync |= (value0 & 0xff);

		if ((csi->img_sync & 0x0f) == 0x0f) {
			sdrv_csi_frm_done_isr(csi, csi->img_sync);
			csi->img_sync &= 0xf0;
		}

		if ((csi->img_sync & 0xf0) == 0xf0) {
			sdrv_csi_img_update_isr(csi, csi->img_sync);
			csi->img_sync &= 0x0f;
		}
	}

	if ((value0 & (CSI_BT_ERR_MASK | CSI_BT_FATAL_MASK)) || value1) {
		sdrv_csi_bt_err_isr(csi, value0);
		sdrv_csi_int1_isr(csi, value1);
		dev_err(csi->dev,
			"csi%d, int0=0x%08x, int1=0x%08x, frm_cnt=%lld, bt_err=%d, bt_fatal=%d, pixel_err=%d, overflow=%d\n",
			csi->host_id, value0, value1, csi->ints.frm_cnt,
			csi->ints.bt_err, csi->ints.bt_fatal, csi->ints.pixel_err, csi->ints.overflow);
	}

	return IRQ_HANDLED;
}

static int sdrv_csi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct csi_core *csi;
	int ret;

	csi = devm_kzalloc(dev, sizeof(*csi), GFP_KERNEL);

	if (!csi)
		return -ENOMEM;

	csi->dev = dev;
	atomic_set(&csi->ref_count, 0);
	platform_set_drvdata(pdev, csi);
	INIT_LIST_HEAD(&csi->kstreams);
	mutex_init(&csi->lock);

	ret = sdrv_of_parse_core(pdev, csi);

	if (ret < 0)
		return ret;

	sdrv_init_core(csi);

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

	ret = devm_request_irq(dev, csi->irq, sdrv_csi_isr,
			       0, dev_name(dev), csi);

	if (ret < 0) {
		dev_err(dev, "Request IRQ failed: %d\n", ret);
		goto err_register_subdevs;
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(40));

	if (ret)
		goto err_register_subdevs;

	dev_info(dev, "CSI driver probed.\n");
	return 0;

 err_register_subdevs:
	sdrv_stream_unregister_device(csi);
 err_register_entities:
	v4l2_device_unregister(&csi->v4l2_dev);

	return ret;
}

static int sdrv_csi_remove(struct platform_device *pdev)
{
	struct csi_core *csi;

	csi = platform_get_drvdata(pdev);

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

static struct platform_driver sdrv_csi_driver = {
	.probe = sdrv_csi_probe,
	.remove = sdrv_csi_remove,
	.driver = {
		   .name = "sdrv-csi",
		   .of_match_table = sdrv_csi_dt_match,
		   },
};

module_platform_driver(sdrv_csi_driver);

#ifdef CONFIG_ARCH_SEMIDRIVE_V9
static const struct of_device_id sdrv_csi_dt_match_sideb[] = {
	{.compatible = "semidrive,sdrv-csi-sideb"},
	{},
};

static struct platform_driver sdrv_csi_driver_sideb = {
	.probe = sdrv_csi_probe,
	.remove = sdrv_csi_remove,
	.driver = {
		   .name = "sdrv-csi-sideb",
		   .of_match_table = sdrv_csi_dt_match_sideb,
		   },
};

static int __init sdrv_csi2_sideb_init(void)
{
	int ret;

	ret = platform_driver_register(&sdrv_csi_driver_sideb);
	if (ret < 0) {
		printk("fail to register sideb csi2 driver ret = %d.\n", ret);
	}
	return ret;
}

late_initcall(sdrv_csi2_sideb_init);
#endif



MODULE_ALIAS("platform:sdrv-csi");
MODULE_DESCRIPTION("Semidrive Camera driver");
MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_LICENSE("GPL v2");
