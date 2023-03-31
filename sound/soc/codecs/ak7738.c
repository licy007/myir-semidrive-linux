/*
 * ak7738.c  --  audio driver for AK7738
 *
 * Copyright (C) 2014 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      15/06/11	    1.3
 *                      16/03/24	    1.4
 *                      16/06/24	    1.5
 *                      17/07/06        1.6   4.4.XX
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>


#include "ak7738.h"
#include "ak7738_dsp_code.h"

//#define AK7738_DEBUG	1		//used at debug mode
//#define AK7738_DIT_ENABLE		//used at DIT mode

#define AK7738_MAX_REGISTER	(AK7738_MAX_REGISTERS - 1)

#ifdef AK7738_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

/* AK7738 Codec Private Data */
struct ak7738_priv {
	struct snd_soc_codec *codec;
	struct spi_device *spi;
	struct i2c_client *i2c;
	struct regmap *regmap;
	int fs;

	int pdn_gpio;
	int MIRNo;
	int status;

	int DSPPramMode;
	int DSPCramMode;
	int DSPOfregMode;

    int PLLInput;  // 0 : XTI,   1 : BICK1 ...5 : BICK5
	int XtiFs;     // 0 : 12.288MHz,  1: 18.432MHz
	int BUSMclk;   // 0 : 24.576MHz,  1: 12.288MHz,  6.144MHz

	int SDfs[NUM_SYNCDOMAIN];     // 0:8kHz, 1:12kHz, 2:16kHz, 3:24kHz, 4:32kHz, 5:48kHz, 6:96kHz, 7:192kHz
    int SDBick[NUM_SYNCDOMAIN];   // 0:64fs, 1:48fs, 2:32fs, 3:128fs, 4:256fs

	int nDSP2;    // 0  :  Power Down, 1: Connect to DSP1, 2: Power Up, 3: Connect to DSP1 & Power Up
	int nSubDSP;  // 0  :  Power Down, 1: Connect to DSP1

	int Master[NUM_SYNCDOMAIN];  // 0 : Slave Mode, 1 : Master
	int SDCks[NUM_SYNCDOMAIN];   // 0 : Low,  1: PLLMCLK,  2:XTI

	int TDMIbit[2]; //0 : TDMI off, 1 : TDMI on
	int DIEDGEbit[NUM_SYNCDOMAIN];
	int DOEDGEbit[NUM_SYNCDOMAIN];
	int DISLbit[NUM_SYNCDOMAIN];
	int DOSLbit[NUM_SYNCDOMAIN];

	/*Next section is added by user by dsp firmware*/
	int DSP2Switch1;
	int DSP1HFPVol;

};

struct _ak7738_pd_handler {
	int ref_count;
	struct mutex lock;
	struct ak7738_priv *data;
} ak7738_pd_handler = {
	.ref_count = -1,
	.data = NULL,
};

/* ak7738 register cache & default register settings */
static const struct reg_default ak7738_reg[] = {
  { 0x00, 0x00 },  /*  AK7738_00_STSCLOCK_SETTING1  */
  { 0x01, 0x00 },  /*  AK7738_01_STSCLOCK_SETTING2  */
  { 0x02, 0x00 },  /*  AK7738_02_MICBIAS_PLOWER  */
  { 0x03, 0x00 },  /*  AK7738_03_SYNCDOMAIN1_SETTING1  */
  { 0x04, 0x00 },  /*  AK7738_04_SYNCDOMAIN1_SETTING2  */
  { 0x05, 0x00 },  /*  AK7738_05_SYNCDOMAIN2_SETTING1  */
  { 0x06, 0x00 },  /*  AK7738_06_SYNCDOMAIN2_SETTING2  */
  { 0x07, 0x00 },  /*  AK7738_07_SYNCDOMAIN3_SETTING1  */
  { 0x08, 0x00 },  /*  AK7738_08_SYNCDOMAIN3_SETTING2  */
  { 0x09, 0x00 },  /*  AK7738_09_SYNCDOMAIN4_SETTING1  */
  { 0x0A, 0x00 },  /*  AK7738_0A_SYNCDOMAIN4_SETTING2  */
  { 0x0B, 0x00 },  /*  AK7738_0B_SYNCDOMAIN5_SETTING1  */
  { 0x0C, 0x00 },  /*  AK7738_0C_SYNCDOMAIN5_SETTING2  */
  { 0x0D, 0x00 },  /*  AK7738_0D_SYNCDOMAIN6_SETTING1  */
  { 0x0E, 0x00 },  /*  AK7738_0E_SYNCDOMAIN6_SETTING2  */
  { 0x0F, 0x00 },  /*  AK7738_0E_SYNCDOMAIN7_SETTING1  */
  { 0x10, 0x00 },  /*  AK7738_10_SYNCDOMAIN7_SETTING2  */
  { 0x11, 0x00 },  /*  AK7738_11_BUS_DSP_CLOCK_SETTING  */
  { 0x12, 0x00 },  /*  AK7738_12_BUS_CLOCK_SETTING2  */
  { 0x13, 0x00 },  /*  AK7738_13_CLOKO_OUTPUT_SETTING  */
  { 0x14, 0x00 },  /*  AK7738_14_RESERVED  */
  { 0x15, 0x00 },  /*  AK7738_15_SYNCDOMAIN_SELECT1  */
  { 0x16, 0x00 },  /*  AK7738_16_SYNCDOMAIN_SELECT2  */
  { 0x17, 0x00 },  /*  AK7738_17_SYNCDOMAIN_SELECT3  */
  { 0x18, 0x00 },  /*  AK7738_18_SYNCDOMAIN_SELECT4  */
  { 0x19, 0x00 },  /*  AK7738_19_SYNCDOMAIN_SELECT5  */
  { 0x1A, 0x00 },  /*  AK7738_1A_SYNCDOMAIN_SELECT6  */
  { 0x1B, 0x00 },  /*  AK7738_1B_SYNCDOMAIN_SELECT7  */
  { 0x1C, 0x00 },  /*  AK7738_1C_SYNCDOMAIN_SELECT8  */
  { 0x1D, 0x00 },  /*  AK7738_1D_SYNCDOMAIN_SELECT9  */
  { 0x1E, 0x00 },  /*  AK7738_1E_SYNCDOMAIN_SELECT10  */
  { 0x1F, 0x00 },  /*  AK7738_1F_SYNCDOMAIN_SELECT11  */
  { 0x20, 0x00 },  /*  AK7738_20_SYNCDOMAIN_SELECT12  */
  { 0x21, 0x00 },  /*  AK7738_21_SYNCDOMAIN_SELECT13  */
  { 0x22, 0x00 },  /*  AK7738_22_RESERVED  */
  { 0x23, 0x00 },  /*  AK7738_23_SDOUT1_DATA_SELECT  */
  { 0x24, 0x00 },  /*  AK7738_24_SDOUT2_DATA_SELECT  */
  { 0x25, 0x00 },  /*  AK7738_25_SDOUT3_DATA_SELECT  */
  { 0x26, 0x00 },  /*  AK7738_26_SDOUT4_DATA_SELECT  */
  { 0x27, 0x00 },  /*  AK7738_27_SDOUT5_DATA_SELECT  */
  { 0x28, 0x00 },  /*  AK7738_28_SDOUT6_DATA_SELECT  */
  { 0x29, 0x00 },  /*  AK7738_29_DAC1_DATA_SELECT  */
  { 0x2A, 0x00 },  /*  AK7738_2A_DAC2_DATA_SELECT  */
  { 0x2B, 0x00 },  /*  AK7738_2B_DSP1_IN1_DATA_SELECT  */
  { 0x2C, 0x00 },  /*  AK7738_2C_DSP1_IN2_DATA_SELECT  */
  { 0x2D, 0x00 },  /*  AK7738_2D_DSP1_IN3_DATA_SELECT  */
  { 0x2E, 0x00 },  /*  AK7738_2E_DSP1_IN4_DATA_SELECT  */
  { 0x2F, 0x00 },  /*  AK7738_2F_DSP1_IN5_DATA_SELECT  */
  { 0x30, 0x00 },  /*  AK7738_30_DSP1_IN6_DATA_SELECT  */
  { 0x31, 0x00 },  /*  AK7738_31_DSP2_IN1_DATA_SELECT  */
  { 0x32, 0x00 },  /*  AK7738_32_DSP2_IN2_DATA_SELECT  */
  { 0x33, 0x00 },  /*  AK7738_33_DSP2_IN3_DATA_SELECT  */
  { 0x34, 0x00 },  /*  AK7738_34_DSP2_IN4_DATA_SELECT  */
  { 0x35, 0x00 },  /*  AK7738_35_DSP2_IN5_DATA_SELECT  */
  { 0x36, 0x00 },  /*  AK7738_36_DSP2_IN6_DATA_SELECT  */
  { 0x37, 0x00 },  /*  AK7738_37_SRC1_DATA_SELECT  */
  { 0x38, 0x00 },  /*  AK7738_38_SRC2_DATA_SELECT  */
  { 0x39, 0x00 },  /*  AK7738_39_SRC3_DATA_SELECT  */
  { 0x3A, 0x00 },  /*  AK7738_3A_SRC4_DATA_SELECT  */
  { 0x3B, 0x00 },  /*  AK7738_3B_FSCONV1_DATA_SELECT  */
  { 0x3C, 0x00 },  /*  AK7738_3C_FSCONV2_DATA_SELECT  */
  { 0x3D, 0x00 },  /*  AK7738_3D_MIXERA_CH1_DATA_SELECT  */
  { 0x3E, 0x00 },  /*  AK7738_3E_MIXERB_CH2_DATA_SELECT  */
  { 0x3F, 0x00 },  /*  AK7738_3F_MIXERC_CH1_DATA_SELECT  */
  { 0x40, 0x00 },  /*  AK7738_40_MIXERD_CH2_DATA_SELECT  */
  { 0x41, 0x00 },  /*  AK7738_41_DIT_DATA_SELECT  */
  { 0x42, 0x00 },  /*  AK7738_42_RESERVED  */
  { 0x43, 0x00 },  /*  AK7738_43_RESERVED  */
  { 0x44, 0x00 },  /*  AK7738_44_CLOCKFORMAT_SETTING1  */
  { 0x45, 0x00 },  /*  AK7738_45_CLOCKFORMAT_SETTING2  */
  { 0x46, 0x00 },  /*  AK7738_46_CLOCKFORMAT_SETTING3  */
  { 0x47, 0x00 },  /*  AK7738_47_RESERVED  */
  { 0x48, 0x00 },  /*  AK7738_48_SDIN1_FORMAT  */
  { 0x49, 0x00 },  /*  AK7738_49_SDIN2_FORMAT  */
  { 0x4A, 0x00 },  /*  AK7738_4A_SDIN3_FORMAT  */
  { 0x4B, 0x00 },  /*  AK7738_4B_SDIN4_FORMAT  */
  { 0x4C, 0x00 },  /*  AK7738_4C_SDIN5_FORMAT  */
  { 0x4D, 0x00 },  /*  AK7738_4D_SDIN6_FORMAT  */
  { 0x4E, 0x00 },  /*  AK7738_4E_SDOUT1_FORMAT  */
  { 0x4F, 0x00 },  /*  AK7738_4F_SDOUT2_FORMAT  */
  { 0x50, 0x00 },  /*  AK7738_50_SDOUT3_FORMAT  */
  { 0x51, 0x00 },  /*  AK7738_51_SDOUT4_FORMAT  */
  { 0x52, 0x00 },  /*  AK7738_52_SDOUT5_FORMAT  */
  { 0x53, 0x00 },  /*  AK7738_53_SDOUT6_FORMAT  */
  { 0x54, 0x00 },  /*  AK7738_54_SDOUT_MODE_SETTING  */
  { 0x55, 0x00 },  /*  AK7738_55_TDM_MODE_SETTING  */
  { 0x56, 0x00 },  /*  AK7738_56_RESERVED  */
  { 0x57, 0x00 },  /*  AK7738_57_OUTPUT_PORT_SELECT  */
  { 0x58, 0x00 },  /*  AK7738_58_OUTPUT_PORT_ENABLE  */
  { 0x59, 0x00 },  /*  AK7738_59_INPUT_PORT_SELECT  */
  { 0x5A, 0x00 },  /*  AK7738_5A_RESERVED  */
  { 0x5B, 0x00 },  /*  AK7738_5B_MIXER_A_SETTING  */
  { 0x5C, 0x00 },  /*  AK7738_5C_MIXER_B_SETTING  */
  { 0x5D, 0x00 },  /*  AK7738_5D_MICAMP_GAIN  */
  { 0x5E, 0x00 },  /*  AK7738_5E_MICAMP_GAIN_CONTROL  */
  { 0x5F, 0x30 },  /*  AK7738_5F_ADC1_LCH_VOLUME  */
  { 0x60, 0x30 },  /*  AK7738_60_ADC1_RCH_VOLUME  */
  { 0x61, 0x30 },  /*  AK7738_61_ADC2_LCH_VOLUME  */
  { 0x62, 0x30 },  /*  AK7738_62_ADC2_RCH_VOLUME  */
  { 0x63, 0x30 },  /*  AK7738_63_ADCM_VOLUME  */
  { 0x64, 0x00 },  /*  AK7738_64_RESERVED  */
  { 0x65, 0x00 },  /*  AK7738_65_RESERVED  */
  { 0x66, 0x00 },  /*  AK7738_66_AIN_FILTER  */
  { 0x67, 0x00 },  /*  AK7738_67_ADC_MUTE_HPF  */
  { 0x68, 0x18 },  /*  AK7738_68_DAC1_LCH_VOLUME  */
  { 0x69, 0x18 },  /*  AK7738_69_DAC1_RCH_VOLUME  */
  { 0x6A, 0x18 },  /*  AK7738_6A_DAC2_LCH_VOLUME  */
  { 0x6B, 0x18 },  /*  AK7738_6B_DAC2_RCH_VOLUME  */
  { 0x6C, 0x00 },  /*  AK7738_6C_RESERVED  */
  { 0x6D, 0x00 },  /*  AK7738_6D_RESERVED  */
  { 0x6E, 0x00 },  /*  AK7738_6E_DAC_MUTE_FILTER  */
  { 0x6F, 0x00 },  /*  AK7738_6F_SRC_CLOCK_SETTING  */
  { 0x70, 0x00 },  /*  AK7738_70_RC_MUTE_SETTING  */
  { 0x71, 0x00 },  /*  AK7738_71_FSCONV_MUTE_SETTING  */
  { 0x72, 0x00 },  /*  AK7738_72_RESERVED  */
  { 0x73, 0x00 },  /*  AK7738_73_DSP_MEMORY_ASSIGNMENT1  */
  { 0x74, 0x00 },  /*  AK7738_74_DSP_MEMORY_ASSIGNMENT2  */
  { 0x75, 0x00 },  /*  AK7738_75_DSP12_DRAM_SETTING  */
  { 0x76, 0x00 },  /*  AK7738_76_DSP1_DLRAM_SETTING  */
  { 0x77, 0x00 },  /*  AK7738_77_DSP2_DLRAM_SETTING  */
  { 0x78, 0x00 },  /*  AK7738_78_FFT_DLP0_SETTING  */
  { 0x79, 0x00 },  /*  AK7738_79_RESERVED  */
  { 0x7A, 0x00 },  /*  AK7738_7A_JX_SETTING  */
  { 0x7B, 0x00 },  /*  AK7738_7B_STO_FLAG_SETTING1  */
  { 0x7C, 0x00 },  /*  AK7738_7C_STO_FLAG_SETTING2  */
  { 0x7D, 0x00 },  /*  AK7738_7D_RESERVED  */
  { 0x7E, 0x00 },  /*  AK7738_7E_DIT_STATUS_BIT1  */
  { 0x7F, 0x04 },  /*  AK7738_7F_DIT_STATUS_BIT2  */
  { 0x80, 0x02 },  /*  AK7738_80_DIT_STATUS_BIT3  */
  { 0x81, 0x00 },  /*  AK7738_81_DIT_STATUS_BIT4  */
  { 0x82, 0x00 },  /*  AK7738_82_RESERVED  */
  { 0x83, 0x00 },  /*  AK7738_83_POWER_MANAGEMENT1  */
  { 0x84, 0x00 },  /*  AK7738_84_POWER_MANAGEMENT2  */
  { 0x85, 0x00 },  /*  AK7738_85_RESET_CTRL  */
  { 0x86, 0x00 },  /*  AK7738_86_RESERVED  */
  { 0x87, 0x00 },  /*  AK7738_87_RESERVED  */
  { 0x88, 0x00 },  /*  Reserved  */
  { 0x89, 0x00 },  /*  Reserved  */
  { 0x8A, 0x00 },  /*  Reserved  */
  { 0x8B, 0x00 },  /*  Reserved  */
  { 0x8C, 0x00 },  /*  Reserved  */
  { 0x8D, 0x00 },  /*  Reserved  */
  { 0x8E, 0x00 },  /*  Reserved  */
  { 0x8F, 0x00 },  /*  Reserved  */
  { 0x90, 0x00 },  /*  AK7738_90_RESERVED  */
  { 0x91, 0x00 },  /*  AK7738_91_PAD_DRIVE_SEL2  */
  { 0x92, 0x00 },  /*  AK7738_92_PAD_DRIVE_SEL3  */
  { 0x93, 0x00 },  /*  AK7738_93_PAD_DRIVE_SEL4  */
  { 0x94, 0x00 },  /*  AK7738_94_PAD_DRIVE_SEL5  */
  { 0x95, 0x00 },  /*  AK7738_95_RESERVED  */
  { 0x96, 0x00 },  /*  Reserved  */
  { 0x97, 0x00 },  /*  Reserved  */
  { 0x98, 0x00 },  /*  Reserved  */
  { 0x99, 0x00 },  /*  Reserved  */
  { 0x9A, 0x00 },  /*  Reserved  */
  { 0x9B, 0x00 },  /*  Reserved  */
  { 0x9C, 0x00 },  /*  Reserved  */
  { 0x9D, 0x00 },  /*  Reserved  */
  { 0x9E, 0x00 },  /*  Reserved  */
  { 0x9F, 0x00 },  /*  Reserved  */
  { 0xA0, 0x00 },  /*  Reserved  */
  { 0xA1, 0x00 },  /*  Reserved  */
  { 0xA2, 0x00 },  /*  Reserved  */
  { 0xA3, 0x00 },  /*  Reserved  */
  { 0xA4, 0x00 },  /*  Reserved  */
  { 0xA5, 0x00 },  /*  Reserved  */
  { 0xA6, 0x00 },  /*  Reserved  */
  { 0xA7, 0x00 },  /*  Reserved  */
  { 0xA8, 0x00 },  /*  Reserved  */
  { 0xA9, 0x00 },  /*  Reserved  */
  { 0xAA, 0x00 },  /*  Reserved  */
  { 0xAB, 0x00 },  /*  Reserved  */
  { 0xAC, 0x00 },  /*  Reserved  */
  { 0xAD, 0x00 },  /*  Reserved  */
  { 0xAE, 0x00 },  /*  Reserved  */
  { 0xAF, 0x00 },  /*  Reserved  */
  { 0xB0, 0x00 },  /*  Reserved  */
  { 0xB1, 0x00 },  /*  Reserved  */
  { 0xB2, 0x00 },  /*  Reserved  */
  { 0xB3, 0x00 },  /*  Reserved  */
  { 0xB4, 0x00 },  /*  Reserved  */
  { 0xB5, 0x00 },  /*  Reserved  */
  { 0xB6, 0x00 },  /*  Reserved  */
  { 0xB7, 0x00 },  /*  Reserved  */
  { 0xB8, 0x00 },  /*  Reserved  */
  { 0xB9, 0x00 },  /*  Reserved  */
  { 0xBA, 0x00 },  /*  Reserved  */
  { 0xBB, 0x00 },  /*  Reserved  */
  { 0xBC, 0x00 },  /*  Reserved  */
  { 0xBD, 0x00 },  /*  Reserved  */
  { 0xBE, 0x00 },  /*  Reserved  */
  { 0xBF, 0x00 },  /*  Reserved  */
  { 0xC0, 0x00 },  /*  Reserved  */
  { 0xC1, 0x00 },  /*  Reserved  */
  { 0xC2, 0x00 },  /*  Reserved  */
  { 0xC3, 0x00 },  /*  Reserved  */
  { 0xC4, 0x00 },  /*  Reserved  */
  { 0xC5, 0x00 },  /*  Reserved  */
  { 0xC6, 0x00 },  /*  Reserved  */
  { 0xC7, 0x00 },  /*  Reserved  */
  { 0xC8, 0x00 },  /*  Reserved  */
  { 0xC9, 0x00 },  /*  Reserved  */
  { 0xCA, 0x00 },  /*  Reserved  */
  { 0xCB, 0x00 },  /*  Reserved  */
  { 0xCC, 0x00 },  /*  Reserved  */
  { 0xCD, 0x00 },  /*  Reserved  */
  { 0xCE, 0x00 },  /*  Reserved  */
  { 0xCF, 0x00 },  /*  Reserved  */
  { 0xD0, 0x00 },  /*  Reserved  */
  { 0xD1, 0x00 },  /*  Reserved  */
  { 0xD2, 0x00 },  /*  Reserved  */
  { 0xD3, 0x00 },  /*  Reserved  */
  { 0xD4, 0x00 },  /*  Reserved  */
  { 0xD5, 0x00 },  /*  Reserved  */
  { 0xD6, 0x00 },  /*  Reserved  */
  { 0xD7, 0x00 },  /*  Reserved  */
  { 0xD8, 0x00 },  /*  Reserved  */
  { 0xD9, 0x00 },  /*  Reserved  */
  { 0xDA, 0x00 },  /*  Reserved  */
  { 0xDB, 0x00 },  /*  Reserved  */
  { 0xDC, 0x00 },  /*  Reserved  */
  { 0xDD, 0x00 },  /*  Reserved  */
  { 0xDE, 0x00 },  /*  Reserved  */
  { 0xDF, 0x00 },  /*  Reserved  */
  { 0xE0, 0x00 },  /*  Reserved  */
  { 0xE1, 0x00 },  /*  Reserved  */
  { 0xE2, 0x00 },  /*  Reserved  */
  { 0xE3, 0x00 },  /*  Reserved  */
  { 0xE4, 0x00 },  /*  Reserved  */
  { 0xE5, 0x00 },  /*  Reserved  */
  { 0xE6, 0x00 },  /*  Reserved  */
  { 0xE7, 0x00 },  /*  Reserved  */
  { 0xE8, 0x00 },  /*  Reserved  */
  { 0xE9, 0x00 },  /*  Reserved  */
  { 0xEA, 0x00 },  /*  Reserved  */
  { 0xEB, 0x00 },  /*  Reserved  */
  { 0xEC, 0x00 },  /*  Reserved  */
  { 0xED, 0x00 },  /*  Reserved  */
  { 0xEE, 0x00 },  /*  Reserved  */
  { 0xEF, 0x00 },  /*  Reserved  */
  { 0xF0, 0x00 },  /*  Reserved  */
  { 0xF1, 0x00 },  /*  Reserved  */
  { 0xF2, 0x00 },  /*  Reserved  */
  { 0xF3, 0x00 },  /*  Reserved  */
  { 0xF4, 0x00 },  /*  Reserved  */
  { 0xF5, 0x00 },  /*  Reserved  */
  { 0xF6, 0x00 },  /*  Reserved  */
  { 0xF7, 0x00 },  /*  Reserved  */
  { 0xF8, 0x00 },  /*  Reserved  */
  { 0xF9, 0x00 },  /*  Reserved  */
  { 0xFA, 0x00 },  /*  Reserved  */
  { 0xFB, 0x00 },  /*  Reserved  */
  { 0xFC, 0x00 },  /*  Reserved  */
  { 0xFD, 0x00 },  /*  Reserved  */
  { 0xFE, 0x00 },  /*  Reserved  */
  { 0xFF, 0x00 },  /*  Reserved  */
  { 0x100, 0x38 },  /*  AK7738_100_DEVICE_ID  */
  { 0x101, 0x02 },  /*  AK7738_101_REVISION_NUM  */
  { 0x102, 0xF0 },  /*  AK7738_102_DSP_ERROR_STATUS  */
  { 0x103, 0x00 },  /*  AK7738_103_SRC_STATUS  */
  { 0x104, 0x80 },  /*  AK7738_104_STO_READ_OUT  */
  { 0x105, 0x00 },  /*  AK7738_105_MICGAIN_READ  */
  { 0x106, 0x00 },  /*  AK7738_VIRT_106_DSP1OUT1_MIX  */
  { 0x107, 0x00 },  /*  AK7738_VIRT_107_DSP1OUT2_MIX  */
  { 0x108, 0x00 },  /*  AK7738_VIRT_108_DSP1OUT3_MIX  */
  { 0x109, 0x00 },  /*  AK7738_VIRT_109_DSP1OUT4_MIX  */
  { 0x10A, 0x00 },  /*  AK7738_VIRT_10A_DSP1OUT5_MIX  */
  { 0x10B, 0x00 },  /*  AK7738_VIRT_10B_DSP1OUT6_MIX  */
  { 0x10C, 0x00 },  /*  AK7738_VIRT_10C_DSP2OUT1_MIX  */
  { 0x10D, 0x00 },  /*  AK7738_VIRT_10D_DSP2OUT2_MIX  */
  { 0x10E, 0x00 },  /*  AK7738_VIRT_10E_DSP2OUT3_MIX  */
  { 0x10F, 0x00 },  /*  AK7738_VIRT_10F_DSP2OUT4_MIX  */
  { 0x110, 0x00 },  /*  AK7738_VIRT_110_DSP2OUT5_MIX  */
  { 0x111, 0x00 },  /*  AK7738_VIRT_111_DSP2OUT6_MIX  */
};

/* MIC Input Volume control:
 * from 0 to 36 dB (quantity of each step is various) */
static DECLARE_TLV_DB_MINMAX(mgnl_tlv, 0, 3600);
static DECLARE_TLV_DB_MINMAX(mgnr_tlv, 0, 3600);

/* ADC, ADC2 Digital Volume control:
 * from -103.5 to 24 dB in 0.5 dB steps (mute instead of -103.5 dB) */
static DECLARE_TLV_DB_SCALE(voladc_tlv, -10350, 50, 0);

/* DAC Digital Volume control:
 * from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB) */
static DECLARE_TLV_DB_SCALE(voldac_tlv, -11550, 50, 0);

static const char *clkosel_texts[] = {
	"12.288MHz", "24.576MHz", "8.192MHz", "6.144MHz",
	"4.096MHz", "2.048MHz", "INT_MCLK1", "INT_MCLK2"
};

static const struct soc_enum ak7738_clkosel_enum[] = {
	SOC_ENUM_SINGLE(AK7738_13_CLOKO_OUTPUT_SETTING, 0, ARRAY_SIZE(clkosel_texts), clkosel_texts),
};

static const char *sdselbit_texts[] = {
	"Low", "SD1", "SD2", "SD3", "SD4", "SD5", "SD6", "SD7"
};

static const struct soc_enum ak7738_sdsel_enum[] = {
	SOC_ENUM_SINGLE(AK7738_17_SYNCDOMAIN_SELECT3, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_18_SYNCDOMAIN_SELECT4, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_18_SYNCDOMAIN_SELECT4, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_19_SYNCDOMAIN_SELECT5, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_19_SYNCDOMAIN_SELECT5, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1A_SYNCDOMAIN_SELECT6, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1A_SYNCDOMAIN_SELECT6, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1B_SYNCDOMAIN_SELECT7, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1B_SYNCDOMAIN_SELECT7, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1C_SYNCDOMAIN_SELECT8, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1C_SYNCDOMAIN_SELECT8, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1D_SYNCDOMAIN_SELECT9, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1D_SYNCDOMAIN_SELECT9, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1E_SYNCDOMAIN_SELECT10, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1E_SYNCDOMAIN_SELECT10, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1F_SYNCDOMAIN_SELECT11, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_1F_SYNCDOMAIN_SELECT11, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_20_SYNCDOMAIN_SELECT12, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_20_SYNCDOMAIN_SELECT12, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_21_SYNCDOMAIN_SELECT13, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK7738_21_SYNCDOMAIN_SELECT13, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
};

static const char *fsmodebit_texts[] = {
	"8kHz:8kHz", "12kHz:12kHz", "16kHz:16kHz", "24kHz:24kHz",
	"32kHz:32kHz", "32kHz:16kHz", "32kHz:8kHz", "48kHz:48kHz",
	"48kHz:24kHz", "48kHz:16kHz", "48kHz:8kHz", "96kHz:96kHz",
	"96kHz:48kHz", "96kHz:32kHz", "96kHz:24kHz", "96kHz:16kHz",
	"96kHz:8kHz", "192kHz:192kHz", "192kHz:96kHz", "192kHz:48kHz",
	"192kHz:32kHz", "192kHz:16kHz"
};

static const struct soc_enum ak7738_fsmode_enum[] = {
	SOC_ENUM_SINGLE(AK7738_01_STSCLOCK_SETTING2, 0, ARRAY_SIZE(fsmodebit_texts), fsmodebit_texts),
};

// ak7738_set_dai_fmt() takes priority
// This is for SD that is not assigned to BICK/LRCK pin
static const char *msnbit_texts[] = {
	"Slave", "Master"
};

static const struct soc_enum ak7738_msnbit_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(msnbit_texts), msnbit_texts),
};

static const struct soc_enum ak7738_portsdsel_enum[] = {
	SOC_ENUM_SINGLE(AK7738_15_SYNCDOMAIN_SELECT1, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBICK1[2:0]
	SOC_ENUM_SINGLE(AK7738_15_SYNCDOMAIN_SELECT1, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBICK2[2:0]
	SOC_ENUM_SINGLE(AK7738_16_SYNCDOMAIN_SELECT2, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBICK3[2:0]
	SOC_ENUM_SINGLE(AK7738_16_SYNCDOMAIN_SELECT2, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBICK4[2:0]
	SOC_ENUM_SINGLE(AK7738_17_SYNCDOMAIN_SELECT3, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBICK5[2:0]
};

static const char *mdivbit_texts[] = {
	"122.88MHz", "61.44MHz", "40.96MHz", "30.72MHz"
};

static const struct soc_enum ak7738_mdiv_enum[] = {
	SOC_ENUM_SINGLE(AK7738_11_BUS_DSP_CLOCK_SETTING, 2, ARRAY_SIZE(mdivbit_texts), mdivbit_texts),
};

static const char *sdcks_texts[] = {
	"Low", "PLLMCLK", "XTI",
};

static const struct soc_enum ak7738_sdcks_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sdcks_texts), sdcks_texts),
};

static const char *busclock_texts[] = {
	"24.576MHz", "12.288MHz", "6.144MHz"
};

static const struct soc_enum ak7738_busclock_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(busclock_texts), busclock_texts),
};

static int ak7738_write_cram(
    struct snd_soc_codec *codec, int nDSPNo, int addr, int len,
    unsigned char
	*cram_data);

static int setBUSClock(struct snd_soc_codec *codec)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int value;

	if(ak7738->BUSMclk > 2){
		akdbgprt("[AK7738] %s Invalid Value selected!\n",__FUNCTION__);
		return(-EINVAL);
	}
	else{
	value = 5;
	value <<= ak7738->BUSMclk;
	value--;

	snd_soc_write(codec, AK7738_12_BUS_CLOCK_SETTING2, value);
	snd_soc_update_bits(codec, AK7738_11_BUS_DSP_CLOCK_SETTING, 0x30, 0x10);  //  Clock Source : PLLMCLK

	return(0);
	}
}

static int get_bus_clock(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->BUSMclk;

    return 0;
}

static int set_bus_clock(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if ( ak7738->BUSMclk != currMode ) {
		ak7738->BUSMclk = currMode;
		setBUSClock(codec);
	}

    return 0;
}

static const char *sd_fs_texts[] = {
	"8kHz", "12kHz",  "16kHz", "24kHz",
    "32kHz", "48kHz", "96kHz", "192kHz"
};

static int sdfstab[] = {
	8000, 12000, 16000, 24000,
    32000, 48000, 96000, 192000
};

static const char *sd_bick_texts[] = {
	"64fs", "48fs", "32fs",  "128fs", "256fs"
};

static int sdbicktab[] = {
	64, 48, 32, 128, 256
};

static const struct soc_enum ak7738_sd_fs_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_fs_texts), sd_fs_texts),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_bick_texts), sd_bick_texts),
};


static int XtiFsTab[] = {
	12288000,  18432000
};

static int PLLInFsTab[] = {
	256000, 384000, 512000, 768000, 1024000,
    1152000, 1536000, 2048000, 2304000, 3072000,
    4096000, 4608000, 6144000, 8192000, 9216000,
    12288000, 18432000, 24576000
};

static int setPLLOut(
struct snd_soc_codec *codec)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int nPLLInFs;
	int value, nBfs, fs;
	int n, nMax;

	value = ( ak7738->PLLInput << 5 );
	snd_soc_update_bits(codec, AK7738_00_STSCLOCK_SETTING1, 0xE0, value);

	if ( ak7738->PLLInput == 0 ) {
		nPLLInFs = XtiFsTab[ak7738->XtiFs];
	}
	else {
		nBfs = sdbicktab[ak7738->SDBick[ak7738->PLLInput-1]];
		fs = sdfstab[ak7738->SDfs[ak7738->PLLInput-1]];
		akdbgprt("[AK7738] %s nBfs=%d fs=%d\n",__FUNCTION__, nBfs, fs);
		nPLLInFs = nBfs * fs;
		if ( ( fs % 4000 ) != 0 ) {
			nPLLInFs *= 441;
			nPLLInFs /= 480;
		}
	}

	n = 0;
	nMax = sizeof(PLLInFsTab) / sizeof(PLLInFsTab[0]);

	do {
		akdbgprt("[AK7738] %s n=%d PLL:%d %d\n",__FUNCTION__, n, nPLLInFs, PLLInFsTab[n]);
		if ( nPLLInFs == PLLInFsTab[n] ) break;
		n++;
	} while ( n < nMax);


	if ( n == nMax ) {
		pr_err("%s: PLL1 setting Error!\n", __func__);
		return(-1);
	}

	snd_soc_update_bits(codec, AK7738_00_STSCLOCK_SETTING1, 0x1F, n);

	return(0);

}

static int setSDMaster(
struct snd_soc_codec *codec,
int nSDNo,
int nMaster)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int addr;
	int value;


		if ( nSDNo >= 7 ) return(0);

		ak7738->Master[nSDNo] = nMaster;

		addr = AK7738_03_SYNCDOMAIN1_SETTING1 + ( 2 * nSDNo );
		value = (nMaster << 7) + (ak7738->SDCks[nSDNo] << 4);
		snd_soc_update_bits(codec, addr, 0xF0, value);

		return(0);

}

static int setSDClock(
struct snd_soc_codec *codec,
int nSDNo)
{

	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int addr;
	int fs, bickfs;
	int sdv, bdv;

		if ( nSDNo >= NUM_SYNCDOMAIN ) return(0);

		fs = sdfstab[ak7738->SDfs[nSDNo]];
		bickfs = sdbicktab[ak7738->SDBick[nSDNo]] * fs;

		addr = AK7738_03_SYNCDOMAIN1_SETTING1 + ( 2 * nSDNo );


		if ( ak7738->SDCks[nSDNo] == 2 ) {
			bdv = XtiFsTab[ak7738->XtiFs] / bickfs;
		}
		else {
			bdv = 122880000 / bickfs;
		}

		sdv = ak7738->SDBick[nSDNo];
		akdbgprt("[AK7738]%s, BDV=%d, SDV=%d\n",__FUNCTION__, bdv, sdbicktab[sdv]);
		bdv--;
		if ( bdv > 255) {
		pr_err("%s: BDV Error! SD No = %d, bdv bit = %d\n", __func__, nSDNo, bdv);
		bdv = 255;
		}
		snd_soc_update_bits(codec, addr, 0x0F, sdv);
		addr++;
		snd_soc_write(codec, addr, bdv);
		if ( ak7738->PLLInput == (nSDNo + 1) ) {
			setPLLOut(codec);
		}
		return(0);
}

static int get_sd1_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDCks[0];

    return 0;
}

static int set_sd1_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 3){
	if ( ak7738->SDCks[0] != currMode ) {
	ak7738->SDCks[0] = currMode;
	setSDClock(codec, 0);
	setSDMaster(codec, 0, ak7738->Master[0]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd2_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDCks[1];

    return 0;
}

static int set_sd2_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 3){
	if ( ak7738->SDCks[1] != currMode ) {
	ak7738->SDCks[1] = currMode;
	setSDClock(codec, 1);
	setSDMaster(codec, 1, ak7738->Master[1]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd3_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDCks[2];

    return 0;
}

static int set_sd3_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 3){
	if ( ak7738->SDCks[2] != currMode ) {
	ak7738->SDCks[2] = currMode;
	setSDClock(codec, 2);
	setSDMaster(codec, 2, ak7738->Master[2]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd4_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDCks[3];

    return 0;
}

static int set_sd4_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 3){
	if ( ak7738->SDCks[3] != currMode ) {
	ak7738->SDCks[3] = currMode;
	setSDClock(codec, 3);
	setSDMaster(codec, 3, ak7738->Master[3]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd5_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDCks[4];

    return 0;
}

static int set_sd5_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 3){
	if ( ak7738->SDCks[4] != currMode ) {
	ak7738->SDCks[4] = currMode;
	setSDClock(codec, 4);
	setSDMaster(codec, 4, ak7738->Master[4]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd6_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDCks[5];

    return 0;
}

static int set_sd6_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 3){
	if ( ak7738->SDCks[5] != currMode ) {
	ak7738->SDCks[5] = currMode;
	setSDClock(codec, 5);
	setSDMaster(codec, 5, 0);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd7_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDCks[6];

    return 0;
}

static int set_sd7_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 3){
	if ( ak7738->SDCks[6] != currMode ) {
	ak7738->SDCks[6] = currMode;
	setSDClock(codec, 6);
	setSDMaster(codec, 6, 0);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd1_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->Master[0];

    return 0;
}

static int set_sd1_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 0, currMode);

    return 0;
}


static int get_sd2_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->Master[1];

    return 0;
}

static int set_sd2_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 1, currMode);

    return 0;
}


static int get_sd3_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->Master[2];

    return 0;
}

static int set_sd3_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 2, currMode);

    return 0;
}


static int get_sd4_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->Master[3];

    return 0;
}

static int set_sd4_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 3, currMode);

    return 0;
}


static int get_sd5_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->Master[4];

    return 0;
}

static int set_sd5_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 4, currMode);

    return 0;
}

static int get_sd1_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDfs[0];

    return 0;
}

static int set_sd1_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
	if ( ak7738->SDfs[0] != currMode ) {
		ak7738->SDfs[0] = currMode;
		setSDClock(codec, 0);
		setSDMaster(codec, 0, ak7738->Master[0]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDfs[1];

    return 0;
}

static int set_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
	if ( ak7738->SDfs[1] != currMode ) {
		ak7738->SDfs[1] = currMode;
		setSDClock(codec, 1);
		setSDMaster(codec, 1, ak7738->Master[1]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd3_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDfs[2];

    return 0;
}

static int set_sd3_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
	if ( ak7738->SDfs[2] != currMode ) {
		ak7738->SDfs[2] = currMode;
		setSDClock(codec, 2);
		setSDMaster(codec, 2,  ak7738->Master[2]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd4_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDfs[3];

    return 0;
}


static int set_sd4_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
	if ( ak7738->SDfs[3] != currMode ) {
		ak7738->SDfs[3] = currMode;
		setSDClock(codec, 3);
		setSDMaster(codec, 3, ak7738->Master[3]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd5_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDfs[4];

    return 0;
}


static int set_sd5_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
	if ( ak7738->SDfs[4] != currMode ) {
		ak7738->SDfs[4] = currMode;
		setSDClock(codec, 4);
		setSDMaster(codec, 4, ak7738->Master[4]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd6_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDfs[5];

    return 0;
}


static int set_sd6_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
	if ( ak7738->SDfs[5] != currMode ) {
		ak7738->SDfs[5] = currMode;
		setSDClock(codec, 5);
		setSDMaster(codec, 5, ak7738->Master[5]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd7_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDfs[6];

    return 0;
}


static int set_sd7_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
	if ( ak7738->SDfs[6] != currMode ) {
		ak7738->SDfs[6] = currMode;
		setSDClock(codec, 6);
		setSDMaster(codec, 6, ak7738->Master[6]);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDBick[0];

    return 0;
}

static int set_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
	if ( ak7738->SDBick[0] != currMode ) {
		ak7738->SDBick[0] = currMode;
		setSDClock(codec, 0);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDBick[1];

    return 0;
}

static int set_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
	if ( ak7738->SDBick[1] != currMode ) {
		ak7738->SDBick[1] = currMode;
		setSDClock(codec, 1);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd3_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDBick[2];

    return 0;
}

static int set_sd3_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
	if ( ak7738->SDBick[2] != currMode ) {
		ak7738->SDBick[2] = currMode;
		setSDClock(codec, 2);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd4_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDBick[3];

    return 0;
}

static int set_sd4_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
	if ( ak7738->SDBick[3] != currMode ) {
		ak7738->SDBick[3] = currMode;
		setSDClock(codec, 3);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd5_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDBick[4];

    return 0;
}

static int set_sd5_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
	if ( ak7738->SDBick[4] != currMode ) {
		ak7738->SDBick[4] = currMode;
		setSDClock(codec, 4);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd6_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDBick[5];

    return 0;
}

static int set_sd6_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
	if ( ak7738->SDBick[5] != currMode ) {
		ak7738->SDBick[5] = currMode;
		setSDClock(codec, 5);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd7_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->SDBick[6];

    return 0;
}

static int set_sd7_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
	if ( ak7738->SDBick[6] != currMode ) {
		ak7738->SDBick[6] = currMode;
		setSDClock(codec, 6);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static const char *pllinput_texts[]  = {
	"XTI", "BICK1",  "BICK2",  "BICK3", "BICK4", "BICK5"
};

static const char *xtifs_texts[]  = {
	"12.288MHz", "18.432MHz"
};

static const struct soc_enum ak7738_pllset_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllinput_texts), pllinput_texts),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(xtifs_texts), xtifs_texts),
};

static int get_pllinput(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->PLLInput;

    return 0;
}

static int set_pllinput(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 6){
	if ( ak7738->PLLInput != currMode ) {
		ak7738->PLLInput = currMode;
		setPLLOut(codec);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_xtifs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->XtiFs;

    return 0;
}

static int set_xtifs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
	if ( ak7738->XtiFs != currMode ) {
		ak7738->XtiFs = currMode;
		setPLLOut(codec);
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static const char *ak7738_tdmin_texts[] = {
	"TDMI OFF", "TDMI ON"
};

static const struct soc_enum ak7738_tdmin_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7738_tdmin_texts), ak7738_tdmin_texts),
};

static int get_sdin2_tdm(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->TDMIbit[0];

    return 0;
}

static int set_sdin2_tdm(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
	if ( ak7738->TDMIbit[0] != currMode ) {
		ak7738->TDMIbit[0] = currMode;
		ak7738->DIEDGEbit[1] = ak7738->TDMIbit[0];
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdin4_tdm(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->TDMIbit[1];

    return 0;
}

static int set_sdin4_tdm(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
	if ( ak7738->TDMIbit[1] != currMode ) {
		ak7738->TDMIbit[1] = currMode;
		ak7738->DIEDGEbit[3] = ak7738->TDMIbit[1];
	}
	}
	else {
	akdbgprt(" [AK7738] %s Invalid Value selected!\n",__FUNCTION__);
	}
   return 0;
}

static const char *ak7738_sd_ioformat_texts[] = {
	"24bit",
	"20bit",
	"16bit",
	"32bit",
};

static const struct soc_enum ak7738_slotlen_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7738_sd_ioformat_texts), ak7738_sd_ioformat_texts),
};

static int get_sdin1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DISLbit[0];

    return 0;
}
static void set_dislbit(struct snd_soc_codec *codec, struct ak7738_priv *ak7738,
			int port, int mode)
{
	int addr, value;
	if (mode < 4) {
		if (ak7738->DISLbit[port] != mode) {
			ak7738->DISLbit[port] = mode;
			addr = AK7738_48_SDIN1_FORMAT + port;
			value = mode << 4;
			snd_soc_update_bits(codec, addr, 0x30, value);
		}
	} else {
		akdbgprt(" [AK7738] %s Invalid Value selected!\n",
			 __FUNCTION__);
	}
}
static int set_sdin1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_dislbit(codec, ak7738, 0, currMode);
	return 0;
}

static int get_sdin2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DISLbit[1];

    return 0;
}

static int set_sdin2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_dislbit(codec, ak7738, 1, currMode);
	return 0;
}

static int get_sdin3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DISLbit[2];

    return 0;
}

static int set_sdin3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_dislbit(codec, ak7738, 2, currMode);
	return 0;
}

static int get_sdin4_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DISLbit[3];

    return 0;
}

static int set_sdin4_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_dislbit(codec, ak7738, 3, currMode);
	return 0;
}

static int get_sdin5_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DISLbit[4];

    return 0;
}

static int set_sdin5_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_dislbit(codec, ak7738, 4, currMode);
	return 0;
}

static void set_doslbit(struct snd_soc_codec *codec, struct ak7738_priv *ak7738, int port, int mode)
{
	int addr, value;
	if (mode < 4) {
		if (ak7738->DOSLbit[port] != mode) {
			ak7738->DOSLbit[port] = mode;
			addr = AK7738_4E_SDOUT1_FORMAT + port;
			value = mode << 4;
			snd_soc_update_bits(codec, addr, 0x30, value);
		}
	} else {
		akdbgprt(" [AK7738] %s Invalid Value selected!\n",
			 __FUNCTION__);
	}
}

static int get_sdout1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DOSLbit[0];

    return 0;
}

static int set_sdout1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_doslbit(codec, ak7738, 0, currMode);
	return 0;
}

static int get_sdout2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DOSLbit[1];

    return 0;
}

static int set_sdout2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_doslbit(codec, ak7738, 1, currMode);
	return 0;
}

static int get_sdout3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DOSLbit[2];

    return 0;
}

static int set_sdout3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_doslbit(codec, ak7738, 2, currMode);
	return 0;
}

static int get_sdout4_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DOSLbit[3];

    return 0;
}

static int set_sdout4_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_doslbit(codec, ak7738, 3, currMode);
	return 0;
}

static int get_sdout5_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->DOSLbit[4];

    return 0;
}

static int set_sdout5_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	set_doslbit(codec, ak7738, 4, currMode);
	return 0;
}

static const struct soc_enum ak7738_sd6_ioformat_enum[] = {
	SOC_ENUM_SINGLE(AK7738_4D_SDIN6_FORMAT, 4, ARRAY_SIZE(ak7738_sd_ioformat_texts), ak7738_sd_ioformat_texts),
	SOC_ENUM_SINGLE(AK7738_4D_SDIN6_FORMAT, 0, ARRAY_SIZE(ak7738_sd_ioformat_texts), ak7738_sd_ioformat_texts),
	SOC_ENUM_SINGLE(AK7738_53_SDOUT6_FORMAT, 4, ARRAY_SIZE(ak7738_sd_ioformat_texts), ak7738_sd_ioformat_texts),
	SOC_ENUM_SINGLE(AK7738_53_SDOUT6_FORMAT, 0, ARRAY_SIZE(ak7738_sd_ioformat_texts), ak7738_sd_ioformat_texts),
};


static const char *sdout_modeset_texts[] = {
	"Slow", "Fast"
};

static const struct soc_enum ak7738_sdout_modeset_enum[] = {
	SOC_ENUM_SINGLE(AK7738_54_SDOUT_MODE_SETTING, 0, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
	SOC_ENUM_SINGLE(AK7738_54_SDOUT_MODE_SETTING, 1, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
	SOC_ENUM_SINGLE(AK7738_54_SDOUT_MODE_SETTING, 2, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
	SOC_ENUM_SINGLE(AK7738_54_SDOUT_MODE_SETTING, 3, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
	SOC_ENUM_SINGLE(AK7738_54_SDOUT_MODE_SETTING, 4, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
	SOC_ENUM_SINGLE(AK7738_54_SDOUT_MODE_SETTING, 5, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
};


static const char *mixer_level_adjst_texts[] = {
	"No Shift", "1bit Right Shift", "Mute"
};
static const char *mixer_data_change_texts[] = {
	"Through", "Lin->LRout", "Rin->LRout", "Swap"
};

static const struct soc_enum ak7738_mixer_setting_enum[] = {
	SOC_ENUM_SINGLE(AK7738_5B_MIXER_A_SETTING, 0, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK7738_5B_MIXER_A_SETTING, 2, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK7738_5B_MIXER_A_SETTING, 4, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK7738_5B_MIXER_A_SETTING, 6, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK7738_5C_MIXER_B_SETTING, 0, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK7738_5C_MIXER_B_SETTING, 2, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK7738_5C_MIXER_B_SETTING, 4, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK7738_5C_MIXER_B_SETTING, 6, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
};

static const char *do2sel_texts[] = {
	"SDOUT2", "GPO0"
};
static const char *do3sel_texts[] = {
	"SDOUT3", "GPO1"
};
static const char *do4sel_texts[] = {
	"STO", "RDY", "SDOUT4"
};
static const char *do5sel_texts[] = {
	"SDOUT5", "GPO2"
};
static const char *do6sel_texts[] = {
	"SDOUT6", "GPO3", "DIT"
};

static const struct soc_enum ak7738_outportsel_enum[] = {
	SOC_ENUM_SINGLE(AK7738_57_OUTPUT_PORT_SELECT, 7, ARRAY_SIZE(do2sel_texts), do2sel_texts),
	SOC_ENUM_SINGLE(AK7738_57_OUTPUT_PORT_SELECT, 6, ARRAY_SIZE(do3sel_texts), do3sel_texts),
	SOC_ENUM_SINGLE(AK7738_57_OUTPUT_PORT_SELECT, 4, ARRAY_SIZE(do4sel_texts), do4sel_texts),
	SOC_ENUM_SINGLE(AK7738_57_OUTPUT_PORT_SELECT, 3, ARRAY_SIZE(do5sel_texts), do5sel_texts),
	SOC_ENUM_SINGLE(AK7738_57_OUTPUT_PORT_SELECT, 0, ARRAY_SIZE(do6sel_texts), do6sel_texts),
};

static const char *di2sel_texts[] = {
	"SDIN2", "JX0"
};
static const char *di3sel_texts[] = {
	"SDIN3", "JX1"
};
static const char *di5sel_texts[] = {
	"SDIN5", "JX0"
};
static const char *lck3sel_texts[] = {
	"LRCK3", "JX2"
};
static const char *bck3sel_texts[] = {
	"BICK3", "JX3"
};

static const struct soc_enum ak7738_inportsel_enum[] = {
	SOC_ENUM_SINGLE(AK7738_59_INPUT_PORT_SELECT, 4, ARRAY_SIZE(di2sel_texts), di2sel_texts),
	SOC_ENUM_SINGLE(AK7738_59_INPUT_PORT_SELECT, 3, ARRAY_SIZE(di3sel_texts), di3sel_texts),
	SOC_ENUM_SINGLE(AK7738_59_INPUT_PORT_SELECT, 2, ARRAY_SIZE(di5sel_texts), di5sel_texts),
	SOC_ENUM_SINGLE(AK7738_59_INPUT_PORT_SELECT, 1, ARRAY_SIZE(lck3sel_texts), lck3sel_texts),
	SOC_ENUM_SINGLE(AK7738_59_INPUT_PORT_SELECT, 0, ARRAY_SIZE(bck3sel_texts), bck3sel_texts),
};

static const char *adda_digfilsel_texts[] = {
	"Sharp",
	"Slow",
	"Short Delay Sharp",
	"Short Delay Slow",
};

static const struct soc_enum ak7738_adda_digfilsel_enum[] = {
	SOC_ENUM_SINGLE(AK7738_66_AIN_FILTER, 6, ARRAY_SIZE(adda_digfilsel_texts), adda_digfilsel_texts),
	SOC_ENUM_SINGLE(AK7738_6E_DAC_MUTE_FILTER, 0, ARRAY_SIZE(adda_digfilsel_texts), adda_digfilsel_texts),
};

static const char *adda_trantime_texts[] = {
	"1/fs",
	"4/fs",
};

static const struct soc_enum ak7738_adda_trantime_enum[] = {
	SOC_ENUM_SINGLE(AK7738_67_ADC_MUTE_HPF, 7, ARRAY_SIZE(adda_trantime_texts), adda_trantime_texts),
	SOC_ENUM_SINGLE(AK7738_6E_DAC_MUTE_FILTER, 7, ARRAY_SIZE(adda_trantime_texts), adda_trantime_texts),
};

static const char *dsm_ckset_texts[] = {
	"12.288MHz",
	"Fs based",
};

static const struct soc_enum ak7738_dsm_ckset_enum[] = {
	SOC_ENUM_SINGLE(AK7738_6E_DAC_MUTE_FILTER, 2, ARRAY_SIZE(dsm_ckset_texts), dsm_ckset_texts),
};

static const char *src_softmute_texts[] = {
	"Manual", "Semi Auto"
};

static const struct soc_enum ak7738_src_softmute_enum[] = {
	SOC_ENUM_SINGLE(AK7738_70_SRC_MUTE_SETTING, 3, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
	SOC_ENUM_SINGLE(AK7738_70_SRC_MUTE_SETTING, 2, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
	SOC_ENUM_SINGLE(AK7738_70_SRC_MUTE_SETTING, 1, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
	SOC_ENUM_SINGLE(AK7738_70_SRC_MUTE_SETTING, 0, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
};

static const struct soc_enum ak7738_fsconv_mute_enum[] = {
	SOC_ENUM_SINGLE(AK7738_71_FSCONV_MUTE_SETTING, 3, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
	SOC_ENUM_SINGLE(AK7738_71_FSCONV_MUTE_SETTING, 2, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
};

static const char *fsconv_valchanel_texts[] = {
	"Lch", "Rch"
};

static const struct soc_enum ak7738_fsconv_valchanel_enum[] = {
	SOC_ENUM_SINGLE(AK7738_71_FSCONV_MUTE_SETTING, 1, ARRAY_SIZE(fsconv_valchanel_texts), fsconv_valchanel_texts),
	SOC_ENUM_SINGLE(AK7738_71_FSCONV_MUTE_SETTING, 0, ARRAY_SIZE(fsconv_valchanel_texts), fsconv_valchanel_texts),
};

// Added for AK7738
static const struct soc_enum ak7738_firmware_enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7738_firmware_pram), ak7738_firmware_pram),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7738_firmware_cram), ak7738_firmware_cram),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7738_firmware_ofreg), ak7738_firmware_ofreg),
};

static int ak7738_firmware_write_ram(struct snd_soc_codec *codec, u16 mode, u16 cmd);
static int ak7738_set_status(struct snd_soc_codec *codec, enum ak7738_status status);

static int get_DSP_write_pram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

   /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7738->DSPPramMode;

    return 0;

}

static int set_DSP_write_pram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	akdbgprt("\t%s PRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7738_firmware_write_ram(codec, RAMTYPE_PRAM, currMode);
	if ( ret != 0 ) return(-1);

	ak7738->DSPPramMode = currMode;

	return(0);
}

static int get_DSP_write_cram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7738->DSPCramMode;

    return 0;

}

static int set_DSP_write_cram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	akdbgprt("\t%s CRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7738_firmware_write_ram(codec, RAMTYPE_CRAM, currMode);
	if ( ret != 0 ) return(-1);

	ak7738->DSPCramMode =currMode;

	return(0);
}

static int get_DSP_write_ofreg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7738->DSPOfregMode;

    return 0;

}

static int set_DSP_write_ofreg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7738_firmware_write_ram(codec, RAMTYPE_OFREG, currMode);
	if ( ret != 0 ) return(-1);

	ak7738->DSPOfregMode =currMode;

	return(0);
}

static const char *dsp_connect_texts[] = {
	"Off", "On",
};

static const struct soc_enum ak7738_dsp_connect_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dsp_connect_texts), dsp_connect_texts),
};


static int get_dsp2_dsp1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = (1 & ak7738->nDSP2);

    return 0;
}

static int set_dsp2_dsp1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	ak7738->nDSP2 &= 0x2;
	ak7738->nDSP2 |= currMode;


    return 0;
}

static int get_subdsp_dsp1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7738->nSubDSP;

    return 0;
}

static int set_subdsp_dsp1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	ak7738->nSubDSP = currMode;

    return 0;
}


static const char *dsp_assign_pram_texts[] = { // dsp1/dsp2
	"4096/4096", "2048/6144", "6144/2048", "8192/0"
};

static const char *dsp_assign_cram_texts[] = { // dsp1/dsp2
	"4096/2048", "2048/4096", "6144/0"
};

static const char *dsp_assign_dlram_texts[] = { // dsp1/dsp2
	"12288/8192", "16384/4096", "8192/12288",
	"4096/16384", "0/20480", "20480/0",
};

static const char *dsp_ram_clear_texts[] = { // dsp1/dsp2
	"Clear", "not Clear"
};

static const struct soc_enum ak7738_dsp_assign_enum[] = {
	SOC_ENUM_SINGLE(AK7738_73_DSP_MEMORY_ASSIGNMENT1, 4, ARRAY_SIZE(dsp_assign_pram_texts), dsp_assign_pram_texts),
	SOC_ENUM_SINGLE(AK7738_73_DSP_MEMORY_ASSIGNMENT1, 2, ARRAY_SIZE(dsp_assign_cram_texts), dsp_assign_cram_texts),
	SOC_ENUM_SINGLE(AK7738_73_DSP_MEMORY_ASSIGNMENT1, 0, ARRAY_SIZE(dsp_assign_cram_texts), dsp_assign_cram_texts),
	SOC_ENUM_SINGLE(AK7738_74_DSP_MEMORY_ASSIGNMENT2, 0, ARRAY_SIZE(dsp_assign_dlram_texts), dsp_assign_dlram_texts),
	SOC_ENUM_SINGLE(AK7738_74_DSP_MEMORY_ASSIGNMENT2, 4, ARRAY_SIZE(dsp_ram_clear_texts), dsp_ram_clear_texts),
	SOC_ENUM_SINGLE(AK7738_74_DSP_MEMORY_ASSIGNMENT2, 5, ARRAY_SIZE(dsp_ram_clear_texts), dsp_ram_clear_texts),
	SOC_ENUM_SINGLE(AK7738_74_DSP_MEMORY_ASSIGNMENT2, 6, ARRAY_SIZE(dsp_ram_clear_texts), dsp_ram_clear_texts),
};

static const char *dsp_addressing_texts[] = {
	"Ring", "Linear"
};

static const struct soc_enum ak7738_dsp_addressing_enum[] = {
	SOC_ENUM_SINGLE(AK7738_75_DSP12_DRAM_SETTING, 7, ARRAY_SIZE(dsp_addressing_texts), dsp_addressing_texts),
	SOC_ENUM_SINGLE(AK7738_75_DSP12_DRAM_SETTING, 6, ARRAY_SIZE(dsp_addressing_texts), dsp_addressing_texts),
	SOC_ENUM_SINGLE(AK7738_75_DSP12_DRAM_SETTING, 3, ARRAY_SIZE(dsp_addressing_texts), dsp_addressing_texts),
	SOC_ENUM_SINGLE(AK7738_75_DSP12_DRAM_SETTING, 2, ARRAY_SIZE(dsp_addressing_texts), dsp_addressing_texts),
	SOC_ENUM_SINGLE(AK7738_76_DSP1_DLRAM_SETTING, 5, ARRAY_SIZE(dsp_addressing_texts), dsp_addressing_texts),
	SOC_ENUM_SINGLE(AK7738_77_DSP2_DLRAM_SETTING, 5, ARRAY_SIZE(dsp_addressing_texts), dsp_addressing_texts),
};

static const char *dsp_dram_bank_size_texts[] = {
	"1024", "2048", "3072", "4096"
};

static const char *dsp_dlram_bank_size_texts[] = {
	"0", "2048", "4096", "6144", "8192", "10240",
	 "12288","14336", "16384", "18432", "20480"
};

static const struct soc_enum ak7738_dsp_bank_size_enum[] = {
	SOC_ENUM_SINGLE(AK7738_75_DSP12_DRAM_SETTING, 4, ARRAY_SIZE(dsp_dram_bank_size_texts), dsp_dram_bank_size_texts),
	SOC_ENUM_SINGLE(AK7738_75_DSP12_DRAM_SETTING, 0, ARRAY_SIZE(dsp_dram_bank_size_texts), dsp_dram_bank_size_texts),
	SOC_ENUM_SINGLE(AK7738_76_DSP1_DLRAM_SETTING, 0, ARRAY_SIZE(dsp_dlram_bank_size_texts), dsp_dlram_bank_size_texts),
	SOC_ENUM_SINGLE(AK7738_77_DSP2_DLRAM_SETTING, 0, ARRAY_SIZE(dsp_dlram_bank_size_texts), dsp_dlram_bank_size_texts),
};

static const char *dlram_sampling_mode_texts[] = {
	"1sampling", "2sampling", "4sampling", "8sampling"
};

static const struct soc_enum ak7738_dlram_sampling_mode_enum[] = {
	SOC_ENUM_SINGLE(AK7738_76_DSP1_DLRAM_SETTING, 6, ARRAY_SIZE(dlram_sampling_mode_texts), dlram_sampling_mode_texts),
	SOC_ENUM_SINGLE(AK7738_77_DSP2_DLRAM_SETTING, 6, ARRAY_SIZE(dlram_sampling_mode_texts), dlram_sampling_mode_texts),
};

static const char *dlram_pointer_mode_texts[] = {
	"OFREG", "DBUS"
};

static const struct soc_enum ak7738_dlram_pointer_mode_enum[] = {
	SOC_ENUM_SINGLE(AK7738_78_FFT_DLP0_SETTING, 7, ARRAY_SIZE(dlram_pointer_mode_texts), dlram_pointer_mode_texts),
	SOC_ENUM_SINGLE(AK7738_78_FFT_DLP0_SETTING, 3, ARRAY_SIZE(dlram_pointer_mode_texts), dlram_pointer_mode_texts),
};

static const char *dsp_wave_point_texts[] = {
	"128",
	"256",
	"512",
	"1024",
	"2048",
	"4096",
};

static const struct soc_enum ak7738_dsp_wave_point_enum[] = {
	SOC_ENUM_SINGLE(AK7738_78_FFT_DLP0_SETTING, 4, ARRAY_SIZE(dsp_wave_point_texts), dsp_wave_point_texts),
	SOC_ENUM_SINGLE(AK7738_78_FFT_DLP0_SETTING, 0, ARRAY_SIZE(dsp_wave_point_texts), dsp_wave_point_texts),
};

static const char *src_digfil_select_texts[] = {
	"HF", "Audio"
};

static const struct soc_enum ak7738_src_digfil_select_enum[] = {
	SOC_ENUM_SINGLE(AK7738_6F_SRC_CLOCK_SETTING, 4, ARRAY_SIZE(src_digfil_select_texts), src_digfil_select_texts),
};

static const char *pad_drive_texts[] = {
	"3.3V", "1.8V"
};

static const struct soc_enum ak7738_pad_drive_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pad_drive_texts), pad_drive_texts),
};

static int setPadDrive(
struct snd_soc_codec *codec,
int nPDSel, // Addr select
int nSft, // Bit select
int nDrv // Data set 3.3 or 1.8
)
{
	int addr;
	int value;
	int nDrv2 = 0;
	int nMask;

	nMask = (0x03 << nSft);

	switch (nDrv){
		case 0: nDrv2 = 0; // 3.3V
			break;
		case 1: nDrv2 = 3; // 1.8V
			break;
	}
	value = nDrv2 << nSft;
	addr = AK7738_91_PAD_DRIVE_SEL2 + nPDSel -2;

	snd_soc_update_bits(codec, addr, nMask, value);

	return 0;
}

static int ak7738_bit_read(
struct snd_soc_codec *codec,
int addr, int bit, int len)
{

	int value, ret = 0;
	value = (len & (snd_soc_read(codec, addr) >> bit));

	if (value == 0){
		ret = 0;
	}
	else if (value == 3){
		ret = 1;
	}
	else
		return (-1);
	return ret;
}

static int get_pad_drive_sd1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_91_PAD_DRIVE_SEL2, 0, 3 );

    return 0;
}

static int set_pad_drive_sd1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 2 ,0 , currMode);

    return 0;
}

static int get_pad_drive_sd2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_91_PAD_DRIVE_SEL2, 2, 3 );

    return 0;
}

static int set_pad_drive_sd2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 2 ,2 , currMode);

    return 0;
}

static int get_pad_drive_sd3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_91_PAD_DRIVE_SEL2, 4, 3 );

    return 0;
}

static int set_pad_drive_sd3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 2 ,4 , currMode);

    return 0;
}

static int get_pad_drive_sd4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_91_PAD_DRIVE_SEL2, 6, 3 );

    return 0;
}

static int set_pad_drive_sd4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 2 ,6 , currMode);

    return 0;
}
static int get_pad_drive_lr1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_93_PAD_DRIVE_SEL4, 4, 3 );

    return 0;
}

static int set_pad_drive_lr1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 4 ,4 , currMode);

    return 0;
}

static int get_pad_drive_lr2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_93_PAD_DRIVE_SEL4, 6, 3 );

    return 0;
}

static int set_pad_drive_lr2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 4 ,6 , currMode);

    return 0;
}

static int get_pad_drive_lr3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_92_PAD_DRIVE_SEL3, 0, 3 );

    return 0;
}

static int set_pad_drive_lr3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 3 ,0 , currMode);

    return 0;
}

static int get_pad_drive_lr4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_92_PAD_DRIVE_SEL3, 2, 3 );

    return 0;
}

static int set_pad_drive_lr4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 3 ,2 , currMode);

    return 0;
}

static int get_pad_drive_bk1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_94_PAD_DRIVE_SEL5, 0, 3 );

    return 0;
}

static int set_pad_drive_bk1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 5 ,0 , currMode);

    return 0;
}

static int get_pad_drive_bk2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_94_PAD_DRIVE_SEL5, 2, 3 );

    return 0;
}

static int set_pad_drive_bk2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 5 ,2 , currMode);

    return 0;
}

static int get_pad_drive_bk3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_94_PAD_DRIVE_SEL5, 4, 3 );

    return 0;
}

static int set_pad_drive_bk3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 5 ,4 , currMode);

    return 0;
}

static int get_pad_drive_bk4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    ucontrol->value.enumerated.item[0] = ak7738_bit_read(codec,AK7738_94_PAD_DRIVE_SEL5, 6, 3 );

    return 0;
}

static int set_pad_drive_bk4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setPadDrive(codec, 5 ,6 , currMode);

    return 0;
}

static int ak7738_reads(struct snd_soc_codec *codec, u8 *, size_t, u8 *, size_t);

#ifdef AK7738_DEBUG
static int test_read_ram(struct snd_soc_codec *codec, int mode)
{
	u8	tx[3], rx[512];
	int i, n, plen, clen;

	akdbgprt("*****[AK7738] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7738_set_status(codec, DOWNLOAD);
	mdelay(1);
	if ( mode < 3 ) {
		plen = 16;
		for ( n = 0 ; n < (5 * plen) ; n++ ) rx[n] = 0;
		tx[0] = (COMMAND_WRITE_PRAM & 0x7F) + mode;
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7738_reads(codec, tx, 3, rx, 5 * plen);
		printk("*****%s PRAM LEN = %d *******\n", __func__, plen);
		n = 0;
		for ( i = 0 ; i < plen ; i ++ ) {
			printk("PAddr=%x %x %x %x %x %x\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2], (int)rx[n+3], (int)rx[n+4]);
			n += 5;
		}
	}
	else if ( mode < 8 ){
		clen = 16;
		for ( n = 0 ; n < (3 * clen) ; n++ ) rx[n] = 0;
		if ( mode < 6 ) tx[0] =  (COMMAND_WRITE_CRAM & 0x7F) + (mode - 3);
		else  {
			tx[0] =  (COMMAND_WRITE_PRAM & 0x7F) + (mode - 5);
			clen = 16;
		}
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7738_reads(codec, tx, 3, rx, 3 * clen);
		printk("*****%s RAM CMD=%d,  LEN = %d*******\n", __func__, (int)tx[0], clen);
		n = 0;
		for ( i = 0 ; i < clen ; i ++ ) {
			printk("CAddr=%x %x %x %x\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2]);
			n += 3;
		}
	}
	mdelay(1);
	ak7738_set_status(codec, DOWNLOAD_FINISH);

	return(0);

}

static const char *test_reg_select[]   =
{
    "read AK7738 Reg 00:87",
    "read AK7738 Reg 90:105",
	"read DSP1 PRAM",
	"read DSP2 PRAM",
	"read DSP3 PRAM",
	"read DSP1 CRAM",
	"read DSP2 CRAM",
	"read DSP3 CRAM",
	"read DSP1 OFREG",
	"read DSP2 OFREG",
};

static const struct soc_enum ak7738_test_enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
};

static int nTestRegNo = 0;

static int get_test_reg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = nTestRegNo;

    return 0;
}

static int set_test_reg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    i, value;
	int	   regs, rege;

	nTestRegNo = currMode;

	if ( currMode < 2 ) {
		if ( currMode == 0 ) {
			regs = 0x00;
			rege = 0x87;
		}
		else {
			regs = 0x090;
			rege = 0x105;
		}
		for ( i = regs ; i <= rege ; i++ ){
			value = snd_soc_read(codec, i);
			printk("***AK7738 Addr,Reg=(%x, %x)\n", i, value);
		}
	}
	else if ( currMode < 9 ) {
		test_read_ram(codec, (currMode - 2));
	}

	return 0;

}
#endif

static const char *ditout_dither_texts[] = {
	"Normal", "Rounded"
};
static const char *ditout_cgmsa_texts[] = {
	"Copying permitted",
	"One generation copies",
	"Condition not used",
	"No copying permitted",
};
static const char *ditout_datasel_texts[] = {
	"Audio Data", "Digital Data"
};

static const char *ditout_category_texts[] = {
	"General",
	"Compact-disc",
	"Laser optical",
	"Mini disc",
	"DVD",
	"Other",
	"PCM",
	"Signal mixer",
	"SRC",
	"Sound sampler",
	"Sound processor",
	"Audio tape",
	"Video tape",
	"Compact cassette",
	"Magnetic disc",
	"Audio broadcast Japan",
	"Audio broadcast Europe",
	"Audio broadcast USA",
	"Electronic software",
	"Another standard",
	"Synthesizer",
	"Microphone",
	"ADC w/o copyright",
	"ADC w/ copyright",
	"Audio recorder",
	"Experimental",
};

static const unsigned int ditout_category_values[] = {
	0x00,
	0x01,
	0x09,
	0x49,
	0x19,
	0x79,
	0x02,
	0x12,
	0x1A,
	0x22,
	0x2A,
	0x03,
	0x0B,
	0x43,
	0x1B,
	0x04,
	0x0C,
	0x64,
	0x44,
	0x24,
	0x05,
	0x0D,
	0x06,
	0x16,
	0x08,
	0x40,
};

static const char *ditout_category2_texts[] = {
	"0",
	"1",
};

static const char *ditout_clk_accuracy_texts[] = {
	"Normal",
	"High-precision",
	"Variable Pitch",
	"Not Specified"
};

static const char *ditout_fs_set_texts[] = {
	"22.05kHz",
	"24kHz",
	"32kHz",
	"44.1kHz",
	"48kHz",
	"64kHz",
	"88.2kHz",
	"96kHz",
	"176.4kHz",
	"192kHz",
	"352.8kHz",
	"384kHz",
	"768kHz",
};
static const unsigned int ditout_fs_set_values[] = {
	0x04,
	0x06,
	0x03,
	0x00,
	0x02,
	0x0B,
	0x08,
	0x0A,
	0x0C,
	0x0E,
	0x0D,
	0x05,
	0x09,
};

static const char *ditout_origfs_set_texts[] = {
	"8kHz",
	"11.025kHz",
	"12kHz",
	"16kHz",
	"22.05kHz",
	"24kHz",
	"32kHz",
	"44.1kHz",
	"48kHz",
	"64kHz",
	"88.2kHz",
	"96kHz",
	"128kHz",
	"176.4kHz",
	"192kH",
	"not indicated",
};

static const unsigned int ditout_origfs_set_values[] = {
	0x06,
	0x0A,
	0x02,
	0x08,
	0x0B,
	0x09,
	0x0C,
	0x0F,
	0x0D,
	0x04,
	0x07,
	0x05,
	0x0E,
	0x03,
	0x01,
	0x00,
};

static const char *ditout_bitlen_set_texts[] = {
	"16bit",
	"17bit",
	"18bit",
	"19bit",
	"20bit",
	"21bit",
	"22bit",
	"23bit",
	"24bit",
	"not indicated",
};

static const unsigned int ditout_bitlen_set_values[] = {
	0x02,
	0x0C,
	0x04,
	0x08,
	0x03,
	0x0D,
	0x05,
	0x09,
	0x0B,
	0x00,
};

static const struct soc_enum ak7738_dit_statusbit_enum[] = {
	SOC_ENUM_SINGLE(AK7738_7E_DIT_STATUS_BIT1, 6, ARRAY_SIZE(ditout_dither_texts), ditout_dither_texts),
	SOC_ENUM_SINGLE(AK7738_7E_DIT_STATUS_BIT1, 4, ARRAY_SIZE(ditout_cgmsa_texts), ditout_cgmsa_texts),
	SOC_ENUM_SINGLE(AK7738_7E_DIT_STATUS_BIT1, 1, ARRAY_SIZE(ditout_datasel_texts), ditout_datasel_texts),
	SOC_VALUE_ENUM_SINGLE(AK7738_7F_DIT_STATUS_BIT2, 0, 0x7F,
					ARRAY_SIZE(ditout_category_texts), ditout_category_texts, ditout_category_values),
	SOC_ENUM_SINGLE(AK7738_7F_DIT_STATUS_BIT2, 7, ARRAY_SIZE(ditout_category2_texts), ditout_category2_texts),
	SOC_ENUM_SINGLE(AK7738_80_DIT_STATUS_BIT3, 4, ARRAY_SIZE(ditout_clk_accuracy_texts), ditout_clk_accuracy_texts),
	SOC_VALUE_ENUM_SINGLE(AK7738_80_DIT_STATUS_BIT3, 0, 0x0F,
					ARRAY_SIZE(ditout_fs_set_texts), ditout_fs_set_texts, ditout_fs_set_values),
	SOC_VALUE_ENUM_SINGLE(AK7738_81_DIT_STATUS_BIT4, 4, 0x0F,
					ARRAY_SIZE(ditout_origfs_set_texts), ditout_origfs_set_texts, ditout_origfs_set_values),
	SOC_VALUE_ENUM_SINGLE(AK7738_81_DIT_STATUS_BIT4, 0, 0x0F,
					ARRAY_SIZE(ditout_bitlen_set_texts), ditout_bitlen_set_texts, ditout_bitlen_set_values)

};

/* Next section is for AK7738_X9REF_FIRMWARE special kcontrol */
static int get_dsp2_switch1(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	akdbgprt("\t%s dsp2_switch1 mode =%d\n", __FUNCTION__,
		 ak7738->DSP2Switch1);
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7738->DSP2Switch1;
	return 0;
}

static int set_dsp2_switch1(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int currMode = ucontrol->value.enumerated.item[0];
	int dspNo = 2;
	unsigned char cram_switch_enable[] = {0x20, 0x0, 0x0};
	unsigned char cram_switch_disable[] = {0x0, 0x0, 0x0};
	int ret;
	akdbgprt("\t%s dsp2_switch1 mode =%d\n", __FUNCTION__, currMode);

	if (currMode == 0) {
		ret = ak7738_write_cram(
		    codec, dspNo, CRAM_ADDR_VOL_IN1_SWITCH_1,
		    sizeof(cram_switch_enable), cram_switch_enable);
		akdbgprt("\t%s dsp2_switch1 ret =%d\n", __FUNCTION__, ret);
		if (ret < 0)
			return (-1);
		akdbgprt("\t%s dsp2_switch1 ret =%d\n", __FUNCTION__,
			 ret);
		ret = ak7738_write_cram(
		    codec, dspNo, CRAM_ADDR_VOL_IN2_SWITCH_1,
		    sizeof(cram_switch_disable), cram_switch_disable);
	} else {
		ret = ak7738_write_cram(
		    codec, dspNo, CRAM_ADDR_VOL_IN2_SWITCH_1,
		    sizeof(cram_switch_enable), cram_switch_enable);
		if (ret < 0)
			return (-1);
		ret = ak7738_write_cram(
		    codec, dspNo, CRAM_ADDR_VOL_IN1_SWITCH_1,
		    sizeof(cram_switch_disable), cram_switch_disable);
	}
	if (ret < 0)
		return (-1);

	ak7738->DSP2Switch1 = currMode;

	return 0;
}
static const char *dsp2_switch1_txt[] = {"slot1", "slot5"};

static const struct soc_enum ak7738_dsp_component_txt_enum[] = {

    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dsp2_switch1_txt), dsp2_switch1_txt),

};


static const DECLARE_TLV_DB_SCALE(dsp_vol_tlv, 0, 255, 1);


/* 255 ->11.5db 0.5dB steps */
static int dsp_vol_to_param(int vol, char* param)
{
	if(vol > 255){
		return -1;
	}else if(vol < 0){
		return -1;
	} else if (vol == 0) {
		/*0 -> set to mute.*/
		param[0]=0;
		param[1]=0;
		param[2]=0;
		akdbgprt(
		    "\t%s dsp_vol_to_param mute, param 0x%x, 0x%x, 0x%x \n",
		    __FUNCTION__, param[0], param[1], param[2]);
		return 0;
	}
	param[0] = dsp_vol_table[vol * 3];
	param[1] = dsp_vol_table[vol * 3 + 1];
	param[2] = dsp_vol_table[vol * 3 + 2];
	akdbgprt("\t%s dsp_vol_to_param 0x%x, param 0x%x, 0x%x, 0x%x \n",
		 __FUNCTION__, vol, param[0], param[1], param[2]);
	return 0;
}

static int dsp1_hfp_vol_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	unsigned char cram_vol[3] = {0x0, 0x0, 0x0};
	int value = ucontrol->value.integer.value[0];
	int ret;
	int dspNo = 1;
	int max = mc->max;

	if (value > max)
		return -EINVAL;

	ret = dsp_vol_to_param(value, cram_vol);
	if (ret < 0)
		return (-1);

	ret = ak7738_write_cram(codec, dspNo, CRAM_ADDR_LEVEL_FADERSCO,
				sizeof(cram_vol), cram_vol);

	if (ret < 0)
		return (-1);

	ak7738->DSP1HFPVol = value;
	return 0;
}

static int dsp1_hfp_vol_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ak7738->DSP1HFPVol;
	akdbgprt("\t%s dsp1_hfp_vol val =%d max=%d\n", __FUNCTION__,
		 ak7738->DSP1HFPVol, ((struct soc_mixer_control *)kcontrol->private_value)->max);

	return 0;
}

static const struct snd_kcontrol_new ak7738_snd_controls[] = {
    SOC_SINGLE_TLV("MIC Input Volume L", AK7738_5D_MICAMP_GAIN, 4, 0x0F, 0,
		   mgnl_tlv),
    SOC_SINGLE_TLV("MIC Input Volume R", AK7738_5D_MICAMP_GAIN, 0, 0x0F, 0,
		   mgnr_tlv),

    SOC_SINGLE_TLV("ADC1 Digital Volume L", AK7738_5F_ADC1_LCH_VOLUME, 0, 0xFF,
		   1, voladc_tlv),
    SOC_SINGLE_TLV("ADC1 Digital Volume R", AK7738_60_ADC1_RCH_VOLUME, 0, 0xFF,
		   1, voladc_tlv),
    SOC_SINGLE_TLV("ADC2 Digital Volume L", AK7738_61_ADC2_LCH_VOLUME, 0, 0xFF,
		   1, voladc_tlv),
    SOC_SINGLE_TLV("ADC2 Digital Volume R", AK7738_62_ADC2_RCH_VOLUME, 0, 0xFF,
		   1, voladc_tlv),
    SOC_SINGLE_TLV("ADCM Digital Volume", AK7738_63_ADCM_VOLUME, 0, 0xFF, 1,
		   voladc_tlv),

    SOC_SINGLE_TLV("DAC1 Digital Volume L", AK7738_68_DAC1_LCH_VOLUME, 0, 0xFF,
		   1, voldac_tlv),
    SOC_SINGLE_TLV("DAC1 Digital Volume R", AK7738_69_DAC1_RCH_VOLUME, 0, 0xFF,
		   1, voldac_tlv),
    SOC_SINGLE_TLV("DAC2 Digital Volume L", AK7738_6A_DAC2_LCH_VOLUME, 0, 0xFF,
		   1, voldac_tlv),
    SOC_SINGLE_TLV("DAC2 Digital Volume R", AK7738_6B_DAC2_RCH_VOLUME, 0, 0xFF,
		   1, voldac_tlv),

    SOC_SINGLE("ADC1 Mute", AK7738_67_ADC_MUTE_HPF, 6, 1, 0),
    SOC_SINGLE("ADC2 Mute", AK7738_67_ADC_MUTE_HPF, 5, 1, 0),
    SOC_SINGLE("ADCM Mute", AK7738_67_ADC_MUTE_HPF, 4, 1, 0),

    SOC_SINGLE("DAC1 Mute", AK7738_6E_DAC_MUTE_FILTER, 6, 1, 0),
    SOC_SINGLE("DAC2 Mute", AK7738_6E_DAC_MUTE_FILTER, 5, 1, 0),

    SOC_SINGLE("CLK Out Enable", AK7738_13_CLOKO_OUTPUT_SETTING, 3, 1, 0),
    SOC_ENUM("CLK Out Select", ak7738_clkosel_enum[0]),

    SOC_ENUM("DSP1 Sync Domain", ak7738_sdsel_enum[0]),
    SOC_ENUM("DSP2 Sync Domain", ak7738_sdsel_enum[1]),
    SOC_ENUM("DSP1 Out1 Sync Domain", ak7738_sdsel_enum[2]),
    SOC_ENUM("DSP1 Out2 Sync Domain", ak7738_sdsel_enum[3]),
    SOC_ENUM("DSP1 Out3 Sync Domain", ak7738_sdsel_enum[4]),

    SOC_ENUM("SDIN1 Sync Domain", ak7738_sdsel_enum[5]),
    SOC_ENUM("SDIN2 Sync Domain", ak7738_sdsel_enum[6]),
    SOC_ENUM("SDIN3 Sync Domain", ak7738_sdsel_enum[7]),
    SOC_ENUM("SDIN4 Sync Domain", ak7738_sdsel_enum[8]),
    SOC_ENUM("SDIN5 Sync Domain", ak7738_sdsel_enum[9]),
    SOC_ENUM("SDIN6 Sync Domain", ak7738_sdsel_enum[10]),

    SOC_ENUM("SRC1 Out Sync Domain", ak7738_sdsel_enum[11]),
    SOC_ENUM("SRC2 Out Sync Domain", ak7738_sdsel_enum[12]),
    SOC_ENUM("SRC3 Out Sync Domain", ak7738_sdsel_enum[13]),
    SOC_ENUM("SRC4 Out Sync Domain", ak7738_sdsel_enum[14]),
    SOC_ENUM("FSCONV1 Out Sync Domain", ak7738_sdsel_enum[15]),
    SOC_ENUM("FSCONV2 Out Sync Domain", ak7738_sdsel_enum[16]),

    SOC_ENUM("MIXA Sync Domain", ak7738_sdsel_enum[17]),
    SOC_ENUM("MIXB Sync Domain", ak7738_sdsel_enum[18]),
    SOC_ENUM("ADC1 Sync Domain", ak7738_sdsel_enum[19]),
    SOC_ENUM("CODEC Sync Domain", ak7738_sdsel_enum[20]),

    SOC_ENUM("CODEC Sampling Frequency", ak7738_fsmode_enum[0]),
    SOC_ENUM_EXT("LRCK1/BICK1 Master", ak7738_msnbit_enum[0], get_sd1_ms,
		 set_sd1_ms),
    SOC_ENUM_EXT("LRCK2/BICK2 Master", ak7738_msnbit_enum[0], get_sd2_ms,
		 set_sd2_ms),
    SOC_ENUM_EXT("LRCK3/BICK3 Master", ak7738_msnbit_enum[0], get_sd3_ms,
		 set_sd3_ms),
    SOC_ENUM_EXT("LRCK4/BICK4 Master", ak7738_msnbit_enum[0], get_sd4_ms,
		 set_sd4_ms),
    SOC_ENUM_EXT("LRCK5/BICK5 Master", ak7738_msnbit_enum[0], get_sd5_ms,
		 set_sd5_ms),

    SOC_ENUM("LRCK1/BICK1 Sync Domain", ak7738_portsdsel_enum[0]),
    SOC_ENUM("LRCK2/BICK2 Sync Domain", ak7738_portsdsel_enum[1]),
    SOC_ENUM("LRCK3/BICK3 Sync Domain", ak7738_portsdsel_enum[2]),
    SOC_ENUM("LRCK4/BICK4 Sync Domain", ak7738_portsdsel_enum[3]),
    SOC_ENUM("LRCK5/BICK5 Sync Domain", ak7738_portsdsel_enum[4]),

    SOC_ENUM("DSP Master Clock", ak7738_mdiv_enum[0]),

    SOC_ENUM_EXT("BUS Master Clock", ak7738_busclock_enum[0], get_bus_clock,
		 set_bus_clock),

    SOC_ENUM_EXT("Sync Domain 1 Clock Source", ak7738_sdcks_enum[0],
		 get_sd1_cks, set_sd1_cks),
    SOC_ENUM_EXT("Sync Domain 2 Clock Source", ak7738_sdcks_enum[0],
		 get_sd2_cks, set_sd2_cks),
    SOC_ENUM_EXT("Sync Domain 3 Clock Source", ak7738_sdcks_enum[0],
		 get_sd3_cks, set_sd3_cks),
    SOC_ENUM_EXT("Sync Domain 4 Clock Source", ak7738_sdcks_enum[0],
		 get_sd4_cks, set_sd4_cks),
    SOC_ENUM_EXT("Sync Domain 5 Clock Source", ak7738_sdcks_enum[0],
		 get_sd5_cks, set_sd5_cks),
    SOC_ENUM_EXT("Sync Domain 6 Clock Source", ak7738_sdcks_enum[0],
		 get_sd6_cks, set_sd6_cks),
    SOC_ENUM_EXT("Sync Domain 7 Clock Source", ak7738_sdcks_enum[0],
		 get_sd7_cks, set_sd7_cks),

    SOC_ENUM_EXT("Sync Domain 1 fs", ak7738_sd_fs_enum[0], get_sd1_fs,
		 set_sd1_fs),
    SOC_ENUM_EXT("Sync Domain 2 fs", ak7738_sd_fs_enum[0], get_sd2_fs,
		 set_sd2_fs),
    SOC_ENUM_EXT("Sync Domain 3 fs", ak7738_sd_fs_enum[0], get_sd3_fs,
		 set_sd3_fs),
    SOC_ENUM_EXT("Sync Domain 4 fs", ak7738_sd_fs_enum[0], get_sd4_fs,
		 set_sd4_fs),
    SOC_ENUM_EXT("Sync Domain 5 fs", ak7738_sd_fs_enum[0], get_sd5_fs,
		 set_sd5_fs),
    SOC_ENUM_EXT("Sync Domain 6 fs", ak7738_sd_fs_enum[0], get_sd6_fs,
		 set_sd6_fs),
    SOC_ENUM_EXT("Sync Domain 7 fs", ak7738_sd_fs_enum[0], get_sd7_fs,
		 set_sd7_fs),

    SOC_ENUM_EXT("Sync Domain 1 BICK", ak7738_sd_fs_enum[1], get_sd1_bick,
		 set_sd1_bick),
    SOC_ENUM_EXT("Sync Domain 2 BICK", ak7738_sd_fs_enum[1], get_sd2_bick,
		 set_sd2_bick),
    SOC_ENUM_EXT("Sync Domain 3 BICK", ak7738_sd_fs_enum[1], get_sd3_bick,
		 set_sd3_bick),
    SOC_ENUM_EXT("Sync Domain 4 BICK", ak7738_sd_fs_enum[1], get_sd4_bick,
		 set_sd4_bick),
    SOC_ENUM_EXT("Sync Domain 5 BICK", ak7738_sd_fs_enum[1], get_sd5_bick,
		 set_sd5_bick),
    SOC_ENUM_EXT("Sync Domain 6 BICK", ak7738_sd_fs_enum[1], get_sd6_bick,
		 set_sd6_bick),
    SOC_ENUM_EXT("Sync Domain 7 BICK", ak7738_sd_fs_enum[1], get_sd7_bick,
		 set_sd7_bick),

    SOC_ENUM_EXT("PLL Input Clock", ak7738_pllset_enum[0], get_pllinput,
		 set_pllinput),
    SOC_ENUM_EXT("XTI Frequency", ak7738_pllset_enum[1], get_xtifs, set_xtifs),

    SOC_ENUM_EXT("SDIN2 TDMI Setting", ak7738_tdmin_enum[0], get_sdin2_tdm,
		 set_sdin2_tdm),
    SOC_ENUM_EXT("SDIN4 TDMI Setting", ak7738_tdmin_enum[0], get_sdin4_tdm,
		 set_sdin4_tdm),

    SOC_ENUM_EXT("SDIN1 Slot Length", ak7738_slotlen_enum[0], get_sdin1_slot,
		 set_sdin1_slot),
    SOC_ENUM_EXT("SDIN2 Slot Length", ak7738_slotlen_enum[0], get_sdin2_slot,
		 set_sdin2_slot),
    SOC_ENUM_EXT("SDIN3 Slot Length", ak7738_slotlen_enum[0], get_sdin3_slot,
		 set_sdin3_slot),
    SOC_ENUM_EXT("SDIN4 Slot Length", ak7738_slotlen_enum[0], get_sdin4_slot,
		 set_sdin4_slot),
    SOC_ENUM_EXT("SDIN5 Slot Length", ak7738_slotlen_enum[0], get_sdin5_slot,
		 set_sdin5_slot),

    SOC_ENUM_EXT("SDOUT1 Slot Length", ak7738_slotlen_enum[0], get_sdout1_slot,
		 set_sdout1_slot),
    SOC_ENUM_EXT("SDOUT2 Slot Length", ak7738_slotlen_enum[0], get_sdout2_slot,
		 set_sdout2_slot),
    SOC_ENUM_EXT("SDOUT3 Slot Length", ak7738_slotlen_enum[0], get_sdout3_slot,
		 set_sdout3_slot),
    SOC_ENUM_EXT("SDOUT4 Slot Length", ak7738_slotlen_enum[0], get_sdout4_slot,
		 set_sdout4_slot),
    SOC_ENUM_EXT("SDOUT5 Slot Length", ak7738_slotlen_enum[0], get_sdout5_slot,
		 set_sdout5_slot),

    SOC_ENUM("SDIN6 Slot Length", ak7738_sd6_ioformat_enum[0]),
    SOC_ENUM("SDIN6 Word Length", ak7738_sd6_ioformat_enum[1]),
    SOC_ENUM("SDOUT6 Slot Length", ak7738_sd6_ioformat_enum[2]),
    SOC_ENUM("SDOUT6 Word Length", ak7738_sd6_ioformat_enum[3]),

    SOC_ENUM("SDOUT1 Fast Mode Setting", ak7738_sdout_modeset_enum[0]),
    SOC_ENUM("SDOUT2 Fast Mode Setting", ak7738_sdout_modeset_enum[1]),
    SOC_ENUM("SDOUT3 Fast Mode Setting", ak7738_sdout_modeset_enum[2]),
    SOC_ENUM("SDOUT4 Fast Mode Setting", ak7738_sdout_modeset_enum[3]),
    SOC_ENUM("SDOUT5 Fast Mode Setting", ak7738_sdout_modeset_enum[4]),
    SOC_ENUM("SDOUT6 Fast Mode Setting", ak7738_sdout_modeset_enum[5]),

    SOC_ENUM("MixerA Input1 Data Change", ak7738_mixer_setting_enum[0]),
    SOC_ENUM("MixerA Input2 Data Change", ak7738_mixer_setting_enum[1]),
    SOC_ENUM("MixerA Input1 Level Adjust", ak7738_mixer_setting_enum[2]),
    SOC_ENUM("MixerA Input2 Level Adjust", ak7738_mixer_setting_enum[3]),

    SOC_ENUM("MixerB Input1 Data Change", ak7738_mixer_setting_enum[4]),
    SOC_ENUM("MixerB Input2 Data Change", ak7738_mixer_setting_enum[5]),
    SOC_ENUM("MixerB Input1 Level Adjust", ak7738_mixer_setting_enum[6]),
    SOC_ENUM("MixerB Input2 Level Adjust", ak7738_mixer_setting_enum[7]),

    SOC_SINGLE("Lch Mic DRC Enable", AK7738_5E_MICAMP_GAIN_CONTROL, 3, 1, 0),
    SOC_SINGLE("Rch Mic DRC Enable", AK7738_5E_MICAMP_GAIN_CONTROL, 2, 1, 0),
    SOC_SINGLE("Lch Mic ZeroCross Enable", AK7738_5E_MICAMP_GAIN_CONTROL, 1, 1,
	       0),
    SOC_SINGLE("Rch Mic ZeroCross Enable", AK7738_5E_MICAMP_GAIN_CONTROL, 0, 1,
	       0),

    SOC_ENUM("ADC Digital Filter Select", ak7738_adda_digfilsel_enum[0]),
    SOC_ENUM("ADC Digital Volume Transition Time",
	     ak7738_adda_trantime_enum[0]),
    SOC_ENUM("DAC Digital Filter Select", ak7738_adda_digfilsel_enum[1]),
    SOC_ENUM("DAC Digital Volume Transition Time",
	     ak7738_adda_trantime_enum[1]),

    SOC_SINGLE("ADC1 HPF Enable", AK7738_67_ADC_MUTE_HPF, 2, 1, 1),
    SOC_SINGLE("ADC2 HPF Enable", AK7738_67_ADC_MUTE_HPF, 1, 1, 1),
    SOC_SINGLE("ADCM HPF Enable", AK7738_67_ADC_MUTE_HPF, 0, 1, 1),

    SOC_ENUM("DAC DSM Sampling Clock Setting", ak7738_dsm_ckset_enum[0]),

    SOC_SINGLE("SRC1 Mute Enable", AK7738_70_SRC_MUTE_SETTING, 7, 1, 0),
    SOC_SINGLE("SRC2 Mute Enable", AK7738_70_SRC_MUTE_SETTING, 6, 1, 0),
    SOC_SINGLE("SRC3 Mute Enable", AK7738_70_SRC_MUTE_SETTING, 5, 1, 0),
    SOC_SINGLE("SRC4 Mute Enable", AK7738_70_SRC_MUTE_SETTING, 4, 1, 0),

    SOC_ENUM("SRC1 Mute Semi-Auto", ak7738_src_softmute_enum[0]),
    SOC_ENUM("SRC2 Mute Semi-Auto", ak7738_src_softmute_enum[1]),
    SOC_ENUM("SRC3 Mute Semi-Auto", ak7738_src_softmute_enum[2]),
    SOC_ENUM("SRC4 Mute Semi-Auto", ak7738_src_softmute_enum[3]),

    SOC_SINGLE("FSCONV1 Mute Enable", AK7738_71_FSCONV_MUTE_SETTING, 5, 1, 0),
    SOC_SINGLE("FSCONV2 Mute Enable", AK7738_71_FSCONV_MUTE_SETTING, 4, 1, 0),
    SOC_ENUM("FSCONV1 Mute Semi-Auto", ak7738_fsconv_mute_enum[0]),
    SOC_ENUM("FSCONV2 Mute Semi-Auto", ak7738_fsconv_mute_enum[1]),
    SOC_ENUM("FSCONV1 Input Data Valid Setting",
	     ak7738_fsconv_valchanel_enum[0]),
    SOC_ENUM("FSCONV2 Input Data Valid Setting",
	     ak7738_fsconv_valchanel_enum[1]),

    SOC_ENUM("SDOUT2 Pin Output Select", ak7738_outportsel_enum[0]),
    SOC_ENUM("SDOUT3 Pin Output Select", ak7738_outportsel_enum[1]),
    SOC_ENUM("SDOUT4 Pin Output Select", ak7738_outportsel_enum[2]),
    SOC_ENUM("SDOUT5 Pin Output Select", ak7738_outportsel_enum[3]),
    SOC_ENUM("SDOUT6 Pin Output Select", ak7738_outportsel_enum[4]),

    SOC_ENUM("SDIN2 Pin Input Select", ak7738_inportsel_enum[0]),
    SOC_ENUM("SDIN3 Pin Input Select", ak7738_inportsel_enum[1]),
    SOC_ENUM("SDIN5 Pin Input Select", ak7738_inportsel_enum[2]),
    SOC_ENUM("LRCK3 Pin Input Select", ak7738_inportsel_enum[3]),
    SOC_ENUM("BICK3 Pin Input Select", ak7738_inportsel_enum[4]),

    SOC_SINGLE("SRC1 FSIO Sync Enable", AK7738_6F_SRC_CLOCK_SETTING, 7, 1, 0),
    SOC_SINGLE("SRC2 FSIO Sync Enable", AK7738_6F_SRC_CLOCK_SETTING, 6, 1, 0),
    SOC_SINGLE("SRC3 FSIO Sync Enable", AK7738_6F_SRC_CLOCK_SETTING, 5, 1, 0),
    SOC_SINGLE("SRC4 FSIO Sync Enable", AK7738_7C_STO_FLAG_SETTING2, 7, 1, 0),
    SOC_ENUM("SRC Digital Filter Setting", ak7738_src_digfil_select_enum[0]),

    SOC_SINGLE("DSP1 JX0 Enable", AK7738_7A_JX_SETTING, 7, 1, 0),
    SOC_SINGLE("DSP1 JX1 Enable", AK7738_7A_JX_SETTING, 6, 1, 0),
    SOC_SINGLE("DSP1 JX2 Enable", AK7738_7A_JX_SETTING, 5, 1, 0),
    SOC_SINGLE("DSP1 JX3 Enable", AK7738_7A_JX_SETTING, 4, 1, 0),
    SOC_SINGLE("DSP2 JX0 Enable", AK7738_7A_JX_SETTING, 3, 1, 0),
    SOC_SINGLE("DSP2 JX1 Enable", AK7738_7A_JX_SETTING, 2, 1, 0),
    SOC_SINGLE("DSP2 JX2 Enable", AK7738_7A_JX_SETTING, 1, 1, 0),
    SOC_SINGLE("DSP2 JX3 Enable", AK7738_7A_JX_SETTING, 0, 1, 0),

    SOC_SINGLE("CRC Error Out Setting", AK7738_7B_STO_FLAG_SETTING1, 7, 1, 0),
    SOC_SINGLE("DSP1 WDT Error Out Setting", AK7738_7B_STO_FLAG_SETTING1, 2, 1,
	       1),
    SOC_SINGLE("DSP2 WDT Error Out Setting", AK7738_7B_STO_FLAG_SETTING1, 1, 1,
	       1),
    SOC_SINGLE("Sub DSP WDT Error Out Setting", AK7738_7B_STO_FLAG_SETTING1, 0,
	       1, 1),

    SOC_SINGLE("PLL Lock Signal Out Setting", AK7738_7C_STO_FLAG_SETTING2, 6, 1,
	       0),
    SOC_SINGLE("SRC1 Lock Signal Out Setting", AK7738_7C_STO_FLAG_SETTING2, 5,
	       1, 0),
    SOC_SINGLE("SRC2 Lock Signal Out Setting", AK7738_7C_STO_FLAG_SETTING2, 4,
	       1, 0),
    SOC_SINGLE("SRC3 Lock Signal Out Setting", AK7738_7C_STO_FLAG_SETTING2, 3,
	       1, 0),
    SOC_SINGLE("SRC4 Lock Signal Out Setting", AK7738_7C_STO_FLAG_SETTING2, 2,
	       1, 0),
    SOC_SINGLE("FSCONV1 Lock Signal Out Setting", AK7738_7C_STO_FLAG_SETTING2,
	       1, 1, 0),
    SOC_SINGLE("FSCONV2 Lock Signal Out Setting", AK7738_7C_STO_FLAG_SETTING2,
	       0, 1, 0),

    SOC_ENUM_EXT("SDOUT1 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_sd1, set_pad_drive_sd1),
    SOC_ENUM_EXT("SDOUT2 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_sd2, set_pad_drive_sd2),
    SOC_ENUM_EXT("SDOUT3 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_sd3, set_pad_drive_sd3),
    SOC_ENUM_EXT("SDOUT4 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_sd4, set_pad_drive_sd4),
    SOC_ENUM_EXT("LRCK1 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_lr1, set_pad_drive_lr1),
    SOC_ENUM_EXT("LRCK2 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_lr2, set_pad_drive_lr2),
    SOC_ENUM_EXT("LRCK3 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_lr3, set_pad_drive_lr3),
    SOC_ENUM_EXT("LRCK4 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_lr4, set_pad_drive_lr4),
    SOC_ENUM_EXT("BICK1 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_bk1, set_pad_drive_bk1),
    SOC_ENUM_EXT("BICK2 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_bk2, set_pad_drive_bk2),
    SOC_ENUM_EXT("BICK3 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_bk3, set_pad_drive_bk3),
    SOC_ENUM_EXT("BICK4 PAD Drive Mode", ak7738_pad_drive_enum[0],
		 get_pad_drive_bk4, set_pad_drive_bk4),

    SOC_ENUM_EXT("DSP Firmware PRAM", ak7738_firmware_enum[0],
		 get_DSP_write_pram, set_DSP_write_pram),
    SOC_ENUM_EXT("DSP Firmware CRAM", ak7738_firmware_enum[1],
		 get_DSP_write_cram, set_DSP_write_cram),
    SOC_ENUM_EXT("DSP Firmware OFREG", ak7738_firmware_enum[2],
		 get_DSP_write_ofreg, set_DSP_write_ofreg),

    SOC_ENUM("PRAM Memory Assignment DSP1/2", ak7738_dsp_assign_enum[0]),
    SOC_ENUM("CRAM Memory Assignment DSP1/2", ak7738_dsp_assign_enum[1]),
    SOC_ENUM("DRAM Memory Assignment DSP1/2", ak7738_dsp_assign_enum[2]),
    SOC_ENUM("DLRAM Memory Assignment DSP1/2", ak7738_dsp_assign_enum[3]),
    SOC_ENUM("DSP1 RAM Clear Setting", ak7738_dsp_assign_enum[4]),
    SOC_ENUM("DSP2 RAM Clear Setting", ak7738_dsp_assign_enum[5]),
    SOC_ENUM("Sub DSP RAM Clear Setting", ak7738_dsp_assign_enum[6]),

    SOC_ENUM("DSP1 DRAM Bank1 Addressing Mode", ak7738_dsp_addressing_enum[0]),
    SOC_ENUM("DSP1 DRAM Bank0 Addressing Mode", ak7738_dsp_addressing_enum[1]),
    SOC_ENUM("DSP2 DRAM Bank1 Addressing Mode", ak7738_dsp_addressing_enum[2]),
    SOC_ENUM("DSP2 DRAM Bank0 Addressing Mode", ak7738_dsp_addressing_enum[3]),
    SOC_ENUM("DSP1 DLRAM Bank1 Addressing Mode", ak7738_dsp_addressing_enum[4]),
    SOC_ENUM("DSP2 DLRAM Bank1 Addressing Mode", ak7738_dsp_addressing_enum[5]),

    SOC_ENUM("DSP1 DRAM Bank1 Size", ak7738_dsp_bank_size_enum[0]),
    SOC_ENUM("DSP2 DRAM Bank1 Size", ak7738_dsp_bank_size_enum[1]),
    SOC_ENUM("DSP1 DLRAM Bank1 Size", ak7738_dsp_bank_size_enum[2]),
    SOC_ENUM("DSP2 DLRAM Bank1 Size", ak7738_dsp_bank_size_enum[3]),

    SOC_ENUM("DSP1 DLRAM Bank0 Sampling Mode",
	     ak7738_dlram_sampling_mode_enum[0]),
    SOC_ENUM("DSP2 DLRAM Bank0 Sampling Mode",
	     ak7738_dlram_sampling_mode_enum[1]),

    SOC_ENUM("DSP1 DLRAM Pointer DLP0 Mode", ak7738_dlram_pointer_mode_enum[0]),
    SOC_ENUM("DSP2 DLRAM Pointer DLP0 Mode", ak7738_dlram_pointer_mode_enum[1]),

    SOC_ENUM("DSP1 FFT Wave Point", ak7738_dsp_wave_point_enum[0]),
    SOC_ENUM("DSP2 FFT Wave Point", ak7738_dsp_wave_point_enum[1]),

    SOC_SINGLE("DIT Validity Flag", AK7738_7E_DIT_STATUS_BIT1, 7, 1, 0),
    SOC_ENUM("DIT Dither Setting", ak7738_dit_statusbit_enum[0]),
    SOC_ENUM("DIT CGMS-A Setting", ak7738_dit_statusbit_enum[1]),
    SOC_SINGLE("DIT with Pre-emphasis", AK7738_7E_DIT_STATUS_BIT1, 3, 1, 0),
    SOC_SINGLE("DIT Copyright Protected Valid", AK7738_7E_DIT_STATUS_BIT1, 2, 1,
	       1),
    SOC_ENUM("DIT Data Select", ak7738_dit_statusbit_enum[2]),
    SOC_SINGLE("DIT Chanel Number Valid", AK7738_7E_DIT_STATUS_BIT1, 0, 1, 1),
    SOC_ENUM("Category Code CS[8:14]", ak7738_dit_statusbit_enum[3]),
    SOC_ENUM("Category Code CS[15]", ak7738_dit_statusbit_enum[4]),
    SOC_ENUM("DIT Clock Accuracy", ak7738_dit_statusbit_enum[5]),
    SOC_ENUM("DIT Sampling Frequency", ak7738_dit_statusbit_enum[6]),
    SOC_ENUM("DIT Original Sampling Frequency", ak7738_dit_statusbit_enum[7]),
    SOC_ENUM("DIT Data Bit Length", ak7738_dit_statusbit_enum[8]),

#ifdef AK7738_DEBUG
    SOC_ENUM_EXT("Reg Read", ak7738_test_enum[0], get_test_reg, set_test_reg),
    SOC_SINGLE("CLK Reset", AK7738_01_STSCLOCK_SETTING2, 7, 1, 0),
#endif

    SOC_ENUM_EXT("DSP2 Connect to DSP1", ak7738_dsp_connect_enum[0],
		 get_dsp2_dsp1, set_dsp2_dsp1),
    SOC_ENUM_EXT("Sub DSP Connect to DSP1", ak7738_dsp_connect_enum[0],
		 get_subdsp_dsp1, set_subdsp_dsp1),
#ifdef AK7738_X9REF_FIRMWARE
    SOC_ENUM_EXT("DSP2 SWITCH_1_SLOT1", ak7738_dsp_component_txt_enum[0],
		 get_dsp2_switch1, set_dsp2_switch1),
    SOC_SINGLE_EXT_TLV("DSP1 HFP VOL", AK7738_VIRT_112_DSP1_HFP_VOL, 0, 0xFF, 0,
		       dsp1_hfp_vol_get, dsp1_hfp_vol_set, dsp_vol_tlv),
#endif
};


static const char *
    ak7738_micbias_select_texts[] = {"LineIn", "MicBias"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak7738_micbias1_mux_enum, ak7738_micbias_select_texts);

static const struct snd_kcontrol_new ak7738_micbias1_mux_control =
	SOC_DAPM_ENUM("MicBias1 Select", ak7738_micbias1_mux_enum);

static SOC_ENUM_SINGLE_VIRT_DECL(ak7738_micbias2_mux_enum, ak7738_micbias_select_texts);

static const struct snd_kcontrol_new ak7738_micbias2_mux_control =
	SOC_DAPM_ENUM("MicBias2 Select", ak7738_micbias2_mux_enum);

static const char *ak7738_ain1l_select_texts[] =
		{"IN1P_N", "AIN1L"};

static const struct soc_enum ak7738_adc1l_mux_enum =
	SOC_ENUM_SINGLE(AK7738_66_AIN_FILTER, 3,
			ARRAY_SIZE(ak7738_ain1l_select_texts), ak7738_ain1l_select_texts);

static const struct snd_kcontrol_new ak7738_ain1l_mux_control =
	SOC_DAPM_ENUM("AIN1 Rch Select", ak7738_adc1l_mux_enum);

static const char *ak7738_ain1r_select_texts[] =
		{"IN2P_N", "AIN1R"};

static const struct soc_enum ak7738_ain1r_mux_enum =
	SOC_ENUM_SINGLE(AK7738_66_AIN_FILTER, 2,
			ARRAY_SIZE(ak7738_ain1r_select_texts), ak7738_ain1r_select_texts);

static const struct snd_kcontrol_new ak7738_ain1r_mux_control =
	SOC_DAPM_ENUM("AIN1 Rch Select", ak7738_ain1r_mux_enum);


static const char *ak7738_adc2_select_texts[] =
		{"AIN2", "AIN3", "AIN4", "AIN5"};

static const struct soc_enum ak7738_adc2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_66_AIN_FILTER, 0,
			ARRAY_SIZE(ak7738_adc2_select_texts), ak7738_adc2_select_texts);

static const struct snd_kcontrol_new ak7738_adc2_mux_control =
	SOC_DAPM_ENUM("ADC2 Select", ak7738_adc2_mux_enum);


static const char *ak7738_adcm_select_texts[] =
		{"AINMP_N", "AINM"};

static const struct soc_enum ak7738_adcm_mux_enum =
	SOC_ENUM_SINGLE(AK7738_66_AIN_FILTER, 4,
			ARRAY_SIZE(ak7738_adcm_select_texts), ak7738_adcm_select_texts);

static const struct snd_kcontrol_new ak7738_adcm_mux_control =
	SOC_DAPM_ENUM("ADCM Select", ak7738_adcm_mux_enum);

static const char *ak7738_source_select_texts[] = {
	"ZERO",    "SDIN1",    "SDIN2",   "SDIN2B",  "SDIN2C",
	"SDIN2D",  "SDIN3",    "SDIN4",   "SDIN4B",  "SDIN4C",
	"SDIN4D",  "SDIN5",    "SDIN6",   "DSP1O1",  "DSP1O2",
	"DSP1O3",  "DSP1O4",   "DSP1O5",  "DSP1O6",  "DSP2O1",
	"DSP2O2",  "DSP2O3",   "DSP2O4",  "DSP2O5",  "DSP2O6",
	"ADC1",    "ADC2",     "ADCM",    "MixerA",  "MixerB",
	"SRCO1",   "SRCO2",    "SRCO3",   "SRCO4",    "FSCO1",
	"FSCO2"
};

static const struct soc_enum ak7738_sout1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_23_SDOUT1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_sout1_mux_control =
	SOC_DAPM_ENUM("SOUT1 Select", ak7738_sout1_mux_enum);

static const struct soc_enum ak7738_sout2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_24_SDOUT2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_sout2_mux_control =
	SOC_DAPM_ENUM("SOUT2 Select", ak7738_sout2_mux_enum);

static const struct soc_enum ak7738_sout3_mux_enum =
	SOC_ENUM_SINGLE(AK7738_25_SDOUT3_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_sout3_mux_control =
	SOC_DAPM_ENUM("SOUT3 Select", ak7738_sout3_mux_enum);

static const struct soc_enum ak7738_sout4_mux_enum =
	SOC_ENUM_SINGLE(AK7738_26_SDOUT4_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_sout4_mux_control =
	SOC_DAPM_ENUM("SOUT4 Select", ak7738_sout4_mux_enum);

static const struct soc_enum ak7738_sout5_mux_enum =
	SOC_ENUM_SINGLE(AK7738_27_SDOUT5_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_sout5_mux_control =
	SOC_DAPM_ENUM("SOUT5 Select", ak7738_sout5_mux_enum);

static const struct soc_enum ak7738_sout6_mux_enum =
	SOC_ENUM_SINGLE(AK7738_28_SDOUT6_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_sout6_mux_control =
	SOC_DAPM_ENUM("SOUT6 Select", ak7738_sout6_mux_enum);

static const struct soc_enum ak7738_dit_mux_enum =
	SOC_ENUM_SINGLE(AK7738_41_DIT_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dit_mux_control =
	SOC_DAPM_ENUM("DIT Select", ak7738_dit_mux_enum);

static const struct soc_enum ak7738_dac1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_29_DAC1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dac1_mux_control =
	SOC_DAPM_ENUM("DAC1 Select", ak7738_dac1_mux_enum);

static const struct soc_enum ak7738_dac2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_2A_DAC2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dac2_mux_control =
	SOC_DAPM_ENUM("DAC2 Select", ak7738_dac2_mux_enum);

static const struct soc_enum ak7738_dsp1in1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_2B_DSP1_IN1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp1in1_mux_control =
	SOC_DAPM_ENUM("DSP1 IN1 Select", ak7738_dsp1in1_mux_enum);

static const struct soc_enum ak7738_dsp1in2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_2C_DSP1_IN2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp1in2_mux_control =
	SOC_DAPM_ENUM("DSP1 IN2 Select", ak7738_dsp1in2_mux_enum);

static const struct soc_enum ak7738_dsp1in3_mux_enum =
	SOC_ENUM_SINGLE(AK7738_2D_DSP1_IN3_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp1in3_mux_control =
	SOC_DAPM_ENUM("DSP1 IN3 Select", ak7738_dsp1in3_mux_enum);

static const struct soc_enum ak7738_dsp1in4_mux_enum =
	SOC_ENUM_SINGLE(AK7738_2E_DSP1_IN4_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp1in4_mux_control =
	SOC_DAPM_ENUM("DSP1 IN4 Select", ak7738_dsp1in4_mux_enum);

static const struct soc_enum ak7738_dsp1in5_mux_enum =
	SOC_ENUM_SINGLE(AK7738_2F_DSP1_IN5_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp1in5_mux_control =
	SOC_DAPM_ENUM("DSP1 IN5 Select", ak7738_dsp1in5_mux_enum);

static const struct soc_enum ak7738_dsp1in6_mux_enum =
	SOC_ENUM_SINGLE(AK7738_30_DSP1_IN6_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp1in6_mux_control =
	SOC_DAPM_ENUM("DSP1 IN6 Select", ak7738_dsp1in6_mux_enum);

static const struct soc_enum ak7738_dsp2in1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_31_DSP2_IN1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp2in1_mux_control =
	SOC_DAPM_ENUM("DSP2 IN1 Select", ak7738_dsp2in1_mux_enum);

static const struct soc_enum ak7738_dsp2in2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_32_DSP2_IN2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp2in2_mux_control =
	SOC_DAPM_ENUM("DSP2 IN2 Select", ak7738_dsp2in2_mux_enum);

static const struct soc_enum ak7738_dsp2in3_mux_enum =
	SOC_ENUM_SINGLE(AK7738_33_DSP2_IN3_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp2in3_mux_control =
	SOC_DAPM_ENUM("DSP2 IN3 Select", ak7738_dsp2in3_mux_enum);

static const struct soc_enum ak7738_dsp2in4_mux_enum =
	SOC_ENUM_SINGLE(AK7738_34_DSP2_IN4_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp2in4_mux_control =
	SOC_DAPM_ENUM("DSP2 IN4 Select", ak7738_dsp2in4_mux_enum);

static const struct soc_enum ak7738_dsp2in5_mux_enum =
	SOC_ENUM_SINGLE(AK7738_35_DSP2_IN5_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp2in5_mux_control =
	SOC_DAPM_ENUM("DSP2 IN5 Select", ak7738_dsp2in5_mux_enum);

static const struct soc_enum ak7738_dsp2in6_mux_enum =
	SOC_ENUM_SINGLE(AK7738_36_DSP2_IN6_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_dsp2in6_mux_control =
	SOC_DAPM_ENUM("DSP2 IN6 Select", ak7738_dsp2in6_mux_enum);

static const struct soc_enum ak7738_src1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_37_SRC1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_src1_mux_control =
	SOC_DAPM_ENUM("SRC1 Select", ak7738_src1_mux_enum);

static const struct soc_enum ak7738_src2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_38_SRC2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_src2_mux_control =
	SOC_DAPM_ENUM("SRC2 Select", ak7738_src2_mux_enum);

static const struct soc_enum ak7738_src3_mux_enum =
	SOC_ENUM_SINGLE(AK7738_39_SRC3_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_src3_mux_control =
	SOC_DAPM_ENUM("SRC3 Select", ak7738_src3_mux_enum);

static const struct soc_enum ak7738_src4_mux_enum =
	SOC_ENUM_SINGLE(AK7738_3A_SRC4_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_src4_mux_control =
	SOC_DAPM_ENUM("SRC4 Select", ak7738_src4_mux_enum);

static const struct soc_enum ak7738_fsconv1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_3B_FSCONV1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_fsconv1_mux_control =
	SOC_DAPM_ENUM("FSCONV1 Select", ak7738_fsconv1_mux_enum);

static const struct soc_enum ak7738_fsconv2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_3C_FSCONV2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_fsconv2_mux_control =
	SOC_DAPM_ENUM("FSCONV2 Select", ak7738_fsconv2_mux_enum);

static const struct soc_enum ak7738_mixerach1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_3D_MIXERA_CH1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_mixerach1_mux_control =
	SOC_DAPM_ENUM("MixerA Ch1 Select", ak7738_mixerach1_mux_enum);

static const struct soc_enum ak7738_mixerach2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_3E_MIXERA_CH2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_mixerach2_mux_control =
	SOC_DAPM_ENUM("MixerA Ch2 Select", ak7738_mixerach2_mux_enum);

static const struct soc_enum ak7738_mixerbch1_mux_enum =
	SOC_ENUM_SINGLE(AK7738_3F_MIXERB_CH1_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_mixerbch1_mux_control =
	SOC_DAPM_ENUM("MixerB Ch1 Select", ak7738_mixerbch1_mux_enum);

static const struct soc_enum ak7738_mixerbch2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_40_MIXERB_CH2_DATA_SELECT, 0,
			ARRAY_SIZE(ak7738_source_select_texts), ak7738_source_select_texts);

static const struct snd_kcontrol_new ak7738_mixerbch2_mux_control =
	SOC_DAPM_ENUM("MixerB Ch2 Select", ak7738_mixerbch2_mux_enum);


static const char *ak7738_tdmo_modeset_texts[] = {
	"Normal",
	"TDM128_1",
	"TDM128_2",
	"TDM256",
};


static const struct soc_enum ak7738_tdmout2_mux_enum =
	SOC_ENUM_SINGLE(AK7738_55_TDM_MODE_SETTING, 6,
			ARRAY_SIZE(ak7738_tdmo_modeset_texts), ak7738_tdmo_modeset_texts);

static const struct snd_kcontrol_new ak7738_tdmout2_mux_control =
	SOC_DAPM_ENUM("SDOUT2 TDMO Setting", ak7738_tdmout2_mux_enum);

static const struct soc_enum ak7738_tdmout3_mux_enum =
	SOC_ENUM_SINGLE(AK7738_55_TDM_MODE_SETTING, 4,
			ARRAY_SIZE(ak7738_tdmo_modeset_texts), ak7738_tdmo_modeset_texts);

static const struct snd_kcontrol_new ak7738_tdmout3_mux_control =
	SOC_DAPM_ENUM("SDOUT3 TDMO Setting", ak7738_tdmout3_mux_enum);

static const struct soc_enum ak7738_tdmout4_mux_enum =
	SOC_ENUM_SINGLE(AK7738_55_TDM_MODE_SETTING, 2,
			ARRAY_SIZE(ak7738_tdmo_modeset_texts)-1, ak7738_tdmo_modeset_texts);

static const struct snd_kcontrol_new ak7738_tdmout4_mux_control =
	SOC_DAPM_ENUM("SDOUT4 TDMO Setting", ak7738_tdmout4_mux_enum);

static const struct soc_enum ak7738_tdmout5_mux_enum =
	SOC_ENUM_SINGLE(AK7738_55_TDM_MODE_SETTING, 0,
			ARRAY_SIZE(ak7738_tdmo_modeset_texts)-1, ak7738_tdmo_modeset_texts);

static const struct snd_kcontrol_new ak7738_tdmout5_mux_control =
	SOC_DAPM_ENUM("SDOUT5 TDMO Setting", ak7738_tdmout5_mux_enum);


static int ak7738_ClockReset(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);  // 4.4.XX

	switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			snd_soc_update_bits(codec, AK7738_01_STSCLOCK_SETTING2, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x01, 0x01);         // HRESETN bit = 1
			break;

		case SND_SOC_DAPM_POST_PMD:
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x01, 0x0);         // HRESETN bit = 0
			snd_soc_update_bits(codec, AK7738_01_STSCLOCK_SETTING2, 0x80, 0x0);  // CKRESETN bit = 0
			break;
	}
	return 0;
}

static const struct snd_kcontrol_new dsp1out1_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP1 IN1", AK7738_VIRT_106_DSP1OUT1_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN2", AK7738_VIRT_106_DSP1OUT1_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN3", AK7738_VIRT_106_DSP1OUT1_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN4", AK7738_VIRT_106_DSP1OUT1_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN5", AK7738_VIRT_106_DSP1OUT1_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN6", AK7738_VIRT_106_DSP1OUT1_MIX, 5, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_106_DSP1OUT1_MIX, 6, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_106_DSP1OUT1_MIX, 7, 1, 0),
};

static const struct snd_kcontrol_new dsp1out2_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP1 IN1", AK7738_VIRT_107_DSP1OUT2_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN2", AK7738_VIRT_107_DSP1OUT2_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN3", AK7738_VIRT_107_DSP1OUT2_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN4", AK7738_VIRT_107_DSP1OUT2_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN5", AK7738_VIRT_107_DSP1OUT2_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN6", AK7738_VIRT_107_DSP1OUT2_MIX, 5, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_107_DSP1OUT2_MIX, 6, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_107_DSP1OUT2_MIX, 7, 1, 0),
/*  Next controller need expand register bits
	SOC_DAPM_SINGLE("DSP2 IN3", AK7738_VIRT_107_DSP1OUT2_MIX, 8, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN4", AK7738_VIRT_107_DSP1OUT2_MIX, 9, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN5", AK7738_VIRT_107_DSP1OUT2_MIX, 10, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN6", AK7738_VIRT_107_DSP1OUT2_MIX, 11, 1, 0), */
};

static const struct snd_kcontrol_new dsp1out3_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP1 IN1", AK7738_VIRT_108_DSP1OUT3_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN2", AK7738_VIRT_108_DSP1OUT3_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN3", AK7738_VIRT_108_DSP1OUT3_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN4", AK7738_VIRT_108_DSP1OUT3_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN5", AK7738_VIRT_108_DSP1OUT3_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN6", AK7738_VIRT_108_DSP1OUT3_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp1out4_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP1 IN1", AK7738_VIRT_109_DSP1OUT4_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN2", AK7738_VIRT_109_DSP1OUT4_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN3", AK7738_VIRT_109_DSP1OUT4_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN4", AK7738_VIRT_109_DSP1OUT4_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN5", AK7738_VIRT_109_DSP1OUT4_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN6", AK7738_VIRT_109_DSP1OUT4_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp1out5_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP1 IN1", AK7738_VIRT_10A_DSP1OUT5_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN2", AK7738_VIRT_10A_DSP1OUT5_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN3", AK7738_VIRT_10A_DSP1OUT5_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN4", AK7738_VIRT_10A_DSP1OUT5_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN5", AK7738_VIRT_10A_DSP1OUT5_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN6", AK7738_VIRT_10A_DSP1OUT5_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp1out6_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP1 IN1", AK7738_VIRT_10B_DSP1OUT6_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN2", AK7738_VIRT_10B_DSP1OUT6_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN3", AK7738_VIRT_10B_DSP1OUT6_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN4", AK7738_VIRT_10B_DSP1OUT6_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN5", AK7738_VIRT_10B_DSP1OUT6_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP1 IN6", AK7738_VIRT_10B_DSP1OUT6_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp2out1_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_10C_DSP2OUT1_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_10C_DSP2OUT1_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN3", AK7738_VIRT_10C_DSP2OUT1_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN4", AK7738_VIRT_10C_DSP2OUT1_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN5", AK7738_VIRT_10C_DSP2OUT1_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN6", AK7738_VIRT_10C_DSP2OUT1_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp2out2_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_10D_DSP2OUT2_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_10D_DSP2OUT2_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN3", AK7738_VIRT_10D_DSP2OUT2_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN4", AK7738_VIRT_10D_DSP2OUT2_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN5", AK7738_VIRT_10D_DSP2OUT2_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN6", AK7738_VIRT_10D_DSP2OUT2_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp2out3_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_10E_DSP2OUT3_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_10E_DSP2OUT3_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN3", AK7738_VIRT_10E_DSP2OUT3_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN4", AK7738_VIRT_10E_DSP2OUT3_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN5", AK7738_VIRT_10E_DSP2OUT3_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN6", AK7738_VIRT_10E_DSP2OUT3_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp2out4_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_10F_DSP2OUT4_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_10F_DSP2OUT4_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN3", AK7738_VIRT_10F_DSP2OUT4_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN4", AK7738_VIRT_10F_DSP2OUT4_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN5", AK7738_VIRT_10F_DSP2OUT4_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN6", AK7738_VIRT_10F_DSP2OUT4_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp2out5_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_110_DSP2OUT5_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_110_DSP2OUT5_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN3", AK7738_VIRT_110_DSP2OUT5_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN4", AK7738_VIRT_110_DSP2OUT5_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN5", AK7738_VIRT_110_DSP2OUT5_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN6", AK7738_VIRT_110_DSP2OUT5_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dsp2out6_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP2 IN1", AK7738_VIRT_111_DSP2OUT6_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN2", AK7738_VIRT_111_DSP2OUT6_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN3", AK7738_VIRT_111_DSP2OUT6_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN4", AK7738_VIRT_111_DSP2OUT6_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN5", AK7738_VIRT_111_DSP2OUT6_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSP2 IN6", AK7738_VIRT_111_DSP2OUT6_MIX, 5, 1, 0),
};

static int ak7738_DSP1PowerUp(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);  // 4.4.XX
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int value, nMask;

	switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			value = 0;
			if ( ak7738->nDSP2 & 1 ) {
				value |= 0x4;
			}
			if ( ak7738->nSubDSP == 1 ) {
				value |= 0x2;
			}
			if ( value != 0 ) {
				snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x06, value);
			}
			break;

		case SND_SOC_DAPM_POST_PMD:
			if ( ak7738->nDSP2 & 2 ) {
				nMask = 0x2;
			}
			else {
				nMask = 0x6;
			}
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, nMask, 0x00);
			break;
	}
	return 0;
}

static int ak7738_DSP2PowerUp(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);  // 4.4.XX
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);


	switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			ak7738->nDSP2 |= 2;
			break;

		case SND_SOC_DAPM_POST_PMD:
			ak7738->nDSP2 &= 2;
			break;
	}
	return 0;
}

/* ak7738 dapm widgets */
static const struct snd_soc_dapm_widget ak7738_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("Clock Power", 1, SND_SOC_NOPM, 0, 0, ak7738_ClockReset,
                                   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CODEC Power", 2, AK7738_85_RESET_CTRL, 4, 0, NULL, 0),

// CODEC
	SND_SOC_DAPM_ADC("ADC1", NULL, AK7738_83_POWER_MANAGEMENT1, 5, 0),
	SND_SOC_DAPM_ADC("ADC2", NULL, AK7738_83_POWER_MANAGEMENT1, 4, 0),
	SND_SOC_DAPM_ADC("ADCM", NULL, AK7738_83_POWER_MANAGEMENT1, 3, 0),
	SND_SOC_DAPM_DAC("DAC1", NULL, AK7738_83_POWER_MANAGEMENT1, 2, 0),
	SND_SOC_DAPM_DAC("DAC2", NULL, AK7738_83_POWER_MANAGEMENT1, 1, 0),

// SRC
	SND_SOC_DAPM_PGA("SRC1", AK7738_84_POWER_MANAGEMENT2, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SRC2", AK7738_84_POWER_MANAGEMENT2, 6, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SRC3", AK7738_84_POWER_MANAGEMENT2, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SRC4", AK7738_84_POWER_MANAGEMENT2, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("FSCONV1", AK7738_84_POWER_MANAGEMENT2, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("FSCONV2", AK7738_84_POWER_MANAGEMENT2, 2, 0, NULL, 0),


// DSP
	SND_SOC_DAPM_SUPPLY_S("DSP1", 3, AK7738_85_RESET_CTRL, 3, 0, ak7738_DSP1PowerUp, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DSP2", 4, AK7738_85_RESET_CTRL, 2, 0, ak7738_DSP2PowerUp, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA("DSP1O1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP1O2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP1O3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP1O4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP1O5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP1O6", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA("DSP2O1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP2O2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP2O3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP2O4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP2O5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP2O6", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("DSP1 OUT1 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out1_mixer_kctrl[0], ARRAY_SIZE(dsp1out1_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP1 OUT2 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out2_mixer_kctrl[0], ARRAY_SIZE(dsp1out2_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP1 OUT3 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out3_mixer_kctrl[0], ARRAY_SIZE(dsp1out3_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP1 OUT4 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out4_mixer_kctrl[0], ARRAY_SIZE(dsp1out4_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP1 OUT5 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out5_mixer_kctrl[0], ARRAY_SIZE(dsp1out5_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP1 OUT6 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out6_mixer_kctrl[0], ARRAY_SIZE(dsp1out6_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP2 OUT1 Mixer", SND_SOC_NOPM, 0, 0, &dsp2out1_mixer_kctrl[0], ARRAY_SIZE(dsp2out1_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP2 OUT2 Mixer", SND_SOC_NOPM, 0, 0, &dsp2out2_mixer_kctrl[0], ARRAY_SIZE(dsp2out2_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP2 OUT3 Mixer", SND_SOC_NOPM, 0, 0, &dsp2out3_mixer_kctrl[0], ARRAY_SIZE(dsp2out3_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP2 OUT4 Mixer", SND_SOC_NOPM, 0, 0, &dsp2out4_mixer_kctrl[0], ARRAY_SIZE(dsp2out4_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP2 OUT5 Mixer", SND_SOC_NOPM, 0, 0, &dsp2out5_mixer_kctrl[0], ARRAY_SIZE(dsp2out5_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP2 OUT6 Mixer", SND_SOC_NOPM, 0, 0, &dsp2out6_mixer_kctrl[0], ARRAY_SIZE(dsp2out6_mixer_kctrl)),


// Mixer
	SND_SOC_DAPM_PGA("MixerA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MixerB", SND_SOC_NOPM, 0, 0, NULL, 0),

// Digital Input/Output
	SND_SOC_DAPM_AIF_IN("SDIN1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2B", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2C", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2D", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN3", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN4", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN4B", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN4C", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN4D", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN5", "AIF5 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN6_AIF1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN6_AIF2", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN6_AIF3", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN6_AIF4", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN6_AIF5", "AIF5 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("SDOUT1", "AIF1 Capture", 0, AK7738_58_OUTPUT_PORT_ENABLE, 5, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT2", "AIF2 Capture", 0, AK7738_58_OUTPUT_PORT_ENABLE, 4, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT3", "AIF3 Capture", 0, AK7738_58_OUTPUT_PORT_ENABLE, 3, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT4", "AIF4 Capture", 0, AK7738_58_OUTPUT_PORT_ENABLE, 2, 1),
	SND_SOC_DAPM_AIF_OUT("SDOUT5", "AIF5 Capture", 0, AK7738_58_OUTPUT_PORT_ENABLE, 1, 0),



	SND_SOC_DAPM_PGA("SDIN6", SND_SOC_NOPM, 0, 0, NULL, 0),

// MIC Bias
	SND_SOC_DAPM_MICBIAS("MicBias1", AK7738_02_MICBIAS_PLOWER, 1, 0),
	SND_SOC_DAPM_MUX("MicBias1 MUX", SND_SOC_NOPM, 0, 0, &ak7738_micbias1_mux_control),

	SND_SOC_DAPM_MICBIAS("MicBias2", AK7738_02_MICBIAS_PLOWER, 0, 0),
	SND_SOC_DAPM_MUX("MicBias2 MUX", SND_SOC_NOPM, 0, 0, &ak7738_micbias2_mux_control),

// Analog Input
	SND_SOC_DAPM_INPUT("IN1P_N"),
	SND_SOC_DAPM_INPUT("AIN1L"),
	SND_SOC_DAPM_INPUT("IN2P_N"),
	SND_SOC_DAPM_INPUT("AIN1R"),

	SND_SOC_DAPM_INPUT("AIN2"),
	SND_SOC_DAPM_INPUT("AIN3"),
	SND_SOC_DAPM_INPUT("AIN4"),
	SND_SOC_DAPM_INPUT("AIN5"),

	SND_SOC_DAPM_INPUT("AINMP_N"),
	SND_SOC_DAPM_INPUT("AINM"),

	SND_SOC_DAPM_MUX("AIN1 Lch MUX", SND_SOC_NOPM, 0, 0, &ak7738_ain1l_mux_control),
	SND_SOC_DAPM_MUX("AIN1 Rch MUX", SND_SOC_NOPM, 0, 0, &ak7738_ain1r_mux_control),

	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0, &ak7738_adc2_mux_control),

	SND_SOC_DAPM_MUX("ADCM MUX", SND_SOC_NOPM, 0, 0, &ak7738_adcm_mux_control),


// Analog Output
	SND_SOC_DAPM_OUTPUT("AOUT1"),
	SND_SOC_DAPM_OUTPUT("AOUT2"),

// Source Selector
	SND_SOC_DAPM_MUX("SDOUT1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_sout1_mux_control),
	SND_SOC_DAPM_MUX("SDOUT2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_sout2_mux_control),
	SND_SOC_DAPM_MUX("SDOUT3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_sout3_mux_control),
	SND_SOC_DAPM_MUX("SDOUT4 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_sout4_mux_control),
	SND_SOC_DAPM_MUX("SDOUT5 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_sout5_mux_control),

#ifdef AK7738_DIT_ENABLE

	SND_SOC_DAPM_OUTPUT("DIT OUT"),

	SND_SOC_DAPM_PGA("DIT", AK7738_58_OUTPUT_PORT_ENABLE, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("DIT Source Selector", AK7738_84_POWER_MANAGEMENT2, 1, 0, &ak7738_dit_mux_control),

#else

	SND_SOC_DAPM_AIF_OUT("SDOUT6_AIF1", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT6_AIF2", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT6_AIF3", "AIF3 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT6_AIF4", "AIF4 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT6_AIF5", "AIF5 Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA("SDOUT6", AK7738_58_OUTPUT_PORT_ENABLE, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("SDOUT6 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_sout6_mux_control),

#endif

	SND_SOC_DAPM_MUX("DAC1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dac1_mux_control),
	SND_SOC_DAPM_MUX("DAC2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dac2_mux_control),

	SND_SOC_DAPM_MUX("DSP1 IN1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp1in1_mux_control),
	SND_SOC_DAPM_MUX("DSP1 IN2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp1in2_mux_control),
	SND_SOC_DAPM_MUX("DSP1 IN3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp1in3_mux_control),
	SND_SOC_DAPM_MUX("DSP1 IN4 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp1in4_mux_control),
	SND_SOC_DAPM_MUX("DSP1 IN5 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp1in5_mux_control),
	SND_SOC_DAPM_MUX("DSP1 IN6 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp1in6_mux_control),

	SND_SOC_DAPM_MUX("DSP2 IN1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp2in1_mux_control),
	SND_SOC_DAPM_MUX("DSP2 IN2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp2in2_mux_control),
	SND_SOC_DAPM_MUX("DSP2 IN3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp2in3_mux_control),
	SND_SOC_DAPM_MUX("DSP2 IN4 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp2in4_mux_control),
	SND_SOC_DAPM_MUX("DSP2 IN5 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp2in5_mux_control),
	SND_SOC_DAPM_MUX("DSP2 IN6 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_dsp2in6_mux_control),

	SND_SOC_DAPM_MUX("SRC1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_src1_mux_control),
	SND_SOC_DAPM_MUX("SRC2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_src2_mux_control),
	SND_SOC_DAPM_MUX("SRC3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_src3_mux_control),
	SND_SOC_DAPM_MUX("SRC4 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_src4_mux_control),

	SND_SOC_DAPM_MUX("FSCONV1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_fsconv1_mux_control),
	SND_SOC_DAPM_MUX("FSCONV2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_fsconv2_mux_control),

	SND_SOC_DAPM_MUX("MixerA Ch1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_mixerach1_mux_control),
	SND_SOC_DAPM_MUX("MixerA Ch2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_mixerach2_mux_control),
	SND_SOC_DAPM_MUX("MixerB Ch1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_mixerbch1_mux_control),
	SND_SOC_DAPM_MUX("MixerB Ch2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7738_mixerbch2_mux_control),


	SND_SOC_DAPM_MUX("SDOUT2 TDM Selector", SND_SOC_NOPM, 0, 0, &ak7738_tdmout2_mux_control),
	SND_SOC_DAPM_PGA("SDOUT2 Normal", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT2 TDM128_1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT2 TDM128_2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT2 TDM256", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("SDOUT3 TDM Selector", SND_SOC_NOPM, 0, 0, &ak7738_tdmout3_mux_control),
	SND_SOC_DAPM_PGA("SDOUT3 Normal", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT3 TDM128_1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT3 TDM128_2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT3 TDM256", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("SDOUT4 TDM Selector", SND_SOC_NOPM, 0, 0, &ak7738_tdmout4_mux_control),
	SND_SOC_DAPM_PGA("SDOUT4 Normal", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT4 TDM128_1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT4 TDM128_2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("SDOUT5 TDM Selector", SND_SOC_NOPM, 0, 0, &ak7738_tdmout5_mux_control),
	SND_SOC_DAPM_PGA("SDOUT5 Normal", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT5 TDM128_1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SDOUT5 TDM128_2", SND_SOC_NOPM, 0, 0, NULL, 0),

};

static const struct snd_soc_dapm_route ak7738_intercon[] = {

	{"CODEC Power", NULL, "Clock Power"},
	{"SDOUT1", NULL, "Clock Power"},
	{"SDOUT2", NULL, "Clock Power"},
	{"SDOUT3", NULL, "Clock Power"},
	{"SDOUT4", NULL, "Clock Power"},
	{"SDOUT5", NULL, "Clock Power"},

	{"ADC1", NULL, "CODEC Power"},
	{"ADC2", NULL, "CODEC Power"},
	{"ADCM", NULL, "CODEC Power"},
	{"DAC1", NULL, "CODEC Power"},
	{"DAC2", NULL, "CODEC Power"},

	{"AIN1 Lch MUX", "IN1P_N", "IN1P_N"},
	{"AIN1 Lch MUX", "AIN1L", "AIN1L"},
	{"MicBias1", NULL, "AIN1 Lch MUX"},
	{"MicBias1 MUX", "LineIn", "AIN1 Lch MUX"},
	{"MicBias1 MUX", "MicBias", "MicBias1"},

	{"AIN1 Rch MUX", "IN2P_N", "IN2P_N"},
	{"AIN1 Rch MUX", "AIN1R", "AIN1R"},
	{"MicBias2", NULL, "AIN1 Rch MUX"},
	{"MicBias2 MUX", "LineIn", "AIN1 Rch MUX"},
	{"MicBias2 MUX", "MicBias", "MicBias2"},

	{"ADC1", NULL, "MicBias1 MUX"},
	{"ADC1", NULL, "MicBias2 MUX"},

	{"ADC2 MUX", "AIN2", "AIN2"},
	{"ADC2 MUX", "AIN3", "AIN3"},
	{"ADC2 MUX", "AIN4", "AIN4"},
	{"ADC2 MUX", "AIN5", "AIN5"},
	{"ADC2", NULL, "ADC2 MUX"},

	{"ADCM MUX", "AINMP_N", "AINMP_N"},
	{"ADCM MUX", "AINM", "AINM"},
	{"ADCM", NULL, "ADCM MUX"},

	{"SDOUT1", NULL, "SDOUT1 Source Selector"},

	{"DAC1", NULL, "DAC1 Source Selector"},
	{"DAC2", NULL, "DAC2 Source Selector"},

	{"AOUT1", NULL, "DAC1"},
	{"AOUT2", NULL, "DAC2"},

	{"DSP1O1", NULL, "DSP1"},
	{"DSP1O2", NULL, "DSP1"},
	{"DSP1O3", NULL, "DSP1"},
	{"DSP1O4", NULL, "DSP1"},
	{"DSP1O5", NULL, "DSP1"},
	{"DSP1O6", NULL, "DSP1"},

	{"DSP2O1", NULL, "DSP2"},
	{"DSP2O2", NULL, "DSP2"},
	{"DSP2O3", NULL, "DSP2"},
	{"DSP2O4", NULL, "DSP2"},
	{"DSP2O5", NULL, "DSP2"},
	{"DSP2O6", NULL, "DSP2"},

	{"DSP1O1", NULL, "DSP1 OUT1 Mixer"},
	{"DSP1O2", NULL, "DSP1 OUT2 Mixer"},
	{"DSP1O3", NULL, "DSP1 OUT3 Mixer"},
	{"DSP1O4", NULL, "DSP1 OUT4 Mixer"},
	{"DSP1O5", NULL, "DSP1 OUT5 Mixer"},
	{"DSP1O6", NULL, "DSP1 OUT6 Mixer"},

	{"DSP2O1", NULL, "DSP2 OUT1 Mixer"},
	{"DSP2O2", NULL, "DSP2 OUT2 Mixer"},
	{"DSP2O3", NULL, "DSP2 OUT3 Mixer"},
	{"DSP2O4", NULL, "DSP2 OUT4 Mixer"},
	{"DSP2O5", NULL, "DSP2 OUT5 Mixer"},
	{"DSP2O6", NULL, "DSP2 OUT6 Mixer"},

	{"DSP1 OUT1 Mixer", "DSP1 IN1", "DSP1 IN1 Source Selector"},
	{"DSP1 OUT1 Mixer", "DSP1 IN2", "DSP1 IN2 Source Selector"},
	{"DSP1 OUT1 Mixer", "DSP1 IN3", "DSP1 IN3 Source Selector"},
	{"DSP1 OUT1 Mixer", "DSP1 IN4", "DSP1 IN4 Source Selector"},
	{"DSP1 OUT1 Mixer", "DSP1 IN5", "DSP1 IN5 Source Selector"},
	{"DSP1 OUT1 Mixer", "DSP1 IN6", "DSP1 IN6 Source Selector"},
	{"DSP1 OUT1 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP1 OUT1 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},

	{"DSP1 OUT2 Mixer", "DSP1 IN1", "DSP1 IN1 Source Selector"},
	{"DSP1 OUT2 Mixer", "DSP1 IN2", "DSP1 IN2 Source Selector"},
	{"DSP1 OUT2 Mixer", "DSP1 IN3", "DSP1 IN3 Source Selector"},
	{"DSP1 OUT2 Mixer", "DSP1 IN4", "DSP1 IN4 Source Selector"},
	{"DSP1 OUT2 Mixer", "DSP1 IN5", "DSP1 IN5 Source Selector"},
	{"DSP1 OUT2 Mixer", "DSP1 IN6", "DSP1 IN6 Source Selector"},
	{"DSP1 OUT2 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP1 OUT2 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},

	{"DSP1 OUT3 Mixer", "DSP1 IN1", "DSP1 IN1 Source Selector"},
	{"DSP1 OUT3 Mixer", "DSP1 IN2", "DSP1 IN2 Source Selector"},
	{"DSP1 OUT3 Mixer", "DSP1 IN3", "DSP1 IN3 Source Selector"},
	{"DSP1 OUT3 Mixer", "DSP1 IN4", "DSP1 IN4 Source Selector"},
	{"DSP1 OUT3 Mixer", "DSP1 IN5", "DSP1 IN5 Source Selector"},
	{"DSP1 OUT3 Mixer", "DSP1 IN6", "DSP1 IN6 Source Selector"},

	{"DSP1 OUT4 Mixer", "DSP1 IN1", "DSP1 IN1 Source Selector"},
	{"DSP1 OUT4 Mixer", "DSP1 IN2", "DSP1 IN2 Source Selector"},
	{"DSP1 OUT4 Mixer", "DSP1 IN3", "DSP1 IN3 Source Selector"},
	{"DSP1 OUT4 Mixer", "DSP1 IN4", "DSP1 IN4 Source Selector"},
	{"DSP1 OUT4 Mixer", "DSP1 IN5", "DSP1 IN5 Source Selector"},
	{"DSP1 OUT4 Mixer", "DSP1 IN6", "DSP1 IN6 Source Selector"},

	{"DSP1 OUT5 Mixer", "DSP1 IN1", "DSP1 IN1 Source Selector"},
	{"DSP1 OUT5 Mixer", "DSP1 IN2", "DSP1 IN2 Source Selector"},
	{"DSP1 OUT5 Mixer", "DSP1 IN3", "DSP1 IN3 Source Selector"},
	{"DSP1 OUT5 Mixer", "DSP1 IN4", "DSP1 IN4 Source Selector"},
	{"DSP1 OUT5 Mixer", "DSP1 IN5", "DSP1 IN5 Source Selector"},
	{"DSP1 OUT5 Mixer", "DSP1 IN6", "DSP1 IN6 Source Selector"},

	{"DSP1 OUT6 Mixer", "DSP1 IN1", "DSP1 IN1 Source Selector"},
	{"DSP1 OUT6 Mixer", "DSP1 IN2", "DSP1 IN2 Source Selector"},
	{"DSP1 OUT6 Mixer", "DSP1 IN3", "DSP1 IN3 Source Selector"},
	{"DSP1 OUT6 Mixer", "DSP1 IN4", "DSP1 IN4 Source Selector"},
	{"DSP1 OUT6 Mixer", "DSP1 IN5", "DSP1 IN5 Source Selector"},
	{"DSP1 OUT6 Mixer", "DSP1 IN6", "DSP1 IN6 Source Selector"},

	{"DSP2 OUT1 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP2 OUT1 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},
	{"DSP2 OUT1 Mixer", "DSP2 IN3", "DSP2 IN3 Source Selector"},
	{"DSP2 OUT1 Mixer", "DSP2 IN4", "DSP2 IN4 Source Selector"},
	{"DSP2 OUT1 Mixer", "DSP2 IN5", "DSP2 IN5 Source Selector"},
	{"DSP2 OUT1 Mixer", "DSP2 IN6", "DSP2 IN6 Source Selector"},

	{"DSP2 OUT2 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP2 OUT2 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},
	{"DSP2 OUT2 Mixer", "DSP2 IN3", "DSP2 IN3 Source Selector"},
	{"DSP2 OUT2 Mixer", "DSP2 IN4", "DSP2 IN4 Source Selector"},
	{"DSP2 OUT2 Mixer", "DSP2 IN5", "DSP2 IN5 Source Selector"},
	{"DSP2 OUT2 Mixer", "DSP2 IN6", "DSP2 IN6 Source Selector"},

	{"DSP2 OUT3 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP2 OUT3 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},
	{"DSP2 OUT3 Mixer", "DSP2 IN3", "DSP2 IN3 Source Selector"},
	{"DSP2 OUT3 Mixer", "DSP2 IN4", "DSP2 IN4 Source Selector"},
	{"DSP2 OUT3 Mixer", "DSP2 IN5", "DSP2 IN5 Source Selector"},
	{"DSP2 OUT3 Mixer", "DSP2 IN6", "DSP2 IN6 Source Selector"},

	{"DSP2 OUT4 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP2 OUT4 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},
	{"DSP2 OUT4 Mixer", "DSP2 IN3", "DSP2 IN3 Source Selector"},
	{"DSP2 OUT4 Mixer", "DSP2 IN4", "DSP2 IN4 Source Selector"},
	{"DSP2 OUT4 Mixer", "DSP2 IN5", "DSP2 IN5 Source Selector"},
	{"DSP2 OUT4 Mixer", "DSP2 IN6", "DSP2 IN6 Source Selector"},

	{"DSP2 OUT5 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP2 OUT5 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},
	{"DSP2 OUT5 Mixer", "DSP2 IN3", "DSP2 IN3 Source Selector"},
	{"DSP2 OUT5 Mixer", "DSP2 IN4", "DSP2 IN4 Source Selector"},
	{"DSP2 OUT5 Mixer", "DSP2 IN5", "DSP2 IN5 Source Selector"},
	{"DSP2 OUT5 Mixer", "DSP2 IN6", "DSP2 IN6 Source Selector"},

	{"DSP2 OUT6 Mixer", "DSP2 IN1", "DSP2 IN1 Source Selector"},
	{"DSP2 OUT6 Mixer", "DSP2 IN2", "DSP2 IN2 Source Selector"},
	{"DSP2 OUT6 Mixer", "DSP2 IN3", "DSP2 IN3 Source Selector"},
	{"DSP2 OUT6 Mixer", "DSP2 IN4", "DSP2 IN4 Source Selector"},
	{"DSP2 OUT6 Mixer", "DSP2 IN5", "DSP2 IN5 Source Selector"},
	{"DSP2 OUT6 Mixer", "DSP2 IN6", "DSP2 IN6 Source Selector"},

	{"SRC1", NULL, "SRC1 Source Selector"},
	{"SRC2", NULL, "SRC2 Source Selector"},
	{"SRC3", NULL, "SRC3 Source Selector"},
	{"SRC4", NULL, "SRC4 Source Selector"},

	{"FSCONV1", NULL, "FSCONV1 Source Selector"},
	{"FSCONV2", NULL, "FSCONV2 Source Selector"},

	{"MixerA", NULL, "MixerA Ch1 Source Selector"},
	{"MixerA", NULL, "MixerA Ch2 Source Selector"},
	{"MixerB", NULL, "MixerB Ch1 Source Selector"},
	{"MixerB", NULL, "MixerB Ch2 Source Selector"},

	{"SDIN6", NULL, "SDIN6_AIF1"},
	{"SDIN6", NULL, "SDIN6_AIF2"},
	{"SDIN6", NULL, "SDIN6_AIF3"},
	{"SDIN6", NULL, "SDIN6_AIF4"},
	{"SDIN6", NULL, "SDIN6_AIF5"},

	{"DSP1 IN1 Source Selector", "SDIN1", "SDIN1"},
	{"DSP1 IN1 Source Selector", "SDIN2", "SDIN2"},
	{"DSP1 IN1 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP1 IN1 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP1 IN1 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP1 IN1 Source Selector", "SDIN3", "SDIN3"},
	{"DSP1 IN1 Source Selector", "SDIN4", "SDIN4"},
	{"DSP1 IN1 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP1 IN1 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP1 IN1 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP1 IN1 Source Selector", "SDIN5", "SDIN5"},
	{"DSP1 IN1 Source Selector", "SDIN6", "SDIN6"},
	{"DSP1 IN1 Source Selector", "DSP2O1", "DSP2O1"},
	{"DSP1 IN1 Source Selector", "DSP2O2", "DSP2O2"},
	{"DSP1 IN1 Source Selector", "DSP2O3", "DSP2O3"},
	{"DSP1 IN1 Source Selector", "DSP2O4", "DSP2O4"},
	{"DSP1 IN1 Source Selector", "DSP2O5", "DSP2O5"},
	{"DSP1 IN1 Source Selector", "DSP2O6", "DSP2O6"},
	{"DSP1 IN1 Source Selector", "ADC1", "ADC1"},
	{"DSP1 IN1 Source Selector", "ADC2", "ADC2"},
	{"DSP1 IN1 Source Selector", "ADCM", "ADCM"},
	{"DSP1 IN1 Source Selector", "MixerA", "MixerA"},
	{"DSP1 IN1 Source Selector", "MixerB", "MixerB"},
	{"DSP1 IN1 Source Selector", "SRCO1", "SRC1"},
	{"DSP1 IN1 Source Selector", "SRCO2", "SRC2"},
	{"DSP1 IN1 Source Selector", "SRCO3", "SRC3"},
	{"DSP1 IN1 Source Selector", "SRCO4", "SRC4"},
	{"DSP1 IN1 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP1 IN1 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP1 IN2 Source Selector", "SDIN1", "SDIN1"},
	{"DSP1 IN2 Source Selector", "SDIN2", "SDIN2"},
	{"DSP1 IN2 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP1 IN2 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP1 IN2 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP1 IN2 Source Selector", "SDIN3", "SDIN3"},
	{"DSP1 IN2 Source Selector", "SDIN4", "SDIN4"},
	{"DSP1 IN2 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP1 IN2 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP1 IN2 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP1 IN2 Source Selector", "SDIN5", "SDIN5"},
	{"DSP1 IN2 Source Selector", "SDIN6", "SDIN6"},
	{"DSP1 IN2 Source Selector", "DSP2O1", "DSP2O1"},
	{"DSP1 IN2 Source Selector", "DSP2O2", "DSP2O2"},
	{"DSP1 IN2 Source Selector", "DSP2O3", "DSP2O3"},
	{"DSP1 IN2 Source Selector", "DSP2O4", "DSP2O4"},
	{"DSP1 IN2 Source Selector", "DSP2O5", "DSP2O5"},
	{"DSP1 IN2 Source Selector", "DSP2O6", "DSP2O6"},
	{"DSP1 IN2 Source Selector", "ADC1", "ADC1"},
	{"DSP1 IN2 Source Selector", "ADC2", "ADC2"},
	{"DSP1 IN2 Source Selector", "ADCM", "ADCM"},
	{"DSP1 IN2 Source Selector", "MixerA", "MixerA"},
	{"DSP1 IN2 Source Selector", "MixerB", "MixerB"},
	{"DSP1 IN2 Source Selector", "SRCO1", "SRC1"},
	{"DSP1 IN2 Source Selector", "SRCO2", "SRC2"},
	{"DSP1 IN2 Source Selector", "SRCO3", "SRC3"},
	{"DSP1 IN2 Source Selector", "SRCO4", "SRC4"},
	{"DSP1 IN2 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP1 IN2 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP1 IN3 Source Selector", "SDIN1", "SDIN1"},
	{"DSP1 IN3 Source Selector", "SDIN2", "SDIN2"},
	{"DSP1 IN3 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP1 IN3 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP1 IN3 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP1 IN3 Source Selector", "SDIN3", "SDIN3"},
	{"DSP1 IN3 Source Selector", "SDIN4", "SDIN4"},
	{"DSP1 IN3 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP1 IN3 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP1 IN3 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP1 IN3 Source Selector", "SDIN5", "SDIN5"},
	{"DSP1 IN3 Source Selector", "SDIN6", "SDIN6"},
	{"DSP1 IN3 Source Selector", "DSP2O1", "DSP2O1"},
	{"DSP1 IN3 Source Selector", "DSP2O2", "DSP2O2"},
	{"DSP1 IN3 Source Selector", "DSP2O3", "DSP2O3"},
	{"DSP1 IN3 Source Selector", "DSP2O4", "DSP2O4"},
	{"DSP1 IN3 Source Selector", "DSP2O5", "DSP2O5"},
	{"DSP1 IN3 Source Selector", "DSP2O6", "DSP2O6"},
	{"DSP1 IN3 Source Selector", "ADC1", "ADC1"},
	{"DSP1 IN3 Source Selector", "ADC2", "ADC2"},
	{"DSP1 IN3 Source Selector", "ADCM", "ADCM"},
	{"DSP1 IN3 Source Selector", "MixerA", "MixerA"},
	{"DSP1 IN3 Source Selector", "MixerB", "MixerB"},
	{"DSP1 IN3 Source Selector", "SRCO1", "SRC1"},
	{"DSP1 IN3 Source Selector", "SRCO2", "SRC2"},
	{"DSP1 IN3 Source Selector", "SRCO3", "SRC3"},
	{"DSP1 IN3 Source Selector", "SRCO4", "SRC4"},
	{"DSP1 IN3 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP1 IN3 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP1 IN4 Source Selector", "SDIN1", "SDIN1"},
	{"DSP1 IN4 Source Selector", "SDIN2", "SDIN2"},
	{"DSP1 IN4 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP1 IN4 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP1 IN4 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP1 IN4 Source Selector", "SDIN3", "SDIN3"},
	{"DSP1 IN4 Source Selector", "SDIN4", "SDIN4"},
	{"DSP1 IN4 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP1 IN4 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP1 IN4 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP1 IN4 Source Selector", "SDIN5", "SDIN5"},
	{"DSP1 IN4 Source Selector", "SDIN6", "SDIN6"},
	{"DSP1 IN4 Source Selector", "DSP2O1", "DSP2O1"},
	{"DSP1 IN4 Source Selector", "DSP2O2", "DSP2O2"},
	{"DSP1 IN4 Source Selector", "DSP2O3", "DSP2O3"},
	{"DSP1 IN4 Source Selector", "DSP2O4", "DSP2O4"},
	{"DSP1 IN4 Source Selector", "DSP2O5", "DSP2O5"},
	{"DSP1 IN4 Source Selector", "DSP2O6", "DSP2O6"},
	{"DSP1 IN4 Source Selector", "ADC1", "ADC1"},
	{"DSP1 IN4 Source Selector", "ADC2", "ADC2"},
	{"DSP1 IN4 Source Selector", "ADCM", "ADCM"},
	{"DSP1 IN4 Source Selector", "MixerA", "MixerA"},
	{"DSP1 IN4 Source Selector", "MixerB", "MixerB"},
	{"DSP1 IN4 Source Selector", "SRCO1", "SRC1"},
	{"DSP1 IN4 Source Selector", "SRCO2", "SRC2"},
	{"DSP1 IN4 Source Selector", "SRCO3", "SRC3"},
	{"DSP1 IN4 Source Selector", "SRCO4", "SRC4"},
	{"DSP1 IN4 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP1 IN4 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP1 IN5 Source Selector", "SDIN1", "SDIN1"},
	{"DSP1 IN5 Source Selector", "SDIN2", "SDIN2"},
	{"DSP1 IN5 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP1 IN5 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP1 IN5 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP1 IN5 Source Selector", "SDIN3", "SDIN3"},
	{"DSP1 IN5 Source Selector", "SDIN4", "SDIN4"},
	{"DSP1 IN5 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP1 IN5 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP1 IN5 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP1 IN5 Source Selector", "SDIN5", "SDIN5"},
	{"DSP1 IN5 Source Selector", "SDIN6", "SDIN6"},
	{"DSP1 IN5 Source Selector", "DSP2O1", "DSP2O1"},
	{"DSP1 IN5 Source Selector", "DSP2O2", "DSP2O2"},
	{"DSP1 IN5 Source Selector", "DSP2O3", "DSP2O3"},
	{"DSP1 IN5 Source Selector", "DSP2O4", "DSP2O4"},
	{"DSP1 IN5 Source Selector", "DSP2O5", "DSP2O5"},
	{"DSP1 IN5 Source Selector", "DSP2O6", "DSP2O6"},
	{"DSP1 IN5 Source Selector", "ADC1", "ADC1"},
	{"DSP1 IN5 Source Selector", "ADC2", "ADC2"},
	{"DSP1 IN5 Source Selector", "ADCM", "ADCM"},
	{"DSP1 IN5 Source Selector", "MixerA", "MixerA"},
	{"DSP1 IN5 Source Selector", "MixerB", "MixerB"},
	{"DSP1 IN5 Source Selector", "SRCO1", "SRC1"},
	{"DSP1 IN5 Source Selector", "SRCO2", "SRC2"},
	{"DSP1 IN5 Source Selector", "SRCO3", "SRC3"},
	{"DSP1 IN5 Source Selector", "SRCO4", "SRC4"},
	{"DSP1 IN5 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP1 IN5 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP1 IN6 Source Selector", "SDIN1", "SDIN1"},
	{"DSP1 IN6 Source Selector", "SDIN2", "SDIN2"},
	{"DSP1 IN6 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP1 IN6 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP1 IN6 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP1 IN6 Source Selector", "SDIN3", "SDIN3"},
	{"DSP1 IN6 Source Selector", "SDIN4", "SDIN4"},
	{"DSP1 IN6 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP1 IN6 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP1 IN6 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP1 IN6 Source Selector", "SDIN5", "SDIN5"},
	{"DSP1 IN6 Source Selector", "SDIN6", "SDIN6"},
	{"DSP1 IN6 Source Selector", "DSP2O1", "DSP2O1"},
	{"DSP1 IN6 Source Selector", "DSP2O2", "DSP2O2"},
	{"DSP1 IN6 Source Selector", "DSP2O3", "DSP2O3"},
	{"DSP1 IN6 Source Selector", "DSP2O4", "DSP2O4"},
	{"DSP1 IN6 Source Selector", "DSP2O5", "DSP2O5"},
	{"DSP1 IN6 Source Selector", "DSP2O6", "DSP2O6"},
	{"DSP1 IN6 Source Selector", "ADC1", "ADC1"},
	{"DSP1 IN6 Source Selector", "ADC2", "ADC2"},
	{"DSP1 IN6 Source Selector", "ADCM", "ADCM"},
	{"DSP1 IN6 Source Selector", "MixerA", "MixerA"},
	{"DSP1 IN6 Source Selector", "MixerB", "MixerB"},
	{"DSP1 IN6 Source Selector", "SRCO1", "SRC1"},
	{"DSP1 IN6 Source Selector", "SRCO2", "SRC2"},
	{"DSP1 IN6 Source Selector", "SRCO3", "SRC3"},
	{"DSP1 IN6 Source Selector", "SRCO4", "SRC4"},
	{"DSP1 IN6 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP1 IN6 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP2 IN1 Source Selector", "SDIN1", "SDIN1"},
	{"DSP2 IN1 Source Selector", "SDIN2", "SDIN2"},
	{"DSP2 IN1 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP2 IN1 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP2 IN1 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP2 IN1 Source Selector", "SDIN3", "SDIN3"},
	{"DSP2 IN1 Source Selector", "SDIN4", "SDIN4"},
	{"DSP2 IN1 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP2 IN1 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP2 IN1 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP2 IN1 Source Selector", "SDIN5", "SDIN5"},
	{"DSP2 IN1 Source Selector", "SDIN6", "SDIN6"},
	{"DSP2 IN1 Source Selector", "DSP1O1", "DSP2O1"},
	{"DSP2 IN1 Source Selector", "DSP1O2", "DSP2O2"},
	{"DSP2 IN1 Source Selector", "DSP1O3", "DSP2O3"},
	{"DSP2 IN1 Source Selector", "DSP1O4", "DSP2O4"},
	{"DSP2 IN1 Source Selector", "DSP1O5", "DSP2O5"},
	{"DSP2 IN1 Source Selector", "DSP1O6", "DSP2O6"},
	{"DSP2 IN1 Source Selector", "ADC1", "ADC1"},
	{"DSP2 IN1 Source Selector", "ADC2", "ADC2"},
	{"DSP2 IN1 Source Selector", "ADCM", "ADCM"},
	{"DSP2 IN1 Source Selector", "MixerA", "MixerA"},
	{"DSP2 IN1 Source Selector", "MixerB", "MixerB"},
	{"DSP2 IN1 Source Selector", "SRCO1", "SRC1"},
	{"DSP2 IN1 Source Selector", "SRCO2", "SRC2"},
	{"DSP2 IN1 Source Selector", "SRCO3", "SRC3"},
	{"DSP2 IN1 Source Selector", "SRCO4", "SRC4"},
	{"DSP2 IN1 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP2 IN1 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP2 IN2 Source Selector", "SDIN1", "SDIN1"},
	{"DSP2 IN2 Source Selector", "SDIN2", "SDIN2"},
	{"DSP2 IN2 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP2 IN2 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP2 IN2 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP2 IN2 Source Selector", "SDIN3", "SDIN3"},
	{"DSP2 IN2 Source Selector", "SDIN4", "SDIN4"},
	{"DSP2 IN2 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP2 IN2 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP2 IN2 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP2 IN2 Source Selector", "SDIN5", "SDIN5"},
	{"DSP2 IN2 Source Selector", "SDIN6", "SDIN6"},
	{"DSP2 IN2 Source Selector", "DSP1O1","DSP2O1"},
	{"DSP2 IN2 Source Selector", "DSP1O2","DSP2O2"},
	{"DSP2 IN2 Source Selector", "DSP1O3","DSP2O3"},
	{"DSP2 IN2 Source Selector", "DSP1O4","DSP2O4"},
	{"DSP2 IN2 Source Selector", "DSP1O5","DSP2O5"},
	{"DSP2 IN2 Source Selector", "DSP1O6","DSP2O6"},
	{"DSP2 IN2 Source Selector", "ADC1", "ADC1"},
	{"DSP2 IN2 Source Selector", "ADC2", "ADC2"},
	{"DSP2 IN2 Source Selector", "ADCM", "ADCM"},
	{"DSP2 IN2 Source Selector", "MixerA", "MixerA"},
	{"DSP2 IN2 Source Selector", "MixerB", "MixerB"},
	{"DSP2 IN2 Source Selector", "SRCO1", "SRC1"},
	{"DSP2 IN2 Source Selector", "SRCO2", "SRC2"},
	{"DSP2 IN2 Source Selector", "SRCO3", "SRC3"},
	{"DSP2 IN2 Source Selector", "SRCO4", "SRC4"},
	{"DSP2 IN2 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP2 IN2 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP2 IN3 Source Selector", "SDIN1", "SDIN1"},
	{"DSP2 IN3 Source Selector", "SDIN2", "SDIN2"},
	{"DSP2 IN3 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP2 IN3 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP2 IN3 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP2 IN3 Source Selector", "SDIN3", "SDIN3"},
	{"DSP2 IN3 Source Selector", "SDIN4", "SDIN4"},
	{"DSP2 IN3 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP2 IN3 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP2 IN3 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP2 IN3 Source Selector", "SDIN5", "SDIN5"},
	{"DSP2 IN3 Source Selector", "SDIN6", "SDIN6"},
	{"DSP2 IN3 Source Selector", "DSP1O1", "DSP2O1"},
	{"DSP2 IN3 Source Selector", "DSP1O2", "DSP2O2"},
	{"DSP2 IN3 Source Selector", "DSP1O3", "DSP2O3"},
	{"DSP2 IN3 Source Selector", "DSP1O4", "DSP2O4"},
	{"DSP2 IN3 Source Selector", "DSP1O5", "DSP2O5"},
	{"DSP2 IN3 Source Selector", "DSP1O6", "DSP2O6"},
	{"DSP2 IN3 Source Selector", "ADC1", "ADC1"},
	{"DSP2 IN3 Source Selector", "ADC2", "ADC2"},
	{"DSP2 IN3 Source Selector", "ADCM", "ADCM"},
	{"DSP2 IN3 Source Selector", "MixerA", "MixerA"},
	{"DSP2 IN3 Source Selector", "MixerB", "MixerB"},
	{"DSP2 IN3 Source Selector", "SRCO1", "SRC1"},
	{"DSP2 IN3 Source Selector", "SRCO2", "SRC2"},
	{"DSP2 IN3 Source Selector", "SRCO3", "SRC3"},
	{"DSP2 IN3 Source Selector", "SRCO4", "SRC4"},
	{"DSP2 IN3 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP2 IN3 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP2 IN4 Source Selector", "SDIN1", "SDIN1"},
	{"DSP2 IN4 Source Selector", "SDIN2", "SDIN2"},
	{"DSP2 IN4 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP2 IN4 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP2 IN4 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP2 IN4 Source Selector", "SDIN3", "SDIN3"},
	{"DSP2 IN4 Source Selector", "SDIN4", "SDIN4"},
	{"DSP2 IN4 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP2 IN4 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP2 IN4 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP2 IN4 Source Selector", "SDIN5", "SDIN5"},
	{"DSP2 IN4 Source Selector", "SDIN6", "SDIN6"},
	{"DSP2 IN4 Source Selector", "DSP1O1", "DSP2O1"},
	{"DSP2 IN4 Source Selector", "DSP1O2", "DSP2O2"},
	{"DSP2 IN4 Source Selector", "DSP1O3", "DSP2O3"},
	{"DSP2 IN4 Source Selector", "DSP1O4", "DSP2O4"},
	{"DSP2 IN4 Source Selector", "DSP1O5", "DSP2O5"},
	{"DSP2 IN4 Source Selector", "DSP1O6", "DSP2O6"},
	{"DSP2 IN4 Source Selector", "ADC1", "ADC1"},
	{"DSP2 IN4 Source Selector", "ADC2", "ADC2"},
	{"DSP2 IN4 Source Selector", "ADCM", "ADCM"},
	{"DSP2 IN4 Source Selector", "MixerA", "MixerA"},
	{"DSP2 IN4 Source Selector", "MixerB", "MixerB"},
	{"DSP2 IN4 Source Selector", "SRCO1", "SRC1"},
	{"DSP2 IN4 Source Selector", "SRCO2", "SRC2"},
	{"DSP2 IN4 Source Selector", "SRCO3", "SRC3"},
	{"DSP2 IN4 Source Selector", "SRCO4", "SRC4"},
	{"DSP2 IN4 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP2 IN4 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP2 IN5 Source Selector", "SDIN1", "SDIN1"},
	{"DSP2 IN5 Source Selector", "SDIN2", "SDIN2"},
	{"DSP2 IN5 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP2 IN5 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP2 IN5 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP2 IN5 Source Selector", "SDIN3", "SDIN3"},
	{"DSP2 IN5 Source Selector", "SDIN4", "SDIN4"},
	{"DSP2 IN5 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP2 IN5 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP2 IN5 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP2 IN5 Source Selector", "SDIN5", "SDIN5"},
	{"DSP2 IN5 Source Selector", "SDIN6", "SDIN6"},
	{"DSP2 IN5 Source Selector", "DSP1O1","DSP2O1"},
	{"DSP2 IN5 Source Selector", "DSP1O2","DSP2O2"},
	{"DSP2 IN5 Source Selector", "DSP1O3","DSP2O3"},
	{"DSP2 IN5 Source Selector", "DSP1O4","DSP2O4"},
	{"DSP2 IN5 Source Selector", "DSP1O5","DSP2O5"},
	{"DSP2 IN5 Source Selector", "DSP1O6","DSP2O6"},
	{"DSP2 IN5 Source Selector", "ADC1", "ADC1"},
	{"DSP2 IN5 Source Selector", "ADC2", "ADC2"},
	{"DSP2 IN5 Source Selector", "ADCM", "ADCM"},
	{"DSP2 IN5 Source Selector", "MixerA", "MixerA"},
	{"DSP2 IN5 Source Selector", "MixerB", "MixerB"},
	{"DSP2 IN5 Source Selector", "SRCO1", "SRC1"},
	{"DSP2 IN5 Source Selector", "SRCO2", "SRC2"},
	{"DSP2 IN5 Source Selector", "SRCO3", "SRC3"},
	{"DSP2 IN5 Source Selector", "SRCO4", "SRC4"},
	{"DSP2 IN5 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP2 IN5 Source Selector", "FSCO2", "FSCONV2"},

	{"DSP2 IN6 Source Selector", "SDIN1", "SDIN1"},
	{"DSP2 IN6 Source Selector", "SDIN2", "SDIN2"},
	{"DSP2 IN6 Source Selector", "SDIN2B", "SDIN2B"},
	{"DSP2 IN6 Source Selector", "SDIN2C", "SDIN2C"},
	{"DSP2 IN6 Source Selector", "SDIN2D", "SDIN2D"},
	{"DSP2 IN6 Source Selector", "SDIN3", "SDIN3"},
	{"DSP2 IN6 Source Selector", "SDIN4", "SDIN4"},
	{"DSP2 IN6 Source Selector", "SDIN4B", "SDIN4B"},
	{"DSP2 IN6 Source Selector", "SDIN4C", "SDIN4C"},
	{"DSP2 IN6 Source Selector", "SDIN4D", "SDIN4D"},
	{"DSP2 IN6 Source Selector", "SDIN5", "SDIN5"},
	{"DSP2 IN6 Source Selector", "SDIN6", "SDIN6"},
	{"DSP2 IN6 Source Selector", "DSP1O1", "DSP2O1"},
	{"DSP2 IN6 Source Selector", "DSP1O2", "DSP2O2"},
	{"DSP2 IN6 Source Selector", "DSP1O3", "DSP2O3"},
	{"DSP2 IN6 Source Selector", "DSP1O4", "DSP2O4"},
	{"DSP2 IN6 Source Selector", "DSP1O5", "DSP2O5"},
	{"DSP2 IN6 Source Selector", "DSP1O6", "DSP2O6"},
	{"DSP2 IN6 Source Selector", "ADC1", "ADC1"},
	{"DSP2 IN6 Source Selector", "ADC2", "ADC2"},
	{"DSP2 IN6 Source Selector", "ADCM", "ADCM"},
	{"DSP2 IN6 Source Selector", "MixerA", "MixerA"},
	{"DSP2 IN6 Source Selector", "MixerB", "MixerB"},
	{"DSP2 IN6 Source Selector", "SRCO1", "SRC1"},
	{"DSP2 IN6 Source Selector", "SRCO2", "SRC2"},
	{"DSP2 IN6 Source Selector", "SRCO3", "SRC3"},
	{"DSP2 IN6 Source Selector", "SRCO4", "SRC4"},
	{"DSP2 IN6 Source Selector", "FSCO1", "FSCONV1"},
	{"DSP2 IN6 Source Selector", "FSCO2", "FSCONV2"},

	{"SRC1 Source Selector", "SDIN1", "SDIN1"},
	{"SRC1 Source Selector", "SDIN2", "SDIN2"},
	{"SRC1 Source Selector", "SDIN2B", "SDIN2B"},
	{"SRC1 Source Selector", "SDIN2C", "SDIN2C"},
	{"SRC1 Source Selector", "SDIN2D", "SDIN2D"},
	{"SRC1 Source Selector", "SDIN3", "SDIN3"},
	{"SRC1 Source Selector", "SDIN4", "SDIN4"},
	{"SRC1 Source Selector", "SDIN4B", "SDIN4B"},
	{"SRC1 Source Selector", "SDIN4C", "SDIN4C"},
	{"SRC1 Source Selector", "SDIN4D", "SDIN4D"},
	{"SRC1 Source Selector", "SDIN5", "SDIN5"},
	{"SRC1 Source Selector", "SDIN6", "SDIN6"},
	{"SRC1 Source Selector", "DSP1O1", "DSP1O1"},
	{"SRC1 Source Selector", "DSP1O2", "DSP1O2"},
	{"SRC1 Source Selector", "DSP1O3", "DSP1O3"},
	{"SRC1 Source Selector", "DSP1O4", "DSP1O4"},
	{"SRC1 Source Selector", "DSP1O5", "DSP1O5"},
	{"SRC1 Source Selector", "DSP1O6", "DSP1O6"},
	{"SRC1 Source Selector", "DSP2O1", "DSP2O1"},
	{"SRC1 Source Selector", "DSP2O2", "DSP2O2"},
	{"SRC1 Source Selector", "DSP2O3", "DSP2O3"},
	{"SRC1 Source Selector", "DSP2O4", "DSP2O4"},
	{"SRC1 Source Selector", "DSP2O5", "DSP2O5"},
	{"SRC1 Source Selector", "DSP2O6", "DSP2O6"},
	{"SRC1 Source Selector", "ADC1", "ADC1"},
	{"SRC1 Source Selector", "ADC2", "ADC2"},
	{"SRC1 Source Selector", "ADCM", "ADCM"},
	{"SRC1 Source Selector", "MixerA", "MixerA"},
	{"SRC1 Source Selector", "MixerB", "MixerB"},
	{"SRC1 Source Selector", "FSCO1", "FSCONV1"},
	{"SRC1 Source Selector", "FSCO2", "FSCONV2"},

	{"SRC2 Source Selector", "SDIN1", "SDIN1"},
	{"SRC2 Source Selector", "SDIN2", "SDIN2"},
	{"SRC2 Source Selector", "SDIN2B", "SDIN2B"},
	{"SRC2 Source Selector", "SDIN2C", "SDIN2C"},
	{"SRC2 Source Selector", "SDIN2D", "SDIN2D"},
	{"SRC2 Source Selector", "SDIN3", "SDIN3"},
	{"SRC2 Source Selector", "SDIN4", "SDIN4"},
	{"SRC2 Source Selector", "SDIN4B", "SDIN4B"},
	{"SRC2 Source Selector", "SDIN4C", "SDIN4C"},
	{"SRC2 Source Selector", "SDIN4D", "SDIN4D"},
	{"SRC2 Source Selector", "SDIN5", "SDIN5"},
	{"SRC2 Source Selector", "SDIN6", "SDIN6"},
	{"SRC2 Source Selector", "DSP1O1", "DSP1O1"},
	{"SRC2 Source Selector", "DSP1O2", "DSP1O2"},
	{"SRC2 Source Selector", "DSP1O3", "DSP1O3"},
	{"SRC2 Source Selector", "DSP1O4", "DSP1O4"},
	{"SRC2 Source Selector", "DSP1O5", "DSP1O5"},
	{"SRC2 Source Selector", "DSP1O6", "DSP1O6"},
	{"SRC2 Source Selector", "DSP2O1", "DSP2O1"},
	{"SRC2 Source Selector", "DSP2O2", "DSP2O2"},
	{"SRC2 Source Selector", "DSP2O3", "DSP2O3"},
	{"SRC2 Source Selector", "DSP2O4", "DSP2O4"},
	{"SRC2 Source Selector", "DSP2O5", "DSP2O5"},
	{"SRC2 Source Selector", "DSP2O6", "DSP2O6"},
	{"SRC2 Source Selector", "ADC1", "ADC1"},
	{"SRC2 Source Selector", "ADC2", "ADC2"},
	{"SRC2 Source Selector", "ADCM", "ADCM"},
	{"SRC2 Source Selector", "MixerA", "MixerA"},
	{"SRC2 Source Selector", "MixerB", "MixerB"},
	{"SRC2 Source Selector", "FSCO1", "FSCONV1"},
	{"SRC2 Source Selector", "FSCO2", "FSCONV2"},

	{"SRC3 Source Selector", "SDIN1", "SDIN1"},
	{"SRC3 Source Selector", "SDIN2", "SDIN2"},
	{"SRC3 Source Selector", "SDIN2B", "SDIN2B"},
	{"SRC3 Source Selector", "SDIN2C", "SDIN2C"},
	{"SRC3 Source Selector", "SDIN2D", "SDIN2D"},
	{"SRC3 Source Selector", "SDIN3", "SDIN3"},
	{"SRC3 Source Selector", "SDIN4", "SDIN4"},
	{"SRC3 Source Selector", "SDIN4B", "SDIN4B"},
	{"SRC3 Source Selector", "SDIN4C", "SDIN4C"},
	{"SRC3 Source Selector", "SDIN4D", "SDIN4D"},
	{"SRC3 Source Selector", "SDIN5", "SDIN5"},
	{"SRC3 Source Selector", "SDIN6", "SDIN6"},
	{"SRC3 Source Selector", "DSP1O1", "DSP1O1"},
	{"SRC3 Source Selector", "DSP1O2", "DSP1O2"},
	{"SRC3 Source Selector", "DSP1O3", "DSP1O3"},
	{"SRC3 Source Selector", "DSP1O4", "DSP1O4"},
	{"SRC3 Source Selector", "DSP1O5", "DSP1O5"},
	{"SRC3 Source Selector", "DSP1O6", "DSP1O6"},
	{"SRC3 Source Selector", "DSP2O1", "DSP2O1"},
	{"SRC3 Source Selector", "DSP2O2", "DSP2O2"},
	{"SRC3 Source Selector", "DSP2O3", "DSP2O3"},
	{"SRC3 Source Selector", "DSP2O4", "DSP2O4"},
	{"SRC3 Source Selector", "DSP2O5", "DSP2O5"},
	{"SRC3 Source Selector", "DSP2O6", "DSP2O6"},
	{"SRC3 Source Selector", "ADC1", "ADC1"},
	{"SRC3 Source Selector", "ADC2", "ADC2"},
	{"SRC3 Source Selector", "ADCM", "ADCM"},
	{"SRC3 Source Selector", "MixerA", "MixerA"},
	{"SRC3 Source Selector", "MixerB", "MixerB"},
	{"SRC3 Source Selector", "FSCO1", "FSCONV1"},
	{"SRC3 Source Selector", "FSCO2", "FSCONV2"},

	{"SRC4 Source Selector", "SDIN1", "SDIN1"},
	{"SRC4 Source Selector", "SDIN2", "SDIN2"},
	{"SRC4 Source Selector", "SDIN2B", "SDIN2B"},
	{"SRC4 Source Selector", "SDIN2C", "SDIN2C"},
	{"SRC4 Source Selector", "SDIN2D", "SDIN2D"},
	{"SRC4 Source Selector", "SDIN3", "SDIN3"},
	{"SRC4 Source Selector", "SDIN4", "SDIN4"},
	{"SRC4 Source Selector", "SDIN4B", "SDIN4B"},
	{"SRC4 Source Selector", "SDIN4C", "SDIN4C"},
	{"SRC4 Source Selector", "SDIN4D", "SDIN4D"},
	{"SRC4 Source Selector", "SDIN5", "SDIN5"},
	{"SRC4 Source Selector", "SDIN6", "SDIN6"},
	{"SRC4 Source Selector", "DSP1O1", "DSP1O1"},
	{"SRC4 Source Selector", "DSP1O2", "DSP1O2"},
	{"SRC4 Source Selector", "DSP1O3", "DSP1O3"},
	{"SRC4 Source Selector", "DSP1O4", "DSP1O4"},
	{"SRC4 Source Selector", "DSP1O5", "DSP1O5"},
	{"SRC4 Source Selector", "DSP1O6", "DSP1O6"},
	{"SRC4 Source Selector", "DSP2O1", "DSP2O1"},
	{"SRC4 Source Selector", "DSP2O2", "DSP2O2"},
	{"SRC4 Source Selector", "DSP2O3", "DSP2O3"},
	{"SRC4 Source Selector", "DSP2O4", "DSP2O4"},
	{"SRC4 Source Selector", "DSP2O5", "DSP2O5"},
	{"SRC4 Source Selector", "DSP2O6", "DSP2O6"},
	{"SRC4 Source Selector", "ADC1", "ADC1"},
	{"SRC4 Source Selector", "ADC2", "ADC2"},
	{"SRC4 Source Selector", "ADCM", "ADCM"},
	{"SRC4 Source Selector", "MixerA", "MixerA"},
	{"SRC4 Source Selector", "MixerB", "MixerB"},
	{"SRC4 Source Selector", "FSCO1", "FSCONV1"},
	{"SRC4 Source Selector", "FSCO2", "FSCONV2"},

	{"FSCONV1 Source Selector", "SDIN1", "SDIN1"},
	{"FSCONV1 Source Selector", "SDIN2", "SDIN2"},
	{"FSCONV1 Source Selector", "SDIN2B", "SDIN2B"},
	{"FSCONV1 Source Selector", "SDIN2C", "SDIN2C"},
	{"FSCONV1 Source Selector", "SDIN2D", "SDIN2D"},
	{"FSCONV1 Source Selector", "SDIN3", "SDIN3"},
	{"FSCONV1 Source Selector", "SDIN4", "SDIN4"},
	{"FSCONV1 Source Selector", "SDIN4B", "SDIN4B"},
	{"FSCONV1 Source Selector", "SDIN4C", "SDIN4C"},
	{"FSCONV1 Source Selector", "SDIN4D", "SDIN4D"},
	{"FSCONV1 Source Selector", "SDIN5", "SDIN5"},
	{"FSCONV1 Source Selector", "SDIN6", "SDIN6"},
	{"FSCONV1 Source Selector", "DSP1O1", "DSP1O1"},
	{"FSCONV1 Source Selector", "DSP1O2", "DSP1O2"},
	{"FSCONV1 Source Selector", "DSP1O3", "DSP1O3"},
	{"FSCONV1 Source Selector", "DSP1O4", "DSP1O4"},
	{"FSCONV1 Source Selector", "DSP1O5", "DSP1O5"},
	{"FSCONV1 Source Selector", "DSP1O6", "DSP1O6"},
	{"FSCONV1 Source Selector", "DSP2O1", "DSP2O1"},
	{"FSCONV1 Source Selector", "DSP2O2", "DSP2O2"},
	{"FSCONV1 Source Selector", "DSP2O3", "DSP2O3"},
	{"FSCONV1 Source Selector", "DSP2O4", "DSP2O4"},
	{"FSCONV1 Source Selector", "DSP2O5", "DSP2O5"},
	{"FSCONV1 Source Selector", "DSP2O6", "DSP2O6"},
	{"FSCONV1 Source Selector", "ADC1", "ADC1"},
	{"FSCONV1 Source Selector", "ADC2", "ADC2"},
	{"FSCONV1 Source Selector", "ADCM", "ADCM"},
	{"FSCONV1 Source Selector", "MixerA", "MixerA"},
	{"FSCONV1 Source Selector", "MixerB", "MixerB"},
	{"FSCONV1 Source Selector", "SRCO1", "SRC1"},
	{"FSCONV1 Source Selector", "SRCO2", "SRC2"},
	{"FSCONV1 Source Selector", "SRCO3", "SRC3"},
	{"FSCONV1 Source Selector", "SRCO4", "SRC4"},

	{"FSCONV2 Source Selector", "SDIN1", "SDIN1"},
	{"FSCONV2 Source Selector", "SDIN2", "SDIN2"},
	{"FSCONV2 Source Selector", "SDIN2B", "SDIN2B"},
	{"FSCONV2 Source Selector", "SDIN2C", "SDIN2C"},
	{"FSCONV2 Source Selector", "SDIN2D", "SDIN2D"},
	{"FSCONV2 Source Selector", "SDIN3", "SDIN3"},
	{"FSCONV2 Source Selector", "SDIN4", "SDIN4"},
	{"FSCONV2 Source Selector", "SDIN4B", "SDIN4B"},
	{"FSCONV2 Source Selector", "SDIN4C", "SDIN4C"},
	{"FSCONV2 Source Selector", "SDIN4D", "SDIN4D"},
	{"FSCONV2 Source Selector", "SDIN5", "SDIN5"},
	{"FSCONV2 Source Selector", "SDIN6", "SDIN6"},
	{"FSCONV2 Source Selector", "DSP1O1", "DSP1O1"},
	{"FSCONV2 Source Selector", "DSP1O2", "DSP1O2"},
	{"FSCONV2 Source Selector", "DSP1O3", "DSP1O3"},
	{"FSCONV2 Source Selector", "DSP1O4", "DSP1O4"},
	{"FSCONV2 Source Selector", "DSP1O5", "DSP1O5"},
	{"FSCONV2 Source Selector", "DSP1O6", "DSP1O6"},
	{"FSCONV2 Source Selector", "DSP2O1", "DSP2O1"},
	{"FSCONV2 Source Selector", "DSP2O2", "DSP2O2"},
	{"FSCONV2 Source Selector", "DSP2O3", "DSP2O3"},
	{"FSCONV2 Source Selector", "DSP2O4", "DSP2O4"},
	{"FSCONV2 Source Selector", "DSP2O5", "DSP2O5"},
	{"FSCONV2 Source Selector", "DSP2O6", "DSP2O6"},
	{"FSCONV2 Source Selector", "ADC1", "ADC1"},
	{"FSCONV2 Source Selector", "ADC2", "ADC2"},
	{"FSCONV2 Source Selector", "ADCM", "ADCM"},
	{"FSCONV2 Source Selector", "MixerA", "MixerA"},
	{"FSCONV2 Source Selector", "MixerB", "MixerB"},
	{"FSCONV2 Source Selector", "SRCO1", "SRC1"},
	{"FSCONV2 Source Selector", "SRCO2", "SRC2"},
	{"FSCONV2 Source Selector", "SRCO3", "SRC3"},
	{"FSCONV2 Source Selector", "SRCO4", "SRC4"},

	{"MixerA Ch1 Source Selector", "SDIN1", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN2", "SDIN2"},
	{"MixerA Ch1 Source Selector", "SDIN2B", "SDIN2B"},
	{"MixerA Ch1 Source Selector", "SDIN2C", "SDIN2C"},
	{"MixerA Ch1 Source Selector", "SDIN2D", "SDIN2D"},
	{"MixerA Ch1 Source Selector", "SDIN3", "SDIN3"},
	{"MixerA Ch1 Source Selector", "SDIN4", "SDIN4"},
	{"MixerA Ch1 Source Selector", "SDIN4B", "SDIN4B"},
	{"MixerA Ch1 Source Selector", "SDIN4C", "SDIN4C"},
	{"MixerA Ch1 Source Selector", "SDIN4D", "SDIN4D"},
	{"MixerA Ch1 Source Selector", "SDIN5", "SDIN5"},
	{"MixerA Ch1 Source Selector", "SDIN6", "SDIN6"},
	{"MixerA Ch1 Source Selector", "DSP1O1", "DSP1O1"},
	{"MixerA Ch1 Source Selector", "DSP1O2", "DSP1O2"},
	{"MixerA Ch1 Source Selector", "DSP1O3", "DSP1O3"},
	{"MixerA Ch1 Source Selector", "DSP1O4", "DSP1O4"},
	{"MixerA Ch1 Source Selector", "DSP1O5", "DSP1O5"},
	{"MixerA Ch1 Source Selector", "DSP1O6", "DSP1O6"},
	{"MixerA Ch1 Source Selector", "DSP2O1", "DSP2O1"},
	{"MixerA Ch1 Source Selector", "DSP2O2", "DSP2O2"},
	{"MixerA Ch1 Source Selector", "DSP2O3", "DSP2O3"},
	{"MixerA Ch1 Source Selector", "DSP2O4", "DSP2O4"},
	{"MixerA Ch1 Source Selector", "DSP2O5", "DSP2O5"},
	{"MixerA Ch1 Source Selector", "DSP2O6", "DSP2O6"},
	{"MixerA Ch1 Source Selector", "ADC1", "ADC1"},
	{"MixerA Ch1 Source Selector", "ADC2", "ADC2"},
	{"MixerA Ch1 Source Selector", "ADCM", "ADCM"},
	{"MixerA Ch1 Source Selector", "MixerB", "MixerB"},
	{"MixerA Ch1 Source Selector", "SRCO1", "SRC1"},
	{"MixerA Ch1 Source Selector", "SRCO2", "SRC2"},
	{"MixerA Ch1 Source Selector", "SRCO3", "SRC3"},
	{"MixerA Ch1 Source Selector", "SRCO4", "SRC4"},
	{"MixerA Ch1 Source Selector", "FSCO1", "FSCONV1"},
	{"MixerA Ch1 Source Selector", "FSCO2", "FSCONV2"},

	{"MixerA Ch2 Source Selector", "SDIN1", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN2", "SDIN2"},
	{"MixerA Ch2 Source Selector", "SDIN2B", "SDIN2B"},
	{"MixerA Ch2 Source Selector", "SDIN2C", "SDIN2C"},
	{"MixerA Ch2 Source Selector", "SDIN2D", "SDIN2D"},
	{"MixerA Ch2 Source Selector", "SDIN3", "SDIN3"},
	{"MixerA Ch2 Source Selector", "SDIN4", "SDIN4"},
	{"MixerA Ch2 Source Selector", "SDIN4B", "SDIN4B"},
	{"MixerA Ch2 Source Selector", "SDIN4C", "SDIN4C"},
	{"MixerA Ch2 Source Selector", "SDIN4D", "SDIN4D"},
	{"MixerA Ch2 Source Selector", "SDIN5", "SDIN5"},
	{"MixerA Ch2 Source Selector", "SDIN6", "SDIN6"},
	{"MixerA Ch2 Source Selector", "DSP1O1", "DSP1O1"},
	{"MixerA Ch2 Source Selector", "DSP1O2", "DSP1O2"},
	{"MixerA Ch2 Source Selector", "DSP1O3", "DSP1O3"},
	{"MixerA Ch2 Source Selector", "DSP1O4", "DSP1O4"},
	{"MixerA Ch2 Source Selector", "DSP1O5", "DSP1O5"},
	{"MixerA Ch2 Source Selector", "DSP1O6", "DSP1O6"},
	{"MixerA Ch2 Source Selector", "DSP2O1", "DSP2O1"},
	{"MixerA Ch2 Source Selector", "DSP2O2", "DSP2O2"},
	{"MixerA Ch2 Source Selector", "DSP2O3", "DSP2O3"},
	{"MixerA Ch2 Source Selector", "DSP2O4", "DSP2O4"},
	{"MixerA Ch2 Source Selector", "DSP2O5", "DSP2O5"},
	{"MixerA Ch2 Source Selector", "DSP2O6", "DSP2O6"},
	{"MixerA Ch2 Source Selector", "ADC1", "ADC1"},
	{"MixerA Ch2 Source Selector", "ADC2", "ADC2"},
	{"MixerA Ch2 Source Selector", "ADCM", "ADCM"},
	{"MixerA Ch2 Source Selector", "MixerB", "MixerB"},
	{"MixerA Ch2 Source Selector", "SRCO1", "SRC1"},
	{"MixerA Ch2 Source Selector", "SRCO2", "SRC2"},
	{"MixerA Ch2 Source Selector", "SRCO3", "SRC3"},
	{"MixerA Ch2 Source Selector", "SRCO4", "SRC4"},
	{"MixerA Ch2 Source Selector", "FSCO1", "FSCONV1"},
	{"MixerA Ch2 Source Selector", "FSCO2", "FSCONV2"},

	{"MixerB Ch1 Source Selector", "SDIN1", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN2", "SDIN2"},
	{"MixerB Ch1 Source Selector", "SDIN2B", "SDIN2B"},
	{"MixerB Ch1 Source Selector", "SDIN2C", "SDIN2C"},
	{"MixerB Ch1 Source Selector", "SDIN2D", "SDIN2D"},
	{"MixerB Ch1 Source Selector", "SDIN3", "SDIN3"},
	{"MixerB Ch1 Source Selector", "SDIN4", "SDIN4"},
	{"MixerB Ch1 Source Selector", "SDIN4B", "SDIN4B"},
	{"MixerB Ch1 Source Selector", "SDIN4C", "SDIN4C"},
	{"MixerB Ch1 Source Selector", "SDIN4D", "SDIN4D"},
	{"MixerB Ch1 Source Selector", "SDIN5", "SDIN5"},
	{"MixerB Ch1 Source Selector", "SDIN6", "SDIN6"},
	{"MixerB Ch1 Source Selector", "DSP1O1", "DSP1O1"},
	{"MixerB Ch1 Source Selector", "DSP1O2", "DSP1O2"},
	{"MixerB Ch1 Source Selector", "DSP1O3", "DSP1O3"},
	{"MixerB Ch1 Source Selector", "DSP1O4", "DSP1O4"},
	{"MixerB Ch1 Source Selector", "DSP1O5", "DSP1O5"},
	{"MixerB Ch1 Source Selector", "DSP1O6", "DSP1O6"},
	{"MixerB Ch1 Source Selector", "DSP2O1", "DSP2O1"},
	{"MixerB Ch1 Source Selector", "DSP2O2", "DSP2O2"},
	{"MixerB Ch1 Source Selector", "DSP2O3", "DSP2O3"},
	{"MixerB Ch1 Source Selector", "DSP2O4", "DSP2O4"},
	{"MixerB Ch1 Source Selector", "DSP2O5", "DSP2O5"},
	{"MixerB Ch1 Source Selector", "DSP2O6", "DSP2O6"},
	{"MixerB Ch1 Source Selector", "ADC1", "ADC1"},
	{"MixerB Ch1 Source Selector", "ADC2", "ADC2"},
	{"MixerB Ch1 Source Selector", "ADCM", "ADCM"},
	{"MixerB Ch1 Source Selector", "MixerA", "MixerA"},
	{"MixerB Ch1 Source Selector", "SRCO1", "SRC1"},
	{"MixerB Ch1 Source Selector", "SRCO2", "SRC2"},
	{"MixerB Ch1 Source Selector", "SRCO3", "SRC3"},
	{"MixerB Ch1 Source Selector", "SRCO4", "SRC4"},
	{"MixerB Ch1 Source Selector", "FSCO1", "FSCONV1"},
	{"MixerB Ch1 Source Selector", "FSCO2", "FSCONV2"},

	{"MixerB Ch2 Source Selector", "SDIN1", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN2", "SDIN2"},
	{"MixerB Ch2 Source Selector", "SDIN2B", "SDIN2B"},
	{"MixerB Ch2 Source Selector", "SDIN2C", "SDIN2C"},
	{"MixerB Ch2 Source Selector", "SDIN2D", "SDIN2D"},
	{"MixerB Ch2 Source Selector", "SDIN3", "SDIN3"},
	{"MixerB Ch2 Source Selector", "SDIN4", "SDIN4"},
	{"MixerB Ch2 Source Selector", "SDIN4B", "SDIN4B"},
	{"MixerB Ch2 Source Selector", "SDIN4C", "SDIN4C"},
	{"MixerB Ch2 Source Selector", "SDIN4D", "SDIN4D"},
	{"MixerB Ch2 Source Selector", "SDIN5", "SDIN5"},
	{"MixerB Ch2 Source Selector", "SDIN6", "SDIN6"},
	{"MixerB Ch2 Source Selector", "DSP1O1", "DSP1O1"},
	{"MixerB Ch2 Source Selector", "DSP1O2", "DSP1O2"},
	{"MixerB Ch2 Source Selector", "DSP1O3", "DSP1O3"},
	{"MixerB Ch2 Source Selector", "DSP1O4", "DSP1O4"},
	{"MixerB Ch2 Source Selector", "DSP1O5", "DSP1O5"},
	{"MixerB Ch2 Source Selector", "DSP1O6", "DSP1O6"},
	{"MixerB Ch2 Source Selector", "DSP2O1", "DSP2O1"},
	{"MixerB Ch2 Source Selector", "DSP2O2", "DSP2O2"},
	{"MixerB Ch2 Source Selector", "DSP2O3", "DSP2O3"},
	{"MixerB Ch2 Source Selector", "DSP2O4", "DSP2O4"},
	{"MixerB Ch2 Source Selector", "DSP2O5", "DSP2O5"},
	{"MixerB Ch2 Source Selector", "DSP2O6", "DSP2O6"},
	{"MixerB Ch2 Source Selector", "ADC1", "ADC1"},
	{"MixerB Ch2 Source Selector", "ADC2", "ADC2"},
	{"MixerB Ch2 Source Selector", "ADCM", "ADCM"},
	{"MixerB Ch2 Source Selector", "MixerA", "MixerA"},
	{"MixerB Ch2 Source Selector", "SRCO1", "SRC1"},
	{"MixerB Ch2 Source Selector", "SRCO2", "SRC2"},
	{"MixerB Ch2 Source Selector", "SRCO3", "SRC3"},
	{"MixerB Ch2 Source Selector", "SRCO4", "SRC4"},
	{"MixerB Ch2 Source Selector", "FSCO1", "FSCONV1"},
	{"MixerB Ch2 Source Selector", "FSCO2", "FSCONV2"},

	{"SDOUT1 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1 Source Selector", "SDIN2B", "SDIN2B"},
	{"SDOUT1 Source Selector", "SDIN2C", "SDIN2C"},
	{"SDOUT1 Source Selector", "SDIN2D", "SDIN2D"},
	{"SDOUT1 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT1 Source Selector", "SDIN4B", "SDIN4B"},
	{"SDOUT1 Source Selector", "SDIN4C", "SDIN4C"},
	{"SDOUT1 Source Selector", "SDIN4D", "SDIN4D"},
	{"SDOUT1 Source Selector", "SDIN5", "SDIN5"},
	{"SDOUT1 Source Selector", "SDIN6", "SDIN6"},
	{"SDOUT1 Source Selector", "DSP1O1", "DSP1O1"},
	{"SDOUT1 Source Selector", "DSP1O2", "DSP1O2"},
	{"SDOUT1 Source Selector", "DSP1O3", "DSP1O3"},
	{"SDOUT1 Source Selector", "DSP1O4", "DSP1O4"},
	{"SDOUT1 Source Selector", "DSP1O5", "DSP1O5"},
	{"SDOUT1 Source Selector", "DSP1O6", "DSP1O6"},
	{"SDOUT1 Source Selector", "DSP2O1", "DSP2O1"},
	{"SDOUT1 Source Selector", "DSP2O2", "DSP2O2"},
	{"SDOUT1 Source Selector", "DSP2O3", "DSP2O3"},
	{"SDOUT1 Source Selector", "DSP2O4", "DSP2O4"},
	{"SDOUT1 Source Selector", "DSP2O5", "DSP2O5"},
	{"SDOUT1 Source Selector", "DSP2O6", "DSP2O6"},
	{"SDOUT1 Source Selector", "ADC1", "ADC1"},
	{"SDOUT1 Source Selector", "ADC2", "ADC2"},
	{"SDOUT1 Source Selector", "ADCM", "ADCM"},
	{"SDOUT1 Source Selector", "MixerA", "MixerA"},
	{"SDOUT1 Source Selector", "MixerB", "MixerB"},
	{"SDOUT1 Source Selector", "SRCO1", "SRC1"},
	{"SDOUT1 Source Selector", "SRCO2", "SRC2"},
	{"SDOUT1 Source Selector", "SRCO3", "SRC3"},
	{"SDOUT1 Source Selector", "SRCO4", "SRC4"},
	{"SDOUT1 Source Selector", "FSCO1", "FSCONV1"},
	{"SDOUT1 Source Selector", "FSCO2", "FSCONV2"},

	{"SDOUT2 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT2 Source Selector", "SDIN2B", "SDIN2B"},
	{"SDOUT2 Source Selector", "SDIN2C", "SDIN2C"},
	{"SDOUT2 Source Selector", "SDIN2D", "SDIN2D"},
	{"SDOUT2 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT2 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT2 Source Selector", "SDIN4B", "SDIN4B"},
	{"SDOUT2 Source Selector", "SDIN4C", "SDIN4C"},
	{"SDOUT2 Source Selector", "SDIN4D", "SDIN4D"},
	{"SDOUT2 Source Selector", "SDIN5", "SDIN5"},
	{"SDOUT2 Source Selector", "SDIN6", "SDIN6"},
	{"SDOUT2 Source Selector", "DSP1O1", "DSP1O1"},
	{"SDOUT2 Source Selector", "DSP1O2", "DSP1O2"},
	{"SDOUT2 Source Selector", "DSP1O3", "DSP1O3"},
	{"SDOUT2 Source Selector", "DSP1O4", "DSP1O4"},
	{"SDOUT2 Source Selector", "DSP1O5", "DSP1O5"},
	{"SDOUT2 Source Selector", "DSP1O6", "DSP1O6"},
	{"SDOUT2 Source Selector", "DSP2O1", "DSP2O1"},
	{"SDOUT2 Source Selector", "DSP2O2", "DSP2O2"},
	{"SDOUT2 Source Selector", "DSP2O3", "DSP2O3"},
	{"SDOUT2 Source Selector", "DSP2O4", "DSP2O4"},
	{"SDOUT2 Source Selector", "DSP2O5", "DSP2O5"},
	{"SDOUT2 Source Selector", "DSP2O6", "DSP2O6"},
	{"SDOUT2 Source Selector", "ADC1", "ADC1"},
	{"SDOUT2 Source Selector", "ADC2", "ADC2"},
	{"SDOUT2 Source Selector", "ADCM", "ADCM"},
	{"SDOUT2 Source Selector", "MixerA", "MixerA"},
	{"SDOUT2 Source Selector", "MixerB", "MixerB"},
	{"SDOUT2 Source Selector", "SRCO1", "SRC1"},
	{"SDOUT2 Source Selector", "SRCO2", "SRC2"},
	{"SDOUT2 Source Selector", "SRCO3", "SRC3"},
	{"SDOUT2 Source Selector", "SRCO4", "SRC4"},
	{"SDOUT2 Source Selector", "FSCO1", "FSCONV1"},
	{"SDOUT2 Source Selector", "FSCO2", "FSCONV2"},

	{"SDOUT3 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT3 Source Selector", "SDIN2B", "SDIN2B"},
	{"SDOUT3 Source Selector", "SDIN2C", "SDIN2C"},
	{"SDOUT3 Source Selector", "SDIN2D", "SDIN2D"},
	{"SDOUT3 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT3 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT3 Source Selector", "SDIN4B", "SDIN4B"},
	{"SDOUT3 Source Selector", "SDIN4C", "SDIN4C"},
	{"SDOUT3 Source Selector", "SDIN4D", "SDIN4D"},
	{"SDOUT3 Source Selector", "SDIN5", "SDIN5"},
	{"SDOUT3 Source Selector", "SDIN6", "SDIN6"},
	{"SDOUT3 Source Selector", "DSP1O1", "DSP1O1"},
	{"SDOUT3 Source Selector", "DSP1O2", "DSP1O2"},
	{"SDOUT3 Source Selector", "DSP1O3", "DSP1O3"},
	{"SDOUT3 Source Selector", "DSP1O4", "DSP1O4"},
	{"SDOUT3 Source Selector", "DSP1O5", "DSP1O5"},
	{"SDOUT3 Source Selector", "DSP1O6", "DSP1O6"},
	{"SDOUT3 Source Selector", "DSP2O1", "DSP2O1"},
	{"SDOUT3 Source Selector", "DSP2O2", "DSP2O2"},
	{"SDOUT3 Source Selector", "DSP2O3", "DSP2O3"},
	{"SDOUT3 Source Selector", "DSP2O4", "DSP2O4"},
	{"SDOUT3 Source Selector", "DSP2O5", "DSP2O5"},
	{"SDOUT3 Source Selector", "DSP2O6", "DSP2O6"},
	{"SDOUT3 Source Selector", "ADC1", "ADC1"},
	{"SDOUT3 Source Selector", "ADC2", "ADC2"},
	{"SDOUT3 Source Selector", "ADCM", "ADCM"},
	{"SDOUT3 Source Selector", "MixerA", "MixerA"},
	{"SDOUT3 Source Selector", "MixerB", "MixerB"},
	{"SDOUT3 Source Selector", "SRCO1", "SRC1"},
	{"SDOUT3 Source Selector", "SRCO2", "SRC2"},
	{"SDOUT3 Source Selector", "SRCO3", "SRC3"},
	{"SDOUT3 Source Selector", "SRCO4", "SRC4"},
	{"SDOUT3 Source Selector", "FSCO1", "FSCONV1"},
	{"SDOUT3 Source Selector", "FSCO2", "FSCONV2"},

	{"SDOUT4 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT4 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT4 Source Selector", "SDIN2B", "SDIN2B"},
	{"SDOUT4 Source Selector", "SDIN2C", "SDIN2C"},
	{"SDOUT4 Source Selector", "SDIN2D", "SDIN2D"},
	{"SDOUT4 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT4 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT4 Source Selector", "SDIN4B", "SDIN4B"},
	{"SDOUT4 Source Selector", "SDIN4C", "SDIN4C"},
	{"SDOUT4 Source Selector", "SDIN4D", "SDIN4D"},
	{"SDOUT4 Source Selector", "SDIN5", "SDIN5"},
	{"SDOUT4 Source Selector", "SDIN6", "SDIN6"},
	{"SDOUT4 Source Selector", "DSP1O1", "DSP1O1"},
	{"SDOUT4 Source Selector", "DSP1O2", "DSP1O2"},
	{"SDOUT4 Source Selector", "DSP1O3", "DSP1O3"},
	{"SDOUT4 Source Selector", "DSP1O4", "DSP1O4"},
	{"SDOUT4 Source Selector", "DSP1O5", "DSP1O5"},
	{"SDOUT4 Source Selector", "DSP1O6", "DSP1O6"},
	{"SDOUT4 Source Selector", "DSP2O1", "DSP2O1"},
	{"SDOUT4 Source Selector", "DSP2O2", "DSP2O2"},
	{"SDOUT4 Source Selector", "DSP2O3", "DSP2O3"},
	{"SDOUT4 Source Selector", "DSP2O4", "DSP2O4"},
	{"SDOUT4 Source Selector", "DSP2O5", "DSP2O5"},
	{"SDOUT4 Source Selector", "DSP2O6", "DSP2O6"},
	{"SDOUT4 Source Selector", "ADC1", "ADC1"},
	{"SDOUT4 Source Selector", "ADC2", "ADC2"},
	{"SDOUT4 Source Selector", "ADCM", "ADCM"},
	{"SDOUT4 Source Selector", "MixerA", "MixerA"},
	{"SDOUT4 Source Selector", "MixerB", "MixerB"},
	{"SDOUT4 Source Selector", "SRCO1", "SRC1"},
	{"SDOUT4 Source Selector", "SRCO2", "SRC2"},
	{"SDOUT4 Source Selector", "SRCO3", "SRC3"},
	{"SDOUT4 Source Selector", "SRCO4", "SRC4"},
	{"SDOUT4 Source Selector", "FSCO1", "FSCONV1"},
	{"SDOUT4 Source Selector", "FSCO2", "FSCONV2"},

	{"SDOUT5 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT5 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT5 Source Selector", "SDIN2B", "SDIN2B"},
	{"SDOUT5 Source Selector", "SDIN2C", "SDIN2C"},
	{"SDOUT5 Source Selector", "SDIN2D", "SDIN2D"},
	{"SDOUT5 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT5 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT5 Source Selector", "SDIN4B", "SDIN4B"},
	{"SDOUT5 Source Selector", "SDIN4C", "SDIN4C"},
	{"SDOUT5 Source Selector", "SDIN4D", "SDIN4D"},
	{"SDOUT5 Source Selector", "SDIN5", "SDIN5"},
	{"SDOUT5 Source Selector", "SDIN6", "SDIN6"},
	{"SDOUT5 Source Selector", "DSP1O1", "DSP1O1"},
	{"SDOUT5 Source Selector", "DSP1O2", "DSP1O2"},
	{"SDOUT5 Source Selector", "DSP1O3", "DSP1O3"},
	{"SDOUT5 Source Selector", "DSP1O4", "DSP1O4"},
	{"SDOUT5 Source Selector", "DSP1O5", "DSP1O5"},
	{"SDOUT5 Source Selector", "DSP1O6", "DSP1O6"},
	{"SDOUT5 Source Selector", "DSP2O1", "DSP2O1"},
	{"SDOUT5 Source Selector", "DSP2O2", "DSP2O2"},
	{"SDOUT5 Source Selector", "DSP2O3", "DSP2O3"},
	{"SDOUT5 Source Selector", "DSP2O4", "DSP2O4"},
	{"SDOUT5 Source Selector", "DSP2O5", "DSP2O5"},
	{"SDOUT5 Source Selector", "DSP2O6", "DSP2O6"},
	{"SDOUT5 Source Selector", "ADC1", "ADC1"},
	{"SDOUT5 Source Selector", "ADC2", "ADC2"},
	{"SDOUT5 Source Selector", "ADCM", "ADCM"},
	{"SDOUT5 Source Selector", "MixerA", "MixerA"},
	{"SDOUT5 Source Selector", "MixerB", "MixerB"},
	{"SDOUT5 Source Selector", "SRCO1", "SRC1"},
	{"SDOUT5 Source Selector", "SRCO2", "SRC2"},
	{"SDOUT5 Source Selector", "SRCO3", "SRC3"},
	{"SDOUT5 Source Selector", "SRCO4", "SRC4"},
	{"SDOUT5 Source Selector", "FSCO1", "FSCONV1"},
	{"SDOUT5 Source Selector", "FSCO2", "FSCONV2"},

	{"DAC1 Source Selector", "SDIN1", "SDIN1"},
	{"DAC1 Source Selector", "SDIN2", "SDIN2"},
	{"DAC1 Source Selector", "SDIN2B", "SDIN2B"},
	{"DAC1 Source Selector", "SDIN2C", "SDIN2C"},
	{"DAC1 Source Selector", "SDIN2D", "SDIN2D"},
	{"DAC1 Source Selector", "SDIN3", "SDIN3"},
	{"DAC1 Source Selector", "SDIN4", "SDIN4"},
	{"DAC1 Source Selector", "SDIN4B", "SDIN4B"},
	{"DAC1 Source Selector", "SDIN4C", "SDIN4C"},
	{"DAC1 Source Selector", "SDIN4D", "SDIN4D"},
	{"DAC1 Source Selector", "SDIN5", "SDIN5"},
	{"DAC1 Source Selector", "SDIN6", "SDIN6"},
	{"DAC1 Source Selector", "DSP1O1", "DSP1O1"},
	{"DAC1 Source Selector", "DSP1O2", "DSP1O2"},
	{"DAC1 Source Selector", "DSP1O3", "DSP1O3"},
	{"DAC1 Source Selector", "DSP1O4", "DSP1O4"},
	{"DAC1 Source Selector", "DSP1O5", "DSP1O5"},
	{"DAC1 Source Selector", "DSP1O6", "DSP1O6"},
	{"DAC1 Source Selector", "DSP2O1", "DSP2O1"},
	{"DAC1 Source Selector", "DSP2O2", "DSP2O2"},
	{"DAC1 Source Selector", "DSP2O3", "DSP2O3"},
	{"DAC1 Source Selector", "DSP2O4", "DSP2O4"},
	{"DAC1 Source Selector", "DSP2O5", "DSP2O5"},
	{"DAC1 Source Selector", "DSP2O6", "DSP2O6"},
	{"DAC1 Source Selector", "ADC1", "ADC1"},
	{"DAC1 Source Selector", "ADC2", "ADC2"},
	{"DAC1 Source Selector", "ADCM", "ADCM"},
	{"DAC1 Source Selector", "MixerA", "MixerA"},
	{"DAC1 Source Selector", "MixerB", "MixerB"},
	{"DAC1 Source Selector", "SRCO1", "SRC1"},
	{"DAC1 Source Selector", "SRCO2", "SRC2"},
	{"DAC1 Source Selector", "SRCO3", "SRC3"},
	{"DAC1 Source Selector", "SRCO4", "SRC4"},
	{"DAC1 Source Selector", "FSCO1", "FSCONV1"},
	{"DAC1 Source Selector", "FSCO2", "FSCONV2"},

	{"DAC2 Source Selector", "SDIN1", "SDIN1"},
	{"DAC2 Source Selector", "SDIN2", "SDIN2"},
	{"DAC2 Source Selector", "SDIN2B", "SDIN2B"},
	{"DAC2 Source Selector", "SDIN2C", "SDIN2C"},
	{"DAC2 Source Selector", "SDIN2D", "SDIN2D"},
	{"DAC2 Source Selector", "SDIN3", "SDIN3"},
	{"DAC2 Source Selector", "SDIN4", "SDIN4"},
	{"DAC2 Source Selector", "SDIN4B", "SDIN4B"},
	{"DAC2 Source Selector", "SDIN4C", "SDIN4C"},
	{"DAC2 Source Selector", "SDIN4D", "SDIN4D"},
	{"DAC2 Source Selector", "SDIN5", "SDIN5"},
	{"DAC2 Source Selector", "SDIN6", "SDIN6"},
	{"DAC2 Source Selector", "DSP1O1", "DSP1O1"},
	{"DAC2 Source Selector", "DSP1O2", "DSP1O2"},
	{"DAC2 Source Selector", "DSP1O3", "DSP1O3"},
	{"DAC2 Source Selector", "DSP1O4", "DSP1O4"},
	{"DAC2 Source Selector", "DSP1O5", "DSP1O5"},
	{"DAC2 Source Selector", "DSP1O6", "DSP1O6"},
	{"DAC2 Source Selector", "DSP2O1", "DSP2O1"},
	{"DAC2 Source Selector", "DSP2O2", "DSP2O2"},
	{"DAC2 Source Selector", "DSP2O3", "DSP2O3"},
	{"DAC2 Source Selector", "DSP2O4", "DSP2O4"},
	{"DAC2 Source Selector", "DSP2O5", "DSP2O5"},
	{"DAC2 Source Selector", "DSP2O6", "DSP2O6"},
	{"DAC2 Source Selector", "ADC1", "ADC1"},
	{"DAC2 Source Selector", "ADC2", "ADC2"},
	{"DAC2 Source Selector", "ADCM", "ADCM"},
	{"DAC2 Source Selector", "MixerA", "MixerA"},
	{"DAC2 Source Selector", "MixerB", "MixerB"},
	{"DAC2 Source Selector", "SRCO1", "SRC1"},
	{"DAC2 Source Selector", "SRCO2", "SRC2"},
	{"DAC2 Source Selector", "SRCO3", "SRC3"},
	{"DAC2 Source Selector", "SRCO4", "SRC4"},
	{"DAC2 Source Selector", "FSCO1", "FSCONV1"},
	{"DAC2 Source Selector", "FSCO2", "FSCONV2"},

#ifdef AK7738_DIT_ENABLE
	{"DIT", NULL, "Clock Power"},

	{"DIT", NULL, "DIT Source Selector"},

	{"DIT OUT", NULL, "DIT"},


	{"DIT Source Selector", "SDIN1", "SDIN1"},
	{"DIT Source Selector", "SDIN2", "SDIN2"},
	{"DIT Source Selector", "SDIN2B", "SDIN2B"},
	{"DIT Source Selector", "SDIN2C", "SDIN2C"},
	{"DIT Source Selector", "SDIN2D", "SDIN2D"},
	{"DIT Source Selector", "SDIN3", "SDIN3"},
	{"DIT Source Selector", "SDIN4", "SDIN4"},
	{"DIT Source Selector", "SDIN4B", "SDIN4B"},
	{"DIT Source Selector", "SDIN4C", "SDIN4C"},
	{"DIT Source Selector", "SDIN4D", "SDIN4D"},
	{"DIT Source Selector", "SDIN5", "SDIN5"},
	{"DIT Source Selector", "SDIN6", "SDIN6"},
	{"DIT Source Selector", "DSP1O1", "DSP1O1"},
	{"DIT Source Selector", "DSP1O2", "DSP1O2"},
	{"DIT Source Selector", "DSP1O3", "DSP1O3"},
	{"DIT Source Selector", "DSP1O4", "DSP1O4"},
	{"DIT Source Selector", "DSP1O5", "DSP1O5"},
	{"DIT Source Selector", "DSP1O6", "DSP1O6"},
	{"DIT Source Selector", "DSP2O1", "DSP2O1"},
	{"DIT Source Selector", "DSP2O2", "DSP2O2"},
	{"DIT Source Selector", "DSP2O3", "DSP2O3"},
	{"DIT Source Selector", "DSP2O4", "DSP2O4"},
	{"DIT Source Selector", "DSP2O5", "DSP2O5"},
	{"DIT Source Selector", "DSP2O6", "DSP2O6"},
	{"DIT Source Selector", "ADC1", "ADC1"},
	{"DIT Source Selector", "ADC2", "ADC2"},
	{"DIT Source Selector", "ADCM", "ADCM"},
	{"DIT Source Selector", "MixerA", "MixerA"},
	{"DIT Source Selector", "MixerB", "MixerB"},
	{"DIT Source Selector", "SRCO1", "SRC1"},
	{"DIT Source Selector", "SRCO2", "SRC2"},
	{"DIT Source Selector", "SRCO3", "SRC3"},
	{"DIT Source Selector", "SRCO4", "SRC4"},
	{"DIT Source Selector", "FSCO1", "FSCONV1"},
	{"DIT Source Selector", "FSCO2", "FSCONV2"},
#else
	{"SDOUT6", NULL, "Clock Power"},

	{"SDOUT6", NULL, "SDOUT6 Source Selector"},

	{"SDOUT6_AIF1", NULL, "SDOUT6"},
	{"SDOUT6_AIF2", NULL, "SDOUT6"},
	{"SDOUT6_AIF3", NULL, "SDOUT6"},
	{"SDOUT6_AIF4", NULL, "SDOUT6"},
	{"SDOUT6_AIF5", NULL, "SDOUT6"},

	{"SDOUT6 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT6 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT6 Source Selector", "SDIN2B", "SDIN2B"},
	{"SDOUT6 Source Selector", "SDIN2C", "SDIN2C"},
	{"SDOUT6 Source Selector", "SDIN2D", "SDIN2D"},
	{"SDOUT6 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT6 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT6 Source Selector", "SDIN4B", "SDIN4B"},
	{"SDOUT6 Source Selector", "SDIN4C", "SDIN4C"},
	{"SDOUT6 Source Selector", "SDIN4D", "SDIN4D"},
	{"SDOUT6 Source Selector", "SDIN5", "SDIN5"},
	{"SDOUT6 Source Selector", "SDIN6", "SDIN6"},
	{"SDOUT6 Source Selector", "DSP1O1", "DSP1O1"},
	{"SDOUT6 Source Selector", "DSP1O2", "DSP1O2"},
	{"SDOUT6 Source Selector", "DSP1O3", "DSP1O3"},
	{"SDOUT6 Source Selector", "DSP1O4", "DSP1O4"},
	{"SDOUT6 Source Selector", "DSP1O5", "DSP1O5"},
	{"SDOUT6 Source Selector", "DSP1O6", "DSP1O6"},
	{"SDOUT6 Source Selector", "DSP2O1", "DSP2O1"},
	{"SDOUT6 Source Selector", "DSP2O2", "DSP2O2"},
	{"SDOUT6 Source Selector", "DSP2O3", "DSP2O3"},
	{"SDOUT6 Source Selector", "DSP2O4", "DSP2O4"},
	{"SDOUT6 Source Selector", "DSP2O5", "DSP2O5"},
	{"SDOUT6 Source Selector", "DSP2O6", "DSP2O6"},
	{"SDOUT6 Source Selector", "ADC1", "ADC1"},
	{"SDOUT6 Source Selector", "ADC2", "ADC2"},
	{"SDOUT6 Source Selector", "ADCM", "ADCM"},
	{"SDOUT6 Source Selector", "MixerA", "MixerA"},
	{"SDOUT6 Source Selector", "MixerB", "MixerB"},
	{"SDOUT6 Source Selector", "SRCO1", "SRC1"},
	{"SDOUT6 Source Selector", "SRCO2", "SRC2"},
	{"SDOUT6 Source Selector", "SRCO3", "SRC3"},
	{"SDOUT6 Source Selector", "SRCO4", "SRC4"},
	{"SDOUT6 Source Selector", "FSCO1", "FSCONV1"},
	{"SDOUT6 Source Selector", "FSCO2", "FSCONV2"},
#endif

	{"SDOUT2", NULL, "SDOUT2 TDM Selector"},

	{"SDOUT2 TDM Selector", "Normal", "SDOUT2 Normal"},
	{"SDOUT2 TDM Selector", "TDM128_1", "SDOUT2 TDM128_1"},
	{"SDOUT2 TDM Selector", "TDM128_2", "SDOUT2 TDM128_2"},
	{"SDOUT2 TDM Selector", "TDM256", "SDOUT2 TDM256"},

	{"SDOUT2 Normal", NULL, "SDOUT2 Source Selector"},
	{"SDOUT2 TDM128_1", NULL, "DSP1O1"},
	{"SDOUT2 TDM128_1", NULL, "DSP1O2"},
	{"SDOUT2 TDM128_2", NULL, "SRC1"},
	{"SDOUT2 TDM128_2", NULL, "SRC2"},
	{"SDOUT2 TDM256", NULL, "DSP2O1"},
	{"SDOUT2 TDM256", NULL, "DSP2O2"},
	{"SDOUT2 TDM256", NULL, "DSP2O3"},
	{"SDOUT2 TDM256", NULL, "DSP2O4"},


	{"SDOUT3", NULL, "SDOUT3 TDM Selector"},

	{"SDOUT3 TDM Selector", "Normal", "SDOUT3 Normal"},
	{"SDOUT3 TDM Selector", "TDM128_1", "SDOUT3 TDM128_1"},
	{"SDOUT3 TDM Selector", "TDM128_2", "SDOUT3 TDM128_2"},
	{"SDOUT3 TDM Selector", "TDM256", "SDOUT3 TDM256"},

	{"SDOUT3 Normal", NULL, "SDOUT3 Source Selector"},
	{"SDOUT3 TDM128_1", NULL, "DSP1O3"},
	{"SDOUT3 TDM128_1", NULL, "DSP1O4"},
	{"SDOUT3 TDM128_2", NULL, "DSP2O3"},
	{"SDOUT3 TDM128_2", NULL, "DSP2O4"},
	{"SDOUT3 TDM256", NULL, "DSP1O1"},
	{"SDOUT3 TDM256", NULL, "DSP1O2"},
	{"SDOUT3 TDM256", NULL, "DSP1O3"},
	{"SDOUT3 TDM256", NULL, "DSP1O4"},


	{"SDOUT4", NULL, "SDOUT4 TDM Selector"},

	{"SDOUT4 TDM Selector", "Normal", "SDOUT4 Normal"},
	{"SDOUT4 TDM Selector", "TDM128_1", "SDOUT4 TDM128_1"},
	{"SDOUT4 TDM Selector", "TDM128_2", "SDOUT4 TDM128_2"},

	{"SDOUT4 Normal", NULL, "SDOUT4 Source Selector"},
	{"SDOUT4 TDM128_1", NULL, "DSP2O5"},
	{"SDOUT4 TDM128_1", NULL, "DSP2O6"},
	{"SDOUT4 TDM128_2", NULL, "SRC3"},
	{"SDOUT4 TDM128_2", NULL, "SRC3"},


	{"SDOUT5", NULL, "SDOUT5 TDM Selector"},

	{"SDOUT5 TDM Selector", "Normal", "SDOUT5 Normal"},
	{"SDOUT5 TDM Selector", "TDM128_1", "SDOUT5 TDM128_1"},
	{"SDOUT5 TDM Selector", "TDM128_2", "SDOUT5 TDM128_2"},

	{"SDOUT5 Normal", NULL, "SDOUT5 Source Selector"},
	{"SDOUT5 TDM128_1", NULL, "DSP1O5"},
	{"SDOUT5 TDM128_1", NULL, "DSP1O6"},
	{"SDOUT5 TDM128_2", NULL, "DSP2O1"},
	{"SDOUT5 TDM128_2", NULL, "DSP2O2"},


};

static int ak7738_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int nSDNo;
	int fsno, nmax;
	int DIODLbit, addr, value, channels;

	ak7738->fs = params_rate(params);

	akdbgprt("[AK7738] %s(%d) fs=%d  Audio format %u\n",__FUNCTION__,__LINE__, ak7738->fs,params_format(params) );
    mdelay(10);

	DIODLbit = 2;

	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			DIODLbit = 2;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			DIODLbit = 0;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			DIODLbit = 3;
			break;
		default:
			pr_err("%s: invalid Audio format %u\n", __func__, params_format(params));
			return -EINVAL;
	}

	switch (dai->id) {
		case AIF_PORT1: nSDNo = 0; break;
		case AIF_PORT2: nSDNo = 1; break;
		case AIF_PORT3: nSDNo = 2; break;
		case AIF_PORT4: nSDNo = 3; break;
		case AIF_PORT5: nSDNo = 4; break;
		default:
			pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
			return -EINVAL;
			break;
	}

	fsno = 0;
	nmax = sizeof(sdfstab) / sizeof(sdfstab[0]);
	channels = params_channels(params);
	akdbgprt("[AK7738] %s nmax = %d channels = %d DIODLbit = %d\n",__FUNCTION__, nmax,channels,DIODLbit);

	do {
		if ( ak7738->fs <= sdfstab[fsno] ) break;
		fsno++;
	} while ( fsno < nmax );
	akdbgprt("[AK7738] %s fsno = %d\n",__FUNCTION__, fsno);

	if ( fsno == nmax ) {
		pr_err("%s: not support Sampling Frequency : %d\n", __func__, ak7738->fs);
		return -EINVAL;
	}

	akdbgprt("[AK7738] %s setSDClock\n",__FUNCTION__);
    mdelay(10);

	ak7738->SDfs[nSDNo] = fsno;
	setSDClock(codec, nSDNo);

	/* set Word length */

	addr = AK7738_48_SDIN1_FORMAT + nSDNo;
	value = DIODLbit;
	snd_soc_update_bits(codec, addr, 0x03, value);
	addr = AK7738_4E_SDOUT1_FORMAT + nSDNo;
	snd_soc_update_bits(codec, addr, 0x03, value);

	return 0;
}

static int ak7738_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	akdbgprt("[AK7738] %s(%d)\n",__FUNCTION__,__LINE__);

	return 0;
}

static int ak7738_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct snd_soc_codec *codec = dai->codec;
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int format, diolsb, diedge, doedge, dislot, doslot;
	int msnbit;
	int nSDNo, value;
	int rdata, addr, mask, shift;

	akdbgprt("[AK7738] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (dai->id) {
		case AIF_PORT1: nSDNo = 0; break;
		case AIF_PORT2: nSDNo = 1; break;
		case AIF_PORT3: nSDNo = 2; break;
		case AIF_PORT4: nSDNo = 3; break;
		case AIF_PORT5: nSDNo = 4; break;
		default:
			pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
			return -EINVAL;
			break;
	}

	/* set master/slave audio interface */

    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBS_CFS:
			msnbit = 0;
			akdbgprt("[AK7738] CBS_CFS %s(Slave_nSDNo=%d)\n",__FUNCTION__,nSDNo);
            break;
        case SND_SOC_DAIFMT_CBM_CFM:
			msnbit = 1;
			akdbgprt("[AK7738] CBM_CFM %s(Master_nSDNo=%d)\n",__FUNCTION__,nSDNo);
            break;
        case SND_SOC_DAIFMT_CBS_CFM:
        case SND_SOC_DAIFMT_CBM_CFS:
        default:
            dev_err(codec->dev, "Clock mode unsupported");
           return -EINVAL;
    	}

	/* set TDMout setting */
	rdata = snd_soc_read(codec, AK7738_55_TDM_MODE_SETTING);
	shift = 8 - nSDNo*2;
	value = ((rdata >> shift) & 0x03) ;

	if ((nSDNo !=0) && (nSDNo != 5)){
		if (value != 0){
			ak7738->DOEDGEbit[nSDNo] = 1;
		}
		else{
			ak7738->DOEDGEbit[nSDNo] = 0;
		}
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			akdbgprt("[AK7738] SND_SOC_DAIFMT_I2S %s(Slave_nSDNo=%d)\n",__FUNCTION__,nSDNo);
			format = AK7738_LRIF_I2S_MODE; // 0
			diolsb = 0;
			diedge = ak7738->DIEDGEbit[nSDNo];
			doedge = ak7738->DOEDGEbit[nSDNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			format = AK7738_LRIF_MSB_MODE; // 5
			diolsb = 0;
			diedge = ak7738->DIEDGEbit[nSDNo];
			doedge = ak7738->DOEDGEbit[nSDNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			format = AK7738_LRIF_LSB_MODE; // 5
			diolsb = 1;
			diedge = ak7738->DIEDGEbit[nSDNo];
			doedge = ak7738->DOEDGEbit[nSDNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			format = AK7738_LRIF_PCM_SHORT_MODE; // 6
			diolsb = 0;
			diedge = 1;
			doedge = 1;
			dislot = ak7738->DISLbit[nSDNo];
			doslot = ak7738->DOSLbit[nSDNo];
			akdbgprt("[AK7738] SND_SOC_DAIFMT_DSP_A %s(Slave_nSDNo=%d) dislot(%d) doslot(%d)\n",__FUNCTION__,nSDNo,dislot,doslot);
			break;
		case SND_SOC_DAIFMT_DSP_B:
			akdbgprt("[AK7738] SND_SOC_DAIFMT_DSP_B %s(Slave_nSDNo=%d)\n",__FUNCTION__,nSDNo);
			format = AK7738_LRIF_PCM_LONG_MODE; // 7
			diolsb = 0;
			diedge = 1;
			doedge = 1;
			dislot = ak7738->DISLbit[nSDNo];
			doslot = ak7738->DOSLbit[nSDNo];
			break;
		default:
			return -EINVAL;
	}

	/* set format */
	setSDMaster(codec, nSDNo, msnbit);
	addr = AK7738_44_CLOCKFORMAT_SETTING1 + (nSDNo/2);
	shift = 4 * ((nSDNo+1) % 2);
	if(nSDNo == 4){
		shift = 0;
	}
	value = (format << shift);
	mask = 0xF << shift;

	snd_soc_update_bits(codec, addr, mask, value);

	/* set SDIO format */
	/* set Slot length */

	addr = AK7738_48_SDIN1_FORMAT + nSDNo;
	value = (diedge <<  7) + (diolsb <<  3) + (dislot <<  4);
	snd_soc_update_bits(codec, addr, 0xB8, value);

	addr = AK7738_4E_SDOUT1_FORMAT + nSDNo;
	value = (doedge <<  7) + (diolsb <<  3) + (doslot <<  4);
	snd_soc_update_bits(codec, addr, 0xB8, value);

	return 0;
}

static bool ak7738_volatile(struct device *dev, unsigned int reg)
{
	bool	ret;

#ifdef AK7738_DEBUG
	if (  reg <  AK7738_VIRT_REGISTER ) ret = true;
	else ret = false;
#else
	ret = false;
#endif
	return(ret);
}

static bool ak7738_readable(struct device *dev, unsigned int reg)
{
	bool ret;

	if (reg <= AK7738_MAX_REGISTER)
		ret = true;
	else
		ret = false;

	return ret;

}

static bool ak7738_writeable(struct device *dev, unsigned int reg)
{
	bool ret;

	if ((reg > 0x85) && (reg < 0x91)) {
		ret = false;
	}
	else if ((reg > 0x94) && (reg < 0x100)) {
		ret = false;
	}
	else {
		switch(reg) {
		case AK7738_14_RESERVED:
		case AK7738_22_RESERVED:
		case AK7738_42_RESERVED:
		case AK7738_43_RESERVED:
		case AK7738_47_RESERVED:
		case AK7738_56_RESERVED:
		case AK7738_5A_RESERVED:
		case AK7738_64_RESERVED:
		case AK7738_65_RESERVED:
		case AK7738_6C_RESERVED:
		case AK7738_6D_RESERVED:
		case AK7738_79_RESERVED:
		case AK7738_86_RESERVED:
		case AK7738_87_RESERVED:
			ret = false;
			break;
		default:
			ret = true;
			break;
		}
	}

	return ret;
}

#ifdef AK7738_I2C_IF
static unsigned int ak7738_i2c_read(
struct i2c_client *client,
u8 *reg,
int reglen,
u8 *data,
int datalen)
{
	struct i2c_msg xfer[2];
	int ret;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = data;

	ret = i2c_transfer(client->adapter, xfer, 2);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return -ret;
	else
		return -EIO;
}
#endif

unsigned int ak7738_read_register(struct snd_soc_codec *codec, unsigned int reg)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[3], rx[1];
	int	wlen, rlen;
	int ret;
	unsigned int rdata;

	if (reg == SND_SOC_NOPM) return 0;

	if (!ak7738_volatile(NULL, reg) && ak7738_readable(NULL, reg)) {
		rdata = 0;
		ret = regmap_read(ak7738->regmap, reg, &rdata);
//		akdbgprt("[AK7738] %s cache(addr, value)=(%XH, %XH)\n",__FUNCTION__, reg, rdata);
		return(rdata);
	}
	wlen = 3;
	rlen = 1;
	tx[0] = (unsigned char)(COMMAND_READ_REG & 0x7F);
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);

#ifdef AK7738_I2C_IF
	ret = ak7738_i2c_read(ak7738->i2c, tx, wlen, rx, rlen);
#else
	ret = spi_write_then_read(ak7738->spi, tx, wlen, rx, rlen);
#endif

	if (ret < 0) {
		akdbgprt("\t[AK7738] %s error ret = %d\n",__FUNCTION__, ret);
		rdata = -EIO;
	}
	else {
		rdata = (unsigned int)rx[0];
	}

//	akdbgprt("[AK7738] %s (addr, value)=(%XH, %XH)\n",__FUNCTION__, reg, rdata);

	return rdata;
}

static int ak7738_reads(struct snd_soc_codec *codec, u8 *tx, size_t wlen, u8 *rx, size_t rlen)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int ret;

	akdbgprt("*****[AK7738] %s tx[0]=%x, %d, %d\n",__FUNCTION__, tx[0], (int)wlen, (int)rlen);
#ifdef AK7738_I2C_IF
	ret = ak7738_i2c_read(ak7738->i2c, tx, wlen, rx, rlen);
#else
	ret = spi_write_then_read(ak7738->spi, tx, wlen, rx, rlen);
#endif

	return ret;

}

static int ak7738_write_register(struct snd_soc_codec *codec,  unsigned int reg,  unsigned int value)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[4];
	int wlen;
	int ret;

	akdbgprt("[AK7738] %s (%XH, %XH)\n",__FUNCTION__, reg, value);

	if (reg == SND_SOC_NOPM)
		return 0;

	if (!ak7738_volatile(NULL, reg)) {
		ret = regmap_write(ak7738->regmap, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

 	if ( reg >= AK7738_VIRT_REGISTER ) {
		return(0);
	}

	wlen = 4;
	tx[0] = (unsigned char)COMMAND_WRITE_REG;
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);
	tx[3] = value;
#ifdef AK7738_I2C_IF
	ret = i2c_master_send(ak7738->i2c, tx, wlen);
#else
	ret = spi_write(ak7738->spi, tx, wlen);
#endif
	return ret;
}

#ifndef AK7738_I2C_IF
static int ak7738_write_spidmy(struct snd_soc_codec *codec)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[4];
	int wlen;
	int rd;

	wlen = 4;
	tx[0] = (unsigned char)(0xDE);
	tx[1] = (unsigned char)(0xAD);
	tx[2] = (unsigned char)(0xDA);
	tx[3] = (unsigned char)(0x7A);

	rd = spi_write(ak7738->spi, tx, wlen);

	return rd;
}
#endif

static int ak7738_writes(struct snd_soc_codec *codec, const u8 *tx, size_t wlen)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int rc;

	akdbgprt("[AK7738W] %s tx[0]=%x tx[1]=%x, len=%d\n",__FUNCTION__, (int)tx[0], (int)tx[1], (int)wlen);

#ifdef AK7738_I2C_IF
	rc = i2c_master_send(ak7738->i2c, tx, wlen);
#else
	rc = spi_write(ak7738->spi, tx, wlen);
#endif

	return rc;
}

/*********************************/

static int crc_read(struct snd_soc_codec *codec)
{
	int rc;

	u8	tx[3], rx[2];

	tx[0] = COMMAND_CRC_READ;
	tx[1] = 0;
	tx[2] = 0;

	rc =  ak7738_reads(codec, tx, 3, rx, 2);

	return (rc < 0) ? rc : ((rx[0] << 8) + rx[1]);
}

static int ak7738_set_status(struct snd_soc_codec *codec, enum ak7738_status status)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

	switch (status) {
		case RUN1:
			snd_soc_update_bits(codec, AK7738_01_STSCLOCK_SETTING2, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x99, 0x19);  // CRESETN bit = D1RESETN = HRESETN = 1;
			break;
		case RUN2:
			snd_soc_update_bits(codec, AK7738_01_STSCLOCK_SETTING2, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x95, 0x15);  // CRESETN bit = D2RESETN = HRESETN = 1;
			break;
		case RUN3:
			snd_soc_update_bits(codec, AK7738_01_STSCLOCK_SETTING2, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x93, 0x13);  // CRESETN bit = D3RESETN = HRESETN = 1;
			break;
		case DOWNLOAD:
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x80, 0x80);  // DLRDY bit = 1
			mdelay(1);
			break;
		case DOWNLOAD_FINISH:
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x80, 0x0);  // DLRDY bit = 0
			mdelay(1);
			break;
		case STANDBY:
			snd_soc_update_bits(codec, AK7738_01_STSCLOCK_SETTING2, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x9F, 0x0);
			break;
		case SUSPEND:
		case POWERDOWN:
			snd_soc_update_bits(codec, AK7738_85_RESET_CTRL, 0x9F, 0x0);
			snd_soc_update_bits(codec, AK7738_01_STSCLOCK_SETTING2, 0x80, 0x0);
			break;
		default:
		return -EINVAL;
	}

	ak7738->status = status;

	return 0;
}

static int ak7738_ram_download(struct snd_soc_codec *codec, const u8 *tx_ram, u64 num, u16 crc)
{
	int rc;
	int nDSPRun;
	u16	read_crc;

	akdbgprt("\t[AK7738] %s num=%ld\n",__FUNCTION__, (long int)num);

	nDSPRun = snd_soc_read(codec, AK7738_85_RESET_CTRL);

	ak7738_set_status(codec, DOWNLOAD);

	rc = ak7738_writes(codec, tx_ram, num);
	if (rc < 0) {
		snd_soc_write(codec, AK7738_85_RESET_CTRL, nDSPRun);
		return rc;
	}

	if ( ( crc != 0 ) && (rc >= 0) )  {
		read_crc = crc_read(codec);
		akdbgprt("\t[AK7738] %s CRC Cal=%x Read=%x\n",__FUNCTION__, (int)crc,(int)read_crc);

		if ( read_crc == crc ) rc = 0;
		else rc = 1;
	}

	snd_soc_write(codec, AK7738_85_RESET_CTRL, nDSPRun);

	return rc;

}

static int calc_CRC(int length, u8 *data )
{

#define CRC16_CCITT (0x1021)

	unsigned short crc = 0x0000;
	int i, j;

	for ( i = 0; i < length; i++ ) {
		crc ^= *data++ << 8;
		for ( j = 0; j < 8; j++) {
			if ( crc & 0x8000) {
				crc <<= 1;
				crc ^= CRC16_CCITT;
			}
			else {
				crc <<= 1;
			}
		}
	}

	akdbgprt("[AK7738] %s CRC=%x\n",__FUNCTION__, crc);

	return crc;
}

static int ak7738_write_ram(
struct snd_soc_codec *codec,
int	 nPCRam,  // 0 : PRAM, 1 : CRAM, 2:  OFREG
u8 	*upRam,
int	 nWSize)
{
	int n, ret;
	int	wCRC;
	int nMaxSize;

	akdbgprt("[AK7738] %s RamNo=%d, len=%d\n",__FUNCTION__, nPCRam, nWSize);

	switch(nPCRam) {
		case RAMTYPE_PRAM:
			if ( upRam[0] == COMMAND_WRITE_PRAM_SUB ) {
				nMaxSize = TOTAL_NUM_OF_SUB_PRAM_MAX;
			}
			else {
				nMaxSize = TOTAL_NUM_OF_PRAM_MAX;
			}

			if (  nWSize > nMaxSize ) {
				printk("%s: PRAM Write size is over! \n", __func__);
				return(-1);
			}
			break;
		case RAMTYPE_CRAM:
			if ( upRam[0] == COMMAND_WRITE_CRAM_SUB ) {
				nMaxSize = TOTAL_NUM_OF_SUB_CRAM_MAX;
			}
			else {
				nMaxSize = TOTAL_NUM_OF_CRAM_MAX;
			}
			if (  nWSize > nMaxSize ) {
				printk("%s: CRAM Write size is over! \n", __func__);
				return(-1);
			}
			break;
		case RAMTYPE_OFREG:
			if (  nWSize > TOTAL_NUM_OF_OFREG_MAX ) {
				printk("%s: OFREG Write size is over! \n", __func__);
				return(-1);
			}
			break;
		default:
			break;
	}

	wCRC = calc_CRC(nWSize, upRam);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7738_ram_download(codec, upRam, nWSize, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! RAM No = %d \n", __func__, nPCRam);
		return(-1);
	}

	return(0);

}

static int ak7738_firmware_write_ram(struct snd_soc_codec *codec, u16 mode, u16 cmd)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int nNumMode, nMaxLen;
	int nRamSize;
	u8  *ram_basic;
	const struct firmware *fw;
	u8  *fwdn;
	char szFileName[32];

	akdbgprt("[AK7738] %s mode=%d, cmd=%d\n",__FUNCTION__, mode, cmd);

	switch(mode) {
		case RAMTYPE_PRAM:
			nNumMode = sizeof(ak7738_firmware_pram) / sizeof(ak7738_firmware_pram[0]);
			break;
		case RAMTYPE_CRAM:
			nNumMode = sizeof(ak7738_firmware_cram) / sizeof(ak7738_firmware_cram[0]);
			break;
		case RAMTYPE_OFREG:
			nNumMode = sizeof(ak7738_firmware_ofreg) / sizeof(ak7738_firmware_ofreg[0]);
			break;
		default:
			akdbgprt("[AK7738] %s mode Error=%d\n",__FUNCTION__, mode);
			return( -EINVAL);
	}

	if ( cmd == 0 ) return(0);

	if ( cmd >= nNumMode ) {
		pr_err("%s: invalid command %d\n", __func__, cmd);
		return( -EINVAL);
	}

	if ( cmd == 1 ) {
		switch(mode) {
			case RAMTYPE_PRAM:
				ram_basic = ak7738_pram_basic1;
				nRamSize = sizeof(ak7738_pram_basic1);
				break;
			case RAMTYPE_CRAM:
				ram_basic = ak7738_cram_basic1;
				nRamSize = sizeof(ak7738_cram_basic1);
				break;
			case RAMTYPE_OFREG:
				ram_basic = ak7738_ofreg_basic1;
				nRamSize = sizeof(ak7738_ofreg_basic1);
				break;
			default:
				return( -EINVAL);
		}
		ret = ak7738_write_ram(codec, (int)mode, ram_basic, nRamSize);
	}

	else if ( cmd == 2 ) {
		switch(mode) {
			case RAMTYPE_PRAM:
				ram_basic = ak7738_pram_basic2;
				nRamSize = sizeof(ak7738_pram_basic2);
				break;
			case RAMTYPE_CRAM:
				ram_basic = ak7738_cram_basic2;
				nRamSize = sizeof(ak7738_cram_basic2);
				break;
			case RAMTYPE_OFREG:
				ram_basic = ak7738_ofreg_basic2;
				nRamSize = sizeof(ak7738_ofreg_basic2);
				break;
			default:
				return( -EINVAL);
		}
		ret = ak7738_write_ram(codec, (int)mode, ram_basic, nRamSize);
	}



	else {
		switch(mode) {
			case RAMTYPE_PRAM:
				sprintf(szFileName, "ak7738_pram_%s.bin", ak7738_firmware_pram[cmd]);
				nMaxLen = TOTAL_NUM_OF_PRAM_MAX;
				break;
			case RAMTYPE_CRAM:
				sprintf(szFileName, "ak7738_cram_%s.bin", ak7738_firmware_cram[cmd]);
				nMaxLen = TOTAL_NUM_OF_CRAM_MAX;
				break;
			case RAMTYPE_OFREG:
				sprintf(szFileName, "ak7738_ofreg_%s.bin", ak7738_firmware_ofreg[cmd]);
				nMaxLen = TOTAL_NUM_OF_OFREG_MAX;
				break;
			default:
				return( -EINVAL);
		}

#ifdef AK7738_I2C_IF
		ret = request_firmware(&fw, szFileName, &(ak7738->i2c->dev));
#else
		ret = request_firmware(&fw, szFileName, &(ak7738->spi->dev));
#endif
		if (ret) {
			akdbgprt("[AK7738] %s could not load firmware=%d\n", szFileName, ret);
			return -EINVAL;
		}

		akdbgprt("[AK7738] %s name=%s size=%d\n",__FUNCTION__, szFileName, (int)fw->size);
		if ( fw->size > nMaxLen ) {
			akdbgprt("[AK7738] %s RAM Size Error : %d\n",__FUNCTION__, (int)fw->size);
			release_firmware(fw);	//16/03/24
			return -ENOMEM;
		}

		fwdn = kmalloc((unsigned long)fw->size, GFP_KERNEL);
		if (fwdn == NULL) {
			printk(KERN_ERR "failed to buffer vmalloc: %d\n", (int)fw->size);
			release_firmware(fw);	//16/03/24
			return -ENOMEM;
		}

		memcpy((void *)fwdn, fw->data, fw->size);

		ret = ak7738_write_ram(codec, (int)mode, (u8 *)fwdn, (int)(fw->size));

		kfree(fwdn);
		release_firmware(fw);	//16/03/24
	}


	return ret;
}


#ifdef AK7738_IO_CONTROL

static int ak7738_write_cram(
struct snd_soc_codec *codec,
int nDSPNo,
int addr,
int len,
unsigned char *cram_data)
{
	int i, n, ret;
	int nDSPRun, nDSPbit;
	unsigned char tx[51];

	akdbgprt("[AK7738] %s addr=%d, len=%d\n",__FUNCTION__, addr, len);

	if ( len > 48 ) {
		akdbgprt("[AK7738] %s Length over!\n",__FUNCTION__);
		return(-1);
	}

	nDSPRun = snd_soc_read(codec, AK7738_85_RESET_CTRL);
	if ( nDSPNo == 1 ) {
		nDSPbit = 0x8;
	}
	else if ( nDSPNo == 2 ) {
		nDSPbit = 0x4;
	}
	else if ( nDSPNo == 3 ) {
		nDSPbit = 0x2;
	}
	else {
		akdbgprt("[AK7738] %s DSP No. Error!\n",__FUNCTION__);
		return(-1);
	}

	if ( nDSPRun & nDSPbit ) {
		tx[0] = COMMAND_WRITE_CRAM_RUN + (unsigned char)((len / 3) - 1);
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}
	else {
		ak7738_set_status(codec, DOWNLOAD);
		tx[0] = COMMAND_WRITE_CRAM + (nDSPNo - 1);
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}

	n = 3;
	for ( i = 0 ; i < len; i ++ ) {
		tx[n++] = cram_data[i];
	}

	ret = ak7738_writes(codec, tx, n);
	if ( nDSPRun & nDSPbit ) {
		tx[0] = COMMAND_WRITE_CRAM_EXEC + nDSPNo - 1;
		tx[1] = 0;
		tx[2] = 0;
		ret = ak7738_writes(codec, tx, 3);
	}
	else {
		snd_soc_write(codec, AK7738_85_RESET_CTRL, nDSPRun);
	}


	return ret;

}

static unsigned long ak7738_readMIR(
struct snd_soc_codec *codec,
int nDSPNo,
int nMIRNo,
unsigned long *dwMIRData)
{
	unsigned char tx[3];
	unsigned char rx[32];
	int n, nRLen;

	if ( nMIRNo >= 8 ) return(-1);

	if ( nDSPNo == 1 ) {
		tx[0] = (unsigned char)COMMAND_MIR1_READ;
	}
	else {
		tx[0] = (unsigned char)COMMAND_MIR2_READ;
	}
	tx[1] = 0;
	tx[2] = 0;
	nRLen = 4 * (nMIRNo + 1);

	ak7738_reads(codec, tx, 3, rx, nRLen);

	n = 4 * nMIRNo;
	*dwMIRData = ((0xFF & (unsigned long)rx[n++]) << 20);
	*dwMIRData += ((0xFF & (unsigned long)rx[n++]) << 12);
    *dwMIRData += ((0xFF & (unsigned long)rx[n++]) << 4);
    *dwMIRData += ((0xFF & (unsigned long)rx[n++]) >> 4);

	return(0);

}


static int ak7738_reg_cmd(struct snd_soc_codec *codec, REG_CMD *reg_param, int len)
{
	int i;
	int rc;
	int  addr, value;

	akdbgprt("*****[AK7738] %s len = %d\n",__FUNCTION__, len);

	rc = 0;
	for (i = 0; i < len; i++) {
		addr = (int)(reg_param[i].addr);
		value = (int)(reg_param[i].data);

		if ( addr != 0xFF ) {
			rc = snd_soc_write(codec, addr, value);
			if (rc < 0) {
				break;
			}
		}
		else {
			mdelay(value);
		}
	}

	if (rc != 0 ) rc = 0;

	return(rc);

}

static long ak7738_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	struct ak7738_priv *ak7738 = (struct ak7738_priv*)file->private_data;
	struct ak7738_wreg_handle ak7738_wreg;
	struct ak7738_wcram_handle  ak7738_wcram;
	void __user *data = (void __user *)args;
	int *val = (int *)args;
	int i;
	unsigned long dwMIRData;
	int ret = 0;
	REG_CMD      regcmd[MAX_WREG];
	unsigned char cram_data[MAX_WCRAM];

	akdbgprt("*****[AK7738] %s cmd, val=%x, %x\n",__FUNCTION__, cmd, val[0]);

	switch(cmd) {
	case AK7738_IOCTL_WRITECRAM:
		if (copy_from_user(&ak7738_wcram, data, sizeof(struct ak7738_wcram_handle)))
			return -EFAULT;
		if ( (  ak7738_wcram.len % 3 ) != 0 ) {
			printk(KERN_ERR "[AK7738] %s CRAM len error\n",__FUNCTION__);
			return -EFAULT;
		}
		if ( ( ak7738_wcram.len < 3 ) || ( ak7738_wcram.len > MAX_WCRAM ) ) {
			printk(KERN_ERR "[AK7738] %s CRAM len error2\n",__FUNCTION__);
			return -EFAULT;
		}
		for ( i = 0 ; i < ak7738_wcram.len ; i ++ ) {
			cram_data[i] = ak7738_wcram.cram[i];
		}
		ret = ak7738_write_cram(ak7738->codec, ak7738_wcram.dsp, ak7738_wcram.addr, ak7738_wcram.len, cram_data);
		break;
	case AK7738_IOCTL_WRITEREG:
		if (copy_from_user(&ak7738_wreg, data, sizeof(struct ak7738_wreg_handle)))
			return -EFAULT;
		if ( ( ak7738_wreg.len < 1 ) || ( ak7738_wreg.len > MAX_WREG ) ) {
			printk(KERN_ERR "MAXREG ERROR %d\n", ak7738_wreg.len );
			return -EFAULT;
		}
		for ( i = 0 ; i < ak7738_wreg.len; i ++ ) {
			regcmd[i].addr = ak7738_wreg.regcmd[i].addr;
			regcmd[i].data = ak7738_wreg.regcmd[i].data;
		}
		ak7738_reg_cmd(ak7738->codec, regcmd, ak7738_wreg.len);
		break;
	case AK7738_IOCTL_SETSTATUS:
		ret = ak7738_set_status(ak7738->codec, val[0]);
		if (ret < 0) {
			printk(KERN_ERR "ak7738: set_status error: \n");
			return ret;
		}
		break;
	case AK7738_IOCTL_SETMIR:
		ak7738->MIRNo = val[0];
		if (ret < 0) {
			printk(KERN_ERR "ak7738: set MIR error\n");
			return -EFAULT;
		}
		break;
	case AK7738_IOCTL_GETMIR:
		ak7738_readMIR(ak7738->codec, (ak7738->MIRNo/16), (0xF & (ak7738->MIRNo)), &dwMIRData);
		ret = copy_to_user(data, (const void*)&dwMIRData, (unsigned long)4);
		if (ret < 0) {
			printk(KERN_ERR "ak7738: get status error\n");
			return -EFAULT;
		}
		break;
		break;
	default:
		printk(KERN_ERR "Unknown command required: %d\n", cmd);
		return -EINVAL;
	}

	return ret;
}

static int init_ak7738_pd(struct ak7738_priv *data)
{
	struct _ak7738_pd_handler *ak7738 = &ak7738_pd_handler;

	if (data == NULL)
		return -EFAULT;

	mutex_init(&ak7738->lock);

	mutex_lock(&ak7738->lock);
	ak7738->data = data;
	mutex_unlock(&ak7738->lock);

	printk("data:%p, ak7738->data:%p\n", data, ak7738->data);

	return 0;
}

static struct ak7738_priv* get_ak7738_pd(void)
{
	struct _ak7738_pd_handler *ak7738 = &ak7738_pd_handler;

	if (ak7738->data == NULL)
		return NULL;

	mutex_lock(&ak7738->lock);
	ak7738->ref_count++;
	mutex_unlock(&ak7738->lock);

	return ak7738->data;
}

static int rel_ak7738_pd(struct ak7738_priv *data)
{
	struct _ak7738_pd_handler *ak7738 = &ak7738_pd_handler;

	if (ak7738->data == NULL)
		return -EFAULT;

	mutex_lock(&ak7738->lock);
	ak7738->ref_count--;
	mutex_unlock(&ak7738->lock);

	data = NULL;

	return 0;
}

/* AK7738 Misc driver interfaces */
static int ak7738_open(struct inode *inode, struct file *file)
{
	struct ak7738_priv *ak7738;

	ak7738 = get_ak7738_pd();
	file->private_data = ak7738;

	return 0;
}

static int ak7738_close(struct inode *inode, struct file *file)
{
	struct ak7738_priv *ak7738 = (struct ak7738_priv*)file->private_data;

	rel_ak7738_pd(ak7738);

	return 0;
}


static const struct file_operations ak7738_fops = {
	.owner = THIS_MODULE,
	.open = ak7738_open,
	.release = ak7738_close,
	.unlocked_ioctl = ak7738_ioctl,
};

static struct miscdevice ak7738_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ak7738-dsp",
	.fops = &ak7738_fops,
};

#endif

/*********************************/


static int ak7738_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	akdbgprt("\t[AK7738] %s(%d)(%d)\n",__FUNCTION__,__LINE__,level);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		ak7738_set_status(codec, POWERDOWN);
		break;
	}
//	codec->dapm.bias_level = level;

	return 0;
}



static int ak7738_set_dai_mute(struct snd_soc_dai *dai, int mute)
{

	return 0;
}

#define AK7738_RATES		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
				SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
				SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |\
				SNDRV_PCM_RATE_192000)

#define AK7738_DAC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE
#define AK7738_ADC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE


static struct snd_soc_dai_ops ak7738_dai_ops = {
	.hw_params	= ak7738_hw_params,
	.set_sysclk	= ak7738_set_dai_sysclk,
	.set_fmt	= ak7738_set_dai_fmt,
	.digital_mute = ak7738_set_dai_mute,
	/*Add for tdm slot function here .set_tdm_slot =*/
};

struct snd_soc_dai_driver ak7738_dai[] = {
	{
		.name = "ak7738-aif1",
		.id = AIF_PORT1,
		.playback = {
		       .stream_name = "AIF1 Playback",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF1 Capture",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_ADC_FORMATS,
		},
		.ops = &ak7738_dai_ops,
	},
	{
		.name = "ak7738-aif2",
		.id = AIF_PORT2,
		.playback = {
		       .stream_name = "AIF2 Playback",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF2 Capture",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_ADC_FORMATS,
		},
		.ops = &ak7738_dai_ops,
	},
	{
		.name = "ak7738-aif3",
		.id = AIF_PORT3,
		.playback = {
		       .stream_name = "AIF3 Playback",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF3 Capture",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_ADC_FORMATS,
		},
		.ops = &ak7738_dai_ops,
	},
	{
		.name = "ak7738-aif4",
		.id = AIF_PORT4,
		.playback = {
		       .stream_name = "AIF4 Playback",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF4 Capture",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_ADC_FORMATS,
		},
		.ops = &ak7738_dai_ops,
	},
	{
		.name = "ak7738-aif5",
		.id = AIF_PORT5,
		.playback = {
		       .stream_name = "AIF5 Playback",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF5 Capture",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7738_RATES,
		       .formats = AK7738_ADC_FORMATS,
		},
		.ops = &ak7738_dai_ops,
	},
};

static int ak7738_parse_dt(struct ak7738_priv *ak7738)
{
	struct device *dev;
	struct device_node *np;

#ifdef AK7738_I2C_IF
	dev = &(ak7738->i2c->dev);
#else
	dev = &(ak7738->spi->dev);
#endif

	np = dev->of_node;

	if (!np)
		return -1;

	printk("Read PDN pin from device tree\n");

	ak7738->pdn_gpio = of_get_named_gpio(np, "ak7738,pdn-gpio", 0);
	if (ak7738->pdn_gpio < 0) {
		ak7738->pdn_gpio = -1;
		return -1;
	}

	if( !gpio_is_valid(ak7738->pdn_gpio) ) {
		printk(KERN_ERR "ak7738 pdn pin(%u) is invalid\n", ak7738->pdn_gpio);
		return -1;
	}

	return 0;
}

static int ak7738_init_reg(struct snd_soc_codec *codec)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int i, devid;

	if ( ak7738->pdn_gpio != -1 ) {   // 16/06/24
		/* Modify from gpio_set_value to gpio_set_value_cansleep for extern gpio */
		akdbgprt("****Set to cansleep*****\n");
		gpio_set_value_cansleep(ak7738->pdn_gpio, 0);
		msleep(1);
		gpio_set_value_cansleep(ak7738->pdn_gpio, 1);
		msleep(1);
	}

#ifndef AK7738_I2C_IF
	ak7738_write_spidmy(codec);
#endif

	devid = snd_soc_read(codec, AK7738_100_DEVICE_ID);
	printk("[AK7738] %s  Device ID = 0x%X\n",__FUNCTION__, devid);
	if ( devid != 0x38 ) {
		akdbgprt("***** This is not AK7738! *****\n");
		akdbgprt("***** This is not AK7738! *****\n");
		akdbgprt("***** This is not AK7738! *****\n");
	}

	ak7738->fs = 48000;
	ak7738->PLLInput = 0;
	ak7738->XtiFs    = 0;
	ak7738->BUSMclk  = 0;
	ak7738->BUSMclk  = 1; /* Change BUS clk to Addr: 0x12 0x9 bus clock equal PLLMCLK*10 */

	setPLLOut(codec);
	setBUSClock(codec);

	for ( i = 0 ; i < 5 ; i ++ ) ak7738->Master[i]=0;

	for ( i = 0 ; i < NUM_SYNCDOMAIN ; i ++ ) {
		ak7738->SDBick[i]  = 0; //64fs
		ak7738->SDfs[i] = 5;  // 48kHz
		ak7738->SDCks[i] = 1;  // 0 : Low -> 1: PLLMCLK,

		setSDClock(codec, i);
	}

	snd_soc_write(codec, AK7738_15_SYNCDOMAIN_SELECT1, 0x12);
	snd_soc_write(codec, AK7738_16_SYNCDOMAIN_SELECT2, 0x34);
	snd_soc_update_bits(codec, AK7738_17_SYNCDOMAIN_SELECT3, 0xF0, 0x50);

	return 0;
}

static int ak7738_probe(struct snd_soc_codec *codec)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	akdbgprt("\t[AK7738] %s(%d)\n",__FUNCTION__,__LINE__);
	ak7738->codec = codec;

	ret = ak7738_parse_dt(ak7738);
	if ( ret < 0 ) ak7738->pdn_gpio = -1;

	if ( ak7738->pdn_gpio != -1 ) {   // 16/06/24
		ret = gpio_request(ak7738->pdn_gpio, "ak7738 pdn");
		akdbgprt("\t[AK7738] %s : gpio_request ret = %d\n",__FUNCTION__, ret);
		gpio_direction_output(ak7738->pdn_gpio, 0);
	}

	ak7738_init_reg(codec);

	ak7738->MIRNo = 0;
	ak7738->status = POWERDOWN;

	ak7738->DSPPramMode = 0;
	ak7738->DSPCramMode = 0;
	ak7738->DSPOfregMode = 0;

#ifdef AK7738_IO_CONTROL
	init_ak7738_pd(ak7738);
#endif

	ak7738->nDSP2 = 0;
	ak7738->nSubDSP = 0;
	/*Next section is added by user by dsp firmware*/
	ak7738->DSP2Switch1 = 0;
	/*Max is 255 :12dB, default set to -10dB
	Hal could overwrite it. */
	ak7738->DSP1HFPVol = 211;
	return ret;
}

static int ak7738_remove(struct snd_soc_codec *codec)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("\t[AK7738] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7738_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if ( ak7738->pdn_gpio != -1 ) {   // 16/06/24
		/* Modify from gpio_set_value to gpio_set_value_cansleep for extern gpio */
		gpio_set_value_cansleep(ak7738->pdn_gpio, 0);
		msleep(1);
		gpio_free(ak7738->pdn_gpio);
		msleep(1);
	}

	return 0;
}

static int ak7738_suspend(struct snd_soc_codec *codec)  // 16/06/24
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);

	ak7738_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if ( ak7738->pdn_gpio != -1 ) {   // 16/06/24
		/* Modify from gpio_set_value to gpio_set_value_cansleep for extern gpio */
		gpio_set_value_cansleep(ak7738->pdn_gpio, 0);
		msleep(1);
	}
	//snd_soc_cache_init(codec);

	return 0;
}

static int ak7738_resume(struct snd_soc_codec *codec)
{
	struct ak7738_priv *ak7738 = snd_soc_codec_get_drvdata(codec);
	int i;

	for ( i = 0 ; i < ARRAY_SIZE(ak7738_reg) ; i++ ) {
		regmap_write(ak7738->regmap, ak7738_reg[i].reg, ak7738_reg[i].def);
	}

	ak7738_init_reg(codec);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_ak7738 = {
	.probe = ak7738_probe,
	.remove = ak7738_remove,
	.suspend =	ak7738_suspend,
	.resume =	ak7738_resume,

	.write = ak7738_write_register,
	.read = ak7738_read_register,

	.idle_bias_off = true,   // 16/06/24
	.set_bias_level = ak7738_set_bias_level,
	.component_driver = {
		.controls = ak7738_snd_controls,
		.num_controls = ARRAY_SIZE(ak7738_snd_controls),
		.dapm_widgets = ak7738_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ak7738_dapm_widgets),
		.dapm_routes = ak7738_intercon,
		.num_dapm_routes = ARRAY_SIZE(ak7738_intercon),
	},
};
EXPORT_SYMBOL_GPL(soc_codec_dev_ak7738);

static const struct regmap_config ak7738_regmap = {
	.reg_bits = 16,
	.val_bits = 8,

	.max_register = AK7738_MAX_REGISTER,
	.readable_reg = ak7738_readable,
	.volatile_reg = ak7738_volatile,
	.writeable_reg = ak7738_writeable,

	.reg_defaults = ak7738_reg,
	.num_reg_defaults = ARRAY_SIZE(ak7738_reg),
	.cache_type = REGCACHE_RBTREE,
};

static struct of_device_id ak7738_if_dt_ids[] = {
	{ .compatible = "akm,ak7738"},
    { }
};
MODULE_DEVICE_TABLE(of, ak7738_if_dt_ids);

#ifdef AK7738_I2C_IF

static int ak7738_i2c_probe(struct i2c_client *i2c,
                            const struct i2c_device_id *id)
{
	struct ak7738_priv *ak7738;
	int ret=0;

	akdbgprt("\t[AK7738] %s(%d)\n",__func__,__LINE__);

	ak7738 = devm_kzalloc(&i2c->dev, sizeof(struct ak7738_priv), GFP_KERNEL);
	if (ak7738 == NULL) return -ENOMEM;

	ak7738->regmap = devm_regmap_init_i2c(i2c, &ak7738_regmap);
	if (IS_ERR(ak7738->regmap)) {
		devm_kfree(&i2c->dev, ak7738);
		return PTR_ERR(ak7738->regmap);
	}

	i2c_set_clientdata(i2c, ak7738);

	ak7738->i2c = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ak7738, &ak7738_dai[0], ARRAY_SIZE(ak7738_dai));
	if (ret < 0){
		devm_kfree(&i2c->dev, ak7738);
		akdbgprt("\t[AK7738 Error!] %s(%d)\n",__func__,__LINE__);
	}
	return ret;
}

static int ak7738_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id ak7738_i2c_id[] = {
	{ "ak7738", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak7738_i2c_id);

static struct i2c_driver ak7738_i2c_driver = {
	.driver = {
		.name = "ak7738",
//		.owner = THIS_MODULE,

		.of_match_table = of_match_ptr(ak7738_if_dt_ids),
	},
	.probe = ak7738_i2c_probe,
	.remove = ak7738_i2c_remove,
	.id_table = ak7738_i2c_id,
};

#else

static int ak7738_spi_probe(struct spi_device *spi)
{
	struct ak7738_priv *ak7738;
	int	ret;

	akdbgprt("\t[AK7738] %s\n",__FUNCTION__);

	ak7738 = devm_kzalloc(&spi->dev, sizeof(struct ak7738_priv),
			      GFP_KERNEL);
	if (ak7738 == NULL)
		return -ENOMEM;

	ak7738->regmap = devm_regmap_init_spi(spi, &ak7738_regmap);
	if (IS_ERR(ak7738->regmap)) {
		ret = PTR_ERR(ak7738->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	spi_set_drvdata(spi, ak7738);

	ak7738->spi = spi;

	ret = snd_soc_register_codec(&spi->dev,
			&soc_codec_dev_ak7738, &ak7738_dai[0], ARRAY_SIZE(ak7738_dai));

	if (ret != 0) {
		dev_err(&spi->dev, "Failed to register CODEC: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ak7738_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}

static struct spi_driver ak7738_spi_driver = {
	.driver = {
		.name = "ak7738",
//		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak7738_if_dt_ids),
	},
	.probe = ak7738_spi_probe,
	.remove = ak7738_spi_remove,
};
#endif

static int __init ak7738_modinit(void)
{
	int ret = 0;

	akdbgprt("\t[AK7738] %s(%d)\n", __FUNCTION__,__LINE__);

#ifdef AK7738_I2C_IF
	ret = i2c_add_driver(&ak7738_i2c_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register AK7738 I2C driver: %d\n", ret);

	}
#else
	ret = spi_register_driver(&ak7738_spi_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register AK7738 SPI driver: %d\n",  ret);

	}
#endif

#ifdef AK7738_IO_CONTROL
	ret = misc_register(&ak7738_misc);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register AK7738 MISC driver: %d\n",  ret);
	}
#endif

	return ret;
}

module_init(ak7738_modinit);

static void __exit ak7738_exit(void)
{
#ifdef AK7738_I2C_IF
	i2c_del_driver(&ak7738_i2c_driver);
#else
	spi_unregister_driver(&ak7738_spi_driver);
#endif

#ifdef AK7738_IO_CONTROL
	misc_deregister(&ak7738_misc);
#endif

}
module_exit(ak7738_exit);

MODULE_AUTHOR("J.Wakasugi <wakasugi.jb@om.asahi-kasei.co.jp>");
MODULE_DESCRIPTION("ASoC ak7738 codec driver");
MODULE_LICENSE("GPL v2");

