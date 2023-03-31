/*
 * sdrv_scr.c
 *
 * Copyright(c) 2020 Semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _SOC_SEMIDRIVE_SDRV_SCR_H
#define _SOC_SEMIDRIVE_SDRV_SCR_H

#include <linux/types.h>
#include <dt-bindings/soc/sdrv,scr.h>

int sdrv_scr_is_locked(uint32_t scr, uint32_t _signal, bool *locked);
int sdrv_scr_lock(uint32_t scr, uint32_t _signal);
int sdrv_scr_get(uint32_t scr, uint32_t _signal, uint32_t *val);
int sdrv_scr_set(uint32_t scr, uint32_t _signal, uint32_t val);

#endif /* _SOC_SEMIDRIVE_SDRV_SCR_H */
