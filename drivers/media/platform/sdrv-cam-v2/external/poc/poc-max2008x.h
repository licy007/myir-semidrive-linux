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



int poc_power(struct i2c_client *client,struct gpio_desc *gpiod, u8 addr, int chan, int enable);

#endif

