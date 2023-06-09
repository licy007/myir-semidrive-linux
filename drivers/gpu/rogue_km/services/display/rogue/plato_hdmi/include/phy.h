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
#ifndef DC_PHY_H
#define DC_PHY_H

#include "hdmi.h"

/* PHY Gen2 slave addr */
#if defined(EMULATOR)
#define HDMI_PHY_SLAVE_ADDRESS                  0xA0
#else
#define HDMI_PHY_SLAVE_ADDRESS                  0x00
#endif

#if defined(VIRTUAL_PLATFORM)
#define HDMI_PHY_TIMEOUT_MS                     1
#define HDMI_PHY_POLL_INTERVAL_MS               1
/* PHY dependent time for reset */
#define HDMI_PHY_RESET_TIME_MS                  1
#else
/* Timeout for waiting on PHY operations */
#define HDMI_PHY_TIMEOUT_MS                     200
#define HDMI_PHY_POLL_INTERVAL_MS               100
/* PHY dependent time for reset */
#define HDMI_PHY_RESET_TIME_MS                  10
#endif

/* PHY I2C register definitions */
#define PHY_PWRCTRL_OFFSET                      (0x00)
#define PHY_SERDIVCTRL_OFFSET                   (0x01)
#define PHY_SERCKCTRL_OFFSET                    (0x02)
#define PHY_SERCKKILLCTRL_OFFSET                (0x03)
#define PHY_TXRESCTRL_OFFSET                    (0x04)
#define PHY_CKCALCTRL_OFFSET                    (0x05)
#define PHY_OPMODE_PLLCFG_OFFSET                (0x06)
#define PHY_CLKMEASCTRL_OFFSET                  (0x07)
#define PHY_TXMEASCTRL_OFFSET                   (0x08)
#define PHY_CKSYMTXCTRL_OFFSET                  (0x09)
#define PHY_CMPSEQCTRL_OFFSET                   (0x0A)
#define PHY_CMPPWRCTRL_OFFSET                   (0x0B)
#define PHY_CMPMODECTRL_OFFSET                  (0x0C)
#define PHY_MEASCTRL_OFFSET                     (0x0D)
#define PHY_VLEVCTRL_OFFSET                     (0x0E)
#define PHY_D2ACTRL_OFFSET                      (0x0F)
#define PHY_PLLCURRCTRL_OFFSET                  (0x10)
#define PHY_PLLDRVANACTRL_OFFSET                (0x11)
#define PHY_PLLCTRL_OFFSET                      (0x14)
#define PHY_PLLGMPCTRL_OFFSET                   (0x15)
#define PHY_PLLMEASCTRL_OFFSET                  (0x16)
#define PHY_PLLCLKBISTPHASE_OFFSET              (0x17)
#define PHY_COMPRCAL_OFFSET                     (0x18)
#define PHY_TXTERM_OFFSET                       (0x19)
#define PHY_PWRSEQ_PATGENSKIP_OFFSET            (0x1A)
/* Leaving out BIST regs */


bool PhyIsPclkSupported(IMG_UINT16 pixelClock);
PVRSRV_ERROR PhyI2CWrite(IMG_CPU_VIRTADDR base, IMG_UINT8 addr, IMG_UINT16 data);
PVRSRV_ERROR PhyInitialize(HDMI_DEVICE * pvDevice);
PVRSRV_ERROR PhyPowerDown(HDMI_DEVICE * pvDevice);
PVRSRV_ERROR PhyWaitLock(HDMI_DEVICE * pvDevice);
void PhyReset(HDMI_DEVICE * pvDevice, IMG_UINT8 value);
void PhyConfigureMode(HDMI_DEVICE * pvDevice);

#endif
