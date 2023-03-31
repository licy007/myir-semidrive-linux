/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <linux/delay.h>
#include "ce_reg.h"
#include "sx_dma.h"
#include "sx_errors.h"
#include "sx_pke_conf.h"
#include "sram_conf.h"

#define WAIT_PK_WITH_REGISTER_POLLING 1
#define LOCAL_TRACE 0 //close local trace 1->0


/**
 * @brief Check cryptodma fifo status
 * @param vce_id      vce index
 * @return CRYPTOLIB_DMA_ERR if bus error occured, CRYPTOLIB_SUCCESS otherwise
 */
static uint32_t cryptodma_check_fifo_empty(void *device)
{
	uint32_t dma_status; //0x300
	uint32_t vce_id;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	dma_status = readl((((struct crypto_dev *)device)->base + REG_DMA_CTRL_CE_(vce_id))) & 0x200;

	if (dma_status) {
		pr_info("CRYPTODMA fifo error 0x%x\n", dma_status);
		return dma_status;
	}
	else {
		return CRYPTOLIB_SUCCESS;
	}
}


void cryptodma_config(void *device, block_t *dst, block_t *src, uint32_t length)
{
	uint32_t value;
	uint32_t transfer_len;
	uint32_t read_value;
	uint32_t fifo_state;
	uint32_t vce_id;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	transfer_len = dst->len < length ? dst->len : length;

	if (EXT_MEM == src->addr_type) {
		writel(_paddr((void *)src->addr), (((struct crypto_dev *)device)->base + (REG_DMA_SRC_ADDR_CE_(vce_id))));
		//TODO: compatible 64bit address
		value = reg_value(((_paddr((void *)src->addr)) >> 32), 0, CE_DMA_SRC_ADDR_H_SHIFT, CE_DMA_SRC_ADDR_H_MASK);
	}
	else {
		writel((addr_t)src->addr, (((struct crypto_dev *)device)->base + (REG_DMA_SRC_ADDR_CE_(vce_id))));
		value = reg_value(0, 0, CE_DMA_SRC_ADDR_H_SHIFT, CE_DMA_SRC_ADDR_H_MASK);
	}


	value = reg_value(switch_addr_type(src->addr_type), value, CE_DMA_SRC_TYPE_SHIFT, CE_DMA_SRC_TYPE_MASK);
	writel(value, (((struct crypto_dev *)device)->base + (REG_DMA_SRC_ADDR_H_CE_(vce_id))));

	if (EXT_MEM == dst->addr_type) {
		writel(_paddr((void *)dst->addr), (((struct crypto_dev *)device)->base + REG_DMA_DST_ADDR_CE_(vce_id)));
		//TODO: compatible 64bit address
		value = reg_value(((_paddr((void *)dst->addr)) >> 32), 0, CE_DMA_DST_ADDR_H_SHIFT, CE_DMA_DST_ADDR_H_MASK);
	}
	else {
		writel((addr_t)dst->addr, (((struct crypto_dev *)device)->base + REG_DMA_DST_ADDR_CE_(vce_id)));
		value = reg_value(0, 0, CE_DMA_DST_ADDR_H_SHIFT, CE_DMA_DST_ADDR_H_MASK);
	}

	value = reg_value(switch_addr_type(dst->addr_type), value, CE_DMA_DST_TYPE_SHIFT, CE_DMA_DST_TYPE_MASK);
	writel(value, (((struct crypto_dev *)device)->base + REG_DMA_DST_ADDR_H_CE_(vce_id)));

	writel(transfer_len, (((struct crypto_dev *)device)->base + REG_DMA_LEN_CE_(vce_id)));
	mb();

	do {

		fifo_state = cryptodma_check_fifo_empty(device);

	}
	while (fifo_state != CRYPTOLIB_SUCCESS);

	read_value = readl(((struct crypto_dev *)device)->base + (REG_DMA_CTRL_CE_(vce_id)));

#if WAIT_PK_WITH_REGISTER_POLLING // polling
	value = 0x0;
#else  //wait interrupt
	value = 0x1;
#endif

	value = reg_value(value, read_value, CE_DMA_DONEINTEN_SHIFT, CE_DMA_DONEINTEN_MASK);
	value = reg_value(0x1, value, CE_DMA_GO_SHIFT, CE_DMA_GO_MASK);
	writel(value, (((struct crypto_dev *)device)->base + REG_DMA_CTRL_CE_(vce_id)));
}

void cryptodma_wait(void *device)
{
	// Wait until DMA is done
	uint32_t vce_id;
#if WAIT_PK_WITH_REGISTER_POLLING // polling
	int i = 0;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	while (readl((((struct crypto_dev *)device)->base + REG_STAT_CE_(vce_id))) & 0x1) {
		if (i == 30) {
			pr_info("DMA is busy.\n");
			break;
		}

		udelay(5);
		i++;
	}

#else  //wait interrupt
	wait_event_interruptible(((struct crypto_dev *)device)->cedmawaitq, ((struct crypto_dev *)device)->dma_condition);
	((struct crypto_dev *)device)->dma_condition = 0;
	//pr_info("wait interrupt in dma end\n");
#endif
}

uint32_t cryptodma_check_status(void *device)
{
	uint32_t vce_id;
	//cryptodma_check_bus_error
	uint32_t err;
	uint32_t value_state;
	vce_id = ((struct crypto_dev *)device)->ce_id;

	err = readl((((struct crypto_dev *)device)->base + REG_ERRSTAT_CE_(vce_id))) & 0x70000;

	if (err) {
		pr_info("CRYPTODMA bus error: 0x%x\n", err);
		value_state = reg_value(1, 0x0, CE_DMA_INTEGRITY_ERROR_INTCLR_SHIFT, CE_DMA_INTEGRITY_ERROR_INTCLR_MASK);
		writel(value_state, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));

		value_state = reg_value(0, 0x0, CE_DMA_INTEGRITY_ERROR_INTCLR_SHIFT, CE_DMA_INTEGRITY_ERROR_INTCLR_MASK);
		writel(value_state, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));
		return err;
	}
	else {
		return CRYPTOLIB_SUCCESS;
	}
}

uint32_t memcpy_blk_common(void *device, block_t *dst, block_t *src, uint32_t length, bool cache_op)
{
	uint32_t status;
	uint32_t vce_id;

	if (!length) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (dst->len < length) {
		length = dst->len;
	}

	vce_id = ((struct crypto_dev *)device)->ce_id;

	if (cache_op) {
		clean_cache_block(src, vce_id);
		invalidate_cache_block(dst, vce_id);
	}

	cryptodma_config(device, dst, src, length);
	cryptodma_wait(device);

	status = cryptodma_check_status(device);

	return status;
}

uint32_t memcpy_blk_cache(void *device, block_t *dst, block_t *src, uint32_t length, bool cache_op_src, bool cache_op_dst)
{
	uint32_t status;
	uint32_t vce_id;

	if (!length) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (dst->len < length) {
		length = dst->len;
	}

	vce_id = ((struct crypto_dev *)device)->ce_id;

	if (cache_op_src) {
		clean_cache_block(src, vce_id);
	}

	if (cache_op_dst) {
		invalidate_cache_block(dst, vce_id);
	}

	cryptodma_config(device, dst, src, length);
	cryptodma_wait(device);

	status = cryptodma_check_status(device);

	return status;
}

uint32_t memcpy_blk(void *device, block_t *dst, block_t *src, uint32_t length)
{
	return memcpy_blk_common(device, dst, src, length, true);
}

uint32_t point2CryptoRAM_rev(void *device, block_t *src, uint32_t size, uint32_t offset)
{
	block_t temp;
	uint32_t status;

	if (src->len < size * 2) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	status = mem2CryptoRAM_rev(device, src, size, offset, true);

	if (status) {
		return status;
	}

	temp.addr = src->addr + size;
	temp.addr_type = src->addr_type;
	temp.len = src->len - size;
	return mem2CryptoRAM_rev(device, &temp, size, offset + 1, true);
}

uint32_t point2CryptoRAM(void *device, block_t *src, uint32_t size, uint32_t offset)
{
	block_t temp;
	uint32_t status;

	if (src->len < size * 2) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	status = mem2CryptoRAM(device, src, size, offset, true);

	if (status) {
		return status;
	}

	temp.addr = src->addr + size;
	temp.addr_type = src->addr_type;
	temp.len = src->len - size;
	status = mem2CryptoRAM(device, &temp, size, offset + 1, true);
	return status;
}

uint32_t CryptoRAM2point_rev(void *device, block_t *dst, uint32_t size, uint32_t offset)
{
	uint32_t status;
	block_t temp;

	if (dst->len < size * 2) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	status  = CryptoRAM2mem_rev(device, dst, size, offset, true);

	if (status) {
		return status;
	}

	temp.addr = dst->addr + size;
	temp.addr_type = dst->addr_type;
	temp.len = dst->len - size;
	status = CryptoRAM2mem_rev(device, &temp, size, offset + 1, true);
	return status;
}

uint32_t CryptoRAM2point(void *device, block_t *dst, uint32_t size, uint32_t offset)
{
	uint32_t status;
	block_t temp;

	if (dst->len < size * 2) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	status  = CryptoRAM2mem(device, dst, size, offset, true);

	if (status) {
		return status;
	}

	temp.addr = dst->addr + size;
	temp.addr_type = dst->addr_type;
	temp.len = dst->len - size;
	status = CryptoRAM2mem(device, &temp, size, offset + 1, true);
	return status;
}

uint32_t mem2CryptoRAM_rev(void *device, block_t *src, uint32_t size, uint32_t offset, bool cache_op)
{
	uint32_t status;
	uint32_t i = 0;
	block_t dst;
	block_t src_temp;
	uint8_t *temp_buf;
	struct mem_node *mem_n;

	if (!src->len || !size || (size > RSA_MAX_SIZE)) {
		pr_info("mem2CryptoRAM_rev: Src:%d or size:%d is null (skip) !\n", src->len, size);
		return CRYPTOLIB_INVALID_PARAM;
	}

	//buffer for bytes order reverse
#if 1
	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		temp_buf = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	memset(temp_buf, 0, RSA_MAX_SIZE);

	dst.addr = ((uint8_t *)(addr_t)BA414EP_ADDR_MEMLOC(offset - 1, 0));
	dst.addr_type = PKE_INTERNAL;
	dst.len = size;
	clean_cache((addr_t)temp_buf, RSA_MAX_SIZE);
	src_temp.addr = temp_buf;
	src_temp.addr_type = EXT_MEM;
	src_temp.len = size;

	//reverse byte order
	for (i = 0; i < size; i++) {
		temp_buf[i] = src->addr[size - i - 1];
	}

	status = memcpy_blk_common(device, &dst, &src_temp, size, cache_op);

	ce_free(mem_n);

#else
	dst.addr = ((uint8_t *)(addr_t)BA414EP_ADDR_MEMLOC(offset - 1, 0));
	dst.addr_type = PKE_INTERNAL;
	dst.len = size;
	dma_reverse(src->addr, size);
	status = memcpy_blk_common(device, &dst, src, size, cache_op);
#endif

	if (status) {
		return status;
	}

	return status;
}

uint32_t CryptoRAM2mem_rev(void *device, block_t *dst, uint32_t size, uint32_t offset, bool cache_op)
{
	block_t src;
	block_t dst_temp;
	uint32_t status;
	uint8_t *temp_buf;
	struct mem_node *mem_n;
	uint32_t i;
	//buffer for bytes order reverse

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		temp_buf = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	dst_temp.addr = temp_buf;
	dst_temp.addr_type = EXT_MEM;
	dst_temp.len = dst->len;

	src.addr = ((uint8_t *)(addr_t)BA414EP_ADDR_MEMLOC(offset - 1, 0));
	src.addr_type = PKE_INTERNAL;
	src.len = size;

	status = memcpy_blk_common(device, &dst_temp, &src, size, cache_op);

	if (status) {
		pr_info("CryptoRAM2mem_rev: status=%x", status);
		ce_free(mem_n);
		return status;
	}

	//reverse byte order
	for (i = 0; i < size; i++) {
		//pr_info("CryptoRAM2mem_rev: temp_buf %d = %x!\n",(size - i - 1),temp_buf[size - i - 1]);
		dst->addr[i] = temp_buf[size - i - 1];
	}

	ce_free(mem_n);
	return status;
}

uint32_t mem2CERAM(void *device,
				   block_t *src,
				   uint32_t size,
				   uint32_t offset,
				   bool be_param,
				   bool cache_op)
{
	return be_param ? (mem2CryptoRAM_rev(device, src, size, offset, cache_op)) : (mem2CryptoRAM(device, src, size, offset, cache_op));
}

uint32_t CERAM2mem(void *device,
				   block_t *dst,
				   uint32_t size,
				   uint32_t offset,
				   bool be_param,
				   bool cache_op)
{
	return be_param ? (CryptoRAM2mem_rev(device, dst, size, offset, cache_op)) : (CryptoRAM2mem(device, dst, size, offset, cache_op));
}


uint32_t mem2CryptoRAM(void *device, block_t *src, uint32_t size, uint32_t offset, bool cache_op)
{
	uint32_t ret;
	block_t dst;

	dst.addr = ((uint8_t *)(addr_t)BA414EP_ADDR_MEMLOC(offset - 1, 0));
	dst.addr_type = PKE_INTERNAL;
	dst.len = size;

	ret = memcpy_blk_common(device, &dst, src, size, cache_op);
	return ret;
}

uint32_t CryptoRAM2mem(void *device, block_t *dst, uint32_t size, uint32_t offset, bool cache_op)
{
	uint32_t ret;
	block_t src;

	src.addr = ((uint8_t *)(addr_t)BA414EP_ADDR_MEMLOC(offset - 1, 0));
	src.addr_type = PKE_INTERNAL;
	src.len = size;

	ret = memcpy_blk_common(device, dst, &src, size, cache_op);
	return ret;
}

int memcmp_rev(uint8_t *src, const uint8_t *dst, uint32_t len)
{
	/*
	uint8_t temp_buf[RSA_MAX_SIZE];

	for (uint32_t i = 0; i < len; i++) {
	    temp_buf[i] = src[len - i - 1];
	}

	return memcmp(temp_buf, dst, len);
	*/
	const unsigned char *su1, *su2;
	int res = 0;

	for (su1 = src + len - 1, su2 = dst; 0 < len; --su1, ++su2, len--)
		if ((res = *su1 - *su2) != 0) {
			//pr_info("memcmp_rev: error at:%d byte, src=%d ,dst=%d ,src at %p!\n", len, *su1, *su2, src);
			break;
		}

	return res;
}
