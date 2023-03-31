/*
 * Copyright (C) 2021 Semidrive Semiconductor
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
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>

/* net device info */
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h> /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h> /* struct iphdr */
#include <linux/tcp.h> /* struct tcphdr */
#include <linux/skbuff.h>
#include <linux/in6.h>
#include <asm/checksum.h>
#include <net/arp.h>
#include <linux/if_ether.h>
#include <net/pkt_sched.h>
#include <linux/soc/semidrive/ulink.h>

#define UC_PACK_SIZE 0x100
#define ULINK_ETH_MAGIC 0x5a5a
#define ULINK_RET_MAGIC 0xa5a5
#define ULINK_IP_OFFSET 32

#define ULINK_MTU (65500)
#define GARBAGE_TIME (HZ * 60)

/* if_arp.h do not match protocal all*/
struct eth_arphdr {
	struct arphdr arp;
	/*     Ethernet looks like this : This bit is variable sized however... */
	unsigned char ar_sha[ETH_ALEN]; /* sender hardware address      */
	__be32 ar_sip; /* sender IP address            */
	unsigned char ar_tha[ETH_ALEN]; /* target hardware address      */
	__be32 ar_tip; /* target IP address            */
} __attribute__((__packed__));

struct unfree_skb_node {
	struct list_head list;
	uint64_t skb;
	int32_t ref_count;
	uint32_t gc_count;
};

struct ulink_eth_channel {
	struct net_device *ndev;
	uint32_t rproc;
	struct ulink_channel *ulink_chnl;
	struct dma_chan *dma_chan;
	struct completion done;

	uint32_t frame;
	uint32_t received;
	struct sk_buff *skb;
	bool init_done;
};

struct ulink_eth_device {
	struct net_device_stats stats;
	struct net_device *ndev;

	uint32_t probe_chs;
	uint32_t default_rproc;
	struct ulink_eth_channel chs[ALL_DP_CPU_MAX];
	struct sk_buff_head txqueue;
	struct work_struct tx_work;
	spinlock_t lock;
	struct timer_list gc_timer;
	struct list_head unfree_skb;
	spinlock_t free_skb_lock;
};

struct ulink_eth_header {
	uint16_t magic;
	uint16_t tot_len;
	uint16_t sec_id;
	uint16_t sec_len;
	uint64_t data_pa;
	uint64_t skb_va;
} __attribute__((__packed__));

static ssize_t devtype_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "unilink\n");
}
static DEVICE_ATTR_RO(devtype);

static uint32_t get_dest_ch(struct sk_buff *skb, struct ulink_eth_device *eth)
{
	struct net_device *ndev = eth->ndev;
	uint32_t daddr;
	struct in_ifaddr *ifaddr;
	uint32_t local;
	uint8_t addr1, addr2;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		daddr = be32_to_cpu(ip_hdr(skb)->daddr);
		break;
	case htons(ETH_P_ARP):
		daddr = be32_to_cpu(
			((struct eth_arphdr *)(arp_hdr(skb)))->ar_tip);
		break;
	default:
		return eth->default_rproc;
	}

	if ((daddr & 0xf0000000) == 0xe0000000)
		return ALL_DP_CPU_MAX;

	rtnl_lock();
	ifaddr = __in_dev_get_rtnl(ndev)->ifa_list;
	while (ifaddr) {
		local = be32_to_cpu(ifaddr->ifa_local);
		if ((daddr & 0xffffff00) == (local & 0xffffff00)) {
			rtnl_unlock();
			addr1 = local & 0xff;
			addr2 = daddr & 0xff;
			if ((addr1 < ULINK_IP_OFFSET) ||
			    (addr2 < ULINK_IP_OFFSET))
				return eth->default_rproc;
			addr1 -= ULINK_IP_OFFSET;
			addr2 -= ULINK_IP_OFFSET;
			if (addr1 < DP_CPU_MAX)
				return addr2;
			else if (addr2 < DP_CPU_MAX)
				return addr2 + DP_CPU_MAX;
			else
				return addr2 - DP_CPU_MAX;
		}
		ifaddr = ifaddr->ifa_next;
	}

	rtnl_unlock();
	return eth->default_rproc;
}

static struct unfree_skb_node *add_unfree_skb(uint64_t skb,
					      struct ulink_eth_device *eth)
{
	struct unfree_skb_node *node;
	unsigned long flags;

	node = kmalloc(sizeof(struct unfree_skb_node), GFP_KERNEL);
	if (!node)
		return NULL;

	node->skb = skb;
	node->gc_count = 0;
	node->ref_count = 0;
	spin_lock_irqsave(&eth->free_skb_lock, flags);
	list_add(&node->list, &eth->unfree_skb);
	spin_unlock_irqrestore(&eth->free_skb_lock, flags);

	return node;
}

static void ref_unfree_skb(struct unfree_skb_node *node,
			   struct ulink_eth_device *eth, int32_t ref)
{
	unsigned long flags;

	spin_lock_irqsave(&eth->free_skb_lock, flags);
	node->ref_count += ref;
	spin_unlock_irqrestore(&eth->free_skb_lock, flags);
}

static void check_and_free_skb(uint64_t skb, struct ulink_eth_device *eth)
{
	struct unfree_skb_node *node;
	struct unfree_skb_node *tmp;
	unsigned long flags;

	spin_lock_irqsave(&eth->free_skb_lock, flags);
	list_for_each_entry_safe (node, tmp, &eth->unfree_skb, list) {
		if (node->skb == skb) {
			node->ref_count--;
			if (node->ref_count > 0)
				continue;
			list_del(&node->list);
			spin_unlock_irqrestore(&eth->free_skb_lock, flags);
			dev_kfree_skb_any((struct sk_buff *)(skb));
			kfree(node);
			return;
		}
	}
	spin_unlock_irqrestore(&eth->free_skb_lock, flags);
}

static void gc_skb(unsigned long data)
{
	struct ulink_eth_device *eth = (struct ulink_eth_device *)data;
	struct list_head head;
	struct unfree_skb_node *node;
	struct unfree_skb_node *tmp;
	unsigned long flags;

	INIT_LIST_HEAD(&head);
	spin_lock_irqsave(&eth->free_skb_lock, flags);
	list_for_each_entry_safe (node, tmp, &eth->unfree_skb, list) {
		node->gc_count++;
		if (node->gc_count > 1) {
			list_del(&node->list);
			list_add(&node->list, &head);
		}
	}
	spin_unlock_irqrestore(&eth->free_skb_lock, flags);

	list_for_each_entry_safe (node, tmp, &head, list) {
		list_del(&node->list);
		dev_kfree_skb_any((struct sk_buff *)(node->skb));
		kfree(node);
	}

	mod_timer(&eth->gc_timer, jiffies + GARBAGE_TIME);
}

static netdev_tx_t ulink_eth_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ulink_eth_device *eth = netdev_priv(ndev);

	spin_lock_bh(&eth->lock);
	skb_queue_tail(&eth->txqueue, skb);
	spin_unlock_bh(&eth->lock);
	schedule_work(&eth->tx_work);
	return NETDEV_TX_OK;
}

static int ulink_eth_open(struct net_device *ndev)
{
	netif_start_queue(ndev);

	return 0;
}

int ulink_eth_stop(struct net_device *ndev)
{
	pr_info("stop called\n");
	netif_stop_queue(ndev);
	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int ulink_eth_config(struct net_device *ndev, struct ifmap *map)
{
	if (ndev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* ignore other fields */
	return 0;
}

static int ulink_eth_set_mac_address(struct net_device *ndev, void *addr)
{
	int ret = 0;

	ret = eth_mac_addr(ndev, addr);

	return ret;
}

struct net_device_stats *ulink_eth_stats(struct net_device *ndev)
{
	struct ulink_eth_device *priv = netdev_priv(ndev);
	return &priv->stats;
}

static u32 always_on(struct net_device *ndev)
{
	return 1;
}

static const struct ethtool_ops ulink_ethtool_ops = {
	.get_link = always_on,
};

static const struct net_device_ops ulink_eth_ops = {
	.ndo_open = ulink_eth_open,
	.ndo_stop = ulink_eth_stop,
	.ndo_start_xmit = ulink_eth_xmit,
	.ndo_set_config = ulink_eth_config,
	.ndo_set_mac_address = ulink_eth_set_mac_address,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_get_stats = ulink_eth_stats,
};

static void eth_xmit_work_handler(struct work_struct *work)
{
	struct ulink_eth_device *eth =
		container_of(work, struct ulink_eth_device, tx_work);
	uint32_t dest;
	struct ulink_eth_channel *uch;
	int len;
	struct sk_buff *skb;
	struct ulink_eth_header header;
	uint8_t *tmp;
	struct unfree_skb_node *node;
	int i;

	for (;;) {
		spin_lock_bh(&eth->lock);
		/* check for data in the queue */
		if (skb_queue_empty(&eth->txqueue)) {
			spin_unlock_bh(&eth->lock);
			break;
		}

		skb = skb_dequeue(&eth->txqueue);
		if (!skb) {
			spin_unlock_bh(&eth->lock);
			break;
		}

		len = skb->len;
		eth->stats.tx_packets++;
		eth->stats.tx_bytes += len;
		spin_unlock_bh(&eth->lock);

		dest = get_dest_ch(skb, eth);
		if (unlikely(dest > ALL_DP_CPU_MAX)) {
			dev_kfree_skb_any(skb);
			continue;
		}

		if (dest != ALL_DP_CPU_MAX) {
			uch = &eth->chs[dest];
			if (!uch->init_done) {
				dev_kfree_skb_any(skb);
				continue;
			}
		}

		tmp = skb->data;

		if (len > UC_PACK_SIZE) {
			header.magic = ULINK_ETH_MAGIC;
			header.tot_len = len;
			header.sec_len = len;
			header.sec_id = 0;
			header.data_pa = __pa(tmp);
			header.skb_va = (uint64_t)skb;
			node = add_unfree_skb(header.skb_va, eth);
			if (node == NULL) {
				dev_kfree_skb_any(skb);
			} else {
				__flush_dcache_area(tmp, len);
				if (dest != ALL_DP_CPU_MAX) {
					ref_unfree_skb(node, eth, 1);
					ulink_channel_send(
						uch->ulink_chnl,
						(uint8_t *)&header,
						sizeof(struct ulink_eth_header));
				} else {
					ref_unfree_skb(node, eth,
						       eth->probe_chs);
					for (i = 0; i < ALL_DP_CPU_MAX; i++) {
						uch = &eth->chs[i];
						if (uch->init_done)
							ulink_channel_send(
								uch->ulink_chnl,
								(uint8_t *)&header,
								sizeof(struct ulink_eth_header));
					}
				}
			}
		} else {
			if (dest != ALL_DP_CPU_MAX) {
				ulink_channel_send(uch->ulink_chnl, tmp, len);
			} else {
				for (i = 0; i < ALL_DP_CPU_MAX; i++) {
					uch = &eth->chs[i];
					if (uch->init_done)
						ulink_channel_send(
							uch->ulink_chnl, tmp,
							len);
				}
			}
			dev_kfree_skb_any(skb);
		}
	}
}

static struct device_type ulink_eth_type = {
	.name = "ulink",
};

static void ulink_netdev_setup(struct net_device *ndev)
{
	SET_NETDEV_DEVTYPE(ndev, &ulink_eth_type);
	ndev->header_ops = &eth_header_ops;
	ndev->type = ARPHRD_ETHER;
	ndev->hard_header_len = ETH_HLEN;
	ndev->min_header_len = ETH_HLEN;
	ndev->mtu = ULINK_MTU;
	ndev->min_mtu = ETH_MIN_MTU;
	ndev->max_mtu = ULINK_MTU;
	ndev->addr_len = ETH_ALEN;
	ndev->tx_queue_len = DEFAULT_TX_QUEUE_LEN;
	ndev->flags = IFF_BROADCAST | IFF_MULTICAST;
	ndev->priv_flags |= IFF_TX_SKB_SHARING;

	eth_broadcast_addr(ndev->broadcast);
}

static void dmatrans_callback(void *data)
{
	struct ulink_eth_channel *uch = data;

	complete(&uch->done);
}

static void cpu_copy(phys_addr_t dst, phys_addr_t src, uint32_t size)
{
	void *tmpsrc;
	void *tmpdst;
	tmpsrc = memremap(src, size, MEMREMAP_WB);
	tmpdst = memremap(dst, size, MEMREMAP_WB);

	__inval_dcache_area(tmpsrc, size);
	memcpy(tmpdst, tmpsrc, size);
	__flush_dcache_area(tmpdst, size);
	memunmap(tmpsrc);
	memunmap(tmpdst);
}

static void dma_copy(struct ulink_eth_channel *uch, phys_addr_t dst,
		     phys_addr_t src, uint32_t size)
{
	dma_cookie_t cookie;
	enum dma_ctrl_flags flags;
	struct dma_async_tx_descriptor *tx;
	struct dma_device *dev = uch->dma_chan->device;
	uint64_t align = cache_line_size();
	size_t size_head, size_body, size_tail;

	if (unlikely(size < (align * 4))) {
		cpu_copy(dst, src, size);
		return;
	}

	if (likely((dst & (align - 1)) == (src & (align - 1)))) {
		size_head = round_up(dst, align) - dst;
		size_body = size - size_head;
		size_tail = size_body & (align - 1);
		size_body = size_body - size_tail;
	} else {
		size_head = 0;
		size_body = size;
		size_tail = 0;
	}

	flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	tx = dev->device_prep_dma_memcpy(uch->dma_chan, dst + size_head,
					 src + size_head, size_body, flags);
	tx->callback = dmatrans_callback;
	tx->callback_param = uch;

	cookie = tx->tx_submit(tx);
	if (dma_submit_error(cookie)) {
		//replace pr_err with devie err code later
		pr_err("dma copy failed\n");
		cpu_copy(dst, src, size);
		return;
	}
	dma_async_issue_pending(uch->dma_chan);
	//copy by cpu when wait for dma
	if (size_head > 0)
		cpu_copy(dst, src, size_head);
	if (size_tail > 0)
		cpu_copy(dst + size_head + size_body,
			 src + size_head + size_body, size_tail);
	if (wait_for_completion_timeout(&uch->done, 1000) == 0) {
		//replace pr_err with devie err code later
		pr_err("dma copy timeout\n");
		dmaengine_terminate_async(uch->dma_chan);
		cpu_copy(dst + size_head, src + size_head, size_body);
	}
}

static struct sk_buff *ulink_uch_alloc_skb(struct ulink_eth_channel *uch,
					   uint32_t offset)
{
	struct net_device *ndev = uch->ndev;
	struct sk_buff *skb;
	struct ulink_eth_device *eth = netdev_priv(ndev);
	uint64_t align = cache_line_size();
	uint64_t reserve;

	if (unlikely(offset > align))
		offset = offset & (align - 1);

	skb = netdev_alloc_skb(ndev, uch->frame + 2 * align);
	if (!skb) {
		/* skb alloc fail, will drop contiguous fragment */
		eth->stats.rx_errors++;
		eth->stats.rx_dropped += uch->frame;
		return NULL;
	}

	reserve = offset + round_up((uint64_t)(skb->data), align) -
		  (uint64_t)(skb->data);
	skb_reserve(skb, reserve);
	return skb;
}

static void ulink_eth_rx(struct ulink_channel *ch, uint8_t *data, uint32_t size)
{
	struct ulink_eth_channel *uch = ch->user_data;
	struct net_device *ndev = uch->ndev;
	struct ulink_eth_device *eth = netdev_priv(ndev);
	struct ulink_eth_header *header = (struct ulink_eth_header *)data;
	struct ulink_eth_header ret_header;
	uint64_t align = cache_line_size();
	uint16_t magic;
	uint32_t rev_size;
	uint8_t *tmp;

	if (size == sizeof(struct ulink_eth_header))
		magic = header->magic;
	else
		magic = 0;

	switch (magic) {
	case ULINK_ETH_MAGIC:
		if (header->sec_id == 0) {
			uch->frame = header->tot_len;
			uch->received = 0;
			uch->skb = ulink_uch_alloc_skb(
				uch, (header->data_pa) & (align - 1));
		}

		rev_size = header->sec_len;
		if (!uch->skb) {
			uch->received += rev_size;
			if (uch->frame == uch->received)
				uch->frame = 0;
			return;
		}
		tmp = skb_put(uch->skb, rev_size);
		__inval_dcache_area(tmp, rev_size);
		if (uch->rproc < DP_CPU_MAX) {
			dma_copy(uch, __pa(tmp), header->data_pa, rev_size);
		} else {
			kunlun_pcie_dma_copy(__pa(tmp), header->data_pa,
					     rev_size, DIR_PULL);
		}
		uch->received += rev_size;

		if (uch->received == uch->frame) {
			ret_header.magic = ULINK_RET_MAGIC;
			ret_header.skb_va = header->skb_va;
			ulink_channel_send(uch->ulink_chnl,
					   (uint8_t *)&ret_header,
					   sizeof(ret_header));
		}
		break;
	case ULINK_RET_MAGIC:
		check_and_free_skb(header->skb_va, eth);
		return;
	default:
		rev_size = size;
		uch->frame = rev_size;
		uch->received = 0;
		uch->skb = ulink_uch_alloc_skb(uch,
					       ((uint64_t)data) & (align - 1));
		if (!uch->skb) {
			uch->received += rev_size;
			if (uch->frame == uch->received)
				uch->frame = 0;
			return;
		}
		memcpy(skb_put(uch->skb, rev_size), data, rev_size);
		uch->received += rev_size;
		break;
	}

	if (uch->received == uch->frame) {
		uch->skb->protocol = eth_type_trans(uch->skb, ndev);
		uch->skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
		eth->stats.rx_packets++;
		eth->stats.rx_bytes += uch->frame;
		netif_rx(uch->skb);
		uch->frame = 0;
		uch->skb = NULL;
	}
}

static int ulink_net_probe_chs(struct ulink_eth_device *eth, uint32_t rproc)
{
	struct ulink_eth_channel *uch = &eth->chs[rproc];
	dma_cap_mask_t mask;

	uch->rproc = rproc;

	if (rproc < DP_CPU_MAX) {
		dma_cap_zero(mask);
		dma_cap_set(DMA_MEMCPY, mask);
		uch->dma_chan = dma_request_channel(mask, NULL, NULL);
		if (!uch->dma_chan)
			return -ENOMEM;
		init_completion(&uch->done);
	}

	uch->ulink_chnl = ulink_request_channel(uch->rproc, 0xE, ulink_eth_rx);
	if (!uch->ulink_chnl)
		return -ENOMEM;

	uch->ulink_chnl->user_data = uch;

	uch->frame = 0;
	uch->ndev = eth->ndev;
	eth->probe_chs++;
	uch->init_done = true;
	return 0;
}

static void ulink_net_remove_chs(struct ulink_eth_device *eth)
{
	struct ulink_eth_channel *uch;
	int i;

	for (i = 0; i < ALL_DP_CPU_MAX; i++) {
		uch = &eth->chs[i];
		if ((uch->init_done) && (uch->ulink_chnl))
			ulink_release_channel(uch->ulink_chnl);
		if ((uch->init_done) && (uch->dma_chan))
			dma_release_channel(uch->dma_chan);
	}
}

static int ulink_net_probe(struct platform_device *pdev)
{
	struct ulink_eth_device *eth;
	struct net_device *ndev;
	struct sockaddr hwaddr;
	const u8 *addr;
	int len;
	int ret = -ENOMEM;

	ndev = alloc_netdev(sizeof(struct ulink_eth_device), "eth%d",
			    NET_NAME_UNKNOWN, ulink_netdev_setup);
	if (ndev == NULL) {
		return ret;
	}

	ndev->ethtool_ops = &ulink_ethtool_ops;
	ndev->netdev_ops = &ulink_eth_ops;
	SET_NETDEV_DEV(ndev, &pdev->dev);

	eth = netdev_priv(ndev);
	memset(eth, 0, sizeof(*eth));

	eth->ndev = ndev;
	eth_hw_addr_random(ndev);
	INIT_LIST_HEAD(&eth->unfree_skb);
	spin_lock_init(&eth->free_skb_lock);

	if (of_property_read_u32(pdev->dev.of_node, "default_rproc",
				 &eth->default_rproc))
		eth->default_rproc = R_DP_CA_AP1;

	eth->probe_chs = 0;
	ulink_net_probe_chs(eth, DP_CR5_SAF);
	ulink_net_probe_chs(eth, DP_CA_AP1);
	ulink_net_probe_chs(eth, DP_CA_AP2);
	ulink_net_probe_chs(eth, R_DP_CR5_SAF);
	ulink_net_probe_chs(eth, R_DP_CA_AP1);
	ulink_net_probe_chs(eth, R_DP_CA_AP2);

	spin_lock_init(&eth->lock);
	skb_queue_head_init(&eth->txqueue);
	INIT_WORK(&eth->tx_work, eth_xmit_work_handler);
	device_create_file(&pdev->dev, &dev_attr_devtype);

	ret = register_netdev(ndev);
	if (ret < 0) {
		goto out_ch;
	}

	addr = of_get_property(pdev->dev.of_node, "local-mac-address", &len);
	if (addr && len == ETH_ALEN) {
		hwaddr.sa_family = ndev->type;
		memcpy(hwaddr.sa_data, addr, ETH_ALEN);
		rtnl_lock();
		dev_set_mac_address(ndev, &hwaddr);
		rtnl_unlock();
	}

	setup_timer(&eth->gc_timer, gc_skb, (unsigned long)eth);
	mod_timer(&eth->gc_timer, jiffies + GARBAGE_TIME);
	platform_set_drvdata(pdev, eth);
	return ret;

out_ch:
	ulink_net_remove_chs(eth);

	if (ndev) {
		free_netdev(ndev);
	}

	return ret;
}

static int ulink_net_remove(struct platform_device *pdev)
{
	struct ulink_eth_device *eth = platform_get_drvdata(pdev);
	if (!eth)
		return 0;

	del_timer(&eth->gc_timer);
	device_remove_file(&pdev->dev, &dev_attr_devtype);
	ulink_net_remove_chs(eth);

	if (eth->ndev)
		free_netdev(eth->ndev);

	return 0;
}

static const struct of_device_id ulink_net_match[] = {
	{ .compatible = "sd,ulink-net" },
	{},
};
MODULE_DEVICE_TABLE(of, ulink_net_match);

static struct platform_driver ulink_net_driver = {
	.driver =
		{
			.name = "ulink_net",
			.of_match_table = ulink_net_match,
		},
	.probe = ulink_net_probe,
	.remove = ulink_net_remove,
};

static int __init ulink_net_init(void)
{
	return platform_driver_register(&ulink_net_driver);
}

static void __exit ulink_net_exit(void)
{
	return platform_driver_unregister(&ulink_net_driver);
}

late_initcall(ulink_net_init);
module_exit(ulink_net_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("ulink-based Network Device Driver");
MODULE_LICENSE("GPL v2");
