/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/kernel.h>
#include "sx_rsa.h"
#include "sx_pke_conf.h"
#include "sx_hash.h"
#include "sx_errors.h"
#include "sx_rsa_pad.h"
#include "sx_dma.h"
#include "sx_prime.h"
#include "ce.h"

#define DEBUGMODE 0

static uint32_t __rsa_encrypt_blk(void *device,
								  block_t *input,
								  block_t *result,
								  block_t *n,
								  block_t *key,
								  bool be_param)
{
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t size = n->len;

	pke_set_command(device, BA414EP_OPTYPE_RSA_ENC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

#if DEBUGMODE
	pr_err("enter %s\n", __func__);
	pr_err("input is\n");
	hexdump8(input->addr, size);
	pr_err("n is\n");
	hexdump8(n->addr, size);
	pr_err("key is\n");
	hexdump8(key->addr, size);
#endif

	// Location 0 -> Modulus N
	status = mem2CERAM(device, n, size, BA414EP_MEMLOC_0, be_param, true);

	if (unlikely(status)) {
		pr_err("%s, %d, status : 0x%x\n", __func__, __LINE__, status);
		return status;
	}

	// Location 8 -> Public exponent
	status = mem2CERAM(device, key, size, BA414EP_MEMLOC_8, be_param, true);

	if (unlikely(status)) {
		pr_err("%s, %d, status : 0x%x\n", __func__, __LINE__, status);
		return status;
	}

	status = mem2CERAM(device, input, size, BA414EP_MEMLOC_5, 1, true);

	if (unlikely(status)) {
		pr_err("%s, %d, status : 0x%x\n", __func__, __LINE__, status);
		return status;
	}

	status = pke_start_wait_status(device);

	if (unlikely(status)) {
		pr_err("%s, %d, status : 0x%x\n", __func__, __LINE__, status);
		return status;
	}

#if AUTO_OUTPUT_BY_CE
#else
	// Location 4 -> Cipher C optional: pke_set_dst_param, copy result by manual mode
	status = CERAM2mem(device, result, size, BA414EP_MEMLOC_4, 1, true);

	if (unlikely(status)) {
		pr_err("%s, %d, status : 0x%x\n", __func__, __LINE__, status);
		return status;
	}

#endif
	//next code will clear ce-memory,It will increase security.But This will take about 30us.
	//Please make your own choice to modify CLEAR_CE_MEMORY_AFTER_CE_RUN.
	//Of course ,You could call ioctl(fd, SEMIDRIVE_CE_CLEAR_CE_MEMORY)
	//in user space.fd is the file description of semidrive-ce2.
	//And it will call ce_clear_memory().
#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

#if DEBUGMODE
	pr_err("after enc, result is\n");
	hexdump8(result->addr, result->len);
#endif

	return status;
}

static uint32_t __rsa_decrypt_blk(void *device,
								  block_t *input,
								  block_t *result,
								  block_t *n,
								  block_t *key,
								  bool be_param)
{
	uint32_t size = n->len, status = CRYPTOLIB_SUCCESS;

	pke_set_command(device, BA414EP_OPTYPE_RSA_DEC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

#if DEBUGMODE
	pr_err("enter %s\n", __func__);
	pr_err("input is\n");
	hexdump8(input->addr, size);
	pr_err("n is\n");
	hexdump8(n->addr, size);
	pr_err("key is\n");
	hexdump8(key->addr, size);
#endif

	status = mem2CERAM(device, n, size, BA414EP_MEMLOC_0, be_param, true);

	if (status) {
		pr_err("n addr: 0x%lx\n", (addr_t)n->addr);
		return status;
	}

	// Location 6 -> Private key
	status = mem2CERAM(device, key, size, BA414EP_MEMLOC_6, be_param, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	status = mem2CERAM(device, input, size, BA414EP_MEMLOC_4, 1, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	status |= pke_start_wait_status(device);

	if (status) {
		pr_err("src addr: 0x%lx\n", (addr_t)input->addr);
		return status;
	}

	status = CERAM2mem(device, result, n->len, BA414EP_MEMLOC_5, 1, true);

	if (unlikely(status)) {
		pr_err("%s, %d, status : 0x%x\n", __func__, __LINE__, status);
		return status;
	}

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

#if DEBUGMODE
	pr_err("after dec, result is\n");
	hexdump8(result->addr, result->len);
#endif

	return status;
}

uint32_t __rsa_x_encrypt_blk(void *device,
							 rsa_pad_types_t padding_type,
							 block_t *message,
							 block_t *n,
							 block_t *key,
							 block_t *result,
							 hash_alg_t hashType,
							 bool be_param,
							 rsa_key_types_t key_type)
{
	uint8_t *msg_padding;
	struct mem_node *mem_n;
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t size = 0;
	block_t block_temp;
	uint32_t (*ce_rsa_fun)(void *, block_t *, block_t *, block_t *, block_t *, bool) =
		key_type == RSA_PUB_KEY ? __rsa_encrypt_blk : __rsa_decrypt_blk;

	if (device == NULL || block_t_check_wrongful(message) || block_t_check_wrongful(n) ||
			block_t_check_wrongful(key) || block_t_check_wrongful(result)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	//Checks that the algoritm is valid for signature
	if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_OAEP &&
			padding_type != ESEC_RSA_PADDING_EME_PKCS) {
		pr_err("padding_type is %d\n", padding_type);
		return CRYPTOLIB_INVALID_PARAM;
	}

	size = n->len;
#if DEBUGMODE
	pr_err("enter %s\n", __func__);
	pr_err("padtype is %d\n", padding_type);
	pr_err("before padding, msg is \n");
	hexdump8(message->addr, message->len);
#endif
	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		msg_padding = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	switch (padding_type) {
		case ESEC_RSA_PADDING_NONE:
			status = rsa_pad_add_none(message->addr, message->len, size);
			msg_padding = message->addr;
			break;

		case ESEC_RSA_PADDING_OAEP:
			status = rsa_pad_eme_oaep_encode(device, msg_padding, size, message, message->len, NULL, 0, hashType, hashType);
			break;

		case ESEC_RSA_PADDING_EME_PKCS:
			status = rsa_pad_eme_pkcs_encode(device, msg_padding, size, message->addr, message->len, key_type);
			break;

		default:
			status = -1;
	}

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		ce_free(mem_n);
		return status;
	}

#if DEBUGMODE
	pr_err("after padding\n");
	pr_err("msg_padding is \n");
	hexdump8(msg_padding, size);
#endif

	block_temp = BLOCK_T_CONV(msg_padding, size, EXT_MEM);

	status = ce_rsa_fun(device, &block_temp, result, n, key, be_param);

	/* RSA encryption */
	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
	}

	ce_free(mem_n);

#if DEBUGMODE
	pr_err("after enc\n");
	pr_err("cipher is \n");
	hexdump8(result->addr, size);
#endif

	return status;
}


uint32_t __rsa_x_decrypt_blk(void *device,
							 rsa_pad_types_t padding_type,
							 block_t *cipher,
							 block_t *n,
							 block_t *key,
							 block_t *result,
							 uint32_t *msg_len,
							 hash_alg_t hashType,
							 bool be_param,
							 rsa_key_types_t key_type)
{
	uint32_t size = 0, status = CRYPTOLIB_SUCCESS;
	uint8_t *msg_padding;
	struct mem_node *mem_n;
	uint8_t *addr;
	uint32_t (*ce_rsa_fun)(void *, block_t *, block_t *, block_t *, block_t *, bool) =
		key_type == RSA_PUB_KEY ? __rsa_encrypt_blk : __rsa_decrypt_blk;

	if (device == NULL || block_t_check_wrongful(cipher) || block_t_check_wrongful(n) ||
			block_t_check_wrongful(key) || block_t_check_wrongful(result) ||
			msg_len == NULL) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	size = n->len;

	//Checks that the algoritm is valid for signature
	if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_OAEP &&
			padding_type != ESEC_RSA_PADDING_EME_PKCS) {
		pr_err("padding_type is %d\n", padding_type);
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		msg_padding = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

#if DEBUGMODE
	pr_err("enter %s\n", __func__);
	pr_err("padtype is %d\n", padding_type);
	pr_err("before dec, \n");
	pr_err("cipher is \n");
	hexdump8(cipher->addr, size);
#endif

	status = ce_rsa_fun(device, cipher, &BLOCK_T_CONV(msg_padding, n->len, EXT_MEM), n, key, be_param);

	if (status) {
		pr_err("src addr: 0x%lx\n", (addr_t)cipher->addr);
		ce_free(mem_n);
		return status;
	}

#if DEBUGMODE
	pr_err("after dec, \n");
	pr_err("msg is \n");
	hexdump8(msg_padding, size);
#endif

	switch (padding_type) {
		case ESEC_RSA_PADDING_NONE:
			addr = msg_padding;
			*msg_len = n->len;
			break;

		case ESEC_RSA_PADDING_OAEP:
			status |= rsa_pad_eme_oaep_decode(device, n->len, hashType, msg_padding, &addr, (size_t *)msg_len);
			break;

		case ESEC_RSA_PADDING_EME_PKCS:
			status |= rsa_pad_eme_pkcs_decode(n->len, msg_padding, &addr, (uint32_t *)msg_len, key_type);
			break;

		default:
			status = -1;
	}

	if (status) {
		ce_free(mem_n);
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	memcpy(result->addr, addr, *msg_len);

#if DEBUGMODE
	pr_err("after decode, \n");
	pr_err("msg is \n");
	hexdump8(result->addr, *msg_len);
#endif

	ce_free(mem_n);
	return status;
}


/* internal memory map:
 * location 0x0: Modulus:n, input
 * location 0x4: Ciphered text: C, output
 * location 0x5: Plain text: M, input
 * location 0x8: Public key: e, input
 */
uint32_t rsa_encrypt_blk(void *device,
						 rsa_pad_types_t padding_type,
						 block_t *message,
						 block_t *n,
						 block_t *public_expo,
						 block_t *result,
						 hash_alg_t hashType,
						 bool be_param)
{
	return __rsa_x_encrypt_blk(device, padding_type, message, n, public_expo, result, hashType, be_param, RSA_PUB_KEY);
}

#define BLOCK_S_CONST_ADDR             0x10000000
/* internal memory map:
 * location 0x0: Modulus:n, input
 * location 0x4: Ciphered text: C, input
 * location 0x5: Plain text: M, output
 * location 0x6: Private key: d, input
 *
 * internal memory map for crt decryption:
 * location 0x2: p, input
 * location 0x3: q: d, input
 * location 0x4: Cipher text: C, input
 * location 0x5: Plain text: M, output
 * location 0xA: dP =d mod(p-1), input
 * location 0xB: dQ = d mod(q-1), input
 * location 0xC: qInv = 1/q mod p, input
 */
uint32_t rsa_decrypt_blk(void *device,
						 rsa_pad_types_t padding_type,
						 block_t *cipher,
						 block_t *n,
						 block_t *private_key,
						 block_t *result,
						 uint32_t *msg_len,
						 hash_alg_t hashType,
						 bool be_param)
{
	return __rsa_x_decrypt_blk(device, padding_type, cipher, n, private_key, result, msg_len, hashType, be_param, RSA_PRIV_KEY);
}

uint32_t rsa_priv_encrypt_blk(void *device,
							  rsa_pad_types_t padding_type,
							  block_t *message,
							  block_t *n,
							  block_t *private_key,
							  block_t *result,
							  hash_alg_t hashType,
							  bool be_param)
{
	return __rsa_x_encrypt_blk(device, padding_type, message, n, private_key, result, hashType, be_param, RSA_PRIV_KEY);
}




uint32_t rsa_pub_decrypt_blk(void *device,
							 rsa_pad_types_t padding_type,
							 block_t *cipher,
							 block_t *n,
							 block_t *public_expo,
							 block_t *result,
							 uint32_t *msg_len,
							 hash_alg_t hashType,
							 bool be_param)
{
	return __rsa_x_decrypt_blk(device, padding_type, cipher, n, public_expo, result, msg_len, hashType, be_param, RSA_PUB_KEY);
}

//TODO If possible, try to refactor this function and decrypt for CRT
/* internal memory map for decryption:
  * internal memory map for crt decryption:
 * location 0x2: p, input
 * location 0x3: q: input
 * location 0x4: Cipher text: C, input
 * location 0x5: Plain text: M, output
 * location 0xA: dP =d mod(p-1), input
 * location 0xB: dQ = d mod(q-1), input
 * location 0xC: qInv = 1/q mod p, input
 *
 */
uint32_t rsa_crt_decrypt_blk(void *device,
							 rsa_pad_types_t padding_type,
							 block_t *cipher,
							 block_t *p,
							 block_t *q,
							 block_t *dP,
							 block_t *dQ,
							 block_t *qInv,
							 block_t *result,
							 uint32_t *msg_len,
							 hash_alg_t hashType)
{
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t size = 0;

	if (device == NULL || block_t_check_wrongful(cipher) || block_t_check_wrongful(p) ||
			block_t_check_wrongful(q) || block_t_check_wrongful(dP) || block_t_check_wrongful(dQ) ||
			block_t_check_wrongful(qInv) || block_t_check_wrongful(result) || msg_len == NULL) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	size = cipher->len;

	//Checks that the algoritm is valid for signature
	if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_OAEP &&
			padding_type != ESEC_RSA_PADDING_EME_PKCS) {
		pr_err("padding_type is %d\n", padding_type);
		return CRYPTOLIB_INVALID_PARAM;
	}

	pke_set_command(device, BA414EP_OPTYPE_RSA_CRT_DEC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = mem2CryptoRAM_rev(device, p, size, BA414EP_MEMLOC_2, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	status = mem2CryptoRAM_rev(device, q, size, BA414EP_MEMLOC_3, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	status = mem2CryptoRAM_rev(device, cipher, size, BA414EP_MEMLOC_4, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
	}

	status = mem2CryptoRAM_rev(device, dP, size, BA414EP_MEMLOC_10, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	status = mem2CryptoRAM_rev(device, dQ, size, BA414EP_MEMLOC_11, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	status = mem2CryptoRAM_rev(device, qInv, size, BA414EP_MEMLOC_12, true);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return status;
	}

	/* RSA encryption */
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
		return status;
	}

	CryptoRAM2mem_rev(device, result, size, BA414EP_MEMLOC_5, true);

	if (padding_type != ESEC_RSA_PADDING_NONE) {
		uint8_t *msg_padding;
		struct mem_node *mem_n;
		uint8_t *addr;
		mem_n = ce_malloc(RSA_MAX_SIZE);

		if (mem_n != NULL) {
			msg_padding = mem_n->ptr;
		}
		else {
			return CRYPTOLIB_PK_N_NOTVALID;
		}

		memcpy(msg_padding, result->addr, p->len);

		if (padding_type == ESEC_RSA_PADDING_OAEP) {
			status |= rsa_pad_eme_oaep_decode(device, p->len, hashType, msg_padding, &addr, (size_t *)msg_len);
		}
		else if (padding_type == ESEC_RSA_PADDING_EME_PKCS) {
			status |= rsa_pad_eme_pkcs_decode(p->len, msg_padding, &addr, (uint32_t *)msg_len, RSA_PUB_KEY);
		}

		memcpy(result->addr, addr, *msg_len);
		ce_free(mem_n);
	}
	else {
		*msg_len = p->len;
	}

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

	return status;
}


uint32_t rsa_signature_by_digest(void *device,
								 hash_alg_t sha_type,
								 rsa_pad_types_t padding_type,
								 block_t *digest,
								 block_t *result,
								 block_t *n,
								 block_t *private_key,
								 uint32_t salt_length,
								 bool be_param)
{
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t size = n->len;
	uint8_t *hash = digest->addr;
	struct mem_node *mem_n;
	uint8_t *digest_padding;
	block_t EM_block;
	int padError = 0;

	if (device == NULL || block_t_check_wrongful(digest) || block_t_check_wrongful(result) ||
			block_t_check_wrongful(n) || block_t_check_wrongful(private_key) ||
			(0 == hash_get_digest_size(sha_type) && padding_type != ESEC_RSA_PADDING_NONE)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_EMSA_PKCS &&
			padding_type != ESEC_RSA_PADDING_PSS) {
		return CRYPTOLIB_INVALID_PARAM;
	}
	else if (padding_type != ESEC_RSA_PADDING_NONE && digest->len != hash_get_digest_size(sha_type)) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		digest_padding = mem_n->ptr;
	}
	else {
		pr_err("%s, %d, ce_malloc is wrong: \n", __func__, __LINE__);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	if (padding_type == ESEC_RSA_PADDING_EMSA_PKCS) {
		padError = rsa_pad_emsa_pkcs_encode(device, n->len, sha_type, digest_padding, hash);
		EM_block.addr   = digest_padding;
		EM_block.len    = n->len;
		EM_block.addr_type  = EXT_MEM;
	}
	else if (padding_type == ESEC_RSA_PADDING_PSS) {
		padError = rsa_pad_emsa_pss_encode(device, n->len, sha_type, digest_padding, hash, n->addr[0],
										   salt_length);
		EM_block.addr   = digest_padding;
		EM_block.len    = n->len;
		EM_block.addr_type  = EXT_MEM;
	}
	else { //No padding
		rsa_pad_zeros(digest_padding, n->len, hash, digest->len);
		EM_block.addr   = digest_padding;
		EM_block.len    = n->len;
		EM_block.addr_type  = EXT_MEM;
	}

	if (padError) {
		pr_err("%s, %d, padError is: %d\n", __func__, __LINE__, padError);
		ce_free(mem_n);
		return padError;
	}

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_RSA_SIGN_GEN, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

	status = mem2CERAM(device, n, size, BA414EP_MEMLOC_0, be_param, true);

	if (status) {
		pr_err("n addr: 0x%lx\n", (addr_t)n->addr);
		ce_free(mem_n);
		return status;
	}

	private_key->len = n->len;
	status = mem2CERAM(device, private_key, size, BA414EP_MEMLOC_6, be_param, true);

	if (status) {
		pr_err("private_key addr: 0x%lx\n", (addr_t)private_key->addr);
		ce_free(mem_n);
		return status;
	}

	// Location 12 -> hash
	status = mem2CERAM(device, &EM_block, size, BA414EP_MEMLOC_12, be_param, true);

	if (status) {
		pr_err("EM_block addr: 0x%lx\n", (addr_t)EM_block.addr);
		ce_free(mem_n);
		return status;
	}

	// Start BA414EP
	status |= pke_start_wait_status(device);

	if (status) {
		pr_err("sig_gen pke status: 0x%x\n", status);
		ce_free(mem_n);
		return status;
	}

	// Fetch result optional: pke_set_dst_param, fetch result by manual mode
	status = CERAM2mem(device, result, result->len, BA414EP_MEMLOC_11, be_param, true);

	if (status) {
		pr_err("result addr: 0x%lx\n", (addr_t)result->addr);
		ce_free(mem_n);
		return status;
	}

	ce_free(mem_n);

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif
	return CRYPTOLIB_SUCCESS;

}

/* internal memory map for decryption:
 * location 0x0: Modulus:n, input
 * location 0x6: Private key: d, input
 * location 0xB: Signature: s, output
 * location 0x8: Hash of message: h, input
 *
 */
uint32_t rsa_signature_generate_blk(void *device,
									hash_alg_t sha_type,
									rsa_pad_types_t padding_type,
									block_t *message,
									block_t *result,
									block_t *n,
									block_t *private_key,
									uint32_t salt_length,
									bool be_param)
{
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t size = n->len;
	uint8_t *hash;
	struct mem_node *mem_n;
	uint8_t *msg_padding;
	struct mem_node *mem_n_1;
	block_t hash_block;
	block_t iv = BLOCK_T_CONV(NULL, 0, 0);
	block_t data_out;
	int padError = 0;

	if (device == NULL || block_t_check_wrongful(message) || block_t_check_wrongful(result) ||
			block_t_check_wrongful(n) || block_t_check_wrongful(private_key)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	//Checks that the algoritm is valid for signature
	if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_EMSA_PKCS &&
			padding_type != ESEC_RSA_PADDING_PSS) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n != NULL) {
		hash = mem_n->ptr;
		data_out = BLOCK_T_CONV(hash, MAX_DIGESTSIZE, EXT_MEM);
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	/*Calculate hash of the message*/
	status |= hash_blk(device, sha_type, &iv, message, &data_out);

	if (status) {
		ce_free(mem_n);
		return status;
	}

	mem_n_1 = ce_malloc(RSA_MAX_SIZE);

	if (mem_n_1 != NULL) {
		msg_padding = mem_n_1->ptr;
	}
	else {
		ce_free(mem_n);
		pr_err("%s, %d, ce_malloc is wrong: \n", __func__, __LINE__);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	if (padding_type == ESEC_RSA_PADDING_EMSA_PKCS) {
		padError = rsa_pad_emsa_pkcs_encode(device, n->len, sha_type, msg_padding, hash);
		hash_block.addr   = msg_padding;
		hash_block.len    = n->len;
		hash_block.addr_type  = EXT_MEM;
	}
	else if (padding_type == ESEC_RSA_PADDING_PSS) {
		padError = rsa_pad_emsa_pss_encode(device, n->len, sha_type, msg_padding, hash, n->addr[0],
										   salt_length);
		hash_block.addr   = msg_padding;
		hash_block.len    = n->len;
		hash_block.addr_type  = EXT_MEM;
	}
	else { //No padding
		rsa_pad_zeros(msg_padding, n->len, hash, hash_get_digest_size(sha_type));
		hash_block.addr   = msg_padding;
		hash_block.len    = n->len;
		hash_block.addr_type  = EXT_MEM;
	}

	if (padError) {
		pr_err("%s, %d, padError is: %d\n", __func__, __LINE__, padError);
		ce_free(mem_n);
		ce_free(mem_n_1);
		return padError;
	}

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_RSA_SIGN_GEN, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

	if (be_param) {
		// Location 0 -> N
		status = mem2CryptoRAM_rev(device, n, size, BA414EP_MEMLOC_0, true);

		if (status) {
			pr_err("n->addr addr: 0x%lx\n", (addr_t)n->addr);
			ce_free(mem_n);
			ce_free(mem_n_1);
			return status;
		}

		private_key->len = n->len;

		// Location 6 -> Private key
		status = mem2CryptoRAM_rev(device, private_key, size, BA414EP_MEMLOC_6, true);
	}
	else {
		// Location 0 -> N
		status = mem2CryptoRAM(device, n, size, BA414EP_MEMLOC_0, true);

		if (status) {
			pr_err("n->addr addr: 0x%lx\n", (addr_t)n->addr);
			ce_free(mem_n);
			ce_free(mem_n_1);
			return status;
		}

		private_key->len = n->len;

		// Location 6 -> Private key
		status = mem2CryptoRAM(device, private_key, size, BA414EP_MEMLOC_6, true);
	}

	if (status) {
		pr_err("private_key addr: 0x%lx\n", (addr_t)private_key->addr);
		ce_free(mem_n);
		ce_free(mem_n_1);
		return status;
	}

	// Location 12 -> hash
	status = mem2CryptoRAM_rev(device, &hash_block, size, BA414EP_MEMLOC_12, true);

	if (status) {
		pr_err("hash_block addr: 0x%lx\n", (addr_t)hash_block.addr);
		ce_free(mem_n);
		ce_free(mem_n_1);
		return status;
	}

	// Start BA414EP
	status |= pke_start_wait_status(device);

	if (status) {
		pr_err("sig_gen pke status: 0x%x\n", status);
		ce_free(mem_n);
		ce_free(mem_n_1);
		return status;
	}

	// Fetch result optional: pke_set_dst_param, fetch result by manual mode
	CryptoRAM2mem_rev(device, result, result->len, BA414EP_MEMLOC_11, true);
	ce_free(mem_n);
	ce_free(mem_n_1);

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif
	return CRYPTOLIB_SUCCESS;
}

uint32_t rsa_signature_verify_by_digest(void *device,
										hash_alg_t sha_type,
										rsa_pad_types_t padding_type,
										block_t *digest,
										block_t *n,
										block_t *public_expo,
										block_t *signature,
										uint32_t salt_length,
										bool be_param)
{
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t size = 0;
	uint8_t *hash = digest->addr;
	struct mem_node *mem_n;
	uint8_t *digest_padding;
	block_t iv;
	block_t EM_block;
	int padError = 0;

	if (device == NULL || block_t_check_wrongful(digest) || block_t_check_wrongful(n) ||
			block_t_check_wrongful(public_expo) || block_t_check_wrongful(signature) ||
			(0 == hash_get_digest_size(sha_type) && padding_type != ESEC_RSA_PADDING_NONE)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	size = n->len;

	//Checks that the algoritm is valid for signature
	if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_EMSA_PKCS &&
			padding_type != ESEC_RSA_PADDING_PSS) {
		return CRYPTOLIB_INVALID_PARAM;
	}
	else if (padding_type != ESEC_RSA_PADDING_NONE && digest->len != hash_get_digest_size(sha_type)) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		digest_padding = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	if (padding_type == ESEC_RSA_PADDING_EMSA_PKCS) {
		padError = rsa_pad_emsa_pkcs_encode(device, n->len, sha_type, digest_padding,  hash);
		EM_block = block_t_convert(digest_padding, n->len, 0);
	}
	else if (padding_type == ESEC_RSA_PADDING_NONE) { //No padding
		rsa_pad_zeros(digest_padding, n->len, hash, digest->len);
		EM_block = block_t_convert(digest_padding, n->len, EXT_MEM);
	}

	if (padError) {
		ce_free(mem_n);
		return padError;
	}

	// Set command to enable byte-swap
	if (padding_type != ESEC_RSA_PADDING_PSS) {
		pke_set_command(device, BA414EP_OPTYPE_RSA_SIGN_VERIF, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	}
	else {
		pke_set_command(device, BA414EP_OPTYPE_RSA_ENC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	}

	status = mem2CERAM(device, n, size, BA414EP_MEMLOC_0, be_param, true);

	if (status) {
		pr_err("n addr: 0x%lx\n", (addr_t)n->addr);
		ce_free(mem_n);
		return status;
	}

	status = mem2CERAM(device, public_expo, size, BA414EP_MEMLOC_8, be_param, true);

	if (status) {
		pr_err("public_expo addr: 0x%lx\n", (addr_t)public_expo->addr);
		ce_free(mem_n);
		return status;
	}

	if (padding_type != ESEC_RSA_PADDING_PSS) {
		// Location 11 -> Signature
		status = mem2CERAM(device, signature, signature->len, BA414EP_MEMLOC_11, be_param, true);

		if (status) {
			pr_err("signature addr: 0x%lx\n", (addr_t)signature->addr);
			ce_free(mem_n);
			return status;
		}

		// Location 12 -> Hash
		status = mem2CERAM(device, &EM_block, size, BA414EP_MEMLOC_12, be_param, true);

		if (status) {
			pr_err("EM_block addr: 0x%lx\n", (addr_t)EM_block.addr);
			ce_free(mem_n);
			return status;
		}
	}
	else { //PSS
		// Location 5 -> Signature
		status = mem2CERAM(device, signature, signature->len, BA414EP_MEMLOC_5, be_param, true);

		if (status) {
			pr_err("signature addr: 0x%lx\n", (addr_t)signature->addr);
			ce_free(mem_n);
			return status;
		}
	}


// Start BA414EP
	status = pke_start_wait_status(device);

	if (status) {
		ce_free(mem_n);
		return status;
	}

	if (padding_type == ESEC_RSA_PADDING_PSS) {
		// Fetch result by manual mode
		iv = BLOCK_T_CONV(digest_padding, n->len, 0);
		status = CERAM2mem(device, &iv, n->len, BA414EP_MEMLOC_4, be_param, true);

		if (status) {
			pr_err("digest_padding addr: 0x%lx\n", (addr_t)digest_padding);
			ce_free(mem_n);
			return status;
		}

		status = rsa_pad_emsa_pss_decode(device, n->len, sha_type, digest_padding, hash,
										 salt_length, n->addr[0]);
	}

	ce_free(mem_n);

	if (status) {
		return status;
	}

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

	return status;

}



/* internal memory map:
 * location 0x0: Modulus:n, input
 * location 0x8: Public key: e, input
 * location 0xB: Signature: s, input
 * location 0x8: Hash of message: h, input
 */
uint32_t rsa_signature_verify_blk(void *device,
								  hash_alg_t sha_type,
								  rsa_pad_types_t padding_type,
								  block_t *message,
								  block_t *n,
								  block_t *public_expo,
								  block_t *signature,
								  uint32_t salt_length,
								  bool be_param)
{
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t size = 0;
	uint8_t *hash;
	struct mem_node *mem_n;
	uint8_t *msg_padding;
	struct mem_node *mem_n_1;
	block_t iv;
	block_t hash_block;
	int padError = 0;

	if (device == NULL || block_t_check_wrongful(message) || block_t_check_wrongful(n) ||
			block_t_check_wrongful(public_expo) || block_t_check_wrongful(signature)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	size = n->len;

	//Checks that the algoritm is valid for signature
	if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_EMSA_PKCS &&
			padding_type != ESEC_RSA_PADDING_PSS) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n != NULL) {
		hash = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	iv = BLOCK_T_CONV(NULL, 0, 0);
	hash_block = BLOCK_T_CONV(hash, hash_get_digest_size(sha_type), EXT_MEM);
	status = hash_blk(device, sha_type, &iv, message, &hash_block);

	if (status) {
		ce_free(mem_n);
		return status;
	}

	mem_n_1 = ce_malloc(RSA_MAX_SIZE);

	if (mem_n_1 != NULL) {
		msg_padding = mem_n_1->ptr;
	}
	else {
		ce_free(mem_n);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	if (padding_type == ESEC_RSA_PADDING_EMSA_PKCS) {
		padError = rsa_pad_emsa_pkcs_encode(device, n->len, sha_type, msg_padding, hash_block.addr);
		hash_block = block_t_convert(msg_padding, n->len, 0);
	}
	else if (padding_type == ESEC_RSA_PADDING_NONE) { //No padding
		rsa_pad_zeros(msg_padding, n->len, hash_block.addr, hash_get_digest_size(sha_type));
		hash_block = block_t_convert(msg_padding, n->len, EXT_MEM);
	}

	if (padError) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		return padError;
	}

	// Set command to enable byte-swap
	if (padding_type != ESEC_RSA_PADDING_PSS) {
		pke_set_command(device, BA414EP_OPTYPE_RSA_SIGN_VERIF, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	}
	else {
		pke_set_command(device, BA414EP_OPTYPE_RSA_ENC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	}

	if (be_param) {
		// Location 0 -> Modulus N
		status = mem2CryptoRAM_rev(device, n, size, BA414EP_MEMLOC_0, true);

		if (status) {
			pr_err("n addr: 0x%lx\n", (addr_t)n->addr);
			ce_free(mem_n);
			ce_free(mem_n_1);
			return status;
		}

		// Location 8 -> Public exponent
		status = mem2CryptoRAM_rev(device, public_expo, size, BA414EP_MEMLOC_8, true);
	}
	else {
		// Location 0 -> Modulus N
		status = mem2CryptoRAM(device, n, size, BA414EP_MEMLOC_0, true);

		if (status) {
			pr_err("n addr: 0x%lx\n", (addr_t)n->addr);
			ce_free(mem_n);
			ce_free(mem_n_1);
			return status;
		}

		// Location 8 -> Public exponent
		status = mem2CryptoRAM(device, public_expo, size, BA414EP_MEMLOC_8, true);
	}

	if (status) {
		pr_err("public_expo addr: 0x%lx\n", (addr_t)public_expo->addr);
		ce_free(mem_n);
		ce_free(mem_n_1);
		return status;
	}

	if (padding_type != ESEC_RSA_PADDING_PSS) {
		// Location 11 -> Signature
		status = mem2CryptoRAM_rev(device, signature, signature->len, BA414EP_MEMLOC_11, true);

		if (status) {
			pr_err("signature addr: 0x%lx\n", (addr_t)signature->addr);
			ce_free(mem_n);
			ce_free(mem_n_1);
			return status;
		}

		// Location 12 -> Hash
		status = mem2CryptoRAM_rev(device, &hash_block, size, BA414EP_MEMLOC_12, true);

		if (status) {
			pr_err("hash_block addr: 0x%lx\n", (addr_t)hash_block.addr);
			ce_free(mem_n);
			ce_free(mem_n_1);
			return status;
		}
	}
	else { //PSS
		// Location 5 -> Signature
		status = mem2CryptoRAM_rev(device, signature, signature->len, BA414EP_MEMLOC_5, true);

		if (status) {
			pr_err("signature addr: 0x%lx\n", (addr_t)signature->addr);
			ce_free(mem_n);
			ce_free(mem_n_1);
			return status;
		}
	}

	// Start BA414EP
	status = pke_start_wait_status(device);

	if (status) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		return status;
	}

	if (padding_type == ESEC_RSA_PADDING_PSS) {
		// Fetch result by manual mode
		iv = BLOCK_T_CONV(msg_padding, n->len, 0);
		CryptoRAM2mem_rev(device, &iv, n->len, BA414EP_MEMLOC_4, true);
		status = rsa_pad_emsa_pss_decode(device, n->len, sha_type, msg_padding, hash_block.addr,
										 salt_length, n->addr[0]);
	}

	ce_free(mem_n);
	ce_free(mem_n_1);

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

	return status;
}

/* internal memory map:
 * location 0x0: Modulus:n, output
 * location 0x1: lcm(p-1, q-1), output
 * location 0x2: p, input
 * location 0x3: q, input
 * location 0x6: Private key: d, output
 * location 0x8: Public key: e, input
 */
uint32_t rsa_private_key_generate_blk(void *device,
									  block_t *p,
									  block_t *q,
									  block_t *public_expo,
									  block_t *n,
									  block_t *private_key,
									  uint32_t size,
									  uint32_t lambda)
{
	uint32_t status = CRYPTOLIB_SUCCESS;

	if (device == NULL || block_t_check_wrongful(p) || block_t_check_wrongful(q) ||
			block_t_check_wrongful(public_expo) || block_t_check_wrongful(n) ||
			block_t_check_wrongful(private_key)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_RSA_PK_GEN, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	//ce will write to this addr auto, do not use this .

	// Location 2 -> P
	status = mem2CryptoRAM_rev(device, p, size, BA414EP_MEMLOC_2, true);

	if (status) {
		pr_err("p addr: 0x%lx\n", (addr_t)p->addr);
		return status;
	}

	// Location 3 -> Q
	status = mem2CryptoRAM_rev(device, q, size, BA414EP_MEMLOC_3, true);

	if (status) {
		pr_err("q addr: 0x%lx\n", (addr_t)q->addr);
		return status;
	}

	// Location 8 -> Public Expo
	status = mem2CryptoRAM_rev(device, public_expo, size, BA414EP_MEMLOC_8, true);

	if (status) {
		pr_err("public_expo addr: 0x%lx\n", (addr_t)public_expo->addr);
		return status;
	}

	// invalidate_cache_block(n, device);
	// invalidate_cache_block(private_key, device);

	// Start BA414EP
	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}

	// Result (Lambda - location 1)
	if (lambda) {
		CryptoRAM2mem_rev(device, private_key, size, BA414EP_MEMLOC_1, true);

		if (!(private_key->addr_type & BLOCK_S_CONST_ADDR)) {
			private_key->addr += size;
		}
	}

	// Result (N - location 0) TODO: why not false & flush
	CryptoRAM2mem_rev(device, n, size, BA414EP_MEMLOC_0, true);
	//flush_cache((addr_t)n.addr, size);


	//Result (Private key - location 6), fetch result by manual mode
	CryptoRAM2mem_rev(device, private_key, size, BA414EP_MEMLOC_6, true);
	//flush_cache((addr_t)private_key.addr, size);

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

	return CRYPTOLIB_SUCCESS;
}

/* internal memory map:
 * location 0x2: p, input
 * location 0x3: q, input
 * location 0x6: Private key: d, input
 * location 0xA: dP =d mod(p-1), output
 * location 0xB: dQ = d mod(q-1), output
 * location 0xC: qInv = 1/q mod p, output
 */
uint32_t rsa_crt_key_generate_blk(void *device,
								  block_t *p,
								  block_t *q,
								  block_t *d,
								  block_t *dP,
								  block_t *dQ,
								  block_t *qInv,
								  uint32_t size)
{
	uint32_t status = CRYPTOLIB_SUCCESS;

	if (device == NULL || block_t_check_wrongful(p) || block_t_check_wrongful(q) ||
			block_t_check_wrongful(d) || block_t_check_wrongful(dP) ||
			block_t_check_wrongful(dQ) || block_t_check_wrongful(qInv)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_RSA_CRT_GEN, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	// Location 2 -> P
	status = mem2CryptoRAM_rev(device, p, size, BA414EP_MEMLOC_2, true);

	if (status) {
		pr_err("p addr: 0x%lx\n", (addr_t)p->addr);
		return status;
	}

	// Location 3 -> Q
	status = mem2CryptoRAM_rev(device, q, size, BA414EP_MEMLOC_3, true);

	if (status) {
		pr_err("q addr: 0x%lx\n", (addr_t)q->addr);
		return status;
	}

	// Location 6 -> D
	status = mem2CryptoRAM_rev(device, d, size, BA414EP_MEMLOC_6, true);

	if (status) {
		pr_err("d addr: 0x%lx\n", (addr_t)d->addr);
		return status;
	}

	// Start BA414EP
	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}


	// Result (DP - location 10)
	CryptoRAM2mem_rev(device, dP, size, BA414EP_MEMLOC_10, true);
	// Result (DQ - location 11)
	CryptoRAM2mem_rev(device, dQ, size, BA414EP_MEMLOC_11, true);

	// Result (INV - location 12)
	CryptoRAM2mem_rev(device, qInv, size, BA414EP_MEMLOC_12, true);

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

	return CRYPTOLIB_SUCCESS;
}

/**
 * @brief Full asymmetric key generate (e and size of n are inputs)
 * Take 2 random
 * Force the LSB to '1'
 * Compute n = rq * rp
 * Is n at the programmed size
 * If not drop it and regenerate 2 random
 * If OK converge to 2 primes (p, q)
 * Calculate (e is programmed)
 * n = p * q
 * phi(n) = (p-1) * (q-1)
 * d = 1/e mod phi(n)
 * The key pairs is [d, e] with size n

 * Typically, n is 1024, 2048, 4096 or 1280
 */

uint32_t rsa_key_generate_blk(void *device,
							  uint32_t nbr_of_bytes,            //keysize   input
							  block_t *e,                       //(e)       input
							  block_t *p,                       //p         output
							  block_t *q,                       //q         output
							  block_t *n,                       //n         output
							  block_t *d,                       //(d)       output
							  bool public,
							  bool lambda)
{
	uint32_t err = 0;
	uint32_t try_again = false;
	uint32_t wrong_time = 0;
	uint32_t status = CRYPTOLIB_SUCCESS;
	uint32_t run_time = 0;
	bool random1_prime = false;
	bool random2_prime = false;
	uint8_t *random1;
	uint8_t *random2;
	uint8_t *random3;
	uint8_t *random4;
	uint32_t primeFound = 0;
	block_t tmp_random;

	if (device == NULL || block_t_check_wrongful(e) || block_t_check_wrongful(p) ||
			block_t_check_wrongful(q) || block_t_check_wrongful(n) ||
			block_t_check_wrongful(d)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (nbr_of_bytes > RSA_MAX_SIZE) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	do {
		status = 0;
		try_again = false;

		if (random1_prime != true) {
			memset(p->addr, 0, nbr_of_bytes);
		}

		if (random2_prime != true) {
			memset(q->addr, 0, nbr_of_bytes);
		}

		if (random2_prime & random1_prime) {
			memset(q->addr, 0, nbr_of_bytes);
			memset(p->addr, 0, nbr_of_bytes);
			random1_prime = false;
			random2_prime = false;
		}

		memset(n->addr, 0, nbr_of_bytes);
		memset(d->addr, 0, nbr_of_bytes);
		random1 = p->addr;
		random2 = q->addr;
		random3 = n->addr;
		random4 = d->addr;
		primeFound = 0;

		// Find proper prime numbers
		while (!primeFound) {
			run_time ++ ;

			// 1. generate two random numbers
			if (!random1_prime) {
				if (trng_get_rand_by_fifo(device, random1 + nbr_of_bytes / 2, nbr_of_bytes / 2)) {
					pr_emerg("trng_get_rand_by_fifo _1 is wrong ");
					continue;
				}
			}

			if (!random2_prime) {
				if (trng_get_rand_by_fifo(device, random2 + nbr_of_bytes / 2, nbr_of_bytes / 2)) {
					pr_emerg("trng_get_rand_by_fifo _2 is wrong ");
					continue;
				}
			}

			// 2. force lsb to '1' and force msb to '1'
			if (!random1_prime) {
				*(random1 + nbr_of_bytes - 1) |= 0x1;
				*(random1 + nbr_of_bytes / 2) |= 0x1;
			}

			if (!random2_prime) {
				*(random2 + nbr_of_bytes - 1) |= 0x1;
				*(random2 + nbr_of_bytes / 2) |= 0x1;
			}

			// 3. calculate modulus result = r1 * r2
			status = multiplicate(device, p, q, n);

			if (status) {
				//    pr_err("%s, %d, status: %d\n", __func__, __LINE__, status);
				return status;
			}

			status = !((*random3) & 0x80);  // MSB == 1 (ensure right size in bits)

			if (!status) {
				// converge to prime numbers.
				if (!random1_prime) {
					tmp_random = block_t_convert(random1 + nbr_of_bytes / 2, nbr_of_bytes / 2, EXT_MEM);
					status = converge_to_prime(device,
											   &tmp_random, 2);

					if (!status) {
						random1_prime = true;
					}
				}

				if (!random2_prime) {
					tmp_random = block_t_convert(random2 + nbr_of_bytes / 2, nbr_of_bytes / 2, EXT_MEM);
					err = converge_to_prime(device,
											&tmp_random, 2);

					if (!err) {
						random2_prime = true;
					}

					status |= err;
				}

				if (!status) {
					err = multiplicate(device, p, q, n);

					if (err) {
						pr_err("%s, %d, result: %d\n", __func__, __LINE__, err);
						return err;
					}

					status = (!(random3[0] & 0x80));
				}
				else if (status != CRYPTOLIB_PK_NOTINVERTIBLE) {
					// pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
					wrong_time++;
					break;
				}
				else {
					continue;
				}

				primeFound = !status; //If no error, prime is found
			}
		}

		if (status) {

			try_again = true;

			if (wrong_time > 500) {
				try_again = false;
				break;
			}

			continue;
		}

		err = rsa_private_key_generate_blk(device, p, q, e, n, d,
										   nbr_of_bytes, 0);
	}
	while (err == CRYPTOLIB_PK_NOTINVERTIBLE || try_again);

	if (wrong_time > 500) {
		try_again = false;
		pr_err("rsa_key_generate_blk is err");
		pr_err("wrong_time is %d times", wrong_time);
		return status | err;
	}

#if CLEAR_CE_MEMORY_AFTER_CE_RUN
	pke_set_command(device, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status(device);

	if (status) {
		pr_err("%s, %d, pke_status is %d\n", __func__, __LINE__, status);
	}

#endif

	return err;
}
