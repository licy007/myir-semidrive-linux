//--=========================================================================--
//	This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//		 This confidential and proprietary software may be used only
//	   as authorized by a licensing agreement from Chips&Media Inc.
//	   In the event of publication, the following notice is applicable:
//
//			  (C) COPYRIGHT 2006 - 2011	 CHIPS&MEDIA INC.
//						ALL RIGHTS RESERVED
//
//		 The entire notice above must be reproduced on all authorized
//		 copies.
//		This file should be modified by some customers according to their SOC configuration.
//--=========================================================================--

#ifndef _CODA_CONFIG_H_
#define _CODA_CONFIG_H_

#include "vpu.h"
#include "ipcc_data.h"

#define CODA980_CODE				0x9800
#define CODA_CORE_ID				0x0

#define PRODUCT_CODE_CODA988(x) (x == CODA980_CODE)

/************************************************************************/
/* VPU COMMON MEMORY							*/
/************************************************************************/
#define COMMAND_QUEUE_DEPTH			4
#define ONE_TASKBUF_SIZE_FOR_CQ			(8*1024*1024)	/* upto 8Kx4K, need 8Mbyte per task*/
#define SIZE_COMMON				(2*1024*1024)


#define BIT_BASE				 0x0000
#define BIT_CODE_RUN				(BIT_BASE + 0x000)
#define BIT_CODE_DOWN				(BIT_BASE + 0x004)
#define BIT_INT_CLEAR				(BIT_BASE + 0x00C)
#define BIT_INT_STS				(BIT_BASE + 0x010)
#define BIT_CODE_RESET				(BIT_BASE + 0x014)
#define BIT_INT_REASON				(BIT_BASE + 0x174)
#define BIT_BUSY_FLAG				(BIT_BASE + 0x160)
/* Product register */
#define VPU_PRODUCT_CODE_REGISTER		(BIT_BASE + 0x1044)

#define VPU_PLATFORM_DEVICE_NAME "vpu-coda988"
#define VPU_DEV_NAME "vpucoda"

/* other config add following */
struct coda_dev {
	struct cdev			cdev_coda;
	vpudrv_buffer_t			registers;
	uint32_t			scr_signal;
	int				irq;
	dev_t				device;
	struct sram_info		sram[2];
	struct clk			*clk_core;

	struct class			*class_coda;
	struct device			*dev;
	struct device			*device_coda;

	unsigned long			interrupt_reason;

	uint32_t			open_count;
	struct mutex			coda_mutex;
	spinlock_t			irqlock;
	int				instance_ref_count;
	vpu_bit_firmware_info_t		bit_firmware_info;
	vpudrv_buffer_t			instance_pool;
	vpudrv_buffer_t			common_memory;

	struct semaphore		vpu_sema;
	struct semaphore		disp_sema;
	struct semaphore		gate_sema;

	struct list_head		vbp_list;
	struct list_head		inst_list;
	int				interrupt_flag;
	wait_queue_head_t		interrupt_wait_q;
	uint32_t			vpu_reg_store[64];

	// vpu virtio
	struct mutex virtio_mutex;
	uint32_t vpu_virtio_enable;
	vpu_ipcc_data_t ipcc_msg;
};

/* virtualization  for coda988 */

/* smmu config for coda988 */

#endif  /* _CODA_CONFIG_H_ */

