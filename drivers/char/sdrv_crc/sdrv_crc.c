/* Copyright (c) 2020, Semidrive Semi.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/fs.h>
#include <linux/dma-buf.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#else
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/irq.h>
#endif
#include <drm/drm_fourcc.h>
#include <uapi/linux/sd-disp-crc-dcmd.h>
#include "sdrv_crc.h"


/*macro definition*/
#define to_disp_crc(x) container_of(x, struct disp_crc, mdev)
#define POLYNOMIAL 0x04c11db7L

/*global variable*/
static struct disp_crc *g_disp_crc;
static const struct attribute_group *sdrv_disp_crc_groups[];

/*static variable*/
static unsigned int crc_table[256];

/*static function*/

static ssize_t crc_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *mdev = dev_get_drvdata(dev);
	struct disp_crc *crc = to_disp_crc(mdev);
	struct dc_reg_status *dc_reg = &crc->dc_reg;
	ssize_t count = 0;

	count += sprintf(buf + count, "%s: dc_id[%d]:\n", crc->name, crc->dc_id);
	count += sprintf(buf + count, "  dc_base_reg[0x%lx], crc_reg[0x%lx]\n",
		(unsigned long)dc_reg->regs, (unsigned long)dc_reg->crc_reg);

	return count;
}

static DEVICE_ATTR_RO(crc_info);

struct attribute *sdrv_disp_crc_attrs[] = {
	&dev_attr_crc_info.attr,
	NULL,
};

ATTRIBUTE_GROUPS(sdrv_disp_crc);

static void copy_params(struct roi_params *dst, struct roi_params *src, uint32_t valid_roi_cnt)
{
	int i = 0;

	if ((dst == NULL) || (src == NULL)) {
		return;
	}

	for (i = 0; i < valid_roi_cnt; i++) {
		dst[i].win_num = src[i].win_num;
		dst[i].start_x = src[i].start_x;
		dst[i].end_x   = src[i].end_x;
		dst[i].start_y = src[i].start_y;
		dst[i].end_y   = src[i].end_y;
	}
}

static int sdrv_disp_crc_open(struct inode *node, struct file *file)
{
	int num = MINOR(node->i_rdev);
	struct disp_crc *crc = g_disp_crc;

	if (num < 0)
		return -ENODEV;

	file->private_data = crc;

	dev_info(crc->dev, "open node %s\n", crc->name);
	return 0;
}

ssize_t sdrv_disp_crc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct disp_crc *crc = file->private_data;
	char str[64] = {0};
	ssize_t sz = sprintf(str, "read from %s\n", crc->name);
	if (copy_to_user(buf, str, sz)){
		dev_err(crc->dev, "copy to user failed: %s\n", crc->name);
	}

	dev_info(crc->dev, "read node %s\n", crc->name);

	return sz;
}

static void __gen_crc_table(void)
{
	register int i, j;
	register unsigned int crc_accum;

	for (i = 0; i < 256; i++) {
		crc_accum = ((unsigned int) i << 24);
		for (j = 0; j < 8; j++) {
			if (crc_accum & 0x80000000L)
				crc_accum = (crc_accum << 1) ^ POLYNOMIAL;
			else
				crc_accum = (crc_accum << 1);
		}
		crc_table[i] = crc_accum;
	}
}

static unsigned int __update_crc(unsigned int crc_accum, void *data_blk_ptr, int data_blk_size)
{
	unsigned char *p;
	register int i, j;

	p = (unsigned char*)data_blk_ptr;

	for (j = 0; j < data_blk_size; j++) {
		i = ((int) (crc_accum >> 24) ^ *p) & 0xff;
		crc_accum = (crc_accum << 8) ^ crc_table[i];
		p++;
	}
	return crc_accum;
}

static unsigned int __sdrv_expect_crc32(int format, bool is_internal, unsigned char *buf_in, struct crc32block *crc32block)
{
	unsigned int crc = 0;
	unsigned int R = 0;
	unsigned int G = 0;
	unsigned int B = 0;
	unsigned int indata;
	unsigned int *pindata;
	unsigned int indata_b0;
	unsigned int indata_b1;
	unsigned int indata_b2;
	unsigned int indata_b3;
	unsigned int indata_MSB;
	unsigned int index;
	unsigned int i, j;

	__gen_crc_table();

	for (i = crc32block->cfg_start_y; i <= crc32block->cfg_end_y; i++) {

		for (j = crc32block->cfg_start_x; j <= crc32block->cfg_end_x; j++) {
		switch(format) {
		   case DRM_FORMAT_RGB888:
				index = i*crc32block->stride + j*3;
				B = buf_in[index];
				G = buf_in[index + 1];
				R = buf_in[index + 2];
			break;

		   case DRM_FORMAT_ARGB8888:
				index = i*crc32block->stride + j*4;
				R = buf_in[index + 1];
				G = buf_in[index + 2];
				B = buf_in[index + 3];
			break;
		}

			if (is_internal) {//internal
				B = (B << 2) | (B >> 6);
				G = (G << 2) | (G >> 6);
				R = (R << 2) | (R >> 6);
			} else {//external
				B = (B << 2);
				G = (G << 2);
				R = (R << 2);
			}

			indata = 0x00000000 | (R<<22) | (G<<12) | (B<<2);
			indata_b0 = (indata & 0x000000FF)<<24;
			indata_b1 = (indata & 0x0000FF00)<<8;
			indata_b2 = (indata & 0x00FF0000)>>8;
			indata_b3 = (indata & 0xFF000000)>>24;
			indata_MSB = indata_b0 | indata_b1 | indata_b2 | indata_b3;
			pindata = &indata_MSB;

			crc = __update_crc(crc, pindata, 4);
		}
	}

	return crc;
}

static unsigned long _get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected = sg_dma_address(s) + sg_dma_len(s);
		size += sg_dma_len(s);
	}

	return size;
}

static int disp_crc_dmabuf_import(struct disp_crc *crc, struct scrc_input *buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct dma_buf *dmabuf;
	int ret = 0;

	dmabuf = dma_buf_get(buf->fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(crc->dev, "disp crc get dmabuf from input buf fd %d\n", buf->fd);
		return PTR_ERR(dmabuf);
	}

	attach = dma_buf_attach(dmabuf, &crc->pdev->dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		dev_err(crc->dev, "dma buf attach devices faild\n");
		goto out_put;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		dev_err(crc->dev, "Error getting dmabuf scatterlist: errno %ld\n", PTR_ERR(sgt));
		goto fail_detach;
	}

	buf->attach = attach;
	buf->size = _get_contiguous_size(sgt);
	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->sgt = sgt;
	buf->vaddr = (uint64_t)dma_buf_vmap(dmabuf);
	dev_dbg(crc->dev, "sdrv crc map scrc buf size = %ld\n", buf->size);
	if (!buf->size) {
		dev_err(crc->dev, "dma buf map attachment faild, buf->size = %ld \n", buf->size);
		ret = -EINVAL;
		goto fail_unmap;
	}

	goto out_put;

fail_unmap:
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
fail_detach:
	dma_buf_detach(dmabuf, attach);
out_put:
	dma_buf_put(dmabuf);
	return ret;
}

static void disp_crc_dmabuf_release(struct disp_crc *crc, struct scrc_input *buf)
{
	struct sg_table *sgt = buf->sgt;
	struct dma_buf *dmabuf;

	if (IS_ERR_OR_NULL(sgt)) {
		dev_err(crc->dev, "dmabuf buffer is already unpinned \n");
		return;
	}

	if (IS_ERR_OR_NULL(buf->attach)) {
		dev_err(crc->dev, "trying to unpin a not attached buffer\n");
		return;
	}

	dmabuf = dma_buf_get(buf->fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(crc->dev, "invalid dmabuf from dma_buf_get: %d", buf->fd);
		return;
	}
	dev_dbg(crc->dev, "buf->vaddr = 0x%ld\n", (unsigned long)buf->vaddr);
	if (buf->vaddr) {
		dma_buf_vunmap(dmabuf, (void *)buf->vaddr);
		buf->vaddr = (unsigned long)NULL;
	}
	dma_buf_unmap_attachment(buf->attach, sgt, 0);

	buf->dma_addr = 0;
	buf->sgt = NULL;

	dma_buf_detach(dmabuf, buf->attach);

	dma_buf_put(dmabuf);

}

static int sdrv_disp_crc_fb_mmap(struct disp_crc *crc, struct scrc_input *input)
{
	int ret = 0;

	ret = disp_crc_dmabuf_import(crc, input);
	if (ret) {
		dev_err(crc->dev, "disp crc dmabuf import failed\n");
		return -1;
	}

	dev_dbg(crc->dev, "input framebuffer vaddr = 0x%lx\n", input->vaddr);

	return ret;

}

static int sdrv_disp_crc_get_scrc(struct disp_crc *crc, struct disp_crc_get_scrc_ipc *scrc)
{
	int ret = 0;
	struct scrc_input *input = &scrc->input;
	struct saved_roi_parms *saved_roi = &crc->saved_roi;
	struct crc32block tmp_crc_block;
	int i;

	if (input->fd <= 0) {
		dev_err(crc->dev, "%s : scrc input fd %d is invalid\n", __func__, input->fd);
		return -1;
	}

	if (input->stride <= 0) {
		dev_err(crc->dev, "scrc input stride %d is invalid\n", input->stride);
		return -1;
	}

	if ((input->format != DRM_FORMAT_RGB888) && (input->format != DRM_FORMAT_ARGB8888)) {
		dev_err(crc->dev, "scrc input format %c%c%c%c unsupport\n",
			input->format & 0xff, (input->format >> 8) & 0xff, (input->format >> 16) & 0xff,
			(input->format >> 24) & 0xff);
		dev_err(crc->dev, "disp crc format only support DRM_FORMAT_RGB888 or DRM_FORMAT_ARGB8888\n");
		return -1;
	}

	if (saved_roi->saved_valid_roi_cnt <= 0) {
		dev_err(crc->dev, "saved_roi->saved_valid_roi_cnt = %d, please set roi first\n",
			saved_roi->saved_valid_roi_cnt);
		return -1;
	}

	ret = sdrv_disp_crc_fb_mmap(crc, input);
	if (ret) {
		dev_err(crc->dev, "sdrv disp crc fb mmap failed\n");
		return -1;
	}

	dev_dbg(crc->dev, "get scrc saved valid roi cnt[%d]\n", saved_roi->saved_valid_roi_cnt);
	tmp_crc_block.stride = input->stride;
	for (i = 0; i < saved_roi->saved_valid_roi_cnt; i++) {
		tmp_crc_block.cfg_start_x = saved_roi->saved_roi_params[i].start_x;
		tmp_crc_block.cfg_start_y = saved_roi->saved_roi_params[i].start_y;
		tmp_crc_block.cfg_end_x   = saved_roi->saved_roi_params[i].end_x;
		tmp_crc_block.cfg_end_y   = saved_roi->saved_roi_params[i].end_y;
		dev_dbg(crc->dev, "scrc roi[%d]: rect[%d, %d, %d, %d]\n", i, tmp_crc_block.cfg_start_x, tmp_crc_block.cfg_start_y,
			tmp_crc_block.cfg_end_x, tmp_crc_block.cfg_end_y);
		scrc->result[i] = __sdrv_expect_crc32(input->format, true , (unsigned char *)input->vaddr, &tmp_crc_block);

		dev_dbg(crc->dev, "scrc->result[%d] = 0x%x\n", i, scrc->result[i]);
	}

	disp_crc_dmabuf_release(crc, input);

	return ret;
}

static long sdrv_disp_crc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1, i;
	struct disp_crc *crc = file->private_data;
	struct saved_roi_parms *saved_roi = &crc->saved_roi;
	struct disp_crc_set_roi_ipc roi;
	struct disp_crc_get_hcrc_ipc hcrc;
	struct disp_crc_get_scrc_ipc scrc;
	int start_crc;

	if (_IOC_TYPE(cmd) != DISP_CRC_IOCTL_BASE)
		return -EINVAL;

	if (_IOC_NR(cmd) > 4)
		return -EINVAL;

	if (_IOC_DIR(cmd) & _IOC_READ) {
		ret = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
		if (ret)
			return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
		if (ret)
			return -EFAULT;
	}

	if (!crc) {
		pr_err("%s: dev isn't inited. crc is NULL\n", __func__);
		return -EFAULT;
	}

	switch (cmd) {
		case DCMD_DISP_CRC_SET_ROI:
			ret = copy_from_user(&roi, (struct disp_crc_set_roi_ipc __user *)arg,
				sizeof(struct disp_crc_set_roi_ipc));
			if (ret) {
				dev_err(crc->dev, "DCMD_DISP_CRC_SET_ROI copy from user failed\n");
				ret = -EFAULT;
				break;
			}

			if((roi.valid_roi_cnt > 8) || (roi.valid_roi_cnt == 0)) {
				dev_err(crc->dev, "error: the range of valid_roi_cnt is 1-8\n");
				return EINVAL;
			}
			mutex_lock(&crc->m_lock);
			copy_params(saved_roi->saved_roi_params, roi.roi_parms, roi.valid_roi_cnt);
			saved_roi->saved_valid_roi_cnt = roi.valid_roi_cnt;
			dev_dbg(crc->dev, "crc set roi saved valid roi cnt[%d]\n", saved_roi->saved_valid_roi_cnt);

			if (crc->ops->set_roi == NULL) {
				dev_err(crc->dev, "crc ops set_roi is NULL\n");
				ret = -EFAULT;
				goto unlock_out;
			}
			ret = crc->ops->set_roi(crc, saved_roi->saved_roi_params, saved_roi->saved_valid_roi_cnt);
			if (ret) {
				dev_err(crc->dev, "set roi failed\n");\
				ret = -EFAULT;
				goto unlock_out;
			}
			mutex_unlock(&crc->m_lock);
			break;
		case DCMD_DISP_CRC_GET_HCRC:
			if (!crc->start_crc) {
				dev_warn(crc->dev, "crc dont set start, please set start first\n");
				break;
			}

			if(crc->saved_roi.saved_valid_roi_cnt <= 0) {
				dev_warn(crc->dev, "crc saved_roi.saved_valid_roi_cnt <= 0, please set roi first\n");
				break;
			}

			crc->ops->clear_done(crc);

			while (crc->ops->wait_done(crc, crc->saved_roi.saved_valid_roi_cnt) == -1) {
				msleep(1);
			}

			memset(&hcrc, 0, sizeof(struct disp_crc_get_hcrc_ipc));
			mutex_lock(&crc->m_lock);
			for(i = 0; i < crc->saved_roi.saved_valid_roi_cnt; i++) {
				ret = crc->ops->get_hcrc(crc, i, &hcrc.hcrc.status[i], &hcrc.hcrc.result[i]);
				if (ret) {
					dev_err(crc->dev, "get hcrc failed\n");
					ret = -EFAULT;
					goto unlock_out;
				}
			}
			mutex_unlock(&crc->m_lock);
			ret = copy_to_user((struct disp_crc_get_hcrc_ipc __user *)arg, &hcrc,
				sizeof(struct disp_crc_get_hcrc_ipc));
			if (ret) {
				dev_err(crc->dev, "DCMD_DISP_CRC_GET_HCRC copy to user failed\n");
				ret = -EFAULT;
				break;
			}
			break;
		case DCMD_DISP_CRC_START_CALC:
			ret = copy_from_user(&start_crc, (int *)arg, sizeof(int));
			dev_dbg(crc->dev, "crc start_crc = %d\n", start_crc);

			mutex_lock(&crc->m_lock);
			crc->start_crc = start_crc;

			if (crc->ops->start_calc == NULL) {
				dev_err(crc->dev, "crc ops start_calc is NULL\n");
				ret = -EFAULT;
				goto unlock_out;
			}
			ret = crc->ops->start_calc(crc);
			mutex_unlock(&crc->m_lock);
			if (ret) {
				dev_err(crc->dev, "DCMD_DISP_CRC_START_CALC start calc failed\n");
				ret = -EFAULT;
				break;
			}
			break;
		case DCMD_DISP_CRC_GET_SCRC:
			if (!crc->start_crc) {
				dev_warn(crc->dev, "crc dont set start, please set start first\n");
				break;
			}

			if(crc->saved_roi.saved_valid_roi_cnt <= 0) {
				dev_warn(crc->dev, "crc saved_roi.saved_valid_roi_cnt <= 0, please set roi first\n");
				break;
			}

			ret = copy_from_user(&scrc, (struct disp_crc_get_scrc_ipc __user *)arg, sizeof(struct disp_crc_get_scrc_ipc));
			if (ret) {
				dev_err(crc->dev, "DCMD_DISP_CRC_GET_SCRC copy from user failed\n");
				ret = -EFAULT;
				break;
			}
			mutex_lock(&crc->m_lock);
			ret = sdrv_disp_crc_get_scrc(crc, &scrc);
			mutex_unlock(&crc->m_lock);
			if (ret) {
				dev_err(crc->dev, "sdrv disp crc get scrc failed\n");
				ret = -EFAULT;
				break;
			}

			ret = copy_to_user((struct disp_crc_get_scrc_ipc __user *)arg, &scrc, sizeof(struct disp_crc_get_scrc_ipc));
			if (ret) {
				dev_err(crc->dev, "DCMD_DISP_CRC_GET_SCRC copy to user failed\n");
				ret = -EFAULT;
				break;
			}

			break;
	}

	return (long)ret;

unlock_out:
	mutex_unlock(&crc->m_lock);
	return (long)ret;
}

#if defined(CONFIG_COMPAT)
static long sdrv_disp_crc_compat_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	return sdrv_disp_crc_ioctl(file, cmd, arg);
}
#endif /* defined(CONFIG_COMPAT) */

static const struct file_operations disp_crc_file_ops = {
	.owner = THIS_MODULE,
	.open = sdrv_disp_crc_open,
	.read = sdrv_disp_crc_read,
	.unlocked_ioctl = sdrv_disp_crc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sdrv_disp_crc_compat_ioctl,
#endif
};

static int sdrv_disp_crc_misc_init(struct disp_crc *crc)
{
	int ret = 0;
	struct miscdevice *m = &crc->mdev;

	m->minor = MISC_DYNAMIC_MINOR;
	m->name = kasprintf(GFP_KERNEL, "%s", crc->name);
	m->fops = &disp_crc_file_ops;
	m->parent = NULL;
	m->groups = sdrv_disp_crc_groups;

	ret = misc_register(m);
	if (ret) {
		dev_err(crc->dev, "failed to register miscdev\n");
		return ret;
	}

	dev_info(crc->dev, "%s misc register\n", m->name);

	return ret;
}


static int sdrv_disp_crc_parse_dt(struct disp_crc *crc,
				      const struct device_node *np)
{
	struct dc_reg_status *dc_reg = &crc->dc_reg;
	struct device_node *dc_np;
	struct resource res;
	uint32_t temp;

	if (of_property_read_u32(np, "crc-dc_id", &temp)) {
		dev_err(crc->dev, "fail to find \"crc-dc_id\"\n");
		return -ENODEV;
	}
	crc->dc_id = temp;

	dc_np = of_parse_phandle(np, "crc-dc", 0);
	if (!dc_np) {
		dev_err(crc->dev, "fail to find \"crc-dc\"\n");
		return -ENODEV;
	}
	/*
	if(!of_device_is_available(dc_np)) {
		dev_err(crc->dev, "OF node %s not available or match\n", dc_np->name);
		return -ENODEV;
	}
	*/
	dev_info(crc->dev, "Add dc component %s\n", dc_np->name);

	if (of_address_to_resource(dc_np, 0, &res)) {
		dev_err(crc->dev, "parse dt base address failed\n");
		return -ENODEV;
	}
	dev_info(crc->dev, "got crc-dc_id %d res 0x%lx\n", crc->dc_id, (unsigned long)res.start);

	dc_reg->regs = devm_ioremap_nocache(crc->dev, res.start, resource_size(&res));
	if(IS_ERR(dc_reg->regs)) {
		dev_err(crc->dev, "Cannot find crc dc regs\n");
		return PTR_ERR(dc_reg->regs);
	}
	dc_reg->crc_reg = (struct crc32_reg *)(dc_reg->regs + CRC32_REG_OFFSET);

	return 0;
}

static int sdrv_disp_crc_probe(struct platform_device *pdev)
{
	struct disp_crc *crc = NULL;
	int ret;

	crc = devm_kzalloc(&pdev->dev, sizeof(struct disp_crc), GFP_KERNEL);
	if (!crc) {
		pr_err("%s : kzalloc disp crc failed\n", __func__);
		return -ENOMEM;
	}

	crc->pdev = pdev;
	crc->dev = &pdev->dev;

	dev_info(crc->dev, "DISP CRC BUILD VERSION: %s\n", CRC_VERSION);

	ret = sdrv_disp_crc_parse_dt(crc, crc->dev->of_node);
	if (ret < 0)
		return ret;

	crc->name = "disp-crc";
	if (!crc->name) {
		dev_err(crc->dev, "crc name is null\n");
		return -ENODEV;
	}

	crc->ops = &disp_crc_ops;
	if (crc->ops == NULL) {
		dev_err(crc->dev, "core ops attach failed\n");
		return -ENOMEM;
	}

	mutex_init(&crc->m_lock);

	ret = sdrv_disp_crc_misc_init(crc);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, crc);
	g_disp_crc = crc;
	crc->du_inited = true;
	crc->start_crc = false;

	return ret;
}

static int sdrv_disp_crc_remove(struct platform_device *pdev)
{
	struct disp_crc *crc = platform_get_drvdata(pdev);
	dev_info(crc->dev, "remove %s\n", crc->name);

	if(crc) {
		misc_deregister(&crc->mdev);
	}

	return 0;
}

static const struct of_device_id sdrv_disp_crc_of_match[] = {
	{ .compatible = "sd,sdrv-disp-crc" },
	{ }
};
MODULE_DEVICE_TABLE(of, sdrv_disp_crc_of_match);

static struct platform_driver sdrv_disp_crc_driver = {
	.driver = {
		.name = "sdrv-disp-crc",
		.of_match_table = of_match_ptr(sdrv_disp_crc_of_match),
	},
	.probe		= sdrv_disp_crc_probe,
	.remove		= sdrv_disp_crc_remove,
};

module_platform_driver(sdrv_disp_crc_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive display crc driver");
MODULE_LICENSE("GPL");
