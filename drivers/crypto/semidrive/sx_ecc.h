/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_ECC
#define SX_ECC

#include "sx_pke_conf.h"

typedef struct sx_ecc_curve_t {
	/**< points to predefined curve params
	   formatted as {q, n, G<SUB>x</SUB>, G<SUB>y</SUB>, a, b} and
	   represented in big number array of uint8 (big endian).
	   The fileds are:
	   - q, the modulus (or the reduction polynomial for ECC Weierstrass binary)
	   - n, the order of the curve
	   - G<SUB>x</SUB>, G<SUB>y</SUB>, the x and y coordinates of the generator
	   - a, b (or d for Edwards curve), the coefficients defining the ECC curve*/
	block_t  params;
	uint32_t pk_flags; /**< required flags for the PK engine*/
	uint32_t bytesize; /**< size expressed in bytes */
} sx_ecc_curve_t;

extern const sx_ecc_curve_t sx_ecc_curve_p192;
extern const sx_ecc_curve_t sx_ecc_curve_p224;
extern const sx_ecc_curve_t sx_ecc_curve_p256;
extern const sx_ecc_curve_t sx_ecc_sm2_curve_p256;
extern const sx_ecc_curve_t sx_ecc_sm2_curve_p256_rev;
extern const sx_ecc_curve_t sx_ecc_curve_p384;
extern const sx_ecc_curve_t sx_ecc_curve_p521;
extern const sx_ecc_curve_t sx_ecc_curve_b233;
extern const sx_ecc_curve_t sx_ecc_curve_e521;

extern const sx_ecc_curve_t sx_ecc_curve_b163;
extern const sx_ecc_curve_t sx_ecc_curve_b283;
extern const sx_ecc_curve_t sx_ecc_curve_b409;
extern const sx_ecc_curve_t sx_ecc_curve_b571;
extern const sx_ecc_curve_t sx_ecc_curve_k163;
extern const sx_ecc_curve_t sx_ecc_curve_k233;
extern const sx_ecc_curve_t sx_ecc_curve_k283;
extern const sx_ecc_curve_t sx_ecc_curve_k409;
extern const sx_ecc_curve_t sx_ecc_curve_k571;

extern const sx_ecc_curve_t sx_ecc_curve_curve192_wrongA;
extern const sx_ecc_curve_t sx_ecc_curve_curve192_wrongGx;
extern const sx_ecc_curve_t sx_ecc_curve_curve192_wrongQ;

extern const sx_ecc_curve_t sx_ecc_curve_p256_iptest;

const void *get_curve_value_by_nid(int nid);

#endif
