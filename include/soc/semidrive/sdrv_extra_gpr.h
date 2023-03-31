/*
 * sdrv_extra_gpr.h
 *
 * Copyright (c) 2022 SemiDrive Semiconductor.
 * All rights reserved.
 *
 * Description:
 *
 * Revision History:
 * -----------------
 */

#ifndef _SDRV_EXTRA_GPR_H_
#define _SDRV_EXTRA_GPR_H_
#include <linux/types.h>

#define SDRV_EXTRA_GPR_NAME_SIZE      4
#define SDRV_EXTRA_GPR_MAX_COUNT      63
#define SDRV_EXTRA_GPR_MAGIC          0x45475052 /* EGPR */
#define SDRV_EXTRA_GPR_TOTAL_SIZE     512
/* addr magic is used to verify the reg data is the addr base of extra gpr */
#define SDRV_EXTRA_GPR_ADDR_MAGIC     0xa
#define SDRV_EXTRA_GPR_ADDR_MASK      0xf

#define SDRV_EXTRA_GPR_OK                        0x00
#define SDRV_EXTRA_GPR_ERROR_INVALID_NAME        0x01
#define SDRV_EXTRA_GPR_ERROR_INVALID_REG_DATA    0x02
#define SDRV_EXTRA_GPR_ERROR_INVALID_ADDR_BASE   0x03
#define SDRV_EXTRA_GPR_ERROR_ADDR_CONVERT_FAIL   0x04
#define SDRV_EXTRA_GPR_ERROR_NOT_INITIALIZED     0x05
#define SDRV_EXTRA_GPR_ERROR_FIND_KEY_FAIL       0x06
#define SDRV_EXTRA_GPR_ERROR_FULL                0x07

#define SDRV_EXTRA_GPR_NAME_HWID      "hwid"

/* use for ramdump */
#define CRASH_ELFCORE_HIGH_ADDR		"cdhi"
#define CRASH_ELFCORE_LOW_ADDR		"cdlo"

/* Please make sure that your extra gpr name length
 * is not over than SDRV_EXTRA_GPR_NAME_SIZE
 */
u32 sdrv_extra_gpr_get(const char *name, u32 *value);

u32 sdrv_extra_gpr_set(const char *name, u32 value);

#endif
