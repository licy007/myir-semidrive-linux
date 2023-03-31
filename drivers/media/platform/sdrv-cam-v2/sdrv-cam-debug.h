/*
 * sdrv-debug.h
 *
 * Semidrive platform csi header file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef SDRV_DEBUG_H
#define SDRV_DEBUG_H

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

extern char *cam_printk[];


/*CAM_EMERG:    printf not
 *CAM_ERR:      printf err
 *CAM_WARNING:  printf info/warning/err
 *CAM_INFO:     printf info/warning/err
 *CAM_DEBUG:    printf debug/info/warning/err
 */

#define CAM_EMERG       0
#define CAM_ERR         1
#define CAM_WARNING     2
#define CAM_INFO        3
#define CAM_DEBUG       4

#define cam_err_loglevel (cam_printk[0])
#define cam_warn_loglevel (cam_printk[1])
#define cam_info_loglevel (cam_printk[2])
#define cam_debug_loglevel (cam_printk[3])

#define cam_dev_printk(level, dev, fmt, arg...)				\
({							\
	do {						\
		if (level)					\
			dev_printk(level, dev, "%s:"fmt, __func__, ##arg);	\
	} while (0);					\
	0;						\
})

#define cam_printk(level, fmt, arg...)				\
({                          \
    do {                        \
        if (level)                  \
            printk("%s" "%s:"fmt, level, __func__, ##arg);  \
    } while (0);                    \
    0;                      \
})

#define cam_dev_loge(dev, format, arg...)		\
	    cam_dev_printk(cam_err_loglevel, dev, "error: "format, ##arg)

#define cam_dev_logw(dev, format, arg...)		\
        cam_dev_printk(cam_warn_loglevel, dev, "warning: "format, ##arg)

#define cam_dev_logi(dev, format, arg...)		\
        cam_dev_printk(cam_info_loglevel, dev, "info: "format, ##arg)

#define cam_dev_logd(dev, format, arg...)		\
        cam_dev_printk(cam_debug_loglevel, dev, "debug: "format, ##arg)

#define cam_loge(format, arg...)		\
	    cam_printk(cam_err_loglevel, "error: "format, ##arg)

#define cam_logw(format, arg...)		\
        cam_printk(cam_warn_loglevel, "warning: "format, ##arg)

#define cam_logi(format, arg...)		\
        cam_printk(cam_info_loglevel, "info: "format, ##arg)

#define cam_logd(format, arg...)		\
        cam_printk(cam_debug_loglevel, "debug: "format, ##arg)


int cam_debug_init(struct platform_device *pdev);
void cam_debug_exit(struct platform_device *pdev);

#endif
