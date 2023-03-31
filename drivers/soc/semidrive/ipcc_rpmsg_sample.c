/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/rpmsg.h>

#define RPMSG_TEST_TIMEOUT 100
#define RPMSG_TEST_SENITY_TAG 0xDEADBEEF
#define RPMSG_TEST_CMD_OPEN (0xA0)
#define RPMSG_TEST_CMD_CLOSE (0xB0)
#define RPMSG_TEST_CMD_SETDATA (0xC0)
#define RPMSG_TEST_PARAM1 0x11111111
#define RPMSG_TEST_PARAM2 0x22222222

struct rpmsg_sample_param {
	u32 size;
	void *data;
};

struct rpmsg_sample_header_msg {
	u32 tag;
	u32 command;
	u32 param1;
	u32 param2;
};

/* the user defined command message */
struct rpmsg_sample_cmd_msg {
	struct rpmsg_sample_header_msg header;
	u32 ipc_buf_size;
	dma_addr_t ipc_buf_paddr;
	u32 param_size;
	dma_addr_t param_paddr;
	char name[32];
};

struct rpmsg_sample_cb_msg {
	struct rpmsg_sample_header_msg header;
	int retcode;
	u32 result[4];
};

struct rpmsg_sample_device {
	struct platform_device *pdev;
	struct device *dev;
	struct mutex lock;
	struct workqueue_struct *work_queue;
	struct rpmsg_driver rpmsg_driver;
	struct rpmsg_device *rpmsg_device;
	struct rpmsg_endpoint *channel;

	int cb_err;
	struct completion done;
};

static void build_msg_header(struct rpmsg_sample_device *ctx, int command,
			     struct rpmsg_sample_header_msg *header)
{
	header->tag = RPMSG_TEST_SENITY_TAG;
	header->param1 = RPMSG_TEST_PARAM1;
	header->param2 = RPMSG_TEST_PARAM2;
	header->command = command;
}

int rpmsg_sample_send_sync(struct rpmsg_sample_device *sampdev, int command,
			   struct rpmsg_sample_param *param)
{
	struct rpmsg_device *rpmsg_device = sampdev->rpmsg_device;
	struct rpmsg_sample_cmd_msg msg;
	int ret;

	if (!rpmsg_device) {
		dev_err(sampdev->dev,
			"ipc: failed to open, rpmsg is not initialized\n");
		return -EINVAL;
	}

	init_completion(&sampdev->done);

	/* build rpmsg message */
	build_msg_header(sampdev, command, &msg.header);

	/* send it */
	ret = rpmsg_send(rpmsg_device->ept, &msg, sizeof(msg));
	if (ret) {
		dev_err(sampdev->dev,
			"ipc: failed to open, rpmsg_send failed (%d) for cmd: %d (size=%d, data=%p)\n",
			ret, command, param->size, param->data);
		goto err;
	}

	/* wait for acknowledge */
	if (!wait_for_completion_timeout(
		    &sampdev->done, msecs_to_jiffies(RPMSG_TEST_TIMEOUT))) {
		dev_err(sampdev->dev,
			"ipc: failed to open, timeout waiting for RPMSG_TEST_CMD_OPEN callback (size=%d, data=%p)\n",
			param->size, param->data);
		ret = -ETIMEDOUT;
		goto err;
	}

	/* command completed, check error */
	if (sampdev->cb_err) {
		dev_err(sampdev->dev,
			"ipc: failed to open, RPMSG_TEST_CMD_OPEN completed but with error (%d) (size=%d, data=%p)\n",
			sampdev->cb_err, param->size, param->data);
		ret = -EIO;
		goto err;
	}

	return 0;

err:

	return ret;
};

/* from AP to remote */
static int rpmsg_sample_send_async(struct rpmsg_sample_device *sampdev,
				   int command,
				   struct rpmsg_sample_param *param)
{
	struct rpmsg_device *rpmsg_device = sampdev->rpmsg_device;
	struct rpmsg_sample_cmd_msg msg;
	int ret;

	if (!rpmsg_device) {
		dev_err(sampdev->dev,
			"ipc: failed to open, rpmsg is not initialized\n");
		return -EINVAL;
	}

	/* build rpmsg message */
	build_msg_header(sampdev, command, &msg.header);

	/* send it */
	ret = rpmsg_send(rpmsg_device->ept, &msg, sizeof(msg));
	if (ret) {
		dev_err(sampdev->dev,
			"ipc: failed to open, rpmsg_send failed (%d) for cmd: %d (size=%d, data=%p)\n",
			ret, command, param->size, param->data);
	}

	return ret;
}

int rpmsg_sample_open(struct rpmsg_sample_device *sampdev,
		      struct rpmsg_sample_param *param)
{
	return rpmsg_sample_send_sync(sampdev, RPMSG_TEST_CMD_OPEN, param);
}

int rpmsg_sample_close(struct rpmsg_sample_device *sampdev,
		       struct rpmsg_sample_param *param)
{
	return rpmsg_sample_send_sync(sampdev, RPMSG_TEST_CMD_CLOSE, param);
};

int rpmsg_sample_setdata_async(struct rpmsg_sample_device *sampdev,
			       struct rpmsg_sample_param *param)
{
	return rpmsg_sample_send_async(sampdev, RPMSG_TEST_CMD_SETDATA, param);
};

static int rpmsg_sample_cb(struct rpmsg_device *rpdev, void *data, int len,
			   void *priv, u32 src)
{
	struct rpmsg_sample_device *sampdev = dev_get_drvdata(&rpdev->dev);
	struct rpmsg_sample_cb_msg *msg;

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

	if (len != sizeof(*msg)) {
		dev_err(&rpdev->dev,
			"unexpected message length received from src=%d (received %d bytes while %zu bytes expected)\n",
			len, src, sizeof(*msg));
		return -EINVAL;
	}

	msg = (struct rpmsg_sample_cb_msg *)data;
	if (msg->header.tag != RPMSG_TEST_SENITY_TAG) {
		dev_err(&rpdev->dev,
			"unexpected message tag received from src=%d (received %x tag while %x expected)\n",
			src, msg->header.tag, RPMSG_TEST_SENITY_TAG);
		return -EINVAL;
	}

	/*
	 * all is fine,
	 * update status & complete command
	 */
	sampdev->cb_err = msg->retcode;
	complete(&sampdev->done);

	return 0;
}

static int rpmsg_sample_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_sample_device *sampdev;

	sampdev = devm_kzalloc(&rpdev->dev, sizeof(*sampdev), GFP_KERNEL);
	if (!sampdev)
		return -ENOMEM;

	sampdev->channel = rpdev->ept;
	sampdev->dev = &rpdev->dev;

	sampdev->rpmsg_device = rpdev;
	dev_set_drvdata(&rpdev->dev, sampdev);

	dev_info(&rpdev->dev, "Semidrive Rpmsg sample device\n");

	return 0;
}

static void rpmsg_sample_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_sample_device *sampdev = dev_get_drvdata(&rpdev->dev);

	sampdev->rpmsg_device = NULL;
	dev_set_drvdata(&rpdev->dev, NULL);
}

static struct rpmsg_device_id rpmsg_sample_id_table[] = {
	{ .name = "rpmsg-ipcc-sample" },
	{ .name = "rpmsg-shm-sample" },
	{},
};

static struct rpmsg_driver rpmsg_sample_driver = {
	.probe = rpmsg_sample_probe,
	.remove = rpmsg_sample_remove,
	.callback = rpmsg_sample_cb,
	.id_table = rpmsg_sample_id_table,
	.drv = { .name = "rpmsg-driver-sample" },
};

static int rpmsg_sample_init(void)
{
	int ret;

	ret = register_rpmsg_driver(&rpmsg_sample_driver);
	if (ret < 0) {
		pr_err("rpmsg-test: failed to register rpmsg driver\n");
	}

	pr_err("Semidrive Rpmsg driver for sample device registered\n");

	return ret;
}
subsys_initcall(rpmsg_sample_init);

static void rpmsg_sample_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_sample_driver);
}
module_exit(rpmsg_sample_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_ALIAS("rpmsg:sample");
MODULE_DESCRIPTION("Semidrive Rpmsg sample driver");
MODULE_LICENSE("GPL v2");
