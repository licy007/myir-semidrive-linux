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
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ioc_reg
 * Author: afischer
 * Date  :2019-05-16  - 11:40
 *  */
#ifndef H_OAK_IOC_REG
#define H_OAK_IOC_REG

typedef struct oak_ioc_regStruct
{
#define OAK_IOCTL_REG_ESU_REQ (SIOCDEVPRIVATE + 1)
#define OAK_IOCTL_REG_MAC_REQ (SIOCDEVPRIVATE + 2)
#define OAK_IOCTL_REG_RD 1
#define OAK_IOCTL_REG_WR 2
#define OAK_IOCTL_REG_WS 3
#define OAK_IOCTL_REG_WC 4
#define OAK_IOCTL_REG_OR 5
#define OAK_IOCTL_REG_AND 6
    __u32 cmd;
    __u32 offs;
    __u32 dev_no;
    __u32 data;
    __u32 device;
    int error;
} oak_ioc_reg;

#endif /* #ifndef H_OAK_IOC_REG */

