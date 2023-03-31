#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <linux/kernel.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/semidrive/ipcc.h>

#include "drm_drv.h"
#include "sdrv_plane.h"
#include "sdrv_dpc.h"

extern const struct dpc_operation dc_r0p1_ops;
extern const struct dpc_operation dp_r0p1_ops;
extern const struct dpc_operation dp_dummy_ops;
extern const struct dpc_operation dp_rpcall_ops;
extern const struct dpc_operation dp_unilink_ops;

struct class *display_class;
struct kunlun_crtc *kcrtc_primary = NULL;

static int dp_enable = 1;
module_param(dp_enable, int, 0660);
static int regdump_flag = 0;
module_param(regdump_flag, int, 0660);
static DEFINE_MUTEX(udp_lock);

/*sysfs begin*/
#pragma pack(push)
#pragma pack(1)
typedef struct display_node_t
{
	uint8_t state : 3;
	uint8_t : 5;
}display_node_t;

#pragma pack(4)
typedef union disp_sync_t
{
	display_node_t args[4]; //four panels
	uint32_t val;
}disp_sync_args_t;
#pragma pack(pop)

#if 0
	DC_STAT_NOTINIT     = 0,	/* not initilized by remote cpu */
	DC_STAT_INITING     = 1,	/* on initilizing */
	DC_STAT_INITED      = 2,	/* initilize compilete, ready for display */
	DC_STAT_BOOTING     = 3,	/* during boot time splash screen */
	DC_STAT_CLOSING     = 4,	/* DC is going to close */
	DC_STAT_CLOSED      = 5,	/* DC is closed safely */
	DC_STAT_NORMAL      = 6,	/* DC is used by DRM */
	DC_STAT_MAX
#endif
void sdrv_crtc_dpms(struct drm_crtc *crtc, int mode);

void dump_registers(struct sdrv_dpc *dpc, int start, int end)
{
	int i, count;
	uint32_t (*pos)[2];
	struct reg_debug_st *reg_debug;
	struct kunlun_crtc *kcrtc = dpc->crtc;

	count = (end - start) / 4 + 1;

	if (kcrtc->enable_debug && kcrtc->recover_done) {
		count = ALIGN(count, 4);
	//	count = (count + 3) / 4;

		reg_debug = &dpc->reg_debug;

		pos = reg_debug->reg_dmem[reg_debug->index];
		pos = pos + reg_debug->count;

		for (i = 0; i < count; i++) {
			pos[i][0] = start + 0x4 * i;
			pos[i][1] = readl(dpc->regs + start + 0x4 * i);

			reg_debug->count++;
		}
		return;
	}

	count = (count + 3) / 4;

	if (regdump_flag)
		pr_info("--[crtc-%d]-[dpc-type:%d]-[0x%04x~0x%04x]-----\n",
			dpc->crtc->base.index, dpc->dpc_type, start, end);
	else
		pr_err("--[crtc-%d]-[dpc-type:%d]-[0x%04x~0x%04x]-----\n",
			dpc->crtc->base.index, dpc->dpc_type, start, end);

	for (i = 0; i < count; i++) {
		if (regdump_flag) {
			pr_info("0x%08x 0x%08x 0x%08x 0x%08x\n",
				readl(dpc->regs + start + 0x10 * i),
				readl(dpc->regs + start + 0x10 * i + 0x4),
				readl(dpc->regs + start + 0x10 * i + 0x8),
				readl(dpc->regs + start + 0x10 * i + 0xc));
		}
		else {
			pr_err("0x%08x 0x%08x 0x%08x 0x%08x\n",
				readl(dpc->regs + start + 0x10 * i),
				readl(dpc->regs + start + 0x10 * i + 0x4),
				readl(dpc->regs + start + 0x10 * i + 0x8),
				readl(dpc->regs + start + 0x10 * i + 0xc));
		}
	}
}
EXPORT_SYMBOL(dump_registers);

static int sdrv_dpc_regdump_init(struct sdrv_dpc *dpc)
{
	uint8_t i = 0;
	int enable = 0;
	struct reg_debug_st *reg_debug = NULL;

	reg_debug = &dpc->reg_debug;
	enable = dpc->crtc->enable_debug;

	if (!enable)
		return 0;

	for (i = 0; i < DPC_REG_FRAME_CONT; i++) {
		reg_debug->reg_dmem[i] = kzalloc(DPC_REG_NUM * 2 * (sizeof(uint32_t)), GFP_KERNEL);
		if (!dpc->reg_debug.reg_dmem[i]) {
			DRM_ERROR("alloc reg_dump mem err\n");
			goto alloc_err;
		}
	}

	dpc->reg_debug.count = 0;
	dpc->reg_debug.index = 0;
	dpc->reg_debug.enable = true;

	return 0;
alloc_err:
	while(i) {
		i--;
		kfree(reg_debug->reg_dmem[i]);
	}

	reg_debug->enable = false;
	return -ENOMEM;
}

static void sdrv_dpc_regdump_unit(struct sdrv_dpc *dpc)
{
	uint8_t i = 0;
	struct reg_debug_st *reg_debug = NULL;

	reg_debug = &dpc->reg_debug;

	if (!reg_debug->enable)
		return;

	for (i = 0; i < DPC_REG_FRAME_CONT; i++) {
		kfree(reg_debug->reg_dmem[i]);
	}

	reg_debug->enable = false;
}

static void sdrv_dpc_regdump_update(struct sdrv_dpc *dpc)
{
	struct reg_debug_st *reg_debug = NULL;
	struct kunlun_crtc *kcrtc = dpc->crtc;

	reg_debug = &dpc->reg_debug;

	if (!reg_debug->enable && !regdump_flag)
		return;

	if (regdump_flag) {
		kcrtc->recover_done = false;
		DRM_INFO("@@@@[crtc-%d]-[dpc-type:%d]-start@@@@\n", kcrtc->base.index, dpc->dpc_type);
	}
	dpc->reg_debug.count = 0;
	if (dpc->ops->dump)
		dpc->ops->dump(dpc);

	if (dpc->next) {
		dpc->next->reg_debug.count = 0;
		if (dpc->next->ops->dump)
			dpc->next->ops->dump(dpc->next);
	}

	if (regdump_flag) {
		kcrtc->recover_done = true;
		DRM_INFO("@@@@[crtc-%d]-[dpc-type:%d]-end@@@@\n", kcrtc->base.index, dpc->dpc_type);
		return;
	}

	dpc->reg_debug.index++;
	if (dpc->reg_debug.index >= DPC_REG_FRAME_CONT)
		dpc->reg_debug.index = 0;

	if (dpc->next) {
		dpc->next->reg_debug.index++;
		if (dpc->next->reg_debug.index >= DPC_REG_FRAME_CONT)
			dpc->next->reg_debug.index = 0;
	}
}

static void sdrv_dpc_regdump(struct sdrv_dpc *dpc)
{
	uint8_t redump = 0;
	uint32_t i = 0;
	uint32_t (*pos)[2];
	struct reg_debug_st *reg_debug = NULL;

	reg_debug = &dpc->reg_debug;

	if (!reg_debug->enable || regdump_flag)
		return;

retry:
	pos = reg_debug->reg_dmem[reg_debug->index];

	pr_info("--[crtc-%d]-[dpc-type:%d]-[frame:%d]-[start]--\n",
		dpc->crtc->base.index, dpc->dpc_type, redump);

	for (i = 0; i < reg_debug->count; i+=4) {
		pr_info("0x%04x:0x%08x 0x%08x 0x%08x 0x%08x\n",
			pos[i][0], pos[i][1], pos[i+1][1], pos[i+2][1], pos[i+3][1]);
	}

	pr_info("--[crtc-%d]-[dpc-type:%d]-[frame:%d]-[end]--\n",
			dpc->crtc->base.index, dpc->dpc_type, redump);

	redump++;

	dpc->reg_debug.index++;
	if (dpc->reg_debug.index >= DPC_REG_FRAME_CONT)
		dpc->reg_debug.index = 0;

	if (redump != DPC_REG_FRAME_CONT)
		goto retry;
}

void drm_mode_convert_sdrv_mode(struct drm_display_mode *mode, struct sdrv_dpc_mode *sd_mode)
{
	sd_mode->clock = mode->clock;
	sd_mode->flags = mode->flags;

	sd_mode->hdisplay = mode->hdisplay;
	sd_mode->hsync_start = mode->hsync_start;
	sd_mode->hsync_end = mode->hsync_end;
	sd_mode->hskew = mode->hskew;
	sd_mode->htotal = mode->htotal;

	sd_mode->vdisplay = mode->vdisplay;
	sd_mode->vsync_start = mode->vsync_start;
	sd_mode->vsync_end = mode->vsync_end;
	sd_mode->vtotal = mode->vtotal;
	sd_mode->vscan = mode->vscan;
	sd_mode->vrefresh = mode->vrefresh;
}
EXPORT_SYMBOL(drm_mode_convert_sdrv_mode);

static void kunlun_crtc_recovery(struct kunlun_crtc *kcrtc);
void sdrv_crtc_recovery(struct drm_crtc *crtc)
{
	int i;
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);

	kcrtc->recover_done = false;

	if ((!kcrtc->master->next) && (kcrtc->master->dpc_type != DPC_TYPE_DC))
		sdrv_rpcall_dcrecovery(kcrtc);

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = (i % 2) ? kcrtc->master : kcrtc->slave;

		if (!dpc)
			continue;

		sdrv_dpc_regdump(dpc);

		if (dpc->ops->dump) {
			pr_err("@@@@[crtc-%d]-[dpc-type:%d]-start@@@@\n", kcrtc->base.index, dpc->dpc_type);
			dpc->ops->dump(dpc);
			pr_err("@@@@[crtc-%d]-[dpc-type:%d]-end@@@@\n", kcrtc->base.index, dpc->dpc_type);
			if (dpc->next && dpc->next->ops->dump) {
				pr_err("@@@@[crtc-%d]-[dpc-type:%d]-start@@@@\n", kcrtc->base.index, dpc->next->dpc_type);
				dpc->next->ops->dump(dpc->next);
				pr_err("@@@@[crtc-%d]-[dpc-type:%d]-end@@@@\n", kcrtc->base.index, dpc->next->dpc_type);
			}
		}
	}

	kunlun_crtc_recovery(kcrtc);
}
EXPORT_SYMBOL(sdrv_crtc_recovery);

static inline int check_avm_alive(struct kunlun_crtc *kcrtc) {
	disp_sync_args_t status;
	struct sdrv_dpc  *dpc = kcrtc->master;

	if (dpc->next || dpc->dpc_type == DPC_TYPE_DC || dp_enable)
		return 0;

	status.val = kcrtc->avm_dc_status;
	switch (status.args[kcrtc->id].state) {
		case DC_STAT_NOTINIT:
			return 0;
		case DC_STAT_INITING:
		case DC_STAT_INITED:
		case DC_STAT_BOOTING:
			return 1;
		case DC_STAT_CLOSING:
		case DC_STAT_CLOSED:
			return 0;
		default:
			return 0;
	}

	return 0;
}

static ssize_t fastavm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	dc_state_t status;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);
	mutex_lock(&kcrtc->refresh_mutex);
	ret = sd_get_dc_status(&status);
	mutex_unlock(&kcrtc->refresh_mutex);
	if (ret) {
		pr_err("[drm] sd_get_dc_status failed\n");
		kcrtc->avm_dc_status = DC_STAT_NOTINIT;
		ret = snprintf(buf, PAGE_SIZE, "sd_get_dc_status failed: 0x%x\n", kcrtc->avm_dc_status);
		return ret;
	}
	kcrtc->avm_dc_status = (int)status;
	ret = snprintf(buf, PAGE_SIZE, "new status: 0x%x, old status: 0x%x\n", (uint32_t)status, kcrtc->avm_dc_status);

	return ret;
}

static ssize_t fastavm_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);
	disp_sync_args_t status;

	ret = kstrtoint(buf, 16, &status.val);
	if (ret) {
		DRM_ERROR("Invalid input: %s\n", buf);
		return count;
	}

	PRINT("set avm_dc_status = 0x%x\n", (int) status.val);
	mutex_lock(&kcrtc->refresh_mutex);
	sd_set_dc_status((int) status.val);
	ret = sd_get_dc_status((dc_state_t *)&kcrtc->avm_dc_status);
	mutex_unlock(&kcrtc->refresh_mutex);
	if (ret) {
		DRM_ERROR("[drm] sd_get_dc_status failed\n");
		kcrtc->avm_dc_status = DC_STAT_NOTINIT;
	}

	return count;
}
static DEVICE_ATTR_RW(fastavm);


int fps(struct kunlun_crtc *kcrtc)
{
	u64 now;

	now = ktime_get_boot_ns() / 1000 / 1000;

	if (now - kcrtc->fi.sw_last_time > 1000) // 取固定时间间隔为1秒
	{
		kcrtc->fi.sw_fps = kcrtc->fi.sw_frame_count;
		kcrtc->fi.sw_frame_count = 0;
		kcrtc->fi.sw_last_time = now;
	}

	if (now - kcrtc->fi.hw_last_time > 1000) // 取固定时间间隔为1秒
	{
		kcrtc->fi.hw_fps = kcrtc->fi.hw_frame_count;
		kcrtc->fi.hw_frame_count = 0;
		kcrtc->fi.hw_last_time = now;
	}

	return 0;
}

static ssize_t sw_fps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "sw fps: %d\n", kcrtc->fi.sw_fps);

	return ret;
}
static DEVICE_ATTR_RO(sw_fps);

static ssize_t hw_fps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "hw fps: %d\n", kcrtc->fi.hw_fps);

	return ret;
}
static DEVICE_ATTR_RO(hw_fps);

static struct attribute *crtc_attrs[] = {
	&dev_attr_fastavm.attr,
	&dev_attr_sw_fps.attr,
	&dev_attr_hw_fps.attr,
	NULL,
};

static const struct attribute_group crtc_group = {
	.attrs = crtc_attrs,
};

int kunlun_crtc_sysfs_init(struct device *dev)
{
	int rc;

	pr_info("[drm] %s: crtc %s sysfs init\n", __func__, dev_name(dev));
	rc = sysfs_create_group(&(dev->kobj), &crtc_group);
	if (rc)
		pr_err("create crtc attr node failed, rc=%d\n", rc);
	return rc;
}

EXPORT_SYMBOL(kunlun_crtc_sysfs_init);

static int compare_name(struct device_node *np, const char *name)
{
	return strstr(np->name, name) != NULL;
}

static ssize_t dispInfo_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	ssize_t count = 0;
	int i, j;
	struct sdrv_dpc *dpc = dev_get_drvdata(dev);
	struct kunlun_crtc *kcrtc = dpc->crtc;
	count += sprintf(buf + count, "%s: combine_mode[%d] num_pipe[%d]:\n", dpc->name, dpc->combine_mode, dpc->num_pipe);
	for (i = 0; i < dpc->num_pipe; i++) {
		struct sdrv_pipe *p = dpc->pipes[i];
		struct dpc_layer *layer = &(p->layer_data);
		count += sprintf(buf + count, "  %s: plane_type[%d] plane_enable[%d]\n", p->name, p->type, layer->enable);
		if (!layer->enable)
			continue;
		count += sprintf(buf + count, "    hw_fps[%d] index[%d] formart[%c%c%c%c] alpha[%d] blend_mode[%d] rotation[%d]\n",
			kcrtc->fi.hw_fps, layer->index, layer->format & 0xff, (layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff,
			(layer->format >> 24) & 0xff, layer->alpha, layer->blend_mode, layer->rotation);
		count += sprintf(buf + count, "    zpos[%d] xfbc[%d] header_size_r[%d] header_size_y[%d] header_size_uv[%d]\n",
			layer->zpos, layer->xfbc, layer->header_size_r, layer->header_size_y, layer->header_size_uv);
		count += sprintf(buf + count, "    src[%d %d %d %d] dst[%d %d %d %d] width[%d] height[%d]\n",
			layer->src_x, layer->src_y, layer->src_w, layer->src_h, layer->dst_x, layer->dst_y, layer->dst_w,
			layer->dst_h, layer->width, layer->height);
		count += sprintf(buf + count,
			"modifier[0x%llx]\n", layer->modifier);
		count += sprintf(buf + count, "    nplanes[%d] :\n", layer->nplanes);
		for(j = 0; j < layer->nplanes; j++) {
			count += sprintf(buf + count, "      addr_l[0x%x] addr_h[0x%x] pitch[%d]\n",
				layer->addr_l[j], layer->addr_h[j], layer->pitch[j]);
		}
	}
	return count;
}

static DEVICE_ATTR_RO(dispInfo);

static struct attribute *disp_class_attrs[] = {
	&dev_attr_dispInfo.attr,
	NULL,
};

static const struct attribute_group disp_group = {
	.attrs = disp_class_attrs,
};

static const struct attribute_group *disp_groups[] = {
	&disp_group,
	NULL,
};

static int __init display_class_init(void)
{
	pr_info("display class register\n");
	if (display_class)
		return 0;
	display_class = class_create(THIS_MODULE, "display");
	if (IS_ERR(display_class)) {
		pr_err("[drm] Unable to create display class: %ld\n", PTR_ERR(display_class));
		return PTR_ERR(display_class);
	}

	display_class->dev_groups = disp_groups;

	return 0;
}

int *get_zorders_from_pipe_mask(uint32_t mask)
{
	int i, n;
	static int zorders[8] = {0};
	int z = 0;
	for(i = 0, n = 0; i < 8; i++) {
		if (mask & (1 << i)) {
			n++;
			if (z == 0)
				z = i;
		}
	}

	for (i = 0; i < n; i++) {
		zorders[i] = z + i;
	}

	return zorders;
}

int _add_pipe(struct sdrv_dpc *dpc, int type, int hwid, const char *pipe_name, uint32_t offset)
{
	struct sdrv_pipe *p = NULL;

	p = devm_kzalloc(&dpc->dev, sizeof(struct sdrv_pipe), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	p->name = pipe_name;
	p->ops = (struct pipe_operation*)pipe_ops_attach(p->name);
	if (!p->ops) {
		DRM_ERROR("error ops attached\n");
		return -EINVAL;
	}

	p->regs = dpc->regs + offset;
	p->id = hwid;
	p->dpc = dpc;
	p->type = type;
	p->fbdc_status = false;
	p->enable_status = false;

	dpc->pipes[dpc->num_pipe] = p;
	dpc->pipe_mask |= (1 << hwid);
	dpc->num_pipe++;

	p->ops->init(p);

	return 0;
}
EXPORT_SYMBOL(_add_pipe);

static void sdrv_wait_updatedone_work(struct work_struct *work)
{
	int ret = 0;
	unsigned long flags;

	struct sdrv_dpc *dpc = NULL;
	struct kunlun_crtc *kcrtc = container_of(work,
						struct kunlun_crtc,
						vsync_work);
	struct drm_crtc *crtc = &kcrtc->base;

	dpc = kcrtc->master;

	if (dpc->ops->update_done)
		ret = dpc->ops->update_done(dpc);

	if (!ret) {
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		if(kcrtc->event) {
			drm_crtc_send_vblank_event(crtc, kcrtc->event);
			drm_crtc_vblank_put(crtc);
			kcrtc->event = NULL;
		}
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

void kunlun_crtc_handle_vblank(struct kunlun_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;

	if (!crtc)
		return;
	drm_crtc_handle_vblank(crtc);

	if(kcrtc->event)
		queue_work(system_highpri_wq, &kcrtc->vsync_work);

	/* count the hw fps*/
	kcrtc->fi.hw_frame_count++;
	fps(kcrtc);
}

struct dp_dummy_data *dummy_datas[10];
static int num_dummy_data = 0;

int register_dummy_data(struct dp_dummy_data *dummy)
{
	int id = num_dummy_data;
	if (!dummy || id >= 10) {
		return -ENODEV;
	}
	DRM_INFO("register dummy data: %p\n", dummy);
	dummy_datas[id] = dummy;
	num_dummy_data++;
	return 0;
}

void unregister_dummy_data(struct dp_dummy_data *dummy)
{
	int id = num_dummy_data;
	if (!dummy) {
		return;
	}
	for (id = num_dummy_data -1; id >= 0; id--) {
		if (dummy_datas[id] == dummy) {
			dummy_datas[id] = 0;
			num_dummy_data--;
		}
	}
}

void call_dummy_vsync(void) {
	int i = 0;
	struct dp_dummy_data *d;
	struct kunlun_crtc *kcrtc;
	for (i = 0; i < num_dummy_data; i++) {
		d = dummy_datas[i];
		kcrtc = d->dpc->crtc;
		if (d->vsync_enabled) {
			kunlun_crtc_handle_vblank(kcrtc);
		}
	}
}

irqreturn_t sdrv_irq_handler(int irq, void *data)
{
	struct sdrv_dpc *dpc = data;
	struct kunlun_crtc *kcrtc = dpc->crtc;
	uint32_t val;

	if (!kcrtc || !kcrtc->du_inited) {
		pr_info_ratelimited("[drm] kcrtc %d, du_inited %d\n",
				    kcrtc->id, kcrtc->du_inited);
		return IRQ_HANDLED;
	}

	val = kcrtc->master->ops->irq_handler(dpc);

	if(val & SDRV_TCON_EOF_MASK) {
		kunlun_crtc_handle_vblank(kcrtc);
		call_dummy_vsync();
	}

	if(val & SDRV_DC_UNDERRUN_MASK) {
		pr_err_ratelimited("under run.....\n");
	}

	if ((dpc->dpc_type == 0) && (val & SDRV_RDMA_MASK)) {
		pr_err_ratelimited("crtc%d dp rdma error:0x%08x\n",
				   kcrtc->id, kcrtc->master->ops->rdma_irq_handler(dpc));
	}

	if ((dpc->dpc_type == 0) && (val & SDRV_MLC_MASK)) {
		pr_err_ratelimited("crtc%d dp mlc error:0x%08x\n",
				   kcrtc->id, kcrtc->master->ops->mlc_irq_handler(dpc));
	}

	return IRQ_HANDLED;
}

static void kunlun_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	int i, j;
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);

	// mutex_lock(&kcrtc->refresh_mutex);
	//slave first
	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!dpc || !crtc->state->planes_changed)
			continue;

		for (j = 0; j< kcrtc->num_planes; j++) {
			struct sdrv_pipe *p = dpc->pipes[j];
			struct dpc_layer *layer = (struct dpc_layer *)&p->layer_data;

			memset(layer, 0, sizeof(struct dpc_layer));
		}
	}

}

static void kunlun_crtc_atomic_flush(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	int ret;
	int i, j;
	// if(WARN_ON(kcrtc->enabled))
	// 	return;

	if (!kcrtc->recover_done)
		goto OUT;

	state->plane_mask = crtc->state->plane_mask;

	//slave first
	for (i = 0; i < 2; i++) {
		u8 count = 0;
		struct dpc_layer *layers[16];
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;

		if (!dpc)
			continue;

		for (j = 0; j< kcrtc->num_planes; j++) {
			u8 plane_enabled =
				state->plane_mask & (1 << drm_plane_index(&kcrtc->planes[j].base))? 1: 0;
			struct sdrv_pipe *p = dpc->pipes[j];
			if (p->layer_data.enable != plane_enabled) {
				pr_debug("id %d this layer[%d] should enabled? %d : %d  plane_mask 0x%x : 0x%x\n",
					kcrtc->planes[j].base.base.id, j, plane_enabled,
					p->layer_data.enable, state->plane_mask,
					(1 << drm_plane_index(&kcrtc->planes[j].base)));
			}
			layers[j] = &p->layer_data;
			if (plane_enabled)
				count++;
			else
				memset(&p->layer_data, 0, sizeof(struct dpc_layer));
		}

		ret = dpc->ops->update(dpc, *layers, count);

		if (ret) {
			DRM_ERROR("update plane[%d] failed\n", i);
			goto OUT;
		}

		dpc->ops->flush(dpc);
		if (dpc->next)
			dpc->next->ops->flush(dpc->next);

		sdrv_dpc_regdump_update(dpc);
	}

OUT:
	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event && old_crtc_state->active) {
		WARN_ON(drm_crtc_vblank_get(crtc));
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	/* count the sw fps*/
	kcrtc->fi.sw_frame_count++;
	fps(kcrtc);

	// mutex_unlock(&kcrtc->refresh_mutex);
	return;
}

static void kunlun_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	int i;

	DRM_DEV_INFO(kcrtc->dev,
			"set mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
			mode->base.id, mode->name, mode->vrefresh, mode->clock,
			mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
			mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
			mode->type, mode->flags);

	state->plane_mask |= crtc->state->plane_mask;

	//slave first
	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (dpc && dpc->ops->modeset) {
			dpc->ops->modeset(dpc, mode, state->bus_flags);
			if (dpc->next && dpc->next->ops->modeset) {
				dpc->next->ops->mlc_select(dpc->next, dpc->mlc_select);
				dpc->next->ops->modeset(dpc->next, mode, state->bus_flags);
			}

		}

	}
}

static void kunlun_crtc_atomic_enable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	int i = 0;

	//if(WARN_ON(kcrtc->enabled))
	//	return;

	//slave first
	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!kcrtc->enable_status && dpc) {
			dpc->ops->enable(dpc, true);
			if (dpc->next)
				dpc->next->ops->enable(dpc->next, true);
		}
	}

	if (!kcrtc->enable_status) {
		if (kcrtc->irq)
			enable_irq(kcrtc->irq);
	}

	kcrtc->enable_status = true;

	drm_crtc_vblank_on(crtc);
	WARN_ON(drm_crtc_vblank_get(crtc));

	if (crtc->state->event) {
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
}

static void kunlun_crtc_atomic_disable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	int i = 0, j;
	struct sdrv_dpc *dpc;
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);

	// if(WARN_ON(!kcrtc->enabled))
	//	return;
	DRM_INFO("crtc_atomic_disable\n");

	for (i = 0; i < 2; i++) {
		dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!dpc || !crtc->state->planes_changed)
			continue;

		for (j = 0; j< kcrtc->num_planes; j++) {

			struct sdrv_pipe *p = dpc->pipes[j];
			struct dpc_layer *layer = (struct dpc_layer *)&p->layer_data;

			memset(layer, 0, sizeof(struct dpc_layer));
		}

		if (dpc->ops->shutdown) {
			dpc->ops->shutdown(dpc);
			if (dpc->next && dpc->next->ops->shutdown) {
				dpc->next->ops->shutdown(dpc->next);
			}
		}
	}

	if (kcrtc->enable_status) {
		if (kcrtc->irq)
			disable_irq(kcrtc->irq);
	}

	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	drm_crtc_vblank_off(crtc);

	kcrtc->enable_status = false;

	/* kcrtc->enabled = false;*/
}


static const struct drm_crtc_helper_funcs kunlun_crtc_helper_funcs = {
	.dpms = sdrv_crtc_dpms,
	.atomic_flush = kunlun_crtc_atomic_flush,
	.atomic_begin = kunlun_crtc_atomic_begin,
	.mode_set_nofb = kunlun_crtc_mode_set_nofb,
	.atomic_enable = kunlun_crtc_atomic_enable,
	.atomic_disable = kunlun_crtc_atomic_disable,
};

static void kunlun_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

static void kunlun_crtc_reset(struct drm_crtc *crtc)
{
	struct kunlun_crtc_state *kunlun_state = NULL;

	if (crtc->state) {
		kunlun_state = to_kunlun_crtc_state(crtc->state);
		kfree(kunlun_state);
	}

	kunlun_state = kzalloc(sizeof(struct kunlun_crtc_state), GFP_KERNEL);
	if (!kunlun_state) {
		DRM_ERROR("Cannot allocate kunlun_crtc_state\n");
		return;
	}

	crtc->state = &kunlun_state->base;
	crtc->state->crtc = crtc;
}

static struct drm_crtc_state *
kunlun_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct kunlun_crtc_state *kunlun_state, *current_state;

	current_state = to_kunlun_crtc_state(crtc->state);

	kunlun_state = kzalloc(sizeof(struct kunlun_crtc_state), GFP_KERNEL);
	if(!kunlun_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &kunlun_state->base);

	kunlun_state->plane_mask = current_state->plane_mask;

	return &kunlun_state->base;
}

static void kunlun_crtc_destroy_state(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct kunlun_crtc_state *s = to_kunlun_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(state);

	kfree(s);
}

static int kunlun_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct sdrv_dpc *dpc;

	if(kcrtc->vblank_enable)
		return -EPERM;

	dpc = kcrtc->master;
	dpc->ops->vblank_enable(dpc, true);

	kcrtc->vblank_enable = true;

	return 0;
}
void sdrv_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	DRM_INFO("kcrtc name %s set dpms %d\n", kcrtc->master->name, mode);
}

static void kunlun_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct sdrv_dpc *dpc;

	if(!kcrtc->vblank_enable)
		return;

	dpc = kcrtc->master;
	dpc->ops->vblank_enable(dpc, false);

	kcrtc->vblank_enable = false;
}

static const struct drm_crtc_funcs kunlun_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = kunlun_crtc_destroy,
	.reset = kunlun_crtc_reset,
	.atomic_duplicate_state = kunlun_crtc_duplicate_state,
	.atomic_destroy_state = kunlun_crtc_destroy_state,
	.enable_vblank = kunlun_crtc_enable_vblank,
	.disable_vblank = kunlun_crtc_disable_vblank,
//	.set_crc_source = kunlun_crtc_set_crc_source,
};

static u32 sdrv_dpc_yuvout_format(const char *format_name)
{
	uint8_t str_len = 0;

	str_len = strlen(format_name);

	switch (str_len) {
		case 2:
			return fourcc_code(format_name[0], format_name[1], ' ', ' ');
		case 3:
			return fourcc_code(format_name[0], format_name[1],format_name[2], ' ');
		case 4:
			return fourcc_code(format_name[0], format_name[1],format_name[2],format_name[3]);
		default:
			break;
	}

	return 0;
}

static int sdrv_dpc_device_create(struct sdrv_dpc *dpc,
				struct device *parent, struct device_node *np)
{
	int err;

	if (!parent) {
		DRM_ERROR("parent device is null\n");
		return -1;
	}

	dpc->dev.class = display_class;
	dpc->dev.parent = parent;
	dpc->dev.of_node = np;
	dev_set_name(&dpc->dev, "%s", np->name);
	dev_set_drvdata(&dpc->dev, dpc);

	err = device_register(&dpc->dev);
	if (err)
		DRM_ERROR("dpc device register failed\n");

	return err;
}

static int sdrv_dpc_get_resource(struct sdrv_dpc *dpc, struct device_node *np)
{
	struct resource res;
	const char *str;
	int irq_num;
	int ret;

	if (!dpc) {
		pr_err("dpc pointer is null\n");
		return -EINVAL;
	}

	if (!of_property_read_string(np, "sdrv,ip", &str)) {
		dpc->name = str;
	} else
		DRM_WARN("sdrv,ip can not found\n");

	if (compare_name(np, "dummy"))
		dpc->dpc_type = DPC_TYPE_DUMMY;

	if (compare_name(np, "unilink")) {
		DRM_INFO("np->name = %s \n", np->name);
		dpc->dpc_type = DPC_TYPE_UNILINK;
	}

	if (compare_name(np, "video")) {
		DRM_INFO("np->name = %s\n", np->name);
		dpc->dpc_type = DPC_TYPE_VIDEO;
	}

	DRM_INFO("dpc->dpc_type = %d\n", dpc->dpc_type);
	if (dpc->dpc_type >= DPC_TYPE_DUMMY)
		return 0;


	/*Only Linux Os, we always need dc.*/
	if (of_address_to_resource(np, 0, &res)) {
		DRM_ERROR("parse dt base address failed\n");
		return -ENODEV;
	}
	DRM_INFO("got %s res 0x%lx\n", dpc->name, (unsigned long)res.start);

	dpc->regs = devm_ioremap_nocache(&dpc->dev, res.start, resource_size(&res));
	if(IS_ERR(dpc->regs)) {
		DRM_ERROR("Cannot find dpc regs 001\n");
		return PTR_ERR(dpc->regs);
	}

	irq_num = irq_of_parse_and_map(np, 0);
	if (!irq_num) {
		DRM_ERROR("error: dpc parse irq num failed\n");
		return -EINVAL;
	}
	DRM_INFO("dpc irq_num = %d\n", irq_num);
	dpc->irq = irq_num;

	ret = of_property_read_u32(np, "mlc-select", &dpc->mlc_select);
	if (ret) {
		dpc->mlc_select = 0;
	}

	return 0;
}

static int sdrv_dpc_init(struct sdrv_dpc *dpc, struct device_node *np)
{
	int i, ret;
	const char *str;
	struct device *pdev;
	const struct sdrv_dpc_data *data;

	if (!dpc)
		return -EINVAL;
	pdev = dpc->crtc->dev;

	dpc->dpc_type = compare_name(np, "dp") ? DPC_TYPE_DP: DPC_TYPE_DC;

	sdrv_dpc_device_create(dpc, pdev, np);

	ret = sdrv_dpc_get_resource(dpc, np);
	if (ret) {
		DRM_ERROR("dpc get resource failed\n");
		return -EINVAL;
	}

	dpc->fmt_change_cfg.enable = of_property_read_bool(np, "input-fmt-change-enable");
	if (dpc->fmt_change_cfg.enable) {
		if (!of_property_read_string(np, "disp-input-fmt", &str))
			dpc->fmt_change_cfg.disp_input_format = sdrv_dpc_yuvout_format(str);
		else
			DRM_ERROR("yuv-input-format don't set please check\n");
	}

	dpc->fmt_change_cfg.g2d_convert = of_property_read_bool(np, "g2d-convert-enable");
	if (dpc->fmt_change_cfg.g2d_convert) {
		if (!of_property_read_string(np, "g2d-out-fmt", &str))
			dpc->fmt_change_cfg.g2d_output_format = sdrv_dpc_yuvout_format(str);
		else
			DRM_ERROR("yuv-output-format don't set please check\n");
	}

	DRM_INFO("yuv out:%d disp_input_format:0x%x  g2d_convert:%d g2d_out_format:0x%x\n",
		dpc->fmt_change_cfg.enable, dpc->fmt_change_cfg.disp_input_format,
		dpc->fmt_change_cfg.g2d_convert,dpc->fmt_change_cfg.g2d_output_format);

	data = of_device_get_match_data(pdev);
	for (i = 0; i < 16; i++){
		if (!strcmp(dpc->name, data[i].version)) {
			dpc->ops = data[i].ops;
			DRM_INFO("%s ops[%d] attached\n", dpc->name, i);
			break;
		}
	}
	if (dpc->ops == NULL) {
		DRM_ERROR("core ops attach failed, have checked %d times\n", i);
		return -1;
	}
	dpc->num_pipe = 0;
	// dpc init
	dpc->ops->init(dpc);

	sdrv_dpc_regdump_init(dpc);

	return 0;
}

static void sdrv_dpc_unit(struct sdrv_dpc *dpc)
{
	if (!dpc)
		return;
	if (dpc->ops->uninit)
		dpc->ops->uninit(dpc);

	sdrv_dpc_regdump_unit(dpc);
}

static void kunlun_crtc_recovery(struct kunlun_crtc *kcrtc)
{
	int i;

	DRM_INFO("recovery crtc-%d start\n", kcrtc->base.index);

	if (kcrtc->master) {
		if (kcrtc->irq)
			disable_irq(kcrtc->irq);
		kcrtc->master->ops->init(kcrtc->master);
		if (kcrtc->master->next)
			kcrtc->master->next->ops->init(kcrtc->master->next);
	}

	if (kcrtc->slave) {
		kcrtc->slave->ops->init(kcrtc->slave);
		if (kcrtc->slave->next)
			kcrtc->slave->next->ops->init(kcrtc->slave->next);
	}

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
			if (dpc) {
				dpc->ops->enable(dpc, true);
				if (dpc->next)
					dpc->next->ops->enable(dpc->next, true);
			}
	}

	kcrtc->vblank_enable = false;
	kcrtc->recover_done = true;

	if (kcrtc->irq)
		enable_irq(kcrtc->irq);

	kunlun_crtc_enable_vblank(&kcrtc->base);

	DRM_INFO("recovery crtc-%d end\n", kcrtc->base.index);
}

static int sdrv_get_crc_resource(struct sdrv_dpc *dpc, const struct device_node *np)
{
	struct device_node *crc_np;
	struct device_node *dc_np;
	struct resource res;

	dpc->kick_sel = false;

	crc_np = of_parse_phandle(np, "crc-node", 0);
	if(!crc_np) {
		DRM_INFO("dts parse crc-node is null\n");
		return -ENODEV;
	}

	if(!of_device_is_available(crc_np)) {
		DRM_INFO("OF node %s not available or match\n", crc_np->name);
		return -ENODEV;
	}

	dc_np = of_parse_phandle(crc_np, "crc-dc", 0);
	if (!dc_np) {
		DRM_INFO("fail to find \"crc-dc\"\n");
		return -ENODEV;
	}

	DRM_INFO("display crc is set\n");

	if (of_address_to_resource(dc_np, 0, &res)) {
		DRM_ERROR("parse dc np base address failed\n");
		return -ENODEV;
	}

	DRM_INFO("got crc-dc res 0x%lx\n", (unsigned long)res.start);

	dpc->crc_dc_regs = devm_ioremap_nocache(&dpc->dev, res.start, resource_size(&res));
	if(IS_ERR(dpc->crc_dc_regs)) {
		DRM_ERROR("Cannot find crc dc regs\n");
		return PTR_ERR(dpc->crc_dc_regs);
	}

	dpc->kick_sel = true;

	return 0;
}

static int kunlun_crtc_bind(struct device *dev, struct device *master,
		void *data)
{
	struct device_node *np;
	struct device_node *port;
	struct drm_device *drm = data;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	struct kunlun_crtc *kcrtc;
	struct drm_plane *primary = NULL, *cursor = NULL;
	struct sdrv_dpc *dpc;
	u32 temp;
	int i, ret;

	kcrtc = dev_get_drvdata(dev);
	kcrtc->drm = drm;

	// check combine mode of_property_read_u32
	if (!of_property_read_u32(dev->of_node, "display-mode", &temp))
		kcrtc->combine_mode = temp;
	else
		kcrtc->combine_mode = 1;

	kcrtc->enable_debug = of_property_read_bool(dev->of_node, "crtc,enable-debug");
	DRM_INFO("crtc enable debug:%d\n", kcrtc->enable_debug);

	for (i = 0; ; i++) {
		np = of_parse_phandle(dev->of_node, "dpc-master", i);
		if (!np)
			break;
		if(!of_device_is_available(np)) {
			DRM_DEV_INFO(dev, "OF node %s not available or match\n", np->name);
			continue;
		}
		dpc = (struct sdrv_dpc *)devm_kzalloc(dev, sizeof(struct sdrv_dpc), GFP_KERNEL);
		if (!dpc) {
			DRM_ERROR("kzalloc dpc failed\n");
			return -ENOMEM;
		}
		dpc->crtc = kcrtc;
		DRM_DEV_INFO(dev, "Add component %s", np->name);
		dpc->combine_mode = kcrtc->combine_mode;
		dpc->is_master = true;
		dpc->inited = false;

		if (!kcrtc->master)
			kcrtc->master = dpc;
		else
			kcrtc->master->next = dpc;

		sdrv_dpc_init(dpc, np);
	}
	if (!kcrtc->master) {
		DRM_ERROR("dpc-master is not specified, dts maybe not match\n");
		return -EINVAL;
	}

	if (kcrtc->combine_mode <= 1) {
		ret = sdrv_get_crc_resource(kcrtc->master, dev->of_node);
		if (ret) {
			DRM_INFO("crc is not set\n");
		}
	}

	if (kcrtc->combine_mode > 1) {
		for (i = 0; ; i++) {
			np = of_parse_phandle(dev->of_node, "dpc-slave", i);
			if (!np)
				break;
			//TODO: check the master and slave dpc type??
			if(!of_device_is_available(np)) {
				DRM_DEV_INFO(dev, "OF node %s not available or match\n", np->name);
				continue;
			}
			dpc = (struct sdrv_dpc *)devm_kzalloc(dev, sizeof(struct sdrv_dpc), GFP_KERNEL);
			if (!dpc) {
				DRM_ERROR("kzalloc dpc failed\n");
				return -ENOMEM;
			}
			dpc->crtc = kcrtc;
			DRM_DEV_INFO(dev, "Add component %s", np->name);
			dpc->combine_mode = kcrtc->combine_mode;
			dpc->is_master = false;
			dpc->inited = false;

			if (!kcrtc->slave)
				kcrtc->slave = dpc;
			else
				kcrtc->slave->next = dpc;

			sdrv_dpc_init(dpc, np);
		}

		if (kcrtc->slave->num_pipe != kcrtc->master->num_pipe) {
			DRM_ERROR("The pipes number of master(%d) and slave(%d) is not match!\n",
				kcrtc->master->num_pipe, kcrtc->slave->num_pipe);
			goto fail_dpc_init;
		}

		/* binding master and slave pipes in combine mode */
		for (i = 0; i < kcrtc->master->num_pipe; i++) {
			kcrtc->master->pipes[i]->next = kcrtc->slave->pipes[i];
		}
	}
	mutex_init(&kcrtc->refresh_mutex);

	//irq_set_status_flags(irq_num, IRQ_NOAUTOEN);
	kcrtc->irq = kcrtc->master->irq;
	if (kcrtc->master->dpc_type < DPC_TYPE_DUMMY) {
		irq_set_status_flags(kcrtc->irq, IRQ_NOAUTOEN);
		ret = devm_request_irq(dev, kcrtc->irq, sdrv_irq_handler,
			0, dev_name(dev), kcrtc->master); //IRQF_SHARED
		if(ret) {
			DRM_DEV_ERROR(dev, "Failed to request DC IRQ: %d\n", kcrtc->irq);
			goto fail_dpc_init;
		}
	}

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		DRM_DEV_ERROR(dev, "no port node found in %pOF\n", dev->of_node);
		return -ENOENT;
	}
	kcrtc->base.port = port;
	// plane init
	kcrtc->num_planes = kcrtc->master->num_pipe;

	kcrtc->planes = devm_kzalloc(kcrtc->dev, kcrtc->num_planes * sizeof(struct kunlun_plane), GFP_KERNEL);
	if(!kcrtc->planes)
		return -ENOMEM;
	DDBG(kcrtc->master->num_pipe);
	for (i = 0; i < kcrtc->num_planes; i++) {
		kcrtc->planes[i].type = kcrtc->master->pipes[i]->type;
		kcrtc->planes[i].cap = kcrtc->master->pipes[i]->cap;
		kcrtc->planes[i].kcrtc = kcrtc;

		if (kcrtc->combine_mode > 1) {
			kcrtc->planes[i].pipes[0] = kcrtc->master->pipes[i];
			kcrtc->planes[i].pipes[1] = kcrtc->slave->pipes[i];
			kcrtc->planes[i].num_pipe = 2;
		} else { //only master
			kcrtc->planes[i].pipes[0] = kcrtc->master->pipes[i];
			kcrtc->planes[i].num_pipe = 1;
		}
	}

	ret = kunlun_planes_init_primary(kcrtc, &primary, &cursor);
	if(ret)
		goto err_planes_fini;
	if(!primary) {
		DRM_DEV_ERROR(kcrtc->dev, "CRTC cannot find primary plane\n");
		goto err_planes_fini;
	}

	ret = drm_crtc_init_with_planes(kcrtc->drm, &kcrtc->base,
			primary, cursor, &kunlun_crtc_funcs, NULL);
	if(ret)
		goto err_planes_fini;

	drm_crtc_helper_add(&kcrtc->base, &kunlun_crtc_helper_funcs);

	ret = kunlun_planes_init_overlay(kcrtc);
	if(ret)
		goto err_crtc_cleanup;

	kunlun_crtc_sysfs_init(dev);

	kdrm->crtcs[kdrm->num_crtcs] = &kcrtc->base;
	kdrm->num_crtcs++;
	kcrtc->vblank_enable = false;
	kcrtc->du_inited = true;
	kcrtc->enable_status = false;
	kcrtc->recover_done = true;

	if (!kcrtc_primary)
		kcrtc_primary = kcrtc;

	INIT_WORK(&kcrtc->vsync_work, sdrv_wait_updatedone_work);

	return 0;
err_crtc_cleanup:
	drm_crtc_cleanup(&kcrtc->base);
err_planes_fini:
	kunlun_planes_fini(kcrtc);
fail_dpc_init:
	sdrv_dpc_unit(kcrtc->slave);
	sdrv_dpc_unit(kcrtc->slave->next);
	sdrv_dpc_unit(kcrtc->master);
	sdrv_dpc_unit(kcrtc->master->next);
	return -1;
}

static void kunlun_crtc_unbind(struct device *dev,
		struct device *master, void *data)
{
	// struct kunlun_crtc *crtc = dev_get_drvdata(dev);

	// kunlun_crtc_planes_fini(crtc);
}
static const struct component_ops kunlun_crtc_ops = {
	.bind = kunlun_crtc_bind,
	.unbind = kunlun_crtc_unbind,
};

static int kunlun_drm_crtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct kunlun_crtc *kcrtc;

	kcrtc = (struct kunlun_crtc *)devm_kzalloc(&pdev->dev, sizeof(struct kunlun_crtc), GFP_KERNEL);
	if (!kcrtc)
		return -ENOMEM;

	kcrtc->dev = dev;
	platform_set_drvdata(pdev, kcrtc);

	if (!display_class) {
		// ugly..
		pipe_ops_register(&spipe_entry);
		pipe_ops_register(&gpipe_dc_entry);
		pipe_ops_register(&gpipe1_dp_entry);
		pipe_ops_register(&gpipe0_dp_entry);
		pipe_ops_register(&apipe_entry);
	}
	display_class_init();

	return component_add(dev, &kunlun_crtc_ops);
}

static int kunlun_drm_crtc_remove(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct kunlun_crtc *kcrtc = platform_get_drvdata(pdev);

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!dpc)
			continue;
		if (dpc->ops->shutdown) {
			dpc->ops->shutdown(dpc);
			if (dpc->next && dpc->next->ops->shutdown) {
				dpc->next->ops->shutdown(dpc->next);
			}
		}
	}

	component_del(dev, &kunlun_crtc_ops);
	return 0;
}

static void kunlun_drm_crtc_shutdown(struct platform_device *pdev)
{
	int i;
	struct kunlun_crtc *kcrtc = platform_get_drvdata(pdev);

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!dpc)
			continue;
		if (dpc->ops->shutdown)
			dpc->ops->shutdown(dpc);
	}
}

#ifdef CONFIG_PM_SLEEP
int kunlun_crtc_sys_suspend(struct drm_crtc *crtc)
{
	int i = 0;
	struct kunlun_crtc *kcrtc = container_of(crtc, struct kunlun_crtc, base);

	DRM_INFO("[crtc-%d] suspend begin!\n", crtc->index);

	if (!kcrtc)
		return 0;

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;

		if (!dpc)
			continue;

		if (dpc->dpc_type == DPC_TYPE_DC)
			dpc->pclk = NULL;

		DRM_INFO("[crtc-%d]-[dpc-name:%s]-[dpc-type:%d]\n", crtc->index, dpc->name, dpc->dpc_type);
		if (dpc->ops->shutdown) {
			dpc->ops->shutdown(dpc);
			if (dpc->next && dpc->next->ops->shutdown) {
				dpc->next->ops->shutdown(dpc->next);
			}
		}
	}

	kcrtc->enable_status = false;

	DRM_INFO("[crtc-%d] suspend completed!\n", crtc->index);

	return 0;
}

int kunlun_crtc_sys_resume(struct drm_crtc *crtc)
{
	int i;
	struct kunlun_crtc *kcrtc = container_of(crtc, struct kunlun_crtc, base);
	struct sdrv_pipe *p = NULL;

	DRM_INFO("[crtc-%d] resume begin!\n", crtc->index);

	kcrtc->recover_done = false;

	if (kcrtc->master) {
		kcrtc->master->ops->init(kcrtc->master);
		for (i = 0; i < kcrtc->master->num_pipe; i++) {
			p = kcrtc->master->pipes[i];
			if (p && p->ops->init)
				p->ops->init(p);
		}
		if (kcrtc->master->next) {
			kcrtc->master->next->ops->init(kcrtc->master->next);
			for (i = 0; i < kcrtc->master->next->num_pipe; i++) {
				p = kcrtc->master->next->pipes[i];
				if (p && p->ops->init)
					p->ops->init(p);
			}
		}
	}

	if (kcrtc->slave) {
		kcrtc->slave->ops->init(kcrtc->slave);
		for (i = 0; i < kcrtc->slave->num_pipe; i++) {
			p = kcrtc->slave->pipes[i];
			if (p && p->ops->init)
				p->ops->init(p);
		}
		if (kcrtc->slave->next) {
			kcrtc->slave->next->ops->init(kcrtc->slave->next);
			for (i = 0; i < kcrtc->slave->next->num_pipe; i++) {
				p = kcrtc->slave->next->pipes[i];
				if (p && p->ops->init)
					p->ops->init(p);
			}
		}
	}

	kcrtc->recover_done = true;

	DRM_INFO("[crtc-%d] resume completed!\n", crtc->index);
	return 0;
}
#endif

struct sdrv_dpc_data dpc_data[] = {
	{.version = "dc-r0p1", .ops = &dc_r0p1_ops},
	{.version = "dp-r0p1", .ops = &dp_r0p1_ops},
	{.version = "dp-dummy", .ops = &dp_dummy_ops},
	{.version = "dp-rpcall", .ops = &dp_rpcall_ops},
	{.version = "dp-unilink", .ops = &dp_unilink_ops},
	{.version = "dp-video", .ops = &dp_unilink_ops},
	{},
};

static const struct of_device_id kunlun_crtc_of_table[] = {
	{.compatible = "semidrive,kunlun-crtc", .data = dpc_data},
	{},
};

static struct platform_driver kunlun_crtc_driver = {
	.probe = kunlun_drm_crtc_probe,
	.remove = kunlun_drm_crtc_remove,
	.shutdown = kunlun_drm_crtc_shutdown,
	.driver = {
		.name = "kunlun-crtc",
		.of_match_table = kunlun_crtc_of_table,
	},
};
module_platform_driver(kunlun_crtc_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive kunlun dc CRTC");
MODULE_LICENSE("GPL");
