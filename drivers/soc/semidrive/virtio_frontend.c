#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/mailbox_client.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <uapi/linux/virtio_mmio.h>


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "vfe: " fmt

#define vdev_to_vfe(ptr) container_of(ptr, struct virtio_frontend, vdev)

/*
 * TODO
 * should be same as r5 cache line size
 * consider passing this value by mmio reg
 */
#define VQ_ALIGN 32

/* mailbox message queue length is 20 */
#define MSG_COUNT 20

#define MSG_ID_MODULO		59

/* msg index, offset in msgs array */
#define MSG_IDX_BITS		5
/* msg id, a unique id modulo MSG_ID_MODULO */
#define MSG_ID_BITS		6
/* offset type, 0 for reg area, 1 for cfg area */
#define MSG_OT_BITS		1
/* msg type, 0 for msg, 1 for ack */
#define MSG_MT_BITS		1
/* transfer type, 0 for oneway, 1 for has ack */
#define MSG_XT_BITS		1
/* transfer state, 0 for transfering, 1 for tx done */
#define MSG_TS_BITS		1
/* msg state, 0 for idle, 1 for busy */
#define MSG_MS_BITS		1

#define MSG_IDX_SHIFT		0
#define MSG_ID_SHIFT		(MSG_IDX_SHIFT+MSG_IDX_BITS)
#define MSG_OT_SHIFT		(MSG_ID_SHIFT+MSG_ID_BITS)
#define MSG_MT_SHIFT		(MSG_OT_SHIFT+MSG_OT_BITS)
#define MSG_XT_SHIFT		(MSG_MT_SHIFT+MSG_MT_BITS)
#define MSG_TS_SHIFT		(MSG_XT_SHIFT+MSG_XT_BITS)
#define MSG_MS_SHIFT		(MSG_TS_SHIFT+MSG_TS_BITS)

struct virtio_message {
	sd_msghdr_t head;
	u16 flag;
	u8 offset;
	u8 len;
} __attribute__((packed));

struct virtio_vq_info {
	struct list_head list;
	struct virtqueue *vq;
	void __iomem *addr;
};

struct virtio_frontend {
	struct device *dev;

	void __iomem *reg_base;
	phys_addr_t reg_phys_base;
	resource_size_t reg_size;
	void __iomem *vq_base;
	phys_addr_t vq_phys_base;
	resource_size_t vq_size;

	void __iomem *vq_cur_base;
	phys_addr_t vq_cur_phys_base;
	resource_size_t vq_cur_size;

	struct mbox_client client;
	struct mbox_chan *chan;
	struct completion msg_avail;
	spinlock_t msg_lock;
	u32 msg_id;
	struct virtio_message msgs[MSG_COUNT];

	struct task_struct *init_task;
	struct virtio_device vdev;
	spinlock_t lock;
	struct list_head vq_list;
};

static inline struct virtio_message *virtio_frontend_get_msg(struct virtio_frontend *vfe)
{
	struct virtio_message *msg = NULL;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&vfe->msg_lock, flags);
	for (i = 0; i < MSG_COUNT; i++) {
		if (!(vfe->msgs[i].flag & BIT(MSG_MS_SHIFT))) {
			msg = &vfe->msgs[i];
			msg->flag = 1u << MSG_MS_SHIFT;
			msg->flag |= (i & ((1u << MSG_IDX_BITS) - 1u)) << MSG_IDX_SHIFT;
			msg->flag |= (vfe->msg_id++ & ((1u << MSG_ID_BITS) - 1u)) << MSG_ID_SHIFT;

			if (vfe->msg_id >= MSG_ID_MODULO)
				vfe->msg_id = 0;

			break;
		}
	}
	spin_unlock_irqrestore(&vfe->msg_lock, flags);

	return msg;
}

static void virtio_frontend_send_reg_update(struct virtio_frontend *vfe, u8 offset, int reply)
{
	struct virtio_message *msg;
	unsigned long timeout;
	unsigned long flags;
	int wait = !!reply;
	u16 _flag = 0;
	u8 _offset;

	msg = virtio_frontend_get_msg(vfe);
	if (unlikely(!msg)) {
		/*
		 * TODO
		 * Current mailbox's implementation has a 20 length message
		 * queue. We just omit duplicated notification here for now.
		 */
		if (offset == VIRTIO_MMIO_QUEUE_NOTIFY)
			return;

		dev_err(vfe->dev, "not enough msg\n");
		return;
	}

	/*SD_RPROC_SAF*/
	mb_msg_init_head(&msg->head, 0, 0, true, sizeof(msg->head) + 4, 0x8f);
	msg->offset = offset;
	msg->len = 0;

	msg->flag |= 1u << MSG_TS_SHIFT;
	msg->flag |= wait << MSG_XT_SHIFT;

	mbox_send_message(vfe->chan, msg);

	if (wait) {
		timeout = msecs_to_jiffies(50);
		while ((timeout = wait_for_completion_timeout(&vfe->msg_avail, timeout))) {
			/* ack arrived */
			if (!(msg->flag & (1u << MSG_XT_SHIFT)))
				break;
		}

		if (!timeout) {
			spin_lock_irqsave(&vfe->msg_lock, flags);
			if (msg->flag & (1u << MSG_XT_SHIFT)) {
				_flag = msg->flag;
				_offset = msg->offset;

				if (unlikely(msg->flag & (1u << MSG_TS_SHIFT))) {
					/*
					 * mailbox driver still hold this buf, mark it
					 * oneway to let tx_done release it
					 */
					msg->flag &= ~(1u << MSG_XT_SHIFT);
				} else {
					/* expire this msg to prevend ack matching */
					msg->flag = 0;
					msg->offset = 0;
					msg->len = 0;
				}
			}
			spin_unlock_irqrestore(&vfe->msg_lock, flags);
		}

		if (_flag) {
			dev_warn(vfe->dev, "ack timeout flag %x offset %x\n", _flag, _offset);
		} else {
			/* ack arrived in time */
			msg->flag = 0;
			msg->offset = 0;
			msg->len = 0;
		}
	}
}

static void virtio_frontend_send_cfg_update(struct virtio_frontend *vfe, u8 offset, u8 len)
{
	struct virtio_message *msg;

	msg = virtio_frontend_get_msg(vfe);
	if (!msg) {
		dev_err(vfe->dev, "not enough msg\n");
		return;
	}

	/*SD_RPROC_SAF*/
	mb_msg_init_head(&msg->head, 0, 0, true, sizeof(msg->head) + 4, 0x8f);
	msg->offset = offset;
	msg->len = len;

	msg->flag |= 1u << MSG_OT_SHIFT;
	msg->flag |= 1u << MSG_TS_SHIFT;

	mbox_send_message(vfe->chan, msg);
}

static void virtio_frontend_mbox_rx_callback(struct mbox_client *client, void *mssg)
{
	struct virtio_frontend *vfe = container_of(client, struct virtio_frontend, client);
	struct virtio_message *msg = mssg, *msg2;
	struct virtio_vq_info *info;
	unsigned long flags;
	u32 status;

	if (unlikely(msg->flag & (1u << MSG_MT_SHIFT))) {
		/* validate if out-dated */
		spin_lock_irqsave(&vfe->msg_lock, flags);

		msg2 = &vfe->msgs[(msg->flag >> MSG_IDX_SHIFT) & ((1u << MSG_IDX_BITS) - 1u)];
		if (!((msg->flag ^ msg2->flag) & ~((1u << MSG_MT_SHIFT) | (1u << MSG_TS_SHIFT)))) {
			/* correct message ack */
			msg2->flag &= ~(1u << MSG_XT_SHIFT);
			complete_all(&vfe->msg_avail);
		}

		spin_unlock_irqrestore(&vfe->msg_lock, flags);

		return;
	}

	status = readl(vfe->reg_base + VIRTIO_MMIO_INTERRUPT_STATUS);
	writel(status, vfe->reg_base + VIRTIO_MMIO_INTERRUPT_ACK);
	/*
	 * ignore INTERRUPT_ACK since mailbox ensures message acknowledgement
	 */
	//virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_INTERRUPT_ACK, 0);

	if (likely(status & VIRTIO_MMIO_INT_VRING)) {
		spin_lock_irqsave(&vfe->lock, flags);
		list_for_each_entry(info, &vfe->vq_list, list)
			vring_interrupt(0, info->vq);
		spin_unlock_irqrestore(&vfe->lock, flags);
	}

	if (unlikely(status & VIRTIO_MMIO_INT_CONFIG)) {
		virtio_config_changed(&vfe->vdev);
	}
}

static void virtio_frontend_mbox_tx_done(struct mbox_client *client, void *mssg, int r)
{
	struct virtio_frontend *vfe = container_of(client, struct virtio_frontend, client);
	struct virtio_message *msg = mssg;
	unsigned long flags;

	spin_lock_irqsave(&vfe->msg_lock, flags);
	if (msg->flag & (1u << MSG_TS_SHIFT)) {
		/* clear transfer state to return ownership */
		msg->flag &= ~(1u << MSG_TS_SHIFT);

		/* our responsibility to release oneway message */
		if (!(msg->flag & (1u << MSG_XT_SHIFT))) {
			msg->flag = 0;
			msg->offset = 0;
			msg->len = 0;
		}
	}
	spin_unlock_irqrestore(&vfe->msg_lock, flags);
}

static bool virtio_frontend_notify(struct virtqueue *vq)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vq->vdev);

	writel(vq->index, vfe->reg_base + VIRTIO_MMIO_QUEUE_NOTIFY);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

	return true;
}

static void virtio_frontend_get(struct virtio_device *vdev,
		unsigned offset, void *buf, unsigned len)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);
	void __iomem *base = vfe->reg_base + VIRTIO_MMIO_CONFIG;
	u8 b;
	__le16 w;
	__le32 l;

	switch (len) {
	case 1:
		b = readb(base + offset);
		memcpy(buf, &b, sizeof(b));
		break;
	case 2:
		w = cpu_to_le16(readw(base + offset));
		memcpy(buf, &w, sizeof(w));
		break;
	case 4:
		l = cpu_to_le32(readl(base + offset));
		memcpy(buf, &l, sizeof(l));
		break;
	case 8:
		l = cpu_to_le32(readl(base + offset));
		memcpy(buf, &l, sizeof(l));
		l = cpu_to_le32(ioread32(base + offset + sizeof(l)));
		memcpy(buf + sizeof(l), &l, sizeof(l));
		break;
	default:
		BUG();
		break;
	}
}

static void virtio_frontend_set(struct virtio_device *vdev,
				     unsigned offset, const void *buf, unsigned len)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);
	void __iomem *base = vfe->reg_base + VIRTIO_MMIO_CONFIG;
	u8 b;
	__le16 w;
	__le32 l;

	switch (len) {
	case 1:
		memcpy(&b, buf, sizeof(b));
		writeb(b, base + offset);
		break;
	case 2:
		memcpy(&w, buf, sizeof(w));
		writew(le16_to_cpu(w), base + offset);
		break;
	case 4:
		memcpy(&l, buf, sizeof(l));
		writel(le32_to_cpu(l), base + offset);
		break;
	case 8:
		memcpy(&l, buf, sizeof(l));
		writel(le32_to_cpu(l), base + offset);
		memcpy(&l, buf + sizeof(l), sizeof(l));
		writel(le32_to_cpu(l), base + offset + sizeof(l));
		break;
	default:
		BUG();
		break;
	}

	virtio_frontend_send_cfg_update(vfe, offset, len);
}

static u32 virtio_frontend_generation(struct virtio_device *vdev)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);

	return readl(vfe->reg_base + VIRTIO_MMIO_CONFIG_GENERATION);
}

static u8 virtio_frontend_get_status(struct virtio_device *vdev)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);

	return readl(vfe->reg_base + VIRTIO_MMIO_STATUS) & 0xff;
}

static void virtio_frontend_set_status(struct virtio_device *vdev, u8 status)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);

	BUG_ON(status == 0);

	/* TODO some status needs device to confirm: VIRTIO_CONFIG_S_FEATURES_OK */
	writel(status, vfe->reg_base + VIRTIO_MMIO_STATUS);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_STATUS, 0);
}

static void virtio_frontend_reset(struct virtio_device *vdev)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);

	writel(0, vfe->reg_base + VIRTIO_MMIO_STATUS);

	/* wait device reset done */
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_STATUS, 1);
}

static struct virtqueue *virtio_frontend_setup_vq(struct virtio_device *vdev,
		unsigned index, void (*callback)(struct virtqueue *vq), const char *name, bool ctx)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);
	struct virtqueue *vq;
	struct virtio_vq_info *info;
	unsigned long flags;
	unsigned int num;
	struct vring vring;
	resource_size_t size;
	phys_addr_t desc, avail, used;

	if (!name) {
		dev_err(vfe->dev, "vq[%u] invalid empty name\n", index);
		return NULL;
	}

	writel(index, vfe->reg_base + VIRTIO_MMIO_QUEUE_SEL);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_QUEUE_SEL, 1);

	if (readl(vfe->reg_base + VIRTIO_MMIO_QUEUE_READY)) {
		dev_err(vfe->dev, "vq[%u] already been set up\n", index);
		return ERR_PTR(-ENOENT);
	}

	num = readl(vfe->reg_base + VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (!num) {
		dev_err(vfe->dev, "vq[%u] no capacity\n", index);
		return ERR_PTR(-ENOENT);
	}

	size = ALIGN(vring_size(num, VQ_ALIGN), VQ_ALIGN);
	if (size > vfe->vq_cur_size) {
		dev_err(vfe->dev, "vq[%u] num %u require %llx bytes but only %llx left\n",
				index, num, size, vfe->vq_cur_size);
		return ERR_PTR(-ENOMEM);
	}

	info = devm_kzalloc(vfe->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(vfe->dev, "vq[%u] fail to alloc\n", index);
		return ERR_PTR(-ENOMEM);
	}

	/* make sure virtqueue is clear */
	memset_io(vfe->vq_cur_base, 0, size);

	/* strong barrier */
	vq = vring_new_virtqueue(index, num, VQ_ALIGN, &vfe->vdev, false,
			ctx, vfe->vq_cur_base, virtio_frontend_notify, callback, name);
	if (!vq) {
		dev_err(vfe->dev, "vq[%u] new virtqueue fails\n", index);
		kfree(info);
		return ERR_PTR(-ENOMEM);
	}

	vq->priv = info;
	info->vq = vq;

	vring_init(&vring, num, (void *)vfe->vq_cur_phys_base, VQ_ALIGN);

	writel(virtqueue_get_vring_size(vq), vfe->reg_base + VIRTIO_MMIO_QUEUE_NUM);

	desc = (phys_addr_t)vring.desc;
	writel((u32)desc, vfe->reg_base + VIRTIO_MMIO_QUEUE_DESC_LOW);
	writel((u32)(desc >> 32), vfe->reg_base + VIRTIO_MMIO_QUEUE_DESC_HIGH);

	avail = (phys_addr_t)vring.avail;
	writel((u32)avail, vfe->reg_base + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
	writel((u32)(avail >> 32), vfe->reg_base + VIRTIO_MMIO_QUEUE_AVAIL_HIGH);

	used = (phys_addr_t)vring.used;
	writel((u32)used, vfe->reg_base + VIRTIO_MMIO_QUEUE_USED_LOW);
	writel((u32)(used >> 32), vfe->reg_base + VIRTIO_MMIO_QUEUE_USED_HIGH);

	writel(1, vfe->reg_base + VIRTIO_MMIO_QUEUE_READY);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_QUEUE_READY, 1);

	spin_lock_irqsave(&vfe->lock, flags);
	list_add(&info->list, &vfe->vq_list);
	spin_unlock_irqrestore(&vfe->lock, flags);

	vfe->vq_cur_base += size;
	vfe->vq_cur_phys_base += size;
	vfe->vq_cur_size -= size;

	dev_info(vfe->dev,
			"vq[%u] (%p) num %u desc %llx avail %llx used %llx size %llx\n",
			index, vq, num, desc, avail, used, size);

	return vq;
}

static void virtio_frontend_del_vq(struct virtqueue *vq)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vq->vdev);
	struct virtio_vq_info *info = vq->priv;
	unsigned long flags;

	spin_lock_irqsave(&vfe->lock, flags);
	list_del(&info->list);
	spin_unlock_irqrestore(&vfe->lock, flags);

	writel(vq->index, vfe->reg_base + VIRTIO_MMIO_QUEUE_SEL);
	writel(0, vfe->reg_base + VIRTIO_MMIO_QUEUE_READY);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_QUEUE_READY, 1);

	vring_del_virtqueue(vq);

	kfree(info);

	vfe->vq_cur_base = vfe->vq_base;
	vfe->vq_cur_phys_base = vfe->vq_phys_base;
	vfe->vq_cur_size = vfe->vq_size;
}

static void virtio_frontend_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;

	list_for_each_entry_safe(vq, n, &vdev->vqs, list)
		virtio_frontend_del_vq(vq);
}

static int virtio_frontend_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		struct virtqueue *vqs[], vq_callback_t *callbacks[],
		const char * const names[], const bool *ctx, struct irq_affinity *desc)
{
	int i;

	for (i = 0; i < nvqs; i++) {
		vqs[i] = virtio_frontend_setup_vq(vdev, i, callbacks[i],
				names[i], ctx ? ctx[i] : false);
		if (IS_ERR(vqs[i])) {
			virtio_frontend_del_vqs(vdev);
			return PTR_ERR(vqs[i]);
		}
	}

	return 0;
}

static u64 virtio_frontend_get_features(struct virtio_device *vdev)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);
	u64 features;

	writel(1, vfe->reg_base + VIRTIO_MMIO_DEVICE_FEATURES_SEL);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
	features = readl(vfe->reg_base + VIRTIO_MMIO_DEVICE_FEATURES);
	features <<= 32;

	writel(0, vfe->reg_base + VIRTIO_MMIO_DEVICE_FEATURES_SEL);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
	features |= readl(vfe->reg_base + VIRTIO_MMIO_DEVICE_FEATURES);

	return features;
}

static int virtio_frontend_finalize_features(struct virtio_device *vdev)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);

	writel(1, vfe->reg_base + VIRTIO_MMIO_DRIVER_FEATURES_SEL);
	writel((u32)(vdev->features >> 32),
			vfe->reg_base + VIRTIO_MMIO_DRIVER_FEATURES);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_DRIVER_FEATURES, 1);

	writel(0, vfe->reg_base + VIRTIO_MMIO_DRIVER_FEATURES_SEL);
	writel((u32)vdev->features,
			vfe->reg_base + VIRTIO_MMIO_DRIVER_FEATURES);
	virtio_frontend_send_reg_update(vfe, VIRTIO_MMIO_DRIVER_FEATURES, 1);

	return 0;
}

static const char *virtio_frontend_bus_name(struct virtio_device *vdev)
{
	struct virtio_frontend *vfe = vdev_to_vfe(vdev);
	struct platform_device *pdev = to_platform_device(vfe->dev);

	return pdev->name;
}

static struct virtio_config_ops virtio_frontend_config_ops = {
	.get			= virtio_frontend_get,
	.set			= virtio_frontend_set,
	.generation		= virtio_frontend_generation,
	.get_status		= virtio_frontend_get_status,
	.set_status		= virtio_frontend_set_status,
	.reset			= virtio_frontend_reset,
	.find_vqs		= virtio_frontend_find_vqs,
	.del_vqs		= virtio_frontend_del_vqs,
	.get_features		= virtio_frontend_get_features,
	.finalize_features	= virtio_frontend_finalize_features,
	.bus_name		= virtio_frontend_bus_name,
};

static void virtio_frontend_device_release(struct device *dev)
{
	/* this handler is provided so driver core doesn't yell at us */
}

static int virtio_frontend_vdev_init(void *data)
{
	struct virtio_frontend *vfe = data;
	u32 magic, version;

	magic = readl(vfe->reg_base + VIRTIO_MMIO_MAGIC_VALUE);
	if (magic != ('v' | 'i' << 8 | 'r' << 16 | 't' << 24)) {
		dev_err(vfe->dev, "invalid magic 0x%08x\n", magic);
		return -ENODEV;
	}

	version = readl(vfe->reg_base + VIRTIO_MMIO_VERSION);
	if (version != 2) {
		dev_err(vfe->dev, "unexpected version %u device\n", version);
		return -ENODEV;
	}

	vfe->vdev.id.device = readl(vfe->reg_base + VIRTIO_MMIO_DEVICE_ID);
	if (!vfe->vdev.id.device) {
		dev_err(vfe->dev, "invalid device id %u\n", vfe->vdev.id.device);
		return -ENODEV;
	}
	vfe->vdev.id.vendor = readl(vfe->reg_base + VIRTIO_MMIO_VENDOR_ID);

	vfe->vdev.config = &virtio_frontend_config_ops;
	vfe->vdev.dev.parent = vfe->dev;
	vfe->vdev.dev.release = virtio_frontend_device_release;

	return register_virtio_device(&vfe->vdev);
}

static int virtio_frontend_res_init(struct virtio_frontend *vfe)
{
	struct platform_device *pdev = to_platform_device(vfe->dev);
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(vfe->dev, "fail to get register memory\n");
		return -ENOMEM;
	}

	if (!devm_request_mem_region(vfe->dev, res->start,
				resource_size(res), pdev->name)) {
		dev_err(vfe->dev, "fail to request register region\n");
		return -EBUSY;
	}

	vfe->reg_phys_base = res->start;
	vfe->reg_size = resource_size(res);

	vfe->reg_base = devm_ioremap_nocache(vfe->dev, res->start, resource_size(res));
	if (!vfe->reg_base) {
		dev_err(vfe->dev, "fail to map register memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(vfe->dev, "fail to get virtqueue memory\n");
		return -ENOMEM;
	}

	if (!devm_request_mem_region(vfe->dev, res->start,
				resource_size(res), pdev->name)) {
		dev_err(vfe->dev, "fail to request virtqueue region\n");
		return -EBUSY;
	}

	vfe->vq_phys_base = res->start;
	vfe->vq_size = resource_size(res);

	vfe->vq_base = devm_ioremap_nocache(vfe->dev, res->start, resource_size(res));
	if (!vfe->vq_base) {
		dev_err(vfe->dev, "fail to map virtqueue memory\n");
		return -ENOMEM;
	}

	dev_info(vfe->dev, "reg(%p) addr %llx size %llx vq(%p) addr %llx size %llx\n",
			vfe->reg_base, vfe->reg_phys_base, vfe->reg_size,
			vfe->vq_base, vfe->vq_phys_base, vfe->vq_size);

	vfe->vq_cur_base = vfe->vq_base;
	vfe->vq_cur_phys_base = vfe->vq_phys_base;
	vfe->vq_cur_size = vfe->vq_size;

	// vfe->vdev.dev.init_name = "vfe-mmio";
	if (!of_reserved_mem_device_init_by_idx(&vfe->vdev.dev, pdev->dev.of_node, 0)) {
		dev_info(vfe->dev, "device has reserved dma pool\n");
	}

	return 0;
}

static int virtio_frontend_irq_init(struct virtio_frontend *vfe)
{
	init_completion(&vfe->msg_avail);

	spin_lock_init(&vfe->msg_lock);
	vfe->msg_id = 0;

	vfe->client.dev = vfe->dev;
	vfe->client.tx_block = false;
	vfe->client.tx_tout = 1000;
	vfe->client.knows_txdone = false;
	vfe->client.rx_callback = virtio_frontend_mbox_rx_callback;
	vfe->client.tx_done = virtio_frontend_mbox_tx_done;

	vfe->chan = mbox_request_channel(&vfe->client, 0);
	if (IS_ERR(vfe->chan)) {
		dev_err(vfe->dev, "fail to request mbox channel\n");
		return PTR_ERR(vfe->chan);
	}

	return 0;
}

static void virtio_frontend_irq_deinit(struct virtio_frontend *vfe)
{
	mbox_free_channel(vfe->chan);
}

static ssize_t dump_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct virtio_frontend *vfe = platform_get_drvdata(pdev);
	struct virtio_message *msg;
	int i;

	dev_info(vfe->dev, "mbox msgs:\n");
	for (i = 0; i < MSG_COUNT; i++) {
		msg = &vfe->msgs[i];
		dev_info(vfe->dev, "%d flag %04x offset %x len %u\n",
				i, msg->flag, msg->offset, msg->len);
	}

	return len;
}

static DEVICE_ATTR_WO(dump);

static struct attribute *virtio_frontend_attributes[] = {
	&dev_attr_dump.attr,
	NULL
};

static const struct attribute_group virtio_frontend_attr_group = {
	.name = "vfe-debug",
	.attrs = virtio_frontend_attributes,
};

static int virtio_frontend_probe(struct platform_device *pdev)
{
	struct virtio_frontend *vfe;
	int err;

	vfe = devm_kzalloc(&pdev->dev, sizeof(*vfe), GFP_KERNEL);
	if (!vfe) {
		dev_err(&pdev->dev, "fail to alloc virtio_frontend\n");
		return -ENOMEM;
	}

	vfe->dev = &pdev->dev;
	platform_set_drvdata(pdev, vfe);

	err = virtio_frontend_res_init(vfe);
	if (err < 0) {
		dev_err(vfe->dev, "fail to init virtio resource\n");
		return err;
	}

	err = virtio_frontend_irq_init(vfe);
	if (err < 0) {
		dev_err(vfe->dev, "fail to init virtio interrupt\n");
		return err;
	}

	spin_lock_init(&vfe->lock);
	INIT_LIST_HEAD(&vfe->vq_list);

	// err = sysfs_create_group(&vfe->dev->kobj, &virtio_frontend_attr_group);
	// if (err < 0) {
	// 	dev_err(vfe->dev, "fail to create debug interface\n");
	// 	goto irq_deinit;
	// }

	vfe->init_task = kthread_create(virtio_frontend_vdev_init, vfe, "vfe-init");
	if (IS_ERR(vfe->init_task)) {
		dev_err(vfe->dev, "fail to create init thread\n");
		err = PTR_ERR(vfe->init_task);
		goto irq_deinit;
	}
	wake_up_process(vfe->init_task);

	return 0;

irq_deinit:
	virtio_frontend_irq_deinit(vfe);

	return err;
}

static int virtio_frontend_remove(struct platform_device *pdev)
{
	struct virtio_frontend *vfe = platform_get_drvdata(pdev);

	// sysfs_remove_group(&vfe->dev->kobj, &virtio_frontend_attr_group);
	virtio_frontend_irq_deinit(vfe);

	return 0;
}

static const struct of_device_id virtio_frontend_match_table[] = {
	{ .compatible = "sd,vfe-mmio", },
	{ /* sentinel */ }
};

static struct platform_driver virtio_frontend_driver = {
	.driver = {
		   .name = "virtio-frontend",
		   .owner = THIS_MODULE,
		   .of_match_table = virtio_frontend_match_table,
	},
	.probe = virtio_frontend_probe,
	.remove = virtio_frontend_remove,
};

module_platform_driver(virtio_frontend_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Semidrive virtio device frontend");
MODULE_AUTHOR("Semidrive Semiconductor");
