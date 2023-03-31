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
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_irq
 * Author: afischer
 * Date  :2019-05-16  - 11:43
 *  */
#ifndef H_OAK_IRQ
#define H_OAK_IRQ

#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */

extern int debug;
struct oak_tStruct;
/* Name      : request_ivec
 * Returns   : int
 * Parameters:  struct oak_tStruct * np
 *  */
int oak_irq_request_ivec(struct oak_tStruct* np);

/* Name      : callback
 * Returns   : irqreturn_t
 * Parameters:  int irq = irq,  void * cookie = cookie
 *  */
irqreturn_t oak_irq_callback(int irq, void* cookie);

/* Name      : request_single_ivec
 * Returns   : int
 * Parameters:  struct oak_tStruct * np = np,  ldg_t * ldg = ldg,  uint64_t val = val,  uint32_t idx
 *  */
int oak_irq_request_single_ivec(struct oak_tStruct* np, ldg_t* ldg, uint64_t val, uint32_t idx);

/* Name      : release_ivec
 * Returns   : void
 * Parameters:  struct oak_tStruct * np
 *  */
void oak_irq_release_ivec(struct oak_tStruct* np);

/* Name      : enable_gicu_64
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint64_t mask = mask
 *  */
void oak_irq_enable_gicu_64(struct oak_tStruct* np, uint64_t mask);

/* Name      : disable_gicu_64
 * Returns   : void
 * Parameters:  uint64_t mask,  struct oak_tStruct * np
 *  */
void oak_irq_disable_gicu_64(uint64_t mask, struct oak_tStruct* np);

/* Name      : dis_gicu
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint32_t mask_0 = mask_0,  uint32_t mask_1 = mask_1
 *  */
void oak_irq_dis_gicu(struct oak_tStruct* np, uint32_t mask_0, uint32_t mask_1);

/* Name      : ena_gicu
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = p,  uint32_t mask_0 = mask_0,  uint32_t mask_1 = mask_1
 *  */
void oak_irq_ena_gicu(struct oak_tStruct* np, uint32_t mask_0, uint32_t mask_1);

/* Name      : ena_general
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint32_t enable = enable
 *  */
void oak_irq_ena_general(struct oak_tStruct* np, uint32_t enable);

/* Name      : enable_groups
 * Returns   : int
 * Parameters:  struct oak_tStruct * np = np
 *  */
int oak_irq_enable_groups(struct oak_tStruct* np);

/* Name      : disable_groups
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np
 *  */
void oak_irq_disable_groups(struct oak_tStruct* np);

#endif /* #ifndef H_OAK_IRQ */

