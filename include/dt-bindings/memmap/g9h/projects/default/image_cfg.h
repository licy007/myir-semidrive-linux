
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
#define ROUTE_TAB_MEMBASE 0x1E7000
#define ROUTE_TAB_MEMSIZE 0x10000
#define SDPE_STAT_MEMBASE 0x1F7000
#define SDPE_STAT_MEMSIZE 0x1000
#define SAF_SDPE_RPC_MEMBASE 0x1F8000
#define SAF_SDPE_RPC_MEMSIZE 0x8000
#define SEC_MEMBASE 0x30000000
#define SEC_MEMSIZE 0x800000
#define SYS_CFG_MEMBASE 0x30800000
#define SYS_CFG_MEMSIZE 0x200000
#define VBMETA_MEMBASE 0x30A00000
#define VBMETA_MEMSIZE 0x10000
#define DIL_IMAGES_MEMBASE 0x30A10000
#define DIL_IMAGES_MEMSIZE 0xFF0000
#define SAF_SEC_MEMBASE 0x31A00000
#define SAF_SEC_MEMSIZE 0x200000
#define SAF_AP1_MEMBASE 0x31C00000
#define SAF_AP1_MEMSIZE 0x200000
#define SAF_AP2_MEMBASE 0x31E00000
#define SAF_AP2_MEMSIZE 0x200000
#define SAF_MP_MEMBASE 0x32000000
#define SAF_MP_MEMSIZE 0x200000
#define SEC_AP1_MEMBASE 0x32200000
#define SEC_AP1_MEMSIZE 0x200000
#define SEC_AP2_MEMBASE 0x32400000
#define SEC_AP2_MEMSIZE 0x200000
#define SEC_MP_MEMBASE 0x32600000
#define SEC_MP_MEMSIZE 0x200000
#define MP_AP1_MEMBASE 0x32800000
#define MP_AP1_MEMSIZE 0x200000
#define MP_AP2_MEMBASE 0x32A00000
#define MP_AP2_MEMSIZE 0x200000
#define AP1_AP2_MEMBASE 0x42C00000
#define AP1_AP2_MEMSIZE 0x200000
#define AP1_ATF_MEMBASE 0x42E00000
#define AP1_ATF_MEMSIZE 0x200000
#define AP1_TEE_MEMBASE 0x43000000
#define AP1_TEE_MEMSIZE 0x1000000
#define SAF_MEMBASE 0x34000000
#define SAF_MEMSIZE 0x800000
#define MP_MEMBASE 0x34800000
#define MP_MEMSIZE 0x800000
#define AP2_PRELOADER_MEMBASE 0x45000000
#define AP2_PRELOADER_MEMSIZE 0x200000
#define SERVICES_MEMBASE 0x45200000
#define SERVICES_MEMSIZE 0x5400000
#define AP1_HYP_MEMBASE 0x4A600000
#define AP1_HYP_MEMSIZE 0x2000000
#define AP1_DOM0_MEMBASE 0x4C600000
#define AP1_DOM0_MEMSIZE 0x2000000
#define AP1_REE_MEMBASE 0x4A600000
#define AP1_REE_MEMSIZE 0x15A00000
#define AP1_PRELOADER_MEMBASE 0x4A800000
#define AP1_PRELOADER_MEMSIZE 0x400000
#define AP1_BOOTLOADER_MEMBASE 0x4C600000
#define AP1_BOOTLOADER_MEMSIZE 0x800000
#define AP1_KERNEL_MEMBASE 0x4E680000
#define AP1_KERNEL_MEMSIZE 0x15800000
#define AP1_BOARD_RAMDISK_MEMBASE 0x56600000
#define AP1_BOARD_RAMDISK_MEMSIZE 0x1000000

#endif /* __IMAGE_CFG_H__*/