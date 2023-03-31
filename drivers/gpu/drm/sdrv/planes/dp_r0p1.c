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

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
#include <drm/drmP.h>
#else
#include <drm/drm_print.h>
#endif
#include <drm/drm_atomic.h>
#include <drm/drm_plane_helper.h>

#include "sdrv_plane.h"
#include "sdrv_dpc.h"

#define RDMA_JUMP           0x1000
#define GP0_JUMP            0x2000
#define GP1_JUMP            0x3000
#define SP0_JUMP            0x5000
#define SP1_JUMP            0x6000
#define MLC_JUMP            0x7000
#define AP_JUMP             0x9000
#define FBDC_JUMP           0xC000

#define DP_INT_RDMA_MASK				BIT(0)
#define DP_INT_RLE0_MASK				BIT(1)
#define DP_INT_MLC_MASK					BIT(2)
#define DP_INT_RLE1_MASK				BIT(3)
#define DP_INT_SDMA_DONE				BIT(4)
#define DP_INT_SOF_MASK					BIT(5)
#define DP_INT_EOF_MASK					BIT(6)

struct dp_reg {
	u32 dp_ctrl;
	u32 dp_flc_ctrl;
	u32 dp_flc_update;
	u32 dp_size;
	u32 sdma_ctrl;
	u32 reserved_0x14_0x1C[3];
	u32 dp_int_mask;
	u32 dp_int_status;
};

struct rdma_chn_reg {
	u32 rdma_dfifo_wml;
	u32 rdma_dfifo_depth;
	u32 rdma_cfifo_depth;
	union _rdma_ch_prio {
		u32 val;
		struct _rdma_ch_prio_ {
			u32 p0: 6;
			u32 reserved0: 2;
			u32 p1: 6;
			u32 reserved1: 2;
			u32 sche: 6;
			u32 reserved2: 10;
		} bits;
	} rdma_ch_prio;
	union _rdma_burst {
		u32 val;
		struct _rdma_burst_ {
			u32 len: 3;
			u32 mode: 1;
			u32 reserved: 28;
		} bits;
	} rdma_burst;
	u32 rdma_axi_user;
	u32 rdma_axi_ctrl;
	u32 rdmapres_wml;
};
struct dp_rdma_reg {
	struct rdma_chn_reg rdma_chn[9];
	u32 reserved_0x120_0x3fc[184];
	u32 rdma_ctrl;
	u32 reserved_0x404_0x4fc[63];
	u32 rdma_dfifo_full;
	u32 rdma_dfifo_empty;
	u32 rdma_cfifo_full;
	u32 rdma_cfifo_empty;
	u32 rdma_ch_idle;
	u32 reserved_0x514_0x51c[3];
	u32 rdma_int_mask;
	u32 rdma_int_status;
	u32 reserved_0x528_0x53c[6];
	u32 rdma_debug_ctrl;
	u32 rdma_debug_sta;
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

struct dp_mlc_reg {
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
			u32 mlc_pd_mode: 5;
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

struct dp_ap_reg {
	u32 ap_pix_cmop;
	u32 ap_frm_ctrl;
	u32 ap_frm_size;
	u32 ap_baddr_l;
	u32 ap_baddr_h;
	u32 reserved_0x14_0x28[6];
	u32 ap_stride;
	u32 reserved_0x30_0x3c[4];
	u32 ap_frm_offset;
	u32 reserved_0x44_0xdfc[879];
	u32 ap_sw_rst;
	u32 reserved_0xe04_0xefc[63];
	u32 ap_sdw_ctrl;
};

struct fbdc_reg {
	union _fbdc_ctrl {
		u32 val;
		struct _fbdc_ctrl_ {
			u32 fbdc_en: 1;
			u32 mode_v3_1_en: 1;
			u32 hdr_bypass: 1;
			u32 inva_sw_en: 1;
			u32 reserved: 27;
			u32 fbdc_sw_rst: 1;
		} bits;
	} fbdc_ctrl;
	u32 fbdc_cr_inval;
	u32 fbdc_cr_val_0;
	u32 fbdc_cr_val_1;
	u32 fbdc_cr_ch0123_0;
	u32 fbdc_cr_ch0123_1;
	u32 fbdc_cr_filter_ctrl;
	u32 fbdc_cr_filter_status;
	u32 fbdc_cr_core_id_0;
	u32 fbdc_cr_core_id_1;
	u32 fbdc_cr_core_id_2;
	u32 fbdc_cr_core_ip;
};

#define REGISTER	volatile struct
typedef REGISTER dp_reg * dp_reg_t;

static uint32_t dp_irq_handler(struct sdrv_dpc *dpc)
{
	dp_reg_t reg = (dp_reg_t )dpc->regs;
	u32 val;
	uint32_t sdrv_val = 0;

	val = reg->dp_int_status;
	reg->dp_int_status = val;

	if (val & DP_INT_EOF_MASK) {
		sdrv_val |= SDRV_TCON_EOF_MASK;
	}

	if (val & DP_INT_MLC_MASK) {
		sdrv_val |= SDRV_MLC_MASK;
	}

	if (val & DP_INT_RDMA_MASK) {
		sdrv_val |= SDRV_RDMA_MASK;
	}

	return sdrv_val;
}

static uint32_t dp_mlc_irq_handler(struct sdrv_dpc *dpc)
{
	REGISTER dp_mlc_reg *mlc_reg = (REGISTER dp_mlc_reg *)(dpc->regs + MLC_JUMP);
	u32 val;

	val = mlc_reg->mlc_int_status;
	mlc_reg->mlc_int_status = val;

	return val;
}

static uint32_t dp_rdma_irq_handler(struct sdrv_dpc *dpc)
{
	REGISTER dp_rdma_reg *rdma_reg = (REGISTER dp_rdma_reg *)(dpc->regs + RDMA_JUMP);
	u32 val;

	val = rdma_reg->rdma_int_status;
	rdma_reg->rdma_int_status = val;

	return val;
}

static void dp_sw_reset(struct sdrv_dpc *dpc)
{
	dp_reg_t reg = (dp_reg_t )dpc->regs;

	reg->dp_ctrl |= BIT(31);
	mdelay(1);
	reg->dp_ctrl &= ~ BIT(31);
}

static void dp_irq_init(struct sdrv_dpc *dpc)
{
	dp_reg_t reg = (dp_reg_t )dpc->regs;
	struct dp_rdma_reg *rdma_reg = (struct dp_rdma_reg *)(dpc->regs + RDMA_JUMP);
	struct dp_mlc_reg *mlc_reg = (struct dp_mlc_reg *)(dpc->regs + MLC_JUMP);

	reg->dp_int_mask = 0x7A; //Default enable MLC&RDMA int.
	rdma_reg->rdma_int_mask = 0; //Default enable all int.
	mlc_reg->mlc_int_mask = 0x1; //Default enable FLU&ERR int.
}

static void dp_flc_kick_auto_enable(struct sdrv_dpc *dpc, bool enable)
{
	dp_reg_t reg = (dp_reg_t)dpc->regs;

	if (enable)
		reg->dp_ctrl |= BIT(1);
	else
		reg->dp_ctrl &= ~BIT(1);
}
static void dp_re_ram_type_set(struct sdrv_dpc *dpc, uint32_t type)
{
	dp_reg_t reg = (dp_reg_t)dpc->regs;

	reg->dp_ctrl &= ~(0xf << 3);
	reg->dp_ctrl |= (type << 3);
}

static void dp_vblank_enable(struct sdrv_dpc *dpc, bool enable)
{
	struct dp_reg *reg = (struct dp_reg *)dpc->regs;
	u32 val;

	val = enable ? 0 : 1;

	if (val)
		reg->dp_int_mask |= DP_INT_EOF_MASK;
	else
		reg->dp_int_mask &= ~DP_INT_EOF_MASK;
}

static void dp_rdma_init(struct sdrv_dpc *dpc)
{
	REGISTER dp_rdma_reg *rdma_reg = (REGISTER dp_rdma_reg *)(dpc->regs + RDMA_JUMP);
	const uint16_t wml_cfg[] =
			{0x80, 0x40, 0x40, 0x80, 0x40, 0x40, 0x60, 0x60, 0x40};
	const uint16_t d_depth_cfg[] =
			{0x10, 0x08, 0x08, 0x10, 0x08, 0x08, 0x0c, 0x0c, 0x08};
	const uint16_t c_depth_cfg[] =
			{0x08, 0x04, 0x04, 0x08, 0x04, 0x04, 0x06, 0x06, 0x04};
	const uint8_t sche_cfg[] =
			{0x04, 0x01, 0x01, 0x04, 0x01, 0x01, 0x04, 0x01, 0x01};
	const uint8_t p1_cfg[] =
			{0x30, 0x20, 0x20, 0x30, 0x20, 0x20, 0x30, 0x20, 0x20};
	const uint8_t p0_cfg[] =
			{0x10, 0x08, 0x08, 0x10, 0x08, 0x08, 0x10, 0x08, 0x08};
	const uint8_t burst_mode_cfg[] =
			{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
	const uint8_t burst_len_cfg[] =
			{0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x5};
	int i;

	for(i = 0; i < ARRAY_SIZE(rdma_reg->rdma_chn); i++) {
		rdma_reg->rdma_chn[i].rdma_dfifo_wml = wml_cfg[i];
		rdma_reg->rdma_chn[i].rdma_dfifo_depth = d_depth_cfg[i];
		rdma_reg->rdma_chn[i].rdma_cfifo_depth = c_depth_cfg[i];

		rdma_reg->rdma_chn[i].rdma_ch_prio.bits.sche = sche_cfg[i];
		rdma_reg->rdma_chn[i].rdma_ch_prio.bits.p1 = p1_cfg[i];
		rdma_reg->rdma_chn[i].rdma_ch_prio.bits.p0 = p0_cfg[i];

		rdma_reg->rdma_chn[i].rdma_burst.bits.mode = burst_mode_cfg[i];
		rdma_reg->rdma_chn[i].rdma_burst.bits.len = burst_len_cfg[i];
		rdma_reg->rdma_chn[i].rdmapres_wml = 0xa00016;
	}

	rdma_reg->rdma_ctrl = 0x3;//cfg_load&arb_sel
}

static void dp_mlc_init(struct sdrv_dpc *dpc)
{
	REGISTER dp_mlc_reg *mlc_reg = (REGISTER dp_mlc_reg *)(dpc->regs + MLC_JUMP);
	const uint8_t prot_val[] = {0x07, 0x07, 0x07, 0x07};
	const uint32_t aflu_time[] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
	const uint8_t alpha_bld[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	const uint8_t layer_out[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	unsigned int i;

	for(i = 0; i < ARRAY_SIZE(mlc_reg->mlc_sf); i++) {
		mlc_reg->mlc_sf[i].mlc_sf_ctrl.bits.sf_en = 0;
		mlc_reg->mlc_sf[i].mlc_sf_ctrl.bits.prot_val = prot_val[i];
		mlc_reg->mlc_sf[i].mlc_sf_ctrl.bits.g_alpha_en = 1;

		mlc_reg->mlc_sf[i].mlc_sf_h_spos = 0;
		mlc_reg->mlc_sf[i].mlc_sf_v_spos = 0;
		mlc_reg->mlc_sf[i].mlc_sf_aflu_time = aflu_time[i];
		mlc_reg->mlc_sf[i].mlc_sf_g_alpha = 0xFF;
	}

	for(i = 0; i < ARRAY_SIZE(mlc_reg->mlc_path_ctrl); i++) {
		mlc_reg->mlc_path_ctrl[i].bits.alpha_bld_idx = alpha_bld[i];
		mlc_reg->mlc_path_ctrl[i].bits.layer_out_idx = layer_out[i];
	}

	/*billy*/
	mlc_reg->mlc_clk_ratio = 0xFFFF;
}

static void dp_fbdc_init(struct sdrv_dpc *dpc)
{
	REGISTER fbdc_reg *reg = (REGISTER fbdc_reg *)(dpc->regs + FBDC_JUMP);

	reg->fbdc_cr_val_0 = 0x02000040;
	reg->fbdc_cr_val_1 = 0x02040044;
	reg->fbdc_cr_ch0123_1 = 0x01000000;
	reg->fbdc_ctrl.bits.hdr_bypass = 1;
	reg->fbdc_ctrl.bits.mode_v3_1_en = 1;
	reg->fbdc_ctrl.bits.fbdc_en = 0;
	// en
	reg->fbdc_cr_filter_ctrl |= BIT(0);
}

static void dp_set_fbdc(void *dpc_regs, bool is_fbdc_cps)
{
	REGISTER fbdc_reg *freg = (REGISTER fbdc_reg *)(dpc_regs + FBDC_JUMP);
	volatile u32 tmp = 0;

	if (is_fbdc_cps) {
		tmp = freg->fbdc_cr_inval;
		tmp |= BIT(9);     //NOTIFY
		tmp &= ~(0xF << 4);//REQUESTER_O
		tmp |= 0x3 << 4;
		tmp |= 0x1;        //PENDING_O
		freg->fbdc_cr_inval = tmp;

		freg->fbdc_ctrl.val |= BIT(0); //en
	} else {
		freg->fbdc_ctrl.val &= ~ BIT(0); //en
	}
}

static int dp_update(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count)
{
	int i;
	struct sdrv_pipe *pipe;
	bool is_fbdc_cps = false;

	for (i = 0; i < dpc->num_pipe; i++) {
		pipe = dpc->pipes[i];

		if (!pipe->enable_status)
			continue;

		if (pipe->fbdc_status)
			is_fbdc_cps = true;
	}

	dp_set_fbdc(dpc->regs, is_fbdc_cps);

	return 0;
}

extern void dc_crc32_flush(void *dpc_regs);

static void dp_flush(struct sdrv_dpc *dpc)
{
	dp_reg_t reg = (dp_reg_t)dpc->regs;

	reg->dp_flc_ctrl |= BIT(1);
	reg->dp_flc_ctrl |= BIT(0);

	if (dpc->kick_sel) {
		dc_crc32_flush(dpc->crc_dc_regs);
	}

}

static int dp_update_done(struct sdrv_dpc *dpc)
{
	int i;
	int value;
	const int loop_count = 100;
	dp_reg_t reg = (dp_reg_t)dpc->regs;

	for (i = 0; i < loop_count; i++) {
		usleep_range(500, 500+10);

		value = reg->dp_flc_ctrl;
		if (!value)
			break;
	}

	if (i == loop_count) {
		pr_info("%s crtc-%d dp wait update done timeout\n", __func__, dpc->crtc->base.index);
		//dp_dump_registers(dpc);
		return -ERANGE;
	}

	return 0;
}

static void dp_enable(struct sdrv_dpc *dpc, bool enable)
{
	dp_reg_t reg = (dp_reg_t )dpc->regs;

	if (dpc->kick_sel) {
		reg->dp_ctrl &= ~BIT(1);
	} else {
		reg->dp_ctrl |= BIT(1);
	}

	if (!enable) {

		if (dpc->kick_sel)
			dc_crc32_flush(dpc->crc_dc_regs);

		dp_update_done(dpc);
		reg->dp_ctrl &= ~BIT(0);
	} else {
		dp_sw_reset(dpc);
		reg->dp_ctrl |= BIT(0);
	}
}

static void dp_init(struct sdrv_dpc *dpc)
{
	// bottom to top
	if (!dpc->inited) {
		dpc->inited = true;

		if (dpc->fmt_change_cfg.enable) {
			_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 1, "gpipe0_dp", 0x2000);
		}
		else {
			_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 1, "gpipe0_dp", 0x2000);
			_add_pipe(dpc, DRM_PLANE_TYPE_OVERLAY, 2, "gpipe1_dp", 0x3000);
			_add_pipe(dpc, DRM_PLANE_TYPE_OVERLAY, 3, "spipe", 0x5000);
			_add_pipe(dpc, DRM_PLANE_TYPE_OVERLAY, 4, "spipe", 0x6000);
		}
	}

	dp_sw_reset(dpc);
	dp_irq_init(dpc);
	dp_flc_kick_auto_enable(dpc, true);
	dp_rdma_init(dpc);
	dp_mlc_init(dpc);
	dp_fbdc_init(dpc);
}

static int dp_modeset(struct sdrv_dpc *dpc, struct drm_display_mode *mode, u32 bus_flags)
{
	void __iomem *base = dpc->regs;
	dp_reg_t reg = (dp_reg_t )base;

	if (dpc->combine_mode == 3) {
		reg->dp_size = ((mode->vdisplay - 1) << 16) |(mode->hdisplay / 2 - 1);
		if (mode->hdisplay / 2 > 2048)
			dpc->re_ram_type = 0;
		else
			dpc->re_ram_type = 2;
	} else {
		reg->dp_size = ((mode->vdisplay - 1) << 16) |(mode->hdisplay - 1);
		if (mode->hdisplay > 2048)
			dpc->re_ram_type = 0;
		else
			dpc->re_ram_type = 2;
	}

	dp_re_ram_type_set(dpc, dpc->re_ram_type);

	return 0;
}

static void dp_uninit(struct sdrv_dpc *dpc)
{

}

static void dp_shutdown(struct sdrv_dpc *dpc)
{
	int i;
	REGISTER dp_mlc_reg *mlc_reg = (REGISTER dp_mlc_reg *)(dpc->regs + MLC_JUMP);
	dp_reg_t reg = (dp_reg_t)dpc->regs;

	for (i = 0; i < ARRAY_SIZE(mlc_reg->mlc_sf); i++) {
		mlc_reg->mlc_sf[i].mlc_sf_ctrl.bits.sf_en = 0;
	}

	/*clear all mlc path*/
	for (i = 0; i < ARRAY_SIZE(mlc_reg->mlc_path_ctrl); i++) {
		mlc_reg->mlc_path_ctrl[i].val = 0xF777F;
	}

	// di trig
	reg->dp_flc_ctrl |= BIT(1);
	reg->dp_flc_ctrl |= BIT(0);

	dp_enable(dpc, false);
}

static void dp_dump_registers(struct sdrv_dpc *dpc)
{
	dump_registers(dpc, 0x0000, 0x002c);

	dump_registers(dpc, 0x1000, 0x111c);
	dump_registers(dpc, 0x1400, 0x1400);
	dump_registers(dpc, 0x1500, 0x154c);

	/*g-pipe0*/
	dump_registers(dpc, 0x2000, 0x204c);
	dump_registers(dpc, 0x2100, 0x212c);
	dump_registers(dpc, 0x2200, 0x222c);
	dump_registers(dpc, 0x2300, 0x230c);
	dump_registers(dpc, 0x2400, 0x241c);
	dump_registers(dpc, 0x2500, 0x250c);
	dump_registers(dpc, 0x2f00, 0x2f00);

	/*g-pipe1*/
	dump_registers(dpc, 0x3000, 0x304c);
	dump_registers(dpc, 0x3100, 0x312c);
	dump_registers(dpc, 0x3200, 0x322c);
	dump_registers(dpc, 0x3300, 0x330c);
	dump_registers(dpc, 0x3400, 0x341c);
	dump_registers(dpc, 0x3500, 0x350c);
	dump_registers(dpc, 0x3f00, 0x3f00);

	/*s-pipe0*/
	dump_registers(dpc, 0x5000, 0x504c);
	dump_registers(dpc, 0x5100, 0x514c);
	dump_registers(dpc, 0x5200, 0x521c);
	dump_registers(dpc, 0x5f00, 0x5f00);

	/*s-pipe1*/
	dump_registers(dpc, 0x6000, 0x604c);
	dump_registers(dpc, 0x6100, 0x614c);
	dump_registers(dpc, 0x6200, 0x621c);
	dump_registers(dpc, 0x6f00, 0x6f00);

	dump_registers(dpc, 0x7000, 0x70bc);
	dump_registers(dpc, 0x7200, 0x724c);

	dump_registers(dpc, 0x9000, 0x904c);
	dump_registers(dpc, 0xc000, 0xc02c);
}

const struct dpc_operation dp_r0p1_ops = {
	.init = dp_init,
	.uninit = dp_uninit,
	.irq_handler = dp_irq_handler,
	.mlc_irq_handler = dp_mlc_irq_handler,
	.rdma_irq_handler = dp_rdma_irq_handler,
	.vblank_enable = dp_vblank_enable,
	.enable = dp_enable,
	.update = dp_update,
	.flush = dp_flush,
	.sw_reset = dp_sw_reset,
	.modeset = dp_modeset,
	.shutdown = dp_shutdown,
	.update_done = dp_update_done,
	.dump = dp_dump_registers,
};


