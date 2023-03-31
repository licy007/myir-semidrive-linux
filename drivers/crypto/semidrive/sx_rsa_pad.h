/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_RSA_PAD_H
#define SX_RSA_PAD_H

#include <stddef.h>
#include "sx_hash.h"
#include "sx_rsa.h"

#define RSA_PKCS1_PADDING_SIZE  11

void oaep_L_init(void);
uint32_t oaep_L_get(block_t *tmp);
uint32_t oaep_L_set(block_t *tmp);

uint32_t rsa_pad_add_none(uint8_t *from, uint32_t flen, uint32_t tlen);

/**
 * Encode a message using OAEP
 * @param  device   device pointer
 * @param  k        RSA parameter length in bytes
 * @param  hashType The hash function to use (default is SHA1)
 * @param  EM       Pointer to the encoded message (EM) buffer
 * @param  message  Block containing address of the message to encode
 * @param  mLen     Length of the message to be encoded (bytes)
 * @param  rng      Random number generator to use
 * @return          CRYPTOLIB_SUCCESS when no error occurs
 */
uint32_t rsa_pad_eme_oaep_encode(void *device,
								 uint8_t *EM,
								 uint32_t k,
								 block_t *message,
								 uint32_t mLen,
								 uint8_t *param,
								 uint32_t plen,
								 hash_alg_t data_hashType,
								 hash_alg_t mgf_hash_Type);

/**
 * Decode a message using OEAP
 * @param  device   device pointer
 * @param  k        RSA parameter length in bytes
 * @param  hashType the hash function to use (default is SHA1)
 * @param  EM       Pointer to the encoded message (EM) buffer
 * @param  message  Output: Pointer to the decoded message. The decoding is
 *                  done in place within the limits of the EM buffer.
 * @param  mLen     Output: Length of the decoded message
 * @return          CRYPTOLIB_SUCCESS when no error occurs
 */
uint32_t rsa_pad_eme_oaep_decode(void *device,
								 uint32_t k,
								 hash_alg_t hashType,
								 uint8_t *EM,
								 uint8_t **message,
								 size_t *mLen);

/**
 * Encode message using PKCS
 * @param  device   device pointer
 * @param  k        RSA parameter length in bytes
 * @param  EM       Pointer to the encoded message (EM) buffer
 * @param  message  Block containing address of the message to encode
 * @param  mLen     Len of the message to be encoded (bytes)
 * @param  rng      random number generator rsa_pad_eme_pkcs_encodeto use
 * @return          CRYPTOLIB_SUCCESS when no error occurs
 */
uint32_t rsa_pad_eme_pkcs_encode(void *device,
								 uint8_t *to,
								 uint32_t tlen,
								 uint8_t *from,
								 uint32_t flen,
								 rsa_key_types_t key_type);

/**
 * Decodes encoded message using PKCS. message will point to the decoded message in EM
 * @param  k        RSA param length in bytes
 * @param  EM       Pointer to the encoded message (EM) buffer
 * @param  message  Output: Pointer to the decoded message in EM. The decoding
 *                  is done in place within the limits of the EM buffer.
 * @param  mLen     Output: Length of the decoded message (bytes)
 * @return          CRYPTOLIB_SUCCESS when no error occurs
 */
uint32_t rsa_pad_eme_pkcs_decode(uint32_t k,
								 uint8_t *EM,
								 uint8_t **message,
								 uint32_t *mLen,
								 rsa_key_types_t key_type);

/**
 * Encode hash using PKCS
 * @param  device    device pointer
 * @param  emLen     Length of encoded message (EM) buffer (RSA parameter length
 *                   in bytes)
 * @param  hash_type Hash function used for hashing \p hash (also used for the
 *                   PKCS algorithm)
 * @param  EM        Output: Pointer to the encoded message buffer
 * @param  hash      Input: Hash to encode
 * @return           CRYPTOLIB_SUCCESS if no error occurs
 */
uint32_t rsa_pad_emsa_pkcs_encode(void *device,
								  uint32_t emLen,
								  hash_alg_t hash_type,
								  uint8_t *EM,
								  uint8_t *hash);

/**
 * Encode hash using PSS. This function uses a salt length equal to the hash
 * digest length.
 * @param  device   device pointer
 * @param  emLen     Length of encoded message (EM) buffer (RSA parameter length
 *                   in bytes)
 * @param  hashType  Hash function used for hashing \p hash (also used for PKCS
 *                   algorithm)
 * @param  EM        Output: Pointer to the encoded message buffer
 * @param  hash      Input: Hash to encode
 * @param  n0        MSB of the modulus (for masking in order to match the
 *                   modulus size)
 * @param  sLen      Intended length of the salt
 * @param  rng       Random number generator to use
 * @return           CRYPTOLIB_SUCCESS if no error occurs
 */
uint32_t rsa_pad_emsa_pss_encode(void *device,
								 uint32_t emLen,
								 hash_alg_t hashType,
								 uint8_t *EM,
								 uint8_t *hash,
								 uint32_t n0,
								 size_t sLen);

/**
 * Decode encoded message using PSS and compares hash to the decoded hash
 * @param  device   device pointer
 * @param  emLen     Length of encoded message (EM) buffer (RSA parameter length
 *                   in bytes)
 * @param  hashType  Hash function used for hasing \p hash
 * @param  EM        Input: Pointer to the encoded message buffer
 * @param  hash      Hash to compare with
 * @param  sLen      Intended length of the salt
 * @param  n0        MSB of the modulus (for masking in order to match the
 *                   modulus size)
 * @return           CRYPTOLIB_SUCCESS if no error occurs and hash is valid
 */
uint32_t rsa_pad_emsa_pss_decode(void *device,
								 uint32_t emLen,
								 hash_alg_t hashType,
								 uint8_t *EM,
								 uint8_t *hash,
								 uint32_t sLen,
								 uint32_t n0);

/**
 * Pads the hash of \p hashLen to \p EM of \p emLen. MSBs are set to 0.
 *
 * \warning There should not be overlapping between \p EM and \p hash !
 * @param EM      Destination buffer (pointer)
 * @param emLen   Length of the destination buffer (bytes)
 * @param hash    Input to pad
 * @param hashLen Length of the input
 */
void rsa_pad_zeros(uint8_t *EM,
				   size_t emLen,
				   uint8_t *hash,
				   size_t hashLen);

#endif
