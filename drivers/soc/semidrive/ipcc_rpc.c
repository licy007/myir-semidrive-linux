/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>

#define RPMSG_TEST_TIMEOUT 1000
#define RPMSG_SANITY_TAG 0xdeadbeef

#define RPMSG_RPC_REQ_PING (0x1000)
#define RPMSG_RPC_ACK_PING (RPMSG_RPC_REQ_PING + 1)
#define RPMSG_RPC_REQ_GETTIMEOFDAY (0x1002)
#define RPMSG_RPC_ACK_GETTIMEOFDAY (RPMSG_RPC_REQ_GETTIMEOFDAY + 1)

#define RPC_DELAY_MS 500

struct rpmsg_rpc_device {
	struct device *dev;
	struct mutex lock;
	struct delayed_work delay_work;
	struct rpmsg_device *rpmsg_device;
	struct rpmsg_endpoint *channel;
	int rproc;

	int cb_err;
	struct completion done;
	struct rpc_ret_msg result;
};

static struct rpmsg_rpc_device *rpmsg_rpc_devp[3];
struct rpmsg_rpc_device *rpmsg_get_rpcdev(int dev_id);

static int rpmsg_rpc_call(struct rpmsg_rpc_device *rpcdev,
			  struct rpc_req_msg *req, struct rpc_ret_msg *result,
			  int timeout)
{
	struct rpmsg_device *rpmsg_device = rpcdev->rpmsg_device;
	int ret;

	if (!rpmsg_device) {
		dev_err(rpcdev->dev,
			"failed to open, rpmsg is not initialized\n");
		return -EINVAL;
	}

	mutex_lock(&rpcdev->lock);
	init_completion(&rpcdev->done);

	/* send it */
	ret = rpmsg_send(rpmsg_device->ept, req, sizeof(*req));
	if (ret) {
		dev_err(rpcdev->dev, "failed to request ret=%d cmd=%x\n", ret,
			req->cmd);
		goto err;
	}

	/* wait for acknowledge */
	if (!wait_for_completion_timeout(&rpcdev->done,
					 msecs_to_jiffies(timeout))) {
		dev_err(rpcdev->dev, "wait reply timeout %x\n", req->cmd);
		ret = -ETIMEDOUT;
		goto err;
	}

	if (result)
		memcpy(result, &rpcdev->result, sizeof(*result));

	dev_dbg(rpcdev->dev, "succeed to call RPC %x\n", req->cmd);

err:
	mutex_unlock(&rpcdev->lock);

	return ret;
}

int rpmsg_rpc_call_trace(int dev, struct rpc_req_msg *req,
			 struct rpc_ret_msg *result)
{
	int ret = 0;
	struct rpmsg_rpc_device *rpcdev;

	rpcdev = rpmsg_get_rpcdev(dev);
	if (!rpcdev)
		return -ENODEV;

	ret = rpmsg_rpc_call(rpcdev, req, result, 1000);
	if (ret < 0) {
		dev_err(rpcdev->dev, "rpc: req=%x fail ret=%d\n", req->cmd,
			ret);
		return ret;
	}

	/* command completed, check error */
	ret = result->retcode;
	if (ret < 0)
		dev_warn(rpcdev->dev, "rpc: req=%x retcode=%d\n", req->cmd,
			 result->retcode);

	return ret;
}

int rpmsg_rpc_ping(struct rpmsg_rpc_device *rpcdev)
{
	struct rpc_ret_msg result = {
		0,
	};
	struct rpc_req_msg request;
	int ret;

	DCF_INIT_RPC_REQ(request, RPMSG_RPC_REQ_PING);
	ret = rpmsg_rpc_call(rpcdev, &request, &result, RPMSG_TEST_TIMEOUT);
	if (ret < 0) {
		dev_err(rpcdev->dev, "rpc: call_func:%x fail ret: %d\n",
			request.cmd, ret);
		return ret;
	}

	ret = result.retcode;
	dev_dbg(rpcdev->dev, "rpc: get result: %x %d %x %x %x\n", result.ack,
		ret, result.result[0], result.result[1], result.result[2]);

	if ((result.ack != RPMSG_RPC_ACK_PING) || ret)
		dev_warn(rpcdev->dev, "rpc: exec bad result %d %d\n",
			 result.ack, ret);

	if (memcmp((char *)request.param, (char *)result.result,
		   sizeof(result.result)))
		dev_warn(rpcdev->dev, "rpc: echo not match\n");

	return ret;
}

int rpmsg_rpc_gettimeofday(struct rpmsg_rpc_device *rpcdev)
{
	struct rpc_ret_msg result = {
		0,
	};
	struct rpc_req_msg request;
	int ret;

	DCF_INIT_RPC_REQ(request, RPMSG_RPC_REQ_GETTIMEOFDAY);
	ret = rpmsg_rpc_call(rpcdev, &request, &result, RPMSG_TEST_TIMEOUT);
	if (ret < 0) {
		dev_err(rpcdev->dev, "rpc: call-func:%x fail ret: %d\n",
			request.cmd, ret);
		return ret;
	}
	ret = result.retcode;
	dev_dbg(rpcdev->dev, "rpc: get result: %x %d %x %x %x\n", result.ack,
		ret, result.result[0], result.result[1], result.result[2]);

	if ((result.ack != RPMSG_RPC_ACK_GETTIMEOFDAY) || ret)
		dev_warn(rpcdev->dev, "rpc: exec bad result %d %d\n",
			 result.ack, ret);

	return ret;
}

static int rpmsg_rpcdev_cb(struct rpmsg_device *rpdev, void *data, int len,
			   void *priv, u32 src)
{
	struct rpmsg_rpc_device *rpcdev = dev_get_drvdata(&rpdev->dev);
	struct rpc_ret_msg *result;

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

	if (len != sizeof(*result)) {
		dev_err(&rpdev->dev,
			"unexpected message length received from src=%d (received %d bytes while %zu bytes expected)\n",
			len, src, sizeof(*result));
		return -EINVAL;
	}

	memcpy(&rpcdev->result, data, sizeof(*result));

	/*
	 * all is fine,
	 * update status & complete command
	 */
	complete(&rpcdev->done);

	return 0;
}

static void rpc_work_handler(struct work_struct *work)
{
	struct rpmsg_rpc_device *rpcdev = container_of(
		to_delayed_work(work), struct rpmsg_rpc_device, delay_work);
	dc_state_t dc_status;
	int ret;
	ktime_t tss, tse;

	tss = ktime_get_boottime();
	ret = rpmsg_rpc_ping(rpcdev);
	if (ret == 0) {
		tse = ktime_get_boottime();
		dev_info(rpcdev->dev, "RPC is on, latency %lld\n",
			 ktime_us_delta(tse, tss));
	}

	if (rpcdev->rproc == 0) {
		ret = sd_get_dc_status(&dc_status);
		if (ret == 0) {
			dev_info(rpcdev->dev, "dc_status is 0x%x\n", dc_status);
		}
	}
}

#define IPCC_RPC_DEVNAME_PREFIX "soc:ipcc@"
static int register_rpcdev(struct rpmsg_rpc_device *rpcdev)
{
	char *name, *bufp, *p;
	int id = -1;
	int ret = 0;

	name = strstr(dev_name(rpcdev->dev), IPCC_RPC_DEVNAME_PREFIX);
	bufp = kstrdup(name, GFP_KERNEL);
	if (bufp) {
		p = bufp + strlen(IPCC_RPC_DEVNAME_PREFIX);
		p[1] = 0;
		ret = kstrtoint(p, 10, &id);
	}

	if (ret < 0 || id < 0) {
		dev_err(rpcdev->dev, "Bad rpcdev name %s\n", bufp);
	} else {
		rpcdev->rproc = id;
		rpmsg_rpc_devp[id] = rpcdev;
	}

	if (bufp)
		kfree(bufp);

	return 0;
}

struct rpmsg_rpc_device *rpmsg_get_rpcdev(int dev_id)
{
	if (dev_id < ARRAY_SIZE(rpmsg_rpc_devp)) {
		return rpmsg_rpc_devp[dev_id];
	}

	return NULL;
}

static int rpmsg_rpcdev_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_rpc_device *rpcdev;

	rpcdev = devm_kzalloc(&rpdev->dev, sizeof(*rpcdev), GFP_KERNEL);
	if (!rpcdev)
		return -ENOMEM;

	rpcdev->channel = rpdev->ept;
	rpcdev->dev = &rpdev->dev;

	mutex_init(&rpcdev->lock);
	rpcdev->rpmsg_device = rpdev;
	dev_set_drvdata(&rpdev->dev, rpcdev);

	register_rpcdev(rpcdev);

	/* just async call RPC */
	INIT_DELAYED_WORK(&rpcdev->delay_work, rpc_work_handler);
	schedule_delayed_work(&rpcdev->delay_work,
			      msecs_to_jiffies(RPC_DELAY_MS));

	/* TODO: add kthread to serve RPC request */
	dev_info(&rpdev->dev, "Semidrive Rpmsg RPC device probe!\n");

	return 0;
}

static void rpmsg_rpcdev_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_rpc_device *rpcdev = dev_get_drvdata(&rpdev->dev);

	rpcdev->rpmsg_device = NULL;
	dev_set_drvdata(&rpdev->dev, NULL);
}

static struct rpmsg_device_id rpmsg_rpcdev_id_table[] = {
	{ .name = "rpmsg-ipcc-rpc" },
	{},
};

static struct rpmsg_driver rpmsg_rpcdev_driver = {
	.probe = rpmsg_rpcdev_probe,
	.remove = rpmsg_rpcdev_remove,
	.callback = rpmsg_rpcdev_cb,
	.id_table = rpmsg_rpcdev_id_table,
	.drv = { .name = "rpmsg-rpc" },
};

static int rpmsg_rpcdev_init(void)
{
	int ret;

	ret = register_rpmsg_driver(&rpmsg_rpcdev_driver);
	if (ret < 0) {
		pr_err("rpmsg-test: failed to register rpmsg driver\n");
	}

	pr_info("Semidrive Rpmsg driver for RPC device registered\n");

	return ret;
}
arch_initcall(rpmsg_rpcdev_init);

static void rpmsg_rpcdev_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_rpcdev_driver);
}
module_exit(rpmsg_rpcdev_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_ALIAS("rpmsg:rpcdev");
MODULE_DESCRIPTION("Semidrive Rpmsg RPC kernel mode driver");
MODULE_LICENSE("GPL v2");
