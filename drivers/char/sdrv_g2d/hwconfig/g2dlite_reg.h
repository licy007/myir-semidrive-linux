/*
* g2dlite_reg.h
*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
* Description: g2dlite reg header file.
*
* Revision History:
* -----------------
* 011, 04/28/2020 BI create this file
*/

#pragma once

#define REG(x)                    (x)

#define G2DLITE_CTRL                     REG(0x0)
#define G2DLITE_SW_RST_SHIFT             31
#define G2DLITE_SW_RST_MASK              1 << G2DLITE_SW_RST_SHIFT
#define G2DLITE_EN_SHIFT                 0
#define G2DLITE_EN_MASK                  1 << G2DLITE_EN_SHIFT

#define G2DLITE_FLC_CTRL                 REG(0x4)
#define G2DLITE_FLC_TRIG_SHIFT           0
#define G2DLITE_FLC_TRIG_MASK            1 << G2DLITE_FLC_TRIG_SHIFT

#define G2DLITE_SIZE                     REG(0xc)
#define G2DLITE_SIZE_V_SHIFT             16
#define G2DLITE_SIZE_V_MASK              0xFFFF << G2DLITE_SIZE_V_SHIFT
#define G2DLITE_SIZE_H_SHIFT             0
#define G2DLITE_SIZE_H_MASK              0xFFFF << G2DLITE_SIZE_H_SHIFT

#define G2DLITE_CMDFILE_ADDR_L                 REG(0x10)
#define G2DLITE_CMDFILE_ADDR_L_ADDR_SHIFT      2
#define G2DLITE_CMDFILE_ADDR_L_ADDR_MASK       0x3FFFFFFF << G2DLITE_CMDFILE_ADDR_L_ADDR_SHIFT

#define G2DLITE_CMDFILE_ADDR_H                 REG(0x14)
#define G2DLITE_CMDFILE_ADDR_H_ADDR_SHIFT      0
#define G2DLITE_CMDFILE_ADDR_H_ADDR_MASK       0xFF << G2DLITE_CMDFILE_ADDR_H_ADDR_SHIFT

#define G2DLITE_CMDFILE_LEN                    REG(0x18)
#define G2DLITE_CMDFILE_LEN_FILE_LEN_SHIFT     0
#define G2DLITE_CMDFILE_LEN_FILE_LEN_MASK      0xFFFF << G2DLITE_CMDFILE_LEN_FILE_LEN_SHIFT

#define G2DLITE_CMDFILE_CFG                    REG(0x1c)
#define G2DLITE_CMDFILE_CFG_CMD_DMA_EN_SHIFT   3
#define G2DLITE_CMDFILE_CFG_CMD_DMA_EN_MASK    1 << G2DLITE_CMDFILE_CFG_CMD_DMA_EN_SHIFT
#define G2DLITE_CMDFILE_CFG_ENDIAN_CTRL_SHIFT  0
#define G2DLITE_CMDFILE_CFG_ENDIAN_CTRL_MASK   0x7 << G2DLITE_CMDFILE_CFG_ENDIAN_CTRL_SHIFT

#define G2DLITE_INT_MASK                   REG(0x20)
#define G2DLITE_INT_STATUS                 REG(0x24)
#define G2DLITE_INT_MASK_FRM_DONE_SHIFT    6
#define G2DLITE_INT_MASK_FRM_DONE_MASK     1 << G2DLITE_INT_MASK_FRM_DONE_SHIFT
#define G2DLITE_INT_MASK_TASK_DONE_SHIFT   5
#define G2DLITE_INT_MASK_TASK_DONE_MASK    1 << G2DLITE_INT_MASK_TASK_DONE_SHIFT
#define G2DLITE_INT_MASK_WDMA_SHIFT        4
#define G2DLITE_INT_MASK_WDMA_MASK         1 << G2DLITE_INT_MASK_WDMA_SHIFT
#define G2DLITE_INT_MASK_RLE_1_SHIFT       3
#define G2DLITE_INT_MASK_RLE_1_MASK        1 << G2DLITE_INT_MASK_RLE_1_SHIFT
#define G2DLITE_INT_MASK_MLC_SHIFT         2
#define G2DLITE_INT_MASK_MLC_MASK          1 << G2DLITE_INT_MASK_MLC_SHIFT
#define G2DLITE_INT_MASK_RLE_0_SHIFT       1
#define G2DLITE_INT_MASK_RLE_0_MASK        1 << G2DLITE_INT_MASK_RLE_0_SHIFT
#define G2DLITE_INT_MASK_RDMA_SHIFT        0
#define G2DLITE_INT_MASK_RDMA_MASK         1 << G2DLITE_INT_MASK_RDMA_SHIFT

#define G2DLITE_SPEED_ADJ                  REG(0x30)
#define G2DLITE_SPEED_ADJ_EN_SHIFT         31
#define G2DLITE_SPEED_ADJ_EN_MASK          1 << G2DLITE_SPEED_ADJ_EN_SHIFT
#define G2DLITE_SPEED_ADJ_INC_SHIFT        0
#define G2DLITE_SPEED_ADJ_INC_MASK         0x3FF << G2DLITE_SPEED_ADJ_INC_SHIFT

/* RDMA */
#define CHN_JMP 0x20
#define CHN_COUNT 5
#define RDMA_DFIFO_WML_(i)             (REG(0x1000) + CHN_JMP * (i))
#define RDMA_DFIFO_WML_SHIFT           0
#define RDMA_DFIFO_WML_MASK            (0xFFFF << RDMA_DFIFO_WML_SHIFT)

#define RDMA_DFIFO_DEPTH_(i)           (REG(0x1004) + CHN_JMP * (i))
#define RDMA_DFIFO_DEPTH_SHIFT         0
#define RDMA_DFIFO_DEPTH_MASK          (0xFFFF << RDMA_DFIFO_DEPTH_SHIFT)


#define RDMA_CFIFO_DEPTH_(i)           (REG(0x1008) + CHN_JMP * (i))
#define RDMA_CFIFO_DEPTH_SHIFT         0
#define RDMA_CFIFO_DEPTH_MASK          (0xFFFF << RDMA_CFIFO_DEPTH_SHIFT)

#define RDMA_CH_PRIO_(i)               (REG(0x100c) + CHN_JMP * (i))
#define RDMA_CH_PRIO_SCHE_SHIFT        16
#define RDMA_CH_PRIO_SCHE_MASK         0x3f << RDMA_CH_PRIO_SCHE_SHIFT
#define RDMA_CH_PRIO_P1_SHIFT          8
#define RDMA_CH_PRIO_P1_MASK           0x3f << RDMA_CH_PRIO_P1_SHIFT
#define RDMA_CH_PRIO_P0_SHIFT          0
#define RDMA_CH_PRIO_P0_MASK           0x3f << RDMA_CH_PRIO_P0_SHIFT

#define RDMA_BURST_(i)                 (REG(0x1010) + CHN_JMP * (i))
#define RDMA_BURST_MODE_SHIFT          3
#define RDMA_BURST_MODE_MASK           1 << RDMA_BURST_MODE_SHIFT
#define RDMA_BURST_LEN_SHIFT           0
#define RDMA_BURST_LEN_MASK            0x7 << RDMA_BURST_LEN_SHIFT

#define RDMA_AXI_USER_(i)              (REG(0x1014) + CHN_JMP * (i))
#define RDMA_AXI_USER_SHIFT            0
#define RDMA_AXI_USER_MASK             0xFFFFF << RDMA_AXI_USER_SHIFT

#define RDMA_AXI_CTRL_(i)              (REG(0x1018) + CHN_JMP * (i))
#define RDMA_AXI_CTRL_PORT_SHIFT       4
#define RDMA_AXI_CTRL_PORT_MASK        0x3 << RDMA_AXI_CTRL_PORT_SHIFT
#define RDMA_AXI_CTRL_CACHE_SHIFT      0
#define RDMA_AXI_CTRL_CACHE_MASK       0xF << RDMA_AXI_CTRL_CACHE_SHIFT

#define RDMA_CTRL                      REG(0x1400)
#define RDMA_CTRL_CFG_LOAD_SHIFT       1
#define RDMA_CTRL_CFG_LOAD_MASK        1 << RDMA_CTRL_CFG_LOAD_SHIFT
#define RDMA_CTRL_ARB_SEL_SHIFT        0
#define RDMA_CTRL_ARB_SEL_MASK         1 << RDMA_CTRL_ARB_SEL_SHIFT

#define RDMA_DFIFO_FULL                REG(0x1500)
#define RDMA_DFIFO_EMPTY               REG(0x1504)
#define RDMA_CFIFO_FULL                REG(0x1508)
#define RDMA_CFIFO_EMPTY               REG(0x150c)
#define RDMA_CH_IDLE                   REG(0x1510)
#define RDMA_INT_MASK                  REG(0x1520)
#define RDMA_INT_STATUS                REG(0x1524)
#define RDMA_CH_4_SHIFT                4
#define RDMA_CH_4_MASK                 1UL << RDMA_CH_4_SHIFT
#define RDMA_CH_3_SHIFT                3
#define RDMA_CH_3_MASK                 1UL << RDMA_CH_3_SHIFT
#define RDMA_CH_2_SHIFT                2
#define RDMA_CH_2_MASK                 1UL << RDMA_CH_2_SHIFT
#define RDMA_CH_1_SHIFT                1
#define RDMA_CH_1_MASK                 1UL << RDMA_CH_1_SHIFT
#define RDMA_CH_0_SHIFT                0
#define RDMA_CH_0_MASK                 1UL << RDMA_CH_0_SHIFT

#define RDMA_DEBUG_CTRL                REG(0x1540)
#define RDMA_SEL_SHIFT                 0
#define RDMA_SEL_MASK                  0x1F << RDMA_SEL_SHIFT

#define RDMA_DEBUG_STA                 REG(0x1544)
#define RDMA_CFIFO_DEP_SHIFT           16
#define RDMA_CFIFO_DEP_MASK            0xFFFF << RDMA_CFIFO_DEP_SHIFT
#define RDMA_DFIFO_DEP_SHIFT           0
#define RDMA_DFIFO_DEP_MASK            0xFFFF << RDMA_DFIFO_DEP_SHIFT

/* GP */
#define G2DLITE_GP_PIX_COMP            REG(0x0)
#define BPV_SHIFT                      24
#define BPV_MASK                       0xF << BPV_SHIFT
#define BPU_SHIFT                      16
#define BPU_MASK                       0xF << BPU_SHIFT
#define BPY_SHIFT                      8
#define BPY_MASK                       0x1F << BPY_SHIFT
#define BPA_SHIFT                      0
#define BPA_MASK                       0xF << BPA_SHIFT

#define G2DLITE_GP_FRM_CTRL            REG(0x4)
#define PIPE_ENDIAN_CTRL_SHIFT              16
#define PIPE_ENDIAN_CTRL_MASK               0x7 << PIPE_ENDIAN_CTRL_SHIFT
#define PIPE_COMP_SWAP_SHIFT                12
#define PIPE_COMP_SWAP_MASK                 0xF << PIPE_COMP_SWAP_SHIFT
#define PIPE_ROT_SHIFT                      9
#define PIPE_ROT_MASK                       0x7 << PIPE_ROT_SHIFT
#define PIPE_RGB_YUV_SHIFT                  8
#define PIPE_RGB_YUV_MASK                   1 << PIPE_RGB_YUV_SHIFT
#define PIPE_UV_SWAP_SHIFT                  7
#define PIPE_UV_SWAP_MASK                   1 << PIPE_UV_SWAP_SHIFT
#define PIPE_UV_MODE_SHIFT                  5
#define PIPE_UV_MODE_MASK                   0x3 << PIPE_UV_MODE_SHIFT
#define PIPE_MODE_SHIFT                     2
#define PIPE_MODE_MASK                      0x7 << PIPE_MODE_SHIFT
#define PIPE_FMT_SHIFT                      0
#define PIPE_FMT_MASK                       0x3 << PIPE_FMT_SHIFT

#define G2DLITE_GP_FRM_SIZE            REG(0x8)
#define FRM_HEIGHT_SHIFT               16
#define FRM_HEIGHT_MASK                0xFFFF << FRM_HEIGHT_SHIFT
#define FRM_WIDTH_SHIFT                0
#define FRM_WIDTH_MASK                 0xFFFF << FRM_WIDTH_SHIFT

#define G2DLITE_GP_Y_BADDR_L           REG(0xc)
#define BADDR_L_Y_SHIFT                0
#define BADDR_L_Y_MASK                 0xFFFFFFFF << BADDR_L_Y_SHIFT

#define G2DLITE_GP_Y_BADDR_H           REG(0x10)
#define BADDR_H_Y_SHIFT                0
#define BADDR_H_Y_MASK                 0xFF << BADDR_H_Y_SHIFT

#define G2DLITE_GP_U_BADDR_L           REG(0x14)
#define BADDR_L_U_SHIFT                0
#define BADDR_L_U_MASK                 0xFFFFFFFF << BADDR_L_U_SHIFT

#define G2DLITE_GP_U_BADDR_H           REG(0x18)
#define BADDR_H_U_SHIFT                0
#define BADDR_H_U_MASK                 0xFF << BADDR_H_U_SHIFT

#define G2DLITE_GP_V_BADDR_L           REG(0x1c)
#define BADDR_L_V_SHIFT                0
#define BADDR_L_V_MASK                 0xFFFFFFFF << BADDR_L_V_SHIFT

#define G2DLITE_GP_V_BADDR_H           REG(0x20)
#define BADDR_H_V_SHIFT                0
#define BADDR_H_V_MASK                 0xFF << BADDR_H_V_SHIFT

#define G2DLITE_GP_Y_STRIDE            REG(0x2c)
#define STRIDE_Y_SHIFT                 0
#define STRIDE_Y_MASK                  0x3FFFFUL << STRIDE_Y_SHIFT

#define G2DLITE_GP_U_STRIDE            REG(0x30)
#define STRIDE_U_SHIFT                 0
#define STRIDE_U_MASK                  0x3FFFFUL << STRIDE_U_SHIFT

#define G2DLITE_GP_V_STRIDE            REG(0x34)
#define STRIDE_V_SHIFT                 0
#define STRIDE_V_MASK                  0x3FFFFUL << STRIDE_V_SHIFT

#define G2DLITE_GP_FRM_OFFSET          REG(0x40)
#define FRM_Y_SHIFT                    16
#define FRM_Y_MASK                     0xFFFFUL << FRM_Y_SHIFT
#define FRM_X_SHIFT                    0
#define FRM_X_MASK                     0xFFFFUL << FRM_X_SHIFT

#define G2DLITE_GP_YUVUP_CTRL               REG(0x44)
#define G2DLITE_GP_YUVUP_EN_SHIFT           31
#define G2DLITE_GP_YUVUP_EN_MASK            0x1 << G2DLITE_GP_YUVUP_EN_SHIFT
#define G2DLITE_GP_YUVUP_VOFSET_SHIFT       6
#define G2DLITE_GP_YUVUP_VOFSET_MASK        0x3 << G2DLITE_GP_YUVUP_VOFSET_SHIFT
#define G2DLITE_GP_YUVUP_HOFSET_SHIFT       4
#define G2DLITE_GP_YUVUP_HOFSET_MASK        0x3 << G2DLITE_GP_YUVUP_HOFSET_SHIFT
#define G2DLITE_GP_YUVUP_FILTER_MODE_SHIFT  3
#define G2DLITE_GP_YUVUP_FILTER_MODE_MASK   0x1 << G2DLITE_GP_YUVUP_FILTER_MODE_SHIFT
#define G2DLITE_GP_YUVUP_UPV_BYPASS_SHIFT   2
#define G2DLITE_GP_YUVUP_UPV_BYPASS_MASK    0x1 << G2DLITE_GP_YUVUP_UPV_BYPASS_SHIFT
#define G2DLITE_GP_YUVUP_UPH_BYPASS_SHIFT   1
#define G2DLITE_GP_YUVUP_UPH_BYPASS_MASK    0x1 << G2DLITE_GP_YUVUP_UPH_BYPASS_SHIFT
#define G2DLITE_GP_YUVUP_BYPASS_SHIFT       0
#define G2DLITE_GP_YUVUP_BYPASS_MASK        0x1 << G2DLITE_GP_YUVUP_BYPASS_SHIFT

#define G2DLITE_GP_CROP_CTRL                REG(0x100)
#define G2DLITE_GP_CROP_BYPASS_SHIFT        0
#define G2DLITE_GP_CROP_BYPASS_MASK         0x1 << G2DLITE_GP_CROP_BYPASS_SHIFT

#define G2DLITE_GP_CROP_UL_POS              REG(0x104)
#define G2DLITE_GP_CROP_UL_POS_Y_SHIFT      16
#define G2DLITE_GP_CROP_UL_POS_Y_MASK       0xFFFF << G2DLITE_GP_CROP_UL_POS_Y_SHIFT
#define G2DLITE_GP_CROP_UL_POS_X_SHIFT      0
#define G2DLITE_GP_CROP_UL_POS_X_MASK       0xFFFF << G2DLITE_GP_CROP_UL_POS_X_SHIFT

#define G2DLITE_GP_CROP_SIZE                REG(0x108)
#define G2DLITE_GP_CROP_SIZE_V_SHIFT        16
#define G2DLITE_GP_CROP_SIZE_V_MASK         0xFFFF << G2DLITE_GP_CROP_SIZE_V_SHIFT
#define G2DLITE_GP_CROP_SIZE_H_SHIFT        0
#define G2DLITE_GP_CROP_SIZE_H_MASK         0xFFFF << G2DLITE_GP_CROP_SIZE_H_SHIFT

#define G2DLITE_GP_CROP_PAR_ERR             REG(0x120)
#define G2DLITE_GP_CROP_PAR_STATUS_SHIFT    0
#define G2DLITE_GP_CROP_PAR_STATUS_MASK     0x1 << G2DLITE_GP_CROP_PAR_STATUS_SHIFT

/*GP CSC*/
#define GP_CSC_CTRL                     REG(0x200)
#define GP_CSC_ALPHA_SHIFT              2
#define GP_CSC_ALPHA_MASK               0x1 << GP_CSC_ALPHA_SHIFT
#define GP_CSC_SBUP_CONV_SHIFT          1
#define GP_CSC_SBUP_CONV_MASK           0x1 << GP_CSC_SBUP_CONV_SHIFT
#define GP_CSC_BYPASS_SHIFT             0
#define GP_CSC_BYPASS_MASK              0x1 << GP_CSC_BYPASS_SHIFT

#define GP_CSC_COEF_COEF1                    REG(0x204)
#define GP_CSC_COEF1_A01_SHIFT          16
#define GP_CSC_COEF1_A01_MASK           0x3FFF << GP_CSC_COEF1_A01_SHIFT
#define GP_CSC_COEF1_A00_SHIFT          0
#define GP_CSC_COEF1_A00_MASK           0x3FFF << GP_CSC_COEF1_A00_SHIFT

#define GP_CSC_COEF_COEF2                    REG(0x208)
#define GP_CSC_COEF2_A10_SHIFT          16
#define GP_CSC_COEF2_A10_MASK           0x3FFF << GP_CSC_COEF2_A10_SHIFT
#define GP_CSC_COEF2_A02_SHIFT          0
#define GP_CSC_COEF2_A02_MASK           0x3FFF << GP_CSC_COEF2_A02_SHIFT

#define GP_CSC_COEF_COEF3                    REG(0x20c)
#define GP_CSC_COEF3_A12_SHIFT          16
#define GP_CSC_COEF3_A12_MASK           0x3FFF << GP_CSC_COEF3_A12_SHIFT
#define GP_CSC_COEF3_A11_SHIFT          0
#define GP_CSC_COEF3_A11_MASK           0x3FFF << GP_CSC_COEF3_A11_SHIFT

#define GP_CSC_COEF_COEF4                    REG(0x210)
#define GP_CSC_COEF4_A21_SHIFT          16
#define GP_CSC_COEF4_A21_MASK           0x3FFF << GP_CSC_COEF4_A21_SHIFT
#define GP_CSC_COEF4_A20_SHIFT          0
#define GP_CSC_COEF4_A20_MASK           0x3FFF << GP_CSC_COEF4_A20_SHIFT

#define GP_CSC_COEF_COEF5                    REG(0x214)
#define GP_CSC_COEF5_B0_SHIFT           16
#define GP_CSC_COEF5_B0_MASK            0x3FFF << GP_CSC_COEF5_B0_SHIFT
#define GP_CSC_COEF5_A22_SHIFT          0
#define GP_CSC_COEF5_A22_MASK           0x3FFF << GP_CSC_COEF5_A22_SHIFT

#define GP_CSC_COEF_COEF6                    REG(0x218)
#define GP_CSC_COEF6_B2_SHIFT           16
#define GP_CSC_COEF6_B2_MASK            0x3FFF << GP_CSC_COEF6_B2_SHIFT
#define GP_CSC_COEF6_B1_SHIFT           0
#define GP_CSC_COEF6_B1_MASK            0x3FFF << GP_CSC_COEF6_B1_SHIFT

#define GP_CSC_COEF_COEF7                    REG(0x21c)
#define GP_CSC_COEF7_C1_SHIFT           16
#define GP_CSC_COEF7_C1_MASK            0x3FF << GP_CSC_COEF7_C1_SHIFT
#define GP_CSC_COEF7_C0_SHIFT           0
#define GP_CSC_COEF7_C0_MASK            0x3FF << GP_CSC_COEF7_C0_SHIFT

#define GP_CSC_COEF_COEF8                    REG(0x220)
#define GP_CSC_COEF8_C2_SHIFT           0
#define GP_CSC_COEF8_C2_MASK            0x3FF << GP_CSC_COEF8_C2_SHIFT

#define GP_HS_HS_CTRL                      REG(0x300)
#define GP_HS_CTRL_NOR_PARA_SHIFT       8
#define GP_HS_CTRL_NOR_PARA_MASK        0xF << GP_HS_CTRL_NOR_PARA_SHIFT
#define GP_HS_CTRL_APB_RD_SHIFT         4
#define GP_HS_CTRL_APB_RD_MASK          1 << GP_HS_CTRL_APB_RD_SHIFT
#define GP_HS_CTRL_FILTER_EN_V_SHIFT    3
#define GP_HS_CTRL_FILTER_EN_V_MASK     1 << GP_HS_CTRL_FILTER_EN_V_SHIFT
#define GP_HS_CTRL_FILTER_EN_U_SHIFT    2
#define GP_HS_CTRL_FILTER_EN_U_MASK     1 << GP_HS_CTRL_FILTER_EN_U_SHIFT
#define GP_HS_CTRL_FILTER_EN_Y_SHIFT    1
#define GP_HS_CTRL_FILTER_EN_Y_MASK     1 << GP_HS_CTRL_FILTER_EN_Y_SHIFT
#define GP_HS_CTRL_FILTER_EN_A_SHIFT    0
#define GP_HS_CTRL_FILTER_EN_A_MASK     1 << GP_HS_CTRL_FILTER_EN_A_SHIFT

#define GP_HS_HS_INI                       REG(0x304)
#define GP_HS_INI_POLA_SHIFT            19
#define GP_HS_INI_POLA_MASK             1 << GP_HS_INI_POLA_SHIFT
#define GP_HS_INI_FRA_SHIFT             0
#define GP_HS_INI_FRA_MASK              0x7FFFF << GP_HS_INI_FRA_SHIFT

#define GP_HS_HS_RATIO                     REG(0x308)
#define GP_HS_RATIO_INT_SHIFT           19
#define GP_HS_RATIO_INT_MASK            0x7 << GP_HS_RATIO_INT_SHIFT
#define GP_HS_RATIO_FRA_SHIFT           0
#define GP_HS_RATIO_FRA_MASK            0x7FFFF << GP_HS_RATIO_FRA_SHIFT

#define GP_HS_HS_WIDTH                     REG(0x30c)
#define GP_HS_WIDTH_OUT_SHIFT           0
#define GP_HS_WIDTH_OUT_MASK            0xFFFF << GP_HS_WIDTH_OUT_SHIFT

#define GP_VS_VS_CTRL                      REG(0x400)
#define GP_VS_CTRL_NORM_SHIFT           4
#define GP_VS_CTRL_NORM_MASK            0xF << GP_VS_CTRL_NORM_SHIFT
#define GP_VS_CTRL_PARITY_SHIFT         3
#define GP_VS_CTRL_PARITY_MASK          1 << GP_VS_CTRL_PARITY_SHIFT
#define GP_VS_CTRL_PXL_MODE_SHIFT       2
#define GP_VS_CTRL_PXL_MODE_MASK        1 << GP_VS_CTRL_PXL_MODE_SHIFT
#define GP_VS_CTRL_VS_MODE_SHIFT        0
#define GP_VS_CTRL_VS_MODE_MASK         0x3 << GP_VS_CTRL_VS_MODE_SHIFT

#define GP_VS_VS_RESV                      REG(0x404)
#define GP_VS_RESV_VSIZE_SHIFT          0
#define GP_VS_RESV_VSIZE_MASK           0xFFFF << GP_VS_RESV_VSIZE_SHIFT

#define GP_VS_VS_INC                       REG(0x408)
#define GP_VS_INC_INC_SHIFT             0
#define GP_VS_INC_INC_MASK              0x1FFFFF << GP_VS_INC_INC_SHIFT

#define GP_VS_VS_INC_INC_E                    REG(0x40c)
#define GP_VS_VS_INC_INC_O                    REG(0x410)
#define GP_VS_INC_INC_E_INIT_POS_SHIFT     18
#define GP_VS_INC_INC_E_INIT_POS_MASK      0x3 << GP_VS_INC_INC_E_INIT_POS_SHIFT
#define GP_VS_INC_INC_E_INIT_PHASE_SHIFT   0
#define GP_VS_INC_INC_E_INIT_PHASE_MASK    0X3FFFF << GP_VS_INC_INC_E_INIT_PHASE_SHIFT

#define GP_RE_RE_STATUS                       REG(0x414)
#define GP_RE_STATUS_V_FRAME_END_SHIFT     2
#define GP_RE_STATUS_V_FRAME_END_MASK      1 << GP_RE_STATUS_V_FRAME_END_SHIFT
#define GP_RE_STATUS_U_FRAME_END_SHIFT     1
#define GP_RE_STATUS_U_FRAME_END_MASK      1 << GP_RE_STATUS_U_FRAME_END_SHIFT
#define GP_RE_STATUS_Y_FRAME_END_SHIFT     0
#define GP_RE_STATUS_Y_FRAME_END_MASK      1 << GP_RE_STATUS_Y_FRAME_END_SHIFT

#define GP_SDW_CTRL_CTRL                        REG(0xf00)
#define GP_SDW_CTRL_TRIG_SHIFT             0
#define GP_SDW_CTRL_TRIG_MASK              1 << GP_SDW_CTRL_TRIG_SHIFT

/* SP */
#define G2DLITE_SP_PIX_COMP                   REG(0x000)
/* SHIFT MASK define see G2D_GP_XX area  */

#define G2DLITE_SP_FRM_CTRL                   REG(0x004)
#define G2DLITE_SP_ENDIAN_CTRL_SHIFT          16
#define G2DLITE_SP_ENDIAN_CTRL_MASK           0x7 << G2DLITE_SP_ENDIAN_CTRL_SHIFT
#define G2DLITE_SP_COMP_SWAP_SHIFT            12
#define G2DLITE_SP_COMP_SWAP_MASK             0xF << G2DLITE_SP_COMP_SWAP_SHIFT
#define G2DLITE_SP_ROT_SHIFT                  8
#define G2DLITE_SP_ROT_MASK                   0x7 << G2DLITE_SP_ROT_SHIFT
#define G2DLITE_SP_RGB_YUV_SHIFT              7
#define G2DLITE_SP_RGB_YUV_MASK               1UL << G2DLITE_SP_RGB_YUV_SHIFT
#define G2DLITE_SP_UV_SWAP_SHIFT              6
#define G2DLITE_SP_UV_SWAP_MASK               1UL << G2DLITE_SP_UV_SWAP_SHIFT
#define G2DLITE_SP_UV_MODE_SHIFT              4
#define G2DLITE_SP_UV_MODE_MASK               0x3 << G2DLITE_SP_UV_MODE_SHIFT
#define G2DLITE_SP_MODE_SHIFT                 2
#define G2DLITE_SP_MODE_MASK                  0x3 << G2DLITE_SP_MODE_SHIFT
#define G2DLITE_SP_FMT_SHIFT                  0
#define G2DLITE_SP_FMT_MASK                   0x3 << G2DLITE_SP_FMT_SHIFT

#define G2DLITE_SP_FRM_SIZE                   REG(0x008)
#define G2DLITE_SP_Y_BADDR_L                  REG(0x00c)
#define G2DLITE_SP_Y_BADDR_H                  REG(0x010)
#define G2DLITE_SP_Y_STRIDE                   REG(0x02c)
#define G2DLITE_SP_FRM_OFFSET                 REG(0x040)
/* SHIFT MASK define see G2D_GP_XX area  */

/* RLE */
#define RLE_Y_Y_LEN                         REG(0x100)
#define RLE_Y_LEN_Y_SHIFT                 0
#define RLE_Y_LEN_Y_MASK                  0xFFFFFF << RLE_Y_LEN_Y_SHIFT

#define RLE_Y_Y_CHECK_SUM                   REG(0x5110)
#define RLE_Y_CHECK_SUM_Y_SHIFT           0
#define RLE_Y_CHECK_SUM_Y_MASK            0xFFFFFFFF << RLE_Y_CHECK_SUM_Y_SHIFT

#define RLE_CTRL                          REG(0x5120)
#define RLE_DATA_SIZE_SHIFT               1
#define RLE_DATA_SIZE_MASK                0x3 << RLE_DATA_SIZE_SHIFT
#define RLE_EN_SHIFT                      0
#define RLE_EN_MASK                       0x1 << RLE_EN_SHIFT

#define RLE_Y_CHECK_SUM_ST                REG(0x130)
#define RLE_U_CHECK_SUM_ST                REG(0x134)
#define RLE_V_CHECK_SUM_ST                REG(0x138)
#define RLE_A_CHECK_SUM_ST                REG(0x13c)

#define RLE_INT_MASK                      REG(0x140)
#define RLE_INT_STATUS                    REG(0x144)
#define RLE_INT_V_ERR_SHIFT               3
#define RLE_INT_V_ERR_MASK                0x1 << RLE_INT_V_ERR_SHIFT
#define RLE_INT_U_ERR_SHIFT               2
#define RLE_INT_U_ERR_MASK                0x1 << RLE_INT_U_ERR_SHIFT
#define RLE_INT_Y_ERR_SHIFT               1
#define RLE_INT_Y_ERR_MASK                0x1 << RLE_INT_Y_ERR_SHIFT
#define RLE_INT_A_ERR_SHIFT               0
#define RLE_INT_A_ERR_MASK                0x1 << RLE_INT_A_ERR_SHIFT

/* CLUT */
#define CLUT_A_CTRL                  REG(0x200)
#define CLUT_A_HAS_ALPHA_SHIFT       18
#define CLUT_A_HAS_ALPHA_MASK        0x1 << CLUT_A_HAS_ALPHA_SHIFT
#define CLUT_A_Y_SEL_SHIFT           17
#define CLUT_A_Y_SEL_MASK            0x1 << CLUT_A_Y_SEL_SHIFT
#define CLUT_A_BYPASS_SHIFT          16
#define CLUT_A_BYPASS_MASK           0x1 << CLUT_A_BYPASS_SHIFT
#define CLUT_A_OFFSET_SHIFT          8
#define CLUT_A_OFFSET_MASK           0xFF << CLUT_A_OFFSET_SHIFT
#define CLUT_A_DEPTH_SHIFT           0
#define CLUT_A_DEPTH_MASK            0xF << CLUT_A_DEPTH_SHIFT

#define CLUT_Y_CTRL                  REG(0x204)
#define CLUT_Y_BYPASS_SHIFT          16
#define CLUT_Y_BYPASS_MASK           0x1 << CLUT_Y_BYPASS_SHIFT
#define CLUT_Y_OFFSET_SHIFT          8
#define CLUT_Y_OFFSET_MASK           0xFF << CLUT_Y_OFFSET_SHIFT
#define CLUT_Y_DEPTH_SHIFT           0
#define CLUT_Y_DEPTH_MASK            0xF << CLUT_Y_DEPTH_SHIFT

#define CLUT_U_CTRL                  REG(0x208)
#define CLUT_U_Y_SEL_SHIFT           17
#define CLUT_U_Y_SEL_MASK            0x1 << CLUT_U_Y_SEL_SHIFT
#define CLUT_U_BYPASS_SHIFT          16
#define CLUT_U_BYPASS_MASK           0x1 << CLUT_U_BYPASS_SHIFT
#define CLUT_U_OFFSET_SHIFT          8
#define CLUT_U_OFFSET_MASK           0xFF << CLUT_U_OFFSET_SHIFT
#define CLUT_U_DEPTH_SHIFT           0
#define CLUT_U_DEPTH_MASK            0xF << CLUT_U_DEPTH_SHIFT

#define CLUT_V_CTRL                  REG(0x20c)
#define CLUT_V_Y_SEL_SHIFT           17
#define CLUT_V_Y_SEL_MASK            0x1 << CLUT_V_Y_SEL_SHIFT
#define CLUT_V_BYPASS_SHIFT          16
#define CLUT_V_BYPASS_MASK           0x1 << CLUT_V_BYPASS_SHIFT
#define CLUT_V_OFFSET_SHIFT          8
#define CLUT_V_OFFSET_MASK           0xFF << CLUT_V_OFFSET_SHIFT
#define CLUT_V_DEPTH_SHIFT           0
#define CLUT_V_DEPTH_MASK            0xF << CLUT_V_DEPTH_SHIFT

#define CLUT_READ_CTRL               REG(0x210)
#define CLUT_V_SEL_SHIFT             3
#define CLUT_V_SEL_MASK              0x1 << CLUT_V_SEL_SHIFT
#define CLUT_U_SEL_SHIFT             2
#define CLUT_U_SEL_MASK              0x1 << CLUT_U_SEL_SHIFT
#define CLUT_Y_SEL_SHIFT             1
#define CLUT_Y_SEL_MASK              0x1 << CLUT_Y_SEL_SHIFT
#define CLUT_A_SEL_SHIFT             0
#define CLUT_A_SEL_MASK              0x1 << CLUT_A_SEL_SHIFT

/* SP_SDW_CTRL */
#define SP_SDW_CTRL_CTRL                  REG(0xf00)
#define SP_SDW_CTRL_TRIG_SHIFT       0
#define SP_SDW_CTRL_TRIG_MASK        0x1 << SP_SDW_CTRL_TRIG_SHIFT


/*MLC*/
#define MLC_LAYER_JMP              0x30
#define MLC_PATH_JMP               0x4

#define MLC_SF_CTRL_(i)              (REG(0x7000) + MLC_LAYER_JMP * (i))
#define MLC_SF_PROT_VAL_SHIFT        8
#define MLC_SF_PROT_VAL_MASK         0x3F << MLC_SF_PROT_VAL_SHIFT
#define MLC_SF_VPOS_PROT_EN_SHIFT    7
#define MLC_SF_VPOS_PROT_EN_MASK     0x1 << MLC_SF_VPOS_PROT_EN_SHIFT
#define MLC_SF_SLOWDOWN_EN_SHIFT     6
#define MLC_SF_SLOWDOWN_EN_MASK      0x1 << MLC_SF_SLOWDOWN_EN_SHIFT
#define MLC_SF_AFLU_PSEL_SHIFT       5
#define MLC_SF_AFLU_PSEL_MASK        0x1 << MLC_SF_AFLU_PSEL_SHIFT
#define MLC_SF_AFLU_EN_SHIFT         4
#define MLC_SF_AFLU_EN_MASK          0x1 << MLC_SF_AFLU_EN_SHIFT
#define MLC_SF_CKEY_EN_SHIFT         3
#define MLC_SF_CKEY_EN_MASK          0x1 << MLC_SF_CKEY_EN_SHIFT
#define MLC_SF_G_ALPHA_EN_SHIFT      2
#define MLC_SF_G_ALPHA_EN_MASK       0x1 << MLC_SF_G_ALPHA_EN_SHIFT
#define MLC_SF_CROP_EN_SHIFT         1
#define MLC_SF_CROP_EN_MASK          0x1 << MLC_SF_CROP_EN_SHIFT
#define MLC_SF_EN_SHIFT              0
#define MLC_SF_EN_MASK               0x1 << MLC_SF_EN_SHIFT

#define MLC_SF_H_H_SPOS_(i)            (REG(0x7004) + MLC_LAYER_JMP * i)
#define MLC_SF_H_SPOS_H_SHIFT        0
#define MLC_SF_H_SPOS_H_MASK         0x1FFFF << MLC_SF_H_SPOS_H_SHIFT

#define MLC_SF_V_V_SPOS_(i)            (REG(0x7008) + MLC_LAYER_JMP * i)
#define MLC_SF_V_SPOS_V_SHIFT        0
#define MLC_SF_V_SPOS_V_MASK         0x1FFFF << MLC_SF_V_SPOS_V_SHIFT

#define MLC_SF_SF_SIZE_(i)              (REG(0x700c) + MLC_LAYER_JMP * i)
#define MLC_SF_SIZE_V_SHIFT          16
#define MLC_SF_SIZE_V_MASK           0xFFFF << MLC_SF_SIZE_V_SHIFT
#define MLC_SF_SIZE_H_SHIFT          0
#define MLC_SF_SIZE_H_MASK           0xFFFF << MLC_SF_SIZE_H_SHIFT

#define MLC_SF_CROP_H_POS_(i)        (REG(0x7010) + MLC_LAYER_JMP * i)
#define MLC_SF_CROP_V_POS_(i)        (REG(0x7014) + MLC_LAYER_JMP * i)
#define MLC_SF_CROP_END_SHIFT        16
#define MLC_SF_CROP_END_MASK         0xFFFF << MLC_SF_CROP_END_SHIFT
#define MLC_SF_CROP_START_SHIFT      0
#define MLC_SF_CROP_START_MASK       0xFFFF << MLC_SF_CROP_START_SHIFT

#define MLC_SF_G_G_ALPHA_(i)           (REG(0x7018) + MLC_LAYER_JMP * i)
#define MLC_SF_G_ALPHA_A_SHIFT       0
#define MLC_SF_G_ALPHA_A_MASK        0xFF << MLC_SF_G_ALPHA_A_SHIFT

#define MLC_SF_CKEY_CKEY_ALPHA_(i)        (REG(0x701c) + MLC_LAYER_JMP * i)
#define MLC_SF_CKEY_ALPHA_A_SHIFT    0
#define MLC_SF_CKEY_ALPHA_A_MASK     0xFF << MLC_SF_CKEY_ALPHA_A_SHIFT

#define MLC_SF_CKEY_R_LV_(i)         (REG(0x7020) + MLC_LAYER_JMP * i)
#define MLC_SF_CKEY_G_LV_(i)         (REG(0x7024) + MLC_LAYER_JMP * i)
#define MLC_SF_CKEY_B_LV_(i)         (REG(0x7028) + MLC_LAYER_JMP * i)
#define MLC_SF_CKEY_LV_UP_SHIFT      16
#define MLC_SF_CKEY_LV_UP_MASK       0x3FF << MLC_SF_CKEY_LV_UP_SHIFT
#define MLC_SF_CKEY_LV_DN_SHIFT      0
#define MLC_SF_CKEY_LV_DN_MASK       0x3FF << MLC_SF_CKEY_LV_DN_SHIFT

#define MLC_SF_AFLU_AFLU_TIME_(i)         (REG(0x702c) + MLC_LAYER_JMP * i)
#define MLC_SF_AFLU_TIMER_SHIFT      0
#define MLC_SF_AFLU_TIMER_MASK       0xFFFFFFFF << MLC_SF_AFLU_TIMER_SHIFT

#define MLC_PATH_CTRL_(i)            (REG(0x7200) + MLC_PATH_JMP * i)
#define PMA_EN_SHIFT                 29
#define PMA_EN_MASK                  0x1 << PMA_EN_SHIFT
#define PD_OUT_SEL_SHIFT             28
#define PD_OUT_SEL_MASK              0x1 << PD_OUT_SEL_SHIFT
#define PD_MODE_SHIFT                20
#define PD_MODE_MASK                 0x1F << PD_MODE_SHIFT
#define ALPHA_BLD_IDX_SHIFT          16
#define ALPHA_BLD_IDX_MASK           0xF << ALPHA_BLD_IDX_SHIFT
#define PD_OUT_IDX_SHIFT             12
#define PD_OUT_IDX_MASK              0x7 << PD_OUT_IDX_SHIFT
#define PD_DES_IDX_SHIFT             8
#define PD_DES_IDX_MASK              0x7 << PD_DES_IDX_SHIFT
#define PD_SRC_IDX_SHIFT             4
#define PD_SRC_IDX_MASK              0x7 << PD_SRC_IDX_SHIFT
#define LAYER_OUT_IDX_SHIFT          0
#define LAYER_OUT_IDX_MASK           0xF << LAYER_OUT_IDX_SHIFT

#define MLC_BG_CTRL                  REG(0x7220)
#define BG_A_SHIFT                   8
#define BG_A_MASK                    0xFF << BG_A_SHIFT
#define AFLU_EN_SHIFT                7
#define AFLU_EN_MASK                 0x1 << AFLU_EN_SHIFT
#define FSTART_SEL_SHIFT             4
#define FSTART_SEL_MASK              0x7 << FSTART_SEL_SHIFT
#define BG_A_SEL_SHIFT               2
#define BG_A_SEL_MASK                0x1 << BG_A_SEL_SHIFT
#define BG_EN_SHIFT                  1
#define BG_EN_MASK                   0x1 << BG_EN_SHIFT
#define ALPHA_BLD_BYPS_SHIFT         0
#define ALPHA_BLD_BYPS_MASK          0x1 << ALPHA_BLD_BYPS_SHIFT

#define MLC_BG_COLOR                 REG(0x7224)
#define BG_COLOR_R_SHIFT             20
#define BG_COLOR_R_MASK              0x3FF << BG_COLOR_R_SHIFT
#define BG_COLOR_G_SHIFT             10
#define BG_COLOR_G_MASK              0x3FF << BG_COLOR_G_SHIFT
#define BG_COLOR_B_SHIFT             0
#define BG_COLOR_B_MASK              0x3FF << BG_COLOR_B_SHIFT

#define MLC_BG_AFLU_AFLU_TIME             REG(0x7228)
#define MLC_BG_AFLU_TIMER_SHIFT      0
#define MLC_BG_AFLU_TIMER_MASK       0xFFFFFFFF << MLC_BG_AFLU_TIMER_SHIFT

#define MLC_CANVAS_COLOR             REG(0x7230)
#define CANVAS_COLOR_R_SHIFT         20
#define CANVAS_COLOR_R_MASK          0x3FF << CANVAS_COLOR_R_SHIFT
#define CANVAS_COLOR_G_SHIFT         10
#define CANVAS_COLOR_G_MASK          0x3FF << CANVAS_COLOR_G_SHIFT
#define CANVAS_COLOR_B_SHIFT         0
#define CANVAS_COLOR_B_MASK          0x3FF << CANVAS_COLOR_B_SHIFT

#define MLC_CLK_CLK_RATIO                REG(0x7234)
#define MLC_CLK_RATIO_SHIFT          0
#define MLC_CLK_RATIO_MASK           0xFFFF << MLC_CLK_RATIO_SHIFT

#define MLC_INT_MASK                 REG(0x7240)
#define MLC_MASK_ERR_L_5_SHIFT       12
#define MLC_MASK_ERR_L_5_MASK        0x1 << MLC_MASK_ERR_L_5_SHIFT
#define MLC_MASK_ERR_L_4_SHIFT       11
#define MLC_MASK_ERR_L_4_MASK        0x1 << MLC_MASK_ERR_L_4_SHIFT
#define MLC_MASK_ERR_L_3_SHIFT       10
#define MLC_MASK_ERR_L_3_MASK        0x1 << MLC_MASK_ERR_L_3_SHIFT
#define MLC_MASK_ERR_L_2_SHIFT       9
#define MLC_MASK_ERR_L_2_MASK        0x1 << MLC_MASK_ERR_L_2_SHIFT
#define MLC_MASK_ERR_L_1_SHIFT       8
#define MLC_MASK_ERR_L_1_MASK        0x1 << MLC_MASK_ERR_L_1_SHIFT
#define MLC_MASK_ERR_L_0_SHIFT       7
#define MLC_MASK_ERR_L_0_MASK        0x1 << MLC_MASK_ERR_L_0_SHIFT
#define MLC_MASK_FLU_L_5_SHIFT       6
#define MLC_MASK_FLU_L_5_MASK        0x1 << MLC_MASK_FLU_L_5_SHIFT
#define MLC_MASK_FLU_L_4_SHIFT       5
#define MLC_MASK_FLU_L_4_MASK        0x1 << MLC_MASK_FLU_L_4_SHIFT
#define MLC_MASK_FLU_L_3_SHIFT       4
#define MLC_MASK_FLU_L_3_MASK        0x1 << MLC_MASK_FLU_L_3_SHIFT
#define MLC_MASK_FLU_L_2_SHIFT       3
#define MLC_MASK_FLU_L_2_MASK        0x1 << MLC_MASK_FLU_L_2_SHIFT
#define MLC_MASK_FLU_L_1_SHIFT       2
#define MLC_MASK_FLU_L_1_MASK        0x1 << MLC_MASK_FLU_L_1_SHIFT
#define MLC_MASK_FLU_L_0_SHIFT       1
#define MLC_MASK_FLU_L_0_MASK        0x1 << MLC_MASK_FLU_L_0_SHIFT
#define MLC_MASK_FRM_END_SHIFT       0
#define MLC_MASK_FRM_END_MASK        0x1 << MLC_MASK_FRM_END_SHIFT

#define MLC_INT_STATUS               REG(0x7244)
#define MLC_S_SLOWD_L_5_SHIFT        27
#define MLC_S_SLOWD_L_5_MASK         0x1 << MLC_S_SLOWD_L_5_SHIFT
#define MLC_S_SLOWD_L_4_SHIFT        26
#define MLC_S_SLOWD_L_4_MASK         0x1 << MLC_S_SLOWD_L_4_SHIFT
#define MLC_S_SLOWD_L_3_SHIFT        25
#define MLC_S_SLOWD_L_3_MASK         0x1 << MLC_S_SLOWD_L_3_SHIFT
#define MLC_S_SLOWD_L_2_SHIFT        24
#define MLC_S_SLOWD_L_2_MASK         0x1 << MLC_S_SLOWD_L_2_SHIFT
#define MLC_S_SLOWD_L_1_SHIFT        23
#define MLC_S_SLOWD_L_1_MASK         0x1 << MLC_S_SLOWD_L_1_SHIFT
#define MLC_S_SLOWD_L_0_SHIFT        22
#define MLC_S_SLOWD_L_0_MASK         0x1 << MLC_S_SLOWD_L_0_SHIFT
#define MLC_S_CROP_E_L_5_SHIFT       21
#define MLC_S_CROP_E_L_5_MASK        0x1 << MLC_S_CROP_E_L_5_SHIFT
#define MLC_S_CROP_E_L_4_SHIFT       20
#define MLC_S_CROP_E_L_4_MASK        0x1 << MLC_S_CROP_E_L_4_SHIFT
#define MLC_S_CROP_E_L_3_SHIFT       19
#define MLC_S_CROP_E_L_3_MASK        0x1 << MLC_S_CROP_E_L_3_SHIFT
#define MLC_S_CROP_E_L_2_SHIFT       18
#define MLC_S_CROP_E_L_2_MASK        0x1 << MLC_S_CROP_E_L_2_SHIFT
#define MLC_S_CROP_E_L_1_SHIFT       17
#define MLC_S_CROP_E_L_1_MASK        0x1 << MLC_S_CROP_E_L_1_SHIFT
#define MLC_S_CROP_E_L_0_SHIFT       16
#define MLC_S_CROP_E_L_0_MASK        0x1 << MLC_S_CROP_E_L_0_SHIFT
#define MLC_S_E_L_5_SHIFT            12
#define MLC_S_E_L_5_MASK             0x1 << MLC_S_E_L_5_SHIFT
#define MLC_S_E_L_4_SHIFT            11
#define MLC_S_E_L_4_MASK             0x1 << MLC_S_E_L_4_SHIFT
#define MLC_S_E_L_3_SHIFT            10
#define MLC_S_E_L_3_MASK             0x1 << MLC_S_E_L_3_SHIFT
#define MLC_S_E_L_2_SHIFT            9
#define MLC_S_E_L_2_MASK             0x1 << MLC_S_E_L_2_SHIFT
#define MLC_S_E_L_1_SHIFT            8
#define MLC_S_E_L_1_MASK             0x1 << MLC_S_E_L_1_SHIFT
#define MLC_S_E_L_0_SHIFT            7
#define MLC_S_E_L_0_MASK             0x1 << MLC_S_E_L_0_SHIFT
#define MLC_S_FLU_L_5_SHIFT          6
#define MLC_S_FLU_L_5_MASK           0x1 << MLC_S_FLU_L_5_SHIFT
#define MLC_S_FLU_L_4_SHIFT          5
#define MLC_S_FLU_L_4_MASK           0x1 << MLC_S_FLU_L_4_SHIFT
#define MLC_S_FLU_L_3_SHIFT          4
#define MLC_S_FLU_L_3_MASK           0x1 << MLC_S_FLU_L_3_SHIFT
#define MLC_S_FLU_L_2_SHIFT          3
#define MLC_S_FLU_L_2_MASK           0x1 << MLC_S_FLU_L_2_SHIFT
#define MLC_S_FLU_L_1_SHIFT          2
#define MLC_S_FLU_L_1_MASK           0x1 << MLC_S_FLU_L_1_SHIFT
#define MLC_S_FLU_L_0_SHIFT          1
#define MLC_S_FLU_L_0_MASK           0x1 << MLC_S_FLU_L_0_SHIFT
#define MLC_S_FRM_END_SHIFT          0
#define MLC_S_FRM_END_MASK           0x1 << MLC_S_FRM_END_SHIFT

/*AP*/
#define AP_PIX_PIX_COMP                  REG(0x9000)
#define AP_PIX_COMP_BPA_SHIFT        0
#define AP_PIX_COMP_BPA_MASK         0xF << AP_PIX_COMP_BPA_SHIFT

#define AP_FRM_FRM_CTRL                      REG(0x9004)
#define AP_FRM_CTRL_ENDIAN_CTRL_SHIFT    16
#define AP_FRM_CTRL_ENDIAN_CTRL_MASK     0x7 << AP_FRM_CTRL_ENDIAN_CTRL_SHIFT
#define AP_FRM_CTRL_ROT_SHIFT            8
#define AP_FRM_CTRL_ROT_MASK             0x7 << AP_FRM_CTRL_ROT_SHIFT
#define AP_FRM_CTRL_FAST_CP_MODE_SHIFT   0
#define AP_FRM_CTRL_FAST_CP_MODE_MASK    0x1 << AP_FRM_CTRL_FAST_CP_MODE_SHIFT

#define AP_FRM_FRM_SIZE                  REG(0x9008)
#define AP_FRM_SIZE_HEIGHT_SHIFT     16
#define AP_FRM_SIZE_HEIGHT_MASK      0xFFFF << AP_FRM_SIZE_HEIGHT_SHIFT
#define AP_FRM_SIZE_WIDTH_SHIFT      0
#define AP_FRM_SIZE_WIDTH_MASK       0xFFFF << AP_FRM_SIZE_WIDTH_SHIFT

#define AP_BADDR_L_L                   REG(0x900c)
#define AP_BADDR_L_SHIFT             0
#define AP_BADDR_L_MASK              0xFFFFFFFF << AP_BADDR_L_SHIFT

#define AP_BADDR_H_H                   REG(0x9010)
#define AP_BADDR_H_SHIFT             0
#define AP_BADDR_H_MASK              0xFFFFFFFF << AP_BADDR_H_SHIFT

#define AP_AP_STRIDE                    REG(0x902c)
#define AP_STRIDE_SHIFT              0
#define AP_STRIDE_MASK               0x3FFFF << AP_STRIDE_SHIFT

#define AP_FRM_FRM_OFFSET                REG(0x9040)
#define AP_FRM_OFFSET_Y_SHIFT        16
#define AP_FRM_OFFSET_Y_MASK         0xFFFF << AP_FRM_OFFSET_Y_SHIFT
#define AP_FRM_OFFSET_X_SHIFT        0
#define AP_FRM_OFFSET_X_MASK         0xFFFF << AP_FRM_OFFSET_X_SHIFT

/* AP_SDW_CTRL */
#define AP_SDW_CTRL_CTRL                  REG(0x9f00)
#define AP_SDW_CTRL_TRIG_SHIFT       0
#define AP_SDW_CTRL_TRIG_MASK        0x1 << AP_SDW_CTRL_TRIG_SHIFT

/*wdma*/
#define WCHN_JUMP   0x20
#define WCHN_COUNT  3

#define WDMA_DFIFO_WML_WML_(i)           (REG(0xa000) + WCHN_JUMP * i)
#define WDMA_DFIFO_WML_SHIFT         0
#define WDMA_DFIFO_WML_MASK          0xFFFF << WDMA_DFIFO_WML_SHIFT

#define WDMA_DFIFO_DEPTH_DEPTH_(i)         (REG(0xa004) + WCHN_JUMP * i)
#define WDMA_DFIFO_DEPTH_SHIFT       0
#define WDMA_DFIFO_DEPTH_MASK        0xFFFF << WDMA_DFIFO_DEPTH_SHIFT

#define WDMA_CFIFO_DEPTH_DEPTH_(i)         (REG(0xa008) + WCHN_JUMP * i)
#define WDMA_CFIFO_DEPTH_SHIFT       0
#define WDMA_CFIFO_DEPTH_MASK        0xFFFF << WDMA_CFIFO_DEPTH_SHIFT

#define WDMA_CH_PRIO_PRIO_(i)             (REG(0xa00c) + WCHN_JUMP * i)
#define WDMA_CH_PRIO_SCHE_SHIFT      16
#define WDMA_CH_PRIO_SCHE_MASK       0x3F << WDMA_CH_PRIO_SCHE_SHIFT
#define WDMA_CH_PRIO_P1_SHIFT        8
#define WDMA_CH_PRIO_P1_MASK         0x3F << WDMA_CH_PRIO_P1_SHIFT
#define WDMA_CH_PRIO_P0_SHIFT        0
#define WDMA_CH_PRIO_P0_MASK         0x3F << WDMA_CH_PRIO_P0_SHIFT

#define WDMA_BURST_BURST_(i)               (REG(0xa010) + WCHN_JUMP * i)
#define WDMA_BURST_MODE_SHIFT        3
#define WDMA_BURST_MODE_MASK         1 << WDMA_BURST_MODE_SHIFT
#define WDMA_BURST_LEN_SHIFT         0
#define WDMA_BURST_LEN_MASK          0x7 << WDMA_BURST_LEN_SHIFT

#define WDMA_AXI_USER_USER_(i)            (REG(0xa014) + WCHN_JUMP * i)
#define WDMA_AXI_USER_SHIFT          0
#define WDMA_AXI_USER_MASK           0xFFFFF << WDMA_AXI_USER_SHIFT

#define WDMA_AXI_CTRL_CTRL_(i)                (REG(0xa018) + WCHN_JUMP * i)
#define WDMA_AXI_CTRL_CHN_RST_SHIFT      7
#define WDMA_AXI_CTRL_CHN_RST_MASK       1 << WDMA_AXI_CTRL_CHN_RST_SHIFT
#define WDMA_AXI_CTRL_BUFAB_CFG_SHFIT    6
#define WDMA_AXI_CTRL_BUFAB_CFG_MASK     1 << WDMA_AXI_CTRL_BUFAB_CFG_SHFIT
#define WDMA_AXI_CTRL_PROT_SHIFT         4
#define WDMA_AXI_CTRL_PROT_MASK          0x3 << WDMA_AXI_CTRL_PROT_SHIFT
#define WDMA_AXI_CTRL_CACHE_SHIFT        0
#define WDMA_AXI_CTRL_CACHE_MASK         0xF << WDMA_AXI_CTRL_CACHE_SHIFT

#define WDMA_CTRL_CTRL                    REG(0xa400)
#define WDMA_CTRL_CFG_LOAD_SHIFT     1
#define WDMA_CTRL_CFG_LOAD_MASK      1 << WDMA_CTRL_CFG_LOAD_SHIFT
#define WDMA_CTRL_ARB_SEL_SHIFT      0
#define WDMA_CTRL_ARB_SEL_MASK       1 << WDMA_CTRL_ARB_SEL_SHIFT

#define WDMA_DFIFO_FULL_FULL              REG(0xa500)
#define WDMA_DFIFO_FULL_CH_2_SHIFT   2
#define WDMA_DFIFO_FULL_CH_2_MASK    1 << WDMA_DFIFO_FULL_CH_2_SHIFT
#define WDMA_DFIFO_FULL_CH_1_SHIFT   1
#define WDMA_DFIFO_FULL_CH_1_MASK    1 << WDMA_DFIFO_FULL_CH_1_SHIFT
#define WDMA_DFIFO_FULL_CH_0_SHIFT   0
#define WDMA_DFIFO_FULL_CH_0_MASK    1 << WDMA_DFIFO_FULL_CH_0_SHIFT

#define WDMA_DFIFO_EMPTY_EMPTY             REG(0xa504)
#define WDMA_DFIFO_EMPTY_CH_2_SHIFT  2
#define WDMA_DFIFO_EMPTY_CH_2_MASK   1 << WDMA_DFIFO_EMPTY_CH_2_SHIFT
#define WDMA_DFIFO_EMPTY_CH_1_SHIFT  1
#define WDMA_DFIFO_EMPTY_CH_1_MASK   1 << WDMA_DFIFO_EMPTY_CH_1_SHIFT
#define WDMA_DFIFO_EMPTY_CH_0_SHIFT  0
#define WDMA_DFIFO_EMPTY_CH_0_MASK   1 << WDMA_DFIFO_EMPTY_CH_0_SHIFT

#define WDMA_CFIFO_FULL_FULL              REG(0xa508)
#define WDMA_CFIFO_FULL_CH_2_SHFIT   2
#define WDMA_CFIFO_FULL_CH_2_MASK    1 << WDMA_CFIFO_FULL_CH_2_SHFIT
#define WDMA_CFIFO_FULL_CH_1_SHFIT   1
#define WDMA_CFIFO_FULL_CH_1_MASK    1 << WDMA_CFIFO_FULL_CH_1_SHFIT
#define WDMA_CFIFO_FULL_CH_0_SHFIT   0
#define WDMA_CFIFO_FULL_CH_0_MASK    1 << WDMA_CFIFO_FULL_CH_0_SHFIT

#define WDMA_CFIFO_EMPTY_EMPTY             REG(0xa50c)
#define WDMA_CFIFO_EMPTY_CH_2_SHIFT  2
#define WDMA_CFIFO_EMPTY_CH_2_MASK   1 << WDMA_CFIFO_EMPTY_CH_2_SHIFT
#define WDMA_CFIFO_EMPTY_CH_1_SHIFT  1
#define WDMA_CFIFO_EMPTY_CH_1_MASK   1 << WDMA_CFIFO_EMPTY_CH_1_SHIFT
#define WDMA_CFIFO_EMPTY_CH_0_SHIFT  0
#define WDMA_CFIFO_EMPTY_CH_0_MASK   1 << WDMA_CFIFO_EMPTY_CH_0_SHIFT

#define WDMA_CH_IDLE_IDLE                 REG(0xa510)
#define WDMA_CH_IDLE_CH_2_SHIFT      2
#define WDMA_CH_IDLE_CH_2_MASK       1 << WDMA_CH_IDLE_CH_2_SHIFT
#define WDMA_CH_IDLE_CH_1_SHIFT      1
#define WDMA_CH_IDLE_CH_1_MASK       1 << WDMA_CH_IDLE_CH_1_SHIFT
#define WDMA_CH_IDLE_CH_0_SHIFT      0
#define WDMA_CH_IDLE_CH_0_MASK       1 << WDMA_CH_IDLE_CH_0_SHIFT

#define WDMA_INT_MASK_MASK                    REG(0xa520)
#define WDMA_INT_MASK_ERR_CH_2_SHIFT     2
#define WDMA_INT_MASK_ERR_CH_2_MASK      1 << WDMA_INT_MASK_ERR_CH_2_SHIFT
#define WDMA_INT_MASK_ERR_CH_1_SHIFT     1
#define WDMA_INT_MASK_ERR_CH_1_MASK      1 << WDMA_INT_MASK_ERR_CH_1_SHIFT
#define WDMA_INT_MASK_ERR_CH_0_SHIFT     0
#define WDMA_INT_MASK_ERR_CH_0_MASK      1 << WDMA_INT_MASK_ERR_CH_0_SHIFT

#define WDMA_INT_STATUS_STATUS                  REG(0xa524)
#define WDMA_INT_STATUS_ERR_CH_2_SHIFT   2
#define WDMA_INT_STATUS_ERR_CH_2_MASK    1 << WDMA_INT_STATUS_ERR_CH_2_SHIFT
#define WDMA_INT_STATUS_ERR_CH_1_SHIFT   1
#define WDMA_INT_STATUS_ERR_CH_1_MASK    1 << WDMA_INT_STATUS_ERR_CH_1_SHIFT
#define WDMA_INT_STATUS_ERR_CH_0_SHIFT   0
#define WDMA_INT_STATUS_ERR_CH_0_MASK    1 << WDMA_INT_STATUS_ERR_CH_0_SHIFT

#define WDMA_DEBUG_CTRL_CTRL                  REG(0xa540)
#define WDMA_DEBUG_CTRL_DEBUG_SEL_SHIFT  0
#define WDMA_DEBUG_CTRL_DEBUG_SEL_MASK   0x1F << WDMA_DEBUG_CTRL_DEBUG_SEL_SHIFT

#define WDMA_DEBUG_STA_STA                   REG(0xa544)
#define WDMA_DEBUG_STA_CFIFO_DEP_SHIFT   16
#define WDMA_DEBUG_STA_CFIFO_DEP_MASK    0xFFFF << WDMA_DEBUG_STA_CFIFO_DEP_SHIFT
#define WDMA_DEBUG_STA_DFIFO_DEP_SHIFT   0
#define WDMA_DEBUG_STA_DFIFO_DEP_MASK    0xFFFF << WDMA_DEBUG_STA_DFIFO_DEP_SHIFT

/*W-PIPE*/
#define WP_CTRL_CTRL                          REG(0xb000)
#define WP_CTRL_PACK_ENDIAN_CTRL_SHIFT   16
#define WP_CTRL_PACK_ENDIAN_CTRL_MASK    0x7 << WP_CTRL_PACK_ENDIAN_CTRL_SHIFT
#define WP_CTRL_PACK_ROUND_SHIFT         12
#define WP_CTRL_PACK_ROUND_MASK          1 << WP_CTRL_PACK_ROUND_SHIFT
#define WP_CTRL_TILE_TYPE_SHIFT          10
#define WP_CTRL_TILE_TYPE_MASK           0x3 << WP_CTRL_TILE_TYPE_SHIFT
#define WP_CTRL_PXL_MODE_SHIFT           9
#define WP_CTRL_PXL_MODE_MASK            1 << WP_CTRL_PXL_MODE_SHIFT
#define WP_CTRL_REFORGE_BYPS_SHIFT       8
#define WP_CTRL_REFORGE_BYPS_MASK        1 << WP_CTRL_REFORGE_BYPS_SHIFT
#define WP_CTRL_UV_MODE_SHIFT            6
#define WP_CTRL_UV_MODE_MASK             0x3 << WP_CTRL_UV_MODE_SHIFT
#define WP_CTRL_STR_FMT_SHIFT            4
#define WP_CTRL_STR_FMT_MASK             0x3 << WP_CTRL_STR_FMT_SHIFT
#define WP_CTRL_VFLIP_SHIFT              3
#define WP_CTRL_VFLIP_MASK               1 << WP_CTRL_VFLIP_SHIFT
#define WP_CTRL_HFLIP_SHIFT              2
#define WP_CTRL_HFLIP_MASK               1 << WP_CTRL_HFLIP_SHIFT
#define WP_CTRL_ROT_EN_SHIFT             1
#define WP_CTRL_ROT_EN_MASK              1 << WP_CTRL_ROT_EN_SHIFT
#define WP_CTRL_FRM_MODE_SHIFT           0
#define WP_CTRL_FRM_MODE_MASK            1 << WP_CTRL_FRM_MODE_SHIFT

#define WP_PIX_COMP_COMP                      REG(0xb004)
#define WP_PIX_COMP_BPV_SHIFT            24
#define WP_PIX_COMP_BPV_MASK             0xF << WP_PIX_COMP_BPV_SHIFT
#define WP_PIX_COMP_BPU_SHIFT            16
#define WP_PIX_COMP_BPU_MASK             0xF << WP_PIX_COMP_BPU_SHIFT
#define WP_PIX_COMP_BPY_SHIFT            8
#define WP_PIX_COMP_BPY_MASK             0x1F << WP_PIX_COMP_BPY_SHIFT
#define WP_PIX_COMP_BPA_SHIFT            0
#define WP_PIX_COMP_BPA_MASK             0xF << WP_PIX_COMP_BPA_SHIFT

#define WP_YUVDOWN_CTRL_CTRL                       REG(0xb008)
#define WP_YUVDOWN_CTRL_V_BYPASS_SHIFT        9
#define WP_YUVDOWN_CTRL_V_BYPASS_MASK         1 << WP_YUVDOWN_CTRL_V_BYPASS_SHIFT
#define WP_YUVDOWN_CTRL_BYPASS_SHIFT          8
#define WP_YUVDOWN_CTRL_BYPASS_MASK           1 << WP_YUVDOWN_CTRL_BYPASS_SHIFT
#define WP_YUVDOWN_CTRL_V_INI_OFST_SHIFT      6
#define WP_YUVDOWN_CTRL_V_INI_OFST_MASK       0x3 << WP_YUVDOWN_CTRL_V_INI_OFST_SHIFT
#define WP_YUVDOWN_CTRL_V_FILTER_TYPE_SHIFT   4
#define WP_YUVDOWN_CTRL_V_FILTER_TYPE_MASK    0x3 << WP_YUVDOWN_CTRL_V_FILTER_TYPE_SHIFT
#define WP_YUVDOWN_CTRL_H_INI_OFST_SHIFT      2
#define WP_YUVDOWN_CTRL_H_INI_OFST_MASK       0x3 << WP_YUVDOWN_CTRL_H_INI_OFST_SHIFT
#define WP_YUVDOWN_CTRL_H_FILTER_TYPE_SHIFT   0
#define WP_YUVDOWN_CTRL_H_FILTER_TYPE_MASK    0x3 << WP_YUVDOWN_CTRL_H_FILTER_TYPE_SHIFT

#define WP_Y_BADDR_L_L                 REG(0xb010)
#define WP_Y_BADDR_L_Y_SHIFT         0
#define WP_Y_BADDR_L_Y_MASK          0xFFFFFFFF << WP_Y_BADDR_L_Y_SHIFT

#define WP_Y_BADDR_H_H                 REG(0xb014)
#define WP_Y_BADDR_H_Y_SHIFT         0
#define WP_Y_BADDR_H_Y_MASK          0xFF << WP_Y_BADDR_H_Y_SHIFT

#define WP_U_BADDR_L_L                 REG(0xb018)
#define WP_U_BADDR_L_U_SHIFT         0
#define WP_U_BADDR_L_U_MASK          0xFFFFFFFF << WP_U_BADDR_L_U_SHIFT

#define WP_U_BADDR_H_H                 REG(0xb01c)
#define WP_U_BADDR_H_U_SHIFT         0
#define WP_U_BADDR_H_U_MASK          0xFF << WP_U_BADDR_H_U_SHIFT

#define WP_V_BADDR_L_L                 REG(0xb020)
#define WP_V_BADDR_L_V_SHIFT         0
#define WP_V_BADDR_L_V_MASK          0xFFFFFFFF << WP_V_BADDR_L_V_SHIFT

#define WP_V_BADDR_H_H                 REG(0xb024)
#define WP_V_BADDR_H_V_SHIFT         0
#define WP_V_BADDR_H_V_MASK          0xFF << WP_V_BADDR_H_V_SHIFT

#define WP_Y_STRIDE_STRIDE                  REG(0xb028)
#define WP_Y_STRIDE_Y_SHIFT          0
#define WP_Y_STRIDE_Y_MASK           0x3FFFF << WP_Y_STRIDE_Y_SHIFT

#define WP_U_STRIDE_STRIDE                  REG(0xb02c)
#define WP_U_STRIDE_U_SHIFT          0
#define WP_U_STRIDE_U_MASK           0x3FFFF << WP_U_STRIDE_U_SHIFT

#define WP_V_STRIDE_STRIDE                  REG(0xb030)
#define WP_V_STRIDE_V_SHIFT          0
#define WP_V_STRIDE_V_MASK           0x3FFFF << WP_V_STRIDE_V_SHIFT

#define WP_CSC_CTRL_CTRL                  REG(0xb040)
#define WP_CSC_CTRL_ALPHA_SHIFT      2
#define WP_CSC_CTRL_ALPHA_MASK       1 << WP_CSC_CTRL_ALPHA_SHIFT
#define WP_CSC_CTRL_SBUP_CONV_SHIFT  1
#define WP_CSC_CTRL_SBUP_CONV_MASK   1 << WP_CSC_CTRL_SBUP_CONV_SHIFT
#define WP_CSC_CTRL_BYPASS_SHIFT     0
#define WP_CSC_CTRL_BYPASS_MASK      1 << WP_CSC_CTRL_BYPASS_SHIFT

#define WP_CSC_COEF_COEF1                 REG(0xb044)
#define WP_CSC_COEF1_A01_SHIFT       16
#define WP_CSC_COEF1_A01_MASK        0x3FFF << WP_CSC_COEF1_A01_SHIFT
#define WP_CSC_COEF1_A00_SHIFT       0
#define WP_CSC_COEF1_A00_MASK        0x3FFF << WP_CSC_COEF1_A00_SHIFT

#define WP_CSC_COEF_COEF2                 REG(0xb048)
#define WP_CSC_COEF2_A10_SHIFT       16
#define WP_CSC_COEF2_A10_MASK        0x3FFF << WP_CSC_COEF2_A10_SHIFT
#define WP_CSC_COEF2_A02_SHIFT       0
#define WP_CSC_COEF2_A02_MASK        0x3FFF << WP_CSC_COEF2_A02_SHIFT

#define WP_CSC_COEF_COEF3                 REG(0xb04c)
#define WP_CSC_COEF3_A12_SHIFT       16
#define WP_CSC_COEF3_A12_MASK        0x3FFF << WP_CSC_COEF3_A12_SHIFT
#define WP_CSC_COEF3_A11_SHIFT       0
#define WP_CSC_COEF3_A11_MASK        0x3FFF << WP_CSC_COEF3_A11_SHIFT

#define WP_CSC_COEF_COEF4                 REG(0xb050)
#define WP_CSC_COEF4_A21_SHIFT       16
#define WP_CSC_COEF4_A21_MASK        0x3FFF << WP_CSC_COEF4_A21_SHIFT
#define WP_CSC_COEF4_A20_SHIFT       0
#define WP_CSC_COEF4_A20_MASK        0x3FFF << WP_CSC_COEF4_A20_SHIFT

#define WP_CSC_COEF_COEF5                 REG(0xb054)
#define WP_CSC_COEF5_B0_SHIFT        16
#define WP_CSC_COEF5_B0_MASK         0x3FFF << WP_CSC_COEF5_B0_SHIFT
#define WP_CSC_COEF5_A22_SHIFT       0
#define WP_CSC_COEF5_A22_MASK        0x3FFF << WP_CSC_COEF5_A22_SHIFT

#define WP_CSC_COEF_COEF6                 REG(0xb058)
#define WP_CSC_COEF6_B2_SHIFT        16
#define WP_CSC_COEF6_B2_MASK         0x3FFF << WP_CSC_COEF6_B2_SHIFT
#define WP_CSC_COEF6_B1_SHIFT        0
#define WP_CSC_COEF6_B1_MASK         0x3FFF << WP_CSC_COEF6_B1_SHIFT

#define WP_CSC_COEF_COEF7                 REG(0xb05c)
#define WP_CSC_COEF7_C1_SHIFT        16
#define WP_CSC_COEF7_C1_MASK         0x3FF << WP_CSC_COEF7_C1_SHIFT
#define WP_CSC_COEF7_C0_SHIFT        0
#define WP_CSC_COEF7_C0_MASK         0x3FF << WP_CSC_COEF7_C0_SHIFT

#define WP_CSC_COEF_COEF8                 REG(0xb060)
#define WP_CSC_COEF8_C2_SHIFT        0
#define WP_CSC_COEF8_C2_MASK         0x3FF << WP_CSC_COEF8_C2_SHIFT
