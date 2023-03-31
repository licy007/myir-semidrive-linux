/*
 * Copyright (C) Semidrive Semiconductor Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/mailbox/semidrive-mailbox.h>
#include <linux/soc/semidrive/mb_msg.h>
#include "mb_regs.h"

/* Mbox has no txdone interrupt, so need to poll for that */
#define MB_TXPOLL_USEC		(40)	/* in micro second */
#define MB_TXPOLL_NSEC		(MB_TXPOLL_USEC * NSEC_PER_USEC)
#define TXDONE_POLL_COUNT 	(16)
#define CONFIG_USE_THREADED_IRQ	(0)
#define SHORTMSG_BUFLEN		(8)

static inline u32 mu_get_rx_msg_len(struct sd_mbox_device *mu,
                                    int remote_proc, int msg_id)
{
	void __iomem * rmh = mu->reg_base + CPU0_MSG0_RMH0_OFF +
		4 * (remote_proc * MB_MSG_REGs_PER_CPU + msg_id * MB_MSG_REGs_PER_MSG);

	return GFV_CPU0_MSG0_RMH0_CPU0_MSG0_LEN(readl(rmh)) * 2;
}

static inline u32 mu_get_msg_rmh1(struct sd_mbox_device *mu,
                                    int remote_proc, int msg_id)
{
	addr_t rmh = mu->reg_base + CPU0_MSG0_RMH1_OFF +
		4 * (remote_proc * MB_MSG_REGs_PER_CPU + msg_id * MB_MSG_REGs_PER_MSG);

#if CONFIG_MBOX_DUMP_HEX
	print_hex_dump_bytes("mu: rmh1: ", DUMP_PREFIX_ADDRESS,
				 rmh, 4);
#endif

	return readl(rmh);
}

static inline u32 mu_get_msg_rmh2(struct sd_mbox_device *mu,
                                    int remote_proc, int msg_id)
{
	addr_t rmh = mu->reg_base + CPU0_MSG0_RMH2_OFF +
		4 * (remote_proc * MB_MSG_REGs_PER_CPU + msg_id * MB_MSG_REGs_PER_MSG);

#if CONFIG_MBOX_DUMP_HEX
		print_hex_dump_bytes("mu: rmh2: ", DUMP_PREFIX_ADDRESS,
					 rmh, 4);
#endif

	return readl(rmh);
}

static inline bool mu_is_rx_msg_ready(struct sd_mbox_device *mu,
                                      int remote_proc, int msg_id)
{
	u32 shift = remote_proc * 4 + msg_id;

	return ((readl(mu->reg_base + TMS_OFF) & (0x01UL << shift)) != 0);
}

static inline bool mu_is_rx_msg_valid(struct sd_mbox_device *mu,
                                      int remote_proc, int msg_id)
{
	addr_t rmh = mu->reg_base + CPU0_MSG0_RMH0_OFF +
		4 * (remote_proc * MB_MSG_REGs_PER_CPU + msg_id * MB_MSG_REGs_PER_MSG);

	return (readl(rmh) & BM_CPU0_MSG0_RMH0_CPU0_MSG0_VLD);
}

static int sd_mu_fill_tmh(struct sd_mbox_chan *mlink)
{
	struct sd_mbox_tx_msg *msg = mlink->msg;

	if (likely(msg)) {
		u32 tmh0 = FV_TMH0_TXMES_LEN((ALIGN(mlink->actual_size, 2))/2)
							| FV_TMH0_MID(msg->msg_id);
		u32 reset_check;

		tmh0 |= FV_TMH0_MDP(1 << mlink->target);
		tmh0 |= BM_TMH0_TXUSE_MB | FV_TMH0_MBM(1 << msg->msg_id);

		writel(tmh0, msg->tmh);
		/* use tmh1 as source addr */
		writel(mlink->cl_data, msg->tmh + TMH1_OFF);
		/* use tmh2 as destination addr */
		writel(mlink->dest_addr, msg->tmh + TMH2_OFF);

		reset_check = readl(msg->tmh);
		if (reset_check != tmh0) {
			return -ENODEV;
		}

#if CONFIG_MBOX_DUMP_HEX
		print_hex_dump_bytes("mu: tmh: ", DUMP_PREFIX_ADDRESS,
					 msg->tmh, 12);
#endif
		return 0;
	}

	return -EINVAL;
}

/* write message to tx buffer */
u32 sd_mu_write_msg(struct sd_mbox_chan *mlink, void *data)
{
	struct sd_mbox_tx_msg *msg = mlink->msg;

	if (likely(msg)) {
		memcpy_toio((void *)msg->tx_buf, data, mlink->actual_size);
#if CONFIG_MBOX_DUMP_HEX
		print_hex_dump_bytes("mu: txb: ", DUMP_PREFIX_ADDRESS,
				 msg->tx_buf, mlink->actual_size);
#endif
	}
	return 0;
}

/* message read from rx buffer */
void* sd_mu_get_read_ptr(struct sd_mbox_device *mbox, int remote_proc, int msg_id)
{
	return (void *) mbox->rxb_base + MU_RX_BUF_OFF(remote_proc) +
		msg_id * MB_BANK_LEN;
}

/* message read from rx buffer */
u32 sd_mu_read_msg(struct sd_mbox_device *mbox, int remote_proc,
						int msg_id, u8* data, int len)
{
	memcpy_fromio(data, (void *) mbox->rxb_base + MU_RX_BUF_OFF(remote_proc) +
			msg_id * MB_BANK_LEN, len);
#if CONFIG_MBOX_DUMP_HEX
	print_hex_dump_bytes("mu : rxb: ", DUMP_PREFIX_ADDRESS,
				 data, len);
#endif
	return len;
}

/* message acknowledgement, will clear RMH valid bit & TSEND & irq */
u32 sd_mu_ack_msg(struct sd_mbox_device *mbox, int remote_proc, int msg_id)
{
	u32 shift = remote_proc * 4 + msg_id;

	writel(readl(mbox->reg_base + RMC_OFF) | (0x01UL << shift),
		mbox->reg_base + RMC_OFF);

	return 0;
}

u32 sd_mu_read_tms(struct sd_mbox_device *mbox)
{
	return readl(mbox->reg_base + TMS_OFF);
}

static void sd_mu_cancel_msg(struct sd_mbox_tx_msg *msg)
{
	writel(readl(msg->tmc) | BM_TMC0_TMC0_MSG_CANCEL, msg->tmc);
#if CONFIG_MBOX_DUMP_HEX
	print_hex_dump_bytes("mu : tmc: ", DUMP_PREFIX_ADDRESS,
				 msg->tmc, 4);
#endif
}

void sd_mu_send_msg(struct sd_mbox_tx_msg *msg)
{
	writel(readl(msg->tmc) | BM_TMC0_TMC0_MSG_SEND, msg->tmc);
#if CONFIG_MBOX_DUMP_HEX
	print_hex_dump_bytes("send : tmc: ", DUMP_PREFIX_ADDRESS,
				 msg->tmc, 4);
#endif
}

bool sd_mu_is_msg_sending(struct sd_mbox_tx_msg *msg)
{
	return (!!(readl(msg->tmc) & BM_TMC0_TMC0_MSG_SEND));
}

struct sd_mbox_tx_msg *sd_mu_alloc_msg(struct sd_mbox_device *mbdev, int prefer, bool priority)
{
	int i;
	struct sd_mbox_tx_msg *msg;

	if (prefer < MB_MAX_MSGS) {
		msg = &mbdev->tmsg[prefer];
		if (!msg->used) {
			msg->used = 1;
			return msg;
		}
	}

	for (i = 0;i < MB_MAX_MSGS;i++) {
		msg = &mbdev->tmsg[i];
		if (!msg->used) {
			msg->used = 1;
			return msg;
		}
	}

	return NULL;
}

void sd_mu_free_msg(struct sd_mbox_device *mbdev, struct sd_mbox_tx_msg *msg)
{
	msg->used = 0;
	msg->remote = (u32) -1;
}

static int sd_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct sd_mbox_chan *mlink = chan->con_priv;
	struct mbox_controller *mbox = chan->mbox;
	struct sd_mbox_device *mbdev =
		container_of(mbox, struct sd_mbox_device, controller);
	unsigned long flags;
	int prefer_msg;
	int ret = 0;

	mlink->actual_size = mb_msg_parse_packet_len(data);
	mlink->priority = mb_msg_parse_packet_prio(data);
	mlink->protocol = mb_msg_parse_packet_proto(data);

	if (mlink->actual_size > MB_BANK_LEN) {
		dev_warn(mbdev->dev, "msg len %d > expected %d, trim\n",
					mlink->actual_size, MB_BANK_LEN);
		mlink->actual_size = MB_BANK_LEN;
	}
	prefer_msg = (MB_MSG_PROTO_ROM == mlink->protocol) ? 1 : 0;

	spin_lock_irqsave(&mbdev->msg_lock, flags);
	if (likely(!mlink->msg)) {
		mlink->msg = sd_mu_alloc_msg(mbdev, prefer_msg, mlink->priority);
		if (!mlink->msg) {
			spin_unlock_irqrestore(&mbdev->msg_lock, flags);
			dev_err(mbdev->dev, "No msg available\n");
			return -EBUSY;
		}
		mlink->msg->remote = mlink->target;
	}

	dev_dbg(mbdev->dev, "mu: send_data to rproc: %d proto: %d length: %d msg: %d\n",
			mlink->target, mlink->protocol,
			mlink->actual_size, mlink->msg->msg_id);

	/* tmh is shared resource should be in atomic context */
	ret = sd_mu_fill_tmh(mlink);

	sd_mu_write_msg(mlink, data);

	if (likely(!ret))
		sd_mu_send_msg(mlink->msg);
	else {
		/* if fail the send anyway, free this msg */
		dev_err(mbdev->dev, "mu: sendto rproc%d unexpected ret: %d\n",
				mlink->target, ret);
		sd_mu_free_msg(NULL, mlink->msg);
	}
	spin_unlock_irqrestore(&mbdev->msg_lock, flags);

	return ret;
}

static void sd_mbox_cancel_data(struct mbox_chan *chan)
{
	struct sd_mbox_chan *mlink = chan->con_priv;
	struct mbox_controller *mbox = chan->mbox;
	struct sd_mbox_device *mbdev =
		container_of(mbox, struct sd_mbox_device, controller);
	unsigned long flags;

	spin_lock_irqsave(&mbdev->msg_lock, flags);
	if (mlink->msg) {
		if (sd_mu_is_msg_sending(mlink->msg)) {
			sd_mu_cancel_msg(mlink->msg);
		}
		sd_mu_free_msg(NULL, mlink->msg);
		mlink->msg = NULL;
	}
	spin_unlock_irqrestore(&mbdev->msg_lock, flags);

	return;
}
static int sd_mbox_startup(struct mbox_chan *chan)
{
	struct sd_mbox_chan *mlink = chan->con_priv;

	mlink->is_run = true;
	mlink->quota = TXDONE_POLL_COUNT;

	return 0;
}

static void sd_mbox_shutdown(struct mbox_chan *chan)
{
	struct sd_mbox_chan *mlink = chan->con_priv;

	sd_mbox_cancel_data(chan);
	mlink->is_run = false;
}

inline static bool __is_last_tx_done(struct sd_mbox_chan *mlink)
{
	if (mlink->msg &&  !sd_mu_is_msg_sending(mlink->msg)) {
		/* use up tx bugdet, postpone freemsg next time */
		if (mlink->quota-- > 0) {
			sd_mu_free_msg(NULL, mlink->msg);
			mlink->msg = NULL;
			return true;
		}
		mlink->quota = TXDONE_POLL_COUNT;
	}

	return false;
}
static bool sd_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct sd_mbox_chan *mlink = chan->con_priv;
	struct mbox_controller *mbox = chan->mbox;
	struct sd_mbox_device *mbdev =
		container_of(mbox, struct sd_mbox_device, controller);
	unsigned long flags;
	bool txdone = false;

	spin_lock_irqsave(&mbdev->msg_lock, flags);
	txdone = __is_last_tx_done(mlink);
	spin_unlock_irqrestore(&mbdev->msg_lock, flags);

	return txdone;
}

void mbox_chan_poll_txdone(struct mbox_chan *chan);
static void mbox_poll_tx_done(struct sd_mbox_device *mbdev)
{
	struct mbox_chan *chan;
	int idx;
	bool txdone;

	for (idx = MB_MAX_CHANS - 1;idx >= 0; idx--) {
		chan = &mbdev->chan[idx];
		if (chan->active_req && chan->cl) {
			txdone = sd_mbox_last_tx_done(chan);
			if (txdone)
				mbox_chan_poll_txdone(chan);
		}
	}
}

inline static struct mbox_chan *find_used_chan_atomic(
		struct sd_mbox_device *mbdev, u32 rproc, u32 dest)
{
	u32 idx;
	struct mbox_chan *chan;
	struct sd_mbox_chan *mlink;
	struct mbox_controller *mbox = &mbdev->controller;

	if (!mbox->dev || mbdev->initialized == false)
		return NULL;

	if (dest) {
		/* match exact reciever address */
		for (idx = 0;idx < MB_MAX_CHANS; idx++) {
			chan = &mbdev->chan[idx];
			mlink = &mbdev->mlink[idx];
			if (chan->cl && (dest == mlink->cl_data))
				return chan;
		}
		dev_info(mbox->dev, "mu: channel addr %d not found\n", dest);
	}

	/* fallback to VDSP, a special case without address */
	if (rproc == MASTER_VDSP) {
		/* if addr not matched, try to match processor */
		for (idx = 0;idx < MB_MAX_CHANS; idx++) {
			chan = &mbdev->chan[idx];
			mlink = &mbdev->mlink[idx];
			if (chan->cl && (rproc == mlink->target))
				return chan;
		}
	}
	dev_warn(mbox->dev, "channel addr %d not found for rproc %d\n",
			dest, rproc);

    return NULL;
}

static irqreturn_t sd_mbox_rx_interrupt(int irq, void *p)
{
	struct sd_mbox_device *mbdev = p;
	struct mbox_controller *mbox = &mbdev->controller;
	struct mbox_chan *chan = NULL;
	struct sd_mbox_chan *mlink;
	unsigned int state, msg_id;
	u32 remote_proc, mmask;
	sd_msghdr_t *msg;
	u32 dest, src;
	u32 msglen, ack = 1;
	u8 shortmsg[SHORTMSG_BUFLEN];

	if (!mbdev) {
		return IRQ_HANDLED;
	}

	state = sd_mu_read_tms(mbdev);
	if (!state) {
		dev_warn(mbox->dev, "%s: spurious interrupt %d\n",
			 __func__, irq);
		return IRQ_HANDLED;
	}

	dev_dbg(mbox->dev, "mu: rx intr state: 0x%x\n", state);

	for (remote_proc = 0; remote_proc < MB_MAX_RPROC; remote_proc++) {
		mmask = 0xf & (state >> (4 * remote_proc));
		while (mmask) {
			msg_id = __ffs(mmask); /* this bit indicates msg id */
			mmask &= (mmask - 1);

			dev_dbg(mbox->dev, "mu: remote: %d msg: %d\n", remote_proc, msg_id);

			msg = (sd_msghdr_t *) sd_mu_get_read_ptr(mbdev,
                                        remote_proc, msg_id);
			dest = mu_get_msg_rmh2(mbdev, remote_proc, msg_id);
			src = mu_get_msg_rmh1(mbdev, remote_proc, msg_id);

			/* protocol default is callback user
			 * otherwise is rom code test
			 */
			if (MB_MSG_PROTO_ROM == msg->protocol && remote_proc != MASTER_VDSP) {
				dev_info(mbox->dev, "mu: suppose not ROM msg\n");
			} else {
				chan = find_used_chan_atomic(mbdev, remote_proc, dest);
				msg->rproc = chan ? remote_proc : msg->rproc;
			}

#if CONFIG_MBOX_DUMP_HEX
			print_hex_dump_bytes("recv: msg: ", DUMP_PREFIX_ADDRESS,
						 msg, msg->dat_len);
#endif

			if (chan) {
				msglen = mu_get_rx_msg_len(mbdev, remote_proc, msg_id);
				mlink = chan->con_priv;

				if (mlink && (mlink->flags & MB_CHAN_FLAG_EARLY_ACK)
						&& msglen <= SHORTMSG_BUFLEN) {
					sd_mu_read_msg(mbdev, remote_proc,
							msg_id, shortmsg, msglen);
					sd_mu_ack_msg(mbdev, remote_proc, msg_id);
					ack = 0;

					msg = (sd_msghdr_t *)shortmsg;
				}

				mbox_chan_received_data(chan, (void *)msg);
			} else {
				mbox_back_received_data(msg, remote_proc, src, dest);
			}

			if (ack)
				sd_mu_ack_msg(mbdev, remote_proc, msg_id);
		}
	}

#if CONFIG_USE_THREADED_IRQ
	return IRQ_WAKE_THREAD;
#else
	mbox_poll_tx_done(mbdev);

	return IRQ_HANDLED;
#endif
}

#if CONFIG_USE_THREADED_IRQ
static irqreturn_t sd_mbox_irq_handler(int irq, void *p)
{
	struct sd_mbox_device *mbdev = p;
	mbox_poll_tx_done(mbdev);
	return IRQ_HANDLED;
}
#endif

static const struct mbox_chan_ops sd_mbox_ops = {
	.send_data    = sd_mbox_send_data,
	.cancel_data  = sd_mbox_cancel_data,
	.startup      = sd_mbox_startup,
	.shutdown     = sd_mbox_shutdown,
	.last_tx_done = sd_mbox_last_tx_done,
};

static struct mbox_chan *sd_mbox_of_xlate(struct mbox_controller *mbox,
					    const struct of_phandle_args *p)
{
	int local_addr, remote_link;
	int i;

	/* #mbox-cells is 2 */
	if (p->args_count != 2) {
		dev_err(mbox->dev, "Invalid arguments in dt[%d] instead of 2\n",
			p->args_count);
		return ERR_PTR(-EINVAL);
	}

	local_addr = p->args[0];
	remote_link = p->args[1];

	for (i = 0;i < mbox->num_chans;i++) {
		struct mbox_chan *mchan = &mbox->chans[i];
		struct sd_mbox_chan *mlink = mchan->con_priv;
		if (0xff == mlink->target) {
			mlink->target = MB_RPROC_ID(remote_link);
			mlink->flags = MB_CHAN_FLAGS(remote_link);
			mlink->cl_data = local_addr;
			mlink->dest_addr = MB_DST_ADDR(remote_link);
			strncpy(mlink->chan_name, p->np->name, MB_MAX_NAMES);
			return mchan;
		}
	}

	dev_info(mbox->dev, "myaddr %d, remote %d is wrong on %s\n",
		local_addr, remote_link, p->np->name);
	return ERR_PTR(-ENOENT);
}

struct sd_mbox_device *g_mbdev;
EXPORT_SYMBOL(g_mbdev);

static int sd_mbox_probe(struct platform_device *pdev)
{
	struct sd_mbox_device *mbdev;
	struct device *dev = &pdev->dev;
	struct resource *mem;
	int i, err;

	/* Allocate memory for device */
	mbdev = devm_kzalloc(dev, sizeof(*mbdev), GFP_KERNEL);
	if (!mbdev)
		return -ENOMEM;

	mbdev->irq = platform_get_irq(pdev, 0);
	if (mbdev->irq < 0)
		return mbdev->irq;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mbdev->reg_base = devm_ioremap_resource(dev, mem);
	if (IS_ERR(mbdev->reg_base)) {
		dev_err(dev, "ioremap failed\n");
		return PTR_ERR(mbdev->reg_base);
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	mbdev->txb_base = devm_ioremap_resource(dev, mem);
	if (IS_ERR(mbdev->txb_base)) {
		dev_err(dev, "ioremap failed\n");
		return PTR_ERR(mbdev->txb_base);
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	mbdev->rxb_base = devm_ioremap_resource(dev, mem);
	if (IS_ERR(mbdev->rxb_base)) {
		dev_err(dev, "ioremap failed\n");
		return PTR_ERR(mbdev->rxb_base);
	}

	dev_info(dev, "mu: mapped regbase %p %p %p\n", mbdev->reg_base,
				mbdev->txb_base, mbdev->rxb_base);

	for (i = 0; i < MB_MAX_CHANS; i++) {
		mbdev->chan[i].con_priv = &mbdev->mlink[i];
		mbdev->mlink[i].is_run = 0;
		mbdev->mlink[i].msg = NULL;
		mbdev->mlink[i].priority = (i == 0)? true : false;
		mbdev->mlink[i].protocol = 0;
		mbdev->mlink[i].target = 0xff; /* assigned during request channel */
		snprintf(mbdev->mlink[i].chan_name, MB_MAX_NAMES, "hwchan%d", i);
	}

	spin_lock_init(&mbdev->msg_lock);
	for (i = 0;i < MB_MAX_MSGS;i++) {
		mbdev->tmsg[i].used = 0;
		mbdev->tmsg[i].msg_id = i;
		mbdev->tmsg[i].use_buffer = 0;
		mbdev->tmsg[i].tmh = mbdev->reg_base + TMH0_OFF;
		mbdev->tmsg[i].tmc = mbdev->reg_base + TMC0_OFF + 4 * i;
		mbdev->tmsg[i].tx_buf = mbdev->txb_base + MB_BANK_LEN * i;
	}

#if CONFIG_USE_THREADED_IRQ
	err = devm_request_threaded_irq(dev, mbdev->irq, sd_mbox_rx_interrupt,
			sd_mbox_irq_handler, 0,
			"mbox", mbdev);
#else
	err = devm_request_irq(dev, mbdev->irq, sd_mbox_rx_interrupt, 0,
			"mbox", mbdev);
#endif
	if (err) {
		dev_err(dev, "Failed to register a mailbox IRQ handler: %d\n",
			err);
		return -ENODEV;
	}

	mbdev->controller.dev = dev;
	mbdev->controller.chans = &mbdev->chan[0];
	mbdev->controller.num_chans = MB_MAX_CHANS;
	mbdev->controller.ops = &sd_mbox_ops;
	mbdev->controller.txdone_irq = false;
	mbdev->controller.txdone_poll = true;
	mbdev->controller.txpoll_period = MB_TXPOLL_NSEC;
	mbdev->controller.of_xlate = sd_mbox_of_xlate;
	platform_set_drvdata(pdev, mbdev);

	err = mbox_controller_register(&mbdev->controller);
	if (err) {
		dev_err(dev, "Failed to register mailboxes %d\n", err);
		return err;
	}
	mbdev->dev = dev;
	mbdev->initialized = true;
	g_mbdev = mbdev;

	dev_info(dev, "Semidrive Mailbox registered\n");
	return 0;
}

static int sd_mbox_remove(struct platform_device *pdev)
{
	struct sd_mbox_device *mdev = platform_get_drvdata(pdev);

	mbox_controller_unregister(&mdev->controller);

	return 0;
}

static struct class sd_mbox_class = { .name = "mbox", };

#ifdef CONFIG_PM_SLEEP
static int sd_mbox_suspend(struct device *dev)
{
	struct sd_mbox_device *mdev = dev_get_drvdata(dev);

	if (pm_runtime_status_suspended(dev))
		return 0;

	/* TODO: */
	dev_info(dev, "not implemented, reg_base %p\n", mdev->reg_base);

	return 0;
}

static int sd_mbox_resume(struct device *dev)
{
	struct sd_mbox_device *mdev = dev_get_drvdata(dev);

	if (pm_runtime_status_suspended(dev))
		return 0;

	/* TODO: */
	dev_info(dev, "not implemented, reg_base %p\n", mdev->reg_base);

	return 0;
}
#endif

static const struct dev_pm_ops sd_mbox_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sd_mbox_suspend, sd_mbox_resume)
};

static const struct of_device_id sd_mailbox_of_match[] = {
	{
		.compatible	= "semidrive,sd-mailbox",
	},
	{
		/* end */
	},
};
MODULE_DEVICE_TABLE(of, sd_mailbox_of_match);

static struct platform_driver sd_mbox_driver = {
	.probe	= sd_mbox_probe,
	.remove	= sd_mbox_remove,
	.driver	= {
		.name = "sd-mailbox",
		.pm = &sd_mbox_pm_ops,
		.of_match_table = of_match_ptr(sd_mailbox_of_match),
	},
};

static int __init sd_mbox_init(void)
{
	int err;

	err = class_register(&sd_mbox_class);
	if (err)
		return err;

	return platform_driver_register(&sd_mbox_driver);
}
arch_initcall(sd_mbox_init);

static void __exit sd_mbox_exit(void)
{
	platform_driver_unregister(&sd_mbox_driver);
	class_unregister(&sd_mbox_class);
}
module_exit(sd_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Semidrive Mailbox Driver");
MODULE_AUTHOR("ye.liu@semidrive.com");
