/*
 * jpu.h
 *
 * Copyright (C) 2020 Semidrive Technology Co., Ltd.
 *
 * Description: header file
 *
 * Revision Histrory:
 * -----------------
 * 1.1, 20/11/2020  chentianming <tianming.chen@semidrive.com> upload & modify this file
 *
 */

#ifndef __JPU_DRV_H__
#define __JPU_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>

#define JDI_IOCTL_MAGIC  'J'
#define JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY          _IO(JDI_IOCTL_MAGIC, 0)
#define JDI_IOCTL_FREE_PHYSICALMEMORY               _IO(JDI_IOCTL_MAGIC, 1)
#define JDI_IOCTL_WAIT_INTERRUPT                    _IO(JDI_IOCTL_MAGIC, 2)
#define JDI_IOCTL_SET_CLOCK_GATE                    _IO(JDI_IOCTL_MAGIC, 3)
#define JDI_IOCTL_RESET                             _IO(JDI_IOCTL_MAGIC, 4)
#define JDI_IOCTL_GET_INSTANCE_POOL                 _IO(JDI_IOCTL_MAGIC, 5)
#define JDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO    _IO(JDI_IOCTL_MAGIC, 6)
#define JDI_IOCTL_GET_REGISTER_INFO                 _IO(JDI_IOCTL_MAGIC, 7)
#define JDI_IOCTL_OPEN_INSTANCE                     _IO(JDI_IOCTL_MAGIC, 8)
#define JDI_IOCTL_CLOSE_INSTANCE                    _IO(JDI_IOCTL_MAGIC, 9)
#define JDI_IOCTL_GET_INSTANCE_NUM                  _IO(JDI_IOCTL_MAGIC, 10)
#define JDI_IOCTL_DEVICE_MEMORY_MAP                 _IO(JDI_IOCTL_MAGIC, 11)
#define JDI_IOCTL_DEVICE_MEMORY_UNMAP               _IO(JDI_IOCTL_MAGIC, 12)
#define JDI_IOCTL_MEMORY_CACHE_REFRESH              _IO(JDI_IOCTL_MAGIC, 13)

typedef struct jpudrv_buffer_t {
	uint32_t	size;
	uint64_t	phys_addr;
	uint64_t	base;			/* kernel logical address in use kernel */
	uint64_t	virt_addr;		/* virtual user space address */
	uint64_t	dma_addr;
	uint64_t	buf_handle;
	uint64_t	attachment;
	uint64_t	sgt;
	uint64_t	data_direction;
	uint64_t	cached;
	uint64_t	pages;
} jpudrv_buffer_t;

typedef struct jpudrv_inst_info_t {
	uint32_t	inst_idx;
	uint32_t	inst_open_count;	/* for output only*/
} jpudrv_inst_info_t;

typedef struct jpudrv_intr_info_t {
	uint32_t	timeout;
	uint32_t	intr_reason;
	uint32_t	inst_idx;
} jpudrv_intr_info_t;

#endif  // __JPU_DRV_H__