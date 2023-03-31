#ifndef XRP_IOMMU_ALLOC_H
#define XRP_IOMMU_ALLOC_H

#include "xrp_alloc.h"

struct device;

#ifdef CONFIG_IOMMU_DMA
long xrp_init_iommu_pool(struct xrp_allocation_pool **pool, struct device *dev);
#else
static inline long xrp_init_iommu_pool(struct xrp_allocation_pool **pool,
				     struct device *dev)
{
	return -ENXIO;
}
#endif

#endif
