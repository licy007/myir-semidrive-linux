/**************************************************************************/ /*!
@File
@Title          Server side Display Class functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side functions of the Display Class
                interface.
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */ /***************************************************************************/

#include "allocmem.h"
#include "lock.h"
#include "osfunc.h"
#include "img_types.h"
#include "img_defs.h"
#include "scp.h"
#include "dc_server.h"
#include "kerneldisplay.h"
#include "pvr_debug.h"
#include "pvr_notifier.h"
#include "pmr.h"
#include "sync_server.h"
#include "pvrsrv.h"
#include "process_stats.h"
#include "cache_km.h"
#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
#include "ri_server.h"
#endif

struct _DC_DISPLAY_CONTEXT_
{
	DC_DEVICE		*psDevice;
	SCP_CONTEXT		*psSCPContext;
	IMG_HANDLE		hDisplayContext;
	IMG_UINT32		ui32ConfigsInFlight;
	ATOMIC_T		sRefCount;
	POS_LOCK		hLock;
	POS_LOCK		hConfigureLock;			// Guard against concurrent calls to pfnContextConfigure during DisplayContextFlush
	IMG_UINT32		ui32TokenOut;
	IMG_UINT32		ui32TokenIn;

	IMG_HANDLE		hCmdCompNotify;

	IMG_BOOL		bIssuedNullFlip;
	IMG_HANDLE		hMISR;
	IMG_HANDLE		hDebugNotify;
	void			*hTimer;

	IMG_BOOL		bPauseMISR;
	DLLIST_NODE		sListNode;
};

struct _DC_DEVICE_
{
	PVRSRV_DEVICE_NODE			*psDevNode;
	const DC_DEVICE_FUNCTIONS	*psFuncTable;
	IMG_UINT32					ui32MaxConfigsInFlight;
	IMG_HANDLE					hDeviceData;
	ATOMIC_T					sRefCount;
	IMG_UINT32					ui32Index;
	IMG_HANDLE					psEventList;
	IMG_HANDLE					hSystemBuffer;
	PMR							*psSystemBufferPMR;
	PHYS_HEAP					*psPhysHeap;
	DC_DISPLAY_CONTEXT			sSystemContext;
	DLLIST_NODE                 sListNode;   /* List of devices */
};

typedef enum _DC_BUFFER_TYPE_
{
	DC_BUFFER_TYPE_UNKNOWN = 0,
	DC_BUFFER_TYPE_ALLOC,
	DC_BUFFER_TYPE_IMPORT,
	DC_BUFFER_TYPE_SYSTEM,
} DC_BUFFER_TYPE;

typedef struct _DC_BUFFER_ALLOC_DATA_
{
	PMR	*psPMR;
} DC_BUFFER_ALLOC_DATA;

typedef struct _DC_BUFFER_IMPORT_DATA_
{
	/*
	Required as the DC doesn't need to map the PMR during the import call we
	need to make sure that the PMR doesn't get freed before the DC maps it
	by taking an ref on the PMR during the import and drop it on the unimport.
	 */
	IMG_UINT32	ui32NumPlanes;
	PMR			*apsImport[3];
} DC_BUFFER_IMPORT_DATA;

struct _DC_BUFFER_
{
	DC_DISPLAY_CONTEXT	*psDisplayContext;
	DC_BUFFER_TYPE		eType;
	union {
		DC_BUFFER_ALLOC_DATA	sAllocData;
		DC_BUFFER_IMPORT_DATA	sImportData;
	} uBufferData;
	IMG_HANDLE			hBuffer;
	IMG_UINT32			ui32MapCount;
	ATOMIC_T			sRefCount;
	POS_LOCK			hMapLock;
};

typedef struct _DC_CMD_RDY_DATA_
{
	DC_DISPLAY_CONTEXT			*psDisplayContext;
	IMG_UINT32					ui32BufferCount;
	PVRSRV_SURFACE_CONFIG_INFO	*pasSurfAttrib;
	IMG_HANDLE					*pahBuffer;
	IMG_UINT32					ui32DisplayPeriod;
} DC_CMD_RDY_DATA;

typedef struct _DC_CMD_COMP_DATA_
{
	DC_DISPLAY_CONTEXT	*psDisplayContext;
	IMG_UINT32			ui32BufferCount;
	DC_BUFFER			**apsBuffer;
	IMG_UINT32			ui32Token;
	IMG_BOOL			bDirectNullFlip;
} DC_CMD_COMP_DATA;

typedef struct _DC_BUFFER_PMR_DATA_
{
	DC_BUFFER				*psBuffer;			/*!< The buffer this PMR private data refers to */
	IMG_DEVMEM_LOG2ALIGN_T	uiLog2PageSize;		/*!< Log 2 of the buffers pagesize */
	IMG_UINT32				ui32PageCount;		/*!< Number of pages in this buffer */
	IMG_DEV_PHYADDR			*pasDevPAddr;		/*!< Pointer to an array of device physical addresses */
	void					*pvLinAddr;			/*!< CPU virtual pointer or NULL if the DC driver didn't have one */
} DC_BUFFER_PMR_DATA;

static POS_LOCK g_hDCListLock;
static POS_LOCK g_hDCDevListLock;				/*!< Mutex for device list access */

static DLLIST_NODE g_sDCDeviceListHead;			/*!< List of DC devices found */
static IMG_UINT32 g_ui32DCDeviceCount;
static IMG_UINT32 g_ui32DCNextIndex;
static POS_LOCK g_hDCDisplayContextsListLock;
static DLLIST_NODE g_sDisplayContextsListHead;


#if defined(DC_DEBUG) && defined(REFCOUNT_DEBUG)
#define DC_REFCOUNT_PRINT(fmt, ...)		\
		PVRSRVDebugPrintf(PVR_DBG_WARNING,	\
		                  __FILE__,		\
		                  __LINE__,		\
		                  fmt,			\
		                  __VA_ARGS__)
#else
#define DC_REFCOUNT_PRINT(fmt, ...)
#endif

#if defined(DC_DEBUG)
#define DC_DEBUG_PRINT(fmt, ...)			\
		PVRSRVDebugPrintf(PVR_DBG_WARNING,		\
		                  __FILE__,			\
		                  __LINE__,			\
		                  fmt,				\
		                  __VA_ARGS__)
#else
#define DC_DEBUG_PRINT(fmt, ...)
#endif

/*****************************************************************************
 *                             Private functions                             *
 *****************************************************************************/

static void _DCDeviceAcquireRef(DC_DEVICE *psDevice)
{
	IMG_INT iRefCount = OSAtomicIncrement(&psDevice->sRefCount);

	DC_REFCOUNT_PRINT("%s: DC device %p, refcount = %d", __func__, psDevice,
	                  iRefCount);
	PVR_UNREFERENCED_PARAMETER(iRefCount);
}

static void _DCDeviceReleaseRef(DC_DEVICE *psDevice)
{
	IMG_UINT iRefCount = OSAtomicDecrement(&psDevice->sRefCount);
	if (iRefCount == 0)
	{
		OSLockAcquire(g_hDCDevListLock);
		dllist_remove_node(&psDevice->sListNode);
		OSLockRelease(g_hDCDevListLock);

		OSLockAcquire(g_hDCListLock);
		g_ui32DCDeviceCount--;
		OSLockRelease(g_hDCListLock);
	}
	else
	{
		/* Signal this devices event list as the unload might be blocked on it */
		OSEventObjectSignal(psDevice->psEventList);
	}

	DC_REFCOUNT_PRINT("%s: DC device %p, refcount = %d", __func__, psDevice,
	                  iRefCount);
}

static void _DCDisplayContextAcquireRef(DC_DISPLAY_CONTEXT *psDisplayContext)
{
	IMG_INT iRefCount = OSAtomicIncrement(&psDisplayContext->sRefCount);

	DC_REFCOUNT_PRINT("%s: DC display context %p, refcount = %d", __func__,
	                  psDisplayContext, iRefCount);
	PVR_UNREFERENCED_PARAMETER(iRefCount);
}

static void _DCDisplayContextReleaseRef(DC_DISPLAY_CONTEXT *psDisplayContext)
{
	IMG_INT iRefCount = OSAtomicDecrement(&psDisplayContext->sRefCount);
	if (iRefCount == 0)
	{
		DC_DEVICE *psDevice = psDisplayContext->psDevice;

		PVRSRVUnregisterDeviceDbgRequestNotify(psDisplayContext->hDebugNotify);

		OSLockAcquire(g_hDCDisplayContextsListLock);
		dllist_remove_node(&psDisplayContext->sListNode);
		OSLockRelease(g_hDCDisplayContextsListLock);

		/* unregister the device from cmd complete notifications */
		PVRSRVUnregisterCmdCompleteNotify(psDisplayContext->hCmdCompNotify);
		psDisplayContext->hCmdCompNotify = NULL;

		OSUninstallMISR(psDisplayContext->hMISR);
		SCPDestroy(psDisplayContext->psSCPContext);
		psDevice->psFuncTable->pfnContextDestroy(psDisplayContext->hDisplayContext);
		_DCDeviceReleaseRef(psDevice);
		OSLockDestroy(psDisplayContext->hConfigureLock);
		OSLockDestroy(psDisplayContext->hLock);
		OSFreeMem(psDisplayContext);
	}

	DC_REFCOUNT_PRINT("%s: DC display context %p, refcount = %d", __func__,
	                  psDisplayContext, iRefCount);
}

static void _DCBufferAcquireRef(DC_BUFFER *psBuffer)
{
	IMG_INT iRefCount = OSAtomicIncrement(&psBuffer->sRefCount);

	DC_REFCOUNT_PRINT("%s: DC buffer %p, refcount = %d", __func__, psBuffer,
	                  iRefCount);
	PVR_UNREFERENCED_PARAMETER(iRefCount);
}


static void _DCFreeAllocedBuffer(DC_BUFFER *psBuffer)
{
	DC_DISPLAY_CONTEXT *psDisplayContext = psBuffer->psDisplayContext;
	DC_DEVICE *psDevice = psDisplayContext->psDevice;

	psDevice->psFuncTable->pfnBufferFree(psBuffer->hBuffer);
	_DCDisplayContextReleaseRef(psDisplayContext);
}

static void _DCFreeImportedBuffer(DC_BUFFER *psBuffer)
{
	DC_DISPLAY_CONTEXT *psDisplayContext = psBuffer->psDisplayContext;
	DC_DEVICE *psDevice = psDisplayContext->psDevice;
	IMG_UINT32 i;

	for (i=0;i<psBuffer->uBufferData.sImportData.ui32NumPlanes;i++)
	{
		PMRUnrefPMR(psBuffer->uBufferData.sImportData.apsImport[i]);
	}
	psDevice->psFuncTable->pfnBufferFree(psBuffer->hBuffer);
	_DCDisplayContextReleaseRef(psDisplayContext);
}

static void _DCFreeSystemBuffer(DC_BUFFER *psBuffer)
{
	DC_DISPLAY_CONTEXT *psDisplayContext = psBuffer->psDisplayContext;
	DC_DEVICE *psDevice = psDisplayContext->psDevice;

	psDevice->psFuncTable->pfnBufferSystemRelease(psBuffer->hBuffer);
	_DCDeviceReleaseRef(psDevice);
}

/*
	Drop a reference on the buffer. Last person gets to free it
 */
static void _DCBufferReleaseRef(DC_BUFFER *psBuffer)
{
	IMG_INT iRefCount = OSAtomicDecrement(&psBuffer->sRefCount);
	if (iRefCount == 0)
	{
		switch (psBuffer->eType)
		{
			case DC_BUFFER_TYPE_ALLOC:
				_DCFreeAllocedBuffer(psBuffer);
				break;
			case DC_BUFFER_TYPE_IMPORT:
				_DCFreeImportedBuffer(psBuffer);
				break;
			case DC_BUFFER_TYPE_SYSTEM:
				_DCFreeSystemBuffer(psBuffer);
				break;
			default:
				PVR_ASSERT(IMG_FALSE);
		}
		OSLockDestroy(psBuffer->hMapLock);
		OSFreeMem(psBuffer);
	}

	DC_REFCOUNT_PRINT("%s: DC buffer %p, refcount = %d", __func__, psBuffer,
	                  iRefCount);
	PVR_UNREFERENCED_PARAMETER(iRefCount);
}

static PVRSRV_ERROR _DCBufferMap(DC_BUFFER *psBuffer)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	OSLockAcquire(psBuffer->hMapLock);
	if (psBuffer->ui32MapCount++ == 0)
	{
		DC_DEVICE *psDevice = psBuffer->psDisplayContext->psDevice;

		if (psDevice->psFuncTable->pfnBufferMap)
		{
			eError = psDevice->psFuncTable->pfnBufferMap(psBuffer->hBuffer);
			PVR_GOTO_IF_ERROR(eError, out_unlock);
		}

		_DCBufferAcquireRef(psBuffer);
	}

	DC_REFCOUNT_PRINT("%s: DC buffer %p, MapCount = %d",
	                  __func__, psBuffer, psBuffer->ui32MapCount);

out_unlock:
	OSLockRelease(psBuffer->hMapLock);
	return eError;
}

static void _DCBufferUnmap(DC_BUFFER *psBuffer)
{
	DC_DEVICE *psDevice = psBuffer->psDisplayContext->psDevice;
	IMG_UINT32 ui32MapCount;

	OSLockAcquire(psBuffer->hMapLock);
	ui32MapCount = --psBuffer->ui32MapCount;
	OSLockRelease(psBuffer->hMapLock);

	if (ui32MapCount == 0)
	{
		if (psDevice->psFuncTable->pfnBufferUnmap)
		{
			psDevice->psFuncTable->pfnBufferUnmap(psBuffer->hBuffer);
		}

		_DCBufferReleaseRef(psBuffer);
	}
	DC_REFCOUNT_PRINT("%s: DC Buffer %p, MapCount = %d",
	                  __func__, psBuffer, ui32MapCount);
}

static PVRSRV_ERROR _DCDeviceBufferArrayCreate(IMG_UINT32 ui32BufferCount,
                                               DC_BUFFER **papsBuffers,
                                               IMG_HANDLE **pahDeviceBuffers)
{
	IMG_HANDLE *ahDeviceBuffers;
	IMG_UINT32 i;

	/* Create an array of the DC's private Buffer handles */
	ahDeviceBuffers = OSAllocZMem(sizeof(IMG_HANDLE) * ui32BufferCount);
	PVR_RETURN_IF_NOMEM(ahDeviceBuffers);

	for (i=0;i<ui32BufferCount;i++)
	{
		ahDeviceBuffers[i] = papsBuffers[i]->hBuffer;
	}

	*pahDeviceBuffers = ahDeviceBuffers;

	return PVRSRV_OK;
}

static void _DCDeviceBufferArrayDestroy(IMG_HANDLE ahDeviceBuffers)
{
	OSFreeMem(ahDeviceBuffers);
}

static IMG_BOOL _DCDisplayContextReady(void *hReadyData)
{
	DC_CMD_RDY_DATA *psReadyData = (DC_CMD_RDY_DATA *) hReadyData;
	DC_DISPLAY_CONTEXT *psDisplayContext = psReadyData->psDisplayContext;
	DC_DEVICE *psDevice = psDisplayContext->psDevice;

	if (psDisplayContext->ui32ConfigsInFlight >= psDevice->ui32MaxConfigsInFlight)
	{
		/*
			We're at the DC's max commands in-flight so don't take this command
			off the queue
		 */
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

#if defined(SUPPORT_DC_COMPLETE_TIMEOUT_DEBUG)
static void _RetireTimeout(void *pvData)
{
	DC_CMD_COMP_DATA *psCompleteData = pvData;
	DC_DISPLAY_CONTEXT *psDisplayContext = psCompleteData->psDisplayContext;

	PVR_DPF((PVR_DBG_ERROR, "Timeout fired for operation %d", psCompleteData->ui32Token));
	SCPDumpStatus(psDisplayContext->psSCPContext, NULL);

	OSDisableTimer(psDisplayContext->hTimer);
	OSRemoveTimer(psDisplayContext->hTimer);
	psDisplayContext->hTimer = NULL;
}
#endif	/* SUPPORT_DC_COMPLETE_TIMEOUT_DEBUG */

static void _DCDisplayContextConfigure(void *hReadyData,
                                       void *hCompleteData)
{
	DC_CMD_RDY_DATA *psReadyData = (DC_CMD_RDY_DATA *) hReadyData;
	DC_DISPLAY_CONTEXT *psDisplayContext = psReadyData->psDisplayContext;
	DC_DEVICE *psDevice = psDisplayContext->psDevice;

	OSLockAcquire(psDisplayContext->hLock);
	psDisplayContext->ui32ConfigsInFlight++;

#if defined(SUPPORT_DC_COMPLETE_TIMEOUT_DEBUG)
	if (psDisplayContext->ui32ConfigsInFlight == psDevice->ui32MaxConfigsInFlight)
	{
		/*
			We've just sent out a new config which has filled the DC's pipeline.
			This means that we expect a retire within a VSync period, start
			a timer that will print out a message if we haven't got a complete
			within a reasonable period (200ms)
		 */
		PVR_ASSERT(psDisplayContext->hTimer == NULL);
		psDisplayContext->hTimer = OSAddTimer(_RetireTimeout, hCompleteData, 200);
		OSEnableTimer(psDisplayContext->hTimer);
	}
#endif

	OSLockRelease(psDisplayContext->hLock);

#if defined(DC_DEBUG)
	{
		DC_DEBUG_PRINT("_DCDisplayContextConfigure: Send command (%d) out",
		               ((DC_CMD_COMP_DATA*) hCompleteData)->ui32Token);
	}
#endif /* DC_DEBUG */

	/*
	 * Note: A risk exists that _DCDisplayContextConfigure may be called simultaneously
	 *       from both SCPRun (MISR context) and DCDisplayContextFlush.
	 *       This lock ensures no concurrent calls are made to pfnContextConfigure.
	 */
	OSLockAcquire(psDisplayContext->hConfigureLock);
	/*
		Note: We've already done all the acquire refs at
		      DCDisplayContextConfigure time.
	 */
	psDevice->psFuncTable->pfnContextConfigure(psDisplayContext->hDisplayContext,
	                                           psReadyData->ui32BufferCount,
	                                           psReadyData->pasSurfAttrib,
	                                           psReadyData->pahBuffer,
	                                           psReadyData->ui32DisplayPeriod,
	                                           hCompleteData);
	OSLockRelease(psDisplayContext->hConfigureLock);

}

/*
	_DCDisplayContextRun

	Kick the MISR which will check for any commands which can be processed
 */
static INLINE void _DCDisplayContextRun(DC_DISPLAY_CONTEXT *psDisplayContext)
{
	OSScheduleMISR(psDisplayContext->hMISR);
}

/*
	DC_MISRHandler_DisplaySCP

	This gets called when this MISR is fired
 */
static void DC_MISRHandler_DisplaySCP(void *pvData)
{
	DC_DISPLAY_CONTEXT *psDisplayContext = pvData;

	if ( !psDisplayContext->bPauseMISR )
	{
		SCPRun(psDisplayContext->psSCPContext);
	}
}

/*
 * PMR related functions and structures
 */

/*
	Callback function for locking the system physical page addresses.
	As we acquire the display memory at PMR create time there is nothing
	to do here.
 */
static PVRSRV_ERROR _DCPMRLockPhysAddresses(PMR_IMPL_PRIVDATA pvPriv)
{
	DC_BUFFER_PMR_DATA *psPMRPriv = pvPriv;
	DC_BUFFER *psBuffer = psPMRPriv->psBuffer;
	DC_DEVICE *psDevice = psBuffer->psDisplayContext->psDevice;
	PVRSRV_ERROR eError;

	psPMRPriv->pasDevPAddr = OSAllocZMem(sizeof(IMG_DEV_PHYADDR) *
	                                     psPMRPriv->ui32PageCount);
	PVR_GOTO_IF_NOMEM(psPMRPriv->pasDevPAddr, eError, fail_alloc);

	eError = psDevice->psFuncTable->pfnBufferAcquire(psBuffer->hBuffer,
	                                                 psPMRPriv->pasDevPAddr,
	                                                 &psPMRPriv->pvLinAddr);
	PVR_GOTO_IF_ERROR(eError, fail_query);

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	{
		IMG_UINT32 i;
		for (i = 0; i < psPMRPriv->ui32PageCount; i++)
		{
			IMG_CPU_PHYADDR sCPUPhysAddr;
			PVRSRV_MEM_ALLOC_TYPE eAllocType;

			if (PhysHeapGetType(psDevice->psPhysHeap) == PHYS_HEAP_TYPE_LMA) {
				eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_LMA_PAGES;
			} else {
				eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES;
			}

			sCPUPhysAddr.uiAddr = ((uintptr_t)psPMRPriv->pvLinAddr) + i * (1 << psPMRPriv->uiLog2PageSize);
			PVRSRVStatsAddMemAllocRecord(eAllocType,
			                             NULL,
			                             sCPUPhysAddr,
			                             1 << psPMRPriv->uiLog2PageSize,
			                             OSGetCurrentClientProcessIDKM()
			                             DEBUG_MEMSTATS_VALUES);
		}
	}
#else
	{
		PVRSRV_MEM_ALLOC_TYPE eAllocType;

		if (PhysHeapGetType(psDevice->psPhysHeap) == PHYS_HEAP_TYPE_LMA) {
			eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_LMA_PAGES;
		} else {
			eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES;
		}

		PVRSRVStatsIncrMemAllocStat(eAllocType,
		                            psPMRPriv->ui32PageCount * (1 << psPMRPriv->uiLog2PageSize),
		                            OSGetCurrentClientProcessIDKM());
	}
#endif
#endif

	return PVRSRV_OK;

fail_query:
	OSFreeMem(psPMRPriv->pasDevPAddr);
fail_alloc:
	return eError;
}

static PVRSRV_ERROR _DCPMRUnlockPhysAddresses(PMR_IMPL_PRIVDATA pvPriv)
{
	DC_BUFFER_PMR_DATA *psPMRPriv = pvPriv;
	DC_BUFFER *psBuffer = psPMRPriv->psBuffer;
	DC_DEVICE *psDevice = psBuffer->psDisplayContext->psDevice;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
	{
		PVRSRV_MEM_ALLOC_TYPE eAllocType;

		if (PhysHeapGetType(psDevice->psPhysHeap) == PHYS_HEAP_TYPE_LMA) {
			eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_LMA_PAGES;
		} else {
			eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES;
		}

		PVRSRVStatsDecrMemAllocStat(eAllocType,
		                            psPMRPriv->ui32PageCount * (1 << psPMRPriv->uiLog2PageSize),
		                            OSGetCurrentClientProcessIDKM());
	}
#else
	{
		PVRSRV_MEM_ALLOC_TYPE eAllocType;
		IMG_UINT32 i;

		if (PhysHeapGetType(psDevice->psPhysHeap) == PHYS_HEAP_TYPE_LMA) {
			eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_LMA_PAGES;
		} else {
			eAllocType = PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES;
		}

		for (i = 0; i < psPMRPriv->ui32PageCount; i++)
		{
			IMG_CPU_PHYADDR sCPUPhysAddr;

			sCPUPhysAddr.uiAddr = ((uintptr_t)psPMRPriv->pvLinAddr) + i * (1 << psPMRPriv->uiLog2PageSize);
			PVRSRVStatsRemoveMemAllocRecord(eAllocType,
			                                sCPUPhysAddr.uiAddr,
			                                OSGetCurrentClientProcessIDKM());
		}
	}
#endif
#endif

	psDevice->psFuncTable->pfnBufferRelease(psBuffer->hBuffer);
	OSFreeMem(psPMRPriv->pasDevPAddr);

	return PVRSRV_OK;
}

static PVRSRV_ERROR _DCPMRDevPhysAddr(PMR_IMPL_PRIVDATA pvPriv,
                                      IMG_UINT32 ui32Log2PageSize,
                                      IMG_UINT32 ui32NumOfPages,
                                      IMG_DEVMEM_OFFSET_T *puiOffset,
                                      IMG_BOOL *pbValid,
                                      IMG_DEV_PHYADDR *psDevAddrPtr)
{
	DC_BUFFER_PMR_DATA *psPMRPriv = pvPriv;
	IMG_UINT32 uiPageIndex;
	IMG_UINT32 uiInPageOffset;
	IMG_DEV_PHYADDR sDevAddr;
	IMG_UINT32 idx;

	PVR_RETURN_IF_INVALID_PARAM(psPMRPriv->uiLog2PageSize == ui32Log2PageSize);

	for (idx=0; idx < ui32NumOfPages; idx++)
	{
		if (pbValid[idx])
		{
			/* verify the cast
			   N.B.  Strictly... this could be triggered by an illegal uiOffset arg too. */
			uiPageIndex = (IMG_UINT32)(puiOffset[idx] >> ui32Log2PageSize);
			PVR_ASSERT((IMG_DEVMEM_OFFSET_T)uiPageIndex << ui32Log2PageSize == puiOffset[idx]);

			uiInPageOffset = (IMG_UINT32)(puiOffset[idx] - ((IMG_DEVMEM_OFFSET_T)uiPageIndex << ui32Log2PageSize));
			PVR_ASSERT(puiOffset[idx] == ((IMG_DEVMEM_OFFSET_T)uiPageIndex << ui32Log2PageSize) + uiInPageOffset);
			PVR_ASSERT(uiPageIndex < psPMRPriv->ui32PageCount);
			PVR_ASSERT(uiInPageOffset < (1U << ui32Log2PageSize));

			sDevAddr.uiAddr = psPMRPriv->pasDevPAddr[uiPageIndex].uiAddr;
			PVR_ASSERT((sDevAddr.uiAddr & ((1UL << ui32Log2PageSize) - 1U)) == 0U);

			psDevAddrPtr[idx] = sDevAddr;
			psDevAddrPtr[idx].uiAddr += uiInPageOffset;
		}
	}

	return PVRSRV_OK;
}

#if defined(INTEGRITY_OS)
static PVRSRV_ERROR _DCPMRAcquireKernelMappingData(PMR_IMPL_PRIVDATA pvPriv,
                                                   size_t uiOffset,
                                                   size_t uiSize,
                                                   void **ppvKernelAddressOut,
                                                   IMG_HANDLE *phHandleOut,
                                                   PMR_FLAGS_T ulFlags)
{
	DC_BUFFER_PMR_DATA *psPMRPriv = (DC_BUFFER_PMR_DATA *)pvPriv;
	DC_BUFFER          *psBuffer = NULL;
	DC_DEVICE          *psDevice = NULL;
	IMG_HANDLE          hMapping = NULL;
	void	           *pvKernelAddr = NULL;
	PVRSRV_ERROR        eError = PVRSRV_OK;

	PVR_LOG_RETURN_IF_INVALID_PARAM(psPMRPriv, "psPMRPriv");

	psBuffer = psPMRPriv->psBuffer;
	psDevice = psBuffer->psDisplayContext->psDevice;

	eError = psDevice->psFuncTable->pfnAcquireKernelMappingData(psBuffer->hBuffer, &hMapping, &pvKernelAddr);
	PVR_LOG_RETURN_IF_ERROR(eError, "pfnAcquireKernelMappingData");

	*phHandleOut = hMapping;
	*ppvKernelAddressOut = pvKernelAddr;

	return eError;
}

static void _DCPMRReleaseKernelMappingData(PMR_IMPL_PRIVDATA pvPriv,
                                           IMG_HANDLE hHandle)
{
	PVR_UNREFERENCED_PARAMETER(pvPriv);
	PVR_UNREFERENCED_PARAMETER(hHandle);
}
#endif

static PVRSRV_ERROR _DCPMRFinalize(PMR_IMPL_PRIVDATA pvPriv)
{
	DC_BUFFER_PMR_DATA *psPMRPriv = pvPriv;

	_DCBufferReleaseRef(psPMRPriv->psBuffer);
	OSFreeMem(psPMRPriv);

	return PVRSRV_OK;
}

static PVRSRV_ERROR _DCPMRReadBytes(PMR_IMPL_PRIVDATA pvPriv,
                                    IMG_DEVMEM_OFFSET_T uiOffset,
                                    IMG_UINT8 *pcBuffer,
                                    size_t uiBufSz,
                                    size_t *puiNumBytes)
{
	DC_BUFFER_PMR_DATA *psPMRPriv = pvPriv;
	DC_BUFFER *psBuffer = psPMRPriv->psBuffer;
	DC_DEVICE *psDevice = psBuffer->psDisplayContext->psDevice;
	IMG_CPU_PHYADDR sCpuPAddr;
	size_t uiBytesCopied = 0;
	size_t uiBytesToCopy = uiBufSz;
	size_t uiBytesCopyableFromPage;
	void *pvMapping;
	IMG_UINT8 *pcKernelPointer;
	size_t uiBufferOffset = 0;
	size_t uiPageIndex;
	size_t uiInPageOffset;

	/* If we already have a CPU mapping just us it */
	if (psPMRPriv->pvLinAddr)
	{
		pcKernelPointer = psPMRPriv->pvLinAddr;
		OSDeviceMemCopy(pcBuffer, &pcKernelPointer[uiOffset], uiBufSz);
		*puiNumBytes = uiBufSz;
		return PVRSRV_OK;
	}

	/* Copy the data page by page */
	while (uiBytesToCopy > 0)
	{
		/* we have to kmap one page in at a time */
		uiPageIndex = TRUNCATE_64BITS_TO_SIZE_T(uiOffset >> psPMRPriv->uiLog2PageSize);

		uiInPageOffset = TRUNCATE_64BITS_TO_SIZE_T(uiOffset - ((IMG_DEVMEM_OFFSET_T)uiPageIndex << psPMRPriv->uiLog2PageSize));
		uiBytesCopyableFromPage = uiBytesToCopy;
		if (uiBytesCopyableFromPage + uiInPageOffset > (1U<<psPMRPriv->uiLog2PageSize))
		{
			uiBytesCopyableFromPage = (1 << psPMRPriv->uiLog2PageSize)-uiInPageOffset;
		}

		PhysHeapDevPAddrToCpuPAddr(psDevice->psPhysHeap, 1, &sCpuPAddr, &psPMRPriv->pasDevPAddr[uiPageIndex]);

		pvMapping = OSMapPhysToLin(sCpuPAddr,
		                           1 << psPMRPriv->uiLog2PageSize,
		                           PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
		PVR_ASSERT(pvMapping != NULL);
		pcKernelPointer = pvMapping;
		OSDeviceMemCopy(&pcBuffer[uiBufferOffset], &pcKernelPointer[uiInPageOffset], uiBytesCopyableFromPage);
		OSUnMapPhysToLin(pvMapping, 1 << psPMRPriv->uiLog2PageSize);

		uiBufferOffset += uiBytesCopyableFromPage;
		uiBytesToCopy -= uiBytesCopyableFromPage;
		uiOffset += uiBytesCopyableFromPage;
		uiBytesCopied += uiBytesCopyableFromPage;
	}

	*puiNumBytes = uiBytesCopied;
	return PVRSRV_OK;
}

static PMR_IMPL_FUNCTAB sDCPMRFuncTab = {
	.pfnLockPhysAddresses = _DCPMRLockPhysAddresses,
	.pfnUnlockPhysAddresses = _DCPMRUnlockPhysAddresses,
	.pfnDevPhysAddr = _DCPMRDevPhysAddr,
#if !defined(INTEGRITY_OS)
	.pfnAcquireKernelMappingData = NULL,
	.pfnReleaseKernelMappingData = NULL,
#else
	.pfnAcquireKernelMappingData = _DCPMRAcquireKernelMappingData,
	.pfnReleaseKernelMappingData = _DCPMRReleaseKernelMappingData,
#endif
	.pfnReadBytes = _DCPMRReadBytes,
	.pfnWriteBytes = NULL,
	.pfnChangeSparseMem = NULL,
	.pfnChangeSparseMemCPUMap = NULL,
	.pfnMMap = NULL,
	.pfnFinalize = _DCPMRFinalize
};

static PVRSRV_ERROR _DCCreatePMR(IMG_DEVMEM_LOG2ALIGN_T uiLog2PageSize,
                                 IMG_UINT32 ui32PageCount,
                                 PHYS_HEAP *psPhysHeap,
                                 DC_BUFFER *psBuffer,
                                 PMR **ppsPMR,
                                 IMG_BOOL bSystemBuffer,
                                 const IMG_CHAR *pszAnnotation)
{
	DC_BUFFER_PMR_DATA *psPMRPriv;
	IMG_DEVMEM_SIZE_T uiBufferSize;
	PVRSRV_ERROR eError;
	IMG_UINT32 uiMappingTable = 0;

	PVR_LOG_RETURN_IF_INVALID_PARAM(psPhysHeap, "psPhysHeap");

	/*
		Create the PMR for this buffer.

		Note: At this stage we don't need to know the physical pages just
		the page size and the size of the PMR. The 1st call that needs the
		physical pages will cause a request into the DC driver (pfnBufferQuery)
	 */
	psPMRPriv = OSAllocZMem(sizeof(DC_BUFFER_PMR_DATA));
	PVR_GOTO_IF_NOMEM(psPMRPriv, eError, fail_privalloc);

	/* Take a reference on the buffer (for the copy in the PMR) */
	_DCBufferAcquireRef(psBuffer);

	/* Fill in the private data for the PMR */
	psPMRPriv->uiLog2PageSize = uiLog2PageSize;
	psPMRPriv->ui32PageCount = ui32PageCount;
	psPMRPriv->pasDevPAddr = NULL;
	psPMRPriv->psBuffer = psBuffer;

	uiBufferSize = (1 << uiLog2PageSize) * ui32PageCount;

	/* Create the PMR for the MM layer */
	eError = PMRCreatePMR(psPhysHeap,
	                      uiBufferSize,
	                      1,
	                      1,
	                      &uiMappingTable,
	                      uiLog2PageSize,
	                      PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
	                      PVRSRV_MEMALLOCFLAG_UNCACHED_WC,
	                      pszAnnotation,
	                      &sDCPMRFuncTab,
	                      psPMRPriv,
	                      PMR_TYPE_DC,
	                      ppsPMR,
	                      bSystemBuffer ? PDUMP_PERSIST : PDUMP_NONE);
	PVR_GOTO_IF_ERROR(eError, fail_pmrcreate);

	return PVRSRV_OK;

fail_pmrcreate:
	_DCBufferReleaseRef(psBuffer);
	OSFreeMem(psPMRPriv);
fail_privalloc:
	return eError;
}

static void _DCDisplayContextNotify(PVRSRV_CMDCOMP_HANDLE hCmdCompHandle)
{
	DC_DISPLAY_CONTEXT	*psDisplayContext = (DC_DISPLAY_CONTEXT*) hCmdCompHandle;

	_DCDisplayContextRun(psDisplayContext);
}

static void _DCDebugRequest(PVRSRV_DBGREQ_HANDLE hDebugRequestHandle,
                            IMG_UINT32 ui32VerbLevel,
                            DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                            void *pvDumpDebugFile)
{
	DC_DISPLAY_CONTEXT	*psDisplayContext = (DC_DISPLAY_CONTEXT*) hDebugRequestHandle;

	PVR_DUMPDEBUG_LOG("Configs in-flight = %d", psDisplayContext->ui32ConfigsInFlight);

	if (DD_VERB_LVL_ENABLED(ui32VerbLevel, DEBUG_REQUEST_VERBOSITY_MEDIUM))
	{
		PVR_DUMPDEBUG_LOG("------[ Display context SCP status ]------");
		SCPDumpStatus(psDisplayContext->psSCPContext, pfnDumpDebugPrintf, pvDumpDebugFile);
	}
}

/*****************************************************************************
 * Public interface functions exposed through the bridge to services client  *
 *****************************************************************************/

PVRSRV_ERROR DCDevicesQueryCount(IMG_UINT32 *pui32DeviceCount)
{
	*pui32DeviceCount = g_ui32DCDeviceCount;
	return PVRSRV_OK;
}

PVRSRV_ERROR DCDevicesEnumerate(CONNECTION_DATA *psConnection,
                                PVRSRV_DEVICE_NODE *psDevNode,
                                IMG_UINT32 ui32DeviceArraySize,
                                IMG_UINT32 *pui32DeviceCount,
                                IMG_UINT32 *paui32DeviceIndex)
{
	IMG_UINT32 ui32DeviceCount = 0;
	DLLIST_NODE *psNode, *psNext;

	PVR_UNREFERENCED_PARAMETER(psConnection);

	/* Check that we don't have any NULL pointers / zero-sized arrays passed */
	PVR_LOG_RETURN_IF_FALSE((0U != ui32DeviceArraySize), "ui32DeviceArraySize invalid", PVRSRV_ERROR_INVALID_PARAMS);
	PVR_LOG_RETURN_IF_FALSE((NULL != pui32DeviceCount), "pui32DeviceCount invalid", PVRSRV_ERROR_INVALID_PARAMS);
	PVR_LOG_RETURN_IF_FALSE((NULL != paui32DeviceIndex), "paui32DeviceIndex invalid", PVRSRV_ERROR_INVALID_PARAMS);

	/* Iterate over whole list using dllist_foreach() */

	OSLockAcquire(g_hDCDevListLock);
	dllist_foreach_node(&g_sDCDeviceListHead, psNode, psNext)
	{
		DC_DEVICE *psTmp = IMG_CONTAINER_OF(psNode, DC_DEVICE, sListNode);

		if (psTmp->psDevNode == psDevNode)
		{
			paui32DeviceIndex[ui32DeviceCount] = psTmp->ui32Index;
			ui32DeviceCount++;
		}
	}
	OSLockRelease(g_hDCDevListLock);

	OSLockAcquire(g_hDCListLock);
	*pui32DeviceCount = ui32DeviceCount;
	OSLockRelease(g_hDCListLock);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCDeviceAcquire(CONNECTION_DATA *psConnection,
                             PVRSRV_DEVICE_NODE *psDevNode,
                             IMG_UINT32 ui32DeviceIndex,
                             DC_DEVICE **ppsDevice)
{
	DLLIST_NODE *psNode, *psNext;
	DC_DEVICE *psDevice;
	PVRSRV_ERROR eError = PVRSRV_ERROR_NO_DC_DEVICES_FOUND;
	IMG_BOOL bFound = IMG_FALSE;

	OSLockAcquire(g_hDCDevListLock);
	if (!dllist_is_empty(&g_sDCDeviceListHead))
	{
		dllist_foreach_node(&g_sDCDeviceListHead, psNode, psNext)
		{
			psDevice = IMG_CONTAINER_OF(psNode, DC_DEVICE, sListNode);
			if ((psDevice->ui32Index == ui32DeviceIndex) &&
			    (psDevice->psDevNode == psDevNode))
			{
				bFound = IMG_TRUE;
				eError = PVRSRV_OK;
				break;
			}
		}
	}
	OSLockRelease(g_hDCDevListLock);

	if (bFound)
	{
		_DCDeviceAcquireRef(psDevice);
		*ppsDevice = psDevice;
	}

	return eError;
}

PVRSRV_ERROR DCDeviceRelease(DC_DEVICE *psDevice)
{
	_DCDeviceReleaseRef(psDevice);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCGetInfo(DC_DEVICE *psDevice,
                       DC_DISPLAY_INFO *psDisplayInfo)
{
	psDevice->psFuncTable->pfnGetInfo(psDevice->hDeviceData,
	                                  psDisplayInfo);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCPanelQueryCount(DC_DEVICE *psDevice,
                               IMG_UINT32 *pui32NumPanels)
{
	psDevice->psFuncTable->pfnPanelQueryCount(psDevice->hDeviceData,
	                                          pui32NumPanels);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCPanelQuery(DC_DEVICE *psDevice,
                          IMG_UINT32 ui32PanelsArraySize,
                          IMG_UINT32 *pui32NumPanels,
                          PVRSRV_PANEL_INFO *pasPanelInfo)
{
	psDevice->psFuncTable->pfnPanelQuery(psDevice->hDeviceData,
	                                     ui32PanelsArraySize,
	                                     pui32NumPanels,
	                                     pasPanelInfo);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCFormatQuery(DC_DEVICE *psDevice,
                           IMG_UINT32 ui32FormatArraySize,
                           PVRSRV_SURFACE_FORMAT *pasFormat,
                           IMG_UINT32 *pui32Supported)
{
	psDevice->psFuncTable->pfnFormatQuery(psDevice->hDeviceData,
	                                      ui32FormatArraySize,
	                                      pasFormat,
	                                      pui32Supported);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCDimQuery(DC_DEVICE *psDevice,
                        IMG_UINT32 ui32DimSize,
                        PVRSRV_SURFACE_DIMS *pasDim,
                        IMG_UINT32 *pui32Supported)
{
	psDevice->psFuncTable->pfnDimQuery(psDevice->hDeviceData,
	                                   ui32DimSize,
	                                   pasDim,
	                                   pui32Supported);

	return PVRSRV_OK;
}


PVRSRV_ERROR DCSetBlank(DC_DEVICE *psDevice,
                        IMG_BOOL bEnabled)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
	if (psDevice->psFuncTable->pfnSetBlank)
	{
		eError = psDevice->psFuncTable->pfnSetBlank(psDevice->hDeviceData,
		                                            bEnabled);
	}

	return eError;
}

PVRSRV_ERROR DCSetVSyncReporting(DC_DEVICE *psDevice,
                                 IMG_BOOL bEnabled)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
	if (psDevice->psFuncTable->pfnSetVSyncReporting)
	{
		eError = psDevice->psFuncTable->pfnSetVSyncReporting(psDevice->hDeviceData,
		                                                     bEnabled);
	}

	return eError;
}

PVRSRV_ERROR DCLastVSyncQuery(DC_DEVICE *psDevice,
                              IMG_INT64 *pi64Timestamp)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
	if (psDevice->psFuncTable->pfnLastVSyncQuery)
	{
		eError = psDevice->psFuncTable->pfnLastVSyncQuery(psDevice->hDeviceData,
		                                                  pi64Timestamp);
	}

	return eError;
}

/*
	The system buffer breaks the rule of only calling DC callbacks on first
	ref and last deref. For the pfnBufferSystemAcquire this is expected
	as each call could get back a different buffer, but calls to
	pfnBufferAcquire and pfnBufferRelease could happen multiple times
	for the same buffer
 */
PVRSRV_ERROR DCSystemBufferAcquire(DC_DEVICE *psDevice,
                                   IMG_UINT32 *pui32ByteStride,
                                   DC_BUFFER **ppsBuffer)
{
	DC_BUFFER *psNew;
	PMR *psPMR;
	PVRSRV_ERROR eError;
	IMG_DEVMEM_LOG2ALIGN_T uiLog2PageSize;
	IMG_UINT32 ui32PageCount;

	if (psDevice->psFuncTable->pfnBufferSystemAcquire == NULL)
	{
		PVR_GOTO_WITH_ERROR(eError, PVRSRV_ERROR_NO_SYSTEM_BUFFER, fail_nopfn);
	}

	psNew = OSAllocZMem(sizeof(DC_BUFFER));
	PVR_GOTO_IF_NOMEM(psNew, eError, fail_alloc);

	eError = OSLockCreate(&psNew->hMapLock);
	PVR_GOTO_IF_ERROR(eError, fail_maplock);

	eError = psDevice->psFuncTable->pfnBufferSystemAcquire(psDevice->hDeviceData,
	                                                       &uiLog2PageSize,
	                                                       &ui32PageCount,
	                                                       pui32ByteStride,
	                                                       &psNew->hBuffer);
	PVR_GOTO_IF_ERROR(eError, fail_bufferacquire);

	psNew->psDisplayContext = &psDevice->sSystemContext;
	psNew->eType = DC_BUFFER_TYPE_SYSTEM;
	psNew->ui32MapCount = 0;
	OSAtomicWrite(&psNew->sRefCount, 1);

	/*
		Creating the PMR for the system buffer is a bit tricky as there is no
		"create" call for it.
		We should only ever have one PMR for the same buffer and so we can't
		just create one every call to this function. We also have to deal with
		the system buffer changing (mode change) so we can't just create the PMR
		once at DC driver register time.
		So what we do is cache the DC's handle to the system buffer and check if
		this call the handle has changed (indicating a mode change) and create
		a new PMR in this case.
	 */
	if (psNew->hBuffer != psDevice->hSystemBuffer)
	{
		DC_DISPLAY_INFO	sDisplayInfo;
		IMG_INT32 i32RITextSize;
		IMG_CHAR pszRIText[DEVMEM_ANNOTATION_MAX_LEN];
		if (psDevice->psSystemBufferPMR)
		{
			/*
				Mode change:
				We've already got a system buffer but the DC has given us a new
				one so we need to drop the 2nd reference we took on it as a
				different system buffer will be freed as DC unregister time
			 */
			PMRUnrefPMR(psDevice->psSystemBufferPMR);
		}

		DCGetInfo(psDevice, &sDisplayInfo);

		i32RITextSize = OSSNPrintf((IMG_CHAR *)pszRIText, DEVMEM_ANNOTATION_MAX_LEN, "%s: SysBuf", (IMG_CHAR *)sDisplayInfo.szDisplayName);
		if (i32RITextSize < 0) {
			pszRIText[0] = '\0';
			i32RITextSize = 0;
		}
		else
		{
			pszRIText[DEVMEM_ANNOTATION_MAX_LEN-1] = '\0';
		}

		eError = _DCCreatePMR(uiLog2PageSize,
		                      ui32PageCount,
		                      psDevice->psPhysHeap,
		                      psNew,
		                      &psPMR,
		                      IMG_TRUE,
		                      pszRIText);

		PVR_GOTO_IF_ERROR(eError, fail_createpmr);

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
		{
			/* Dummy handle - we don't need to store the reference to the PMR RI entry. Its deletion is handled internally. */

			eError = RIWritePMREntryKM (psPMR);
		}
#endif

		psNew->uBufferData.sAllocData.psPMR = psPMR;
		psDevice->hSystemBuffer = psNew->hBuffer;
		psDevice->psSystemBufferPMR = psPMR;

		/*
			Take a 2nd reference on the PMR as we always drop a reference
			in the release call but we don't want the PMR to be freed until
			either a new system buffer as been acquired or the DC device gets
			unregistered
		 */
		PMRRefPMR(psDevice->psSystemBufferPMR);
	}
	else
	{
		/*
			A PMR for the system buffer as already been created so just
			take a reference to the PMR to make sure it doesn't go away
		 */
		PMRRefPMR(psDevice->psSystemBufferPMR);
		psNew->uBufferData.sAllocData.psPMR = psDevice->psSystemBufferPMR;
	}

	/*
		The system buffer is tied to the device unlike all other buffers
		which are tied to a display context.
	 */
	_DCDeviceAcquireRef(psDevice);

	*ppsBuffer = psNew;

	return PVRSRV_OK;

fail_createpmr:
fail_bufferacquire:
	OSLockDestroy(psNew->hMapLock);
fail_maplock:
	OSFreeMem(psNew);
fail_alloc:
fail_nopfn:
	return eError;
}

PVRSRV_ERROR DCSystemBufferRelease(DC_BUFFER *psBuffer)
{
	PMRUnrefPMR(psBuffer->uBufferData.sAllocData.psPMR);
	_DCBufferReleaseRef(psBuffer);
	return PVRSRV_OK;
}

PVRSRV_ERROR DCDisplayContextCreate(DC_DEVICE *psDevice,
                                    DC_DISPLAY_CONTEXT **ppsDisplayContext)
{
	DC_DISPLAY_CONTEXT *psDisplayContext;
	PVRSRV_ERROR eError;

	psDisplayContext = OSAllocMem(sizeof(DC_DISPLAY_CONTEXT));
	PVR_RETURN_IF_NOMEM(psDisplayContext);

	psDisplayContext->psDevice = psDevice;
	psDisplayContext->hDisplayContext = NULL;
	psDisplayContext->ui32TokenOut = 0;
	psDisplayContext->ui32TokenIn = 0;
	psDisplayContext->ui32ConfigsInFlight = 0;
	psDisplayContext->bIssuedNullFlip = IMG_FALSE;
	psDisplayContext->hTimer = NULL;
	psDisplayContext->bPauseMISR = IMG_FALSE;
	OSAtomicWrite(&psDisplayContext->sRefCount, 1);

	eError = OSLockCreate(&psDisplayContext->hLock);
	PVR_GOTO_IF_ERROR(eError, FailLock);

	eError = OSLockCreate(&psDisplayContext->hConfigureLock);
	PVR_GOTO_IF_ERROR(eError, FailLock2);

	/* Create a Software Command Processor with 4K CCB size.
	 * With the HWC it might be possible to reach the limit off the buffer.
	 * This could be bad when the buffers currently on the screen can't be
	 * flipped to the new one, cause the command for them doesn't fit into the
	 * queue (Deadlock). This situation should properly detected to make at
	 * least the debugging easier. */
	eError = SCPCreate(psDevice->psDevNode, 12, &psDisplayContext->psSCPContext);
	PVR_GOTO_IF_ERROR(eError, FailSCP);

	eError = psDevice->psFuncTable->pfnContextCreate(psDevice->hDeviceData,
	                                                 &psDisplayContext->hDisplayContext);

	PVR_GOTO_IF_ERROR(eError, FailDCDeviceContext);

	_DCDeviceAcquireRef(psDevice);

	/* Create an MISR for our display context */
	eError = OSInstallMISR(&psDisplayContext->hMISR,
	                       DC_MISRHandler_DisplaySCP,
	                       psDisplayContext,
	                       "DC_DisplaySCP");
	PVR_GOTO_IF_ERROR(eError, FailMISR);

	/*
		Register for the command complete callback.

		Note:
		After calling this function our MISR can be called at any point.
	 */
	eError = PVRSRVRegisterCmdCompleteNotify(&psDisplayContext->hCmdCompNotify, _DCDisplayContextNotify, psDisplayContext);
	PVR_GOTO_IF_ERROR(eError, FailRegisterCmdComplete);

	/* Register our debug request notify callback */
	eError = PVRSRVRegisterDeviceDbgRequestNotify(&psDisplayContext->hDebugNotify,
	                                              psDevice->psDevNode,
	                                              _DCDebugRequest,
	                                              DEBUG_REQUEST_DC,
	                                              psDisplayContext);
	PVR_GOTO_IF_ERROR(eError, FailRegisterDbgRequest);

	*ppsDisplayContext = psDisplayContext;

	OSLockAcquire(g_hDCDisplayContextsListLock);
	/* store pointer to first/only display context, required for DCDisplayContextFlush */
	dllist_add_to_tail(&g_sDisplayContextsListHead, &psDisplayContext->sListNode);
	OSLockRelease(g_hDCDisplayContextsListLock);

	return PVRSRV_OK;

FailRegisterDbgRequest:
	PVRSRVUnregisterCmdCompleteNotify(psDisplayContext->hCmdCompNotify);
FailRegisterCmdComplete:
	OSUninstallMISR(psDisplayContext->hMISR);
FailMISR:
	_DCDeviceReleaseRef(psDevice);
	psDevice->psFuncTable->pfnContextDestroy(psDisplayContext->hDisplayContext);
FailDCDeviceContext:
	SCPDestroy(psDisplayContext->psSCPContext);
FailSCP:
	OSLockDestroy(psDisplayContext->hConfigureLock);
FailLock2:
	OSLockDestroy(psDisplayContext->hLock);
FailLock:
	OSFreeMem(psDisplayContext);
	return eError;
}

PVRSRV_ERROR DCDisplayContextConfigureCheck(DC_DISPLAY_CONTEXT *psDisplayContext,
                                            IMG_UINT32 ui32PipeCount,
                                            PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
                                            DC_BUFFER **papsBuffers)
{
	DC_DEVICE *psDevice = psDisplayContext->psDevice;
	PVRSRV_ERROR eError;
	IMG_HANDLE *ahBuffers;

	_DCDisplayContextAcquireRef(psDisplayContext);

	/* Create an array of private device specific buffer handles */
	eError = _DCDeviceBufferArrayCreate(ui32PipeCount,
	                                    papsBuffers,
	                                    &ahBuffers);
	PVR_GOTO_IF_ERROR(eError, FailBufferArrayCreate);

	/* Do we need to check if this is valid config? */
	if (psDevice->psFuncTable->pfnContextConfigureCheck)
	{

		eError = psDevice->psFuncTable->pfnContextConfigureCheck(psDisplayContext->hDisplayContext,
		                                                         ui32PipeCount,
		                                                         pasSurfAttrib,
		                                                         ahBuffers);
		PVR_GOTO_IF_ERROR(eError, FailConfigCheck);
	}

	_DCDeviceBufferArrayDestroy(ahBuffers);
	_DCDisplayContextReleaseRef(psDisplayContext);
	return PVRSRV_OK;

FailConfigCheck:
	_DCDeviceBufferArrayDestroy(ahBuffers);
FailBufferArrayCreate:
	_DCDisplayContextReleaseRef(psDisplayContext);

	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


static void _DCDisplayContextFlush(PDLLIST_NODE psNode)
{
	DC_CMD_RDY_DATA sReadyData;
	DC_CMD_COMP_DATA sCompleteData;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 ui32NumConfigsInSCP, ui32GoodRuns, ui32LoopCount;

	DC_DISPLAY_CONTEXT * psDisplayContext = IMG_CONTAINER_OF(psNode, DC_DISPLAY_CONTEXT, sListNode);

	/* Make the NULL flip command data */
	sReadyData.psDisplayContext = psDisplayContext;
	sReadyData.ui32DisplayPeriod = 0;
	sReadyData.ui32BufferCount = 0;
	sReadyData.pasSurfAttrib = NULL;
	sReadyData.pahBuffer = NULL;

	sCompleteData.psDisplayContext = psDisplayContext;
	sCompleteData.ui32BufferCount = 0;
	sCompleteData.ui32Token = 0;
	sCompleteData.bDirectNullFlip = IMG_TRUE;

	/* Stop the MISR to stop the SCP from running outside of our control */
	psDisplayContext->bPauseMISR = IMG_TRUE;

	/*
	 * Flush loop control:
	 * take the total number of Configs owned by the SCP including those
	 * "in-flight" with the DC, then multiply by 2 to account for any padding
	 * commands in the SCP buffer
	 */
	ui32NumConfigsInSCP = psDisplayContext->ui32TokenOut - psDisplayContext->ui32TokenIn;
	ui32NumConfigsInSCP *= 2;
	ui32GoodRuns = 0;
	ui32LoopCount = 0;

	/*
	 * Calling SCPRun first, ensures that any call to SCPRun from the MISR
	 * context completes before we insert any NULL flush direct to the DC.
	 * SCPRun returns PVRSRV_OK (0) if the run command (Configure) executes OR there
	 * is no work to be done OR it consumes a padding command.
	 * By counting a "good" SCPRun for each of the ui32NumConfigsInSCP we ensure
	 * that all Configs currently in the SCP are flushed to the DC.
	 *
	 * In the case where we fail dependencies (PVRSRV_ERROR_FAILED_DEPENDENCIES (15))
	 * but there are outstanding ui32ConfigsInFlight that may satisfy them,
	 * we just loop and try again.
	 * In the case where there is still more work but the DC is full
	 * (PVRSRV_ERROR_NOT_READY (254)), we just loop and try again.
	 *
	 * During a flush, NULL flips may be inserted if waiting for the 3D (not
	 * actually deadlocked), but this should be benign
	 */
	while ( ui32GoodRuns < ui32NumConfigsInSCP && ui32LoopCount < 500 )
	{
		eError = SCPRun( psDisplayContext->psSCPContext );

		if ( 0 == ui32LoopCount && PVRSRV_ERROR_FAILED_DEPENDENCIES != eError && 1 != psDisplayContext->ui32ConfigsInFlight )
		{
			PVR_DPF((PVR_DBG_ERROR, "DCDisplayContextFlush: called when not required"));
			break;
		}

		if ( PVRSRV_OK == eError )
		{
			ui32GoodRuns++;
		}
		else if ( PVRSRV_ERROR_FAILED_DEPENDENCIES == eError && 1 == psDisplayContext->ui32ConfigsInFlight )
		{
			PVR_DPF((PVR_DBG_WARNING, "DCDisplayContextFlush: inserting NULL flip"));

			/* The next Config may be dependent on the single Config currently in the DC */
			/* Issue a NULL flip to free it */
			_DCDisplayContextAcquireRef(psDisplayContext);
			_DCDisplayContextConfigure( (void *)&sReadyData, (void *)&sCompleteData );
		}

		/* Give up the timeslice to let something happen */
		OSSleepms(1);
		ui32LoopCount++;
	}

	if ( ui32LoopCount >= 500 )
	{
		PVR_DPF((PVR_DBG_ERROR, "DCDisplayContextFlush: Failed to flush after > 500 milliseconds"));
	}

	PVR_DPF((PVR_DBG_WARNING, "DCDisplayContextFlush: inserting final NULL flip"));

	/* The next Config may be dependent on the single Config currently in the DC */
	/* Issue a NULL flip to free it */
	_DCDisplayContextAcquireRef(psDisplayContext);
	_DCDisplayContextConfigure( (void *)&sReadyData, (void *)&sCompleteData );

	/* re-enable the MISR/SCP */
	psDisplayContext->bPauseMISR = IMG_FALSE;
}


PVRSRV_ERROR DCDisplayContextFlush(void)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	OSLockAcquire(g_hDCDisplayContextsListLock);

	if ( !dllist_is_empty(&g_sDisplayContextsListHead) )
	{
		DLLIST_NODE *psNode, *psNext;
		dllist_foreach_node(&g_sDisplayContextsListHead, psNode, psNext)
		{
			_DCDisplayContextFlush(psNode);
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "DCDisplayContextFlush: No display contexts found"));
		eError = PVRSRV_ERROR_INVALID_CONTEXT;
	}

	OSLockRelease(g_hDCDisplayContextsListLock);

	return eError;
}


PVRSRV_ERROR DCDisplayContextConfigure(DC_DISPLAY_CONTEXT *psDisplayContext,
                                       IMG_UINT32 ui32PipeCount,
                                       PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
                                       DC_BUFFER **papsBuffers,
                                       IMG_UINT32 ui32DisplayPeriod,
                                       IMG_UINT32 ui32MaxDepth,
                                       PVRSRV_FENCE iAcquireFence,
                                       PVRSRV_TIMELINE iReleaseFenceTimeline,
                                       PVRSRV_FENCE *piReleaseFence)
{
	DC_DEVICE *psDevice = psDisplayContext->psDevice;
	PVRSRV_ERROR eError;
	IMG_HANDLE *ahBuffers;
	IMG_UINT32 ui32BuffersMapped = 0;
	IMG_UINT32 i;
	IMG_UINT32 ui32CmdRdySize;
	IMG_UINT32 ui32CmdCompSize;
	IMG_UINT32 ui32CopySize;
	IMG_PUINT8 pui8ReadyData;
	void *pvCompleteData;
	DC_CMD_RDY_DATA *psReadyData;
	DC_CMD_COMP_DATA *psCompleteData;

	_DCDisplayContextAcquireRef(psDisplayContext);

	if (ui32MaxDepth == 1)
	{
		PVR_GOTO_WITH_ERROR(eError, PVRSRV_ERROR_DC_INVALID_MAXDEPTH, FailMaxDepth);
	}
	else if (ui32MaxDepth > 0)
	{
		/* ui32TokenOut/In wrap-around case takes care of itself. */
		if (psDisplayContext->ui32TokenOut - psDisplayContext->ui32TokenIn >= ui32MaxDepth)
		{
			_DCDisplayContextRun(psDisplayContext);
			PVR_GOTO_WITH_ERROR(eError, PVRSRV_ERROR_RETRY, FailMaxDepth);
		}
	}

	/* Reset the release fence */
	if (piReleaseFence)
		*piReleaseFence = PVRSRV_NO_FENCE;

	/* If we get sent a NULL flip then we don't need to do the check or map */
	if (ui32PipeCount != 0)
	{
		/* Create an array of private device specific buffer handles */
		eError = _DCDeviceBufferArrayCreate(ui32PipeCount,
		                                    papsBuffers,
		                                    &ahBuffers);
		PVR_GOTO_IF_ERROR(eError, FailBufferArrayCreate);

		/* Do we need to check if this is valid config? */
		if (psDevice->psFuncTable->pfnContextConfigureCheck)
		{
			eError = psDevice->psFuncTable->pfnContextConfigureCheck(psDisplayContext->hDisplayContext,
			                                                         ui32PipeCount,
			                                                         pasSurfAttrib,
			                                                         ahBuffers);
			PVR_GOTO_IF_ERROR(eError, FailConfigCheck);
		}

		/* Map all the buffers that are going to be used */
		for (i=0;i<ui32PipeCount;i++)
		{
			eError = _DCBufferMap(papsBuffers[i]);
			PVR_LOG_GOTO_IF_ERROR(eError, "_DCBufferMap", FailMapBuffer);
			ui32BuffersMapped++;
		}
	}

	ui32CmdRdySize = sizeof(DC_CMD_RDY_DATA) +
			((sizeof(IMG_HANDLE) + sizeof(PVRSRV_SURFACE_CONFIG_INFO))
					* ui32PipeCount);
	ui32CmdCompSize = sizeof(DC_CMD_COMP_DATA) +
			(sizeof(DC_BUFFER *) * ui32PipeCount);

	/* Allocate a command */
	eError = SCPAllocCommand(psDisplayContext->psSCPContext,
	                         iAcquireFence,
	                         _DCDisplayContextReady,
	                         _DCDisplayContextConfigure,
	                         ui32CmdRdySize,
	                         ui32CmdCompSize,
	                         (void **)&pui8ReadyData,
	                         &pvCompleteData,
	                         iReleaseFenceTimeline,
	                         piReleaseFence);

	PVR_GOTO_IF_ERROR(eError, FailCommandAlloc);

	/*
		Set up command ready data
	 */
	psReadyData = (DC_CMD_RDY_DATA *)(void *)pui8ReadyData;
	pui8ReadyData += sizeof(DC_CMD_RDY_DATA);

	psReadyData->ui32DisplayPeriod = ui32DisplayPeriod;
	psReadyData->psDisplayContext = psDisplayContext;
	psReadyData->ui32BufferCount = ui32PipeCount;

	/* Copy over surface attribute array */
	if (ui32PipeCount != 0)
	{
		psReadyData->pasSurfAttrib = (PVRSRV_SURFACE_CONFIG_INFO *)(void *)pui8ReadyData;
		ui32CopySize = sizeof(PVRSRV_SURFACE_CONFIG_INFO) * ui32PipeCount;
		OSCachedMemCopy(psReadyData->pasSurfAttrib, pasSurfAttrib, ui32CopySize);
		pui8ReadyData = pui8ReadyData + ui32CopySize;
	}
	else
	{
		psReadyData->pasSurfAttrib = NULL;
	}

	/* Copy over device buffer handle buffer array */
	if (ui32PipeCount != 0)
	{
		psReadyData->pahBuffer = (IMG_HANDLE)pui8ReadyData;
		ui32CopySize = sizeof(IMG_HANDLE) * ui32PipeCount;
		OSCachedMemCopy(psReadyData->pahBuffer, ahBuffers, ui32CopySize);
	}
	else
	{
		psReadyData->pahBuffer = NULL;
	}

	/*
		Set up command complete data
	 */
	psCompleteData = pvCompleteData;
	pvCompleteData = (IMG_PUINT8)pvCompleteData + sizeof(DC_CMD_COMP_DATA);

	psCompleteData->psDisplayContext = psDisplayContext;
	psCompleteData->ui32Token = psDisplayContext->ui32TokenOut++;
	psCompleteData->ui32BufferCount = ui32PipeCount;
	psCompleteData->bDirectNullFlip = IMG_FALSE;

	if (ui32PipeCount != 0)
	{
		/* Copy the buffer pointers */
		psCompleteData->apsBuffer = pvCompleteData;
		for (i=0;i<ui32PipeCount;i++)
		{
			psCompleteData->apsBuffer[i] = papsBuffers[i];
		}
	}

	/* Submit the command */
	eError = SCPSubmitCommand(psDisplayContext->psSCPContext);

	/* Check for new work on this display context */
	_DCDisplayContextRun(psDisplayContext);

	/* The only way this submit can fail is if there is a bug in this module */
	PVR_ASSERT(eError == PVRSRV_OK);

	if (ui32PipeCount != 0)
	{
		_DCDeviceBufferArrayDestroy(ahBuffers);
	}

	return PVRSRV_OK;

FailCommandAlloc:
FailMapBuffer:
	if (ui32PipeCount != 0)
	{
		for (i=0;i<ui32BuffersMapped;i++)
		{
			_DCBufferUnmap(papsBuffers[i]);
		}
	}
FailConfigCheck:
	if (ui32PipeCount != 0)
	{
		_DCDeviceBufferArrayDestroy(ahBuffers);
	}
FailBufferArrayCreate:
FailMaxDepth:
	_DCDisplayContextReleaseRef(psDisplayContext);

	return eError;
}

PVRSRV_ERROR DCDisplayContextDestroy(DC_DISPLAY_CONTEXT *psDisplayContext)
{
	PVRSRV_ERROR eError;

	/*
		On the first cleanup request try to issue the NULL flip.
		If we fail then we should get retry which we pass back to
		the caller who will try again later.
	 */
	if (!psDisplayContext->bIssuedNullFlip)
	{
		eError = DCDisplayContextConfigure(psDisplayContext,
		                                   0,
		                                   NULL,
		                                   NULL,
		                                   0,
		                                   0,
		                                   PVRSRV_NO_FENCE,
		                                   PVRSRV_NO_TIMELINE,
		                                   PVRSRV_NO_FENCE_PTR);

		PVR_RETURN_IF_ERROR(eError);
		psDisplayContext->bIssuedNullFlip = IMG_TRUE;
	}

	/*
		Flush out everything from SCP

		This will ensure that the MISR isn't dropping the last reference
		which would cause a deadlock during cleanup
	 */
	eError = SCPFlush(psDisplayContext->psSCPContext);
	PVR_RETURN_IF_ERROR(eError);

	_DCDisplayContextReleaseRef(psDisplayContext);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCBufferAlloc(DC_DISPLAY_CONTEXT *psDisplayContext,
                           DC_BUFFER_CREATE_INFO *psSurfInfo,
                           IMG_UINT32 *pui32ByteStride,
                           DC_BUFFER **ppsBuffer)
{
	DC_DEVICE *psDevice = psDisplayContext->psDevice;
	DC_BUFFER *psNew;
	PMR *psPMR;
	PVRSRV_ERROR eError;
	IMG_DEVMEM_LOG2ALIGN_T uiLog2PageSize;
	IMG_UINT32 ui32PageCount;
	DC_DISPLAY_INFO	sDisplayInfo;
	IMG_INT32 i32RITextSize;
	IMG_CHAR pszRIText[DEVMEM_ANNOTATION_MAX_LEN];

	psNew = OSAllocZMem(sizeof(DC_BUFFER));
	PVR_RETURN_IF_NOMEM(psNew);

	eError = OSLockCreate(&psNew->hMapLock);
	PVR_GOTO_IF_ERROR(eError, fail_maplock);

	eError = psDevice->psFuncTable->pfnBufferAlloc(psDisplayContext->hDisplayContext,
	                                               psSurfInfo,
	                                               &uiLog2PageSize,
	                                               &ui32PageCount,
	                                               pui32ByteStride,
	                                               &psNew->hBuffer);
	PVR_GOTO_IF_ERROR(eError, fail_bufferalloc);

	/*
		Fill in the basic info for our buffer
		(must be before _DCCreatePMR)
	 */
	psNew->psDisplayContext = psDisplayContext;
	psNew->eType = DC_BUFFER_TYPE_ALLOC;
	psNew->ui32MapCount = 0;
	OSAtomicWrite(&psNew->sRefCount, 1);

	DCGetInfo(psDevice, &sDisplayInfo);

	i32RITextSize = OSSNPrintf((IMG_CHAR *)pszRIText, DEVMEM_ANNOTATION_MAX_LEN, "%s: BufAlloc", (IMG_CHAR *)sDisplayInfo.szDisplayName);
	if (i32RITextSize < 0) {
		pszRIText[0] = '\0';
		i32RITextSize = 0;
	}
	else
	{
		pszRIText[DEVMEM_ANNOTATION_MAX_LEN-1] = '\0';
	}

	eError = _DCCreatePMR(uiLog2PageSize,
	                      ui32PageCount,
	                      psDevice->psPhysHeap,
	                      psNew,
	                      &psPMR,
	                      IMG_FALSE,
	                      pszRIText);
	PVR_GOTO_IF_ERROR(eError, fail_createpmr);

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
	{
		/* Dummy handle - we don't need to store the reference to the PMR RI entry. Its deletion is handled internally. */
		eError = RIWritePMREntryKM (psPMR);
	}
#endif

	psNew->uBufferData.sAllocData.psPMR = psPMR;
	_DCDisplayContextAcquireRef(psDisplayContext);

	*ppsBuffer = psNew;

	return PVRSRV_OK;

fail_createpmr:
	psDevice->psFuncTable->pfnBufferFree(psNew->hBuffer);
fail_bufferalloc:
	OSLockDestroy(psNew->hMapLock);
fail_maplock:
	OSFreeMem(psNew);
	return eError;
}

PVRSRV_ERROR DCBufferFree(DC_BUFFER *psBuffer)
{
	/*
		Only drop the reference on the PMR if this is a DC allocated
		buffer. In the case of imported buffers the 3rd party DC
		driver manages the PMR's "directly"
	 */
	if (psBuffer->eType == DC_BUFFER_TYPE_ALLOC)
	{
		PMRUnrefPMR(psBuffer->uBufferData.sAllocData.psPMR);
	}
	_DCBufferReleaseRef(psBuffer);

	return PVRSRV_OK;
}

PVRSRV_ERROR DCBufferImport(DC_DISPLAY_CONTEXT *psDisplayContext,
                            IMG_UINT32 ui32NumPlanes,
                            PMR **papsImport,
                            DC_BUFFER_IMPORT_INFO *psSurfAttrib,
                            DC_BUFFER **ppsBuffer)
{
	DC_DEVICE *psDevice = psDisplayContext->psDevice;
	DC_BUFFER *psNew;
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	if (psDevice->psFuncTable->pfnBufferImport == NULL)
	{
		PVR_GOTO_WITH_ERROR(eError, PVRSRV_ERROR_NOT_SUPPORTED, FailEarlyError);
	}

	psNew = OSAllocZMem(sizeof(DC_BUFFER));
	PVR_GOTO_IF_NOMEM(psNew, eError, FailEarlyError);

	eError = OSLockCreate(&psNew->hMapLock);
	PVR_GOTO_IF_ERROR(eError, FailMapLock);

	eError = psDevice->psFuncTable->pfnBufferImport(psDisplayContext->hDisplayContext,
	                                                ui32NumPlanes,
	                                                (IMG_HANDLE **)papsImport,
	                                                psSurfAttrib,
	                                                &psNew->hBuffer);
	PVR_GOTO_IF_ERROR(eError, FailBufferImport);

	/*
		Take a reference on the PMR to make sure it can't be released before
		we've finished with it
	 */
	for (i=0;i<ui32NumPlanes;i++)
	{
		PMRRefPMR(papsImport[i]);

		/* Mark the PMR such that no layout changes can happen
		 * The PMR is marked immutable to make sure the exported
		 * and imported parties see the same view of the memory
		 * */
		PMR_SetLayoutFixed(papsImport[i], IMG_TRUE);
		psNew->uBufferData.sImportData.apsImport[i] = papsImport[i];
	}

	_DCDisplayContextAcquireRef(psDisplayContext);
	psNew->psDisplayContext = psDisplayContext;
	psNew->eType = DC_BUFFER_TYPE_IMPORT;
	psNew->uBufferData.sImportData.ui32NumPlanes = ui32NumPlanes;
	psNew->ui32MapCount = 0;
	OSAtomicWrite(&psNew->sRefCount, 1);

	*ppsBuffer = psNew;

	return PVRSRV_OK;

FailBufferImport:
	OSLockDestroy(psNew->hMapLock);
FailMapLock:
	OSFreeMem(psNew);

FailEarlyError:
	return eError;
}

PVRSRV_ERROR DCBufferUnimport(DC_BUFFER *psBuffer)
{
	_DCBufferReleaseRef(psBuffer);
	return PVRSRV_OK;
}


PVRSRV_ERROR DCBufferAcquire(DC_BUFFER *psBuffer, PMR **ppsPMR)
{
	PMR *psPMR = psBuffer->uBufferData.sAllocData.psPMR;
	PVRSRV_ERROR eError;

	PVR_LOG_GOTO_IF_INVALID_PARAM(psBuffer->eType != DC_BUFFER_TYPE_IMPORT, eError, fail_typecheck);
	PMRRefPMR(psPMR);

	*ppsPMR = psPMR;
	return PVRSRV_OK;

fail_typecheck:
	return eError;
}

PVRSRV_ERROR DCBufferRelease(PMR *psPMR)
{
	/*
		Drop our reference on the PMR. If we're the last one then the PMR
		will be freed and are _DCPMRFinalize function will be called where
		we drop our reference on the buffer
	 */
	PMRUnrefPMR(psPMR);
	return PVRSRV_OK;
}

PVRSRV_ERROR DCBufferPin(DC_BUFFER *psBuffer, DC_PIN_HANDLE *phPin)
{
	*phPin = psBuffer;
	return _DCBufferMap(psBuffer);
}

PVRSRV_ERROR DCBufferUnpin(DC_PIN_HANDLE hPin)
{
	DC_BUFFER *psBuffer = hPin;

	_DCBufferUnmap(psBuffer);
	return PVRSRV_OK;
}


/*****************************************************************************
 *     Public interface functions for 3rd party display class devices        *
 *****************************************************************************/

PVRSRV_ERROR DCRegisterDevice(DC_DEVICE_FUNCTIONS *psFuncTable,
                              IMG_UINT32 ui32MaxConfigsInFlight,
                              IMG_HANDLE hDeviceData,
                              IMG_HANDLE *phSrvHandle)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	DC_DEVICE *psNew;
	PVRSRV_ERROR eError;

	if (!psPVRSRVData || !psPVRSRVData->psDeviceNodeList)
	{
		return PVRSRV_ERROR_RETRY;
	}

	psNew = OSAllocMem(sizeof(DC_DEVICE));
	PVR_GOTO_IF_NOMEM(psNew, eError, FailAlloc);

	/* Associate display devices to the first device node */
	/* Iterate over psDeviceNodeList to get the correct dev_t for this
	 * instance - Use psNext to traverse the list
	 */
	psNew->psDevNode = PVRSRVGetDeviceInstance(g_ui32DCDeviceCount);
	PVR_GOTO_IF_INVALID_PARAM(psNew->psDevNode != NULL, eError, FailEventObject);
	psNew->psFuncTable = psFuncTable;
	psNew->ui32MaxConfigsInFlight = ui32MaxConfigsInFlight;
	psNew->hDeviceData = hDeviceData;
	psNew->ui32Index = g_ui32DCNextIndex++;
	dllist_init(&psNew->sListNode);
	OSAtomicWrite(&psNew->sRefCount, 1);
	eError = OSEventObjectCreate("DC_EVENT_OBJ", &psNew->psEventList);
	PVR_GOTO_IF_ERROR(eError, FailEventObject);

	eError = PhysHeapAcquireByUsage((PHYS_HEAP_USAGE_FLAGS)PHYS_HEAP_USAGE_DISPLAY, psNew->psDevNode, &psNew->psPhysHeap);
	if (eError == PVRSRV_ERROR_PHYSHEAP_ID_INVALID)
	{
		/* DC based system layers must provide a DISPLAY Phys Heap or a
		 * GPU_LOCAL heap which the display controller can operate with. This
		 * can not use the default defined heap for the device as a CPU_LOCAL
		 * value might not be accessible by the display controller HW. */
		eError = PhysHeapAcquireByUsage(PHYS_HEAP_USAGE_GPU_LOCAL, psNew->psDevNode, &psNew->psPhysHeap);
	}
	PVR_LOG_GOTO_IF_ERROR(eError, "DCRegisterDevice: DISPLAY heap not found", FailPhysHeap);

	/* Init state required for system surface */
	psNew->hSystemBuffer = NULL;
	psNew->psSystemBufferPMR = NULL;
	psNew->sSystemContext.psDevice = psNew;
	psNew->sSystemContext.hDisplayContext = hDeviceData;

	OSLockAcquire(g_hDCDevListLock);
	dllist_add_to_tail(&g_sDCDeviceListHead, &psNew->sListNode);
	OSLockRelease(g_hDCDevListLock);

	OSLockAcquire(g_hDCListLock);
	g_ui32DCDeviceCount++;
	OSLockRelease(g_hDCListLock);

	*phSrvHandle = (IMG_HANDLE) psNew;

	return PVRSRV_OK;

FailPhysHeap:
	OSEventObjectDestroy(psNew->psEventList);
FailEventObject:
	OSFreeMem(psNew);
FailAlloc:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

void DCUnregisterDevice(IMG_HANDLE hSrvHandle)
{
	DC_DEVICE *psDevice = (DC_DEVICE *) hSrvHandle;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR eError;
	IMG_INT iRefCount;

	/*
		If the system buffer was acquired and a PMR created for it, release
		it before releasing the device as the PMR will have a reference to
		the device
	 */
	if (psDevice->psSystemBufferPMR)
	{
		PMRUnrefPMR(psDevice->psSystemBufferPMR);
	}

	/*
	 * At this stage the DC driver wants to unload, if other things have
	 * reference to the DC device we need to block here until they have
	 * been release as when this function returns the DC driver code could
	 * be unloaded.
	 */

	iRefCount = OSAtomicRead(&psDevice->sRefCount);
	if (iRefCount != 1)
	{
		/* If the driver is in a bad state we just free resources regardless */
		if (psPVRSRVData->eServicesState == PVRSRV_SERVICES_STATE_OK)
		{
			IMG_HANDLE hEvent;

			eError = OSEventObjectOpen(psDevice->psEventList, &hEvent);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,
						"%s: Failed to open event object (%d), will busy wait",
						__func__, eError));
				hEvent = NULL;
			}

			while (OSAtomicRead(&psDevice->sRefCount) != 1)
			{
				if (hEvent)
				{
					OSEventObjectWait(hEvent);
				}
			}

			OSEventObjectClose(hEvent);
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Services is in a bad state. Force unregister DC device.",
					__func__));
		}
	}

	_DCDeviceReleaseRef(psDevice);

	if (OSAtomicRead(&psDevice->sRefCount) == 0)
	{
		PhysHeapRelease(psDevice->psPhysHeap);
		OSEventObjectDestroy(psDevice->psEventList);
		OSFreeMem(psDevice);
	}
	else
	{
		/* there are stuck references so it is not safe to free the memory */
		PVR_DPF((PVR_DBG_ERROR, "%s: DC device has stuck references. Not freeing memory.", __func__));
	}
}

void DCDisplayConfigurationRetired(IMG_HANDLE hConfigData)
{
	DC_CMD_COMP_DATA *psData = hConfigData;
	DC_DISPLAY_CONTEXT *psDisplayContext = psData->psDisplayContext;
	IMG_UINT32 i;

	DC_DEBUG_PRINT("DCDisplayConfigurationRetired: Command (%d) received", psData->ui32Token);
	/* Check the config is as expected */
	if (!psData->bDirectNullFlip && psData->ui32Token != psDisplayContext->ui32TokenIn)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"Display config retired in unexpected order (was %d, expecting %d)",
				psData->ui32Token, psDisplayContext->ui32TokenIn));
		PVR_ASSERT(IMG_FALSE);
	}

	OSLockAcquire(psDisplayContext->hLock);
	if ( !psData->bDirectNullFlip )
	{
		psDisplayContext->ui32TokenIn++;
	}

#if defined(SUPPORT_DC_COMPLETE_TIMEOUT_DEBUG)
	if (psDisplayContext->hTimer)
	{
		OSDisableTimer(psDisplayContext->hTimer);
		OSRemoveTimer(psDisplayContext->hTimer);
		psDisplayContext->hTimer = NULL;
	}
#endif	/* SUPPORT_DC_COMPLETE_TIMEOUT_DEBUG */

	psDisplayContext->ui32ConfigsInFlight--;
	OSLockRelease(psDisplayContext->hLock);

	for (i = 0; i < psData->ui32BufferCount; i++)
	{
		_DCBufferUnmap(psData->apsBuffer[i]);
	}

	_DCDisplayContextReleaseRef(psDisplayContext);

	/*
		Note:

		We must call SCPCommandComplete here and not before as we need
		to ensure that we're not the last to hold the reference as
		we can't destroy the display context from the MISR which we
		can be called from.

		Ignore any fence checks if doing a null flip (e.g. when trying to unblock
		stalled applications).
	 */
	SCPCommandComplete(psDisplayContext->psSCPContext, psData->bDirectNullFlip);

	/* Notify devices (including ourself) in case some item has been unblocked */
	PVRSRVCheckStatus(NULL);
}

IMG_BOOL DCDisplayHasPendingCommand(IMG_HANDLE hConfigData)
{
	DC_CMD_COMP_DATA *psData = hConfigData;
	DC_DISPLAY_CONTEXT *psDisplayContext = psData->psDisplayContext;
	IMG_BOOL bRet;

	_DCDisplayContextAcquireRef(psDisplayContext);
	bRet = SCPHasPendingCommand(psDisplayContext->psSCPContext);
	_DCDisplayContextReleaseRef(psDisplayContext);

	return bRet;
}

PVRSRV_ERROR DCImportBufferAcquire(IMG_HANDLE hImport,
                                   IMG_DEVMEM_LOG2ALIGN_T uiLog2PageSize,
                                   IMG_UINT32 *pui32PageCount,
                                   IMG_DEV_PHYADDR **ppasDevPAddr)
{
	PMR *psPMR = hImport;
	IMG_DEV_PHYADDR *pasDevPAddr;
	IMG_DEVMEM_SIZE_T uiLogicalSize;
	size_t uiPageCount;
	IMG_BOOL *pbValid;
	PVRSRV_ERROR eError;
#if defined(DEBUG)
	IMG_UINT32 i;
#endif

	PMR_LogicalSize(psPMR, &uiLogicalSize);

	uiPageCount = TRUNCATE_64BITS_TO_SIZE_T(uiLogicalSize >> uiLog2PageSize);

	pasDevPAddr = OSAllocMem(sizeof(IMG_DEV_PHYADDR) * uiPageCount);
	PVR_GOTO_IF_NOMEM(pasDevPAddr, eError, e0);

	pbValid = OSAllocMem(uiPageCount * sizeof(IMG_BOOL));
	PVR_GOTO_IF_NOMEM(pbValid, eError, e1);

	/* Lock the pages */
	eError = PMRLockSysPhysAddresses(psPMR);
	PVR_GOTO_IF_ERROR(eError, e2);

	/* Get page physical addresses */
	eError = PMR_DevPhysAddr(psPMR, uiLog2PageSize, uiPageCount, 0,
	                         pasDevPAddr, pbValid);
	PVR_GOTO_IF_ERROR(eError, e3);

#if defined(DEBUG)
	/* The DC import function doesn't support
	   sparse allocations */
	for (i=0; i<uiPageCount; i++)
	{
		PVR_ASSERT(pbValid[i]);
	}
#endif

	OSFreeMem(pbValid);

	*pui32PageCount = TRUNCATE_SIZE_T_TO_32BITS(uiPageCount);
	*ppasDevPAddr = pasDevPAddr;
	return PVRSRV_OK;

e3:
	PMRUnlockSysPhysAddresses(psPMR);
e2:
	OSFreeMem(pbValid);
e1:
	OSFreeMem(pasDevPAddr);
e0:
	return eError;
}

void DCImportBufferRelease(IMG_HANDLE hImport,
                           IMG_DEV_PHYADDR *pasDevPAddr)
{
	PMR *psPMR = hImport;

	/* Unlock the pages */
	PMRUnlockSysPhysAddresses(psPMR);
	OSFreeMem(pasDevPAddr);
}

#if defined(INTEGRITY_OS)
IMG_HANDLE DCDisplayContextGetHandle(DC_DISPLAY_CONTEXT *psDisplayContext)
{
	PVR_ASSERT(psDisplayContext);
	return psDisplayContext->hDisplayContext;
}

IMG_UINT32 DCDeviceGetIndex(IMG_HANDLE hDeviceData)
{
	IMG_UINT32 ui32Index = 0;
	DLLIST_NODE *psNode, *psNext;

	OSLockAcquire(g_hDCDevListLock);
	dllist_foreach_node(&g_sDCDeviceListHead, psNode, psNext)
	{
		DC_DEVICE *psDevice = IMG_CONTAINER_OF(psNode, DC_DEVICE, sListNode);
		if (psDevice->hDeviceData == hDeviceData)
		{
			ui32Index = psDevice->ui32Index;
			break;
		}
	}
	OSLockRelease(g_hDCDevListLock);

	return ui32Index;
}

IMG_HANDLE DCDeviceGetDeviceAtIndex(IMG_UINT32 ui32DeviceIndex)
{
	IMG_HANDLE hDeviceData = NULL;
	DLLIST_NODE *psNode, *psNext;

	OSLockAcquire(g_hDCDevListLock);
	dllist_foreach_node(&g_sDCDeviceListHead, psNode, psNext)
	{
		DC_DEVICE *psDevice = IMG_CONTAINER_OF(psNode, DC_DEVICE, sListNode);

		if (psDevice->ui32Index == ui32DeviceIndex)
		{
			hDeviceData = psDevice->hDeviceData;
			break;
		}
	}
	OSLockRelease(g_hDCDevListLock);

	return hDeviceData;
}

#endif

/*****************************************************************************
 *                Public interface functions for services                    *
 *****************************************************************************/

PVRSRV_ERROR DCResetDevice(DC_DEVICE *psDevice)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
	if (psDevice->psFuncTable->pfnResetDevice != NULL)
	{
		eError = psDevice->psFuncTable->pfnResetDevice(psDevice->hDeviceData);
	}

	return eError;
}

PVRSRV_ERROR DCInit(void)
{
	PVRSRV_ERROR eError;

	g_ui32DCNextIndex = 0;
	dllist_init(&g_sDisplayContextsListHead);
	dllist_init(&g_sDCDeviceListHead);

	eError = OSLockCreate(&g_hDCListLock);
	PVR_LOG_GOTO_IF_ERROR(eError, "OSLockCreate:1", err_out);

	eError = OSLockCreate(&g_hDCDisplayContextsListLock);
	PVR_LOG_GOTO_IF_ERROR(eError, "OSLockCreate:2", err_hDCDisplayContextsListLock);

	eError = OSLockCreate(&g_hDCDevListLock);
	PVR_LOG_GOTO_IF_ERROR(eError, "OSLockCreate:3", err_hDCDevListLock);

	return PVRSRV_OK;

err_hDCDevListLock:
	OSLockDestroy(g_hDCDisplayContextsListLock);
err_hDCDisplayContextsListLock:
	OSLockDestroy(g_hDCListLock);
err_out:
	return eError;
}

PVRSRV_ERROR DCDeInit(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	if (psPVRSRVData->eServicesState == PVRSRV_SERVICES_STATE_OK)
	{
		PVR_ASSERT(dllist_is_empty(&g_sDCDeviceListHead));
	}

	OSLockDestroy(g_hDCDevListLock);
	OSLockDestroy(g_hDCDisplayContextsListLock);
	OSLockDestroy(g_hDCListLock);

	return PVRSRV_OK;
}
