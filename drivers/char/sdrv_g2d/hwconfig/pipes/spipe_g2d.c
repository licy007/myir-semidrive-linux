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
#include "sdrv_g2d.h"
#include "../g2dlite_reg.h"
#include "g2d_common.h"

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

struct sp_sf_reg {
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
	DRM_FORMAT_R8
};

static struct g2d_pipe_capability sp_cap = {
	.nformats = ARRAY_SIZE(sp_formats),
	.yuv = 0,
	.yuv_fbc = 0,
	.xfbc = 0,
	.rotation = 0,
	.scaling = 0,
};

static unsigned int reg_value(unsigned int val,
	unsigned int src, unsigned int shift, unsigned int mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

static int spipe_init(struct g2d_pipe *pipe)
{
	struct sp_reg *reg = (struct sp_reg *)pipe->iomem_regs;
	if (!strcmp(pipe->name, SPIPE_NAME)) {
		memcpy(sp_cap.formats,sp_formats,sizeof(sp_formats));
		pipe->cap = &sp_cap;
	} if (!pipe->cap) {
		G2D_ERROR("%s capacities is invalid\n", pipe->name);
		return -EINVAL;
	}

	reg->clut_a_ctrl |= BIT(16);
	reg->clut_y_ctrl |= BIT(16);
	reg->clut_u_ctrl |= BIT(16);
	reg->clut_v_ctrl |= BIT(16);

	reg->sp_sdw_ctrl = 1;
	return 0;
}

static int spipe_set(struct g2d_pipe *pipe, int cmdid, struct g2d_layer *layer)
{
	//return spipe_g2d_set(pipe->iomem_regs, &gd->cmd_info, cmdid, layer);
	unsigned int val, size;
	unsigned long offset;
	struct sp_reg *reg = (struct sp_reg *)pipe->iomem_regs;
	struct pix_g2dcomp *comp = &layer->comp;
	struct sdrv_g2d *gd = pipe->gd;
	cmdfile_info *cmd = (cmdfile_info *)&gd->cmd_info;
	unsigned char pipe_id = layer->index;

	if (!cmdid) {
		/*pix comp*/
		val = reg_value(comp->bpv, 0, BPV_SHIFT, BPV_MASK);
		val = reg_value(comp->bpu, val, BPU_SHIFT, BPU_MASK);
		val = reg_value(comp->bpy, val, BPY_SHIFT, BPY_MASK);
		val = reg_value(comp->bpa, val, BPA_SHIFT, BPA_MASK);
		pipe_write_reg(cmd, 0, pipe_id, G2DLITE_SP_PIX_COMP, val);
		/*frm ctrl*/
		val = reg_value(comp->endin, 0, PIPE_ENDIAN_CTRL_SHIFT, PIPE_ENDIAN_CTRL_MASK);
		val = reg_value(comp->swap, val, PIPE_COMP_SWAP_SHIFT, PIPE_COMP_SWAP_MASK);
		val = reg_value(comp->is_yuv, val, PIPE_RGB_YUV_SHIFT, PIPE_RGB_YUV_MASK);
		val = reg_value(comp->uvswap, val, PIPE_UV_SWAP_SHIFT, PIPE_UV_SWAP_MASK);
		val = reg_value(comp->uvmode, val, PIPE_UV_MODE_SHIFT, PIPE_UV_MODE_MASK);
		val = reg_value(layer->ctx.data_mode, val,  PIPE_MODE_SHIFT, PIPE_MODE_MASK);
		val = reg_value(comp->buf_fmt, val, PIPE_FMT_SHIFT, PIPE_FMT_MASK);
		pipe_write_reg(cmd, 0, pipe_id, G2DLITE_SP_FRM_CTRL, val);
	}

	/*pipe related*/
	size = reg_value(layer->src_w - 1, 0, FRM_WIDTH_SHIFT, FRM_WIDTH_MASK);
	size = reg_value(layer->src_h - 1, size, FRM_HEIGHT_SHIFT, FRM_HEIGHT_MASK);
	offset = PIPE_OFFSET(reg, sp_frm_size);
	pipe_write_reg(cmd, cmdid, pipe_id, offset, size);

	pipe_write_reg(cmd, cmdid, pipe_id, PIPE_OFFSET(reg, sp_y_baddr_l),	 layer->addr_l[0]);
	pipe_write_reg(cmd, cmdid, pipe_id, PIPE_OFFSET(reg, sp_y_baddr_h), layer->addr_h[0]);
	pipe_write_reg(cmd, cmdid, pipe_id, PIPE_OFFSET(reg, sp_y_stride), layer->pitch[0] - 1);
	val = reg_value(layer->src_x, 0, FRM_X_SHIFT, FRM_X_MASK);
	val = reg_value(layer->src_y, val, FRM_Y_SHIFT, FRM_Y_MASK);
	offset = PIPE_OFFSET(reg, sp_frm_offset);
	pipe_write_reg(cmd, cmdid, pipe_id, offset, val);

	return 0;
}

static struct pipe_operation pipe_ops = {
	.init =	 spipe_init,
	.set = spipe_set,
};

struct ops_entry spipe_g2d_entry = {
	.ver = SPIPE_NAME,
	.ops = &pipe_ops,
};
