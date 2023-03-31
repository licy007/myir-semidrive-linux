/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_RSA_H
#define SX_RSA_H

//#include <stdint.h>
#include "sx_hash.h"
#include "ce_reg.h"
#include "sx_pke_funcs.h"
#include "sx_prime.h"
#include "ce.h"
/**
* @brief Enumeration of possible padding types
*/

typedef enum rsa_key_types_e {
	RSA_PRIV_KEY = 0x0,
	RSA_PUB_KEY  = 0x1
} rsa_key_types_t;

typedef enum rsa_pad_types_e {
	ESEC_RSA_PADDING_NONE      = 0x0,/**< No padding */
	ESEC_RSA_PADDING_OAEP      = 0x1,/**< Optimal Asymmetric Encryption Padding */
	ESEC_RSA_PADDING_EME_PKCS  = 0x2,/**< EME-PKCS padding */
	ESEC_RSA_PADDING_EMSA_PKCS = 0x3,/**< EMSA-PKCS padding */
	ESEC_RSA_PADDING_PSS       = 0x4 /**< EMSA-PSS padding */
} rsa_pad_types_t;

/**
* @brief RSA encryption (block_t as parameters)
* @param device        device pointer
* @param padding_type  Padding to use to encode message
* @param message       Message to be encrypted
* @param n             = p * q (multiplication of two random primes)
* @param public_expo   Public exponent
* @param result        Result buffer
* @param hashType      Hash type used in OAEP padding type
* @param be_param      n/public_expo is big_endian
* @return CRYPTOLIB_SUCCESS if successful
*/

uint32_t rsa_encrypt_blk(void *device,
						 rsa_pad_types_t padding_type,
						 block_t *message,
						 block_t *n,
						 block_t *public_expo,
						 block_t *result,
						 hash_alg_t hashType,
						 bool be_param);

/**
* @brief RSA decryption (block_t as parameters)
* @param device          device pointer
* @param padding_type    Padding to use to encode message
* @param cipher          Cipher to be decrypted
* @param n               = p * q (multiplication of two random primes), the RSA
*                        modulus
* @param private_key     Private key
* @param result          Result buffer
* @param msg_len         Pointer to the message length (for padded messages)
* @param hashType        Hash type used in OAEP padding type
* @param be_param        n/private_key is big_endian
* @return CRYPTOLIB_SUCCESS if successful
*/
uint32_t rsa_decrypt_blk(void *device,
						 rsa_pad_types_t padding_type,
						 block_t *cipher,
						 block_t *n,
						 block_t *private_key,
						 block_t *result,
						 uint32_t *msg_len,
						 hash_alg_t hashType,
						 bool be_param);
/**
* @brief RSA crt decryption (block_t as parameters)
* @param device          device pointer
* @param padding_type    Padding to use to encode message
* @param cipher          Cipher to be decrypted
* @param p               one random primes
* @param q               one random primes
* @param dP              dP = d (mod p-1)
* @param private_key     Private key
* @param result          Result buffer
* @param msg_len         Pointer to the message length (for padded messages)
* @param hashType        Hash type used in OAEP padding type
* @param be_param        n/private_key is big_endian
* @return CRYPTOLIB_SUCCESS if successful
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
							 hash_alg_t hashType);

uint32_t rsa_priv_encrypt_blk(void *device,
							  rsa_pad_types_t padding_type,
							  block_t *message,
							  block_t *n,
							  block_t *public_expo,
							  block_t *result,
							  hash_alg_t hashType,
							  bool be_param);

uint32_t rsa_pub_decrypt_blk(void *device,
							 rsa_pad_types_t padding_type,
							 block_t *cipher,
							 block_t *n,
							 block_t *private_key,
							 block_t *result,
							 uint32_t *msg_len,
							 hash_alg_t hashType,
							 bool be_param);
/**
* @brief RSA signature generate (block_t as parameters, input is digest)
* @param device       device pointer
* @param sha_type     Hash type to use for signature, if pad use hash,
*					  digest->size should be the same as digest_len which is sha_type hash's len of result
* @param padding_type Padding type to use (see ::rsa_pad_types_t)
* @param digest       digest to be signed
* @param result       Location where the signature will be stored
* @param n             = p * q (multiplication of two random primes), the RSA
*                     modulus
* @param private_key  Private key:
*                     - the pair (n, d) where n is the RSA modulus and d is the
*                     private exponent
*                     - the tuple (p, q, dP, dQ, qInv) where p is the first
*                     factor, q  is the second factor, dP is the first factor's
*                     CRT exponent, dQ is the second factor's CRT exponent, qInv
*                     is the (first) CRT coefficient
* @param crt          If equal to 1, uses CRT parameters
* @param salt_length  Expected length of the salt used in PSS padding_type
* @param be_param     n/private_key is big_endian
* @return CRYPTOLIB_SUCCESS if successful
*/
uint32_t rsa_signature_by_digest(void *device,
								 hash_alg_t sha_type,
								 rsa_pad_types_t padding_type,
								 block_t *digest,
								 block_t *result,
								 block_t *n,
								 block_t *private_key,
								 uint32_t salt_length,
								 bool be_param);

/**
* @brief RSA signature generate (block_t as parameters, input is message)
* @param device       device pointer
* @param sha_type     Hash type to use for signature
* @param padding_type Padding type to use (see ::rsa_pad_types_t)
* @param message      Message to be signed
* @param result       Location where the signature will be stored
* @param n             = p * q (multiplication of two random primes), the RSA
*                     modulus
* @param private_key  Private key:
*                     - the pair (n, d) where n is the RSA modulus and d is the
*                     private exponent
*                     - the tuple (p, q, dP, dQ, qInv) where p is the first
*                     factor, q  is the second factor, dP is the first factor's
*                     CRT exponent, dQ is the second factor's CRT exponent, qInv
*                     is the (first) CRT coefficient
* @param crt          If equal to 1, uses CRT parameters
* @param salt_length  Expected length of the salt used in PSS padding_type
* @param be_param     n/private_key is big_endian
* @return CRYPTOLIB_SUCCESS if successful
*/
uint32_t rsa_signature_generate_blk(void *device,
									hash_alg_t sha_type,
									rsa_pad_types_t padding_type,
									block_t *message,
									block_t *result,
									block_t *n,
									block_t *private_key,
									uint32_t salt_length,
									bool be_param);

/**
 * @brief RSA Signature verification (on block_t, input is digest)
 * @param device         device pointer
 * @param sha_type       Hash function type used for verification
 * @param padding_type   Defines the padding to use
 * @param digest         The digest
 * @param n              = p * q (multiplication of two random primes), the RSA
 *                       modulus
 * @param public_expo    e, the RSA public exponent
 * @param signature      Pointer to signature
 * @param salt_length    Expected length of the salt used in PSS padding_type
 * @param be_param       n/public_expo is big_endian
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t rsa_signature_verify_by_digest(void *device,
										hash_alg_t sha_type,
										rsa_pad_types_t padding_type,
										block_t *digest,
										block_t *n,
										block_t *public_expo,
										block_t *signature,
										uint32_t salt_length,
										bool be_param);

/**
 * @brief RSA Signature verification (on block_t)
 * @param device         device pointer
 * @param sha_type       Hash function type used for verification
 * @param padding_type   Defines the padding to use
 * @param message        The message
 * @param n              = p * q (multiplication of two random primes), the RSA
 *                       modulus
 * @param public_expo    e, the RSA public exponent
 * @param signature      Pointer to signature
 * @param salt_length    Expected length of the salt used in PSS padding_type
 * @param be_param       n/public_expo is big_endian
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t rsa_signature_verify_blk(void *device,
								  hash_alg_t sha_type,
								  rsa_pad_types_t padding_type,
								  block_t *message,
								  block_t *n,
								  block_t *public_expo,
								  block_t *signature,
								  uint32_t salt_length,
								  bool be_param);


/**
* @brief RSA private key generate using block_t
* @param device      device pointer
* @param p           p, the first factor
* @param q           q, the second factor
* @param public_expo e, the RSA public exponent
* @param n           = p * q (multiplication of two random primes), the RSA
*                    modulus
* @param private_key The private key: n followed by lambda(n) if \p lambda is
*                    set, followed by d, the private exponent
* @param size        Number of bytes of the parameters
* @param lambda      1 if lambda(n) should be copied to the resulting private
*                    key (non-CRT only)
* @return CRYPTOLIB_SUCCESS if successful
*/
uint32_t rsa_private_key_generate_blk(void *device,
									  block_t *p,
									  block_t *q,
									  block_t *public_expo,
									  block_t *n,
									  block_t *private_key,
									  uint32_t size,
									  uint32_t lambda);

/**
* Generates CRT parameters based on \p p, \p q and \p d
* @param  device device pointer
* @param  p      p, the first factor
* @param  q      q, the second factor
* @param  d      the private exponent
* @param  dp     the first factor's CRT exponent, dP
* @param  dq     the second factor's CRT exponent, dQ
* @param  inv    the (first) CRT coefficient, qInv
* @param  size   the size (in bytes) of an element
* @return CRYPTOLIB_SUCCESS if no error
*/
uint32_t rsa_crt_key_generate_blk(void *device,
								  block_t *p,
								  block_t *q,
								  block_t *d,
								  block_t *dp,
								  block_t *dq,
								  block_t *inv,
								  uint32_t size);

uint32_t rsa_key_generate_blk(void *device,
							  uint32_t nbr_of_bytes,          //keysize   input
							  block_t *e,                     //(e)       input
							  block_t *p,                     //p         output
							  block_t *q,                     //q         output
							  block_t *n,                     //n         output
							  block_t *d,                     //(d)       output
							  bool public,
							  bool lambda);

#endif
