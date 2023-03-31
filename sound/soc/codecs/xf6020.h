/**
 * @file xf6020.h
 * @author shao yi (yi.shao@semidrive.com)
 * @brief xf6020 module dsp driver header file.
 *        i2c address is 0x47.
 * @version 0.1
 * @date 2020-08-11
 *
 * @copyright Copyright (c) 2020 semdrive
 *
 */
#ifndef __DSP_XF6020_H__
#define __DSP_XF6020_H__
/**
 * @brief xf6020's sample rate
 *
 */
#define XF6020_RATES (SNDRV_PCM_RATE_48000)
/**
 * @brief xf6020 support format
 *
 */
#define XF6020_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE| \
			 SNDRV_PCM_FMTBIT_S32_LE)

/**
 * @brief xf6020 register definition
 *
 */
#define XF6020_REG_VOICE_MODE 0x00  /*!< @link Xf6020VoiceMode */
#define XF6020_REG_PARAM 0x01		/*!< @link XF6020ParamValue */
#define XF6020_REG_BYPASS 0x02 		/*!< 0: off 1:on*/
#define XF6020_REG_VERSION 0x0c 	/* Four bits result*/

/* next definition is used in previous version*/
//#define XF6020_CMD_READ  0xe1
//#define XF6020_CMD_WRITE 0xe2
//#define XF6020_CMD_GET_VESION  0xe3
//#define XF6020_CMD_BYPASS 0xe4
//#define XF6020_GET_CMD_END 0xee
//#define XF6020_CMD_END 0xef

//#define XF6020_REG_CMD 0xfc
//#define XF6020_REG_START 0xfd
/**
 * @brief voice mode definition.
 *
 */
typedef enum  {
		/*! four sound areas*/
		FOUR_SND_AREAS = 0,
		/*! two sound areas*/
		TWO_SND_AREAS =1,
}XF6020VoiceMode;

#define XF6020_REG_PARAM 0x01
/**
 * @brief xf6020's parameters
 *
 */
typedef enum {
	AEC_TEL = 0, /*!< telecom mode*/
	AEC_MUSIC = 1,
	USE_BEAM_0 = 2,
	USE_BEAM_1 = 3,
	USE_BEAM_2 = 4,
	USE_BEAM_3 = 5,
	USE_BEAM_OFF = 6,

	WAKEUP_TWO = 7,
	WAKEUP_FOUR = 8,
	WAKEUP_OFF = 9,

	REC_BEAM = 10,
	REC_MAE = 11,
	REC_OFF = 12,

	DUAL_MAE = 13,
	DUAL_MAB = 14,
	DUAL_MAE_MICL = 15,
	DUAL_MAE_MICR = 16,
	DUAL_MAE_REFL = 17,
	DUAL_MAE_REFR = 18,
	DUAL_MAE_REFMIX =19,
	DUAL_PHONE =20,
	//BYPASS_OFF,//13
	//BYPASS_ON,//14
	//AEC_OFF,
	//AEC_REF_SIGLE,
	//AEC_REF_DOUBLE,
	PARAME_VALUE_MAX
}XF6020ParamValue;

#endif