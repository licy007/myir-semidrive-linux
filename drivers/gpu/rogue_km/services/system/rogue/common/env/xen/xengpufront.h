/*
* xengpufront.h
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System Configuration functions.
*
* Revision History:
* -----------------
* 011, 11/10/2020 Lili create this file
*/

#include "xen_pvz_shbuf.h"

#define GPU_MAX_RING_SIZE	\
	__CONST_RING_SIZE(gpuif, XEN_PAGE_SIZE * XENBUS_MAX_RING_GRANTS)
#define GPU_RING_SIZE	\
	__CONST_RING_SIZE(gpuif, XEN_PAGE_SIZE)
#define GPU_WAIT_RSP_TIMEOUT	MAX_SCHEDULE_TIMEOUT


struct gpufront_info {
	spinlock_t ring_lock;
	struct gpuif_front_ring front_ring;
	grant_ref_t ring_ref;
	int evtchn;
	int irq;
};

struct xengpu_info {
	struct xenbus_device    *xb_dev;
	struct xen_pvz_shbuf    *shbuf;
	struct dev_heap_object  *heap_obj;
	DMA_ALLOC *psDmaAlloc;

	struct gpufront_info    *rinfo;
	struct xengpu_call_info *call_head;
	struct completion pvz_completion;
	spinlock_t calls_lock;
	bool inited;
	bool mapped;
};


int gsx_front_map(uint32_t func_id, uint32_t dev_id, uint32_t os_id,
	uint64_t size, uint64_t addr);
int gsx_front_unmap(uint32_t func_id, uint32_t dev_id, uint32_t os_id);

int xengpu_init(void);
void xengpu_exit(void);
int xengpu_do_request(struct gpuif_request *pReq, struct gpuif_response **rsp);
DMA_ALLOC * getDMAallocinfo(void);

