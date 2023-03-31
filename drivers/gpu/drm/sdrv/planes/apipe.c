#include "sdrv_plane.h"

const uint32_t big_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_BGRX4444,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_BGRX1010102,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_R8,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_AYUV,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV444,
};

static struct pipe_capability apipe_cap = {
	.formats = big_formats,
	.nformats = ARRAY_SIZE(big_formats),
	.yuv = 0,
	.yuv_fbc = 0,
	.xfbc = 1,
	.rotation = 0,
	.scaling = 0,
};

static int apipe_init(struct sdrv_pipe *pipe)
{
	ENTRY();
	pipe->cap = &apipe_cap;
	return 0;
}

static int apipe_check(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	return 0;
}

static int apipe_update(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	return 0;
}

static int apipe_disable(struct sdrv_pipe *pipe)
{
	return 0;
}

static int apipe_sw_reset(struct sdrv_pipe *pipe)
{
	return 0;
}

static struct pipe_operation apipe_ops = {
	.init =  apipe_init,
	.update = apipe_update,
	.check = apipe_check,
	.disable = apipe_disable,
	.sw_reset = apipe_sw_reset,
};

struct ops_entry apipe_entry = {
	.ver = "apipe",
	.ops = &apipe_ops,
};

// static int __init pipe_register(void) {
// 	pipe_ops_register(&entry);
// }
// subsys_initcall(pipe_register);