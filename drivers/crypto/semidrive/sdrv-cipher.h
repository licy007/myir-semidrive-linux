/*
 * Copyright (c) 2021 Semidrive Semiconductor.
 * All rights reserved.
 *
 */

#ifndef _SDRV_CIPHER_H_
#define _SDRV_CIPHER_H_

#include "crypto.h"
#include "sx_cipher.h"

#define SDCE_MAX_KEY_SIZE	64

struct semidrive_cipher_ctx {
	u8 __attribute__((__aligned__(CACHE_LINE))) enc_key[SDCE_MAX_KEY_SIZE];
	unsigned int enc_keylen;
	struct crypto_skcipher *fallback;
	enum aes_ctx_t aes_ctx;
};

/**
 * struct sdce_cipher_reqctx - holds private cipher objects per request
 * @flags: operation flags
 * @iv: pointer to the IV
 * @ivsize: IV size
 * @src_nents: source entries
 * @dst_nents: destination entries
 * @result_sg: scatterlist used for result buffer
 * @dst_tbl: destination sg table
 * @dst_sg: destination sg pointer table beginning
 * @src_tbl: source sg table
 * @src_sg: source sg pointer table beginning;
 * @cryptlen: crypto length
 */
struct semidrive_cipher_reqctx {
	unsigned long flags;
	u8 __attribute__((__aligned__(CACHE_LINE))) iv[SDCE_MAX_KEY_SIZE];
	unsigned int ivsize;
	int src_nents;
	int dst_nents;
	struct scatterlist result_sg;
	struct scatterlist *dst_sg;
	struct scatterlist *src_sg;
	unsigned int cryptlen;
};

static inline struct semidrive_ce_alg_template *to_cipher_tmpl(struct crypto_tfm *tfm)
{
	struct crypto_alg *alg = tfm->__crt_alg;
	return container_of(alg, struct semidrive_ce_alg_template, alg.crypto);
}

extern const struct semidrive_alg_ops ablkcipher_ops;

#endif /* _SDRV_CIPHER_H_ */

