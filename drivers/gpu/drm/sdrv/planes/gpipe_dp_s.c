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

#define GPIPE1_DP_NAME	"gpipe1_dp"
#define MLC_GPIPE_JMP 0x30

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
	u32 reserved_0x50c_0xdfc[573];
	u32 gp_sw_rst;
	u32 reserved_0xe04_0xefc[63];
	u32 gp_sdw_ctrl;
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

const uint32_t gp_mid_formats[] = {
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
};

static struct pipe_capability gp1_cap = {
	.formats = gp_mid_formats,
	.nformats = ARRAY_SIZE(gp_mid_formats),
	.yuv = 0,
	.yuv_fbc = 0,
	.xfbc = 1,
	.rotation = 0,
	.scaling = 0,
};

static int gpipe_sw_reset(struct sdrv_pipe *pipe)
{
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;

	reg->gp_sw_rst = 1;
	udelay(100);
	reg->gp_sw_rst = 0;

	return 0;
}

static int gpipe_init(struct sdrv_pipe *pipe)
{
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;

	reg->gp_yuvup_ctrl |= BIT(0);//yuvup bypass

	reg->gp_csc_ctrl |= BIT(0);//csc bypass
	reg->gp_csc_coef1 = (csc_param[0][1] << 16) | csc_param[0][0];
	reg->gp_csc_coef2 = (csc_param[0][3] << 16) | csc_param[0][2];
	reg->gp_csc_coef3 = (csc_param[0][5] << 16) | csc_param[0][4];
	reg->gp_csc_coef4 = (csc_param[0][7] << 16) | csc_param[0][6];
	reg->gp_csc_coef5 = (csc_param[0][9] << 16) | csc_param[0][8];
	reg->gp_csc_coef6 = (csc_param[0][11] << 16) | csc_param[0][10];
	reg->gp_csc_coef7 = (csc_param[0][13] << 16) | csc_param[0][12];
	reg->gp_csc_coef8 = csc_param[0][14];
	reg->gp_sdw_ctrl = 1;

	pipe->cap = &gp1_cap;
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

static int gpipe_dp_s_update(struct sdrv_pipe *pipe, struct dpc_layer *layer, int pipe_id)
{
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);
	struct tile_ctx *ctx = &layer->ctx;
	volatile u32 tmp = 0;

	/*pix comp*/
	reg->gp_pix_comp = (layer->comp.bpv << 24) | (layer->comp.bpu << 16) |
		(layer->comp.bpy << 8) | (layer->comp.bpa << 0);

	/*frm_ctrl*/
	tmp |= layer->comp.buf_fmt;
	/*linear or tile mode*/
	tmp |= layer->ctx.data_mode << 2;
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

		if ((ctx->height != layer->dst_h) || (ctx->width != layer->dst_w)) {
			reg->gp_crop_ctrl &= ~BIT(0);
		} else {
			reg->gp_crop_ctrl |= BIT(0);
		}

		pipe->fbdc_status = true;
	} else {
		reg->gp_frm_size = ((layer->src_h - 1) << 16) | (layer->src_w - 1);
		reg->gp_frm_offset = (layer->src_y << 16) | layer->src_x;

		reg->gp_crop_ctrl |= BIT(0);

		reg->gp_tile_ctrl &= ~ 0x7F;
		pipe->fbdc_status = false;
	}

	reg->gp_crop_size = (layer->dst_h - 1) << 16 | (layer->dst_w - 1);

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
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.g_alpha_en = 0;
			/*premulit pma_en need set in mlc path*/
			break;
		case DPC_MODE_BLEND_COVERAGE:
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.g_alpha_en = 0;
			break;
		default:
			mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.g_alpha_en = 1;
			break;
	}

	mlc_reg->mlc_sf[pipe_id - 1].mlc_sf_ctrl.bits.sf_en = 1;//sf_en

	reg->gp_sdw_ctrl = 1;

	pipe->enable_status = true;

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
	pipe_clean_mlc(pipe);

	_set_pipe_path(pipe);

	return gpipe_dp_s_update(pipe, layer, pipe->id);
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

static struct pipe_operation gpipe_l_ops = {
	.init =  gpipe_init,
	.update = gpipe_update,
	.check = gpipe_check,
	.disable = gpipe_disable,
	.sw_reset = gpipe_sw_reset,
};

struct ops_entry gpipe1_dp_entry = {
	.ver = GPIPE1_DP_NAME,
	.ops = &gpipe_l_ops,
};

// static int __init pipe_register(void) {
// 	return pipe_ops_register(&entry);
// }
// subsys_initcall(pipe_register);
