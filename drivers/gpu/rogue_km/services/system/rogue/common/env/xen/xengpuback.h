/*
* xengpuback.h
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

#ifndef XEN_GPU_BACK_H
#define XEN_GPU_BACK_H

#include <xen/xenbus.h>
#include <xen/page.h>

#include "xengpu.h"
#include "xen_pvz_shbuf.h"

struct xen_gpuif {
	/* Unique identifier for this interface. */
	domid_t                 domid;
	uint32_t                ui32DevID;
	unsigned int            handle;
	/* Back pointer to the backend_info. */
	struct backend_info     *be;
	struct gpuif_back_ring  back_ring;
	unsigned int gpuif_irq;
	void * configaddr;
};

struct backend_info {
	struct xenbus_device    *dev;
	struct xen_pvz_shbuf    *shbuf;
	struct dev_heap_object  *heap_obj;
	DMA_ALLOC *psDmaAlloc;

	struct xen_gpuif        *gpuif;
	unsigned                major;
	unsigned                minor;
	char                    *mode;
};

irqreturn_t xen_gpuif_irq_fn(int irq, void *data);
#endif
