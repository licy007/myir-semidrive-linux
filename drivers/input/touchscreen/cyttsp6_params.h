//*****************************************************************************
//*****************************************************************************
//  FILENAME: Driver.h
//  TrueTouch Host Emulator Version Information: 3.3, b943
//  TrueTouch Firmware Version Information: 1.0.843908
//
//  DESCRIPTION: This file contains configuration values.
//-----------------------------------------------------------------------------
//  Copyright (c) Cypress Semiconductor 2009 - 2016. All Rights Reserved.
//*****************************************************************************
//*****************************************************************************
//-----------------------------------------------------------------------------
/* Touchscreen Version Information */
static u8 ttconfig_fw_ver[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0xE0, 0x84, 0x16, 0x1E, 0x11, 0xA6, 0x00, 0x00
};

/* Touchscreen Parameters Endianess (Endianess: 0:Little; 1:Big)*/
static const uint8_t cyttsp6_param_endianess = 0;

/* Touchscreen Parameters */
static const uint8_t cyttsp6_param_regs[] = {
/*	Value	Name	*/
	0x0C, 0x05,  /* CONFIG_DATA_SIZE */
	0x0C, 0x05,  /* CONFIG_DATA_MAX_SIZE */
	0xC6, 0x03,  /* CROSS_NUM */
	0x2A,  /* TX_NUM */
	0x17,  /* RX_NUM */
	0x41,  /* SENS_NUM */
	0x00,  /* BUTTON_NUM */
	0x01,  /* SLOTS_MUT */
	0x01,  /* SLOTS_SELF_RX */
	0x01,  /* SLOTS_SELF_TX */
	0x02,  /* SLOTS_SELF */
	0x00,  /* PROX_SCAN_AXIS */
	0x02,  /* SCANNING_MODE_BUTTON */
	0x01,  /* SELF_Z_MODE */
	0x01,  /* WATER_REJ_ENABLE */
	0x00,  /* WF_ENABLE */
	0x00,  /* CHARGER_ARMOR_ENABLE */
	0xE8, 0x03, 0x00, 0x00,  /* CA_REVERT_TIME_MS */
	0x88, 0x13, 0x00, 0x00,  /* CA_TOUCH_REVERT_TIME_MS */
	0x00, 0x00, 0x00, 0x00,  /* CA_TOUCH_REVERT_SIG_SUM_THRESH */
	0x00,  /* CA_HOST_CTRL */
	0x00,  /* CHARGER_STATUS */
	0x03,  /* CA_TRIG_SRC */
	0x01,  /* WB_CMF_ENABLE */
	0x0F,  /* WB_REVERT_THRESH */
	0x02,  /* AFH_HOP_CYCLES_COUNT */
	0x06,  /* NMI_SCAN_CNT */
	0x00,  /* Reserved39 */
	0x0A, 0x00, 0x0A, 0x00,  /* CA_NMF_LIMIT */
	0xB4, 0x00,  /* NMI_TCH_MAGNITUDE */
	0x5A, 0x00,  /* NMI_TOUCH_THRESH */
	0x0F, 0x00,  /* NMI_THRESH */
	0x0F, 0x00,  /* WB_THRESH */
	0x00, 0x80,  /* SC_TRIG_THRESH */
	0x32, 0x00,  /* CA_DYN_CAL_SAFE_RAW_RANGE */
	0x4B,  /* CA_DYN_CAL_NUM_SENSOR_THLD_PERCENT */
	0x00,  /* Reserved57 */
	0x0A,  /* NMF_DETECT_THRESH */
	0x02,  /* NM_WB_SCAN_COUNT */
	0x00,  /* Reserved60 */
	0x00,  /* Reserved61 */
	0x64, 0x00,  /* MAX_MUTUAL_SCAN_INTERVAL */
	0x64, 0x00,  /* MAX_SELF_SCAN_INTERVAL */
	0x00, 0x00,  /* Reserved66 */
	0xFF, 0xFF, 0x7F, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x80, 0xFF, 
	0xFF, 0x01, 0xF8, 0xFF, 
	0xFF, 0x07, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0x01, 0xF8, 0xFF, 
	0xFF, 0x07, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00,  /* CDC_SENSOR_MASKS */
	0x16, 0x15, 0x14, 0x13, 
	0x12, 0x11, 0x10, 0x0F, 
	0x0E, 0x0D, 0x0C, 0x0B, 
	0x0A, 0x09, 0x08, 0x07, 
	0x06, 0x05, 0x04, 0x03, 
	0x02, 0x01, 0x00, 0x17, 
	0x18, 0x19, 0x1A, 0x1B, 
	0x1C, 0x1D, 0x1E, 0x1F, 
	0x20, 0x21, 0x22, 0x23, 
	0x24, 0x25, 0x26, 0x27, 
	0x28, 0x33, 0x34, 0x35, 
	0x36, 0x37, 0x38, 0x39, 
	0x3A, 0x3B, 0x3C, 0x3D, 
	0x3E, 0x3F, 0x40, 0x41, 
	0x42, 0x43, 0x44, 0x45, 
	0x46, 0x47, 0x48, 0x49, 
	0x4A, 0x57, 0x48, 0x55, 
	0x00, 0x53, 0x02, 0x51, 
	0x04, 0x4F, 0x06, 0x4D, 
	0x08, 0x4B, 0x0A, 0x49, 
	0x0C, 0x47, 0x0E, 0x44, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00,  /* CDC_PIN_INDEX_TABLE */
	0x16, 0x15, 0x14, 0x13, 
	0x12, 0x11, 0x10, 0x0F, 
	0x0E, 0x0D, 0x0C, 0x0B, 
	0x0A, 0x09, 0x08, 0x07, 
	0x06, 0x05, 0x04, 0x03, 
	0x02, 0x01, 0x00, 0x17, 
	0x18, 0x19, 0x1A, 0x1B, 
	0x1C, 0x1D, 0x1E, 0x1F, 
	0x20, 0x21, 0x22, 0x23, 
	0x24, 0x25, 0x26, 0x27, 
	0x28, 0x29, 0x2A, 0x2B, 
	0x2C, 0x2D, 0x2E, 0x2F, 
	0x30, 0x31, 0x32, 0x33, 
	0x34, 0x35, 0x36, 0x37, 
	0x38, 0x39, 0x3A, 0x3B, 
	0x3C, 0x3D, 0x3E, 0x3F, 
	0x40, 0x57, 0x48, 0x55, 
	0x00, 0x53, 0x02, 0x51, 
	0x04, 0x4F, 0x06, 0x4D, 
	0x08, 0x4B, 0x0A, 0x49, 
	0x0C, 0x47, 0x0E, 0x44, 
	0x00, 0x00, 0x00, 0x00, 
	0x01, 0x00, 0x00, 0x00, 
	0x00,  /* CDC_REAL_PIN_INDEX_TABLE */
	0x00, 0x00, 0x00,  /* Reserved301 */
	0xB7, 0x01, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00,  /* CDC_MTX_PHASE_VECTOR */
	0x9A, 0x11, 0xCD, 0x04, 
	0x66, 0x16, 0x33, 0xFB, 
	0x9A, 0x11, 0xCD, 0x0C, 
	0x66, 0xFE, 0x33, 0x0B, 
	0x9A, 0x09, 0xCD, 0xF4, 
	0x66, 0xFE, 0x33, 0xF3, 
	0x9A, 0x11, 0xCD, 0x04, 
	0x66, 0x16, 0x33, 0xFB, 
	0x9A, 0x11, 0xCD, 0x0C, 
	0x66, 0xFE, 0x33, 0x0B, 
	0x9A, 0x09, 0xCD, 0xF4, 
	0x66, 0xFE, 0x33, 0xF3, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00,  /* CDC_MTX_DECONV_COEF */
	0xFF, 0xFF, 0x7F, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x3F, 0xFC, 0xFF, 0x07, 
	0x00, 0x00, 0x00, 0x00, 
	0x11, 0x17, 0x05, 0x04, 
	0x03, 0x02, 0x01, 0x00, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0x16, 0x15, 0x14, 0x13, 
	0x12, 0x11, 0x10, 0x0F, 
	0x0E, 0x0D, 0x0C, 0x0B, 
	0x0A, 0x09, 0x08, 0x07, 
	0x06, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0x00, 0x00, 0x80, 0xFF, 
	0xFF, 0x01, 0xF8, 0xFF, 
	0xFF, 0x07, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0xC0, 0xFF, 0xFF, 0x78, 
	0xFC, 0xFF, 0x3F, 0x00, 
	0xFF, 0x2A, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0x17, 0x18, 0x19, 0x1A, 
	0x1B, 0x1C, 0x1D, 0x1E, 
	0x1F, 0x20, 0x21, 0x22, 
	0x23, 0x24, 0x25, 0x26, 
	0x27, 0x28, 0xFF, 0xFF, 
	0xFF, 0x3D, 0x3E, 0x3F, 
	0x40, 0xFF, 0xFF, 0xFF, 
	0x29, 0x2A, 0x2B, 0x2C, 
	0x2D, 0x2E, 0x2F, 0x30, 
	0x31, 0x32, 0x33, 0x34, 
	0x35, 0x36, 0x37, 0x38, 
	0x39, 0x3A, 0x3B, 0x3C, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0xFF, 0x00, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00,  /* CDC_SLOT_TABLE */
	0x00,  /* Reserved704 */
	0x00,  /* Reserved705 */
	0x00, 0x00,  /* Reserved706 */
	0x00, 0x00,  /* Reserved708 */
	0x03,  /* MC_RAW_FILTER_MASK */
	0x01,  /* MC_RAW_IIR_COEF */
	0x22, 0x01,  /* MC_RAW_IIR_THRESH */
	0xAA, 0x00,  /* MC_RAW_CMF_THRESH */
	0x03,  /* SC_RAW_FILTER_MASK */
	0x01,  /* SC_RAW_IIR_COEF */
	0xE8, 0x03,  /* SC_RAW_IIR_THRESH */
	0x58, 0x02,  /* SC_RAW_CMF_THRESH */
	0x03,  /* BTN_MC_RAW_FILTER_MASK */
	0x01,  /* BTN_MC_RAW_IIR_COEF */
	0x64, 0x00,  /* BTN_MC_RAW_IIR_THRESH */
	0xC8, 0x00,  /* BTN_MC_RAW_CMF_THRESH */
	0x03,  /* BTN_SC_RAW_FILTER_MASK */
	0x01,  /* BTN_SC_RAW_IIR_COEF */
	0x64, 0x00,  /* BTN_SC_RAW_IIR_THRESH */
	0xC8, 0x00,  /* BTN_SC_RAW_CMF_THRESH */
	0x03,  /* Reserved734 */
	0x01,  /* Reserved735 */
	0x64, 0x00,  /* Reserved736 */
	0xC8, 0x00,  /* Reserved738 */
	0x03,  /* CA_MC_RAW_FILTER_MASK */
	0x01,  /* CA_MC_RAW_IIR_COEF */
	0x64, 0x00,  /* CA_MC_RAW_IIR_THRESH */
	0xC8, 0x00,  /* CA_MC_RAW_CMF_THRESH */
	0x03,  /* CA_BTN_MC_RAW_FILTER_MASK */
	0x01,  /* CA_BTN_MC_RAW_IIR_COEFF_BUT */
	0x64, 0x00,  /* CA_BTN_MC_RAW_IIR_THRESH */
	0xC8, 0x00,  /* CA_BTN_MC_RAW_CMF_THRESH */
	0x03,  /* GLOVE_MC_RAW_FILTER_MASK */
	0x01,  /* GLOVE_MC_RAW_IIR_COEF */
	0xBE, 0x00,  /* GLOVE_MC_RAW_IIR_THRESH */
	0x64, 0x00,  /* GLOVE_MC_RAW_CMF_THRESH */
	0x03,  /* GLOVE_SC_RAW_FILTER_MASK */
	0x02,  /* GLOVE_SC_RAW_IIR_COEF */
	0xE8, 0x03,  /* GLOVE_SC_RAW_IIR_THRESH */
	0xF4, 0x01,  /* GLOVE_SC_RAW_CMF_THRESH */
	0x03,  /* BTN_GLOVE_MC_RAW_FILTER_MASK */
	0x01,  /* BTN_GLOVE_MC_RAW_IIR_COEF */
	0x64, 0x00,  /* BTN_GLOVE_MC_RAW_IIR_THRESH */
	0xC8, 0x00,  /* BTN_GLOVE_MC_RAW_CMF_THRESH */
	0x03,  /* BTN_GLOVE_SC_RAW_FILTER_MASK */
	0x01,  /* BTN_GLOVE_SC_RAW_IIR_COEF */
	0x64, 0x00,  /* BTN_GLOVE_SC_RAW_IIR_THRESH */
	0xC8, 0x00,  /* BTN_GLOVE_SC_RAW_CMF_THRESH */
	0x03,  /* Reserved776 */
	0x01,  /* Reserved777 */
	0x64,  /* Reserved778 */
	0x00,  /* Reserved779 */
	0xC8,  /* Reserved780 */
	0x00,  /* Reserved781 */
	0x03,  /* Reserved782 */
	0x01,  /* Reserved783 */
	0x64,  /* Reserved784 */
	0x00,  /* Reserved785 */
	0xC8,  /* Reserved786 */
	0x00,  /* Reserved787 */
	0xFA, 0x00,  /* WF_RAW_CALC_THRESH */
	0xC8, 0x00,  /* WF_DIFF_CALC_THRESH */
	0xDC, 0x05,  /* WF_RAW_VAR_THRESH */
	0xF4, 0x01,  /* WF_DIFF_VAR_THRESH */
	0xAC, 0x0D,  /* WF_LEVEL_THRESH */
	0x0A,  /* WF_ENTER_DEBOUNCE */
	0x0A,  /* WF_EXIT_DEBOUNCE */
	0x01,  /* FINGER_BL_SNS_WIDTH */
	0x01,  /* FINGER_BL_UPDATE_SPEED */
	0x64, 0x00,  /* FINGER_BL_THRESH_MC */
	0xC8, 0x00,  /* FINGER_BL_THRESH_SC */
	0x01,  /* GLOVE_BL_SNS_WIDTH */
	0x01,  /* GLOVE_BL_UPDATE_SPEED */
	0x64, 0x00,  /* GLOVE_BL_THRESH_MC */
	0xC8, 0x00,  /* GLOVE_BL_THRESH_SC */
	0x01,  /* Reserved812 */
	0x01,  /* Reserved813 */
	0x64,  /* Reserved814 */
	0x00,  /* Reserved815 */
	0xC8,  /* Reserved816 */
	0x00,  /* Reserved817 */
	0x32, 0x00,  /* BL_THRESH_BTN_MC */
	0x32, 0x00,  /* BL_THRESH_BTN_SC */
	0x00, 0x00,  /* Reserved822 */
	0x00, 0x00, 0x00, 0x00,  /* PQ_CTRL */
	0x00, 0x00, 0x00, 0x00,  /* PQ_CTRL2 */
	0x00, 0x00, 0x00, 0x00,  /* PQ_CTRL3 */
	0x00, 0x50, 0x15, 0xA0,  /* REFGEN_CTL */
	0x00, 0x00, 0xFF, 0x80,  /* TX_CTRL */
	0x00, 0x00, 0x20, 0x00,  /* RX_CTRL */
	0x00, 0x20, 0x00, 0x00,  /* INFRA_CTRL */
	0x64,  /* STARTUP_DELAY */
	0x00,  /* FORCE_SINGLE_TX */
	0x2C, 0x01,  /* SCALE_FACT_MC */
	0xC8, 0x00,  /* SCALE_FACT_SC */
	0xC8, 0x00,  /* SCALE_FACT_BTN_MC */
	0xC8, 0x00,  /* SCALE_FACT_BTN_SC */
	0xC8, 0x00,  /* Reserved862 */
	0x02,  /* TX_PUMP_VOLTAGE */
	0x0A,  /* DISCARD_TIME */
	0x01,  /* VDDA_MODE */
	0x00,  /* BUTTON_LAYOUT */
	0x0C,  /* MTX_ORDER */
	0x00,  /* EXT_SYNC */
	0x00,  /* TX_FREQ_METHOD_MC */
	0x00,  /* TX_FREQ_METHOD_SC */
	0x00,  /* Reserved872 */
	0x00,  /* Reserved873 */
	0x00,  /* Reserved874 */
	0x18,  /* NM_WB_IDAC */
	0x20,  /* SAFE_RAW_RANGE_PERCENT_MC */
	0x40,  /* SAFE_RAW_RANGE_PERCENT_SC */
	0x40,  /* SAFE_RAW_RANGE_PERCENT_MC_BTN */
	0x40,  /* SAFE_RAW_RANGE_PERCENT_SC_BTN */
	0xFA, 0x00,  /* INT_VOLTAGE_MC */
	0xFA, 0x00,  /* INT_VOLTAGE_SC */
	0xFA, 0x00,  /* INT_VOLTAGE_MC_BTN */
	0xFA, 0x00,  /* INT_VOLTAGE_SC_BTN */
	0x26,  /* BAL_TARGET_MC */
	0x40,  /* BAL_TARGET_SC */
	0x26,  /* BAL_TARGET_MC_BTN */
	0x26,  /* BAL_TARGET_SC_BTN */
	0x14, 0x05,  /* ILEAK_MAX */
	0xF0, 0x0A,  /* VDDA_LEVEL */
	0xE8, 0x03,  /* PUMP_DELAY_US */
	0x73,  /* MC_PWC_LIMIT_PERCENT */
	0x73,  /* SC_PWC_LIMIT_PERCENT */
	0x01,  /* HW_BL_GIDAC_LSB_CONFIG */
	0x00,  /* Reserved901 */
	0x86, 0x00,  /* TX_PERIOD_MC */
	0x8B, 0x00,  /* CA_HOP0_TX_PERIOD_MC */
	0x94, 0x00,  /* CA_HOP1_TX_PERIOD_MC */
	0xA5, 0x00,  /* CA_HOP2_TX_PERIOD_MC */
	0x99, 0x00,  /* TX_PERIOD_SC */
	0xA2, 0x00,  /* TX_PERIOD_BTN_MC */
	0xA5, 0x00,  /* TX_PERIOD_BTN_SC */
	0x13, 0x00,  /* TX_PULSES_MC */
	0x7F, 0x00,  /* CA_MC_BASE_TX_PULSES_NUM */
	0x7F, 0x00,  /* CA_HOP0_TX_PULSES_MC */
	0x7F, 0x00,  /* CA_HOP1_TX_PULSES_MC */
	0x7F, 0x00,  /* CA_HOP2_TX_PULSES_MC */
	0x28, 0x00,  /* TX_PULSES_SC */
	0x40, 0x00,  /* TX_PULSES_BTN_MC */
	0x20, 0x00,  /* TX_PULSES_BTN_SC */
	0x40, 0x00,  /* Reserved932 */
	0x2B, 0x00,  /* TX_PULSES_GLOVE_MC */
	0x28, 0x00,  /* TX_PULSES_GLOVE_SC */
	0x40, 0x00,  /* TX_PULSES_BTN_GLOVE_MC */
	0x20, 0x00,  /* TX_PULSES_BTN_GLOVE_SC */
	0x40, 0x00,  /* Reserved942 */
	0x40, 0x00,  /* Reserved944 */
	0x00,  /* RX_ATTEN_RES_BYPASS */
	0x00,  /* TX_SPREADER_STEP */
	0x01,  /* TX_SPREADER_PULSES */
	0x00, 0x00, 0x00,  /* Reserved949 */
	0x50,  /* BTN_LS_ON_THRSH_MUT_0 */
	0x50,  /* BTN_LS_ON_THRSH_MUT_1 */
	0x50,  /* BTN_LS_ON_THRSH_MUT_2 */
	0x50,  /* BTN_LS_ON_THRSH_MUT_3 */
	0x46,  /* BTN_LS_OFF_THRSH_MUT_0 */
	0x46,  /* BTN_LS_OFF_THRSH_MUT_1 */
	0x46,  /* BTN_LS_OFF_THRSH_MUT_2 */
	0x46,  /* BTN_LS_OFF_THRSH_MUT_3 */
	0x41,  /* BTN_LS_ON_THRSH_SELF_0 */
	0x41,  /* BTN_LS_ON_THRSH_SELF_1 */
	0x41,  /* BTN_LS_ON_THRSH_SELF_2 */
	0x41,  /* BTN_LS_ON_THRSH_SELF_3 */
	0x28,  /* BTN_LS_OFF_THRSH_SELF_0 */
	0x28,  /* BTN_LS_OFF_THRSH_SELF_1 */
	0x28,  /* BTN_LS_OFF_THRSH_SELF_2 */
	0x28,  /* BTN_LS_OFF_THRSH_SELF_3 */
	0x00,  /* BTN_LS_TD_DEBOUNCE */
	0x00,  /* Reserved969 */
	0x00, 0x00,  /* Reserved970 */
	0x14,  /* BTN_HS_ON_THRSH_MUT_0 */
	0x14,  /* BTN_HS_ON_THRSH_MUT_1 */
	0x14,  /* BTN_HS_ON_THRSH_MUT_2 */
	0x14,  /* BTN_HS_ON_THRSH_MUT_3 */
	0x0A,  /* BTN_HS_OFF_THRSH_MUT_0 */
	0x0A,  /* BTN_HS_OFF_THRSH_MUT_1 */
	0x0A,  /* BTN_HS_OFF_THRSH_MUT_2 */
	0x0A,  /* BTN_HS_OFF_THRSH_MUT_3 */
	0x1E,  /* BTN_HS_ON_THRSH_SELF_0 */
	0x1E,  /* BTN_HS_ON_THRSH_SELF_1 */
	0x1E,  /* BTN_HS_ON_THRSH_SELF_2 */
	0x1E,  /* BTN_HS_ON_THRSH_SELF_3 */
	0x0A,  /* BTN_HS_OFF_THRSH_SELF_0 */
	0x0A,  /* BTN_HS_OFF_THRSH_SELF_1 */
	0x0A,  /* BTN_HS_OFF_THRSH_SELF_2 */
	0x0A,  /* BTN_HS_OFF_THRSH_SELF_3 */
	0x01,  /* BTN_HS_TOUCHDOWN_DEBOUNCE */
	0x00,  /* Reserved989 */
	0x00, 0x00,  /* Reserved990 */
	0x28, 0x00,  /* BTN_HIGHSEN_MODE_THRSH_MUT */
	0x28, 0x00,  /* BTN_HIGHSEN_MODE_THRSH_SELF */
	0xC8, 0x00,  /* BTN_LOWSEN_MODE_THRSH_MUT */
	0x5E, 0x01,  /* BTN_LOWSEN_MODE_THRSH_SELF */
	0x0A,  /* GLOVE_BTN_FORBID_DEBOUNCE */
	0x01,  /* GLOVE_BTN_MODE_SWITCH_DEBOUNCE */
	0x00,  /* BTN_PROCESS_IF_TOUCH_DETECTED */
	0x00,  /* Reserved1003 */
	0xC8, 0x00,  /* FINGER_THRESH_MUT_HI */
	0xB4, 0x00,  /* FINGER_THRESH_MUT_LO */
	0xC8, 0x00,  /* FINGER_THRESH_SELF */
	0x02,  /* FINGER_Z9_FILT_SCALE */
	0x00,  /* FINGER_Z8_FILT_SCALE */
	0x0A,  /* MIN_FAT_FINGER_SIZE */
	0x01,  /* MIN_FAT_FINGER_SIZE_HYST */
	0x23,  /* MAX_FAT_FINGER_SIZE */
	0x08,  /* MAX_FAT_FINGER_SIZE_HYST */
	0x40,  /* FINGER_SIG_THRESH_MULT */
	0x03,  /* FINGER_OBJECT_FEATURES */
	0x80, 0x00,  /* FINGER_Z_SCALE */
	0x08,  /* FINGER_INNER_EDGE_GAIN */
	0x78,  /* FINGER_OUTER_EDGE_GAIN */
	0x00,  /* FINGER_POS_CALC_METHOD */
	0x00,  /* Reserved1023 */
	0x2C, 0x01,  /* CA_FINGER_THRESH_MUT */
	0x02,  /* FINGER_MT_DEBOUNCE */
	0x02,  /* CA_FINGER_MT_DEBOUNCE */
	0x04,  /* CA_FINGER_Z9_FILT_SCALE */
	0x0F,  /* CA_MIN_FAT_FINGER_SIZE */
	0x28,  /* CA_MAX_FAT_FINGER_SIZE */
	0x40,  /* WF_THRESH_MUT_COEF */
	0x05,  /* WF_MT_DEBOUNCE */
	0x01,  /* RX_LINE_FILT_ENABLE */
	0x02,  /* RX_LINE_FILT_DEBOUNCE */
	0x58,  /* RX_LINE_FILT_THRESH */
	0x05,  /* TOUCHMODE_FRAME_NUM_TO_CONFIRM_FINGER_MODE */
	0x00, 0x00, 0x00,  /* Reserved1037 */
	0x64, 0x00,  /* GLOVES_THRESH_MUT_HI */
	0x46, 0x00,  /* GLOVES_THRESH_MUT_LO */
	0x1E, 0x00,  /* GLOVES_THRESH_SELF */
	0x03,  /* GLOVES_Z9_FILT_SCALE */
	0x01,  /* GLOVES_Z8_FILT_SCALE */
	0x0A,  /* GLOVES_MIN_FAT_SIZE */
	0x01,  /* GLOVES_MIN_FAT_SIZE_HYST */
	0x20,  /* GLOVES_MAX_FAT_SIZE */
	0x04,  /* GLOVES_MAX_FAT_SIZE_HYST */
	0x40,  /* GLOVES_SIG_THRESH_MULT */
	0x00,  /* GLOVES_OBJECT_FEATURES */
	0x40, 0x00,  /* GLOVES_Z_SCALE */
	0x08,  /* GLOVES_INNER_EDGE_GAIN */
	0x78,  /* GLOVES_OUTER_EDGE_GAIN */
	0x00,  /* GLOVES_POS_CALC_METHOD */
	0x00,  /* Reserved1059 */
	0x2C, 0x01,  /* TOUCHMODE_GLOVE_HTI */
	0xDC, 0x05,  /* TOUCHMODE_GLOVE_MAX_SIG_SUM */
	0x01,  /* GLOVES_FT_DEBOUNCE */
	0x00,  /* GLOVES_FT_DEBOUNCE_EDGE_MASK */
	0x03,  /* GLOVES_MT_DEBOUNCE */
	0x20,  /* GLOVES_GRIP_FILT_SCALE */
	0x64,  /* TOUCHMODE_FRAME_NUM_TO_CONFIRM_GLOVE_MODE */
	0x00, 0x00, 0x00,  /* Reserved1069 */
	0x00, 0x00,  /* TOUCHMODE_FINGER_SWITCH_DEBOUNCE */
	0x2C, 0x01,  /* TOUCHMODE_FINGER_EXIT_DELAY */
	0x2A, 0x00,  /* TOUCHMODE_GLOVE_SWITCH_DEBOUNCE */
	0x2C, 0x01,  /* TOUCHMODE_GLOVE_EXIT_DELAY */
	0x00, 0x00,  /* TOUCHMODE_GLOVE_FINGER_SWITCH_DEBOUNCE */
	0x00, 0x00,  /* Reserved1082 */
	0x00, 0x00,  /* ACT_DIST0_SQR */
	0x00, 0x00,  /* ACT_DIST2_SQR */
	0x00, 0x00,  /* ACT_DIST_TOUCHDOWN_SQR */
	0x00, 0x00,  /* ACT_DIST_LIFTOFF_SQR */
	0xFF,  /* ACT_DIST_Z_THRESHOLD */
	0x00,  /* LARGE_OBJ_CFG */
	0x00, 0x00,  /* Reserved1094 */
	0xF0,  /* XY_FILTER_MASK */
	0x10,  /* XY_FILT_IIR_COEF_SLOW */
	0x10,  /* XY_FILT_IIR_COEF_FAST */
	0x64,  /* XY_FILT_XY_THR_SLOW */
	0xC8,  /* XY_FILT_XY_THR_FAST */
	0x01,  /* XY_FILT_Z_IIR_COEFF */
	0x00,  /* XY_FILT_PREDICTION_COEF */
	0x00,  /* Reserved1103 */
	0xF0,  /* XY_FILTER_MASK_CA */
	0x08,  /* XY_FILT_IIR_COEF_SLOW_CA */
	0x08,  /* XY_FILT_IIR_COEF_FAST_CA */
	0xFF,  /* XY_FILT_XY_THR_SLOW_CA */
	0xFF,  /* XY_FILT_XY_THR_FAST_CA */
	0x01,  /* XY_FILT_Z_IIR_COEFF_CA */
	0x00,  /* XY_FILT_PREDICTION_COEF_CA */
	0x00,  /* Reserved1111 */
	0x00,  /* XY_FILT_AXIS_IIR_COEF */
	0x00,  /* XY_FILT_AXIS_HYST */
	0x00,  /* XY_FILT_ANGLE_IIR_COEF */
	0x00,  /* XY_FILT_ANGLE_HYST */
	0xE0, 0x93, 0x04, 0x00,  /* MAX_VELOCITY_SQR */
	0x40, 0x0D, 0x03, 0x00,  /* FINGER_ID_MAX_FINGER_ACCELERATION2 */
	0x00, 0x00,  /* GRIP_XEDG_A */
	0x00, 0x00,  /* GRIP_XEDG_B */
	0x00, 0x00,  /* GRIP_XEXC_A */
	0x00, 0x00,  /* GRIP_XEXC_B */
	0x00, 0x00,  /* GRIP_YEDG_A */
	0x00, 0x00,  /* GRIP_YEDG_B */
	0x00, 0x00,  /* GRIP_YEXC_A */
	0x00, 0x00,  /* GRIP_YEXC_B */
	0x01,  /* GRIP_FIRST_EXC */
	0x00,  /* GRIP_EXC_EDGE_ORIGIN */
	0x00,  /* GRIP_ENABLE */
	0x03,  /* FINGER_LIFTOFF_DEBOUNCE */
	0x05,  /* GLOVE_LIFTOFF_DEBOUNCE */
	0x00, 0x00, 0x00,  /* Reserved1145 */
	0x03,  /* WATER_REJ_SNS_WIDTH */
	0x00,  /* SLIM_POSITION_OFFSET_ALONG_TX */
	0x00,  /* SLIM_POSITION_OFFSET_ALONG_RX */
	0x05,  /* WET_FINGER_Z8_MULT */
	0x40, 0x1F, 0x00, 0x00,  /* MIN_FF_Z9 */
	0xE0, 0x2E, 0x00, 0x00,  /* MAX_MF_Z9 */
	0x40, 0x1F, 0x00, 0x00,  /* MIN_FF_SIG_SUM_EDGE */
	0x34,  /* MF_CENTERSIG_RATIO */
	0x14,  /* SD_SIZE_THRESH */
	0xE8, 0x03,  /* SD_SIG_THRESH_ON */
	0xB0, 0x04,  /* SD_SIG_THRESH_OFF */
	0xAA, 0x00,  /* VP_DLT_RST_THRESH */
	0x48, 0x0D,  /* VP_DLT_THRESH */
	0x05,  /* FAT_AXIS_LENGTH_THRESH */
	0x00,  /* PEAK_IGNORE_COEF */
	0x03,  /* AXIS_ORIENTATION_ENABLE */
	0x00, 0x00, 0x00,  /* Reserved1177 */
	0x00,  /* CLIPPING_X_LOW */
	0x00,  /* CLIPPING_X_HIGH */
	0x00,  /* CLIPPING_Y_LOW */
	0x00,  /* CLIPPING_Y_HIGH */
	0x0F, 0x00,  /* CALC_THRESH */
	0x37, 0x00,  /* OFFSET_S1 */
	0x19, 0x00,  /* OFFSET_S2 */
	0x66, 0x08,  /* Z_SUM_8MM */
	0x26, 0x02,  /* Z_SUM_4MM */
	0x4A, 0x01,  /* Z_SUM_3MM */
	0x82, 0x00,  /* Z_SUM_1MM */
	0x90, 0x01,  /* LOW_PIVOT */
	0xA3, 0x02,  /* HIGH_PIVOT */
	0x78, 0x00,  /* LOW_PIVOT2 */
	0xB4, 0x00,  /* HIGH_PIVOT2 */
	0x7A, 0x03,  /* EDGE_DEBOUNCE_THRESH */
	0xBC, 0x02,  /* CENTER_MAGNITUDE_SCALE */
	0xC0, 0x00,  /* CENTROID_CORNER_GAIN */
	0x03,  /* EDGE_DEBOUNCE_COUNT */
	0x00,  /* BR2_ALWAYS_ON_FLAG */
	0x03,  /* FINGER_SIZE_MIN_CHANGE_EDGE */
	0x00,  /* Reserved1215 */
	0x00,  /* Reserved1216 */
	0x0A,  /* GEST_PAN_ACTIVE_DISTANCE_X */
	0x0A,  /* GEST_PAN_ACTIVE_DISTANCE_Y */
	0x0A,  /* GEST_ZOOM_ACTIVE_DISTANCE */
	0x02,  /* GEST_DEBOUNCE_MULTITOUCH */
	0x23,  /* GEST_FLICK_ACTIVE_DISTANCE_X */
	0x23,  /* GEST_FLICK_ACTIVE_DISTANCE_Y */
	0x50,  /* GEST_FLICK_SAMPLE_TIME */
	0x02,  /* GEST_DEBOUNCE_SINGLETOUCH_PAN_COUNT */
	0x04,  /* GEST_MULTITOUCH_ROTATION_THRESHOLD */
	0x14,  /* GEST_ROTATE_DEBOUNCE_LIMIT */
	0x00,  /* Reserved1227 */
	0x32,  /* GEST_ST_MAX_DOUBLE_CLICK_RADIUS */
	0x1E,  /* GEST_CLICK_X_RADIUS */
	0x64,  /* GEST_CLICK_Y_RADIUS */
	0x00,  /* Reserved1231 */
	0xC8, 0x00,  /* GEST_MT_MAX_CLICK_TIMEOUT_MSEC */
	0x14, 0x00,  /* GEST_MT_MIN_CLICK_TIMEOUT_MSEC */
	0xC8, 0x00,  /* GEST_ST_MAX_CLICK_TIMEOUT_MSEC */
	0x14, 0x00,  /* GEST_ST_MIN_CLICK_TIMEOUT_MSEC */
	0x90, 0x01,  /* GEST_ST_MAX_DOUBLECLICK_TIMEOUT_MSEC */
	0x64, 0x00,  /* GEST_ST_MIN_DOUBLECLICK_TIMEOUT_MSEC */
	0x14, 0x00,  /* GEST_RIGHTCLICK_MIN_TIMEOUT_MSEC */
	0xC8, 0x00,  /* GEST_RIGHTCLICK_MAX_TIMEOUT_MSEC */
	0x28, 0x00,  /* GEST_SETTLING_TIMEOUT_MSEC */
	0xF7, 0xFF,  /* GEST_GROUP_MASK */
	0x46, 0x00,  /* TOUCHMODE_LFT_SELF_THRSH */
	0x56, 0x05,  /* X_RESOLUTION */
	0x00, 0x03,  /* Y_RESOLUTION */
	0xD5, 0x2D,  /* X_LENGTH_100xMM */
	0xC8, 0x19,  /* Y_LENGTH_100xMM */
	0x2D,  /* X_PITCH_10xMM */
	0x2D,  /* Y_PITCH_10xMM */
	0x01,  /* SENSOR_ASSIGNMENT */
	0x01,  /* ACT_LFT_EN */
	0x03,  /* TOUCHMODE_CONFIG */
	0x02,  /* LRG_OBJ_CFG */
	0x0A,  /* MAX_REPORTED_TOUCH_NUM */
	0x08,  /* OPMODE_CFG */
	0x00,  /* Reserved1270 */
	0x00,  /* Reserved1271 */
	0x02,  /* Reserved1272 */
	0x01,  /* GESTURE_ENABLED */
	0x00, 0x00,  /* Reserved1274 */
	0x01,  /* LOW_POWER_ENABLE */
	0x08,  /* ACT_INTRVL0 */
	0x03, 0x00,  /* ACT_LFT_INTRVL0 */
	0x64, 0x00,  /* LP_INTRVL0 */
	0xE8, 0x03,  /* TCH_TMOUT0 */
	0x00,  /* POST_CFG */
	0x00,  /* Reserved1285 */
	0x00, 0x00,  /* CONFIG_VER */
	0x00,  /* SEND_REPORT_AFTER_ACTIVE_INTERVAL_CFG */
	0x00,  /* PIP_REPORTING_DISABLE */
	0x00, 0x00,  /* INTERRUPT_PIN_OVERRIDE */
	0xDA, 0x32,  /* CONFIG_CRC */
};

/* Touchscreen Parameters Field Sizes (Writable: 0:Readonly; 1:Writable) */
static const uint16_t cyttsp6_param_size[] = {
/*	Size	Name	*/
	2, /* CONFIG_DATA_SIZE */
	2, /* CONFIG_DATA_MAX_SIZE */
	2, /* CROSS_NUM */
	1, /* TX_NUM */
	1, /* RX_NUM */
	1, /* SENS_NUM */
	1, /* BUTTON_NUM */
	1, /* SLOTS_MUT */
	1, /* SLOTS_SELF_RX */
	1, /* SLOTS_SELF_TX */
	1, /* SLOTS_SELF */
	1, /* PROX_SCAN_AXIS */
	1, /* SCANNING_MODE_BUTTON */
	1, /* SELF_Z_MODE */
	1, /* WATER_REJ_ENABLE */
	1, /* WF_ENABLE */
	1, /* CHARGER_ARMOR_ENABLE */
	4, /* CA_REVERT_TIME_MS */
	4, /* CA_TOUCH_REVERT_TIME_MS */
	4, /* CA_TOUCH_REVERT_SIG_SUM_THRESH */
	1, /* CA_HOST_CTRL */
	1, /* CHARGER_STATUS */
	1, /* CA_TRIG_SRC */
	1, /* WB_CMF_ENABLE */
	1, /* WB_REVERT_THRESH */
	1, /* AFH_HOP_CYCLES_COUNT */
	1, /* NMI_SCAN_CNT */
	1, /* Reserved39 */
	4, /* CA_NMF_LIMIT */
	2, /* NMI_TCH_MAGNITUDE */
	2, /* NMI_TOUCH_THRESH */
	2, /* NMI_THRESH */
	2, /* WB_THRESH */
	2, /* SC_TRIG_THRESH */
	2, /* CA_DYN_CAL_SAFE_RAW_RANGE */
	1, /* CA_DYN_CAL_NUM_SENSOR_THLD_PERCENT */
	1, /* Reserved57 */
	1, /* NMF_DETECT_THRESH */
	1, /* NM_WB_SCAN_COUNT */
	1, /* Reserved60 */
	1, /* Reserved61 */
	2, /* MAX_MUTUAL_SCAN_INTERVAL */
	2, /* MAX_SELF_SCAN_INTERVAL */
	2, /* Reserved66 */
	48, /* CDC_SENSOR_MASKS */
	92, /* CDC_PIN_INDEX_TABLE */
	93, /* CDC_REAL_PIN_INDEX_TABLE */
	3, /* Reserved301 */
	8, /* CDC_MTX_PHASE_VECTOR */
	72, /* CDC_MTX_DECONV_COEF */
	320, /* CDC_SLOT_TABLE */
	1, /* Reserved704 */
	1, /* Reserved705 */
	2, /* Reserved706 */
	2, /* Reserved708 */
	1, /* MC_RAW_FILTER_MASK */
	1, /* MC_RAW_IIR_COEF */
	2, /* MC_RAW_IIR_THRESH */
	2, /* MC_RAW_CMF_THRESH */
	1, /* SC_RAW_FILTER_MASK */
	1, /* SC_RAW_IIR_COEF */
	2, /* SC_RAW_IIR_THRESH */
	2, /* SC_RAW_CMF_THRESH */
	1, /* BTN_MC_RAW_FILTER_MASK */
	1, /* BTN_MC_RAW_IIR_COEF */
	2, /* BTN_MC_RAW_IIR_THRESH */
	2, /* BTN_MC_RAW_CMF_THRESH */
	1, /* BTN_SC_RAW_FILTER_MASK */
	1, /* BTN_SC_RAW_IIR_COEF */
	2, /* BTN_SC_RAW_IIR_THRESH */
	2, /* BTN_SC_RAW_CMF_THRESH */
	1, /* Reserved734 */
	1, /* Reserved735 */
	2, /* Reserved736 */
	2, /* Reserved738 */
	1, /* CA_MC_RAW_FILTER_MASK */
	1, /* CA_MC_RAW_IIR_COEF */
	2, /* CA_MC_RAW_IIR_THRESH */
	2, /* CA_MC_RAW_CMF_THRESH */
	1, /* CA_BTN_MC_RAW_FILTER_MASK */
	1, /* CA_BTN_MC_RAW_IIR_COEFF_BUT */
	2, /* CA_BTN_MC_RAW_IIR_THRESH */
	2, /* CA_BTN_MC_RAW_CMF_THRESH */
	1, /* GLOVE_MC_RAW_FILTER_MASK */
	1, /* GLOVE_MC_RAW_IIR_COEF */
	2, /* GLOVE_MC_RAW_IIR_THRESH */
	2, /* GLOVE_MC_RAW_CMF_THRESH */
	1, /* GLOVE_SC_RAW_FILTER_MASK */
	1, /* GLOVE_SC_RAW_IIR_COEF */
	2, /* GLOVE_SC_RAW_IIR_THRESH */
	2, /* GLOVE_SC_RAW_CMF_THRESH */
	1, /* BTN_GLOVE_MC_RAW_FILTER_MASK */
	1, /* BTN_GLOVE_MC_RAW_IIR_COEF */
	2, /* BTN_GLOVE_MC_RAW_IIR_THRESH */
	2, /* BTN_GLOVE_MC_RAW_CMF_THRESH */
	1, /* BTN_GLOVE_SC_RAW_FILTER_MASK */
	1, /* BTN_GLOVE_SC_RAW_IIR_COEF */
	2, /* BTN_GLOVE_SC_RAW_IIR_THRESH */
	2, /* BTN_GLOVE_SC_RAW_CMF_THRESH */
	1, /* Reserved776 */
	1, /* Reserved777 */
	1, /* Reserved778 */
	1, /* Reserved779 */
	1, /* Reserved780 */
	1, /* Reserved781 */
	1, /* Reserved782 */
	1, /* Reserved783 */
	1, /* Reserved784 */
	1, /* Reserved785 */
	1, /* Reserved786 */
	1, /* Reserved787 */
	2, /* WF_RAW_CALC_THRESH */
	2, /* WF_DIFF_CALC_THRESH */
	2, /* WF_RAW_VAR_THRESH */
	2, /* WF_DIFF_VAR_THRESH */
	2, /* WF_LEVEL_THRESH */
	1, /* WF_ENTER_DEBOUNCE */
	1, /* WF_EXIT_DEBOUNCE */
	1, /* FINGER_BL_SNS_WIDTH */
	1, /* FINGER_BL_UPDATE_SPEED */
	2, /* FINGER_BL_THRESH_MC */
	2, /* FINGER_BL_THRESH_SC */
	1, /* GLOVE_BL_SNS_WIDTH */
	1, /* GLOVE_BL_UPDATE_SPEED */
	2, /* GLOVE_BL_THRESH_MC */
	2, /* GLOVE_BL_THRESH_SC */
	1, /* Reserved812 */
	1, /* Reserved813 */
	1, /* Reserved814 */
	1, /* Reserved815 */
	1, /* Reserved816 */
	1, /* Reserved817 */
	2, /* BL_THRESH_BTN_MC */
	2, /* BL_THRESH_BTN_SC */
	2, /* Reserved822 */
	4, /* PQ_CTRL */
	4, /* PQ_CTRL2 */
	4, /* PQ_CTRL3 */
	4, /* REFGEN_CTL */
	4, /* TX_CTRL */
	4, /* RX_CTRL */
	4, /* INFRA_CTRL */
	1, /* STARTUP_DELAY */
	1, /* FORCE_SINGLE_TX */
	2, /* SCALE_FACT_MC */
	2, /* SCALE_FACT_SC */
	2, /* SCALE_FACT_BTN_MC */
	2, /* SCALE_FACT_BTN_SC */
	2, /* Reserved862 */
	1, /* TX_PUMP_VOLTAGE */
	1, /* DISCARD_TIME */
	1, /* VDDA_MODE */
	1, /* BUTTON_LAYOUT */
	1, /* MTX_ORDER */
	1, /* EXT_SYNC */
	1, /* TX_FREQ_METHOD_MC */
	1, /* TX_FREQ_METHOD_SC */
	1, /* Reserved872 */
	1, /* Reserved873 */
	1, /* Reserved874 */
	1, /* NM_WB_IDAC */
	1, /* SAFE_RAW_RANGE_PERCENT_MC */
	1, /* SAFE_RAW_RANGE_PERCENT_SC */
	1, /* SAFE_RAW_RANGE_PERCENT_MC_BTN */
	1, /* SAFE_RAW_RANGE_PERCENT_SC_BTN */
	2, /* INT_VOLTAGE_MC */
	2, /* INT_VOLTAGE_SC */
	2, /* INT_VOLTAGE_MC_BTN */
	2, /* INT_VOLTAGE_SC_BTN */
	1, /* BAL_TARGET_MC */
	1, /* BAL_TARGET_SC */
	1, /* BAL_TARGET_MC_BTN */
	1, /* BAL_TARGET_SC_BTN */
	2, /* ILEAK_MAX */
	2, /* VDDA_LEVEL */
	2, /* PUMP_DELAY_US */
	1, /* MC_PWC_LIMIT_PERCENT */
	1, /* SC_PWC_LIMIT_PERCENT */
	1, /* HW_BL_GIDAC_LSB_CONFIG */
	1, /* Reserved901 */
	2, /* TX_PERIOD_MC */
	2, /* CA_HOP0_TX_PERIOD_MC */
	2, /* CA_HOP1_TX_PERIOD_MC */
	2, /* CA_HOP2_TX_PERIOD_MC */
	2, /* TX_PERIOD_SC */
	2, /* TX_PERIOD_BTN_MC */
	2, /* TX_PERIOD_BTN_SC */
	2, /* TX_PULSES_MC */
	2, /* CA_MC_BASE_TX_PULSES_NUM */
	2, /* CA_HOP0_TX_PULSES_MC */
	2, /* CA_HOP1_TX_PULSES_MC */
	2, /* CA_HOP2_TX_PULSES_MC */
	2, /* TX_PULSES_SC */
	2, /* TX_PULSES_BTN_MC */
	2, /* TX_PULSES_BTN_SC */
	2, /* Reserved932 */
	2, /* TX_PULSES_GLOVE_MC */
	2, /* TX_PULSES_GLOVE_SC */
	2, /* TX_PULSES_BTN_GLOVE_MC */
	2, /* TX_PULSES_BTN_GLOVE_SC */
	2, /* Reserved942 */
	2, /* Reserved944 */
	1, /* RX_ATTEN_RES_BYPASS */
	1, /* TX_SPREADER_STEP */
	1, /* TX_SPREADER_PULSES */
	3, /* Reserved949 */
	1, /* BTN_LS_ON_THRSH_MUT_0 */
	1, /* BTN_LS_ON_THRSH_MUT_1 */
	1, /* BTN_LS_ON_THRSH_MUT_2 */
	1, /* BTN_LS_ON_THRSH_MUT_3 */
	1, /* BTN_LS_OFF_THRSH_MUT_0 */
	1, /* BTN_LS_OFF_THRSH_MUT_1 */
	1, /* BTN_LS_OFF_THRSH_MUT_2 */
	1, /* BTN_LS_OFF_THRSH_MUT_3 */
	1, /* BTN_LS_ON_THRSH_SELF_0 */
	1, /* BTN_LS_ON_THRSH_SELF_1 */
	1, /* BTN_LS_ON_THRSH_SELF_2 */
	1, /* BTN_LS_ON_THRSH_SELF_3 */
	1, /* BTN_LS_OFF_THRSH_SELF_0 */
	1, /* BTN_LS_OFF_THRSH_SELF_1 */
	1, /* BTN_LS_OFF_THRSH_SELF_2 */
	1, /* BTN_LS_OFF_THRSH_SELF_3 */
	1, /* BTN_LS_TD_DEBOUNCE */
	1, /* Reserved969 */
	2, /* Reserved970 */
	1, /* BTN_HS_ON_THRSH_MUT_0 */
	1, /* BTN_HS_ON_THRSH_MUT_1 */
	1, /* BTN_HS_ON_THRSH_MUT_2 */
	1, /* BTN_HS_ON_THRSH_MUT_3 */
	1, /* BTN_HS_OFF_THRSH_MUT_0 */
	1, /* BTN_HS_OFF_THRSH_MUT_1 */
	1, /* BTN_HS_OFF_THRSH_MUT_2 */
	1, /* BTN_HS_OFF_THRSH_MUT_3 */
	1, /* BTN_HS_ON_THRSH_SELF_0 */
	1, /* BTN_HS_ON_THRSH_SELF_1 */
	1, /* BTN_HS_ON_THRSH_SELF_2 */
	1, /* BTN_HS_ON_THRSH_SELF_3 */
	1, /* BTN_HS_OFF_THRSH_SELF_0 */
	1, /* BTN_HS_OFF_THRSH_SELF_1 */
	1, /* BTN_HS_OFF_THRSH_SELF_2 */
	1, /* BTN_HS_OFF_THRSH_SELF_3 */
	1, /* BTN_HS_TOUCHDOWN_DEBOUNCE */
	1, /* Reserved989 */
	2, /* Reserved990 */
	2, /* BTN_HIGHSEN_MODE_THRSH_MUT */
	2, /* BTN_HIGHSEN_MODE_THRSH_SELF */
	2, /* BTN_LOWSEN_MODE_THRSH_MUT */
	2, /* BTN_LOWSEN_MODE_THRSH_SELF */
	1, /* GLOVE_BTN_FORBID_DEBOUNCE */
	1, /* GLOVE_BTN_MODE_SWITCH_DEBOUNCE */
	1, /* BTN_PROCESS_IF_TOUCH_DETECTED */
	1, /* Reserved1003 */
	2, /* FINGER_THRESH_MUT_HI */
	2, /* FINGER_THRESH_MUT_LO */
	2, /* FINGER_THRESH_SELF */
	1, /* FINGER_Z9_FILT_SCALE */
	1, /* FINGER_Z8_FILT_SCALE */
	1, /* MIN_FAT_FINGER_SIZE */
	1, /* MIN_FAT_FINGER_SIZE_HYST */
	1, /* MAX_FAT_FINGER_SIZE */
	1, /* MAX_FAT_FINGER_SIZE_HYST */
	1, /* FINGER_SIG_THRESH_MULT */
	1, /* FINGER_OBJECT_FEATURES */
	2, /* FINGER_Z_SCALE */
	1, /* FINGER_INNER_EDGE_GAIN */
	1, /* FINGER_OUTER_EDGE_GAIN */
	1, /* FINGER_POS_CALC_METHOD */
	1, /* Reserved1023 */
	2, /* CA_FINGER_THRESH_MUT */
	1, /* FINGER_MT_DEBOUNCE */
	1, /* CA_FINGER_MT_DEBOUNCE */
	1, /* CA_FINGER_Z9_FILT_SCALE */
	1, /* CA_MIN_FAT_FINGER_SIZE */
	1, /* CA_MAX_FAT_FINGER_SIZE */
	1, /* WF_THRESH_MUT_COEF */
	1, /* WF_MT_DEBOUNCE */
	1, /* RX_LINE_FILT_ENABLE */
	1, /* RX_LINE_FILT_DEBOUNCE */
	1, /* RX_LINE_FILT_THRESH */
	1, /* TOUCHMODE_FRAME_NUM_TO_CONFIRM_FINGER_MODE */
	3, /* Reserved1037 */
	2, /* GLOVES_THRESH_MUT_HI */
	2, /* GLOVES_THRESH_MUT_LO */
	2, /* GLOVES_THRESH_SELF */
	1, /* GLOVES_Z9_FILT_SCALE */
	1, /* GLOVES_Z8_FILT_SCALE */
	1, /* GLOVES_MIN_FAT_SIZE */
	1, /* GLOVES_MIN_FAT_SIZE_HYST */
	1, /* GLOVES_MAX_FAT_SIZE */
	1, /* GLOVES_MAX_FAT_SIZE_HYST */
	1, /* GLOVES_SIG_THRESH_MULT */
	1, /* GLOVES_OBJECT_FEATURES */
	2, /* GLOVES_Z_SCALE */
	1, /* GLOVES_INNER_EDGE_GAIN */
	1, /* GLOVES_OUTER_EDGE_GAIN */
	1, /* GLOVES_POS_CALC_METHOD */
	1, /* Reserved1059 */
	2, /* TOUCHMODE_GLOVE_HTI */
	2, /* TOUCHMODE_GLOVE_MAX_SIG_SUM */
	1, /* GLOVES_FT_DEBOUNCE */
	1, /* GLOVES_FT_DEBOUNCE_EDGE_MASK */
	1, /* GLOVES_MT_DEBOUNCE */
	1, /* GLOVES_GRIP_FILT_SCALE */
	1, /* TOUCHMODE_FRAME_NUM_TO_CONFIRM_GLOVE_MODE */
	3, /* Reserved1069 */
	2, /* TOUCHMODE_FINGER_SWITCH_DEBOUNCE */
	2, /* TOUCHMODE_FINGER_EXIT_DELAY */
	2, /* TOUCHMODE_GLOVE_SWITCH_DEBOUNCE */
	2, /* TOUCHMODE_GLOVE_EXIT_DELAY */
	2, /* TOUCHMODE_GLOVE_FINGER_SWITCH_DEBOUNCE */
	2, /* Reserved1082 */
	2, /* ACT_DIST0_SQR */
	2, /* ACT_DIST2_SQR */
	2, /* ACT_DIST_TOUCHDOWN_SQR */
	2, /* ACT_DIST_LIFTOFF_SQR */
	1, /* ACT_DIST_Z_THRESHOLD */
	1, /* LARGE_OBJ_CFG */
	2, /* Reserved1094 */
	1, /* XY_FILTER_MASK */
	1, /* XY_FILT_IIR_COEF_SLOW */
	1, /* XY_FILT_IIR_COEF_FAST */
	1, /* XY_FILT_XY_THR_SLOW */
	1, /* XY_FILT_XY_THR_FAST */
	1, /* XY_FILT_Z_IIR_COEFF */
	1, /* XY_FILT_PREDICTION_COEF */
	1, /* Reserved1103 */
	1, /* XY_FILTER_MASK_CA */
	1, /* XY_FILT_IIR_COEF_SLOW_CA */
	1, /* XY_FILT_IIR_COEF_FAST_CA */
	1, /* XY_FILT_XY_THR_SLOW_CA */
	1, /* XY_FILT_XY_THR_FAST_CA */
	1, /* XY_FILT_Z_IIR_COEFF_CA */
	1, /* XY_FILT_PREDICTION_COEF_CA */
	1, /* Reserved1111 */
	1, /* XY_FILT_AXIS_IIR_COEF */
	1, /* XY_FILT_AXIS_HYST */
	1, /* XY_FILT_ANGLE_IIR_COEF */
	1, /* XY_FILT_ANGLE_HYST */
	4, /* MAX_VELOCITY_SQR */
	4, /* FINGER_ID_MAX_FINGER_ACCELERATION2 */
	2, /* GRIP_XEDG_A */
	2, /* GRIP_XEDG_B */
	2, /* GRIP_XEXC_A */
	2, /* GRIP_XEXC_B */
	2, /* GRIP_YEDG_A */
	2, /* GRIP_YEDG_B */
	2, /* GRIP_YEXC_A */
	2, /* GRIP_YEXC_B */
	1, /* GRIP_FIRST_EXC */
	1, /* GRIP_EXC_EDGE_ORIGIN */
	1, /* GRIP_ENABLE */
	1, /* FINGER_LIFTOFF_DEBOUNCE */
	1, /* GLOVE_LIFTOFF_DEBOUNCE */
	3, /* Reserved1145 */
	1, /* WATER_REJ_SNS_WIDTH */
	1, /* SLIM_POSITION_OFFSET_ALONG_TX */
	1, /* SLIM_POSITION_OFFSET_ALONG_RX */
	1, /* WET_FINGER_Z8_MULT */
	4, /* MIN_FF_Z9 */
	4, /* MAX_MF_Z9 */
	4, /* MIN_FF_SIG_SUM_EDGE */
	1, /* MF_CENTERSIG_RATIO */
	1, /* SD_SIZE_THRESH */
	2, /* SD_SIG_THRESH_ON */
	2, /* SD_SIG_THRESH_OFF */
	2, /* VP_DLT_RST_THRESH */
	2, /* VP_DLT_THRESH */
	1, /* FAT_AXIS_LENGTH_THRESH */
	1, /* PEAK_IGNORE_COEF */
	1, /* AXIS_ORIENTATION_ENABLE */
	3, /* Reserved1177 */
	1, /* CLIPPING_X_LOW */
	1, /* CLIPPING_X_HIGH */
	1, /* CLIPPING_Y_LOW */
	1, /* CLIPPING_Y_HIGH */
	2, /* CALC_THRESH */
	2, /* OFFSET_S1 */
	2, /* OFFSET_S2 */
	2, /* Z_SUM_8MM */
	2, /* Z_SUM_4MM */
	2, /* Z_SUM_3MM */
	2, /* Z_SUM_1MM */
	2, /* LOW_PIVOT */
	2, /* HIGH_PIVOT */
	2, /* LOW_PIVOT2 */
	2, /* HIGH_PIVOT2 */
	2, /* EDGE_DEBOUNCE_THRESH */
	2, /* CENTER_MAGNITUDE_SCALE */
	2, /* CENTROID_CORNER_GAIN */
	1, /* EDGE_DEBOUNCE_COUNT */
	1, /* BR2_ALWAYS_ON_FLAG */
	1, /* FINGER_SIZE_MIN_CHANGE_EDGE */
	1, /* Reserved1215 */
	1, /* Reserved1216 */
	1, /* GEST_PAN_ACTIVE_DISTANCE_X */
	1, /* GEST_PAN_ACTIVE_DISTANCE_Y */
	1, /* GEST_ZOOM_ACTIVE_DISTANCE */
	1, /* GEST_DEBOUNCE_MULTITOUCH */
	1, /* GEST_FLICK_ACTIVE_DISTANCE_X */
	1, /* GEST_FLICK_ACTIVE_DISTANCE_Y */
	1, /* GEST_FLICK_SAMPLE_TIME */
	1, /* GEST_DEBOUNCE_SINGLETOUCH_PAN_COUNT */
	1, /* GEST_MULTITOUCH_ROTATION_THRESHOLD */
	1, /* GEST_ROTATE_DEBOUNCE_LIMIT */
	1, /* Reserved1227 */
	1, /* GEST_ST_MAX_DOUBLE_CLICK_RADIUS */
	1, /* GEST_CLICK_X_RADIUS */
	1, /* GEST_CLICK_Y_RADIUS */
	1, /* Reserved1231 */
	2, /* GEST_MT_MAX_CLICK_TIMEOUT_MSEC */
	2, /* GEST_MT_MIN_CLICK_TIMEOUT_MSEC */
	2, /* GEST_ST_MAX_CLICK_TIMEOUT_MSEC */
	2, /* GEST_ST_MIN_CLICK_TIMEOUT_MSEC */
	2, /* GEST_ST_MAX_DOUBLECLICK_TIMEOUT_MSEC */
	2, /* GEST_ST_MIN_DOUBLECLICK_TIMEOUT_MSEC */
	2, /* GEST_RIGHTCLICK_MIN_TIMEOUT_MSEC */
	2, /* GEST_RIGHTCLICK_MAX_TIMEOUT_MSEC */
	2, /* GEST_SETTLING_TIMEOUT_MSEC */
	2, /* GEST_GROUP_MASK */
	2, /* TOUCHMODE_LFT_SELF_THRSH */
	2, /* X_RESOLUTION */
	2, /* Y_RESOLUTION */
	2, /* X_LENGTH_100xMM */
	2, /* Y_LENGTH_100xMM */
	1, /* X_PITCH_10xMM */
	1, /* Y_PITCH_10xMM */
	1, /* SENSOR_ASSIGNMENT */
	1, /* ACT_LFT_EN */
	1, /* TOUCHMODE_CONFIG */
	1, /* LRG_OBJ_CFG */
	1, /* MAX_REPORTED_TOUCH_NUM */
	1, /* OPMODE_CFG */
	1, /* Reserved1270 */
	1, /* Reserved1271 */
	1, /* Reserved1272 */
	1, /* GESTURE_ENABLED */
	2, /* Reserved1274 */
	1, /* LOW_POWER_ENABLE */
	1, /* ACT_INTRVL0 */
	2, /* ACT_LFT_INTRVL0 */
	2, /* LP_INTRVL0 */
	2, /* TCH_TMOUT0 */
	1, /* POST_CFG */
	1, /* Reserved1285 */
	2, /* CONFIG_VER */
	1, /* SEND_REPORT_AFTER_ACTIVE_INTERVAL_CFG */
	1, /* PIP_REPORTING_DISABLE */
	2, /* INTERRUPT_PIN_OVERRIDE */
	2, /* CONFIG_CRC */
};

/* Touchscreen Parameters Field Address*/
static const uint8_t cyttsp6_param_addr[] = {
/*	Address	Name	*/
	0xE1, 0x00, /* CONFIG_DATA_SIZE */
	0xE1, 0x02, /* CONFIG_DATA_MAX_SIZE */
	0xE1, 0x04, /* CROSS_NUM */
	0xE1, 0x06, /* TX_NUM */
	0xE1, 0x07, /* RX_NUM */
	0xE1, 0x08, /* SENS_NUM */
	0xE1, 0x09, /* BUTTON_NUM */
	0xE1, 0x0A, /* SLOTS_MUT */
	0xE1, 0x0B, /* SLOTS_SELF_RX */
	0xE1, 0x0C, /* SLOTS_SELF_TX */
	0xE1, 0x0D, /* SLOTS_SELF */
	0xE1, 0x0E, /* PROX_SCAN_AXIS */
	0xE1, 0x0F, /* SCANNING_MODE_BUTTON */
	0xE1, 0x10, /* SELF_Z_MODE */
	0xE1, 0x11, /* WATER_REJ_ENABLE */
	0xE1, 0x12, /* WF_ENABLE */
	0xE1, 0x13, /* CHARGER_ARMOR_ENABLE */
	0xE1, 0x14, /* CA_REVERT_TIME_MS */
	0xE1, 0x18, /* CA_TOUCH_REVERT_TIME_MS */
	0xE1, 0x1C, /* CA_TOUCH_REVERT_SIG_SUM_THRESH */
	0xE1, 0x20, /* CA_HOST_CTRL */
	0xE1, 0x21, /* CHARGER_STATUS */
	0xE1, 0x22, /* CA_TRIG_SRC */
	0xE1, 0x23, /* WB_CMF_ENABLE */
	0xE1, 0x24, /* WB_REVERT_THRESH */
	0xE1, 0x25, /* AFH_HOP_CYCLES_COUNT */
	0xE1, 0x26, /* NMI_SCAN_CNT */
	0xE1, 0x27, /* Reserved39 */
	0xE1, 0x28, /* CA_NMF_LIMIT */
	0xE1, 0x2C, /* NMI_TCH_MAGNITUDE */
	0xE1, 0x2E, /* NMI_TOUCH_THRESH */
	0xE1, 0x30, /* NMI_THRESH */
	0xE1, 0x32, /* WB_THRESH */
	0xE1, 0x34, /* SC_TRIG_THRESH */
	0xE1, 0x36, /* CA_DYN_CAL_SAFE_RAW_RANGE */
	0xE1, 0x38, /* CA_DYN_CAL_NUM_SENSOR_THLD_PERCENT */
	0xE1, 0x39, /* Reserved57 */
	0xE1, 0x3A, /* NMF_DETECT_THRESH */
	0xE1, 0x3B, /* NM_WB_SCAN_COUNT */
	0xE1, 0x3C, /* Reserved60 */
	0xE1, 0x3D, /* Reserved61 */
	0xE1, 0x3E, /* MAX_MUTUAL_SCAN_INTERVAL */
	0xE1, 0x40, /* MAX_SELF_SCAN_INTERVAL */
	0xE1, 0x42, /* Reserved66 */
	0xE1, 0x44, /* CDC_SENSOR_MASKS */
	0xE1, 0x74, /* CDC_PIN_INDEX_TABLE */
	0xE1, 0xD0, /* CDC_REAL_PIN_INDEX_TABLE */
	0xE2, 0x2D, /* Reserved301 */
	0xE2, 0x30, /* CDC_MTX_PHASE_VECTOR */
	0xE2, 0x38, /* CDC_MTX_DECONV_COEF */
	0xE2, 0x80, /* CDC_SLOT_TABLE */
	0xE3, 0xC0, /* Reserved704 */
	0xE3, 0xC1, /* Reserved705 */
	0xE3, 0xC2, /* Reserved706 */
	0xE3, 0xC4, /* Reserved708 */
	0xE3, 0xC6, /* MC_RAW_FILTER_MASK */
	0xE3, 0xC7, /* MC_RAW_IIR_COEF */
	0xE3, 0xC8, /* MC_RAW_IIR_THRESH */
	0xE3, 0xCA, /* MC_RAW_CMF_THRESH */
	0xE3, 0xCC, /* SC_RAW_FILTER_MASK */
	0xE3, 0xCD, /* SC_RAW_IIR_COEF */
	0xE3, 0xCE, /* SC_RAW_IIR_THRESH */
	0xE3, 0xD0, /* SC_RAW_CMF_THRESH */
	0xE3, 0xD2, /* BTN_MC_RAW_FILTER_MASK */
	0xE3, 0xD3, /* BTN_MC_RAW_IIR_COEF */
	0xE3, 0xD4, /* BTN_MC_RAW_IIR_THRESH */
	0xE3, 0xD6, /* BTN_MC_RAW_CMF_THRESH */
	0xE3, 0xD8, /* BTN_SC_RAW_FILTER_MASK */
	0xE3, 0xD9, /* BTN_SC_RAW_IIR_COEF */
	0xE3, 0xDA, /* BTN_SC_RAW_IIR_THRESH */
	0xE3, 0xDC, /* BTN_SC_RAW_CMF_THRESH */
	0xE3, 0xDE, /* Reserved734 */
	0xE3, 0xDF, /* Reserved735 */
	0xE3, 0xE0, /* Reserved736 */
	0xE3, 0xE2, /* Reserved738 */
	0xE3, 0xE4, /* CA_MC_RAW_FILTER_MASK */
	0xE3, 0xE5, /* CA_MC_RAW_IIR_COEF */
	0xE3, 0xE6, /* CA_MC_RAW_IIR_THRESH */
	0xE3, 0xE8, /* CA_MC_RAW_CMF_THRESH */
	0xE3, 0xEA, /* CA_BTN_MC_RAW_FILTER_MASK */
	0xE3, 0xEB, /* CA_BTN_MC_RAW_IIR_COEFF_BUT */
	0xE3, 0xEC, /* CA_BTN_MC_RAW_IIR_THRESH */
	0xE3, 0xEE, /* CA_BTN_MC_RAW_CMF_THRESH */
	0xE3, 0xF0, /* GLOVE_MC_RAW_FILTER_MASK */
	0xE3, 0xF1, /* GLOVE_MC_RAW_IIR_COEF */
	0xE3, 0xF2, /* GLOVE_MC_RAW_IIR_THRESH */
	0xE3, 0xF4, /* GLOVE_MC_RAW_CMF_THRESH */
	0xE3, 0xF6, /* GLOVE_SC_RAW_FILTER_MASK */
	0xE3, 0xF7, /* GLOVE_SC_RAW_IIR_COEF */
	0xE3, 0xF8, /* GLOVE_SC_RAW_IIR_THRESH */
	0xE3, 0xFA, /* GLOVE_SC_RAW_CMF_THRESH */
	0xE3, 0xFC, /* BTN_GLOVE_MC_RAW_FILTER_MASK */
	0xE3, 0xFD, /* BTN_GLOVE_MC_RAW_IIR_COEF */
	0xE3, 0xFE, /* BTN_GLOVE_MC_RAW_IIR_THRESH */
	0xE4, 0x00, /* BTN_GLOVE_MC_RAW_CMF_THRESH */
	0xE4, 0x02, /* BTN_GLOVE_SC_RAW_FILTER_MASK */
	0xE4, 0x03, /* BTN_GLOVE_SC_RAW_IIR_COEF */
	0xE4, 0x04, /* BTN_GLOVE_SC_RAW_IIR_THRESH */
	0xE4, 0x06, /* BTN_GLOVE_SC_RAW_CMF_THRESH */
	0xE4, 0x08, /* Reserved776 */
	0xE4, 0x09, /* Reserved777 */
	0xE4, 0x0A, /* Reserved778 */
	0xE4, 0x0B, /* Reserved779 */
	0xE4, 0x0C, /* Reserved780 */
	0xE4, 0x0D, /* Reserved781 */
	0xE4, 0x0E, /* Reserved782 */
	0xE4, 0x0F, /* Reserved783 */
	0xE4, 0x10, /* Reserved784 */
	0xE4, 0x11, /* Reserved785 */
	0xE4, 0x12, /* Reserved786 */
	0xE4, 0x13, /* Reserved787 */
	0xE4, 0x14, /* WF_RAW_CALC_THRESH */
	0xE4, 0x16, /* WF_DIFF_CALC_THRESH */
	0xE4, 0x18, /* WF_RAW_VAR_THRESH */
	0xE4, 0x1A, /* WF_DIFF_VAR_THRESH */
	0xE4, 0x1C, /* WF_LEVEL_THRESH */
	0xE4, 0x1E, /* WF_ENTER_DEBOUNCE */
	0xE4, 0x1F, /* WF_EXIT_DEBOUNCE */
	0xE4, 0x20, /* FINGER_BL_SNS_WIDTH */
	0xE4, 0x21, /* FINGER_BL_UPDATE_SPEED */
	0xE4, 0x22, /* FINGER_BL_THRESH_MC */
	0xE4, 0x24, /* FINGER_BL_THRESH_SC */
	0xE4, 0x26, /* GLOVE_BL_SNS_WIDTH */
	0xE4, 0x27, /* GLOVE_BL_UPDATE_SPEED */
	0xE4, 0x28, /* GLOVE_BL_THRESH_MC */
	0xE4, 0x2A, /* GLOVE_BL_THRESH_SC */
	0xE4, 0x2C, /* Reserved812 */
	0xE4, 0x2D, /* Reserved813 */
	0xE4, 0x2E, /* Reserved814 */
	0xE4, 0x2F, /* Reserved815 */
	0xE4, 0x30, /* Reserved816 */
	0xE4, 0x31, /* Reserved817 */
	0xE4, 0x32, /* BL_THRESH_BTN_MC */
	0xE4, 0x34, /* BL_THRESH_BTN_SC */
	0xE4, 0x36, /* Reserved822 */
	0xE4, 0x38, /* PQ_CTRL */
	0xE4, 0x3C, /* PQ_CTRL2 */
	0xE4, 0x40, /* PQ_CTRL3 */
	0xE4, 0x44, /* REFGEN_CTL */
	0xE4, 0x48, /* TX_CTRL */
	0xE4, 0x4C, /* RX_CTRL */
	0xE4, 0x50, /* INFRA_CTRL */
	0xE4, 0x54, /* STARTUP_DELAY */
	0xE4, 0x55, /* FORCE_SINGLE_TX */
	0xE4, 0x56, /* SCALE_FACT_MC */
	0xE4, 0x58, /* SCALE_FACT_SC */
	0xE4, 0x5A, /* SCALE_FACT_BTN_MC */
	0xE4, 0x5C, /* SCALE_FACT_BTN_SC */
	0xE4, 0x5E, /* Reserved862 */
	0xE4, 0x60, /* TX_PUMP_VOLTAGE */
	0xE4, 0x61, /* DISCARD_TIME */
	0xE4, 0x62, /* VDDA_MODE */
	0xE4, 0x63, /* BUTTON_LAYOUT */
	0xE4, 0x64, /* MTX_ORDER */
	0xE4, 0x65, /* EXT_SYNC */
	0xE4, 0x66, /* TX_FREQ_METHOD_MC */
	0xE4, 0x67, /* TX_FREQ_METHOD_SC */
	0xE4, 0x68, /* Reserved872 */
	0xE4, 0x69, /* Reserved873 */
	0xE4, 0x6A, /* Reserved874 */
	0xE4, 0x6B, /* NM_WB_IDAC */
	0xE4, 0x6C, /* SAFE_RAW_RANGE_PERCENT_MC */
	0xE4, 0x6D, /* SAFE_RAW_RANGE_PERCENT_SC */
	0xE4, 0x6E, /* SAFE_RAW_RANGE_PERCENT_MC_BTN */
	0xE4, 0x6F, /* SAFE_RAW_RANGE_PERCENT_SC_BTN */
	0xE4, 0x70, /* INT_VOLTAGE_MC */
	0xE4, 0x72, /* INT_VOLTAGE_SC */
	0xE4, 0x74, /* INT_VOLTAGE_MC_BTN */
	0xE4, 0x76, /* INT_VOLTAGE_SC_BTN */
	0xE4, 0x78, /* BAL_TARGET_MC */
	0xE4, 0x79, /* BAL_TARGET_SC */
	0xE4, 0x7A, /* BAL_TARGET_MC_BTN */
	0xE4, 0x7B, /* BAL_TARGET_SC_BTN */
	0xE4, 0x7C, /* ILEAK_MAX */
	0xE4, 0x7E, /* VDDA_LEVEL */
	0xE4, 0x80, /* PUMP_DELAY_US */
	0xE4, 0x82, /* MC_PWC_LIMIT_PERCENT */
	0xE4, 0x83, /* SC_PWC_LIMIT_PERCENT */
	0xE4, 0x84, /* HW_BL_GIDAC_LSB_CONFIG */
	0xE4, 0x85, /* Reserved901 */
	0xE4, 0x86, /* TX_PERIOD_MC */
	0xE4, 0x88, /* CA_HOP0_TX_PERIOD_MC */
	0xE4, 0x8A, /* CA_HOP1_TX_PERIOD_MC */
	0xE4, 0x8C, /* CA_HOP2_TX_PERIOD_MC */
	0xE4, 0x8E, /* TX_PERIOD_SC */
	0xE4, 0x90, /* TX_PERIOD_BTN_MC */
	0xE4, 0x92, /* TX_PERIOD_BTN_SC */
	0xE4, 0x94, /* TX_PULSES_MC */
	0xE4, 0x96, /* CA_MC_BASE_TX_PULSES_NUM */
	0xE4, 0x98, /* CA_HOP0_TX_PULSES_MC */
	0xE4, 0x9A, /* CA_HOP1_TX_PULSES_MC */
	0xE4, 0x9C, /* CA_HOP2_TX_PULSES_MC */
	0xE4, 0x9E, /* TX_PULSES_SC */
	0xE4, 0xA0, /* TX_PULSES_BTN_MC */
	0xE4, 0xA2, /* TX_PULSES_BTN_SC */
	0xE4, 0xA4, /* Reserved932 */
	0xE4, 0xA6, /* TX_PULSES_GLOVE_MC */
	0xE4, 0xA8, /* TX_PULSES_GLOVE_SC */
	0xE4, 0xAA, /* TX_PULSES_BTN_GLOVE_MC */
	0xE4, 0xAC, /* TX_PULSES_BTN_GLOVE_SC */
	0xE4, 0xAE, /* Reserved942 */
	0xE4, 0xB0, /* Reserved944 */
	0xE4, 0xB2, /* RX_ATTEN_RES_BYPASS */
	0xE4, 0xB3, /* TX_SPREADER_STEP */
	0xE4, 0xB4, /* TX_SPREADER_PULSES */
	0xE4, 0xB5, /* Reserved949 */
	0xE4, 0xB8, /* BTN_LS_ON_THRSH_MUT_0 */
	0xE4, 0xB9, /* BTN_LS_ON_THRSH_MUT_1 */
	0xE4, 0xBA, /* BTN_LS_ON_THRSH_MUT_2 */
	0xE4, 0xBB, /* BTN_LS_ON_THRSH_MUT_3 */
	0xE4, 0xBC, /* BTN_LS_OFF_THRSH_MUT_0 */
	0xE4, 0xBD, /* BTN_LS_OFF_THRSH_MUT_1 */
	0xE4, 0xBE, /* BTN_LS_OFF_THRSH_MUT_2 */
	0xE4, 0xBF, /* BTN_LS_OFF_THRSH_MUT_3 */
	0xE4, 0xC0, /* BTN_LS_ON_THRSH_SELF_0 */
	0xE4, 0xC1, /* BTN_LS_ON_THRSH_SELF_1 */
	0xE4, 0xC2, /* BTN_LS_ON_THRSH_SELF_2 */
	0xE4, 0xC3, /* BTN_LS_ON_THRSH_SELF_3 */
	0xE4, 0xC4, /* BTN_LS_OFF_THRSH_SELF_0 */
	0xE4, 0xC5, /* BTN_LS_OFF_THRSH_SELF_1 */
	0xE4, 0xC6, /* BTN_LS_OFF_THRSH_SELF_2 */
	0xE4, 0xC7, /* BTN_LS_OFF_THRSH_SELF_3 */
	0xE4, 0xC8, /* BTN_LS_TD_DEBOUNCE */
	0xE4, 0xC9, /* Reserved969 */
	0xE4, 0xCA, /* Reserved970 */
	0xE4, 0xCC, /* BTN_HS_ON_THRSH_MUT_0 */
	0xE4, 0xCD, /* BTN_HS_ON_THRSH_MUT_1 */
	0xE4, 0xCE, /* BTN_HS_ON_THRSH_MUT_2 */
	0xE4, 0xCF, /* BTN_HS_ON_THRSH_MUT_3 */
	0xE4, 0xD0, /* BTN_HS_OFF_THRSH_MUT_0 */
	0xE4, 0xD1, /* BTN_HS_OFF_THRSH_MUT_1 */
	0xE4, 0xD2, /* BTN_HS_OFF_THRSH_MUT_2 */
	0xE4, 0xD3, /* BTN_HS_OFF_THRSH_MUT_3 */
	0xE4, 0xD4, /* BTN_HS_ON_THRSH_SELF_0 */
	0xE4, 0xD5, /* BTN_HS_ON_THRSH_SELF_1 */
	0xE4, 0xD6, /* BTN_HS_ON_THRSH_SELF_2 */
	0xE4, 0xD7, /* BTN_HS_ON_THRSH_SELF_3 */
	0xE4, 0xD8, /* BTN_HS_OFF_THRSH_SELF_0 */
	0xE4, 0xD9, /* BTN_HS_OFF_THRSH_SELF_1 */
	0xE4, 0xDA, /* BTN_HS_OFF_THRSH_SELF_2 */
	0xE4, 0xDB, /* BTN_HS_OFF_THRSH_SELF_3 */
	0xE4, 0xDC, /* BTN_HS_TOUCHDOWN_DEBOUNCE */
	0xE4, 0xDD, /* Reserved989 */
	0xE4, 0xDE, /* Reserved990 */
	0xE4, 0xE0, /* BTN_HIGHSEN_MODE_THRSH_MUT */
	0xE4, 0xE2, /* BTN_HIGHSEN_MODE_THRSH_SELF */
	0xE4, 0xE4, /* BTN_LOWSEN_MODE_THRSH_MUT */
	0xE4, 0xE6, /* BTN_LOWSEN_MODE_THRSH_SELF */
	0xE4, 0xE8, /* GLOVE_BTN_FORBID_DEBOUNCE */
	0xE4, 0xE9, /* GLOVE_BTN_MODE_SWITCH_DEBOUNCE */
	0xE4, 0xEA, /* BTN_PROCESS_IF_TOUCH_DETECTED */
	0xE4, 0xEB, /* Reserved1003 */
	0xE4, 0xEC, /* FINGER_THRESH_MUT_HI */
	0xE4, 0xEE, /* FINGER_THRESH_MUT_LO */
	0xE4, 0xF0, /* FINGER_THRESH_SELF */
	0xE4, 0xF2, /* FINGER_Z9_FILT_SCALE */
	0xE4, 0xF3, /* FINGER_Z8_FILT_SCALE */
	0xE4, 0xF4, /* MIN_FAT_FINGER_SIZE */
	0xE4, 0xF5, /* MIN_FAT_FINGER_SIZE_HYST */
	0xE4, 0xF6, /* MAX_FAT_FINGER_SIZE */
	0xE4, 0xF7, /* MAX_FAT_FINGER_SIZE_HYST */
	0xE4, 0xF8, /* FINGER_SIG_THRESH_MULT */
	0xE4, 0xF9, /* FINGER_OBJECT_FEATURES */
	0xE4, 0xFA, /* FINGER_Z_SCALE */
	0xE4, 0xFC, /* FINGER_INNER_EDGE_GAIN */
	0xE4, 0xFD, /* FINGER_OUTER_EDGE_GAIN */
	0xE4, 0xFE, /* FINGER_POS_CALC_METHOD */
	0xE4, 0xFF, /* Reserved1023 */
	0xE5, 0x00, /* CA_FINGER_THRESH_MUT */
	0xE5, 0x02, /* FINGER_MT_DEBOUNCE */
	0xE5, 0x03, /* CA_FINGER_MT_DEBOUNCE */
	0xE5, 0x04, /* CA_FINGER_Z9_FILT_SCALE */
	0xE5, 0x05, /* CA_MIN_FAT_FINGER_SIZE */
	0xE5, 0x06, /* CA_MAX_FAT_FINGER_SIZE */
	0xE5, 0x07, /* WF_THRESH_MUT_COEF */
	0xE5, 0x08, /* WF_MT_DEBOUNCE */
	0xE5, 0x09, /* RX_LINE_FILT_ENABLE */
	0xE5, 0x0A, /* RX_LINE_FILT_DEBOUNCE */
	0xE5, 0x0B, /* RX_LINE_FILT_THRESH */
	0xE5, 0x0C, /* TOUCHMODE_FRAME_NUM_TO_CONFIRM_FINGER_MODE */
	0xE5, 0x0D, /* Reserved1037 */
	0xE5, 0x10, /* GLOVES_THRESH_MUT_HI */
	0xE5, 0x12, /* GLOVES_THRESH_MUT_LO */
	0xE5, 0x14, /* GLOVES_THRESH_SELF */
	0xE5, 0x16, /* GLOVES_Z9_FILT_SCALE */
	0xE5, 0x17, /* GLOVES_Z8_FILT_SCALE */
	0xE5, 0x18, /* GLOVES_MIN_FAT_SIZE */
	0xE5, 0x19, /* GLOVES_MIN_FAT_SIZE_HYST */
	0xE5, 0x1A, /* GLOVES_MAX_FAT_SIZE */
	0xE5, 0x1B, /* GLOVES_MAX_FAT_SIZE_HYST */
	0xE5, 0x1C, /* GLOVES_SIG_THRESH_MULT */
	0xE5, 0x1D, /* GLOVES_OBJECT_FEATURES */
	0xE5, 0x1E, /* GLOVES_Z_SCALE */
	0xE5, 0x20, /* GLOVES_INNER_EDGE_GAIN */
	0xE5, 0x21, /* GLOVES_OUTER_EDGE_GAIN */
	0xE5, 0x22, /* GLOVES_POS_CALC_METHOD */
	0xE5, 0x23, /* Reserved1059 */
	0xE5, 0x24, /* TOUCHMODE_GLOVE_HTI */
	0xE5, 0x26, /* TOUCHMODE_GLOVE_MAX_SIG_SUM */
	0xE5, 0x28, /* GLOVES_FT_DEBOUNCE */
	0xE5, 0x29, /* GLOVES_FT_DEBOUNCE_EDGE_MASK */
	0xE5, 0x2A, /* GLOVES_MT_DEBOUNCE */
	0xE5, 0x2B, /* GLOVES_GRIP_FILT_SCALE */
	0xE5, 0x2C, /* TOUCHMODE_FRAME_NUM_TO_CONFIRM_GLOVE_MODE */
	0xE5, 0x2D, /* Reserved1069 */
	0xE5, 0x30, /* TOUCHMODE_FINGER_SWITCH_DEBOUNCE */
	0xE5, 0x32, /* TOUCHMODE_FINGER_EXIT_DELAY */
	0xE5, 0x34, /* TOUCHMODE_GLOVE_SWITCH_DEBOUNCE */
	0xE5, 0x36, /* TOUCHMODE_GLOVE_EXIT_DELAY */
	0xE5, 0x38, /* TOUCHMODE_GLOVE_FINGER_SWITCH_DEBOUNCE */
	0xE5, 0x3A, /* Reserved1082 */
	0xE5, 0x3C, /* ACT_DIST0_SQR */
	0xE5, 0x3E, /* ACT_DIST2_SQR */
	0xE5, 0x40, /* ACT_DIST_TOUCHDOWN_SQR */
	0xE5, 0x42, /* ACT_DIST_LIFTOFF_SQR */
	0xE5, 0x44, /* ACT_DIST_Z_THRESHOLD */
	0xE5, 0x45, /* LARGE_OBJ_CFG */
	0xE5, 0x46, /* Reserved1094 */
	0xE5, 0x48, /* XY_FILTER_MASK */
	0xE5, 0x49, /* XY_FILT_IIR_COEF_SLOW */
	0xE5, 0x4A, /* XY_FILT_IIR_COEF_FAST */
	0xE5, 0x4B, /* XY_FILT_XY_THR_SLOW */
	0xE5, 0x4C, /* XY_FILT_XY_THR_FAST */
	0xE5, 0x4D, /* XY_FILT_Z_IIR_COEFF */
	0xE5, 0x4E, /* XY_FILT_PREDICTION_COEF */
	0xE5, 0x4F, /* Reserved1103 */
	0xE5, 0x50, /* XY_FILTER_MASK_CA */
	0xE5, 0x51, /* XY_FILT_IIR_COEF_SLOW_CA */
	0xE5, 0x52, /* XY_FILT_IIR_COEF_FAST_CA */
	0xE5, 0x53, /* XY_FILT_XY_THR_SLOW_CA */
	0xE5, 0x54, /* XY_FILT_XY_THR_FAST_CA */
	0xE5, 0x55, /* XY_FILT_Z_IIR_COEFF_CA */
	0xE5, 0x56, /* XY_FILT_PREDICTION_COEF_CA */
	0xE5, 0x57, /* Reserved1111 */
	0xE5, 0x58, /* XY_FILT_AXIS_IIR_COEF */
	0xE5, 0x59, /* XY_FILT_AXIS_HYST */
	0xE5, 0x5A, /* XY_FILT_ANGLE_IIR_COEF */
	0xE5, 0x5B, /* XY_FILT_ANGLE_HYST */
	0xE5, 0x5C, /* MAX_VELOCITY_SQR */
	0xE5, 0x60, /* FINGER_ID_MAX_FINGER_ACCELERATION2 */
	0xE5, 0x64, /* GRIP_XEDG_A */
	0xE5, 0x66, /* GRIP_XEDG_B */
	0xE5, 0x68, /* GRIP_XEXC_A */
	0xE5, 0x6A, /* GRIP_XEXC_B */
	0xE5, 0x6C, /* GRIP_YEDG_A */
	0xE5, 0x6E, /* GRIP_YEDG_B */
	0xE5, 0x70, /* GRIP_YEXC_A */
	0xE5, 0x72, /* GRIP_YEXC_B */
	0xE5, 0x74, /* GRIP_FIRST_EXC */
	0xE5, 0x75, /* GRIP_EXC_EDGE_ORIGIN */
	0xE5, 0x76, /* GRIP_ENABLE */
	0xE5, 0x77, /* FINGER_LIFTOFF_DEBOUNCE */
	0xE5, 0x78, /* GLOVE_LIFTOFF_DEBOUNCE */
	0xE5, 0x79, /* Reserved1145 */
	0xE5, 0x7C, /* WATER_REJ_SNS_WIDTH */
	0xE5, 0x7D, /* SLIM_POSITION_OFFSET_ALONG_TX */
	0xE5, 0x7E, /* SLIM_POSITION_OFFSET_ALONG_RX */
	0xE5, 0x7F, /* WET_FINGER_Z8_MULT */
	0xE5, 0x80, /* MIN_FF_Z9 */
	0xE5, 0x84, /* MAX_MF_Z9 */
	0xE5, 0x88, /* MIN_FF_SIG_SUM_EDGE */
	0xE5, 0x8C, /* MF_CENTERSIG_RATIO */
	0xE5, 0x8D, /* SD_SIZE_THRESH */
	0xE5, 0x8E, /* SD_SIG_THRESH_ON */
	0xE5, 0x90, /* SD_SIG_THRESH_OFF */
	0xE5, 0x92, /* VP_DLT_RST_THRESH */
	0xE5, 0x94, /* VP_DLT_THRESH */
	0xE5, 0x96, /* FAT_AXIS_LENGTH_THRESH */
	0xE5, 0x97, /* PEAK_IGNORE_COEF */
	0xE5, 0x98, /* AXIS_ORIENTATION_ENABLE */
	0xE5, 0x99, /* Reserved1177 */
	0xE5, 0x9C, /* CLIPPING_X_LOW */
	0xE5, 0x9D, /* CLIPPING_X_HIGH */
	0xE5, 0x9E, /* CLIPPING_Y_LOW */
	0xE5, 0x9F, /* CLIPPING_Y_HIGH */
	0xE5, 0xA0, /* CALC_THRESH */
	0xE5, 0xA2, /* OFFSET_S1 */
	0xE5, 0xA4, /* OFFSET_S2 */
	0xE5, 0xA6, /* Z_SUM_8MM */
	0xE5, 0xA8, /* Z_SUM_4MM */
	0xE5, 0xAA, /* Z_SUM_3MM */
	0xE5, 0xAC, /* Z_SUM_1MM */
	0xE5, 0xAE, /* LOW_PIVOT */
	0xE5, 0xB0, /* HIGH_PIVOT */
	0xE5, 0xB2, /* LOW_PIVOT2 */
	0xE5, 0xB4, /* HIGH_PIVOT2 */
	0xE5, 0xB6, /* EDGE_DEBOUNCE_THRESH */
	0xE5, 0xB8, /* CENTER_MAGNITUDE_SCALE */
	0xE5, 0xBA, /* CENTROID_CORNER_GAIN */
	0xE5, 0xBC, /* EDGE_DEBOUNCE_COUNT */
	0xE5, 0xBD, /* BR2_ALWAYS_ON_FLAG */
	0xE5, 0xBE, /* FINGER_SIZE_MIN_CHANGE_EDGE */
	0xE5, 0xBF, /* Reserved1215 */
	0xE5, 0xC0, /* Reserved1216 */
	0xE5, 0xC1, /* GEST_PAN_ACTIVE_DISTANCE_X */
	0xE5, 0xC2, /* GEST_PAN_ACTIVE_DISTANCE_Y */
	0xE5, 0xC3, /* GEST_ZOOM_ACTIVE_DISTANCE */
	0xE5, 0xC4, /* GEST_DEBOUNCE_MULTITOUCH */
	0xE5, 0xC5, /* GEST_FLICK_ACTIVE_DISTANCE_X */
	0xE5, 0xC6, /* GEST_FLICK_ACTIVE_DISTANCE_Y */
	0xE5, 0xC7, /* GEST_FLICK_SAMPLE_TIME */
	0xE5, 0xC8, /* GEST_DEBOUNCE_SINGLETOUCH_PAN_COUNT */
	0xE5, 0xC9, /* GEST_MULTITOUCH_ROTATION_THRESHOLD */
	0xE5, 0xCA, /* GEST_ROTATE_DEBOUNCE_LIMIT */
	0xE5, 0xCB, /* Reserved1227 */
	0xE5, 0xCC, /* GEST_ST_MAX_DOUBLE_CLICK_RADIUS */
	0xE5, 0xCD, /* GEST_CLICK_X_RADIUS */
	0xE5, 0xCE, /* GEST_CLICK_Y_RADIUS */
	0xE5, 0xCF, /* Reserved1231 */
	0xE5, 0xD0, /* GEST_MT_MAX_CLICK_TIMEOUT_MSEC */
	0xE5, 0xD2, /* GEST_MT_MIN_CLICK_TIMEOUT_MSEC */
	0xE5, 0xD4, /* GEST_ST_MAX_CLICK_TIMEOUT_MSEC */
	0xE5, 0xD6, /* GEST_ST_MIN_CLICK_TIMEOUT_MSEC */
	0xE5, 0xD8, /* GEST_ST_MAX_DOUBLECLICK_TIMEOUT_MSEC */
	0xE5, 0xDA, /* GEST_ST_MIN_DOUBLECLICK_TIMEOUT_MSEC */
	0xE5, 0xDC, /* GEST_RIGHTCLICK_MIN_TIMEOUT_MSEC */
	0xE5, 0xDE, /* GEST_RIGHTCLICK_MAX_TIMEOUT_MSEC */
	0xE5, 0xE0, /* GEST_SETTLING_TIMEOUT_MSEC */
	0xE5, 0xE2, /* GEST_GROUP_MASK */
	0xE5, 0xE4, /* TOUCHMODE_LFT_SELF_THRSH */
	0xE5, 0xE6, /* X_RESOLUTION */
	0xE5, 0xE8, /* Y_RESOLUTION */
	0xE5, 0xEA, /* X_LENGTH_100xMM */
	0xE5, 0xEC, /* Y_LENGTH_100xMM */
	0xE5, 0xEE, /* X_PITCH_10xMM */
	0xE5, 0xEF, /* Y_PITCH_10xMM */
	0xE5, 0xF0, /* SENSOR_ASSIGNMENT */
	0xE5, 0xF1, /* ACT_LFT_EN */
	0xE5, 0xF2, /* TOUCHMODE_CONFIG */
	0xE5, 0xF3, /* LRG_OBJ_CFG */
	0xE5, 0xF4, /* MAX_REPORTED_TOUCH_NUM */
	0xE5, 0xF5, /* OPMODE_CFG */
	0xE5, 0xF6, /* Reserved1270 */
	0xE5, 0xF7, /* Reserved1271 */
	0xE5, 0xF8, /* Reserved1272 */
	0xE5, 0xF9, /* GESTURE_ENABLED */
	0xE5, 0xFA, /* Reserved1274 */
	0xE5, 0xFC, /* LOW_POWER_ENABLE */
	0xE5, 0xFD, /* ACT_INTRVL0 */
	0xE5, 0xFE, /* ACT_LFT_INTRVL0 */
	0xE6, 0x00, /* LP_INTRVL0 */
	0xE6, 0x02, /* TCH_TMOUT0 */
	0xE6, 0x04, /* POST_CFG */
	0xE6, 0x05, /* Reserved1285 */
	0xE6, 0x06, /* CONFIG_VER */
	0xE6, 0x08, /* SEND_REPORT_AFTER_ACTIVE_INTERVAL_CFG */
	0xE6, 0x09, /* PIP_REPORTING_DISABLE */
	0xE6, 0x0A, /* INTERRUPT_PIN_OVERRIDE */
	0xE6, 0x0C, /* CONFIG_CRC */
};

