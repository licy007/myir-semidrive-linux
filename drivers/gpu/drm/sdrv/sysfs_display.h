#ifndef _SYSFS_DISPLAY_H_
#define _SYSFS_DISPLAY_H_

#include <linux/device.h>
#include <linux/sysfs.h>

extern struct class *display_class;
int sysfs_crtc_init(struct device *);

#endif