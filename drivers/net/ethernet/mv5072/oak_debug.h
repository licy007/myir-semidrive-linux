
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
 * Contents:  debug interface functions as called from Linux kernel
 *
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_debug
 * Author: Naresh Bhat
 * Date  :2021-04-27  - 16:57
 *  */

#include "linux/etherdevice.h"
#include "linux/pci.h"

extern int debug;

void oak_dbg_set_level(struct net_device* dev, uint32_t level);
uint32_t oak_dbg_get_level(struct net_device* dev);
