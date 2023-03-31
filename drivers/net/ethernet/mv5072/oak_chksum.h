/*
 *        LICENSE:
 *        (C)Copyright 2021-2022 Marvell.
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
 * Contents: Checksum Offload interface functions as called from Linux kernel
 *
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_chksum
 * Author: Arijit De
 * Date  : 2021-06-30  - 23:29
 *  */
#ifndef H_OAK_CHKSUM
#define H_OAK_CHKSUM

/* Checksum configurations supported by the Oak HW */
#define OAK_CHKSUM_TYPE (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | NETIF_F_RXCSUM)

#define L3_L4_CHKSUM 2
#define L3_CHKSUM    1
#define NO_CHKSUM    0

#define OAK_TCP_IP_FRAME  1
#define OAK_TCP_UDP_FRAME 2

/* Name        : oak_chksum_get_config
 * Returns     : netdev_features_t
 * Parameters  : void
 * Description : This function provides Oak Hardware's Checksum Offload
 *               capabilities.
 *  */
netdev_features_t oak_chksum_get_config(void);

/* Name        : oak_chksum_get_tx_config
 * Returns     : uint32_t
 * Parameters  : struct sk_buff *skb, uint32_t *cs_l3, uint32_t *cs_l4
 * Description : This function returns the Checksum Offload configuration for
 *               the transmit frame.
 *  */
uint32_t oak_chksum_get_tx_config(struct sk_buff *skb, uint32_t *cs_l3,
                                  uint32_t *cs_l4);

/* Name        : oak_chksum_get_rx_config
 * Returns     : uint32_t
 * Parameters  : oak_rx_chan_t*, oak_rxs_t*
 * Description : This function returns the current receive frames
 *               checksum state.
 *  */
uint32_t oak_chksum_get_rx_config(oak_rx_chan_t *rxc, oak_rxs_t *rsr);

#endif /* #ifndef H_OAK_CHKSUM */
