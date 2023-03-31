/*
 * sdrv-unilink-x9.h
 *
 * Copyright(c) 2022 Semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DT_BINDINGS_SDRV_ULINK_X9HP_H
#define __DT_BINDINGS_SDRV_ULINK_X9HP_H

#ifndef SERVICES_MEMBASE
#error "undefined macro SERVICES_MEMBASE"
#endif
#define UNILINK_BASE (SERVICES_MEMBASE+0x800000) //0x78000000

#define SAF_TO_AP1 0x0
#define SAF_TO_AP2 0x200000
#define AP1_TO_SAF 0x400000
#define AP1_TO_AP2 0x800000
#define AP2_TO_SAF 0xa00000
#define AP2_TO_AP1 0xc00000

/*AP1*/
#define UNILINK_TX_AP1_TO_SAF_L (UNILINK_BASE + AP1_TO_SAF)
#define UNILINK_RX_AP1_TO_SAF_L (UNILINK_BASE + SAF_TO_AP1)
#define UNILINK_TX_AP1_TO_AP2_L (UNILINK_BASE + AP1_TO_AP2)
#define UNILINK_RX_AP1_TO_AP2_L (UNILINK_BASE + AP2_TO_AP1)

/*AP2*/
#define UNILINK_TX_AP2_TO_SAF_L (UNILINK_BASE + AP2_TO_SAF)
#define UNILINK_RX_AP2_TO_SAF_L (UNILINK_BASE + SAF_TO_AP2)
#define UNILINK_TX_AP2_TO_AP1_L (UNILINK_BASE + AP2_TO_AP1)
#define UNILINK_RX_AP2_TO_AP1_L (UNILINK_BASE + AP1_TO_AP2)

#endif /* __DT_BINDINGS_SDRV_ULINK_X9_HP */
