/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_DSA_H
#define SX_DSA_H

#include <stdint.h>
#include "sx_hash.h"

uint32_t dsa_generate_private_key(void *device,
								  block_t *n,
								  block_t *priv);

/**
 * @brief Generates DSA public key with a given private
 *
 * @param  device     device point
 * @param  generator  Generator to use for key generation
 * @param  p          p modulo
 * @param  priv_key   private key
 * @param  pub_key    public key (output)
 * @return        returns CRYPTOLIB_SUCCESS if success
 */
uint32_t dsa_generate_public_key(
	void *device,
	block_t *p,
	block_t *generator,
	block_t *priv_key,
	block_t *pub_key);

/**
 * @brief Generates DSA signature of a message
 *
 * @param  device    device point
 * @param  algo_hash hash function to use
 * @param  p         p modulo
 * @param  q         q modulo
 * @param  generator generator of the key \p priv
 * @param  priv      private key
 * @param  message   message
 * @param  signature DSA signature of \p message (output)
 * @return           CRYPTOLIB_SUCCESS if success
 */
uint32_t dsa_generate_signature(
	void *device,
	hash_alg_t hash_alg,
	block_t *p,
	block_t *q,
	block_t *generator,
	block_t *priv,
	block_t *message,
	block_t *signature);

/**
 * @brief Verifies a given signature corresponds to a given message
 *
 * @param  device    device point
 * @param  algo_hash hash function to use
 * @param  generator generator of the key
 * @param  p         p modulo
 * @param  q         q modulo
 * @param  pub       public key
 * @param  message   message to check
 * @param  signature DSA signature to check
 * @return           CRYPTOLIB_SUCCESS if signature matches
 *                   CRYPTOLIB_CRYPTO_ERR if signature or error occurs
 */
uint32_t dsa_verify_signature(
	void *device,
	hash_alg_t hash_alg,
	block_t *p,
	block_t *q,
	block_t *generator,
	block_t *pub,
	block_t *message,
	block_t *signature);

#endif
