/*
* sysconfig.h
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System abstraction layer .
*
* Revision History:
* -----------------
* 011, 01/14/2019 Lili create this file
*/

#include "pvrsrv_device.h"
#include "rgxdevice.h"

#if !defined(__SYSCCONFIG_H__)
#define __SYSCCONFIG_H__


#define RGX_KUNLUN_CORE_CLOCK_SPEED 800000000 //800mHz
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (100)

/*****************************************************************************
 * system specific data structures
 *****************************************************************************/

PVRSRV_DEVICE_CONFIG *getDevConfig(void);

#endif	/* __SYSCCONFIG_H__ */

