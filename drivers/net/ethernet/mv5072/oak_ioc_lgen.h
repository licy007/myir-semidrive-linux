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
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ioc_lgen
 * Author: afischer
 * Date  :2019-05-16  - 11:40
 *  */
#ifndef H_OAK_IOC_LGEN
#define H_OAK_IOC_LGEN

typedef struct oak_ioc_lgenStruct
{
#define OAK_IOCTL_LGEN (SIOCDEVPRIVATE + 3)
#define OAK_IOCTL_LGEN_INIT (1 << 0)
#define OAK_IOCTL_LGEN_TX_DATA (1 << 1)
#define OAK_IOCTL_LGEN_TX_START (1 << 2)
#define OAK_IOCTL_LGEN_TX_STOP (1 << 3)
#define OAK_IOCTL_LGEN_RX_START (1 << 4)
#define OAK_IOCTL_LGEN_RX_STOP (1 << 5)
#define OAK_IOCTL_LGEN_RX_DATA (1 << 6)
#define OAK_IOCTL_LGEN_RELEASE (1 << 7)
#define OAK_IOCTL_LGEN_TX_RESET (1 << 8)
#define OAK_IOCTL_LGEN_RX_RESET (1 << 9)
    __u32 cmd;
    __u32 offs;
    __u32 len;
    __u32 channel;
    __u32 count;
    char data[32];
    int error;
} oak_ioc_lgen;

#endif /* #ifndef H_OAK_IOC_LGEN */

