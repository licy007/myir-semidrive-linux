/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/spinlock_types.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/types.h>

#define SDRV_DDR_PFM_IO_CMD         0xB0

#define SDRV_DDR_PFM_IO_CMD_GET_DATA                       _IO(SDRV_DDR_PFM_IO_CMD, 1)
#define SDRV_DDR_PFM_IO_CMD_SET_TYPE                       _IO(SDRV_DDR_PFM_IO_CMD, 2)
#define SDRV_DDR_PFM_IO_CMD_GET_GROUP_INFO                 _IO(SDRV_DDR_PFM_IO_CMD, 3)
#define SDRV_DDR_PFM_IO_CMD_TRIG_START                     _IO(SDRV_DDR_PFM_IO_CMD, 4)
#define SDRV_DDR_PFM_IO_CMD_TRIG_STOP                      _IO(SDRV_DDR_PFM_IO_CMD, 5)
#define SDRV_DDR_PFM_IO_CMD_TRIG_INIT_RUN                  _IO(SDRV_DDR_PFM_IO_CMD, 6)

#define PFM_RECORD_MASTER_ID_NUM_MAX 16
#define PFM_MASTER_ID_NUM_MAX 128
#define PFM_MASTER_NAME_LEN_MAX 16

/*window time 1s = 1000 ms*/
#define PFM_WINDOW_TIME_MS 1000

/*time window 20ms, 1s 50 records, 60s 3000 records, 10 min 30000 reocrds, can be change by custome*/
#define PFM_RECORD_ON_DDR_WINDOW_TIME_MS 20
#define PFM_RECORD_ON_DDR_ITEM_NUM 30000

#define PFM_MON_BASE_CNT_CTL       0x0
#define PFM_MON_BASE_CNT_CMP       0x4

#define PFM_MON_MISC_ST            0xA0
#define PFM_MON_MISC_INT_CTL       0xA4
#define PFM_MON_AXI_CNT_CTL0       0x100
#define PFM_MON_AXI_CNT_CTL1       0x104

#define PFM_MON_AXI_RD_THR(i)      0x110 + i*16
#define PFM_MON_AXI_WR_THR(i)      0x114 + i*16
#define PFM_MON_AXI_RD_BCNT_THR(i) 0x118 + i*16
#define PFM_MON_AXI_WR_BCNT_THR(i) 0x11C + i*16

#define PFM_MON_AXI_ID0(i)         0x200 + i*16
#define PFM_MON_AXI_ID0_MSK(i)     0x204 + i*16
#define PFM_MON_AXI_ID1(i)         0x208 + i*16
#define PFM_MON_AXI_ID1_MSK(i)     0x20C + i*16

#define PFM_MON_AXI_RND_CNT        0x1000
#define PFM_MON_AXI_CNT_ST         0x1004

#define PFM_MON_AXI_RD_CNT(i)      0x1010 + i*24
#define PFM_MON_AXI_WR_CNT(i)      0x1014 + i*24
#define PFM_MON_AXI_RD_BCNTL(i)    0x1018 + i*24
#define PFM_MON_AXI_RD_BCNTH(i)    0x101C + i*24
#define PFM_MON_AXI_WR_BCNTL(i)    0x1020 + i*24
#define PFM_MON_AXI_WR_BCNTH(i)    0x1024 + i*24

#define PFM_MON_GROUP                    PFM_MON_AXI_CNT_CTL0

#define PFM_MON_ENABLE_SHIFT             (0U)
#define PFM_MON_ENABLE_MASK              (uint32_t)(1 << PFM_MON_ENABLE_SHIFT)

#define PFM_MON_CLEAR_REG_SHIFT          (1U)
#define PFM_MON_CLEAR_REG_MASK           (uint32_t)(1 << PFM_MON_CLEAR_REG_SHIFT)

#define PFM_MON_BASE_CNT_TIMEOUT_SHIFT   (4U)
#define PFM_MON_BASE_CNT_TIMEOUT_MASK    (uint32_t)(1 << PFM_MON_BASE_CNT_TIMEOUT_SHIFT)

#define PFM_MON_GROUP_SHIFT              (16U)
#define PFM_MON_GROUP_MSK                (uint32_t)(1 << PFM_MON_GROUP_SHIFT - 1)

#define PFM_MON_REACH_THR_SHIFT          (8U)
#define PFM_MON_REACH_THR_MSK            (uint32_t)(1 << PFM_MON_REACH_THR_SHIFT - 1)

#define PFM_MON_BCNTH_SHIFT              (8u)
#define PFM_MON_BCNTH_MSK                (uint32_t)(1 << PFM_MON_BCNTH_SHIFT - 1)

/* DDR_4266 */
#define time_window_per_ms_4266 1066500

/* DDR_3200 */
#define time_window_per_ms_3200 799875

/* DDR_2133 */
#define time_window_per_ms_2133 533250

/*this master id is defined by soc, can not change*/
enum pfm_master_id {
	SAF_PLATFORM,
	SEC_PLATFORM,
	MP_PLATFORM,
	AP1,
	AP2,
	VDSP,
	ADSP,
	RESERVED1,
	DMA1,
	DMA2,
	DMA3,
	DMA4,
	DMA5,
	DMA6,
	DMA7,
	DMA8,
	CSI1,
	CSI2,
	CSI3,
	DC1,
	DC2,
	DC3,
	DC4,
	DP1,
	DP2,
	DP3,
	DC5,
	G2D1,
	G2D2,
	VPU1,
	VPU2,
	MJPEG,
	MSHC1,
	MSHC2,
	MSHC3,
	MSHC4,
	ENET_QOS1,
	ENET_QOS2,
	USB1,
	USB2,
	AI,
	RESERVED2,
	RESERVED3,
	RESERVED4,
	RESERVED5,
	RESERVED6,
	RESERVED7,
	CE1,
	CE2_VCE1,
	CE2_VCE2,
	CE2_VCE3,
	CE2_VCE4,
	CE2_VCE5,
	CE2_VCE6,
	CE2_VCE7,
	CE2_VCE8,
	GPU1_OS1,
	GPU1_OS2,
	GPU1_OS3,
	GPU1_OS4,
	GPU1_OS5,
	GPU1_OS6,
	GPU1_OS7,
	GPU1_OS8,
	GPU2_OS1,
	GPU2_OS2,
	GPU2_OS3,
	GPU2_OS4,
	GPU2_OS5,
	GPU2_OS6,
	GPU2_OS7,
	GPU2_OS8,
	PTB,
	CSSYS,
	RESERVED8,
	RESERVED9,
	RESERVED10,
	RESERVED11,
	RESERVED12,
	RESERVED13,
	PCIE1_0,
	PCIE1_1,
	PCIE1_2,
	PCIE1_3,
	PCIE1_4,
	PCIE1_5,
	PCIE1_6,
	PCIE1_7,
	PCIE1_8,
	PCIE1_9,
	PCIE1_10,
	PCIE1_11,
	PCIE1_12,
	PCIE1_13,
	PCIE1_14,
	PCIE1_15,
	PCIE2_0,
	PCIE2_1,
	PCIE2_2,
	PCIE2_3,
	PCIE2_4,
	PCIE2_5,
	PCIE2_6,
	PCIE2_7,
	RESERVED14,
	RESERVED15,
	RESERVED16,
	RESERVED17,
	RESERVED18,
	RESERVED19,
	RESERVED20,
	RESERVED21,
	RESERVED22,
	RESERVED23,
	RESERVED24,
	RESERVED25,
	RESERVED26,
	RESERVED27,
	RESERVED28,
	RESERVED29,
	RESERVED30,
	RESERVED31,
	RESERVED32,
	RESERVED33,
	RESERVED34,
	RESERVED35,
	RESERVED36,
	RESERVED37
};

enum pfm_ddr_freq_type {
	PFM_DDR_FREQ_2133,
	PFM_DDR_FREQ_3200,
	PFM_DDR_FREQ_4266,
	PFM_DDR_FREQ_TYPE_MAX,
};

/*master name follow pfm_master_id*/
const char *master_name[RESERVED37] ={
	"all", "", "", "ap1", "ap2", "vdsp", "adsp", "",
	"dma1", "dma2", "dma3", "dma4", "dma5", "dma6", "dma7", "dma8",
	"csi1", "csi2", "csi3", "dc1", "dc2", "dc3", "dc4", "dp1",
	"dp2", "dp3", "dc5", "g2d1", "g2d2", "vpu1", "vpu2", "mjpeg",
	"mshc1", "mshc2", "mshc3", "mshc4", "enet1", "enet2", "usb1", "usb2",
	"ai", "", "", "", "", "", "", "ce1",
	"vce1", "vce2", "vce3", "vce4", "vce5", "vce6", "vce7", "vce8",
	"gpu1_os1", "gpu1_os2", "gpu1_os3", "gpu1_os4", "gpu1_os5", "gpu1_os6",
	    "gpu1_os7", "gpu1_os8",
	"gpu2_os1", "gpu2_os2", "gpu2_os3", "gpu2_os4", "gpu2_os5", "gpu2_os6",
	    "gpu2_os7", "gpu2_os8",
	"ptb", "cssys", "", "", "", "", "", "",
	"pcie1_0", "pcie1_1", "pcie1_2", "pcie1_3", "pcie1_4", "pcie1_5",
	    "pcie1_6", "pcie1_7",
	"pcie1_8", "pcie1_9", "pcie1_10", "pcie1_11", "pcie1_12", "pcie1_13",
	    "pcie1_14", "pcie1_15",
	"pcie2_0", "pcie2_1", "pcie2_2", "pcie2_3", "pcie2_4", "pcie2_5",
	    "pcie2_6", "pcie2_7",
	"", "", ""
};

/*master name follow pfm_master_id*/
uint32_t pfm_time_window[PFM_DDR_FREQ_TYPE_MAX] = {
	time_window_per_ms_2133,
	time_window_per_ms_3200,
	time_window_per_ms_4266
};

typedef struct pfm_observer {
	uint32_t master0;
	uint32_t msk0;
	uint32_t master1;
	uint32_t msk1;
} pfm_observer_t;

typedef struct pfm_stop_condition {
	uint32_t rd_thr;
	uint32_t wr_thr;
	uint32_t rd_bcnt_thr;
	uint32_t wr_bcnt_thr;
} pfm_stop_condition_t;

typedef struct pfm_head {
	uint32_t magic;
	uint32_t start_time;
	uint32_t time_window;
	uint32_t group_nr;
	pfm_observer_t observer_config[PFM_RECORD_MASTER_ID_NUM_MAX];
	pfm_stop_condition_t condition[8];
	uint32_t mode;
	uint32_t rounds;
	uint32_t pool_size;
	uint32_t record_offset;
	uint32_t reach_thr;
} pfm_head_t;

struct pfm_record_irq {
	uint32_t rd_bcntl;
	uint32_t rd_bcnth;
	uint32_t wr_bcntl;
	uint32_t wr_bcnth;
};

struct pfm_out_print_group_info {
	uint32_t rd_cnt;
	uint32_t wr_cnt;
	uint32_t rd_bcntl;
	uint32_t wr_bcntl;
	uint32_t rd_bcnth;
	uint32_t wr_bcnth;
};

struct pfm_out_print_info {
	uint32_t is_fill;
	uint32_t write_index;
	struct pfm_out_print_group_info group_inf[PFM_RECORD_MASTER_ID_NUM_MAX];
};

struct pfm_group_info {
	uint32_t is_cust;
	uint32_t group_num;
	pfm_observer_t observer[PFM_RECORD_MASTER_ID_NUM_MAX];
};

struct pfm_ddr_record_item {
	struct pfm_out_print_group_info group_inf[PFM_RECORD_MASTER_ID_NUM_MAX];
};

struct pfm_ddr_record_info {
	uint32_t is_fill;
	uint32_t write_index;
	struct pfm_ddr_record_item *record_item;
};

struct sdrv_ddr_pfm_dev {
	int is_init;
	struct device *dev;
	char name_buff[32];
	spinlock_t lock;
	struct miscdevice miscdev;
	struct pfm_group_info *group;
	struct pfm_ddr_record_info *info_record;
	int dma_condition;
	int irq;
	uint32_t ddr_freq;
	uint32_t time_window;
	uint32_t is_record_on_ddr;
	void *record_addr;
	dma_addr_t record_dma_addr;
	void __iomem *base;
	void __iomem *ram_base;
	struct class pfm_class;
	atomic_t irq_refcount;
};

struct pfm_out_print_info out_info;
struct pfm_out_print_info proc_pfm_info;

struct pfm_ddr_record_info info_record;

struct pfm_group_info group_info = {
	0, 16, {
		{AP1, 0x7f, AP1, 0x7f},	/* AP 3 */
		{AP2, 0x7f, AP2, 0x7f},	/* AP 4 */
		{GPU1_OS1, 0x78, GPU1_OS1, 0x78},	/* GPU1 56 */
		{GPU2_OS1, 0x78, GPU2_OS1, 0x78},	/* GPU2 64 */
		{VPU1, 0x7f, VPU1, 0x7f},	/* VPU1 29 */
		{VPU2, 0x7f, VPU2, 0x7f},	/* VPU2 30 */
		{DP1, 0x7f, DP1, 0x7f},	/* DP1 23 */
		{DP2, 0x7f, DP2, 0x7f},	/* DP2 24 */
		{DP3, 0x7f, DP3, 0x7f},	/* DP3 25 */
		{DC1, 0x7f, DC1, 0x7f},	/* DC1 19 */
		{DC2, 0x7f, DC2, 0x7f},	/* DC2 20 */
		{DC3, 0x7f, DC3, 0x7f},	/* DC3 21 */
		{DC4, 0x7f, DC4, 0x7f},	/* DC4 22 */
		{CSI1, 0x7e, CSI3, 0x7f},	/* CSI 16,18 */
		{VDSP, 0x7f, VDSP, 0x7f},	/* VDSP 5 */
		{0, 0, 0, 0}	/* ALL */
		}
};

#define SDRV_DDR_BIT_MASK(x) (((x) >= sizeof(unsigned long) * 8) ? (0UL-1) : ((1UL << (x)) - 1))
#define to_ddr_pfm_dev(priv) container_of((priv), struct sdrv_ddr_pfm_dev, miscdev)

static void ddr_pfm_disable_counter(struct sdrv_ddr_pfm_dev *pfm)
{
	/* Clear base counter */
	writel(2, (pfm->base + PFM_MON_BASE_CNT_CTL));
	/* disable base counter */
	writel(0, (pfm->base + PFM_MON_BASE_CNT_CTL));
}

static void ddr_pfm_enable_counter(struct sdrv_ddr_pfm_dev *pfm)
{
	/* Clear base counter */
	writel(2, (pfm->base + PFM_MON_BASE_CNT_CTL));
	/* Enable base counter */
	writel(1, (pfm->base + PFM_MON_BASE_CNT_CTL));
}

static void ddr_pfm_update_time_window(struct sdrv_ddr_pfm_dev *pfm)
{
	uint32_t ddr_freq;

	/*set time window: get time window per ms and get time set */
	if (pfm->ddr_freq >= PFM_DDR_FREQ_TYPE_MAX) {
		ddr_freq = PFM_DDR_FREQ_3200;
	} else {
		ddr_freq = pfm->ddr_freq;
	}

	writel(pfm_time_window[ddr_freq] * (pfm->time_window),
	       (pfm->base + PFM_MON_BASE_CNT_CMP));
}

static uint32_t ddr_pfm_master_observer(struct sdrv_ddr_pfm_dev *pfm,
					uint32_t group,
					pfm_observer_t * observer)
{
	if ((group > PFM_RECORD_MASTER_ID_NUM_MAX) || (observer == NULL)) {
		return -1;
	}

	writel((observer->master0) << 6, (pfm->base + PFM_MON_AXI_ID0(group)));
	writel((observer->msk0) << 6, (pfm->base + PFM_MON_AXI_ID0_MSK(group)));

	writel((observer->master1) << 6, (pfm->base + PFM_MON_AXI_ID1(group)));
	writel((observer->msk1) << 6, (pfm->base + PFM_MON_AXI_ID1_MSK(group)));

	return 0;
}

static void ddr_pfm_irq_enable(struct sdrv_ddr_pfm_dev *pfm)
{
	if (atomic_read(&pfm->irq_refcount) == 0)
		enable_irq(pfm->irq);

	atomic_inc(&pfm->irq_refcount);
}

static void ddr_pfm_irq_disable(struct sdrv_ddr_pfm_dev *pfm)
{
	if (atomic_sub_and_test(1, &pfm->irq_refcount))
		disable_irq(pfm->irq);
}

static uint32_t ddr_pfm_stop_trigger(struct sdrv_ddr_pfm_dev *pfm,
				     uint32_t group,
				     pfm_stop_condition_t * condition)
{
	if ((group > PFM_RECORD_MASTER_ID_NUM_MAX) || (condition == NULL)) {
		return -1;
	}

	writel(condition->rd_thr, (pfm->base + PFM_MON_AXI_RD_THR(group)));
	writel(condition->wr_thr, (pfm->base + PFM_MON_AXI_WR_THR(group)));

	writel(condition->rd_bcnt_thr,
	       (pfm->base + PFM_MON_AXI_RD_BCNT_THR(group)));
	writel(condition->wr_bcnt_thr,
	       (pfm->base + PFM_MON_AXI_WR_BCNT_THR(group)));

	return 0;
}

static uint32_t ddr_pfm_init(struct sdrv_ddr_pfm_dev *pfm)
{
	int ret;
	uint32_t group_num;
	pfm_stop_condition_t condition;
	uint32_t i;
	uint32_t ddr_freq;

	/*set time window: get time window per ms and get time set */
	if (pfm->ddr_freq >= PFM_DDR_FREQ_TYPE_MAX) {
		ddr_freq = PFM_DDR_FREQ_3200;
	} else {
		ddr_freq = pfm->ddr_freq;
	}

	group_num = pfm->group->group_num;

	writel(pfm_time_window[ddr_freq] * (pfm->time_window),
	       (pfm->base + PFM_MON_BASE_CNT_CMP));

	/*set pfm group num */
	writel(SDRV_DDR_BIT_MASK(group_num), (pfm->base + PFM_MON_GROUP));

	/*set pfm observer */
	for (i = 0; i < group_num; i++) {
		ddr_pfm_master_observer(pfm, i, &pfm->group->observer[i]);
	}

	/*set pfm stop trigger value */
	condition.rd_thr = 0xffffffff;
	condition.wr_thr = 0xffffffff;
	condition.rd_bcnt_thr = 0xffffffff;
	condition.wr_bcnt_thr = 0xffffffff;

	for (i = 0; i < 8; i++) {
		ddr_pfm_stop_trigger(pfm, i, &condition);
	}

	/*set enable mode pfm_enable_irq_mode */
	/* Enable base counter timeout irq && auto restart */
	ret = readl((pfm->base + PFM_MON_AXI_CNT_CTL1));
	ret |= 2;
	writel(ret, (pfm->base + PFM_MON_AXI_CNT_CTL1));
	/* Enable timeout irq  */
	ret = readl((pfm->base + PFM_MON_MISC_INT_CTL));
	ret |= 1;
	writel(ret, (pfm->base + PFM_MON_MISC_INT_CTL));

	/*unmask int */
	enable_irq(pfm->irq);
	atomic_set(&pfm->irq_refcount, 1);
	/*enable counter */
	/* Clear base counter */
	writel(2, (pfm->base + PFM_MON_BASE_CNT_CTL));
	/* Enable base counter */
	writel(1, (pfm->base + PFM_MON_BASE_CNT_CTL));

	pfm->is_init = 1;
	return 0;
}

static int ddr_pfm_open(struct inode *inode, struct file *filp)
{
	pr_info("ddr_pfm_open enter ");

	return 0;
}

static int ddr_pfm_release(struct inode *inode, struct file *filp)
{
	pr_info("ddr_pfm_release enter ");

	return 0;
}

static ssize_t ddr_pfm_read(struct file *filp, char __user * userbuf,
			    size_t len, loff_t * f_pos)
{
	uint32_t i;
	uint32_t j;
	uint32_t k;
	int print_buff_len = 512;
	char print_value_buff[512];
	struct sdrv_ddr_pfm_dev *pfm;
	pfm = to_ddr_pfm_dev(filp->private_data);

	pr_info("ddr_pfm_read enter ");

	if (pfm->is_record_on_ddr == 1) {
		ddr_pfm_disable_counter(pfm);

		/*print all group value bw:bandwidth cv:cmd value */
		pr_emerg("start dump %d items, time window 20ms, bw(kB/s), cv(num/20ms)",
		     pfm->info_record->write_index);

		for (k = 0; k < pfm->info_record->write_index; k++) {
			j = 0;

			for (i = 0; i < group_info.group_num; i++) {
				j += snprintf(print_value_buff + j,
					      print_buff_len, "| %d/%d ",
					      (((pfm->info_record->
						 record_item[k].group_inf[i].
						 rd_bcntl) >> 10) +
					       ((pfm->info_record->
						 record_item[k].group_inf[i].
						 rd_bcnth & 0xff) << 22)) * 50,
					      (((pfm->info_record->
						 record_item[k].group_inf[i].
						 wr_bcntl) >> 10) +
					       ((pfm->info_record->
						 record_item[k].group_inf[i].
						 wr_bcnth & 0xff) << 22)) * 50);
			}

			snprintf(print_value_buff + j, print_buff_len, "|");
			pr_emerg("%s--", &print_value_buff);

			j = 0;

			for (i = 0; i < group_info.group_num; i++) {
				j += snprintf(print_value_buff + j,
					      print_buff_len, "| %d/%d ",
					      (pfm->info_record->record_item[k].
					       group_inf[i].rd_cnt),
					      (pfm->info_record->record_item[k].
					       group_inf[i].wr_cnt));
			}

			snprintf(print_value_buff + j, print_buff_len, "|\n");
			pr_emerg("%s \n", &print_value_buff);
		}

		pr_emerg("end dump %d items, time window 20ms, bw(kB/s), cv(num/20ms)",
		     pfm->info_record->write_index);

		pfm->info_record->is_fill = 0;
		pfm->info_record->write_index = 0;
		ddr_pfm_enable_counter(pfm);
	} else {
		pr_info("please echo 1 > /dev/semidrive-pfm1 open info record on ddr");
	}

	return 0;
}

static ssize_t ddr_pfm_write(struct file *filp, const char __user * userbuf,
			     size_t len, loff_t * f_pos)
{

	int rc = 0;
	uint8_t data_value[2];
	struct sdrv_ddr_pfm_dev *pfm;
	struct page *page;
	phys_addr_t paddr;
	pfm = to_ddr_pfm_dev(filp->private_data);

	pr_info("ddr_pfm_write enter ");

	copy_from_user(&data_value, userbuf, min(len, sizeof(data_value)));

	/*echo 1 value is 0x31 */
	if (data_value[0] == 0x31) {
		pr_info("ddr_pfm_write enter 1");
		ddr_pfm_disable_counter(pfm);
		/*malloc buff, if use dma mode, must on cma buff */
		//pfm->record_addr = dma_alloc_coherent(pfm->dev, (sizeof(struct pfm_ddr_record_item) * PFM_RECORD_ON_DDR_ITEM_NUM), &pfm->record_dma_addr, GFP_ATOMIC);

		page =
		    dma_alloc_from_contiguous(pfm->dev,
					      PAGE_ALIGN((sizeof
							  (struct
							   pfm_ddr_record_item)
							  *
							  PFM_RECORD_ON_DDR_ITEM_NUM))
					      >> PAGE_SHIFT,
					      get_order(PAGE_ALIGN
							(sizeof
							 (struct
							  pfm_ddr_record_item) *
							 PFM_RECORD_ON_DDR_ITEM_NUM)),
					      GFP_KERNEL);
		if (!page)
			return NULL;

		pfm->record_dma_addr =
		    phys_to_dma(pfm->dev, page_to_phys(page));
		pfm->record_addr = page_address(page);
		memset(pfm->record_addr, 0,
		       sizeof(struct pfm_ddr_record_item) *
		       PFM_RECORD_ON_DDR_ITEM_NUM);

		if (pfm->record_addr != NULL) {
			pfm->info_record->record_item =
			    (struct pfm_ddr_record_item *)(pfm->record_addr);
		} else {
			pr_info("pfm enable error");
			return len;
		}

		pfm->time_window = PFM_RECORD_ON_DDR_WINDOW_TIME_MS;
		pfm->is_record_on_ddr = 1;
		pfm->info_record->is_fill = 0;
		ddr_pfm_update_time_window(pfm);
		ddr_pfm_enable_counter(pfm);
	} else if (data_value[0] == 0x30) {
		ddr_pfm_disable_counter(pfm);

		if (pfm->record_addr != NULL) {
			//dma_free_coherent(pfm->dev, (sizeof(struct pfm_ddr_record_item) * PFM_RECORD_ON_DDR_ITEM_NUM), pfm->record_addr, pfm->record_dma_addr);
			paddr = dma_to_phys(pfm->dev, pfm->record_dma_addr);
			dma_release_from_contiguous(pfm->dev,
						    phys_to_page(paddr),
						    PAGE_ALIGN((sizeof
								(struct
								 pfm_ddr_record_item)
								*
								PFM_RECORD_ON_DDR_ITEM_NUM))
						    >> PAGE_SHIFT);
		}

		pfm->time_window = PFM_RECORD_ON_DDR_ITEM_NUM;
		pfm->is_record_on_ddr = 0;
		ddr_pfm_update_time_window(pfm);
		ddr_pfm_enable_counter(pfm);
	} else {

	}

	return len;
}

static long ddr_pfm_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	long ret;
	struct sdrv_ddr_pfm_dev *pfm;
	struct pfm_out_print_info *info_temp = NULL;
	pfm = to_ddr_pfm_dev(filp->private_data);

	ret = -ENOTTY;

	switch (cmd) {
	case SDRV_DDR_PFM_IO_CMD_GET_DATA:
		if (out_info.is_fill == 1) {
			info_temp = &out_info;
		} else {
			info_temp = NULL;
		}

		if (info_temp != NULL) {
			if (copy_to_user
			    ((void *)arg, info_temp,
			     sizeof(struct pfm_out_print_info))) {
				pr_err("%s(CIOCGKEY) - bad return copy\n",
				       __FUNCTION__);
				ret = EFAULT;
			} else {
				ret = 0;
			}

			info_temp->is_fill = 0;
		}

		break;

	case SDRV_DDR_PFM_IO_CMD_GET_GROUP_INFO:
		if (copy_to_user
		    ((void *)arg, pfm->group, sizeof(struct pfm_group_info))) {
			pr_err("%s(CIOCGKEY) - bad return copy\n",
			       __FUNCTION__);
			ret = EFAULT;
		} else {
			ret = 0;
		}

		break;

	case SDRV_DDR_PFM_IO_CMD_TRIG_INIT_RUN:
		if (pfm->is_init == 0) {
			ddr_pfm_init(pfm);
			ret = 0;
		}

		break;

	case SDRV_DDR_PFM_IO_CMD_TRIG_START:
		ddr_pfm_irq_enable(pfm);
		ret = 0;
		break;

	case SDRV_DDR_PFM_IO_CMD_TRIG_STOP:
		ddr_pfm_irq_disable(pfm);
		ret = 0;
		break;

	default:
		pr_warn("%s: Unhandled ioctl cmd: 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations ddr_pfm_fops = {
	.open = ddr_pfm_open,
	.release = ddr_pfm_release,
	.unlocked_ioctl = ddr_pfm_ioctl,
	.read = ddr_pfm_read,
	.write = ddr_pfm_write,
	.owner = THIS_MODULE,
};

static ssize_t store_enable(struct class *class, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct sdrv_ddr_pfm_dev *pfm =
	    container_of(class, struct sdrv_ddr_pfm_dev, pfm_class);
	int val;
	int ret;

	if (pfm->is_init == 0) {
		ddr_pfm_init(pfm);
		return count;
	}

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return ret;

	if (val)
		ddr_pfm_irq_enable(pfm);
	else
		ddr_pfm_irq_disable(pfm);

	return count;
}

static ssize_t show_enable(struct class *class, struct class_attribute *attr,
			   char *buf)
{
	struct sdrv_ddr_pfm_dev *pfm =
	    container_of(class, struct sdrv_ddr_pfm_dev, pfm_class);

	if (atomic_read(&pfm->irq_refcount) > 0)
		return snprintf(buf, 8, "enable\n");
	else
		return snprintf(buf, 9, "disable\n");
}

static struct class_attribute pfm_class_attrs =
__ATTR(enable, S_IRUGO | S_IWUSR | S_IRUSR, show_enable, store_enable);

static struct attribute *pfm_attrs[] = {
	&pfm_class_attrs.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pfm);

static irqreturn_t sdrv_ddr_pfm_irq_handler(int irq, void *data)
{
	struct sdrv_ddr_pfm_dev *pfm = data;
	struct pfm_out_print_info *info_temp = NULL;
	int irq_value;
	int i;

	irq_value = readl((pfm->base + PFM_MON_MISC_ST)) & 0x10;

	if (irq_value != 0) {
		if (pfm->is_record_on_ddr == 1) {
			if (pfm->info_record->is_fill == 0) {
				for (i = 0; i < pfm->group->group_num; i++) {
					pfm->info_record->record_item[pfm->
								      info_record->
								      write_index].
					    group_inf[i].rd_cnt =
					    readl((pfm->base + PFM_MON_AXI_RD_CNT(i)));
					pfm->info_record->record_item[pfm->
								      info_record->
								      write_index].
					    group_inf[i].wr_cnt =
					    readl((pfm->base + PFM_MON_AXI_WR_CNT(i)));
					pfm->info_record->record_item[pfm->
								      info_record->
								      write_index].
					    group_inf[i].rd_bcntl =
					    readl((pfm->base + PFM_MON_AXI_RD_BCNTL(i)));
					pfm->info_record->record_item[pfm->
								      info_record->
								      write_index].
					    group_inf[i].wr_bcntl =
					    readl((pfm->base + PFM_MON_AXI_WR_BCNTL(i)));
					pfm->info_record->record_item[pfm->
								      info_record->
								      write_index].
					    group_inf[i].rd_bcnth =
					    readl((pfm->base + PFM_MON_AXI_RD_BCNTH(i)));
					pfm->info_record->record_item[pfm->
								      info_record->
								      write_index].
					    group_inf[i].wr_bcnth =
					    readl((pfm->base + PFM_MON_AXI_WR_BCNTH(i)));
				}

				pfm->info_record->write_index++;

				if (pfm->info_record->write_index ==
				    PFM_RECORD_ON_DDR_ITEM_NUM) {
					pfm->info_record->is_fill = 1;
					pfm->info_record->write_index = 0;
				}
			}
		} else {
			if (out_info.is_fill == 0) {
				info_temp = &out_info;
			} else {
				info_temp = NULL;
			}

			/*get group enable mask, group enable, then get cnt, this use default all group from 0 is enabled */
			/*default record to app get buff, if app get buff is not update, record in proc buff,for cat /proc/pfminfo print */
			/*if app buff is update on time use app buff when cat /proc/pfminfo, */
			/*app buff can not be changed when print, cat /proc/pfminfo buff can be changed when print */
			if (info_temp != NULL) {

				for (i = 0; i < pfm->group->group_num; i++) {
					info_temp->group_inf[i].rd_cnt =
					    readl((pfm->base + PFM_MON_AXI_RD_CNT(i)));
					info_temp->group_inf[i].wr_cnt =
					    readl((pfm->base + PFM_MON_AXI_WR_CNT(i)));
					info_temp->group_inf[i].rd_bcntl =
					    readl((pfm->base + PFM_MON_AXI_RD_BCNTL(i)));
					info_temp->group_inf[i].wr_bcntl =
					    readl((pfm->base + PFM_MON_AXI_WR_BCNTL(i)));
					info_temp->group_inf[i].rd_bcnth =
					    readl((pfm->base + PFM_MON_AXI_RD_BCNTH(i)));
					info_temp->group_inf[i].wr_bcnth =
					    readl((pfm->base + PFM_MON_AXI_WR_BCNTH(i)));
				}

				info_temp->is_fill = 1;
				proc_pfm_info.is_fill = 0;
			} else {

				for (i = 0; i < pfm->group->group_num; i++) {
					proc_pfm_info.group_inf[i].rd_cnt =
					    readl((pfm->base + PFM_MON_AXI_RD_CNT(i)));
					proc_pfm_info.group_inf[i].wr_cnt =
					    readl((pfm->base + PFM_MON_AXI_WR_CNT(i)));
					proc_pfm_info.group_inf[i].rd_bcntl =
					    readl((pfm->base + PFM_MON_AXI_RD_BCNTL(i)));
					proc_pfm_info.group_inf[i].wr_bcntl =
					    readl((pfm->base + PFM_MON_AXI_WR_BCNTL(i)));
					proc_pfm_info.group_inf[i].rd_bcnth =
					    readl((pfm->base + PFM_MON_AXI_RD_BCNTH(i)));
					proc_pfm_info.group_inf[i].wr_bcnth =
					    readl((pfm->base + PFM_MON_AXI_WR_BCNTH(i)));
				}

				proc_pfm_info.is_fill = 1;
			}
		}

		/*enable counter */
		/* Clear base counter */
		writel(2, (pfm->base + PFM_MON_BASE_CNT_CTL));
		/* Enable base counter */
		writel(1, (pfm->base + PFM_MON_BASE_CNT_CTL));
	}

	return IRQ_HANDLED;
}

static int sdrv_ddr_pfm_probe(struct platform_device *pdev)
{
	struct sdrv_ddr_pfm_dev *ddr_pfm_inst;
	struct miscdevice *ddr_pfm_fn;
	struct resource *ctrl_reg;
	void __iomem *base_temp;
	int ret;
	int irq;
	int num;
	int i;
	u32 val;
	u32 auto_run;

	pr_info("sdrv_ddr_pfm_probe enter");

	auto_run = 0;
	ret =
	    of_property_read_u32(pdev->dev.of_node, "pfm,auto-run", &auto_run);

	val = 0;
	ret = of_property_read_u32(pdev->dev.of_node, "time-window-mode", &val);

	if (ret < 0) {
		dev_err(&pdev->dev, "No time-window optinos configed in dts\n");
		return ret;
	} else {

		ctrl_reg =
		    platform_get_resource_byname(pdev, IORESOURCE_MEM,
						 "ctrl_reg");
		base_temp = devm_ioremap_resource(&pdev->dev, ctrl_reg);

		if (IS_ERR(base_temp)) {
			return PTR_ERR(base_temp);
		}

		irq = platform_get_irq(pdev, 0);
	}

	num = of_property_count_u32_elems(pdev->dev.of_node, "sdrv,group_info");

	if (num <= 0) {
		/*use default value defined in group_info */
		pr_info("sdrv_ddr_pfm_probe use default group info");
	} else {

		if (num > PFM_RECORD_MASTER_ID_NUM_MAX * 4) {
			num = PFM_RECORD_MASTER_ID_NUM_MAX * 4;
		}

		for (i = 0; i < num;) {
			of_property_read_u32_index(pdev->dev.of_node,
						   "sdrv,group_info", i,
						   &group_info.observer[i /
									4].
						   master0);
			i++;
			of_property_read_u32_index(pdev->dev.of_node,
						   "sdrv,group_info", i,
						   &group_info.observer[i /
									4].
						   msk0);
			i++;
			of_property_read_u32_index(pdev->dev.of_node,
						   "sdrv,group_info", i,
						   &group_info.observer[i /
									4].
						   master1);
			i++;
			of_property_read_u32_index(pdev->dev.of_node,
						   "sdrv,group_info", i,
						   &group_info.observer[i /
									4].
						   msk1);
			i++;
		}

		group_info.group_num = num / 4;
		group_info.is_cust = 1;
	}

	ddr_pfm_inst =
	    devm_kzalloc(&pdev->dev, sizeof(*ddr_pfm_inst), GFP_KERNEL);

	if (!ddr_pfm_inst) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ddr_pfm_inst);

	ddr_pfm_inst->base = base_temp;

	ddr_pfm_inst->dev = &pdev->dev;

	if (irq < 0) {
		dev_err(&pdev->dev, "Cannot get IRQ resource\n");
		return irq;
	}

	ddr_pfm_inst->irq = irq;
	ddr_pfm_inst->ddr_freq = val;

	ddr_pfm_inst->time_window = PFM_WINDOW_TIME_MS;

	ddr_pfm_fn = &ddr_pfm_inst->miscdev;
	sprintf(ddr_pfm_inst->name_buff, "semidrive-pfm%d", val);

	ddr_pfm_fn->minor = MISC_DYNAMIC_MINOR;
	ddr_pfm_fn->name = ddr_pfm_inst->name_buff;
	ddr_pfm_fn->fops = &ddr_pfm_fops;

	ret = misc_register(ddr_pfm_fn);

	if (ret) {
		dev_err(&pdev->dev, "failed to register sdrv ddr pfm\n");
		return ret;
	}

	ddr_pfm_inst->pfm_class.name = "pfm_class";
	ddr_pfm_inst->pfm_class.owner = THIS_MODULE;
	ddr_pfm_inst->pfm_class.class_groups = pfm_groups;
	ret = class_register(&(ddr_pfm_inst->pfm_class));
	if (ret < 0) {
		dev_err(&pdev->dev, "register pfm class failed: %d\n", ret);
		return ret;
	}

	pdev->dev.class = &(ddr_pfm_inst->pfm_class);

	ddr_pfm_inst->group = &group_info;
	ddr_pfm_inst->info_record = &info_record;

	ret =
	    devm_request_threaded_irq(&pdev->dev, irq, sdrv_ddr_pfm_irq_handler,
				      NULL, IRQF_ONESHOT, dev_name(&pdev->dev),
				      ddr_pfm_inst);

	/* Disable the IRQ, we'll enable it in open() */
	disable_irq(irq);
	atomic_set(&ddr_pfm_inst->irq_refcount, 0);

	/*auto run */
	if (auto_run == 1) {
		ddr_pfm_init(ddr_pfm_inst);
	}

	return 0;
}

static void sdrv_ddr_pfm_shutdown(struct platform_device *pdev)
{
	struct sdrv_ddr_pfm_dev *pfm = dev_get_drvdata(&pdev->dev);

	writel(0x0, (pfm->base + PFM_MON_AXI_CNT_CTL1));
	writel(0x0, (pfm->base + PFM_MON_MISC_INT_CTL));
	disable_irq(pfm->irq);
	ddr_pfm_disable_counter(pfm);
}

static const struct of_device_id sdrv_ddr_pfm_match[] = {
	{.compatible = "semidrive,pfm"},
	{}
};

MODULE_DEVICE_TABLE(of, sdrv_ddr_pfm_match);

static struct platform_driver sdrv_ddr_pfm_driver = {
	.probe = sdrv_ddr_pfm_probe,
	.driver = {
		   .name = "semidrive-pfm",
		   .of_match_table = of_match_ptr(sdrv_ddr_pfm_match),
		   },
	.shutdown = sdrv_ddr_pfm_shutdown,
};

module_platform_driver(sdrv_ddr_pfm_driver);

static int pfminfo_proc_show(struct seq_file *m, void *v)
{
	uint32_t i;
	struct pfm_out_print_info *proc_pfm_info_temp = NULL;

	if (proc_pfm_info.is_fill == 1) {
		proc_pfm_info_temp = &proc_pfm_info;
	} else {
		proc_pfm_info_temp = &out_info;
	}

	seq_printf(m, "ddr_perf_monitor:\n");
	seq_printf(m, "group_num:%d\n", group_info.group_num);
	seq_printf(m, "group_read_write_detail_info(kB):\n");

	for (i = 0; i < group_info.group_num; i++) {
		if (group_info.observer[i].master0 ==
		    group_info.observer[i].master1) {
			seq_printf(m, "%s: ",
				   master_name[group_info.observer[i].master0]);
		} else {
			seq_printf(m, "%s_%s: ",
				   master_name[group_info.observer[i].master0],
				   master_name[group_info.observer[i].master1]);
		}

		seq_printf(m, "%d %d \n",
			   (((proc_pfm_info_temp->group_inf[i].
			      rd_bcntl) >> 10) +
			    ((proc_pfm_info_temp->group_inf[i].
			      rd_bcnth & 0xff) << 22)),
			   (((proc_pfm_info_temp->group_inf[i].
			      wr_bcntl) >> 10) +
			    ((proc_pfm_info_temp->group_inf[i].
			      wr_bcnth & 0xff) << 22)));
	}

	return 0;
}

static int pfminfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pfminfo_proc_show, NULL);
}

static const struct file_operations pfminfo_proc_fops = {
	.open = pfminfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_pfminfo_init(void)
{
	proc_create("pfminfo", 0, NULL, &pfminfo_proc_fops);
	return 0;
}

fs_initcall(proc_pfminfo_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
MODULE_DESCRIPTION("semidrive ddr pfm driver");
