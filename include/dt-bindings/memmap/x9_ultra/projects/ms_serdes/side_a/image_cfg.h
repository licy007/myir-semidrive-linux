
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
#define SAF_AP2_MEMBASE 0x35E00000
#define SAF_AP2_MEMSIZE 0x200000
#define SEC_AP1_MEMBASE 0x36000000
#define SEC_AP1_MEMSIZE 0x200000
#define SEC_AP2_MEMBASE 0x36200000
#define SEC_AP2_MEMSIZE 0x200000
#define AP1_AP2_MEMBASE 0x46400000
#define AP1_AP2_MEMSIZE 0x200000
#define SAF_MP_MEMBASE 0x36600000
#define SAF_MP_MEMSIZE 0x200000
#define SEC_MP_MEMBASE 0x36800000
#define SEC_MP_MEMSIZE 0x200000
#define MP_AP1_MEMBASE 0x36A00000
#define MP_AP1_MEMSIZE 0x200000
#define MP_AP2_MEMBASE 0x36C00000
#define MP_AP2_MEMSIZE 0x200000
#define AP1_ATF_MEMBASE 0x46E00000
#define AP1_ATF_MEMSIZE 0x200000
#define AP1_TEE_MEMBASE 0x47000000
#define AP1_TEE_MEMSIZE 0x1000000
#define MP_MEMBASE 0x38000000
#define MP_MEMSIZE 0xE800000
#define SAF_MEMBASE 0x46800000
#define SAF_MEMSIZE 0x1000000
#define AP1_HYP_MEMBASE 0x57800000
#define AP1_HYP_MEMSIZE 0x2000000
#define AP1_DOM0_MEMBASE 0x59800000
#define AP1_DOM0_MEMSIZE 0x2000000
#define AP1_REE_MEMBASE 0x57800000
#define AP1_REE_MEMSIZE 0x60000000
#define AP1_PRELOADER_MEMBASE 0x57A00000
#define AP1_PRELOADER_MEMSIZE 0x400000
#define AP1_BOOTLOADER_MEMBASE 0x59800000
#define AP1_BOOTLOADER_MEMSIZE 0x800000
#define AP1_KERNEL_MEMBASE 0x5B880000
#define AP1_KERNEL_MEMSIZE 0x5FE00000
#define AP1_BOARD_RAMDISK_MEMBASE 0x63800000
#define AP1_BOARD_RAMDISK_MEMSIZE 0x2000000
#define SERVICES_MEMBASE 0xB7800000
#define SERVICES_MEMSIZE 0x5400000
#define AP2_IMAGES_MEMBASE 0xACC00000
#define AP2_IMAGES_MEMSIZE 0x13000000
#define AP2_ATF_MEMBASE 0xCFC00000
#define AP2_ATF_MEMSIZE 0x200000
#define AP2_TEE_MEMBASE 0xCFE00000
#define AP2_TEE_MEMSIZE 0x1000000
#define AP2_REE_MEMBASE 0xD0E00000
#define AP2_REE_MEMSIZE 0x40000000
#define AP2_PRELOADER_MEMBASE 0xD1000000
#define AP2_PRELOADER_MEMSIZE 0x400000
#define AP2_BOOTLOADER_MEMBASE 0xD2E00000
#define AP2_BOOTLOADER_MEMSIZE 0x800000
#define AP2_KERNEL_MEMBASE 0xD4E80000
#define AP2_KERNEL_MEMSIZE 0x3FE00000
#define AP2_BOARD_RAMDISK_MEMBASE 0xDCE00000
#define AP2_BOARD_RAMDISK_MEMSIZE 0x10000000
#define AP1_2ND_MEMBASE 0x110E00000
#define AP1_2ND_MEMSIZE 0x12F200000

#endif /* __IMAGE_CFG_H__*/