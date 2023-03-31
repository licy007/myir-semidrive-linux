// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Linaro Limited, All rights reserved.
 * Author: Mike Leach <mike.leach@linaro.org>
 */

#include <linux/property.h>
#include <linux/coresight.h>
#include <linux/amba/bus.h>
#include "coresight-cti.h"

/**
 * CTI devices can be associated with a PE, or be connected to CoreSight
 * hardware. We have a list of all CTIs irrespective of CPU bound or
 * otherwise.
 *
 * We assume that the non-CPU CTIs are always powered as we do with sinks etc.
 *
 * We leave the client to figure out if all the CTIs are interconnected with
 * the same CTM, in general this is the case but does not always have to be.
 */

/* net of CTI devices connected via CTM */
LIST_HEAD(ect_net);

/* protect the list */
static DEFINE_MUTEX(ect_mutex);

#define csdev_to_cti_drvdata(csdev)	\
	dev_get_drvdata(csdev->dev.parent)

/* write set of regs to hardware - call with spinlock claimed */
void cti_write_all_hw_regs(struct cti_drvdata *drvdata)
{
	struct cti_config *config = &drvdata->config;
	int i;

	CS_UNLOCK(drvdata->base);

	/* disable CTI before writing registers */
	writel_relaxed(0, drvdata->base + CTICONTROL);

	/* write the CTI trigger registers */
	for (i = 0; i < config->nr_trig_max; i++) {
		writel_relaxed(config->ctiinen[i], drvdata->base + CTIINEN(i));
		writel_relaxed(config->ctiouten[i],
			       drvdata->base + CTIOUTEN(i));
	}

	/* other regs */
	writel_relaxed(config->ctigate, drvdata->base + CTIGATE);
	writel_relaxed(config->asicctl, drvdata->base + ASICCTL);
	writel_relaxed(config->ctiappset, drvdata->base + CTIAPPSET);

	/* re-enable CTI */
	writel_relaxed(1, drvdata->base + CTICONTROL);

	CS_LOCK(drvdata->base);
}

static void cti_enable_hw_smp_call(void *info)
{
	struct cti_drvdata *drvdata = info;

	cti_write_all_hw_regs(drvdata);
}

/* write regs to hardware and enable */
static int cti_enable_hw(struct cti_drvdata *drvdata)
{
	struct cti_config *config = &drvdata->config;
	struct device *dev = &drvdata->csdev->dev;
	int rc = 0;

	pm_runtime_get_sync(dev->parent);
	spin_lock(&drvdata->spinlock);

	/* no need to do anything if enabled or unpowered*/
	if (config->hw_enabled || !config->hw_powered)
		goto cti_state_unchanged;

	if (drvdata->ctidev.cpu >= 0) {
		rc = smp_call_function_single(drvdata->ctidev.cpu,
					      cti_enable_hw_smp_call,
					      drvdata, 1);
		if (rc)
			goto cti_err_not_enabled;
	} else {
		cti_write_all_hw_regs(drvdata);
	}

	config->hw_enabled = true;
	atomic_inc(&drvdata->config.enable_req_count);
	spin_unlock(&drvdata->spinlock);
	return rc;

cti_state_unchanged:
	atomic_inc(&drvdata->config.enable_req_count);

	/* cannot enable due to error */
cti_err_not_enabled:
	spin_unlock(&drvdata->spinlock);
	pm_runtime_put(dev->parent);
	return rc;
}

/* disable hardware */
static int cti_disable_hw(struct cti_drvdata *drvdata)
{
	struct cti_config *config = &drvdata->config;
	struct device *dev = &drvdata->csdev->dev;

	spin_lock(&drvdata->spinlock);

	/* check refcount - disable on 0 */
	if (atomic_dec_return(&drvdata->config.enable_req_count) > 0)
		goto cti_not_disabled;

	/* no need to do anything if disabled or cpu unpowered */
	if (!config->hw_enabled || !config->hw_powered)
		goto cti_not_disabled;

	CS_UNLOCK(drvdata->base);

	/* disable CTI */
	writel_relaxed(0, drvdata->base + CTICONTROL);
	config->hw_enabled = false;

	CS_LOCK(drvdata->base);
	spin_unlock(&drvdata->spinlock);
	pm_runtime_put(dev);
	return 0;

	/* not disabled this call */
cti_not_disabled:
	spin_unlock(&drvdata->spinlock);
	return 0;
}

void cti_write_single_reg(struct cti_drvdata *drvdata, int offset, u32 value)
{
	CS_UNLOCK(drvdata->base);
	writel_relaxed(value, drvdata->base + offset);
	CS_LOCK(drvdata->base);
}

void cti_write_intack(struct device *dev, u32 ackval)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct cti_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	/* write if enabled */
	if (cti_active(config))
		cti_write_single_reg(drvdata, CTIINTACK, ackval);
	spin_unlock(&drvdata->spinlock);
}

/*
 * Look at the HW DEVID register for some of the HW settings.
 * DEVID[15:8] - max number of in / out triggers.
 */
#define CTI_DEVID_MAXTRIGS(devid_val) ((int) BMVAL(devid_val, 8, 15))

/* DEVID[19:16] - number of CTM channels */
#define CTI_DEVID_CTMCHANNELS(devid_val) ((int) BMVAL(devid_val, 16, 19))

static void cti_set_default_config(struct device *dev,
				   struct cti_drvdata *drvdata)
{
	struct cti_config *config = &drvdata->config;
	u32 devid;

	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	config->nr_trig_max = CTI_DEVID_MAXTRIGS(devid);

	/*
	 * no current hardware should exceed this, but protect the driver
	 * in case of fault / out of spec hw
	 */
	if (config->nr_trig_max > CTIINOUTEN_MAX) {
		dev_warn_once(dev,
			"Limiting HW MaxTrig value(%d) to driver max(%d)\n",
			config->nr_trig_max, CTIINOUTEN_MAX);
		config->nr_trig_max = CTIINOUTEN_MAX;
	}

	config->nr_ctm_channels = CTI_DEVID_CTMCHANNELS(devid);

	/* Most regs default to 0 as zalloc'ed except...*/
	config->trig_filter_enable = true;
	config->ctigate = GENMASK(config->nr_ctm_channels - 1, 0);
	atomic_set(&config->enable_req_count, 0);
}

/*
 * Add a connection entry to the list of connections for this
 * CTI device.
 */
int cti_add_connection_entry(struct device *dev, struct cti_drvdata *drvdata,
			     struct cti_trig_con *tc,
			     struct coresight_device *csdev,
			     const char *assoc_dev_name)
{
	struct cti_device *cti_dev = &drvdata->ctidev;

	tc->con_dev = csdev;
	/*
	 * Prefer actual associated CS device dev name to supplied value -
	 * which is likely to be node name / other conn name.
	 */
	if (csdev)
		tc->con_dev_name = dev_name(&csdev->dev);
	else if (assoc_dev_name != NULL) {
		tc->con_dev_name = devm_kstrdup(dev,
						assoc_dev_name, GFP_KERNEL);
		if (!tc->con_dev_name)
			return -ENOMEM;
	}
	list_add_tail(&tc->node, &cti_dev->trig_cons);
	cti_dev->nr_trig_con++;

	/* add connection usage bit info to overall info */
	drvdata->config.trig_in_use |= tc->con_in->used_mask;
	drvdata->config.trig_out_use |= tc->con_out->used_mask;

	return 0;
}

/* create a trigger connection with appropriately sized signal groups */
struct cti_trig_con *cti_allocate_trig_con(struct device *dev, int in_sigs,
					   int out_sigs)
{
	struct cti_trig_con *tc = NULL;
	struct cti_trig_grp *in = NULL, *out = NULL;

	tc = devm_kzalloc(dev, sizeof(struct cti_trig_con), GFP_KERNEL);
	if (!tc)
		return tc;

	in = devm_kzalloc(dev,
			  offsetof(struct cti_trig_grp, sig_types[in_sigs]),
			  GFP_KERNEL);
	if (!in)
		return NULL;

	out = devm_kzalloc(dev,
			   offsetof(struct cti_trig_grp, sig_types[out_sigs]),
			   GFP_KERNEL);
	if (!out)
		return NULL;

	tc->con_in = in;
	tc->con_out = out;
	tc->con_in->nr_sigs = in_sigs;
	tc->con_out->nr_sigs = out_sigs;
	return tc;
}

/*
 * Add a default connection if nothing else is specified.
 * single connection based on max in/out info, no assoc device
 */
int cti_add_default_connection(struct device *dev, struct cti_drvdata *drvdata)
{
	int ret = 0;
	int n_trigs = drvdata->config.nr_trig_max;
	u32 n_trig_mask = GENMASK(n_trigs - 1, 0);
	struct cti_trig_con *tc = NULL;

	/*
	 * Assume max trigs for in and out,
	 * all used, default sig types allocated
	 */
	tc = cti_allocate_trig_con(dev, n_trigs, n_trigs);
	if (!tc)
		return -ENOMEM;

	tc->con_in->used_mask = n_trig_mask;
	tc->con_out->used_mask = n_trig_mask;
	ret = cti_add_connection_entry(dev, drvdata, tc, NULL, "default");
	return ret;
}

/** cti channel api **/
/* attach/detach channel from trigger - write through if enabled. */
int cti_channel_trig_op(struct device *dev, enum cti_chan_op op,
			enum cti_trig_dir direction, u32 channel_idx,
			u32 trigger_idx)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct cti_config *config = &drvdata->config;
	u32 trig_bitmask;
	u32 chan_bitmask;
	u32 reg_value;
	int reg_offset;

	/* ensure indexes in range */
	if ((channel_idx >= config->nr_ctm_channels) ||
	   (trigger_idx >= config->nr_trig_max))
		return -EINVAL;

	trig_bitmask = BIT(trigger_idx);

	/* ensure registered triggers and not out filtered */
	if (direction == CTI_TRIG_IN)	{
		if (!(trig_bitmask & config->trig_in_use))
			return -EINVAL;
	} else {
		if (!(trig_bitmask & config->trig_out_use))
			return -EINVAL;

		if ((config->trig_filter_enable) &&
		    (config->trig_out_filter & trig_bitmask))
			return -EINVAL;
	}

	/* update the local register values */
	chan_bitmask = BIT(channel_idx);
	reg_offset = (direction == CTI_TRIG_IN ? CTIINEN(trigger_idx) :
		      CTIOUTEN(trigger_idx));

	spin_lock(&drvdata->spinlock);

	/* read - modify write - the trigger / channel enable value */
	reg_value = direction == CTI_TRIG_IN ? config->ctiinen[trigger_idx] :
		     config->ctiouten[trigger_idx];
	if (op == CTI_CHAN_ATTACH)
		reg_value |= chan_bitmask;
	else
		reg_value &= ~chan_bitmask;

	/* write local copy */
	if (direction == CTI_TRIG_IN)
		config->ctiinen[trigger_idx] = reg_value;
	else
		config->ctiouten[trigger_idx] = reg_value;

	/* write through if enabled */
	if (cti_active(config))
		cti_write_single_reg(drvdata, reg_offset, reg_value);
	spin_unlock(&drvdata->spinlock);
	return 0;
}

int cti_channel_gate_op(struct device *dev, enum cti_chan_gate_op op,
			u32 channel_idx)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct cti_config *config = &drvdata->config;
	u32 chan_bitmask;
	u32 reg_value;
	int err = 0;

	if (channel_idx >= config->nr_ctm_channels)
		return -EINVAL;

	chan_bitmask = BIT(channel_idx);

	spin_lock(&drvdata->spinlock);
	reg_value = config->ctigate;
	switch (op) {
	case CTI_GATE_CHAN_ENABLE:
		reg_value |= chan_bitmask;
		break;

	case CTI_GATE_CHAN_DISABLE:
		reg_value &= ~chan_bitmask;
		break;

	default:
		err = -EINVAL;
		break;
	}
	if (err == 0) {
		config->ctigate = reg_value;
		if (cti_active(config))
			cti_write_single_reg(drvdata, CTIGATE, reg_value);
	}
	spin_unlock(&drvdata->spinlock);
	return err;
}

int cti_channel_setop(struct device *dev, enum cti_chan_set_op op,
		      u32 channel_idx)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct cti_config *config = &drvdata->config;
	u32 chan_bitmask;
	u32 reg_value;
	u32 reg_offset;
	int err = 0;

	if (channel_idx >= config->nr_ctm_channels)
		return -EINVAL;

	chan_bitmask = BIT(channel_idx);

	spin_lock(&drvdata->spinlock);
	reg_value = config->ctiappset;
	switch (op) {
	case CTI_CHAN_SET:
		config->ctiappset |= chan_bitmask;
		reg_value  = config->ctiappset;
		reg_offset = CTIAPPSET;
		break;

	case CTI_CHAN_CLR:
		config->ctiappset &= ~chan_bitmask;
		reg_value = chan_bitmask;
		reg_offset = CTIAPPCLEAR;
		break;

	case CTI_CHAN_PULSE:
		config->ctiappset &= ~chan_bitmask;
		reg_value = chan_bitmask;
		reg_offset = CTIAPPPULSE;
		break;

	default:
		err = -EINVAL;
		break;
	}

	if ((err == 0) && cti_active(config))
		cti_write_single_reg(drvdata, reg_offset, reg_value);
	spin_unlock(&drvdata->spinlock);

	return err;
}

/** cti ect operations **/
int cti_enable(struct coresight_device *csdev)
{
	struct cti_drvdata *drvdata = csdev_to_cti_drvdata(csdev);

	return cti_enable_hw(drvdata);
}

int cti_disable(struct coresight_device *csdev)
{
	struct cti_drvdata *drvdata = csdev_to_cti_drvdata(csdev);

	return cti_disable_hw(drvdata);
}

const struct coresight_ops_ect cti_ops_ect = {
	.enable = cti_enable,
	.disable = cti_disable,
};

const struct coresight_ops cti_ops = {
	.ect_ops = &cti_ops_ect,
};

/*
 * Free up CTI specific resources
 * called by dev->release, need to call down to underlying csdev release.
 */
static void cti_device_release(struct device *dev)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct cti_drvdata *ect_item, *ect_tmp;

	mutex_lock(&ect_mutex);

	/* remove from the list */
	list_for_each_entry_safe(ect_item, ect_tmp, &ect_net, node) {
		if (ect_item == drvdata) {
			list_del(&ect_item->node);
			break;
		}
	}
	mutex_unlock(&ect_mutex);

	if (drvdata->csdev_release)
		drvdata->csdev_release(dev);
}

static int cti_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret = 0;
	void __iomem *base;
	struct device *dev = &adev->dev;
	struct cti_drvdata *drvdata = NULL;
	struct coresight_desc cti_desc;
	struct coresight_platform_data *pdata = NULL;
	struct resource *res = &adev->res;
	/* driver data*/
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		ret = -ENOMEM;
		dev_info(dev, "%s, mem err\n", __func__);
		goto err_out;
	}

	/* Validity for the resource is already checked by the AMBA core */
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		ret = PTR_ERR(base);
		dev_err(dev, "%s, remap err\n", __func__);
		goto err_out;
	}
	drvdata->base = base;

	dev_set_drvdata(dev, drvdata);

	/* default CTI device info  */
	drvdata->ctidev.cpu = -1;
	drvdata->ctidev.nr_trig_con = 0;
	drvdata->ctidev.ctm_id = 0;
	INIT_LIST_HEAD(&drvdata->ctidev.trig_cons);

	spin_lock_init(&drvdata->spinlock);

	/* initialise CTI driver config values */
	cti_set_default_config(dev, drvdata);

	pdata = coresight_cti_get_platform_data(dev);
	if (IS_ERR(pdata)) {
		dev_err(dev, "coresight_cti_get_platform_data err\n");
		ret =  PTR_ERR(pdata);
		goto err_out;
	}
	/* default to powered - could change on PM notifications */
	drvdata->config.hw_powered = true;

	/* create dynamic attributes for connections */
	ret = cti_create_cons_sysfs(dev, drvdata);
	if (ret) {
		dev_err(dev, "%s: create dynamic sysfs entries failed\n",
			dev_name(dev));
		goto err_out;
	}
	/* set up coresight component description */
	cti_desc.pdata = pdata;
	cti_desc.type = CORESIGHT_DEV_TYPE_ECT;
	cti_desc.subtype.ect_subtype = CORESIGHT_DEV_SUBTYPE_ECT_CTI;
	cti_desc.ops = &cti_ops;
	cti_desc.groups = drvdata->ctidev.con_groups;
	cti_desc.dev = dev;
	drvdata->csdev = coresight_register(&cti_desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err_out;
	}

	/* add to list of CTI devices */
	mutex_lock(&ect_mutex);
	list_add(&drvdata->node, &ect_net);
	mutex_unlock(&ect_mutex);

	/* set up release chain */
	drvdata->csdev_release = drvdata->csdev->dev.release;
	drvdata->csdev->dev.release = cti_device_release;

	/* all done - dec pm refcount */
	pm_runtime_put(&adev->dev);
	dev_info(&drvdata->csdev->dev, "CTI initialized\n");
	return 0;

err_out:
	return ret;
}

static const struct amba_id cti_ids[] = {
        {       /* x9 soc600 */
                .id = 0x006bbd05,
                .mask = 0x00ffffff,
                .data = "CTI armv8",
        },
	{ 0, 0},
};

static struct amba_driver cti_driver = {
	.drv = {
		.name	= "coresight-cti",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe		= cti_probe,
	.id_table	= cti_ids,
};
builtin_amba_driver(cti_driver);
