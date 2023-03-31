/*
 * Copyright (C) Semidrive Semiconductor Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <asm/unaligned.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <xen/xen.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

#define STS_MAX_HEIGHT		4096
#define STS_MAX_WIDTH		4096
#define STS_CONTACT_SIZE	8
#define STS_MAX_CONTACTS	10

#define STS_VERSION	0xff
#define STS_ID		0xff
#define STS_VENDOR	0xff

#define TS_SUPPORT_ANRDOID_MAIN (1<<0)
#define TS_SUPPORT_ANRDOID_AUX1 (1<<1)
#define TS_SUPPORT_ANRDOID_AUX2 (1<<2)
#define TS_SUPPORT_ANRDOID_AUX3 (1<<3)

struct ts_coord_data {
	u8 id;
	u16 x;
	u16 y;
	u16 w;
};

struct ts_report_data {
	u8 key_value;
	u8 touch_num;
	struct ts_coord_data coord_data[10];
};

struct semidrive_sts_data {
	struct platform_device *pdev;
	struct mbox_client client;
	struct mbox_chan *mbox;
	struct input_dev *input_dev;
	struct delayed_work ts_work;
	int ts_work_count;
	struct delayed_work ts_resume_work;
	int ts_resume_count;
	bool read_config_stat;
	bool set_init_stat;
	int abs_x_max;
	int abs_y_max;
	u16 id;
	u16 version;
	u16 vendor;
	u16 instance;
};

enum sts_op_type {
	STS_OP_GET_CONFIG,
	STS_OP_SET_INITED,
};

/* Do not exceed 16 bytes so far */
struct sts_ioctl_cmd {
	u16 op;
	u16 instance;
	union {
		struct {
			u16 what;
			u16 why;
			u32 how;
		} s;
	} msg;
};

/* Do not exceed 16 bytes so far */
struct sts_ioctl_result {
	u16 op;
	u16 instance;
	union {
		/** used for get config */
		struct {
			u16 version;
			u16 id;
			u16 vendor;
			u16 abs_x_max;
			u16 abs_y_max;
		} cfg;
		u8 data[12];
	} msg;
};

static void
semidrive_process_events(struct semidrive_sts_data *ts, u8 *point_data)
{
	int i;
	struct ts_report_data *report_data = (struct ts_report_data *)point_data;

	input_report_key(ts->input_dev, KEY_LEFTMETA, report_data->key_value);

	for (i = 0; i < report_data->touch_num; i++) {
		int id = report_data->coord_data[i].id;
		int input_x = report_data->coord_data[i].x;
		int input_y = report_data->coord_data[i].y;
		int input_w = report_data->coord_data[i].w;

		if ((input_x < 0) || (input_x > ts->abs_x_max))
			return;
		if ((input_y < 0) || (input_y > ts->abs_y_max))
			return;

		input_mt_slot(ts->input_dev, id);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, input_w);
	}

	input_mt_sync_frame(ts->input_dev);
	input_sync(ts->input_dev);
}

static void semidrive_free_channel(struct semidrive_sts_data *ts)
{
	mbox_free_channel(ts->mbox);
}

static void sts_coor_updated(struct mbox_client *client, void *mssg)
{
	struct semidrive_sts_data *ts = container_of(client,
				struct semidrive_sts_data, client);
	struct platform_device *pdev = ts->pdev;
	sd_msghdr_t *msghdr = mssg;

	if (!msghdr) {
		dev_err(&pdev->dev, "%s NULL mssg\n", __func__);
		return;
	}

	semidrive_process_events(ts, msghdr->data);
}

static int semidrive_request_channel(struct semidrive_sts_data *ts)
{
	struct platform_device *pdev = ts->pdev;
	struct mbox_client *client;
	int ret = 0;

	client = &ts->client;
	/* Initialize the mbox channel used by touchscreen */
	client->dev = &pdev->dev;
	client->tx_done = NULL;
	client->rx_callback = sts_coor_updated;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	ts->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(ts->mbox)) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "mbox_request_channel failed: %ld\n",
				PTR_ERR(ts->mbox));
	}

	return ret;
}

static int semidrive_sts_ioctl(struct semidrive_sts_data *ts, int command,
	struct sts_ioctl_result *data)
{
	struct rpc_ret_msg result = {{0},};
	struct rpc_req_msg request;
	struct sts_ioctl_cmd *ctl = DCF_RPC_PARAM(request, struct sts_ioctl_cmd);
	struct sts_ioctl_result *rs = (struct sts_ioctl_result *) &result.result[0];
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_STS_IOCTL);
	ctl->op = command;
	ctl->instance = ts->instance;
	ret = semidrive_rpcall(&request, &result);
	if (ret < 0 || result.retcode < 0) {
		if (result.retcode != -27)
			dev_err(&ts->pdev->dev, "rpcall failed=%d %d\n", ret, result.retcode);
		goto fail_call;
	}

	memcpy(data, rs, sizeof(*rs));

fail_call:

	return ret;
}

static int semidrive_set_inited(struct semidrive_sts_data *ts)
{
	struct sts_ioctl_result op_ret;

	return semidrive_sts_ioctl(ts, STS_OP_SET_INITED, &op_ret);
}

/**
 * semidrive_read_config - Read the embedded configuration of the panel
 *
 * @ts: our semidrive_sts_data pointer
 *
 * Must be called during probe
 */
static int semidrive_read_config(struct semidrive_sts_data *ts)
{
	struct sts_ioctl_result op_ret;
	int ret;

	ret = semidrive_sts_ioctl(ts, STS_OP_GET_CONFIG, &op_ret);
	if (ret) {
		ts->version = STS_VERSION;
		ts->id = STS_ID;
		ts->vendor = STS_VENDOR;
		ts->abs_x_max = STS_MAX_WIDTH;
		ts->abs_y_max = STS_MAX_HEIGHT;
		dev_err(&ts->pdev->dev, "get config fail,use default\n");
	} else {
		ts->version = op_ret.msg.cfg.version;
		ts->id = op_ret.msg.cfg.id;
		ts->vendor = op_ret.msg.cfg.vendor;

		ts->abs_x_max = op_ret.msg.cfg.abs_x_max;
		if (!ts->abs_x_max) {
			ret = -1;
			ts->abs_x_max = STS_MAX_WIDTH;
			dev_err(&ts->pdev->dev, "invalid x_max,use default\n");
		}

		ts->abs_y_max = op_ret.msg.cfg.abs_y_max;
		if (!ts->abs_y_max) {
			ret = -1;
			ts->abs_y_max = STS_MAX_HEIGHT;
			dev_err(&ts->pdev->dev, "invalid y_max,use default\n");
		}
	}

	return ret;
}

/**
 * semidrive_register_input_dev - Allocate, populate and register the input device
 *
 * @ts: our semidrive_sts_data pointer
 *
 * Must be called during probe
 */
static int semidrive_register_input_dev(struct semidrive_sts_data *ts)
{
	int ret = 0;

	ts->input_dev = devm_input_allocate_device(&ts->pdev->dev);
	if (!ts->input_dev) {
		dev_err(&ts->pdev->dev, "Failed to allocate input device.");
		return -ENOMEM;
	}

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
		0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
		0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
		0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
		0, 255, 0, 0);

	input_mt_init_slots(ts->input_dev, STS_MAX_CONTACTS,
				INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);

	ts->input_dev->name = "Semidrive Safe TouchScreen";
	if (ts->instance == TS_SUPPORT_ANRDOID_MAIN)
		ts->input_dev->phys = "input/ts_main";
	else if(ts->instance == TS_SUPPORT_ANRDOID_AUX1)
		ts->input_dev->phys = "input/ts_aux";
	else if(ts->instance == TS_SUPPORT_ANRDOID_AUX2)
		ts->input_dev->phys = "input/ts_aux1";
	else if(ts->instance == TS_SUPPORT_ANRDOID_AUX3)
		ts->input_dev->phys = "input/ts_aux2";

	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = ts->vendor;
	ts->input_dev->id.product = ts->id;
	ts->input_dev->id.version = ts->version;

	/* Capacitive Windows/Home button on some devices */
	input_set_capability(ts->input_dev, EV_KEY, KEY_LEFTMETA);
	ret = input_register_device(ts->input_dev);
	if (ret)
		dev_err(&ts->pdev->dev, "Failed to register input device: %d", ret);

	return ret;
}

/**
 * semidrive_configure_dev - Finish device initialization
 *
 * @ts: our semidrive_sts_data pointer
 *
 * Must be called from probe to finish initialization of the device.
 * Contains the common initialization code for both devices that
 * declare gpio pins and devices that do not. It is either called
 * directly from probe or from request_firmware_wait callback.
 */
static int semidrive_configure_dev(struct semidrive_sts_data *ts)
{
	//struct platform_device *pdev = ts->pdev;
	int ret = 0;

	ret = semidrive_register_input_dev(ts);
	if (ret)
		return ret;

	ret = semidrive_request_channel(ts);

	return ret;
}

static void sts_resume_work_handler(struct work_struct *work)
{
	int ret;
	struct semidrive_sts_data *ts =
		container_of(work, struct semidrive_sts_data, ts_resume_work.work);

	ret = semidrive_set_inited(ts);
	if (!ret || ts->ts_resume_count >= 10) {
		dev_err(&ts->pdev->dev, "sts resume exit ret=%d, cnt=%d\n",
			ret, ts->ts_resume_count);
		ts->ts_resume_count = 0;
		cancel_delayed_work(&ts->ts_resume_work);
	} else {
		ts->ts_resume_count++;
		schedule_delayed_work(&ts->ts_resume_work, msecs_to_jiffies(200));
	}
}

static void sts_work_handler(struct work_struct *work)
{
	int ret;

	struct semidrive_sts_data *ts =
		container_of(work, struct semidrive_sts_data, ts_work.work);

	if (!ts->read_config_stat) {
		ret = semidrive_read_config(ts);
		if (!ret)
			ts->read_config_stat = true;
	}

	if (!ts->set_init_stat) {
		ret = semidrive_set_inited(ts);
		if (!ret)
			ts->set_init_stat = true;
	}

	ts->ts_work_count++;

	if (ts->read_config_stat && ts->set_init_stat) {
		cancel_delayed_work(&ts->ts_work);

		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			0, ts->abs_x_max, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			0, ts->abs_y_max, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
			0, 255, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
			0, 255, 0, 0);

		input_mt_init_slots(ts->input_dev, STS_MAX_CONTACTS,
					INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);

		ts->input_dev->id.bustype = BUS_I2C;
		ts->input_dev->id.vendor = ts->vendor;
		ts->input_dev->id.product = ts->id;
		ts->input_dev->id.version = ts->version;

	} else if (ts->ts_work_count >= 10) {
		cancel_delayed_work(&ts->ts_work);
		dev_err(&ts->pdev->dev, "cstat=%d, istat=%d\n",
			ts->read_config_stat, ts->set_init_stat);
	} else {
		schedule_delayed_work(&ts->ts_work, msecs_to_jiffies(500));
	}
}

static int semidrive_sts_probe(struct platform_device *pdev)
{
	struct semidrive_sts_data *ts;
	int ret;
	u16 instance;

	ts = devm_kzalloc(&pdev->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->pdev = pdev;
	platform_set_drvdata(pdev, ts);

	INIT_DELAYED_WORK(&ts->ts_resume_work, sts_resume_work_handler);
	INIT_DELAYED_WORK(&ts->ts_work, sts_work_handler);

	/* Read instance from dts */
	ret = of_property_read_u16(pdev->dev.of_node, "touchscreen-id", &instance);
	if (ret < 0) {
		dev_err(&pdev->dev, "Warning: Missing touchscreen id\n");
		ts->instance = TS_SUPPORT_ANRDOID_MAIN;
	} else {
		if (instance == 0)
			ts->instance = TS_SUPPORT_ANRDOID_MAIN;
		else if (instance == 1)
			ts->instance = TS_SUPPORT_ANRDOID_AUX1;
		else if (instance == 2)
			ts->instance = TS_SUPPORT_ANRDOID_AUX2;
		else if (instance == 3)
			ts->instance = TS_SUPPORT_ANRDOID_AUX3;
	}

	ret = semidrive_read_config(ts);
	if (ret)
		dev_err(&pdev->dev, "%s, Read config failed %d\n", __func__, ret);
	else
		ts->read_config_stat = true;

	ret = semidrive_configure_dev(ts);
	if (ret) {
		dev_err(&pdev->dev, "configure dev failed %d\n", ret);
		goto err;
	}

	ret = semidrive_set_inited(ts);
	if (ret)
		dev_err(&pdev->dev, "%s, set init failed %d\n", __func__, ret);
	else
		ts->set_init_stat = true;

	if (!ts->read_config_stat || !ts->set_init_stat)
		schedule_delayed_work(&ts->ts_work, msecs_to_jiffies(500));
	else
		dev_err(&pdev->dev, "%s(): done ok\n", __func__);

	return 0;

err:
	devm_kfree(&pdev->dev, ts);
	return ret;
}

static int semidrive_sts_remove(struct platform_device *pdev)
{
	struct semidrive_sts_data *ts = platform_get_drvdata(pdev);

	semidrive_free_channel(ts);
	return 0;
}

static int __maybe_unused semidrive_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused semidrive_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct semidrive_sts_data *ts = platform_get_drvdata(pdev);

	schedule_delayed_work(&ts->ts_resume_work, msecs_to_jiffies(200));
	return 0;
}

static SIMPLE_DEV_PM_OPS(semidrive_pm_ops, semidrive_suspend, semidrive_resume);

static const struct platform_device_id semidrive_sts_id[] = {
	{ "sd,safetouch", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, semidrive_sts_id);

static const struct of_device_id semidrive_of_match[] = {
	{ .compatible = "sd,safetouch" },
	{ }
};
MODULE_DEVICE_TABLE(of, semidrive_of_match);

static struct platform_driver semidrive_sts_driver = {
	.probe = semidrive_sts_probe,
	.remove = semidrive_sts_remove,
	.id_table = semidrive_sts_id,
	.driver = {
		.name = "semidrive,safets",
		.of_match_table = of_match_ptr(semidrive_of_match),
		.pm = &semidrive_pm_ops,
	},
};
module_platform_driver(semidrive_sts_driver);

MODULE_AUTHOR("<ye.liu@semidrive.com>");
MODULE_DESCRIPTION("Semidrive Safe TouchScreen driver");
MODULE_LICENSE("GPL v2");

