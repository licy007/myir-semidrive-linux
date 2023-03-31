//--=========================================================================--
//	This file is linux device driver for VPU.
//-----------------------------------------------------------------------------
//
//	This confidential and proprietary software may be used only
//	as authorized by a licensing agreement from Chips&Media Inc.
//	In the event of publication, the following notice is applicable:
//
//	  (C) COPYRIGHT 2006 - 2015	 CHIPS&MEDIA INC.
//			ALL RIGHTS RESERVED
//
//	 The entire notice above must be reproduced on all authorized
//	 copies.
//
//--=========================================================================--

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>

#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY

#define VDI_IOCTL_MAGIC				'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY	_IO(VDI_IOCTL_MAGIC, 0)
#define VDI_IOCTL_FREE_PHYSICALMEMORY		_IO(VDI_IOCTL_MAGIC, 1)
#define VDI_IOCTL_WAIT_INTERRUPT		_IO(VDI_IOCTL_MAGIC, 2)
#define VDI_IOCTL_SET_CLOCK_GATE		_IO(VDI_IOCTL_MAGIC, 3)
#define VDI_IOCTL_RESET				_IO(VDI_IOCTL_MAGIC, 4)
#define VDI_IOCTL_GET_INSTANCE_POOL		_IO(VDI_IOCTL_MAGIC, 5)
#define VDI_IOCTL_GET_COMMON_MEMORY		_IO(VDI_IOCTL_MAGIC, 6)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(VDI_IOCTL_MAGIC, 8)
#define VDI_IOCTL_OPEN_INSTANCE			_IO(VDI_IOCTL_MAGIC, 9)
#define VDI_IOCTL_CLOSE_INSTANCE		_IO(VDI_IOCTL_MAGIC, 10)
#define VDI_IOCTL_GET_INSTANCE_NUM		_IO(VDI_IOCTL_MAGIC, 11)
#define VDI_IOCTL_GET_REGISTER_INFO		_IO(VDI_IOCTL_MAGIC, 12)
#define VDI_IOCTL_DEVICE_MEMORY_MAP		_IO(VDI_IOCTL_MAGIC, 13)
#define VDI_IOCTL_DEVICE_MEMORY_UNMAP		_IO(VDI_IOCTL_MAGIC, 14)
#define VDI_IOCTL_DEVICE_SRAM_CFG		_IO(VDI_IOCTL_MAGIC, 15)
#define VDI_IOCTL_DEVICE_GET_SRAM_INFO		_IO(VDI_IOCTL_MAGIC, 16)
#define VDI_IOCTL_MEMORY_CACHE_REFRESH		_IO(VDI_IOCTL_MAGIC, 17)
#define VDI_IOCTL_VDI_LOCK			_IO(VDI_IOCTL_MAGIC, 18)
#define VDI_IOCTL_VDI_UNLOCK			_IO(VDI_IOCTL_MAGIC, 19)
#define VDI_IOCTL_SEND_CMD_MSG			_IO(VDI_IOCTL_MAGIC, 20)

#define MAX_INST_HANDLE_SIZE			48	/* DO NOT CHANGE THIS VALUE */
#define MAX_NUM_INSTANCE			8
#define PTHREAD_MUTEX_T_HANDLE_SIZE		4

#define READ_VPU_REGISTER(dev, addr)		   \
	(*(volatile unsigned int *)(dev->registers.virt_addr + dev->bit_firmware_info.reg_base_offset + addr))


#define WRITE_VPU_REGISTER(dev, addr, val)		 \
do {							 \
	*(volatile unsigned int *)(dev->registers.virt_addr + dev->bit_firmware_info.reg_base_offset + addr) = (unsigned int)val; \
} while (0)

#ifndef VM_RESERVED /*for kernel after 3.7.0 version*/
#define	 VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

/**
 *@Brief Description of buffer memory allocated by driver
 *@param size
 *@param phys_add phy address in kernel
 *@param base	virt address in kernel
 *@param virt_addr virt address in user space
 *@param dam_addr device memory address
 *@param attachment	 for vpu map dma device
 *@param buf_handle	 dam buffer handle
 */
typedef struct vpudrv_buffer_t {
	uint32_t size;
	uint64_t phys_addr;
	uint64_t base;
	uint64_t virt_addr;
	uint64_t dma_addr;
	uint64_t attachment;
	uint64_t sgt;
	int32_t	 buf_handle;
	int32_t	 data_direction;
} vpudrv_buffer_t;

typedef struct vpu_bit_firmware_info_t {
	uint32_t size;			/* size of this structure*/
	uint32_t core_idx;
	uint64_t reg_base_offset;
	uint16_t bit_code[512];
} vpu_bit_firmware_info_t;

typedef struct vpudrv_inst_info_t {
	uint32_t core_idx;
	uint32_t inst_idx;
	int32_t inst_open_count;	/* for output only*/
} vpudrv_inst_info_t;

typedef struct vpudrv_intr_info_t {
	uint32_t timeout;
	int32_t	 intr_reason;
} vpudrv_intr_info_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
	struct list_head list;
	struct vpudrv_buffer_t vb;
	struct file *filp;
} vpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
	struct list_head list;
	uint32_t inst_idx;
	uint32_t core_idx;
	struct file *filp;
} vpudrv_instanace_list_t;

/*
 *sram info: inter sram; soc sram
 * id  1 inter; 2 soc sram
 * phy address
 * size mem size
 */
struct sram_info {
	uint32_t id;
	uint32_t phy;
	uint32_t size;
};

typedef struct vpudrv_instance_pool_t {
	uint8_t inst_pool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
} vpudrv_instance_pool_t;

typedef enum {
	VPUDRV_MUTEX_VPU,
	VPUDRV_MUTEX_GATE,
	VPUDRV_MUTEX_DISP,
	VPUDRV_MUTEX_MAX,
} vpudrv_mutex_type_t;
#endif
