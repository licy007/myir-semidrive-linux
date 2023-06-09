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

#include "hdmi.h"
#include "edid.h"
#include "phy.h"
#include "video.h"
#include "hdcp.h"
#include "dc_external.h"
#include <linux/delay.h>
#include "plato.h"

static IMG_UINT32 g_uiNumHDMIInstances;   /* Number of instances */

static void PrintDebugInformation(HDMI_DEVICE * pvDevice)
{
    HDMI_DEBUG_PRINT("\nDesign ID: %d\nRev ID: %d\nProduct ID: %d\nProduct ID HDCP: %d\n",
        HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_DESIGN_ID_OFFSET),
        HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_REVISION_ID_OFFSET),
        HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_PRODUCT_ID0_OFFSET),
        GET_FIELD(HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_PRODUCT_ID1_OFFSET),
            HDMI_ID_PRODUCT_ID1_HDCP_START, HDMI_ID_PRODUCT_ID1_HDCP_MASK));

    HDMI_DEBUG_PRINT("\nHDCP Present: %d\nHDMI 1.4: %d\nHDMI 2.0: %d\nPHY Type: %d\n",
        GET_FIELD(HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_CONFIG0_ID_OFFSET),
            HDMI_ID_CONFIG0_ID_HDCP_START, HDMI_ID_CONFIG0_ID_HDCP_MASK),
        GET_FIELD(HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_CONFIG0_ID_OFFSET),
            HDMI_ID_CONFIG0_ID_HDMI14_START, HDMI_ID_CONFIG0_ID_HDMI14_MASK),
        GET_FIELD(HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_CONFIG1_ID_OFFSET),
            HDMI_ID_CONFIG1_ID_HDMI20_START, HDMI_ID_CONFIG1_ID_HDMI20_MASK),
        HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_ID_CONFIG2_ID_OFFSET));
}

#if defined(HDMI_PDUMP)

static bool HdmiPdumpSetup(HDMI_DEVICE * pvDevice)
{
    PVRSRV_ERROR status = PVRSRV_OK;

    HDMI_CHECKPOINT;

    /* Set to VIC 16 (1920x1080) */
    #if DC_DEFAULT_VIDEO_FORMAT == VIDEO_FORMAT_1920_1080p
    EdidAddDtd(&pvDevice->videoParams, 16, 60000);
    #elif DC_DEFAULT_VIDEO_FORMAT == VIDEO_FORMAT_1280_720p
    EdidAddDtd(&pvDevice->videoParams, 4, 60000);
    #endif

    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_TX_INVID0_OFFSET, 0x03);

    /* This maps 1:1 to pdump */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INVIDCONF_OFFSET, 0x78);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INHACTIV0_OFFSET, 0x10);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INHACTIV0_OFFSET, 0x80);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INHACTIV1_OFFSET, 0x07);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INVACTIV0_OFFSET, 0x38);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INVACTIV1_OFFSET, 0x04);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INHBLANK0_OFFSET, 0x18);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INHBLANK1_OFFSET, 0x01);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_INVBLANK_OFFSET, 0x2D);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_HSYNCINDELAY0_OFFSET, 0x58);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_HSYNCINDELAY1_OFFSET, 0x00);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_VSYNCINDELAY_OFFSET, 0x04);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_HSYNCINWIDTH0_OFFSET, 0x2C);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_HSYNCINWIDTH1_OFFSET, 0x00);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_VSYNCINWIDTH_OFFSET, 0x05);

    PhyPowerDown(pvDevice);

    /* pixel packing and 10bit color depth */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_VP_PR_CD_OFFSET, 0x50);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_VP_CONF_OFFSET, 0x64);

    /* Enable HPD sense...user guide says this should be the first step */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_CONF0_OFFSET,
        SET_FIELD(HDMI_PHY_CONF0_SELDATAENPOL_START, HDMI_PHY_CONF0_SELDATAENPOL_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_ENHPDRXSENSE_START, HDMI_PHY_CONF0_ENHPDRXSENSE_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_PDDQ_START, HDMI_PHY_CONF0_PDDQ_MASK, 1));

    HDMI_CHECKPOINT;

    /* Audio N and CTS value */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_AUD_N1_OFFSET, 0xE5);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_AUD_N2_OFFSET, 0x0F);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_AUD_N3_OFFSET, 0x00);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_AUD_CTS1_OFFSET, 0x19);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_AUD_CTS2_OFFSET, 0xD5);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_AUD_CTS3_OFFSET, 0x02);

    /* Write Phy values through I2C, based on MPLL settings in PHY spec (p 193):

    Pixel Clock: 148.5MHz
    Pixel repetition: no
    Color depth: 10-bit
    TMDS clk src: 185.625MHz

    NOTE: THIS IS BASED ON PHY MODEL E302 (see HDMI reference driver: phy.c:3187)

    */
    status = PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x214c);
    CHECK_AND_EXIT(status, EXIT);
    status = PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0038);
    CHECK_AND_EXIT(status, EXIT);
    status = PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0003);
    CHECK_AND_EXIT(status, EXIT);
    status = PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET, 0x0211);
    CHECK_AND_EXIT(status, EXIT);
    status = PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_TXTERM_OFFSET, 0x0004);
    CHECK_AND_EXIT(status, EXIT);

    /* Similar to step 3.2.2.g (except we're setting ENHPDRXSENSE as well) */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_CONF0_OFFSET,
        SET_FIELD(HDMI_PHY_CONF0_SELDATAENPOL_START, HDMI_PHY_CONF0_SELDATAENPOL_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_ENHPDRXSENSE_START, HDMI_PHY_CONF0_ENHPDRXSENSE_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_TXPWRON_START, HDMI_PHY_CONF0_TXPWRON_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_SVSRET_START, HDMI_PHY_CONF0_SVSRET_MASK, 1));

    /* Wait for PHY to lock */
    status = PhyWaitLock(pvDevice);
    #if defined(EMULATOR)
    status = PVRSRV_OK;
    #endif
    CHECK_AND_EXIT(status, EXIT);

    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_CTRLDUR_OFFSET, 0x0D);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_EXCTRLDUR_OFFSET, 0x20);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_EXCTRLSPAC_OFFSET, 0x01);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_CH0PREAM_OFFSET, 0x0B);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_CH1PREAM_OFFSET, 0x16);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_CH2PREAM_OFFSET, 0x21);
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_MC_CLKDIS_OFFSET,
        SET_FIELD(HDMI_MC_CLKDIS_HDCPCLK_DIS_START, HDMI_MC_CLKDIS_HDCPCLK_DIS_MASK, 1));
    // This should already be set??
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_FC_VSYNCINWIDTH_OFFSET, 0x05);

EXIT:
    return (status == PVRSRV_OK) ? true : false;
}

#endif /* HDMI_PDUMP */

static void HdmiInitInterrupts(HDMI_DEVICE * psDeviceData)
{
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_PHY_STAT0_OFFSET , 0x3e);

    /* Mute all other interrupts */
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_FC_STAT0_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_FC_STAT1_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_FC_STAT2_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_AS_STAT0_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_I2CM_STAT0_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_CEC_STAT0_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_VP_STAT0_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_I2CMPHY_STAT0_OFFSET , 0xff);
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_AHBDMAAUD_STAT0_OFFSET , 0xff);
}
static void HdmiEnableHPDInterrupt(HDMI_DEVICE * psDeviceData)
{
    /* Enable hot plug interrupts */
    HDMI_CHECKPOINT;

    /* Clear HPD interrupt by writing 1*/
    if (IS_BIT_SET(HDMI_READ_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_PHY_STAT0_OFFSET), HDMI_IH_PHY_STAT0_HPD_START))
    {
        HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_PHY_STAT0_OFFSET,
            SET_FIELD(HDMI_IH_PHY_STAT0_HPD_START, HDMI_IH_PHY_STAT0_HPD_MASK, 1));
    }

    HdmiInitInterrupts(psDeviceData);

    HDMI_CHECKPOINT;

    /* Power off */
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_PHY_CONF0_OFFSET,
        SET_FIELD(HDMI_PHY_CONF0_TXPWRON_START, HDMI_PHY_CONF0_TXPWRON_MASK, 0) |
        SET_FIELD(HDMI_PHY_CONF0_PDDQ_START, HDMI_PHY_CONF0_PDDQ_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_SVSRET_START, HDMI_PHY_CONF0_SVSRET_MASK, 1));

    /* Enable hot plug detection */
    HDMI_READ_MOD_WRITE(psDeviceData->pvHDMIRegCpuVAddr, HDMI_PHY_CONF0_OFFSET,
        SET_FIELD(HDMI_PHY_CONF0_ENHPDRXSENSE_START, HDMI_PHY_CONF0_ENHPDRXSENSE_MASK, 1));

    /* Now flip the master switch to unmute */
    HDMI_WRITE_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_OFFSET , SET_FIELD(HDMI_IH_MUTE_ALL_START, HDMI_IH_MUTE_ALL_MASK, 0));

}

static IMG_BOOL HdmiLISRHandler(void *pvData)
{
    HDMI_DEVICE* psDevice = (HDMI_DEVICE*)pvData;

    IMG_UINT32 ui32PhyStat0 = 0;
    IMG_UINT32 ui32IHPhyStat0 = 0;
    IMG_UINT32 ui32HPDPolarity = 0;
    IMG_UINT32 ui32Decode = 0;
    unsigned long ulFlags = 0;

    HDMI_CHECKPOINT;

    DC_OSSpinLockIRQSave(&psDevice->irqLock, ulFlags);

    /* Mute all interrupts */
    HDMI_WRITE_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_OFFSET, 0x03);

    ui32Decode = HDMI_READ_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_IH_DECODE_OFFSET);

    /* Only support interrupts on PHY (eg HPD) */
    if (!IS_BIT_SET(ui32Decode, HDMI_IH_DECODE_PHY_START))
    {
        HDMI_DEBUG_PRINT("- %s: Unknown interrupt generated, decode: %x\n", __func__,
            HDMI_READ_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_IH_DECODE_OFFSET));
        goto IRQRestore;
    }

    ui32PhyStat0 = HDMI_READ_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_PHY_STAT0_OFFSET);
    ui32HPDPolarity = GET_FIELD(HDMI_READ_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_PHY_POL0_OFFSET), HDMI_PHY_POL0_HPD_START, HDMI_PHY_POL0_HPD_MASK);
    ui32IHPhyStat0 = HDMI_READ_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_IH_PHY_STAT0_OFFSET);

    HDMI_DEBUG_PRINT("- %s: Hdmi interrupt detected PHYStat0: 0x%x, HPD Polarity: 0x%x, IH Decode: 0x%x, IH PhyStat0: 0x%x",
					 __func__, ui32PhyStat0, ui32HPDPolarity, ui32Decode, ui32IHPhyStat0);

    /* Check if hot-plugging occurred */
    if (GET_FIELD(ui32PhyStat0, HDMI_PHY_STAT0_HPD_START, HDMI_PHY_STAT0_HPD_MASK) == IMG_TRUE &&
        GET_FIELD(ui32IHPhyStat0, HDMI_IH_PHY_STAT0_HPD_START, HDMI_IH_PHY_STAT0_HPD_MASK) == IMG_TRUE)
    {
        HDMI_DEBUG_PRINT("- %s: Hot plug detected", __func__);

        /* Flip polarity */
        HDMI_WRITE_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_PHY_POL0_OFFSET,
            SET_FIELD(HDMI_PHY_POL0_HPD_START, HDMI_PHY_POL0_HPD_MASK, !ui32HPDPolarity));

        /* Write 1 to clear interrupt */
        HDMI_WRITE_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_IH_PHY_STAT0_OFFSET, ui32IHPhyStat0);

        /* schedule work for HDMI/PDP init*/
        if (psDevice->bHPD == IMG_FALSE)
        {
            psDevice->bHPD = IMG_TRUE;
            schedule_delayed_work(&(psDevice->delayedWork), msecs_to_jiffies(10));
        }

        /* Mute non-HPD interrupts */
        HdmiInitInterrupts(psDevice);

        /* Now flip the master switch to unmute */
        HDMI_WRITE_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_IH_MUTE_OFFSET, 0);

    }
    else
    {
        HDMI_DEBUG_PRINT("- %s: Cable unplugged\n", __func__);

        /* Flip polarity */
        HDMI_WRITE_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_PHY_POL0_OFFSET,
            SET_FIELD(HDMI_PHY_POL0_HPD_START, HDMI_PHY_POL0_HPD_MASK, !ui32HPDPolarity));

        /* Write 1 to clear interrupts */
        HDMI_WRITE_CORE_REG(psDevice->pvHDMIRegCpuVAddr, HDMI_IH_PHY_STAT0_OFFSET, ui32IHPhyStat0);

        /* Unmute and enable HPD interrupt */
        HdmiEnableHPDInterrupt(psDevice);

        /* Cable was unplugged */
        psDevice->bHPD = IMG_FALSE;
    }

IRQRestore:

    DC_OSSpinUnlockIRQRestore(&psDevice->irqLock, ulFlags);

    return IMG_TRUE;
}

static void HdmiMISRHandler(struct work_struct *work)
{
    PVRSRV_ERROR eError;
    struct delayed_work *delay_work = to_delayed_work(work);
    HDMI_DEVICE* psDeviceData = container_of(delay_work, HDMI_DEVICE, delayedWork);

    HDMI_CHECKPOINT;

    if (!psDeviceData->bHPD || !psDeviceData->mInitialized)
    {
        return;
    }

#if defined(HDMI_PDUMP)
    HdmiPdumpSetup(psDeviceData)
#endif /* HDMI_PDUMP */

    /* Set VGA (640x480p, 60kHz refresh) in DVI mode */
    psDeviceData->videoParams.mDtdIndex = 0;
    EdidAddDtd(&psDeviceData->videoParams, 1, 60000);

    /* Step A: Initialize PHY */
    if ((eError = PhyInitialize(psDeviceData)) != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to initialize PHY\n", __func__);
        return;
    }

    /* Step B: Initialize video/frame composer and DTD for VGA/DVI mode */
    if ((eError = VideoInitialize(psDeviceData)) != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to initialize Video\n", __func__);
        return;
    }

    if ((eError = EdidInitialize(psDeviceData)) != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to initialize EDID\n", __func__);
        return;
    }

    /* Step C: Read Sinks E-EDID information */
    if ((eError = EdidRead(psDeviceData)) != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to read EDID\n", __func__);
        return;
    }

    /* Step D: Configure video mode */
    if ((eError = VideoConfigureMode(psDeviceData)) != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to configure video mode\n", __func__);
        return;
    }

    /* No need to configure audio on Plato, skip to step F: Configure InfoFrames */
    if ((eError = VideoConfigureInfoframes(psDeviceData)) != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to initialize PHY\n", __func__);
        return;
    }

    /* Step G: Configure HDCP */
    if ((eError = HdcpConfigure(psDeviceData)) != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to initialize PHY\n", __func__);
        return;
    }

#if defined(HDMI_DEBUG)
    {
        DTD * dtd = &psDeviceData->videoParams.mDtdList[psDeviceData->videoParams.mDtdActiveIndex];
        HDMI_DEBUG_PRINT("- %s: Final HDMI timing configuration: \n\nmHdmi: %d\nmEncodingOut: %d\nmColorRes: %d\nmPixelRep: %d\n \
\nDTD Info: \n\tmCode: %d\n\tmPixelRep: %d\n\tmPixelClock: %d\n\tmInterlaced: %d\n\tmHActive: %d\n\tmHBlanking: %d\n\tmHBorder: %d\n\t\
mHImageSize: %d\n\tmHFrontPorchWidth: %d\n\tmHSyncPulseWidth: %d\n\tmHsyncPolarity: %d\n\tmVActive: %d\n\tmVBlanking: %d\n\tmVBorder: %d\n\t\
mVImageSize: %d\n\tmVFrontPorchWidth: %d\n\tmVSyncPulseWidth: %d\n\tmVsyncPolarity: %d\n\n", __func__, psDeviceData->videoParams.mHdmi,
psDeviceData->videoParams.mEncodingOut, psDeviceData->videoParams.mColorResolution, psDeviceData->videoParams.mPixelRepetitionFactor,
dtd->mCode, dtd->mPixelRepetitionInput, dtd->mPixelClock, dtd->mInterlaced, dtd->mHActive, dtd->mHBlanking, dtd->mHBorder,
dtd->mHImageSize, dtd->mHFrontPorchWidth, dtd->mHSyncPulseWidth, dtd->mHSyncPolarity, dtd->mVActive, dtd->mVBlanking, dtd->mVBorder,
dtd->mVImageSize, dtd->mVFrontPorchWidth, dtd->mVSyncPulseWidth, dtd->mVSyncPolarity);
    }
#endif

    if (psDeviceData->pfnPDPInitialize)
    {
        eError = psDeviceData->pfnPDPInitialize(&(psDeviceData->videoParams),
                                                  psDeviceData->ui32Instance);
    }
    else
    {
        HDMI_ERROR_PRINT("- %s: Failed to get PDP<->Hdmi interface\n", __func__);
        eError = PVRSRV_ERROR_INIT_FAILURE;
    }

    if (eError != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: PDP initialization failed \n", __func__);
    }

}

PVRSRV_ERROR HDMIDrvInit(void *pvDevice, HDMI_DEVICE **ppsDeviceData)
{
    HDMI_DEVICE *psDeviceData;
    PVRSRV_ERROR eError;

    if (ppsDeviceData == NULL)
    {
        return PVRSRV_ERROR_INVALID_PARAMS;
    }

    DC_OSSetDrvName(DRVNAME);

    HDMI_CHECKPOINT;

    #if defined(VIRTUAL_PLATFORM)
    HDMI_ERROR_PRINT("-%s: HDMI not supported on VP\n", __func__);
    #endif

    psDeviceData = DC_OSCallocMem(sizeof(*psDeviceData));
    if (psDeviceData == NULL)
    {
        HDMI_ERROR_PRINT("- %s: Failed to allocate device data (size %d)\n", __func__, sizeof(*psDeviceData));
        eError = PVRSRV_ERROR_OUT_OF_MEMORY;
        goto FAIL_DEV_ALLOC;
    }

    psDeviceData->pvDevice = pvDevice;
    psDeviceData->bHPD = IMG_FALSE;
    psDeviceData->ui32Instance = g_uiNumHDMIInstances++;

    DC_OSSpinLockCreate(&psDeviceData->irqLock);

    eError = DC_OSPVRServicesConnectionOpen(&psDeviceData->hPVRServicesConnection);
    if (eError != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to open connection to PVR Services (%d)\n", __func__, eError);
        goto FAIL_CONN_CLOSE;
    }

    eError = DC_OSPVRServicesSetupFuncs(psDeviceData->hPVRServicesConnection, &psDeviceData->sPVRServicesFuncs);
    if (eError != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to setup PVR Services function table (%d)\n", __func__, eError);
        goto FAIL_CONN_CLOSE;
    }

    HDMI_DEBUG_PRINT("- %s: Installing HDMI interrupt handler in services\n", __func__);

    eError = psDeviceData->sPVRServicesFuncs.pfnSysInstallDeviceLISR(pvDevice,
        PLATO_IRQ_HDMI,
        DRVNAME,
        HdmiLISRHandler,
        psDeviceData,
        &psDeviceData->hLISRData);
    if (eError != PVRSRV_OK)
    {
        HDMI_ERROR_PRINT("- %s: Failed to install system Device LISR (%s) unit %u\n",
        __func__, psDeviceData->sPVRServicesFuncs.pfnGetErrorString(eError),
        psDeviceData->ui32Instance);
        goto FAIL_DETACH_ISR;
    }

    INIT_DELAYED_WORK(&psDeviceData->delayedWork, HdmiMISRHandler);

    HDMI_DEBUG_PRINT("- %s: Reserving HDMI Core registers\n", __func__);

    /* Reserve and map the HDMI registers */
    psDeviceData->sHDMIRegCpuPAddr.uiAddr =
        DC_OSAddrRangeStart(psDeviceData->pvDevice, SYS_PLATO_REG_PCI_BASENUM) + SYS_PLATO_REG_HDMI_OFFSET;

    if (DC_OSRequestAddrRegion(psDeviceData->sHDMIRegCpuPAddr, SYS_PLATO_REG_HDMI_SIZE, DRVNAME) == NULL)
    {
        HDMI_ERROR_PRINT("- %s: Failed to reserve the HDMI registers\n", __func__);
        psDeviceData->sHDMIRegCpuPAddr.uiAddr = 0;
        eError = PVRSRV_ERROR_PCI_REGION_UNAVAILABLE;
        goto FAIL_CONN_CLOSE;
    }

    psDeviceData->pvHDMIRegCpuVAddr = DC_OSMapPhysAddr(psDeviceData->sHDMIRegCpuPAddr, SYS_PLATO_REG_HDMI_SIZE);
    if (psDeviceData->pvHDMIRegCpuVAddr == NULL)
    {
        HDMI_ERROR_PRINT("- %s: Failed to map HDMI registers\n", __func__);
        eError = PVRSRV_ERROR_OUT_OF_MEMORY;
        goto RELEASE_MEM_REGION;
    }

    /* Check for invalid register space */
    if (HDMI_READ_CORE_REG(psDeviceData->pvHDMIRegCpuVAddr, HDMI_ID_DESIGN_ID_OFFSET) == 0xFFFFFFFF)
    {
        HDMI_DEBUG_PRINT("- %s: Invalid HDMI register space", __func__);
        eError = PVRSRV_ERROR_PCI_REGION_UNAVAILABLE;
        goto RELEASE_MEM_REGION;
    }

    /*
     * Map Plato TOP registers - needed for pixel clock setup.
     */
    psDeviceData->sTopRegCpuPAddr.uiAddr =
        DC_OSAddrRangeStart(psDeviceData->pvDevice, SYS_PLATO_REG_PCI_BASENUM) + SYS_PLATO_REG_CHIP_LEVEL_OFFSET;

    psDeviceData->pvTopRegCpuVAddr = DC_OSMapPhysAddr(psDeviceData->sTopRegCpuPAddr, SYS_PLATO_REG_CHIP_LEVEL_SIZE);
    if (psDeviceData->pvTopRegCpuVAddr == NULL)
    {
        HDMI_ERROR_PRINT("- %s: Failed to map TOP registers\n", __func__);
        eError = PVRSRV_ERROR_OUT_OF_MEMORY;
        goto RELEASE_MEM_REGION;
    }

    PrintDebugInformation(psDeviceData);

    HdmiEnableHPDInterrupt(psDeviceData);

    *ppsDeviceData = psDeviceData;

    HDMI_CHECKPOINT;

    psDeviceData->mInitialized = IMG_TRUE;

    return PVRSRV_OK;

RELEASE_MEM_REGION:
    DC_OSReleaseAddrRegion(psDeviceData->sHDMIRegCpuPAddr, SYS_PLATO_REG_HDMI_SIZE);
FAIL_DETACH_ISR:
    /* Uninstall the ISR as we've failed the later stage of initialisation */
    HDMI_ERROR_PRINT("- %s: Uninstalling Services ISR, Unit %u\n", __func__,
                     psDeviceData->ui32Instance);
    g_uiNumHDMIInstances--;
    if (psDeviceData->sPVRServicesFuncs.pfnSysUninstallDeviceLISR != NULL)
    {
        psDeviceData->sPVRServicesFuncs.pfnSysUninstallDeviceLISR(psDeviceData->hLISRData);
        DC_OSSpinLockDestroy(&psDeviceData->irqLock);
    }
    else
    {
        HDMI_ERROR_PRINT("- %s: SysUninstallDeviceLISR handle not present. Power-cycle system to reset\n", __func__);
    }
FAIL_CONN_CLOSE:
    DC_OSFreeMem(psDeviceData);

FAIL_DEV_ALLOC:

    return eError;
}

void HDMIDrvDeInit(HDMI_DEVICE *psDeviceData)
{
    HDMI_CHECKPOINT;
    if (psDeviceData)
    {

        HDMI_DEBUG_PRINT(" - %s: DeInit unit %u\n", __func__,
                         psDeviceData->ui32Instance);

        /* Unregister the LISR - we have to disable and flush the deferred
         * worker threads here too
         */
        if (psDeviceData->sPVRServicesFuncs.pfnSysUninstallDeviceLISR != NULL)
        {
            HDMI_DEBUG_PRINT(" - %s: Uninstall Services LISR unit %u\n",
                             __func__, psDeviceData->ui32Instance);
            psDeviceData->sPVRServicesFuncs.pfnSysUninstallDeviceLISR(psDeviceData->hLISRData);
            HDMI_DEBUG_PRINT(" - %s: Cleanup delayed work queues, unit %u\n",
                             __func__, psDeviceData->ui32Instance);

            (void) flush_delayed_work(&psDeviceData->delayedWork);
            (void) cancel_delayed_work_sync(&psDeviceData->delayedWork);

            DC_OSSpinLockDestroy(&psDeviceData->irqLock);
        }

        DC_OSUnmapPhysAddr(psDeviceData->pvHDMIRegCpuVAddr, SYS_PLATO_REG_HDMI_SIZE);
        DC_OSReleaseAddrRegion(psDeviceData->sHDMIRegCpuPAddr, SYS_PLATO_REG_HDMI_SIZE);
        DC_OSPVRServicesConnectionClose(psDeviceData->hPVRServicesConnection);
        HDMI_CHECKPOINT;
        DC_OSFreeMem(psDeviceData);
        g_uiNumHDMIInstances--;
    }
}
