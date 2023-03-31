/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_ECDSA_H
#define SX_ECDSA_H

#include "sx_ecc_keygen.h"
#include "ce_reg.h"
#include "sx_hash.h"
#include "sx_trng.h"
#include "ce.h"
// #define WITH_SIMULATION_PLATFORM 0

/**
 * @brief Validates curve parameters when given by the host \p curve
 *
 * The checks done over the curve parameters are described in BA414EP_PKE_DataSheet
 * Section Elliptic Curve Digital Signature Algorithm
 * @param device    device pointer
 * @param curve the ECC curve
 * @return CRYPTOLIB_SUCCESS if successful
 *         CRYPTOLIB_CRYPTO_ERR if there is an issue with the parameters
 */
uint32_t ecdsa_validate_domain(void *device,
							   const sx_ecc_curve_t *curve);

/**
 * @brief Verify an ECDSA signature from a \p formatted_digest
 * @param device    device pointer
 * @param curve            ECC curve
 * @param formatted_digest digest to verify against \p signature.
 * @param key              ECC public key
 * @param signature        signature to verify against the \p formatted_digest
 * @return CRYPTOLIB_SUCCESS if successful
 *         CRYPTOLIB_INVALID_SIGN_ERR if verification failed
 *
 * \note the \p formatted_digest corresponds to the digest truncated if required
 *       to match the ecc curve element size and also right shifted if required
 *       (left padded with zeros)
 */
uint32_t ecdsa_verify_signature_digest(
	void *device,
	const sx_ecc_curve_t *curve,
	block_t *formatted_digest,
	block_t *key,
	block_t *signature);

/**
 * @brief Configure hardware for signature generation.
 *
 * Load the curve parameters, the key and formatted_digest. Those are
 * needed once before attempting to generate signatures.
 * @param device           device pointer
 * @param curve            selects the ECC curve
 * @param formatted_digest digest to generate the signature.
 * @param key              ECC private key
 * @return CRYPTOLIB_SUCCESS if successful
 *
 * \note the \p formatted_digest corresponds to the digest truncated if required
 *       to match the ecc curve element size and also right shifted if required
 *       (left padded with zeros)
 */
uint32_t ecdsa_configure_signature(
	void *device,
	const sx_ecc_curve_t *curve,
	block_t *formatted_digest,
	block_t *key);

/**
 * @brief Attempt to compute an ECDSA signature
 *
 * This function tries to generate a signature and gives up immediately in case
 * of error. It is under the caller responsibility to attempt again to generate
 * a signature.
 * @param device           device pointer
 * @param curve            selects the ECC curve
 * @param signature        output signature of the \p formatted_digest
 * @param rng              used random number generator
 * @return CRYPTOLIB_SUCCESS if the signature is generated and valid,
 *         CRYPTOLIB_INVALID_SIGN_ERR if the generated signature is not valid
 *
 * \warning It is required to call ::sx_ecdsa_configure_signature \e before
 *          attempting to generate a signature as a few parameters have to be
 *          loaded.
 */
uint32_t ecdsa_attempt_signature(
	void *device,
	const sx_ecc_curve_t *curve,
	block_t *signatur);

/**
 * @brief Verifies a given signature corresponds to a given message
 *
 * @param device    device pointer
 * @param curve     selects the ECC curve
 * @param message   data to hash and verify against \p signature
 * @param key       ECC public key
 * @param signature signature to check
 * @param hash_fct  select hash algorithm to use on \p message
 * @return CRYPTOLIB_SUCCESS if successful
 *         CRYPTOLIB_INVALID_SIGN_ERR if signature verification failed
 */
uint32_t ecdsa_verify_signature(
	void *device,
	const void *curve_info,
	block_t *message,
	block_t *key,
	block_t *signature,
	hash_alg_t hash_fct);

/**
 * @brief Verifies a given signature corresponds to a given digest
 *
 * @param device           device pointer
 * @param curve            selects the ECC curve
 * @param formatted_digest digest to use to generate \p signature.
 * @param key              ECC private key
 * @param signature        output signature of the \p formatted_digest
 * @param rng              used random number generator
 * @return CRYPTOLIB_SUCCESS if successful
 *         CRYPTOLIB_INVALID_SIGN_ERR if signature verification failed
 *
 * \note the \p formatted_digest corresponds to the digest truncated if required
 *       to match the ecc curve element size and also right shifted if required
 *       (left padded with zeros)
 */
uint32_t ecdsa_generate_signature_digest(
	void *device,
	const sx_ecc_curve_t *curve,
	block_t *formatted_digest,
	block_t *key,
	block_t *signature);

/**
 * @brief Generates an ECDSA signature from a \p message
 *
 * @param device    device pointer
 * @param curve     selects the ECC curve
 * @param message   data to hash and sign
 * @param key ECC   private key
 * @param signature location to store the signature
 * @param hash_fct  select hash algorithm to use on \p message
 * @param rng       used random number generator
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t ecdsa_generate_signature(
	void *device,
	const void *curve_info,
	block_t *message,
	block_t *key,
	block_t *signature,
	hash_alg_t hash_fct);

#endif
