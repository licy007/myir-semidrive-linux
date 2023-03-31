/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <linux/fs.h>
#include "sram_conf.h"
#include "ce.h"
#include "ce_reg.h"
#include "sx_pke_conf.h"
#include "sx_errors.h"

#define CE_HEAP_MEM_NODE_ITEM_NUM_MAX       32
#define CE_HEAP_SIZE_OF_PAGE                4096

#define CE_HEAP_CAPACITY_OF_PAGE            (CE_HEAP_SIZE_OF_PAGE/CE_HEAP_MALLOC_MAX_SIZE)
#define CE_HEAP_PAGE_NUM                    (CE_HEAP_MEM_NODE_ITEM_NUM_MAX/CE_HEAP_CAPACITY_OF_PAGE)

/** Helper to build a ::block_t from a pair of data address and size */

struct ce_heap {
	size_t item_num;
	size_t ce_capacity_of_page;
	size_t ce_page_num;
	uint32_t free;
	bool is_init;
	uint8_t *ce_pageaddr[CE_HEAP_PAGE_NUM];
	struct mem_node mem_node_list[CE_HEAP_MEM_NODE_ITEM_NUM_MAX];
};

uint32_t enter_ce_heap_init_time = 0;
static DEFINE_SPINLOCK(ce_heap_lock);
static struct ce_heap inheap;

uint32_t ce_inheap_init(void)
{
	uint8_t i, j;
	inheap.item_num = CE_HEAP_MEM_NODE_ITEM_NUM_MAX;
	inheap.ce_capacity_of_page = CE_HEAP_CAPACITY_OF_PAGE;
	inheap.ce_page_num = CE_HEAP_PAGE_NUM;
	inheap.is_init = 1;
	inheap.free = 0;
	enter_ce_heap_init_time++;
	pr_info(" enter ce_inheap_init \n");

	for (i = 0; i < inheap.ce_page_num ; i++) {
		inheap.ce_pageaddr[i] = (uint8_t *)get_zeroed_page(GFP_DMA);

		if (inheap.ce_pageaddr[i] == NULL) {
			pr_err("get_zeroed_page is wrong in %s, %d,", __func__, __LINE__);

			for (j = 0; j < i ; j++) {
				free_pages((unsigned long)inheap.ce_pageaddr[j], 0);
				inheap.ce_pageaddr[i] = NULL;
			}

			inheap.is_init = 0;
			return 1;
		}
	}

	for (i = 0; i < inheap.item_num; i++) {
		inheap.mem_node_list[i].size = CE_HEAP_MALLOC_MAX_SIZE;
		inheap.mem_node_list[i].nid = i;
		inheap.mem_node_list[i].next = i + 1;
		inheap.mem_node_list[i].ptr = NULL;
	}

	return 0;
}

struct mem_node *ce_malloc(size_t size)
{
	uint32_t status = 0;
	spin_lock_saved_state_t states;
	struct mem_node *return_node = NULL;

	if (unlikely(size > CE_HEAP_MALLOC_MAX_SIZE)) {
		pr_err("apply space is too big\n");
		return NULL;
	}

	if (unlikely(inheap.is_init == 0)) {
		status = ce_inheap_init();

		if (status) {
			pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
			return NULL;
		}
	}

	spin_lock_irqsave(&ce_heap_lock, states);

	if (likely(inheap.free < inheap.item_num)) {
		inheap.mem_node_list[inheap.free].ptr = inheap.ce_pageaddr[inheap.free / inheap.ce_capacity_of_page]
												+ CE_HEAP_MALLOC_MAX_SIZE * (inheap.free % inheap.ce_capacity_of_page);
		return_node = &(inheap.mem_node_list[inheap.free]);
		inheap.free = inheap.mem_node_list[inheap.free].next;
		// pr_info("%d is be used\n",inheap.free);
	}
	else {
		pr_err("one page is lack\n");
		spin_unlock_irqrestore(&ce_heap_lock, states);
		return NULL;
	}

	memset(return_node->ptr, 0, CE_HEAP_MALLOC_MAX_SIZE);
	spin_unlock_irqrestore(&ce_heap_lock, states);
	return return_node;
	//malloc();
}

void ce_free(struct mem_node *mem_node)
{
	spin_lock_saved_state_t states;
	spin_lock_irqsave(&ce_heap_lock, states);

	if (likely(mem_node != NULL)) {
		// pr_info("%d is be free\n",mem_node->nid);
		mem_node->next = inheap.free;
		inheap.free = mem_node->nid;
		mem_node->ptr = NULL;
	}

	spin_unlock_irqrestore(&ce_heap_lock, states);
}

void ce_inheap_free(void)
{
	uint8_t i;
	pr_info("enter_ce_heap_init_time is %d\n", enter_ce_heap_init_time);
	enter_ce_heap_init_time = 0;

	if (inheap.is_init == 1) {
		inheap.is_init = 0;
		inheap.free = 0;

		for (i = 0; i < inheap.ce_page_num ; i++) {
			free_pages((unsigned long)inheap.ce_pageaddr[i], 0);
			inheap.ce_pageaddr[i] = NULL;
		}

		// pr_info("page is been free\n");
	}
}

void ce_inheap_clear(void)
{
	pr_info("ce inheap is already cleared\n");
	memset(&inheap, 0, sizeof(inheap));
}

bool g_ce_inited = false;
volatile uint32_t g_int_flag = 0;

#define APB_SYS_CNT_RO_BASE (0x31400000)

#define RECODE_COUNT_NUM_MAX 256
typedef struct cout_info_ce {
	uint32_t cout_value;
	uint32_t cout_num;
} cout_info_ce_t;

typedef struct recode_info_ce {
	cout_info_ce_t cout_info[RECODE_COUNT_NUM_MAX];
	uint32_t r_index;
} recode_info_ce_t;

recode_info_ce_t recode_info_ce_a;

void __iomem *sys_cnt_base_ce = NULL;

void of_set_sys_cnt_ce(int i)
{

	if (recode_info_ce_a.r_index == RECODE_COUNT_NUM_MAX) {
		return;
	}

	if (sys_cnt_base_ce == NULL) {
		sys_cnt_base_ce = ioremap(APB_SYS_CNT_RO_BASE, 0x1000);
	}

	recode_info_ce_a.cout_info[recode_info_ce_a.r_index].cout_value = (readl(sys_cnt_base_ce)) / 3;
	recode_info_ce_a.cout_info[recode_info_ce_a.r_index].cout_num = i;
	recode_info_ce_a.r_index++;
}

uint32_t of_get_sys_cnt_ce(void)
{
	uint32_t ret;

	if (sys_cnt_base_ce == NULL) {
		sys_cnt_base_ce = ioremap(APB_SYS_CNT_RO_BASE, 0x1000);
	}

	ret = (readl(sys_cnt_base_ce)) / 3;
	return ret;
}



addr_t addr_switch_to_ce(uint32_t vce_id, ce_addr_type_t addr_type_s, addr_t addr)
{
	addr_t addr_switch = addr;

	if (EXT_MEM == addr_type_s) {
		addr_switch = addr;
	}
	else if (SRAM_PUB == addr_type_s || SRAM_SEC == addr_type_s) {
		addr_switch = get_sram_base(vce_id);
		addr_switch = ~addr_switch & addr;
	}

	return addr_switch;
}

int32_t ce_init(uint32_t vce_id)
{
	return 0;
}

int32_t ce_globle_init(void)
{

	if (g_ce_inited) {
		return 0;
	}

	pr_debug("ce_globle_init enter\n");

	g_ce_inited = true;

	return 0;
}

uint32_t switch_addr_type(ce_addr_type_t addr_type_s)
{
	uint32_t addr_type_d;

	switch (addr_type_s) {
		case SRAM_PUB:
			addr_type_d = 0x1;
			break;

		case SRAM_SEC:
			addr_type_d = 0x3;
			break;

		case KEY_INT:
			addr_type_d = 0x4;
			break;

		case EXT_MEM:
			addr_type_d = 0x0;
			break;

		case PKE_INTERNAL:
			addr_type_d = 0x2;
			break;

		default:
			addr_type_d = 0x0;
	}

	return addr_type_d;
}

uint32_t reg_value(uint32_t val, uint32_t src, uint32_t shift, uint32_t mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

void clean_cache_block(block_t *data, uint32_t ce_id)
{
	if ((data == NULL) || (data->len == 0)) {
		return;
	}

	switch (data->addr_type) {
		case EXT_MEM:
			//dma_cache_sync(g_sm2.dev, (addr_t)data->addr, data->len, DMA_TO_DEVICE);
			__flush_dcache_area((void *)data->addr, data->len);
			//dma_sync_single_for_cpu(g_sm2.dev, __pa((addr_t)data->addr), data->len, DMA_TO_DEVICE);
			break;

		case SRAM_PUB:
			//dma_sync_single_for_cpu(g_sm2.dev, (addr_t)data->addr+sram_addr(data->addr_type, ce_id), data->len, DMA_TO_DEVICE);
			break;

		default:
			return;
	}
}

void invalidate_cache_block(block_t *data, uint32_t ce_id)
{
	if ((data == NULL) || (data->len == 0)) {
		return;
	}

	switch (data->addr_type) {
		case EXT_MEM:
			__inval_dcache_area((void *)data->addr, data->len);
			//dma_sync_single_for_device(g_sm2.dev, __pa((addr_t)data->addr), data->len, DMA_TO_DEVICE);
			break;

		case SRAM_PUB:
			//dma_sync_single_for_device(g_sm2.dev, (addr_t)data->addr+sram_addr(data->addr_type, ce_id), data->len, DMA_TO_DEVICE);
			break;

		default:
			return;
	}
}

void clean_cache(addr_t start, uint32_t size)
{
	// arch_clean_cache_range(start, size);
}

void flush_cache(addr_t start, uint32_t size)
{
	__inval_dcache_area((void *)start, size);
	//dma_sync_single_for_device(g_sm2.dev, __pa(start), size, DMA_TO_DEVICE);
}

uint32_t ce_clear_memory(struct file *filp)
{
	struct crypto_dev *crypto = to_crypto_dev(filp->private_data);
	uint32_t status;
	pke_set_command((void *)crypto, BA414EP_OPTYPE_CLEAR_MEM, 0,
					BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
	status = pke_start_wait_status((void *)crypto);

	if (status) {
		pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
	}
	else {
		pr_info("ce_memory is deleted\n");
	}

	return status;
}

inline bool block_t_check_wrongful(block_t *variable)
{
	if (variable == NULL || variable->addr == NULL) {
		return true;
	}

	return false;
}

void hexdump8(const void *ptr, uint32_t len)
{
	unsigned long address = (unsigned long)ptr;
	size_t count;
	size_t i;
	pr_err("address is %p, len is %d\n", ptr, len);

	for (count = 0 ; count < len; count += 16) {
		for (i = 0; i < MIN(len - count, 16); i++) {
			pr_cont("%02hhx ", *(const uint8_t *)(address + i));
		}

		pr_err("\n");
		address += 16;
	}
}
