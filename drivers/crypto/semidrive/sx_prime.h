/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_PRIME_H
#define SX_PRIME_H

// #include <stdint.h>
// #include <stdbool.h>
#include "sx_trng.h"
/**
 * @brief Converge from a given number to a prime number using the Miller-Rabin
 * algorithm
 * @details This algorithm tries to converge to a prime number from a given
 * random number. First it tries to find a small divider, then a larger divider.
 * If neither is found, the Miller-Rabin algorithm is applied. If this also
 * confirms the number is probably prime, the number is assumed prime. If the
 * given number is not prime, it is incremented (in steps of 2) and checked again.
 * The procedure is repeated until a prime is found.
 * @param number pointer to a given random number
 * @param nr_of_bytes byte size of \p number
 * @param mr_rounds number of rounds for the Miller-Rabin algorithm
 * @param rng random number generator to use with the Miller-Rabin algorithm
 * @return CRYPTOLIB_SUCCESS if no errors occurred
 */
uint32_t converge_to_prime(void *device,
						   block_t *candi_number,
						   uint32_t rounds);
#if 0
/**
 * @brief Generate a prime starting from a random number using the Miller-Rabin
 * algorithm
 * @param prime pointer to the resulting prime
 * @param bitsize the size of the prime in bits
 * @param mr_rounds number of rounds for the Miller-Rabin algorithm
 * @param rng random number generator to be used by the Miller-Rabin algorithm
 * @return CRYPTOLIB_SUCCESS if no errors occurred
 */
uint32_t generate_prime(uint8_t *prime, uint32_t bitsize, uint32_t mr_rounds,
						struct sx_rng rng);
#endif

/**
 * @brief Verify a candidate number is a prime
 *
 * @param device      device pointer
 * @param candi_prime candidate prime
 * @param ref_prime   auxiliary prime
 * @param iteration   iterating times, it depends on size (in bits) of candidate prime, the security strength, the error probability used
 * @param is_prime    this number is prime probably
 * @return CRYPTOLIB_SUCCESS if successful
 *         CRYPTOLIB_INVALID_SIGN_ERR if verification failed
 */
uint32_t prime_verify_rsa(void *device,
						  block_t *candi_prime,
						  uint32_t iteration,
						  bool *is_prime);
#endif
