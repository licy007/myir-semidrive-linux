/*
 * kunlun_drm_fbdev.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_FBDEV_H
#define _KUNLUN_DRM_FBDEV_H

#ifdef CONFIG_DRM_FBDEV_EMULATION
int kunlun_drm_fbdev_init(struct drm_device *drm);
void kunlun_drm_fbdev_fini(struct drm_device *drm);
#else
static inline int kunlun_drm_fbdev_init(struct drm_device *drm)
{
	return 0;
}

static inline void kunlun_drm_fbdev_fini(struct drm_device *drm)
{

}
#endif

#endif

