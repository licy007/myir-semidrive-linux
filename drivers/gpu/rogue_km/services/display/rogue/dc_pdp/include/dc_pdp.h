/*************************************************************************/ /*!
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

#if !defined(DC_PDP_H)
#define DC_PDP_H

#include "dc_osfuncs.h"
#include "img_types.h"
#include "pvrsrv_error.h"

#define DRVNAME	"dc_pdp"

#define DCPDP_INTERRUPT_ID		(1)

#include "pdp_regs.h"

/*************************************************************************/ /*!
 dc_pdp OS functions
*/ /**************************************************************************/

typedef struct DCPDP_MODULE_PARAMETERS_TAG
{
	IMG_UINT32  ui32PDPEnabled;
	IMG_UINT32  ui32PDPWidth;
	IMG_UINT32  ui32PDPHeight;
} DCPDP_MODULE_PARAMETERS;

typedef enum DCPDP_ADDRESS_RANGE_TAG
{
	DCPDP_ADDRESS_RANGE_PDP = 0,
	DCPDP_ADDRESS_RANGE_PLL,
} DCPDP_ADDRESS_RANGE;

typedef struct DCPDP_DEVICE_PRIV_TAG DCPDP_DEVICE_PRIV;


const DCPDP_MODULE_PARAMETERS *DCPDPGetModuleParameters(void);

IMG_CPU_PHYADDR DCPDPGetAddrRangeStart(DCPDP_DEVICE_PRIV *psDevicePriv,
				       DCPDP_ADDRESS_RANGE eRange);
IMG_UINT32 DCPDPGetAddrRangeSize(DCPDP_DEVICE_PRIV *psDevicePriv,
				 DCPDP_ADDRESS_RANGE eRange);

PVRSRV_ERROR DCPDPInstallDeviceLISR(DCPDP_DEVICE_PRIV *psDevicePriv,
				    PFN_LISR pfnLISR, void *pvData);
PVRSRV_ERROR DCPDPUninstallDeviceLISR(DCPDP_DEVICE_PRIV *psDevicePriv);

/*******************************************************************************
 * dc_pdp common functions
 ******************************************************************************/

typedef struct DCPDP_DEVICE_TAG DCPDP_DEVICE;

PVRSRV_ERROR DCPDPInit(DCPDP_DEVICE_PRIV *psDevicePriv,
		       const DC_SERVICES_FUNCS *psServicesFuncs,
		       DCPDP_DEVICE **ppsDeviceData);
void DCPDPDeInit(DCPDP_DEVICE *psDeviceData, DCPDP_DEVICE_PRIV **ppsDevicePriv);

void DCPDPEnableMemoryRequest(DCPDP_DEVICE *psDeviceData, IMG_BOOL bEnable);

#endif /* !defined(DC_PDP_H) */
