 /*
 * rstgen.h
 *
 * Copyright(c); 2020 Semidrive
 *
 * Author: Yujin <jin.yu@semidrive.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RSTGEN_H__
#define __RSTGEN_H__

#define RSTGEN_MODULE_OSPI2 0
#define RSTGEN_MODULE_I2S_SC3 1
#define RSTGEN_MODULE_I2S_SC4 2
#define RSTGEN_MODULE_I2S_SC5 3
#define RSTGEN_MODULE_I2S_SC6 4
#define RSTGEN_MODULE_I2S_SC7 5
#define RSTGEN_MODULE_I2S_SC8 6
#define RSTGEN_MODULE_I2S_MC1 7
#define RSTGEN_MODULE_I2S_MC2 8
#define RSTGEN_MODULE_CANFD5 9
#define RSTGEN_MODULE_CANFD6 10
#define RSTGEN_MODULE_CANFD7 11
#define RSTGEN_MODULE_CANFD8 12
#define RSTGEN_MODULE_ETH2 13
#define RSTGEN_MODULE_MSHC1 14
#define RSTGEN_MODULE_MSHC2 15
#define RSTGEN_MODULE_MSHC3 16
#define RSTGEN_MODULE_MSHC4 17
#define RSTGEN_MODULE_ADSP 18
#define RSTGEN_MODULE_GIC2 19
#define RSTGEN_MODULE_GIC3 20
#define RSTGEN_MODULE_CPU1_CORE0_WARM 21
#define RSTGEN_MODULE_CPU1_CORE1_WARM 22
#define RSTGEN_MODULE_CPU1_CORE2_WARM 23
#define RSTGEN_MODULE_CPU1_CORE3_WARM 24
#define RSTGEN_MODULE_CPU1_CORE4_WARM 25
#define RSTGEN_MODULE_CPU1_CORE5_WARM 26
#define RSTGEN_MODULE_CPU1_SCU_WARM 27
#define RSTGEN_MODULE_DDR_SS 29
#define RSTGEN_MODULE_DDR_SW1 30
#define RSTGEN_MODULE_DDR_SW2 31
#define RSTGEN_MODULE_DDR_SW3 32
#define RSTGEN_MODULE_GIC4 33
#define RSTGEN_MODULE_GIC5 34
#define RSTGEN_MODULE_CSSYS_TRESET_N 36
#define RSTGEN_MODULE_NNA 37
#define RSTGEN_MODULE_VDSP_DRESET 38
#define RSTGEN_MODULE_VPU1 39
#define RSTGEN_MODULE_VPU2 40
#define RSTGEN_MODULE_MJPEG 41
#define RSTGEN_MODULE_CPU1_SS 45
#define RSTGEN_MODULE_CPU2_SS 46
#define RSTGEN_MODULE_PCIE1 47
#define RSTGEN_MODULE_PCIE2 48
#define RSTGEN_MODULE_MIPI_CSI1 56
#define RSTGEN_MODULE_MIPI_CSI2 57
#define RSTGEN_MODULE_MIPI_CSI3 58
#define RSTGEN_MODULE_MIPI_DSI1 59
#define RSTGEN_MODULE_MIPI_DSI2 60
#define RSTGEN_MODULE_DC1 61
#define RSTGEN_MODULE_DC2 62
#define RSTGEN_MODULE_DC3 63
#define RSTGEN_MODULE_DC4 64
#define RSTGEN_MODULE_DC5 65
#define RSTGEN_MODULE_DP1 66
#define RSTGEN_MODULE_DP2 67
#define RSTGEN_MODULE_DP3 68
#define RSTGEN_MODULE_LVDS_SS 69
#define RSTGEN_MODULE_CSI1 70
#define RSTGEN_MODULE_CSI2 71
#define RSTGEN_MODULE_CSI3 72
#define RSTGEN_MODULE_DISP_MUX 73
#define RSTGEN_MODULE_G2D1 74
#define RSTGEN_MODULE_G2D2 75
#define RSTGEN_MODULE_GPU1_CORE 80
#define RSTGEN_MODULE_GPU1_SS 81
#define RSTGEN_MODULE_GPU2_CORE 82
#define RSTGEN_MODULE_GPU2_SS 83
#define RSTGEN_MODULE_DBG_REG 89
#define RSTGEN_MODULE_CANFD9 90
#define RSTGEN_MODULE_CANFD10 91
#define RSTGEN_MODULE_CANFD11 92
#define RSTGEN_MODULE_CANFD12 93
#define RSTGEN_MODULE_CANFD13 94
#define RSTGEN_MODULE_CANFD14 95
#define RSTGEN_MODULE_CANFD15 96
#define RSTGEN_MODULE_CANFD16 97
#define RSTGEN_MODULE_CANFD17 98
#define RSTGEN_MODULE_CANFD18 99
#define RSTGEN_MODULE_CANFD19 100
#define RSTGEN_MODULE_CANFD20 101

#define RSTGEN_CORE_BASE 1000
#define RSTGEN_CORE_VDSP     1000
#define RSTGEN_CORE_SEC      1001
#define RSTGEN_CORE_MP       1002
#define RSTGEN_CORE_CPU1_ALL 1003
#define RSTGEN_CORE_CPU2     1004
#define RSTGEN_CORE_ADSP     1005
#define RSTGEN_MAX 1006

#endif
