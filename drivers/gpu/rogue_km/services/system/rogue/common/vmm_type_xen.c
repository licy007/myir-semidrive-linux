/*
* vmm_type_xen.c
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System abstraction layer .
*
* Revision History:
* -----------------
* 01, 07/14/2019 Lili create this file
*/
#include "pvrsrv.h"
#include "img_types.h"
#include "pvrsrv_error.h"
#include "rgxheapconfig.h"

#include "vmm_impl.h"
#include "vmm_pvz_server.h"
#include "xengpu.h"
#include "xengpufront.h"
#include "sysconfig.h"
#include "vmm_type_xen.h"
#include "xengpu.h"

static PVRSRV_ERROR
XenVMMMapDevPhysHeap(IMG_UINT32 ui32FuncID,
					  IMG_UINT32 ui32DevID,
					  IMG_UINT64 ui64Size,
					  IMG_UINT64 ui64Addr)
{
	int ret;
	PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
#if defined(VMM_DEBUG)
	int i;
	struct device * pvrsrvdev = (struct device *)pConfig->pvOSDevice;
	const struct dma_map_ops *dma_ops;
	PHYS_HEAP_CONFIG * psPhysHeapConfig = &(pConfig->pasPhysHeaps[pConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL]]);
	DMA_ALLOC *psDmaAlloc = (DMA_ALLOC *)psPhysHeapConfig->pasRegions[0].hPrivData;
	IMG_UINT64 *start = psDmaAlloc->pvVirtAddr;
	memset(start, 0xAB,100);
	dma_ops = get_dma_ops(pvrsrvdev);
	dma_ops->sync_single_for_device(pvrsrvdev, ui64Addr, 100, DMA_BIDIRECTIONAL );
	dma_ops->sync_single_for_cpu(pvrsrvdev, ui64Addr, 100, DMA_BIDIRECTIONAL );
	VMM_DEBUG_PRINT("#########set 0x%x to virt 0x%p############", *start, start);
#endif

	ret = gsx_front_map(ui32FuncID, ui32DevID, pConfig->ui32OsId, ui64Size, ui64Addr);
	if (ret) {
		VMM_DEBUG_PRINT("%s:%d, ret=%d", __FUNCTION__, __LINE__, ret);
	}

#if defined(VMM_DEBUG)
	VMM_DEBUG_PRINT("#########return to guest begin############", *start, start);
	for (i = 0; i< 100; i++)
	{
		VMM_DEBUG_PRINT("---- 0x%x ----", start[i]);
	}
	VMM_DEBUG_PRINT("#########finish guest############", *start, start);
#endif

	return ret;
}

static PVRSRV_ERROR
XenVMMUnmapDevPhysHeap(IMG_UINT32 ui32FuncID,
						IMG_UINT32 ui32DevID)
{
	PVRSRV_ERROR ret = PVRSRV_OK;
	PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();

	ret = gsx_front_unmap(ui32FuncID, ui32DevID, pConfig->ui32OsId);
	if (ret) {
		VMM_DEBUG_PRINT("%s:%d, ret=%d", __FUNCTION__, __LINE__, ret);
	}

	return ret;
}

static VMM_PVZ_CONNECTION gsXenVmmPvz =
{
	.sClientFuncTab = {
		/* pfnMapDevPhysHeap */
		&XenVMMMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&XenVMMUnmapDevPhysHeap
	},

	.sServerFuncTab = {
		/* pfnMapDevPhysHeap */
		&PvzServerMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&PvzServerUnmapDevPhysHeap
	},

	.sVmmFuncTab = {
		/* pfnOnVmOnline */
		&PvzServerOnVmOnline,

		/* pfnOnVmOffline */
		&PvzServerOnVmOffline,

		/* pfnVMMConfigure */
		&PvzServerVMMConfigure
	}
};


/* this function is running on host */
void XenHostDestroyConnection(IMG_UINT32 uiOSID, IMG_UINT32 uiDevID)
{
	VMM_DEBUG_PRINT("enter %s, uiOSID(%d)",
			__FUNCTION__,
			uiOSID);

	if (PVRSRV_OK != gsXenVmmPvz.sVmmFuncTab.pfnOnVmOffline(uiOSID, uiDevID))
	{
		PVR_DPF((PVR_DBG_ERROR, "OnVmOffline failed for OSID%d", uiOSID));
	}
	else
	{
		PVR_LOG(("Guest OSID%d driver is offline", uiOSID));
	}

}

/* this function is running on host */
PVRSRV_ERROR XenHostCreateConnection(IMG_UINT32 uiOSID, IMG_UINT32 uiDevID)
{
	VMM_DEBUG_PRINT("enter %s, uiOSID(%d)",
			__FUNCTION__,
			uiOSID);

	if (PVRSRV_OK != gsXenVmmPvz.sVmmFuncTab.pfnOnVmOnline(uiOSID, uiDevID))
	{
		PVR_DPF((PVR_DBG_ERROR, "OnVmOnline failed for OSID%d", uiOSID));
	}
	else
	{
		PVR_LOG(("Guest OSID%d driver is online", uiOSID));
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR XenCreatePvzConnection(void)
{
	PVRSRV_ERROR ret = PVRSRV_OK;

	if (PVRSRV_VZ_MODE_IS(GUEST)) {
		struct gpuif_request request;
		struct gpuif_response *rsp = NULL;
		PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
		XENGPU_REQUEST_INIT(&request);

		ret = xengpu_init();
		if (ret != PVRSRV_OK) {
			pr_err("%s:%d,xengpu_init return 0x%x", __FUNCTION__, __LINE__, ret);
			return ret;
		}
		request.operation = GPUIF_OP_CREATE_PVZCONNECTION;
		request.ui32OsID = pConfig->ui32OsId;
		ret = xengpu_do_request(&request, &rsp);
		if (rsp != NULL) {
			ret = rsp->status;
			kfree(rsp);
		}
	} else {
		ret = xengpu_xenbus_init();
		if (ret != PVRSRV_OK) {
			pr_err("%s:%d,xengpu_xenbus_init return 0x%x", __FUNCTION__, __LINE__, ret);
			return ret;
		}
	}

	VMM_DEBUG_PRINT("%s:%d,XenCreatePvzConnection success", __FUNCTION__, __LINE__);
	return ret;
}

/* this function is running on guest */
static void XenDestroyPvzConnection(void)
{
	PVRSRV_ERROR ret = PVRSRV_OK;
	if (PVRSRV_VZ_MODE_IS(GUEST))
	{
		struct gpuif_request request;
		struct gpuif_response *rsp = NULL;
		PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
		XENGPU_REQUEST_INIT(&request);

		request.operation = GPUIF_OP_DESTROY_PVZCONNECTION;
		request.ui32OsID = pConfig->ui32OsId;
		ret = xengpu_do_request(&request, &rsp);
		if (rsp != NULL) {
			ret = rsp->status;
			kfree(rsp);
		}
		xengpu_exit();
	} else {
		xengpu_xenbus_fini();
	}
}


PVRSRV_ERROR VMMCreatePvzConnection(VMM_PVZ_CONNECTION **psPvzConnection)
{
	PVR_RETURN_IF_FALSE((NULL != psPvzConnection), PVRSRV_ERROR_INVALID_PARAMS);
	VMM_DEBUG_PRINT("enter %s", __FUNCTION__);
	*psPvzConnection = &gsXenVmmPvz;
	return XenCreatePvzConnection();
}

void VMMDestroyPvzConnection(VMM_PVZ_CONNECTION *psPvzConnection)
{
	PVR_LOG_IF_FALSE((NULL != psPvzConnection), "VMMDestroyPvzConnection");
	VMM_DEBUG_PRINT("enter %s", __FUNCTION__);
	return XenDestroyPvzConnection();
}

/******************************************************************************
 End of file (vmm_type_xen.c)
******************************************************************************/

