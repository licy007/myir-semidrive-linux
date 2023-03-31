#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

MODULE_LICENSE("Dual BSD/GPL");

static int memdisk_major = 0;
module_param(memdisk_major, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512
#define MEMDISK_MINORS	16

/*
 * The internal representation of our device.
 */
struct memdisk_dev {
	u64 size;                      	/* Device size in sectors */
	spinlock_t lock;                /* For mutual exclusion */
	u8 *data;                       /* The data array */
	struct request_queue *queue;    /* The device request queue */
	struct gendisk *gd;             /* The gendisk structure */
};

struct memdisk_dev *memdisk_pdev = NULL;
/*
 * Handle an I/O request.
 */
static void memdisk_transfer(struct memdisk_dev *dev, sector_t sector,
		unsigned long nsect, char *buffer, int write)
{
	u64 offset = sector * hardsect_size;
	u64 nbytes = nsect * hardsect_size;

	//printk("%s: %s offset:%x, %d bytes. \n", __FUNCTION__, write ? "write" : "read", offset, nbytes);
	if ((offset + nbytes) > (dev->size * hardsect_size)) {
		pr_err("%s: Beyond-end write (%llu %llu)\n", __FUNCTION__, offset, nbytes);
		return;
	}

	if (write)
		memcpy(dev->data + offset, buffer, nbytes);
	else
		memcpy(buffer, dev->data + offset, nbytes);
}

static void memdisk_request(struct request_queue *q) {
	struct request *req;
	struct memdisk_dev *dev = q->queuedata;

	req = blk_fetch_request(q);
	while (req != NULL) {
		memdisk_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req),
				bio_data(req->bio), rq_data_dir(req));
		if ( ! __blk_end_request_cur(req, 0) ) {
			req = blk_fetch_request(q);
		}
	}
}

int memdisk_getgeo(struct block_device * bd, struct hd_geometry * geo)
{
	long size;

	struct memdisk_dev *dev = bd->bd_disk->private_data;
	size = dev->size *(hardsect_size / KERNEL_SECTOR_SIZE);

	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 4;

	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations memdisk_ops = {
	.owner  = THIS_MODULE,
	.getgeo = memdisk_getgeo
};

#if IS_ENABLED(CONFIG_OF)
static int memdisk_parse_dt(void)
{
	struct device_node *np, *res_np;
	struct resource res;
	int ret = 0;

	np = of_find_compatible_node(NULL, NULL, "semidrive,memdisk");
	if (!np) {
		pr_err("memdisk: no memdisk node in dts\n");
		return -ENODEV;
	}

	if (!of_device_is_available(np)) {
		pr_err("memdisk: memdisk node is disabled in dts\n");
		of_node_put(np);
		return -ENODEV;;
	}

	res_np = of_parse_phandle(np, "memory-region", 0);
	if (!res_np) {
		pr_err("memdisk: no memory reserved for memdisk\n");
		of_node_put(np);
		return -ENODEV;
	}

	if (!of_device_is_available(res_np)) {
		pr_err("memdisk: reserved memory region is disabled in dts\n");
                ret = -ENODEV;
		goto err;
        }

	ret = of_address_to_resource(res_np, 0, &res);
	if (ret < 0) {
		pr_err("memdisk: no reg in reserved memory\n");
		goto err;
	}

	if (!of_find_property(res_np, "no-map", NULL)) {
		pr_err("memdisk: Can't apply mapped reserved-memory\n");
		goto err;
	}

	pr_info("%s: reserved memdisk memory : 0x%llx - 0x%llx\n", __FUNCTION__, res.start, res.end);
	memdisk_pdev =(struct memdisk_dev *)kmalloc(sizeof(struct memdisk_dev), GFP_KERNEL);
	if (!memdisk_pdev) {
		ret = -ENOMEM;
		goto err;
	}
	memdisk_pdev->data = (u8 *)res.start;
	memdisk_pdev->size = resource_size(&res);
err:
	of_node_put(res_np);
	of_node_put(np);

	return ret;
}
#endif

/*
 * Set up our internal device.
 */
static int memdisk_setup_device(struct memdisk_dev *dev)
{
	unsigned char __iomem  *base;

	spin_lock_init(&dev->lock);
	base = ioremap((phys_addr_t)dev->data, dev->size);
	if(!base) {
		pr_err("memdisk: ioremap error, data: %llx, size: %llx\n",
			(u64)dev->data, dev->size);
		return -EINVAL;
	}

	pr_info("%s base: %llx, start: %llx, size: %llx\n", __FUNCTION__,
		(u64)base, (u64)dev->data, dev->size);
	dev->data = base;
	dev->size = (dev->size / hardsect_size);

	dev->queue = blk_init_queue(memdisk_request, &dev->lock);
	if (dev->queue == NULL) {
		pr_err("memdisk: blk_init_queue failure\n");
		return -EINVAL;
	}

	blk_queue_logical_block_size(dev->queue, hardsect_size);
	dev->queue->queuedata = dev;

	dev->gd = alloc_disk(MEMDISK_MINORS);
	if (! dev->gd) {
		pr_err("memdisk: alloc_disk failure\n");
		return -ENODEV;
	}
	dev->gd->major = memdisk_major;
	dev->gd->first_minor = 0;
	dev->gd->fops = &memdisk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;

	snprintf(dev->gd->disk_name, 32, "memdisk%d", dev->gd->first_minor);
	set_capacity(dev->gd, dev->size*(hardsect_size/KERNEL_SECTOR_SIZE));
	add_disk(dev->gd);

	pr_info("%s: memdisk%d success\n", __FUNCTION__, dev->gd->first_minor);
	return 0;
}

static int __init memdisk_init(void)
{
	int rc = 0;

	pr_info("%s enter\n", __FUNCTION__);
	memdisk_major = register_blkdev(memdisk_major, "memdisk");
	if (memdisk_major <= 0) {
		pr_err("memdisk: unable to get major number\n");
		return -EBUSY;
	}
#if IS_ENABLED(CONFIG_OF)
	if (memdisk_parse_dt()) {
		pr_err("memdisk: cannot find any valid memdisk, skip\n");
		return -ENODEV;
	}

	rc = memdisk_setup_device(memdisk_pdev);
#endif
	pr_info("%s finished\n", __FUNCTION__);
	return 0;
}

static void __exit memdisk_exit(void)
{
	struct memdisk_dev *dev = memdisk_pdev;

	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}
	if (dev->queue) {
		blk_cleanup_queue(dev->queue);
	}
	unregister_blkdev(memdisk_major, "memdisk");
}

module_init(memdisk_init);
module_exit(memdisk_exit);
