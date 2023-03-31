/*
* SEMIDRIVE Copyright Statement
* Copyright (c) SEMIDRIVE. All rights reserved
* This software and all rights therein are owned by SEMIDRIVE,
* and are protected by copyright law and other relevant laws, regulations and protection.
* Without SEMIDRIVE’s prior written consent and /or related rights,
* please do not use this software or any potion thereof in any form or by any means.
* You may not reproduce, modify or distribute this software except in compliance with the License.
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* You should have received a copy of the License along with this program.
* If not, see <http://www.semidrive.com/licenses/>.
*/
#include <uapi/drm/sdrv_g2d_cfg.h>
#include "../g2dlite_reg.h"
#include "sdrv_g2d.h"
#include "g2d_common.h"

#define DUMP_CSC_COEF 0

#define GP0_JUMP  (0x2000)
#define GP1_JUMP  (0x3000)
#define GP2_JUMP  (0x4000)
#define SP0_JUMP  (0x5000)
#define SP1_JUMP  (0x6000)
#define MLC_JUMP  (0x7000)
#define RDMA_JUMP (0x1000)
#define WDMA_JUMP (0xA000)
#define WP_JUMP   (0xb000)
#define AP_JUMP   (0x9000)
#define PIPE_JUMP  (0x1000)
#define MLC_GPIPE_JMP 0x30

#ifdef REGISTER
#undef REGISTER
#endif
#define REGISTER	volatile struct
#define PIPE_OFFSET(r, x) ((unsigned long)&(r->x) - (unsigned long)r)

struct g2d_gp_reg {
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
	u32 reserved_0x50c_0x5fc[61];
	u32 gp_hscaler[0x100]; //0x600_0x9fc
	u32 gp_vscaler[0x100]; //0xa00_0xdfc
	u32 reserved_0xe00_0xefc[64];
	u32 gp_sdw_ctrl; //0xf00
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

static const uint32_t gp_big_formats[] = {
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

static struct g2d_pipe_capability gp_high_cap = {
	.nformats = ARRAY_SIZE(gp_big_formats),
	.yuv = 1,
	.yuv_fbc = 1,
	.xfbc = 1,
	.rotation = 0,
	.scaling = 1,
};

static struct g2d_pipe_capability gp_mid_cap = {
	.nformats = ARRAY_SIZE(gp_big_formats),
	.yuv = 1,
	.yuv_fbc = 1,
	.xfbc = 1,
	.rotation = 0,
	.scaling = 1,
};

static struct g2d_pipe_capability gp_echo_cap = {
	.nformats = ARRAY_SIZE(gp_big_formats),
	.yuv = 1,
	.yuv_fbc = 1,
	.xfbc = 1,
	.rotation = 0,
	.scaling = 1,
};


static int csc_param[4][15] = {
	/*YCbCr to RGB*/
	{
		0x094F, 0x0000, 0x0CC4, 0x094F,
		0x3CE0, 0x397F, 0x094F, 0x1024,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0040, 0x0200, 0x0200
	},
	/*YUV to RGB*/
	{
		0x0800, 0x0000, 0x091E, 0x0800,
		0x7CD8, 0x7B5B, 0x0800, 0x1041,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0200, 0x0200
	},
	/*RGB to YCbCr*/
	{
		0x0000
	},
	/*RGB to YUV*/
	{
		0x0000
	}
};

static unsigned int reg_value(unsigned int val,
	unsigned int src, unsigned int shift, unsigned int mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

#if 0
static void gpipe_dp_hscaler_coef_init(void *pipe_regs)
{
	static const int hscaler_coef[33][5] = {
		{  0,	0, 512,	  0,   0},
		{  0,  -9, 511,	 10,   0},
		{  0, -17, 507,	 22,   0},
		{ -1, -24, 503,	 35,  -1},
		{ -2, -30, 497,	 49,  -2},
		{ -2, -35, 488,	 64,  -3},
		{ -3, -39, 479,	 80,  -5},
		{ -4, -41, 466,	 98,  -7},
		{ -5, -43, 453, 116,  -9},
		{ -6, -44, 438, 135, -11},
		{ -7, -44, 421, 155, -13},
		{ -8, -44, 405, 175, -16},
		{ -9, -43, 387, 196, -19},
		{-10, -41, 367, 218, -22},
		{-11, -39, 347, 240, -25},
		{-11, -37, 326, 262, -28},
		{-12, -34, 305, 284, -31},
		{-12, -31, 284, 305, -34},
		{-12, -28, 262, 327, -37},
		{-12, -25, 240, 348, -39},
		{-12, -22, 219, 368, -41},
		{-11, -19, 197, 388, -43},
		{-11, -16, 176, 407, -44},
		{-10, -14, 156, 425, -45},
		{-10, -11, 136, 441, -44},
		{ -9,  -9, 117, 456, -43},
		{ -8,  -7,	98, 471, -42},
		{ -7,  -5,	81, 482, -39},
		{ -6,  -3,	65, 491, -35},
		{ -5,  -2,	49, 500, -30},
		{ -3,  -1,	35, 505, -24},
		{ -2,	0,	22, 509, -17},
		{ -1,	0,	10, 512,  -9}
	};
	uint32_t i, head, tail;
	REGISTER g2d_gp_reg *reg = (REGISTER g2d_gp_reg *)pipe_regs;

	for (i = 0; i < 33; i++) {
		head = ((hscaler_coef[i][0] & 0x7ff) << 12)
			| ((hscaler_coef[i][1] & 0x7ff) << 1)
			| ((hscaler_coef[i][2] & 0x400) >> 10);
		tail = ((hscaler_coef[i][2] & 0x3ff) << 22)
			| ((hscaler_coef[i][3] & 0x7ff) << 11)
			| (hscaler_coef[i][4] & 0x7ff);

		reg->gp_hscaler[i] = head;
		reg->gp_hscaler[0x100 + i] = tail;
	}
}

static void gpipe_dp_vscaler_coef_init(void *pipe_regs, int pipe_id)
{
	const int vscaler_coef[33][4] = {
		{ 0, 64,  0,  0},
		{-1, 64,  1,  0},
		{-2, 63,  3,  0},
		{-3, 63,  4,  0},
		{-4, 62,  6,  0},
		{-4, 60,  8,  0},
		{-5, 60, 10, -1},
		{-5, 58, 12, -1},
		{-5, 56, 14, -1},
		{-5, 53, 17, -1},
		{-5, 52, 19, -2},
		{-5, 49, 22, -2},
		{-5, 47, 24, -2},
		{-5, 45, 27, -3},
		{-5, 43, 29, -3},
		{-5, 40, 32, -3},
		{-4, 37, 35, -4},
		{-4, 35, 37, -4},
		{-3, 32, 40, -5},
		{-3, 29, 43, -5},
		{-3, 27, 45, -5},
		{-2, 24, 47, -5},
		{-2, 22, 49, -5},
		{-2, 19, 52, -5},
		{-1, 17, 53, -5},
		{-1, 14, 56, -5},
		{-1, 12, 58, -5},
		{-1, 10, 60, -5},
		{ 0,  8, 60, -4},
		{ 0,  6, 62, -4},
		{ 0,  4, 63, -3},
		{ 0,  3, 63, -2},
		{ 0,  1, 64, -1}
	};
	u32 i, value, gp2_vlut_offset = 0;
	REGISTER g2d_gp_reg *reg = (REGISTER g2d_gp_reg *)pipe_regs;
	if (pipe_id == G2D_GPIPE2)
		gp2_vlut_offset = 0x80;//{0x4c00, 0x4dff} {VS_2_SADDR, VS_2_EADDR}

	for (i = 0; i < 33; i++) {
		value = ((vscaler_coef[i][3] & 0xff) << 24) | ((vscaler_coef[i][2] & 0xff) << 16)
				| ((vscaler_coef[i][1] & 0xff) << 8) | ((vscaler_coef[i][0] & 0xff));

		reg->gp_vscaler[i + gp2_vlut_offset] = value;
	}
}
#endif

static void gpipe_g2d_csc_coef_set(void *pipe_regs, int *csc_coef)
{
	REGISTER g2d_gp_reg *reg = (REGISTER g2d_gp_reg *)pipe_regs;

	reg->gp_csc_coef1 = (csc_coef[1] << 16) | csc_coef[0];
	reg->gp_csc_coef2 = (csc_coef[3] << 16) | csc_coef[2];
	reg->gp_csc_coef3 = (csc_coef[5] << 16) | csc_coef[4];
	reg->gp_csc_coef4 = (csc_coef[7] << 16) | csc_coef[6];
	reg->gp_csc_coef5 = (csc_coef[9] << 16) | csc_coef[8];
	reg->gp_csc_coef6 = (csc_coef[11] << 16) | csc_coef[10];
	reg->gp_csc_coef7 = (csc_coef[13] << 16) | csc_coef[12];
	reg->gp_csc_coef8 = csc_coef[14];
}

static int gpipe_init(struct g2d_pipe *pipe)
{
	REGISTER g2d_gp_reg *reg = (REGISTER g2d_gp_reg *)pipe->iomem_regs;
	if (!strcmp(pipe->name, GP_HIGH_NAME)) {
		memcpy(gp_high_cap.formats,gp_big_formats,sizeof(gp_big_formats));
		pipe->cap = &gp_high_cap;
	} if (!strcmp(pipe->name, GP_MID_NAME)) {
		memcpy(gp_mid_cap.formats,gp_big_formats,sizeof(gp_big_formats));
		pipe->cap = &gp_mid_cap;
	} if (!strcmp(pipe->name, GP_ECHO_NAME)) {
		memcpy(gp_echo_cap.formats,gp_big_formats,sizeof(gp_big_formats));
		pipe->cap = &gp_echo_cap;
	} if (!pipe->cap) {
		G2D_ERROR("%s capacities is invalid\n", pipe->name);
		return -EINVAL;
	}

	gpipe_g2d_csc_coef_set(pipe->iomem_regs, &csc_param[0][0]);

	reg->gp_yuvup_ctrl |= BIT(0);//yuvup bypass
	reg->gp_csc_ctrl |= BIT(0);//csc bypass
	reg->gp_sdw_ctrl = 1;

	return 0;
}

static int gpipe_g2d_set(void *pipe_regs, void *cmd_info, int cmdid, struct g2d_layer *layer)
{
	const unsigned int p_hs = 524288;
	const unsigned int p_vs = 262114;

	unsigned int val, size, vs_inc, is_yuv;
	unsigned long offset;
	REGISTER g2d_gp_reg *reg = (REGISTER g2d_gp_reg *)pipe_regs;
	cmdfile_info *cmd = (cmdfile_info *)cmd_info;
	int ratio_int, ratio_fra;
	unsigned char nplanes = layer->nplanes;
	struct pix_g2dcomp *comp = &layer->comp;
	unsigned char pipe_id = layer->index;

	if (!cmdid) {
		/*pix comp*/
		val = reg_value(comp->bpv, 0, BPV_SHIFT, BPV_MASK);
		val = reg_value(comp->bpu, val, BPU_SHIFT, BPU_MASK);
		val = reg_value(comp->bpy, val, BPY_SHIFT, BPY_MASK);
		val = reg_value(comp->bpa, val, BPA_SHIFT, BPA_MASK);
		pipe_write_reg(cmd, 0, pipe_id, G2DLITE_GP_PIX_COMP, val);
		/*frm ctrl*/
		val = reg_value(comp->endin, 0, PIPE_ENDIAN_CTRL_SHIFT, PIPE_ENDIAN_CTRL_MASK);
		val = reg_value(comp->swap, val, PIPE_COMP_SWAP_SHIFT, PIPE_COMP_SWAP_MASK);
		val = reg_value(comp->is_yuv, val, PIPE_RGB_YUV_SHIFT, PIPE_RGB_YUV_MASK);
		val = reg_value(comp->uvswap, val, PIPE_UV_SWAP_SHIFT, PIPE_UV_SWAP_MASK);
		val = reg_value(comp->uvmode, val, PIPE_UV_MODE_SHIFT, PIPE_UV_MODE_MASK);
		val = reg_value(layer->ctx.data_mode, val, PIPE_MODE_SHIFT, PIPE_MODE_MASK);
		val = reg_value(comp->buf_fmt, val, PIPE_FMT_SHIFT, PIPE_FMT_MASK);
		pipe_write_reg(cmd, 0, pipe_id, G2DLITE_GP_FRM_CTRL, val);
	}

	/*pipe related*/
	size = reg_value(layer->src_w - 1, 0, FRM_WIDTH_SHIFT, FRM_WIDTH_MASK);
	size = reg_value(layer->src_h - 1, size, FRM_HEIGHT_SHIFT, FRM_HEIGHT_MASK);
	offset = PIPE_OFFSET(reg, gp_frm_size);
	pipe_write_reg(cmd, cmdid, pipe_id, offset, size);

	pipe_write_reg(cmd, cmdid, pipe_id, PIPE_OFFSET(reg, gp_y_baddr_l), layer->addr_l[0]);
	pipe_write_reg(cmd, cmdid, pipe_id, PIPE_OFFSET(reg, gp_y_baddr_h), layer->addr_h[0]);
	pipe_write_reg(cmd, cmdid, pipe_id, PIPE_OFFSET(reg, gp_y_stride), layer->pitch[0] - 1);
	val = reg_value(layer->src_x, 0, FRM_X_SHIFT, FRM_X_MASK);
	val = reg_value(layer->src_y, val, FRM_Y_SHIFT, FRM_Y_MASK);
	pipe_write_reg(cmd, cmdid,	pipe_id,  PIPE_OFFSET(reg, gp_frm_offset), val);

	if (nplanes > 1) {
		pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_U_BADDR_L, layer->addr_l[1]);
		pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_U_BADDR_H, layer->addr_h[1]);
		pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_U_STRIDE, layer->pitch[1] - 1);
	}

	if (nplanes > 2) {
		pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_V_BADDR_L,  layer->addr_l[2]);
		pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_V_BADDR_H,  layer->addr_h[2]);
		pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_V_STRIDE, layer->pitch[2] - 1);
	}

	/*yuv params*/
	is_yuv = layer->comp.is_yuv;
	val = reg->gp_yuvup_ctrl;
	val = reg_value(is_yuv, val,
		G2DLITE_GP_YUVUP_EN_SHIFT, G2DLITE_GP_YUVUP_EN_MASK);
	val = reg_value(is_yuv ? 0 : 1, val,
		G2DLITE_GP_YUVUP_BYPASS_SHIFT, G2DLITE_GP_YUVUP_BYPASS_MASK);
	pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_YUVUP_CTRL, val);

	val = reg->gp_csc_ctrl;
	val = reg_value(is_yuv ? 0 : 1, val,
		GP_CSC_BYPASS_SHIFT, GP_CSC_BYPASS_MASK);
	pipe_write_reg(cmd, cmdid, pipe_id, GP_CSC_CTRL, val);

	/*scaler part*/
	if (layer->src_w == layer->dst_w) {
		val = reg->gp_hs_ctrl;
		val = reg_value(0, val, GP_HS_CTRL_FILTER_EN_V_SHIFT, GP_HS_CTRL_FILTER_EN_V_MASK);
		val = reg_value(0, val, GP_HS_CTRL_FILTER_EN_U_SHIFT, GP_HS_CTRL_FILTER_EN_U_MASK);
		val = reg_value(0, val, GP_HS_CTRL_FILTER_EN_Y_SHIFT, GP_HS_CTRL_FILTER_EN_Y_MASK);
		val = reg_value(0, val, GP_HS_CTRL_FILTER_EN_A_SHIFT, GP_HS_CTRL_FILTER_EN_A_MASK);
		pipe_write_reg(cmd, cmdid, pipe_id, GP_HS_HS_CTRL, val);

		val = reg_value(1, val, GP_HS_RATIO_INT_SHIFT, GP_HS_RATIO_INT_MASK);
		val = reg_value(0, val, GP_HS_RATIO_FRA_SHIFT, GP_HS_RATIO_FRA_MASK);
		pipe_write_reg(cmd, cmdid, pipe_id, GP_HS_HS_RATIO, val);
	} else {
		ratio_int = (layer->src_w / layer->dst_w);
		ratio_fra = (layer->src_w % layer->dst_w) * p_hs / (layer->dst_w);

		val = reg->gp_hs_ctrl;
		val |= (0x9 << 8);
		val = reg_value(1, val, GP_HS_CTRL_FILTER_EN_V_SHIFT, GP_HS_CTRL_FILTER_EN_V_MASK);
		val = reg_value(1, val, GP_HS_CTRL_FILTER_EN_U_SHIFT, GP_HS_CTRL_FILTER_EN_U_MASK);
		val = reg_value(1, val, GP_HS_CTRL_FILTER_EN_Y_SHIFT, GP_HS_CTRL_FILTER_EN_Y_MASK);
		val = reg_value(1, val, GP_HS_CTRL_FILTER_EN_A_SHIFT, GP_HS_CTRL_FILTER_EN_A_MASK);
		pipe_write_reg(cmd, cmdid, pipe_id, GP_HS_HS_CTRL, val);

		val = reg_value(ratio_int, val, GP_HS_RATIO_INT_SHIFT, GP_HS_RATIO_INT_MASK);
		val = reg_value(ratio_fra, val, GP_HS_RATIO_FRA_SHIFT, GP_HS_RATIO_FRA_MASK);
		pipe_write_reg(cmd, cmdid, pipe_id, GP_HS_HS_RATIO, val);
	}
	pipe_write_reg(cmd, cmdid, pipe_id, GP_HS_HS_WIDTH, layer->dst_w - 1);

	size = reg_value(layer->src_h - 1, 0, G2DLITE_GP_CROP_SIZE_V_SHIFT, G2DLITE_GP_CROP_SIZE_V_MASK);
	size = reg_value(layer->dst_w - 1, size, G2DLITE_GP_CROP_SIZE_H_SHIFT, G2DLITE_GP_CROP_SIZE_H_MASK);
	pipe_write_reg(cmd, cmdid, pipe_id, G2DLITE_GP_CROP_SIZE, size);

	if (layer->src_h == layer->dst_h) {
		val = reg->gp_vs_ctrl;
		val = reg_value(0, val, GP_VS_CTRL_VS_MODE_SHIFT, GP_VS_CTRL_VS_MODE_MASK);
		pipe_write_reg(cmd, cmdid, pipe_id, GP_VS_VS_CTRL, val);

		pipe_write_reg(cmd, cmdid, pipe_id, GP_VS_VS_INC, 0);
	} else {
		vs_inc = (long long)(layer->src_h - 1) *p_vs / (layer->dst_h - 1);
		val = reg->gp_vs_ctrl;
		val |= (0x6 << 4);
		val = reg_value(2, val, GP_VS_CTRL_VS_MODE_SHIFT, GP_VS_CTRL_VS_MODE_MASK);
		pipe_write_reg(cmd, cmdid, pipe_id, GP_VS_VS_CTRL, val);

		pipe_write_reg(cmd, cmdid, pipe_id, GP_VS_VS_INC, vs_inc);
	}
	pipe_write_reg(cmd, cmdid, pipe_id, GP_VS_VS_RESV, layer->dst_h - 1);

	return 0;
}

static int gpipe_set(struct g2d_pipe *pipe, int cmdid, struct g2d_layer *layer)
{
	struct sdrv_g2d * gd = pipe->gd;
	if (layer->format == DRM_FORMAT_NV12 && (layer->src_w > 2560)) {
		layer->src_h /= 2;
		layer->pitch[0] *= 2;
		// change to yuv422fd
		layer->comp.uvmode = 1;
	}
	return gpipe_g2d_set(pipe->iomem_regs, &gd->cmd_info[0], cmdid, layer);
}

#if DUMP_CSC_COEF
static void dump_csc_coef(struct g2d_pipe *pipe)
{
	unsigned long base = (unsigned long)pipe->iomem_regs;
	int i;

	g2d_udelay(100);

	G2D_DBG("pipe id = %d \n", pipe->id);
	for (i = 0x0204; i < 0x0224; i += 0x4) {
		G2D_DBG("csc_coef1 = 0x%x \n", readl((void *)(base + i)));
	}

}
#endif

static void gpipe_coef_set(struct g2d_pipe *pipe, struct g2d_coeff_table *tables)
{
	if (!tables->set_tables) {
		if (!!tables->csc_coef_set)
			gpipe_g2d_csc_coef_set(pipe->iomem_regs, &csc_param[0][0]);
#if DUMP_CSC_COEF
		dump_csc_coef(pipe);
#endif
		return;
	}

	if (!!tables->csc_coef_set)
		gpipe_g2d_csc_coef_set(pipe->iomem_regs, tables->csc_coef);
#if DUMP_CSC_COEF
	dump_csc_coef(pipe);
#endif
}

static struct pipe_operation gpipe_ops = {
	.init =	 gpipe_init,
	.set = gpipe_set,
	.csc_coef_set = gpipe_coef_set,
};

struct ops_entry gpipe_high_g2d_entry = {
	.ver = GP_HIGH_NAME,
	.ops = &gpipe_ops,
};

struct ops_entry gpipe_mid_g2d_entry = {
	.ver = GP_MID_NAME,
	.ops = &gpipe_ops,
};
