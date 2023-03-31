/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/kernel.h>
#include <linux/delay.h>
#include "sx_errors.h"
#include "ce_reg.h"
#include "sx_dma.h"
#include "sx_pke_conf.h"

#define WAIT_PK_WITH_REGISTER_POLLING 1
#define LOCAL_TRACE 0 //close local trace 1->0

#if RSA_PERFORMANCE_TEST
static uint64_t time_slice = 0;
#endif

#if PK_CM_ENABLED
struct sx_rng pk_rng = {
	.param = NULL,
	.get_rand_blk = NULL
};

void dma_set_rng(struct sx_rng rng)
{
	pk_rng = rng;
}
#endif

/** @brief This function starts the given PK
 *  @param device      device pointer
 *  @return The start bit of the CommandReg of the given BA414EP struct has been set
 *  to appropriate value.
 */

/**
 * @brief Function is used to get the full contents of the DMA status register
 * @param device      device pointer
 * @return the contents oft he status register as uint32_t.
 */
static uint32_t pke_get_status(void *device)
{
	uint32_t err;//get low 14 bits
	uint32_t vce_id;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	//pr_info("pke_get_status enter err=%d\n",err);
	err = readl((((struct crypto_dev *)device)->base + REG_PK_STATUSREG_CE_(vce_id))) & 0x3FFF;

	switch (err) {
		case 0:
			return CRYPTOLIB_SUCCESS;

		case CE_PK_NOTQUADRATICRESIDUE_MASK:
			return CRYPTOLIB_PK_NOTQUADRATICRESIDUE;

		case CE_PK_COMPOSITE_MASK:
			return CRYPTOLIB_PK_COMPOSITE;

		case CE_PK_NOTINVERTIBLE_MASK:
			return CRYPTOLIB_PK_NOTINVERTIBLE;

		case CE_PK_PARAM_AB_NOTVALID_MASK:
			return CRYPTOLIB_PK_PARAM_AB_NOTVALID;

		case CE_PK_SIGNATURE_NOTVALID_MASK:
			return CRYPTOLIB_PK_SIGNATURE_NOTVALID;

		case CE_PK_NOTIMPLEMENTED_MASK:
			return CRYPTOLIB_PK_NOTIMPLEMENTED;

		case CE_PK_PARAM_N_NOTVALID_MASK:
			return CRYPTOLIB_PK_N_NOTVALID;

		case CE_PK_COUPLE_NOTVALID_MASK:
			return CRYPTOLIB_PK_COUPLE_NOTVALID;

		case CE_PK_POINT_PX_ATINFINITY_MASK:
			return CRYPTOLIB_PK_POINT_PX_ATINFINITY;

		case CE_PK_POINT_PX_NOTONCURVE_MASK:
			return CRYPTOLIB_PK_POINT_PX_NOTONCURVE;

		case CE_PK_FAIL_ADDRESS_MASK:
			return CRYPTOLIB_PK_FAIL_ADDRESS;

		default:
			return err;
	}

	return err;
}

/** @brief: Function tells if a given Public Key is Busy or not (checking its status register)
 *  @param device      device pointer
 *  @return 1 when given pub key is busy, 0 otherwise
 */
#if WAIT_PK_WITH_REGISTER_POLLING
static int pke_is_busy(void *device)
{
	uint32_t vce_id;
	uint32_t ret;

	vce_id = ((struct crypto_dev *)device)->ce_id;
	ret = readl((((struct crypto_dev *)device)->base + REG_PK_STATUSREG_CE_(vce_id))) & 0x10000;

	return ret;
}
#endif

void pke_set_config(void *device,
					uint32_t PtrA,
					uint32_t PtrB,
					uint32_t PtrC,
					uint32_t PtrN)
{
	uint32_t vce_id;
	uint32_t value;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	value = reg_value(PtrA, 0, CE_PK_OPPTRA_SHIFT, CE_PK_OPPTRA_MASK);
	value = reg_value(PtrB, value, CE_PK_OPPTRB_SHIFT, CE_PK_OPPTRB_MASK);
	value = reg_value(PtrC, value, CE_PK_OPPTRC_SHIFT, CE_PK_OPPTRC_MASK);
	value = reg_value(PtrN, value, CE_PK_OPPTRN_SHIFT, CE_PK_OPPTRN_MASK);

	writel(value, (((struct crypto_dev *)device)->base + REG_PK_POINTERREG_CE_(vce_id)));
}

void pke_set_command(void *device,
					 uint32_t op,
					 uint32_t operandsize,
					 uint32_t swap,
					 uint32_t curve_flags)
{
	uint32_t value = 0x80000000;  //PK_CALCR2
	uint32_t vce_id;
	uint32_t NumberOfBytes;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	if (operandsize > 0) {
		NumberOfBytes = operandsize - 1;
	}
	else {
		NumberOfBytes = 0;
	}

	// Data ram is erased automatically after reset in PK engine.
	// Wait until erasing is finished before writing any data
	// (this routine is called before any data transfer)
#if WAIT_PK_WITH_REGISTER_POLLING

	while (pke_is_busy(device));

#else
	//PK_WAITIRQ_FCT();
#endif

	value = reg_value(op, value, CE_PK_TYPE_SHIFT, CE_PK_TYPE_MASK);
	value = reg_value(NumberOfBytes, value, CE_PK_SIZE_SHIFT, CE_PK_SIZE_MASK);

#if PK_CM_ENABLED

	// Counter-Measures for the Public Key
	if (BA414EP_IS_OP_WITH_SECRET_ECC(op)) {
		// ECC operation
		value = reg_value(BA414EP_CMD_RANDPR(PK_CM_RANDPROJ_ECC), value, CE_PK_RANDPROJ_SHIFT, CE_PK_RANDPROJ_MASK);
		value = reg_value(BA414EP_CMD_RANDKE(PK_CM_RANDKE_ECC), value, CE_PK_RANDKE_SHIFT, CE_PK_RANDKE_MASK);
	}
	else if (BA414EP_IS_OP_WITH_SECRET_MOD(op)) {
		// Modular operations
		value = reg_value(BA414EP_CMD_RANDPR(PK_CM_RANDPROJ_MOD), value, CE_PK_RANDPROJ_SHIFT, CE_PK_RANDPROJ_MASK);
		value = reg_value(BA414EP_CMD_RANDKE(PK_CM_RANDKE_MOD), value, CE_PK_RANDKE_SHIFT, CE_PK_RANDKE_MASK);
	}

#endif
	value = reg_value(swap, value, CE_PK_SWAP_SHIFT, CE_PK_SWAP_MASK);
	//value = reg_value(curve_flags, value, CE_PK_SELCURVE_SHIFT, CE_PK_SELCURVE_MASK);
	value |= curve_flags;

	writel(value, (((struct crypto_dev *)device)->base + REG_PK_COMMANDREG_CE_(vce_id)));
}

void pke_set_dst_param(void *device,
					   uint32_t operandsize,
					   uint32_t operandsel,
					   addr_t dst_addr,
					   ce_addr_type_t dst_type)
{
	uint32_t value;
	uint32_t vce_id;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	value = reg_value(operandsize, 0, CE_PKE_RESULTS_OP_LEN_SHIFT, CE_PKE_RESULTS_OP_LEN_MASK);
	value = reg_value(operandsel, value, CE_PKE_RESULTS_OP_SELECT_SHIFT, CE_PKE_RESULTS_OP_SELECT_MASK);

	writel(value, (((struct crypto_dev *)device)->base + REG_PK_RESULTS_CTRL_CE_(vce_id)));

	if (EXT_MEM == dst_type) {
		writel(_paddr((void *)dst_addr), (((struct crypto_dev *)device)->base + REG_PK_RESULTS_DST_ADDR_CE_(vce_id)));
		//TODO: compatible 64bit address
		value = reg_value(((_paddr((void *)dst_addr)) >> 32), 0, CE_PKE_RESULTS_DST_ADDR_H_SHIFT, CE_PKE_RESULTS_DST_ADDR_H_MASK);
	}
	else {
		writel(dst_addr, (((struct crypto_dev *)device)->base + REG_PK_RESULTS_DST_ADDR_CE_(vce_id)));
		value = reg_value(0, 0, CE_PKE_RESULTS_DST_ADDR_H_SHIFT, CE_PKE_RESULTS_DST_ADDR_H_MASK);
	}

	value = reg_value(switch_addr_type(dst_type), value, CE_PKE_RESULTS_DST_TYPE_SHIFT, CE_PKE_RESULTS_DST_TYPE_MASK);

	writel(value, (((struct crypto_dev *)device)->base + REG_PK_RESULTS_DST_ADDR_H_CE_(vce_id)));
}

uint32_t pke_start_wait_status(void *device)
{
	uint32_t res;
	uint32_t value;
	uint32_t vce_id;
	int i = 0;
	vce_id = ((struct crypto_dev *)device)->ce_id;
#if RSA_PERFORMANCE_TEST
	uint64_t cur_time = current_time_hires();
#endif

	value = reg_value(0x1, 0, CE_PKE_GO_SHIFT, CE_PKE_GO_MASK);
#if AUTO_OUTPUT_BY_CE
	value = reg_value(0x1, value, CE_PKE_CLRMEMAFTEROP_SHIFT, CE_PKE_CLRMEMAFTEROP_MASK);
#else
	value = reg_value(0x0, value, CE_PKE_CLRMEMAFTEROP_SHIFT, CE_PKE_CLRMEMAFTEROP_MASK);
#endif
	value = reg_value(0x0, value, CE_PKE_CLRMEMAFTEROP_SHIFT, CE_PKE_CLRMEMAFTEROP_MASK);
	writel(value, (((struct crypto_dev *)device)->base + REG_PKE_CTRL_CE_(vce_id)));

	of_set_sys_cnt_ce(15);
//next is wait for an interrupt, and read & return the status register
#if WAIT_PK_WITH_REGISTER_POLLING


	while (pke_is_busy(device)) {
		if (i == 1000) {
			pr_emerg("times: %d, PKE is busy.\n", i);
			break;
		}

		//if(((struct crypto_dev *)device)->pke_condition == 1){
		//    ((struct crypto_dev *)device)->pke_condition = 0;
		//    break;
		//}
		udelay(30);
		i++;
	}

#else
	//pr_info("wait irq in pke\n");
	//event_wait(&g_ce_signal[vce_id]);
	//int sleep = 0;

	wait_event_interruptible(((struct crypto_dev *)device)->cepkewaitq, ((struct crypto_dev *)device)->pke_condition);
	((struct crypto_dev *)device)->pke_condition = 0;
	//pr_info("wait irq end in pke\n");
#endif
	of_set_sys_cnt_ce(16);
#if AUTO_OUTPUT_BY_CE
	//clear pk result ctrl, must use this if auto output
	writel(0x0, (((struct crypto_dev *)device)->base + REG_PK_RESULTS_CTRL_CE_(vce_id)));
#endif
	res =  pke_get_status(device);

#if RSA_PERFORMANCE_TEST
	time_slice += current_time_hires() - cur_time;
#endif

	return res;
}

void pke_reset(void *device)
{
	uint32_t vce_id;

	vce_id = ((struct crypto_dev *)device)->ce_id;

	writel(0x1 << CE_PKE_SOFT_RST_SHIFT, (((struct crypto_dev *)device)->base + REG_PKE_CTRL_CE_(vce_id)));
}

void pke_load_curve(void *device,
					const block_t *curve,
					uint32_t size,
					uint32_t byte_swap,
					uint32_t gen)
{
	uint32_t i;

	block_t param;
	param.addr  = curve->addr;
	param.len   = size;
	param.addr_type = curve->addr_type;

	/* Load ECC parameters */
	for (i = 0; i * size < curve->len; i++) {
		if (gen || (i != 2 && i != 3)) {
			if (!byte_swap) {
				mem2CryptoRAM_rev(device, &param, size, i, true);
			}
			else {
				mem2CryptoRAM(device, &param, size, i, false);
			}
		}

		param.addr += param.len;
	}
}

#if RSA_PERFORMANCE_TEST
uint64_t get_rsa_time_slice(void)
{
	return time_slice;
}
void clear_rsa_time_slice(void)
{
	time_slice = 0;
}
#endif
