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
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_gicu
 * Author: afischer
 * Date  :2019-05-16  - 11:40
 *  */
#ifndef H_OAK_GICU
#define H_OAK_GICU

#include "ldg_t.h" /* Include for relation to classifier ldg_t */
typedef struct oak_gicuStruct
{
#define OAK_GICU_IRQ_BASE 0x00070000
#define OAK_GICU_IRQ_REG(o) (OAK_GICU_IRQ_BASE + (o))
#define TX_DMA_BIT 0
#define TX_ERR_BIT 1
#define RX_DMA_BIT 2
#define RX_ERR_BIT 3
#define UNIMAC_DMA_BIT 31
#define OAK_GICU_INTR_DBG_CTRL OAK_GICU_IRQ_REG(0x000)
#define OAK_GICU_INTR_FLAG_0 OAK_GICU_IRQ_REG(0x010)
#define OAK_GICU_INTR_FLAG_1 OAK_GICU_IRQ_REG(0x014)
#define OAK_GICU_HOST_SET_MASK_0 OAK_GICU_IRQ_REG(0x020)
#define OAK_GICU_HOST_SET_MASK_1 OAK_GICU_IRQ_REG(0x024)
#define OAK_GICU_HOST_CLR_MASK_0 OAK_GICU_IRQ_REG(0x030)
#define OAK_GICU_HOST_CLR_MASK_1 OAK_GICU_IRQ_REG(0x034)
#define OAK_GICU_HOST_MASK_0 0xFFFFFFFF
#define OAK_GICU_HOST_MASK_1 0x000000FF
#define OAK_GICU_HOST_MASK_E 0x003FFC00
#define OAK_GICU_HOST_UNIMAC_P11_IRQ (1 << 8)
#define OAK_GICU_HOST_UNIMAC_P11_RESET (1 << 9)
#define OAK_GICU_DBG_INTR_EVNT_0 OAK_GICU_IRQ_REG(0x040)
#define OAK_GICU_DBG_INTR_EVNT_1 OAK_GICU_IRQ_REG(0x044)
#define OAK_GICU_DBG_REG_0_L OAK_GICU_IRQ_REG(0x050)
#define OAK_GICU_DBG_REG_0_H OAK_GICU_IRQ_REG(0x054)
#define OAK_GICU_DBG_REG_1_L OAK_GICU_IRQ_REG(0x060)
#define OAK_GICU_DBG_REG_1_H OAK_GICU_IRQ_REG(0x064)
#define OAK_GICU_DBG_REG_2 OAK_GICU_IRQ_REG(0x070)
#define OAK_GICU_DBG_REG_3 OAK_GICU_IRQ_REG(0x078)
#define OAK_GICU_INTR_GRP_SET_MASK OAK_GICU_IRQ_REG(0x080)
#define OAK_GICU_INTR_GRP_CLR_MASK OAK_GICU_IRQ_REG(0x084)
#define OAK_GICU_INTR_GRP_MASK_ENABLE (1 << 31)
#define OAK_GICU_INTR_GRP_MASK_0 OAK_GICU_IRQ_REG(0x090)
#define OAK_GICU_INTR_GRP_MASK_1 OAK_GICU_IRQ_REG(0x094)
#define OAK_GICU_EPU_INTR_MASK_0 OAK_GICU_IRQ_REG(0x0C0)
#define OAK_GICU_EPU_INTR_MASK_1 OAK_GICU_IRQ_REG(0x0C4)
#define OAK_GICU_PIN_INTR_MASK_0 OAK_GICU_IRQ_REG(0x0d0)
#define OAK_GICU_PIN_INTR_MASK_1 OAK_GICU_IRQ_REG(0x0d4)
#define OAK_MAX_INTR_GRP 64
#define OAK_MAX_CHAN_NUM 10
#define OAK_GICU_INTR_GRP_NUM(g) OAK_GICU_IRQ_REG(0x100 + 4 * (g))
#define OAK_INTR_MASK_TX_DMA (1 << 0)
#define OAK_INTR_MASK_TX_ERR (1 << 1)
#define OAK_INTR_MASK_RX_DMA (1 << 2)
#define OAK_INTR_MASK_RX_ERR (1 << 3)
#define OAK_NUM_IVEC (OAK_MAX_CHAN_NUM * 4 + 1)
    struct msix_entry msi_vec[OAK_NUM_IVEC];
    uint32_t num_ldg;
    ldg_t ldg[OAK_NUM_IVEC];
} oak_gicu;

#endif /* #ifndef H_OAK_GICU */

