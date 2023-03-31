 /*
 *
 * Copyright(c) 2020 Semidrive
 * *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SDRV_SHM_X9H_DEFAULT_H__
#define __SDRV_SHM_X9H_DEFAULT_H__

/*
 * the MEMBASE is defined in image_cfg.h of related project.
 * <dt-bindings/memmap/x9_high/projects/[prj]/image_cfg.h>
 */
#define RPMSG_MEM_CONV(addr)	(addr+0x10000000)
#define RPMSG0_MEM_HEAD_BASE	RPMSG_MEM_CONV(SAF_AP1_MEMBASE)
#define RPMSG0_MEM_HEAD_SIZE	0x10000
#define RPMSG0_MEM_POOL_BASE	(RPMSG0_MEM_HEAD_BASE+RPMSG0_MEM_HEAD_SIZE)
#define RPMSG0_MEM_POOL_SIZE	0xf0000

#define RPMSG1_MEM_HEAD_BASE	RPMSG_MEM_CONV(SEC_AP1_MEMBASE)
#define RPMSG1_MEM_HEAD_SIZE	0x10000
#define RPMSG1_MEM_POOL_BASE	(RPMSG1_MEM_HEAD_BASE+RPMSG1_MEM_HEAD_SIZE)
#define RPMSG1_MEM_POOL_SIZE	0xf0000



#define MBOX_INDEX_CAMERA_CSI0	0x80
#define MBOX_INDEX_CAMERA_CSI0_PARAM	0x8000
#define MBOX_INDEX_CAMERA_CSI0_0	0x81
#define MBOX_INDEX_CAMERA_CSI0_0_PARAM	0x8100
#define MBOX_INDEX_CAMERA_CSI0_1	0x82
#define MBOX_INDEX_CAMERA_CSI0_1_PARAM	0x8200
#define MBOX_INDEX_CAMERA_CSI0_2	0x83
#define MBOX_INDEX_CAMERA_CSI0_2_PARAM	0x8300
#define MBOX_INDEX_CAMERA_CSI0_3	0x84
#define MBOX_INDEX_CAMERA_CSI0_3_PARAM	0x8400

#define MBOX_INDEX_CAMERA_CSI1	0x86
#define MBOX_INDEX_CAMERA_CSI1_PARAM	0x8600
#define MBOX_INDEX_CAMERA_CSI1_0	0x87
#define MBOX_INDEX_CAMERA_CSI1_0_PARAM	0x8700
#define MBOX_INDEX_CAMERA_CSI1_1	0x88
#define MBOX_INDEX_CAMERA_CSI1_1_PARAM	0x8800
#define MBOX_INDEX_CAMERA_CSI1_2	0x89
#define MBOX_INDEX_CAMERA_CSI1_2_PARAM	0x8900
#define MBOX_INDEX_CAMERA_CSI1_3	0x8a
#define MBOX_INDEX_CAMERA_CSI1_3_PARAM	0x8a00

#define MBOX_INDEX_CAMERA_CSI2	0x8c
#define MBOX_INDEX_CAMERA_CSI2_PARAM	0x8c00
#define MBOX_INDEX_CAMERA_CSI2_0	0x8d
#define MBOX_INDEX_CAMERA_CSI2_0_PARAM	0x8d00
#define MBOX_INDEX_CAMERA_CSI2_1	0x8e
#define MBOX_INDEX_CAMERA_CSI2_1_PARAM	0x8e00


#define MBOX_INDEX_AUDIO_RPC_SVR 0xd0
#define MBOX_INDEX_AUDIO_RPC_SVR_PARAM 0xd000


#endif //__SDRV_SHM_X9H_DEFAULT_H__
