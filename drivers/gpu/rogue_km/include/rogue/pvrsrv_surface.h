/*************************************************************************/ /*!
@File
@Title          PowerVR Services Surface
@Description    Surface definitions and utilities that are externally visible
                (i.e. visible to clients of services), but are also required
                within services.
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

#ifndef PVRSRV_SURFACE_H
#define PVRSRV_SURFACE_H

#include "img_types.h"
#include "img_defs.h"
#include <powervr/buffer_attribs.h>

#define PVRSRV_SURFACE_TRANSFORM_NONE      (0U << 0)		/*!< No transformation */
#define PVRSRV_SURFACE_TRANSFORM_FLIP_H    (1U << 0)		/*!< Flip horizontally */
#define PVRSRV_SURFACE_TRANSFORM_FLIP_V    (1U << 1)		/*!< Flip vertically */
#define PVRSRV_SURFACE_TRANSFORM_ROT_90    (1U << 2)                          /*!< Rotate  90 degree clockwise */
#define PVRSRV_SURFACE_TRANSFORM_ROT_180   ((1U << 0) + (1U << 1))             /*!< Rotate 180 degree clockwise */
#define PVRSRV_SURFACE_TRANSFORM_ROT_270   ((1U << 0) + (1U << 1) + (1U << 2))  /*!< Rotate 270 degree clockwise */

#define PVRSRV_SURFACE_BLENDING_NONE       0	/*!< Use no blending */
#define PVRSRV_SURFACE_BLENDING_PREMULT    1	/*!< Use blending with pre-multiplier */
#define PVRSRV_SURFACE_BLENDING_COVERAGE   2	/*!< Use coverage blending */

/*!
 * Modes of memory layouts for surfaces.
 *
 *   Enum: #_PVRSRV_SURFACE_MEMLAYOUT_
 *   Typedef: ::PVRSRV_SURFACE_MEMLAYOUT
 */
typedef enum _PVRSRV_SURFACE_MEMLAYOUT_
{
	PVRSRV_SURFACE_MEMLAYOUT_STRIDED = 0,		/*!< Strided memory buffer */
	PVRSRV_SURFACE_MEMLAYOUT_FBC,				/*!< Compressed frame buffer */
} PVRSRV_SURFACE_MEMLAYOUT;

/*!
 * Frame Buffer Compression layout.
 * Defines the FBC mode to use.
 *
 *   Structure: #PVRSRV_SURFACE_FBC_LAYOUT_TAG
 *   Typedef: ::PVRSRV_SURFACE_FBC_LAYOUT
 */
typedef struct PVRSRV_SURFACE_FBC_LAYOUT_TAG
{
	/*! The compression mode for this surface */
	IMG_FB_COMPRESSION	eFBCompressionMode;
} PVRSRV_SURFACE_FBC_LAYOUT;

/*!
 * Pixel and memory format of a surface
 *
 *   Structure: #PVRSRV_SURFACE_FORMAT_TAG
 *   Typedef: ::PVRSRV_SURFACE_FORMAT
 */
typedef struct PVRSRV_SURFACE_FORMAT_TAG
{
	/*! Enum value of type IMG_PIXFMT for the pixel format */
	IMG_UINT32					ePixFormat;

	/*! Enum surface memory layout */
	PVRSRV_SURFACE_MEMLAYOUT	eMemLayout;

	/*! Special layout options for the surface.
	 * Needs services support.
	 * Depends on eMemLayout.*/
	union {
		PVRSRV_SURFACE_FBC_LAYOUT	sFBCLayout;
	} u;
} PVRSRV_SURFACE_FORMAT;

/*!
 * Width and height of a surface
 *
 *   Structure: #PVRSRV_SURFACE_DIMS_TAG
 *   Typedef: ::PVRSRV_SURFACE_DIMS
 */
typedef struct PVRSRV_SURFACE_DIMS_TAG
{
	IMG_UINT32		ui32Width;		/*!< Width in pixels */
	IMG_UINT32		ui32Height;		/*!< Height in pixels */
} PVRSRV_SURFACE_DIMS;

/*!
 * Dimension and format details of a surface
 *
 *   Structure: #PVRSRV_SURFACE_INFO_TAG
 *   Typedef: ::PVRSRV_SURFACE_INFO
 */
typedef struct PVRSRV_SURFACE_INFO_TAG
{
	PVRSRV_SURFACE_DIMS		sDims;		/*!< Width and height */
	PVRSRV_SURFACE_FORMAT	sFormat;	/*!< Memory format */

	/*! Needed for FBCv3.
	 * Indicates offset from allocation start
	 * to the actual data, skipping the header.
	 */
	IMG_UINT32				ui32FBCBaseOffset;
} PVRSRV_SURFACE_INFO;

/*!
 * Defines a rectangle on a surface
 *
 *   Structure: #PVRSRV_SURFACE_RECT_TAG
 *   Typedef: ::PVRSRV_SURFACE_RECT
 */
typedef struct PVRSRV_SURFACE_RECT_TAG
{
	IMG_INT32				i32XOffset;	/*!< X offset from origin in pixels */
	IMG_INT32				i32YOffset;	/*!< Y offset from origin in pixels */
	PVRSRV_SURFACE_DIMS		sDims;		/*!< Rectangle dimensions */
} PVRSRV_SURFACE_RECT;

/*!
 * Surface configuration details
 *
 *   Structure: #PVRSRV_SURFACE_CONFIG_INFO_TAG
 *   Typedef: ::PVRSRV_SURFACE_CONFIG_INFO
 */
typedef struct PVRSRV_SURFACE_CONFIG_INFO_TAG
{
	/*! Crop applied to surface (BEFORE transformation) */
	PVRSRV_SURFACE_RECT		sCrop;

	/*! Region of screen to display surface in (AFTER scaling) */
	PVRSRV_SURFACE_RECT		sDisplay;

	/*! Surface transformation none/flip/rotate.
	 * Use PVRSRV_SURFACE_TRANSFORM_xxx macros
	 */
	IMG_UINT32				ui32Transform;

	/*! Alpha blending mode e.g. none/premult/coverage.
	 * Use PVRSRV_SURFACE_BLENDING_xxx macros
	 */
	IMG_UINT32				eBlendType;

	/*! Custom data for the display engine */
	IMG_UINT32				ui32Custom;

	/*! Alpha value for this plane */
	IMG_UINT8				ui8PlaneAlpha;

	/*! Reserved for later use */
	IMG_UINT8				ui8Reserved1[3];
} PVRSRV_SURFACE_CONFIG_INFO;

/*!
 * Contains information about a panel
 */
typedef struct PVRSRV_PANEL_INFO_TAG
{
	PVRSRV_SURFACE_INFO sSurfaceInfo;		/*!< Panel surface details */
	IMG_UINT32			ui32RefreshRate;	/*!< Panel refresh rate in Hz */
	IMG_UINT32			ui32XDpi;			/*!< Panel DPI in x direction */
	IMG_UINT32			ui32YDpi;			/*!< Panel DPI in y direction */
} PVRSRV_PANEL_INFO;

/*!
 * Helper function to create a Config Info based on a Surface Info
 * to do a flip with no scale, transformation etc.
 */
static INLINE void SurfaceConfigFromSurfInfo(const PVRSRV_SURFACE_INFO *psSurfaceInfo,
                                             PVRSRV_SURFACE_CONFIG_INFO *psConfigInfo)
{
	psConfigInfo->sCrop.sDims = psSurfaceInfo->sDims;
	psConfigInfo->sCrop.i32XOffset = 0;
	psConfigInfo->sCrop.i32YOffset = 0;
	psConfigInfo->sDisplay.sDims = psSurfaceInfo->sDims;
	psConfigInfo->sDisplay.i32XOffset = 0;
	psConfigInfo->sDisplay.i32YOffset = 0;
	psConfigInfo->ui32Transform = PVRSRV_SURFACE_TRANSFORM_NONE;
	psConfigInfo->eBlendType = PVRSRV_SURFACE_BLENDING_NONE;
	psConfigInfo->ui32Custom = 0;
	psConfigInfo->ui8PlaneAlpha = 0xff;
}

#endif /* PVRSRV_SURFACE_H */
