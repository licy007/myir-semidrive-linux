/*
 * sdrv-audio-service.c
 * Copyright (C) 2019 semidrive
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

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/mailbox_client.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/mb_msg.h>

#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>

dev_t devno;
struct class *am_class;
static struct cdev cdev;

#define AM_MINOR 32
#define NUMBER_OF_DEVICES 2
#define TYPE 'S'
#define IOCTL_OP_HANDSHAKE _IOWR(TYPE, 0, int)
#define IOCTL_OP_START _IOWR(TYPE, 1, int)
#define IOCTL_OP_STOP _IOWR(TYPE, 2, int)
#define IOCTL_OP_MUTE _IOWR(TYPE, 3, int)
#define IOCTL_OP_SETVOL _IOWR(TYPE, 4, int)
#define IOCTL_OP_SWITCH _IOWR(TYPE, 5, int)
#define IOCTL_OP_RESET _IOWR(TYPE, 6, int)
#define IOCTL_OP_RESTORE _IOWR(TYPE, 7, int)
#define IOCTL_OP_PLAY_AGENT _IOWR(TYPE, 8, int)
#define IOCTL_OP_PCM_PLAYBACK_CTL _IOWR(TYPE, 9, int)
#define IOCTL_OP_PCM_CAPTURE_CTL _IOWR(TYPE, 10, int)
#define IOCTL_OP_PCM_PLAYBACK_STREAM _IOWR(TYPE, 11, int)
#define IOCTL_OP_PCM_CAPTURE_STREAM _IOWR(TYPE, 12, int)
#define IOCTL_OP_PCM_LOOPBACK_CTL _IOWR(TYPE, 13, int)

enum audiomanager_cmd {
    OP_HANDSHAKE = 0,
    OP_START = 1,
    OP_STOP = 2,
    OP_MUTE = 3,
    OP_SETVOL = 4,
    OP_SWITCH = 5,
    OP_RESET = 6,
    OP_RESTORE = 7,
    OP_PLAY_AGENT = 8,
    OP_PCM_PLAYBACK_CTL = 9,
    OP_PCM_CAPTURE_CTL = 10,
    OP_PCM_PLAYBACK_STREAM = 11,
    OP_PCM_CAPTURE_STREAM = 12,
    OP_PCM_LOOPBACK_CTL = 13,
    OP_NUMB
};

typedef enum {
	IDLE_PATH = 0,
	HIFI_PLAYBACK_TO_MAIN_SPK_48K,
	HIFI_CAPTURE_FROM_MAIN_MIC_48K,
	CLUSTER_PLAYBACK_TO_MAIN_SPK_48K,
	BT_PLAYBACK_TO_MAIN_SPK_16K,
	BT_CAPTURE_FROM_MAIN_MIC_16K,
	BT_PLAYBACK_TO_MAIN_SPK_8K,
	BT_CAPTURE_FROM_MAIN_MIC_8K,
	PATH_TOTAL_NUMB,
	INVALID_PATH_ID = PATH_TOTAL_NUMB,
	INVALID_ORDER_INDEX = PATH_TOTAL_NUMB,
} AM_PATH;

struct audio_rpc_cmd {
	u16 op;
	union {
		u8 data[16];
		struct {
			u32 data;
		} send_msg;
		struct {
			u32 data;
			u32 path;
			u32 vol;
		} recv_msg;
	} msg;
};
struct usr_data {
	u32 data;
	u32 path;
	u32 vol;
};
struct audio_rpc_result {
	u16 op;
	union {
		u8 data[14];
		struct {
			u32 data;
		} fmt;
		struct {
			u32 data;
		} buffer;
	} msg;
};

struct audio_rpc {
	struct device *dev;
	struct mutex lock;
	spinlock_t buf_lock;

	struct mbox_client client;
	struct mbox_chan *mbox;
};

static struct audio_rpc *audio_service_hdr;

static int sdrv_audio_svc_rpcall(struct audio_rpc *audio_rpc_dev,
				 struct audio_rpc_cmd *cmd,
				 struct audio_rpc_result *data)
{
	struct rpc_ret_msg result = {};
	struct rpc_req_msg request;
	struct audio_rpc_cmd *ctl =
	    DCF_RPC_PARAM(request, struct audio_rpc_cmd);
	struct audio_rpc_result *result_priv =
	    (struct audio_rpc_result *)&result.result[0];
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_AUDIO_IOCTL);
	memcpy(ctl, cmd, sizeof(*cmd));
	/* rpcall safety, add rpcall to security in future */
	ret = semidrive_rpcall(&request, &result);

	if (ret < 0 || result.retcode < 0) {
		dev_err(audio_rpc_dev->dev, "rpcall failed=%d %d\n", ret,
			result.retcode);
		goto fail_call;
	}
	memcpy(data, result_priv, sizeof(*result_priv));

fail_call:
	return ret;
}

int sdrv_audio_svc_test(void)
{
	int ret;
	struct audio_rpc_cmd ctl;
	struct audio_rpc_result result;

	ctl.op = OP_PCM_CAPTURE_CTL;
	ret = sdrv_audio_svc_rpcall(audio_service_hdr, &ctl, &result);
	if (ret < 0) {
		dev_err(audio_service_hdr->dev,
			"ioctl test failed. ret code: %d\n", ret);
		return ret;
	}

	/* print value(designated), check the result if succeed */
	dev_err(audio_service_hdr->dev,
		"receive data: %d, %d, %d, %d.\nret code: %d.\n",
		result.msg.data[0], result.msg.data[1], result.msg.data[2],
		result.msg.data[3], ret);
	return 0;
}
EXPORT_SYMBOL(sdrv_audio_svc_test);

static void sdrv_dummy_notify(struct mbox_client *client, void *msg)
{
	struct audio_rpc *dummy_dev =
	    container_of(client, struct audio_rpc, client);
	sd_msghdr_t *msghdr = msg;

	dev_err(dummy_dev->dev, "%s. <head>\n", __func__);
	if (!msghdr) {
		dev_err(dummy_dev->dev, "%s NULL mssg\n", __func__);
		return;
	}
	dev_err(dummy_dev->dev, "dat_len=%d, data[0]=%d, data[1]=%d\n",
		msghdr->dat_len, msghdr->data[0], msghdr->data[1]);
	dev_err(dummy_dev->dev, "%s. <tail>\n", __func__);
	return;
}

static int sdrv_audio_svc_request_irq(struct audio_rpc *audio_rpc_dev)
{
	struct mbox_client *client;
	int ret = 0;

	client = &audio_rpc_dev->client;
	client->dev = audio_rpc_dev->dev;
	client->tx_done = NULL;
	client->rx_callback = sdrv_dummy_notify;
	client->tx_block = true;
	client->tx_tout = 100;
	client->knows_txdone = false;
	audio_rpc_dev->mbox = mbox_request_channel(client, 0);

	if (IS_ERR(audio_rpc_dev->mbox)) {
		ret = -EBUSY;
		dev_err(audio_rpc_dev->dev, "mbox_request_channel failed:%ld\n",
			PTR_ERR(audio_rpc_dev->mbox));
	}

	return ret;
}
static ssize_t am_read(struct file *file, char __user *buf, size_t count,
		       loff_t *ppos)
{
	return -ENOSYS;
}
ssize_t am_write(struct file *file, const char __user *buf, size_t sz,
		 loff_t *oft)
{
	sdrv_audio_svc_test();
	return sz;
}
static int am_open(struct inode *inode, struct file *file)
{
	return 0;
}

long am_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct usr_data data;
	struct audio_rpc_cmd ctl;
	struct audio_rpc_result result;
	if (copy_from_user(&data, (struct msg __user *)arg,
		       sizeof(struct usr_data)))
	{
		dev_err(audio_service_hdr->dev,
				"got data from user failed!\n");
		return -EINVAL;
	}

	switch (cmd)
	{
		case IOCTL_OP_HANDSHAKE:
			ctl.op = OP_HANDSHAKE;
			break;
		case IOCTL_OP_START:
			ctl.op = OP_START;
			break;
		case IOCTL_OP_STOP:
			ctl.op = OP_STOP;
			break;
		case IOCTL_OP_MUTE:
			ctl.op = OP_MUTE;
			break;
		case IOCTL_OP_SETVOL:
			ctl.op = OP_SETVOL;
			break;
		case IOCTL_OP_SWITCH:
			ctl.op = OP_SWITCH;
			break;
		case IOCTL_OP_RESET:
			ctl.op = OP_RESET;
			break;
		case IOCTL_OP_RESTORE:
			ctl.op = OP_RESTORE;
			break;
		case IOCTL_OP_PLAY_AGENT:
			ctl.op = OP_PLAY_AGENT;
			break;
		case IOCTL_OP_PCM_PLAYBACK_CTL:
			ctl.op = OP_PCM_PLAYBACK_CTL;
			break;
		case IOCTL_OP_PCM_CAPTURE_CTL:
			ctl.op = OP_PCM_CAPTURE_CTL;
			break;
		case IOCTL_OP_PCM_PLAYBACK_STREAM:
			ctl.op = OP_PCM_PLAYBACK_STREAM;
			break;
		case IOCTL_OP_PCM_CAPTURE_STREAM:
			ctl.op = OP_PCM_CAPTURE_STREAM;
			break;
		case IOCTL_OP_PCM_LOOPBACK_CTL:
			ctl.op = OP_PCM_LOOPBACK_CTL;
			break;
		default:
			dev_err(audio_service_hdr->dev,
				"am_ioctl unknown cmd: %#x", ctl.op);
			return -EINVAL;
	}
	ctl.msg.recv_msg.data = data.data;
	ctl.msg.recv_msg.path = data.path;
	ctl.msg.recv_msg.vol = data.vol;
	ret = sdrv_audio_svc_rpcall(audio_service_hdr, &ctl, &result);
	if (ret < 0) {
		dev_err(audio_service_hdr->dev,
			"%s :rpcall failed. ret code: %d\n",
			__func__, ret);
		return ret;
	}
	return 0;
}
long am_unlock_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return am_ioctl(file, cmd, arg);
}
static const struct file_operations am_fops = {
    .open = am_open,
    .read = am_read,
    .write = am_write,
    .unlocked_ioctl = am_unlock_ioctl,
    .compat_ioctl = am_ioctl,
    .owner = THIS_MODULE,
};

static int sdrv_audio_service_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct audio_rpc *audio_rpc_dev;
	int ret;
	int major;
	dev_err(dev, "%s: start probe.\n", __func__);

	audio_rpc_dev = devm_kzalloc(dev, sizeof(*audio_rpc_dev), GFP_KERNEL);
	if (!audio_rpc_dev) {
		return -ENOMEM;
	}
	audio_rpc_dev->dev = dev;
	platform_set_drvdata(pdev, audio_rpc_dev);
	audio_service_hdr = audio_rpc_dev;

	sdrv_audio_svc_request_irq(audio_rpc_dev);
	ret = of_property_read_u32(np, "major", &major);
	if (ret < 0) {
		dev_err(dev, "Must config major in dts\n");
		return -ENODEV;
	}
	devno = MKDEV(major, AM_MINOR);
	if (major) {
		ret = register_chrdev_region(devno, NUMBER_OF_DEVICES,
					     "audio_rpc");
	} else {
		ret = alloc_chrdev_region(&devno, 0, NUMBER_OF_DEVICES,
					  "audio_rpc");
	}
	if (ret < 0) {
		dev_err(dev, "%s register audio_rpc error\n", __func__);
		return ret;
	}

	am_class = class_create(THIS_MODULE, "audiomanager");
	if (IS_ERR(am_class)) {
		dev_err(dev, "%s create class error\n", __func__);
		return -1;
	}

	device_create(am_class, NULL, devno, NULL, "audio_rpc");

	cdev_init(&cdev, &am_fops);
	cdev.owner = THIS_MODULE;
	cdev_add(&cdev, devno, NUMBER_OF_DEVICES);
	dev_err(dev, "%s: probed.\n", __func__);
	return ret;
}

static int sdrv_audio_service_remove(struct platform_device *pdev)
{
	struct audio_rpc *audio_rpc_dev;

	audio_rpc_dev = platform_get_drvdata(pdev);
	dev_info(audio_rpc_dev->dev, "removed.\n");

	return 0;
}

static const struct of_device_id sdrv_audio_service_of_match[] = {
    {
	.compatible = "semidrive,audio_rpc_service",
    },
};

static struct platform_driver sdrv_audio_service_driver = {
    .driver =
	{
	    .name = "sdrv_audio_svc",
	    .of_match_table = sdrv_audio_service_of_match,
	},
    .probe = sdrv_audio_service_probe,
    .remove = sdrv_audio_service_remove,
};

module_platform_driver(sdrv_audio_service_driver);

MODULE_ALIAS("platform audio service");
MODULE_DESCRIPTION("Semidrive X9 audio service driver");
MODULE_AUTHOR("Su Zhenxiu <zhenxiu.su@semidrive.com");
MODULE_LICENSE("GPL");
