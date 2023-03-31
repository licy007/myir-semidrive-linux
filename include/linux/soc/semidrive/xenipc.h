#ifndef __XEN_PUBLIC_IO_IPC_H__
#define __XEN_PUBLIC_IO_IPC_H__

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>

#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/soc/semidrive/ipcc.h>

/*
 * Enable to track contexts and buffers
 */
#define VMM_DEBUG 0

#if (VMM_DEBUG == 1)
    #define VMM_DEBUG_PRINT(fmt, ...) \
        pr_err(fmt, __VA_ARGS__)
#else
    #define VMM_DEBUG_PRINT(fmt, ...)
#endif
#define VMM_ERROR_PRINT(fmt, ...) \
	pr_err(fmt, __VA_ARGS__)

#define IPC_RPC_MAX_PARAMS     (8)
#define IPC_RPC_MAX_RESULT     (4)

/*
 * REQUEST CODES.
 */
#define GPUIF_OP_CREATE_PVZCONNECTION             0
#define GPUIF_OP_DESTROY_PVZCONNECTION            1
#define GPUIF_OP_CREATE_DEVPHYSHEAPS              2
#define GPUIF_OP_DESTROY_DEVPHYSHEAPS             3
#define GPUIF_OP_MAP_DEVPHYSHEAPS                 4
#define GPUIF_OP_UNMAP_DEVPHYSHEAPS               5
#define GPUIF_OP_DEBUG_DUMPCONFIG                 6

struct xengsx_page_directory {
	grant_ref_t gref_dir_next_page;
	grant_ref_t gref[1]; /* Variable length */
};

struct xenipc_request {
	u64 id;
	u32 cmd;
	u32 cksum;
	u32 param[IPC_RPC_MAX_PARAMS];
};

struct xenipc_respond {
	u64 id;
	u32 ack;
	u32 retcode;
	u32 result[IPC_RPC_MAX_RESULT];
};

struct xenipc_call_info {
	struct xenipc_request  *req;
	struct xenipc_respond *rsp;
	struct xenipc_call_info *next;
	struct completion call_completed;
};

DEFINE_RING_TYPES(xenipc, struct xenipc_request, struct xenipc_respond);

#define XENIPC_REQUEST_INIT(_r) \
	memset((void *)_r, 0, sizeof(struct xenipc_request))

#define GRANT_INVALID_REF	0

#endif /* __XEN_PUBLIC_IO_IPC_H__ */

