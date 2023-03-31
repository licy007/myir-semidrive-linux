/*
 * Copyright (C) 2020-2030 Semidrive, Inc. All Rights Reserved.
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
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of.h>
#include <linux/of_graph.h>

#include "sdrv_serdes_core.h"
#include "csi-controller-os.h"

#define SDRV_SERDES_NAME  "sdrv,semidrive_serdes"

#ifdef CONFIG_MIPI_MAX9286_FENGJING_MC163_720P_MAX96705
extern serdes_para_t max9286_fengjing_MC163_MAX96705_720p;
#endif

#ifdef CONFIG_MIPI_MAX96722_SENGYUN_IMX390C_5200_MAX9295
extern serdes_para_t max96722_sengyun_IMX390c_5200_GMSL2_1080p;
#endif

#ifdef CONFIG_DVP_MAX96706_FENGJING_MC163_MAX96705
extern serdes_para_t max96706_fengjing_MC163_MAX96705_720p;
#endif

#ifdef CONFIG_MIPI_MAX96722_FENGJING_MC163_MAX96705
extern serdes_para_t max96722_fengjing_MC163_MAX96705_720p;
#endif

serdes_para_t *g_serdes_config[] = {
#ifdef CONFIG_MIPI_MAX9286_FENGJING_MC163_720P_MAX96705
	&max9286_fengjing_MC163_MAX96705_720p,
#endif

#ifdef CONFIG_MIPI_MAX96722_SENGYUN_IMX390C_5200_MAX9295
	&max96722_sengyun_IMX390c_5200_GMSL2_1080p,
#endif

#ifdef CONFIG_DVP_MAX96706_FENGJING_MC163_MAX96705
    &max96706_fengjing_MC163_MAX96705_720p,
#endif

#ifdef CONFIG_MIPI_MAX96722_FENGJING_MC163_MAX96705
	&max96722_fengjing_MC163_MAX96705_720p,
#endif
};

static inline struct serdes_dev_t *to_serdes_dev(struct v4l2_subdev *sd)
{
	struct serdes_dev_t *pserdes = (struct serdes_dev_t *)
								 v4l2_get_subdev_hostdata(sd);
	return pserdes;
}

int serdes_read_reg_16(struct i2c_client *client,
					   u16 reg,
					   u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);
	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&client->dev, "%s:chip 0x%02x error: reg=0x%02x\n",
				 __func__,   msg[0].addr, reg);
		return ret;
	}
	*val = buf[0];
	return 0;
}
EXPORT_SYMBOL(serdes_read_reg_16);

int serdes_write_reg_16(struct i2c_client *client,
						u16 reg,
						u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev,
				"%s:  chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
				__func__, msg.addr, reg, val);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(serdes_write_reg_16);

int serdes_read_reg_8(struct i2c_client *client,
					  u8 reg,
					  u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	buf[0] = reg;
	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);
	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&client->dev, "%s:chip 0x%02x error: reg=0x%02x\n",
				 __func__,   msg[0].addr, reg);
		return ret;
	}
	*val = buf[0];
	return 0;
}
EXPORT_SYMBOL(serdes_read_reg_8);

int serdes_write_reg_8(struct i2c_client *client,
					   u8 reg,
					   u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev,
				"%s:  chip 0x%02x error: reg=0x%02x, val=0x%02x\n",
				__func__, msg.addr, reg, val);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(serdes_write_reg_8);

//get the corresponding stream id from subdev
int serdes_get_stream_id(struct v4l2_subdev *sd)
{
	struct kstream_device *kstream;

	kstream = (struct kstream_device *)v4l2_get_subdevdata(sd);
	if (kstream == NULL)
		return -EINVAL;
	return kstream->id;
}

struct sdrv_cam_video *serdes_get_video_dev(struct v4l2_subdev *sd)
{
	struct kstream_device *kstream;

	kstream = (struct kstream_device *)v4l2_get_subdevdata(sd);
	if (kstream == NULL)
		return NULL;
	return &kstream->video;
}

const char *serdes_get_vdev_name(struct v4l2_subdev *sd)
{
	struct sdrv_cam_video *video;

	video = serdes_get_video_dev(sd);
	return dev_name(&video->vdev.dev);
}

#define WAIT_COUNT (10)
static int serdes_s_power(struct v4l2_subdev *sd, int on)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);
	serdes_para_t *pserdes_para = serdes->priv;
	int i = 0;
	int link = 0;
	int ret = 0;
	int channel = 0;

	channel = serdes_get_stream_id(sd);
	if (channel < 0)
		return -EINVAL;
	if (on == 1) {
#ifdef PROBE_INIT
		if (serdes->serdes_status.init_status !=
			SERDES_DONE) {//maybe the init thread is not DES_DONE,here wait for up to 200ms
			while ((serdes->serdes_status.init_status != SERDES_DONE)
				   && (i < WAIT_COUNT)) {
				usleep_range(20000, 21000);
				i++;
			}
			if (serdes->serdes_status.init_status != SERDES_DONE) {
				dev_err(&(serdes->i2c_client->dev), "failed to wait for init SERDES_DONE\n");
				return -EIO;
			}
		}
		if (pserdes_para->serdes_check_link_status_ex != NULL) {
			mutex_lock(&serdes->lock);
			link = pserdes_para->serdes_check_link_status_ex(serdes,
					channel);//check if lock is on
			mutex_unlock(&serdes->lock);
			if (link == 0) {
				//current channel not link locked. do something
				ret = -ENODEV;
			}
		}
#else
		if ((serdes->serdes_status.stream_cnt ==
			 0)) { //do init only when no stream is on
			dev_info(&serdes->i2c_client->dev, "open camera\n");
			mutex_lock(&serdes->lock);
			if (pserdes_para->serdes_power != NULL)
				pserdes_para->serdes_power(serdes, 1);
			if (pserdes_para->serdes_init != NULL)
				pserdes_para->serdes_init(serdes);
			serdes->serdes_status.init_status = SERDES_DONE;
			mutex_unlock(&serdes->lock);
			dev_info(&serdes->i2c_client->dev, "open camera end\n");
		}
#endif
	} else if (on == 0) {
#ifndef PROBE_INIT
		//power down the camera when no stream is on
		if (serdes->serdes_status.stream_cnt == 0) {
			if (pserdes_para->serdes_power != NULL) {
				mutex_lock(&serdes->lock);
				pserdes_para->serdes_power(serdes, 0);
				mutex_unlock(&serdes->lock);
			}
			dev_info(&serdes->i2c_client->dev, "close camera\n");
		}
#endif
	}
	return ret;
}

static int serdes_enum_frame_size(struct v4l2_subdev *sd,
								 struct v4l2_subdev_pad_config *cfg,
								 struct v4l2_subdev_frame_size_enum *fse)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);
	int channel;

	if (fse->pad != 0)
		return -EINVAL;
	if (fse->index >= 1)
		return -EINVAL;
	channel = serdes_get_stream_id(sd);
	dev_info(&serdes->i2c_client->dev, "%s channel %d", __func__, channel);
	if (serdes->sync == 0) {
		fse->min_width = serdes->width[channel];
		serdes->fmt.width = serdes->width[channel];
		serdes->fmt.height = serdes->height[channel];
		fse->min_height = serdes->height[channel];
	} else {
		fse->min_width = serdes->width[0];
		serdes->fmt.width = serdes->width[0];
		serdes->fmt.height = serdes->height[0];
		fse->min_height = serdes->height[0] * 4;
	}
	fse->max_width = fse->min_width;
	fse->max_height = fse->min_height;
	return 0;
}

static int serdes_enum_frame_interval(struct v4l2_subdev *sd,
									 struct v4l2_subdev_pad_config *cfg,
									 struct v4l2_subdev_frame_interval_enum
									 *fie)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);

	if (fie->pad != 0)
		return -EINVAL;
	if (fie->index >= 2)
		return -EINVAL;
	fie->interval.numerator = 1;
	fie->interval.denominator = serdes->frame_interval.denominator;
	return 0;
}

static int serdes_g_frame_interval(struct v4l2_subdev *sd,
								  struct v4l2_subdev_frame_interval *fi)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);

	fi->interval = serdes->frame_interval;
	return 0;
}

static int serdes_s_frame_interval(struct v4l2_subdev *sd,
								  struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

static int serdes_enum_mbus_code(struct v4l2_subdev *sd,
								struct v4l2_subdev_pad_config *cfg,
								struct v4l2_subdev_mbus_code_enum *code)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);
	serdes_para_t *pserdes_para = serdes->priv;
	int ret = 0;

	if (code->pad != 0)
		return -EINVAL;
	if (pserdes_para->serdes_enum_mbus_code)
		ret = pserdes_para->serdes_enum_mbus_code(sd, cfg, code);
	return ret;
}

static int serdes_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);
	serdes_para_t *pserdes_para = serdes->priv;
	int channel;

	channel = serdes_get_stream_id(sd);
	if (channel < 0)
		return -EINVAL;
	if (enable == 1) {
		mutex_lock(&serdes->lock);
		if (serdes->serdes_status.stream_cnt == 0) {
			pserdes_para->serdes_start(serdes, enable);
			schedule_delayed_work(&serdes->hotplug_work, msecs_to_jiffies(20));
		}
		serdes->serdes_status.stream_cnt++;
		if (serdes->sync == 1) {
			serdes->serdes_streamon_bitmap = 0xf;
		} else {
			serdes->serdes_streamon_bitmap |= (1 << channel);
		}
		serdes->last_channel_lock_status |= serdes->serdes_streamon_bitmap;
		mutex_unlock(&serdes->lock);
	} else if (enable == 0) {
		mutex_lock(&serdes->lock);
		serdes->serdes_status.stream_cnt--;
		if ((serdes->sync == 1)
			&& (serdes->serdes_status.stream_cnt ==
				0)) { //set stream on bitmap to zero only when no stream is on
			serdes->serdes_streamon_bitmap = 0;
		} else {
			serdes->serdes_streamon_bitmap &= ~(1 << channel);
		}
		serdes->last_channel_lock_status &=
				serdes->serdes_streamon_bitmap;
		if (serdes->serdes_status.stream_cnt == 0) {
			pserdes_para->serdes_start(serdes, enable);
			mutex_unlock(&serdes->lock);
			cancel_delayed_work_sync(&serdes->hotplug_work);
			return 0;
		}
		mutex_unlock(&serdes->lock);
	}
	return 0;
}

static int serdes_get_fmt(struct v4l2_subdev *sd,
						 struct v4l2_subdev_pad_config *cfg,
						 struct v4l2_subdev_format *format)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;
	mutex_lock(&serdes->lock);
	fmt = &serdes->fmt;
	format->format = *fmt;
	mutex_unlock(&serdes->lock);
	return 0;
}

static int serdes_set_fmt(struct v4l2_subdev *sd,
						 struct v4l2_subdev_pad_config *cfg,
						 struct v4l2_subdev_format *format)
{
	struct serdes_dev_t *serdes = to_serdes_dev(sd);

	if (format->format.code != serdes->fmt.code)
		return -EINVAL;
	return 0;
}

static int serdes_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_subdev_core_ops serdes_core_ops = {
	.s_power = serdes_s_power,
};

static const struct v4l2_subdev_video_ops serdes_video_ops = {
	.g_frame_interval = serdes_g_frame_interval,
	.s_frame_interval = serdes_s_frame_interval,
	.s_stream = serdes_s_stream,
};

static const struct v4l2_subdev_pad_ops serdes_pad_ops = {
	.enum_mbus_code = serdes_enum_mbus_code,
	.get_fmt = serdes_get_fmt,
	.set_fmt = serdes_set_fmt,
	.enum_frame_size = serdes_enum_frame_size,
	.enum_frame_interval = serdes_enum_frame_interval,
};

static const struct v4l2_subdev_ops serdes_subdev_ops = {
	.core = &serdes_core_ops,
	.video = &serdes_video_ops,
	.pad = &serdes_pad_ops,
};

static const struct v4l2_ctrl_ops serdes_ctrl_ops = {
	.s_ctrl = serdes_s_ctrl,
};

static void init_delayed_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct serdes_dev_t *serdes = container_of(dwork, struct serdes_dev_t,
								  init_work);
	serdes_para_t *serdes_param;
	int ret = 0;

	if (serdes == NULL)
		return;
	serdes_param = serdes->priv;
	dev_info(&serdes->i2c_client->dev, "init work start\n");
	if (serdes_param->serdes_power != NULL) {
		mutex_lock(&serdes->lock);
		serdes_param->serdes_power(serdes, 1);
		mutex_unlock(&serdes->lock);
	}
	if (serdes_param->serdes_init != NULL){
		mutex_lock(&serdes->lock);
		ret = serdes_param->serdes_init(serdes);
		if (ret < 0) {
			dev_err(&serdes->i2c_client->dev, "init failed\n");
		} else
			serdes->serdes_status.init_status = SERDES_DONE;
		mutex_unlock(&serdes->lock);
	}
}

static void hotplug_delayed_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct serdes_dev_t *serdes = container_of(dwork, struct serdes_dev_t,
								  hotplug_work);
	serdes_para_t *serdes_param;
	bool fg_reinit = false;
	struct sdrv_cam_video *video;
	const char *video_name;
	char *signal_envp[3] = {NULL, NULL, NULL};
	int channel = 0;
	int res;
	int i;

	if (serdes == NULL)
		return;
	serdes_param = serdes->priv;
	mutex_lock(&serdes->lock);
	if (serdes_param->serdes_check_link_status != NULL)
		serdes->current_channel_lock_status =
			serdes_param->serdes_check_link_status(serdes);
	if (serdes_param->serdes_check_video_lock_status != NULL) {
		fg_reinit = serdes_param->serdes_check_video_lock_status(serdes);
	}
	if ((serdes->current_channel_lock_status >
		 serdes->last_channel_lock_status) || fg_reinit) {
		dev_info(&serdes->i2c_client->dev,
				 "plugin detected! current lock %d last lock %d reinit %d\n",
				 serdes->current_channel_lock_status, serdes->last_channel_lock_status,
				 fg_reinit);
		msleep(200);//wait for link stable
		if (serdes_param->serdes_power != NULL) {
			serdes_param->serdes_power(serdes, 0);
			serdes_param->serdes_power(serdes, 1);
		}
		if (serdes_param->serdes_init != NULL)
			serdes_param->serdes_init(serdes);
		if (serdes->sync == 1) {
			video = serdes_get_video_dev(&serdes->sd[0]);
			if (video != NULL) {
				if (video->hotplug_callback != NULL)
					video->hotplug_callback(video);
			}
		} else {
			for (i = 0; i < MAX_SENSOR_NUM; i++) { //reset mipi
				if (((serdes->serdes_streamon_bitmap>>i)&1) != 0) {
					video = serdes_get_video_dev(&serdes->sd[i]);
					if (video != NULL) {
						if (video->hotplug_callback != NULL)
							video->hotplug_callback(video);
					}
				}
			}
		}
		serdes_param->serdes_start(serdes, 1);
		usleep_range(50000,
					 51000);//wait for the link status to get stable
		if (serdes_param->serdes_check_link_status != NULL)
			serdes->current_channel_lock_status =
				serdes_param->serdes_check_link_status(serdes);
		if (serdes->serdes_status.init_status == SERDES_PROBED)
			serdes->last_channel_lock_status = serdes->current_channel_lock_status;
		serdes->serdes_status.init_status = SERDES_DONE;
	}
	if (serdes->current_channel_lock_status !=
		serdes->last_channel_lock_status) {
		if (serdes->sync == 1) {
			video_name = serdes_get_vdev_name(&serdes->sd[0]);
		} else {
			res = serdes->current_channel_lock_status^serdes->last_channel_lock_status;
			for (i = 0; i < MAX_SENSOR_NUM; i++) {
				if (((res>>i)&0x1) > 0) {
					channel = i;
					break;
				}
			}
			video_name = serdes_get_vdev_name(&serdes->sd[channel]);
		}
		dev_info(&serdes->i2c_client->dev,
				 "video name %s channel %d current %d last %d\n", video_name, channel,
				 serdes->current_channel_lock_status, serdes->last_channel_lock_status);
		signal_envp[0] = kasprintf(GFP_KERNEL, "DEV_NAME=%s", video_name);
		signal_envp[1] = kasprintf(GFP_KERNEL, "DEV_INFO=%s", serdes_param->module_name);
		signal_envp[2] = NULL;
		if (serdes->current_channel_lock_status >
			serdes->last_channel_lock_status) {
			if (serdes->sync == 1) {
				kobject_uevent_env(&serdes->sd[0].dev->kobj, KOBJ_ADD, signal_envp);
			} else {
				kobject_uevent_env(&serdes->sd[channel].dev->kobj, KOBJ_ADD,
							   signal_envp);
			}

		} else{
			if (serdes->sync == 1) {
				kobject_uevent_env(&serdes->sd[0].dev->kobj, KOBJ_REMOVE, signal_envp);
			} else {
				kobject_uevent_env(&serdes->sd[channel].dev->kobj, KOBJ_REMOVE,
							   signal_envp);
			}
		}
		kfree(signal_envp[0]);
		kfree(signal_envp[1]);
		serdes->last_channel_lock_status = serdes->current_channel_lock_status;
	}
	mutex_unlock(&serdes->lock);
	schedule_delayed_work(&serdes->hotplug_work, msecs_to_jiffies(200));
}

static int of_get_port_count(const struct device_node *np)
{
	struct device_node *ports, *child;
	int num = 0;
	/* check if this node has a ports subnode */
	ports = of_get_child_by_name(np, "ports");
	if (ports)
		np = ports;
	for_each_child_of_node(np, child)
	if (of_node_cmp(child->name, "port") == 0)
		num++;
	of_node_put(ports);
	return num;
}

int serdes_parse_dt(struct serdes_dev_t *serdes,
				   struct i2c_client *client)
{
	u32 sync;
	int ret;
	struct device_node *port;
	struct gpio_desc *gpiod;
	int i = 0;
	int port_count = 0;

	ret = of_property_read_string(client->dev.of_node, "cam_module",
								  &serdes->module_name);
	if (ret < 0) {
		dev_err(&client->dev, "no camera module is parsed\n");
		return -EINVAL;
	}
	dev_info(&client->dev, "%s %s\n", __func__, serdes->module_name);
	ret = of_property_read_u32(client->dev.of_node, "ser_addr",
							   &serdes->addr_ser);
	if (ret < 0) {
		dev_info(&client->dev, "no serializer addr is parsed\n");
	}
	ret = of_property_read_u32(client->dev.of_node, "poc_addr",
							   &serdes->addr_poc);
	if (ret < 0) {
		dev_info(&client->dev, "no poc addr is parsed\n");
	}
	ret = of_property_read_u32(client->dev.of_node, "width",
							   &serdes->width[0]);
	if (ret < 0) {
		dev_info(&client->dev, "fail to get image width\n");
	} else {
		for (i = 1; i < MAX_SENSOR_NUM; i++)
			serdes->width[i] = serdes->width[0];
	}
	ret = of_property_read_u32(client->dev.of_node, "height",
							   &serdes->height[0]);
	if (ret < 0) {
		dev_info(&client->dev, "fail to get image height\n");
	} else {
		for (i = 1; i < MAX_SENSOR_NUM; i++)
			serdes->height[i] = serdes->height[0];
	}
	gpiod = devm_gpiod_get_optional(&client->dev, "pwdn", GPIOD_IN);
	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"pwdn", ret);
	} else {
		serdes->pwdn_gpio = gpiod;
	}
	gpiod = devm_gpiod_get_optional(&client->dev, "poc", GPIOD_IN);
	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"poc", ret);
	} else {
		serdes->poc_gpio = gpiod;
	}
	gpiod = devm_gpiod_get_optional(&client->dev, "gpi", GPIOD_IN);
	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get %s GPIO: %d\n",
					"gpi", ret);
		gpiod = NULL;
	} else {
		serdes->gpi_gpio = gpiod;
	}
	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "sync", &sync);
	if (ret < 0) {
		sync = 0;
	}
	dev_info(&client->dev, "sync: %d, ret=%d\n", sync, ret);
	serdes->sync = sync;
	port_count = of_get_port_count(client->dev.of_node);
	dev_info(&client->dev, "port count %d\n", port_count);
	for (i = 0; i < port_count; i++) {
		port = of_graph_get_port_by_id(client->dev.of_node, i);
		if (!port) {
			dev_info(&client->dev, "no port %d\n", i);
			continue;
		}
		dev_info(&client->dev, "parse port %d\n", i);
		serdes->ser_bitmap |= (1<<i);
		ret = of_property_read_u32(port, "width",  &serdes->width[i]);
		if (ret < 0) {
			dev_info(&client->dev, "no width parsed in endpoint %d\n", i);
		}
		ret = of_property_read_u32(port, "height",  &serdes->height[i]);
		if (ret < 0) {
			dev_info(&client->dev, "no height parsed in endpoint %d\n", i);
		}
		ret = of_property_read_u32(port, "remap_ser_addr",
								   &serdes->remap_addr_ser[i]);
		if (ret < 0) {
			dev_info(&client->dev, "no ser_addr parsed in endpoint %d\n", i);
		}
	}
	return 0;
}

int serdes_subdev_init(struct serdes_dev_t *serdes)
{
	int ret = 0;
	int i = 0;
	serdes_para_t *pserdes_para = serdes->priv;
	struct v4l2_mbus_framefmt *fmt;
	struct i2c_client *client = serdes->i2c_client;
	struct device_node *subdev_of_node;

	fmt = &serdes->fmt;
	fmt->code = pserdes_para->mbus_code;
	fmt->colorspace = pserdes_para->colorspace;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = pserdes_para->quantization;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->field = pserdes_para->field;
	serdes->frame_interval.numerator = pserdes_para->numerator;
	serdes->frame_interval.denominator = pserdes_para->fps;
	serdes->serdes_status.init_status = SERDES_PROBED;
	serdes->serdes_status.stream_cnt = 0;
	serdes->pads[MIPI_CSI2_SENS_VC0_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	serdes->pads[MIPI_CSI2_SENS_VC1_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	serdes->pads[MIPI_CSI2_SENS_VC2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	serdes->pads[MIPI_CSI2_SENS_VC3_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	mutex_init(&serdes->lock);
	mutex_init(&serdes->lock_hotplug);
	if (serdes->sync == 1) {
		v4l2_i2c_subdev_init(&serdes->sd[0], client, &serdes_subdev_ops);
		serdes->sd[0].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		serdes->sd[0].entity.function = MEDIA_ENT_F_CAM_SENSOR;
		v4l2_set_subdev_hostdata(&serdes->sd[0], serdes);
		ret = media_entity_pads_init(&serdes->sd[0].entity,
									 MIPI_CSI2_SENS_VCX_PADS_NUM, serdes->pads);
		if (ret)
			goto mutex_fail;
		ret = v4l2_async_register_subdev(&serdes->sd[0]);
		if (ret) {
			dev_err(&client->dev, "%s %d register subdev failed\n", __func__,
					__LINE__);
			goto free_ctrls;
		}
	} else {
		for (i = 0 ; i < MAX_SENSOR_NUM; i++) {
			if (((serdes->ser_bitmap>>i) & 0x1) ==
				1) { //a camera is available in this channel,thus a subdev is needed here
				v4l2_i2c_subdev_init(&serdes->sd[i], client, &serdes_subdev_ops);
				subdev_of_node = of_graph_get_port_by_id(client->dev.of_node,i);
				serdes->sd[i].fwnode = of_fwnode_handle(subdev_of_node);
				serdes->sd[i].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
				serdes->sd[i].entity.function = MEDIA_ENT_F_CAM_SENSOR;
				v4l2_set_subdev_hostdata(&serdes->sd[i], serdes);
				ret = media_entity_pads_init(&serdes->sd[i].entity,
											 MIPI_CSI2_SENS_VCX_PADS_NUM, serdes->pads);
				if (ret)
					goto mutex_fail;
				ret = v4l2_async_register_subdev(&serdes->sd[i]);
				if (ret) {
					dev_err(&client->dev, "%s %d register subdev failed\n", __func__,
							__LINE__);
					goto free_ctrls;
				}
			}
		}
	}
	INIT_DELAYED_WORK(&serdes->init_work, init_delayed_work);
	INIT_DELAYED_WORK(&serdes->hotplug_work, hotplug_delayed_work);
	return 0;
free_ctrls:
	if (serdes->sync == 1)
		media_entity_cleanup(&serdes->sd[0].entity);
	else {
		for (i = 0 ; i < MAX_SENSOR_NUM; i++) {
			if (((serdes->ser_bitmap>>i) & 0x1) == 1) {
				media_entity_cleanup(&serdes->sd[i].entity);
			}
		}
	}
mutex_fail:
	mutex_destroy(&serdes->lock);
	mutex_destroy(&serdes->lock_hotplug);
	return ret;
}

///////////serdesializer probe
static int serdes_probe(struct i2c_client *client,
					   const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct serdes_dev_t *serdes;
	int ret;
	int i = 0;

	serdes = devm_kzalloc(dev, sizeof(*serdes), GFP_KERNEL);
	if (!serdes)
		return -ENOMEM;
	serdes->i2c_client = client;
	serdes->addr_des = client->addr;
	ret = serdes_parse_dt(serdes, client);
	if (ret < 0) {
		dev_err(dev, "parse dts failed\n");
		return ret;
	}

	dev_info(dev, "module name %s ser addr 0x%x poc addr 0x%x\n",
			 serdes->module_name, serdes->addr_ser, serdes->addr_poc);
	for (i = 0; i < ARRAY_SIZE(g_serdes_config); i++) {
		if (strcmp(serdes->module_name, g_serdes_config[i]->module_name) == 0) {
			serdes->priv = g_serdes_config[i];
			break;
		}
	}
	if (serdes->priv == NULL) {
		dev_err(dev, "no config array detected!\n");
		return -ENODEV;
	}
	ret = serdes_subdev_init(serdes);
	if (ret < 0) {
		return ret;
	}
	schedule_delayed_work(&serdes->init_work,
						  msecs_to_jiffies(serdes->priv->delay_time));
	return 0;
}

int serdes_remove(struct i2c_client *client)
{
	int i = 0;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct serdes_dev_t *serdes = to_serdes_dev(sd);

	mutex_destroy(&serdes->lock);
	if (serdes->sync == 1) {
		media_entity_cleanup(&serdes->sd[0].entity);
		v4l2_async_unregister_subdev(&serdes->sd[0]);
	} else {
		for (i = 0; i < MAX_SENSOR_NUM; i++) {
			if (((serdes->ser_bitmap>>i) & 0x1) == 1) {
				media_entity_cleanup(&serdes->sd[i].entity);
				v4l2_async_unregister_subdev(&serdes->sd[i]);
			}
		}
	}
	cancel_delayed_work_sync(&serdes->init_work);
	cancel_delayed_work_sync(&serdes->hotplug_work);
	return 0;
}

static const struct of_device_id serdes_dt_ids[] = {
	{.compatible = SDRV_SERDES_NAME},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, serdes_dt_ids);

int serdes_pm_suspend(struct device *dev)
{
	pr_err("serdes: suspend\n");
	return 0;
}

int serdes_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct serdes_dev_t *serdes = to_serdes_dev(sd);
	//serdes_para_t *pserdes_para = serdes->priv;
	serdes_para_t *pserdes_para;
	if(NULL == serdes) {
		//pserdes_para = NULL;
		return 0;
	}
	else {
		pserdes_para = serdes->priv;
	}

	if (pserdes_para == NULL)
		return 0;
	mutex_lock(&serdes->lock);
	if (pserdes_para->serdes_init != NULL) {
		pserdes_para->serdes_init(serdes);
		if (serdes->serdes_status.stream_cnt > 0) {
			if (pserdes_para->serdes_start) {
				pserdes_para->serdes_start(serdes, 1);
			}
		}
	}
	mutex_unlock(&serdes->lock);
	pr_err("serdes: resume\n");
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops pm_ops = {
	.suspend = serdes_pm_suspend,
	.resume = serdes_pm_resume,
};
#else
int serdes_dev_suspend(struct device *dev, pm_message_t state)
{
	serdes_pm_suspend(dev);
	return 0;
}
int serdes_dev_resume(struct device *dev)
{
	serdes_pm_resume(dev);
	return 0;
}
#endif // CONFIG_PM

static struct i2c_driver serdes_i2c_driver = {
	.driver = {
		.name = "sdrv-serdes",
		.of_match_table = serdes_dt_ids,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#ifdef CONFIG_PM
		.pm = &pm_ops,
#else
		.pm = NULL,
		.suspend = serdes_dev_suspend,
		.resume = serdes_dev_resume,
#endif // CONFIG_PM
	},
	.probe = serdes_probe,
	.remove = serdes_remove,
};

module_i2c_driver(serdes_i2c_driver);

MODULE_DESCRIPTION("SERDES MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
