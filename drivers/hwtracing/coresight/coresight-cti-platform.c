// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, The Linaro Limited. All rights reserved.
 */

#include <linux/of.h>
#include "coresight-cti.h"

#define GEN_IO		0
#define GEN_INTREQ	1
#define GEN_INTACK	2
#define GEN_HALTREQ	3
#define GEN_RESTARTREQ	4
#define PE_EDBGREQ	5
#define PE_DBGRESTART	6
#define PE_CTIIRQ	7
#define PE_PMUIRQ	8
#define PE_DBGTRIGGER	9
#define ETM_EXTOUT	10
#define ETM_EXTIN	11
#define SNK_FULL	12
#define SNK_ACQCOMP	13
#define SNK_FLUSHCOMP	14
#define SNK_FLUSHIN	15
#define SNK_TRIGIN	16
#define STM_ASYNCOUT	17
#define STM_TOUT_SPTE	18
#define STM_TOUT_SW	19
#define STM_TOUT_HETE	20
#define STM_HWEVENT	21
#define ELA_TSTART	22
#define ELA_TSTOP	23
#define ELA_DBGREQ	24
#define CTI_TRIG_MAX	25

/* Number of CTI signals in the v8 architecturally defined connection */
#define NR_V8PE_IN_SIGS		2
#define NR_V8PE_OUT_SIGS	3
#define NR_V8ETM_INOUT_SIGS	4

/* CTI device tree trigger connection node keyword */
#define CTI_DT_CONNS		"trig-conns"

/* CTI device tree connection property keywords */
#define CTI_DT_V8ARCH_COMPAT	"arm,coresight-cti-v8-arch"
#define CTI_DT_CSDEV_ASSOC	"arm,cs-dev-assoc"
#define CTI_DT_TRIGIN_SIGS	"arm,trig-in-sigs"
#define CTI_DT_TRIGOUT_SIGS	"arm,trig-out-sigs"
#define CTI_DT_TRIGIN_TYPES	"arm,trig-in-types"
#define CTI_DT_TRIGOUT_TYPES	"arm,trig-out-types"
#define CTI_DT_FILTER_OUT_SIGS	"arm,trig-filters"
#define CTI_DT_CONN_NAME	"arm,trig-conn-name"
#define CTI_DT_CTM_ID		"arm,cti-ctm-id"

/*
 * Create an architecturally defined v8 connection
 * must have a cpu, can have an ETM.
 */
static int cti_plat_create_v8_connections(struct device *dev,
					  struct cti_drvdata *drvdata)
{
	struct cti_device *cti_dev = &drvdata->ctidev;
	struct cti_trig_con *tc = NULL;
	int cpuid = 0;
	char cpu_name_str[16];
	int ret = -ENOMEM;
	/* Must have a cpu node */
	struct device_node *np = dev->of_node;
	cpuid = of_coresight_get_cpu(np);
	if (cpuid < 0) {
		dev_err(dev,
			 "ARM v8 architectural CTI connection: missing cpu\n");
		return -EINVAL;
	}
	cti_dev->cpu = cpuid;

	/* Allocate the v8 cpu connection memory */
	tc = cti_allocate_trig_con(dev, NR_V8PE_IN_SIGS, NR_V8PE_OUT_SIGS);
	if (!tc)
		goto of_create_v8_out;

	/* Set the v8 PE CTI connection data */
	tc->con_in->used_mask = 0x3; /* sigs <0 1> */
	tc->con_in->sig_types[0] = PE_DBGTRIGGER;
	tc->con_in->sig_types[1] = PE_PMUIRQ;
	tc->con_out->used_mask = 0x7; /* sigs <0 1 2 > */
	tc->con_out->sig_types[0] = PE_EDBGREQ;
	tc->con_out->sig_types[1] = PE_DBGRESTART;
	tc->con_out->sig_types[2] = PE_CTIIRQ;
	scnprintf(cpu_name_str, sizeof(cpu_name_str), "cpu%d", cpuid);

	ret = cti_add_connection_entry(dev, drvdata, tc, NULL, cpu_name_str);
	if (ret)
		goto of_create_v8_out;

	/* etm extout skipped, not used */

	/* filter pe_edbgreq - PE trigout sig <0> */
	drvdata->config.trig_out_filter |= 0x1;

of_create_v8_out:
	return ret;
}

/* get the hardware configuration & connection data. */
int cti_plat_get_hw_data(struct device *dev,
			 struct cti_drvdata *drvdata)
{
	int rc = 0;
	struct cti_device *cti_dev = &drvdata->ctidev;

	/* get any CTM ID - defaults to 0 */
	device_property_read_u32(dev, CTI_DT_CTM_ID, &cti_dev->ctm_id);

	/* only need support v8 architectural CTI device */
	rc = cti_plat_create_v8_connections(dev, drvdata);
	if (rc)
		return rc;

	/* if no connections, just add a single default based on max IN-OUT */
	if (cti_dev->nr_trig_con == 0)
		rc = cti_add_default_connection(dev, drvdata);
	return rc;
}

struct coresight_platform_data *
coresight_cti_get_platform_data(struct device *dev)
{
	int ret = -ENOENT;
	struct coresight_platform_data *pdata = NULL;
	struct cti_drvdata *drvdata = dev_get_drvdata(dev);
	/*
	 * Alloc platform data but leave it zero init. CTI does not use the
	 * same connection infrastructuree as trace path components but an
	 * empty struct enables us to use the standard coresight component
	 * registration code.
	 */
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto error;
	}

	/* Use device name as sysfs handle */
	pdata->name = dev_name(dev);

	/* get some CTI specifics */
	ret = cti_plat_get_hw_data(dev, drvdata);
	if (!ret)
		return pdata;
error:
	return ERR_PTR(ret);
}
