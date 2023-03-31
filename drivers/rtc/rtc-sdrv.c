/*
 * sdrv_scr.c
 *
 * Copyright(c) 2020 Semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/spinlock.h>
#include <linux/log2.h>
#include <linux/pm_wakeirq.h>
#include "rtc-sdrv.h"

#define RTC_CLK_HZ	(32 * 1024)
#define RTC_CLK_SHIFT   ilog2(RTC_CLK_HZ)
#define US_HZ		(1000 * 1000)
#define SYNC_DELAY_US	DIV_ROUND_UP(US_HZ, RTC_CLK_HZ)
#define SYNC_TICK	(7)
#define RTC_TICK_MAX	((1ul << 48) -1) /* about 388 years */
#define RTC_SEC_MAX	(RTC_TICK_MAX >> RTC_CLK_SHIFT)

struct sdrv_rtc {
	spinlock_t		lock;
	int			irq_alrm;
	void __iomem		*base;
	struct rtc_device	*rtc;
	struct device		*dev;
};

/* static struct wake_lock rtc_irq_wake_lock; */
static unsigned long secs_to_start;

static u64 __get_tick(void __iomem *base)
{
	u32 tickl = readl(base + RTCROFF(RTC_L));
	u32 tickh = readl(base + RTCROFF(RTC_H));

	u64 tick = ((uint64_t)tickh << 32) | (uint64_t)tickl;

	return tick;
}

static u64 __get_alarm(void __iomem *base)
{
	u32 tickl = readl(base + RTCROFF(TIMER_L));
	u32 tickh = readl(base + RTCROFF(TIMER_H));

	u64 tick = ((uint64_t)tickh << 32) | (uint64_t)tickl;

	return tick;
}

static void __clk_sync(void)
{
	int retries = SYNC_TICK;

	do {
		udelay(SYNC_DELAY_US);
	} while (--retries);
}

static void __set_tick(void __iomem *base, u64 tick)
{
	u32 var, tmp;

	var = tmp = readl(base + RTCROFF(RTC_CTRL));

	if (var & RTCB(RTC_CTRL, EN)) {
		tmp = var & ~RTCB(RTC_CTRL, EN);
		writel(tmp, base + RTCROFF(RTC_CTRL));
		__clk_sync();
	}
	else {
		var |= RTCB(RTC_CTRL, EN);
	}

	writel((u32)tick, base + RTCROFF(RTC_L));
	writel((u32)(tick >> 32), base + RTCROFF(RTC_H));
	__clk_sync();

	writel(var, base + RTCROFF(RTC_CTRL));
	__clk_sync();
}

static void __set_alarm(void __iomem *base, u64 tick)
{
	/* alarm should be turned off beforehand */
	/* shall zero all H bits at first, then write L, finally write H */
	writel(0, base + RTCROFF(TIMER_H));
	writel((u32)tick, base + RTCROFF(TIMER_L));
	writel((u32)(tick >> 32), base + RTCROFF(TIMER_H));
	__clk_sync();
}

static bool __rtc_is_enabled(void __iomem *base)
{
	u32 var = readl(base + RTCROFF(RTC_CTRL));

	if (var & RTCB(RTC_CTRL, EN)) {
		return true;
	}
	else {
		return false;
	}
}

static bool __check_wakeup_status(struct sdrv_rtc *info)
{
	u32 var = readl(info->base + RTCROFF(WAKEUP_CTRL));

	if(var & RTCB(WAKEUP_CTRL, STATUS)) {
		return true;
	}
	else {
		return false;
	}
}

static void __wakeup_enable(struct sdrv_rtc *info, bool en)
{
	u32 tmp, var;
	void __iomem *base = info->base;

	var = readl(base + RTCROFF(WAKEUP_CTRL));
	var &= ~( RTCB(WAKEUP_CTRL, REQ_EN) |
		  RTCB(WAKEUP_CTRL, IRQ_EN) |
		  RTCB(WAKEUP_CTRL, EN)     |
		  RTCB(WAKEUP_CTRL, STATUS) |
		  RTCB(WAKEUP_CTRL, CLEAR) );

	if(en)
	{
		var |= ( RTCB(WAKEUP_CTRL, IRQ_EN) |
			 RTCB(WAKEUP_CTRL, REQ_EN) |
			 RTCB(WAKEUP_CTRL, EN) );
	}

	tmp = var;
	tmp |= RTCB(WAKEUP_CTRL, CLEAR);
	writel(tmp, base + RTCROFF(WAKEUP_CTRL));
	__clk_sync();

	writel(var, base + RTCROFF(WAKEUP_CTRL));
	__clk_sync();
}

static void __rtc_enable(void __iomem *base, bool en)
{
	u32 val = readl(base + RTCROFF(RTC_CTRL));

	if (en)
		val |= RTCB(RTC_CTRL, EN);
	else
		val &= ~RTCB(RTC_CTRL, EN);

	writel(val, base + RTCROFF(RTC_CTRL));
	__clk_sync();
}

static irqreturn_t sdrv_rtc_irq(int irq, void *dev_id)
{
	struct sdrv_rtc *info = dev_id;

	/* dev_dbg(&info->rtc->dev, "%s:irq(%d)\n", __func__, irq); */
	rtc_update_irq(info->rtc, 1, RTC_IRQF | RTC_AF);

	spin_lock(&info->lock);
	__wakeup_enable(info, false);
	spin_unlock(&info->lock);

	return IRQ_HANDLED;
}

static int sdrv_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sdrv_rtc *info = dev_get_drvdata(dev);
	unsigned long secs;
	int ret;

	ret = rtc_tm_to_time(&alrm->time, &secs);
	if (ret != 0)
		return ret;

	if (secs < secs_to_start) {
#ifdef CONFIG_RTC_START_YEAR
		dev_err(dev, "can't set alarm which is earlier than year %d", CONFIG_RTC_START_YEAR);
#else
		dev_err(dev, "can't set alarm which is earlier than year 1970");
#endif
		return -EINVAL;
	}

	secs -= secs_to_start;
	if (secs > RTC_SEC_MAX) {
		dev_err(dev, "set alarm overflow");
		return -EINVAL;
	}

	spin_lock_irq(&info->lock);

	if ((secs - 1) <= (__get_tick(info->base) >> RTC_CLK_SHIFT)) {
		dev_err(dev, "don't allow setting alarm in the past");
		spin_unlock_irq(&info->lock);
		return -EINVAL;
	}

	__wakeup_enable(info, false);

	__set_alarm(info->base, secs << RTC_CLK_SHIFT);

	if (alrm->enabled)
		__wakeup_enable(info, true);

	spin_unlock_irq(&info->lock);

	return ret;
}

static int sdrv_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	u32 var;
	struct sdrv_rtc *info = dev_get_drvdata(dev);
	void __iomem *base = info->base;
	unsigned long secs;

	spin_lock_irq(&info->lock);

	secs = __get_alarm(base);

        var = readl(base + RTCROFF(WAKEUP_CTRL));
	alrm->enabled = (var & RTCB(WAKEUP_CTRL, IRQ_EN)) &&
			(var & RTCB(WAKEUP_CTRL, REQ_EN));
	alrm->pending = !!(var & RTCB(WAKEUP_CTRL, STATUS));

	spin_unlock_irq(&info->lock);

	secs += secs_to_start;
	rtc_time_to_tm(secs, &alrm->time);

	return 0;
}

static int sdrv_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct sdrv_rtc *info = dev_get_drvdata(dev);
	unsigned long secs = __get_tick(info->base) >> RTC_CLK_SHIFT;

	rtc_time_to_tm(secs + secs_to_start, tm);
	return rtc_valid_tm(tm);
}

static int sdrv_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct sdrv_rtc *info = dev_get_drvdata(dev);
	unsigned long secs = 0;

	rtc_tm_to_time(tm, &secs);

	if (secs < secs_to_start) {
		dev_err(dev, "can't set time which is earlier than year %d", CONFIG_RTC_START_YEAR);
		return -EINVAL;
	}

	secs -= secs_to_start;
	if (secs > RTC_SEC_MAX) {
		dev_err(dev, "set time overflow");
		return -EINVAL;
	}

	spin_lock_irq(&info->lock);
	__set_tick(info->base, secs << RTC_CLK_SHIFT);
	spin_unlock_irq(&info->lock);

	return 0;
}

static int sdrv_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct sdrv_rtc *info = dev_get_drvdata(dev);

	spin_lock_irq(&info->lock);
	__wakeup_enable(info, enabled);
	spin_unlock_irq(&info->lock);

	return 0;
}

static int sdrv_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct sdrv_rtc *info;
	void __iomem *base;

	if (!dev || !dev->driver)
		return -ENXIO;

	info = dev_get_drvdata(dev);
	base = info->base;

	seq_printf(seq, "RTC_CTRL\t: 0x%x\n", readl(base + RTCROFF(RTC_CTRL)));
	seq_printf(seq, "WAKEUP_CTRL\t: 0x%x\n", readl(base + RTCROFF(WAKEUP_CTRL)));
	seq_printf(seq, "RTC_L\t: 0x%x\n", readl(base + RTCROFF(RTC_L)));
	seq_printf(seq, "RTC_H\t: 0x%x\n", readl(base + RTCROFF(RTC_H)));
	seq_printf(seq, "TIMER_L\t: 0x%x\n", readl(base + RTCROFF(TIMER_L)));
	seq_printf(seq, "TIMER_H\t: 0x%x\n", readl(base + RTCROFF(TIMER_H)));

	return 0;
}

static const struct rtc_class_ops sdrv_rtc_ops = {
	.read_time = sdrv_rtc_read_time,
	.set_time = sdrv_rtc_set_time,
	.read_alarm = sdrv_rtc_read_alarm,
	.set_alarm = sdrv_rtc_set_alarm,
	.proc = sdrv_rtc_proc,
	.alarm_irq_enable = sdrv_rtc_alarm_irq_enable,
};

static int sdrv_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct sdrv_rtc *info;
	struct resource *iores;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	int irq, ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->base = base = devm_ioremap_resource(&pdev->dev, iores);
	if (IS_ERR(info->base))
		return PTR_ERR(info->base);

	irq = platform_get_irq(pdev, 0);
	if (unlikely(irq <= 0)) {
		dev_err(&pdev->dev, "no irq resource specified\n");
		return -ENXIO;
	}

	info->irq_alrm = platform_get_irq(pdev, 0);

	spin_lock_init(&info->lock);

	platform_set_drvdata(pdev, info);

	if (!__rtc_is_enabled(base)) {
		__rtc_enable(base, true);
		dev_info(&pdev->dev, "RTC is enabled now\n");
	} else {
		dev_info(&pdev->dev, "RTC has been enabled previously!\n");
	}

	if (__check_wakeup_status(info))
		__wakeup_enable(info, false);

	rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);
	rtc->ops = &sdrv_rtc_ops;
	info->rtc = rtc;
	info->dev = &pdev->dev;

	ret = devm_request_irq(&pdev->dev, irq, sdrv_rtc_irq,
				  IRQF_ONESHOT | IRQF_EARLY_RESUME,
				  pdev->name, info);
	if (ret)
		return ret;

	device_init_wakeup(&pdev->dev, 1);
	dev_pm_set_wake_irq(&pdev->dev, info->irq_alrm);

	ret = rtc_register_device(info->rtc);
	if (ret)
		return ret;

	dev_notice(&pdev->dev, "SEMIDRIVE internal Real Time Clock\n");

	return 0;
}

static int sdrv_rtc_remove(struct platform_device *pdev)
{
	struct sdrv_rtc *info = platform_get_drvdata(pdev);

	if (info) {
		/* NOTE: clear and disable all interrupt sources */
		spin_lock_irq(&info->lock);
		__wakeup_enable(info, false);
		spin_unlock_irq(&info->lock);
		/* NOTE: free irqs  */
		free_irq(info->irq_alrm, &pdev->dev);
	}

	device_init_wakeup(&pdev->dev, 0);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sdrv_rtc_suspend(struct device *dev)
{
	struct sdrv_rtc *info = dev_get_drvdata(dev);

	dev_vdbg(dev, "Suspend (device_may_wakeup=%d) IRQ:%d\n",
		 device_may_wakeup(dev), info->irq_alrm);

#if 0
	/* leave the alarms on as a wake source */
	if (device_may_wakeup(dev)) {
		/* NOTE: No need to write reg to enable rtc wakeup */
		enable_irq_wake(info->irq_alrm);
	}
#endif
	return 0;
}

static int sdrv_rtc_resume(struct device *dev)
{
#if 0
	struct sdrv_rtc *info = dev_get_drvdata(dev);
#endif

	dev_vdbg(dev, "Resume (device_may_wakeup=%d)\n",
		 device_may_wakeup(dev));

#if 0
	/* alarms were left on as a wake source, turn them off */
	if (device_may_wakeup(dev)) {
		/* NOTE: No need to write reg to disable rtc wakeup */
		disable_irq_wake(info->irq_alrm);
	}
#endif
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sdrv_rtc_pm_ops, sdrv_rtc_suspend,
			sdrv_rtc_resume);

static const struct of_device_id sdrv_rtc_dt_ids[] = {
	{ .compatible = "semidrive,sdrv-rtc", },
	{}
};

static struct platform_driver sdrv_rtc_driver = {
	.probe		= sdrv_rtc_probe,
	.remove		= sdrv_rtc_remove,
	.driver		= {
		.name	= "sdrv-rtc",
		.pm	= &sdrv_rtc_pm_ops,
		.of_match_table = of_match_ptr(sdrv_rtc_dt_ids),
	},
};

static int __init sdrv_rtc_init(void)
{
	int ret;

#ifdef CONFIG_RTC_START_YEAR
	if (CONFIG_RTC_START_YEAR > 1970)
		secs_to_start = mktime(CONFIG_RTC_START_YEAR,
			1, 1, 0, 0, 0);
	else
#endif
		secs_to_start = mktime(1970, 1, 1, 0, 0, 0);

	ret = platform_driver_register(&sdrv_rtc_driver);
	if (ret) {
		return ret;
	}

	return 0;
}

static void __exit sdrv_rtc_exit(void)
{
        platform_driver_unregister(&sdrv_rtc_driver);
}

MODULE_DESCRIPTION("SEMIDRIVE Realtime Clock Driver (RTC)");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sdrv-rtc");

module_init(sdrv_rtc_init);
module_exit(sdrv_rtc_exit);
