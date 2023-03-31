/* Copyright (c) 2020, Semidrive Semi.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

struct rpmsg_bl_data {
	struct device	*dev;
	unsigned int	brightness;
	unsigned int	*levels;
	bool			enabled;
	unsigned int	scale;
	unsigned int	bl_screen_id;
};

struct platform_rpmsg_bl_data {
	unsigned int max_brightness;
	unsigned int dft_brightness;
	unsigned int lth_brightness;
	unsigned int *levels;
	unsigned int bl_screen_id;
};

enum backlight_op_type {
	BL_OP_GET_BRIGHT,
	BL_OP_SET_BRIGHT,
};

/* Do not exceed 16 bytes so far */
struct bl_ioctl_cmd {
	u32 op;
	union {
		struct {
			u8 screenid;
			u8 brightness;
		} ctl;
		u8 data[8];
	}u;
};

static int backlight_rpc_set_brightness(struct rpmsg_bl_data *pb, int brightness, int screenid)
{
	struct rpc_ret_msg result = { };
	struct rpc_req_msg request;
	struct bl_ioctl_cmd *ctl = DCF_RPC_PARAM(request, struct bl_ioctl_cmd);
	int ret = 0;
	int retry_count = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_BL_IOCTL);
	ctl->op = BL_OP_SET_BRIGHT;
	ctl->u.ctl.screenid = screenid;
	ctl->u.ctl.brightness = brightness;

retry:
	semidrive_rpcall(&request, &result);
	ret = result.retcode;
	if (ret) {
		dev_err(pb->dev,
			"%s: rpmsg screenid[%d] brightness[%d] set failed\n",
			__func__, screenid, brightness);
		msleep(1000);
		retry_count++;
		if (retry_count < 10)
			goto retry;
		else
			dev_err(pb->dev, "%s : set screenid[%d] brightness[%d] failed, retry count = %d\n",
							__func__, screenid, brightness, retry_count);
	}

	return ret;
}

static void rpmsg_backlight_power_on(struct rpmsg_bl_data *pb, int brightness, int screenid)
{
	backlight_rpc_set_brightness(pb, brightness, screenid);
	pb->brightness = brightness;
	pb->enabled = !!brightness;
	dev_dbg(pb->dev, "%s : screenid[%d] brightness[%d]\n",
		__func__, screenid, brightness);
}

static void rpmsg_backlight_power_off(struct rpmsg_bl_data *pb, int screenid)
{
	if (!pb->enabled)
		return;

	backlight_rpc_set_brightness(pb, 0, screenid);
	pb->brightness = 0;
	pb->enabled = false;
	dev_dbg(pb->dev, "%s : screenid[%d] brightness[%d]\n", __func__, screenid, pb->brightness);
}

static int rpmsg_backlight_update_status(struct backlight_device *bl)
{
	struct rpmsg_bl_data *pb = bl_get_data(bl);
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK ||
		bl->props.fb_blank != FB_BLANK_UNBLANK ||
		bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	if(brightness > bl->props.max_brightness) {
		dev_err(pb->dev, "%s : brightness ERR_INVALID_ARGS! \n", __func__);
		return EINVAL;
	}
	if (brightness > 0) {
		rpmsg_backlight_power_on(pb, brightness, pb->bl_screen_id);
	} else
		rpmsg_backlight_power_off(pb, pb->bl_screen_id);

	return 0;
}

static int rpmsg_backlight_get_brightness(struct backlight_device *bl)
{
	struct rpmsg_bl_data *pb = bl_get_data(bl);
	int brightness = bl->props.brightness;
	struct rpc_ret_msg result = {{0},};
	struct rpc_req_msg request;
	struct bl_ioctl_cmd *ctl = DCF_RPC_PARAM(request, struct bl_ioctl_cmd);
	struct bl_ioctl_cmd *rs = (struct bl_ioctl_cmd *) &result.result[0];

	if (bl->props.power != FB_BLANK_UNBLANK ||
		bl->props.fb_blank != FB_BLANK_UNBLANK ||
		bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_BL_IOCTL);
	ctl->op = BL_OP_GET_BRIGHT;
	ctl->u.ctl.screenid = pb->bl_screen_id;
	semidrive_rpcall(&request, &result);

	brightness = rs->u.ctl.brightness;
	if (brightness > bl->props.max_brightness) {
		dev_err(pb->dev, "%s : brightness ERR_INVALID_ARGS!\n",
			__func__);
		brightness = bl->props.max_brightness;
	}

	return brightness;
}

static int rpmsg_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	return true;
}

static const struct backlight_ops rpmsg_backlight_ops = {
	.update_status	= rpmsg_backlight_update_status,
	.check_fb	= rpmsg_backlight_check_fb,
	.get_brightness = rpmsg_backlight_get_brightness,
};

static int rpmsg_backlight_parse_dt(struct device *dev,
				  struct platform_rpmsg_bl_data *data)
{
	struct device_node *node = dev->of_node;
	struct property *prop;
	int length;
	u32 value;
	int ret;

	if (!node)
		return -ENODEV;

	memset(data, 0, sizeof(*data));

	/*read backlight screen id*/
	ret = of_property_read_u32(node, "bl_screen-id", &data->bl_screen_id);
	if (ret < 0) {
		dev_err(dev, "Missing touchscreen id, use 0\n");
		data->bl_screen_id = 0;
	}

	/* determine the number of brightness levels */
	prop = of_find_property(node, "brightness-levels", &length);
	if (!prop)
		return -EINVAL;

	data->max_brightness = length / sizeof(u32);

	/* read brightness levels from DT property */
	if (data->max_brightness > 0) {
		size_t size = sizeof(*data->levels) * data->max_brightness;

		data->levels = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!data->levels)
			return -ENOMEM;

		ret = of_property_read_u32_array(node, "brightness-levels",
						 data->levels,
						 data->max_brightness);
		if (ret < 0)
			return ret;

		ret = of_property_read_u32(node, "default-brightness-level",
					   &value);
		if (ret < 0)
			return ret;

		data->dft_brightness = value;
		data->max_brightness--;
	}

	return 0;
}

static const struct of_device_id rpmsg_backlight_of_match[] = {
	{ .compatible = "sd,rpmsg-bl" },
	{ }
};

MODULE_DEVICE_TABLE(of, rpmsg_backlight_of_match);

static int rpmsg_backlight_initial_power_state(const struct rpmsg_bl_data *pb)
{
	struct device_node *node = pb->dev->of_node;

	/* Not booted with device tree or no phandle link to the node */
	if (!node || !node->phandle)
		return FB_BLANK_UNBLANK;

	return FB_BLANK_UNBLANK;
}

static int rpmsg_backlight_probe(struct platform_device *pdev)
{
	struct platform_rpmsg_bl_data *data = dev_get_platdata(&pdev->dev);
	struct platform_rpmsg_bl_data defdata;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct rpmsg_bl_data *pb;
	int ret;

	if (!data) {
		ret = rpmsg_backlight_parse_dt(&pdev->dev, &defdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return ret;
		}

		data = &defdata;
	}

	pb = devm_kzalloc(&pdev->dev, sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (data->levels) {
		unsigned int i;

		for (i = 0; i <= data->max_brightness; i++)
			if (data->levels[i] > pb->scale)
				pb->scale = data->levels[i];

		pb->levels = data->levels;
	} else
		pb->scale = data->max_brightness;

	pb->dev = &pdev->dev;
	pb->enabled = false;
	pb->bl_screen_id = data->bl_screen_id;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	dev_info(pb->dev, "dev_name : %s\n", dev_name(&pdev->dev));
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
					   &rpmsg_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
	}

	bl->props.brightness = data->dft_brightness;
	bl->props.power = rpmsg_backlight_initial_power_state(pb);
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
	return 0;

err_alloc:

	return ret;
}

static int rpmsg_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct rpmsg_bl_data *pb = bl_get_data(bl);

	backlight_device_unregister(bl);
	rpmsg_backlight_power_off(pb, pb->bl_screen_id);

	return 0;
}

static void rpmsg_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct rpmsg_bl_data *pb = bl_get_data(bl);

	if (pb != NULL)
		rpmsg_backlight_power_off(pb, pb->bl_screen_id);

}

#ifdef CONFIG_PM_SLEEP
static int sd_rpmsg_bl_suspend(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct rpmsg_bl_data *pb = bl_get_data(bl);
	dev_info(dev, "%s enter ! \n", __func__);

	rpmsg_backlight_power_off(pb, pb->bl_screen_id);

	dev_info(dev, "%s out ! \n", __func__);

	return 0;
}

static int sd_rpmsg_bl_resume(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	dev_info(dev, "%s enter ! \n", __func__);

	backlight_update_status(bl);

	dev_info(dev, "%s out ! \n", __func__);

	return 0;
}

#endif //end #ifdef CONFIG_PM_SLEEP

static const struct dev_pm_ops sd_rpmsg_bl_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sd_rpmsg_bl_suspend,
				sd_rpmsg_bl_resume)
};


static struct platform_driver rpmsg_backlight_driver = {
	.driver		= {
		.name	= "rpmsg-bl",
		.of_match_table	= of_match_ptr(rpmsg_backlight_of_match),
		.pm = &sd_rpmsg_bl_pm_ops,
	},
	.probe		= rpmsg_backlight_probe,
	.remove		= rpmsg_backlight_remove,
	.shutdown	= rpmsg_backlight_shutdown,
};

module_platform_driver(rpmsg_backlight_driver);


MODULE_DESCRIPTION("RPMSG based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ye.liu@semidrive.com");
MODULE_ALIAS("rpmsg:backlight");

