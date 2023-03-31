/*
 * mipicsi-os.c
 *
 * Semidrive platform mipi csi2 operation
 *
 * Copyright (C) 2022, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include <linux/delay.h>
#include <linux/io.h>

#include "mipicsi-os.h"

static DEFINE_MUTEX(res_mutex);
static void __iomem *s_dispmux_address;

static int mipi_csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sdrv_csi_mipi_csi2 *kcmc;
	int ret = 0;

	kcmc = container_of(sd, struct sdrv_csi_mipi_csi2, subdev);

	if (!kcmc)
		return -EINVAL;

	pr_debug("cam:func[%s] enable[%d]\n", __func__, enable);

	mutex_lock(&kcmc->lock);
	if (enable == 1) {
		ret = mipi_csi2_initialization(&kcmc->core);
		if (ret < 0) {
			mutex_unlock(&kcmc->lock);
			return -ENODEV;
		}
		mipi_csi2_enable(&kcmc->core, enable);
	} else {
		mipi_csi2_enable(&kcmc->core, enable);
	}
	mutex_unlock(&kcmc->lock);

	return 0;
}

static const struct v4l2_subdev_video_ops sdrv_mipi_csi2_video_ops = {
	.s_stream = mipi_csi2_s_stream,
};

static const struct v4l2_subdev_ops sdrv_mipi_csi2_v4l2_ops = {
	.video = &sdrv_mipi_csi2_video_ops,
};

static irqreturn_t dw_mipi_csi_irq(int irq, void *dev_id)
{
	struct sdrv_csi_mipi_csi2 *kcmc = dev_id;

	mipi_csi_irq(&kcmc->core);

	return IRQ_HANDLED;
}

static int dw_mipi_csi_parse_dt(struct platform_device *pdev,
                                struct sdrv_csi_mipi_csi2 *kcmc)
{
	struct device_node *node = pdev->dev.of_node;
	struct csi_vch_info *ipi = &kcmc->core.vch_hw[0];
	int i, ret = 0;

	ret = of_property_read_u32(node, "host_id", &kcmc->core.id);

	if (ret < 0) {
		dev_err(&pdev->dev, "Missing host id\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "lanerate", &kcmc->core.lanerate);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read lanerate\n");
		kcmc->core.lanerate = 150000000;
	} else {
		dev_info(&pdev->dev, "lanerate %d\n", kcmc->core.lanerate);
	}

	ret = of_property_read_u32(node, "lanecount", &kcmc->core.lane_num);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read lane count\n");
		kcmc->core.lane_num = 4;
	} else {
		dev_info(&pdev->dev, "lane_num = %d\n", kcmc->core.lane_num);
	}

	ret = of_property_read_u32(node, "output-type", &ipi->output_type);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't read output-type\n");
		ipi->output_type = DT_YUV422_8;
	} else {
		dev_info(&pdev->dev, "output_type=0x%x\n", ipi->output_type);
	}

	ret = of_property_read_u32(node, "ipi-mode", &ipi->ipi_mode);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read ipi-mode\n");
	}

	ipi->ipi_mode = IPI_MODE_CUT_THROUGH | IPI_MODE_ENABLE;
	ipi->adv_feature = IPI_ADV_SYNC_LEGACCY | IPI_ADV_USE_VIDEO | IPI_ADV_SEL_LINE_EVENT;
	dev_info(&pdev->dev, "ipi_mode=0x%x\n", ipi->ipi_mode);
	dev_info(&pdev->dev, "adv_feature=0x%x\n", ipi->adv_feature);

	ret = of_property_read_u32(node, "ipi-auto-flush", &ipi->ipi_auto_flush);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read ipi-auto-flush\n");
	}

	ret = of_property_read_u32(node, "ipi-color-mode", &ipi->ipi_color_mode);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read ipi-color-mode\n");
	}

	ret = of_property_read_u32(node, "hsa", &ipi->hsa);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read hsa\n");
		ipi->hsa = 10;
	} else {
		dev_info(&pdev->dev, "mipi hsa=%d\n", ipi->hsa);
	}

	ret = of_property_read_u32(node, "hbp", &ipi->hbp);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read hbp\n");
		ipi->hbp = 20;
	} else {
		dev_info(&pdev->dev, "mipi hbp=%d\n", ipi->hbp);
	}

	ret = of_property_read_u32(node, "hsd", &ipi->hsd);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't read hsd\n");
		ipi->hsd = 0x60;
	} else {
		dev_info(&pdev->dev, "mipi hsd=%d\n", ipi->hsd);
	}

	ipi->virtual_ch = 0;
	for (i = 1; i < 4; i++) {
		memcpy(&kcmc->core.vch_hw[i], ipi, sizeof(struct csi_vch_info));
		kcmc->core.vch_hw[i].virtual_ch = i;
	}

	return 0;
}

static const struct of_device_id sdrv_mipi_csi2_dt_match[] = {
    {.compatible = "semidrive,sdrv-csi-mipi"},
    {},
};

static int sdrv_mipi_csi2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_csi_mipi_csi2 *kcmc;
	struct v4l2_subdev *sd;
	int ret;
	struct resource *res = NULL;

	dev_info(dev, "%s\n", __func__);

	kcmc = devm_kzalloc(dev, sizeof(*kcmc), GFP_KERNEL);
	if (!kcmc)
		return -ENOMEM;

	kcmc->dev = dev;
	sd = &kcmc->subdev;
	mutex_init(&kcmc->lock);
	kcmc->pdev = pdev;

	ret = dw_mipi_csi_parse_dt(pdev, kcmc);
	if (ret < 0) {
		dev_err(dev, "fail to parse dt\n");
		goto free_dev;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	kcmc->base_address = devm_ioremap_resource(dev, res);

	if (IS_ERR_OR_NULL(kcmc->base_address)) {
		dev_err(dev, "base address not right.\n");
		ret = PTR_ERR(kcmc->base_address);
		goto free_dev;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	mutex_lock(&res_mutex);
	if (s_dispmux_address == NULL) {
		kcmc->dispmux_address = devm_ioremap_resource(dev, res);

		if (IS_ERR_OR_NULL(kcmc->dispmux_address)) {
			dev_err(dev, "dispmux address not right.\n");
			ret = PTR_ERR(kcmc->dispmux_address);
			goto failed;
		}
		s_dispmux_address = kcmc->dispmux_address;
	} else {
		kcmc->dispmux_address = s_dispmux_address;
	}
	mutex_unlock(&res_mutex);

	kcmc->core.base = (reg_addr_t)kcmc->base_address;
	kcmc->core.dispmux_base = (reg_addr_t)kcmc->dispmux_address;

	kcmc->ctrl_irq_number = platform_get_irq(pdev, 0);

	if (kcmc->ctrl_irq_number <= 0) {
		dev_err(dev, "IRQ number not set.\n");
		ret = -EINVAL;
		goto failed;
	}

	ret = devm_request_irq(dev, kcmc->ctrl_irq_number, dw_mipi_csi_irq,
						IRQF_SHARED, dev_name(dev), kcmc);
	if (ret) {
		dev_err(dev, "IRQ failed\n");
		ret = -EINVAL;
		goto failed;
	}

	v4l2_subdev_init(sd, &sdrv_mipi_csi2_v4l2_ops);

	sd->owner = THIS_MODULE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sprintf(sd->name, "%s", SDRV_MIPI_CSI2_NAME);

	v4l2_set_subdevdata(sd, kcmc);

	sd->dev = dev;
	sd->entity.function = MEDIA_ENT_F_IO_V4L;
	sd->grp_id = CAM_SUBDEV_GRP_ID_MIPICSI_PARALLEL;

	platform_set_drvdata(pdev, kcmc);

	ret = v4l2_async_register_subdev(sd);
	if (ret < 0) {
		dev_err(dev, "Failed to register async subdev");
		goto failed;
	}

	mipi_csi2_enable(&kcmc->core,0);

	dev_info(dev, "%s done\n", __func__);
	return 0;

failed:
	if (!IS_ERR_OR_NULL(kcmc->base_address))
		devm_iounmap(dev, kcmc->base_address);

free_dev:
	mutex_destroy(&kcmc->lock);
	devm_kfree(dev, kcmc);

	return ret;
}

static int sdrv_mipi_csi2_remove(struct platform_device *pdev)
{
	struct sdrv_csi_mipi_csi2 *kcmc = platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = &kcmc->subdev;

	if (!IS_ERR_OR_NULL(kcmc->base_address))
		devm_iounmap(kcmc->dev, kcmc->base_address);

	v4l2_async_unregister_subdev(sd);
	mutex_destroy(&kcmc->lock);

	return 0;
}

MODULE_DEVICE_TABLE(of, sdrv_mipi_csi2_dt_match);

int mipicsi2_pm_suspend(struct device *dev)
{
	pr_err("mipicsi2: suspend\n");
	return 0;
}

int mipicsi2_pm_resume(struct device *dev)
{
	pr_err("mipicsi2: resume\n");
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops pm_ops = {
	.suspend_late = mipicsi2_pm_suspend,
	.resume_early = mipicsi2_pm_resume,
};
#else
int mipicsi2_dev_suspend(struct device *dev, pm_message_t state)
{
	mipicsi2_pm_suspend(dev);
	return 0;
}
int mipicsi2_dev_resume(struct device *dev)
{
	mipicsi2_pm_resume(dev);
	return 0;
}
#endif // CONFIG_PM


static struct platform_driver sdrv_mipi_csi2_driver = {
	.probe = sdrv_mipi_csi2_probe,
	.remove = sdrv_mipi_csi2_remove,
	.driver = {
		.name = SDRV_MIPI_CSI2_NAME,
		.of_match_table = sdrv_mipi_csi2_dt_match,
#ifdef CONFIG_PM
		.pm = &pm_ops,
#else
		.pm = NULL,
		.suspend = mipicsi2_dev_suspend,
		.resume = mipicsi2_dev_resume,
#endif // CONFIG_PM
	},
};

module_platform_driver(sdrv_mipi_csi2_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive mipi-csi2 driver");
MODULE_LICENSE("GPL v2");
