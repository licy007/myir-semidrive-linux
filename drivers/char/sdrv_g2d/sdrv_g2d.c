/*
* SEMIDRIVE Copyright Statement
* Copyright (c) SEMIDRIVE. All rights reserved
* This software and all rights therein are owned by SEMIDRIVE,
* and are protected by copyright law and other relevant laws, regulations and protection.
* Without SEMIDRIVE’s prior written consent and /or related rights,
* please do not use this software or any potion thereof in any form or by any means.
* You may not reproduce, modify or distribute this software except in compliance with the License.
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* You should have received a copy of the License along with this program.
* If not, see <http://www.semidrive.com/licenses/>.
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
#include <asm/current.h>
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
#include "sdrv_g2d.h"
#include "g2d_common.h"

static DEFINE_MUTEX(m_init);
extern const struct g2d_ops g2d_normal_ops;
extern const struct g2d_ops g2d_lite_ops;
extern struct ops_entry spipe_g2d_entry;
extern struct ops_entry gpipe_mid_g2d_entry;
extern struct ops_entry gpipe_high_g2d_entry;
extern int g2d_dump_registers(struct sdrv_g2d *dev);
extern int g2d_post_config(struct sdrv_g2d *dev, struct g2d_input *ins);
extern int g2d_fastcopy_set(struct sdrv_g2d *dev, addr_t iaddr, u32 width,
	u32 height, u32 istride, addr_t oaddr, u32 ostride);
extern int g2d_fill_rect(struct sdrv_g2d *dev, struct g2d_bg_cfg *bgcfg, struct g2d_output_cfg *output);
extern int g2d_set_coefficients_table(struct sdrv_g2d *gd, struct g2d_coeff_table *table);
extern struct attribute *sdrv_g2d_attrs[];
static const struct attribute_group *sdrv_g2d_groups[];
ATTRIBUTE_GROUPS(sdrv_g2d);

static int wait_timeout = 500;
module_param(wait_timeout, int, 0660);
MODULE_PARM_DESC(wait_timeout, "wait timeout (ms)");

static int dump_register_g2d = 0;
module_param(dump_register_g2d, int, 0660);
MODULE_PARM_DESC(dump_register_g2d, "dump register g2d 0:off 1:on");

int debug_g2d = 0;
EXPORT_SYMBOL(debug_g2d);
module_param(debug_g2d, int, 0660);
MODULE_PARM_DESC(debug_g2d, "debug g2d 0:off 1:on");

static char *version = KO_VERSION;
module_param(version, charp, S_IRUGO);

LIST_HEAD(g2d_pipe_list_head);

int g2d_major = 227;
int g2d_minor = -1;
static struct sdrv_g2d *g_g2d[G2D_NR_DEVS];
const char *PIPE_TYPE_STRING[] = {
	GP_ECHO_NAME,
	GP_MID_NAME,
	GP_HIGH_NAME,
	SPIPE_NAME
};

struct sdrv_g2d_data g2d_data[] = {
	{.version = "g2dlite-r0p0", .ops = &g2d_lite_ops},
	{.version = "g2d-r0p1", .ops = &g2d_normal_ops},
	{},
};

static void dump_input(struct g2d_input *input)
{
	struct g2d_output_cfg *output = &input->output;
	struct g2d_layer *layer;
	struct g2d_bg_cfg *bg = &input->bg_layer;
	int i = 0;

	if (bg->en) {
		G2D_ERROR("[dump bg layer] en:%d, color:0x%x, g_alpha:0x%x, zorder:%d, bpa:0x%x, \
			astride:%d, rect(%d, %d, %d, %d), pd_type:%d, fd:%d \n",
			bg->en, bg->color, bg->g_alpha, bg->zorder, bg->bpa, bg->astride,
			bg->x, bg->y, bg->width, bg->height, bg->pd_type, bg->abufs.fd);
	}

	for (i = 0; i < input->layer_num; i++) {
		layer = &input->layer[i];
		G2D_ERROR("[dumplayer] index = %d, *ENABLE = %d*, format: %c%c%c%c source (%d, %d, %d, %d) => dest (%d, %d, %d, %d)\n",
				layer->index, layer->enable,
				layer->format & 0xff, (layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff, (layer->format >> 24) & 0xff,
				layer->src_x, layer->src_y, layer->src_w, layer->src_h,
				layer->dst_x, layer->dst_y, layer->dst_w, layer->dst_h);
	}

	G2D_ERROR("[dump output]: w,h(%d,%d) format:%c%c%c%c rota:%d nplanes:%d\n",
		output->width, output->height,
		output->fmt & 0xff, (output->fmt >> 8) & 0xff, (output->fmt >> 16) & 0xff, (output->fmt >> 24) & 0xff,
		output->rotation, output->nplanes);

	return;
}

struct sdrv_g2d *get_g2d_by_id(int id)
{
	return g_g2d[id];
}

int g2d_ops_register(struct ops_entry *entry, struct list_head *head)
{
	struct ops_list *list;

	list = kzalloc(sizeof(struct ops_list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	list->entry = entry;
	list_add(&list->head, head);

	return 0;
}

void *g2d_ops_attach(const char *str, struct list_head *head)
{
	struct ops_list *list;
	const char *ver;

	list_for_each_entry(list, head, head) {
		ver = list->entry->ver;

		if (!strcmp(str, ver))
			return list->entry->ops;
	}

	G2D_ERROR("attach disp ops %s failed\n", str);
	return NULL;
}

irqreturn_t sdrv_g2d_irq_handler(int irq, void *data)
{
	struct sdrv_g2d *gd = data;
	uint32_t val;

	if (!gd->du_inited) {
		G2D_ERROR("g2d du_inited does not init\n");
		return IRQ_HANDLED;
	}

	val = gd->ops->irq_handler(gd);
	if (val & G2D_INT_MASK_FRM_DONE) {
		G2D_DBG("frame done\n");
		gd->frame_done = true;
		wake_up(&gd->wq);
	}

	return IRQ_HANDLED;
}

int g2d_choose_pipe(struct sdrv_g2d *gd, int hwid, int type, uint32_t offset)
{
	struct g2d_pipe *p = NULL;

	p = devm_kzalloc(&gd->pdev->dev, sizeof(struct g2d_pipe), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	p->type = type;
	p->name = PIPE_TYPE_STRING[type];
	p->ops = (struct pipe_operation*)g2d_pipe_ops_attach(p->name);
	if (!p->ops) {
		G2D_ERROR("error ops attached\n");
		return -EINVAL;
	}

	p->regs = gd->regs + (ulong)offset;
	p->iomem_regs = gd->iomem_regs + (ulong)offset;
	p->reg_offset = offset;

	p->id = hwid;
	p->gd = gd;
	gd->pipes[gd->num_pipe] = p;
	gd->num_pipe++;

	p->ops->init(p);
	G2D_DBG("pipe %d name %s registered\n", p->id, p->name);
	return 0;
}

#ifdef CONFIG_OF
int sdrv_g2d_init(struct sdrv_g2d *gd, struct device_node *np) {
	int i, ret;
	int irq_num;
	struct resource res;
	const char *str;
	const struct sdrv_g2d_data *data;
	static int g2d_cnt = 0;

	if (!np || !gd)
		return -ENODEV;
	if(!of_device_is_available(np)) {
		G2D_ERROR("OF node %s not available or match\n", np->name);
		return -ENODEV;
	}

	if (!of_property_read_string(np, "sdrv,ip", &str)) {
		gd->name = str;
	} else {
		G2D_ERROR("sdrv,ip can not found\n");
		return -ENODEV;
	}

	if (of_address_to_resource(np, 0, &res)) {
		G2D_ERROR("parse dt base address failed\n");
		return -ENODEV;
	}

	G2D_INFO("got %s res 0x%lx\n", gd->name, (unsigned long)res.start);
	gd->regs = (void *)res.start;
	gd->iomem_regs = devm_ioremap_nocache(&gd->pdev->dev, res.start, resource_size(&res));
	if(IS_ERR(gd->iomem_regs)) {
		G2D_ERROR("Cannot find g2d regs 001\n");
		return PTR_ERR(gd->regs);
	}

	irq_num = irq_of_parse_and_map(np, 0);
	if (!irq_num) {
		G2D_ERROR("error: g2d parse irq num failed\n");
		return -EINVAL;
	}
	G2D_INFO("g2d irq_num = %d\n", irq_num);
	data = of_device_get_match_data(&gd->pdev->dev);
	for (i = 0; i < 3; i++) {
		if (!strcmp(gd->name, data[i].version)) {
			gd->ops = data[i].ops;
			G2D_INFO("%s ops[%d] attached\n", gd->name, i);
			break;
		}
	}

	if (gd->ops == NULL) {
		G2D_ERROR("core ops attach failed, have checked %d times\n", i);
		return -1;
	}
	gd->num_pipe = 0;
	// g2d init
	gd->ops->init(gd);
	gd->irq = irq_num;
	gd->cap.num_pipe = gd->num_pipe;
	for (i = 0; i < gd->num_pipe; i++) {
		memcpy(&gd->cap.pipe_caps[i], gd->pipes[i]->cap, sizeof(struct g2d_pipe_capability));
	}

	gd->id = g2d_cnt;
	irq_set_status_flags(gd->irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&gd->pdev->dev, gd->irq, sdrv_g2d_irq_handler,
			0, dev_name(&gd->pdev->dev), gd); //IRQF_SHARED
	if(ret) {
		G2D_ERROR("Failed to request DC IRQ: %d\n", gd->irq);
		return -ENODEV;
	}

	//wait queue
	init_waitqueue_head(&gd->wq);
	gd->frame_done = false;
	g2d_cnt++;
	return 0;
}

#else
int sdrv_g2d_init(struct sdrv_g2d *gd, struct platform_device *pdev) {
	int i, ret;
	int irq_num;
	struct device *dev = &pdev->dev;
	struct resource *res;
	const char *str;
	const struct sdrv_g2d_data *data;
	struct g2d_platform_data *pdata;
	static int g2d_cnt = 0;

	if (!pdev || !gd)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gd->regs = (void *)res->start;
	gd->iomem_regs = devm_ioremap_nocache(&gd->pdev->dev, res->start, resource_size(res));
	if(IS_ERR(gd->iomem_regs)) {
		G2D_ERROR("Cannot find g2d regs 001\n");
		return PTR_ERR(gd->regs);
	}

	pdata = (struct g2d_platform_data *)platform_get_drvdata(pdev);
	gd->name = "g2d-r0p1";
	if (!gd->name) {
		G2D_ERROR("sdrv,ip can not found\n");
		return -ENODEV;
	}
	G2D_INFO("got %s res 0x%lx\n", gd->name, (unsigned long)gd->regs);
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	irq_num = (int) res->start;
	if (!irq_num) {
		G2D_ERROR("error: g2d parse irq num failed\n");
		return -EINVAL;
	}
	G2D_INFO("g2d irq_num = %d\n", irq_num);
	data = g2d_data;
	for (i = 0; i < 16; i++) {
		if (!strcmp(gd->name, data[i].version)) {
			gd->ops = data[i].ops;
			G2D_DBG("%s ops[%d] attached\n", gd->name, i);
			break;
		}
	}

	if (gd->ops == NULL) {
		G2D_ERROR("core ops attach failed, have checked %d times\n", i);
		return -1;
	}
	gd->num_pipe = 0;
	// g2d init
	gd->ops->init(gd);
	gd->irq = irq_num;
	gd->cap.num_pipe = gd->num_pipe;
	for (i = 0; i < gd->num_pipe; i++) {
		memcpy(&gd->cap.pipe_caps[i], gd->pipes[i]->cap, sizeof(struct g2d_pipe_capability));
	}

	gd->id = g2d_cnt;
	irq_set_status_flags(gd->irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&gd->pdev->dev, gd->irq, sdrv_g2d_irq_handler,
			0, dev_name(&gd->pdev->dev), gd); //IRQF_SHARED
	if(ret) {
		G2D_ERROR("Failed to request DC IRQ: %d\n", gd->irq);
		return -ENODEV;
	}
	//wait queue
	init_waitqueue_head(&gd->wq);
	gd->frame_done = false;
	g2d_cnt++;
	return 0;
}
#endif

static void sdrv_g2d_unit(struct sdrv_g2d *gd)
{
	if (!gd)
		return;

	// if (gd->ops->uninit)
	//	gd->ops->uninit(gd);
}

static int sdrv_g2d_open(struct inode *node, struct file *file)
{
	int i;
	struct sdrv_g2d *gd = NULL;
	int num = MINOR(node->i_rdev);

	if (num < 0)
		return -ENODEV;
	for (i = 0; i < G2D_NR_DEVS; i++){
		gd = get_g2d_by_id(i);
		if (gd->mdev.minor == num)
			break;
	}

	file->private_data = gd;
	G2D_DBG("open node %s\n", gd->name);
	return 0;
}

static int sdrv_init_iommu(struct sdrv_g2d *gd)
{
	struct device *dev = &gd->pdev->dev;
	struct device_node *iommu = NULL;
	struct property *prop = NULL;
	struct iommu_domain_geometry *geometry;
	u64 start, end;
	int ret = 0;

	gd->iommu_enable = false;
	iommu = of_parse_phandle(dev->of_node, "iommus", 0);
	if(!iommu) {
		G2D_DBG("iommu not specified\n");
		return ret;
	}
	if (!of_device_is_available(iommu)) {
		G2D_DBG("smmu disabled\n");
		return ret;
	}
	prop = of_find_property(dev->of_node, "smmu", NULL);
	if(!prop) {
		G2D_DBG("smmu bypassed\n");
		return ret;
	}

	gd->domain = iommu_get_domain_for_dev(dev);
	if(!gd->domain) {
		ret = -ENOMEM;
		goto err_free_mm;;
	}

	geometry = &gd->domain->geometry;
	start = geometry->aperture_start;
	end = GENMASK(37, 0);// 38 bits address for KUNLUN G2D rdma

	G2D_DBG("IOMMU context initialized: %#llx - %#llx\n",
			start, end);

	gd->iommu_enable = true;

	of_node_put(iommu);
	return ret;

err_free_mm:
	of_node_put(iommu);
	return ret;
}

static void sdrv_iommu_cleanup(struct sdrv_g2d *gd)
{
	if(!gd->iommu_enable)
		return;

	iommu_domain_free(gd->domain);
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

static int g2d_dmabuf_import(struct sdrv_g2d *gd, struct g2d_buf *buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct dma_buf *dmabuf;
	int ret = 0;

	if (buf->fd < 0) {
		G2D_ERROR("dmabuf handle invalid: %d\n", buf->fd);
		return -EINVAL;
	}

	buf->vaddr = (unsigned long)NULL;

	dmabuf = dma_buf_get(buf->fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		G2D_ERROR("g2d get dmabuf err from buf fd %d\n", buf->fd);
		return PTR_ERR(dmabuf);
	}

	attach = dma_buf_attach(dmabuf, &gd->pdev->dev);
	if (IS_ERR(attach)) {
		G2D_ERROR("dma buf attach devices faild\n");
		goto out_put;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		G2D_ERROR("Error getting dmabuf scatterlist: errno %ld\n", PTR_ERR(sgt));
		goto fail_detach;
	}

	buf->attach = attach;
	buf->size = _get_contiguous_size(sgt);
	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->sgt = sgt;
	buf->vaddr = (unsigned long)NULL;
	G2D_DBG("buf->size = 0x%llx \n", buf->size);
	if (!buf->size) {
		G2D_ERROR("dma buf map attachment faild, buf->size = %lld \n", buf->size);
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

static void g2d_dmabuf_release(struct sdrv_g2d *gd, struct g2d_buf *buf)
{
	struct sg_table *sgt = buf->sgt;
	struct dma_buf *dmabuf;

	if (IS_ERR_OR_NULL(sgt)) {
		G2D_ERROR("dmabuf buffer is already unpinned \n");
		return;
	}

	if (IS_ERR_OR_NULL(buf->attach)) {
		G2D_ERROR("trying to unpin a not attached buffer\n");
		return;
	}

	dmabuf = dma_buf_get(buf->fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		G2D_ERROR("invalid dmabuf from dma_buf_get: %d", buf->fd);
		return;
	}
	G2D_DBG("buf->vaddr = 0x%ld\n", (unsigned long)buf->vaddr);
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

static int g2d_alph_layer_mmap(struct sdrv_g2d *gd, struct g2d_bg_cfg *bgcfg)
{

	int ret = 0;
	struct g2d_buf *buf = &bgcfg->abufs;

	if (buf->fd > 0) {
		ret = g2d_dmabuf_import(gd, buf);
		if (ret) {
			G2D_ERROR("g2d alph layer mmap faild \n");
			return ret;
		}
		bgcfg->aaddr = buf->dma_addr;
		G2D_DBG("alph layer used, fd is valid: fd = %d , phy addr = 0x%llx\n", buf->fd, bgcfg->aaddr);
	} else {
		G2D_DBG("alph layer used, fd is invalid, aaddr = 0x%llx \n", bgcfg->aaddr);
	}

	return ret;
}

static int g2d_layer_mmap(struct sdrv_g2d *gd, struct g2d_layer *layer)
{
	int ret, i, j;
	struct g2d_buf *buf = &layer->bufs[0];
	uint32_t tmp_addr_h;
	uint32_t tmp_addr_l;

	if (buf->fd <= 0) {
		G2D_ERROR("input layer buf fd invaild, fd(%d) <= 0\n", buf->fd);
		return -EINVAL;
	}

	 ret = g2d_dmabuf_import(gd, buf);
	 if (ret) {
	 	G2D_ERROR("g2d input layer mmap faild \n");
	 	return ret;
	}

	G2D_DBG("layer->nplanes = %d\n", layer->nplanes);
	for (i = 0; i < layer->nplanes; i++) {
		unsigned long addr = buf->dma_addr + layer->offsets[i];
		layer->addr_l[i] = get_l_addr(addr);
		layer->addr_h[i] = get_h_addr(addr);

		G2D_DBG("layer[%d] addr_l[%d] = 0x%x addr_h[%d] = 0x%x\n",
			layer->index, i, layer->addr_l[i], i, layer->addr_h[i]);
	}

	if(layer->format == DRM_FORMAT_BGR888_PLANE) {
		if (layer->nplanes != 3) {
			G2D_ERROR("format set : DRM_FORMAT_BGR888_PLANE, but nplanes(%d) != 3 \n", layer->nplanes);
			return -1;
		}
		tmp_addr_l = layer->addr_l[0];
		tmp_addr_h = layer->addr_h[0];
		layer->addr_l[0] = layer->addr_l[2];
		layer->addr_h[0] = layer->addr_h[2];
		layer->addr_l[2] = tmp_addr_l;
		layer->addr_h[2] = tmp_addr_h;

		for (j = 0; j < layer->nplanes; j++) {
			G2D_DBG("layer[%d] addr_l[%d] = 0x%x addr_h[%d] = 0x%x\n",
				layer->index, j, layer->addr_l[j], j, layer->addr_h[j]);
		}
	}

	return 0;
}

int g2d_output_layer_mmap(struct sdrv_g2d *gd, struct g2d_output_cfg *layer)
{
	int ret;
	int j;
	uint64_t tmp_addr;
	struct g2d_buf *buf = &layer->bufs[0];

	if (buf->fd <= 0) {
		G2D_ERROR("output layer buf fd invaild, fd(%d) <= 0\n", buf->fd);
		return -EINVAL;
	}

	 ret = g2d_dmabuf_import(gd, buf);
	 if (ret) {
	 	G2D_ERROR("g2d output layer mmap faild \n");
	 	return ret;
	}

	for (j = 0; j < layer->nplanes; j++) {
		layer->addr[j] = buf->dma_addr + layer->offsets[j];
		G2D_DBG("layer->addr[%d] = 0x%llx \n", j, layer->addr[j]);
	}

	if(layer->fmt == DRM_FORMAT_BGR888_PLANE) {
		if (layer->nplanes != 3) {
			G2D_ERROR("fmt set : DRM_FORMAT_BGR888_PLANE, but nplanes(%d) != 3 \n", layer->nplanes);
			return -1;
		}
		tmp_addr = layer->addr[0];
		layer->addr[0] = layer->addr[2];
		layer->addr[2] = tmp_addr;
		for (j = 0; j < layer->nplanes; j++) {
			G2D_DBG("fmt == DRM_FORMAT_BGR888_PLANE : layer->addr[%d] = 0x%llx \n", j, layer->addr[j]);
		}
	}

	return 0;
}

void g2d_alph_layer_unmap(struct sdrv_g2d *gd, struct g2d_bg_cfg *bgcfg)
{
	struct g2d_buf *buf = &bgcfg->abufs;

	G2D_DBG("g2d dmabuf:%d\n", buf->fd);
	if (buf->fd <= 0)
		return;
	g2d_dmabuf_release(gd, buf);

}

void g2d_layer_unmap(struct sdrv_g2d *gd, struct g2d_layer *layer)
{
	struct g2d_buf *buf = &layer->bufs[0];

	G2D_DBG("g2d dmabuf:%d\n", buf->fd);
	if (buf->fd <= 0)
		return;
	g2d_dmabuf_release(gd, buf);
}

void g2d_output_layer_unmap(struct sdrv_g2d *gd, struct g2d_output_cfg *layer)
{
	struct g2d_buf *buf = &layer->bufs[0];

	G2D_DBG("g2d dmabuf:%d\n", buf->fd);
	if (buf->fd <= 0)
		return;
	g2d_dmabuf_release(gd, buf);
}

static int g2d_ioctl_begin(struct sdrv_g2d *gd, struct g2d_input *input)
{
	int i;
	int ret;

	set_user_nice(current, -12);
	/*bg layer*/
	if (input->bg_layer.en) {
		ret = g2d_alph_layer_mmap(gd, &input->bg_layer);
		if (ret) {
			return ret;
		}
	}

	/*input layer*/
	for (i = 0; i < input->layer_num; i++) {
		struct g2d_layer *l =  &input->layer[i];
		if (!l->enable)
			continue;
		ret = g2d_layer_mmap(gd, l);
		if (ret) {
			return ret;
		}
	}

	/*output layer*/
	ret = g2d_output_layer_mmap(gd, &input->output);
	if (ret) {
		return ret;
	}

	return 0;
}

static void g2d_ioctl_finish(struct sdrv_g2d *gd, struct g2d_input *input)
{
	int i;

	/*bg layer*/
	if (input->bg_layer.en) {
		g2d_alph_layer_unmap(gd, &input->bg_layer);
	}

	/*input layer*/
	for (i = 0; i < input->layer_num; i++) {
		struct g2d_layer *l =  &input->layer[i];
		if (!l->enable)
			continue;
		g2d_layer_unmap(gd, l);
	}

	/*output layer*/
	g2d_output_layer_unmap(gd, &input->output);
}

static int g2d_wait(struct sdrv_g2d *gd)
{
	int status = 0;
	int rc;

	//g2d_dump_registers(gd);
	/* wait for stop done interrupt wait_event_timeout */
	rc = wait_event_timeout(gd->wq, (gd->frame_done == true),
						   msecs_to_jiffies(wait_timeout));
	gd->frame_done = false;
	if (!rc) {
		status = -1;
		G2D_ERROR("g2d operation wait timeout %d\n", wait_timeout);
		g2d_dump_registers(gd);
	} else {
		if (dump_register_g2d == 1) {
			g2d_dump_registers(gd);
		}
		G2D_DBG("wait time %d\n", rc);
	}

	if (gd->ops->reset)
		gd->ops->reset(gd);

	return status;
}

static int g2d_fill_rect_ioctl(struct sdrv_g2d *gd, struct g2d_input *input)
{
	int ret;
	ret = g2d_fill_rect(gd, &input->bg_layer, &input->output);
	if (ret < 0) {
		G2D_ERROR("g2d fill rect set register err \n");
		goto OUT;
	}

	ret = g2d_wait(gd);

OUT:
	if (ret < 0)
		dump_input(input);
	return ret;
}

static int g2d_fastcopy_dmabuf(struct sdrv_g2d *gd, struct g2d_input *input)
{
	int ret = -1;
	addr_t iaddr, oaddr;
	struct g2d_output_cfg *out_layer = &input->output;
	struct g2d_bg_cfg *bg_layer = &input->bg_layer;
	struct g2d_buf *buf;

	if (!bg_layer->en) {
		G2D_ERROR("bg_layer en is %d, fast copy cannot be used\n", bg_layer->en);
		return ret;
	}

	iaddr = bg_layer->aaddr;
	buf = &out_layer->bufs[0];
	oaddr = buf->dma_addr + out_layer->offsets[0];

	if (iaddr % 4) {
		G2D_ERROR("The phy-addr(0x%lx) of the input needs to be 4-byte aligned\n", iaddr);
		return ret;
	}

	if (oaddr % 4) {
		G2D_ERROR("The phy-addr(0x%lx) of the output needs to be 4-byte aligned\n", oaddr);
		return ret;
	}

	if ((iaddr <= 0) || (oaddr <= 0)) {
		G2D_ERROR("input iaddr(0x%lx) or oaddr(0x%lx) = null\n", iaddr, oaddr);
		return ret;
	}

	ret = g2d_fastcopy_set(gd, iaddr, out_layer->width, out_layer->height,
		bg_layer->astride, oaddr, out_layer->stride[0]);
	if (ret < 0) {
		G2D_ERROR("g2d_fastcopy set register err \n");
		goto OUT;
	}

	ret = g2d_wait(gd);

OUT:
	if (ret < 0)
		dump_input(input);
	return ret;
}

static int sdrv_g2d_post_config(struct sdrv_g2d *gd, struct g2d_input *input)
{
	int ret = 0;

	ret = g2d_post_config(gd, input);
	if(ret < 0)
		goto OUT;

	ret = g2d_wait(gd);

OUT:
	if (ret < 0)
		dump_input(input);
	return ret;
}

static int sdrv_g2d_tasks(struct sdrv_g2d *gd, unsigned int cmd, struct g2d_input *input)
{
	int ret;

	mutex_lock(&gd->m_lock);
	if (gd->monitor.is_monitor)
		gd->monitor.g2d_on_task = true;

	if (input->tables.set_tables) {//set filter tables
		g2d_set_coefficients_table(gd, &input->tables);
	}

	switch (cmd) {
		case G2D_IOCTL_POST_CONFIG:
			ret = sdrv_g2d_post_config(gd, input);
			G2D_DBG(" G2D_IOCTL_POST_CONFIG ret = %d\n", ret);
		break;
		case G2D_IOCTL_FAST_COPY:
			ret = g2d_fastcopy_dmabuf(gd, input);
			G2D_DBG("G2D_IOCTL_FAST_COPY end  ret = %d\n", ret);
		break;
		case G2D_IOCTL_FILL_RECT:
			ret = g2d_fill_rect_ioctl(gd, input);
			G2D_DBG("G2D_IOCTL_FILL_RECT end  ret = %d\n", ret);
		break;
		default:
			G2D_ERROR("Invalid ioctl cmd: 0x%x\n", cmd);
			ret = -EINVAL;
			break;
	}

	if (input->tables.set_tables) {//reset filter tables
		input->tables.set_tables = false;
		g2d_set_coefficients_table(gd, &input->tables);
	}

	if (gd->monitor.is_monitor)
		gd->monitor.g2d_on_task = false;

	mutex_unlock(&gd->m_lock);

	return ret;
}

void sdrv_dpc_to_g2d_layer(struct dpc_layer *int_layer, struct g2d_layer *out_layer)
{
	out_layer->index = int_layer->index; //plane index
	out_layer->enable = int_layer->enable;
	out_layer->nplanes = int_layer->nplanes;
	out_layer->src_x = int_layer->src_x;
	out_layer->src_y = int_layer->src_y;
	out_layer->src_w = int_layer->src_w;
	out_layer->src_h = int_layer->src_h;
	out_layer->dst_x = int_layer->dst_x;
	out_layer->dst_y = int_layer->dst_y;
	out_layer->dst_w = int_layer->dst_w;
	out_layer->dst_h = int_layer->dst_h;
	out_layer->format = int_layer->format;
	out_layer->alpha = int_layer->alpha;
	out_layer->blend_mode = int_layer->blend_mode;
	out_layer->rotation   = int_layer->rotation;
	out_layer->zpos       = int_layer->zpos;
	out_layer->xfbc       = int_layer->xfbc;
	out_layer->modifier   = int_layer->modifier;
	out_layer->width      = int_layer->width;
	out_layer->height     = int_layer->height;

	memcpy(out_layer->addr_l, int_layer->addr_l, sizeof(out_layer->addr_l));
	memcpy(out_layer->addr_h, int_layer->addr_h, sizeof(out_layer->addr_h));
	memcpy(out_layer->pitch, int_layer->pitch, sizeof(out_layer->pitch));
	memcpy(&out_layer->comp, &int_layer->comp, sizeof(struct pix_g2dcomp));
	memcpy(&out_layer->ctx, &int_layer->ctx, sizeof(struct tile_ctx));
}

int sdrv_g2d_convert_format(struct dpc_layer *layer, uint32_t g2d_out_format)
{
	int ret = 0, i = 0;
	struct sdrv_g2d *gd = g_g2d[0];
	struct g2d_input *input = NULL;
	uint32_t size = 0;
	static dma_addr_t paddr[2];
	static void *vaddr[2];
	static uint8_t index = 0;

	if (!gd) {
		G2D_ERROR("g2d hasn't exist\n");
		return -ENODEV;
	}

	input = kzalloc(sizeof(struct g2d_input), GFP_KERNEL);
	if (!input) {
		G2D_ERROR("alloc input error\n");
		return -ENOMEM;
	}

	size = layer->src_w * layer->src_h * 2;
	size = round_up(size, PAGE_SIZE);

	if (!vaddr[0]) {
		for (i = 0; i < 2; i++) {
			vaddr[i] = dma_alloc_wc(&gd->pdev->dev, size,
					&paddr[i], GFP_KERNEL | __GFP_NOWARN);
			if(!vaddr[i]) {
				G2D_ERROR("failed to allocate buffer of size %u\n", size);
				goto alloc_dma_err;
			}

			pr_info("dma addr[%d]:0x%llx vaddr[%d]:0x%p\n", i ,paddr[i], i, vaddr[i]);
		}
	}

	input->layer_num = 1;
	memcpy(&input->layer[0], layer, sizeof(struct g2d_layer));
	sdrv_dpc_to_g2d_layer(layer, &input->layer[0]);

	pr_debug("format:%x, w:%d, h:%d s:%d al:%x\n", layer->format, layer->src_w,
		layer->src_h, layer->pitch[0], layer->addr_l[0]);

	input->output.width = layer->dst_w;
	input->output.height = layer->dst_h;
	input->output.stride[0] = layer->dst_w * 2;
	input->output.fmt = g2d_out_format;
	input->output.nplanes = 1;

	input->output.addr[0] = paddr[index];

	pr_debug("o format:%x, w:%d, h:%d s:%d a:%llx\n", input->output.fmt, input->output.width,
		input->output.height, input->output.stride[0], input->output.addr[0]);

	mutex_lock(&gd->m_lock);

	ret = sdrv_g2d_post_config(gd, input);
	if (ret) {
		mutex_unlock(&gd->m_lock);
		goto out;
	}

	mutex_unlock(&gd->m_lock);

	layer->addr_l[0] = get_l_addr(input->output.addr[0]);
	layer->addr_h[0] = get_h_addr(input->output.addr[0]);
	layer->src_h = input->output.height;
	layer->src_w = input->output.width;
	layer->dst_h = input->output.height;
	layer->dst_w = input->output.width;
	layer->pitch[0] = input->output.stride[0];

	index ++;
	if (index >= 2)
		index = 0;

out:
	kfree(input);
	return ret;

alloc_dma_err:
	while (i) {
		dma_free_wc(&gd->pdev->dev, size, vaddr[i], paddr[i]);
		i--;
	}

	kfree(input);
	return -ENOMEM;
}
EXPORT_SYMBOL(sdrv_g2d_convert_format);

static int sdrv_g2d_func_work(struct sdrv_g2d *gd, unsigned int cmd, struct g2d_input *input)
{
	int ret;

	if (!gd || !input) {
		G2D_ERROR("dev or input isn't inited.[dev:%p, ins:%p]\n", gd, input);
		return -EINVAL;
	}

	if ((input->output.height <= 0) || (input->output.width <= 0)) {
		G2D_ERROR("output input->output.height = %d, input->output.width = %d\n",
			input->output.height, input->output.width);
		return -EINVAL;
	}

	G2D_DBG("\r\n");
	ret = g2d_ioctl_begin(gd, input);
	if (ret) {
		G2D_ERROR("input parameter err\n");
		goto finish_out;
	}

	ret = sdrv_g2d_tasks(gd, cmd, input);

finish_out:
	g2d_ioctl_finish(gd, input);

	return ret;
}

int sdrv_g2d_dma_copy(dma_addr_t dst, dma_addr_t src, size_t data_size)
{
	int ret = 0;
	struct g2d_input *input;
	struct sdrv_g2d *gd = g_g2d[0];
	int width, height, stride;

	width = 32;
	stride = width * 4;
	height = (data_size / stride) + ((data_size % stride) ? 1 : 0);
	G2D_DBG("data_size, width, stride, height : (%ld, %d, %d, %d)\n", data_size, width, stride, height);
	input = kzalloc(sizeof(struct g2d_input), GFP_ATOMIC | GFP_DMA);
	if (!input) {
		G2D_ERROR("kzalloc input failed\n");
		return -EFAULT;
	}

	input->bg_layer.en = 1;
	input->bg_layer.width = width;
	input->bg_layer.height = height;
	input->bg_layer.astride = stride;
	input->bg_layer.aaddr = (uint64_t)src;

	input->output.bufs[0].dma_addr = (uint64_t)dst;
	input->output.width = width;
	input->output.height = height;
	input->output.stride[0] = stride;

	ret = sdrv_g2d_tasks(gd, G2D_IOCTL_FAST_COPY, input);

	kfree(input);
	return ret;
}
EXPORT_SYMBOL(sdrv_g2d_dma_copy);

static long sdrv_g2d_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;
	int i=0, n = 0;
	struct sdrv_g2d *gd = file->private_data;
	struct g2d_input *input;
	struct g2d_inputx *inputx;

	if (_IOC_TYPE(cmd) != G2D_IOCTL_BASE)
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

	inputx = kzalloc(sizeof(struct g2d_inputx), GFP_ATOMIC | GFP_DMA);
	if (!inputx) {
		G2D_ERROR("kzalloc input failed\n");
		return -EFAULT;
	}
	input = kzalloc(sizeof(struct g2d_input), GFP_ATOMIC | GFP_DMA);
	if (!input) {
		G2D_ERROR("kzalloc input failed\n");
		if (inputx)
			kfree(inputx);
		return -EFAULT;
	}
	memset(inputx,0,sizeof(struct g2d_inputx));
	memset(input,0,sizeof(struct g2d_input));
	if (cmd == G2D_IOCTL_GET_CAPABILITIES) {
		ret = copy_to_user((struct g2d_capability __user *)arg, &gd->cap, sizeof(struct g2d_capability));
		if (ret) {
			G2D_ERROR("get capabilities err \n");
			ret = -EFAULT;
		}
	} else {
		ret = copy_from_user(inputx, (struct g2d_inputx __user *)arg, sizeof(struct g2d_inputx));
		if (ret) {
			G2D_ERROR("copy_from_user failed\n");
			ret = -EFAULT;
			goto unlock_out;
		}
		//for 32bit and 64 bit capibility;
		input->layer_num = inputx->layer_num;
		memcpy((void *)(&input->bg_layer),(void *)(&inputx->bg_layer),sizeof(struct g2d_bg_cfg_x));
		input->bg_layer.abufs.dma_addr = input->bg_layer.cfg_buf.dma_addr;
		input->bg_layer.abufs.fd = input->bg_layer.cfg_buf.fd;
		input->bg_layer.abufs.size = input->bg_layer.cfg_buf.size;
		input->bg_layer.abufs.vaddr = input->bg_layer.cfg_buf.vaddr;
		memcpy((void *)(&input->output), (void *)(&inputx->output),sizeof(struct g2d_output_cfg_x));
		for (i = 0; i < 4; i++) {
			input->output.bufs[i].dma_addr = input->output.out_buf[i].dma_addr;
			input->output.bufs[i].fd = input->output.out_buf[i].fd;
			input->output.bufs[i].size = input->output.out_buf[i].size;
			input->output.bufs[i].vaddr = input->output.out_buf[i].vaddr;
		}
		memcpy((void *)(&input->tables), (void *)(&inputx->tables),sizeof(struct g2d_coeff_table));
		for (n = 0; n < G2D_LAYER_MAX_NUM;n ++) {
			memcpy((void *)(&input->layer[n]),(void *)(&inputx->layer[n]),sizeof(struct g2d_layer_x));
			for (i = 0; i < 4; i++) {
				input->layer[n].bufs[i].dma_addr = input->layer[n].in_buf[i].dma_addr;
				input->layer[n].bufs[i].fd = input->layer[n].in_buf[i].fd;
				input->layer[n].bufs[i].size = input->layer[n].in_buf[i].size;
				input->layer[n].bufs[i].vaddr = input->layer[n].in_buf[i].vaddr;
			}
		}
		ret = sdrv_g2d_func_work(gd, cmd, input);
	}

unlock_out:
	if (input)
		kfree(input);
	if (inputx)
		kfree(inputx);
	return (long)ret;
}

#if defined(CONFIG_COMPAT)
static long sdrv_g2d_compat_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	return sdrv_g2d_ioctl(file, cmd, arg);
}
#endif /* defined(CONFIG_COMPAT) */

ssize_t sdrv_g2d_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct sdrv_g2d *gd = file->private_data;
	char str[64] = {0};
	ssize_t sz = sprintf(str, "read from %s\n", gd->name);
	if (copy_to_user(buf, str, sz)){
		G2D_ERROR("copy to user failed: %s\n", gd->name);
	}
	return sz;
}

static const struct file_operations g2d_fops = {
	.owner = THIS_MODULE,
	.open = sdrv_g2d_open,
	.read = sdrv_g2d_read,
	.unlocked_ioctl = sdrv_g2d_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sdrv_g2d_compat_ioctl,
#endif
};

static int g2d_misc_init(struct sdrv_g2d *gd)
{
	int ret;
	struct miscdevice *m = &gd->mdev;;

	m->minor = MISC_DYNAMIC_MINOR;
	m->name = kasprintf(GFP_KERNEL, "g2d%d", gd->id);
	m->fops = &g2d_fops;
	m->parent = NULL;
	m->groups = sdrv_g2d_groups;

	ret = misc_register(m);
	if (ret) {
		G2D_ERROR("failed to register miscdev\n");
		return ret;
	}

	G2D_INFO("%s misc register \n", m->name);

	return ret;
}

static int sdrv_g2d_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_g2d *gd = NULL;
	static int pipe_registered = 0;
	dma_addr_t dma_handle;
	int ret = 0, i;

	mutex_lock(&m_init);
	G2D_INFO("G2D BUILD VERSION : %s \n", version);
    // 38 bits address for KUNLUN G2D rdma,use G2D_CPU_WRITE config 38bit; use G2D_CMD_WRITE config 32bit
	dma_set_mask(dev, DMA_BIT_MASK(32));
	dma_set_coherent_mask(dev, DMA_BIT_MASK(32));

	gd = devm_kzalloc(&pdev->dev, sizeof(struct sdrv_g2d), GFP_KERNEL);
	if (!gd) {
		G2D_ERROR("kalloc sdrv_g2d failed\n");
		ret = -1;
		goto OUT;
	}
	gd->du_inited = false;
	gd->pdev = pdev;
	if (!pipe_registered) {
		pipe_registered++;
		g2d_pipe_ops_register(&spipe_g2d_entry);
		g2d_pipe_ops_register(&gpipe_high_g2d_entry);
		g2d_pipe_ops_register(&gpipe_mid_g2d_entry);
	}
	/*cmdfile init*/
	gd->cmd_info[0].arg = (unsigned int*)dma_alloc_coherent(dev,
		G2D_CMDFILE_MAX_MEM * sizeof(unsigned int), &dma_handle, GFP_KERNEL);
	gd->dma_buf = (unsigned long)dma_handle;
	if (gd->cmd_info[0].arg == NULL) {
		G2D_ERROR("malloc cmd_info failed\n");
		goto OUT;
	}
	G2D_INFO("gd->cmd_info[0].arg virtual address = 0x%lx, phy address 0x%lx,dma alloc coherent len = %ld\n",
		(unsigned long)gd->cmd_info[0].arg, gd->dma_buf, G2D_CMDFILE_MAX_MEM * sizeof(unsigned int));
	for (i = 1 ; i < G2D_CMDFILE_MAX_NUM; i++) {
		gd->cmd_info[i].arg = gd->cmd_info[i - 1].arg + G2D_CMDFILE_MAX_MEM / G2D_CMDFILE_MAX_NUM;
	}

#ifdef CONFIG_OF
	G2D_INFO("CONFIG_OF scope\n");
	sdrv_init_iommu(gd);
	ret = sdrv_g2d_init(gd, dev->of_node);
#else
	G2D_INFO("CONFIG_OF is closed\n");
	ret = sdrv_g2d_init(gd, pdev);
#endif
	if (ret)
		goto OUT;

	mutex_init(&gd->m_lock);
	gd->monitor.sampling_time = 5;
	ret = g2d_misc_init(gd);
	if (ret)
		goto OUT;
	else
		printk("%s : semidrive g2d driver registered.\n", __func__);

	platform_set_drvdata(pdev, gd);
	g_g2d[gd->id] = gd;
	gd->du_inited = true;

	enable_irq(gd->irq);

	ret = 0;
OUT:
	mutex_unlock(&m_init);
	return ret;
}

static int sdrv_g2d_remove(struct platform_device *pdev)
{
	struct sdrv_g2d *gd = platform_get_drvdata(pdev);
	G2D_DBG("remove g2d %s\n", gd->name);

	if (gd) {
		sdrv_iommu_cleanup(gd);
		sdrv_g2d_unit(gd);
		misc_deregister(&gd->mdev);
	}

	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id g2d_of_table[] = {
	{.compatible = "semidrive,g2d", .data = g2d_data},
	{.compatible = "semidrive,g2d_lite", .data = g2d_data},
	{},
};
#endif

static int sdrv_g2d_suspend(struct device *dev)
{
	struct sdrv_g2d *gd = dev_get_drvdata(dev);

	G2D_INFO("%s start\n", __func__);

	gd->ops->reset(gd);

	G2D_INFO("gd->du_inited = %d, gd->num_pipe = %d\n",
		gd->du_inited, gd->num_pipe);

	G2D_INFO("%s end\n", __func__);
	return 0;
}

static int sdrv_g2d_resume(struct device *dev)
{
	struct sdrv_g2d *gd = dev_get_drvdata(dev);
	struct g2d_pipe *p = NULL;
	int i;

	G2D_INFO("%s start\n", __func__);

	G2D_INFO("gd->du_inited = %d, gd->num_pipe = %d\n",
		gd->du_inited, gd->num_pipe);

	gd->ops->init(gd);

	for (i = 0; i < gd->num_pipe; i++) {
		p = gd->pipes[i];
		if (p && p->ops->init)
			p->ops->init(p);
		else
			G2D_ERROR("p or p->ops->init is null\n");
	}

	gd->ops->reset(gd);

	G2D_INFO("%s end\n", __func__);
	return 0;
}

static const struct dev_pm_ops sdrv_g2d_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdrv_g2d_suspend,
				sdrv_g2d_resume)
};

static struct platform_driver g2d_driver = {
	.probe = sdrv_g2d_probe,
	.remove = sdrv_g2d_remove,
	.driver = {
		.name = "semidrive-g2d",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = g2d_of_table,
#endif
		.pm = &sdrv_g2d_pm_ops,
	},
};
module_platform_driver(g2d_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive g2d");
MODULE_LICENSE("GPL");
