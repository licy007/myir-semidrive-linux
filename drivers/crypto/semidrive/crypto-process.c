/*
 * Driver for /dev/crypto device (aka CryptoDev)
 *
 * Copyright (c) 2004 Michal Ludvig <mludvig@logix.net.nz>, SuSE Labs
 * Copyright (c) 2009-2013 Nikos Mavrogiannopoulos <nmav@gnutls.org>
 *
 * This file is part of linux cryptodev.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * Device /dev/crypto provides an interface for
 * accessing kernel CryptoAPI algorithms (ciphers,
 * hashes) from userspace programs.
 *
 * /dev/crypto interface was originally introduced in
 * OpenBSD and this module attempts to keep the API.
 *
 */
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/ioctl.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <crypto/cryptodev.h>
#include <crypto/scatterwalk.h>
#include <linux/scatterlist.h>
#include <linux/version.h>
#include <crypto/cryptodev.h>
#include <crypto/internal/akcipher.h>

#include "cryptodev_int.h"
#include "zc.h"
#include "cryptlib.h"
#include "sdrv-rsa.h"
#include "sx_sm2.h"
#include "sx_ecc.h"
#include "sx_hash.h"
#include "sx_ecc_keygen.h"

/* This file contains the traditional operations of encryption
 * and hashing of /dev/crypto.
 */

static int
hash_n_crypt(struct crypto_dev *crypto, struct csession *ses_ptr, struct crypt_op *cop,
			 struct scatterlist *src_sg, struct scatterlist *dst_sg,
			 uint32_t len)
{
	int ret;

	/* Always hash before encryption and after decryption. Maybe
	 * we should introduce a flag to switch... TBD later on.
	 */
	if (cop->op == COP_ENCRYPT) {
		if (ses_ptr->hdata.init != 0) {
			ret = cryptodev_hash_update(crypto, &ses_ptr->hdata,
										src_sg, len);

			if (unlikely(ret)) {
				goto out_err;
			}
		}

		if (ses_ptr->cdata.init != 0) {
			ret = cryptodev_cipher_encrypt(&ses_ptr->cdata,
										   src_sg, dst_sg, len);

			if (unlikely(ret)) {
				goto out_err;
			}
		}
	}
	else {
		if (ses_ptr->cdata.init != 0) {
			ret = cryptodev_cipher_decrypt(&ses_ptr->cdata,
										   src_sg, dst_sg, len);

			if (unlikely(ret)) {
				goto out_err;
			}
		}

		if (ses_ptr->hdata.init != 0) {
			ret = cryptodev_hash_update(crypto, &ses_ptr->hdata,
										dst_sg, len);

			if (unlikely(ret)) {
				goto out_err;
			}
		}
	}

	return 0;
out_err:
	pr_err("CryptoAPI failure: %d", ret);
	return ret;
}

/* This is the main crypto function - feed it with plaintext
   and get a ciphertext (or vice versa :-) */
static int
__crypto_run_std(struct crypto_dev *crypto, struct csession *ses_ptr, struct crypt_op *cop)
{
	char *data;
	char __user *src, *dst;
	struct scatterlist sg;
	size_t nbytes, bufsize;
	int ret = 0;

	nbytes = cop->len;
	data = (char *)__get_free_page(GFP_KERNEL);

	if (unlikely(!data)) {
		pr_err("Error getting free page.");
		return -ENOMEM;
	}

	bufsize = PAGE_SIZE < nbytes ? PAGE_SIZE : nbytes;

	src = cop->src;
	dst = cop->dst;

	while (nbytes > 0) {
		size_t current_len = nbytes > bufsize ? bufsize : nbytes;

		if (unlikely(copy_from_user(data, src, current_len))) {
			pr_err("Error copying %zu bytes from user address %p.", current_len, src);
			ret = -EFAULT;
			break;
		}

		sg_init_one(&sg, data, current_len);

		ret = hash_n_crypt(crypto, ses_ptr, cop, &sg, &sg, current_len);

		if (unlikely(ret)) {
			pr_err("hash_n_crypt failed.");
			break;
		}

		if (ses_ptr->cdata.init != 0) {
			if (unlikely(copy_to_user(dst, data, current_len))) {
				pr_err("could not copy to user.");
				ret = -EFAULT;
				break;
			}
		}

		dst += current_len;
		nbytes -= current_len;
		src += current_len;
	}

	free_page((unsigned long)data);
	return ret;
}

/* This is the main crypto function - zero-copy edition */
static int
__crypto_run_zc(struct csession *ses_ptr, struct kernel_crypt_op *kcop)
{
	struct scatterlist *src_sg, *dst_sg;
	struct crypt_op *cop = &kcop->cop;
	int ret = 0;
	ret = get_userbuf(ses_ptr, cop->src, cop->len, cop->dst, cop->len,
					  kcop->task, kcop->mm, &src_sg, &dst_sg);

	if (unlikely(ret)) {
		pr_err("Error getting user pages. Falling back to non zero copy.");
		return __crypto_run_std(NULL, ses_ptr, cop);
	}

	ret = hash_n_crypt(NULL, ses_ptr, cop, src_sg, dst_sg, cop->len);

	release_user_pages(ses_ptr);
	return ret;
}

int crypto_run(struct crypto_dev *crypto, struct fcrypt *fcr, struct kernel_crypt_op *kcop)
{
	struct csession *ses_ptr;
	struct crypt_op *cop = &kcop->cop;
	int ret;

	if (unlikely(cop->op != COP_ENCRYPT && cop->op != COP_DECRYPT)) {
		pr_err("invalid operation op=%u", cop->op);
		return -EINVAL;
	}

	/* this also enters ses_ptr->sem */
	ses_ptr = crypto_get_session_by_sid(fcr, cop->ses);

	if (unlikely(!ses_ptr)) {
		pr_err("invalid session ID=0x%08X", cop->ses);
		return -EINVAL;
	}

	if (ses_ptr->hdata.init != 0 && (cop->flags == 0 || cop->flags & COP_FLAG_RESET)) {
		ret = cryptodev_hash_reset(&ses_ptr->hdata);

		if (unlikely(ret)) {
			pr_err("error in cryptodev_hash_reset()");
			goto out_unlock;
		}
	}

	if (ses_ptr->cdata.init != 0) {
		int blocksize = ses_ptr->cdata.blocksize;

		if (unlikely(cop->len % blocksize)) {
			pr_err("data size (%u) isn't a multiple of block size (%u)",
				   cop->len, blocksize);
			ret = -EINVAL;
			goto out_unlock;
		}

		cryptodev_cipher_set_iv(&ses_ptr->cdata, kcop->iv,
								min(ses_ptr->cdata.ivsize, kcop->ivlen));
	}

	if (likely(cop->len)) {
		if (!(cop->flags & COP_FLAG_NO_ZC)) {
			if (unlikely(ses_ptr->alignmask && !IS_ALIGNED((unsigned long)cop->src, ses_ptr->alignmask + 1))) {
				pr_err("source address %p is not %d byte aligned - disabling zero copy",
					   cop->src, ses_ptr->alignmask + 1);
				cop->flags |= COP_FLAG_NO_ZC;
			}

			if (unlikely(ses_ptr->alignmask && !IS_ALIGNED((unsigned long)cop->dst, ses_ptr->alignmask + 1))) {
				pr_err("destination address %p is not %d byte aligned - disabling zero copy",
					   cop->dst, ses_ptr->alignmask + 1);
				cop->flags |= COP_FLAG_NO_ZC;
			}
		}

		if (cop->flags & COP_FLAG_NO_ZC) {
			pr_err("COP_FLAG_NO_ZC");
			ret = __crypto_run_std(crypto, ses_ptr, &kcop->cop);
		}
		else {
			ret = __crypto_run_zc(ses_ptr, kcop);
		}

		if (unlikely(ret)) {
			goto out_unlock;
		}
	}

	if (ses_ptr->cdata.init != 0) {
		cryptodev_cipher_get_iv(&ses_ptr->cdata, kcop->iv,
								min(ses_ptr->cdata.ivsize, kcop->ivlen));
	}

	if (ses_ptr->hdata.init != 0 &&
			((cop->flags & COP_FLAG_FINAL) ||
			 (!(cop->flags & COP_FLAG_UPDATE) || cop->len == 0))) {

		ret = cryptodev_hash_final(crypto, &ses_ptr->hdata, kcop->hash_output);

		if (unlikely(ret)) {
			pr_err("CryptoAPI failure: %d", ret);
			goto out_unlock;
		}

		kcop->digestsize = ses_ptr->hdata.digestsize;
	}

out_unlock:
	crypto_put_session(ses_ptr);
	return ret;
}

int cryptodev_rsa(struct crypto_dev *crypto, struct crypt_asym *asym_data)
{
	int ret = EINVAL;
	//struct crypto_akcipher *tfm = NULL;

	//tfm = crypto_alloc_akcipher("pkc(rsa)", 0, 0);
	//if (unlikely(IS_ERR(tfm))) {
	//	pr_err("Failed to load cipher %s\n", "pkc(rsa)");
	//	ret = -ENOMEM;
	//	return ret;
	//}

	//struct semidrive_rsa_ctx *ctx = akcipher_tfm_ctx(tfm);

	struct semidrive_rsa_ctx *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	if (!ctx) {
		ret = -ENOMEM;
		goto error;
	}

	switch (asym_data->asym_op) {
		case ASYM_ENC:
		case ASYM_PUB_DEC:
		case ASYM_VERIFY:
			ret = copy_from_user(ctx->n, asym_data->n, asym_data->rsa_size);

			if (unlikely(ret)) {
				pr_err("cpy n fail.\n");
				goto error;
			}

			ret = copy_from_user(ctx->e, asym_data->e, asym_data->e_len);

			if (unlikely(ret)) {
				pr_err("cpy e fail.\n");
				goto error;
			}

			break;

		case ASYM_DEC:
		case ASYM_PRIV_ENC:
		case ASYM_SIGN:
			ret = copy_from_user(ctx->n, asym_data->n, asym_data->rsa_size);

			if (unlikely(ret)) {
				pr_err("cpy n fail.\n");
				goto error;
			}

			ret = copy_from_user(ctx->d, asym_data->d, asym_data->rsa_size);

			if (unlikely(ret)) {
				pr_err("cpy e fail.\n");
				goto error;
			}

			break;

		default:
			ret = EINVAL;
			goto error;
	}

	ret = __semidrive_rsa_op(crypto, NULL, asym_data, ctx);

	if (unlikely(ret)) {
		pr_err("__semidrive_rsa_enc fail.\n");
		goto error;
	}

error:
	kfree(ctx);
	//crypto_free_akcipher(tfm);
	return ret;
}

int cryptodev_sm2(struct crypto_dev *crypto, struct crypt_asym *asym_data)
{
	int ret = 0;
	void *input_buff = NULL;
	void *output_buff = NULL;
	u32 input_offset = 0;
	u32 output_offset = 0;
	addr_t phy_addr;
	u8 __attribute__((__aligned__(CACHE_LINE))) key[SM2_KEY_SIZE * 2];
	int key_size = SM2_KEY_SIZE * 2;

	input_buff = kzalloc(asym_data->src_len + CACHE_LINE, GFP_KERNEL);

	if (!input_buff) {
		pr_err("alloc fail line : %d\n", __LINE__);
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

	if (ASYM_SIGN == asym_data->asym_op) {
		key_size = SM2_KEY_SIZE;
	}

	ret = copy_from_user(key, asym_data->key, key_size);

	if (unlikely(ret)) {
		pr_err("cpy key fail.\n");
		goto out2;
	}

	if (ASYM_SIGN == asym_data->asym_op) {
		ret = sm2_generate_signature((void *)crypto, &sx_ecc_sm2_curve_p256,
									 &BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
									 &BLOCK_T_CONV(key, key_size, EXT_MEM),
									 &BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
									 ALG_SM3);

		if (!ret) {
			ret = copy_to_user(asym_data->dst, output_buff + output_offset, asym_data->dst_len);
		}
	}
	else {
		ret = copy_from_user(output_buff + output_offset, asym_data->dst, asym_data->dst_len);

		if (unlikely(ret)) {
			pr_err("cpy key fail.\n");
			goto out2;
		}

		ret = sm2_verify_signature((void *)crypto,
								   &sx_ecc_sm2_curve_p256,
								   &BLOCK_T_CONV(input_buff + input_offset, asym_data->src_len, EXT_MEM),
								   &BLOCK_T_CONV(key, key_size, EXT_MEM),
								   &BLOCK_T_CONV(output_buff + output_offset, asym_data->dst_len, EXT_MEM),
								   ALG_SM3);
	}

out2:
	kfree(output_buff);

out1:
	kfree(input_buff);

	return ret;
}

int cryptodev_ecc_key_gen(struct crypto_dev *crypto, struct crypt_asym *asym_data)
{
	u8 __attribute__((__aligned__(CACHE_LINE))) pub_key[SM2_KEY_SIZE * 2];
	u8 __attribute__((__aligned__(CACHE_LINE))) priv_key[SM2_KEY_SIZE];

	int ret = ecc_generate_keypair((void *)crypto,
								   &sx_ecc_sm2_curve_p256.params,
								   &BLOCK_T_CONV(pub_key, SM2_KEY_SIZE * 2, EXT_MEM),
								   &BLOCK_T_CONV((void *)(sx_ecc_sm2_curve_p256.params.addr + SM2_KEY_SIZE), SM2_KEY_SIZE, EXT_MEM),
								   &BLOCK_T_CONV(priv_key, SM2_KEY_SIZE, EXT_MEM),
								   SM2_KEY_SIZE,
								   sx_ecc_sm2_curve_p256.pk_flags);

	if (ret) {
		pr_err("generate ecc key fail.\n");
		return ret;
	}

	ret = copy_to_user(asym_data->dst, priv_key, SM2_KEY_SIZE);
	ret = copy_to_user(asym_data->dst + SM2_KEY_SIZE, pub_key, SM2_KEY_SIZE * 2);

	return ret;
}

int cryptodev_pke(struct crypto_dev *crypto, struct crypt_asym *asym_data)
{
	int ret = EINVAL;

	switch (asym_data->asym_alg) {
		case CRYPTO_RSA:
			return cryptodev_rsa(crypto, asym_data);

		case CRYPTO_SM2:
			if (ASYM_KEY_GEN == asym_data->asym_op) {
				return cryptodev_ecc_key_gen(crypto, asym_data);
			}

			return cryptodev_sm2(crypto, asym_data);

		default:
			break;
	}

	return ret;
}
