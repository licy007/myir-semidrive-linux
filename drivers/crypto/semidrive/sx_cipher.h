/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_CIPHER_H
#define SX_CIPHER_H

#include "ce.h"

/** @brief Size for IV in GCM mode */
#define AES_IV_GCM_SIZE       16//12
/** @brief Size for IV in all modes except GCM */
#define AES_IV_SIZE           16
/** @brief Size for Context in GCM and CCM modes */
#define AES_CTX_xCM_SIZE      32
/** @brief Size for Context in all modes except GCM and CCM */
#define AES_CTX_SIZE          16

/**
* @brief Enumeration of possible mode of operation for AES algorithm.
*/
typedef enum aes_fct_t {
	FCT_ECB  = 0x0,              /**< Electronic Codebook */
	FCT_CBC  = 0x1,              /**< Cipher Block Chaining */
	FCT_CTR  = 0x2,              /**< Counter Feedback */
	FCT_CFB  = 0x3,              /**< Cipher Feedback */
	FCT_OFB  = 0x4,              /**< Output Feedback */
	FCT_CCM  = 0x5,              /**< Counter with CBC-MAC Mode */
	FCT_GCM  = 0x6,              /**< Galois/Counter Mode */
	FCT_XTS  = 0x7,              /**< XEX-based tweaked-codebook mode with ciphertext stealing */
	FCT_CMAC  = 0x8,             /**< CMAC mode */
	FCT_NUM_MAX  = 0x9,
} aes_fct_t;

/**
* @brief Enumeration of possible key type for AES algorithm.
*/
typedef enum sx_aes_key_type {
	SX_KEY_TYPE_128 = 0,
	SX_KEY_TYPE_256 = 1,
	SX_KEY_TYPE_192 = 2
} sx_aes_key_type_t;

/**
* @brief Enumeration of possible mode for AES algorithm.
*/
typedef enum aes_mode {
	MODE_ENC = 0,            /**< Encrypt */
	MODE_DEC = 1,            /**< Decrypt but does not return any tag */
} aes_mode_t;

/**
* @brief Enumeration of possible context states for AES algorithm.
*/
typedef enum aes_ctx_t {
	CTX_WHOLE = 0,            /**< No context switching (whole message) */
	CTX_BEGIN = 1,            /**< Save context (operation is not final) */
	CTX_END = 2,              /**< Load context (operation is not initial) */
	CTX_MIDDLE = 3            /**< Save & load context (operation is not initial & not final) */
} aes_ctx_t;

/**
 * @brief AES generic function to handle any AES mode of operation.
 *        Encrypt or decrypt input to output, using aes_fct mode of operation.
 *
 * This function will handle AES operation depending of the mode used, for both
 * encryption/decryption and authentication.
 * It also handles the different cases of context switching.
 *
 * @param aes_fct  mode of operation for AES. See ::aes_fct_t
 * @param vce_id, which vce will be used
 * @param mode specify the direction of aes, in encryption or in decryption
 * @param ctx current state for the context
 * @param header_len specify the header of input data
 * @param header_save header save in output or not, even it is false, engine will fill '0' in head of output with header_len
 * @param pre_cal_key specify pre-caculate key or not
 * @param key pointer to the AES key.
 * @param xtskey pointer to the XTS key.
 * @param iv pointer to initialization vector. Used for aes_fct ::CBC, ::CTR, ::CFB, ::OFB, ::XTS and ::GCM, 16 bytes will be read.
 * @param ctx_ptr pointer to the AES context (after operation)
 * @param data_in pointer to input data (plaintext or ciphertext).
 * @param data_out pointer to output data (plaintext or ciphertext).
 *
 * @return CRYPTOLIB_SUCCESS when execution was successful,
 *         CRYPTOLIB_INVALID_SIGN_ERR when computed tag does
 *         not match input tag (i.e. message corruption)
 */
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
				 block_t *data_out);

#endif
