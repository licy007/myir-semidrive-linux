/*******************************************************************************
@File
@Title          Common bridge header for dc
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures used by both the client
                and server side of the bridge for dc
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
*******************************************************************************/

#ifndef COMMON_DC_BRIDGE_H
#define COMMON_DC_BRIDGE_H

#include <powervr/mem_types.h>

#include "img_defs.h"
#include "img_types.h"
#include "pvrsrv_error.h"

#include "pvrsrv_surface.h"
#include "dc_external.h"
#include "pvrsrv_sync_km.h"

#define PVRSRV_BRIDGE_DC_CMD_FIRST			0
#define PVRSRV_BRIDGE_DC_DCDEVICESQUERYCOUNT			PVRSRV_BRIDGE_DC_CMD_FIRST+0
#define PVRSRV_BRIDGE_DC_DCDEVICESENUMERATE			PVRSRV_BRIDGE_DC_CMD_FIRST+1
#define PVRSRV_BRIDGE_DC_DCDEVICEACQUIRE			PVRSRV_BRIDGE_DC_CMD_FIRST+2
#define PVRSRV_BRIDGE_DC_DCDEVICERELEASE			PVRSRV_BRIDGE_DC_CMD_FIRST+3
#define PVRSRV_BRIDGE_DC_DCGETINFO			PVRSRV_BRIDGE_DC_CMD_FIRST+4
#define PVRSRV_BRIDGE_DC_DCPANELQUERYCOUNT			PVRSRV_BRIDGE_DC_CMD_FIRST+5
#define PVRSRV_BRIDGE_DC_DCPANELQUERY			PVRSRV_BRIDGE_DC_CMD_FIRST+6
#define PVRSRV_BRIDGE_DC_DCFORMATQUERY			PVRSRV_BRIDGE_DC_CMD_FIRST+7
#define PVRSRV_BRIDGE_DC_DCDIMQUERY			PVRSRV_BRIDGE_DC_CMD_FIRST+8
#define PVRSRV_BRIDGE_DC_DCSETBLANK			PVRSRV_BRIDGE_DC_CMD_FIRST+9
#define PVRSRV_BRIDGE_DC_DCSETVSYNCREPORTING			PVRSRV_BRIDGE_DC_CMD_FIRST+10
#define PVRSRV_BRIDGE_DC_DCLASTVSYNCQUERY			PVRSRV_BRIDGE_DC_CMD_FIRST+11
#define PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERACQUIRE			PVRSRV_BRIDGE_DC_CMD_FIRST+12
#define PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERRELEASE			PVRSRV_BRIDGE_DC_CMD_FIRST+13
#define PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCREATE			PVRSRV_BRIDGE_DC_CMD_FIRST+14
#define PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURECHECK			PVRSRV_BRIDGE_DC_CMD_FIRST+15
#define PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTDESTROY			PVRSRV_BRIDGE_DC_CMD_FIRST+16
#define PVRSRV_BRIDGE_DC_DCBUFFERALLOC			PVRSRV_BRIDGE_DC_CMD_FIRST+17
#define PVRSRV_BRIDGE_DC_DCBUFFERIMPORT			PVRSRV_BRIDGE_DC_CMD_FIRST+18
#define PVRSRV_BRIDGE_DC_DCBUFFERFREE			PVRSRV_BRIDGE_DC_CMD_FIRST+19
#define PVRSRV_BRIDGE_DC_DCBUFFERUNIMPORT			PVRSRV_BRIDGE_DC_CMD_FIRST+20
#define PVRSRV_BRIDGE_DC_DCBUFFERPIN			PVRSRV_BRIDGE_DC_CMD_FIRST+21
#define PVRSRV_BRIDGE_DC_DCBUFFERUNPIN			PVRSRV_BRIDGE_DC_CMD_FIRST+22
#define PVRSRV_BRIDGE_DC_DCBUFFERACQUIRE			PVRSRV_BRIDGE_DC_CMD_FIRST+23
#define PVRSRV_BRIDGE_DC_DCBUFFERRELEASE			PVRSRV_BRIDGE_DC_CMD_FIRST+24
#define PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURE2			PVRSRV_BRIDGE_DC_CMD_FIRST+25
#define PVRSRV_BRIDGE_DC_CMD_LAST			(PVRSRV_BRIDGE_DC_CMD_FIRST+25)

/*******************************************
            DCDevicesQueryCount
 *******************************************/

/* Bridge in structure for DCDevicesQueryCount */
typedef struct PVRSRV_BRIDGE_IN_DCDEVICESQUERYCOUNT_TAG
{
	IMG_UINT32 ui32EmptyStructPlaceholder;
} __packed PVRSRV_BRIDGE_IN_DCDEVICESQUERYCOUNT;

/* Bridge out structure for DCDevicesQueryCount */
typedef struct PVRSRV_BRIDGE_OUT_DCDEVICESQUERYCOUNT_TAG
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32DeviceCount;
} __packed PVRSRV_BRIDGE_OUT_DCDEVICESQUERYCOUNT;

/*******************************************
            DCDevicesEnumerate
 *******************************************/

/* Bridge in structure for DCDevicesEnumerate */
typedef struct PVRSRV_BRIDGE_IN_DCDEVICESENUMERATE_TAG
{
	IMG_UINT32 *pui32DeviceIndex;
	IMG_UINT32 ui32DeviceArraySize;
} __packed PVRSRV_BRIDGE_IN_DCDEVICESENUMERATE;

/* Bridge out structure for DCDevicesEnumerate */
typedef struct PVRSRV_BRIDGE_OUT_DCDEVICESENUMERATE_TAG
{
	IMG_UINT32 *pui32DeviceIndex;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32DeviceCount;
} __packed PVRSRV_BRIDGE_OUT_DCDEVICESENUMERATE;

/*******************************************
            DCDeviceAcquire
 *******************************************/

/* Bridge in structure for DCDeviceAcquire */
typedef struct PVRSRV_BRIDGE_IN_DCDEVICEACQUIRE_TAG
{
	IMG_UINT32 ui32DeviceIndex;
} __packed PVRSRV_BRIDGE_IN_DCDEVICEACQUIRE;

/* Bridge out structure for DCDeviceAcquire */
typedef struct PVRSRV_BRIDGE_OUT_DCDEVICEACQUIRE_TAG
{
	IMG_HANDLE hDevice;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCDEVICEACQUIRE;

/*******************************************
            DCDeviceRelease
 *******************************************/

/* Bridge in structure for DCDeviceRelease */
typedef struct PVRSRV_BRIDGE_IN_DCDEVICERELEASE_TAG
{
	IMG_HANDLE hDevice;
} __packed PVRSRV_BRIDGE_IN_DCDEVICERELEASE;

/* Bridge out structure for DCDeviceRelease */
typedef struct PVRSRV_BRIDGE_OUT_DCDEVICERELEASE_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCDEVICERELEASE;

/*******************************************
            DCGetInfo
 *******************************************/

/* Bridge in structure for DCGetInfo */
typedef struct PVRSRV_BRIDGE_IN_DCGETINFO_TAG
{
	IMG_HANDLE hDevice;
} __packed PVRSRV_BRIDGE_IN_DCGETINFO;

/* Bridge out structure for DCGetInfo */
typedef struct PVRSRV_BRIDGE_OUT_DCGETINFO_TAG
{
	DC_DISPLAY_INFO sDisplayInfo;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCGETINFO;

/*******************************************
            DCPanelQueryCount
 *******************************************/

/* Bridge in structure for DCPanelQueryCount */
typedef struct PVRSRV_BRIDGE_IN_DCPANELQUERYCOUNT_TAG
{
	IMG_HANDLE hDevice;
} __packed PVRSRV_BRIDGE_IN_DCPANELQUERYCOUNT;

/* Bridge out structure for DCPanelQueryCount */
typedef struct PVRSRV_BRIDGE_OUT_DCPANELQUERYCOUNT_TAG
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32NumPanels;
} __packed PVRSRV_BRIDGE_OUT_DCPANELQUERYCOUNT;

/*******************************************
            DCPanelQuery
 *******************************************/

/* Bridge in structure for DCPanelQuery */
typedef struct PVRSRV_BRIDGE_IN_DCPANELQUERY_TAG
{
	IMG_HANDLE hDevice;
	PVRSRV_PANEL_INFO *psPanelInfo;
	IMG_UINT32 ui32PanelsArraySize;
} __packed PVRSRV_BRIDGE_IN_DCPANELQUERY;

/* Bridge out structure for DCPanelQuery */
typedef struct PVRSRV_BRIDGE_OUT_DCPANELQUERY_TAG
{
	PVRSRV_PANEL_INFO *psPanelInfo;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32NumPanels;
} __packed PVRSRV_BRIDGE_OUT_DCPANELQUERY;

/*******************************************
            DCFormatQuery
 *******************************************/

/* Bridge in structure for DCFormatQuery */
typedef struct PVRSRV_BRIDGE_IN_DCFORMATQUERY_TAG
{
	IMG_HANDLE hDevice;
	PVRSRV_SURFACE_FORMAT *psFormat;
	IMG_UINT32 *pui32Supported;
	IMG_UINT32 ui32NumFormats;
} __packed PVRSRV_BRIDGE_IN_DCFORMATQUERY;

/* Bridge out structure for DCFormatQuery */
typedef struct PVRSRV_BRIDGE_OUT_DCFORMATQUERY_TAG
{
	IMG_UINT32 *pui32Supported;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCFORMATQUERY;

/*******************************************
            DCDimQuery
 *******************************************/

/* Bridge in structure for DCDimQuery */
typedef struct PVRSRV_BRIDGE_IN_DCDIMQUERY_TAG
{
	IMG_HANDLE hDevice;
	PVRSRV_SURFACE_DIMS *psDim;
	IMG_UINT32 *pui32Supported;
	IMG_UINT32 ui32NumDims;
} __packed PVRSRV_BRIDGE_IN_DCDIMQUERY;

/* Bridge out structure for DCDimQuery */
typedef struct PVRSRV_BRIDGE_OUT_DCDIMQUERY_TAG
{
	IMG_UINT32 *pui32Supported;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCDIMQUERY;

/*******************************************
            DCSetBlank
 *******************************************/

/* Bridge in structure for DCSetBlank */
typedef struct PVRSRV_BRIDGE_IN_DCSETBLANK_TAG
{
	IMG_HANDLE hDevice;
	IMG_BOOL bEnabled;
} __packed PVRSRV_BRIDGE_IN_DCSETBLANK;

/* Bridge out structure for DCSetBlank */
typedef struct PVRSRV_BRIDGE_OUT_DCSETBLANK_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCSETBLANK;

/*******************************************
            DCSetVSyncReporting
 *******************************************/

/* Bridge in structure for DCSetVSyncReporting */
typedef struct PVRSRV_BRIDGE_IN_DCSETVSYNCREPORTING_TAG
{
	IMG_HANDLE hDevice;
	IMG_BOOL bEnabled;
} __packed PVRSRV_BRIDGE_IN_DCSETVSYNCREPORTING;

/* Bridge out structure for DCSetVSyncReporting */
typedef struct PVRSRV_BRIDGE_OUT_DCSETVSYNCREPORTING_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCSETVSYNCREPORTING;

/*******************************************
            DCLastVSyncQuery
 *******************************************/

/* Bridge in structure for DCLastVSyncQuery */
typedef struct PVRSRV_BRIDGE_IN_DCLASTVSYNCQUERY_TAG
{
	IMG_HANDLE hDevice;
} __packed PVRSRV_BRIDGE_IN_DCLASTVSYNCQUERY;

/* Bridge out structure for DCLastVSyncQuery */
typedef struct PVRSRV_BRIDGE_OUT_DCLASTVSYNCQUERY_TAG
{
	IMG_INT64 i64Timestamp;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCLASTVSYNCQUERY;

/*******************************************
            DCSystemBufferAcquire
 *******************************************/

/* Bridge in structure for DCSystemBufferAcquire */
typedef struct PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERACQUIRE_TAG
{
	IMG_HANDLE hDevice;
} __packed PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERACQUIRE;

/* Bridge out structure for DCSystemBufferAcquire */
typedef struct PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERACQUIRE_TAG
{
	IMG_HANDLE hBuffer;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32Stride;
} __packed PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERACQUIRE;

/*******************************************
            DCSystemBufferRelease
 *******************************************/

/* Bridge in structure for DCSystemBufferRelease */
typedef struct PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERRELEASE_TAG
{
	IMG_HANDLE hBuffer;
} __packed PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERRELEASE;

/* Bridge out structure for DCSystemBufferRelease */
typedef struct PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERRELEASE_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERRELEASE;

/*******************************************
            DCDisplayContextCreate
 *******************************************/

/* Bridge in structure for DCDisplayContextCreate */
typedef struct PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCREATE_TAG
{
	IMG_HANDLE hDevice;
} __packed PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCREATE;

/* Bridge out structure for DCDisplayContextCreate */
typedef struct PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCREATE_TAG
{
	IMG_HANDLE hDisplayContext;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCREATE;

/*******************************************
            DCDisplayContextConfigureCheck
 *******************************************/

/* Bridge in structure for DCDisplayContextConfigureCheck */
typedef struct PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURECHECK_TAG
{
	IMG_HANDLE hDisplayContext;
	PVRSRV_SURFACE_CONFIG_INFO *psSurfInfo;
	IMG_HANDLE *phBuffers;
	IMG_UINT32 ui32PipeCount;
} __packed PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURECHECK;

/* Bridge out structure for DCDisplayContextConfigureCheck */
typedef struct PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURECHECK_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURECHECK;

/*******************************************
            DCDisplayContextDestroy
 *******************************************/

/* Bridge in structure for DCDisplayContextDestroy */
typedef struct PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTDESTROY_TAG
{
	IMG_HANDLE hDisplayContext;
} __packed PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTDESTROY;

/* Bridge out structure for DCDisplayContextDestroy */
typedef struct PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTDESTROY_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTDESTROY;

/*******************************************
            DCBufferAlloc
 *******************************************/

/* Bridge in structure for DCBufferAlloc */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERALLOC_TAG
{
	IMG_HANDLE hDisplayContext;
	DC_BUFFER_CREATE_INFO sSurfInfo;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERALLOC;

/* Bridge out structure for DCBufferAlloc */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERALLOC_TAG
{
	IMG_HANDLE hBuffer;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32Stride;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERALLOC;

/*******************************************
            DCBufferImport
 *******************************************/

/* Bridge in structure for DCBufferImport */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERIMPORT_TAG
{
	IMG_HANDLE hDisplayContext;
	IMG_HANDLE *phImport;
	DC_BUFFER_IMPORT_INFO sSurfAttrib;
	IMG_UINT32 ui32NumPlanes;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERIMPORT;

/* Bridge out structure for DCBufferImport */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERIMPORT_TAG
{
	IMG_HANDLE hBuffer;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERIMPORT;

/*******************************************
            DCBufferFree
 *******************************************/

/* Bridge in structure for DCBufferFree */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERFREE_TAG
{
	IMG_HANDLE hBuffer;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERFREE;

/* Bridge out structure for DCBufferFree */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERFREE_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERFREE;

/*******************************************
            DCBufferUnimport
 *******************************************/

/* Bridge in structure for DCBufferUnimport */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERUNIMPORT_TAG
{
	IMG_HANDLE hBuffer;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERUNIMPORT;

/* Bridge out structure for DCBufferUnimport */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERUNIMPORT_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERUNIMPORT;

/*******************************************
            DCBufferPin
 *******************************************/

/* Bridge in structure for DCBufferPin */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERPIN_TAG
{
	IMG_HANDLE hBuffer;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERPIN;

/* Bridge out structure for DCBufferPin */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERPIN_TAG
{
	IMG_HANDLE hPinHandle;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERPIN;

/*******************************************
            DCBufferUnpin
 *******************************************/

/* Bridge in structure for DCBufferUnpin */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERUNPIN_TAG
{
	IMG_HANDLE hPinHandle;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERUNPIN;

/* Bridge out structure for DCBufferUnpin */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERUNPIN_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERUNPIN;

/*******************************************
            DCBufferAcquire
 *******************************************/

/* Bridge in structure for DCBufferAcquire */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERACQUIRE_TAG
{
	IMG_HANDLE hBuffer;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERACQUIRE;

/* Bridge out structure for DCBufferAcquire */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERACQUIRE_TAG
{
	IMG_HANDLE hExtMem;
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERACQUIRE;

/*******************************************
            DCBufferRelease
 *******************************************/

/* Bridge in structure for DCBufferRelease */
typedef struct PVRSRV_BRIDGE_IN_DCBUFFERRELEASE_TAG
{
	IMG_HANDLE hExtMem;
} __packed PVRSRV_BRIDGE_IN_DCBUFFERRELEASE;

/* Bridge out structure for DCBufferRelease */
typedef struct PVRSRV_BRIDGE_OUT_DCBUFFERRELEASE_TAG
{
	PVRSRV_ERROR eError;
} __packed PVRSRV_BRIDGE_OUT_DCBUFFERRELEASE;

/*******************************************
            DCDisplayContextConfigure2
 *******************************************/

/* Bridge in structure for DCDisplayContextConfigure2 */
typedef struct PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURE2_TAG
{
	IMG_HANDLE hDisplayContext;
	PVRSRV_SURFACE_CONFIG_INFO *psSurfInfo;
	IMG_HANDLE *phBuffers;
	PVRSRV_FENCE hAcquireFence;
	PVRSRV_TIMELINE hReleaseFenceTimeline;
	IMG_UINT32 ui32DisplayPeriod;
	IMG_UINT32 ui32MaxDepth;
	IMG_UINT32 ui32PipeCount;
} __packed PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURE2;

/* Bridge out structure for DCDisplayContextConfigure2 */
typedef struct PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURE2_TAG
{
	PVRSRV_ERROR eError;
	PVRSRV_FENCE hReleaseFence;
} __packed PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURE2;

#endif /* COMMON_DC_BRIDGE_H */
