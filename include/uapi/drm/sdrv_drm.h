/*
 * sdrv_drm.h
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

#ifndef __SDRV_DRM_H
#define __SDRV_DRM_H

#include <linux/types.h>

struct pix_comp {
	__u8 bpa;
	__u8 bpy;
	__u8 bpu;
	__u8 bpv;
	__u8 swap;
	__u8 endin;
	__u8 uvswap;
	__u8 is_yuv;
	__u8 uvmode;
	__u8 buf_fmt;
	const char *format_string;
};

struct pix_g2dcomp {
	__u8 bpa;
	__u8 bpy;
	__u8 bpu;
	__u8 bpv;
	__u8 swap;
	__u8 endin;
	__u8 uvswap;
	__u8 is_yuv;
	__u8 uvmode;
	__u8 buf_fmt;
	char format_string[64];
};

struct tile_ctx {
	bool is_tile_mode;
	__u32 data_mode;
	bool is_fbdc_cps;
	__u32 tile_hsize;
	__u32 tile_vsize;
	__u32 tile_hsize_value;
	__u32 tile_vsize_value;
	__u32 fbdc_fmt;
	__u32 x1;
	__u32 y1;
	__u32 x2;
	__u32 y2;
	__u32 width;
	__u32 height;
};

struct g2d_buf {
	int fd;
	__u64 dma_addr;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	__u64 vaddr;
	__u64 size;
};

struct g2d_buf_info {
	int fd;
	__u64 dma_addr;
	__u64 vaddr;
	__u64 size;
};
struct g2d_layer {
	__u8 index; //plane index
	__u8 enable;
	__u8 nplanes;
	__u32 addr_l[4];
	__u32 addr_h[4];
	__u32 pitch[4];
	__u32 offsets[4];
	__s16 src_x;
	__s16 src_y;
	__s16 src_w;
	__s16 src_h;
	__s16 dst_x;
	__s16 dst_y;
	__u16 dst_w;
	__u16 dst_h;
	__u32 format;
	struct pix_g2dcomp comp;
	struct tile_ctx ctx;
	__u32 alpha;
	__u32 blend_mode;
	__u32 rotation;
	__u32 zpos;
	__u32 xfbc;
	__u64 modifier;
	__u32 width;
	__u32 height;
	struct g2d_buf_info in_buf[4];
	struct g2d_buf bufs[4];
};

struct dpc_layer {
	__u8 index; //plane index
	__u8 enable;
	__u8 nplanes;
	__u32 addr_l[4];
	__u32 addr_h[4];
	__u32 pitch[4];
	__s16 src_x;
	__s16 src_y;
	__s16 src_w;
	__s16 src_h;
	__s16 dst_x;
	__s16 dst_y;
	__u16 dst_w;
	__u16 dst_h;
	__u32 format;
	struct pix_comp comp;
	struct tile_ctx ctx;
	__u32 alpha;
	__u32 blend_mode;
	__u32 rotation;
	__u32 zpos;
	__u32 xfbc;
	__u64 modifier;
	__u32 width;
	__u32 height;
	__u32 header_size_r;
	__u32 header_size_y;
	__u32 header_size_uv;
	int pd_type;
	int pd_mode;
	__u32 dataspace;
};


#endif
