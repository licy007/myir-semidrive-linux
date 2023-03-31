/*
 * sdrv_extra_gpr.c
 *
 * Copyright (c) 2022 SemiDrive Semiconductor.
 * All rights reserved.
 *
 * Description:
 *
 * Revision History:
 * -----------------
 */

#include <dt-bindings/soc/sdrv-common-reg.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <soc/semidrive/sdrv_extra_gpr.h>
#include <linux/slab.h>

typedef struct sdrv_extra_gpr {
	uint32_t key;
	uint32_t value;
} sdrv_extra_gpr_t;

typedef struct sdrv_extra_gpr_head {
	volatile uint32_t magic;
	volatile uint32_t count;
	volatile sdrv_extra_gpr_t gpr[0];
} sdrv_extra_gpr_head_t;

struct sdrv_extra_gpr_data {
	u32 reg_data;
	u32 addr_base;
	void __iomem *addr_base_convert;
};

static struct sdrv_extra_gpr_data *sdrv_gpr_data;

static u32 sdrv_extra_gpr_check(u32 reg_data)
{
	u32 magic;

	magic = reg_data & (u32)SDRV_EXTRA_GPR_ADDR_MASK;

	if (magic != SDRV_EXTRA_GPR_ADDR_MAGIC) {
		return 0;
	} else {
		reg_data &= ~(u32)SDRV_EXTRA_GPR_ADDR_MASK;
		return reg_data;
	}
}

static u32 sdrv_extra_gpr_key_translate(const char *gpr_name)
{
	int len = strlen(gpr_name);
	u32 key = 0;

	if (len > SDRV_EXTRA_GPR_NAME_SIZE || len == 0) {
		pr_err("sdrv extra gpr name length range must be 1-%d\n",
		       SDRV_EXTRA_GPR_NAME_SIZE);
		return 0;
	}

	while (len > 0) {
		len--;
		key |= gpr_name[len] << (8 * len);
	}

	return key;
}

static u32 sdrv_extra_gpr_reg_data_get(void)
{
	u32 reg_data;
	struct device_node *np;
	struct regmap *regmap;

	np = of_find_compatible_node(NULL, NULL, "semidrive,reg-ctl");

	if (!np) {
		pr_err("no regctl\n");
		return 0;
	}

	regmap = dev_get_regmap((struct device *)np->data, NULL);

	if (!regmap) {
		pr_err("no reg mapped\n");
		return 0;
	}

	if (regmap_read(regmap, SDRV_REG_HWID, &reg_data) != 0) {
		pr_err("no hwid reg configured in dts\n");
		return 0;
	}

	return reg_data;
}

u32 sdrv_extra_gpr_get(const char *name, u32 *value)
{
	sdrv_extra_gpr_head_t *sdrv_extra_gpr_head = NULL;
	u32 key;
	u32 ret;
	u32 i;
	void *addr_base_convert = sdrv_gpr_data->addr_base_convert;

	if (!name || !value) {
		pr_err("invalid sdrv extra gpr para\n");
		ret = SDRV_EXTRA_GPR_ERROR_INVALID_NAME;
		return ret;
	}

	key = sdrv_extra_gpr_key_translate(name);
	if (!key) {
		pr_err("invalid sdrv extra gpr name\n");
		ret = SDRV_EXTRA_GPR_ERROR_INVALID_NAME;
		return ret;
	}

	if (!addr_base_convert) {
		ret = SDRV_EXTRA_GPR_ERROR_ADDR_CONVERT_FAIL;
		return ret;
	} else
		sdrv_extra_gpr_head = (sdrv_extra_gpr_head_t *)addr_base_convert;

	sdrv_extra_gpr_head = (sdrv_extra_gpr_head_t *)addr_base_convert;

	if (sdrv_extra_gpr_head->magic != SDRV_EXTRA_GPR_MAGIC) {
		pr_err("sdrv extra gpr have not been initialized\n");
		ret = SDRV_EXTRA_GPR_ERROR_NOT_INITIALIZED;
		goto out;
	}

	for (i = 0; i < sdrv_extra_gpr_head->count; i++) {
		if (sdrv_extra_gpr_head->gpr[i].key == key) {
			*value = sdrv_extra_gpr_head->gpr[i].value;
			ret = SDRV_EXTRA_GPR_OK;
			goto out;
		}
	}

	ret = SDRV_EXTRA_GPR_ERROR_FIND_KEY_FAIL;

out:
	return ret;
}

u32 sdrv_extra_gpr_set(const char *name, u32 value)
{
	sdrv_extra_gpr_head_t *sdrv_extra_gpr_head = NULL;
	u32 key;
	u32 ret;
	u32 i;
	u32 num;
	void *addr_base_convert = sdrv_gpr_data->addr_base_convert;

	if (!name) {
		pr_err("invalid sdrv extra gpr para\n");
		ret = SDRV_EXTRA_GPR_ERROR_INVALID_NAME;
		return ret;
	}

	key = sdrv_extra_gpr_key_translate(name);
	if (!key) {
		pr_err("invalid sdrv extra gpr name\n");
		ret = SDRV_EXTRA_GPR_ERROR_INVALID_NAME;
		return ret;
	}

	if (!addr_base_convert) {
		ret = SDRV_EXTRA_GPR_ERROR_ADDR_CONVERT_FAIL;
		return ret;
	} else
		sdrv_extra_gpr_head = (sdrv_extra_gpr_head_t *)addr_base_convert;

	sdrv_extra_gpr_head = (sdrv_extra_gpr_head_t *)addr_base_convert;

	if (sdrv_extra_gpr_head->magic != SDRV_EXTRA_GPR_MAGIC) {
		pr_err("sdrv extra gpr have not been initialized\n");
		ret = SDRV_EXTRA_GPR_ERROR_NOT_INITIALIZED;
		goto out;
	}

	for (i = 0; i < sdrv_extra_gpr_head->count; i++) {
		if (sdrv_extra_gpr_head->gpr[i].key == key) {
			pr_info("you have set this sdrv extra gpr\n");
			sdrv_extra_gpr_head->gpr[i].value = value;
			ret = SDRV_EXTRA_GPR_OK;
			goto out;
		}
	}

	num = sdrv_extra_gpr_head->count;
	sdrv_extra_gpr_head->gpr[num].key = key;
	sdrv_extra_gpr_head->gpr[num].value = value;
	sdrv_extra_gpr_head->count++;
	ret = SDRV_EXTRA_GPR_OK;

out:
	return ret;
}

static int __init sdrv_extra_gpr_init(void)
{
	sdrv_gpr_data = kzalloc(sizeof(struct sdrv_extra_gpr_data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(sdrv_gpr_data))
		return -ENOMEM;

	sdrv_gpr_data->reg_data = sdrv_extra_gpr_reg_data_get();
	if (!sdrv_gpr_data->reg_data) {
		pr_err("fail to get extra gpr reg data\n");
		return -EINVAL;
	}

	sdrv_gpr_data->addr_base = sdrv_extra_gpr_check(sdrv_gpr_data->reg_data);
	if (!sdrv_gpr_data->addr_base) {
		pr_err("fail to get extra gpr addr base\n");
		return -EINVAL;
	}

	sdrv_gpr_data->addr_base_convert =
		ioremap(sdrv_gpr_data->addr_base + 0x10000000, SDRV_EXTRA_GPR_TOTAL_SIZE);
	if (IS_ERR_OR_NULL(sdrv_gpr_data->addr_base_convert)) {
		pr_err("fail to ioremap extra gpr addr base\n");
		sdrv_gpr_data->addr_base_convert = NULL;
		return PTR_ERR(sdrv_gpr_data->addr_base_convert);
	}

	return 0;
}
subsys_initcall_sync(sdrv_extra_gpr_init)
