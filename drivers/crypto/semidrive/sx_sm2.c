/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <linux/kernel.h>
#include "sx_pke_conf.h"
#include "sx_hash.h"
#include "sx_errors.h"
#include "sx_dma.h"
#include "sx_sm2.h"
#include "sx_trng.h"
#include "sram_conf.h"

#define SM2_MAX_ID_LENGTH 16
#define SM2_ID_DIGEST_MSG_LEN_MAX 210 // id_(16) + id_len(2) + a/b(64) + x/y(64) + pubkey(64)

#if WITH_GB_PATTERN_VALUE
/*gb k：59276E27 D506861A 16680F3A D9C02DCC EF3CC1FA 3CDBE4CE 6D54B80D EAC1BC21*/
uint8_t __attribute__((aligned(CACHE_LINE))) sm2_sign_rnd_p256[32] =
	"\x59\x27\x6E\x27\xD5\x06\x86\x1A\x16\x68\x0F\x3A\xD9\xC0\x2D\xCC\xEF\x3C\xC1\xFA\x3C\xDB\xE4\xCE\x6D\x54\xB8\x0D\xEA\xC1\xBC\x21"; //-- k
/*gb k：59276E27 D506861A 16680F3A D9C02DCC EF3CC1FA 3CDBE4CE 6D54B80D EAC1BC21*/
uint8_t __attribute__((aligned(CACHE_LINE))) sm2_key_ex_rnd_p256[32] =
	"\x59\x27\x6E\x27\xD5\x06\x86\x1A\x16\x68\x0F\x3A\xD9\xC0\x2D\xCC\xEF\x3C\xC1\xFA\x3C\xDB\xE4\xCE\x6D\x54\xB8\x0D\xEA\xC1\xBC\x21"; //-- k

#else
uint8_t  __attribute__((aligned(CACHE_LINE))) sm2_sign_rnd_p256[32] =
	"\x6c\xb2\x8d\x99\x38\x5c\x17\x5c\x94\xf9\x4e\x93\x48\x17\x66\x3f"
	"\xc1\x76\xd9\x25\xdd\x72\xb7\x27\x26\x0d\xba\xae\x1f\xb2\xf9\x6f";
uint8_t  __attribute__((aligned(CACHE_LINE))) sm2_key_ex_rnd_p256[32] =
	"\x83\xa2\xc9\xc8\xb9\x6e\x5a\xf7\x0b\xd4\x80\xb4\x72\x40\x9a\x9a"
	"\x32\x72\x57\xf1\xeb\xb7\x3f\x5b\x07\x33\x54\xb2\x48\x66\x85\x63";
#endif
uint8_t  __attribute__((aligned(CACHE_LINE))) sm2_sign_rnd_p192[24] =
	"\xa5\xc8\x17\xa2\x36\xa5\xf7\xfa\xa3\x29\xb8\xec\xc3\xc5\x96\x68\x7c\x71\xaa\xaf\x86\xc7\x70\x3d";
uint8_t __attribute__((aligned(CACHE_LINE))) sm2_sign_rnd_p384[48] =
	"\x2e\x44\xef\x1f\x8c\x0b\xea\x83\x94\xe3\xdd\xa8\x1e\xc6\xa7\x84\x2a\x45\x9b\x53\x47\x01\x74\x9e"
	"\x2e\xd9\x5f\x05\x4f\x01\x37\x68\x08\x78\xe0\x74\x9f\xc4\x3f\x85\xed\xca\xe0\x6c\xc2\xf4\x3f\xef";
uint8_t __attribute__((aligned(CACHE_LINE))) sm2_sign_rnd_p521[66] =
	"\x01\xfc\x81\xab\xec\x3f\xab\x91\xa4\x39\xdf\x81\xeb\x49\xa7\x17"
	"\x92\x30\x86\x6c\x2e\x0e\x50\x5d\x69\x6c\xa3\x79\x72\xe4\xaf\x04"
	"\x3f\x6c\xe6\xed\xea\x69\xc5\x71\x1d\x14\xc4\x39\x05\xb3\xdc\xc9"
	"\xed\x48\xd2\x25\xd5\x0d\x8b\x13\x6b\x6f\x07\x87\x21\x5b\x35\x00"
	"\xbe\x52";
uint8_t  __attribute__((aligned(CACHE_LINE))) sm2_key_ex_rnd_p192[24] =
	"\xa5\xc8\x17\xa2\x36\xa5\xf7\xfa\xa3\x29\xb8\xec\xc3\xc5\x96\x68\x7c\x71\xaa\xaf\x86\xc7\x70\x3d";
uint8_t __attribute__((aligned(CACHE_LINE))) sm2_key_ex_rnd_p384[48] =
	"\x2e\x44\xef\x1f\x8c\x0b\xea\x83\x94\xe3\xdd\xa8\x1e\xc6\xa7\x84\x2a\x45\x9b\x53\x47\x01\x74\x9e"
	"\x2e\xd9\x5f\x05\x4f\x01\x37\x68\x08\x78\xe0\x74\x9f\xc4\x3f\x85\xed\xca\xe0\x6c\xc2\xf4\x3f\xef";
uint8_t __attribute__((aligned(CACHE_LINE))) sm2_key_ex_rnd_p521[66] =
	"\x01\xfc\x81\xab\xec\x3f\xab\x91\xa4\x39\xdf\x81\xeb\x49\xa7\x17"
	"\x92\x30\x86\x6c\x2e\x0e\x50\x5d\x69\x6c\xa3\x79\x72\xe4\xaf\x04"
	"\x3f\x6c\xe6\xed\xea\x69\xc5\x71\x1d\x14\xc4\x39\x05\xb3\xdc\xc9"
	"\xed\x48\xd2\x25\xd5\x0d\x8b\x13\x6b\x6f\x07\x87\x21\x5b\x35\x00"
	"\xbe\x52";

uint8_t __attribute__((aligned(CACHE_LINE))) z_msg[SM2_ID_DIGEST_MSG_LEN_MAX];

uint32_t sm2_configure_signature(void *device,
								 const void *curve_info,
								 block_t *formatted_digest,
								 block_t *key)
{
	uint32_t size;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;
	size = ecc_curve_bytesize(curve);
	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_SM2_SIGN_GEN, size, BA414EP_LITTLEEND, curve->pk_flags);

	// Load parameters
	pke_load_curve(device, &(curve->params), size, BA414EP_LITTLEEND, 1);

	/* Load SM2 parameters */
	mem2CryptoRAM_rev(device, key, size, BA414EP_MEMLOC_6, true);
	mem2CryptoRAM_rev(device, formatted_digest, size, BA414EP_MEMLOC_12, true);

	return CRYPTOLIB_SUCCESS;
}

uint32_t sm2_attempt_signature(void *device, const void *curve_info, block_t *signature)
{
	uint32_t status;
	uint32_t size;
	block_t rnd;
	uint8_t *rnd_buff;
	struct mem_node *mem_n;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;

	mem_n = ce_malloc(ECC_MAX_KEY_SIZE);

	if (mem_n != NULL) {
		rnd_buff = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	size = ecc_curve_bytesize(curve);
	rnd.addr = rnd_buff;
	rnd.addr_type = EXT_MEM;
	rnd.len = size;
	//TODO: use trng IP to generate random number after IP work
#if 1//rng not work on paladium
	trng_get_rand_by_fifo(device, rnd_buff, size);
#else
	/* just workaround for rng not work!!!!!!!*/
	uint8_t *rnd_const;

	switch (size) {
		case 24:
			rnd_const = sm2_sign_rnd_p192;
			break;

		case 32:
			rnd_const = sm2_sign_rnd_p256;
			break;

		case 48:
			rnd_const = sm2_sign_rnd_p384;
			break;

		case 66:
			rnd_const = sm2_sign_rnd_p521;
			break;

		default:
			rnd_const = sm2_sign_rnd_p521;
			break;
	}

	memcpy(rnd_buff, rnd_const, size);
	/* workaround end!!!!! */
#endif

	// Fetch the results by PKE config
#if AUTO_OUTPUT_BY_CE

	pke_set_dst_param(device, size, 0x1 << BA414EP_MEMLOC_10 | 0x1 << BA414EP_MEMLOC_11, _paddr((void *)signature->addr), signature->addr_type);

#endif
	mem2CryptoRAM_rev(device, &rnd, rnd.len, BA414EP_MEMLOC_7, true);

	/* SM2 signature generation */
	pke_set_command(device,
					BA414EP_OPTYPE_SM2_SIGN_GEN,
					size,
					BA414EP_LITTLEEND,
					curve->pk_flags);

	status = pke_start_wait_status(device);

	if (status & CE_PK_SIGNATURE_NOTVALID_MASK) {
		ce_free(mem_n);
		return CRYPTOLIB_INVALID_SIGN_ERR;
	}
	else if (status) {
		ce_free(mem_n);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	// Fetch the results
#if AUTO_OUTPUT_BY_CE
#else
	CryptoRAM2point_rev(device, signature, size, BA414EP_MEMLOC_10);
#endif
	ce_free(mem_n);
	return CRYPTOLIB_SUCCESS;
}

uint32_t sm2_generate_signature_digest(void *device,
									   const void *curve_info,
									   block_t *formatted_digest,
									   block_t *key,
									   block_t *signature)
{
	uint32_t ctr = 0;
	uint32_t status;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;

	status = sm2_configure_signature(device, curve, formatted_digest, key);

	if (status) {
		return status;
	}

	do {
		status = sm2_attempt_signature(device, curve, signature/*, rng*/);
		++ctr;
	}
	while ((status == CRYPTOLIB_INVALID_SIGN_ERR) && (ctr < 10));

	if (status) {
		return status;
	}

	return CRYPTOLIB_SUCCESS;
}

uint32_t sm2_verify_signature_digest(void *device,
									 const void *curve_info,
									 block_t *formatted_digest,
									 block_t *key,
									 block_t *signature)
{
	uint32_t ret;
	uint32_t size;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;

	size = ecc_curve_bytesize(curve);

	//pr_info("sm2_verify_signature_digest enter\n");
	//of_set_sys_cnt_ce(11);
	pke_set_command(device, BA414EP_OPTYPE_SM2_SIGN_VERIF, size, BA414EP_LITTLEEND, curve->pk_flags);
	//of_set_sys_cnt_ce(12);
	//pke_load_curve(vce_id, curve->params, size, BA414EP_LITTLEEND, 1);
	//pke_load_curve(device, &(curve->params), size, BA414EP_BIGEND, 1);
	pke_load_curve(device, &(curve->params), size, BA414EP_LITTLEEND, 1);

	//of_set_sys_cnt_ce(13);
	/* Load SM2 parameters */
	point2CryptoRAM_rev(device, key, size, BA414EP_MEMLOC_8);
	//of_set_sys_cnt_ce(14);
	mem2CryptoRAM_rev(device, formatted_digest, formatted_digest->len, BA414EP_MEMLOC_12, true);
	//of_set_sys_cnt_ce(15);
	point2CryptoRAM_rev(device, signature, size, BA414EP_MEMLOC_10);
	//of_set_sys_cnt_ce(16);

	/* SM2 signature verification */
	ret = pke_start_wait_status(device);
	return ret;
}

void sm2_load_curve(void *device, const void *curve_info, uint32_t swap)
{
	uint32_t size;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;

	size = ecc_curve_bytesize(curve);
	pke_load_curve(device, &(curve->params), size, swap, 1);
}

uint32_t sm2_verify_update_pubkey(void *device, block_t *key, uint32_t size)
{
	uint32_t ret;
	ret = point2CryptoRAM_rev(device, key, size, BA414EP_MEMLOC_8);
	return ret;
}

uint32_t sm2_verify_signature_digest_update(void *device,
		const void *curve_info,
		uint32_t update_curve,
		block_t *formatted_digest,
		block_t *key,
		uint32_t update_key,
		block_t *signature)
{
	uint32_t ret;
	uint32_t size;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;

	size = ecc_curve_bytesize(curve);

	//pr_info("sm2_verify_signature_digest enter\n");
	//of_set_sys_cnt_ce(11);
	pke_set_command(device, BA414EP_OPTYPE_SM2_SIGN_VERIF, size, BA414EP_LITTLEEND, curve->pk_flags);

	//pke_load_curve(vce_id, curve->params, size, BA414EP_LITTLEEND, 1);
	if (update_curve == 1) {
		pke_load_curve(device, &(curve->params), size, BA414EP_BIGEND, 1);
	}

	/* Load SM2 parameters */
	if (update_key != 0) {
		point2CryptoRAM_rev(device, key, size, BA414EP_MEMLOC_8);
	}

	//of_set_sys_cnt_ce(12);
	mem2CryptoRAM_rev(device, formatted_digest, size, BA414EP_MEMLOC_12, true);
	//of_set_sys_cnt_ce(13);
	point2CryptoRAM_rev(device, signature, size, BA414EP_MEMLOC_10);
	//of_set_sys_cnt_ce(14);
	/* SM2 signature verification */
	ret = pke_start_wait_status(device);
	return ret;
}

uint32_t math_array_nbits_sm2(uint8_t *a, const size_t length)
{
	uint32_t i, j;
	uint32_t nbits = 8 * length;

	for (i = 0; i < length; i++) {
		if (a[i] == 0) {
			nbits -= 8;
		}
		else {
			for (j = 7; j > 0; j--) {
				if (a[i] >> j) {
					break;
				}
				else {
					nbits--;
				}
			}

			break;
		}
	}

	return nbits;
}

/**
 * @brief perform a bit shift to the right of a large value stored in a byte array
 * @param array value to shift (input and output)
 * @param len length of the \p array
 * @param shift size of the bit shift (between 0 and 7)
 */
static void bitshift_sm2(uint8_t *array, uint8_t len, uint8_t shift)
{
	if (shift) {
		uint8_t prev, val;
		int i;
		prev = 0;

		for (i = 0; i < len; i++) {
			val = ((array[i] >> shift) & 0xFF) | prev;
			prev = array[i] << (8 - shift);
			array[i] = val;
		}
	}
}

/**
 * @brief perform digest computation used for signing or verification
 * @param curve SM2 curve to use
 * @param message message to sign or verify
 * @param hash_fct algorithm to use for hash
 * @param digest_blk computed digest
 * @return CRYPTOLIB_INVALID_PARAM when there is not enough space in the
 *                                 digest_blk to compute the digest
 *         CRYPTOLIB_SUCCESS otherwise
 */
uint32_t sm2_generate_digest(void *device, const void *curve_info,
							 block_t *message,
							 hash_alg_t hash_fct,
							 block_t *digest_blk)
{
	uint32_t i;
	uint32_t status;
	uint32_t dgst_local_len;
	uint8_t extra_bits;

	uint32_t curve_bytesize;
	uint32_t curve_order_bitsize;
	uint32_t curve_order_bytesize;
	uint32_t hash_len;
	const sx_ecc_curve_t *curve;
	block_t iv;

	curve = (sx_ecc_curve_t *)curve_info;

	curve_bytesize = ecc_curve_bytesize(curve);
	curve_order_bitsize = math_array_nbits_sm2(
							  curve->params.addr + curve_bytesize, curve_bytesize);
	curve_order_bytesize = (curve_order_bitsize + 7) / 8;
	hash_len = hash_get_digest_size(hash_fct);

	if (digest_blk->len < hash_len) {
		return CRYPTOLIB_INVALID_PARAM;
	}
	else {
		digest_blk->len = hash_len;
	}

	//pr_info("sm2_generate_digest enter\n");
	/* Call hash fct to get digest. */
	iv.addr = NULL;
	iv.addr_type = 0;
	iv.len = 0;

	status = hash_blk(device, hash_fct, &iv, message, digest_blk);

	if (status) {
		return status;
	}

	/* Define digest size. This only take the most significant bytes when curve
	 * is smaller than hash. If it's greater, leading zeroes will be inserted
	 * within sm2_signature_* functions.
	 */
	dgst_local_len = curve_order_bytesize > hash_len ? hash_len : curve_order_bytesize;

	/* Shorten the digest to match the expected length */
	digest_blk->len = dgst_local_len;

	/* Bitshift if needed, for curve smaller than digest and with order N not on
	 * bytes boundaries.
	 */
	extra_bits = (curve_order_bitsize & 0x7);

	if (extra_bits && (hash_len * 8 > curve_order_bitsize)) {
		bitshift_sm2(digest_blk->addr, dgst_local_len,  8 - extra_bits);
	}

	if (curve_bytesize > hash_len) {//insert 0 to meet key len
		for (i = 0; i < hash_len; i++) {
			*(digest_blk->addr + curve_bytesize - 1 - i) = *(digest_blk->addr + hash_len - 1 - i);
		}

		memset(digest_blk->addr, 0, curve_bytesize - hash_len);
		digest_blk->len = curve_bytesize;
	}

	return CRYPTOLIB_SUCCESS;
}

uint32_t sm2_generate_digest_sm3(void *device, const void *curve_info,
								 block_t *message,
								 hash_alg_t hash_fct,
								 block_t *digest_blk)
{

	uint32_t status;
	block_t iv;

	//pr_info("sm2_generate_digest enter\n");
	/* Call hash fct to get digest. */
	iv.addr = NULL;
	iv.addr_type = 0;
	iv.len = 0;

	status = hash_blk(device, hash_fct, &iv, message, digest_blk);

	return status;
}

uint32_t sm2_generate_signature(void *device,
								const void *curve_info,
								block_t *message,
								block_t *key,
								block_t *signature,
								hash_alg_t hash_fct)
{
	block_t digest_blk;
	uint32_t status;
	uint8_t *digest;
	struct mem_node *mem_n;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;

	mem_n = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n != NULL) {
		digest = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	digest_blk.addr = digest;
	digest_blk.addr_type = EXT_MEM;
	digest_blk.len = MAX_DIGESTSIZE;

	curve = (sx_ecc_curve_t *)curve_info;

	status = sm2_generate_digest(device, curve, message, hash_fct, &digest_blk);

	if (status) {
		ce_free(mem_n);
		return status;
	}

	status = sm2_generate_signature_digest(device, curve, &digest_blk, key, signature);
	ce_free(mem_n);
	return status;
}

uint32_t sm2_generate_signature_update(void *device,
									   const void *curve_info,
									   block_t *message,
									   block_t *key,
									   block_t *signature,
									   hash_alg_t hash_fct)
{
	block_t digest_blk;
	uint32_t status;
	uint8_t *digest;
	struct mem_node *mem_n;
	const sx_ecc_curve_t *curve;

	curve = (sx_ecc_curve_t *)curve_info;

	mem_n = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n != NULL) {
		digest = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	digest_blk.addr = digest;
	digest_blk.addr_type = EXT_MEM;
	digest_blk.len = MAX_DIGESTSIZE;

	curve = (sx_ecc_curve_t *)curve_info;

	status = sm2_generate_digest_sm3(device, curve, message, hash_fct, &digest_blk);

	if (status) {
		ce_free(mem_n);
		return status;
	}

	status = sm2_verify_signature_digest_update(device, curve, 0, &digest_blk, key, 1, signature);
	ce_free(mem_n);
	return status;
}

uint32_t sm2_verify_signature(void *device,
							  const void *curve_info,
							  block_t *message,
							  block_t *key,
							  block_t *signature,
							  hash_alg_t hash_fct)
{
	block_t digest_blk;
	uint32_t status;
	uint8_t *digest;
	struct mem_node *mem_n;
	const sx_ecc_curve_t *curve;
	//pr_info("sm2_verify_signature enter\n");

	mem_n = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n != NULL) {
		digest = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	digest_blk.addr = digest;
	digest_blk.addr_type = EXT_MEM;
	digest_blk.len = MAX_DIGESTSIZE;
	curve = (sx_ecc_curve_t *)curve_info;

	status = sm2_generate_digest(device, curve, message, hash_fct, &digest_blk);

	if (status) {
		ce_free(mem_n);
		return status;
	}

	status = sm2_verify_signature_digest(device, curve, &digest_blk, key, signature);
	ce_free(mem_n);
	return status;
}

uint32_t sm2_key_exchange(void *device,
						  const void *curve_info,
						  block_t *prv_key,
						  block_t *pub_key,
						  block_t *pointb,
						  block_t *cofactor,
						  block_t *pointa,
						  block_t *two_w,
						  block_t *ex_pub_key)
{
	uint32_t status;
	uint32_t size = 0;
	block_t rnd;
	const sx_ecc_curve_t *curve;
	uint8_t *rnd_buff;
	struct mem_node *mem_n;
	uint8_t *rnd_const;
	mem_n = ce_malloc(ECC_MAX_KEY_SIZE);

	if (mem_n != NULL) {
		rnd_buff = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	curve = (sx_ecc_curve_t *)curve_info;

	size = ecc_curve_bytesize(curve);
	rnd.addr = rnd_buff;
	rnd.addr_type = EXT_MEM;
	rnd.len = size;
	// Fetch the results by PKE config
#if AUTO_OUTPUT_BY_CE

	pke_set_dst_param(device, size, 0x1 << BA414EP_MEMLOC_13 | 0x1 << BA414EP_MEMLOC_14, _paddr((void *)ex_pub_key->addr), ex_pub_key->addr_type);

#endif

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_SM2_KEY_EXCAHNGE, size, BA414EP_LITTLEEND, curve->pk_flags);

	// Load parameters
	pke_load_curve(device, &(curve->params), size, BA414EP_LITTLEEND, 1);

	/* Load SM2 parameters */
	mem2CryptoRAM_rev(device, prv_key, size, BA414EP_MEMLOC_6, true);

	//TODO: use trng IP to generate random number after IP work
#if 0 //rng not work on paladium
	status = trng_get_rand_blk(vce_id, rnd);

	if (status) {
		ce_free(mem_n);
		return status;
	}

#else
	/* just workaround for rng not work!!!!!!!*/

	switch (size) {
		case 24:
			rnd_const = sm2_key_ex_rnd_p192;
			break;

		case 32:
			rnd_const = sm2_key_ex_rnd_p256;
			break;

		case 48:
			rnd_const = sm2_key_ex_rnd_p384;
			break;

		case 66:
			rnd_const = sm2_key_ex_rnd_p521;
			break;

		default:
			rnd_const = sm2_key_ex_rnd_p521;
			break;
	}

	memcpy(rnd_buff, rnd_const, size);
	/* workaround end!!!!! */
#endif

	mem2CryptoRAM_rev(device, &rnd, size, BA414EP_MEMLOC_7, true);

	point2CryptoRAM_rev(device, pub_key, size, BA414EP_MEMLOC_8);
	point2CryptoRAM_rev(device, pointb, size, BA414EP_MEMLOC_10);
	mem2CryptoRAM_rev(device, cofactor, size, BA414EP_MEMLOC_12, true);
	point2CryptoRAM_rev(device, pointa, size, BA414EP_MEMLOC_13);
	mem2CryptoRAM_rev(device, two_w, size, BA414EP_MEMLOC_15, true);

	status = pke_start_wait_status(device);

	if (status & CE_PK_SIGNATURE_NOTVALID_MASK) {
		ce_free(mem_n);
		return CRYPTOLIB_INVALID_SIGN_ERR;
	}
	else if (status) {
		ce_free(mem_n);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	// Fetch the results
#if AUTO_OUTPUT_BY_CE
#else
	CryptoRAM2point_rev(device, ex_pub_key, size, BA414EP_MEMLOC_13);
#endif
	ce_free(mem_n);
	return CRYPTOLIB_SUCCESS;
}

int sm2_compute_id_digest(void *device, const uint8_t *in, unsigned in_len, uint8_t *out,
						  unsigned out_len, const uint8_t *pubkey, unsigned pubkey_len)
{
	int ret = 0;
	uint8_t *z_msg_ptr;
	uint32_t z_msg_len = 0;
	block_t out_block;
	block_t in_block;

	//pr_info("crypto_compute_id_digest enter ");

	if (in_len > SM2_MAX_ID_LENGTH || in_len <= 0) {
		return 0;
	}

	//pr_info("crypto_compute_id_digest begin z_msg ");
	/*ZA= H256(ENTLA||IDA||a||b||xG||yG||xA||yA)*/
	z_msg_len = in_len + 194;

	z_msg_ptr = z_msg;
	memset(z_msg_ptr, 0x0, z_msg_len);

	/* 2-byte id length in bits */
	z_msg[0] = ((in_len * 8) >> 8) % 256;
	z_msg[1] = (in_len * 8) % 256;

	memcpy(z_msg_ptr + 2, in, in_len);
	memcpy(z_msg_ptr + 2 + in_len, sx_ecc_sm2_curve_p256.params.addr + 128, 64); //copy a b
	memcpy(z_msg_ptr + 2 + in_len + 64, sx_ecc_sm2_curve_p256.params.addr + 64, 64); //copy x y
	memcpy(z_msg_ptr + 2 + in_len + 64 + 64, pubkey, 64); //copy pubkey

	in_block.addr = z_msg_ptr;
	in_block.addr_type = EXT_MEM;
	in_block.len = z_msg_len;
	out_block.addr = out;
	out_block.addr_type = EXT_MEM;
	out_block.len = out_len;
	ret = sm2_generate_digest(device, &sx_ecc_sm2_curve_p256, &in_block, ALG_SM3, &out_block);

	return ret;
}
