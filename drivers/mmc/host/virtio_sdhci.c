/*
 *  linux/drivers/mmc/host/virtio_sdhci.c
 *  Secure Digital Host Controller Virtio Interface driver
 */

#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/swiotlb.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>

#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/slot-gpio.h>

#include <asm/cacheflush.h>
#include <asm/fixmap.h>
#include <linux/dma-contiguous.h>
#include "sdhci.h"
#include <linux/virtio.h>
#include <linux/virtio_mmc.h>

#define MAX_TUNING_LOOP 140
#define DWC_ADMA_BOUNDARY_WORKAROUND 1
#define DWC_ADMA_BOUNDARY_SIZE (0x8000000)
#define SDHCI_ARG2_STUFF GENMASK(31, 16)

#define DEBUG 0

static unsigned int debug_quirks = 0;
static unsigned int debug_quirks2;

u8 hw_part_type = 0;
u8 ext_csd[512] = {0};
unsigned int from = 0;
unsigned int to = 0;

#define TX_COUNT	16u
#define RX_COUNT	16u
#define BUF_SIZE	PAGE_SIZE
#define CACHE_LINE	64

#define VIRTIO_MMC_RES_ID RES_MSHC_SD1
#define VIRTIO_MMC_RESP_OK (0x00000900)

#define RX_SIZE 256
#define TX_SIZE 128

enum {
	VQ_RX = 0,
	VQ_TX,
	VQ_COUNT,
};

struct virtio_sdhci_host {
	struct sdhci_host *host;
	/* The virtio device we're associated with */
	struct virtio_device *vdev;
	struct virtqueue *tx;
	struct virtqueue *rx;
	spinlock_t	lock;
	struct completion req_comp;
	unsigned int done;
	struct virtio_mmc_config cfg;
	struct virtio_mmc_req reqs[VIRTIO_MMC_VIRTQUEUE_SIZE];
	uint16_t last[VQ_COUNT];
};

static void virtio_sdhci_rx_callback(struct virtqueue *vq)
{
	struct virtio_sdhci_host *vhost = vq->vdev->priv;
	void *buf = NULL;
	struct virtio_mmc_req *req;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vhost->lock, flags);
	buf = virtqueue_get_buf(vhost->rx, &len);
	if (!buf) {
		pr_err("vmmc rx callback buf is NULL\n");
		goto out;
	}
	__inval_dcache_area(buf, sizeof(struct virtio_mmc_req));
	req = (struct virtio_mmc_req *)buf;

	__inval_dcache_area(req->cmd, sizeof(struct virtio_mmc_cmd_req));
	if (vhost->last[VQ_RX] != req->cmd->id)
		printk("req[%d] is err: %lld\n", vhost->last[VQ_RX], req->cmd->id);

	req->cmd->id = 0;
out:
	spin_unlock_irqrestore(&vhost->lock, flags);
}

static void virtio_sdhci_fill_rx(struct virtio_sdhci_host *vhost)
{
	struct virtio_mmc_req *req;
	struct scatterlist sg;
	unsigned long flags;
	unsigned int i, size;

	size = min(RX_COUNT, virtqueue_get_vring_size(vhost->rx));
#if DEBUG
	printk("size = %d\n", virtqueue_get_vring_size(vhost->rx));
#endif

	spin_lock_irqsave(&vhost->lock, flags);
	for (i = 0; i < size; i++) {
		if (!vhost->reqs[i].cmd->id)
			break;
	}
	if (i >= size) {
		pr_err("mmc vq reqs is full\n");
		spin_unlock_irqrestore(&vhost->lock, flags);
		return;
	}
	vhost->last[VQ_RX] = i + 1;

	req = &vhost->reqs[i];
#if DEBUG
	printk("req[%d]: %llx %llx %llx\n", i, __pa(req->cmd), __pa(req->req_data), __pa(req->cmd_resp));
#endif
	memset(req->cmd, 0, sizeof(struct virtio_mmc_cmd_req));
	memset(req->cmd_resp, 0, sizeof(struct virtio_mmc_cmd_resp));
	req->cmd->id = i + 1;

	__flush_dcache_area(req->cmd, DIV_ROUND_UP(sizeof(struct virtio_mmc_cmd_req), CACHE_LINE));
	__flush_dcache_area(vhost->reqs[i].cmd, DIV_ROUND_UP(sizeof(struct virtio_mmc_cmd_req), CACHE_LINE));
	__flush_dcache_area(vhost->reqs[i].cmd_resp, DIV_ROUND_UP(sizeof(struct virtio_mmc_cmd_resp), CACHE_LINE));
	sg_init_one(&sg, vhost->reqs[i].cmd_resp, sizeof(struct virtio_mmc_cmd_resp));
	virtqueue_add_inbuf(vhost->rx, &sg, 1, &vhost->reqs[i], GFP_ATOMIC);
	spin_unlock_irqrestore(&vhost->lock, flags);
	virtqueue_kick(vhost->rx);
}

static ssize_t test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct virtio_device *vdev = container_of(dev, struct virtio_device, dev);
	struct virtio_sdhci_host *vhost = vdev->priv;
	unsigned long flags;
	struct virtio_mmc_req *req;
	struct virtio_mmc_sdhci_cmd_desc *desc;
	struct scatterlist hdr, mid, last, *sg[3];
	int i = 0;

	spin_lock_irqsave(&vhost->lock, flags);
	for (i = 0; i < VIRTIO_MMC_VIRTQUEUE_SIZE; i++) {
		if (!vhost->reqs[i].cmd->id)
			break;
	}
	if (i >= VIRTIO_MMC_VIRTQUEUE_SIZE) {
		pr_err("mmc vq reqs is full\n");
		spin_unlock_irqrestore(&vhost->lock, flags);
		return 0;
	}
	vhost->last[VQ_TX] = i + 1;

	req = &vhost->reqs[i];

#if DEBUG
	printk("req[%d]: %x %x %x\n", i, __pa(req->cmd), __pa(req->req_data), __pa(req->cmd_resp));
#endif
	memset(req->cmd, 0, sizeof(struct virtio_mmc_cmd_req));
	memset(ext_csd, 0, 512);
	memset(req->cmd_resp, 0, sizeof(struct virtio_mmc_cmd_resp));

	req->cmd->id = 0;
	req->cmd->id = i + 1;
	req->cmd->priority = 0;
	req->cmd->cdb_type = VIRTIO_MMC_CDB_SDHCI;

	desc = (struct virtio_mmc_sdhci_cmd_desc *)(req->cmd->CDB);

	desc->sector = 512;
	desc->num_sectors = 1;
	desc->cmd_index = 8;
	desc->argument = 0;
	desc->cmd_timeout = 0xe;
	desc->resp_type = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	desc->cmd_retry = 0;
	desc->cmd23_support = 1;
	desc->buf_mode = VIRTIO_MMC_SDHCI_BUF_NORMAL;

	req->req_data = ext_csd;
	req->req_data_len = 512;

	__flush_dcache_area(req->cmd, DIV_ROUND_UP(sizeof(struct virtio_mmc_cmd_req), CACHE_LINE));
	__flush_dcache_area(req->req_data, DIV_ROUND_UP(req->req_data_len, CACHE_LINE));
	__flush_dcache_area(req->cmd_resp, DIV_ROUND_UP(sizeof(struct virtio_mmc_cmd_resp), CACHE_LINE));

	sg_init_one(&hdr, req->cmd, sizeof(struct virtio_mmc_cmd_req));
	sg_init_one(&mid, req->req_data, 8);
	sg_init_one(&last, req->cmd_resp, sizeof(struct virtio_mmc_cmd_resp));

	sg[0] = &hdr;
	sg[1] = &mid;
	sg[2] = &last;

	virtqueue_add_sgs(vhost->tx, sg, 2, 1, &vhost->reqs[i], GFP_ATOMIC);
	spin_unlock_irqrestore(&vhost->lock, flags);
	virtqueue_kick(vhost->tx);

	return len;
}
DEVICE_ATTR_WO(test);

void vsdhci_dumpregs(struct virtio_sdhci_host *vhost)
{
}

static char *vsdhci_kmap_atomic(struct scatterlist *sg, unsigned long *flags)
{
	local_irq_save(*flags);
	return kmap_atomic(sg_page(sg)) + sg->offset;
}

static void vsdhci_kunmap_atomic(void *buffer, unsigned long *flags)
{
	kunmap_atomic(buffer);
	local_irq_restore(*flags);
}

void vsdhci_adma_write_desc(struct sdhci_host *host, void **desc,
		dma_addr_t addr, int len, unsigned int cmd)
{
	struct sdhci_adma2_64_desc *dma_desc = *desc;

	/* 32-bit and 64-bit descriptors have these members in same position */
	dma_desc->cmd = cpu_to_le16(((len & 0x3ff0000) >> 10) | (cmd & 0x3f));
	dma_desc->len = cpu_to_le16(len);
	dma_desc->addr_lo = cpu_to_le32(lower_32_bits(addr));

	if (host->flags & SDHCI_USE_64_BIT_DMA)
		dma_desc->addr_hi = cpu_to_le32(upper_32_bits(addr));

#if DEBUG
	printk("desc: cmd = %x, len = %x, addr_lo = %x, addr_hi = %x\n",
			dma_desc->cmd, dma_desc->len, dma_desc->addr_lo, dma_desc->addr_hi);
#endif
	*desc += host->desc_sz;
}

static inline void __vsdhci_adma_write_desc(struct sdhci_host *host,
		void **desc, dma_addr_t addr,
		int len, unsigned int cmd)
{
	vsdhci_adma_write_desc(host, desc, addr, len, cmd);
}

static void vsdhci_adma_mark_end(void *desc)
{
	struct sdhci_adma2_64_desc *dma_desc = desc;

	/* 32-bit and 64-bit descriptors have 'cmd' in same position */
	dma_desc->cmd |= cpu_to_le16(ADMA2_END);
}

#ifdef DWC_ADMA_BOUNDARY_WORKAROUND
static dma_addr_t _calc_adma_boundary(dma_addr_t addr, dma_addr_t boundary)
{
	return ((addr + boundary) & ~(boundary - 1));
}
#endif

static void vsdhci_adma_table_pre(struct sdhci_host *host,
		struct mmc_data *data, int sg_count)
{
	struct scatterlist *sg;
	unsigned long flags;
	dma_addr_t addr, align_addr;
	void *desc, *align;
	char *buffer;
	int len, offset, i;
#ifdef DWC_ADMA_BOUNDARY_WORKAROUND
	dma_addr_t boundary_addr;
#endif
	/*
	 * The spec does not specify endianness of descriptor table.
	 * We currently guess that it is LE.
	 */

	host->sg_count = sg_count;

	desc = host->adma_table;
	align = host->align_buffer;

	align_addr = host->align_addr;

	for_each_sg(data->sg, sg, host->sg_count, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);

		/*
		 * The SDHCI specification states that ADMA addresses must
		 * be 32-bit aligned. If they aren't, then we use a bounce
		 * buffer for the (up to three) bytes that screw up the
		 * alignment.
		 */
		offset = (SDHCI_ADMA2_ALIGN - (addr & SDHCI_ADMA2_MASK)) &
			SDHCI_ADMA2_MASK;
		if (offset) {
			if (data->flags & MMC_DATA_WRITE) {
				buffer = vsdhci_kmap_atomic(sg, &flags);
				memcpy(align, buffer, offset);
				vsdhci_kunmap_atomic(buffer, &flags);
			}

			/* tran, valid */
			__vsdhci_adma_write_desc(host, &desc, align_addr,
					offset, ADMA2_TRAN_VALID);

			BUG_ON(offset > 0x4000000);

			align += SDHCI_ADMA2_ALIGN;
			align_addr += SDHCI_ADMA2_ALIGN;

			addr += offset;
			len -= offset;
		}

#ifdef DWC_ADMA_BOUNDARY_WORKAROUND
		offset = 0;
		boundary_addr = _calc_adma_boundary(addr, DWC_ADMA_BOUNDARY_SIZE);
		if ((addr + len) > boundary_addr)
			offset = boundary_addr - addr;
		if (offset) {
			/* tran, valid */
			__vsdhci_adma_write_desc(host, &desc, addr,
					offset, ADMA2_TRAN_VALID);

			BUG_ON(offset > 0x4000000);

			addr += offset;
			len -= offset;
		}
#endif

		BUG_ON(len > 0x4000000);

		/* tran, valid */
		if (len)
			__vsdhci_adma_write_desc(host, &desc, addr, len,
					ADMA2_TRAN_VALID);

		/*
		 * If this triggers then we have a calculation bug
		 * somewhere. :/
		 */
		WARN_ON((desc - host->adma_table) >= host->adma_table_sz);
	}

	if (host->quirks & SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC) {
		/* Mark the last descriptor as the terminating descriptor */
		if (desc != host->adma_table) {
			desc -= host->desc_sz;
			vsdhci_adma_mark_end(desc);
		}
	} else {
		/* Add a terminating entry - nop, end, valid */
		__vsdhci_adma_write_desc(host, &desc, 0, 0, ADMA2_NOP_END_VALID);
	}
}

static void vsdhci_adma_table_post(struct sdhci_host *host,
		struct mmc_data *data)
{
	struct scatterlist *sg;
	int i, size;
	void *align;
	char *buffer;
	unsigned long flags;

	if (data->flags & MMC_DATA_READ) {
		bool has_unaligned = false;

#if DEBUG
		printk("%s %d\n", __func__, __LINE__);
#endif
		/* Do a quick scan of the SG list for any unaligned mappings */
		for_each_sg(data->sg, sg, host->sg_count, i)
			if (sg_dma_address(sg) & SDHCI_ADMA2_MASK) {
				has_unaligned = true;
				break;
			}

		if (has_unaligned) {
			dma_sync_sg_for_cpu(mmc_dev(host->mmc), data->sg,
					data->sg_len, DMA_FROM_DEVICE);

			align = host->align_buffer;

			for_each_sg(data->sg, sg, host->sg_count, i) {
				if (sg_dma_address(sg) & SDHCI_ADMA2_MASK) {
					size = SDHCI_ADMA2_ALIGN -
						(sg_dma_address(sg) & SDHCI_ADMA2_MASK);

					buffer = vsdhci_kmap_atomic(sg, &flags);
					memcpy(buffer, align, size);
					vsdhci_kunmap_atomic(buffer, &flags);

					align += SDHCI_ADMA2_ALIGN;
				}
			}
		}
	}
}

static bool vsdhci_needs_reset(struct sdhci_host *host, struct mmc_request *mrq)
{
	return (!(host->flags & SDHCI_DEVICE_DEAD) &&
			((mrq->cmd && mrq->cmd->error) ||
			 (mrq->sbc && mrq->sbc->error) ||
			 (mrq->data && mrq->data->stop && mrq->data->stop->error) ||
			 (host->quirks & SDHCI_QUIRK_RESET_AFTER_REQUEST)));
}

static void vsdhci_init(struct virtio_sdhci_host *vhost, int soft)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_host *mmc = host->mmc;

	if (soft) {
		/* force clock reconfiguration */
		host->clock = 0;
		mmc->ops->set_ios(mmc, &mmc->ios);
	}
}

void vsdhci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct virtio_sdhci_host *vhost = mmc_priv(mmc);
	struct sdhci_host *host = vhost->host;

	if (ios->power_mode == MMC_POWER_UNDEFINED)
		return;

	if (host->flags & SDHCI_DEVICE_DEAD)
		return;

	if (!ios->clock || ios->clock != host->clock) {
		mmc->actual_clock = ios->clock;
		host->clock = ios->clock;
	}

	mmiowb();
}

static inline bool vsdhci_data_line_cmd(struct mmc_command *cmd)
{
	return cmd->data || cmd->flags & MMC_RSP_BUSY;
}

static void vsdhci_mod_timer(struct sdhci_host *host, struct mmc_request *mrq,
		unsigned long timeout)
{
	if (vsdhci_data_line_cmd(mrq->cmd))
		mod_timer(&host->data_timer, timeout);
	else
		mod_timer(&host->timer, timeout);
}

static void vsdhci_del_timer(struct sdhci_host *host, struct mmc_request *mrq)
{
	if (vsdhci_data_line_cmd(mrq->cmd))
		del_timer(&host->data_timer);
	else
		del_timer(&host->timer);
}

static bool vsdhci_request_done(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	struct virtio_mmc_req *req;
	unsigned long flags;
	struct mmc_request *mrq;
	int i;

	spin_lock_irqsave(&vhost->lock, flags);

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		mrq = host->mrqs_done[i];
		if (mrq)
			break;
	}

	if (!mrq) {
		spin_unlock_irqrestore(&vhost->lock, flags);
		return true;
	}

	req = &vhost->reqs[vhost->last[VQ_TX] - 1];

	vsdhci_del_timer(host, mrq);

	/*
	 * Always unmap the data buffers if they were mapped by
	 * sdhci_prepare_data() whenever we finish with a request.
	 * This avoids leaking DMA mappings on error.
	 */
	if (host->flags & SDHCI_REQ_USE_DMA) {
		struct mmc_data *data = mrq->data;

		__inval_dcache_area(req->req_data, DIV_ROUND_UP(host->align_buffer_sz + host->adma_table_sz, CACHE_LINE));
		if (data && data->host_cookie == COOKIE_MAPPED) {
			/* Unmap the raw data */
			dma_unmap_sg(mmc_dev(host->mmc), data->sg,
					data->sg_len,
					mmc_get_dma_dir(data));
			data->host_cookie = COOKIE_UNMAPPED;
		}
	}

	/*
	 * The controller needs a reset of internal state machines
	 * upon error conditions.
	 */
	if (vsdhci_needs_reset(host, mrq)) {
		/*
		 * Do not finish until command and data lines are available for
		 * reset. Note there can only be one other mrq, so it cannot
		 * also be in mrqs_done, otherwise host->cmd and host->data_cmd
		 * would both be null.
		 */
		if (host->cmd || host->data_cmd) {
			spin_unlock_irqrestore(&vhost->lock, flags);
			return true;
		}

		host->pending_reset = false;
	}

	host->mrqs_done[i] = NULL;

	req->cmd->id = 0;
	mmiowb();
	spin_unlock_irqrestore(&vhost->lock, flags);

#if DEBUG
	printk("%s %d\n", __func__, __LINE__);
#endif
	complete(&vhost->req_comp);
	mmc_request_done(host->mmc, mrq);

	return false;
}

static void vsdhci_tasklet_finish(unsigned long param)
{
	struct virtio_sdhci_host *vhost = (struct virtio_sdhci_host *)param;

	while (!vsdhci_request_done(vhost))
		;
}

static void __vsdhci_finish_mrq(struct virtio_sdhci_host *vhost, struct mmc_request *mrq)
{
	int i;
	struct sdhci_host *host = vhost->host;

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (host->mrqs_done[i] == mrq) {
			WARN_ON(1);
			return;
		}
	}

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (!host->mrqs_done[i]) {
			host->mrqs_done[i] = mrq;
			break;
		}
	}

	WARN_ON(i >= SDHCI_MAX_MRQS);

	tasklet_schedule(&host->finish_tasklet);
}

static void vsdhci_finish_mrq(struct virtio_sdhci_host *vhost, struct mmc_request *mrq)
{
	struct sdhci_host *host = vhost->host;

	if (host->cmd && host->cmd->mrq == mrq)
		host->cmd = NULL;

	if (host->data_cmd && host->data_cmd->mrq == mrq)
		host->data_cmd = NULL;

	if (host->data && host->data->mrq == mrq)
		host->data = NULL;

	if (vsdhci_needs_reset(host, mrq))
		host->pending_reset = true;

	__vsdhci_finish_mrq(vhost, mrq);
}

static void vsdhci_finish_data(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_command *data_cmd = host->data_cmd;
	struct mmc_data *data = host->data;

	host->data = NULL;
	host->data_cmd = NULL;

	if (data_cmd->opcode == MMC_SEND_EXT_CSD) {
		struct scatterlist *sg;
		int len, i;
		dma_addr_t addr;
		u8 *buffer;
		struct virtio_mmc_req *req;
		unsigned long flags;

		req = &vhost->reqs[vhost->last[VQ_TX] - 1];
		__inval_dcache_area(req->req_data, 512);
		host->sg_count = dma_map_sg(mmc_dev(host->mmc),
				data->sg, data->sg_len,
				mmc_get_dma_dir(data));

#if DEBUG
		printk("%s %d\n", __func__, __LINE__);
#endif
		for_each_sg(data->sg, sg, host->sg_count, i) {
			addr = sg_dma_address(sg);
			len = sg_dma_len(sg);
			buffer = vsdhci_kmap_atomic(sg, &flags);
			memcpy(buffer, req->req_data, len);
			vsdhci_kunmap_atomic(buffer, &flags);
		}
	} else {
		if ((host->flags & (SDHCI_REQ_USE_DMA | SDHCI_USE_ADMA)) ==
				(SDHCI_REQ_USE_DMA | SDHCI_USE_ADMA))
			vsdhci_adma_table_post(host, data);
	}

	/*
	 * The specification states that the block count register must
	 * be updated, but it does not specify at what point in the
	 * data flow. That makes the register entirely useless to read
	 * back so we have to assume that nothing made it to the card
	 * in the event of an error.
	 */
	if (data->error)
		data->bytes_xfered = 0;
	else
		data->bytes_xfered = data->blksz * data->blocks;

	/*
	 * Need to send CMD12 if -
	 * a) open-ended multiblock transfer (no CMD23)
	 * b) error in multiblock transfer
	 */
	if (data->stop &&
			(data->error ||
			 !data->mrq->sbc)) {
		/*
		 * 'cap_cmd_during_tfr' request must not use the command line
		 * after mmc_command_done() has been called. It is upper layer's
		 * responsibility to send the stop command if required.
		 */
		if (data->mrq->cap_cmd_during_tfr) {
			vsdhci_finish_mrq(vhost, data->mrq);
		} else {
			/* Avoid triggering warning in sdhci_send_command() */
			host->cmd = NULL;
		}
	} else {
		vsdhci_finish_mrq(vhost, data->mrq);
	}
}

static inline void _mmc_complete_cmd(struct mmc_request *mrq)
{
	if (mrq->cap_cmd_during_tfr && !completion_done(&mrq->cmd_completion))
		complete_all(&mrq->cmd_completion);
}

void _mmc_command_done(struct mmc_host *host, struct mmc_request *mrq)
{
	if (!mrq->cap_cmd_during_tfr)
		return;

	_mmc_complete_cmd(mrq);

	pr_debug("%s: cmd done, tfr ongoing (CMD%u)\n",
			mmc_hostname(host), mrq->cmd->opcode);
}

static void vsdhci_finish_command(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_command *cmd = host->cmd;
	struct virtio_mmc_req *req;

	host->cmd = NULL;

	req = &vhost->reqs[vhost->last[VQ_TX] - 1];
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[0] = req->cmd_resp->resp[3];
			cmd->resp[1] = req->cmd_resp->resp[2];
			cmd->resp[2] = req->cmd_resp->resp[1];
			cmd->resp[3] = req->cmd_resp->resp[0];
		} else {
			cmd->resp[0] = req->cmd_resp->resp[0];
			cmd->resp[1] = req->cmd_resp->resp[1];
			cmd->resp[2] = req->cmd_resp->resp[2];
			cmd->resp[3] = req->cmd_resp->resp[3];
		}
#if DEBUG
		printk("resp: %x %x %x %x\n",
				cmd->resp[0], cmd->resp[1], cmd->resp[2], cmd->resp[3]);
#endif
	}

	if (cmd->mrq->cap_cmd_during_tfr && cmd == cmd->mrq->cmd)
		_mmc_command_done(host->mmc, cmd->mrq);

	/*
	 * The host can send and interrupt when the busy state has
	 * ended, allowing us to wait without wasting CPU cycles.
	 * The busy signal uses DAT0 so this is similar to waiting
	 * for data to complete.
	 *
	 * Note: The 1.0 specification is a bit ambiguous about this
	 *       feature so there might be some problems with older
	 *       controllers.
	 */
	if (cmd->flags & MMC_RSP_BUSY) {
		if (cmd->data) {
			pr_debug("Cannot wait for busy signal when also doing a data transfer");
		} else if (!(host->quirks & SDHCI_QUIRK_NO_BUSY_IRQ) &&
				cmd == host->data_cmd && cmd->opcode != MMC_SWITCH && cmd->opcode != MMC_ERASE) {
#if DEBUG
			printk("%s %d\n", __func__, __LINE__);
#endif
			/* Command complete before busy is ended */
			return;
		}
	}

	/* Finished CMD23, now send actual command. */
	if (cmd != cmd->mrq->sbc) {
		/* Processed actual command. */
		if (host->data && host->data_early)
			vsdhci_finish_data(vhost);

		if (!cmd->data)
			vsdhci_finish_mrq(vhost, cmd->mrq);
	}
}

static int vsdhci_pre_dma_transfer(struct sdhci_host *host,
		struct mmc_data *data, int cookie)
{
	int sg_count;

	/*
	 * If the data buffers are already mapped, return the previous
	 * dma_map_sg() result.
	 */
	if (data->host_cookie == COOKIE_PRE_MAPPED)
		return data->sg_count;

	/* Just access the data directly from memory */
	sg_count = dma_map_sg(mmc_dev(host->mmc),
			data->sg, data->sg_len,
			mmc_get_dma_dir(data));

	if (sg_count == 0)
		return -ENOSPC;

	data->sg_count = sg_count;
	data->host_cookie = cookie;

	return sg_count;
}

static void vsdhci_cmd_irq(struct virtio_sdhci_host *vhost, u32 intmask, u32 *intmask_p)
{
	struct sdhci_host *host = vhost->host;

	if (!host->cmd) {
		/*
		 * SDHCI recovers from errors by resetting the cmd and data
		 * circuits.  Until that is done, there very well might be more
		 * interrupts, so ignore them in that case.
		 */
		if (host->pending_reset)
			return;
		pr_err("%s: Got command interrupt 0x%08x even though no command operation was in progress.\n",
				mmc_hostname(host->mmc), (unsigned)intmask);
		vsdhci_dumpregs(vhost);
		return;
	}

	if (intmask & (SDHCI_INT_TIMEOUT | SDHCI_INT_CRC |
				SDHCI_INT_END_BIT | SDHCI_INT_INDEX)) {
		if (intmask & SDHCI_INT_TIMEOUT)
			host->cmd->error = -ETIMEDOUT;
		else
			host->cmd->error = -EILSEQ;

		/* Treat data command CRC error the same as data CRC error */
		if (host->cmd->data &&
				(intmask & (SDHCI_INT_CRC | SDHCI_INT_TIMEOUT)) ==
				SDHCI_INT_CRC) {
			host->cmd = NULL;
			if (!(intmask & SDHCI_INT_DATA_CRC))
				*intmask_p |= SDHCI_INT_DATA_CRC;
			return;
		}

		vsdhci_finish_mrq(vhost, host->cmd->mrq);
		return;
	}

	/* Handle auto-CMD23 error */
	if (intmask & SDHCI_INT_AUTO_CMD_ERR) {
		struct mmc_request *mrq = host->cmd->mrq;
		u16 auto_cmd_status = 0;
		int err = (auto_cmd_status & SDHCI_AUTO_CMD_TIMEOUT) ?
			-ETIMEDOUT :
			-EILSEQ;

		if (mrq->sbc && (host->flags & SDHCI_AUTO_CMD23)) {
			mrq->sbc->error = err;
			vsdhci_finish_mrq(vhost, mrq);
			return;
		}
	}

	if (intmask & SDHCI_INT_RESPONSE)
		vsdhci_finish_command(vhost);
}

static void vsdhci_adma_show_error(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	void *desc = host->adma_table;

	vsdhci_dumpregs(vhost);

	while (true) {
		struct sdhci_adma2_64_desc *dma_desc = desc;

		if (host->flags & SDHCI_USE_64_BIT_DMA)
			pr_debug("%p: DMA 0x%08x%08x, LEN 0x%04x, Attr=0x%02x\n",
					desc, le32_to_cpu(dma_desc->addr_hi),
					le32_to_cpu(dma_desc->addr_lo),
					le16_to_cpu(dma_desc->len),
					le16_to_cpu(dma_desc->cmd));
		else
			pr_debug("%p: DMA 0x%08x, LEN 0x%04x, Attr=0x%02x\n",
					desc, le32_to_cpu(dma_desc->addr_lo),
					le16_to_cpu(dma_desc->len),
					le16_to_cpu(dma_desc->cmd));

		desc += host->desc_sz;

		if (dma_desc->cmd & cpu_to_le16(ADMA2_END))
			break;
	}
}

static void vsdhci_data_irq(struct virtio_sdhci_host *vhost, u32 intmask)
{
	struct sdhci_host *host = vhost->host;

	if (!host->data) {
		struct mmc_command *data_cmd = host->data_cmd;

		/*
		 * The "data complete" interrupt is also used to
		 * indicate that a busy state has ended. See comment
		 * above in sdhci_cmd_irq().
		 */
		if (data_cmd && (data_cmd->flags & MMC_RSP_BUSY)) {
			if (intmask & SDHCI_INT_DATA_TIMEOUT) {
				host->data_cmd = NULL;
				data_cmd->error = -ETIMEDOUT;
				vsdhci_finish_mrq(vhost, data_cmd->mrq);
				return;
			}
			if (intmask & SDHCI_INT_DATA_END) {
				host->data_cmd = NULL;
				/*
				 * Some cards handle busy-end interrupt
				 * before the command completed, so make
				 * sure we do things in the proper order.
				 */
				if (host->cmd == data_cmd)
					return;

				vsdhci_finish_mrq(vhost, data_cmd->mrq);
				return;
			}
		}

		/*
		 * SDHCI recovers from errors by resetting the cmd and data
		 * circuits. Until that is done, there very well might be more
		 * interrupts, so ignore them in that case.
		 */
		if (host->pending_reset)
			return;

		pr_err("%s: Got data interrupt 0x%08x even though no data operation was in progress.\n",
				mmc_hostname(host->mmc), (unsigned)intmask);
		vsdhci_dumpregs(vhost);

		return;
	}

	if (intmask & SDHCI_INT_DATA_TIMEOUT)
		host->data->error = -ETIMEDOUT;
	else if (intmask & SDHCI_INT_DATA_END_BIT)
		host->data->error = -EILSEQ;
	else if ((intmask & SDHCI_INT_DATA_CRC) &&
			SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND))
			!= MMC_BUS_TEST_R)
		host->data->error = -EILSEQ;
	else if (intmask & SDHCI_INT_ADMA_ERROR) {
		pr_err("%s: ADMA error\n", mmc_hostname(host->mmc));
		vsdhci_adma_show_error(vhost);
		host->data->error = -EIO;
	}

	if (host->data->error)
		vsdhci_finish_data(vhost);
	else {
		if (intmask & SDHCI_INT_DATA_END) {
			if (host->cmd == host->data_cmd) {
				/*
				 * Data managed to finish before the
				 * command completed. Make sure we do
				 * things in the proper order.
				 */
				host->data_early = 1;
			} else {
				vsdhci_finish_data(vhost);
			}
		}
	}
}

static unsigned int vsdhci_target_timeout(struct sdhci_host *host,
		struct mmc_command *cmd,
		struct mmc_data *data)
{
	unsigned int target_timeout;

	/* timeout in us */
	if (!data) {
		target_timeout = cmd->busy_timeout * 1000;
	} else {
		target_timeout = DIV_ROUND_UP(data->timeout_ns, 1000);
		if (host->clock && data->timeout_clks) {
			unsigned long long val;

			/*
			 * data->timeout_clks is in units of clock cycles.
			 * host->clock is in Hz.  target_timeout is in us.
			 * Hence, us = 1000000 * cycles / Hz.  Round up.
			 */
			val = 1000000ULL * data->timeout_clks;
			if (do_div(val, host->clock))
				target_timeout++;
			target_timeout += val;
		}
	}

	return target_timeout;
}

static void vsdhci_calc_sw_timeout(struct sdhci_host *host,
		struct mmc_command *cmd)
{
	struct mmc_data *data = cmd->data;
	struct mmc_host *mmc = host->mmc;
	struct mmc_ios *ios = &mmc->ios;
	unsigned char bus_width = 1 << ios->bus_width;
	unsigned int blksz;
	unsigned int freq;
	u64 target_timeout;
	u64 transfer_time;

	target_timeout = vsdhci_target_timeout(host, cmd, data);
	target_timeout *= NSEC_PER_USEC;

	if (data) {
		blksz = data->blksz;
		freq = host->mmc->actual_clock ? : host->clock;
		transfer_time = (u64)blksz * NSEC_PER_SEC * (8 / bus_width);
		do_div(transfer_time, freq);
		/* multiply by '2' to account for any unknowns */
		transfer_time = transfer_time * 2;
		/* calculate timeout for the entire data */
		host->data_timeout = data->blocks * target_timeout +
			transfer_time;
	} else {
		host->data_timeout = target_timeout;
	}

	if (host->data_timeout)
		host->data_timeout += MMC_CMD_TRANSFER_TIME;
}

static u8 vsdhci_calc_timeout(struct sdhci_host *host, struct mmc_command *cmd,
		bool *too_big)
{
	u8 count;
	struct mmc_data *data;
	unsigned target_timeout, current_timeout;

	*too_big = true;

	/*
	 * If the host controller provides us with an incorrect timeout
	 * value, just skip the check and use 0xE.  The hardware may take
	 * longer to time out, but that's much better than having a too-short
	 * timeout value.
	 */
	if (host->quirks & SDHCI_QUIRK_BROKEN_TIMEOUT_VAL)
		return 0xE;

	/* Unspecified command, asume max */
	if (cmd == NULL)
		return 0xE;

	data = cmd->data;
	/* Unspecified timeout, assume max */
	if (!data && !cmd->busy_timeout)
		return 0xE;

	/* timeout in us */
	target_timeout = vsdhci_target_timeout(host, cmd, data);

	/*
	 * Figure out needed cycles.
	 * We do this in steps in order to fit inside a 32 bit int.
	 * The first step is the minimum timeout, which will have a
	 * minimum resolution of 6 bits:
	 * (1) 2^13*1000 > 2^22,
	 * (2) host->timeout_clk < 2^16
	 *     =>
	 *     (1) / (2) > 2^6
	 */
	count = 0;
	current_timeout = (1 << 13) * 1000 / host->timeout_clk;
	while (current_timeout < target_timeout) {
		count++;
		current_timeout <<= 1;
		if (count >= 0xF)
			break;
	}

	if (count >= 0xF) {
		if (!(host->quirks2 & SDHCI_QUIRK2_DISABLE_HW_TIMEOUT))
			pr_debug("Too large timeout 0x%x requested for CMD%d!\n",
					count, cmd->opcode);
		count = 0xE;
	} else {
		*too_big = false;
	}

	return count;
}

static void vsdhci_set_timeout(struct sdhci_host *host, struct mmc_command *cmd)
{
	u8 count;
	bool too_big = false;

	count = vsdhci_calc_timeout(host, cmd, &too_big);

	if (too_big &&
			host->quirks2 & SDHCI_QUIRK2_DISABLE_HW_TIMEOUT) {
		vsdhci_calc_sw_timeout(host, cmd);
	}
}

static void vsdhci_prepare_data(struct virtio_sdhci_host *vhost, struct mmc_command *cmd)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_data *data = cmd->data;
	struct virtio_mmc_req *req;
	struct virtio_mmc_sdhci_cmd_desc *desc;

	host->data_timeout = 0;

	if (vsdhci_data_line_cmd(cmd))
		vsdhci_set_timeout(host, cmd);

	if (!data)
		return;

	WARN_ON(host->data);

	/* Sanity checks */
	BUG_ON(data->blksz * data->blocks > 524288);
	BUG_ON(data->blksz > host->mmc->max_blk_size);
	BUG_ON(data->blocks > 65535);

	host->data = data;
	host->data_early = 0;
	host->data->bytes_xfered = 0;

	req = &vhost->reqs[vhost->last[VQ_TX] - 1];
	desc = (struct virtio_mmc_sdhci_cmd_desc *)(req->cmd->CDB);
	desc->num_sectors = data->blocks;

	memset(host->adma_table, 0, host->align_buffer_sz + host->adma_table_sz);
	if (cmd->opcode == MMC_SEND_EXT_CSD) {
		desc->buf_mode = VIRTIO_MMC_SDHCI_BUF_NORMAL;
		req->req_data = host->adma_table;
		req->req_data_len = 4;
		__flush_dcache_area(req->req_data, host->align_buffer_sz + host->adma_table_sz);
		return;
	}

	if (host->flags & (SDHCI_USE_SDMA | SDHCI_USE_ADMA)) {
		host->flags |= SDHCI_REQ_USE_DMA;

		if ((host->quirks2 & SDHCI_QUIRK2_SLOW_USE_PIO) &&
				host->clock < 25000000)
			host->flags &= ~SDHCI_REQ_USE_DMA;
	}

	if (host->flags & SDHCI_REQ_USE_DMA) {
		int sg_cnt = vsdhci_pre_dma_transfer(host, data, COOKIE_MAPPED);

		if (sg_cnt <= 0) {
			/*
			 * This only happens when someone fed
			 * us an invalid request.
			 */
			WARN_ON(1);
			host->flags &= ~SDHCI_REQ_USE_DMA;
		} else if (host->flags & SDHCI_USE_ADMA) {
			vsdhci_adma_table_pre(host, data, sg_cnt);

			desc->buf_mode = VIRTIO_MMC_SDHCI_BUF_AMDA2_64BITS_DESC;
			req->req_data = host->adma_table;
			req->req_data_len = host->align_buffer_sz + host->adma_table_sz;
			__flush_dcache_area(req->req_data, host->align_buffer_sz + host->adma_table_sz);
		} else {
			WARN_ON(sg_cnt != 1);
		}
	}

	if (!(host->flags & SDHCI_REQ_USE_DMA)) {
		int flags;

		flags = SG_MITER_ATOMIC;
		if (host->data->flags & MMC_DATA_READ)
			flags |= SG_MITER_TO_SG;
		else
			flags |= SG_MITER_FROM_SG;
		sg_miter_start(&host->sg_miter, data->sg, data->sg_len, flags);
		host->blocks = data->blocks;
	}
}

static void vsdhci_resp_op(struct virtio_sdhci_host *vhost, void *buf)
{
	struct sdhci_host *host = vhost->host;
	struct virtio_mmc_req *req = (struct virtio_mmc_req *)buf;
	u32 status = 0;
	u32 intmask = 0;

	status = req->cmd_resp->status;

	if (status == VIRTIO_MMC_SDHCI_S_OK) {
		intmask |= SDHCI_INT_RESPONSE;
		if (host->data)
			intmask |= SDHCI_INT_DATA_END;
	}
	if (status & VIRTIO_MMC_SDHCI_S_CMD_TOUT_ERR)
		intmask |= SDHCI_INT_TIMEOUT;
	if (status & VIRTIO_MMC_SDHCI_S_CMD_CRC_ERR)
		intmask |= SDHCI_INT_CRC;
	if (status & VIRTIO_MMC_SDHCI_S_CMD_END_BIT_ERR)
		intmask |= SDHCI_INT_END_BIT;
	if (status & VIRTIO_MMC_SDHCI_S_CMD_IDX_ERR)
		intmask |= SDHCI_INT_INDEX;
	if (status & VIRTIO_MMC_SDHCI_S_DATA_TOUT_ERR)
		intmask |= SDHCI_INT_DATA_TIMEOUT;
	if (status & VIRTIO_MMC_SDHCI_S_DATA_CRC_ERR)
		intmask |= SDHCI_INT_DATA_CRC;
	if (status & VIRTIO_MMC_SDHCI_S_DATA_END_BIT_ERR)
		intmask |= SDHCI_INT_DATA_END_BIT;
	//	if (status & VIRTIO_MMC_SDHCI_S_CUR_LMT_ERR)
	//		intmask |= ;
	if (status & VIRTIO_MMC_SDHCI_S_AUTO_CMD_ERR)
		intmask |= SDHCI_INT_AUTO_CMD_ERR;
	if (status & VIRTIO_MMC_SDHCI_S_AMDA_ERR)
		intmask |= SDHCI_INT_ADMA_ERROR;
	if (status & VIRTIO_MMC_SDHCI_S_TUNING_ERR)
		intmask |= SDHCI_INT_RETUNE;
	if (status & VIRTIO_MMC_SDHCI_S_RESP_ERR)
		intmask |= SDHCI_INT_ERROR;
	if (status & VIRTIO_MMC_SDHCI_S_BOOT_ACK_ERR)
		intmask |= SDHCI_INT_TIMEOUT;
	if (status & VIRTIO_MMC_SDHCI_S_UNSUPP)
		intmask |= SDHCI_INT_TIMEOUT;

#if DEBUG
	pr_debug("status = %x, intmask = %x\n", status, intmask);
#endif
	if (intmask & SDHCI_INT_CMD_MASK)
		vsdhci_cmd_irq(vhost, intmask & SDHCI_INT_CMD_MASK, &intmask);

	if (intmask & SDHCI_INT_DATA_MASK)
		vsdhci_data_irq(vhost, intmask & SDHCI_INT_DATA_MASK);
}

static void virtio_sdhci_tx_callback(struct virtqueue *vq)
{
	struct virtio_sdhci_host *vhost = vq->vdev->priv;
	struct sdhci_host *host = vhost->host;
	void *buf = NULL;
	struct virtio_mmc_req *req;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vhost->lock, flags);
	if (host->runtime_suspended)
		goto out;

	buf = virtqueue_get_buf(vhost->tx, &len);
	if (!buf) {
		pr_err("vmmc tx callback buf is NULL\n");
		goto out;
	}

	__inval_dcache_area(buf, sizeof(struct virtio_mmc_req));
	req = (struct virtio_mmc_req *)buf;

	__inval_dcache_area(req->cmd, sizeof(struct virtio_mmc_cmd_req));
	__inval_dcache_area(req->cmd_resp, sizeof(struct virtio_mmc_cmd_resp));
#if DEBUG
	printk("req = %x %x %x\n", __pa(req->cmd), __pa(req->req_data), __pa(req->cmd_resp));
#endif
	if (vhost->last[VQ_TX] == req->cmd->id)
		vsdhci_resp_op(vhost, req);
	else
		pr_err("req[%d] is err: %lld\n", vhost->last[VQ_TX], req->cmd->id);
out:
	spin_unlock_irqrestore(&vhost->lock, flags);
}

void vsdhci_send_command(struct virtio_sdhci_host *vhost, struct mmc_command *cmd)
{
	unsigned long timeout;
	struct sdhci_host *host = vhost->host;
	struct virtio_mmc_req *req;
	struct virtio_mmc_sdhci_cmd_desc *desc;
	struct scatterlist hdr, mid, last, *sg[3];
	int i = 0, part = 0;

	WARN_ON(host->cmd);

	/* Initially, a command has no error */
	cmd->error = 0;

	/* Wait max 10 ms */
	timeout = 10;

#if DEBUG
	printk("cmd = %d, arg = %x, flags = %x\n", cmd->opcode, cmd->arg, cmd->flags);
#endif
	host->cmd = cmd;
	if (vsdhci_data_line_cmd(cmd)) {
		WARN_ON(host->data_cmd);
		host->data_cmd = cmd;
	}
	if (cmd->opcode == MMC_ERASE_GROUP_START) {
		from = cmd->arg;
		vsdhci_finish_mrq(vhost, cmd->mrq);
		return;
	}
	if (cmd->opcode == MMC_ERASE_GROUP_END) {
		to = cmd->arg;
		vsdhci_finish_mrq(vhost, cmd->mrq);
		return;
	}

	for (i = 0; i < VIRTIO_MMC_VIRTQUEUE_SIZE; i++) {
		if (!vhost->reqs[i].cmd->id)
			break;
	}
	if (i >= VIRTIO_MMC_VIRTQUEUE_SIZE) {
		pr_err("mmc vq reqs is full\n");
		return;
	}
	vhost->last[VQ_TX] = i + 1;

	req = &vhost->reqs[i];
#if DEBUG
	printk("req[%d]: %x %x %x\n", i, __pa(req->cmd), __pa(req->req_data), __pa(req->cmd_resp));
#endif
	memset(req->cmd, 0, sizeof(struct virtio_mmc_cmd_req));
	memset(req->cmd_resp, 0, sizeof(struct virtio_mmc_cmd_resp));

	req->cmd->id = i + 1;
	req->cmd->priority = 0;
	req->cmd->cdb_type = VIRTIO_MMC_CDB_SDHCI;

	req->req_data_len = 4;
	vsdhci_prepare_data(vhost, cmd);

	if ((cmd->flags & MMC_RSP_136) && (cmd->flags & MMC_RSP_BUSY)) {
		pr_err("%s: Unsupported response type!\n",
				mmc_hostname(host->mmc));
		cmd->error = -EINVAL;
		vsdhci_finish_mrq(vhost, cmd->mrq);
		return;
	}

	timeout = jiffies;
	if (host->data_timeout)
		timeout += nsecs_to_jiffies(host->data_timeout);
	else if (!cmd->data && cmd->busy_timeout > 9000)
		timeout += DIV_ROUND_UP(cmd->busy_timeout, 1000) * HZ + HZ;
	else
		timeout += 10 * HZ;
	vsdhci_mod_timer(host, cmd->mrq, timeout);

	desc = (struct virtio_mmc_sdhci_cmd_desc *)(req->cmd->CDB);

	desc->cmd_index = cmd->opcode;
	desc->argument = cmd->arg;
	desc->cmd_timeout = timeout;
	desc->cmd_retry = cmd->retries;
	desc->cmd23_support = 1;
	desc->part_type = PART_ACCESS_DEFAULT;

	if (!hw_part_type)
		desc->part_type = hw_part_type;


	switch (cmd->opcode) {
		case MMC_GO_IDLE_STATE:
		case MMC_SET_DSR:
			desc->resp_type = 0;
			break;
		case MMC_SET_RELATIVE_ADDR:
		case MMC_SELECT_CARD:
		case MMC_SEND_EXT_CSD:
		case MMC_SEND_CSD:
		case MMC_SEND_STATUS:
		case MMC_SET_BLOCKLEN:
		case MMC_READ_SINGLE_BLOCK:
		case MMC_READ_MULTIPLE_BLOCK:
		case MMC_SEND_TUNING_BLOCK_HS200:
		case MMC_SET_BLOCK_COUNT:
		case MMC_WRITE_BLOCK:
		case MMC_WRITE_MULTIPLE_BLOCK:
			desc->resp_type = BIT(0);
			break;
		case MMC_SWITCH:
		case MMC_STOP_TRANSMISSION:
		case MMC_ERASE:
			if (cmd->opcode == MMC_ERASE) {
				desc->sector = from;
				desc->num_sectors = to - from + 1;
				if (desc->num_sectors <= 0) {
					pr_err("err: erase: %x %x %x\n", from, to, cmd->arg);
					return;
				}
			}
			desc->resp_type = BIT(0);
			if (cmd->flags & MMC_RSP_BUSY)
				desc->resp_type = BIT(1);
			break;
		case MMC_ALL_SEND_CID:
			desc->resp_type = BIT(2);
			break;
		case MMC_SEND_OP_COND:
			desc->resp_type = BIT(3);
			break;
		default:
			pr_err("cmd %d is not supported.\n", cmd->opcode);
			break;
	}

	__flush_dcache_area(req, sizeof(struct virtio_mmc_req));
	__flush_dcache_area(req->cmd, sizeof(struct virtio_mmc_cmd_req));
	__flush_dcache_area(req->cmd_resp, sizeof(struct virtio_mmc_cmd_resp));

	sg_init_one(&hdr, req->cmd, sizeof(struct virtio_mmc_cmd_req));
	sg_init_one(&mid, req->req_data, req->req_data_len);
	sg_init_one(&last, req->cmd_resp, sizeof(struct virtio_mmc_cmd_resp));

	sg[0] = &hdr;
	sg[1] = &mid;
	sg[2] = &last;
	virtqueue_add_sgs(vhost->tx, sg, 2, 1, &vhost->reqs[i], GFP_ATOMIC);
	vhost->done = 1;

	if (cmd->opcode == 6) {
		part = (cmd->arg & 0xff0000) >> 16;
		if (part == 179) {
			part = ((cmd->arg & 0xff00) >> 8) & EXT_CSD_PART_CONFIG_ACC_MASK;
			if (part != hw_part_type)
				hw_part_type = part;
		}
	}
}

static void vsdhci_check_auto_cmd23(struct mmc_host *mmc,
				    struct mmc_request *mrq)
{
	struct virtio_sdhci_host *vhost = mmc_priv(mmc);
	struct sdhci_host *host = vhost->host;

	/*
	* No matter V4 is enabled or not, ARGUMENT2 register is 32-bit
	* block count register which doesn't support stuff bits of
	* CMD23 argument on dwcmsch host controller.
	*/
	if (mrq->sbc && (mrq->sbc->arg & SDHCI_ARG2_STUFF))
		host->flags &= ~SDHCI_AUTO_CMD23;
	else
		host->flags |= SDHCI_AUTO_CMD23;
}

void vsdhci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct virtio_sdhci_host *vhost;
	struct sdhci_host *host;
	unsigned long flags;

	vhost = mmc_priv(mmc);
	host = vhost->host;

	wait_for_completion_timeout(&vhost->req_comp,msecs_to_jiffies(100));
	spin_lock_irqsave(&vhost->lock, flags);
	vhost->done = 0;

//	vsdhci_check_auto_cmd23(mmc, mrq);
	if (host->flags & SDHCI_DEVICE_DEAD) {
		mrq->cmd->error = -ENOMEDIUM;
		vsdhci_finish_mrq(vhost, mrq);
	} else {
		if (mrq->sbc && !(host->flags & SDHCI_AUTO_CMD23))
			vsdhci_send_command(vhost, mrq->sbc);
		else
			vsdhci_send_command(vhost, mrq->cmd);
	}

	mmiowb();
	spin_unlock_irqrestore(&vhost->lock, flags);
	if (vhost->done)
		virtqueue_kick(vhost->tx);
}

static inline bool vsdhci_has_requests(struct sdhci_host *host)
{
	return host->cmd || host->data_cmd;
}

static void vsdhci_error_out_mrqs(struct virtio_sdhci_host *vhost, int err)
{
	struct sdhci_host *host = vhost->host;

	if (host->data_cmd) {
		host->data_cmd->error = err;
		vsdhci_finish_mrq(vhost, host->data_cmd->mrq);
	}

	if (host->cmd) {
		host->cmd->error = err;
		vsdhci_finish_mrq(vhost, host->cmd->mrq);
	}
}

static void vsdhci_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
		int err)
{
	struct virtio_sdhci_host *vhost = mmc_priv(mmc);
	struct sdhci_host *host = vhost->host;
	struct mmc_data *data = mrq->data;

	if (data->host_cookie != COOKIE_UNMAPPED)
		dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
				mmc_get_dma_dir(data));

	data->host_cookie = COOKIE_UNMAPPED;
}
static void vsdhci_pre_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct virtio_sdhci_host *vhost = mmc_priv(mmc);
	struct sdhci_host *host = vhost->host;

	mrq->data->host_cookie = COOKIE_UNMAPPED;

	/*
	 * No pre-mapping in the pre hook if we're using the bounce buffer,
	 * for that we would need two bounce buffers since one buffer is
	 * in flight when this is getting called.
	 */
	if (host->flags & SDHCI_REQ_USE_DMA && !host->bounce_buffer)
		vsdhci_pre_dma_transfer(host, mrq->data, COOKIE_PRE_MAPPED);
}

static int vsdhci_card_busy(struct mmc_host *mmc)
{
	return 0;
}

static const struct mmc_host_ops vsdhci_host_ops = {
	.request	= vsdhci_request,
	.post_req	= vsdhci_post_req,
	.pre_req	= vsdhci_pre_req,
	.set_ios	= vsdhci_set_ios,
	.card_busy	= vsdhci_card_busy,
};

static int vsdhci_set_dma_mask(struct sdhci_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct device *dev = mmc_dev(mmc);
	int ret = -EINVAL;

	/* Try 64-bit mask if hardware is capable  of it */
	if (host->flags & SDHCI_USE_64_BIT_DMA) {
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
		if (ret) {
			pr_warn("%s: Failed to set 64-bit DMA mask.\n",
					mmc_hostname(mmc));
			host->flags &= ~SDHCI_USE_64_BIT_DMA;
		}
	}

	return ret;
}

int vsdhci_setup_host(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_host *mmc;
	unsigned int ocr_avail = 0;
	unsigned int override_timeout_clk;
	int ret;
	struct page *page;

	WARN_ON(host == NULL);
	if (host == NULL)
		return -EINVAL;

	mmc = host->mmc;

	host->version = 5;
	override_timeout_clk = host->timeout_clk;

	host->flags |= SDHCI_USE_SDMA |
		SDHCI_USE_ADMA |
		SDHCI_USE_64_BIT_DMA |
		SDHCI_AUTO_CMD23 |
		SDHCI_SIGNALING_180;
	ret = vsdhci_set_dma_mask(host);
	if (ret) {
		pr_warn("%s: No suitable DMA available - falling back to PIO\n",
				mmc_hostname(mmc));
		host->flags &= ~(SDHCI_USE_SDMA | SDHCI_USE_ADMA);
		return -EINVAL;
	}

	if (host->flags & SDHCI_USE_ADMA) {
		void *buf;

		if (host->flags & SDHCI_USE_64_BIT_DMA) {
			host->adma_table_sz = host->adma_table_cnt *
				SDHCI_ADMA2_64_DESC_SZ(host);
			host->desc_sz = SDHCI_ADMA2_64_DESC_SZ(host);
		} else {
			host->adma_table_sz = host->adma_table_cnt *
				SDHCI_ADMA2_32_DESC_SZ;
			host->desc_sz = SDHCI_ADMA2_32_DESC_SZ;
		}

		host->align_buffer_sz = SDHCI_MAX_SEGS * SDHCI_ADMA2_ALIGN;
		/*
		 * Use zalloc to zero the reserved high 32-bits of 128-bit
		 * descriptors so that they never need to be written.
		 */
		host->adma_table_page_cnt = (host->align_buffer_sz + host->adma_table_sz) / PAGE_SIZE; 
		if ((host->align_buffer_sz + host->adma_table_sz) % PAGE_SIZE)
			host->adma_table_page_cnt++;
		page = dma_alloc_from_contiguous(mmc_dev(mmc),
				host->adma_table_page_cnt,
				get_order(host->adma_table_page_cnt << PAGE_SHIFT ), GFP_KERNEL);
		if (!page) {
			pr_warn("%s: Unable to allocate ADMA buffers - falling back to standard DMA\n",
					mmc_hostname(mmc));
			host->flags &= ~SDHCI_USE_ADMA;
			return -ENOMEM;
		} else {
			buf = page_to_virt(page);
			memset(buf, 0, host->adma_table_page_cnt * PAGE_SIZE);

			host->align_buffer = buf;
			host->align_addr = __pa(host->align_buffer);

			host->adma_table = buf + host->align_buffer_sz;
			host->adma_addr = __pa(host->adma_table);
		}
	}

#if DEBUG
	printk("align: %x %x, adma: %x %x\n", host->align_buffer, host->align_addr, host->adma_table, host->adma_addr);
#endif
	host->max_clk = 200000000;
	mmc->f_min = host->max_clk / SDHCI_MAX_DIV_SPEC_300;
	mmc->retune_period = 0;

	host->timeout_clk = 1000;
	if (override_timeout_clk)
		host->timeout_clk = override_timeout_clk;
	mmc->max_busy_timeout = (1 << 27) / host->timeout_clk;

	mmc->caps |= MMC_CAP_ERASE |
	   	MMC_CAP_CMD23 |
		MMC_CAP_4_BIT_DATA |
		MMC_CAP_SD_HIGHSPEED |
		MMC_CAP_MMC_HIGHSPEED |
		MMC_CAP_UHS_SDR12 |
		MMC_CAP_UHS_SDR25 |
		MMC_CAP_UHS_SDR50 |
		MMC_CAP_UHS_SDR104 |
		MMC_CAP_UHS_DDR50 |
		MMC_CAP_DRIVER_TYPE_A |
		MMC_CAP_DRIVER_TYPE_C |
		MMC_CAP_DRIVER_TYPE_D; 
	mmc->caps2 |= MMC_CAP2_SDIO_IRQ_NOTHREAD;

	host->tuning_mode = SDHCI_TUNING_MODE_3;
	ocr_avail |= MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	mmc->ocr_avail = ocr_avail;
	mmc->ocr_avail_sdio = ocr_avail;
	mmc->ocr_avail_sd = ocr_avail & (~MMC_VDD_165_195);
	mmc->ocr_avail_mmc = ocr_avail;
	mmc->max_req_size = 524288;
	mmc->max_segs = SDHCI_MAX_SEGS;
	mmc->max_seg_size = 0x4000000;
	mmc->max_blk_size = 512;
	mmc->max_blk_count = 65535;
	return 0;
}

static void vsdhci_get_property(struct virtio_device *vdev)
{
	struct virtio_sdhci_host *vhost = vdev->priv;
	struct sdhci_host *host = vhost->host;

	host->mmc->caps |= MMC_CAP_8_BIT_DATA |
		MMC_CAP_NONREMOVABLE |
		MMC_CAP_MMC_HIGHSPEED |
		MMC_CAP_1_8V_DDR |
		MMC_CAP_HW_RESET;
	host->mmc->caps2 |= MMC_CAP2_NO_SDIO |
		MMC_CAP2_HS200_1_8V_SDR |
		MMC_CAP2_HS400_1_8V |
		MMC_CAP2_NO_SD;
	host->mmc->pm_caps |= MMC_PM_KEEP_POWER;
	host->mmc->f_max = 200000000;
}

static void vsdhci_timeout_timer(unsigned long data)
{
	struct virtio_sdhci_host *vhost;
	struct sdhci_host *host;
	unsigned long flags;

	vhost = (struct virtio_sdhci_host *)data;
	host = vhost->host;

	spin_lock_irqsave(&vhost->lock, flags);

	if (host->cmd && !vsdhci_data_line_cmd(host->cmd)) {
		pr_err("%s: Timeout waiting for hardware cmd interrupt.\n",
				mmc_hostname(host->mmc));
		vsdhci_dumpregs(vhost);

		host->cmd->error = -ETIMEDOUT;
		vsdhci_finish_mrq(vhost, host->cmd->mrq);
	}

	mmiowb();
	spin_unlock_irqrestore(&vhost->lock, flags);
}

static void vsdhci_timeout_data_timer(unsigned long data)
{
	struct virtio_sdhci_host *vhost;
	struct sdhci_host *host;
	unsigned long flags;

	vhost = (struct virtio_sdhci_host *)data;
	host = vhost->host;

	spin_lock_irqsave(&vhost->lock, flags);

	if (host->data || host->data_cmd ||
			(host->cmd && vsdhci_data_line_cmd(host->cmd))) {
		pr_err("%s: Timeout waiting for hardware interrupt.\n",
				mmc_hostname(host->mmc));
		vsdhci_dumpregs(vhost);

		if (host->data) {
			host->data->error = -ETIMEDOUT;
			vsdhci_finish_data(vhost);
		} else if (host->data_cmd) {
			host->data_cmd->error = -ETIMEDOUT;
			vsdhci_finish_mrq(vhost, host->data_cmd->mrq);
		} else {
			host->cmd->error = -ETIMEDOUT;
			vsdhci_finish_mrq(vhost, host->cmd->mrq);
		}
	}

	mmiowb();
	spin_unlock_irqrestore(&vhost->lock, flags);
}

int _vsdhci_add_host(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_host *mmc = host->mmc;
	int ret;

	/*
	 * Init tasklets.
	 */
	tasklet_init(&host->finish_tasklet,
			vsdhci_tasklet_finish, (unsigned long)vhost);

	setup_timer(&host->timer, vsdhci_timeout_timer, (unsigned long)vhost);
	setup_timer(&host->data_timer, vsdhci_timeout_data_timer,
			(unsigned long)vhost);

	mmiowb();

	ret = mmc_add_host(mmc);
	if (ret)
		goto unirq;

	pr_info("%s: SDHCI controller on %s [%s] using %s\n",
			mmc_hostname(mmc), host->hw_name, dev_name(mmc_dev(mmc)),
			(host->flags & SDHCI_USE_ADMA) ?
			(host->flags & SDHCI_USE_64_BIT_DMA) ? "ADMA 64-bit" : "ADMA" :
			(host->flags & SDHCI_USE_SDMA) ? "DMA" : "PIO");

	return 0;

unirq:
	tasklet_kill(&host->finish_tasklet);

	return ret;
}

void vsdhci_cleanup_host(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_host *mmc = host->mmc;

	if (host->align_buffer)
		dma_release_from_contiguous(mmc_dev(mmc),
				host->align_buffer, host->adma_table_page_cnt);
	host->adma_table = NULL;
	host->align_buffer = NULL;
}

int vsdhci_add_host(struct virtio_sdhci_host *vhost)
{
	int ret;

	ret = vsdhci_setup_host(vhost);
	if (ret)
		return ret;

	ret = _vsdhci_add_host(vhost);
	if (ret)
		goto cleanup;

	return 0;

cleanup:
	vsdhci_cleanup_host(vhost);

	return ret;
}

void vsdhci_dma_cfg(struct virtio_device *vdev)
{
	u64 mask, size;

	vdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	vdev->dev.dma_mask = &vdev->dev.coherent_dma_mask;
	size = max(vdev->dev.coherent_dma_mask, vdev->dev.coherent_dma_mask + 1);

	vdev->dev.dma_pfn_offset = 0;
	mask = DMA_BIT_MASK(ilog2(0 + size - 1) + 1);
	vdev->dev.coherent_dma_mask &= mask;
	*vdev->dev.dma_mask &= mask;
	arch_setup_dma_ops(&vdev->dev, 0, size, NULL, false);
}

struct virtio_sdhci_host *vsdhci_alloc_host(struct device *dev,
		size_t priv_size)
{
	struct mmc_host *mmc;
	struct virtio_sdhci_host *vhost;
	struct sdhci_host *host;
	int i = 0;
	struct page *page = NULL;
	void *buf = NULL;

	WARN_ON(dev == NULL);

	mmc = mmc_alloc_host(sizeof(struct virtio_sdhci_host), dev);
	if (!mmc)
		return ERR_PTR(-ENOMEM);

	vhost = mmc_priv(mmc);

	vhost->host = kzalloc(sizeof(struct sdhci_host) + priv_size, GFP_KERNEL);
	if (!vhost->host) {
		kfree(mmc);
		return ERR_PTR(-ENOMEM);
	}

	page = dma_alloc_from_contiguous(dev, VIRTIO_MMC_VIRTQUEUE_SIZE, get_order(PAGE_SIZE), GFP_KERNEL);
	if (!page) {
		kfree(vhost->host);
		kfree(mmc);
		return ERR_PTR(-ENOMEM);
	}
	buf = page_to_virt(page);
	memset(buf, 0, PAGE_SIZE * VIRTIO_MMC_VIRTQUEUE_SIZE);
	for (i = 0; i < VIRTIO_MMC_VIRTQUEUE_SIZE; i++) {
		vhost->reqs[i].cmd = (struct virtio_mmc_cmd_req *)(buf + i * PAGE_SIZE);
		vhost->reqs[i].cmd_resp = (struct virtio_mmc_cmd_resp *)(buf + i * PAGE_SIZE + PAGE_SIZE / 2);
	}

	host = vhost->host;
	host->mmc = mmc;
	host->mmc_host_ops = vsdhci_host_ops;
	mmc->ops = &host->mmc_host_ops;

	host->flags = SDHCI_SIGNALING_330;

	host->tuning_delay = -1;
	host->tuning_loop_count = MAX_TUNING_LOOP;

	/*
	 * The DMA table descriptor count is calculated as the maximum
	 * number of segments times 2, to allow for an alignment
	 * descriptor for each segment, plus 1 for a nop end descriptor.
	 */
	host->adma_table_cnt = SDHCI_MAX_SEGS * 2 + 1;

	return vhost;
}
void vsdhci_remove_host(struct virtio_sdhci_host *vhost, int dead)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_host *mmc = host->mmc;
	unsigned long flags;

	if (dead) {
		spin_lock_irqsave(&vhost->lock, flags);

		host->flags |= SDHCI_DEVICE_DEAD;

		if (vsdhci_has_requests(host)) {
			pr_err("%s: Controller removed during "
					" transfer!\n", mmc_hostname(mmc));
			vsdhci_error_out_mrqs(vhost, -ENOMEDIUM);
		}

		spin_unlock_irqrestore(&vhost->lock, flags);
	}

	mmc_remove_host(mmc);

	del_timer_sync(&host->timer);
	del_timer_sync(&host->data_timer);

	tasklet_kill(&host->finish_tasklet);

	if (host->align_buffer)
		dma_release_from_contiguous(mmc_dev(mmc),
				(struct page *)host->align_buffer,
				host->adma_table_page_cnt);

	host->adma_table = NULL;
	host->align_buffer = NULL;
	kfree(host);
}

static struct attribute *virtio_sdhci_attributes[] = {
	&dev_attr_test.attr,
	NULL
};

static const struct attribute_group virtio_sdhci_attr_group = {
	.name = "virtio_sdhci",
	.attrs = virtio_sdhci_attributes,
};

static int virtio_sdhci_probe(struct virtio_device *vdev)
{
	struct virtio_sdhci_host *vhost;
	struct virtqueue *vqs[2];
	vq_callback_t *cbs[] = { virtio_sdhci_tx_callback,
		virtio_sdhci_rx_callback };
	static const char * const names[] = { "tx", "rx" };
	struct sdhci_host *host;
	int err;

	vhost = vsdhci_alloc_host(&vdev->dev, 0);
	if (IS_ERR(vhost)) {
		err = PTR_ERR(vhost);
		goto err;
	}

	vdev->priv = vhost;
	vhost->vdev = vdev;
	spin_lock_init(&vhost->lock);

	init_completion(&vhost->req_comp);

	err = virtio_find_vqs(vhost->vdev, 2, vqs, cbs, names, NULL);
	if (err)
		goto free_host;

	vhost->tx = vqs[0];
	vhost->rx = vqs[1];

	dev_info(&vhost->vdev->dev, "find vqs: tx %p rx %p\n", vhost->tx, vhost->rx);

	err = sysfs_create_group(&vhost->vdev->dev.kobj, &virtio_sdhci_attr_group);
	if (err)
		goto free_host;

	virtio_device_ready(vdev);

	virtio_sdhci_fill_rx(vhost);

	dev_info(&vdev->dev, "virtio_sdhci ready\n");

	host = vhost->host;
	host->hw_name = dev_name(&vdev->dev);
	host->quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN | SDHCI_QUIRK2_SLOW_USE_PIO;

	vsdhci_get_property(vdev);

	host->v4_mode = true;

	vsdhci_dma_cfg(vdev);

	err = vsdhci_add_host(vhost);
	if (err)
		goto free_host;

	return 0;

free_host:
	kfree(vhost->host);
	dma_release_from_contiguous(&vdev->dev,
			(struct page *)vhost->reqs[0].cmd, VIRTIO_MMC_VIRTQUEUE_SIZE);
	kfree(vhost);
err:
	return err;
}

static void virtio_sdhci_remove(struct virtio_device *vdev)
{
	struct virtio_sdhci_host *vhost = vdev->priv;

	vsdhci_remove_host(vhost, 0);
	sysfs_remove_group(&vhost->vdev->dev.kobj, &virtio_sdhci_attr_group);
	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
	dma_release_from_contiguous(&vdev->dev,
			(struct page *)vhost->reqs[0].cmd, VIRTIO_MMC_VIRTQUEUE_SIZE);
	kfree(vhost);
}

#ifdef CONFIG_PM
int vsdhci_suspend_host(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;

	mmc_retune_timer_stop(host->mmc);

	return 0;
}
EXPORT_SYMBOL_GPL(vsdhci_suspend_host);

int vsdhci_resume_host(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;

	vsdhci_init(vhost, (host->mmc->pm_flags & MMC_PM_KEEP_POWER));
	mmiowb();

	return 0;
}
EXPORT_SYMBOL_GPL(vsdhci_resume_host);

int vsdhci_runtime_suspend_host(struct virtio_sdhci_host *vhost)
{
	unsigned long flags;
	struct sdhci_host *host = vhost->host;

	mmc_retune_timer_stop(host->mmc);

	spin_lock_irqsave(&vhost->lock, flags);
	host->runtime_suspended = true;
	spin_unlock_irqrestore(&vhost->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(vsdhci_runtime_suspend_host);

int vsdhci_runtime_resume_host(struct virtio_sdhci_host *vhost)
{
	struct sdhci_host *host = vhost->host;
	struct mmc_host *mmc = host->mmc;
	unsigned long flags;

	if (mmc->ios.power_mode != MMC_POWER_UNDEFINED &&
	    mmc->ios.power_mode != MMC_POWER_OFF) {
		/* Force clock and power re-program */
		host->clock = 0;
		mmc->ops->set_ios(mmc, &mmc->ios);

		if ((mmc->caps2 & MMC_CAP2_HS400_ES) &&
		    mmc->ops->hs400_enhanced_strobe)
			mmc->ops->hs400_enhanced_strobe(mmc, &mmc->ios);
	}

	spin_lock_irqsave(&vhost->lock, flags);

	host->runtime_suspended = false;

	spin_unlock_irqrestore(&vhost->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(vsdhci_runtime_resume_host);
#endif /* CONFIG_PM */

#ifdef CONFIG_PM_SLEEP
static int virtio_sdhci_suspend(struct virtio_device *vdev)
{
	struct virtio_sdhci_host *vhost = vdev->priv;

	return vsdhci_suspend_host(vhost);
}

static int virtio_sdhci_resume(struct virtio_device *vdev)
{
	struct virtio_sdhci_host *vhost = vdev->priv;

	return vsdhci_resume_host(vhost);
}
#endif

static unsigned int virtio_sdhci_features[] = {
	/* none */
};

static struct virtio_device_id virtio_sdhci_id_table[] = {
	{ VIRTIO_ID_SDHCI, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_sdhci_driver = {
	.driver = {
		.name = "sdrv_virtio_sdhci",
		.owner = THIS_MODULE,
	},
	.feature_table       = virtio_sdhci_features,
	.feature_table_size  = ARRAY_SIZE(virtio_sdhci_features),
	.id_table            = virtio_sdhci_id_table,
	.probe               = virtio_sdhci_probe,
#ifdef CONFIG_PM_SLEEP
	.freeze = virtio_sdhci_suspend,
	.restore = virtio_sdhci_resume,
#endif
	.remove              = virtio_sdhci_remove,
};

module_virtio_driver(virtio_sdhci_driver);

MODULE_DESCRIPTION("Virtio sdhci driver");
MODULE_AUTHOR("Semidrive SDHCI");
MODULE_LICENSE("GPL v2");
