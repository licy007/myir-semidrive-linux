// SPDX-License-Identifier:  GPL-2.0
// (C) 2017-2018 Synopsys, Inc. (www.synopsys.com)

/*
 * Synopsys DesignWare AXI DMA Controller driver.
 *
 * Author: Eugeniy Paltsev <Eugeniy.Paltsev@synopsys.com>
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include "dw-axi-dmac.h"
#include "../dmaengine.h"
#include "../virt-dma.h"
/* #define DEBUG 1 */

/*
 * The set of bus widths supported by the DMA controller. DW AXI DMAC supports
 * master data bus width up to 512 bits (for both AXI master interfaces), but
 * it depends on IP block configurarion.
 */
#define AXI_DMA_BUSWIDTHS          \
	(DMA_SLAVE_BUSWIDTH_1_BYTE |   \
	 DMA_SLAVE_BUSWIDTH_2_BYTES |  \
	 DMA_SLAVE_BUSWIDTH_4_BYTES |  \
	 DMA_SLAVE_BUSWIDTH_8_BYTES |  \
	 DMA_SLAVE_BUSWIDTH_16_BYTES | \
	 DMA_SLAVE_BUSWIDTH_32_BYTES | \
	 DMA_SLAVE_BUSWIDTH_64_BYTES)

static inline void axi_dma_iowrite32(struct axi_dma_chip *chip, u32 reg,
				     u32 val)
{
	iowrite32(val, chip->regs + reg);
}

static inline u32 axi_dma_ioread32(struct axi_dma_chip *chip, u32 reg)
{
	return ioread32(chip->regs + reg);
}

static inline void axi_chan_iowrite32(struct axi_dma_chan *chan, u32 reg,
				      u32 val)
{
	iowrite32(val, chan->chan_regs + reg);
}

static inline u32 axi_chan_ioread32(struct axi_dma_chan *chan, u32 reg)
{
	return ioread32(chan->chan_regs + reg);
}

static inline void axi_dma_mux_iowrite32(struct axi_dma_chip *chip, u32 reg,
					 u32 val)
{
	iowrite32(val, chip->mux_regs + reg);
}

static inline u32 axi_dma_mux_ioread32(struct axi_dma_chip *chip, u32 reg)
{
	return ioread32(chip->mux_regs + reg);
}

static inline void axi_chan_mux_iowrite32(struct axi_dma_chan *chan, u32 reg,
					  u32 val)
{
	iowrite32(val, chan->chan_mux_regs + reg);
}

static inline u32 axi_chan_mux_ioread32(struct axi_dma_chan *chan, u32 reg)
{
	return ioread32(chan->chan_mux_regs + reg);
}

static inline void axi_chan_iowrite64(struct axi_dma_chan *chan, u32 reg,
				      u64 val)
{
	/*
	 * We split one 64 bit write for two 32 bit write as some HW doesn't
	 * support 64 bit access.
	 */
	iowrite32(lower_32_bits(val), chan->chan_regs + reg);
	iowrite32(upper_32_bits(val), chan->chan_regs + reg + 4);
}

static inline u64 axi_chan_ioread64(struct axi_dma_chan *chan, u32 reg)
{
	/*
	 * We split one 64 bit read for two 32 bit read as some HW doesn't
	 * support 64 bit access.
	 */
	u64 res;

	res = ioread32(chan->chan_regs + reg + 4);
	res = ioread32(chan->chan_regs + reg) + (res << 32);
	return res;
}

static inline void axi_dma_disable(struct axi_dma_chip *chip)
{
	u32 val;

	val = axi_dma_ioread32(chip, DMAC_CFG);
	val &= ~DMAC_EN_MASK;
	axi_dma_iowrite32(chip, DMAC_CFG, val);
}

static inline void axi_dma_enable(struct axi_dma_chip *chip)
{
	u32 val;

	val = axi_dma_ioread32(chip, DMAC_CFG);
	val |= DMAC_EN_MASK;
	axi_dma_iowrite32(chip, DMAC_CFG, val);
}

static inline void axi_dma_irq_disable(struct axi_dma_chip *chip)
{
	u32 val;

	val = axi_dma_ioread32(chip, DMAC_CFG);
	val &= ~INT_EN_MASK;
	axi_dma_iowrite32(chip, DMAC_CFG, val);
}

static inline void axi_dma_irq_enable(struct axi_dma_chip *chip)
{
	u32 val;

	val = axi_dma_ioread32(chip, DMAC_CFG);
	val |= INT_EN_MASK;
	axi_dma_iowrite32(chip, DMAC_CFG, val);
}

static inline void axi_chan_irq_disable(struct axi_dma_chan *chan, u32 irq_mask)
{
	u32 val;

	if (likely(irq_mask == DWAXIDMAC_IRQ_ALL)) {
		axi_chan_iowrite32(chan, CH_INTSTATUS_ENA, DWAXIDMAC_IRQ_NONE);
	} else {
		val = axi_chan_ioread32(chan, CH_INTSTATUS_ENA);
		val &= ~irq_mask;
		axi_chan_iowrite32(chan, CH_INTSTATUS_ENA, val);
	}
}

static inline bool axi_dma_reset(struct axi_dma_chip *chip)
{
	u32 cnt = 0;
	axi_dma_iowrite32(chip, DMAC_RESET, 0x1);
	/*Wait for reset success*/
	while(axi_dma_ioread32(chip, DMAC_RESET)){
		udelay(1);
		cnt ++ ;
		if(cnt > 100)
			return false;
	}
	return true;
}
static inline void axi_chan_irq_set(struct axi_dma_chan *chan, u32 irq_mask)
{
	axi_chan_iowrite32(chan, CH_INTSTATUS_ENA, irq_mask);
}

static inline void axi_chan_irq_sig_set(struct axi_dma_chan *chan, u32 irq_mask)
{
	axi_chan_iowrite32(chan, CH_INTSIGNAL_ENA, irq_mask);
}

static inline void axi_chan_irq_clear(struct axi_dma_chan *chan, u32 irq_mask)
{
	axi_chan_iowrite32(chan, CH_INTCLEAR, irq_mask);
}

static inline u32 axi_chan_irq_read(struct axi_dma_chan *chan)
{
	return axi_chan_ioread32(chan, CH_INTSTATUS);
}

static inline const char *axi_chan_name(struct axi_dma_chan *chan)
{
	return dma_chan_name(&chan->vc.chan);
}

static inline bool axi_chan_is_hw_enable(struct axi_dma_chan *chan)
{
	u32 val;

	val = axi_dma_ioread32(chan->chip, DMAC_CHEN);

	return !!(val & (BIT(chan->id) << DMAC_CHAN_EN_SHIFT));
}

static inline void axi_chan_disabled_check(struct axi_dma_chan *chan)
{
	u32 reg;
	ktime_t start,diff;
	start = ktime_get();
	diff = ktime_sub(ktime_get(), start);
	while (axi_chan_is_hw_enable(chan)) {
		if ((chan->chip->dw->hdata->dma_abort_enable == true) &&
		    (chan->direction == DMA_MEM_TO_DEV)) {
			/*Switch to extra hs port*/
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      0 << DMAC_MUX_EN_POS);
			reg = (DMA_MUX_EXTRA_HS_PORT << DMAC_MUX_WR_HDSK_SHIFT);
			axi_chan_mux_iowrite32(chan, DMAC_CHAN_MUX, reg);
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      1 << DMAC_MUX_EN_POS);

			/*Switch to extra slave id port*/
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      0 << DMAC_MUX_EN_POS);
			reg = (chan->dma_sconfig.slave_id
			       << DMAC_MUX_WR_HDSK_SHIFT);
			axi_chan_mux_iowrite32(chan, DMAC_CHAN_MUX, reg);
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      1 << DMAC_MUX_EN_POS);
		}else{
			udelay(10);
		}
		diff = ktime_sub(ktime_get(), start);
		if (ktime_to_ms(diff) > 300) {
			dev_info(chan2dev(chan),
				 "%s :BUG is non-idle after disabled "
				 "and delay 300ms!!!\n",
				 axi_chan_name(chan));
			BUG_ON(axi_chan_is_hw_enable(chan));
			break;
		}
	}
	if (ktime_to_ms(diff) > 10) {
		dev_info(chan2dev(chan),
			 "%s :BUG is non-idle after disabled "
			 "and delay %lld ms!\n",
			 axi_chan_name(chan), ktime_to_ms(diff));
	}
}

static inline void axi_chan_disable(
    struct axi_dma_chan *chan)
{
	u32 val;
	val = axi_dma_ioread32(chan->chip, DMAC_CHEN);
	val &= ~(BIT(chan->id) << DMAC_CHAN_EN_SHIFT);
	val |= BIT(chan->id) << DMAC_CHAN_EN_WE_SHIFT;
	axi_dma_iowrite32(chan->chip, DMAC_CHEN, val);
	axi_chan_disabled_check(chan);
}
static inline void axi_chan_enable(struct axi_dma_chan *chan)
{
	u32 val;

	val = axi_dma_ioread32(chan->chip, DMAC_CHEN);
	val |= BIT(chan->id) << DMAC_CHAN_EN_SHIFT |
	       BIT(chan->id) << DMAC_CHAN_EN_WE_SHIFT;

	axi_dma_iowrite32(chan->chip, DMAC_CHEN, val);
}


static u32 bytes2block(size_t max_blocks, size_t bytes, unsigned int width,
		       size_t *len)
{
	u32 block;

	if ((bytes >> width) > max_blocks) {
		block = max_blocks;
		*len = block << width;
	} else {
		block = bytes >> width;
		*len = bytes;
	}

	return block;
}

static void axi_dma_hw_init(struct axi_dma_chip *chip)
{
	u32 i;

	for (i = 0; i < chip->dw->hdata->nr_channels; i++) {
		axi_chan_irq_disable(&chip->dw->chan[i], DWAXIDMAC_IRQ_ALL);
		axi_chan_disable(&chip->dw->chan[i]);
	}
}

static u32 axi_chan_get_xfer_width(struct axi_dma_chan *chan, dma_addr_t src,
				   dma_addr_t dst, size_t len)
{
	u32 max_width = chan->chip->dw->hdata->m_data_width;

	return __ffs(src | dst | len | BIT(max_width));
}



static struct axi_dma_desc *axi_desc_get(struct axi_dma_chan *chan)
{
	struct dw_axi_dma *dw = chan->chip->dw;
	struct axi_dma_desc *desc;
	dma_addr_t phys;

	desc = dma_pool_zalloc(dw->desc_pool, GFP_NOWAIT, &phys);
	if (unlikely(!desc)) {
		dev_err(chan2dev(chan),
			"%s: not enough descriptors available\n",
			axi_chan_name(chan));
		return NULL;
	}

	atomic_inc(&chan->descs_allocated);
	INIT_LIST_HEAD(&desc->xfer_list);
	desc->vd.tx.phys = phys;
	desc->chan = chan;
	/* Set default value to false and zero, modify it at related functions.  */
	desc->cyclic = false;
	desc->residue = 0;
	desc->len = 0;
	desc->total_len = 0;
	return desc;
}

static void axi_desc_put(struct axi_dma_desc *desc)
{
	struct axi_dma_chan *chan = desc->chan;
	struct dw_axi_dma *dw = chan->chip->dw;
	struct axi_dma_desc *child, *_next;
	unsigned int descs_put = 0;

	list_for_each_entry_safe(child, _next, &desc->xfer_list, xfer_list)
	{
		list_del(&child->xfer_list);
		dma_pool_free(dw->desc_pool, child, child->vd.tx.phys);
		descs_put++;
	}

	dma_pool_free(dw->desc_pool, desc, desc->vd.tx.phys);
	descs_put++;

	atomic_sub(descs_put, &chan->descs_allocated);
	dev_vdbg(chan2dev(chan), "%s: %d descs put, %d still allocated\n",
		 axi_chan_name(chan), descs_put,
		 atomic_read(&chan->descs_allocated));
}

static void vchan_desc_put(struct virt_dma_desc *vdesc)
{
	axi_desc_put(vd_to_axi_desc(vdesc));
}

static void write_desc_llp(struct axi_dma_desc *desc, dma_addr_t adr)
{
	desc->lli.llp = cpu_to_le64(adr);
}

static void write_chan_llp(struct axi_dma_chan *chan, dma_addr_t adr)
{
	axi_chan_iowrite64(chan, CH_LLP, adr);
}

/**
 * @brief read dma link list point from dma chan.
 *
 * @param chan
 * @return dma_addr_t link list point address.
 */
static dma_addr_t read_chan_llp(struct axi_dma_chan *chan)
{
	return axi_chan_ioread64(chan, CH_LLP);
}

static void axi_chan_dump_lli(struct axi_dma_chan *chan,
			      struct axi_dma_desc *desc)
{
	dev_err(dchan2dev(&chan->vc.chan),
		"SAR: 0x%llx DAR: 0x%llx LLP: 0x%llx BTS 0x%x CTL: 0x%x:%08x"
		"SSTAT: 0x%x DSTAT: 0x%x STATUS: 0x%x:%08x",
		le64_to_cpu(desc->lli.sar), le64_to_cpu(desc->lli.dar),
		le64_to_cpu(desc->lli.llp), le32_to_cpu(desc->lli.block_ts_lo),
		le32_to_cpu(desc->lli.ctl_hi), le32_to_cpu(desc->lli.ctl_lo),
		le32_to_cpu(desc->lli.sstat), le32_to_cpu(desc->lli.dstat),
		le32_to_cpu(desc->lli.status_hi),
		le32_to_cpu(desc->lli.status_lo));
}

static void axi_chan_list_dump_lli(struct axi_dma_chan *chan,
				   struct axi_dma_desc *desc_head)
{
	struct axi_dma_desc *desc;

	axi_chan_dump_lli(chan, desc_head);
	list_for_each_entry(desc, &desc_head->xfer_list, xfer_list)
	    axi_chan_dump_lli(chan, desc);
}


static size_t axi_chan_get_residue(struct axi_dma_chan *chan,
				   struct axi_dma_desc *desc_head)
{
	struct axi_dma_desc *desc;
	s64 tr_len;
	u32 residue;
	dma_addr_t llp;
	llp = read_chan_llp(chan);

	residue = desc_head->total_len;
	dev_vdbg(dchan2dev(&chan->vc.chan), "llp(0x%llx) vd.tx.phys (0x%llx) total: %lud : %lud: %ud",
		 read_chan_llp(chan), desc_head->vd.tx.phys,desc_head->total_len, desc_head->len, residue);

	/*FIXME:  Current ip llp will be cleared after stop. So checked by sar range*/
	/* if(llp  == desc_head->vd.tx.phys ) */

	switch (chan->direction) {
		case DMA_MEM_TO_DEV:

			tr_len = (axi_chan_ioread64(chan, CH_SAR) -
				  desc_head->lli.sar);
			if ((tr_len >= 0) && ( tr_len <= desc_head->len)) {
				residue -= tr_len;
				return residue;
			} else {
				/* Not in this link list
					dev_vdbg(chan2dev(chan),
					"axi_chan_ioread64(chan, CH_SAR) - "
					"desc_head->lli.sar is %d !\n",tr_len); */
			}
			break;

		case DMA_DEV_TO_MEM:
			tr_len = (axi_chan_ioread64(chan, CH_DAR) -
				  desc_head->lli.dar);
			if ((tr_len >= 0) && ( tr_len <= desc_head->len)) {
				residue -= tr_len;
				return residue;
			} else {
				/* Not in this link list
					dev_vdbg(chan2dev(chan),
					"axi_chan_ioread64(chan, CH_DAR) - "
					"desc_head->lli.dar is %d !\n",tr_len); */
			}
			break;

		default:
			dev_err(chan2dev(chan),
				"No support this direction to get residue!\n");
			return 0;
	}
	/*Remove header len from residue*/
	residue -=  desc_head->len;

	list_for_each_entry(desc, &desc_head->xfer_list, xfer_list)
	{
/* 		dev_vdbg(dchan2dev(&chan->vc.chan), "desc llp(0x%llx) vd.tx.phys (0x%llx) total: %d : %d",
		 read_chan_llp(chan), desc->vd.tx.phys,desc->total_len,  desc->len);  */
		/*FIXME:  Current ip llp will be cleared after stop. So need check by other conditions*/
		//if (llp == desc->vd.tx.phys) {
		switch (chan->direction) {
			case DMA_MEM_TO_DEV:

				tr_len = (axi_chan_ioread64(chan, CH_SAR) -
					  desc->lli.sar);
				if ((tr_len >= 0) && ( tr_len <= desc->len)) {
					residue -= tr_len;
					return residue;
				} else {
					/* Not in this link list
					dev_info(chan2dev(chan),
						"axi_chan_ioread64(chan, CH_SAR) - "
						"desc->lli.sar is %d !\n",tr_len);*/
				}
				break;
			case DMA_DEV_TO_MEM:

				tr_len = (axi_chan_ioread64(chan, CH_DAR) -
					  desc->lli.dar);
				if ((tr_len >= 0) && ( tr_len <= desc->len)) {
					residue -= tr_len;
					return residue;
				} else {
					/* Not in this link list
					dev_info(chan2dev(chan),
						"axi_chan_ioread64(chan, CH_DAR) - "
						"desc->lli.dar is %d !\n",tr_len);*/
				}
				break;
			default:
				dev_err(chan2dev(chan),
					"No support this direction to get "
					"residue!\n");
				return 0;
		}
		residue -= desc->len;
	}
	return residue;
}

static enum dma_status dma_chan_tx_status(struct dma_chan *dchan,
					  dma_cookie_t cookie,
					  struct dma_tx_state *txstate)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	enum dma_status ret;
	struct virt_dma_desc *vd;
	struct axi_dma_desc *desc;
	unsigned long flags;
	size_t residue = 0;
	ret = dma_cookie_status(dchan, cookie, txstate);
	if (ret == DMA_COMPLETE || !txstate)
		return ret;
	spin_lock_irqsave(&chan->vc.lock, flags);
	ret = dma_cookie_status(dchan, cookie, txstate);
	if (ret != DMA_COMPLETE) {
		vd = vchan_find_desc(&chan->vc, cookie);
 		if (vd) {
			/* Vd is on the issued list, maybe still not removed by terminate function.*/
			desc = vd_to_axi_desc(vd);
			residue = desc->total_len;
		}
		if ((chan->desc && chan->desc->vd.tx.cookie == cookie)) {
			/* Vd is running or not be removed */
			residue = axi_chan_get_residue(
				    chan, vd_to_axi_desc(&chan->desc->vd));
		}
	}

	spin_unlock_irqrestore(&chan->vc.lock, flags);
	dma_set_residue(txstate, residue);
	dev_vdbg(chan2dev(chan),"%s get residue %lud.  \n", __func__,residue);

	if (chan->is_paused && ret == DMA_IN_PROGRESS)
		ret = DMA_PAUSED;

	return ret;
}

/* Called in chan locked context */
static void axi_chan_block_xfer_start(struct axi_dma_chan *chan,
				      struct axi_dma_desc *first)
{
	u32 priority = chan->chip->dw->hdata->priority[chan->id];
	u32 reg, irq_mask, hs_id;
	u8 lms = 0; /* Select AXI0 master for LLI fetching */

	if (unlikely(axi_chan_is_hw_enable(chan))) {
		dev_info(chan2dev(chan), "%s is non-idle!\n",
				 axi_chan_name(chan));
		return;
	}

	switch (chan->direction) {

	case DMA_MEM_TO_MEM:
		reg = (DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_DST_MULTBLK_TYPE_POS |
		       DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_SRC_MULTBLK_TYPE_POS);
		axi_chan_iowrite32(chan, CH_CFG_L, reg);

		reg = (DWAXIDMAC_TT_FC_MEM_TO_MEM_DMAC << CH_CFG_H_TT_FC_POS |
		       priority << CH_CFG_H_PRIORITY_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_DST_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_SRC_POS);
		axi_chan_iowrite32(chan, CH_CFG_H, reg);
		break;

	case DMA_MEM_TO_DEV:
		reg = (DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_DST_MULTBLK_TYPE_POS |
		       DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_SRC_MULTBLK_TYPE_POS);
		axi_chan_iowrite32(chan, CH_CFG_L, reg);
		dev_vdbg(chan2dev(chan), "%s: CH_CFG_L(0x%x)", __func__, reg);
		if (chan->chip->dw->hdata->dma_mux_enable == true) {
			hs_id = chan->id * 2 + 1;
		} else {
			hs_id = chan->dma_sconfig.slave_id;
		}
		reg = (DWAXIDMAC_TT_FC_MEM_TO_PER_DMAC << CH_CFG_H_TT_FC_POS |
		       priority << CH_CFG_H_PRIORITY_POS |
		       hs_id << CH_CFG_H_DST_PER_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_DST_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_SRC_POS);
		axi_chan_iowrite32(chan, CH_CFG_H, reg);

		dev_vdbg(chan2dev(chan),
			 "%s: direction(%d) hs(%d) L(0x%x) H(0x%x)\n", __func__,
			 chan->direction, chan->dma_sconfig.slave_id,
			 axi_chan_ioread32(chan, CH_CFG_L),
			 axi_chan_ioread32(chan, CH_CFG_H));
		/* Set dma mux here MEM TO DEV -> WR */
		if (chan->chip->dw->hdata->dma_mux_enable == true) {
			/* Disable DMA MUX before setting*/
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      0 << DMAC_MUX_EN_POS);
			reg = (chan->dma_sconfig.slave_id
			       << DMAC_MUX_WR_HDSK_SHIFT);
			axi_chan_mux_iowrite32(chan, DMAC_CHAN_MUX, reg);
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      1 << DMAC_MUX_EN_POS);
			dev_vdbg(chan2dev(chan),
				 "%s: WR_HDSK(0x%x) vaddr(0x%p) vaddr(0x%p) \n",
				 __func__, reg,
				 chan->chip->mux_regs + DMAC_MUX_EN,
				 chan->chan_mux_regs + DMAC_CHAN_MUX);
		}
		break;

	case DMA_DEV_TO_MEM:
		reg = (DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_DST_MULTBLK_TYPE_POS |
		       DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_SRC_MULTBLK_TYPE_POS);
		axi_chan_iowrite32(chan, CH_CFG_L, reg);

		if (chan->chip->dw->hdata->dma_mux_enable == true) {
			hs_id = chan->id * 2;
		} else {
			hs_id = chan->dma_sconfig.slave_id;
		}
		reg = (DWAXIDMAC_TT_FC_PER_TO_MEM_DMAC << CH_CFG_H_TT_FC_POS |
		       priority << CH_CFG_H_PRIORITY_POS |
		       hs_id << CH_CFG_H_SRC_PER_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_DST_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_SRC_POS);
		axi_chan_iowrite32(chan, CH_CFG_H, reg);
		/* Set dma mux here DEV TO MUX -> RD */
		if (chan->chip->dw->hdata->dma_mux_enable == true) {
			/* Disable DMA MUX before setting*/
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      0 << DMAC_MUX_EN_POS);
			reg = (chan->dma_sconfig.slave_id
			       << DMAC_MUX_RD_HDSK_SHIFT);
			axi_chan_mux_iowrite32(chan, DMAC_CHAN_MUX, reg);
			axi_dma_mux_iowrite32(chan->chip, DMAC_MUX_EN,
					      1 << DMAC_MUX_EN_POS);
			dev_vdbg(chan2dev(chan), "%s: RD_HDSK(0x%x) (0x%x) \n",
				 __func__, reg,
				 axi_chan_mux_ioread32(chan, DMAC_CHAN_MUX));
		}
		break;

	// Don't support it for hand shake.
	case DMA_DEV_TO_DEV:
		reg = (DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_DST_MULTBLK_TYPE_POS |
		       DWAXIDMAC_MBLK_TYPE_LL << CH_CFG_L_SRC_MULTBLK_TYPE_POS);
		axi_chan_iowrite32(chan, CH_CFG_L, reg);

		reg = (DWAXIDMAC_TT_FC_PER_TO_PER_DMAC << CH_CFG_H_TT_FC_POS |
		       priority << CH_CFG_H_PRIORITY_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_DST_POS |
		       DWAXIDMAC_HS_SEL_HW << CH_CFG_H_HS_SEL_SRC_POS);
		axi_chan_iowrite32(chan, CH_CFG_H, reg);
		break;
	default:
		dev_err(chan2dev(chan), "No support this direction!\n");
		// return;
	}
	write_chan_llp(chan, first->vd.tx.phys | lms);

	// TODO:  if need block transfer interrupt, set here.
	// dma_chan_prep_cyclic
	if (first->cyclic) {
		irq_mask = DWAXIDMAC_IRQ_BLOCK_TRF | DWAXIDMAC_IRQ_ALL_ERR;
		dev_vdbg(chan2dev(chan), "%s: irq cyclic mask 0x%x 0x%x\n",
			 __func__, irq_mask, reg);
	} else {
		irq_mask = DWAXIDMAC_IRQ_DMA_TRF | DWAXIDMAC_IRQ_ALL_ERR;
		dev_vdbg(chan2dev(chan), "%s: irq mask 0x%x 0x%x\n", __func__,
			 irq_mask, reg);
	}
	axi_chan_irq_sig_set(chan, irq_mask);

	/* Generate 'suspend' status but don't generate interrupt */
	irq_mask |= DWAXIDMAC_IRQ_SUSPENDED;
	axi_chan_irq_set(chan, irq_mask);

	first->residue = first->total_len;
	chan->desc = first;

	axi_chan_enable(chan);
}

static void axi_chan_start_first_queued(struct axi_dma_chan *chan)
{
	struct axi_dma_desc *desc;
	struct virt_dma_desc *vd;

	vd = vchan_next_desc(&chan->vc);
	if (!vd)
		return;

	desc = vd_to_axi_desc(vd);
	dev_vdbg(chan2dev(chan), "%s: started %u\n", axi_chan_name(chan),
		 vd->tx.cookie);
	axi_chan_block_xfer_start(chan, desc);
}

static void dma_chan_issue_pending(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);
	if (vchan_issue_pending(&chan->vc) && (!chan->desc))
		axi_chan_start_first_queued(chan);
	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static void dw_axi_dma_synchronize(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	vchan_synchronize(&chan->vc);
}

static int dma_chan_alloc_chan_resources(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	/* ASSERT: channel is idle */
	if (axi_chan_is_hw_enable(chan)) {
		dev_err(chan2dev(chan), "%s is non-idle!\n",
			axi_chan_name(chan));
		return -EBUSY;
	}

	dev_vdbg(dchan2dev(dchan), "%s: allocating\n", axi_chan_name(chan));

	pm_runtime_get(chan->chip->dev);

	return 0;
}

static void dma_chan_free_chan_resources(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	/* ASSERT: channel is idle */
	if (axi_chan_is_hw_enable(chan))
		dev_err(dchan2dev(dchan), "%s is non-idle!\n",
			axi_chan_name(chan));

	axi_chan_disable(chan);
	axi_chan_irq_disable(chan, DWAXIDMAC_IRQ_ALL);

	vchan_free_chan_resources(&chan->vc);

	dev_vdbg(dchan2dev(dchan),
		 "%s: free resources, descriptor still allocated: %u\n",
		 axi_chan_name(chan), atomic_read(&chan->descs_allocated));

	pm_runtime_put(chan->chip->dev);
}

/*
 * If DW_axi_dmac sees CHx_CTL.ShadowReg_Or_LLI_Last bit of the fetched LLI
 * as 1, it understands that the current block is the final block in the
 * transfer and completes the DMA transfer operation at the end of current
 * block transfer.
 */
static void set_desc_last(struct axi_dma_desc *desc)
{
	u32 val;

	val = le32_to_cpu(desc->lli.ctl_hi);
	val |= CH_CTL_H_LLI_LAST;
	desc->lli.ctl_hi = cpu_to_le32(val);
}

static void write_desc_sar(struct axi_dma_desc *desc, dma_addr_t adr)
{
	desc->lli.sar = cpu_to_le64(adr);
}

static void write_desc_dar(struct axi_dma_desc *desc, dma_addr_t adr)
{
	desc->lli.dar = cpu_to_le64(adr);
}

static void set_desc_src_master(struct axi_dma_desc *desc)
{
	u32 val;

	/* Select AXI0 for source master */
	val = le32_to_cpu(desc->lli.ctl_lo);
	val &= ~CH_CTL_L_SRC_MAST;
	desc->lli.ctl_lo = cpu_to_le32(val);
}

static void set_desc_dest_master(struct axi_dma_desc *desc)
{
	u32 val;

	/* Select AXI1 for source master if available */
	val = le32_to_cpu(desc->lli.ctl_lo);
	if (desc->chan->chip->dw->hdata->nr_masters > 1)
		val |= CH_CTL_L_DST_MAST;
	else
		val &= ~CH_CTL_L_DST_MAST;

	desc->lli.ctl_lo = cpu_to_le32(val);
}

static struct dma_async_tx_descriptor *
dma_chan_prep_dma_memcpy(struct dma_chan *dchan, dma_addr_t dst_adr,
			 dma_addr_t src_adr, size_t len, unsigned long flags)
{
	struct axi_dma_desc *first = NULL, *desc = NULL, *prev = NULL;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	size_t block_ts, max_block_ts, xfer_len;
	u32 xfer_width, reg;
	u8 lms = 0; /* Select AXI0 master for LLI fetching */

	chan->direction = DMA_MEM_TO_MEM;

	dev_vdbg(chan2dev(chan),
		"%s: memcpy: src: %pad dst: %pad length: %zd flags: %#lx",
		axi_chan_name(chan), &src_adr, &dst_adr, len, flags);

	max_block_ts = chan->chip->dw->hdata->block_size[chan->id];

	while (len) {
		xfer_len = len;

		/*
		 * Take care for the alignment.
		 * Actually source and destination widths can be different, but
		 * make them same to be simpler.
		 */
		xfer_width =
		    axi_chan_get_xfer_width(chan, src_adr, dst_adr, xfer_len);

		/*
		 * block_ts indicates the total number of data of width
		 * to be transferred in a DMA block transfer.
		 * BLOCK_TS register should be set to block_ts - 1
		 */
		block_ts = xfer_len >> xfer_width;
		if (block_ts > max_block_ts) {
			block_ts = max_block_ts;
			xfer_len = max_block_ts << xfer_width;
		}

		desc = axi_desc_get(chan);
		if (unlikely(!desc))
			goto err_desc_get;

		desc->len = xfer_len;

		write_desc_sar(desc, src_adr);
		write_desc_dar(desc, dst_adr);
		desc->lli.block_ts_lo = cpu_to_le32(block_ts - 1);

		reg = CH_CTL_H_LLI_VALID;
		if (chan->chip->dw->hdata->restrict_axi_burst_len) {
			u32 burst_len = chan->chip->dw->hdata->axi_rw_burst_len;

			reg |= (CH_CTL_H_ARLEN_EN |
				burst_len << CH_CTL_H_ARLEN_POS |
				CH_CTL_H_AWLEN_EN |
				burst_len << CH_CTL_H_AWLEN_POS);
		}
		desc->lli.ctl_hi = cpu_to_le32(reg);

		reg = (DWAXIDMAC_BURST_TRANS_LEN_4 << CH_CTL_L_DST_MSIZE_POS |
		       DWAXIDMAC_BURST_TRANS_LEN_4 << CH_CTL_L_SRC_MSIZE_POS |
		       xfer_width << CH_CTL_L_DST_WIDTH_POS |
		       xfer_width << CH_CTL_L_SRC_WIDTH_POS |
		       DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_DST_INC_POS |
		       DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_SRC_INC_POS);
		desc->lli.ctl_lo = cpu_to_le32(reg);

		set_desc_src_master(desc);
		set_desc_dest_master(desc);

		/* Manage transfer list (xfer_list) */
		if (!first) {
			first = desc;
		} else {
			list_add_tail(&desc->xfer_list, &first->xfer_list);
			write_desc_llp(prev, desc->vd.tx.phys | lms);
		}
		prev = desc;
		/*  TODO: Need Debug Line set first desc to total_len*/
		first->total_len = len;
		/* update the length and addresses for the next loop cycle */
		len -= xfer_len;
		dst_adr += xfer_len;
		src_adr += xfer_len;
	}

	/* Total len of src/dest sg == 0, so no descriptor were allocated */
	if (unlikely(!first))
		return NULL;

	/* Set end-of-link to the last link descriptor of list */
	set_desc_last(desc);

	return vchan_tx_prep(&chan->vc, &first->vd, flags);

err_desc_get:
	axi_desc_put(first);
	return NULL;
}

static noinline void axi_chan_handle_err(struct axi_dma_chan *chan, u32 status)
{
	struct virt_dma_desc *vd;
	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);

	axi_chan_disable(chan);

	/* The bad descriptor currently is in the head of vc list */
	vd = vchan_next_desc(&chan->vc);
	/* Remove the completed descriptor from issued list */
	list_del(&vd->node);

	/* WARN about bad descriptor */
	dev_err(chan2dev(chan),
		"Bad descriptor submitted for %s, cookie: %d, irq: 0x%08x\n",
		axi_chan_name(chan), vd->tx.cookie, status);
	axi_chan_list_dump_lli(chan, vd_to_axi_desc(vd));

	vchan_cookie_complete(vd);

	/* Try to restart the controller */
	axi_chan_start_first_queued(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);
}
static inline struct virt_dma_desc *
vchan_submited_desc(struct virt_dma_chan *vc)
{
	return list_first_entry_or_null(&vc->desc_issued, struct virt_dma_desc,
					node);
}
static void axi_chan_blocks_xfer_complete(struct axi_dma_chan *chan)
{
	struct virt_dma_desc *vd;
	unsigned long flags;
	spin_lock_irqsave(&chan->vc.lock, flags);
	if (unlikely(axi_chan_is_hw_enable(chan))) {
		dev_info(chan2dev(chan),
			"WARNNING: %s caught DWAXIDMAC_IRQ_DMA_TRF, but channel not "
			"idle!\n",
			axi_chan_name(chan));
		axi_chan_disable(chan);
	}

	/* The completed descriptor currently is in the head of vc list */
	vd = vchan_next_desc(&chan->vc);
	if(!vd) {
		dev_info(chan2dev(chan), "WARNNING: %s, IRQ with no descriptors\n",
			axi_chan_name(chan));
		goto out;
	}
	/* Remove the completed descriptor from issued list before completing */
	list_del(&vd->node);
	chan->desc = NULL;

	vchan_cookie_complete(vd);

	/* Submit queued descriptors after processing the completed ones */
	axi_chan_start_first_queued(chan);
out:
	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static void axi_chan_one_block_xfer_complete(struct axi_dma_chan *chan)
{
	struct virt_dma_desc *vd;
	unsigned long flags;
	struct axi_dma_desc *axi_desc;

	if (unlikely(!axi_chan_is_hw_enable(chan))) {
		dev_info(chan2dev(chan),
			"WARNNING: %s caught DWAXIDMAC_IRQ_DMA_BLK, but channel "
			"already disable!\n",
			axi_chan_name(chan));
		return;
	}
	spin_lock_irqsave(&chan->vc.lock, flags);

	vd = vchan_next_desc(&chan->vc);
	if(vd){
		axi_desc = vd_to_axi_desc(vd);
		if(axi_desc){
			if (true == axi_desc->cyclic) {
				// dev_vdbg(chan2dev(chan), "%s: cyclic
				// interrupt!\n",axi_chan_name(chan));
				vchan_cyclic_callback(vd);
			} else {
				dev_err(chan2dev(chan), "%s: cyclic interrupt status error!\n",
					axi_chan_name(chan));
			}
		}
	}
	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static irqreturn_t dw_axi_dma_interrupt(int irq, void *dev_id)
{
	struct axi_dma_chip *chip = dev_id;
	struct dw_axi_dma *dw = chip->dw;
	struct axi_dma_chan *chan;
	u32 status, i;

	spin_lock(&chip->dev_lock);
	/* Disable DMAC inerrupts. We'll enable them after processing chanels */
	axi_dma_irq_disable(chip);

	/* Poll, clear and process every chanel interrupt status */
	for (i = 0; i < dw->hdata->nr_channels; i++) {
		chan = &dw->chan[i];
		status = axi_chan_irq_read(chan);
		axi_chan_irq_clear(chan, status);
		// dev_vdbg(chan2dev(chan), "%s:interrupt!\n",
		// axi_chan_name(chan));
		if (status & DWAXIDMAC_IRQ_ALL_ERR)
			axi_chan_handle_err(chan, status);
		else if (status & DWAXIDMAC_IRQ_DMA_TRF)
			axi_chan_blocks_xfer_complete(chan);
		else if (status & DWAXIDMAC_IRQ_BLOCK_TRF) {
			axi_chan_one_block_xfer_complete(chan);
		}
	}

	/* Re-enable interrupts */
	axi_dma_irq_enable(chip);
	spin_unlock(&chip->dev_lock);
	return IRQ_HANDLED;
}

static int dma_chan_terminate_all(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	unsigned long flags;

	LIST_HEAD(head);
	spin_lock_irqsave(&chan->vc.lock, flags);
	axi_chan_disable(chan);
	//axi_desc = chan->desc;
	chan->desc = NULL;

	vchan_get_all_descriptors(&chan->vc, &head);
/* 	if(axi_desc){
		//axi_desc_put(axi_desc);
	} */
	/*
	 * As vchan_dma_desc_free_list can access to desc_allocated list
	 * we need to call it in vc.lock context. it will put axi desc
	 */

	vchan_dma_desc_free_list(&chan->vc, &head);
	spin_unlock_irqrestore(&chan->vc.lock, flags);


	dev_vdbg(dchan2dev(dchan), "terminated: %s\n", axi_chan_name(chan));
	return 0;
}

static int dma_chan_pause(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	unsigned long flags;
	unsigned int timeout = 20; /* timeout iterations */
	u32 val;

	spin_lock_irqsave(&chan->vc.lock, flags);

	val = axi_dma_ioread32(chan->chip, DMAC_CHEN);
	val |= BIT(chan->id) << DMAC_CHAN_SUSP_SHIFT |
	       BIT(chan->id) << DMAC_CHAN_SUSP_WE_SHIFT;
	axi_dma_iowrite32(chan->chip, DMAC_CHEN, val);

	do {
		if (axi_chan_irq_read(chan) & DWAXIDMAC_IRQ_SUSPENDED)
			break;

		udelay(2);
	} while (--timeout);

	axi_chan_irq_clear(chan, DWAXIDMAC_IRQ_SUSPENDED);

	chan->is_paused = true;

	spin_unlock_irqrestore(&chan->vc.lock, flags);

	return timeout ? 0 : -EAGAIN;
}

/* Called in chan locked context */
static inline void axi_chan_resume(struct axi_dma_chan *chan)
{
	u32 val;

	val = axi_dma_ioread32(chan->chip, DMAC_CHEN);
	val &= ~(BIT(chan->id) << DMAC_CHAN_SUSP_SHIFT);
	val |= (BIT(chan->id) << DMAC_CHAN_SUSP_WE_SHIFT);
	axi_dma_iowrite32(chan->chip, DMAC_CHEN, val);

	chan->is_paused = false;
}

static int dma_chan_resume(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);

	if (chan->is_paused)
		axi_chan_resume(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);

	return 0;
}

static int axi_dma_suspend(struct axi_dma_chip *chip)
{
	unsigned long flags;
	spin_lock_irqsave(&chip->dev_lock,flags);
	axi_dma_irq_disable(chip);
	axi_dma_disable(chip);
	spin_unlock_irqrestore(&chip->dev_lock,flags);

	clk_disable_unprepare(chip->core_clk);
	clk_disable_unprepare(chip->cfgr_clk);

	return 0;
}

static int axi_dma_resume(struct axi_dma_chip *chip)
{
	int ret;
	unsigned long flags;

	ret = clk_prepare_enable(chip->cfgr_clk);
	if (ret < 0)
		return ret;

	ret = clk_prepare_enable(chip->core_clk);
	if (ret < 0)
		return ret;
	spin_lock_irqsave(&chip->dev_lock,flags);
	axi_dma_enable(chip);
	axi_dma_irq_enable(chip);
	spin_unlock_irqrestore(&chip->dev_lock,flags);
	return 0;
}

static int __maybe_unused axi_dma_runtime_suspend(struct device *dev)
{
	struct axi_dma_chip *chip = dev_get_drvdata(dev);

	return axi_dma_suspend(chip);
}

static int __maybe_unused axi_dma_runtime_resume(struct device *dev)
{
	struct axi_dma_chip *chip = dev_get_drvdata(dev);

	return axi_dma_resume(chip);
}

static int parse_device_properties(struct axi_dma_chip *chip)
{
	struct device *dev = chip->dev;
	u32 tmp, carr[DMAC_MAX_CHANNELS];
	int ret;

	ret = device_property_read_u32(dev, "dma-channels", &tmp);
	if (ret)
		return ret;
	if (tmp == 0 || tmp > DMAC_MAX_CHANNELS)
		return -EINVAL;

	chip->dw->hdata->nr_channels = tmp;

	ret = device_property_read_u32(dev, "snps,dma-masters", &tmp);
	if (ret)
		return ret;
	if (tmp == 0 || tmp > DMAC_MAX_MASTERS)
		return -EINVAL;

	chip->dw->hdata->nr_masters = tmp;

	ret = device_property_read_u32(dev, "snps,data-width", &tmp);
	if (ret)
		return ret;
	if (tmp > DWAXIDMAC_TRANS_WIDTH_MAX)
		return -EINVAL;

	chip->dw->hdata->m_data_width = tmp;

	ret = device_property_read_u32_array(dev, "snps,block-size", carr,
					     chip->dw->hdata->nr_channels);
	if (ret)
		return ret;
	for (tmp = 0; tmp < chip->dw->hdata->nr_channels; tmp++) {
		if (carr[tmp] == 0 || carr[tmp] > DMAC_MAX_BLK_SIZE)
			return -EINVAL;

		chip->dw->hdata->block_size[tmp] = carr[tmp];
	}

	ret = device_property_read_u32_array(dev, "snps,priority", carr,
					     chip->dw->hdata->nr_channels);
	if (ret)
		return ret;
	/* Priority value must be programmed within [0:nr_channels-1] range */
	for (tmp = 0; tmp < chip->dw->hdata->nr_channels; tmp++) {
		if (carr[tmp] >= chip->dw->hdata->nr_channels)
			return -EINVAL;

		chip->dw->hdata->priority[tmp] = carr[tmp];
	}

	/* axi-max-burst-len is optional property */
	ret = device_property_read_u32(dev, "snps,axi-max-burst-len", &tmp);
	if (!ret) {
		if (tmp > DWAXIDMAC_ARWLEN_MAX + 1)
			return -EINVAL;
		if (tmp < DWAXIDMAC_ARWLEN_MIN + 1)
			return -EINVAL;

		chip->dw->hdata->restrict_axi_burst_len = true;
		chip->dw->hdata->axi_rw_burst_len = tmp - 1;
	}

	if (device_property_read_bool(dev, "sdrv,disabled-abort")) {
		/*If find node 'sdrv,disabled-abort' for TO1.0 disable dma abort
		 * function.*/
		chip->dw->hdata->dma_abort_enable = false;
	} else if (chip->dw->hdata->dma_mux_enable == false) {
		/*No mux exist, don't enable abort */
		chip->dw->hdata->dma_abort_enable = false;
	} else {
		chip->dw->hdata->dma_abort_enable = true;
	}
	return 0;
}
/****
 *
 *
 * src_maxburst/dst_maxburst only support 1,4,8,16,32,64 items.
 */
static int dma_chan_config(struct dma_chan *dchan,
			   struct dma_slave_config *sconfig)
{
	// get axi dma channel from dma_chan
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	struct dma_slave_config *sc = &chan->dma_sconfig;
	unsigned int slave_id = sc->slave_id;

	/* Check if chan will be configured for slave transfers */
	if (!is_slave_direction(sconfig->direction))
		return -EINVAL;

	memcpy(&chan->dma_sconfig, sconfig, sizeof(*sconfig));
	chan->direction = sconfig->direction;

	/* Restore slave id if it is allocated by of dma*/
	if (chan->fix_slave_id == true) {
		sc->slave_id = slave_id;
		printk(KERN_DEBUG "DW AXI DMAC "
				  "%s slave id: %d \n",
		       __FUNCTION__, sc->slave_id);
	}
	/*
	 * Fix sconfig's burst size according to dw_axi_dmac. We need to convert
	 * them as:
	 * number of units(DMA_DEV_BUS_WIDTH)
	 * 1 -> 0(1ITEM), 4 -> 1(4ITEMS), 8 -> 2(8ITEMS), 16 -> 3(16ITEMS). 32
	 * -> 4
	 *
	 * NOTE: DesignWare controller.
	 *       iDMA 32-bit supports
	 * DMA_BURST_TR_1ITEM = 0,
	 * DMA_BURST_TR_4ITEMS = 1,
	 * DMA_BURST_TR_8ITEMS = 2,
	 * DMA_BURST_TR_16ITEMS = 3,
	 * DMA_BURST_TR_32ITEMS = 4,
	 * DMA_BURST_TR_64ITEMS = 5,
	 * DMA_BURST_TR_128ITEMS = 6,
	 * DMA_BURST_TR_256ITEMS = 7,
	 * DMA_BURST_TR_512ITEMS = 8,
	 * DMA_BURST_TR_1024ITEMS = 9,
	 */
	sc->src_maxburst = sc->src_maxburst > 1 ? fls(sc->src_maxburst) - 2 : 0;
	sc->dst_maxburst = sc->dst_maxburst > 1 ? fls(sc->dst_maxburst) - 2 : 0;

	return 0;
}
/* TODO: Need test with dev */
static struct dma_async_tx_descriptor *dma_chan_prep_slave_sg(
    struct dma_chan *dchan, struct scatterlist *sgl, unsigned int sg_len,
    enum dma_transfer_direction direction, unsigned long flags, void *context)
{
	struct axi_dma_desc *first = NULL, *desc = NULL, *prev = NULL;
	/* Get axi_dma_chan from dma_chan */
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	/* Get slave config from axi_dma_chan */
	struct dma_slave_config *sconfig = &chan->dma_sconfig;
	size_t max_block_ts;
	u32 reg;
	u32 burst_items;
	u32 reg_width;
	u32 mem_width;
	dma_addr_t reg_addr;
	dma_addr_t mem_addr;

	struct scatterlist *sg;
	u32 i, data_width;
	size_t total_len = 0;

	u8 lms = 0; /* Select AXI0 master for LLI fetching */

	max_block_ts = chan->chip->dw->hdata->block_size[chan->id];
	data_width = BIT(chan->chip->dw->hdata->m_data_width);
	// TODO: set channel need add test case for multi transfer
	chan->direction = direction;

	if (unlikely(!is_slave_direction(direction) || !sg_len))
		return NULL;
	switch (direction) {

	case DMA_MEM_TO_DEV:
		reg_width = __ffs(sconfig->dst_addr_width);
		reg_addr = sconfig->dst_addr;
		for_each_sg(sgl, sg, sg_len, i)
		{
			u32 len;
			size_t dlen; /* desc len */
			/* get current sg dma address. */
			mem_addr = sg_dma_address(sg);
			/* get current sg len */
			len = sg_dma_len(sg);
			/* get mem data width */
			mem_width = __ffs(data_width | mem_addr | len);
			/* get burst items */
			burst_items = sconfig->dst_maxburst;

		slave_sg_todev_fill_desc:
			/* create a axi desc */
			desc = axi_desc_get(chan);
			if (unlikely(!desc))
				goto err_desc_get;
			/* Memory address. */
			write_desc_sar(desc, mem_addr);
			/* Device address */
			write_desc_dar(desc, reg_addr);
			/* calculate block size */
			desc->lli.block_ts_lo = cpu_to_le32(
			    bytes2block(max_block_ts, len, mem_width, &dlen) -
			    1);
			/* 0: Shadow Register content/LLI is invalid.1: Last
			 * Shadow Register/LLI is valid. */
			reg = CH_CTL_H_LLI_VALID;
			/* Enable axi burst len as config */
			if (chan->chip->dw->hdata->restrict_axi_burst_len) {
				u32 burst_len =
				    chan->chip->dw->hdata->axi_rw_burst_len;

				reg |= (CH_CTL_H_ARLEN_EN |
					burst_len << CH_CTL_H_ARLEN_POS |
					CH_CTL_H_AWLEN_EN |
					burst_len << CH_CTL_H_AWLEN_POS);
			}

			/* set lli high */
			desc->lli.ctl_hi = cpu_to_le32(reg);

			/* lli low */
			reg =
			    (burst_items
				 << CH_CTL_L_DST_MSIZE_POS | /* change to max
								burst */
			     burst_items << CH_CTL_L_SRC_MSIZE_POS |
			     reg_width << CH_CTL_L_DST_WIDTH_POS |
			     mem_width << CH_CTL_L_SRC_WIDTH_POS |
			     DWAXIDMAC_CH_CTL_L_NOINC << CH_CTL_L_DST_INC_POS |
			     DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_SRC_INC_POS);

			desc->lli.ctl_lo = cpu_to_le32(reg);
			/* set desc len*/
			desc->len = dlen;

			/* 			set_desc_src_master(desc);
			set_desc_dest_master(desc); */

			if (!first) {
				first = desc;
			} else {
				list_add_tail(&desc->xfer_list,
					      &first->xfer_list);
				write_desc_llp(prev, desc->vd.tx.phys | lms);
			}
			prev = desc;
			mem_addr += dlen;
			len -= dlen;
			total_len += dlen;
			/* 	dev_dbg(chan2dev(chan), "%s: desc mem2dev:
			   ctl_li: 0x%x ctl_lo: 0x%x src 0x%llx dst 0x%llx
			   mem_addr 0x%llx sg_len 0x%x dlen: 0x%x len: %d
			   total_len: %d block: %d flag: %d",
					axi_chan_name(chan), desc->lli.ctl_hi,
			   desc->lli.ctl_lo, desc->lli.sar, desc->lli.dar,
			   mem_addr, sg_len, dlen, len, total_len,
			   desc->lli.block_ts_lo, flags);
		 */
			if (len)
				goto slave_sg_todev_fill_desc;
		}
		break;

	case DMA_DEV_TO_MEM:
		reg_width = __ffs(sconfig->src_addr_width);
		reg_addr = sconfig->src_addr;
		for_each_sg(sgl, sg, sg_len, i)
		{
			u32 len;
			size_t dlen; /* desc len */

			// get current sg dma address.
			mem_addr = sg_dma_address(sg);
			// get current sg len
			len = sg_dma_len(sg);
			// get mem data width
			mem_width = __ffs(data_width | mem_addr | len);
			// get burst items
			burst_items = sconfig->src_maxburst;

		slave_sg_fromdev_fill_desc:
			desc = axi_desc_get(chan);
			if (unlikely(!desc))
				goto err_desc_get;

			/* Device address */
			write_desc_sar(desc, reg_addr);
			/* Memory address. */
			write_desc_dar(desc, mem_addr);
			/* calculate block size */
			desc->lli.block_ts_lo = cpu_to_le32(
			    bytes2block(max_block_ts, len, reg_width, &dlen) -
			    1);
			/* 0: Shadow Register content/LLI is invalid.1: Last
			 * Shadow Register/LLI is valid. */
			reg = CH_CTL_H_LLI_VALID;
			/* Enable axi burst len as config */
			if (chan->chip->dw->hdata->restrict_axi_burst_len) {
				u32 burst_len =
				    chan->chip->dw->hdata->axi_rw_burst_len;

				reg |= (CH_CTL_H_ARLEN_EN |
					burst_len << CH_CTL_H_ARLEN_POS |
					CH_CTL_H_AWLEN_EN |
					burst_len << CH_CTL_H_AWLEN_POS);
			}

			/* set lli valid */
			desc->lli.ctl_hi = cpu_to_le32(reg);
			/* Set desc len*/
			desc->len = dlen;

			/* lli low */
			reg =
			    (burst_items
				 << CH_CTL_L_DST_MSIZE_POS | /* change to max
								burst */
			     burst_items << CH_CTL_L_SRC_MSIZE_POS |
			     reg_width << CH_CTL_L_SRC_WIDTH_POS |
			     mem_width << CH_CTL_L_DST_WIDTH_POS |
			     DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_DST_INC_POS |
			     DWAXIDMAC_CH_CTL_L_NOINC << CH_CTL_L_SRC_INC_POS);

			desc->lli.ctl_lo = cpu_to_le32(reg);

			/* 			set_desc_src_master(desc);
			set_desc_dest_master(desc); */

			if (!first) {
				first = desc;
			} else {
				list_add_tail(&desc->xfer_list,
					      &first->xfer_list);
				write_desc_llp(prev, desc->vd.tx.phys | lms);
			}
			prev = desc;
			mem_addr += dlen;
			len -= dlen;
			total_len += dlen;
			/* 			dev_dbg(chan2dev(chan), "%s: desc
			   mem2dev: ctl_li: 0x%x ctl_lo: 0x%x src 0x%llx dst
			   0x%llx mem_addr 0x%llx sg_len 0x%x dlen: 0x%x len: %d
			   total_len: %d block: %d flag: %d",
					axi_chan_name(chan), desc->lli.ctl_hi,
			   desc->lli.ctl_lo, desc->lli.sar, desc->lli.dar,
			   mem_addr, sg_len, dlen, len, total_len,
			   desc->lli.block_ts_lo, flags);
			*/

			if (len)
				goto slave_sg_fromdev_fill_desc;
		}
		break;

	default:
		return NULL;
	}
	/* Total len of src/dest sg == 0, so no descriptor were allocated */
	if (unlikely(!first))
		return NULL;

	/*  TODO: Need Debug Line set first desc total len*/
	first->total_len = total_len;
	/* Set end-of-link to the last link descriptor of list */
	set_desc_last(desc);
	return vchan_tx_prep(&chan->vc, &first->vd, flags);

err_desc_get:
	axi_desc_put(first);
	return NULL;
}

/* Tested by I2S case */
static struct dma_async_tx_descriptor *
dma_chan_prep_cyclic(struct dma_chan *dchan, dma_addr_t buf_addr,
		     size_t buf_len, size_t period_len,
		     enum dma_transfer_direction direction, unsigned long flags)
{
	struct axi_dma_desc *first = NULL, *desc = NULL, *prev = NULL;
	// Get axi_dma_chan from dma_chan
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	// Get slave config from axi_dma_chan
	struct dma_slave_config *sconfig = &chan->dma_sconfig;
	size_t max_block_ts, offset;
	u32 reg;
	u32 burst_items;
	u32 reg_width;
	u32 mem_width;
	dma_addr_t reg_addr;

	u32 data_width;
	size_t total_len = 0;

	u8 lms = 0; /* Select AXI0 master for LLI fetching */

	max_block_ts = chan->chip->dw->hdata->block_size[chan->id];
	data_width = BIT(chan->chip->dw->hdata->m_data_width);
	/* TODO: set channel need add test case for multi transfer */
	chan->direction = direction;

	if (unlikely(buf_len % period_len)) {
		dev_err(chan2dev(chan),
			"Period length should be multiple of buffer length!\n");
		return NULL;
	}
	/* u32 periods = buf_len / period_len; */

	if (unlikely(!is_slave_direction(direction) || !period_len))
		return NULL;

	mem_width = __ffs(data_width | buf_addr | period_len);

	for (offset = 0; offset < buf_len; offset += period_len) {
		size_t dlen; /* desc len */
		/* Create transfer descriptor */
		desc = axi_desc_get(chan);
		desc->cyclic = true;
		if (unlikely(!desc))
			goto err_desc_get;

		switch (direction) {
			/* buf_addr -> reg_addr */
		case DMA_MEM_TO_DEV:
			reg_width = __ffs(sconfig->dst_addr_width);
			reg_addr = sconfig->dst_addr;
			burst_items = sconfig->dst_maxburst;
			if (unlikely((period_len >> sconfig->dst_addr_width) >
				     max_block_ts))
				goto err_desc_get;
			/* Memory address. */
			write_desc_sar(desc, buf_addr + offset);
			/* Device address */
			write_desc_dar(desc, reg_addr);

			/* calculate block size */
			desc->lli.block_ts_lo =
			    cpu_to_le32(bytes2block(max_block_ts, period_len,
						    reg_width, &dlen) -
					1);

			/* 0: Shadow Register content/LLI is invalid.1: Last
			 * Shadow Register/LLI is valid. */
			reg = CH_CTL_H_LLI_VALID | CH_CTL_H_IOC_BLK_TFR;

			/* Enable axi burst len as config */
			/* 				if
			   (chan->chip->dw->hdata->restrict_axi_burst_len) { u32
			   burst_len = chan->chip->dw->hdata->axi_rw_burst_len;

					reg |= (CH_CTL_H_ARLEN_EN |
						burst_len << CH_CTL_H_ARLEN_POS
			   | CH_CTL_H_AWLEN_EN | burst_len <<
			   CH_CTL_H_AWLEN_POS);

				} */

			/* set lli valid */
			desc->lli.ctl_hi = cpu_to_le32(reg);

			// lli low
			reg =
			    (burst_items
				 << CH_CTL_L_DST_MSIZE_POS | /* change to max
								burst */
			     burst_items
				 << CH_CTL_L_SRC_MSIZE_POS | /* change to device
							      */
			     reg_width << CH_CTL_L_DST_WIDTH_POS | /* dest */
			     reg_width << CH_CTL_L_SRC_WIDTH_POS | /* src */
			     DWAXIDMAC_CH_CTL_L_NOINC << CH_CTL_L_DST_INC_POS |
			     DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_SRC_INC_POS);

			desc->lli.ctl_lo = cpu_to_le32(reg);

			// set lli valid

			break;

		case DMA_DEV_TO_MEM:
			reg_width = __ffs(sconfig->src_addr_width);
			reg_addr = sconfig->src_addr;
			burst_items = sconfig->src_maxburst;
			if (unlikely((period_len >> sconfig->src_addr_width) >
				     max_block_ts))
				goto err_desc_get;
			/* Device address. */
			write_desc_sar(desc, reg_addr);
			/* Memory  address */
			write_desc_dar(desc, buf_addr + offset);

			/* calculate block size */
			desc->lli.block_ts_lo =
			    cpu_to_le32(bytes2block(max_block_ts, period_len,
						    reg_width, &dlen) -
					1);

			/*0: Shadow Register content/LLI is invalid.1: Last
			 * Shadow Register/LLI is valid.*/
			reg = CH_CTL_H_LLI_VALID | CH_CTL_H_IOC_BLK_TFR;

			/* Enable axi burst len as config */
			/* 				if
			   (chan->chip->dw->hdata->restrict_axi_burst_len) { u32
			   burst_len = chan->chip->dw->hdata->axi_rw_burst_len;

					reg |= (CH_CTL_H_ARLEN_EN |
						burst_len << CH_CTL_H_ARLEN_POS
			   | CH_CTL_H_AWLEN_EN | burst_len <<
			   CH_CTL_H_AWLEN_POS);

				} */

			// set lli valid
			desc->lli.ctl_hi = cpu_to_le32(reg);

			// lli low
			reg =
			    (burst_items
				 << CH_CTL_L_DST_MSIZE_POS | /* change to max
								burst */
			     burst_items
				 << CH_CTL_L_SRC_MSIZE_POS | /* change to device
							      */
			     reg_width << CH_CTL_L_DST_WIDTH_POS | /* dest */
			     reg_width << CH_CTL_L_SRC_WIDTH_POS | /* src */
			     DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_DST_INC_POS |
			     DWAXIDMAC_CH_CTL_L_NOINC << CH_CTL_L_SRC_INC_POS);

			desc->lli.ctl_lo = cpu_to_le32(reg);

			break;
		default:
			return NULL;
		}

		/* 		set_desc_src_master(desc);
		set_desc_dest_master(desc); */
		desc->len = dlen;

		if (!first) {
			first = desc;
		} else {
			list_add_tail(&desc->xfer_list, &first->xfer_list);
			write_desc_llp(prev, desc->vd.tx.phys | lms);
		}
		prev = desc;

		total_len += dlen;
		dev_vdbg(chan2dev(chan), "%s: desc cyclic: ctl_li: 0x%x"
		  " ctl_lo: 0x%x src 0x%llx dst 0x%llx buff 0x%llx offset: 0x%lx"
		  " buf_len: %ld total_len: %ld block: %d flag: %ld",
		  axi_chan_name(chan), desc->lli.ctl_hi, desc->lli.ctl_lo,
		  desc->lli.sar, desc->lli.dar, buf_addr, offset, buf_len,
		  total_len, desc->lli.block_ts_lo, flags);
	}



	/* Total len of src/dest sg == 0, so no descriptor were allocated */
	if (unlikely(!first))
		return NULL;

	/* Set first phy to  the last link descriptor of list */
	write_desc_llp(desc, first->vd.tx.phys | lms);
	/* Set total_len to buf_len.
	 */
	first->total_len = buf_len;

	return vchan_tx_prep(&chan->vc, &first->vd, flags);

err_desc_get:
	axi_desc_put(first);
	return NULL;
}

/* static struct dma_async_tx_descriptor *dma_chan_prep_cyclic_fake(
	struct dma_chan *dchan, dma_addr_t buf_addr, size_t buf_len,
	size_t period_len, enum dma_transfer_direction direction,
	unsigned long flags)
{
	printk(KERN_DEBUG "DW AXI DMAC"
					  "dma_chan_prep_cyclic_fake %s \n",
		   __FUNCTION__);
	return dma_chan_prep_dma_memcpy(dchan, buf_addr,
									buf_addr
+ 0x1000, 0x1000, flags); struct scatterlist sgl[4], *sg; sg_init_table(sgl, 4);
	unsigned int sg_len = 0;

	sg = &sgl[sg_len++];
	sg_dma_address(sg) = buf_addr;
	sg_dma_len(sg) = 0x1000;

	sg = &sgl[sg_len++];
	sg_dma_address(sg) = buf_addr + 0x1000;
	sg_dma_len(sg) = 0x1000;

	sg = &sgl[sg_len++];
	sg_dma_address(sg) = buf_addr + 0x2000;
	sg_dma_len(sg) = 0x1000;

	sg = &sgl[sg_len++];
	sg_dma_address(sg) = buf_addr + 0x2000;
	sg_dma_len(sg) = 0x1000;
	return dma_chan_prep_slave_sg(dchan,
								  sgl,
								  sg_len,
								  direction,
								  flags, NULL);
} */

struct dw_dma_filter_data {
	struct device_node *of_node;
	unsigned int slave_id;
	/* 	unsigned int chan_id; */
};

/* FIXME:  Complete this function with more filter*/
static bool dw_dma_filter_fn(struct dma_chan *dchan, void *param)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	struct dw_dma_filter_data *data = param;
	struct dw_axi_dma *dw = chan->chip->dw;

	if (NULL != dw->dma.dev->of_node) {
		if (dw->dma.dev->of_node != data->of_node)
		{
			return false;
		}
	}
	/*Set dma chan slave id */
	chan->fix_slave_id = true;
	chan->dma_sconfig.slave_id = data->slave_id;
	printk(KERN_DEBUG "DW AXI DMAC"
			  " %s %s slave id: %d \n",
	       __FUNCTION__, axi_chan_name(chan), data->slave_id);
	return true;
}

static struct of_dma_filter_info dw_dma_info = {
    .filter_fn = dw_dma_filter_fn,
};

struct dma_chan *dw_dma_of_xlate(struct of_phandle_args *dma_spec,
				 struct of_dma *ofdma)
{
	/* struct dw_dma *dw = ofdma->of_dma_data; */
	int count = dma_spec->args_count;
	struct of_dma_filter_info *info = ofdma->of_dma_data;
	struct dw_dma_filter_data data;
	printk(KERN_DEBUG "DW AXI DMAC"
			  "DMA channel translation requires args_count %d \n",
	       dma_spec->args_count);

	if (!info || !info->filter_fn)
		return NULL;

	if (count != 1)
		return NULL;

	data.of_node = ofdma->of_node;
	data.slave_id = dma_spec->args[0];
	printk(KERN_DEBUG "DW AXI DMAC"
			  "DMA(%s) channel translation args %d  \n",
	       data.of_node->full_name, dma_spec->args[0]);
	return dma_request_channel(info->dma_cap, info->filter_fn, &data);
}

static int dw_probe(struct platform_device *pdev)
{
	struct axi_dma_chip *chip;
	struct resource *mem;
	struct resource *mux_mem;
	struct dw_axi_dma *dw;
	struct dw_axi_dma_hcfg *hdata;
	u32 i;
	int ret;
	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dw = devm_kzalloc(&pdev->dev, sizeof(*dw), GFP_KERNEL);
	if (!dw)
		return -ENOMEM;

	hdata = devm_kzalloc(&pdev->dev, sizeof(*hdata), GFP_KERNEL);
	if (!hdata)
		return -ENOMEM;

	spin_lock_init(&chip->dev_lock);
	chip->dw = dw;
	chip->dev = &pdev->dev;
	chip->dw->hdata = hdata;

	chip->irq = platform_get_irq(pdev, 0);
	printk(KERN_DEBUG "DW AXI DMAC"
			  ": in dw_probe -- irq(%d)\n",
	       chip->irq);
	if (chip->irq < 0)
		return chip->irq;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->regs = devm_ioremap_resource(chip->dev, mem);
	if (IS_ERR(chip->regs))
		return PTR_ERR(chip->regs);

	if(axi_dma_reset(chip) != true){
		dev_err(&pdev->dev, "Reset timeout. \n");
		return EINVAL;
	}

	if (pdev->num_resources == 3) {
		mux_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		chip->mux_regs = devm_ioremap_resource(chip->dev, mux_mem);
		chip->dw->hdata->dma_mux_enable = true;

		printk(KERN_DEBUG "DW AXI DMAC"
				  ": enable mux irq(%d) num(%d) vaddr(0x%p) "
				  "paddr(0x%llx) vaddr(0x%p) paddr(0x%llx)\n",
		       chip->irq, pdev->num_resources, chip->regs, mem->start,
		       chip->mux_regs, mux_mem->start);
	} else if (pdev->num_resources == 2) {
		chip->dw->hdata->dma_mux_enable = false;
		printk(KERN_DEBUG "DW AXI DMAC"
				  ": without mux irq(%d) num(%d) vaddr(0x%p) "
				  "paddr(0x%llx) \n",
		       chip->irq, pdev->num_resources, chip->regs, mem->start);
	} else {
		dev_err(&pdev->dev, "invalid num_resources: %i\n",
			pdev->num_resources);
	}

	chip->core_clk = devm_clk_get(chip->dev, "core-clk");
	if (IS_ERR(chip->core_clk))
		return PTR_ERR(chip->core_clk);

	chip->cfgr_clk = devm_clk_get(chip->dev, "cfgr-clk");
	if (IS_ERR(chip->cfgr_clk))
		return PTR_ERR(chip->cfgr_clk);

	ret = parse_device_properties(chip);
	if (ret)
		return ret;

	dw->chan = devm_kcalloc(chip->dev, hdata->nr_channels,
				sizeof(*dw->chan), GFP_KERNEL);
	if (!dw->chan)
		return -ENOMEM;

	ret = devm_request_irq(chip->dev, chip->irq, dw_axi_dma_interrupt,
			       IRQF_SHARED, KBUILD_MODNAME, chip);
	if (ret)
		return ret;

	/* Lli address must be aligned to a 64-byte boundary */
	dw->desc_pool = dmam_pool_create(KBUILD_MODNAME, chip->dev,
					 sizeof(struct axi_dma_desc), 64, 0);
	if (!dw->desc_pool) {
		dev_err(chip->dev, "No memory for descriptors dma pool\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&dw->dma.channels);
	for (i = 0; i < hdata->nr_channels; i++) {
		struct axi_dma_chan *chan = &dw->chan[i];
		chan->direction = DMA_TRANS_NONE;
		chan->chip = chip;
		chan->id = i;
		chan->chan_regs =
		    chip->regs + COMMON_REG_LEN + i * CHAN_REG_LEN;
		chan->fix_slave_id = false;
		/* Set MUX base address */
		if (chip->dw->hdata->dma_mux_enable == true) {
			chan->chan_mux_regs =
			    chip->mux_regs + i * CHAN_MUX_REG_LEN;
		}
		atomic_set(&chan->descs_allocated, 0);

		chan->vc.desc_free = vchan_desc_put;
		vchan_init(&chan->vc, &dw->dma);
	}

	/* Set capabilities */
	dma_cap_set(DMA_MEMCPY, dw->dma.cap_mask);
	dma_cap_set(DMA_SLAVE, dw->dma.cap_mask);
	dma_cap_set(DMA_CYCLIC, dw->dma.cap_mask);

	/* DMA capabilities */
	dw->dma.chancnt = hdata->nr_channels;
	dw->dma.src_addr_widths = AXI_DMA_BUSWIDTHS;
	dw->dma.dst_addr_widths = AXI_DMA_BUSWIDTHS;
	dw->dma.directions =
	    BIT(DMA_DEV_TO_MEM) | BIT(DMA_MEM_TO_DEV) | BIT(DMA_MEM_TO_MEM);
	dw->dma.residue_granularity =
	    DMA_RESIDUE_GRANULARITY_BURST; // DMA_RESIDUE_GRANULARITY_BURST;
	// DMA_RESIDUE_GRANULARITY_DESCRIPTOR;

	dw->dma.dev = chip->dev;
	dw->dma.device_tx_status = dma_chan_tx_status;
	dw->dma.device_issue_pending = dma_chan_issue_pending;
	dw->dma.device_terminate_all = dma_chan_terminate_all;
	dw->dma.device_pause = dma_chan_pause;
	dw->dma.device_resume = dma_chan_resume;

	dw->dma.device_alloc_chan_resources = dma_chan_alloc_chan_resources;
	dw->dma.device_free_chan_resources = dma_chan_free_chan_resources;

	dw->dma.device_prep_dma_memcpy = dma_chan_prep_dma_memcpy;
	dw->dma.device_synchronize = dw_axi_dma_synchronize;
	dw->dma.device_prep_slave_sg = dma_chan_prep_slave_sg;
	dw->dma.device_prep_dma_cyclic = dma_chan_prep_cyclic;

	dw->dma.device_config = dma_chan_config;

	platform_set_drvdata(pdev, chip);

	pm_runtime_enable(chip->dev);

	/*
	 * We can't just call pm_runtime_get here instead of
	 * pm_runtime_get_noresume + axi_dma_resume because we need
	 * driver to work also without Runtime PM.
	 */
	pm_runtime_get_noresume(chip->dev);
	ret = axi_dma_resume(chip);
	if (ret < 0)
		goto err_pm_disable;

	pm_runtime_put(chip->dev);

	// ret = dmaenginem_async_device_register(&dw->dma);
	ret = dma_async_device_register(&dw->dma);
	if (ret)
		goto err_dma_register;

	axi_dma_hw_init(chip);
	dev_info(chip->dev, "DesignWare AXI DMA Controller, %d channels\n",
		 dw->hdata->nr_channels);

	if (pdev->dev.of_node) {
		dev_info(chip->dev, "Register DesignWare AXI DMA Controller\n");
		ret = of_dma_controller_register(pdev->dev.of_node,
						 dw_dma_of_xlate, &dw_dma_info);
		if (ret)
			dev_err(&pdev->dev,
				"could not register of_dma_controller\n");
	}
	return 0;

err_dma_register:
	free_irq(chip->irq, dw);

err_pm_disable:
	pm_runtime_disable(chip->dev);
	return ret;
}

static int dw_remove(struct platform_device *pdev)
{
	struct axi_dma_chip *chip = platform_get_drvdata(pdev);
	struct dw_axi_dma *dw = chip->dw;
	struct axi_dma_chan *chan, *_chan;
	u32 i;
	unsigned long flags;

	/* Enable clk before accessing to registers */
	clk_prepare_enable(chip->cfgr_clk);
	clk_prepare_enable(chip->core_clk);
	axi_dma_irq_disable(chip);
	spin_lock_irqsave(&chip->dev_lock,flags);
	for (i = 0; i < dw->hdata->nr_channels; i++) {
		axi_chan_disable(&chip->dw->chan[i]);
		axi_chan_irq_disable(&chip->dw->chan[i], DWAXIDMAC_IRQ_ALL);
	}
	axi_dma_disable(chip);
	spin_unlock_irqrestore(&chip->dev_lock,flags);
	pm_runtime_disable(chip->dev);
	axi_dma_suspend(chip);

	devm_free_irq(chip->dev, chip->irq, chip);

	list_for_each_entry_safe(chan, _chan, &dw->dma.channels,
				 vc.chan.device_node)
	{
		list_del(&chan->vc.chan.device_node);
		tasklet_kill(&chan->vc.task);
	}

	return 0;
}

static void dw_shutdown(struct platform_device *pdev)
{
	struct axi_dma_chip *chip = platform_get_drvdata(pdev);
	axi_dma_suspend(chip);
}
#ifdef CONFIG_PM_SLEEP

static int axi_dma_suspend_late(struct device *dev)
{
	struct axi_dma_chip *chip = dev_get_drvdata(dev);
	return axi_dma_suspend(chip);
}

static int axi_dma_resume_early(struct device *dev)
{
	struct axi_dma_chip *chip = dev_get_drvdata(dev);
	return axi_dma_resume(chip);
}

#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops dw_axi_dma_pm_ops = {
	SET_RUNTIME_PM_OPS(axi_dma_runtime_suspend, axi_dma_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(axi_dma_suspend_late,
				     axi_dma_resume_early)};

static const struct of_device_id dw_dma_of_id_table[] = {
    {.compatible = "snps,axi-dma-1.01a"}, {}};
MODULE_DEVICE_TABLE(of, dw_dma_of_id_table);

static struct platform_driver dw_driver = {
	.probe = dw_probe,
	.remove = dw_remove,
	.shutdown = dw_shutdown,
	.driver =
	{
		.name = KBUILD_MODNAME,
		.of_match_table = of_match_ptr(dw_dma_of_id_table),
		.pm = &dw_axi_dma_pm_ops,
	},
};
module_platform_driver(dw_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Synopsys DesignWare AXI DMA Controller platform driver");
MODULE_AUTHOR("Eugeniy Paltsev <Eugeniy.Paltsev@synopsys.com>");
