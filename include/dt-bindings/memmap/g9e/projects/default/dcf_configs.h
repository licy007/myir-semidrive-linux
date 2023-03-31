
//*****************************************************************************
//
// WARNING: Automatically generated file, don't modify anymore!!!
//
// Copyright (c) 2019-2029 Semidrive Incorporated.  All rights reserved.
// Software License Agreement
//
//*****************************************************************************

#ifndef __DCF_CONFIGS_H__
#define __DCF_CONFIGS_H__

typedef struct dcf_config {
    uint32_t res_id;
    addr_t paddr;
    uint32_t size;
    uint8_t mid_src;
    uint8_t mid_dst;
    uint8_t mid_master;
    uint8_t channel;
}dcf_config_t;

typedef struct dcf_configs {
    uint32_t config_num;
    dcf_config_t dcf_config[];
} dcf_configs_t;

const dcf_configs_t dcf_configs_list = {

	.config_num = 0,
};

#endif /* __DCF_CONFIGS_H__*/
