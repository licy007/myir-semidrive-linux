
/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#ifndef SD_RSA_H
#define SD_RSA_H

#include <linux/fs.h>
#include "ce.h"

#define MAX_OUTPUT_NUM                  5

typedef enum {
	OAEP_L_RESTORE_DEFAULT = 0,
	OAEP_L_GET,
	OAEP_L_SET,
} crypto_rsa_oaep_L_mode_t;


struct crypto_rsa_L_config_st {
	crypto_rsa_oaep_L_mode_t mode;
	uint32_t len;
	uint8_t *addr;
};


typedef enum {
	RSA_MODE_ENC = 0,
	RSA_MODE_DEC,
	RSA_MODE_CRT_DEC,
	RSA_MODE_SIG_GEN,
	RSA_MODE_SIG_VER,
	RSA_MODE_PRIV_KEY_GEN,
	RSA_MODE_CRT_KEY_GEN,
	RSA_MODE_KEY_GEN,
} rsa_mode_t;

struct crypto_rsa_key_st {
	uint8_t *n;       /* Modulus */
	uint8_t *e;       /* Public exponent */
	uint8_t *d;       /* Private exponent */
	uint8_t *p;
	uint8_t *q;
	uint8_t *dP;
	uint8_t *dQ;
	uint8_t *qInv;
	size_t n_len;
	size_t e_len;
	size_t d_len;
	size_t p_len;
	size_t q_len;
	size_t dP_len;
	size_t dQ_len;
	size_t qInv_len;
} __attribute__((packed));

struct crypto_rsa_instance_st {
	size_t keysize;
	struct crypto_rsa_key_st rsa_keypair;
	ce_addr_type_t addr_type;
	hash_alg_t rsa_hashType;
	rsa_pad_types_t rsa_pad_types;
	rsa_mode_t mode;
	const uint8_t *msg;
	size_t msg_len;
	const uint8_t *cipher;
	size_t cipher_len;
	const uint8_t *sig;
	size_t sig_len;
	uint32_t *ret;
} __attribute__((packed));

uint32_t sd_rsa_run(struct file *filp, unsigned long arg);
uint32_t ce_oaep_L_config(unsigned long arg);
#endif