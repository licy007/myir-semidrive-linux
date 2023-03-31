/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_ECC_KEYGEN
#define SX_ECC_KEYGEN

#include "sx_ecc.h"
void math_array_incr(uint8_t *a,
					 const size_t length,
					 int8_t value);

/** @brief ECC structure for ECC curve definition */

uint32_t ecc_curve_bytesize(const sx_ecc_curve_t *curve);

/**
 * @brief Validates the ECC public key is a point on the curve
 *        (valid for Weierstrass prime, binary or Edwards)
 *
 * @param  device       device pointer
 * @param  domain       curve parameters
 * @param  pub          public key to validate
 * @param  size         size of one element (expressed in bytes)
 * @param  curve_flags  curve acceleration parameters
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t ecc_validate_public_key(
	void *device,
	const block_t *domain,
	block_t *pub,
	uint32_t size,
	uint32_t curve_flags);

/**
 * @brief Generates an ECC key pair (private and public key)
 *        (valid for Weierstrass prime, binary or Edwards)
 *
 * @param  device      device pointer
 * @param  domain      curve parameters
 * @param  pub         public key to be generated
 * @param  priv        private key to be generated
 * @param  size        size of one element (expressed in bytes)
 * @param  curve_flags curve acceleration parameters
 * @param  rng         random number generator used
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t ecc_generate_keypair(void *device,
							  const block_t *group,
							  block_t *pub,
							  block_t *n,
							  block_t *priv,
							  uint32_t size,
							  uint32_t curve_flags);

/**
 * @brief Generates an ECC private key
 *        (valid for Weierstrass prime, binary or Edwards)
 *
 * @param  device      device pointer
 * @param  n           the order coming from the curve parameters
 * @param  priv        private key to be generated
 * @param  rng         random number generator used
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t ecc_generate_private_key(void *device,
								  block_t *n,
								  block_t *priv);

/**
 * @brief Generates a ECC public key
 *        (Weierstrass prime, binary or Edwards)
 *        based on the curve and a given private key.
 *
 * @param  device      device pointer
 * @param  curve       curve parameters (domain)
 * @param  pub         public key to be generated
 * @param  priv        private key for which public key is desired
 * @param  size        size of one element (expressed in bytes)
 * @param  curve_flags curve acceleration parameters
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t ecc_generate_public_key(void *device,
								 const block_t *group,
								 block_t *pub,
								 block_t *priv,
								 uint32_t size,
								 uint32_t curve_flags);

/**
 * @brief Generates a key pair for ECC Montgomery curve \p curve
 *
 * @param  device      device pointer
 * @param  domain      curve parameters (domain)
 * @param  pub         public key to be generated
 * @param  priv        private key to be generated
 * @param  size        size of one element (expressed in bytes)
 * @param  curve_flags curve acceleration parameters
 * @param  rng         used random number generator
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t ecc_montgomery_generate_keypair(
	void *device,
	block_t *domain,
	block_t *pub,
	block_t *priv,
	uint32_t size,
	uint32_t curve_flags);

/**
 * @brief Generates a Montgomery ECC private key
 *
 * @param  device      device pointer
 * @param  priv        private key to be generated
 * @param  rng         used random number generator
 */
void ecc_montgomery_generate_private_key(
	void *device,
	block_t *priv);

/**
 * @brief Generates a Montgomery ECC public key
 *        based on the curve and a given private key.
 *
 * @param  device      device pointer
 * @param  curve       curve parameters (domain)
 * @param  pub         public key to be generated
 * @param  priv        private key for which public key is desired
 * @param  size        size of one element (expressed in bytes)
 * @param  curve_flags curve acceleration parameters
 * @return  CRYPTOLIB_SUCCESS if no error
 */
uint32_t ecc_montgomery_generate_public_key(
	void *device,
	block_t *curve,
	block_t *pub,
	block_t *priv,
	uint32_t size,
	uint32_t curve_flags);

uint32_t rng_get_rand_lt_n_blk(void *device,
							   block_t *n,
							   block_t *dst
							  );
#endif
