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
 * Contents:  functions that are executed from ethtool requests
 *
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ethtool
 * Author: afischer
 * Date  :2019-05-16  - 11:40
 *  */
#include "oak_ethtool.h"
#include "oak_net.h"

struct oak_s;
extern int debug;
static const char umac_strings[][ETH_GSTRING_LEN] = {
    {"rx_good_frames"},
    {"rx_bad_frames"},
    {"rx_stall_fifo"},
    {"rx_stall_desc"},
    {"rx_discard_desc"},
    {"tx_pause"},
    {"tx_stall_fifo"},
};
static const char rx_strings[][ETH_GSTRING_LEN] = {
    {"Rx Channel"},
    {"rx_alloc_pages"},
    {"rx_unmap_pages"},
    {"rx_alloc_error"},
    {"rx_frame_error"},
    {"rx_errors"},
    {"rx_interrupts"},
    {"rx_good_frames"},
    {"rx_byte_count"},
    {"rx_vlan"},
    {"rx_bad_frames"},
    {"rx_no_sof"},
    {"rx_no_eof"},
    {"rx_bad_crc"},
    {"rx_bad_csum"},
    {"rx_l4p_ok"},
    {"rx_ip4_ok"},
    {"rx_bad_nores"},
#if 1
    {"rx_64"},
    {"rx_128"},
    {"rx_256"},
    {"rx_512"},
    {"rx_1024"},
    {"rx_2048"},
    {"rx_fragments"},
#endif
};
static const char tx_strings[][ETH_GSTRING_LEN] = {
    {"Tx Channel"},
    {"tx_frame_count"},
    {"tx_frame_compl"},
    {"tx_byte_count"},
    {"tx_fragm_count"},
    {"tx_drop"},
    {"tx_errors"},
    {"tx_interrupts"},
    {"tx_stall_count"},
#if 1
    {"tx_64"},
    {"tx_128"},
    {"tx_256"},
    {"tx_512"},
    {"tx_1024"},
    {"tx_2048"},
#endif
};

/* private function prototypes */
static void oak_ethtool_get_txc_stats(oak_t *np, uint64_t *data);
static void oak_ethtool_get_rxc_stats(oak_t *np, uint64_t *data);
static void oak_ethtool_get_stall_stats(oak_t *np);
static void oak_ethtool_get_misc_stats(oak_t *np);

/* Name      : get_rxc_stats
 * Returns   : void
 * Parameters: oak_t *np, * uint64_t * data
 */
static void oak_ethtool_get_rxc_stats(oak_t *np, uint64_t *data)
{
	uint32_t i;

	for (i = 0; i < np->num_rx_chan; i++) {
		oak_rx_chan_t *rxc = &np->rx_channel[i];
		memcpy(data, &rxc->stat, sizeof(oak_driver_rx_stat));
		*data = i + 1;
		data += (sizeof(oak_driver_rx_stat) / sizeof(uint64_t));
	}
}

/* Name      : get_txc_stats
 * Returns   : void
 * Parameters: oak_t *np, uint64_t * data
 */
static void oak_ethtool_get_txc_stats(oak_t *np, uint64_t *data)
{
	uint32_t i;

	for (i = 0; i < np->num_tx_chan; i++) {
		oak_tx_chan_t *txc = &np->tx_channel[i];
		memcpy(data, &txc->stat, sizeof(oak_driver_tx_stat));
		*data = i + 1;
		data += (sizeof(oak_driver_tx_stat) / sizeof(uint64_t));
	}
}

/* Name      : get_stall_stats
 * Returns   : void
 * Parameters: oak_t *np
 */
static void oak_ethtool_get_stall_stats(oak_t *np)
{
	np->unimac_stat.tx_stall_fifo = oak_unimac_io_read_32(np, OAK_UNI_STAT_TX_STALL_FIFO);
	np->unimac_stat.rx_stall_desc = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_STALL_DESC);
	np->unimac_stat.rx_stall_fifo = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_STALL_FIFO);
}

/* Name      : get_misc_stats
 * Returns   : void
 * Parameters: oak_t *np
 */
static void oak_ethtool_get_misc_stats(oak_t *np)
{
	np->unimac_stat.tx_pause = oak_unimac_io_read_32(np, OAK_UNI_STAT_TX_PAUSE);
	np->unimac_stat.rx_good_frames = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_GOOD_FRAMES);
	np->unimac_stat.rx_bad_frames = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_BAD_FRAMES);
	np->unimac_stat.rx_discard_desc = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_DISC_DESC);
}

/* Name      : get_stats
 * Returns   : void
 * Parameters: struct net_device * dev,  struct ethtool_stats * stats,
 * uint64_t * data
 */
void oak_ethtool_get_stats(struct net_device *dev,
		struct ethtool_stats *stats, uint64_t *data)
{
	oak_t *np = netdev_priv(dev);

        oak_ethtool_get_stall_stats(np);
        oak_ethtool_get_misc_stats(np);

	memcpy(data, &np->unimac_stat, sizeof(np->unimac_stat));
	data += sizeof(np->unimac_stat) / sizeof(uint64_t);

	oak_ethtool_get_rxc_stats(np, data);
	oak_ethtool_get_txc_stats(np, data);
}

/* Name      : get_sscnt
 * Returns   : int
 * Parameters: struct net_device* dev, int stringset
 *  */
int oak_ethtool_get_sscnt(struct net_device* dev, int stringset)
{
    int retval;
    oak_t* np = netdev_priv(dev);

    if (stringset == ETH_SS_STATS) {
        retval = sizeof(np->unimac_stat) / sizeof(uint64_t);
        retval += (np->num_rx_chan * sizeof(oak_driver_rx_stat) / sizeof(uint64_t));
        retval += (np->num_tx_chan * sizeof(oak_driver_tx_stat) / sizeof(uint64_t));
        return retval;
    }
    else
        return -EINVAL;
}

/* Name      : get_strings
 * Returns   : void
 * Parameters: struct net_device* dev, uint32_t stringset, uint8_t* data
 *  */
void oak_ethtool_get_strings(struct net_device* dev, uint32_t stringset, uint8_t* data)
{
    if (stringset == ETH_SS_STATS) {
        int off = 0;
        uint32_t i;
        oak_t* np = netdev_priv(dev);

        memcpy(&data[off], umac_strings, sizeof(umac_strings));
        off += sizeof(umac_strings);

        for (i = 0; i < np->num_rx_chan; i++) {
            memcpy(&data[off], rx_strings, sizeof(rx_strings));
            off += sizeof(rx_strings);
        }

        for (i = 0; i < np->num_tx_chan; i++) {
            memcpy(&data[off], tx_strings, sizeof(tx_strings));
            off += sizeof(tx_strings);
        }
    }
}

/* Name      : get_cur_speed
 * Returns   : int
 * Parameters:  oak_t * np, int pspeed
 *  */
int oak_ethtool_cap_cur_speed(oak_t* np, int pspeed)
{
    enum pcie_link_width wdth;

    wdth = oak_net_pcie_get_width_cap(np->pdev);
    if (wdth == PCIE_LNK_X1){ /* Oak */
        if (pspeed > OAK_MAX_SPEED)
            pspeed = OAK_MAX_SPEED;
    }else if (wdth == PCIE_LNK_X2){
        if (pspeed > SPRUCE_MAX_SPEED)
            pspeed = SPRUCE_MAX_SPEED;
    }

    return pspeed;
}

/* Name        : ethtool_get_link_ksettings
 * Returns     : int
 * Parameters  : struct net_device * dev,  struct ethtool_cmd * ecmd
 * Description : Technically the function does the oak ethtool get settings
 * But to maintain the same API name across lin4 and lin5 driver we made the
 * API name change.  But internally functionalities inside the function
 * ethtool_get_link_ksettings will be derived from the linux kernel version
 * 4.x or 5.x as implimented in the drivers.
 *  */
int oak_ethtool_get_link_ksettings(struct net_device* dev, struct ethtool_cmd* ecmd)
{
    oak_t *oak;
    oak = netdev_priv(dev);

    ecmd->autoneg = AUTONEG_ENABLE;
    ecmd->port = PORT_TP;
    ecmd->duplex = DUPLEX_FULL;
    if (oak->speed == OAK_SPEED_1GBPS)
    {
        ecmd->supported = ADVERTISED_1000baseT_Full;
        ecmd->advertising = ADVERTISED_1000baseT_Full;
        ethtool_cmd_speed_set(ecmd, SPEED_1000);
    }
    else
    { /* applies to 5 Gbit speed too */
        ecmd->supported = ADVERTISED_10000baseT_Full;
        ecmd->advertising = ADVERTISED_10000baseT_Full;
        ethtool_cmd_speed_set(ecmd, SPEED_10000);
    }
    return 0;
}
