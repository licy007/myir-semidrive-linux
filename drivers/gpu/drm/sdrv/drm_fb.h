/*
 * kunlun_drm_gem.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_FB_H
#define _KUNLUN_DRM_FB_H

struct drm_framebuffer *kunlun_drm_fbdev_fb_alloc(struct drm_device *drm,
		struct drm_fb_helper_surface_size *sizes,
		unsigned int pitch_align, struct drm_gem_object *obj);
#endif /* _KUNLUN_DRM_FB_H */

