/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <string.h>

#include "sx_pke_conf.h"
#include "sx_hash.h"
#include "sx_trng.h"
#include "sx_errors.h"
#include "sx_dma.h"

#include "sx_dsa.h"
#include "sx_hash.h"
#include "sx_ecc_keygen.h"

#define LOCAL_TRACE 1 //close local trace 1->0

#if !WITH_SIMULATION_PLATFORM //rng not work on paladium
#else
uint8_t  rnd_1024[20] =
	"\x9e\x4d\x0c\x96\xb2\x51\xe3\x60\x56\x89\xd6\x99\xa5\xd3\xde\x4d\xfc\x43\xc0\x18";
uint8_t  rnd_2048_256[32] =
	"\x5a\x4b\x87\x57\xe6\x39\xb1\x22\xea\x61\x77\x20\x58\x54\xee\x4d\x7e\xd9\x73\x1b\xc9\x8a\xf0\x3e\x76\xc1\xa0\x9d\xf1\x0e\xb8\xa4";
uint8_t rnd_2048_224[28] =
	"\x3b\x0f\xf8\xa6\xeb\xf2\x92\x65\xe1\x28\x7b\xef\x21\x61\xea\x71\xc3\x8f\xff\xf9\xb6\x80\xd9\x85\x72\x6c\xe4\x0d";
#endif

uint32_t dsa_generate_private_key(void *device,
								  block_t *n,
								  block_t *priv)
{
	return rng_get_rand_lt_n_blk(device, n, priv);
}

uint32_t dsa_generate_public_key(
	void *device,
	block_t *p,
	block_t *generator,
	block_t *priv_key,
	block_t *pub_key)
{
	uint32_t size_adapt = p->len;

#if PK_CM_ENABLED

	if (PK_CM_RANDPROJ_MOD) {
		size_adapt += PK_CM_RAND_SIZE;
	}

#endif
#if AUTO_OUTPUT_BY_CE

	pke_set_dst_param(device, pub_key->len, 0x1 << BA414EP_MEMLOC_8, (_paddr((void *)pub_key->addr)), pub_key->addr_type);

#endif

	pke_set_command(device, BA414EP_OPTYPE_DSA_KEY_GEN, size_adapt, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

	mem2CryptoRAM_rev(device, p, size_adapt, BA414EP_MEMLOC_0, true);
	mem2CryptoRAM_rev(device, generator, size_adapt, BA414EP_MEMLOC_3, true);
	mem2CryptoRAM_rev(device, priv_key, size_adapt, BA414EP_MEMLOC_6, true);

	uint32_t error = pke_start_wait_status(device);

	if (error) {
		LTRACEF("dsa_generate_public_key result: %d\n", error);
		return CRYPTOLIB_CRYPTO_ERR;
	}

#if AUTO_OUTPUT_BY_CE

#else
	CryptoRAM2mem_rev(device, pub_key, pub_key->len, BA414EP_MEMLOC_8, true);
#endif

	return CRYPTOLIB_SUCCESS;
}

uint32_t dsa_generate_signature(
	void *device,
	hash_alg_t hash_alg,
	block_t *p,
	block_t *q,
	block_t *generator,
	block_t *priv,
	block_t *message,
	block_t *signature)
{
	uint32_t error = 0;
	uint8_t *k_buff;
	struct mem_node *mem_n;
	uint8_t *hash_buff;
	struct mem_node *mem_n_1;
	block_t k_blk;
	/* FIPS 186-3 4.6: leftmost bits of hash if q.len is smaller then hash len */
	uint32_t hash_len;
	block_t hash_block;
	uint32_t size_adapt;
	uint8_t *rnd_const;
	uint32_t rnd_len;

	mem_n = ce_malloc(DSA_MAX_SIZE_P);

	if (mem_n != NULL) {
		k_buff = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	mem_n_1 = ce_malloc(DSA_MAX_SIZE_P);

	if (mem_n_1 != NULL) {
		hash_buff = mem_n_1->ptr;
	}
	else {
		ce_free(mem_n);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	k_blk = BLOCK_T_CONV(k_buff, q->len, EXT_MEM);
	/* FIPS 186-3 4.6: leftmost bits of hash if q.len is smaller then hash len */
	hash_len = q->len < hash_get_digest_size(hash_alg) ? q->len : hash_get_digest_size(hash_alg);
	hash_block = BLOCK_T_CONV(hash_buff, hash_len, EXT_MEM);

	size_adapt = p->len;

#if PK_CM_ENABLED

	if (PK_CM_RANDPROJ_MOD) {
		size_adapt += PK_CM_RAND_SIZE;
	}

#endif

	switch (size_adapt) {
		case 128:
			rnd_len = 20;
			break;

		case 256:
			if (!memcmp(message->addr + (size_adapt  - 1), "\xf4", 1)) {
				rnd_len = 28;
			}
			else {
				rnd_len = 32;
			}

			break;

		default:
			ce_free(mem_n);
			ce_free(mem_n_1);
			return -1;
	}

#if !WITH_SIMULATION_PLATFORM //rng not work on paladium
	k_blk = BLOCK_T_CONV(k_buff + (size_adapt - rnd_len), rnd_len, EXT_MEM);
	error = trng_get_rand_blk(device, k_blk);

	if (error) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		return error;
	}

	k_blk = BLOCK_T_CONV(k_buff, q.len, EXT_MEM);

#else
	/* just workaround for rng not work!!!!!!!*/

	switch (rnd_len) {
		case 20:
			rnd_const = rnd_1024;
			break;

		case 28:
			rnd_const = rnd_2048_224;
			break;

		case 32:
			rnd_const = rnd_2048_256;
			break;

		default:
			ce_free(mem_n);
			ce_free(mem_n_1);
			return -1;
	}

	memcpy(k_buff + (size_adapt - rnd_len), rnd_const, rnd_len);
	/* workaround end!!!!! */
#endif

	error = hash_blk(hash_alg, device, BLOCK_T_CONV(NULL, 0, 0), message, hash_block);

	if (error) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		LTRACEF("dsa_generate_signature hash_blk: %d\n", error);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	if (hash_block.len < size_adapt) {
		memcpy(hash_buff + (size_adapt - hash_block.len), hash_buff, hash_block.len);
		memset(hash_buff, 0, size_adapt - hash_block.len);
		hash_block.len = size_adapt;
	}

	memset(k_buff, 0, size_adapt - rnd_len);

	// Fetch the results by PKE config 0x1 << BA414EP_MEMLOC_10 | 0x1 << BA414EP_MEMLOC_11
#if AUTO_OUTPUT_BY_CE
	pke_set_dst_param(device, size_adapt * 2, 0x1 << BA414EP_MEMLOC_10 | 0x1 << BA414EP_MEMLOC_11, (_paddr((void *)signature.addr)), signature.addr_type);
#endif
	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_DSA_SIGN_GEN, size_adapt, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

	mem2CryptoRAM_rev(device, p, size_adapt, BA414EP_MEMLOC_0, true);
	mem2CryptoRAM_rev(device, q, size_adapt, BA414EP_MEMLOC_2, true);
	mem2CryptoRAM_rev(device, generator, size_adapt, BA414EP_MEMLOC_3, true);
	mem2CryptoRAM_rev(device, &k_blk, size_adapt, BA414EP_MEMLOC_5, true);
	mem2CryptoRAM_rev(device, priv, size_adapt, BA414EP_MEMLOC_6, true);
	mem2CryptoRAM_rev(device, &hash_block, size_adapt, BA414EP_MEMLOC_12, true);

	error = pke_start_wait_status(device);

	if (error) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		LTRACEF("dsa_generate_signature pke_start_wait_status: %d\n", error);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	// Result signature
#if AUTO_OUTPUT_BY_CE
#else
	CryptoRAM2point_rev(device, signature, size_adapt, BA414EP_MEMLOC_10);
#endif
	ce_free(mem_n);
	ce_free(mem_n_1);
	return CRYPTOLIB_SUCCESS;
}

uint32_t dsa_verify_signature(
	void *device,
	hash_alg_t hash_alg,
	block_t *p,
	block_t *q,
	block_t *generator,
	block_t *pub,
	block_t *message,
	block_t *signature)
{
	uint32_t error = 0;
	uint8_t *hash_buff;
	struct mem_node *mem_n;

	/* FIPS 186-3 4.7: leftmost bits of hash if q.len is smaller then hash len */
	uint32_t hash_len;
	block_t hash_block;
	uint32_t size;

	mem_n = ce_malloc(DSA_MAX_SIZE_P);

	if (mem_n != NULL) {
		hash_buff = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	hash_len = q->len < hash_get_digest_size(hash_alg) ? q->len : hash_get_digest_size(hash_alg);
	hash_block = BLOCK_T_CONV(hash_buff, hash_len, EXT_MEM);

	size = p->len;

	error = hash_blk(hash_alg, device, BLOCK_T_CONV(NULL, 0, 0), message, hash_block);

	if (error) {
		ce_free(mem_n);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	if (hash_block.len < size) {
		memcpy(hash_buff + (size - hash_block.len), hash_buff, hash_block.len);
		memset(hash_buff, 0, size - hash_block.len);
		hash_block.len = size;
	}

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_DSA_SIGN_VERIF, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

	mem2CryptoRAM_rev(device, p, size, BA414EP_MEMLOC_0, true);
	mem2CryptoRAM_rev(device, q, size, BA414EP_MEMLOC_2, true);
	mem2CryptoRAM_rev(device, generator, size, BA414EP_MEMLOC_3, true);
	mem2CryptoRAM_rev(device, pub, size, BA414EP_MEMLOC_8, true);

	point2CryptoRAM_rev(device, signature, size, BA414EP_MEMLOC_10);
	mem2CryptoRAM_rev(device, &hash_block, size, BA414EP_MEMLOC_12, true);

	error = pke_start_wait_status(device);

	if (error & CE_PK_SIGNATURE_NOTVALID_MASK) {
		ce_free(mem_n);
		return CRYPTOLIB_INVALID_SIGN_ERR;
	}
	else if (error) {
		ce_free(mem_n);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	ce_free(mem_n);
	return CRYPTOLIB_SUCCESS;
}
