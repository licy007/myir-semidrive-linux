/*
 * x9-clk.h
 *
 *
 * Copyright(c); 2018 Semidrive
 *
 * Author: Alex Chen <qing.chen@semidrive.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __X9_CLK_H
#define __X9_CLK_H
#define CLK_TYPE_CORE 0
#define CLK_TYPE_BUS 1
#define CLK_TYPE_IP 2
#define CLK_TYPE_IP_POST 3
#define CLK_TYPE_UUU_MUX 4
#define CLK_TYPE_UUU_MUX2 5
#define CLK_TYPE_UUU_DIVIDER 6
#define CLK_TYPE_GATE 7
#define CLK_TYPE_IP_COM 8

#define UUU_M_SHIFT 12
#define UUU_M_WIDTH 4
#define UUU_N_SHIFT 8
#define UUU_N_WIDTH 4
#define UUU_P_SHIFT 4
#define UUU_P_WIDTH 4
#define UUU_Q_SHIFT 0
#define UUU_Q_WIDTH 4

#define PLL_ROOT 0
#define PLL_DIVA 1
#define PLL_DIVB 2
#define PLL_DIVC 3
#define PLL_DIVD 4
#define SYS_CNT_FREQ 3000000
#endif
