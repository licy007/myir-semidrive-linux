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

#define MLC_GPIPE_JMP 0x30

typedef uint32_t csc_param_t[15];

#define REGISTER	volatile struct
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
	u32 reserved_0x38_0x3c[2];
	u32 gp_frm_offset;
	u32 gp_yuvup_ctrl;
	u32 reserved_0x48_0x1fc[110];
	u32 gp_csc_ctrl;
	u32 gp_csc_coef1;
	u32 gp_csc_coef2;
	u32 gp_csc_coef3;
	u32 gp_csc_coef4;
	u32 gp_csc_coef5;
	u32 gp_csc_coef6;
	u32 gp_csc_coef7;
	u32 gp_csc_coef8;
	u32 reserved_0x224_0xdfc[759];
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

const static uint32_t gp_lite_formats[] = {
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

static struct pipe_capability cap = {
	.formats = gp_lite_formats,
	.nformats = ARRAY_SIZE(gp_lite_formats),
	.yuv_fbc = 0,
	.xfbc = 0,
	.rotation = 0,
	.scaling = 0,
	.dataspaces = dataspace_supported,
	.num_dataspaces = ARRAY_SIZE(dataspace_supported),
};

struct dataspace_map {
	uint32_t dataspace_id;
	const csc_param_t  csc_param;
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

	pipe->cap = &cap;

	return 0;
}

static int gpipe_check(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	return 0;
}

static int gpipe_dc_set_csc_param(void *pipe_regs,  const csc_param_t csc_param) {
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

static int gpipe_dc_update(struct sdrv_pipe *pipe, struct dpc_layer *layer, int pipe_id)
{
	REGISTER gp_reg *reg = (REGISTER gp_reg *)pipe->regs;
	REGISTER mlc_reg *mlc_reg = (REGISTER mlc_reg *)(pipe->dpc->regs + MLC_JUMP);
	volatile u32 tmp = 0;

	/*pix comp*/
	reg->gp_pix_comp = (layer->comp.bpv << 24) | (layer->comp.bpu << 16) |
		(layer->comp.bpy << 8) | (layer->comp.bpa << 0);

	/*frm_ctrl*/
	tmp |= layer->comp.buf_fmt;
	tmp |= layer->comp.uvmode << 4;
	tmp |= layer->comp.uvswap << 6;
	tmp |= layer->comp.is_yuv << 7;
	tmp |= layer->comp.swap << 12;
	tmp |= layer->comp.endin << 16;

	reg->gp_frm_ctrl = tmp;

	tmp = reg->gp_yuvup_ctrl;
	tmp = (~ BIT(0) & tmp) | !layer->comp.is_yuv;
	reg->gp_yuvup_ctrl = tmp;

	tmp = reg->gp_csc_ctrl;
	tmp = (~ BIT(0) & tmp) | !layer->comp.is_yuv;
	reg->gp_csc_ctrl = tmp;

	reg->gp_frm_size = ((layer->src_h - 1) << 16) | (layer->src_w - 1);
	reg->gp_frm_offset = (layer->src_y << 16) | layer->src_x;

	reg->gp_y_baddr_h = layer->addr_h[0];
	reg->gp_y_baddr_l = layer->addr_l[0];
	reg->gp_y_stride = layer->pitch[0] - 1;

	if (layer->nplanes > 1) {
		reg->gp_u_baddr_h = layer->addr_h[1];
		reg->gp_u_baddr_l = layer->addr_l[1];
		reg->gp_u_stride = layer->pitch[1] - 1;
	}

	if (layer->nplanes > 2) {
		reg->gp_v_baddr_h = layer->addr_h[2];
		reg->gp_v_baddr_l = layer->addr_l[2];
		reg->gp_v_stride = layer->pitch[2] - 1;
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

static int set_dataspace(struct sdrv_pipe *pipe, int dataspace) {
	int index = 0;

	if (pipe->cap->num_dataspaces) {
		for (index = 0; index < pipe->cap->num_dataspaces; index++) {
			if (dataspace == gpipe_dataspace_maps[index].dataspace_id)
				break;
		}
	}

	if (dataspace != pipe->current_dataspace) {
		pipe->current_dataspace = dataspace;
		DRM_INFO("change dataspace[%d] %d\n", index, dataspace);
		return gpipe_dc_set_csc_param(pipe->regs, gpipe_dataspace_maps[index].csc_param);
	}
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

	/*set layer out idx*/
	mlc_reg->mlc_path_ctrl[hwid].bits.layer_out_idx = zpos;
	mlc_reg->mlc_path_ctrl[zpos].bits.alpha_bld_idx = hwid;

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
	}

	mlc_reg->mlc_path_ctrl[pipe->id].bits.layer_out_idx = 0xF;
}

static int gpipe_update(struct sdrv_pipe *pipe, struct dpc_layer *layer)
{
	int ret = set_dataspace(pipe, layer->dataspace);

	pipe_clean_mlc(pipe);

	_set_pipe_path(pipe);

	ret |= gpipe_dc_update(pipe, layer, pipe->id);
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

struct ops_entry gpipe_dc_entry = {
	.ver = "gpipe_dc",
	.ops = &gpipe_ops,
};

// static int __init pipe_register(void) {
// 	return pipe_ops_register(&entry);
// }
// subsys_initcall(pipe_register);
