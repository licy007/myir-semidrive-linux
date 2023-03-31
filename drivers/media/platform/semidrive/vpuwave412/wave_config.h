//--=========================================================================--
//	This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//	This confidential and proprietary software may be used only
//	as authorized by a licensing agreement from Chips&Media Inc.
//	In the event of publication, the following notice is applicable:
//
//	(C) COPYRIGHT 2006 - 2011	 CHIPS&MEDIA INC.
//			ALL RIGHTS RESERVED
//
//	 The entire notice above must be reproduced on all authorized
//	 copies.
//	This file should be modified by some customers according to their SOC configuration.
//--=========================================================================--

#ifndef _WAVE_CONFIG_H_
#define _WAVE_CONFIG_H_

#include "vpu.h"
#include "ipcc_data.h"

#define WAVE412_CODE			0x4120
#define WAVE_CORE_ID			0x1

#define PRODUCT_CODE_WAVE412(x) (x == WAVE412_CODE)

/************************************************************************/
/* VPU COMMON MEMORY							*/
/************************************************************************/
#define SIZE_COMMON			(4*1024*1024)

/* WAVE4 registers */
#define W4_REG_BASE			0x0000
#define W4_VPU_BUSY_STATUS		(W4_REG_BASE + 0x0070)
#define W4_VPU_INT_REASON_CLEAR		(W4_REG_BASE + 0x0034)
#define W4_VPU_VINT_CLEAR		(W4_REG_BASE + 0x003C)
#define W4_VPU_VPU_INT_STS		(W4_REG_BASE + 0x0044)
#define W4_VPU_INT_REASON		(W4_REG_BASE + 0x004c)
#define W4_RET_SUCCESS			(W4_REG_BASE + 0x0110)
#define W4_RET_FAIL_REASON		(W4_REG_BASE + 0x0114)

/* WAVE4 INIT, WAKEUP */
#define W4_PO_CONF			(W4_REG_BASE + 0x0000)
#define W4_VCPU_CUR_PC			(W4_REG_BASE + 0x0004)
#define W4_VPU_VINT_ENABLE		(W4_REG_BASE + 0x0048)
#define W4_VPU_RESET_REQ		(W4_REG_BASE + 0x0050)
#define W4_VPU_RESET_STATUS		(W4_REG_BASE + 0x0054)
#define W4_VPU_REMAP_CTRL		(W4_REG_BASE + 0x0060)
#define W4_VPU_REMAP_VADDR		(W4_REG_BASE + 0x0064)
#define W4_VPU_REMAP_PADDR		(W4_REG_BASE + 0x0068)
#define W4_VPU_REMAP_CORE_START		(W4_REG_BASE + 0x006C)

#define W4_REMAP_CODE_INDEX		0
enum {
	W4_INT_INIT_VPU		   = 0,
	W4_INT_DEC_PIC_HDR	   = 1,
	W4_INT_FINI_SEQ		   = 2,
	W4_INT_DEC_PIC		   = 3,
	W4_INT_SET_FRAMEBUF	   = 4,
	W4_INT_FLUSH_DEC	   = 5,
	W4_INT_GET_FW_VERSION      = 8,
	W4_INT_QUERY_DEC	   = 9,
	W4_INT_SLEEP_VPU	   = 10,
	W4_INT_WAKEUP_VPU	   = 11,
	W4_INT_CHANGE_INT	   = 12,
	W4_INT_CREATE_INSTANCE	   = 14,
	W4_INT_BSBUF_EMPTY	   = 15,   /*!<< Bitstream buffer empty */
	W4_INT_ENC_SLICE_INT       = 15,
};

#define W4_HW_OPTION			(W4_REG_BASE + 0x0124)
#define W4_CODE_SIZE			(W4_REG_BASE + 0x011C)
/* Note: W4_INIT_CODE_BASE_ADDR should be aligned to 4KB */
#define W4_ADDR_CODE_BASE		(W4_REG_BASE + 0x0118)
#define W4_CODE_PARAM			(W4_REG_BASE + 0x0120)
#define W4_INIT_VPU_TIME_OUT_CNT	(W4_REG_BASE + 0x0134)

/* WAVE4 Wave4BitIssueCommand */
#define W4_CORE_INDEX			(W4_REG_BASE + 0x0104)
#define W4_INST_INDEX			(W4_REG_BASE + 0x0108)
#define W4_COMMAND			(W4_REG_BASE + 0x0100)
#define W4_VPU_HOST_INT_REQ		(W4_REG_BASE + 0x0038)

#define W4_MAX_CODE_BUF_SIZE		(512*1024)
#define W4_CMD_INIT_VPU			(0x0001)
#define W4_CMD_SLEEP_VPU		(0x0400)

/* Product register */
#define VPU_PRODUCT_CODE_REGISTER	(W4_REG_BASE + 0x1044)

#define VPU_PLATFORM_DEVICE_NAME "vpu-wave412"
#define VPU_DEV_NAME "vpuwave"

/* other config add following */
struct wave_dev {
	struct cdev			cdev_wave;
	vpudrv_buffer_t			registers;
	uint32_t			scr_signal;
	int				irq;
	dev_t				device;
	struct sram_info		sram[2];
	struct clk			*clk_bcore;
	struct clk			*clk_core;

	struct class			*class_wave;
	struct device			*dev;
	struct device			*device_wave;

	unsigned long			interrupt_reason;

	uint32_t			open_count;
	struct mutex			wave_mutex;
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

	// vpu virtio
	struct mutex virtio_mutex;
	uint32_t vpu_virtio_enable;
	vpu_ipcc_data_t ipcc_msg;
};

/* virtualization  for wave412 */

/* smmu config for wave412*/

#endif  /* _WAVE_CONFIG_H_ */

