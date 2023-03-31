/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

#define CONFIG_DC_NEGOTIABLE		(1)

#if CONFIG_DC_NEGOTIABLE

#define SYS_PROP_DC_STATUS			(4)

static int rpc_get_dc_status(dc_state_t *val)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request;
	int ret = 0;

	DCF_INIT_RPC_REQ1(request, SYS_RPC_REQ_GET_PROPERTY, SYS_PROP_DC_STATUS);
	ret = semidrive_rpcall(&request, &result);
	if (!ret && val) {
		*val = result.result[0];
	}

	return ret;
}

static int rpc_set_dc_status(dc_state_t val, bool block)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request;
	int ret = 0;

	DCF_INIT_RPC_REQ4(request, SYS_RPC_REQ_SET_PROPERTY, SYS_PROP_DC_STATUS,
						val, block, 0);
	ret = semidrive_rpcall(&request, &result);

	return ret;
}

int sd_get_dc_status(dc_state_t *val)
{
	return rpc_get_dc_status(val);
}
EXPORT_SYMBOL(sd_get_dc_status);

int sd_set_dc_status(dc_state_t val)
{
	return rpc_set_dc_status(val, true);
}
EXPORT_SYMBOL(sd_set_dc_status);

bool sd_is_dc_inited(void)
{
	dc_state_t status;
	int ret;

	ret = sd_get_dc_status(&status);
	if (ret < 0) {
		pr_warn("%s: Remote device not exist\n", __func__);
		return false;
	}

	return (status != DC_STAT_NOTINIT);
}
EXPORT_SYMBOL(sd_is_dc_inited);

int sd_wait_dc_init(unsigned int timeout_ms)
{
	dc_state_t status;
	ktime_t start;
	int ret;

	if (!sd_is_dc_inited()) {
		pr_err("%s: dc is not initialized by otherend\n", __func__);
		return -ENODEV;
	}

	pr_info("%s: waiting dc, now status is %d\n", __func__, status);
	start = ktime_get_boottime();
	do {
		ret = sd_get_dc_status(&status);
		if (ret < 0) {
			pr_warn("%s: Remote device not exist\n", __func__);
			break;
		}

		if (ktime_us_delta(ktime_get_boottime(), start) >= timeout_ms) {
			ret = -ETIME;
			break;
		}
	} while(status != DC_STAT_INITED);

	return ret;
}
EXPORT_SYMBOL(sd_wait_dc_init);

bool sd_is_dc_closed(void)
{
	dc_state_t status;
	int ret;

	ret = sd_get_dc_status(&status);
	if (ret < 0) {
		return true;
	}
	pr_info("%s: status is %d\n", __func__, status);

	return (status == DC_STAT_CLOSED);
}
EXPORT_SYMBOL(sd_is_dc_closed);

int sd_close_dc(bool is_block)
{
	dc_state_t status;
	int ret;

	ret = sd_get_dc_status(&status);
	if (ret < 0) {
		pr_warn("%s: Remote device not exist\n", __func__);
		return -ENODEV;
	}
	pr_warn("%s: cleaning dc, status is %d\n", __func__, status);

	if (status != DC_STAT_CLOSED)
		return sd_set_dc_status(DC_STAT_CLOSING);

	return 0;
}
EXPORT_SYMBOL(sd_close_dc);
#endif	//CONFIG_DC_NEGOTIABLE


