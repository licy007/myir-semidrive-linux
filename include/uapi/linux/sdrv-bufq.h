#ifndef __BUFFER_QUEUE_H__
#define __BUFFER_QUEUE_H__

#include <linux/types.h>


#define BUFQ_DEVICE_NAME            "/dev/sdrv-bufq"
#define BUFQ_QUEUE_MAX_SIZE         16
#define BUFQ_QUEUE_MAX_NUM          16

typedef struct bufq_alloc {
	size_t size;
	size_t count;
} bufq_alloc_t;

typedef struct bufq_buf {
	phys_addr_t addr;
	size_t size;
} bufq_buf_t;

#define BQIOC_MAGIC 'b'

/*
 * bind queue to some unique ID
 */
#define BQIOC_BIND _IOW(BQIOC_MAGIC, 0, int)

/*
 * allocate buffers for binded queue
 * Note: user must check the actual buffer count returned
 */
#define BQIOC_ALLOC _IOWR(BQIOC_MAGIC, 1, bufq_alloc_t)

/*
 * free all buffers in binded queue
 */
#define BQIOC_FREE _IO(BQIOC_MAGIC, 2)

/*
 * dequeue buffer from binded queue
 * may fail if buffer is currently at remote side
 */
#define BQIOC_G_BUF _IOR(BQIOC_MAGIC, 3, bufq_buf_t)

/*
 * enqueue buffer to binded queue
 */
#define BQIOC_S_BUF _IOW(BQIOC_MAGIC, 4, bufq_buf_t)

/*
 * for debug use
 */
#define BQIOC_DEBUG_PING _IO(BQIOC_MAGIC, 5)
#define BQIOC_DEBUG_ALLOC _IOWR(BQIOC_MAGIC, 6, bufq_buf_t)
#define BQIOC_DEBUG_FREE _IOW(BQIOC_MAGIC, 6, bufq_buf_t)

#endif

