#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/logbuf.h>
#include "logflag.h"

#define SDRV_LOGD_IOC_GET_LOGSIZE		_IOWR('K', 50, __u32)
#define SDRV_LOGD_IOC_GET_LASTLOG		_IOWR('K', 51, __u32)
#define SDRV_LOGD_IOC_GET_NEWLOG		_IOWR('K', 52, __u32)
#define SDRV_LOGD_IOC_GET_UPDATE_MASK		_IOR('K', 53, __u32)
#define SDRV_LOGD_IOC_SET_POLL_MASK		_IOW('K', 54, __u32)
#define SDRV_LOGD_IOC_SET_UPDATE_THRESHOLD	_IOW('K', 55, __u32)
#define SDRV_LOGD_IOC_SET_TIMER_PERIOD		_IOW('K', 56, __u32)

#define SDRV_LOGD_CHAR_DEV_OPEN			(1)
#define SDRV_LOGD_CHAR_DEV			"sdrv-logd"

#define SDRV_LOGD_TIMER_PERIOD_DEFAULT		(5000) /* 5s        */
#define SDRV_LOGD_UPD_THRESHOLD_DEFAULT		(500)  /* 500 bytes */
#define SDRV_LOGD_POLL_MASK_DEFAULT		((1 << SAFETY_R5) |	\
						(1 << SECURE_R5) |	\
						(1 << AP2_A55) |	\
						(1 << AP2_USER))

typedef struct logd_cdev {
	int update_threshold;
	int timer_period;
	int log_upd_mask;
	int log_poll_mask;
	unsigned long state;
	wait_queue_head_t waitq;
	struct timer_list timer;
	struct miscdevice mcdev;
	struct regmap *regmap;
	int size[LOG_TYPE_MAX];
	char *buf[LOG_TYPE_MAX];
} logd_cdev_t;

static logd_cdev_t *g_cdev;

static logd_cdev_t *miscdev_to_logd_cdev(struct miscdevice *misc)
{
	return container_of(misc, struct logd_cdev, mcdev);
}

static void sdrv_logd_cdev_update_mask_set(logd_cdev_t *cdev, log_type_t typ)
{
	if (typ == LOG_TYPE_MAX) {
		cdev->log_upd_mask = 0;
		return;
	}

	cdev->log_upd_mask |= (1 << typ);
}

static int sdrv_logd_cdev_update_mask_get(logd_cdev_t *cdev)
{
	return cdev->log_upd_mask;
}

static int sdrv_logd_cdev_open(struct inode *inode, struct file *file)
{
	logd_cdev_t *cdev = miscdev_to_logd_cdev(file->private_data);

	/* the cdev is single open! */
	if (test_and_set_bit(SDRV_LOGD_CHAR_DEV_OPEN, &cdev->state))
		return -EBUSY;

	cdev->update_threshold = SDRV_LOGD_UPD_THRESHOLD_DEFAULT;
	cdev->log_poll_mask = SDRV_LOGD_POLL_MASK_DEFAULT;
	cdev->timer_period = SDRV_LOGD_TIMER_PERIOD_DEFAULT;

	sdrv_exp_rst_cnt_set(cdev->regmap, FLAG_ID_AP1, 0);

	mod_timer(&cdev->timer, jiffies + msecs_to_jiffies(cdev->timer_period));

	return 0;
}

static long sdrv_logd_cdev_buffer_handle(struct file *file, unsigned int cmd,
					 unsigned long arg)
{
	logd_cdev_t *cdev = miscdev_to_logd_cdev(file->private_data);
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
	case SDRV_LOGD_IOC_GET_LASTLOG:
		sdrv_logd_cdev_update_mask_set(cdev, LOG_TYPE_MAX);

		act_sz = sdrv_logbuf_get_last_buf_and_size(desc,
							   cdev->buf[msg.type],
							   cdev->size[msg.type],
							   true);
		if (act_sz < 0)
			return -EINVAL;

		ret = sdrv_logbuf_copy_to_user(&msg, uarg, cdev->buf[msg.type],
					       act_sz);
		if (ret)
			return ret;

		break;

	case SDRV_LOGD_IOC_GET_NEWLOG:
		act_sz = sdrv_logbuf_get_new_buf_and_size(desc,
							  cdev->buf[msg.type],
							  cdev->size[msg.type],
							  true);
		if (act_sz < 0)
			return -EINVAL;

		ret = sdrv_logbuf_copy_to_user(&msg, uarg, cdev->buf[msg.type],
					       act_sz);
		if (ret)
			return ret;

		break;

	case SDRV_LOGD_IOC_GET_LOGSIZE:
		msg.size = sdrv_logbuf_get_bank_buf_size(desc);
		if (copy_to_user((void *)uarg, &msg, sizeof(msg)))
			return -EFAULT;

		break;

	default:
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static long sdrv_logd_cdev_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	int val;
	void __user *uarg = (void __user *)arg;
	logd_cdev_t *cdev = miscdev_to_logd_cdev(file->private_data);

	switch (cmd) {
	case SDRV_LOGD_IOC_GET_UPDATE_MASK:
		val = sdrv_logd_cdev_update_mask_get(cdev);
		if (copy_to_user(uarg, &val, sizeof(val)))
			return -EFAULT;

		sdrv_logd_cdev_update_mask_set(cdev, LOG_TYPE_MAX);
		break;
	case SDRV_LOGD_IOC_SET_POLL_MASK:
		if (copy_from_user(&val, uarg, sizeof(val)))
			return -EFAULT;

		cdev->log_poll_mask = val;
		break;
	case SDRV_LOGD_IOC_SET_UPDATE_THRESHOLD:
		if (copy_from_user(&val, uarg, sizeof(val)))
			return -EFAULT;

		cdev->update_threshold = val;
		pr_info("polling update threshold: %d (bytes)\n", val);
		break;
	case SDRV_LOGD_IOC_SET_TIMER_PERIOD:
		if (copy_from_user(&val, uarg, sizeof(val)))
			return -EFAULT;

		cdev->timer_period = val;
		pr_info("polling time interval: %d (ms)\n", val);
		break;
	default:
		return sdrv_logd_cdev_buffer_handle(file, cmd, arg);
	}

	return 0;
}

static unsigned int sdrv_logd_cdev_poll(struct file *file,
					struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	logd_cdev_t *cdev = miscdev_to_logd_cdev(file->private_data);

	poll_wait(file, &cdev->waitq, wait);

	/* there is data to read */
	if (sdrv_logd_cdev_update_mask_get(cdev))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int sdrv_logd_cdev_release(struct inode *imode, struct file *file)
{
	logd_cdev_t *cdev = miscdev_to_logd_cdev(file->private_data);

	/* make sure that char device can be re-opened */
	clear_bit(SDRV_LOGD_CHAR_DEV_OPEN, &cdev->state);

	del_timer_sync(&cdev->timer);

	return 0;
}

static void sdrv_logd_cdev_timer(unsigned long data)
{
	logd_cdev_t *cdev = (logd_cdev_t *)data;
	logbuf_bk_desc_t *desc;
	int count;
	int i;

	for (i = 0; i < LOG_TYPE_MAX; i++) {
		if (!((1 << i) & cdev->log_poll_mask))
			continue;

		desc = sdrv_logbuf_get_bank_desc(i);
		if (!desc || !desc->hdr)
			continue;

		count = sdrv_logbuf_get_new_log_size(desc);
		if (count >= cdev->update_threshold)
			sdrv_logd_cdev_update_mask_set(cdev, i);
	}

	if (sdrv_logd_cdev_update_mask_get(cdev) != 0) {
		pr_debug("wake up poll thread\n");
		wake_up_interruptible(&cdev->waitq);
	}

	mod_timer(&cdev->timer, jiffies + msecs_to_jiffies(cdev->timer_period));
}

static const struct file_operations sdrv_logd_cdev_fops = {
	.owner		= THIS_MODULE,
	.open		= sdrv_logd_cdev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= sdrv_logd_cdev_ioctl,
#endif
	.unlocked_ioctl = sdrv_logd_cdev_ioctl,
	.poll		= sdrv_logd_cdev_poll,
	.release	= sdrv_logd_cdev_release,
};

static ssize_t exception_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct miscdevice *mcdev = dev_get_drvdata(dev);
	logd_cdev_t *cdev = miscdev_to_logd_cdev(mcdev);

	sdrv_exp_rst_cnt_set(cdev->regmap, FLAG_ID_AP1, 0);

	return sprintf(buf, "%u\n", sdrv_global_exp_flag_get(cdev->regmap));
}
static DEVICE_ATTR_RO(exception_state);

static struct attribute *sdrv_logd_attrs[] = {
	&dev_attr_exception_state.attr,
	NULL
};

static const struct attribute_group sdrv_logd_attribute_group = {
	.attrs = sdrv_logd_attrs,
};

static const struct attribute_group *sdrv_logd_attribute_groups[] = {
	&sdrv_logd_attribute_group,
	NULL
};

static void sdrv_logd_cdev_free(void)
{
	int i;

	for (i = 0; i < LOG_TYPE_MAX; i++)
		vfree(g_cdev->buf[i]);

	kfree(g_cdev);
}

static int __init sdrv_logd_cdev_init(void)
{
	if (!sdrv_logbuf_has_initialized()) {
		pr_err("log buffer is not initialized\n");
		return -EINVAL;
	}

	g_cdev = kzalloc(sizeof(logd_cdev_t), GFP_KERNEL);
	if (!g_cdev)
		return -ENOMEM;

	g_cdev->mcdev.minor = MISC_DYNAMIC_MINOR;
	g_cdev->mcdev.name = SDRV_LOGD_CHAR_DEV;
	g_cdev->mcdev.fops  = &sdrv_logd_cdev_fops;
	g_cdev->mcdev.groups = sdrv_logd_attribute_groups;

	g_cdev->regmap = sdrv_logbuf_regmap_get();
	if (!g_cdev->regmap)
		return -EINVAL;

	init_waitqueue_head(&g_cdev->waitq);
	setup_timer(&g_cdev->timer, sdrv_logd_cdev_timer, (unsigned long)g_cdev);
	if (misc_register(&g_cdev->mcdev)) {
		pr_err("failed to register misc device\n");
		return -EINVAL;
	}

	return 0;
}
static void __exit sdrv_logd_cdev_exit(void)
{
	del_timer_sync(&g_cdev->timer);
	misc_deregister(&g_cdev->mcdev);

	sdrv_logd_cdev_free();
}

late_initcall_sync(sdrv_logd_cdev_init);
module_exit(sdrv_logd_cdev_exit);

MODULE_DESCRIPTION("semidrive logd cdev driver");
MODULE_AUTHOR("Xingyu Chen <xingyu.chen@semidrive.com>");
MODULE_LICENSE("GPL v2");
