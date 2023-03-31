/*
 * sdriv_drm_dsi.c
 *
 * Semidrive platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>
#include <linux/component.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <video/of_display_timing.h>

#include <drm/drm_mipi_dsi.h>
#include "dwc_mipi_dsi_host.h"

#define SDRV_MIPI_DSI	"sdrv-mipi-dsi"

#define DPHY_DIV_UPPER_LIMIT (8000)
#define DPHY_DIV_LOWER_LIMIT (2000)
#define MIN_OUTPUT_FREQ (80)

#define MAX_TIME_HS2LP	60 //120 /* unit: ns */
#define MAX_TIME_LP2HS	60 //500 /* unit: ns */

#define enc_to_dsi(enc) container_of(enc, struct sdrv_mipi_dsi, encoder)
#define con_to_dsi(con) container_of(con, struct sdrv_mipi_dsi, connector)
#define host_to_dsi(host) container_of(host, struct sdrv_mipi_dsi, dsi_host)
#define bridge_to_dsi(bridge) container_of(bridge, struct sdrv_mipi_dsi, bridge)

struct sdrv_mipi_dsi {
	struct device *dev;
	struct mipi_dsi_host dsi_host;
	struct mipi_dsi_device *slave;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_display_mode *mode;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct dsi_context ctx;
};

extern struct dsi_core_ops dwc_mipi_dsi_host_ops;

/*dsi api*/
static void mipi_dsih_dphy_open(struct dsi_context *ctx)
{
	/*
	struct dsi_core_ops *dsi_ops = ctx->ops;
	dsi_ops->dphy_reset(ctx, 0);
	dsi_ops->dphy_stop_wait_time(ctx, 0x1C);
	dsi_ops->dphy_n_lanes(ctx, 1);
	dsi_ops->dphy_enableclk(ctx, 1);
	dsi_ops->dphy_shutdown(ctx, 1);
	dsi_ops->dphy_reset(ctx, 1);
	*/
}

static int wait_dphy_ready(struct dsi_context *ctx)
{
	int32_t count = 0;
	struct dsi_core_ops *dsi_ops = ctx->ops;

	while (!dsi_ops->dphy_get_status(ctx, 0x5))  {
		udelay(1);
		count++;
		if (count > 5000) {
			DRM_ERROR("wait dphy lock&& stopstate timeout\n");
			return -1;
		}
	}

	DRM_INFO("dphy lock and stopstate ok\n");
	return 0;
}

int mipi_dsih_dphy_configure(struct dsi_context *ctx,
	uint8_t num_of_lanes, uint32_t output_data_rate)
{
	int ret;
	int flag = 0;
	uint64_t loop_divider = 0; /*(M)*/
	uint64_t input_divider = 0; /*(N)*/
	uint64_t vco_divider = 1; /*(VCO)*/
	uint64_t delta = 0;
	uint64_t tmp_loop_divider = 0;
	uint64_t output_freq = output_data_rate / 2;
	uint32_t step = 0;
	uint32_t reference_freq = 24000; /*[KHz]*/

	uint8_t i;
	uint8_t no_of_bytes = 0;
	uint8_t range_index = 0;
	uint8_t data[4]; /* maximum data for now are 4 bytes per test mode */
	struct dsi_core_ops *dsi_ops = ctx->ops;
	struct
	{
		uint32_t data_rate; /* upper margin of frequency range */
		uint8_t hs_freq;	/* hsfreqrange */
		uint8_t vco_range;  /* vcorange */
	} ranges[] =
		{
			{80, 0x00, 0x3F},
			{90, 0x10, 0x3F},
			{100, 0x20, 0x3F},
			{110, 0x30, 0x39},
			{120, 0x01, 0x39},
			{130, 0x11, 0x39},
			{140, 0x21, 0x39},
			{150, 0x31, 0x39},
			{160, 0x02, 0x39},
			{170, 0x12, 0x2F},
			{180, 0x22, 0x2F},
			{190, 0x32, 0x2F},
			{205, 0x03, 0x2F},
			{220, 0x13, 0x29},
			{235, 0x23, 0x29},
			{250, 0x33, 0x29},
			{275, 0x04, 0x29},
			{300, 0x14, 0x29},
			{325, 0x25, 0x29},
			{350, 0x35, 0x1F},
			{400, 0x05, 0x1F},
			{450, 0x16, 0x19},
			{500, 0x26, 0x19},
			{550, 0x37, 0x19},
			{600, 0x07, 0x19},
			{640, 0x07, 0x19},
			{650, 0x18, 0x19},
			{700, 0x28, 0x0F},
			{750, 0x39, 0x0F},
			{800, 0x09, 0x0F},
			{850, 0x19, 0x0F},
			{900, 0x29, 0x09},
			{950, 0x3A, 0x09},
			{1000, 0x0A, 0x09},
			{1050, 0x1A, 0x09},
			{1100, 0x2A, 0x09},
			{1150, 0x3B, 0x09},
			{1200, 0x0B, 0x09},
			{1250, 0x1B, 0x09},
			{1300, 0x2B, 0x09},
			{1350, 0x3C, 0x03},
			{1400, 0x0C, 0x03},
			{1450, 0x1C, 0x03},
			{1500, 0x2C, 0x03},
			{1550, 0x3D, 0x03},
			{1600, 0x0D, 0x03},
			{1650, 0x1D, 0x03},
			{1700, 0x2E, 0x03},
			{1750, 0x3E, 0x03},
			{1800, 0x0E, 0x03},
			{1850, 0x1E, 0x03},
			{1900, 0x2F, 0x03},
			{1950, 0x3F, 0x03},
			{2000, 0x0F, 0x03},
			{2050, 0x40, 0x03},
			{2100, 0x41, 0x03},
			{2150, 0x42, 0x03},
			{2200, 0x43, 0x03},
			{2250, 0x44, 0x03},
			{2300, 0x45, 0x01},
			{2350, 0x46, 0x01},
			{2400, 0x47, 0x01},
			{2450, 0x48, 0x01},
			{2500, 0x49, 0x01}};

	if (ctx == NULL)
		return -EINVAL;

	if (output_freq < MIN_OUTPUT_FREQ)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(ranges); i++) {
		if ((output_data_rate / 1000) <= ranges[i].data_rate) {
			range_index = i;
			break;
		}
	}

	if (range_index >= ARRAY_SIZE(ranges)) {
		DRM_ERROR("Your input output_data_rate=%d is out of our ranges",
			output_data_rate);
		return -EINVAL;
	}

	switch (ranges[range_index].vco_range >> 4) {
		case 3:
			vco_divider = 8;
			break;
		case 2:
			vco_divider = 4;
			break;
		default:
			vco_divider = 2;
			break;
	}

	if (ranges[range_index].data_rate > 640)
		vco_divider = 1;

	output_freq = output_freq * vco_divider;

	loop_divider = (output_freq * (reference_freq / DPHY_DIV_LOWER_LIMIT)) / reference_freq;

	/*Here delta will account for the rounding*/
	delta = (loop_divider * reference_freq) / (reference_freq / DPHY_DIV_LOWER_LIMIT) - output_freq;

	for (input_divider = 1 + reference_freq / DPHY_DIV_UPPER_LIMIT;
		((reference_freq / input_divider) >= DPHY_DIV_LOWER_LIMIT) && (!flag);
		input_divider++) {
		tmp_loop_divider = (output_freq * input_divider) / reference_freq;
		if ((tmp_loop_divider % 2) == 0) {
			/*if even*/
			if (output_freq == tmp_loop_divider * (reference_freq / input_divider)) {
				/*exact values found*/
				flag = 1;
				loop_divider = tmp_loop_divider;

				delta = output_freq - tmp_loop_divider * (reference_freq / input_divider);

				/*variable was incremented before exiting the loop*/
				input_divider--;
			}

			if ((output_freq - tmp_loop_divider * (reference_freq / input_divider)) < delta) {
				/* values found with smaller delta */
				loop_divider = tmp_loop_divider;

				delta =
				output_freq - (tmp_loop_divider * (reference_freq / input_divider));
				step = 1;
			}
		} else {
			tmp_loop_divider += 1;

			if (output_freq == tmp_loop_divider * (reference_freq / input_divider)) {
				/*exact values found*/
				flag = 1;
				loop_divider = tmp_loop_divider;
				delta = tmp_loop_divider * (reference_freq / input_divider) - output_freq;

				/*variable was incremented before exiting the loop*/
				input_divider--;
			}

			if ((tmp_loop_divider * (reference_freq / input_divider) - output_freq) < delta) {
				/* values found with smaller delta */
				loop_divider = tmp_loop_divider;
				delta = tmp_loop_divider * (reference_freq / input_divider) - output_freq;
				step = 0;
			}
		}
	}

	if (!flag)
		input_divider = step + (loop_divider * reference_freq) / output_freq;

	/*
	 * Get the PHY in power down mode
	 * (shutdownz = 0) and reset it (rstz = 0) to avoid transient
	 * periods in PHY operation during re-configuration procedures
	 */
	dsi_ops->dphy_reset(ctx, 0);
	dsi_ops->dphy_enableclk(ctx, 0);
	dsi_ops->dphy_shutdown(ctx, 0);

	dsi_ops->dphy_test_clear(ctx, 1);
	dsi_ops->dphy_test_clear(ctx, 0);

#if 1  // this is is_g118
	/* PLL Analog Programmability Control */
	data[0] = 0x01;
	dsi_ops->dphy_write(ctx, 0x1F, data, 1);

	/* setup PLL
	 * Reserved | pll_vco_cntrl_ovr_en [7]| pll_vco_cntrl_ovr[6:0]*/
	data[0] = (1 << 7) | ranges[range_index].hs_freq;
	dsi_ops->dphy_write(ctx, 0x44, data, 1);

	/* PLL Proportional Charge Pump control*/
	if (ranges[range_index].data_rate >= 1150)
		data[0] = 0x0E;
	else
		data[0] = 0x0D;
	dsi_ops->dphy_write(ctx, 0x0e, data, 1);

	/* PLL Integral Charge Pump control*/
	data[0] = 0x0;
	dsi_ops->dphy_write(ctx, 0x0f, data, 1);

	/* PLL Charge Pump Bias Control*/
	data[0] = 0x10;
	dsi_ops->dphy_write(ctx, 0x1c, data, 1);

	/* PLL GMP Control and Digital Testability*/
	data[0] = 0x10;
	dsi_ops->dphy_write(ctx, 0x13, data, 1);

	/* setup PLL
	* Reserved | pll_vco_cntrl_ovr_en | pll_vco_cntrl_ovr*/
	data[0] = (1 << 6) | (ranges[range_index].vco_range);
	dsi_ops->dphy_write(ctx, 0x12, data, 1);

	data[0] = (0x00 << 6) | (0x01 << 5) | (0x01 << 4);
	if (ranges[range_index].data_rate > 1250)
	data[0] = 0x00;
	dsi_ops->dphy_write(ctx, 0x19, data, 1);

	/* PLL input divider ratio [7:0] */
	data[0] = input_divider - 1;
	dsi_ops->dphy_write(ctx, 0x17, data, 1);
	/* pll loop divider (code 0x18)
	* takes only 2 bytes (10 bits in data) */
	no_of_bytes = 2;
	/* 7 is dependent on no_of_bytes make sure 5 bits
	* only of value are written at a time */
	for (i = 0; i < no_of_bytes; i++)
		data[i] = (uint8_t)((((loop_divider - 2) >> (5 * i)) & 0x1F) | (i << 7));

	/* PLL loop divider ratio -
	* SET no|reserved|feedback divider [7]|[6:5]|[4:0] */
	dsi_ops->dphy_write(ctx, 0x18, data, no_of_bytes);
#endif

	data[0]= 6;//50 * 8 * output_data_rate + 1;

	dsi_ops->dphy_write(ctx, 0x60, data, 1);

	data[0] = 6;//50*8/output_data_rate + 1;
	dsi_ops->dphy_write(ctx, 0x70, data, 1);

	data[0] = 0x80;
	dsi_ops->dphy_write(ctx, 0x64, data, 1);

	data[0] = 0x80;
	dsi_ops->dphy_write(ctx, 0x74, data, 1);

	dsi_ops->dphy_n_lanes(ctx, num_of_lanes);
	dsi_ops->dphy_stop_wait_time(ctx, 0x1C);

	dsi_ops->dphy_enableclk(ctx, 1);
	dsi_ops->dphy_shutdown(ctx, 1);
	dsi_ops->dphy_reset(ctx, 1);

	ret = wait_dphy_ready(ctx);
	if (ret < 0) {
		DRM_ERROR("dphy not ready\n");
		return ret;
	}
	return 0;
}

static int mipi_dsih_open(struct dsi_context *ctx)
{
	//int ret;
	struct dsi_core_ops *dsi_ops = ctx->ops;

	mipi_dsih_dphy_open(ctx);

	if (!dsi_ops->check_version(ctx)) {
		DRM_ERROR("dsi version error!\n");
		return -1;
	}

	dsi_ops->power_enable(ctx, 0);
	ctx->max_lanes = 4;
	ctx->max_bta_cycles = 4095;

	dsi_ops->dpi_color_mode_pol(ctx, 0);
	dsi_ops->dpi_shut_down_pol(ctx, 0);

	dsi_ops->int0_mask(ctx, 0x0);
	dsi_ops->int1_mask(ctx, 0x0);
	dsi_ops->max_read_time(ctx, ctx->max_bta_cycles);

	/*
	 * By default, return to LP during ALL
	 * unless otherwise specified
	 */
	dsi_ops->dpi_hporch_lp_en(ctx, 1);
	dsi_ops->dpi_vporch_lp_en(ctx, 1);
	/* by default, all commands are sent in LP */
	dsi_ops->cmd_mode_lp_cmd_en(ctx, 1);
	/* by default, RX_VC = 0, NO EOTp, EOTn, BTA, ECC rx and CRC rx */
	dsi_ops->rx_vcid(ctx, 0);
	dsi_ops->eotp_rx_en(ctx, 0);
	dsi_ops->eotp_tx_en(ctx, 0);
	dsi_ops->bta_en(ctx, 0);
	dsi_ops->ecc_rx_en(ctx, 0);
	dsi_ops->crc_rx_en(ctx, 0);

	dsi_ops->video_mode_lp_cmd_en(ctx, 1);
	dsi_ops->power_enable(ctx, 1);
	/* dividing by 6 is aimed for max PHY frequency, 1GHz */
	dsi_ops->tx_escape_division(ctx, 6); /*need change to calc -- billy*/

	/*ret = mipi_dsih_dphy_configure(ctx, 0, DEFAULT_BYTE_CLOCK);
	if (ret < 0) {
		DRM_ERROR("dphy configure failed\n");
		return -1;
	}*/

	return 0;
}

static void dpi_video_params_init(struct dsi_context *ctx)
{
	struct dsih_dpi_video_t *dpi_video = &ctx->dpi_video;
	uint32_t hs2lp, lp2hs;

	/*init dpi video params*/
	dpi_video->n_lanes = ctx->lane_number;
	dpi_video->burst_mode = ctx->burst_mode;
	dpi_video->phy_freq = ctx->phy_freq;

	dpi_video->color_coding = COLOR_CODE_24BIT;

	hs2lp = MAX_TIME_HS2LP * dpi_video->phy_freq / 8000000;
	hs2lp += (MAX_TIME_HS2LP * dpi_video->phy_freq % 8000000) < 4000000 ? 0 : 1;
	dpi_video->data_hs2lp = hs2lp;

	lp2hs = MAX_TIME_LP2HS * dpi_video->phy_freq / 8000000;
	lp2hs += (MAX_TIME_LP2HS * dpi_video->phy_freq % 8000000) < 4000000 ? 0 : 1;
	dpi_video->data_lp2hs = lp2hs;

	dpi_video->clk_hs2lp = 4;
	dpi_video->clk_lp2hs = 15;

	dpi_video->nc_clk_en = 0;
	dpi_video->frame_ack_en = 0;
	dpi_video->is_18_loosely = 0;
	dpi_video->eotp_rx_en = 0;
	dpi_video->eotp_tx_en = 0;
	dpi_video->dpi_lp_cmd_en = 0;

	dpi_video->virtual_channel= 0;

	dpi_video->h_total_pixels = ctx->vm.hactive + ctx->vm.hback_porch +
		ctx->vm.hfront_porch + ctx->vm.hsync_len;

	dpi_video->h_active_pixels = ctx->vm.hactive;
	dpi_video->h_sync_pixels = ctx->vm.hsync_len;
	dpi_video->h_back_porch_pixels = ctx->vm.hback_porch;

	dpi_video->v_total_lines = ctx->vm.vactive + ctx->vm.vback_porch +
		ctx->vm.vfront_porch + ctx->vm.vsync_len;
	dpi_video->v_active_lines = ctx->vm.vactive;
	dpi_video->v_sync_lines = ctx->vm.vsync_len;
	dpi_video->v_back_porch_lines = ctx->vm.vback_porch;
	dpi_video->v_front_porch_lines = ctx->vm.vfront_porch;

	dpi_video->data_en_polarity = 0;
	dpi_video->h_polarity = 0;
	dpi_video->v_polarity = 0;

	dpi_video->pixel_clock = ctx->vm.pixelclock / 1000;
	dpi_video->byte_clock = dpi_video->phy_freq / 8;
}

static int mipi_dsih_dpi_video(struct dsi_context *ctx)
{
    int error = 0;
    int m, n;
    uint32_t remain = 0;
    uint32_t counter = 0;
    uint32_t bytes_left = 0;
    uint32_t hs_timeout = 0;
    uint16_t video_size = 0;
    uint32_t total_bytes = 0;
    uint16_t no_of_chunks = 0;
    uint32_t chunk_overhead = 0;
    uint8_t video_size_step = 0;
    uint32_t bytes_per_chunk = 0;
    uint32_t ratio_clock_xPF = 0; /* holds dpi clock/byte clock times precision factor */
    uint16_t null_packet_size = 0;
    uint16_t bytes_per_pixel_x100 = 0; /* bpp x 100 because it can be 2.25 */
    uint32_t hsync_time = 0;
    struct dsi_core_ops *dsi_ops = ctx->ops;
    struct dsih_dpi_video_t *dpi_video = &ctx->dpi_video;

    dsi_ops->dphy_datalane_hs2lp_config(ctx, dpi_video->data_hs2lp);
    dsi_ops->dphy_datalane_lp2hs_config(ctx, dpi_video->data_lp2hs);
    dsi_ops->dphy_clklane_hs2lp_config(ctx, dpi_video->clk_hs2lp);
    dsi_ops->dphy_clklane_lp2hs_config(ctx, dpi_video->clk_lp2hs);
    dsi_ops->nc_clk_en(ctx, dpi_video->nc_clk_en);

    mipi_dsih_dphy_configure(ctx, dpi_video->n_lanes, dpi_video->phy_freq);

	ratio_clock_xPF =
		(dpi_video->byte_clock * PRECISION_FACTOR) / (dpi_video->pixel_clock);
    video_size = dpi_video->h_active_pixels;

    dsi_ops->video_mode_frame_ack_en(ctx, dpi_video->frame_ack_en);
    if (dpi_video->frame_ack_en) {
        /*
         * if ACK is requested, enable BTA
         * otherwise leave as is
         */
        dsi_ops->bta_en(ctx, 1);
    }
    dsi_ops->video_mode(ctx);

    /*
     * get bytes per pixel and video size
     * step (depending if loosely or not
     */
    switch (dpi_video->color_coding) {
    case COLOR_CODE_16BIT_CONFIG1:
    case COLOR_CODE_16BIT_CONFIG2:
    case COLOR_CODE_16BIT_CONFIG3:
        bytes_per_pixel_x100 = 200;
        //video_size_step = 1;
        break;
    case COLOR_CODE_18BIT_CONFIG1:
    case COLOR_CODE_18BIT_CONFIG2:
        dsi_ops->dpi_18_loosely_packet_en(ctx, dpi_video->is_18_loosely);
        bytes_per_pixel_x100 = 225;
        if (!dpi_video->is_18_loosely) {
        /*18bits per pixel and NOT loosely,packets should be multiples of 4*/
        //video_size_step = 4;
        /*round up active H pixels to a multiple of 4*/
            for (; (video_size % 4) != 0; video_size++) {
                ;
            }
        } else {
            video_size_step = 1;
        }
        break;
    case COLOR_CODE_24BIT:
        bytes_per_pixel_x100 = 300;
        video_size_step = 1;
        break;
    case COLOR_CODE_20BIT_YCC422_LOOSELY:
        bytes_per_pixel_x100 = 250;
        video_size_step = 2;
        /* round up active H pixels to a multiple of 2 */
        if ((video_size % 2) != 0) {
            video_size += 1;
        }
        break;
    case COLOR_CODE_24BIT_YCC422:
        bytes_per_pixel_x100 = 300;
        video_size_step = 2;
        /* round up active H pixels to a multiple of 2 */
        if ((video_size % 2) != 0) {
            video_size += 1;
        }
        break;
    case COLOR_CODE_16BIT_YCC422:
        bytes_per_pixel_x100 = 200;
        video_size_step = 2;
        /* round up active H pixels to a multiple of 2 */
        if ((video_size % 2) != 0) {
            video_size += 1;
        }
        break;
    case COLOR_CODE_30BIT:
        bytes_per_pixel_x100 = 375;
        video_size_step = 2;
        break;
    case COLOR_CODE_36BIT:
        bytes_per_pixel_x100 = 450;
        video_size_step = 2;
        break;
    case COLOR_CODE_12BIT_YCC420:
        bytes_per_pixel_x100 = 150;
        video_size_step = 2;
        /* round up active H pixels to a multiple of 2 */
        if ((video_size % 2) != 0) {
            video_size += 1;
        }
        break;
    case COLOR_CODE_DSC24:
        bytes_per_pixel_x100 = 300;
        video_size_step = 1;
        break;
    default:
        DRM_ERROR("invalid color coding\n");
        return -1;
    }

    dsi_ops->video_mode_lp_hbp_en(ctx, 0);
    dsi_ops->dpi_color_coding(ctx, dpi_video->color_coding);
    dsi_ops->eotp_rx_en(ctx, dpi_video->eotp_rx_en);
    dsi_ops->eotp_tx_en(ctx, dpi_video->eotp_tx_en);
    dsi_ops->video_mode_lp_cmd_en(ctx, dpi_video->dpi_lp_cmd_en);
    dsi_ops->video_mode_mode_type(ctx, dpi_video->burst_mode);

    hsync_time = dpi_video->h_sync_pixels * ratio_clock_xPF / PRECISION_FACTOR + ((dpi_video->h_sync_pixels * ratio_clock_xPF % PRECISION_FACTOR) ? 1 :0);
    if (dpi_video->burst_mode != VIDEO_BURST_WITH_SYNC_PULSES) {
        int h = (ratio_clock_xPF -  3 * PRECISION_FACTOR / dpi_video->n_lanes) * dpi_video->h_active_pixels;
        int s = h / PRECISION_FACTOR + ((h % PRECISION_FACTOR) ? 1 :0);
         if (s > 0)
            hsync_time += s;
    }
    dsi_ops->dpi_hsync_time(ctx, hsync_time); //hsync
    m = dpi_video->h_back_porch_pixels * ratio_clock_xPF;
    n = m / PRECISION_FACTOR + ((m % PRECISION_FACTOR) ? 1 :0);
    dsi_ops->dpi_hbp_time(ctx, n);//hbp
    dsi_ops->dpi_hline_time(ctx, dpi_video->h_total_pixels * dpi_video->byte_clock / dpi_video->pixel_clock); //hsync+hbp_hact+hfp



    dsi_ops->dpi_vsync(ctx, dpi_video->v_sync_lines);//vsync
    dsi_ops->dpi_vbp(ctx, dpi_video->v_back_porch_lines);//vbp
    dsi_ops->dpi_vfp(ctx, dpi_video->v_front_porch_lines);//vfp
    dsi_ops->dpi_vact(ctx, dpi_video->v_active_lines);//vact

    dsi_ops->dpi_hsync_pol(ctx, dpi_video->h_polarity);
    dsi_ops->dpi_vsync_pol(ctx, dpi_video->v_polarity);
    dsi_ops->dpi_data_en_pol(ctx, dpi_video->data_en_polarity);

    hs_timeout = (dpi_video->h_total_pixels * dpi_video->v_active_lines) +
        (DSIH_PIXEL_TOLERANCE * bytes_per_pixel_x100) / 100;
    for (counter = 0x80; (counter < hs_timeout) && (counter > 2); counter--) {
        if (hs_timeout % counter == 0) {
            dsi_ops->timeout_clock_division(ctx, counter + 1);
            dsi_ops->lp_rx_timeout(ctx, (uint16_t)(hs_timeout / counter));
            dsi_ops->hs_tx_timeout(ctx, (uint16_t)(hs_timeout / counter));
            break;
        }
    }
    dsi_ops->tx_escape_division(ctx, 6);

    /*video packetisation*/
    if (dpi_video->burst_mode == VIDEO_BURST_WITH_SYNC_PULSES) {
        /*BURST*/
        DRM_INFO("INFO: burst video\n");
        dsi_ops->dpi_null_packet_size(ctx, 0);
        dsi_ops->dpi_chunk_num(ctx, 1);
        dsi_ops->dpi_video_packet_size(ctx, video_size);

        /*
         * BURST by default returns to LP during
         * ALL empty periods - energy saving
         */
        dsi_ops->dpi_hporch_lp_en(ctx, 1);
        dsi_ops->dpi_vporch_lp_en(ctx, 1);

        DRM_INFO("INFO: h line time -> %d\n",
            (uint16_t)dpi_video->h_total_pixels);
        DRM_INFO("INFO: video_size -> %d\n", video_size);
    } else {
        /*NON BURST*/
        DRM_INFO("INFO: non burst video\n");

        null_packet_size = 0;
        /* Bytes to be sent - first as one chunk */
        bytes_per_chunk = bytes_per_pixel_x100 * dpi_video->h_active_pixels / 100
            + VIDEO_PACKET_OVERHEAD + NULL_PACKET_OVERHEAD;

        total_bytes = bytes_per_pixel_x100 * dpi_video->n_lanes *
            dpi_video->h_total_pixels / 100;

        if (total_bytes >= bytes_per_chunk) {
            chunk_overhead = total_bytes - bytes_per_chunk -
                VIDEO_PACKET_OVERHEAD - NULL_PACKET_OVERHEAD;
            DRM_INFO("DSI INFO: overhead %d -> enable multi packets\n",
                chunk_overhead);

            if (!(chunk_overhead > 1)) {
                /* MULTI packets */
                DRM_INFO("DSI INFO: multi packets\n");
                DRM_INFO("DSI INFO: video_size -> %d\n", video_size);
                DRM_INFO("DSI INFO: video_size_step -> %d\n", video_size_step);
                for (video_size = video_size_step;
                    video_size < dpi_video->h_active_pixels;
                    video_size += video_size_step) {
                    DRM_INFO("DSI INFO: determine no of chunks\n");
                    DRM_INFO("DSI INFO: video_size -> %d\n", video_size);
                    remain = (dpi_video->h_active_pixels * PRECISION_FACTOR /
                        video_size) % PRECISION_FACTOR;
                    DRM_INFO("DSI INFO: remain -> %d", remain);
                    if (remain == 0) {
                        no_of_chunks = dpi_video->h_active_pixels / video_size;
                        DRM_INFO("DSI INFO: no_of_chunks -> %d", no_of_chunks);
                        bytes_per_chunk =  bytes_per_pixel_x100 *
                            video_size / 100 + VIDEO_PACKET_OVERHEAD;
                        DRM_INFO("DSI INFO: bytes_per_chunk -> %d\n",
                            bytes_per_chunk);
                        if (total_bytes >= (bytes_per_chunk * no_of_chunks)) {
                            bytes_left = total_bytes -
                                (bytes_per_chunk * no_of_chunks);
                            DRM_INFO("DSI INFO: bytes_left -> %d", bytes_left);
                            break;
                        }
                    }
                }
                if (bytes_left > (NULL_PACKET_OVERHEAD * no_of_chunks)) {
                    null_packet_size = (bytes_left -
                        (NULL_PACKET_OVERHEAD * no_of_chunks)) / no_of_chunks;
                    if (null_packet_size > MAX_NULL_SIZE) {
                        /* avoid register overflow */
                        null_packet_size = MAX_NULL_SIZE;
                    }
                } else {
                    /* no multi packets */
                    no_of_chunks = 1;

                    DRM_INFO("DSI INFO: no multi packets\n");
                    DRM_INFO("DSI INFO: horizontal line time -> %d\n",
                             (uint16_t)((dpi_video->h_total_pixels *
                             ratio_clock_xPF) / PRECISION_FACTOR));
                    DRM_INFO("DSI INFO: video size -> %d\n", video_size);

                    /* video size must be a multiple of 4 when not 18 loosely */
                    for (video_size = dpi_video->h_active_pixels;
                         (video_size % video_size_step) != 0;
                         video_size++)
                        ;
                }
            }
            dsi_ops->dpi_null_packet_size(ctx, null_packet_size);
            dsi_ops->dpi_chunk_num(ctx, no_of_chunks);
            dsi_ops->dpi_video_packet_size(ctx, video_size);
        } else {
            DRM_ERROR("resolution cannot be sent to display through current settings\n");
            error = -1;
        }
    }

    dsi_ops->video_vcid(ctx, dpi_video->virtual_channel);
    dsi_ops->dphy_n_lanes(ctx, dpi_video->n_lanes);

    /*enable high speed clock*/
    //dsi_ops->dphy_enable_hs_clk(ctx, 1);//panel not init yet, could not set to hs

    return error;
}

/*drm funcs*/
static int sdriv_dsi_find_panel(struct sdrv_mipi_dsi *dsi)
{
	struct device *dev = dsi->dsi_host.dev;
	struct device_node *child, *lcds_node;
	struct drm_panel *panel;

	/* search /lcds child node first */
	lcds_node = of_find_node_by_path("/lcds");
	for_each_child_of_node(lcds_node, child) {
		panel = of_drm_find_panel(child);
		if (panel) {
			dsi->panel = panel;
			return 0;
		}
	}

	/*
	 * If /lcds child node search failed, we search
	 * the child of dsi host node.
	 */
	for_each_child_of_node(dev->of_node, child) {
		panel = of_drm_find_panel(child);
		if (panel) {
			dsi->panel = panel;
			return 0;
		}
	}

	DRM_ERROR("of_drm_find_panel() failed\n");
	return -ENODEV;
}

static int sdrv_mipi_dsi_host_attach(struct mipi_dsi_host *host,
	struct mipi_dsi_device *slave)
{
	struct sdrv_mipi_dsi *dsi = host_to_dsi(host);
	struct dsi_context *ctx = &dsi->ctx;
	struct device_node *lcd_node;
	uint32_t val;
	int ret;

	DRM_INFO("%s()\n", __func__);

	dsi->slave = slave;

	ctx->lane_number = slave->lanes;

	/*ret = sdriv_dsi_find_panel(dsi);
	if (ret)
		return ret;

	spanel = = container_of(dsi->panel, struct sdrv_panel, panel)*/

	if (slave->mode_flags & MIPI_DSI_MODE_VIDEO)
		ctx->work_mode = DSI_MODE_VIDEO;
	else
		ctx->work_mode = DSI_MODE_CMD;

	if (slave->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
		ctx->burst_mode = VIDEO_BURST_WITH_SYNC_PULSES;
	else if (slave->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
		ctx->burst_mode = VIDEO_NON_BURST_WITH_SYNC_PULSES;
	else
		ctx->burst_mode = VIDEO_NON_BURST_WITH_SYNC_EVENTS;

	ret = sdriv_dsi_find_panel(dsi);
	if (ret)
		return ret;

	lcd_node = dsi->panel->dev->of_node;

	ret = of_property_read_u32(lcd_node, "semidrive,phy_freq", &val);
	if (!ret) {
		ctx->phy_freq = val * 1000;
	} else {
		ctx->phy_freq = 500000;
	}

	return 0;
}

static int sdrv_mipi_dsi_host_detach(struct mipi_dsi_host *host,
	struct mipi_dsi_device *device)
{
	DRM_INFO("%s()\n", __func__);
	/* do nothing */
	return 0;
}

static ssize_t sdrv_mipi_dsi_host_transfer(struct mipi_dsi_host *host,
	const struct mipi_dsi_msg *msg)
{
	/*billy*/
	return 0;
}

static void sdrv_mipi_dsi_encoder_enable(struct drm_encoder *encoder)
{
	struct sdrv_mipi_dsi *dsi = enc_to_dsi(encoder);
	struct dsi_context *ctx = &dsi->ctx;
	int ret;

	DRM_INFO("%s()\n", __func__);

	if (ctx->is_inited)
		return;

	ret = mipi_dsih_open(ctx);
	if (ret < 0) {
		DRM_ERROR("mipi dsih open failed\n");
		return;
	}

	dpi_video_params_init(ctx);

	ret = mipi_dsih_dpi_video(ctx);
	if (ret < 0) {
		DRM_ERROR("dsi: mipi_dsih_dpi_video failed\n");
	}

	if (dsi->panel) {
		drm_panel_prepare(dsi->panel);
		drm_panel_enable(dsi->panel);
	}

	ctx->ops->dphy_enable_hs_clk(ctx, 1);


	ctx->ops->power_enable(ctx, 0);
	udelay(100);
	ctx->ops->power_enable(ctx, 1);
	udelay(10*1000);

	ctx->ops->video_mode(ctx);

}

static void sdrv_mipi_dsi_encoder_disable(struct drm_encoder *encoder)
{
	/*billy*/
}

static int sdrv_mipi_dsi_encoder_atomic_check(struct drm_encoder *encoder,
			struct drm_crtc_state *crtc_state,
			struct drm_connector_state *conn_state)
{
	DRM_INFO("%s()\n", __func__);

	return 0;
}

static void sdrv_mipi_dsi_connector_destory(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static int sdrv_mipi_dsi_connector_get_modes(struct drm_connector *connector)
{
	struct sdrv_mipi_dsi *dsi = con_to_dsi(connector);
	struct device_node *np = dsi->dev->of_node;
	struct drm_display_mode *mode;
	int ret, num_modes = 0;

	num_modes = drm_panel_get_modes(dsi->panel);
	if(num_modes > 0)
		return num_modes;

	num_modes = 0;

	if(np) {
		ret = of_get_drm_display_mode(np, dsi->mode, NULL,
			OF_USE_NATIVE_MODE);
		if(ret)
			return ret;

		mode = drm_mode_create(connector->dev);
		if(!mode)
			return -ENOMEM;

		drm_mode_copy(mode, dsi->mode);
		mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
		num_modes++;
	}

	return num_modes;
}

static enum drm_mode_status
sdrv_mipi_dsi_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	struct sdrv_mipi_dsi *dsi = con_to_dsi(connector);

	DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__, DRM_MODE_ARG(mode));

	if (mode->type & DRM_MODE_TYPE_PREFERRED) {
		dsi->mode = mode;
		drm_display_mode_to_videomode(dsi->mode, &dsi->ctx.vm);
	}

	return MODE_OK;
}

static struct drm_encoder *
sdrv_mipi_dsi_connector_best_encoder(struct drm_connector *connector)
{
	struct sdrv_mipi_dsi *dsi = con_to_dsi(connector);

	DRM_INFO("%s()\n", __func__);
	return &dsi->encoder;
}

static const struct mipi_dsi_host_ops sdrv_mipi_dsi_host_ops = {
	.attach = sdrv_mipi_dsi_host_attach,
	.detach = sdrv_mipi_dsi_host_detach,
	.transfer = sdrv_mipi_dsi_host_transfer,
};

static const struct drm_encoder_funcs sdrv_mipi_dsi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs sdrv_mipi_dsi_encoder_helper_funcs = {
	.enable = sdrv_mipi_dsi_encoder_enable,
	.disable = sdrv_mipi_dsi_encoder_disable,
	.atomic_check = sdrv_mipi_dsi_encoder_atomic_check,
};

static const struct drm_connector_funcs sdrv_mipi_dsi_connector_funcs= {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = sdrv_mipi_dsi_connector_destory,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs sdrv_mipi_dsi_connector_helper_funcs = {
	.get_modes = sdrv_mipi_dsi_connector_get_modes,
	.mode_valid = sdrv_mipi_dsi_connector_mode_valid,
	.best_encoder = sdrv_mipi_dsi_connector_best_encoder,
};

static irqreturn_t dsi_irq_handler(int irq, void *data)
{

	return 0;
}

static int sdrv_mipi_get_resource(struct sdrv_mipi_dsi *dsi)
{
	struct device *dev = dsi->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *regs;
	int ret;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dsi->ctx.base = devm_ioremap(dev, regs->start, resource_size(regs));
	if (!dsi->ctx.base) {
		DRM_ERROR("Cannot find dsi base regs.\n");
		return -EADDRNOTAVAIL;
	}

	dsi->ctx.irq = platform_get_irq(pdev, 0);
	if (dsi->ctx.irq < 0) {
		DRM_ERROR("Cannot find irq for DSI.\n");
		return dsi->ctx.irq;
	}

	ret = request_irq(dsi->ctx.irq, dsi_irq_handler, 0, "DSI_INT", dsi);
	if (ret) {
		DRM_ERROR("dsi failed to request irq int!\n");
		return -EINVAL;
	}

	return 0;
}

static int sdrv_mipi_dsi_encoder_init(struct drm_device *drm,
			struct sdrv_mipi_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct device *dev = dsi->dev;
	int ret;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	if(!encoder->possible_crtcs) {
		DRM_ERROR("failed to find crtc mask\n");
		return -EPROBE_DEFER;
	}
	DRM_INFO("Find possible crtcs: 0x%08x\n", encoder->possible_crtcs);

	ret = drm_encoder_init(drm, encoder, &sdrv_mipi_dsi_encoder_funcs,
			DRM_MODE_ENCODER_DSI, NULL);
	if(ret) {
		DRM_ERROR("Failed to initialize encoder with drm\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &sdrv_mipi_dsi_encoder_helper_funcs);

	return 0;
}

static int __maybe_unused sdrv_mipi_dsi_bridge_attach(struct sdrv_mipi_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct device *dev = dsi->dsi_host.dev;
	struct device_node *node;
	int ret;

	node = of_graph_get_remote_node(dev->of_node, 2, 0);
	if (!node)
		return 0;

	dsi->bridge = of_drm_find_bridge(node);
	if (!dsi->bridge) {
		DRM_ERROR("drm find bridge failed.\n");
		return -ENODEV;
	}

	ret = drm_bridge_attach(encoder, dsi->bridge, NULL);
	if (ret) {
		DRM_ERROR("encoder failed to attach external bridge.\n");
		return ret;
	}

	return 0;
}

static int sdrv_mipi_dsi_connector_init(struct drm_device *drm,
			struct sdrv_mipi_dsi *dsi)
{
	struct drm_connector *connector = &dsi->connector;
	struct drm_encoder *encoder = &dsi->encoder;
	int ret;

	if(dsi->bridge) {
		ret = drm_bridge_attach(encoder, dsi->bridge, NULL);
		if(ret < 0) {
			DRM_ERROR("Failed to attach bridge\n");
			return ret;
		}
	} else {
		ret = drm_connector_init(drm, connector, &sdrv_mipi_dsi_connector_funcs,
					DRM_MODE_CONNECTOR_DSI);
		if (ret) {
			DRM_ERROR("drm_connector_init() failed\n");
			return ret;
		}

		drm_connector_helper_add(connector, &sdrv_mipi_dsi_connector_helper_funcs);

		drm_mode_connector_attach_encoder(connector, encoder);
	}

	if(dsi->panel){
		drm_panel_attach(dsi->panel, connector);
	}

	return 0;
}

static int sdrv_mipi_dsi_register(struct drm_device *drm,
			struct sdrv_mipi_dsi *dsi)
{
	int ret;

	ret = sdrv_mipi_dsi_encoder_init(drm, dsi);
	if (ret)
		goto cleanup_host;

	ret = sdrv_mipi_dsi_connector_init(drm, dsi);
	if (ret)
		goto cleanup_encoder;

	return 0;

cleanup_encoder:
	drm_encoder_cleanup(&dsi->encoder);
cleanup_host:
	mipi_dsi_host_unregister(&dsi->dsi_host);

	return ret;
}

static int sdrv_mipi_dsi_bind(struct device *dev,
			struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sdrv_mipi_dsi *dsi;
	int ret;
	DRM_INFO("sdrv_mipi_dsi_bind\n");

	dsi = devm_kzalloc(dev, sizeof(struct sdrv_mipi_dsi), GFP_KERNEL);
	if(!dsi) {
		DRM_ERROR("failed to allocate dsi data.\n");
		return -ENOMEM;
	}

	dsi->dev = dev;
	ret = sdrv_mipi_get_resource(dsi);
	if (ret) {
		DRM_ERROR("dsi get resouce failed.\n");
		return ret;
	}

	ret = drm_of_find_panel_or_bridge(dev->of_node, 2, 0,
			&dsi->panel, &dsi->bridge);
	if(ret && ret != -ENODEV) {
		DRM_ERROR("Cannot find effective panel or bridge\n");
		return ret;
	}

	dsi->ctx.ops = &dwc_mipi_dsi_host_ops;
	dsi->dsi_host.dev = dev;
	dsi->dsi_host.ops = &sdrv_mipi_dsi_host_ops;

	ret = mipi_dsi_host_register(&dsi->dsi_host);
	if (ret) {
		DRM_ERROR("failed to register dsi host.\n");
		return ret;
	}

	ret = sdrv_mipi_dsi_register(drm, dsi);
	if(ret) {
		DRM_ERROR("failed to register drm dsi.\n");
		return ret;
	}

	dev_set_drvdata(dev, dsi);

	return 0;
}

static void sdrv_mipi_dsi_unbind(struct device *dev,
			struct device *master, void *data)
{
	struct sdrv_mipi_dsi *dsi = dev_get_drvdata(dev);

	mipi_dsi_host_unregister(&dsi->dsi_host);
}


static const struct component_ops sdrv_mipi_dsi_ops = {
	.bind = sdrv_mipi_dsi_bind,
	.unbind = sdrv_mipi_dsi_unbind,
};

static int sdrv_mipi_dsi_probe(struct platform_device *pdev)
{
	DRM_INFO("sdrv_mipi_dsi_probe.\n");
	return component_add(&pdev->dev, &sdrv_mipi_dsi_ops);
}

static int sdrv_mipi_dsi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &sdrv_mipi_dsi_ops);
	return 0;
}

static const struct of_device_id sdrv_mipi_dsi_ids[] = {
	{ .compatible = "semidrive,dsi-host",},
	{  },
};
MODULE_DEVICE_TABLE(of, sdrv_mipi_dsi_ids);

static struct platform_driver sdrv_mipi_dsi_driver = {
	.probe = sdrv_mipi_dsi_probe,
	.remove = sdrv_mipi_dsi_remove,
	.driver = {
		.of_match_table = sdrv_mipi_dsi_ids,
		.name = SDRV_MIPI_DSI
	},
};

module_platform_driver(sdrv_mipi_dsi_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive DRM DSI");
MODULE_LICENSE("GPL");
