/*
 * ak7738.h  --  audio driver for ak7738
 *
 * Copyright (C) 2015 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      15/05/14	    1.1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _AK7738_H
#define _AK7738_H

#include "ak7738ctl.h"

#define AK7738_I2C_IF
#define AK7738_IO_CONTROL

#define NUM_SYNCDOMAIN  7

/* AK7738_C1_CLOCK_SETTING2 (0xC1)  Fields */
#define AK7738_AIF_BICK32		(2 << 4)
#define AK7738_AIF_BICK48		(1 << 4)
#define AK7738_AIF_BICK64		(0 << 4)
#define AK7738_AIF_TDM			(3 << 4)	//TDM256 bit is set "1" at initialization.

/* TDMMODE Input Source */
#define AK7738_TDM_DSP				(0 << 2)
#define AK7738_TDM_DSP_AD1			(1 << 2)
#define AK7738_TDM_DSP_AD1_AD2		(2 << 2)

/* User Setting */
//#define DIGITAL_MIC
//#define CLOCK_MODE_BICK
//#define CLOCK_MODE_18_432
#define AK7738_AUDIO_IF_MODE		AK7738_AIF_BICK64	//32fs, 48fs, 64fs, 256fs(TDM)
#define AK7738_TDM_INPUT_SOURCE		AK7738_TDM_DSP		//Effective only in TDM mode
#define AK7738_BCKP_BIT				(0 << 6)	//BICK Edge Setting
#define AK7738_SOCFG_BIT  			(0 << 4)	//SO pin Hi-z Setting
#define AK7738_DMCLKP1_BIT			(0 << 6)	//DigitalMIC 1 Channnel Setting
#define AK7738_DMCLKP2_BIT			(0 << 3)	//DigitalMIC 1 Channnel Setting
/* User Setting */

#define AK7738_00_STSCLOCK_SETTING1        0x00
#define AK7738_01_STSCLOCK_SETTING2        0x01
#define AK7738_02_MICBIAS_PLOWER           0x02
#define AK7738_03_SYNCDOMAIN1_SETTING1     0x03
#define AK7738_04_SYNCDOMAIN1_SETTING2     0x04
#define AK7738_05_SYNCDOMAIN2_SETTING1     0x05
#define AK7738_06_SYNCDOMAIN2_SETTING2     0x06
#define AK7738_07_SYNCDOMAIN3_SETTING1     0x07
#define AK7738_08_SYNCDOMAIN3_SETTING2     0x08
#define AK7738_09_SYNCDOMAIN4_SETTING1     0x09
#define AK7738_0A_SYNCDOMAIN4_SETTING2     0x0A
#define AK7738_0B_SYNCDOMAIN5_SETTING1     0x0B
#define AK7738_0C_SYNCDOMAIN5_SETTING2     0x0C
#define AK7738_0D_SYNCDOMAIN6_SETTING1     0x0D
#define AK7738_0E_SYNCDOMAIN6_SETTING2     0x0E
#define AK7738_0E_SYNCDOMAIN7_SETTING1     0x0F
#define AK7738_10_SYNCDOMAIN7_SETTING2     0x10
#define AK7738_11_BUS_DSP_CLOCK_SETTING    0x11
#define AK7738_12_BUS_CLOCK_SETTING2       0x12
#define AK7738_13_CLOKO_OUTPUT_SETTING     0x13
#define AK7738_14_RESERVED                 0x14
#define AK7738_15_SYNCDOMAIN_SELECT1       0x15
#define AK7738_16_SYNCDOMAIN_SELECT2       0x16
#define AK7738_17_SYNCDOMAIN_SELECT3       0x17
#define AK7738_18_SYNCDOMAIN_SELECT4       0x18
#define AK7738_19_SYNCDOMAIN_SELECT5       0x19
#define AK7738_1A_SYNCDOMAIN_SELECT6       0x1A
#define AK7738_1B_SYNCDOMAIN_SELECT7       0x1B
#define AK7738_1C_SYNCDOMAIN_SELECT8       0x1C
#define AK7738_1D_SYNCDOMAIN_SELECT9       0x1D
#define AK7738_1E_SYNCDOMAIN_SELECT10      0x1E
#define AK7738_1F_SYNCDOMAIN_SELECT11      0x1F
#define AK7738_20_SYNCDOMAIN_SELECT12      0x20
#define AK7738_21_SYNCDOMAIN_SELECT13      0x21
#define AK7738_22_RESERVED                 0x22
#define AK7738_23_SDOUT1_DATA_SELECT       0x23
#define AK7738_24_SDOUT2_DATA_SELECT       0x24
#define AK7738_25_SDOUT3_DATA_SELECT       0x25
#define AK7738_26_SDOUT4_DATA_SELECT       0x26
#define AK7738_27_SDOUT5_DATA_SELECT       0x27
#define AK7738_28_SDOUT6_DATA_SELECT       0x28
#define AK7738_29_DAC1_DATA_SELECT         0x29
#define AK7738_2A_DAC2_DATA_SELECT         0x2A
#define AK7738_2B_DSP1_IN1_DATA_SELECT     0x2B
#define AK7738_2C_DSP1_IN2_DATA_SELECT     0x2C
#define AK7738_2D_DSP1_IN3_DATA_SELECT     0x2D
#define AK7738_2E_DSP1_IN4_DATA_SELECT     0x2E
#define AK7738_2F_DSP1_IN5_DATA_SELECT     0x2F
#define AK7738_30_DSP1_IN6_DATA_SELECT     0x30
#define AK7738_31_DSP2_IN1_DATA_SELECT     0x31
#define AK7738_32_DSP2_IN2_DATA_SELECT     0x32
#define AK7738_33_DSP2_IN3_DATA_SELECT     0x33
#define AK7738_34_DSP2_IN4_DATA_SELECT     0x34
#define AK7738_35_DSP2_IN5_DATA_SELECT     0x35
#define AK7738_36_DSP2_IN6_DATA_SELECT     0x36
#define AK7738_37_SRC1_DATA_SELECT         0x37
#define AK7738_38_SRC2_DATA_SELECT         0x38
#define AK7738_39_SRC3_DATA_SELECT         0x39
#define AK7738_3A_SRC4_DATA_SELECT         0x3A
#define AK7738_3B_FSCONV1_DATA_SELECT      0x3B
#define AK7738_3C_FSCONV2_DATA_SELECT      0x3C
#define AK7738_3D_MIXERA_CH1_DATA_SELECT   0x3D
#define AK7738_3E_MIXERA_CH2_DATA_SELECT   0x3E
#define AK7738_3F_MIXERB_CH1_DATA_SELECT   0x3F
#define AK7738_40_MIXERB_CH2_DATA_SELECT   0x40
#define AK7738_41_DIT_DATA_SELECT          0x41
#define AK7738_42_RESERVED                 0x42
#define AK7738_43_RESERVED                 0x43
#define AK7738_44_CLOCKFORMAT_SETTING1     0x44
#define AK7738_45_CLOCKFORMAT_SETTING2     0x45
#define AK7738_46_CLOCKFORMAT_SETTING3     0x46
#define AK7738_47_RESERVED                 0x47
#define AK7738_48_SDIN1_FORMAT             0x48
#define AK7738_49_SDIN2_FORMAT             0x49
#define AK7738_4A_SDIN3_FORMAT             0x4A
#define AK7738_4B_SDIN4_FORMAT             0x4B
#define AK7738_4C_SDIN5_FORMAT             0x4C
#define AK7738_4D_SDIN6_FORMAT             0x4D
#define AK7738_4E_SDOUT1_FORMAT            0x4E
#define AK7738_4F_SDOUT2_FORMAT            0x4F
#define AK7738_50_SDOUT3_FORMAT            0x50
#define AK7738_51_SDOUT4_FORMAT            0x51
#define AK7738_52_SDOUT5_FORMAT            0x52
#define AK7738_53_SDOUT6_FORMAT            0x53
#define AK7738_54_SDOUT_MODE_SETTING       0x54
#define AK7738_55_TDM_MODE_SETTING         0x55
#define AK7738_56_RESERVED                 0x56
#define AK7738_57_OUTPUT_PORT_SELECT       0x57
#define AK7738_58_OUTPUT_PORT_ENABLE       0x58
#define AK7738_59_INPUT_PORT_SELECT        0x59
#define AK7738_5A_RESERVED                 0x5A
#define AK7738_5B_MIXER_A_SETTING          0x5B
#define AK7738_5C_MIXER_B_SETTING          0x5C
#define AK7738_5D_MICAMP_GAIN              0x5D
#define AK7738_5E_MICAMP_GAIN_CONTROL      0x5E
#define AK7738_5F_ADC1_LCH_VOLUME          0x5F
#define AK7738_60_ADC1_RCH_VOLUME          0x60
#define AK7738_61_ADC2_LCH_VOLUME          0x61
#define AK7738_62_ADC2_RCH_VOLUME          0x62
#define AK7738_63_ADCM_VOLUME              0x63

#define AK7738_64_RESERVED                 0x64
#define AK7738_65_RESERVED                 0x65
#define AK7738_66_AIN_FILTER               0x66
#define AK7738_67_ADC_MUTE_HPF             0x67
#define AK7738_68_DAC1_LCH_VOLUME          0x68
#define AK7738_69_DAC1_RCH_VOLUME          0x69
#define AK7738_6A_DAC2_LCH_VOLUME          0x6A
#define AK7738_6B_DAC2_RCH_VOLUME          0x6B

#define AK7738_6C_RESERVED                 0x6C
#define AK7738_6D_RESERVED                 0x6D
#define AK7738_6E_DAC_MUTE_FILTER          0x6E
#define AK7738_6F_SRC_CLOCK_SETTING        0x6F
#define AK7738_70_SRC_MUTE_SETTING         0x70
#define AK7738_71_FSCONV_MUTE_SETTING      0x71
#define AK7738_72_RESERVED                 0x72
#define AK7738_73_DSP_MEMORY_ASSIGNMENT1   0x73
#define AK7738_74_DSP_MEMORY_ASSIGNMENT2   0x74
#define AK7738_75_DSP12_DRAM_SETTING       0x75
#define AK7738_76_DSP1_DLRAM_SETTING       0x76
#define AK7738_77_DSP2_DLRAM_SETTING       0x77
#define AK7738_78_FFT_DLP0_SETTING         0x78
#define AK7738_79_RESERVED                 0x79
#define AK7738_7A_JX_SETTING               0x7A
#define AK7738_7B_STO_FLAG_SETTING1        0x7B
#define AK7738_7C_STO_FLAG_SETTING2        0x7C
#define AK7738_7D_RESERVED                 0x7D
#define AK7738_7E_DIT_STATUS_BIT1          0x7E
#define AK7738_7F_DIT_STATUS_BIT2          0x7F
#define AK7738_80_DIT_STATUS_BIT3          0x80
#define AK7738_81_DIT_STATUS_BIT4          0x81
#define AK7738_82_RESERVED                 0x82
#define AK7738_83_POWER_MANAGEMENT1        0x83
#define AK7738_84_POWER_MANAGEMENT2        0x84
#define AK7738_85_RESET_CTRL               0x85
#define AK7738_86_RESERVED                 0x86
#define AK7738_87_RESERVED                 0x87
#define AK7738_90_RESERVED                 0x90
#define AK7738_91_PAD_DRIVE_SEL2           0x91
#define AK7738_92_PAD_DRIVE_SEL3           0x92
#define AK7738_93_PAD_DRIVE_SEL4           0x93
#define AK7738_94_PAD_DRIVE_SEL5           0x94
#define AK7738_95_RESERVED                 0x95
#define AK7738_100_DEVICE_ID               0x100
#define AK7738_101_REVISION_NUM            0x101
#define AK7738_102_DSP_ERROR_STATUS        0x102
#define AK7738_103_SRC_STATUS              0x103
#define AK7738_104_STO_READ_OUT            0x104
#define AK7738_105_MICGAIN_READ            0x105
#define AK7738_VIRT_106_DSP1OUT1_MIX       0x106
#define AK7738_VIRT_107_DSP1OUT2_MIX       0x107
#define AK7738_VIRT_108_DSP1OUT3_MIX       0x108
#define AK7738_VIRT_109_DSP1OUT4_MIX       0x109
#define AK7738_VIRT_10A_DSP1OUT5_MIX       0x10A
#define AK7738_VIRT_10B_DSP1OUT6_MIX       0x10B
#define AK7738_VIRT_10C_DSP2OUT1_MIX       0x10C
#define AK7738_VIRT_10D_DSP2OUT2_MIX       0x10D
#define AK7738_VIRT_10E_DSP2OUT3_MIX       0x10E
#define AK7738_VIRT_10F_DSP2OUT4_MIX       0x10F
#define AK7738_VIRT_110_DSP2OUT5_MIX       0x110
#define AK7738_VIRT_111_DSP2OUT6_MIX       0x111

#define AK7738_VIRT_112_DSP1_HFP_VOL       0x112
#define AK7738_MAX_REGISTERS (AK7738_VIRT_112_DSP1_HFP_VOL + 1)

#define AK7738_VIRT_REGISTER    AK7738_VIRT_106_DSP1OUT1_MIX

/* Bitfield Definitions */

/* AK7738_C0_CLOCK_SETTING1 (0xC0) Fields */
#define AK7738_FS				0x07
#define AK7738_FS_8KHZ			(0x00 << 0)
#define AK7738_FS_12KHZ			(0x01 << 0)
#define AK7738_FS_16KHZ			(0x02 << 0)
#define AK7738_FS_24KHZ			(0x03 << 0)
#define AK7738_FS_32KHZ			(0x04 << 0)
#define AK7738_FS_48KHZ			(0x05 << 0)
#define AK7738_FS_96KHZ			(0x06 << 0)

#define AK7738_M_S				0x30		//CKM1-0 (CKM2 bit is not use)
#define AK7738_M_S_0			(0 << 4)	//Master, XTI=12.288MHz
#define AK7738_M_S_1			(1 << 4)	//Master, XTI=18.432MHz
#define AK7738_M_S_2			(2 << 4)	//Slave, XTI=12.288MHz
#define AK7738_M_S_3			(3 << 4)	//Slave, BICK

/* AK7738_C2_SERIAL_DATA_FORMAT (0xC2) Fields */
/* LRCK I/F Format */
#define AK7738_LRIF_I2S_MODE		0
#define AK7738_LRIF_MSB_MODE		5
#define AK7738_LRIF_LSB_MODE		5
#define AK7738_LRIF_PCM_SHORT_MODE	6
#define AK7738_LRIF_PCM_LONG_MODE	7
/* Input Format is set as "MSB(24- bit)" by following register.
   CONT03(DIF2,DOF2), CONT06(DIFDA, DIF1), CONT07(DOF4,DOF3,DOF1) */

/* AK7738_CA_CLK_SDOUT_SETTING (0xCA) Fields */
#define AK7738_BICK_LRCK			(3 << 5)	//BICKOE, LRCKOE


#define MAX_LOOP_TIMES		3

#define COMMAND_WRITE_REG			0xC0
#define COMMAND_READ_REG			0x40

#define COMMAND_CRC_READ			0x72
#define COMMAND_MIR1_READ			0x76
#define COMMAND_MIR2_READ			0x77

#define COMMAND_WRITE_CRAM_RUN		0x80
#define COMMAND_WRITE_CRAM_EXEC     0xA4

#define COMMAND_WRITE_CRAM          0xB4
#define COMMAND_WRITE_PRAM          0xB8
#define COMMAND_WRITE_OFREG         0xB2

#define COMMAND_WRITE_CRAM_SUB      0xB6
#define COMMAND_WRITE_PRAM_SUB      0xBA

#define	TOTAL_NUM_OF_PRAM_MAX		40963
#define	TOTAL_NUM_OF_CRAM_MAX		18435
#define	TOTAL_NUM_OF_OFREG_MAX		195
#define	TOTAL_NUM_OF_SUB_PRAM_MAX	5123
#define	TOTAL_NUM_OF_SUB_CRAM_MAX	6147


static const char *ak7738_firmware_pram[] =
{
    "Off",
    "basic1",
    "basic2",
    "dsp1_data1",
    "dsp1_data2",
    "dsp1_data3",
    "dsp1_data4",
    "dsp1_data5",
    "dsp2_data1",
    "dsp2_data2",
    "dsp2_data3",
    "dsp2_data4",
    "dsp2_data5",
    "dsp3_data1",
    "dsp3_data2",
    "dsp3_data3",
    "dsp3_data4",
    "dsp3_data5",
};

static const char *ak7738_firmware_cram[] =
{
    "Off",
    "basic1",
    "basic2",
    "dsp1_data1",
    "dsp1_data2",
    "dsp1_data3",
    "dsp1_data4",
    "dsp1_data5",
    "dsp2_data1",
    "dsp2_data2",
    "dsp2_data3",
    "dsp2_data4",
    "dsp2_data5",
    "dsp3_data1",
    "dsp3_data2",
    "dsp3_data3",
    "dsp3_data4",
    "dsp3_data5",
};

static const char *ak7738_firmware_ofreg[] =
{
    "Off",
    "basic1",
    "basic2",
    "dsp1_data1",
    "dsp1_data2",
    "dsp1_data3",
    "dsp1_data4",
    "dsp1_data5",
    "dsp2_data1",
    "dsp2_data2",
    "dsp2_data3",
    "dsp2_data4",
    "dsp2_data5",
};

enum {
	AIF_PORT1 = 0,
	AIF_PORT2,
	AIF_PORT3,
	AIF_PORT4,
	AIF_PORT5,
	NUM_AIF_DAIS,
};
/*Next section is based on DSP firmware, need modify them by different
 * firmware.*/
// Component: SFADER_2 (fader_mul2_1)
#define AK7738_X9REF_FIRMWARE 1
#ifdef AK7738_X9REF_FIRMWARE
/* DSP1 parameter */
// Component: FADERSCO (fader_mul_1)
#define CRAM_ADDR_LEVEL_FADERSCO 0x0000
#define CRAM_ADDR_ATT_TIME_FADERSCO 0x0001
#define CRAM_ADDR_REL_TIME_FADERSCO 0x0003

// Component: SFADER_1 (fader_mul2_1)
#define CRAM_ADDR_LEVEL_SFADER_1 0x0006
#define CRAM_ADDR_ATT_TIME_SFADER_1 0x0007
#define CRAM_ADDR_REL_TIME_SFADER_1 0x0009

// Component: GEQs_1 (biquad2_1)
#define CRAM_ADDR_BAND1_GEQs_1 0x000C
#define CRAM_ADDR_BAND2_GEQs_1 0x0020
#define CRAM_ADDR_BAND3_GEQs_1 0x0025

// Component: MIXER2_1 (mixerl2_1)
#define CRAM_ADDR_VOL_IN1_MIXER2_1 0x002A
#define CRAM_ADDR_VOL_IN2_MIXER2_1 0x002B

// Component: FADER_1 (fader_mul_2)
#define CRAM_ADDR_LEVEL_FADER_1 0x002C
#define CRAM_ADDR_ATT_TIME_FADER_1 0x002D
#define CRAM_ADDR_REL_TIME_FADER_1 0x002F

// Component: MIXER2_2 (mixerl2_2)
#define CRAM_ADDR_VOL_IN1_MIXER2_2 0x0032
#define CRAM_ADDR_VOL_IN2_MIXER2_2 0x0033

/* DSP2 parameter */
#define CRAM_ADDR_LEVEL_SFADER_2 0x0000
#define CRAM_ADDR_ATT_TIME_SFADER_2 0x0001
#define CRAM_ADDR_REL_TIME_SFADER_2 0x0003

// Component: HPF2_1 (biquad2_1)
#define CRAM_ADDR_BAND1_HPF2_1 0x0006

// Component: SFADER_3 (fader_mul2_2)
#define CRAM_ADDR_LEVEL_SFADER_3 0x0010
#define CRAM_ADDR_ATT_TIME_SFADER_3 0x0011
#define CRAM_ADDR_REL_TIME_SFADER_3 0x0013

// Component: HPF2_2 (biquad2_2)
#define CRAM_ADDR_BAND1_HPF2_2 0x0016

// Component: SWITCH_1 (mixerl2_1)
#define CRAM_ADDR_VOL_IN1_SWITCH_1 0x0020
#define CRAM_ADDR_VOL_IN2_SWITCH_1 0x0021
/* use vol table to avoid use -lm for compiling math lib  */
unsigned char dsp_vol_table[] = {
    0x0,  0x0,	0x4,  /* Vol index 0(mute)*/
    0x0,  0x0,	0x4,  /* Vol index 1(-115.0 dB)*/
    0x0,  0x0,	0x4,  /* Vol index 2(-114.5 dB)*/
    0x0,  0x0,	0x5,  /* Vol index 3(-114.0 dB)*/
    0x0,  0x0,	0x5,  /* Vol index 4(-113.5 dB)*/
    0x0,  0x0,	0x5,  /* Vol index 5(-113.0 dB)*/
    0x0,  0x0,	0x5,  /* Vol index 6(-112.5 dB)*/
    0x0,  0x0,	0x6,  /* Vol index 7(-112.0 dB)*/
    0x0,  0x0,	0x6,  /* Vol index 8(-111.5 dB)*/
    0x0,  0x0,	0x6,  /* Vol index 9(-111.0 dB)*/
    0x0,  0x0,	0x7,  /* Vol index 10(-110.5 dB)*/
    0x0,  0x0,	0x7,  /* Vol index 11(-110.0 dB)*/
    0x0,  0x0,	0x8,  /* Vol index 12(-109.5 dB)*/
    0x0,  0x0,	0x8,  /* Vol index 13(-109.0 dB)*/
    0x0,  0x0,	0x8,  /* Vol index 14(-108.5 dB)*/
    0x0,  0x0,	0x9,  /* Vol index 15(-108.0 dB)*/
    0x0,  0x0,	0x9,  /* Vol index 16(-107.5 dB)*/
    0x0,  0x0,	0xa,  /* Vol index 17(-107.0 dB)*/
    0x0,  0x0,	0xa,  /* Vol index 18(-106.5 dB)*/
    0x0,  0x0,	0xb,  /* Vol index 19(-106.0 dB)*/
    0x0,  0x0,	0xc,  /* Vol index 20(-105.5 dB)*/
    0x0,  0x0,	0xc,  /* Vol index 21(-105.0 dB)*/
    0x0,  0x0,	0xd,  /* Vol index 22(-104.5 dB)*/
    0x0,  0x0,	0xe,  /* Vol index 23(-104.0 dB)*/
    0x0,  0x0,	0xf,  /* Vol index 24(-103.5 dB)*/
    0x0,  0x0,	0xf,  /* Vol index 25(-103.0 dB)*/
    0x0,  0x0,	0x10, /* Vol index 26(-102.5 dB)*/
    0x0,  0x0,	0x11, /* Vol index 27(-102.0 dB)*/
    0x0,  0x0,	0x12, /* Vol index 28(-101.5 dB)*/
    0x0,  0x0,	0x13, /* Vol index 29(-101.0 dB)*/
    0x0,  0x0,	0x14, /* Vol index 30(-100.5 dB)*/
    0x0,  0x0,	0x15, /* Vol index 31(-100.0 dB)*/
    0x0,  0x0,	0x17, /* Vol index 32(-99.5 dB)*/
    0x0,  0x0,	0x18, /* Vol index 33(-99.0 dB)*/
    0x0,  0x0,	0x19, /* Vol index 34(-98.5 dB)*/
    0x0,  0x0,	0x1b, /* Vol index 35(-98.0 dB)*/
    0x0,  0x0,	0x1c, /* Vol index 36(-97.5 dB)*/
    0x0,  0x0,	0x1e, /* Vol index 37(-97.0 dB)*/
    0x0,  0x0,	0x20, /* Vol index 38(-96.5 dB)*/
    0x0,  0x0,	0x22, /* Vol index 39(-96.0 dB)*/
    0x0,  0x0,	0x24, /* Vol index 40(-95.5 dB)*/
    0x0,  0x0,	0x26, /* Vol index 41(-95.0 dB)*/
    0x0,  0x0,	0x28, /* Vol index 42(-94.5 dB)*/
    0x0,  0x0,	0x2a, /* Vol index 43(-94.0 dB)*/
    0x0,  0x0,	0x2d, /* Vol index 44(-93.5 dB)*/
    0x0,  0x0,	0x2f, /* Vol index 45(-93.0 dB)*/
    0x0,  0x0,	0x32, /* Vol index 46(-92.5 dB)*/
    0x0,  0x0,	0x35, /* Vol index 47(-92.0 dB)*/
    0x0,  0x0,	0x38, /* Vol index 48(-91.5 dB)*/
    0x0,  0x0,	0x3c, /* Vol index 49(-91.0 dB)*/
    0x0,  0x0,	0x3f, /* Vol index 50(-90.5 dB)*/
    0x0,  0x0,	0x43, /* Vol index 51(-90.0 dB)*/
    0x0,  0x0,	0x47, /* Vol index 52(-89.5 dB)*/
    0x0,  0x0,	0x4b, /* Vol index 53(-89.0 dB)*/
    0x0,  0x0,	0x4f, /* Vol index 54(-88.5 dB)*/
    0x0,  0x0,	0x54, /* Vol index 55(-88.0 dB)*/
    0x0,  0x0,	0x59, /* Vol index 56(-87.5 dB)*/
    0x0,  0x0,	0x5e, /* Vol index 57(-87.0 dB)*/
    0x0,  0x0,	0x64, /* Vol index 58(-86.5 dB)*/
    0x0,  0x0,	0x6a, /* Vol index 59(-86.0 dB)*/
    0x0,  0x0,	0x70, /* Vol index 60(-85.5 dB)*/
    0x0,  0x0,	0x76, /* Vol index 61(-85.0 dB)*/
    0x0,  0x0,	0x7d, /* Vol index 62(-84.5 dB)*/
    0x0,  0x0,	0x85, /* Vol index 63(-84.0 dB)*/
    0x0,  0x0,	0x8d, /* Vol index 64(-83.5 dB)*/
    0x0,  0x0,	0x95, /* Vol index 65(-83.0 dB)*/
    0x0,  0x0,	0x9e, /* Vol index 66(-82.5 dB)*/
    0x0,  0x0,	0xa7, /* Vol index 67(-82.0 dB)*/
    0x0,  0x0,	0xb1, /* Vol index 68(-81.5 dB)*/
    0x0,  0x0,	0xbb, /* Vol index 69(-81.0 dB)*/
    0x0,  0x0,	0xc6, /* Vol index 70(-80.5 dB)*/
    0x0,  0x0,	0xd2, /* Vol index 71(-80.0 dB)*/
    0x0,  0x0,	0xdf, /* Vol index 72(-79.5 dB)*/
    0x0,  0x0,	0xec, /* Vol index 73(-79.0 dB)*/
    0x0,  0x0,	0xfa, /* Vol index 74(-78.5 dB)*/
    0x0,  0x1,	0x9,  /* Vol index 75(-78.0 dB)*/
    0x0,  0x1,	0x18, /* Vol index 76(-77.5 dB)*/
    0x0,  0x1,	0x29, /* Vol index 77(-77.0 dB)*/
    0x0,  0x1,	0x3a, /* Vol index 78(-76.5 dB)*/
    0x0,  0x1,	0x4d, /* Vol index 79(-76.0 dB)*/
    0x0,  0x1,	0x61, /* Vol index 80(-75.5 dB)*/
    0x0,  0x1,	0x75, /* Vol index 81(-75.0 dB)*/
    0x0,  0x1,	0x8c, /* Vol index 82(-74.5 dB)*/
    0x0,  0x1,	0xa3, /* Vol index 83(-74.0 dB)*/
    0x0,  0x1,	0xbc, /* Vol index 84(-73.5 dB)*/
    0x0,  0x1,	0xd6, /* Vol index 85(-73.0 dB)*/
    0x0,  0x1,	0xf2, /* Vol index 86(-72.5 dB)*/
    0x0,  0x2,	0xf,  /* Vol index 87(-72.0 dB)*/
    0x0,  0x2,	0x2e, /* Vol index 88(-71.5 dB)*/
    0x0,  0x2,	0x50, /* Vol index 89(-71.0 dB)*/
    0x0,  0x2,	0x73, /* Vol index 90(-70.5 dB)*/
    0x0,  0x2,	0x98, /* Vol index 91(-70.0 dB)*/
    0x0,  0x2,	0xbf, /* Vol index 92(-69.5 dB)*/
    0x0,  0x2,	0xe9, /* Vol index 93(-69.0 dB)*/
    0x0,  0x3,	0x15, /* Vol index 94(-68.5 dB)*/
    0x0,  0x3,	0x43, /* Vol index 95(-68.0 dB)*/
    0x0,  0x3,	0x75, /* Vol index 96(-67.5 dB)*/
    0x0,  0x3,	0xa9, /* Vol index 97(-67.0 dB)*/
    0x0,  0x3,	0xe1, /* Vol index 98(-66.5 dB)*/
    0x0,  0x4,	0x1c, /* Vol index 99(-66.0 dB)*/
    0x0,  0x4,	0x5a, /* Vol index 100(-65.5 dB)*/
    0x0,  0x4,	0x9c, /* Vol index 101(-65.0 dB)*/
    0x0,  0x4,	0xe2, /* Vol index 102(-64.5 dB)*/
    0x0,  0x5,	0x2c, /* Vol index 103(-64.0 dB)*/
    0x0,  0x5,	0x7a, /* Vol index 104(-63.5 dB)*/
    0x0,  0x5,	0xcd, /* Vol index 105(-63.0 dB)*/
    0x0,  0x6,	0x25, /* Vol index 106(-62.5 dB)*/
    0x0,  0x6,	0x82, /* Vol index 107(-62.0 dB)*/
    0x0,  0x6,	0xe5, /* Vol index 108(-61.5 dB)*/
    0x0,  0x7,	0x4e, /* Vol index 109(-61.0 dB)*/
    0x0,  0x7,	0xbc, /* Vol index 110(-60.5 dB)*/
    0x0,  0x8,	0x32, /* Vol index 111(-60.0 dB)*/
    0x0,  0x8,	0xae, /* Vol index 112(-59.5 dB)*/
    0x0,  0x9,	0x32, /* Vol index 113(-59.0 dB)*/
    0x0,  0x9,	0xbd, /* Vol index 114(-58.5 dB)*/
    0x0,  0xa,	0x51, /* Vol index 115(-58.0 dB)*/
    0x0,  0xa,	0xed, /* Vol index 116(-57.5 dB)*/
    0x0,  0xb,	0x93, /* Vol index 117(-57.0 dB)*/
    0x0,  0xc,	0x42, /* Vol index 118(-56.5 dB)*/
    0x0,  0xc,	0xfc, /* Vol index 119(-56.0 dB)*/
    0x0,  0xd,	0xc1, /* Vol index 120(-55.5 dB)*/
    0x0,  0xe,	0x92, /* Vol index 121(-55.0 dB)*/
    0x0,  0xf,	0x6f, /* Vol index 122(-54.5 dB)*/
    0x0,  0x10, 0x59, /* Vol index 123(-54.0 dB)*/
    0x0,  0x11, 0x51, /* Vol index 124(-53.5 dB)*/
    0x0,  0x12, 0x57, /* Vol index 125(-53.0 dB)*/
    0x0,  0x13, 0x6e, /* Vol index 126(-52.5 dB)*/
    0x0,  0x14, 0x94, /* Vol index 127(-52.0 dB)*/
    0x0,  0x15, 0xcc, /* Vol index 128(-51.5 dB)*/
    0x0,  0x17, 0x17, /* Vol index 129(-51.0 dB)*/
    0x0,  0x18, 0x75, /* Vol index 130(-50.5 dB)*/
    0x0,  0x19, 0xe8, /* Vol index 131(-50.0 dB)*/
    0x0,  0x1b, 0x71, /* Vol index 132(-49.5 dB)*/
    0x0,  0x1d, 0x11, /* Vol index 133(-49.0 dB)*/
    0x0,  0x1e, 0xca, /* Vol index 134(-48.5 dB)*/
    0x0,  0x20, 0x9d, /* Vol index 135(-48.0 dB)*/
    0x0,  0x22, 0x8c, /* Vol index 136(-47.5 dB)*/
    0x0,  0x24, 0x98, /* Vol index 137(-47.0 dB)*/
    0x0,  0x26, 0xc3, /* Vol index 138(-46.5 dB)*/
    0x0,  0x29, 0xf,  /* Vol index 139(-46.0 dB)*/
    0x0,  0x2b, 0x7e, /* Vol index 140(-45.5 dB)*/
    0x0,  0x2e, 0x12, /* Vol index 141(-45.0 dB)*/
    0x0,  0x30, 0xcc, /* Vol index 142(-44.5 dB)*/
    0x0,  0x33, 0xb1, /* Vol index 143(-44.0 dB)*/
    0x0,  0x36, 0xc1, /* Vol index 144(-43.5 dB)*/
    0x0,  0x39, 0xff, /* Vol index 145(-43.0 dB)*/
    0x0,  0x3d, 0x6f, /* Vol index 146(-42.5 dB)*/
    0x0,  0x41, 0x13, /* Vol index 147(-42.0 dB)*/
    0x0,  0x44, 0xee, /* Vol index 148(-41.5 dB)*/
    0x0,  0x49, 0x3,  /* Vol index 149(-41.0 dB)*/
    0x0,  0x4d, 0x57, /* Vol index 150(-40.5 dB)*/
    0x0,  0x51, 0xec, /* Vol index 151(-40.0 dB)*/
    0x0,  0x56, 0xc7, /* Vol index 152(-39.5 dB)*/
    0x0,  0x5b, 0xeb, /* Vol index 153(-39.0 dB)*/
    0x0,  0x61, 0x5d, /* Vol index 154(-38.5 dB)*/
    0x0,  0x67, 0x22, /* Vol index 155(-38.0 dB)*/
    0x0,  0x6d, 0x3e, /* Vol index 156(-37.5 dB)*/
    0x0,  0x73, 0xb8, /* Vol index 157(-37.0 dB)*/
    0x0,  0x7a, 0x93, /* Vol index 158(-36.5 dB)*/
    0x0,  0x81, 0xd6, /* Vol index 159(-36.0 dB)*/
    0x0,  0x89, 0x88, /* Vol index 160(-35.5 dB)*/
    0x0,  0x91, 0xae, /* Vol index 161(-35.0 dB)*/
    0x0,  0x9a, 0x4f, /* Vol index 162(-34.5 dB)*/
    0x0,  0xa3, 0x74, /* Vol index 163(-34.0 dB)*/
    0x0,  0xad, 0x24, /* Vol index 164(-33.5 dB)*/
    0x0,  0xb7, 0x66, /* Vol index 165(-33.0 dB)*/
    0x0,  0xc2, 0x44, /* Vol index 166(-32.5 dB)*/
    0x0,  0xcd, 0xc7, /* Vol index 167(-32.0 dB)*/
    0x0,  0xd9, 0xf8, /* Vol index 168(-31.5 dB)*/
    0x0,  0xe6, 0xe2, /* Vol index 169(-31.0 dB)*/
    0x0,  0xf4, 0x91, /* Vol index 170(-30.5 dB)*/
    0x1,  0x3,	0xe,  /* Vol index 171(-30.0 dB)*/
    0x1,  0x12, 0x68, /* Vol index 172(-29.5 dB)*/
    0x1,  0x22, 0xaa, /* Vol index 173(-29.0 dB)*/
    0x1,  0x33, 0xe3, /* Vol index 174(-28.5 dB)*/
    0x1,  0x46, 0x22, /* Vol index 175(-28.0 dB)*/
    0x1,  0x59, 0x75, /* Vol index 176(-27.5 dB)*/
    0x1,  0x6d, 0xed, /* Vol index 177(-27.0 dB)*/
    0x1,  0x83, 0x9c, /* Vol index 178(-26.5 dB)*/
    0x1,  0x9a, 0x93, /* Vol index 179(-26.0 dB)*/
    0x1,  0xb2, 0xe7, /* Vol index 180(-25.5 dB)*/
    0x1,  0xcc, 0xac, /* Vol index 181(-25.0 dB)*/
    0x1,  0xe7, 0xf8, /* Vol index 182(-24.5 dB)*/
    0x2,  0x4,	0xe2, /* Vol index 183(-24.0 dB)*/
    0x2,  0x23, 0x82, /* Vol index 184(-23.5 dB)*/
    0x2,  0x43, 0xf3, /* Vol index 185(-23.0 dB)*/
    0x2,  0x66, 0x51, /* Vol index 186(-22.5 dB)*/
    0x2,  0x8a, 0xb7, /* Vol index 187(-22.0 dB)*/
    0x2,  0xb1, 0x46, /* Vol index 188(-21.5 dB)*/
    0x2,  0xda, 0x1d, /* Vol index 189(-21.0 dB)*/
    0x3,  0x5,	0x60, /* Vol index 190(-20.5 dB)*/
    0x3,  0x33, 0x34, /* Vol index 191(-20.0 dB)*/
    0x3,  0x63, 0xbe, /* Vol index 192(-19.5 dB)*/
    0x3,  0x97, 0x29, /* Vol index 193(-19.0 dB)*/
    0x3,  0xcd, 0x9f, /* Vol index 194(-18.5 dB)*/
    0x4,  0x7,	0x50, /* Vol index 195(-18.0 dB)*/
    0x4,  0x44, 0x6c, /* Vol index 196(-17.5 dB)*/
    0x4,  0x85, 0x27, /* Vol index 197(-17.0 dB)*/
    0x4,  0xc9, 0xb8, /* Vol index 198(-16.5 dB)*/
    0x5,  0x12, 0x59, /* Vol index 199(-16.0 dB)*/
    0x5,  0x5f, 0x47, /* Vol index 200(-15.5 dB)*/
    0x5,  0xb0, 0xc5, /* Vol index 201(-15.0 dB)*/
    0x6,  0x7,	0x16, /* Vol index 202(-14.5 dB)*/
    0x6,  0x62, 0x85, /* Vol index 203(-14.0 dB)*/
    0x6,  0xc3, 0x5f, /* Vol index 204(-13.5 dB)*/
    0x7,  0x29, 0xf6, /* Vol index 205(-13.0 dB)*/
    0x7,  0x96, 0xa2, /* Vol index 206(-12.5 dB)*/
    0x8,  0x9,	0xbd, /* Vol index 207(-12.0 dB)*/
    0x8,  0x83, 0xab, /* Vol index 208(-11.5 dB)*/
    0x9,  0x4,	0xd2, /* Vol index 209(-11.0 dB)*/
    0x9,  0x8d, 0xa1, /* Vol index 210(-10.5 dB)*/
    0xa,  0x1e, 0x8a, /* Vol index 211(-10.0 dB)*/
    0xa,  0xb8, 0xa,  /* Vol index 212(-9.5 dB)*/
    0xb,  0x5a, 0xa2, /* Vol index 213(-9.0 dB)*/
    0xc,  0x6,	0xdd, /* Vol index 214(-8.5 dB)*/
    0xc,  0xbd, 0x4c, /* Vol index 215(-8.0 dB)*/
    0xd,  0x7e, 0x8a, /* Vol index 216(-7.5 dB)*/
    0xe,  0x4b, 0x3c, /* Vol index 217(-7.0 dB)*/
    0xf,  0x24, 0xf,  /* Vol index 218(-6.5 dB)*/
    0x10, 0x9,	0xba, /* Vol index 219(-6.0 dB)*/
    0x10, 0xfd, 0x2,  /* Vol index 220(-5.5 dB)*/
    0x11, 0xfe, 0xb4, /* Vol index 221(-5.0 dB)*/
    0x13, 0xf,	0xaa, /* Vol index 222(-4.5 dB)*/
    0x14, 0x30, 0xce, /* Vol index 223(-4.0 dB)*/
    0x15, 0x63, 0x13, /* Vol index 224(-3.5 dB)*/
    0x16, 0xa7, 0x7e, /* Vol index 225(-3.0 dB)*/
    0x17, 0xff, 0x23, /* Vol index 226(-2.5 dB)*/
    0x19, 0x6b, 0x23, /* Vol index 227(-2.0 dB)*/
    0x1a, 0xec, 0xb6, /* Vol index 228(-1.5 dB)*/
    0x1c, 0x85, 0x21, /* Vol index 229(-1.0 dB)*/
    0x1e, 0x35, 0xc0, /* Vol index 230(-0.5 dB)*/
    0x20, 0x0,	0x0,  /* Vol index 231(0.0 dB)*/
    0x21, 0xe5, 0x68, /* Vol index 232(0.5 dB)*/
    0x23, 0xe7, 0x94, /* Vol index 233(1.0 dB)*/
    0x26, 0x8,	0x36, /* Vol index 234(1.5 dB)*/
    0x28, 0x49, 0x1e, /* Vol index 235(2.0 dB)*/
    0x2a, 0xac, 0x35, /* Vol index 236(2.5 dB)*/
    0x2d, 0x33, 0x82, /* Vol index 237(3.0 dB)*/
    0x2f, 0xe1, 0x2a, /* Vol index 238(3.5 dB)*/
    0x32, 0xb7, 0x72, /* Vol index 239(4.0 dB)*/
    0x35, 0xb8, 0xc3, /* Vol index 240(4.5 dB)*/
    0x38, 0xe7, 0xaa, /* Vol index 241(5.0 dB)*/
    0x3c, 0x46, 0xdb, /* Vol index 242(5.5 dB)*/
    0x3f, 0xd9, 0x31, /* Vol index 243(6.0 dB)*/
    0x43, 0xa1, 0xb4, /* Vol index 244(6.5 dB)*/
    0x47, 0xa3, 0x9b, /* Vol index 245(7.0 dB)*/
    0x4b, 0xe2, 0x4b, /* Vol index 246(7.5 dB)*/
    0x50, 0x61, 0x60, /* Vol index 247(8.0 dB)*/
    0x55, 0x24, 0xa9, /* Vol index 248(8.5 dB)*/
    0x5a, 0x30, 0x32, /* Vol index 249(9.0 dB)*/
    0x5f, 0x88, 0x41, /* Vol index 250(9.5 dB)*/
    0x65, 0x31, 0x61, /* Vol index 251(10.0 dB)*/
    0x6b, 0x30, 0x5e, /* Vol index 252(10.5 dB)*/
    0x71, 0x8a, 0x50, /* Vol index 253(11.0 dB)*/
    0x78, 0x44, 0x9a, /* Vol index 254(11.5 dB)*/
    0x7f, 0x64, 0xf0, /* Vol index 255(12.0 dB)*/
};

#endif
#endif

