/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef _SEMIDRIVE_ION_H
#define _SEMIDRIVE_ION_H

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <dt-bindings/memory/sdrv_ion.h>
#include "../ion.h"

/**
 * struct sdrv_ion_device_data - the metadata of the ion-sdrv device node
 * @num_heaps:		number of all the platform-defined heaps
 * @heaps:		list of all the platform-defined heaps
 */
struct sdrv_ion_device_data {
	uint32_t num_heaps;
	struct ion_heap **heaps;
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data);

#endif
