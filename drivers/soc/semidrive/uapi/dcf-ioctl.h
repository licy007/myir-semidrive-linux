/*
 * drivers/soc/semidrive/uapi/dcf-ioctl.h
 *
 * Copyright (C) 2020 Semidrive, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _UAPI_LINUX_DCF_IOC_H
#define _UAPI_LINUX_DCF_IOC_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DCF_MSG_MAX_LEN		(120)

#define MAX_USER_NAME		(16)
#define DCF_USER_ANY		(255)
#define DCF_USER_SUPER		(0)

struct dcf_usercontext {
	char name[MAX_USER_NAME];
	__u32 user_id;
	__u32 user_id_other;
	__u32 reserved1;
	__u32 reserved2;
};

struct dcf_ioc_setproperty {
	__u32 property_id;
	__u32 property_value;
	__u32 reserved1;
	__u32 reserved2;
};

struct vircan_ioc_setbaud {
	__u32 controller_id;
	__u32 baudrate_level;
	__u32 reserved1;
	__u32 reserved2;
};

struct vircan_ioc_setfilter {
	__u16 mode;
	__u16 id_range_high;
	__u16 id_range_low;
	__u16 batch_id;
	__u16 id_count;
	__u16 can_id[4];
};

#define DCF_IOC_MAGIC		'D'

/**
 * DOC: DCF_IOC_SET_USER_ID - set user context and id
 *
 */
#define DCF_IOC_SET_USER_CTX	_IOWR(DCF_IOC_MAGIC, 1,\
					struct dcf_usercontext)

#define DCF_IOC_SET_PROPERTY	_IOWR(DCF_IOC_MAGIC, 2,\
					struct dcf_ioc_setproperty)

#define DCF_IOC_GET_PROPERTY	_IOWR(DCF_IOC_MAGIC, 3,\
					struct dcf_ioc_setproperty)

#define VIRCAN_IOC_MAGIC		'V'

#define VIRCAN_IOC_SET_FILTER	_IOWR(VIRCAN_IOC_MAGIC, 1,\
					struct vircan_ioc_setfilter)

#endif /* _UAPI_LINUX_DCF_IOC_H */

