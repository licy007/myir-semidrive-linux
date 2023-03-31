/*
* xen_pvz_shbuf.h
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

#ifndef __XEN_PVZ_FRONT_SHBUF_H_
#define __XEN_PVZ_FRONT_SHBUF_H_

#include <linux/kernel.h>
#include <xen/grant_table.h>
#include "xengpu.h"
#include "dma_support.h"

struct xen_pvz_shbuf {
	/*
	 * number of references granted for the backend use:
	 *  - for allocated/imported buffers this holds number of grant
	 *    references for the page directory and pages of the buffer
	 */
	int num_grefs;
	grant_ref_t *grefs;
	unsigned char *directory;

	/*
	 * there are 2 ways to provide backing storage for this shared buffer:
	 * either pages or base address. if buffer created from address then we own
	 * the pages and must free those ourselves on closure
	 */
	int num_pages;
	struct page **pages;
	uint64_t addr;

	struct xenbus_device *xb_dev;
};

struct xen_pvz_shbuf_cfg {
	struct xenbus_device *xb_dev;
	size_t size;
	uint64_t addr;
	struct page **pages;
};

struct dev_heap_object {
	uint64_t addr;
	uint64_t vAddr;
	grant_handle_t *map_handles;

	/* these are pages from Xen balloon for allocated Guest FW heap */
	uint32_t num_pages;
	struct page **pages;
};

struct xen_drvinfo {
	struct xenbus_device *xb_dev;
	struct xen_pvz_shbuf *shbuf;
	struct dev_heap_object *heap_obj;
	DMA_ALLOC *psDmaAlloc;
};

struct xen_pvz_shbuf *xen_pvz_shbuf_alloc(
		struct xen_pvz_shbuf_cfg *cfg);

#define xen_page_to_vaddr(page) \
				((phys_addr_t)pfn_to_kaddr(page_to_xen_pfn(page)))


grant_ref_t xen_pvz_shbuf_get_dir_start(struct xen_pvz_shbuf *buf);

void xen_pvz_shbuf_free(struct xen_pvz_shbuf *buf);
int xdrv_map_dev_heap(struct xen_drvinfo *drv_info,
		grant_ref_t gref_directory, uint64_t buffer_sz);
int xdrv_unmap_dev_heap(struct xen_drvinfo *drv_info);
#endif /* __XEN_PVZ_FRONT_SHBUF_H_ */
