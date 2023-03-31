/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
* The SRP protocol is described by the RFC2945
* (https://tools.ietf.org/html/rfc2945).
*
*/

#ifndef SX_SRP_H
#define SX_SRP_H

#include <stdint.h>

#include "sx_hash.h"
#include "sx_trng.h"

/**
* Generate pub key (by the host/server)
* @param  vce_id    ce index
* @param  N         Domain of the key
* @param  g         Generator for the key
* @param  v         Verifier generated previously using srp_gen_verifier()
* @param  B         Output: public key B = g^b mod N
* @param  b         Output: private key
* @param  hash_fct  Hash function to use
* @param  SRP6a     Enables SRP-6a
* @param  size      Size of the operands
* @param  rng       Random number generator to use
* @return  CRYPTOLIB_SUCCESS if no error was encountered
*/
uint32_t srp_host_gen_pub(uint32_t vce_id,
						  hash_alg_t hash_alg,
						  block_t N,
						  block_t g,
						  block_t v,
						  block_t B,
						  block_t b,
						  uint32_t SRP6a,
						  uint32_t size);

/**
* Generation of session key (by the host/server)
* @param  vce_id    ce index
* @param  N         Domain of the key
* @param  A         Public key of the user
* @param  v         Verifier (generated previously)
* @param  B         Public key of the host
* @param  b         Private key of the host
* @param  K         Output: Session key
* @param  hash_fct  Hash function to use
* @param  size      Size of the operands
* @return  CRYPTOLIB_SUCCESS if no error was encountered
*/
uint32_t srp_host_gen_key(uint32_t vce_id,
						  hash_alg_t hash_alg,
						  block_t N,
						  block_t A,
						  block_t v,
						  block_t b,
						  block_t B,
						  block_t K,
						  uint32_t size);

/**
* Generation of session key (by the user/client)
* @param  vce_id    ce index
* @param  N         Domain of the key
* @param  g         Generator for the key
* @param  A         Public key of the client
* @param  B         Public key of the host
* @param  usrpwd    The concatenation of user name, a colon (:) and the password
* @param  s         Salt
* @param  a         User private key
* @param  K         Output: Session key
* @param  hash_fct  Hash function to use
* @param  SRP6a     Enables SRP-6a
* @param  size      Size of the operands
* @return  CRYPTOLIB_SUCCESS if no error
*/
uint32_t srp_user_gen_key(uint32_t vce_id,
						  hash_alg_t hash_alg,
						  block_t N,
						  block_t g,
						  block_t A,
						  block_t B,
						  block_t usrpwd,
						  block_t s,
						  block_t a,
						  block_t K,
						  uint32_t SRP6a,
						  uint32_t size);

#endif
