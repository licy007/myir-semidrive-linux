/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef __DTS_SDX9_PINFUNC_H
#define __DTS_SDX9_PINFUNC_H

/*
 * based on PinList_v0.9.9
 * The pin function ID is a tuple of
 * <mux_reg conf_reg mux_mode config >  + direction <dts>
 * *conf_reg = 0x000, invalid reg address, no need to set
 */

#define	    X9_PINCTRL_GPIO_C0__GPIO_MUX2_IO0_1                                      0x200 0x000 0x00 0x00
#define	    X9_PINCTRL_GPIO_C0__I2C5_SCL_1                                           0x200 0x400 0x01 0x00
#define	    X9_PINCTRL_GPIO_C0__SPI6_SCLK_1                                          0x200 0x404 0x02 0x00
#define	    X9_PINCTRL_GPIO_C0__CANFD5_TX_1                                          0x200 0x000 0x03 0x00
#define	    X9_PINCTRL_GPIO_C0__MSHC4_CARD_DET_N_1                                   0x200 0x408 0x04 0x00
#define	    X9_PINCTRL_GPIO_C0__USB1_PWR_1                                           0x200 0x000 0x05 0x00
#define	    X9_PINCTRL_GPIO_C0__TMR3_CH0_1                                           0x200 0x000 0x06 0x00
#define	    X9_PINCTRL_GPIO_C0__DFM_DDR_PWROKIN_1                                    0x200 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C1__GPIO_MUX2_IO1_1                                      0x204 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C1__I2C5_SDA_1                                           0x204 0x40c 0x01 0x00
#define     X9_PINCTRL_GPIO_C1__SPI6_MISO_1                                          0x204 0x410 0x02 0x00
#define     X9_PINCTRL_GPIO_C1__CANFD5_RX_1                                          0x204 0x414 0x03 0x00
#define     X9_PINCTRL_GPIO_C1__MSHC4_WP_1                                           0x204 0x418 0x04 0x00
#define     X9_PINCTRL_GPIO_C1__USB1_OC_1                                            0x204 0x41c 0x05 0x00
#define     X9_PINCTRL_GPIO_C1__TMR3_CH1_1                                           0x204 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C1__DFM_DDR_RESET_1                                      0x204 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C2__GPIO_MUX2_IO2_1                                      0x208 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C2__I2C6_SCL_1                                           0x208 0x420 0x01 0x00
#define     X9_PINCTRL_GPIO_C2__SPI6_MOSI_1                                          0x208 0x424 0x02 0x00
#define     X9_PINCTRL_GPIO_C2__CANFD6_TX_1                                          0x208 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C2__MSHC4_VOLT_SW_1                                      0x208 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C2__USB2_PWR_1                                           0x208 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_C2__TMR3_CH2_1                                           0x208 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C2__DFM_DDR_DWC_DDRPHY_DTO_1                             0x208 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C3__GPIO_MUX2_IO3_1                                      0x20c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C3__I2C6_SDA_1                                           0x20c 0x428 0x01 0x00
#define     X9_PINCTRL_GPIO_C3__SPI6_SS_1                                            0x20c 0x42c 0x02 0x00
#define     X9_PINCTRL_GPIO_C3__CANFD6_RX_1                                          0x20c 0x430 0x03 0x00
#define     X9_PINCTRL_GPIO_C3__MSHC4_LED_CTRL_1                                     0x20c 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C3__USB2_OC_1                                            0x20c 0x434 0x05 0x00
#define     X9_PINCTRL_GPIO_C3__TMR3_CH3_1                                           0x20c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C3__DFM_DDR_WSI_1                                        0x20c 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C4__GPIO_MUX2_IO4_1                                      0x210 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C4__UART9_TX_1                                           0x210 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_C4__I2C7_SCL_1                                           0x210 0x438 0x02 0x00
#define     X9_PINCTRL_GPIO_C4__UART10_RTS_1                                         0x210 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C4__MSHC3_CARD_DET_N_1                                   0x210 0x43c 0x04 0x00
#define     X9_PINCTRL_GPIO_C4__PCIE1_CLKREQ_N_1                                     0x210 0x440 0x05 0x00
#define     X9_PINCTRL_GPIO_C4__TMR4_CH0_1                                           0x210 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C4__DFM_DDR_TDRCLK_1                                     0x210 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C5__GPIO_MUX2_IO5_1                                      0x214 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C5__UART9_RX_1                                           0x214 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_C5__I2C7_SDA_1                                           0x214 0x444 0x02 0x00
#define     X9_PINCTRL_GPIO_C5__UART10_CTS_1                                         0x214 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C5__MSHC3_WP_1                                           0x214 0x448 0x04 0x00
#define     X9_PINCTRL_GPIO_C5__PCIE2_CLKREQ_N_1                                     0x214 0x44c 0x05 0x00
#define     X9_PINCTRL_GPIO_C5__TMR4_CH1_1                                           0x214 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C5__DFM_DDR_WRSTN_1                                      0x214 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C6__GPIO_MUX2_IO6_1                                      0x218 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C6__UART10_TX_1                                          0x218 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_C6__I2C8_SCL_1                                           0x218 0x450 0x02 0x00
#define     X9_PINCTRL_GPIO_C6__ETH2_MDIO_1                                         0x218 0x454 0x03 0x00
#define     X9_PINCTRL_GPIO_C6__MSHC3_VOLT_SW_1                                      0x218 0x458 0x04 0x00
#define     X9_PINCTRL_GPIO_C6__UART12_DE_1                                          0x218 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_C6__TMR4_CH2_1                                           0x218 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C6__DFM_DDR_DDRPHYCSRCMDTDRSHIFTEN_1                     0x218 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C7__GPIO_MUX2_IO7_1                                      0x21c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C7__UART10_RX_1                                          0x21c 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_C7__I2C8_SDA_1                                           0x21c 0x45c 0x02 0x00
#define     X9_PINCTRL_GPIO_C7__ETH2_MDC_1                                          0x21c 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C7__MSHC3_LED_CTRL_1                                     0x21c 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C7__UART12_RE_1                                          0x21c 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_C7__TMR4_CH3_1                                           0x21c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C7__DFM_DDR_DDRPHYCSRCMDTDRCAPTUREEN_1                   0x21c 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C8__GPIO_MUX2_IO8_1                                      0x220 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C8__SPI5_SCLK_1                                          0x220 0x460 0x01 0x00
#define     X9_PINCTRL_GPIO_C8__UART11_TX_1                                          0x220 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_C8__UART12_RTS_1                                         0x220 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C8__CKGEN_SEC_ETH_25M_125M_1                            0x220 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C8__WDT5_RESET_N_1                                       0x220 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_C8__PWM3_CH0_1                                           0x220 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C8__DFM_DDR_DDRPHYCSRCMDTDRUPDATEEN_1                    0x220 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C9__GPIO_MUX2_IO9_1                                      0x224 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C9__SPI5_MISO_1                                          0x224 0x464 0x01 0x00
#define     X9_PINCTRL_GPIO_C9__UART11_RX_1                                          0x224 0x468 0x02 0x00
#define     X9_PINCTRL_GPIO_C9__UART12_CTS_1                                         0x224 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C9__ETH2_CAP_COMP_1_1                                   0x224 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C9__WDT6_RESET_N_1                                       0x224 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_C9__PWM3_CH1_1                                           0x224 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C9__DFM_DDR_DDRPHYCSRCMDTDR_TDO_1                        0x224 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C10__GPIO_MUX2_IO10_1                                    0x228 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C10__SPI5_MOSI_1                                         0x228 0x46c 0x01 0x00
#define     X9_PINCTRL_GPIO_C10__UART12_TX_1                                         0x228 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_C10__ETH2_MDIO_2                                        0x228 0x454 0x03 0x01
#define     X9_PINCTRL_GPIO_C10__TMR5_CH2_1                                          0x228 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C10__WDT3_RESET_N_1                                      0x228 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_C10__PWM3_CH2_1                                          0x228 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C10__DFM_DDR_DDRPHYCSRRDDATATDRSHIFTEN_1                 0x228 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C11__GPIO_MUX2_IO11_1                                    0x22c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C11__SPI5_SS_1                                           0x22c 0x470 0x01 0x00
#define     X9_PINCTRL_GPIO_C11__UART12_RX_1                                         0x22c 0x474 0x02 0x00
#define     X9_PINCTRL_GPIO_C11__ETH2_MDC_2                                         0x22c 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C11__TMR5_CH3_1                                          0x22c 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C11__WDT4_RESET_N_1                                      0x22c 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_C11__PWM3_CH3_1                                          0x22c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C11__DFM_DDR_DDRPHYCSRRDDATATDRCAPTUREEN_1               0x22c 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C12__GPIO_MUX2_IO12_1                                    0x230 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C12__I2C13_SCL_1                                         0x230 0x478 0x01 0x00
#define     X9_PINCTRL_GPIO_C12__USB1_PWR_2                                          0x230 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_C12__CANFD7_TX_1                                         0x230 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C12__CKGEN_SEC_I2S_MCLK2_1                               0x230 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C12__PCIE2_PERST_N_1                                     0x230 0x47c 0x05 0x00
#define     X9_PINCTRL_GPIO_C12__PWM4_CH0_1                                          0x230 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C12__DFM_DDR_DDRPHYCSRRDDATATDRUPDATEEN_1                0x230 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C13__GPIO_MUX2_IO13_1                                    0x234 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C13__I2C13_SDA_1                                         0x234 0x480 0x01 0x00
#define     X9_PINCTRL_GPIO_C13__USB1_OC_2                                           0x234 0x41c 0x02 0x01
#define     X9_PINCTRL_GPIO_C13__CANFD7_RX_1                                         0x234 0x484 0x03 0x00
#define     X9_PINCTRL_GPIO_C13__CKGEN_SEC_CSI_MCLK1_1                               0x234 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C13__PCIE2_WAKE_N_1                                      0x234 0x488 0x05 0x00
#define     X9_PINCTRL_GPIO_C13__PWM4_CH1_1                                          0x234 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C13__DFM_DDR_DDRPHYCSRRDDATATDR_TDO_1                    0x234 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C14__GPIO_MUX2_IO14_1                                    0x238 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C14__I2C14_SCL_1                                         0x238 0x48c 0x01 0x00
#define     X9_PINCTRL_GPIO_C14__USB2_PWR_2                                          0x238 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_C14__CANFD8_TX_1                                         0x238 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_C14__CKGEN_SEC_I2S_MCLK3_1                               0x238 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C14__PCIE1_PERST_N_1                                     0x238 0x490 0x05 0x00
#define     X9_PINCTRL_GPIO_C14__PWM4_CH2_1                                          0x238 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C14__DFM_DDR_DDRPHY_TEST_RSTN_1                          0x238 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_C15__GPIO_MUX2_IO15_1                                    0x23c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_C15__I2C14_SDA_1                                         0x23c 0x494 0x01 0x00
#define     X9_PINCTRL_GPIO_C15__USB2_OC_2                                           0x23c 0x434 0x02 0x01
#define     X9_PINCTRL_GPIO_C15__CANFD8_RX_1                                         0x23c 0x498 0x03 0x00
#define     X9_PINCTRL_GPIO_C15__CKGEN_SEC_CSI_MCLK2_1                               0x23c 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_C15__PCIE1_WAKE_N_1                                      0x23c 0x49c 0x05 0x00
#define     X9_PINCTRL_GPIO_C15__PWM4_CH3_1                                          0x23c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_C15__DFM_DDR_DDRPHY_TEST_APBRESETN_1                     0x23c 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D0__GPIO_MUX2_IO16_1                                     0x240 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D0__I2C9_SCL_1                                           0x240 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_D0__SPI8_SCLK_1                                          0x240 0x4a0 0x02 0x00
#define     X9_PINCTRL_GPIO_D0__CANFD5_TX_2                                          0x240 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D0__MSHC4_CARD_DET_N_2                                   0x240 0x408 0x04 0x01
#define     X9_PINCTRL_GPIO_D0__PCIE2_PERST_N_2                                      0x240 0x47c 0x05 0x01
#define     X9_PINCTRL_GPIO_D0__TMR5_CH0_1                                           0x240 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D0__DFM_DDR_DDRPHY_TEST_DFI_RSTN_1                       0x240 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D1__GPIO_MUX2_IO17_1                                     0x244 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D1__I2C9_SDA_1                                           0x244 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_D1__SPI8_MISO_1                                          0x244 0x4a4 0x02 0x00
#define     X9_PINCTRL_GPIO_D1__CANFD5_RX_2                                          0x244 0x414 0x03 0x01
#define     X9_PINCTRL_GPIO_D1__MSHC4_WP_2                                           0x244 0x418 0x04 0x01
#define     X9_PINCTRL_GPIO_D1__PCIE2_WAKE_N_2                                       0x244 0x488 0x05 0x01
#define     X9_PINCTRL_GPIO_D1__TMR5_CH1_1                                           0x244 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D1__DFM_PHY_BYPASSMODEENAC_1                             0x244 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D2__GPIO_MUX2_IO18_1                                     0x248 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D2__I2C10_SCL_1                                          0x248 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_D2__SPI8_MOSI_1                                          0x248 0x4a8 0x02 0x00
#define     X9_PINCTRL_GPIO_D2__CANFD6_TX_2                                          0x248 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D2__MSHC4_VOLT_SW_2                                      0x248 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D2__PCIE1_PERST_N_2                                      0x248 0x490 0x05 0x01
#define     X9_PINCTRL_GPIO_D2__SPDIF3_IN_1                                          0x248 0x4ac 0x06 0x00
#define     X9_PINCTRL_GPIO_D2__DFM_PHY_BYPASSOUTENAC_1                              0x248 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D3__GPIO_MUX2_IO19_1                                     0x24c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D3__I2C10_SDA_1                                          0x24c 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_D3__SPI8_SS_1                                            0x24c 0x4b0 0x02 0x00
#define     X9_PINCTRL_GPIO_D3__CANFD6_RX_2                                          0x24c 0x430 0x03 0x01
#define     X9_PINCTRL_GPIO_D3__MSHC4_LED_CTRL_2                                     0x24c 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D3__PCIE1_WAKE_N_2                                       0x24c 0x49c 0x05 0x01
#define     X9_PINCTRL_GPIO_D3__SPDIF3_OUT_1                                         0x24c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D3__DFM_PHY_BYPASSOUTDATAAC_1                            0x24c 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D4__GPIO_MUX2_IO20_1                                     0x250 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D4__UART13_TX_1                                          0x250 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_D4__I2C11_SCL_1                                          0x250 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_D4__UART14_RTS_1                                         0x250 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D4__MSHC3_CARD_DET_N_2                                   0x250 0x43c 0x04 0x01
#define     X9_PINCTRL_GPIO_D4__USB1_PWR_3                                           0x250 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D4__TMR6_CH0_1                                           0x250 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D4__DFM_PHY_BAPASSMODEENDAT_1                            0x250 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D5__GPIO_MUX2_IO21_1                                     0x254 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D5__UART13_RX_1                                          0x254 0x4b4 0x01 0x00
#define     X9_PINCTRL_GPIO_D5__I2C11_SDA_1                                          0x254 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_D5__UART14_CTS_1                                         0x254 0x4b8 0x03 0x00
#define     X9_PINCTRL_GPIO_D5__MSHC3_WP_2                                           0x254 0x448 0x04 0x01
#define     X9_PINCTRL_GPIO_D5__USB1_OC_3                                            0x254 0x41c 0x05 0x02
#define     X9_PINCTRL_GPIO_D5__TMR6_CH1_1                                           0x254 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D5__DFM_PHY_BYPASSOUTENDAT_1                             0x254 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D6__GPIO_MUX2_IO22_1                                     0x258 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D6__UART14_TX_1                                          0x258 0x000 0x01 0x00
#define     X9_PINCTRL_GPIO_D6__I2C12_SCL_1                                          0x258 0x4bc 0x02 0x00
#define     X9_PINCTRL_GPIO_D6__ETH2_MDIO_3                                         0x258 0x454 0x03 0x02
#define     X9_PINCTRL_GPIO_D6__MSHC3_VOLT_SW_2                                      0x258 0x458 0x04 0x01
#define     X9_PINCTRL_GPIO_D6__USB2_PWR_3                                           0x258 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D6__SPDIF4_IN_1                                          0x258 0x4c0 0x06 0x00
#define     X9_PINCTRL_GPIO_D6__DFM_PHY_BYPASSOUTDATADAT_1                           0x258 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D7__GPIO_MUX2_IO23_1                                     0x25c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D7__UART14_RX_1                                          0x25c 0x4c4 0x01 0x00
#define     X9_PINCTRL_GPIO_D7__I2C12_SDA_1                                          0x25c 0x4c8 0x02 0x00
#define     X9_PINCTRL_GPIO_D7__ETH2_MDC_3                                          0x25c 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D7__MSHC3_LED_CTRL_2                                     0x25c 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D7__USB2_OC_3                                            0x25c 0x434 0x05 0x02
#define     X9_PINCTRL_GPIO_D7__SPDIF4_OUT_1                                         0x25c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D7__DFM_PHY_BYPASSMODEENMASTER_1                         0x25c 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D8__GPIO_MUX2_IO24_1                                     0x260 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D8__SPI7_SCLK_1                                          0x260 0x4cc 0x01 0x00
#define     X9_PINCTRL_GPIO_D8__UART15_TX_1                                          0x260 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_D8__UART16_RTS_1                                         0x260 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D8__CKGEN_SEC_ETH_25M_125M_2                            0x260 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D8__WDT5_RESET_N_2                                       0x260 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D8__PWM5_CH0_1                                           0x260 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D8__DFM_PHY_BYPASSOUTENMASTER_1                          0x260 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D9__GPIO_MUX2_IO25_1                                     0x264 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D9__SPI7_MISO_1                                          0x264 0x4d0 0x01 0x00
#define     X9_PINCTRL_GPIO_D9__UART15_RX_1                                          0x264 0x4d4 0x02 0x00
#define     X9_PINCTRL_GPIO_D9__UART16_CTS_1                                         0x264 0x4d8 0x03 0x00
#define     X9_PINCTRL_GPIO_D9__ETH2_CAP_COMP_2_1                                   0x264 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D9__WDT6_RESET_N_2                                       0x264 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D9__PWM5_CH1_1                                           0x264 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D9__DFM_PHY_BYPASSOUTDATAMASTER_1                        0x264 0x000 0x06 0x00

#define     X9_PINCTRL_GPIO_D10__GPIO_MUX2_IO26_1                                    0x268 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D10__SPI7_MOSI_1                                         0x268 0x4dc 0x01 0x00
#define     X9_PINCTRL_GPIO_D10__UART16_TX_1                                         0x268 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_D10__ETH2_MDIO_4                                        0x268 0x454 0x03 0x03
#define     X9_PINCTRL_GPIO_D10__TMR6_CH2_1                                          0x268 0x4e0 0x04 0x00
#define     X9_PINCTRL_GPIO_D10__WDT3_RESET_N_2                                      0x268 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D10__PWM3_EXT_CLK_1                                      0x268 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D10__DFM_DDRPHY_BYPASS_IN_DATA_SEL_0_1                   0x268 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D11__GPIO_MUX2_IO27_1                                    0x26c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D11__SPI7_SS_1                                           0x26c 0x4e4 0x01 0x00
#define     X9_PINCTRL_GPIO_D11__UART16_RX_1                                         0x26c 0x4e8 0x02 0x00
#define     X9_PINCTRL_GPIO_D11__ETH2_MDC_4                                         0x26c 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D11__TMR6_CH3_1                                          0x26c 0x4ec 0x04 0x00
#define     X9_PINCTRL_GPIO_D11__WDT4_RESET_N_2                                      0x26c 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D11__PWM4_EXT_CLK_1                                      0x26c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D11__DFM_DDRPHY_BYPASS_IN_DATA_SEL_1_1                   0x26c 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D12__GPIO_MUX2_IO28_1                                    0x270 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D12__I2C15_SCL_1                                         0x270 0x4f0 0x01 0x00
#define     X9_PINCTRL_GPIO_D12__USB1_PWR_4                                          0x270 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_D12__CANFD7_TX_2                                         0x270 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D12__CKGEN_SEC_I2S_MCLK2_2                               0x270 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D12__PCIE1_CLKREQ_N_2                                    0x270 0x440 0x05 0x01
#define     X9_PINCTRL_GPIO_D12__PWM6_CH0_1                                          0x270 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D12__DFM_DDRPHY_BYPASS_IN_DATA_SEL_2_1                   0x270 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D13__GPIO_MUX2_IO29_1                                    0x274 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D13__I2C15_SDA_1                                         0x274 0x4f4 0x01 0x00
#define     X9_PINCTRL_GPIO_D13__USB1_OC_4                                           0x274 0x41c 0x02 0x03
#define     X9_PINCTRL_GPIO_D13__CANFD7_RX_2                                         0x274 0x484 0x03 0x01
#define     X9_PINCTRL_GPIO_D13__CKGEN_SEC_CSI_MCLK1_2                               0x274 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D13__PCIE2_CLKREQ_N_2                                    0x274 0x44c 0x05 0x01
#define     X9_PINCTRL_GPIO_D13__PWM6_CH1_1                                          0x274 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D13__DFM_PHY_BYPASSINDATA0_1                             0x274 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D14__GPIO_MUX2_IO30_1                                    0x278 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D14__I2C16_SCL_1                                         0x278 0x4f8 0x01 0x00
#define     X9_PINCTRL_GPIO_D14__USB2_PWR_4                                          0x278 0x000 0x02 0x00
#define     X9_PINCTRL_GPIO_D14__CANFD8_TX_2                                         0x278 0x000 0x03 0x00
#define     X9_PINCTRL_GPIO_D14__CKGEN_SEC_I2S_MCLK3_2                               0x278 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D14__OSPI2_ECC_FAIL_1                                    0x278 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D14__OBSERVER_MUX2_D0_1                                  0x278 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D14__DFM_PHY_BYPASSINDATA1_1                             0x278 0x000 0x07 0x00

#define     X9_PINCTRL_GPIO_D15__GPIO_MUX2_IO31_1                                    0x27c 0x000 0x00 0x00
#define     X9_PINCTRL_GPIO_D15__I2C16_SDA_1                                         0x27c 0x4fc 0x01 0x00
#define     X9_PINCTRL_GPIO_D15__USB2_OC_4                                           0x27c 0x434 0x02 0x03
#define     X9_PINCTRL_GPIO_D15__CANFD8_RX_2                                         0x27c 0x498 0x03 0x01
#define     X9_PINCTRL_GPIO_D15__CKGEN_SEC_CSI_MCLK2_2                               0x27c 0x000 0x04 0x00
#define     X9_PINCTRL_GPIO_D15__OSPI2_INTB_1                                        0x27c 0x000 0x05 0x00
#define     X9_PINCTRL_GPIO_D15__OBSERVER_MUX2_D1_1                                  0x27c 0x000 0x06 0x00
#define     X9_PINCTRL_GPIO_D15__DFM_PHY_BYPASSINDATA2_1                             0x27c 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_SCLK__GPIO_MUX2_IO32_1                                  0x280 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_SCLK__OSPI2_SCLK_1                                      0x280 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_SCLK__SPI5_SCLK_2                                       0x280 0x460 0x02 0x01
#define     X9_PINCTRL_OSPI2_SCLK__MSHC2_CARD_DET_N_1                                0x280 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_SCLK__I2S_MC2_SCKO_1                                    0x280 0x500 0x04 0x00
#define     X9_PINCTRL_OSPI2_SCLK__CSI3_CH0_CLK_1                                    0x280 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_SCLK__OBSERVER_MUX2_D2_1                                0x280 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_SCLK__DFM_PHY_BYPASSINDATA3_1                           0x280 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_SS0__GPIO_MUX2_IO33_1                                   0x284 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_SS0__OSPI2_SS0_1                                        0x284 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_SS0__SPI5_SS_2                                          0x284 0x470 0x02 0x01
#define     X9_PINCTRL_OSPI2_SS0__MSHC2_WP_1                                         0x284 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_SS0__I2S_MC2_WSO_1                                      0x284 0x504 0x04 0x00
#define     X9_PINCTRL_OSPI2_SS0__CSI3_CH0_HSYNC_1                                   0x284 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_SS0__OBSERVER_MUX2_D3_1                                 0x284 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_SS0__DFM_PHY_BYPASSINDATA4_1                            0x284 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA0__GPIO_MUX2_IO34_1                                 0x288 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA0__OSPI2_DATA0_1                                    0x288 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA0__SPI5_MISO_2                                      0x288 0x464 0x02 0x01
#define     X9_PINCTRL_OSPI2_DATA0__MSHC2_VOLT_SW_1                                  0x288 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_DATA0__I2S_MC2_SDI0_SDO0_1                              0x288 0x508 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA0__CSI3_CH0_VSYNC_1                                 0x288 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA0__OBSERVER_MUX2_D4_1                               0x288 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA0__DFM_CKGEN_SAF_TESTOUT_1                          0x288 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA1__GPIO_MUX2_IO35_1                                 0x28c 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA1__OSPI2_DATA1_1                                    0x28c 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA1__SPI5_MOSI_2                                      0x28c 0x46c 0x02 0x01
#define     X9_PINCTRL_OSPI2_DATA1__MSHC2_LED_CTRL_1                                 0x28c 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_DATA1__I2S_MC2_SDI1_SDO1_1                              0x28c 0x50c 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA1__CSI3_CH0_ENABLE_1                                0x28c 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA1__OBSERVER_MUX2_D5_1                               0x28c 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA1__DFM_CKGEN_SEC_TESTOUT_1                          0x28c 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA2__GPIO_MUX2_IO36_1                                 0x290 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA2__OSPI2_DATA2_1                                    0x290 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA2__ETH2_MDIO_5                                     0x290 0x454 0x02 0x04
#define     X9_PINCTRL_OSPI2_DATA2__I2C12_SCL_2                                      0x290 0x4bc 0x03 0x01
#define     X9_PINCTRL_OSPI2_DATA2__I2S_MC2_SDI2_SDO2_1                              0x290 0x510 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA2__CSI3_CH0_DATA0_1                                 0x290 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA2__OBSERVER_MUX2_D6_1                               0x290 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA2__DFM_CKGEN_HPI_TESTOUT_1                          0x290 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA3__GPIO_MUX2_IO37_1                                 0x294 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA3__OSPI2_DATA3_1                                    0x294 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA3__ETH2_MDC_5                                      0x294 0x000 0x02 0x00
#define     X9_PINCTRL_OSPI2_DATA3__I2C12_SDA_2                                      0x294 0x4c8 0x03 0x01
#define     X9_PINCTRL_OSPI2_DATA3__I2S_MC2_SDI3_SDO3_1                              0x294 0x514 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA3__CSI3_CH0_DATA1_1                                 0x294 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA3__OBSERVER_MUX2_D7_1                               0x294 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA3__DFM_CKGEN_DISP_TESTOUT_1                         0x294 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA4__GPIO_MUX2_IO38_1                                 0x298 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA4__OSPI2_DATA4_1                                    0x298 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA4__UART11_TX_2                                      0x298 0x000 0x02 0x00
#define     X9_PINCTRL_OSPI2_DATA4__MSHC1_CARD_DET_N_1                               0x298 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_DATA4__I2S_MC2_SDI4_SDO4_1                              0x298 0x518 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA4__CSI3_CH0_DATA2_1                                 0x298 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA4__MSHC4_CLK_1                                      0x298 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA4__DFM_RTCXTAL_FOUT_1                               0x298 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA5__GPIO_MUX2_IO39_1                                 0x29c 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA5__OSPI2_DATA5_1                                    0x29c 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA5__UART11_RX_2                                      0x29c 0x468 0x02 0x01
#define     X9_PINCTRL_OSPI2_DATA5__MSHC1_WP_1                                       0x29c 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_DATA5__I2S_MC2_SDI5_SDO5_1                              0x29c 0x51c 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA5__CSI3_CH0_DATA3_1                                 0x29c 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA5__MSHC4_CMD_1                                      0x29c 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA5__DFM_RTC2M_FOUT_1                                 0x29c 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA6__GPIO_MUX2_IO40_1                                 0x2a0 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA6__OSPI2_DATA6_1                                    0x2a0 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA6__UART11_CTS_1                                     0x2a0 0x000 0x02 0x00
#define     X9_PINCTRL_OSPI2_DATA6__MSHC1_VOLT_SW_1                                  0x2a0 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_DATA6__I2S_MC2_SDI6_SDO6_1                              0x2a0 0x520 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA6__CSI3_CH0_DATA4_1                                 0x2a0 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA6__MSHC4_DATA0_1                                    0x2a0 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA6__DFM_XTAL24M_FOUT_1                               0x2a0 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DATA7__GPIO_MUX2_IO41_1                                 0x2a4 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DATA7__OSPI2_DATA7_1                                    0x2a4 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_DATA7__UART11_RTS_1                                     0x2a4 0x000 0x02 0x00
#define     X9_PINCTRL_OSPI2_DATA7__MSHC1_LED_CTRL_1                                 0x2a4 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_DATA7__I2S_MC2_SDI7_SDO7_1                              0x2a4 0x524 0x04 0x00
#define     X9_PINCTRL_OSPI2_DATA7__CSI3_CH0_DATA5_1                                 0x2a4 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DATA7__MSHC4_DATA1_1                                    0x2a4 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DATA7__DFM_XTALSOC_FOUT_1                               0x2a4 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_DQS__GPIO_MUX2_IO42_1                                   0x2a8 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_DQS__OSPI2_DQS_1                                        0x2a8 0x528 0x01 0x00
#define     X9_PINCTRL_OSPI2_DQS__USB1_PWR_5                                         0x2a8 0x000 0x02 0x00
#define     X9_PINCTRL_OSPI2_DQS__USB2_PWR_5                                         0x2a8 0x000 0x03 0x00
#define     X9_PINCTRL_OSPI2_DQS__I2S_MC2_WSI_1                                      0x2a8 0x52c 0x04 0x00
#define     X9_PINCTRL_OSPI2_DQS__CSI3_CH0_DATA6_1                                   0x2a8 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_DQS__MSHC4_DATA2_1                                      0x2a8 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_DQS__DFM_RC24M_FOUT_1                                   0x2a8 0x000 0x07 0x00

#define     X9_PINCTRL_OSPI2_SS1__GPIO_MUX2_IO43_1                                   0x2ac 0x000 0x00 0x00
#define     X9_PINCTRL_OSPI2_SS1__OSPI2_SS1_1                                        0x2ac 0x000 0x01 0x00
#define     X9_PINCTRL_OSPI2_SS1__USB1_OC_5                                          0x2ac 0x41c 0x02 0x04
#define     X9_PINCTRL_OSPI2_SS1__USB2_OC_5                                          0x2ac 0x434 0x03 0x04
#define     X9_PINCTRL_OSPI2_SS1__I2S_MC2_SCKI_1                                     0x2ac 0x530 0x04 0x00
#define     X9_PINCTRL_OSPI2_SS1__CSI3_CH0_DATA7_1                                   0x2ac 0x000 0x05 0x00
#define     X9_PINCTRL_OSPI2_SS1__MSHC4_DATA3_1                                      0x2ac 0x000 0x06 0x00
#define     X9_PINCTRL_OSPI2_SS1__DFM_USB_PORTRESET_1                                0x2ac 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_TXC__GPIO_MUX2_IO44_1                                  0x2b0 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_TXC__ETH2_TXC_1                                       0x2b0 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_TXC__ETH2_AUS_IN_0_1                                  0x2b0 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_TXC__UART15_CTS_1                                      0x2b0 0x000 0x03 0x00
#define     X9_PINCTRL_RGMII2_TXC__I2S_SC3_SCK_1                                     0x2b0 0x534 0x04 0x00
#define     X9_PINCTRL_RGMII2_TXC__CSI3_CH1_CLK_1                                    0x2b0 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_TXC__OBSERVER_MUX2_D8_1                                0x2b0 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_TXC__DFM_USB_ATERESET_1                                0x2b0 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_TXD0__GPIO_MUX2_IO45_1                                 0x2b4 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_TXD0__ETH2_TXD0_1                                     0x2b4 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_TXD0__TMR8_CH0_1                                       0x2b4 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_TXD0__UART15_TX_2                                      0x2b4 0x000 0x03 0x00
#define     X9_PINCTRL_RGMII2_TXD0__I2S_SC3_WS_1                                     0x2b4 0x538 0x04 0x00
#define     X9_PINCTRL_RGMII2_TXD0__CSI3_CH1_HSYNC_1                                 0x2b4 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_TXD0__OBSERVER_MUX2_D9_1                               0x2b4 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_TXD0__DFM_USB_PHY_RESET_1                              0x2b4 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_TXD1__GPIO_MUX2_IO46_1                                 0x2b8 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_TXD1__ETH2_TXD1_1                                     0x2b8 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_TXD1__TMR8_CH1_1                                       0x2b8 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_TXD1__UART15_RX_2                                      0x2b8 0x4d4 0x03 0x01
#define     X9_PINCTRL_RGMII2_TXD1__I2S_SC3_SDO_SDI_1                                0x2b8 0x000 0x04 0x00
#define     X9_PINCTRL_RGMII2_TXD1__CSI3_CH1_VSYNC_1                                 0x2b8 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_TXD1__OBSERVER_MUX2_D10_1                              0x2b8 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_TXD1__DFM_USB_PIPE_RESET_N_1                           0x2b8 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_TXD2__GPIO_MUX2_IO47_1                                 0x2bc 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_TXD2__ETH2_TXD2_1                                     0x2bc 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_TXD2__ETH2_MDIO_6                                     0x2bc 0x454 0x02 0x05
#define     X9_PINCTRL_RGMII2_TXD2__UART15_RTS_1                                     0x2bc 0x000 0x03 0x00
#define     X9_PINCTRL_RGMII2_TXD2__I2S_SC3_SDI_SDO_1                                0x2bc 0x53c 0x04 0x00
#define     X9_PINCTRL_RGMII2_TXD2__CSI3_CH1_ENABLE_1                                0x2bc 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_TXD2__CKGEN_SEC_I2S_MCLK2_3                            0x2bc 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_TXD2__DFM_PCIE_PHY_RESET_1                             0x2bc 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_TXD3__GPIO_MUX2_IO48_1                                 0x2c0 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_TXD3__ETH2_TXD3_1                                     0x2c0 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_TXD3__ETH2_MDC_6                                      0x2c0 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_TXD3__I2C7_SCL_2                                       0x2c0 0x438 0x03 0x01
/* X9_PINCTRL_RGMII2_TXD3 MUX4 IDLE */
#define     X9_PINCTRL_RGMII2_TXD3__CSI3_CH1_DATA0_1                                 0x2c0 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_TXD3__CKGEN_SEC_I2S_MCLK3_3                            0x2c0 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_TXD3__DFM_PCIE_DTB_OUT0_1                              0x2c0 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_TX_CTL__GPIO_MUX2_IO49_1                               0x2c4 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_TX_CTL__ETH2_TXEN_1                                   0x2c4 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_TX_CTL__TMR8_EXT_CLK_1                                 0x2c4 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_TX_CTL__I2C7_SDA_2                                     0x2c4 0x444 0x03 0x01
/* X9_PINCTRL_RGMII2_TX_CTL MUX4 IDLE */
#define     X9_PINCTRL_RGMII2_TX_CTL__CSI3_CH1_DATA1_1                               0x2c4 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_TX_CTL__OBSERVER_MUX2_D11_1                            0x2c4 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_TX_CTL__DFM_PCIE_DTB_OUT1_1                            0x2c4 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_RXC__GPIO_MUX2_IO50_1                                  0x2c8 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_RXC__ETH2_RXC_1                                       0x2c8 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_RXC__ETH2_CAP_COMP_0_1                                0x2c8 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_RXC__UART16_CTS_2                                      0x2c8 0x4d8 0x03 0x01
#define     X9_PINCTRL_RGMII2_RXC__I2S_SC4_SCK_1                                     0x2c8 0x540 0x04 0x00
#define     X9_PINCTRL_RGMII2_RXC__CSI3_CH1_DATA2_1                                  0x2c8 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_RXC__OBSERVER_MUX2_D12_1                               0x2c8 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_RXC__DFM_EMMC_TEST_DLOUT_EN_1                          0x2c8 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_RXD0__GPIO_MUX2_IO51_1                                 0x2cc 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_RXD0__ETH2_RXD0_1                                     0x2cc 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_RXD0__TMR8_CH2_1                                       0x2cc 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_RXD0__UART16_TX_2                                      0x2cc 0x000 0x03 0x00
#define     X9_PINCTRL_RGMII2_RXD0__I2S_SC4_WS_1                                     0x2cc 0x544 0x04 0x00
#define     X9_PINCTRL_RGMII2_RXD0__CSI3_CH1_DATA3_1                                 0x2cc 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_RXD0__OBSERVER_MUX2_D13_1                              0x2cc 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_RXD0__DFM_EMMC_PHY_TEST_INTF_SEL_1                     0x2cc 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_RXD1__GPIO_MUX2_IO52_1                                 0x2d0 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_RXD1__ETH2_RXD1_1                                     0x2d0 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_RXD1__TMR8_CH3_1                                       0x2d0 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_RXD1__UART16_RX_2                                      0x2d0 0x4e8 0x03 0x01
#define     X9_PINCTRL_RGMII2_RXD1__I2S_SC4_SDO_SDI_1                                0x2d0 0x000 0x04 0x00
#define     X9_PINCTRL_RGMII2_RXD1__CSI3_CH1_DATA4_1                                 0x2d0 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_RXD1__OBSERVER_MUX2_D14_1                              0x2d0 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_RXD1__DFM_EMMC_TEST_DL_CLK_1                           0x2d0 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_RXD2__GPIO_MUX2_IO53_1                                 0x2d4 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_RXD2__ETH2_RXD2_1                                     0x2d4 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_RXD2__ETH2_RMII_REF_CLK_1                             0x2d4 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_RXD2__UART16_RTS_2                                     0x2d4 0x000 0x03 0x00
#define     X9_PINCTRL_RGMII2_RXD2__I2S_SC4_SDI_SDO_1                                0x2d4 0x548 0x04 0x00
#define     X9_PINCTRL_RGMII2_RXD2__CSI3_CH1_DATA5_1                                 0x2d4 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_RXD2__OBSERVER_MUX2_D15_1                              0x2d4 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_RXD2__DFM_EMMC2_JTAGIF_SEL_1                           0x2d4 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_RXD3__GPIO_MUX2_IO54_1                                 0x2d8 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_RXD3__ETH2_RXD3_1                                     0x2d8 0x000 0x01 0x00
#define     X9_PINCTRL_RGMII2_RXD3__ETH2_RX_ER_1                                    0x2d8 0x000 0x02 0x00
#define     X9_PINCTRL_RGMII2_RXD3__I2C8_SCL_2                                       0x2d8 0x450 0x03 0x01
#define     X9_PINCTRL_RGMII2_RXD3__TMR4_EXT_CLK_1                                   0x2d8 0x000 0x04 0x00
#define     X9_PINCTRL_RGMII2_RXD3__CSI3_CH1_DATA6_1                                 0x2d8 0x000 0x05 0x00
#define     X9_PINCTRL_RGMII2_RXD3__OBSERVER_MUX2_D16_1                              0x2d8 0x000 0x06 0x00
#define     X9_PINCTRL_RGMII2_RXD3__DFM_MIPI_PHY_SEL_0_1                             0x2d8 0x000 0x07 0x00

#define     X9_PINCTRL_RGMII2_RX_CTL__GPIO_MUX2_IO55_1                               0x2dc 0x000 0x00 0x00
#define     X9_PINCTRL_RGMII2_RX_CTL__ETH2_RXDV_1                                   0x2dc 0x000 0x01 0x00
/* X9_PINCTRL_RGMII2_RX_CTL MUX2 IDLE */
#define     X9_PINCTRL_RGMII2_RX_CTL__I2C8_SDA_2                                     0x2dc 0x45c 0x03 0x01
#define     X9_PINCTRL_RGMII2_RX_CTL__TMR5_EXT_CLK_1                                 0x2dc 0x000 0x04 0x00
#define     X9_PINCTRL_RGMII2_RX_CTL__CSI3_CH1_DATA7_1                               0x2dc 0x000 0x05 0x00
/* X9_PINCTRL_RGMII2_RX_CTL MUX6 IDLE */
#define     X9_PINCTRL_RGMII2_RX_CTL__DFM_MIPI_PHY_SEL_1_1                           0x2dc 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC3_SCK__GPIO_MUX2_IO56_1                                 0x2e0 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC3_SCK__I2S_SC3_SCK_2                                    0x2e0 0x534 0x01 0x01
#define     X9_PINCTRL_I2S_SC3_SCK__ADC_CSEL6_1                                      0x2e0 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC3_SCK__I2C13_SCL_2                                      0x2e0 0x478 0x03 0x01
#define     X9_PINCTRL_I2S_SC3_SCK__I2S_MC2_SCKI_2                                   0x2e0 0x530 0x04 0x01
#define     X9_PINCTRL_I2S_SC3_SCK__DISP_CLK_1                                       0x2e0 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC3_SCK__CSSYS_TRACECLK_1                                 0x2e0 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC3_SCK__DFM_MIPI_PHY_SEL_2_1                             0x2e0 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC3_WS__GPIO_MUX2_IO57_1                                  0x2e4 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC3_WS__I2S_SC3_WS_2                                      0x2e4 0x538 0x01 0x01
#define     X9_PINCTRL_I2S_SC3_WS__ADC_CSEL5_1                                       0x2e4 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC3_WS__I2C13_SDA_2                                       0x2e4 0x480 0x03 0x01
#define     X9_PINCTRL_I2S_SC3_WS__I2S_MC2_WSI_2                                     0x2e4 0x52c 0x04 0x01
#define     X9_PINCTRL_I2S_SC3_WS__DISP_HSYNC_1                                      0x2e4 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC3_WS__CSSYS_TRACECTRL_1                                 0x2e4 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC3_WS__DFM_MIPI_RX_SHUTDOWNZ_1                           0x2e4 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC3_SD__GPIO_MUX2_IO58_1                                  0x2e8 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC3_SD__I2S_SC3_SDO_SDI_2                                 0x2e8 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC3_SD__I2S_SC4_SDI_SDO_2                                 0x2e8 0x548 0x02 0x01
#define     X9_PINCTRL_I2S_SC3_SD__UART13_TX_2                                       0x2e8 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_SC3_SD__I2S_MC2_SDI0_SDO0_2                               0x2e8 0x508 0x04 0x01
#define     X9_PINCTRL_I2S_SC3_SD__DISP_VSYNC_1                                      0x2e8 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC3_SD__CSSYS_TRACEDATA0_1                                0x2e8 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC3_SD__DFM_MIPI_RX_RSTZ_1                                0x2e8 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC4_SCK__GPIO_MUX2_IO59_1                                 0x2ec 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC4_SCK__I2S_SC4_SCK_2                                    0x2ec 0x540 0x01 0x01
#define     X9_PINCTRL_I2S_SC4_SCK__OSPI2_DQS_2                                      0x2ec 0x528 0x02 0x01
#define     X9_PINCTRL_I2S_SC4_SCK__I2C13_SCL_3                                      0x2ec 0x478 0x03 0x02
#define     X9_PINCTRL_I2S_SC4_SCK__I2S_MC2_SDI1_SDO1_2                              0x2ec 0x50c 0x04 0x01
#define     X9_PINCTRL_I2S_SC4_SCK__DISP_ENABLE_1                                    0x2ec 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC4_SCK__CSSYS_TRACEDATA1_1                               0x2ec 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC4_SCK__DFM_MIPI_TESTDIN_0_1                             0x2ec 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC4_WS__GPIO_MUX2_IO60_1                                  0x2f0 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC4_WS__I2S_SC4_WS_2                                      0x2f0 0x544 0x01 0x01
#define     X9_PINCTRL_I2S_SC4_WS__PWM5_EXT_CLK_1                                    0x2f0 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC4_WS__I2C13_SDA_3                                       0x2f0 0x480 0x03 0x02
#define     X9_PINCTRL_I2S_SC4_WS__I2S_MC2_SDI2_SDO2_2                               0x2f0 0x510 0x04 0x01
#define     X9_PINCTRL_I2S_SC4_WS__DISP_DATA0_CANFD9_TX_1                            0x2f0 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC4_WS__CSSYS_TRACEDATA2_1                                0x2f0 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC4_WS__DFM_MIPI_TESTDIN_1_1                              0x2f0 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC4_SD__GPIO_MUX2_IO61_1                                  0x2f4 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC4_SD__I2S_SC4_SDO_SDI_2                                 0x2f4 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC4_SD__I2S_SC3_SDI_SDO_2                                 0x2f4 0x53c 0x02 0x01
#define     X9_PINCTRL_I2S_SC4_SD__UART13_RX_2                                       0x2f4 0x4b4 0x03 0x01
#define     X9_PINCTRL_I2S_SC4_SD__I2S_MC2_SDI3_SDO3_2                               0x2f4 0x514 0x04 0x01
#define     X9_PINCTRL_I2S_SC4_SD__DISP_DATA1_CANFD9_RX_1                            0x2f4 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC4_SD__CSSYS_TRACEDATA3_1                                0x2f4 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC4_SD__DFM_MIPI_TESTDIN_2_1                              0x2f4 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC5_SCK__GPIO_MUX2_IO62_1                                 0x2f8 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC5_SCK__I2S_SC5_SCK_1                                    0x2f8 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC5_SCK__PWM6_EXT_CLK_1                                   0x2f8 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC5_SCK__I2C14_SCL_2                                      0x2f8 0x48c 0x03 0x01
#define     X9_PINCTRL_I2S_SC5_SCK__I2S_MC2_SDI4_SDO4_2                              0x2f8 0x518 0x04 0x01
#define     X9_PINCTRL_I2S_SC5_SCK__DISP_DATA2_CANFD10_TX_1                          0x2f8 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC5_SCK__CSSYS_TRACEDATA4_1                               0x2f8 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC5_SCK__DFM_MIPI_TESTDIN_3_1                             0x2f8 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC5_WS__GPIO_MUX2_IO63_1                                  0x2fc 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC5_WS__I2S_SC5_WS_1                                      0x2fc 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC5_WS__PWM7_EXT_CLK_1                                    0x2fc 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC5_WS__I2C14_SDA_2                                       0x2fc 0x494 0x03 0x01
#define     X9_PINCTRL_I2S_SC5_WS__I2S_MC2_SDI5_SDO5_2                               0x2fc 0x51c 0x04 0x01
#define     X9_PINCTRL_I2S_SC5_WS__DISP_DATA3_CANFD10_RX_1                           0x2fc 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC5_WS__CSSYS_TRACEDATA5_1                                0x2fc 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC5_WS__DFM_MIPI_TESTDIN_4_1                              0x2fc 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC5_SD__GPIO_MUX2_IO64_1                                  0x300 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC5_SD__I2S_SC5_SDO_SDI_1                                 0x300 0x550 0x01 0x01
#define     X9_PINCTRL_I2S_SC5_SD__I2S_SC6_SDI_SDO_1                                 0x300 0x54c 0x02 0x00
#define     X9_PINCTRL_I2S_SC5_SD__PWM6_CH2_1                                        0x300 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_SC5_SD__I2S_MC2_SDI6_SDO6_2                               0x300 0x520 0x04 0x01
#define     X9_PINCTRL_I2S_SC5_SD__DISP_DATA4_CANFD11_TX_1                           0x300 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC5_SD__CSSYS_TRACEDATA6_1                                0x300 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC5_SD__DFM_MIPI_TESTDIN_5_1                              0x300 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC6_SCK__GPIO_MUX2_IO65_1                                 0x304 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC6_SCK__I2S_SC6_SCK_1                                    0x304 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC6_SCK__TMR6_EXT_CLK_1                                   0x304 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC6_SCK__I2C14_SCL_3                                      0x304 0x48c 0x03 0x02
#define     X9_PINCTRL_I2S_SC6_SCK__I2S_MC2_SDI7_SDO7_2                              0x304 0x524 0x04 0x01
#define     X9_PINCTRL_I2S_SC6_SCK__DISP_DATA5_CANFD11_RX_1                          0x304 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC6_SCK__CSSYS_TRACEDATA7_1                               0x304 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC6_SCK__DFM_MIPI_TESTDIN_6_1                             0x304 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC6_WS__GPIO_MUX2_IO66_1                                  0x308 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC6_WS__I2S_SC6_WS_1                                      0x308 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC6_WS__TMR7_EXT_CLK_1                                    0x308 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC6_WS__I2C14_SDA_3                                       0x308 0x494 0x03 0x02
#define     X9_PINCTRL_I2S_SC6_WS__I2S_MC2_WSO_2                                     0x308 0x504 0x04 0x01
#define     X9_PINCTRL_I2S_SC6_WS__DISP_DATA6_CANFD12_TX_1                           0x308 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC6_WS__CSSYS_TRACEDATA8_1                                0x308 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC6_WS__DFM_MIPI_TESTDIN_7_1                              0x308 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC6_SD__GPIO_MUX2_IO67_1                                  0x30c 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC6_SD__I2S_SC6_SDO_SDI_1                                 0x30c 0x54c 0x01 0x01
#define     X9_PINCTRL_I2S_SC6_SD__I2S_SC5_SDI_SDO_1                                 0x30c 0x550 0x02 0x00
#define     X9_PINCTRL_I2S_SC6_SD__PWM6_CH3_1                                        0x30c 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_SC6_SD__I2S_MC2_SCKO_2                                    0x30c 0x500 0x04 0x01
#define     X9_PINCTRL_I2S_SC6_SD__DISP_DATA7_CANFD12_RX_1                           0x30c 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC6_SD__CSSYS_TRACEDATA9_1                                0x30c 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC6_SD__DFM_MIPI_TESTDOUT_0_1                             0x30c 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC7_SCK__GPIO_MUX2_IO68_1                                 0x310 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC7_SCK__I2S_SC7_SCK_1                                    0x310 0x554 0x01 0x00
#define     X9_PINCTRL_I2S_SC7_SCK__ADC_CSEL4_1                                      0x310 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC7_SCK__CKGEN_SEC_I2S_MCLK2_4                            0x310 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_SC7_SCK__I2S_MC1_SCKO_1                                   0x310 0x558 0x04 0x00
#define     X9_PINCTRL_I2S_SC7_SCK__DISP_DATA8_CANFD13_TX_1                          0x310 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC7_SCK__CSSYS_TRACEDATA10_1                              0x310 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC7_SCK__DFM_MIPI_TESTDOUT_1_1                            0x310 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC7_WS__GPIO_MUX2_IO69_1                                  0x314 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC7_WS__I2S_SC7_WS_1                                      0x314 0x55c 0x01 0x00
#define     X9_PINCTRL_I2S_SC7_WS__ADC_CSEL3_1                                       0x314 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC7_WS__CKGEN_SEC_I2S_MCLK3_4                             0x314 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_SC7_WS__I2S_MC1_WSO_1                                     0x314 0x560 0x04 0x00
#define     X9_PINCTRL_I2S_SC7_WS__DISP_DATA9_CANFD13_RX_1                           0x314 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC7_WS__CSSYS_TRACEDATA11_1                               0x314 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC7_WS__DFM_MIPI_TESTDOUT_2_1                             0x314 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC7_SD__GPIO_MUX2_IO70_1                                  0x318 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC7_SD__I2S_SC7_SDO_SDI_1                                 0x318 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC7_SD__I2S_SC8_SDI_SDO_1                                 0x318 0x564 0x02 0x00
#define     X9_PINCTRL_I2S_SC7_SD__UART15_TX_3                                       0x318 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_SC7_SD__I2S_MC1_SDI4_SDO4_1                               0x318 0x568 0x04 0x00
#define     X9_PINCTRL_I2S_SC7_SD__DISP_DATA10_CANFD14_TX_1                          0x318 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC7_SD__CSSYS_TRACEDATA12_1                               0x318 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC7_SD__DFM_MIPI_TESTDOUT_3_1                             0x318 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC8_SCK__GPIO_MUX2_IO71_1                                 0x31c 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC8_SCK__I2S_SC8_SCK_1                                    0x31c 0x56c 0x01 0x00
#define     X9_PINCTRL_I2S_SC8_SCK__ADC_EXT_CLK_1                                    0x31c 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC8_SCK__I2C15_SCL_2                                      0x31c 0x4f0 0x03 0x01
#define     X9_PINCTRL_I2S_SC8_SCK__I2S_MC1_SDI5_SDO5_1                              0x31c 0x570 0x04 0x00
#define     X9_PINCTRL_I2S_SC8_SCK__DISP_DATA11_CANFD14_RX_1                         0x31c 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC8_SCK__CSSYS_TRACEDATA13_1                              0x31c 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC8_SCK__DFM_MIPI_TESTDOUT_4_1                            0x31c 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC8_WS__GPIO_MUX2_IO72_1                                  0x320 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC8_WS__I2S_SC8_WS_1                                      0x320 0x574 0x01 0x00
#define     X9_PINCTRL_I2S_SC8_WS__PWM8_EXT_CLK_1                                    0x320 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_SC8_WS__I2C15_SDA_2                                       0x320 0x4f4 0x03 0x01
#define     X9_PINCTRL_I2S_SC8_WS__I2S_MC1_SDI6_SDO6_1                               0x320 0x578 0x04 0x00
#define     X9_PINCTRL_I2S_SC8_WS__DISP_DATA12_CANFD15_TX_1                          0x320 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC8_WS__CSSYS_TRACEDATA14_1                               0x320 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC8_WS__DFM_MIPI_TESTDOUT_5_1                             0x320 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_SC8_SD__GPIO_MUX2_IO73_1                                  0x324 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_SC8_SD__I2S_SC8_SDO_SDI_1                                 0x324 0x000 0x01 0x00
#define     X9_PINCTRL_I2S_SC8_SD__I2S_SC7_SDI_SDO_1                                 0x324 0x57c 0x02 0x00
#define     X9_PINCTRL_I2S_SC8_SD__UART15_RX_3                                       0x324 0x4d4 0x03 0x02
#define     X9_PINCTRL_I2S_SC8_SD__I2S_MC1_SDI7_SDO7_1                               0x324 0x580 0x04 0x00
#define     X9_PINCTRL_I2S_SC8_SD__DISP_DATA13_CANFD15_RX_1                          0x324 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_SC8_SD__CSSYS_TRACEDATA15_1                               0x324 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_SC8_SD__DFM_MIPI_TESTDOUT_6_1                             0x324 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SCK__GPIO_MUX2_IO74_1                                  0x328 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SCK__I2S_MC1_SCKO_2                                    0x328 0x558 0x01 0x01
#define     X9_PINCTRL_I2S_MC_SCK__I2S_MC1_SCKI_1                                    0x328 0x584 0x02 0x00
#define     X9_PINCTRL_I2S_MC_SCK__I2C15_SCL_3                                       0x328 0x4f0 0x03 0x02
#define     X9_PINCTRL_I2S_MC_SCK__I2S_SC8_SCK_2                                     0x328 0x56c 0x04 0x01
#define     X9_PINCTRL_I2S_MC_SCK__DISP_DATA14_CANFD16_TX_1                          0x328 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SCK__PWM7_CH0_1                                        0x328 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SCK__DFM_MIPI_TESTDOUT_7_1                             0x328 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_WS__GPIO_MUX2_IO75_1                                   0x32c 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_WS__I2S_MC1_WSO_2                                      0x32c 0x560 0x01 0x01
#define     X9_PINCTRL_I2S_MC_WS__I2S_MC1_WSI_1                                      0x32c 0x588 0x02 0x00
#define     X9_PINCTRL_I2S_MC_WS__I2C15_SDA_3                                        0x32c 0x4f4 0x03 0x02
#define     X9_PINCTRL_I2S_MC_WS__I2S_SC8_WS_2                                       0x32c 0x574 0x04 0x01
#define     X9_PINCTRL_I2S_MC_WS__DISP_DATA15_CANFD16_RX_1                           0x32c 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_WS__PWM7_CH1_1                                         0x32c 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_WS__DFM_MIPI_TESTEN_1                                  0x32c 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD0__GPIO_MUX2_IO76_1                                  0x330 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD0__I2S_MC1_SDI0_SDO0_1                               0x330 0x58c 0x01 0x00
#define     X9_PINCTRL_I2S_MC_SD0__CKGEN_SEC_I2S_MCLK2_5                             0x330 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_MC_SD0__TMR7_CH0_1                                        0x330 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_MC_SD0__I2S_SC8_SDI_SDO_2                                 0x330 0x564 0x04 0x01
#define     X9_PINCTRL_I2S_MC_SD0__DISP_DATA16_CANFD17_TX_1                          0x330 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD0__PWM7_CH2_1                                        0x330 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD0__DFM_MIPI_TESTCLK_1                                0x330 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD1__GPIO_MUX2_IO77_1                                  0x334 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD1__I2S_MC1_SDI1_SDO1_1                               0x334 0x590 0x01 0x00
#define     X9_PINCTRL_I2S_MC_SD1__CKGEN_SEC_I2S_MCLK3_5                             0x334 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_MC_SD1__TMR7_CH1_1                                        0x334 0x000 0x03 0x00
#define     X9_PINCTRL_I2S_MC_SD1__I2S_SC8_SDO_SDI_2                                 0x334 0x000 0x04 0x00
#define     X9_PINCTRL_I2S_MC_SD1__DISP_DATA17_CANFD17_RX_1                          0x334 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD1__PWM7_CH3_1                                        0x334 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD1__DFM_MIPI_TESTCLR_1                                0x334 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD2__GPIO_MUX2_IO78_1                                  0x338 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD2__I2S_MC1_SDI2_SDO2_1                               0x338 0x594 0x01 0x00
#define     X9_PINCTRL_I2S_MC_SD2__SPDIF1_IN_1                                       0x338 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_MC_SD2__I2C16_SCL_2                                       0x338 0x4f8 0x03 0x01
#define     X9_PINCTRL_I2S_MC_SD2__TMR7_CH2_1                                        0x338 0x000 0x04 0x00
#define     X9_PINCTRL_I2S_MC_SD2__DISP_DATA18_CANFD18_TX_1                          0x338 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD2__CKGEN_SEC_CSI_MCLK1_3                             0x338 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD2__DFM_MIPI_CONT_EN_1                                0x338 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD3__GPIO_MUX2_IO79_1                                  0x33c 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD3__I2S_MC1_SDI3_SDO3_1                               0x33c 0x598 0x01 0x00
#define     X9_PINCTRL_I2S_MC_SD3__SPDIF1_OUT_1                                      0x33c 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_MC_SD3__I2C16_SDA_2                                       0x33c 0x4fc 0x03 0x01
#define     X9_PINCTRL_I2S_MC_SD3__TMR7_CH3_1                                        0x33c 0x000 0x04 0x00
#define     X9_PINCTRL_I2S_MC_SD3__DISP_DATA19_CANFD18_RX_1                          0x33c 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD3__CKGEN_SEC_CSI_MCLK2_3                             0x33c 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD3__DFM_MIPI_CONT_DATA_0_1                            0x33c 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD4__GPIO_MUX2_IO80_1                                  0x340 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD4__I2S_MC1_SDI4_SDO4_2                               0x340 0x568 0x01 0x01
#define     X9_PINCTRL_I2S_MC_SD4__SPDIF2_IN_1                                       0x340 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_MC_SD4__PCIE2_CLKREQ_N_3                                  0x340 0x44c 0x03 0x02
#define     X9_PINCTRL_I2S_MC_SD4__SPI8_SCLK_2                                       0x340 0x4a0 0x04 0x01
#define     X9_PINCTRL_I2S_MC_SD4__DISP_DATA20_CANFD19_TX_1                          0x340 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD4__PU_DBG1_TRST_N_1                                  0x340 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD4__DFM_MIPI_CONT_DATA_1_1                            0x340 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD5__GPIO_MUX2_IO81_1                                  0x344 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD5__I2S_MC1_SDI5_SDO5_2                               0x344 0x570 0x01 0x01
#define     X9_PINCTRL_I2S_MC_SD5__SPDIF2_OUT_1                                      0x344 0x000 0x02 0x00
#define     X9_PINCTRL_I2S_MC_SD5__PCIE1_CLKREQ_N_3                                  0x344 0x440 0x03 0x02
#define     X9_PINCTRL_I2S_MC_SD5__SPI8_MISO_2                                       0x344 0x4a4 0x04 0x01
#define     X9_PINCTRL_I2S_MC_SD5__DISP_DATA21_CANFD19_RX_1                          0x344 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD5__PU_DBG1_TMS_1                                     0x344 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD5__DFM_MIPI_CONT_DATA_2_1                            0x344 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD6__GPIO_MUX2_IO82_1                                  0x348 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD6__I2S_MC1_SDI6_SDO6_2                               0x348 0x578 0x01 0x01
#define     X9_PINCTRL_I2S_MC_SD6__I2S_MC1_SCKI_2                                    0x348 0x584 0x02 0x01
#define     X9_PINCTRL_I2S_MC_SD6__I2C16_SCL_3                                       0x348 0x4f8 0x03 0x02
#define     X9_PINCTRL_I2S_MC_SD6__SPI8_MOSI_2                                       0x348 0x4a8 0x04 0x01
#define     X9_PINCTRL_I2S_MC_SD6__DISP_DATA22_CANFD20_TX_1                          0x348 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD6__PU_DBG1_TDI_1                                     0x348 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD6__DFM_MIPI_CONT_DATA_3_1                            0x348 0x000 0x07 0x00

#define     X9_PINCTRL_I2S_MC_SD7__GPIO_MUX2_IO83_1                                  0x34c 0x000 0x00 0x00
#define     X9_PINCTRL_I2S_MC_SD7__I2S_MC1_SDI7_SDO7_2                               0x34c 0x580 0x01 0x01
#define     X9_PINCTRL_I2S_MC_SD7__I2S_MC1_WSI_2                                     0x34c 0x588 0x02 0x01
#define     X9_PINCTRL_I2S_MC_SD7__I2C16_SDA_3                                       0x34c 0x4fc 0x03 0x02
#define     X9_PINCTRL_I2S_MC_SD7__SPI8_SS_2                                         0x34c 0x4b0 0x04 0x01
#define     X9_PINCTRL_I2S_MC_SD7__DISP_DATA23_CANFD20_RX_1                          0x34c 0x000 0x05 0x00
#define     X9_PINCTRL_I2S_MC_SD7__PU_DBG1_JTAG_TCK_1                                0x34c 0x000 0x06 0x00
#define     X9_PINCTRL_I2S_MC_SD7__DFM_MIPI_CONT_DATA_4_1                            0x34c 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_CLK__GPIO_MUX2_IO84_1                                   0x350 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_CLK__MSHC1_CLK_1                                        0x350 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_CLK__SPI7_SCLK_2                                        0x350 0x4cc 0x02 0x01
#define     X9_PINCTRL_EMMC1_CLK__UART14_CTS_2                                       0x350 0x4b8 0x03 0x01
#define     X9_PINCTRL_EMMC1_CLK__I2S_MC1_SCKO_3                                     0x350 0x558 0x04 0x02
#define     X9_PINCTRL_EMMC1_CLK__UART16_TX_3                                        0x350 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_CLK__PU_DBG1_TDO_1                                      0x350 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC1_CLK__DFM_MIPI_CONT_DATA_5_1                             0x350 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_CMD__GPIO_MUX2_IO85_1                                   0x354 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_CMD__MSHC1_CMD_1                                        0x354 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_CMD__SPI7_MISO_2                                        0x354 0x4d0 0x02 0x01
#define     X9_PINCTRL_EMMC1_CMD__UART14_TX_2                                        0x354 0x000 0x03 0x00
#define     X9_PINCTRL_EMMC1_CMD__I2S_MC1_WSO_3                                      0x354 0x560 0x04 0x02
#define     X9_PINCTRL_EMMC1_CMD__UART16_RX_3                                        0x354 0x4e8 0x05 0x02
#define     X9_PINCTRL_EMMC1_CMD__PU_DBG2_TRST_N_1                                   0x354 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC1_CMD__DFM_MIPI_CONT_DATA_6_1                             0x354 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA0__GPIO_MUX2_IO86_1                                 0x358 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA0__MSHC1_DATA_0_1                                   0x358 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA0__SPI7_MOSI_2                                      0x358 0x4dc 0x02 0x01
#define     X9_PINCTRL_EMMC1_DATA0__UART14_RX_2                                      0x358 0x4c4 0x03 0x01
#define     X9_PINCTRL_EMMC1_DATA0__I2S_MC1_SDI0_SDO0_2                              0x358 0x58c 0x04 0x01
#define     X9_PINCTRL_EMMC1_DATA0__UART16_CTS_3                                     0x358 0x4d8 0x05 0x02
#define     X9_PINCTRL_EMMC1_DATA0__PU_DBG2_TMS_1                                    0x358 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC1_DATA0__DFM_MIPI_CONT_DATA_7_1                           0x358 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA1__GPIO_MUX2_IO87_1                                 0x35c 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA1__MSHC1_DATA_1_1                                   0x35c 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA1__SPI7_SS_2                                        0x35c 0x4e4 0x02 0x01
#define     X9_PINCTRL_EMMC1_DATA1__UART14_RTS_2                                     0x35c 0x000 0x03 0x00
#define     X9_PINCTRL_EMMC1_DATA1__I2S_MC1_SDI1_SDO1_2                              0x35c 0x590 0x04 0x01
#define     X9_PINCTRL_EMMC1_DATA1__UART16_RTS_3                                     0x35c 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_DATA1__PU_DBG2_TDI_1                                    0x35c 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC1_DATA1__DFM_MIPI_CONT_DATA_8_1                           0x35c 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA2__GPIO_MUX2_IO88_1                                 0x360 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA2__MSHC1_DATA_2_1                                   0x360 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA2__UART11_TX_3                                      0x360 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC1_DATA2__I2C5_SCL_2                                       0x360 0x400 0x03 0x01
#define     X9_PINCTRL_EMMC1_DATA2__I2S_MC1_SDI2_SDO2_2                              0x360 0x594 0x04 0x01
#define     X9_PINCTRL_EMMC1_DATA2__SPDIF3_IN_2                                      0x360 0x4ac 0x05 0x01
#define     X9_PINCTRL_EMMC1_DATA2__PU_DBG2_JTAG_TCK_1                               0x360 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC1_DATA2__DFM_MIPI_CONT_DATA_9_1                           0x360 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA3__GPIO_MUX2_IO89_1                                 0x364 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA3__MSHC1_DATA_3_1                                   0x364 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA3__UART11_RX_3                                      0x364 0x468 0x02 0x02
#define     X9_PINCTRL_EMMC1_DATA3__I2C5_SDA_2                                       0x364 0x40c 0x03 0x01
#define     X9_PINCTRL_EMMC1_DATA3__I2S_MC1_SDI3_SDO3_2                              0x364 0x598 0x04 0x01
#define     X9_PINCTRL_EMMC1_DATA3__SPDIF3_OUT_2                                     0x364 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_DATA3__PU_DBG2_TDO_1                                    0x364 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC1_DATA3__DFM_MIPI_CONT_DATA_10_1                          0x364 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA4__GPIO_MUX2_IO90_1                                 0x368 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA4__MSHC1_DATA_4_1                                   0x368 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA4__SPI8_SCLK_3                                      0x368 0x4a0 0x02 0x02
#define     X9_PINCTRL_EMMC1_DATA4__UART13_CTS_1                                     0x368 0x000 0x03 0x00
#define     X9_PINCTRL_EMMC1_DATA4__I2S_MC1_SDI4_SDO4_3                              0x368 0x568 0x04 0x02
#define     X9_PINCTRL_EMMC1_DATA4__PWM8_CH0_1                                       0x368 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_DATA4__TMR6_CH2_2                                       0x368 0x4e0 0x06 0x01
#define     X9_PINCTRL_EMMC1_DATA4__DFM_MIPI_STOPSTATEDATA_0_1                       0x368 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA5__GPIO_MUX2_IO91_1                                 0x36c 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA5__MSHC1_DATA_5_1                                   0x36c 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA5__SPI8_MISO_3                                      0x36c 0x4a4 0x02 0x02
#define     X9_PINCTRL_EMMC1_DATA5__UART13_TX_3                                      0x36c 0x000 0x03 0x00
#define     X9_PINCTRL_EMMC1_DATA5__I2S_MC1_SDI5_SDO5_3                              0x36c 0x570 0x04 0x02
#define     X9_PINCTRL_EMMC1_DATA5__PWM8_CH1_1                                       0x36c 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_DATA5__TMR6_CH3_2                                       0x36c 0x4ec 0x06 0x01
#define     X9_PINCTRL_EMMC1_DATA5__DFM_MIPI_STOPSTATEDATA_1_1                       0x36c 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA6__GPIO_MUX2_IO92_1                                 0x370 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA6__MSHC1_DATA_6_1                                   0x370 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA6__SPI8_MOSI_3                                      0x370 0x4a8 0x02 0x02
#define     X9_PINCTRL_EMMC1_DATA6__UART13_RX_3                                      0x370 0x4b4 0x03 0x02
#define     X9_PINCTRL_EMMC1_DATA6__I2S_MC1_SDI6_SDO6_3                              0x370 0x578 0x04 0x02
#define     X9_PINCTRL_EMMC1_DATA6__PWM8_CH2_1                                       0x370 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_DATA6__I2S_SC8_SDI_SDO_3                                0x370 0x564 0x06 0x02
#define     X9_PINCTRL_EMMC1_DATA6__DFM_MIPI_STOPSTATEDATA_2_1                       0x370 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_DATA7__GPIO_MUX2_IO93_1                                 0x374 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_DATA7__MSHC1_DATA_7_1                                   0x374 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_DATA7__SPI8_SS_3                                        0x374 0x4b0 0x02 0x02
#define     X9_PINCTRL_EMMC1_DATA7__UART13_RTS_1                                     0x374 0x000 0x03 0x00
#define     X9_PINCTRL_EMMC1_DATA7__I2S_MC1_SDI7_SDO7_3                              0x374 0x580 0x04 0x02
#define     X9_PINCTRL_EMMC1_DATA7__PWM8_CH3_1                                       0x374 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_DATA7__I2S_SC8_SCK_3                                    0x374 0x56c 0x06 0x02
#define     X9_PINCTRL_EMMC1_DATA7__DFM_MIPI_STOPSTATEDATA_3_1                       0x374 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_STROBE__GPIO_MUX2_IO94_1                                0x378 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_STROBE__MSHC1_STB_1                                     0x378 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_STROBE__UART12_TX_2                                     0x378 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC1_STROBE__I2C6_SCL_2                                      0x378 0x420 0x03 0x01
#define     X9_PINCTRL_EMMC1_STROBE__I2S_MC1_WSI_3                                   0x378 0x588 0x04 0x02
#define     X9_PINCTRL_EMMC1_STROBE__SPDIF4_IN_2                                     0x378 0x4c0 0x05 0x01
#define     X9_PINCTRL_EMMC1_STROBE__I2S_SC8_WS_3                                    0x378 0x574 0x06 0x02
#define     X9_PINCTRL_EMMC1_STROBE__DFM_MIPI_STOPSTATECLK_1                         0x378 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC1_RESET_N__GPIO_MUX2_IO95_1                               0x37c 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC1_RESET_N__MSHC1_RST_N_1                                  0x37c 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC1_RESET_N__UART12_RX_2                                    0x37c 0x474 0x02 0x01
#define     X9_PINCTRL_EMMC1_RESET_N__I2C6_SDA_2                                     0x37c 0x428 0x03 0x01
#define     X9_PINCTRL_EMMC1_RESET_N__I2S_MC1_SCKI_3                                 0x37c 0x584 0x04 0x02
#define     X9_PINCTRL_EMMC1_RESET_N__SPDIF4_OUT_2                                   0x37c 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC1_RESET_N__I2S_SC8_SDO_SDI_3                              0x37c 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC1_RESET_N__DFM_MIPI_RX_FORCERXMODE_0_1                    0x37c 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_CLK__GPIO_MUX2_IO96_1                                   0x380 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_CLK__MSHC2_CLK_1                                        0x380 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_CLK__USB1_PWR_6                                         0x380 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_CLK__I2C7_SCL_3                                         0x380 0x438 0x03 0x02
#define     X9_PINCTRL_EMMC2_CLK__I2S_MC2_SCKO_3                                     0x380 0x500 0x04 0x02
#define     X9_PINCTRL_EMMC2_CLK__PWM5_CH2_1                                         0x380 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC2_CLK__CANFD5_TX_3                                        0x380 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC2_CLK__DFM_MIPI_RX_FORCERXMODE_1_1                        0x380 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_CMD__GPIO_MUX2_IO97_1                                   0x384 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_CMD__MSHC2_CMD_1                                        0x384 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_CMD__USB1_OC_6                                          0x384 0x41c 0x02 0x05
#define     X9_PINCTRL_EMMC2_CMD__I2C7_SDA_3                                         0x384 0x444 0x03 0x02
#define     X9_PINCTRL_EMMC2_CMD__I2S_MC2_WSO_3                                      0x384 0x504 0x04 0x02
#define     X9_PINCTRL_EMMC2_CMD__PWM5_CH3_1                                         0x384 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC2_CMD__CANFD5_RX_3                                        0x384 0x414 0x06 0x02
#define     X9_PINCTRL_EMMC2_CMD__DFM_MIPI_RX_FORCERXMODE_2_1                        0x384 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA0__GPIO_MUX2_IO98_1                                 0x388 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA0__MSHC2_DATA_0_1                                   0x388 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA0__USB2_PWR_6                                       0x388 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_DATA0__SPI6_SCLK_2                                      0x388 0x404 0x03 0x01
#define     X9_PINCTRL_EMMC2_DATA0__I2S_MC2_SDI0_SDO0_3                              0x388 0x508 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA0__PCIE2_PERST_N_3                                  0x388 0x47c 0x05 0x02
#define     X9_PINCTRL_EMMC2_DATA0__CANFD6_TX_3                                      0x388 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC2_DATA0__DFM_MIPI_RX_FORCERXMODE_3_1                      0x388 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA1__GPIO_MUX2_IO99_1                                 0x38c 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA1__MSHC2_DATA_1_1                                   0x38c 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA1__USB2_OC_6                                        0x38c 0x434 0x02 0x05
#define     X9_PINCTRL_EMMC2_DATA1__SPI6_MISO_2                                      0x38c 0x410 0x03 0x01
#define     X9_PINCTRL_EMMC2_DATA1__I2S_MC2_SDI1_SDO1_3                              0x38c 0x50c 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA1__PCIE2_WAKE_N_3                                   0x38c 0x488 0x05 0x02
#define     X9_PINCTRL_EMMC2_DATA1__CANFD6_RX_3                                      0x38c 0x430 0x06 0x02
#define     X9_PINCTRL_EMMC2_DATA1__DFM_MIPI_ENABLE_0_1                              0x38c 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA2__GPIO_MUX2_IO100_1                                0x390 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA2__MSHC2_DATA_2_1                                   0x390 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA2__I2C15_SCL_4                                      0x390 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_DATA2__SPI6_MOSI_2                                      0x390 0x424 0x03 0x01
#define     X9_PINCTRL_EMMC2_DATA2__I2S_MC2_SDI2_SDO2_3                              0x390 0x510 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA2__PCIE1_PERST_N_3                                  0x390 0x490 0x05 0x02
#define     X9_PINCTRL_EMMC2_DATA2__CANFD7_TX_3                                      0x390 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC2_DATA2__DFM_MIPI_ENABLE_1_1                              0x390 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA3__GPIO_MUX2_IO101_1                                0x394 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA3__MSHC2_DATA_3_1                                   0x394 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA3__I2C15_SDA_4                                      0x394 0x4f4 0x02 0x03
#define     X9_PINCTRL_EMMC2_DATA3__SPI6_SS_2                                        0x394 0x42c 0x03 0x01
#define     X9_PINCTRL_EMMC2_DATA3__I2S_MC2_SDI3_SDO3_3                              0x394 0x514 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA3__PCIE1_WAKE_N_3                                   0x394 0x49c 0x05 0x02
#define     X9_PINCTRL_EMMC2_DATA3__CANFD7_RX_3                                      0x394 0x484 0x06 0x02
#define     X9_PINCTRL_EMMC2_DATA3__DFM_MIPI_ENABLE_2_1                              0x394 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA4__GPIO_MUX2_IO102_1                                0x398 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA4__MSHC2_DATA_4_1                                   0x398 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA4__MSHC3_DATA_3_1                                   0x398 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_DATA4__SPI5_SCLK_3                                      0x398 0x460 0x03 0x02
#define     X9_PINCTRL_EMMC2_DATA4__I2S_MC2_SDI4_SDO4_3                              0x398 0x518 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA4__USB1_PWR_7                                       0x398 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC2_DATA4__CANFD8_TX_3                                      0x398 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC2_DATA4__DFM_MIPI_ENABLE_3_1                              0x398 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA5__GPIO_MUX2_IO103_1                                0x39c 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA5__MSHC2_DATA_5_1                                   0x39c 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA5__MSHC3_DATA_2_1                                   0x39c 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_DATA5__SPI5_MISO_3                                      0x39c 0x464 0x03 0x02
#define     X9_PINCTRL_EMMC2_DATA5__I2S_MC2_SDI5_SDO5_3                              0x39c 0x51c 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA5__USB1_OC_7                                        0x39c 0x41c 0x05 0x06
#define     X9_PINCTRL_EMMC2_DATA5__CANFD8_RX_3                                      0x39c 0x498 0x06 0x02
#define     X9_PINCTRL_EMMC2_DATA5__DFM_MIPI_ENABLECLK_1                             0x39c 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA6__GPIO_MUX2_IO104_1                                0x3a0 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA6__MSHC2_DATA_6_1                                   0x3a0 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA6__MSHC3_DATA_1_1                                   0x3a0 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_DATA6__SPI5_MOSI_3                                      0x3a0 0x46c 0x03 0x02
#define     X9_PINCTRL_EMMC2_DATA6__I2S_MC2_SDI6_SDO6_3                              0x3a0 0x520 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA6__USB2_PWR_7                                       0x3a0 0x000 0x05 0x00
#define     X9_PINCTRL_EMMC2_DATA6__I2S_SC7_SDI_SDO_2                                0x3a0 0x57c 0x06 0x01
#define     X9_PINCTRL_EMMC2_DATA6__DFM_MIPI_BASEDIR_0_1                             0x3a0 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_DATA7__GPIO_MUX2_IO105_1                                0x3a4 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_DATA7__MSHC2_DATA_7_1                                   0x3a4 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_DATA7__MSHC3_DATA_0_1                                   0x3a4 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_DATA7__SPI5_SS_3                                        0x3a4 0x470 0x03 0x02
#define     X9_PINCTRL_EMMC2_DATA7__I2S_MC2_SDI7_SDO7_3                              0x3a4 0x524 0x04 0x02
#define     X9_PINCTRL_EMMC2_DATA7__USB2_OC_7                                        0x3a4 0x434 0x05 0x06
#define     X9_PINCTRL_EMMC2_DATA7__I2S_SC7_SCK_2                                    0x3a4 0x554 0x06 0x01
#define     X9_PINCTRL_EMMC2_DATA7__DFM_MIPI_TX_TXCLKLOCK_1                          0x3a4 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_STROBE__GPIO_MUX2_IO106_1                               0x3a8 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_STROBE__MSHC2_STB_1                                     0x3a8 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_STROBE__MSHC3_CLK_1                                     0x3a8 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_STROBE__I2C8_SCL_3                                      0x3a8 0x450 0x03 0x02
#define     X9_PINCTRL_EMMC2_STROBE__I2S_MC2_WSI_3                                   0x3a8 0x52c 0x04 0x02
#define     X9_PINCTRL_EMMC2_STROBE__PCIE1_CLKREQ_N_4                                0x3a8 0x440 0x05 0x03
#define     X9_PINCTRL_EMMC2_STROBE__I2S_SC7_WS_2                                    0x3a8 0x55c 0x06 0x01
#define     X9_PINCTRL_EMMC2_STROBE__DFM_MIPI_TX_BISTON_1                            0x3a8 0x000 0x07 0x00

#define     X9_PINCTRL_EMMC2_RESET_N__GPIO_MUX2_IO107_1                              0x3ac 0x000 0x00 0x00
#define     X9_PINCTRL_EMMC2_RESET_N__MSHC2_RST_N_1                                  0x3ac 0x000 0x01 0x00
#define     X9_PINCTRL_EMMC2_RESET_N__MSHC3_CMD_1                                    0x3ac 0x000 0x02 0x00
#define     X9_PINCTRL_EMMC2_RESET_N__I2C8_SDA_3                                     0x3ac 0x45c 0x03 0x02
#define     X9_PINCTRL_EMMC2_RESET_N__I2S_MC2_SCKI_3                                 0x3ac 0x530 0x04 0x02
#define     X9_PINCTRL_EMMC2_RESET_N__PCIE2_CLKREQ_N_4                               0x3ac 0x44c 0x05 0x03
#define     X9_PINCTRL_EMMC2_RESET_N__I2S_SC7_SDO_SDI_2                              0x3ac 0x000 0x06 0x00
#define     X9_PINCTRL_EMMC2_RESET_N__DFM_MIPI_TX_CLKOUT_GP_1                        0x3ac 0x000 0x07 0x00

#define     X9_PINCTRL_RESERVED 							    						0x000 0x000 0x00 0x00

#define     X9_PINCTRL_OPEN_DRAIN_DISABLE                                            0x0
#define     X9_PINCTRL_OPEN_DRAIN_ENABLE                                             0x1

#endif /* __DTS_SDX9_PINFUNC_H */
