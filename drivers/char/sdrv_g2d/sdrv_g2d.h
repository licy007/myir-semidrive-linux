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
#ifndef __SDRV_G2D_H__
#define __SDRV_G2D_H__

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <asm/io.h>
#include <linux/iommu.h>
#include <linux/wait.h>
#include <uapi/drm/drm_fourcc.h>
#include <uapi/drm/sdrv_g2d_cfg.h>
#include "g2d_common.h"

#define PR_INFO	pr_info
#define ERROR	pr_err
typedef unsigned long int addr_t;


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

extern int debug_g2d;
#define G2D_INFO(fmt, args...) do {\
	PR_INFO("[g2d] [%20s] " fmt, __func__, ##args);\
}while(0)
#define G2D_DBG(fmt, args...) do {\
	if (debug_g2d >= 1) {\
		PR_INFO("[g2d] <%d> [%20s] " fmt, __LINE__, __func__, ##args);}\
}while(0)
#define G2D_ERROR(fmt, args...) ERROR("[g2d] <%d> [%20s] Error: " fmt, __LINE__, __func__, ##args)

#define DDBG(x) G2D_DBG(#x " -> %d\n", x)
#define XDBG(x)	G2D_DBG(#x " -> 0x%x\n", x)
#define PDBG(x)	G2D_DBG(#x " -> %p\n", x)
#define ENTRY() G2D_DBG("call <%d>\n", __LINE__)

#define GP_ECHO_NAME    "g2d_gpipe_echo"
#define GP_MID_NAME     "g2d_gpipe_mid"
#define GP_HIGH_NAME    "g2d_gpipe_high"
#define SPIPE_NAME      "g2d_spipe"

#define G2D_NR_DEVS 4

/*Kunlun DP layer format TILE vsize*/
enum {
	TILE_VSIZE_1	   = 0b000,
	TILE_VSIZE_2	   = 0b001,
	TILE_VSIZE_4	   = 0b010,
	TILE_VSIZE_8	   = 0b011,
	TILE_VSIZE_16	   = 0b100,
};

/*Kunlun DP layer format TILE hsize*/
enum {
	TILE_HSIZE_1	   = 0b000,
	TILE_HSIZE_8	   = 0b001,
	TILE_HSIZE_16	   = 0b010,
	TILE_HSIZE_32	   = 0b011,
	TILE_HSIZE_64	   = 0b100,
	TILE_HSIZE_128	   = 0b101,
};

/**/
enum {
	FBDC_U8U8U8U8	   = 0xc,
	FBDC_U16		   = 0x9,
	FBDC_R5G6B5		   = 0x5,
	FBDC_U8			   = 0x0,
	FBDC_NV21		   = 0x37,
	FBDC_YUV420_16PACK = 0x65
};
enum kunlun_plane_property {
	PLANE_PROP_ALPHA,
	PLANE_PROP_BLEND_MODE,
	PLANE_PROP_FBDC_HSIZE_Y,
	PLANE_PROP_FBDC_HSIZE_UV,
	PLANE_PROP_CAP_MASK,
	PLANE_PROP_MAX_NUM
};
enum {
	DRM_MODE_BLEND_PIXEL_NONE = 0,
	DRM_MODE_BLEND_PREMULTI,
	DRM_MODE_BLEND_COVERAGE
};
enum {
	PLANE_DISABLE,
	PLANE_ENABLE
};

enum {
	PROP_PLANE_CAP_RGB = 0,
	PROP_PLANE_CAP_YUV,
	PROP_PLANE_CAP_XFBC,
	PROP_PLANE_CAP_YUV_FBC,
	PROP_PLANE_CAP_ROTATION,
	PROP_PLANE_CAP_SCALING,
};

enum {
	TYPE_GP_ECHO = 0,
	TYPE_GP_MID,
	TYPE_GP_HIGH,
	TYPE_SPIPE
};

struct g2d_pipe;

struct pipe_operation {
	int (*init)(struct g2d_pipe *);
	int (*set)(struct g2d_pipe *, int , struct g2d_layer *);
	void (*csc_coef_set)(struct g2d_pipe *, struct g2d_coeff_table *);
};

struct g2d_pipe {
	void __iomem *iomem_regs;
	void __iomem *regs;
	unsigned long reg_offset;
	int id; // the ordered id from 0
	struct sdrv_g2d *gd;
	const char *name;
	int type;
	struct pipe_operation *ops;
	struct g2d_pipe_capability *cap;
	struct g2d_pipe *next;
};

struct g2d_monitor {
	int is_monitor;
	int is_init;
	ktime_t timeout;
	struct hrtimer timer;
	bool g2d_on_task;
	int occupancy_rate;
	int timer_count;
	int valid_times;
	int sampling_time;
};

struct sdrv_g2d {
	struct platform_device *pdev;
	struct cdev cdev;
	struct miscdevice mdev;
	void __iomem *iomem_regs;
	void __iomem *regs;
	bool iommu_enable;
	struct iommu_domain *domain;
	struct mutex m_lock;
	struct wait_queue_head wq;
	bool frame_done;
	int id;
	const char *name;
	int irq;
	int write_mode;
	cmdfile_info cmd_info[G2D_CMDFILE_MAX_NUM];
	unsigned long dma_buf;
	const struct g2d_ops *ops;
	struct g2d_capability cap;
	struct g2d_pipe *pipes[PIPE_MAX];
	int num_pipe;
	int du_inited;
	struct g2d_monitor monitor;
};

struct g2d_ops {
	int (*init)(struct sdrv_g2d *);
	int (*enable)(struct sdrv_g2d*, int);
	int (*reset)(struct sdrv_g2d *);
	int (*mlc_set)(struct sdrv_g2d *, int , struct g2d_input *);
	int (*fill_rect)(struct sdrv_g2d *, struct g2d_bg_cfg *, struct g2d_output_cfg *);
	int (*fastcopy)(struct sdrv_g2d *, addr_t , u32 , u32 , u32 , addr_t , u32);
	int (*config)(struct sdrv_g2d *);
	int (*irq_handler)(struct sdrv_g2d *);
	int (*rwdma)(struct sdrv_g2d *, struct g2d_input *);
	void (*close_fastcopy)(struct sdrv_g2d *);
	int (*wpipe_set)(struct sdrv_g2d *, int, struct g2d_output_cfg *);
	int (*check_stroke)(struct g2d_input *);
	int (*scaler_coef_set)(struct sdrv_g2d *, struct g2d_coeff_table *);
};

struct sdrv_g2d_data {
	const char *version;
	const struct g2d_ops* ops;
};

struct ops_entry {
	const char *ver;
	void *ops;
};

int g2d_get_capability(struct g2d_capability *cap);
unsigned int get_compval_from_comp(struct pix_g2dcomp *comp);
unsigned int get_frm_ctrl_from_comp(struct pix_g2dcomp *comp);
int sdrv_wpipe_pix_comp(uint32_t format, struct pix_g2dcomp *comp);
int sdrv_pix_comp(uint32_t format, struct pix_g2dcomp *comp);
bool g2d_format_is_yuv(uint32_t format);
int g2d_format_wpipe_bypass(uint32_t format);


struct ops_list {
	struct list_head head;
	struct ops_entry *entry;
};
extern struct list_head g2d_pipe_list_head;

int g2d_ops_register(struct ops_entry *entry, struct list_head *head);
void *g2d_ops_attach(const char *str, struct list_head *head);
#define g2d_pipe_ops_register(entry)	g2d_ops_register(entry, &g2d_pipe_list_head)
#define g2d_pipe_ops_attach(str)	g2d_ops_attach(str, &g2d_pipe_list_head)
int g2d_choose_pipe(struct sdrv_g2d *gd, int hwid, int type, uint32_t offset);
struct sdrv_g2d *get_g2d_by_id(int id);
extern struct ops_entry gpipe_mid_g2d_entry;
extern struct ops_entry gpipe_high_g2d_entry;
extern struct ops_entry spipe_g2d_entry;



#endif //__SDRV_G2D_H__
