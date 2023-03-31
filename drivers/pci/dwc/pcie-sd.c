/*
 * PCIe host controller driver for Semidrive
 *
 *
 *
 *
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/compiler.h>
#include <linux/clk.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <linux/types.h>
#include <linux/reset.h>

#include "pcie-designware.h"

#define to_sd_pcie(x) dev_get_drvdata((x)->dev)

#define SD_PCIE_ATU_OFFSET (0xc0000)

/* PCIe PHY NCR registers */
#define PCIE_PHY_NCR_CTRL0 (0x0)
#define PCIE_PHY_NCR_CTRL1 (0x4)
#define PCIE_PHY_NCR_CTRL4 (0x10)
#define PCIE_PHY_NCR_CTRL15 (0x3c)
#define PCIE_PHY_NCR_CTRL25 (0x64)
#define PCIE_PHY_NCR_STS0 (0x80)

#define PCIE1_DBI (0x31000000)
#define PCIE2_DBI (0x31100000)

/* info in PCIe PHY NCR registers */
#define CR_ADDR_MODE_BIT (0x1 << 29)
#define BIF_EN_BIT (0x1 << 1)
#define PHY_REF_USE_PAD_BIT (0x1 << 5)
#define PHY_REF_ALT_CLK_SEL_BIT (0x1 << 8)

#define PHY_RESET_BIT (0x1 << 0)
#define CR_CKEN_BIT (0x1 << 24)

/* PCIe CTRL NCR registers */
#define PCIE_CTRL_NCR_INTR0 (0x0)
#define PCIE_CTRL_NCR_INTEN0 (0x34)
#define PCIE_CTRL_NCR_INTEN1 (0x38)
#define PCIE_CTRL_NCR_INTEN2 (0x3c)
#define PCIE_CTRL_NCR_INTEN3 (0x40)
#define PCIE_CTRL_NCR_INTEN4 (0x44)
#define PCIE_CTRL_NCR_INTEN5 (0x48)
#define PCIE_CTRL_NCR_INTEN6 (0x4c)
#define PCIE_CTRL_NCR_INTEN7 (0x50)
#define PCIE_CTRL_NCR_INTEN8 (0x54)
#define PCIE_CTRL_NCR_INTEN9 (0x58)
#define PCIE_CTRL_NCR_INTEN10 (0x5c)
#define PCIE_CTRL_NCR_INTEN11 (0x60)
#define PCIE_CTRL_NCR_INTEN12 (0x64)
#define PCIE_CTRL_NCR_CTRL0 (0x68)
#define PCIE_CTRL_NCR_CTRL2 (0x70)
#define PCIE_CTRL_NCR_CTRL6 (0x80)
#define PCIE_CTRL_NCR_CTRL21 (0xbc)
#define PCIE_CTRL_NCR_CTRL22 (0xc0)
#define PCIE_CTRL_NCR_CTRL23 (0xc4)
#define PCIE_CTRL_NCR_CTRL24 (0xc8)

/* info in PCIe CTRL NCR registers */
#define APP_LTSSM_EN_BIT (0x1 << 2)
#define DEVICE_TYPE_BIT (0x1 << 3)
#define APP_HOLD_PHY_RST_BIT (0x1 << 6)
#define INTR_SMLH_LINK_UP (0x1 << 28)
#define INTR_RDLH_LINK_UP (0x1 << 27)
#define RESET_MASK (0x3f << 6)
#define RESET_OFF (0x3f << 6)
#define RESET_ON (0x1f << 6)
#define MSI_OVRDEN (0x1 << 30)

/* Time for delay */
#define TIME_PHY_RST_MIN (5)
#define TIME_PHY_RST_MAX (10)
#define LINK_WAIT_MIN (900)
#define LINK_WAIT_MAX (1000)

#define NO_PCIE (0)
#define PCIE1_INDEX (1)
#define PCIE2_INDEX (2)

#define PHY_REFCLK_USE_INTERNAL (0x1)
#define PHY_REFCLK_USE_EXTERNAL (0x2)
#define PHY_REFCLK_USE_DIFFBUF (0x3)
#define PHY_REFCLK_USE_MASK (0x3)
#define DIFFBUF_OUT_EN (0x1 << 31)

#define LANE_CFG(lane1, lane0) (lane0 | lane1 << 16)
#define GET_LANE0_CFG(val) (val & 0xffff)
#define GET_LANE1_CFG(val) (val >> 16)

static DEFINE_MUTEX(phy_lock);
static int phy_init_flag = 0;

enum sd_pcie_lane_cfg {
	PCIE1_WITH_LANE0 = LANE_CFG(NO_PCIE, PCIE1_INDEX),
	PCIE1_WITH_LANE1 = LANE_CFG(PCIE1_INDEX, NO_PCIE),
	PCIE1_WITH_ALL_LANES = LANE_CFG(PCIE1_INDEX, PCIE1_INDEX),
	PCIE2_WITH_LANE1 = LANE_CFG(PCIE2_INDEX, NO_PCIE),
	PCIE1_2_EACH_ONE_LANE = LANE_CFG(PCIE2_INDEX, PCIE1_INDEX),
};

struct sd_pcie {
	struct dw_pcie *pcie;
	struct clk *pcie_aclk;
	struct clk *pcie_pclk;
	struct clk *phy_ref;
	struct clk *phy_pclk;
	void __iomem *phy_base;
	void __iomem *phy_ncr_base;
	void __iomem *ctrl_ncr_base;
	enum sd_pcie_lane_cfg lane_config;
	u32 phy_refclk_sel;
	u32 index;
	struct gpio_desc *reset_gpio;
	int reset_time;
};

static inline void sd_apb_phy_writel(struct sd_pcie *sd_pcie,
					 u32 val, u32 reg)
{
	writel(val, sd_pcie->phy_base + reg);
}

static inline u32 sd_apb_phy_readl(struct sd_pcie *sd_pcie, u32 reg)
{
	return readl(sd_pcie->phy_base + reg);
}

static inline void sd_phy_ncr_writel(struct sd_pcie *sd_pcie,
					 u32 val, u32 reg)
{
	writel(val, sd_pcie->phy_ncr_base + reg);
}

static inline u32 sd_phy_ncr_readl(struct sd_pcie *sd_pcie, u32 reg)
{
	return readl(sd_pcie->phy_ncr_base + reg);
}

static inline void sd_ctrl_ncr_writel(struct sd_pcie *sd_pcie,
					  u32 val, u32 reg)
{
	writel(val, sd_pcie->ctrl_ncr_base + reg);
}

static inline u32 sd_ctrl_ncr_readl(struct sd_pcie *sd_pcie,
					u32 reg)
{
	return readl(sd_pcie->ctrl_ncr_base + reg);
}

static int sd_pcie_get_clk(struct sd_pcie *sd_pcie,
			       struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sd_pcie->pcie_aclk = devm_clk_get(dev, "pcie_aclk");
	if (IS_ERR(sd_pcie->pcie_aclk))
		return PTR_ERR(sd_pcie->pcie_aclk);

	sd_pcie->pcie_pclk = devm_clk_get(dev, "pcie_pclk");
	if (IS_ERR(sd_pcie->pcie_pclk))
		return PTR_ERR(sd_pcie->pcie_pclk);

	sd_pcie->phy_ref = devm_clk_get(dev, "pcie_phy_ref");
	if (IS_ERR(sd_pcie->phy_ref))
		return PTR_ERR(sd_pcie->phy_ref);

	sd_pcie->phy_pclk = devm_clk_get(dev, "pcie_phy_pclk");
	if (IS_ERR(sd_pcie->phy_pclk))
		return PTR_ERR(sd_pcie->phy_pclk);

	return 0;
}

static int pcie_rstgen_reset(struct device *device)
{
	struct reset_control *pcie_rst;
	int status;

	pcie_rst = devm_reset_control_get(device, "pcie-reset");
	if (IS_ERR(pcie_rst)) {
		dev_err(device, "Missing controller reset\n");
		return 0;
	}

	status = reset_control_status(pcie_rst);
	dev_info(device, "Before reset, PCIe rstgen status is %d\n", status);

	if (reset_control_reset(pcie_rst)) {
		dev_err(device, "rstgen reset PCIe failed\n");
		return -1;
	}

	status = reset_control_status(pcie_rst);
	dev_info(device, "After reset, PCIe rstgen status is %d\n", status);

	return 0;
}

static int sd_pcie_get_resource(struct sd_pcie *sd_pcie,
				    struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *ctrl_ncr;
	struct resource *phy;
	struct resource *phy_ncr;
	struct resource *dbi;

	ctrl_ncr = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				      "ctrl_ncr");
	sd_pcie->ctrl_ncr_base = devm_ioremap_resource(dev, ctrl_ncr);
	if (IS_ERR(sd_pcie->ctrl_ncr_base))
		return PTR_ERR(sd_pcie->ctrl_ncr_base);

	phy = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	sd_pcie->phy_base = devm_ioremap(dev, phy->start, resource_size(phy));
	if (IS_ERR(sd_pcie->phy_base))
		return PTR_ERR(sd_pcie->phy_base);

	dbi = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	sd_pcie->pcie->dbi_base = devm_ioremap_resource(dev, dbi);
	if (IS_ERR(sd_pcie->pcie->dbi_base))
		return PTR_ERR(sd_pcie->pcie->dbi_base);

	if (dbi->start == PCIE1_DBI)
		sd_pcie->index = PCIE1_INDEX;
	else if (dbi->start == PCIE2_DBI)
		sd_pcie->index = PCIE2_INDEX;

	sd_pcie->pcie->atu_base =
	    sd_pcie->pcie->dbi_base + SD_PCIE_ATU_OFFSET;

	phy_ncr = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy_ncr");
	sd_pcie->phy_ncr_base = devm_ioremap(dev, phy_ncr->start,
						resource_size(phy_ncr));
	if (IS_ERR(sd_pcie->phy_ncr_base))
		return PTR_ERR(sd_pcie->phy_ncr_base);

	return 0;
}

void sd_pcie_phy_refclk_sel(struct sd_pcie *sd_pcie)
{
	u32 reg_val;

	u32 flag = sd_pcie->phy_refclk_sel & PHY_REFCLK_USE_MASK;
	bool diffbuf_out_en = !!(sd_pcie->phy_refclk_sel >> 31);

	if ((flag == PHY_REFCLK_USE_DIFFBUF) && diffbuf_out_en) {
		pr_info("[ERROR] wrong ref clk cfg\n");
		BUG();
	}

	if (flag == PHY_REFCLK_USE_INTERNAL) {
		pr_info("using internel ref clk\n");
		reg_val = sd_phy_ncr_readl(sd_pcie, PCIE_PHY_NCR_CTRL1);
		reg_val &= ~PHY_REF_USE_PAD_BIT;
		reg_val &= ~PHY_REF_ALT_CLK_SEL_BIT;
		sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL1);
	} else if (flag == PHY_REFCLK_USE_EXTERNAL) {
		pr_info("using externel ref clk\n");
		reg_val = sd_phy_ncr_readl(sd_pcie, PCIE_PHY_NCR_CTRL1);
		reg_val |= PHY_REF_USE_PAD_BIT;
		sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL1);
	} else if (flag == PHY_REFCLK_USE_DIFFBUF) {
		pr_info("using diffbuf_extref_clk\n");
		reg_val = sd_phy_ncr_readl(sd_pcie, PCIE_PHY_NCR_CTRL1);
		reg_val &= ~(PHY_REF_USE_PAD_BIT);
		reg_val |= PHY_REF_ALT_CLK_SEL_BIT;
		sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL1);

		reg_val = sd_phy_ncr_readl(sd_pcie,
				       PCIE_PHY_NCR_CTRL15);
		reg_val &= ~0x1;
		sd_phy_ncr_writel(sd_pcie, reg_val,
				       PCIE_PHY_NCR_CTRL15);
	} else {
		pr_info("error phy_refclk_sel\n");
		BUG();
	}

	if (diffbuf_out_en) {
		pr_info("diffbuf out enable\n");
		reg_val = sd_phy_ncr_readl(sd_pcie,
				      PCIE_PHY_NCR_CTRL15);
		reg_val |= 0x1;
		sd_phy_ncr_writel(sd_pcie, reg_val,
				      PCIE_PHY_NCR_CTRL15);
	}
}

void sd_pcie_of_get_ref_clk(const struct device_node *device, struct sd_pcie *pcie)
{
	const char *ref_clk;
	int len;

	ref_clk = of_get_property(device, "ref-clk", &len);

	if (ref_clk == NULL) {
		pr_info("no \"ref-clk\" property, will using default internal clk\n");
		pcie->phy_refclk_sel = PHY_REFCLK_USE_INTERNAL;
		return;
	}

	if (!strcmp(ref_clk, "external,diffbuf_on")) {
		pcie->phy_refclk_sel = PHY_REFCLK_USE_EXTERNAL | DIFFBUF_OUT_EN;
	} else if (!strcmp(ref_clk, "external")) {
		pcie->phy_refclk_sel = PHY_REFCLK_USE_EXTERNAL;
	} else if (!strcmp(ref_clk, "internal")) {
		pcie->phy_refclk_sel = PHY_REFCLK_USE_INTERNAL;
	} else if (!strcmp(ref_clk, "internal,diffbuf_on")) {
		pcie->phy_refclk_sel = PHY_REFCLK_USE_INTERNAL | DIFFBUF_OUT_EN;
	} else {
		pr_info("wrong \"ref-clk\" property value, will using default internal clk\n");
		pcie->phy_refclk_sel = PHY_REFCLK_USE_INTERNAL;
	}

}

static void sd_pcie_lane_config(struct sd_pcie *sd_pcie)
{
	u32 lane0 = GET_LANE0_CFG(sd_pcie->lane_config);
	u32 lane1 = GET_LANE1_CFG(sd_pcie->lane_config);

	u32 reg_val = 0;

	reg_val = sd_phy_ncr_readl(sd_pcie, PCIE_PHY_NCR_CTRL0);
	if (lane1 == PCIE2_INDEX)
		reg_val |= BIF_EN_BIT;
	else
		reg_val &= ~BIF_EN_BIT;
	sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL0);

	reg_val = 0;

	if (lane0 == NO_PCIE)
		reg_val |= 0xf2;
	if (lane1 == NO_PCIE)
		reg_val |= 0xf20000;
	sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL25);
}

static int sd_pcie_phy_init(struct sd_pcie *sd_pcie)
{
	u32 reg_val;

	mutex_lock(&phy_lock);
	if (phy_init_flag == 1)
		goto init_end;

	sd_pcie_phy_refclk_sel(sd_pcie);

	reg_val = sd_phy_ncr_readl(sd_pcie, PCIE_PHY_NCR_CTRL0);
	reg_val |= CR_CKEN_BIT;
	sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL0);

	reg_val = sd_phy_ncr_readl(sd_pcie, PCIE_PHY_NCR_CTRL4);
	reg_val &= ~CR_ADDR_MODE_BIT;
	sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL4);

	sd_pcie_lane_config(sd_pcie);

	reg_val = sd_phy_ncr_readl(sd_pcie, PCIE_PHY_NCR_CTRL0);
	reg_val &= ~PHY_RESET_BIT;
	sd_phy_ncr_writel(sd_pcie, reg_val, PCIE_PHY_NCR_CTRL0);

	usleep_range(TIME_PHY_RST_MIN, TIME_PHY_RST_MAX);
	phy_init_flag = 1;

init_end:
	mutex_unlock(&phy_lock);
	return 0;
}

static int sd_pcie_clk_ctrl(struct sd_pcie *sd_pcie, bool enable)
{
	int ret = 0;

	if (!enable)
		goto disable_clk;

	ret = clk_prepare_enable(sd_pcie->phy_ref);
	if (ret)
		return ret;

	ret = clk_prepare_enable(sd_pcie->phy_pclk);
	if (ret)
		goto phy_pclk_fail;

	ret = clk_prepare_enable(sd_pcie->pcie_aclk);
	if (ret)
		goto pcie_aclk_fail;

	ret = clk_prepare_enable(sd_pcie->pcie_pclk);
	if (ret)
		goto pcie_pclk_fail;

	return 0;

disable_clk:
	clk_disable_unprepare(sd_pcie->pcie_pclk);

pcie_pclk_fail:
	clk_disable_unprepare(sd_pcie->pcie_aclk);

pcie_aclk_fail:
	clk_disable_unprepare(sd_pcie->phy_pclk);

phy_pclk_fail:
	clk_disable_unprepare(sd_pcie->phy_ref);

	return ret;
}

static int sd_pcie_power_on(struct sd_pcie *sd_pcie)
{
	int ret;

	ret = sd_pcie_clk_ctrl(sd_pcie, true);
	if (ret)
		return ret;

	sd_pcie_phy_init(sd_pcie);

	return 0;
}

static int sd_pcie_link_up(struct dw_pcie *pcie)
{
	struct sd_pcie *sd_pcie = to_sd_pcie(pcie);

	u32 reg_val = sd_ctrl_ncr_readl(sd_pcie, PCIE_CTRL_NCR_INTR0);

	if ((reg_val & INTR_SMLH_LINK_UP) != INTR_SMLH_LINK_UP)
		return 0;

	if ((reg_val & INTR_RDLH_LINK_UP) != INTR_RDLH_LINK_UP)
		return 0;

	return 1;
}

static void sd_pcie_core_init(struct sd_pcie *sd_pcie)
{
	u32 reg_val;

	writel(0, sd_pcie->pcie->dbi_base + 0x1010);

	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN0);
	if (IS_ENABLED(CONFIG_PCI_MSI) && pci_msi_enabled())
		sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN1);
	else
		sd_ctrl_ncr_writel(sd_pcie, 0x3c000000, PCIE_CTRL_NCR_INTEN1);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN2);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN3);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN4);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN5);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN6);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN7);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN8);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN9);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN10);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN11);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_INTEN12);

	reg_val = sd_ctrl_ncr_readl(sd_pcie, PCIE_CTRL_NCR_CTRL0);
	reg_val &= ~APP_HOLD_PHY_RST_BIT;
	sd_ctrl_ncr_writel(sd_pcie, reg_val, PCIE_CTRL_NCR_CTRL0);

	reg_val = sd_ctrl_ncr_readl(sd_pcie, PCIE_CTRL_NCR_CTRL0);
	reg_val |= DEVICE_TYPE_BIT;
	sd_ctrl_ncr_writel(sd_pcie, reg_val, PCIE_CTRL_NCR_CTRL0);

	reg_val = sd_ctrl_ncr_readl(sd_pcie, PCIE_CTRL_NCR_CTRL2);
	reg_val &= ~RESET_MASK;
	reg_val |= RESET_ON;
	sd_ctrl_ncr_writel(sd_pcie, reg_val, PCIE_CTRL_NCR_CTRL2);

#if defined(CONFIG_GK20A_PCI)
	/* for GP106 */
	usleep_range(10000, 11000);
#else
	usleep_range(1000, 1100);
#endif

	reg_val &= ~RESET_MASK;
	reg_val |= RESET_OFF;
	sd_ctrl_ncr_writel(sd_pcie, reg_val, PCIE_CTRL_NCR_CTRL2);

	reg_val = sd_ctrl_ncr_readl(sd_pcie, PCIE_CTRL_NCR_CTRL6);
	reg_val |= MSI_OVRDEN;
	sd_ctrl_ncr_writel(sd_pcie, reg_val, PCIE_CTRL_NCR_CTRL6);

	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_CTRL21);
	sd_ctrl_ncr_writel(sd_pcie, 0, PCIE_CTRL_NCR_CTRL23);
}

static int sd_pcie_establish_link(struct pcie_port *pp)
{
	struct dw_pcie *pcie = to_dw_pcie_from_pp(pp);
	struct sd_pcie *sd_pcie = to_sd_pcie(pcie);
	struct device *dev = sd_pcie->pcie->dev;
	u32 reg_val;
	int count = 0;
	u32 exp_cap_off = 0x70;

	dw_pcie_setup_rc(pp);

	reg_val = sd_ctrl_ncr_readl(sd_pcie, PCIE_CTRL_NCR_CTRL0);
	reg_val |= APP_LTSSM_EN_BIT;
	sd_ctrl_ncr_writel(sd_pcie, reg_val, PCIE_CTRL_NCR_CTRL0);

	/* check if the link is up or not */
	while (!sd_pcie_link_up(pcie)) {
		usleep_range(LINK_WAIT_MIN, LINK_WAIT_MAX);
		count++;

		reg_val = dw_pcie_readl_dbi(pcie, exp_cap_off + PCI_EXP_LNKCTL);
		reg_val |= PCI_EXP_LNKCTL_RL;
		dw_pcie_writel_dbi(pcie, exp_cap_off + PCI_EXP_LNKCTL, reg_val);

		if (count == 1000) {
			dev_err(dev, "Link Fail\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int sd_pcie_host_init(struct pcie_port *pp)
{
	int ret =0;
	struct dw_pcie *pcie = to_dw_pcie_from_pp(pp);
	struct sd_pcie *sd_pcie = to_sd_pcie(pcie);

	sd_pcie_core_init(sd_pcie);
	ret = sd_pcie_establish_link(pp);
	if (ret)
		return ret;

	if (IS_ENABLED(CONFIG_PCI_MSI) && pci_msi_enabled())
		dw_pcie_msi_init(pp);

	return 0;
}

u32 sd_pcie_get_index(struct pcie_port *pp)
{
	struct dw_pcie *pcie = to_dw_pcie_from_pp(pp);
	struct sd_pcie *sd_pcie = to_sd_pcie(pcie);

	return sd_pcie->index;
}

static struct dw_pcie_ops sd_dw_pcie_ops = {
	.link_up = sd_pcie_link_up,
};

static const struct dw_pcie_host_ops sd_pcie_host_ops = {
	.host_init = sd_pcie_host_init,
};

static int __init sd_add_pcie_port(struct dw_pcie *pcie,
				       struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct pcie_port *pp = &(pcie->pp);

	if (IS_ENABLED(CONFIG_PCI_MSI) && pci_msi_enabled()) {
		pp->msi_irq = platform_get_irq_byname(pdev, "msi");
		if (pp->msi_irq <= 0) {
			dev_err(dev, "failed to get MSI irq\n");
			return -ENODEV;
		}
	}

	pp->ops = &sd_pcie_host_ops;

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "failed to initialize host\n");
		return ret;
	}

	return 0;
}

static void sd_pcie_reset_gpio(struct sd_pcie *sd_pcie)
{
	struct device *dev = sd_pcie->pcie->dev;
	struct device_node *np  = dev->of_node;

	sd_pcie->reset_gpio = gpiod_get_index(dev, "reset", 0, GPIOD_OUT_LOW);
	if (IS_ERR(sd_pcie->reset_gpio))
		return;

	if (of_property_read_u32(np, "reset-time", &sd_pcie->reset_time))
		sd_pcie->reset_time = 100;

	gpiod_set_value_cansleep(sd_pcie->reset_gpio, 0);
	msleep(sd_pcie->reset_time);
	gpiod_set_value_cansleep(sd_pcie->reset_gpio, 1);
}

static int sd_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sd_pcie *sd_pcie;
	struct dw_pcie *pcie;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "NULL node\n");
		return -EINVAL;
	}

	ret = pcie_rstgen_reset(dev);
	if (ret)
		return ret;

	sd_pcie = devm_kzalloc(dev, sizeof(struct sd_pcie), GFP_KERNEL);
	if (!sd_pcie)
		return -ENOMEM;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pcie->dev = dev;
	pcie->ops = &sd_dw_pcie_ops;
	sd_pcie->pcie = pcie;
	sd_pcie->lane_config = PCIE1_2_EACH_ONE_LANE;

	sd_pcie_reset_gpio(sd_pcie);

	sd_pcie_of_get_ref_clk(dev->of_node, sd_pcie);

	ret = sd_pcie_get_clk(sd_pcie, pdev);
	if (ret)
		return ret;

	ret = sd_pcie_get_resource(sd_pcie, pdev);
	if (ret)
		return ret;

	ret = sd_pcie_power_on(sd_pcie);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, sd_pcie);

	ret = sd_add_pcie_port(sd_pcie->pcie, pdev);
	if (ret < 0)
		return ret;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sd_pcie_suspend(struct device *dev)
{
	struct sd_pcie *sd_pcie = dev_get_drvdata(dev);

	phy_init_flag = 0;
	clk_disable_unprepare(sd_pcie->pcie_pclk);
	clk_disable_unprepare(sd_pcie->pcie_aclk);
	clk_disable_unprepare(sd_pcie->phy_pclk);
	clk_disable_unprepare(sd_pcie->phy_ref);
	return 0;
}

static int sd_pcie_resume(struct device *dev)
{
	struct sd_pcie *sd_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pcie = sd_pcie->pcie;

	sd_pcie_power_on(sd_pcie);
	sd_pcie_host_init(&pcie->pp);
	return 0;
}
#endif

static const struct dev_pm_ops sd_pcie_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sd_pcie_suspend, sd_pcie_resume)
};


static const struct of_device_id sd_pcie_match[] = {
	{.compatible = "semidrive,kunlun-pcie"},
	{},
};

static struct platform_driver sd_pcie_driver = {
	.probe = sd_pcie_probe,
	.driver = {
		.name = "sd-pcie",
		.of_match_table = sd_pcie_match,
		.suppress_bind_attrs = true,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.pm = &sd_pcie_pm_ops,
	},
};
builtin_platform_driver(sd_pcie_driver);
