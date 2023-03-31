/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <linux/kernel.h>

#include "ce_reg.h"
#include "ce.h"
#include "sx_hash.h"
#include "sram_conf.h"
#include "sx_errors.h"
#include "sx_dma.h"

static void hash_run(void *device, hash_reg_config_t *reg_config)
{
	uint32_t value;
	uint32_t value_state;
	uint32_t vce_id;
	uint32_t i = 0;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	if (EXT_MEM == reg_config->key_addr_type) {
		value = reg_value((_paddr((void *)(addr_t)reg_config->key_addr)), 0, CE_HASH_KEY_ADDR_SHIFT, CE_HASH_KEY_ADDR_MASK);
	}
	else {
		value = reg_value((addr_t)reg_config->key_addr, 0, CE_HASH_KEY_ADDR_SHIFT, CE_HASH_KEY_ADDR_MASK);
	}

	if (EXT_MEM == reg_config->iv_type) {
		value = reg_value((_paddr((void *)(addr_t)reg_config->iv_addr)), value, CE_HASH_IV_ADDR_SHIFT, CE_HASH_IV_ADDR_MASK);
	}
	else {
		value = reg_value((addr_t)reg_config->iv_addr, value, CE_HASH_IV_ADDR_SHIFT, CE_HASH_IV_ADDR_MASK);
	}

	//pr_info("value = %x reg= 0x%x\n", value, REG_HASH_KEYIV_ADDR_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_HASH_KEYIV_ADDR_CE_(vce_id)));

	value = reg_value(reg_config->key_addr_type, 0, CE_HMAC_KEY_TYPE_SHIFT, CE_HMAC_KEY_TYPE_MASK);
	value = reg_value(reg_config->key_len, value, CE_HMAC_KEY_LEN_SHIFT, CE_HMAC_KEY_LEN_MASK);
	//pr_info("value = %x reg= 0x%x\n", value, REG_HMAC_KEY_CTRL_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_HMAC_KEY_CTRL_CE_(vce_id)));

	value = reg_value(reg_config->data_len, 0, CE_HASH_CALC_LEN_SHIFT, CE_HASH_CALC_LEN_MASK);
	//pr_info("value = %x reg= 0x%x\n", value, REG_HASH_CALC_LEN_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_HASH_CALC_LEN_CE_(vce_id)));

	if (switch_addr_type(EXT_MEM) == reg_config->src_type) {
		value = reg_value((_paddr((void *)(addr_t)reg_config->src_addr)), 0, CE_HASH_SRC_ADDR_SHIFT, CE_HASH_SRC_ADDR_MASK);
	}
	else {
		value = reg_value((addr_t)reg_config->src_addr, 0, CE_HASH_SRC_ADDR_SHIFT, CE_HASH_SRC_ADDR_MASK);
	}

	//pr_info("value = %x reg= 0x%x\n", value, REG_HASH_SRC_ADDR_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_HASH_SRC_ADDR_CE_(vce_id)));

	value = reg_value(reg_config->src_type, 0, CE_HASH_SRC_TYPE_SHIFT, CE_HASH_SRC_TYPE_MASK);
	value = reg_value(_paddr((void *)(addr_t)reg_config->src_addr) >> 32, value, CE_HASH_SRC_ADDR_H_SHIFT, CE_HASH_SRC_ADDR_H_MASK);
	//pr_info("value = %x reg= 0x%x\n", value, REG_HASH_SRC_ADDR_H_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_HASH_SRC_ADDR_H_CE_(vce_id)));

	if (switch_addr_type(EXT_MEM) == reg_config->dst_type) {
		value = reg_value((_paddr((void *)(addr_t)reg_config->dst_addr)), 0, CE_HASH_DST_ADDR_SHIFT, CE_HASH_DST_ADDR_MASK);
	}
	else {
		value = reg_value((addr_t)reg_config->dst_addr, 0, CE_HASH_DST_ADDR_SHIFT, CE_HASH_DST_ADDR_MASK);
	}

	value = reg_value(reg_config->dst_type, value, CE_HASH_DST_TYPE_SHIFT, CE_HASH_DST_TYPE_MASK);
	//pr_info("value = %x reg= 0x%x\n", value, REG_HASH_DST_ADDR_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_HASH_DST_ADDR_CE_(vce_id)));

	value = reg_value(reg_config->hmac_en, 0, CE_HASH_HMAC_EN_SHIFT, CE_HASH_HMAC_EN_MASK);
	value = reg_value(reg_config->key_load, value, CE_HASH_KEY_SHIFT, CE_HASH_KEY_MASK);
	value = reg_value(reg_config->iv_type, value, CE_HASH_INIT_SECURE_SHIFT, CE_HASH_INIT_SECURE_MASK);
	value = reg_value(reg_config->iv_load, value, CE_HASH_INIT_SHIFT, CE_HASH_INIT_MASK);
	value = reg_value(reg_config->update, value, CE_HASH_SHAUPDATE_SHIFT, CE_HASH_SHAUPDATE_MASK);
	value = reg_value(reg_config->padding, value, CE_HASH_PADDINGEN_SHIFT, CE_HASH_PADDINGEN_MASK);
	value = reg_value(reg_config->mode, value, CE_HASH_MODE_SHIFT, CE_HASH_MODE_MASK);
	value = reg_value(1, value, CE_HASH_GO_SHIFT, CE_HASH_GO_MASK);
	//pr_info("value = %x reg= 0x%x\n", value, REG_HASH_CTRL_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_HASH_CTRL_CE_(vce_id)));

#if 1

//of_set_sys_cnt_ce(15);
	while (!(readl(((struct crypto_dev *)device)->base + (REG_INTSTAT_CE_(vce_id))) & 0x8)) {
		i++;

		if (i > 10) {
			//pr_info("times: %d, CE_(%d) is busy, alg: 0x%x, state reg: 0x%x\n", i, vce_id, reg_config->mode, readl(_ioaddr(REG_INTSTAT_CE_(vce_id))));
		}
	}

//of_set_sys_cnt_ce(16);
#else
	//unmask_interrupt(ZONE_VCE0_INT);
	//event_wait(&g_ce_signal[vce_id]);
	//int sleep = 0;
	wait_event_interruptible(((struct crypto_dev *)device)->cehashwaitq, ((struct crypto_dev *)device)->hash_condition);
	((struct crypto_dev *)device)->hash_condition = 0;
	//pr_info("wait irq end in hash\n");
#endif

	value_state = readl((((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));
	value = reg_value(1, value_state, CE_HASH_DONE_INTCLR_SHIFT, CE_HASH_DONE_INTCLR_MASK);
	//pr_info("value = %x reg= 0x%x\n", value, REG_INTCLR_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));
	value = reg_value(0, value, CE_HASH_DONE_INTCLR_SHIFT, CE_HASH_DONE_INTCLR_MASK);
	//pr_info("value = %x reg= 0x%x\n", value, REG_INTCLR_CE_(vce_id));
	writel(value, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));

	value_state = readl((((struct crypto_dev *)device)->base + REG_ERRSTAT_CE_(vce_id))) & 0xF;

	if (0 != value_state) {
		//pr_info("CE_(%d) state: %d, error: 0x%x, control value: 0x%x\n", vce_id, (readl(((struct crypto_dev *)device)->base + (REG_STAT_CE_(vce_id))) & 0x8), value_state, value);
		//clear err status
		value_state = reg_value(1, 0x0, CE_HASH_INTEGRITY_ERROR_INTSTAT_SHIFT, CE_HASH_INTEGRITY_ERROR_INTSTAT_MASK);
		writel(value_state, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));
		value_state = reg_value(0, 0x0, CE_HASH_INTEGRITY_ERROR_INTSTAT_SHIFT, CE_HASH_INTEGRITY_ERROR_INTSTAT_MASK);
		writel(value_state, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));
	}
}

static uint32_t sx_hash_internal(void *device,
								 hash_alg_t hash_alg,
								 block_t *extra_in,
								 block_t *key,
								 op_type_t operation_type,
								 block_t *data_in,
								 block_t *data_out)
{
	uint32_t ret = 0;
	hash_reg_config_t reg_config;
	uint32_t hash_len;
	uint32_t ce_id;
	block_t ce_hash_out;

	hash_len = hash_get_digest_size(hash_alg);

	ce_id = ((struct crypto_dev *)device)->ce_id;

	if (hash_len > data_out->len) {
		return CRYPTOLIB_INVALID_PARAM_OUTPUT;
	}

	memset((void *)(&reg_config), 0, sizeof(reg_config));
	reg_config.mode = 1UL << hash_alg;

	/* config iv/context of register */
	if (0 == extra_in->len) {
		reg_config.iv_load = false;
	}
	else {
		reg_config.iv_load = true;
		reg_config.iv_addr = (addr_t)extra_in->addr;
		reg_config.iv_type = extra_in->addr_type; //switch_addr_type(extra_in->addr_type);
	}

	switch (operation_type) {
		/* complete hash operation: no need to load extra info, enable padding in
		 * hardware, output will be digest
		 */
		case OP_FULL_HASH:
			reg_config.update = false;
			reg_config.hmac_en = false;
			reg_config.key_load = false;
			reg_config.padding = 1;
			break;

		/* complete hmac operation: need to load extra info K0, enable padding in
		 * hardware, output will be digest
		 */
		case OP_FULL_HMAC:
			reg_config.update = false;
			reg_config.hmac_en = true;
			reg_config.key_load = true;
			reg_config.key_addr = (addr_t)key->addr;
			reg_config.key_addr_type = key->addr_type;

			reg_config.key_len = key->len;
			reg_config.padding = 1;
			break;

		/* partial hash operation: need to load initial state, don't enable padding in
		 * hardware, output will be state
		 */
		case OP_PART_HASH:
			reg_config.update = true;
			reg_config.hmac_en = false;
			reg_config.key_load = false;
			reg_config.padding = 0;
			break;

		/* final hash operation: need to load initial state, don't enable padding in
		 * hardware, output will be digest
		 */
		case OP_FINAL_HASH:
			reg_config.update = false;
			reg_config.hmac_en = false;
			reg_config.key_load = false;
			reg_config.padding = 0;
			break;

		default:
			return CRYPTOLIB_INVALID_PARAM;
	}

	//pr_info("data_in->addr= %p\n", data_in->addr);

	reg_config.src_addr = (addr_t)(data_in->addr);
	reg_config.src_type = switch_addr_type(data_in->addr_type);

	reg_config.dst_addr = (addr_t)(data_out->addr);
	reg_config.dst_type = switch_addr_type(data_out->addr_type);

	if ((EXT_MEM == data_out->addr_type) || ((NULL == data_out->addr))) {
		reg_config.dst_addr = CE2_SRAM_PUB_HASH_OUT_ADDR_OFFSET; //base address of public sram
		reg_config.dst_type = switch_addr_type(SRAM_PUB);
	}

	reg_config.data_len = data_in->len;

	clean_cache_block(data_in, ce_id);

	flush_cache((addr_t)(((struct crypto_dev *)device)->sram_base + CE2_SRAM_PUB_HASH_OUT_ADDR_OFFSET), data_out->len);

	hash_run(device, &reg_config);

	if ((EXT_MEM == data_out->addr_type) && (NULL != data_out->addr)) {

		ce_hash_out.addr = (void *)CE2_SRAM_PUB_HASH_OUT_ADDR_OFFSET;
		ce_hash_out.addr_type = SRAM_PUB;
		ce_hash_out.len = data_out->len;

		ret = memcpy_blk_cache(device, data_out, &ce_hash_out, data_out->len, false, true);

		if (ret) {
			return CRYPTOLIB_INVALID_PARAM_OUTPUT;
		}
	}

	return CRYPTOLIB_SUCCESS;
}

/**
 * internal function for SHA2 message padding
 * @param hash_alg hash function to use. See ::hash_alg_t.
 * @param data_len length of the input message to be padded
 * @param total_len length of all data hashed including the data hashed before using update and including data_len
 * @param padding output for padding, length must be equal or bigger than padding length
 * @return padsize length of the padding to be added
 */
static uint32_t sha_padding(hash_alg_t hash_alg, uint32_t remain_size, uint64_t total_len, block_t padding)
{
	uint32_t block_size = hash_get_block_size(hash_alg);
	uint32_t length_field_size = (hash_alg >= ALG_SHA384) ? 16 : 8;
	uint32_t pad_size = block_size;
	uint64_t len_in_bits = total_len, i, start;

	len_in_bits *= 8;

	if (padding.len < pad_size) {
		return 0;
	}

	/* As block_size is a power of 2, the modulo computation has been converted
	 * into a binary and operation. This avoids a divion operation which can
	 * be very slow on some chips. */
	if (block_size - remain_size < (length_field_size + 1)) {
		pad_size += block_size;
	}

	/* first bit of padding should be 1 */
	padding.addr[remain_size] = 0x80;

	/* calculate length of total data size */
	start = length_field_size;

	if (start > sizeof(len_in_bits)) {
		start = sizeof(len_in_bits);
	}

	/* followed by zeros until the length field. */
	memset((void *)(padding.addr + (remain_size + 1)), 0, pad_size - start - (remain_size + 1));

	/* write number of bits at end of padding in big endian order */
	for (i = start; i > 0; i--) {
		padding.addr[pad_size - i] = (len_in_bits >> ((i - 1) * 8)) & 0xFF;
	}

	return pad_size;
}

/**
 * internal function for SHA2 message padding
 * @param hash_alg hash function to use. See ::hash_alg_t.
 * @param data_len length of the input message to be padded
 * @param total_len length of all data hashed including the data hashed before using update and including data_len
 * @param padding output for padding, length must be equal or bigger than padding length
 * @return padsize length of the padding to be added
 */
static uint32_t md5_padding(hash_alg_t hash_alg, uint32_t remain_size, uint64_t total_len, block_t padding)
{
	uint32_t pad_size, available, start, i;
	uint64_t len_in_bits = total_len;
	len_in_bits *= 8;
	padding.addr[remain_size] = 0x80;
	available = 64 - remain_size - 1;
	pad_size = 64;

	start = 8 > sizeof(len_in_bits) ? sizeof(len_in_bits) : 8;

	if (available < start) {
		pad_size += 64;
		available += 64;
	}

	memset((void *)(padding.addr + (remain_size + 1)), 0, available - start);

	for (i = 0; i < start; i++) {
		padding.addr[pad_size - start + i] = (len_in_bits >> (i * 8)) & 0xFF;
	}

	return pad_size;
}

/* Public functions: properties */
uint32_t hash_get_digest_size(hash_alg_t hash_alg)
{
	switch (hash_alg) {
		case ALG_MD5:
			return MD5_DIGESTSIZE;

		case ALG_SHA1:
			return SHA1_DIGESTSIZE;

		case ALG_SHA224:
			return SHA224_DIGESTSIZE;

		case ALG_SHA256:
			return SHA256_DIGESTSIZE;

		case ALG_SHA384:
			return SHA384_DIGESTSIZE;

		case ALG_SHA512:
			return SHA512_DIGESTSIZE;

		case ALG_SM3:
			return SM3_DIGESTSIZE;

		default:
			return 0;
	}
}

uint32_t hash_get_block_size(hash_alg_t hash_alg)
{
	switch (hash_alg) {
		case ALG_MD5:
			return MD5_BLOCKSIZE;

		case ALG_SHA1:
			return SHA1_BLOCKSIZE;

		case ALG_SHA224:
			return SHA224_BLOCKSIZE;

		case ALG_SHA256:
			return SHA256_BLOCKSIZE;

		case ALG_SHA384:
			return SHA384_BLOCKSIZE;

		case ALG_SHA512:
			return SHA512_BLOCKSIZE;

		case ALG_SM3:
			return SM3_BLOCKSIZE;

		default:
			return 0;
	}
}

uint32_t hash_get_state_size(hash_alg_t hash_alg)
{

	switch (hash_alg) {
		case ALG_MD5:
			return MD5_INITSIZE;

		case ALG_SHA1:
			return SHA1_INITSIZE;

		case ALG_SHA224:
			return SHA224_INITSIZE;

		case ALG_SHA256:
			return SHA256_INITSIZE;

		case ALG_SHA384:
			return SHA384_INITSIZE;

		case ALG_SHA512:
			return SHA512_INITSIZE;

		case ALG_SM3:
			return SM3_INITSIZE;

		default:
			return 0;
	}
}

uint32_t hash_blk(
	void *device,
	hash_alg_t hash_alg,
	block_t *iv,
	block_t *data_in,
	block_t *data_out)
{
	uint32_t digest_len;
	uint32_t initsize;
	uint32_t ret;
	block_t hash_key;

	struct mem_node *mem_n;
	block_t padding_blk;

	if (device == NULL || block_t_check_wrongful(data_out)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	initsize = hash_get_state_size(hash_alg);

	if ((0 != iv->len) && (/*(NULL == iv->addr) || */(initsize != iv->len) || (KEY_INT == iv->addr_type) || (EXT_MEM == iv->addr_type))) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (NULL == data_in->addr && 0 != data_in->len) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	digest_len = hash_get_digest_size(hash_alg);

	if ((data_out->len < digest_len) || (NULL == data_out->addr)) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	hash_key.addr = NULL;
	hash_key.addr_type = 0;
	hash_key.len = 0;

	if ((0 == data_in->len) /*|| (NULL == data_in->addr)*/) {
		mem_n = ce_malloc(SHA512_BLOCKSIZE * 2);

		if (mem_n == NULL) {
			return CRYPTOLIB_PK_N_NOTVALID;
		}

		padding_blk.addr = mem_n->ptr;
		padding_blk.addr_type = EXT_MEM;
		padding_blk.len = SHA512_BLOCKSIZE * 2;

		if (ALG_MD5 == hash_alg) {
			padding_blk.len = md5_padding(hash_alg, 0, 0, padding_blk);
		}
		else {
			padding_blk.len = sha_padding(hash_alg, 0, 0, padding_blk);
		}

		ret = sx_hash_internal(device, hash_alg, iv, &hash_key, OP_FINAL_HASH, &padding_blk, data_out);
		ce_free(mem_n);
	}
	else {
		ret = sx_hash_internal(device, hash_alg, iv, &hash_key, OP_FULL_HASH,
							   data_in, data_out);
	}

	return ret;
}

uint32_t hmac_blk(
	void *device,
	hash_alg_t hash_alg,
	block_t *iv,
	block_t *key,
	block_t *data_in,
	block_t *data_out)
{
	uint32_t init_size;
	uint32_t digest_len;
	uint32_t ret;

	if (device == NULL || block_t_check_wrongful(key) || block_t_check_wrongful(data_in) ||
			block_t_check_wrongful(data_out)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	init_size = hash_get_state_size(hash_alg);

	if ((0 != iv->len) && (/*(NULL == iv->addr) || */(init_size != iv->len) || (KEY_INT == iv->addr_type) || (EXT_MEM == iv->addr_type))) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if ((0 == key->len) /*|| (NULL == key.addr)*/ || (EXT_MEM == key->addr_type)) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if ((0 == data_in->len)/* || (NULL == data_in->addr)*/) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	digest_len = hash_get_digest_size(hash_alg);

	if ((data_out->len < digest_len) || (NULL == data_out->addr)) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	ret = sx_hash_internal(device, hash_alg, iv, key, OP_FULL_HMAC, data_in, data_out);
	return ret;
}

uint32_t hash_update_blk(
	void *device,
	hash_alg_t hash_alg,
	bool first_part,
	block_t *state,
	block_t *data_in)
{
	uint32_t blocksize;
	block_t iv;
	block_t key;
	uint32_t digest_len;
	uint32_t init_size;
	uint32_t ret;

	if (device == NULL || block_t_check_wrongful(state) || block_t_check_wrongful(data_in)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	if ((0 == data_in->len) || (NULL == data_in->addr)) {
		//pr_info("hash_update_blk check data_in fail.\n");
		return CRYPTOLIB_INVALID_PARAM;
	}

	blocksize = hash_get_block_size(hash_alg);

	if (0 != (data_in->len & (blocksize - 1))) {
		//pr_info("hash_update_blk check data_in not align.\n");
		return CRYPTOLIB_INVALID_PARAM;
	}

	digest_len = hash_get_digest_size(hash_alg);
	init_size = hash_get_state_size(hash_alg);

	if (first_part) {
		iv.addr = NULL;
		iv.addr_type = SRAM_PUB;
		iv.len = 0;
	}
	else { //iv address must be sram
		iv.addr = (uint8_t *)CE2_SRAM_PUB_HASH_MULT_PART_UPDATE_IV_ADDR_OFFSET;
		iv.addr_type = SRAM_PUB;
		iv.len = init_size;

		if (EXT_MEM == state->addr_type) {
			ret = memcpy_blk_cache(device, &iv, state, init_size, true, true);//maybe memcpy is fast?

			if (ret) {
				return CRYPTOLIB_INVALID_PARAM_OUTPUT;
			}
		}
		else {
			iv.addr = state->addr;
			iv.addr_type = state->addr_type;
		}
	}

	key.addr = NULL;
	key.addr_type = SRAM_PUB;
	key.len = 0;

	return sx_hash_internal(device, hash_alg, &iv, &key, OP_PART_HASH, data_in, state);

}

/* multipart logic should refuse key_interface as input resources */
/* total len: couldn't exceed 4G*4G-1(bytes) in sha384 and sha512, if there is a need, please contact provider */
uint32_t hash_finish_blk(
	void *device,
	hash_alg_t hash_alg,
	block_t *state,
	block_t *data_in,
	block_t *data_out,
	uint64_t total_len)
{
	uint32_t digest_size;
	uint32_t block_size;
	uint32_t init_size;
	uint32_t remain_size;
	block_t iv;
	block_t key;

	uint8_t *padding;
	struct mem_node *mem_n;
	addr_t addr_base;

	block_t padding_blk;
	uint32_t ce_id;
	uint32_t ret;

	if (device == NULL || block_t_check_wrongful(state) || block_t_check_wrongful(data_in) ||
			block_t_check_wrongful(data_out)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	ce_id = ((struct crypto_dev *)device)->ce_id;

	if (NULL == data_in->addr) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	digest_size = hash_get_digest_size(hash_alg);
	init_size = hash_get_state_size(hash_alg);

	if (data_out->len < digest_size) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	iv.addr = (uint8_t *)CE2_SRAM_PUB_HASH_MULT_PART_UPDATE_IV_ADDR_OFFSET;
	iv.addr_type = SRAM_PUB;
	iv.len = init_size;

	if (EXT_MEM == state->addr_type) {
		ret = memcpy_blk_cache(device, &iv, state, init_size, true, true);//maybe memcpy is fast?

		if (ret) {
			return CRYPTOLIB_INVALID_PARAM_OUTPUT;
		}
	}
	else {
		iv.addr = state->addr;
		iv.addr_type = state->addr_type;
	}

	block_size = hash_get_block_size(hash_alg);
	remain_size = data_in->len & (block_size - 1);

	if ((0 == remain_size) && (0 != data_in->len)) {
		remain_size = data_in->len;
	}

	addr_base = (addr_t)(data_in->addr);

	mem_n = ce_malloc(SHA512_BLOCKSIZE * 2);

	if (mem_n != NULL) {
		padding = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	if ((SRAM_PUB == data_in->addr_type) || (SRAM_SEC == data_in->addr_type)) {
		addr_base = (addr_t)_vaddr(_paddr((void *)addr_base) + sram_addr(data_in->addr_type, ce_id));
	}

	memcpy((void *)padding, (void *)(addr_base + (data_in->len - remain_size)), remain_size);
	data_in->len -= remain_size;

	if (data_in->len > 0) {
		key.addr = NULL;
		key.addr_type = 0;
		key.len = 0;
		sx_hash_internal(device, hash_alg, &iv, &key, OP_PART_HASH, data_in, data_out);
	}

	padding_blk.addr = padding;
	padding_blk.addr_type = EXT_MEM;
	padding_blk.len = SHA512_BLOCKSIZE * 2;

	if (ALG_MD5 == hash_alg) {
		padding_blk.len = md5_padding(hash_alg, remain_size, total_len, padding_blk);
	}
	else {
		padding_blk.len = sha_padding(hash_alg, remain_size, total_len, padding_blk);
	}

	key.addr = NULL;
	key.addr_type = SRAM_PUB;
	key.len = 0;
	sx_hash_internal(device, hash_alg, &iv, &key, OP_FINAL_HASH, &padding_blk, data_out);

	ce_free(mem_n);

	return CRYPTOLIB_SUCCESS;
}
