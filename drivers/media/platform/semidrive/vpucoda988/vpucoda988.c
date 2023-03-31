//--=========================================================================--
//	This file is linux device driver for VPU.
//-----------------------------------------------------------------------------
//
//	 This confidential and proprietary software may be used only
//	 as authorized by a licensing agreement from Chips&Media Inc.
//	 In the event of publication, the following notice is applicable:
//
//	 (C) COPYRIGHT 2006 - 2015	 CHIPS&MEDIA INC.
//			ALL RIGHTS RESERVED
//
//	 The entire notice above must be reproduced on all authorized
//	 copies.
//
//--=========================================================================-

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/sched/signal.h>

#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/semidrive/sdrv_scr.h>

#include "coda_config.h"
#include "vpuvirtioclient.h"


static int vpu_hw_reset(struct device *dev);
static int vpu_clk_enable(struct coda_dev *dev, bool enable);
static int vpu_vdi_lock(struct coda_dev *dev, unsigned int type);
static void vpu_vdi_unlock(struct coda_dev *dev, unsigned int type);

static int vpu_hw_reset(struct device *dev)
{
	struct reset_control *vpu_rst = NULL;

	if (dev)
	{
		vpu_rst = devm_reset_control_get(dev, "vpu-reset");
		if (IS_ERR(vpu_rst)) {
			pr_err("Error: missing controller reset\n");
			return -1;
		}

		if (reset_control_reset(vpu_rst)) {
			reset_control_put(vpu_rst);
			pr_err("reset vpu failed\n");
			return -1;
		}

		reset_control_put(vpu_rst);
	} else {
		pr_err("[VPU-CODA-DRV-ERR] no coda_device\n");
		return -1;
	}

	return 0;
}

static int vpu_clk_enable(struct coda_dev *dev, bool enable)
{
	int ret = 0;

	if (!dev)
		return -1;

	if (enable) {
		if(0 != (ret = clk_prepare(dev->clk_core))) {
			pr_err("[VPU-CODA-DRV-ERR] coda clk_core prepare return with error code %d \n", ret);
			return ret;
		}

		if(0 != (ret = clk_enable(dev->clk_core))) {
			pr_err("[VPU-CODA-DRV-ERR] coda clk_core enable return with error code %d \n", ret);
			return ret;
		}
	} else {
		clk_disable(dev->clk_core);
		clk_unprepare(dev->clk_core);
	}

	return 0;
}

static int vpu_vdi_lock(struct coda_dev *dev, unsigned int type)
{
	int ret;

	if (type == VPUDRV_MUTEX_VPU) {
		ret = down_interruptible(&dev->vpu_sema);
	} else if (type == VPUDRV_MUTEX_DISP) {
		ret = down_interruptible(&dev->disp_sema);
	} else if (type == VPUDRV_MUTEX_GATE) {
		ret = down_interruptible(&dev->gate_sema);
	} else {
		pr_err("[VPU-CODA-DRV-ERR] %s unknown MUTEX_TYPE type=%d\n", __FUNCTION__, type);
		return -EFAULT;
	}

	if (ret != 0) {
		pr_err("[VPU-CODA-DRV-ERR] %s down_interruptible error ret=%d\n", __FUNCTION__, ret);
	}

	return ret;
}

static void vpu_vdi_unlock(struct coda_dev *dev, unsigned int type)
{
	if (type == VPUDRV_MUTEX_VPU) {
		up(&dev->vpu_sema);
	} else if (type == VPUDRV_MUTEX_DISP) {
		up(&dev->disp_sema);
	} else if (type == VPUDRV_MUTEX_GATE) {
		up(&dev->gate_sema);
	} else {
		pr_err("[VPU-CODA-DRV-ERR] %s unknown MUTEX_TYPE type=%d\n", __FUNCTION__, type);
	}
}

static int vpu_alloc_common_buffer(struct coda_dev *dev, vpudrv_buffer_t *vb)
{
	if (!vb)
		return -1;

	vb->base = (unsigned long) dma_alloc_coherent(dev->dev,
			   PAGE_ALIGN(vb->size), (dma_addr_t *)(&vb->dma_addr),
			   GFP_DMA | GFP_KERNEL | GFP_USER);
	vb->phys_addr = vb->dma_addr;

	if ((void *)(vb->base) == NULL)	 {
		pr_err("[VPU-CODA-DRV-ERR] Physical memory allocation error size=%d\n",
				vb->size);
		return -1;
	}

	pr_info("[VPU-CODA-DRV] common buffer allocate info :vpucoda_device %p, handle %d , virt %p,  phy %p, dma addr %p, size %d\n",
			dev->dev,
			vb->buf_handle,
			(void *)vb->base,
			(void *)vb->phys_addr,
			(void *)vb->dma_addr,
			vb->size);

	return 0;
}

static int vpu_alloc_dma_buffer(struct coda_dev *dev, vpudrv_buffer_t *vb)
{
	if (!vb)
		return -1;

	vb->base = (unsigned long)dma_alloc_coherent(dev->dev,
			   PAGE_ALIGN(vb->size),
			   (dma_addr_t *) (&vb->dma_addr), GFP_DMA | GFP_KERNEL);

	if ((void *)(vb->base) == NULL) {
		pr_err( "[VPU-CODA-DRV-ERR] Physical memory allocation error size=%d\n",
				 vb->size);
		return -1;
	}

	vb->phys_addr = vb->dma_addr;
	return 0;
}

static void vpu_free_dma_buffer(struct coda_dev *dev, vpudrv_buffer_t *vb)
{
	if (!vb)
		return;

	if (vb->base)
		dma_free_coherent(dev->dev, PAGE_ALIGN(vb->size),
						  (void *)vb->base,
						  vb->dma_addr);

	pr_info("[VPU-CODA-DRV] vpu_free dma buffer coda device %p \n", dev->dev);
}

static int vpu_free_instances(struct file *filp)
{
	void *vip_base;
	void *vdi_mutexes_base;
	vpudrv_instance_pool_t *vip;
	vpudrv_instanace_list_t *vil, *n;
	int instance_pool_size_per_core;

	const int PTHREAD_MUTEX_T_DESTROY_VALUE = 0xdead10cc;
	struct coda_dev *dev = (struct coda_dev *)filp->private_data;

	pr_info("[VPU-CODA-DRV] vpu_free_instances\n");

	/* instance_pool.size  assigned to the size of all core once call VDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = dev->instance_pool.size;

	list_for_each_entry_safe(vil, n, &dev->inst_list, list) {
		if (vil->filp == filp) {
			if (dev->vpu_virtio_enable) {
				pr_err("[VPU-CODA-DRV] vpu_free_instances detect instance crash instIdx=%u, "
					   "coreIdx=%d\n", vil->inst_idx, (int)vil->core_idx);
				vpu_virtio_send_clear_instance(VPU_DEVICES_CODA988, vil->inst_idx);
			} else {
				vip_base = (void *)(dev->instance_pool.base);
				pr_err("[VPU-CODA-DRV] vpu_free_instances detect instance crash instIdx=%d, "
					   "coreIdx=%d, vip_base=%p, instance_pool_size_per_core=%d\n",
					   (int)vil->inst_idx, (int)vil->core_idx, vip_base,
					   (int)instance_pool_size_per_core);
				vip = (vpudrv_instance_pool_t *)vip_base;

				if (vip) {
					/* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the
					 * corresponding instance. */
					memset(&vip->inst_pool[vil->inst_idx], 0x00, 4);

					vdi_mutexes_base = (vip_base + (instance_pool_size_per_core -
													PTHREAD_MUTEX_T_HANDLE_SIZE * 4));
					pr_err("[VPU-CODA-DRV] vpu_free_instances : force to destroy "
						   "vdi_mutexes_base=%p in userspace \n",
						   vdi_mutexes_base);

					if (vdi_mutexes_base) {
						int i;

						for (i = 0; i < 4; i++) {
							memcpy(vdi_mutexes_base, &PTHREAD_MUTEX_T_DESTROY_VALUE,
								   PTHREAD_MUTEX_T_HANDLE_SIZE);
							vdi_mutexes_base += PTHREAD_MUTEX_T_HANDLE_SIZE;
						}
					}
				}
			}

			dev->instance_ref_count--;
			list_del(&vil->list);
			kfree(vil);
		}
	}
	return 1;
}

static int vpu_free_buffers(struct file *filp)
{
	vpudrv_buffer_pool_t *pool, *n;
	vpudrv_buffer_t vb;
	struct coda_dev *dev = (struct coda_dev *)filp->private_data;
	pr_info("[VPU-CODA-DRV] vpu_free_buffers\n");

	list_for_each_entry_safe(pool, n, &dev->vbp_list, list) {
		if (pool->filp == filp) {
			vb = pool->vb;
			pr_err("[VPU-CODA-DRV] vpu_free_buffers ongoing\n");
			if (vb.base) {
				vpu_free_dma_buffer(dev, &vb);
				list_del(&pool->list);
				kfree(pool);
			}
		}
	}

	return 0;
}

static irqreturn_t vpu_irq_handler(int irq, void *data)
{
	struct coda_dev *dev = (struct coda_dev *)data;
	int product_code;

	/* it means that we didn't get an information the current core from API layer. No core activated.*/
	if (dev->bit_firmware_info.size == 0) {
		pr_err("[VPU-CODA-DRV-ERR] : bit_firmware_info.size is zero\n");
	}

	product_code = READ_VPU_REGISTER(dev, VPU_PRODUCT_CODE_REGISTER);

	if (likely(PRODUCT_CODE_CODA988(product_code))) {
		spin_lock(&dev->irqlock);
		if (READ_VPU_REGISTER(dev, BIT_INT_STS)) {
			dev->interrupt_reason = READ_VPU_REGISTER(dev, BIT_INT_REASON);
			WRITE_VPU_REGISTER(dev, BIT_INT_CLEAR, 0x1);
		}
		dev->interrupt_flag = 1;
		spin_unlock(&dev->irqlock);

		wake_up_interruptible(&dev->interrupt_wait_q);
	}
	else {
		pr_err("[VPU-CODA-DRV-ERR] Unknown product id : %08x\n", product_code);
	}

	return IRQ_HANDLED;
}

static int vpu_open(struct inode *inode, struct file *filp)
{
	struct coda_dev *dev = container_of(inode->i_cdev, struct coda_dev, cdev_coda);
	pr_info("[VPU-CODA-DRV] coda driver %s\n", __func__);

	mutex_lock(&dev->coda_mutex);
	dev->open_count++;
	filp->private_data = (void *)dev;
	mutex_unlock(&dev->coda_mutex);

	return 0;
}

static long vpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;
	struct coda_dev *dev = (struct coda_dev *)filp->private_data;

	switch (cmd) {
	case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
		{
			vpudrv_buffer_pool_t *vbp;

			pr_info("[VPU-CODA-DRV] VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");

			vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
			if (!vbp) {
				return -ENOMEM;
			}

			ret = copy_from_user(&vbp->vb, (vpudrv_buffer_t *)arg,
								 sizeof(vpudrv_buffer_t));

			if (ret) {
				kfree(vbp);
				return -EFAULT;
			}

			ret = vpu_alloc_dma_buffer(dev, &vbp->vb);

			if (ret == -1) {
				ret = -ENOMEM;
				kfree(vbp);
				break;
			}

			ret = copy_to_user((void __user *)arg, &vbp->vb,
							   sizeof(vpudrv_buffer_t));

			if (ret) {
				vpu_free_dma_buffer(dev, &vbp->vb);
				kfree(vbp);
				ret = -EFAULT;
				break;
			}

			vbp->filp = filp;

			mutex_lock(&dev->coda_mutex);
			list_add(&vbp->list, &dev->vbp_list);
			mutex_unlock(&dev->coda_mutex);
		}
		break;

	case VDI_IOCTL_FREE_PHYSICALMEMORY:
		{
			vpudrv_buffer_pool_t *vbp, *n;
			vpudrv_buffer_t vb;
			pr_info("[VPU-CODA-DRV] VDI_IOCTL_FREE_PHYSICALMEMORY\n");

			ret = copy_from_user(&vb, (vpudrv_buffer_t *)arg,
								 sizeof(vpudrv_buffer_t));

			if (ret) {
				return -EACCES;
			}

			if (vb.base) {
				mutex_lock(&dev->coda_mutex);
				list_for_each_entry_safe(vbp, n, &dev->vbp_list, list) {
					if (vbp->vb.base == vb.base) {
						list_del(&vbp->list);
						kfree(vbp);
						break;
					}
				}
				mutex_unlock(&dev->coda_mutex);
				vpu_free_dma_buffer(dev, &vb);
			}
		}
		break;

	case VDI_IOCTL_WAIT_INTERRUPT:
		{
			if (dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable return directly\n");
				return -EFAULT;
			}

			vpudrv_intr_info_t info;
			ret = copy_from_user(&info, (vpudrv_intr_info_t *)arg,
								 sizeof(vpudrv_intr_info_t));

			if (ret != 0)
				return -EFAULT;

			ret = wait_event_interruptible_timeout(dev->interrupt_wait_q,
												   dev->interrupt_flag!= 0,
												   msecs_to_jiffies(info.timeout));

			if (!ret) {
				ret = -ETIME;
				break;
			}

			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				pr_err("[VPU-CODA-DRV-ERR]	Signal is received now \n");
				break;
			}

			spin_lock_irq(&dev->irqlock);
			info.intr_reason = dev->interrupt_reason;
			dev->interrupt_flag = 0;
			spin_unlock_irq(&dev->irqlock);

			ret = copy_to_user((void __user *)arg, &info,
							   sizeof(vpudrv_intr_info_t));

			if (ret != 0)
				return -EFAULT;
		}

		break;

	case VDI_IOCTL_SET_CLOCK_GATE:
		{
			u32 enable;
			if (dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable return directly\n");
				return -EFAULT;
			}

			if (get_user(enable, (u32 __user *) arg))
				return -EFAULT;

			mutex_lock(&dev->coda_mutex);
			vpu_clk_enable(dev, enable);
			mutex_unlock(&dev->coda_mutex);
		}
		break;

	case VDI_IOCTL_GET_INSTANCE_POOL:
		{
			pr_info("[VPU-CODA-DRV] VDI_IOCTL_GET_INSTANCE_POOL\n");

			mutex_lock(&dev->coda_mutex);
			if (dev->instance_pool.base != 0) {

				ret = copy_to_user((void __user *)arg, &dev->instance_pool,
								   sizeof(vpudrv_buffer_t));

				if (ret != 0)
					ret = -EFAULT;
			} else {
				ret = copy_from_user(&dev->instance_pool, (vpudrv_buffer_t *)arg,
									 sizeof(vpudrv_buffer_t));

				if (ret == 0) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
					dev->instance_pool.size = PAGE_ALIGN(dev->instance_pool.size);
					dev->instance_pool.base = (unsigned long)vmalloc(dev->instance_pool.size);
					/* pool phys_addr do nothing */
					dev->instance_pool.phys_addr = dev->instance_pool.base;

					if (dev->instance_pool.base != 0)
#else

					if (vpu_alloc_dma_buffer(&dev->instance_pool) != -1)
#endif
					{
						memset((void *)dev->instance_pool.base, 0x0,
							   dev->instance_pool.size); /*clearing memory*/
						ret = copy_to_user((void __user *)arg, &dev->instance_pool,
										   sizeof(vpudrv_buffer_t));

						if (ret) {
							if (dev->instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
								vfree((const void *)dev->instance_pool.base);
#else
								vpu_free_dma_buffer(&dev->instance_pool)
#endif
							}
							dev->instance_pool.base = 0;
							ret = -EFAULT;
						}
					}
				}
			}
			mutex_unlock(&dev->coda_mutex);
		}
		break;

	case VDI_IOCTL_GET_COMMON_MEMORY:
		{
			pr_info("[VPU-CODA-DRV] VDI_IOCTL_GET_COMMON_MEMORY\n");

			if (dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable return directly\n");
				return -EFAULT;
			}

			mutex_lock(&dev->coda_mutex);
			if (dev->common_memory.base != 0) {
				ret = copy_to_user((void __user *)arg, &dev->common_memory,
								   sizeof(vpudrv_buffer_t));

				if (ret != 0)
					ret = -EFAULT;
			}
			else {
				ret = copy_from_user(&dev->common_memory, (vpudrv_buffer_t *)arg,
									 sizeof(vpudrv_buffer_t));

				if (ret == 0) {
					if (vpu_alloc_common_buffer(dev, &dev->common_memory) != -1) {
						ret = copy_to_user((void __user *) arg, &dev->common_memory,
										   sizeof(vpudrv_buffer_t));
						if (ret) {
							if (dev->common_memory.base) {
								vpu_free_dma_buffer(dev, &dev->common_memory);
								dev->common_memory.base = 0;
							}
							ret = -EFAULT;
						}
					}
				}
			}
			mutex_unlock(&dev->coda_mutex);
		}
		break;

	case VDI_IOCTL_OPEN_INSTANCE:
		{
			vpudrv_inst_info_t inst_info;
			vpudrv_instanace_list_t *vil, *n;

			if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
							   sizeof(vpudrv_inst_info_t)))
				return -EFAULT;

			vil = kzalloc(sizeof(*vil), GFP_KERNEL);
			if (!vil)
				return -ENOMEM;

			vil->inst_idx = inst_info.inst_idx;
			vil->core_idx = inst_info.core_idx;
			vil->filp = filp;

			mutex_lock(&dev->coda_mutex);
			list_add(&vil->list, &dev->inst_list);

			inst_info.inst_open_count = 0; /* current open instance number */
			list_for_each_entry_safe(vil, n, &dev->inst_list, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}

			dev->instance_ref_count++;
			mutex_unlock(&dev->coda_mutex);

			if (copy_to_user((void __user *)arg, &inst_info,
								sizeof(vpudrv_inst_info_t))) {
				kfree(vil);
				return -EFAULT;
			}

			pr_info("[VPU-CODA-DRV] VDI_IOCTL_OPEN_INSTANCE core_idx=%d, inst_idx=%d, vpu_coda_open_ref_count=%d, inst_open_count=%d\n",
					(int)inst_info.core_idx, (int)inst_info.inst_idx, dev->instance_ref_count, inst_info.inst_open_count);
		}
		break;

	case VDI_IOCTL_CLOSE_INSTANCE:
		{
			vpudrv_inst_info_t inst_info;
			vpudrv_instanace_list_t *vil, *n;

			if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
							   sizeof(vpudrv_inst_info_t)))
				return -EFAULT;

			mutex_lock(&dev->coda_mutex);
			list_for_each_entry_safe(vil, n, &dev->inst_list, list) {
				if (vil->inst_idx == inst_info.inst_idx
						&& vil->core_idx == inst_info.core_idx) {
					list_del(&vil->list);
					kfree(vil);
					break;
				}
			}

			inst_info.inst_open_count = 0;
			list_for_each_entry_safe(vil, n, &dev->inst_list, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}

			dev->instance_ref_count--;
			mutex_unlock(&dev->coda_mutex);

			if (copy_to_user((void __user *)arg, &inst_info,
							 sizeof(vpudrv_inst_info_t)))
				return -EFAULT;

			pr_info("[VPU-CODA-DRV] VDI_IOCTL_CLOSE_INSTANCE core_idx=%d, inst_idx=%d, vpu_coda_open_ref_count=%d, inst_open_count=%d\n",
					(int)inst_info.core_idx, (int)inst_info.inst_idx,
					dev->instance_ref_count,
					inst_info.inst_open_count);
		}
		break;

	case VDI_IOCTL_GET_INSTANCE_NUM:
		{
			vpudrv_inst_info_t inst_info;
			vpudrv_instanace_list_t *vil, *n;

			ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
								 sizeof(vpudrv_inst_info_t));

			if (ret != 0)
				break;

			inst_info.inst_open_count = 0;

			mutex_lock(&dev->coda_mutex);
			list_for_each_entry_safe(vil, n, &dev->inst_list, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}
			mutex_unlock(&dev->coda_mutex);

			ret = copy_to_user((void __user *)arg, &inst_info,
							   sizeof(vpudrv_inst_info_t));

			pr_info("[VPU-CODA-DRV] VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n",
					(int)inst_info.core_idx, (int)inst_info.inst_idx, inst_info.inst_open_count);
		}
		break;

	case VDI_IOCTL_RESET:
		{
			if (dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable return directly\n");
				return -EFAULT;
			}
			ret = vpu_hw_reset(dev->dev);
		}
		break;

	case VDI_IOCTL_GET_REGISTER_INFO:
		{
			ret = copy_to_user((void __user *)arg, &dev->registers,
							   sizeof(vpudrv_buffer_t));

			if (ret != 0)
				ret = -EFAULT;

			pr_info("[VPU-CODA-DRV] VDI_IOCTL_GET_REGISTER_INFO s_vpu_register.phys_addr %llx, s_vpu_register.virt_addr %llx, s_vpu_register.size %d\n",
					 dev->registers.phys_addr, dev->registers.virt_addr, dev->registers.size);
		}
		break;

	case VDI_IOCTL_DEVICE_MEMORY_MAP:
		{
			struct dma_buf *temp = NULL;
			vpudrv_buffer_t	 buf = {0};

			if (copy_from_user(&buf,  (void __user *)arg,
							   sizeof(vpudrv_buffer_t)))
				return -1;

			if (NULL == (temp = dma_buf_get(buf.buf_handle))) {
				pr_err("[VPU-CODA-DRV-ERR] get dma buf error \n");
				return -1;
			}

			if (IS_ERR(temp)) {
				pr_err("[VPU-CODA-DRV-ERR] get dma buf error 0x%lx\n", PTR_ERR(temp));
				return PTR_ERR(temp);
			}

			if (NULL == (void *)(buf.attachment = (uint64_t)dma_buf_attach(temp,
												  dev->dev))) {
				pr_err("[VPU-CODA-DRV-ERR] get dma buf attach error \n");
				return -1;
			}

			if (IS_ERR((void *)buf.attachment)) {
				pr_err("[VPU-CODA-DRV-ERR] get dma buf attach error 0x%lx\n", PTR_ERR((void *)buf.attachment));
				return PTR_ERR((void *)buf.attachment);
			}

			if (NULL ==	 (void *)(buf.sgt = (uint64_t) dma_buf_map_attachment((
												struct dma_buf_attachment *)buf.attachment, 0))) {
				pr_err("[VPU-CODA-DRV-ERR] dma_buf_map_attachment error \n");
				return -1;
			}

			if (IS_ERR((void *)buf.sgt)) {
				pr_err("[VPU-CODA-DRV-ERR] dma_buf_map_attachment error 0x%lx\n", PTR_ERR((void *)buf.sgt));
				return PTR_ERR((void *)buf.sgt);
			}

			if (NULL == (void *)( buf.dma_addr	= sg_dma_address(((
					struct sg_table *)(buf.sgt))->sgl))) {
				pr_err("[VPU-CODA-DRV-ERR] sg_dma_address error \n");
				return -1;
			}

			if (IS_ERR((void *)buf.dma_addr)) {
				pr_err("[VPU-CODA-DRV-ERR] sg_dma_address error 0x%lx\n", PTR_ERR((void *)buf.dma_addr));
				return PTR_ERR((void *)buf.dma_addr);
			}

			buf.phys_addr = buf.dma_addr;

			if (copy_to_user((void __user *)arg, &buf, sizeof(vpudrv_buffer_t)))
				return -1;

			dma_buf_put(temp);
		}
		break;

	case VDI_IOCTL_DEVICE_MEMORY_UNMAP:
		{
			struct dma_buf *temp = NULL;
			vpudrv_buffer_t	 buf = {0};

			if (copy_from_user(&buf,  (void __user *)arg,
							   sizeof(vpudrv_buffer_t)))
				return -1;

			if (NULL == (temp = dma_buf_get(buf.buf_handle))) {
				pr_err("[VPU-CODA-DRV-ERR] unmap get dma buf error buf.buf_handle %d \n",
						buf.buf_handle);
				return -1;
			}

			if (IS_ERR(temp)) {
				pr_err("[VPU-CODA-DRV-ERR] unmap get dma buf error 0x%lx\n", PTR_ERR(temp));
				return PTR_ERR(temp);
			}

			if ((NULL == (struct dma_buf_attachment *)(buf.attachment))
					|| ((NULL == (struct sg_table *)buf.sgt))) {
				pr_err("[VPU-CODA-DRV-ERR] dma_buf_unmap_attachment param null	\n");
				return -1;
			}

			if (IS_ERR((void *)buf.attachment)) {
				pr_err("[VPU-CODA-DRV-ERR] dma_buf_unmap_attachment error 0x%lx\n", PTR_ERR((void *)buf.attachment));
				return PTR_ERR((void *)buf.attachment);
			}

			if (IS_ERR((void *)buf.sgt)) {
				pr_err("[VPU-CODA-DRV-ERR] sg_dma_address error 0x%lx\n", PTR_ERR((void *)buf.sgt));
				return PTR_ERR((void *)buf.sgt);
			}

			dma_buf_unmap_attachment((struct dma_buf_attachment *)(
										 buf.attachment),
									 (struct sg_table *)buf.sgt, 0);
			dma_buf_detach(temp, (struct dma_buf_attachment *)buf.attachment);
			dma_buf_put(temp);
			temp = NULL;
		}
		break;

	case VDI_IOCTL_DEVICE_SRAM_CFG:
		{
			uint32_t mode = 0;

			if (dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable return directly\n");
				return -EFAULT;
			}

			if (get_user(mode, (u32 __user *) arg)) {
				pr_err("[VPU-CODA-DRV-ERR]	sram cfg error get_user\n");
				return -EFAULT;
			}

			if (!dev->scr_signal) {
				pr_err("[VPU-CODA-DRV-ERR]	scr signal value null \n");
				return -EFAULT;
			}

			mutex_lock(&dev->coda_mutex);
			/* using  scr-api to set sram/linebuffer mode  */
			ret = sdrv_scr_set(SCR_SEC, dev->scr_signal, mode);
			mutex_unlock(&dev->coda_mutex);

			if (0 != ret) {
				pr_err("[VPU-CODA-DRV-ERR]	scr config error, ret %d , mode %d \n", ret, mode);
				return ret;
			}
		}
		break;

	case VDI_IOCTL_DEVICE_GET_SRAM_INFO:
		{
			/* copy sram_info to user space;  note: two srams */
			if (copy_to_user((void __user *)arg, &(dev->sram[0]), sizeof(struct sram_info) * 2))
				return -EFAULT;

		}
		break;

	case VDI_IOCTL_MEMORY_CACHE_REFRESH:
		{
			vpudrv_buffer_t buf = {0};

			if (copy_from_user(&buf,  (void __user *)arg, sizeof(vpudrv_buffer_t))) {
				pr_err("[VPU-CODA-DRV-ERR]	copy user param error\n");
				return -EFAULT;
			}

			if (buf.data_direction == DMA_TO_DEVICE)
				dma_sync_single_for_device(dev->dev, buf.dma_addr,buf.size, DMA_TO_DEVICE);

			else if (buf.data_direction == DMA_FROM_DEVICE)
				dma_sync_single_for_cpu(dev->dev, buf.dma_addr,buf.size, DMA_FROM_DEVICE);

			else {
				pr_err("[VPU-CODA-DRV-ERR] memory cache refresh direction %d error \n", buf.data_direction);
				return -1;
			}
		}
		break;

	case VDI_IOCTL_VDI_LOCK:
		{
			if (dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable return directly\n");
				return -EFAULT;
			}

			unsigned int mutex_type;
			ret = copy_from_user(&mutex_type, (void __user *)arg, sizeof(unsigned int));
			if (ret != 0)
				return -EFAULT;

			ret = vpu_vdi_lock(dev, mutex_type);
		}
		break;

	case VDI_IOCTL_VDI_UNLOCK:
		{
			if (dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable return directly\n");
				return -EFAULT;
			}

			unsigned int mutex_type;
			ret = copy_from_user(&mutex_type, (void __user *)arg, sizeof(unsigned int));
			if (ret != 0)
				return -EFAULT;

			vpu_vdi_unlock(dev, mutex_type);
		}
		break;

	case VDI_IOCTL_SEND_CMD_MSG:
		{
			pr_debug("[VPU-CODA-DRV] ioctl cmd:%d\n", VDI_IOCTL_SEND_CMD_MSG);

			if (!dev->vpu_virtio_enable) {
				pr_err("[VPUDRV] vpu_virtio_enable is false, return directly\n");
				return -EFAULT;
			}

			mutex_lock(&dev->virtio_mutex);

			ret = copy_from_user(&dev->ipcc_msg, (vpu_ipcc_data_t *)arg, sizeof(vpu_ipcc_data_t));

			if (ret)
				goto done;

			ret = vpu_virtio_send_sync(VPU_DEVICES_CODA988, &dev->ipcc_msg);

			if (ret)
				goto done;

			ret = copy_to_user((void __user *)arg, &dev->ipcc_msg, sizeof(vpu_ipcc_data_t));
			if (ret)
				ret = -EFAULT;

		done:
			mutex_unlock(&dev->virtio_mutex);
			pr_debug("[VPU-CODA-DRV] ioctl cmd:%d, ret:%d\n", VDI_IOCTL_SEND_CMD_MSG, ret);
		}
		break;

	default:
		pr_err("[VPU-CODA-DRV-ERR] No such IOCTL, cmd is %d\n", cmd);
		break;
	}

	return ret;
}

static ssize_t vpu_read(struct file *filp, char __user *buf,
						size_t len,
						loff_t *ppos)
{

	return -1;
}

static ssize_t vpu_write(struct file *filp, const char __user *buf,
						 size_t len,
						 loff_t *ppos)
{
	int ret = 0;
	struct coda_dev *dev = (struct coda_dev *)filp->private_data;

	vpu_bit_firmware_info_t *bit_firmware_info = NULL;

	if (!buf) {
		pr_err( "[VPU-CODA-DRV] vpu_write buf = NULL error \n");
		return -EFAULT;
	}

	if (len == sizeof(vpu_bit_firmware_info_t)) {
		bit_firmware_info = kmalloc(sizeof(vpu_bit_firmware_info_t),
									GFP_KERNEL);

		if (!bit_firmware_info) {
			pr_err("[VPU-CODA-DRV-ERR] vpu_write  bit_firmware_info allocation error \n");
			ret = -EFAULT;
			goto err_free_mm;
		}

		if (copy_from_user(bit_firmware_info, buf, len)) {
			pr_err("[VPU-CODA-DRV-ERR] vpu_write copy_from_user error for bit_firmware_info\n");
			ret = -EFAULT;
			goto err_free_mm;
		}

		if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
			pr_info("[VPU-CODA-DRV] vpu_write set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
					bit_firmware_info->core_idx, (int)bit_firmware_info->reg_base_offset,
					bit_firmware_info->size, bit_firmware_info->bit_code[0]);

			if (bit_firmware_info->core_idx != CODA_CORE_ID) {
				pr_err("[VPU-CODA-DRV] vpu_write coreIdx[%d] \n",bit_firmware_info->core_idx);
				ret = -ENODEV;
				goto err_free_mm;
			}

			memcpy((void *)&dev->bit_firmware_info, bit_firmware_info,
				   sizeof(vpu_bit_firmware_info_t));

			ret = len;
		}
	}

err_free_mm:
	if (NULL != bit_firmware_info)
		kfree(bit_firmware_info);

	return ret;
}

static int vpu_release(struct inode *inode, struct file *filp)
{
	struct coda_dev *dev = (struct coda_dev *)filp->private_data;

	pr_info("[VPU-CODA-DRV] vpu_release\n");

	mutex_lock(&dev->coda_mutex);

	/* found and free the not handled buffer by user applications */
	vpu_free_buffers(filp);

	/* found and free the not closed instance by user applications */
	vpu_free_instances(filp);
	dev->open_count--;
	pr_info("[VPU-CODA-DRV] coda driver release, open count is %d\n", dev->open_count);
	if (dev->open_count == 0) {
		if (dev->instance_pool.base) {
			pr_info("[VPU-CODA-DRV] free instance pool\n");
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
			vfree((const void *)dev->instance_pool.base);
#else
			vpu_free_dma_buffer(&dev->instance_pool);
#endif
			dev->instance_pool.base = 0;
		}
	}

	mutex_unlock(&dev->coda_mutex);
	return 0;
}

static int vpu_map_to_register(struct coda_dev *dev,
							   struct vm_area_struct *vm)
{
	unsigned long pfn;

	vm->vm_flags |= VM_IO | VM_RESERVED;
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn = dev->registers.phys_addr >> PAGE_SHIFT;
	return remap_pfn_range(vm, vm->vm_start, pfn,
						   vm->vm_end - vm->vm_start,
						   vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_physical_memory(struct file *filp, struct vm_area_struct *vm)
{
	bool found = false;
	struct vpudrv_buffer_pool_t *pool = NULL, *n = NULL;
	vpudrv_buffer_t vb = {0};
	struct coda_dev *dev = (struct coda_dev *)filp->private_data;

	if (!dev) {
		pr_err("error: %s get coda dev error\n", __func__);
		return -ENOMEM;
	}

	list_for_each_entry_safe(pool, n, &dev->vbp_list, list)
	{
		if (pool->filp == filp) {
			vb = pool->vb;
			if (vb.phys_addr >> PAGE_SHIFT == vm->vm_pgoff) { // find me
				found = true;
				break;
			}
		}
	}

	if (!found) {
		// this memory is requested by other places
		vm->vm_flags |= VM_IO | VM_RESERVED;
		// vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
		vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot);
		return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end - vm->vm_start,
							   vm->vm_page_prot)
				   ? -EAGAIN
				   : 0;
	}

	pr_info("map dma buffer vb.dma_addr %p base %p\n", (void *)vb.dma_addr, (void *)vb.base);
	vm->vm_pgoff = 0;
	return dma_mmap_coherent(dev->dev, vm, (void *)vb.base, vb.dma_addr, vm->vm_end - vm->vm_start);
}

static int vpu_map_to_instance_pool_memory(struct coda_dev *dev,
												  struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	int ret;
	long length = vm->vm_end - vm->vm_start;
	unsigned long start = vm->vm_start;
	char *vmalloc_area_ptr = (char *)dev->instance_pool.base;
	unsigned long pfn;

	vm->vm_flags |= VM_RESERVED;

	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);

		if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE,
								   PAGE_SHARED)) < 0) {
			return ret;
		}

		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}


	return 0;
#else
	vm->vm_flags |= VM_RESERVED;
	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
						   vm->vm_end - vm->vm_start,
						   vm->vm_page_prot) ? -EAGAIN : 0;
#endif
}

static int vpu_mmap(struct file *filp, struct vm_area_struct *vm)
{
	struct coda_dev *dev = (struct coda_dev *)filp->private_data;

#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	if (vm->vm_pgoff == 0)
		return vpu_map_to_instance_pool_memory(dev, vm);

	if (vm->vm_pgoff == (dev->registers.phys_addr >> PAGE_SHIFT)) {
		pr_info("[VPU-CODA-DRV]s_vpu_register.phys_addr %p, vm_pagoff %lx\n",
				(void *)(dev->registers.phys_addr), vm->vm_pgoff);
		return vpu_map_to_register(dev, vm);
	}

	/*
	 * Common memory onyl using by vpu
	 * map dma memory using dam_address
	 * dma_address same as phys_address when no smmu
	 */
	if (vm->vm_pgoff == (0xfe000 >> PAGE_SHIFT)) {
		pr_info("[VPU-CODA-DRV] vpucoda_device %p, s_common_memory.phys_addr %p, dma_addr %p ,base %p,	size %d, vm_pagoff %lx \n",
				(void *)dev->dev,
				(void *)dev->common_memory.phys_addr,
				(void *)dev->common_memory.dma_addr,
				(void *)dev->common_memory.base,
				dev->common_memory.size, vm->vm_pgoff);

		vm->vm_pgoff = 0;
		return dma_mmap_coherent(dev->dev, vm,
								 (void *)dev->common_memory.base, dev->common_memory.dma_addr,
								 vm->vm_end - vm->vm_start);
	}

	pr_info("[VPU-CODA-DRV] not using ion, vm_pgoff %lx, s_common_memory.phys_addr %p, s_common_memory.phys_addr >> PAGE_SHIFT %llx \n	",
			vm->vm_pgoff,
			(void *)dev->common_memory.phys_addr,
			dev->common_memory.phys_addr >> PAGE_SHIFT);

	return vpu_map_to_physical_memory(filp, vm);
#else

	if (vm->vm_pgoff) {
		if (vm->vm_pgoff == (dev->instance_pool.phys_addr >> PAGE_SHIFT))
			return vpu_map_to_instance_pool_memory(dev, vm);

		return vpu_map_to_physical_memory(filp, vm);
	}
	else {
		return vpu_map_to_register(dev, vm);
	}

#endif
}

static void vpu_get_sram_info(struct platform_device *pdev, struct sram_info  *info)
{
	struct resource *res = NULL;
	struct device_node *ram_node = NULL;
	struct resource res_temp = {0};
	uint32_t index = 0;

	if (pdev) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inter_sram");
		if (res) {
			info[0].phy = res->start;
			info[0].size = res->end - res->start + 1;
			info[0].id = 1; /* 1 for inter sram */
		}

		if (NULL != (ram_node = of_parse_phandle(pdev->dev.of_node, "vpu2,sram", 0))) {
			if (!of_property_read_u32(pdev->dev.of_node, "sram-index", &index)) {
				if (!of_address_to_resource(ram_node, index, &res_temp)) {
					info[1].phy = dma_map_page_attrs(&pdev->dev, phys_to_page(res_temp.start), 0,
									 res_temp.end - res_temp.start + 1,
									 DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
					if (dma_mapping_error(&pdev->dev, info[1].phy)) {
						info[1].phy = 0;
					} else {
						info[1].size = res_temp.end - res_temp.start + 1;
						info[1].id = 2; // 2 for soc sram
					}
				}
			}
		}
	}

	pr_info("[VPU-CODA-DRV] coda988 get sram info : sram[0].id %d;	sram[0].phy %#x; sram[0].size "
			"%#x ,sram[1].id %d;  sram[1].phy %#x; sram[1].size %#x \n",
			info[0].id, info[0].phy, info[0].size, info[1].id, info[1].phy, info[1].size);
}

static struct file_operations coda_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.read = vpu_read,
	.write = vpu_write,
	.unlocked_ioctl = vpu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpu_ioctl,
#endif
	.release = vpu_release,
	.mmap = vpu_mmap,
};

#ifdef CONFIG_OF
static const struct of_device_id vpucoda_of_table[] = {
	{.compatible = "semidrive,coda988",},
	{},
};
#endif

static int vpu_probe(struct platform_device *pdev)
{
	int err = 0, ret = 0;
	struct resource *res = NULL;
	struct coda_dev *dev = NULL;

	pr_info("[VPU-CODA-DRV] vpu_probe\n");


	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->dev = &pdev->dev;
	dma_set_coherent_mask(dev->dev, DMA_BIT_MASK(32));

	if (!device_property_read_u32(&(pdev->dev), "vpu-virtio-enable", &(dev->vpu_virtio_enable))) {
		pr_info("[VPU-CODA-DRV] vpu-virtio-enable value: %d\n", dev->vpu_virtio_enable);
	} else {
		pr_info("[VPU-CODA-DRV] read vpu-virtio-enable value failed, set defalut value 0\n");
		dev->vpu_virtio_enable = 0;
	}

	if (dev->vpu_virtio_enable) {
		mutex_init(&dev->virtio_mutex);
	} else {
		if (0 != vpu_hw_reset(&pdev->dev)) {
			pr_err("[VPU-CODA-DRV-ERR] vpu coda reset error\n");
			return -EFAULT;
		}
		spin_lock_init(&dev->irqlock);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "register");
		if (res) {
			dev->registers.phys_addr = res->start;
			dev->registers.virt_addr =
				(unsigned long)ioremap_nocache(res->start, res->end - res->start + 1);
			dev->registers.size = res->end - res->start + 1;
			pr_info("[VPU-CODA-DRV] : vpu base address get from platform driver physical base addr "
					"%p , kernel virtual base %p , vpucoda_device %p	register size %#x\n",
					(void *)dev->registers.phys_addr, (void *)dev->registers.virt_addr, &pdev->dev,
					dev->registers.size);
		} else {
			ret = -1;
			goto ERROR_PROBE_DEVICE;
		}

		if (!device_property_read_u32(&(pdev->dev), "sdrv,scr_signal", &(dev->scr_signal))) {
			pr_info("[VPU-CODA-DRV] coda988 scr signal value  %d\n", dev->scr_signal);
		}

		dev->clk_core = clk_get(&pdev->dev, "core-clk");
		if (IS_ERR(dev->clk_core)) {
			dev_err(&pdev->dev, "Could not get core clock\n");
			return PTR_ERR(dev->clk_core);
		}

		res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
		if (!res) {
			pr_info("[%s]: IORESOURCE_IRQ unavailable", __func__);
			ret = -ENODEV;
			goto ERROR_PROBE_DEVICE;
		}

		dev->irq = res->start;
		err = request_irq(dev->irq, vpu_irq_handler, 0, dev_name(&pdev->dev), (void *)dev);

		if (err) {
			pr_err("[VPU-CODA-DRV-ERR] :  fail to register interrupt handler\n");
			goto ERROR_PROBE_DEVICE;
		}

		vpu_get_sram_info(pdev, &(dev->sram[0]));

		sema_init(&dev->vpu_sema, 1);
		sema_init(&dev->disp_sema, 1);
		sema_init(&dev->gate_sema, 1);

		init_waitqueue_head(&dev->interrupt_wait_q);
	}

	if ((alloc_chrdev_region(&dev->device, 0, 1, VPU_DEV_NAME)) < 0) {
		err = -EBUSY;
		pr_err( "[VPU-CODA-DRV-ERR] could not allocate major number\n");
		goto ERROR_PROBE_DEVICE;
	}

	cdev_init(&dev->cdev_coda, &coda_fops);
	if ((cdev_add(&dev->cdev_coda, dev->device, 1)) < 0) {
		err = -EBUSY;
		pr_err( "[VPU-CODA-DRV-ERR] could not allocate chrdev\n");

		goto ERROR_PROBE_DEVICE;
	}

	if(NULL == (dev->class_coda = class_create(THIS_MODULE, "vpu_coda"))) {
		pr_err("[VPU-CODA-DRV-ERR] could not allocate class\n");
		err = -EBUSY;
		goto ERROR_PROBE_DEVICE;
	}

	if (NULL == (dev->device_coda = device_create(dev->class_coda, NULL, dev->device, NULL, VPU_DEV_NAME))) {
		err = -EBUSY;
		pr_err("ERR could not allocate device\n");
		goto ERROR_PROBE_DEVICE;
	}


	platform_set_drvdata(pdev, dev);

	mutex_init(&dev->coda_mutex);

	INIT_LIST_HEAD(&dev->vbp_list);
	INIT_LIST_HEAD(&dev->inst_list);


	pr_info("[VPU-CODA-DRV] success to probe vpu coda\n");
	return 0;

ERROR_PROBE_DEVICE:
	if (dev->device_coda && dev->class_coda) {
		device_destroy(dev->class_coda, dev->device);
	}

	if (dev->class_coda) {
		class_destroy(dev->class_coda);
	}

	if (dev->device > 0) {
		cdev_del(&dev->cdev_coda);
		unregister_chrdev_region(dev->device, 1);
	}

	if (dev->registers.virt_addr)
		iounmap((void *)dev->registers.virt_addr);

	devm_kfree(&pdev->dev, dev);
	return err;
}

static int vpu_remove(struct platform_device *pdev)
{
	struct coda_dev *dev = platform_get_drvdata(pdev);

	pr_info("[VPU-CODA-DRV] vpu_remove\n");

	if (dev->instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		vfree((const void *)dev->instance_pool.base);
#else
		vpu_free_dma_buffer(dev, dev->instance_pool);
#endif
		dev->instance_pool.base = 0;
	}

	if (dev->common_memory.base) {
		vpu_free_dma_buffer(dev, &dev->common_memory);
		dev->common_memory.base = 0;
	}

	if(dev->device_coda && dev->class_coda) {
		device_destroy(dev->class_coda, dev->device);
	}

	if(dev->class_coda) {
		class_destroy(dev->class_coda);
	}

	if (dev->device > 0) {
		cdev_del(&dev->cdev_coda);
		unregister_chrdev_region(dev->device, 1);
	}

	if (dev->irq) {
		free_irq(dev->irq, dev);
		dev->irq = 0;
	}

	if (dev->registers.virt_addr)
		iounmap((void *)dev->registers.virt_addr);

	if (dev->clk_core)
		clk_put(dev->clk_core);

	return 0;
}

#ifdef CONFIG_PM
static int vpu_suspend(struct platform_device *pdev,
					   pm_message_t state)
{
	int i;
	unsigned long timeout = jiffies + HZ;	/* vpu wait timeout to 1sec */
	int product_code;
	struct coda_dev *dev = platform_get_drvdata(pdev);
	pr_info("[VPU-CODA-DRV] vpu_suspend\n");

	if (dev->vpu_virtio_enable) {
		pr_info("[VPUDRV] vpu_virtio_enable return directly\n");
		return 0;
	}

	vpu_clk_enable(dev, true);

	if (dev->instance_ref_count > 0) {
		if (dev->bit_firmware_info.size == 0) {
			pr_err("[VPU-CODA-DRV] vpu have no init\n");
			goto DONE_SUSPEND;
		}
		product_code = READ_VPU_REGISTER(dev, VPU_PRODUCT_CODE_REGISTER);

		if (PRODUCT_CODE_CODA988(product_code)) {
			while (READ_VPU_REGISTER(dev, BIT_BUSY_FLAG)) {
				if (time_after(jiffies, timeout))
					goto DONE_SUSPEND;
			}

			for (i = 0; i < 64; i++)
				dev->vpu_reg_store[i] = READ_VPU_REGISTER(dev, BIT_BASE + (0x100 + (i * 4)));
		}
		else {
			pr_err("[VPU-CODA-DRV] Unknown product id : %08x\n", product_code);
			goto DONE_SUSPEND;
		}
	}

	vpu_clk_enable(dev, false);
	return 0;

DONE_SUSPEND:

	vpu_clk_enable(dev, false);

	return -EAGAIN;

}
static int vpu_resume(struct platform_device *pdev)
{
	int i;
	u32 val;
	unsigned long timeout = jiffies + HZ;	/* vpu wait timeout to 1sec */
	int product_code;
	struct coda_dev *dev = platform_get_drvdata(pdev);

	pr_info("[VPUDRV] coda vpu_resume\n");
	if (dev->vpu_virtio_enable) {
		pr_info("[VPUDRV] vpu_virtio_enable return directly\n");
		return 0;
	}

	vpu_clk_enable(dev, true);

	if (dev->bit_firmware_info.size == 0) {
		pr_err("[VPUDRV-ERR] vpu have no init \n");
		goto DONE_WAKEUP;
	}
	if (dev->instance_ref_count > 0) {
		product_code = READ_VPU_REGISTER(dev, VPU_PRODUCT_CODE_REGISTER);

		if (PRODUCT_CODE_CODA988(product_code)) {
			WRITE_VPU_REGISTER(dev, BIT_CODE_RUN, 0);
			/*---- LOAD BOOT CODE*/
			for (i = 0; i < 512; i++) {
				val = dev->bit_firmware_info.bit_code[i];
				WRITE_VPU_REGISTER(dev, BIT_CODE_DOWN, ((i << 16) | val));
			}

			for (i = 0 ; i < 64 ; i++)
				WRITE_VPU_REGISTER(dev, BIT_BASE + (0x100 + (i * 4)), dev->vpu_reg_store[i]);

			WRITE_VPU_REGISTER(dev, BIT_BUSY_FLAG, 1);
			WRITE_VPU_REGISTER(dev, BIT_CODE_RESET, 1);
			WRITE_VPU_REGISTER(dev, BIT_CODE_RUN, 1);

			while (READ_VPU_REGISTER(dev, BIT_BUSY_FLAG)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}

		}
		else {
			pr_err("[VPUDRV-ERR] Unknown product id : %08x\n", product_code);
			goto DONE_WAKEUP;
		}
	}

DONE_WAKEUP:
	if (dev->instance_ref_count == 0)
		vpu_clk_enable(dev, false);

	return 0;
}
#else
#define vpu_suspend NULL
#define vpu_resume NULL
#endif	/* !CONFIG_PM */


MODULE_DEVICE_TABLE(of, vpucoda_of_table);
static struct platform_driver vpu_driver = {
	.driver = {
		.name = VPU_PLATFORM_DEVICE_NAME,
		.of_match_table = vpucoda_of_table,
	.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = vpu_probe,
	.remove = vpu_remove,
	.suspend = vpu_suspend,
	.resume = vpu_resume,
};

static int __init vpu_init(void)
{
	int res;

	pr_info("[VPU-CODA-DRV] coda guy ...\n");
	res = platform_driver_register(&vpu_driver);

	return res;
}

static void __exit vpu_exit(void)
{
	pr_info("[VPU-CODA-DRV] vpu_exit\n");
	platform_driver_unregister(&vpu_driver);

	return;
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
MODULE_DESCRIPTION("semidrive vpu coda driver");


module_init(vpu_init);
module_exit(vpu_exit);

