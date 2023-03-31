/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MM_CMA_H__
#define __MM_CMA_H__

struct cma {
	unsigned long   base_pfn;
	unsigned long   count;
	unsigned long   *bitmap;
	unsigned int order_per_bit; /* Order of pages represented by one bit */
	struct mutex    lock;
#ifdef CONFIG_CMA_DEBUGFS
	struct hlist_head mem_head;
	spinlock_t mem_head_lock;
#endif
	const char *name;
};

extern struct cma cma_areas[MAX_CMA_AREAS];
extern unsigned cma_area_count;

static inline unsigned long cma_bitmap_maxno(struct cma *cma)
{
	return cma->count >> cma->order_per_bit;
}

#ifdef CONFIG_SEMIDRIVE_CMA_MEM_RECORD
struct cma_record {
	unsigned long base_pfn;
	unsigned long count;
	unsigned long *mem_record_bitmap;
	struct list_head cma_mem_list;
	struct list_head cma_mem_sys_list;
	struct list_head cma_free_list;
	spinlock_t lock;
};

extern struct static_key_false cma_mem_record_inited;
extern int cma_sys_release_record(unsigned long pfn_start,
			unsigned long pfn_end);
extern void cma_sys_alloc_record(struct page *page, unsigned int order);
extern struct cma_record *get_cma_record_areas(unsigned long start_pfn,
			unsigned long end_pfn);
#endif

#endif
