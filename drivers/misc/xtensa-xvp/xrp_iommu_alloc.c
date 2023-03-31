#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <linux/iova.h>
#include "xrp_iommu_alloc.h"

struct xrp_iommu_allocation {
	struct xrp_allocation allocation;
	void *kvaddr;
};

struct xrp_iommu_pool {
	struct xrp_allocation_pool pool;
	struct device *dev;
};

static long xrp_iommu_alloc(struct xrp_allocation_pool *allocation_pool,
			  u32 size, u32 align, struct xrp_allocation **alloc)
{
	struct xrp_iommu_pool *pool = container_of(allocation_pool,
						 struct xrp_iommu_pool, pool);
	struct xrp_iommu_allocation *new_iommu;
	struct xrp_allocation *new;
	dma_addr_t dma_addr;
	void *kvaddr;

	size = ALIGN(size, PAGE_SIZE);

	new_iommu = kzalloc(sizeof(struct xrp_iommu_allocation), GFP_KERNEL);
	if (!new_iommu)
		return -ENOMEM;

	new = &new_iommu->allocation;

	kvaddr = dma_alloc_attrs(pool->dev, size, &dma_addr, GFP_KERNEL,
				 0);

	if (!kvaddr) {
		kfree(new_iommu);
		return -ENOMEM;
	}
	new->pool = allocation_pool;
	new->start = dma_to_phys(pool->dev, dma_addr);
	new->size = size;
	atomic_set(&new->ref, 0);
	xrp_allocation_get(new);
	new_iommu->kvaddr = kvaddr;
	*alloc = new;

	return 0;
}

static void xrp_iommu_free(struct xrp_allocation *xrp_allocation)
{
	struct xrp_iommu_pool *pool = container_of(xrp_allocation->pool,
						 struct xrp_iommu_pool, pool);
	struct xrp_iommu_allocation *a = container_of(xrp_allocation,
						    struct xrp_iommu_allocation,
						    allocation);

	dma_free_attrs(pool->dev, xrp_allocation->size,
		       a->kvaddr,
		       phys_to_dma(pool->dev, xrp_allocation->start),
		       0);

	kfree(a);
}

static void xrp_iommu_free_pool(struct xrp_allocation_pool *allocation_pool)
{
	struct xrp_iommu_pool *pool = container_of(allocation_pool,
						 struct xrp_iommu_pool, pool);

	kfree(pool);
}

static phys_addr_t xrp_iommu_offset(const struct xrp_allocation *allocation)
{
	return allocation->start;
}

static const struct xrp_allocation_ops xrp_iommu_pool_ops = {
	.alloc = xrp_iommu_alloc,
	.free = xrp_iommu_free,
	.free_pool = xrp_iommu_free_pool,
	.offset = xrp_iommu_offset,
};

long xrp_init_iommu_pool(struct xrp_allocation_pool **ppool, struct device *dev)
{
	struct xrp_iommu_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool)
		return -ENOMEM;

	pool->pool.ops = &xrp_iommu_pool_ops;
	pool->dev = dev;
	*ppool = &pool->pool;
	return 0;
}