 /*
 *
 * Copyright(c) 2020 Semidrive
 * *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SDRV_SHM_V9752_DEFAULT_H__
#define __SDRV_SHM_V9752_DEFAULT_H__

#define RPMSG0_MEM_HEAD_BASE	0x48E00000
#define RPMSG0_MEM_HEAD_SIZE	0x10000
#define RPMSG0_MEM_POOL_BASE	0x48E10000
#define RPMSG0_MEM_POOL_SIZE	0xf0000

#define RPMSG1_MEM_HEAD_BASE	0x48D00000
#define RPMSG1_MEM_HEAD_SIZE	0x10000
#define RPMSG1_MEM_POOL_BASE	0x48D10000
#define RPMSG1_MEM_POOL_SIZE	0xf0000



#define MBOX_INDEX_CAMERA_AVM	0x80
#define MBOX_INDEX_CAMERA_AVM_PARAM	0x8000
#define MBOX_INDEX_CAMERA_DMS	0x81
#define MBOX_INDEX_CAMERA_DMS_PARAM	0x8100


#endif //__SDRV_SHM_V9752_DEFAULT_H__

