// SPDX-License-Identifier:  GPL-2.0
// (C) 2017-2018 Synopsys, Inc. (www.synopsys.com)

/*
 * Synopsys DesignWare AXI DMA Controller driver.
 *
 * Author: Eugeniy Paltsev <Eugeniy.Paltsev@synopsys.com>
 */

#ifndef _AXI_DMA_PLATFORM_H
#define _AXI_DMA_PLATFORM_H

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/types.h>

#include "../virt-dma.h"

#define DMAC_MAX_CHANNELS 8
#define DMAC_MAX_MASTERS 2
#define DMAC_MAX_BLK_SIZE 0x200000
/**
 * @brief designware axi dma hardware configuration
 *
 */
struct dw_axi_dma_hcfg {
	/*! number of channels */
	u32 nr_channels;
	u32 nr_masters;
	u32 m_data_width;
	/*! maximum block size of channels */
	u32 block_size[DMAC_MAX_CHANNELS];
	/*! priority of channel */
	u32 priority[DMAC_MAX_CHANNELS];
	/*! maximum supported axi burst length */
	u32 axi_rw_burst_len;
	bool restrict_axi_burst_len;
	/*! dma mux flag  */
	bool dma_mux_enable;
	/*! dmac abort*/
	bool dma_abort_enable;
};

/**
 * @brief local axi dma channel configration.
 *
 */
struct axi_dma_chan {
	struct axi_dma_chip *chip;
	void __iomem *chan_regs;
	void __iomem *chan_mux_regs;

	u8 id;
	atomic_t descs_allocated;

	struct virt_dma_chan vc;

	/*! these other elements are all protected by vc.lock */
	bool is_paused;
	/*!  Fixed slave id, true: slave id is allocated by of dma node, false:
	 * changed by dma_chan_config*/
	bool fix_slave_id;

	/*! axi dma transfer descriptor*/
	struct axi_dma_desc *desc;
	/*! dma transfer direction  */
	enum dma_transfer_direction direction;
	/*!  configuration passed via .device_config */
	struct dma_slave_config dma_sconfig;
};
/**
 * @brief dw axi dma controller
 *
 */
struct dw_axi_dma {
	struct dma_device dma;
	struct dw_axi_dma_hcfg *hdata;
	struct dma_pool *desc_pool;

	/* channels */
	struct axi_dma_chan *chan;
};
/**
 * @brief dw axi dma controller chip
 *
 */
struct axi_dma_chip {
	struct device *dev;
	int irq;
	void __iomem *regs;
	void __iomem *mux_regs;
	struct clk *core_clk;
	struct clk *cfgr_clk;
	struct dw_axi_dma *dw;
	/*add a spinlock for driver*/
	spinlock_t   dev_lock;
};

/* LLI == Linked List Item */
struct __packed axi_dma_lli {
	__le64 sar;
	__le64 dar;
	__le32 block_ts_lo;
	__le32 block_ts_hi;
	__le64 llp;
	__le32 ctl_lo;
	__le32 ctl_hi;
	__le32 sstat;
	__le32 dstat;
	__le32 status_lo;
	__le32 status_hi;
	__le32 reserved_lo;
	__le32 reserved_hi;
};
/**
 * @brief axi dma transfer descriptor
 *
 */
struct axi_dma_desc {
	struct axi_dma_lli lli;

	struct virt_dma_desc vd;
	struct axi_dma_chan *chan;
	struct list_head xfer_list;
	bool cyclic;
	/* the left descscriptors' residue */
	u32 residue;
	/* desc len*/
	size_t len;
	/* total len */
	size_t total_len;
};

static inline struct device *dchan2dev(struct dma_chan *dchan)
{
	return &dchan->dev->device;
}

static inline struct device *chan2dev(struct axi_dma_chan *chan)
{
	return &chan->vc.chan.dev->device;
}

static inline struct axi_dma_desc *vd_to_axi_desc(struct virt_dma_desc *vd)
{
	return container_of(vd, struct axi_dma_desc, vd);
}

static inline struct axi_dma_chan *vc_to_axi_dma_chan(struct virt_dma_chan *vc)
{
	return container_of(vc, struct axi_dma_chan, vc);
}

static inline struct axi_dma_chan *dchan_to_axi_dma_chan(struct dma_chan *dchan)
{
	return vc_to_axi_dma_chan(to_virt_chan(dchan));
}

#define COMMON_REG_LEN 0x100
#define CHAN_REG_LEN 0x100

#define CHAN_MUX_REG_LEN 0x4

/**
 * @brief Common registers offset
 *
 */
#define DMAC_ID 0x000			/* R DMAC ID */
#define DMAC_COMPVER 0x008		/* R DMAC Component Version */
#define DMAC_CFG 0x010			/* R/W DMAC Configuration */
#define DMAC_CHEN 0x018			/* R/W DMAC Channel Enable */
#define DMAC_CHEN_L 0x018		/* R/W DMAC Channel Enable 00-31 */
#define DMAC_CHEN_H 0x01C		/* R/W DMAC Channel Enable 32-63 */
#define DMAC_INTSTATUS 0x030		/* R DMAC Interrupt Status */
#define DMAC_COMMON_INTCLEAR 0x038	/* W DMAC Interrupt Clear */
#define DMAC_COMMON_INTSTATUS_ENA 0x040 /* R DMAC Interrupt Status Enable */
#define DMAC_COMMON_INTSIGNAL_ENA 0x048 /* R/W DMAC Interrupt Signal Enable */
#define DMAC_COMMON_INTSTATUS 0x050	/* R DMAC Interrupt Status */
#define DMAC_RESET 0x058		/* R DMAC Reset Register1 */

/**
 * @brief DMA channel registers offset
 *
 */
#define CH_SAR 0x000		   /* R/W Chan Source Address */
#define CH_DAR 0x008		   /* R/W Chan Destination Address */
#define CH_BLOCK_TS 0x010	   /* R/W Chan Block Transfer Size */
#define CH_CTL 0x018		   /* R/W Chan Control */
#define CH_CTL_L 0x018		   /* R/W Chan Control 00-31 */
#define CH_CTL_H 0x01C		   /* R/W Chan Control 32-63 */
#define CH_CFG 0x020		   /* R/W Chan Configuration */
#define CH_CFG_L 0x020		   /* R/W Chan Configuration 00-31 */
#define CH_CFG_H 0x024		   /* R/W Chan Configuration 32-63 */
#define CH_LLP 0x028		   /* R/W Chan Linked List Pointer */
#define CH_STATUS 0x030		   /* R Chan Status */
#define CH_SWHSSRC 0x038	   /* R/W Chan SW Handshake Source */
#define CH_SWHSDST 0x040	   /* R/W Chan SW Handshake Destination */
#define CH_BLK_TFR_RESUMEREQ 0x048 /* W Chan Block Transfer Resume Req */
#define CH_AXI_ID 0x050		   /* R/W Chan AXI ID */
#define CH_AXI_QOS 0x058	   /* R/W Chan AXI QOS */
#define CH_SSTAT 0x060		   /* R Chan Source Status */
#define CH_DSTAT 0x068		   /* R Chan Destination Status */
#define CH_SSTATAR 0x070	   /* R/W Chan Source Status Fetch Addr */
#define CH_DSTATAR 0x078	   /* R/W Chan Destination Status Fetch Addr */
#define CH_INTSTATUS_ENA 0x080	   /* R/W Chan Interrupt Status Enable */
#define CH_INTSTATUS 0x088	   /* R/W Chan Interrupt Status */
#define CH_INTSIGNAL_ENA 0x090	   /* R/W Chan Interrupt Signal Enable */
#define CH_INTCLEAR 0x098	   /* W Chan Interrupt Clear */

/**
 * @brief DMA mux registers and offset.
 *
 */
#define DMAC_CHAN_MUX (0x0)
#define DMAC_MUX_WR_HDSK_SHIFT 0
#define DMAC_MUX_UPDATED_WR_HDSK_SHIFT 8
#define DMAC_MUX_RD_HDSK_SHIFT 16
#define DMAC_MUX_UPDATED_RD_HDSK_SHIFT 24

#define DMAC_MUX_EN (0x4 * 0x8)
#define DMAC_MUX_EN_POS 0
#define DMAC_MUX_EN_MASK BIT(DMAC_MUX_EN_POS)

/* DMAC_CFG */
#define DMAC_EN_POS 0
#define DMAC_EN_MASK BIT(DMAC_EN_POS)

#define INT_EN_POS 1
#define INT_EN_MASK BIT(INT_EN_POS)

#define DMAC_CHAN_EN_SHIFT 0
#define DMAC_CHAN_EN_WE_SHIFT 8

#define DMAC_CHAN_SUSP_SHIFT 16
#define DMAC_CHAN_SUSP_WE_SHIFT 24

#define DMA_MUX_EXTRA_HS_PORT 255

/* CH_CTL_H */
#define CH_CTL_H_ARLEN_EN BIT(6)
#define CH_CTL_H_ARLEN_POS 7
#define CH_CTL_H_AWLEN_EN BIT(15)
#define CH_CTL_H_AWLEN_POS 16

enum {
	DWAXIDMAC_ARWLEN_1 = 0,
	DWAXIDMAC_ARWLEN_2 = 1,
	DWAXIDMAC_ARWLEN_4 = 3,
	DWAXIDMAC_ARWLEN_8 = 7,
	DWAXIDMAC_ARWLEN_16 = 15,
	DWAXIDMAC_ARWLEN_32 = 31,
	DWAXIDMAC_ARWLEN_64 = 63,
	DWAXIDMAC_ARWLEN_128 = 127,
	DWAXIDMAC_ARWLEN_256 = 255,
	DWAXIDMAC_ARWLEN_MIN = DWAXIDMAC_ARWLEN_1,
	DWAXIDMAC_ARWLEN_MAX = DWAXIDMAC_ARWLEN_256
};

#define CH_CTL_H_SRC_STAT_EN BIT(24)
#define CH_CTL_H_DST_STAT_EN BIT(25)
#define CH_CTL_H_IOC_BLK_TFR BIT(26)
#define CH_CTL_H_LLI_LAST BIT(30)
#define CH_CTL_H_LLI_VALID BIT(31)
#define CH_CTL_H_IOC_BLK_TFR BIT(26)

/* CH_CTL_L */
#define CH_CTL_L_LAST_WRITE_EN BIT(30)

#define CH_CTL_L_DST_MSIZE_POS 18
#define CH_CTL_L_SRC_MSIZE_POS 14

enum {
	DWAXIDMAC_BURST_TRANS_LEN_1 = 0,
	DWAXIDMAC_BURST_TRANS_LEN_4,
	DWAXIDMAC_BURST_TRANS_LEN_8,
	DWAXIDMAC_BURST_TRANS_LEN_16,
	DWAXIDMAC_BURST_TRANS_LEN_32,
	DWAXIDMAC_BURST_TRANS_LEN_64,
	DWAXIDMAC_BURST_TRANS_LEN_128,
	DWAXIDMAC_BURST_TRANS_LEN_256,
	DWAXIDMAC_BURST_TRANS_LEN_512,
	DWAXIDMAC_BURST_TRANS_LEN_1024

};

#define CH_CTL_L_DST_WIDTH_POS 11
#define CH_CTL_L_SRC_WIDTH_POS 8

#define CH_CTL_L_DST_INC_POS 6
#define CH_CTL_L_SRC_INC_POS 4
enum {
	DWAXIDMAC_CH_CTL_L_INC = 0,
	DWAXIDMAC_CH_CTL_L_NOINC
};

#define CH_CTL_L_DST_MAST BIT(2)
#define CH_CTL_L_SRC_MAST BIT(0)

/* CH_CFG_H */
#define CH_CFG_H_DST_OSR_LMT_POS 27
#define CH_CFG_H_SRC_OSR_LMT_POS 23

#define CH_CFG_H_PRIORITY_POS 17

#define CH_CFG_H_DST_PER_POS 12
#define CH_CFG_H_SRC_PER_POS 7

#define CH_CFG_H_HS_SEL_DST_POS 4
#define CH_CFG_H_HS_SEL_SRC_POS 3

#define CH_CFG_H_TT_FC_POS 0

enum {
	DWAXIDMAC_HS_SEL_HW = 0,
	DWAXIDMAC_HS_SEL_SW
};


enum {
	DWAXIDMAC_TT_FC_MEM_TO_MEM_DMAC = 0,
	DWAXIDMAC_TT_FC_MEM_TO_PER_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_MEM_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_PER_DMAC,

};

/* CH_CFG_L */
#define CH_CFG_L_DST_MULTBLK_TYPE_POS 2
#define CH_CFG_L_SRC_MULTBLK_TYPE_POS 0

enum {
	DWAXIDMAC_MBLK_TYPE_CONTIGUOUS = 0,
	DWAXIDMAC_MBLK_TYPE_RELOAD,
	DWAXIDMAC_MBLK_TYPE_SHADOW_REG,
	DWAXIDMAC_MBLK_TYPE_LL
};

/**
 * @brief DW AXI DMA channel interrupts
 *
 */
enum {
	/*! Bitmask of no one interrupt */
	DWAXIDMAC_IRQ_NONE = 0,
	/*! Block transfer complete */
	DWAXIDMAC_IRQ_BLOCK_TRF = BIT(0),
	/*! Dma transfer complete */
	DWAXIDMAC_IRQ_DMA_TRF = BIT(1),
	/*! Source transaction complete */
	DWAXIDMAC_IRQ_SRC_TRAN = BIT(3),
	/*! Destination transaction complete*/
	DWAXIDMAC_IRQ_DST_TRAN = BIT(4),
	/*! Source decode error*/
	DWAXIDMAC_IRQ_SRC_DEC_ERR = BIT(5),
	/*! Destination decode error*/
	DWAXIDMAC_IRQ_DST_DEC_ERR = BIT(6),
	/*! Source slave error*/
	DWAXIDMAC_IRQ_SRC_SLV_ERR = BIT(7),
	/*! Destination slave error*/
	DWAXIDMAC_IRQ_DST_SLV_ERR = BIT(8),
	/*! LLI read decode error*/
	DWAXIDMAC_IRQ_LLI_RD_DEC_ERR = BIT(9),
	/*! LLI write decode error*/
	DWAXIDMAC_IRQ_LLI_WR_DEC_ERR = BIT(10),
	/*! LLI read slave error*/
	DWAXIDMAC_IRQ_LLI_RD_SLV_ERR = BIT(11),
	/*! LLI write slave error*/
	DWAXIDMAC_IRQ_LLI_WR_SLV_ERR = BIT(12),
	/*! LLI invalid error or Shadow register error */
	DWAXIDMAC_IRQ_INVALID_ERR = BIT(13),
	/*! Slave Interface Multiblock type error*/
	DWAXIDMAC_IRQ_MULTIBLKTYPE_ERR = BIT(14),
	/*! Slave Interface decode error*/
	DWAXIDMAC_IRQ_DEC_ERR = BIT(16),
	/*! Slave Interface write to read only error*/
	DWAXIDMAC_IRQ_WR2RO_ERR = BIT(17),
	/*! Slave Interface read to write only error*/
	DWAXIDMAC_IRQ_RD2RWO_ERR = BIT(18),
	/*! Slave Interface write to channel error*/
	DWAXIDMAC_IRQ_WRONCHEN_ERR = BIT(19),
	/*! Slave Interface shadow reg error*/
	DWAXIDMAC_IRQ_SHADOWREG_ERR = BIT(20),
	/*! Slave Interface hold error*/
	DWAXIDMAC_IRQ_WRONHOLD_ERR = BIT(21),
	/*! Lock Cleared Status*/
	DWAXIDMAC_IRQ_LOCK_CLEARED = BIT(27),
	/*! Source Suspended Status */
	DWAXIDMAC_IRQ_SRC_SUSPENDED = BIT(28),
	/*! Channel Suspended Status*/
	DWAXIDMAC_IRQ_SUSPENDED = BIT(29),
	/*! Channel Disabled Status */
	DWAXIDMAC_IRQ_DISABLED = BIT(30),
	/*! Channel Aborted Status*/
	DWAXIDMAC_IRQ_ABORTED = BIT(31),
	/*! Bitmask of all error interrupts */
	DWAXIDMAC_IRQ_ALL_ERR = (GENMASK(21, 16) | GENMASK(14, 5)),
	/*! Bitmask of all interrupts*/
	DWAXIDMAC_IRQ_ALL = GENMASK(31, 0)
};

enum {
	DWAXIDMAC_TRANS_WIDTH_8 = 0,
	DWAXIDMAC_TRANS_WIDTH_16,
	DWAXIDMAC_TRANS_WIDTH_32,
	DWAXIDMAC_TRANS_WIDTH_64,
	DWAXIDMAC_TRANS_WIDTH_128,
	DWAXIDMAC_TRANS_WIDTH_256,
	DWAXIDMAC_TRANS_WIDTH_512,
	DWAXIDMAC_TRANS_WIDTH_MAX = DWAXIDMAC_TRANS_WIDTH_512
};

#endif /* _AXI_DMA_PLATFORM_H */
