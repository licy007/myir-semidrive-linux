/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_ERRORS_H
#define SX_ERRORS_H

/** @brief Operation (whatever the kind of operation) terminated nicely */
#define CRYPTOLIB_SUCCESS 0

/**
 * @brief Operation involving a parameter which does not respect
 *        the system limitations
 *
 *        (i.e. trying to store a byte string bigger than the reserved memory)
 */
#define CRYPTOLIB_UNSUPPORTED_ERR 1

/**
 * @brief Operation involving a signature verification which is failing
 *
 * Algorithms involving signature (i.e. DSA, ECDSA, ...) may return this code
 */
#define CRYPTOLIB_INVALID_SIGN_ERR 3

/**
 * @brief Operation failing to to data transfer over the DMA bus
 *
 *        For instance, it can be due to too much/not enough data transferred.
 */
#define CRYPTOLIB_DMA_ERR 4

/** @brief Operation involving a invalid ECC point (i.e. not on a curve) */
#define CRYPTOLIB_INVALID_POINT 5

/**
 * @brief Operation which didn't terminate nicely and not due to any other listed error.
 *
 *        i.e. due to a crypto engine error
 */
#define CRYPTOLIB_CRYPTO_ERR 6

/**
 * @brief DRBG random generation can't complete
 *        because new entropy should be reseeded in the DRBG
 */
#define CRYPTOLIB_DRBG_RESEED_NEEDED  7

/**
 * @brief Operation involving a signature is failing because the signature
 *        is not invertible.
 *
 *        Issuing a new operation with slightly different parameters
 *        (i.e. random if present) may solve this issue
 */
#define CRYPTOLIB_NON_INVERT_SIGN 8

/** @brief Operation involving a parameter detected as invalid
 *
 *        (i.e. out of range value)
 */
#define CRYPTOLIB_INVALID_PARAM 9
#define CRYPTOLIB_INVALID_PARAM_IV 10
#define CRYPTOLIB_INVALID_PARAM_KEY 11
#define CRYPTOLIB_INVALID_PARAM_CTX 12
#define CRYPTOLIB_INVALID_PARAM_INPUT 13
#define CRYPTOLIB_INVALID_PARAM_OUTPUT 14

#define CRYPTOLIB_PK_NOTQUADRATICRESIDUE 15
#define CRYPTOLIB_PK_COMPOSITE 16
#define CRYPTOLIB_PK_NOTINVERTIBLE 17
#define CRYPTOLIB_PK_PARAM_AB_NOTVALID 18
#define CRYPTOLIB_PK_SIGNATURE_NOTVALID 19
#define CRYPTOLIB_PK_NOTIMPLEMENTED 20
#define CRYPTOLIB_PK_N_NOTVALID 21
#define CRYPTOLIB_PK_COUPLE_NOTVALID 22
#define CRYPTOLIB_PK_POINT_PX_ATINFINITY 23
#define CRYPTOLIB_PK_POINT_PX_NOTONCURVE 24
#define CRYPTOLIB_PK_FAIL_ADDRESS 25

#endif
