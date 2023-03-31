/*
 * Copyright (C) 2020 Semidrive Ltd.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#define USB_PHY_NCR_CTRL0	0x10000

struct phy_semidrive_usb_priv {
	void __iomem		*base;
};

static int phy_semidrive_usb_init(struct phy *phy)
{
	struct phy_semidrive_usb_priv *priv = phy_get_drvdata(phy);
	unsigned int data;

	/* use internal phy clock and reset usb phy, reset high effective */
	data = readl(priv->base + USB_PHY_NCR_CTRL0);
	data &= ~(1<<18);
	data |= (1<<0);
	writel(data, priv->base + USB_PHY_NCR_CTRL0);

	udelay(30);

	data = readl(priv->base + USB_PHY_NCR_CTRL0);
	data &= ~(1<<0);
	writel(data, priv->base + USB_PHY_NCR_CTRL0);

	return 0;
}

static const struct phy_ops phy_semidrive_usb_ops = {
	.init		= phy_semidrive_usb_init,
	.owner		= THIS_MODULE,
};

static int phy_semidrive_usb_probe(struct platform_device *pdev)
{
	struct phy_semidrive_usb_priv *priv;
	struct resource *res;
	struct phy *phy;
	struct phy_provider *phy_provider;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	phy = devm_phy_create(&pdev->dev, NULL, &phy_semidrive_usb_ops);
	if (IS_ERR(phy)) {
		dev_err(&pdev->dev, "failed to create PHY\n");
		return PTR_ERR(phy);
	}

	phy_set_drvdata(phy, priv);

	phy_provider =
		devm_of_phy_provider_register(&pdev->dev, of_phy_simple_xlate);
	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id phy_semidrive_usb_of_match[] = {
	{ .compatible = "semidrive,usb-phy" },
	{ },
};
MODULE_DEVICE_TABLE(of, phy_semidrive_usb_of_match);

static struct platform_driver phy_semidrive_usb_driver = {
	.probe	= phy_semidrive_usb_probe,
	.driver	= {
		.name		= "semidrive-usb-phy",
		.of_match_table	= phy_semidrive_usb_of_match,
	},
};
module_platform_driver(phy_semidrive_usb_driver);

MODULE_AUTHOR("Dejin Zheng <dejin.zheng@semidrive.com>");
MODULE_DESCRIPTION("Semidrive PHY driver for USB");
MODULE_LICENSE("GPL");
