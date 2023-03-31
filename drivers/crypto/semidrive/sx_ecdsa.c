/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/kernel.h>

#include "sx_pke_conf.h"
#include "sx_hash.h"
#include "sx_trng.h"
#include "sx_errors.h"
#include "sx_ecdsa.h"
#include "sx_dma.h"


#if !WITH_SIMULATION_PLATFORM //rng not work on paladium

#else
/* just workaround for rng not work!!!!!!!*/
uint8_t  ecdsa_rnd_p256[32] =
	"\x51\x9b\x42\x3d\x71\x5f\x8b\x58\x1f\x4f\xa8\xee\x59\xf4\x77\x1a"
	"\x5b\x44\xc8\x13\x0b\x4e\x3e\xac\xca\x54\xa5\x6d\xda\x72\xb4\x64";
uint8_t  ecdsa_rnd_p192[24] =
	"\xa5\xc8\x17\xa2\x36\xa5\xf7\xfa\xa3\x29\xb8\xec\xc3\xc5\x96\x68\x7c\x71\xaa\xaf\x86\xc7\x70\x3d";
uint8_t ecdsa_rnd_p384[48] =
	"\x2e\x44\xef\x1f\x8c\x0b\xea\x83\x94\xe3\xdd\xa8\x1e\xc6\xa7\x84\x2a\x45\x9b\x53\x47\x01\x74\x9e"
	"\x2e\xd9\x5f\x05\x4f\x01\x37\x68\x08\x78\xe0\x74\x9f\xc4\x3f\x85\xed\xca\xe0\x6c\xc2\xf4\x3f\xef";
uint8_t ecdsa_rnd_p521[66] =
	"\x01\xfc\x81\xab\xec\x3f\xab\x91\xa4\x39\xdf\x81\xeb\x49\xa7\x17"
	"\x92\x30\x86\x6c\x2e\x0e\x50\x5d\x69\x6c\xa3\x79\x72\xe4\xaf\x04"
	"\x3f\x6c\xe6\xed\xea\x69\xc5\x71\x1d\x14\xc4\x39\x05\xb3\xdc\xc9"
	"\xed\x48\xd2\x25\xd5\x0d\x8b\x13\x6b\x6f\x07\x87\x21\x5b\x35\x00"
	"\xbe\x52";
#endif

uint32_t ecdsa_validate_domain(void *device, const sx_ecc_curve_t *curve)
{
	uint32_t size;

	size = ecc_curve_bytesize(curve);

	pke_set_command(device, BA414EP_OPTYPE_ECDSA_PARAM_EVAL, size, BA414EP_LITTLEEND, curve->pk_flags);
	pke_load_curve(device, &curve->params, size, BA414EP_LITTLEEND, 1);

	return pke_start_wait_status(device);
}

uint32_t ecdsa_configure_signature(void *device,
								   const sx_ecc_curve_t *curve,
								   block_t *formatted_digest,
								   block_t *key)
{
	uint32_t error = CRYPTOLIB_SUCCESS;
	uint32_t size = ecc_curve_bytesize(curve);

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_ECDSA_SIGN_GEN, size, BA414EP_LITTLEEND, curve->pk_flags);

	// Load parameters
	pke_load_curve(device, &curve->params, size, BA414EP_LITTLEEND, 1);

	/* Load ECDSA parameters */
	error = mem2CryptoRAM_rev(device, key, size, BA414EP_MEMLOC_6, true);

	if (error) {
		pr_err("key addr: 0x%lx\n", (addr_t)key->addr);
		return error;
	}

	error = mem2CryptoRAM_rev(device, formatted_digest, formatted_digest->len, BA414EP_MEMLOC_12, true);

	if (error) {
		pr_err("formatted_digest addr: 0x%lx\n", (addr_t)formatted_digest->addr);
		return error;
	}

	return error;
}

uint32_t ecdsa_attempt_signature(void *device,
								 const sx_ecc_curve_t *curve,
								 block_t *signature)
{
	//void * sign_temp = NULL;
	uint32_t status;
	uint32_t size;
	block_t rnd;
	uint8_t *rnd_buff = NULL;
	struct mem_node *mem_n = NULL;
#if WITH_SIMULATION_PLATFORM
	uint8_t *rnd_const;
#endif

	mem_n = ce_malloc(ECC_MAX_KEY_SIZE);

	if (mem_n != NULL) {
		rnd_buff = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	size = ecc_curve_bytesize(curve);
	rnd = BLOCK_T_CONV(rnd_buff, size, EXT_MEM);

#if !WITH_SIMULATION_PLATFORM //rng not work on paladium
	// status = trng_get_rand_blk(device, rnd);
	status = trng_get_rand_by_fifo(device, rnd.addr, rnd.len);

	if (status) {
		pr_emerg("trng_get_rand_by_fifo is wrong ");
		ce_free(mem_n);
		return status;
	}

#else
	/* just workaround for rng not work!!!!!!!*/

	switch (size) {
		case 24:
			rnd_const = ecdsa_rnd_p192;
			break;

		case 32:
			rnd_const = ecdsa_rnd_p256;
			break;

		case 48:
			rnd_const = ecdsa_rnd_p384;
			break;

		case 66:
			rnd_const = ecdsa_rnd_p521;
			break;

		default:
			rnd_const = ecdsa_rnd_p521;
			break;
	}

	memcpy(rnd_buff, rnd_const, size);
	/* workaround end!!!!! */
#endif

	mem2CryptoRAM_rev(device, &rnd, rnd.len, BA414EP_MEMLOC_7, true);

	/* ECDSA signature generation */
	pke_set_command(device,
					BA414EP_OPTYPE_ECDSA_SIGN_GEN,
					size,
					BA414EP_LITTLEEND,
					curve->pk_flags);

	//fetch the results by auto mode
// #if AUTO_OUTPUT_BY_CE
//  pke_set_dst_param(vce_id, size, 0x1 << BA414EP_MEMLOC_10 | 0x1 << BA414EP_MEMLOC_11, (addr_t)(signature.addr), signature.addr_type);
// #endif
	status = pke_start_wait_status(device);

	if (status & CE_PK_SIGNATURE_NOTVALID_MASK) {
		ce_free(mem_n);
		return CRYPTOLIB_INVALID_SIGN_ERR;
	}
	else if (status) {
		ce_free(mem_n);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	// Fetch the results by manual mode
#if AUTO_OUTPUT_BY_CE
#else
	//get r/s value, one value len is size
	CryptoRAM2point_rev(device, signature, size, BA414EP_MEMLOC_10);
#endif
	ce_free(mem_n);
	return CRYPTOLIB_SUCCESS;
}

uint32_t ecdsa_generate_signature_digest(void *device,
		const sx_ecc_curve_t *curve,
		block_t *formatted_digest,
		block_t *key,
		block_t *signature)
{
	uint32_t ctr = 0;
	uint32_t status = ecdsa_configure_signature(device, curve, formatted_digest, key);

	if (status) {
		pr_err("status is %d\n", status);
		return status;
	}

	do {
		status = ecdsa_attempt_signature(device, curve, signature);
		ctr++;
	}
	while ((status == CRYPTOLIB_INVALID_SIGN_ERR) && (ctr < 10));

	if (status) {
		return status;
	}

	return CRYPTOLIB_SUCCESS;
}

uint32_t ecdsa_verify_signature_digest(void *device,
									   const sx_ecc_curve_t *curve,
									   block_t *formatted_digest,
									   block_t *key,
									   block_t *signature)
{
	uint32_t size = ecc_curve_bytesize(curve);

	pke_set_command(device, BA414EP_OPTYPE_ECDSA_SIGN_VERIF, size, BA414EP_LITTLEEND, curve->pk_flags);
	pke_load_curve(device, &curve->params, size, BA414EP_LITTLEEND, 1);

	/* Load ECDSA parameters */
	point2CryptoRAM_rev(device, key, size, BA414EP_MEMLOC_8);
	mem2CryptoRAM_rev(device, formatted_digest, formatted_digest->len, BA414EP_MEMLOC_12, true);

	// Fetch the signature
	point2CryptoRAM_rev(device, signature, size, BA414EP_MEMLOC_10);

	/* ECDSA signature verification */
	return pke_start_wait_status(device);
}

uint32_t math_array_nbits(uint8_t *a, const size_t length)
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
//TODO Move this to some "includable" module ?
static void bitshift(uint8_t *array, uint8_t len, uint8_t shift)
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
 * @param vce_id    vce index
 * @param curve ECDSA curve to use
 * @param message message to sign or verify
 * @param key private or public key
 * @param signature result output or signature input
 * @param hash_fct algorithm to use for hash
 * @param digest_blk computed digest
 * @return CRYPTOLIB_INVALID_PARAM when there is not enough space in the
 *                                 digest_blk to compute the digest
 *         CRYPTOLIB_SUCCESS otherwise
 */
static uint32_t ecdsa_generate_digest(void *device,
									  const sx_ecc_curve_t *curve,
									  block_t *message,
									  block_t *key,
									  block_t *signature,
									  hash_alg_t hash_fct,
									  block_t *digest_blk)
{
	uint32_t i = 0;
	uint32_t status;
	block_t iv = BLOCK_T_CONV(NULL, 0, 0);
	uint32_t dgst_local_len;
	uint8_t extra_bits;
	uint32_t curve_bytesize = ecc_curve_bytesize(curve);
	uint32_t curve_order_bitsize = math_array_nbits(
									   curve->params.addr + curve_bytesize, curve_bytesize);
	uint32_t curve_order_bytesize = (curve_order_bitsize + 7) / 8;
	uint32_t hash_len = hash_get_digest_size(hash_fct);

	if (digest_blk->len < hash_len) { //if curve_order_bytesize < hash_len, please use more len function
		return CRYPTOLIB_INVALID_PARAM;
	}

	/* Call hash fct to get digest. */
	status = hash_blk(device, hash_fct, &iv, message, digest_blk);

	if (status) {
		return status;
	}

	/* Define digest size. This only take the most significant bytes when curve
	 * is smaller than hash. If it's greater, leading zeroes will be inserted
	 * within ecdsa_signature_* functions.
	 */
	dgst_local_len = curve_order_bytesize > hash_len ? hash_len : curve_order_bytesize;

	/* Shorten the digest to match the expected length */
	digest_blk->len = dgst_local_len;

	/* Bitshift if needed, for curve smaller than digest and with order N not on
	 * bytes boundaries.
	 */
	extra_bits = (curve_order_bitsize & 0x7);

	if (extra_bits && (hash_len * 8 > curve_order_bitsize)) {
		bitshift(digest_blk->addr, dgst_local_len,  8 - extra_bits);
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

uint32_t ecdsa_generate_signature(void *device,
								  const void *curve_info,
								  block_t *message,
								  block_t *key,
								  block_t *signature,
								  hash_alg_t hash_fct)
{
	block_t digest_blk;
	uint32_t status;
	const sx_ecc_curve_t *curve;
	uint8_t *digest;
	struct mem_node *mem_n;

	mem_n = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n != NULL) {
		digest = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	digest_blk = BLOCK_T_CONV(digest, MAX_DIGESTSIZE, EXT_MEM);

	curve = (sx_ecc_curve_t *)curve_info;

	//memset(digest, 0xff, MAX_DIGESTSIZE);

	status = ecdsa_generate_digest(device, curve, message, key, signature, hash_fct, &digest_blk);

	if (status) {
		pr_err("status is %d\n", status);
		ce_free(mem_n);
		return status;
	}

	status = ecdsa_generate_signature_digest(device, curve, &digest_blk, key, signature/*, rng*/);
	ce_free(mem_n);
	return status;
}

uint32_t ecdsa_verify_signature(void *device,
								const void *curve_info,
								block_t *message,
								block_t *key,
								block_t *signature,
								hash_alg_t hash_fct)
{
	block_t digest_blk;
	uint32_t status;

	const sx_ecc_curve_t *curve = (sx_ecc_curve_t *)curve_info;
	uint8_t *digest;
	struct mem_node *mem_n = NULL;

	mem_n = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n != NULL) {
		digest = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	digest_blk = BLOCK_T_CONV(digest, MAX_DIGESTSIZE, EXT_MEM);

	curve = (sx_ecc_curve_t *)curve_info;

	status = ecdsa_generate_digest(device, curve, message, key, signature, hash_fct, &digest_blk);

	if (status) {
		pr_err("ecdsa_generate_digest result: 0x%x\n", status);
		ce_free(mem_n);
		return status;
	}

	status = ecdsa_verify_signature_digest(device, curve, &digest_blk, key, signature);
	ce_free(mem_n);
	return status;
}
