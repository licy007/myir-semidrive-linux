/*
 * kunlun_drm_fbdev.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_crtc_helper.h>

#include "drm_drv.h"
#include "drm_gem.h"
#include "drm_fb.h"
#include "drm_fbdev.h"

#define PREFERRED_BPP		32

static int kunlun_fbdev_mmap(struct fb_info *info,
		struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct drm_gem_object *obj;

	obj = drm_gem_fb_get_obj(helper->fb, 0);

	return kunlun_gem_prime_mmap(obj, vma);
}

static struct fb_ops kunlun_drm_fbdev_ops = {
	.owner			= THIS_MODULE,
	DRM_FB_HELPER_DEFAULT_OPS,
	.fb_mmap		= kunlun_fbdev_mmap,
	.fb_fillrect	= drm_fb_helper_cfb_fillrect,
	.fb_copyarea	= drm_fb_helper_cfb_copyarea,
	.fb_imageblit	= drm_fb_helper_cfb_imageblit,
};

static int kunlun_drm_fbdev_create(struct drm_fb_helper *helper,
		struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *drm = helper->dev;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	struct kunlun_gem_object *kg_obj;
	unsigned int bytes_per_pixel, pitches;
	struct drm_framebuffer *fb;
	unsigned long offset;
	struct fb_info *fbi;
	size_t size;
	int ret;

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);
	pitches = sizes->surface_width * bytes_per_pixel;
	size = pitches * sizes->surface_height;

	kg_obj = kunlun_gem_create_object(drm, size);
	if(IS_ERR(kg_obj))
		return PTR_ERR(kg_obj);

	fbi = drm_fb_helper_alloc_fbi(helper);
	if(IS_ERR(fbi)) {
		ret = PTR_ERR(fbi);
		dev_err(drm->dev, "Failed to allocate framebuffer info\n");
		goto err_free_object;
	}

	fb = kunlun_drm_fbdev_fb_alloc(drm, sizes, 0, &kg_obj->base);
	if(IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		dev_err(drm->dev, "failed to allocate DRM framebuffer\n");
		goto err_fbi_destroy;
	}

	helper->fb = fb;

	fbi->par = helper;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->fbops = &kunlun_drm_fbdev_ops;

	drm_fb_helper_fill_fix(fbi, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(fbi, helper, sizes->fb_width, sizes->fb_height);

	offset = fbi->var.xoffset * bytes_per_pixel;
	offset += fbi->var.yoffset * fb->pitches[0];

	if(kdrm->iommu_enable) {
		kg_obj->vaddr = vmap(kg_obj->pages, kg_obj->num_pages, VM_MAP,
				pgprot_writecombine(PAGE_KERNEL));
		if(!kg_obj->vaddr) {
			dev_err(drm->dev, "failed to vmap framebuffer\n");
			ret = -ENOMEM;
			goto err_drm_buffer_remove;
		}
	}

	drm->mode_config.fb_base = (resource_size_t)kg_obj->paddr;
	fbi->screen_base = (void __iomem *)kg_obj->vaddr + offset;
	fbi->screen_size = size;
	fbi->fix.smem_start = (unsigned long)(kg_obj->paddr + offset);
	fbi->fix.smem_len = size;

	DRM_DEBUG_KMS("FB [%dx%d]-%d vaddr=%p offset=%ld size=%zu\n",
		      fb->width, fb->height, fb->format->depth,
		      kg_obj->vaddr,
		      offset, size);

	return 0;

err_drm_buffer_remove:
	drm_framebuffer_remove(fb);
err_fbi_destroy:
	drm_fb_helper_fini(helper);
err_free_object:
	kunlun_gem_free_object(&kg_obj->base);
	return ret;
}

static const struct drm_fb_helper_funcs kunlun_fb_helper_funcs = {
	.fb_probe = kunlun_drm_fbdev_create,
};

int kunlun_drm_fbdev_init(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;
	struct drm_fb_helper *helper;
	int ret;

	helper = kzalloc(sizeof(*helper), GFP_KERNEL);
	if(!helper) {
		dev_err(drm->dev, "failed to allocate drm fb helper\n");
		return -ENOMEM;
	}

	drm_fb_helper_prepare(drm, helper, &kunlun_fb_helper_funcs);

	ret = drm_fb_helper_init(drm, helper, kdrm->num_crtcs);
	if(ret < 0) {
		dev_err(drm->dev, "Failed to initialize drm fb helper.\n");
		goto err_free;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if(ret < 0) {
		dev_err(drm->dev, "Failed to add all connectors.\n");
		goto err_fb_helper_fini;
	}

	ret = drm_fb_helper_initial_config(helper, PREFERRED_BPP);
	if(ret < 0) {
		dev_err(drm->dev, "Failed to set initial hw config\n");
		goto err_fb_helper_fini;
	}

	kdrm->fb_helper = helper;

	return 0;

err_fb_helper_fini:
	drm_fb_helper_fini(helper);
err_free:
	kfree(helper);
	return ret;
}

void kunlun_drm_fbdev_fini(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;
	struct drm_fb_helper *helper = kdrm->fb_helper;

	kdrm->fb_helper = NULL;

	drm_fb_helper_unregister_fbi(helper);

	if(helper->fb)
		drm_framebuffer_put(helper->fb);

	drm_fb_helper_fini(helper);

	kfree(helper);
}
