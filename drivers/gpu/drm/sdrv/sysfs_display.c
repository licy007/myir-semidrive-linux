#include <linux/kernel.h>
#include <linux/module.h>
#include "sysfs_display.h"

struct class *display_class;

static int __init display_class_init(void)
{
    pr_info("display class register\n");
    display_class = class_create(THIS_MODULE, "display");
    if (IS_ERR(display_class)) {
        pr_err("Unable to create display class\n");
        return PTR_ERR(display_class);
    }
    return 0;
}
postcore_initcall(display_class_init);