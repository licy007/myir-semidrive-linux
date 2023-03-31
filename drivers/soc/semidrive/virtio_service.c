#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ring.h>
#include <uapi/linux/virtio_mmio.h>
#include "virtio_service.h"

#define MSG_MT_REG 0
#define MSG_MT_CFG 1
#define MSG_MT_BOOT 2
#define MSG_MT_IRQ 3

#define WORK_MSG 0x1
#define WORK_BOOT 0x2
#define WORK_DEV 0x3
#define WORK_MASK 0x3
#define WORK_F_NOTIFY BIT(31)

#define WORK_COUNT 64

/* align vring to 256 bytes */
#define VRING_ALIGN_ORDER 8

/* same as backend */
#define VRING_MAX_NUM 2

/* a55 cacheline size */
#define VRING_ALIGN 64

#define FRONTEND_NAME_MAX_LEN 32

#define to_frontend(ptr) container_of(ptr, struct virtio_frontend, vdev)

struct virtio_frontend {
	struct virtio_device vdev;
	void *service;
	u32 vid;
	phys_addr_t reg_phys;
	void __iomem *reg;
	char name[FRONTEND_NAME_MAX_LEN];
	struct list_head list;
	struct virtqueue *vqs[VRING_MAX_NUM];
	void *vq_addrs[VRING_MAX_NUM];
	size_t vq_sizes[VRING_MAX_NUM];
};

struct virtio_service_message {
	sd_msghdr_t head;

	u16 mt : 2;
	u16 ac : 1;
	u16 ar : 1;
	u16 mid : 6;
	u16 vid : 6;

	u8 offset;
	u8 length;
} __attribute__((packed));

struct virtio_service_work {
	u32 flags;
	union {
		struct {
			struct virtio_service_message msg;
		} msg_param;
		struct {
			u32 vid;
			u32 addr;
		} dev_param;
	};
	struct completion done;
	u32 ref_count;
	struct list_head list;
};

struct virtio_service {
	struct device *dev;
	int ready;
	int boot;
	/* vdev of provider object */
	struct virtio_device *provider;

	phys_addr_t mem_base;
	resource_size_t mem_size;
	void __iomem *base;
	struct gen_pool *mem_pool;

	struct mbox_client client;
	struct mbox_chan *chan;

	spinlock_t work_lock;
	struct list_head pending_works;
	struct list_head idle_works;
	struct virtio_service_work __works[WORK_COUNT];

	struct completion kick_worker;
	struct completion worker_exited;
	int should_exit;
	struct task_struct *worker_thread;

	spinlock_t dev_lock;
	struct list_head dev_list;
};

/*
 * get work from idle list and upref it
 */
static struct virtio_service_work *
virtio_service_get_work(struct virtio_service *service)
{
	struct device *dev = service->dev;
	struct virtio_service_work *work;
	unsigned long flags;

	spin_lock_irqsave(&service->work_lock, flags);
	work = list_first_entry_or_null(&service->idle_works,
					struct virtio_service_work, list);
	if (work) {
		work->ref_count++;
		list_del(&work->list);
	}
	spin_unlock_irqrestore(&service->work_lock, flags);

	/* complain here to enlarge it */
	if (unlikely(!work))
		dev_err(dev, "not enough idle work\n");

	return work;
}

/*
 * downref work, put back to idle list if ref is 0
 */
static void virtio_service_downref_work(struct virtio_service *service,
					struct virtio_service_work *work)
{
	unsigned long flags;

	spin_lock_irqsave(&service->work_lock, flags);
	work->ref_count--;
	if (!work->ref_count) {
		reinit_completion(&work->done);
		list_add_tail(&work->list, &service->idle_works);
	}
	spin_unlock_irqrestore(&service->work_lock, flags);
}

/*
 * enqueue work to pending list and upref it
 */
static void virtio_service_q_work(struct virtio_service *service,
				  struct virtio_service_work *work)
{
	unsigned long flags;

	spin_lock_irqsave(&service->work_lock, flags);
	work->ref_count++;
	list_add_tail(&work->list, &service->pending_works);
	spin_unlock_irqrestore(&service->work_lock, flags);
}

/*
 * dequeue work from pending list, don't change ref
 */
static struct virtio_service_work *
virtio_service_dq_work(struct virtio_service *service)
{
	struct virtio_service_work *work;
	unsigned long flags;

	spin_lock_irqsave(&service->work_lock, flags);
	work = list_first_entry_or_null(&service->pending_works,
					struct virtio_service_work, list);
	if (work)
		list_del(&work->list);
	spin_unlock_irqrestore(&service->work_lock, flags);

	return work;
}

static struct virtio_frontend *
virtio_service_find_frontend(struct virtio_service *service, u32 vid)
{
	struct virtio_frontend *frontend = NULL, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&service->dev_lock, flags);
	list_for_each_entry(tmp, &service->dev_list, list) {
		if (tmp->vid == vid) {
			frontend = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&service->dev_lock, flags);

	return frontend;
}

static int __send_update_common(struct virtio_service *service, u32 vid,
				int wait, int cfg, u8 offset, u8 length)
{
	struct virtio_service_work *work;
	struct virtio_service_message _msg, *msg;
	unsigned long timeout;
	int err = 0;

	/* if we're service thread, just send it */
	if (current == service->worker_thread) {
		msg = &_msg;
		memset(msg, 0, sizeof(*msg));

		mb_msg_init_head(&msg->head, 0 /* rproc is not used */,
				 0 /* use default protocol */,
				 true /* high priority */, sizeof(*msg),
				 0 /* dest is not used */);

		msg->mt = cfg ? MSG_MT_CFG : MSG_MT_REG;
		msg->vid = vid;
		msg->offset = offset;
		msg->length = length;

		return mbox_send_message(service->chan, msg);
	}

	work = virtio_service_get_work(service);
	if (!work)
		return -ENOMEM;

	work->flags = WORK_MSG;
	if (wait)
		work->flags |= WORK_F_NOTIFY;

	msg = &work->msg_param.msg;
	memset(msg, 0, sizeof(*msg));

	mb_msg_init_head(&msg->head, 0 /* rproc is not used */,
			 0 /* use default protocol */, true /* high priority */,
			 sizeof(*msg), 0 /* dest is not used */);
	msg->mt = cfg ? MSG_MT_CFG : MSG_MT_REG;
	msg->vid = vid;
	msg->offset = offset;
	msg->length = length;

	virtio_service_q_work(service, work);
	complete(&service->kick_worker);

	if (wait) {
		timeout = wait_for_completion_timeout(&work->done,
						      msecs_to_jiffies(50));
		if (!timeout)
			err = -ETIME;
	}

	virtio_service_downref_work(service, work);

	return err;
}

static void virtio_service_send_reg_update(struct virtio_service *service,
					   u32 vid, u8 offset, u8 wait)
{
	struct device *dev = service->dev;
	int err;

	err = __send_update_common(service, vid, wait, 0, offset, 0);
	if (err < 0) {
		dev_warn(dev, "fail to send reg %x (%d)\n", offset, err);
		/* TODO statistics */
	}
}

static void virtio_service_send_cfg_update(struct virtio_service *service,
					   u32 vid, u8 offset, u8 length)
{
	struct device *dev = service->dev;
	int err;

	err = __send_update_common(service, vid, 0, 1, offset, length);
	if (err < 0) {
		dev_warn(dev, "fail to send cfg %x %x (%d)\n", offset, length,
			 err);
		/* TODO statistics */
	}
}

static void virtio_frontend_get(struct virtio_device *vdev, unsigned int offset,
				void *buf, unsigned int len)
{
	struct virtio_frontend *frontend = to_frontend(vdev);
	void __iomem *base = frontend->reg + VIRTIO_MMIO_CONFIG;
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

static void virtio_frontend_set(struct virtio_device *vdev, unsigned int offset,
				const void *buf, unsigned int len)
{
	struct virtio_frontend *frontend = to_frontend(vdev);
	void __iomem *base = frontend->reg + VIRTIO_MMIO_CONFIG;
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

	virtio_service_send_cfg_update(frontend->service, frontend->vid, offset,
				       len);
}

static u32 virtio_frontend_generation(struct virtio_device *vdev)
{
	struct virtio_frontend *frontend = to_frontend(vdev);

	return readl(frontend->reg + VIRTIO_MMIO_CONFIG_GENERATION);
}

static u8 virtio_frontend_get_status(struct virtio_device *vdev)
{
	struct virtio_frontend *frontend = to_frontend(vdev);

	return readl(frontend->reg + VIRTIO_MMIO_STATUS) & 0xff;
}

static void virtio_frontend_set_status(struct virtio_device *vdev, u8 status)
{
	struct virtio_frontend *frontend = to_frontend(vdev);

	BUG_ON(status == 0);

	/* TODO some status needs device to confirm: VIRTIO_CONFIG_S_FEATURES_OK */
	writel(status, frontend->reg + VIRTIO_MMIO_STATUS);
	virtio_service_send_reg_update(frontend->service, frontend->vid,
				       VIRTIO_MMIO_STATUS, 0);
}

static void virtio_frontend_reset(struct virtio_device *vdev)
{
	struct virtio_frontend *frontend = to_frontend(vdev);

	writel(0, frontend->reg + VIRTIO_MMIO_STATUS);
	/* wait device reset done */
	virtio_service_send_reg_update(frontend->service, frontend->vid,
				       VIRTIO_MMIO_STATUS, 1);
}

static bool virtio_frontend_notify(struct virtqueue *vq)
{
	struct virtio_frontend *frontend = to_frontend(vq->vdev);

	writel(vq->index, frontend->reg + VIRTIO_MMIO_QUEUE_NOTIFY);
	virtio_service_send_reg_update(frontend->service, frontend->vid,
				       VIRTIO_MMIO_QUEUE_NOTIFY, 0);

	return true;
}

static struct virtqueue *
virtio_frontend_setup_vq(struct virtio_device *vdev, unsigned int index,
			 void (*callback)(struct virtqueue *vq),
			 const char *name, bool ctx)
{
	struct virtio_frontend *frontend = to_frontend(vdev);
	struct virtio_service *service = frontend->service;
	struct device *dev = &vdev->dev;
	struct virtqueue *vq;
	struct vring vring;
	unsigned int num;
	resource_size_t size;
	dma_addr_t dma_addr;
	void *addr;
	phys_addr_t desc, avail, used;

	if (index >= VRING_MAX_NUM) {
		dev_err(dev, "dev%u vq%u but max %u\n", frontend->vid, index,
			VRING_MAX_NUM);
		return ERR_PTR(-EINVAL);
	}

	if (!name) {
		dev_err(dev, "dev%u vq%u invalid name\n", frontend->vid, index);
		return NULL;
	}

	writel(index, frontend->reg + VIRTIO_MMIO_QUEUE_SEL);
	virtio_service_send_reg_update(service, frontend->vid,
				       VIRTIO_MMIO_QUEUE_SEL, 1);

	if (readl(frontend->reg + VIRTIO_MMIO_QUEUE_READY)) {
		dev_err(dev, "dev%u vq%u already been set up\n", frontend->vid,
			index);
		return ERR_PTR(-ENOENT);
	}

	num = readl(frontend->reg + VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (!num) {
		dev_err(dev, "dev%u vq%u no capacity\n", frontend->vid, index);
		return ERR_PTR(-ENOENT);
	}

	size = ALIGN(vring_size(num, VRING_ALIGN), VRING_ALIGN);
	addr = gen_pool_dma_alloc(service->mem_pool, size, &dma_addr);
	if (!addr) {
		dev_err(dev, "dev%u vq%u fail to alloc vq\n", frontend->vid,
			index);
		return ERR_PTR(-ENOMEM);
	}

	/* make sure virtqueue is clear */
	memset_io(addr, 0, size);

	/* strong barrier */
	vq = vring_new_virtqueue(index, num, VRING_ALIGN, vdev, false, ctx,
				 addr, virtio_frontend_notify, callback, name);
	if (!vq) {
		dev_err(dev, "dev%u vq%u fail to create vq\n", frontend->vid,
			index);
		return ERR_PTR(-ENOMEM);
	}

	frontend->vqs[index] = vq;
	frontend->vq_addrs[index] = addr;
	frontend->vq_sizes[index] = size;

	vring_init(&vring, num, (void *)dma_addr, VRING_ALIGN);

	writel(virtqueue_get_vring_size(vq),
	       frontend->reg + VIRTIO_MMIO_QUEUE_NUM);

	desc = (phys_addr_t)vring.desc;
	writel((u32)desc, frontend->reg + VIRTIO_MMIO_QUEUE_DESC_LOW);
	writel((u32)(desc >> 32), frontend->reg + VIRTIO_MMIO_QUEUE_DESC_HIGH);

	avail = (phys_addr_t)vring.avail;
	writel((u32)avail, frontend->reg + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
	writel((u32)(avail >> 32),
	       frontend->reg + VIRTIO_MMIO_QUEUE_AVAIL_HIGH);

	used = (phys_addr_t)vring.used;
	writel((u32)used, frontend->reg + VIRTIO_MMIO_QUEUE_USED_LOW);
	writel((u32)(used >> 32), frontend->reg + VIRTIO_MMIO_QUEUE_USED_HIGH);

	writel(1, frontend->reg + VIRTIO_MMIO_QUEUE_READY);
	virtio_service_send_reg_update(service, frontend->vid,
				       VIRTIO_MMIO_QUEUE_READY, 1);

	dev_info(dev, "dev%u vq%u num %u size %llx vq %llx %llx %llx\n",
		 frontend->vid, index, num, size, desc, avail, used);

	return vq;
}

static void virtio_frontend_del_vq(struct virtqueue *vq)
{
	struct virtio_frontend *frontend = to_frontend(vq->vdev);
	struct virtio_service *service = frontend->service;
	unsigned int index;

	writel(vq->index, frontend->reg + VIRTIO_MMIO_QUEUE_SEL);
	writel(0, frontend->reg + VIRTIO_MMIO_QUEUE_READY);
	virtio_service_send_reg_update(service, frontend->vid,
				       VIRTIO_MMIO_QUEUE_READY, 1);

	index = vq->index;
	vring_del_virtqueue(vq);

	gen_pool_free(service->mem_pool, frontend->vq_addrs[index],
		      frontend->vq_sizes[index]);
	frontend->vqs[index] = NULL;
	frontend->vq_addrs[index] = NULL;
	frontend->vq_sizes[index] = 0;
}

static void virtio_frontend_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;

	list_for_each_entry_safe(vq, n, &vdev->vqs, list)
		virtio_frontend_del_vq(vq);
}

static int virtio_frontend_find_vqs(struct virtio_device *vdev,
				    unsigned int nvqs, struct virtqueue *vqs[],
				    vq_callback_t *callbacks[],
				    const char *const names[], const bool *ctx,
				    struct irq_affinity *desc)
{
	int i;

	for (i = 0; i < nvqs; i++) {
		vqs[i] = virtio_frontend_setup_vq(
			vdev, i, callbacks[i], names[i], ctx ? ctx[i] : false);
		if (IS_ERR(vqs[i])) {
			virtio_frontend_del_vqs(vdev);
			return PTR_ERR(vqs[i]);
		}
	}

	return 0;
}

static u64 virtio_frontend_get_features(struct virtio_device *vdev)
{
	struct virtio_frontend *frontend = to_frontend(vdev);
	u64 features;

	writel(1, frontend->reg + VIRTIO_MMIO_DEVICE_FEATURES_SEL);
	virtio_service_send_reg_update(frontend->service, frontend->vid,
				       VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
	features = readl(frontend->reg + VIRTIO_MMIO_DEVICE_FEATURES);
	features <<= 32;

	writel(0, frontend->reg + VIRTIO_MMIO_DEVICE_FEATURES_SEL);
	virtio_service_send_reg_update(frontend->service, frontend->vid,
				       VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
	features |= readl(frontend->reg + VIRTIO_MMIO_DEVICE_FEATURES);

	return features;
}

static int virtio_frontend_finalize_features(struct virtio_device *vdev)
{
	struct virtio_frontend *frontend = to_frontend(vdev);

	writel(1, frontend->reg + VIRTIO_MMIO_DRIVER_FEATURES_SEL);
	writel((u32)(vdev->features >> 32),
	       frontend->reg + VIRTIO_MMIO_DRIVER_FEATURES);
	virtio_service_send_reg_update(frontend->service, frontend->vid,
				       VIRTIO_MMIO_DRIVER_FEATURES, 1);

	writel(0, frontend->reg + VIRTIO_MMIO_DRIVER_FEATURES_SEL);
	writel((u32)vdev->features,
	       frontend->reg + VIRTIO_MMIO_DRIVER_FEATURES);
	virtio_service_send_reg_update(frontend->service, frontend->vid,
				       VIRTIO_MMIO_DRIVER_FEATURES, 1);

	return 0;
}

static const char *virtio_frontend_bus_name(struct virtio_device *vdev)
{
	struct virtio_frontend *frontend = to_frontend(vdev);
	struct virtio_service *service = frontend->service;
	struct platform_device *pdev = to_platform_device(service->dev);

	return pdev->name;
}

/* this handler is provided so driver core doesn't yell at us */
static void virtio_frontend_device_release(struct device *dev)
{
	struct virtio_device *vdev =
		container_of(dev, struct virtio_device, dev);
	struct device *parent = dev->parent;

	dev_info(parent, "virtio device %u released\n", vdev->id.device);
}

static struct virtio_config_ops virtio_frontend_config_ops = {
	.get = virtio_frontend_get,
	.set = virtio_frontend_set,
	.generation = virtio_frontend_generation,
	.get_status = virtio_frontend_get_status,
	.set_status = virtio_frontend_set_status,
	.reset = virtio_frontend_reset,
	.find_vqs = virtio_frontend_find_vqs,
	.del_vqs = virtio_frontend_del_vqs,
	.get_features = virtio_frontend_get_features,
	.finalize_features = virtio_frontend_finalize_features,
	.bus_name = virtio_frontend_bus_name,
};

static struct virtio_frontend *
virtio_service_detect_frontend(struct virtio_service *service, u32 vid,
			       phys_addr_t addr)
{
	struct device *dev = service->dev;
	struct virtio_frontend *frontend;
	u32 magic, version, id;
	unsigned long flags;
	void __iomem *reg;
	int i, len;

	if (addr >= service->mem_base + service->mem_size ||
	    addr < service->mem_base) {
		dev_err(dev, "reg addr %x out of range\n", addr);
		return NULL;
	}

	reg = service->base + (addr - service->mem_base);

	magic = readl(reg + VIRTIO_MMIO_MAGIC_VALUE);
	if (magic != ('v' | 'i' << 8 | 'r' << 16 | 't' << 24)) {
		dev_err(dev, "invalid magic 0x%08x\n", magic);
		return NULL;
	}

	version = readl(reg + VIRTIO_MMIO_VERSION);
	if (version != 2) {
		dev_err(dev, "unexpected version %u\n", version);
		return NULL;
	}

	id = readl(reg + VIRTIO_MMIO_DEVICE_ID);
	if (!id) {
		dev_err(dev, "invalid device id %u\n", id);
		return NULL;
	}

	frontend =
		devm_kzalloc(dev, sizeof(struct virtio_frontend), GFP_KERNEL);
	if (!frontend) {
		dev_err(dev, "fail to alloc frontend\n");
		return NULL;
	}

	len = snprintf(frontend->name, FRONTEND_NAME_MAX_LEN, "%08x.frontend",
		       addr);
	len = min(len, FRONTEND_NAME_MAX_LEN - 1);
	frontend->name[len] = '\0';

	frontend->vdev.id.device = id;
	frontend->vdev.id.vendor = readl(reg + VIRTIO_MMIO_VENDOR_ID);
	frontend->vdev.config = &virtio_frontend_config_ops;
	frontend->vdev.dev.init_name = frontend->name;
	frontend->vdev.dev.parent = dev;
	frontend->vdev.dev.release = virtio_frontend_device_release;
	frontend->service = service;
	frontend->vid = vid;
	frontend->reg_phys = addr;
	frontend->reg = reg;
	INIT_LIST_HEAD(&frontend->list);
	for (i = 0; i < VRING_MAX_NUM; i++)
		frontend->vqs[i] = NULL;

	spin_lock_irqsave(&service->dev_lock, flags);
	list_add_tail(&frontend->list, &service->dev_list);
	spin_unlock_irqrestore(&service->dev_lock, flags);

	dev_info(dev, "detected device %u id %u at %x(%p)\n", vid, id, addr,
		 reg);
	return frontend;
}

static void virtio_service_flush_works(struct virtio_service *service)
{
	struct device *dev = service->dev;
	struct virtio_service_work *work;

	while ((work = virtio_service_dq_work(service))) {
		dev_warn(dev, "discard work %x ref %u\n", work->flags,
			 work->ref_count);

		if (work->flags & WORK_F_NOTIFY)
			complete(&work->done);

		virtio_service_downref_work(service, work);
	}
}

static void virtio_service_do_work(struct virtio_service *service,
				   struct virtio_service_work *work)
{
	struct device *dev = service->dev;
	struct virtio_service_message *msg;
	struct virtio_frontend *frontend;
	struct list_head list;
	unsigned long flags;
	int err;

	switch (work->flags & WORK_MASK) {
	case WORK_MSG: {
		msg = &work->msg_param.msg;
		err = mbox_send_message(service->chan, msg);
		if (err < 0) {
			dev_warn(dev, "fail to send msg %x (%d)\n",
				 *((u32 *)(((void *)msg) + sizeof(msg->head))),
				 err);
			/* TODO statistics */
		}
	} break;
	case WORK_BOOT: {
		if (service->boot) {
			/* already boot, peer may reset, unregister all */
			dev_warn(dev, "detect peer reset\n");

			INIT_LIST_HEAD(&list);
			spin_lock_irqsave(&service->dev_lock, flags);
			list_replace_init(&service->dev_list, &list);
			spin_unlock_irqrestore(&service->dev_lock, flags);

			while ((frontend = list_first_entry_or_null(
					&list, struct virtio_frontend, list))) {
				dev_info(
					dev,
					"releasing dev %u id %u at %x(%p)...\n",
					frontend->vid, frontend->vdev.id.device,
					frontend->reg_phys, frontend->reg);
				unregister_virtio_device(&frontend->vdev);
				list_del(&frontend->list);
				devm_kfree(dev, frontend);
			}

			/* all devs removed, clear pending works */
			virtio_service_flush_works(service);
		}
		service->boot = 1;

		frontend = virtio_service_detect_frontend(service, 0,
							  service->mem_base);
		if (!frontend) {
			dev_err(dev, "fail to detect provider\n");
			break;
		}

		err = register_virtio_device(&frontend->vdev);
		if (err < 0) {
			dev_err(dev, "fail to register provider (%d)\n", err);
			break;
		}

		service->provider = &frontend->vdev;
	} break;
	case WORK_DEV: {
		if (!work->dev_param.vid) {
			dev_err(dev, "invalid vid 0\n");
			break;
		}

		frontend = virtio_service_find_frontend(service,
							work->dev_param.vid);
		if (frontend)
			break;

		frontend = virtio_service_detect_frontend(
			service, work->dev_param.vid, work->dev_param.addr);
		if (!frontend) {
			dev_err(dev, "fail to detect %u at %x\n",
				work->dev_param.vid, work->dev_param.addr);
			break;
		}

		err = register_virtio_device(&frontend->vdev);
		if (err < 0) {
			dev_err(dev, "fail to register vdev %u (%d)\n",
				work->dev_param.vid, err);
			break;
		}

		dev_info(dev, "add vdev %u at %x\n", work->dev_param.vid,
			 work->dev_param.addr);
	} break;
	default: {
		dev_warn(dev, "unknown work flag %lx\n", work->flags);
	} break;
	}
}

static int virtio_service_thread(void *data)
{
	struct virtio_service *service = data;
	struct device *dev = service->dev;
	struct virtio_service_work *work;

	for (;;) {
		/* always wake up every 1 second */
		wait_for_completion_timeout(&service->kick_worker,
					    msecs_to_jiffies(1000));

		if (service->should_exit)
			break;

		while ((work = virtio_service_dq_work(service))) {
			virtio_service_do_work(service, work);

			if (work->flags & WORK_F_NOTIFY)
				complete(&work->done);

			virtio_service_downref_work(service, work);
		}
	}

	dev_warn(dev, "virtio service thread exited\n");
	complete(&service->worker_exited);

	return 0;
}

static void virtio_service_mbox_callback(struct mbox_client *client, void *mssg)
{
	struct virtio_service *service =
		container_of(client, struct virtio_service, client);
	struct virtio_service_message *msg = mssg;
	struct virtio_service_work *work;
	struct virtio_frontend *frontend;
	u32 status;
	int i;

	if (unlikely(!service->ready))
		return;

	if (unlikely(msg->mt == MSG_MT_BOOT)) {
		work = virtio_service_get_work(service);
		if (!work)
			return;

		work->flags = WORK_BOOT;
		virtio_service_q_work(service, work);
		virtio_service_downref_work(service, work);
		complete(&service->kick_worker);
	} else if (msg->mt == MSG_MT_IRQ) {
		frontend = virtio_service_find_frontend(service, msg->vid);
		if (unlikely(!frontend))
			return;

		status = readl(frontend->reg + VIRTIO_MMIO_INTERRUPT_STATUS);
		writel(status, frontend->reg + VIRTIO_MMIO_INTERRUPT_ACK);
		/* TODO send reg notify */

		if (unlikely(status & VIRTIO_MMIO_INT_CONFIG))
			virtio_config_changed(&frontend->vdev);

		if (likely(status & VIRTIO_MMIO_INT_VRING)) {
			for (i = 0; i < VRING_MAX_NUM; i++)
				vring_interrupt(0, frontend->vqs[i]);
		}
	} else {
		/* as a frontend, we can ignore all other msg */
	}
}

static ssize_t dump_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t len)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct virtio_service *service = platform_get_drvdata(pdev);
	struct virtio_frontend *frontend;
	const struct vring *ring;
	struct virtqueue *vq;
	unsigned long flags;
	int i;

	dev_info(dev, "service mem %p(%x) size %x\n", service->base,
		 service->mem_base, service->mem_size);
	spin_lock_irqsave(&service->dev_lock, flags);
	list_for_each_entry(frontend, &service->dev_list, list) {
		dev_info(dev, "dev[%u] reg 0x%x(%p) vqs:\n", frontend->vid,
			 frontend->reg_phys, frontend->reg);
		for (i = 0; i < VRING_MAX_NUM; i++) {
			vq = frontend->vqs[i];
			if (!vq) {
				dev_info(dev, "  vq%d not ready\n");
				continue;
			}

			ring = virtqueue_get_vring(vq);
			dev_info(dev,
				 "  vq%d %s free %u size %u ring %p %p %p\n", i,
				 vq->name, vq->num_free,
				 virtqueue_get_vring_size(vq), ring->desc,
				 ring->avail, ring->used);
		}
	}
	spin_unlock_irqrestore(&service->dev_lock, flags);

	return len;
}

static ssize_t text_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t len)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct virtio_service *service = platform_get_drvdata(pdev);
	size_t size;

	if (!service->provider) {
		dev_err(dev, "provider not ready\n");
		return len;
	}

	size = virtio_provider_send_text(service->provider, buf, len);
	if (!size) {
		dev_err(dev, "fail to send text\n");
		return len;
	}

	dev_info(dev, "send %zu bytes\n", size);
	return len;
}

static ssize_t latency_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct virtio_service *service = platform_get_drvdata(pdev);
	unsigned int count;

	if (kstrtouint(buf, 10, &count)) {
		dev_err(dev, "invalid count %s\n", buf);
		return len;
	}

	if (!service->provider) {
		dev_err(dev, "provider not ready\n");
		return len;
	}

	virtio_provider_latency_test(service->provider, count);

	return len;
}

static DEVICE_ATTR_WO(dump);
static DEVICE_ATTR_WO(text);
static DEVICE_ATTR_WO(latency);

static struct attribute *virtio_service_attributes[] = {
	&dev_attr_dump.attr, &dev_attr_text.attr, &dev_attr_latency.attr, NULL
};

static const struct attribute_group virtio_service_attr_group = {
	.name = "virtio-debug",
	.attrs = virtio_service_attributes,
};

size_t virtio_service_get_provider_memory(struct device *dev, void **va,
					  phys_addr_t *pa)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct virtio_service *service = platform_get_drvdata(pdev);

	if (va)
		*va = service->base + VIRTIO_MMIO_TOTAL_SIZE;

	if (pa)
		*pa = service->mem_base + VIRTIO_MMIO_TOTAL_SIZE;

	return VIRTIO_PROVIDER_BUFSIZE;
}

void virtio_service_on_device_change(struct device *dev, u32 vid, u32 addr)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct virtio_service *service = platform_get_drvdata(pdev);
	struct virtio_service_work *work;

	work = virtio_service_get_work(service);
	if (unlikely(!work)) {
		dev_err(dev, "fail to handle dev %u %u change\n", vid, addr);
		return;
	}

	work->flags = WORK_DEV;
	work->dev_param.vid = vid;
	work->dev_param.addr = addr;

	virtio_service_q_work(service, work);
	virtio_service_downref_work(service, work);

	complete(&service->kick_worker);
}

static int virtio_service_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct virtio_service *service;
	struct virtio_service_work *work;
	struct virtio_service_message *msg;
	struct resource *res;
	int err, i;

	service = devm_kzalloc(dev, sizeof(*service), GFP_KERNEL);
	if (!service) {
		dev_err(dev, "fail to alloc service\n");
		return -ENOMEM;
	}

	service->dev = dev;
	platform_set_drvdata(pdev, service);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "fail to parse service memory\n");
		return -ENOMEM;
	}

	service->mem_base = res->start;
	service->mem_size = resource_size(res);
	dev_info(dev, "total memory 0x%zx bytes from 0x%zx\n",
		 service->mem_size, service->mem_base);

	if (service->mem_size < VIRTIO_TOTAL_SIZE) {
		dev_err(dev, "need 0x%zx bytes but only have 0x%zx\n",
			VIRTIO_TOTAL_SIZE, service->mem_size);
		return -ENOMEM;
	}

	if (!devm_request_mem_region(dev, service->mem_base, service->mem_size,
				     pdev->name)) {
		dev_err(dev, "fail to request service memory\n");
		return -EBUSY;
	}

	service->base = devm_ioremap(dev, service->mem_base, service->mem_size);
	if (!service->base) {
		dev_err(dev, "fail to remap service memory\n");
		return -ENOMEM;
	}

	service->mem_pool = devm_gen_pool_create(dev, VRING_ALIGN_ORDER, -1,
						 "virtio-service-pool");
	if (IS_ERR(service->mem_pool)) {
		err = PTR_ERR(service->mem_pool);
		dev_err(dev, "fail to create memory pool (%d)\n", err);
		return err;
	}

	err = gen_pool_add_virt(service->mem_pool,
				(unsigned long)(service->base +
						VIRTIO_MMIO_TOTAL_SIZE +
						VIRTIO_PROVIDER_BUFSIZE),
				service->mem_base + VIRTIO_MMIO_TOTAL_SIZE +
					VIRTIO_PROVIDER_BUFSIZE,
				VIRTIO_VRING_TOTAL_SIZE, -1);
	if (err < 0) {
		dev_err(dev, "fail to add %zx bytes to memory pool (%d)\n",
			VIRTIO_VRING_TOTAL_SIZE, err);
		return err;
	}

	service->client.dev = dev;
	service->client.tx_block = true;
	service->client.tx_tout = 1000;
	service->client.rx_callback = virtio_service_mbox_callback;

	service->chan = mbox_request_channel(&service->client, 0);
	if (IS_ERR(service->chan)) {
		err = PTR_ERR(service->chan);
		dev_err(dev, "fail to request mbox channel (%d)\n", err);
		return err;
	}

	spin_lock_init(&service->work_lock);
	INIT_LIST_HEAD(&service->pending_works);
	INIT_LIST_HEAD(&service->idle_works);
	for (i = 0; i < WORK_COUNT; i++) {
		work = &service->__works[i];
		init_completion(&work->done);
		work->ref_count = 0;
		INIT_LIST_HEAD(&work->list);
		list_add_tail(&work->list, &service->idle_works);
	}

	init_completion(&service->kick_worker);
	init_completion(&service->worker_exited);
	service->should_exit = 0;
	service->worker_thread = kthread_run(virtio_service_thread, service,
					     "virtio-service-thread");
	if (IS_ERR(service->worker_thread)) {
		err = PTR_ERR(service->worker_thread);
		dev_err(dev, "fail to create service thread (%d)\n", err);
		goto err_task;
	}

	spin_lock_init(&service->dev_lock);
	INIT_LIST_HEAD(&service->dev_list);

	err = sysfs_create_group(&dev->kobj, &virtio_service_attr_group);
	if (err < 0) {
		dev_err(dev, "fail to create debug interface (%d)\n", err);
		goto err_sysfs;
	}

	/* we can receive mbox msg from here */
	service->ready = 1;

	/* trigger init process */
	work = virtio_service_get_work(service);
	work->flags = WORK_MSG;

	msg = &work->msg_param.msg;
	memset(msg, 0, sizeof(*msg));

	mb_msg_init_head(&msg->head, 0 /* rproc is not used */,
			 0 /* use default protocol */, true /* high priority */,
			 sizeof(*msg), 0 /* dest is not used */);
	msg->mt = MSG_MT_BOOT;

	virtio_service_q_work(service, work);
	virtio_service_downref_work(service, work);
	complete(&service->kick_worker);

	return 0;

err_sysfs:
	service->should_exit = 1;
	complete(&service->kick_worker);
	wait_for_completion(&service->worker_exited);

err_task:
	mbox_free_channel(service->chan);

	return err;
}

static int virtio_service_remove(struct platform_device *pdev)
{
	struct virtio_service *service = platform_get_drvdata(pdev);
	struct device *dev = service->dev;
	unsigned long timeout;

	sysfs_remove_group(&dev->kobj, &virtio_service_attr_group);
	service->should_exit = 1;
	complete(&service->kick_worker);
	timeout = wait_for_completion_timeout(&service->worker_exited,
					      msecs_to_jiffies(5000));
	if (!timeout)
		dev_warn(dev, "timeout waiting for service thread\n");
	mbox_free_channel(service->chan);

	return 0;
}

static const struct of_device_id virtio_service_match_table[] = {
	{
		.compatible = "sd,virtio-service",
	},
	{ /* sentinel */ }
};

static struct platform_driver virtio_service_driver = {
	.driver = {
		   .name = "virtio-service",
		   .owner = THIS_MODULE,
		   .of_match_table = virtio_service_match_table,
	},
	.probe = virtio_service_probe,
	.remove = virtio_service_remove,
};

module_platform_driver(virtio_service_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Semidrive virtio service");
MODULE_AUTHOR("Semidrive Semiconductor");
