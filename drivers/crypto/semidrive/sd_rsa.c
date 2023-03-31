
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "ce.h"
#include "sx_errors.h"
#include "sx_hash.h"
#include "sx_rsa.h"
#include "sd_rsa.h"
#include "sx_rsa_pad.h"

#define free_current_mem_n			\
do {								\
	for (i = 0; i < 11; i ++) {	\
		ce_free(mem_n[i]);			\
	}								\
} while(0)


uint32_t sd_rsa_run(struct file *filp, unsigned long arg)
{
	uint32_t ret;
	struct crypto_rsa_instance_st l_rsainstance;
	int err;
	struct crypto_dev *crypto = to_crypto_dev(filp->private_data);
	size_t RSA_len;
	uint8_t i = 0;
	block_t message;
	block_t cipher;
	block_t signature;
	block_t key_n;
	block_t key_e;
	block_t key_d;
	block_t key_p;
	block_t key_q;
	block_t key_dP;
	block_t key_dQ;
	block_t key_qInv;
	ce_addr_type_t addr_type;
	hash_alg_t hashType ;
	rsa_pad_types_t rsa_pad_types;

	struct mem_node *mem_n[11];
	uint8_t *verify_msg_buff = NULL;
	uint8_t *verify_cipher_buff = NULL;
	uint8_t *verify_sig_buff = NULL;
	uint8_t *verify_key_n_buff = NULL;
	uint8_t *verify_key_e_buff = NULL;
	uint8_t *verify_key_d_buff = NULL;
	uint8_t *verify_key_p_buff = NULL;
	uint8_t *verify_key_q_buff = NULL;
	uint8_t *verify_key_dP_buff = NULL;
	uint8_t *verify_key_dQ_buff = NULL;
	uint8_t *verify_key_qInv_buff = NULL;

	for (i = 0; i < 11; i ++) {
		mem_n[i] = ce_malloc(CE_HEAP_MALLOC_MAX_SIZE);

		if (mem_n[i] == NULL) {
			while (i) {
				ce_free(mem_n[i--]);
			}

			return CRYPTOLIB_PK_N_NOTVALID;
		}
	}

	verify_msg_buff = mem_n[0]->ptr;
	verify_cipher_buff = mem_n[1]->ptr;
	verify_sig_buff = mem_n[2]->ptr;
	verify_key_n_buff = mem_n[3]->ptr;
	verify_key_e_buff = mem_n[4]->ptr;
	verify_key_d_buff = mem_n[5]->ptr;
	verify_key_p_buff = mem_n[6]->ptr;
	verify_key_q_buff = mem_n[7]->ptr;
	verify_key_dP_buff = mem_n[8]->ptr;
	verify_key_dQ_buff = mem_n[9]->ptr;
	verify_key_qInv_buff = mem_n[10]->ptr;
	/* UNIX - not MSDOS         */
	ret = -ENOTTY;

	err = copy_from_user((char *)&l_rsainstance, (char __user *)arg, sizeof(struct crypto_rsa_instance_st));

	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)arg, err);
		free_current_mem_n;
		return err;
	}

	switch (l_rsainstance.mode) {
		case RSA_MODE_SIG_VER:
			err = copy_from_user((char *)verify_sig_buff, (char __user *)l_rsainstance.sig, l_rsainstance.sig_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_rsainstance.sig, err);
				free_current_mem_n;
				return err;
			}

		case RSA_MODE_ENC:
			err = copy_from_user((char *)verify_msg_buff, (char __user *)l_rsainstance.msg, l_rsainstance.msg_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_rsainstance.msg, err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_n_buff, (char __user *)(l_rsainstance.rsa_keypair.n), l_rsainstance.rsa_keypair.n_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.n), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_e_buff, (char __user *)(l_rsainstance.rsa_keypair.e), l_rsainstance.rsa_keypair.e_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.e), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_DEC:
			err = copy_from_user((char *)verify_cipher_buff, (char __user *)l_rsainstance.cipher, l_rsainstance.cipher_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_rsainstance.cipher, err);
				free_current_mem_n;
				return err;
			}

		case RSA_MODE_SIG_GEN:
			err = copy_from_user((char *)verify_key_n_buff, (char __user *)(l_rsainstance.rsa_keypair.n), l_rsainstance.rsa_keypair.n_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.n), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_d_buff, (char __user *)(l_rsainstance.rsa_keypair.d), l_rsainstance.rsa_keypair.d_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.d), err);
				free_current_mem_n;
				return err;
			}

			if (l_rsainstance.mode == RSA_MODE_SIG_GEN) {
				err = copy_from_user((char *)verify_msg_buff, (char __user *)l_rsainstance.msg, l_rsainstance.msg_len);

				if (err < 0) {
					pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_rsainstance.msg, err);
					free_current_mem_n;
					return err;
				}
			}

			break;

		case RSA_MODE_CRT_DEC:
			err = copy_from_user((char *)verify_cipher_buff, (char __user *)l_rsainstance.cipher, l_rsainstance.cipher_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)l_rsainstance.cipher, err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_p_buff, (char __user *)(l_rsainstance.rsa_keypair.p), l_rsainstance.rsa_keypair.p_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.p), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_q_buff, (char __user *)(l_rsainstance.rsa_keypair.q), l_rsainstance.rsa_keypair.q_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.q), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_dP_buff, (char __user *)(l_rsainstance.rsa_keypair.dP), l_rsainstance.rsa_keypair.dP_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.dP), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_dQ_buff, (char __user *)(l_rsainstance.rsa_keypair.dQ), l_rsainstance.rsa_keypair.dQ_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.dQ), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_qInv_buff, (char __user *)(l_rsainstance.rsa_keypair.qInv), l_rsainstance.rsa_keypair.qInv_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.qInv), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_PRIV_KEY_GEN:
			err = copy_from_user((char *)verify_key_p_buff, (char __user *)(l_rsainstance.rsa_keypair.p), l_rsainstance.rsa_keypair.p_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.p), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_q_buff, (char __user *)(l_rsainstance.rsa_keypair.q), l_rsainstance.rsa_keypair.q_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.q), err);
				free_current_mem_n;
				return err;
			}

		case RSA_MODE_KEY_GEN:
			err = copy_from_user((char *)verify_key_e_buff, (char __user *)(l_rsainstance.rsa_keypair.e), l_rsainstance.rsa_keypair.e_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.e), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_CRT_KEY_GEN:
			err = copy_from_user((char *)verify_key_p_buff, (char __user *)(l_rsainstance.rsa_keypair.p), l_rsainstance.rsa_keypair.p_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.p), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_q_buff, (char __user *)(l_rsainstance.rsa_keypair.q), l_rsainstance.rsa_keypair.q_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.q), err);
				free_current_mem_n;
				return err;
			}

			err = copy_from_user((char *)verify_key_d_buff, (char __user *)(l_rsainstance.rsa_keypair.d), l_rsainstance.rsa_keypair.d_len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.d), err);
				free_current_mem_n;
				return err;
			}

			break;

		default :
			pr_err("%s: rsa_mode is 0x%x\n", __func__, l_rsainstance.mode);
			free_current_mem_n;
			return -EINVAL;
	}

	RSA_len = l_rsainstance.keysize;
	rsa_pad_types = (rsa_pad_types_t)l_rsainstance.rsa_pad_types;
	addr_type = (ce_addr_type_t)l_rsainstance.addr_type;
	hashType = (hash_alg_t)l_rsainstance.rsa_hashType;
	message.addr = verify_msg_buff;
	message.addr_type = addr_type;
	message.len = l_rsainstance.msg_len;

	cipher.addr = verify_cipher_buff;
	cipher.addr_type = addr_type;
	cipher.len = l_rsainstance.cipher_len;

	signature.addr = verify_sig_buff;
	signature.addr_type = addr_type;
	signature.len = l_rsainstance.sig_len;

	key_n.addr = verify_key_n_buff;
	key_n.addr_type = addr_type;
	key_n.len = l_rsainstance.rsa_keypair.n_len;

	key_e.addr = verify_key_e_buff;
	key_e.addr_type = addr_type;
	key_e.len = l_rsainstance.rsa_keypair.e_len;

	key_d.addr = verify_key_d_buff;
	key_d.addr_type = addr_type;
	key_d.len = l_rsainstance.rsa_keypair.d_len;

	key_p.addr = verify_key_p_buff;
	key_p.addr_type = addr_type;
	key_p.len = l_rsainstance.rsa_keypair.p_len;

	key_q.addr = verify_key_q_buff;
	key_q.addr_type = addr_type;
	key_q.len = l_rsainstance.rsa_keypair.q_len;

	key_dP.addr = verify_key_dP_buff;
	key_dP.addr_type = addr_type;
	key_dP.len = l_rsainstance.rsa_keypair.dP_len;

	key_dQ.addr = verify_key_dQ_buff;
	key_dQ.addr_type = addr_type;
	key_dQ.len = l_rsainstance.rsa_keypair.dQ_len;

	key_qInv.addr = verify_key_qInv_buff;
	key_qInv.addr_type = addr_type;
	key_qInv.len = l_rsainstance.rsa_keypair.qInv_len;

	switch (l_rsainstance.mode) {
		case RSA_MODE_ENC:
			ret = rsa_encrypt_blk((void *)crypto, rsa_pad_types, &message,
								  &key_n, &key_e, &cipher, hashType, true);

			if (ret < 0) {
				free_current_mem_n;
				pr_err("%s: rsa_encrypt_blk  failed (%d)\n", __func__, ret);
				return ret;
			}

			err = copy_to_user((char __user *)(l_rsainstance.cipher), (char *)(verify_cipher_buff), cipher.len);

			if (err < 0) {
				free_current_mem_n;
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.cipher), err);
				return err;
			}

			break;

		case RSA_MODE_DEC:
			ret = rsa_decrypt_blk((void *)crypto, rsa_pad_types, &cipher,
								  &key_n, &key_d, &message, &message.len, hashType, true);

			if (ret < 0) {
				pr_err("%s: rsa_decrypt_blk  failed (%d)\n", __func__, ret);
				free_current_mem_n;
				return ret;
			}

			err = copy_to_user((char __user *)(l_rsainstance.msg), (char *)(verify_msg_buff), message.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.msg), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_CRT_DEC:
			ret = rsa_crt_decrypt_blk((void *)crypto, rsa_pad_types, &cipher,
									  &key_p, &key_q, &key_dP, &key_dQ, &key_qInv,
									  &message, &message.len, hashType);

			if (ret < 0)  {
				pr_err("%s: rsa_crt_decrypt_blk  failed (%d)\n", __func__, ret);
				free_current_mem_n;
				return ret;
			}

			err = copy_to_user((char __user *)(l_rsainstance.msg), (char *)(verify_msg_buff), message.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.msg), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_SIG_GEN:
			ret = rsa_signature_generate_blk((void *)crypto, hashType, rsa_pad_types, &message,
											 &signature, &key_n, &key_d, 0, true);

			if (ret < 0) {
				pr_err("%s: rsa_signature_generation_blk  failed (%d)\n", __func__, ret);
				free_current_mem_n;
				return ret;
			}

			err = copy_to_user((char __user *)(l_rsainstance.sig), (char *)(verify_sig_buff), signature.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.sig), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_SIG_VER:
			ret = rsa_signature_verify_blk((void *)crypto, hashType, rsa_pad_types, &message,
										   &key_n, &key_e, &signature, 0, true);

			if (ret < 0) {
				pr_err("%s: rsa_signature_verification_blk  failed (%d)\n", __func__, ret);
				free_current_mem_n;
				return ret;
			}

			break;

		case RSA_MODE_PRIV_KEY_GEN:
			ret = rsa_private_key_generate_blk((void *)crypto, &key_p, &key_q, &key_e, &key_n, &key_d,
											   l_rsainstance.rsa_keypair.p_len, 0);

			if (ret < 0) {
				pr_err("%s: rsa_private_key_generate_blk  failed (%d)\n", __func__, ret);
				free_current_mem_n;
				return ret;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.n), (char *)(verify_key_n_buff), key_n.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.n), err);
				free_current_mem_n;
				return err;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.d), (char *)(verify_key_d_buff), key_d.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.d), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_CRT_KEY_GEN:
			ret = rsa_crt_key_generate_blk((void *)crypto, &key_p, &key_q, &key_d, &key_dP, &key_dQ,
										   &key_qInv, l_rsainstance.rsa_keypair.p_len);

			if (ret < 0) {
				pr_err("%s: rsa_crt_key_generate_blk  failed (%d)\n", __func__, ret);
				free_current_mem_n;
				return ret;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.dP), (char *)(verify_key_dP_buff), key_dP.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.dP), err);
				free_current_mem_n;
				return err;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.dQ), (char *)(verify_key_dQ_buff), key_dQ.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.dQ), err);
				free_current_mem_n;
				return err;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.qInv), (char *)(verify_key_qInv_buff), key_qInv.len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.qInv), err);
				free_current_mem_n;
				return err;
			}

			break;

		case RSA_MODE_KEY_GEN:
			ret = rsa_key_generate_blk((void *)crypto, RSA_len, &key_e, &key_p, &key_q, &key_n, &key_d, 0, 0);

			if (ret < 0) {
				pr_err("%s: rsa_key_generate_blk  failed (%d)\n", __func__, ret);
				free_current_mem_n;
				return ret;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.p), (char *)(verify_key_p_buff), RSA_len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.p), err);
				free_current_mem_n;
				return err;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.q), (char *)(verify_key_q_buff), RSA_len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.q), err);
				free_current_mem_n;
				return err;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.n), (char *)(verify_key_n_buff), RSA_len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.n), err);
				free_current_mem_n;
				return err;
			}

			err = copy_to_user((char __user *)(l_rsainstance.rsa_keypair.d), (char *)(verify_key_d_buff), RSA_len);

			if (err < 0) {
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(l_rsainstance.rsa_keypair.qInv), err);
				free_current_mem_n;
				return err;
			}

			break;

		default:
			pr_err("%s: Unhandled rsa mode: 0x%x\n", __func__, l_rsainstance.mode);
			ret = -EINVAL;
			free_current_mem_n;
			return ret;
	}

	err = copy_to_user((char __user *)(l_rsainstance.ret), (char *)(&ret), sizeof(uint32_t));
	free_current_mem_n;

	if (err < 0) {
		pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)l_rsainstance.ret, err);
		return err;
	}

	return 0;


}

uint32_t ce_oaep_L_config(unsigned long arg)
{
	uint32_t ret = 0, err;
	struct mem_node *mem_n = NULL;
	struct crypto_rsa_L_config_st crypto_rsa_L_config;
	block_t tmp;

	if (arg == 0) {
		oaep_L_init();
		return 0;
	}

	err = copy_from_user((char *)&crypto_rsa_L_config, (char __user *)arg, sizeof(crypto_rsa_L_config));

	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)arg, err);
		return err;
	}

	mem_n = ce_malloc(CE_HEAP_MALLOC_MAX_SIZE);

	if (mem_n == NULL) {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	tmp = BLOCK_T_CONV(mem_n->ptr, crypto_rsa_L_config.len, EXT_MEM);

	switch (crypto_rsa_L_config.mode) {
		case OAEP_L_RESTORE_DEFAULT:
			oaep_L_init();
			break;

		case OAEP_L_GET:
			ret = oaep_L_get(&tmp);
			err = copy_to_user((char __user *)(crypto_rsa_L_config.addr), (char *)(tmp.addr), tmp.len);

			if (err < 0) {
				ce_free(mem_n);
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(crypto_rsa_L_config.addr), err);
				return err;
			}

			crypto_rsa_L_config.len = tmp.len;
			err = copy_to_user((char __user *)(arg), (char *)(&crypto_rsa_L_config), sizeof(crypto_rsa_L_config));

			if (err < 0) {
				ce_free(mem_n);
				pr_err("%s: copy_to_user (%p) failed (%d)\n", __func__, (char __user *)(arg), err);
				return err;
			}

			break;

		case OAEP_L_SET:
			err = copy_from_user((char *)tmp.addr, (char __user *)(crypto_rsa_L_config.addr), tmp.len);

			if (err < 0) {
				pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)arg + sizeof(uint32_t), err);
				ce_free(mem_n);
				return err;
			}

			ret = oaep_L_set(&tmp);
			break;

		default:
			pr_err("%s: Unhandled rsa_L_config mode: 0x%x in %s\n", __func__,
				   crypto_rsa_L_config.mode, __func__);
			ret = -EINVAL;
			ce_free(mem_n);
			return ret;
	}

	ce_free(mem_n);
	return ret;
}


