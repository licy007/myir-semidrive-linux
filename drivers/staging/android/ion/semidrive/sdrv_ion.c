/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <linux/genalloc.h>
#include <asm/cacheflush.h>
#include "sdrv_ion.h"

#define SEMIDRIVE_COMPAT_STR	"semidrive,sdrv-ion"

#define MAKE_HEAP_TYPE_MAPPING(h) { .name = ION_HEAP_NAME_##h, \
			.heap_type = ION_HEAP_TYPE_##h, }

static struct heap_types_info {
	const char *name;
	enum ion_heap_type heap_type;
} heap_types_info[] = {
	MAKE_HEAP_TYPE_MAPPING(CARVEOUT),
	//MAKE_HEAP_TYPE_MAPPING(SECURE_DMA),
	//MAKE_HEAP_TYPE_MAPPING(HYP_CMA),
};

static struct sdrv_mem {
	phys_addr_t	base;
	phys_addr_t	size;
} sdrv_heap_mem ;


//#define CONFIG_USE_PCIE
#ifdef CONFIG_USE_PCIE
#define RAM_POOL_BASE	0x540000000
#define RAM_POOL_SIZE 0x8000000
static struct gen_pool *ram_pool;
struct gen_pool *ram_pool_get(void)
{
	return ram_pool;
}
EXPORT_SYMBOL(ram_pool_get);

void *ram_alloc(struct device *dev, size_t len, dma_addr_t *dma)
{
	unsigned long offset;
	dma_addr_t *dst;

	if (dma)
		*dma = 0;
	if (!ram_pool) {
		pr_err("drm: ram_pool %p\n", ram_pool);
		return NULL;
	}

	return gen_pool_dma_alloc(ram_pool, len, dma);
}
EXPORT_SYMBOL(ram_alloc);

void ram_free(void *vaddr, void *paddr, size_t len)
{
	gen_pool_free(ram_pool, (unsigned long) vaddr, len);
}
EXPORT_SYMBOL(ram_free);

int ram_init(struct	device *pdev, phys_addr_t phys, size_t len)
{
	int status = 0;
	void __iomem *addr;

	if (!phys && !len) {
		phys = RAM_POOL_BASE;
		len = RAM_POOL_SIZE;
	}

	if (len) {
		ram_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!ram_pool)
			status = -ENOMEM;
	}

	if (ram_pool) {
		addr = __ioremap(phys, len, __pgprot(PROT_NORMAL));
		status = gen_pool_add_virt(ram_pool, addr, phys, len, -1);
	}

	WARN_ON(status < 0);
	return status;
}
EXPORT_SYMBOL(ram_init);
#else
void *ram_alloc(struct device *dev, size_t len, dma_addr_t *dma)
{
	return NULL;
}
EXPORT_SYMBOL(ram_alloc);
void ram_free(void *vaddr, void *paddr, size_t len)
{
	return;
}
EXPORT_SYMBOL(ram_free);

int ram_init(struct	device *pdev, phys_addr_t phys, size_t len)
{
	return 0;
}
EXPORT_SYMBOL(ram_init);
#endif

static struct ion_heap *sdrv_ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;
	switch ((int)heap_data->type) {
	case ION_HEAP_TYPE_CARVEOUT:
		heap = ion_carveout_heap_create(heap_data);
		break;
	//case ION_HEAP_TYPE_SECURE_DMA:
	//	break;
	//case ION_HEAP_TYPE_HYP_CMA:
	//	break;
	default:
		pr_err("%s: Invalid heap type %d\n", __func__,
		       heap_data->type);
		return ERR_PTR(-EINVAL);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base 0x%llx size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;

	return heap;
}

static int sdrv_ion_get_heap_type_from_dt_node(struct device_node *node,
					struct ion_platform_heap *heap)
{
	const char *type_name;
	int i, ret = -EINVAL;
	ret = of_property_read_string(node, "sdrv,ion-heap-type", &type_name);
	if (ret)
		goto out;
	for (i = 0; i < ARRAY_SIZE(heap_types_info); ++i) {
		if (!strcmp(heap_types_info[i].name, type_name)) {
			heap->type = heap_types_info[i].heap_type;
			ret = 0;
			goto out;
		}
	}
	WARN(1, "Unknown heap type: %s for heap %s. You might need to update heap_types_info in %s",
		type_name, heap->name, __FILE__);
out:
	return ret;
}

static int sdrv_ion_get_heap_memory_region(struct device_node *node,
					struct ion_platform_heap *heap)
{
	struct device_node *target;
	const __be32 *basep;
	u64 size, base;

	target = of_parse_phandle(node, "memory-region", 0);
	if (!target) {
		WARN(1, "Not found node of memory-region for heap %s\n", heap->name);
		goto err;
	}
	if(sdrv_heap_mem.base == 0) {
		basep = of_get_address(target, 0, &size, NULL);
		if (!basep) {
			WARN(1, "Not found address from reg property");
			goto err;
		} else {
			heap->size = size;
			base = of_translate_address(target, basep);
			if (OF_BAD_ADDR == heap->base) {
				WARN(1, "Failed to parse DT node for heap %s\n", heap->name);
				goto err;
			}
		}

		heap->base = base;
	} else {
		heap->base = sdrv_heap_mem.base;
		heap->size = sdrv_heap_mem.size;
	}

	of_node_put(target);

	return 0;

err:
	return -EINVAL;
}

static struct ion_platform_heap *sdrv_ion_parse_dt(struct platform_device *pdev,
					uint32_t *num_valid, uint32_t *num_child)
{
	struct ion_platform_heap *heaps = NULL;
	uint32_t num_heaps = 0;
	int idx = 0;
	const struct device_node *dt_node = pdev->dev.of_node;
	struct device_node *node;
        int ret = 0;

	for_each_available_child_of_node(dt_node, node)
		num_heaps++;

	if (0 == num_heaps)
		return ERR_PTR(-EINVAL);

	heaps = kzalloc(sizeof(struct ion_platform_heap) * num_heaps, GFP_KERNEL);
	if (!heaps) {
		return ERR_PTR(-ENOMEM);
	}

	*num_child = num_heaps;

	for_each_available_child_of_node(dt_node, node) {
		ret = sdrv_ion_get_heap_type_from_dt_node(node, &heaps[idx]);
		if (ret)
			continue;
		ret = sdrv_ion_get_heap_memory_region(node, &heaps[idx]);
		if (ret)
			continue;

		heaps[idx].name = node->name;
		pr_info("Scan out new heap %s\n", heaps[idx].name);
		++idx;
	}

	if (0 == idx)
		goto free_heaps;

	*num_valid = idx;
	return heaps;

free_heaps:
	kfree(heaps);
	return ERR_PTR(-EINVAL);
}

static int sdrv_ion_probe(struct platform_device *pdev)
{
	struct ion_heap **heaps;
	struct ion_platform_heap *pdatas;
	uint32_t num_valid, num_child;
	int ret = 0;
	int i;

#ifdef CONFIG_USE_PCIE
	ret = ram_init(NULL, RAM_POOL_BASE, RAM_POOL_SIZE);
	if (ret < 0)
		pr_err("pcie ram_init fail.\n");
	else
		pr_notice("pcie ram_init success.\n");
#endif

	pdatas = sdrv_ion_parse_dt(pdev, &num_valid, &num_child);
	if (IS_ERR(pdatas)) {
		ret = PTR_ERR(pdatas);
		goto out;
	}

	heaps = kcalloc(num_valid, sizeof(struct ion_heap *), GFP_KERNEL);
	if (!heaps) {
		ret = -ENOMEM;
		goto freepdatas;
	}

	/* create the heaps as specified in the board dts file */
	for (i = 0; i < num_valid; i++) {
		struct ion_platform_heap *heap_data = &pdatas[i];
		heaps[i] = sdrv_ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			heaps[i] = NULL;
			continue;
		} else {
			if (heap_data->size)
				pr_info("SDRV ION %s created at 0x%llx with size %zx\n",
					heap_data->name,
					heap_data->base,
					heap_data->size);
			else
				pr_info("SDRV ION %s created, but size is 0\n",
					heap_data->name);
		}

		ion_device_add_heap(heaps[i]);
	}

	platform_set_drvdata(pdev, heaps);

freepdatas:
	kfree(pdatas);
out:
	return ret;
}

static int sdrv_ion_remove(struct platform_device *pdev)
{
	struct sdrv_ion_device_data *idev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < idev->num_heaps; i++)
		kfree(idev->heaps[i]);

	/* Todo: can release additional resources like debugfs */
	kfree(idev->heaps);

	kfree(idev);
	return 0;
}

static struct of_device_id sdrv_ion_match_table[] = {
	{.compatible = SEMIDRIVE_COMPAT_STR},
	{},
};

static struct platform_driver sdrv_ion_driver = {
	.probe = sdrv_ion_probe,
	.remove = sdrv_ion_remove,
	.driver = {
		.name = "ion-sdrv",
		.of_match_table = sdrv_ion_match_table,
	},
};

static int __init sdrv_ion_init(void)
{
	return platform_driver_register(&sdrv_ion_driver);
}

static void __exit sdrv_ion_exit(void)
{
	platform_driver_unregister(&sdrv_ion_driver);
}
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>


static int __init rmem_carveout_setup(struct reserved_mem *rmem)
{
	phys_addr_t align = PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);
	phys_addr_t mask = align - 1;
	unsigned long node = rmem->fdt_node;
	int err;
	sdrv_heap_mem.base = 0;
	sdrv_heap_mem.size = 0;

	if ((rmem->base & mask) || (rmem->size & mask)) {
		pr_err("Reserved memory: incorrect alignment of CARVEOUT region\n");
		return -EINVAL;
	}

	pr_info("Reserved memory: created  CARVEOUT memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);
	sdrv_heap_mem.base = rmem->base;
	sdrv_heap_mem.size= rmem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(carveout, "sdrv,ion", rmem_carveout_setup);
#endif
subsys_initcall(sdrv_ion_init);
module_exit(sdrv_ion_exit);
