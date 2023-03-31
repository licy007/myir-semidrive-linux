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

#define REGISTER volatile struct

#define DP_POTER_DUFF_ID	(5)

struct sp_reg {
	u32 sp_pix_comp; //0000
	u32 sp_frm_ctrl;
	u32 sp_frm_size;
	u32 sp_y_baddr_l;
	u32 sp_y_baddr_h;//0010
	u32 reserved_0x14_0x28[6];
	u32 sp_y_stride;// 002c
	u32 reserved_0x30_0x3c[4];
	u32 sp_frm_offset;//0040
	u32 reserved_0x44_0xfc[47];
	u32 rle_y_len;//0100
	u32 reserved_0x104_0x10c[3];
	u32 rle_y_check_sum;
	u32 reserved_0x114_0x11c[3];
	u32 rle_ctrl;
	u32 reserved_0x124_0x12c[3];
	u32 rle_y_check_sum_st;
	u32 rle_u_check_sum_st;
	u32 rle_v_check_sum_st;
	u32 rle_a_check_suem_st;
	u32 rle_int_mask;
	u32 rle_int_status;
	u32 reserved_0x148_0x1fc[46];
	u32 clut_a_ctrl;
	u32 clut_y_ctrl;
	u32 clut_u_ctrl;
	u32 clut_v_ctrl;
	u32 clut_read_ctrl;
	u32 clut_baddr_l;
	u32 clut_baddr_h;
	u32 clut_load_ctrl;
	u32 reserved_0x220_0xdfc[760];
	u32 sp_sw_rst;
	u32 reserved_0xe04_0xefc[63];
	u32 sp_sdw_ctrl;
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

const static uint32_t sp_formats[] = {
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

static struct pipe_capability cap = {
	.formats = sp_formats,
	.nformats = ARRAY_SIZE(sp_formats),
	.yuv_fbc = 0,
	.xfbc = 0,
	.rotation = 0,
	.scaling = 0,
};

static int spipe_sw_reset(struct sdrv_pipe *pipe)
{
	REGISTER sp_reg *reg = (REGISTER sp_reg *)pipe->regs;

	reg->sp_sw_rst = 1;
	udelay(100);
	reg->sp_sw_rst = 0;
	return 0;
}
static int spipe_init(struct sdrv_pipe *pipe)
{
	REGISTER sp_reg *reg = (REGISTER sp_reg *)pipe->regs;

	reg->clut_a_ctrl |= BIT(16);
	reg->clut_y_ctrl |= BIT(16);
	reg->clut_u_ctrl |= BIT(16);
	reg->clut_v_ctrl |= BIT(16);

	reg->sp_sdw_ctrl = 1;

	pipe->cap = &cap;

	return 0;
}

static int spipe_check(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	return 0;
}

static int spipe_ops_update(struct sdrv_pipe *pipe, struct dpc_layer *layer, int pipe_id)
{
	REGISTER sp_reg *reg = (REGISTER sp_reg *)pipe->regs;
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);
	volatile u32 tmp = 0;
	/*pix comp*/
	reg->sp_pix_comp = (layer->comp.bpv << 24) | (layer->comp.bpu << 16) |
		(layer->comp.bpy << 8) | (layer->comp.bpa << 0);

	/*frm_ctrl*/
	tmp |= layer->comp.buf_fmt;
	tmp |= layer->comp.uvmode << 4;
	tmp |= layer->comp.uvswap << 6;
	tmp |= layer->comp.is_yuv << 7;
	tmp |= layer->comp.swap << 12;

	reg->sp_frm_ctrl = tmp;

	reg->sp_frm_size = ((layer->src_h - 1) << 16) | (layer->src_w - 1);
	reg->sp_y_baddr_h = layer->addr_h[0];
	reg->sp_y_baddr_l = layer->addr_l[0];
	reg->sp_y_stride = layer->pitch[0] - 1;
	reg->sp_frm_offset = (layer->src_y << 16) | layer->src_x;

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

	reg->sp_sdw_ctrl = 1;

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

static int spipe_update(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	pipe_clean_mlc(pipe);

	_set_pipe_path(pipe);

	return spipe_ops_update(pipe, layer, pipe->id);
}

static int spipe_disable(struct sdrv_pipe *pipe)
{
	REGISTER sp_reg *reg = (REGISTER sp_reg *)pipe->regs;
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);

	pipe_clean_mlc(pipe);

	/*s-pipe triggle*/
	mlc_reg->mlc_sf[pipe->id - 1].mlc_sf_ctrl.bits.sf_en = 0;

	reg->sp_sdw_ctrl = 1;

	pipe->enable_status = false;
	return 0;
}

static struct pipe_operation spipe_ops = {
	.init =  spipe_init,
	.update = spipe_update,
	.check = spipe_check,
	.disable = spipe_disable,
	.sw_reset = spipe_sw_reset,
};

struct ops_entry spipe_entry = {
	.ver = "spipe",
	.ops = &spipe_ops,
};

// static int __init pipe_register(void) {
// 	return pipe_ops_register(&entry);
// }
// subsys_initcall(pipe_register);
