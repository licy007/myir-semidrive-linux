/*
 * Copyright (c) 2019 Semidrive Semiconductor.
 * All rights reserved.
 *
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <crypto/akcipher.h>
#include <crypto/internal/akcipher.h>
#include <crypto/internal/rsa.h>
#include <crypto/cryptodev.h>
#include "sx_rsa.h"
#include "sx_hash.h"

#include "sdrv-rsa.h"

static LIST_HEAD(akcipher_algs);
static struct semidrive_ce_alg_template *tmpl_rsa = NULL;

int __semidrive_rsa_op(struct crypto_dev *crypto,
					   struct crypto_akcipher *tfm, struct crypt_asym *asym_data, struct semidrive_rsa_ctx *ctx)
{
	int ret = 0;
	u32 hw_ret = 0;
	void *input_buff = NULL;
	void *output_buff = NULL;
	u32 input_offset = 0;
	u32 output_offset = 0;
	addr_t phy_addr;
	int sign_len = 0;

	input_buff = kzalloc(asym_data->src_len + CACHE_LINE, GFP_KERNEL);

	if (!input_buff) {
		return -ENOMEM;
	}

	phy_addr = _paddr(input_buff);

	if (phy_addr & (CACHE_LINE - 1)) {
		input_offset = CACHE_LINE
					   - (phy_addr & (CACHE_LINE - 1));
	}

	output_buff = kzalloc(asym_data->dst_len + CACHE_LINE, GFP_KERNEL);

	if (!output_buff) {
		pr_err("alloc fail line : %d\n", __LINE__);
		ret = -ENOMEM;
		goto out1;
	}

	phy_addr = _paddr(output_buff);

	if (phy_addr & (CACHE_LINE - 1)) {
		output_offset = CACHE_LINE
						- (phy_addr & (CACHE_LINE - 1));
	}

	ret = copy_from_user(input_buff + input_offset, asym_data->src, asym_data->src_len);

	if (unlikely(ret)) {
		pr_err("cpy src fail.\n");
		goto out2;
	}

	//rsa operation
	switch (asym_data->asym_op) {
		case ASYM_ENC:
			hw_ret = rsa_encrypt_blk(tmpl_rsa->sdce,
									 asym_data->padding,
									 &BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
									 &BLOCK_T_CONV((u8 *)ctx->n, asym_data->rsa_size, EXT_MEM),
									 &BLOCK_T_CONV((u8 *)ctx->e, asym_data->rsa_size, EXT_MEM),
									 &BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
									 ALG_SHA1, asym_data->be_param);
			break;

		case ASYM_DEC:
			hw_ret = rsa_decrypt_blk(tmpl_rsa->sdce,
									 asym_data->padding,
									 &BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
									 &BLOCK_T_CONV(ctx->n, asym_data->rsa_size, EXT_MEM),
									 &BLOCK_T_CONV(ctx->d, asym_data->rsa_size, EXT_MEM),
									 &BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
									 &sign_len, ALG_SHA1, asym_data->be_param);

			if (!hw_ret) {
				ret = copy_to_user(asym_data->sign_len, &sign_len, sizeof(int));
			}

			break;

		case ASYM_PRIV_ENC:
			hw_ret = rsa_priv_encrypt_blk(tmpl_rsa->sdce,
										  asym_data->padding,
										  &BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
										  &BLOCK_T_CONV((u8 *)ctx->n, asym_data->rsa_size, EXT_MEM),
										  &BLOCK_T_CONV((u8 *)ctx->d, asym_data->rsa_size, EXT_MEM),
										  &BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
										  ALG_SHA1, asym_data->be_param);
			break;

		case ASYM_PUB_DEC:
			hw_ret = rsa_pub_decrypt_blk(tmpl_rsa->sdce,
										 asym_data->padding,
										 &BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
										 &BLOCK_T_CONV(ctx->n, asym_data->rsa_size, EXT_MEM),
										 &BLOCK_T_CONV(ctx->e, asym_data->rsa_size, EXT_MEM),
										 &BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
										 &sign_len, ALG_SHA1, asym_data->be_param);

			if (!hw_ret) {
				ret = copy_to_user(asym_data->sign_len, &sign_len, sizeof(int));
			}

			break;

		case ASYM_SIGN:
			hw_ret = rsa_signature_by_digest(tmpl_rsa->sdce,
											 asym_data->digest_type,
											 asym_data->padding,
											 &BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
											 &BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
											 &BLOCK_T_CONV(ctx->n, asym_data->rsa_size, EXT_MEM),
											 &BLOCK_T_CONV(ctx->d, asym_data->rsa_size, EXT_MEM),
											 0, asym_data->be_param);

			if (!hw_ret) {
				ret = copy_to_user(asym_data->sign_len, &asym_data->dst_len, sizeof(int));
			}

			break;

		case ASYM_VERIFY:
			ret = copy_from_user(output_buff + output_offset, asym_data->dst, asym_data->dst_len);

			if (unlikely(ret)) {
				pr_err("cpy signature fail.\n");
				goto out2;
			}

			hw_ret = rsa_signature_verify_by_digest(tmpl_rsa->sdce,
													asym_data->digest_type,
													asym_data->padding,
													&BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
													&BLOCK_T_CONV(ctx->n, asym_data->rsa_size, EXT_MEM),
													&BLOCK_T_CONV(ctx->e, asym_data->rsa_size, EXT_MEM),
													&BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
													0, asym_data->be_param);
			break;

		default:
			ret = EINVAL;
			goto out2;
	}

	if (!hw_ret) {
		ret = hw_ret;
	}
	else {
		ret = -hw_ret;
	}

	if (unlikely(ret)) {
		pr_err("enc by hw fail, ret: %d, hw_ret: %d\n", ret, hw_ret);
		goto out2;
	}

	if (ASYM_VERIFY != asym_data->asym_op) {
		ret = copy_to_user(asym_data->dst, output_buff + output_offset, asym_data->dst_len);

		if (unlikely(ret)) {
			pr_err("cpy dst fail.\n");
			goto out2;
		}
	}

out2:
	kfree(output_buff);

out1:
	kfree(input_buff);

	return ret;
}

static int semidrive_rsa_enc(struct akcipher_request *req)
{
	pr_err("%s\n", __FUNCTION__);
	return 0;
}

static int semidrive_rsa_dec(struct akcipher_request *req)
{
	pr_err("%s\n", __FUNCTION__);
	return 0;
}

static int semidrive_rsa_sign(struct akcipher_request *req)
{
	pr_err("%s\n", __FUNCTION__);
	return 0;
}


static int semidrive_rsa_verify(struct akcipher_request *req)
{
	pr_err("%s\n", __FUNCTION__);
	return 0;
}

static int check_key_size(unsigned int len)
{
	unsigned int bitslen = len << 3;

	switch (bitslen) {
		case 1024:
		case 2048:
		case 3072:
		case 4096:
			return 1;

		default:
			return 0;
	};
}

static int semidrive_rsa_set_n(struct semidrive_rsa_ctx *ctx, const char *value,
							   size_t vlen)
{
	const char *ptr = value;
	int ret;

	while (!*ptr && vlen) {
		ptr++;
		vlen--;
	}

	ctx->key_sz = vlen;
	ret = -EINVAL;

	/* invalid key size provided */
	if (!check_key_size(ctx->key_sz)) {
		goto err;
	}

	memcpy(ctx->n, ptr, ctx->key_sz);
	return 0;

err:
	ctx->key_sz = 0;
	return ret;
}

static int semidrive_rsa_set_e(struct semidrive_rsa_ctx *ctx, const char *value,
							   size_t vlen)
{
	const char *ptr = value;

	while (!*ptr && vlen) {
		ptr++;
		vlen--;
	}

	if (!ctx->key_sz || !vlen || vlen > ctx->key_sz) {
		return -EINVAL;
	}

	memcpy(ctx->e + (ctx->key_sz - vlen), ptr, vlen);
	return 0;
}

static int semidrive_rsa_set_d(struct semidrive_rsa_ctx *ctx, const char *value,
							   size_t vlen)
{
	const char *ptr = value;
	int ret;

	while (!*ptr && vlen) {
		ptr++;
		vlen--;
	}

	ret = -EINVAL;

	if (!ctx->key_sz || !vlen || vlen > ctx->key_sz) {
		return ret;
	}

	memcpy(ctx->d + (ctx->key_sz - vlen), ptr, vlen);
	return 0;
}

static int semidrive_rsa_setkey(struct crypto_akcipher *tfm, const void *key,
								unsigned int keylen, bool private)
{
	struct semidrive_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rsa_key raw_key;
	int ret;

	memset(&raw_key, 0, sizeof(raw_key));

	/* Code borrowed from crypto/rsa.c */
	if (private) {
		ret = rsa_parse_priv_key(&raw_key, key, keylen);
	}
	else {
		ret = rsa_parse_pub_key(&raw_key, key, keylen);
	}

	if (ret) {
		goto free;
	}

	ret = semidrive_rsa_set_n(ctx, raw_key.n, raw_key.n_sz);

	if (ret < 0) {
		goto free;
	}

	ret = semidrive_rsa_set_e(ctx, raw_key.e, raw_key.e_sz);

	if (ret < 0) {
		goto free;
	}

	if (private) {
		ret = semidrive_rsa_set_d(ctx, raw_key.d, raw_key.d_sz);

		if (ret < 0) {
			goto free;
		}
	}

	return 0;

free:
	return ret;
}

static int semidrive_rsa_setprikey(struct crypto_akcipher *tfm, const void *key,
								   unsigned int keylen)
{
	return semidrive_rsa_setkey(tfm, key, keylen, true);
}

static int semidrive_rsa_setpubkey(struct crypto_akcipher *tfm, const void *key,
								   unsigned int keylen)
{
	return semidrive_rsa_setkey(tfm, key, keylen, false);
}

static int semidrive_rsa_init(struct crypto_akcipher *tfm)
{
	struct semidrive_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

static void semidrive_rsa_exit(struct crypto_akcipher *tfm)
{
}

static unsigned int semidrive_rsa_max_size(struct crypto_akcipher *tfm)
{
	struct semidrive_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);

	return ctx->key_sz;
}

static struct akcipher_alg akcippher_rsa = {
	.encrypt = semidrive_rsa_enc,
	.decrypt = semidrive_rsa_dec,
	.sign = semidrive_rsa_sign,
	.verify = semidrive_rsa_verify,
	.set_priv_key = semidrive_rsa_setprikey,
	.set_pub_key = semidrive_rsa_setpubkey,
	.max_size = semidrive_rsa_max_size,
	.init = semidrive_rsa_init,
	.exit = semidrive_rsa_exit,
	.base = {
		.cra_name = "pkc(rsa)",
		.cra_driver_name = "rsa-sdce",
		.cra_priority = 300,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct semidrive_rsa_ctx),
	},
};

static int semidrive_akcipher_rsa_register(struct crypto_dev *sdce)
{
	int ret;

	ret = crypto_register_akcipher(&akcippher_rsa);

	if (ret) {
		return ret;
	}

	tmpl_rsa = kzalloc(sizeof(*tmpl_rsa), GFP_KERNEL);

	if (!tmpl_rsa) {
		return -ENOMEM;
	}

	tmpl_rsa->sdce = sdce;

	return 0;
}

static void semidrive_akcipher_rsa_unregister(struct crypto_dev *sdce)
{
	crypto_unregister_akcipher(&akcippher_rsa);

	if (tmpl_rsa) {
		kfree(tmpl_rsa);
	}
}

const struct semidrive_alg_ops akcipher_ops = {
	.type = CRYPTO_ALG_TYPE_AKCIPHER,
	.register_algs = semidrive_akcipher_rsa_register,
	.unregister_algs = semidrive_akcipher_rsa_unregister,
};

