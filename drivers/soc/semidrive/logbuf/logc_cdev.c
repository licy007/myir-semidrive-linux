#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/logbuf.h>

#define SDRV_LOGC_IOC_GET_LOGSIZE               _IOWR('K', 3, __u32)
#define SDRV_LOGC_IOC_GET_LASTLOG               _IOWR('K', 4, __u32)
#define SDRV_LOGC_IOC_SET_LOGCLR		_IOW('K', 5, __u32)

#define SDRV_LOGC_CHAR_DEV_OPEN			(1)
#define SDRV_LOGC_CHAR_DEV			"sdrv-logc"

typedef struct logc_cdev {
	struct miscdevice mcdev;
	unsigned long state;
	int size[LOG_TYPE_MAX];
	char *buf[LOG_TYPE_MAX];
} logc_cdev_t;

static logc_cdev_t *cdev;

static int sdrv_logc_cdev_open(struct inode *inode, struct file *file)
{
	/* the cdev is single open! */
	if (test_and_set_bit(SDRV_LOGC_CHAR_DEV_OPEN, &cdev->state))
		return -EBUSY;

	return 0;
}

static long sdrv_logc_cdev_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	logbuf_bk_desc_t *desc;
	sdlog_msg_t msg;
	int act_sz;
	int ret = 0;

	if (!uarg) {
		pr_err("invalid user space pointer\n");
		return -EINVAL;
	}

	if (copy_from_user(&msg, uarg, sizeof(msg)))
		return -EFAULT;

	if (msg.type >= LOG_TYPE_MAX)
		return -EINVAL;

	desc = sdrv_logbuf_get_bank_desc(msg.type);
	if (!desc || !desc->hdr)
		return -EINVAL;

	if (!cdev->buf[msg.type]) {
		cdev->size[msg.type] = sdrv_logbuf_get_bank_buf_size(desc);
		cdev->buf[msg.type] = vzalloc(cdev->size[msg.type]);
		if (!cdev->buf[msg.type]) {
			pr_err("failed to alloc buf (sz: 0x%x, type: %d)\n",
				cdev->size[msg.type], msg.type);
			return -ENOMEM;
		}
	}

	switch (cmd) {
	case SDRV_LOGC_IOC_SET_LOGCLR:
		sdrv_logbuf_clear_buf(desc);
		break;

	case SDRV_LOGC_IOC_GET_LASTLOG:
		act_sz = sdrv_logbuf_get_last_buf_and_size(desc,
							   cdev->buf[msg.type],
							   cdev->size[msg.type],
							   false);
		if (act_sz < 0)
			return -EINVAL;

		ret = sdrv_logbuf_copy_to_user(&msg, uarg, cdev->buf[msg.type],
					       act_sz);
		if (ret)
			return ret;

		break;

	case SDRV_LOGC_IOC_GET_LOGSIZE:
		msg.size = sdrv_logbuf_get_bank_buf_size(desc);
		if (copy_to_user((void *)uarg, &msg, sizeof(msg)))
			return -EFAULT;

		break;

	default:
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static int sdrv_logc_cdev_release(struct inode *imode, struct file *file)
{
	/* make sure that char device can be re-opened */
	clear_bit(SDRV_LOGC_CHAR_DEV_OPEN, &cdev->state);

	return 0;
}

static const struct file_operations sdrv_logc_cdev_fops = {
	.owner		= THIS_MODULE,
	.open		= sdrv_logc_cdev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= sdrv_logc_cdev_ioctl,
#endif
	.unlocked_ioctl = sdrv_logc_cdev_ioctl,
	.release	= sdrv_logc_cdev_release,
};

static void sdrv_logc_cdev_free(void)
{
	int i;

	for (i = 0; i < LOG_TYPE_MAX; i++)
		vfree(cdev->buf[i]);

	kfree(cdev);
}

static int __init sdrv_logc_cdev_init(void)
{
	if (!sdrv_logbuf_has_initialized()) {
		pr_err("log buffer is not initialized\n");
		return -EINVAL;
	}
	cdev = kzalloc(sizeof(logc_cdev_t), GFP_KERNEL);
	if (!cdev)
		return -ENOMEM;

	cdev->mcdev.minor = MISC_DYNAMIC_MINOR;
	cdev->mcdev.name = SDRV_LOGC_CHAR_DEV;
	cdev->mcdev.fops  = &sdrv_logc_cdev_fops;

	if (misc_register(&cdev->mcdev)) {
		pr_err("failed to register misc device\n");
		return -EINVAL;
	}

	return 0;
}

static void __exit sdrv_logc_cdev_exit(void)
{
	misc_deregister(&cdev->mcdev);

	sdrv_logc_cdev_free();
}

module_init(sdrv_logc_cdev_init);
module_exit(sdrv_logc_cdev_exit);

MODULE_DESCRIPTION("semidrive logc cdev driver");
MODULE_AUTHOR("Xingyu Chen <xingyu.chen@semidrive.com>");
MODULE_LICENSE("GPL v2");
