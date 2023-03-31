/*
* xengpu.h
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

#ifndef __XEN_PUBLIC_IO_GPUIF_H__
#define __XEN_PUBLIC_IO_GPUIF_H__

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>

#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/sched.h>

/*
    Enable to track contexts and buffers
*/
//#define VMM_DEBUG 1

#if defined(VMM_DEBUG)
	#define VMM_DEBUG_PRINT(fmt, ...) \
		pr_err(fmt, __VA_ARGS__)
#else
	#define VMM_DEBUG_PRINT(fmt, ...)
#endif
#define VMM_ERROR_PRINT(fmt, ...) \
	pr_err(fmt, __VA_ARGS__)

/*
 * REQUEST CODES.
 */
#define GPUIF_OP_CREATE_PVZCONNECTION             0
#define GPUIF_OP_DESTROY_PVZCONNECTION            1
#define GPUIF_OP_MAP_DEVPHYSHEAPS                 2
#define GPUIF_OP_UNMAP_DEVPHYSHEAPS               3

#define XEN_GPUIF_STATUS_SUCCESS           0
#define XEN_GPUIF_STATUS_NOT_SUPPORTED     1
#define XEN_GPUIF_STATUS_INVALID_PARAMETER 2
#define XEN_GPUIF_STATUS_BUFFER_OVERFLOW   3

struct xengsx_page_directory {
	grant_ref_t gref_dir_next_page;
	grant_ref_t gref[1]; /* Variable length */
};

/*
 * Map Guest FW heap
 */
struct xengsx_map_dev_heap_req {
	/* page directory contains grefs of the Guest FW heap */
	grant_ref_t gref_directory;
	/* size of the heap, bytes */
	uint64_t buffer_sz;
};

struct gpuif_request {
	uint64_t        id;
	uint8_t  operation;    /* GPUIF_OP_???                         */
	uint8_t  reserved;
	uint32_t ui32FuncID;
	uint32_t ui32OsID;
	uint32_t ui32DevID;
	uint64_t ui64Size;
	uint64_t ui64Addr;
	union {
		struct xengsx_map_dev_heap_req map_dev_heap;
		uint8_t reserved[16];
	} op;
};

struct gpuif_response {
	uint64_t        id;              /* copied from request */
	uint8_t         operation;       /* copied from request */
	uint8_t         reserved;
	int16_t         status;          /* GPUIF_RSP_???       */
	/*below fields are only used for XenVMMCreateDevPhysHeaps */
	uint32_t        eType;
	uint64_t        ui64FwPhysHeapSize;
	uint64_t        ui64FwPhysHeapAddr;
	uint64_t        ui64GpuPhysHeapSize;
	uint64_t        ui64GpuPhysHeapAddr;
	union {
		struct xengsx_map_dev_heap_req map_dev_heap;
		uint8_t reserved[16];
	} op;
};

struct xengpu_call_info {
	struct gpuif_request  *req;
	struct gpuif_response *rsp;
	struct xengpu_call_info *next;
	struct completion call_completed;
};


DEFINE_RING_TYPES(gpuif, struct gpuif_request, struct gpuif_response);


#define XENGPU_REQUEST_INIT(_r) \
	memset((void *)_r, 0, sizeof(struct gpuif_request))

#define GRANT_INVALID_REF	0

int xengpu_xenbus_init(void);
void xengpu_xenbus_fini(void);
#endif /* __XEN_PUBLIC_IO_GPUIF_H__ */

