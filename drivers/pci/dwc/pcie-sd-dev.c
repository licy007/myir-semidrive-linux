#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/kernel.h>

extern irqreturn_t sd_remote_gic_handle_irq(int virq, void* dev);
extern int sd_remote_gic_init(void);
extern int sd_remote_gic_exit(void);

#define DRV_NAME		"sd pcie dev"

static const struct pci_device_id sd_pcie_dev_ids[] = {
	{ PCI_DEVICE(0x1e8c, 0x1), },
	{ 0,}
};

static int sd_pcie_dev_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret = 0, irq = 0;

	ret = pci_enable_device(pdev);
	if (ret) {
		pr_info("pci_enable_device() failed, rc = %d.\n", ret);
		goto err0;
	}

	pci_set_master(pdev);

	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
	if (ret < 0) {
		pr_info("pci_alloc_irq_vectors failed: rc = %d\n", ret);
		goto err0;
	} else
		ret = 0;

	irq = pci_irq_vector(pdev, 0);

	if (irq < 0) {
		pr_info("pci_irq_vector failed: rc = %d\n", irq);
		ret = -1;
		goto err0;
	} else
		ret = 0;

	ret = request_irq(irq, sd_remote_gic_handle_irq, IRQF_SHARED, "pcie_gic", pdev);
	if (ret < 0) {
		pr_info("request_irq failed: rc = %d\n", ret);
		goto err0;
	} else
		ret = 0;

	ret = sd_remote_gic_init();
	if (ret < 0) {
		pr_info("sd_remote_gic_init failed: rc = %d\n", ret);
		ret = -1;
	}

err0:
	if (ret)
		pr_info("sd_pcie_dev_probe fail\n");
	else
		pr_info("sd_pcie_dev_probe success\n");

	return ret;
}

static void sd_pcie_dev_remove(struct pci_dev *pdev)
{
	pr_info(DRV_NAME"pci_disable_device()\n");
	sd_remote_gic_exit();
	pci_free_irq_vectors(pdev);
	pci_disable_device(pdev);
}

static struct pci_driver sd_pcie_dev_driver = {
	.name = DRV_NAME,
	.id_table = sd_pcie_dev_ids,
	.probe = sd_pcie_dev_probe,
	.remove = sd_pcie_dev_remove,
};

static int __init sd_pcie_dev_init(void)
{
	int ret = 0;
	ret = pci_register_driver(&sd_pcie_dev_driver);
	return ret;
}

static void __exit sd_pcie_dev_exit(void)
{
	pr_info(DRV_NAME" exit()\n");
	pci_unregister_driver(&sd_pcie_dev_driver);
}

module_init(sd_pcie_dev_init);
module_exit(sd_pcie_dev_exit);
