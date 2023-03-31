/*
 * kunlun_drm_gem.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_GEM_H_
#define _KUNLUN_DRM_GEM_H_

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mode.h>

struct kunlun_gem_object {
	struct drm_gem_object base;

	void *vaddr;
	uint8_t vmap_count;

	dma_addr_t paddr;

	/* IOMMU enabled */
	struct drm_mm_node *mm;
	unsigned long num_pages;
	struct page **pages;
	struct sg_table *sgt;
	size_t size;
};

#define to_kunlun_obj(obj)	container_of(obj, struct kunlun_gem_object, base)

struct kunlun_gem_object *kunlun_gem_create_object(
		struct drm_device *drm, size_t size);
void kunlun_gem_free_object(struct drm_gem_object *obj);
int kunlun_gem_dumb_create(struct drm_file *file_priv,
		struct drm_device *drm, struct drm_mode_create_dumb *args);
struct sg_table *kunlun_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *kunlun_gem_prime_import_sg_table(struct drm_device *drm,
		struct dma_buf_attachment *attach, struct sg_table *sgt);
void kunlun_gem_prime_vunmap(struct drm_gem_object *obj, void *addr);
void *kunlun_gem_prime_vmap(struct drm_gem_object *obj);
int kunlun_gem_prime_mmap(struct drm_gem_object *obj,
		struct vm_area_struct *vma);
int kunlun_drm_mmap(struct file *filp, struct vm_area_struct *vma);
#endif
