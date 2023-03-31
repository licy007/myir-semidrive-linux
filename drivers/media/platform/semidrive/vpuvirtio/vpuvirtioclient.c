/*
 * Copyright (c) 2022 Semidrive Semiconductor, Inc.
 * All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/soc/semidrive/sys_counter.h>
#include <linux/string.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <uapi/linux/virtio_ids.h>

#include "vpuvirtioclient.h"

#define TX_COUNT 16u
#define RX_COUNT 16u

#define BUF_SIZE 512 // for data of sizeof(vpu_ipcc_data_t)

// #define VPU_VIRTIO_DEBUG

struct vpu_virtio_buffer {
	struct list_head list;
	size_t size;
	void *addr;
	dma_addr_t dma_addr;
};

struct vpu_virtio_client {
	struct virtio_device *vdev;
	struct virtqueue *tx, *rx;
	spinlock_t tx_lock;
	struct list_head list;
	struct vpu_virtio_buffer buf_pool;
	struct vpu_virtio_buffer bufs[TX_COUNT + RX_COUNT];

	int32_t virtio_cb_err;
	struct completion virtio_done;
	vpu_ipcc_data_t ipcc_msg_send;
	vpu_ipcc_data_t ipcc_msg_return;
};

struct virtio_device *g_virt_vpu_devs[VPU_DEVICES_MAX];

static inline struct vpu_virtio_buffer *vpu_virtio_buf_try_get(struct vpu_virtio_client *client)
{
	struct vpu_virtio_buffer *buf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&client->tx_lock, flags);
	if (!list_empty(&client->list)) {
		buf = list_first_entry(&client->list, struct vpu_virtio_buffer, list);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&client->tx_lock, flags);

	return buf;
}

static inline struct vpu_virtio_buffer *vpu_virtio_buf_get(struct vpu_virtio_client *client)
{
	struct vpu_virtio_buffer *buf = NULL;

	while (!(buf = vpu_virtio_buf_try_get(client)))
		cpu_relax();

	return buf;
}

static void vpu_virtio_tx_callback(struct virtqueue *vq)
{
	struct vpu_virtio_client *client = vq->vdev->priv;
	struct vpu_virtio_buffer *buf = NULL;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&client->tx_lock, flags);
	while ((buf = virtqueue_get_buf(client->tx, &len)) != NULL) {
		list_add(&buf->list, &client->list);
	}
	spin_unlock_irqrestore(&client->tx_lock, flags);
}

static void vpu_virtio_rx_callback(struct virtqueue *vq)
{
	struct virtio_device *vdev = vq->vdev;
	struct vpu_virtio_client *client = vdev->priv;
	struct vpu_virtio_buffer *buf = NULL;
	vpu_ipcc_data_t *msg = NULL;
	struct scatterlist sg[1];
	unsigned int len;

	while ((buf = virtqueue_get_buf(client->rx, &len)) != NULL) {

#ifdef VPU_VIRTIO_DEBUG
		dev_info(&vdev->dev, "recv %u addr: %p\n", len, buf->addr);
#endif

		if (len != sizeof(vpu_ipcc_data_t)) {
			dev_err(&vdev->dev,
					"unexpected message length received from backend (received %d bytes while "
					"%zu bytes expected)\n",
					len, sizeof(vpu_ipcc_data_t));

			goto NEXT;
		}

		__inval_dcache_area(buf->addr, len);
		msg = (vpu_ipcc_data_t *)buf->addr;
		if (msg->cmd == VPU_IPC_HEARTBEAT) {
			// headbeat packet
			if (msg->return_value == VPU_IPC_RETURN_INVAILD) {
				msg->return_value = VPU_IPC_RETUERN_OK;
				vpu_virtio_send_async(vdev->id.device, msg);
			}
		} else if (msg->cmd == VPU_IPC_CLEAR_INSTANCE) {
			// do nothing
		} else {
			/*
			 * all is fine,
			 * update status & complete command
			 */
			client->virtio_cb_err = msg->return_value;
			memcpy(&client->ipcc_msg_return, msg, sizeof(*msg));
			complete(&client->virtio_done);
		}

	NEXT:
		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(buf->addr), buf->size, offset_in_page(buf->addr));
		virtqueue_add_inbuf(client->rx, sg, 1, buf, GFP_ATOMIC);
	}
}

int vpu_virtio_send_sync(uint32_t dev_id, vpu_ipcc_data_t *msg)
{
	struct virtio_device *vdev = NULL;
	struct vpu_virtio_client *client = NULL;
	struct vpu_virtio_buffer *buffer = NULL;
	struct scatterlist sg[1];
	unsigned long flags;

	if (dev_id >= VPU_DEVICES_MAX || !g_virt_vpu_devs[dev_id]) {
		pr_err("[%s][%d] invaild vpu virtio device id:%d\n", __func__, __LINE__, dev_id);
		return -1;
	}

	vdev = g_virt_vpu_devs[dev_id];
	client = vdev->priv;

	buffer = vpu_virtio_buf_try_get(client);
	if (!buffer) {
		dev_err(&vdev->dev, "not enough buffer\n");
		return -1;
	}

	init_completion(&client->virtio_done);
	memcpy(buffer->addr, msg, sizeof(vpu_ipcc_data_t));

	// dma_sync_single_for_device(&vdev->dev, buffer->dma_addr, buffer->size, DMA_TO_DEVICE);
	__flush_dcache_area(buffer->addr, sizeof(vpu_ipcc_data_t));

	sg_init_table(sg, 1);
	sg_set_page(sg, vmalloc_to_page(buffer->addr), buffer->size, offset_in_page(buffer->addr));
	spin_lock_irqsave(&client->tx_lock, flags);
	virtqueue_add_outbuf(client->tx, sg, 1, buffer, GFP_ATOMIC);
	spin_unlock_irqrestore(&client->tx_lock, flags);
	virtqueue_kick(client->tx);

#ifdef VPU_VIRTIO_DEBUG
	dev_info(&vdev->dev, "send %zu bytes\n", sizeof(vpu_ipcc_data_t));
#endif

	/* wait for acknowledge */
	if (!wait_for_completion_timeout(&client->virtio_done, msecs_to_jiffies(VPU_VIRTIO_TIMEOUT))) {
		dev_err(&vdev->dev,
				"ipc: failed to send, timeout waiting for %s callback (cmd:%d "
				"size=%d, data=%p)\n",
				__func__, msg->cmd, msg->cmd_param_full_length, msg->cmd_param);
		return -ETIMEDOUT;
	}

	/* command completed, check error */
	if (client->virtio_cb_err < 0) {
		dev_err(&vdev->dev,
				"ipc: failed to send, send sync completed but with error (%d) (cmd:%d size=%d, "
				"data=%p)\n",
				client->virtio_cb_err, msg->cmd, msg->cmd_param_full_length, msg->cmd_param);
		return -EIO;
	}

	memcpy(msg, &client->ipcc_msg_return, sizeof(vpu_ipcc_data_t));

	return 0;
}

int vpu_virtio_send_async(uint32_t dev_id, vpu_ipcc_data_t *msg)
{
	struct virtio_device *vdev = NULL;
	struct vpu_virtio_client *client = NULL;
	struct vpu_virtio_buffer *buffer = NULL;
	struct scatterlist sg[1];
	unsigned long flags;

	if (dev_id >= VPU_DEVICES_MAX || !g_virt_vpu_devs[dev_id]) {
		pr_err("[%s][%d] invaild vpu virtio device id:%d\n", __func__, __LINE__, dev_id);
		return -1;
	}

	vdev = g_virt_vpu_devs[dev_id];
	client = vdev->priv;

	buffer = vpu_virtio_buf_try_get(client);
	if (!buffer) {
		dev_err(&vdev->dev, "not enough buffer\n");
		return -1;
	}

	memcpy(buffer->addr, msg, sizeof(vpu_ipcc_data_t));

	// dma_sync_single_for_device(&vdev->dev, buffer->dma_addr, buffer->size, DMA_TO_DEVICE);
	__flush_dcache_area(buffer->addr, sizeof(vpu_ipcc_data_t));

	sg_init_table(sg, 1);
	sg_set_page(sg, vmalloc_to_page(buffer->addr), buffer->size, offset_in_page(buffer->addr));
	spin_lock_irqsave(&client->tx_lock, flags);
	virtqueue_add_outbuf(client->tx, sg, 1, buffer, GFP_ATOMIC);
	spin_unlock_irqrestore(&client->tx_lock, flags);
	virtqueue_kick(client->tx);

#ifdef VPU_VIRTIO_DEBUG
	dev_info(&vdev->dev, "send %zu bytes\n", sizeof(vpu_ipcc_data_t));
#endif

	return 0;
}

int vpu_virtio_send_headbeat_pack(uint32_t dev_id)
{
	struct virtio_device *vdev = NULL;
	struct vpu_virtio_client *client = NULL;

	if (dev_id >= VPU_DEVICES_MAX || !g_virt_vpu_devs[dev_id]) {
		pr_err("[%s][%d] invaild vpu virtio device id:%d\n", __func__, __LINE__, dev_id);
		return -1;
	}

	vdev = g_virt_vpu_devs[dev_id];
	client = vdev->priv;

	client->ipcc_msg_send.cmd = VPU_IPC_HEARTBEAT;
	client->ipcc_msg_send.cmd_param_full_length = 0;
	client->ipcc_msg_send.current_packed_start_offset = 0;
	client->ipcc_msg_send.current_packed_end_offset = 0;

	return vpu_virtio_send_async(dev_id, &client->ipcc_msg_send);
}

int vpu_virtio_send_clear_instance(uint32_t dev_id, uint32_t instance)
{
	struct virtio_device *vdev = NULL;
	struct vpu_virtio_client *client = NULL;

	if (dev_id >= VPU_DEVICES_MAX || !g_virt_vpu_devs[dev_id]) {
		pr_err("[%s][%d] invaild vpu virtio device id:%d\n", __func__, __LINE__, dev_id);
		return -1;
	}

	vdev = g_virt_vpu_devs[dev_id];
	client = vdev->priv;

	client->ipcc_msg_send.cmd = VPU_IPC_CLEAR_INSTANCE;
	*((uint32_t *)client->ipcc_msg_send.cmd_param) = instance;
	client->ipcc_msg_send.cmd_param_full_length = sizeof(uint32_t);
	client->ipcc_msg_send.current_packed_start_offset = 0;
	client->ipcc_msg_send.current_packed_end_offset = sizeof(uint32_t);

	return vpu_virtio_send_async(dev_id, &client->ipcc_msg_send);
}

static int vpu_virtio_probe(struct virtio_device *vdev)
{
	struct vpu_virtio_client *client;
	struct vpu_virtio_buffer *buf;
	struct virtqueue *vqs[2];
	vq_callback_t *cbs[] = {vpu_virtio_tx_callback, vpu_virtio_rx_callback};
	static const char *const names[] = {"tx", "rx"};
	int err, i;

	if (!vdev || !vdev->dev.parent)
		return -ENODEV;

	client = kzalloc(sizeof(struct vpu_virtio_client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	vdev->priv = client;
	client->vdev = vdev;
	spin_lock_init(&client->tx_lock);
	INIT_LIST_HEAD(&client->list);

	// tx & rx buffers
	client->buf_pool.size = BUF_SIZE * ARRAY_SIZE(client->bufs);
	// alloc dma buffer from parent dev
	client->buf_pool.addr = dma_alloc_coherent(vdev->dev.parent, client->buf_pool.size,
											   &client->buf_pool.dma_addr, GFP_KERNEL);
	if (!client->buf_pool.addr) {
		dev_err(&vdev->dev, "fail to alloc dmabuf\n");
		goto ERROR;
	}

	dev_info(&vdev->dev, "alloc dmabuf pool %p size %zu at %x\n", client->buf_pool.dma_addr,
			 client->buf_pool.size, client->buf_pool.addr);

	for (i = 0; i < ARRAY_SIZE(client->bufs); i++) {
		buf = &client->bufs[i];

		buf->size = BUF_SIZE;
		buf->addr = client->buf_pool.addr + BUF_SIZE * i;
		INIT_LIST_HEAD(&buf->list);
		list_add(&buf->list, &client->list);
	}

	err = virtio_find_vqs(client->vdev, 2, vqs, cbs, names, NULL);
	if (err)
		goto ERROR;

	client->tx = vqs[0];
	client->rx = vqs[1];

	dev_info(&vdev->dev, "find vqs: tx %p rx %p, total bufs:%d\n", client->tx, client->rx, i);

	virtio_device_ready(vdev);

	// fill rx queue:
	// All buffers in the transmission process are provided and maintained by the client,
	// where rx and tx are relative to the client,
	// First put it into the rx buffer for service use, and keep the tx buffer for writing by self
	{
		struct scatterlist sg[1];
		int size;

		size = min(RX_COUNT, virtqueue_get_vring_size(client->rx));

		for (i = 0; i < size; i++) {
			buf = list_first_entry(&client->list, struct vpu_virtio_buffer, list);
			list_del(&buf->list);
			sg_init_table(sg, 1);
			sg_set_page(sg, vmalloc_to_page(buf->addr), buf->size, offset_in_page(buf->addr));
			virtqueue_add_inbuf(client->rx, sg, 1, buf, GFP_ATOMIC);
		}
		virtqueue_kick(client->rx);

		dev_info(&vdev->dev, "add %u bufs in rx queue\n", size);
	}

	if (vdev->id.device == VIRTIO_ID_VIDEO_CODA988) {
		g_virt_vpu_devs[VPU_DEVICES_CODA988] = vdev;
		dev_info(&vdev->dev, "coda988 ready\n");
	} else if (vdev->id.device == VIRTIO_ID_VIDEO_WAVE412) {
		g_virt_vpu_devs[VPU_DEVICES_WAVE412] = vdev;
		dev_info(&vdev->dev, "wave412 ready\n");
	}

	return 0;

ERROR:
	if (client->buf_pool.addr)
		dma_free_coherent(vdev->dev.parent, client->buf_pool.size, client->buf_pool.addr,
						  client->buf_pool.dma_addr);
	kfree(client);

	return err;
}

static void vpu_virtio_remove(struct virtio_device *vdev)
{
	if (!vdev || !vdev->priv)
		return;

	struct vpu_virtio_client *client = vdev->priv;

	dev_info(&vdev->dev, "[%s][%d]\n", __func__, __LINE__);

	if (vdev->id.device == VIRTIO_ID_VIDEO_CODA988) {
		g_virt_vpu_devs[VPU_DEVICES_CODA988] = NULL;
	} else if (vdev->id.device == VIRTIO_ID_VIDEO_WAVE412) {
		g_virt_vpu_devs[VPU_DEVICES_WAVE412] = NULL;
	}

	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);

	if (client->buf_pool.addr)
		dma_free_coherent(vdev->dev.parent, client->buf_pool.size, client->buf_pool.addr,
						  client->buf_pool.dma_addr);

	kfree(client);
}

static unsigned int vpu_virtio_features[] = {
	VIRTIO_F_VERSION_1
	/* none */
};

static struct virtio_device_id vpu_virtio_id_table[] = {
	{VIRTIO_ID_VIDEO_CODA988, VIRTIO_DEV_ANY_ID},
	{VIRTIO_ID_VIDEO_WAVE412, VIRTIO_DEV_ANY_ID},
	{0},
};

static struct virtio_driver vpu_virtio_driver = {
	.driver =
		{
			.name = "vpu-virtio",
			.owner = THIS_MODULE,
		},
	.feature_table = vpu_virtio_features,
	.feature_table_size = ARRAY_SIZE(vpu_virtio_features),
	.id_table = vpu_virtio_id_table,
	.probe = vpu_virtio_probe,
	.remove = vpu_virtio_remove,
};

module_virtio_driver(vpu_virtio_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("semidrive vpu virtio driver");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
