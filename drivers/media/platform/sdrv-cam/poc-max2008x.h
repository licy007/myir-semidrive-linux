/*
 * poc-max2008x.h
 *
 * Semidrive platform csi header file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef POC_MAX2008X_H
#define POC_MAX2008X_H

extern int poc_mdeser0_power(int chan, int enable, u8 reg, u8 val);
extern int poc_mdeser1_power(int chan, int enable, int reg, u32 val);
extern int poc_r_mdeser0_power(int chan, int enable, u8 reg, u8 val);
extern int poc_r_mdeser1_power(int chan, int enable, int reg, u32 val);
extern int register_sideb_poc_driver(void);
#endif

