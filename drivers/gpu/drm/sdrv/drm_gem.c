/*
 * kunlun_drm_gem.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>
#include <linux/iommu.h>
#include <linux/dma-buf.h>

#include "drm_drv.h"
#include "drm_gem.h"

static int	vm_cache = 1;
module_param(vm_cache, int, 0664);

static int kunlun_gem_iommu_map(struct kunlun_gem_object *kg_obj)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	int prot = IOMMU_READ | IOMMU_WRITE;
	size_t ret;

	if(kg_obj->mm)
		return -EBUSY;

	kg_obj->mm = kzalloc(sizeof(*kg_obj->mm), GFP_KERNEL);
	if(!kg_obj->mm)
		return -ENOMEM;

	mutex_lock(&kdrm->mm_lock);

	ret = drm_mm_insert_node_generic(kdrm->mm, kg_obj->mm,
			gem_obj->size, PAGE_SIZE, 0, DRM_MM_INSERT_BEST);
	if(ret < 0) {
		DRM_DEV_ERROR(drm->dev, "out of I/O virtual memory:%zd\n", ret);
		goto err_free_mm;
	}

	kg_obj->paddr = kg_obj->mm->start;

	ret = iommu_map_sg(kdrm->domain, kg_obj->paddr,
				kg_obj->sgt->sgl, kg_obj->sgt->nents, prot);
	if((ret < 0) || (ret < gem_obj->size)) {
		DRM_DEV_ERROR(drm->dev, "failed to map buffer: %zd,request: %zd\n",
				ret, gem_obj->size);
		ret= -ENOMEM;
		goto err_remove_node;
	}

	kg_obj->size = ret;

#if 0
	kg_obj->vaddr = vmap(kg_obj->pages, kg_obj->num_pages, VM_MAP,
				pgprot_writecombine(PAGE_KERNEL));
	if(!kg_obj->vaddr) {
		dev_err(drm->dev, "failed to vmap framebuffer\n");
		ret = -ENOMEM;
		goto err_remove_node;
	}
#endif

	mutex_unlock(&kdrm->mm_lock);

	return 0;

err_remove_node:
	drm_mm_remove_node(kg_obj->mm);
err_free_mm:
	mutex_unlock(&kdrm->mm_lock);
	kfree(kg_obj->mm);
	return ret;
}

#define Q_LENS (8)

struct saved_kg_obj
{
	dma_addr_t paddr;
	size_t size;
	struct drm_mm_node *mm;
};
struct saved_kg_obj defer_free_queue[Q_LENS];
int defer_free_header = 0;
struct mutex defer_index_mutex;

static int kunlun_gem_iommu_unmap(struct kunlun_gem_object *kg_obj)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	static int first = 1;

	if(!kg_obj->mm)
		return 0;

	if(kdrm->iommu_enable) {
#if 1
		int index = 0;
		DRM_DEBUG("%s: %d, free gem_obj=%p, defer iommu_unmap!\n", __func__, __LINE__,
				gem_obj);
		if (first)
		{
			memset(defer_free_queue, 0, sizeof(struct saved_kg_obj) * Q_LENS);
			first = 0;
			mutex_init(&defer_index_mutex);
		}
		mutex_lock(&defer_index_mutex);
		index = defer_free_header++;
		if (defer_free_header == Q_LENS)
			defer_free_header = 0;
		mutex_unlock(&defer_index_mutex);
		if (defer_free_queue[index].mm != NULL)
		{
			iommu_unmap(kdrm->domain, defer_free_queue[index].paddr, defer_free_queue[index].size);
			mutex_lock(&kdrm->mm_lock);
			drm_mm_remove_node(defer_free_queue[index].mm);
			mutex_unlock(&kdrm->mm_lock);
			kfree(defer_free_queue[index].mm);
			defer_free_queue[index].mm = NULL;
		}
		defer_free_queue[index].paddr	= kg_obj->paddr;
		defer_free_queue[index].size	= kg_obj->size;
		defer_free_queue[index].mm		= kg_obj->mm;
		// need to clear mm explictly, in case free the same gem second time
		kg_obj->mm = NULL;
#endif
		return 0;
	}

	iommu_unmap(kdrm->domain, kg_obj->paddr, kg_obj->size);


	mutex_lock(&kdrm->mm_lock);

	drm_mm_remove_node(kg_obj->mm);

	mutex_unlock(&kdrm->mm_lock);

	kfree(kg_obj->mm);
	kg_obj->mm = NULL;

	return 0;

}

static int kunlun_gem_get_pages(struct kunlun_gem_object *kg_obj)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;
	int ret = -1;
	int i = 0;
	struct scatterlist *s;

	kg_obj->pages = drm_gem_get_pages(gem_obj);
	if(IS_ERR(kg_obj->pages))
		return PTR_ERR(kg_obj->pages);

	kg_obj->num_pages = gem_obj->size >> PAGE_SHIFT;

	kg_obj->sgt = drm_prime_pages_to_sg(kg_obj->pages, kg_obj->num_pages);
	if(IS_ERR(kg_obj->sgt)) {
		ret = PTR_ERR(kg_obj->sgt);
		dev_err(drm->dev, "failed to allocate sgt: ret=%d\n", ret);
		kg_obj->sgt = NULL;
		goto err_put_pags;
	}

	/*
	 * Fake up the SG table so that dma_sync_sg_for_device() can be used
	 * to flush the pages associated with it.
	 *
	 * TODO: Replace this by drm_clflush_sg() once it can be implemented
	 * without relying on symbols that are not exported.
	 */
	for_each_sg(kg_obj->sgt->sgl, s, kg_obj->sgt->nents, i)
		sg_dma_address(s) = sg_phys(s);

	dma_sync_sg_for_device(drm->dev, kg_obj->sgt->sgl,
			kg_obj->sgt->nents, DMA_TO_DEVICE);

	return 0;

err_put_pags:
	drm_gem_put_pages(gem_obj, kg_obj->pages, false, false);
	kg_obj->pages = NULL;
	return ret;
}

static void kunlun_gem_put_pages(struct kunlun_gem_object *kg_obj)
{
	if (kg_obj->sgt) {
		sg_free_table(kg_obj->sgt);
		kfree(kg_obj->sgt);
		kg_obj->sgt = NULL;
	}

	if (kg_obj->pages) {
		drm_gem_put_pages(&kg_obj->base, kg_obj->pages, true, false);
		kg_obj->pages = NULL;
	}
}

static int kunlun_gem_alloc_iommu(struct kunlun_gem_object *kg_obj)
{
	int ret;

	ret = kunlun_gem_get_pages(kg_obj);
	if(ret < 0)
		return ret;

	ret = kunlun_gem_iommu_map(kg_obj);
	if(ret < 0)
		goto err_kunlun_put_pages;

	return ret;

err_kunlun_put_pages:
	kunlun_gem_put_pages(kg_obj);

	return ret;
}

static void kunlun_gem_free_iommu(struct kunlun_gem_object *kg_obj)
{
	if (kg_obj->vaddr)
		vunmap(kg_obj->vaddr);

	kunlun_gem_iommu_unmap(kg_obj);

	kunlun_gem_put_pages(kg_obj);
}

static int kunlun_gem_alloc_dma(struct kunlun_gem_object *kg_obj)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;

	kg_obj->vaddr = dma_alloc_wc(drm->dev, gem_obj->size,
			&kg_obj->paddr, GFP_KERNEL | __GFP_NOWARN);
	if(!kg_obj->vaddr) {
		DRM_DEV_ERROR(drm->dev, "failed to allocate buffer of size %zu\n",
				gem_obj->size);
		return -ENOMEM;
	}

	return 0;
}

static void kunlun_gem_free_dma(struct kunlun_gem_object *kg_obj)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;

	dma_free_wc(drm->dev, gem_obj->size, kg_obj->vaddr, kg_obj->paddr);
}

static void kunlun_gem_free_buf(struct kunlun_gem_object *kg_obj)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;
	struct kunlun_drm_data *kdrm = drm->dev_private;

	if(kdrm->iommu_enable)
		kunlun_gem_free_iommu(kg_obj);
	else
		kunlun_gem_free_dma(kg_obj);
}

static int kunlun_gem_alloc_buf(struct kunlun_gem_object *kg_obj)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;
	struct kunlun_drm_data *kdrm = drm->dev_private;

	if(kdrm->iommu_enable)
		return kunlun_gem_alloc_iommu(kg_obj);
	else
		return kunlun_gem_alloc_dma(kg_obj);
}

static void kunlun_gem_release_object(struct kunlun_gem_object *kg_obj)
{
	drm_gem_object_release(&kg_obj->base);
	kfree(kg_obj);
	kg_obj = NULL;
}

struct kunlun_gem_object *kunlun_gem_create_object(
		struct drm_device *drm, size_t size)
{
	struct kunlun_gem_object *kg_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	size = round_up(size, PAGE_SIZE);

	kg_obj = kzalloc(sizeof(*kg_obj), GFP_KERNEL);
	if(!kg_obj)
		return ERR_PTR(-ENOMEM);

	gem_obj = &kg_obj->base;

	ret = drm_gem_object_init(drm, gem_obj, size);
	if(ret)
		goto err_free;

	ret = kunlun_gem_alloc_buf(kg_obj);
	if(ret)
		goto err_release_obj;

	return kg_obj;

err_release_obj:
	drm_gem_object_release(gem_obj);
err_free:
	kfree(kg_obj);
	return ERR_PTR(ret);
}

static struct kunlun_gem_object *
kunlun_gem_create_with_handle(struct drm_file *file_priv,
		struct drm_device *drm, size_t size, uint32_t *handle)
{
	struct kunlun_gem_object *kg_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	kg_obj = kunlun_gem_create_object(drm, size);
	if(IS_ERR(kg_obj))
		return kg_obj;

	gem_obj = &kg_obj->base;

	ret = drm_gem_handle_create(file_priv, gem_obj, handle);

	drm_gem_object_put_unlocked(gem_obj);
	if(ret)
		goto err_free_object;

	return kg_obj;

err_free_object:
	kunlun_gem_free_object(gem_obj);
	return ERR_PTR(ret);
}

void kunlun_gem_free_object(struct drm_gem_object *obj)
{
	struct drm_device *drm = obj->dev;
	struct kunlun_gem_object *kg_obj;
	struct kunlun_drm_data *kdrm = drm->dev_private;

	kg_obj = to_kunlun_obj(obj);

	if (obj->import_attach) {
		if (kdrm->iommu_enable) {
			kunlun_gem_iommu_unmap(kg_obj);
		}
		else {
			dma_unmap_sg(drm->dev, kg_obj->sgt->sgl,
					kg_obj->sgt->nents, DMA_BIDIRECTIONAL);
		}
		drm_prime_gem_destroy(obj, kg_obj->sgt);
	}
	else {
		kunlun_gem_free_buf(kg_obj);
	}

	kunlun_gem_release_object(kg_obj);
}

int kunlun_gem_dumb_create(struct drm_file *file_priv,
		struct drm_device *drm, struct drm_mode_create_dumb *args)
{
	struct kunlun_gem_object *kg_obj;

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	args->size = args->pitch * args->height;

	kg_obj = kunlun_gem_create_with_handle(file_priv, drm,
			args->size, &args->handle);

	return PTR_ERR_OR_ZERO(kg_obj);
}

struct sg_table *kunlun_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct drm_device *drm = obj->dev;
	struct kunlun_gem_object *kg_obj;
	struct sg_table *sgt;
	int ret;

	kg_obj = to_kunlun_obj(obj);

	if(kg_obj->pages)
		return drm_prime_pages_to_sg(kg_obj->pages, kg_obj->num_pages);

	if (kg_obj->sgt)
		return kg_obj->sgt;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if(!sgt)
		return NULL;

	ret = dma_get_sgtable(drm->dev, sgt, kg_obj->vaddr,
			kg_obj->paddr, obj->size);
	if(ret < 0)
		goto err_free;

	sg_dma_address(sgt->sgl) = kg_obj->paddr;

	return sgt;

err_free:
	kfree(sgt);
	return NULL;
}

struct drm_gem_object *kunlun_gem_prime_import_sg_table(struct drm_device *drm,
		struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;
	size_t size = attach->dmabuf->size;
	struct kunlun_gem_object *kg_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	kg_obj = kzalloc(sizeof(*kg_obj), GFP_KERNEL);
	if(!kg_obj)
		return ERR_PTR(-ENOMEM);

	gem_obj = &kg_obj->base;

	ret = drm_gem_object_init(drm, gem_obj, size);
	if(ret)
		goto err_free;

	kg_obj->sgt = sgt;

	if(kdrm->iommu_enable) {
		ret = kunlun_gem_iommu_map(kg_obj);
		if(ret)
			goto err_free;
		return gem_obj;
	}
	else {
		ret = dma_map_sg(drm->dev, sgt->sgl, sgt->nents, DMA_BIDIRECTIONAL);
		if (!ret) {
			ret = -EINVAL;
			goto err_free;
		}

		if(sgt->nents != 1) {
			ret = -EINVAL;
			goto err_free;
		}

		kg_obj->paddr = sg_dma_address(sgt->sgl);

		return gem_obj;
	}

err_free:
	kunlun_gem_release_object(kg_obj);
	return ERR_PTR(ret);
}

void kunlun_gem_prime_vunmap(struct drm_gem_object *obj, void *addr)
{
	struct kunlun_gem_object *kg_obj;

	kg_obj = to_kunlun_obj(obj);

	kg_obj->vmap_count--;

	if (kg_obj->pages) {
		vunmap(addr);
		kg_obj->vaddr = NULL;
	}
}

void *kunlun_gem_prime_vmap(struct drm_gem_object *obj)
{
	struct kunlun_gem_object *kg_obj;
#if 0
	struct kunlun_drm_data *kdrm = drm->dev_private;
	int ret = -EFAULT;
#endif

	kg_obj = to_kunlun_obj(obj);

	kg_obj->vmap_count++;

#if 0
	if(kg_obj->vaddr)
		return kg_obj->vaddr;

	if(kdrm->iommu_enable) {
		kg_obj->vaddr = vmap(kg_obj->pages, kg_obj->num_pages, VM_MAP,
				pgprot_writecombine(PAGE_KERNEL));
		if(!kg_obj->vaddr) {
			ret = -ENOMEM;
			goto err_vmap_count;
		}

		return kg_obj->vaddr;
	}

err_vmap_count:
	kg_obj->vmap_count--;
	return ERR_PTR(ret);
#else
	if (kg_obj->pages)
		return vmap(kg_obj->pages, kg_obj->num_pages, VM_MAP,
				pgprot_writecombine(PAGE_KERNEL));

	return kg_obj->vaddr;
#endif

}

static int kunlun_drm_gem_mmap_iommu_obj(struct kunlun_gem_object *kg_obj,
		struct vm_area_struct *vma)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	unsigned int i, count;
	unsigned long ucount, offset;
	unsigned long uaddr;
	int ret;

	uaddr = vma->vm_start;
	offset = vma->vm_pgoff;
	count = gem_obj->size >> PAGE_SHIFT;
	ucount = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

	if(ucount > (count - offset))
		return -ENXIO;

	for(i = 0; i < ucount; i++) {
		ret = vm_insert_page(vma, uaddr, kg_obj->pages[i + offset]);
		if(ret)
			return ret;
		uaddr += PAGE_SIZE;
	}

	return 0;
}

static int kunlun_drm_gem_mmap_dma_obj(struct kunlun_gem_object *kg_obj,
		struct vm_area_struct *vma)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;

	return dma_mmap_wc(drm->dev, vma, kg_obj->vaddr, kg_obj->paddr,
			gem_obj->size);
}

static int kunlun_gem_mmap_obj(struct kunlun_gem_object *kg_obj,
		struct vm_area_struct *vma)
{
	struct drm_gem_object *gem_obj = &kg_obj->base;
	struct drm_device *drm = gem_obj->dev;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	int ret;

	/*
	* Clear the VM_PFNMAP flag that was set by drm_gem_mmap().
	*/
	vma->vm_flags &= ~VM_PFNMAP;

	if(kdrm->iommu_enable)
		ret = kunlun_drm_gem_mmap_iommu_obj(kg_obj, vma);
	else
		ret = kunlun_drm_gem_mmap_dma_obj(kg_obj, vma);

	if(ret)
		drm_gem_vm_close(vma);

	return ret;
}

int kunlun_gem_prime_mmap(struct drm_gem_object *obj,
		struct vm_area_struct *vma)
{
	struct kunlun_gem_object *kg_obj;
	int ret;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if(ret < 0)
		return ret;

	kg_obj = to_kunlun_obj(obj);
	return kunlun_gem_mmap_obj(kg_obj, vma);
}

int kunlun_drm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *gem_obj;
	struct kunlun_gem_object *kg_obj;
	struct kunlun_drm_data *kdrm;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if(ret)
		return ret;

	/*
	 * Set vm_pgoff (used as a fake buffer offset by DRM) to 0 and map the
	 * whole buffer from the start.
	 */
	vma->vm_pgoff = 0;

	vma->vm_flags &= ~VM_PFNMAP;
	if (vm_cache) {
		vma->vm_flags &= ~(VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	}
	gem_obj = vma->vm_private_data;
	kg_obj = to_kunlun_obj(gem_obj);
	kdrm = gem_obj->dev->dev_private;

	if (kdrm->iommu_enable) {
		ret = kunlun_drm_gem_mmap_iommu_obj(kg_obj, vma);
	}
	else {
		ret = remap_pfn_range(vma, vma->vm_start, PFN_DOWN(kg_obj->paddr) + vma->vm_pgoff,
						  vma->vm_end - vma->vm_start, vma->vm_page_prot);
	}

	if(ret)
		drm_gem_vm_close(vma);

	return ret;
}

int sdrv_gem_dmabuf_begin_cpu_access(struct dma_buf *buffer,
		enum dma_data_direction dir)
{
	struct drm_gem_object *obj = buffer->priv;
	struct drm_device *drm = obj->dev;
	struct sg_table *table;

	table = drm->driver->gem_prime_get_sg_table(obj);

	if (table)
		dma_sync_sg_for_cpu(drm->dev, table->sgl, table->nents, dir);
	return 0;
}

int sdrv_gem_dmabuf_end_cpu_access(struct dma_buf *buffer,
					  enum dma_data_direction dir)
{
	struct drm_gem_object *obj = buffer->priv;
	struct drm_device *drm = obj->dev;

	struct sg_table *table;
	table = drm->driver->gem_prime_get_sg_table(obj);

	if (table)
		dma_sync_sg_for_device(drm->dev, table->sgl, table->nents, dir);

	return 0;
}
