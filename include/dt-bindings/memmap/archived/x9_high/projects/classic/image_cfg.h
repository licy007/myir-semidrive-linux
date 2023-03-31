
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

#define DTB_MEMSIZE 0x200000
#define BOARD_RAMDISK_OFFSET 0xC000000
#define BOARD_TARS_OFFSET 0x0
#define BOARD_KERNEL_OFFSET 0x4080000
#define SEC_MEMBASE 0x30000000
#define SEC_MEMSIZE 0x600000
#define SYS_CFG_MEMBASE 0x30600000
#define SYS_CFG_MEMSIZE 0x200000
#define VBMETA_MEMBASE 0x30800000
#define VBMETA_MEMSIZE 0x10000
#define DIL_IMAGES_MEMBASE 0x30810000
#define DIL_IMAGES_MEMSIZE 0x1F0000
#define VDSP_MEMBASE 0x40A00000
#define VDSP_MEMSIZE 0x3000000
#define VDSP_SHARE_MEMBASE 0x43A00000
#define VDSP_SHARE_MEMSIZE 0x2000000
#define SAF_SEC_MEMBASE 0x35A00000
#define SAF_SEC_MEMSIZE 0x200000
#define SAF_AP1_MEMBASE 0x35C00000
#define SAF_AP1_MEMSIZE 0x200000
#define SEC_AP1_MEMBASE 0x35E00000
#define SEC_AP1_MEMSIZE 0x200000
#define AP1_ATF_MEMBASE 0x46000000
#define AP1_ATF_MEMSIZE 0x200000
#define AP1_TEE_MEMBASE 0x46200000
#define AP1_TEE_MEMSIZE 0x1000000
#define SAF_MEMBASE 0x37200000
#define SAF_MEMSIZE 0x10000000
#define AP1_HYP_MEMBASE 0x57200000
#define AP1_HYP_MEMSIZE 0x2000000
#define AP1_DOM0_MEMBASE 0x59200000
#define AP1_DOM0_MEMSIZE 0x2000000
#define AP1_REE_MEMBASE 0x57200000
#define AP1_REE_MEMSIZE 0x20000000
#define AP1_PRELOADER_MEMBASE 0x57400000
#define AP1_PRELOADER_MEMSIZE 0x400000
#define AP1_BOOTLOADER_MEMBASE 0x59200000
#define AP1_BOOTLOADER_MEMSIZE 0x800000
#define AP1_KERNEL_MEMBASE 0x5B280000
#define AP1_KERNEL_MEMSIZE 0x1FE00000
#define AP1_BOARD_RAMDISK_MEMBASE 0x63200000
#define AP1_BOARD_RAMDISK_MEMSIZE 0x2000000
#define SERVICES_MEMBASE 0x77200000
#define SERVICES_MEMSIZE 0x5400000
#define AP1_2ND_MEMBASE 0x7C600000
#define AP1_2ND_MEMSIZE 0x1A3A00000
#define CLU_MEMBASE 0x220000000
#define CLU_MEMSIZE 0x20000000
#define CLU_BOOTLOADER_MEMBASE 0x220800000
#define CLU_BOOTLOADER_MEMSIZE 0x800000

#endif /* __IMAGE_CFG_H__*/
