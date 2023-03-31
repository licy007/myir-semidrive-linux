/*
 * Copyright (c) 2019 Semidrive Semiconductor.
 * All rights reserved.
 *
 */

#ifndef _SDRV_HASH_H
#define _SDRV_HASH_H

#include <crypto/scatterwalk.h>
#include <crypto/sha.h>

#include "ce.h"
#include "sx_hash.h"
#include "crypto.h"

#define SD_SHA_MAX_BLOCKSIZE        SHA512_BLOCKSIZE
#define SD_SHA_MAX_DIGESTSIZE       SHA512_DIGESTSIZE
#define MD5_DIGEST_SIZE         MD5_DIGESTSIZE
#define MD5_BLOCK_SIZE          MD5_BLOCKSIZE

struct semidrive_sha_ctx {
	u8 authkey[SD_SHA_MAX_BLOCKSIZE];
};

/**
 * struct semidrive_sha_reqctx - holds private ahash objects per request
 * @buf: used during update, import and export
 * @tmpbuf: buffer for internal use
 * @digest: calculated digest buffer
 * @buflen: length of the buffer
 * @flags: operation flags
 * @src_orig: original request sg list
 * @nbytes_orig: original request number of bytes
 * @src_nents: source number of entries
 * @byte_count: byte count
 * @count: save count in states during update, import and export
 * @first_blk: is it the first block
 * @last_blk: is it the last block
 * @sg: used to chain sg lists
 * @authkey: pointer to auth key in sha ctx
 * @authklen: auth key length
 * @result_sg: scatterlist used for result buffer
 */
struct semidrive_sha_reqctx {
	u8 buf[SD_SHA_MAX_BLOCKSIZE];
	u8 __attribute__((aligned(CACHE_LINE))) tmpbuf[SD_SHA_MAX_BLOCKSIZE];
	u8 __attribute__((aligned(CACHE_LINE))) digest[SD_SHA_MAX_DIGESTSIZE];
	u8 __attribute__((aligned(CACHE_LINE))) state[SD_SHA_MAX_DIGESTSIZE];
	unsigned int buflen;
	unsigned long flags;
	struct scatterlist *src_orig;
	unsigned int nbytes_orig;
	int src_nents;
	__be32 byte_count[2];
	u64 count;
	bool first_blk;
	bool last_blk;
	struct scatterlist sg[2];
	u8 *authkey;
	unsigned int authklen;
	struct scatterlist result_sg;
};

static inline struct semidrive_ce_alg_template *to_ahash_tmpl(
	struct crypto_tfm *tfm)
{
	struct crypto_ahash *ahash = __crypto_ahash_cast(tfm);
	struct ahash_alg *alg = container_of(crypto_hash_alg_common(ahash),
										 struct ahash_alg, halg);

	return container_of(alg, struct semidrive_ce_alg_template, alg.ahash);
}

extern const struct semidrive_alg_ops ahash_ops;

#endif /* _SDRV_HASH_H */
