#include <linux/module.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/sys_counter.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <uapi/linux/virtio_ids.h>
#include "virtio_service.h"


#define VQ_TX					0
#define VQ_RX					1
#define VQ_NUM					2

#define MSG_TEXT				0
#define MSG_TIMESTAMP				1
#define MSG_DEVICE				2

struct virtio_devinfo {
	u32 vid;
	u32 reg;
};

struct virtio_provider_message {
	u32 type;
	union {
		struct {
			u32 len;
			char str[];
		} text;
		struct {
			u32 cs;
			u32 sr;
			u32 ss;
			u32 cr;
		} timestamp;
		struct {
			u32 num;
			struct virtio_devinfo devs[];
		} device;
	};
} __attribute__((packed, aligned(4)));

struct virtio_provider_buffer {
	void __iomem *addr;
	size_t size;
	struct list_head list;
};

struct virtio_provider_config {
	u32 __reserved[4];
	u32 ap_domain;
};

struct virtio_provider {
	struct virtio_device *vdev;
	struct virtqueue *tx, *rx;

	phys_addr_t mem_base;
	size_t mem_size;
	void __iomem *base;

	spinlock_t lock;
	struct list_head buf_list;
	struct virtio_provider_buffer __bufs[PROVIDER_TX_VRING_LEN];

	u32 latency_count;
	u64 total_latency;
	u32 min_latency;
	u32 max_latency;
	u32 idx, cs, sr, ss, cr;
};

size_t virtio_provider_send_text(
		struct virtio_device *vdev, const char *buf, size_t len)
{
	struct device *dev = &vdev->dev;
	struct virtio_provider *provider;
	struct virtio_provider_buffer *buffer = NULL;
	struct virtio_provider_message *msg;
	struct scatterlist sg[1];
	unsigned long flags;
	size_t size;

	if (!vdev)
		return 0;

	provider = vdev->priv;
	if (!provider) {
		dev_err(dev, "provider not ready\n");
		return 0;
	}

	spin_lock_irqsave(&provider->lock, flags);
	buffer = list_first_entry_or_null(&provider->buf_list,
			struct virtio_provider_buffer, list);
	if (buffer)
		list_del(&buffer->list);
	spin_unlock_irqrestore(&provider->lock, flags);

	if (!buffer) {
		dev_err(dev, "not enough buffer\n");
		return 0;
	}

	msg = (struct virtio_provider_message *)buffer->addr;
	size = min(len, buffer->size
			- sizeof(msg->type)
			- sizeof(msg->text.len) - 1u);

	msg->type = MSG_TEXT;
	msg->text.len = size;
	memcpy(msg->text.str, buf, size);
	msg->text.str[size] = '\0';

	sg_init_table(sg, 1);
	sg_set_page(sg, vmalloc_to_page(buffer->addr),
			buffer->size, offset_in_page(buffer->addr));

	spin_lock_irqsave(&provider->lock, flags);
	virtqueue_add_outbuf(provider->tx, sg, 1, buffer, GFP_ATOMIC);
	spin_unlock_irqrestore(&provider->lock, flags);
	virtqueue_kick(provider->tx);

	return size;
}

void virtio_provider_latency_test(struct virtio_device *vdev, u32 count)
{
	struct device *dev = &vdev->dev;
	struct virtio_provider *provider;
	struct virtio_provider_buffer *buffer;
	struct virtio_provider_message *msg;
	struct scatterlist sg[1];
	unsigned long flags;
	u32 freq, i;

	if (!vdev)
		return;

	provider = vdev->priv;
	if (!provider) {
		dev_err(dev, "provider not ready\n");
		return;
	}

	provider->latency_count = 0;
	provider->total_latency = 0;
	provider->min_latency = U32_MAX;
	provider->max_latency = 0;

	for (i = 0; i < count; i++) {
		/* get a msg buf */
		buffer = NULL;
		spin_lock_irqsave(&provider->lock, flags);
		buffer = list_first_entry_or_null(&provider->buf_list,
				struct virtio_provider_buffer, list);
		if (buffer)
			list_del(&buffer->list);
		spin_unlock_irqrestore(&provider->lock, flags);

		if (!buffer) {
			dev_err(dev, "not enough buffer\n");
			break;
		}

		msg = (struct virtio_provider_message *)buffer->addr;
		msg->type = MSG_TIMESTAMP;
		msg->timestamp.cs = semidrive_get_syscntr();

		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(buffer->addr),
				buffer->size, offset_in_page(buffer->addr));

		spin_lock_irqsave(&provider->lock, flags);
		virtqueue_add_outbuf(provider->tx, sg, 1, buffer, GFP_ATOMIC);
		spin_unlock_irqrestore(&provider->lock, flags);
		virtqueue_kick(provider->tx);

		while (provider->latency_count < i + 1) {
			dev_info_ratelimited(dev,
					"waiting for %u",
					provider->latency_count);
			cpu_relax();
		}
	}

	freq = semidrive_get_syscntr_freq();
	dev_info(dev, "lantency test result (freq %u):\n", freq);
	dev_info(dev, "    avg %llu min %llu max %llu\n",
			provider->total_latency * 1000000u / freq / provider->latency_count,
			provider->min_latency * 1000000u / freq,
			provider->max_latency * 1000000u / freq);
	dev_info(dev, "transaction %u has max latency, timestamp:\n",
			provider->idx);
	dev_info(dev, "    cs %u sr %u ss %u cr %u\n",
			provider->cs, provider->sr, provider->ss, provider->cr);
}

static void virtio_provider_handle_device(
		struct virtio_provider *provider,
		struct virtio_provider_message *msg)
{
	struct device *dev = &provider->vdev->dev;
	struct virtio_devinfo *info;
	u32 i;

	for (i = 0; i < msg->device.num; i++) {
		info = &msg->device.devs[i];

		virtio_service_on_device_change(
				dev->parent, info->vid, info->reg);
	}
}

static void virtio_provider_tx_callback(struct virtqueue *vq)
{
	struct virtio_provider *provider = vq->vdev->priv;
	struct virtio_provider_message *msg;
	struct virtio_provider_buffer *buf;
	unsigned long flags;
	unsigned int _len;
	u32 diff;

	spin_lock_irqsave(&provider->lock, flags);
	while ((buf = virtqueue_get_buf(vq, &_len)) != NULL) {
		spin_unlock_irqrestore(&provider->lock, flags);

		msg = (struct virtio_provider_message *)buf->addr;
		if (msg->type == MSG_TIMESTAMP) {
			msg->timestamp.cr = semidrive_get_syscntr();
			diff = msg->timestamp.cr - msg->timestamp.cs;
			provider->total_latency += diff;

			if (diff < provider->min_latency)
				provider->min_latency = diff;

			if (diff > provider->max_latency) {
				provider->max_latency = diff;

				provider->idx = provider->latency_count;
				provider->cs = msg->timestamp.cs;
				provider->sr = msg->timestamp.sr;
				provider->ss = msg->timestamp.ss;
				provider->cr = msg->timestamp.cr;
			}

			provider->latency_count += 1;
		} else if (msg->type == MSG_DEVICE) {
			virtio_provider_handle_device(provider, msg);
		}

		spin_lock_irqsave(&provider->lock, flags);
		list_add_tail(&buf->list, &provider->buf_list);
	}
	spin_unlock_irqrestore(&provider->lock, flags);
}

static void virtio_provider_rx_callback(struct virtqueue *vq)
{
	struct device *dev = &vq->vdev->dev;
	struct virtio_provider *provider = vq->vdev->priv;
	struct virtio_provider_message *msg;
	struct scatterlist sg[1];
	unsigned long flags;
	unsigned int _len;
	u32 len;

	spin_lock_irqsave(&provider->lock, flags);
	while ((msg = virtqueue_get_buf(vq, &_len)) != NULL) {
		spin_unlock_irqrestore(&provider->lock, flags);

		if (msg->type == MSG_TEXT) {
			len = msg->text.len;
			dev_info(dev, "recv %u bytes:\n", len);
			msg->text.str[len] = '\0';
			dev_info(dev, "%s\n", msg->text.str);
		} else if (msg->type == MSG_TIMESTAMP) {
			/* TODO */
		} else if (msg->type == MSG_DEVICE) {
			virtio_provider_handle_device(provider, msg);
		} else {
			dev_warn(dev, "unexpected type %u\n", msg->type);
		}

		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(msg),
				PROVIDER_RX_BUFSIZE, offset_in_page(msg));
		spin_lock_irqsave(&provider->lock, flags);
		virtqueue_add_inbuf(vq, sg, 1, msg, GFP_ATOMIC);
	}
	spin_unlock_irqrestore(&provider->lock, flags);
}

static int virtio_provider_probe(struct virtio_device *vdev)
{
	struct device *dev = &vdev->dev;
	struct virtio_provider *provider;
	struct virtio_provider_buffer *buf;
	struct virtio_provider_message *msg;
	struct scatterlist sg[1];
	struct virtqueue *vqs[VQ_NUM];
	vq_callback_t *cbs[] = {
		virtio_provider_tx_callback,
		virtio_provider_rx_callback,
	};
	const char *names[] = { "tx_cb", "rx_cb" };
	unsigned int rxlen;
	unsigned long flags;
	phys_addr_t mem_base;
	size_t mem_size;
	void __iomem *base;
	u32 value;
	int err, i;

	provider = devm_kzalloc(dev,
			sizeof(struct virtio_provider), GFP_KERNEL);
	if (!provider) {
		dev_err(dev, "fail to alloc provider\n");
		return -ENOMEM;
	}

	vdev->priv = provider;
	provider->vdev = vdev;

	spin_lock_init(&provider->lock);
	INIT_LIST_HEAD(&provider->buf_list);

	err = virtio_find_vqs(vdev, VQ_NUM, vqs, cbs, names, NULL);
	if (err < 0) {
		dev_err(dev, "fail to find virtqueue (%d)\n", err);
		return err;
	}
	provider->tx = vqs[0];
	provider->rx = vqs[1];

	/* tell backend we're from ap domain */
	value = 1;
	virtio_cwrite(vdev, struct virtio_provider_config, ap_domain, &value);

	mem_size = virtio_service_get_provider_memory(dev->parent, &base, &mem_base);
	if (mem_size != VIRTIO_PROVIDER_BUFSIZE) {
		/* just in case */
		dev_err(dev, "invalid provider memory 0x%x(%p) 0x%x\n",
				mem_base, base, mem_size);
		return -ENOMEM;
	}

	provider->mem_base = mem_base;
	provider->mem_size = mem_size;
	provider->base = base;

	/* tx bufs add to list */
	for (i = 0; i < PROVIDER_TX_VRING_LEN; i++) {
		buf = &provider->__bufs[i];

		buf->addr = base;
		buf->size = PROVIDER_TX_BUFSIZE;
		INIT_LIST_HEAD(&buf->list);
		list_add_tail(&buf->list, &provider->buf_list);

		base += PROVIDER_TX_BUFSIZE;
	}

	/* rx bufs send to backend */
	rxlen = min(PROVIDER_RX_VRING_LEN,
			virtqueue_get_vring_size(provider->rx));
	for (i = 0; i < rxlen; i++) {
		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(base),
				PROVIDER_RX_BUFSIZE, offset_in_page(base));

		spin_lock_irqsave(&provider->lock, flags);
		virtqueue_add_inbuf(provider->rx, sg, 1, base, GFP_ATOMIC);
		spin_unlock_irqrestore(&provider->lock, flags);

		base += PROVIDER_RX_BUFSIZE;
	}

	virtqueue_kick(provider->rx);

	buf = list_first_entry(&provider->buf_list,
			struct virtio_provider_buffer, list);
	list_del(&buf->list);

	/* don't need cache ops since we're mapped as uc */
	msg = buf->addr;
	msg->type = MSG_DEVICE;
	msg->device.num = 0;

	sg_init_table(sg, 1);
	sg_set_page(sg, vmalloc_to_page(buf->addr),
			buf->size, offset_in_page(buf->addr));

	spin_lock_irqsave(&provider->lock, flags);
	virtqueue_add_outbuf(provider->tx, sg, 1, buf, GFP_ATOMIC);
	spin_unlock_irqrestore(&provider->lock, flags);

	virtqueue_kick(provider->tx);

	/* TODO debug node */

	return 0;
}

static void virtio_provider_remove(struct virtio_device *vdev)
{
	struct virtio_provider *provider = vdev->priv;
	struct device *dev = &vdev->dev;

	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
	devm_kfree(dev, provider);
}

static unsigned int virtio_provider_features[] = {
	VIRTIO_F_VERSION_1
};

static struct virtio_device_id virtio_provider_id_table[] = {
	{ VIRTIO_ID_PROVIDER, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_provider_driver = {
	.driver = {
		.name = "virtio-provider",
		.owner = THIS_MODULE,
	},
	.feature_table = virtio_provider_features,
	.feature_table_size = ARRAY_SIZE(virtio_provider_features),
	.id_table = virtio_provider_id_table,
	.probe = virtio_provider_probe,
	.remove = virtio_provider_remove,
};

module_virtio_driver(virtio_provider_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Semidrive virtio provider");
MODULE_AUTHOR("Semidrive Semiconductor");
