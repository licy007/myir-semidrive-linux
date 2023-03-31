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
#include <linux/clk.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
#include <drm/drmP.h>
#else
#include <drm/drm_print.h>
#endif
#include <drm/drm_atomic.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_connector.h>

#include "sdrv_plane.h"
#include "sdrv_dpc.h"

#define RDMA_JUMP           0x1000
#define GP_JUMP             0x2000
#define SP_JUMP             0x5000
#define TCON_JUMP           0x9000
#define CSC_JUMP            0xA000
#define GAMMA_DITHER_JUMP   0xC000
#define CRC32_JUMP          0xE000

#define SDRV_TCON_EOF_MASK 		BIT(6)
#define	SDRV_DC_UNDERRUN_MASK	BIT(8)
#define TCON_SOF_MASK       BIT(3)
#define TCON_EOF_MASK       BIT(4)
#define DC_UNDERRUN_MASK	BIT(6)

struct dc_reg {
	u32 dc_ctrl;
	u32 dc_flc_ctrl;
	u32 dc_flc_update;
	u32 reserved_0x0c;
	u32 sdma_ctrl;
	u32 reserved_0x14_0x1c[3];
	u32 dc_int_mask;
	u32 dc_int_status;
	u32 reserved_0x28_0xfc[54];
	u32 dc_sf_flc_ctrl;
	u32 reserved_0x104_0x11c[7];
	u32 dc_sf_int_mask;
	u32 dc_sf_int_status;
};

struct rdma_chn_reg {
	u32 rdma_dfifo_wml;
	u32 rdma_dfifo_depth;
	u32 rdma_cfifo_depth;
	u32 rdma_ch_prio;
	u32 rdma_burst;
	u32 rdma_axi_user;
	u32 rdma_axi_ctrl;
	u32 rdmapres_wml;
};
struct dc_rdma_reg {
	struct rdma_chn_reg rdma_chn[4];
	u32 reserved_0x80_0xfc[32];
	u32 rdma_ctrl;
	u32 reserved_0x104_0x1fc[63];
	u32 rdma_dfifo_full;
	u32 rdma_dfifo_empty;
	u32 rdma_cfifo_full;
	u32 rdma_cfifo_empty;
	u32 rdma_ch_idle;
	u32 reserved_0x214_0x21c[3];
	u32 rdma_int_mask;
	u32 rdma_int_status;
	u32 reserved_0x228_0x23c[6];
	u32 rdma_debug_ctrl;
	u32 rdma_debug_sta;
	u32 reserved_0x248_0x3fc[110];
	struct rdma_chn_reg s_rdam_chn;
	u32 reserved_0x420_0x4fc[56];
	u32 s_rdma_ctrl;
	u32 reserved_0x504_0x5fc[63];
	u32 s_rdma_dfifo_full;
	u32 s_rdma_dfifo_empty;
	u32 s_rdma_cfifo_full;
	u32 s_rdma_cfifo_empty;
	u32 s_rdma_idle;
	u32 reserved_0x614_0x61c[3];
	u32 s_rdma_int_mask;
	u32 s_rdma_int_status;
	u32 reserved_0x628_0x63c[5];
	u32 s_rdma_debug_ctrl;
	u32 s_rdma_debug_sta;
};

struct mlc_sf_reg {
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

struct dc_mlc_reg {
	struct mlc_sf_reg mlc_sf[4];
	u32 reserved_0xc0_0x1fc[80];
	u32 mlc_path_ctrl[5];
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

struct tcon_layer_kick_reg {
	u32 tcon_layer_kick_coor;
	u32 tcon_layer_kick_en;
};
struct tcon_reg {
	u32 tcon_h_para_1;
	u32 tcon_h_para_2;
	u32 tcon_v_para_1;
	u32 tcon_v_para_2;
	u32 tcon_ctrl;
	u32 reserved_0x14_0x1c[3];
	struct tcon_layer_kick_reg layer_kick[19];
	u32 reserved_0xb8_0xfc[18];
	u32 tcon_underrun_cnt;
	u32 reserved_0x104_0x3fc[191];
	u32 tcon_post_ctrl;
	u32 reserved_0x404_0x40c[3];
	u32 tcon_post_ch_ctrl[4];
	u32 reserved_0x420_0x42c[4];
	u32 tcon_post_pos[4];
};

struct dc_csc_reg {
	u32 csc_ctrl;
	u32 csc_coef1;
	u32 csc_coef2;
	u32 csc_coef3;
	u32 csc_coef4;
	u32 csc_coef5;
	u32 csc_coef6;
	u32 csc_coef7;
	u32 csc_coef8;
};

struct gamma_dither_reg {
	u32 gamma_ctrl;
	u32 dither_ctrl;
};

struct crc32_block_reg {
	u32 crc32_block_ctrl0;
	u32 crc32_block_ctrl1;
	u32 crc32_block_expect_data;
	u32 crc32_block_result_data;
};
struct crc32_reg {
	u32 crc32_ctrl;
	u32 crc32_int_st;
	u32 crc32_int_mask;
	u32 reserved_0x0c;
	struct crc32_block_reg crc32_blk[8];
};

typedef struct dc_reg * dc_reg_t;

static void dc_underrun_clear_mode(void *dpc_regs, int mode, int is_master)
{
	dc_reg_t reg = (dc_reg_t )dpc_regs;

	int slf_mask = is_master ? 0: BIT(1);

	if (mode)
		reg->dc_ctrl |= (BIT(3) | slf_mask);
	else
		reg->dc_ctrl &= ~ (BIT(3) | slf_mask);
}

static void dc_mlc_discard_mode(void *dpc_regs, int mode)
{
	dc_reg_t reg = (dc_reg_t )dpc_regs;

	if (mode)
		reg->dc_ctrl |= BIT(2);
	else
		reg->dc_ctrl &= ~ BIT(2);
}

static void dc_irq_init(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	dc_reg_t reg = (dc_reg_t )base;
	struct dc_rdma_reg *rdma_reg = (struct dc_rdma_reg *)(base + RDMA_JUMP);
	struct dc_mlc_reg *mlc_reg = (struct dc_mlc_reg *)(base + MLC_JUMP);
	struct crc32_reg *crc_reg =  (struct crc32_reg *)(base + CRC32_JUMP);

	reg->dc_int_mask = 0xFFFFFFF;
	reg->dc_sf_int_mask = 0xFFFFFFF;
	rdma_reg->rdma_int_mask = 0x7F;
	mlc_reg->mlc_int_mask = 0x1FFF;
	crc_reg->crc32_int_mask = 0xFFFF;
}

static void dc_rdma_init(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	volatile struct dc_rdma_reg *rdma_reg = (struct dc_rdma_reg *)(base + RDMA_JUMP);
	const uint16_t wml_cfg[] = {128, 64, 32, 32};
	const uint16_t d_depth_cfg[] = {16, 8, 4, 4};
	const uint16_t c_depth_cfg[] = {8, 4, 2, 2};
	const uint8_t sche_cfg[] = {0x04, 0x01, 0x01, 0x04};
	const uint8_t p1_cfg[] = {0x30, 0x20, 0x20, 0x30};
	const uint8_t p0_cfg[] = {0x10, 0x08, 0x08, 0x10};
	const uint8_t burst_mode_cfg[] = {0x0, 0x0, 0x0, 0x0};
	const uint8_t burst_len_cfg[] = {0x05, 0x05, 0x05, 0x05};
	int i;

	for(i = 0; i < ARRAY_SIZE(rdma_reg->rdma_chn); i++) {
		rdma_reg->rdma_chn[i].rdma_dfifo_wml = wml_cfg[i];
		rdma_reg->rdma_chn[i].rdma_dfifo_depth = d_depth_cfg[i];
		rdma_reg->rdma_chn[i].rdma_cfifo_depth = c_depth_cfg[i];

		rdma_reg->rdma_chn[i].rdma_ch_prio = (sche_cfg[i] << 16) | ((p1_cfg[i]) << 8) | p0_cfg[i];
		rdma_reg->rdma_chn[i].rdma_burst = (burst_mode_cfg[i] << 3) | burst_len_cfg[i];
	}

	rdma_reg->rdma_ctrl = 0x3;//cfg_load&arb_sel
}

static void dc_csc_set_bypass(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	struct dc_csc_reg *csc_reg = (struct dc_csc_reg *)(base + CSC_JUMP);

	csc_reg->csc_ctrl |= BIT(0);
}

static void dc_gamma_set_bypass(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	struct gamma_dither_reg *csc_reg = (struct gamma_dither_reg *)(base
			+ GAMMA_DITHER_JUMP);

	csc_reg->gamma_ctrl |= BIT(0);
}

static void dc_dither_init(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	struct gamma_dither_reg *csc_reg = (struct gamma_dither_reg *)(base
			+ GAMMA_DITHER_JUMP);

	csc_reg->dither_ctrl |= (BIT(6) | BIT(0));
}

static void dc_mlc_init(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	struct dc_mlc_reg *mlc_reg = (struct dc_mlc_reg *)(base + MLC_JUMP);
	const u32 aflu_time[] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
	unsigned int i;

	for(i = 0; i < ARRAY_SIZE(mlc_reg->mlc_sf); i++) {
		/*init prot_val && g_alpha_en*/
		mlc_reg->mlc_sf[i].mlc_sf_ctrl = (0x7 << 8) | ((0x1 << 2) & (~ BIT(0)));

		mlc_reg->mlc_sf[i].mlc_sf_h_spos = 0;
		mlc_reg->mlc_sf[i].mlc_sf_v_spos = 0;
		mlc_reg->mlc_sf[i].mlc_sf_aflu_time = aflu_time[i];
		mlc_reg->mlc_sf[i].mlc_sf_g_alpha = 0xFF;
	}

	for(i = 0; i < ARRAY_SIZE(mlc_reg->mlc_path_ctrl); i++)
		mlc_reg->mlc_path_ctrl[i] = 0xF000F;

	mlc_reg->mlc_clk_ratio = 0xFFFF;
}

static void dc_mlc_disable(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	struct dc_mlc_reg *mlc_reg = (struct dc_mlc_reg *)(base + MLC_JUMP);
	int i;

	for(i = 0; i < ARRAY_SIZE(mlc_reg->mlc_sf); i++) {
		mlc_reg->mlc_sf[i].mlc_sf_ctrl &= ~ BIT(0);
	}

	for(i = 0; i < ARRAY_SIZE(mlc_reg->mlc_path_ctrl); i++)
		mlc_reg->mlc_path_ctrl[i] = 0xF000F;
}

static void dc_init(struct sdrv_dpc *dpc)
{
	if (!dpc->inited) {
		dpc->inited = true;

		if(dpc->fmt_change_cfg.enable) {
			_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 2, "gpipe_dc", 0x2000);
		}
		else {
			_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 1, "spipe", 0x5000);
			_add_pipe(dpc, DRM_PLANE_TYPE_OVERLAY, 2, "gpipe_dc", 0x2000);
		}
	}

	// dc_r0p1_sw_reset(dpc);

	dc_underrun_clear_mode(dpc->regs, 1, dpc->is_master);
	dc_mlc_discard_mode(dpc->regs, 0);
	dc_irq_init(dpc->regs);
	dc_rdma_init(dpc->regs);
	dc_csc_set_bypass(dpc->regs);
	dc_gamma_set_bypass(dpc->regs);
	dc_dither_init(dpc->regs);
	dc_mlc_init(dpc->regs);

	PRINT("add %d pipes for dc\n", dpc->num_pipe);
}

static void __maybe_unused sw_reset_pipes(struct sdrv_dpc *dpc) {
	int i;

	for (i = 0;i <  dpc->num_pipe;i++) {
		struct sdrv_pipe *p = dpc->pipes[i];

		if (p->ops->sw_reset)
			p->ops->sw_reset(p);
	}
}

static uint32_t dc_irq_handler(struct sdrv_dpc *dpc)
{
	dc_reg_t reg = (dc_reg_t )dpc->regs;
	volatile u32 val;
	uint32_t sdrv_val = 0;

	val = reg->dc_int_status;
	reg->dc_int_status = val;

	if(val & TCON_EOF_MASK) {
		sdrv_val |= SDRV_TCON_EOF_MASK;
		//schedule_work(&dpc->vsync_work);
	}

	if(val & DC_UNDERRUN_MASK) {
		sdrv_val |= SDRV_DC_UNDERRUN_MASK;
		// sw_reset_pipes(dpc);
	}

	// if(val & DC_MLC_MASK) {
	// 	sdrv_val |= SDRV_MLC_MASK;
	// 	schedule_work(&dpc->mlc_int_work);
	// }

	// if(val & DC_RDMA_MASK) {
	// 	sdrv_val |= SDRV_RDMA_MASK;
	// 	schedule_work(&dpc->rdma_int_work);
	// }

	return sdrv_val;
}

static void dc_vblank_enable(struct sdrv_dpc *dpc, bool enable)
{
	dc_reg_t reg = (dc_reg_t )dpc->regs;
	u32 val;

	val = (enable ? 0 : 1);
	if (val)
		reg->dc_int_mask |= TCON_EOF_MASK;
	else
		reg->dc_int_mask &= ~TCON_EOF_MASK;
}

static void dc_enable(struct sdrv_dpc *dpc, bool enable)
{
	void __iomem *base = dpc->regs;
	dc_reg_t reg = (dc_reg_t )base;
	volatile struct tcon_reg *c_reg = (struct tcon_reg *)(base + TCON_JUMP);

	if(enable) {
		reg->dc_flc_ctrl |= 0x3;// di_triggle&flc_triggle
		udelay(500);
		reg->dc_flc_update = 1; //force update
		c_reg->tcon_ctrl |= BIT(0);//TCON enable
	} else {
		dc_mlc_disable(reg);
	}
}

static int dc_update(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count)
{
	int i;
	struct sdrv_pipe *pipe;
	dc_reg_t reg = (dc_reg_t )dpc->regs;

	for (i = 0; i < dpc->num_pipe; i++) {
		pipe = dpc->pipes[i];

		if (!pipe->enable_status)
			continue;

		reg->dc_flc_ctrl |= BIT(1);
	}

	return 0;
}

static void dc_flush(struct sdrv_dpc *dpc)
{
	void __iomem *base = dpc->regs;
	dc_reg_t reg = (dc_reg_t )base;

	if (dpc->kick_sel) {
		reg->dc_flc_ctrl |= BIT(1);
	}

	reg->dc_flc_ctrl |= BIT(0);
}

static int dc_check_tcon(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	dc_reg_t reg = (dc_reg_t )base;
	volatile struct tcon_reg *c_reg = (struct tcon_reg *)(base + TCON_JUMP);

	if (c_reg->tcon_ctrl & 0x1) {
		reg->dc_flc_ctrl |= BIT(2);
		return 1;
	}

	return 0;
}

static int dc_modeset_(void *dpc_regs, struct sdrv_dpc_mode *mode, u32 bus_flags, int combine)
{
	void __iomem *base = dpc_regs;
	dc_reg_t reg = (dc_reg_t )base;
	volatile struct tcon_reg *c_reg = (struct tcon_reg *)(base + TCON_JUMP);
	volatile struct dc_mlc_reg *mlc_reg = (struct dc_mlc_reg *)(base + MLC_JUMP);
	u32 hsbp, hsync, vsbp, vsync, i, x_kick, y_kick;
	u32 hdisplay, htotal;
	u8 clk_pol, de_pol, vysnc_pol, hsync_pol;

	hsbp = mode->htotal - mode->hsync_start;
	hsync = mode->hsync_end - mode->hsync_start;
	htotal = mode->htotal;
	hdisplay = mode->hdisplay;

	if (combine == 3) {
		hsbp /= 2;
		hsync /= 2;
		htotal /= 2;
		hdisplay /= 2;
	}
	c_reg->tcon_h_para_1 = ((hdisplay - 1) << 16) | (htotal - 1);
	c_reg->tcon_h_para_2 = ((hsbp - 1) << 16) | (hsync - 1);

	c_reg->tcon_v_para_1 = ((mode->vdisplay -1) << 16) | (mode->vtotal -1);

	vsbp = mode->vtotal - mode->vsync_start;
	vsync = mode->vsync_end - mode->vsync_start;
	c_reg->tcon_v_para_2 = ((vsbp -1) << 16) | (vsync -1);

	for (i = 0; i < ARRAY_SIZE(c_reg->layer_kick); i++) {
		if(i < 2)
			y_kick = mode->vdisplay + 4;
		else
			y_kick = mode->vdisplay + 5;
		if(y_kick > mode->vtotal - 1)
			y_kick = mode->vtotal -1;

		x_kick = htotal / 2 + i;
		if(x_kick > htotal - 1)
			x_kick = htotal - 1;

		c_reg->layer_kick[i].tcon_layer_kick_coor = (y_kick << 16) | x_kick;
		c_reg->layer_kick[i].tcon_layer_kick_en = 1;
	}

	if(bus_flags & DRM_BUS_FLAG_PIXDATA_NEGEDGE) clk_pol = 1;
	else clk_pol = 0;

	if(bus_flags & DRM_BUS_FLAG_DE_LOW) de_pol = 1;
	else de_pol = 0;

	// de_pol = 1;

	if(mode->flags & DRM_MODE_FLAG_NVSYNC) vysnc_pol = 1;
	else vysnc_pol= 0;

	if(mode->flags & DRM_MODE_FLAG_NHSYNC) hsync_pol = 1;
	else hsync_pol = 0;

	c_reg->tcon_ctrl |= (1 << 5) | (clk_pol << 4) | (de_pol << 3) |
		(vysnc_pol << 2) | (hsync_pol << 1);

	reg->dc_flc_ctrl |= BIT(2);

	for (i = 0; i < 4; i++)
		mlc_reg->mlc_sf[i].mlc_sf_size =
			 (mode->vdisplay - 1) << 16 | (hdisplay -1);

	return 0;
}

static int dc_modeset(struct sdrv_dpc *dpc, struct drm_display_mode *mode, u32 bus_flags)
{
	int ret = 0;
	unsigned long pll_clk = 0;
	struct sdrv_dpc_mode sd_mode;

	ret = dc_check_tcon(dpc->regs);
	if (ret)
		return 0;

	drm_mode_convert_sdrv_mode(mode, &sd_mode);

	if (dpc->fmt_change_cfg.enable &&
		(dpc->fmt_change_cfg.g2d_output_format == DRM_FORMAT_R8)) {
		sd_mode.hdisplay *= 2;
		sd_mode.hsync_start *= 2;
		sd_mode.hsync_end *= 2;
		sd_mode.htotal *=2;
		sd_mode.hskew *= 2;
	}

	if (dpc->pclk == NULL) {
		dpc->pclk = devm_clk_get(&dpc->dev, "pll_clk");
		if (!IS_ERR(dpc->pclk))
		{
			if (dpc->combine_mode == 3)
				pll_clk = sd_mode.htotal / 2 * sd_mode.vtotal * sd_mode.vrefresh * 7 / (dpc->pclk_div + 1);
			else
				pll_clk = sd_mode.htotal * sd_mode.vtotal * sd_mode.vrefresh * 7 / (dpc->pclk_div + 1);

			ret = clk_set_rate(dpc->pclk, pll_clk);
			if (ret)
				DRM_ERROR("%s clk_set_rate clk:%ld err, ret:%d\n", __func__, pll_clk, ret);
			else
				DRM_INFO("set pll clk h:%d, v:%d, fps:%d, clock:%ld pclk_div:%d\n",
					sd_mode.htotal, sd_mode.vtotal, sd_mode.vrefresh, pll_clk, dpc->pclk_div);
		}
		else
			DRM_ERROR("%s get pll_clk err ret:%d\n", __func__, IS_ERR(dpc->pclk));
	}

	dc_modeset_(dpc->regs, &sd_mode, bus_flags, dpc->combine_mode);

	return 0;
}

static void dc_mlc_select(struct sdrv_dpc *dpc, int mlc_select)
{
	struct dc_mlc_reg *mlc_reg = (struct dc_mlc_reg *)(dpc->regs + MLC_JUMP);
	u32 tmp = 0;
	int zpos = 2;
	int hwid = 4;

	if (mlc_select) {
		hwid = 4;
		zpos = 4;
	} else {
		hwid =3;
		zpos = 3;
	}

	/*set layer out idx*/
	tmp = mlc_reg->mlc_path_ctrl[hwid];
	tmp = (~0xF & tmp) | zpos;
	mlc_reg->mlc_path_ctrl[hwid] = tmp;

	/*set alpha bld idx*/
	tmp = mlc_reg->mlc_path_ctrl[zpos];
	tmp = (~(0xF << 16) & tmp) | ((hwid) << 16);

	mlc_reg->mlc_path_ctrl[zpos] = tmp;
	mlc_reg->mlc_sf[hwid - 1].mlc_sf_ctrl |= BIT(0);
}

static void dc_shutdown(struct sdrv_dpc *dpc)
{
	dc_reg_t reg = (dc_reg_t )dpc->regs;

	reg->dc_ctrl |= BIT(31);
	udelay(1);
	reg->dc_ctrl &= ~ BIT(31);
}

static void dc_uninit(struct sdrv_dpc *dpc)
{
	ENTRY();
}

static void dc_dump_registers(struct sdrv_dpc *dpc)
{
	dump_registers(dpc, 0x0000, 0x002c);

	dump_registers(dpc, 0x1000, 0x107c);
	dump_registers(dpc, 0x1100, 0x1100);
	dump_registers(dpc, 0x1200, 0x124c);

	/*g-pipe0*/
	dump_registers(dpc, 0x2000, 0x204c);
	dump_registers(dpc, 0x2200, 0x222c);
	dump_registers(dpc, 0x2f00, 0x2f00);

	/*s-pipe0*/
	dump_registers(dpc, 0x5000, 0x504c);
	dump_registers(dpc, 0x5100, 0x514c);
	dump_registers(dpc, 0x5200, 0x521c);
	dump_registers(dpc, 0x5f00, 0x5f00);

	dump_registers(dpc, 0x7000, 0x70bc);
	dump_registers(dpc, 0x7200, 0x724c);

	dump_registers(dpc, 0x9000, 0x90bc);
	dump_registers(dpc, 0xa000, 0xa02c);
	dump_registers(dpc, 0xc000, 0xc00c);
	dump_registers(dpc, 0xe000, 0xe08c);
}

static int dc_update_done(struct sdrv_dpc *dpc)
{
	int i;
	int value;
	const int loop_count = 50;
	dc_reg_t reg = (dc_reg_t)dpc->regs;

	for (i = 0; i < loop_count; i++) {
		msleep(1);
		value = reg->dc_flc_ctrl;
		if (!value)
			break;
	}

	if (i == loop_count) {
		pr_err("%s crtc-%d dc wait update done timeout\n", __func__, dpc->crtc->base.index);
		//dc_dump_registers(dpc);
		return -ERANGE;
	}

	return 0;
}

void dc_crc32_flush(void *dpc_regs)
{
	void __iomem *base = dpc_regs;
	dc_reg_t reg = (dc_reg_t )base;
	struct crc32_reg *crc_reg =  (struct crc32_reg *)(base + CRC32_JUMP);

	reg->dc_flc_ctrl |= BIT(1);
	reg->dc_flc_ctrl |= BIT(0);

	crc_reg->crc32_int_st = 0xffff;
}

const struct dpc_operation dc_r0p1_ops = {
	.init = dc_init,
	.uninit = dc_uninit,
	.update = dc_update,
	.irq_handler = dc_irq_handler,
	.vblank_enable = dc_vblank_enable,
	.enable = dc_enable,
	.flush = dc_flush,
	.modeset = dc_modeset,
	.mlc_select = dc_mlc_select,
	.shutdown = dc_shutdown,
	.update_done = dc_update_done,
	.dump = dc_dump_registers,
};

