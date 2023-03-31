/*
* xen_pvz_shbuf.c
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

#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/xen.h>
#include <xen/xenbus.h>

#include "xengpu.h"
#include "xen_pvz_shbuf.h"
#include "pvrsrv.h"
#include <linux/highmem.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
#include <linux/dma-contiguous.h>
#endif
#include <asm/tlbflush.h>
#include "sysconfig.h"

/*
 * number of grefs a page can hold with respect to the
 * struct xengsx_page_directory header
 */
#define XEN_GSX_NUM_GREFS_PER_PAGE ((PAGE_SIZE - \
	offsetof(struct xengsx_page_directory, gref)) / \
	sizeof(grant_ref_t))

#define xen_page_to_vaddr(page) \
		((phys_addr_t)pfn_to_kaddr(page_to_xen_pfn(page)))


static int __xdrv_map_dev_heap(struct xen_drvinfo *drv_info,
		uint32_t num_grefs, grant_ref_t *grefs)
{
	struct dev_heap_object *heap_obj;
	struct gnttab_map_grant_ref *map_ops = NULL;
	int i, ret=0;
	uint32_t *vaddr = NULL;
	DMA_ALLOC *psDmaAlloc = OSAllocZMem(sizeof(DMA_ALLOC));
	dma_addr_t dev_addr, cpu_addr;
	VMM_ERROR_PRINT("%s:%d, ret = %d\n",
		__FUNCTION__, __LINE__, ret);

	if (!num_grefs || !grefs)
	{
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	heap_obj = kzalloc(sizeof(*heap_obj), GFP_KERNEL);
	if (!heap_obj)
		return -ENOMEM;

	heap_obj->num_pages = num_grefs;
	heap_obj->pages = kcalloc(heap_obj->num_pages, sizeof(*heap_obj->pages),
			GFP_KERNEL);
	if (!heap_obj->pages) {
		ret = -ENOMEM;
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		goto fail;
	}

	heap_obj->map_handles = kcalloc(heap_obj->num_pages,
			sizeof(*heap_obj->map_handles), GFP_KERNEL);
	if (!heap_obj->map_handles) {
		ret = -ENOMEM;
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		goto fail;
	}

	map_ops = kcalloc(heap_obj->num_pages, sizeof(*map_ops), GFP_KERNEL);
	if (!map_ops) {
		ret = -ENOMEM;
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		goto fail;
	}

	{
		int num_pages = heap_obj->num_pages;
		struct page **pages = heap_obj->pages;
		size_t size = num_pages * PAGE_SIZE;
		PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
		struct device * pvrsrvdev = (struct device *)pConfig->pvOSDevice;
		psDmaAlloc->pvOSDevice = pvrsrvdev;

		VMM_DEBUG_PRINT("in __xdrv_map_dev_heap, pvOSDevice = %p", pvrsrvdev);
		psDmaAlloc->ui64Size = size;

		ret = SysDmaAllocMem(psDmaAlloc);
		if (ret != PVRSRV_OK)
		{
			VMM_ERROR_PRINT("Failed to allocate DMA buffer with size %zu\n", size);
			return -ENOMEM;
		}

		ret  = SysDmaRegisterForIoRemapping(psDmaAlloc);
		if (ret != PVRSRV_OK)
		{
			VMM_ERROR_PRINT("Failed to register DMA buffer with size %zu\n", size);
			return -ENOMEM;
		}

		vaddr = (uint32_t *)psDmaAlloc->pvVirtAddr;
		dev_addr = psDmaAlloc->sBusAddr.uiAddr;

		cpu_addr = dev_addr;
		for (i = 0; i < num_pages; i++) {
			pages[i] = pfn_to_page(__phys_to_pfn(cpu_addr));
			clear_highpage(pages[i]);
			cpu_addr += PAGE_SIZE;
		}

		for (i = 0; i < num_pages; i++) {
			SetPagePrivate(pages[i]);
		}

		for (i = 0; i < num_pages; i++) {
			cpu_addr = xen_page_to_vaddr(pages[i]);
			gnttab_set_map_op(&map_ops[i], cpu_addr, GNTMAP_host_map, grefs[i],
				drv_info->xb_dev->otherend_id);
			cpu_addr += PAGE_SIZE;
		}
	}

	VMM_DEBUG_PRINT("Mapping %d pages\n", heap_obj->num_pages);
	ret = gnttab_map_refs(map_ops, NULL, heap_obj->pages, heap_obj->num_pages);
	BUG_ON(ret);

	for (i = 0; i < heap_obj->num_pages; i++) {
		heap_obj->map_handles[i] = map_ops[i].handle;
		if (unlikely(map_ops[i].status != GNTST_okay))
			VMM_ERROR_PRINT("Failed to map page %d with ref %d: %d\n",
					i, grefs[i], map_ops[i].status);
	}

	heap_obj->addr = dev_addr;
	heap_obj->vAddr = (uint64_t)vaddr;
	kfree(map_ops);
	{
		int i;
		for (i = 0; i< 100; i++)
		{
			VMM_DEBUG_PRINT("---- 0x%x ----", vaddr[i]);
		}
	}
	drv_info->heap_obj = heap_obj;
	drv_info->psDmaAlloc = psDmaAlloc;
	VMM_DEBUG_PRINT("ret = %d\n", ret);

	return ret;

fail:
	kfree(map_ops);
	kfree(heap_obj->map_handles);
	kfree(heap_obj->pages);
	kfree(heap_obj);

	return ret;
}

int xdrv_map_dev_heap(struct xen_drvinfo *drv_info,
		grant_ref_t gref_directory, uint64_t buffer_sz)
{
	struct gnttab_map_grant_ref *map_ops = NULL;
	struct gnttab_unmap_grant_ref *unmap_ops = NULL;
	struct xengsx_page_directory *page_dir;
	struct page *page = NULL;
	IMG_UINT32 num_grefs;
	grant_ref_t *grefs;
	phys_addr_t addr;
	int i, ret, num_pages_dir, grefs_left;

	if (drv_info->heap_obj)
	{
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	if (gref_directory == GRANT_INVALID_REF || buffer_sz == 0)
	{
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	num_grefs = DIV_ROUND_UP(buffer_sz, PAGE_SIZE);
	num_pages_dir = DIV_ROUND_UP(num_grefs, XEN_GSX_NUM_GREFS_PER_PAGE);

	grefs = kcalloc(num_grefs, sizeof(grant_ref_t), GFP_KERNEL);
	if (!grefs)
		return -ENOMEM;

	map_ops = kzalloc(sizeof(*map_ops), GFP_KERNEL);
	if (!map_ops) {
		ret = -ENOMEM;
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		goto out;
	}

	unmap_ops = kzalloc(sizeof(*unmap_ops), GFP_KERNEL);
	if (!unmap_ops) {
		ret = -ENOMEM;
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		goto out;
	}

	ret = gnttab_alloc_pages(1, &page);
	if (ret || !page) {
		VMM_ERROR_PRINT("in %s:%d\n", __FUNCTION__, __LINE__);
		goto out;
	}

	addr = xen_page_to_vaddr(page);
	page_dir = (struct xengsx_page_directory *)addr;
	grefs_left = num_grefs;

	VMM_DEBUG_PRINT("Mapping %d page directories\n", num_pages_dir);

	for (i = 0; i < num_pages_dir; i++) {
		int to_copy = XEN_GSX_NUM_GREFS_PER_PAGE;

		if (unlikely(gref_directory == GRANT_INVALID_REF)) {
			ret = -EINVAL;
			VMM_ERROR_PRINT("Got invalid ref for page directory %d\n", i);
			goto out;
		}

		gnttab_set_map_op(map_ops, addr, GNTMAP_host_map, gref_directory,
				drv_info->xb_dev->otherend_id);

		ret = gnttab_map_refs(map_ops, NULL, &page, 1);
		BUG_ON(ret);

		if (unlikely(map_ops->status != GNTST_okay))
			VMM_ERROR_PRINT("Failed to map page directory %d with ref %d: %d\n",
				i, gref_directory, map_ops->status);

		if (to_copy > grefs_left)
			to_copy = grefs_left;

		memcpy(&grefs[XEN_GSX_NUM_GREFS_PER_PAGE * i], page_dir->gref,
				to_copy * sizeof(grant_ref_t));

		gref_directory = page_dir->gref_dir_next_page;
		grefs_left -= to_copy;

		gnttab_set_unmap_op(unmap_ops, addr, GNTMAP_host_map, map_ops->handle);

		ret = gnttab_unmap_refs(unmap_ops, NULL, &page, 1);
		BUG_ON(ret);

		if (unlikely(unmap_ops->status != GNTST_okay))
				VMM_ERROR_PRINT("Failed to unmap page directory %d: %d\n",
						i, unmap_ops->status);
	}

	ret = __xdrv_map_dev_heap(drv_info, num_grefs, grefs);
	if (ret < 0)
		VMM_ERROR_PRINT("Failed to map %d pages: %d\n", num_grefs, ret);

	flush_tlb_all();

out:
	if (page)
		gnttab_free_pages(1, &page);
	kfree(unmap_ops);
	kfree(map_ops);
	kfree(grefs);

	return ret;
}


int xdrv_unmap_dev_heap(struct xen_drvinfo *drv_info)
{
	struct dev_heap_object *heap_obj = drv_info->heap_obj;
	struct gnttab_unmap_grant_ref *unmap_ops;
	int i, ret;

	if (!heap_obj)
		return 0;

	unmap_ops = kcalloc(heap_obj->num_pages, sizeof(*unmap_ops), GFP_KERNEL);
	if (!unmap_ops)
		return -ENOMEM;

	VMM_DEBUG_PRINT("Unmapping %d pages\n", heap_obj->num_pages);

	for (i = 0; i < heap_obj->num_pages; i++) {
		phys_addr_t addr = xen_page_to_vaddr(heap_obj->pages[i]);

		/*
		 * Map the grant entry for access by host CPUs.
		 * If <host_addr> or <dev_bus_addr> is zero, that
		 * field is ignored. If non-zero, they must refer to
		 * a device/host mapping that is tracked by <handle>
		 */
		gnttab_set_unmap_op(&unmap_ops[i], addr, GNTMAP_host_map,
				heap_obj->map_handles[i]);
		unmap_ops[i].dev_bus_addr =
				__pfn_to_phys(__pfn_to_mfn(page_to_pfn(heap_obj->pages[i])));
	}

	ret = gnttab_unmap_refs(unmap_ops, NULL, heap_obj->pages,
			heap_obj->num_pages);
	BUG_ON(ret);

	for (i = 0; i < heap_obj->num_pages; i++) {
		if (unlikely(unmap_ops[i].status != GNTST_okay))
			VMM_ERROR_PRINT("Failed to unmap page %d: %d\n", i, unmap_ops[i].status);
	}

	kfree(unmap_ops);

	gnttab_free_pages(heap_obj->num_pages, heap_obj->pages);

	kfree(heap_obj->map_handles);
	heap_obj->map_handles = NULL;
	kfree(heap_obj->pages);
	heap_obj->pages = NULL;
	kfree(heap_obj);
	drv_info->heap_obj = NULL;

	if(drv_info->psDmaAlloc!=NULL)
	{
		SysDmaDeregisterForIoRemapping(drv_info->psDmaAlloc);
		SysDmaFreeMem(drv_info->psDmaAlloc);
		OSFreeMem(drv_info->psDmaAlloc);
		drv_info->psDmaAlloc = NULL;
	}
	return ret;
}

grant_ref_t xen_pvz_shbuf_get_dir_start(struct xen_pvz_shbuf *buf)
{
	if (!buf->grefs)
		return GRANT_INVALID_REF;

	return buf->grefs[0];
}

void xen_pvz_shbuf_free(struct xen_pvz_shbuf *buf)
{
	if (buf->grefs) {
		int i;

		for (i = 0; i < buf->num_grefs; i++)
			if (buf->grefs[i] != GRANT_INVALID_REF)
				gnttab_end_foreign_access(buf->grefs[i],
					0, 0UL);
		kfree(buf->grefs);
	}
	kfree(buf->directory);
	kfree(buf->pages);
	kfree(buf);
}

static int get_num_pages_dir(struct xen_pvz_shbuf *buf)
{
	/* number of pages the page directory consumes itself */
	return DIV_ROUND_UP(buf->num_pages, XEN_GSX_NUM_GREFS_PER_PAGE);
}

static void calc_num_grefs(struct xen_pvz_shbuf *buf)
{
	/*
	 * number of pages the page directory consumes itself
	 * plus grefs for the buffer pages
	 */
	buf->num_grefs = get_num_pages_dir(buf) + buf->num_pages;
}

static void fill_page_dir(struct xen_pvz_shbuf *buf)
{
	unsigned char *ptr;
	int cur_gref, grefs_left, to_copy, i, num_pages_dir;

	ptr = buf->directory;
	num_pages_dir = get_num_pages_dir(buf);

	/*
	 * while copying, skip grefs at start, they are for pages
	 * granted for the page directory itself
	 */
	cur_gref = num_pages_dir;
	grefs_left = buf->num_pages;
	for (i = 0; i < num_pages_dir; i++) {
		struct xengsx_page_directory *page_dir =
				(struct xengsx_page_directory *)ptr;

		if (grefs_left <= XEN_GSX_NUM_GREFS_PER_PAGE) {
			to_copy = grefs_left;
			page_dir->gref_dir_next_page = GRANT_INVALID_REF;
		} else {
			to_copy = XEN_GSX_NUM_GREFS_PER_PAGE;
			page_dir->gref_dir_next_page = buf->grefs[i + 1];
		}
		memcpy(&page_dir->gref, &buf->grefs[cur_gref],
				to_copy * sizeof(grant_ref_t));
		ptr += PAGE_SIZE;
		grefs_left -= to_copy;
		cur_gref += to_copy;
	}
}

static int grant_refs_for_buffer(struct xen_pvz_shbuf *buf,
		grant_ref_t *priv_gref_head, int gref_idx)
{
	int i, cur_ref, otherend_id;

	otherend_id = buf->xb_dev->otherend_id;
	for (i = 0; i < buf->num_pages; i++) {
		cur_ref = gnttab_claim_grant_reference(priv_gref_head);
		if (cur_ref < 0)
			return cur_ref;
		gnttab_grant_foreign_access_ref(cur_ref, otherend_id,
				xen_page_to_gfn(buf->pages[i]), 0);
		buf->grefs[gref_idx++] = cur_ref;
	}
	return 0;
}

static int grant_references(struct xen_pvz_shbuf *buf)
{
	grant_ref_t priv_gref_head;
	int ret, i, j, cur_ref;
	int otherend_id, num_pages_dir;

	ret = gnttab_alloc_grant_references(buf->num_grefs, &priv_gref_head);
	if (ret < 0) {
		VMM_ERROR_PRINT("%s:Cannot allocate grant references\n",__FUNCTION__);
		return ret;
	}
	otherend_id = buf->xb_dev->otherend_id;
	j = 0;
	num_pages_dir = get_num_pages_dir(buf);
	for (i = 0; i < num_pages_dir; i++) {
		unsigned long frame;

		cur_ref = gnttab_claim_grant_reference(&priv_gref_head);
		if (cur_ref < 0)
			return cur_ref;

		frame = xen_page_to_gfn(virt_to_page(buf->directory +
				PAGE_SIZE * i));
		gnttab_grant_foreign_access_ref(cur_ref, otherend_id,
				frame, 0);
		buf->grefs[j++] = cur_ref;
	}

	ret = grant_refs_for_buffer(buf, &priv_gref_head, j);
	if (ret)
		return ret;

	gnttab_free_grant_references(priv_gref_head);
	return 0;
}

static int alloc_storage(struct xen_pvz_shbuf *buf)
{
	if (buf->addr) {
		int i;

		buf->pages = kcalloc(buf->num_pages, sizeof(*buf->pages), GFP_KERNEL);
		if (!buf->pages)
			return -ENOMEM;

		for (i = 0; i < buf->num_pages; i++)
			buf->pages[i] = phys_to_page(buf->addr + i * PAGE_SIZE);
	}

	buf->grefs = kcalloc(buf->num_grefs, sizeof(*buf->grefs), GFP_KERNEL);
	if (!buf->grefs)
		return -ENOMEM;

	buf->directory = kcalloc(get_num_pages_dir(buf), PAGE_SIZE, GFP_KERNEL);
	if (!buf->directory)
		return -ENOMEM;

	return 0;
}

struct xen_pvz_shbuf *xen_pvz_shbuf_alloc(
		struct xen_pvz_shbuf_cfg *cfg)
{
	struct xen_pvz_shbuf *buf;
	int ret;

	/* either pages or base address, not both */
	BUG_ON(cfg->pages && cfg->addr);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->xb_dev = cfg->xb_dev;
	buf->num_pages = DIV_ROUND_UP(cfg->size, PAGE_SIZE);
	buf->pages = cfg->pages;
	buf->addr = cfg->addr;

	calc_num_grefs(buf);

	ret = alloc_storage(buf);
	if (ret)
		goto fail;

	ret = grant_references(buf);
	if (ret)
		goto fail;

	fill_page_dir(buf);

	return buf;

fail:
	xen_pvz_shbuf_free(buf);
	return ERR_PTR(ret);
}
