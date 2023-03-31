/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#ifndef SD_ECDSA_H
#define SD_ECDSA_H

#include "sx_hash.h"
#include "ce.h"
#include <linux/fs.h>


typedef enum {
	ECDSA_MODE_SIG_GEN = 0,
	ECDSA_MODE_SIG_VER = 1,
} ecdsa_mode_t;

struct crypto_ec_key_st {
	uint8_t *pub_key;
	uint8_t *priv_key;
	uint32_t pub_key_len;
	uint32_t priv_key_len;
} __attribute__((packed));

struct crypto_ecdsa_instance_st {
	size_t  curve_nid;
	struct crypto_ec_key_st ec_keypair;
	ce_addr_type_t addr_type;
	hash_alg_t ecdsa_hashType;
	const uint8_t *msg;
	size_t msg_len;
	const uint8_t *sig;
	size_t sig_len;
	uint32_t *ret;
} __attribute__((packed));


uint32_t sd_ecdsa_run(struct file *filp, unsigned long arg, ecdsa_mode_t mode);

uint32_t sd_ecdsa_generate_signature(struct file *filp, unsigned long arg);
uint32_t sd_ecdsa_verify_signature(struct file *filp, unsigned long arg);

#endif