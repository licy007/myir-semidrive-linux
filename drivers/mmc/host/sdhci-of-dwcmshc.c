// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Synopsys DesignWare Cores Mobile Storage Host Controller
 *
 * Copyright (C) 2018 Synaptics Incorporated
 *
 * Author: Jisheng Zhang <jszhang@kernel.org>
 */

#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/mmc/mmc.h>

#include <soc/semidrive/sdrv_scr.h>
#include "sdhci-pltfm.h"

#define SDHCI_ARG2_STUFF GENMASK(31, 16)

#define SDHCI_VENDOR_BASE_REG (0xE8)

#define SDHCI_VENDER_EMMC_CTRL_REG (0x2C)
#define SDHCI_IS_EMMC_CARD_MASK BIT(0)
#define SDHCI_EMMC_RST_N_MASK BIT(2)
#define SDHCI_EMMC_RST_N_OE_MASK BIT(3)
#define SDHCI_EMMC_STROBE_EN_MASK BIT(8)

#define SDHCI_VENDER_AT_CTRL_REG (0x40)
#define SDHCI_TUNE_CLK_STOP_EN_MASK BIT(16)
#define SDHCI_TUNE_SWIN_TH_VAL_LSB (24)
#define SDHCI_TUNE_SWIN_TH_VAL_MASK (0xFF)

#define DWC_MSHC_PTR_PHY_REGS 0x300
#define DWC_MSHC_PHY_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x0)
#define PAD_SN_LSB 20
#define PAD_SN_MASK 0xF
#define PAD_SN_DEFAULT ((0x8 & PAD_SN_MASK) << PAD_SN_LSB)
#define PAD_SP_LSB 16
#define PAD_SP_MASK 0xF
#define PAD_SP_DEFAULT ((0x9 & PAD_SP_MASK) << PAD_SP_LSB)
#define PHY_PWRGOOD BIT(1)
#define PHY_RSTN BIT(0)

#define DWC_MSHC_CMDPAD_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x4)
#define DWC_MSHC_DATPAD_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x6)
#define DWC_MSHC_CLKPAD_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x8)
#define DWC_MSHC_STBPAD_CNFG (DWC_MSHC_PTR_PHY_REGS + 0xA)
#define DWC_MSHC_RSTNPAD_CNFG (DWC_MSHC_PTR_PHY_REGS + 0xC)
#define TXSLEW_N_LSB 9
#define TXSLEW_N_MASK 0xF
#define TXSLEW_P_LSB 5
#define TXSLEW_P_MASK 0xF
#define WEAKPULL_EN_LSB 3
#define WEAKPULL_EN_MASK 0x3
#define RXSEL_LSB 0
#define RXSEL_MASK 0x3

#define DWC_MSHC_COMMDL_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x1C)
#define DWC_MSHC_SDCLKDL_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x1D)
#define DWC_MSHC_SDCLKDL_DC (DWC_MSHC_PTR_PHY_REGS + 0x1E)
#define DWC_MSHC_SMPLDL_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x20)
#define DWC_MSHC_ATDL_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x21)

#define DWC_MSHC_DLL_CTRL (DWC_MSHC_PTR_PHY_REGS + 0x24)
#define DWC_MSHC_DLL_CNFG1 (DWC_MSHC_PTR_PHY_REGS + 0x25)
#define DWC_MSHC_DLL_CNFG2 (DWC_MSHC_PTR_PHY_REGS + 0x26)
#define DWC_MSHC_DLLDL_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x28)
#define DWC_MSHC_DLL_OFFSET (DWC_MSHC_PTR_PHY_REGS + 0x29)
#define DWC_MSHC_DLLLBT_CNFG (DWC_MSHC_PTR_PHY_REGS + 0x2C)
#define DWC_MSHC_DLL_STATUS (DWC_MSHC_PTR_PHY_REGS + 0x2E)
#define ERROR_STS BIT(1)
#define LOCK_STS BIT(0)

#define DWC_MSHC_PHY_PAD_SD_CLK                                                \
	((1 << TXSLEW_N_LSB) | (3 << TXSLEW_P_LSB) | (0 << WEAKPULL_EN_LSB) |  \
	 (1 << RXSEL_LSB))
#define DWC_MSHC_PHY_PAD_SD_DAT                                                \
	((1 << TXSLEW_N_LSB) | (3 << TXSLEW_P_LSB) | (1 << WEAKPULL_EN_LSB) |  \
	 (1 << RXSEL_LSB))
#define DWC_MSHC_PHY_PAD_SD_STB                                                \
	((1 << TXSLEW_N_LSB) | (3 << TXSLEW_P_LSB) | (2 << WEAKPULL_EN_LSB) |  \
	 (1 << RXSEL_LSB))

#define DWC_MSHC_PHY_PAD_EMMC_CLK                                              \
	((2 << TXSLEW_N_LSB) | (2 << TXSLEW_P_LSB) | (0 << WEAKPULL_EN_LSB) |  \
	 (0 << RXSEL_LSB))
#define DWC_MSHC_PHY_PAD_EMMC_DAT                                              \
	((2 << TXSLEW_N_LSB) | (2 << TXSLEW_P_LSB) | (1 << WEAKPULL_EN_LSB) |  \
	 (1 << RXSEL_LSB))
#define DWC_MSHC_PHY_PAD_EMMC_STB                                              \
	((2 << TXSLEW_N_LSB) | (2 << TXSLEW_P_LSB) | (2 << WEAKPULL_EN_LSB) |  \
	 (1 << RXSEL_LSB))

struct dwcmshc_priv {
	/* bus clock */
	struct clk	*bus_clk;
	u32	 scr_signals_ddr;
	bool	    card_is_emmc;
};

extern void mmc_set_clock(struct mmc_host *host, unsigned int hz);

static void dwcmshc_phy_pad_config(struct sdhci_host *host)
{
	u16 clk_ctrl;
	struct sdhci_pltfm_host *pltfm_host;
	struct dwcmshc_priv *priv;

	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	/* Disable the card clock */
	clk_ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk_ctrl &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk_ctrl, SDHCI_CLOCK_CONTROL);

	if (priv->card_is_emmc) {
		sdhci_writew(host, DWC_MSHC_PHY_PAD_EMMC_DAT, DWC_MSHC_CMDPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_EMMC_DAT, DWC_MSHC_DATPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_EMMC_CLK, DWC_MSHC_CLKPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_EMMC_STB, DWC_MSHC_STBPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_EMMC_DAT, DWC_MSHC_RSTNPAD_CNFG);
	} else {
		sdhci_writew(host, DWC_MSHC_PHY_PAD_SD_DAT, DWC_MSHC_CMDPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_SD_DAT, DWC_MSHC_DATPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_SD_CLK, DWC_MSHC_CLKPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_SD_STB, DWC_MSHC_STBPAD_CNFG);
		sdhci_writew(host, DWC_MSHC_PHY_PAD_SD_DAT, DWC_MSHC_RSTNPAD_CNFG);
	}

	return;
}

static inline void dwcmshc_phy_delay_config(struct sdhci_host *host)
{
	sdhci_writeb(host, 1, DWC_MSHC_COMMDL_CNFG);
	sdhci_writeb(host, 0, DWC_MSHC_SDCLKDL_CNFG);
	sdhci_writeb(host, 8, DWC_MSHC_SMPLDL_CNFG);
	sdhci_writeb(host, 8, DWC_MSHC_ATDL_CNFG);
	return;
}

static int dwcmshc_phy_dll_config(struct sdhci_host *host)
{
	u16 clk_ctrl;
	u32 reg;
	ktime_t timeout;

	sdhci_writeb(host, 0, DWC_MSHC_DLL_CTRL);

	/* Disable the card clock */
	clk_ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk_ctrl &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk_ctrl, SDHCI_CLOCK_CONTROL);

	dwcmshc_phy_delay_config(host);

	sdhci_writeb(host, 0x20, DWC_MSHC_DLL_CNFG1);
	// TODO: set the dll value by real chip
	sdhci_writeb(host, 0x0, DWC_MSHC_DLL_CNFG2);
	sdhci_writeb(host, 0x60, DWC_MSHC_DLLDL_CNFG);
	sdhci_writeb(host, 0x0, DWC_MSHC_DLL_OFFSET);
	sdhci_writew(host, 0x0, DWC_MSHC_DLLLBT_CNFG);

	/* Enable the clock */
	clk_ctrl |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk_ctrl, SDHCI_CLOCK_CONTROL);

	sdhci_writeb(host, 1, DWC_MSHC_DLL_CTRL);

	/* Wait max 150 ms */
	timeout = ktime_add_ms(ktime_get(), 150);
	while (1) {
		bool timedout = ktime_after(ktime_get(), timeout);

		reg = sdhci_readb(host, DWC_MSHC_DLL_STATUS);
		if (reg & LOCK_STS)
			break;
		if (timedout) {
			pr_err("%s: dwcmshc wait for phy dll lock timeout!\n",
				   mmc_hostname(host->mmc));
			sdhci_dumpregs(host);
			return -1;
		}
		udelay(10);
	}

	reg = sdhci_readb(host, DWC_MSHC_DLL_STATUS);
	if (reg & ERROR_STS)
		return -1;

	return 0;
}

static void dwcmshc_phy_reset(struct sdhci_host *host)
{
	/* reset phy */
	sdhci_writew(host, 0, DWC_MSHC_PHY_CNFG);
}

static int dwcmshc_phy_init(struct sdhci_host *host)
{
	u32 reg;
	ktime_t timeout;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	/* reset phy */
	sdhci_writew(host, 0, DWC_MSHC_PHY_CNFG);

	/* Disable the clock */
	clk_disable_unprepare(pltfm_host->clk);
	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	dwcmshc_phy_pad_config(host);
	dwcmshc_phy_delay_config(host);

	/* Wait max 150 ms */
	timeout = ktime_add_ms(ktime_get(), 150);
	while (1) {
		bool timedout = ktime_after(ktime_get(), timeout);

		reg = sdhci_readl(host, DWC_MSHC_PHY_CNFG);
		if (reg & PHY_PWRGOOD)
			break;
		if (timedout) {
			pr_err("%s: dwcmshc wait for phy power good timeout!\n",
				   mmc_hostname(host->mmc));
			sdhci_dumpregs(host);
			return -1;
		}
		udelay(10);
	}

	reg = PAD_SN_DEFAULT | PAD_SP_DEFAULT;
	sdhci_writel(host, reg, DWC_MSHC_PHY_CNFG);

	/* de-assert the phy */
	reg |= PHY_RSTN;
	sdhci_writel(host, reg, DWC_MSHC_PHY_CNFG);

	clk_prepare_enable(pltfm_host->clk);
	/* Enable the clock */
	sdhci_enable_clk(host, 0);

	return 0;
}


static void emmc_card_init(struct sdhci_host *host)
{
	u16 reg;
	u16 vender_base;
	struct sdhci_pltfm_host *pltfm_host;
        struct dwcmshc_priv *priv;

	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	/* read verder base register address */
	vender_base = sdhci_readw(host, SDHCI_VENDOR_BASE_REG) & 0xFFF;

	reg = sdhci_readw(host, vender_base + SDHCI_VENDER_EMMC_CTRL_REG);
	reg &= ~SDHCI_IS_EMMC_CARD_MASK;
	reg |= priv->card_is_emmc;
	sdhci_writew(host, reg, vender_base + SDHCI_VENDER_EMMC_CTRL_REG);
}

static inline void dwcmshc_auto_tuning_set(struct sdhci_host *host)
{
	u32 reg;
	u16 vender_base;

	/* read verder base register address */
	vender_base = sdhci_readw(host, SDHCI_VENDOR_BASE_REG) & 0xFFF;

	reg = sdhci_readl(host, vender_base + SDHCI_VENDER_AT_CTRL_REG);
	reg &= ~(SDHCI_TUNE_SWIN_TH_VAL_MASK << SDHCI_TUNE_SWIN_TH_VAL_LSB);
	reg |= (0xF << SDHCI_TUNE_SWIN_TH_VAL_LSB);
	reg |= SDHCI_TUNE_CLK_STOP_EN_MASK;
	sdhci_writel(host, reg, vender_base + SDHCI_VENDER_AT_CTRL_REG);
}

static void dwcmshc_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host;
	pltfm_host = sdhci_priv(host);

	host->mmc->actual_clock = 0;

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		return;

	/*
	 * Beacuse the clock will be 2 dvider by mshc model,
	 * so we need twice base frequency.
	 */
	if (host->quirks & SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN) {
		if (!clk_set_rate(pltfm_host->clk, clock * 2))
			host->mmc->actual_clock = clk_get_rate(pltfm_host->clk) / 2;
	} else {
		host->mmc->actual_clock = clock;
	}
	pr_debug("%s: Set clock = %d, actual clock = %d\n",
		mmc_hostname(host->mmc), clock, host->mmc->actual_clock);

	sdhci_enable_clk(host, 0);
}

static void set_ddr_mode(struct sdhci_host *host, unsigned int ddr_mode)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct dwcmshc_priv *priv;

	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	if (priv->scr_signals_ddr)
		sdrv_scr_set(SCR_SEC, priv->scr_signals_ddr, ddr_mode);
}

static void dwcmshc_check_auto_cmd23(struct mmc_host *mmc,
				    struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);

	/*
	* No matter V4 is enabled or not, ARGUMENT2 register is 32-bit
	* block count register which doesn't support stuff bits of
	* CMD23 argument on dwcmsch host controller.
	*/
	if (mrq->sbc && (mrq->sbc->arg & SDHCI_ARG2_STUFF))
		host->flags &= ~SDHCI_AUTO_CMD23;
	else
		host->flags |= SDHCI_AUTO_CMD23;
}

static void dwcmshc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	dwcmshc_check_auto_cmd23(mmc, mrq);

	sdhci_request(mmc, mrq);
}

static void dwcmshc_set_uhs_signaling(struct sdhci_host *host, unsigned timing)
{
	u32 ddr_mode = 0;

	sdhci_set_uhs_signaling(host, timing);

	if ((timing > MMC_TIMING_UHS_DDR50) && (timing != MMC_TIMING_MMC_HS200))
		ddr_mode = 1;
	set_ddr_mode(host, ddr_mode);

	if ((timing > MMC_TIMING_MMC_HS200) &&
			(host->mmc->actual_clock > 100000000)) {
		if (dwcmshc_phy_dll_config(host))
			pr_err("%s: phy dll config failed!\n", mmc_hostname(host->mmc));
	}
}

static void dwcmshc_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	if (mask & SDHCI_RESET_ALL) {
		/* Hold phy reset when software reset host. */
		dwcmshc_phy_reset(host);
	}

	sdhci_reset(host, mask);

	if (mask & SDHCI_RESET_ALL) {
		emmc_card_init(host);
	}
}

static void dwcmshc_hw_reset(struct sdhci_host *host)
{
	u16 reg;
	u16 vender_base;

	pr_debug("%s: hw reset\n", mmc_hostname(host->mmc));
	/* read verder base register address */
	vender_base = sdhci_readw(host, SDHCI_VENDOR_BASE_REG) & 0xFFF;

	reg = sdhci_readw(host, vender_base + SDHCI_VENDER_EMMC_CTRL_REG);
	reg |= SDHCI_EMMC_RST_N_OE_MASK;
	reg &= ~SDHCI_EMMC_RST_N_MASK;
	sdhci_writew(host, reg, vender_base + SDHCI_VENDER_EMMC_CTRL_REG);

	udelay(5);
	reg |= SDHCI_EMMC_RST_N_MASK;
	sdhci_writew(host, reg, vender_base + SDHCI_VENDER_EMMC_CTRL_REG);
}

int dwcmshc_start_signal_voltage_switch(struct mmc_host *mmc,
				      struct mmc_ios *ios)
{
	int ret;
	u32 phy_cnfg_reg;
	struct sdhci_host *host = mmc_priv(mmc);

	ret = sdhci_start_signal_voltage_switch(mmc, ios);

	/* When switch signal voltage success, need init mshc phy. */
	if (!ret) {
		phy_cnfg_reg = sdhci_readw(host, DWC_MSHC_PHY_CNFG);
		if ((phy_cnfg_reg & PHY_RSTN) == 0u) {
			dwcmshc_phy_init(host);
		}
	}

	return ret;
}

static void dwcmshc_get_property(struct platform_device *pdev, struct dwcmshc_priv *priv)
{
	struct device *dev = &pdev->dev;
	u32 scr_signal;

	if (device_property_present(dev, "card-is-emmc"))
		priv->card_is_emmc = 1;
	else
		priv->card_is_emmc = 0;

	if (!device_property_read_u32(dev, "sdrv,scr_signals_ddr", &scr_signal))
		priv->scr_signals_ddr = scr_signal;
	dev_info(&pdev->dev, "the ddr mode scr signal = %d\n", priv->scr_signals_ddr);

}

static unsigned int dwcmshc_get_max_clock(struct sdhci_host *host)
{
	return host->mmc->f_max;
}

static int __dwcmshc_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	int i;

	/*
	 * Issue opcode repeatedly till Execute Tuning is set to 0 or the number
	 * of loops reaches tuning loop count.
	 */
	for (i = 0; i < host->tuning_loop_count; i++) {
		u16 ctrl;

		sdhci_send_tuning(host, opcode);

		if (!host->tuning_done) {
			pr_info("%s: Tuning timeout, falling back to fixed sampling clock\n",
				mmc_hostname(host->mmc));
			sdhci_abort_tuning(host, opcode);
			return -ETIMEDOUT;
		}

		/* Spec does not require a delay between tuning cycles */
		if (host->tuning_delay > 0)
			mdelay(host->tuning_delay);

		ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		if (!(ctrl & SDHCI_CTRL_EXEC_TUNING)) {
			if (ctrl & SDHCI_CTRL_TUNED_CLK)
				return 0; /* Success! */
			break;
		}
	}

	pr_info("%s: Tuning failed, falling back to fixed sampling clock\n",
		mmc_hostname(host->mmc));
	sdhci_reset_tuning(host);
	return -EAGAIN;
}

static int dwcmshc_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	u16 clk_ctrl;

        if (host->flags & SDHCI_HS400_TUNING)
		host->mmc->retune_period = 0;

	if (host->tuning_delay < 0)
		host->tuning_delay = opcode == MMC_SEND_TUNING_BLOCK;

	sdhci_reset_tuning(host);
	dwcmshc_auto_tuning_set(host);
	dwcmshc_sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	/*
	 * For avoid glitches, need disable the card clock before set
	 * EXEC_TUNING bit.
	 */
	clk_ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk_ctrl &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk_ctrl, SDHCI_CLOCK_CONTROL);

	sdhci_start_tuning(host);

	/* enable the card clock */
	clk_ctrl |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk_ctrl, SDHCI_CLOCK_CONTROL);

	host->tuning_err = __dwcmshc_execute_tuning(host, opcode);

	/*
	 * For avoid glitches, need disable the card clock after sample
	 * clk select.
	 */
	clk_ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk_ctrl &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk_ctrl, SDHCI_CLOCK_CONTROL);

	/* enable the card clock */
	clk_ctrl |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk_ctrl, SDHCI_CLOCK_CONTROL);

	sdhci_end_tuning(host);

	return 0;
}

static void dwcmshc_hs400_enhanced_strobe(struct mmc_host *mmc, struct mmc_ios *ios)
{
	u16 reg;
	u16 vender_base;
	bool enhanced_strobe = 0;
	struct sdhci_host *host = mmc_priv(mmc);

	pr_debug("%s: hs400 enhanced strobe\n", mmc_hostname(host->mmc));

	enhanced_strobe = mmc->ios.enhanced_strobe;
	/* After hs400E is enabled, must switch the clock, */
	if(enhanced_strobe) {
		mmc_set_clock(host->mmc, MMC_HS200_MAX_DTR);
	}
	/* read verder base register address */
	vender_base = sdhci_readw(host, SDHCI_VENDOR_BASE_REG) & 0xFFF;

	reg = sdhci_readw(host, vender_base + SDHCI_VENDER_EMMC_CTRL_REG);
	if(enhanced_strobe)
		reg |= SDHCI_EMMC_STROBE_EN_MASK;
	else
		reg &= ~SDHCI_EMMC_STROBE_EN_MASK;
	sdhci_writew(host, reg, vender_base + SDHCI_VENDER_EMMC_CTRL_REG);
}

static const struct sdhci_ops sdhci_dwcmshc_ops = {
	.set_clock		= dwcmshc_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= dwcmshc_get_max_clock,
	.reset			= dwcmshc_sdhci_reset,
	.hw_reset = dwcmshc_hw_reset,
	.platform_execute_tuning = dwcmshc_execute_tuning,
};

static const struct sdhci_pltfm_data sdhci_dwcmshc_pdata = {
	.ops = &sdhci_dwcmshc_ops,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN | SDHCI_QUIRK2_SLOW_USE_PIO,
};

static int dwcmshc_probe(struct platform_device *pdev)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_host *host;
	struct dwcmshc_priv *priv;
	int err;

	host = sdhci_pltfm_init(pdev, &sdhci_dwcmshc_pdata,
				sizeof(struct dwcmshc_priv));
	if (IS_ERR(host))
		return PTR_ERR(host);

	host->mmc_host_ops.start_signal_voltage_switch =
		dwcmshc_start_signal_voltage_switch;
	host->mmc_host_ops.hs400_enhanced_strobe =
		dwcmshc_hs400_enhanced_strobe;

	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	pltfm_host->clk = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR(pltfm_host->clk)) {
		err = PTR_ERR(pltfm_host->clk);
		dev_err(&pdev->dev, "failed to get core clk: %d\n", err);
		goto free_pltfm;
	} else {
		err = clk_prepare_enable(pltfm_host->clk);
		if (err)
			goto free_pltfm;
		host->quirks |= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN;
	}

	priv->bus_clk = devm_clk_get(&pdev->dev, "bus");
	if (!IS_ERR(priv->bus_clk))
		clk_prepare_enable(priv->bus_clk);

	err = mmc_of_parse(host->mmc);
	if (err)
		goto err_clk;

	sdhci_get_of_property(pdev);

	dwcmshc_get_property(pdev, priv);

	sdhci_enable_v4_mode(host);

	host->mmc_host_ops.request = dwcmshc_request;

	err = sdhci_add_host(host);
	if (err)
		goto err_clk;

	return 0;

err_clk:
	clk_disable_unprepare(pltfm_host->clk);
	if (!IS_ERR(priv->bus_clk))
		clk_disable_unprepare(priv->bus_clk);
free_pltfm:
	sdhci_pltfm_free(pdev);
	return err;
}

static int dwcmshc_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);

	sdhci_remove_host(host, 0);

	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(priv->bus_clk);

	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwcmshc_of_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	if (host->tuning_mode != SDHCI_TUNING_MODE_3)
                mmc_retune_needed(host->mmc);

	return sdhci_suspend_host(host);
}

static int dwcmshc_of_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_resume_host(host);
}
#endif

static SIMPLE_DEV_PM_OPS(dwcmshc_of_dev_pm_ops, dwcmshc_of_suspend,
			dwcmshc_of_resume);

static const struct of_device_id sdhci_dwcmshc_dt_ids[] = {
	{ .compatible = "snps,dwcmshc-sdhci" },
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_dwcmshc_dt_ids);

static struct platform_driver sdhci_dwcmshc_driver = {
	.driver	= {
		.name	= "sdhci-dwcmshc",
		.of_match_table = sdhci_dwcmshc_dt_ids,
		.pm = &dwcmshc_of_dev_pm_ops,
	},
	.probe	= dwcmshc_probe,
	.remove	= dwcmshc_remove,
};
module_platform_driver(sdhci_dwcmshc_driver);

MODULE_DESCRIPTION("SDHCI platform driver for Synopsys DWC MSHC");
MODULE_AUTHOR("Jisheng Zhang <jszhang@kernel.org>");
MODULE_LICENSE("GPL v2");
