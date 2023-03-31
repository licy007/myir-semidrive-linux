/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <xen/xen.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

#ifdef IPCC_RPC_RPROC_MP
#define RPMSG_CP_STATUS_DEV			(0)
#define RPMSG_DC_STATUS_DEV			(2)
#else  /* !IPCC_RPC_RPROC_MP */
#define RPMSG_DC_STATUS_DEV			(0)
#endif /* end IPCC_RPC_RPROC_MP */

int rpmsg_rpc_call_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);
int xenipc_rpc_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);

int semidrive_rpcall(struct rpc_req_msg *req, struct rpc_ret_msg *result)
{
	int ret = 0;

	if (xen_domain() && !xen_initial_domain()) {
		/* TODO: XEN domain IPC call */
		ret = xenipc_rpc_trace(RPMSG_DC_STATUS_DEV, req, result);
	} else {
#ifdef IPCC_RPC_RPROC_MP
		if (MOD_RPC_REQ_CP_IOCTL == req->cmd) {
			ret = rpmsg_rpc_call_trace(RPMSG_CP_STATUS_DEV, req, result);
		} else {
#endif
			ret = rpmsg_rpc_call_trace(RPMSG_DC_STATUS_DEV, req, result);
#ifdef IPCC_RPC_RPROC_MP
		}
#endif
	}

	return ret;
}
EXPORT_SYMBOL(semidrive_rpcall);

int semidrive_set_property(u32 id, u32 val)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request;
	int ret = 0;

	DCF_INIT_RPC_REQ4(request, SYS_RPC_REQ_SET_PROPERTY, id, val, 0, 0);
	ret = semidrive_rpcall(&request, &result);

	return ret;
}
EXPORT_SYMBOL(semidrive_set_property);

int semidrive_get_property(u32 id, u32 *val)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request;
	int ret = 0;

	DCF_INIT_RPC_REQ1(request, SYS_RPC_REQ_GET_PROPERTY, id);
	ret = semidrive_rpcall(&request, &result);
	if (!ret && val) {
		*val = result.result[0];
	}

	return ret;
}
EXPORT_SYMBOL(semidrive_get_property);

int semidrive_send(void *ept, void *data, int len)
{
	return 0;
}
EXPORT_SYMBOL(semidrive_send);

int semidrive_trysend(void *ept, void *data, int len)
{
	return 0;
}
EXPORT_SYMBOL(semidrive_trysend);

int semidrive_poll(void *ept, struct file *filp, poll_table *wait)
{
	return 0;
}
EXPORT_SYMBOL(semidrive_poll);

MODULE_AUTHOR("ye.liu@semidrive.com");
MODULE_ALIAS("rpmsg:rpcall");
MODULE_DESCRIPTION("Semidrive RPC kernel helper");
MODULE_LICENSE("GPL v2");

