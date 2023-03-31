/*
 * dwc_mipi_dsi_host.h
 *
 * Semidrive platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#ifndef _DWC_MIPI_DSI_HOST_H_
#define _DWC_MIPI_DSI_HOST_H_

#include <asm/types.h>
#include <linux/io.h>
#include <video/videomode.h>

struct dsi_reg {
	union _0x00 {
		uint32_t val;
		struct _VERSION {
		uint32_t version: 32;
		} bits;
	} VERSION;

	union _0x04 {
		uint32_t val;
		struct _PWR_UP {
		/*
		 * This bit configures the core either to work normal or to
		 * reset. It's default value is 0. After the core configur-
		 * ation, to enable the mipi_dsi_host, set this register to 1.
		 * 1: power up     0: reset core
		 */
		uint32_t shutdownz: 1;

		uint32_t reserved: 31;
		} bits;
	} PWR_UP;

	union _0x08 {
		uint32_t val;
		struct _CLKMGR_CFG {
		/*
		 * This field indicates the division factor for the TX Escape
		 * clock source (lanebyteclk). The values 0 and 1 stop the
		 * TX_ESC clock generation.
		 */
		uint32_t tx_esc_clk_division: 8;

		/*
		 * This field indicates the division factor for the Time Out
		 * clock used as the timing unit in the configuration of HS to
		 * LP and LP to HS transition error.
		 */
		uint32_t to_clk_division: 8;

		uint32_t reserved: 16;

		} bits;
	} CLKMGR_CFG;

	union _0x0C {
		uint32_t val;
		struct _DPI_VCID {
		/* This field configures the DPI virtual channel id that
		 * is indexed to the VIDEO mode packets
		 */
		uint32_t dpi_vcid: 2;

		uint32_t reserved: 30;

		} bits;
	} DPI_VCID;

	union _0x10 {
		uint32_t val;
		struct _DPI_COLOR_CODING {
		/*
		 * This field configures the DPI color coding as follows:
		 * 0000: 16-bit configuration 1
		 * 0001: 16-bit configuration 2
		 * 0010: 16-bit configuration 3
		 * 0011: 18-bit configuration 1
		 * 0100: 18-bit configuration 2
		 * 0101: 24-bit
		 * 0110: 20-bit YCbCr 4:2:2 loosely packed
		 * 0111: 24-bit YCbCr 4:2:2
		 * 1000: 16-bit YCbCr 4:2:2
		 * 1001: 30-bit-DSC_ENC 10bit
		 * 1010: 36-bit
		 * 1011: 12-bit YCbCr 4:2:0
		 * 1100: DSC24 compressed Data
		 * 1101-1111: 12-bit YCbCr 4:2:0
		 */
		uint32_t dpi_color_coding: 4;

		uint32_t reserved0: 4;

		/* When set to 1, this bit activates loosely packed
		 * variant to 18-bit configurations
		 */
		uint32_t loosely18_en: 1;

		uint32_t reserved1: 23;

		} bits;
	} DPI_COLOR_CODING;

	union _0x14 {
		uint32_t val;
		struct _DPI_CFG_POL {
		/* When set to 1, this bit configures the data enable
		 * pin (dpidataen) asactive low.
		 */
		uint32_t dataen_active_low: 1;

		/* When set to 1, this bit configures the vertical
		 * synchronism pin (dpivsync) as active low.
		 */
		uint32_t vsync_active_low: 1;

		/* When set to 1, this bit configures the horizontal
		 * synchronism pin (dpihsync) as active low.
		 */
		uint32_t hsync_active_low: 1;

		/* When set to 1, this bit configures the shutdown pin
		 * (dpishutdn) as active low.
		 */
		uint32_t shutd_active_low: 1;

		/* When set to 1, this bit configures the color mode pin
		 * (dpicolorm) as active low.
		 */
		uint32_t colorm_active_low: 1;

		uint32_t reserved: 27;

		} bits;
	} DPI_CFG_POL;

	union _0x18 {
		uint32_t val;
		struct _DPI_LP_CMD_TIM {
		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VACT
		 * region.
		 */
		uint32_t invact_lpcmd_time: 8;

		uint32_t reserved0: 8;

		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VSA, VBP,
		 * and VFP regions.
		 */
		uint32_t outvact_lpcmd_time: 8;

		uint32_t reserved1: 8;

		} bits;
	} DPI_LP_CMD_TIM;

	union _0x1C {
		uint32_t val;
		struct _DBI_VCID {
		uint32_t dbi_vcid: 2;
		uint32_t reserved0: 30;
		} bits;
	} DBI_VCID;

	union _0x20 {
		uint32_t val;
		struct _DBI_CFG {
		uint32_t in_dbi_conf: 4;
		uint32_t reserved0: 4;
		uint32_t out_dbi_conf: 4;
		uint32_t reserved1: 4;
		uint32_t lut_size_conf: 2;
		uint32_t reserved2: 14;
		} bits;
	} DBI_CFG;

	union _0x24 {
		uint32_t val;
		struct _DBI_PARTITIONING_EN {
		uint32_t partitioning_en: 1;
		uint32_t reserved0: 31;
		} bits;
	} DBI_PARTITIONING_EN;

	union _0x28 {
		uint32_t val;
		struct _DBI_CMDSIZE {
		uint32_t wr_cmd_size: 16;
		uint32_t allowed_cmd_size: 16;
		} bits;
	} DBI_CMDSIZE;

	union _0x2C {
		uint32_t val;
		struct _PCKHDL_CFG {
		/* When set to 1, this bit enables the EoTp transmission */
		uint32_t eotp_tx_en: 1;

		/* When set to 1, this bit enables the EoTp reception. */
		uint32_t eotp_rx_en: 1;

		/* When set to 1, this bit enables the Bus Turn-Around (BTA)
		 * request.
		 */
		uint32_t bta_en: 1;

		/* When set to 1, this bit enables the ECC reception, error
		 * correction, and reporting.
		 */
		uint32_t ecc_rx_en: 1;

		/* When set to 1, this bit enables the CRC reception and error
		 * reporting.
		 */
		uint32_t crc_rx_en: 1;

		/* When set to 1,this bit enables the EoTp transmission in
		 * Low-Power.
		 */
		uint32_t eotp_tx_lp_en: 1;

		uint32_t reserved: 26;

		} bits;
	} PCKHDL_CFG;

	union _0x30 {
		uint32_t val;
		struct _GEN_VCID {
		/* This field indicates the Generic interface read-back
		 * virtual channel identification
		 */
		uint32_t gen_vcid_rx: 2;

		uint32_t reserved0: 6;

		/* This field indicates the virtual channel identificaton
		 * for tear effect by hardware
		 */
		uint32_t gen_vcid_tear_auto: 2;

		uint32_t reserved1: 6;

		/* This field indicates the Generic interface virtual channel
		 * identification where generic packet is automatically
		 * generated & transmitted
		 */
		uint32_t gen_vcid_tx_auto: 2;

		uint32_t reserved2: 14;

		} bits;
	} GEN_VCID;

	union _0x34 {
		uint32_t val;
		struct _MODE_CFG {
		/* This bit configures the operation mode
		 * 0: Video mode ;   1: Command mode
		 */
		uint32_t cmd_video_mode: 1;

		uint32_t reserved: 31;

		} bits;
	} MODE_CFG;

	union _0x38 {
		uint32_t val;
		struct _VID_MODE_CFG {
		/*
		 * This field indicates the video mode transmission type as
		 * follows:
		 * 00: Non-burst with sync pulses
		 * 01: Non-burst with sync events
		 * 10 and 11: Burst mode
		 */
		uint32_t vid_mode_type: 2;

		uint32_t reserved_0: 6;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VSA period when timing allows.
		 */
		uint32_t lp_vsa_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VBP period when timing allows.
		 */
		uint32_t lp_vbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VFP period when timing allows.
		 */
		uint32_t lp_vfp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VACT period when timing allows.
		 */
		uint32_t lp_vact_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HBP period when timing allows.
		 */
		uint32_t lp_hbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HFP period when timing allows.
		 */
		uint32_t lp_hfp_en: 1;

		/* When set to 1, this bit enables the request for an ack-
		 * nowledge response at the end of a frame.
		 */
		uint32_t frame_bta_ack_en: 1;

		/* When set to 1, this bit enables the command transmission
		 * only in low-power mode.
		 */
		uint32_t lp_cmd_en: 1;

		/* When set to 1, this bit enables the video mode pattern
		 * generator
		 */
		uint32_t vpg_en: 1;

		uint32_t reserved_1: 3;

		/* This field is to select the pattern:
		 * 0: Color bar (horizontal or vertical)
		 * 1: BER pattern (vertical only)
		 */
		uint32_t vpg_mode: 1;

		uint32_t reserved_2: 3;

		/* This field indicates the color bar orientation as follows:
		 * 0: Vertical mode
		 * 1: Horizontal mode
		 */
		uint32_t vpg_orientation: 1;

		uint32_t reserved_3: 7;

		} bits;
	} VID_MODE_CFG;

	union _0x3C {
		uint32_t val;
		struct _VID_PKT_SIZE {
		/*
		 * This field configures the number of pixels in a single
		 * video packet. For 18-bit not loosely packed data types,
		 * this number must be a multiple of 4. For YCbCr data
		 * types, it must be a multiple of 2, as described in the
		 * DSI specification.
		 */
		uint32_t vid_pkt_size: 14;

		uint32_t reserved: 18;

		} bits;
	} VID_PKT_SIZE;

	union _0x40 {
		uint32_t val;
		struct _VID_NUM_CHUNKS {
		/*
		 * This register configures the number of chunks to be
		 * transmitted during a Line period (a chunk consists of
		 * a video packet and a null packet). If set to 0 or 1,
		 * the video line is transmitted in a single packet. If
		 * set to 1, the packet is part of a chunk, so a null packet
		 * follows it if vid_null_size > 0. Otherwise, multiple chunks
		 * are used to transmit each video line.
		 */
		uint32_t vid_num_chunks: 13;

		uint32_t reserved: 19;

		} bits;
	} VID_NUM_CHUNKS;

	union _0x44 {
		uint32_t val;
		struct _VID_NULL_SIZE {
		/*
		 * This register configures the number of bytes inside a null
		 * packet. Setting it to 0 disables the null packets.
		 */
		uint32_t vid_null_size: 13;

		uint32_t reserved: 19;

		} bits;
	} VID_NULL_SIZE;

	union _0x48 {
		uint32_t val;
		struct _VID_HSA_TIME {
		/* This field configures the Horizontal Synchronism Active
		 * period in lane byte clock cycles
		 */
		uint32_t vid_hsa_time: 12;

		uint32_t reserved: 20;

		} bits;
	} VID_HSA_TIME;

	union _0x4C {
		uint32_t val;
		struct _VID_HBP_TIME {
		/* This field configures the Horizontal Back Porch period
		 * in lane byte clock cycles
		 */
		uint32_t vid_hbp_time: 12;

		uint32_t reserved: 20;

		} bits;
	} VID_HBP_TIME;

	union _0x50 {
		uint32_t val;
		struct _VID_HLINE_TIME {
		/* This field configures the size of the total line time
		 * (HSA+HBP+HACT+HFP) counted in lane byte clock cycles
		 */
		uint32_t vid_hline_time: 15;

		uint32_t reserved: 17;

		} bits;
	} VID_HLINE_TIME;

	union _0x54 {
		uint32_t val;
		struct _VID_VSA_LINES {
		/* This field configures the Vertical Synchronism Active
		 * period measured in number of horizontal lines
		 */
		uint32_t vsa_lines: 10;

		uint32_t reserved: 22;

		} bits;
	} VID_VSA_LINES;

	union _0x58 {
		uint32_t val;
		struct _VID_VBP_LINES {
		/* This field configures the Vertical Back Porch period
		 * measured in number of horizontal lines
		 */
		uint32_t vbp_lines: 10;

		uint32_t reserved: 22;

		} bits;
	} VID_VBP_LINES;

	union _0x5C {
		uint32_t val;
		struct _VID_VFP_LINES {
		/* This field configures the Vertical Front Porch period
		 * measured in number of horizontal lines
		 */
		uint32_t vfp_lines: 10;

		uint32_t reserved: 22;

		} bits;
	} VID_VFP_LINES;

	union _0x60 {
		uint32_t val;
		struct _VID_VACTIVE_LINES {
		/* This field configures the Vertical Active period measured
		 * in number of horizontal lines
		 */
		uint32_t v_active_lines: 14;

		uint32_t reserved: 18;

		} bits;
	} VID_VACTIVE_LINES;

	union _0x64 {
		uint32_t val;
		struct _EDPI_CMD_SIZE {
		/*
		 * This field configures the maximum allowed size for an eDPI
		 * write memory command, measured in pixels. Automatic parti-
		 * tioning of data obtained from eDPI is permanently enabled.
		 */
		uint32_t edpi_allowed_cmd_size: 16;

		uint32_t reserved: 16;
		} bits;
	} EDPI_CMD_SIZE;

	union _0x68 {
		uint32_t val;
		struct _CMD_MODE_CFG {
		/*
		 * When set to 1, this bit enables the tearing effect
		 * acknowledge request.
		 */
		uint32_t tear_fx_en: 1;

		/*
		 * When set to 1, this bit enables the acknowledge request
		 * after each packet transmission.
		 */
		uint32_t ack_rqst_en: 1;

		uint32_t reserved_0: 6;

		/*
		 * This bit configures the Generic short write packet with
		 * zero parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t gen_sw_0p_tx: 1;

		/*
		 * This bit configures the Generic short write packet with
		 * one parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t gen_sw_1p_tx: 1;

		/*
		 * This bit configures the Generic short write packet with
		 * two parameters command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t gen_sw_2p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * zero parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t gen_sr_0p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * one parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t gen_sr_1p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * two parameters command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t gen_sr_2p_tx: 1;

		/*
		 * This bit configures the Generic long write packet command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t gen_lw_tx: 1;

		uint32_t reserved_1: 1;

		/*
		 * This bit configures the DCS short write packet with zero
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t dcs_sw_0p_tx: 1;

		/*
		 * This bit configures the DCS short write packet with one
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t dcs_sw_1p_tx: 1;

		/*
		 * This bit configures the DCS short read packet with zero
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t dcs_sr_0p_tx: 1;

		/*
		 * This bit configures the DCS long write packet command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t dcs_lw_tx: 1;

		uint32_t reserved_2: 4;

		/*
		 * This bit configures the maximum read packet size command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		uint32_t max_rd_pkt_size: 1;

		uint32_t reserved_3: 7;

		} bits;
	} CMD_MODE_CFG;

	union _0x6C {
		uint32_t val;
		struct _GEN_HDR {
		/*
		 * This field configures the packet data type of the header
		 * packet.
		 */
		uint32_t gen_dt: 6;

		/*
		 * This field configures the virtual channel id of the header
		 * packet.
		 */
		uint32_t gen_vc: 2;

		/*
		 * This field configures the least significant byte of the
		 * header packet's Word count for long packets or data 0 for
		 * short packets.
		 */
		uint32_t gen_wc_lsbyte: 8;

		/*
		 * This field configures the most significant byte of the
		 * header packet's word count for long packets or data 1 for
		 * short packets.
		 */
		uint32_t gen_wc_msbyte: 8;

		uint32_t reserved: 8;

		} bits;
	} GEN_HDR;

	union _0x70 {
		uint32_t val;
		struct _GEN_PLD_DATA {
		/* This field indicates byte 1 of the packet payload. */
		uint32_t gen_pld_b1: 8;

		/* This field indicates byte 2 of the packet payload. */
		uint32_t gen_pld_b2: 8;

		/* This field indicates byte 3 of the packet payload. */
		uint32_t gen_pld_b3: 8;

		/* This field indicates byte 4 of the packet payload. */
		uint32_t gen_pld_b4: 8;

		} bits;
	} GEN_PLD_DATA;

	union _0x74 {
		uint32_t val;
		struct _CMD_PKT_STATUS {
		/*
		 * This bit indicates the empty status of the generic
		 * command FIFO.
		 * Value after reset: 0x1
		 */
		uint32_t gen_cmd_empty: 1;

		/*
		 * This bit indicates the full status of the generic
		 * command FIFO.
		 * Value after reset: 0x0
		 */
		uint32_t gen_cmd_full: 1;

		/*
		 * This bit indicates the empty status of the generic write
		 * payload FIFO.
		 * Value after reset: 0x1
		 */
		uint32_t gen_pld_w_empty: 1;

		/*
		 * This bit indicates the full status of the generic write
		 * payload FIFO.
		 * Value after reset: 0x0
		 */
		uint32_t gen_pld_w_full: 1;

		/*
		 * This bit indicates the empty status of the generic read
		 * payload FIFO.
		 * Value after reset: 0x1
		 */
		uint32_t gen_pld_r_empty: 1;

		/*
		 * This bit indicates the full status of the generic read
		 * payload FIFO.
		 * Value after reset: 0x0
		 */
		uint32_t gen_pld_r_full: 1;

		/*
		 * This bit is set when a read command is issued and cleared
		 * when the entire response is stored in the FIFO.
		 * Value after reset: 0x0
		 */
		uint32_t gen_rd_cmd_busy: 1;

		uint32_t reserved0: 1;

		uint32_t dbi_cmd_empty: 1;
		uint32_t dbi_cmd_full: 1;
		uint32_t dbi_pld_w_empty: 1;
		uint32_t dbi_pld_w_full: 1;
		uint32_t dbi_pld_r_empty: 1;
		uint32_t dbi_pld_r_full: 1;
		uint32_t dbi_rd_cmd_busy: 1;

		uint32_t reserved1: 1;

		uint32_t gen_buff_cmd_empty: 1;
		uint32_t gen_buff_cmd_full: 1;
		uint32_t gen_buff_pld_empty: 1;
		uint32_t gen_buff_pld_full: 1;

		uint32_t reserved2: 4;

		uint32_t dbi_buff_cmd_empty: 1;
		uint32_t dbi_buff_cmd_full: 1;
		uint32_t dbi_buff_pld_empty: 1;
		uint32_t dbi_buff_pld_full: 1;

		uint32_t reserved3: 4;
		} bits;
	} CMD_PKT_STATUS;

	union _0x78 {
		uint32_t val;
		struct _TO_CNT_CFG {
		/*
		 * This field configures the timeout counter that triggers
		 * a low-power reception timeout contention detection (measured
		 * in TO_CLK_DIVISION cycles).
		 */
		uint32_t lprx_to_cnt: 16;

		/*
		 * This field configures the timeout counter that triggers
		 * a high speed transmission timeout contention detection
		 * (measured in TO_CLK_DIVISION cycles).
		 *
		 * If using the non-burst mode and there is no sufficient
		 * time to switch from HS to LP and back in the period which
		 * is from one line data finishing to the next line sync
		 * start, the DSI link returns the LP state once per frame,
		 * then you should configure the TO_CLK_DIVISION and
		 * hstx_to_cnt to be in accordance with:
		 * hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the
		 * time of one FRAME data transmission * (1 + 10%)
		 *
		 * In burst mode, RGB pixel packets are time-compressed,
		 * leaving more time during a scan line. Therefore, if in
		 * burst mode and there is sufficient time to switch from HS
		 * to LP and back in the period of time from one line data
		 * finishing to the next line sync start, the DSI link can
		 * return LP mode and back in this time interval to save power.
		 * For this, configure the TO_CLK_DIVISION and hstx_to_cnt
		 * to be in accordance with:
		 * hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the
		 * time of one LINE data transmission * (1 + 10%)
		 */
		uint32_t hstx_to_cnt: 16;

		} bits;
	} TO_CNT_CFG;

	union _0x7C {
		uint32_t val;
		struct _HS_RD_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a high-speed read oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		uint32_t hs_rd_to_cnt: 16;

		uint32_t reserved: 16;

		} bits;
	} HS_RD_TO_CNT;

	union _0x80 {
		uint32_t val;
		struct _LP_RD_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a low-power read oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		uint32_t lp_rd_to_cnt: 16;

		uint32_t reserved: 16;

		} bits;
	} LP_RD_TO_CNT;

	union _0x84 {
		uint32_t val;
		struct _HS_WR_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link inactive after sending a high-speed write
		 * operation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		uint32_t hs_wr_to_cnt: 16;

		uint32_t reserved_0: 8;

		/*
		 * When set to 1, this bit ensures that the peripheral response
		 * timeout caused by hs_wr_to_cnt is used only once per eDPI
		 * frame, when both the following conditions are met:
		 * dpivsync_edpiwms has risen and fallen.
		 * Packets originated from eDPI have been transmitted and its
		 * FIFO is empty again In this scenario no non-eDPI requests
		 * are sent to the D-PHY, even if there is traffic from generic
		 * or DBI ready to be sent, making it return to stop state.
		 * When it does so, PRESP_TO counter is activated and only when
		 * it finishes does the controller send any other traffic that
		 * is ready.
		 */
		uint32_t presp_to_mode: 1;

		uint32_t reserved_1: 7;

		} bits;
	} HS_WR_TO_CNT;

	union _0x88 {
		uint32_t val;
		struct _LP_WR_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a low-power write oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		uint32_t lp_wr_to_cnt: 16;

		uint32_t reserved: 16;

		} bits;
	} LP_WR_TO_CNT;

	union _0x8C {
		uint32_t val;
		struct _BTA_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after completing a Bus Turn-Around.
		 * This period is measured in cycles of lanebyteclk. The
		 * counting starts when the D-PHY enters the Stop state and
		 * causes no interrupts.
		 */
		uint32_t bta_to_cnt: 16;

		uint32_t reserved: 16;

		} bits;
	} BTA_TO_CNT;

	union _0x90 {
		uint32_t val;
		struct _SDF_3D {
		/*
		 * This field defines the 3D mode on/off & display orientation:
		 * 00: 3D mode off (2D mode on)
		 * 01: 3D mode on, portrait orientation
		 * 10: 3D mode on, landscape orientation
		 * 11: Reserved
		 */
		uint32_t mode_3d: 2;

		/*
		 * This field defines the 3D image format:
		 * 00: Line (alternating lines of left and right data)
		 * 01: Frame (alternating frames of left and right data)
		 * 10: Pixel (alternating pixels of left and right data)
		 * 11: Reserved
		 */
		uint32_t format_3d: 2;

		/*
		 * This field defines whether there is a second VSYNC pulse
		 * between Left and Right Images, when 3D Image Format is
		 * Frame-based:
		 * 0: No sync pulses between left and right data
		 * 1: Sync pulse (HSYNC, VSYNC, blanking) between left and
		 *    right data
		 */
		uint32_t second_vsync: 1;

		/*
		 * This bit defines the left or right order:
		 * 0: Left eye data is sent first, and then the right eye data
		 *    is sent.
		 * 1: Right eye data is sent first, and then the left eye data
		 *    is sent.
		 */
		uint32_t right_first: 1;

		uint32_t reserved_0: 10;

		/*
		 * When set, causes the next VSS packet to include 3D control
		 * payload in every VSS packet.
		 */
		uint32_t send_3d_cfg: 1;

		uint32_t reserved_1: 15;

		} bits;
	} SDF_3D;

	union _0x94 {
		uint32_t val;
		struct _LPCLK_CTRL {
		/* This bit controls the D-PHY PPI txrequestclkhs signal */
		uint32_t phy_txrequestclkhs: 1;

		/* This bit enables the automatic mechanism to stop providing
		 * clock in the clock lane when time allows.
		 */
		uint32_t auto_clklane_ctrl: 1;

		uint32_t reserved: 30;

		} bits;
	} LPCLK_CTRL;

	union _0x98 {
		uint32_t val;
		struct _PHY_TMR_LPCLK_CFG {
		/*
		 * This field configures the maximum time that the D-PHY
		 * clock lane takes to go from low-power to high-speed
		 * transmission measured in lane byte clock cycles.
		 */
		uint32_t phy_clklp2hs_time: 10;

		uint32_t reserved0: 6;

		/*
		 * This field configures the maximum time that the D-PHY
		 * clock lane takes to go from high-speed to low-power
		 * transmission measured in lane byte clock cycles.
		 */
		uint32_t phy_clkhs2lp_time: 10;

		uint32_t reserved1: 6;

		} bits;
	} PHY_TMR_LPCLK_CFG;

	union _0x9C {
		uint32_t val;
		struct _PHY_TMR_CFG {
		/*
		 * This field configures the maximum time that the D-PHY data
		 * lanes take to go from low-power to high-speed transmission
		 * measured in lane byte clock cycles.
		 */
		uint32_t phy_lp2hs_time: 10;

		uint32_t reserved0: 6;

		/*
		 * This field configures the maximum time that the D-PHY data
		 * lanes take to go from high-speed to low-power transmission
		 * measured in lane byte clock cycles.
		 */
		uint32_t phy_hs2lp_time: 10;

		uint32_t reserved1: 6;

		} bits;
	} PHY_TMR_CFG;

	union _0xA0 {
		uint32_t val;
		struct _PHY_RSTZ {
		/* When set to 0, this bit places the D-PHY macro in power-
		 * down state.
		 */
		uint32_t phy_shutdownz: 1;

		/* When set to 0, this bit places the digital section of the
		 * D-PHY in the reset state.
		 */
		uint32_t phy_rstz: 1;

		/* When set to 1, this bit enables the D-PHY Clock Lane
		 * module.
		 */
		uint32_t phy_enableclk: 1;

		/* When the D-PHY is in ULPS, this bit enables the D-PHY PLL. */
		uint32_t phy_forcepll: 1;

		uint32_t reserved: 28;

		} bits;
	} PHY_RSTZ;

	union _0xA4 {
		uint32_t val;
		struct _PHY_IF_CFG {
		/*
		 * This field configures the number of active data lanes:
		 * 00: One data lane (lane 0)
		 * 01: Two data lanes (lanes 0 and 1)
		 * 10: Three data lanes (lanes 0, 1, and 2)
		 * 11: Four data lanes (lanes 0, 1, 2, and 3)
		 */
		uint32_t n_lanes: 2;

		uint32_t reserved0: 6;

		/* This field configures the minimum wait period to request
		 * a high-speed transmission after the Stop state.
		 */
		uint32_t phy_stop_wait_time: 8;

		uint32_t reserved1: 16;

		} bits;
	} PHY_IF_CFG;

	union _0xA8 {
		uint32_t val;
		struct _PHY_ULPS_CTRL {
		/* ULPS mode Request on clock lane */
		uint32_t phy_txrequlpsclk: 1;

		/* ULPS mode Exit on clock lane */
		uint32_t phy_txexitulpsclk: 1;

		/* ULPS mode Request on all active data lanes */
		uint32_t phy_txrequlpslan: 1;

		/* ULPS mode Exit on all active data lanes */
		uint32_t phy_txexitulpslan: 1;

		uint32_t reserved: 28;
		} bits;
	} PHY_ULPS_CTRL;

	union _0xAC {
		uint32_t val;
		struct _PHY_TX_TRIGGERS {
		/* This field controls the trigger transmissions. */
		uint32_t phy_tx_triggers: 4;

		uint32_t reserved: 28;
		} bits;
	} PHY_TX_TRIGGERS;

	union _0xB0 {
		uint32_t val;
		struct _PHY_STATUS {
		/* the status of phylock D-PHY signal */
		uint32_t phy_lock: 1;

		/* the status of phydirection D-PHY signal */
		uint32_t phy_direction: 1;

		/* the status of phystopstateclklane D-PHY signal */
		uint32_t phy_stopstateclklane: 1;

		/* the status of phyulpsactivenotclk D-PHY signal */
		uint32_t phy_ulpsactivenotclk: 1;

		/* the status of phystopstate0lane D-PHY signal */
		uint32_t phy_stopstate0lane: 1;

		/* the status of ulpsactivenot0lane D-PHY signal */
		uint32_t phy_ulpsactivenot0lane: 1;

		/* the status of rxulpsesc0lane D-PHY signal */
		uint32_t phy_rxulpsesc0lane: 1;

		/* the status of phystopstate1lane D-PHY signal */
		uint32_t phy_stopstate1lane: 1;

		/* the status of ulpsactivenot1lane D-PHY signal */
		uint32_t phy_ulpsactivenot1lane: 1;

		/* the status of phystopstate2lane D-PHY signal */
		uint32_t phy_stopstate2lane: 1;

		/* the status of ulpsactivenot2lane D-PHY signal */
		uint32_t phy_ulpsactivenot2lane: 1;

		/* the status of phystopstate3lane D-PHY signal */
		uint32_t phy_stopstate3lane: 1;

		/* the status of ulpsactivenot3lane D-PHY signal */
		uint32_t phy_ulpsactivenot3lane: 1;

		uint32_t reserved: 19;

		} bits;
	} PHY_STATUS;

	union _0xB4 {
		uint32_t val;
		struct _PHY_TST_CTRL0 {
		/* PHY test interface clear (active high) */
		uint32_t phy_testclr: 1;

		/* This bit is used to clock the TESTDIN bus into the D-PHY */
		uint32_t phy_testclk: 1;

		uint32_t reserved: 30;
		} bits;
	} PHY_TST_CTRL0;

	union _0xB8 {
		uint32_t val;
		struct _PHY_TST_CTRL1 {
		/* PHY test interface input 8-bit data bus for internal
		 * register programming and test functionalities access.
		 */
		uint32_t phy_testdin: 8;

		/* PHY output 8-bit data bus for read-back and internal
		 * probing functionalities.
		 */
		uint32_t phy_testdout: 8;

		/*
		 * PHY test interface operation selector:
		 * 1: The address write operation is set on the falling edge
		 *    of the testclk signal.
		 * 0: The data write operation is set on the rising edge of
		 *    the testclk signal.
		 */
		uint32_t phy_testen: 1;

		uint32_t reserved: 15;
		} bits;
	} PHY_TST_CTRL1;

	union _0xBC {
		uint32_t val;
		struct _INT_STS0 {
		/* SoT error from the Acknowledge error report */
		uint32_t ack_with_err_0: 1;

		/* SoT Sync error from the Acknowledge error report */
		uint32_t ack_with_err_1: 1;

		/* EoT Sync error from the Acknowledge error report */
		uint32_t ack_with_err_2: 1;

		/* Escape Mode Entry Command error from the Acknowledge
		 * error report
		 */
		uint32_t ack_with_err_3: 1;

		/* LP Transmit Sync error from the Acknowledge error report */
		uint32_t ack_with_err_4: 1;

		/* Peripheral Timeout error from the Acknowledge error report */
		uint32_t ack_with_err_5: 1;

		/* False Control error from the Acknowledge error report */
		uint32_t ack_with_err_6: 1;

		/* reserved (specific to device) from the Acknowledge error
		 * report
		 */
		uint32_t ack_with_err_7: 1;

		/* ECC error, single-bit (detected and corrected) from the
		 * Acknowledge error report
		 */
		uint32_t ack_with_err_8: 1;

		/* ECC error, multi-bit (detected, not corrected) from the
		 * Acknowledge error report
		 */
		uint32_t ack_with_err_9: 1;

		/* checksum error (long packet only) from the Acknowledge
		 * error report
		 */
		uint32_t ack_with_err_10: 1;

		/* not recognized DSI data type from the Acknowledge error
		 * report
		 */
		uint32_t ack_with_err_11: 1;

		/* DSI VC ID Invalid from the Acknowledge error report */
		uint32_t ack_with_err_12: 1;

		/* invalid transmission length from the Acknowledge error
		 * report
		 */
		uint32_t ack_with_err_13: 1;

		/* reserved (specific to device) from the Acknowledge error
		 * report
		 */
		uint32_t ack_with_err_14: 1;

		/* DSI protocol violation from the Acknowledge error report */
		uint32_t ack_with_err_15: 1;

		/* ErrEsc escape entry error from Lane 0 */
		uint32_t dphy_errors_0: 1;

		/* ErrSyncEsc low-power data transmission synchronization
		 * error from Lane 0
		 */
		uint32_t dphy_errors_1: 1;

		/* ErrControl error from Lane 0 */
		uint32_t dphy_errors_2: 1;

		/* ErrContentionLP0 LP0 contention error from Lane 0 */
		uint32_t dphy_errors_3: 1;

		/* ErrContentionLP1 LP1 contention error from Lane 0 */
		uint32_t dphy_errors_4: 1;

		uint32_t reserved: 11;

		} bits;
	} INT_STS0;

	union _0xC0 {
		uint32_t val;
		struct _INT_STS1 {
		/* This bit indicates that the high-speed transmission timeout
		 * counter reached the end and contention is detected.
		 */
		uint32_t to_hs_tx: 1;

		/* This bit indicates that the low-power reception timeout
		 * counter reached the end and contention is detected.
		 */
		uint32_t to_lp_rx: 1;

		/* This bit indicates that the ECC single error is detected
		 * and corrected in a received packet.
		 */
		uint32_t ecc_single_err: 1;

		/* This bit indicates that the ECC multiple error is detected
		 * in a received packet.
		 */
		uint32_t ecc_multi_err: 1;

		/* This bit indicates that the CRC error is detected in the
		 * received packet payload.
		 */
		uint32_t crc_err: 1;

		/* This bit indicates that the packet size error is detected
		 * during the packet reception.
		 */
		uint32_t pkt_size_err: 1;

		/* This bit indicates that the EoTp packet is not received at
		 * the end of the incoming peripheral transmission
		 */
		uint32_t eotp_err: 1;

		/* This bit indicates that during a DPI pixel line storage,
		 * the payload FIFO becomes full and the data stored is
		 * corrupted.
		 */
		uint32_t dpi_pld_wr_err: 1;

		/* This bit indicates that the system tried to write a command
		 * through the Generic interface and the FIFO is full. There-
		 * fore, the command is not written.
		 */
		uint32_t gen_cmd_wr_err: 1;

		/* This bit indicates that the system tried to write a payload
		 * data through the Generic interface and the FIFO is full.
		 * Therefore, the payload is not written.
		 */
		uint32_t gen_pld_wr_err: 1;

		/* This bit indicates that during a Generic interface packet
		 * build, the payload FIFO becomes empty and corrupt data is
		 * sent.
		 */
		uint32_t gen_pld_send_err: 1;

		/* This bit indicates that during a DCS read data, the payload
		 * FIFO becomes	empty and the data sent to the interface is
		 * corrupted.
		 */
		uint32_t gen_pld_rd_err: 1;

		/* This bit indicates that during a generic interface packet
		 * read back, the payload FIFO becomes full and the received
		 * data is corrupted.
		 */
		uint32_t gen_pld_recev_err: 1;

		uint32_t dbi_cmd_wr_err: 1;
		uint32_t dbi_pld_wr_err: 1;
		uint32_t dbi_pld_rd_err: 1;
		uint32_t dbi_pld_recv_err: 1;
		uint32_t dbi_ilegal_comm_err: 1;

		uint32_t reserved0: 1;

		uint32_t dpi_buff_pld_under: 1;
		uint32_t tear_request_err: 1;

		uint32_t reserved1: 11;
		} bits;
	} INT_STS1;

	union _0xC4 {
		uint32_t val;
		struct _INT_MASK0 {
		uint32_t mask_ack_with_err_0: 1;
		uint32_t mask_ack_with_err_1: 1;
		uint32_t mask_ack_with_err_2: 1;
		uint32_t mask_ack_with_err_3: 1;
		uint32_t mask_ack_with_err_4: 1;
		uint32_t mask_ack_with_err_5: 1;
		uint32_t mask_ack_with_err_6: 1;
		uint32_t mask_ack_with_err_7: 1;
		uint32_t mask_ack_with_err_8: 1;
		uint32_t mask_ack_with_err_9: 1;
		uint32_t mask_ack_with_err_10: 1;
		uint32_t mask_ack_with_err_11: 1;
		uint32_t mask_ack_with_err_12: 1;
		uint32_t mask_ack_with_err_13: 1;
		uint32_t mask_ack_with_err_14: 1;
		uint32_t mask_ack_with_err_15: 1;
		uint32_t mask_dphy_errors_0: 1;
		uint32_t mask_dphy_errors_1: 1;
		uint32_t mask_dphy_errors_2: 1;
		uint32_t mask_dphy_errors_3: 1;
		uint32_t mask_dphy_errors_4: 1;
		uint32_t reserved: 11;
		} bits;
	} INT_MASK0;

	union _0xC8 {
		uint32_t val;
		struct _INT_MASK1 {
		uint32_t mask_to_hs_tx: 1;
		uint32_t mask_to_lp_rx: 1;
		uint32_t mask_ecc_single_err: 1;
		uint32_t mask_ecc_multi_err: 1;
		uint32_t mask_crc_err: 1;
		uint32_t mask_pkt_size_err: 1;
		uint32_t mask_eotp_err: 1;
		uint32_t mask_dpi_pld_wr_err: 1;
		uint32_t mask_gen_cmd_wr_err: 1;
		uint32_t mask_gen_pld_wr_err: 1;
		uint32_t mask_gen_pld_send_err: 1;
		uint32_t mask_gen_pld_rd_err: 1;
		uint32_t mask_gen_pld_recev_err: 1;
		uint32_t mask_dbi_cmd_wr_err: 1;
		uint32_t mask_dbi_pld_wr_err: 1;
		uint32_t mask_dbi_pld_rd_err: 1;
		uint32_t mask_dbi_pld_recv_err: 1;
		uint32_t mask_dbi_ilegal_comm_err: 1;
		uint32_t reserved0: 1;
		uint32_t mask_dpi_buff_pld_under: 1;
		uint32_t mask_tear_request_err: 1;
		uint32_t reserved1: 11;
		} bits;
	} INT_MASK1;

	union _0xCC {
		uint32_t val;
		struct _PHY_CAL {

		uint32_t txskewcalhs: 1;

		uint32_t reserved: 31;

		} bits;
	} PHY_CAL;

	uint32_t reservedD0_D4[2];

	union _0xD8 {
		uint32_t val;
		struct _INT_FORCE0 {
		uint32_t force_ack_with_err_0: 1;
		uint32_t force_ack_with_err_1: 1;
		uint32_t force_ack_with_err_2: 1;
		uint32_t force_ack_with_err_3: 1;
		uint32_t force_ack_with_err_4: 1;
		uint32_t force_ack_with_err_5: 1;
		uint32_t force_ack_with_err_6: 1;
		uint32_t force_ack_with_err_7: 1;
		uint32_t force_ack_with_err_8: 1;
		uint32_t force_ack_with_err_9: 1;
		uint32_t force_ack_with_err_10: 1;
		uint32_t force_ack_with_err_11: 1;
		uint32_t force_ack_with_err_12: 1;
		uint32_t force_ack_with_err_13: 1;
		uint32_t force_ack_with_err_14: 1;
		uint32_t force_ack_with_err_15: 1;
		uint32_t force_dphy_errors_0: 1;
		uint32_t force_dphy_errors_1: 1;
		uint32_t force_dphy_errors_2: 1;
		uint32_t force_dphy_errors_3: 1;
		uint32_t force_dphy_errors_4: 1;
		uint32_t reserved: 11;
		} bits;
	} INT_FORCE0;

	union _0xDC {
		uint32_t val;
		struct _INT_FORCE1 {
		uint32_t force_to_hs_tx: 1;
		uint32_t force_to_lp_rx: 1;
		uint32_t force_ecc_single_err: 1;
		uint32_t force_ecc_multi_err: 1;
		uint32_t force_crc_err: 1;
		uint32_t force_pkt_size_err: 1;
		uint32_t force_eotp_err: 1;
		uint32_t force_dpi_pld_wr_err: 1;
		uint32_t force_gen_cmd_wr_err: 1;
		uint32_t force_gen_pld_wr_err: 1;
		uint32_t force_gen_pld_send_err: 1;
		uint32_t force_gen_pld_rd_err: 1;
		uint32_t force_gen_pld_recev_err: 1;
		uint32_t force_dbi_cmd_wr_err: 1;
		uint32_t force_dbi_pld_wr_err: 1;
		uint32_t force_dbi_pld_rd_err: 1;
		uint32_t force_dbi_pld_recv_err: 1;
		uint32_t force_dbi_ilegal_comm_err: 1;
		uint32_t reserved0: 1;
		uint32_t force_dpi_buff_pld_under: 1;
		uint32_t force_tear_request_err: 1;
		uint32_t reserved1: 11;
		} bits;
	} INT_FORCE1;

	union _0xE0 {
		uint32_t val;
		struct _AUTO_ULPS_MODE {
		uint32_t auto_ulps: 1;
		uint32_t reserved0: 15;
		uint32_t pll_off_ulps: 1;
		uint32_t pre_pll_off_req: 1;
		uint32_t reserved1: 14;
		}bits;
	} AUTO_ULPS_MODE;

	union _0xE4 {
		uint32_t val;
		struct _AUTO_ULPS_ENTRY_DELAY {
		uint32_t val;
		} bits;
	} AUTO_ULPS_ENTRY_DELAY;

	union _0xE8 {
		uint32_t val;
		struct _AUTO_ULPS_WAKEUP_TIME {
		uint32_t twakeup_clk_div: 16;
		uint32_t twakeup_cnt: 16;
		} bits;
	} AUTO_ULPS_WAKEUP_TIME;

	uint32_t reservedEC;

	union _0xF0 {
		uint32_t val;
		struct _DSC_PARAMETER {
		/* When set to 1, this bit enables the compression mode. */
		uint32_t compression_mode: 1;

		uint32_t reserved0: 7;

		/* This field indicates the algorithm identifier:
		 * 00 = VESA DSC Standard 1.1
		 * 11 = vendor-specific algorithm
		 * 01 and 10 = reserved, not used
		 */
		uint32_t compress_algo: 2;

		uint32_t reserved1: 6;

		/* This field indicates the PPS selector:
		 * 00 = PPS Table 1
		 * 01 = PPS Table 2
		 * 10 = PPS Table 3
		 * 11 = PPS Table 4
		 */
		uint32_t pps_sel: 2;

		uint32_t reserved2: 14;

		} bits;
	} DSC_PARAMETER;

	union _0xF4 {
		uint32_t val;
		struct _PHY_TMR_RD_CFG {
		/*
		 * This field configures the maximum time required to perform
		 * a read command in lane byte clock cycles. This register can
		 * only be modified when no read command is in progress.
		 */
		uint32_t max_rd_time: 15;

		uint32_t reserved: 17;

		} bits;
	} PHY_TMR_RD_CFG;

	union _0xF8 {
		uint32_t val;
		struct _AUTO_ULPS_MIN_TIME {
		uint32_t ulps_min_time: 12;
		uint32_t reserved: 20;
		} bits;
	} AUTO_ULPS_MIN_TIME;

	union _0xFC {
		uint32_t val;
		struct _PHY_MODE {
		uint32_t phy_mode: 1;
		uint32_t reserved: 31;
		} bits;
	} PHY_MODE;

	union _0x100 {
		uint32_t val;
		struct _VID_SHADOW_CTRL {
		/*
		 * When set to 1, DPI receives the active configuration
		 * from the auxiliary registers. When the feature is set
		 * at the same time than vid_shadow_req the auxiliary
		 * registers are automatically updated.
		 */
		uint32_t vid_shadow_en: 1;

		uint32_t reserved0: 7;

		/*
		 * When set to 1, this bit request that the dpi registers
		 * from regbank are copied to the auxiliary registers. When
		 * the request is completed this bit is a auto clear.
		 */
		uint32_t vid_shadow_req: 1;

		uint32_t reserved1: 7;

		/*
		 * When set to 1, the video request is done by external pin.
		 * In this mode vid_shadow_req is ignored.
		 */
		uint32_t vid_shadow_pin_req: 1;

		uint32_t reserved2: 15;

		} bits;
	} VID_SHADOW_CTRL;

	uint32_t reserved104_108[2];

	union _0x10C {
		uint32_t val;
		struct _DPI_VCID_ACT {
		/* This field configures the DPI virtual channel id that
		 * is indexed to the VIDEO mode packets
		 */
		uint32_t dpi_vcid: 2;

		uint32_t reserved: 30;

		} bits;
	} DPI_VCID_ACT;

	union _0x110 {
		uint32_t val;
		struct _DPI_COLOR_CODING_ACT {
		/*
		 * This field configures the DPI color coding as follows:
		 * 0000: 16-bit configuration 1
		 * 0001: 16-bit configuration 2
		 * 0010: 16-bit configuration 3
		 * 0011: 18-bit configuration 1
		 * 0100: 18-bit configuration 2
		 * 0101: 24-bit
		 * 0110: 20-bit YCbCr 4:2:2 loosely packed
		 * 0111: 24-bit YCbCr 4:2:2
		 * 1000: 16-bit YCbCr 4:2:2
		 * 1001: 30-bit
		 * 1010: 36-bit
		 * 1011: 12-bit YCbCr 4:2:0
		 * 1100: Compression Display Stream
		 * 1101-1111: 12-bit YCbCr 4:2:0
		 */
		uint32_t dpi_color_coding: 4;

		uint32_t reserved0: 4;

		/* When set to 1, this bit activates loosely packed
		 * variant to 18-bit configurations
		 */
		uint32_t loosely18_en: 1;

		uint32_t reserved1: 23;

		} bits;
	} DPI_COLOR_CODING_ACT;

	uint32_t reserved144;

	union _0x118 {
		uint32_t val;
		struct _DPI_LP_CMD_TIM_ACT {
		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VACT
		 * region.
		 */
		uint32_t invact_lpcmd_time: 8;

		uint32_t reserved0: 8;

		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VSA, VBP,
		 * and VFP regions.
		 */
		uint32_t outvact_lpcmd_time: 8;

		uint32_t reserved1: 8;

		} bits;
	} DPI_LP_CMD_TIM_ACT;

	union _0x11C {
		uint32_t val;
		struct _EDPI_TE_HW_CFG {
		uint32_t hw_tear_effect_on: 1;
		uint32_t hw_tear_effect_gen: 1;
		uint32_t reserved0: 2;
		uint32_t hw_set_scan_line: 1;
		uint32_t reserved1: 11;
		uint32_t scan_line_parameter: 16;
		} bits;
	} EDPI_TE_HW_CFG;

	uint32_t reserved120_134[6];

	union _0x138 {
		uint32_t val;
		struct _VID_MODE_CFG_ACT {
		/*
		 * This field indicates the video mode transmission type as
		 * follows:
		 * 00: Non-burst with sync pulses
		 * 01: Non-burst with sync events
		 * 10 and 11: Burst mode
		 */
		uint32_t vid_mode_type: 2;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VSA period when timing allows.
		 */
		uint32_t lp_vsa_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VBP period when timing allows.
		 */
		uint32_t lp_vbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VFP period when timing allows.
		 */
		uint32_t lp_vfp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VACT period when timing allows.
		 */
		uint32_t lp_vact_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HBP period when timing allows.
		 */
		uint32_t lp_hbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HFP period when timing allows.
		 */
		uint32_t lp_hfp_en: 1;

		/* When set to 1, this bit enables the request for an ack-
		 * nowledge response at the end of a frame.
		 */
		uint32_t frame_bta_ack_en: 1;

		/* When set to 1, this bit enables the command transmission
		 * only in low-power mode.
		 */
		uint32_t lp_cmd_en: 1;

		uint32_t reserved: 22;
		} bits;
	} VID_MODE_CFG_ACT;

	union _0x13C {
		uint32_t val;
		struct _VID_PKT_SIZE_ACT {
		/*
		 * This field configures the number of pixels in a single
		 * video packet. For 18-bit not loosely packed data types,
		 * this number must be a multiple of 4. For YCbCr data
		 * types, it must be a multiple of 2, as described in the
		 * DSI specification.
		 */
		uint32_t vid_pkt_size: 14;

		uint32_t reserved: 18;

		} bits;
	} VID_PKT_SIZE_ACT;

	union _0x140 {
		uint32_t val;
		struct _VID_NUM_CHUNKS_ACT {
		/*
		 * This register configures the number of chunks to be
		 * transmitted during a Line period (a chunk consists of
		 * a video packet and a null packet). If set to 0 or 1,
		 * the video line is transmitted in a single packet. If
		 * set to 1, the packet is part of a chunk, so a null packet
		 * follows it if vid_null_size > 0. Otherwise, multiple chunks
		 * are used to transmit each video line.
		 */
		uint32_t vid_num_chunks: 13;

		uint32_t reserved: 19;

		} bits;
	} VID_NUM_CHUNKS_ACT;

	union _0x144 {
		uint32_t val;
		struct _VID_NULL_SIZE_ACT {
		/*
		 * This register configures the number of bytes inside a null
		 * packet. Setting it to 0 disables the null packets.
		 */
		uint32_t vid_null_size: 13;

		uint32_t reserved: 19;

		} bits;
	} VID_NULL_SIZE_ACT;

	union _0x148 {
		uint32_t val;
		struct _VID_HSA_TIME_ACT {
		/* This field configures the Horizontal Synchronism Active
		 * period in lane byte clock cycles
		 */
		uint32_t vid_hsa_time: 12;

		uint32_t reserved: 20;

		} bits;
	} VID_HSA_TIME_ACT;

	union _0x14C {
		uint32_t val;
		struct _VID_HBP_TIME_ACT {
		/* This field configures the Horizontal Back Porch period
		 * in lane byte clock cycles
		 */
		uint32_t vid_hbp_time: 12;

		uint32_t reserved: 20;

		} bits;
	} VID_HBP_TIME_ACT;

	union _0x150 {
		uint32_t val;
		struct _VID_HLINE_TIME_ACT {
		/* This field configures the size of the total line time
		 * (HSA+HBP+HACT+HFP) counted in lane byte clock cycles
		 */
		uint32_t vid_hline_time: 15;

		uint32_t reserved: 17;

		} bits;
	} VID_HLINE_TIME_ACT;

	union _0x154 {
		uint32_t val;
		struct _VID_VSA_LINES_ACT {
		/* This field configures the Vertical Synchronism Active
		 * period measured in number of horizontal lines
		 */
		uint32_t vsa_lines: 10;

		uint32_t reserved: 22;

		} bits;
	} VID_VSA_LINES_ACT;

	union _0x158 {
		uint32_t val;
		struct _VID_VBP_LINES_ACT {
		/* This field configures the Vertical Back Porch period
		 * measured in number of horizontal lines
		 */
		uint32_t vbp_lines: 10;

		uint32_t reserved: 22;

		} bits;
	} VID_VBP_LINES_ACT;

	union _0x15C {
		uint32_t val;
		struct _VID_VFP_LINES_ACT {
		/* This field configures the Vertical Front Porch period
		 * measured in number of horizontal lines
		 */
		uint32_t vfp_lines: 10;

		uint32_t reserved: 22;

		} bits;
	} VID_VFP_LINES_ACT;

	union _0x160 {
		uint32_t val;
		struct _VID_VACTIVE_LINES_ACT {
		/* This field configures the Vertical Active period measured
		 * in number of horizontal lines
		 */
		uint32_t v_active_lines: 14;

		uint32_t reserved: 18;

		} bits;
	} VID_VACTIVE_LINES_ACT;

	uint32_t reserved164;

	union _0x168 {
		uint32_t val;
		struct _VID_PKT_STATUS {
		uint32_t dpi_cmd_w_empty: 1;
		uint32_t dpi_cmd_w_full: 1;
		uint32_t dpi_pld_w_empty: 1;
		uint32_t dpi_pld_w_full: 1;
		uint32_t edpi_cmd_w_empty: 1;
		uint32_t edpi_cmd_w_full: 1;
		uint32_t edpi_pld_w_empty: 1;
		uint32_t edpi_pld_w_full: 1;
		uint32_t reserved0: 8;
		uint32_t dpi_buff_pld_empty: 1;
		uint32_t dpi_buff_pld__full: 1;
		uint32_t reserved1: 2;
		uint32_t edpi_buff_cmd_empty: 1;
		uint32_t edpi_buff_cmd_full: 1;
		uint32_t edpi_buff_pld_empty: 1;
		uint32_t edpi_buff_pld_full: 1;
		uint32_t reserved2: 8;
		} bits;
	} VID_PKT_STATUS;

	uint32_t reserved16C_18C[9];

	union _0x190 {
		uint32_t val;
		struct _SDF_3D_ACT {
		/*
		 * This field defines the 3D mode on/off & display orientation:
		 * 00: 3D mode off (2D mode on)
		 * 01: 3D mode on, portrait orientation
		 * 10: 3D mode on, landscape orientation
		 * 11: Reserved
		 */
		uint32_t mode_3d: 2;

		/*
		 * This field defines the 3D image format:
		 * 00: Line (alternating lines of left and right data)
		 * 01: Frame (alternating frames of left and right data)
		 * 10: Pixel (alternating pixels of left and right data)
		 * 11: Reserved
		 */
		uint32_t format_3d: 2;

		/*
		 * This field defines whether there is a second VSYNC pulse
		 * between Left and Right Images, when 3D Image Format is
		 * Frame-based:
		 * 0: No sync pulses between left and right data
		 * 1: Sync pulse (HSYNC, VSYNC, blanking) between left and
		 *    right data
		 */
		uint32_t second_vsync: 1;

		/*
		 * This bit defines the left or right order:
		 * 0: Left eye data is sent first, and then the right eye data
		 *    is sent.
		 * 1: Right eye data is sent first, and then the left eye data
		 *    is sent.
		 */
		uint32_t right_first: 1;

		uint32_t reserved_0: 10;

		/*
		 * When set, causes the next VSS packet to include 3D control
		 * payload in every VSS packet.
		 */
		uint32_t send_3d_cfg: 1;

		uint32_t reserved_1: 15;

		} bits;
	} SDF_3D_ACT;

};

#define dsi_read(c)	readl((void __force __iomem *)(c))
#define dsi_write(v, c)	writel(v, (void __force __iomem *)(c))

#define DEFAULT_BYTE_CLOCK    (432000)
#define DSIH_PIXEL_TOLERANCE  (2)
#define VIDEO_PACKET_OVERHEAD  6 /* HEADER (4 bytes) + CRC (2 bytes) */
#define NULL_PACKET_OVERHEAD   6  /* HEADER (4 bytes) + CRC (2 bytes) */

#define PRECISION_FACTOR      (1000)
#define MAX_NULL_SIZE         (1023)

enum dsi_work_mode {
	DSI_MODE_CMD = 0,
	DSI_MODE_VIDEO
};

enum video_burst_mode {
	VIDEO_NON_BURST_WITH_SYNC_PULSES = 0,
	VIDEO_NON_BURST_WITH_SYNC_EVENTS,
	VIDEO_BURST_WITH_SYNC_PULSES
};

typedef enum {
    COLOR_CODE_16BIT_CONFIG1 = 0,
    COLOR_CODE_16BIT_CONFIG2 = 1,
    COLOR_CODE_16BIT_CONFIG3 = 2,
    COLOR_CODE_18BIT_CONFIG1 = 3,
    COLOR_CODE_18BIT_CONFIG2 = 4,
    COLOR_CODE_24BIT = 5,
    COLOR_CODE_20BIT_YCC422_LOOSELY = 6,
    COLOR_CODE_24BIT_YCC422 = 7,
    COLOR_CODE_16BIT_YCC422 = 8,
    COLOR_CODE_30BIT = 9,
    COLOR_CODE_36BIT = 10,
    COLOR_CODE_12BIT_YCC420 = 11,
    COLOR_CODE_DSC24 = 12,
    COLOR_CODE_MAX
} dsih_color_coding_t;

struct dsih_dpi_video_t {
	/** Virtual channel number to send this video stream */
	uint8_t virtual_channel;
	/** Number of lanes used to send current video */
	uint8_t n_lanes;
	/** Video mode, whether burst with sync pulses, or packets with either sync pulses or events */
	uint8_t burst_mode;
	/** Maximum number of byte clock cycles needed by the PHY to transition
	 * the data lanes from high speed to low power - REQUIRED */
	uint16_t data_hs2lp;
	/** Maximum number of byte clock cycles needed by the PHY to transition
	 * the data lanes from low power to high speed - REQUIRED */
	uint16_t data_lp2hs;
	/** Maximum number of byte clock cycles needed by the PHY to transition
	 * the clock lane from high speed to low power - REQUIRED */
	uint16_t clk_hs2lp;
	/** Maximum number of byte clock cycles needed by the PHY to transition
	 * the clock lane from low power to high speed - REQUIRED */
	uint16_t clk_lp2hs;
	/** Enable non coninuous clock for energy saving
	 * - Clock lane will go to LS while not transmitting video */
	int nc_clk_en;
	/** Enable receiving of ack packets */
	int frame_ack_en;
	/** Byte (lane) clock [KHz] */
	unsigned long byte_clock;
	/** Pixel (DPI) Clock [KHz]*/
	unsigned long pixel_clock;
    /**Dphy output frequency*/
    uint32_t phy_freq;
	/** Colour coding - BPP and Pixel configuration */
	dsih_color_coding_t color_coding;
	/** Is 18-bit loosely packets (valid only when BPP == 18) */
	int is_18_loosely;
	/** Data enable signal (dpidaten) whether it is active high or low */
	int data_en_polarity;
	/** Horizontal synchronisation signal (dpihsync) whether it is active high or low */
	int h_polarity;
	/** Horizontal resolution or Active Pixels */
	uint16_t h_active_pixels;	/* hadr */
	/** Horizontal Sync Pixels - min 4 for best performance */
	uint16_t h_sync_pixels;
	/** Horizontal back porch pixels */
	uint16_t h_back_porch_pixels;	/* hbp */
	/** Total Horizontal pixels */
	uint16_t h_total_pixels;	/* h_total */
	/** Vertical synchronisation signal (dpivsync) whether it is active high or low */
	int v_polarity;
	/** Vertical active lines (resolution) */
	uint16_t v_active_lines;	/* vadr */
	/** Vertical sync lines */
	uint16_t v_sync_lines;
	/** Vertical back porch lines */
	uint16_t v_back_porch_lines;	/* vbp */
	/** Vertical front porch lines */
	uint16_t v_front_porch_lines;	/* vbp */
	/** Total no of vertical lines */
	uint16_t v_total_lines;	/* v_total */
	/** When set to 1, this bit enables the EoTp reception */
	int eotp_rx_en;
	/** When set to 1, this bit enables the EoTp transmission */
	int eotp_tx_en;
	/** This register configures the number of chunks to use */
	int no_of_chunks;
	/** This register configures the size of null packets */
	uint16_t null_packet_size;
	/** */
	int dpi_lp_cmd_en;
	/** Diplay type*/
	int display_type;

	uint16_t hline;

};

struct dsi_context {
	void __iomem *base;
	int irq;
	uint32_t int0_mask;
	uint32_t int1_mask;
	uint8_t id;
	struct videomode vm;

	bool is_inited;

	uint8_t work_mode;

	uint8_t max_lanes;
	uint8_t lane_number;
	uint16_t burst_mode;
	uint32_t phy_freq;
	uint16_t max_bta_cycles;
	struct dsih_dpi_video_t dpi_video;

	struct dsi_core_ops *ops;
};

struct dsi_core_ops {
	bool (*check_version)(struct dsi_context *ctx);
	void (*power_enable)(struct dsi_context *ctx, int enable);
	uint8_t (*get_power_status)(struct dsi_context *ctx);
	void (*timeout_clock_division)(struct dsi_context *ctx, uint8_t div);
	void (*tx_escape_division)(struct dsi_context *ctx, uint8_t division);
	void (*video_vcid)(struct dsi_context *ctx, uint8_t vc);
	uint8_t (*get_video_vcid)(struct dsi_context *ctx);
	void (*dpi_color_coding)(struct dsi_context *ctx, int coding);
	uint8_t (*dpi_get_color_coding)(struct dsi_context *ctx);
	uint8_t (*dpi_get_color_depth)(struct dsi_context *ctx);
	uint8_t (*dpi_get_color_config)(struct dsi_context *ctx);
	void (*dpi_18_loosely_packet_en)(struct dsi_context *ctx, int enable);
	void (*dpi_color_mode_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_shut_down_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_hsync_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_vsync_pol)(struct dsi_context *ctx, int active_low);
	void (*dpi_data_en_pol)(struct dsi_context *ctx, int active_low);
	void (*eotp_rx_en)(struct dsi_context *ctx, int enable);
	void (*eotp_tx_en)(struct dsi_context *ctx, int enable);
	void (*bta_en)(struct dsi_context *ctx, int enable);
	void (*ecc_rx_en)(struct dsi_context *ctx, int enable);
	void (*crc_rx_en)(struct dsi_context *ctx, int enable);
	void (*eotp_tx_lp_en)(struct dsi_context *ctx, int enable);
	void (*rx_vcid)(struct dsi_context *ctx, uint8_t vc);
	void (*video_mode)(struct dsi_context *ctx);
	void (*cmd_mode)(struct dsi_context *ctx);
	bool (*is_cmd_mode)(struct dsi_context *ctx);
	void (*video_mode_lp_cmd_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_frame_ack_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_lp_hfp_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_lp_hbp_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_lp_vact_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_lp_vfp_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_lp_vbp_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_lp_vsa_en)(struct dsi_context *ctx, int enable);
	void (*dpi_hporch_lp_en)(struct dsi_context *ctx, int enable);
	void (*dpi_vporch_lp_en)(struct dsi_context *ctx, int enable);
	void (*video_mode_mode_type)(struct dsi_context *ctx, int mode);
	void (*vpg_orientation_act)(struct dsi_context *ctx,
		uint8_t orientation);
	void (*vpg_mode_act)(struct dsi_context *ctx, uint8_t mode);
	void (*enable_vpg_act)(struct dsi_context *ctx, uint8_t enable);
	void (*dpi_video_packet_size)(struct dsi_context *ctx, uint16_t size);
	void (*dpi_chunk_num)(struct dsi_context *ctx, uint16_t num);
	void (*dpi_null_packet_size)(struct dsi_context *ctx, uint16_t size);
	void (*dpi_hline_time)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*dpi_hbp_time)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*dpi_hsync_time)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*dpi_vsync)(struct dsi_context *ctx, uint16_t lines);
	void (*dpi_vbp)(struct dsi_context *ctx, uint16_t lines);
	void (*dpi_vfp)(struct dsi_context *ctx, uint16_t lines);
	void (*dpi_vact)(struct dsi_context *ctx, uint16_t lines);
	void (*tear_effect_ack_en)(struct dsi_context *ctx, int enable);
	void (*cmd_ack_request_en)(struct dsi_context *ctx, int enable);
	void (*cmd_mode_lp_cmd_en)(struct dsi_context *ctx, int enable);
	void (*set_packet_header)(struct dsi_context *ctx,
		uint8_t vc, uint8_t type, uint8_t wc_lsb, uint8_t wc_msb);
	void (*set_packet_payload)(struct dsi_context *ctx, uint32_t payload);
	uint32_t (*get_rx_payload)(struct dsi_context *ctx);
	bool (*is_bta_returned)(struct dsi_context *ctx);
	bool (*is_rx_payload_fifo_full)(struct dsi_context *ctx);
	bool (*is_rx_payload_fifo_empty)(struct dsi_context *ctx);
	bool (*is_tx_payload_fifo_full)(struct dsi_context *ctx);
	bool (*is_tx_payload_fifo_empty)(struct dsi_context *ctx);
	bool (*is_tx_cmd_fifo_full)(struct dsi_context *ctx);
	bool (*is_tx_cmd_fifo_empty)(struct dsi_context *ctx);
	void (*lp_rx_timeout)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*hs_tx_timeout)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*hs_read_presp_timeout)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*lp_read_presp_timeout)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*hs_write_presp_timeout)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*lp_write_presp_timeout)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*bta_presp_timeout)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*nc_clk_en)(struct dsi_context *ctx, int enable);
	uint8_t (*nc_clk_status)(struct dsi_context *ctx);
	uint32_t (*int0_status)(struct dsi_context *ctx);
	uint32_t (*int1_status)(struct dsi_context *ctx);
	void (*int0_mask)(struct dsi_context *ctx, uint32_t mask);
	uint32_t (*int_get_mask_0)(struct dsi_context *ctx, uint32_t mask);
	void (*int1_mask)(struct dsi_context *ctx, uint32_t mask);
	uint32_t (*int_get_mask_1)(struct dsi_context *ctx, uint32_t mask);
	void (*force_int_0)(struct dsi_context *ctx, uint32_t force);
	void (*force_int_1)(struct dsi_context *ctx, uint32_t force);
	void (*max_read_time)(struct dsi_context *ctx, uint16_t byte_cycle);
	void (*activate_shadow_registers)(struct dsi_context *ctx,
		uint8_t activate);
	uint8_t (*read_state_shadow_registers)(struct dsi_context *ctx);
	void (*request_registers_change)(struct dsi_context *ctx);
	void (*external_pin_registers_change)(struct dsi_context *ctx,
		uint8_t external);
	uint8_t (*get_dpi_video_vc_act)(struct dsi_context *ctx);
	uint8_t (*get_loosely18_en_act)(struct dsi_context *ctx);
	uint8_t (*get_dpi_color_coding_act)(struct dsi_context *ctx);
	uint8_t (*get_lp_cmd_en_act)(struct dsi_context *ctx);
	uint8_t (*get_frame_bta_ack_en_act)(struct dsi_context *ctx);
	uint8_t (*get_lp_hfp_en_act)(struct dsi_context *ctx);
	uint8_t (*get_lp_hbp_en_act)(struct dsi_context *ctx);
	uint8_t (*get_lp_vact_en_act)(struct dsi_context *ctx);
	uint8_t (*get_lp_vfp_en_act)(struct dsi_context *ctx);
	uint8_t (*get_lp_vbp_en_act)(struct dsi_context *ctx);
	uint8_t (*get_lp_vsa_en_act)(struct dsi_context *ctx);
	uint8_t (*get_vid_mode_type_act)(struct dsi_context *ctx);
	uint16_t (*get_vid_pkt_size_act)(struct dsi_context *ctx);
	uint16_t (*get_vid_num_chunks_act)(struct dsi_context *ctx);
	uint16_t (*get_vid_null_size_act)(struct dsi_context *ctx);
	uint16_t (*get_vid_hsa_time_act)(struct dsi_context *ctx);
	uint16_t (*get_vid_hbp_time_act)(struct dsi_context *ctx);
	uint16_t (*get_vid_hline_time_act)(struct dsi_context *ctx);
	uint16_t (*get_vsa_lines_act)(struct dsi_context *ctx);
	uint16_t (*get_vbp_lines_act)(struct dsi_context *ctx);
	uint16_t (*get_vfp_lines_act)(struct dsi_context *ctx);
	uint16_t (*get_v_active_lines_act)(struct dsi_context *ctx);
	uint8_t (*get_send_3d_cfg_act)(struct dsi_context *ctx);
	uint8_t (*get_right_left_act)(struct dsi_context *ctx);
	uint8_t (*get_second_vsync_act)(struct dsi_context *ctx);
	uint8_t (*get_format_3d_act)(struct dsi_context *ctx);
	uint8_t (*get_mode_3d_act)(struct dsi_context *ctx);

	void (*dphy_clklane_hs2lp_config)(struct dsi_context *ctx,
		uint16_t byte_cycle);
	void (*dphy_clklane_lp2hs_config)(struct dsi_context *ctx,
		uint16_t byte_cycle);
	void (*dphy_datalane_hs2lp_config)(struct dsi_context *ctx,
		uint16_t byte_cycle);
	void (*dphy_datalane_lp2hs_config)(struct dsi_context *ctx,
		uint16_t byte_cycle);
	void (*dphy_enableclk)(struct dsi_context *ctx, int enable);
	void (*dphy_reset)(struct dsi_context *ctx, int reset);
	void (*dphy_shutdown)(struct dsi_context *ctx, int powerup);
	void (*dphy_force_pll)(struct dsi_context *ctx, int force);
	uint8_t (*dphy_get_force_pll)(struct dsi_context *ctx);
	void (*dphy_stop_wait_time)(struct dsi_context *ctx,
		uint8_t no_of_byte_cycles);
	void (*dphy_n_lanes)(struct dsi_context *ctx, uint8_t n_lanes);
	uint8_t (*dphy_get_n_lanes)(struct dsi_context *ctx);
	void (*dphy_enable_hs_clk)(struct dsi_context *ctx, int enable);
	uint32_t (*dphy_get_status)(struct dsi_context *ctx, uint16_t mask);
	void (*dphy_test_clear)(struct dsi_context *ctx, int value);
	void (*dphy_write)(struct dsi_context *ctx, uint16_t address,
		uint8_t *data, uint8_t data_length);
};

#endif //_DWC_MIPI_DSI_HOST_H_

