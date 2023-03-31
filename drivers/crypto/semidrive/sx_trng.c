/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include "sx_trng.h"
#include "sx_errors.h"
#include "sx_ecc_keygen.h"
#include "sx_pke_conf.h"

#define LOCAL_TRACE 0 //close local trace 1->0

#define RUN_WITH_NON_INNER_STATUS 1 // trng init run with non inner status

uint32_t trng_get_rand(void *device, uint8_t *dst, uint32_t size)
{
	return trng_get_rand_by_fifo(device, dst, size);
}

uint32_t trng_get_rand_blk(void *device, block_t *dest)
{
	return trng_get_rand(device, dest->addr, dest->len);
}

uint32_t trng_get_rand_by_fifo(void *device, uint8_t *dst, uint32_t size)
{
	uint32_t read_cnt = 256;         //16
	void __iomem *ce_base = ((struct crypto_dev *)device) ->base;
	uint32_t vce_id = ((struct crypto_dev *)device)->ce_id;
	uint32_t rng_value;
	uint32_t last_rng_value;
	addr_t baddr;
	uint32_t read_size = 0;
	uint32_t i = 0;

	if (!dst) {
		return CRYPTOLIB_INVALID_PARAM;
	}


	do {
		baddr = REG_HRNG_NUM_CE_(vce_id);

		for (i = 0; i < read_cnt; i++) {
			if (size <= read_size) {
				return CRYPTOLIB_SUCCESS;
			}

			rng_value = readl(ce_base + baddr);

			while (rng_value == last_rng_value) {
				continue;
			}

			last_rng_value = rng_value;
			memcpy(dst, (void *)(&rng_value), 4);
			read_size += 4;
			dst += 4;
		}
	}
	while (read_size < size);

	return CRYPTOLIB_UNSUPPORTED_ERR;
}

uint32_t rng_get_rand_less_ref(void *device, block_t *dst, block_t *n)
{
	uint32_t index = 0;
	uint8_t msb_mask = 0xFF;
	uint32_t i;
	block_t rnd;
	bool gt = false;
	bool eq = true;
	bool cmp = true;
	uint32_t leftop = 0;
	uint32_t rightop = 0;

	if (dst->len != n->len) {
		return CRYPTOLIB_INVALID_PARAM;
	}

	if (!(n->addr[n->len - 1] & 0x01)) { /*n must be odd*/
		return CRYPTOLIB_INVALID_PARAM;
	}

	/* Check what is the most significant bit of n and compute an index of and a
	 * mask for the most significant byte that can be used to remove the leading
	 * zeros.
	 */

	/*since n is odd, at minimum the
	 *least significant byte should be
	 *different from 0
	*/
	for (; !n->addr[index]; index++);

	for (; n->addr[index] & msb_mask; msb_mask <<= 1);

	/* Create container for random value from RNG, pointing to the same buffer as
	 * dst but referring only to [MSB-1:0] instead of [len-1:0].
	 * Force the leading, non-significant bytes to zero.
	 */
	memset(dst->addr, 0, index);
	rnd = block_t_convert(dst->addr + index, dst->len - index, EXT_MEM);

	do {
		/* Get a random value */
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
		dst->addr[index] &= ~msb_mask; /*Note that dst.addr[index] = rnd.addr[0]*/

		/* Check if rnd > n-2 (constant time comparison) */
		gt = false;
		eq = true;
		cmp = true ;
		leftop = 0;
		rightop = 0;

		for (i = 0; cmp ; i++) {
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
			 *
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
			//    gt |= (bool)((rightop - leftop) >> 8) & eq;     //为0则随机数小
			eq &= (bool)(((rightop ^ leftop) - 1) >> 8);    //为1则相等
			cmp = (i < dst->len) & eq;
		}

		if (eq) {
			continue;
		}

		gt = (bool)((rightop - leftop) >> 8) ;

		if (gt == 0) {
			break;
		}
	}
	while (1);

	/* Compute k = rnd + 1 (constant time increment) */
	math_array_incr(dst->addr, dst->len, 1);

	return CRYPTOLIB_SUCCESS;

}
