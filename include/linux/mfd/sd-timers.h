#ifndef _LINUX_SEMIDRIVE_TIMER_H_
#define _LINUX_SEMIDRIVE_TIMER_H_

#include <linux/clk.h>
#include <linux/regmap.h>

/*****************************************************************************
** Semidrive timer each register structure description
*****************************************************************************/
#define INT_STA_OFF (0x0U)

#define BM_INT_STA_FIFO_D_OVERRUN (0x01U << 23U)

#define BM_INT_STA_FIFO_C_OVERRUN (0x01U << 22U)

#define BM_INT_STA_FIFO_B_OVERRUN (0x01U << 21U)

#define BM_INT_STA_FIFO_A_OVERRUN (0x01U << 20U)

#define BM_INT_STA_FIFO_D_UNDERRUN (0x01U << 19U)

#define BM_INT_STA_FIFO_C_UNDERRUN (0x01U << 18U)

#define BM_INT_STA_FIFO_B_UNDERRUN (0x01U << 17U)

#define BM_INT_STA_FIFO_A_UNDERRUN (0x01U << 16U)

#define BM_INT_STA_CNT_LOCAL_A_UNF (0x01U << 14U)

#define BM_INT_STA_CNT_LOCAL_D_OVF (0x01U << 13U)

#define BM_INT_STA_CNT_LOCAL_C_OVF (0x01U << 12U)

#define BM_INT_STA_CNT_LOCAL_B_OVF (0x01U << 11U)

#define BM_INT_STA_CNT_LOCAL_A_OVF (0x01U << 10U)

#define BM_INT_STA_CNT_G1_OVF (0x01U << 9U)

#define BM_INT_STA_CNT_G0_OVF (0x01U << 8U)

#define BM_INT_STA_CMP_D (0x01U << 7U)

#define BM_INT_STA_CMP_C (0x01U << 6U)

#define BM_INT_STA_CMP_B (0x01U << 5U)

#define BM_INT_STA_CMP_A (0x01U << 4U)

#define BM_INT_STA_CPT_D (0x01U << 3U)

#define BM_INT_STA_CPT_C (0x01U << 2U)

#define BM_INT_STA_CPT_B (0x01U << 1U)

#define BM_INT_STA_CPT_A (0x01U << 0U)

#define INT_STA_EN_OFF (0x4U)

#define BM_INT_STA_EN_FIFO_D_OVERRUN (0x01U << 23U)

#define BM_INT_STA_EN_FIFO_C_OVERRUN (0x01U << 22U)

#define BM_INT_STA_EN_FIFO_B_OVERRUN (0x01U << 21U)

#define BM_INT_STA_EN_FIFO_A_OVERRUN (0x01U << 20U)

#define BM_INT_STA_EN_FIFO_D_UNDERRUN (0x01U << 19U)

#define BM_INT_STA_EN_FIFO_C_UNDERRUN (0x01U << 18U)

#define BM_INT_STA_EN_FIFO_B_UNDERRUN (0x01U << 17U)

#define BM_INT_STA_EN_FIFO_A_UNDERRUN (0x01U << 16U)

#define BM_INT_STA_EN_CNT_LOCAL_A_UNF (0x01U << 14U)

#define BM_INT_STA_EN_CNT_LOCAL_D_OVF (0x01U << 13U)

#define BM_INT_STA_EN_CNT_LOCAL_C_OVF (0x01U << 12U)

#define BM_INT_STA_EN_CNT_LOCAL_B_OVF (0x01U << 11U)

#define BM_INT_STA_EN_CNT_LOCAL_A_OVF (0x01U << 10U)

#define BM_INT_STA_EN_CNT_G1_OVF (0x01U << 9U)

#define BM_INT_STA_EN_CNT_G0_OVF (0x01U << 8U)

#define BM_INT_STA_EN_CMP_D (0x01U << 7U)

#define BM_INT_STA_EN_CMP_C (0x01U << 6U)

#define BM_INT_STA_EN_CMP_B (0x01U << 5U)

#define BM_INT_STA_EN_CMP_A (0x01U << 4U)

#define BM_INT_STA_EN_CPT_D (0x01U << 3U)

#define BM_INT_STA_EN_CPT_C (0x01U << 2U)

#define BM_INT_STA_EN_CPT_B (0x01U << 1U)

#define BM_INT_STA_EN_CPT_A (0x01U << 0U)

#define INT_SIG_EN_OFF (0x8U)

#define BM_INT_SIG_EN_FIFO_D_OVERRUN (0x01U << 23U)

#define BM_INT_SIG_EN_FIFO_C_OVERRUN (0x01U << 22U)

#define BM_INT_SIG_EN_FIFO_B_OVERRUN (0x01U << 21U)

#define BM_INT_SIG_EN_FIFO_A_OVERRUN (0x01U << 20U)

#define BM_INT_SIG_EN_FIFO_D_UNDERRUN (0x01U << 19U)

#define BM_INT_SIG_EN_FIFO_C_UNDERRUN (0x01U << 18U)

#define BM_INT_SIG_EN_FIFO_B_UNDERRUN (0x01U << 17U)

#define BM_INT_SIG_EN_FIFO_A_UNDERRUN (0x01U << 16U)

#define BM_INT_SIG_EN_CNT_LOCAL_A_UNF (0x01U << 14U)

#define BM_INT_SIG_EN_CNT_LOCAL_D_OVF (0x01U << 13U)

#define BM_INT_SIG_EN_CNT_LOCAL_C_OVF (0x01U << 12U)

#define BM_INT_SIG_EN_CNT_LOCAL_B_OVF (0x01U << 11U)

#define BM_INT_SIG_EN_CNT_LOCAL_A_OVF (0x01U << 10U)

#define BM_INT_SIG_EN_CNT_G1_OVF (0x01U << 9U)

#define BM_INT_SIG_EN_CNT_G0_OVF (0x01U << 8U)

#define BM_INT_SIG_EN_CMP_D (0x01U << 7U)

#define BM_INT_SIG_EN_CMP_C (0x01U << 6U)

#define BM_INT_SIG_EN_CMP_B (0x01U << 5U)

#define BM_INT_SIG_EN_CMP_A (0x01U << 4U)

#define BM_INT_SIG_EN_CPT_D (0x01U << 3U)

#define BM_INT_SIG_EN_CPT_C (0x01U << 2U)

#define BM_INT_SIG_EN_CPT_B (0x01U << 1U)

#define BM_INT_SIG_EN_CPT_A (0x01U << 0U)

#define DMA_CTRL_OFF (0x10U)

#define FM_DMA_CTRL_CHN_D_WML (0xfU << 28U)
#define FV_DMA_CTRL_CHN_D_WML(v) (((v) << 28U) & FM_DMA_CTRL_CHN_D_WML)
#define GFV_DMA_CTRL_CHN_D_WML(v) (((v)&FM_DMA_CTRL_CHN_D_WML) >> 28U)

#define FM_DMA_CTRL_CHN_C_WML (0xfU << 24U)
#define FV_DMA_CTRL_CHN_C_WML(v) (((v) << 24U) & FM_DMA_CTRL_CHN_C_WML)
#define GFV_DMA_CTRL_CHN_C_WML(v) (((v)&FM_DMA_CTRL_CHN_C_WML) >> 24U)

#define FM_DMA_CTRL_CHN_B_WML (0xfU << 20U)
#define FV_DMA_CTRL_CHN_B_WML(v) (((v) << 20U) & FM_DMA_CTRL_CHN_B_WML)
#define GFV_DMA_CTRL_CHN_B_WML(v) (((v)&FM_DMA_CTRL_CHN_B_WML) >> 20U)

#define FM_DMA_CTRL_CHN_A_WML (0xfU << 16U)
#define FV_DMA_CTRL_CHN_A_WML(v) (((v) << 16U) & FM_DMA_CTRL_CHN_A_WML)
#define GFV_DMA_CTRL_CHN_A_WML(v) (((v)&FM_DMA_CTRL_CHN_A_WML) >> 16U)

#define BM_DMA_CTRL_CNT_OVF_SEL (0x01U << 13U)

#define FM_DMA_CTRL_CHN_D_SEL (0x03U << 11U)
#define FV_DMA_CTRL_CHN_D_SEL(v) (((v) << 11U) & FM_DMA_CTRL_CHN_D_SEL)
#define GFV_DMA_CTRL_CHN_D_SEL (((v)&FM_DMA_CTRL_CHN_D_SEL) >> 11U)

#define FM_DMA_CTRL_CHN_C_SEL (0x03U << 9U)
#define FV_DMA_CTRL_CHN_C_SEL(v) (((v) << 9U) & FM_DMA_CTRL_CHN_C_SEL)
#define GFV_DMA_CTRL_CHN_C_SEL (((v)&FM_DMA_CTRL_CHN_C_SEL) >> 9U)

#define FM_DMA_CTRL_CHN_B_SEL (0x03U << 7U)
#define FV_DMA_CTRL_CHN_B_SEL(v) (((v) << 7U) & FM_DMA_CTRL_CHN_B_SEL)
#define GFV_DMA_CTRL_CHN_B_SEL (((v)&FM_DMA_CTRL_CHN_B_SEL) >> 7U)

#define FM_DMA_CTRL_CHN_A_SEL (0x03U << 5U)
#define FV_DMA_CTRL_CHN_A_SEL(v) (((v) << 5U) & FM_DMA_CTRL_CHN_A_SEL)
#define GFV_DMA_CTRL_CHN_A_SEL (((v)&FM_DMA_CTRL_CHN_A_SEL) >> 5U)

#define BM_DMA_CTRL_CNT_OVF_EN (0x01U << 4U)

#define BM_DMA_CTRL_CHN_D_EN (0x01U << 3U)

#define BM_DMA_CTRL_CHN_C_EN (0x01U << 2U)

#define BM_DMA_CTRL_CHN_B_EN (0x01U << 1U)

#define BM_DMA_CTRL_CHN_A_EN (0x01U << 0U)

#define DMA_SIG_MASK_OFF (0x14U)

#define BM_DMA_SIG_MASK_CNT_OVF (0x01U << 4U)

#define BM_DMA_SIG_MASK_CHN_D (0x01U << 3U)

#define BM_DMA_SIG_MASK_CHN_C (0x01U << 2U)

#define BM_DMA_SIG_MASK_CHN_B (0x01U << 1U)

#define BM_DMA_SIG_MASK_CHN_A (0x01U << 0U)

#define SW_RST_OFF (0x18U)

#define BM_SW_RST_CHN_D_SOFT_RST (0x01U << 19U)

#define BM_SW_RST_CHN_C_SOFT_RST (0x01U << 18U)

#define BM_SW_RST_CHN_B_SOFT_RST (0x01U << 17U)

#define BM_SW_RST_CHN_A_SOFT_RST (0x01U << 16U)

#define FIFO_STA_OFF (0x1cU)

#define BM_FIFO_STA_FIFO_ERR_D (0x01U << 31U)

#define FM_FIFO_STA_FIFO_ENTRIES_D (0x1fU << 26U)
#define FV_FIFO_STA_FIFO_ENTRIES_D(v)                                          \
	(((v) << 26U) & FM_FIFO_STA_FIFO_ENTRIES_D)
#define GFV_FIFO_STA_FIFO_ENTRIES_D(v) (((v)&FM_FIFO_STA_FIFO_ENTRIES_D) >> 26U)

#define BM_FIFO_STA_FIFO_EMPTY_D (0x01U << 25U)

#define BM_FIFO_STA_FIFO_FULL_D (0x01U << 24U)

#define BM_FIFO_STA_FIFO_ERR_C (0x01U << 23U)

#define FM_FIFO_STA_FIFO_ENTRIES_C (0x1fU << 18U)
#define FV_FIFO_STA_FIFO_ENTRIES_C(v)                                          \
	(((v) << 18U) & FM_FIFO_STA_FIFO_ENTRIES_C)
#define GFV_FIFO_STA_FIFO_ENTRIES_C(v) (((v)&FM_FIFO_STA_FIFO_ENTRIES_C) >> 18U)

#define BM_FIFO_STA_FIFO_EMPTY_C (0x01U << 17U)

#define BM_FIFO_STA_FIFO_FULL_C (0x01U << 16U)

#define BM_FIFO_STA_FIFO_ERR_B (0x01U << 15U)

#define FM_FIFO_STA_FIFO_ENTRIES_B (0x1fU << 10U)
#define FV_FIFO_STA_FIFO_ENTRIES_B(v)                                          \
	(((v) << 10U) & FM_FIFO_STA_FIFO_ENTRIES_B)
#define GFV_FIFO_STA_FIFO_ENTRIES_B(v) (((v)&FM_FIFO_STA_FIFO_ENTRIES_B) >> 10U)

#define BM_FIFO_STA_FIFO_EMPTY_B (0x01U << 9U)

#define BM_FIFO_STA_FIFO_FULL_B (0x01U << 8U)

#define BM_FIFO_STA_FIFO_ERR_A (0x01U << 7U)

#define FM_FIFO_STA_FIFO_ENTRIES_A (0x1fU << 2U)
#define FV_FIFO_STA_FIFO_ENTRIES_A(v) (((v) << 2U) & FM_FIFO_STA_FIFO_ENTRIES_A)
#define GFV_FIFO_STA_FIFO_ENTRIES_A(v) (((v)&FM_FIFO_STA_FIFO_ENTRIES_A) >> 2U)

#define BM_FIFO_STA_FIFO_EMPTY_A (0x01U << 1U)

#define BM_FIFO_STA_FIFO_FULL_A (0x01U << 0U)

#define TIM_CLK_CONFIG_OFF (0x20U)

#define FM_TIM_CLK_CONFIG_SRC_CLK_SEL (0x3U << 16U)
#define FV_TIM_CLK_CONFIG_SRC_CLK_SEL(v)                                       \
	(((v) << 16U) & FM_TIM_CLK_CONFIG_SRC_CLK_SEL)
#define GFV_TIM_CLK_CONFIG_SRC_CLK_SEL(v)                                      \
	(((v)&FM_TIM_CLK_CONFIG_SRC_CLK_SEL) >> 16U)

#define FM_TIM_CLK_CONFIG_DIV_NUM (0xffffU << 0U)
#define FV_TIM_CLK_CONFIG_DIV_NUM(v) (((v) << 0U) & FM_TIM_CLK_CONFIG_DIV_NUM)
#define GFV_TIM_CLK_CONFIG_DIV_NUM(v) (((v)&FM_TIM_CLK_CONFIG_DIV_NUM) >> 0U)

#define CNT_CONFIG_OFF (0x24U)

#define BM_CNT_CONFIG_INDEX_EDGE_SEL (0x01U << 16U)

#define BM_CNT_CONFIG_HOME_EDGE_SEL (0x01U << 15U)

#define BM_CNT_CONFIG_INDEX_RST_EN (0x01U << 14U)

#define BM_CNT_CONFIG_HOME_RST_EN (0x01U << 13U)

#define BM_CNT_CONFIG_QUADRATURE_MODE (0x01U << 12U)

#define BM_CNT_CONFIG_LOCAL_D_RST_EN (0x01U << 11U)

#define BM_CNT_CONFIG_LOCAL_C_RST_EN (0x01U << 10U)

#define BM_CNT_CONFIG_LOCAL_B_RST_EN (0x01U << 9U)

#define BM_CNT_CONFIG_LOCAL_A_RST_EN (0x01U << 8U)

#define BM_CNT_CONFIG_CASCADE_MODE (0x01U << 6U)

#define BM_CNT_CONFIG_CNT_LOCAL_D_RLD (0x01U << 5U)

#define BM_CNT_CONFIG_CNT_LOCAL_C_RLD (0x01U << 4U)

#define BM_CNT_CONFIG_CNT_LOCAL_B_RLD (0x01U << 3U)

#define BM_CNT_CONFIG_CNT_LOCAL_A_RLD (0x01U << 2U)

#define BM_CNT_CONFIG_CNT_G1_RLD (0x01U << 1U)

#define BM_CNT_CONFIG_CNT_G0_RLD (0x01U << 0U)

#define CNT_G0_OVF_OFF (0x28U)

#define FM_CNT_G0_OVF_OVF_VAL (0xffffffffU << 0U)
#define FV_CNT_G0_OVF_OVF_VAL(v) (((v) << 0U) & FM_CNT_G0_OVF_OVF_VAL)
#define GFV_CNT_G0_OVF_OVF_VAL(v) (((v)&FM_CNT_G0_OVF_OVF_VAL) >> 0U)

#define CNT_G1_OVF_OFF (0x2cU)

#define FM_CNT_G1_OVF_OVF_VAL (0xffffffffU << 0U)
#define FV_CNT_G1_OVF_OVF_VAL(v) (((v) << 0U) & FM_CNT_G1_OVF_OVF_VAL)
#define GFV_CNT_G1_OVF_OVF_VAL(v) (((v)&FM_CNT_G1_OVF_OVF_VAL) >> 0U)

#define CNT_LOCAL_A_OVF_OFF (0x30U)

#define FM_CNT_LOCAL_A_OVF_OVF_VAL (0xffffffffU << 0U)
#define FV_CNT_LOCAL_A_OVF_OVF_VAL(v) (((v) << 0U) & FM_CNT_LOCAL_A_OVF_OVF_VAL)
#define GFV_CNT_LOCAL_A_OVF_OVF_VAL(v) (((v)&FM_CNT_LOCAL_A_OVF_OVF_VAL) >> 0U)

#define CNT_LOCAL_B_OVF_OFF (0x34U)

#define FM_CNT_LOCAL_B_OVF_OVF_VAL (0xffffffffU << 0U)
#define FV_CNT_LOCAL_B_OVF_OVF_VAL(v) (((v) << 0U) & FM_CNT_LOCAL_B_OVF_OVF_VAL)
#define GFV_CNT_LOCAL_B_OVF_OVF_VAL(v) (((v)&FM_CNT_LOCAL_B_OVF_OVF_VAL) >> 0U)

#define CNT_LOCAL_C_OVF_OFF (0x38U)

#define FM_CNT_LOCAL_C_OVF_OVF_VAL (0xffffffffU << 0U)
#define FV_CNT_LOCAL_C_OVF_OVF_VAL(v) (((v) << 0U) & FM_CNT_LOCAL_C_OVF_OVF_VAL)
#define GFV_CNT_LOCAL_C_OVF_OVF_VAL(v) (((v)&FM_CNT_LOCAL_C_OVF_OVF_VAL) >> 0U)

#define CNT_LOCAL_D_OVF_OFF (0x3cU)

#define FM_CNT_LOCAL_D_OVF_OVF_VAL (0xffffffffU << 0U)
#define FV_CNT_LOCAL_D_OVF_OVF_VAL(v) (((v) << 0U) & FM_CNT_LOCAL_D_OVF_OVF_VAL)
#define GFV_CNT_LOCAL_D_OVF_OVF_VAL(v) (((v)&FM_CNT_LOCAL_D_OVF_OVF_VAL) >> 0U)

#define CNT_G0_OFF (0x40U)

#define FM_CNT_G0_TIMER (0xffffffffU << 0U)
#define FV_CNT_G0_TIMER(v) (((v) << 0U) & FM_CNT_G0_TIMER)
#define GFV_CNT_G0_TIMER(v) (((v)&FM_CNT_G0_TIMER) >> 0U)

#define CNT_G1_OFF (0x44U)

#define FM_CNT_G1_TIMER (0xffffffffU << 0U)
#define FV_CNT_G1_TIMER(v) (((v) << 0U) & FM_CNT_G1_TIMER)
#define GFV_CNT_G1_TIMER(v) (((v)&FM_CNT_G1_TIMER) >> 0U)

#define CNT_LOCAL_A_OFF (0x48U)

#define FM_CNT_LOCAL_A_TIMER (0xffffffffU << 0U)
#define FV_CNT_LOCAL_A_TIMER(v) (((v) << 0U) & FM_CNT_LOCAL_A_TIMER)
#define GFV_CNT_LOCAL_A_TIMER(v) (((v)&FM_CNT_LOCAL_A_TIMER) >> 0U)

#define CNT_LOCAL_B_OFF (0x4cU)

#define FM_CNT_LOCAL_B_TIMER (0xffffffffU << 0U)
#define FV_CNT_LOCAL_B_TIMER(v) (((v) << 0U) & FM_CNT_LOCAL_B_TIMER)
#define GFV_CNT_LOCAL_B_TIMER(v) (((v)&FM_CNT_LOCAL_B_TIMER) >> 0U)

#define CNT_LOCAL_C_OFF (0x50U)

#define FM_CNT_LOCAL_C_TIMER (0xffffffffU << 0U)
#define FV_CNT_LOCAL_C_TIMER(v) (((v) << 0U) & FM_CNT_LOCAL_C_TIMER)
#define GFV_CNT_LOCAL_C_TIMER(v) (((v)&FM_CNT_LOCAL_C_TIMER) >> 0U)

#define CNT_LOCAL_D_OFF (0x54U)

#define FM_CNT_LOCAL_D_TIMER (0xffffffffU << 0U)
#define FV_CNT_LOCAL_D_TIMER(v) (((v) << 0U) & FM_CNT_LOCAL_D_TIMER)
#define GFV_CNT_LOCAL_D_TIMER(v) (((v)&FM_CNT_LOCAL_D_TIMER) >> 0U)

#define CMP_A_VAL_UPT_OFF (0x60U)

#define BM_CMP_A_VAL_UPT_UPT (0x01U << 0U)

#define CMP0_A_VAL_OFF (0x64U)

#define FM_CMP0_A_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP0_A_VAL_DATA(v) (((v) << 0U) & FM_CMP0_A_VAL_DATA)
#define GFV_CMP0_A_VAL_DATA(v) (((v)&FM_CMP0_A_VAL_DATA) >> 0U)

#define CMP1_A_VAL_OFF (0x68U)

#define FM_CMP1_A_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP1_A_VAL_DATA(v) (((v) << 0U) & FM_CMP1_A_VAL_DATA)
#define GFV_CMP1_A_VAL_DATA(v) (((v)&FM_CMP1_A_VAL_DATA) >> 0U)

#define CMP_B_VAL_UPT_OFF (0x70U)

#define BM_CMP_B_VAL_UPT_UPT (0x01U << 0U)

#define CMP0_B_VAL_OFF (0x74U)

#define FM_CMP0_B_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP0_B_VAL_DATA(v) (((v) << 0U) & FM_CMP0_B_VAL_DATA)
#define GFV_CMP0_B_VAL_DATA(v) (((v)&FM_CMP0_B_VAL_DATA) >> 0U)

#define CMP1_B_VAL_OFF (0x78U)

#define FM_CMP1_B_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP1_B_VAL_DATA(v) (((v) << 0U) & FM_CMP1_B_VAL_DATA)
#define GFV_CMP1_B_VAL_DATA(v) (((v)&FM_CMP1_B_VAL_DATA) >> 0U)

#define CMP_C_VAL_UPT_OFF (0x80U)

#define BM_CMP_C_VAL_UPT_UPT (0x01U << 0U)

#define CMP0_C_VAL_OFF (0x84U)

#define FM_CMP0_C_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP0_C_VAL_DATA(v) (((v) << 0U) & FM_CMP0_C_VAL_DATA)
#define GFV_CMP0_C_VAL_DATA(v) (((v)&FM_CMP0_C_VAL_DATA) >> 0U)

#define CMP1_C_VAL_OFF (0x88U)

#define FM_CMP1_C_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP1_C_VAL_DATA(v) (((v) << 0U) & FM_CMP1_C_VAL_DATA)
#define GFV_CMP1_C_VAL_DATA(v) (((v)&FM_CMP1_C_VAL_DATA) >> 0U)

#define CMP_D_VAL_UPT_OFF (0x90U)

#define BM_CMP_D_VAL_UPT_UPT (0x01U << 0U)

#define CMP0_D_VAL_OFF (0x94U)

#define FM_CMP0_D_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP0_D_VAL_DATA(v) (((v) << 0U) & FM_CMP0_D_VAL_DATA)
#define GFV_CMP0_D_VAL_DATA(v) (((v)&FM_CMP0_D_VAL_DATA) >> 0U)

#define CMP1_D_VAL_OFF (0x98U)

#define FM_CMP1_D_VAL_DATA (0xffffffffU << 0U)
#define FV_CMP1_D_VAL_DATA(v) (((v) << 0U) & FM_CMP1_D_VAL_DATA)
#define GFV_CMP1_D_VAL_DATA(v) (((v)&FM_CMP1_D_VAL_DATA) >> 0U)

#define FIFO_A_OFF (0xa0U)

#define FM_FIFO_A_DATA (0xffffffffU << 0U)
#define FV_FIFO_A_DATA(v) (((v) << 0U) & FM_FIFO_A_DATA)
#define GFV_FIFO_A_DATA(v) (((v)&FM_FIFO_A_DATA) >> 0U)

#define FIFO_B_OFF (0xa4U)

#define FM_FIFO_B_DATA (0xffffffffU << 0U)
#define FV_FIFO_B_DATA(v) (((v) << 0U) & FM_FIFO_B_DATA)
#define GFV_FIFO_B_DATA(v) (((v)&FM_FIFO_B_DATA) >> 0U)

#define FIFO_C_OFF (0xa8U)

#define FM_FIFO_C_DATA (0xffffffffU << 0U)
#define FV_FIFO_C_DATA(v) (((v) << 0U) & FM_FIFO_C_DATA)
#define GFV_FIFO_C_DATA(v) (((v)&FM_FIFO_C_DATA) >> 0U)

#define FIFO_D_OFF (0xacU)

#define FM_FIFO_D_DATA (0xffffffffU << 0U)
#define FV_FIFO_D_DATA(v) (((v) << 0U) & FM_FIFO_D_DATA)
#define GFV_FIFO_D_DATA(v) (((v)&FM_FIFO_D_DATA) >> 0U)

#define CNT_G0_INIT_OFF (0xb0U)

#define FM_CNT_G0_INIT_VALUE (0xffffffffU << 0U)
#define FV_CNT_G0_INIT_VALUE(v) (((v) << 0U) & FM_CNT_G0_INIT_VALUE)
#define GFV_CNT_G0_INIT_VALUE(v) (((v)&FM_CNT_G0_INIT_VALUE) >> 0U)

#define CNT_G1_INIT_OFF (0xb4U)

#define FM_CNT_G1_INIT_VALUE (0xffffffffU << 0U)
#define FV_CNT_G1_INIT_VALUE(v) (((v) << 0U) & FM_CNT_G1_INIT_VALUE)
#define GFV_CNT_G1_INIT_VALUE(v) (((v)&FM_CNT_G1_INIT_VALUE) >> 0U)

#define CNT_LOCAL_A_INIT_OFF (0xb8U)

#define FM_CNT_LOCAL_A_INIT_VALUE (0xffffffffU << 0U)
#define FV_CNT_LOCAL_A_INIT_VALUE(v) (((v) << 0U) & FM_CNT_LOCAL_A_INIT_VALUE)
#define GFV_CNT_LOCAL_A_INIT_VALUE(v) (((v)&FM_CNT_LOCAL_A_INIT_VALUE) >> 0U)

#define CNT_LOCAL_B_INIT_OFF (0xbcU)

#define FM_CNT_LOCAL_B_INIT_VALUE (0xffffffffU << 0U)
#define FV_CNT_LOCAL_B_INIT_VALUE(v) (((v) << 0U) & FM_CNT_LOCAL_B_INIT_VALUE)
#define GFV_CNT_LOCAL_B_INIT_VALUE(v) (((v)&FM_CNT_LOCAL_B_INIT_VALUE) >> 0U)

#define CNT_LOCAL_C_INIT_OFF (0xc0U)

#define FM_CNT_LOCAL_C_INIT_VALUE (0xffffffffU << 0U)
#define FV_CNT_LOCAL_C_INIT_VALUE(v) (((v) << 0U) & FM_CNT_LOCAL_C_INIT_VALUE)
#define GFV_CNT_LOCAL_C_INIT_VALUE(v) (((v)&FM_CNT_LOCAL_C_INIT_VALUE) >> 0U)

#define CNT_LOCAL_D_INIT_OFF (0xc4U)

#define FM_CNT_LOCAL_D_INIT_VALUE (0xffffffffU << 0U)
#define FV_CNT_LOCAL_D_INIT_VALUE(v) (((v) << 0U) & FM_CNT_LOCAL_D_INIT_VALUE)
#define GFV_CNT_LOCAL_D_INIT_VALUE(v) (((v)&FM_CNT_LOCAL_D_INIT_VALUE) >> 0U)

#define CPT_FLT_OFF (0xc8U)

#define FM_CPT_FLT_FLT_BAND_WID_D (0xfU << 28U)
#define FV_CPT_FLT_FLT_BAND_WID_D(v) (((v) << 28U) & FM_CPT_FLT_FLT_BAND_WID_D)
#define GFV_CPT_FLT_FLT_BAND_WID_D(v) (((v)&FM_CPT_FLT_FLT_BAND_WID_D) >> 28U)

#define BM_CPT_FLT_FLT_DIS_D (0x01U << 24U)

#define FM_CPT_FLT_FLT_BAND_WID_C (0xfU << 20U)
#define FV_CPT_FLT_FLT_BAND_WID_C(v) (((v) << 20U) & FM_CPT_FLT_FLT_BAND_WID_C)
#define GFV_CPT_FLT_FLT_BAND_WID_C(v) (((v)&FM_CPT_FLT_FLT_BAND_WID_C) >> 20U)

#define BM_CPT_FLT_FLT_DIS_C (0x01U << 16U)

#define FM_CPT_FLT_FLT_BAND_WID_B (0xfU << 12U)
#define FV_CPT_FLT_FLT_BAND_WID_B(v) (((v) << 12U) & FM_CPT_FLT_FLT_BAND_WID_B)
#define GFV_CPT_FLT_FLT_BAND_WID_B(v) (((v)&FM_CPT_FLT_FLT_BAND_WID_B) >> 12U)

#define BM_CPT_FLT_FLT_DIS_B (0x01U << 8U)

#define FM_CPT_FLT_FLT_BAND_WID_A (0xfU << 4U)
#define FV_CPT_FLT_FLT_BAND_WID_A(v) (((v) << 4U) & FM_CPT_FLT_FLT_BAND_WID_A)
#define GFV_CPT_FLT_FLT_BAND_WID_A(v) (((v)&FM_CPT_FLT_FLT_BAND_WID_A) >> 4U)

#define BM_CPT_FLT_FLT_DIS_A (0x01U << 0U)

#define SSE_CTRL_OFF (0xccU)

#define FM_SSE_CTRL_CPT_CMP_SEL_D (0x3U << 14U)
#define FV_SSE_CTRL_CPT_CMP_SEL_D(v) (((v) << 14U) & FM_SSE_CTRL_CPT_CMP_SEL_D)
#define GFV_SSE_CTRL_CPT_CMP_SEL_D(v) (((v)&FM_SSE_CTRL_CPT_CMP_SEL_D) >> 14U)

#define FM_SSE_CTRL_CPT_CMP_SEL_C (0x3U << 12U)
#define FV_SSE_CTRL_CPT_CMP_SEL_C(v) (((v) << 12U) & FM_SSE_CTRL_CPT_CMP_SEL_C)
#define GFV_SSE_CTRL_CPT_CMP_SEL_C(v) (((v)&FM_SSE_CTRL_CPT_CMP_SEL_C) >> 12U)

#define FM_SSE_CTRL_CPT_CMP_SEL_B (0x3U << 10U)
#define FV_SSE_CTRL_CPT_CMP_SEL_B(v) (((v) << 10U) & FM_SSE_CTRL_CPT_CMP_SEL_B)
#define GFV_SSE_CTRL_CPT_CMP_SEL_B(v) (((v)&FM_SSE_CTRL_CPT_CMP_SEL_B) >> 10U)

#define FM_SSE_CTRL_CPT_CMP_SEL_A (0x3U << 8U)
#define FV_SSE_CTRL_CPT_CMP_SEL_A(v) (((v) << 8U) & FM_SSE_CTRL_CPT_CMP_SEL_A)
#define GFV_SSE_CTRL_CPT_CMP_SEL_A(v) (((v)&FM_SSE_CTRL_CPT_CMP_SEL_A) >> 8U)

#define BM_SSE_CTRL_CMP_D_SYS_EN (0x01U << 7U)

#define BM_SSE_CTRL_CMP_C_SYS_EN (0x01U << 6U)

#define BM_SSE_CTRL_CMP_B_SYS_EN (0x01U << 5U)

#define BM_SSE_CTRL_CMP_A_SYS_EN (0x01U << 4U)

#define BM_SSE_CTRL_CPT_D_SYS_EN (0x01U << 3U)

#define BM_SSE_CTRL_CPT_C_SYS_EN (0x01U << 2U)

#define BM_SSE_CTRL_CPT_B_SYS_EN (0x01U << 1U)

#define BM_SSE_CTRL_CPT_A_SYS_EN (0x01U << 0U)

#define SSE_CPT_A_OFF (0xd0U)

#define FM_SSE_CPT_A_SSE (0xffffffffU << 0U)
#define FV_SSE_CPT_A_SSE(v) (((v) << 0U) & FM_SSE_CPT_A_SSE)
#define GFV_SSE_CPT_A_SSE(v) (((v)&FM_SSE_CPT_A_SSE) >> 0U)

#define SSE_CPT_B_OFF (0xd4U)

#define FM_SSE_CPT_B_SSE (0xffffffffU << 0U)
#define FV_SSE_CPT_B_SSE(v) (((v) << 0U) & FM_SSE_CPT_B_SSE)
#define GFV_SSE_CPT_B_SSE(v) (((v)&FM_SSE_CPT_B_SSE) >> 0U)

#define SSE_CPT_C_OFF (0xd8U)

#define FM_SSE_CPT_C_SSE (0xffffffffU << 0U)
#define FV_SSE_CPT_C_SSE(v) (((v) << 0U) & FM_SSE_CPT_C_SSE)
#define GFV_SSE_CPT_C_SSE(v) (((v)&FM_SSE_CPT_C_SSE) >> 0U)

#define SSE_CPT_D_OFF (0xdcU)

#define FM_SSE_CPT_D_SSE (0xffffffffU << 0U)
#define FV_SSE_CPT_D_SSE(v) (((v) << 0U) & FM_SSE_CPT_D_SSE)
#define GFV_SSE_CPT_D_SSE(v) (((v)&FM_SSE_CPT_D_SSE) >> 0U)

#define SSE_CMP_A_OFF (0xe0U)

#define FM_SSE_CMP_A_SSE (0xffffffffU << 0U)
#define FV_SSE_CMP_A_SSE(v) (((v) << 0U) & FM_SSE_CMP_A_SSE)
#define GFV_SSE_CMP_A_SSE(v) (((v)&FM_SSE_CMP_A_SSE) >> 0U)

#define SSE_CMP_B_OFF (0xe4U)

#define FM_SSE_CMP_B_SSE (0xffffffffU << 0U)
#define FV_SSE_CMP_B_SSE(v) (((v) << 0U) & FM_SSE_CMP_B_SSE)
#define GFV_SSE_CMP_B_SSE(v) (((v)&FM_SSE_CMP_B_SSE) >> 0U)

#define SSE_CMP_C_OFF (0xe8U)

#define FM_SSE_CMP_C_SSE (0xffffffffU << 0U)
#define FV_SSE_CMP_C_SSE(v) (((v) << 0U) & FM_SSE_CMP_C_SSE)
#define GFV_SSE_CMP_C_SSE(v) (((v)&FM_SSE_CMP_C_SSE) >> 0U)

#define SSE_CMP_D_OFF (0xecU)

#define FM_SSE_CMP_D_SSE (0xffffffffU << 0U)
#define FV_SSE_CMP_D_SSE(v) (((v) << 0U) & FM_SSE_CMP_D_SSE)
#define GFV_SSE_CMP_D_SSE(v) (((v)&FM_SSE_CMP_D_SSE) >> 0U)

#define CPT_A_CONFIG_OFF (0x100U)

#define FM_CPT_A_CONFIG_CPT_TRIG_MODE (0x3U << 5U)
#define FV_CPT_A_CONFIG_CPT_TRIG_MODE(v)                                       \
	(((v) << 5U) & FM_CPT_A_CONFIG_CPT_TRIG_MODE)
#define GFV_CPT_A_CONFIG_CPT_TRIG_MODE(v)                                      \
	(((v)&FM_CPT_A_CONFIG_CPT_TRIG_MODE) >> 5U)

#define FM_CPT_A_CONFIG_CNT_SEL (0x3U << 3U)
#define FV_CPT_A_CONFIG_CNT_SEL(v) (((v) << 3U) & FM_CPT_A_CONFIG_CNT_SEL)
#define GFV_CPT_A_CONFIG_CNT_SEL(v) (((v)&FM_CPT_A_CONFIG_CNT_SEL) >> 3U)

#define BM_CPT_A_CONFIG_DUAL_CPT_MODE (0x01U << 2U)

#define BM_CPT_A_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CPT_A_CONFIG_EN (0x01U << 0U)

#define CPT_B_CONFIG_OFF (0x104U)

#define FM_CPT_B_CONFIG_CPT_TRIG_MODE (0x3U << 5U)
#define FV_CPT_B_CONFIG_CPT_TRIG_MODE(v)                                       \
	(((v) << 5U) & FM_CPT_B_CONFIG_CPT_TRIG_MODE)
#define GFV_CPT_B_CONFIG_CPT_TRIG_MODE(v)                                      \
	(((v)&FM_CPT_B_CONFIG_CPT_TRIG_MODE) >> 5U)

#define FM_CPT_B_CONFIG_CNT_SEL (0x3U << 3U)
#define FV_CPT_B_CONFIG_CNT_SEL(v) (((v) << 3U) & FM_CPT_B_CONFIG_CNT_SEL)
#define GFV_CPT_B_CONFIG_CNT_SEL(v) (((v)&FM_CPT_B_CONFIG_CNT_SEL) >> 3U)

#define BM_CPT_B_CONFIG_DUAL_CPT_MODE (0x01U << 2U)

#define BM_CPT_B_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CPT_B_CONFIG_EN (0x01U << 0U)

#define CPT_C_CONFIG_OFF (0x108U)

#define FM_CPT_C_CONFIG_CPT_TRIG_MODE (0x3U << 5U)
#define FV_CPT_C_CONFIG_CPT_TRIG_MODE(v)                                       \
	(((v) << 5U) & FM_CPT_C_CONFIG_CPT_TRIG_MODE)
#define GFV_CPT_C_CONFIG_CPT_TRIG_MODE(v)                                      \
	(((v)&FM_CPT_C_CONFIG_CPT_TRIG_MODE) >> 5U)

#define FM_CPT_C_CONFIG_CNT_SEL (0x3U << 3U)
#define FV_CPT_C_CONFIG_CNT_SEL(v) (((v) << 3U) & FM_CPT_C_CONFIG_CNT_SEL)
#define GFV_CPT_C_CONFIG_CNT_SEL(v) (((v)&FM_CPT_C_CONFIG_CNT_SEL) >> 3U)

#define BM_CPT_C_CONFIG_DUAL_CPT_MODE (0x01U << 2U)

#define BM_CPT_C_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CPT_C_CONFIG_EN (0x01U << 0U)

#define CPT_D_CONFIG_OFF (0x10cU)

#define FM_CPT_D_CONFIG_CPT_TRIG_MODE (0x3U << 5U)
#define FV_CPT_D_CONFIG_CPT_TRIG_MODE(v)                                       \
	(((v) << 5U) & FM_CPT_D_CONFIG_CPT_TRIG_MODE)
#define GFV_CPT_D_CONFIG_CPT_TRIG_MODE(v)                                      \
	(((v)&FM_CPT_D_CONFIG_CPT_TRIG_MODE) >> 5U)

#define FM_CPT_D_CONFIG_CNT_SEL (0x3U << 3U)
#define FV_CPT_D_CONFIG_CNT_SEL(v) (((v) << 3U) & FM_CPT_D_CONFIG_CNT_SEL)
#define GFV_CPT_D_CONFIG_CNT_SEL(v) (((v)&FM_CPT_D_CONFIG_CNT_SEL) >> 3U)

#define BM_CPT_D_CONFIG_DUAL_CPT_MODE (0x01U << 2U)

#define BM_CPT_D_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CPT_D_CONFIG_EN (0x01U << 0U)

#define CMP_A_CONFIG_OFF (0x110U)

#define FM_CMP_A_CONFIG_CMP1_PULSE_WID (0xffU << 24U)
#define FV_CMP_A_CONFIG_CMP1_PULSE_WID(v)                                      \
	(((v) << 24U) & FM_CMP_A_CONFIG_CMP1_PULSE_WID)
#define GFV_CMP_A_CONFIG_CMP1_PULSE_WID(v)                                     \
	(((v)&FM_CMP_A_CONFIG_CMP1_PULSE_WID) >> 24U)

#define FM_CMP_A_CONFIG_CMP0_PULSE_WID (0xffU << 16U)
#define FV_CMP_A_CONFIG_CMP0_PULSE_WID(v)                                      \
	(((v) << 16U) & FM_CMP_A_CONFIG_CMP0_PULSE_WID)
#define GFV_CMP_A_CONFIG_CMP0_PULSE_WID(v)                                     \
	(((v)&FM_CMP_A_CONFIG_CMP0_PULSE_WID) >> 16U)

#define BM_CMP_A_CONFIG_FRC_LOW (0x01U << 15U)

#define BM_CMP_A_CONFIG_FRC_HIGH (0x01U << 14U)

#define FM_CMP_A_CONFIG_CMP1_OUT_MODE (0x7U << 7U)
#define FV_CMP_A_CONFIG_CMP1_OUT_MODE(v)                                       \
	(((v) << 7U) & FM_CMP_A_CONFIG_CMP1_OUT_MODE)
#define GFV_CMP_A_CONFIG_CMP1_OUT_MODE(v)                                      \
	(((v)&FM_CMP_A_CONFIG_CMP1_OUT_MODE) >> 7U)

#define FM_CMP_A_CONFIG_CMP0_OUT_MODE (0x7U << 4U)
#define FV_CMP_A_CONFIG_CMP0_OUT_MODE(v)                                       \
	(((v) << 4U) & FM_CMP_A_CONFIG_CMP0_OUT_MODE)
#define GFV_CMP_A_CONFIG_CMP0_OUT_MODE(v)                                      \
	(((v)&FM_CMP_A_CONFIG_CMP0_OUT_MODE) >> 4U)

#define BM_CMP_A_CONFIG_CNT_SEL (0x01U << 3U)

#define BM_CMP_A_CONFIG_DUAL_CMP_MODE (0x01U << 2U)

#define BM_CMP_A_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CMP_A_CONFIG_EN (0x01U << 0U)

#define CMP_B_CONFIG_OFF (0x114U)

#define FM_CMP_B_CONFIG_CMP1_PULSE_WID (0xffU << 24U)
#define FV_CMP_B_CONFIG_CMP1_PULSE_WID(v)                                      \
	(((v) << 24U) & FM_CMP_B_CONFIG_CMP1_PULSE_WID)
#define GFV_CMP_B_CONFIG_CMP1_PULSE_WID(v)                                     \
	(((v)&FM_CMP_B_CONFIG_CMP1_PULSE_WID) >> 24U)

#define FM_CMP_B_CONFIG_CMP0_PULSE_WID (0xffU << 16U)
#define FV_CMP_B_CONFIG_CMP0_PULSE_WID(v)                                      \
	(((v) << 16U) & FM_CMP_B_CONFIG_CMP0_PULSE_WID)
#define GFV_CMP_B_CONFIG_CMP0_PULSE_WID(v)                                     \
	(((v)&FM_CMP_B_CONFIG_CMP0_PULSE_WID) >> 16U)

#define BM_CMP_B_CONFIG_FRC_LOW (0x01U << 15U)

#define BM_CMP_B_CONFIG_FRC_HIGH (0x01U << 14U)

#define FM_CMP_B_CONFIG_CMP1_OUT_MODE (0x7U << 7U)
#define FV_CMP_B_CONFIG_CMP1_OUT_MODE(v)                                       \
	(((v) << 7U) & FM_CMP_B_CONFIG_CMP1_OUT_MODE)
#define GFV_CMP_B_CONFIG_CMP1_OUT_MODE(v)                                      \
	(((v)&FM_CMP_B_CONFIG_CMP1_OUT_MODE) >> 7U)

#define FM_CMP_B_CONFIG_CMP0_OUT_MODE (0x7U << 4U)
#define FV_CMP_B_CONFIG_CMP0_OUT_MODE(v)                                       \
	(((v) << 4U) & FM_CMP_B_CONFIG_CMP0_OUT_MODE)
#define GFV_CMP_B_CONFIG_CMP0_OUT_MODE(v)                                      \
	(((v)&FM_CMP_B_CONFIG_CMP0_OUT_MODE) >> 4U)

#define BM_CMP_B_CONFIG_CNT_SEL (0x01U << 3U)

#define BM_CMP_B_CONFIG_DUAL_CMP_MODE (0x01U << 2U)

#define BM_CMP_B_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CMP_B_CONFIG_EN (0x01U << 0U)

#define CMP_C_CONFIG_OFF (0x118U)

#define FM_CMP_C_CONFIG_CMP1_PULSE_WID (0xffU << 24U)
#define FV_CMP_C_CONFIG_CMP1_PULSE_WID(v)                                      \
	(((v) << 24U) & FM_CMP_C_CONFIG_CMP1_PULSE_WID)
#define GFV_CMP_C_CONFIG_CMP1_PULSE_WID(v)                                     \
	(((v)&FM_CMP_C_CONFIG_CMP1_PULSE_WID) >> 24U)

#define FM_CMP_C_CONFIG_CMP0_PULSE_WID (0xffU << 16U)
#define FV_CMP_C_CONFIG_CMP0_PULSE_WID(v)                                      \
	(((v) << 16U) & FM_CMP_C_CONFIG_CMP0_PULSE_WID)
#define GFV_CMP_C_CONFIG_CMP0_PULSE_WID(v)                                     \
	(((v)&FM_CMP_C_CONFIG_CMP0_PULSE_WID) >> 16U)

#define BM_CMP_C_CONFIG_FRC_LOW (0x01U << 15U)

#define BM_CMP_C_CONFIG_FRC_HIGH (0x01U << 14U)

#define FM_CMP_C_CONFIG_CMP1_OUT_MODE (0x7U << 7U)
#define FV_CMP_C_CONFIG_CMP1_OUT_MODE(v)                                       \
	(((v) << 7U) & FM_CMP_C_CONFIG_CMP1_OUT_MODE)
#define GFV_CMP_C_CONFIG_CMP1_OUT_MODE(v)                                      \
	(((v)&FM_CMP_C_CONFIG_CMP1_OUT_MODE) >> 7U)

#define FM_CMP_C_CONFIG_CMP0_OUT_MODE (0x7U << 4U)
#define FV_CMP_C_CONFIG_CMP0_OUT_MODE(v)                                       \
	(((v) << 4U) & FM_CMP_C_CONFIG_CMP0_OUT_MODE)
#define GFV_CMP_C_CONFIG_CMP0_OUT_MODE(v)                                      \
	(((v)&FM_CMP_C_CONFIG_CMP0_OUT_MODE) >> 4U)

#define BM_CMP_C_CONFIG_CNT_SEL (0x01U << 3U)

#define BM_CMP_C_CONFIG_DUAL_CMP_MODE (0x01U << 2U)

#define BM_CMP_C_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CMP_C_CONFIG_EN (0x01U << 0U)

#define CMP_D_CONFIG_OFF (0x11cU)

#define FM_CMP_D_CONFIG_CMP1_PULSE_WID (0xffU << 24U)
#define FV_CMP_D_CONFIG_CMP1_PULSE_WID(v)                                      \
	(((v) << 24U) & FM_CMP_D_CONFIG_CMP1_PULSE_WID)
#define GFV_CMP_D_CONFIG_CMP1_PULSE_WID(v)                                     \
	(((v)&FM_CMP_D_CONFIG_CMP1_PULSE_WID) >> 24U)

#define FM_CMP_D_CONFIG_CMP0_PULSE_WID (0xffU << 16U)
#define FV_CMP_D_CONFIG_CMP0_PULSE_WID(v)                                      \
	(((v) << 16U) & FM_CMP_D_CONFIG_CMP0_PULSE_WID)
#define GFV_CMP_D_CONFIG_CMP0_PULSE_WID(v)                                     \
	(((v)&FM_CMP_D_CONFIG_CMP0_PULSE_WID) >> 16U)

#define BM_CMP_D_CONFIG_FRC_LOW (0x01U << 15U)

#define BM_CMP_D_CONFIG_FRC_HIGH (0x01U << 14U)

#define FM_CMP_D_CONFIG_CMP1_OUT_MODE (0x7U << 7U)
#define FV_CMP_D_CONFIG_CMP1_OUT_MODE(v)                                       \
	(((v) << 7U) & FM_CMP_D_CONFIG_CMP1_OUT_MODE)
#define GFV_CMP_D_CONFIG_CMP1_OUT_MODE(v)                                      \
	(((v)&FM_CMP_D_CONFIG_CMP1_OUT_MODE) >> 7U)

#define FM_CMP_D_CONFIG_CMP0_OUT_MODE (0x7U << 4U)
#define FV_CMP_D_CONFIG_CMP0_OUT_MODE(v)                                       \
	(((v) << 4U) & FM_CMP_D_CONFIG_CMP0_OUT_MODE)
#define GFV_CMP_D_CONFIG_CMP0_OUT_MODE(v)                                      \
	(((v)&FM_CMP_D_CONFIG_CMP0_OUT_MODE) >> 4U)

#define BM_CMP_D_CONFIG_CNT_SEL (0x01U << 3U)

#define BM_CMP_D_CONFIG_DUAL_CMP_MODE (0x01U << 2U)

#define BM_CMP_D_CONFIG_SINGLE_MODE (0x01U << 1U)

#define BM_CMP_D_CONFIG_EN (0x01U << 0U)

typedef enum {
	TIMER_DRV_CPT_CNT_G0 = 0,
	TIMER_DRV_CPT_CNT_G0G1,
	TIMER_DRV_CPT_CNT_LOCAL,
} timer_drv_cpt_cntr_t;
typedef enum {
	TIMER_DRV_CPT_RISING_EDGE = 0,
	TIMER_DRV_CPT_FALLING_EDGE,
	TIMER_DRV_CPT_TOGGLE_EDGE,
} timer_drv_cpt_edge_t;
typedef enum {
	TIMER_DRV_SEL_HF_CLK = 0, // 400MHz
	TIMER_DRV_SEL_AHF_CLK,
	TIMER_DRV_SEL_LF_CLK, // 24MHz
	TIMER_DRV_SEL_LP_CLK,
} timer_drv_clk_sel_t;
typedef enum {
	TIMER_DRV_DMA_SEL_CMP = 0,
	TIMER_DRV_DMA_SEL_CPT,
	TIMER_DRV_DMA_SEL_OVERFLOW,
} timer_drv_dma_select_t;
typedef struct {
	uint8_t cpt_cnt_sel;
	uint8_t trig_mode;
	uint8_t dual_cpt_mode;
	uint8_t single_mode;
	uint8_t filter_dis;
	uint8_t filter_width;
	uint8_t rsvd0;
	uint8_t rsvd1;
	uint32_t first_cpt_rst_en;
	uint32_t rsvd2;
} timer_drv_func_cpt_t;

#define MAX_TIM_PSC 0xFFFF
#define TIM_CR2_MMS_SHIFT 4
#define TIM_CR2_MMS2_SHIFT 20
#define TIM_SMCR_TS_SHIFT 4
#define TIM_BDTR_BKF_MASK 0xF
#define TIM_BDTR_BKF_SHIFT 16
#define TIM_BDTR_BK2F_SHIFT 20
#define CPT_DEBOUNCE 1

struct sd_timers {
	struct clk *clk;
	struct regmap *regmap;
	u32 max_arr;
};

#define REG_DUMP(pc, off, value)                                               \
	regmap_read(pc->regmap, off, &value);                                  \
	dev_err(pc->dev, "%d, REG_DUMP %#08x: %#08x \n", __LINE__, off, value);

#endif
