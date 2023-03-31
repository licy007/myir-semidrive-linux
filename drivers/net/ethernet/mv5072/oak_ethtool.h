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
#ifndef H_OAK_ETHTOOL
#define H_OAK_ETHTOOL

#include <linux/ethtool.h>
#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */

/* Oak & Spruce max PCIe speed in Gbps */
#define OAK_MAX_SPEED    5
#define SPRUCE_MAX_SPEED 10

#define OAK_SPEED_1GBPS 1
#define OAK_SPEED_5GBPS 5

struct oak_s;
extern int debug;

/* Name      : get_stats
 * Returns   : void
 * Parameters: struct net_device * dev,  struct ethtool_stats * stats,
 * uint64_t * data
 */
void oak_ethtool_get_stats(struct net_device *dev,
	struct ethtool_stats *stats, uint64_t *data);


/* Name      : get_sscnt
 * Returns   : int
 * Parameters:  oak_t * np
 *  */
int oak_ethtool_get_sscnt(struct net_device* dev, int stringset);

/* Name      : get_strings
 * Returns   : void
 * Parameters: struct net_device* dev, uint32_t stringset, uint8_t* data
 *  */
void oak_ethtool_get_strings(struct net_device* dev, uint32_t stringset, uint8_t* data);

/* Name      : cap_cur_speed
 * Returns   : int
 * Parameters:  oak_t * np,  int pspeed
 *  */
int oak_ethtool_cap_cur_speed(oak_t* np, int pspeed);


/* Name      : get_link_ksettings
 * Returns   : int
 * Parameters:  net_device * dev,  ethtool_cmd * ecmd
 *  */
int oak_ethtool_get_link_ksettings(struct net_device* dev, struct ethtool_cmd* ecmd);

#endif /* #ifndef H_OAK_ETHTOOL */

