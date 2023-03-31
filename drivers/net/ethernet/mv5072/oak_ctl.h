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
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ctl
 * Author: afischer
 * Date  :2020-05-11  - 10:11
 *
 *  */
#ifndef H_OAK_CTL
#define H_OAK_CTL

#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */
#include "oak_net.h" /* Include for relation to classifier oak_net */

extern int debug;
/* Name      : channel_status_access
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_channel_status_access(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : lgen_rx_pkt
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  int desc_num = desc_num,  struct sk_buff ** target = target
 *  */
int oak_ctl_lgen_rx_pkt(oak_t* np, uint32_t ring, int desc_num, struct sk_buff** target);

/* Name      : lgen_xmit_frame
 * Returns   : void
 * Parameters:  oak_t * np = np,  oak_tx_chan_t * txc = txc,  uint32_t idx = idx,  uint32_t num = num
 *  */
void oak_ctl_lgen_xmit_frame(oak_t* np, oak_tx_chan_t* txc, uint32_t idx, uint32_t num);

/* Name      : process_tx_pkt_lgen
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  int desc_num = desc_num
 *  */
int oak_ctl_process_tx_pkt_lgen(oak_t* np, uint32_t ring, int desc_num);

/* Name      : set_mac_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd
 *  */
int oak_ctl_set_mac_rate(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : set_rx_flow
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_rx_flow(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : set_txr_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_txr_rate(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : direct_register_access
 * Returns   : int
 * Parameters:  oak_t * np,  struct ifreq * ifr,  int cmd
 *  */
int oak_ctl_direct_register_access(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : load_generator_access
 * Returns   : int
 * Parameters:  oak_t * np,  struct ifreq * ifr,  int cmd
 *  */
int oak_ctl_load_generator_access(oak_t* np, struct ifreq* ifr, int cmd);

#endif /* #ifndef H_OAK_CTL */

