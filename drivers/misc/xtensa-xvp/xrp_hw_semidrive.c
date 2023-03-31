/*
 * xrp_hw_semidrive: xtensa/arm low-level XRP driver for semidrive board
 *
 * Copyright (c) 2017 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Alternatively you can use and distribute this file under the terms of
 * the GNU General Public License version 2 or later.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include "xrp_kernel_defs.h"
#include "xrp_hw.h"
#include "xrp_internal.h"
#include "xrp_hw_semidrive_dsp_interface.h"

#define DRIVER_NAME "xrp-hw-semidrive"

enum xrp_irq_mode {
	XRP_IRQ_NONE,
	XRP_IRQ_LEVEL,
	XRP_IRQ_EDGE,
	XRP_IRQ_EDGE_SW,
	XRP_IRQ_MAX,
};

struct xrp_hw_semidrive {
	struct xvp        *xrp;
	phys_addr_t       regs_phys;        /* vdsp mu io address */
	void __iomem      *regs_altvec;     /* host io address for scr vdsp altvec */
	void __iomem      *regs_ctrl;       /* host io address for scr vdsp runstall */
	void __iomem      *regs_rst_core;   /* host io address for rstgen core 0 */
	void __iomem      *regs_rst_module; /* host io address for rstgen module dreset */
	enum xrp_irq_mode device_irq_mode;  /* how IRQ is used to notify the device of incoming data */
	u32               device_irq;       /* device IRQ num */
	enum xrp_irq_mode host_irq_mode;    /* how IRQ is used to notify the host of incoming data */
	struct mbox_chan  *tx_channel;      /* if IRQ mode, need mailbox channels */
	u32               eentry;           /* firmware elf entry addr */
};

static inline void reg_write32(void __iomem *regs, unsigned addr, u32 v)
{
	if (regs)
		__raw_writel(v, regs + addr);
}

static inline u32 reg_read32(void __iomem *regs, unsigned addr)
{
	if (regs)
		return __raw_readl(regs + addr);
	else
		return 0;
}

static u32 get_eentry(void *hw_arg)
{
	struct xrp_hw_semidrive *hw = hw_arg;
	return hw->eentry;
}

static void *get_hw_sync_data(void *hw_arg, size_t *sz)
{
	static const u32 irq_mode[] = {
		[XRP_IRQ_NONE]    = XRP_DSP_SYNC_IRQ_MODE_NONE,
		[XRP_IRQ_LEVEL]   = XRP_DSP_SYNC_IRQ_MODE_LEVEL,
		[XRP_IRQ_EDGE]    = XRP_DSP_SYNC_IRQ_MODE_EDGE,
		[XRP_IRQ_EDGE_SW] = XRP_DSP_SYNC_IRQ_MODE_EDGE,
	};
	struct xrp_hw_semidrive *hw = hw_arg;
	struct xrp_hw_semidrive_sync_data *hw_sync_data =
		kmalloc(sizeof(*hw_sync_data), GFP_KERNEL);

	if (!hw_sync_data)
		return NULL;

	*hw_sync_data = (struct xrp_hw_semidrive_sync_data){
		.device_mmio_base = hw->regs_phys,
		.host_irq_mode    = hw->host_irq_mode,
		.device_irq_mode  = irq_mode[hw->device_irq_mode],
		.device_irq       = hw->device_irq,
	};
	*sz = sizeof(*hw_sync_data);
	return hw_sync_data;
}

/* temp rstgen code, will be removed after rstgen module OK */
static void rstgen_module_rst_dreset(void *hw_arg, uint32_t rst_b)
{
	struct xrp_hw_semidrive *hw = hw_arg;
	uint32_t value;
	int count = 100;

	value  = reg_read32(hw->regs_rst_module, 0);
	value &=  ~3;
	value |= 0x2 | (rst_b & 0x1);
	reg_write32(hw->regs_rst_module, 0, value);

	while (count-- > 0) {
		value = reg_read32(hw->regs_rst_module, 0);
		if ((value & (1 << 30)) == ((rst_b & 0x1) << 30))
			break;
		udelay(1);
	}
}

static void rstgen_core_rst_vdsp(void *hw_arg)
{
	struct xrp_hw_semidrive *hw = hw_arg;
	uint32_t value;
	int count = 100;

	/* Unlock core reset register. */
	value  = reg_read32(hw->regs_rst_core, 0);
	value &= 0x7fffffff;
	reg_write32(hw->regs_rst_core, 0, value);

        /* Release STATIC_RST_B. */
	value  = reg_read32(hw->regs_rst_core, 0x1000);
	value |= 0x1;
	reg_write32(hw->regs_rst_core, 0x1000, value);

        while (count-- > 0) {
                value = reg_read32(hw->regs_rst_core, 0x1000);
                if ((value & ((1 << 29))))
                        break;
                udelay(1);
        }

        /* Enable SW core reset. */
	value  = reg_read32(hw->regs_rst_core, 0);
	value |= 0x1;
	reg_write32(hw->regs_rst_core, 0, value);

	count = 100;
        while (count-- > 0) {
                value = reg_read32(hw->regs_rst_core, 0);
                if ((value & ((1 << 30))))
                        break;
                udelay(1);
        }

        /* Trigger auto-clear reset. */
	value  = reg_read32(hw->regs_rst_core, 0x1000);
        value |= 0x2;
	reg_write32(hw->regs_rst_core, 0x1000, value);
}

static void reset(void *hw_arg)
{
	rstgen_module_rst_dreset(hw_arg, 0);
	rstgen_module_rst_dreset(hw_arg, 1);
	rstgen_core_rst_vdsp(hw_arg);
}

/* set vdsp vector address, hold stall */
static void halt(void *hw_arg)
{
	struct xrp_hw_semidrive *hw = hw_arg;
	uint32_t addr = hw->eentry;

	reg_write32(hw->regs_altvec, 0, (addr >> 4));
	reg_write32(hw->regs_ctrl, 0, 3);
}

/* realse vdsp stall, start to run */
static void release(void *hw_arg)
{
	struct xrp_hw_semidrive *hw = hw_arg;
	uint32_t value;

	value = reg_read32(hw->regs_ctrl, 0);
	value &= 0xfffffffd;
	reg_write32(hw->regs_ctrl, 0, value);
}

/* send irq to device */
static void send_irq(void *hw_arg)
{
	struct xrp_hw_semidrive *hw = hw_arg;
	int ret;
#ifndef CONFIG_SEMIDRIVE_IPCC
	sd_msghdr_t msg;
#endif

	switch (hw->device_irq_mode) {
	case XRP_IRQ_LEVEL:
#ifndef CONFIG_SEMIDRIVE_IPCC
		mb_msg_init_head(&msg, 5, 0, 0, MB_MSG_HDR_SZ, IPCC_ADDR_VDSP_ANN);
		ret = mbox_send_message(hw->tx_channel, &msg); // send no-content mail
#else
		ret = sd_kick_vdsp();
#endif
		if (ret < 0)
			pr_err("Failed to send message via mailbox, ret = %d\n", ret);
	default:
		break;
	}
}
#ifndef CONFIG_SEMIDRIVE_IPCC
static void xrp_hw_mbox_irq(struct mbox_client *client, void *message)
{
	struct xvp *xvp = dev_get_drvdata(client->dev); /* set by xrp_init_common */
	/* called before sd_mu_ack_msg, no need to lock */
	xrp_irq_handler(-1, xvp);
}
#else
static void xrp_hw_mbox_cb(void *ctx, void *message)
{
	struct device *dev = ctx;
	struct xvp *xvp = dev_get_drvdata(dev); /* set by xrp_init_common */
	/* called before sd_mu_ack_msg, no need to lock */
	xrp_irq_handler(-1, xvp);
}
#endif

#if defined(__XTENSA__)
static bool cacheable(void *hw_arg, unsigned long pfn, unsigned long n_pages)
{
	return true;
}

static void dma_sync_for_device(void *hw_arg,
				void *vaddr, phys_addr_t paddr,
				unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ:
		__flush_dcache_range((unsigned long)vaddr, sz);
		break;

	case XRP_FLAG_READ_WRITE:
		__flush_dcache_range((unsigned long)vaddr, sz);
		__invalidate_dcache_range((unsigned long)vaddr, sz);
		break;

	case XRP_FLAG_WRITE:
		__invalidate_dcache_range((unsigned long)vaddr, sz);
		break;
	}
}

static void dma_sync_for_cpu(void *hw_arg,
			     void *vaddr, phys_addr_t paddr,
			     unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ_WRITE:
	case XRP_FLAG_WRITE:
		__invalidate_dcache_range((unsigned long)vaddr, sz);
		break;
	}
}

#elif defined(__arm__)
static bool cacheable(void *hw_arg, unsigned long pfn, unsigned long n_pages)
{
	return true;
}

static void dma_sync_for_device(void *hw_arg,
				void *vaddr, phys_addr_t paddr,
				unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_clean_range(paddr, paddr + sz);
		break;

	case XRP_FLAG_WRITE:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_inv_range(paddr, paddr + sz);
		break;

	case XRP_FLAG_READ_WRITE:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_flush_range(paddr, paddr + sz);
		break;
	}
}

static void dma_sync_for_cpu(void *hw_arg,
			     void *vaddr, phys_addr_t paddr,
			     unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_WRITE:
	case XRP_FLAG_READ_WRITE:
		__cpuc_flush_dcache_area(vaddr, sz);
		outer_inv_range(paddr, paddr + sz);
		break;
	}
}
#else
static bool cacheable(void *hw_arg, unsigned long pfn, unsigned long n_pages)
{
	return true;
}

static void dma_sync_for_device(void *hw_arg,
				void *vaddr, phys_addr_t paddr,
				unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ:
		__flush_dcache_area(vaddr, sz);
		break;

	case XRP_FLAG_READ_WRITE:
		__flush_dcache_area(vaddr, sz);
		__inval_dcache_area(vaddr, sz);
		break;

	case XRP_FLAG_WRITE:
		__inval_dcache_area(vaddr, sz);
		break;
	}
}

static void dma_sync_for_cpu(void *hw_arg,
			     void *vaddr, phys_addr_t paddr,
			     unsigned long sz, unsigned flags)
{
	switch (flags) {
	case XRP_FLAG_READ_WRITE:
	case XRP_FLAG_WRITE:
		__inval_dcache_area(vaddr, sz);
		break;
	}
}

#endif

static const struct xrp_hw_ops hw_ops = {
	.halt                = halt,
	.release             = release,
	.reset               = reset,

	.get_hw_sync_data    = get_hw_sync_data,
	.get_eentry          = get_eentry,

	.send_irq            = send_irq,

/* for iomem not in kernel dma region */
	.cacheable           = cacheable,
	.dma_sync_for_device = dma_sync_for_device,
	.dma_sync_for_cpu    = dma_sync_for_cpu,
};

#ifndef CONFIG_SEMIDRIVE_IPCC
static struct mbox_chan *
xrp_hw_mbox_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev          = &pdev->dev;
	client->rx_callback  = xrp_hw_mbox_irq;
	client->tx_prepare   = NULL; /* no content to send */
	client->tx_done      = NULL;
	client->tx_block     = true;
	client->knows_txdone = false;
	client->tx_tout      = 500;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		dev_warn(&pdev->dev, "Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}
#endif
static long init_hw(struct platform_device *pdev, struct xrp_hw_semidrive *hw,
		    int mem_idx, enum xrp_init_flags *init_flags)
{
	struct resource *mem, *mem_altvec, *mem_ctrl, *mem_rst_core, *mem_rst_module;
	int irq;
	long ret;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx);
	if (!mem) {
		ret = -ENODEV;
		goto err;
	}
	hw->regs_phys = mem->start;

	mem_altvec = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx + 1);
	if (!mem_altvec) {
		ret = -ENODEV;
		goto err;
	}

	mem_ctrl = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx + 2);
	if (!mem_ctrl) {
		ret = -ENODEV;
		goto err;
	}

	mem_rst_core = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx + 3);
	if (!mem_rst_core) {
		ret = -ENODEV;
		goto err;
	}

	mem_rst_module = platform_get_resource(pdev, IORESOURCE_MEM, mem_idx + 4);
	if (!mem_rst_module) {
		ret = -ENODEV;
		goto err;
	}

	hw->regs_altvec = devm_ioremap_resource(&pdev->dev, mem_altvec);
	hw->regs_ctrl   = devm_ioremap_resource(&pdev->dev, mem_ctrl);

	hw->regs_rst_core   = devm_ioremap_resource(&pdev->dev, mem_rst_core);
	hw->regs_rst_module = devm_ioremap_resource(&pdev->dev, mem_rst_module);

	pr_debug("%s: regs = %pap/%d\n", __func__, &mem->start, 0);
	pr_debug("%s: regs = %pap/%p\n", __func__, &mem_altvec->start, hw->regs_altvec);
	pr_debug("%s: regs = %pap/%p\n", __func__, &mem_ctrl->start, hw->regs_ctrl);
	pr_debug("%s: regs = %pap/%p\n", __func__, &mem_rst_core->start, hw->regs_rst_core);
	pr_debug("%s: regs = %pap/%p\n", __func__, &mem_rst_module->start, hw->regs_rst_module);

	ret = of_property_read_u32(pdev->dev.of_node, "firmware-entry", &hw->eentry);
	if (ret == 0) {
		dev_dbg(&pdev->dev, "%s: firmware entry = 0x%x",
                        __func__, hw->eentry);
        } else {
                dev_info(&pdev->dev, "using default firmware entry \n");
		hw->eentry = 0x60000000;
        }

	ret = of_property_read_u32(pdev->dev.of_node, "device-irq", &hw->device_irq);
	if (ret == 0) {
		u32 device_irq_mode;
		ret = of_property_read_u32(pdev->dev.of_node, "device-irq-mode", &device_irq_mode);
		if (device_irq_mode < XRP_IRQ_MAX)
			hw->device_irq_mode = device_irq_mode;
		else
			ret = -ENOENT;
	}
	if (ret == 0) {
		dev_dbg(&pdev->dev, "%s: device IRQ = %d, IRQ mode = %d",
			__func__, hw->device_irq, hw->device_irq_mode);
	} else {
		dev_info(&pdev->dev, "using polling mode on the device side\n");
	}

	if (ret == 0) { /* if device using polling, host using polling too */
		u32 host_irq_mode;
		ret = of_property_read_u32(pdev->dev.of_node, "host-irq-mode", &host_irq_mode);
		if (host_irq_mode < XRP_IRQ_MAX)
			hw->host_irq_mode = host_irq_mode;
		else
			ret = -ENOENT;
	}

	if (ret == 0 && hw->host_irq_mode != XRP_IRQ_NONE)
		of_property_read_u32(pdev->dev.of_node, "host-irq", &irq);
	else
		irq = -1;

	if (irq > 0) {
		dev_dbg(&pdev->dev, "%s: host IRQ = %d, ", __func__, irq);
#ifndef CONFIG_SEMIDRIVE_IPCC
		/* if using IRQ mode, get mailbox channel */
		hw->tx_channel = xrp_hw_mbox_request_channel(pdev, "tx");
		dev_info(&pdev->dev, "tx %p channel\n", hw->tx_channel);
#else
		sd_connect_vdsp(&pdev->dev, xrp_hw_mbox_cb);
#endif
		*init_flags |= XRP_INIT_USE_HOST_IRQ;
	} else {
		dev_info(&pdev->dev, "using polling mode on the host side\n");
	}
	ret = 0;
err:
	return ret;
}

static long init(struct platform_device *pdev, struct xrp_hw_semidrive *hw)
{
	long ret;
	enum xrp_init_flags init_flags = 0;

	ret = init_hw(pdev, hw, 0, &init_flags);
	if (ret < 0)
		return ret;

	return xrp_init(pdev, init_flags, &hw_ops, hw);
}

static long init_v1(struct platform_device *pdev, struct xrp_hw_semidrive *hw)
{
	long ret;
	enum xrp_init_flags init_flags = 0;

	/* use 2 reg to seperate 0: cmd queue 1: shared buffer */
	ret = init_hw(pdev, hw, 2, &init_flags);
	if (ret < 0)
		return ret;

	return xrp_init_v1(pdev, init_flags, &hw_ops, hw);
}

static long init_cma(struct platform_device *pdev, struct xrp_hw_semidrive *hw)
{
	long ret;
	enum xrp_init_flags init_flags = 0;

	ret = init_hw(pdev, hw, 0, &init_flags);
	if (ret < 0)
		return ret;

	return xrp_init_cma(pdev, init_flags, &hw_ops, hw);
}

static long init_iommu(struct platform_device *pdev, struct xrp_hw_semidrive *hw)
{
	long ret;
	enum xrp_init_flags init_flags = 0;

	ret = init_hw(pdev, hw, 0, &init_flags);
	if (ret < 0)
		return ret;

	return xrp_init_iommu(pdev, init_flags, &hw_ops, hw);
}

#ifdef CONFIG_OF
static const struct of_device_id xrp_hw_semidrive_match[] = {
	{
		.compatible = "cdns,xrp-hw-semidrive",
		.data = init,
	}, {
		.compatible = "cdns,xrp-hw-semidrive,v1",
		.data = init_v1,
	}, {
		.compatible = "cdns,xrp-hw-semidrive,cma",
		.data = init_cma,
	}, {
		.compatible = "cdns,xrp-hw-semidrive,iommu",
		.data = init_iommu,
	}, {},
};
MODULE_DEVICE_TABLE(of, xrp_hw_semidrive_match);
#endif

static int xrp_hw_semidrive_probe(struct platform_device *pdev)
{
	struct xrp_hw_semidrive *hw =
		devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	const struct of_device_id *match;
	long (*init)(struct platform_device *pdev, struct xrp_hw_semidrive *hw);
	long ret;

	if (!hw)
		return -ENOMEM;

	match = of_match_device(of_match_ptr(xrp_hw_semidrive_match),
				&pdev->dev);
	if (!match)
		return -ENODEV;

	init = match->data;
	ret = init(pdev, hw);
	if (IS_ERR_VALUE(ret)) {
		pr_err("xrp probe failed, ret=%ld\n", ret);
		xrp_deinit(pdev);
		return ret;
	} else {
		hw->xrp = ERR_PTR(ret);
		return 0;
	}

}

static int xrp_hw_semidrive_remove(struct platform_device *pdev)
{
	struct xvp *xvp = platform_get_drvdata(pdev);
	struct xrp_hw_semidrive *hw = xvp->hw_arg;
        if (hw->tx_channel)
                mbox_free_channel(hw->tx_channel);
	return xrp_deinit(pdev);
}

static const struct dev_pm_ops xrp_hw_semidrive_pm_ops = {
	SET_RUNTIME_PM_OPS(xrp_runtime_suspend,
			   xrp_runtime_resume, NULL)
};

static struct platform_driver xrp_hw_semidrive_driver = {
	.probe   = xrp_hw_semidrive_probe,
	.remove  = xrp_hw_semidrive_remove,
	.driver  = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(xrp_hw_semidrive_match),
		.pm = &xrp_hw_semidrive_pm_ops,
	},
};

module_platform_driver(xrp_hw_semidrive_driver);

MODULE_AUTHOR("Max Filippov");
MODULE_DESCRIPTION("XRP: low level device driver for Xtensa Remote Processing");
MODULE_LICENSE("Dual MIT/GPL");
