/*
 * sdrv_wdt.c
 *
 *
 * Copyright(c); 2020 Semidrive
 *
 * Author: Yujin <jin.yu@semidrive.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/watchdog.h>
#include <linux/printk.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/pm.h>
#include <dt-bindings/soc/sdrv-common-reg.h>

#define SDRV_WDT_REBOOT_DELAY 0xffff
#define SDRV_WDT_PRE_DIV 1

/* wdt ctl */
#define WDG_CTRL_SOFT_RST_MASK		((unsigned) (1 << 0))
#define WDG_CTRL_SOFT_RST_SHIFT		(0U)
#define WDG_CTRL_SOFT_RST(x)		(((unsigned)(((unsigned)(x)) << WDG_CTRL_SOFT_RST_SHIFT)) & WDG_CTRL_SOFT_RST_MASK)

#define WDG_CTRL_WDG_EN_MASK		((unsigned) (1 << 1))

#define WDG_CTRL_CLK_SRC_MASK		(((unsigned) (1 << 2)) | ((unsigned) (1 << 3)) |((unsigned) (1 << 4)))

#define WDG_CTRL_WTC_SRC_MASK		((unsigned) (1 << 5))
#define WDG_CTRL_WTC_SRC_SHIFT		(5U)
#define WDG_CTRL_WTC_SRC(x)		(((unsigned)(((unsigned)(x)) << WDG_CTRL_WTC_SRC_SHIFT)) & WDG_CTRL_WTC_SRC_MASK)

#define WDG_CTRL_AUTO_RESTART_MASK	((unsigned) (1 << 6))

#define WDG_CTRL_WDG_EN_SRC_MASK	((unsigned) (1 << 8))
#define WDG_CTRL_WDG_EN_SRC_SHIFT	(8U)
#define WDG_CTRL_WDG_EN_SRC(x)		(((unsigned)(((unsigned)(x)) << WDG_CTRL_WDG_EN_SRC_SHIFT)) & WDG_CTRL_WDG_EN_SRC_MASK)

#define WDG_CTRL_WDG_EN_STA_MASK	((unsigned) (1 << 10))

#define WDG_CTRL_PRE_DIV_NUM_MASK	0xFFFF0000
#define WDG_CTRL_PRE_DIV_NUM_SHIFT	(16U)
#define WDG_CTRL_PRE_DIV_NUM(x)		(((unsigned)(((unsigned)(x)) << WDG_CTRL_PRE_DIV_NUM_SHIFT)) & WDG_CTRL_PRE_DIV_NUM_MASK)

/* wdt wrc ctl*/

#define WDG_WRC_CTRL_MODEM0_MASK	((unsigned) (1 << 0))
#define WDG_WRC_CTRL_MODEM1_MASK	((unsigned) (1 << 1))
#define WDG_WRC_CTRL_SEQ_REFR_MASK	((unsigned) (1 << 2))
#define WDG_WRC_CTRL_REFR_TRIG_MASK	((unsigned) (1 << 3))

/* wdt rst ctl */
#define WDG_RST_CTRL_RST_CNT_MASK	0x0000ffff
#define WDG_RST_CTRL_RST_CNT_SHIFT	(0U)
#define WDG_RST_CTRL_RST_CNT(x)		(((unsigned)(((unsigned)(x)) << WDG_RST_CTRL_RST_CNT_SHIFT)) & WDG_RST_CTRL_RST_CNT_MASK)
#define WDG_RST_CTRL_INT_RST_EN_MASK	((unsigned) (1 << 16))

#define WDG_RST_CTRL_INT_RST_MODE_MASK	((unsigned) (1 << 17))

/*RSTGEN_BOOT_REASON_REG:bit 0~3:bootreason, bit 4~7:wakeup source, bit 8~31: params */
#define BOOT_REASON_MASK	0xF

typedef enum {
	HALT_REASON_UNKNOWN = 0,
	HALT_REASON_POR,            // Cold-boot
	HALT_REASON_HW_WATCHDOG,    // HW watchdog timer
	HALT_REASON_LOWVOLTAGE,     // LV/Brownout condition
	HALT_REASON_HIGHVOLTAGE,    // High voltage condition.
	HALT_REASON_THERMAL,        // Thermal reason (probably overtemp)
	HALT_REASON_SW_RECOVERY,    // SW triggered reboot in order to enter recovery mode
	HALT_REASON_SW_RESET,       // Generic Software Initiated Reboot
	HALT_REASON_SW_WATCHDOG,    // Reboot triggered by a SW watchdog timer
	HALT_REASON_SW_PANIC,       // Reboot triggered by a SW panic or ASSERT
	HALT_REASON_SW_UPDATE,      // SW triggered reboot in order to begin firmware update
	HALT_REASON_SW_GLOBAL_POR,  // SW triggered global reboot
} platform_halt_reason;

struct halt_reason_desc {
	const char *desc;
	platform_halt_reason reason;
};

typedef enum wdg_clock_source {
	wdg_main_clk = 0,   // main clk, 3'b000
	wdg_bus_clk = 0x4,  // bus clk, 3'b001
	wdg_ext_clk = 0x8,  // ext clk, 3'b010
	wdg_lp_clk = 0x10   // lp clk, 3'b1xx
} wdg_clock_source_t;

struct sdrv_wdt_clk_struct {
	wdg_clock_source_t src;
	const char *name;
	int ration_to_ms;
};

struct sdrv_wdt_priv {
	struct device *dev;
	void __iomem *iobase;
	int irq;
	int dying_delay;
	const struct sdrv_wdt_clk_struct *wdt_clk;
	unsigned clk_div;
	int sig_rstgen;
};

extern void (*arm_pm_restart)(enum reboot_mode reboot_mode, const char *cmd);
extern void (*arm_pm_idle)(void);
extern void arch_flush_dcache_all(void);

static int sdrv_wdt_is_running(struct sdrv_wdt_priv *sdrv_wdt);
static int recovery_mode = 0;
static bool is_panic;
static bool global_reset;

static const struct sdrv_wdt_clk_struct wdt_clk[4] = {
	{
		.src = wdg_main_clk,
		.name = "main-clk",
		.ration_to_ms = 24000, /* 24MHz */
	},
	{
		.src = wdg_bus_clk,
		.name = "bus-clk",
		.ration_to_ms = 250000, /* 250MHz */
	},
	{
		.src = wdg_ext_clk,
		.name = "external-clk",
		.ration_to_ms = 26000, /* 26MHz */
	},
	{
		.src = wdg_lp_clk,
		.name = "xtal-clk",
		.ration_to_ms= 32, /* 32KHz */
	},
};

static const struct halt_reason_desc halt_reasons[] = {
	{ "bootloader",	HALT_REASON_SW_UPDATE },
	{ "shutdown",	HALT_REASON_POR },
	{ "recovery",	HALT_REASON_SW_RECOVERY },
	{ "soc",	HALT_REASON_SW_GLOBAL_POR },
	{ "reboot-ab-update",	HALT_REASON_SW_GLOBAL_POR },
	{ "panic",	HALT_REASON_SW_PANIC }
};

static int sdrv_wdt_start(struct watchdog_device *wdd)
{
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(wdd);
	unsigned val, clk_div, wtc_con;
	void __iomem *wdt_ctl = sdrv_wdt->iobase;
	void __iomem *wdt_wtc = wdt_ctl + 4;
	void __iomem *wdt_wrc_ctl = wdt_ctl + 8;
	void __iomem *wdt_wrc_val = wdt_ctl + 0xc;
	void __iomem *wdt_rst_ctl = wdt_ctl + 0x14;
	void __iomem *wdt_rst_int = wdt_ctl + 0x24;

	unsigned int timeout = wdd->timeout;

	/* set wdg_en_src in wdg_ctrl to 0x0 and clear wdg en src to 0x0 */
	writel(0, wdt_ctl);

	/* set wdg_en_src in wdg_ctrl to 0x1, select wdg src enable from register */
	val = readl(wdt_ctl);
	writel(val | WDG_CTRL_WDG_EN_SRC(1) | WDG_CTRL_WTC_SRC(1), wdt_ctl);

	/* wait until wdg_en_sta to 0x0 */
	while(readl(wdt_ctl) & WDG_CTRL_WDG_EN_STA_MASK);

	/* selcet clk src & div */
	val = readl(wdt_ctl);
	val &= ~WDG_CTRL_CLK_SRC_MASK;
	val |= sdrv_wdt->wdt_clk->src;

	clk_div = sdrv_wdt->clk_div;
	val |= WDG_CTRL_PRE_DIV_NUM(clk_div);

	/* disable auto restart */
	val &= ~WDG_CTRL_AUTO_RESTART_MASK;

	writel(val, wdt_ctl);

	/* wdt teminal count */
	timeout = wdd->timeout;
	timeout *= 1000;
	wtc_con = timeout * (sdrv_wdt->wdt_clk->ration_to_ms/(clk_div + 1));

	writel(wtc_con, wdt_wtc);

	/* refresh mode select */
	if (wdd->min_hw_heartbeat_ms) {
		/* wdt refresh window low limit */
		writel(wdd->min_hw_heartbeat_ms * (sdrv_wdt->wdt_clk->ration_to_ms/(clk_div + 1)),
				wdt_wrc_val);
		/* setup time window refresh mode */
		writel(WDG_WRC_CTRL_MODEM0_MASK | WDG_WRC_CTRL_REFR_TRIG_MASK
				| WDG_WRC_CTRL_MODEM1_MASK, wdt_wrc_ctl);
	} else {
		/* setup direct refresh mode */
		writel(WDG_WRC_CTRL_MODEM0_MASK|WDG_WRC_CTRL_REFR_TRIG_MASK, wdt_wrc_ctl);
	}

	/* delay of overflow before reset */
	val = WDG_RST_CTRL_RST_CNT(sdrv_wdt->dying_delay);

	/* wdt rest ctl */
	if (sdrv_wdt->sig_rstgen) {
		val |= WDG_RST_CTRL_INT_RST_EN_MASK;
		/* level mode */
		val &= ~WDG_RST_CTRL_INT_RST_MODE_MASK;
	}
	writel(val, wdt_rst_ctl);

	/* enable wdt overflow int */
	writel(0x4, wdt_rst_int);

	/* enable wct */
	val = readl(wdt_ctl);
	writel(val|WDG_CTRL_WDG_EN_MASK, wdt_ctl);

	while(!(readl(wdt_ctl) & WDG_CTRL_WDG_EN_STA_MASK));

	val = readl(wdt_ctl);
	writel(val|WDG_CTRL_SOFT_RST_MASK, wdt_ctl);

	while(readl(wdt_ctl) & WDG_CTRL_SOFT_RST_MASK);

	return 0;
}

static int sdrv_wdt_stop(struct watchdog_device *wdd)
{
	unsigned val;
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(wdd);
	void __iomem *wdt_ctl = sdrv_wdt->iobase;

	val = readl(wdt_ctl);

	val &= ~WDG_CTRL_WDG_EN_MASK;
	writel(val, wdt_ctl);

	while((readl(wdt_ctl) & WDG_CTRL_WDG_EN_STA_MASK));

	return 0;
}

static int sdrv_wdt_refresh(struct watchdog_device *wdd)
{
	void __iomem *wdt_ctl, *wdt_wrc_ctl;
	unsigned val;
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(wdd);
	wdt_ctl = sdrv_wdt->iobase;
	wdt_wrc_ctl = wdt_ctl + 8;
	val = readl(wdt_wrc_ctl);
	val |= WDG_WRC_CTRL_REFR_TRIG_MASK;
	writel(val, wdt_wrc_ctl);
	return 0;
}

static int sdrv_wdt_settimeout(struct watchdog_device *wdd, unsigned int to)
{
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(wdd);
	void __iomem *wdt_ctl = sdrv_wdt->iobase;
	void __iomem *wdt_wtc = wdt_ctl + 4;
	unsigned clk_div = sdrv_wdt->clk_div;
	unsigned wtc_con;

	wdd->timeout = to;
	to = to * 1000;
	wtc_con = to * (sdrv_wdt->wdt_clk->ration_to_ms/(clk_div + 1));
	writel(wtc_con, wdt_wtc);

	return 0;
}

static int sdrv_wdt_restart(struct  watchdog_device *wdd,
		unsigned long action, void *data)
{
	unsigned val;
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(wdd);
	void __iomem *wdt_ctl = sdrv_wdt->iobase;
	if (sdrv_wdt_is_running(sdrv_wdt))
		return 0;
	val = readl(wdt_ctl);
	writel(val|WDG_CTRL_WDG_EN_MASK, wdt_ctl);

	while(!(readl(wdt_ctl) & WDG_CTRL_WDG_EN_STA_MASK));

	val |= WDG_CTRL_SOFT_RST_MASK;
	writel(val, wdt_ctl);

	while(readl(wdt_ctl) & WDG_CTRL_SOFT_RST_MASK);

	return 0;
}

static irqreturn_t sdrv_wdt_irq(int irq, void *devid)
{
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(devid);
	void __iomem *wdt_ctl = sdrv_wdt->iobase;
	void __iomem *wdt_rst_int =  wdt_ctl + 0x24;
	unsigned int val = readl(wdt_rst_int);

	if (val & 0x20) {
		pr_err("wdt overflow occured\n");
		/* TODO , save context */

		/* clear int bit */
		val |= 0x100;
		writel(val, wdt_rst_int);
	}

	return IRQ_HANDLED;
}

static const struct watchdog_info sdrv_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING
		| WDIOF_MAGICCLOSE,
	.identity =	"Semidrive Watchdog",
};

static const struct watchdog_ops sdrv_wdt_ops = {
	.owner = THIS_MODULE,
	.start = sdrv_wdt_start,
	.stop =	sdrv_wdt_stop,
	.ping = sdrv_wdt_refresh,
	.set_timeout = sdrv_wdt_settimeout,
	.restart = sdrv_wdt_restart,
};

static struct watchdog_device sdrv_wtd_wdd = {
	.info = &sdrv_wdt_info,
	.ops = &sdrv_wdt_ops,
	.min_timeout = 1,
	.max_timeout = 10,
};

static int sdrv_wdt_is_running(struct sdrv_wdt_priv *sdrv_wdt)
{
	void __iomem *wdt_ctl = sdrv_wdt->iobase;
	return !!((readl(wdt_ctl) & WDG_CTRL_WDG_EN_STA_MASK)
			&& (readl(wdt_ctl + 0x4) != readl(wdt_ctl + 0x1c)));
}
void sdrv_set_bootreason(enum reboot_mode reboot_mode, const char *cmd)
{
	struct of_phandle_args args;
	struct device_node *np;
	struct regmap *regmap;
	struct device_node *regctl_node;
	struct device *regctl_dev;
	int ret, reason, val;
	struct sdrv_wdt_priv *sdrv_wdt;

	if (recovery_mode  || global_reset)
		reason = HALT_REASON_SW_GLOBAL_POR;
	else
		reason = HALT_REASON_SW_RESET;

	if (cmd) {
		size_t i;
		for (i = 0; i < ARRAY_SIZE(halt_reasons); i++) {
			if (!strcmp(cmd, halt_reasons[i].desc)) {
				reason = halt_reasons[i].reason;
				break;
			}
		}
	} else {
		if (is_panic)
			reason = HALT_REASON_SW_PANIC;
	}

	sdrv_wdt = watchdog_get_drvdata(&sdrv_wtd_wdd);
	if (!sdrv_wdt) {
		pr_err("no watchdog dev, ignore\n");
		return;
	}
	np = sdrv_wdt->dev->of_node;

	ret = of_parse_phandle_with_args(np, "regctl",
			"#regctl-cells", 0, &args);
	if (!ret) {
		regctl_node = args.np;
		regctl_dev =  regctl_node->data;
		regmap = dev_get_regmap(regctl_dev, NULL);
		if (regmap) {
			ret = regmap_read(regmap, SDRV_REG_BOOTREASON, &val);
			if (!ret) {
				val &= ~BOOT_REASON_MASK;
				val |= reason;
				regmap_write(regmap, SDRV_REG_BOOTREASON, val);
			} else {
				pr_err("failed to save reboot reason\n");
			}
			ret = regmap_write(regmap, SDRV_REG_STATUS, 0);
			if (ret)
				pr_err("failed to clear status register\n");
		}
		of_node_put(args.np);
	} else {
		pr_err("boot reg no found from dts\n");
	}
}
void sdrv_restart_without_reason(void)
{
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(&sdrv_wtd_wdd);

	pr_emerg("flush all cache when reboot\n");
	arch_flush_dcache_all();
	pr_emerg("flush all cache done when reboot\n");

	if (!sdrv_wdt) {
		pr_err("no watchdog dev, ignore\n");
		return;
	}

	if (!sdrv_wdt_is_running(sdrv_wdt)) {
		/* run watchdog, and don't kick it */
		sdrv_wdt_start(&sdrv_wtd_wdd);
	} else {
		/* kick watchdog with an impossible delay */
		sdrv_wtd_wdd.min_hw_heartbeat_ms =  0xffffffff;
	}
}
static int sdrv_wdt_panic_handler(struct notifier_block *this,
				  unsigned long event, void *unused)
{
	is_panic = true;
	sdrv_set_bootreason(0, "panic");
	sdrv_wdt_refresh(&sdrv_wtd_wdd);

	return NOTIFY_OK;
}

static struct notifier_block sdrv_wdt_panic_event_nb = {
	.notifier_call= sdrv_wdt_panic_handler,
	.priority= INT_MAX,
};

void sdrv_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	sdrv_set_bootreason(reboot_mode, cmd);
	sdrv_restart_without_reason();
}

void sdrv_poweroff(void)
{
	struct sdrv_wdt_priv *sdrv_wdt = watchdog_get_drvdata(&sdrv_wtd_wdd);

	if (!sdrv_wdt) {
		pr_err("no watchdog dev, ignore\n");
		return;
	}

	sdrv_set_bootreason(0, "shutdown");

	if (!sdrv_wdt_is_running(sdrv_wdt)) {
		/* run watchdog, and don't kick it */
		sdrv_wdt_start(&sdrv_wtd_wdd);
	} else {
		/* kick watchdog with an impossible delay */
		sdrv_wtd_wdd.min_hw_heartbeat_ms =  0xffffffff;
	}
}

static int sdrv_wdt_probe(struct platform_device *pdev)
{
	struct sdrv_wdt_priv *sdrv_wdt;
	struct resource *res;
	struct device_node *np;
	const char *str = NULL;
	unsigned clk_div, hearbeat;
	int i, ret;

	sdrv_wdt = devm_kzalloc(&pdev->dev, sizeof(*sdrv_wdt), GFP_KERNEL);
	if (!sdrv_wdt)
		return -ENOMEM;

	sdrv_wdt->dev = &pdev->dev;
	np = pdev->dev.of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sdrv_wdt->iobase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sdrv_wdt->iobase))
		return PTR_ERR(sdrv_wdt->iobase);

	watchdog_set_drvdata(&sdrv_wtd_wdd, sdrv_wdt);

	sdrv_wtd_wdd.parent = &pdev->dev;

	/* get wdt clock scr */
	ret = of_property_read_string(np, "wdt,clock-source", &str);

	if (str && !ret) {
		for (i = 0; i < sizeof(wdt_clk)/sizeof(wdt_clk[0]); i++)
		{
			if (!strcmp(wdt_clk[i].name, str))
			{
				sdrv_wdt->wdt_clk = &wdt_clk[i];
			}
		}
	} else {
		dev_info(&pdev->dev,
				"wdt clock source not specified, select main clock\n");
		sdrv_wdt->wdt_clk = &wdt_clk[0];
	}

	/* get wdt clk prescaler divider */
	ret = of_property_read_u32(np, "wdt,clock-divider", &clk_div);

	if (!ret) {
		sdrv_wdt->clk_div = clk_div;
	} else {
		dev_info(&pdev->dev, "wdt clock divider not specified, don't divide\n");
		sdrv_wdt->clk_div = 0;
	}

	/* get min hw hearbeat in ms */
	ret = of_property_read_u32(np, "wdt,min-hw-hearbeat", &hearbeat);
	if (!ret) {
		sdrv_wtd_wdd.min_hw_heartbeat_ms = hearbeat;
	}

	/* get max hw hearbeat in ms */
	ret = of_property_read_u32(np, "wdt,max-hw-hearbeat", &hearbeat);
	if (!ret) {
		sdrv_wtd_wdd.max_hw_heartbeat_ms = hearbeat;
	}

	/* get rest after overflow occured */
	ret = of_property_read_bool(np, "wdt,dying-delay");
	if (ret) {
		sdrv_wdt->dying_delay = SDRV_WDT_REBOOT_DELAY;
	}

	/* from dts timeout-sec node */
	watchdog_init_timeout(&sdrv_wtd_wdd, 0, &pdev->dev);

	/* domain reset */
	str = NULL;
	if (!of_property_read_string(np, "wdt,sig-rstgen", &str)) {
		if (str && !strcmp(str, "true"))
			sdrv_wdt->sig_rstgen = 1;
	}

	watchdog_set_nowayout(&sdrv_wtd_wdd, false);

	watchdog_set_restart_priority(&sdrv_wtd_wdd, 128);

	watchdog_stop_on_reboot(&sdrv_wtd_wdd);

	platform_set_drvdata(pdev, &sdrv_wtd_wdd);

	sdrv_wdt->irq = platform_get_irq(pdev, 0);
	if (sdrv_wdt->irq > 0)
		ret = devm_request_irq(&pdev->dev, sdrv_wdt->irq,
					sdrv_wdt_irq, 0, pdev->name, &sdrv_wtd_wdd);
	str = NULL;
	if (!of_property_read_string(np, "wdt,auto-run", &str)) {
		if (str && !strcmp(str, "true")) {
			sdrv_wdt_start(&sdrv_wtd_wdd);

			set_bit(WDOG_HW_RUNNING, &sdrv_wtd_wdd.status);

			ret = devm_watchdog_register_device(&pdev->dev, &sdrv_wtd_wdd);
			if (ret) {
				dev_err(&pdev->dev, "Failed to register watchdog device");
				return ret;
			}
		}else{
			sdrv_wdt_stop(&sdrv_wtd_wdd);
		}
	}else{
		sdrv_wdt_stop(&sdrv_wtd_wdd);
	}

	atomic_notifier_chain_register(&panic_notifier_list, &sdrv_wdt_panic_event_nb);

	if (!pm_power_off)
		pm_power_off =  &sdrv_poweroff;

	if (!arm_pm_restart)
		arm_pm_restart =  &sdrv_restart;
	return 0;
}

static int sdrv_wdt_remove(struct platform_device *pdev)
{
	struct watchdog_device *wdd = platform_get_drvdata(pdev);
	sdrv_wdt_stop(wdd);
	watchdog_unregister_device(wdd);
	return 0;
}

static void sdrv_wdt_shutdown(struct platform_device *pdev)
{
	struct watchdog_device *wdd = platform_get_drvdata(pdev);

	if (!watchdog_active(wdd) && !watchdog_hw_running(wdd))
		return;

	sdrv_wdt_stop(wdd);
}

#ifdef CONFIG_PM_SLEEP
static int sdrv_wdt_suspend(struct device *dev)
{
	struct watchdog_device *wdd = dev_get_drvdata(dev);

	if (!watchdog_active(wdd) && !watchdog_hw_running(wdd))
		return 0;

	sdrv_wdt_stop(wdd);

	return 0;
}

static int sdrv_wdt_resume(struct device *dev)
{
	struct watchdog_device *wdd = dev_get_drvdata(dev);

	if (!watchdog_active(wdd) && !watchdog_hw_running(wdd))
		return 0;

	sdrv_wdt_start(wdd);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sdrv_wdt_pm_ops, sdrv_wdt_suspend,
		        sdrv_wdt_resume);
#endif

static const struct of_device_id sdrv_wdt_of_match[] = {
	{ .compatible = "semidrive,watchdog" },
	{ /* sentinel */ },
};

static struct platform_driver sdrv_wdt_driver = {
	.driver = {
		.name = "sdrv-wdt",
		.of_match_table = sdrv_wdt_of_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &sdrv_wdt_pm_ops,
#endif
	},
	.probe = sdrv_wdt_probe,
	.remove = sdrv_wdt_remove,
	.shutdown = sdrv_wdt_shutdown,
};

static int __init set_recovery_mode(char *str)
{
	recovery_mode = 1;
	return 0;
}

__setup("recovery_mode", set_recovery_mode);

static int __init enable_global_reset(char *str)
{
	global_reset = true;
	return 0;
}

__setup("global_reset", enable_global_reset);

builtin_platform_driver(sdrv_wdt_driver);
