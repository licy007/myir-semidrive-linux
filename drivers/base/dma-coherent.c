// SPDX-License-Identifier: GPL-2.0
/*
 * Coherent per-device memory handling.
 * Borrowed from i386
 */
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/kthread.h>

#if defined(CONFIG_GK20A_PCI)
#define RESIZE_MAGIC 0xC11A900d
struct heap_info {
	int magic;
	char *name;
	/* number of chunks memory to manage in */
	unsigned int num_chunks;
	/* dev to manage cma/coherent memory allocs, if resize allowed */
	struct device dev;
	/* device to allocate memory from cma */
	struct device *cma_dev;
	/* lock to synchronise heap resizing */
	struct mutex resize_lock;
	/* CMA chunk size if resize supported */
	size_t cma_chunk_size;
	/* heap current base */
	phys_addr_t curr_base;
	/* heap current allocated memory in bytes */
	size_t curr_used;
	/* heap current length */
	size_t curr_len;
	/* heap lowest base */
	phys_addr_t cma_base;
	/* heap max length */
	size_t cma_len;
	size_t rem_chunk_size;
	struct dentry *dma_debug_root;
	int (*update_resize_cfg)(phys_addr_t , size_t);
	/* The timer used to wakeup the shrink thread */
	struct timer_list shrink_timer;
	/* Pointer to the current shrink thread for this resizable heap */
	struct task_struct *task;
	unsigned long shrink_interval;
	size_t floor_size;
};
#define DMA_BUF_ALIGNMENT 8
#endif

struct dma_coherent_mem {
	void		*virt_base;
	dma_addr_t	device_base;
	unsigned long	pfn_base;
	int		size;
	int		flags;
	unsigned long	*bitmap;
	spinlock_t	spinlock;
	bool		use_dev_dma_pfn_offset;
};

#if defined(CONFIG_GK20A_PCI)
static int shrink_thread(void *arg);
static void shrink_resizable_heap(struct heap_info *h);
static int heap_resize_locked(struct heap_info *h, bool skip_vpr_config);
static void release_from_contiguous_heap(struct heap_info *h, phys_addr_t base,
		size_t len);
static int update_vpr_config(struct heap_info *h);
#define RESIZE_DEFAULT_SHRINK_AGE 3

static phys_addr_t alloc_from_contiguous_heap(
		struct heap_info *h,
		phys_addr_t base, size_t len)
{
	size_t count;
	struct page *page;
	unsigned long order;

	dev_dbg(h->cma_dev, "req at base (%pa) size (0x%zx)\n",
			&base, len);
	order = get_order(len);
	count = PAGE_ALIGN(len) >> PAGE_SHIFT;
	page = dma_alloc_at_from_contiguous(h->cma_dev, count,
			order, base, true);
	if (!page) {
		dev_err(h->cma_dev, "dma_alloc_at_from_contiguous failed\n");
		goto dma_alloc_err;
	}

	base = page_to_phys(page);
	dev_dbg(h->cma_dev, "allocated at base (%pa) size (0x%zx)\n",
			&base, len);
	BUG_ON(base < h->cma_base || base - h->cma_base + len > h->cma_len);
	return base;

dma_alloc_err:
	return DMA_ERROR_CODE;
}

static void release_from_contiguous_heap(
		struct heap_info *h,
		phys_addr_t base, size_t len)
{
	struct page *page = phys_to_page(base);
	size_t count = PAGE_ALIGN(len) >> PAGE_SHIFT;

	dma_release_from_contiguous(h->cma_dev, page, count);
	dev_dbg(h->cma_dev, "released at base (%pa) size (0x%zx)\n",
			&base, len);
}

static void get_first_and_last_idx(struct heap_info *h,
		int *first_alloc_idx, int *last_alloc_idx)
{
	if (!h->curr_len) {
		*first_alloc_idx = -1;
		*last_alloc_idx = h->num_chunks;
	} else {
		*first_alloc_idx = div_u64(h->curr_base - h->cma_base,
				h->cma_chunk_size);
		*last_alloc_idx = div_u64(h->curr_base - h->cma_base +
				h->curr_len + h->cma_chunk_size -
				h->rem_chunk_size,
				h->cma_chunk_size) - 1;
	}
}

static void update_alloc_range(struct heap_info *h)
{
	if (!h->curr_len)
		h->dev.dma_mem->size = 0;
	else
		h->dev.dma_mem->size = (h->curr_base - h->cma_base +
				h->curr_len) >> PAGE_SHIFT;
}

static int heap_resize_locked(struct heap_info *h, bool skip_vpr_config)
{
	phys_addr_t base = -1;
	size_t len = h->cma_chunk_size;
	phys_addr_t prev_base = h->curr_base;
	size_t prev_len = h->curr_len;
	int alloc_at_idx = 0;
	int first_alloc_idx;
	int last_alloc_idx;
	phys_addr_t start_addr = h->cma_base;

	get_first_and_last_idx(h, &first_alloc_idx, &last_alloc_idx);
	pr_debug("req resize, fi=%d,li=%d\n", first_alloc_idx, last_alloc_idx);

	/* All chunks are in use. Can't grow it. */
	if (first_alloc_idx == 0 && last_alloc_idx == h->num_chunks - 1)
		return -ENOMEM;

	/* All chunks are free. Attempt to allocate the first chunk. */
	if (first_alloc_idx == -1) {
		base = alloc_from_contiguous_heap(h, start_addr, len);
		if (base == start_addr)
			goto alloc_success;
		BUG_ON(!dma_mapping_error(h->cma_dev, base));
	}

	/* Free chunk before previously allocated chunk. Attempt
	 * to allocate only immediate previous chunk.
	 */
	if (first_alloc_idx > 0) {
		alloc_at_idx = first_alloc_idx - 1;
		start_addr = alloc_at_idx * h->cma_chunk_size + h->cma_base;
		base = alloc_from_contiguous_heap(h, start_addr, len);
		if (base == start_addr)
			goto alloc_success;
		BUG_ON(!dma_mapping_error(h->cma_dev, base));
	}

	/* Free chunk after previously allocated chunk. */
	if (last_alloc_idx < h->num_chunks - 1) {
		alloc_at_idx = last_alloc_idx + 1;
		len = (alloc_at_idx == h->num_chunks - 1) ?
			h->rem_chunk_size : h->cma_chunk_size;
		start_addr = alloc_at_idx * h->cma_chunk_size + h->cma_base;
		base = alloc_from_contiguous_heap(h, start_addr, len);
		if (base == start_addr)
			goto alloc_success;
		BUG_ON(!dma_mapping_error(h->cma_dev, base));
	}

	if (dma_mapping_error(h->cma_dev, base))
		dev_err(&h->dev,
				"Failed to allocate contiguous memory on heap grow req\n");

	return -ENOMEM;

alloc_success:
	if (!h->curr_len || h->curr_base > base)
		h->curr_base = base;
	h->curr_len += len;

	if (!skip_vpr_config && update_vpr_config(h))
		goto fail_update;

	dev_dbg(&h->dev,
			"grow heap base from=%pa to=%pa,"
			" len from=0x%zx to=0x%zx\n",
			&prev_base, &h->curr_base, prev_len, h->curr_len);
	return 0;

fail_update:
	release_from_contiguous_heap(h, base, len);
	h->curr_base = prev_base;
	h->curr_len = prev_len;
	return -ENOMEM;
}

static int update_vpr_config(struct heap_info *h)
{
	/* Handle VPR configuration updates*/
	if (h->update_resize_cfg) {
		int err = h->update_resize_cfg(h->curr_base, h->curr_len);
		if (err) {
			dev_err(&h->dev, "Failed to update heap resize\n");
			return err;
		}
		dev_dbg(&h->dev, "update vpr base to %pa, size=%zx\n",
				&h->curr_base, h->curr_len);
	}

	update_alloc_range(h);
	return 0;
}

bool dma_is_coherent_dev(struct device *dev)
{
	struct heap_info *h;

	if (!dev)
		return false;
	h = dev_get_drvdata(dev);
	if (!h)
		return false;
	if (h->magic != RESIZE_MAGIC)
		return false;
	return true;
}
EXPORT_SYMBOL(dma_is_coherent_dev);

int dma_set_resizable_heap_floor_size(struct device *dev, size_t floor_size)
{
	int ret = 0;
	struct heap_info *h = NULL;
	phys_addr_t orig_base, prev_base, left_chunks_base, right_chunks_base;
	size_t orig_len, prev_len, left_chunks_len, right_chunks_len;

	if (!dma_is_coherent_dev(dev))
		return -ENODEV;

	h = dev_get_drvdata(dev);
	if (!h)
		return -ENOENT;

	mutex_lock(&h->resize_lock);
	orig_base = h->curr_base;
	orig_len = h->curr_len;
	right_chunks_base = h->curr_base + h->curr_len;
	left_chunks_len = right_chunks_len = 0;

	h->floor_size = floor_size > h->cma_len ? h->cma_len : floor_size;
	while (h->curr_len < h->floor_size) {
		prev_base = h->curr_base;
		prev_len = h->curr_len;

		ret = heap_resize_locked(h, true);
		if (ret)
			goto fail_set_floor;

		if (h->curr_base < prev_base) {
			left_chunks_base = h->curr_base;
			left_chunks_len += (h->curr_len - prev_len);
		} else {
			right_chunks_len += (h->curr_len - prev_len);
		}
	}

	ret = update_vpr_config(h);
	if (!ret) {
		dev_dbg(&h->dev,
				"grow heap base from=%pa to=%pa,"
				" len from=0x%zx to=0x%zx\n",
				&orig_base, &h->curr_base, orig_len, h->curr_len);
		goto success_set_floor;
	}

fail_set_floor:
	if (left_chunks_len != 0)
		release_from_contiguous_heap(h, left_chunks_base,
				left_chunks_len);
	if (right_chunks_len != 0)
		release_from_contiguous_heap(h, right_chunks_base,
				right_chunks_len);
	h->curr_base = orig_base;
	h->curr_len = orig_len;

success_set_floor:
	if (h->task)
		mod_timer(&h->shrink_timer, jiffies + h->shrink_interval);
	mutex_unlock(&h->resize_lock);
	if (!h->task)
		shrink_resizable_heap(h);
	return ret;
}
EXPORT_SYMBOL(dma_set_resizable_heap_floor_size);

static void shrink_resizable_heap(struct heap_info *h)
{
#if 0
	bool unlock = false;
	int first_alloc_idx, last_alloc_idx;

check_next_chunk:
	if (unlock) {
		mutex_unlock(&h->resize_lock);
		cond_resched();
	}
	mutex_lock(&h->resize_lock);
	unlock = true;
	if (h->curr_len <= h->floor_size)
		goto out_unlock;
	get_first_and_last_idx(h, &first_alloc_idx, &last_alloc_idx);
	/* All chunks are free. Exit. */
	if (first_alloc_idx == -1)
		goto out_unlock;
	if (shrink_chunk_locked(h, first_alloc_idx))
		goto check_next_chunk;
	/* Only one chunk is in use. */
	if (first_alloc_idx == last_alloc_idx)
		goto out_unlock;
	if (shrink_chunk_locked(h, last_alloc_idx))
		goto check_next_chunk;

out_unlock:
	mutex_unlock(&h->resize_lock);
#endif
}

static void shrink_timeout(unsigned long __data)
{
	struct task_struct *p = (struct task_struct *) __data;

	wake_up_process(p);
}

static int shrink_thread(void *arg)
{
	struct heap_info *h = arg;

	/*
	 * Set up an interval timer which can be used to trigger a commit wakeup
	 * after the commit interval expires
	 */
	setup_timer(&h->shrink_timer, shrink_timeout,
			(unsigned long)current);
	h->task = current;

	while (1) {
		if (kthread_should_stop())
			break;

		shrink_resizable_heap(h);
		/* resize done. goto sleep */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}
#endif

static struct dma_coherent_mem *dma_coherent_default_memory __ro_after_init;

static inline struct dma_coherent_mem *dev_get_coherent_memory(struct device *dev)
{
	if (dev && dev->dma_mem)
		return dev->dma_mem;
	return NULL;
}

static inline dma_addr_t dma_get_device_base(struct device *dev,
					     struct dma_coherent_mem * mem)
{
	if (mem->use_dev_dma_pfn_offset)
		return (mem->pfn_base - dev->dma_pfn_offset) << PAGE_SHIFT;
	else
		return mem->device_base;
}

static int dma_init_coherent_memory(
	phys_addr_t phys_addr, dma_addr_t device_addr, size_t size, int flags,
	struct dma_coherent_mem **mem)
{
	struct dma_coherent_mem *dma_mem = NULL;
	void __iomem *mem_base = NULL;
	int pages = size >> PAGE_SHIFT;
	int bitmap_size = BITS_TO_LONGS(pages) * sizeof(long);
	int ret;

	if (!size) {
		ret = -EINVAL;
		goto out;
	}

	mem_base = memremap(phys_addr, size, MEMREMAP_WC);
	if (!mem_base) {
		ret = -EINVAL;
		goto out;
	}
	dma_mem = kzalloc(sizeof(struct dma_coherent_mem), GFP_KERNEL);
	if (!dma_mem) {
		ret = -ENOMEM;
		goto out;
	}
	dma_mem->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!dma_mem->bitmap) {
		ret = -ENOMEM;
		goto out;
	}

	dma_mem->virt_base = mem_base;
	dma_mem->device_base = device_addr;
	dma_mem->pfn_base = PFN_DOWN(phys_addr);
	dma_mem->size = pages;
	dma_mem->flags = flags;
	spin_lock_init(&dma_mem->spinlock);

	*mem = dma_mem;
	return 0;

out:
	kfree(dma_mem);
	if (mem_base)
		memunmap(mem_base);
	return ret;
}

static void dma_release_coherent_memory(struct dma_coherent_mem *mem)
{
	if (!mem)
		return;

	memunmap(mem->virt_base);
	kfree(mem->bitmap);
	kfree(mem);
}

#if defined(CONFIG_GK20A_PCI)
static int declare_coherent_heap(struct device *dev, phys_addr_t base,
		size_t size, int map)
{
	int err;
	//int flags = map ? DMA_MEMORY_MAP : DMA_MEMORY_NOMAP;
	int flags = DMA_MEMORY_EXCLUSIVE;

	BUG_ON(dev->dma_mem);
	dma_set_coherent_mask(dev,  DMA_BIT_MASK(64));
	err = dma_declare_coherent_memory(dev, 0,
			base, size, flags);
	if (err & flags) {
		dev_dbg(dev, "dma coherent mem base (%pa) size (0x%zx) %x\n",
				&base, size, flags);
		return 0;
	}
	dev_err(dev, "declare dma coherent_mem fail %pa 0x%zx %x\n",
			&base, size, flags);
	return -ENOMEM;
}

int dma_declare_coherent_resizable_cma_memory(struct device *dev,
		struct dma_declare_info *dma_info)
{
#ifdef CONFIG_DMA_CMA
	int err = 0;
	struct heap_info *heap_info = NULL;
	struct dma_contiguous_stats stats;

	if (!dev || !dma_info || !dma_info->name || !dma_info->cma_dev)
		return -EINVAL;

	heap_info = kzalloc(sizeof(*heap_info), GFP_KERNEL);
	if (!heap_info)
		return -ENOMEM;

	heap_info->magic = RESIZE_MAGIC;
	heap_info->name = kmalloc(strlen(dma_info->name) + 1, GFP_KERNEL);
	if (!heap_info->name) {
		kfree(heap_info);
		return -ENOMEM;
	}

	dma_get_contiguous_stats(dma_info->cma_dev, &stats);
	pr_info("resizable heap=%s, base=%pa, size=0x%zx\n",
			dma_info->name, &stats.base, stats.size);
	strcpy(heap_info->name, dma_info->name);
	dev_set_name(dev, "dma-%s", heap_info->name);
	heap_info->cma_dev = dma_info->cma_dev;
	heap_info->cma_chunk_size = dma_info->size ? : stats.size;
	heap_info->cma_base = stats.base;
	heap_info->cma_len = stats.size;
	heap_info->curr_base = stats.base;
	dev_set_name(heap_info->cma_dev, "cma-%s-heap", heap_info->name);
	mutex_init(&heap_info->resize_lock);
	if (heap_info->cma_len < heap_info->cma_chunk_size) {
		dev_err(dev, "error cma_len(0x%zx) < cma_chunk_size(0x%zx)\n",
				heap_info->cma_len, heap_info->cma_chunk_size);
		err = -EINVAL;
		goto fail;
	}

	heap_info->num_chunks = div64_u64_rem(heap_info->cma_len,
			(u64)heap_info->cma_chunk_size, (u64 *)&heap_info->rem_chunk_size);
	if (heap_info->rem_chunk_size) {
		heap_info->num_chunks++;
		dev_info(dev, "heap size is not multiple of cma_chunk_size "
				"heap_info->num_chunks (%d) rem_chunk_size(0x%zx)\n",
				heap_info->num_chunks, heap_info->rem_chunk_size);
	} else
		heap_info->rem_chunk_size = heap_info->cma_chunk_size;

	dev_set_name(&heap_info->dev, "%s-heap", heap_info->name);

	if (dma_info->notifier.ops)
		heap_info->update_resize_cfg =
			dma_info->notifier.ops->resize;

	dev_set_drvdata(dev, heap_info);
	//dma_debugfs_init(dev, heap_info);

	if (declare_coherent_heap(&heap_info->dev,
				heap_info->cma_base, heap_info->cma_len,
				(dma_info->notifier.ops &&
				 dma_info->notifier.ops->resize) ? 0 : 1))
		goto declare_fail;
	heap_info->dev.dma_mem->size = 0;
	heap_info->shrink_interval = HZ * RESIZE_DEFAULT_SHRINK_AGE;
	kthread_run(shrink_thread, heap_info, "%s-shrink_thread",
			heap_info->name);

	if (dma_info->notifier.ops && dma_info->notifier.ops->resize)
		dma_contiguous_enable_replace_pages(dma_info->cma_dev);
	pr_info("resizable cma heap=%s create successful", heap_info->name);
	return 0;
declare_fail:
	kfree(heap_info->name);
fail:
	kfree(heap_info);
	return err;
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL(dma_declare_coherent_resizable_cma_memory);
#endif

static int dma_assign_coherent_memory(struct device *dev,
				      struct dma_coherent_mem *mem)
{
	if (!dev)
		return -ENODEV;

	if (dev->dma_mem)
		return -EBUSY;

	dev->dma_mem = mem;
	return 0;
}

int dma_declare_coherent_memory(struct device *dev, phys_addr_t phys_addr,
				dma_addr_t device_addr, size_t size, int flags)
{
	struct dma_coherent_mem *mem;
	int ret;

	ret = dma_init_coherent_memory(phys_addr, device_addr, size, flags, &mem);
	if (ret)
		return ret;

	ret = dma_assign_coherent_memory(dev, mem);
	if (ret)
		dma_release_coherent_memory(mem);
	return ret;
}
EXPORT_SYMBOL(dma_declare_coherent_memory);

void dma_release_declared_memory(struct device *dev)
{
	struct dma_coherent_mem *mem = dev->dma_mem;

	if (!mem)
		return;
	dma_release_coherent_memory(mem);
	dev->dma_mem = NULL;
}
EXPORT_SYMBOL(dma_release_declared_memory);

void *dma_mark_declared_memory_occupied(struct device *dev,
					dma_addr_t device_addr, size_t size)
{
	struct dma_coherent_mem *mem = dev->dma_mem;
	unsigned long flags;
	int pos, err;

	size += device_addr & ~PAGE_MASK;

	if (!mem)
		return ERR_PTR(-EINVAL);

	spin_lock_irqsave(&mem->spinlock, flags);
	pos = PFN_DOWN(device_addr - dma_get_device_base(dev, mem));
	err = bitmap_allocate_region(mem->bitmap, pos, get_order(size));
	spin_unlock_irqrestore(&mem->spinlock, flags);

	if (err != 0)
		return ERR_PTR(err);
	return mem->virt_base + (pos << PAGE_SHIFT);
}
EXPORT_SYMBOL(dma_mark_declared_memory_occupied);

static void *__dma_alloc_from_coherent(struct dma_coherent_mem *mem,
		ssize_t size, dma_addr_t *dma_handle)
{
	int order = get_order(size);
	unsigned long flags;
	int pageno;
	void *ret;

	spin_lock_irqsave(&mem->spinlock, flags);

	if (unlikely(size > (mem->size << PAGE_SHIFT)))
		goto err;

	pageno = bitmap_find_free_region(mem->bitmap, mem->size, order);
	if (unlikely(pageno < 0))
		goto err;

	/*
	 * Memory was found in the coherent area.
	 */
	*dma_handle = mem->device_base + (pageno << PAGE_SHIFT);
	ret = mem->virt_base + (pageno << PAGE_SHIFT);
	spin_unlock_irqrestore(&mem->spinlock, flags);
	memset(ret, 0, size);
	return ret;
err:
	spin_unlock_irqrestore(&mem->spinlock, flags);
	return NULL;
}

/**
 * dma_alloc_from_dev_coherent() - allocate memory from device coherent pool
 * @dev:	device from which we allocate memory
 * @size:	size of requested memory area
 * @dma_handle:	This will be filled with the correct dma handle
 * @ret:	This pointer will be filled with the virtual address
 *		to allocated area.
 *
 * This function should be only called from per-arch dma_alloc_coherent()
 * to support allocation from per-device coherent memory pools.
 *
 * Returns 0 if dma_alloc_coherent should continue with allocating from
 * generic memory areas, or !0 if dma_alloc_coherent should return @ret.
 */
int dma_alloc_from_dev_coherent(struct device *dev, ssize_t size,
		dma_addr_t *dma_handle, void **ret)
{
	struct dma_coherent_mem *mem = dev_get_coherent_memory(dev);

	if (!mem)
		return 0;

	*ret = __dma_alloc_from_coherent(mem, size, dma_handle);
	if (*ret)
		return 1;

	/*
	 * In the case where the allocation can not be satisfied from the
	 * per-device area, try to fall back to generic memory if the
	 * constraints allow it.
	 */
	return mem->flags & DMA_MEMORY_EXCLUSIVE;
}
EXPORT_SYMBOL(dma_alloc_from_dev_coherent);

void *dma_alloc_from_global_coherent(ssize_t size, dma_addr_t *dma_handle)
{
	if (!dma_coherent_default_memory)
		return NULL;

	return __dma_alloc_from_coherent(dma_coherent_default_memory, size,
			dma_handle);
}

static int __dma_release_from_coherent(struct dma_coherent_mem *mem,
				       int order, void *vaddr)
{
	if (mem && vaddr >= mem->virt_base && vaddr <
		   (mem->virt_base + (mem->size << PAGE_SHIFT))) {
		int page = (vaddr - mem->virt_base) >> PAGE_SHIFT;
		unsigned long flags;

		spin_lock_irqsave(&mem->spinlock, flags);
		bitmap_release_region(mem->bitmap, page, order);
		spin_unlock_irqrestore(&mem->spinlock, flags);
		return 1;
	}
	return 0;
}

/**
 * dma_release_from_dev_coherent() - free memory to device coherent memory pool
 * @dev:	device from which the memory was allocated
 * @order:	the order of pages allocated
 * @vaddr:	virtual address of allocated pages
 *
 * This checks whether the memory was allocated from the per-device
 * coherent memory pool and if so, releases that memory.
 *
 * Returns 1 if we correctly released the memory, or 0 if the caller should
 * proceed with releasing memory from generic pools.
 */
int dma_release_from_dev_coherent(struct device *dev, int order, void *vaddr)
{
	struct dma_coherent_mem *mem = dev_get_coherent_memory(dev);

	return __dma_release_from_coherent(mem, order, vaddr);
}
EXPORT_SYMBOL(dma_release_from_dev_coherent);

int dma_release_from_global_coherent(int order, void *vaddr)
{
	if (!dma_coherent_default_memory)
		return 0;

	return __dma_release_from_coherent(dma_coherent_default_memory, order,
			vaddr);
}

static int __dma_mmap_from_coherent(struct dma_coherent_mem *mem,
		struct vm_area_struct *vma, void *vaddr, size_t size, int *ret)
{
	if (mem && vaddr >= mem->virt_base && vaddr + size <=
		   (mem->virt_base + (mem->size << PAGE_SHIFT))) {
		unsigned long off = vma->vm_pgoff;
		int start = (vaddr - mem->virt_base) >> PAGE_SHIFT;
		int user_count = vma_pages(vma);
		int count = PAGE_ALIGN(size) >> PAGE_SHIFT;

		*ret = -ENXIO;
		if (off < count && user_count <= count - off) {
			unsigned long pfn = mem->pfn_base + start + off;
			*ret = remap_pfn_range(vma, vma->vm_start, pfn,
					       user_count << PAGE_SHIFT,
					       vma->vm_page_prot);
		}
		return 1;
	}
	return 0;
}

/**
 * dma_mmap_from_dev_coherent() - mmap memory from the device coherent pool
 * @dev:	device from which the memory was allocated
 * @vma:	vm_area for the userspace memory
 * @vaddr:	cpu address returned by dma_alloc_from_dev_coherent
 * @size:	size of the memory buffer allocated
 * @ret:	result from remap_pfn_range()
 *
 * This checks whether the memory was allocated from the per-device
 * coherent memory pool and if so, maps that memory to the provided vma.
 *
 * Returns 1 if we correctly mapped the memory, or 0 if the caller should
 * proceed with mapping memory from generic pools.
 */
int dma_mmap_from_dev_coherent(struct device *dev, struct vm_area_struct *vma,
			   void *vaddr, size_t size, int *ret)
{
	struct dma_coherent_mem *mem = dev_get_coherent_memory(dev);

	return __dma_mmap_from_coherent(mem, vma, vaddr, size, ret);
}
EXPORT_SYMBOL(dma_mmap_from_dev_coherent);

int dma_mmap_from_global_coherent(struct vm_area_struct *vma, void *vaddr,
				   size_t size, int *ret)
{
	if (!dma_coherent_default_memory)
		return 0;

	return __dma_mmap_from_coherent(dma_coherent_default_memory, vma,
					vaddr, size, ret);
}

/*
 * Support for reserved memory regions defined in device tree
 */
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

static struct reserved_mem *dma_reserved_default_memory __initdata;

static int rmem_dma_device_init(struct reserved_mem *rmem, struct device *dev)
{
	struct dma_coherent_mem *mem = rmem->priv;
	int ret;

	if (!mem) {
		ret = dma_init_coherent_memory(rmem->base, rmem->base,
					       rmem->size,
					       DMA_MEMORY_EXCLUSIVE, &mem);
		if (ret) {
			pr_err("Reserved memory: failed to init DMA memory pool at %pa, size %ld MiB\n",
				&rmem->base, (unsigned long)rmem->size / SZ_1M);
			return ret;
		}
	}
	mem->use_dev_dma_pfn_offset = true;
	rmem->priv = mem;
	dma_assign_coherent_memory(dev, mem);
	return 0;
}

static void rmem_dma_device_release(struct reserved_mem *rmem,
				    struct device *dev)
{
	if (dev)
		dev->dma_mem = NULL;
}

static const struct reserved_mem_ops rmem_dma_ops = {
	.device_init	= rmem_dma_device_init,
	.device_release	= rmem_dma_device_release,
};

static int __init rmem_dma_setup(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;

	if (of_get_flat_dt_prop(node, "reusable", NULL))
		return -EINVAL;

#ifdef CONFIG_ARM
	if (!of_get_flat_dt_prop(node, "no-map", NULL)) {
		pr_err("Reserved memory: regions without no-map are not yet supported\n");
		return -EINVAL;
	}

	if (of_get_flat_dt_prop(node, "linux,dma-default", NULL)) {
		WARN(dma_reserved_default_memory,
		     "Reserved memory: region for default DMA coherent area is redefined\n");
		dma_reserved_default_memory = rmem;
	}
#endif

	rmem->ops = &rmem_dma_ops;
	pr_info("Reserved memory: created DMA memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

static int __init dma_init_reserved_memory(void)
{
	const struct reserved_mem_ops *ops;
	int ret;

	if (!dma_reserved_default_memory)
		return -ENOMEM;

	ops = dma_reserved_default_memory->ops;

	/*
	 * We rely on rmem_dma_device_init() does not propagate error of
	 * dma_assign_coherent_memory() for "NULL" device.
	 */
	ret = ops->device_init(dma_reserved_default_memory, NULL);

	if (!ret) {
		dma_coherent_default_memory = dma_reserved_default_memory->priv;
		pr_info("DMA: default coherent area is set\n");
	}

	return ret;
}

core_initcall(dma_init_reserved_memory);

RESERVEDMEM_OF_DECLARE(dma, "shared-dma-pool", rmem_dma_setup);
#endif
