/*
 * sdrv-unilink-v9.h
 *
 * Copyright(c) 2022 Semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DT_BINDINGS_SDRV_ULINK_V9_H
#define __DT_BINDINGS_SDRV_ULINK_V9_H

#ifndef SERVICES_MEMBASE
#error "undefined macro SERVICES_MEMBASE"
#endif

#define UNILINK_TX_BASE_R 0x16000000
#define UNILINK_RX_BASE_R (SERVICES_MEMBASE+0x600000) //0x46000000
#define UNILINK_TX_BASE_L (UNILINK_RX_BASE_R+0x1000000)//0x47000000
#define UNILINK_RX_BASE_L (UNILINK_RX_BASE_R+0x1000000) //0x47000000

#define SAF_TO_AP1_R 0x400000
#define AP1_TO_SAF_R 0x400000
#define AP1_TO_AP1_R 0x800000

#define SAF_TO_AP1 0x0
#define AP1_TO_SAF 0x400000

/*AP1*/
#define UNILINK_TX_AP1_TO_SAF_L (UNILINK_TX_BASE_L + AP1_TO_SAF)
#define UNILINK_RX_AP1_TO_SAF_L (UNILINK_RX_BASE_L + SAF_TO_AP1)

#define UNILINK_TX_AP1_TO_AP1_R (UNILINK_TX_BASE_R + AP1_TO_AP1_R)
#define UNILINK_RX_AP1_TO_AP1_R (UNILINK_RX_BASE_R + AP1_TO_AP1_R)

/* only b have this */
#define UNILINK_TX_AP1_TO_SAF_R (UNILINK_TX_BASE_R + AP1_TO_SAF_R)
#define UNILINK_RX_AP1_TO_SAF_R (UNILINK_RX_BASE_R + SAF_TO_AP1_R)
#endif /* __DT_BINDINGS_SDRV_ULINK_V9_H */

