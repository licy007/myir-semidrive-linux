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

@Copyright      Portions Copyright (c) Synopsys Ltd. All Rights Reserved
@License        Synopsys Permissive License

The Synopsys Software Driver and documentation (hereinafter "Software")
is an unsupported proprietary work of Synopsys, Inc. unless otherwise
expressly agreed to in writing between  Synopsys and you.

The Software IS NOT an item of Licensed Software or Licensed Product under
any End User Software License Agreement or Agreement for Licensed Product
with Synopsys or any supplement thereto.  Permission is hereby granted,
free of charge, to any person obtaining a copy of this software annotated
with this license and the Software, to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so, subject
to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT     LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.
*/ /**************************************************************************/

#include "edid.h"
#include "phy.h"
#include "img_defs.h"

/* Section 2.12.4 of HDMI TX spec */
const IMG_UINT32 I2C_DIV_FACTOR = 100000;
const IMG_UINT16 EDID_I2C_MIN_FS_SCL_HIGH_TIME = 600;
const IMG_UINT16 EDID_I2C_MIN_FS_SCL_LOW_TIME = 1300;
const IMG_UINT16 EDID_I2C_MIN_SS_SCL_HIGH_TIME = 40000;
const IMG_UINT16 EDID_I2C_MIN_SS_SCL_LOW_TIME = 47000;
const IMG_UINT16 EDID_SFR_CLOCK = 2700;

#ifdef HDMI_DEBUG

//#define HDMI_TEST_EDID 1

#ifdef HDMI_TEST_EDID
IMG_UINT8 gSampleEdid[128] = {0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
0x10,0xAC,0x26,0xA0,0x53,0x34,0x43,0x32, // 8
0x31,0x11,0x01,0x03,0x80,0x34,0x21,0x78, // 16
0xEE,0x9B,0xB5,0xA9,0x54,0x34,0xAE,0x25, // 24
0x13,0x50,0x54,0xA5,0x4B,0x00,0x81,0x80, // 32
0xA9,0x40,0x71,0x4F,0xB3,0x00,0x01,0x01, // 40
0x01,0x01,0x01,0x01,0x01,0x01,0x28,0x3C, // 48
0x80,0xA0,0x70,0xB0,0x23,0x40,0x30,0x20, // 56
0x36,0x00,0x07,0x44,0x21,0x00,0x00,0x1A, // 64
0x00,0x00,0x00,0xFF,0x00,0x47,0x4D,0x35, // 72
0x30,0x34,0x37,0x43,0x34,0x32,0x43,0x34, // 80
0x53,0x20,0x00,0x00,0x00,0xFC,0x00,0x44, // 88
0x45,0x4C,0x4C,0x32,0x34,0x30,0x37,0x57, // 96
0x46,0x50,0x48,0x43,0x00,0x00,0x00,0xFD, // 104
0x00,0x38,0x4C,0x1E,0x53,0x11,0x00,0x0A, // 112
0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x6F}; // 120
#endif

#endif


#if defined(HDMI_ENABLE_FAST_COUNT)
/* calculate the fast speed high time counter - round up */
static IMG_UINT16 EdidCalcCount(IMG_UINT16 sclMinTime)
{
	unsigned long tmpSclPeriod = 0;
	if (((EDID_SFR_CLOCK * sclMinTime) % I2C_DIV_FACTOR) != 0)
	{
		tmpSclPeriod = (unsigned long)((EDID_SFR_CLOCK * sclMinTime) + (I2C_DIV_FACTOR - ((EDID_SFR_CLOCK * sclMinTime) % I2C_DIV_FACTOR))) / I2C_DIV_FACTOR;
	}
	else
	{
		tmpSclPeriod = (unsigned long)(EDID_SFR_CLOCK * sclMinTime) / I2C_DIV_FACTOR;
	}
	return (IMG_UINT16)(tmpSclPeriod);
}
#endif

static PVRSRV_ERROR EdidFindBaseBlock(IMG_UINT8 * buffer, IMG_UINT32 size, EDID_BASE_BLOCK * pBase)
{
    IMG_UINT32 index = 0;

    HDMI_CHECKPOINT;

    /* First find the EDID signature and fill in global structure */
    for (index = 0; index < size; index++)
    {
        if (*((IMG_UINT64*)&buffer[index]) == EDID_SIGNATURE)
        {
            HDMI_DEBUG_PRINT("- %s: Found EDID signature at offset %d\n", __func__, index);
            break;
        }
    }

    if (index == size)
    {
        HDMI_ERROR_PRINT("- %s: Could not find EDID signature\n", __func__);
        return PVRSRV_ERROR_NOT_FOUND;
    }

    /* Copy to global EDID buffer */
    if (index + EDID_BLOCK_SIZE > size)
    {
        HDMI_ERROR_PRINT("- %s: Found signature but it spills over into next block?!?!\n", __func__);
        return PVRSRV_ERROR_OUT_OF_MEMORY;
    }
    memcpy(pBase, &buffer[index], EDID_BLOCK_SIZE);

    HDMI_DEBUG_PRINT("- %s EDID Version %d, Revision ID: %d", __func__, pBase->EdidVersion, pBase->EdidRevision);

#ifdef HDMI_DEBUG
    {
        IMG_UINT8* pData = (IMG_UINT8*)pBase;
        IMG_UINT32 i = 0;
        HDMI_DEBUG_PRINT("- %s: EDID Data:\n\n", __func__);
        for (i = 0; i < EDID_BLOCK_SIZE; i+=8)
        {
            HDMI_DEBUG_PRINT(" Offset %3d: %02x %02x %02x %02x %02x %02x %02x %02x\n", i,
                       pData[i], pData[i+1], pData[i+2], pData[i+3], pData[i+4], pData[i+5], pData[i+6], pData[i+7]);
        }
    }
#endif

    return PVRSRV_OK;

}

static PVRSRV_ERROR EdidParseDtd(IMG_UINT8 * pDesc, VIDEO_PARAMS * pVideoParams)
{
    IMG_UINT8 index = pVideoParams->mDtdIndex;
    DTD_RAW * dtd = (DTD_RAW*)pDesc;
    DTD decodedDtd = {0};

    HDMI_CHECKPOINT;

    if (index >= HDMI_DTD_MAX)
    {
        HDMI_ERROR_PRINT("- %s: Ran out of space for DTD\n", __func__);
        return PVRSRV_ERROR_OUT_OF_MEMORY;
    }


    decodedDtd.mCode = -1;
    decodedDtd.mPixelRepetitionInput = 0;

    decodedDtd.mPixelClock = dtd->pixelClock; /*  [10000Hz] */
    if (decodedDtd.mPixelClock < 0x01)
    { /* 0x0000 is defined as reserved */
        return PVRSRV_ERROR_INVALID_PARAMS;
    }

    decodedDtd.mHActive = dtd->hActiveLower | (dtd->hActiveUpper << 8);
    decodedDtd.mHBlanking = dtd->hBlankingLower | (dtd->hBlankingUpper << 8);
    decodedDtd.mHFrontPorchWidth = dtd->hFrontPorchLower | (dtd->hFrontPorchUpper << 8);
    decodedDtd.mHSyncPulseWidth = dtd->hSyncPulseWidthLower | (dtd->hSyncPulseWidthUpper << 8);
    decodedDtd.mHImageSize = dtd->hImageSizeLower | (dtd->hImageSizeUpper << 8);
    decodedDtd.mHBorder = dtd->hBorder;
    decodedDtd.mHBackPorchWidth = decodedDtd.mHBlanking - decodedDtd.mHSyncPulseWidth - decodedDtd.mHFrontPorchWidth;

    decodedDtd.mVActive = dtd->vActiveLower | (dtd->vActiveUpper << 8);
    decodedDtd.mVBlanking = dtd->vBlankingLower | (dtd->vBlankingUpper << 8);
    decodedDtd.mVFrontPorchWidth = dtd->vFrontPorchLower | (dtd->vFrontPorchUpper << 8);
    decodedDtd.mVSyncPulseWidth = dtd->vSyncPulseWidthLower | (dtd->vSyncPulseWidthUpper << 8);
    decodedDtd.mVImageSize = dtd->vImageSizeLower | (dtd->vImageSizeUpper << 8);
    decodedDtd.mVBorder = dtd->vBorder;
    decodedDtd.mVBackPorchWidth = decodedDtd.mVBlanking - decodedDtd.mVSyncPulseWidth - decodedDtd.mVFrontPorchWidth;

    /* no stereo viewing support in HDMI */
    decodedDtd.mInterlaced = dtd->interlaced;
    HDMI_DEBUG_PRINT("- %s: sync signal defs: %d", __func__, dtd->syncSignalDefs);
    if ((dtd->syncSignalDefs >> 3) & 0x01)
    {
        decodedDtd.mVSyncPolarity = (dtd->syncSignalDefs >> 1) & 0x01; // BIT 1
        decodedDtd.mHSyncPolarity = (dtd->syncSignalDefs >> 0) & 0x01; // BIT 0
    }

    HDMI_DEBUG_PRINT("Decoded DTD %d with pixel clock %dHz, Horz (%d, %d, %d, %d, %d, %d, %d), Vert (%d, %d, %d, %d, %d, %d, %d)\n", index, decodedDtd.mPixelClock*10000,
        decodedDtd.mHActive, decodedDtd.mHBlanking, decodedDtd.mHBorder, decodedDtd.mHImageSize, decodedDtd.mHFrontPorchWidth, decodedDtd.mHBackPorchWidth, decodedDtd.mHSyncPulseWidth,
        decodedDtd.mVActive, decodedDtd.mVBlanking, decodedDtd.mVBorder, decodedDtd.mVImageSize, decodedDtd.mVFrontPorchWidth, decodedDtd.mVBackPorchWidth, decodedDtd.mVSyncPulseWidth);

#if defined(HDMI_EXPERIMENTAL_CLOCKS)
    pVideoParams->mDtdList[index] = decodedDtd;
    pVideoParams->mDtdIndex++;
#else
    if (PhyIsPclkSupported(dtd->pixelClock))
    {
        pVideoParams->mDtdList[index] = decodedDtd;
        pVideoParams->mDtdIndex++;
    }
    else
    {
        HDMI_DEBUG_PRINT("- %s: Found unsupported pixel clock (%d) in DTD, not adding", __func__, dtd->pixelClock);
    }
#endif

    return PVRSRV_OK;
}

static inline IMG_UINT8 GetDescriptorType(IMG_UINT8 * pDesc)
{
    if (pDesc[0] == 0 && pDesc[1] == 0 && pDesc[2] == 0)
    {
        return EDID_DESC_DISPLAY;
    }
    else
    {
        return EDID_DESC_DETAILED_TIMING;
    }
}

static PVRSRV_ERROR EdidParseDispDesc(IMG_UINT8 * pDesc, VIDEO_PARAMS * pVideoParams)
{
    DISPLAY_DESCRIPTOR * pDispDesc = (DISPLAY_DESCRIPTOR*)pDesc;

    /* Confirm this is a display descriptor */
    if (pDispDesc->DescTag[0] != 0 || pDispDesc->DescTag[1] != 0 || pDispDesc->DescTag[2] != 0)
    {
        return PVRSRV_ERROR_INVALID_PARAMS;
    }

    HDMI_CHECKPOINT;

    HDMI_DEBUG_PRINT(" Display descriptor tag: %x\n", pDispDesc->DispDescTag);

    switch (pDispDesc->DispDescTag)
    {
        case DISP_DESC_TAG_PRODUCT_NAME:
        {
            IMG_UINT8 monitorName[14] = { 0 }; // Up to 13 characters
            IMG_UINT8 i = 0;

            for (i = 0; i < 13; i++)
            {
                if (pDispDesc->Data[i] != 0x0A) // 0A is terminator
                {
                    monitorName[i] = pDispDesc->Data[i];
                }
            }

            /* Just print for now */
            HDMI_DEBUG_PRINT(" Found monitor name descriptor: %s", monitorName);
            break;
        }
        case DISP_DESC_TAG_RANGE_LIMITS:
        {
            /* Only fill range limits if offsets are 0 */
            MONITOR_RANGE_DATA * pData = (MONITOR_RANGE_DATA*)&pDispDesc->Reserved; // This one is unique, no reserved field
            if (pData->offsetFlags == 0)
            {
                HDMI_DEBUG_PRINT("- %s: Found valid monitor range limits, vMin: %x, vMax: %x, hMin: %x, hMax: %x, maxPClock: %x, support flags: %x\n",
								 __func__, pData->vMinRate, pData->vMaxRate, pData->hMinRate, pData->hMaxRate, pData->maxPixelClock, pData->supportFlags);
                pVideoParams->mRangeLimits.vMinRate = pData->vMinRate;
                pVideoParams->mRangeLimits.vMaxRate = pData->vMaxRate;
                pVideoParams->mRangeLimits.hMinRate = pData->hMinRate;
                pVideoParams->mRangeLimits.hMaxRate = pData->hMaxRate;
                pVideoParams->mRangeLimits.maxPixelClock = pData->maxPixelClock;
                pVideoParams->mRangeLimits.valid = true;
            }
            break;
        }
        case DISP_DESC_TAG_EST_TIMING_3:
        case DISP_DESC_TAG_CVT_3:
        case DISP_DESC_TAG_DCM:
        case DISP_DESC_TAG_STD_TIMING:
        case DISP_DESC_TAG_COLOR_POINT_DATA:
        case DISP_DESC_TAG_DATA_STRING:
        case DISP_DESC_TAG_SERIAL_NUMBER:
            /* Don't care */
            break;
        default:
            HDMI_ERROR_PRINT("- %s: Invalid display descriptor tag\n", __func__);
            return PVRSRV_ERROR_INVALID_PARAMS;
    }

    return PVRSRV_OK;
}

static PVRSRV_ERROR EdidParseDescriptor(IMG_UINT8 * pDesc, VIDEO_PARAMS * pVideoParams)
{
    PVRSRV_ERROR status = PVRSRV_OK;
    IMG_UINT8 dtdDesc = 0;

    HDMI_CHECKPOINT;

    dtdDesc = GetDescriptorType(pDesc);

    if (dtdDesc == EDID_DESC_DETAILED_TIMING)
    {
        status = EdidParseDtd(pDesc, pVideoParams);
        if (status != PVRSRV_OK)
        {
            HDMI_ERROR_PRINT("- %s: EDID failed to parse descriptor. Status: %d\n", __func__, status);
            return status;
        }
    }
    else if (dtdDesc == EDID_DESC_DISPLAY)
    {
        status = EdidParseDispDesc(pDesc, pVideoParams);
        if (status != PVRSRV_OK)
        {
            HDMI_ERROR_PRINT("- %s: EDID failed to parse descriptor. Status: %d\n", __func__, status);
            return status;
        }
    }

    return status;

}

static IMG_UINT8 EdidGetColorResolution(VIDEO_INPUT_DEFINITION videoInput)
{
	IMG_UINT8 colorRes = 0;

	switch (videoInput.digital.colorBitDepth)
    {
		case 1:
            colorRes = 6;
            break;
        case 2:
            colorRes = 8;
            break;
        case 3:
            colorRes = 10;
            break;
        case 4:
            colorRes = 12;
            break;
        case 5:
            colorRes = 14;
            break;
        case 6:
            colorRes = 16;
            break;
        default:
            HDMI_DEBUG_PRINT("- %s: color bit depth undefined!", __func__);
            break;
    }

    return colorRes;
}

static PVRSRV_ERROR EdidParseBaseBlock(EDID_BASE_BLOCK * edidBlock, VIDEO_PARAMS * pVideoParams)
{
    PVRSRV_ERROR status = PVRSRV_OK;
    IMG_UINT8 index = 0;
    IMG_UINT8 checkSum = 0;
    IMG_UINT8 * buffer = (IMG_UINT8*)edidBlock;

    HDMI_CHECKPOINT;

    /* Checksum */
    for (index = 0; index < EDID_BLOCK_SIZE; index++)
    {
        checkSum = (IMG_UINT8) (checkSum + buffer[index]);
    }

    if (checkSum != EDID_CHECKSUM)
    {
        HDMI_ERROR_PRINT("- %s: EDID checksum failed, computed checksum: %d\n", __func__, checkSum);
        status = PVRSRV_ERROR_DC_INVALID_CONFIG;
        goto EXIT;
    }

    /* Extract color resolution */
    HDMI_DEBUG_PRINT("- %s: Video Input definition 0x%x, Feature Support 0x%x", __func__, edidBlock->VideoInputDefinition.value, edidBlock->FeatureSupport.value);
    if (edidBlock->VideoInputDefinition.digital.signalInterface == 1)
    {
        if (edidBlock->VideoInputDefinition.digital.colorBitDepth != 0)
        {
            HDMI_DEBUG_PRINT("- %s: Setting color resolution to %d", __func__, EdidGetColorResolution(edidBlock->VideoInputDefinition));
            pVideoParams->mColorResolution = EdidGetColorResolution(edidBlock->VideoInputDefinition);
        }
        /* Otherwise grab from feature support flags */
        else
        {
            /* RGB 4:4:4 must be supported */
            pVideoParams->mColorResolution = 8;
        }

#if 0
        if (edidBlock->VideoInputDefinition.digital.signalInterface == 1)
#else
		if (edidBlock->VideoInputDefinition.digital.vidInterfaceSupported == DIGITAL_VIDEO_INTERFACE_HDMI_A ||
			edidBlock->VideoInputDefinition.digital.vidInterfaceSupported == DIGITAL_VIDEO_INTERFACE_HDMI_B)
#endif
		{
			pVideoParams->mHdmi = 1;
		}
    }

    /* First DTD is preferred timing (VESA 1.4) */
    if (edidBlock->FeatureSupport.fields.preferredTimingIncluded)
    {
        HDMI_DEBUG_PRINT("- %s: Found preferred timing descriptor", __func__);
        status = EdidParseDtd((IMG_UINT8*)&edidBlock->PreferredTimingMode, pVideoParams);
        if (status != PVRSRV_OK)
        {
            HDMI_ERROR_PRINT("- %s: EDID failed to parse preferred timing DTD. Status: %d\n", __func__, status);
            goto EXIT;
        }

        pVideoParams->mPreferredTimingIncluded = true;
    }

    HDMI_CHECKPOINT;

    /* Parse remaining descriptors, could be DTD or display descriptor (first 3 bytes = 0) */
    status = EdidParseDescriptor(edidBlock->Descriptor2, pVideoParams);
    CHECK_AND_EXIT(status, EXIT);

    status = EdidParseDescriptor(edidBlock->Descriptor3, pVideoParams);
    CHECK_AND_EXIT(status, EXIT);

    status = EdidParseDescriptor(edidBlock->Descriptor4, pVideoParams);
    CHECK_AND_EXIT(status, EXIT);

    HDMI_CHECKPOINT;

EXIT:

    return PVRSRV_OK;
}

static void EdidParseCeaDataBlock(VIDEO_PARAMS * pVideoParams, IMG_UINT8 * data, IMG_UINT32 * length)
{
    IMG_UINT8 tag;
    IMG_UINT8 nBlocks;
    IMG_UINT8 i;

    HDMI_CHECKPOINT;

    // Upper 3 bits of first byte are the tag
    tag = data[0] >> 5;
    // Bottom 5 bits of first byte are the number of blocks
    nBlocks = data[0] & 0x1F;

    HDMI_DEBUG_PRINT("- %s: Parsing CEA data block, tag: %d, nBlocks: %d\n", __func__, tag, nBlocks);

    switch (tag)
    {
        case EDID_CEA_DATA_BLOCK_VIDEO_TAG:
            for (i = 1; i < (nBlocks + 1); i++)
            {
                HDMI_DEBUG_PRINT("- %s: Native: %d, VIC: %d\n", __func__, data[i] >> 7, data[i] & 0x7F);
                EdidAddDtd(pVideoParams, data[i] & 0x7F, EDID_REFRESH_RATE);
                /* If its a native setup, then make it the active */
                if (data[i] >> 7 == 1 && pVideoParams->mPreferredTimingIncluded == false)
                {
                    pVideoParams->mDtdActiveIndex = pVideoParams->mDtdIndex - 1;
                }
            }
            break;
        case EDID_CEA_DATA_BLOCK_VENDOR_TAG:
        case EDID_CEA_DATA_BLOCK_AUDIO_TAG:
        case EDID_CEA_DATA_BLOCK_SPEAKER_TAG:
        default:
            // don't care
            break;
    }

    *length = nBlocks + 1;

}

static PVRSRV_ERROR EdidParseCeaBlock(IMG_UINT8 * data, VIDEO_PARAMS * pVideoParams)
{
    EDID_EXT_CEA * pCeaBlock = (EDID_EXT_CEA*)data;
    IMG_UINT8 i = 0, j = 0;
    IMG_UINT32 length; // of each CEA data block

    HDMI_CHECKPOINT;

    HDMI_DEBUG_PRINT("- %s: Parsing CEA extension block, DTD Offset: %d, total size: %d", __func__, pCeaBlock->DtdOffset, sizeof(pCeaBlock->Data));

    /* Only support revision 3 */
    if (pCeaBlock->Revision != EDID_CEA_REVISION)
    {
        HDMI_ERROR_PRINT("- %s: Found unsupported CEA block revision number (%d)\n", __func__, pCeaBlock->Revision);
        return PVRSRV_ERROR_NOT_FOUND;
    }

    /* First parse data blocks */
    for (i = 0; i < (pCeaBlock->DtdOffset + 4); i+= length)
    {
        EdidParseCeaDataBlock(pVideoParams, &pCeaBlock->Data[i], &length);
    }

    /* Now parse DTDs (max = 6)*/
    for (i = pCeaBlock->DtdOffset - 4, j = 0; i < sizeof(pCeaBlock->Data) && j < 6; i += sizeof(DTD_RAW), j++)
    {
        EdidParseDtd(&pCeaBlock->Data[i], pVideoParams);
    }

    return PVRSRV_OK;
}

static PVRSRV_ERROR EdidParseExtBlock(IMG_UINT8 * edidBlock, VIDEO_PARAMS * pVideoParams)
{
    PVRSRV_ERROR status = PVRSRV_OK;
    EDID_EXT_BLOCK * pExtBlock = (EDID_EXT_BLOCK*) edidBlock;

    HDMI_CHECKPOINT;

#ifdef HDMI_DEBUG
    HDMI_DEBUG_PRINT("- %s: Parsing extension block tag %d", __func__, pExtBlock->Tag);
    {
        IMG_UINT8* pData = (IMG_UINT8*)edidBlock;
        IMG_UINT32 i = 0;
        for (i = 0; i < EDID_BLOCK_SIZE; i+=8)
        {
            HDMI_DEBUG_PRINT(" Offset 0x%02x: %02x %02x %02x %02x %02x %02x %02x %02x\n", i,
                       pData[i], pData[i+1], pData[i+2], pData[i+3], pData[i+4], pData[i+5], pData[i+6], pData[i+7]);
        }
    }
#endif

    /* First byte is tag */
    switch (pExtBlock->Tag)
    {
        case EDID_EXT_TAG_CEA:
            status = EdidParseCeaBlock(edidBlock, pVideoParams);
            break;
        case EDID_EXT_TAG_VTB:
        case EDID_EXT_TAG_DI:
        case EDID_EXT_TAG_LS:
        case EDID_EXT_TAG_DPVL:
        case EDID_EXT_TAG_BLOCK_MAP:
        case EDID_EXT_TAG_MFG_DEFINED:
            status = PVRSRV_ERROR_NOT_SUPPORTED;
            break;
        default:
            HDMI_ERROR_PRINT("- %s: Unknown ext block type %d\n", __func__, pExtBlock->Tag);
            status = PVRSRV_ERROR_NOT_SUPPORTED;
            break;
    }

    return status;
}


static PVRSRV_ERROR EdidI2cReadByte(IMG_CPU_VIRTADDR base, IMG_UINT8 offset, IMG_UINT8 * value)
{
    IMG_UINT32 intReg = 0;
    IMG_UINT32 pollTime = 0;

    #ifdef HDMI_TEST_EDID
    *value = ((IMG_UINT8*)&gSampleEdid[0])[offset];
    return PVRSRV_OK;
    #endif

    /* Write 1 to clear interrupt status */
    HDMI_WRITE_CORE_REG(base, HDMI_IH_I2CM_STAT0_OFFSET,
        SET_FIELD(HDMI_IH_I2CM_STAT0_I2CMASTERDONE_START, HDMI_IH_I2CM_STAT0_I2CMASTERDONE_MASK, 1));

    /* Setup EDID base address */
    HDMI_WRITE_CORE_REG(base, HDMI_I2CM_SLAVE_OFFSET, EDID_SLAVE_ADDRESS);
    HDMI_WRITE_CORE_REG(base, HDMI_I2CM_SEGADDR_OFFSET, EDID_SEGMENT_ADDRESS);

    // address
    HDMI_WRITE_CORE_REG(base, HDMI_I2CM_ADDRESS_OFFSET, offset);

    /* Set operation to normal read */
    HDMI_WRITE_CORE_REG(base, HDMI_I2CM_OPERATION_OFFSET,
        SET_FIELD(HDMI_I2CM_OPERATION_RD_START, HDMI_I2CM_OPERATION_RD_MASK, 1));

    // poll on done

    intReg = HDMI_READ_CORE_REG(base, HDMI_IH_I2CM_STAT0_OFFSET);
    while (!IS_BIT_SET(intReg, HDMI_IH_I2CM_STAT0_I2CMASTERDONE_START)
        && !IS_BIT_SET(intReg, HDMI_IH_I2CM_STAT0_I2CMASTERERROR_START))
    {
        intReg = HDMI_READ_CORE_REG(base, HDMI_IH_I2CM_STAT0_OFFSET);
        DC_OSDelayus(1 * 100);
        pollTime += 1;
        if (pollTime > EDID_I2C_TIMEOUT_MS)
        {
            HDMI_ERROR_PRINT("- %s: Timeout polling on I2CMasterDoneBit\n", __func__);
            return PVRSRV_ERROR_TIMEOUT;
        }
    }

    if (IS_BIT_SET(intReg, HDMI_IH_I2CM_STAT0_I2CMASTERERROR_START))
    {
        HDMI_ERROR_PRINT("- %s: I2C EDID read failed, Master error bit is set\n", __func__);
        return PVRSRV_ERROR_TIMEOUT;
    }

    *value = HDMI_READ_CORE_REG(base, HDMI_I2CM_DATAI_OFFSET) & 0xFF;

    return PVRSRV_OK;
}


/*

Steps for reading EDID information:

1. Initialize I2C DDC interface (clocks, interrupts, etc)
2. Read EDID block byte-by-byte or with sequential
read (up to 8 bytes) to limit interrupts
3. Parse base block:
    a. Mfg/vendor info
    b. Preferred timing
    c. Other descriptors
4. Parse Extension blocks

*/
PVRSRV_ERROR EdidRead(HDMI_DEVICE * pvDevice)
{
    PVRSRV_ERROR status = PVRSRV_OK;
    IMG_UINT8 bufferOffset = 0;
    IMG_UINT8 i2cOffset = 0;
    IMG_UINT8 localBuffer[EDID_BLOCK_SIZE];
    IMG_UINT8 i = 0;
    EDID_BASE_BLOCK edidBase;

    HDMI_CHECKPOINT;

    #if defined(HDMI_PDUMP)
    /* Setup for VIC 16, color res 10 */
    pvDevice->videoParams.mDtdActiveIndex = pvDevice->videoParams.mDtdIndex;
    EdidAddDtd(&pvDevice->videoParams, 16, 60000);
    return PVRSRV_OK;
    #endif

	/*to incorporate extensions we have to include the following - see VESA E-DDC spec. P 11 */

    /*
    Each segment pointer contains one byte of addressable space and
    2 EDID blocks (128 bytes each)

    SegPtr/Address range        Block type

    00h/00h->7Fh                Base EDID structure
    00h/80h->FFh                Extension block 1 OR block map 1
    01h/00h->7Fh                Extension block 2
    01h/80h->FFh                Extension block 3
    ...
    40h/00h->7Fh                Block map 2 if 129<=N<=254
    ...
    */

    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_SEGPTR_OFFSET, 0);

    /*
    Loop through and read byte by byte, could optimize and use ext read
    */
    for (bufferOffset = 0; bufferOffset < sizeof(localBuffer); bufferOffset++, i2cOffset++)
    {
        // read data
        status = EdidI2cReadByte(pvDevice->pvHDMIRegCpuVAddr, i2cOffset, &localBuffer[bufferOffset]);
        if (status != PVRSRV_OK)
        {
            return status;
        }
    }

    /* Find base block and populate global */
    status = EdidFindBaseBlock(localBuffer, EDID_BLOCK_SIZE, &edidBase);
    if (status != PVRSRV_OK)
    {
        return status;
    }

    /* Extract video parameter data from base block (includes DTD parsing) */
    status = EdidParseBaseBlock(&edidBase, &pvDevice->videoParams);
    if (status != PVRSRV_OK)
    {
        return status;
    }

    HDMI_DEBUG_PRINT("- %s: Finished parsing base block, extension block count: %d\n", __func__, edidBase.ExtensionBlockCount);

    /* Loop through each extension block */
    for (i = 0; i < edidBase.ExtensionBlockCount; i++)
    {
        PVRSRV_ERROR extstatus = PVRSRV_OK;
        for (bufferOffset = 0; bufferOffset < sizeof(localBuffer); bufferOffset++)
        {
            // read data
            status = EdidI2cReadByte(pvDevice->pvHDMIRegCpuVAddr, i2cOffset++, &localBuffer[bufferOffset]);
            if (status != PVRSRV_OK)
            {
                return status;
            }
        }

        // don't treat error in extension block as fatal, so use separate status var.
        extstatus = EdidParseExtBlock(localBuffer, &pvDevice->videoParams);
        if (extstatus != PVRSRV_OK && extstatus != PVRSRV_ERROR_NOT_SUPPORTED)
        {
            HDMI_ERROR_PRINT("- %s: Failed to parse extension block %d\n", __func__, i);
            break;
        }
    }

    #if defined(HDMI_FORCE_MODE) && (HDMI_FORCE_MODE != 0)
    HDMI_DEBUG_PRINT("- %s: Forcing HDMI mode %d", __func__, HDMI_FORCE_MODE);
    EdidAddDtd(&pvDevice->videoParams, HDMI_FORCE_MODE, EDID_REFRESH_RATE);
    pvDevice->videoParams.mDtdActiveIndex = pvDevice->videoParams.mDtdIndex - 1;
    #endif

    return status;
}

PVRSRV_ERROR EdidInitialize(HDMI_DEVICE * pvDevice)
{
    HDMI_CHECKPOINT;

    /* Mask interrupts */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_INT_OFFSET,
        SET_FIELD(HDMI_I2CM_INT_DONE_MASK_START, HDMI_I2CM_INT_DONE_MASK_MASK, 1) |
        SET_FIELD(HDMI_I2CM_INT_READ_REQ_MASK_START, HDMI_I2CM_INT_READ_REQ_MASK_MASK, 1));
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_CTLINT_OFFSET,
        SET_FIELD(HDMI_I2CM_CTLINT_ARB_MASK_START, HDMI_I2CM_CTLINT_ARB_MASK_START, 1) |
        SET_FIELD(HDMI_I2CM_CTLINT_NACK_MASK_START, HDMI_I2CM_CTLINT_NACK_MASK_START, 1));


    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_DIV_OFFSET,
        SET_FIELD(HDMI_I2CM_DIV_FAST_STD_MODE_START, HDMI_I2CM_DIV_FAST_STD_MODE_START, 0));

/* Set clock division and counts */

/* Fast mode setup */
/*
    count = EdidCalcCount(EDID_I2C_MIN_FS_SCL_HIGH_TIME);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_FS_SCL_HCNT_1_OFFSET, count >> 8);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_FS_SCL_HCNT_0_OFFSET, count & 0xFF);
    count = EdidCalcCount(EDID_I2C_MIN_FS_SCL_LOW_TIME);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_FS_SCL_LCNT_1_OFFSET, count >> 8);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_FS_SCL_LCNT_0_OFFSET, count & 0xFF);
    count = EdidCalcCount(EDID_I2C_MIN_SS_SCL_HIGH_TIME);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_SS_SCL_HCNT_1_OFFSET, count >> 8);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_SS_SCL_HCNT_0_OFFSET, count & 0xFF);
    count = EdidCalcCount(EDID_I2C_MIN_SS_SCL_LOW_TIME);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_SS_SCL_LCNT_1_OFFSET, count >> 8);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_SS_SCL_LCNT_0_OFFSET, count & 0xFF);
*/
    /* Re-enable interrupts */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_INT_OFFSET,
        SET_FIELD(HDMI_I2CM_INT_DONE_MASK_START, HDMI_I2CM_INT_DONE_MASK_MASK, 0));
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_I2CM_CTLINT_OFFSET,
        SET_FIELD(HDMI_I2CM_CTLINT_ARB_MASK_START, HDMI_I2CM_CTLINT_ARB_MASK_START, 0) |
        SET_FIELD(HDMI_I2CM_CTLINT_NACK_MASK_START, HDMI_I2CM_CTLINT_NACK_MASK_START, 0));

    return PVRSRV_OK;
}


PVRSRV_ERROR EdidAddDtd(VIDEO_PARAMS * pVideoParams, IMG_UINT8 code, IMG_UINT32 refreshRate)
{
    DTD * dtd;

    if (pVideoParams->mDtdIndex >= HDMI_DTD_MAX)
    {
        HDMI_DEBUG_PRINT("- %s: Hit max DTD entries (%d)", __func__, pVideoParams->mDtdIndex);
        return PVRSRV_ERROR_OUT_OF_MEMORY;
    }

    dtd = &pVideoParams->mDtdList[pVideoParams->mDtdIndex];

    HDMI_CHECKPOINT;

    HDMI_DEBUG_PRINT("- %s: Adding DTD with VIC %d and refresh rate %d\n", __func__, code, refreshRate);

	dtd->mCode = code;
	dtd->mHBorder = DEFAULT_BORDER_WIDTH_PIXELS;
	dtd->mVBorder = DEFAULT_BORDER_WIDTH_PIXELS;
	dtd->mPixelRepetitionInput = 0;
	dtd->mHImageSize = 16;
	dtd->mVImageSize = 9;
	dtd->mYcc420 = 0;
	dtd->mLimitedToYcc420 = 0;

	switch (code)
	{
	case 1: /* 640x480p @ 59.94/60Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		dtd->mHActive = 640;
		dtd->mVActive = 480;
		dtd->mHBlanking = 160;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 16;
		dtd->mVFrontPorchWidth = 10;
		dtd->mHSyncPulseWidth = 96;
		dtd->mVSyncPulseWidth = 2;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0; /* Active low */
		dtd->mInterlaced = 0; /* not(progressive_nI) */
		dtd->mPixelClock = (refreshRate == 59940) ? 2517 : 2520; /* pixel clock depends on refresh rate */
		break;
	case 2: /* 720x480p @ 59.94/60Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 3: /* 720x480p @ 59.94/60Hz 16:9 */
		dtd->mHActive = 720;
		dtd->mVActive = 480;
		dtd->mHBlanking = 138;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 16;
		dtd->mVFrontPorchWidth = 9;
		dtd->mHSyncPulseWidth = 62;
		dtd->mVSyncPulseWidth = 6;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 59940) ? 2700 : 2702;
		break;
	case 4: /* 1280x720p @ 59.94/60Hz 16:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 370;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 110;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 59940) ? 7417 : 7425;
		break;
	case 5: /* 1920x1080i @ 59.94/60Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 540;
		dtd->mHBlanking = 280;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 88;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = (refreshRate == 59940) ? 7417 : 7425;
		break;
	case 6: /* 720(1440)x480i @ 59.94/60Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 7: /* 720(1440)x480i @ 59.94/60Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 240;
		dtd->mHBlanking = 276;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 38;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 124;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = (refreshRate == 59940) ? 2700 : 2702;
		dtd->mPixelRepetitionInput = 1;
		break;
	case 8: /* 720(1440)x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 9: /* 720(1440)x240p @ 59.826/60.054/59.886/60.115Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 240;
		dtd->mHBlanking = 276;
		dtd->mVBlanking = (refreshRate > 60000) ? 22 : 23;
		dtd->mHFrontPorchWidth = 38;
		dtd->mVFrontPorchWidth = (refreshRate > 60000) ? 4 : 5;
		dtd->mHSyncPulseWidth = 124;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock
		= ((refreshRate == 60054) || refreshRate == 59826) ? 2700
				: 2702; /*  else 60.115/59.886 Hz */
		dtd->mPixelRepetitionInput = 1;
		break;
	case 10: /* 2880x480i @ 59.94/60Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 11: /* 2880x480i @ 59.94/60Hz 16:9 */
		dtd->mHActive = 2880;
		dtd->mVActive = 240;
		dtd->mHBlanking = 552;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 76;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 248;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = (refreshRate == 59940) ? 5400 : 5405;
		break;
	case 12: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 13: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 16:9 */
		dtd->mHActive = 2880;
		dtd->mVActive = 240;
		dtd->mHBlanking = 552;
		dtd->mVBlanking = (refreshRate > 60000) ? 22 : 23;
		dtd->mHFrontPorchWidth = 76;
		dtd->mVFrontPorchWidth = (refreshRate > 60000) ? 4 : 5;
		dtd->mHSyncPulseWidth = 248;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock
		= ((refreshRate == 60054) || refreshRate == 59826) ? 5400
				: 5405; /*  else 60.115/59.886 Hz */
		break;
	case 14: /* 1440x480p @ 59.94/60Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 15: /* 1440x480p @ 59.94/60Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 480;
		dtd->mHBlanking = 276;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 32;
		dtd->mVFrontPorchWidth = 9;
		dtd->mHSyncPulseWidth = 124;
		dtd->mVSyncPulseWidth = 6;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 59940) ? 5400 : 5405;
		break;
	case 16: /* 1920x1080p @ 59.94/60Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 280;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 88;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 59940) ? 14835 : 14850;
		break;
	case 17: /* 720x576p @ 50Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 18: /* 720x576p @ 50Hz 16:9 */
		dtd->mHActive = 720;
		dtd->mVActive = 576;
		dtd->mHBlanking = 144;
		dtd->mVBlanking = 49;
		dtd->mHFrontPorchWidth = 12;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 64;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 2700;
		break;
	case 19: /* 1280x720p @ 50Hz 16:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 700;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 440;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 7425;
		break;
	case 20: /* 1920x1080i @ 50Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 540;
		dtd->mHBlanking = 720;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 528;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = 7425;
		break;
	case 21: /* 720(1440)x576i @ 50Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 22: /* 720(1440)x576i @ 50Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 288;
		dtd->mHBlanking = 288;
		dtd->mVBlanking = 24;
		dtd->mHFrontPorchWidth = 24;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 126;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = 2700;
		dtd->mPixelRepetitionInput = 1;
		break;
	case 23: /* 720(1440)x288p @ 50Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 24: /* 720(1440)x288p @ 50Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 288;
		dtd->mHBlanking = 288;
		dtd->mVBlanking = (refreshRate == 50000) ? 24
				: ((refreshRate == 49920) ? 25 : 26);
		dtd->mHFrontPorchWidth = 24;
		dtd->mVFrontPorchWidth = (refreshRate == 50000) ? 2
				: ((refreshRate == 49920) ? 3 : 4);
		dtd->mHSyncPulseWidth = 126;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 2700;
		dtd->mPixelRepetitionInput = 1;
		break;
	case 25: /* 2880x576i @ 50Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 26: /* 2880x576i @ 50Hz 16:9 */
		dtd->mHActive = 2880;
		dtd->mVActive = 288;
		dtd->mHBlanking = 576;
		dtd->mVBlanking = 24;
		dtd->mHFrontPorchWidth = 48;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 252;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = 5400;
		break;
	case 27: /* 2880x288p @ 50Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 28: /* 2880x288p @ 50Hz 16:9 */
		dtd->mHActive = 2880;
		dtd->mVActive = 288;
		dtd->mHBlanking = 576;
		dtd->mVBlanking = (refreshRate == 50000) ? 24
				: ((refreshRate == 49920) ? 25 : 26);
		dtd->mHFrontPorchWidth = 48;
		dtd->mVFrontPorchWidth = (refreshRate == 50000) ? 2
				: ((refreshRate == 49920) ? 3 : 4);
		dtd->mHSyncPulseWidth = 252;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 5400;
		break;
	case 29: /* 1440x576p @ 50Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 30: /* 1440x576p @ 50Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 576;
		dtd->mHBlanking = 288;
		dtd->mVBlanking = 49;
		dtd->mHFrontPorchWidth = 24;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 128;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 5400;
		break;
	case 31: /* 1920x1080p @ 50Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 720;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 528;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 14850;
		break;
	case 32: /* 1920x1080p @ 23.976/24Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 830;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 638;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 23976) ? 7417 : 7425;
		break;
	case 33: /* 1920x1080p @ 25Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 720;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 528;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 7425;
		break;
	case 34: /* 1920x1080p @ 29.97/30Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 280;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 88;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 29970) ? 7417 : 7425;
		break;
	case 35: /* 2880x480p @ 60Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 36: /* 2880x480p @ 60Hz 16:9 */
		dtd->mHActive = 2880;
		dtd->mVActive = 480;
		dtd->mHBlanking = 552;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 64;
		dtd->mVFrontPorchWidth = 9;
		dtd->mHSyncPulseWidth = 248;
		dtd->mVSyncPulseWidth = 6;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 59940) ? 10800 : 10810;
		break;
	case 37: /* 2880x576p @ 50Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 38: /* 2880x576p @ 50Hz 16:9 */
		dtd->mHActive = 2880;
		dtd->mVActive = 576;
		dtd->mHBlanking = 576;
		dtd->mVBlanking = 49;
		dtd->mHFrontPorchWidth = 48;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 256;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 10800;
		break;
	case 39: /* 1920x1080i (1250 total) @ 50Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 540;
		dtd->mHBlanking = 384;
		dtd->mVBlanking = 85;
		dtd->mHFrontPorchWidth = 32;
		dtd->mVFrontPorchWidth = 23;
		dtd->mHSyncPulseWidth = 168;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = 1;
		dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = 7200;
		break;
	case 40: /* 1920x1080i @ 100Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 540;
		dtd->mHBlanking = 720;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 528;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = 14850;
		break;
	case 41: /* 1280x720p @ 100Hz 16:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 700;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 440;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 14850;
		break;
	case 42: /* 720x576p @ 100Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 43: /* 720x576p @ 100Hz 16:9 */
		dtd->mHActive = 720;
		dtd->mVActive = 576;
		dtd->mHBlanking = 144;
		dtd->mVBlanking = 49;
		dtd->mHFrontPorchWidth = 12;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 64;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 5400;
		break;
	case 44: /* 720(1440)x576i @ 100Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 45: /* 720(1440)x576i @ 100Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 288;
		dtd->mHBlanking = 288;
		dtd->mVBlanking = 24;
		dtd->mHFrontPorchWidth = 24;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 126;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = 5400;
		dtd->mPixelRepetitionInput = 1;
		break;
	case 46: /* 1920x1080i @ 119.88/120Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 540;
		dtd->mHBlanking = 280;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 88;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = (refreshRate == 119880) ? 14835 : 14850;
		break;
	case 47: /* 1280x720p @ 119.88/120Hz 16:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 370;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 110;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 119880) ? 14835 : 14850;
		break;
	case 48: /* 720x480p @ 119.88/120Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 49: /* 720x480p @ 119.88/120Hz 16:9 */
		dtd->mHActive = 720;
		dtd->mVActive = 480;
		dtd->mHBlanking = 138;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 16;
		dtd->mVFrontPorchWidth = 9;
		dtd->mHSyncPulseWidth = 62;
		dtd->mVSyncPulseWidth = 6;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 119880) ? 5400 : 5405;
		break;
	case 50: /* 720(1440)x480i @ 119.88/120Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 51: /* 720(1440)x480i @ 119.88/120Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 240;
		dtd->mHBlanking = 276;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 38;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 124;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = (refreshRate == 119880) ? 5400 : 5405;
		dtd->mPixelRepetitionInput = 1;
		break;
	case 52: /* 720X576p @ 200Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 53: /* 720X576p @ 200Hz 16:9 */
		dtd->mHActive = 720;
		dtd->mVActive = 576;
		dtd->mHBlanking = 144;
		dtd->mVBlanking = 49;
		dtd->mHFrontPorchWidth = 12;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 64;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 10800;
		break;
	case 54: /* 720(1440)x576i @ 200Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 55: /* 720(1440)x576i @ 200Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 288;
		dtd->mHBlanking = 288;
		dtd->mVBlanking = 24;
		dtd->mHFrontPorchWidth = 24;
		dtd->mVFrontPorchWidth = 2;
		dtd->mHSyncPulseWidth = 126;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = 10800;
		dtd->mPixelRepetitionInput = 1;
		break;
	case 56: /* 720x480p @ 239.76/240Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 57: /* 720x480p @ 239.76/240Hz 16:9 */
		dtd->mHActive = 720;
		dtd->mVActive = 480;
		dtd->mHBlanking = 138;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 16;
		dtd->mVFrontPorchWidth = 9;
		dtd->mHSyncPulseWidth = 62;
		dtd->mVSyncPulseWidth = 6;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 239760) ? 10800 : 10810;
		break;
	case 58: /* 720(1440)x480i @ 239.76/240Hz 4:3 */
		dtd->mHImageSize = 4;
		dtd->mVImageSize = 3;
		__fallthrough;
	case 59: /* 720(1440)x480i @ 239.76/240Hz 16:9 */
		dtd->mHActive = 1440;
		dtd->mVActive = 240;
		dtd->mHBlanking = 276;
		dtd->mVBlanking = 22;
		dtd->mHFrontPorchWidth = 38;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 124;
		dtd->mVSyncPulseWidth = 3;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 0;
		dtd->mInterlaced = 1;
		dtd->mPixelClock = (refreshRate == 239760) ? 10800 : 10810;
		dtd->mPixelRepetitionInput = 1;
		break;
	case 60: /* 1280x720p @ 23.97/24Hz 16:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 2020;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 1760;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 23970) ? 5934 : 5940;
		break;
	case 61: /* 1280x720p @ 25Hz 16:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 2680;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 2420;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 7425;
		break;
	case 62: /* 1280x720p @ 29.97/30Hz  16:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 2020;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 1760;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 29970) ? 7417: 7425;
		break;
	case 63: /* 1920x1080p @ 119.88/120Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 280;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 88;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = (refreshRate == 119880) ?  29670: 29700;
		break;
	case 64: /* 1920x1080p @ 100Hz 16:9 */
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 720;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 528;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700;
		break;
	case 68:
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 700;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 440;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 7425;
		break;
	case 69: /* 1280x720p @ 60Hz 21:9 */
		dtd->mHActive = 1280;
		dtd->mVActive = 720;
		dtd->mHBlanking = 370;
		dtd->mVBlanking = 30;
		dtd->mHFrontPorchWidth = 110;
		dtd->mVFrontPorchWidth = 5;
		dtd->mHSyncPulseWidth = 40;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 7425;
		break;
	case 75: //1080p
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 720;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 528;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 14850;
		break;
	case 76: //1080p
		dtd->mHActive = 1920;
		dtd->mVActive = 1080;
		dtd->mHBlanking = 280;
		dtd->mVBlanking = 45;
		dtd->mHFrontPorchWidth = 88;
		dtd->mVFrontPorchWidth = 4;
		dtd->mHSyncPulseWidth = 44;
		dtd->mVSyncPulseWidth = 5;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 14850;
		break;
	case 93: /* 4k x 2k, 30Hz, HDMI_VIC 3 */
		dtd->mCode = 93;
		dtd->mHActive = 3840;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 1660;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 1276;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;
	case 94: /* 4k x 2k, 30Hz, HDMI_VIC 2 */
		dtd->mCode = 94;
		dtd->mHActive = 3840;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 1440;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 1056;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;

	case 95: /* 4k x 2k, HDMI_VIC 1 */
		dtd->mCode = 95;
		dtd->mHActive = 3840;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 560;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 176;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;
	case 96: /* 4k x 2k, HDMI_VIC 2 */
		dtd->mCode = 96;
		dtd->mHActive = 3840;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 1440;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 1056;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;
	case 97: /* 4k x 2k, HDMI_VIC 1 */
		dtd->mCode = 97;
		dtd->mHActive = 3840;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 560;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 176;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;
	case 98: /* 4k x 2k, HDMI_VIC 1 */
		dtd->mCode = 98;
		dtd->mHActive = 4096;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 1404;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 1020;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;
	case 103:
		dtd->mCode = 103;
		dtd->mHActive = 3840;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 1660;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 1276;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;
	case 104:
		dtd->mCode = 104;
		dtd->mHActive = 3840;
		dtd->mVActive = 2160;
		dtd->mHBlanking = 1440;
		dtd->mVBlanking = 90;
		dtd->mHFrontPorchWidth = 1056;
		dtd->mVFrontPorchWidth = 8;
		dtd->mHSyncPulseWidth = 88;
		dtd->mVSyncPulseWidth = 10;
		dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
		dtd->mInterlaced = 0;
		dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
		break;
    case 105:
        dtd->mCode = 105;
        dtd->mHActive = 3840;
        dtd->mVActive = 2160;
        dtd->mHBlanking = 560;
        dtd->mVBlanking = 90;
        dtd->mHFrontPorchWidth = 176;
        dtd->mVFrontPorchWidth = 8;
        dtd->mHSyncPulseWidth = 88;
        dtd->mVSyncPulseWidth = 10;
        dtd->mHSyncPolarity = dtd->mVSyncPolarity = 1;
        dtd->mInterlaced = 0;
        dtd->mPixelClock = 29700; //(refreshRate == 59940) ? 14835 : 14850;
        break;
    default:
        dtd->mCode = 0;
        return PVRSRV_ERROR_INVALID_PARAMS;
    }
#if !defined(HDMI_EXPERIMENTAL_CLOCKS)
    if (PhyIsPclkSupported(dtd->mPixelClock) == false)
    {
        /* Return success but don't add to list */
        return PVRSRV_OK;
    }
#endif
    dtd->mValid = true;

    pVideoParams->mDtdIndex = (pVideoParams->mDtdIndex + 1) % HDMI_DTD_MAX;
    return PVRSRV_OK;
}
