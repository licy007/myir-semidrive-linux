/*
 * Copyright (C) 2020 Semidrive Semiconductor
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>

/* net device info */
#include <linux/in.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>
#include <linux/in6.h>
#include <asm/checksum.h>
#include <net/arp.h>
#include <linux/if_ether.h>
#include <net/pkt_sched.h>
#include <linux/io.h>
#include <linux/soc/semidrive/sys_counter.h>

#define RPMSG_ETHERNET_EPT		60
#define RPMSG_ETHERNET_NAME		"rpmsg-net"
#define MAX_DEV_MTU 			(64000)
#define DEFAULT_DEV_MTU 		(ETH_DATA_LEN)
#define RPMSG_ETH_MAX_RX_BUFS	(128)
#define RPMSG_ETH_MAX_TX_BUFS	(128)

/* make sure we don't miss shut_period == 1 */
#define RPMSG_SHUT_PERIOD_1	BIT(0)
#define RPMSG_SHUT_PERIOD_2	BIT(1)

struct rpmsg_eth_device {
	struct rpmsg_device *rpmsg_chnl;
	struct rpmsg_endpoint *ether_ept;
	struct net_device_stats stats;
	struct net_device *dev;
	struct rpmsg_channel_info chinfo;
	int link_status;
	unsigned long last_update_time;
	int link_check_timeout;
	struct timer_list keepalive_timer;

	unsigned long last_stats_time;
	unsigned long last_obytes;
	unsigned long last_ibytes;
	unsigned long last_opkts;
	unsigned long last_ipkts;

	/* detect version compatible of remote endpoint */
	int detect;
	int version;
	struct work_struct keepalive_task;
	u32 shut_period;
	u32 peer_state;
	int flush_pending;
	wait_queue_head_t shut_wait;
	struct completion shut_com;

	/* MTU is for rpmsg payload */
	int rpmsg_mtu;

	/* ether frame fragment */
	bool frag;
	uint32_t frame_len;
	uint32_t frame_received;
	struct sk_buff *skb_stage;

	/* TX */
	spinlock_t lock;
	struct sk_buff_head txqueue;
	struct sk_buff_head txwait_queue;
	struct workqueue_struct *tx_wq;
	struct work_struct tx_work;
	uint32_t tx_high_wm;
	uint32_t tx_low_wm;
	int tx_slow_down;

	/* RX */
	struct sk_buff_head rxqueue;
	struct workqueue_struct *rx_wq;
	struct work_struct rx_work;
	uint32_t rx_low_wm;
	uint32_t rx_high_wm;
	uint32_t flow_control;
};

struct rpmsg_net_device {
	struct rpmsg_device *rpmsg_chnl;
	struct net_device *ether_dev;
};

#define PL_TYPE_INVALID_HDR		(0)
#define PL_TYPE_FRAG_INDICATOR	(1)
#define PL_TYPE_ENTIRE_PACK		(2)
#define PL_TYPE_FRAG_END		(4)
#define PL_TYPE_DETECT			(7)
#define PL_TYPE_DETECT_ACK		(8)
#define PL_TYPE_PKT_AVAIL		(16)
#define PL_TYPE_TACK_OK			(17)
#define PL_TYPE_TACK_ERR		(18)
#define PL_TYPE_KEEPALIVE		(19)
#define PL_TYPE_KEEPALIVE2		(20)
#define PL_TYPE_SHUTDOWN		(21)
#define PL_TYPE_FLOW_CTRL		(22)
#define PL_MAGIC				(0x88)

/* keep all kind of basic header in 4 Bytes */
struct rpmsg_eth_header {
	uint8_t type;
	uint8_t	magic;
	uint16_t option;
};

/* keep all kind of extended header in 32 Bytes */
struct rpmsg_eth_header_ctrl {
	struct rpmsg_eth_header hdr;
	uint32_t option1;
	uint32_t usrdat[6];
};

struct rpmsg_eth_header_data {
	struct rpmsg_eth_header hdr;
	uint64_t phy_addr;
	uint64_t tag;
	uint32_t ts;
};

static int rpmsg_eth_send_keepalive(struct rpmsg_eth_device *eth);
int rpmsg_eth_stop(struct net_device *ndev);

inline static void rpmsg_eth_down(struct rpmsg_eth_device *priv)
{
	priv->link_status = 0;
	netif_carrier_off(priv->dev);
}

inline static void rpmsg_eth_up(struct rpmsg_eth_device *priv)
{
	priv->link_status = 1;
	netif_carrier_on(priv->dev);
}

static void rpmsg_eth_keepalive(unsigned long data)
{
	struct rpmsg_eth_device *eth = (struct rpmsg_eth_device *)data;
	/* Do the rest outside of interrupt context */
	schedule_work(&eth->keepalive_task);
}

static void __reset_link_status(struct rpmsg_eth_device *eth)
{
	eth->link_check_timeout = 0;
	eth->last_update_time = 0;
	eth->detect = -1;
	eth->version = 1;
}

static void __flush_work_task(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;
	/* flush all ongoing work, make sure all pkt be processed */

	/* notify tx work to exit */
	eth->flush_pending = 1;
	complete(&eth->shut_com);

	/* by flushing workqueue we know tx work is done */
	flush_workqueue(eth->tx_wq);
	eth->flush_pending = 0;

	flush_workqueue(eth->rx_wq);

	flush_work(&eth->tx_work);
	flush_work(&eth->rx_work);
}

static void __release_all_tx_buf(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;

	if (skb_queue_len(&eth->txqueue) != 0) {
		dev_warn(&ndev->dev, "flushing txq %d\n", skb_queue_len(&eth->txqueue));
		skb_queue_purge(&eth->txqueue);
	}

	if (skb_queue_len(&eth->txwait_queue) != 0) {
		dev_warn(&ndev->dev, "force flushing tx wq %d\n", skb_queue_len(&eth->txwait_queue));
		skb_queue_purge(&eth->txwait_queue);
	}
}

static void __release_all_rx_buf(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;

	if (skb_queue_len(&eth->rxqueue) != 0) {
		dev_warn(&ndev->dev, "flushing rxq %d\n", skb_queue_len(&eth->rxqueue));
		skb_queue_purge(&eth->rxqueue);
	}
}

static void __grace_shutdown(struct rpmsg_eth_device *eth, bool wait)
{
	struct net_device *ndev = eth->dev;
	int ret;

	dev_info(&ndev->dev, "grace shutdown enter\n");

	if (netif_running(ndev))
		rpmsg_eth_down(eth);

	if (wait) {
		ret = wait_event_interruptible_timeout(eth->shut_wait,
				(eth->shut_period & RPMSG_SHUT_PERIOD_2),
				msecs_to_jiffies(1000));
		if (!ret) {
			dev_warn(&ndev->dev, "timeout waiting for shutdown 2\n");
		}
	}

	__flush_work_task(eth);
	__release_all_rx_buf(eth);
	__release_all_tx_buf(eth);
	__reset_link_status(eth);
	eth->shut_period = 0;
	dev_warn(&ndev->dev, "grace shutdown complete\n");

	rpmsg_eth_up(eth);
	eth->last_update_time = jiffies;
	netif_start_queue(ndev);
	mod_timer(&eth->keepalive_timer, jiffies + msecs_to_jiffies(500));
}
static void rpmsg_eth_keepalive_task(struct work_struct *work)
{
	struct rpmsg_eth_device *eth = container_of(work,
						   struct rpmsg_eth_device,
						   keepalive_task);
	struct net_device *ndev = eth->dev;
	unsigned long timeout;
	int ret;

	if (eth->shut_period & RPMSG_SHUT_PERIOD_1)
		return __grace_shutdown(eth, true);

	timeout = eth->last_update_time + msecs_to_jiffies(500);
	if (time_after(jiffies, timeout)) {
		ret = rpmsg_eth_send_keepalive(eth);
		if (ret < 0) {
			dev_warn(&ndev->dev, "detect low link down ret=%d", ret);
			return __grace_shutdown(eth, false);
		}
	}

	timeout = eth->last_stats_time + msecs_to_jiffies(2000);
	if (time_after(jiffies, timeout)) {
		struct net_device_stats *stats = &eth->stats;
		unsigned long obytes = stats->tx_bytes - eth->last_obytes;
		unsigned long ibytes = stats->rx_bytes - eth->last_ibytes;
		unsigned long opkts  = stats->tx_packets - eth->last_opkts;
		unsigned long ipkts  = stats->rx_packets - eth->last_ipkts;
		uint32_t opktlen = opkts ? obytes / opkts : 0;
		uint32_t ipktlen = ipkts ? ibytes / ipkts : 0;
		uint32_t diff = jiffies_to_msecs(jiffies - eth->last_stats_time);

		if (opkts || ipkts)
			dev_info(&ndev->dev, "Stats: OUT l:%d pkt:%ld x:%ld e:%ld IN l:%d pkt:%ld x:%ld e:%ld q:[%d %d %d]",
				opktlen, opkts, obytes / diff, stats->tx_errors,
				ipktlen, ipkts, ibytes / diff, stats->rx_errors,
				skb_queue_len(&eth->txwait_queue),
				skb_queue_len(&eth->txqueue),
				skb_queue_len(&eth->rxqueue));

		eth->last_obytes = stats->tx_bytes;
		eth->last_ibytes = stats->rx_bytes;
		eth->last_opkts  = stats->tx_packets;
		eth->last_ipkts  = stats->rx_packets;
		eth->last_stats_time = jiffies;
	}

	mod_timer(&eth->keepalive_timer, jiffies + msecs_to_jiffies(500));
}

static int rpmsg_eth_xmit_check_stop(struct rpmsg_eth_device *eth);
/*
 * The higher levels take care of making this non-reentrant (it's
 * called with bh's disabled).
 */
static netdev_tx_t rpmsg_eth_xmit(struct sk_buff *skb,
                            struct net_device *ndev)
{
	struct rpmsg_eth_device *eth = netdev_priv(ndev);

	if (!netif_carrier_ok(ndev)) {
		eth->stats.tx_carrier_errors++;
		eth->stats.tx_errors++;
		eth->stats.tx_dropped += skb->len;
		return NETDEV_TX_BUSY;
	}

	/* too many ptk to xmit in queue, flow control */
	//rpmsg_eth_xmit_check_stop(eth);
	if ((skb_queue_len(&eth->txqueue) > RPMSG_ETH_MAX_TX_BUFS)) {
		eth->stats.tx_fifo_errors++;
		eth->stats.tx_errors++;
		eth->stats.tx_dropped += skb->len;
		 return NETDEV_TX_BUSY;
	}

	skb_queue_tail(&eth->txqueue, skb);

	if(!work_pending(&eth->tx_work))
		queue_work(eth->tx_wq, &eth->tx_work);

	return NETDEV_TX_OK;
}

static int rpmsg_eth_open(struct net_device *ndev)
{
	struct rpmsg_eth_device *eth = netdev_priv(ndev);

	netif_start_queue(ndev);

	mod_timer(&eth->keepalive_timer, jiffies + HZ);
	eth->last_update_time = jiffies;

	dev_info(&ndev->dev, "opened\n");

	return 0;
}

int rpmsg_eth_stop(struct net_device *ndev)
{
	struct rpmsg_eth_device *eth = netdev_priv(ndev);

	netif_stop_queue(ndev);

	dev_info(&ndev->dev, "flushing wq\n");
	__flush_work_task(eth);

	dev_info(&ndev->dev, "freeing all buffers\n");
	__release_all_rx_buf(eth);
	__release_all_tx_buf(eth);
	dev_info(&ndev->dev, "stopped\n");

	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int rpmsg_eth_config(struct net_device *ndev, struct ifmap *map)
{
	if (ndev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* ignore other fields */
	return 0;
}

static int rpmsg_eth_set_mac_address(struct net_device *ndev, void *addr)
{
	int ret = 0;

	ret = eth_mac_addr(ndev, addr);

	return ret;
}

struct net_device_stats *rpmsg_eth_stats(struct net_device *ndev)
{
	struct rpmsg_eth_device *priv = netdev_priv(ndev);
	return &priv->stats;
}

static u32 always_on(struct net_device *ndev)
{
	return 1;
}

static const struct ethtool_ops rpmsg_ethtool_ops = {
	.get_link           = always_on,
};

static int rpmsg_eth_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct rpmsg_eth_device *priv = netdev_priv(ndev);

	if (netif_running(ndev))
		rpmsg_eth_down(priv);

	dev_info(&ndev->dev, "changing MTU from %d to %d\n", ndev->mtu, new_mtu);
	ndev->mtu = new_mtu;

	if (netif_running(ndev))
		rpmsg_eth_up(priv);

	return 0;
}

static const struct net_device_ops rpmsg_eth_ops = {
	.ndo_open           = rpmsg_eth_open,
	.ndo_stop           = rpmsg_eth_stop,
	.ndo_start_xmit     = rpmsg_eth_xmit,
	.ndo_set_config     = rpmsg_eth_config,
	.ndo_set_mac_address = rpmsg_eth_set_mac_address,
	.ndo_validate_addr  = eth_validate_addr,
	.ndo_get_stats      = rpmsg_eth_stats,
	.ndo_change_mtu     = rpmsg_eth_change_mtu
};

static struct sk_buff *rpmsg_eth_alloc_skb(struct rpmsg_eth_device *eth, int len)
{
	struct net_device *ndev = eth->dev;
	struct sk_buff *skb;

	skb = __netdev_alloc_skb_ip_align(ndev, len, GFP_KERNEL);
	if (!skb) {
		/* skb alloc fail, will drop contiguous fragment */
		eth->stats.rx_errors++;
		eth->stats.rx_dropped += len;
		return NULL;
	}

	return skb;
}

static int rpmsg_eth_ack_ok(struct rpmsg_eth_device *eth, struct rpmsg_eth_header_data *hdr)
{
	hdr->hdr.type = PL_TYPE_TACK_OK;

	return rpmsg_sendto(eth->ether_ept, hdr, sizeof(*hdr), eth->chinfo.dst);
}

#define PL_FLOW_CTRL_BEGIN	(1)
#define PL_FLOW_CTRL_END	(2)

static int rpmsg_eth_flow_ctrl(struct rpmsg_eth_device *eth, struct rpmsg_eth_header_ctrl *hdr, int action)
{
	hdr->hdr.type = PL_TYPE_FLOW_CTRL;
	hdr->hdr.magic = PL_MAGIC;
	hdr->hdr.option  = action;

	return rpmsg_sendto(eth->ether_ept, hdr, sizeof(*hdr), eth->chinfo.dst);
}

static int rpmsg_eth_ack_err(struct rpmsg_eth_device *eth, struct rpmsg_eth_header_data *hdr)
{
	hdr->hdr.type = PL_TYPE_TACK_ERR;
	hdr->hdr.magic = PL_MAGIC;

	return rpmsg_sendto(eth->ether_ept, hdr, sizeof(*hdr), eth->chinfo.dst);
}

static int rpmsg_eth_annouce_feature(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;
	struct rpmsg_eth_header hdr;
	int err;
	int fail_cnt;
	const int max_fail_cnt = 15;
	unsigned long timeout;

	hdr.type = PL_TYPE_DETECT;
	hdr.magic = PL_MAGIC;
	hdr.option = ndev->mtu;

	fail_cnt = 0;
	while (fail_cnt++ < max_fail_cnt) {
		err = rpmsg_trysendto(eth->ether_ept,
				&hdr,
				sizeof(hdr),
				eth->chinfo.dst);

		if (err != -ENOMEM)
			break;

		/* wake up every 1s (total 15s) to try if no txbuf */
		if (wait_for_completion_timeout(
					&eth->shut_com,
					msecs_to_jiffies(1000)) > 0) {
			/* interrupted by other thread, stop and exit */
			break;
		}
	}

	return err;
}

static int rpmsg_eth_ack_feature(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;
	struct rpmsg_eth_header hdr;

	hdr.type = PL_TYPE_DETECT_ACK;
	hdr.magic = PL_MAGIC;
	hdr.option = ndev->mtu;
	return rpmsg_sendto(eth->ether_ept, &hdr, sizeof(hdr), eth->chinfo.dst);
}

static int rpmsg_eth_send_keepalive(struct rpmsg_eth_device *eth)
{
	struct rpmsg_eth_header hdr;

	hdr.type = PL_TYPE_KEEPALIVE;
	hdr.magic = PL_MAGIC;
	hdr.option = eth->version;
	return rpmsg_sendto(eth->ether_ept, &hdr, sizeof(hdr), eth->chinfo.dst);
}

static int rpmsg_eth_reply_keepalive(struct rpmsg_eth_device *eth)
{
	struct rpmsg_eth_header hdr;

	hdr.type = PL_TYPE_KEEPALIVE2;
	hdr.magic = PL_MAGIC;
	hdr.option = eth->version;
	return rpmsg_sendto(eth->ether_ept, &hdr, sizeof(hdr), eth->chinfo.dst);
}

static int rpmsg_eth_send_shutdown(struct rpmsg_eth_device *eth, int code)
{
	struct rpmsg_eth_header hdr;

	hdr.type = PL_TYPE_SHUTDOWN;
	hdr.magic = PL_MAGIC;
	hdr.option = code;
	return rpmsg_trysendto(eth->ether_ept, &hdr, sizeof(hdr), eth->chinfo.dst);
}

static void rpmsg_eth_netif_rcv(struct rpmsg_eth_device *eth, struct sk_buff *skb)
{
	spin_lock_bh(&eth->lock);
	skb->protocol = eth_type_trans(skb, eth->dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check checksum */
	eth->stats.rx_packets++;
	eth->stats.rx_bytes += eth->frame_len;
	netif_rx(skb);
	spin_unlock_bh(&eth->lock);
}

static int rpmsg_eth_xmit_check_stop(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;
	uint32_t in_wtq = skb_queue_len(&eth->txwait_queue);
	uint32_t in_txq = skb_queue_len(&eth->txqueue);

	spin_lock_bh(&eth->lock);
	// if ((in_txq >= RPMSG_ETH_MAX_TX_BUFS) ||
	// 	(in_wtq >= eth->tx_high_wm)) {
	if (in_wtq >= eth->tx_high_wm) {
		netif_stop_queue(ndev);
		spin_unlock_bh(&eth->lock);
		dev_info(&ndev->dev, "flowctl: toff [%d %d %d %d]\n",
				eth->tx_high_wm, in_wtq, in_txq,
				skb_queue_len(&eth->rxqueue));
		return 1;
	}

	spin_unlock_bh(&eth->lock);
	return 0;
}

static void rpmsg_eth_xmit_check_start(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;
	uint32_t in_wtq = skb_queue_len(&eth->txwait_queue);
	uint32_t in_txq = skb_queue_len(&eth->txqueue);

	spin_lock_bh(&eth->lock);
	// if ((in_txq < eth->tx_high_wm) &&
	// 	(in_wtq < eth->tx_low_wm)) {
	if (in_wtq < eth->tx_low_wm) {
		if (netif_queue_stopped(ndev) && !eth->shut_period) {
			netif_start_queue(ndev);
			/* the txqueue has work todo, kick xmit work task */
			if(!work_pending(&eth->tx_work))
				queue_work(eth->tx_wq, &eth->tx_work);
			dev_info(&ndev->dev, "flowctl: ton  [%d %d %d %d]\n",
				eth->tx_low_wm, in_wtq,	in_txq,
				skb_queue_len(&eth->rxqueue));
		}
	}
	spin_unlock_bh(&eth->lock);
}

static void rpmsg_eth_flow_control(struct rpmsg_eth_device *eth, uint32_t ctrl)
{
	struct net_device *ndev = eth->dev;

	spin_lock_bh(&eth->lock);
	if (PL_FLOW_CTRL_BEGIN == ctrl) {
		//netif_stop_queue(ndev);
		eth->tx_slow_down = 1;
	} else if (PL_FLOW_CTRL_END == ctrl) {
		//netif_start_queue(ndev);
		eth->tx_slow_down = 0;
	}
	spin_unlock_bh(&eth->lock);

	dev_info_ratelimited(&ndev->dev, "flowctl: rcb%d [%d %d %d]\n",
			ctrl,
			skb_queue_len(&eth->txwait_queue),
			skb_queue_len(&eth->txqueue),
			skb_queue_len(&eth->rxqueue));
}

static int rpmsg_eth_rx_hdr(struct rpmsg_eth_device *eth, struct sk_buff *skb)
{
	struct net_device *ndev = eth->dev;
	void *data;
	int len;

	len = skb->len;
	data = skb->data;

	if (len == sizeof(struct rpmsg_eth_header)) {
		struct rpmsg_eth_header *hdr = (struct rpmsg_eth_header *)data;

		if (eth->frag == true) {
			dev_warn_ratelimited(&ndev->dev, "WARN: pkt fragment may lost, resync\n");
			eth->frag = false;
		}

		if (hdr->magic != PL_MAGIC) {
			dev_warn_ratelimited(&ndev->dev, "WARN: invalid hdr pkt dropped\n");
			return -EINVAL;
		}

		switch (hdr->type) {
		case PL_TYPE_DETECT:
			eth->version = 2;
			eth->peer_state = 1;
			dev_info_ratelimited(&ndev->dev, "recv type detect, reply ack\n");
			rpmsg_eth_ack_feature(eth);
			break;

		case PL_TYPE_DETECT_ACK:
			eth->version = 2;
			dev_info_ratelimited(&ndev->dev, "recv type detect ack\n");
			break;

		case PL_TYPE_FRAG_INDICATOR:
			eth->frag = true;
			eth->frame_len = hdr->option;
			eth->frame_received = 0;
			eth->skb_stage = rpmsg_eth_alloc_skb(eth, hdr->option);
			if (!eth->skb_stage) {
				/* skb alloc fail, will drop contiguous fragment */
				dev_err(&ndev->dev, "ERROR: alloc skbuf, drop contiguous frags\n");
				return -ENOMEM;
			}
			dev_dbg_ratelimited(&ndev->dev, "RX large frame len=%d\n", eth->frame_len);
			break;

		case PL_TYPE_KEEPALIVE:
			/* this is regular keepalive message */
			rpmsg_eth_reply_keepalive(eth);
			//dev_info(&ndev->dev, "keep alive=%d\n", hdr->len);
			break;

		case PL_TYPE_KEEPALIVE2:
			/* this is to keep channel alive, no further action */
			break;

		case PL_TYPE_SHUTDOWN:
			dev_warn(&ndev->dev, "Recieve shutdown signal=%d\n", hdr->option);
			/* reset the link and dump all tx buffers in queue */
			if (hdr->option == 1) {
				eth->shut_period |= RPMSG_SHUT_PERIOD_1;
				eth->peer_state = 0;
				netif_stop_queue(ndev);
				schedule_work(&eth->keepalive_task);
			} else {
				eth->shut_period |= RPMSG_SHUT_PERIOD_2;
				wake_up_interruptible(&eth->shut_wait);
			}
			break;

		default:
			dev_warn_ratelimited(&ndev->dev, "invalid hdr type=%d\n", hdr->type);
			break;
		}

		return 0;
	} else if (len == sizeof(struct rpmsg_eth_header_data)) {
		struct rpmsg_eth_header_data *ext_hdr = (struct rpmsg_eth_header_data *)data;
		struct rpmsg_eth_header *hdr = &ext_hdr->hdr;
		uint32_t delta = 0;
		uint32_t len;
		void *tmp;

		if (hdr->magic != PL_MAGIC) {
			dev_warn_ratelimited(&ndev->dev, "WARN: invalid ext hdr pkt dropped\n");
			return -EINVAL;
		}

		switch (hdr->type) {
		case PL_TYPE_PKT_AVAIL:
			len = hdr->option;

			/* TODO:
			 * alloc the skb from cached skb pool for rx
			 */
			skb = rpmsg_eth_alloc_skb(eth, len);
			if (unlikely(!skb)) {
				dev_err(&ndev->dev, "RX alloc skb fail pa=%llx len=%d [%d %d %d]\n",
						ext_hdr->phy_addr, len,
						skb_queue_len(&eth->txwait_queue),
						skb_queue_len(&eth->txqueue),
						skb_queue_len(&eth->rxqueue));
				rpmsg_eth_ack_err(eth, ext_hdr);
				return -ENOMEM;
			}
			delta = semidrive_get_syscntr() - ext_hdr->ts;
			tmp = memremap(ext_hdr->phy_addr, len, MEMREMAP_WB);
			__inval_dcache_area(tmp, len);
			memcpy(skb_put(skb, len), tmp, len);
			memunmap(tmp);

			eth->frame_len = len;
			rpmsg_eth_netif_rcv(eth, skb);
			rpmsg_eth_ack_ok(eth, ext_hdr);

			if (eth->shut_period)
				dev_info_ratelimited(&ndev->dev, "RX pa=%llx sack stt=%d\n",
					ext_hdr->phy_addr, delta/3);
			break;

		case PL_TYPE_FLOW_CTRL:
			rpmsg_eth_flow_control(eth, hdr->option);
			break;

		case PL_TYPE_TACK_ERR:
			rpmsg_eth_xmit_check_stop(eth);
			/* fallback for force recycle */
			dev_warn_ratelimited(&ndev->dev, "TACK ERR pa=%llx %d [%d %d %d]\n",
					ext_hdr->phy_addr, hdr->option,
					skb_queue_len(&eth->txwait_queue),
					skb_queue_len(&eth->txqueue),
					skb_queue_len(&eth->rxqueue));

		case PL_TYPE_TACK_OK:
			if (unlikely(skb_queue_empty(&eth->txwait_queue))) {
				dev_warn(&ndev->dev, "TACK pa=%llx %d no skb in waitq\n",
						ext_hdr->phy_addr, hdr->type);
				break;
			}

			skb = skb_dequeue(&eth->txwait_queue);
			if (skb) {
				if (likely((uint64_t)skb == ext_hdr->tag)) {
					delta = semidrive_get_syscntr() - ext_hdr->ts;

					dev_kfree_skb_any(skb);

					if (eth->shut_period)
						dev_info_ratelimited(&ndev->dev, "TACK pa=%llx ack rtt=%d\n",
							ext_hdr->phy_addr, delta/3);
				} else {
					struct sk_buff *next;

					dev_warn(&ndev->dev, "TACK pa=%llx skb %llx!=%llx free\n",
							ext_hdr->phy_addr, (uint64_t)skb, ext_hdr->tag);
					dev_kfree_skb_any(skb);

					skb_queue_walk_safe(&eth->txwait_queue, skb, next) {
						if ((uint64_t)skb == ext_hdr->tag) {
							skb_unlink(skb, &eth->txwait_queue);
							dev_kfree_skb_any(skb);
							dev_warn(&ndev->dev, "TACK pa=%llx ack2 rtt=%d\n",
								ext_hdr->phy_addr, delta/3);
							break;
						}
					}
				}

				/* in case throttle triggered, recover the tx pipe */
				rpmsg_eth_xmit_check_start(eth);
			}
			break;

		default:
			/* Should not be here, fallback to normal frame */
			dev_warn_ratelimited(&ndev->dev, "invalid ext_hdr type=%d\n",
								hdr->type);
			break;
		}

		return 0;
	}

	/* Not pkt header, try other cases */
	return 1;
}

static int rpmsg_eth_rx_skb(struct rpmsg_eth_device *eth, struct sk_buff *rx_skb)
{
	struct net_device *ndev = eth->dev;
	struct sk_buff *skb;
	void *data;
	int len;
	int ret = 0;

	len = rx_skb->len;
	data = rx_skb->data;

	ret = rpmsg_eth_rx_hdr(eth, rx_skb);
	if (ret <= 0)
		return ret;

	/* Receive contiguous fragments by rpmsg MTU */
	if ((eth->frag == true)) {
		if (eth->skb_stage) {
			/** in case recv len exceed frame length, drop it */
			if (eth->frame_received + len > eth->frame_len) {
				eth->frag = false;
				eth->stats.rx_errors++;
				eth->stats.rx_dropped += eth->frame_len;
				kfree_skb(eth->skb_stage);
				eth->skb_stage = NULL;
				dev_warn_ratelimited(&ndev->dev, "WARN: RX Wrong pktsize assembled=%d(=%d?), len=%d DROP\n",
						eth->frame_received, eth->frame_len, len);
				return 0;
			}

			/* Assembly fragments into one ether frame */
			if (eth->frame_received < eth->frame_len) {
				memcpy(skb_put(eth->skb_stage, len), data, len);
				eth->frame_received += len;
			}

			dev_dbg_ratelimited(&ndev->dev, "RX frame assembled=%d(=%d?), len=%d\n",
				eth->frame_received, eth->frame_len, len);
			/* last fragment to complete ether frame */
			if (eth->frame_received == eth->frame_len) {
				eth->frag = false;
				skb = eth->skb_stage;
				eth->skb_stage = NULL;
				rpmsg_eth_netif_rcv(eth, skb);
			}
			return 0;
		}

		/* skb alloc fail previously, drop contiguous fragment */
		eth->frame_received += len;
		if (eth->frame_received >= eth->frame_len)
			eth->frag = false;
		return -EINVAL;
	}

	/* Rx single frame packet, use directly for up layer, do not free */
	eth->frame_len = len;
	rpmsg_eth_netif_rcv(eth, rx_skb);

	return 1;
}

static void rpmsg_eth_recv_check_stop(struct rpmsg_eth_device *eth)
{
	struct net_device *ndev = eth->dev;
	struct rpmsg_eth_header_ctrl ctrl_hdr;
	int fullness;

	if (eth->flow_control) {
		fullness = skb_queue_len(&eth->rxqueue);
		if (fullness < eth->rx_low_wm) {
			dev_info(&ndev->dev, "flowctl: end   [%d %d %d] <lwm=%d\n",
				skb_queue_len(&eth->txwait_queue),
				skb_queue_len(&eth->txqueue),
				fullness, eth->rx_low_wm);

			eth->flow_control = 0;
			rpmsg_eth_flow_ctrl(eth, &ctrl_hdr, PL_FLOW_CTRL_END);
		}
	}
}

static void rpmsg_eth_recv_check_start(struct rpmsg_eth_device *eth, int hdr_type)
{
	struct net_device *ndev = eth->dev;
	struct rpmsg_eth_header_ctrl ctrl_hdr;
	int fullness = skb_queue_len(&eth->rxqueue);

	if (fullness > eth->rx_high_wm) {
		if (eth->flow_control == 0) {
			dev_info(&ndev->dev, "flowctl: begin [%d %d %d] >hwm=%d hdr=%d\n",
					skb_queue_len(&eth->txwait_queue),
					skb_queue_len(&eth->txqueue),
					fullness, eth->rx_high_wm, hdr_type);
			eth->flow_control = 1;
			rpmsg_eth_flow_ctrl(eth, &ctrl_hdr, PL_FLOW_CTRL_BEGIN);
		}
	}
}

static void rpmsg_ept_rx_work_task(struct work_struct *work)
{
	struct rpmsg_eth_device *eth = container_of(work, struct rpmsg_eth_device, rx_work);
	struct sk_buff *skb = NULL;

	for(;;) {
		/* check for data in the rxqueue */
		skb = skb_dequeue(&eth->rxqueue);
		if (unlikely(!skb)) {
			break;
		}

		/* return 1 in case that skb input to up layer */
		if (rpmsg_eth_rx_skb(eth, skb) <= 0)
			dev_kfree_skb_any(skb);

		rpmsg_eth_recv_check_stop(eth);
	}
}

/* check if the data is header message */
static int rpmsg_eth_get_hdr_type(void *data, int len)
{
	struct rpmsg_eth_header *hdr = NULL;

	if (len == sizeof(struct rpmsg_eth_header)) {
		hdr = (struct rpmsg_eth_header *)data;
	} else if (len == sizeof(struct rpmsg_eth_header_data)) {
		struct rpmsg_eth_header_data *ext_hdr = (struct rpmsg_eth_header_data *)data;
		hdr = &ext_hdr->hdr;
	}
	if (hdr && (hdr->magic == PL_MAGIC)) {
		return hdr->type;
	}

	return 0;
}

static int rpmsg_ept_rx_cb(struct rpmsg_device *rpdev, void *data,
                                        int len, void *priv, u32 src)
{
	struct rpmsg_eth_device *eth = priv;
	struct net_device *ndev = eth->dev;
	struct sk_buff *skb;
	void *tmp;
	int hdr_type = rpmsg_eth_get_hdr_type(data, len);

	eth->last_update_time = jiffies;
	eth->link_check_timeout = 0;

	rpmsg_eth_recv_check_start(eth, hdr_type);

	skb = netdev_alloc_skb_ip_align(eth->dev, len);
	if (unlikely(!skb)) {
		dev_warn_ratelimited(&ndev->dev, "ERROR: rx no skb qlen=%d DROP hdr:%d %d\n",
				skb_queue_len(&eth->rxqueue), hdr_type, len);

		goto recv_fail;
	}

	tmp = skb_put(skb, len);
	memcpy(tmp, data, len);
	skb_queue_tail(&eth->rxqueue, skb);

	if(!work_pending(&eth->rx_work))
		queue_work(eth->rx_wq, &eth->rx_work);

	return 0;

recv_fail:
	eth->stats.rx_over_errors++;
	eth->stats.rx_dropped += len;

	return -ENOMEM;
}

static void eth_xmit_work_task(struct work_struct *work)
{
	struct rpmsg_eth_device *eth = container_of(work, struct rpmsg_eth_device,
	                    tx_work);
	struct net_device *ndev = eth->dev;
	struct sk_buff *skb;
	int len = 0, send_bytes, off;
	int err = 0;
	int fail_cnt;
	const int max_fail_cnt = 15;
	unsigned long timeout;

	for (;;) {

		/* too many ptk to xmit in queue, skip once */
		if (skb_queue_len(&eth->txwait_queue) > eth->tx_high_wm)
			break;

		if (eth->tx_slow_down == 1) {
			break;
		}

		/* check for data in the queue */
		skb = skb_dequeue(&eth->txqueue);
		if (unlikely(!skb)) {
			break;
		}

		if (eth->detect == -1) {
			rpmsg_eth_annouce_feature(eth);
			eth->detect = 0;
		}

		if (unlikely(eth->flush_pending)) {
			dev_info_ratelimited(&ndev->dev,
					"pkt discarded, TX len:%d pa=%llx %d tail:%#lx end:%#lx q=%d+%d\n",
					skb->len, (uint64_t)virt_to_phys(skb->data), skb->truesize,
					(unsigned long)skb->tail, (unsigned long)skb->end,
					skb_queue_len(&eth->txwait_queue),
					skb_queue_len(&eth->txqueue));
			dev_kfree_skb_any(skb);
			continue;
		}

		len = skb->len;
		if (len > eth->rpmsg_mtu) {
			if (eth->version == 2) {
				struct rpmsg_eth_header_data frhdr;

				frhdr.hdr.type  = PL_TYPE_PKT_AVAIL;
				frhdr.hdr.magic = PL_MAGIC;
				frhdr.hdr.option   = len;
				frhdr.phy_addr  = (uint64_t)virt_to_phys(skb->data);
				frhdr.tag       = (uint64_t)skb;
				frhdr.ts        = semidrive_get_syscntr();

				__flush_dcache_area(skb->data, len);

				/* add into waiting list for tx acknowledge before tx */
				skb_queue_tail(&eth->txwait_queue, skb);

				fail_cnt = 0;
				while (fail_cnt++ < max_fail_cnt) {
					err = rpmsg_trysendto(eth->ether_ept,
							&frhdr,
							sizeof(frhdr),
							eth->chinfo.dst);

					if (err != -ENOMEM)
						break;

					/* wake up every 1s (total 15s) to try if no txbuf */
					if (wait_for_completion_timeout(
								&eth->shut_com,
								msecs_to_jiffies(1000)) > 0) {
						/* interrupted by other thread, stop and exit */
						break;
					}
				}
				if (unlikely(err < 0)) {
					dev_err(&ndev->dev, "ERROR: %d rpmsg_send 1 skb=%p\n", err, skb);
					skb_unlink(skb, &eth->txwait_queue);
					dev_kfree_skb_any(skb);
					continue;
				}

				if (eth->shut_period)
					dev_info_ratelimited(&ndev->dev, "TX len:%d pa=%llx %d tail:%#lx end:%#lx q=%d+%d\n",
						skb->len, frhdr.phy_addr, skb->truesize,
						(unsigned long)skb->tail, (unsigned long)skb->end,
						skb_queue_len(&eth->txwait_queue),
						skb_queue_len(&eth->txqueue));

			} else {
				struct rpmsg_eth_header frhdr;

				frhdr.type  = PL_TYPE_FRAG_INDICATOR;
				frhdr.magic = PL_MAGIC;
				frhdr.option = len;

				fail_cnt = 0;
				while (fail_cnt++ < max_fail_cnt) {
					err = rpmsg_trysendto(eth->ether_ept,
							&frhdr,
							sizeof(frhdr),
							eth->chinfo.dst);

					if (err != -ENOMEM)
						break;

					/* wake up every 1s (total 15s) to try if no txbuf */
					if (wait_for_completion_timeout(
								&eth->shut_com,
								msecs_to_jiffies(1000)) > 0) {
						/* interrupted by other thread, stop and exit */
						break;
					}
				}
				if (unlikely(err < 0)) {
					dev_err(&ndev->dev, "ERROR: %d rpmsg_send 2\n", err);
					dev_kfree_skb_any(skb);
					continue;
				}

				off = 0;
				while (off < len) {
					send_bytes = min(len - off, eth->rpmsg_mtu);

					fail_cnt = 0;
					while (fail_cnt++ < max_fail_cnt) {
						err = rpmsg_trysendto(eth->ether_ept,
								skb->data + off,
								send_bytes,
								eth->chinfo.dst);

						if (err != -ENOMEM)
							break;

						/* wake up every 1s (total 15s) to try if no txbuf */
						if (wait_for_completion_timeout(
									&eth->shut_com,
									msecs_to_jiffies(1000)) > 0) {
							/* interrupted by other thread, stop and exit */
							break;
						}
					}
					if (unlikely(err < 0)) {
						dev_err(&ndev->dev, "ERROR: rc=%d cc=%d len=%d\n", err, off, send_bytes);
						break;
					}
					off += send_bytes;
					dev_dbg_ratelimited(&ndev->dev, "TX cc=%d, send=%d left=%d\n", off, send_bytes, len - off);
				}
				dev_kfree_skb_any(skb);
			}
		} else {
			fail_cnt = 0;
			while (fail_cnt++ < max_fail_cnt) {
				err = rpmsg_trysendto(eth->ether_ept,
						skb->data,
						len,
						eth->chinfo.dst);

				if (err != -ENOMEM)
					break;

				/* wake up every 1s (total 15s) to try if no txbuf */
				if (wait_for_completion_timeout(
							&eth->shut_com,
							msecs_to_jiffies(1000)) > 0) {
					/* interrupted by other thread, stop and exit */
					break;
				}
			}
			if (unlikely(err < 0))
				dev_err(&ndev->dev, "ERROR: rpmsg_send %d len=%d 3\n", err, len);
			dev_kfree_skb_any(skb);
		}

		if (likely(!err)) {
			eth->stats.tx_packets++;
			eth->stats.tx_bytes += len;
			dev_dbg_ratelimited(&ndev->dev, "TX success len=%d\n", len);
		}

		rpmsg_eth_xmit_check_start(eth);
	}
}

static struct device_type rpmsg_eth_type = {
	.name = "rpmsg",
};
static void rpmsg_netdev_setup(struct net_device *ndev)
{
	SET_NETDEV_DEVTYPE(ndev, &rpmsg_eth_type);
	ndev->header_ops	= &eth_header_ops;
	ndev->type		= ARPHRD_ETHER;
	ndev->hard_header_len 	= ETH_HLEN;
	ndev->min_header_len	= ETH_HLEN;
	ndev->mtu			= ETH_DATA_LEN; /* override later */
	ndev->min_mtu		= ETH_MIN_MTU;
	ndev->max_mtu		= MAX_DEV_MTU;
	ndev->addr_len		= ETH_ALEN;
	ndev->tx_queue_len	= RPMSG_ETH_MAX_TX_BUFS;
	ndev->flags		= IFF_BROADCAST|IFF_MULTICAST;
	ndev->priv_flags		|= IFF_TX_SKB_SHARING;

	eth_broadcast_addr(ndev->broadcast);
}

int rpmsg_net_create_etherdev(struct rpmsg_net_device *rndev)
{
	struct device *dev = &rndev->rpmsg_chnl->dev;
	struct rpmsg_eth_device *eth;
	struct net_device *ndev;
	int ret = -ENOMEM;

	ndev = alloc_netdev(sizeof(struct rpmsg_eth_device),
				"eth%d", NET_NAME_UNKNOWN, rpmsg_netdev_setup);
	if (ndev == NULL) {
		dev_err(dev, "ERROR: alloc_netdev\n");
		return ret;
	}

	ndev->ethtool_ops = &rpmsg_ethtool_ops;
	ndev->netdev_ops  = &rpmsg_eth_ops;
	SET_NETDEV_DEV(ndev, dev);

	eth = netdev_priv(ndev);
	memset(eth, 0, sizeof(*eth));

	eth->dev = ndev;
	eth_hw_addr_random(ndev);

	eth->rpmsg_chnl = rndev->rpmsg_chnl;
	eth->chinfo.src = RPMSG_ETHERNET_EPT;
	eth->chinfo.dst = RPMSG_ETHERNET_EPT;
	snprintf(eth->chinfo.name, RPMSG_NAME_SIZE, "rpmsg-net-%d", eth->chinfo.src);
	rndev->ether_dev = ndev;
	spin_lock_init(&eth->lock);
	skb_queue_head_init(&eth->txqueue);
	skb_queue_head_init(&eth->txwait_queue);

	INIT_WORK(&eth->tx_work, eth_xmit_work_task);
	eth->tx_wq = create_workqueue("rpmsg_net_tx_wq");
	if (eth->tx_wq < 0) {
		dev_err(dev, "ERROR: rpmsg_net_tx_wq\n");
		goto out;
	}

	skb_queue_head_init(&eth->rxqueue);
	INIT_WORK(&eth->rx_work, rpmsg_ept_rx_work_task);
	eth->rx_wq = create_workqueue("rpmsg_net_rx_wq");
	if (eth->rx_wq < 0) {
		dev_err(dev, "ERROR: rpmsg_net_rx_wq\n");
		goto out;
	}

	setup_timer(&eth->keepalive_timer, rpmsg_eth_keepalive,
			(unsigned long) eth);
	INIT_WORK(&eth->keepalive_task, rpmsg_eth_keepalive_task);
	init_waitqueue_head(&eth->shut_wait);
	init_completion(&eth->shut_com);

	eth->ether_ept = rpmsg_create_ept(eth->rpmsg_chnl, rpmsg_ept_rx_cb,
					eth, eth->chinfo);
	if (!eth->ether_ept) {
		dev_err(dev, "ERROR: rpmsg_create_ept\n");
		goto out;
	}
	eth->rpmsg_mtu = rpmsg_get_mtu(eth->ether_ept);
	ndev->mtu = DEFAULT_DEV_MTU; /* TODO: read from dts */
	eth->frag = false;
	eth->version = 1;
	eth->detect = -1;

	/* initial tx wait throttle with whole queue length */
	eth->tx_high_wm = ndev->tx_queue_len * 3 / 4;
	eth->tx_low_wm  = ndev->tx_queue_len / 8;

	eth->rx_high_wm = RPMSG_ETH_MAX_RX_BUFS * 3 / 4;
	eth->rx_low_wm  = RPMSG_ETH_MAX_RX_BUFS / 8;
	eth->flow_control = 0;
	ret = register_netdev(ndev);
	if (ret < 0 ) {
		dev_err(dev, "ERROR: register netdev\n");
		goto out;
	}

	dev_info(&ndev->dev, "created mtu(%d,%d)\n", eth->rpmsg_mtu, ndev->mtu);

	return ret;

out:
	if(ndev) {
		free_netdev(ndev);
	}

	dev_info(dev, "ERROR: Failed!\n");
	return ret;
}

static int rpmsg_net_device_cb(struct rpmsg_device *rpdev, void *data,
        int len, void *priv, u32 src)
{
	dev_info(&rpdev->dev, "%s: called\n",  __func__);
	return 0;
}

static void rpmsg_net_drv_shutdown(struct device *dev)
{
	struct rpmsg_device *rpdev = container_of(dev, struct rpmsg_device, dev);
	struct rpmsg_net_device *rndev;
	struct net_device *ndev;
	struct rpmsg_eth_device *eth;

	rndev = dev_get_drvdata(&rpdev->dev);
	ndev = rndev->ether_dev;
	eth = netdev_priv(ndev);

	/* indicate the remote side to stop sending */
	if (eth->peer_state)
		rpmsg_eth_send_shutdown(eth, 1);
	/* indicate not receive any pkt */
	if (netif_running(ndev))
		rpmsg_eth_down(eth);

	netif_stop_queue(ndev);
	dev_info(&ndev->dev, "flushing IO wq\n");
	__flush_work_task(eth);

	dev_info(&ndev->dev, "send going to shutdown\n");
	if (eth->peer_state)
		rpmsg_eth_send_shutdown(eth, 2);

	dev_info(&ndev->dev, "shutdown\n");
}

static int rpmsg_net_device_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_net_device *rndev;

	rndev = devm_kzalloc(&rpdev->dev, sizeof(struct rpmsg_net_device),
	                     GFP_KERNEL);
	if (!rndev) {
		dev_err(&rpdev->dev, "Failed to allocate memory for rpmsg netdev\n");
		return -ENOMEM;
	}

	dev_info(&rpdev->dev, "new channel!\n");

	memset(rndev, 0x0, sizeof(struct rpmsg_net_device));
	rndev->rpmsg_chnl = rpdev;

	dev_set_drvdata(&rpdev->dev, rndev);

	if (rpmsg_net_create_etherdev(rndev)) {
		goto error2;
	}

	return 0;

error2:
	return -ENODEV;
}

static void rpmsg_net_device_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_net_device *rndev = dev_get_drvdata(&rpdev->dev);
	struct rpmsg_eth_device *eth;

	if (rndev && rndev->ether_dev) {
		unregister_netdev(rndev->ether_dev);
		eth = netdev_priv(rndev->ether_dev);
		if (eth && eth->ether_ept)
			rpmsg_destroy_ept(eth->ether_ept);

		del_timer_sync(&eth->keepalive_timer);
		cancel_work_sync(&eth->keepalive_task);
		free_netdev(rndev->ether_dev);
		rndev->ether_dev = NULL;
	}
}

static struct rpmsg_device_id rpmsg_net_driver_id_table[] =
{
	{ .name = RPMSG_ETHERNET_NAME },
	{},
};

static struct rpmsg_driver rpmsg_net_driver =
{
	.drv.name = "rpmsg_netdrv",
	.drv.owner = THIS_MODULE,
	.drv.shutdown = rpmsg_net_drv_shutdown,
	.id_table = rpmsg_net_driver_id_table,
	.probe = rpmsg_net_device_probe,
	.remove = rpmsg_net_device_remove,
	.callback = rpmsg_net_device_cb,
};

static int __init rpmsg_net_init(void)
{
	int err = 0;

	err = register_rpmsg_driver(&rpmsg_net_driver);
	if (err != 0) {
		pr_err("%s: Failed, rc=%d\n", __func__, err);
	}

	return err;
}

static void __exit rpmsg_net_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_net_driver);
}

module_init(rpmsg_net_init);
module_exit(rpmsg_net_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("RPMsg-based Network Device Driver");
MODULE_LICENSE("GPL v2");

