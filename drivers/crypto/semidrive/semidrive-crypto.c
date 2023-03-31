/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/rtnetlink.h>
#include <crypto/authenc.h>
#include <linux/uaccess.h> //put_user
#include <linux/syscalls.h> //sys_close

#include "ce.h"
#include "sx_sm2.h"
#include "cryptodev_int.h"
#include "crypto.h"
#include "sx_errors.h"
#include "ce_reg.h"
#include <crypto/cryptodev.h>
#include "sx_ecdsa.h"
#include "sd_ecdsa.h"
#include "sx_rsa.h"
#include "sd_rsa.h"



#define TRNG_CE2_VCE2_NUM       1
#define SEMIDRIVE_QUEUE_LENGTH  1

#define DEVCRYPTO_CRYPTO_TYPE           0x63 //'c' for devcrypto-linux framework
#define SEMIDRIVE_CRYPTO_ALG_SM2        0xaa
#define SEMIDRIVE_CRYPTO_ALG            0xab
#define SEMIDRIVE_CRYPTO_ALG_ECDSA      0xac
#define SEMIDRIVE_CRYPTO_CONFIG         0xaf


#define SEMIDRIVE_CRYPTO_SM2_INIT                   _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 1)
#define SEMIDRIVE_CRYPTO_SM2_VERIFY_NO_DIGEST       _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 1)
#define SEMIDRIVE_CRYPTO_SM2_VERIFY_DIGEST          _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 2)
#define SEMIDRIVE_CRYPTO_SM2_VERIFY_MSG             _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 3)
#define SEMIDRIVE_CRYPTO_SM2_SET_PUBKEY             _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 4)
#define SEMIDRIVE_CRYPTO_SM2_SIGN                   _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 4)


#define SEMIDRIVE_CRYPTO_ALG_RSA                    _IO(SEMIDRIVE_CRYPTO_ALG, 1)

#define SEMIDRIVE_CRYPTO_ALG_ECDSA_SIG_GEN          _IO(SEMIDRIVE_CRYPTO_ALG_ECDSA, 1)
#define SEMIDRIVE_CRYPTO_ALG_ECDSA_SIG_VER          _IO(SEMIDRIVE_CRYPTO_ALG_ECDSA, 2)

#define SEMIDRIVE_CRYPTO_GET_PAGE                   _IO(SEMIDRIVE_CRYPTO_CONFIG, 1)
#define SEMIDRIVE_CRYPTO_FREE_PAGE                  _IO(SEMIDRIVE_CRYPTO_CONFIG, 2)
#define SEMIDRIVE_CRYPTO_CLEAR_PAGE                 _IO(SEMIDRIVE_CRYPTO_CONFIG, 3)

#define SEMIDRIVE_CRYPTO_CLEAR_CE_MEMORY            _IO(SEMIDRIVE_CRYPTO_CONFIG, 7)
#define SEMIDRIVE_CRYPTO_OAEP_L_CONFIG              _IO(SEMIDRIVE_CRYPTO_CONFIG, 8)

#define SEMIDRIVE_CRYPTO_SM2_SET_TIME_STAMP _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 9)
#define SEMIDRIVE_CRYPTO_SM2_GET_TIME_STAMP _IO(SEMIDRIVE_CRYPTO_ALG_SM2, 0xa)

#define CRYPTOLIB_SUCCESS               0
#define CRYPTOLIB_CRYPTO_ERR            6


#define MAX_ARREY_NUM                   512

struct crypto_sm2_msg {
	const uint8_t *msg;
	size_t msg_len;
	const uint8_t *sig;
	size_t sig_len;
	const uint8_t *key;
	size_t key_len;
	uint32_t *ret;
} __attribute__((packed));



/*dB：3945208F 7B2144B1 3F36E38A C6D39F95 88939369 2860B51A 42FB81EF 4DF7C5B8*/
uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_prv_key[32] = "\x39\x45\x20\x8F\x7B\x21\x44\xB1\x3F\x36\xE3\x8A\xC6\xD3\x9F\x95\x88\x93\x93\x69\x28\x60\xB5\x1A\x42\xFB\x81\xEF\x4D\xF7\xC5\xB8";


/*xB：09F9DF31 1E5421A1 50DD7D16 1E4BC5C6 72179FAD 1833FC07 6BB08FF3 56F35020*/
/*yB：CCEA490C E26775A5 2DC6EA71 8CC1AA60 0AED05FB F35E084A 6632F607 2DA9AD13*/

uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_pub_key[64] = "\x09\xF9\xDF\x31\x1E\x54\x21\xA1\x50\xDD\x7D\x16\x1E\x4B\xC5\xC6\x72\x17\x9F\xAD\x18\x33\xFC\x07\x6B\xB0\x8F\xF3\x56\xF3\x50\x20"
		"\xCC\xEA\x49\x0C\xE2\x67\x75\xA5\x2D\xC6\xEA\x71\x8C\xC1\xAA\x60\x0A\xED\x05\xFB\xF3\x5E\x08\x4A\x66\x32\xF6\x07\x2D\xA9\xAD\x13";

/*message digest:6D65737361676520646967657374*/
uint8_t crypto_gb_sig_msg[14] = "\x6D\x65\x73\x73\x61\x67\x65\x20\x64\x69\x67\x65\x73\x74";
/*IDA GB/T1988: 31323334 35363738 31323334 35363738*/
uint8_t crypto_gb_id_test[16] = "\x31\x32\x33\x34\x35\x36\x37\x38\x31\x32\x33\x34\x35\x36\x37\x38";
/*
r：F5A03B06 48D2C463 0EEAC513 E1BB81A1 5944DA38 27D5B741 43AC7EAC EEE720B3
s：B1B6AA29 DF212FD8 763182BC 0D421CA1 BB9038FD 1F7F42D4 840B69C4 85BBC1AA*/

uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_ver_msg[64] = "\xF5\xA0\x3B\x06\x48\xD2\xC4\x63\x0E\xEA\xC5\x13\xE1\xBB\x81\xA1\x59\x44\xDA\x38\x27\xD5\xB7\x41\x43\xAC\x7E\xAC\xEE\xE7\x20\xB3"
		"\xB1\xB6\xAA\x29\xDF\x21\x2F\xD8\x76\x31\x82\xBC\x0D\x42\x1C\xA1\xBB\x90\x38\xFD\x1F\x7F\x42\xD4\x84\x0B\x69\xC4\x85\xBB\xC1\xAA";
uint8_t __attribute__((aligned(CACHE_LINE))) crypto_gb_m[46];

// uint8_t __attribute__((aligned(CACHE_LINE))) verify_msg_buff[MAX_ARREY_NUM] = {0};
// uint8_t __attribute__((aligned(CACHE_LINE))) verify_sig_buff[MAX_ARREY_NUM] = {0};
// uint8_t __attribute__((aligned(CACHE_LINE))) verify_key_buff[MAX_ARREY_NUM] = {0};




/* Prepare session for future use. */
/* Default (pre-allocated) and maximum size of the job queue.
 * These are free, pending and done items all together. */
#define DEF_COP_RINGSIZE 16
#define MAX_COP_RINGSIZE 64

static int crypto_copy_hash_state(struct fcrypt *fcr, uint32_t dst_ses, uint32_t src_ses)
{
	struct csession *cses_dst = NULL, *cses_src = NULL;
	int ret = 0;

	cses_src = crypto_get_session_by_sid(fcr, src_ses);

	if (unlikely(!cses_src)) {
		pr_err("invalid src_ses session ID=0x%08X", src_ses);
		ret = -EINVAL;
		goto lab_out;
	}

	cses_dst = crypto_get_session_by_sid(fcr, dst_ses);

	if (unlikely(!cses_dst)) {
		pr_err("invalid dst_ses session ID=0x%08X", dst_ses);
		ret = -EINVAL;
		goto lab_unlock_cses_src;
	}

	ret = cryptodev_hash_copy(&cses_dst->hdata, &cses_src->hdata);

	if (unlikely(ret < 0)) {
		pr_err("cryptodev_hash_copy err, dst_ses ID=0x%08X, src_ses ID=0x%08X ",
			   dst_ses, src_ses);
	}

lab_unlock_cses_dst:
	crypto_put_session(cses_dst);
lab_unlock_cses_src:
	crypto_put_session(cses_src);
lab_out:
	return ret;
}

static int crypto_create_session(struct fcrypt *fcr, struct session_op *sop)
{
	struct csession *ses_new = NULL, *ses_ptr;
	int ret = 0;
	const char *alg_name = NULL;
	const char *hash_name = NULL;
	int hmac_mode = 1, stream = 0, aead = 0;

	/*
	 * With composite aead ciphers, only ckey is used and it can cover all the
	 * structure space; otherwise both keys may be used simultaneously but they
	 * are confined to their spaces
	 */
	struct {
		uint8_t ckey[CRYPTO_CIPHER_MAX_KEY_LEN];
		uint8_t mkey[CRYPTO_HMAC_MAX_KEY_LEN];
		/* padding space for aead keys */
		uint8_t pad[RTA_SPACE(sizeof(struct crypto_authenc_key_param))];
	} keys;

	/* Does the request make sense? */
	if (unlikely(!sop->cipher && !sop->mac)) {
		pr_err("Both 'cipher' and 'mac' unset.");
		return -EINVAL;
	}

	switch (sop->cipher) {
		case 0:
			break;

		case CRYPTO_AES_CBC:
			alg_name = "cbc(aes)";
			break;

		case CRYPTO_AES_ECB:
			alg_name = "ecb(aes)";
			break;

		case CRYPTO_AES_XTS:
			alg_name = "xts(aes)";
			break;

		case CRYPTO_AES_CTR:
			alg_name = "ctr(aes)";
			stream = 1;
			break;

		case CRYPTO_AES_GCM:
			alg_name = "gcm(aes)";
			stream = 1;
			aead = 1;
			break;

		case CRYPTO_NULL:
			alg_name = "ecb(cipher_null)";
			stream = 1;
			break;

		default:
			pr_err("bad cipher: %d", sop->cipher);
			return -EINVAL;
	}

	switch (sop->mac) {
		case 0:
			break;

		/*case CRYPTO_MD5_HMAC:
		    hash_name = "hmac(md5)";
		    break;
		*/
		case CRYPTO_SHA1_HMAC:
			hash_name = "hmac(sha1)";
			break;

		case CRYPTO_SHA2_224_HMAC:
			hash_name = "hmac(sha224)";
			break;

		case CRYPTO_SHA2_256_HMAC:
			hash_name = "hmac(sha256)";
			break;

		case CRYPTO_SHA2_384_HMAC:
			hash_name = "hmac(sha384)";
			break;

		case CRYPTO_SHA2_512_HMAC:
			hash_name = "hmac(sha512)";
			break;

		/* non-hmac cases */
		/*case CRYPTO_MD5:
		    hash_name = "md5";
		    hmac_mode = 0;
		    break;
		*/
		case CRYPTO_SHA1:
			hash_name = "sha1";
			hmac_mode = 0;
			break;

		case CRYPTO_SHA2_224:
			hash_name = "sha224";
			hmac_mode = 0;
			break;

		case CRYPTO_SHA2_256:
			hash_name = "sha256";
			hmac_mode = 0;
			break;

		case CRYPTO_SHA2_384:
			hash_name = "sha384";
			hmac_mode = 0;
			break;

		case CRYPTO_SHA2_512:
			hash_name = "sha512";
			hmac_mode = 0;
			break;

		case CRYPTO_SM3:
			hash_name = "sm3";
			hmac_mode = 0;
			break;

		default:
			pr_err("bad mac: %d", sop->mac);
			return -EINVAL;
	}

	/* Create a session and put it to the list. Zeroing the structure helps
	 * also with a single exit point in case of errors */
	ses_new = kzalloc(sizeof(*ses_new), GFP_KERNEL);

	if (!ses_new) {
		return -ENOMEM;
	}

	/* Set-up crypto transform. */
	if (alg_name) {
		unsigned int keylen;
		ret = cryptodev_get_cipher_keylen(&keylen, sop, aead);

		if (unlikely(ret < 0)) {
			pr_err("Setting key failed for %s-%zu.",
				   alg_name, (size_t)sop->keylen * 8);
			goto session_error;
		}

		ret = cryptodev_get_cipher_key(keys.ckey, sop, aead);

		if (unlikely(ret < 0)) {
			goto session_error;
		}

		ret = cryptodev_cipher_init(&ses_new->cdata, alg_name, keys.ckey,
									keylen, stream, aead);

		if (ret < 0) {
			pr_err("Failed to load cipher for %s", alg_name);
			ret = -EINVAL;
			goto session_error;
		}
	}

	if (hash_name && aead == 0) {
		if (unlikely(sop->mackeylen > CRYPTO_HMAC_MAX_KEY_LEN)) {
			pr_err("Setting key failed for %s-%zu.",
				   hash_name, (size_t)sop->mackeylen * 8);
			ret = -EINVAL;
			goto session_error;
		}

		if (sop->mackey && unlikely(copy_from_user(keys.mkey, sop->mackey,
									sop->mackeylen))) {
			ret = -EFAULT;
			goto session_error;
		}

		ret = cryptodev_hash_init(&ses_new->hdata, hash_name, hmac_mode,
								  keys.mkey, sop->mackeylen);

		if (ret != 0) {
			pr_err("Failed to load hash for %s", hash_name);
			ret = -EINVAL;
			goto session_error;
		}

		ret = cryptodev_hash_reset(&ses_new->hdata);

		if (ret != 0) {
			goto session_error;
		}
	}

	ses_new->alignmask = max(ses_new->cdata.alignmask,
							 ses_new->hdata.alignmask);
	pr_info("got alignmask %d", ses_new->alignmask);

	ses_new->array_size = DEFAULT_PREALLOC_PAGES;
	pr_info("preallocating for %d user pages", ses_new->array_size);
	ses_new->pages = kzalloc(ses_new->array_size *
							 sizeof(struct page *), GFP_KERNEL);
	ses_new->sg = kzalloc(ses_new->array_size *
						  sizeof(struct scatterlist), GFP_KERNEL);

	if (ses_new->sg == NULL || ses_new->pages == NULL) {
		pr_err("Memory error");
		ret = -ENOMEM;
		goto session_error;
	}

	/* put the new session to the list */
	get_random_bytes(&ses_new->sid, sizeof(ses_new->sid));
	mutex_init(&ses_new->sem);
	mutex_lock(&fcr->sem);

restart:
	list_for_each_entry(ses_ptr, &fcr->list, entry) {
		/* Check for duplicate SID */
		if (unlikely(ses_new->sid == ses_ptr->sid)) {
			get_random_bytes(&ses_new->sid, sizeof(ses_new->sid));
			/* Unless we have a broken RNG this
			   shouldn't loop forever... ;-) */
			goto restart;
		}
	}

	list_add(&ses_new->entry, &fcr->list);
	mutex_unlock(&fcr->sem);

	/* Fill in some values for the user. */
	sop->ses = ses_new->sid;

	return 0;

	/* We count on ses_new to be initialized with zeroes
	 * Since hdata and cdata are embedded within ses_new, it follows that
	 * hdata->init and cdata->init are either zero or one as they have been
	 * initialized or not */
session_error:
	cryptodev_hash_deinit(&ses_new->hdata);
	cryptodev_cipher_deinit(&ses_new->cdata);
	kfree(ses_new->sg);
	kfree(ses_new->pages);
	kfree(ses_new);
	return ret;
}

/* Everything that needs to be done when removing a session. */
static inline void
crypto_destroy_session(struct csession *ses_ptr)
{
	if (!mutex_trylock(&ses_ptr->sem)) {
		pr_err("Waiting for semaphore of sid=0x%08X", ses_ptr->sid);
		mutex_lock(&ses_ptr->sem);
	}

	cryptodev_cipher_deinit(&ses_ptr->cdata);
	cryptodev_hash_deinit(&ses_ptr->hdata);

	kfree(ses_ptr->pages);
	kfree(ses_ptr->sg);
	mutex_unlock(&ses_ptr->sem);
	mutex_destroy(&ses_ptr->sem);
	kfree(ses_ptr);
}

/* Look up a session by ID and remove. */
static int
crypto_finish_session(struct fcrypt *fcr, uint32_t sid)
{
	struct csession *tmp, *ses_ptr;
	struct list_head *head;
	int ret = 0;

	mutex_lock(&fcr->sem);
	head = &fcr->list;
	list_for_each_entry_safe(ses_ptr, tmp, head, entry) {
		if (ses_ptr->sid == sid) {
			list_del(&ses_ptr->entry);
			crypto_destroy_session(ses_ptr);
			break;
		}
	}

	if (unlikely(!ses_ptr)) {
		pr_err("Session with sid=0x%08X not found!", sid);
		ret = -ENOENT;
	}

	mutex_unlock(&fcr->sem);

	return ret;
}

/* Remove all sessions when closing the file */
static int
crypto_finish_all_sessions(struct fcrypt *fcr)
{
	struct csession *tmp, *ses_ptr;
	struct list_head *head;

	mutex_lock(&fcr->sem);

	head = &fcr->list;
	list_for_each_entry_safe(ses_ptr, tmp, head, entry) {
		list_del(&ses_ptr->entry);
		crypto_destroy_session(ses_ptr);
	}
	mutex_unlock(&fcr->sem);

	return 0;
}

static int
clonefd(struct file *filp)
{
	int ret;
	ret = get_unused_fd_flags(0);

	if (ret >= 0) {
		get_file(filp);
		fd_install(ret, filp);
	}

	return ret;
}

static void cryptask_routine(struct crypto_dev *crypto, struct work_struct *work)
{
	struct crypt_priv *pcr = container_of(work, struct crypt_priv, cryptask);
	struct todo_list_item *item;
	LIST_HEAD(tmp);

	/* fetch all pending jobs into the temporary list */
	mutex_lock(&pcr->todo.lock);
	list_cut_position(&tmp, &pcr->todo.list, pcr->todo.list.prev);
	mutex_unlock(&pcr->todo.lock);

	/* handle each job locklessly */
	list_for_each_entry(item, &tmp, __hook) {
		item->result = crypto_run(crypto, &pcr->fcrypt, &item->kcop);

		if (unlikely(item->result)) {
			pr_err("crypto_run() failed: %d", item->result);
		}
	}

	/* push all handled jobs to the done list at once */
	mutex_lock(&pcr->done.lock);
	list_splice_tail(&tmp, &pcr->done.list);
	mutex_unlock(&pcr->done.lock);
}

/* Look up session by session ID. The returned session is locked. */
/* if you got a cession,you should call crypto_put_session when you finish */
struct csession *
crypto_get_session_by_sid(struct fcrypt *fcr, uint32_t sid)
{
	struct csession *ses_ptr, *retval = NULL;

	if (unlikely(fcr == NULL)) {
		return NULL;
	}

	mutex_lock(&fcr->sem);
	list_for_each_entry(ses_ptr, &fcr->list, entry) {
		if (ses_ptr->sid == sid) {
			mutex_lock(&ses_ptr->sem);
			retval = ses_ptr;
			break;
		}
	}
	mutex_unlock(&fcr->sem);

	return retval;
}

/* this function has to be called from process context */
static int fill_kcop_from_cop(struct kernel_crypt_op *kcop, struct fcrypt *fcr)
{
	struct crypt_op *cop = &kcop->cop;
	struct csession *ses_ptr;
	int rc;

	/* this also enters ses_ptr->sem */
	ses_ptr = crypto_get_session_by_sid(fcr, cop->ses);

	if (unlikely(!ses_ptr)) {
		pr_err("invalid session ID=0x%08X", cop->ses);
		return -EINVAL;
	}

	kcop->ivlen = cop->iv ? ses_ptr->cdata.ivsize : 0;
	kcop->digestsize = 0; /* will be updated during operation */

	crypto_put_session(ses_ptr);

	kcop->task = current;
	kcop->mm = current->mm;

	if (cop->iv) {
		rc = copy_from_user(kcop->iv, cop->iv, kcop->ivlen);

		if (unlikely(rc)) {
			pr_err("error copying IV (%d bytes), copy_from_user returned %d for address %p",
				   kcop->ivlen, rc, cop->iv);
			return -EFAULT;
		}
	}

	return 0;
}

/* this function has to be called from process context */
static int fill_cop_from_kcop(struct kernel_crypt_op *kcop, struct fcrypt *fcr)
{
	int ret;

	if (kcop->digestsize) {
		ret = copy_to_user(kcop->cop.mac,
						   kcop->hash_output, kcop->digestsize);

		if (unlikely(ret)) {
			return -EFAULT;
		}
	}

	if (kcop->ivlen && kcop->cop.flags & COP_FLAG_WRITE_IV) {
		ret = copy_to_user(kcop->cop.iv,
						   kcop->iv, kcop->ivlen);

		if (unlikely(ret)) {
			return -EFAULT;
		}
	}

	return 0;
}

static int kcop_from_user(struct kernel_crypt_op *kcop,
						  struct fcrypt *fcr, void __user *arg)
{
	if (unlikely(copy_from_user(&kcop->cop, arg, sizeof(kcop->cop)))) {
		return -EFAULT;
	}

	return fill_kcop_from_cop(kcop, fcr);
}

static int kcop_to_user(struct kernel_crypt_op *kcop,
						struct fcrypt *fcr, void __user *arg)
{
	int ret;

	ret = fill_cop_from_kcop(kcop, fcr);

	if (unlikely(ret)) {
		pr_err("Error in fill_cop_from_kcop");
		return ret;
	}

	if (unlikely(copy_to_user(arg, &kcop->cop, sizeof(kcop->cop)))) {
		pr_err("Cannot copy to userspace");
		return -EFAULT;
	}

	return 0;
}


uint32_t sd_sm2_integration(struct file *filp, unsigned long arg, unsigned int cmd)
{
	uint32_t ret = -ENOTTY;
	int err;
	struct crypto_sm2_msg sm2_msg;
	int time_stamp;
	struct crypto_dev *crypto;
	uint8_t i = 0;
	block_t message;
	block_t key;
	block_t signature;
	/* UNIX - not MSDOS         */
	struct mem_node *mem_n[3];
	uint8_t *verify_msg_buff = NULL;
	uint8_t *verify_sig_buff = NULL;
	uint8_t *verify_key_buff = NULL;

	for (i = 0; i < 3 ; i ++) {
		mem_n[i] = ce_malloc(MAX_DIGESTSIZE);

		if (unlikely(mem_n[i] == NULL)) {
			while (i) {
				ce_free(mem_n[--i]);
			}

			pr_err("first ce_malloc is wrong ");
			return CRYPTOLIB_PK_N_NOTVALID;
		}
	}

	verify_msg_buff = mem_n[0]->ptr;
	verify_sig_buff = mem_n[1]->ptr;
	verify_key_buff = mem_n[2]->ptr;
	// memset (verify_msg_buff, 0, MAX_ARREY_NUM);
	// memset (verify_sig_buff, 0, MAX_ARREY_NUM);
	// memset (verify_key_buff, 0, MAX_ARREY_NUM);
	crypto = to_crypto_dev(filp->private_data);

	if (cmd == SEMIDRIVE_CRYPTO_SM2_SET_TIME_STAMP) {
		err = copy_from_user((char *)&time_stamp, (char __user *)arg, sizeof(int));

		if (err < 0) {
			pr_err("%s: copy_from_user (%p) failed (%d)\n", __func__, (char __user *)arg, err);
			ce_inheap_free();
			return err;
		}

		ce_inheap_free();
		of_set_sys_cnt_ce(time_stamp);
		return ret;
	}

	if (cmd == SEMIDRIVE_CRYPTO_SM2_GET_TIME_STAMP) {
		ret = of_get_sys_cnt_ce();
		err = copy_to_user((char __user *)arg, &ret, sizeof(uint32_t));

		if (err < 0) {

			ce_inheap_free();
			return err;
		}

		ce_inheap_free();
		return ret;
	}

	err = copy_from_user((char *)&sm2_msg, (char __user *)arg, sizeof(struct crypto_sm2_msg));

	if (err < 0) {
		ce_inheap_free();
		return err;
	}

	err = copy_from_user((char *)&verify_msg_buff, (char __user *)(sm2_msg.msg), sm2_msg.msg_len);

	if (err < 0) {
		ce_inheap_free();
		return err;
	}

	err = copy_from_user((char *)&verify_sig_buff, (char __user *)(sm2_msg.sig), sm2_msg.sig_len);

	if (err < 0) {
		ce_inheap_free();
		return err;
	}

	if (sm2_msg.key_len > 0) {
		err = copy_from_user((char *)&verify_key_buff, (char __user *)(sm2_msg.key), sm2_msg.key_len);

		if (err < 0) {
			ce_inheap_free();
			return err;
		}
	}

	//curve =   get_curve_value_by_nid(sm2_msg.curve_nid);
	switch (cmd) {
		case SEMIDRIVE_CRYPTO_SM2_VERIFY_NO_DIGEST:
			message.addr = verify_msg_buff;
			message.addr_type = EXT_MEM;
			message.len = sm2_msg.msg_len;
			key.addr = verify_key_buff;
			key.addr_type = EXT_MEM;
			key.len = sm2_msg.key_len;
			signature.addr = verify_sig_buff;
			signature.addr_type = EXT_MEM;
			signature.len = sm2_msg.sig_len;
			//sm2_generate_digest must use sx_ecc_sm2_curve_p256 , if use sx_ecc_sm2_curve_p256_rev will error
			ret = sm2_verify_signature((void *)crypto, &sx_ecc_sm2_curve_p256, &message, &key, &signature, ALG_SM3);
			break;

		case SEMIDRIVE_CRYPTO_SM2_VERIFY_DIGEST:
			message.addr = verify_msg_buff;
			message.addr_type = EXT_MEM;
			message.len = sm2_msg.msg_len;
			key.addr = verify_key_buff;
			key.addr_type = EXT_MEM;
			key.len = sm2_msg.key_len;
			signature.addr = verify_sig_buff;
			signature.addr_type = EXT_MEM;
			signature.len = sm2_msg.sig_len;
			ret = sm2_verify_signature_digest_update((void *)crypto, &sx_ecc_sm2_curve_p256_rev, 0, &message, &key, sm2_msg.key_len, &signature);
			break;

		case SEMIDRIVE_CRYPTO_SM2_SET_PUBKEY:
			key.addr = verify_key_buff;
			key.addr_type = EXT_MEM;
			key.len = sm2_msg.key_len;
			ret = sm2_verify_update_pubkey((void *)crypto, &key, sx_ecc_sm2_curve_p256_rev.bytesize);
			break;

		case SEMIDRIVE_CRYPTO_SM2_VERIFY_MSG:
			sm2_compute_id_digest((void *)crypto, crypto_gb_id_test, 16, crypto_gb_m, 32, verify_key_buff, 64);

			memcpy(&crypto_gb_m[32], verify_msg_buff, sm2_msg.msg_len);

			//printf_binary("z msg", crypto_gb_m, 46);
			message.addr = crypto_gb_m;
			message.addr_type = EXT_MEM;
			message.len = sm2_msg.msg_len + 32;
			key.addr = verify_key_buff;
			key.addr_type = EXT_MEM;
			key.len = sm2_msg.key_len;
			signature.addr = verify_sig_buff;
			signature.addr_type = EXT_MEM;
			signature.len = sm2_msg.sig_len;
			ret = sm2_generate_signature_update((void *)crypto, &sx_ecc_sm2_curve_p256_rev, &message, &key, &signature, ALG_SM3);
			break;

		default:
			pr_warn("%s: Unhandled ioctl cmd: 0x%x\n", __func__, cmd);
			ret = -EINVAL;
	}

	err = copy_to_user((char __user *)(sm2_msg.ret), &ret, sizeof(uint32_t));

	if (err < 0) {
		ce_inheap_free();
		return err;
	}

	for (i = 0; i < 3 ; i ++) {
		ce_free(mem_n[i]);
	}

	return ret;
}






static long crypto_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;
	struct crypto_dev *crypto = to_crypto_dev(filp->private_data);
	int __user *p = (int __user *)arg;
	int fd;
	struct session_op sop;
	uint32_t ses;
	struct cphash_op cphop;
	struct kernel_crypt_op kcop;
	struct crypt_asym asym;
	struct fcrypt *fcr;
	struct crypt_priv *pcr = crypto->pcr;
	fcr = &pcr->fcrypt;
	ret = -ENOTTY;

	switch (_IOC_TYPE(cmd)) {
		case SEMIDRIVE_CRYPTO_ALG_SM2:

			sd_sm2_integration(filp, arg, cmd);
			break;

		case SEMIDRIVE_CRYPTO_ALG:

			switch (cmd) {
				case SEMIDRIVE_CRYPTO_ALG_RSA:

					ret = sd_rsa_run(filp, arg);
					break;

				default:
					pr_err("%s: Unhandled ioctl cmd: 0x%x\n", __func__, cmd);
					ret = -EINVAL;
			}

			break;

		case SEMIDRIVE_CRYPTO_ALG_ECDSA:
			switch (cmd) {
				case SEMIDRIVE_CRYPTO_ALG_ECDSA_SIG_GEN:
					ret = sd_ecdsa_generate_signature(filp, arg);
					break;

				case SEMIDRIVE_CRYPTO_ALG_ECDSA_SIG_VER:
					ret = sd_ecdsa_verify_signature(filp, arg);
					break;

				default :
					pr_warn("%s: Unhandled ioctl cmd: 0x%x\n", __func__, cmd);
			}

			break;

		case SEMIDRIVE_CRYPTO_CONFIG:
			switch (cmd) {
				case SEMIDRIVE_CRYPTO_GET_PAGE:

					ret = ce_inheap_init();
					break;

				case SEMIDRIVE_CRYPTO_FREE_PAGE:
					ce_inheap_free();
					break;

				case SEMIDRIVE_CRYPTO_CLEAR_PAGE:
					ce_inheap_clear();
					break;

				case SEMIDRIVE_CRYPTO_CLEAR_CE_MEMORY:
					ce_clear_memory(filp);
					break;

				case SEMIDRIVE_CRYPTO_OAEP_L_CONFIG:
					ret = ce_oaep_L_config(arg);
					break;

				default:
					pr_warn("%s: Unhandled ioctl cmd: 0x%x\n", __func__, cmd);
					ret = -EINVAL;
			}

			break;

		case DEVCRYPTO_CRYPTO_TYPE:
			switch (cmd) {
				case CRIOGET:
					fd = clonefd(filp);
					ret = put_user(fd, p);

					if (unlikely(ret)) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0))
						sys_close(fd);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
						ksys_close(fd);
#else
						close_fd(fd);
#endif
						return ret;
					}

					return ret;

				case CIOCGSESSION:
					if (unlikely(copy_from_user(&sop, \
												(void __user *)arg, sizeof(sop)))) {
						return -EFAULT;
					}

					ret = crypto_create_session(fcr, &sop);

					if (unlikely(ret)) {
						return ret;
					}

					ret = copy_to_user(
							  (void __user *)arg, &sop, sizeof(sop));

					if (unlikely(ret)) {
						crypto_finish_session(fcr, sop.ses);
						pr_err("CIOCG Line %d, ret: %ld\n", __LINE__, ret);
						return -EFAULT;
					}

					return ret;

				case CIOCFSESSION:
					ret = get_user(ses, (uint32_t __user *)arg);

					if (unlikely(ret)) {
						return ret;
					}

					ret = crypto_finish_session(fcr, ses);
					return ret;

				case CIOCGSESSINFO:
					/*
					    if (unlikely(copy_from_user(&siop, (void __user *)arg, sizeof(siop))))
					        return -EFAULT;

					    ret = get_session_info(fcr, &siop);
					    if (unlikely(ret))
					        return ret;
					    return copy_to_user(arg, &siop, sizeof(siop));
					*/
					return 1;

				case CIOCCPHASH:

					if (unlikely(copy_from_user(&cphop, (void __user *)arg, sizeof(cphop)))) {
						return -EFAULT;
					}

					return crypto_copy_hash_state(fcr, cphop.dst_ses, cphop.src_ses);

				case CIOCCRYPT:
					ret = kcop_from_user(&kcop, fcr, (void __user *)arg);

					if (unlikely(ret)) {
						pr_err("Error copying from user");
						return ret;
					}

					ret = crypto_run(crypto, fcr, &kcop);

					if (unlikely(ret)) {
						pr_err("Error in crypto_run");
						return ret;
					}

					ret = kcop_to_user(&kcop, fcr, (void __user *)arg);
					break;

				case CIOCAUTHCRYPT:
					/*
					    if (unlikely(ret = kcaop_from_user(&kcaop, fcr, (void __user *)arg))) {
					        pr_err("Error copying from user");
					        return ret;
					    }

					    ret = crypto_auth_run(fcr, &kcaop);
					    if (unlikely(ret)) {
					        dpr_err("Error in crypto_auth_run");
					        return ret;
					    }

					    return kcaop_to_user(&kcaop, fcr, (void __user *)arg);
					*/
					return 1;

				case CIOCASYMFEAT:
					ses = CRF_EC_SM2;

					if (crypto_has_alg("pkc(rsa)", 0, 0)) {
						pr_info("enable pkc(rsa)\n");
						ses = CRF_MOD_EXP_CRT | CRF_MOD_EXP;
					}
					else {
						pr_err("crypto_has_alg ret: %ld\n", ret);
					}

					if (crypto_has_alg("pkc(dh)", 0, 0)) {
						ses |= CRF_DH_COMPUTE_KEY;
					}

					return put_user(ses, p);

				case CIOCKEY:
					if (copy_from_user(&asym, (void *)arg, sizeof(asym))) {
						pr_err("%s(CIOCKEY) - bad copy\n", __FUNCTION__);
						ret = EFAULT;
						return ret;
					}

					ret = cryptodev_pke(crypto, &asym);

					if (unlikely(ret)) {
						pr_err("Error in cryptodev_pke\n");
						return ret;
					}

					if (copy_to_user((void *)arg, &asym, sizeof(asym))) {
						pr_err("%s(CIOCGKEY) - bad return copy\n", __FUNCTION__);
						ret = EFAULT;
					}

					break;
			}

			break;

		default:
			pr_warn("%s: Unhandled ioctl cmd: 0x%x\n", __func__, cmd);
			ret = -EINVAL;
	}

	return ret;
}

static ssize_t crypto_read(struct file *filp, char __user *userbuf, size_t len,
						   loff_t *f_pos)
{
	return 0;
}

static ssize_t crypto_write(struct file *filp, const char __user *userbuf,
							size_t len, loff_t *f_pos)
{

	int rc = 0;

	return rc;
}
__attribute__((unused)) static int crypto_ip_test(void *device)
{
	int ret;
	block_t message;
	block_t key;
	block_t signature;

	pr_info("crypto_ip_test enter ");

	sm2_compute_id_digest(device, crypto_gb_id_test, 16, crypto_gb_m, 32, crypto_gb_pub_key, 64);

	memcpy(&crypto_gb_m[32], crypto_gb_sig_msg, 14);

	//printf_binary("z msg", crypto_gb_m, 46);
	message.addr = (uint8_t *)&crypto_gb_m;
	message.addr_type = EXT_MEM;
	message.len = 46;
	key.addr = (uint8_t *)&crypto_gb_pub_key;
	key.addr_type = EXT_MEM;
	key.len = 64;
	signature.addr = (uint8_t *)&crypto_gb_ver_msg;
	signature.addr_type = EXT_MEM;
	signature.len = 64;
	ret = sm2_verify_signature(device, &sx_ecc_sm2_curve_p256, &message, &key, &signature, ALG_SM3);
	pr_info("crypto_ip_test crypto_verify_signature ret =%d ", ret);
	return 0;
}

static int crypto_open(struct inode *inode, struct file *filp)
{
	struct crypto_dev *crypto = to_crypto_dev(filp->private_data);
	struct todo_list_item *tmp, *tmp_next;
	struct crypt_priv *pcr;
	int i, ret = 0;

	mutex_lock(&crypto->mutex_lock);
	crypto->open_cnt++;

	if (1 != crypto->open_cnt) {
		goto lab_open_finish;
	}

	pr_info("crypto_open enter ");
	sm2_load_curve(crypto, &sx_ecc_sm2_curve_p256_rev, BA414EP_BIGEND);
	pcr = kzalloc(sizeof(*pcr), GFP_KERNEL);

	if (!pcr) {
		ret = -ENOMEM;
		goto lab_open_finish;
	}

	crypto->pcr = pcr;
	mutex_init(&pcr->fcrypt.sem);
	mutex_init(&pcr->free.lock);
	mutex_init(&pcr->todo.lock);
	mutex_init(&pcr->done.lock);

	INIT_LIST_HEAD(&pcr->fcrypt.list);
	INIT_LIST_HEAD(&pcr->free.list);
	INIT_LIST_HEAD(&pcr->todo.list);
	INIT_LIST_HEAD(&pcr->done.list);

	//INIT_WORK(&pcr->cryptask, cryptask_routine);
	cryptask_routine(crypto, &pcr->cryptask);
	init_waitqueue_head(&pcr->user_waiter);

	for (i = 0; i < DEF_COP_RINGSIZE; i++) {
		tmp = kzalloc(sizeof(struct todo_list_item), GFP_KERNEL);

		if (!tmp) {
			goto err_ringalloc;
		}

		pcr->itemcount++;

		list_add(&tmp->__hook, &pcr->free.list);
	}

lab_open_finish:
	mutex_unlock(&crypto->mutex_lock);
	return ret;
	/* In case of errors, free any memory allocated so far */
err_ringalloc:

	list_for_each_entry_safe(tmp, tmp_next, &pcr->free.list, __hook) {
		list_del(&tmp->__hook);
		kfree(tmp);
	}

	mutex_destroy(&pcr->done.lock);
	mutex_destroy(&pcr->todo.lock);
	mutex_destroy(&pcr->free.lock);
	mutex_destroy(&pcr->fcrypt.sem);
	kfree(pcr);
	crypto->pcr = NULL;
	mutex_unlock(&crypto->mutex_lock);
	return -ENOMEM;
}

static int crypto_release(struct inode *inode, struct file *filp)
{
	struct crypto_dev *crypto = to_crypto_dev(filp->private_data);
	struct crypt_priv *pcr = crypto->pcr;
	struct todo_list_item *item, *item_safe;
	int items_freed = 0;

	mutex_lock(&crypto->mutex_lock);
	crypto->open_cnt--;

	if (0 != crypto->open_cnt) {
		mutex_unlock(&crypto->mutex_lock);
		return 0;
	}

	mutex_unlock(&crypto->mutex_lock);
	pr_info("crypto_release enter ");

	if (!pcr) {
		return 0;
	}

	list_splice_tail(&pcr->todo.list, &pcr->free.list);
	list_splice_tail(&pcr->done.list, &pcr->free.list);

	list_for_each_entry_safe(item, item_safe, &pcr->free.list, __hook) {
		list_del(&item->__hook);
		kfree(item);
		items_freed++;
	}

	crypto_finish_all_sessions(&pcr->fcrypt);

	mutex_destroy(&pcr->done.lock);
	mutex_destroy(&pcr->todo.lock);
	mutex_destroy(&pcr->free.lock);
	mutex_destroy(&pcr->fcrypt.sem);

	kfree(pcr);
	crypto->pcr = NULL;

	return 0;
}
static const struct semidrive_alg_ops *sd_alg_ops[] = {
	&ablkcipher_ops,
	&ahash_ops,
	&akcipher_ops,
};

__attribute__((unused)) static void semidrive_unregister_algs(struct crypto_dev *sdce)
{
	const struct semidrive_alg_ops *ops;
	int i;

	for (i = 0; i < ARRAY_SIZE(sd_alg_ops); i++) {
		ops = sd_alg_ops[i];
		ops->unregister_algs(sdce);
	}
}

static int semidrive_register_algs(struct crypto_dev *sdce)
{
	const struct semidrive_alg_ops *ops;
	int i, ret = -ENODEV;

	for (i = 0; i < ARRAY_SIZE(sd_alg_ops); i++) {
		ops = sd_alg_ops[i];
		ret = ops->register_algs(sdce);

		if (ret) {
			break;
		}
	}

	return ret;
}

static int semidrive_handle_request(struct crypto_async_request *async_req)
{
	int ret = -EINVAL, i;
	const struct semidrive_alg_ops *ops;
	u32 type = crypto_tfm_alg_type(async_req->tfm);

	for (i = 0; i < ARRAY_SIZE(sd_alg_ops); i++) {
		ops = sd_alg_ops[i];

		if (type != ops->type) {
			continue;
		}

		ret = ops->async_req_handle(async_req);
		break;
	}

	return ret;
}

static int semidrive_handle_queue(struct crypto_dev *sdce,
								  struct crypto_async_request *req)
{
	struct crypto_async_request *async_req, *backlog;
	unsigned long flags;
	int ret = 0, err;

	spin_lock_irqsave(&sdce->lock, flags);

	if (req) {
		ret = crypto_enqueue_request(&sdce->queue, req);
	}

	/* busy, do not dequeue request */
	if (sdce->req) {
		spin_unlock_irqrestore(&sdce->lock, flags);
		return ret;
	}

	backlog = crypto_get_backlog(&sdce->queue);
	async_req = crypto_dequeue_request(&sdce->queue);

	if (async_req) {
		sdce->req = async_req;
	}

	spin_unlock_irqrestore(&sdce->lock, flags);

	if (!async_req) {
		return ret;
	}

	if (backlog) {
		spin_lock_bh(&sdce->lock);
		backlog->complete(backlog, -EINPROGRESS);
		spin_unlock_bh(&sdce->lock);
	}

	err = semidrive_handle_request(async_req);

	if (err) {
		sdce->result = err;
		tasklet_schedule(&sdce->done_tasklet);
	}

	return ret;
}

static void semidrive_tasklet_req_done(unsigned long data)
{
	struct crypto_dev *sdce = (struct crypto_dev *)data;
	struct crypto_async_request *req;
	unsigned long flags;

	spin_lock_irqsave(&sdce->lock, flags);
	req = sdce->req;
	sdce->req = NULL;
	spin_unlock_irqrestore(&sdce->lock, flags);

	if (req) {
		req->complete(req, sdce->result);
	}

	semidrive_handle_queue(sdce, NULL);
}

static int semidrive_async_request_enqueue(struct crypto_dev *sdce,
		struct crypto_async_request *req)
{
	return semidrive_handle_queue(sdce, req);
}

static void semidrive_async_request_done(struct crypto_dev *sdce, int ret)
{
	sdce->result = ret;
	tasklet_schedule(&sdce->done_tasklet);
}

static const struct file_operations crypto_fops = {
	.open       = crypto_open,
	.release    = crypto_release,
	.unlocked_ioctl = crypto_ioctl,
	.read   = crypto_read,
	.write  = crypto_write,
	.owner      = THIS_MODULE,
};

static irqreturn_t crypto_irq_handler(int irq, void *data)
{
	struct crypto_dev *crypto = data;
	int irq_value;

	irq_value = readl((crypto->base + REG_INTSTAT_CE_(crypto->ce_id)));

	//pr_info("crypto_irq_handler irq_value:%d\n", irq_value);

	writel(0x1f, (crypto->base + REG_INTCLR_CE_(crypto->ce_id)));

	if (irq_value == 0x1) {
		crypto->dma_condition = 1;
		wake_up_interruptible(&crypto->cedmawaitq);
	}

	if (irq_value == 0x8) {
		//pr_info("crypto_irq_handler wakeup hash %p\n",sm2);
		crypto->hash_condition = 1;
		wake_up_interruptible(&crypto->cehashwaitq);
	}

	if (irq_value == 0x10) {
		crypto->pke_condition = 1;
		//wake_up_interruptible(&sm2->cepkewaitq);
	}

	return IRQ_HANDLED;
}

void *hash_op_buf = NULL;
void *cipher_op_input_buf = NULL;
void *cipher_op_output_buf = NULL;

static int semidrive_crypto_probe(struct platform_device *pdev)
{
	struct crypto_dev *crypto_inst;
	struct miscdevice *crypto_fn;
	int ret;
	int irq;
	struct semidrive_ce_device *ce_device;

	pr_info("semidrive_crypto_probe enter");

	if (&pdev->dev) {
		pr_err("pdev->dev not null\n");
	}

	if (pdev->dev.dma_ops) {
		pr_err("pdev->dev.dma_ops\n");
	}

	ce_device = (struct semidrive_ce_device *)platform_get_drvdata(pdev);

	if (!ce_device) {
		return -ENOMEM;
	}

	crypto_inst = devm_kzalloc(&pdev->dev, sizeof(*crypto_inst), GFP_KERNEL);

	if (!crypto_inst) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, crypto_inst);

	crypto_inst->base = ce_device->base;

	if (IS_ERR(crypto_inst->base)) {
		return PTR_ERR(crypto_inst->base);
	}

	crypto_inst->sram_base = ce_device->sram_base;

	if (IS_ERR(crypto_inst->sram_base)) {
		return PTR_ERR(crypto_inst->sram_base);
	}

	crypto_inst->ce_id = ce_device->ce_id;
	crypto_inst->dev = &pdev->dev;

	init_waitqueue_head(&crypto_inst->cehashwaitq);
	init_waitqueue_head(&crypto_inst->cedmawaitq);
	init_waitqueue_head(&crypto_inst->cepkewaitq);

	irq = ce_device->irq;

	if (irq < 0) {
		dev_err(&pdev->dev, "Cannot get IRQ resource\n");
		return irq;
	}

	ret = devm_request_threaded_irq(&pdev->dev, irq, crypto_irq_handler,
									NULL, IRQF_ONESHOT,
									dev_name(&pdev->dev), crypto_inst);

	writel(0x1f, (crypto_inst->base + REG_INTCLR_CE_(crypto_inst->ce_id)));
	//writel(0xe, (crypto_inst->base +REG_INTEN_CE_(ce_id))); //disable dma/pke intrrupt
	writel(0x0, (crypto_inst->base + REG_INTEN_CE_(crypto_inst->ce_id))); //disable all intrrupt

	crypto_fn = &crypto_inst->miscdev;
	sprintf(crypto_inst->name_buff, "semidrive-ce%d", crypto_inst->ce_id);

	crypto_fn->minor = MISC_DYNAMIC_MINOR;
	crypto_fn->name = crypto_inst->name_buff;
	crypto_fn->fops = &crypto_fops;

	// mutex_init(&(crypto_inst->lock));

	ret = misc_register(crypto_fn);

	if (ret) {
		dev_err(&pdev->dev, "failed to register ce\n");
		return ret;
	}

	spin_lock_init(&crypto_inst->lock);
	tasklet_init(&crypto_inst->done_tasklet, semidrive_tasklet_req_done,
				 (unsigned long)crypto_inst);
	crypto_init_queue(&crypto_inst->queue, SEMIDRIVE_QUEUE_LENGTH);

	crypto_inst->async_req_enqueue = semidrive_async_request_enqueue;
	crypto_inst->async_req_done = semidrive_async_request_done;

	ret = semidrive_register_algs(crypto_inst);

	if (ret) {
		return ret;
	}

	ce_oaep_L_config(0);

	//malloc memory for hash/aes operation
	hash_op_buf = (void *)__get_free_pages(GFP_KERNEL, SDCE_ORDER);

	if (!hash_op_buf) {
		return -ENOMEM;
	}

	cipher_op_input_buf = (void *)__get_free_pages(GFP_KERNEL, SDCE_ORDER);

	if (!cipher_op_input_buf) {
		free_pages((unsigned long)hash_op_buf, SDCE_ORDER);
		return -ENOMEM;
	}

	cipher_op_output_buf = (void *)__get_free_pages(GFP_KERNEL, SDCE_ORDER);

	if (!cipher_op_output_buf) {
		free_pages((unsigned long)hash_op_buf, SDCE_ORDER);
		free_pages((unsigned long)cipher_op_input_buf, SDCE_ORDER);
		return -ENOMEM;
	}

	spin_lock_init(&crypto_inst->hash_lock);
	spin_lock_init(&crypto_inst->cipher_lock);
	crypto_inst->open_cnt = 0;
	mutex_init(&crypto_inst->mutex_lock);
	return 0;
}

static const struct of_device_id semidrive_crypto_match[] = {
	{ .compatible = "semidrive,ceunuse" },
	{ }
};

MODULE_DEVICE_TABLE(of, semidrive_crypto_match);

static struct platform_driver semidrive_crypto_driver = {
	.probe      = semidrive_crypto_probe,
	.driver     = {
		.name   = "semidrive-ce",
		.of_match_table = of_match_ptr(semidrive_crypto_match),
	},
};

module_platform_driver(semidrive_crypto_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
MODULE_DESCRIPTION("semidrive ce driver");
