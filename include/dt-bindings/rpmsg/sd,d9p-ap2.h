 /*
 *
 * Copyright(c) 2020 Semidrive
 * *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SDRV_SHM_D9P_AP2_H__
#define __SDRV_SHM_D9P_AP2_H__

/*
 * the MEMBASE is defined in image_cfg.h of related project.
 * <dt-bindings/memmap/x9_high-plus/projects/[prj]/image_cfg.h>
 */
#define RPMSG_MEM_CONV(addr)	(addr+0x10000000)
#define RPMSG0_MEM_HEAD_BASE	RPMSG_MEM_CONV(SAF_AP2_MEMBASE)
#define RPMSG0_MEM_HEAD_SIZE	0x10000
#define RPMSG0_MEM_POOL_BASE	(RPMSG0_MEM_HEAD_BASE+RPMSG0_MEM_HEAD_SIZE)
#define RPMSG0_MEM_POOL_SIZE	0xf0000

#define RPMSG1_MEM_HEAD_BASE	RPMSG_MEM_CONV(SEC_AP2_MEMBASE)
#define RPMSG1_MEM_HEAD_SIZE	0x10000
#define RPMSG1_MEM_POOL_BASE	(RPMSG1_MEM_HEAD_BASE+RPMSG1_MEM_HEAD_SIZE)
#define RPMSG1_MEM_POOL_SIZE	0xf0000

#define RPMSG3_MEM_HEAD_BASE	AP1_AP2_MEMBASE
#define RPMSG3_MEM_HEAD_SIZE	0x10000
#define RPMSG3_MEM_POOL_BASE	(RPMSG3_MEM_HEAD_BASE+RPMSG3_MEM_HEAD_SIZE)
#define RPMSG3_MEM_POOL_SIZE	0xf0000

#endif //__SDRV_SHM_X9HP_AP2_H__
