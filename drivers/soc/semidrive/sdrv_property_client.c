/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>

#include "sdrv_property.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define PROP_START_MAGIC 0x7E7E
#define PROP_END_MAGIC 0x7B7B

#define PROP_HAVE_NEXT_PACK 0
#define PROP_LAST_PACK 1

#define PROCESS_OVER 1

#define PROP_NEED_INIT 0
#define PROP_CLIENT_INITED 1

#define MAGIC_ERR1_PRINT \
"property msg_checker:recv msg flag1 is 0x%04x, expect 0x%04x\n"
#define MAGIC_ERR2_PRINT \
"property msg_checker:recv msg flag2 is 0x%04x, expect 0x%04x\n"

static const char ping_msg[] = "PING TEST";
static const char chrdev_name[] = "sdrv-sys-prop";
// static const struct attribute_group *sdrv_prop_groups[];

enum str_flag {
	STR_NAME = 0,
	STR_NAME_VAL = 1,
	STR_VAL = 2,
	STR_MAX,
};

enum prop_cmd {
	PROP_CMD_PING,  // 检查通路状态
	PROP_CMD_PACK,  // 检查回复

	PROP_CMD_REQ_LIST,  // data: null   client向manager请求列表
	PROP_CMD_SND_LIST,  // data: name   manager向client回复列表
	// data: recv len  有分包情况时
	// client向manager回复上一包接收完了
	PROP_CMD_SND_LIST_ACK,

	PROP_CMD_REQ_ALL,   // data: null
	PROP_CMD_SND_ALL,   // data: name + val
	PROP_CMD_SND_ALL_ACK, // data: recv len

	PROP_CMD_REQ_VAL,   // data: id
	PROP_CMD_SND_VAL,   // data: val
	PROP_CMD_SND_VAL_ACK, // data: recv len

	// client准备好后，才开始接收更新(manager再发送更新)
	PROP_CMD_INITED,

	// 通知manager更新val
	// 设置时，防止更新不及时，是否应该先请求
	PROP_CMD_SET_VAL,
	// 当前property已经存在，设置时已经存在？
	PROP_CMD_SET_VAL_ACK,

	PROP_CMD_VAL_UPDATE,  // 广播val发生了变化
	PROP_CMD_PROP_ADD,	// 广播追加新项
	PROP_CMD_PROP_ADD_ACK, // 回复是否追踪

	PROP_CMD_MAX,
};

struct prop_msg_head {
	uint16_t flag1;  //0x7e7e
	uint16_t len;  // data_len
	uint8_t cmd;
	uint8_t end;   // last package?
	// uint32_t crc;
	uint16_t flag2;  //0x7b7b
} __packed;

struct msg_reply {
	enum prop_cmd reply_cmd;
	uint8_t	  reply_end;
	uint16_t	 reply_data_len;
};

#define MSG_HEAD_SIZE (sizeof(struct prop_msg_head))
#define MAX_DATA_LENGTH 470
#define MAX_SEND_LENGTH (MAX_DATA_LENGTH + MSG_HEAD_SIZE)
#define MAX_RECV_LENGTH (MAX_DATA_LENGTH + MSG_HEAD_SIZE)

struct rpmsg_prop_device {
	struct device *dev;
	struct miscdevice mdev;
	struct mutex lock;
	struct delayed_work delay_work;
	struct rpmsg_device *rpmsg_device;
	struct rpmsg_endpoint *channel;
	int rproc;
	struct task_struct *thr;
	int status;

	int cb_err;
	struct completion new_msg_event;
	// struct rpc_ret_msg result;
	wait_queue_head_t wait;
	char *r_str;
	struct mutex r_str_lock;

	struct prop_msg_head *recv_head;
	char recv_msg[MAX_RECV_LENGTH];
	struct prop_msg_head *send_head;
	char send_msg[MAX_SEND_LENGTH];
	char *msg_str;
	int msg_str_len;
	struct msg_reply reply;
};

struct rpmsg_prop_device *g_sdrv_prop_dev;

static int prop_msg_sender(struct rpmsg_prop_device *propdev);
int prop_client_send_set_val(char *update_msg, int msg_len)
{
	if (g_sdrv_prop_dev == NULL)
		return -1;
	if (g_sdrv_prop_dev->status != PROP_CLIENT_INITED)
		return -1;
	if (update_msg == NULL)
		return -1;

	memset(g_sdrv_prop_dev->send_msg, 0x00, MAX_SEND_LENGTH);

	// send to manager
	memcpy(g_sdrv_prop_dev->send_msg + MSG_HEAD_SIZE, update_msg, msg_len);
	g_sdrv_prop_dev->reply = (struct msg_reply) {PROP_CMD_SET_VAL,
			PROP_LAST_PACK,
			msg_len};

	prop_msg_sender(g_sdrv_prop_dev);

	return 0;
}

int semidrive_set_property_new(u32 id, u32 val)
{
	char c[100] = {0};

	sprintf(c, "%d", val);
	// return sdrv_property_list_set_val(id, c);
	return sdrv_property_client_set_val(id, NULL, c);
}

int semidrive_get_property_new(u32 id, u32 *val)
{
	int ret = 0;
	char c[100] = {0};

	ret = sdrv_property_list_get_val(id, c);
	ret = kstrtouint(c, 10, val);

	return ret;
}

int prop_ping_ack(struct rpmsg_prop_device *dev)
{
	// 通信通路OK，可以开始发送请求list
	if (strncmp(ping_msg,
		dev->recv_msg + MSG_HEAD_SIZE,
		strlen(ping_msg)) == 0) {
		pr_err("tran establish ok!!\n");

#if 0
		// request list name
		memset(dev->send_msg, 0x00, MAX_SEND_LENGTH);
		// PROP_CMD_REQ_ALL
		dev->reply = (struct msg_reply) {PROP_CMD_REQ_LIST,
			PROP_LAST_PACK,
			0};
#else
		// request all list
		memset(dev->send_msg, 0x00, MAX_SEND_LENGTH);
		dev->reply = (struct msg_reply) {PROP_CMD_REQ_ALL,
			PROP_LAST_PACK, 0};
#endif
	}
	// memset(dev->send_msg, 0x00, MAX_SEND_LENGTH);
	// memcpy(dev->send_msg + MSG_HEAD_SIZE,
	//	dev->recv_msg + MSG_HEAD_SIZE,
	//	dev->recv_head->len);
	// dev->reply = (struct msg_reply) {PROP_CMD_INITED,
	//	PROP_LAST_PACK,
	//	dev->recv_head->len};

	return 0;
}

int prop_recv_list(struct rpmsg_prop_device *dev,
	enum str_flag flag,
	enum prop_cmd snd_cmd)
{
	dev->msg_str_len += dev->recv_head->len;

	char *tmp = kmalloc(dev->msg_str_len + 1, GFP_KERNEL);

	if (!tmp) {
		// pr_err("buff malloc failed!\n");
		return -1;
	}
	memset(tmp, 0x00, dev->msg_str_len + 1);

	if (dev->msg_str) {
		memcpy(tmp, dev->msg_str,
			dev->msg_str_len - dev->recv_head->len);
		kfree(dev->msg_str);
		dev->msg_str = NULL;
	}

	memcpy(tmp + dev->msg_str_len - dev->recv_head->len,
		dev->recv_msg + MSG_HEAD_SIZE,
		dev->recv_head->len);
	dev->msg_str = tmp;

	if (dev->recv_head->end != PROP_LAST_PACK) {
		memset(dev->send_msg, 0x00, MAX_SEND_LENGTH);
		(*(uint32_t *)(dev->send_msg + MSG_HEAD_SIZE))
			= dev->msg_str_len;
		dev->reply = (struct msg_reply) {snd_cmd,
			PROP_LAST_PACK, sizeof(uint32_t)};
	} else {
		sdrv_property_str2list(flag, dev->msg_str, dev->msg_str_len);
		if (dev->msg_str) {

			kfree(dev->msg_str);
			dev->msg_str = NULL;
			dev->msg_str_len = 0;
		}

		// inited
		if (snd_cmd == PROP_CMD_SND_ALL_ACK) {
			memset(dev->send_msg, 0x00, MAX_SEND_LENGTH);
			(*(uint32_t *)(dev->send_msg + MSG_HEAD_SIZE))
				= dev->msg_str_len;
			dev->reply = (struct msg_reply) {PROP_CMD_INITED,
				PROP_LAST_PACK,
				sizeof(uint32_t)};
		}

		dev->status = PROP_CLIENT_INITED;
	}

	return 0;
}

int get_poll_str(struct rpmsg_prop_device *dev, char *str, int len)
{
	int namelen = 0;

	// TODO str list
	if (dev == NULL
		|| str == NULL
		|| len > PROP_NAME_MAX)
		return -1;

	mutex_lock(&dev->r_str_lock);

	if (dev->r_str == NULL)
		goto err_out;

	namelen = strlen(dev->r_str);
	if (namelen > len)
		goto err_out;

	memcpy(str, dev->r_str, len);

	kfree(dev->r_str);
	dev->r_str = NULL;

	mutex_unlock(&dev->r_str_lock);
	return 0;
err_out:
	mutex_unlock(&dev->r_str_lock);
	return -1;
}

int set_poll_str(struct rpmsg_prop_device *dev, char *str, int len)
{
	int namelen = 0;

	// TODO str list
	if (dev == NULL
		|| str == NULL
		|| len > PROP_NAME_MAX)
		return -1;

	mutex_lock(&dev->r_str_lock);

	if (dev->r_str) {

		kfree(dev->r_str);
		dev->r_str = NULL;
	}

	namelen = strlen(str);
	dev->r_str = kmalloc(namelen + 1, GFP_KERNEL);
	memset(dev->r_str, 0x00, namelen + 1);
	memcpy(dev->r_str, str, namelen);

	mutex_unlock(&dev->r_str_lock);
	pr_err("recv prop update:[%s]\n", dev->r_str);
	return 0;
}

int clear_poll_str(struct rpmsg_prop_device *dev)
{
	if (dev == NULL)
		return -1;

	mutex_lock(&dev->r_str_lock);

	if (dev->r_str) {

		kfree(dev->r_str);
		dev->r_str = NULL;
	}

	mutex_unlock(&dev->r_str_lock);
	return 0;
}

int check_poll_str(struct rpmsg_prop_device *dev)
{
	if (dev == NULL)
		return -1;

	mutex_lock(&dev->r_str_lock);
	if (dev->r_str) {
		mutex_unlock(&dev->r_str_lock);
		return 0;
	}

	mutex_unlock(&dev->r_str_lock);
	return -1;
}

int wake_up_read_poll(struct rpmsg_prop_device *dev, int property_id)
{
	int ret = 0;
	char name[PROP_NAME_MAX] = {0};
	char value[PROP_VALUE_MAX] = {0};
	int namelen = 0;

	// save name
	ret = sdrv_property_list_get_name_val(property_id, name,
		PROP_NAME_MAX,
		value,
		PROP_VALUE_MAX);
	if (ret < 0)
		return ret;

	set_poll_str(dev, name, PROP_NAME_MAX);
	// send update notify to app
	wake_up_all(&dev->wait);

	return ret;
}

int prop_recv_update(struct rpmsg_prop_device *dev)
{
	char *recv_data = dev->recv_msg + MSG_HEAD_SIZE;
	int *property_id = (int *)recv_data;
	char *val = recv_data + sizeof(int);

	sdrv_property_list_set_val(*property_id, val);
	wake_up_read_poll(dev, *property_id);
	return PROCESS_OVER;
}

int prop_recv_add(struct rpmsg_prop_device *dev)
{
	char *recv_data = dev->recv_msg + MSG_HEAD_SIZE;
	int *property_id = (int *)recv_data;
	char *val = recv_data + sizeof(int);

	pr_info("recv add [%d][%s]\n", *property_id, val);

	sdrv_property_str2list(STR_NAME_VAL, val, strlen(val));

	// 可以选择是否关注，不关注，
	// 对于list应该有对应的标志.
	memset(dev->send_msg, 0, MAX_SEND_LENGTH);
	// 1表示关注，0表示不关注
	(*(uint32_t *)(dev->send_msg + MSG_HEAD_SIZE)) = *property_id;
	(*(uint32_t *)(dev->send_msg + MSG_HEAD_SIZE + sizeof(uint32_t))) = 1;
	dev->reply = (struct msg_reply) {PROP_CMD_PROP_ADD_ACK,
		PROP_LAST_PACK,
		2 * sizeof(uint32_t)};

	wake_up_read_poll(dev, *property_id);

	return 0;
}

static int prop_msg_sender(struct rpmsg_prop_device *propdev)
{
	struct prop_msg_head *send_head;
	enum prop_cmd cmd = propdev->reply.reply_cmd;
	uint16_t data_len = propdev->reply.reply_data_len;
	uint8_t end = propdev->reply.reply_end;
	int ret = -1;
	uint16_t total_len;
	struct rpmsg_device *rpmsg_device = propdev->rpmsg_device;

	if (data_len > MAX_DATA_LENGTH) {
		dev_info(&rpmsg_device->dev,
			"%s: length error, data_len =%d\n",
			__func__,
			data_len);
		return -1;
	}

	total_len = data_len + MSG_HEAD_SIZE;
	send_head = propdev->send_head;
	/*clean the buffer*/
	memset(propdev->send_msg, 0, MSG_HEAD_SIZE);
	/*head*/
	send_head->flag1	=   PROP_START_MAGIC;
	send_head->len	  =   data_len;
	send_head->cmd	  =   cmd;
	send_head->end	  =   end;
	send_head->flag2	=   PROP_END_MAGIC;
	/*data is already put in the send_msg*/
	/*crc*/
	// send_head->crc	  =   crc32(0, dev->send_msg, total_len);

	/*send msg*/
	ret = rpmsg_send(rpmsg_device->ept, propdev->send_msg, total_len);
	if (ret) {
		dev_info(&rpmsg_device->dev,
			"%s: send data error, ret=%d\n",
			__func__, ret);
		return -1;
	}

	return 0;
}

static int prop_msg_checker(struct rpmsg_prop_device *dev)
{
	struct prop_msg_head *recv_head =  dev->recv_head;

	if (recv_head->len > MAX_DATA_LENGTH) {
		pr_err("property msg_checker:recv msg len error");
		return -1;
	}

	if (recv_head->flag1 != PROP_START_MAGIC) {
		pr_err(MAGIC_ERR1_PRINT,
		recv_head->flag1, PROP_START_MAGIC);
		return -1;
	}

	if (recv_head->flag2 != PROP_END_MAGIC) {
		pr_err(MAGIC_ERR2_PRINT,
		recv_head->flag2, PROP_END_MAGIC);
		return -1;
	}

	return 0;
}

static int prop_msg_processer(struct rpmsg_prop_device *dev)
{
	/*clear send data*/
	memset(dev->send_msg, 0, MAX_SEND_LENGTH);
	int ret = 0;

	/*chose action from cmd*/
	switch (dev->recv_head->cmd) {
	case PROP_CMD_PACK:
		ret = prop_ping_ack(dev);
		break;
	case PROP_CMD_SND_LIST:
		ret = prop_recv_list(dev,
			STR_NAME, PROP_CMD_SND_LIST_ACK);
		break;
	case PROP_CMD_SND_ALL:
		ret = prop_recv_list(dev,
			STR_NAME_VAL, PROP_CMD_SND_ALL_ACK);
		break;
	case PROP_CMD_VAL_UPDATE:
		ret = prop_recv_update(dev);
		break;
	case PROP_CMD_PROP_ADD:
		ret = prop_recv_add(dev);
		break;
	default:
		pr_err("property msg_processer: error, unknown cmd: %d\n",
				dev->recv_head->cmd);
		ret = -1;
	}

	return ret;
}

static int prop_msg_handler(void *arg)
{
	struct rpmsg_prop_device *dev = (struct rpmsg_prop_device *)arg;
	int ret = -1;

	while (true) {
		wait_for_completion(&dev->new_msg_event);

		/*check msg*/
		ret = prop_msg_checker(dev);

		if (ret < 0) {
			pr_err("%s: msg format error\n", __func__);
			continue;
		}

		/*process msg*/
		ret = prop_msg_processer(dev);

		if (ret) {
			if (ret == PROCESS_OVER) {
				pr_info("%s: msg process over\n", __func__);
				continue;
			}
			pr_err("%s: msg process error\n", __func__);
			continue;
		}

		/*send reply msg*/
		ret = prop_msg_sender(dev);

		if (ret) {
			pr_err("%s: msg send error\n", __func__);
			continue;
		}
	}

	return 0;
}

static void prop_work_handler(struct work_struct *work)
{
	struct rpmsg_prop_device *propdev = container_of(to_delayed_work(work),
					 struct rpmsg_prop_device, delay_work);
	int ret;
	char ping_msg[] = "PING TEST";

	memcpy(propdev->send_msg + MSG_HEAD_SIZE, ping_msg, strlen(ping_msg));
	propdev->reply = (struct msg_reply) {PROP_CMD_PING,
		PROP_LAST_PACK,
		strlen(ping_msg)};

	ret = prop_msg_sender(propdev);

	if (ret)
		schedule_delayed_work(&propdev->delay_work,
			msecs_to_jiffies(100));

}

static int sdrv_prop_misc_open(struct inode *node, struct file *file)
{
	int num = MINOR(node->i_rdev);
	struct rpmsg_prop_device *prop = g_sdrv_prop_dev;

	if (num < 0)
		return -ENODEV;

	file->private_data = prop;
	// init_waitqueue_head(&prop->wait);
	clear_poll_str(prop);

	dev_info(prop->dev, "open node\n");
	return 0;
}

ssize_t sdrv_prop_misc_read(struct file *file,
	char __user *buf,
	size_t size,
	loff_t *ppos)
{
	struct rpmsg_prop_device *prop = file->private_data;
	ssize_t len = 0;
	char name[PROP_NAME_MAX] = {0};

	if (file->f_flags & O_NONBLOCK)
		return -EAGAIN;

	if (get_poll_str(prop, name, PROP_NAME_MAX)) {
		DECLARE_WAITQUEUE(wait, current);

		add_wait_queue(&prop->wait, &wait);

		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (!get_poll_str(prop, name, PROP_NAME_MAX))
				break;
			if (signal_pending(current))
				break;
			schedule();
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&prop->wait, &wait);
	}

	len = strlen(name);
	if (copy_to_user(buf, name, len))
		dev_info(prop->dev, "copy to user failed: %s\n", name);

	return len;
}

static unsigned int sdrv_prop_misc_poll(struct file *file, poll_table *wait)
{
	struct rpmsg_prop_device *prop = file->private_data;

	poll_wait(file, &prop->wait, wait);

	if (!check_poll_str(prop))
		return POLLIN | POLLRDNORM;

	return 0;
}

static long sdrv_prop_misc_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int ret = 1;
	struct rpmsg_prop_device *prop = file->private_data;
	struct sdrv_prop_ioctl io_data;
	int *ret_id = NULL;

	if (_IOC_TYPE(cmd) != SDRV_PROP_IOCTL_BASE)
		return -EINVAL;

	if (_IOC_NR(cmd) > 4)
		return -EINVAL;

	if (_IOC_DIR(cmd) & _IOC_READ) {
		ret = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
		if (ret)
			return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
		if (ret)
			return -EFAULT;
	}

	if (!prop) {
		pr_err("%s: dev isn't inited. prop is NULL\n", __func__);
		return -EFAULT;
	}

	ret = copy_from_user(&io_data,
		(struct sdrv_prop_ioctl __user *)arg,
		sizeof(struct sdrv_prop_ioctl));
	if (ret) {
		dev_err(prop->dev, "copy from user failed\n");
		return -EFAULT;
	}

	switch (cmd) {
	case PCMD_SDRV_PROP_ADD:
		mutex_lock(&prop->lock);
		sdrv_property_client_set_val(-1,
			io_data.name,
			io_data.val);
		mutex_unlock(&prop->lock);
		break;
	case PCMD_SDRV_PROP_GET:
		mutex_lock(&prop->lock);
		ret_id = &io_data.id;
		*ret_id = sdrv_property_list_find(io_data.name,
				io_data.val,
				PROP_VALUE_MAX);
		mutex_unlock(&prop->lock);
		break;
	case PCMD_SDRV_PROP_FOREACH:
		mutex_lock(&prop->lock);
		ret_id = &io_data.id;
		ret = sdrv_property_list_get_name_val(io_data.id,
				io_data.name,
				PROP_NAME_MAX,
				io_data.val,
				PROP_VALUE_MAX);
		if (ret < 0)
			*ret_id = ret;
		mutex_unlock(&prop->lock);
		break;
	}

	ret = copy_to_user((struct sdrv_prop_ioctl __user *)arg,
			&io_data,
			sizeof(struct sdrv_prop_ioctl));
	if (ret) {
		dev_err(prop->dev, "copy to user failed\n");
		return -EFAULT;
	}

	return (long)ret;
}

static const struct file_operations sdrv_prop_file_ops = {
	.owner = THIS_MODULE,
	.open = sdrv_prop_misc_open,
	.read = sdrv_prop_misc_read,
	.poll = sdrv_prop_misc_poll,
	.unlocked_ioctl = sdrv_prop_misc_ioctl,
	// .release = sdrv_prop_misc_release,
};

static int sdrv_prop_misc_init(struct rpmsg_prop_device *prop)
{
	int ret = 0;
	struct miscdevice *m = &prop->mdev;

	m->minor = MISC_DYNAMIC_MINOR;
	m->name = kasprintf(GFP_KERNEL, "%s", chrdev_name);
	m->fops = &sdrv_prop_file_ops;
	m->parent = NULL;
	// m->groups = sdrv_prop_groups;

	ret = misc_register(m);
	if (ret) {
		dev_err(prop->dev, "failed to register miscdev\n");
		return ret;
	}

	dev_info(prop->dev, "%s misc register\n", chrdev_name);

	return ret;
}

static int rpmsg_propdev_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_prop_device *propdev;

	propdev = devm_kzalloc(&rpdev->dev, sizeof(*propdev), GFP_KERNEL);
	if (!propdev)
		return -ENOMEM;

	memset(propdev->recv_msg, 0x00, MAX_RECV_LENGTH);
	propdev->recv_head = (struct prop_msg_head *)propdev->recv_msg;
	propdev->send_head = (struct prop_msg_head *)propdev->send_msg;
	propdev->msg_str = NULL;
	propdev->msg_str_len = 0;
	propdev->status = PROP_NEED_INIT;

	propdev->channel = rpdev->ept;
	propdev->dev = &rpdev->dev;

	mutex_init(&propdev->lock);
	propdev->rpmsg_device = rpdev;
	dev_set_drvdata(&rpdev->dev, propdev);

	g_sdrv_prop_dev = propdev;

	init_waitqueue_head(&propdev->wait);
	init_completion(&propdev->new_msg_event);

	sdrv_property_list_init();

	/* just async call RPC */
	INIT_DELAYED_WORK(&propdev->delay_work, prop_work_handler);
	schedule_delayed_work(&propdev->delay_work, msecs_to_jiffies(500));

	propdev->thr = kthread_run(prop_msg_handler,
		(void *)propdev,
		"rpmsg_propdev_recv");
	if (!propdev->thr || IS_ERR(propdev->thr)) {
		dev_info(&rpdev->dev,
			"rpmsg_propdev_recv thread create failed!\n");
		return -1;
	}

	mutex_init(&propdev->r_str_lock);
	sdrv_prop_misc_init(propdev);

	/* TODO: add kthread to serve RPC request */
	dev_info(&rpdev->dev, "Semidrive Rpmsg Property device probe!\n");

	return 0;
}

static void rpmsg_propdev_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_prop_device *propdev = dev_get_drvdata(&rpdev->dev);

	propdev->rpmsg_device = NULL;
	dev_set_drvdata(&rpdev->dev, NULL);
}

static int rpmsg_propdev_cb(struct rpmsg_device *rpdev,
	void *data, int len, void *priv, u32 src)
{
	struct rpmsg_prop_device *propdev = dev_get_drvdata(&rpdev->dev);
	// struct rpc_ret_msg *result;

	/* sanity check */
	if (!rpdev) {
		dev_err(NULL, "rpdev is NULL\n");
		return -EINVAL;
	}

	if (!data || !len) {
		dev_err(&rpdev->dev,
			"unexpected empty message received from src=%d\n", src);
		return -EINVAL;
	}

	// dev_err(&rpdev->dev,
	//	 "recv msg: %s\n", data);
	memset(propdev->recv_msg, 0, MAX_RECV_LENGTH);
	memcpy(propdev->recv_msg,
		(char *)data,
		MIN((unsigned int)len, MAX_RECV_LENGTH));
	complete(&propdev->new_msg_event);
	return 0;
}

static struct rpmsg_device_id rpmsg_propdev_id_table[] = {
	{.name = "prop-ap"},
	{},
};

static struct rpmsg_driver rpmsg_propdev_driver = {
	.probe = rpmsg_propdev_probe,
	.remove = rpmsg_propdev_remove,
	.callback = rpmsg_propdev_cb,
	.id_table = rpmsg_propdev_id_table,
	.drv = {
		.name = "rpmsg-prop"
	},
};

static int rpmsg_propdev_init(void)
{
	int ret;

	ret = register_rpmsg_driver(&rpmsg_propdev_driver);
	if (ret < 0)
		pr_err("rpmsg-test: failed to register rpmsg driver\n");

	pr_err("Semidrive Rpmsg driver for Property device registered\n");

	return ret;
}
arch_initcall(rpmsg_propdev_init);

static void rpmsg_propdev_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_propdev_driver);
}
module_exit(rpmsg_propdev_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_ALIAS("rpmsg:propdev");
MODULE_DESCRIPTION("Semidrive Rpmsg Property kernel mode driver");
// MODULE_LICENSE("GPL v1");
