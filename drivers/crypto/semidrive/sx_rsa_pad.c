/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include "sx_hash.h"
#include "sx_rsa_pad.h"
#include "sx_errors.h"
#include "sx_pke_conf.h"
#include "sx_trng.h"
#include "ce.h"

static const __attribute__((aligned(0x4))) uint8_t zeros[32] = {0};

static const __attribute__((aligned(CACHE_LINE))) uint8_t L_default_str[] = "semidrive";


struct L_s {
	uint32_t len;
	uint8_t data[508];          // sizeof(L_s)is 512
} __attribute__((aligned(CACHE_LINE)));

static struct L_s oaep_L;

void oaep_L_init(void)
{
	oaep_L.len = sizeof(L_default_str);
	memcpy(oaep_L.data, L_default_str, oaep_L.len);
}

uint32_t oaep_L_get(block_t *tmp)
{
	if (block_t_check_wrongful(tmp)) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	tmp->len = oaep_L.len;
	memcpy(tmp->addr, oaep_L.data, oaep_L.len);
	return 0;
}

uint32_t oaep_L_set(block_t *tmp)
{
	if (block_t_check_wrongful(tmp) || tmp->len == 0) {
		pr_err("%s, %d, parameter input is wrongful\n", __func__, __LINE__);
		return CRYPTOLIB_INVALID_PARAM;
	}

	oaep_L.len = tmp->len;
	memcpy(oaep_L.data, tmp->addr, oaep_L.len);

	return 0;
}

/**
 * @brief Get random bytes
 * @param vce_id    vce index
 * @param dst pointer to output result
 * @param n size of \p dst
 * @param rem_zeros remove zeroes if set.
*/
static void gen_rnd_bytes(void *device,
						  uint8_t *dst,
						  size_t n,
						  uint32_t rem_zeros)
{
#if !WITH_SIMULATION_PLATFORM  //rng not work on paladium
	trng_get_rand_by_fifo(device, dst, n);

	if (rem_zeros) {
		//Remove zeros (not perfect for entropy)
		for (; n > 0; n--) if (dst[n - 1] == 0x00) {
				dst[n - 1] = 0x01;
			}
	}

#else
	memset(dst, 0x1, n);
#endif
}

uint32_t rsa_pad_add_none(uint8_t *from, uint32_t flen, uint32_t tlen)
{
	if (flen != tlen) {
		pr_err("rsa flen and tlen are not equal\n");
		return CRYPTOLIB_INVALID_PARAM;
	}

	return 0;
}

/**
 * @brief Get first mask
 * @param n0 input
 * @return mask corresponding to \p n0
*/
static uint32_t getFirstMask(uint32_t n0)
{
	if (n0 & 0x80) {
		return 0x7F;
	}
	else if (n0 & 0x40) {
		return 0x3F;
	}
	else if (n0 & 0x20) {
		return 0x1F;
	}
	else if (n0 & 0x10) {
		return 0x0F;
	}
	else if (n0 & 0x08) {
		return 0x07;
	}
	else if (n0 & 0x04) {
		return 0x03;
	}
	else if (n0 & 0x02) {
		return 0x01;
	}
	else {
		return 0x00;
	}
}

/**
 * @brief perform buff = buff XOR mask
 * @param buff input/output buffer
 * @param mask to xor with
 * @param n size of \p buff and \p mask
*/
static void mask(uint8_t *buff, uint8_t *mask, size_t n)
{
	for (; n > 0; n--) {
		buff[n - 1] = buff[n - 1] ^ mask[n - 1];
	}
}

/**
 * Mask Generation Function as defined by RFC-8017 B.2.1.
 * @param vce_id    vce index
 * @param hashType hash function to use
 * @param seed input
 * @param seedLen length of \p seed
 * @param mask output
 * @param maskLen length of \p mask
*/
static void MGF1(void *device,
				 hash_alg_t hashType,
				 uint8_t *seed,
				 size_t seedLen,
				 uint8_t *mask,
				 size_t maskLen)
{
	uint8_t cnt[8] = {0}; //todo: reduce size (get warning "stack protector not protecting function: all local arrays are less than 8 bytes long")
	uint32_t c = 0;
	uint32_t hashLen;
	uint8_t *hash_in;
	uint8_t *hash_res;
	struct mem_node *mem_n;
	struct mem_node *mem_n_1;
	uint32_t res = 0;
	block_t iv ;
	block_t data_in ;
	block_t data_out ;

	mem_n = ce_malloc(68 + 28);     //>>

	if (mem_n != NULL) {
		hash_in = mem_n->ptr;
		memset(hash_in, 0, 68 + 28);
	}
	else {
		return;
	}

	mem_n_1 = ce_malloc(68 + 28);     //>>

	if (mem_n_1 != NULL) {
		hash_res = mem_n_1->ptr;
	}
	else {
		ce_free(mem_n);
		return;
	}

	hashLen = hash_get_digest_size(hashType);

	memcpy(hash_in, seed, seedLen);

	while (maskLen > 0) {
		cnt[0] = ((c >> 24)  & 0xFF); // note: modifying cnt content via array_blk[1].addr to make static analyzer happy
		cnt[1] = ((c >> 16)  & 0xFF);
		cnt[2] = ((c >> 8)   & 0xFF);
		cnt[3] = ((c)        & 0xFF);
		memcpy(hash_in + seedLen, cnt, 4);
		iv = BLOCK_T_CONV(NULL, 0, 0);
		data_in =  BLOCK_T_CONV(hash_in, seedLen + 4, EXT_MEM);

		if (maskLen >= hashLen) {
			data_out =  BLOCK_T_CONV(mask, hashLen, EXT_MEM);
		}
		else {
			data_out =  BLOCK_T_CONV(hash_res, hashLen, EXT_MEM);
		}

		res = hash_blk(device, hashType, &iv, &data_in, &data_out);

		if (CRYPTOLIB_SUCCESS != res) {
			pr_err("%s, %d, res is %d\n", __func__, __LINE__, res);
//            dprintf(CRITICAL, "generate mask hash fail in MGF1.\n");
//            printf("generate mask hash fail in MGF1.\n");
		}

		if (maskLen < hashLen) {
			memcpy(mask, hash_res, maskLen);
		}

		c++;
		maskLen -= hashLen >= maskLen ? maskLen : hashLen;
		mask += hashLen;
	}

	ce_free(mem_n);
	ce_free(mem_n_1);
}

/* message len <= key len -2 * hlen -2 */
uint32_t rsa_pad_eme_oaep_encode(void *device,
								 uint8_t *EM,
								 uint32_t k,
								 block_t *message,
								 uint32_t mLen,
								 uint8_t *param,
								 uint32_t plen,
								 hash_alg_t data_hashType,
								 hash_alg_t mgf_hash_Type)
{
	uint8_t *dbMask;
	struct mem_node *mem_n;
	uint8_t *seedMask;
	struct mem_node *mem_n_1;

	uint32_t hLen = hash_get_digest_size(data_hashType);

	uint32_t status;
	block_t iv = BLOCK_T_CONV(NULL, 0, 0);
	// block_t data_in = BLOCK_T_CONV(oaep_L.data, oaep_L.len, EXT_MEM);
	block_t data_in = BLOCK_T_CONV(param, plen, EXT_MEM);
	block_t data_out = BLOCK_T_CONV(EM + hLen + 1, hLen, EXT_MEM);


	if (mLen + 2 * hLen + 2 > k) {
		return CRYPTOLIB_INVALID_PARAM;    // MESSAGE_TOO_LONG;
	}

	// get label hash -> no label so NULL_blk
	status = hash_blk(device, data_hashType, &iv, &data_in, &data_out);

	if (status != CRYPTOLIB_SUCCESS) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	//Assemply of DB
	gen_rnd_bytes(device, EM + 1, hLen, 0);

	EM[0] = 0x00;
	memset((uint8_t *)(EM + 2 * hLen + 1), 0x00, k - mLen - 2 * hLen - 2);
	*((uint8_t *)(EM + k - mLen - 1)) = 0x01;
	// *((uint8_t *)(EM + k - mLen - hLen - 1)) = 0x01;
	memcpy(EM + k - mLen, message->addr, mLen);

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		dbMask = mem_n->ptr;
	}
	else {
		pr_err("%s, %d, ce_malloc is wrong\n", __func__, __LINE__);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	mem_n_1 = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n_1 != NULL) {
		seedMask = mem_n_1->ptr;
	}
	else {
		ce_free(mem_n);
		pr_err("%s, %d, ce_malloc is wrong\n", __func__, __LINE__);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	MGF1(device, mgf_hash_Type, EM + 1, hLen, dbMask, k - hLen - 1);
	mask(EM + 1 + hLen, dbMask, k - hLen - 1);
	MGF1(device, mgf_hash_Type, EM + 1 + hLen, k - hLen - 1, seedMask, hLen);
	mask(EM + 1, seedMask, hLen);

	ce_free(mem_n);
	ce_free(mem_n_1);
	return CRYPTOLIB_SUCCESS;
}

uint32_t rsa_pad_eme_oaep_decode(void *device,
								 uint32_t k,
								 hash_alg_t hashType,
								 uint8_t *EM,
								 uint8_t **message,
								 size_t *mLen)
{
	uint32_t status;
	size_t hLen;
	uint8_t *dbMask = NULL;
	struct mem_node *mem_n;
	uint8_t *seedMask = NULL;
	struct mem_node *mem_n_1;
	struct mem_node *mem_n_2;
	uint8_t *LHash = NULL;
	block_t iv;
	block_t data_in;
	block_t data_out;
	size_t i = 0;
	int chkLHash = 0;

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		dbMask = mem_n->ptr;
		memset(dbMask, 0, RSA_MAX_SIZE);
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	mem_n_1 = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n_1 != NULL) {
		seedMask = mem_n_1->ptr;
		memset(seedMask, 0, RSA_MAX_SIZE);
	}
	else {
		ce_free(mem_n);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	mem_n_2 = ce_malloc(MAX_DIGESTSIZE);

	if (mem_n_2 != NULL) {
		LHash = mem_n_2->ptr;
		memset(LHash, 0, RSA_MAX_SIZE);
	}
	else {
		ce_free(mem_n);
		ce_free(mem_n_1);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	hLen = hash_get_digest_size(hashType);

	MGF1(device, hashType, EM + hLen + 1, k - hLen - 1, seedMask, hLen);
	mask(EM + 1, seedMask, hLen);
	MGF1(device, hashType, EM + 1, hLen, dbMask, k - hLen - 1);
	mask(EM + hLen + 1, dbMask, k - hLen - 1);

	*mLen = 0;

	for (i = 2 * hLen + 1; i < k; i++) {
		if (*(uint8_t *)(EM + i) == 0x01) {
			*mLen = k - i - 1;
			break;
		}
	}


	iv = BLOCK_T_CONV(NULL, 0, 0);
	// data_in = BLOCK_T_CONV(oaep_L.data, oaep_L.len, EXT_MEM);
	data_in = BLOCK_T_CONV(NULL, 0, EXT_MEM);
	data_out = BLOCK_T_CONV(LHash, hLen, EXT_MEM);
	status = hash_blk(device, hashType, &iv, &data_in, &data_out);

	if (status != CRYPTOLIB_SUCCESS) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		ce_free(mem_n_2);
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	for (i = 0;  i < hLen; i++) {
		if (LHash[i] != EM[hLen + 1 + i]) {
			chkLHash = 1;
			pr_err("L is mismatch\n");
			status = CRYPTOLIB_PK_SIGNATURE_NOTVALID;
			break;
		}
	}

	ce_free(mem_n);
	ce_free(mem_n_1);
	ce_free(mem_n_2);
	*message = (uint8_t *)((EM + k) - *mLen);

	return status;
}

/* message len <= key len -11 */
uint32_t rsa_pad_eme_pkcs_encode(void *device,
								 uint8_t *to,
								 uint32_t tlen,
								 uint8_t *from,
								 uint32_t flen,
								 rsa_key_types_t key_type)
{
	if (flen > (tlen - RSA_PKCS1_PADDING_SIZE)) {
		return CRYPTOLIB_CRYPTO_ERR;    // MESSAGE_TOO_LONG;
	}

	if (key_type == RSA_PUB_KEY) {
		to[0] = 0x00;
		to[1] = 0x02;
		gen_rnd_bytes(device, to + 2, tlen - flen - 2, 1); //PS (first written for alignment purpose)
	}
	else {
		to[0] = 0x00;
		to[1] = 0x01;
		memset(to + 2, 0xff, tlen - flen - 2);
	}

	//Assemply of DB

	*((uint8_t *)(to + tlen - flen - 1)) = 0x00;
	memcpy(to + tlen - flen, from, flen);

	return CRYPTOLIB_SUCCESS;
}

uint32_t rsa_pad_eme_pkcs_decode(uint32_t k,
								 uint8_t *EM,
								 uint8_t **message,
								 uint32_t *mLen,
								 rsa_key_types_t key_type)
{
	size_t i = 0;
	size_t PSLen = 0;
	uint32_t BT = key_type == RSA_PUB_KEY ? 0x1 : 0x2;
	*mLen = 0;

	for (i = 2; i < k; i++) {
		if (*(uint8_t *)(EM + i) == 0x00) {
			*mLen = k - i - 1;
			break;
		}
	}

	PSLen = k - 3 - *mLen;

	if (*mLen == 0 || EM[0] || EM[1] != BT || PSLen < 8) {
		return CRYPTOLIB_CRYPTO_ERR;    // DECRYPTION_ERROR;
	}

	*message = (uint8_t *)((EM + k) - *mLen);

	return CRYPTOLIB_SUCCESS;
}

uint32_t rsa_pad_emsa_pkcs_encode(void *device,
								  uint32_t emLen,
								  hash_alg_t hash_type,
								  uint8_t *EM,
								  uint8_t *hash)
{
	uint8_t hashTypeDer[19] = {0x30, 0x21, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x2, 0x06, 0x05, 0x00, 0x04, 0x20};
	size_t tLen;
	size_t hLen;
	size_t dLen = 19;

	switch (hash_type) { //adapt DER encoded hashType
		case ALG_SHA1: //SHA1
			hLen = 20;
			dLen = 15;
			hashTypeDer[3] = 0x09;
			hashTypeDer[5] = 0x05;
			hashTypeDer[6] = 0x2b;
			hashTypeDer[7] = 0x0e;
			hashTypeDer[8] = 0x03;
			hashTypeDer[9] = 0x02;
			hashTypeDer[10] = 0x1a;
			hashTypeDer[11] = 0x05;
			hashTypeDer[12] = 0x00;
			hashTypeDer[13] = 0x04;
			hashTypeDer[14] = 0x14;
			break;

		case ALG_SHA224: //SHA224
			hLen = 28;
			hashTypeDer[1] = 0x2d;
			hashTypeDer[14] = 0x04;
			hashTypeDer[18] = 0x1c;
			break;

		case ALG_SHA256: //SHA256
			hLen = 32;
			hashTypeDer[1] = 0x31;
			hashTypeDer[14] = 0x01;
			hashTypeDer[18] = 0x20;
			break;

		case ALG_SHA384: //SHA384
			hLen = 4 * 12;
			hashTypeDer[1] = 0x41;
			hashTypeDer[14] = 0x02;
			hashTypeDer[18] = 0x30;
			break;

		case ALG_SHA512: //SHA512
			hLen = 4 * 16;
			hashTypeDer[1] = 0x51;
			hashTypeDer[14] = 0x03;
			hashTypeDer[18] = 0x40;
			break;

		default:
			pr_err("in %d, %s\n", __LINE__, __func__);
			return CRYPTOLIB_CRYPTO_ERR; // HASH_TYPE_ERROR;
	}

	tLen = hLen + dLen;

	if (emLen < tLen + 11) {
		pr_err("in %d, %s\n", __LINE__, __func__);
		return CRYPTOLIB_CRYPTO_ERR;    // MESSAGE_TOO_SHORT;
	}

	*EM = 0x00;
	*(uint8_t *)(EM + 1) = 0x01;
	memset((uint8_t *)(EM + 2), 0xff, emLen - tLen - 3); //PS
	*(uint8_t *)(EM + emLen - tLen - 1) = 0x00;
	memcpy(EM + emLen - tLen, hashTypeDer, dLen); //HashType
	memcpy(EM + emLen - hLen, hash, hLen); //Hash

	return CRYPTOLIB_SUCCESS;
}

uint32_t rsa_pad_emsa_pss_encode(void *device,
								 uint32_t emLen,
								 hash_alg_t hashType,
								 uint8_t *EM,
								 uint8_t *hash,
								 uint32_t n0,
								 size_t sLen)
{
	uint32_t status;
	uint8_t *dbMask;
	struct mem_node *mem_n;
	uint8_t *tempm;
	struct mem_node *mem_n_1;
	size_t hLen;
	block_t iv;
	block_t data_in ;
	block_t data_out ;
	hLen = hash_get_digest_size(hashType);

	if (!hLen) {
		return CRYPTOLIB_CRYPTO_ERR;    // HASH_TYPE_ERROR;
	}

	if (emLen < sLen + hLen + 2) {
		return CRYPTOLIB_CRYPTO_ERR;    // ENCODING_ERROR;
	}

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		dbMask = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	mem_n_1 = ce_malloc(128);

	if (mem_n_1 != NULL) {
		tempm = mem_n_1->ptr;
	}
	else {
		ce_free(mem_n);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	memset(EM, 0x00, emLen - sLen - hLen - 2); //PS
	gen_rnd_bytes(device, (uint8_t *)(EM + emLen - sLen - hLen - 1), sLen, 0); //Salt
	*((uint8_t *)(EM + emLen - sLen - hLen - 2)) = 0x01; //0x01

	memcpy(tempm, zeros, 8);
	memcpy(tempm + 8, hash, hLen);
	memcpy(tempm + 8 + hLen, (EM + emLen - (sLen + hLen) - 1), sLen);
	iv = BLOCK_T_CONV(NULL, 0, 0);
	data_in = BLOCK_T_CONV(tempm, 8 + hLen + sLen, EXT_MEM);
	data_out = BLOCK_T_CONV((uint8_t *)(EM + emLen - hLen - 1), hLen, EXT_MEM);
	status = hash_blk(device, hashType, &iv, &data_in, &data_out) ;

	if (status != CRYPTOLIB_SUCCESS) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		return CRYPTOLIB_CRYPTO_ERR;    // ENCODING_ERROR;
	}

	MGF1(device, hashType, EM + emLen - hLen - 1, hLen, dbMask, emLen - hLen - 1);
	mask(EM, dbMask, emLen - hLen - 1);

	EM[emLen - 1] = 0xBC;
	EM[0] = EM[0] & getFirstMask(n0);

	ce_free(mem_n);
	ce_free(mem_n_1);
	return 0;
}

/* Steps from rfc8017 9.1.2 Verification operation */
uint32_t rsa_pad_emsa_pss_decode(void *device,
								 uint32_t emLen,
								 hash_alg_t hashType,
								 uint8_t *EM,
								 uint8_t *hash,
								 uint32_t sLen,
								 uint32_t n0)
{
	uint8_t *maskedDB;
	uint32_t dbLen;
	uint8_t *H;
	uint32_t mlen;
	uint8_t *salt;
	uint8_t *H_;
	uint32_t status;
	int chkLHash;
	uint8_t *dbMask;
	struct mem_node *mem_n;
	uint8_t *hash_in;
	struct mem_node *mem_n_1;
	size_t hLen;
	block_t iv;
	block_t data_in ;
	block_t data_out ;
	size_t i = 1;
	uint8_t *DB = NULL;
	hLen = hash_get_digest_size(hashType);

	/* 3.  If emLen < hLen + sLen + 2, output "inconsistent" and stop. */
	/* 4.  If the rightmost octet of EM does not have hexadecimal value
	    0xbc, output "inconsistent" and stop. */
	if (emLen < hLen + sLen + 2 || EM[emLen - 1] != 0xbc) {
		return CRYPTOLIB_CRYPTO_ERR;    // INCONSISTENT;
	}

	/* 5.  Let maskedDB be the leftmost emLen - hLen - 1 octets of EM, and
	    let H be the next hLen octets. */
	maskedDB = EM;
	dbLen = emLen - hLen - 1;
	H = EM + emLen - hLen - 1;

	//FIXME: This check is not performed because it fails a few known-good test cases.
	/* 6.   If the leftmost 8emLen - emBits bits of the leftmost octet in
	        maskedDB are not all equal to zero, output "inconsistent" and
	        stop. */

	/* 7.  Let dbMask = MGF(H, emLen - hLen - 1). */
	//This buffer is later reused to store H'

	mlen = emLen - hLen - 1;

	//mlen = mlen + ((mlen % hLen) ? hLen - (mlen % hLen) : 0);
	if (mlen > RSA_MAX_SIZE) {
//       dprintf(CRITICAL, "size of mask is too big.\n");
//        printf("size of mask is too big.\n");
		return CRYPTOLIB_CRYPTO_ERR;
	}

	mem_n = ce_malloc(RSA_MAX_SIZE);

	if (mem_n != NULL) {
		dbMask = mem_n->ptr;
	}
	else {
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	MGF1(device, hashType, H, hLen, dbMask, mlen);

	/* 8.  Let DB = maskedDB \xor dbMask. */
	mask(maskedDB, dbMask, emLen - hLen - 1);
	//from here maskedDB = RFC's DB
	DB = maskedDB;
	/* 9.  Set the leftmost 8emLen - emBits bits of the leftmost octet in DB
	       to zero. */
	DB[0] = DB[0] & getFirstMask(n0);

	/* 10. If the emLen - hLen - sLen - 2 leftmost octets of DB are not zero
	    or if the octet at position emLen - hLen - sLen - 1 (the leftmost
	    position is "position 1") does not have hexadecimal value 0x01,
	    output "inconsistent" and stop. */
	for (i = 1; i < (emLen - hLen - sLen - 2); i++) {
		if (*(uint8_t *)(DB + i)) {
			return CRYPTOLIB_CRYPTO_ERR; // INCONSISTENT; //Check padding 0x00
		}
	}

	if (*(uint8_t *)(DB + emLen - hLen - sLen - 2) != 0x01) {
		return CRYPTOLIB_CRYPTO_ERR; // INCONSISTENT;
	}

	/* 11.  Let salt be the last sLen octets of DB. */
	salt = DB + dbLen - sLen;

	/* 12.  Let M' = (0x)00 00 00 00 00 00 00 00 || mHash || salt ;
	    M' is an octet string of length 8 + hLen + sLen with eight
	    initial zero octets. */

	mem_n_1 = ce_malloc(128);

	if (mem_n_1 != NULL) {
		hash_in = mem_n_1->ptr;
	}
	else {
		ce_free(mem_n);
		return CRYPTOLIB_PK_N_NOTVALID;
	}

	memcpy(hash_in, zeros, 8);
	memcpy(hash_in + 8, hash, hLen);
	memcpy(hash_in + 8 + hLen, salt, sLen);

	/* 13. Let H' = Hash(M'), an octet string of length hLen. */
	//Hash is placed in the unused big buffer. First byte of EM 1 byte to small.
	H_ = dbMask;
	iv = BLOCK_T_CONV(NULL, 0, 0);
	data_in = BLOCK_T_CONV(hash_in, 8 + hLen + sLen, EXT_MEM);
	data_out = BLOCK_T_CONV(H_, hLen, EXT_MEM);
	status = hash_blk(device, hashType, &iv, &data_in, &data_out);

	if (status != CRYPTOLIB_SUCCESS) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		return CRYPTOLIB_CRYPTO_ERR;
	}

	/* 14. If H = H', output "consistent." Otherwise, output "inconsistent." */
	chkLHash = memcmp(H, H_, hLen);

	if (chkLHash) {
		ce_free(mem_n);
		ce_free(mem_n_1);
		return CRYPTOLIB_CRYPTO_ERR; // INCONSISTENT;
	}

	ce_free(mem_n);
	ce_free(mem_n_1);
	return CRYPTOLIB_SUCCESS;
}


void rsa_pad_zeros(uint8_t *EM,
				   size_t emLen,
				   uint8_t *hash,
				   size_t hashLen)
{
	memset(EM, 0x00, emLen - hashLen);

	if (hashLen) {
		memcpy(EM + emLen - hashLen, hash, hashLen);
	}
}
