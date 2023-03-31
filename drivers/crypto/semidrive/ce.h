/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef _CE_H
#define _CE_H
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/mm.h>

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/spinlock_types.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <crypto/algapi.h>
#include <linux/interrupt.h>
#include <crypto/cryptodev.h>
#define WITH_SIMULATION_PLATFORM        0
#define CLEAR_CE_MEMORY_AFTER_CE_RUN    0
#define CE_HEAP_MALLOC_MAX_SIZE         512
#define PK_CM_ENABLED			0
#define RSA_PERFORMANCE_TEST		0
//TODO: need consider ce count in domain


#define to_crypto_dev(priv) container_of((priv), struct crypto_dev, miscdev)
#define CACHE_LINE 64
#define CE2_SRAM_PUB_HASH_HMAC_KEY_SIZE 0x40 //64 byte

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

/** ram offset
 * |                             sram public                                 |                           sram secure                       |
 * |----------------------------4k(0x1000)-----------------------------------|----------------------------4k(0x1000)-----------------------|
 * |         |hmac key 64 byte      |hash update iv 64 byte| hash out 64 byte|aes iv 16 byte|aes key 32byte|aes xkey 32byte|               |
 * |         |offset      0xf40     |offset      0xf80     |offset   0xfc0   |offset 0x0    |offset 0x10   |offset 0x30    |0ffset 0x50    |
*/
#define CE2_SRAM_PUB_HASH_HMAC_KEY_ADDR_OFFSET 0xf40
#define CE2_SRAM_PUB_HASH_MULT_PART_UPDATE_IV_ADDR_OFFSET 0xf80
#define CE2_SRAM_PUB_HASH_OUT_ADDR_OFFSET 0xfc0

#define CE2_SRAM_SEC_AES_IV_ADDR_OFFSET 0x0
#define CE2_SRAM_SEC_AES_KEY_ADDR_OFFSET 0x10
#define CE2_SRAM_SEC_AES_XKEY_ADDR_OFFSET 0x30
#define CE2_SRAM_SEC_AES_CTX_ADDR_OFFSET 0x50

typedef uintptr_t addr_t;

#define _vaddr(pa)      __va(pa)
#define _paddr(va)      __pa(va)
#define _ioaddr(offset)     (offset)

#define SDCE_ORDER		2
#define HASH_BUF_SIZE	((1 << SDCE_ORDER) * 4096)
#define CIPHER_BUF_SIZE	((1 << (SDCE_ORDER / 2)) * 4096)

/**
 Enumeration of data/iv/key/context address type
*/
typedef enum ce_addr_type_t {
	SRAM_PUB = 0,
	SRAM_SEC = 1,
	KEY_INT = 2,
	EXT_MEM = 3,
	PKE_INTERNAL = 4
} ce_addr_type_t;

typedef struct block {
	uint8_t *addr;   /* Start address of the data (FIFO or contiguous memory) */
	uint32_t len;    /* Length of data expressed in bytes */
	ce_addr_type_t addr_type; /* address type */
} block_t;

#define BLOCK_T_CONV(array, length, addr_type) ((block_t){(uint8_t *) (array), (uint32_t) (length), (ce_addr_type_t)addr_type})

//static inline block_t BLOCK_T_CONV(uint8_t * (array), uint32_t length,ce_addr_type_t addr_type)
// {
//     block_t_temp.addr = array;
//     block_t_temp.len = length;
//     block_t_temp.addr_type = addr_type;
//     return block_t_temp;
// }

static inline block_t block_t_convert(const volatile void *array, uint32_t length, ce_addr_type_t addr_type)
{
#if defined __cplusplus
	//'compound literals' are not valid in C++
	block_t  blk = BLOCK_T_CONV(array, length, addr_type);
	return blk;
#else
	//'compound literal' (used below) is valid in C99
	return (block_t)BLOCK_T_CONV(array, length, addr_type);
#endif
}

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))



struct mem_node {
	size_t size;
	size_t nid;
	size_t next;
	uint8_t *ptr;
};


struct kernel_crypt_op {
	struct crypt_op cop;

	int ivlen;
	__u8 iv[EALG_MAX_BLOCK_LEN];

	int digestsize;
	uint8_t hash_output[AALG_MAX_RESULT_LEN];

	struct task_struct *task;
	struct mm_struct *mm;
};

struct todo_list_item {
	struct list_head __hook;
	struct kernel_crypt_op kcop;
	int result;
};

struct fcrypt {
	struct list_head list;
	struct mutex sem;
};

struct locked_list {
	struct list_head list;
	struct mutex lock;
};

struct crypt_priv {
	struct fcrypt fcrypt;
	struct locked_list free, todo, done;
	int itemcount;
	struct work_struct cryptask;
	wait_queue_head_t user_waiter;
};

struct crypto_dev {
	struct device *dev;
	char name_buff[32];
	int ce_id;
	struct mutex mutex_lock;
	/*struct mutex*/spinlock_t lock;
	struct miscdevice miscdev;
	wait_queue_head_t cehashwaitq;
	int hash_condition;
	wait_queue_head_t cepkewaitq;
	int pke_condition;
	wait_queue_head_t cedmawaitq;
	int dma_condition;
	void __iomem *base;
	void __iomem *sram_base;

	struct crypt_priv *pcr;
	struct crypto_queue queue;
	struct tasklet_struct queue_task;
	struct tasklet_struct done_tasklet;
	struct crypto_async_request *req;
	int result;
	spinlock_t hash_lock;
	spinlock_t cipher_lock;
	int (*async_req_enqueue)(struct crypto_dev *sdce,
							 struct crypto_async_request *req);
	void (*async_req_done)(struct crypto_dev *sdce, int ret);
	uint32_t open_cnt;
};


typedef struct ec_key {
	block_t pub_key;
	block_t priv_key;
} ec_key_t;

extern  bool g_ce_inited;
extern volatile uint32_t g_int_flag;
extern void *hash_op_buf;
extern void *cipher_op_input_buf;
extern void *cipher_op_output_buf;

typedef unsigned long spin_lock_saved_state_t;
typedef unsigned long spin_lock_save_flags_t;

struct mem_node *ce_malloc(size_t size);
void ce_free(struct mem_node *mem_node);
void of_set_sys_cnt_ce(int i);
uint32_t of_get_sys_cnt_ce(void);

uint32_t reg_value(uint32_t val, uint32_t src, uint32_t shift, uint32_t mask);
uint32_t switch_addr_type(ce_addr_type_t addr_type_s);
addr_t addr_switch_to_ce(uint32_t vce_id, ce_addr_type_t addr_type_s, addr_t addr);
int32_t ce_init(uint32_t vce_id);
int32_t ce_globle_init(void);

void clean_cache(addr_t start, uint32_t size);
void flush_cache(addr_t start, uint32_t size);
void clean_cache_block(block_t *data, uint32_t ce_id);
void invalidate_cache_block(block_t *data, uint32_t ce_id);

uint32_t ce_inheap_init(void);
void ce_inheap_free(void);
void ce_inheap_clear(void);

uint32_t ce_clear_memory(struct file *filp);
bool block_t_check_wrongful(block_t *variable);

void hexdump8(const void *ptr, uint32_t len);
#endif
