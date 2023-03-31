/*
 * Copyright (c) 2022, Semidriver Semiconductor
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
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/soc/semidrive/ulink.h>

#define  DMA_WRITE_ENGINE_EN (0XC)
#define  DMA_WRITE_DOORBELL (0x10)
#define  DMA_WRITE_INT_STS (0X4c)
#define  DMA_WRITE_INT_MASK (0X54)
#define  DMA_WRITE_INT_CLR (0X58)

#define  DMA_CTRL1_WRCH (0x0)
#define  DMA_SIZE_WRCH (0x8)
#define  DMA_SRC_LOW_WRCH (0xc)
#define  DMA_SRC_HI_WRCH (0x10)
#define  DMA_DST_LOW_WRCH (0x14)
#define  DMA_DST_HI_WRCH (0x18)

#define  DMA_READ_ENGINE_EN (0X2C)
#define  DMA_READ_DOORBELL (0x30)
#define  DMA_READ_INT_STS (0Xa0)
#define  DMA_READ_INT_MASK (0Xa8)
#define  DMA_READ_INT_CLR (0Xac)

#define  DMA_CTRL1_RDCH (0x0)
#define  DMA_SIZE_RDCH (0x8)
#define  DMA_SRC_LOW_RDCH (0xc)
#define  DMA_SRC_HI_RDCH (0x10)
#define  DMA_DST_LOW_RDCH (0x14)
#define  DMA_DST_HI_RDCH (0x18)

#define LOW32(x) ((x) & 0xffffffff)
#define HIGH32(x) ((x) >> 32)

enum pcie_mode {
	rc_mode,
	ep_mode,
};

struct ulink_pcie_device {
	struct device *dev;
	spinlock_t dma_lock;
	enum pcie_mode mode;
	uint32_t channel;
	void __iomem *pcie_base;
};

static struct ulink_pcie_device *ulink_pcie_handle;

static int ulink_pcie_phy_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	return pci_enable_device(pdev);
}

static void ulink_pcie_phy_remove(struct pci_dev *pdev)
{
}

static void uilnk_pcie_dma_init(struct ulink_pcie_device *tdev)
{
	uint32_t val;
	void *dma_base;

	dma_base = tdev->pcie_base;
	val = readl(dma_base + DMA_WRITE_ENGINE_EN);
	val |= 0x1;
	writel(val, dma_base + DMA_WRITE_ENGINE_EN);
	val = readl(dma_base + DMA_READ_ENGINE_EN);
	val |= 0x1;
	writel(val, dma_base + DMA_READ_ENGINE_EN);
	writel(0, dma_base + DMA_WRITE_INT_MASK);
	writel(0, dma_base + DMA_READ_INT_MASK);
}

static int ulink_pcie_resume(struct device *dev)
{
	struct ulink_pcie_device *tdev =
		platform_get_drvdata(to_platform_device(dev));

	uilnk_pcie_dma_init(tdev);
	return 0;
}

static int ulink_pcie_probe(struct platform_device *pdev)
{
	struct ulink_pcie_device *tdev;
	struct resource *res;
	void __iomem *addr;

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	if (of_property_read_u32(pdev->dev.of_node, "channel", &tdev->channel))
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	addr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	tdev->pcie_base = addr;
	tdev->mode = (enum pcie_mode)(of_device_get_match_data(&pdev->dev));
	spin_lock_init(&tdev->dma_lock);
	uilnk_pcie_dma_init(tdev);
	tdev->dev = &pdev->dev;
	platform_set_drvdata(pdev, tdev);
	ulink_pcie_handle = tdev;

	return 0;
}

static int ulink_pcie_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct pci_device_id ulink_pcie_phy_ids[] = {
	{ PCI_DEVICE(0x1e8c, 0x0002), },
	{ 0,}
};

static struct pci_driver ulink_pcie_phy_driver = {
	.name = "ulink_pcie_phy",
	.id_table = ulink_pcie_phy_ids,
	.probe = ulink_pcie_phy_probe,
	.remove = ulink_pcie_phy_remove,
};

static const struct of_device_id ulink_pcie_match[] = {
	{ .compatible = "sd,ulink-pcie-rc", .data = (void*)rc_mode},
	{ .compatible = "sd,ulink-pcie-ep", .data = (void*)ep_mode},
	{},
};
MODULE_DEVICE_TABLE(of, ulink_pcie_match);

static SIMPLE_DEV_PM_OPS(ulink_pcie_ops, NULL, ulink_pcie_resume);

static struct platform_driver ulink_pcie_driver = {
	.driver = {
		.name = "ulink_pcie",
		.of_match_table = ulink_pcie_match,
                .pm = &ulink_pcie_ops,
	},
	.probe  = ulink_pcie_probe,
	.remove = ulink_pcie_remove,
};

int ulink_pcie_init(void)
{
	int ret;

	ret = pci_register_driver(&ulink_pcie_phy_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&ulink_pcie_driver);

	return ret;
}

void ulink_pcie_exit(void)
{
	pci_unregister_driver(&ulink_pcie_phy_driver);
	platform_driver_unregister(&ulink_pcie_driver);
}

static int kunlun_pcie_dma_write(int dma_channel, uint64_t dst, uint64_t src, uint32_t len)
{
	uint32_t val, val1;
	void *dma_base, *dma_channel_base;
	uint32_t count = len/1024 + 1;

	dma_base = ulink_pcie_handle->pcie_base;
	dma_channel_base = dma_base + 0x200 + 0x200 * dma_channel;

	writel(0x8, dma_channel_base + DMA_CTRL1_WRCH);

	writel(LOW32(src), dma_channel_base + DMA_SRC_LOW_WRCH);
	writel(HIGH32(src), dma_channel_base + DMA_SRC_HI_WRCH);

	writel(LOW32(dst), dma_channel_base + DMA_DST_LOW_WRCH);
	writel(HIGH32(dst), dma_channel_base + DMA_DST_HI_WRCH);

	writel(len, dma_channel_base + DMA_SIZE_WRCH);

	writel(dma_channel, dma_base + DMA_WRITE_DOORBELL);

	while (1) {
		val = readl(dma_base + DMA_WRITE_INT_STS);
		val1 = readl(dma_channel_base + DMA_CTRL1_WRCH);
		if (val & (1 << (dma_channel + 16))) {
			writel(1 << (dma_channel + 16), dma_base + DMA_WRITE_INT_CLR);
			return -1;
		}
		if ((val & (1 << dma_channel)) && ((val1 & 0x60) == 0x60)) {
			writel(1 << (dma_channel), dma_base + DMA_WRITE_INT_CLR);
			return 0;
		}

		if (count--)
			udelay(3);
		else
			return -2;
	}
}

static int kunlun_pcie_dma_read(int dma_channel, uint64_t dst, uint64_t src, uint32_t len)
{
	uint32_t val, val1;
	void *dma_base, *dma_channel_base;
	uint32_t count = len/1024 + 1;

	dma_base = ulink_pcie_handle->pcie_base;
	dma_channel_base = dma_base + 0x300 + 0x200 * dma_channel;

	writel(0x8, dma_channel_base + DMA_CTRL1_RDCH);

	writel(LOW32(src), dma_channel_base + DMA_SRC_LOW_RDCH);
	writel(HIGH32(src), dma_channel_base + DMA_SRC_HI_RDCH);

	writel(LOW32(dst), dma_channel_base + DMA_DST_LOW_RDCH);
	writel(HIGH32(dst), dma_channel_base + DMA_DST_HI_RDCH);

	writel(len, dma_channel_base + DMA_SIZE_RDCH);

	writel(dma_channel, dma_base + DMA_READ_DOORBELL);

	while (1) {
		val = readl(dma_base + DMA_READ_INT_STS);
		val1 = readl(dma_channel_base + DMA_CTRL1_RDCH);
		if (val & (1 << (dma_channel + 16))) {
			writel(1 << (dma_channel + 16), dma_base + DMA_READ_INT_CLR);
			return -1;
	        }
		if ((val & (1 << dma_channel)) && ((val1 & 0x60) == 0x60)) {
			writel((1 << dma_channel), dma_base + DMA_READ_INT_CLR);
			return 0;
		}

		if (count--)
			udelay(3);
		else
			return -2;
	}

}

int kunlun_pcie_dma_copy(uint64_t local, uint64_t remote, uint32_t len, pcie_dir dir)
{
	int ret;
	unsigned long flag;

	if (!ulink_pcie_handle)
		return -1;

	spin_lock_irqsave(&ulink_pcie_handle->dma_lock, flag);
	if (ulink_pcie_handle->mode == rc_mode) {
		if (dir == DIR_PUSH)
			ret = kunlun_pcie_dma_read(ulink_pcie_handle->channel, remote, local, len);
		else if (dir == DIR_PULL)
			ret = kunlun_pcie_dma_write(ulink_pcie_handle->channel, local, remote, len);
		else
			ret = 1;
	} else if (ulink_pcie_handle->mode == ep_mode) {
		if (dir == DIR_PUSH)
			ret = kunlun_pcie_dma_write(ulink_pcie_handle->channel, remote, local, len);
		else if (dir == DIR_PULL)
			ret = kunlun_pcie_dma_read(ulink_pcie_handle->channel, local, remote, len);
		else
			ret = 1;
	} else {
		ret = -1;
	}
	spin_unlock_irqrestore(&ulink_pcie_handle->dma_lock, flag);
	return ret;
}
EXPORT_SYMBOL(kunlun_pcie_dma_copy);

MODULE_DESCRIPTION("ulink pcie dma driver");
MODULE_AUTHOR("mingmin.ling <mingmin.ling@semidrive.com");
MODULE_LICENSE("GPL v2");
