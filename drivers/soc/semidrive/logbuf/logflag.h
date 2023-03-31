#ifndef __LOG_FLAG_H__
#define __LOG_FLAG_H__
#include <linux/regmap.h>
#include <dt-bindings/soc/sdrv-common-reg.h>

/*
 * BIT : 16         15       14       13      12     9          8     6     4     2     0
 * FUNC: | dumpflag | AP2_EXP | AP_EXP | G_EXP | REV | SAF_UART | AP1 | MPC | SEC | SAF |
 */
#define EXP_LOG_RST_CNT_MASK        (0x3)
#define EXP_LOG_RST_CNT_BITS        (0x2)

#define EXP_SAF_UART_FLAG_SHIFT     (8)
#define EXP_SAF_UART_FLAG_MASK      (0x1)

#define EXP_LOG_GLOBAL_FLAG_SHIFT   (12)
#define EXP_LOG_GLOBAL_FLAG_MASK    (0x1)

/* transfer the ap exception from safety to preloader */
#define EXP_LOG_AP_EXP_FLAG_SHIFT   (13)
#define EXP_LOG_AP_EXP_FLAG_MASK    (0x1)

typedef enum {
	FLAG_ID_SAF = 0,
	FLAG_ID_SEC,
	FLAG_ID_MPC,
	FLAG_ID_AP1,
	FLAG_ID_MAX
} flag_id_t;

int sdrv_global_exp_flag_get(struct regmap *regmap);
int sdrv_exp_rst_cnt_get(struct regmap *regmap, flag_id_t id);
void sdrv_exp_rst_cnt_set(struct regmap *regmap, flag_id_t id, int val);
void sdrv_saf_uart_flag_set(struct regmap *regmap, int val);
int sdrv_saf_uart_flag_get(struct regmap *regmap);

#endif
