/*
* dwc_mipi_dsi_host.c
*
* Semidrive platform drm driver
*
* Copyright (C) 2019, Semidrive  Communications Inc.
*
* This file is licensed under a dual GPLv2 or X11 license.
*/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>

#include "dwc_mipi_dsi_host.h"

#define DSI_VESION_1_40A 0x3134302A

/**
* Get DSI Host core version
* @param instance pointer to structure holding the DSI Host core information
* @return ascii number of the version
*/
static bool dsi_check_version(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	/* DWC_mipi_dsi_host_1.140 */
	if (dsi_read(&reg->VERSION) == DSI_VESION_1_40A)
		return true;

	return false;
}

/**
* Modify power status of DSI Host core
* @param instance pointer to structure holding the DSI Host core information
* @param on (1) or off (0)
*/
static void dsi_power_enable(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(enable, &reg->PWR_UP);
}

/**
* Get the power status of the DSI Host core
* @param dev pointer to structure holding the DSI Host core information
* @return power status
*/
static uint8_t dsi_get_power_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x04 power_up;

	power_up.val = dsi_read(&reg->PWR_UP);
	return power_up.bits.shutdownz;
}

/* PRESP Time outs */
/**
* configure timeout divisions (so they would have more clock ticks)
* @param instance pointer to structure holding the DSI Host core information
* @param div no of hs cycles before transiting back to LP in
*  (lane_clk / div)
*/
static void dsi_timeout_clock_division(struct dsi_context *ctx,
	uint8_t div)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x08 clkmgr_cfg;

	clkmgr_cfg.val = dsi_read(&reg->CLKMGR_CFG);
	clkmgr_cfg.bits.to_clk_division = div;

	dsi_write(clkmgr_cfg.val, &reg->CLKMGR_CFG);
}

/**
* Write transmission escape timeout
* a safe guard so that the state machine would reset if transmission
* takes too long
* @param instance pointer to structure holding the DSI Host core information
* @param div
*/
static void dsi_tx_escape_division(struct dsi_context *ctx,
	uint8_t division)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x08 clkmgr_cfg;

	clkmgr_cfg.val = dsi_read(&reg->CLKMGR_CFG);
	clkmgr_cfg.bits.tx_esc_clk_division = division;

	dsi_write(clkmgr_cfg.val, &reg->CLKMGR_CFG);
}

/**
* Write the DPI video virtual channel destination
* @param instance pointer to structure holding the DSI Host core information
* @param vc virtual channel
*/
static void dsi_video_vcid(struct dsi_context *ctx, uint8_t vc)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(vc, &reg->DPI_VCID);
}

/**
 * Get the DPI video virtual channel destination
 * @param dev pointer to structure holding the DSI Host core information
 * @return virtual channel
 */
static uint8_t dsi_get_video_vcid(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x0C dpi_vcid;

	dpi_vcid.val = dsi_read(&reg->DPI_VCID);
	return dpi_vcid.bits.dpi_vcid;
}

/**
* Set DPI video color coding
* @param instance pointer to structure holding the DSI Host core information
* @param color_coding enum (configuration and color depth)
* @return error code
*/
static void dsi_dpi_color_coding(struct dsi_context *ctx, int coding)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x10 dpi_color_coding;

	dpi_color_coding.val = dsi_read(&reg->DPI_COLOR_CODING);
	dpi_color_coding.bits.dpi_color_coding = coding;

	dsi_write(dpi_color_coding.val, &reg->DPI_COLOR_CODING);
}

/**
 * Get DPI video color coding
 * @param dev pointer to structure holding the DSI Host core information
 * @return color coding enum (configuration and color depth)
 */
static uint8_t dsi_dpi_get_color_coding(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x10 dpi_color_coding;

	dpi_color_coding.val = dsi_read(&reg->DPI_COLOR_CODING);
	return dpi_color_coding.bits.dpi_color_coding;
}

/**
 * Get DPI video color depth
 * @param dev pointer to structure holding the DSI Host core information
 * @return number of bits per pixel
 */
static uint8_t dsi_dpi_get_color_depth(struct dsi_context *ctx)
{
	uint8_t color_depth = 0;
	switch (dsi_dpi_get_color_coding(ctx))
	{
		case 0:
		case 1:
		case 2:
			color_depth = 16;
			break;
		case 3:
		case 4:
			color_depth = 18;
			break;
		case 5:
			color_depth = 24;
			break;
		case 6:
			color_depth = 20;
			break;
		case 7:
			color_depth = 24;
			break;
		case 8:
			color_depth = 16;
			break;
		case 9:
			color_depth = 30;
			break;
		case 10:
			color_depth = 36;
			break;
		case 11:
			color_depth = 12;
			break;
		default:
			break;
	}
	return color_depth;
}

/**
 * Get DPI video pixel configuration
 * @param dev pointer to structure holding the DSI Host core information
 * @return pixel configuration
 */
static uint8_t dsi_dpi_get_color_config(struct dsi_context *ctx)
{
	uint8_t color_config = 0;
	switch (dsi_dpi_get_color_coding(ctx)) {
		case 0:
			color_config = 1;
			break;
		case 1:
			color_config = 2;
			break;
		case 2:
			color_config = 3;
			break;
		case 3:
			color_config = 1;
			break;
		case 4:
			color_config = 2;
			break;
		default:
			color_config = 0;
			break;
	}
	return color_config;
}

/**
* Set DPI loosely packetisation video (used only when color depth = 18
* @param instance pointer to structure holding the DSI Host core information
* @param enable
*/
static void dsi_dpi_18_loosely_packet_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x10 dpi_color_coding;

	dpi_color_coding.val = dsi_read(&reg->DPI_COLOR_CODING);
	dpi_color_coding.bits.loosely18_en = enable;

	dsi_write(dpi_color_coding.val, &reg->DPI_COLOR_CODING);
}

/*
* Set DPI color mode pin polarity
* @param instance pointer to structure holding the DSI Host core information
* @param active_low (1) or active high (0)
*/
static void dsi_dpi_color_mode_pol(struct dsi_context *ctx,
	int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = dsi_read(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.colorm_active_low = active_low;

	dsi_write(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}

/*
* Set DPI shut down pin polarity
* @param instance pointer to structure holding the DSI Host core information
* @param active_low (1) or active high (0)
*/
static void dsi_dpi_shut_down_pol(struct dsi_context *ctx,
	int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = dsi_read(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.shutd_active_low = active_low;

	dsi_write(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}

/*
* Set DPI horizontal sync pin polarity
* @param instance pointer to structure holding the DSI Host core information
* @param active_low (1) or active high (0)
*/
static void dsi_dpi_hsync_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = dsi_read(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.hsync_active_low = active_low;

	dsi_write(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}

/*
* Set DPI vertical sync pin polarity
* @param instance pointer to structure holding the DSI Host core information
* @param active_low (1) or active high (0)
*/
static void dsi_dpi_vsync_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = dsi_read(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.vsync_active_low = active_low;

	dsi_write(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}

/*
* Set DPI data enable pin polarity
* @param instance pointer to structure holding the DSI Host core information
* @param active_low (1) or active high (0)
*/
static void dsi_dpi_data_en_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = dsi_read(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.dataen_active_low = active_low;

	dsi_write(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}

/**
* Enable EOTp reception
* @param instance pointer to structure holding the DSI Host core information
* @param enable
*/
static void dsi_eotp_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = dsi_read(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.eotp_rx_en = enable;

	dsi_write(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}

/**
* Enable EOTp transmission
* @param instance pointer to structure holding the DSI Host core information
* @param enable
*/
static void dsi_eotp_tx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = dsi_read(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.eotp_tx_en = enable;

	dsi_write(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}

/**
* Enable Bus Turn-around request
* @param instance pointer to structure holding the DSI Host core information
* @param enable
*/
static void dsi_bta_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = dsi_read(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.bta_en = enable;

	dsi_write(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}

/**
* Enable ECC reception, error correction and reporting
* @param instance pointer to structure holding the DSI Host core information
* @param enable
*/
static void dsi_ecc_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = dsi_read(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.ecc_rx_en = enable;

	dsi_write(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}

/**
* Enable CRC reception, error reporting
* @param instance pointer to structure holding the DSI Host core information
* @param enable
*/
static void dsi_crc_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = dsi_read(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.crc_rx_en = enable;

	dsi_write(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}

/**
* Enable the EoTp transmission in low-Power
* @param instance pointer to structure holding the DSI Host core information
* @param enable
*/
static void dsi_eotp_tx_lp_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = dsi_read(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.eotp_tx_lp_en = enable;

	dsi_write(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}

/**
* Configure the read back virtual channel for the generic interface
* @param instance pointer to structure holding the DSI Host core information
* @param vc to listen to on the line
*/
static void dsi_rx_vcid(struct dsi_context *ctx, uint8_t vc)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(vc, &reg->GEN_VCID);
}

/**
* Enable/disable DPI video mode
* @param instance pointer to structure holding the DSI Host core information
*/
static void dsi_video_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(0, &reg->MODE_CFG);
}

/**
* Enable command mode (Generic interface)
* @param instance pointer to structure holding the DSI Host core information
*/
static void dsi_cmd_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(1, &reg->MODE_CFG);
}

/**
* Get the status of video mode, whether enabled or not in core
* @param dev pointer to structure holding the DSI Host core information
* @return status
*/
static bool dsi_is_cmd_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return dsi_read(&reg->MODE_CFG);
}

/**
* Set DCS read command packet transmission to transmission type
* @param instance pointer to structure holding the DSI Host core information
* @param no_of_param of command
* @param lp transmit in low power
* @return error code
*/
static void dsi_video_mode_lp_cmd_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_cmd_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
* Enable FRAME BTA ACK
* @param instance pointer to structure holding the DSI Host core information
* @param enable (1) - disable (0)
*/
static void dsi_video_mode_frame_ack_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.frame_bta_ack_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Enable return to low power mode inside horizontal front porch periods when
 *  timing allows
 * @param dev pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_video_mode_lp_hfp_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_hfp_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Enable return to low power mode inside horizontal back porch periods when
 *  timing allows
 * @param dev pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_video_mode_lp_hbp_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_hbp_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Enable return to low power mode inside vertical active lines periods when
 *  timing allows
 * @param dev pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_video_mode_lp_vact_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_vact_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Enable return to low power mode inside vertical front porch periods when
 *  timing allows
 * @param dev pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_video_mode_lp_vfp_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_vfp_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Enable return to low power mode inside vertical back porch periods when
 * timing allows
 * @param dev pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_video_mode_lp_vbp_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_vbp_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Enable return to low power mode inside vertical sync periods when
 *  timing allows
 * @param dev pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_video_mode_lp_vsa_en(struct dsi_context *ctx,
	int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_vsa_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
* Enable return to low power mode inside horizontal front porch periods when
*  timing allows
* @param instance pointer to structure holding the DSI Host core information
* @param enable (1) - disable (0)
*/
static void dsi_dpi_hporch_lp_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);

	vid_mode_cfg.bits.lp_hfp_en = enable;
	vid_mode_cfg.bits.lp_hbp_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
* Enable return to low power mode inside vertical active lines periods when
*  timing allows
* @param instance pointer to structure holding the DSI Host core information
* @param enable (1) - disable (0)
*/
static void dsi_dpi_vporch_lp_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);

	vid_mode_cfg.bits.lp_vact_en = enable;
	vid_mode_cfg.bits.lp_vfp_en = enable;
	vid_mode_cfg.bits.lp_vbp_en = enable;
	vid_mode_cfg.bits.lp_vsa_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
* Set DPI video mode type (burst/non-burst - with sync pulses or events)
* @param instance pointer to structure holding the DSI Host core information
* @param type
* @return error code
*/
static void dsi_video_mode_mode_type(struct dsi_context *ctx, int mode)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.vid_mode_type = mode;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Change Pattern orientation
 * @param dev pointer to structure holding the DSI Host core information
 * @param orientation choose between horizontal or vertical pattern
 */
static void dsi_vpg_orientation_act(struct dsi_context *ctx,
	uint8_t orientation)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.vpg_orientation = orientation;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Change Pattern Type
 * @param dev pointer to structure holding the DSI Host core information
 * @param mode choose between normal or BER pattern
 */
static void dsi_vpg_mode_act(struct dsi_context *ctx, uint8_t mode)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.vpg_mode = mode;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Change Video Pattern Generator
 * @param dev pointer to structure holding the DSI Host core information
 * @param enable enable video pattern generator
 */
static void dsi_enable_vpg_act(struct dsi_context *ctx, uint8_t enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = dsi_read(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.vpg_en = enable;

	dsi_write(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
* Write video packet size. obligatory for sending video
* @param instance pointer to structure holding the DSI Host core information
* @param size of video packet - containing information
* @return error code
*/
static void dsi_dpi_video_packet_size(struct dsi_context *ctx,
	uint16_t size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x3C vid_pkt_size;

	vid_pkt_size.val = dsi_read(&reg->VID_PKT_SIZE);
	vid_pkt_size.bits.vid_pkt_size = size;

	dsi_write(vid_pkt_size.val, &reg->VID_PKT_SIZE);
}

/*
* Write no of chunks to core - taken into consideration only when multi packet
* is enabled
* @param instance pointer to structure holding the DSI Host core information
* @param no of chunks
*/
static void dsi_dpi_chunk_num(struct dsi_context *ctx, uint16_t num)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x40 vid_num_chunks;

	vid_num_chunks.val = dsi_read(&reg->VID_NUM_CHUNKS);
	vid_num_chunks.bits.vid_num_chunks = num;

	dsi_write(vid_num_chunks.val, &reg->VID_NUM_CHUNKS);
}

/**
* Write the null packet size - will only be taken into account when null
* packets are enabled.
* @param instance pointer to structure holding the DSI Host core information
* @param size of null packet
* @return error code
*/
static void dsi_dpi_null_packet_size(struct dsi_context *ctx,
	uint16_t size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(size, &reg->VID_NULL_SIZE);
}

/**
* Configure the Horizontal sync time
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle taken to transmit the horizontal sync
*/
static void dsi_dpi_hsync_time(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x48 vid_hsa_time;

	vid_hsa_time.val = dsi_read(&reg->VID_HSA_TIME);
	vid_hsa_time.bits.vid_hsa_time = byte_cycle;

	dsi_write(vid_hsa_time.val, &reg->VID_HSA_TIME);
}

/**
* Configure the Horizontal back porch time
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle taken to transmit the horizontal back porch
*/
static void dsi_dpi_hbp_time(struct dsi_context *ctx, uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x4C vid_hbp_time;

	vid_hbp_time.val = dsi_read(&reg->VID_HBP_TIME);
	vid_hbp_time.bits.vid_hbp_time = byte_cycle;

	dsi_write(vid_hbp_time.val, &reg->VID_HBP_TIME);
}

/*
* Configure the Horizontal Line time
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle taken to transmit the total of the horizontal line
*/
static void dsi_dpi_hline_time(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x50 vid_hline_time;

	vid_hline_time.val = dsi_read(&reg->VID_HLINE_TIME);
	vid_hline_time.bits.vid_hline_time = byte_cycle;

	dsi_write(vid_hline_time.val, &reg->VID_HLINE_TIME);
}

/**
* Configure the vertical sync lines of the video stream
* @param instance pointer to structure holding the DSI Host core information
* @param lines
*/
static void dsi_dpi_vsync(struct dsi_context *ctx, uint16_t lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x54 vid_vsa_lines;

	vid_vsa_lines.val = dsi_read(&reg->VID_VSA_LINES);
	vid_vsa_lines.bits.vsa_lines = lines;

	dsi_write(vid_vsa_lines.val, &reg->VID_VSA_LINES);
}

/**
* Configure the vertical back porch lines of the video stream
* @param instance pointer to structure holding the DSI Host core information
* @param lines
*/
static void dsi_dpi_vbp(struct dsi_context *ctx, uint16_t lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x58 vid_vbp_lines;

	vid_vbp_lines.val = dsi_read(&reg->VID_VBP_LINES);
	vid_vbp_lines.bits.vbp_lines = lines;

	dsi_write(vid_vbp_lines.val, &reg->VID_VBP_LINES);
}

/**
* Configure the vertical front porch lines of the video stream
* @param instance pointer to structure holding the DSI Host core information
* @param lines
*/
static void dsi_dpi_vfp(struct dsi_context *ctx, uint16_t lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x5C vid_vfp_lines;

	vid_vfp_lines.val = dsi_read(&reg->VID_VFP_LINES);
	vid_vfp_lines.bits.vfp_lines = lines;

	dsi_write(vid_vfp_lines.val, &reg->VID_VFP_LINES);
}

/**
* Configure the vertical active lines of the video stream
* @param instance pointer to structure holding the DSI Host core information
* @param lines
*/
static void dsi_dpi_vact(struct dsi_context *ctx, uint16_t lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x60 vid_vactive_lines;

	vid_vactive_lines.val = dsi_read(&reg->VID_VACTIVE_LINES);
	vid_vactive_lines.bits.v_active_lines = lines;

	dsi_write(vid_vactive_lines.val, &reg->VID_VACTIVE_LINES);
}

/**
* Enable tear effect acknowledge
* @param instance pointer to structure holding the DSI Host core information
* @param enable (1) - disable (0)
*/
static void dsi_tear_effect_ack_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = dsi_read(&reg->CMD_MODE_CFG);
	cmd_mode_cfg.bits.tear_fx_en = enable;

	dsi_write(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}

/**
* Enable packets acknowledge request after each packet transmission
* @param instance pointer to structure holding the DSI Host core information
* @param enable (1) - disable (0)
*/
static void dsi_cmd_ack_request_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = dsi_read(&reg->CMD_MODE_CFG);
	cmd_mode_cfg.bits.ack_rqst_en = enable;

	dsi_write(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}

/**
* Set DCS&Generic command packet transmission to transmission type
* @param instance pointer to structure holding the DSI Host core information
* @param no_of_param of command
* @param lp transmit in low power
* @return error code
*/
static void dsi_cmd_mode_lp_cmd_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = dsi_read(&reg->CMD_MODE_CFG);

	cmd_mode_cfg.bits.gen_sw_0p_tx = enable;
	cmd_mode_cfg.bits.gen_sw_1p_tx = enable;
	cmd_mode_cfg.bits.gen_sw_2p_tx = enable;
	cmd_mode_cfg.bits.gen_lw_tx = enable;
	cmd_mode_cfg.bits.dcs_sw_0p_tx = enable;
	cmd_mode_cfg.bits.dcs_sw_1p_tx = enable;
	cmd_mode_cfg.bits.dcs_lw_tx = enable;
	cmd_mode_cfg.bits.max_rd_pkt_size = enable;

	cmd_mode_cfg.bits.gen_sr_0p_tx = enable;
	cmd_mode_cfg.bits.gen_sr_1p_tx = enable;
	cmd_mode_cfg.bits.gen_sr_2p_tx = enable;
	cmd_mode_cfg.bits.dcs_sr_0p_tx = enable;

	dsi_write(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}

/**
* Write command header in the generic interface
* (which also sends DCS commands) as a subset
* @param instance pointer to structure holding the DSI Host core information
* @param vc of destination
* @param packet_type (or type of DCS command)
* @param ls_byte (if DCS, it is the DCS command)
* @param ms_byte (only parameter of short DCS packet)
* @return error code
*/
static void dsi_set_packet_header(struct dsi_context *ctx,
	uint8_t vc,
	uint8_t type,
	uint8_t wc_lsb,
	uint8_t wc_msb)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x6C gen_hdr;

	gen_hdr.bits.gen_dt = type;
	gen_hdr.bits.gen_vc = vc;
	gen_hdr.bits.gen_wc_lsbyte = wc_lsb;
	gen_hdr.bits.gen_wc_msbyte = wc_msb;

	dsi_write(gen_hdr.val, &reg->GEN_HDR);
}

/**
* Write the payload of the long packet commands
* @param instance pointer to structure holding the DSI Host core information
* @param payload array of bytes of payload
* @return error code
*/
static void dsi_set_packet_payload(struct dsi_context *ctx,
	uint32_t payload)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(payload, &reg->GEN_PLD_DATA);
}

/**
* Write the payload of the long packet commands
* @param instance pointer to structure holding the DSI Host core information
* @param payload pointer to 32-bit array to hold read information
* @return error code
*/
static uint32_t dsi_get_rx_payload(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return dsi_read(&reg->GEN_PLD_DATA);
}

/**
 * Get status of read command
 * @param dev pointer to structure holding the DSI Host core information
 * @return 1 if busy
 */
/*static uint8_t dsi_rd_cmd_busy(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_rd_cmd_busy;
}*/

/**
* Get status of read command
* @param instance pointer to structure holding the DSI Host core information
* @return 1 if busy
*/
static bool dsi_is_bta_returned(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return !cmd_pkt_status.bits.gen_rd_cmd_busy;
}

/**
* Get the FULL status of generic read payload fifo
* @param instance pointer to structure holding the DSI Host core information
* @return 1 if fifo full
*/
static bool dsi_is_rx_payload_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_pld_r_full;
}

/**
* Get the EMPTY status of generic read payload fifo
* @param instance pointer to structure holding the DSI Host core information
* @return 1 if fifo empty
*/
static bool dsi_is_rx_payload_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_pld_r_empty;
}

/**
* Get the FULL status of generic write payload fifo
* @param instance pointer to structure holding the DSI Host core information
* @return 1 if fifo full
*/
static bool dsi_is_tx_payload_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_pld_w_full;
}

/**
* Get the EMPTY status of generic write payload fifo
* @param instance pointer to structure holding the DSI Host core information
* @return 1 if fifo empty
*/
static bool dsi_is_tx_payload_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_pld_w_empty;
}

/**
* Get the FULL status of generic command fifo
* @param instance pointer to structure holding the DSI Host core information
* @return 1 if fifo full
*/
static bool dsi_is_tx_cmd_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_full;
}

/**
* Get the EMPTY status of generic command fifo
* @param instance pointer to structure holding the DSI Host core information
* @return 1 if fifo empty
*/
static bool dsi_is_tx_cmd_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = dsi_read(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_empty;
}

/**
* Configure the Low power receive time out
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle (of byte cycles)
*/
static void dsi_lp_rx_timeout(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x78 to_cnt_cfg;

	to_cnt_cfg.val = dsi_read(&reg->TO_CNT_CFG);
	to_cnt_cfg.bits.lprx_to_cnt = byte_cycle;

	dsi_write(to_cnt_cfg.val, &reg->TO_CNT_CFG);
}

/**
* Configure a high speed transmission time out
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle (byte cycles)
*/
static void dsi_hs_tx_timeout(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x78 to_cnt_cfg;

	to_cnt_cfg.val = dsi_read(&reg->TO_CNT_CFG);
	to_cnt_cfg.bits.hstx_to_cnt = byte_cycle;

	dsi_write(to_cnt_cfg.val, &reg->TO_CNT_CFG);
}

/**
* Timeout for peripheral between HS data transmission read requests
* @param instance pointer to structure holding the DSI Host core information
* @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
* link still, after sending a high-speed read operation. This period is
* measured in cycles of lanebyteclk
*/
static void dsi_hs_read_presp_timeout(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(byte_cycle, &reg->HS_RD_TO_CNT);
}

/**
* Timeout for peripheral (for controller to stay still) after LP data
* transmission read requests
* @param instance pointer to structure holding the DSI Host core information
* @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
* link still, after sending a low power read operation. This period is
* measured in cycles of lanebyteclk
*/
static void dsi_lp_read_presp_timeout(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(byte_cycle, &reg->LP_RD_TO_CNT);
}

/**
* Timeout for peripheral (for controller to stay still) after HS data
* transmission write requests
* @param instance pointer to structure holding the DSI Host core information
* @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
* link still, after sending a high-speed write operation. This period is
* measured in cycles of lanebyteclk
*/
static void dsi_hs_write_presp_timeout(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x84 hs_wr_to_cnt;

	hs_wr_to_cnt.val = dsi_read(&reg->HS_WR_TO_CNT);
	hs_wr_to_cnt.bits.hs_wr_to_cnt = byte_cycle;

	dsi_write(hs_wr_to_cnt.val, &reg->HS_WR_TO_CNT);
}

/**
* Timeout for peripheral (for controller to stay still) after LP data
* transmission write requests
* @param instance pointer to structure holding the DSI Host core information
* @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
* link still, after sending a low power write operation. This period is
* measured in cycles of lanebyteclk
*/
static void __maybe_unused dsi_lp_write_presp_timeout(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(byte_cycle, &reg->LP_WR_TO_CNT);
}

/* PRESP Time outs */
/**
* Timeout for peripheral (for controller to stay still) after bus turn around
* @param instance pointer to structure holding the DSI Host core information
* @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
* link still, after sending a BTA operation. This period is
* measured in cycles of lanebyteclk
*/
static void dsi_bta_presp_timeout(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(byte_cycle, &reg->BTA_TO_CNT);
}

/**
* Enable the automatic mechanism to stop providing clock in the clock
* lane when time allows
* @param instance pointer to structure holding the DSI Host core information
* @param enable
* @return error code
*/
static void dsi_nc_clk_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x94 lpclk_ctrl;

	lpclk_ctrl.val = dsi_read(&reg->LPCLK_CTRL);
	lpclk_ctrl.bits.auto_clklane_ctrl = enable;

	dsi_write(lpclk_ctrl.val, &reg->LPCLK_CTRL);
}

/**
 * Get the status of the automatic mechanism to stop providing clock in the
 * clock lane when time allows
 * @param dev pointer to structure holding the DSI Host core information
 * @return status 1 (enabled) 0 (disabled)
 */
static uint8_t dsi_nc_clk_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x94 lpclk_ctrl;

	lpclk_ctrl.val = dsi_read(&reg->LPCLK_CTRL);

	return lpclk_ctrl.bits.auto_clklane_ctrl;
}

/**
* Get the error 0 interrupt register status
* @param instance pointer to structure holding the DSI Host core information
* @param mask the mask to be read from the register
* @return error status 0 value
*/
static uint32_t dsi_int0_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xBC int_sts0;

	int_sts0.val = dsi_read(&reg->INT_STS0);

	if (int_sts0.bits.dphy_errors_0)
		pr_err("dphy_err: escape entry error\n");

	if (int_sts0.bits.dphy_errors_1)
		pr_err("dphy_err: lp data transmission sync error\n");

	if (int_sts0.bits.dphy_errors_2)
		pr_err("dphy_err: control error\n");

	if (int_sts0.bits.dphy_errors_3)
		pr_err("dphy_err: LP0 contention error\n");

	if (int_sts0.bits.dphy_errors_4)
		pr_err("dphy_err: LP1 contention error\n");

	if (int_sts0.bits.ack_with_err_0)
		pr_err("ack_err: SoT error\n");

	if (int_sts0.bits.ack_with_err_1)
		pr_err("ack_err: SoT Sync error\n");

	if (int_sts0.bits.ack_with_err_2)
		pr_err("ack_err: EoT Sync error\n");

	if (int_sts0.bits.ack_with_err_3)
		pr_err("ack_err: Escape Mode Entry Command error\n");

	if (int_sts0.bits.ack_with_err_4)
		pr_err("ack_err: LP Transmit Sync error\n");

	if (int_sts0.bits.ack_with_err_5)
		pr_err("ack_err: Peripheral Timeout error\n");

	if (int_sts0.bits.ack_with_err_6)
		pr_err("ack_err: False Control error\n");

	if (int_sts0.bits.ack_with_err_7)
		pr_err("ack_err: reserved (specific to device)\n");

	if (int_sts0.bits.ack_with_err_8)
		pr_err("ack_err: ECC error, single-bit (corrected)\n");

	if (int_sts0.bits.ack_with_err_9)
		pr_err("ack_err: ECC error, multi-bit (not corrected)\n");

	if (int_sts0.bits.ack_with_err_10)
		pr_err("ack_err: checksum error (long packet only)\n");

	if (int_sts0.bits.ack_with_err_11)
		pr_err("ack_err: not recognized DSI data type\n");

	if (int_sts0.bits.ack_with_err_12)
		pr_err("ack_err: DSI VC ID Invalid\n");

	if (int_sts0.bits.ack_with_err_13)
		pr_err("ack_err: invalid transmission length\n");

	if (int_sts0.bits.ack_with_err_14)
		pr_err("ack_err: reserved (specific to device)\n");

	if (int_sts0.bits.ack_with_err_15)
		pr_err("ack_err: DSI protocol violation\n");

	return 0;
}

/**
* Get the error 1 interrupt register status
* @param instance pointer to structure holding the DSI Host core information
* @param mask the mask to be read from the register
* @return error status 1 value
*/
static uint32_t dsi_int1_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xC0 int_sts1;
	uint32_t status = 0;

	int_sts1.val = dsi_read(&reg->INT_STS1);

	if (int_sts1.bits.to_hs_tx)
		pr_err("high-speed transmission timeout\n");

	if (int_sts1.bits.to_lp_rx)
		pr_err("low-power reception timeout\n");

	if (int_sts1.bits.ecc_single_err)
		pr_err("ECC single error in a received packet\n");

	if (int_sts1.bits.ecc_multi_err)
		pr_err("ECC multiple error in a received packet\n");

	if (int_sts1.bits.crc_err)
		pr_err("CRC error in the received packet payload\n");

	if (int_sts1.bits.pkt_size_err)
		pr_err("receive packet size error\n");

	if (int_sts1.bits.eotp_err)
		pr_err("EoTp packet is not received\n");

	if (int_sts1.bits.dpi_pld_wr_err) {
		pr_err("DPI pixel-fifo is full\n");
		status |= BIT(0);//DSI_INT_STS_NEED_SOFT_RESET;
	}

	if (int_sts1.bits.gen_cmd_wr_err)
		pr_err("cmd header-fifo is full\n");

	if (int_sts1.bits.gen_pld_wr_err)
		pr_err("cmd write-payload-fifo is full\n");

	if (int_sts1.bits.gen_pld_send_err)
		pr_err("cmd write-payload-fifo is empty\n");

	if (int_sts1.bits.gen_pld_rd_err)
		pr_err("cmd read-payload-fifo is empty\n");

	if (int_sts1.bits.gen_pld_recev_err)
		pr_err("cmd read-payload-fifo is full\n");

	return status;
}

/**
* Configure MASK (hiding) of interrupts coming from error 0 source
* @param instance pointer to structure holding the DSI Host core information
* @param mask to be written to the register
*/
static void dsi_int0_mask(struct dsi_context *ctx, uint32_t mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(mask, &reg->INT_MASK0);
}

/**
 * Get the ERROR MASK  0 register status
 * @param dev pointer to structure holding the DSI Host core information
 * @param mask the bits to read from the mask register
 */
static uint32_t dsi_int_get_mask_0(struct dsi_context *ctx, uint32_t mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return dsi_read(&reg->INT_MASK0);
}

/**
* Configure MASK (hiding) of interrupts coming from error 1 source
* @param instance pointer to structure holding the DSI Host core information
* @param mask the mask to be written to the register
*/
static void dsi_int1_mask(struct dsi_context *ctx, uint32_t mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(mask, &reg->INT_MASK1);
}

/**
 * Get the ERROR MASK  1 register status
 * @param dev pointer to structure holding the DSI Host core information
 * @param mask the bits to read from the mask register
 */
static uint32_t dsi_int_get_mask_1(struct dsi_context *ctx, uint32_t mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return dsi_read(&reg->INT_MASK1);
}

/**
 * Force Interrupt coming from INT 0
 * @param dev pointer to structure holding the DSI Host core information
 * @param force interrupts to be forced
 */
static void dsi_force_int_0(struct dsi_context *ctx, uint32_t force)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(force, &reg->INT_FORCE0);
}

/**
 * Force Interrupt coming from INT 0
 * @param dev pointer to structure holding the DSI Host core information
 * @param force interrupts to be forced
 */
static void dsi_force_int_1(struct dsi_context *ctx, uint32_t force)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(force, &reg->INT_FORCE1);
}

/**
* Configure how many cycles of byte clock would the PHY module take
* to turn the bus around to start receiving
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle
* @return error code
*/
static void dsi_max_read_time(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xF4 phy_tmr_rd_cfg;

	phy_tmr_rd_cfg.val = dsi_read(&reg->PHY_TMR_RD_CFG);
	phy_tmr_rd_cfg.bits.max_rd_time = byte_cycle;

	dsi_write(phy_tmr_rd_cfg.val, &reg->PHY_TMR_RD_CFG);
}

/**
 * Function to activate shadow registers functionality
 * @param dev pointer to structure holding the DSI Host core information
 * @param activate activate or deactivate shadow registers
 */
static void dsi_activate_shadow_registers(struct dsi_context *ctx,
	uint8_t activate)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x100 vid_shadow_ctrl;

	vid_shadow_ctrl.val = dsi_read(&reg->VID_SHADOW_CTRL);
	vid_shadow_ctrl.bits.vid_shadow_en = activate;

	dsi_write(vid_shadow_ctrl.val, &reg->VID_SHADOW_CTRL);
}

/**
 * Function to read shadow registers functionality state
 * @param dev pointer to structure holding the DSI Host core information
 */
static uint8_t dsi_read_state_shadow_registers(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x100 vid_shadow_ctrl;

	vid_shadow_ctrl.val = dsi_read(&reg->VID_SHADOW_CTRL);

	return vid_shadow_ctrl.bits.vid_shadow_en;
}

/**
 * Request registers change
 * @param dev pointer to structure holding the DSI Host core information
 */
static void dsi_request_registers_change(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x100 vid_shadow_ctrl;

	vid_shadow_ctrl.val = dsi_read(&reg->VID_SHADOW_CTRL);
	vid_shadow_ctrl.bits.vid_shadow_req = 1;

	dsi_write(vid_shadow_ctrl.val, &reg->VID_SHADOW_CTRL);
}

/**
 * Use external pin as control to registers change
 * @param dev pointer to structure holding the DSI Host core information
 * @param external choose between external or internal control
 */
static void dsi_external_pin_registers_change(struct dsi_context *ctx,
	uint8_t external)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x100 vid_shadow_ctrl;

	vid_shadow_ctrl.val = dsi_read(&reg->VID_SHADOW_CTRL);
	vid_shadow_ctrl.bits.vid_shadow_pin_req = external;

	dsi_write(vid_shadow_ctrl.val, &reg->VID_SHADOW_CTRL);
}

/**
 * Get the DPI video virtual channel destination
 * @param dev pointer to structure holding the DSI Host core information
 * @return virtual channel
 */
static uint8_t dsi_get_dpi_video_vc_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x10C dpi_vcid_act;

	dpi_vcid_act.val = dsi_read(&reg->DPI_VCID_ACT);

	return dpi_vcid_act.bits.dpi_vcid;
}

/**
 * Get loosely packed variant to 18-bit configurations status
 * @param dev pointer to structure holding the DSI Host core information
 * @return loosely status
 */
static uint8_t dsi_get_loosely18_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x110 dpi_color_coding_act;

	dpi_color_coding_act.val = dsi_read(&reg->DPI_COLOR_CODING_ACT);

	return dpi_color_coding_act.bits.loosely18_en;
}

/**
 * Get DPI video color coding
 * @param dev pointer to structure holding the DSI Host core information
 * @return color coding enum (configuration and color depth)
 */
static uint8_t dsi_get_dpi_color_coding_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x110 dpi_color_coding_act;

	dpi_color_coding_act.val = dsi_read(&reg->DPI_COLOR_CODING_ACT);

	return dpi_color_coding_act.bits.dpi_color_coding;
}

/**
 * See if the command transmission is in low-power mode.
 * @param dev pointer to structure holding the DSI Host core information
 * @return lpm command transmission
 */
static uint8_t dsi_get_lp_cmd_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.lp_cmd_en;
}

/**
 * See if there is a request for an acknowledge response at
 * the end of a frame.
 * @param dev pointer to structure holding the DSI Host core information
 * @return  acknowledge response
 */
static uint8_t dsi_get_frame_bta_ack_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.frame_bta_ack_en;
}

/**
 * Get the return to low-power inside the Horizontal Front Porch (HFP)
 * period when timing allows.
 * @param dev pointer to structure holding the DSI Host core information
 * @return return to low-power
 */
static uint8_t dsi_get_lp_hfp_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.lp_hfp_en;
}

/**
 * Get the return to low-power inside the Horizontal Back Porch (HBP) period
 * when timing allows.
 * @param dev pointer to structure holding the DSI Host core information
 * @return return to low-power
 */
static uint8_t dsi_get_lp_hbp_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.lp_hbp_en;
}

/**
 * Get the return to low-power inside the Vertical Active (VACT) period when
 * timing allows.
 * @param dev pointer to structure holding the DSI Host core information
 * @return return to low-power
 */
static uint8_t dsi_get_lp_vact_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.lp_vact_en;
}

/**
 * Get the return to low-power inside the Vertical Front Porch (VFP) period
 * when timing allows.
 * @param dev pointer to structure holding the DSI Host core information
 * @return return to low-power
 */
static uint8_t dsi_get_lp_vfp_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.lp_vfp_en;
}

/**
 * Get the return to low-power inside the Vertical Back Porch (VBP) period
 * when timing allows.
 * @param dev pointer to structure holding the DSI Host core information
 * @return return to low-power
 */
static uint8_t dsi_get_lp_vbp_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.lp_vbp_en;
}

/**
 * Get the return to low-power inside the Vertical Sync Time (VSA) period
 * when timing allows.
 * @param dev pointer to structure holding the DSI Host core information
 * @return return to low-power
 */
static uint8_t dsi_get_lp_vsa_en_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.lp_vsa_en;
}

/**
 * Get the video mode transmission type
 * @param dev pointer to structure holding the DSI Host core information
 * @return video mode transmission type
 */
static uint8_t dsi_get_vid_mode_type_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x138 vid_mode_cfg_act;

	vid_mode_cfg_act.val = dsi_read(&reg->VID_MODE_CFG_ACT);

	return vid_mode_cfg_act.bits.vid_mode_type;
}

/**
 * Get the number of pixels in a single video packet
 * @param dev pointer to structure holding the DSI Host core information
 * @return video packet size
 */
static uint16_t dsi_get_vid_pkt_size_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x13C vid_pkt_size_act;

	vid_pkt_size_act.val = dsi_read(&reg->VID_PKT_SIZE_ACT);

	return vid_pkt_size_act.bits.vid_pkt_size;
}

/**
 * Get the number of chunks to be transmitted during a Line period
 * (a chunk is pair made of a video packet and a null packet).
 * @param dev pointer to structure holding the DSI Host core information
 * @return num_chunks number of chunks to use
 */
static uint16_t dsi_get_vid_num_chunks_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x140 vid_num_chunks_act;

	vid_num_chunks_act.val = dsi_read(&reg->VID_NUM_CHUNKS_ACT);

	return vid_num_chunks_act.bits.vid_num_chunks;
}

/**
 * Get the number of bytes inside a null packet
 * @param dev pointer to structure holding the DSI Host core information
 * @return size of null packets
 */
static uint16_t dsi_get_vid_null_size_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x144 vid_null_size_act;

	vid_null_size_act.val = dsi_read(&reg->VID_NULL_SIZE_ACT);

	return vid_null_size_act.bits.vid_null_size;
}

/**
 * Get the Horizontal Synchronism Active period in lane byte clock cycles.
 * @param dev pointer to structure holding the DSI Host core information
 * @return video HSA time
 */
static uint16_t dsi_get_vid_hsa_time_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x148 vid_hsa_time_act;

	vid_hsa_time_act.val = dsi_read(&reg->VID_HSA_TIME_ACT);

	return vid_hsa_time_act.bits.vid_hsa_time;
}

/**
 * Get the Horizontal Back Porch period in lane byte clock cycles.
 * @param dev pointer to structure holding the DSI Host core information
 * @return video HBP time
 */
static uint16_t dsi_get_vid_hbp_time_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14C vid_hbp_time_act;

	vid_hbp_time_act.val = dsi_read(&reg->VID_HBP_TIME_ACT);

	return vid_hbp_time_act.bits.vid_hbp_time;
}

/**
 * Get the size of the total line time (HSA+HBP+HACT+HFP)
 * counted in lane byte clock cycles.
 * @param dev pointer to structure holding the DSI Host core information
 * @return overall time for each video line
 */
static uint16_t dsi_get_vid_hline_time_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x150 vid_hline_time_act;

	vid_hline_time_act.val = dsi_read(&reg->VID_HLINE_TIME_ACT);

	return vid_hline_time_act.bits.vid_hline_time;
}

/**
 * Get the Vertical Synchronism Active period measured in number of horizontal lines.
 * @param dev pointer to structure holding the DSI Host core information
 * @return VSA period
 */
static uint16_t dsi_get_vsa_lines_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x154 vid_vsa_lines_act;

	vid_vsa_lines_act.val = dsi_read(&reg->VID_VSA_LINES_ACT);

	return vid_vsa_lines_act.bits.vsa_lines;
}

/**
 * Get the Vertical Back Porch period measured in number of horizontal lines.
 * @param dev pointer to structure holding the DSI Host core information
 * @return VBP period
 */
static uint16_t dsi_get_vbp_lines_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x158 vid_vbp_lines_act;

	vid_vbp_lines_act.val = dsi_read(&reg->VID_VBP_LINES_ACT);

	return vid_vbp_lines_act.bits.vbp_lines;
}

/**
 * Get the Vertical Front Porch period measured in number of horizontal lines.
 * @param dev pointer to structure holding the DSI Host core information
 * @return VFP period
 */
static uint16_t dsi_get_vfp_lines_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x15C vid_vfp_lines_act;

	vid_vfp_lines_act.val = dsi_read(&reg->VID_VFP_LINES_ACT);

	return vid_vfp_lines_act.bits.vfp_lines;
}

/**
 * Get the Vertical Active period measured in number of horizontal lines
 * @param dev pointer to structure holding the DSI Host core information
 * @return vertical resolution of video.
 */
static uint16_t dsi_get_v_active_lines_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x160 vid_vactive_lines_act;

	vid_vactive_lines_act.val = dsi_read(&reg->VID_VACTIVE_LINES_ACT);

	return vid_vactive_lines_act.bits.v_active_lines;
}

/**
 * See if the next VSS packet includes 3D control payload
 * @param dev pointer to structure holding the DSI Host core information
 * @return include 3D control payload
 */
static uint8_t dsi_get_send_3d_cfg_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x190 sdf_3d_act;

	sdf_3d_act.val = dsi_read(&reg->SDF_3D_ACT);

	return sdf_3d_act.bits.send_3d_cfg;
}

/**
 * Get the left/right order:
 * @param dev pointer to structure holding the DSI Host core information
 * @return left/right order:
 */
static uint8_t dsi_get_right_left_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x190 sdf_3d_act;

	sdf_3d_act.val = dsi_read(&reg->SDF_3D_ACT);

	return sdf_3d_act.bits.right_first;
}

/**
 * See if there is a second VSYNC pulse between left and right Images,
 * when 3D image format is frame-based:
 * @param dev pointer to structure holding the DSI Host core information
 * @return second vsync
 */
static uint8_t dsi_get_second_vsync_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x190 sdf_3d_act;

	sdf_3d_act.val = dsi_read(&reg->SDF_3D_ACT);

	return sdf_3d_act.bits.second_vsync;
}

/**
 * Get the 3D image format
 * @param dev pointer to structure holding the DSI Host core information
 * @return 3D image format
 */
static uint8_t dsi_get_format_3d_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x190 sdf_3d_act;

	sdf_3d_act.val = dsi_read(&reg->SDF_3D_ACT);

	return sdf_3d_act.bits.format_3d;
}

/**
 * Get the 3D mode on/off and display orientation
 * @param dev pointer to structure holding the DSI Host core information
 * @return 3D mode
 */
static uint8_t dsi_get_mode_3d_act(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x190 sdf_3d_act;

	sdf_3d_act.val = dsi_read(&reg->SDF_3D_ACT);

	return sdf_3d_act.bits.mode_3d;
}
#if 0
static void dsi_auto_ulps_entry(struct dsi_context *ctx, int delay)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	dsi_write(delay, &reg->AUTO_ULPS_ENTRY_DELAY);
}

static void dsi_auto_ulps_wakeup(struct dsi_context *ctx,
			int twakeup_clk_div, int twakeup_cnt)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xE8  auto_ulps_wakeup_time;

	auto_ulps_wakeup_time.bits.twakeup_clk_div = twakeup_clk_div;
	auto_ulps_wakeup_time.bits.twakeup_cnt = twakeup_cnt;

	dsi_write(auto_ulps_wakeup_time.val, &reg->AUTO_ULPS_WAKEUP_TIME);
}

static void dsi_auto_ulps_mode(struct dsi_context *ctx, int mode)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xE0  auto_ulps_mode;

	auto_ulps_mode.bits.auto_ulps = mode;

	dsi_write(auto_ulps_mode.val, &reg->AUTO_ULPS_MODE);
}

static void dsi_pll_off_in_ulps(struct dsi_context *ctx,
		int pll_off_ulps, int pre_pll_off_req)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xE0  auto_ulps_mode;

	auto_ulps_mode.bits.pll_off_ulps = pll_off_ulps;
	auto_ulps_mode.bits.pre_pll_off_req = pre_pll_off_req;

	dsi_write(auto_ulps_mode.val, &reg->AUTO_ULPS_MODE);
}
/**
* Specifiy the size of the packet memory write start/continue
* @param instance pointer to structure holding the DSI Host core information
* @ size of the packet
* @note when different than zero (0) eDPI is enabled
*/
static void dsi_edpi_max_pkt_size(struct dsi_context *ctx, uint16_t size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x64 edpi_cmd_size;

	edpi_cmd_size.val = dsi_read(&reg->EDPI_CMD_SIZE);
	edpi_cmd_size.bits.edpi_allowed_cmd_size = size;

	dsi_write(edpi_cmd_size.val, &reg->EDPI_CMD_SIZE);
}
#endif

/*----------------------------------------------------------------------------*/
/*---------------------------dsi api for dphy---------------------------------*/
/*----------------------------------------------------------------------------*/

/**
* Configure how many cycles of byte clock would the PHY module take
* to switch clock lane from high speed to low power
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle
* @return error code
*/
static void dsi_dphy_clklane_hs2lp_config(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 phy_tmr_lpclk_cfg;

	phy_tmr_lpclk_cfg.val = dsi_read(&reg->PHY_TMR_LPCLK_CFG);
	phy_tmr_lpclk_cfg.bits.phy_clkhs2lp_time = byte_cycle;

	dsi_write(phy_tmr_lpclk_cfg.val, &reg->PHY_TMR_LPCLK_CFG);
}

/**
* Configure how many cycles of byte clock would the PHY module take
* to switch clock lane from to low power high speed
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle
* @return error code
*/
static void dsi_dphy_clklane_lp2hs_config(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 phy_tmr_lpclk_cfg;

	phy_tmr_lpclk_cfg.val = dsi_read(&reg->PHY_TMR_LPCLK_CFG);
	phy_tmr_lpclk_cfg.bits.phy_clklp2hs_time = byte_cycle;

	dsi_write(phy_tmr_lpclk_cfg.val, &reg->PHY_TMR_LPCLK_CFG);
}

/**
* Configure how many cycles of byte clock would the PHY module take
* to switch data lane from high speed to low power
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle
* @return error code
*/
static void dsi_dphy_datalane_hs2lp_config(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x9C phy_tmr_cfg;

	phy_tmr_cfg.val = dsi_read(&reg->PHY_TMR_CFG);
	phy_tmr_cfg.bits.phy_hs2lp_time = byte_cycle;

	dsi_write(phy_tmr_cfg.val, &reg->PHY_TMR_CFG);
}

/**
* Configure how many cycles of byte clock would the PHY module take
* to switch the data lane from to low power high speed
* @param instance pointer to structure holding the DSI Host core information
* @param byte_cycle
* @return error code
*/
static void dsi_dphy_datalane_lp2hs_config(struct dsi_context *ctx,
	uint16_t byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x9C phy_tmr_cfg;

	phy_tmr_cfg.val = dsi_read(&reg->PHY_TMR_CFG);
	phy_tmr_cfg.bits.phy_lp2hs_time = byte_cycle;

	dsi_write(phy_tmr_cfg.val, &reg->PHY_TMR_CFG);
}

/*
 * Enable clock lane module
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param en
 */
static void dsi_dphy_enableclk(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA0 phy_rstz;

	phy_rstz.val = dsi_read(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_enableclk = enable;

	dsi_write(phy_rstz.val, &reg->PHY_RSTZ);
}

/*
 * Reset D-PHY module
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param reset
 */
static void dsi_dphy_reset(struct dsi_context *ctx, int reset)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA0 phy_rstz;

	phy_rstz.val = dsi_read(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_rstz = reset;

	dsi_write(phy_rstz.val, &reg->PHY_RSTZ);
}

/**
 * Power up/down D-PHY module
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param powerup (1) shutdown (0)
 */
static void dsi_dphy_shutdown(struct dsi_context *ctx, int powerup)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA0 phy_rstz;

	phy_rstz.val = dsi_read(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_shutdownz = powerup;

	dsi_write(phy_rstz.val, &reg->PHY_RSTZ);
}

/*
 * Force D-PHY PLL to stay on while in ULPS
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param force (1) disable (0)
 * @note To follow the programming model, use wakeup_pll function
 */
static void dsi_dphy_force_pll(struct dsi_context *ctx, int force)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA0 phy_rstz;

	phy_rstz.val = dsi_read(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_forcepll = force;

	dsi_write(phy_rstz.val, &reg->PHY_RSTZ);
}

/**
 * Get force D-PHY PLL module
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @return force value
 */
static uint8_t dsi_dphy_get_force_pll(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA0 phy_rstz;

	phy_rstz.val = dsi_read(&reg->PHY_RSTZ);

	return phy_rstz.bits.phy_forcepll;
}

/**
 * Configure minimum wait period for HS transmission request after a stop state
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param no_of_byte_cycles [in byte (lane) clock cycles]
 */
static void dsi_dphy_stop_wait_time(struct dsi_context *ctx,
	uint8_t no_of_byte_cycles)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA4 phy_if_cfg;

	phy_if_cfg.val = dsi_read(&reg->PHY_IF_CFG);
	phy_if_cfg.bits.phy_stop_wait_time = no_of_byte_cycles;

	dsi_write(phy_if_cfg.val, &reg->PHY_IF_CFG);
}

/**
 * Set number of active lanes
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param no_of_lanes
 */
static void dsi_dphy_n_lanes(struct dsi_context *ctx,
	uint8_t n_lanes)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA4 phy_if_cfg;

	phy_if_cfg.val = dsi_read(&reg->PHY_IF_CFG);
	phy_if_cfg.bits.n_lanes = n_lanes - 1;
	dsi_write(phy_if_cfg.val, &reg->PHY_IF_CFG);
}

/**
 * Get number of currently active lanes
 *
 * @param dev pointer to structure which holds information about the d-phy
 *	module
 * @return number of active lanes
 */
static uint8_t dsi_dphy_get_n_lanes(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA4 phy_if_cfg;

	phy_if_cfg.val = dsi_read(&reg->PHY_IF_CFG);

	return (phy_if_cfg.bits.n_lanes + 1); /*note:actually,lanes=the return value + 1*/
}

/**
 * Request the PHY module to start transmission of high speed clock.
 * This causes the clock lane to start transmitting DDR clock on the
 * lane interconnect.
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param enable
 * @note this function should be called explicitly by user always except for
 * transmitting
 */
static void dsi_dphy_enable_hs_clk(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x94 lpclk_ctrl;

	lpclk_ctrl.val = dsi_read(&reg->LPCLK_CTRL);
	lpclk_ctrl.bits.phy_txrequestclkhs = enable;

	dsi_write(lpclk_ctrl.val, &reg->LPCLK_CTRL);
}

/*
 * Get D-PHY PPI status
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param mask
 * @return status
 */
static uint32_t dsi_dphy_get_status(struct dsi_context *ctx, uint16_t mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return dsi_read(&reg->PHY_STATUS) & mask;
}

/*
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dsi_dphy_testclk(struct dsi_context *ctx, int value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB4 phy_tst_ctrl0;

	phy_tst_ctrl0.val = dsi_read(&reg->PHY_TST_CTRL0);
	phy_tst_ctrl0.bits.phy_testclk = value;
	dsi_write(phy_tst_ctrl0.val, &reg->PHY_TST_CTRL0);
}

/**
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dsi_dphy_test_clear(struct dsi_context *ctx, int value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB4 phy_tst_ctrl0;

	phy_tst_ctrl0.val = dsi_read(&reg->PHY_TST_CTRL0);
	phy_tst_ctrl0.bits.phy_testclr = value;
	dsi_write(phy_tst_ctrl0.val, &reg->PHY_TST_CTRL0);
}

/**
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param test_data
 */
static void dsi_dphy_testdin(struct dsi_context *ctx, uint8_t test_data)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB8 phy_tst_ctrl1;

	phy_tst_ctrl1.val = dsi_read(&reg->PHY_TST_CTRL1);
	phy_tst_ctrl1.bits.phy_testdin = test_data;
	dsi_write(phy_tst_ctrl1.val, &reg->PHY_TST_CTRL1);
}

/**
 * @param dev pointer to structure which holds information about the d-phy
 * module
 */
static uint8_t __maybe_unused dsi_dphy_test_dout(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB8 phy_tst_ctrl1;

	phy_tst_ctrl1.val = dsi_read(&reg->PHY_TST_CTRL1);

	return phy_tst_ctrl1.bits.phy_testdout;
}

/**
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param on_falling_edge
 */
static void dsi_dphy_testen(struct dsi_context *ctx,
	uint8_t on_falling_edge)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB8 phy_tst_ctrl1;

	phy_tst_ctrl1.val = dsi_read(&reg->PHY_TST_CTRL1);
	phy_tst_ctrl1.bits.phy_testen = on_falling_edge;
	dsi_write(phy_tst_ctrl1.val, &reg->PHY_TST_CTRL1);
}

/**
 * Write to D-PHY module (encapsulating the digital interface)
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param address offset inside the D-PHY digital interface
 * @param data array of bytes to be written to D-PHY
 * @param data_length of the data array
 */
static void dsi_dphy_write(struct dsi_context *ctx, uint16_t address,
	uint8_t *data, uint8_t data_length)
{
	unsigned i = 0;

	pr_debug("TEST CODE: ADDR %X DATA %X\n", address, data[0]);
	if (data != 0) {
		/* set the TESTCLK input high in preparation
		* to latch in the desired test mode */
		dsi_dphy_testclk(ctx, 1);
		/* set the desired test code in the input 8-bit bus
		* TESTDIN[7:0] */
		dsi_dphy_testdin(ctx, (uint8_t)address);
		/* set TESTEN input high  */
		dsi_dphy_testen(ctx, 1);
		/* drive the TESTCLK input low; the falling edge
		* captures the chosen test code into the transceiver
		*/
		dsi_dphy_testclk(ctx, 0);
		/* set TESTEN input low to disable further test mode
		* code latching  */
		dsi_dphy_testen(ctx, 0);
		/* start writing MSB first */
		for (i = data_length; i > 0; i--) {
			/* set TESTDIN[7:0] to the desired test data
			* appropriate to the chosen test mode */
			dsi_dphy_testdin(ctx, data[i - 1]);
			/* pulse TESTCLK high to capture this test data
			* into the macrocell; repeat these two steps
			* as necessary */
			dsi_dphy_testclk(ctx, 1);
			dsi_dphy_testclk(ctx, 0);
		}
	}
}
#if 0
/**
 * Wake up or make sure D-PHY PLL module is awake
 * This function must be called after going into ULPS and before exiting it
 * to force the DPHY PLLs to wake up. It will wait until the DPHY status is
 * locked. It follows the procedure described in the user guide.
 * This function should be used to make sure the PLL is awake, rather than
 * the force_pll above.
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @return error code
 * @note this function has an active wait
 */
static int dsi_dphy_wakeup_pll(struct dphy_context *ctx)
{
	unsigned i = 0;
	if (dsi_dphy_status(ctx, 0x1) == 0) {
		dsi_dphy_force_pll(ctx, 1);
		for (i = 0; i < DSIH_PHY_ACTIVE_WAIT; i++) {
			if (dsi_dphy_status(ctx, 0x1)) {
				break;
			}
		}
		if (dsi_dphy_status(ctx, 0x1) == 0) {
			return false;
		}
	}
	return true;
}


/**
 * One bit is asserted in the trigger_request (4bits) to cause the lane module
 * to cause the associated trigger to be sent across the lane interconnect.
 * The trigger request is synchronous with the rising edge of the clock.
 * @note: Only one bit of the trigger_request is asserted at any given time, the
 * remaining must be left set to 0, and only when not in LPDT or ULPS modes
 *
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param trigger_request 4 bit request
 */
static int dsi_dphy_escape_mode_trigger(struct dphy_context *ctx,
			uint8_t trigger_request)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	uint8_t sum = 0;
	int i = 0;

	for (i = 0; i < 4; i++) {
		sum += ((trigger_request >> i) & 1);
	}

	if (sum == 1) {

		/* clear old trigger */
		dsi_write(0, &reg->PHY_TX_TRIGGERS);

		dsi_write(trigger_request, &reg->PHY_TX_TRIGGERS);

		for (i = 0; i < DSIH_PHY_ACTIVE_WAIT; i++) {
			if (dsi_dphy_status(ctx, 0x0010)){
				break;
			}
		}

		dsi_write(0, &reg->PHY_TX_TRIGGERS);
		if (i >= DSIH_PHY_ACTIVE_WAIT) {
			return false;
		}
		return true;
	}
	return false;
}

/**
 * ULPS mode request/exit on all active data lanes.
 * @param dev pointer to structure which holds information about the d-phy
 * module
 * @param enable (request 1/ exit 0)
 * @return error code
 * @note this is a blocking function. wait upon exiting the ULPS will exceed 1ms
 */
static int dsi_dphy_ulps_data_lanes(struct dphy_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA8 phy_ulps_ctrl;
	/* mask 1 0101 0010 0000 */
	uint16_t data_lanes_mask = 0;
	int timeout;

	if (enable) {
		phy_ulps_ctrl.val = dsi_read(&reg->PHY_ULPS_CTRL);
		phy_ulps_ctrl.bits.phy_txrequlpslan = 1;
		dsi_write(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
		return true;
	} else {
		if (dsi_dphy_status(ctx, 0x1) == 0)
			return false;

		phy_ulps_ctrl.val = dsi_read(&reg->PHY_ULPS_CTRL);
		phy_ulps_ctrl.bits.phy_txexitulpslan = 1;
		dsi_write(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);

		switch (dsi_dphy_get_n_lanes(ctx)) {
			case 3:
				data_lanes_mask |= (1 << 12);
			case 2:
				data_lanes_mask |= (1 << 10);
			case 1:
				data_lanes_mask |= (1 << 8);
			case 0:
				data_lanes_mask |= (1 << 5);
				break;
			default:
				data_lanes_mask = 0;
				break;
		}
		for (timeout = 0; timeout < DSIH_PHY_ACTIVE_WAIT; timeout++) {
			/* verify that the DPHY has left ULPM */
			if (dsi_dphy_status(ctx, data_lanes_mask) == data_lanes_mask) {
				break;
			}
			mdelay(5);
		}

		if (dsi_dphy_status(ctx, data_lanes_mask) != data_lanes_mask) {
			pr_debug("stat %x, mask %x", dsi_dphy_status(ctx, data_lanes_mask),
					data_lanes_mask);
			return false;
		}

		phy_ulps_ctrl.bits.phy_txrequlpslan = 0;
		phy_ulps_ctrl.bits.phy_txexitulpslan = 0;
		dsi_write(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
	}
	return true;
}

/*
 * ULPS mode request/exit on Clock Lane.
 *
 * @param dev pointer to structure which holds information about the
 * d-phy module
 * @param enable 1 or disable 0 of the Ultra Low Power State of the clock lane
 * @return error code
 * @note this is a blocking function. wait upon exiting the ULPS will exceed 1ms
 */
static int dsi_dphy_ulps_clk_lane(struct dphy_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA8 phy_ulps_ctrl;
	/* mask 1000 */
	uint16_t clk_lane_mask = 0x0008;
	int timeout;

	if (enable) {
		phy_ulps_ctrl.val = dsi_read(&reg->PHY_ULPS_CTRL);
		phy_ulps_ctrl.bits.phy_txrequlpsclk = 1;
		dsi_write(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
	}
	else
	{
		if (dsi_dphy_status(ctx, 0x1) == 0)
			return false;

		phy_ulps_ctrl.val = dsi_read(&reg->PHY_ULPS_CTRL);
		phy_ulps_ctrl.bits.phy_txexitulpsclk = 1;
		dsi_write(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);

		for (timeout = 0; timeout < DSIH_PHY_ACTIVE_WAIT; timeout++) {
			/* verify that the DPHY has left ULPM */
			if (dsi_dphy_status(ctx, clk_lane_mask) == clk_lane_mask) {
				pr_debug("stat %x, mask %x", dsi_dphy_status(ctx, clk_lane_mask),
						clk_lane_mask);
				break;
			}
			mdelay(5);
		}
		if (dsi_dphy_status(ctx, clk_lane_mask) != clk_lane_mask)
			return false;

		phy_ulps_ctrl.bits.phy_txrequlpsclk = 0;
		phy_ulps_ctrl.bits.phy_txexitulpsclk = 0;
		dsi_write(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
	}
	return true;
}


static uint8_t dsi_dphy_is_pll_locked(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB0 phy_status;

	phy_status.val = dsi_read(&reg->PHY_STATUS);

	return phy_status.bits.phy_lock;
}

static void dsi_dphy_delay(struct dphy_context *ctx, int value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	uint8_t data[4];

	data[0] = (value << 2);

	dsi_dphy_write(ctx, R_12BITS_DPHY_RDY_L0, data, 1);
	dsi_dphy_write(ctx, R_12BITS_DPHY_RDY_L1, data, 1);
	dsi_dphy_write(ctx, R_12BITS_DPHY_RDY_L2, data, 1);
	dsi_dphy_write(ctx, R_12BITS_DPHY_RDY_L3, data, 1);

	pr_debug("%d clk cyles of delay on READY signal - 12bits. \n",
			 value);
}
#endif

struct dsi_core_ops dwc_mipi_dsi_host_ops = {
	.check_version = dsi_check_version,
	.power_enable = dsi_power_enable,
	.get_power_status = dsi_get_power_status,
	.timeout_clock_division = dsi_timeout_clock_division,
	.tx_escape_division = dsi_tx_escape_division,
	.video_vcid = dsi_video_vcid,
	.get_video_vcid = dsi_get_video_vcid,
	.dpi_color_coding = dsi_dpi_color_coding,
	.dpi_get_color_coding = dsi_dpi_get_color_coding,
	.dpi_get_color_depth = dsi_dpi_get_color_depth,
	.dpi_get_color_config = dsi_dpi_get_color_config,
	.dpi_18_loosely_packet_en = dsi_dpi_18_loosely_packet_en,
	.dpi_color_mode_pol = dsi_dpi_color_mode_pol,
	.dpi_shut_down_pol = dsi_dpi_shut_down_pol,
	.dpi_hsync_pol = dsi_dpi_hsync_pol,
	.dpi_vsync_pol = dsi_dpi_vsync_pol,
	.dpi_data_en_pol = dsi_dpi_data_en_pol,
	.eotp_rx_en = dsi_eotp_rx_en,
	.eotp_tx_en = dsi_eotp_tx_en,
	.bta_en = dsi_bta_en,
	.ecc_rx_en = dsi_ecc_rx_en,
	.crc_rx_en = dsi_crc_rx_en,
	.eotp_tx_lp_en = dsi_eotp_tx_lp_en,
	.rx_vcid = dsi_rx_vcid,
	.video_mode = dsi_video_mode,
	.cmd_mode = dsi_cmd_mode,
	.is_cmd_mode = dsi_is_cmd_mode,
	.video_mode_lp_cmd_en = dsi_video_mode_lp_cmd_en,
	.video_mode_frame_ack_en = dsi_video_mode_frame_ack_en,
	.video_mode_lp_hfp_en = dsi_video_mode_lp_hfp_en,
	.video_mode_lp_hbp_en = dsi_video_mode_lp_hbp_en,
	.video_mode_lp_vact_en = dsi_video_mode_lp_vact_en,
	.video_mode_lp_vfp_en = dsi_video_mode_lp_vfp_en,
	.video_mode_lp_vbp_en = dsi_video_mode_lp_vbp_en,
	.video_mode_lp_vsa_en = dsi_video_mode_lp_vsa_en,
	.dpi_hporch_lp_en = dsi_dpi_hporch_lp_en,
	.dpi_vporch_lp_en = dsi_dpi_vporch_lp_en,
	.video_mode_mode_type= dsi_video_mode_mode_type,
	.vpg_orientation_act = dsi_vpg_orientation_act,
	.vpg_mode_act = dsi_vpg_mode_act,
	.enable_vpg_act = dsi_enable_vpg_act,
	.dpi_video_packet_size = dsi_dpi_video_packet_size,
	.dpi_chunk_num = dsi_dpi_chunk_num,
	.dpi_null_packet_size = dsi_dpi_null_packet_size,
	.dpi_hline_time = dsi_dpi_hline_time,
	.dpi_hbp_time = dsi_dpi_hbp_time,
	.dpi_hsync_time = dsi_dpi_hsync_time,
	.dpi_vsync = dsi_dpi_vsync,
	.dpi_vbp = dsi_dpi_vbp,
	.dpi_vfp = dsi_dpi_vfp,
	.dpi_vact = dsi_dpi_vact,
	.tear_effect_ack_en = dsi_tear_effect_ack_en,
	.cmd_ack_request_en = dsi_cmd_ack_request_en,
	.cmd_mode_lp_cmd_en = dsi_cmd_mode_lp_cmd_en,
	.set_packet_header = dsi_set_packet_header,
	.set_packet_payload = dsi_set_packet_payload,
	.get_rx_payload = dsi_get_rx_payload ,
	.is_bta_returned = dsi_is_bta_returned,
	.is_rx_payload_fifo_full = dsi_is_rx_payload_fifo_full,
	.is_rx_payload_fifo_empty = dsi_is_rx_payload_fifo_empty,
	.is_tx_payload_fifo_full = dsi_is_tx_payload_fifo_full ,
	.is_tx_payload_fifo_empty = dsi_is_tx_payload_fifo_empty,
	.is_tx_cmd_fifo_full = dsi_is_tx_cmd_fifo_full,
	.is_tx_cmd_fifo_empty = dsi_is_tx_cmd_fifo_empty,
	.lp_rx_timeout = dsi_lp_rx_timeout,
	.hs_tx_timeout = dsi_hs_tx_timeout,
	.hs_read_presp_timeout = dsi_hs_read_presp_timeout,
	.lp_read_presp_timeout = dsi_lp_read_presp_timeout,
	.hs_write_presp_timeout = dsi_hs_write_presp_timeout,
	.bta_presp_timeout = dsi_bta_presp_timeout,
	.nc_clk_en = dsi_nc_clk_en ,
	.nc_clk_status = dsi_nc_clk_status,
	.int0_status = dsi_int0_status,
	.int1_status = dsi_int1_status,
	.int0_mask = dsi_int0_mask,
	.int_get_mask_0 = dsi_int_get_mask_0,
	.int1_mask = dsi_int1_mask,
	.int_get_mask_1 = dsi_int_get_mask_1,
	.force_int_0 = dsi_force_int_0,
	.force_int_1 = dsi_force_int_1,
	.max_read_time = dsi_max_read_time,
	.activate_shadow_registers = dsi_activate_shadow_registers,
	.read_state_shadow_registers = dsi_read_state_shadow_registers,
	.request_registers_change = dsi_request_registers_change,
	.external_pin_registers_change = dsi_external_pin_registers_change,
	.get_dpi_video_vc_act = dsi_get_dpi_video_vc_act,
	.get_loosely18_en_act = dsi_get_loosely18_en_act,
	.get_dpi_color_coding_act = dsi_get_dpi_color_coding_act,
	.get_lp_cmd_en_act = dsi_get_lp_cmd_en_act,
	.get_frame_bta_ack_en_act = dsi_get_frame_bta_ack_en_act,
	.get_lp_hfp_en_act = dsi_get_lp_hfp_en_act,
	.get_lp_hbp_en_act = dsi_get_lp_hbp_en_act,
	.get_lp_vact_en_act = dsi_get_lp_vact_en_act,
	.get_lp_vfp_en_act = dsi_get_lp_vfp_en_act,
	.get_lp_vbp_en_act = dsi_get_lp_vbp_en_act,
	.get_lp_vsa_en_act = dsi_get_lp_vsa_en_act ,
	.get_vid_mode_type_act = dsi_get_vid_mode_type_act,
	.get_vid_pkt_size_act = dsi_get_vid_pkt_size_act,
	.get_vid_num_chunks_act = dsi_get_vid_num_chunks_act,
	.get_vid_null_size_act = dsi_get_vid_null_size_act,
	.get_vid_hsa_time_act = dsi_get_vid_hsa_time_act,
	.get_vid_hbp_time_act = dsi_get_vid_hbp_time_act,
	.get_vid_hline_time_act = dsi_get_vid_hline_time_act,
	.get_vsa_lines_act = dsi_get_vsa_lines_act,
	.get_vbp_lines_act = dsi_get_vbp_lines_act,
	.get_vfp_lines_act = dsi_get_vfp_lines_act,
	.get_v_active_lines_act = dsi_get_v_active_lines_act,
	.get_send_3d_cfg_act = dsi_get_send_3d_cfg_act,
	.get_right_left_act = dsi_get_right_left_act,
	.get_second_vsync_act = dsi_get_second_vsync_act,
	.get_format_3d_act = dsi_get_format_3d_act,
	.get_mode_3d_act = dsi_get_mode_3d_act,

	.dphy_clklane_hs2lp_config = dsi_dphy_clklane_hs2lp_config,
	.dphy_clklane_lp2hs_config = dsi_dphy_clklane_lp2hs_config,
	.dphy_datalane_hs2lp_config = dsi_dphy_datalane_hs2lp_config,
	.dphy_datalane_lp2hs_config = dsi_dphy_datalane_lp2hs_config,
	.dphy_enableclk = dsi_dphy_enableclk,
	.dphy_reset = dsi_dphy_reset,
	.dphy_shutdown = dsi_dphy_shutdown,
	.dphy_force_pll = dsi_dphy_force_pll,
	.dphy_get_force_pll = dsi_dphy_get_force_pll,
	.dphy_stop_wait_time = dsi_dphy_stop_wait_time,
	.dphy_n_lanes = dsi_dphy_n_lanes,
	.dphy_enable_hs_clk = dsi_dphy_enable_hs_clk,
	.dphy_get_n_lanes = dsi_dphy_get_n_lanes,
	.dphy_get_status = dsi_dphy_get_status,
	.dphy_test_clear = dsi_dphy_test_clear,
	.dphy_write = dsi_dphy_write,
};
