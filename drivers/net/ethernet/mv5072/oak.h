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
 * Contents: functions as called from Linux kernel (entry points)
 *
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak
 * Author: afischer
 * Date  :20.02.00005-12  - 16:10
 *  */
#ifndef H_OAK
#define H_OAK

#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */
#include "oak_net.h" /* Include for relation to classifier oak_net */
#include "oak_ethtool.h" /* Include for relation to classifier oak_ethtool */
#include "oak_module.h" /* Include for relation to classifier oak_module */
#include "oak_debug.h"
#include "oak_chksum.h"
#include "oak_dpm.h"

#define OAK_DRIVER_NAME "oak"

#define OAK_DRIVER_STRING "Marvell PCIe Switch Driver"

#define OAK_DRIVER_VERSION "0.02.0000"

#define OAK_DRIVER_COPYRIGHT "Copyright (c) Marvell - 2018"

#define OAK_MAX_JUMBO_FRAME_SIZE (10 * 1024)


int debug;
int txs;
int rxs;
int chan;
int rto;
int mhdr;
int port_speed;

/* Name      : get_msix_resources
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_get_msix_resources(struct pci_dev* pdev);

/* Name      : release_hardware
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_hardware(struct pci_dev* pdev);

/* Name      : init_hardware
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_init_hardware(struct pci_dev* pdev);

/* Name      : init_pci
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_init_pci(struct pci_dev* pdev);

/* Name      : init_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_init_software(struct pci_dev* pdev);

/* Name      : release_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_software(struct pci_dev* pdev);

/* Name      : start_hardware
 * Returns   : int
 * Parameters:
 *  */
int oak_start_hardware(void);

/* Name      : start_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_start_software(struct pci_dev* pdev);

/* Name      : stop_hardware
 * Returns   : void
 * Parameters:
 *  */
void oak_stop_hardware(void);

/* Name      : stop_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_stop_software(struct pci_dev* pdev);

#endif /* #ifndef H_OAK */

