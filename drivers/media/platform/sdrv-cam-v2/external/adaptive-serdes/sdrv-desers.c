/*
 * Copyright (C) 2021 Semidrive, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#include <dt-bindings/media/sdrv_csi.h>

#include "sdrv-cam-video.h"
#include "sdrv-cam-core.h"
#include "mipicsi-os.h"
#include "sdrv-deser.h"
#include "deser-max96722.h"
#include "deser-max9286.h"
#include "deser-max96706.h"
#include "deser-max9296.h"

#define MIPI_CSI2_SENS_VC0_PAD_SOURCE   0
#define MIPI_CSI2_SENS_VC1_PAD_SOURCE   1
#define MIPI_CSI2_SENS_VC2_PAD_SOURCE   2
#define MIPI_CSI2_SENS_VC3_PAD_SOURCE   3
#define MIPI_CSI2_SENS_VCX_PADS_NUM	 4

deser_para_t *desers_array_mipi[] = {
	&max9286_para,
	&max96722_para,
	&max9286_para_2,
	&max9296_para,
	NULL,
};
deser_para_t *desers_array_parallel[] = {
	&max96706_para,
	NULL,
};

static inline deser_dev_t *to_deser_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, deser_dev_t, sd);
}

static int deser_s_power(struct v4l2_subdev *sd, int on)
{
	deser_dev_t *sensor = to_deser_dev(sd);
	deser_para_t *pdeser = sensor->priv;
	int ret = 0;

	//printk("%s on = %d +\n", __FUNCTION__, on);

#ifndef PROBE_INIT
	if (on == 1) {
		if (pdeser->power_deser) {
			pdeser->power_deser(sensor, 1);
			msleep(30);
		}

		if (pdeser->init_deser) {
			ret = pdeser->init_deser(sensor);
			msleep(30);
		}
	} else if (on == 0) {
		if (pdeser->power_deser)
			pdeser->power_deser(sensor, 0);

		if (pdeser->power_module) {
			pdeser->power_module(sensor, 0);
		}
	}
#endif

	//printk("%s on = %d -\n", __FUNCTION__, on);
	dev_info(&sensor->i2c_client->dev, "%s: ret=%d\n", __FUNCTION__, ret);
	return ret;
}

static int deser_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	deser_dev_t *sensor = to_deser_dev(sd);

	if (fse->pad != 0)
	{
		return -EINVAL;
	}

	/* only enum index 0 for now */
	if (fse->index >= 1)
	{
		return -EINVAL;
	}


	fse->min_width = sensor->fmt.width;
	fse->max_width = fse->min_width;

	if (sensor->sync == 0)
		fse->min_height = sensor->fmt.height;
	else
		fse->min_height = sensor->fmt.height * 4;

	fse->max_height = fse->min_height;
	return 0;
}

static int deser_enum_frame_interval(struct v4l2_subdev *sd,
					   struct v4l2_subdev_pad_config *cfg,
					   struct v4l2_subdev_frame_interval_enum
					   *fie)
{
	struct v4l2_fract tpf;
	deser_dev_t *sensor = to_deser_dev(sd);

	if (fie->pad != 0)
		return -EINVAL;

	if (fie->index >= 1)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = sensor->frame_interval.denominator;

	fie->interval = tpf;

	return 0;
}

static int deser_g_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *fi)
{
	deser_dev_t *sensor = to_deser_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int deser_s_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

static int deser_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	deser_dev_t *sensor = to_deser_dev(sd);
	deser_para_t *pdeser = sensor->priv;
	int ret = 0;

	if (code->pad != 0)
		return -EINVAL;

	if (pdeser->deser_enum_mbus_code)
		ret = pdeser->deser_enum_mbus_code(sd, cfg, code);

	return ret;
}

static int deser_s_stream(struct v4l2_subdev *sd, int enable)
{
	deser_dev_t *sensor = to_deser_dev(sd);
	deser_para_t *pdeser = sensor->priv;
	//printk("%s enable =%d +\n", __FUNCTION__, enable);

	if (enable == 1) {
		if (pdeser->start_deser)
			pdeser->start_deser(sensor, 1);

	} else if (enable == 0) {
		if (pdeser->start_deser)
			pdeser->start_deser(sensor, 0);
	}

	if (pdeser->dump_deser)
		pdeser->dump_deser(sensor);
	//printk("%s enable =%d -\n", __FUNCTION__, enable);

	return 0;
}

static int deser_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	deser_dev_t *sensor = to_deser_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	fmt = &sensor->fmt;

	format->format = *fmt;

	mutex_unlock(&sensor->lock);

	return 0;
}

static int deser_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	deser_dev_t *sensor = to_deser_dev(sd);

	if(format->format.code != sensor->fmt.code)
		return -EINVAL;

	return 0;
}

static int deser_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops deser_core_ops = {
	.s_power = deser_s_power,
};

static const struct v4l2_subdev_video_ops deser_video_ops = {
	.g_frame_interval = deser_g_frame_interval,
	.s_frame_interval = deser_s_frame_interval,
	.s_stream = deser_s_stream,
};

static const struct v4l2_subdev_pad_ops deser_pad_ops = {
	.enum_mbus_code = deser_enum_mbus_code,
	.get_fmt = deser_get_fmt,
	.set_fmt = deser_set_fmt,
	.enum_frame_size = deser_enum_frame_size,
	.enum_frame_interval = deser_enum_frame_interval,
};

static const struct v4l2_subdev_ops deser_subdev_ops = {
	.core = &deser_core_ops,
	.video = &deser_video_ops,
	.pad = &deser_pad_ops,
};

static const struct v4l2_ctrl_ops deser_ctrl_ops = {
	.s_ctrl = deser_s_ctrl,
};

static int deser_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	deser_dev_t *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	u32 sec_9286;
	u32 sync;
	u32 device_type;
	u32 type;
	int ret;
	struct gpio_desc *gpiod;
	int deser_index = 0;
	deser_para_t *pdeser_param;
	int deser_detect = false;
	deser_para_t **pdeser;

	//printk("%s %s +\n", __FUNCTION__, client->name);
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);

	if (!sensor) {
		dev_err(&client->dev, "No memory!");
		return -ENOMEM;
	}


	sensor->i2c_client = client;

	//Keep compatible dts power configuration
	gpiod = devm_gpiod_get_optional(&client->dev, "pwdn", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
				"pwdn", ret);
		sensor->pwdn_gpio = NULL;
	} else {
		sensor->pwdn_gpio = gpiod;
	}

	gpiod = devm_gpiod_get_optional(&client->dev, "poc", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"poc", ret);
	} else {
		sensor->poc_gpio = gpiod;
	}

	gpiod = devm_gpiod_get_optional(&client->dev, "gpi", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"gpi", ret);
	} else {
		sensor->gpi_gpio = gpiod;
	}

	gpiod = devm_gpiod_get_optional(&client->dev, "pmu", GPIOD_IN);

	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);

		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"poc", ret);
	} else {
		sensor->pmu_gpio = gpiod;
	}

	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "sync", &sync);
	dev_info(&client->dev, "sync: %d, ret=%d\n", sync, ret);

	if (ret < 0) {
		sync = 0;
	}

	sensor->sync = sync;

	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "device", &device_type);
	dev_info(&client->dev, "device_type: 0x%x, ret=%d\n", device_type, ret);

	if (device_type < 0) {
		device_type = 0;
	}

	sensor->device_type = device_type;

	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "type", &type);
	dev_info(&client->dev, "type: 0x%x, ret=%d\n", type, ret);

	if (ret < 0) {
		type = 0;
	}

	sensor->type = type;

	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "sec_9286",
					   &sec_9286);
	dev_info(&client->dev, "sec_9286: %d, ret=%d\n", sec_9286, ret);

	if (!ret) {
		if (sec_9286 == 1)
			sensor->sec_9286_shift = -4;
		else
			sensor->sec_9286_shift = 0;
	} else {
		sensor->sec_9286_shift = 0;
	}

	mutex_init(&sensor->lock);
	mutex_init(&sensor->serer_lock);

	dev_err(&client->dev, "Scan desers ....\n");

	if (sensor->type == SERDES_MIPI)
		pdeser = (deser_para_t **)&desers_array_mipi;
	else
		pdeser = (deser_para_t **)&desers_array_parallel;

	for (deser_index = 0; *(pdeser + deser_index) != NULL; deser_index++) {
		pdeser_param = *(pdeser + deser_index);
		if (pdeser_param->used == DESER_USE_ONCE || NULL == pdeser_param->detect_deser)
			continue;

		sensor->addr_deser = pdeser_param->addr_deser;
		sensor->addr_poc = pdeser_param->addr_poc;
		sensor->addr_serer = pdeser_param->addr_serer;
		sensor->addr_gpioext = pdeser_param->addr_gpioext;
		sensor->priv = pdeser_param;

		switch (sensor->device_type) {
		case SDRV3_ICL02:
			//sensor->addr_deser = pdeser_param->addr_deser = 0x7e;
			break;
		default:
			;
		}

		if (pdeser_param->power_deser) {
			pdeser_param->power_deser(sensor, 0);
			usleep_range(30, 31);
			pdeser_param->power_deser(sensor, 1);
			msleep(20);
		}

		//Please make sure that each deser has different i2c address.
		ret = pdeser_param->detect_deser(sensor);
		if (ret == 0) {
			deser_detect = true;
			if (pdeser_param->power_deser) {
				pdeser_param->power_deser(sensor, 0);
			}
			break;
		} else
			sensor->priv = NULL;
	}

	if (deser_detect == true) {
		pdeser_param = *(pdeser + deser_index);
		client->addr = pdeser_param->addr_deser;
		if (pdeser_param->used == DESER_NOT_USED)
			pdeser_param->used = DESER_USE_ONCE;
		dev_err(&client->dev, "Done, %s\n", pdeser_param->name);
		/* for (deser_index = 0; desers_array[deser_index] != NULL; deser_index++) {
			if (pdeser_param->used == DESER_USE_ONCE)
				pr_info("deser name = %s\n"	, desers_array[deser_index]->name);
		} */
	} else {
		dev_err(&client->dev, "Can't match any deser at i2c bus %s.\n", dev_name(&client->dev));
		dev_info(&client->dev, "deser list:\n");
		for (deser_index = 0; *(pdeser + deser_index) != NULL; deser_index++) {
			dev_info(&client->dev, "deser name = %s\n", (*(pdeser + deser_index))->name);
		}
		ret = -ENODEV;
		goto mutex_fail;
	}

	fmt = &sensor->fmt;
	fmt->code = pdeser_param->mbus_code;
	fmt->colorspace = pdeser_param->colorspace;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = pdeser_param->quantization;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = pdeser_param->width;
	fmt->height = pdeser_param->height;
	fmt->field = pdeser_param->field;
	sensor->frame_interval.numerator = pdeser_param->numerator;
	sensor->frame_interval.denominator = pdeser_param->denominator;

	sensor->ae_target = 52;
	sensor->priv = pdeser_param;

	switch (sensor->device_type) {
	case SDRV1_ICL02:
		pdeser_param->mipi_bps = 792*1000*1000;
		break;
	case SDRV3_ICL02:
		fmt->code = pdeser_param->mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8;
		fmt->height = pdeser_param->height = 800;
		break;
	case SDRV3_MINIEYE:
		fmt->code = pdeser_param->mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8;
		//fmt->height = pdeser_param->height = 800;
		break;
	default:
		;
	}

	/* optional indication of physical rotation of sensor */
	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "rotation",
					   &rotation);

	if (!ret) {
		switch (rotation) {
		case 180:
			sensor->upside_down = true;

			/* fall through */
		case 0:
			break;

		default:
			dev_warn(dev,
				 "%u degrees rotation is not supported, ignoring...\n",
				 rotation);
		}
	}

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(&client->dev),
						  NULL);

	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		ret = -EINVAL;
		goto mutex_fail;
	}

	ret = v4l2_fwnode_endpoint_parse(endpoint, &sensor->ep);
	fwnode_handle_put(endpoint);

	if (ret) {
		dev_err(dev, "Could not parse endpoint\n");
		ret = -EINVAL;
		goto mutex_fail;
	}

	v4l2_i2c_subdev_init(&sensor->sd, client, &deser_subdev_ops);

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->pads[MIPI_CSI2_SENS_VC0_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC1_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sensor->pads[MIPI_CSI2_SENS_VC3_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity,
					 MIPI_CSI2_SENS_VCX_PADS_NUM, sensor->pads);

	if (ret)
		goto mutex_fail;

#ifdef PROBE_INIT
	//Power deser
	if (pdeser_param->power_deser) {
		pdeser_param->power_deser(sensor, 0);
		usleep_range(5000, 5000);
		pdeser_param->power_deser(sensor, 1);
		msleep(50);
	}

	//Init deser
	if (pdeser_param->init_deser) {
		ret = pdeser_param->init_deser(sensor);
		if (ret < 0)
			goto free_ctrls;
	}
#endif

	ret = v4l2_async_register_subdev(&sensor->sd);

	if (ret)
		goto free_ctrls;


	//printk("%s -\n", __FUNCTION__);

	return 0;

free_ctrls:
	//v4l2_ctrl_handler_free(&sensor->ctrls.handler);
	media_entity_cleanup(&sensor->sd.entity);
mutex_fail:
	mutex_destroy(&sensor->lock);
	mutex_destroy(&sensor->serer_lock);
	devm_kfree(dev, sensor);
	return ret;
}

static int deser_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	deser_dev_t *sensor = to_deser_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	mutex_destroy(&sensor->serer_lock);
	media_entity_cleanup(&sensor->sd.entity);
	//v4l2_ctrl_handler_free(&sensor->ctrls.handler);

	return 0;
}

static const struct of_device_id deser_dt_ids[] = {
	{.compatible = SDRV_DESER_NAME},
	{.compatible = SDRV2_DESER_NAME},
	{.compatible = SDRV3_DESER_NAME},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, deser_dt_ids);

static struct i2c_driver deser_i2c_driver = {
	.driver = {
		   .name = "sdrv-deser",
		   .of_match_table = deser_dt_ids,
		   .probe_type = PROBE_PREFER_ASYNCHRONOUS,
		   },
	.probe = deser_probe,
	.remove = deser_remove,
};

module_i2c_driver(deser_i2c_driver);

MODULE_DESCRIPTION("Deser MIPI Camera Subdev Driver");
MODULE_AUTHOR("michael chen");
MODULE_LICENSE("GPL");
