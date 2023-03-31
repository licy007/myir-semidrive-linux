/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/platform_device.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/init.h>
#include <linux/irqreturn.h>
#include <linux/irq.h>
#include <linux/pm_runtime.h>
#include <linux/pci.h>

#define set_r_int_ready(r_int_id)   writel(0xA5A5A5A5, r_int_id)

struct remote_gic_chip_data {
    struct list_head node;
    struct gic_chip_data *remote_gic;
    void __iomem *int_id;
    unsigned short attached_pci_dev;
};

static LIST_HEAD(remote_gic_list);
static DEFINE_SPINLOCK(remote_gic_lock);

static int sd_remote_gic_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct remote_gic_chip_data *r_gic_chip_data;
    int ret;
    u64 int_id_phys_addr;
    unsigned long flags;

    r_gic_chip_data = devm_kzalloc(dev,
                                   sizeof(struct remote_gic_chip_data),
                                   GFP_KERNEL);
    if (!r_gic_chip_data)
        return -ENOMEM;

    pm_runtime_enable(dev);

    ret = pm_runtime_get_sync(dev);
    if (ret < 0)
        goto rpm_disable;

    /* virq not yet known, set an invalid  virq to skip setting
     * chained handler.
     */
    ret = gic_of_init_child(dev, &r_gic_chip_data->remote_gic, -1);
    if (ret)
        goto rpm_put;

    ret = of_property_read_u64(dev->of_node, "int-id", &int_id_phys_addr);
    if (ret) {
        dev_err(dev, "read int-id failed\n");
        goto rpm_put;
    }

    pr_info("%s: read int id phys addr 0x%llx\n", __func__, int_id_phys_addr);
    r_gic_chip_data->int_id = ioremap_nocache(int_id_phys_addr, 4);

    if (!r_gic_chip_data->int_id) {
        dev_err(dev, "can't remap int_id \n");
        ret = -1;
        goto rpm_put;
    }

    ret = of_property_read_u16(dev->of_node, "attached-pci-dev",
                               &r_gic_chip_data->attached_pci_dev);
    if (ret) {
        dev_err(dev, "read attached-pci-dev failed\n");
        goto rpm_put;
    }

    spin_lock_irqsave(&remote_gic_lock, flags);
    list_add_tail(&r_gic_chip_data->node, &remote_gic_list);
    spin_unlock_irqrestore(&remote_gic_lock, flags);

    set_r_int_ready(r_gic_chip_data->int_id);

    platform_set_drvdata(pdev, r_gic_chip_data);

    pm_runtime_put(dev);

    dev_info(dev, "remote GIC registered\n");

    return 0;

rpm_put:
    pm_runtime_put_sync(dev);
rpm_disable:
    pm_runtime_disable(dev);

    return ret;
}

static int sd_remote_gic_remove(struct platform_device *pdev)
{
    struct remote_gic_chip_data *r_gic_chip_data;
    unsigned long flags;

    pm_runtime_put_sync(&pdev->dev);
    pm_runtime_disable(&pdev->dev);

    r_gic_chip_data = platform_get_drvdata(pdev);

    iounmap(r_gic_chip_data->int_id);

    spin_lock_irqsave(&remote_gic_lock, flags);
    list_del(&r_gic_chip_data->node);
    spin_unlock_irqrestore(&remote_gic_lock, flags);

    return 0;
}

static const struct of_device_id sd_remote_gic_dt_ids[] = {
    {.compatible = "semidrive,remote_gic"},
    {}
};

static struct platform_driver sd_remote_gic_driver = {
    .driver = {
        .name = "sd_remote_gic",
        .of_match_table = sd_remote_gic_dt_ids
    },
    .probe = sd_remote_gic_probe,
    .remove = sd_remote_gic_remove
};

int sd_remote_gic_init(void)
{
    return platform_driver_register(&sd_remote_gic_driver);
}

void sd_remote_gic_exit(void)
{
    platform_driver_unregister(&sd_remote_gic_driver);
}

irqreturn_t sd_remote_gic_handle_irq(int virq, void* dev)
{
    struct irq_desc *desc = irq_to_desc(virq);
    struct pci_dev *pdev = dev;
    unsigned short pci_dev_id = pdev->device;
    struct remote_gic_chip_data *r_gic_data;
    struct irq_domain *domain;
    unsigned int cascade_irq, gic_irq;

    list_for_each_entry(r_gic_data, &remote_gic_list, node) {
        if (r_gic_data->attached_pci_dev == pci_dev_id) {
            if (gic_get_base_and_domain(r_gic_data->remote_gic, NULL,
                                        NULL, &domain))
                return IRQ_NONE;

            gic_irq = readl(r_gic_data->int_id);

            cascade_irq = irq_find_mapping(domain, gic_irq);
            if (unlikely(gic_irq < 32 || gic_irq > 1020)) {
                handle_bad_irq(desc);
            } else {
                isb();
                generic_handle_irq(cascade_irq);
            }

            return IRQ_HANDLED;
        }
    }

    return IRQ_NONE;
}

bool sd_remote_int_completed(unsigned short pci_dev)
{
    struct remote_gic_chip_data *r_gic_data;

    list_for_each_entry(r_gic_data, &remote_gic_list, node) {
        if (r_gic_data->attached_pci_dev == pci_dev) {
            set_r_int_ready(r_gic_data->int_id);
            return true;
        }
    }

    return false;
}
