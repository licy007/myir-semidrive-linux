/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include "sx_ecc_keygen.h"
//#include "sx_primitives.h"
#include "sx_dma.h"
#include "sx_errors.h"
#include "sx_trng.h"

void math_array_incr(uint8_t *a,
					 const size_t length,
					 int8_t value)
{
	int i;
	int32_t carry = value;

	/* The LSB are at the end of the array so start there. */
	for (i = length - 1; i >= 0; i--) {
		int32_t byte = a[i];
		int32_t sum = byte + carry;
		a[i] = (uint8_t)(sum & 0xFF);
		carry = sum >> 8;
	}
}


/* This function implements one of the methods approved by FIPS 186-4 to
 * generate a random number k with 1 <= k < n.
 *
 * Get a random value rnd of the appropriate length.
 * If rnd > n -2
 *    Try another value
 * Else
 *    k = rnd + 1
 */

uint32_t rng_get_rand_lt_n_blk(void *device,
							   block_t *n,
							   block_t *dst
							  )
{
	uint32_t i;
	block_t rnd;

	bool rnd_invalid;

	/* Check what is the most significant bit of n and compute an index of and a
	 * mask for the most significant byte that can be used to remove the leading
	 * zeros.
	 */
	uint32_t index;
	uint8_t msb_mask;
	bool gt = false;
	bool eq = true;
	uint32_t leftop;
	uint32_t rightop;

	if (dst->len != n->len) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (!(n->addr[n->len - 1] & 0x01)) { /*n must be odd*/
		return CRYPTOLIB_INVALID_PARAM;
	}

	/*since n is odd, at minimum the
	 *least significant byte should be
	 *different from 0
	 */
	for (index = 0; !n->addr[index]; index++);

	for (msb_mask = 0xFF; n->addr[index] & msb_mask; msb_mask <<= 1);

	/* Create container for random value from RNG, pointing to the same buffer as
	 * dst but referring only to [MSB-1:0] instead of [len-1:0].
	 * Force the leading, non-significant bytes to zero.
	 */
	memset(dst->addr, 0, index);

	rnd.addr = (dst->addr + index);
	rnd.addr_type = dst->addr_type;
	rnd.len = dst->len - index;

	do {
		/* Get a random value */
		//trng_get_rand_blk(vce_id, rnd);
		trng_get_rand_by_fifo(device, rnd.addr, rnd.len);

		/* Mask off the leading non-significant bits. Keep only the bits that are
		 * relevant according to msb_mask. This is done to speed up the process of
		 * finding a proper random value.
		 * For example:
		 * If the highest byte of n is 0x06, the chance that we get a random with
		 * a highest byte <= 0x06 is only 7/256 without masking.
		 * With the masking process (msb_mask = 0xF8, ~msb_mask = 0x07) we
		 * significantly increase the chances of immediately finding a suitable
		 * random.
		 */
		dst->addr[index] &= ~msb_mask; /*Note that dst->addr[index] = rnd.addr[0]*/

		/* Check if rnd > n-2 (constant time comparison) */
		gt = false;
		eq = true;

		for (i = 0; i < dst->len; i++) {
			leftop = dst->addr[i];
			rightop = n->addr[i];

			/* We rephrase rnd > n-2 as rnd >= n-1. Since n is odd, n-1 is obtained
			 * by masking 1 bit.
			 */
			if (i == dst->len - 1) {
				rightop &= 0xFE;
			}

			/* We use a trick to determine whether leftop >= rightop to avoid
			 * possible time dependency in the implementations of ">", "<" and "==".
			 * If leftop > rightop then (rightop - leftop) will be 0xFFFFFFxx.
			 * If leftop <= rightop then (rightop - leftop) will be 0x000000xx.
			 * By shifting out the xx, we can determine the relation between left
			 * and right.
			 *
			 * A similar trick is used to determine whether leftop == rightop.
			 * If leftop == rightop then (rightop ^ leftop) - 1 will be 0xFFFFFFFF.
			 * If leftop != rightop then (rightop ^ leftop) - 1 will be 0x000000xx.
			 *
			 * By muxing eq with eq, we ensure that eq will be zero from the first
			 * different byte onwards.
			 * By muxing the leftop >= rightop check with eq, we ensure that it
			 * only has an effect when executed on the first most significant byte
			 * that is different between the arrays.
			 */
			gt |= (bool)((rightop - leftop) >> 8) & eq;
			eq &= (bool)(((rightop ^ leftop) - 1) >> 8);
		}

		rnd_invalid = gt | eq;

	}
	while (rnd_invalid);

	/* Compute k = rnd + 1 (constant time increment) */
	math_array_incr(dst->addr, dst->len, 1);

	return CRYPTOLIB_SUCCESS;
}

uint32_t ecc_curve_bytesize(const sx_ecc_curve_t *curve)
{
	return curve->bytesize;
}

uint32_t ecc_validate_public_key(void *device,
								 const block_t *domain,
								 block_t *pub,
								 uint32_t size,
								 uint32_t curve_flags)
{
	uint32_t status;

	pke_set_command(device, BA414EP_OPTYPE_ECC_CHECK_POINTONCURVE, size, BA414EP_LITTLEEND, curve_flags);
	pke_load_curve(device, domain, size, BA414EP_LITTLEEND, 1);
	point2CryptoRAM_rev(device, pub, size, BA414EP_MEMLOC_6);

	pke_set_config(device, BA414EP_MEMLOC_6, BA414EP_MEMLOC_6, BA414EP_MEMLOC_6, 0x0);
	status = pke_start_wait_status(device);

	if (status) {
		return status;
	}

	return CRYPTOLIB_SUCCESS;
}

uint32_t ecc_generate_keypair(void *device,
							  const block_t *group,
							  block_t *pub,
							  block_t *n,
							  block_t *priv,
							  uint32_t size,
							  uint32_t curve_flags)
{
	// Get random number < n -> private key
	//block_t n = BLOCK_T_CONV(domain.addr + size, size, domain.addr_type);
	uint32_t status = ecc_generate_private_key(device, n, priv);

	if (status) {
		return status;
	}

	//Point mult for Public key
	return ecc_generate_public_key(device, group, pub, priv, size, curve_flags);
}

uint32_t ecc_generate_private_key(void *device,
								  block_t *n,
								  block_t *priv)
{
	return rng_get_rand_lt_n_blk(device, n, priv);
}

uint32_t ecc_generate_public_key(void *device,
								 const block_t *group,
								 block_t *pub,
								 block_t *priv,
								 uint32_t size,
								 uint32_t curve_flags)
{
	uint32_t status;

	// Only domain of 5,6 parameters are supported (Weierstrass p/b and Edwards)
	if (pub->len != 2 * size || priv->len != size ||
			(group->len != 6 * size && group->len != 5 * size)) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	// Set command to enable byte-swap
	pke_set_command(device, BA414EP_OPTYPE_ECC_POINT_MULT, size, BA414EP_LITTLEEND, curve_flags);

	// Load parameters
	pke_load_curve(device, group, size, BA414EP_LITTLEEND, 1);

	// Location 14 -> Private key
	mem2CryptoRAM_rev(device, priv, priv->len, BA414EP_MEMLOC_14, true);

	/* Set Configuration register */
	pke_set_config(device, BA414EP_MEMLOC_2, BA414EP_MEMLOC_14, BA414EP_MEMLOC_6, 0x0);

	// Fetch the results by PKE config
#if AUTO_OUTPUT_BY_CE
	pke_set_dst_param(device, size, 0x1 << BA414EP_MEMLOC_6 | 0x1 << BA414EP_MEMLOC_7, (_paddr((void *)pub->addr)), pub->addr_type);
#endif
	/* Start ECC Point Mult */
	status = pke_start_wait_status(device);

	if (status) {
		return CRYPTOLIB_CRYPTO_ERR;
	}

	// Fetch the results by manual
#if AUTO_OUTPUT_BY_CE
#else
	CryptoRAM2point_rev(device, pub, size, BA414EP_MEMLOC_6);
#endif
	return CRYPTOLIB_SUCCESS;
}
