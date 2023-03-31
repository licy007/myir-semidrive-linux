
//*****************************************************************************
//
// WARNING: Automatically generated file, don't modify anymore!!!
//
// Copyright (c) 2019-2029 Semidrive Incorporated.  All rights reserved.
// Software License Agreement
//
//*****************************************************************************

#ifndef __MEM_SHARE_H__
#define __MEM_SHARE_H__
#define AP1_CMASIZE 0x40000000
#define CARVEOUT_SIZE 0x4000000
#if ((SERVICES_MEMBASE - AP1_CMASIZE - 0x100000) > 0x40000000)
#define AP2_RESERVE_BASE (SERVICES_MEMBASE - AP1_CMASIZE - 0x100000)
#define AP2_RESERVE_SIZE (AP1_CMASIZE + 0x100000)
#else
#define AP2_RESERVE_BASE AP1_2ND_MEMBASE
#define AP2_RESERVE_SIZE AP1_2ND_MEMSIZE
#endif
#endif
