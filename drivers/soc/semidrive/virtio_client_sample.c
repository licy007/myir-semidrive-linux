#include <asm/cacheflush.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/soc/semidrive/sys_counter.h>
#include <linux/string.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <uapi/linux/virtio_ids.h>

#define USE_CMA		1

#define TX_COUNT	16u
#define RX_COUNT	16u
#define BUF_SIZE	PAGE_SIZE

#define DATA_TEXT	0
#define DATA_LATENCY	1
#define DATA_BURST	2

struct virtio_sample_data {
	u32 type;

	union {
		struct {
			u32 len;
			char str[];
		} text;
		struct {
			u32 server_recv;
			u32 server_send;
			u32 cf;
		} latency;
	};
} __attribute__((packed, aligned(4)));

struct virtio_sample_buffer {
	struct list_head list;
	size_t size;
#if USE_CMA
	struct page *pages;
#else
	void *addr;
	dma_addr_t dma_addr;
#endif

	u32 client_send;
	u32 client_recv;
	u32 cache_flush;
	u32 cache_inval;

	u32 t1, t2, t3;
};

struct virtio_sample {
	struct virtio_device        *vdev;
	struct virtqueue            *tx, *rx;
	spinlock_t                  lock;
	struct list_head            list;
	struct virtio_sample_buffer bufs[TX_COUNT + RX_COUNT];

	u64 total_latency;
	u32 latency_count;
	u32 max_latency;
	u32 min_latency;

	u32 index;
	u32 cache_flush;
	u32 client_send;
	u32 server_recv;
	u32 server_send;
	u32 client_recv;
	u32 cache_inval;

	u32 t1, t2, t3;
};

static inline struct virtio_sample_buffer *virtio_sample_buf_try_get(struct virtio_sample *vs)
{
	struct virtio_sample_buffer *buf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&vs->lock, flags);
	if (!list_empty(&vs->list)) {
		buf = list_first_entry(&vs->list, struct virtio_sample_buffer, list);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&vs->lock, flags);

	return buf;
}

static inline struct virtio_sample_buffer *virtio_sample_buf_get(struct virtio_sample *vs)
{
	struct virtio_sample_buffer *buf = NULL;

	while (!(buf = virtio_sample_buf_try_get(vs)))
		cpu_relax();

	return buf;
}

static void virtio_sample_tx_callback(struct virtqueue *vq)
{
	struct virtio_sample *vs = vq->vdev->priv;
	struct virtio_sample_buffer *buf = NULL;
	struct virtio_sample_data *d;
	unsigned long flags;
	unsigned int len;
	u32 diff;

	spin_lock_irqsave(&vs->lock, flags);
	while ((buf = virtqueue_get_buf(vs->tx, &len)) != NULL) {
		spin_unlock_irqrestore(&vs->lock, flags);

#if USE_CMA
		d = (struct virtio_sample_data *)page_to_virt(buf->pages);
#else
		d = (struct virtio_sample_data *)buf->addr;
#endif

		buf->client_recv = semidrive_get_syscntr();

		//dma_sync_single_for_cpu(&vdev->dev, b->dma_addr, b->size, DMA_FROM_DEVICE);
		__inval_dcache_area(d, sizeof(struct virtio_sample_data));

		if (d->type == DATA_LATENCY) {
			buf->cache_inval = semidrive_get_syscntr();

			diff = buf->client_recv - buf->client_send;
			vs->total_latency += diff;
			vs->min_latency = min(vs->min_latency, diff);

			if (vs->max_latency < diff) {
				vs->max_latency = diff;

				vs->index = vs->latency_count;
				vs->cache_flush = buf->cache_flush;
				vs->client_send = buf->client_send;
				vs->server_recv = d->latency.server_recv;
				vs->server_send = d->latency.server_send;
				vs->client_recv = buf->client_recv;
				vs->cache_inval = buf->cache_inval;

				vs->t1 = buf->t1;
				vs->t2 = buf->t2;
				vs->t3 = buf->t3;
			}

			vs->latency_count += 1;
		} else if (d->type == DATA_BURST) {
			diff = buf->client_recv - buf->client_send;
			vs->total_latency += diff;
			vs->min_latency = min(vs->min_latency, diff);
			vs->max_latency = max(vs->max_latency, diff);

			vs->latency_count += 1;
		}

		spin_lock_irqsave(&vs->lock, flags);
		list_add(&buf->list, &vs->list);
	}
	spin_unlock_irqrestore(&vs->lock, flags);
}

static void virtio_sample_rx_callback(struct virtqueue *vq)
{
	struct virtio_sample *vs = vq->vdev->priv;
	struct virtio_sample_buffer *buf = NULL;
	struct scatterlist sg[1];
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vs->lock, flags);
	while ((buf = virtqueue_get_buf(vs->rx, &len)) != NULL) {
		spin_unlock_irqrestore(&vs->lock, flags);

#if USE_CMA
		__inval_dcache_area(page_to_virt(buf->pages), len);
		dev_info(&vs->vdev->dev, "recv %u bytes: %s\n",
				len, page_to_virt(buf->pages));
#else
		__inval_dcache_area(buf->addr, len);
		dev_info(&vs->vdev->dev, "recv %u bytes: %s\n",
				len, buf->addr);
#endif

		spin_lock_irqsave(&vs->lock, flags);

#if USE_CMA
		sg_init_one(sg, page_to_virt(buf->pages), buf->size);
#else
		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(buf->addr),
				buf->size, offset_in_page(buf->addr));
#endif
		virtqueue_add_inbuf(vs->rx, sg, 1, buf, GFP_ATOMIC);
	}
	spin_unlock_irqrestore(&vs->lock, flags);
}

static void virtio_sample_fill_rx(struct virtio_sample *vs)
{
	struct virtio_sample_buffer *buf;
	struct scatterlist sg[1];
	unsigned long flags;
	unsigned int i, size;

	size = min(RX_COUNT, virtqueue_get_vring_size(vs->rx));

	spin_lock_irqsave(&vs->lock, flags);
	for (i = 0; i < size; i++) {
		buf = list_first_entry(&vs->list, struct virtio_sample_buffer, list);
		list_del(&buf->list);

#if USE_CMA
		sg_init_one(sg, page_to_virt(buf->pages), buf->size);
#else
		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(buf->addr),
				buf->size, offset_in_page(buf->addr));
#endif
		virtqueue_add_inbuf(vs->rx, sg, 1, buf, GFP_ATOMIC);
	}
	spin_unlock_irqrestore(&vs->lock, flags);

	dev_err(&vs->vdev->dev, "add %u bufs in rx queue\n", size);
}

static void virtio_sample_init_latency(struct virtio_sample *vs)
{
	vs->total_latency = 0;
	vs->latency_count = 0;
	vs->max_latency = 0;
	vs->min_latency = U32_MAX;

	vs->index = 0;
	vs->cache_flush = 0;
	vs->client_send = 0;
	vs->server_recv = 0;
	vs->server_send = 0;
	vs->client_recv = 0;
	vs->cache_inval = 0;
}

static ssize_t text_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct virtio_device *vdev = container_of(dev, struct virtio_device, dev);
	struct virtio_sample *vs = vdev->priv;
	struct virtio_sample_buffer *b = NULL;
	struct virtio_sample_data *d;
	struct scatterlist sg[1];
	unsigned long flags;

	b = virtio_sample_buf_try_get(vs);
	if (!b) {
		dev_err(&vs->vdev->dev, "not enough buffer\n");
		return len;
	}

#if USE_CMA
	d = (struct virtio_sample_data *)page_to_virt(b->pages);
#else
	d = (struct virtio_sample_data *)b->addr;
#endif

	d->type = DATA_TEXT;
	d->text.len = min(BUF_SIZE - offsetof(struct virtio_sample_data, text.str), len);
	memcpy(d->text.str, buf, d->text.len);

	//dma_sync_single_for_device(&vdev->dev, b->dma_addr, b->size, DMA_TO_DEVICE);
#if USE_CMA
	__flush_dcache_area(page_to_virt(b->pages),
			offsetof(struct virtio_sample_data, text.str) + d->text.len);
#else
	__flush_dcache_area(b->addr,
			offsetof(struct virtio_sample_data, text.str) + d->text.len);
#endif

#if USE_CMA
	//sg_init_one(sg, page_to_virt(b->pages), b->size);
	sg_init_table(sg, 1);
	sg_set_page(sg, b->pages, b->size, 0);
#else
	sg_init_table(sg, 1);
	sg_set_page(sg, vmalloc_to_page(b->addr),
			b->size, offset_in_page(b->addr));
#endif

	spin_lock_irqsave(&vs->lock, flags);
	virtqueue_add_outbuf(vs->tx, sg, 1, b, GFP_ATOMIC);
	spin_unlock_irqrestore(&vs->lock, flags);
	virtqueue_kick(vs->tx);

	dev_info(&vs->vdev->dev, "send %zu bytes\n", d->text.len);

	return len;
}

static uint latency_threshold = 0;
module_param(latency_threshold, uint, 0644);

static ssize_t latency_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct virtio_device *vdev = container_of(dev, struct virtio_device, dev);
	struct virtio_sample *vs = vdev->priv;
	struct virtio_sample_buffer *b = NULL;
	struct virtio_sample_data *d;
	int count, i;
	unsigned long flags;
	struct scatterlist sg[1];
	ktime_t start;

	if (kstrtoint(buf, 10, &count)) {
		dev_err(&vdev->dev, "invalid count %s\n", buf);
		return len;
	}

	virtio_sample_init_latency(vs);

	start = ktime_get_boottime();

	for (i = 0; i < count; i++) {
		b = virtio_sample_buf_get(vs);

#if USE_CMA
		d = (struct virtio_sample_data *)page_to_virt(b->pages);
#else
		d = (struct virtio_sample_data *)b->addr;
#endif

		d->type = DATA_LATENCY;

		b->cache_flush = semidrive_get_syscntr();
		d->latency.cf = b->cache_flush;

		//dma_sync_single_for_device(&vdev->dev, b->dma_addr, b->size, DMA_TO_DEVICE);
#if USE_CMA
		__flush_dcache_area(page_to_virt(b->pages),
				sizeof(struct virtio_sample_data));
#else
		__flush_dcache_area(b->addr,
				sizeof(struct virtio_sample_data));
#endif

		b->client_send = semidrive_get_syscntr();

#if USE_CMA
		//sg_init_one(sg, page_to_virt(b->pages), b->size);
		sg_init_table(sg, 1);
		sg_set_page(sg, b->pages, b->size, 0);
#else
		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(b->addr),
				b->size, offset_in_page(b->addr));
#endif

		b->t1 = semidrive_get_syscntr();
		spin_lock_irqsave(&vs->lock, flags);
		virtqueue_add_outbuf(vs->tx, sg, 1, b, GFP_ATOMIC);
		spin_unlock_irqrestore(&vs->lock, flags);

		b->t2 = semidrive_get_syscntr();
		virtqueue_kick(vs->tx);

		b->t3 = semidrive_get_syscntr();
		while (vs->latency_count < i + 1) {
			/* although not counted in, this will cost lots of time */
			dev_info_ratelimited(&vdev->dev,
					"waiting for %u\n", vs->latency_count);
			cpu_relax();
		}

		if (latency_threshold && vs->server_recv - vs->cache_flush > latency_threshold)
			break;
	}

	dev_info(&vdev->dev,
			"single test %u cost %lld, latency avg %llu min %llu max %llu freq %u\n",
			vs->latency_count, ktime_get_boottime() - start,
			vs->total_latency / vs->latency_count,
			vs->min_latency, vs->max_latency, semidrive_get_syscntr_freq());
	dev_info(&vdev->dev,
			"max latency %u cf %u cs %u sr %u ss %u cr %u ci %u c2s %u s2c %u\n", vs->index,
			vs->cache_flush, vs->client_send, vs->server_recv,
			vs->server_send, vs->client_recv, vs->cache_inval,
			vs->server_recv - vs->client_send,
			vs->client_recv - vs->server_send);
	dev_info(&vdev->dev, "t1 %u t2 %u t3 %u\n", vs->t1, vs->t2, vs->t3);

	return len;
}

static ssize_t burst_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct virtio_device *vdev = container_of(dev, struct virtio_device, dev);
	struct virtio_sample *vs = vdev->priv;
	struct virtio_sample_buffer *b = NULL;
	struct virtio_sample_data *d;
	int count, i;
	unsigned long flags;
	struct scatterlist sg[1];
	ktime_t start;

	if (kstrtoint(buf, 10, &count)) {
		dev_err(&vdev->dev, "invalid count %s\n", buf);
		return len;
	}

	virtio_sample_init_latency(vs);

	start = ktime_get_boottime();

	for (i = 0; i < count; i++) {
		b = virtio_sample_buf_get(vs);

#if USE_CMA
		d = (struct virtio_sample_data *)page_to_virt(b->pages);
#else
		d = (struct virtio_sample_data *)b->addr;
#endif

		d->type = DATA_BURST;

		//dma_sync_single_for_device(&vdev->dev, b->dma_addr, b->size, DMA_TO_DEVICE);
		__flush_dcache_area(d, sizeof(d->type));

		b->client_send = semidrive_get_syscntr();

#if USE_CMA
		sg_init_table(sg, 1);
		sg_set_page(sg, b->pages, b->size, 0);
#else
		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(b->addr),
				b->size, offset_in_page(b->addr));
#endif

		spin_lock_irqsave(&vs->lock, flags);
		virtqueue_add_outbuf(vs->tx, sg, 1, b, GFP_ATOMIC);
		spin_unlock_irqrestore(&vs->lock, flags);

		virtqueue_kick(vs->tx);
	}

	while (vs->latency_count < count) {
		dev_info_ratelimited(&vdev->dev,
				"waiting for %u, total %d\n", vs->latency_count, count);
		cpu_relax();
	}

	dev_info(&vdev->dev,
			"burst test %u cost %lld, latency avg %llu min %llu max %llu freq %u\n",
			vs->latency_count, ktime_get_boottime() - start,
			vs->total_latency / vs->latency_count,
			vs->min_latency, vs->max_latency, semidrive_get_syscntr_freq());

	return len;
}

static DEVICE_ATTR_WO(text);
static DEVICE_ATTR_WO(latency);
static DEVICE_ATTR_WO(burst);

static struct attribute *virtio_sample_attributes[] = {
	&dev_attr_text.attr,
	&dev_attr_latency.attr,
	&dev_attr_burst.attr,
	NULL
};

static const struct attribute_group virtio_sample_attr_group = {
	.name = "vs-debug",
	.attrs = virtio_sample_attributes,
};

static int virtio_sample_probe(struct virtio_device *vdev)
{
	struct virtio_sample *vs;
	struct virtio_sample_buffer *buf;
	struct virtqueue *vqs[2];
	vq_callback_t *cbs[] = { virtio_sample_tx_callback,
				 virtio_sample_rx_callback };
	static const char * const names[] = { "tx", "rx" };
	int err, i;

	vs = kzalloc(sizeof(*vs), GFP_KERNEL);
	if (!vs)
		return -ENOMEM;

	vdev->priv = vs;
	vs->vdev = vdev;
	spin_lock_init(&vs->lock);
	INIT_LIST_HEAD(&vs->list);

	for (i = 0; i < ARRAY_SIZE(vs->bufs); i++) {
		buf = &vs->bufs[i];

		INIT_LIST_HEAD(&buf->list);

		buf->size = BUF_SIZE;
#if USE_CMA
		buf->pages = dma_alloc_from_contiguous(&vdev->dev,
				buf->size >> PAGE_SHIFT, get_order(buf->size), GFP_KERNEL);
		if (!buf->pages) {
			dev_err(&vdev->dev, "fail to alloc from contiguous\n");
			goto fail;
		}
		dev_info(&vdev->dev, "alloc dmabuf %p size %zu\n", buf->pages, buf->size);
#else
		buf->addr = dma_alloc_coherent(&vdev->dev,
				buf->size, &buf->dma_addr, GFP_KERNEL);
		if (!buf->addr) {
			dev_err(&vdev->dev, "fail to alloc dmabuf\n");
			goto fail;
		}
		dev_info(&vdev->dev, "alloc dmabuf %p size %zu at %x\n",
				buf->addr, buf->size, buf->dma_addr);
#endif

		list_add(&buf->list, &vs->list);
	}

	dev_info(&vdev->dev, "alloc %d bufs\n", i);

	err = virtio_find_vqs(vs->vdev, 2, vqs, cbs, names, NULL);
	if (err)
		goto fail;

	vs->tx = vqs[0];
	vs->rx = vqs[1];

	dev_info(&vs->vdev->dev, "find vqs: tx %p rx %p\n", vs->tx, vs->rx);

	err = sysfs_create_group(&vs->vdev->dev.kobj, &virtio_sample_attr_group);
	if (err)
		goto fail;

	virtio_device_ready(vdev);

	virtio_sample_fill_rx(vs);

	virtio_sample_init_latency(vs);

	dev_info(&vdev->dev, "virtio_sample ready\n");
	return 0;

fail:
	for (i = i - 1; i >= 0; i--) {
		buf = &vs->bufs[i];
#if USE_CMA
		dma_release_from_contiguous(&vdev->dev,
				buf->pages, buf->size >> PAGE_SHIFT);
#else
		dma_free_coherent(&vdev->dev,
				buf->size, buf->addr, buf->dma_addr);
#endif
	}

	kfree(vs);

	return err;
}

static void virtio_sample_remove(struct virtio_device *vdev)
{
	struct virtio_sample *vs = vdev->priv;

	sysfs_remove_group(&vs->vdev->dev.kobj, &virtio_sample_attr_group);
	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
	kfree(vs);
}

static unsigned int virtio_sample_features[] = {
	VIRTIO_F_VERSION_1
	/* none */
};

static struct virtio_device_id virtio_sample_id_table[] = {
	{ 25, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_sample_driver = {
	.driver = {
		.name = "virtio-sample",
		.owner = THIS_MODULE,
	},
	.feature_table       = virtio_sample_features,
	.feature_table_size  = ARRAY_SIZE(virtio_sample_features),
	.id_table            = virtio_sample_id_table,
	.probe               = virtio_sample_probe,
	.remove              = virtio_sample_remove,
};

module_virtio_driver(virtio_sample_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Semidrive virtio driver sample");
MODULE_AUTHOR("Semidrive Semiconductor");
