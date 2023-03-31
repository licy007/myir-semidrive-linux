
/*
 * sdrv-pfm-group.h
 *
 * Copyright(c) 2020 Semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DT_BINDINGS_SDRV_PFM_GROUP_H
#define __DT_BINDINGS_SDRV_PFM_GROUP_H

/*this master id enum is defined by soc, can not change*/
/*
    SAF_PLATFORM,
    SEC_PLATFORM,
    MP_PLATFORM,
    AP1, //3
    AP2,/*4
    VDSP,/*5
    ADSP,/*6
    RESERVED1,
    DMA1,/*8
    DMA2,/*9
    DMA3,/*10
    DMA4,/*11
    DMA5,/*12
    DMA6,/*13
    DMA7,/*14
    DMA8,/*15
    CSI1,/*16
    CSI2,/*17
    CSI3,/*18
    DC1,/*19
    DC2,/*20
    DC3,/*21
    DC4,/*22
    DP1,/*23
    DP2,/*24
    DP3,/*25
    DC5,/*26
    G2D1,/*27
    G2D2,/*28
    VPU1,/*29
    VPU2,/*30
    MJPEG,/*31
    MSHC1,/*32
    MSHC2,/*33
    MSHC3,/*34
    MSHC4,/*35
    ETH_QOS1,/*36
    ETH_QOS2,/*37
    USB1,/*38
    USB2,/*39
    AI,/*40
    RESERVED2,
    RESERVED3,
    RESERVED4,
    RESERVED5,
    RESERVED6,
    RESERVED7,
    CE1,/*47
    CE2_VCE1,/*48
    CE2_VCE2,/*49
    CE2_VCE3,/*50
    CE2_VCE4,/*51
    CE2_VCE5,/*52
    CE2_VCE6,/*53
    CE2_VCE7,/*54
    CE2_VCE8,/*55
    GPU1_OS1,/*56
    GPU1_OS2,/*57
    GPU1_OS3,/*58
    GPU1_OS4,/*59
    GPU1_OS5,/*60
    GPU1_OS6,/*61
    GPU1_OS7,/*62
    GPU1_OS8,/*63
    GPU2_OS1,/*64
    GPU2_OS2,/*65
    GPU2_OS3,/*66
    GPU2_OS4,/*67
    GPU2_OS5,/*68
    GPU2_OS6,/*69
    GPU2_OS7,/*70
    GPU2_OS8,/*71
    PTB,/*72
    CSSYS,/*73
    RESERVED8,
    RESERVED9,
    RESERVED10,
    RESERVED11,
    RESERVED12,
    RESERVED13,
    PCIE1_0,
    PCIE1_1,
    PCIE1_2,
    PCIE1_3,
    PCIE1_4,
    PCIE1_5,
    PCIE1_6,
    PCIE1_7,
    PCIE1_8,
    PCIE1_9,
    PCIE1_10,
    PCIE1_11,
    PCIE1_12,
    PCIE1_13,
    PCIE1_14,
    PCIE1_15,
    PCIE2_0,
    PCIE2_1,
    PCIE2_2,
    PCIE2_3,
    PCIE2_4,
    PCIE2_5,
    PCIE2_6,
    PCIE2_7,
    RESERVED14,
    RESERVED15,
    RESERVED16,
    RESERVED17,
    RESERVED18,
    RESERVED19,
    RESERVED20,
    RESERVED21,
    RESERVED22,
    RESERVED23,
    RESERVED24,
    RESERVED25,
    RESERVED26,
    RESERVED27,
    RESERVED28,
    RESERVED29,
    RESERVED30,
    RESERVED31,
    RESERVED32,
    RESERVED33,
    RESERVED34,
    RESERVED35,
    RESERVED36,
    RESERVED37
*/
/*---------------------------------------------------
 * define group info, one group can set two master
 *---------------------------------------------------*/
 #define SDRV_PFM_GROUP_INFO_ALL 0 0 0 0
 #define SDRV_PFM_GROUP_INFO_SAF 0 0x7f 0 0x7f
 #define SDRV_PFM_GROUP_INFO_AP1 3 0x7f 3 0x7f
 #define SDRV_PFM_GROUP_INFO_AP2 4 0x7f 4 0x7f
 #define SDRV_PFM_GROUP_INFO_AP1_AP2 3 0x7f 4 0x7f
 #define SDRV_PFM_GROUP_INFO_AP2 4 0x7f 4 0x7f
 #define SDRV_PFM_GROUP_INFO_VDSP 5 0x7f 5 0x7f

 #define SDRV_PFM_GROUP_INFO_DMA1 8 0x7f 8 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA2 9 0x7f 9 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA3 10 0x7f 10 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA4 11 0x7f 11 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA5 12 0x7f 12 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA6 13 0x7f 13 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA7 14 0x7f 14 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA8 15 0x7f 15 0x7f

 #define SDRV_PFM_GROUP_INFO_DMA1_2 8 0x7f 9 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA3_4 10 0x7f 11 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA5_6 12 0x7f 13 0x7f
 #define SDRV_PFM_GROUP_INFO_DMA7_8 14 0x7f 15 0x7f

 #define SDRV_PFM_GROUP_INFO_CSI1 16 0x7f 16 0x7f
 #define SDRV_PFM_GROUP_INFO_CSI2 17 0x7f 17 0x7f
 #define SDRV_PFM_GROUP_INFO_CSI3 18 0x7f 18 0x7f
 #define SDRV_PFM_GROUP_INFO_CSI1_CSI2 16 0x7f 17 0x7f
 #define SDRV_PFM_GROUP_INFO_CSI1_CSI3 16 0x7f 18 0x7f

 #define SDRV_PFM_GROUP_INFO_DC1 19 0x7f 19 0x7f
 #define SDRV_PFM_GROUP_INFO_DC2 20 0x7f 20 0x7f
 #define SDRV_PFM_GROUP_INFO_DC3 21 0x7f 21 0x7f
 #define SDRV_PFM_GROUP_INFO_DC4 22 0x7f 22 0x7f

 #define SDRV_PFM_GROUP_INFO_DP1 23 0x7f 23 0x7f
 #define SDRV_PFM_GROUP_INFO_DP2 24 0x7f 24 0x7f
 #define SDRV_PFM_GROUP_INFO_DP3 25 0x7f 25 0x7f

 #define SDRV_PFM_GROUP_INFO_G2D1 27 0x7f 27 0x7f
 #define SDRV_PFM_GROUP_INFO_G2D2 28 0x7f 28 0x7f
 #define SDRV_PFM_GROUP_INFO_G2D1_2 27 0x7f 28 0x7f
 #define SDRV_PFM_GROUP_INFO_VPU1 29 0x7f 29 0x7f
 #define SDRV_PFM_GROUP_INFO_VPU2 30 0x7f 30 0x7f
 #define SDRV_PFM_GROUP_INFO_MJPEG 31 0x7f 31 0x7f

 #define SDRV_PFM_GROUP_INFO_MSHC1_3 32 0x7f 34 0x7f

 #define SDRV_PFM_GROUP_INFO_ETH1_2 36 0x7f 37 0x7f
 #define SDRV_PFM_GROUP_INFO_USB1_2 38 0x7f 39 0x7f

 #define SDRV_PFM_GROUP_INFO_CE1_2 47 0x7f 48 0x7f

 #define SDRV_PFM_GROUP_INFO_GPU1 56 0x7f 56 0x7f
 #define SDRV_PFM_GROUP_INFO_GPU2 64 0x7f 64 0x7f

#endif /* __DT_BINDINGS_SDRV_PFM_GROUP_H */
