/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <string.h>
#include <stdbool.h>

#include "sx_pke_conf.h"
#include "sx_hash.h"
#include "sx_trng.h"
#include "sx_errors.h"
#include "sx_sm2.h"
#include "sx_dma.h"
#include "sx_eddsa.h"

#define DOM4_ED448_STR "SigEd448"

/* Curve Ed25519 */
static const uint8_t ecc_ed25519_params[ED25519_KEY_SIZE * 6] = {
	/* q = */
	0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xED,
	/* l = */
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x14, 0xDE, 0xF9, 0xDE, 0xA2, 0xF7, 0x9C, 0xD6,
	0x58, 0x12, 0x63, 0x1A, 0x5C, 0xF5, 0xD3, 0xED,
	/* Bx = */
	0x21, 0x69, 0x36, 0xD3, 0xCD, 0x6E, 0x53, 0xFE,
	0xC0, 0xA4, 0xE2, 0x31, 0xFD, 0xD6, 0xDC, 0x5C,
	0x69, 0x2C, 0xC7, 0x60, 0x95, 0x25, 0xA7, 0xB2,
	0xC9, 0x56, 0x2D, 0x60, 0x8F, 0x25, 0xD5, 0x1A,
	/* By = */
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x58,
	/* d = */
	0x52, 0x03, 0x6C, 0xEE, 0x2B, 0x6F, 0xFE, 0x73,
	0x8C, 0xC7, 0x40, 0x79, 0x77, 0x79, 0xE8, 0x98,
	0x00, 0x70, 0x0A, 0x4D, 0x41, 0x41, 0xD8, 0xAB,
	0x75, 0xEB, 0x4D, 0xCA, 0x13, 0x59, 0x78, 0xA3,
	/*  I = */
	0x2B, 0x83, 0x24, 0x80, 0x4F, 0xC1, 0xDF, 0x0B,
	0x2B, 0x4D, 0x00, 0x99, 0x3D, 0xFB, 0xD7, 0xA7,
	0x2F, 0x43, 0x18, 0x06, 0xAD, 0x2F, 0xE4, 0x78,
	0xC4, 0xEE, 0x1B, 0x27, 0x4A, 0x0E, 0xA0, 0xB0
};

/* Curve Ed448 */
static const uint8_t ecc_ed448_params[ED448_KEY_SIZE * 5] = {
	/* q = */
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff,
	/* l = */
	0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x7c, 0xca, 0x23,
	0xe9, 0xc4, 0x4e, 0xdb, 0x49, 0xae, 0xd6, 0x36,
	0x90, 0x21, 0x6c, 0xc2, 0x72, 0x8d, 0xc5, 0x8f,
	0x55, 0x23, 0x78, 0xc2, 0x92, 0xab, 0x58, 0x44,
	0xf3,
	/* Bx = */
	0x00, 0x4f, 0x19, 0x70, 0xc6, 0x6b, 0xed, 0x0d,
	0xed, 0x22, 0x1d, 0x15, 0xa6, 0x22, 0xbf, 0x36,
	0xda, 0x9e, 0x14, 0x65, 0x70, 0x47, 0x0f, 0x17,
	0x67, 0xea, 0x6d, 0xe3, 0x24, 0xa3, 0xd3, 0xa4,
	0x64, 0x12, 0xae, 0x1a, 0xf7, 0x2a, 0xb6, 0x65,
	0x11, 0x43, 0x3b, 0x80, 0xe1, 0x8b, 0x00, 0x93,
	0x8e, 0x26, 0x26, 0xa8, 0x2b, 0xc7, 0x0c, 0xc0,
	0x5e,
	/* By = */
	0x00, 0x69, 0x3f, 0x46, 0x71, 0x6e, 0xb6, 0xbc,
	0x24, 0x88, 0x76, 0x20, 0x37, 0x56, 0xc9, 0xc7,
	0x62, 0x4b, 0xea, 0x73, 0x73, 0x6c, 0xa3, 0x98,
	0x40, 0x87, 0x78, 0x9c, 0x1e, 0x05, 0xa0, 0xc2,
	0xd7, 0x3a, 0xd3, 0xff, 0x1c, 0xe6, 0x7c, 0x39,
	0xc4, 0xfd, 0xbd, 0x13, 0x2c, 0x4e, 0xd7, 0xc8,
	0xad, 0x98, 0x08, 0x79, 0x5b, 0xf2, 0x30, 0xfa,
	0x14,
	/* d = */
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x67,
	0x56
};

static const uint8_t *ecc_eddsa_params[] = {
	ecc_ed25519_params,
	ecc_ed448_params
};

static const size_t ecc_eddsa_param_size[] = {
	sizeof(ecc_ed25519_params),
	sizeof(ecc_ed448_params)
};

static const size_t eddsa_key_size[] = {
	ED25519_KEY_SIZE,
	ED448_KEY_SIZE
};

static const uint32_t eddsa_hw_flags[] = {
	BA414EP_SELCUR_ACCEL_25519,
	BA414EP_SELCUR_NO_ACCELERATOR | (1 << BA414EP_CMD_EDWARDS_LSB)
};

/**
 * @brief Load curve parameters into pk engine
 * @param[in] vce_id   ce index
 * @param[in] curve select the curve for which to load the parameters
 * @return CRYPTOLIB_SUCCESS
 */
static uint32_t eddsa_load_parameters(void *device, eddsa_curve_t curve)
{
	pke_load_curve(device,
				   BLOCK_T_CONV(ecc_eddsa_params[curve], ecc_eddsa_param_size[curve], EXT_MEM),
				   eddsa_key_size[curve],
				   BA414EP_LITTLEEND,
				   1);

	return CRYPTOLIB_SUCCESS;
}

/**
 * @brief Hash and clamp the secret key
 * @param[in] curve select the curve for which to load the parameters
 * @param[in] sk pointer to secret key
 * @param[out] result pointer to the result
 * @param[in] result_size the size of \p buffer
 * @return CRYPTOLIB_SUCCESS
 */
static uint32_t eddsa_hash_and_clamp(void *device,
									 eddsa_curve_t curve,
									 block_t *sk,
									 uint8_t *result,
									 const size_t result_size)
{
	uint32_t status = 0;

	size_t key_size = eddsa_key_size[curve];

	/* Hash the secret key into buffer */
	if (curve == SX_ED25519) {
		status = hash_blk(ALG_SHA512, device, &BLOCK_T_CONV(NULL, 0, 0), sk, &BLOCK_T_CONV(result, result_size, EXT_MEM));

		if (status) {
			dprintf(CRITICAL, "eddsa_hash_and_clamp hash fail.\n");
			return status;
		}

		/* Clamp the LSB */
		result[key_size - 1] &= ~0x80; // Mask bit 255
		result[key_size - 1] |=  0x40; // Set bit 254
		result[0]  &= ~0x07; // Mask bits 2 to 0
	}
	else {
		status = hash_blk(ALG_SHA256, device, &BLOCK_T_CONV(NULL, 0, 0), sk, &BLOCK_T_CONV(result, ED448_SIGN_SIZE, EXT_MEM));

		if (status) {
			dprintf(CRITICAL, "eddsa_hash_and_clamp hash fail.\n");
			return status;
		}

		result[0] &= ~(0x03);
		result[key_size - 1] = 0x00;
		result[key_size - 2] |= 0x80;
	}

	return status;
}

/**
 * @brief Use point coordinates compression for encoding
 * @param[in] size number of bytes needed to represent the x and y coordinates
 * @param[in] x pointer to coordinate x
 * @param[in, out] y pointer to coordinate y - encoded point output
 */
static void eddsa_encode_point(uint32_t size, const uint8_t *x, uint8_t *y)
{
	// Mask bit MSb
	y[size - 1] &= 0x7F;

	// Use LSb of x as MSb
	y[size - 1] |= (x[0] & 1) << 7;
}

/**
 * @brief decode an encoded point
 * @param[in] size number of bytes needed to represent an coordinate
 * @param[in, out] p pointer to the encoded point - y coordinate output
 * @return sign (lsb of x coordinate = x_0)
 */
static uint32_t eddsa_decode_point(uint32_t size, uint8_t *p)
{
	/* Get lsb of x for encoded point */
	uint32_t sign = ((p[size - 1]) >> 7) & 0x01; // Get MSb (sign)

	/* Get y coordinates for encoded point */
	p[size - 1] &= 0x7F; //Mask the MSb (sign bit)

	return sign;
}

uint32_t eddsa_generate_pubkey(
	uint32_t *device,
	eddsa_curve_t curve,
	block_t *sk,
	block_t *pk)
{
	uint32_t status;
	uint8_t *buffer;
	struct mem_node *mem_n;

	if (curve != SX_ED25519 && curve != SX_ED448) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	size_t key_size = eddsa_key_size[curve];

	if (pk->len != key_size || sk->len != (2 * key_size)) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(2 * EDDSA_MAX_KEY_SIZE);

	if (mem_n != NULL) {
		buffer = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	// Hash and clamp the secret key
	status = eddsa_hash_and_clamp(device, curve, sk, buffer, 2 * key_size);

	if (status) {
		ce_free(mem_n);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	/* Clear the MSB */
	//memset(buffer + key_size, 0, key_size);

	/*  Set operands for EdDSA point multiplication */
	point2CryptoRAM_rev(device, &BLOCK_T_CONV(buffer, key_size * 2, EXT_MEM),
						key_size, BA414EP_MEMLOC_8);

	// Set command to disable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_EDDSA_POINT_MULT, key_size,
					BA414EP_LITTLEEND, eddsa_hw_flags[curve]);

	// Load parameters
	eddsa_load_parameters(device, curve);

	/* EDDSA Point Multiplication */
	status = pke_start_wait_status(device);

	if (status) {
		ce_free(mem_n);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	CryptoRAM2point_rev(device, &BLOCK_T_CONV(buffer, key_size * 2, EXT_MEM),
						key_size, BA414EP_MEMLOC_10);

	/* Encode point  */
	eddsa_encode_point(key_size, buffer, buffer + key_size);

	/* copy result to pk */
	memcpy(pk->addr, buffer + key_size, key_size);

	ce_free(mem_n);
	return status;
}

/**
 * @brief Generate an EdDSA signature of a message
 * @param[in]  vce_id    ce index
 * @param[in]  curve     Curve used by the EdDSA algorithm
 * @param[in]  message   Message to sign
 * @param[in]  pub_key   EdDSA public key
 * @param[in]  priv_key  EdDSA private key
 * @param[in]  context   EdDSA context, maximum 255 bytes, NULL_blk for Ed25519
 * @param[out] signature EdDSA signature of message
 * @param[in]  use_cm    When true, use the countermeasures
 * @param[in]  rng       Random number generator to use when use_cm == true,
 *                       ignored otherwise
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t eddsa_generate_signature(void *device,
								  eddsa_curve_t curve,
								  block_t *k,
								  block_t *r,
								  block_t *s,
								  block_t *sig)
{
	uint32_t status;
	size_t key_size = eddsa_key_size[curve];

	eddsa_load_parameters(device, curve);

	pke_set_command(device, BA414EP_OPTYPE_EDDSA_SIGN_GEN, key_size,
					BA414EP_LITTLEEND, eddsa_hw_flags[curve]);

	/* Prepare EdDSA parameters for signature generation */
	point2CryptoRAM_rev(device, k, key_size, BA414EP_MEMLOC_6);
	point2CryptoRAM_rev(device, r, key_size, BA414EP_MEMLOC_8);
	mem2CryptoRAM_rev(device, s, key_size, BA414EP_MEMLOC_11, true);

	/* Generate signature */
	status = pke_start_wait_status(device);

	if (status) {
		return CRYPTOLIB_CRYPTO_ERR;
	}

	CryptoRAM2mem_rev(deivce, sig, key_size, BA414EP_MEMLOC_10, true);

	return status;
}

/**
 * Check parameters for the EdDSA operations
 *
 * @param[in]  curve     Curve used by the EdDSA algorithm
 * @param[in]  pub_key   EdDSA public key
 * @param[in]  priv_key  EdDSA private key
 * @param[in]  context   EdDSA context, maximum 255 bytes, NULL_blk for Ed25519
 * @param[in] signature EdDSA signature of message
 * @return CRYPTOLIB_SUCCESS when no issues were encountered
 *         CRYPTOLIB_INVALID_PARAM otherwise
 */

uint32_t eddsa_verify_signature(void *device,
								eddsa_curve_t curve,
								block_t *k,
								block_t *pk_y,
								block_t *r_y,
								block_t *sig)
{
	uint32_t status;
	size_t key_size = eddsa_key_size[curve];

	// Set command to disable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_EDDSA_SIGN_VERIF,
					key_size,
					BA414EP_LITTLEEND,
					SX_ED25519 == curve ? eddsa_hw_flags[curve] : (1 << BA414EP_CMD_FLAGA_LSB) | (1 << BA414EP_CMD_FLAGB_LSB) |
					eddsa_hw_flags[curve]);

	// Load curve parameters
	eddsa_load_parameters(device, curve);

	/* Prepare EdDSA parameters for signature verification */
	point2CryptoRAM_rev(device, k, key_size, BA414EP_MEMLOC_6);
	mem2CryptoRAM_rev(device, pk_y, key_size, BA414EP_MEMLOC_9, true);
	mem2CryptoRAM_rev(device, sig, key_size, BA414EP_MEMLOC_10, true); // S
	mem2CryptoRAM_rev(device, r_y, key_size, BA414EP_MEMLOC_11, true); // R

	status = pke_start_wait_status(device);

	if (status) {
		return CRYPTOLIB_INVALID_SIGN_ERR;
	}

	return status;
}
