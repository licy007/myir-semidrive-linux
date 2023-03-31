/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_EDDSA_H
#define SX_EDDSA_H

#include <stdint.h>

#include "sx_trng.h"

/** Curve to be used by the EdDSA algorithms */
typedef enum eddsa_curve {
	SX_ED25519, /**< Ed25519 curve */
	SX_ED448    /**< Ed448 curve */
} eddsa_curve_t;

/** @brief Ed448 max context size in bytes */
#define EDDSA_MAX_CONTEXT_SIZE 255

/** @brief EdDSA key size in bytes */
#define ED25519_KEY_SIZE      32
#define ED448_KEY_SIZE        57
#define EDDSA_MAX_KEY_SIZE    ED448_KEY_SIZE

/** @brief EdDSA signature size in bytes */
#define ED25519_SIGN_SIZE     (2 * (ED25519_KEY_SIZE))
#define ED448_SIGN_SIZE       (2 * (ED448_KEY_SIZE))
#define EDDSA_MAX_SIGN_SIZE  ED448_SIGN_SIZE

/**
 * @brief Generates an EdDSA ECC public key based on a given private key.
 * @param[in]  vce_id    vce index
 * @param[in]  curve     Curve used by the EdDSA algorithm
 * @param[in]  sk        Private key for which public key is desired
 * @param[out] pk        Public key to be generated
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t eddsa_generate_pubkey(
	void *device,
	eddsa_curve_t curve,
	block_t *sk,
	block_t *pk);

/**
 * @brief Generate an EdDSA signature of a message
 * @param[in]  vce_id    vce index
 * @param[in]  curve     Curve used by the EdDSA algorithm
 * @param[in]  message   Message to sign
 * @param[in]  pub_key   EdDSA public key
 * @param[in]  priv_key  EdDSA private key
 * @param[in]  context   EdDSA context, maximum 255 bytes, NULL_blk for Ed25519
 * @param[out] signature EdDSA signature of message
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t eddsa_generate_signature(
	void *device,
	eddsa_curve_t curve,
	block_t *k,
	block_t *r,
	block_t *s,
	block_t *sig);

/**
 * @brief Verifies a given signature corresponds to a given message
 * @param[in] vce_id    vce index
 * @param[in] curve     Curve used by the EdDSA algorithm
 * @param[in] message   Message to verify against signature
 * @param[in] key       EdDSA public key
 * @param[in] context   EdDSA context, maximum 255 bytes, NULL_blk for Ed25519
 * @param[in] signature Signature to check
 * @return CRYPTOLIB_SUCCESS if successful
 *         CRYPTOLIB_INVALID_SIGN_ERR if signature verification failed
*/
uint32_t eddsa_verify_signature(
	void *device,
	eddsa_curve_t curve,
	block_t *k,
	block_t *pk_y,
	block_t *r_y,
	block_t *sig);

#endif
