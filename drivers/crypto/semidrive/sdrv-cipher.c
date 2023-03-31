/*
 * Copyright (c) 2019 Semidrive Semiconductor.
 * All rights reserved.
 *
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <crypto/aes.h>
#include <crypto/internal/skcipher.h>

#include "sdrv-cipher.h"
#include "sx_cipher.h"

static LIST_HEAD(ablkcipher_algs);

__attribute__((unused)) static void semidrive_ablkcipher_done(void *data)
{
	return;
}

static int
semidrive_ablkcipher_async_req_handle(struct crypto_async_request *async_req)
{
	struct ablkcipher_request *req = ablkcipher_request_cast(async_req);
	struct semidrive_cipher_reqctx *rctx = ablkcipher_request_ctx(req);
	struct crypto_ablkcipher *ablkcipher = crypto_ablkcipher_reqtfm(req);
	struct semidrive_ce_alg_template *tmpl = to_cipher_tmpl(async_req->tfm);
	struct crypto_dev *sdce = tmpl->sdce;
	struct crypto_tfm *tfm = crypto_ablkcipher_tfm(ablkcipher);
	struct semidrive_cipher_ctx *ctx = crypto_tfm_ctx(tfm);
	bool diff_dst;
	int ret;
	int aes_ctx_size = AES_CTX_SIZE;
	u32 cpy_offset = 0;
	int i, j;
	int input_offset, output_offset, remain_len;
	aes_fct_t alg_fct;
	aes_mode_t alg_mode;
	u32 alg_fct_all = SD_MODE_CBC | SD_MODE_ECB | SD_MODE_CTR | SD_MODE_XTS
					  | SD_MODE_CCM | SD_MODE_GCM | SD_MODE_CFB | SD_MODE_OFB
					  | SD_MODE_CMAC;

	rctx->ivsize = crypto_ablkcipher_ivsize(ablkcipher);
	memcpy(rctx->iv, req->info, rctx->ivsize);
	rctx->cryptlen = req->nbytes;
	rctx->dst_sg = req->dst;
	rctx->src_sg = req->src;

	diff_dst = (req->src != req->dst) ? true : false;

	rctx->src_nents = sg_nents_for_len(req->src, req->nbytes);

	if (diff_dst) {
		rctx->dst_nents = sg_nents_for_len(req->dst, req->nbytes);
	}
	else {
		rctx->dst_nents = rctx->src_nents;
	}

	if (rctx->src_nents < 0) {
		dev_err(sdce->dev, "Invalid numbers of src SG.\n");
		return -rctx->src_nents;
	}

	if (rctx->dst_nents < 0) {
		dev_err(sdce->dev, "Invalid numbers of dst SG.\n");
		return -rctx->dst_nents;
	}

	switch (rctx->flags & alg_fct_all) {
		case SD_MODE_CBC:
			alg_fct = FCT_CBC;
			break;

		case SD_MODE_ECB:
			alg_fct = FCT_ECB;
			aes_ctx_size = 0;
			rctx->ivsize = 0;
			break;

		case SD_MODE_CTR:
			alg_fct = FCT_CTR;
			break;

		case SD_MODE_XTS:
			alg_fct = FCT_XTS;
			break;

		case SD_MODE_CCM:
			alg_fct = FCT_CCM;
			break;

		case SD_MODE_GCM:
			alg_fct = FCT_GCM;
			break;

		case SD_MODE_CFB:
			alg_fct = FCT_CFB;
			break;

		case SD_MODE_OFB:
			alg_fct = FCT_OFB;
			break;

		case SD_MODE_CMAC:
			alg_fct = FCT_CMAC;
			break;

		default:
			alg_fct = FCT_ECB;
	}

	if (SD_ENCRYPT == (rctx->flags & SD_ENCRYPT)) {
		alg_mode = MODE_ENC;
	}
	else {
		alg_mode = MODE_DEC;
	}

	spin_lock(&sdce->cipher_lock);

	if (rctx->cryptlen <= CIPHER_BUF_SIZE) {
		for (i = 0; i < rctx->src_nents; i++) {
			memcpy(cipher_op_input_buf + cpy_offset,
				   sg_virt(&rctx->src_sg[i]), (rctx->src_sg[i]).length);
			cpy_offset += (rctx->src_sg[i]).length;
		}

		ret = aes_blk(alg_fct, sdce, alg_mode,
					  ctx->aes_ctx,
					  0,
					  false,
					  false,
					  (block_t)BLOCK_T_CONV(ctx->enc_key,
											ctx->enc_keylen, EXT_MEM),
					  (block_t)BLOCK_T_CONV(NULL, 0, EXT_MEM),
					  (block_t)BLOCK_T_CONV(rctx->iv, rctx->ivsize, EXT_MEM),
					  (block_t)BLOCK_T_CONV(CE2_SRAM_SEC_AES_CTX_ADDR_OFFSET,
											aes_ctx_size, SRAM_SEC),
					  (block_t)BLOCK_T_CONV(cipher_op_input_buf,
											rctx->cryptlen, EXT_MEM),
					  &BLOCK_T_CONV(cipher_op_output_buf,
									rctx->cryptlen, EXT_MEM));

		if (!ret) {
			cpy_offset = 0;

			for (i = 0; i < rctx->src_nents; i++) {
				memcpy(sg_virt(&rctx->dst_sg[i]),
					   cipher_op_output_buf + cpy_offset,
					   (rctx->dst_sg[i]).length);
				cpy_offset += (rctx->dst_sg[i]).length;
			}
		}
	}
	else {//req size > CIPHER_BUF_SIZE
		i = 0;
		j = 0;
		input_offset = 0;
		output_offset = 0;

		for (remain_len = rctx->cryptlen; remain_len > 0;) {
			cpy_offset = 0;

			for (; i < rctx->src_nents; i++) {
				if (cpy_offset + (rctx->src_sg[i]).length - input_offset <= CIPHER_BUF_SIZE) {
					memcpy(cipher_op_input_buf + cpy_offset,
						   sg_virt(&rctx->src_sg[i]) + input_offset, (rctx->src_sg[i]).length - input_offset);

					cpy_offset += (rctx->src_sg[i]).length - input_offset;

					if (input_offset > 0) {
						input_offset = 0;
					}
				}
				else {
					memcpy(cipher_op_input_buf + cpy_offset,
						   sg_virt(&rctx->src_sg[i]) + input_offset, CIPHER_BUF_SIZE - cpy_offset);//(rctx->src_sg[i]).length);

					input_offset += CIPHER_BUF_SIZE - cpy_offset;
					cpy_offset = CIPHER_BUF_SIZE;
					break;
				}
			}

			ret = aes_blk(alg_fct, sdce, alg_mode,
						  ctx->aes_ctx,
						  0,
						  false,
						  false,
						  (block_t)BLOCK_T_CONV(ctx->enc_key,
												ctx->enc_keylen, EXT_MEM),
						  (block_t)BLOCK_T_CONV(NULL, 0, EXT_MEM),
						  (block_t)BLOCK_T_CONV(rctx->iv, rctx->ivsize, EXT_MEM),
						  (block_t)BLOCK_T_CONV(CE2_SRAM_SEC_AES_CTX_ADDR_OFFSET,
												aes_ctx_size, SRAM_SEC),
						  (block_t)BLOCK_T_CONV(cipher_op_input_buf,
												cpy_offset, EXT_MEM),
						  &BLOCK_T_CONV(cipher_op_output_buf,
										cpy_offset, EXT_MEM));

			if (ret) {
				goto OUT;
			}

			cpy_offset = 0;

			for (; j < rctx->src_nents; j++) {
				if (cpy_offset + (rctx->dst_sg[j]).length - output_offset <= CIPHER_BUF_SIZE) {
					memcpy(sg_virt(&rctx->dst_sg[j]) + output_offset,
						   cipher_op_output_buf + cpy_offset,
						   (rctx->dst_sg[j]).length - output_offset);

					cpy_offset += (rctx->dst_sg[j]).length - output_offset;

					if (output_offset > 0) {
						output_offset = 0;
					}
				}
				else {
					memcpy(sg_virt(&rctx->dst_sg[j]) + output_offset,
						   cipher_op_output_buf + cpy_offset,
						   CIPHER_BUF_SIZE - cpy_offset);

					output_offset += CIPHER_BUF_SIZE - cpy_offset;
					cpy_offset += CIPHER_BUF_SIZE;
					break;
				}
			}

			remain_len -= cpy_offset;
		}
	}

OUT:
	spin_unlock(&sdce->cipher_lock);
	sdce->async_req_done(tmpl->sdce, ret);

	return ret;
}

static int semidrive_ablkcipher_setkey(struct crypto_ablkcipher *ablk, const u8 *key,
									   unsigned int keylen)
{
	struct crypto_tfm *tfm = crypto_ablkcipher_tfm(ablk);
	struct semidrive_cipher_ctx *ctx = crypto_tfm_ctx(tfm);
	unsigned long flags = to_cipher_tmpl(tfm)->alg_flags;
	int ret;

	if (!key || !keylen) {
		return -EINVAL;
	}

	if (IS_AES(flags)) {
		switch (keylen) {
			case AES_KEYSIZE_128:
			case AES_KEYSIZE_192:
			case AES_KEYSIZE_256:
				break;

			default:
				goto fallback;
		}
	}

	ctx->enc_keylen = keylen;
	memcpy(ctx->enc_key, key, keylen);
	return 0;

fallback:
	ret = crypto_skcipher_setkey(ctx->fallback, key, keylen);

	if (!ret) {
		ctx->enc_keylen = keylen;
	}

	return ret;
}

static int semidrive_ablkcipher_crypt(struct ablkcipher_request *req, int encrypt)
{
	struct crypto_tfm *tfm =
		crypto_ablkcipher_tfm(crypto_ablkcipher_reqtfm(req));
	struct semidrive_cipher_ctx *ctx = crypto_tfm_ctx(tfm);
	struct semidrive_cipher_reqctx *rctx = ablkcipher_request_ctx(req);
	struct semidrive_ce_alg_template *tmpl = to_cipher_tmpl(tfm);
	int ret;

	rctx->flags = tmpl->alg_flags;
	rctx->flags |= encrypt ? SD_ENCRYPT : SD_DECRYPT;

	if (IS_AES(rctx->flags) && ctx->enc_keylen != AES_KEYSIZE_128 &&
			ctx->enc_keylen != AES_KEYSIZE_192 &&
			ctx->enc_keylen != AES_KEYSIZE_256) {
		SKCIPHER_REQUEST_ON_STACK(subreq, ctx->fallback);

		skcipher_request_set_tfm(subreq, ctx->fallback);
		skcipher_request_set_callback(subreq, req->base.flags,
									  NULL, NULL);
		skcipher_request_set_crypt(subreq, req->src, req->dst,
								   req->nbytes, req->info);
		ret = encrypt ? crypto_skcipher_encrypt(subreq) :
			  crypto_skcipher_decrypt(subreq);
		skcipher_request_zero(subreq);

		return ret;
	}

	return tmpl->sdce->async_req_enqueue(tmpl->sdce, &req->base);
}

static int semidrive_ablkcipher_encrypt(struct ablkcipher_request *req)
{
	return semidrive_ablkcipher_crypt(req, 1);
}

static int semidrive_ablkcipher_decrypt(struct ablkcipher_request *req)
{
	return semidrive_ablkcipher_crypt(req, 0);
}

static int semidrive_ablkcipher_init(struct crypto_tfm *tfm)
{
	struct semidrive_cipher_ctx *ctx = crypto_tfm_ctx(tfm);

	memset(ctx, 0, sizeof(*ctx));
	ctx->aes_ctx = CTX_BEGIN;

	tfm->crt_ablkcipher.reqsize = sizeof(struct semidrive_cipher_reqctx);

	ctx->fallback = crypto_alloc_skcipher(crypto_tfm_alg_name(tfm), 0,
										  CRYPTO_ALG_ASYNC |
										  CRYPTO_ALG_NEED_FALLBACK);

	if (IS_ERR(ctx->fallback)) {
		return PTR_ERR(ctx->fallback);
	}

	return 0;
}

static void semidrive_ablkcipher_exit(struct crypto_tfm *tfm)
{
	struct semidrive_cipher_ctx *ctx = crypto_tfm_ctx(tfm);

	crypto_free_skcipher(ctx->fallback);
}

struct semidrive_ablkcipher_def {
	unsigned long flags;
	const char *name;
	const char *drv_name;
	unsigned int blocksize;
	unsigned int ivsize;
	unsigned int min_keysize;
	unsigned int max_keysize;
};

static const struct semidrive_ablkcipher_def ablkcipher_def[] = {
	{
		.flags		= SD_ALG_AES | SD_MODE_ECB,
		.name		= "ecb(aes)",
		.drv_name	= "ecb-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_CBC,
		.name		= "cbc(aes)",
		.drv_name	= "cbc-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_CTR,
		.name		= "ctr(aes)",
		.drv_name	= "ctr-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_XTS,
		.name		= "xts(aes)",
		.drv_name	= "xts-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_CCM,
		.name		= "ccm(aes)",
		.drv_name	= "ccm-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_GCM,
		.name		= "gcm(aes)",
		.drv_name	= "gcm-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_CFB,
		.name		= "cfb(aes)",
		.drv_name	= "cfb-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_OFB,
		.name		= "ofb(aes)",
		.drv_name	= "ofb-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
	{
		.flags		= SD_ALG_AES | SD_MODE_CMAC,
		.name		= "cmac(aes)",
		.drv_name	= "cmac-aes-sdce",
		.blocksize	= AES_BLOCK_SIZE,
		.ivsize		= AES_BLOCK_SIZE,
		.min_keysize	= AES_MIN_KEY_SIZE,
		.max_keysize	= AES_MAX_KEY_SIZE,
	},
};

static int semidrive_ablkcipher_register_one(const struct semidrive_ablkcipher_def *def,
		struct crypto_dev *sdce)
{
	struct semidrive_ce_alg_template *tmpl;
	struct crypto_alg *alg;
	int ret;

	tmpl = kzalloc(sizeof(*tmpl), GFP_KERNEL);

	if (!tmpl) {
		return -ENOMEM;
	}

	alg = &tmpl->alg.crypto;

	snprintf(alg->cra_name, CRYPTO_MAX_ALG_NAME, "%s", def->name);
	snprintf(alg->cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
			 def->drv_name);

	alg->cra_blocksize = def->blocksize;
	alg->cra_ablkcipher.ivsize = def->ivsize;
	alg->cra_ablkcipher.min_keysize = def->min_keysize;
	alg->cra_ablkcipher.max_keysize = def->max_keysize;
	alg->cra_ablkcipher.setkey = semidrive_ablkcipher_setkey;
	alg->cra_ablkcipher.encrypt = semidrive_ablkcipher_encrypt;
	alg->cra_ablkcipher.decrypt = semidrive_ablkcipher_decrypt;

	alg->cra_priority = 300;
	alg->cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC |
					 CRYPTO_ALG_NEED_FALLBACK;
	alg->cra_ctxsize = sizeof(struct semidrive_cipher_ctx);
	alg->cra_alignmask = 0;
	alg->cra_type = &crypto_ablkcipher_type;
	alg->cra_module = THIS_MODULE;
	alg->cra_init = semidrive_ablkcipher_init;
	alg->cra_exit = semidrive_ablkcipher_exit;
	INIT_LIST_HEAD(&alg->cra_list);

	INIT_LIST_HEAD(&tmpl->entry);
	tmpl->crypto_alg_type = CRYPTO_ALG_TYPE_ABLKCIPHER;
	tmpl->alg_flags = def->flags;
	tmpl->sdce = sdce;

	ret = crypto_register_alg(alg);

	if (ret) {
		kfree(tmpl);
		dev_err(sdce->dev, "%s registration failed\n", alg->cra_name);
		return ret;
	}

	list_add_tail(&tmpl->entry, &ablkcipher_algs);
	dev_dbg(sdce->dev, "%s is registered\n", alg->cra_name);
	return 0;
}

static void semidrive_ablkcipher_unregister(struct crypto_dev *sdce)
{
	struct semidrive_ce_alg_template *tmpl, *n;

	list_for_each_entry_safe(tmpl, n, &ablkcipher_algs, entry) {
		crypto_unregister_alg(&tmpl->alg.crypto);
		list_del(&tmpl->entry);
		kfree(tmpl);
	}
}

static int semidrive_ablkcipher_register(struct crypto_dev *sdce)
{
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(ablkcipher_def); i++) {
		ret = semidrive_ablkcipher_register_one(&ablkcipher_def[i], sdce);

		if (ret) {
			goto err;
		}
	}

	return 0;
err:
	semidrive_ablkcipher_unregister(sdce);
	return ret;
}

const struct semidrive_alg_ops ablkcipher_ops = {
	.type = CRYPTO_ALG_TYPE_ABLKCIPHER,
	.register_algs = semidrive_ablkcipher_register,
	.unregister_algs = semidrive_ablkcipher_unregister,
	.async_req_handle = semidrive_ablkcipher_async_req_handle,
};

