/*
 * Copyright (C) 2020 Semidrive Ltd.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/netdevice.h>
#include <linux/if.h>
#include "stmmac.h"
#include "hwif.h"
#include "dwmac4.h"

enum filter_cmd {
	SET_MAC_ADDR,
	CLEAN_MAC_ADDR,
	GET_MAC_ADDR,
	SET_MAC_GROUP,
	SET_UNI_DA_SA,
	SET_SA_DROP,
	SET_MC_DROP,
	SET_BC_DROP,
	SET_DISALBE_FILTER,
};

struct filter_config {
	int cmd;
	int addr_index;
	int flag;
	unsigned char addr[ETH_ALEN];
};

static void dwmac_set_mac_addr(struct stmmac_priv *priv, int mac_index, u8 *addr)
{
	unsigned long data = 0;
	void __iomem *ioaddr = priv->hw->pcsr;

	if (mac_index < 0 && mac_index > 127)
		return;

	data = (addr[5] << 8) | addr[4];
	writel(data | (1 << 31), ioaddr + GMAC_ADDR_HIGH(mac_index));
	data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	writel(data, ioaddr + GMAC_ADDR_LOW(mac_index));
}

static void dwmac_get_mac_addr(struct stmmac_priv *priv, int mac_index, u8 *addr)
{
	void __iomem *ioaddr = priv->hw->pcsr;
	unsigned int hi_data, lo_data;

	if (mac_index < 0 && mac_index > 127)
		return;

	hi_data = readl(ioaddr + GMAC_ADDR_HIGH(mac_index));
	lo_data = readl(ioaddr + GMAC_ADDR_LOW(mac_index));

	addr[0] = lo_data & 0xff;
	addr[1] = (lo_data >> 8) & 0xff;
	addr[2] = (lo_data >> 16) & 0xff;
	addr[3] = (lo_data >> 24) & 0xff;
	addr[4] = hi_data & 0xff;
	addr[5] = (hi_data >> 8) & 0xff;
}

static int dwmac_set_mac_addr_group(struct stmmac_priv *priv, int mac_index, u8 addr_mask)
{
	void __iomem *ioaddr = priv->hw->pcsr;
	unsigned int hi_data, lo_data;

	if (mac_index < 1 && mac_index > 31)
		return -1;

	hi_data = readl(ioaddr + GMAC_ADDR_HIGH(mac_index));
	lo_data = readl(ioaddr + GMAC_ADDR_LOW(mac_index));

	hi_data &= ~(0x3f << 24);
	hi_data |= ((addr_mask & 0x3f) << 24);
	printk("\033[31mhi_data %x\033[00m\n", hi_data);
	writel(hi_data, ioaddr + GMAC_ADDR_HIGH(mac_index));
	writel(lo_data, ioaddr + GMAC_ADDR_LOW(mac_index));
	return 0;
}

static int dwmac_set_mac_addr_da_sa(struct stmmac_priv *priv, int mac_index, u8 is_sa)
{
	void __iomem *ioaddr = priv->hw->pcsr;
	unsigned int hi_data, lo_data;

	if (mac_index < 1 && mac_index > 31)
		return -1;

	hi_data = readl(ioaddr + GMAC_ADDR_HIGH(mac_index));
	lo_data = readl(ioaddr + GMAC_ADDR_LOW(mac_index));

	hi_data &= ~(0x1 << 30);
	hi_data |= ((is_sa & 0x1) << 30);
	printk("\033[31mhi_data %x\033[00m\n", hi_data);
	writel(hi_data, ioaddr + GMAC_ADDR_HIGH(mac_index));
	writel(lo_data, ioaddr + GMAC_ADDR_LOW(mac_index));
	return 0;
}

static void dwmac_clean_mac_addr(struct stmmac_priv *priv, int mac_index)
{
	void __iomem *ioaddr = priv->hw->pcsr;

	writel(0, ioaddr + GMAC_ADDR_HIGH(mac_index));
	writel(0, ioaddr + GMAC_ADDR_LOW(mac_index));
}

static void dwmac_set_mac_sa_drop(struct stmmac_priv *priv, int sa_drop)
{
	unsigned long data = 0;
	void __iomem *ioaddr = priv->hw->pcsr;

	data = readl(ioaddr + GMAC_PACKET_FILTER);
	if (sa_drop)
		data |= (1 << 9);
	else
		data &= ~(1 << 9);
	writel(data, ioaddr + GMAC_PACKET_FILTER);
}

static void dwmac_set_mac_multicast_drop(struct stmmac_priv *priv, int mc_drop)
{
	unsigned long data = 0;
	void __iomem *ioaddr = priv->hw->pcsr;

	data = readl(ioaddr + GMAC_PACKET_FILTER);
	if (mc_drop)
		data |= (1 << 4);
	else
		data &= ~(1 << 4);
	writel(data, ioaddr + GMAC_PACKET_FILTER);
}

static void dwmac_set_mac_broadcast_drop(struct stmmac_priv *priv, int bc_drop)
{
	unsigned long data = 0;
	void __iomem *ioaddr = priv->hw->pcsr;

	data = readl(ioaddr + GMAC_PACKET_FILTER);
	if (bc_drop)
		data |= (1 << 5);
	else
		data &= ~(1 << 5);
	writel(data, ioaddr + GMAC_PACKET_FILTER);
}

static void dwmac_set_mac_disable_filter(struct stmmac_priv *priv, int disable_filter)
{
	unsigned long data = 0;
	void __iomem *ioaddr = priv->hw->pcsr;

	data = readl(ioaddr + GMAC_PACKET_FILTER);
	if (disable_filter)
		data |= (1 << 0);
	else
		data &= ~(1 << 0);
	writel(data, ioaddr + GMAC_PACKET_FILTER);
}

static void dwmac_set_mac_disable_hash_filter(struct stmmac_priv *priv)
{
	unsigned long data = 0;
	void __iomem *ioaddr = priv->hw->pcsr;

	data = readl(ioaddr + GMAC_PACKET_FILTER);
	if (data & (1 << 10)) {
		data &= ~(1 << 10);
		writel(data, ioaddr + GMAC_PACKET_FILTER);
	}
}

int dwmac_filter_ioctl(struct net_device *dev, struct ifreq *ifr)
{
	struct stmmac_priv *priv = netdev_priv(dev);
	struct filter_config config;

	if (copy_from_user(&config, ifr->ifr_data,
				sizeof(struct filter_config)))
		return -EFAULT;

	printk("\033[31m%s dejin cmd %x index %d, flag %x, " \
		"addr[%02x:%02x:%02x:%02x:%02x:%02x:]\033[00m\n", __func__,
		config.cmd, config.addr_index, config.flag,
		config.addr[0], config.addr[1], config.addr[2], config.addr[3],
		config.addr[4], config.addr[5]);

	dwmac_set_mac_disable_hash_filter(priv);

	switch (config.cmd) {
	case SET_MAC_ADDR:
		dwmac_set_mac_addr(priv, config.addr_index, config.addr);
		break;
	case CLEAN_MAC_ADDR:
		dwmac_clean_mac_addr(priv, config.addr_index);
		break;
	case GET_MAC_ADDR:
		dwmac_get_mac_addr(priv, config.addr_index, config.addr);
		break;
	case SET_MAC_GROUP:
		dwmac_set_mac_addr_group(priv, config.addr_index, config.flag);
		break;
	case SET_UNI_DA_SA:
		dwmac_set_mac_addr_da_sa(priv, config.addr_index, config.flag);
		break;
	case SET_SA_DROP:
		dwmac_set_mac_sa_drop(priv, config.flag);
		break;
	case SET_MC_DROP:
		dwmac_set_mac_multicast_drop(priv, config.flag);
		break;
	case SET_BC_DROP:
		dwmac_set_mac_broadcast_drop(priv, config.flag);
		break;
	case SET_DISALBE_FILTER:
		dwmac_set_mac_disable_filter(priv, config.flag);
		break;
	default:
		break;
	}

	return copy_to_user(ifr->ifr_data, &config,
			    sizeof(struct filter_config)) ? -EFAULT : 0;
}
