/*
 * ALSA SoC Texas Instruments TAS5404 Quad-Channel Audio Amplifier
 *
 */

#ifndef __TAS5404_H__
#define __TAS5404_H__

/* Register Address Map */
#define TAS5404_FAULT_REG1		0x00 /* Latched fault register 1, global and channel fault */
#define TAS5404_FAULT_OTW			BIT(0) /* Overtemperature warning has occurred. */
#define TAS5404_FAULT_DC_OFFSET		BIT(1) /* DC offset has occurred in any channel. */
#define TAS5404_FAULT_OCSD			BIT(2) /* Overcurrent shutdown has occurred in any channel.*/
#define TAS5404_FAULT_OTSD			BIT(3) /* Overtemperature shutdown has occurred. */
#define TAS5404_FAULT_CP_UV			BIT(4) /* Charge-pump undervoltage has occurred. */
#define TAS5404_FAULT_AVDD_UV		BIT(5) /* AVDD, analog voltage, undervoltage has occurred. */
#define TAS5404_FAULT_PVDD_UV		BIT(6) /* PVDD undervoltage has occurred.*/
#define TAS5404_FAULT_AVDD_OV		BIT(7) /* PVDD overvoltage has occurred. */

#define TAS5404_FAULT_REG2		0x01 /* dc offset and overcurrent detect */
#define TAS5404_FAULT_OTSD_CH1		BIT(0) /* Overcurrent shutdown channel 1 has occurred. */
#define TAS5404_FAULT_OTSD_CH2		BIT(1) /* Overcurrent shutdown channel 2 has occurred. */
#define TAS5404_FAULT_OTSD_CH3		BIT(2) /* Overcurrent shutdown channel 3 has occurred.*/
#define TAS5404_FAULT_OTSD_CH4		BIT(3) /* Overcurrent shutdown channel 4 has occurred.*/
#define TAS5404_FAULT_DC_OFFSET_CH1	BIT(4) /* DC offset channel 1 has occurred. */
#define TAS5404_FAULT_DC_OFFSET_CH2	BIT(5) /* DC offset channel 2 has occurred. */
#define TAS5404_FAULT_DC_OFFSET_CH3	BIT(6) /* DC offset channel 3 has occurred. */
#define TAS5404_FAULT_DC_OFFSET_CH4	BIT(7) /* DC offset channel 4 has occurred. */

#define TAS5404_DIAG_REG1		0x02 /* load diagnostics */
#define TAS5404_DIAG_REG2		0x03 /* load diagnostics */
#define TAS5404_STAT_REG1		0x04 /* temperature and voltage detect */
#define TAS5404_STAT_REG2		0x05 /* Hi-Z and low-low state */
#define TAS5404_STAT_REG3		0x06 /* mute and play modes */
#define TAS5404_STAT_REG4		0x07 /* load diagnostics */
#define TAS5404_CTRL1			0x08 /* channel gain select */
#define TAS5404_CTRL2			0x09 /* overcurrent control */
#define TAS5404_CTRL3			0x0a /* switching frequency and clip pin select */
#define TAS5404_CTRL4			0x0b /* load diagnostic, master mode select */
#define TAS5404_CTRL5		 	0x0c /* output state control */
#define TAS5404_CTRL5_ALL_CH_HIZ 	0xF
#define TAS5404_CTRL5_UNMUTE_CH1 0	/*0: Set channel 1 to mute mode, non-Hi-Z*/
#define TAS5404_CTRL5_UNMUTE_CH1_BIT BIT(0)
#define TAS5404_CTRL5_UNMUTE_CH2 1	/*0: Set channel 2 to mute mode, non-Hi-Z*/
#define TAS5404_CTRL5_UNMUTE_CH2_BIT BIT(1)
#define TAS5404_CTRL5_UNMUTE_CH3 2	/*0: Set channel 3 to mute mode, non-Hi-Z*/
#define TAS5404_CTRL5_UNMUTE_CH3_BIT BIT(2)
#define TAS5404_CTRL5_UNMUTE_CH4 3  /*0: Set channel 4 to mute mode, non-Hi-Z*/
#define TAS5404_CTRL5_UNMUTE_CH4_BIT BIT(3)
#define TAS5404_CTRL5_ALL_CH_BIT                                               \
	(TAS5404_CTRL5_UNMUTE_CH1_BIT | TAS5404_CTRL5_UNMUTE_CH2_BIT |         \
	 TAS5404_CTRL5_UNMUTE_CH3_BIT | TAS5404_CTRL5_UNMUTE_CH4_BIT)

#define TAS5404_CTRL5_UNMUTE_OFFSET 4
#define TAS5404_CTRL5_UNMUTE_BIT BIT(4)
#define TAS5404_CTRL5_DC_SD_OFFSET  5
#define TAS5404_CTRL5_DC_SD_OFFSET_BIT BIT(5)
#define TAS5404_CTRL5_RESET_OFFSET  7
#define TAS5404_CTRL5_RESET_BIT BIT(7)

#define TAS5404_CTRL6			0x0d /* output state control */
#define TAS5404_CTRL6_ALL_CH_LOW_LOW 0xF
#define TAS5404_CTRL6_ALL_CH_LOW_LOW_DISABLED 0x0
#define TAS5404_CTRL6_LOW_LOW_CH1 0 /*0: Set channel 1 to low-low mode*/
#define TAS5404_CTRL6_LOW_LOW_CH1_BIT BIT(0)
#define TAS5404_CTRL6_LOW_LOW_CH2 1 /*0: Set channel 2 to low-low mode*/
#define TAS5404_CTRL6_LOW_LOW_CH2_BIT BIT(1)
#define TAS5404_CTRL6_LOW_LOW_CH3 2 /*0: Set channel 3 to low-low mode*/
#define TAS5404_CTRL6_LOW_LOW_CH3_BIT BIT(2)
#define TAS5404_CTRL6_LOW_LOW_CH4 3 /*0: Set channel 4 to low-low mode*/
#define TAS5404_CTRL6_LOW_LOW_CH4_BIT BIT(3)
#define TAS5404_CTRL6_ALL_CH_BIT                                               \
	(TAS5404_CTRL6_LOW_LOW_CH1_BIT | TAS5404_CTRL6_LOW_LOW_CH2_BIT |       \
	 TAS5404_CTRL6_LOW_LOW_CH3_BIT | TAS5404_CTRL6_LOW_LOW_CH4_BIT)

#define TAS5404_CTRL7			0x10 /* dc offset detect threshold selection */
#define TAS5404_STAT_REG5		0x13 /* overtemperature shutdown and thermal foldback */
#define TAS5404_MAX			TAS5404_STAT_REG5

#endif /* __TAS5404_H__ */