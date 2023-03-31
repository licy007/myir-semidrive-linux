
#ifndef _SDRV_ADC_REG_H
#define _SDRV_ADC_REG_H
#ifndef BIT_
#define BIT_(nr) (1UL << (nr))
#endif
//--------------------------------------------------------------------------
// IP Ref Info     : REG_AP_APB_ADC
// RTL version     :
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// Address Block Name : ADC_APB_AB0
// Description        :
//--------------------------------------------------------------------------
#define ADC_APB_AB0_BASE_ADDR 0x0
//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_CTRL
// Register Offset : 0x0
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_CTRL (ADC_APB_AB0_BASE_ADDR + (0x0<<0))
#define ADC_CTRL_DWC_SW_RST_FIELD_OFFSET 17
#define ADC_CTRL_DWC_SW_RST_FIELD_SIZE 1
#define ADC_CTRL_SW_RST_FIELD_OFFSET 16
#define ADC_CTRL_SW_RST_FIELD_SIZE 1
#define ADC_CTRL_POWERDOWN_FIELD_OFFSET 9
#define ADC_CTRL_POWERDOWN_FIELD_SIZE 1
#define ADC_CTRL_STANDBY_FIELD_OFFSET 8
#define ADC_CTRL_STANDBY_FIELD_SIZE 1
#define ADC_CTRL_DIFF_EN_FIELD_OFFSET 5
#define ADC_CTRL_DIFF_EN_FIELD_SIZE 1
#define ADC_CTRL_SELRES_FIELD_OFFSET 3
#define ADC_CTRL_SELRES_FIELD_SIZE 2
#define ADC_CTRL_SELBG_FIELD_OFFSET 2
#define ADC_CTRL_SELBG_FIELD_SIZE 1
#define ADC_CTRL_SELREF_FIELD_OFFSET 1
#define ADC_CTRL_SELREF_FIELD_SIZE 1
#define ADC_CTRL_ENADC_FIELD_OFFSET 0
#define ADC_CTRL_ENADC_FIELD_SIZE 1

#define BIT_AP_APB_ADC_ADC_CTRL_DWC_SW_RST    (BIT_(17))
#define BIT_AP_APB_ADC_ADC_CTRL_SW_RST    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_CTRL_POWERDOWN    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_CTRL_STANDBY    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_CTRL_DIFF_EN    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_CTRL_SELRES_1    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_CTRL_SELRES_0    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_CTRL_SELBG    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_CTRL_SELREF    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_CTRL_ENADC    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_CLK_CFG
// Register Offset : 0x4
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_CLK_CFG (ADC_APB_AB0_BASE_ADDR + (0x4<<0))
#define ADC_CLK_CFG_DWC_CLK_EN_FIELD_OFFSET 15
#define ADC_CLK_CFG_DWC_CLK_EN_FIELD_SIZE 1
#define ADC_CLK_CFG_DIV_NUM_FIELD_OFFSET 2
#define ADC_CLK_CFG_DIV_NUM_FIELD_SIZE 8
#define ADC_CLK_CFG_SRC_SEL_FIELD_OFFSET 0
#define ADC_CLK_CFG_SRC_SEL_FIELD_SIZE 2

#define BIT_AP_APB_ADC_ADC_CLK_CFG_DWC_CLK_EN    (BIT_(15))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_7    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_6    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_5    (BIT_(7))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_4    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_3    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_2    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_1    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_DIV_NUM_0    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_SRC_SEL_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_CLK_CFG_SRC_SEL_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_TIMER
// Register Offset : 0x8
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_TIMER (ADC_APB_AB0_BASE_ADDR + (0x8<<0))
#define ADC_TIMER_TERMINAL_VAL_FIELD_OFFSET 16
#define ADC_TIMER_TERMINAL_VAL_FIELD_SIZE 16
#define ADC_TIMER_TS_EN_FIELD_OFFSET 2
#define ADC_TIMER_TS_EN_FIELD_SIZE 1
#define ADC_TIMER_AUTO_EN_FIELD_OFFSET 1
#define ADC_TIMER_AUTO_EN_FIELD_SIZE 1
#define ADC_TIMER_RELOAD_FIELD_OFFSET 0
#define ADC_TIMER_RELOAD_FIELD_SIZE 1

#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_15    (BIT_(31))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_14    (BIT_(30))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_13    (BIT_(29))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_12    (BIT_(28))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_11    (BIT_(27))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_10    (BIT_(26))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_9    (BIT_(25))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_8    (BIT_(24))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_7    (BIT_(23))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_6    (BIT_(22))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_5    (BIT_(21))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_4    (BIT_(20))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_3    (BIT_(19))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_2    (BIT_(18))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_1    (BIT_(17))
#define BIT_AP_APB_ADC_ADC_TIMER_TERMINAL_VAL_0    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_TIMER_TS_EN    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_TIMER_AUTO_EN    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_TIMER_RELOAD    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_TEST
// Register Offset : 0x10
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_TEST (ADC_APB_AB0_BASE_ADDR + (0x10<<0))
#define ADC_TEST_SEL_FIELD_OFFSET 4
#define ADC_TEST_SEL_FIELD_SIZE 3
#define ADC_TEST_ENCTR_FIELD_OFFSET 1
#define ADC_TEST_ENCTR_FIELD_SIZE 3
#define ADC_TEST_TEST_MODE_FIELD_OFFSET 0
#define ADC_TEST_TEST_MODE_FIELD_SIZE 1

#define BIT_AP_APB_ADC_ADC_TEST_SEL_2    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_TEST_SEL_1    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_TEST_SEL_0    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_TEST_ENCTR_2    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_TEST_ENCTR_1    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_TEST_ENCTR_0    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_TEST_TEST_MODE    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_IMASK
// Register Offset : 0x14
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_IMASK (ADC_APB_AB0_BASE_ADDR + (0x14<<0))
#define ADC_IMASK_WML_FIELD_OFFSET 1
#define ADC_IMASK_WML_FIELD_SIZE 1
#define ADC_IMASK_EOC_FIELD_OFFSET 0
#define ADC_IMASK_EOC_FIELD_SIZE 1
#define BIT_AP_APB_ADC_ADC_IMASK_TMR_OVF    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_IMASK_EOL    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_IMASK_CTC    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_IMASK_WML    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_IMASK_EOC    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_IFLAG
// Register Offset : 0x18
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_IFLAG (ADC_APB_AB0_BASE_ADDR + (0x18<<0))
#define ADC_IFLAG_DUMMY_FIELD_OFFSET 5
#define ADC_IFLAG_DUMMY_FIELD_SIZE 1
#define ADC_IFLAG_TMR_OVF_FIELD_OFFSET 4
#define ADC_IFLAG_TMR_OVF_FIELD_SIZE 1
#define ADC_IFLAG_EOL_FIELD_OFFSET 3
#define ADC_IFLAG_EOL_FIELD_SIZE 1
#define ADC_IFLAG_CTC_FIELD_OFFSET 2
#define ADC_IFLAG_CTC_FIELD_SIZE 1
#define ADC_IFLAG_WML_FIELD_OFFSET 1
#define ADC_IFLAG_WML_FIELD_SIZE 1
#define ADC_IFLAG_EOC_FIELD_OFFSET 0
#define ADC_IFLAG_EOC_FIELD_SIZE 1

#define BIT_AP_APB_ADC_ADC_IFLAG_DUMMY    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_IFLAG_TMR_OVF    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_IFLAG_EOL    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_IFLAG_CTC    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_IFLAG_WML    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_IFLAG_EOC    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_DMA_CFG
// Register Offset : 0x1c
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_DMA_CFG (ADC_APB_AB0_BASE_ADDR + (0x1c<<0))
#define ADC_DMA_CFG_FIFO_TRIG_VAL_FIELD_OFFSET 8
#define ADC_DMA_CFG_FIFO_TRIG_VAL_FIELD_SIZE 7
#define ADC_DMA_CFG_SW_ACK_FIELD_OFFSET 2
#define ADC_DMA_CFG_SW_ACK_FIELD_SIZE 1
#define ADC_DMA_CFG_SINGLE_EN_FIELD_OFFSET 1
#define ADC_DMA_CFG_SINGLE_EN_FIELD_SIZE 1
#define ADC_DMA_CFG_DMA_EN_FIELD_OFFSET 0
#define ADC_DMA_CFG_DMA_EN_FIELD_SIZE 1

#define BIT_AP_APB_ADC_ADC_DMA_CFG_FIFO_TRIG_VAL_6    (BIT_(14))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_FIFO_TRIG_VAL_5    (BIT_(13))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_FIFO_TRIG_VAL_4    (BIT_(12))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_FIFO_TRIG_VAL_3    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_FIFO_TRIG_VAL_2    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_FIFO_TRIG_VAL_1    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_FIFO_TRIG_VAL_0    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_SW_ACK    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_SINGLE_EN    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_DMA_CFG_DMA_EN    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_SSC
// Register Offset : 0x20
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_SSC (ADC_APB_AB0_BASE_ADDR + (0x20<<0))
#define ADC_SSC_INTERVAL_FIELD_OFFSET 8
#define ADC_SSC_INTERVAL_FIELD_SIZE 8
#define ADC_SSC_CONV_STOP_FIELD_OFFSET 4
#define ADC_SSC_CONV_STOP_FIELD_SIZE 1
#define ADC_SSC_CONV_START_FIELD_OFFSET 3
#define ADC_SSC_CONV_START_FIELD_SIZE 1
#define ADC_SSC_CONV_INIT_FIELD_OFFSET 2
#define ADC_SSC_CONV_INIT_FIELD_SIZE 1
#define ADC_SSC_CONV_MODE_FIELD_OFFSET 0
#define ADC_SSC_CONV_MODE_FIELD_SIZE 2

#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_7    (BIT_(15))
#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_6    (BIT_(14))
#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_5    (BIT_(13))
#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_4    (BIT_(12))
#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_3    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_2    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_1    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_SSC_INTERVAL_0    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_SSC_CONV_STOP    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_SSC_CONV_START    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_SSC_CONV_INIT    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_SSC_CONV_MODE_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_SSC_CONV_MODE_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_SETUP
// Register Offset : 0x24
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_SETUP (ADC_APB_AB0_BASE_ADDR + (0x24<<0))
#define ADC_SETUP_CFG3_FIELD_OFFSET 12
#define ADC_SETUP_CFG3_FIELD_SIZE 4
#define ADC_SETUP_CFG2_FIELD_OFFSET 8
#define ADC_SETUP_CFG2_FIELD_SIZE 4
#define ADC_SETUP_CFG1_FIELD_OFFSET 4
#define ADC_SETUP_CFG1_FIELD_SIZE 4
#define ADC_SETUP_CFG0_FIELD_OFFSET 0
#define ADC_SETUP_CFG0_FIELD_SIZE 4

#define BIT_AP_APB_ADC_ADC_SETUP_CFG3_3    (BIT_(15))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG3_2    (BIT_(14))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG3_1    (BIT_(13))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG3_0    (BIT_(12))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG2_3    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG2_2    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG2_1    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG2_0    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG1_3    (BIT_(7))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG1_2    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG1_1    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG1_0    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG0_3    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG0_2    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG0_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_SETUP_CFG0_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_SCHC
// Register Offset : 0x28
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_SCHC (ADC_APB_AB0_BASE_ADDR + (0x28<<0))
#define ADC_SCHC_CTC_FIELD_OFFSET 16
#define ADC_SCHC_CTC_FIELD_SIZE 16
#define ADC_SCHC_CSEL_FIELD_OFFSET 0
#define ADC_SCHC_CSEL_FIELD_SIZE 7

#define BIT_AP_APB_ADC_ADC_SCHC_CTC_15    (BIT_(31))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_14    (BIT_(30))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_13    (BIT_(29))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_12    (BIT_(28))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_11    (BIT_(27))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_10    (BIT_(26))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_9    (BIT_(25))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_8    (BIT_(24))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_7    (BIT_(23))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_6    (BIT_(22))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_5    (BIT_(21))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_4    (BIT_(20))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_3    (BIT_(19))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_2    (BIT_(18))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_1    (BIT_(17))
#define BIT_AP_APB_ADC_ADC_SCHC_CTC_0    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_SCHC_CSEL_6    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_SCHC_CSEL_5    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_SCHC_CSEL_4    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_SCHC_CSEL_3    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_SCHC_CSEL_2    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_SCHC_CSEL_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_SCHC_CSEL_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_MCHC
// Register Offset : 0x2c
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_MCHC (ADC_APB_AB0_BASE_ADDR + (0x2c<<0))
#define ADC_MCHC_REPEAT_FIELD_OFFSET 8
#define ADC_MCHC_REPEAT_FIELD_SIZE 4
#define ADC_MCHC_LOOP_END_FIELD_OFFSET 0
#define ADC_MCHC_LOOP_END_FIELD_SIZE 6

#define BIT_AP_APB_ADC_ADC_MCHC_REPEAT_3    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_MCHC_REPEAT_2    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_MCHC_REPEAT_1    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_MCHC_REPEAT_0    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_MCHC_LOOP_END_5    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_MCHC_LOOP_END_4    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_MCHC_LOOP_END_3    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_MCHC_LOOP_END_2    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_MCHC_LOOP_END_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_MCHC_LOOP_END_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_FIFO
// Register Offset : 0x30
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_FIFO (ADC_APB_AB0_BASE_ADDR + (0x30<<0))
#define ADC_FIFO_ENABLE_FIELD_OFFSET 31
#define ADC_FIFO_ENABLE_FIELD_SIZE 1
#define ADC_FIFO_FLUSH_FIELD_OFFSET 30
#define ADC_FIFO_FLUSH_FIELD_SIZE 1
#define ADC_FIFO_THRE_FIELD_OFFSET 16
#define ADC_FIFO_THRE_FIELD_SIZE 8
#define ADC_FIFO_IDLE_FIELD_OFFSET 10
#define ADC_FIFO_IDLE_FIELD_SIZE 1
#define ADC_FIFO_FULL_FIELD_OFFSET 9
#define ADC_FIFO_FULL_FIELD_SIZE 1
#define ADC_FIFO_EMPTY_FIELD_OFFSET 8
#define ADC_FIFO_EMPTY_FIELD_SIZE 1
#define ADC_FIFO_WML_FIELD_OFFSET 0
#define ADC_FIFO_WML_FIELD_SIZE 8

#define BIT_AP_APB_ADC_ADC_FIFO_ENABLE    (BIT_(31))
#define BIT_AP_APB_ADC_ADC_FIFO_FLUSH    (BIT_(30))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_7    (BIT_(23))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_6    (BIT_(22))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_5    (BIT_(21))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_4    (BIT_(20))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_3    (BIT_(19))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_2    (BIT_(18))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_1    (BIT_(17))
#define BIT_AP_APB_ADC_ADC_FIFO_THRE_0    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_FIFO_IDLE    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_FIFO_FULL    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_FIFO_EMPTY    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_7    (BIT_(7))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_6    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_5    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_4    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_3    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_2    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_FIFO_WML_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_RSLT
// Register Offset : 0x40
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_RSLT (ADC_APB_AB0_BASE_ADDR + (0x100<<0))
#define ADC_RSLT_TS_IND_FIELD_OFFSET 31
#define ADC_RSLT_TS_IND_FIELD_SIZE 1
#define ADC_RSLT_CSEL_FIELD_OFFSET 16
#define ADC_RSLT_CSEL_FIELD_SIZE 7
#define ADC_RSLT_VALUE_FIELD_OFFSET 0
#define ADC_RSLT_VALUE_FIELD_SIZE 16

#define BIT_AP_APB_ADC_ADC_RSLT_TS_IND    (BIT_(31))
#define BIT_AP_APB_ADC_ADC_RSLT_CSEL_6    (BIT_(22))
#define BIT_AP_APB_ADC_ADC_RSLT_CSEL_5    (BIT_(21))
#define BIT_AP_APB_ADC_ADC_RSLT_CSEL_4    (BIT_(20))
#define BIT_AP_APB_ADC_ADC_RSLT_CSEL_3    (BIT_(19))
#define BIT_AP_APB_ADC_ADC_RSLT_CSEL_2    (BIT_(18))
#define BIT_AP_APB_ADC_ADC_RSLT_CSEL_1    (BIT_(17))
#define BIT_AP_APB_ADC_ADC_RSLT_CSEL_0    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_15    (BIT_(15))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_14    (BIT_(14))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_13    (BIT_(13))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_12    (BIT_(12))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_11    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_10    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_9    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_8    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_7    (BIT_(7))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_6    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_5    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_4    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_3    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_2    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_RSLT_VALUE_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_ENTRY
// Register Offset : 0x100
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_ENTRY (ADC_APB_AB0_BASE_ADDR + (0x300<<0))
#define ADC_ENTRY_ENABLE_FIELD_OFFSET 23
#define ADC_ENTRY_ENABLE_FIELD_SIZE 1
#define ADC_ENTRY_SKIP_FIELD_OFFSET 22
#define ADC_ENTRY_SKIP_FIELD_SIZE 1
#define ADC_ENTRY_DUMMY_FIELD_OFFSET 21
#define ADC_ENTRY_DUMMY_FIELD_SIZE 1
#define ADC_ENTRY_SETUP_SEL_FIELD_OFFSET 16
#define ADC_ENTRY_SETUP_SEL_FIELD_SIZE 2
#define ADC_ENTRY_CSEL_FIELD_OFFSET 8
#define ADC_ENTRY_CSEL_FIELD_SIZE 7
#define ADC_ENTRY_RCT_FIELD_OFFSET 0
#define ADC_ENTRY_RCT_FIELD_SIZE 8

#define BIT_AP_APB_ADC_ADC_ENTRY_ENABLE    (BIT_(23))
#define BIT_AP_APB_ADC_ADC_ENTRY_SKIP    (BIT_(22))
#define BIT_AP_APB_ADC_ADC_ENTRY_DUMMY    (BIT_(21))
#define BIT_AP_APB_ADC_ADC_ENTRY_SETUP_SEL_1    (BIT_(17))
#define BIT_AP_APB_ADC_ADC_ENTRY_SETUP_SEL_0    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_ENTRY_CSEL_6    (BIT_(14))
#define BIT_AP_APB_ADC_ADC_ENTRY_CSEL_5    (BIT_(13))
#define BIT_AP_APB_ADC_ADC_ENTRY_CSEL_4    (BIT_(12))
#define BIT_AP_APB_ADC_ADC_ENTRY_CSEL_3    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_ENTRY_CSEL_2    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_ENTRY_CSEL_1    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_ENTRY_CSEL_0    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_7    (BIT_(7))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_6    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_5    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_4    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_3    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_2    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_ENTRY_RCT_0    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_MON_CSR
// Register Offset : 0x200
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_MON_CSR (ADC_APB_AB0_BASE_ADDR + (0x400<<0))
#define ADC_MON_CSR_VIOLATE_FIELD_OFFSET 16
#define ADC_MON_CSR_VIOLATE_FIELD_SIZE 1
#define ADC_MON_CSR_FILT_EN_FIELD_OFFSET 12
#define ADC_MON_CSR_FILT_EN_FIELD_SIZE 1
#define ADC_MON_CSR_DET_OORNG_FIELD_OFFSET 11
#define ADC_MON_CSR_DET_OORNG_FIELD_SIZE 1
#define ADC_MON_CSR_DET_INRNG_FIELD_OFFSET 10
#define ADC_MON_CSR_DET_INRNG_FIELD_SIZE 1
#define ADC_MON_CSR_DET_HI_FIELD_OFFSET 9
#define ADC_MON_CSR_DET_HI_FIELD_SIZE 1
#define ADC_MON_CSR_DET_LO_FIELD_OFFSET 8
#define ADC_MON_CSR_DET_LO_FIELD_SIZE 1
#define ADC_MON_CSR_CSEL_FIELD_OFFSET 1
#define ADC_MON_CSR_CSEL_FIELD_SIZE 7
#define ADC_MON_CSR_ENABLE_FIELD_OFFSET 0
#define ADC_MON_CSR_ENABLE_FIELD_SIZE 1

#define BIT_AP_APB_ADC_ADC_MON_CSR_VIOLATE    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_MON_CSR_FILT_EN    (BIT_(12))
#define BIT_AP_APB_ADC_ADC_MON_CSR_DET_OORNG    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_MON_CSR_DET_INRNG    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_MON_CSR_DET_HI    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_MON_CSR_DET_LO    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_MON_CSR_CSEL_6    (BIT_(7))
#define BIT_AP_APB_ADC_ADC_MON_CSR_CSEL_5    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_MON_CSR_CSEL_4    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_MON_CSR_CSEL_3    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_MON_CSR_CSEL_2    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_MON_CSR_CSEL_1    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_MON_CSR_CSEL_0    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_MON_CSR_ENABLE    (BIT_(0))

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_ADC_ADC_MON_THRE
// Register Offset : 0x204
// Description     :
//--------------------------------------------------------------------------
#define REG_AP_APB_ADC_ADC_MON_THRE (ADC_APB_AB0_BASE_ADDR + (0x404<<0))
#define ADC_MON_THRE_HI_FIELD_OFFSET 16
#define ADC_MON_THRE_HI_FIELD_SIZE 12
#define ADC_MON_THRE_LO_FIELD_OFFSET 0
#define ADC_MON_THRE_LO_FIELD_SIZE 12

#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_11    (BIT_(27))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_10    (BIT_(26))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_9    (BIT_(25))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_8    (BIT_(24))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_7    (BIT_(23))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_6    (BIT_(22))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_5    (BIT_(21))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_4    (BIT_(20))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_3    (BIT_(19))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_2    (BIT_(18))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_1    (BIT_(17))
#define BIT_AP_APB_ADC_ADC_MON_THRE_HI_0    (BIT_(16))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_11    (BIT_(11))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_10    (BIT_(10))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_9    (BIT_(9))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_8    (BIT_(8))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_7    (BIT_(7))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_6    (BIT_(6))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_5    (BIT_(5))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_4    (BIT_(4))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_3    (BIT_(3))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_2    (BIT_(2))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_1    (BIT_(1))
#define BIT_AP_APB_ADC_ADC_MON_THRE_LO_0    (BIT_(0))



#endif
