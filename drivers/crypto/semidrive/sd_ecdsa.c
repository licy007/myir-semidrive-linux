/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "sd_ecdsa.h"
#include "ce.h"
#include "sx_hash.h"
#include "sx_errors.h"
#include "sx_ecc.h"
#include "sx_ecdsa.h"

uint32_t sd_ecdsa_run(struct file *filp, unsigned long arg, ecdsa_mode_t mode)
{
	uint32_t ret;
	uint32_t ce_malloc_item = 4;
	struct crypto_ecdsa_instance_st l_ecdsainstance;
	int err;
	uint8_t i = 0;
	struct crypto_dev *crypto = to_crypto_dev(filp->private_data);
	int curve_nid;
	block_t pub_key;
	block_t priv_key;
	ce_addr_type_t addr_type;
	hash_alg_t hashType;
	block_t message;
	block_t signature;
	const void *curve = NULL;
	struct mem_node *mem_n[9];
	uint8_t *pub_key_buff = NULL;
	uint8_t *priv_key_buff = NULL;
	uint8_t *msg_buff = NULL;
	uint8_t *sig_buff = NULL;

	for (i = 0; i < ce_malloc_item ; i ++) {
		mem_n[i] = ce_malloc(MAX_DIGESTSIZE);

		if (mem_n[i] == NULL) {
			while (i) {
				ce_free(mem_n[i--]);
			}

			return CRYPTOLIB_PK_N_NOTVALID;
		}
	}

	pub_key_buff = mem_n[0]->ptr;
	priv_key_buff = mem_n[1]->ptr;
	msg_buff = mem_n[2]->ptr;
	sig_buff = mem_n[3]->ptr;
	/* UNIX - not MSDOS         */
	ret = -ENOTTY;
	err = copy_from_user((char *)&l_ecdsainstance, (char __user *)arg, sizeof(struct crypto_ecdsa_instance_st));

	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)arg, err);
		ce_inheap_free();
		return err;
	}

	err = copy_from_user((char *)pub_key_buff, (char __user *)l_ecdsainstance.ec_keypair.pub_key, l_ecdsainstance.ec_keypair.pub_key_len);

	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_ecdsainstance.ec_keypair.pub_key, err);
		ce_inheap_free();
		return err;
	}

	err = copy_from_user((char *)priv_key_buff, (char __user *)l_ecdsainstance.ec_keypair.priv_key, l_ecdsainstance.ec_keypair.priv_key_len);

	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_ecdsainstance.ec_keypair.priv_key, err);
		ce_inheap_free();
		return err;
	}

	err = copy_from_user((char *)msg_buff, (char __user *)l_ecdsainstance.msg, l_ecdsainstance.msg_len);

	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_ecdsainstance.msg, err);
		ce_inheap_free();
		return err;
	}

	err = copy_from_user((char *)sig_buff, (char __user *)l_ecdsainstance.sig, l_ecdsainstance.sig_len);

	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_ecdsainstance.sig, err);
		ce_inheap_free();
		return err;
	}

	curve_nid = (int)l_ecdsainstance.curve_nid;
	addr_type = (ce_addr_type_t)l_ecdsainstance.addr_type;
	hashType = (hash_alg_t)l_ecdsainstance.ecdsa_hashType;

	message.addr = msg_buff;
	message.addr_type = addr_type;
	message.len = l_ecdsainstance.msg_len;
	signature.addr = sig_buff;
	signature.addr_type = addr_type;
	signature.len = l_ecdsainstance.sig_len;
	pub_key.addr = pub_key_buff;
	pub_key.addr_type = addr_type;
	pub_key.len = l_ecdsainstance.ec_keypair.pub_key_len;
	priv_key.addr = priv_key_buff;
	priv_key.addr_type = addr_type;
	priv_key.len = l_ecdsainstance.ec_keypair.priv_key_len;

	curve = get_curve_value_by_nid(curve_nid);

	if (curve == NULL) {
		return CRYPTOLIB_INVALID_POINT;
	}

	switch (mode) {
		case ECDSA_MODE_SIG_GEN:
			ret = ecdsa_generate_signature((void *)crypto, curve, &message, &priv_key, &signature, hashType);

			if (ret) {
				pr_err("%s: ecdsa_generate_signature  failed (%d)\n", __func__, ret);
			}

			err = copy_to_user((char __user *)(l_ecdsainstance.ret), (char *)(&ret), sizeof(uint32_t));

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)l_ecdsainstance.ret, err);
				ce_inheap_free();
				return err;
			}

			err = copy_to_user((char __user *)(l_ecdsainstance.sig), (char *)(signature.addr), signature.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_ecdsainstance.sig), err);
				ce_inheap_free();
				return err;
			}

			break;

		case ECDSA_MODE_SIG_VER:
			ret = ecdsa_verify_signature((void *)crypto, curve, &message, &pub_key, &signature, hashType);

			if (ret) {
				pr_err("%s: ecdsa_verify_signature  failed (%d)\n", __func__, ret);
			}

			err = copy_to_user((char __user *)(l_ecdsainstance.ret), (char *)(&ret), sizeof(uint32_t));

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)l_ecdsainstance.ret, err);
				ce_inheap_free();
				return err;
			}

			break;

		default:
			pr_warn("%s: Unhandled ecdsa mode: 0x%x\n", __func__, mode);
			ret = -EINVAL;
	}

	for (i = 0; i < ce_malloc_item ; i ++) {
		ce_free(mem_n[i]);
	}

	return err;

}


uint32_t sd_ecdsa_generate_signature(struct file *filp, unsigned long arg)
{
	return sd_ecdsa_run(filp, arg, ECDSA_MODE_SIG_GEN);
}

uint32_t sd_ecdsa_verify_signature(struct file *filp, unsigned long arg)
{
	return sd_ecdsa_run(filp, arg, ECDSA_MODE_SIG_VER);
}







