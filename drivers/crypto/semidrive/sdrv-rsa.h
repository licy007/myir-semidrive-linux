/*
 * Copyright (c) 2021 Semidrive Semiconductor.
 * All rights reserved.
 *
 */

#ifndef _SDRV_RSA_H_
#define _SDRV_RSA_H_

#include "crypto.h"
#include <crypto/cryptodev.h>

#define SDCE_MAX_RSA_KEY_SIZE	512

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
struct semidrive_rsa_reqctx {
	unsigned long flags;
	int src_nents;
	int dst_nents;
	struct scatterlist *src_sg;
	struct scatterlist *dst_sg;
	unsigned int cryptlen;
};

struct semidrive_rsa_ctx {
	u8 __attribute__((__aligned__(CACHE_LINE))) n[SDCE_MAX_RSA_KEY_SIZE];
	u8 __attribute__((__aligned__(CACHE_LINE))) e[SDCE_MAX_RSA_KEY_SIZE];
	u8 __attribute__((__aligned__(CACHE_LINE))) d[SDCE_MAX_RSA_KEY_SIZE];
	u8 *p;
	u8 *q;
	u8 *dp;
	u8 *dq;
	u8 *qinv;
	unsigned int key_sz;
	bool crt_mode;
} __packed __aligned(64);

static inline struct semidrive_ce_alg_template *to_rsa_tmpl(struct crypto_akcipher *tfm)
{
	struct crypto_alg *alg = tfm->base.__crt_alg;
	return container_of(alg, struct semidrive_ce_alg_template, alg.crypto);
}

extern const struct semidrive_alg_ops akcipher_ops;

int __semidrive_rsa_op(struct crypto_dev *crypto,
					   struct crypto_akcipher *tfm, struct crypt_asym *asym_data, struct semidrive_rsa_ctx *ctx);

#endif /* _SDRV_RSA_H_ */

