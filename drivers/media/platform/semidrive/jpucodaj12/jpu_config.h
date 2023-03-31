#ifndef __JPU_CONFIG_H__
#define __JPU_CONFIG_H__

#include "jpu.h"

#define JPU_PLATFORM_DEVICE_NAME    "semidrive_jpu"
#define JPU_DEV_NAME                "jpucoda"
#define MAX_NUM_JPU_CORE            1
#define MAX_NUM_INSTANCE            4
#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
#define MAX_INST_HANDLE_SIZE        48

/* driver disable and enable IRQ whenever interrupt asserted. */
//#define JPU_IRQ_CONTROL

#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define NPT_BASE                        0x0000
#define NPT_REG_SIZE                    0x300
#define NPT_PROC_BASE                   (NPT_BASE + (MAX_NUM_INSTANCE * NPT_REG_SIZE))
#define MJPEG_PIC_STATUS_REG(_inst_no)  (NPT_BASE + (_inst_no*NPT_REG_SIZE) + 0x004)
#define MJPEG_INTR_MASK_REG             (NPT_BASE + 0x0C0)
#define MJPEG_INST_CTRL_START_REG       (NPT_PROC_BASE + 0x000)
#define ReadJpuRegister(s_jpu_drv_context, addr)           (*(volatile unsigned int *)(s_jpu_drv_context->jpu_register.virt_addr + addr))
#define WriteJpuRegister(s_jpu_drv_context, addr, val)     (*(volatile unsigned int *)(s_jpu_drv_context->jpu_register.virt_addr + addr) = (unsigned int)val)

/* to track the allocated memory buffer */
struct jpudrv_buffer_pool_t {
	struct list_head        list;
	struct jpudrv_buffer_t  jb;
	struct file            *filp;
};

/* to track the instance index and buffer in instance pool */
struct jpudrv_instance_list_t {
	struct list_head    list;
	unsigned long       inst_idx;
	struct file        *filp;
}jpudrv_instance_list_t;

struct jpudrv_instance_pool_t {
	unsigned char codec_inst_pool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
};

struct jpu_clock {
	struct clk *core_aclk;
	struct clk *core_cclk;
	uint32_t clk_reference;
};

struct jpu_drv_context_t {
	uint32_t open_count;  /* file open count */
	int jpu_major;
	int jpu_open_ref_count; /* instances open count */
	int jpu_irq;
	bool is_jpu_reset;
	int interrupt_flag[MAX_NUM_INSTANCE];
	uint32_t interrupt_reason[MAX_NUM_INSTANCE];
	jpudrv_buffer_t instance_pool;
	jpudrv_buffer_t jpu_register;
	wait_queue_head_t interrupt_wait_q[MAX_NUM_INSTANCE];
	struct fasync_struct *async_queue;
	struct list_head jbp_head;
	struct list_head inst_list_head;
	struct device  *jpu_device;
	struct cdev jpu_cdev;
	struct jpu_clock jpu_clk;
	struct class  *jpu_class;
	struct device *jpu_class_device;
	struct mutex jpu_mutex;
	struct cma *cma;
};

#endif