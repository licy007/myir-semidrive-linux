/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _CRYPTO_H
#define _CRYPTO_H
#include <crypto/hash.h>
#include <crypto/akcipher.h>
#include "ce.h"

/* cipher algorithms */
#define SD_ALG_DES              BIT(0)
#define SD_ALG_3DES             BIT(1)
#define SD_ALG_AES              BIT(2)

/* hash and hmac algorithms */
#define SD_HASH_MD5             BIT(3)
#define SD_HASH_SHA1            BIT(4)
#define SD_HASH_SHA224          BIT(5)
#define SD_HASH_SHA256          BIT(6)
#define SD_HASH_SHA384          BIT(7)
#define SD_HASH_SHA512          BIT(8)
#define SD_HASH_SM3             BIT(9)
#define SD_HASH_SHA1_HMAC       BIT(10)
#define SD_HASH_SHA224_HMAC     BIT(11)
#define SD_HASH_SHA256_HMAC     BIT(12)
#define SD_HASH_SHA384_HMAC     BIT(13)
#define SD_HASH_SHA512_HMAC     BIT(14)
#define SD_HASH_AES_CMAC        BIT(15)

/* cipher modes */
#define SD_MODE_CBC             BIT(16)
#define SD_MODE_ECB             BIT(17)
#define SD_MODE_CTR             BIT(18)
#define SD_MODE_XTS             BIT(19)
#define SD_MODE_CCM             BIT(20)
#define SD_MODE_GCM             BIT(21)
#define SD_MODE_CFB             BIT(22)
#define SD_MODE_OFB             BIT(23)
#define SD_MODE_CMAC            BIT(24)
#define SD_MODE_MASK            GENMASK(12, 8)

#define IS_MD5(flags)           (flags & SD_HASH_MD5)
#define IS_SHA1(flags)          (flags & SD_HASH_SHA1)
#define IS_SHA224(flags)        (flags & SD_HASH_SHA224)
#define IS_SHA256(flags)        (flags & SD_HASH_SHA256)
#define IS_SHA384(flags)        (flags & SD_HASH_SHA384)
#define IS_SHA512(flags)        (flags & SD_HASH_SHA512)
#define IS_SM3(flags)           (flags & SD_HASH_SM3)
#define IS_SHA1_HMAC(flags)     (flags & SD_HASH_SHA1_HMAC)
#define IS_SHA224_HMAC(flags)   (flags & SD_HASH_SHA224_HMAC)
#define IS_SHA256_HMAC(flags)   (flags & SD_HASH_SHA256_HMAC)
#define IS_SHA384_HMAC(flags)   (flags & SD_HASH_SHA384_HMAC)
#define IS_SHA512_HMAC(flags)   (flags & SD_HASH_SHA512_HMAC)
#define IS_SHA_HMAC(flags)      \
        (IS_SHA1_HMAC(flags) || IS_SHA256_HMAC(flags) \
        || IS_SHA224_HMAC(flags) || IS_SHA512_HMAC(flags) \
        || IS_SHA384_HMAC(flags))
#define IS_CMAC(flags)          (flags & SD_HASH_AES_CMAC)
#define IS_AES(flags)           (flags & SD_ALG_AES)

/* cipher encryption/decryption operations */
#define SD_ENCRYPT              BIT(30)
#define SD_DECRYPT              BIT(31)

struct semidrive_ce_device {
	u32 ce_id;
	int irq;
	void __iomem *base;
	void __iomem *sram_base;
};

struct semidrive_ce_alg_template {
	struct list_head entry;
	u32 crypto_alg_type;
	unsigned long alg_flags;
	const u32 *std_iv;
	union {
		struct crypto_alg crypto;
		struct ahash_alg ahash;
		struct akcipher_alg rsa;
	} alg;
	struct crypto_dev *sdce;
};

/**
 * struct qce_algo_ops - algorithm operations per crypto type
 * @type: should be CRYPTO_ALG_TYPE_XXX
 * @register_algs: invoked by core to register the algorithms
 * @unregister_algs: invoked by core to unregister the algorithms
 * @async_req_handle: invoked by core to handle enqueued request
 */
struct semidrive_alg_ops {
	u32 type;
	int (*register_algs)(struct crypto_dev *sdce);
	void (*unregister_algs)(struct crypto_dev *sdce);
	int (*async_req_handle)(struct crypto_async_request *async_req);
};

extern const struct semidrive_alg_ops ahash_ops;
extern const struct semidrive_alg_ops ablkcipher_ops;
extern const struct semidrive_alg_ops akcipher_ops;

#endif //_CRYPTO_H
