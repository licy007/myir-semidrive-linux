/*
* SEMIDRIVE Copyright Statement
* Copyright (c) SEMIDRIVE. All rights reserved
* This software and all rights therein are owned by SEMIDRIVE,
* and are protected by copyright law and other relevant laws, regulations and protection.
* Without SEMIDRIVE's prior written consent and /or related rights,
* please do not use this software or any potion thereof in any form or by any means.
* You may not reproduce, modify or distribute this software except in compliance with the License.
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* You should have received a copy of the License along with this program.
* If not, see <http://www.semidrive.com/licenses/>.
*/

#include "sdrv_plane.h"

#define GPIPE0_DP_NAME	"gpipe0_dp"
#define MLC_GPIPE_JMP 0x30

typedef uint32_t csc_param_t[15];

#define REGISTER	volatile struct

#define DP_POTER_DUFF_ID	(5)

struct gp_reg {
	u32 gp_pix_comp;
	u32 gp_frm_ctrl;
	u32 gp_frm_size;
	u32 gp_y_baddr_l;
	u32 gp_y_baddr_h;
	u32 gp_u_baddr_l;
	u32 gp_u_baddr_h;
	u32 gp_v_baddr_l;
	u32 gp_v_baddr_h;
	u32 reserved_0x24_0x28[2];
	u32 gp_y_stride;
	u32 gp_u_stride;
	u32 gp_v_stride;
	u32 gp_tile_ctrl;
	u32 reserved_0x3c;
	u32 gp_frm_offset;
	u32 gp_yuvup_ctrl;
	u32 reserved_0x48_0xfc[46];
	u32 gp_crop_ctrl;
	u32 gp_crop_ul_pos;
	u32 gp_crop_size;
	u32 reserved_0x10c_0x11c[5];
	u32 gp_crop_par_err;
	u32 reserved_0x124_0x1fc[55];
	u32 gp_csc_ctrl;
	u32 gp_csc_coef1;
	u32 gp_csc_coef2;
	u32 gp_csc_coef3;
	u32 gp_csc_coef4;
	u32 gp_csc_coef5;
	u32 gp_csc_coef6;
	u32 gp_csc_coef7;
	u32 gp_csc_coef8;
	u32 reserved_0x224_0x2fc[55];
	u32 gp_hs_ctrl;
	u32 gp_hs_ini;
	u32 gp_hs_ratio;
	u32 gp_hs_width;
	u32 reserved_0x310_0x3fc[60];
	u32 gp_vs_ctrl;
	u32 gp_vs_resv;
	u32 gp_vs_inc;
	u32 reserved_0x40c;
	u32 gp_re_status;
	u32 reserved_0x414_0x4fc[59];
	u32 gp_fbdc_ctrl;
	u32 gp_fbdc_clear_0;
	u32 gp_fbdc_clear_1;
	u32 reserved_0x50c_0xdfc[61];
	u32 gp_hscaler[256];
	u32 gp_vscaler[256];
	u32 gp_sw_rst;
	u32 reserved_0xe04_0xefc[63];
	u32 gp_sdw_ctrl;
};

struct gp_sf_reg {
	u32 mlc_sf_ctrl;
	u32 mlc_sf_h_spos;
	u32 mlc_sf_v_spos;
	u32 mlc_sf_size;
	u32 mlc_sf_crop_h_pos;
	u32 mlc_sf_crop_v_pos;
	u32 mlc_sf_g_alpha;
	u32 mlc_sf_ckey_alpha;
	u32 mlc_sf_ckey_r_lv;
	u32 mlc_sf_ckey_g_lv;
	u32 mlc_sf_ckey_b_lv;
	u32 mlc_sf_aflu_time;
};

struct mlc_sf_reg {
	union _mlc_sf_ctrl {
		u32 val;
		struct _mlc_sf_ctrl_ {
			u32 sf_en: 1;
			u32 crop_en: 1;
			u32 g_alpha_en: 1;
			u32 ckey_en: 1;
			u32 aflu_en: 1;
			u32 aflu_psel: 1;
			u32 slowdown_en: 1;
			u32 vpos_prot_en: 1;
			u32 prot_val: 6;
			u32 reserved: 18;
		} bits;
	} mlc_sf_ctrl;
	u32 mlc_sf_h_spos;
	u32 mlc_sf_v_spos;
	u32 mlc_sf_size;
	u32 mlc_sf_crop_h_pos;
	u32 mlc_sf_crop_v_pos;
	u32 mlc_sf_g_alpha;
	u32 mlc_sf_ckey_alpha;
	u32 mlc_sf_ckey_r_lv;
	u32 mlc_sf_ckey_g_lv;
	u32 mlc_sf_ckey_b_lv;
	u32 mlc_sf_aflu_time;
};

struct mlc_reg {
	struct mlc_sf_reg mlc_sf[4];
	u32 reserved_0xc0_0x1fc[80];
	union _mlc_path_ctrl {
		u32 val;
		struct _mlc_path_ctrl_ {
			u32 layer_out_idx: 4;
			u32 pd_src_idx: 3;
			u32 reserved0: 1;
			u32 pd_des_idx: 3;
			u32 reserved1: 1;
			u32 pd_out_idx: 3;
			u32 reserved2: 1;
			u32 alpha_bld_idx: 4;
			u32 pd_mode: 5;
			u32 reserved3: 3;
			u32 pd_out_sel: 1;
			u32 pma_en: 1;
			u32 reserved4: 2;
		} bits;
	} mlc_path_ctrl[5];
	u32 reserved_0x214_0x21c[3];
	u32 mlc_bg_ctrl;
	u32 mlc_bg_color;
	u32 mlc_bg_aflu_time;
	u32 reserved_0x22c;
	u32 mlc_canvas_color;
	u32 mlc_clk_ratio;
	u32 reserved_0x238_0x23c[2];
	u32 mlc_int_mask;
	u32 mlc_int_status;
};

const uint32_t gp_big_formats[] = {
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
	DRM_FORMAT_BGR888_PLANE,
	DRM_FORMAT_RGB888_PLANE,


	DRM_FORMAT_YUYV,
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

static const uint32_t dataspace_supported[] = {
	DATASPACE_UNKNOWN,
	DATASPACE_V0_SRGB_LINEAR,
	DATASPACE_V0_SCRGB_LINEAR,
	DATASPACE_SRGB,
	DATASPACE_V0_SRGB,
	DATASPACE_V0_SCRGB,
	DATASPACE_JFIF,
	DATASPACE_V0_JFIF,
	DATASPACE_BT601_625,
	DATASPACE_V0_BT601_625,
	DATASPACE_BT601_525,
	DATASPACE_V0_BT601_525,
	DATASPACE_BT709,
	DATASPACE_V0_BT709,
	DATASPACE_DCI_P3_LINEAR,
	DATASPACE_DCI_P3,
	DATASPACE_DISPLAY_P3_LINEAR,
	DATASPACE_DISPLAY_P3,
	DATASPACE_ADOBE_RGB,
	DATASPACE_BT2020_LINEAR,
	DATASPACE_BT2020,
	DATASPACE_BT2020_PQ,
};
static struct pipe_capability gp0_cap = {
	.formats = gp_big_formats,
	.nformats = ARRAY_SIZE(gp_big_formats),
	.yuv = 1,
	.yuv_fbc = 1,
	.xfbc = 1,
	.rotation = 0,
	.scaling = 1,
	.dataspaces = dataspace_supported,
	.num_dataspaces = ARRAY_SIZE(dataspace_supported),
};

struct dataspace_map {
	uint32_t dataspace_id;
	const csc_param_t csc_param;
};

static const uint32_t csc_param[4][15] = {
	{/*YCbCr to RGB BT601 limited*/
		0x094F, 0x0000, 0x0CC4, 0x094F,
		0xF97F, 0xFCE0, 0x094F, 0x1024,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0040, 0x0200, 0x0200,
	},
	{/*YUV to RGB*/
		0x0800, 0x0000, 0x091E, 0x0800,
		0x7CD8, 0x7B5B, 0x0800, 0x1041,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000
	},
	{/*RGB to YCbCr*/
		0x0000
	},
	{/*RGB to YUV*/
		0x0000
	}
};

static struct dataspace_map gpipe_dataspace_maps[ARRAY_SIZE(dataspace_supported)] = {
	{
		DATASPACE_UNKNOWN, //0
		{
			0x0800, 0x0000, 0x0B37, 0x0800,
			0xFD40, 0xFA4A, 0x0800, 0x0E2D,
			0x0000, 0x0000, 0x0000, 0x0000,
			0x40,
			0x200,
			0x200,
		},
	},
	/*BT601 v0 yuv2rgb*/
	{
		DATASPACE_V0_BT601_625,
		{
			0x094F, 0x0000, 0x0CC4, 0x094F,
			0xF97F, 0xFCE0, 0x094F, 0x1024,
			0x0000, 0x0000, 0x0000, 0x0000,
			0x40,
			0x200,
			0x200,
		},
	},
	/*(STANDARD_BT601_625 | TRANSFER_SMPTE_170M) | RANGE_FULL*/
	{
		DATASPACE_V0_JFIF,
		{
			0x0800, 0x0000, 0x0B37, 0x0800,
			0xFD40, 0xFA4A, 0x0800, 0x0E2D,
			0x0000, 0x0000, 0x0000, 0x0000,
			0x0,
			0x200,
			0x200,
		},
	},
	/*BT709 limited range yuv2rgb*/
	{
		DATASPACE_V0_BT709,
		{
			0x094F, 0x0000, 0x0E58, 0x094F,
			0xFE4C, 0xFBBB, 0x094F, 0x10EB,
			0x0000, 0x0000, 0x0000, 0x0000,
			0x40,
			0x200,
			0x200,
		},
	},
	/*BT709 full range yuv2rgb*/
	{
		DATASPACE_BT709,
		{
			0x0800, 0x0000, 0x0C51, 0x0800,
			0xFE8A, 0xFC54, 0x0800, 0x0E87,
			0x0000, 0x0000, 0x0000, 0x0000,
			0x0,
			0x200,
			0x200,
		},
	},
};

static int gpipe_sw_reset(struct sdrv_pipe *pipe)
{
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;

	reg->gp_sw_rst = 1;
	udelay(100);
	reg->gp_sw_rst = 0;

	return 0;
}

static void gpipe_dp_hscaler_coef_init(void *pipe_regs)
{
	const int hscaler_coef[33][5] = {
		{  0,    0,  512,    0,    0},
		{  0,   -5,  512,    5,    0},
		{  0,  -10,  511,   11,    0},
		{  0,  -14,  509,   17,    0},
		{  0,  -18,  508,   23,   -1},
		{ -1,  -22,  507,   29,   -1},
		{ -1,  -25,  503,   36,   -1},
		{ -1,  -28,  500,   43,   -2},
		{ -2,  -31,  496,   51,   -2},
		{ -2,  -33,  491,   59,   -3},
		{ -3,  -35,  486,   67,   -3},
		{ -3,  -37,  481,   75,   -4},
		{ -3,  -39,  475,   84,   -5},
		{ -4,  -41,  471,   92,   -6},
		{ -5,  -42,  464,  102,   -7},
		{ -5,  -43,  457,  111,   -8},
		{ -6,  -43,  449,  121,   -9},
		{ -6,  -44,  442,  130,  -10},
		{ -7,  -44,  435,  140,  -12},
		{ -7,  -44,  425,  151,  -13},
		{ -8,  -44,  417,  161,  -14},
		{ -8,  -44,  408,  172,  -16},
		{ -9,  -44,  399,  183,  -17},
		{ -9,  -43,  390,  193,  -19},
		{ -9,  -42,  379,  204,  -20},
		{-10,  -41,  369,  216,  -22},
		{-10,  -40,  358,  227,  -23},
		{-11,  -39,  349,  238,  -25},
		{-11,  -38,  339,  249,  -27},
		{-11,  -37,  327,  261,  -28},
		{-11,  -36,  317,  272,  -30},
		{-12,  -34,  306,  283,  -31},
		{-12,  -33,  295,  295,  -33},
	};
	u32 i, head, tail;
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe_regs;

	for (i = 0; i < 33; i++) {
		head = ((hscaler_coef[i][0] & 0x7ff) << 12)
			| ((hscaler_coef[i][1] & 0x7ff) << 1)
			| ((hscaler_coef[i][2] & 0x400) >> 10);
		tail = ((hscaler_coef[i][2] & 0x3ff) << 22)
			| ((hscaler_coef[i][3] & 0x7ff) << 11)
			| (hscaler_coef[i][4] & 0x7ff);

		reg->gp_hscaler[i] = head;
		reg->gp_hscaler[64 + i] = tail;
	}
}

static void gpipe_dp_vscaler_coef_init(void *pipe_regs)
{
	const int vscaler_coef[33][4] = {
		{ 0,   64,    0,    0},
		{-1,   64,    1,    0},
		{-1,   64,    1,    0},
		{-2,   64,    2,    0},
		{-2,   63,    3,    0},
		{-3,   63,    4,    0},
		{-3,   62,    5,    0},
		{-3,   62,    5,    0},
		{-4,   62,    6,    0},
		{-4,   61,    7,    0},
		{-4,   60,    8,    0},
		{-5,   61,    9,   -1},
		{-5,   60,   10,   -1},
		{-5,   59,   11,   -1},
		{-5,   57,   13,   -1},
		{-5,   56,   14,   -1},
		{-5,   55,   15,   -1},
		{-5,   54,   16,   -1},
		{-5,   53,   17,   -1},
		{-5,   52,   19,   -2},
		{-5,   51,   20,   -2},
		{-5,   50,   21,   -2},
		{-5,   49,   22,   -2},
		{-5,   47,   24,   -2},
		{-5,   46,   25,   -2},
		{-5,   46,   26,   -3},
		{-5,   44,   28,   -3},
		{-5,   43,   29,   -3},
		{-5,   41,   31,   -3},
		{-5,   40,   32,   -3},
		{-4,   39,   33,   -4},
		{-4,   37,   35,   -4},
		{-4,   36,   36,   -4},
	};
	u32 i, value;
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe_regs;

	for (i = 0; i < 33; i++) {
		value = ((vscaler_coef[i][3] & 0xff) << 24) | ((vscaler_coef[i][2] & 0xff) << 16)
				| ((vscaler_coef[i][1] & 0xff) << 8) | ((vscaler_coef[i][0] & 0xff));

		reg->gp_vscaler[i] = value;
	}
}

static int gpipe_init(struct sdrv_pipe *pipe)
{
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;

	reg->gp_yuvup_ctrl |= BIT(0) | BIT(3);//yuvup bypass, repeat mode

	reg->gp_csc_ctrl |= BIT(0);//csc bypass
	reg->gp_csc_coef1 = (csc_param[0][1] << 16) | csc_param[0][0];
	reg->gp_csc_coef2 = (csc_param[0][3] << 16) | csc_param[0][2];
	reg->gp_csc_coef3 = (csc_param[0][5] << 16) | csc_param[0][4];
	reg->gp_csc_coef4 = (csc_param[0][7] << 16) | csc_param[0][6];
	reg->gp_csc_coef5 = (csc_param[0][9] << 16) | csc_param[0][8];
	reg->gp_csc_coef6 = (csc_param[0][11] << 16) | csc_param[0][10];
	reg->gp_csc_coef7 = (csc_param[0][13] << 16) | csc_param[0][12];
	reg->gp_csc_coef8 = csc_param[0][14];

	gpipe_dp_hscaler_coef_init(pipe->regs);
	gpipe_dp_vscaler_coef_init(pipe->regs);

	reg->gp_sdw_ctrl = 1;

	pipe->cap = &gp0_cap;
	if (!pipe->cap) {
		DRM_ERROR("%s capacities is invalid\n", pipe->name);
		return -EINVAL;
	}

	return 0;
}

static int gpipe_check(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	return 0;
}

static int gpipe_dp_set_csc_param(void *pipe_regs, const csc_param_t csc_param) {
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe_regs;

	reg->gp_yuvup_ctrl |= BIT(0) | BIT(3);//yuvup bypass, repeat mode

	reg->gp_csc_ctrl |= BIT(0);//csc bypass
	reg->gp_csc_coef1 = (csc_param[1] << 16) | csc_param[0];
	reg->gp_csc_coef2 = (csc_param[3] << 16) | csc_param[2];
	reg->gp_csc_coef3 = (csc_param[5] << 16) | csc_param[4];
	reg->gp_csc_coef4 = (csc_param[7] << 16) | csc_param[6];
	reg->gp_csc_coef5 = (csc_param[9] << 16) | csc_param[8];
	reg->gp_csc_coef6 = (csc_param[11] << 16) | csc_param[10];
	reg->gp_csc_coef7 = (csc_param[13] << 16) | csc_param[12];
	reg->gp_csc_coef8 = csc_param[14];

	return 0;
}

static int set_dataspace(struct sdrv_pipe *pipe, int dataspace) {
	int index = 0;

	if (pipe->cap->num_dataspaces) {
		for (index = 0; index < pipe->cap->num_dataspaces; index++) {
			if (dataspace == gpipe_dataspace_maps[index].dataspace_id)
				break;
		}
	}

	if (dataspace != pipe->current_dataspace) {
		if (index >= pipe->cap->num_dataspaces) {
			index = DATASPACE_UNKNOWN;
			DRM_WARN("can't support dataspaces map %d\n", dataspace);
		}

		pipe->current_dataspace = dataspace;
		DRM_INFO("change dataspace[%d] %d\n", index, gpipe_dataspace_maps[index].dataspace_id);
		return gpipe_dp_set_csc_param(pipe->regs, gpipe_dataspace_maps[index].csc_param);
	}

	return 0;
}

static int gpipe_dp_update(struct sdrv_pipe *pipe, struct dpc_layer *layer, int pipe_id)
{
	const unsigned int p_hs = 524288;
	const unsigned int p_vs = 262114;
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);
	struct tile_ctx *ctx = &layer->ctx;
	volatile u32 tmp = 0;
	int ratio_int, ratio_fra, vs_inc;
	int pic_4k = 0;

	if (layer->format == DRM_FORMAT_NV12 && (layer->src_w > 2560)) {
		pic_4k = 1;
		// height / 2
		layer->src_h /= 2;
	}

	/*pix comp*/
	reg->gp_pix_comp = (layer->comp.bpv << 24) | (layer->comp.bpu << 16) |
		(layer->comp.bpy << 8) | (layer->comp.bpa << 0);

	/*frm_ctrl*/
	tmp |= layer->comp.buf_fmt;
	/*linear or tile mode*/
	tmp |= layer->ctx.data_mode << 2;

	// UV_YUV444_RGB       = 0b00,
	// UV_YUV422           = 0b01,
	// UV_YUV440           = 0b10,
	// UV_YUV420           = 0b11

	if (pic_4k)
		tmp |= 1 << 5;
	else
		tmp |= layer->comp.uvmode << 5;

	if (layer->comp.buf_fmt != FMT_PLANAR)
		tmp |= layer->comp.uvswap << 7;

	tmp |= layer->comp.is_yuv << 8;
	tmp |= layer->rotation << 9;
	tmp |= layer->comp.swap << 12;
	/*endin swap*/
	tmp |= layer->comp.endin << 16;
	reg->gp_frm_ctrl = tmp;

	tmp = reg->gp_yuvup_ctrl;
	tmp = (~ BIT(0) & tmp) | !layer->comp.is_yuv;
	reg->gp_yuvup_ctrl = tmp;

	tmp = reg->gp_csc_ctrl;
	tmp = (~ BIT(0) & tmp) | !layer->comp.is_yuv;
	reg->gp_csc_ctrl = tmp;

	if (ctx->is_tile_mode) {
		reg->gp_frm_size = ((ctx->height - 1) << 16) | (ctx->width - 1);
		reg->gp_frm_offset = (ctx->y1 << 16) | ctx->x1;

		tmp = reg->gp_tile_ctrl;
		tmp &= ~(0x3FF << 16);
		if ((layer->format == DRM_FORMAT_NV12) || (layer->format == DRM_FORMAT_NV21))
			tmp |= ((0x1 << 9) | (ctx->width / ctx->tile_hsize_value - 1)) << 16;
		tmp &= ~ BIT(9);//re_fifo_byps
		tmp &= ~ 0x7F;
		tmp |= ctx->tile_vsize << 4 | ctx->tile_hsize;
		reg->gp_tile_ctrl = tmp;
		pipe->fbdc_status = true;

		if ((ctx->height != layer->src_h)
			|| (ctx->width != layer->src_w)) {
			reg->gp_crop_ctrl &= ~BIT(0);
		} else {
			reg->gp_crop_ctrl |= BIT(0);
		}
	} else {
		reg->gp_frm_size = ((layer->src_h - 1) << 16) | (layer->src_w - 1);
		reg->gp_frm_offset = (layer->src_y << 16) | layer->src_x;

		reg->gp_crop_ctrl |= BIT(0);

		reg->gp_tile_ctrl &= ~ 0x7F;
		if ((pipe->dpc->re_ram_type == 0) && (layer->src_w > 1706))
			reg->gp_tile_ctrl |= BIT(9);
		else
			reg->gp_tile_ctrl &= ~BIT(9);

		pipe->fbdc_status = false;
	}

	if (ctx->is_fbdc_cps) {
		tmp = reg->gp_fbdc_ctrl;
		tmp &= ~ 0xFF;
		tmp |= (ctx->fbdc_fmt << 1) | 0x1; //fmt, en
		reg->gp_fbdc_ctrl = tmp;
	} else {
		reg->gp_fbdc_ctrl &= ~ 0xFF;
	}

	reg->gp_y_baddr_h = layer->addr_h[0];
	reg->gp_y_baddr_l = layer->addr_l[0];
	if (pic_4k)
		reg->gp_y_stride = layer->pitch[0] * 2 - 1;
	else
		reg->gp_y_stride = layer->pitch[0] - 1;

	if (layer->nplanes > 1) {
		reg->gp_u_baddr_h = layer->addr_h[1];
		reg->gp_u_baddr_l = layer->addr_l[1];
		reg->gp_u_stride = layer->pitch[1] - 1;
	}

	if (layer->nplanes > 2) {
		if ((layer->comp.buf_fmt == FMT_PLANAR) && (layer->comp.uvswap)) {
			reg->gp_u_baddr_h = layer->addr_h[2];
			reg->gp_u_baddr_l = layer->addr_l[2];
			reg->gp_u_stride = layer->pitch[2] - 1;

			reg->gp_v_baddr_h = layer->addr_h[1];
			reg->gp_v_baddr_l = layer->addr_l[1];
			reg->gp_v_stride = layer->pitch[1] - 1;
		} else {
			reg->gp_v_baddr_h = layer->addr_h[2];
			reg->gp_v_baddr_l = layer->addr_l[2];
			reg->gp_v_stride = layer->pitch[2] - 1;
		}
	}

	if (layer->src_w == layer->dst_w) {
		reg->gp_hs_ctrl &= ~0xF;//filter disable
		reg->gp_hs_ratio = BIT(19);
	} else {
		ratio_int = layer->src_w / layer->dst_w;
		ratio_fra = (layer->src_w % layer->dst_w) * p_hs / layer->dst_w;

		reg->gp_hs_ctrl |= 0xF;//filter enable
		reg->gp_hs_ratio = (ratio_int << 19) | (ratio_fra & 0x7FFFF);
	}

	reg->gp_hs_width = layer->dst_w - 1;
	reg->gp_crop_size = (layer->src_h - 1) << 16 | (layer->dst_w - 1);

	if (layer->src_h == layer->dst_h) {
		reg->gp_vs_ctrl &= ~0x7;
		reg->gp_vs_inc = 0;
	} else {
		vs_inc = (long long)(layer->src_h - 1) * p_vs / (layer->dst_h - 1);
		tmp = reg->gp_vs_ctrl;
		tmp &= ~0x7;
		tmp |= 0x2;
		reg->gp_vs_ctrl |= tmp;
		reg->gp_vs_inc = vs_inc;
	}

	reg->gp_vs_resv = layer->dst_h - 1;

	/*mlc layer*/
	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.val = 0x700;

	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_crop_h_pos = 0;
	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_crop_v_pos = 0;
	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_h_spos = layer->dst_x;
	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_v_spos = layer->dst_y;

	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_size   = ((layer->dst_h - 1) << 16) | (layer->dst_w - 1);

	switch (layer->blend_mode) {
		case DPC_MODE_BLEND_PIXEL_NONE:
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.g_alpha_en = 1;//g_alpha_en
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_g_alpha = layer->alpha;
			break;
		case DPC_MODE_BLEND_PREMULTI:
			//sf_reg->mlc_sf_ctrl &= ~ BIT(2);
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.g_alpha_en = 0;
			/*premulit pma_en need set in mlc path*/
			break;
		case DPC_MODE_BLEND_COVERAGE:
			//sf_reg->mlc_sf_ctrl &= ~ BIT(2);
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.g_alpha_en = 0;
			break;
		default:
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.g_alpha_en = 1;
			break;
	}
	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.sf_en = 1;//sf_en

	pipe->enable_status = true;
	reg->gp_sdw_ctrl = 1;
	return 0;
}

static void _set_pipe_path(struct sdrv_pipe *pipe)
{
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);
	struct dpc_layer *layer = NULL;
	uint32_t zpos, hwid;
	uint32_t tmp;

	layer = &pipe->layer_data;
	zpos = pipe->layer_data.zpos;
	hwid = pipe->layer_data.index;

	if (layer->pd_type) {
		switch (layer->pd_type) {
			case PD_SRC:
				mlc_reg->mlc_path_ctrl[0].bits.pd_src_idx = hwid;
			break;
			case PD_DST:
				mlc_reg->mlc_path_ctrl[0].bits.pd_des_idx = hwid;
				mlc_reg->mlc_path_ctrl[0].bits.pd_mode = layer->pd_mode;
				mlc_reg->mlc_path_ctrl[0].bits.pd_out_idx = zpos;
				mlc_reg->mlc_path_ctrl[zpos].bits.alpha_bld_idx = DP_POTER_DUFF_ID;
			break;
			default:
			break;
		}
		/*set layer out idx*/
		mlc_reg->mlc_path_ctrl[hwid].bits.layer_out_idx = DP_POTER_DUFF_ID;
	} else {
		/*set layer out idx*/
		mlc_reg->mlc_path_ctrl[hwid].bits.layer_out_idx = zpos;
		mlc_reg->mlc_path_ctrl[zpos].bits.alpha_bld_idx = hwid;
	}

	if (pipe->layer_data.blend_mode == DPC_MODE_BLEND_PREMULTI)
		tmp = 1;
	else
		tmp = 0;

	mlc_reg->mlc_path_ctrl[zpos].bits.pma_en = tmp;
}

static void pipe_clean_mlc(struct sdrv_pipe *pipe)
{
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);
	uint32_t old_zpos, old_hwid;

	old_zpos = mlc_reg->mlc_path_ctrl[pipe->id].bits.layer_out_idx;
	if (old_zpos == 0xF)
		return;

	old_hwid   = mlc_reg->mlc_path_ctrl[old_zpos].bits.alpha_bld_idx;

	if (pipe->id == old_hwid) {
		mlc_reg->mlc_path_ctrl[old_zpos].bits.alpha_bld_idx = 0xF;
		mlc_reg->mlc_path_ctrl[old_zpos].bits.pma_en = 0;
	} else if (old_zpos == DP_POTER_DUFF_ID) {
		/* pd mode */
		old_hwid = mlc_reg->mlc_path_ctrl[0].bits.pd_src_idx;

		if (old_hwid == pipe->id) {
			/* clear pd src */
			mlc_reg->mlc_path_ctrl[0].bits.pd_src_idx = 0x7;
		} else {
			old_hwid = mlc_reg->mlc_path_ctrl[0].bits.pd_des_idx;
			if (old_hwid == pipe->id) {
				/* clear pd dst */
				old_zpos = mlc_reg->mlc_path_ctrl[0].bits.pd_out_idx;
				mlc_reg->mlc_path_ctrl[old_zpos].bits.alpha_bld_idx = 0xF;
				mlc_reg->mlc_path_ctrl[0].bits.pd_out_idx = 0x7;
				mlc_reg->mlc_path_ctrl[0].bits.pd_des_idx = 0x7;
				mlc_reg->mlc_path_ctrl[0].bits.pd_mode = 0;
			}
		}
	}

	mlc_reg->mlc_path_ctrl[pipe->id].bits.layer_out_idx = 0xF;
}

static int gpipe_update(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	int ret = set_dataspace(pipe, layer->dataspace);

	pipe_clean_mlc(pipe);

	_set_pipe_path(pipe);

	ret |= gpipe_dp_update(pipe, layer, pipe->id);

	return ret;
}

static int gpipe_disable(struct sdrv_pipe *pipe)
{
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);

	pipe_clean_mlc(pipe);

	/*g-pipe triggle*/
	mlc_reg->mlc_sf[pipe->id - 1].mlc_sf_ctrl.bits.sf_en = 0;
	reg->gp_sdw_ctrl = 1;

	pipe->enable_status = false;

	return 0;
}

static struct pipe_operation gpipe_ops = {
	.init =  gpipe_init,
	.update = gpipe_update,
	.check = gpipe_check,
	.disable = gpipe_disable,
	.sw_reset = gpipe_sw_reset,
};

struct ops_entry gpipe0_dp_entry = {
	.ver = GPIPE0_DP_NAME,
	.ops = &gpipe_ops,
};

// static int __init pipe_register(void) {
// 	return pipe_ops_register(&entry);
// }
// subsys_initcall(pipe_register);
