/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include "sx_prime.h"

#include "sx_errors.h"
#include "sx_pke_conf.h"
#include "sx_dma.h"
#include "sx_trng.h"
#include "sx_pke_funcs.h"
#include "sx_ecc_keygen.h"

#define LOCAL_TRACE 0 //close local trace 1->0


/** @brief Executes a single round of the Miller-Rabin primality test
 *  @param rand random number with 1 < rand < number-1
 *  @param number number that is a prime candidate
 *  @param size byte size of \p number
 *  @param is_probably_prime true if \p number is probably prime
 *  @return CRYPTOLIB_SUCCESS if no errors occurred
 */
static uint32_t miller_rabin_round(
	void *device,
	block_t *ref_prime,
	block_t *candi_prime,
	bool *is_prime)
{
	uint32_t status = 0;
	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_MILLER_RABIN, candi_prime->len, BA414EP_LITTLEEND, 0);

	/* Load verification parameters */
	mem2CryptoRAM_rev(device, ref_prime, ref_prime->len, BA414EP_MEMLOC_6, true);
	mem2CryptoRAM_rev(device, candi_prime, candi_prime->len, BA414EP_MEMLOC_0, true);

	status = pke_start_wait_status(device);

	if (status && status != CRYPTOLIB_PK_COMPOSITE) {
		//    LTRACEF("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
		return status;
	}

	*is_prime = (status != CRYPTOLIB_PK_COMPOSITE);

	return CRYPTOLIB_SUCCESS;
}

uint32_t prime_verify_rsa(void *device,
						  block_t *candi_prime,
						  uint32_t iteration,
						  bool *is_prime)
{
	uint8_t *rand;// working buffer for intermediate steps
	struct mem_node *mem_n;
	block_t tmp_block;
	uint32_t i;
	uint32_t length;
	uint32_t status;

	if (candi_prime->len > PRIME_MAX_SIZE || candi_prime->len == 0) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	length = candi_prime->len;

	mem_n = ce_malloc(PRIME_MAX_SIZE);

	if (mem_n != NULL) {
		rand = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	for (i = 0; i < iteration; i++) {
		/* Generate a random number with 1 < rand < number-1 */
		math_array_incr((uint8_t *)(candi_prime->addr), length, -2);
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		tmp_block = block_t_convert(rand, length, EXT_MEM);
		status = rng_get_rand_less_ref(device, // 0 < rand < number-2
									   &tmp_block, candi_prime);

		if (status) {
			//    pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
			ce_free(mem_n);
			return status;
		}

		math_array_incr(rand, length, 1); // 1 < rand < number-1
		math_array_incr((uint8_t *)(candi_prime->addr), length, 2); // restore prime candidate
		/* Execute Miller-Rabin round through BA414EP */
		status = miller_rabin_round(device, &tmp_block, candi_prime, is_prime);

		if (status) {
			// pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
			ce_free(mem_n);
			return status;
		}

		/* Return if the prime candidate is composite */
		if (*is_prime == false) {
			ce_free(mem_n);
			return CRYPTOLIB_SUCCESS;
		}
	}

	ce_free(mem_n);
	return CRYPTOLIB_SUCCESS;
}

static const __attribute__((aligned(CACHE_LINE))) uint8_t  first_odd_primes[8] = {3, 5, 7, 11, 13, 17, 19, 23};
static const uint32_t prod_first_8_odd_primes = 111546435;
static const __attribute__((aligned(CACHE_LINE))) uint8_t  prod_first_130_odd_primes[128] = {
	0x0a, 0x89, 0x99, 0x92, 0x50, 0x8f, 0xd5, 0x92,
	0xb9, 0xda, 0xfd, 0x74, 0xdd, 0x54, 0x3e, 0xf7,
	0x6e, 0x2d, 0x9f, 0x50, 0x33, 0x68, 0x19, 0xf2,
	0x28, 0x33, 0xfb, 0x3b, 0x07, 0xb1, 0xf0, 0xc9,
	0x0b, 0x3e, 0x4f, 0x1a, 0xcd, 0x73, 0x80, 0x18,
	0xf9, 0x15, 0x42, 0x07, 0x0a, 0x92, 0x99, 0x8f,
	0x1d, 0xd8, 0x70, 0x32, 0xb7, 0x05, 0x16, 0x70,
	0xac, 0x8c, 0xfa, 0x03, 0x33, 0x56, 0x52, 0x24,
	0x0c, 0x3e, 0xae, 0x8e, 0x5c, 0x8e, 0x31, 0xf6,
	0x91, 0x04, 0xd3, 0xef, 0xc3, 0xe0, 0xcd, 0x17,
	0x08, 0xcf, 0xbc, 0x23, 0x00, 0x15, 0xa4, 0x65,
	0xfc, 0x0f, 0xc8, 0xb6, 0xb0, 0xf3, 0x61, 0xe0,
	0x16, 0x07, 0xfb, 0x81, 0xd3, 0x86, 0x57, 0x78,
	0xff, 0x11, 0xf0, 0xb5, 0x3d, 0x56, 0x03, 0x74,
	0xc7, 0xdf, 0x5d, 0xe0, 0x46, 0x94, 0x12, 0x32,
	0xbd, 0xa4, 0xb5, 0x47, 0x06, 0x39, 0xd7, 0x87
};

/**
 * Compute the modulo of a number when divided by the product of the first 8 odd primes
 * @param number number to divide
 * @param nr_of_bytes byte size of \p number \pre should be <= PRIME_MAX_SIZE
 * @param mod modulo that was computed
 * @return CRYPTOLIB_SUCCESS if successful
 */
static uint32_t get_prime_modulo(void *device, uint8_t *number, uint32_t nr_of_bytes, uint32_t *mod)
{
	uint8_t *tmp;// working buffer for intermediate steps
	struct mem_node *mem_n;
	block_t tmp_block = block_t_convert(NULL, 0, EXT_MEM);
	block_t number_block = block_t_convert(number, nr_of_bytes, EXT_MEM);
	uint32_t status;

	if (nr_of_bytes > PRIME_MAX_SIZE) {
		// pr_emerg("%s, %d, status: 0x%x\n", __func__, __LINE__, CRYPTOLIB_INVALID_PARAM);
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(PRIME_MAX_SIZE);

	if (mem_n != NULL) {
		tmp = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	memset(tmp, 0, nr_of_bytes - sizeof(prod_first_8_odd_primes));
	tmp[nr_of_bytes - 4] = (prod_first_8_odd_primes & 0xFF000000) >> 24;        //6 byte
	tmp[nr_of_bytes - 3] = (prod_first_8_odd_primes & 0x00FF0000) >> 16;        //4 byte
	tmp[nr_of_bytes - 2] = (prod_first_8_odd_primes & 0x0000FF00) >> 8;         //2 byte
	tmp[nr_of_bytes - 1] = (prod_first_8_odd_primes & 0x000000FF);
	tmp_block = block_t_convert(tmp, nr_of_bytes, EXT_MEM);
	status = modular_reduce_odd(device,                    //tmp_block = number_block mod tmp_block
								&number_block,
								&tmp_block,
								&tmp_block);

	if (status) {
		ce_free(mem_n);
		return status;
	}

	//char2int
	*mod = (tmp[nr_of_bytes - 4] << 24) | (tmp[nr_of_bytes - 3] << 16) |
		   (tmp[nr_of_bytes - 2] << 8) | (tmp[nr_of_bytes - 1]);

	ce_free(mem_n);
	return CRYPTOLIB_SUCCESS;
}

/**
 * Check if a number is co-prime with the product of the first 130 odd prime numbers
 * @param number number to check
 * @param nr_of_bytes byte size of \p number \pre should be <= PRIME_MAX_SIZE
 * @param coprime true if co-prime
 * @return CRYPTOLIB_SUCCESS if successful
 */
static uint32_t is_coprime(void *device, uint8_t *number, uint32_t nr_of_bytes, bool *coprime)
{
	uint8_t *tmp;// working buffer for intermediate steps
	struct mem_node *mem_n;
	uint8_t *tmp2;
	const size_t prodsize = sizeof(prod_first_130_odd_primes);
	uint32_t status;
	block_t block_tmp, block_number, block_prod;

	if (nr_of_bytes > PRIME_MAX_SIZE) {
		// pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, CRYPTOLIB_INVALID_PARAM);
		return CRYPTOLIB_INVALID_PARAM;
	}

	mem_n = ce_malloc(PRIME_MAX_SIZE);

	if (mem_n != NULL) {
		tmp = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	/* All inputs to a PK primitive operation (modular reduction, inversion...)
	 * should have the same byte size.
	 * If the number is shorter than 1024 bits, pad it with leading zeros before
	 * proceeding to the modular inversion that checks co-primality.
	 */
	tmp2 = tmp;

	if (nr_of_bytes < prodsize) {
		memset(tmp, 0, prodsize);
		block_tmp = block_t_convert(tmp + prodsize - nr_of_bytes, nr_of_bytes, EXT_MEM);
		block_number = block_t_convert(number, nr_of_bytes, EXT_MEM);
		memcpy_blk(device, &block_tmp, &block_number, nr_of_bytes);
		tmp2 = tmp;
		//number = tmp;
	}
	/* If the number is larger than 1024 bits, reduce it to its modulo when
	 * divided by prod_first_130_odd_primes. Rather than padding the product,
	 * we reduce the number itself. This is done for performance: doing a modular
	 * reduction + modular inversion on 1024 bits is faster than a modular
	 * inversion on larger operands.
	 */
//    else if (nr_of_bytes > prodsize) {
	else {
		memset(tmp, 0, nr_of_bytes);
		block_tmp = block_t_convert(tmp + nr_of_bytes - prodsize, prodsize, EXT_MEM);
		block_prod = block_t_convert(prod_first_130_odd_primes, prodsize, EXT_MEM);
		memcpy_blk(device, &block_tmp, &block_prod, prodsize);
		block_number = block_t_convert(number, nr_of_bytes, EXT_MEM);
		block_tmp = block_t_convert(tmp, nr_of_bytes, EXT_MEM);
		status = modular_reduce_odd(device, &block_number, &block_tmp, &block_tmp);

		if (status) {
			//    pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
			ce_free(mem_n);
			return CRYPTOLIB_CRYPTO_ERR;
		}

		tmp2 = tmp + nr_of_bytes - prodsize;
	}


	/* Check that the greatest common divisor between the number and
	 * prod_first_130_odd_primes is 1.
	 */
	block_number = block_t_convert(tmp2, prodsize, EXT_MEM);
	block_prod = block_t_convert((uint8_t *)prod_first_130_odd_primes, prodsize, EXT_MEM);
	block_tmp = block_t_convert(tmp, prodsize, EXT_MEM);
	status = modular_inverse_odd(device, &block_number, &block_prod, &block_tmp);

	if (status) {
		//    pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
		ce_free(mem_n);
		return status;
	}

	*coprime = (status == CRYPTOLIB_SUCCESS);
	ce_free(mem_n);
	return CRYPTOLIB_SUCCESS;
}

uint32_t converge_to_prime(void *device,
						   block_t *candi_number,
						   uint32_t rounds)
{
	uint32_t mod = 0;
	uint8_t incr = 0;
	uint32_t i;
	uint32_t status;
	bool possibly_prime;
	bool coprime;

	/* Make the number odd */
	*(candi_number->addr + candi_number->len - 1) |= 0x01;
	/* Compute the number's modulo when divided by the product of the first 8 odd
	 * prime numbers (on 32 bits)
	 */
	status = get_prime_modulo(device, candi_number->addr, candi_number->len, &mod);

	if (status) {
		//    pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
		return status;
	}

	/* Test candidates for primality until a prime is found
	 * candidate = number + incr (number is odd)
	 * mod = candidate % prod_first_8_odd_primes
	 */
	while (1) {
		possibly_prime = true;

		/* Step 1:
		 * Sieve based on the modulo: check if it is a multiple of 1 of the first
		 * 8 odd prime numbers. If that is the case the number can't be prime.
		 *
		 */
		for (i = 0; i < sizeof(first_odd_primes); i++) {
			if ((mod % first_odd_primes[i]) == 0) {
				possibly_prime = false;
				break;
			}
		}

		if (possibly_prime) {
			/* Add a small increment (incr) to the number (constant time) with
			 * incr <=40 (because we will always find a number that is not a
			 * multiple of the 8 first primes within 20 tries - empirically tested)
			 */
			math_array_incr(candi_number->addr, candi_number->len, incr); //令candi_number每一位 += incr；但incr = 0
			incr = 0;
			/* Step 2:
			 * Check coprimality between the number and the product of the first
			 * 130 odd prime numbers (on 1024 bits)
			 */
			coprime = false;
			status = is_coprime(device, candi_number->addr, candi_number->len, &coprime);

			if (status) {
				//    pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
				return status;
			}

			if (!coprime) {
				possibly_prime = false;
			}
		}

		if (possibly_prime) {
			/* Step 3:
			 * Perform Miller-Rabin primality test on the number
			 */
			bool prime = false;
			status = prime_verify_rsa(device, candi_number, rounds, &prime);

			if (status) {
				//    pr_err("%s, %d, status: 0x%x\n", __func__, __LINE__, status);
				return status;
			}

			/* If the number is prime, stop searching */
			if (prime) {
				break;
			}
		}

		/* If the number is not prime, try again, incrementing the number and the
		 * corresponding modulo by 2.
		 */
		incr += 2;
		mod += 2;

		if (mod >= prod_first_8_odd_primes) {
			mod -= prod_first_8_odd_primes;
		}
	}

	return CRYPTOLIB_SUCCESS;
}

#if 0
uint32_t generate_prime(uint8_t *prime, uint32_t bitsize, uint32_t mr_rounds,
						struct sx_rng rng)
{
	uint32_t byte_length = (bitsize + 7) / 8;
	uint32_t unaligned_bits = bitsize % 8;

	rng.get_rand_blk(rng.param, block_t_convert(prime, byte_length));
	/* Force most significant bit to always have a random starting point of
	 * maximal length. Shift right to reduce the bit size in case the requested
	 * prime is not aligned on 8 bit.
	 */
	prime[0] |= 0x80;

	if (unaligned_bits) {
		prime[0] >>= 8 - unaligned_bits;
	}

	uint32_t status = converge_to_prime(prime, byte_length, mr_rounds, rng);
	return status;
}
#endif
