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

#ifndef DC_EDID_H
#define DC_EDID_H

#include "hdmi.h"

/*************************************************************************/ /*!
 EDID Information
*/ /**************************************************************************/
#define EDID_SIGNATURE                      0x00ffffffffffff00ull
#define EDID_BLOCK_SIZE                     128
#define EDID_CHECKSUM                       0 // per VESA spec, 8 bit checksum of base block
#define EDID_SLAVE_ADDRESS                  0x50
#define EDID_SEGMENT_ADDRESS                0x00
#define EDID_REFRESH_RATE                   60000
#define EDID_I2C_TIMEOUT_MS					100

/* Extension block tag numbers (sec 2.2.4 of EDID spec) */
#define EDID_EXT_TAG_CEA                    (0x02)
#define EDID_EXT_TAG_VTB                    (0x10)
#define EDID_EXT_TAG_DI                     (0x40)
#define EDID_EXT_TAG_LS                     (0x50)
#define EDID_EXT_TAG_DPVL                   (0x60)
#define EDID_EXT_TAG_BLOCK_MAP              (0xF0)
#define EDID_EXT_TAG_MFG_DEFINED            (0xFF)

/* CEA Extension defines */
#define EDID_CEA_REVISION                   (3)
#define EDID_CEA_DATA_BLOCK_AUDIO_TAG       (1)
#define EDID_CEA_DATA_BLOCK_VIDEO_TAG       (2)
#define EDID_CEA_DATA_BLOCK_VENDOR_TAG      (3)
#define EDID_CEA_DATA_BLOCK_SPEAKER_TAG     (4)

/* Descriptor types */
#define EDID_DESC_DETAILED_TIMING           0
#define EDID_DESC_DISPLAY                   1

/* Display descriptor tags */
enum
{
    DISP_DESC_TAG_MFG_START = 0x00,
    DISP_DESC_TAG_MFG_END = 0x0F,
    DISP_DESC_TAG_DUMMY,
    DISP_DESC_TAG_RESERVED_START,
    DISP_DESC_TAG_RESERVED_END = 0xF6,
    DISP_DESC_TAG_EST_TIMING_3,
    DISP_DESC_TAG_CVT_3,
    DISP_DESC_TAG_DCM,
    DISP_DESC_TAG_STD_TIMING,
    DISP_DESC_TAG_COLOR_POINT_DATA,
    DISP_DESC_TAG_PRODUCT_NAME,
    DISP_DESC_TAG_RANGE_LIMITS,
    DISP_DESC_TAG_DATA_STRING,
    DISP_DESC_TAG_SERIAL_NUMBER,
};

/* Digital video interface standards.
 * See chapter 3.6.1 of VESA spec */
enum
{
	DIGITAL_VIDEO_INTERFACE_NULL = 0x0,
	DIGITAL_VIDEO_INTERFACE_DVI,
	DIGITAL_VIDEO_INTERFACE_HDMI_A,
	DIGITAL_VIDEO_INTERFACE_HDMI_B,
	DIGITAL_VIDEO_INTERFACE_MDDI,
	DIGITAL_VIDEO_INTERFACE_DISPLAY_PORT,
};

/* See chapter 3.10.2 of VESA spec */
typedef struct _DTD_RAW
{
    IMG_UINT16                  pixelClock; // 0x0001 - 0xFFFF stored value = pixel clock / 10,000

    IMG_UINT8                   hActiveLower;
    IMG_UINT8                   hBlankingLower;

    IMG_UINT8                   hBlankingUpper          : 4;
    IMG_UINT8                   hActiveUpper            : 4;

    IMG_UINT8                   vActiveLower;
    IMG_UINT8                   vBlankingLower;

    IMG_UINT8                   vBlankingUpper          : 4;
    IMG_UINT8                   vActiveUpper            : 4;

    IMG_UINT8                   hFrontPorchLower;
    IMG_UINT8                   hSyncPulseWidthLower;

    // Byte 10
    IMG_UINT8                   vSyncPulseWidthLower    : 4;
    IMG_UINT8                   vFrontPorchLower        : 4;

    // Byte 11
    IMG_UINT8                   vSyncPulseWidthUpper    : 2;
    IMG_UINT8                   vFrontPorchUpper        : 2;
    IMG_UINT8                   hSyncPulseWidthUpper    : 2;
    IMG_UINT8                   hFrontPorchUpper        : 2;

    IMG_UINT8                   hImageSizeLower;
    IMG_UINT8                   vImageSizeLower;
    IMG_UINT8                   vImageSizeUpper			: 4;
    IMG_UINT8                   hImageSizeUpper         : 4;
    IMG_UINT8                   hBorder;
    IMG_UINT8                   vBorder;

    // Byte 17
    IMG_UINT8                   stereoViewing2          : 1;
    IMG_UINT8                   syncSignalDefs          : 4;
    IMG_UINT8                   stereoViewing1          : 2;
    IMG_UINT8                   interlaced              : 1;

} DTD_RAW;

typedef union _ESTABILISHED_TIMING_I
{
    IMG_UINT8 value;
    struct {
        IMG_UINT8 _800x600_60Hz : 1;
        IMG_UINT8 _800x600_56Hz : 1;
        IMG_UINT8 _640x480_75Hz : 1;
        IMG_UINT8 _640x480_72Hz : 1;
        IMG_UINT8 _640x480_67Hz : 1;
        IMG_UINT8 _640x480_60Hz : 1;
        IMG_UINT8 _720x400_88Hz : 1;
        IMG_UINT8 _720x400_70Hz : 1;
    } fields;
}ESTABILISHED_TIMING_I;

typedef union _STANDARD_TIMING
{
    IMG_UINT16 value;
    struct
    {
	    IMG_UINT16 horizontalAddressablePixels : 8;
        IMG_UINT16 refreshRate : 6;
        IMG_UINT16 aspectRatio : 2;
    } fields;
} STANDARD_TIMING;

typedef union _VIDEO_INPUT_DEFINITION
{
    IMG_UINT8 value;
    struct
    {
        IMG_UINT8 serrations : 1; // BIT 0
        IMG_UINT8 syncTypes : 3; // BIT 3-1
        IMG_UINT8 videoSetup : 1; // BIT 4
        IMG_UINT8 signalLevelStandard : 2; // BIT 6-5
        IMG_UINT8 signalInterface : 1; // BIT 7
    } analog;

    struct
    {
        IMG_UINT8 vidInterfaceSupported : 4; // BIT 3-0
        IMG_UINT8 colorBitDepth : 3; // BIT 6-4
        IMG_UINT8 signalInterface : 1; // BIT 7
    } digital;
} VIDEO_INPUT_DEFINITION;

typedef union _FEATURE_SUPPORT
{
    IMG_UINT8 value;
    struct
    {
	        // Indicates whether Preferred Timing Mode includes native pixel format
	        // and preferred refresh rate of the display
        IMG_UINT8 continuousFrequency : 1; // BIT 0
        IMG_UINT8 preferredTimingIncluded : 1; // BIT 1
            // BIT 7 at address 14h must be 1 for this layout (signalInterface = digital)
            // 00 : RBG 4:4:4
            // 01 : RGB 4:4:4 & YCrCb 4:4:4
            // 10 : RGB 4:4:4 & YCrCb 4:2:2
            // 11 : RGB 4:4:4 & YCrCb 4:4:4 & YCrCb 4:2:2
        IMG_UINT8 sRGBstandard : 1; // BIT 2
        IMG_UINT8 colorEncodingFormat : 2; // BIT 4 - 3
        IMG_UINT8 veryLowPowerSupport : 1; // BIT 5
        IMG_UINT8 suspendSupport : 1; // BIT 6
        IMG_UINT8 standbySupport : 1; // BIT 7
    } fields;
} FEATURE_SUPPORT;

typedef struct _DISPLAY_DESCRIPTOR
{
    IMG_UINT8                   DescTag[3];                         // all 0 for display descriptor
    IMG_UINT8                   DispDescTag;                        // See enum above
    IMG_UINT8                   Reserved;                           // 00h except for Display Range Limits
    IMG_UINT8                   Data[13];
} DISPLAY_DESCRIPTOR;

/* Structures for each type of display descriptor */
typedef struct _MONITOR_RANGE_DATA
{
    IMG_UINT8                   offsetFlags;
    IMG_UINT8                   vMinRate;
    IMG_UINT8                   vMaxRate;
    IMG_UINT8                   hMinRate;
    IMG_UINT8                   hMaxRate;
    IMG_UINT8                   maxPixelClock;
    IMG_UINT8                   supportFlags;
        // Determines layout of next 7 bytes
        // 00: Default GTF supported (if BIT 0 in feature support = 1)
        // 01: Range limits only
        // 02: Secondary GTF supported
        // 04: CVT supported
    union
    {
        // Byte 10 = 00h or 01h
        struct
        {
            IMG_UINT8           lineFeed;
            IMG_UINT8           space[6];
        };
        // Byte 10 = 02h or 04h
        struct
        {
            IMG_UINT8           timingData[7];
        };
    };
} MONITOR_RANGE_DATA;

typedef struct _EDID_EXT_BLOCK
{
    IMG_UINT8                   Tag;
    IMG_UINT8                   Revision;
    IMG_UINT8                   Data[125];
    IMG_UINT8                   Checksum;
} EDID_EXT_BLOCK;

typedef struct _EDID_EXT_CEA
{
    IMG_UINT8                   Tag;
    IMG_UINT8                   Revision;
    IMG_UINT8                   DtdOffset; // Offset within block that has first 18-byte DTD
    IMG_UINT8                   Flags; // Contains info about YC222, YC444, number of DTDs, etc
    IMG_UINT8                   Data[123];
    IMG_UINT8                   Checksum;
} EDID_EXT_CEA;


typedef struct
{
    IMG_UINT8                   Header[8];                          //EDID header "00 FF FF FF FF FF FF 00"
    IMG_UINT16                  ManufactureName;                    //EISA 3-character ID (5 bit compressed ASCII)
    IMG_UINT16                  ProductCode;                        //Vendor assigned code
    IMG_UINT32                  SerialNumber;                       //32-bit serial number
    IMG_UINT8                   WeekOfManufacture;                  //Week number
    IMG_UINT8                   YearOfManufacture;                  //Year
    IMG_UINT8                   EdidVersion;                        //EDID Structure Version
    IMG_UINT8                   EdidRevision;                       //EDID Structure Revision
    VIDEO_INPUT_DEFINITION      VideoInputDefinition;
    IMG_UINT8                   MaxHorizontalImageSize;             //cm
    IMG_UINT8                   MaxVerticalImageSize;               //cm
    IMG_UINT8                   Gamma;
    FEATURE_SUPPORT             FeatureSupport;
    IMG_UINT8                   RedGreenLowBits;                    //Rx1 Rx0 Ry1 Ry0 Gx1 Gx0 Gy1Gy0
    IMG_UINT8                   BlueWhiteLowBits;                   //Bx1 Bx0 By1 By0 Wx1 Wx0 Wy1 Wy0
    IMG_UINT8                   RedX;                               //Red-x Bits 9 - 2
    IMG_UINT8                   RedY;                               //Red-y Bits 9 - 2
    IMG_UINT8                   GreenX;                             //Green-x Bits 9 - 2
    IMG_UINT8                   GreenY;                             //Green-y Bits 9 - 2
    IMG_UINT8                   BlueX;                              //Blue-x Bits 9 - 2
    IMG_UINT8                   BlueY;                              //Blue-y Bits 9 - 2
    IMG_UINT8                   WhiteX;                             //White-x Bits 9 - 2
    IMG_UINT8                   WhiteY;                             //White-x Bits 9 - 2
    IMG_UINT8                   EstablishedTimings[3];              // Optional (see above)
    STANDARD_TIMING             StandardTimings[8];                 // Optional
    /* The next 72 bytes vary per E-EDID Standard (1.4 is below) */
    DTD_RAW                     PreferredTimingMode;                // Check FEATURE_SUPPORT BIT 1 before using this
    IMG_UINT8                   Descriptor2[18];                    // Or display descriptor
    IMG_UINT8                   Descriptor3[18];                    // Or display descriptor
    IMG_UINT8                   Descriptor4[18];                    // Or display descriptor
    IMG_UINT8                   ExtensionBlockCount;                //Number of (optional) 128-byte EDID extension blocks to follow
    IMG_UINT8                   Checksum;
} EDID_BASE_BLOCK;

/* Function prototypes */
PVRSRV_ERROR EdidAddDtd(VIDEO_PARAMS * pVideoParams, IMG_UINT8 code, IMG_UINT32 refreshRate);
PVRSRV_ERROR EdidInitialize(HDMI_DEVICE * pvDevice);
PVRSRV_ERROR EdidRead(HDMI_DEVICE * pvDevice);

#endif
