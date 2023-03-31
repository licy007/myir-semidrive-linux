/*
 * sdrv-i2s-rpmsg.c
 * Copyright (C) 2022 semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "sdrv-i2s-rpmsg.h"

extern sdrv_i2s_rpmsg_dev_t g_rpmsg_device;

static int i2s_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
			void *priv, u32 src)
{
	struct ipcc_data *msg;
	am_stream_ctrl_t *stream_param;
	int stream_type;
	int stream_id;
	struct i2s_remote_info *i2s_info;
	unsigned long flags;
	/* sanity check */
	if (!rpdev) {
		dev_err(NULL, "rpdev is NULL\n");
		return -EINVAL;
	}

	if (!data || !len) {
		dev_err(&rpdev->dev,
			"unexpected empty message received from src=%d\n", src);
		return -EINVAL;
	}

	msg = (struct ipcc_data *)data;
	stream_param = (am_stream_ctrl_t *)&msg->data[0];
	stream_type = stream_param->stream_type;
	stream_id = stream_param->stream_id / SDRV_STREAM_TYPE_NUM;

	if (stream_type >= SDRV_STREAM_TYPE_NUM &&
	    stream_id >= SND_RPMSG_DEVICE_MAX) {
		dev_err(&rpdev->dev,
			"unexpected stream_type =%d,stream_id = %d\n",
			stream_type, stream_id);
		return -EINVAL;
	}

	i2s_info = g_rpmsg_device.rpmsg_i2s_info[stream_id];

	if (i2s_info &&
	    i2s_info->status[stream_type] > AM_STREAM_STATUS_CLOSED) {
		if (stream_param->op_code ==
		    AM_OP_PCM_STREAM_IND) { /* far end response */
			complete(&i2s_info->cmd_complete);
		}

		if (stream_param->op_code ==
			AM_OP_PCM_STREAM_REQ) { /* far end request */

			spin_lock_irqsave(&i2s_info->lock[stream_type], flags);
			i2s_info->buffer_tail[stream_type] =
				stream_param->buffer_tail_ptr;
			spin_unlock_irqrestore(&i2s_info->lock[stream_type],
					       flags);

			if (i2s_info->callback[stream_type]) {
				i2s_info->callback[stream_type](
					(void *)i2s_info, stream_type);
			}
		}
	}

	return 0;
}

static int i2s_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_channel_info chinfo;

	if (!rpdev) {
		pr_err("%s, err: vi2s rpmsg rpdev\n", __func__);
		return -ENODEV;
	}

	g_rpmsg_device.rpdev[g_rpmsg_device.rpmsg_device_cnt] = rpdev;

	strncpy(chinfo.name, "vi2s", RPMSG_NAME_SIZE);
	chinfo.src = SD_VIRI2S_EPT + g_rpmsg_device.rpmsg_device_cnt;
	chinfo.dst = SD_VIRI2S_EPT + g_rpmsg_device.rpmsg_device_cnt;

	rpdev->ept = rpmsg_create_ept(rpdev, i2s_rpmsg_cb, rpdev, chinfo);
	if (!rpdev->ept) {
		pr_err("%s, err: vi2s rpmsg chanel,%p\n", __func__, rpdev->ept);
		return -ENODEV;
	}

	g_rpmsg_device.endpoint[g_rpmsg_device.rpmsg_device_cnt] = rpdev->ept;

	g_rpmsg_device.rpmsg_device_cnt++;

	return 0;
}

static int i2s_rpmsg_device_cb(struct rpmsg_device *rpdev, void *data, int len,
			       void *priv, u32 src)
{
	dev_info(&rpdev->dev, "%s: called\n", __func__);

	return 0;
}

static void i2s_rpmsg_remove(struct rpmsg_device *rpdev)
{
	rpmsg_destroy_ept(rpdev->ept);
	if (g_rpmsg_device.rpmsg_device_cnt > 0) {
		g_rpmsg_device.rpmsg_device_cnt--;
	}
}

static struct rpmsg_device_id i2s_rpmsg_id_table[] = {
	{ .name = "safety,vi2s" },
	{},
};

static struct rpmsg_driver i2s_rpmsg_driver = {
	.drv.name = "rpmsg-vi2s-driver",
	.probe = i2s_rpmsg_probe,
	.remove = i2s_rpmsg_remove,
	.callback = i2s_rpmsg_device_cb,
	.id_table = i2s_rpmsg_id_table,
};

static int sdrv_i2s_rpmsg_init(void)
{
	int ret;

	g_rpmsg_device.rpmsg_device_cnt = 0;

	ret = register_rpmsg_driver(&i2s_rpmsg_driver);
	if (ret < 0) {
		pr_err("rpmsg-vi2s: failed to register rpmsg driver....\n");
	}

	return ret;
};

static void sdrv_i2s_rpmsg_exit(void)
{
	unregister_rpmsg_driver(&i2s_rpmsg_driver);
};

subsys_initcall(sdrv_i2s_rpmsg_init);
module_exit(sdrv_i2s_rpmsg_exit);

MODULE_AUTHOR("Yuansong Jiao <yuansong.jiao@semidrive.com>");
MODULE_DESCRIPTION("X9 rpmsg i2s device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 rpmsg i2s device");
