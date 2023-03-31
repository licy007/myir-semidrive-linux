/*
 * kunlun_drm_drv.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/component.h>
#include <linux/console.h>
#include <linux/iommu.h>
#include <linux/of_platform.h>
#include <linux/bitops.h>
#include <linux/dma-buf.h>

#include "drm_drv.h"
#include "drm_fbdev.h"
#include "drm_gem.h"
#include "rpc_define.h"
#include "sdrv_dpc.h"

static char *version = KO_VERSION;
module_param(version, charp, S_IRUGO);

#define KUNLUN_DRM_DRIVER_NAME		"semidrive"
#define KUNLUN_DRM_DRIVER_DESC		"semidrive Soc DRM"
#define KUNLUN_DRM_DRIVER_DATE		"20190424"
#define KUNLUN_DRM_DRIVER_MAJOR		1
#define KUNLUN_DRM_DRIVER_MINOR		0



#define DRM_IOCTL_SEMIDRIVE_MAP_PHYS 		DRM_IOWR(DRM_COMMAND_BASE + 1, struct drm_buffer_t)
#define DRM_IOCTL_SEMIDRIVE_UNMAP_PHYS 		DRM_IOWR(DRM_COMMAND_BASE + 2, struct drm_buffer_t)

#define DRM_IOCTL_SEMIDRIVE_EXPORT_DMABUF	DRM_IOWR(DRM_COMMAND_BASE + 3, struct drm_buffer_t)
#define DRM_IOCTL_SEMIDRIVE_RELEASE_DMABUF \
	DRM_IOWR(DRM_COMMAND_BASE + 5, struct drm_buffer_t)

#define DRM_IOCTL_SEMIDRIVE_CUSTOM_POST	DRM_IOWR(DRM_COMMAND_BASE + 4, struct sdrv_post_config)

extern const struct drm_framebuffer_funcs kunlun_drm_fb_funcs;

static struct drm_framebuffer *
kunlun_gem_fb_create(struct drm_device *drm,
		struct drm_file *file_priv, const struct drm_mode_fb_cmd2 *cmd)
{
	return drm_gem_fb_create_with_funcs(drm, file_priv, cmd,
			&kunlun_drm_fb_funcs);
}

static void kunlun_drm_output_poll_changed(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;

	drm_fb_helper_hotplug_event(kdrm->fb_helper);
}

static const struct drm_format_info ccs_formats[] = {
	{ .format = DRM_FORMAT_RGB888, .depth = 0, .num_planes = 3,
		.cpp = { 1, 1, 1}, .hsub = 1, .vsub = 1, },
	{ .format = DRM_FORMAT_BGR888, .depth = 0, .num_planes = 3,
		.cpp = { 1, 1, 1}, .hsub = 1, .vsub = 1, },

};

static const struct drm_format_info *
lookup_format_info(const struct drm_format_info formats[],
		   int num_formats, u32 format)
{
	int i;

	for (i = 0; i < num_formats; i++) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}

static const struct drm_format_info *
sdrv_get_format_info(const struct drm_mode_fb_cmd2 *cmd)
{
	switch (cmd->modifier[0]) {
	case DRM_FORMAT_MOD_SEMIDRIVE_RGB888_PLANE:
	case DRM_FORMAT_MOD_SEMIDRIVE_BGR888_PLANE:
		return lookup_format_info(ccs_formats,
					  ARRAY_SIZE(ccs_formats),
					  cmd->pixel_format);
	default:
		return NULL;
	}
}


static const struct drm_mode_config_funcs kunlun_mode_config_funcs = {
	.fb_create = kunlun_gem_fb_create,
	.get_format_info = sdrv_get_format_info,
	.output_poll_changed = kunlun_drm_output_poll_changed,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void kunlun_drm_lastclose(struct drm_device *drm)
{
	return;
	//struct kunlun_drm_data *kdrm = drm->dev_private;
	//if(kdrm->fb_helper)
	//	drm_fb_helper_restore_fbdev_mode_unlocked(kdrm->fb_helper);
}

int drm_get_physic_memory_ioctl(struct drm_device *drm, void *data, struct drm_file *file)
{

	 struct dma_buf *temp;
	 drm_buffer_t  *buf = (drm_buffer_t *)data;

	temp = dma_buf_get(buf->buf_handle);

	if (IS_ERR_OR_NULL(temp)) {
	    DRM_DEV_INFO(drm->dev,"[drm] map get dma buf error buf.buf_handle %d \n",buf->buf_handle);
	    return PTR_ERR(temp);
	}
	buf->attachment = (uint64_t)dma_buf_attach(temp, drm->dev);

	if (!buf->attachment) {
	    DRM_DEV_INFO(drm->dev,"[drm] get dma buf attach error \n");
	    return -1;
	}
	buf->sgt = (uint64_t) dma_buf_map_attachment((
											struct dma_buf_attachment *)buf->attachment, 0);

	if (!buf->sgt) {
	    DRM_DEV_INFO(drm->dev,"[drm] dma_buf_map_attachment error \n");
	    return -1;
	}
	buf->dma_addr  = sg_phys(((
			   struct sg_table *)(buf->sgt))->sgl);

	if (!buf->dma_addr) {
	    DRM_DEV_INFO(drm->dev,"[drm] sg_dma_address error \n");
	    return -1;
	}

	buf->size = sg_dma_len(((
			   struct sg_table *)(buf->sgt))->sgl);
	if (buf->size <= 0) {
	    DRM_DEV_INFO(drm->dev,"[drm] sg_dma_len error: len = %d\n", buf->size);
	    return -1;
	}

	dma_buf_put(temp);

	DRM_DEV_DEBUG(drm->dev, "get dma handle %d, dma buf addr %p, attachment %p, sgt %p\n",
	        buf->buf_handle,
	        (void *)buf->dma_addr,
	        (void *)buf->attachment,
	        (void *)buf->sgt);

	return 0;
}

int drm_release_physic_memory_ioctl(struct drm_device *drm, void *data, struct drm_file *file)
{
		struct dma_buf *temp;
		drm_buffer_t  *buf = (drm_buffer_t *)data;

		temp = dma_buf_get(buf->buf_handle);

		if (IS_ERR_OR_NULL(temp)) {
			DRM_DEV_INFO(drm->dev,"[drm] unmap get dma buf error buf.buf_handle %d \n",
					buf->buf_handle);
			return -1;
		}

		if ((NULL == (struct dma_buf_attachment *)(buf->attachment))
				|| ((NULL == (struct sg_table *)buf->sgt))) {
			DRM_DEV_INFO(drm->dev,"[drm] dma_buf_unmap_attachment param null  \n");
			return -1;
		}

		dma_buf_unmap_attachment((struct dma_buf_attachment *)(
									 buf->attachment),
								 (struct sg_table *)buf->sgt, 0);
		dma_buf_detach(temp, (struct dma_buf_attachment *)buf->attachment);
		drm_mb_free(temp);
		dma_buf_put(temp);
		temp = NULL;

		DRM_DEV_DEBUG(drm->dev, "dma unmap attachment success now \n" );


	return 0;
}


int drm_export_dmabuf_from_phys_ioctl(struct drm_device *drm, void *data, struct drm_file *file)
{

	drm_buffer_t  *drm_buf = (drm_buffer_t *)data;

	struct dma_buf *buf = drm_mb_export(drm,drm_buf->phys_addr, drm_buf->size, 2);

	if (IS_ERR(buf))
		return -EFAULT;

	drm_buf->buf_handle = dma_buf_fd(buf, O_CLOEXEC);

	return 0;
}


int drm_release_dmabuf_from_phys_ioctl(struct drm_device *drm, void *data,
		struct drm_file *file)
{

	struct dma_buf *temp;
	drm_buffer_t  *buf = (drm_buffer_t *) data;

	temp = dma_buf_get(buf->buf_handle);

	if (IS_ERR_OR_NULL(temp)) {
		DRM_DEV_INFO(drm->dev,
			"[drm] unmap get dma buf error buf.buf_handle %d\n",
			buf->buf_handle);
		return -1;
	}
	drm_mb_free(temp);
	dma_buf_put(temp);
	return 0;
}

extern int sdrv_custom_plane_update(struct drm_device *dev, struct sdrv_post_config *post);
int drm_sdrv_custom_post(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct sdrv_post_config *post = (struct sdrv_post_config *)data;
	if (!post) {
		DRM_ERROR("post pointer is invalid\n");
		return -EINVAL;
	}

	return sdrv_custom_plane_update(drm, post);
}

static const struct drm_ioctl_desc semidrive_ioctls[] = {
	DRM_IOCTL_DEF_DRV(SEMIDRIVE_MAP_PHYS, drm_get_physic_memory_ioctl, 0),
	DRM_IOCTL_DEF_DRV(SEMIDRIVE_UNMAP_PHYS, drm_release_physic_memory_ioctl, 0),
	DRM_IOCTL_DEF_DRV(SEMIDRIVE_EXPORT_DMABUF,
		drm_export_dmabuf_from_phys_ioctl, 0),
	DRM_IOCTL_DEF_DRV(SEMIDRIVE_RELEASE_DMABUF,
			drm_release_dmabuf_from_phys_ioctl, 0),
	DRM_IOCTL_DEF_DRV(SEMIDRIVE_CUSTOM_POST, drm_sdrv_custom_post, 0),
};

static const struct file_operations kunlun_drm_driver_fops = {
	.owner				= THIS_MODULE,
	.open				= drm_open,
	.mmap				= kunlun_drm_mmap,
	.poll				= drm_poll,
	.read				= drm_read,
	.unlocked_ioctl		= drm_ioctl,
	.compat_ioctl		= drm_compat_ioctl,
	.release			= drm_release,
};

static struct drm_driver kunlun_drm_driver = {
	.driver_features			= DRIVER_GEM | DRIVER_PRIME |
			DRIVER_RENDER | DRIVER_ATOMIC | DRIVER_MODESET,
	.lastclose					= kunlun_drm_lastclose,
	.gem_vm_ops					= &drm_gem_cma_vm_ops,
	.gem_free_object_unlocked	= kunlun_gem_free_object,
	.dumb_create				= kunlun_gem_dumb_create,
	.prime_handle_to_fd			= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle			= drm_gem_prime_fd_to_handle,
	.gem_prime_import			= drm_gem_prime_import,
	.gem_prime_export			= drm_gem_prime_export,
	.gem_prime_get_sg_table		= kunlun_gem_prime_get_sg_table,
	.gem_prime_import_sg_table	= kunlun_gem_prime_import_sg_table,
	.gem_prime_vmap				= kunlun_gem_prime_vmap,
	.gem_prime_vunmap			= kunlun_gem_prime_vunmap,
	.gem_prime_mmap				= kunlun_gem_prime_mmap,
	.fops 						= &kunlun_drm_driver_fops,

	.ioctls = semidrive_ioctls,
	.num_ioctls = ARRAY_SIZE(semidrive_ioctls),
	.name						= KUNLUN_DRM_DRIVER_NAME,
	.desc						= KUNLUN_DRM_DRIVER_DESC,
	.date						= KUNLUN_DRM_DRIVER_DATE,
	.major						= KUNLUN_DRM_DRIVER_MAJOR,
	.minor						= KUNLUN_DRM_DRIVER_MINOR,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int kunlun_drm_init_iommu(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;
	struct device *dev = drm->dev;
	struct device_node *iommu = NULL;
	struct property *prop = NULL;
	struct iommu_domain_geometry *geometry;
	u64 start, end;
	int ret = 0;

	kdrm->iommu_enable = false;
	iommu = of_parse_phandle(dev->of_node, "iommus", 0);
	if(!iommu) {
		DRM_DEV_DEBUG(dev, "iommu not specified\n");
		return ret;
	}
	if (!of_device_is_available(iommu)) {
		DRM_DEV_DEBUG(dev, "smmu disabled\n");
		return ret;
	}
	prop = of_find_property(dev->of_node, "smmu", NULL);
	if(!prop) {
		DRM_DEV_DEBUG(dev, "smmu bypassed\n");
		return ret;
	}

	kdrm->mm = kzalloc(sizeof(*kdrm->mm), GFP_KERNEL);
	if(!kdrm->mm) {
		ret = -ENOMEM;
		goto err_domain;
	}

	kdrm->domain = iommu_get_domain_for_dev(drm->dev);
	if(!kdrm->domain) {
		ret = -ENOMEM;
		goto err_free_mm;;
	}

	geometry = &kdrm->domain->geometry;
	start = geometry->aperture_start;
	end = GENMASK(37, 0); // 38 bits address for KUNLUN DP rdma

	DRM_DEV_DEBUG(dev, "IOMMU context initialized: %#llx - %#llx\n",
			start, end);
	drm_mm_init(kdrm->mm, start, end - start + 1);
	mutex_init(&kdrm->mm_lock);

	kdrm->iommu_enable = true;

	of_node_put(iommu);
	return ret;

err_free_mm:
	kfree(kdrm->mm);
err_domain:
	of_node_put(iommu);
	return ret;
}

static void kunlun_drm_iommu_cleanup(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;

	if(!kdrm->iommu_enable)
		return;

	drm_mm_takedown(kdrm->mm);
	iommu_domain_free(kdrm->domain);
	kfree(kdrm->mm);
}

static int kunlun_drm_bind(struct device *dev)
{
	struct drm_device *drm;
	struct kunlun_drm_data *kdrm;
	int ret;
	DRM_INFO("drm_drv bind\n");
	drm = drm_dev_alloc(&kunlun_drm_driver, dev);
	if(IS_ERR(drm)) {
		dev_err(dev, "failed to allocate drm_device\n");
		return PTR_ERR(drm);
	}

	kdrm = devm_kzalloc(dev, sizeof(*kdrm), GFP_KERNEL);
	if(!kdrm) {
		ret = -ENOMEM;
		goto err_unref;
	}

	drm->dev_private = kdrm;
	kdrm->drm = drm;

	drm->irq_enabled = true;

	ret = kunlun_drm_init_iommu(drm);
	if(ret)
		goto err_config_cleanup;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 0;
	drm->mode_config.min_height = 0;
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;

	drm->mode_config.allow_fb_modifiers = true;
	drm->mode_config.funcs = &kunlun_mode_config_funcs;

	ret = component_bind_all(dev, drm);
	if(ret) {
		dev_err(dev, "failed to bind component\n");
		goto err_iommu_cleanup;
	}

	dev_set_drvdata(dev, drm);

	if(!kdrm->num_crtcs) {
		dev_err(dev, "error crtc numbers\n");
		goto err_unbind;
	}

	ret = drm_vblank_init(drm, kdrm->num_crtcs);
	if(ret) {
		goto err_unbind;
	}

	drm_mode_config_reset(drm);

	ret = kunlun_drm_fbdev_init(drm);
	if(ret) {
		goto err_unbind;
	}

	drm_kms_helper_poll_init(drm);

	ret = drm_dev_register(drm, 0);
	if(ret)
		goto err_helper;

	return ret;

err_helper:
	drm_kms_helper_poll_fini(drm);
	kunlun_drm_fbdev_fini(drm);
err_unbind:
	dev_set_drvdata(dev, NULL);
	component_unbind_all(dev, drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);
err_iommu_cleanup:
	kunlun_drm_iommu_cleanup(drm);

err_unref:
	drm_dev_unref(drm);

	return ret;
}

static void kunlun_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);

	drm_kms_helper_poll_fini(drm);

	kunlun_drm_fbdev_fini(drm);

	dev_set_drvdata(dev, NULL);

	component_unbind_all(dev, drm);

	drm_mode_config_cleanup(drm);

	kunlun_drm_iommu_cleanup(drm);

	drm_dev_unref(drm);
}

static const struct component_master_ops kunlun_drm_ops = {
	.bind = kunlun_drm_bind,
	.unbind = kunlun_drm_unbind,
};

#ifdef CONFIG_PM_SLEEP
static void kunlun_drm_fb_suspend(struct drm_device *drm)
{
	struct kunlun_drm_data *priv = drm->dev_private;

	console_lock();
	drm_fb_helper_set_suspend(priv->fb_helper, true);
	console_unlock();
}

static void kunlun_drm_fb_resume(struct drm_device *drm)
{
	struct kunlun_drm_data *priv = drm->dev_private;

	console_lock();
	drm_fb_helper_set_suspend(priv->fb_helper, false);
	console_unlock();
}

static int kunlun_drm_sys_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct kunlun_drm_data *priv;
	int i;

	if (!drm)
		return 0;
	drm_kms_helper_poll_disable(drm);
	kunlun_drm_fb_suspend(drm);

	priv = drm->dev_private;
	priv->state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(priv->state)) {
		kunlun_drm_fb_resume(drm);
		drm_kms_helper_poll_enable(drm);
		return PTR_ERR(priv->state);
	}

	for (i = 0; i < priv->num_crtcs; i++) {
		kunlun_crtc_sys_suspend(priv->crtcs[i]);
	}

	return 0;
}

static int kunlun_drm_sys_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct kunlun_drm_data *priv;
	int i;

	if (!drm)
		return 0;

	priv = drm->dev_private;

	for (i = 0; i < priv->num_crtcs; i++) {
		kunlun_crtc_sys_resume(priv->crtcs[i]);
	}

	drm_atomic_helper_resume(drm, priv->state);
	kunlun_drm_fb_resume(drm);
	drm_kms_helper_poll_enable(drm);

	return 0;
}
#endif

static const struct dev_pm_ops kunlun_drm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(kunlun_drm_sys_suspend,
							kunlun_drm_sys_resume)
};

static int add_components_dsd(struct device *master, struct device_node *np,
		struct component_match **matchptr)
{
	struct device_node *ep_node, *intf;

	for_each_endpoint_of_node(np, ep_node) {
		intf = of_graph_get_remote_port_parent(ep_node);
		if(!intf)
			continue;

		drm_of_component_match_add(master, matchptr, compare_of, intf);
		DRM_DEV_INFO(master, "Add component %s", intf->name);
		of_node_put(intf);
	}

	return 0;
}

static int add_display_components(struct device *dev,
		struct component_match **matchptr)
{
	struct device_node *np;
	const struct kunlun_drm_device_info *kdd_info;
	int ret, i;

	kdd_info = of_device_get_match_data(dev);
	if(!kdd_info || !kdd_info->match)
		return -ENODEV;

	for (i = 0; ; i++) {
		np = of_parse_phandle(dev->of_node, "sdriv,crtc", i);
		if (!np)
			break;

		if(!of_device_is_available(np) || !kdd_info->match(np)) {
			DRM_DEV_INFO(dev, "OF node %s not available or match\n", np->name);
			continue;
		}

		drm_of_component_match_add(dev, matchptr, compare_of, np);
		DRM_DEV_INFO(dev, "Add component %s", np->name);

		ret = add_components_dsd(dev, np, matchptr);
		if(ret) {
			return ret;
		}
	}

	return ret;
}

static int __maybe_unused wait_safety_ready(void)
{
	uint32_t status;
	void __iomem *reg_base_ap = ioremap_nocache(SAF_WRITE_REG, 0x0f);
	status = *((uint32_t *)reg_base_ap);

	pr_info("[drm]: *begin* safety generic register status : 0x%x\n", status);
	if (status & BIT(0)) {
		while ((status & BIT(2)) == 0) {
			status = *((uint32_t *)reg_base_ap);
			mdelay(50);
		}
	}
	pr_info("[drm]: *end* safety generic register status : 0x%x\n", status);
	__iounmap(reg_base_ap);

	return 0;
}

static int kunlun_drm_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	int ret;

	DRM_INFO("DRM BUILD VERSION: %s\n", version);
	ret = add_display_components(&pdev->dev, &match);
	if(ret) {
		DRM_ERROR("add_display_components failed: %d\n", ret);
		return ret;
	}

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(38));
	if(ret)
		return ret;

	if(!match) {
		DRM_ERROR("drm not match\n");
		return -EINVAL;
	}
	DRM_INFO("component_master_add_with_match ...\n");
	return component_master_add_with_match(&pdev->dev,
			&kunlun_drm_ops, match);
}

static int kunlun_drm_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &kunlun_drm_ops);
	of_platform_depopulate(&pdev->dev);

	return 0;
}

static int compare_name_crtc(struct device_node *np)
{
	return strstr(np->name, "crtc") != NULL;
}

static const struct kunlun_drm_device_info kunlun_display_info = {
	.match = compare_name_crtc,
};

static const struct of_device_id kunlun_drm_of_table[] = {
	{.compatible = "semdrive,display-subsystem", .data = &kunlun_display_info },
	{},
};
MODULE_DEVICE_TABLE(of, kunlun_drm_of_table);

static struct platform_driver kunlun_drm_platform_driver = {
	.probe = kunlun_drm_probe,
	.remove = kunlun_drm_remove,
	.driver = {
		.name = "kunlun-drm",
		.of_match_table = kunlun_drm_of_table,
		.pm = &kunlun_drm_pm_ops,
	},
};
module_platform_driver(kunlun_drm_platform_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive Kunlun DRM driver");
MODULE_LICENSE("GPL");

