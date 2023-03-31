//*****************************************************************************
//
// WARNING: Automatically generated file, don't modify anymore!!!
//
// Copyright (c) 2019-2029 Semidrive Incorporated.  All rights reserved.
// Software License Agreement
//
//*****************************************************************************

#ifndef __IMAGE_CFG_H__
#define __IMAGE_CFG_H__

#define LOW32(x) ((x) & 0xffffffff)
#define HIGH32(x) ((x) >> 32)

#define BOARD_RAMDISK_OFFSET 0x8000000
#define BOARD_TARS_OFFSET 0x0
#define BOARD_KERNEL_OFFSET 0x200000
#define SAF_MEMBASE 0x30000000
#define SAF_MEMSIZE 0x2000000
#define SEC_MEMBASE 0x32000000
#define SEC_MEMSIZE 0x800000
#define SAF_SEC_MEMBASE 0x32800000
#define SAF_SEC_MEMSIZE 0x100000
#define SAF_ECO_MEMBASE 0x32900000
#define SAF_ECO_MEMSIZE 0x200000
#define SEC_ECO_MEMBASE 0x32B00000
#define SEC_ECO_MEMSIZE 0x100000
#define ECO_ATF_MEMBASE 0x42C00000
#define ECO_ATF_MEMSIZE 0x200000
#define ECO_TEE_MEMBASE 0x42E00000
#define ECO_TEE_MEMSIZE 0x1000000
#define ECO_HYP_MEMBASE 0x43E00000
#define ECO_HYP_MEMSIZE 0x2000000
#define ECO_DEV_MEMBASE 0x45E00000
#define ECO_DEV_MEMSIZE 0x2000000
#define ECO_REE_MEMBASE 0x47E00000
#define ECO_REE_MEMSIZE 0xD8200000
#define PRELOADER_MEMBASE 0x49E00000
#define PRELOADER_MEMSIZE 0x400000
#define ECO_BOOTLOADER_MEMBASE 0x4C200000
#define ECO_BOOTLOADER_MEMSIZE 0x800000
#define KERNEL_MEMBASE 0x48000000
#define CLU_MEMBASE 0x120000000
#define CLU_MEMSIZE 0x20000000
#define CLU_BOOTLOADER_MEMBASE 0x120800000
#define CLU_BOOTLOADER_MEMSIZE 0x800000

#endif /* __IMAGE_CFG_H__*/
