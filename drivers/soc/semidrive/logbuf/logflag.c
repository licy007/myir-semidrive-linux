#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitfield.h>
#include "logflag.h"

int sdrv_exp_rst_cnt_get(struct regmap *regmap, flag_id_t id)
{
	int val;

	if (id >= FLAG_ID_MAX)
		return 0;

	regmap_read(regmap, SDRV_REG_EXCEPTION, &val);

	return ((val >> (id * EXP_LOG_RST_CNT_BITS)) & EXP_LOG_RST_CNT_MASK);
}

void sdrv_exp_rst_cnt_set(struct regmap *regmap, flag_id_t id, int val)
{
	if (id >= FLAG_ID_MAX)
		return;

	regmap_update_bits(regmap, SDRV_REG_EXCEPTION,
			   (EXP_LOG_RST_CNT_MASK << (id * EXP_LOG_RST_CNT_BITS)),
			   (val << (id * EXP_LOG_RST_CNT_BITS)));
}

int sdrv_global_exp_flag_get(struct regmap *regmap)
{
	int val;

	regmap_read(regmap, SDRV_REG_EXCEPTION, &val);

	return ((val >> EXP_LOG_GLOBAL_FLAG_SHIFT) & EXP_LOG_GLOBAL_FLAG_MASK);
}

void sdrv_saf_uart_flag_set(struct regmap *regmap, int val)
{
	regmap_update_bits(regmap, SDRV_REG_EXCEPTION,
			   EXP_SAF_UART_FLAG_MASK << EXP_SAF_UART_FLAG_SHIFT,
			   val << EXP_SAF_UART_FLAG_SHIFT);
}

int sdrv_saf_uart_flag_get(struct regmap *regmap)
{
	int val;

	regmap_read(regmap, SDRV_REG_EXCEPTION, &val);

	return ((val >> EXP_SAF_UART_FLAG_SHIFT) & EXP_SAF_UART_FLAG_MASK);
}
