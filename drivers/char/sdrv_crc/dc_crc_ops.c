/*
 * dc_crc_ops.c
 *
 * Semidrive kunlun platform disp crc driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include "sdrv_crc.h"


/**
 * Set the region of interest in according to the params and valid_roi_cnt
 * @param [IN] desc Pointer to device instance/structure.
 * @param [IN] the roi params to set.
 * @param [IN] the valid roi count.
 * @return EOK on success, failure code on error.
 */
static int sd_x9_disp_crc_set_roi(struct disp_crc *desc, struct roi_params *params, uint32_t valid_roi_cnt)
{
	int ret = 0;
	struct dc_reg_status *sd_dev = &desc->dc_reg;
	struct crc32_reg  *crc_reg = sd_dev->crc_reg;
	int i = 0;
	unsigned int val = 0;

	if(params == NULL)
		return EINVAL;

	if((valid_roi_cnt == 0) || (valid_roi_cnt > 8))
		return EINVAL;

	for(i = 0; i < valid_roi_cnt; i++) {
		val = (1 << 31)
			  | (params[i].start_y << 16)
	      | (params[i].start_x << 0);
		crc_reg->crc32_blk[i].crc32_block_ctrl0 |= val;

		val = (params[i].end_y << 16) | (params[i].end_x << 0);
		crc_reg->crc32_blk[i].crc32_block_ctrl1 |= val;

		dev_dbg(desc->dev, "dc hcrc roi[%d]: [%d, %d, %d, %d]\n", i,
			params[i].start_x, params[i].start_y, params[i].end_x,
			params[i].end_y);
	}

	return ret;
}


/**
 * Set global enable of hardware crc calculation.
 * @param [IN] desc Pointer to device instance/structure.
 * @return EOK on success, failure code on error.
 */
static int sd_x9_disp_crc_start_calc(struct disp_crc *desc)
{
	int ret = 0, i;
	struct dc_reg_status *sd_dev = &desc->dc_reg;
	struct crc32_reg  *crc_reg = sd_dev->crc_reg;
	struct dc_reg *reg = sd_dev->regs;
	unsigned int val = 0;
	if (desc->start_crc) {
		val = crc_reg->crc32_ctrl;
		val |= 1; /* crc global enable */
		crc_reg->crc32_ctrl = val;

		val = reg->dc_flc_ctrl;
		val |= BIT(3); /*CRC32_TRIG*/
		// val |= BIT(0); /*FLC_TRIG*/
		reg->dc_flc_ctrl = val;

	} else {
		val = crc_reg->crc32_ctrl;
		val &= ~1; /* crc global disabled */
		crc_reg->crc32_ctrl = val;
		//clear crc set
		for (i = 0; i < 8; i++) {
			crc_reg->crc32_blk[i].crc32_block_ctrl0 = 0;
			crc_reg->crc32_blk[i].crc32_block_ctrl1 = 0;
			crc_reg->crc32_int_st |= (1 << i);
		}
	}

	return ret;
}

/**
 * Get the hardware crc result data and status.
 * @param [IN] desc Pointer to device instance/structure.
 * @param [IN] the specified crc channel num.
 * @param [OUT] the crc status.
 * @param [OUT] the crc result.
 * @return EOK on success, failure code on error.
 */
static int sd_x9_disp_crc_get_hcrc(struct disp_crc *desc, uint32_t crc_chan_num, bool *crc_status, uint32_t *crc_result)
{
	int ret = 0;
	struct dc_reg_status *sd_dev = &desc->dc_reg;
	struct crc32_reg  *crc_reg = sd_dev->crc_reg;
	bool *tmp_status = sd_dev->tmp_status;

	*crc_status = tmp_status[crc_chan_num];
	*crc_result = crc_reg->crc32_blk[crc_chan_num].crc32_block_result_data;
	crc_reg->crc32_int_st |= (0 << crc_chan_num); /* clear done int status */

	return ret;
}

/**
 * Wait all crc calculation done or get err .
 * @param [IN] desc Pointer to device instance/structure.
 * @param [IN] the specified crc channel count.
 * @return EOK on success, failure code on error.
 */
static int sd_x9_disp_crc_wait_done(struct disp_crc *desc, uint32_t crc_chan_cnt)
{
	int ret = 0;
	struct dc_reg_status *sd_dev = &desc->dc_reg;
	struct crc32_reg  *crc_reg = sd_dev->crc_reg;
	unsigned int crc32_val = crc_reg->crc32_int_st;
	bool *tmp_status = sd_dev->tmp_status;
	uint32_t done_bit,err_bit;
	int i;

	for(i = 0; i < crc_chan_cnt; i++) {
		done_bit = 1 << i;
		err_bit = 1 << (i + 8);

		// if ((crc32_val & err_bit) == err_bit)
		//     dev_err(desc->dev, "CRC_ERROR_%d\n", i);

		if ((crc32_val & done_bit) != done_bit)
	        return -1;
		else
			tmp_status[i] = CRC_OK;
	}

	return ret;
}

static int sd_x9_disp_crc_clear_done(struct disp_crc *desc)
{
	int ret = 0;
	struct dc_reg_status *sd_dev = &desc->dc_reg;
	struct crc32_reg  *crc_reg = sd_dev->crc_reg;

	crc_reg->crc32_int_st = 0xff;

	return ret;
}

const struct disp_crc_ops disp_crc_ops = {
	.start_calc = sd_x9_disp_crc_start_calc,
	.set_roi = sd_x9_disp_crc_set_roi,
	.get_hcrc = sd_x9_disp_crc_get_hcrc,
	.wait_done = sd_x9_disp_crc_wait_done,
	.clear_done = sd_x9_disp_crc_clear_done,
};

