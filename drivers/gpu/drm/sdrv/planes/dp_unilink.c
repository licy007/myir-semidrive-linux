/*
 * dp_unilink.c
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
#include "sdrv_dpc.h"
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>

#include <linux/init.h>
#include <linux/module.h>


#define RDMA_JUMP           0x1000
#define GP0_JUMP            0x2000
#define GP1_JUMP            0x3000
#define SP0_JUMP            0x5000
#define SP1_JUMP            0x6000
#define MLC_JUMP            0x7000
#define AP_JUMP             0x9000
#define FBDC_JUMP           0xC000

#define USE_HRTIMER_UNI	(1)

static uint32_t dp_irq_handler(struct sdrv_dpc *dpc)
{
	uint32_t sdrv_val = 0;
	//DRM_INFO("%s : unilink dp irq handler \n", __func__);

	sdrv_val |= SDRV_TCON_EOF_MASK;
	return sdrv_val;
}

#if USE_HRTIMER_UNI
static enum hrtimer_restart hrtimer_handler(struct hrtimer *timer)
{
	struct dp_dummy_data *unilink =
	 (struct dp_dummy_data *)container_of(timer, struct dp_dummy_data, timer);

	sdrv_irq_handler(0, unilink->dpc);
	hrtimer_forward_now(timer, unilink->timeout);

	return HRTIMER_RESTART;
}
#endif

static void dp_sw_reset(struct sdrv_dpc *dpc)
{
	return;
}

static void dp_vblank_enable(struct sdrv_dpc *dpc, bool enable)
{
	struct dp_dummy_data *unilink = (struct dp_dummy_data *)dpc->priv;

	if (unilink->vsync_enabled == enable)
		return;

	unilink->vsync_enabled = enable;
}

static void dp_enable(struct sdrv_dpc *dpc, bool enable)
{
	struct dp_dummy_data *unilink = (struct dp_dummy_data *)dpc->priv;

	//TODO socket connect/disconnect
	sdrv_unilink_rpcall_start(dpc, enable);

#if USE_HRTIMER_UNI
	if (enable) {
		hrtimer_start(&unilink->timer, unilink->timeout, HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&unilink->timer);
	}
#endif

}

static void dp_shutdown(struct sdrv_dpc *dpc)
{
	dp_enable(dpc, false);
}

static int dp_update(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count)
{
	int ret;

	//TODO : socket send framebuffer info
	ret = sdrv_unilink_set_frameinfo(dpc, layers, count);
	if (ret) {
		DRM_ERROR("sdrv unilink_set_frameinfo faild \n");
	}

	return ret;

}

static void dp_flush(struct sdrv_dpc *dpc)
{
	//DRM_INFO("flush commit , trigger update data\n");
	return;
}

static void dp_init(struct sdrv_dpc *dpc)
{
	struct dp_dummy_data *unilink;

	if (dpc->priv)
		return;

	dpc->regs = (void __iomem *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 6);
	if (!dpc->regs) {
		DRM_ERROR("kalloc regs memory failed\n");
		return;
	}
	DRM_INFO("set a memory region for dpc->regs: %p\n", dpc->regs);
	// bottom to top
	_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 1, "spipe", SP0_JUMP);

	unilink = devm_kzalloc(&dpc->dev, sizeof(struct dp_dummy_data), GFP_KERNEL);
	if (!unilink) {
		DRM_ERROR("kalloc unilink failed\n");
		return;
	}
	dpc->priv = unilink;
	unilink->dpc = dpc;
	unilink->vsync_enabled = true;

	/*
	 * 1. parse IP and port num from dt
	 * 2. create receiver task for buffer cycling if needed
	 *
	 * Must be called before display sharing -> .init() before .enable()
	 */
	sdrv_unilink_init_socket_info(dpc, dpc->dev.of_node);

#if USE_HRTIMER_UNI
	unilink->timeout = ktime_set(0, 17 * 1000 * 1000);
	hrtimer_init(&unilink->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	unilink->timer.function = hrtimer_handler;
#else
	register_dummy_data(unilink);
#endif
	DRM_INFO("dp unilink init\n");
}

static void dp_uninit(struct sdrv_dpc *dpc)
{
	return;
}

const struct dpc_operation dp_unilink_ops = {
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


