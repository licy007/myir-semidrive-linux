/*************************************************************************/ /*!
@File
@Title          Interface between 3rd party display controller (DC) drivers
                and the Services server module.
@Description    API between Services and the 3rd party DC driver and vice versa.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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
*/ /**************************************************************************/

#if !defined(KERNELDISPLAY_H)
#define KERNELDISPLAY_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <powervr/mem_types.h>

#include "pvrsrv_error.h"
#include "img_types.h"
#include "pvrsrv_surface.h"
#include "dc_external.h"

/*!
 ******************************************************************************
 @mainpage Introduction and Document Scope
This document is a function reference for the 3rd party display
controller interface. The interface ensures that the 3rd party display driver and
Services can exchange configurations and buffers and can inform each
other about occurring events like render completion or vsyncs.

This interface should be used and implemented if the 3rd party display driver
is not using a native OS interface such as the Linux DRM/KMS API.

To fully understand the document the reader should be aware that
the Services driver - offering PowerVR core and platform functionality to the
driver layers (OGLES etc.) above - is split into the "Services Client"
user mode library and the "Services Server" kernel mode driver that communicate
via the "Services Bridge". The terms "User-Mode" and "Client" are used as
synonyms as well as "Kernel-Mode" and "Server".

Please refer to the more comprehensive '3rd Party Display Integration Guide'
for an architecture overview and an explanation of the data flow between a
client process, Services and the 3rd party display driver. It also contains
descriptions about how to make use of the client side interface that is supposed
to be integrated in some kind of display manager like e.g. PowerVR Graphics DDK WSEGL
window manager.

The documented functions are split into different parts:
- Callbacks that need an implementation by the 3rd party display driver and
that are used by the Services server, some of them optional (kerneldisplay.h)
- Functions that the Services server module exports and can be used by the
3rd party display driver. Mainly to register/deregister a new display device
and to query state information (kerneldisplay.h)
- Functions that are called by the client process (a display manager e.g. WSEGL)
to control the DC interaction (dc_client.h)
- Commonly used structure definitions to exchange data between the modules
(dc_external.h, pvrsrv_surface.h)
 *****************************************************************************/

/*************************************************************************/ /*!
@Function       GetInfo

@Description    Query the display controller for its information structure.

   Called by client function: #PVRSRVDCGetInfo()

   Implementation of this callback is mandatory.

@Input          hDeviceData             Device private data

@Output         psDisplayInfo           Display info structure
*/
/*****************************************************************************/
typedef void (*GetInfo)(IMG_HANDLE hDeviceData,
                        DC_DISPLAY_INFO *psDisplayInfo);

/*************************************************************************/ /*!
@Function       PanelQueryCount

@Description    Query the display controller for how many panels are
                connected to it.

   Called by client function: #PVRSRVDCPanelQueryCount()

   Implementation of this callback is mandatory.

@Input          hDeviceData            Device private data

@Output         pui32NumPanels         Number of panels

@Return         PVRSRV_OK if the query was successful
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*PanelQueryCount)(IMG_HANDLE hDeviceData,
                                        IMG_UINT32 *pui32NumPanels);

/*************************************************************************/ /*!
@Function       PanelQuery

@Description    Query the display controller for information on what panel(s)
                are connected to it and their properties.

   Called by client function: #PVRSRVDCPanelQuery()

   Implementation of this callback is mandatory.

@Input          hDeviceData             Device private data

@Input          ui32PanelsArraySize     Size of the PanelInfo array
                                        (i.e. number of panels that
                                        can be returned)

@Output         pui32NumPanels          Number of panels returned

@Output         pasPanelInfo            Array of formats, allocated beforehand

@Return         PVRSRV_OK if the query was successful
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*PanelQuery)(IMG_HANDLE hDeviceData,
                                   IMG_UINT32 ui32PanelsArraySize,
                                   IMG_UINT32 *pui32NumPanels,
                                   PVRSRV_PANEL_INFO *pasPanelInfo);

/*************************************************************************/ /*!
@Function       FormatQuery

@Description    Query the display controller to check if it supports the specified
                format(s).

   Called by client function: #PVRSRVDCFormatQuery()

   Implementation of this callback is mandatory.

@Input          hDeviceData             Device private data

@Input          ui32NumFormats          Number of formats to check
                                        (i.e. length of the Format array)

@Input          pasFormat               Array of formats to check

@Output         pui32Supported          For each format, the number of display
                                        pipes that support that format

@Return         PVRSRV_OK if the query was successful
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*FormatQuery)(IMG_HANDLE hDeviceData,
                                    IMG_UINT32 ui32NumFormats,
                                    PVRSRV_SURFACE_FORMAT *pasFormat,
                                    IMG_UINT32 *pui32Supported);

/*************************************************************************/ /*!
@Function       DimQuery

@Description    Query the specified display plane for the display dimensions
                it supports.

   Called by client function: #PVRSRVDCDimQuery()

   Implementation of this callback is mandatory.

@Input          hDeviceData             Device private data

@Input          ui32NumDims             Number of dimensions to check
                                        (i.e. length of the Dim array)

@Input          pasDim                  Array of dimensions to check

@Output         pui32Supported          For each dimension, the number of
                                        display pipes that support that
                                        dimension

@Return         PVRSRV_OK if the query was successful
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*DimQuery)(IMG_HANDLE hDeviceData,
                                 IMG_UINT32 ui32NumDims,
                                 PVRSRV_SURFACE_DIMS *pasDim,
                                 IMG_UINT32 *pui32Supported);

/*************************************************************************/ /*!
@Function       SetBlank

@Description    Enable/disable blanking of the screen.

   Called by client function: #PVRSRVDCSetBlank()

   Implementation of this callback is optional.

@Input          hDeviceData             Device private data

@Input          bEnable                 Enable/Disable the blanking

@Return         PVRSRV_OK on success
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*SetBlank)(IMG_HANDLE hDeviceData,
                                 IMG_BOOL bEnable);

/*************************************************************************/ /*!
@Function       SetVSyncReporting

@Description    Enable VSync reporting. If enabled, the 3rd party display
                driver is expected to call PVRSRVCheckStatus() after a VSync
                event occurred. This will signal the Services driver global
                event object.

   Called by client function: #PVRSRVDCSetVSyncReporting()

   Implementation of this callback is optional.

@Input          hDeviceData             Device private data

@Input          bEnable                 Enable/Disable the reporting

@Return         PVRSRV_OK on success
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*SetVSyncReporting)(IMG_HANDLE hDeviceData,
                                          IMG_BOOL bEnable);

/*************************************************************************/ /*!
@Function       LastVSyncQuery

@Description    Query the time the last vsync happened.

   Called by client function: #PVRSRVDCLastVSyncQuery()

   Implementation of this callback is optional.

@Input          hDeviceData             Device private data

@Output         pi64Timestamp           The requested timestamp
                                        of the system time in ns

@Return         PVRSRV_OK if the query was successful
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*LastVSyncQuery)(IMG_HANDLE hDeviceData,
                                       IMG_INT64 *pi64Timestamp);

/*************************************************************************/ /*!
@Function       ContextCreate

@Description    Create display context.
                The client application and the display driver have to agree
                as to what exactly a context represents, Services will just
                pass it through and tie it to its own concept of a
                display context.
                It might contain additional locks, work-queues or other
                important data that is necessary to drive the interaction
                with Services. A context is usually associated with one device
                but that is not a hard requirement.

   Called by client function: #PVRSRVDCDisplayContextCreate()

   Implementation of this callback is mandatory.

@Input          hDeviceData             Device private data

@Output         hDisplayContext         Created display context

@Return         PVRSRV_OK if the context was created
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*ContextCreate)(IMG_HANDLE hDeviceData,
                                      IMG_HANDLE *hDisplayContext);

/*************************************************************************/ /*!
@Function       ContextConfigureCheck

@Description    Check to see if a configuration is valid for the display
                controller. Because of runtime changes the display context
                might not be able to work with a certain configuration anymore.
                This function is intended to be called before ContextConfigure
                so that the application can query details about the current
                surface config and then do the ContextConfigure call with
                the updated data.
                The arrays should be z-sorted, with the farthest plane
                first and the nearest plane last.

   Called by client function: #PVRSRVDCContextConfigureCheck()

   Implementation of this callback is optional.

@Input          hDisplayContext         Display context

@Input          ui32PipeCount           Number of display pipes to configure
                                        (length of the input arrays)

@Input          pasSurfAttrib           Array of surface attributes (one for
                                        each display plane)

@Input          ahBuffers               Array of buffers (one for
                                        each display plane)

@Return         PVRSRV_OK if the configuration is valid
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*ContextConfigureCheck)(IMG_HANDLE hDisplayContext,
                                              IMG_UINT32 ui32PipeCount,
                                              PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
                                              IMG_HANDLE *ahBuffers);

/*************************************************************************/ /*!
@Function       ContextConfigure

@Description    Configure the display pipeline to display a given buffer.
                The arrays should be z-sorted, with the farthest plane first
                and the nearest plane last.

   Called by client function: #PVRSRVDCContextConfigureWithFence()

   Implementation of this callback is mandatory.

@Input          hDisplayContext         Display context

@Input          ui32PipeCount           Number of display pipes to configure
                                        (i.e. length of the input arrays)

@Input          pasSurfAttrib           Array of surface attributes (one for
                                        each display plane)

@Input          ahBuffers               Array of buffers (one for
                                        each display plane)

@Input          ui32DisplayPeriod       The number of VSync periods this
                                        configuration should be displayed for

@Input          hConfigData             Config handle which gets passed to
                                        DisplayConfigurationRetired when this
                                        configuration is retired
*/
/*****************************************************************************/
typedef void (*ContextConfigure)(IMG_HANDLE hDisplayContext,
                                 IMG_UINT32 ui32PipeCount,
                                 PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
                                 IMG_HANDLE *ahBuffers,
                                 IMG_UINT32 ui32DisplayPeriod,
                                 IMG_HANDLE hConfigData);

/*************************************************************************/ /*!
@Function       ContextDestroy

@Description    Destroy a display context.

   Called by client function: #PVRSRVDCDisplayContextDestroy()

   Implementation of this callback is mandatory.

@Input          hDisplayContext         Display context to destroy

@Return         None
*/
/*****************************************************************************/
typedef void (*ContextDestroy)(IMG_HANDLE hDisplayContext);

/*************************************************************************/ /*!
@Function       BufferAlloc

@Description    Allocate a display buffer. This is a request to the display
                controller to allocate a buffer from memory that is addressable
                by the display controller.

                Note: The actual allocation of display memory can be deferred
                until the first call to acquire, but the handle for the buffer
                still needs to be created and returned to the caller as well
                as some information about the buffer that's required upfront.

   Called by client function: #PVRSRVDCBufferAlloc()

   Implementation of this callback is mandatory.

@Input          hDisplayContext         Display context this buffer will be
                                        used on

@Input          psSurfInfo              Attributes of the buffer

@Output         puiLog2PageSize         Pagesize in log2(bytes) of one page

@Output         pui32PageCount          Number of pages in the buffer

@Output         pui32ByteStride         Stride (in bytes) of allocated buffer

@Output         phBuffer                Handle to allocated buffer

@Return         PVRSRV_OK if the buffer was successfully allocated
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*BufferAlloc)(IMG_HANDLE hDisplayContext,
                                    DC_BUFFER_CREATE_INFO *psSurfInfo,
                                    IMG_DEVMEM_LOG2ALIGN_T *puiLog2PageSize,
                                    IMG_UINT32 *pui32PageCount,
                                    IMG_UINT32 *pui32ByteStride,
                                    IMG_HANDLE *phBuffer);

/*************************************************************************/ /*!
@Function       BufferImport

@Description    Import memory allocated from an external source (e.g. Services)
                to the display controller. The DC checks to see if the import
                is compatible and potentially sets up HW to map the imported
                buffer, although this isn't required to happen until the first
                call to BufferMap.

                Note: Provide this function if the controller
                can scan out arbitrary memory, allocated for another purpose by
                Services. To be able to use this buffer (depending on its
                origin) the display controller probably will need a MMU.

   Called by client function: #PVRSRVDCBufferImport()

   Implementation of this callback is optional.

@Input          hDisplayContext         Display context this buffer will be
                                        used on

@Input          ui32NumPlanes           Number of planes

@Input          pahImport               Array of handles (one per colour channel)

@Input          psSurfAttrib            Surface attributes of the buffer

@Output         phBuffer                Handle to imported buffer

@Return         PVRSRV_OK if the buffer was successfully imported
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*BufferImport)(IMG_HANDLE hDisplayContext,
                                     IMG_UINT32 ui32NumPlanes,
                                     IMG_HANDLE **pahImport,
                                     DC_BUFFER_IMPORT_INFO *psSurfAttrib,
                                     IMG_HANDLE *phBuffer);

/*************************************************************************/ /*!
@Function       BufferAcquire

@Description    Acquire the buffer's physical memory pages. If the buffer doesn't
                have any memory backing yet then this will trigger the 3rd
                party driver to allocate it.

                Note: The page count isn't passed back in this function as
                Services has already obtained it during BufferAlloc.

   Called when Services requests buffer access for the first time. Usually part
   of non display class functions.

   Implementation of this callback is mandatory.

@Input          hBuffer                 Handle to the buffer

@Output         pasDevPAddr             Array of device physical page address
                                        of this buffer

@Output         ppvLinAddr              CPU virtual address of buffer. This is
                                        optional but if you have one you must
                                        return it otherwise return NULL.

@Return         PVRSRV_OK if the buffer was successfully acquired
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*BufferAcquire)(IMG_HANDLE hBuffer,
                                      IMG_DEV_PHYADDR *pasDevPAddr,
                                      void **ppvLinAddr);

/*************************************************************************/ /*!
@Function       BufferRelease

@Description    Undo everything done by BufferAcquire.
                Will release the buffer's physical memory pages if
                BufferAcquire allocated them.

   Called when Services finished buffer access.

   Implementation of this callback is mandatory.

@Input          hBuffer                 Handle to the buffer
*/
/*****************************************************************************/
typedef void (*BufferRelease)(IMG_HANDLE hBuffer);

/*************************************************************************/ /*!
@Function       BufferFree

@Description    Release a reference to the device buffer. If this was the last
                reference the 3rd party driver is entitled to free the backing
                memory and other related resources.

   Called by client function: #PVRSRVDCBufferFree()

   Implementation of this callback is mandatory.

@Input          hBuffer                 Buffer handle we're releasing
*/
/*****************************************************************************/
typedef void (*BufferFree)(IMG_HANDLE hBuffer);

/*************************************************************************/ /*!
@Function       BufferMap

@Description    Map the buffer into the display controller
                Note: This function depends on the behaviour of
                BufferAlloc/BufferAcquire/BufferImport/BufferSystemAcquire
                and the controller's ability to map in memory.
                If the controller has no MMU or the above functions already
                map the buffer this callback does not need an implementation.

   Called by client function: #PVRSRVDCBufferPin()

   Implementation of this callback is optional.

@Input          hBuffer                 Buffer to map

@Return         PVRSRV_OK if the buffer was successfully mapped
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*BufferMap)(IMG_HANDLE hBuffer);

/*************************************************************************/ /*!
@Function       BufferUnmap

@Description    Undo everything done by BufferMap.
                Usually that means to unmap a buffer from the display controller.

   Called by client function: #PVRSRVDCBufferUnpin()

   Implementation of this callback is optional.

@Input          hBuffer                 Buffer to unmap
*/
/*****************************************************************************/
typedef void (*BufferUnmap)(IMG_HANDLE hBuffer);


/*************************************************************************/ /*!
@Function       BufferSystemAcquire

@Description    DEPRICATED, please use BufferAlloc
                Acquire the system buffer from the display driver.
                If the OS should trigger a mode change then it's not allowed to
                free the previous buffer until Services has released it
                via BufferSystemRelease.

   Called by client function: #PVRSRVDCSystemBufferAcquire()

   Implementation of this callback is optional.

@Input          hDeviceData             Device private data

@Output         puiLog2PageSize         The physical pagesize in log2(bytes)
                                        of one page that the buffer is composed of

@Output         pui32PageCount          The number of pages the buffer contains

@Output         pui32ByteStride         Byte stride of the buffer

@Output         phSystemBuffer          Handle to the buffer object

@Return         PVRSRV_OK if the query was successful
*/
/*****************************************************************************/
typedef PVRSRV_ERROR (*BufferSystemAcquire)(IMG_HANDLE hDeviceData,
                                            IMG_DEVMEM_LOG2ALIGN_T *puiLog2PageSize,
                                            IMG_UINT32 *pui32PageCount,
                                            IMG_UINT32 *pui32ByteStride,
                                            IMG_HANDLE *phSystemBuffer);

/*************************************************************************/ /*!
@Function       BufferSystemRelease

@Description    DEPRICATED, please use BufferFree
                Release a display buffer acquired with BufferSystemAcquire.
                Services calls this after it has no use for the buffer anymore.
                The buffer must not be destroyed before Services releases it
                with this call.

   Called by client function: #PVRSRVDCSystemBufferRelease()

   Implementation of this callback is optional.

@Input          hSystemBuffer          Handle to the buffer object
*/ /**************************************************************************/
typedef	void (*BufferSystemRelease)(IMG_HANDLE hSystemBuffer);

/*************************************************************************/ /*!
@Function       ResetDevice

@Description    Reset the device state. This function should perform actions
                required for getting the device to the working state. This can
                be for example used during resume operation from a hibernation.

                Implementation of this callback is optional.

@Input          hDeviceData             Device private data
*/ /**************************************************************************/
typedef PVRSRV_ERROR (*ResetDevice)(IMG_HANDLE hDeviceData);

#if defined(INTEGRITY_OS)
typedef PVRSRV_ERROR (*AcquireKernelMappingData)(IMG_HANDLE hBuffer, IMG_HANDLE *phMapping, void **ppPhysAddr);

#if (RGX_NUM_OS_SUPPORTED > 1)
typedef IMG_HANDLE (*GetPmr)(IMG_HANDLE hBuffer, size_t ulOffset);
#endif
#endif
/*!
 * Function table for functions to be implemented by the display controller
 * that will be called from within Services.
 * The table will be provided to Services with the call to DCRegisterDevice.
 *
 * Typedef: ::DC_DEVICE_FUNCTIONS
 */
typedef struct _DC_DEVICE_FUNCTIONS_
{
	/* Mandatory query functions */
	GetInfo                 pfnGetInfo; /*!< See #GetInfo */
	PanelQueryCount         pfnPanelQueryCount; /*!< See #PanelQueryCount */
	PanelQuery              pfnPanelQuery; /*!< See #PanelQuery */
	FormatQuery             pfnFormatQuery; /*!< See #FormatQuery */
	DimQuery                pfnDimQuery; /*!< See #DimQuery */

	/* Optional blank/vsync functions */
	SetBlank                pfnSetBlank; /*!< See #SetBlank */
	SetVSyncReporting       pfnSetVSyncReporting; /*!< See #SetVSyncReporting */
	LastVSyncQuery          pfnLastVSyncQuery; /*!< See #LastVSyncQuery */

	/* Mandatory configure functions */
	ContextCreate           pfnContextCreate; /*!< See #ContextCreate */
	ContextDestroy          pfnContextDestroy; /*!< See #ContextDestroy */
	ContextConfigure        pfnContextConfigure; /*!< See #ContextConfigure */

	/* Optional context functions */
	ContextConfigureCheck   pfnContextConfigureCheck; /*!< See #ContextConfigureCheck */

	/* Mandatory buffer functions */
	BufferAlloc             pfnBufferAlloc; /*!< See #BufferAlloc */
	BufferAcquire           pfnBufferAcquire; /*!< See #BufferAcquire */
	BufferRelease           pfnBufferRelease; /*!< See #BufferRelease */
	BufferFree              pfnBufferFree; /*!< See #BufferFree */

	/* Optional - Provide this function if your controller can
	 * scan out arbitrary memory, allocated for another purpose
	 * by Services. */
	BufferImport            pfnBufferImport; /*!< See #BufferImport */

	/* Optional - Provide these functions if your controller
	 * has an MMU and does not (or cannot) map/unmap buffers at
	 * alloc/free time */
	BufferMap               pfnBufferMap; /*!< See #BufferMap */
	BufferUnmap             pfnBufferUnmap; /*!< See #BufferUnmap */

	/* Optional - DEPRICATED */
	BufferSystemAcquire     pfnBufferSystemAcquire; /*!< See #BufferSystemAcquire */
	BufferSystemRelease     pfnBufferSystemRelease; /*!< See #BufferSystemRelease */

#if defined(INTEGRITY_OS)
	/* The addition of these functions allow dc_server to delegate calls to
	 * the respective functions on its PMRs towards the DC module
	 */
	AcquireKernelMappingData    pfnAcquireKernelMappingData;

#if (RGX_NUM_OS_SUPPORTED > 1)
	GetPmr                      pfnGetPmr;
#endif
#endif

   /* Optional - Provide this function if your controller should be allowed
    * to be reset by Services.
    * This can be used for example during suspend-to-RAM or suspend-to-disk
    * procedures. */
	ResetDevice                 pfnResetDevice; /*!< See #ResetDevice */
} DC_DEVICE_FUNCTIONS;


/*
 * Functions exported by kernel Services for use by 3rd party kernel display
 * controller device driver
*/

/*************************************************************************/ /*!
@Function       DCRegisterDevice

@Description    This needs to be called by the display driver before any further
                communication with Services.
                It registers a display controller device with Services. After this
                registration Services is able to use the display controller
                and will make use of the callbacks. Services will provide the
                hDeviceData in the callbacks whenever necessary.

@Input          psFuncTable             Callback function table

@Input          ui32MaxConfigsInFlight  The maximum number of configs that this
                                        display device can have in-flight. This
                                        determines the number of possible calls
                                        to ContextConfigure before Services has to
                                        wait for DCDisplayConfigurationRetired
                                        calls.

@Input          hDeviceData             Device private data passed into callbacks

@Output         phSrvHandle             Services handle to pass back into
                                        UnregisterDCDevice

@Return         PVRSRV_OK if the display controller driver was successfully registered
*/
/*****************************************************************************/
PVRSRV_ERROR DCRegisterDevice(DC_DEVICE_FUNCTIONS *psFuncTable,
                              IMG_UINT32 ui32MaxConfigsInFlight,
                              IMG_HANDLE hDeviceData,
                              IMG_HANDLE *phSrvHandle);

/*************************************************************************/ /*!
@Function       DCUnregisterDevice

@Description    Unregister a display controller device. Undo everything done with
                DCRegisterDevice. Services will stop using this device.

@Input          hSrvHandle              Services device handle
*/
/*****************************************************************************/
void DCUnregisterDevice(IMG_HANDLE hSrvHandle);

/*************************************************************************/ /*!
@Function       DCDisplayConfigurationRetired

@Description    Called when a configuration as been retired due to a new
                configuration now being active.

@Input          hConfigData             ConfigData that is being retired
*/
/*****************************************************************************/
void DCDisplayConfigurationRetired(IMG_HANDLE hConfigData);

/*************************************************************************/ /*!
@Function       DCDisplayHasPendingCommand

@Description    Called to check if there are still pending commands in
                the Software Command Processor queue.

@Input          hConfigData             ConfigData to check for pending
                                        commands

@Return         IMG_TRUE if there is at least one pending command
*/
/*****************************************************************************/
IMG_BOOL DCDisplayHasPendingCommand(IMG_HANDLE hConfigData);

/*************************************************************************/ /*!
@Function       DCImportBufferAcquire

@Description    Acquire information about a buffer that was imported with
                BufferImport. DCImportBufferRelease has to be called after
                the buffer will not be used anymore.

@Input          hImport                 Import buffer

@Input          uiLog2PageSize          Pagesize in log2(bytes) of the buffer

@Output         pui32PageCount          Size of the buffer in pages

@Output         pasDevPAddr             Array of device physical page address
                                        of this buffer

@Return         PVRSRV_OK if the import buffer was successfully acquired
*/
/*****************************************************************************/
PVRSRV_ERROR DCImportBufferAcquire(IMG_HANDLE hImport,
                                   IMG_DEVMEM_LOG2ALIGN_T uiLog2PageSize,
                                   IMG_UINT32 *pui32PageCount,
                                   IMG_DEV_PHYADDR **pasDevPAddr);

/*************************************************************************/ /*!
@Function       DCImportBufferRelease

@Description    Release an imported buffer.

@Input          hImport                 Import handle we are releasing

@Input          pasDevPAddr             Import data which was returned from
                                        DCImportBufferAcquire
*/
/*****************************************************************************/
void DCImportBufferRelease(IMG_HANDLE hImport,
                           IMG_DEV_PHYADDR *pasDevPAddr);



#if defined(__cplusplus)
}
#endif

#endif /* KERNELDISPLAY_H */

/******************************************************************************
 End of file (kerneldisplay.h)
******************************************************************************/
