/*
 * dp_rpcall.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_plane_helper.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/soc/semidrive/ipcc.h>
#include "sdrv_dpc.h"

#include <linux/mailbox_client.h>
#include <linux/soc/semidrive/mb_msg.h>

#define RDMA_JUMP           0x1000
#define GP0_JUMP            0x2000
#define GP1_JUMP            0x3000
#define SP0_JUMP            0x5000
#define SP1_JUMP            0x6000
#define MLC_JUMP            0x7000
#define AP_JUMP             0x9000
#define FBDC_JUMP           0xC000

#define USE_FREERTOS_RPCALL 1

struct dp_rpcall_data {
	// struct hrtimer vsync_timer;
	struct work_struct rx_work;
	struct mbox_client client;
	struct mbox_chan *mbox;
	struct delayed_work fake_vsync_work;
	bool vsync_enabled;
	struct sdrv_dpc *dpc;
};

static uint32_t dp_irq_handler(struct sdrv_dpc *dpc)
{
	uint32_t sdrv_val = 0;

	sdrv_val |= SDRV_TCON_EOF_MASK;

	return sdrv_val;
}

static void dp_sw_reset(struct sdrv_dpc *dpc)
{

}

static void dp_vblank_enable(struct sdrv_dpc *dpc, bool enable)
{
	struct dp_rpcall_data *rpcall = (struct dp_rpcall_data *)dpc->priv;
	if (rpcall->vsync_enabled == enable)
		return;
	PRINT("vblank enable---: %d", enable);
	rpcall->vsync_enabled = enable;

}

static void dp_enable(struct sdrv_dpc *dpc, bool enable)
{
		PRINT("dp_enable enable--- %d", enable);

		sdrv_rpcall_start(enable);
}

static void dp_shutdown(struct sdrv_dpc *dpc)
{
	dp_enable(dpc, false);
}

static int dp_update(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count) {
#if USE_FREERTOS_RPCALL
	return sdrv_set_frameinfo(dpc, layers, count);
#else
	return 0;
#endif
}

static void dp_flush(struct sdrv_dpc *dpc)
{
	// DRM_INFO("flush commit , trigger update data\n");
}

static void rpcall_work_handler(struct work_struct *work)
{
	//TODO: send frontent to trigger irq by event channel
	pr_err("%s: Not implemented\n", __func__);
}

static int __maybe_unused rpcall_fps(void) {
    static u64 last_time;
    static int fps_value = 0;
    static int frame_count = 0;

	u64 now = ktime_get_boot_ns() / 1000 / 1000;
    frame_count++;
    if (now - last_time > 1000) {
        fps_value = frame_count;
        frame_count = 0;
        last_time = now;
        DRM_INFO("rpcall disp fps = %d\n", fps_value);
    }
    return fps_value;
}

static void rpcall_vsync_updated(struct mbox_client *client, void *mssg)
{
	struct dp_rpcall_data *rpcall = container_of(client,
						struct dp_rpcall_data, client);

	sd_msghdr_t *msghdr = mssg;

	if (!msghdr) {
		DRM_ERROR("%s NULL mssg\n", __func__);
		return;
	}
	if (rpcall->vsync_enabled)
		sdrv_irq_handler(0, rpcall->dpc);

	return;
}

static int rpcall_request_irq(struct dp_rpcall_data *rpcall)
{
	struct mbox_client *client;
	int ret;

	INIT_WORK(&rpcall->rx_work, rpcall_work_handler);

	client = &rpcall->client;
	/* Initialize the mbox channel used by touchscreen */
	client->dev = &rpcall->dpc->dev;
	client->tx_done = NULL;
	client->rx_callback = rpcall_vsync_updated;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	rpcall->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(rpcall->mbox)) {
		ret = -EBUSY;
		DRM_ERROR("mbox_request_channel failed: %ld\n",
				PTR_ERR(rpcall->mbox));
	} else
	DRM_INFO("mbox register success on device %s\n",  client->dev->of_node->name);
	return ret;
}


static void dp_init(struct sdrv_dpc *dpc)
{
	struct dp_rpcall_data *rpcall;

	if (dpc->priv)
		return;
	dpc->regs = (void __iomem *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 6);
	if (!dpc->regs) {
		DRM_ERROR("kalloc regs memory failed\n");
		return;
	}
	PRINT("set a memory region for dpc->regs: %p\n", dpc->regs);
	dpc->irq = 0;

	// bottom to top
	_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 1, "spipe", SP0_JUMP);

	rpcall = devm_kzalloc(&dpc->dev, sizeof(struct dp_rpcall_data), GFP_KERNEL);
	if (!rpcall) {
		DRM_ERROR("kalloc rpcall failed\n");
		return;
	}
	dpc->priv = rpcall;
	rpcall->dpc = dpc;

	rpcall_request_irq(rpcall);

	rpcall->vsync_enabled = true;
}

static void dp_uninit(struct sdrv_dpc *dpc)
{
	ENTRY();
}

const struct dpc_operation dp_rpcall_ops = {
	.init = dp_init,
	.uninit = dp_uninit,
	.irq_handler = dp_irq_handler,
	.vblank_enable = dp_vblank_enable,
	.enable = dp_enable,
	.update = dp_update,
	.shutdown = dp_shutdown,
	.flush = dp_flush,
	.sw_reset = dp_sw_reset,
	.modeset = NULL,
};


