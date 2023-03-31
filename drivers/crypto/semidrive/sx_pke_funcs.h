/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_PKE_FUNCS_H
#define SX_PKE_FUNCS_H


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
uint32_t prime_verify(void *device,
					  block_t *candi_prime,
					  block_t *ref_prime,
					  uint32_t iteration,
					  bool *is_prime);

/**
 * @brief modular addation(C= A + B mod N).
 *
 * @param device      device pointer
 * @param ptra        input number1
 * @param ptrb        input number2
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_add(void *device,
					 block_t *ptra,
					 block_t *ptrb,
					 block_t *ptrn,
					 block_t *ptrc);

/**
 * @brief modular substraction(C= A - B mod N).
 *
 * @param device      device pointer
 * @param ptra        input number1
 * @param ptrb        input number2
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_sub(void *device,
					 block_t *ptra,
					 block_t *ptrb,
					 block_t *ptrn,
					 block_t *ptrc);

/**
 * @brief modular multiplication(C= A * B mod N).
 *
 * @param device      device pointer
 * @param ptra        input number1
 * @param ptrb        input number2
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_multi(void *device,
					   block_t *ptra,
					   block_t *ptrb,
					   block_t *ptrn,
					   block_t *ptrc);

/**
 * @brief modular reduction for odd number(C= B mod N).
 *
 * @param device      device pointer
 * @param ptrb        input number
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_reduce_odd(void *device,
							block_t *ptrb,
							block_t *ptrn,
							block_t *ptrc);

/**
 * @brief modular divsion(C= A / B mod N).
 *
 * @param device      device pointer
 * @param ptra        input number1
 * @param ptrb        input number2
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_div(void *device,
					 block_t *ptra,
					 block_t *ptrb,
					 block_t *ptrn,
					 block_t *ptrc);

/**
 * @brief modular inversion for odd number(C= 1 / B mod N).
 *
 * @param device      device pointer
 * @param ptrb        input number
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_inverse_odd(void *device,
							 block_t *ptrb,
							 block_t *ptrn,
							 block_t *ptrc);

/**
 * @brief modular square root(C= sqrt(A) mod N).
 *
 * @param device      device pointer
 * @param ptra        input number
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_square_root(void *device,
							 block_t *ptra,
							 block_t *ptrn,
							 block_t *ptrc);

/**
 * @brief multiplicaton(C= A * B).
 *
 * @param device      device pointer
 * @param ptra        input number1
 * @param ptrb        input number2
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
/*  */
uint32_t multiplicate(void *device,
					  block_t *ptra,
					  block_t *ptrb,
					  block_t *ptrc);

/**
 * @brief modular inversion for even number(C= 1 / B mod N).
 *
 * @param device      device pointer
 * @param ptrb        input number
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_inverse_even(void *device,
							  block_t *ptrb,
							  block_t *ptrn,
							  block_t *ptrc);

/**
 * @brief modular reduction for even number(C= B mod N).
 *
 * @param device      device pointer
 * @param ptrb        input number
 * @param ptrn        modular number
 * @param ptrc        result
 * @return CRYPTOLIB_SUCCESS if successful
 */
uint32_t modular_reduce_even(void *device,
							 block_t *ptrb,
							 block_t *ptrn,
							 block_t *ptrc);

#endif
