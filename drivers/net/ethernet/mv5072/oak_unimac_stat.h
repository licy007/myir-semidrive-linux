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
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_unimac_stat
 * Author: afischer
 * Date  :2019-05-16  - 11:40
 *  */
#ifndef H_OAK_UNIMAC_STAT
#define H_OAK_UNIMAC_STAT

typedef struct oak_unimac_statStruct
{
    uint64_t rx_good_frames;
    uint64_t rx_bad_frames;
    uint64_t rx_stall_fifo;
    uint64_t rx_stall_desc;
    uint64_t rx_discard_desc;
    uint64_t tx_pause;
    uint64_t tx_stall_fifo;
} oak_unimac_stat;

#endif /* #ifndef H_OAK_UNIMAC_STAT */

