/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include "ce.h"

#ifndef _SRAM_CONF_H
#define _SRAM_CONF_H

#define CE0_SRAM_SIZE   8
#define CE1_SRAM_SIZE   8
#define CE2_SRAM_SIZE   8
#define CE3_SRAM_SIZE   8
#define CE4_SRAM_SIZE   8
#define CE5_SRAM_SIZE   8
#define CE6_SRAM_SIZE   8
#define CE7_SRAM_SIZE   8

#define CE0_SEC_SRAM_SIZE    4
#define CE1_SEC_SRAM_SIZE    4
#define CE2_SEC_SRAM_SIZE    4
#define CE3_SEC_SRAM_SIZE    4
#define CE4_SEC_SRAM_SIZE    4
#define CE5_SEC_SRAM_SIZE    4
#define CE6_SEC_SRAM_SIZE    4
#define CE7_SEC_SRAM_SIZE    4

#define SRAM_BASE_ADDR 0x00520000

#define SRAM_SIZE           64

void sram_config(void);
addr_t sram_pub_addr(uint32_t ce_id);
addr_t sram_sec_addr(uint32_t ce_id);
addr_t sram_addr(ce_addr_type_t addr_type, uint32_t ce_id);
addr_t get_sram_base(uint32_t vce_id);

#endif
