/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <linux/kernel.h>
#include "ce_reg.h"
#include "ce.h"
#include "sx_cipher.h"
#include "sram_conf.h"
#include "sx_errors.h"
#include "sx_dma.h"
#include "ce.h"

#define LOCAL_TRACE 0 //close local trace 1->0

/**
 * @brief Verify the AES key (and in case of ::XTS, the key2) length are valid.
 * @param fct mode of operation of AES. Used only for ::XTS.
 * @param len length of the key (first key in ::XTS)
 * @param xts_len length of the secondary key used in ::XTS
 * @return true if key(s) length is/are valid.
 */
static bool IsKeyLenValid(aes_fct_t fct, uint32_t len, uint32_t xts_len)
{
	bool valid = len == 16 || (len == 24) || len == 32;

	if (fct == FCT_XTS) {
		return valid && len == xts_len;
	}

	return valid && xts_len == 0;
}

/**
 * @brief Verify that the size of the IV or the context passed to the BA411 is valid
 * @param fct mode of operation of AES, it determines with the context the expected length
 * @param ctx current state for the context, it determines with the mode the expected length
 * @param len length of the IV for full message (::CTX_WHOLE) or for beginning of message (::CTX_BEGIN), context for oters cases (::CTX_MIDDLE or ::CTX_END)
 * @return true if IV/context length is valid.
 */
static bool IsIVContextLenValid(aes_fct_t fct, aes_ctx_t ctx, uint32_t len)
{
	// Use a context switching, save it and add it later is mathematically equal
	// to the IV mechanism. So, at first iteration, it is a "true" IV which is
	// injected in the AES. For the following iteration the context replaces the
	// IV but works exactly the same way
	switch (fct) {
		case FCT_ECB:
			return !len;

		case FCT_CMAC:
			if (ctx == CTX_BEGIN || ctx == CTX_WHOLE) {
				return !len;
			}

			break;

		case FCT_CCM:
			if (ctx == CTX_BEGIN || ctx == CTX_WHOLE) {
				return !len;
			}

			return len == AES_CTX_xCM_SIZE;

		case FCT_GCM:
			if (ctx == CTX_BEGIN || ctx == CTX_WHOLE) {
				return len == AES_IV_GCM_SIZE;
			}

			return len == AES_CTX_xCM_SIZE;

		default:
			break;
	}

	return len == AES_IV_SIZE;
}

/**
 * @brief Verify that the next-context size to read from BA411 is valid
 * @param fct mode of operation of AES, it determines with the context the expected length
 * @param ctx current state for the context, it determines with the mode the expected length
 * @param len length of the next context
 * @return true if next context length is valid.
 */
static bool IsNextContextLenValid(aes_fct_t fct, aes_ctx_t ctx, uint32_t len)
{
	if (fct == FCT_ECB || ctx == CTX_WHOLE) {
		return !len;
	}
	else if (fct == FCT_GCM || fct == FCT_CCM) {
		return len == AES_CTX_xCM_SIZE;
	}

	return len == AES_CTX_SIZE;
}

/**
 * @brief Verify that the data (payload) size is valid
 * @param fct mode of operation of AES, it determines with the context the expected length
 * @param ctx current state for the context, it determines with the mode the expected length
 * @param len length of the payload
 * @return true if payload length is valid.
 */
static bool IsPayloadLenValid(aes_fct_t fct, aes_ctx_t ctx, uint32_t len, uint32_t header_len)
{
	// Context check

	if (ctx == CTX_BEGIN || ctx == CTX_MIDDLE) {
		if (fct != FCT_GCM) {
			return !((len + header_len) % 16);
		}
	}

#if 0

	// Mode check
	if (fct == ECB || fct == OFB || fct == CFB) {
		return !(len % 16) && len;
	}
	else if (fct == CBC || fct == XTS) {
		if (ctx == CTX_WHOLE) {
			return len >= 16;
		}

		return len;
	}

#endif
	return len > 0;
}

/**
 * @brief Verify the differents inputs of sx_aes_blk
 * @param dir specify the direction of aes, in encryption or in decryption
 * @param fct mode of operation of AES, it determines with the context the expected lengths
 * @param ctx current state for the context, it determines with the mode the expected lengths
 * @param key pointer to the AES key.
 * @param xtskey pointer to the XTS key.
 * @param iv pointer to initialization vector. Used for \p aes_fct ::CBC, ::CTR, ::CFB, ::OFB, ::XTS and ::GCM, mode of operation. 16 bytes will be read.
 * @param data_in pointer to input data (plaintext or ciphertext).
 * @param aad pointer to additional authenticated data. Used for \p aes_fct ::GCM mode of operation.
 * @param tag pointer to the authentication tag. Used for ::GCM mode of operation. 16 bytes will be written.
 * @param ctx_ptr pointer to the AES context (after operation)
 * @param nonce_len_blk pointer to the lenAlenC data (AES GCM context switching only) or the AES CCM Nonce
 * @return ::CRYPTOLIB_SUCCESS if inputs are all valid, otherwise ::CRYPTOLIB_INVALID_PARAM.
 */
static uint32_t sx_aes_validate_input(aes_mode_t dir,
									  aes_fct_t fct,
									  aes_ctx_t ctx,
									  block_t key,
									  block_t xtskey,
									  block_t iv,
									  block_t ctx_ptr,
									  block_t data_in,
									  block_t data_out,
									  uint32_t header_len)
{
	if (KEY_INT == data_in.addr_type) {
		return CRYPTOLIB_INVALID_PARAM_INPUT;
	}

	if (KEY_INT == data_out.addr_type) {
		return CRYPTOLIB_INVALID_PARAM_OUTPUT;
	}

	if (dir != MODE_ENC && dir != MODE_DEC) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (!IsKeyLenValid(fct, key.len, xtskey.len)) {
		return CRYPTOLIB_INVALID_PARAM_KEY;
	}

	if (!IsIVContextLenValid(fct, ctx, iv.len)) {
		return CRYPTOLIB_INVALID_PARAM_IV;
	}

	if (!IsNextContextLenValid(fct, ctx, ctx_ptr.len)) {
		return CRYPTOLIB_INVALID_PARAM_CTX;
	}

	if (!IsPayloadLenValid(fct, ctx, data_in.len, header_len)) {
		return CRYPTOLIB_INVALID_PARAM_INPUT;
	}

	return CRYPTOLIB_SUCCESS;
}

uint32_t aes_blk(aes_fct_t aes_fct,
				 void *device,
				 aes_mode_t mode,
				 aes_ctx_t ctx,
				 uint32_t header_len,
				 bool header_save,
				 bool pre_cal_key,
				 block_t key,
				 block_t xtskey,
				 block_t iv,
				 block_t ctx_ptr,
				 block_t data_in,
				 block_t *data_out)
{
	uint32_t value;
	uint32_t key_size_conf;
	uint32_t vce_id, i = 0, value_state;
	uint32_t ret;

	if (device == NULL || block_t_check_wrongful(&key) ||
			block_t_check_wrongful(&data_in) || block_t_check_wrongful(data_out)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	vce_id = ((struct crypto_dev *)device)->ce_id;

	/* Step 0: input validation */
	ret = sx_aes_validate_input(mode, aes_fct, ctx, key, xtskey, iv, ctx_ptr, data_in, *data_out, header_len);

	if (ret) {
		return ret;
	}

	/* config header_len/key/src/dst/iv/context registers */
	value = reg_value(header_len, 0, CE_CIPHER_HEADER_LEN_SHIFT, CE_CIPHER_HEADER_LEN_MASK);
	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_HEADER_LEN_CE_(vce_id)));

	if (EXT_MEM == key.addr_type) {
		//copy and set to sec-sram
		ret = memcpy_blk_cache(device, &BLOCK_T_CONV(CE2_SRAM_SEC_AES_KEY_ADDR_OFFSET,
							   key.len, SRAM_SEC), &key, key.len, true, false);

		if (ret) {
			return CRYPTOLIB_INVALID_PARAM_KEY;
		}

		value = reg_value(CE2_SRAM_SEC_AES_KEY_ADDR_OFFSET, 0, CE_CIPHER_KEY_ADDR_SHIFT, CE_CIPHER_KEY_ADDR_MASK);
	}
	else {
		value = reg_value((addr_t)key.addr, 0, CE_CIPHER_KEY_ADDR_SHIFT, CE_CIPHER_KEY_ADDR_MASK);
	}

	if (FCT_XTS == aes_fct) {
		if (EXT_MEM == xtskey.addr_type) {
			//copy and set to sec-sram
			ret = memcpy_blk_cache(device, &BLOCK_T_CONV(CE2_SRAM_SEC_AES_XKEY_ADDR_OFFSET,
								   xtskey.len, SRAM_SEC), &xtskey, xtskey.len, true, false);

			if (ret) {
				return CRYPTOLIB_INVALID_PARAM_KEY;
			}

			value = reg_value(CE2_SRAM_SEC_AES_XKEY_ADDR_OFFSET, value, CE_CIPHER_KEY2_ADDR_SHIFT, CE_CIPHER_KEY2_ADDR_MASK);
		}
		else {
			value = reg_value((addr_t)xtskey.addr, value, CE_CIPHER_KEY2_ADDR_SHIFT, CE_CIPHER_KEY2_ADDR_MASK);
		}
	}

	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_KEY_ADDR_CE_(vce_id)));

	if (EXT_MEM == data_in.addr_type) {
		value = reg_value(_paddr((void *)data_in.addr), 0, CE_CIPHER_SRC_ADDR_SHIFT, CE_CIPHER_SRC_ADDR_MASK0);
	}
	else {
		value = reg_value((addr_t)data_in.addr, 0, CE_CIPHER_SRC_ADDR_SHIFT, CE_CIPHER_SRC_ADDR_MASK0);
	}

	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_SRC_ADDR_CE_(vce_id)));

	value = reg_value(switch_addr_type(data_in.addr_type), 0, CE_CIPHER_SRC_TYPE_SHIFT, CE_CIPHER_SRC_TYPE_MASK);
	value = reg_value(_paddr((void *)data_in.addr) >> 32, value, CE_CIPHER_SRC_ADDR_H_SHIFT, CE_CIPHER_SRC_ADDR_H_MASK);
	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_SRC_ADDR_H_CE_(vce_id)));

	if (EXT_MEM == data_out->addr_type) {
		value = reg_value(_paddr((void *)data_out->addr), 0, CE_CIPHER_DST_ADDR_SHIFT, CE_CIPHER_DST_ADDR_MASK);
	}
	else {
		value = reg_value((addr_t)data_out->addr, 0, CE_CIPHER_DST_ADDR_SHIFT, CE_CIPHER_DST_ADDR_MASK);
	}

	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_DST_ADDR_CE_(vce_id)));

	value = reg_value(switch_addr_type(data_out->addr_type), 0, CE_CIPHER_DST_TYPE_SHIFT, CE_CIPHER_DST_TYPE_MASK);
	value = reg_value(_paddr((void *)data_out->addr) >> 32, value, CE_CIPHER_DST_ADDR_H_SHIFT, CE_CIPHER_DST_ADDR_H_MASK);
	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_DST_ADDR_H_CE_(vce_id)));

	if (EXT_MEM == ctx_ptr.addr_type) {
		//copy and set to sec-sram
		if (0 != ctx_ptr.len) {
			ret = memcpy_blk_cache(device, &BLOCK_T_CONV(CE2_SRAM_SEC_AES_CTX_ADDR_OFFSET,
								   ctx_ptr.len, SRAM_SEC), &ctx_ptr, ctx_ptr.len, true, false);

			if (ret) {
				return CRYPTOLIB_INVALID_PARAM_KEY;
			}
		}

		value = reg_value(CE2_SRAM_SEC_AES_CTX_ADDR_OFFSET, 0, CE_CIPHER_CONTEXT_ADDR_SHIFT, CE_CIPHER_CONTEXT_ADDR_MASK);
	}
	else {
		value = reg_value((addr_t)ctx_ptr.addr, 0, CE_CIPHER_CONTEXT_ADDR_SHIFT, CE_CIPHER_CONTEXT_ADDR_MASK);
	}

	if (FCT_ECB != aes_fct) {
		if ((EXT_MEM == iv.addr_type) && (0 != iv.len)) {
			//copy and set to sec-sram
			ret = memcpy_blk_cache(device, &BLOCK_T_CONV(CE2_SRAM_SEC_AES_IV_ADDR_OFFSET,
								   iv.len, SRAM_SEC), &iv, iv.len, true, false);

			if (ret) {
				return CRYPTOLIB_INVALID_PARAM_KEY;
			}

			value = reg_value(CE2_SRAM_SEC_AES_IV_ADDR_OFFSET, value, CE_CIPHER_IV_ADDR_SHIFT, CE_CIPHER_IV_ADDR_MASK);
		}
		else {
			value = reg_value((addr_t)iv.addr, value, CE_CIPHER_IV_ADDR_SHIFT, CE_CIPHER_IV_ADDR_MASK);
		}
	}

	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_IV_CONTEXT_ADDR_CE_(vce_id)));

	value = reg_value(data_in.len, 0, CE_CIPHER_PAYLOAD_LEN_SHIFT, CE_CIPHER_PAYLOAD_LEN_MASK);
	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_PAYLOAD_LEN_CE_(vce_id)));

	/* config control register */
	value = reg_value(header_save, 0, CE_CIPHER_CTRL_HEADER_SAVE_SHIFT, CE_CIPHER_CTRL_HEADER_SAVE_MASK);

	if (FCT_XTS == aes_fct) {
		value = reg_value(xtskey.addr_type, value, CE_CIPHER_CTRL_KEY2TYPE_SHIFT, CE_CIPHER_CTRL_KEY2TYPE_MASK);
	}

	if (EXT_MEM == key.addr_type) {
		value = reg_value(SRAM_SEC, value, CE_CIPHER_CTRL_KEYTYPE_SHIFT, CE_CIPHER_CTRL_KEYTYPE_MASK);
	}
	else {
		value = reg_value(key.addr_type, value, CE_CIPHER_CTRL_KEYTYPE_SHIFT, CE_CIPHER_CTRL_KEYTYPE_MASK);
	}

	value = reg_value(1 << aes_fct, value, CE_CIPHER_CTRL_AESMODE_SHIFT, CE_CIPHER_CTRL_AESMODE_MASK);

	if (CTX_BEGIN == ctx || CTX_MIDDLE == ctx) {
		value = reg_value(1, value, CE_CIPHER_CTRL_SAVE_SHIFT, CE_CIPHER_CTRL_SAVE_MASK);
	}

	if (CTX_MIDDLE == ctx || CTX_END == ctx) {
		value = reg_value(1, value, CE_CIPHER_CTRL_LOAD_SHIFT, CE_CIPHER_CTRL_LOAD_MASK);
	}

	value = reg_value((pre_cal_key || MODE_DEC == mode) ? 1 : 0, value, CE_CIPHER_CTRL_DECKEYCAL_SHIFT, CE_CIPHER_CTRL_DECKEYCAL_MASK);

	switch (key.len) {
		case 16:
			key_size_conf = SX_KEY_TYPE_128;
			break;

		case 24:
			key_size_conf = SX_KEY_TYPE_192;
			break;

		case 32:
			key_size_conf = SX_KEY_TYPE_256;
			break;

		default: // Should not arrive, checked in sx_aes_validate_input
			return CRYPTOLIB_UNSUPPORTED_ERR;
	}

	value = reg_value(key_size_conf, value, CE_CIPHER_CTRL_KEYSIZE_SHIFT, CE_CIPHER_CTRL_KEYSIZE_MASK);
	value = reg_value(mode, value, CE_CIPHER_CTRL_CIPHERMODE_SHIFT, CE_CIPHER_CTRL_CIPHERMODE_MASK);
	value = reg_value(1, value, CE_CIPHER_CTRL_GO_SHIFT, CE_CIPHER_CTRL_GO_MASK);

	//clean cache to assure memory is fresh
	clean_cache_block(&key, vce_id);
	clean_cache_block(&xtskey, vce_id);
	clean_cache_block(&iv, vce_id);
	//some crypto type has head msg in data_in buff
	data_in.len = data_in.len + header_len;
	clean_cache_block(&data_in, vce_id);

	invalidate_cache_block(data_out, vce_id);

	writel(value, (((struct crypto_dev *)device)->base + REG_CIPHER_CTRL_CE_(vce_id)));

#if 1 //WAIT_PK_WITH_REGISTER_POLLING

	while (readl((((struct crypto_dev *)device)->base + REG_STAT_CE_(vce_id))) & 0x4) {
		i++;

		if (i > 5) {
			i = 0;
		}
	}

#else
	//wait interrupt
	event_wait(&g_ce_signal[vce_id]);
#endif

	value_state = readl((((struct crypto_dev *)device)->base + REG_ERRSTAT_CE_(vce_id))) & 0x3F00;

	if (0 != value_state) {
		//clear err status
		value_state = reg_value(1, 0xFFFFFFFF, CE_CIPHER_INTEGRITY_ERROR_INTSTAT_SHIFT, CE_CIPHER_INTEGRITY_ERROR_INTSTAT_MASK);
		writel(value_state, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));
		value_state = reg_value(0, 0xFFFFFFFF, CE_CIPHER_INTEGRITY_ERROR_INTSTAT_SHIFT, CE_CIPHER_INTEGRITY_ERROR_INTSTAT_MASK);
		writel(value_state, (((struct crypto_dev *)device)->base + REG_INTCLR_CE_(vce_id)));
	}

	return CRYPTOLIB_SUCCESS;
}
