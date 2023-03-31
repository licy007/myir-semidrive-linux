#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-contiguous.h>
#include <linux/genalloc.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <uapi/linux/sdrv-bufq.h>


#define BUFQ_HOLDER_MAX_NUM         (BUFQ_QUEUE_MAX_SIZE*BUFQ_QUEUE_MAX_NUM)
#define BUFQ_BIT_IDLE               0
#define BUFQ_BIT_BIND               1
#define BUFQ_BIT_RELEASE            2

#define BUFQ_MESSAGE_MAX_NUM        (BUFQ_QUEUE_MAX_SIZE*BUFQ_QUEUE_MAX_NUM)
#define BUFQ_MSG_QUEUE_ALIGN        4096
#define BUFQ_MSGF_READ_DONE         0
#define BUFQ_MSGF_WRITE_DONE        1
#define BUFQ_TASK_MAX_NUM           (BUFQ_QUEUE_MAX_SIZE*BUFQ_QUEUE_MAX_NUM)
#define BUFQ_EVT_TYPE_BUF_DATA      0
#define BUFQ_EVT_TYPE_ALLOC_REQ     1
#define BUFQ_EVT_TYPE_ALLOC_RESULT  2
#define BUFQ_EVT_TYPE_FREE_REQ      3
#define BUFQ_EVT_TYPE_FREE_RESULT   4
#define BUFQ_POOL_ORDER             12

#define BUFQ_MAGIC                  0x71667562

struct bufq_core;

typedef struct bufq_holder {
	bufq_buf_t buf;
	struct list_head list;
} bufq_holder_t;

typedef struct bufq_queue {
	struct bufq_core *bq;

	size_t alloc_count;
	struct list_head list;

	//int idle;
	volatile unsigned long flag;
	int index;

	spinlock_t lock;
	struct completion done;
} bufq_queue_t;

typedef struct bufq_user {
	bufq_queue_t *q;

	struct list_head list;
	spinlock_t lock;
} bufq_user_t;

typedef struct bufq_msg {
	uint8_t type;
	uint8_t flags;

	union {
		struct {
			int id;
			bufq_buf_t buf;
		} buf_data;
		bufq_alloc_t alloc_data;
	};
} bufq_msg_t;

typedef struct bufq_task {
	uint8_t type;

	int id;
	bufq_buf_t buf;

	struct list_head list;
} bufq_task_t;

typedef struct bufq_core {
	struct device *dev;

	bufq_queue_t q_array[BUFQ_QUEUE_MAX_NUM];

	bufq_holder_t *raw_holders;
	spinlock_t holder_lock;
	struct list_head holder_list;

	phys_addr_t shared_memory_addr;
	size_t shared_memory_length;
	struct gpio_desc *tx_gpio;
	struct gpio_desc *rx_gpio;
	int master;
	int irq;

	int ready;
	int wait_count;
	int pending_exit;
	struct task_struct *init_task;
	struct gen_pool *pool;

	spinlock_t tx_lock;
	int tx_index;
	bufq_msg_t *tx_msgs;
	int rx_index;
	bufq_msg_t *rx_msgs;

	spinlock_t raw_task_lock;
	bufq_task_t *raw_tasks;
	struct list_head raw_task_list;

	struct task_struct *async_task;
	spinlock_t task_lock;
	struct list_head task_list;
	struct completion kick_task;

	dev_t devno;
	struct class *class;
	struct device *chrdev;
	struct cdev cdev;
} bufq_core_t;

static bufq_core_t *gbq = NULL;

static bufq_queue_t *bufq_queue_find_id(bufq_core_t *bq, int id) {
	bufq_queue_t *q = NULL;
	int i;

	for (i = 0; i < BUFQ_QUEUE_MAX_NUM; i++) {
		if (test_bit(BUFQ_BIT_BIND, &bq->q_array[i].flag)
				&& bq->q_array[i].index == id) {
			q = &bq->q_array[i];
			break;
		}
	}

	return q;
}

static bufq_holder_t *bufq_holder_get(bufq_core_t *bq) {
	bufq_holder_t *holder = NULL;
	unsigned long flags;

	spin_lock_irqsave(&bq->holder_lock, flags);
	if (!list_empty(&bq->holder_list)) {
		holder = list_first_entry(&bq->holder_list, bufq_holder_t, list);
		list_del(&holder->list);
	}
	spin_unlock_irqrestore(&bq->holder_lock, flags);

	return holder;
}

static void bufq_holder_put(bufq_core_t *bq, bufq_holder_t *holder) {
	unsigned long flags;

	spin_lock_irqsave(&bq->holder_lock, flags);
	list_add(&holder->list, &bq->holder_list);
	spin_unlock_irqrestore(&bq->holder_lock, flags);
}

static int bufq_dmabuf_alloc_shared(bufq_core_t *bq, bufq_buf_t *b) {
	b->addr = gen_pool_alloc(bq->pool, b->size);
	return b->addr ? 0 : -ENOMEM;
}

static void bufq_dmabuf_free_shared(bufq_core_t *bq, bufq_buf_t b) {
	gen_pool_free(bq->pool, b.addr, b.size);
}

static bufq_holder_t *bufq_buffer_try_get(bufq_queue_t *q) {
	bufq_holder_t *holder = NULL;
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	if (!list_empty(&q->list)) {
		holder = list_first_entry(&q->list, bufq_holder_t, list);
		list_del(&holder->list);
	}
	spin_unlock_irqrestore(&q->lock, flags);

	return holder;
}

static bufq_holder_t *bufq_buffer_get(bufq_queue_t *q) {
	bufq_holder_t *holder;
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	while (list_empty(&q->list)) {
		spin_unlock_irqrestore(&q->lock, flags);
		wait_for_completion_timeout(&q->done, msecs_to_jiffies(50));
		spin_lock_irqsave(&q->lock, flags);
	}

	holder = list_first_entry(&q->list, bufq_holder_t, list);
	list_del(&holder->list);
	spin_unlock_irqrestore(&q->lock, flags);

	return holder;
}

static void bufq_buffer_set(bufq_queue_t *q, bufq_holder_t *holder) {
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	list_add(&holder->list, &q->list);
	spin_unlock_irqrestore(&q->lock, flags);
	complete(&q->done);
}

static size_t bufq_buffer_drain(bufq_queue_t *q) {
	bufq_core_t *bq = q->bq;
	bufq_holder_t *holder;
	size_t count = 0;

	while (q->alloc_count) {
		holder = bufq_buffer_get(q);
		bufq_dmabuf_free_shared(bq, holder->buf);
		bufq_holder_put(bq, holder);
		q->alloc_count--;
		count++;
	}

	return count;
}

static inline void bufq_address_to_offset(bufq_core_t *bq, bufq_buf_t *b) {
	b->addr -= bq->shared_memory_addr;
}

static inline void bufq_offset_to_address(bufq_core_t *bq, bufq_buf_t *b) {
	b->addr += bq->shared_memory_addr;
}

static void bufq_user_buf_inc(bufq_user_t *user, bufq_holder_t *holder) {
	unsigned long flags;

	spin_lock_irqsave(&user->lock, flags);
	list_add(&holder->list, &user->list);
	spin_unlock_irqrestore(&user->lock, flags);
}

static bufq_holder_t *bufq_user_buf_dec(bufq_user_t *user, bufq_buf_t b) {
	unsigned long flags;
	bufq_holder_t *holder = NULL;

	spin_lock_irqsave(&user->lock, flags);
	list_for_each_entry(holder, &user->list, list) {
		if (holder->buf.addr == b.addr) {
			list_del(&holder->list);
			break;
		}
	}
	spin_unlock_irqrestore(&user->lock, flags);

	return holder;
}

static void bufq_task_enqueue_task(bufq_core_t *bq, bufq_task_t *task) {
	unsigned long flags;

	spin_lock_irqsave(&bq->task_lock, flags);
	list_add(&task->list, &bq->task_list);
	spin_unlock_irqrestore(&bq->task_lock, flags);
}

static bufq_task_t *bufq_task_dequeue_task(bufq_core_t *bq) {
	bufq_task_t *task = NULL;
	unsigned long flags;

	spin_lock_irqsave(&bq->task_lock, flags);
	if (!list_empty(&bq->task_list)) {
		task = list_first_entry(&bq->task_list, bufq_task_t, list);
		list_del(&task->list);
	}
	spin_unlock_irqrestore(&bq->task_lock, flags);

	return task;
}

static int bufq_msg_send_buf(bufq_core_t *bq, bufq_buf_t b, int id) {
	unsigned long flags;
	bufq_msg_t *msg;

	spin_lock_irqsave(&bq->tx_lock, flags);
	msg = &bq->tx_msgs[bq->tx_index];
	if (readb(&msg->flags) != BUFQ_MSGF_READ_DONE) {
		spin_unlock_irqrestore(&bq->tx_lock, flags);
		dev_err(bq->dev, "%s(): message queue full\n", __func__);
		return -ENOMEM;
	}

	writeb(BUFQ_EVT_TYPE_BUF_DATA, &msg->type);
	writel(id, &msg->buf_data.id);
	memcpy_toio(&msg->buf_data.buf, &b, sizeof(bufq_buf_t));
	writeb(BUFQ_MSGF_WRITE_DONE, &msg->flags);
	bq->tx_index = (bq->tx_index + 1) % BUFQ_MESSAGE_MAX_NUM;
	spin_unlock_irqrestore(&bq->tx_lock, flags);

	gpiod_set_value_cansleep(bq->tx_gpio, 1);
	usleep_range(1, 2);
	gpiod_set_value_cansleep(bq->tx_gpio, 0);

	return 0;
}

static int bufq_msg_send_buf_async(bufq_core_t *bq, bufq_buf_t b, int id) {
	bufq_task_t *task = NULL;
	unsigned long flags;

	spin_lock_irqsave(&bq->raw_task_lock, flags);
	if (!list_empty(&bq->raw_task_list)) {
		task = list_first_entry(&bq->raw_task_list, bufq_task_t, list);
		list_del(&task->list);
	}
	spin_unlock_irqrestore(&bq->raw_task_lock, flags);

	if (!task) {
		dev_err(bq->dev, "%s(): fail to get async task\n", __func__);
		return -ENOMEM;
	}

	task->buf = b;
	task->id = id;
	bufq_task_enqueue_task(bq, task);
	complete(&bq->kick_task);

	return 0;
}

static void bufq_msg_recv_all_locked(bufq_core_t *bq) {
	bufq_msg_t *msg;
	bufq_queue_t *q;
	bufq_holder_t *holder;
	bufq_buf_t b;

	while (readb(&bq->rx_msgs[bq->rx_index].flags) == BUFQ_MSGF_WRITE_DONE) {
		msg = &bq->rx_msgs[bq->rx_index];

		if (readb(&msg->type) == BUFQ_EVT_TYPE_BUF_DATA) {
			q = bufq_queue_find_id(bq, readl(&msg->buf_data.id));
			holder = bufq_holder_get(bq);

			if (unlikely(!q || !holder || test_bit(BUFQ_BIT_RELEASE, &q->flag))) {
				memcpy_fromio(&b, &msg->buf_data.buf, sizeof(bufq_buf_t));
				if (bq->master) {
					bufq_offset_to_address(bq, &b);
					bufq_dmabuf_free_shared(bq, b);
				} else {
					bufq_msg_send_buf_async(bq, b, readl(&msg->buf_data.id));
				}

				if (holder) {
					bufq_holder_put(bq, holder);
				}
			} else {
				memcpy_fromio(&holder->buf, &msg->buf_data.buf, sizeof(bufq_buf_t));
				bufq_offset_to_address(bq, &holder->buf);
				bufq_buffer_set(q, holder);
			}
		} else if (readb(&msg->type) == BUFQ_EVT_TYPE_ALLOC_REQ) {
			// TODO
		} else if (readb(&msg->type) == BUFQ_EVT_TYPE_ALLOC_RESULT) {
			// TODO
		} else if (readb(&msg->type) == BUFQ_EVT_TYPE_FREE_REQ) {
			// TODO
		}

		writeb(BUFQ_MSGF_READ_DONE, &msg->flags);
		bq->rx_index = (bq->rx_index + 1) % BUFQ_MESSAGE_MAX_NUM;
	}
}

static int bufq_open(struct inode *inode, struct file *filp) {
	bufq_core_t *bq = gbq;
	bufq_user_t *user;
	bufq_queue_t *q = NULL;
	int i;

	if (unlikely(!bq->ready)) {
		dev_err(bq->dev, "%s(): bufq not ready\n", __func__);
		return -ENODEV;
	}

	user = devm_kzalloc(bq->dev, sizeof(bufq_user_t), GFP_KERNEL);
	if (!user) {
		dev_err(bq->dev, "%s(): fail to alloc bufq_user_t\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < BUFQ_QUEUE_MAX_NUM; i++) {
		if (test_and_clear_bit(BUFQ_BIT_IDLE, &bq->q_array[i].flag)) {
			q = &bq->q_array[i];
			break;
		}
	}

	if (!q) {
		dev_err(bq->dev, "%s(): fail to find idle queue\n", __func__);
		devm_kfree(bq->dev, user);
		return -ENODEV;
	}

	user->q = q;
	spin_lock_init(&user->lock);
	INIT_LIST_HEAD(&user->list);
	filp->private_data = user;

	dev_info(bq->dev, "%s(): user %px open q %px\n", __func__, user, q);
	return 0;
}

static long bufq_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	bufq_user_t *user = filp->private_data;
	bufq_queue_t *q = user->q;
	bufq_core_t *bq = q->bq;
	int ret = 0;

	switch (cmd) {
		case BQIOC_BIND:
			{
				int id;
				ret = copy_from_user(&id, (void __user *)arg, sizeof(int));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_from_user (%d)\n", __func__, ret);
					return ret;
				}

				if (test_and_set_bit(BUFQ_BIT_BIND, &q->flag)) {
					dev_err(bq->dev, "%s(): fail to bind %px, already bound\n", __func__, q);
					return -EINVAL;
				}
				q->index = id;

				dev_info(bq->dev, "%s(): bind queue %px to %d\n", __func__, q, id);
				break;
			}
		case BQIOC_ALLOC:
			{
				bufq_alloc_t alloc;
				bufq_holder_t *holder;
				bufq_buf_t b;
				int i;

				if (q->alloc_count) {
					dev_err(bq->dev, "%s(): already alloc %zu buffers\n", __func__, q->alloc_count);
					return -EINVAL;
				}

				ret = copy_from_user(&alloc, (void __user *)arg, sizeof(bufq_alloc_t));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_from_user (%d)\n", __func__, ret);
					return ret;
				}

				if (alloc.count > BUFQ_QUEUE_MAX_SIZE) {
					dev_err(bq->dev, "%s(): allocating %zu buffers exceeds limit %d\n",
							__func__, alloc.count, BUFQ_QUEUE_MAX_SIZE);
					return -ENOMEM;
				}

				if (bq->master) {
					b.size = alloc.size;
					for (i = 0; i < alloc.count; i++) {
						holder = bufq_holder_get(bq);
						if (!holder) {
							dev_warn(bq->dev, "%s(): fail to get buffer holder\n", __func__);
							break;
						}

						ret = bufq_dmabuf_alloc_shared(bq, &b);
						if (ret < 0) {
							dev_warn(bq->dev, "%s(): fail to alloc shared dmabuf (%d)\n", __func__, ret);
							bufq_holder_put(bq, holder);
							break;
						}

						holder->buf = b;
						bufq_buffer_set(q, holder);
						q->alloc_count++;
					}
				} else {
					// TODO
				}

				if (q->alloc_count < alloc.count) {
					dev_warn(bq->dev, "%s(): allocate %zu buffers but %zu succeed\n",
							__func__, alloc.count, q->alloc_count);
				}

				alloc.count = q->alloc_count;
				ret = copy_to_user((void __user *)arg, &alloc, sizeof(bufq_alloc_t));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_to_user (%d)\n", __func__, ret);

					if (bq->master) {
						bufq_buffer_drain(q);
					} else {
						// TODO
					}

					return ret;
				}

				dev_info(bq->dev, "%s(): queue %px alloc %zu buffers\n", __func__, q, q->alloc_count);
				break;
			}
		case BQIOC_FREE:
			{
				size_t count = 0;

				if (bq->master) {
					count = bufq_buffer_drain(q);
				} else {
					// TODO
				}

				dev_info(bq->dev, "%s(): queue %px free %zu buffers\n", __func__, q, count);
				break;
			}
		case BQIOC_G_BUF:
			{
				bufq_holder_t *holder;

				holder = bufq_buffer_try_get(q);
				if (!holder) {
					dev_dbg(bq->dev, "%s(): no buffer available\n", __func__);
					return -ENOMEM;
				}

				ret = copy_to_user((void __user *)arg, &holder->buf, sizeof(bufq_buf_t));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_to_user (%d)\n", __func__, ret);
					bufq_buffer_set(q, holder);
					return ret;
				}

				bufq_user_buf_inc(user, holder);
				break;
			}
		case BQIOC_S_BUF:
			{
				bufq_buf_t b;
				bufq_holder_t *holder;

				ret = copy_from_user(&b, (void __user *)arg, sizeof(bufq_buf_t));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_from_user (%d)\n", __func__, ret);
					return ret;
				}

				holder = bufq_user_buf_dec(user, b);
				if (unlikely(!holder)) {
					dev_warn(bq->dev, "%s(): buffer %llx size %lx is wild or already returned\n",
							__func__, b.addr, b.size);
					return -EINVAL;
				}

				bufq_address_to_offset(bq, &b);
				ret = bufq_msg_send_buf(bq, b, q->index);
				if (ret < 0) {
					dev_err(bq->dev, "%s(): fail to send buffer (%d)\n", __func__, ret);
					bufq_buffer_set(q, holder);
					return ret;
				}

				bufq_holder_put(bq, holder);
				break;
			}
		case BQIOC_DEBUG_ALLOC:
			{
				bufq_buf_t b;
				struct page *page;

				ret = copy_from_user(&b, (void __user *)arg, sizeof(bufq_buf_t));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_from_user (%d)\n", __func__, ret);
					return ret;
				}

				page = dma_alloc_from_contiguous(bq->dev, b.size >> PAGE_SHIFT,
						get_order(b.size), GFP_KERNEL);
				if (!page) {
					dev_err(bq->dev, "%s(): fail to alloc from configuous\n", __func__);
					return -ENOMEM;
				}

				b.addr = page_to_phys(page);
				ret = copy_to_user((void __user *)arg, &b, sizeof(bufq_buf_t));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_to_user (%d)\n", __func__, ret);
					dma_release_from_contiguous(bq->dev, page, b.size >> PAGE_SHIFT);
					return ret;
				}

				dev_info(bq->dev, "%s(): dma alloc %zu bytes at %llx\n", __func__, b.size, b.addr);
				break;
			}
		case BQIOC_DEBUG_FREE:
			{
				bufq_buf_t b;
				struct page *page;

				ret = copy_from_user(&b, (void __user *)arg, sizeof(bufq_buf_t));
				if (ret) {
					dev_err(bq->dev, "%s(): fail to copy_from_user (%d)\n", __func__, ret);
					return ret;
				}

				page = phys_to_page(b.addr);
				if (!page) {
					dev_err(bq->dev, "%s(): invalid address %llx\n", __func__, b.addr);
					return -EINVAL;
				}

				dma_release_from_contiguous(bq->dev, page, b.size >> PAGE_SHIFT);
				dev_info(bq->dev, "%s(): dma release %zu bytes at %llx\n", __func__, b.size, b.addr);
				break;
			}
		default:
			{
				dev_warn(bq->dev, "%s(): unknown ioctl cmd %x\n", __func__, cmd);
				return -ENOSYS;
			}
	}

	return ret;
}

static int bufq_release(struct inode *inode, struct file *filp) {
	bufq_user_t *user = filp->private_data;
	bufq_queue_t *q = user->q;
	bufq_core_t *bq = q->bq;
	bufq_holder_t *holder = NULL;
	unsigned long flags;

	spin_lock_irqsave(&user->lock, flags);
	while (!list_empty(&user->list)) {
		holder = list_first_entry(&user->list, bufq_holder_t, list);
		list_del(&holder->list);
		bufq_buffer_set(q, holder);
	}
	spin_unlock_irqrestore(&user->lock, flags);

	if (q->alloc_count) {
		dev_warn(bq->dev, "%s(): still %zu buffers not free\n", __func__, q->alloc_count);
		if (bq->master) {
			bufq_buffer_drain(q);
		} else {
			// TODO
		}
	} else {
		size_t count = 0;

		set_bit(BUFQ_BIT_RELEASE, &q->flag);
		while ((holder = bufq_buffer_try_get(q))) {
			bufq_address_to_offset(bq, &holder->buf);
			bufq_msg_send_buf(bq, holder->buf, q->index);
			bufq_holder_put(bq, holder);
			count++;
		}
		if (count)
			dev_info(bq->dev, "%s(): send back %zu buf\n", __func__, count);
	}

	clear_bit(BUFQ_BIT_BIND, &q->flag);
	clear_bit(BUFQ_BIT_RELEASE, &q->flag);
	set_bit(BUFQ_BIT_IDLE, &q->flag);
	devm_kfree(bq->dev, user);
	dev_info(bq->dev, "%s(): user %px release q %px\n", __func__, user, q);
	return 0;
}

static int bufq_msg_queue_init(bufq_core_t *bq, phys_addr_t start_addr, size_t *msgq_size) {
	size_t size, magic_size = sizeof(uint32_t);
	int i;

	size = ALIGN(sizeof(bufq_msg_t)*BUFQ_MESSAGE_MAX_NUM+magic_size,
			BUFQ_MSG_QUEUE_ALIGN);

	if (bq->master) {
		bq->tx_msgs = devm_ioremap(bq->dev, start_addr, size);
		bq->rx_msgs = devm_ioremap(bq->dev, start_addr + size, size);
	} else {
		bq->rx_msgs = devm_ioremap(bq->dev, start_addr, size);
		bq->tx_msgs = devm_ioremap(bq->dev, start_addr + size, size);
	}

	if (!bq->tx_msgs || !bq->rx_msgs) {
		dev_err(bq->dev, "%s(): fail to map IO address\n", __func__);
		return -ENOMEM;
	}

	/* wait for memory ready */
	writel(BUFQ_MAGIC, bq->tx_msgs);
	while (readl(bq->tx_msgs) != BUFQ_MAGIC || readl(bq->rx_msgs) != BUFQ_MAGIC) {
		msleep(1000);
		writel(BUFQ_MAGIC, bq->tx_msgs);
		bq->wait_count++;
		dev_info(bq->dev, "%s(): waiting %ds for link ready...\n",
				__func__, bq->wait_count);

		if (bq->pending_exit) {
			devm_iounmap(bq->dev, bq->tx_msgs);
			devm_iounmap(bq->dev, bq->rx_msgs);
			dev_info(bq->dev, "%s(): stop initializing\n", __func__);

			return -ENODEV;
		}
	}

	bq->tx_msgs ++;
	bq->rx_msgs ++;

	spin_lock_init(&bq->tx_lock);
	bq->tx_index = bq->rx_index = 0;
	for (i = 0; i < BUFQ_MESSAGE_MAX_NUM; i++) {
		writeb(BUFQ_MSGF_READ_DONE, &bq->tx_msgs[i].flags);
	}
	*msgq_size= size * 2;

	dev_info(bq->dev, "%s(): message queue: addr %llx size %zu tx %px rx %px\n",
			__func__, start_addr, *msgq_size, bq->tx_msgs, bq->rx_msgs);

	return 0;
}

static int bufq_parse_dt(bufq_core_t *bq, struct device_node *np) {
	uint64_t values[2];
	int ret;

	ret = of_property_read_u64_array(np, "shared-memory", values, 2);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to parse shared memory (%d)\n", __func__, ret);
		return ret;
	}

	bq->shared_memory_addr = values[0];
	bq->shared_memory_length = values[1];

	bq->tx_gpio = devm_gpiod_get_optional(bq->dev, "tx", GPIOD_OUT_LOW);
	if (IS_ERR(bq->tx_gpio)) {
		ret = PTR_ERR(bq->tx_gpio);
		dev_err(bq->dev, "%s(): fail to parse tx gpio (%d)\n", __func__, ret);
		return ret;
	}

	bq->rx_gpio = devm_gpiod_get_optional(bq->dev, "rx", GPIOD_IN);
	if (IS_ERR(bq->rx_gpio)) {
		ret = PTR_ERR(bq->rx_gpio);
		dev_err(bq->dev, "%s(): fail to parse rx gpio (%d)\n", __func__, ret);
		return ret;
	}

	bq->master = !!of_find_property(np, "sdrv-bufq,master", NULL);

	ret = gpiod_to_irq(bq->rx_gpio);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to get rx irq (%d)\n", __func__, ret);
		return ret;
	}
	bq->irq = ret;

	dev_info(bq->dev, "%s(): shared memory: addr %llx len %lx, master: %d, irq: %d\n",
			__func__, bq->shared_memory_addr, bq->shared_memory_length, bq->master, bq->irq);

	return 0;
}

static struct file_operations bufq_fops = {
	.owner = THIS_MODULE,
	.open = bufq_open,
	.release = bufq_release,
	.unlocked_ioctl = bufq_ioctl,
};

static char *bufq_chrdev_devnode(struct device *dev, umode_t *mode) {
	if (mode)
		*mode = 0666;

	return NULL;
}

static int bufq_chrdev_init(bufq_core_t *bq) {
	int ret;

	ret = alloc_chrdev_region(&bq->devno, 0, 1, "bufq");
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to alloc dev number (%d)\n", __func__, ret);
		return ret;
	}

	bq->class = class_create(THIS_MODULE, "sdrv-bufq");
	if (IS_ERR(bq->class)) {
		ret = PTR_ERR(bq->class);
		dev_err(bq->dev, "%s(): fail to create class (%d)\n", __func__, ret);
		goto err_class_create;
	}
	bq->class->devnode = bufq_chrdev_devnode;

	bq->chrdev = device_create(bq->class, bq->dev, bq->devno, bq, "sdrv-bufq");
	if (IS_ERR(bq->chrdev)) {
		ret = PTR_ERR(bq->chrdev);
		dev_err(bq->dev, "%s(): fail to create device (%d)\n", __func__, ret);
		goto err_device_create;
	}

	cdev_init(&bq->cdev, &bufq_fops);
	bq->cdev.owner = THIS_MODULE;
	ret = cdev_add(&bq->cdev, bq->devno, 1);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to add cdev (%d)\n", __func__, ret);
		goto err_add_cdev;
	}

	return 0;

err_add_cdev:
	device_destroy(bq->class, bq->devno);

err_device_create:
	class_destroy(bq->class);

err_class_create:
	unregister_chrdev_region(bq->devno, 1);

	return ret;
}

static void bufq_chrdev_remove(bufq_core_t *bq) {
	cdev_del(&bq->cdev);
	device_destroy(bq->class, bq->devno);
	class_destroy(bq->class);
	unregister_chrdev_region(bq->devno, 1);
}

static int bufq_init_func(void *data) {
	bufq_core_t *bq = data;
	size_t size;
	int ret;

	ret = bufq_msg_queue_init(bq, bq->shared_memory_addr, &size);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to init message queue (%d)\n", __func__, ret);
		goto exit;
	}

	bq->pool = devm_gen_pool_create(bq->dev, BUFQ_POOL_ORDER, -1, "sdrv-bufq-pool");
	if (IS_ERR(bq->pool)) {
		ret = PTR_ERR(bq->pool);
		dev_err(bq->dev, "%s(): fail to create memory pool (%d)\n", __func__, ret);
		goto exit;
	}

	ret = gen_pool_add(bq->pool, bq->shared_memory_addr + size
			, bq->shared_memory_length - size, -1);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to add memory (%d)\n", __func__, ret);
		goto exit;
	}
	dev_info(bq->dev, "%s(): memory pool: addr %llx size %zu\n",
			__func__, bq->shared_memory_addr + size, bq->shared_memory_length - size);

	bq->ready = 1;

exit:
	bq->init_task = NULL;

	return ret;
}

static int bufq_start_init_thread(bufq_core_t *bq) {
	int ret;

	bq->wait_count = bq->pending_exit = 0;
	bq->init_task = kthread_create(bufq_init_func, bq, "sdrv-bufq-init");
	if (IS_ERR(bq->init_task)) {
		ret = PTR_ERR(bq->init_task);
		dev_err(bq->dev, "%s(): fail to create init task (%d)\n", __func__, ret);
		return ret;
	}

	wake_up_process(bq->init_task);
	return 0;
}

static void bufq_stop_init_thread(bufq_core_t *bq) {
	bq->pending_exit = 1;
	while (bq->init_task) {
		dev_warn_ratelimited(bq->dev,
				"%s(): waiting for init task to exit...\n", __func__);
		cpu_relax();
	}
}

static int bufq_thread_func(void *data) {
	bufq_core_t *bq = data;
	bufq_task_t *task;
	unsigned long flags;

	while (!kthread_should_stop()) {
		task = bufq_task_dequeue_task(bq);
		if (!task) {
			wait_for_completion(&bq->kick_task);
			continue;
		}

		// only send msg task now
		bufq_msg_send_buf(bq, task->buf, task->id);

		spin_lock_irqsave(&bq->raw_task_lock, flags);
		list_add(&task->list, &bq->raw_task_list);
		spin_unlock_irqrestore(&bq->raw_task_lock, flags);
	}

	bq->async_task = NULL;

	return 0;
}

static irqreturn_t bufq_irq_handler(int irq, void *data) {
	bufq_core_t *bq = data;

	bufq_msg_recv_all_locked(bq);
	return IRQ_HANDLED;
}

static int bufq_pm_suspend(struct device *dev) {
	struct platform_device *pdev = to_platform_device(dev);
	bufq_core_t *bq = platform_get_drvdata(pdev);

	bufq_stop_init_thread(bq);

	return 0;
}

static int bufq_pm_resume(struct device *dev) {
	struct platform_device *pdev = to_platform_device(dev);
	bufq_core_t *bq = platform_get_drvdata(pdev);

	if (bq->ready)
		return 0;

	return bufq_start_init_thread(bq);
}

static int bufq_probe(struct platform_device *pdev) {
	bufq_core_t *bq;
	bufq_queue_t *q;
	int ret, i;

	gbq = NULL;

	bq = devm_kzalloc(&pdev->dev, sizeof(bufq_core_t), GFP_KERNEL);
	if (!bq) {
		dev_err(&pdev->dev, "%s(): fail to alloc buffer queue core\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, bq);
	bq->dev = &pdev->dev;

	ret = bufq_parse_dt(bq, bq->dev->of_node);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to parse dt (%d)\n", __func__, ret);
		return ret;
	}

	spin_lock_init(&bq->raw_task_lock);
	bq->raw_tasks = devm_kzalloc(bq->dev,
			BUFQ_TASK_MAX_NUM * sizeof(bufq_task_t), GFP_KERNEL);
	if (!bq->raw_tasks) {
		dev_err(bq->dev, "%s(): fail to alloc tasks\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&bq->raw_task_list);
	for (i = 0; i < BUFQ_TASK_MAX_NUM; i++) {
		INIT_LIST_HEAD(&bq->raw_tasks[i].list);
		list_add(&bq->raw_tasks[i].list, &bq->raw_task_list);
	}

	spin_lock_init(&bq->holder_lock);
	bq->raw_holders = devm_kzalloc(bq->dev,
			BUFQ_HOLDER_MAX_NUM * sizeof(bufq_holder_t), GFP_KERNEL);
	if (!bq->raw_holders) {
		dev_err(bq->dev, "%s(): fail to alloc holders\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&bq->holder_list);
	for (i = 0; i < BUFQ_HOLDER_MAX_NUM; i++) {
		INIT_LIST_HEAD(&bq->raw_holders[i].list);
		list_add(&bq->raw_holders[i].list, &bq->holder_list);
	}

	bq->ready = 0;
	ret = bufq_start_init_thread(bq);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to init queue memory (%d)\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < BUFQ_QUEUE_MAX_NUM; i++) {
		q = &bq->q_array[i];

		q->bq = bq;
		q->alloc_count = 0;
		INIT_LIST_HEAD(&q->list);
		q->flag = BIT(BUFQ_BIT_IDLE);
		spin_lock_init(&q->lock);
		init_completion(&q->done);
	}

	ret = devm_request_irq(bq->dev, bq->irq,
			bufq_irq_handler, IRQF_TRIGGER_FALLING, "sdrv-bufq-irq", bq);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to request irq (%d)\n", __func__, ret);
		return ret;
	}

	ret = bufq_chrdev_init(bq);
	if (ret < 0) {
		dev_err(bq->dev, "%s(): fail to init char dev (%d)\n", __func__, ret);
		return ret;
	}

	spin_lock_init(&bq->task_lock);
	INIT_LIST_HEAD(&bq->task_list);
	init_completion(&bq->kick_task);
	bq->async_task = kthread_create(bufq_thread_func, bq, "sdrv-bufq-async");
	if (IS_ERR(bq->async_task)) {
		ret = PTR_ERR(bq->async_task);
		dev_err(bq->dev, "%s(): fail to create async task (%d)\n", __func__, ret);
		bufq_chrdev_remove(bq);
		return ret;
	}

	wake_up_process(bq->async_task);

	gbq = bq;
	dev_info(bq->dev, "%s(): buffer queue probe OK\n", __func__);
	return 0;
}

static int bufq_remove(struct platform_device *pdev) {
	bufq_core_t *bq = platform_get_drvdata(pdev);

	bufq_stop_init_thread(bq);

	kthread_stop(bq->async_task);
	while (bq->async_task) {
		dev_warn_ratelimited(bq->dev,
				"%s(): waiting for async task to exit...\n", __func__);
		cpu_relax();
	}

	bufq_chrdev_remove(bq);

	dev_info(bq->dev, "%s(): buffer queue remove OK\n", __func__);
	return 0;
}

static const struct dev_pm_ops bufq_pm_ops = {
	.suspend = bufq_pm_suspend,
	.resume = bufq_pm_resume,
};

static const struct of_device_id bufq_dt_match[] = {
	{.compatible = "sd,sdrv-bufq"},
	{},
};

static struct platform_driver bufq_driver = {
	.probe = bufq_probe,
	.remove = bufq_remove,
	.driver = {
		.name = "sdrv-bufq",
		.of_match_table = bufq_dt_match,
		.pm = &bufq_pm_ops,
	},
};

module_platform_driver(bufq_driver);

MODULE_DESCRIPTION("Semidrive buffer queue between SSA/SSB");
MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_LICENSE("GPL v2");

