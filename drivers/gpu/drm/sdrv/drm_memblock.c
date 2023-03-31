/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/highmem.h>
//#include <linux/memblock.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include "drm_drv.h"
static void drm_mb_release(struct dma_buf *buf);
static void *drm_mb_kmap_atomic(struct dma_buf *buf,
		unsigned long pgoffset);
static void drm_mb_kunmap_atomic(struct dma_buf *buf,
		unsigned long pgoffset, void *vaddr);
static void *drm_mb_kmap(struct dma_buf *buf, unsigned long pgoffset);
static void drm_mb_kunmap(struct dma_buf *buf, unsigned long pgoffset,
		void *vaddr);
static int drm_mb_mmap(struct dma_buf *buf, struct vm_area_struct *vma);


#ifdef CONFIG_DRM_DMAFD_V2

struct drm_dummy_buffer {
	union {
		struct rb_node node;
		struct list_head list;
	};
	unsigned long flags;
	size_t size;
	struct mutex lock;
	void *vaddr;
	phys_addr_t base;
	struct drm_device *dev;
	struct sg_table *sg_table;
	struct list_head attachments;
};

struct drm_mb_pdata {
	phys_addr_t base;
	size_t size;
	struct drm_dummy_buffer *pbuffer;
};

struct drm_dma_buf_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
};

static struct sg_table *dupp_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sg(table->sgl, sg, table->nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		sg->dma_address = 0;
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void freee_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

static int drm_dma_buf_attach(struct dma_buf *dmabuf, struct device *dev,
			      struct dma_buf_attachment *attachment)
{
	struct drm_dma_buf_attachment *a;
	struct sg_table *table;
	struct drm_mb_pdata *pdata = dmabuf->priv;
	struct drm_dummy_buffer *buffer = pdata->pbuffer;
	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dupp_sg_table(buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = dev;
//	INIT_LIST_HEAD(&a->list);

	attachment->priv = a;
/*
	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);
*/
	return 0;
}

static void drm_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct drm_dma_buf_attachment *a = attachment->priv;
	struct drm_mb_pdata *pdata = dmabuf->priv;
	struct drm_dummy_buffer *buffer = pdata->pbuffer;
	if (!a)
		return -ENOMEM;

	freee_duped_table(a->table);
/*
	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);
*/

	kfree(a);
}

#define DRM_FLAG_CACHED (1 << 0)
void *drm_heap_map_kernel(struct drm_dummy_buffer *buffer)
{
	struct scatterlist *sg;
	int i, j;
	void *vaddr;
	pgprot_t pgprot;
	struct sg_table *table = buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	if (buffer->flags & DRM_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(table->sgl, sg, table->nents, i) {
		int npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		BUG_ON(i >= npages);
		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);


	return vaddr;
}

void drm_heap_unmap_kernel( struct drm_dummy_buffer *buffer)
{
	vunmap(buffer->vaddr);
}

static void *drm_buffer_kmap_get(struct drm_dummy_buffer *buffer)
{
	void *vaddr;

	vaddr = drm_heap_map_kernel(buffer);
	if (WARN_ONCE(!vaddr,
			  "map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	return vaddr;
}

static void drm_buffer_kmap_put(struct drm_dummy_buffer *buffer)
{
		drm_heap_unmap_kernel(buffer);
		buffer->vaddr = NULL;
}


static int drm_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	struct drm_dummy_buffer *buffer = dmabuf->priv;
	void *vaddr;
	struct drm_dma_buf_attachment *a;


	vaddr = drm_buffer_kmap_get(buffer);

	mutex_lock(&buffer->lock);
	list_for_each_entry(a, &buffer->attachments, list) {
		dma_sync_sg_for_cpu(a->dev, a->table->sgl, a->table->nents,
					direction);
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int drm_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction)
{
	struct drm_dummy_buffer *buffer = dmabuf->priv;
	struct drm_dma_buf_attachment *a;

	drm_buffer_kmap_put(buffer);

	mutex_lock(&buffer->lock);
	list_for_each_entry(a, &buffer->attachments, list) {
		dma_sync_sg_for_device(a->dev, a->table->sgl, a->table->nents,
					   direction);
	}
	mutex_unlock(&buffer->lock);


	return 0;
}

static struct sg_table *drm_mb_map(struct dma_buf_attachment *attach,
		enum dma_data_direction direction)
{

	struct drm_dma_buf_attachment *a = attach->priv;
	struct sg_table *table;

	table = a->table;

	if (!dma_map_sg(attach->dev, table->sgl, table->nents,
			direction))
		return ERR_PTR(-ENOMEM);

	return table;

}

static void drm_mb_unmap(struct dma_buf_attachment *attach,
		struct sg_table *table, enum dma_data_direction direction)
{

	dma_unmap_sg(attach->dev, table->sgl, table->nents, direction);
}


static int drm_dummy_allocate(struct drm_dummy_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct sg_table *table;
	int ret;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(buffer->base)), size, 0);
	buffer->sg_table = table;

	return 0;


err_free:
	kfree(table);
	return ret;
}

static void drm_dummy_buffer_free(struct drm_dummy_buffer *buffer)
{

	struct sg_table *table = buffer->sg_table;
	sg_free_table(table);
	kfree(table);
	kfree(buffer);
}


/* this function should only be called while dev->lock is held */
static int drm_dummy_buffer_create(struct drm_device *dev,
						struct drm_mb_pdata *pdata,
					    unsigned long len,
					    unsigned long flags)
{
	struct drm_dummy_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->flags = flags;
	buffer->base = pdata->base;
	buffer->dev = dev;
	buffer->size = len;
	pdata->pbuffer = buffer;

	drm_dummy_allocate(buffer, len,flags);

	if (!buffer->sg_table) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		goto err;
	}

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);

	return 0;


err:
	kfree(buffer);
	return ERR_PTR(ret);
}

static void drm_dummy_buffer_destroy(struct drm_dummy_buffer *buffer)
{
	//drm_buffer_kmap_put(buffer);
	drm_dummy_buffer_free(buffer);
}

struct dma_buf_ops drm_mb_ops = {
	.map_dma_buf = drm_mb_map,
	.unmap_dma_buf = drm_mb_unmap,
	.attach = drm_dma_buf_attach,
	.detach = drm_dma_buf_detatch,
	.begin_cpu_access = drm_dma_buf_begin_cpu_access,
	.end_cpu_access = drm_dma_buf_end_cpu_access,
	.release = drm_mb_release,
	.map_atomic = drm_mb_kmap_atomic,
	.unmap_atomic = drm_mb_kunmap_atomic,
	.map = drm_mb_kmap,
	.unmap = drm_mb_kunmap,
	.mmap = drm_mb_mmap,
};

/**
 * drm_mb_export - export a memblock reserved area as a dma-buf
 *
 * @base: base physical address
 * @size: memblock size
 * @flags: mode flags for the dma-buf's file
 *
 * @base and @size must be page-aligned.
 *
 * Returns a dma-buf on success or ERR_PTR(-errno) on failure.
 */
struct dma_buf *drm_mb_export(struct drm_device *drm, phys_addr_t base, size_t size, int flags)
{
	struct drm_mb_pdata *pdata;
	struct dma_buf *buf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	if (PAGE_ALIGN(base) != base || PAGE_ALIGN(size) != size)
		return ERR_PTR(-EINVAL);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;
	drm_dummy_buffer_create(drm, pdata, size, flags);

	exp_info.ops = &drm_mb_ops;
	exp_info.size = size;
	exp_info.flags = flags;
	exp_info.priv = pdata;

	buf = dma_buf_export(&exp_info);
	if (IS_ERR(buf))
		kfree(pdata);

	return buf;
}
EXPORT_SYMBOL(drm_mb_export);

void drm_mb_free(struct dma_buf *dmabuf)
{
	struct drm_mb_pdata *pdata = dmabuf->priv;
	if(pdata && pdata->pbuffer)
		drm_dummy_buffer_destroy(pdata->pbuffer);
}
EXPORT_SYMBOL(drm_mb_free);

#else
struct drm_mb_pdata {
	phys_addr_t base;
	size_t size;
};

static struct sg_table *drm_mb_map(struct dma_buf_attachment *attach,
		enum dma_data_direction direction)
{
	struct drm_mb_pdata *pdata = attach->dmabuf->priv;
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret < 0)
		goto err_alloc;

	sg_dma_address(table->sgl) = pdata->base;
	sg_dma_len(table->sgl) = pdata->size;

	return table;

err_alloc:
	kfree(table);
	return ERR_PTR(ret);
}

static void drm_mb_unmap(struct dma_buf_attachment *attach,
		struct sg_table *table, enum dma_data_direction direction)
{

	sg_free_table(table);
	kfree(table);
}


struct dma_buf_ops drm_mb_ops = {
	.map_dma_buf = drm_mb_map,
	.unmap_dma_buf = drm_mb_unmap,
	.release = drm_mb_release,
	.map_atomic = drm_mb_kmap_atomic,
	.unmap_atomic = drm_mb_kunmap_atomic,
	.map = drm_mb_kmap,
	.unmap = drm_mb_kunmap,
	.mmap = drm_mb_mmap,
};


/**
 * drm_mb_export - export a memblock reserved area as a dma-buf
 *
 * @base: base physical address
 * @size: memblock size
 * @flags: mode flags for the dma-buf's file
 *
 * @base and @size must be page-aligned.
 *
 * Returns a dma-buf on success or ERR_PTR(-errno) on failure.
 */
struct dma_buf *drm_mb_export(struct drm_device *drm, phys_addr_t base, size_t size, int flags)
{
	struct drm_mb_pdata *pdata;
	struct dma_buf *buf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	if (PAGE_ALIGN(base) != base || PAGE_ALIGN(size) != size)
		return ERR_PTR(-EINVAL);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;

	exp_info.ops = &drm_mb_ops;
	exp_info.size = size;
	exp_info.flags = flags;
	exp_info.priv = pdata;

	buf = dma_buf_export(&exp_info);
	if (IS_ERR(buf))
		kfree(pdata);

	return buf;
}
EXPORT_SYMBOL(drm_mb_export);

void drm_mb_free(struct dma_buf *dmabuf)
{
	return;
}
EXPORT_SYMBOL(drm_mb_free);
#endif


static void drm_mb_release(struct dma_buf *buf)
{
	struct drm_mb_pdata *pdata = buf->priv;

	kfree(pdata);
}

static void *drm_mb_do_kmap(struct dma_buf *buf, unsigned long pgoffset,
		bool atomic)
{
	struct drm_mb_pdata *pdata = buf->priv;
	unsigned long pfn = PFN_DOWN(pdata->base) + pgoffset;
	struct page *page = pfn_to_page(pfn);

	if (atomic)
		return kmap_atomic(page);
	else
		return kmap(page);
}

static void *drm_mb_kmap_atomic(struct dma_buf *buf,
		unsigned long pgoffset)
{
	return drm_mb_do_kmap(buf, pgoffset, true);
}

static void drm_mb_kunmap_atomic(struct dma_buf *buf,
		unsigned long pgoffset, void *vaddr)
{
	kunmap_atomic(vaddr);
}

static void *drm_mb_kmap(struct dma_buf *buf, unsigned long pgoffset)
{
	return drm_mb_do_kmap(buf, pgoffset, false);
}

static void drm_mb_kunmap(struct dma_buf *buf, unsigned long pgoffset,
		void *vaddr)
{
	kunmap(vaddr);
}

static int drm_mb_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	struct drm_mb_pdata *pdata = buf->priv;
	vma->vm_pgoff = 0;

	vma->vm_flags |= (VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_page_prot = pgprot_writecombine(vm_get_page_prot(vma->vm_flags));

	return remap_pfn_range(vma, vma->vm_start, PFN_DOWN(pdata->base),
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

