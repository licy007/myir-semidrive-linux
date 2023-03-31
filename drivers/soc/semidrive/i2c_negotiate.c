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

#define CONFIG_I2C_NEGOTIABLE		(1)

#if CONFIG_I2C_NEGOTIABLE

#define SYS_PROP_I2C_STATUS			(7)

static int rpc_get_i2c_status(i2c_state_t *val)
{
	struct rpc_ret_msg result;
	struct rpc_req_msg request;
	int ret = 0;

	DCF_INIT_RPC_REQ1(request, SYS_RPC_REQ_GET_PROPERTY, SYS_PROP_I2C_STATUS);
	ret = semidrive_rpcall(&request, &result);
	if (!ret && val) {
		*val = result.result[0];
	}

	return ret;
}

static int rpc_set_i2c_status(i2c_state_t val, bool block)
{
	struct rpc_ret_msg result;
	struct rpc_req_msg request;
	int ret = 0;

	DCF_INIT_RPC_REQ4(request, SYS_RPC_REQ_SET_PROPERTY, SYS_PROP_I2C_STATUS,
						val, block, 0);
	ret = semidrive_rpcall(&request, &result);

	return ret;
}

int sd_get_i2c_status(i2c_state_t *val)
{
	return rpc_get_i2c_status(val);
}
EXPORT_SYMBOL(sd_get_i2c_status);

int sd_set_i2c_status(i2c_state_t val)
{
	return rpc_set_i2c_status(val, true);
}
EXPORT_SYMBOL(sd_set_i2c_status);

#endif	//CONFIG_I2C_NEGOTIABLE


