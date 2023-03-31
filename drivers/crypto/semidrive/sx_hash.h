/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
* key location must be in the following list: SRAM public/secure area and efuse
* IV location must be in the following list: SRAM public/secure area
*/

#ifndef _SX_HASH_H
#define _SX_HASH_H

#include "ce.h"

/* Size of MD5 data block in bytes */
#define MD5_BLOCKSIZE      64
/* Size of MD5 initialization value in bytes */
#define MD5_INITSIZE       16
/* Size of MD5 digest in bytes */
#define MD5_DIGESTSIZE     16
/* Size of SHA1 data block in bytes */
#define SHA1_BLOCKSIZE     64
/* Size of SHA1 initialization value in bytes */
#define SHA1_INITSIZE      20
/* Size of SHA1 digest in bytes */
#define SHA1_DIGESTSIZE    20
/* Size of SHA224 data block in bytes */
#define SHA224_BLOCKSIZE   64
/* Size of SHA224 initialization value in bytes */
#define SHA224_INITSIZE    32
/* Size of SHA224 digest in bytes */
#define SHA224_DIGESTSIZE  28
/* Size of SHA256 data block in bytes */
#define SHA256_BLOCKSIZE   64
/* Size of SHA256 initialization value in bytes */
#define SHA256_INITSIZE    32
/* Size of SHA256 digest in bytes */
#define SHA256_DIGESTSIZE  32
/* Size of SHA384 data block in bytes */
#define SHA384_BLOCKSIZE   128
/* Size of SHA384 initialization value in bytes */
#define SHA384_INITSIZE    64
/* Size of SHA384 digest in bytes */
#define SHA384_DIGESTSIZE  48
/* Size of SHA512 data block in bytes */
#define SHA512_BLOCKSIZE   128
/* Size of SHA512 initialization value in bytes */
#define SHA512_INITSIZE    64
/* Size of SHA512 digest in bytes */
#define SHA512_DIGESTSIZE  64
/* Size of SHA512 data block in bytes */
#define SM3_BLOCKSIZE   64
/* Size of SHA512 initialization value in bytes */
#define SM3_INITSIZE    32
/* Size of SHA512 digest in bytes */
#define SM3_DIGESTSIZE  32
/* Maximum block size to be supported */
#define MAX_BLOCKSIZE   SHA512_BLOCKSIZE
/* Maximum digest size to be supported */
#define MAX_DIGESTSIZE  SHA512_DIGESTSIZE

/**
 Enumeration of the supported hash algorithms
*/
typedef enum hash_alg_t {
	ALG_MD5     = 0x0,
	ALG_SHA1    = 0x1,
	ALG_SHA224  = 0x2,
	ALG_SHA256  = 0x3,
	ALG_SHA384  = 0x4,
	ALG_SHA512  = 0x5,
	ALG_SM3     = 0x6
} hash_alg_t;

/**
 Enumeration of the supported hash operation
*/
typedef enum op_type_t {
	OP_FULL_HASH = 0x1,
	OP_FULL_HMAC = 0x2,
	OP_PART_HASH = 0x3,
	OP_FINAL_HASH = 0x4
} op_type_t;

typedef struct hash_reg_config_t {
	bool hmac_en;
	bool key_load;
	uint32_t iv_type;
	bool iv_load;
	bool update;
	bool padding;
	hash_alg_t mode;
	uint32_t key_addr;
	uint32_t iv_addr;
	uint32_t key_addr_type;
	uint32_t key_len;
	uint32_t data_len;
	addr_t src_addr;
	uint32_t src_type;
	addr_t dst_addr;
	uint32_t dst_type;
} hash_reg_config_t;

/**
 * Get digest size in bytes for the given  hash_alg
 * @param hash_alg hash function. See ::hash_alg_t.
 * @return digest size in bytes, or 0 if invalid  hash_alg
 */
uint32_t hash_get_digest_size(hash_alg_t hash_alg);

/**
 * Get block size in bytes for the given  hash_alg
 * @param hash_alg hash function. See ::hash_alg_t.
 * @return block size in bytes, or 0 if invalid  hash_alg
 */
uint32_t hash_get_block_size(hash_alg_t hash_alg);

/**
 * Get state size in bytes for the given  hash_alg
 * @param hash_alg hash function. See ::hash_alg_t.
 * @return state size in bytes, or 0 if invalid  hash_alg
 */
uint32_t hash_get_state_size(hash_alg_t hash_alg);

/**
 * Compute hash digest of the content of  data_in and write the result in  data_out.
 * @param hash_alg hash function to use. See ::hash_alg_t.
 * @param data_in input data to process
 * @param data_out output digest
 * @return CRYPTOLIB_SUCCESS if execution was successful
 */
uint32_t hash_blk(
	void *device,
	hash_alg_t hash_alg,
	block_t *iv,
	block_t *data_in,
	block_t *data_out);

/**
 * Compute HMAC of the content of  data_in and write the result in  data_out.
 * @param hash_alg hash function to use. See ::hash_alg_t.
 * @param key      HMAC key to use
 * @param data_in  input data to process
 * @param data_out output digest
 * @return CRYPTOLIB_SUCCESS if execution was successful
 */
uint32_t hmac_blk(
	void *device,
	hash_alg_t hash_alg,
	block_t *iv,
	block_t *key,
	block_t *data_in,
	block_t *data_out);

/**
 * Compute hash block operation with  state and  data input, and write result back to  state.
 * @param hash_alg hash function to use. See ::hash_alg_t.
 * @param first_part first part or not
 * @param state     input and output state required in context switching,
 *                  size must be equal to state size of  hash_alg
 * @param data_in   input data to process. Size must be multiple of the block size of  hash_alg
 * @return CRYPTOLIB_SUCCESS if execution was successful
 */
uint32_t hash_update_blk(
	void *device,
	hash_alg_t hash_alg,
	bool first_part,
	block_t *state,
	block_t *data_in);

/**
 * Finish a hash operation after zero or more update operations after doing necessary padding
 * @param hash_alg  hash function to use. See ::hash_alg_t.
 * @param state     input and output state required in context switching.
 *                  Size must be equal to state size of  hash_alg,
 *                  if it's the first operation must be filled with initialization values.
 * @param data_in   input data to process , can be of any length.
 * @param data_out  output of hashing the padded input
 * @param total_len length of all data hashed including the data hashed before using update and length of data_in
 * 					if the total_len cannot be expressed in 64 bits, sha384 and sha512 could not get correct result.
 * @return CRYPTOLIB_SUCCESS if execution was successful
 */
uint32_t hash_finish_blk(
	void *device,
	hash_alg_t hash_alg,
	block_t *state,
	block_t *data_in,
	block_t *data_out,
	uint64_t total_len);

#endif
