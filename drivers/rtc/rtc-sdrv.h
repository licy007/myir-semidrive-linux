/*
 * sdrv_scr.h
 *
 * Copyright(c) 2020 Semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RTC_SDRV_H__
#define __RTC_SDRV_H__

#include <linux/bitops.h>

/*------------ registers offset definition. ------------*/

#define RTCROFF(_name) RTCROFF_##_name##_OFFSET

#define RTCROFF_RTC_CTRL_OFFSET		(0x00)
#define RTCROFF_RTC_H_OFFSET		(0x04)
#define RTCROFF_RTC_L_OFFSET		(0x08)
#define RTCROFF_AUTO_ADJUST_OFFSE	(0x0C)
#define RTCROFF_TIMER_H_OFFSET		(0x10)
#define RTCROFF_TIMER_L_OFFSET		(0x14)
#define RTCROFF_WAKEUP_CTRL_OFFSET	(0x18)
#define RTCROFF_PERIODICAL_CTRL_OFFSET	(0x1C)
#define RTCROFF_VIOLATION_INT_OFFSET	(0x20)

/*------------ bit definition. ------------*/
#define RTCB(_reg_name,_bit_name) RTCB_##_reg_name##_##_bit_name

/* RTC_CTRL */
#define RTCB_RTC_CTRL_LOCK	BIT(31)
#define RTCB_RTC_CTRL_PRV_EN	BIT(2)
#define RTCB_RTC_CTRL_SEC_EN	BIT(1)
#define RTCB_RTC_CTRL_EN	BIT(0)

/* WAKEUP_CTRL */
#define RTCB_WAKEUP_CTRL_DIS_VIO_CLR	BIT(6)
#define RTCB_WAKEUP_CTRL_OVF_VIO_CLR	BIT(5)
#define RTCB_WAKEUP_CTRL_CLEAR		BIT(4) //write 1 to clear IRQ status
#define RTCB_WAKEUP_CTRL_STATUS		BIT(3)
#define RTCB_WAKEUP_CTRL_EN		BIT(2)
#define RTCB_WAKEUP_CTRL_IRQ_EN		BIT(1)
#define RTCB_WAKEUP_CTRL_REQ_EN		BIT(0)

#endif
