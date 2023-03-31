/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mailbox_client.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

struct vdsp_ipc_device {
	struct device *dev;
	struct mbox_client client;
	struct mbox_chan *mbox;
	int rproc;
	struct work_struct rx_work;

	vdsp_isr_callback cb;
	void	*usrdata;
};

static struct vdsp_ipc_device *vdsp_ipc_dev;
#define VDSP_DEV			(vdsp_ipc_dev)

static int vdsp_send_mbox_msg(struct vdsp_ipc_device *vdsp);

int sd_kick_vdsp(void)
{
	int ret = 0;

	if (xen_initial_domain()) {
		/* mailbox API is not supported in Dom0 */
		pr_err("%s: Dom0 not implemented\n", __func__);
	} else {
		ret = vdsp_send_mbox_msg(VDSP_DEV);
	}

	return ret;
}
EXPORT_SYMBOL(sd_kick_vdsp);

int sd_connect_vdsp(void *hwctx, vdsp_isr_callback isr_cb)
{
	if (xen_initial_domain()) {
		/* mailbox API is not supported in Dom0 */
		pr_err("%s: Dom0 not implemented\n", __func__);
	} else {
		struct vdsp_ipc_device *vdsp = VDSP_DEV;

		if (!vdsp || !vdsp->mbox) {
			/* ERROR: open one mailbox channel*/
			return -ENODEV;
		}

		// called by mailbox irq rx
		vdsp->cb = isr_cb;
		vdsp->usrdata = hwctx;
	}

	return 0;
}
EXPORT_SYMBOL(sd_connect_vdsp);

/* send irq to device */
static int vdsp_send_mbox_msg(struct vdsp_ipc_device *vdsp)
{
	sd_msghdr_t msg;
	int ret;

	if (!vdsp || !vdsp->mbox) {
		return -ENODEV;
	}

	MB_MSG_INIT_VDSP_HEAD(&msg, MB_MSG_HDR_SZ);
	ret = mbox_send_message(vdsp->mbox, &msg); // send no-content mail
	if (ret < 0)
		pr_err("%s: Failed, ret = %d\n", __func__, ret);

	return ret;
}

static void vdsp_work_handler(struct work_struct *work)
{
	//TODO: send vIPC frontent to trigger irq by event channel
	pr_err("%s: Not implemented\n", __func__);
}

static void vdsp_mbox_cb(struct mbox_client *client, void *mssg)
{
	struct vdsp_ipc_device *vdsp = container_of(client, struct vdsp_ipc_device,
						client);
	struct device *dev = client->dev;
	sd_msghdr_t *msghdr = mssg;

	if (!msghdr) {
		dev_err(dev, "%s NULL mssg\n", __func__);
		return;
	}

	if (xen_initial_domain()) {
		/* We are in dom0, call work to trigger vIPC to frontend */
		schedule_work(&vdsp->rx_work);
	} else {
		if (vdsp->cb)
			vdsp->cb(vdsp->usrdata, mssg);
	}

	return;
}

static int vdsp_ipc_probe(struct platform_device *pdev)
{
	struct vdsp_ipc_device *vdsp;
	struct mbox_client *client;
	int ret = 0;

	vdsp = kzalloc(sizeof(struct vdsp_ipc_device), GFP_KERNEL);
	if (!vdsp) {
		dev_err(&pdev->dev, "no memory to probe\n");
		return -ENOMEM;
	}

	INIT_WORK(&vdsp->rx_work, vdsp_work_handler);

	client = &vdsp->client;
	/* Initialize the mbox unit used by rpmsg */
	client->dev = &pdev->dev;
	client->tx_done = NULL;
	client->rx_callback = vdsp_mbox_cb;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	vdsp->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(vdsp->mbox)) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "mbox_request_channel failed: %ld\n", PTR_ERR(vdsp->mbox));
		goto fail_out;
	}
	vdsp->dev = &pdev->dev;
	platform_set_drvdata(pdev, vdsp);
	vdsp_ipc_dev = vdsp;

	dev_info(&pdev->dev, "probed!\n");
	return ret;

fail_out:
	if (vdsp->mbox)
		mbox_free_channel(vdsp->mbox);

	if (vdsp)
		kfree(vdsp);

	return ret;
}

/*
 * Shut down all clients by making sure that each edge stops processing
 * events and scanning for new channels, then call destroy on the devices.
 */
static int vdsp_ipc_remove(struct platform_device *pdev)
{
	int ret;
	struct vdsp_ipc_device *vdsp = platform_get_drvdata(pdev);

	if (!vdsp)
		return -ENODEV;

	device_unregister(&pdev->dev);

	if (vdsp->mbox)
		mbox_free_channel(vdsp->mbox);

	kfree(vdsp);

	return ret;
}

static const struct of_device_id vdsp_ipc_of_ids[] = {
	{ .compatible = "sd,vdsp-ipc"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vdsp_ipc_of_ids);

static struct platform_driver vdsp_ipc_driver = {
	.probe = vdsp_ipc_probe,
	.remove = vdsp_ipc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sd,vdsp-ipc",
		.of_match_table = vdsp_ipc_of_ids,
	},
};

static int __init vdsp_ipc_init(void)
{
	int ret;

	ret = platform_driver_register(&vdsp_ipc_driver);
	if (ret)
		pr_err("Unable to initialize vdsp ipc\n");
	else
		pr_info("Semidrive vdsp ipc is registered.\n");

	return ret;
}
module_init(vdsp_ipc_init);

static void __exit vdsp_ipc_fini(void)
{
	platform_driver_unregister(&vdsp_ipc_driver);
}
module_exit(vdsp_ipc_fini);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive vdsp ipc Driver");
MODULE_LICENSE("GPL v2");

