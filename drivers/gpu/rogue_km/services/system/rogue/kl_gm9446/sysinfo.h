/*
* sysinfo.h
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

#if !defined(__SYSINFO_H__)
#define __SYSINFO_H__

/*!< System specific poll/timeout details */
#define MAX_HW_TIME_US                           (500000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT  (1500)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT (3600000)
#define WAIT_TRY_COUNT                           (10000)

/* unit ms, timeout interval for DVFS detection */
#define SD_DVFS_SWITCH_INTERVAL  0

#if defined(__linux__)
#define SYS_RGX_DEV_NAME    "rgxkunlun"
#define SYS_RGX_OF_COMPATIBLE "imagination,pvr-rogue"
#endif

#endif	/* !defined(__SYSINFO_H__) */

