#ifndef __PWM_SEMIDRIVE_H
#define __PWM_SEMIDRIVE_H
#define DRV_PWM_HF_CLOCK_FREQ (398000000ul)

#define DRV_PWM_SIMPLE_CLOCK_DIV_NUM  1

#define DRV_PWM_PCM_TUNE_TABLE_METHOD 0
#define DRV_PWM_PCM_TUNE_CALCU_METHOD 1
#define DRV_PWM_PCM_TUNE_GET_METHOD   DRV_PWM_PCM_TUNE_TABLE_METHOD

#define DRV_PWM_FIFO_WML   32
#define DRV_PWM_FIFO_DEPTH 64

typedef enum {
    DRV_PWM_FORCE_OUT_DISABLE = 0,
    DRV_PWM_FORCE_OUT_HIGH,
    DRV_PWM_FORCE_OUT_LOW,
} drv_pwm_force_out_t;

typedef enum {
    DRV_PWM_CMP_DATA_32BITS = 0,
    DRV_PWM_CMP_DATA_16BITS,
    DRV_PWM_CMP_DATA_8BITS,
} drv_pwm_data_format_t;

typedef enum {
    DRV_PWM_CHN_A = 0,
    DRV_PWM_CHN_B,
    DRV_PWM_CHN_C,
    DRV_PWM_CHN_D,
    DRV_PWM_CHN_TOTAL
} drv_pwm_chn_t;

typedef enum {
    DRV_PWM_SRC_CLK_HF = 0,
    DRV_PWM_SRC_CLK_AHF,
    DRV_PWM_SRC_CLK_RESERVE,
    DRV_PWM_SRC_CLK_EXT,
} drv_pwm_src_clk_t;


typedef enum {
	DRV_CMP_OUT_POSITIVE_PULSE = 0,
	DRV_CMP_OUT_NEGATIVE_PULSE,
	DRV_CMP_OUT_SIGNAL_TOGGLE,
	DRV_CMP_OUT_LEVEL_HIGH,
	DRV_CMP_OUT_LEVEL_LOW,
	DRV_CMP_OUT_KEEP,
} drv_pwm_cmp_out_mode_t;

typedef enum
{
    DRV_PWM_EDGE_ALIGN_MODE = 0,
    DRV_PWM_CENTER_ALIGN_MODE,
} drv_pwm_align_mode_t;

typedef enum
{
    DRV_PWM_PHASE_POLARITY_POS = 0,
    DRV_PWM_PHASE_POLARITY_NEG,
} drv_pwm_phase_polarity_t;

typedef enum
{
    DRV_PWM_PCM_DRIVE_MONO_CHANNEL = 0,
    DRV_PWM_PCM_DRIVE_DUAL_CHANNEL,
    DRV_PWM_PCM_DRIVE_DUAL_H_BRIDGE,
    DRV_PWM_PCM_DRIVE_FOUR_H_BRIDGE,
} drv_pwm_pcm_drive_mode_t;

typedef enum
{
    DRV_PWM_CHN_A_WORK = 0,
    DRV_PWM_CHN_A_B_WORK,
    DRV_PWM_CHN_A_B_C_D_WORK,
} drv_pwm_group_num_t;

typedef enum
{
    DRV_PWM_CONTINUE_CMP = 0,
    DRV_PWM_ONE_SHOT_CMP,
} drv_pwm_single_mode_t;

typedef struct
{
    uint8_t duty;                   //simple pwm duty, unit is %
    drv_pwm_phase_polarity_t phase; //simple pwm phase polarity
} drv_pwm_simple_cmp_cfg_t;

typedef struct
{
    uint64_t freq;                                       //simple pwm frequency
    drv_pwm_group_num_t grp_num;                         //group number
    drv_pwm_single_mode_t single_mode;                   //single one shot mode
    drv_pwm_align_mode_t align_mode;                     //pwm wave edge align or center align mode
    drv_pwm_simple_cmp_cfg_t cmp_cfg[DRV_PWM_CHN_TOTAL]; //sub-channel compare configure
} drv_pwm_simple_cfg_t;

typedef struct {
    drv_pwm_src_clk_t clk_src;  //pwm source clock select
    uint16_t clk_div;   //pwm source clock div number
} drv_pwm_clk_cfg_t;

typedef enum {
    DRV_PWM_INT_SRC_CMP_EVENT = 0,
    DRV_PWM_INT_SRC_CNT_G0_OVF,
    DRV_PWM_INT_SRC_FIFO_UNDERRUN,
} drv_pwm_int_src_t;

typedef union {
    struct {
        uint32_t src_clk_sel  : 2;
        uint32_t frc_rld      : 1;
        uint32_t ext_clr_en   : 1;
        uint32_t int_clr      : 1;
        uint32_t reserve0     : 11;
        uint32_t div_num      : 16;
    };
    uint32_t val;
} sdrv_pwm_cnt_g0_config_t;

typedef union {
    struct {
        uint32_t data_format    : 2;
        uint32_t grp_num        : 2;
        uint32_t dual_cmp_mode  : 1;
        uint32_t dma_en         : 1;
        uint32_t reserve0       : 2;
        uint32_t fifo_wml       : 6;
        uint32_t reserve1       : 2;
        uint32_t rpt_num        : 8;
    };
    uint32_t val;
} sdrv_pwm_cmp_config_t;

typedef union {
    struct {
        uint32_t cmp_en         : 1;
        uint32_t single_mode    : 1;
        uint32_t reserve0       : 29;
        uint32_t sw_rst         : 1;
    };
    uint32_t val;
} sdrv_pwm_cmp_ctrl_t;

typedef union {
    struct {
        uint32_t cmp0_out_mode  : 3;
        uint32_t cmp1_out_mode  : 3;
        uint32_t reserve0       : 10;
        uint32_t cmp0_pulse_wid : 8;
        uint32_t cmp1_pulse_wid : 8;
    };
    uint32_t val;
} sdrv_pwm_cmp_ch_config0_t;

typedef union {
    struct {
        uint32_t ovf_out_mode  : 3;
        uint32_t reserve0      : 1;
        uint32_t frc_high      : 1;
        uint32_t frc_low       : 1;
        uint32_t reserve1      : 10;
        uint32_t ovf_pulse_wid : 16;
    };
    uint32_t val;
} sdrv_pwm_cmp_ch_config1_t;

typedef union {
    struct {
        uint32_t dither_en      : 1;
        uint32_t init_offset_en : 1;
        uint32_t in_rslt        : 2;
        uint32_t drop           : 4;
        uint32_t clip_rslt      : 4;
        uint32_t reserve0       : 4;
        uint32_t init_offset    : 16;
    };
    uint32_t val;
} sdrv_pwm_dither_ctrl_t;

typedef union {
    struct {
        uint32_t sse_en_a   : 1;
        uint32_t sse_en_b   : 1;
        uint32_t sse_en_c   : 1;
        uint32_t sse_en_d   : 1;
        uint32_t sse_sel_a  : 6;
        uint32_t sse_sel_b  : 6;
        uint32_t sse_sel_c  : 6;
        uint32_t sse_sel_d  : 6;
        uint32_t reserve0   : 4;
    };
    uint32_t val;
} sdrv_pwm_sse_ctrl_t;

typedef union {
    struct {
        uint32_t full      : 1;
        uint32_t empty     : 1;
        uint32_t entries   : 7;
        uint32_t reserve0  : 23;
    };
    uint32_t val;
} sdrv_pwm_fifo_stat_t;

typedef struct {
    uint32_t ovf_val;
    drv_pwm_align_mode_t align_mode;
    drv_pwm_phase_polarity_t phase[DRV_PWM_CHN_TOTAL];
} drv_pwm_simple_context_t;

typedef struct {
    volatile u32 int_sta;       /* offset: 0x0 */
    volatile u32 int_sta_en;    /* offset: 0x4 */
    volatile u32 int_sig_en;    /* offset: 0x8 */
    volatile sdrv_pwm_cnt_g0_config_t cnt_g0_config; /* offset: 0xc */
    volatile u32 clk_config;    /* offset: 0x10 */
    volatile u32 cnt_g0_ovf;    /* offset: 0x14 */
    volatile u32 cnt_g0;        /* offset: 0x18 */
    volatile u32 cmp_val_upt;   /* offset: 0x1c */
    volatile u32 cmp0_a_val;    /* offset: 0x20 */
    volatile u32 cmp1_a_val;    /* offset: 0x24 */
    volatile u32 cmp0_b_val;    /* offset: 0x28 */
    volatile u32 cmp1_b_val;    /* offset: 0x2c */
    volatile u32 cmp0_c_val;    /* offset: 0x30 */
    volatile u32 cmp1_c_val;    /* offset: 0x34 */
    volatile u32 cmp0_d_val;    /* offset: 0x38 */
    volatile u32 cmp1_d_val;    /* offset: 0x3c */
    volatile sdrv_pwm_cmp_config_t cmp_config;    /* offset: 0x40 */
    volatile u32 fifo_entry;    /* offset: 0x44 */
    volatile sdrv_pwm_fifo_stat_t fifo_stat;     /* offset: 0x48 */
    volatile sdrv_pwm_cmp_ctrl_t cmp_ctrl;      /* offset: 0x4c */
    volatile sdrv_pwm_cmp_ch_config0_t cmp_a_config0; /* offset: 0x50 */
    volatile sdrv_pwm_cmp_ch_config1_t cmp_a_config1; /* offset: 0x54 */
    volatile sdrv_pwm_cmp_ch_config0_t cmp_b_config0; /* offset: 0x58 */
    volatile sdrv_pwm_cmp_ch_config1_t cmp_b_config1; /* offset: 0x5c */
    volatile sdrv_pwm_cmp_ch_config0_t cmp_c_config0; /* offset: 0x60 */
    volatile sdrv_pwm_cmp_ch_config1_t cmp_c_config1; /* offset: 0x64 */
    volatile sdrv_pwm_cmp_ch_config0_t cmp_d_config0; /* offset: 0x68 */
    volatile sdrv_pwm_cmp_ch_config1_t cmp_d_config1; /* offset: 0x6c */
    volatile sdrv_pwm_dither_ctrl_t dither_ctrl;   /* offset: 0x70 */
    volatile u32 mfc_ctrl;      /* offset: 0x74 */
    volatile u8 resvd1[8];      /* offset: 0x78 */
    volatile sdrv_pwm_sse_ctrl_t sse_ctrl;      /* offset: 0x80 */
    volatile u32 sse_a;         /* offset: 0x84 */
    volatile u32 sse_b;         /* offset: 0x88 */
    volatile u32 sse_c;         /* offset: 0x8c */
    volatile u32 sse_d;         /* offset: 0x90 */
} sdrv_pwm_t;

#endif
