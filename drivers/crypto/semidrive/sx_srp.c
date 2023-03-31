/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <string.h>
#include <malloc.h>

#include <sx_pke_conf.h>
#include <sx_hash.h>
#include <sx_trng.h>
#include <sx_errors.h>
#include <sx_dma.h>
#include <sx_srp.h>

#include <trace.h>

#define LOCAL_TRACE 0 //close local trace 1->0

#if PK_CM_ENABLED && PK_CM_RANDPROJ_MOD
#define SIZE_ADAPTATION (PK_CM_RAND_SIZE)
#else
#define SIZE_ADAPTATION (0)
#endif

/**
 * @brief Generate \p out = hash( \p in )
 * @param  vce_id    ce index
 * @param out output digest buffer
 * @param in message to hash
 * @param hash_alg function to use
 * @return length to \p out
*/
static uint32_t genHash(uint32_t vce_id,
						block_t out,
						block_t in,
						hash_alg_t hash_alg)
{
	uint32_t status = hash_blk(hash_alg, vce_id, BLOCK_T_CONV(NULL, 0, EXT_MEM), in, out);

	if (status) {
		return status;
	}

	return hash_get_digest_size(hash_alg);
}

/**
 * @brief Generate \p out = hash( \p in1 | \p in2 )
 * @param  vce_id    ce index
 * @param out output digest buffer
 * @param in1 first part of message to hash
 * @param in2 second part message to hash
 * @param hash_alg function to use
 * @return length to \p out
*/
static uint32_t genHash2(uint32_t vce_id,
						 block_t out,
						 block_t in1,
						 block_t in2,
						 hash_alg_t hash_alg)
{
	uint8_t *hash_buf = NULL;
	uint32_t status;

	hash_buf = malloc(in1.len + in2.len);

	if (NULL == hash_buf) {
		LTRACEF("memory alloc fail.\n");
		return -1;
	}

	memcpy(hash_buf, in1.addr, in1.len);
	memcpy(hash_buf + in1.len, in2.addr, in2.len);

	status = hash_blk(hash_alg, vce_id, BLOCK_T_CONV(NULL, 0, EXT_MEM), BLOCK_T_CONV(hash_buf, (in1.len + in2.len), EXT_MEM), out);
	free(hash_buf);

	if (status) {
		LTRACEF("hash fail in srp\n");
	}

	return status;
}

/** Compute the SRP k multiplier and copy it into crypto mem. */
static uint32_t srp_push_k_multiplier(uint32_t vce_id,
									  hash_alg_t hash_alg,
									  block_t N,
									  block_t g,
									  uint32_t size,
									  uint32_t SRP6a,
									  uint32_t cryptomem_idx)
{
	uint8_t hashbuf[MAX_DIGESTSIZE] = {0};
	uint32_t status = 0;
	block_t k = BLOCK_T_CONV(hashbuf, MAX_DIGESTSIZE, EXT_MEM);

	if (SRP6a) {
		k.len = hash_get_digest_size(hash_alg);

		/* hash( N | g) */
		status = genHash2(vce_id, k, N, g, hash_alg);

		if (status) {
			LTRACEF("genHash2 fail.\n");
			return status;
		}
	}
	else {
		k.len = 4;
		memset(hashbuf, 0, 4);
		hashbuf[3] = 3;
	}

	/* copy into crypto RAM */
	mem2CryptoRAM_rev(vce_id, k, (size > k.len ? k.len : size) + SIZE_ADAPTATION, cryptomem_idx, true);

	return status;
}

/** Compute the SRP u factor and copy it into crypto mem. */
static uint32_t srp_push_u_factor(uint32_t vce_id,
								  hash_alg_t hash_alg,
								  block_t A,
								  block_t B,
								  uint32_t size,
								  uint32_t cryptomem_idx)
{
	uint8_t hashbuf[MAX_DIGESTSIZE] = {0};
	uint32_t hash_size = hash_get_digest_size(hash_alg);
	block_t u = BLOCK_T_CONV(hashbuf, hash_size, EXT_MEM);

	/* hash(A | B) */
	uint32_t status = genHash2(vce_id, u, A, B, hash_alg);

	if (status) {
		LTRACEF("genHash2 fail.\n");
		return status;
	}

	/* copy into crypto RAM     */
	mem2CryptoRAM_rev(vce_id, u, (size > hash_size ? hash_size : size) + SIZE_ADAPTATION, cryptomem_idx, true);

	return status;
}

/** Derive the SRP x user secret and copy it into crypto mem. */
static uint32_t srp_push_x_secret(uint32_t vce_id,
								  hash_alg_t hash_alg,
								  block_t usrpwd,
								  block_t s,
								  uint32_t size,
								  uint32_t cryptomem_idx)
{
	uint8_t hashbuf[MAX_DIGESTSIZE] = {0};
	uint32_t hash_size = hash_get_digest_size(hash_alg);
	uint32_t status;
	block_t x = BLOCK_T_CONV(hashbuf, hash_size, EXT_MEM);

	/* intermediate x = hash( username | ":" | p ) */
	status = hash_blk(hash_alg, vce_id, BLOCK_T_CONV(NULL, 0, EXT_MEM), usrpwd, x);

	if (status) {
		LTRACEF("hash in srp fail\n");
		return status;
	}

	/* x = hash(s | x) */
	status = genHash2(vce_id, x, s, x, hash_alg);

	if (status) {
		LTRACEF("genHash2 fail.\n");
		return status;
	}

	/* copy into crypto RAM */
	mem2CryptoRAM_rev(vce_id, x, (size > hash_size ? hash_size : size) + SIZE_ADAPTATION, cryptomem_idx, true);
	return status;
}

uint32_t srp_host_gen_pub(uint32_t vce_id,
						  hash_alg_t hash_alg,
						  block_t N,
						  block_t g,
						  block_t v,
						  block_t B,
						  block_t b,
						  uint32_t SRP6a,
						  uint32_t size)
{
	uint32_t status;
	uint32_t size_adapt = size + SIZE_ADAPTATION;
	uint8_t buf[SRP_MAX_KEY_SIZE] = {0};
	block_t tmp = BLOCK_T_CONV(buf, size, EXT_MEM);

	if (size > SRP_MAX_KEY_SIZE) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	// Set byte_swap
	pke_set_command(vce_id, BA414EP_OPTYPE_SRP_SERVER_PK, size_adapt, BA414EP_LITTLEEND,
					BA414EP_SELCUR_NO_ACCELERATOR);

	N.len = size;
	g.len = size;

	// Location 0 -> N
	mem2CryptoRAM_rev(vce_id, N, size_adapt, BA414EP_MEMLOC_0, true);

	// Location 3 -> g
	mem2CryptoRAM_rev(vce_id, g, size_adapt, BA414EP_MEMLOC_3, true);

	// Location 10 -> v
	mem2CryptoRAM_rev(vce_id, v, size_adapt, BA414EP_MEMLOC_10, true);

	// Location 7:  k mulitplier
	status = srp_push_k_multiplier(vce_id, hash_alg, N, g, size, SRP6a, BA414EP_MEMLOC_7);

	if (status) {
		LTRACEF("srp_push_k_multiplier fail.\n");
		return status;
	}

	//Generates priv key
#if 0 //trng not work, just workaround use IP pattern parameters
	status = sx_rng_get_rand_lt_n_blk(tmp, N, rng);

	if (status) {
		return status;
	}

#else
	memcpy(buf,  "\x0F\xF1\x8E\x02\x42\xAF\x9F\xC3\x85\x77\x6E\x9A\xDD\x84\xF3\x9E\x71\x54\x5A\x13\x7A\x1D\x50\x06\x8D\x72\x31\x04\xF7\x73\x83\xC1"\
		   "\x34\x58\xA7\x48\xE9\xBB\x17\xBC\xA3\xF2\xC9\xBF\x9C\x63\x16\xB9\x50\xF2\x44\x55\x6F\x25\xE2\xA2\x5A\x92\x11\x87\x19\xC7\x8D\xF4"\
		   "\x8F\x4F\xF3\x1E\x78\xDE\x58\x57\x54\x87\xCE\x1E\xAF\x19\x92\x2A\xD9\xB8\xA7\x14\xE6\x1A\x44\x1C\x12\xE0\xC8\xB2\xBA\xD6\x40\xFB"\
		   "\x19\x48\x8D\xEC\x4F\x65\xD4\xD9\x25\x9F\x43\x29\xE6\xF4\x59\x0B\x9A\x16\x41\x06\xCF\x6A\x65\x9E\xB4\x86\x2B\x21\xFB\x97\xD4\x35", 128);
#endif
	// Location 12 -> k
	mem2CryptoRAM_rev(vce_id, tmp, size_adapt, BA414EP_MEMLOC_12, true);

	//Output of the pub key
	memcpy_blk(vce_id, b, tmp, size); //To the output FIFO

	/* Start BA414EP */
	status = pke_start_wait_status(vce_id);

	if (status) {
		return status;
	}

	// Output of the pub key
	CryptoRAM2mem_rev(vce_id, B, size, BA414EP_MEMLOC_5, true);
	return CRYPTOLIB_SUCCESS;
}

uint32_t srp_host_gen_key(uint32_t vce_id,
						  hash_alg_t hash_alg,
						  block_t N,
						  block_t A,
						  block_t v,
						  block_t b,
						  block_t B,
						  block_t K,
						  uint32_t size)
{
	uint32_t status;
	uint32_t size_adapt = size + SIZE_ADAPTATION;

	// Set byte_swap
	pke_set_command(vce_id, BA414EP_OPTYPE_SRP_SERVER_KEY, size_adapt, BA414EP_LITTLEEND,
					BA414EP_SELCUR_NO_ACCELERATOR);

	//From parameters
	N.len = size;

	// Location 10 -> v
	mem2CryptoRAM_rev(vce_id, v, size_adapt, BA414EP_MEMLOC_10, true);
	// Location 12-> b
	mem2CryptoRAM_rev(vce_id, b, size_adapt, BA414EP_MEMLOC_12, true);
	// Location 0 -> N
	mem2CryptoRAM_rev(vce_id, N, size_adapt, BA414EP_MEMLOC_0, true);
	// Location 2-> A
	mem2CryptoRAM_rev(vce_id, A, size_adapt, BA414EP_MEMLOC_2, true);

	//Generates u
	status = srp_push_u_factor(vce_id, hash_alg, A, B, size, BA414EP_MEMLOC_8);

	if (status) {
		LTRACEF("srp_push_u_factor fail.\n");
		return status;
	}

	/* Start BA414EP */
	status = pke_start_wait_status(vce_id);

	if (status) {
		return status;
	}

	// Output of the pub key
	CryptoRAM2mem_rev(vce_id, K, K.len, BA414EP_MEMLOC_11, true);

	return CRYPTOLIB_SUCCESS;
}


uint32_t srp_user_gen_key(uint32_t vce_id,
						  hash_alg_t hash_alg,
						  block_t N,
						  block_t g,
						  block_t A,
						  block_t B,
						  block_t usrpwd,
						  block_t s,
						  block_t a,
						  block_t K,
						  uint32_t SRP6a,
						  uint32_t size)
{
	uint32_t status;
	uint32_t size_adapt = size + SIZE_ADAPTATION;

	// Set byte_swap
	pke_set_command(vce_id, BA414EP_OPTYPE_SRP_CLIENT_KEY, size_adapt, BA414EP_LITTLEEND,
					BA414EP_SELCUR_NO_ACCELERATOR);

	g.len = size;
	a.len = size;
	N.len = size;

	// Location 0 -> N
	mem2CryptoRAM_rev(vce_id, N, size_adapt, BA414EP_MEMLOC_0, true);
	// Location 3 -> g
	mem2CryptoRAM_rev(vce_id, g, size_adapt, BA414EP_MEMLOC_3, true);
	// Location 5 -> B
	mem2CryptoRAM_rev(vce_id, B, size_adapt, BA414EP_MEMLOC_5, true);

	// Location 6: Generate hash x
	status = srp_push_x_secret(vce_id, hash_alg, usrpwd, s, size, BA414EP_MEMLOC_6);

	if (status) {
		LTRACEF("srp_push_x_secret fail.\n");
		return status;
	}

	// Location 4 -> a (From the parameters but need to keep the order of inputs)
	mem2CryptoRAM_rev(vce_id, a, size_adapt, BA414EP_MEMLOC_4, true);

	//Generates hash k
	status = srp_push_k_multiplier(vce_id, hash_alg, N, g, size, SRP6a, BA414EP_MEMLOC_7);

	if (status) {
		LTRACEF("srp_push_k_multiplier fail.\n");
		return status;
	}

	// Location 8: Generate hash u
	status = srp_push_u_factor(vce_id, hash_alg, A, B, size, BA414EP_MEMLOC_8);

	if (status) {
		LTRACEF("srp_push_u_factor fail.\n");
		return status;
	}

	/* Start BA414EP */
	status = pke_start_wait_status(vce_id);

	if (status) {
		return status;
	}

	// Output of the pub key
	CryptoRAM2mem_rev(vce_id, K, K.len, BA414EP_MEMLOC_11, true);

	return CRYPTOLIB_SUCCESS;
}
