/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/kernel.h>

#include "sx_pke_conf.h"
#include "ce_reg.h"
#include "sx_errors.h"
#include "sx_dma.h"
#include "sx_pke_funcs.h"

uint32_t prime_verify(void *device,
					  block_t *ref_prime,
					  block_t *candi_prime,
					  uint32_t iteration,
					  bool *is_prime)
{
	uint32_t i;
	uint32_t status;
	uint32_t composite = 0;

	if (iteration < 1) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_MILLER_RABIN, candi_prime->len, BA414EP_LITTLEEND, 0);

	/* Load verification parameters */
	mem2CryptoRAM_rev(device, ref_prime, ref_prime->len, BA414EP_MEMLOC_0, true);
	mem2CryptoRAM_rev(device, candi_prime, candi_prime->len, BA414EP_MEMLOC_6, true);

	for (i = 0; i < iteration; i++) {
		status = pke_start_wait_status(device);

		if (status && status != CRYPTOLIB_PK_COMPOSITE) {
			return status;
		}

		if (status == CRYPTOLIB_PK_COMPOSITE) {
			composite++;
		}
	}

	*is_prime = (0 == composite);

	return CRYPTOLIB_SUCCESS;
}

uint32_t modular_common(void *device,
						uint32_t op,
						block_t *ptra,
						block_t *ptrb,
						block_t *ptrn,
						block_t *ptrc)
{
	uint32_t status;

	//Set pointer register
	pke_set_config(device, 0, 1, 2, 3);

	// Set command to enable byte-swap
	pke_set_command(device, op, ptra->len, BA414EP_LITTLEEND, 0);

	/* Load verification parameters */
	mem2CryptoRAM_rev(device, ptra, ptra->len, BA414EP_MEMLOC_0, true);
	mem2CryptoRAM_rev(device, ptrb, ptrb->len, BA414EP_MEMLOC_1, true);
	mem2CryptoRAM_rev(device, ptrn, ptrn->len, BA414EP_MEMLOC_3, true);

	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}

	CryptoRAM2mem_rev(device, ptrc, ptrc->len, BA414EP_MEMLOC_2, true);

	return status;
}

/* C= A + B mod N */
uint32_t modular_add(void *device,
					 block_t *ptra,
					 block_t *ptrb,
					 block_t *ptrn,
					 block_t *ptrc)
{
	return modular_common(device, BA414EP_OPTYPE_MOD_ADD, ptra, ptrb, ptrn, ptrc);
}

/* C= A - B mod N */
uint32_t modular_sub(void *device,
					 block_t *ptra,
					 block_t *ptrb,
					 block_t *ptrn,
					 block_t *ptrc)
{
	return modular_common(device, BA414EP_OPTYPE_MOD_SUB, ptra, ptrb, ptrn, ptrc);
}

/* C= A * B mod N */
uint32_t modular_multi(void *device,
					   block_t *ptra,
					   block_t *ptrb,
					   block_t *ptrn,
					   block_t *ptrc)
{
	return modular_common(device, BA414EP_OPTYPE_MOD_MULT_ODD, ptra, ptrb, ptrn, ptrc);
}

/* C= B mod N */
uint32_t modular_reduce(void *device,
						uint32_t op,
						block_t *ptrb,
						block_t *ptrn,
						block_t *ptrc)
{
	uint32_t status;

	//Set pointer register
	pke_set_config(device, 0, 1, 2, 3);

	// Set command to enable byte-swap
	pke_set_command(device, op, ptrb->len, BA414EP_LITTLEEND, 0);

	/* Load verification parameters */
	mem2CryptoRAM_rev(device, ptrb, ptrb->len, BA414EP_MEMLOC_1, true);
	mem2CryptoRAM_rev(device, ptrn, ptrn->len, BA414EP_MEMLOC_3, true);

	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}

	CryptoRAM2mem_rev(device, ptrc, ptrc->len, BA414EP_MEMLOC_2, true);

	return status;
}

/* C= B mod N */
uint32_t modular_reduce_odd(void *device,
							block_t *ptrb,
							block_t *ptrn,
							block_t *ptrc)
{
	return modular_reduce(device, BA414EP_OPTYPE_MOD_RED_ODD, ptrb, ptrn, ptrc);
}

/* C= A / B mod N */
uint32_t modular_div(void *device,
					 block_t *ptra,
					 block_t *ptrb,
					 block_t *ptrn,
					 block_t *ptrc)
{
	return modular_common(device, BA414EP_OPTYPE_MOD_DIV_ODD, ptra, ptrb, ptrn, ptrc);
}

/* C= 1 / B mod N */
uint32_t modular_inverse(void *device,
						 uint32_t op,
						 block_t *ptrb,
						 block_t *ptrn,
						 block_t *ptrc)
{
	uint32_t status;

	//Set pointer register
	pke_set_config(device, 0, 1, 2, 3);

	// Set command to enable byte-swap
	pke_set_command(device, op, ptrb->len, BA414EP_LITTLEEND, 0);

	/* Load verification parameters */
	mem2CryptoRAM_rev(device, ptrb, ptrb->len, BA414EP_MEMLOC_1, true);
	mem2CryptoRAM_rev(device, ptrn, ptrn->len, BA414EP_MEMLOC_3, true);

	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}

	CryptoRAM2mem_rev(device, ptrc, ptrc->len, BA414EP_MEMLOC_2, true);
	return status;
}

/* C= 1 / B mod N */
uint32_t modular_inverse_odd(void *device,
							 block_t *ptrb,
							 block_t *ptrn,
							 block_t *ptrc)
{
	return modular_inverse(device, BA414EP_OPTYPE_MOD_INV_ODD, ptrb, ptrn, ptrc);
}

/* C= sqrt(A) mod N */
uint32_t modular_square_root(void *device,
							 block_t *ptra,
							 block_t *ptrn,
							 block_t *ptrc)
{
	uint32_t status;

	//Set pointer register
	pke_set_config(device, 7, 0, 8, 0);

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_MOD_SQRT, ptra->len, BA414EP_LITTLEEND, 0);

	/* Load verification parameters */
	mem2CryptoRAM_rev(device, ptra, ptra->len, BA414EP_MEMLOC_7, true);
	mem2CryptoRAM_rev(device, ptrn, ptrn->len, BA414EP_MEMLOC_0, true);

	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}

	CryptoRAM2mem_rev(device, ptrc, ptrc->len, BA414EP_MEMLOC_8, true);

	return status;
}

/* C= A * B */
uint32_t multiplicate(void *device,
					  block_t *ptra,
					  block_t *ptrb,
					  block_t *ptrc)
{
	//Set pointer register
	uint32_t status;
	pke_set_config(device, 0, 1, 2, 3);

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_MULT, ptra->len, BA414EP_LITTLEEND, 0);

	/* Load verification parameters */
	mem2CryptoRAM_rev(device, ptra, ptra->len, BA414EP_MEMLOC_0, true);
	mem2CryptoRAM_rev(device, ptrb, ptrb->len, BA414EP_MEMLOC_1, true);

	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}

	CryptoRAM2mem_rev(device, ptrc, ptrc->len, BA414EP_MEMLOC_2, true);

	return status;
}

/* C= 1 / B mod N */
uint32_t modular_inverse_even(void *device,
							  block_t *ptrb,
							  block_t *ptrn,
							  block_t *ptrc)
{
	return modular_inverse(device, BA414EP_OPTYPE_MOD_INV_EVEN, ptrb, ptrn, ptrc);
}

/* C= B mod N */
uint32_t modular_reduce_even(void *device,
							 block_t *ptrb,
							 block_t *ptrn,
							 block_t *ptrc)
{
	return modular_reduce(device, BA414EP_OPTYPE_MOD_RED_EVEN, ptrb, ptrn, ptrc);
}
