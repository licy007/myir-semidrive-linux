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
#include "sdrv_g2d.h"
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

//############################ api #############################
static inline void dump_layer(const char *func, struct g2d_layer *layer)
{
	G2D_DBG("[dumplayer] %s *ENABLE = %d*, format: %c%c%c%c source (%d, %d, %d, %d) => dest (%d, %d, %d, %d)\n",
			func, layer->enable,
			layer->format & 0xff, (layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff, (layer->format >> 24) & 0xff,
			layer->src_x, layer->src_y, layer->src_w, layer->src_h,
			layer->dst_x, layer->dst_y, layer->dst_w, layer->dst_h);
}

static inline void dump_output(const char *func, struct g2d_output_cfg *output)
{
	G2D_DBG("[dump output] %s : w,h(%d,%d) format:%c%c%c%c rota:%d nplanes:%d\n",
		func, output->width, output->height,
		output->fmt & 0xff, (output->fmt >> 8) & 0xff, (output->fmt >> 16) & 0xff, (output->fmt >> 24) & 0xff,
		output->rotation, output->nplanes);
}
//############################ api #############################

void g2d_udelay(int us)
{
	udelay(us);
}
EXPORT_SYMBOL(g2d_udelay);

static void dump_registers(struct sdrv_g2d *dev, int start, int end)
{
	int i, count;

	count = (end - start) / 4 + 1;
	count = (count + 3) / 4;
	pr_err("[0x%04x~0x%04x]\n", start, end);
	for (i = 0; i < count; i++)
		pr_err("0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(dev->iomem_regs + start + 0x10 * i),
			readl(dev->iomem_regs + start + 0x10 * i + 0x4),
			readl(dev->iomem_regs + start + 0x10 * i + 0x8),
			readl(dev->iomem_regs + start + 0x10 * i + 0xc));
}

void g2d_dump_registers(struct sdrv_g2d *dev)
{
	pr_err("@@@@@@@@[g2d-%d]-[g2d-name:%s]-start@@@@@@@@\n", dev->id, dev->name);
	pr_err("@@@@@@@@ g2d ctrl regs @@@@@@@@\n");
	dump_registers(dev, 0x0000, 0x002c);

	pr_err("@@@@@@@@ g2d RDMA regs @@@@@@@@\n");
	dump_registers(dev, 0x1000, 0x111c);
	dump_registers(dev, 0x1400, 0x1400);
	dump_registers(dev, 0x1500, 0x154c);

	/*g-pipe0*/
	pr_err("@@@@@@@@ g2d g-pipe0 regs @@@@@@@@\n");
	dump_registers(dev, 0x2000, 0x204c);
	dump_registers(dev, 0x2100, 0x212c);
	dump_registers(dev, 0x2200, 0x222c);
	dump_registers(dev, 0x2300, 0x230c);
	dump_registers(dev, 0x2400, 0x241c);
	dump_registers(dev, 0x2500, 0x250c);
	dump_registers(dev, 0x2f00, 0x2f00);

	/*g-pipe1*/
	pr_err("@@@@@@@@ g2d g-pipe1 regs @@@@@@@@\n");
	dump_registers(dev, 0x3000, 0x304c);
	dump_registers(dev, 0x3100, 0x312c);
	dump_registers(dev, 0x3200, 0x322c);
	dump_registers(dev, 0x3300, 0x330c);
	dump_registers(dev, 0x3400, 0x341c);
	dump_registers(dev, 0x3500, 0x350c);
	dump_registers(dev, 0x3f00, 0x3f00);

	/*g-pipe2*/
	pr_err("@@@@@@@@ g2d g-pipe2 regs @@@@@@@@\n");
	dump_registers(dev, 0x4000, 0x404c);
	dump_registers(dev, 0x4100, 0x412c);
	dump_registers(dev, 0x4200, 0x422c);
	dump_registers(dev, 0x4300, 0x430c);
	dump_registers(dev, 0x4400, 0x441c);
	dump_registers(dev, 0x4500, 0x450c);
	dump_registers(dev, 0x4f00, 0x4f00);

	/*s-pipe0*/
	pr_err("@@@@@@@@ g2d S-pipe0 regs @@@@@@@@\n");
	dump_registers(dev, 0x5000, 0x504c);
	dump_registers(dev, 0x5100, 0x514c);
	dump_registers(dev, 0x5200, 0x521c);
	dump_registers(dev, 0x5f00, 0x5f00);

	/*s-pipe1*/
	pr_err("@@@@@@@@ g2d S-pipe1 regs @@@@@@@@\n");
	dump_registers(dev, 0x6000, 0x604c);
	dump_registers(dev, 0x6100, 0x614c);
	dump_registers(dev, 0x6200, 0x621c);
	dump_registers(dev, 0x6f00, 0x6f00);
	/*mlc*/
	pr_err("@@@@@@@@ g2d MLC regs @@@@@@@@\n");
	dump_registers(dev, 0x7000, 0x70ec);
	dump_registers(dev, 0x7200, 0x726c);
	/*a-pipe*/
	pr_err("@@@@@@@@ g2d a-pipe regs @@@@@@@@\n");
	dump_registers(dev, 0x9000, 0x904c);
	/*w-dma*/
	pr_err("@@@@@@@@ g2d w-dma regs @@@@@@@@\n");
	dump_registers(dev, 0xa000, 0xa11c);
	dump_registers(dev, 0xa400, 0xa400);
	dump_registers(dev, 0xa500, 0xa54c);
	/*w-pipe*/
	pr_err("@@@@@@@@ g2d w-pipe regs @@@@@@@@\n");
	dump_registers(dev, 0xb000, 0xb06c);
	/*fdbc*/
	pr_err("@@@@@@@@ g2d fdbc regs @@@@@@@@\n");
	dump_registers(dev, 0xc000, 0xc03c);
	pr_err("@@@@@@@@[g2d-%d]-[g2d-name:%s]-end@@@@@@@@\n", dev->id, dev->name);
}

static void get_bpp_by_format(unsigned int format, unsigned char *bpp)
{
	switch (format) {
		case DRM_FORMAT_R8:
			bpp[0] = 8;
			break;
		case DRM_FORMAT_RGB565:
		case DRM_FORMAT_BGR565:
		case DRM_FORMAT_YUYV:
		case DRM_FORMAT_UYVY:
		case DRM_FORMAT_XRGB4444:
		case DRM_FORMAT_XBGR4444:
		case DRM_FORMAT_BGRX4444:
		case DRM_FORMAT_ARGB4444:
		case DRM_FORMAT_ABGR4444:
		case DRM_FORMAT_BGRA4444:
		case DRM_FORMAT_XRGB1555:
		case DRM_FORMAT_XBGR1555:
		case DRM_FORMAT_BGRX5551:
		case DRM_FORMAT_ARGB1555:
		case DRM_FORMAT_ABGR1555:
		case DRM_FORMAT_BGRA5551:
			bpp[0] = 16;
			break;
		case DRM_FORMAT_RGB888:
		case DRM_FORMAT_BGR888:
			bpp[0] = 24;
			break;
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_BGRA8888:
		case DRM_FORMAT_ABGR8888:
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_BGRX8888:
		case DRM_FORMAT_XRGB2101010:
		case DRM_FORMAT_XBGR2101010:
		case DRM_FORMAT_BGRX1010102:
		case DRM_FORMAT_ARGB2101010:
		case DRM_FORMAT_ABGR2101010:
		case DRM_FORMAT_BGRA1010102:
		case DRM_FORMAT_AYUV:
			bpp[0] = 32;
			break;
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
			bpp[0] = 8;
			bpp[1] = 8;
			break;
		case DRM_FORMAT_YUV422:
			bpp[0] = 8;
			bpp[1] = 4;
			bpp[2] = 4;
			break;
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YVU420:
			bpp[0] = 8;
			bpp[1] = 4;
			bpp[2] = 4;
			break;
		case DRM_FORMAT_YUV444:
			bpp[0] = 8;
			bpp[1] = 8;
			bpp[2] = 8;
			break;
		default:
			break;
	}
}

static int get_output_bpp_by_format(unsigned int format, unsigned char *bpp)
{
	switch (format) {
		case DRM_FORMAT_R8:
			bpp[0] = 8;
			break;
		case DRM_FORMAT_RGB565:
		case DRM_FORMAT_XRGB4444:
		case DRM_FORMAT_ARGB4444:
		case DRM_FORMAT_XRGB1555:
		case DRM_FORMAT_ARGB1555:
		case DRM_FORMAT_YUYV:
		case DRM_FORMAT_UYVY:
			bpp[0] = 16;
			break;
		case DRM_FORMAT_RGB888:
			bpp[0] = 24;
			break;
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB2101010:
		case DRM_FORMAT_ARGB2101010:
		case DRM_FORMAT_AYUV:
			bpp[0] = 32;
			break;
		case DRM_FORMAT_NV21:
			bpp[0] = 8;
			bpp[1] = 8;
			break;
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YUV422:
			bpp[0] = 8;
			bpp[1] = 4;
			bpp[2] = 4;
			break;
		case DRM_FORMAT_YUV444:
			bpp[0] = 8;
			bpp[1] = 8;
			bpp[2] = 8;
			break;
		default:
			return -1;
			break;
	}
	return 0;
}

static int get_planes_ratio_by_format(unsigned int format)
{
	int planes_ratio = 1;
	switch (format) {
		case DRM_FORMAT_YUV420:
			planes_ratio = 2;
			break;
		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_NV21:
			planes_ratio = 2;
			break;
		case DRM_FORMAT_YUV444:
			planes_ratio = 1;
			break;
		default:
			planes_ratio = 1;
			break;
	}
	return planes_ratio;

}

bool g2d_format_is_yuv(uint32_t format) {
	switch (format) {
		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_YUYV:
		case DRM_FORMAT_VYUY:
		case DRM_FORMAT_UYVY:
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YUV444:
		case DRM_FORMAT_AYUV:
			return true; // bypass: 0, v-bypass: 1
		default:
			return false;
	}
	return false;
}

int g2d_format_wpipe_bypass(uint32_t format) {
	switch (format) {
		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_YUYV:
		case DRM_FORMAT_VYUY:
		case DRM_FORMAT_UYVY:
			return 0x1; // bypass: 0, v-bypass: 1
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_YUV420:
			return 0x0; // bypass: 0, v-bypass: 0
		case DRM_FORMAT_YUV444:
		default:
			return 0x3; // bypass: 1, v-bypass: 1
	}
	return 0;
}

static int get_tile_hsize(uint32_t select)
{
	int value;
	switch (select) {
		case TILE_HSIZE_1:
			value = 1;
			break;
		case TILE_HSIZE_8:
			value = 8;
			break;
		case TILE_HSIZE_16:
			value = 16;
			break;
		case TILE_HSIZE_32:
			value = 32;
			break;
		case TILE_HSIZE_64:
			value = 64;
			break;
		case TILE_HSIZE_128:
			value = 128;
			break;
		default:
			value = 0;
			break;
	}
	return value;
}

static int get_tile_vsize(uint32_t select)
{
	int value;
	switch (select) {
		case TILE_VSIZE_1:
			value = 1;
			break;
		case TILE_VSIZE_2:
			value = 2;
			break;
		case TILE_VSIZE_4:
			value = 4;
			break;
		case TILE_VSIZE_8:
			value = 8;
			break;
		case TILE_VSIZE_16:
			value = 16;
			break;
		default:
			value = 0;
			break;
	}
	return value;
}

int sdrv_wpipe_pix_comp(uint32_t format, struct pix_g2dcomp *comp)
{
	memset(comp, 0, sizeof(*comp));

	comp->endin = 0;
	switch (format) {
		case DRM_FORMAT_XRGB8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB8888");
			break;
		case DRM_FORMAT_ARGB8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB8888");
			break;
		case DRM_FORMAT_RGB888:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_RGB888");
			break;
		case DRM_FORMAT_RGB565:
			comp->bpa = 0;
			comp->bpy = comp->bpv = 5;
			comp->bpu = 6;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_RGB565");
			break;
		case DRM_FORMAT_XRGB4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB4444");
			break;
		case DRM_FORMAT_ARGB4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB4444");
			break;
		case DRM_FORMAT_XRGB1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB1555");
			break;
		case DRM_FORMAT_ARGB1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB1555");
			break;
		case DRM_FORMAT_XRGB2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB2101010");
			break;
		case DRM_FORMAT_ARGB2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB2101010");
			break;
		case DRM_FORMAT_R8:
			comp->bpy = 8;
			comp->bpa = comp->bpu = comp->bpv = 0;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_R8");
			break;

		case DRM_FORMAT_YUYV:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->endin = 1;
			comp->uvmode = UV_YUV422;
			sprintf(comp->format_string,"DRM_FORMAT_YUYV");
			break;

		case DRM_FORMAT_UYVY:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			sprintf(comp->format_string,"DRM_FORMAT_UYVY");
			break;
		case DRM_FORMAT_AYUV:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			sprintf(comp->format_string,"DRM_FORMAT_AYUV");
			break;
		case DRM_FORMAT_NV21:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_SEMI_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_NV21");
			break;
		case DRM_FORMAT_YUV420:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_YUV420");
			break;
		case DRM_FORMAT_YUV422:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_YUV422");
			break;
		case DRM_FORMAT_YUV444:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_YUV444");
			break;
		case DRM_FORMAT_RGB888_PLANE:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_RGB888_PLANE");
			break;
		case DRM_FORMAT_BGR888_PLANE:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_BGR888_PLANE");
			break;

		default:
			G2D_ERROR("unsupported format[%08x]\n", format);
			return -EINVAL;
	}
	return 0;
}

int sdrv_pix_comp(uint32_t format, struct pix_g2dcomp *comp)
{
	memset(comp, 0, sizeof(*comp));

	comp->endin = 0;
	switch (format) {
		case DRM_FORMAT_XRGB8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB8888");
			break;
		case DRM_FORMAT_XBGR8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_XBGR8888");
			break;
		case DRM_FORMAT_BGRX8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRX8888");
			break;
		case DRM_FORMAT_ARGB8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB8888");
			break;
		case DRM_FORMAT_ABGR8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_ABGR8888");
			break;
		case DRM_FORMAT_BGRA8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRA8888");
			break;
		case DRM_FORMAT_RGB888:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_RGB888");
			break;
		case DRM_FORMAT_BGR888:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_BGR888");
			break;
		case DRM_FORMAT_RGB565:
			comp->bpa = 0;
			comp->bpy = comp->bpv = 5;
			comp->bpu = 6;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_RGB565");
			break;
		case DRM_FORMAT_BGR565:
			comp->bpa = 0;
			comp->bpy = comp->bpv = 5;
			comp->bpu = 6;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_BGR565");
			break;
		case DRM_FORMAT_XRGB4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB4444");
			break;
		case DRM_FORMAT_XBGR4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_XBGR4444");
			break;
		case DRM_FORMAT_BGRX4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRX4444");
			break;
		case DRM_FORMAT_ARGB4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB4444");
			break;
		case DRM_FORMAT_ABGR4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_ABGR4444");
			break;
		case DRM_FORMAT_BGRA4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRA4444");
			break;
		case DRM_FORMAT_XRGB1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB1555");
			break;
		case DRM_FORMAT_XBGR1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_XBGR1555");
			break;
		case DRM_FORMAT_BGRX5551:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRX5551");
			break;
		case DRM_FORMAT_ARGB1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB1555");
			break;
		case DRM_FORMAT_ABGR1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_ABGR1555");
			break;
		case DRM_FORMAT_BGRA5551:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRA5551");
			break;
		case DRM_FORMAT_XRGB2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_XRGB2101010");
			break;
		case DRM_FORMAT_XBGR2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_XBGR2101010");
			break;
		case DRM_FORMAT_BGRX1010102:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRX1010102");
			break;
		case DRM_FORMAT_ARGB2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_ARGB2101010");
			break;
		case DRM_FORMAT_ABGR2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_BGR;
			sprintf(comp->format_string,"DRM_FORMAT_ABGR2101010");
			break;
		case DRM_FORMAT_BGRA1010102:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_B_GRA;
			sprintf(comp->format_string,"DRM_FORMAT_BGRA1010102");
			break;
		case DRM_FORMAT_R8:
			comp->bpy = 8;
			comp->bpa = comp->bpu = comp->bpv = 0;
			comp->swap = SWAP_A_RGB;
			sprintf(comp->format_string,"DRM_FORMAT_R8");
			break;

		case DRM_FORMAT_YUYV:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->endin = 1;
			comp->uvmode = UV_YUV422;
			sprintf(comp->format_string,"DRM_FORMAT_YUYV");
			break;

		case DRM_FORMAT_UYVY:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			sprintf(comp->format_string,"DRM_FORMAT_UYVY");
			break;
		case DRM_FORMAT_VYUY:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->uvswap = 1;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			sprintf(comp->format_string,"DRM_FORMAT_VYUY");
			break;
		case DRM_FORMAT_AYUV:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			sprintf(comp->format_string,"DRM_FORMAT_AYUV");
			break;
		case DRM_FORMAT_NV12:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->uvswap = 1;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_SEMI_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_NV12");
			break;
		case DRM_FORMAT_NV21:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_SEMI_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_NV21");
			break;
		case DRM_FORMAT_YUV420:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_YUV420");
			break;
		case DRM_FORMAT_YVU420:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RBG;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_YUV420");
			break;
		case DRM_FORMAT_YUV422:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_YUV422");
			break;
		case DRM_FORMAT_YUV444:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_YUV444");
			break;
		case DRM_FORMAT_RGB888_PLANE:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_RGB888_PLANE");
			break;
		case DRM_FORMAT_BGR888_PLANE:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->buf_fmt = FMT_PLANAR;
			sprintf(comp->format_string,"DRM_FORMAT_BGR888_PLANE");
			break;
		default:
			G2D_ERROR("unsupported format[%08x]\n", format);
			return -EINVAL;
	}
	return 0;
}

static int sdrv_set_tile_ctx(struct g2d_layer *layer)
{
	uint32_t x1, y1, x2, y2;
	struct tile_ctx *ctx = &layer->ctx;
	uint32_t format = layer->format;

	switch (format) {
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_BGRX8888:
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_ABGR8888:
		case DRM_FORMAT_BGRA8888:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_8X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_8;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_RAW_TILE_MODE;
				break;
				case DRM_FORMAT_MOD_SEMIDRIVE_16X4_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X2_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_8X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_8;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8U8U8U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_16X4_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8U8U8U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X2_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8U8U8U8;
					break;
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		case DRM_FORMAT_RGB565:
		case DRM_FORMAT_BGR565:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_16X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X4_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X2_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_16X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U16;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X4_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U16;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X2_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U16;
					break;
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		case DRM_FORMAT_R8:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_32X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X4_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_128X2_TILE:
					ctx->tile_hsize = TILE_HSIZE_128;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X4_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_128X2_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_128;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8;
					break;
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_CODA_16X16_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_16;
					ctx->data_mode = VPU_RAW_TILE_988_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_WAVE_32X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = VPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_WAVE_32X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = VPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_NV21;
					break;
				/*case DRM_FORMAT_MOD_SEMIDRIVE_WAVE_16X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_YUV420_16PACK;
					break;*/
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		default:
			ctx->data_mode = LINEAR_MODE;
			break;
	}

	x1 = layer->src_x;
	y1 = layer->src_y;
	x2 = layer->src_x + layer->src_w;
	y2 = layer->src_y + layer->src_h;

	ctx->tile_hsize_value = get_tile_hsize(ctx->tile_hsize);
	ctx->tile_vsize_value = get_tile_vsize(ctx->tile_vsize);

	ctx->x1 = ALIGN_DOWN(x1, ctx->tile_hsize_value);
	ctx->y1 = ALIGN_DOWN(y1, ctx->tile_vsize_value);

	ctx->x2 = ALIGN(x2, ctx->tile_hsize_value);
	ctx->y2 = ALIGN(y2, ctx->tile_vsize_value);

	ctx->width = ctx->x2 - ctx->x1;
	ctx->height = ctx->y2 - ctx->y1;
	if ((format == DRM_FORMAT_NV12) || (format == DRM_FORMAT_NV21)) {
		if (ctx->width / ctx->tile_hsize_value % 2 == 1) {
			ctx->x2 += ctx->tile_hsize_value;
			ctx->width = ctx->x2 - ctx->x1;
		}
	}

	if ((ctx->data_mode == GPU_RAW_TILE_MODE) ||
		(ctx->data_mode == GPU_CPS_TILE_MODE) ||
		(ctx->data_mode == VPU_RAW_TILE_MODE) ||
		(ctx->data_mode == VPU_CPS_TILE_MODE) ||
		(ctx->data_mode == VPU_RAW_TILE_988_MODE))
		ctx->is_tile_mode = true;
	else
		ctx->is_tile_mode = false;

	if ((ctx->data_mode == GPU_CPS_TILE_MODE) ||
		(ctx->data_mode == VPU_CPS_TILE_MODE))
		ctx->is_fbdc_cps = true;
	else
		ctx->is_fbdc_cps = false;

	return 0;
}

/**
 * sdrv_layer_config()
 * @brief Get pixel component details
 *
 */
static int sdrv_layer_config(struct g2d_layer *layer)
{

	dump_layer(__func__, layer);

	sdrv_pix_comp(layer->format, &layer->comp);

	if (layer->modifier)
		sdrv_set_tile_ctx(layer);

	return 0;
}

static void g2d_slice_input(struct g2d_layer *src, struct g2d_layer *slice_input,
		unsigned int slice_w, unsigned char max_slice_num, unsigned char slice_idx)
{
	int i, j;
	unsigned int slice_width;
	unsigned long src_addr;
	unsigned char nplanes;
	unsigned char bpp[3] = {0};
	/* copy */
	if (!slice_idx) {
		slice_input->enable = src->enable;
		slice_input->format = src->format;
		slice_input->nplanes = src->nplanes;

		slice_input->zpos = src->zpos;
		slice_input->src_x = src->src_x;
		slice_input->src_y = src->src_y;
		slice_input->src_h = src->src_h;
		slice_input->dst_x = src->dst_x;
		slice_input->dst_y = src->dst_y;
		slice_input->dst_h = src->dst_h;
		// slice_input->ckey_en = src->ckey_en;
		// slice_input->ckey = src->ckey;
		slice_input->blend_mode = src->blend_mode;
		slice_input->alpha = src->alpha;
		for (j = 0; j < src->nplanes; j++) {
			slice_input->pitch[j] = src->pitch[j];
			G2D_DBG("slice_input->pitch[%d] = %d\n", j, slice_input->pitch[j]);
		}
	}

	slice_input->index = src->index;
	/*change to slice*/
	nplanes = src->nplanes;
	get_bpp_by_format(src->format, bpp);
	slice_width = (slice_idx == (max_slice_num -1)) ? ((src->dst_w	% slice_w) ? : slice_w) : slice_w;
	slice_input->src_w = slice_width;
	slice_input->dst_w = slice_width;

	src_addr = src->addr_l[0] + slice_idx * slice_w * bpp[0] / 8;
	slice_input->addr_l[0] = src_addr;
	if (nplanes > 1) {
		src_addr = src->addr_l[1] + slice_idx * slice_w * bpp[1] / 8;
		slice_input->addr_l[1] = src_addr;
	}
	if (nplanes > 2) {
		src_addr = src->addr_l[2] + slice_idx * slice_w * bpp[2] / 8;
		slice_input->addr_l[2] = src_addr;
	}

	for(i = 0; i < nplanes; i++) {
		G2D_DBG("slice id[%d] slice_input->addr_l[%d] = 0x%x\n", slice_idx, i, slice_input->addr_l[i]);
	}

#if 0 //use G2D_CPU_WRITE for test
	src_addr = get_full_addr(src->addr_h[0], src->addr_l[0]) + slice_idx * slice_w * bpp[0] / 8;
	slice_input->addr_l[0] = get_l_addr(src_addr);
	slice_input->addr_h[0] = get_h_addr(src_addr);
	if (nplanes > 1) {
		src_addr = get_full_addr(src->addr_h[1], src->addr_l[1]) + slice_idx * slice_w * bpp[1] / 8;
		slice_input->addr_l[1] = get_l_addr(src_addr);
		slice_input->addr_h[1] = get_h_addr(src_addr);
	}
	if (nplanes > 2) {
		src_addr = get_full_addr(src->addr_h[2], src->addr_l[2]) + slice_idx * slice_w * bpp[2] / 8;
		slice_input->addr_l[2] = get_l_addr(src_addr);
		slice_input->addr_h[2] = get_l_addr(src_addr);
	}

	for(i = 0; i < nplanes; i++) {
		G2D_DBG("slice id[%d] slice_input->addr_l[%d] = 0x%x, slice_input->addr_h[%d] = 0x%x\n",
			 slice_idx, i, slice_input->addr_l[i], i, slice_input->addr_h[i]);
	}
#endif
}

static int g2d_slice_output(struct g2d_output_cfg *src,
		struct g2d_output_cfg *slice_output, unsigned int slice_w,
		unsigned char max_slice_num, unsigned char slice_idx)
{
	int ret = 0;
	unsigned int slice_width;
	unsigned char nplanes;
	unsigned char bpp[3] = {0};
	int i;
	int planes_ratio;
	if (!slice_idx) {
		slice_output->height = src->height;
		slice_output->fmt = src->fmt;
		slice_output->nplanes = src->nplanes;
		for (i = 0; i < src->nplanes; i++) {
			slice_output->stride[i] = src->stride[i];
			G2D_DBG("slice_input->pitch[%d] = %d\n", i, slice_output->stride[i]);
		}
		slice_output->rotation = src->rotation;
	}
	nplanes = src->nplanes;
	ret = get_output_bpp_by_format(src->fmt, bpp);
	if (ret < 0)
		return ret;
	planes_ratio = get_planes_ratio_by_format(src->fmt);
	slice_width = (slice_idx == (max_slice_num -1)) ? ((src->width	% slice_w) ? : slice_w) : slice_w;
	slice_output->width = slice_width;
	switch (src->rotation) {
		case ROTATION_TYPE_NONE:
		case ROTATION_TYPE_VFLIP:
			slice_output->addr[0] = src->addr[0] + slice_idx * slice_w * bpp[0] / 8;
			if (nplanes > 1) {
				slice_output->addr[1] = src->addr[1] + slice_idx * slice_w	* bpp[1] / 8;
			}
			if (nplanes > 2) {
				slice_output->addr[2] = src->addr[2] + slice_idx * slice_w * bpp[2] / 8;
			}
			break;
		case ROTATION_TYPE_ROT_90:
		case ROTATION_TYPE_HF_90:
			slice_output->addr[0] = src->addr[0] + slice_idx * slice_w * src->stride[0];
			if (nplanes > 1) {
				slice_output->addr[1] = src->addr[1] + slice_idx * slice_w * src->stride[1] / planes_ratio;
			}
			if (nplanes > 2) {
				slice_output->addr[2] = src->addr[2] + slice_idx * slice_w * src->stride[2] / planes_ratio;
			}
			break;
		case ROTATION_TYPE_HFLIP:
		case ROTATION_TYPE_ROT_180:
			slice_output->addr[0] = src->addr[0] + ((slice_idx == max_slice_num - 1) ? 0 :
				(src->width - (slice_idx + 1) * slice_w) * bpp[0] / 8);
			if (nplanes > 1) {
				slice_output->addr[1] = src->addr[1] + ((slice_idx == max_slice_num - 1) ? 0 :
					(src->width - (slice_idx + 1) * slice_w) * bpp[1] / 8);
			}
			if (nplanes > 2) {
				slice_output->addr[2] = src->addr[2]  + ((slice_idx == max_slice_num - 1) ? 0 :
					(src->width - (slice_idx + 1) * slice_w) * bpp[2] / 8);
			}
			break;
		case ROTATION_TYPE_ROT_270:
		case ROTATION_TYPE_VF_90:
			slice_output->addr[0] = src->addr[0] + ((slice_idx == max_slice_num - 1) ? 0 :
				(src->width - (slice_idx + 1) * slice_w) * src->stride[0]);
			if (nplanes > 1) {
				slice_output->addr[1] = src->addr[1] + ((slice_idx == max_slice_num - 1) ? 0 :
					(src->width - (slice_idx + 1) * slice_w) * src->stride[1] / planes_ratio);
			}
			if (nplanes > 2) {
				slice_output->addr[2] = src->addr[2] + ((slice_idx == max_slice_num - 1) ? 0 :
					(src->width - (slice_idx + 1) * slice_w) * src->stride[2] / planes_ratio);
			}
			break;
		default:
			break;
	}

	G2D_DBG("rotation %d\n", src->rotation);
	for(i = 0; i < nplanes; i++) {
		G2D_DBG("slice id[%d] slice_output->addr[%d] = 0x%llx\n", slice_idx, i, slice_output->addr[i]);
		G2D_DBG("out stride[%d] = %d\n", i, src->stride[i]);
	}
	return ret;
}

/*
FIXME:Existence problem
1. Multi layer stitching in strip case
*/
static int g2d_strip_func(struct sdrv_g2d *dev, struct g2d_input *ins)
{
	int ret = 0;
	unsigned int i, j;
	struct g2d_layer *slice_input;
	struct g2d_output_cfg *slice_output;
	struct g2d_pipe *pipe;
	const unsigned int slice_w = 256;
	unsigned int slice_count;
	struct g2d_input *slice_ins;

	G2D_DBG("strip start !\n");
	ret = dev->ops->rwdma(dev, ins);
	if (ret) {
		return ret;
	}

	slice_ins = kzalloc(sizeof(struct g2d_input), GFP_ATOMIC | GFP_DMA);
	if (!slice_ins) {
		G2D_ERROR("kzalloc slice_ins failed\n");
		return -1;
	}
	slice_input = &slice_ins->layer[0];
	slice_output = &slice_ins->output;
	slice_count = (ins->output.width / slice_w) + ((ins->output.width % slice_w) ? 1 : 0);
	for (i = 0; i < slice_count; i++) {
		G2D_DBG("slice[%d][%d]\n", i, slice_count);
		for (j = 0; j < ins->layer_num; j++) {
			struct g2d_layer *layer =  &ins->layer[j];
			if (!layer->enable)
				continue;

			if (layer->index >= dev->num_pipe) {
				dev->ops->reset(dev);
				G2D_ERROR("ins layer index is invalid : %d \n", layer->index);
				ret = -1;
				goto strip_out;
			}
			pipe = dev->pipes[layer->index];
			//build slice layer
			g2d_slice_input(layer, slice_input, slice_w, slice_count, i);
			//pipe set
			sdrv_layer_config(slice_input);
			pipe->ops->set(pipe, i, slice_input);
			//mlc & apipe set
			ret = dev->ops->mlc_set(dev, i, slice_ins);
			if (ret < 0) {
				ret = -1;
				goto strip_out;
			}
		}
		dump_output(__func__, &ins->output);
		//build slice output
		ret = g2d_slice_output(&ins->output, slice_output, slice_w, slice_count, i);
		if (ret < 0) {
			ret = -1;
			goto strip_out;
		}
		//wpipe set
		ret = dev->ops->wpipe_set(dev, i, slice_output);
		if (ret < 0) {
			ret = -1;
			goto strip_out;
		}
	}
	G2D_DBG("strip end !\n");

strip_out:
	kfree(slice_ins);
	return ret;
}

static int g2d_convert_func(struct sdrv_g2d *dev, struct g2d_input *ins)
{
	int ret = 0;
	unsigned int i;
	struct g2d_layer *layer;
	struct g2d_pipe *pipe;
	uint32_t index;

	G2D_DBG("convert start \n");

	ret = dev->ops->rwdma(dev, ins);
	if (ret) {
		return ret;
	}

	ret = dev->ops->mlc_set(dev, 0, ins);
	if (ret) {
		G2D_ERROR("mlc set faild \n");
		return ret;
	}

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
		pipe = dev->pipes[index];
		if (layer->src_w != layer->dst_w || layer->src_h != layer->dst_h)
		{
			if (!pipe->cap->scaling) {
				G2D_ERROR("pipe %d [%s] cannot support scaling\n",
				pipe->id, pipe->name);
				return -1;
			}
		}

		sdrv_layer_config(layer);
		pipe->ops->set(pipe, 0, layer);
	}
	dump_output(__func__, &ins->output);
	ret = dev->ops->wpipe_set(dev, 0, &ins->output);
	if(ret < 0) {
		return ret;
	}

	G2D_DBG("convert end \n");

	return ret;
}

static int g2d_enable(struct sdrv_g2d *gd, int enable)
{
	int i, j;
	unsigned char valid_count = 0;
	cmdfile_info *cmd;
	unsigned long input_addr;
	struct platform_device *pdev;
	struct device *dev;

#define G2DLITE_CMDFILE_ADDR_L (0x10)
#define G2DLITE_CMDFILE_ADDR_H (0x14)
#define G2DLITE_CMDFILE_LEN    (0x18)
#define G2DLITE_CMDFILE_CFG    (0x1c)

	if (gd->write_mode == G2D_CMD_WRITE) {
		for (i = 0; i < G2D_CMDFILE_MAX_NUM; i++) {
			cmd = &gd->cmd_info[i];
			if (!cmd->dirty) {
				valid_count = i;
				break;
			} else {
				cmd->dirty = 0;
			}
		}
		G2D_DBG("valid_count = %d\n", valid_count);
		for (i = 0; i < valid_count; i++) {
			cmd = &gd->cmd_info[i];
			if (i < valid_count - 1) {
				input_addr = gd->dma_buf + G2D_CMDFILE_MAX_LEN * 2 * sizeof(unsigned int) * (i + 1);
				cmd->arg[cmd->len++] = G2DLITE_CMDFILE_ADDR_L;
				cmd->arg[cmd->len++] = get_l_addr(input_addr);
				cmd->arg[cmd->len++] = G2DLITE_CMDFILE_ADDR_H;
				cmd->arg[cmd->len++] = get_h_addr(input_addr);
				cmd->arg[cmd->len++] = G2DLITE_CMDFILE_LEN;
				cmd->arg[cmd->len++] = 0;//reserve for len value config
			} else {
				cmd->arg[cmd->len++] = G2DLITE_CMDFILE_CFG;
				cmd->arg[cmd->len++] = 0;

				G2D_DBG("G2DLITE_CMDFILE_CFG = %d\n", G2DLITE_CMDFILE_CFG);
			}
		}

		for (i = 0; i < valid_count - 1; i++) {
			cmd = &gd->cmd_info[i];
			cmd->arg[cmd->len - 1] = gd->cmd_info[i + 1].len / 2 - 1;
			G2D_DBG("gd->cmd_info[i + 1].len / 2 - 1 = 0x%x\n", gd->cmd_info[i + 1].len / 2 - 1);
		}

		for (i = 0; i < valid_count; i++) {
			cmd = &gd->cmd_info[i];
			//cache opration
			pdev = gd->pdev;
			dev = &pdev->dev;
			dma_sync_single_for_device(dev, ((dma_addr_t)(gd->dma_buf) + G2D_CMDFILE_MAX_LEN * 2 * sizeof(unsigned int) * i),
				cmd->len * 4, DMA_TO_DEVICE);
		}

		input_addr = gd->dma_buf;
		// the low 32bits bit[1:0] always be 0
		G2D_DBG("the low 32bits bit[1:0] always be 0 = l 0x%x, h 0x%x\n",
			get_l_addr(input_addr), get_h_addr(input_addr));
		for (i = 0; i < valid_count; i++) {
			cmd = &gd->cmd_info[i];
			G2D_DBG("\n");
			G2D_DBG("cmdfile len = %d \n", cmd->len /2);
			for (j = 0; j < cmd->len; j += 2) {
				G2D_DBG("0x%08x | 0x%08x \n", cmd->arg[j], cmd->arg[j+1]);
			}
		}
	}

	if (gd->ops->enable) {
		gd->ops->enable(gd, 1);
	}

	if (gd->write_mode == G2D_CMD_WRITE) {
		for (i = 0; i < valid_count; i++) {
			gd->cmd_info[i].len = 0;
		}
	}

	return 0;
}

int g2d_post_config(struct sdrv_g2d *dev, struct g2d_input *ins)
{
	int ret = 0;

	G2D_DBG("start\n");

	smp_mb();
	if (ins->layer_num > dev->num_pipe) {
		G2D_ERROR("dev->num_pipe: %d, layer_num: %d invalid.\n", dev->num_pipe, ins->layer_num);
		return -1;
	}

	dev->ops->reset(dev);
	dev->ops->close_fastcopy(dev);

	ret = dev->ops->check_stroke(ins);
	if (ret < 0)
		return ret;

	if (ret)
		ret = g2d_strip_func(dev, ins);
	else
		ret = g2d_convert_func(dev, ins);

	if (ret < 0)
		return ret;

	g2d_enable(dev, 1);

	G2D_DBG("end\n");
	return 0;
}

int g2d_fill_rect(struct sdrv_g2d *dev, struct g2d_bg_cfg *bgcfg, struct g2d_output_cfg *output)
{
	int ret = -1;
	if (dev->ops->reset)
		dev->ops->reset(dev);

	dev->ops->close_fastcopy(dev);

	if (dev->ops->fill_rect) {
		dev->ops->fill_rect(dev, bgcfg, output);
		ret = dev->ops->wpipe_set(dev, 0, output);
	}

	g2d_enable(dev, 1);

	return ret;
}

int g2d_fastcopy_set(struct sdrv_g2d *dev, addr_t iaddr, u32 width,
	u32 height, u32 istride, addr_t oaddr, u32 ostride)
{
	int ret = -1;
	G2D_DBG("start\n");
	smp_mb();
	if (dev->ops->reset)
		dev->ops->reset(dev);

	if (dev->ops->fastcopy) {
		ret = dev->ops->fastcopy(dev, iaddr, width, height, istride, oaddr, ostride);
		if (ret) {
			G2D_ERROR("fastcopy failed\n");
			return ret;
		}
	}

	g2d_enable(dev, 1);

	if (ret) {
		G2D_ERROR("enable failed\n");
		return ret;
	}
	G2D_DBG("end\n");

	return 0;
}

int g2d_set_coefficients_table(struct sdrv_g2d *gd, struct g2d_coeff_table *tables)
{
	int i, ret = 0;
	struct g2d_pipe *pipe;

	if (gd->ops->scaler_coef_set) {
		ret = gd->ops->scaler_coef_set(gd, tables);
	}

	for (i = 0; i < gd->num_pipe; i++) {
		pipe = gd->pipes[i];
		if (pipe->type < TYPE_SPIPE) {
			if(pipe->ops->csc_coef_set)
				pipe->ops->csc_coef_set(pipe, tables);
		}
	}

	return ret;
}
