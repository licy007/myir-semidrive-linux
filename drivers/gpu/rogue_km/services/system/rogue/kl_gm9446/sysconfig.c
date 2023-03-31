/*
* sysconfig.c
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System Configuration functions.
*
* Revision History:
* -----------------
* 011, 01/14/2019 Lili create this file
*/

#include "pvrsrv_device.h"
#include "syscommon.h"
#include "allocmem.h"
#include "sysinfo.h"
#include "sysconfig.h"
#include "physheap.h"
#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif
#if defined(__linux__)
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reset.h>
#endif
#include "rgx_bvnc_defs_km.h"
#include "interrupt_support.h"
#include "dma_support.h"

/*
 * In systems that support trusted device address protection, there are three
 * physical heaps from which pages should be allocated:
 * - one heap for normal allocations
 * - one heap for allocations holding META code memory
 * - one heap for allocations holding secured DRM data
 */

#define PHYS_HEAP_IDX_GENERAL     0
#define PHYS_HEAP_IDX_FW          1
#define PHYS_HEAP_IDX_TDFWCODE    2
#define PHYS_HEAP_IDX_TDSECUREBUF 3

/* Change to test CPU_LOCAL sys layers*/
#define UMA_DEFAULT_HEAP PVRSRV_PHYS_HEAP_GPU_LOCAL

/*
	CPU to Device physical address translation
*/
static
void UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_DEV_PHYADDR *psDevPAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psDevPAddr[0].uiAddr = psCpuPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psDevPAddr[ui32Idx].uiAddr = psCpuPAddr[ui32Idx].uiAddr;
		}
	}
}

/*
	Device to CPU physical address translation
*/
static
void UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr,
								   IMG_DEV_PHYADDR *psDevPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psCpuPAddr[0].uiAddr = psDevPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psCpuPAddr[ui32Idx].uiAddr = psDevPAddr[ui32Idx].uiAddr;
		}
	}
}

static PHYS_HEAP_FUNCTIONS gsPhysHeapFuncs =
{
	/* pfnCpuPAddrToDevPAddr */
	UMAPhysHeapCpuPAddrToDevPAddr,
	/* pfnDevPAddrToCpuPAddr */
	UMAPhysHeapDevPAddrToCpuPAddr,
};

static PVRSRV_ERROR PhysHeapsCreate(void *pvOSDevice,
									PHYS_HEAP_CONFIG **ppasPhysHeapsOut,
									IMG_UINT32 *puiPhysHeapCountOut)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PHYS_HEAP_CONFIG *pasPhysHeaps;
	DMA_ALLOC *psDmaAlloc = NULL;
	IMG_UINT32 uiHeapCount = 1;

#if defined(SUPPORT_TRUSTED_DEVICE)
	uiHeapCount += 2;
#endif
	if (!PVRSRV_VZ_MODE_IS(NATIVE))
		uiHeapCount += 1;

	pasPhysHeaps = OSAllocZMem(sizeof(*pasPhysHeaps) * uiHeapCount);
	PVR_LOG_GOTO_IF_NOMEM(pasPhysHeaps, eError, e0);

	pasPhysHeaps[PHYS_HEAP_IDX_GENERAL].pszPDumpMemspaceName = "SYSMEM";
	pasPhysHeaps[PHYS_HEAP_IDX_GENERAL].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[PHYS_HEAP_IDX_GENERAL].psMemFuncs = &gsPhysHeapFuncs;
	pasPhysHeaps[PHYS_HEAP_IDX_GENERAL].ui32UsageFlags = PHYS_HEAP_USAGE_GPU_LOCAL;

	if (!PVRSRV_VZ_MODE_IS(NATIVE))
	{
		pr_err("PVR:DMA heap is created!!");
		pasPhysHeaps[PHYS_HEAP_IDX_FW].pszPDumpMemspaceName = "SYSMEM_FW";
		pasPhysHeaps[PHYS_HEAP_IDX_FW].eType = PHYS_HEAP_TYPE_DMA;
		pasPhysHeaps[PHYS_HEAP_IDX_FW].psMemFuncs = &gsPhysHeapFuncs;
		pasPhysHeaps[PHYS_HEAP_IDX_FW].ui32UsageFlags = PHYS_HEAP_USAGE_FW_MAIN;

		psDmaAlloc = OSAllocZMem(sizeof(DMA_ALLOC));
		PVR_LOG_GOTO_IF_NOMEM(psDmaAlloc, eError, e0);
		psDmaAlloc->pvOSDevice = pvOSDevice;
		psDmaAlloc->ui64Size = RGX_FIRMWARE_RAW_HEAP_SIZE;

		eError = SysDmaAllocMem(psDmaAlloc);
		PVR_LOG_GOTO_IF_ERROR(eError, "SysDmaAllocMem", e0);

		eError = SysDmaRegisterForIoRemapping(psDmaAlloc);
		PVR_LOG_GOTO_IF_ERROR(eError, "SysDmaRegisterForIoRemapping", e0);

		pasPhysHeaps[PHYS_HEAP_IDX_FW].hPrivData = psDmaAlloc;
		pasPhysHeaps[PHYS_HEAP_IDX_FW].sStartAddr.uiAddr = psDmaAlloc->sBusAddr.uiAddr;
		pasPhysHeaps[PHYS_HEAP_IDX_FW].sCardBase.uiAddr = psDmaAlloc->sBusAddr.uiAddr;
		pasPhysHeaps[PHYS_HEAP_IDX_FW].uiSize = psDmaAlloc->ui64Size;
	}

#if defined(SUPPORT_TRUSTED_DEVICE)
	pasPhysHeaps[PHYS_HEAP_IDX_TDFWCODE].ui32PhysHeapID = PHYS_HEAP_IDX_TDFWCODE;
	pasPhysHeaps[PHYS_HEAP_IDX_TDFWCODE].pszPDumpMemspaceName = "TDFWCODEMEM";
	pasPhysHeaps[PHYS_HEAP_IDX_TDFWCODE].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[PHYS_HEAP_IDX_TDFWCODE].psMemFuncs = &gsPhysHeapFuncs;
	pasPhysHeaps[PHYS_HEAP_IDX_TDFWCODE].ui32UsageFlags =
		PHYS_HEAP_USAGE_FW_CODE | PHYS_HEAP_USAGE_FW_PRIV_DATA;

	pasPhysHeaps[PHYS_HEAP_IDX_TDSECUREBUF].ui32PhysHeapID = PHYS_HEAP_IDX_TDSECUREBUF;
	pasPhysHeaps[PHYS_HEAP_IDX_TDSECUREBUF].pszPDumpMemspaceName = "TDSECUREBUFMEM";
	pasPhysHeaps[PHYS_HEAP_IDX_TDSECUREBUF].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[PHYS_HEAP_IDX_TDSECUREBUF].psMemFuncs = &gsPhysHeapFuncs;
	pasPhysHeaps[PHYS_HEAP_IDX_TDSECUREBUF].ui32UsageFlags = PHYS_HEAP_USAGE_GPU_SECURE;
#endif

	*ppasPhysHeapsOut = pasPhysHeaps;
	*puiPhysHeapCountOut = uiHeapCount;

	return eError;
e0:
	SysDmaDeregisterForIoRemapping(psDmaAlloc);
	SysDmaFreeMem(psDmaAlloc);
	OSFreeMem(psDmaAlloc);
	OSFreeMem(pasPhysHeaps);
	return eError;
}

static void PhysHeapsDestroy(PHYS_HEAP_CONFIG *pasPhysHeaps)
{
	DMA_ALLOC *psDmaAlloc = pasPhysHeaps[PHYS_HEAP_IDX_FW].hPrivData;

	SysDmaDeregisterForIoRemapping(psDmaAlloc);
	SysDmaFreeMem(psDmaAlloc);
	OSFreeMem(psDmaAlloc);
	OSFreeMem(pasPhysHeaps);
}

static void SysDevFeatureDepInit(PVRSRV_DEVICE_CONFIG *psDevConfig, IMG_UINT64 ui64Features)
{
#if defined(SUPPORT_AXI_ACE_TEST)
		if( ui64Features & RGX_FEATURE_AXI_ACELITE_BIT_MASK)
		{
			psDevConfig->eCacheSnoopingMode		= PVRSRV_DEVICE_SNOOP_CPU_ONLY;
		}else
#endif
		{
			psDevConfig->eCacheSnoopingMode		= PVRSRV_DEVICE_SNOOP_NONE;
		}
}


PVRSRV_DEVICE_CONFIG *gpsDevConfig = NULL;
PVRSRV_DEVICE_CONFIG *getDevConfig(void)
{
	return gpsDevConfig;
}
struct clk * gpsCoreClock = NULL;
struct clk * getCoreClock(void)
{
       return gpsCoreClock;
}

#if defined(SUPPORT_LINUX_DVFS)
static void SetFrequency(unsigned long freq)
{
	int ret;
	if (IS_ERR_OR_NULL(gpsCoreClock))
	{
		return;
	}

	ret = clk_set_rate(gpsCoreClock, clk_round_rate(gpsCoreClock,freq));

	return;
}

static void SetVoltage(int volt)
{
	PVR_UNREFERENCED_PARAMETER(volt);
}
#endif

static int gpu_hw_reset(struct device *dev)
{
	struct reset_control *gpu_rst = NULL;
	int ret = 0;

	if (dev) {
		gpu_rst = devm_reset_control_get(dev, "gpucore-reset");
		if (IS_ERR(gpu_rst)) {
			dev_err(dev, "failed to get gpucore reset control\n");
			return PTR_ERR(gpu_rst);
		}

		if (reset_control_reset(gpu_rst)) {
			dev_err(dev, "failed to reset gpucore\n");
			ret = -1;
		}
		reset_control_put(gpu_rst);
	}
	else {
		ret = -2;
	}

	return ret;
}

PVRSRV_ERROR SysDevInit(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	RGX_DATA *psRGXData;
	RGX_TIMING_INFORMATION *psRGXTimingInfo;
	PHYS_HEAP_CONFIG *pasPhysHeaps = NULL;
	IMG_UINT32 uiPhysHeapCount;
	PVRSRV_ERROR eError;

#if defined(__linux__)
	int iIrq;
	IMG_UINT32 uiClk = RGX_KUNLUN_CORE_CLOCK_SPEED;
	IMG_UINT32 uiOsId = -1;
	struct platform_device *psDev;
	struct resource *psDevMemRes = NULL;
	PVRSRV_ERROR err;

	psDev = to_platform_device((struct device *)pvOSDevice);
	dma_set_mask(pvOSDevice, DMA_BIT_MASK(40));

	pr_info("start to do gpu hw reset !\n");
	if (gpu_hw_reset((struct device *)pvOSDevice)) {
		pr_err("pvrsrvkm: gpu reset error !\n");
		//FIXME: bypas this return for now before all dts have reset handler
		//return PVRSRV_ERROR_INVALID_DEVICE;
	}

#endif

	psDevConfig = OSAllocZMem(sizeof(*psDevConfig) +
							  sizeof(*psRGXData) +
							  sizeof(*psRGXTimingInfo));
	if (!psDevConfig)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psRGXData = (RGX_DATA *)((IMG_CHAR *)psDevConfig + sizeof(*psDevConfig));
	psRGXTimingInfo = (RGX_TIMING_INFORMATION *)((IMG_CHAR *)psRGXData + sizeof(*psRGXData));

	eError = PhysHeapsCreate(pvOSDevice, &pasPhysHeaps, &uiPhysHeapCount);
	PVR_LOG_GOTO_IF_ERROR(eError, "PhysHeapsCreate", ErrorFreeDevConfig);

	/* Setup RGX specific timing data */
	psRGXTimingInfo->ui32CoreClockSpeed        = RGX_KUNLUN_CORE_CLOCK_SPEED;
	psRGXTimingInfo->bEnableActivePM           = IMG_FALSE;
	psRGXTimingInfo->bEnableRDPowIsland        = IMG_FALSE;
	psRGXTimingInfo->ui32ActivePMLatencyms     = SYS_RGX_ACTIVE_POWER_LATENCY_MS;

	/* Set up the RGX data */
	psRGXData->psRGXTimingInfo = psRGXTimingInfo;

#if defined(SUPPORT_TRUSTED_DEVICE)
	psRGXData->bHasTDFWCodePhysHeap = IMG_TRUE;
	psRGXData->uiTDFWCodePhysHeapID = PHYS_HEAP_IDX_TDFWCODE;

	psRGXData->bHasTDSecureBufPhysHeap = IMG_TRUE;
	psRGXData->uiTDSecureBufPhysHeapID = PHYS_HEAP_IDX_TDSECUREBUF;
#endif

	/* Setup the device config */
	psDevConfig->pvOSDevice				= pvOSDevice;
	psDevConfig->pszName                = "kl_gm9446";
	psDevConfig->pszVersion             = NULL;
	psDevConfig->pfnSysDevFeatureDepInit = SysDevFeatureDepInit;

        /* Device setup information */
#if defined(__linux__)
        psDevMemRes = platform_get_resource(psDev, IORESOURCE_MEM, 0);
        if (psDevMemRes)
        {
            psDevConfig->sRegsCpuPBase.uiAddr = psDevMemRes->start;
            psDevConfig->ui32RegsSize         = (unsigned int)(psDevMemRes->end - psDevMemRes->start);
        }
        else
#endif
        {
#if defined(__linux__)
            PVR_LOG(("%s: platform_get_resource() failed",
                    __func__));
#endif
            psDevConfig->sRegsCpuPBase.uiAddr   = 0x34c00000;
            psDevConfig->ui32RegsSize           = 0x10000;
        }

	psDevConfig->ui32IRQ                = 168;
#if defined(__linux__)
	iIrq = platform_get_irq(psDev, 0);
	if (iIrq >= 0)
	{
		psDevConfig->ui32IRQ = (IMG_UINT32) iIrq;
	}
#endif

#if defined(__linux__)
	/* If there is separate gpucoreclk, get the rate from it. */
	gpsCoreClock = devm_clk_get(&psDev->dev, "gpucoreclk");
	if (IS_ERR(gpsCoreClock) && PTR_ERR(gpsCoreClock) != -EPROBE_DEFER)
		gpsCoreClock = devm_clk_get(&psDev->dev, NULL);
	if (IS_ERR(gpsCoreClock) && PTR_ERR(gpsCoreClock) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (!IS_ERR_OR_NULL(gpsCoreClock)) {
		err = clk_prepare_enable(gpsCoreClock);
		if (err)
			dev_warn(&psDev->dev, "could not enable optional gpucoreclk: %d\n", err);
		else
			uiClk  = clk_get_rate(gpsCoreClock);
	}

	/* If no clock rate is defined, fail. */
	if (!uiClk) {
		dev_err(&psDev->dev, "clock rate not defined\n");
		err = -EINVAL;
		goto ErrorFreeDevConfig;
	}
	psRGXTimingInfo->ui32CoreClockSpeed = uiClk;
	pr_info("core clock is %d\n", psRGXTimingInfo->ui32CoreClockSpeed);

	device_property_read_u32(&psDev->dev, "osid", &uiOsId);
	if (uiOsId < 0) {
		dev_err(&psDev->dev, "osid not defined\n");
		err = -EINVAL;
		goto ErrorFreeDevConfig;
	}
	psDevConfig->ui32OsId = uiOsId;
#endif

	psDevConfig->pasPhysHeaps			= pasPhysHeaps;
	psDevConfig->ui32PhysHeapCount		= uiPhysHeapCount;

	/* to-do */
	psDevConfig->pfnPrePowerState       = NULL;
	psDevConfig->pfnPostPowerState      = NULL;

	/* to-do */
	psDevConfig->pfnClockFreqGet        = NULL;

	psDevConfig->hDevData               = psRGXData;
	psDevConfig->eDefaultHeap           = UMA_DEFAULT_HEAP;
	/* Setup other system specific stuff */
#if defined(SUPPORT_ION)
	IonInit(NULL);
#endif
#if defined(SUPPORT_LINUX_DVFS)
	psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable = NULL;
	psDevConfig->sDVFS.sDVFSDeviceCfg.ui32OPPTableSize = 0;

	psDevConfig->sDVFS.sDVFSDeviceCfg.bIdleReq = IMG_FALSE;
	psDevConfig->sDVFS.sDVFSDeviceCfg.pfnSetFrequency = (PFN_SYS_DEV_DVFS_SET_FREQUENCY) SetFrequency;
	psDevConfig->sDVFS.sDVFSDeviceCfg.pfnSetVoltage = (PFN_SYS_DEV_DVFS_SET_VOLTAGE) SetVoltage;
	psDevConfig->sDVFS.sDVFSDeviceCfg.ui32PollMs = SD_DVFS_SWITCH_INTERVAL;

	psDevConfig->sDVFS.sDVFSGovernorCfg.ui32UpThreshold = 90;
	psDevConfig->sDVFS.sDVFSGovernorCfg.ui32DownDifferential = 10;

#endif

	*ppsDevConfig = psDevConfig;
	gpsDevConfig = psDevConfig;

	return PVRSRV_OK;

ErrorFreeDevConfig:
	OSFreeMem(psDevConfig);
	return eError;
}

void SysDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
#if defined(SUPPORT_ION)
	IonDeinit();
#endif

	PhysHeapsDestroy(psDevConfig->pasPhysHeaps);
	OSFreeMem(psDevConfig);
}

PVRSRV_ERROR SysInstallDeviceLISR(IMG_HANDLE hSysData,
                                                                  IMG_UINT32 ui32IRQ,
                                                                  const IMG_CHAR *pszName,
                                                                  PFN_LISR pfnLISR,
                                                                  void *pvData,
                                                                  IMG_HANDLE *phLISRData)
{
        PVR_UNREFERENCED_PARAMETER(hSysData);
        return OSInstallSystemLISR(phLISRData, ui32IRQ, pszName, pfnLISR, pvData,
                                                                SYS_IRQ_FLAG_TRIGGER_DEFAULT);
}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
        return OSUninstallSystemLISR(hLISRData);
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_DEVICE_CONFIG *psDevConfig,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);
	return PVRSRV_OK;
}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
