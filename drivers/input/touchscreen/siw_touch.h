#ifndef __SIW_TOUCH_H__
#define __SIW_TOCUH_H__

#ifdef CONFIG_OF
#define SIW_CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#endif

#define SIW_DRIVER_VERSION	    "1.2"
#define CFG_MAX_TOUCH_POINTS    10

// SCREEN SIZE
#define SCREEN_MAX_X    (1920)
#define SCREEN_MAX_Y    (1200)

// RAW DATE DEBUGGING
#define COLUMN_CELL_SIZE    15
#define ROW_CELL_SIZE       20

// Debug
#define DEBUG_SIW_TOUCH
#ifdef DEBUG_SIW_TOUCH
	#define SIW_TOUCH_DBG(fmt, args...) \
		printk(KERN_INFO "[SIW-DBG] %s: " fmt, __func__, ## args)
	#define SIW_TOUCH_ERR(fmt, args...) \
		printk(KERN_ERR "[SIW-ERR] %s: " fmt, __func__, ## args)
#else
	#define SIW_TOUCH_DBG(fmt, args...)
	#define SIW_TOUCH_ERR(fmt, args...)
#endif

#endif /*__SIW_TOCUH_H__ */

