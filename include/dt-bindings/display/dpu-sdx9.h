/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef __DTS_SDX9_DPU_H
#define __DTS_SDX9_DPU_H

#define LVDS_BASE   0x0 0x30c40000

#define DP1_BASE    0x0 0x30d80000
#define DP2_BASE    0x0 0x30d90000
#define DP3_BASE    0x0 0x30da0000

#define DC1_BASE    0x0 0x30db0000
#define DC2_BASE    0x0 0x30dc0000
#define DC3_BASE    0x0 0x30dd0000
#define DC4_BASE    0x0 0x30de0000
#define DC5_BASE    0x0 0x30df0000

#define DPU_RANGE   0x0 0x10000
#define LVDS_RANGE  0x0 0x50000

#define DC1_IRQ     127
#define DC2_IRQ     129
#define DC3_IRQ     131
#define DC4_IRQ     133
#define DC5_IRQ     135

#define DP1_IRQ     136
#define DP2_IRQ     137
#define DP3_IRQ     138

#endif /*__DTS_SDX9_DPU_H*/
