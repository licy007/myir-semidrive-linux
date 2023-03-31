/*
 *        LICENSE:
 *        (C)Copyright 2010-2011 Marvell.
 *
 *        All Rights Reserved
 *
 *        THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF MARVELL
 *        The copyright notice above does not evidence any
 *        actual or intended publication of such source code.
 *
 *        This Module contains Proprietary Information of Marvell
 *        and should be treated as Confidential.
 *
 *        The information in this file is provided for the exclusive use of
 *        the licensees of Marvell. Such users have the right to use, modify,
 *        and incorporate this code into products for purposes authorized by
 *        the license agreement provided they include this notice and the
 *        associated copyright notice with any such product.
 *
 *        The information in this file is provided "AS IS" without warranty.
 *
 * Contents: collection of functions that provide user IO-control requests
 *
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_channel_stat
 * Author: afischer
 * Date  :2019-05-16  - 11:40
 *  */
#ifndef H_OAK_CHANNEL_STAT
#define H_OAK_CHANNEL_STAT

typedef struct oak_chan_infoStruct
{
    uint32_t flags;
    uint32_t r_count;
    uint32_t r_size;
    uint32_t r_pend;
    uint32_t r_widx;
    uint32_t r_ridx;
    uint32_t r_len;
} oak_chan_info;

typedef struct oak_driver_rx_statStruct
{
    uint64_t channel;
    uint64_t rx_alloc_pages;
    uint64_t rx_unmap_pages;
    uint64_t rx_alloc_error;
    uint64_t rx_frame_error;
    uint64_t rx_errors;
    uint64_t rx_interrupts;
    uint64_t rx_goodframe;
    uint64_t rx_byte_count;
    uint64_t rx_vlan;
    uint64_t rx_badframe;
    uint64_t rx_no_sof;
    uint64_t rx_no_eof;
    uint64_t rx_badcrc;
    uint64_t rx_badcsum;
    uint64_t rx_l4p_ok;
    uint64_t rx_ip4_ok;
    uint64_t rx_nores;
    uint64_t rx_64;
    uint64_t rx_128;
    uint64_t rx_256;
    uint64_t rx_512;
    uint64_t rx_1024;
    uint64_t rx_2048;
    uint64_t rx_fragments;
} oak_driver_rx_stat;

typedef struct oak_driver_tx_statStruct
{
    uint64_t channel;
    uint64_t tx_frame_count;
    uint64_t tx_frame_compl;
    uint64_t tx_byte_count;
    uint64_t tx_fragm_count;
    uint64_t tx_drop;
    uint64_t tx_errors;
    uint64_t tx_interrupts;
    uint64_t tx_stall_count;
    uint64_t tx_64;
    uint64_t tx_128;
    uint64_t tx_256;
    uint64_t tx_512;
    uint64_t tx_1024;
    uint64_t tx_2048;
} oak_driver_tx_stat;

#endif /* #ifndef H_OAK_CHANNEL_STAT */

