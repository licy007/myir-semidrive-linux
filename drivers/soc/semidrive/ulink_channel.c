/*
 * Copyright (c) 2021, Semidriver Semiconductor
 *
 * This program is free software; you can rediulinkibute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is diulinkibuted in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ulink.h>
#include <linux/workqueue.h>

#define UC_BLOCK_SIZE	0x1000
#define ULINK_MBOX_ID	0x41

typedef enum {
	MAGIC_READY = 0x5a5a5a5a,
	MAGIC_CLEAN = 0xa5a5a5a5,
} channel_magic;

typedef enum {
	type_mbox,
	type_gpio,
} ulink_channel_type;

typedef struct {
	uint32_t magic;
	uint32_t addr_main;
	uint32_t addr_sub;
	uint32_t size;
	uint8_t payload[];
} __attribute__((__packed__)) ulink_channel_block_t;

struct ulink_data {
	ulink_channel_type type;
};

struct ulink_channel_head {
	uint32_t block_num;
	uint32_t tx;
	uint32_t rx;
};

struct ulink_channel_device {
	struct device			*dev;
	ulink_channel_type		type;
	sd_msghdr_t			msg;
	uint32_t			rproc;
	struct mbox_chan		*chan;
	uint32_t			tx;
	uint32_t			rx;
	struct workqueue_struct	*ulink_queue;
	struct work_struct		ulink_work;
	void __iomem			*tx_mem;
	void __iomem			*rx_mem;
	struct ulink_channel_head __iomem *tx_head;
	struct ulink_channel_head __iomem *rx_head;
	spinlock_t			tx_lock;
	spinlock_t			rx_lock;
	int (*tx_func)(struct ulink_channel_device *tdev,
				uint32_t addr_main, uint32_t addr_sub,
				const char *data, uint32_t size);
	bool				suspend;
	struct list_head		channels;
	spinlock_t			channel_lock;
};

struct ulink_channel_inner {
	struct ulink_channel		ch;
	struct ulink_channel_device	*udev;
	struct list_head		list;
	struct completion		rx_event;
	uint8_t				rx_buf[UC_BLOCK_SIZE];
	uint32_t			rx_size;
};

struct ulink_channel_manager {
	struct ulink_channel_device *udev[ALL_DP_CPU_MAX];
};

static struct ulink_channel_manager g_manager;

static struct ulink_data ch_data = {.type = type_mbox};
static struct ulink_data gpio_ch_data = {.type = type_gpio};

static void payload(struct ulink_channel_inner *ch_inner,
		uint8_t *data, uint32_t size)
{
	memcpy_fromio(ch_inner->rx_buf, data, size);
	ch_inner->rx_size = size;
	if (ch_inner->ch.callback)
		ch_inner->ch.callback(&ch_inner->ch, ch_inner->rx_buf, size);
	else
		complete(&ch_inner->rx_event);
}


static struct ulink_channel_inner *find_channel(struct ulink_channel_device *tdev, uint32_t addr_main, uint32_t addr_sub)
{
	struct ulink_channel_inner *temp_ch;

	spin_lock(&tdev->channel_lock);
	list_for_each_entry(temp_ch, &tdev->channels, list) {
		if (temp_ch->ch.addr_sub == addr_sub) {
			spin_unlock(&tdev->channel_lock);
			return temp_ch;
		}
	}
	spin_unlock(&tdev->channel_lock);
	return NULL;
}


static void ulink_queue_work(struct work_struct *work)
{
	struct ulink_channel_device *tdev =
		container_of(work, struct ulink_channel_device, ulink_work);
	ulink_channel_block_t *block =
		(ulink_channel_block_t *)
		(tdev->rx_mem + tdev->rx_head->rx * UC_BLOCK_SIZE);
	struct ulink_channel_inner *ch_inner;

	while (block->magic == MAGIC_READY) {
		ch_inner = find_channel(tdev, block->addr_main, block->addr_sub);
		if (ch_inner)
			payload(ch_inner, block->payload, block->size);
		block->magic = MAGIC_CLEAN;
		tdev->rx_head->rx++;
		if (tdev->rx_head->rx >= tdev->rx_head->block_num)
			tdev->rx_head->rx = 1;
		block = (ulink_channel_block_t *)
			(tdev->rx_mem + tdev->rx_head->rx * UC_BLOCK_SIZE);
	}
}

static void ulink_mbox_rx(struct mbox_client *client, void *message)
{
	struct device *dev = client->dev;
	struct ulink_channel_device *tdev = dev_get_drvdata(dev);

	if (tdev->ulink_queue)
		queue_work(tdev->ulink_queue, &tdev->ulink_work);
}

static struct mbox_chan *ulink_channel_mbox_request(struct platform_device *pdev)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->rx_callback = ulink_mbox_rx;
	client->tx_prepare	= NULL;
	client->tx_done		= NULL;
	client->tx_block	= true;
	client->tx_tout		= 20000;
	client->knows_txdone	= false;

	channel = mbox_request_channel(client, 0);
	if (IS_ERR(channel)) {
		dev_warn(&pdev->dev, "Failed to request channel\n");
		return NULL;
	}

	return channel;
}

static void recive_gpio(void *data)
{
	struct ulink_channel_device *tdev = (struct ulink_channel_device *)data;

	if (tdev->ulink_queue)
		queue_work(tdev->ulink_queue, &tdev->ulink_work);
}

static int ulink_tx(struct ulink_channel_device *tdev,
				uint32_t addr_main, uint32_t addr_sub,
				const char *data, uint32_t size)
{
	ulink_channel_block_t *block;
	unsigned long flags;
	int wait_count = 0;

	if (tdev->suspend)
		return -EBUSY;

	/*wait pcie ready*/
	if (tdev->tx_head->block_num == 0xffffffff)
		return -EBUSY;


	spin_lock_irqsave(&tdev->tx_lock, flags);
	if ((tdev->tx_head->tx == 0) ||
			(tdev->tx_head->tx >= tdev->tx_head->block_num))
		tdev->tx_head->tx = 1;
	block = (ulink_channel_block_t *)
		(tdev->tx_mem + tdev->tx_head->tx * UC_BLOCK_SIZE);
	while (block->magic == MAGIC_READY) {
		if (printk_ratelimit())
			dev_err(tdev->dev, "ulink send overflow! offset= %d\n",tdev->tx_head->tx);
		udelay(10);
		if (wait_count++ > 1000) {
			spin_unlock_irqrestore(&tdev->tx_lock, flags);
			return -EBUSY;
		}
	}
	block->addr_main = addr_main;
	block->addr_sub = addr_sub;
	block->size = size;
	memcpy_toio(block->payload, data, size);
	block->magic = MAGIC_READY;
	tdev->tx_head->tx++;
	if (tdev->tx_head->tx >= tdev->tx_head->block_num)
		tdev->tx_head->tx = 1;
	spin_unlock_irqrestore(&tdev->tx_lock, flags);

	if (tdev->type == type_gpio) {
		ulink_trigger_irq(tdev->tx);
	} else {
		if (-ETIME == mbox_send_message(tdev->chan, &tdev->msg))
			dev_err(tdev->dev, "ulink mbox send irq timeout!\n");
	}

	return 0;
}

static int ulink_channel_suspend(struct device *dev)
{
	struct ulink_channel_device *tdev =
		platform_get_drvdata(to_platform_device(dev));

	tdev->suspend = true;
	return 0;
}

static int ulink_channel_resume(struct device *dev)
{
	struct ulink_channel_device *tdev =
		platform_get_drvdata(to_platform_device(dev));

	tdev->suspend = false;
	return 0;
}

static int ulink_channel_probe(struct platform_device *pdev)
{
	struct ulink_channel_device *tdev;
	struct resource *res;
	void __iomem *addr;
	struct resource *res1;
	void __iomem *addr1;
	const struct ulink_data *cop_data = of_device_get_match_data(&pdev->dev);

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	tdev->type = cop_data->type;

	if (of_property_read_u32(pdev->dev.of_node, "rproc", &tdev->rproc))
		return -ENOMEM;
	if (tdev->rproc >= ALL_DP_CPU_MAX)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	addr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(addr))
		return -ENOMEM;

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1)
		return -ENOMEM;

	addr1 = devm_ioremap(&pdev->dev, res1->start, resource_size(res1));
	if (IS_ERR_OR_NULL(addr1))
		return -ENOMEM;


	tdev->ulink_queue = create_singlethread_workqueue("ulink_channel");
	INIT_WORK(&tdev->ulink_work, ulink_queue_work);

	tdev->tx_mem = addr;
	tdev->rx_mem = addr1;
	tdev->tx_head = (struct ulink_channel_head *)addr;
	tdev->tx_head->block_num = resource_size(res) / UC_BLOCK_SIZE;
	if ((tdev->tx_head->tx == 0) || (tdev->tx_head->tx >= tdev->tx_head->block_num))
		tdev->tx_head->tx = 1;
	tdev->rx_head = (struct ulink_channel_head *)addr1;
	tdev->rx_head->block_num = resource_size(res1) / UC_BLOCK_SIZE;
	if ((tdev->rx_head->rx == 0) || (tdev->rx_head->rx >= tdev->rx_head->block_num))
		tdev->rx_head->rx = 1;
	spin_lock_init(&tdev->tx_lock);
	spin_lock_init(&tdev->rx_lock);
	tdev->tx_func = ulink_tx;

	INIT_LIST_HEAD(&tdev->channels);
	spin_lock_init(&tdev->channel_lock);

	tdev->suspend = false;
	tdev->dev = &pdev->dev;
	platform_set_drvdata(pdev, tdev);

	if (tdev->type == type_gpio) {
		if (of_property_read_u32(pdev->dev.of_node, "tx", &tdev->tx))
			return -ENOMEM;
		if (of_property_read_u32(pdev->dev.of_node, "rx", &tdev->rx))
			return -ENOMEM;
		ulink_request_irq(tdev->rx, recive_gpio, tdev);
	} else {
		mb_msg_init_head(&tdev->msg, ((tdev->rproc < DP_CPU_MAX) ? tdev->rproc : 0),
			0, 0, MB_MSG_HDR_SZ, ULINK_MBOX_ID);
		tdev->chan = ulink_channel_mbox_request(pdev);
		if (!tdev->chan) {
			destroy_workqueue(tdev->ulink_queue);
			return -EPROBE_DEFER;
		}
	}
	g_manager.udev[tdev->rproc] = tdev;

	return 0;
}

static int ulink_channel_remove(struct platform_device *pdev)
{
	struct ulink_channel_device *tdev = platform_get_drvdata(pdev);

	if (tdev->chan)
		mbox_free_channel(tdev->chan);
	if (tdev->ulink_queue)
		destroy_workqueue(tdev->ulink_queue);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ulink_channel_ops, ulink_channel_suspend, ulink_channel_resume);

static const struct of_device_id ulink_channel_match[] = {
	{ .compatible = "sd,ulink-channel", .data = &ch_data},
	{ .compatible = "sd,ulink-gpio-channel", .data = &gpio_ch_data},
	{},
};
MODULE_DEVICE_TABLE(of, ulink_channel_match);

static struct platform_driver ulink_channel_driver = {
	.driver = {
		.name = "ulink_channel",
		.of_match_table = ulink_channel_match,
		.pm = &ulink_channel_ops,
	},
	.probe  = ulink_channel_probe,
	.remove = ulink_channel_remove,
};

int ulink_channel_init(void)
{
	return  platform_driver_register(&ulink_channel_driver);
}

void ulink_channel_exit(void)
{
	platform_driver_unregister(&ulink_channel_driver);
}

static bool check_channel_exist(struct ulink_channel_device *tdev, uint32_t addr_sub)
{
	struct ulink_channel_inner *temp_ch;

	spin_lock(&tdev->channel_lock);
	list_for_each_entry(temp_ch, &tdev->channels, list) {
		if (temp_ch->ch.addr_sub == addr_sub) {
			spin_unlock(&tdev->channel_lock);
			return true;
		}
	}
	spin_unlock(&tdev->channel_lock);
	return false;
}

struct ulink_channel *ulink_request_channel(uint32_t rproc, uint32_t addr_sub, ulink_rx_callback f)
{
	struct ulink_channel_device *tdev;
	struct ulink_channel_inner *ch_inner;

	if (rproc >= ALL_DP_CPU_MAX)
		return NULL;

	tdev = g_manager.udev[rproc];
	if (tdev == NULL)
		return NULL;

	if (check_channel_exist(tdev, addr_sub))
		return NULL;

	ch_inner = devm_kzalloc(tdev->dev, sizeof(*ch_inner), GFP_KERNEL);
	if (!ch_inner)
		return NULL;

	ch_inner->ch.rproc = rproc;
	ch_inner->ch.addr_sub = addr_sub;
	ch_inner->udev = tdev;
	if (f)
		ch_inner->ch.callback = f;
	else
		init_completion(&ch_inner->rx_event);

	spin_lock(&tdev->channel_lock);
	list_add(&ch_inner->list, &tdev->channels);
	spin_unlock(&tdev->channel_lock);

	return &ch_inner->ch;
}
EXPORT_SYMBOL(ulink_request_channel);


void ulink_release_channel(struct ulink_channel *ch)
{
	struct ulink_channel_inner *ch_inner =
		container_of(ch, struct ulink_channel_inner, ch);
	struct ulink_channel_device *tdev = ch_inner->udev;

	spin_lock(&tdev->channel_lock);
	list_del(&ch_inner->list);
	spin_lock(&tdev->channel_lock);
	devm_kfree(tdev->dev, ch_inner);
}
EXPORT_SYMBOL(ulink_release_channel);

int ulink_channel_send(struct ulink_channel *ch, const char *data, uint32_t size)
{
	struct ulink_channel_inner *ch_inner =
		container_of(ch, struct ulink_channel_inner, ch);
	struct ulink_channel_device *tdev = ch_inner->udev;
	uint32_t send_size;
	uint32_t send_max = UC_BLOCK_SIZE - sizeof(ulink_channel_block_t);

	if (size > send_max)
		send_size = send_max;
	else
		send_size = size;

	return tdev->tx_func(tdev, ch->rproc, ch->addr_sub, data, send_size);
}
EXPORT_SYMBOL(ulink_channel_send);

uint32_t ulink_channel_rev(struct ulink_channel *ch, uint8_t **data)
{
	struct ulink_channel_inner *ch_inner =
		container_of(ch, struct ulink_channel_inner, ch);
	wait_for_completion(&ch_inner->rx_event);

	*data = ch_inner->rx_buf;
	return ch_inner->rx_size;
}
EXPORT_SYMBOL(ulink_channel_rev);
