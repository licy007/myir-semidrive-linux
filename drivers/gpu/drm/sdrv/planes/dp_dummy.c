/*
 * dp_dummy.c
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

#define RDMA_JUMP           0x1000
#define GP0_JUMP            0x2000
#define GP1_JUMP            0x3000
#define SP0_JUMP            0x5000
#define SP1_JUMP            0x6000
#define MLC_JUMP            0x7000
#define AP_JUMP             0x9000
#define FBDC_JUMP           0xC000

#define USE_FREERTOS_RPCALL (1)
#define USE_HRTIMER	(1)
int dummy_enable = 0;

static uint32_t dp_irq_handler(struct sdrv_dpc *dpc)
{
	uint32_t sdrv_val = 0;

	sdrv_val |= SDRV_TCON_EOF_MASK;
	return sdrv_val;
}
#if USE_HRTIMER
static enum hrtimer_restart hrtimer_handler(struct hrtimer *timer)
{
	struct dp_dummy_data *dummy =
	 (struct dp_dummy_data *)container_of(timer, struct dp_dummy_data, timer);

	sdrv_irq_handler(0, dummy->dpc);
	hrtimer_forward_now(timer, dummy->timeout);

	return HRTIMER_RESTART;
}
#endif
static void dp_sw_reset(struct sdrv_dpc *dpc)
{

}

static void dp_vblank_enable(struct sdrv_dpc *dpc, bool enable)
{
	struct dp_dummy_data *dummy = (struct dp_dummy_data *)dpc->priv;
	if (dummy->vsync_enabled == enable)
		return;

	dummy->vsync_enabled = enable;
}

static void dp_enable(struct sdrv_dpc *dpc, bool enable)
{
	struct dp_dummy_data *dummy = (struct dp_dummy_data *)dpc->priv;
#if USE_FREERTOS_RPCALL
	sdrv_rpcall_start(enable);

	PRINT("dp enable [%d]---\n", enable);
#endif
#if USE_HRTIMER
	if (enable) {
		hrtimer_start(&dummy->timer, dummy->timeout, HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&dummy->timer);
	}
#endif
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
	//DRM_INFO("flush commit , trigger update data\n");
}

static void dp_init(struct sdrv_dpc *dpc)
{
	struct dp_dummy_data *dummy;

	if (dpc->priv)
		return;

	dpc->regs = (void __iomem *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 6);
	if (!dpc->regs) {
		DRM_ERROR("kalloc regs memory failed\n");
		return;
	}
	PRINT("set a memory region for dpc->regs: %p\n", dpc->regs);
	// bottom to top
	_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 1, "spipe", SP0_JUMP);

	dummy = devm_kzalloc(&dpc->dev, sizeof(struct dp_dummy_data), GFP_KERNEL);
	if (!dummy) {
		DRM_ERROR("kalloc dummy failed\n");
		return;
	}
	dpc->priv = dummy;
	dummy->dpc = dpc;
	dummy->vsync_enabled = true;

#if USE_HRTIMER
	dummy->timeout = ktime_set(0, 17 * 1000 * 1000);
	hrtimer_init(&dummy->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	dummy->timer.function = hrtimer_handler;
#else
	register_dummy_data(dummy);
#endif
}

static void dp_uninit(struct sdrv_dpc *dpc)
{
//	struct dp_dummy_data *dummy = (struct dp_dummy_data *)dpc->priv;

}

const struct dpc_operation dp_dummy_ops = {
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


