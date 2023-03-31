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
#include <linux/delay.h>
#include <uapi/drm/sdrv_g2d_cfg.h>
#include "g2dlite_reg.h"
#include "sdrv_g2d.h"
#include "g2d_common.h"

#define G2D_DUMP_HVSCALER 0

extern int g2d_dump_registers(struct sdrv_g2d *dev);

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

#define REGISTER  volatile struct

#define _EM(a, b) ((a-b)/4)

#define G2D_MLC_LAYER_COUNT 5
#define G2D_MLC_PATH_COUNT 6
#define G2D_RCHN_COUNT 12

#define G2D_RDMA_APIPE_CH 11
#define G2D_RDMA_WML_TOTAL 128
#define G2D_RDMA_DFIFO_TOTAL_DEPTH 16
#define G2D_RDMA_CFIFO_TOTAL_DEPTH 16

#define G2D_WDMA_WML_TOTAL 64
#define G2D_WDMA_DFIFO_TOTAL_DEPTH 8
#define G2D_WDMA_CFIFO_TOTAL_DEPTH 8

struct g2d_reg {
	u32 g2d_ctrl;
	u32 flc_ctrl;
	u32 reserved_0x8[1];
	u32 dp_size;
	u32 cmdfile_addr_l;
	u32 cmdfile_addr_h;
	u32 cmdfile_len;
	u32 cmdfile_cfg;
	//20
	u32 int_mask;
	u32 int_status;
	u32 reserved_0x28[2];
	//30
	u32 speed_adj;
};

typedef REGISTER g2d_reg * g2d_reg_t;

struct rdma_chn_reg {
	u32 rdma_dfifo_wml;
	u32 rdma_dfifo_depth;
	u32 rdma_cfifo_depth;
	union {
		u32 val;
		struct {
			u32 p0: 6;
			u32 reserved0: 2;
			u32 p1: 6;
			u32 reserved1: 2;
			u32 sche: 6;
			u32 reserved2: 10;
		} bits;
	} ch_prio;
	union {
		u32 val;
		struct _rdma_burst_ {
			u32 len: 3;
			u32 mode: 1;
			u32 reserved: 28;
		} bits;
	} burst;
	u32 rdma_axi_user;
	u32 rdma_axi_ctrl;
	u32 rdmapres_wml;
};

struct rdma_reg {
	struct rdma_chn_reg rdma_chn[12];
	// x = (0x400 - 9 * 0x20 ) / 4
	u32 reserved_0x120_0x3fc[(0x400 - 12 * 0x20) / 4];
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

struct dma_chn_reg {
	u32 xdma_dfifo_wml;
	u32 xdma_dfifo_depth;
	u32 xdma_cfifo_depth;
	union {
		u32 val;
		struct {
			u32 p0: 6;
			u32 reserved0: 2;
			u32 p1: 6;
			u32 reserved1: 2;
			u32 sche: 6;
			u32 reserved2: 10;
		} bits;
	} xdma_ch_prio;
	union {
		u32 val;
		struct {
			u32 len: 3;
			u32 mode: 1;
			u32 reserved: 28;
		} bits;
	} xdma_burst;
	u32 xdma_axi_user;
	u32 xdma_axi_ctrl;
	u32 xdma_rdmapres_wml;
};

struct xdma_reg {
	struct dma_chn_reg dma_chn[9];
	u32 reserved_0x120_0x3fc[184];
	u32 xdma_ctrl;
	u32 reserved_0x404[63];
	u32 xdma_dfifo_full;
	u32 xdma_dfifo_empty;
	u32 xdma_cfifo_full;
	u32 xdma_cfifo_empty;
	u32 xdma_ch_idle;
	u32 reserved_0x514[3];
	u32 xdma_int_mask;
	u32 xdma_int_status;
	u32 reserved_0x528[6];
	u32 xdma_debug_ctrl;
	u32 xdma_debug_sta;
};
struct csc_coef {
	u32 val1 : 14;
	u32 reserved1: 2;
	u32 val2 : 14;
	u32 reserved2: 2;
};

struct ap_reg {
	u32 ap_pix_comp;
	u32 ap_frm_ctrl;
	u32 ap_frm_size;
	u32 ap_baddr_l;
	u32 ap_baddr_h;
	u32 reserved0x14[5];
	u32 ap_stride;
	u32 reserved_0x30[4];
};

struct wp_reg {
	u32 wp_ctrl;
	u32 wp_pix_comp;
	u32 wp_yuvdown_ctrl;
	u32 reserved_0xc[1];
	u32 wp_y_addrl;
	u32 wp_y_addr_h;
	u32 wp_u_addr_l;
	u32 wp_u_addr_h;
	u32 wp_v_addr_l;
	u32 wp_v_addr_h;
	u32 wp_y_stride;
	u32 wp_u_stride;
	u32 wp_v_stride;
	u32 reserved_0x34[3];
	u32 wp_csc_ctrl;
	u32 csc_coef[8];
	u32 wp_r2t_status;
	u32 reserved_0x68[2];
};


struct mlc_sf_reg {
	union {
		u32 val;
		struct {
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
	struct mlc_sf_reg mlc_sf[5];
	u32 reserved_0xc0_0x1fc[68];
	union {
		u32 val;
		struct {
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
	} mlc_path_ctrl[6];
	u32 reserved_0x214[2];
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

struct fbdc_reg {
	union {
		u32 val;
		struct {
			u32 fbdc_en: 1;
			u32 fbdc_mode_v3_1_en: 1;
			u32 fbdc_hdr_bypass: 1;
			u32 fbdc_inva_sw_en: 1;
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

static const uint32_t csc_param[5][15] = {
	{/*bt709 YCbCr to RGB(16-235)*/
		0x0800, 0x0000, 0x0C99, 0x0800,
		0x3E81, 0x3C42, 0x0800, 0x0ED8,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0200, 0x0200
	},
	{/*YCbCr to RGB*/
		0x094F, 0x0000, 0x0CC4, 0x094F,
		0x3CE0, 0x397F, 0x094F, 0x1024,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0040, 0x0200, 0x0200
	},
	{/*YUV to RGB*/
		0x0800, 0x0000, 0x091E, 0x0800,
		0x3CD8, 0x3B5B, 0x0800, 0x1041,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000
	},
	{/*RGB to YCbCr*/
		0x0264, 0x04B2, 0x00E9, 0x3EA0,
		0x3D4A, 0x0416, 0x0416, 0x3C94,
		0x3F57, 0x0000, 0x0200, 0x0200,
		0x0000, 0x0000, 0x0000
	},
	{/*RGB to YUV*/
		0x0000
	}
};

static unsigned int reg_value(unsigned int val,
	unsigned int src, unsigned int shift, unsigned int mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

static int get_rdma_ch_and_count(struct sdrv_g2d *dev, struct g2d_input *ins, char *ch)
{
	int ret, i, index, count = 0;
	struct g2d_layer * layer;

	for (i = 0; i < ins->layer_num; i++) {
		layer =  &ins->layer[i];
		if (!layer->enable)
			continue;

		index = layer->index;

		if (index >= dev->num_pipe) {
			dev->ops->reset(dev);
			G2D_ERROR("layer index is invalid: %d", index);
			return -1;
		}

		if (index > G2D_GPIPE2) {
			ch[index + G2D_GPIPE2 * 3] = 1;
			count++;
			continue;
		}

		ret = sdrv_pix_comp(layer->format, &layer->comp);
		if (ret) {
			return -1;
		}
		G2D_DBG("buf_fmt = %d, uvmode = %d\n", layer->comp.buf_fmt, layer->comp.uvmode);
		if (layer->comp.buf_fmt == FMT_SEMI_PLANAR) {
			ch[index * 3] = 1;
			ch[index * 3 + 1] = 1;
			count += 2;
		} else if (layer->comp.buf_fmt == FMT_PLANAR) {
			if (layer->comp.uvmode != 0) {
				ch[index * 3] = 2;
				count += 4;
			} else {
				ch[index * 3] = 1;
				count += 3;
			}
			ch[index * 3 + 1] = 1;
			ch[index * 3 + 2] = 1;
		} else {
			ch[index * 3] = 1;
			count++;
		}
	}

	//apipe & cmdfile
	if (dev->write_mode == G2D_CMD_WRITE || ins->bg_layer.en) {
		ch[G2D_RDMA_APIPE_CH] = 1;
	}

	G2D_DBG("get rdma ch count[%d]\n", count);
	return count;

}

static int get_wdma_ch_and_count(struct g2d_output_cfg *output, char *ch)
{
	int ret, count = 0;
	struct pix_g2dcomp comp = {0};

	ret = sdrv_wpipe_pix_comp(output->fmt, &comp);
	if(ret < 0) {
		return ret;
	}
	G2D_DBG("buf_fmt = %d, uvmode = %d\n", comp.buf_fmt, comp.uvmode);
	if (comp.buf_fmt == FMT_SEMI_PLANAR) {
		ch[0] = 1;
		ch[1] = 1;
		count += 2;
	} else if (comp.buf_fmt == FMT_PLANAR) {
		if (comp.uvmode != 0) {
			ch[0] = 2;
			count += 4;
		} else {
			ch[0] = 1;
			count += 3;
		}
		ch[1] = 1;
		ch[2] = 1;
	} else {
		ch[0] = 1;
		count++;
	}

	return count;
}

static int g2d_irq_handler(struct sdrv_g2d *gd)
{
	g2d_reg_t reg = (g2d_reg_t )gd->iomem_regs;
	unsigned int reg_val, val = 0;

	reg_val = reg->int_status;
	// clear irq
	reg->int_status = reg_val;
	if (reg_val & G2DLITE_INT_MASK_FRM_DONE_MASK) {
		val |= G2D_INT_MASK_FRM_DONE;
	}

	if (reg_val & G2DLITE_INT_MASK_TASK_DONE_MASK) {
		val |= G2D_INT_MASK_TASK_DONE;
	}

	return val;
}

static int g2d_reset(struct sdrv_g2d *gd)
{
	g2d_reg_t reg = (g2d_reg_t )gd->iomem_regs;

	reg->g2d_ctrl |= BIT(31);
	udelay(5);
	reg->g2d_ctrl &= ~BIT(31);

	if (gd->write_mode == G2D_CMD_WRITE) {
		memset(gd->cmd_info[0].arg, 0, G2D_CMDFILE_MAX_MEM * sizeof(unsigned int));
	}

	G2D_DBG("g2d reset done \n");
	return 0;
}

static int g2d_enable(struct sdrv_g2d *gd, int enable)
{
	g2d_reg_t reg = (g2d_reg_t )gd->iomem_regs;
	cmdfile_info *cmd = (cmdfile_info *)&gd->cmd_info[0];

	if (gd->write_mode == G2D_CMD_WRITE) {
		reg->cmdfile_addr_l = get_l_addr(gd->dma_buf);
		reg->cmdfile_addr_h = get_h_addr(gd->dma_buf);
		reg->cmdfile_len =	cmd[0].len / 2 - 1;
		reg->cmdfile_cfg |= BIT(3);
	}
	//g2d_dump_registers(gd);
	reg->g2d_ctrl |= BIT(0);

	return 0;
}

static int g2d_rdma_set(struct sdrv_g2d *gd, struct g2d_input *ins, bool fastcopy)
{
	int i, count = 0;
	char ch[G2D_RCHN_COUNT] = {0};
	int burst_len = 0, wml_unit = 0, d_depth_unit = 0, c_depth_unit = 0;
	REGISTER rdma_reg *rdma = (REGISTER rdma_reg *)(gd->iomem_regs + RDMA_JUMP);

	if (fastcopy) {
		count = 1;
		ch[G2D_RDMA_APIPE_CH] = 1;
		burst_len = 6;
		goto set_rdma_regs;
	}

	if (!ins) {
		G2D_ERROR("ins is NULL\n");
		return -1;
	}
	//rdma
	count = get_rdma_ch_and_count(gd, ins, ch);
	if (count <= 0) {
		G2D_ERROR("get rdma ch count[%d] failed\n", count);
		return -1;
	}
	//2^burst_len = DFIFO/(4*CFIFO)
	switch (count) {
	case 1:
		burst_len = 6;
		break;
	case 2:
	case 3:
		burst_len = 5;
		break;
	default:
		burst_len = 4;
		break;
	}

set_rdma_regs:
	wml_unit = G2D_RDMA_WML_TOTAL / count;
	d_depth_unit = G2D_RDMA_DFIFO_TOTAL_DEPTH / count;
	c_depth_unit = G2D_RDMA_CFIFO_TOTAL_DEPTH / count;
	for (i = 0; i < G2D_RCHN_COUNT; i++) {
		rdma->rdma_chn[i].rdma_dfifo_wml = ch[i] * wml_unit;
		rdma->rdma_chn[i].rdma_dfifo_depth = ch[i] * d_depth_unit;
		rdma->rdma_chn[i].rdma_cfifo_depth = ch[i] * c_depth_unit;
		rdma->rdma_chn[i].burst.bits.len = burst_len;
		G2D_DBG("rdma ch %d wml = %d, dfifo = %d, cfifo = %d, burst = %d\n",
			i, rdma->rdma_chn[i].rdma_dfifo_wml,
			rdma->rdma_chn[i].rdma_dfifo_depth,
			rdma->rdma_chn[i].rdma_cfifo_depth,
			rdma->rdma_chn[i].burst.bits.len);
	}
	rdma->rdma_ctrl = BIT(1) | BIT(0);

	return 0;
}

static int g2d_wdma_set(struct sdrv_g2d *gd, struct g2d_output_cfg *output, bool fastcopy)
{
	int i, count = 0;
	char ch[WCHN_COUNT] = {0};
	int burst_len = 0, wml_unit = 0, d_depth_unit = 0, c_depth_unit = 0;
	REGISTER xdma_reg *wdma = (REGISTER xdma_reg *)(gd->iomem_regs + WDMA_JUMP);

	if (fastcopy) {
		count = 1;
		ch[0] = 1;
		burst_len = 5;
		goto set_wdma_regs;
	}

	if (!output) {
		G2D_ERROR("g2d_output_cfg is NULL\n");
		return -1;
	}

	//wdma
	count = get_wdma_ch_and_count(output, ch);
	switch (count) {
	case 1:
		burst_len = 5;
		break;
	case 2:
	case 3:
		burst_len = 4;
		break;
	default:
		burst_len = 3;
		break;
	}

set_wdma_regs:
	wml_unit = G2D_WDMA_WML_TOTAL / count;
	d_depth_unit = G2D_WDMA_DFIFO_TOTAL_DEPTH / count;
	c_depth_unit = G2D_WDMA_CFIFO_TOTAL_DEPTH / count;
	for (i = 0; i < WCHN_COUNT; i++) {
		wdma->dma_chn[i].xdma_dfifo_wml = ch[i] * wml_unit;
		wdma->dma_chn[i].xdma_dfifo_depth =	ch[i] * d_depth_unit;
		wdma->dma_chn[i].xdma_cfifo_depth =	ch[i] * c_depth_unit;
		wdma->dma_chn[i].xdma_burst.bits.len = burst_len;
		G2D_DBG("wdma ch %d wml = %d, dfifo = %d, cfifo = %d, burst = %d\n",
			i, wdma->dma_chn[i].xdma_dfifo_wml,
			wdma->dma_chn[i].xdma_dfifo_depth,
			wdma->dma_chn[i].xdma_cfifo_depth,
			wdma->dma_chn[i].xdma_burst.bits.len);
	}
	wdma->xdma_ctrl = BIT(1) | BIT(0);

	return 0;
}

static int g2d_rwdma_set(struct sdrv_g2d *gd, struct g2d_input *ins)
{
	int ret;

	ret = g2d_rdma_set(gd, ins, false);
	if (ret) {
		G2D_ERROR("g2d rdma set failed\n");
		return -1;
	}

	ret = g2d_wdma_set(gd, &ins->output, false);
	if (ret) {
		G2D_ERROR("g2d wdma set failed\n");
		return -1;
	}

	return 0;
}

static inline unsigned int g2d_read(unsigned long base, unsigned int reg)
{
	return readl((void *)(base + reg));
}

static const uint32_t G2D_HSCALER[3][2] = {
	{0x2600, 0x29ff},
	{0x3600, 0x39ff},
	{0x4600, 0x49ff}
};

static const uint32_t G2D_VSCALER[3][2] = {
	{0x2a00, 0x2dff},
	{0x3a00, 0x3dff},
	{0x4c00, 0x4dff}
};

static const int hscaler_coef[33][5] = {
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

static const int vscaler_coef[33][4] = {
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

#if G2D_DUMP_HVSCALER
static void g2d_dump_hvscaler(struct sdrv_g2d *gd)
{
	int i;
	unsigned long base = (unsigned long)gd->iomem_regs;
	unsigned int val;

	udelay(100);

#define GP_HS_CTRL                      0x0300

	val = g2d_read(base, (GP_HS_CTRL + GP0_JUMP));
	val |= 0b1 << 4;
	writel(val, (void *)(base + (GP_HS_CTRL + GP0_JUMP)));
	val = g2d_read(base, (GP_HS_CTRL + GP1_JUMP));
	val |= 0b1 << 4;
	writel(val, (void *)(base + (GP_HS_CTRL + GP1_JUMP)));
	val = g2d_read(base, (GP_HS_CTRL + GP2_JUMP));
	val |= 0b1 << 4;
	writel(val, (void *)(base + (GP_HS_CTRL + GP2_JUMP)));

	udelay(100);

	for (i = 0; i < 33; i++) {
		G2D_INFO(">>> hscaler coef reg = 0x%lx, val = 0x%x \n",
		 (base + G2D_HSCALER[0][0] + i * 4), g2d_read(base, (G2D_HSCALER[0][0] + i * 4)));
		G2D_INFO(">>> hscaler coef reg = 0x%lx, val = 0x%x \n",
		 (base + G2D_HSCALER[0][0] + 0x100 + i * 4), g2d_read(base, (G2D_HSCALER[0][0] + 0x100 + i * 4)));
		G2D_INFO(">>> hscaler coef reg = 0x%lx, val = 0x%x \n",
		 (base + G2D_HSCALER[1][0] + i * 4), g2d_read(base, (G2D_HSCALER[1][0] + i * 4)));
		G2D_INFO(">>> hscaler coef reg = 0x%lx, val = 0x%x \n",
		 (base + G2D_HSCALER[1][0] + 0x100 + i * 4), g2d_read(base, (G2D_HSCALER[1][0] + 0x100 + i * 4)));
		G2D_INFO(">>> hscaler coef reg = 0x%lx, val = 0x%x \n",
		 (base + G2D_HSCALER[2][0] + i * 4), g2d_read(base, (G2D_HSCALER[2][0] + i * 4)));
		G2D_INFO(">>> hscaler coef reg = 0x%lx, val = 0x%x \n",
		 (base + G2D_HSCALER[2][0] + 0x100 + i * 4), g2d_read(base, (G2D_HSCALER[2][0] + 0x100 + i * 4)));
	}

	for (i = 0; i < 33; i++) {
		G2D_INFO(">>> vscaler coef reg = 0x%lx, val = 0x%x \n",
		(base + G2D_VSCALER[0][0] + i * 4), g2d_read(base, (G2D_VSCALER[0][0] + i * 4)));
		G2D_INFO(">>> vscaler coef reg = 0x%lx, val = 0x%x \n",
		(base + G2D_VSCALER[1][0] + i * 4), g2d_read(base, (G2D_VSCALER[1][0] + i * 4)));
		G2D_INFO(">>> vscaler coef reg = 0x%lx, val = 0x%x \n",
		(base + G2D_VSCALER[2][0] + i * 4), g2d_read(base, (G2D_VSCALER[2][0] + i * 4)));
	}

	val = g2d_read(base, (GP_HS_CTRL + GP0_JUMP));
	val &= ~(0b1 << 4);
	writel(val, (void *)(base + (GP_HS_CTRL + GP0_JUMP)));
	val = g2d_read(base, (GP_HS_CTRL + GP1_JUMP));
	val &= ~(0b1 << 4);
	writel(val, (void *)(base + (GP_HS_CTRL + GP1_JUMP)));
	val = g2d_read(base, (GP_HS_CTRL + GP2_JUMP));
	val &= ~(0b1 << 4);
	writel(val, (void *)(base + (GP_HS_CTRL + GP2_JUMP)));
}
#endif

static void g2d_set_hscaler_coef(struct sdrv_g2d *gd, const int (*hcoef)[5])
{
	int i;
	unsigned int head, tail;
	unsigned long base = (unsigned long)gd->iomem_regs;

	for (i = 0; i < 33; i++) {

		head = ((hcoef[i][0] & 0x7ff) << 12)
				| ((hcoef[i][1] & 0x7ff) << 1)
				| ((hcoef[i][2] & 0x400) >> 10);
		tail = ((hcoef[i][2] & 0x3ff) << 22)
				| ((hcoef[i][3] & 0x7ff) << 11)
				| (hcoef[i][4] & 0x7ff);

		writel(head, (void *)(base + G2D_HSCALER[0][0] + i * 4));
		writel(tail, (void *)(base + G2D_HSCALER[0][0] + 0x100 + i * 4));
		writel(head, (void *)(base + G2D_HSCALER[1][0] + i * 4));
		writel(tail, (void *)(base + G2D_HSCALER[1][0] + 0x100 + i * 4));
		writel(head, (void *)(base + G2D_HSCALER[2][0] + i * 4));
		writel(tail, (void *)(base + G2D_HSCALER[2][0] + 0x100 + i * 4));
	}

}

static void g2d_set_vscaler_coef(struct sdrv_g2d *gd, const int (*vcoef)[4])
{
	unsigned long base = (unsigned long)gd->iomem_regs;
	unsigned int value;
	int i;

	for (i = 0; i < 33; i++) {
		value = ((vcoef[i][3] & 0xff) << 24)
				| ((vcoef[i][2] & 0xff) << 16)
				| ((vcoef[i][1] & 0xff) << 8)
				| ((vcoef[i][0] & 0xff));

		writel(value, (void *)(base + G2D_VSCALER[0][0] + i * 4));
		writel(value, (void *)(base + G2D_VSCALER[1][0] + i * 4));
		writel(value, (void *)(base + G2D_VSCALER[2][0] + i * 4));
	}
}

static void g2d_init_set_scaler_coef(struct sdrv_g2d *gd)
{
	g2d_set_hscaler_coef(gd, hscaler_coef);
	g2d_set_vscaler_coef(gd, vscaler_coef);
#if G2D_DUMP_HVSCALER
	g2d_dump_hvscaler(gd);
#endif
}

static void g2d_rdma_init(struct sdrv_g2d *gd)
{
	int i;
	REGISTER rdma_reg *rdma = (REGISTER rdma_reg *)(gd->iomem_regs + RDMA_JUMP);

	#define RDMA_CHN_COUNT 12
	const unsigned short wml_cfg[12]	   = {0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10};
	const unsigned short d_depth_cfg[12]   = {0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2};
	const unsigned short c_depth_cfg[12]   = {0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2};
	const unsigned char sche_cfg[12]		= {0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x2, 0x2, 0x2};
	const unsigned char p1_cfg[12]			= {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,0x30, 0x30, 0x30};
	const unsigned char p0_cfg[12]			= {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,0x10, 0x10, 0x10};
	const unsigned char burst_mode_cfg[12]			= {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
	const unsigned char burst_len_cfg[12]			= {0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4};

	for (i = 0; i < RDMA_CHN_COUNT; i++) {
		rdma->rdma_chn[i].rdma_dfifo_wml = wml_cfg[i];

		rdma->rdma_chn[i].rdma_dfifo_depth = d_depth_cfg[i];
		rdma->rdma_chn[i].rdma_cfifo_depth = c_depth_cfg[i];

		rdma->rdma_chn[i].ch_prio.val = 0;
		rdma->rdma_chn[i].ch_prio.bits.sche = sche_cfg[i];
		rdma->rdma_chn[i].ch_prio.bits.p1 = p1_cfg[i];
		rdma->rdma_chn[i].ch_prio.bits.p0 = p0_cfg[i];

		rdma->rdma_chn[i].burst.val = 0;
		rdma->rdma_chn[i].burst.bits.mode = burst_mode_cfg[i];
		rdma->rdma_chn[i].burst.bits.len = burst_len_cfg[i];
	}

	rdma->rdma_int_mask = 0;
	rdma->rdma_ctrl = BIT(0) | BIT(1);
}

static void g2d_mlc_init(struct sdrv_g2d *gd)
{
	int i;
	REGISTER mlc_reg *reg = (struct mlc_reg *)(gd->iomem_regs + MLC_JUMP);

	const unsigned char prot_val[]	= {0x7, 0x7, 0x7, 0x7, 0x7, 0x7};
	const unsigned char alpha_bld[] = {0xF, 0xF, 0xF, 0xF, 0xF, 0xF};
	const unsigned char pd_out[]	= {0x7, 0x7, 0x7, 0x7, 0x7, 0x7};
	const unsigned char pd_des[]	= {0x7, 0x7, 0x7, 0x7, 0x7, 0x7};
	const unsigned char pd_src[]	= {0x7, 0x7, 0x7, 0x7, 0x7, 0x7};
	const unsigned char layer_out[] = {0xF, 0xF, 0xF, 0xF, 0xF, 0xF};

	/*LAYER related*/
	for (i = 0; i < G2D_MLC_LAYER_COUNT; i++) {
		REGISTER mlc_sf_reg *sf = &reg->mlc_sf[i];

		sf->mlc_sf_ctrl.bits.prot_val = prot_val[i];
		sf->mlc_sf_ctrl.bits.g_alpha_en = 1;

		sf->mlc_sf_g_alpha = 0xFF;
	}

	/*PATH related*/
	for (i = 0; i < G2D_MLC_PATH_COUNT; i++) {
		reg->mlc_path_ctrl[i].bits.alpha_bld_idx = alpha_bld[i];
		reg->mlc_path_ctrl[i].bits.pd_out_idx = pd_out[i];
		reg->mlc_path_ctrl[i].bits.pd_des_idx = pd_des[i];
		reg->mlc_path_ctrl[i].bits.pd_src_idx = pd_src[i];
		reg->mlc_path_ctrl[i].bits.layer_out_idx = layer_out[i];
	}
	reg->mlc_clk_ratio = 0xFFFF;
	reg->mlc_int_mask = 0;
}

static void g2d_wdma_init(struct sdrv_g2d *gd)
{
	const unsigned short wml_cfg[]		 = {0x20, 0x10, 0x10};
	const unsigned short d_depth_cfg[]	 = {0x04, 0x02, 0x02};
	const unsigned short c_depth_cfg[]	 = {0x04, 0x02, 0x02};
	const unsigned char sche_cfg[]		 = {0x04, 0x02, 0x02};
	const unsigned char p1_cfg[]		 = {0x30, 0x30, 0x30};
	const unsigned char p0_cfg[]		 = {0x10, 0x10, 0x10};
	const unsigned char burst_mode_cfg[] = {0x00, 0x00, 0x00};
	const unsigned char burst_len_cfg[]	 = {0x06, 0x06, 0x06};
	int i;
	REGISTER xdma_reg *wdma = (REGISTER xdma_reg *)(gd->iomem_regs + WDMA_JUMP);

	for (i = 0; i < WCHN_COUNT; i++) {
		wdma->dma_chn[i].xdma_dfifo_wml = wml_cfg[i];
		wdma->dma_chn[i].xdma_dfifo_depth = d_depth_cfg[i];
		wdma->dma_chn[i].xdma_cfifo_depth = c_depth_cfg[i];
		wdma->dma_chn[i].xdma_ch_prio.bits.sche = sche_cfg[i];
		wdma->dma_chn[i].xdma_ch_prio.bits.p1 = p1_cfg[i];
		wdma->dma_chn[i].xdma_ch_prio.bits.p0 = p0_cfg[i];
		wdma->dma_chn[i].xdma_burst.bits.mode = burst_mode_cfg[i];
		wdma->dma_chn[i].xdma_burst.bits.len = burst_len_cfg[i];
	}

	wdma->xdma_int_mask = 0;
	wdma->xdma_ctrl = BIT(0) | BIT(1);
}

static void g2d_wpipe_init(struct sdrv_g2d *gd)
{
	REGISTER wp_reg *wp = (REGISTER wp_reg *)(gd->iomem_regs + WP_JUMP);
	int i, n;

	/*YUVDOWN Bypass*/
	wp->wp_yuvdown_ctrl = BIT(9);

	/*CSC Bypass*/
	wp->wp_csc_ctrl = BIT(0);
	for (i = 0, n = 0; i < 7; i++) {
		wp->csc_coef[i] = csc_param[3][n] << 0 | csc_param[3][n + 1] << 16;
		n += 2;
	}

	wp->csc_coef[7] = csc_param[3][14] << 0 ;
}

static int g2d_init(struct sdrv_g2d *gd)
{
	REGISTER g2d_reg *reg = (g2d_reg_t )gd->iomem_regs;

	gd->write_mode = G2D_CMD_WRITE;//G2D_CPU_WRITE;

	g2d_init_set_scaler_coef(gd);

	g2d_reset(gd);
	g2d_rdma_init(gd);
	g2d_mlc_init(gd);
	g2d_wdma_init(gd);
	g2d_wpipe_init(gd);
	//irq mask
	reg->int_mask = 0x3F;

	if (!gd->du_inited) {
		gd->du_inited = true;
		g2d_choose_pipe(gd, G2D_GPIPE0, TYPE_GP_HIGH, GP0_JUMP);
		g2d_choose_pipe(gd, G2D_GPIPE1, TYPE_GP_HIGH, GP1_JUMP);
		g2d_choose_pipe(gd, G2D_GPIPE2, TYPE_GP_MID, GP2_JUMP);
		g2d_choose_pipe(gd, G2D_SPIPE0, TYPE_SPIPE, SP0_JUMP);
		g2d_choose_pipe(gd, G2D_SPIPE1, TYPE_SPIPE, SP1_JUMP);
		G2D_DBG("add %d pipes for g2d\n", gd->num_pipe);
	}

	return 0;
}

static void g2d_mlc_clear(struct sdrv_g2d *gd, int cmdid)
{
	unsigned int val, i;
	cmdfile_info *cmd = (cmdfile_info *)&gd->cmd_info[0];

	/*disable all layers*/
	for (i = 0; i < G2D_MLC_LAYER_COUNT; i++) {
		val = 0x00000704;
		val = reg_value(0, val, MLC_SF_EN_SHIFT, MLC_SF_EN_MASK);
		g2d_cwrite(cmd, cmdid, MLC_SF_CTRL_(i), val);
	}

	/*disable all paths*/
	for (i = 0; i < G2D_MLC_PATH_COUNT; i++) {
		val = 0xF777F;
		g2d_cwrite(cmd, cmdid, MLC_PATH_CTRL_(i), val);
	}
}

static int g2d_mlc_set(struct sdrv_g2d *gd, int cmdid, struct g2d_input *input)
{
	unsigned int i, val;
	cmdfile_info *cmd = (cmdfile_info *)&gd->cmd_info[0];
	unsigned int sf_val[G2D_MLC_LAYER_COUNT], path_val[G2D_MLC_PATH_COUNT];
	unsigned char layer_index, path_index;
	struct g2d_layer *layer;

	g2d_mlc_clear(gd, cmdid);

	for (i = 0; i < G2D_MLC_LAYER_COUNT; i++) {
		sf_val[i] = 0x00000704;
	}

	for (i = 0; i < G2D_MLC_PATH_COUNT; i++) {
		path_val[i] = 0xF777F;
	}

	for (i = 0; i < G2D_MLC_LAYER_COUNT; i++) {
		layer = &input->layer[i];

		if (!layer->enable)
			continue;

		if (layer->index >= G2D_MLC_LAYER_COUNT) {
			return -1;
		}

		layer_index = layer->index;
		path_index = layer->index + 1;

		/*POS*/
		g2d_cwrite(cmd, cmdid, MLC_SF_H_H_SPOS_(layer_index), layer->dst_x);
		g2d_cwrite(cmd, cmdid, MLC_SF_V_V_SPOS_(layer_index), layer->dst_y);

		/*dst size*/
		val = reg_value(layer->dst_h - 1, 0, MLC_SF_SIZE_V_SHIFT, MLC_SF_SIZE_V_MASK);
		val = reg_value(layer->dst_w - 1, val, MLC_SF_SIZE_H_SHIFT, MLC_SF_SIZE_H_MASK);
		g2d_cwrite(cmd, cmdid, MLC_SF_SF_SIZE_(layer_index), val);

		/*PATH with zorder*/
		path_val[path_index] = reg_value(layer->zpos, path_val[path_index], LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
		g2d_cwrite(cmd, cmdid, MLC_PATH_CTRL_((path_index)), path_val[path_index]);

		path_val[layer->zpos] = reg_value(path_index, path_val[layer->zpos], ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
		g2d_cwrite(cmd, cmdid, MLC_PATH_CTRL_(layer->zpos), path_val[layer->zpos]);


		/*Blend mode*/
		switch (layer->blend_mode) {
			case BLEND_PIXEL_NONE:
			default:
				sf_val[layer_index] = reg_value(1, sf_val[layer_index], MLC_SF_G_ALPHA_EN_SHIFT, MLC_SF_G_ALPHA_EN_MASK);
				g2d_cwrite(cmd, cmdid, MLC_SF_G_G_ALPHA_(layer_index), layer->alpha);
				break;
			case BLEND_PIXEL_PREMULTI:
				sf_val[layer_index] = reg_value(0, sf_val[layer_index], MLC_SF_G_ALPHA_EN_SHIFT, MLC_SF_G_ALPHA_EN_MASK);

				path_val[layer->zpos] = reg_value(1, path_val[layer->zpos], PMA_EN_SHIFT, PMA_EN_MASK);
				g2d_cwrite(cmd, cmdid, MLC_PATH_CTRL_(layer->zpos), path_val[layer->zpos]);
				break;
			case BLEND_PIXEL_COVERAGE:
				sf_val[layer_index] = reg_value(0, sf_val[layer_index], MLC_SF_G_ALPHA_EN_SHIFT, MLC_SF_G_ALPHA_EN_MASK);
				break;
		}

		/*enable layer*/
		sf_val[layer_index] = reg_value(1, sf_val[layer_index], MLC_SF_EN_SHIFT, MLC_SF_EN_MASK);
		g2d_cwrite(cmd, cmdid, MLC_SF_CTRL_(layer_index), sf_val[layer_index]);
	}

	return 0;
}

static int g2d_wpipe_pixcomp_set(struct sdrv_g2d *gd, struct g2d_output_cfg *output)
{
	int ret = 0;
	unsigned int val;
	unsigned int reforce;
	struct pix_g2dcomp comp = {0};
	cmdfile_info *cmd = (cmdfile_info *)gd->cmd_info;
	REGISTER wp_reg *wp = (REGISTER wp_reg *)(gd->iomem_regs + WP_JUMP);

	ret = sdrv_wpipe_pix_comp(output->fmt, &comp);
	if(ret < 0) {
		return ret;
	}

	val = reg_value(comp.bpv, 0, BPV_SHIFT, BPV_MASK);
	val = reg_value(comp.bpu, val, BPU_SHIFT, BPU_MASK);
	val = reg_value(comp.bpy, val, BPY_SHIFT, BPY_MASK);
	val = reg_value(comp.bpa, val, BPA_SHIFT, BPA_MASK);
	g2d_cwrite(cmd, 0, WP_PIX_COMP_COMP, val);

	reforce = (output->rotation == ROTATION_TYPE_NONE) ? 1 : 0;
	val = wp->wp_ctrl;
	val = reg_value(reforce, val, WP_CTRL_REFORGE_BYPS_SHIFT, WP_CTRL_REFORGE_BYPS_MASK);
	val = reg_value(comp.uvmode, val, WP_CTRL_UV_MODE_SHIFT, WP_CTRL_UV_MODE_MASK);
	val = reg_value(comp.buf_fmt, val, WP_CTRL_STR_FMT_SHIFT, WP_CTRL_STR_FMT_MASK);
	val = reg_value(output->rotation, val, WP_CTRL_ROT_EN_SHIFT,
			WP_CTRL_VFLIP_MASK | WP_CTRL_HFLIP_MASK | WP_CTRL_ROT_EN_MASK);
	val = reg_value(comp.endin, val, WP_CTRL_PACK_ENDIAN_CTRL_SHIFT,
			WP_CTRL_PACK_ENDIAN_CTRL_MASK);

	g2d_cwrite(cmd, 0, WP_CTRL_CTRL, val);

	return 0;
}

static int g2d_wp_csc_set(struct sdrv_g2d *gd, struct g2d_output_cfg *output, unsigned char flag)
{
	unsigned char is_yuv;
	unsigned char bypass;
	unsigned int val;
	REGISTER wp_reg *wp = (REGISTER wp_reg *)(gd->iomem_regs + WP_JUMP);
	cmdfile_info *cmd = (cmdfile_info *)&gd->cmd_info[0];

	is_yuv = g2d_format_is_yuv(output->fmt);
	bypass = g2d_format_wpipe_bypass(output->fmt);
	G2D_DBG("bypass = %d \n", bypass);

	if (flag & G2D_FUN_BLEND) {
		val = wp->wp_csc_ctrl;
		val = reg_value(!is_yuv, val, WP_CSC_CTRL_BYPASS_SHIFT, WP_CSC_CTRL_BYPASS_MASK);
		g2d_cwrite(cmd, 0, WP_CSC_CTRL_CTRL, val);
	}

	val = wp->wp_yuvdown_ctrl;
	val = reg_value(bypass & 0x1 , val, WP_YUVDOWN_CTRL_V_BYPASS_SHIFT, WP_YUVDOWN_CTRL_V_BYPASS_MASK); //TODO:need to check.
	val = reg_value(bypass >> 1, val, WP_YUVDOWN_CTRL_BYPASS_SHIFT, WP_YUVDOWN_CTRL_BYPASS_MASK);
	g2d_cwrite(cmd, 0, WP_YUVDOWN_CTRL_CTRL, val);

	return 0;
}

static int g2d_wpipe_set(struct sdrv_g2d *gd,
			int cmdid, struct g2d_output_cfg *output)
{
	int ret;
	unsigned int val;
	unsigned char nplanes;
	unsigned long input_addr;
	cmdfile_info *cmd = (cmdfile_info *)&gd->cmd_info[0];
	if (!cmdid) {
		ret = g2d_wpipe_pixcomp_set(gd, output);
		if(ret < 0) {
			G2D_ERROR("wqpipe failed\n");
			return ret;
		}
		g2d_wp_csc_set(gd, output, G2D_FUN_BLEND);
	}
	if ((output->height <= 0) || (output->width <= 0)) {
		G2D_ERROR("wpipe set output->height = %d, output->width = %d \n",
			output->height, output->width);
		return -1;
	}

	nplanes = output->nplanes;
	val = reg_value(output->height - 1, 0, G2DLITE_SIZE_V_SHIFT, G2DLITE_SIZE_V_MASK);

	val = reg_value(output->width - 1, val, G2DLITE_SIZE_H_SHIFT, G2DLITE_SIZE_H_MASK);
	g2d_cwrite(cmd, cmdid, G2DLITE_SIZE, val);
	input_addr = get_real_addr(output->addr[0]);
	g2d_cwrite(cmd, cmdid, WP_Y_BADDR_L_L, get_l_addr(input_addr));
	g2d_cwrite(cmd, cmdid, WP_Y_BADDR_H_H, get_h_addr(input_addr));
	g2d_cwrite(cmd, cmdid, WP_Y_STRIDE_STRIDE, output->stride[0] - 1);

	if (nplanes > 1) {
		input_addr = get_real_addr(output->addr[1]);
		g2d_cwrite(cmd, cmdid, WP_U_BADDR_L_L, get_l_addr(input_addr));
		g2d_cwrite(cmd, cmdid, WP_U_BADDR_H_H, get_h_addr(input_addr));
		g2d_cwrite(cmd, cmdid, WP_U_STRIDE_STRIDE, output->stride[1] - 1);
	}

	if (nplanes > 2) {
		input_addr = get_real_addr(output->addr[2]);
		g2d_cwrite(cmd, cmdid, WP_V_BADDR_L_L, get_l_addr(input_addr));
		g2d_cwrite(cmd, cmdid, WP_V_BADDR_H_H, get_h_addr(input_addr));
		g2d_cwrite(cmd, cmdid, WP_V_STRIDE_STRIDE, output->stride[2] - 1);
	}

	return 0;
}

/**
 * check_is_stroke()
 * @brief
 * Purpose: Determine whether stroke processing is needed
 * Hardware ip limited size module:
 * GPIPE : RE、YUV_UP、Vscaler
 * WPIPE : YUV_DOWN、GPipeR2T(/Reforge)
 *
 * Circumstances dealt with by strokes:
 * 1. output rotation by wpipe
 * 2. GPIPE0/1 processes the input YUV420 format
 *
 * Note:
 * 1. Horizontal scaling cannot be handled with a stroke
 * 2. Horizontal and vertical scaling at the same time can only be used with GPIPE2
 */
static int g2d_check_is_stroke(struct g2d_input *ins)
{
	int i;
	int stroke_action = 0;

	G2D_DBG("storke action check \n");

	/*GPipeR2T(/Reforge) :output rotation by wpipe*/
	if (ins->output.rotation > 0) {
		if ((ins->output.fmt == DRM_FORMAT_YUYV) || (ins->output.fmt == DRM_FORMAT_UYVY)) {
			G2D_ERROR("The rotation function is not supported when the output format is YUYV or UYVY !\n");
			return -1;
		}
		stroke_action = 1;
		G2D_DBG("output.rotation = %d , stroke_action = %d \n", ins->output.rotation,
				stroke_action);
	}

	/*YUV_UP : GPIPE0/1 processes the input UV_YUV420 format*/
	for (i = 0; i < ins->layer_num; i++) {
		struct g2d_layer *layer =  &ins->layer[i];
		if (!layer->enable)
			continue;
		G2D_DBG("input layer index = [%d|%d] \n", ins->layer[i].index, ins->layer_num);

		sdrv_pix_comp(layer->format, &layer->comp);
		if (layer->comp.is_yuv) {
			if (layer->index > 2) {
				G2D_ERROR("layer 3/4 unsupport yuv fmt !\n");
				return -1;
			}

			/*The scaler of the UV_YUV420 format input layer can use GPIPE2*/
			if (layer->index != 2 ) {
				/*only yuv420 fmt limited by line buffer */
				if (layer->comp.uvmode == UV_YUV420 && layer->src_w > 256) {
					stroke_action = 1;
					G2D_DBG("input layer index = %d is UV_YUV420 and src_w[%d] > 256, stroke_action = %d \n",
						layer->index, layer->src_w, stroke_action);
				}
			}
		}
		/*HScaling cannot be handled with a stroke*/
		if ((stroke_action == 1) && (layer->src_w != layer->dst_w)) {
			G2D_ERROR("Scaling cannot be handled with a stroke !\n");
			G2D_ERROR("The cases of handled with a stroke are:\n");
			G2D_ERROR("1.output rotation by wpipe\n");
			G2D_ERROR("2.GPIPE0/1 processes the input UV_YUV420 format\n");
			return -1;
		}
		/*HScaling & Vscaler can only use GPIPE2*/
		if ((layer->src_h != layer->dst_h) && (layer->src_w != layer->dst_w) && (layer->index != 2)) {
			G2D_ERROR("Horizontal and vertical scaling at the same time can only be used with GPIPE2\n");
			return -1;
		}
	}

	G2D_DBG("stroke_action = %d \n", stroke_action);

	return stroke_action;
}

static int g2d_func_fill_rect_(void *g2d_regs, void *cmd_info, struct g2d_bg_cfg *bgcfg)
{
	u32 val, a_addr_l, a_addr_h, a_stride;
	cmdfile_info *cmd = (cmdfile_info *)cmd_info;
	REGISTER ap_reg *ap = (REGISTER ap_reg *)(g2d_regs + AP_JUMP);
	REGISTER mlc_reg *mlc_reg = (struct mlc_reg *)(g2d_regs + MLC_JUMP);
	unsigned long a_addr;
	u8 alpha_select = 0;

	g2d_cwrite(cmd, 0, MLC_PATH_CTRL_(0), 0x07770);

	if (bgcfg->aaddr) {
		a_addr = get_real_addr(bgcfg->aaddr);
		a_addr_l = get_l_addr(a_addr);
		a_addr_h = get_h_addr(a_addr);
		a_stride = bgcfg->astride;
		alpha_select = 1;
		if (!a_stride)
			a_stride = (bgcfg->width * bgcfg->bpa + 7) / 8 - 1;

		/*AP_PIX_COMP*/
		val = ap->ap_pix_comp;
		val = reg_value(bgcfg->bpa, val, AP_PIX_COMP_BPA_SHIFT, AP_PIX_COMP_BPA_MASK);
		g2d_cwrite(cmd, 0, AP_PIX_PIX_COMP, val);

		/*AP_FRM_SIZE*/
		val = ((bgcfg->height - 1) << 16) | ((bgcfg->width - 1) << 0);
		g2d_cwrite(cmd, 0, AP_FRM_FRM_SIZE, val);

		/*AP_FRM_OFFSET*/
		val = (bgcfg->y << 16) | (bgcfg->x << 0);
		g2d_cwrite(cmd, 0, AP_FRM_FRM_OFFSET, val);

		/*AP_BADDR_L*/
		g2d_cwrite(cmd, 0, AP_BADDR_L_L, a_addr_l);

		/*AP_BADDR_H*/
		g2d_cwrite(cmd, 0, AP_BADDR_H_H, a_addr_h);

		/*AP_STRIDE*/
		g2d_cwrite(cmd, 0, AP_AP_STRIDE, a_stride - 1);
	}

	/*MLC_BG_CTRL*/
	val = mlc_reg->mlc_bg_ctrl;
	val = reg_value(bgcfg->g_alpha, val, BG_A_SHIFT, BG_A_MASK);
	val = reg_value(alpha_select, val, BG_A_SEL_SHIFT, BG_A_SEL_MASK);
	val = reg_value(1, val, BG_EN_SHIFT, BG_EN_MASK);
	g2d_cwrite(cmd, 0, MLC_BG_CTRL, val);

	/*MLC_BG_COLOR*/
	g2d_cwrite(cmd, 0, MLC_BG_COLOR, bgcfg->color);

	return 0;
}

static int g2d_func_fill_rect(struct sdrv_g2d *gd,
	struct g2d_bg_cfg *bgcfg, struct g2d_output_cfg *output)
{
	int ret;

	ret = g2d_rdma_set(gd, NULL, true);
	if (ret) {
		G2D_ERROR("g2d rdma set failed\n");
		return -1;
	}

	ret = g2d_wdma_set(gd, output, false);
	if (ret) {
		G2D_ERROR("g2d wdma set failed\n");
		return -1;
	}

	g2d_mlc_clear(gd, 0);

	g2d_func_fill_rect_(gd->iomem_regs, &gd->cmd_info[0], bgcfg);

	return 0;
}

static void g2d_close_fastcopy(struct sdrv_g2d *gd)
{
	unsigned int val;
	REGISTER ap_reg *ap = (struct ap_reg *)(gd->iomem_regs +AP_JUMP);
	REGISTER mlc_reg *mlc_reg = (struct mlc_reg *)(gd->iomem_regs + MLC_JUMP);
	/*disable background in order to remove fastcopy
	 *bring affect when fastcopy using before other
	 *functions*/
	val = ap->ap_frm_ctrl;
	val = reg_value(0, val, AP_FRM_CTRL_FAST_CP_MODE_SHIFT, AP_FRM_CTRL_FAST_CP_MODE_MASK);
	ap->ap_frm_ctrl = val;

	val = mlc_reg->mlc_bg_ctrl;
	val = reg_value(0, val, BG_A_SEL_SHIFT, BG_A_SEL_MASK);
	val = reg_value(0, val, BG_EN_SHIFT, BG_EN_MASK);
	mlc_reg->mlc_bg_ctrl = val;
}

static int _g2d_fastcopy(void *g2d_regs, void *cmd_info, addr_t iaddr,
	uint32_t width, uint32_t height, addr_t oaddr, uint32_t stride)
{
	uint32_t val, i_addr_l, i_addr_h, o_addr_l, o_addr_h;
	unsigned long i_addr, o_addr;
	REGISTER ap_reg *ap = (struct ap_reg *)(g2d_regs +AP_JUMP);
	cmdfile_info *cmd = (cmdfile_info *)cmd_info;

	i_addr = get_real_addr(iaddr);
	o_addr = get_real_addr(oaddr);

	i_addr_l = get_l_addr(i_addr);
	i_addr_h = get_h_addr(i_addr);
	o_addr_l = get_l_addr(o_addr);
	o_addr_h = get_h_addr(o_addr);

	/*AP_FRM_CTRL*/
	val = ap->ap_frm_ctrl;
	val = reg_value(1, val, AP_FRM_CTRL_FAST_CP_MODE_SHIFT, AP_FRM_CTRL_FAST_CP_MODE_MASK);
	g2d_cwrite(cmd, 0, AP_FRM_FRM_CTRL, val);

	/*AP_FRM_SIZE*/
	val = reg_value(height - 1, 0, AP_FRM_SIZE_HEIGHT_SHIFT, AP_FRM_SIZE_HEIGHT_MASK);
	val = reg_value(width - 1, val, AP_FRM_SIZE_WIDTH_SHIFT, AP_FRM_SIZE_WIDTH_MASK);
	g2d_cwrite(cmd, 0, AP_FRM_FRM_SIZE, val);

	/*AP_BADDR_L*/
	val = reg_value(i_addr_l, 0, AP_BADDR_L_SHIFT, AP_BADDR_L_MASK);
	g2d_cwrite(cmd, 0, AP_BADDR_L_L, val);

	/*AP_BADDR_H*/
	val = reg_value(i_addr_h, 0, AP_BADDR_H_SHIFT, AP_BADDR_H_MASK);
	g2d_cwrite(cmd, 0, AP_BADDR_H_H, val);

	/*AP_STRIDE*/
	val = reg_value(stride - 1, 0, AP_STRIDE_SHIFT, AP_STRIDE_MASK);
	g2d_cwrite(cmd, 0, AP_AP_STRIDE, val);

	/*Config W-pipe*/
	val = reg_value(1, 0, WP_CTRL_REFORGE_BYPS_SHIFT, WP_CTRL_REFORGE_BYPS_MASK);
	g2d_cwrite(cmd, 0, WP_CTRL_CTRL, val); //clean wp_ctrl

	val = reg_value(height - 1, 0, G2DLITE_SIZE_V_SHIFT, G2DLITE_SIZE_V_MASK);
	val = reg_value(width - 1, val, G2DLITE_SIZE_H_SHIFT, G2DLITE_SIZE_H_MASK);
	g2d_cwrite(cmd, 0, G2DLITE_SIZE, val);

	g2d_cwrite(cmd, 0, WP_Y_BADDR_L_L, o_addr_l);
	g2d_cwrite(cmd, 0, WP_Y_BADDR_H_H, o_addr_h);
	g2d_cwrite(cmd, 0, WP_Y_STRIDE_STRIDE, stride - 1);

	return 0;
}

static int g2d_fastcopy(struct sdrv_g2d *gd, addr_t iaddr, u32 width,
	u32 height, u32 istride, addr_t oaddr, u32 ostride)
{
	int ret;

	ret = g2d_rdma_set(gd, NULL, true);
	if (ret) {
		G2D_ERROR("g2d rdma set failed\n");
		return -1;
	}

	ret = g2d_wdma_set(gd, NULL, true);
	if (ret) {
		G2D_ERROR("g2d wdma set failed\n");
		return -1;
	}

	_g2d_fastcopy(gd->iomem_regs, &gd->cmd_info,
		iaddr, width, height, oaddr, ostride);

	return 0;
}

static int g2d_scaler_coef_set(struct sdrv_g2d *gd,
	struct g2d_coeff_table *tables)
{
	if (!tables->set_tables) {
		if (!!tables->hcoef_set)
			g2d_set_hscaler_coef(gd, hscaler_coef);

		if (!!tables->vcoef_set)
			g2d_set_vscaler_coef(gd, vscaler_coef);
#if G2D_DUMP_HVSCALER
		g2d_dump_hvscaler(gd);
#endif
		return 0;
	}

	if (!!tables->hcoef_set)
		g2d_set_hscaler_coef(gd, tables->hcoef);

	if (!!tables->vcoef_set) {
		g2d_set_vscaler_coef(gd, tables->vcoef);
	}
#if G2D_DUMP_HVSCALER
	g2d_dump_hvscaler(gd);
#endif

	return 0;
}

const struct g2d_ops g2d_normal_ops = {
	.init = g2d_init,
	.enable = g2d_enable,
	.reset = g2d_reset,
	.mlc_set = g2d_mlc_set,
	.fill_rect = g2d_func_fill_rect,
	.fastcopy = g2d_fastcopy,
	.irq_handler = g2d_irq_handler,
	.rwdma = g2d_rwdma_set,
	.close_fastcopy = g2d_close_fastcopy,
	.wpipe_set = g2d_wpipe_set,
	.check_stroke = g2d_check_is_stroke,
	.scaler_coef_set = g2d_scaler_coef_set,
};
