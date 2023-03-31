/*
 * Copyright (c) 2021 Semidrive Semiconductor.
 * All rights reserved.
 *
 * Description: PVT
 *
 * Revision History:
 * -----------------
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/thermal.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "semidrive_pvt.h"

static const s16 dout_to_temp[] = {
	-62, -62, -62, -62, -62, -61, -61, -61, -60, -60, -60, -59, -59, -59, -58, -58,
	-58, -57, -57, -57, -56, -56, -56, -55, -55, -55, -54, -54, -54, -53, -53, -53,
	-52, -52, -52, -51, -51, -51, -50, -50, -50, -49, -49, -49, -48, -48, -48, -47,
	-47, -47, -47, -46, -46, -46, -45, -45, -45, -44, -44, -44, -43, -43, -43, -42,
	-42, -42, -41, -41, -41, -41, -40, -40, -40, -39, -39, -39, -38, -38, -38, -37,
	-37, -37, -36, -36, -36, -36, -35, -35, -35, -34, -34, -34, -33, -33, -33, -32,
	-32, -32, -32, -31, -31, -31, -30, -30, -30, -29, -29, -29, -29, -28, -28, -28,
	-27, -27, -27, -26, -26, -26, -26, -25, -25, -25, -24, -24, -24, -23, -23, -23,
	-23, -22, -22, -22, -21, -21, -21, -20, -20, -20, -20, -19, -19, -19, -18, -18,
	-18, -18, -17, -17, -17, -16, -16, -16, -16, -15, -15, -15, -14, -14, -14, -13,
	-13, -13, -13, -12, -12, -12, -11, -11, -11, -11, -10, -10, -10,  -9,  -9,  -9,
	 -9,  -8,  -8,  -8,  -8,  -7,  -7,  -7,  -6,  -6,  -6,  -6,  -5,  -5,  -5,  -4,
	 -4,  -4,  -4,  -3,  -3,  -3,  -2,  -2,  -2,  -2,  -1,  -1,  -1,  -1,   0,   0,
	  0,   1,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,   4,   4,   4,
	  5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   8,   8,   8,   8,   9,
	  9,   9,   9,  10,  10,  10,  10,  11,  11,  11,  12,  12,  12,  12,  13,  13,
	 13,  13,  14,  14,  14,  14,  15,  15,  15,  15,  16,  16,  16,  16,  17,  17,
	 17,  18,  18,  18,  18,  19,  19,  19,  19,  20,  20,  20,  20,  21,  21,  21,
	 21,  22,  22,  22,  22,  23,  23,  23,  23,  24,  24,  24,  24,  25,  25,  25,
	 25,  26,  26,  26,  26,  27,  27,  27,  27,  28,  28,  28,  28,  29,  29,  29,
	 29,  30,  30,  30,  30,  30,  31,  31,  31,  31,  32,  32,  32,  32,  33,  33,
	 33,  33,  34,  34,  34,  34,  35,  35,  35,  35,  36,  36,  36,  36,  36,  37,
	 37,  37,  37,  38,  38,  38,  38,  39,  39,  39,  39,  40,  40,  40,  40,  40,
	 41,  41,  41,  41,  42,  42,  42,  42,  43,  43,  43,  43,  43,  44,  44,  44,
	 44,  45,  45,  45,  45,  46,  46,  46,  46,  46,  47,  47,  47,  47,  48,  48,
	 48,  48,  48,  49,  49,  49,  49,  50,  50,  50,  50,  50,  51,  51,  51,  51,
	 52,  52,  52,  52,  52,  53,  53,  53,  53,  54,  54,  54,  54,  54,  55,  55,
	 55,  55,  56,  56,  56,  56,  56,  57,  57,  57,  57,  57,  58,  58,  58,  58,
	 59,  59,  59,  59,  59,  60,  60,  60,  60,  60,  61,  61,  61,  61,  62,  62,
	 62,  62,  62,  63,  63,  63,  63,  63,  64,  64,  64,  64,  64,  65,  65,  65,
	 65,  66,  66,  66,  66,  66,  67,  67,  67,  67,  67,  68,  68,  68,  68,  68,
	 69,  69,  69,  69,  69,  70,  70,  70,  70,  70,  71,  71,  71,  71,  71,  72,
	 72,  72,  72,  72,  73,  73,  73,  73,  73,  74,  74,  74,  74,  74,  75,  75,
	 75,  75,  75,  76,  76,  76,  76,  76,  77,  77,  77,  77,  77,  78,  78,  78,
	 78,  78,  79,  79,  79,  79,  79,  79,  80,  80,  80,  80,  80,  81,  81,  81,
	 81,  81,  82,  82,  82,  82,  82,  83,  83,  83,  83,  83,  84,  84,  84,  84,
	 84,  84,  85,  85,  85,  85,  85,  86,  86,  86,  86,  86,  86,  87,  87,  87,
	 87,  87,  88,  88,  88,  88,  88,  89,  89,  89,  89,  89,  89,  90,  90,  90,
	 90,  90,  91,  91,  91,  91,  91,  91,  92,  92,  92,  92,  92,  93,  93,  93,
	 93,  93,  93,  94,  94,  94,  94,  94,  95,  95,  95,  95,  95,  95,  96,  96,
	 96,  96,  96,  96,  97,  97,  97,  97,  97,  98,  98,  98,  98,  98,  98,  99,
	 99,  99,  99,  99,  99, 100, 100, 100, 100, 100, 100, 101, 101, 101, 101, 101,
	102, 102, 102, 102, 102, 102, 103, 103, 103, 103, 103, 103, 104, 104, 104, 104,
	104, 104, 105, 105, 105, 105, 105, 105, 106, 106, 106, 106, 106, 106, 107, 107,
	107, 107, 107, 107, 108, 108, 108, 108, 108, 108, 109, 109, 109, 109, 109, 109,
	110, 110, 110, 110, 110, 110, 111, 111, 111, 111, 111, 111, 112, 112, 112, 112,
	112, 112, 113, 113, 113, 113, 113, 113, 114, 114, 114, 114, 114, 114, 114, 115,
	115, 115, 115, 115, 115, 116, 116, 116, 116, 116, 116, 117, 117, 117, 117, 117,
	117, 118, 118, 118, 118, 118, 118, 118, 119, 119, 119, 119, 119, 119, 120, 120,
	120, 120, 120, 120, 121, 121, 121, 121, 121, 121, 121, 122, 122, 122, 122, 122,
	122, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125,
	125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127,
	127, 127, 128, 128, 128, 128, 128, 128, 129, 129, 129, 129, 129, 129, 129, 130,
	130, 130, 130, 130, 130, 130, 131, 131, 131, 131, 131, 131, 132, 132, 132, 132,
	132, 132, 132, 133, 133, 133, 133, 133, 133, 133, 134, 134, 134, 134, 134, 134,
	135, 135, 135, 135, 135, 135, 135, 136, 136, 136, 136, 136, 136, 136, 137, 137,
	137, 137, 137, 137, 137, 138, 138, 138, 138, 138, 138, 139, 139, 139, 139, 139,
	139, 139, 140, 140, 140, 140, 140, 140, 140, 141, 141, 141, 141, 141, 141, 141,
	142, 142, 142, 142, 142, 142, 142, 143, 143, 143, 143, 143, 143, 143, 144, 144,
	144, 144, 144, 144, 144, 145, 145, 145, 145, 145, 145, 146, 146, 146, 146, 146,
	146, 146, 147, 147, 147, 147, 147, 147, 147, 148, 148, 148, 148, 148, 148, 148,
	149, 149, 149, 149, 149, 149, 149, 150, 150, 150, 150, 150, 150, 150, 151, 151,
	151, 151, 151, 151, 151, 152, 152, 152, 152, 152, 152, 152, 153, 153, 153, 153,
	153, 153, 153, 154, 154, 154, 154, 154, 154, 154, 155, 155, 155, 155, 155, 155
};

static const s16 temp_to_dout[] = {
	  1,   4,   7,  11,  14,  17,  20,  23,  26,  29,  32,  36,  39,  42,  45,  48,
	 51,  55,  58,  61,  64,  67,  71,  74,  77,  80,  84,  87,  90,  93,  97, 100,
	103, 107, 110, 113, 117, 120, 123, 127, 130, 133, 137, 140, 144, 147, 150, 154,
	157, 161, 164, 168, 171, 175, 178, 182, 185, 189, 193, 196, 200, 203, 207, 211,
	214, 218, 222, 225, 229, 233, 237, 240, 244, 248, 252, 256, 259, 263, 267, 271,
	275, 279, 283, 287, 291, 295, 299, 303, 307, 311, 315, 319, 323, 327, 331, 335,
	339, 344, 348, 352, 356, 361, 365, 369, 373, 378, 382, 387, 391, 395, 400, 404,
	409, 413, 418, 422, 427, 431, 436, 441, 445, 450, 455, 459, 464, 469, 474, 479,
	483, 488, 493, 498, 503, 508, 513, 518, 523, 528, 533, 538, 543, 549, 554, 559,
	564, 569, 575, 580, 585, 591, 596, 602, 607, 613, 618, 624, 629, 635, 640, 646,
	652, 657, 663, 669, 675, 681, 686, 692, 698, 704, 710, 716, 722, 728, 734, 741,
	747, 753, 759, 766, 772, 778, 785, 791, 797, 804, 810, 817, 823, 830, 837, 843,
	850, 857, 864, 870, 877, 884, 891, 898, 905, 912, 919, 926, 933, 941, 948, 955,
	962, 970, 977, 984, 992, 999, 1007, 1015, 1022, 1023
};

static const char * const pvt_detect_mode[] = {
	[PVT_DETECT_MODE_TYPE_TEMP]	= "temp",
	[PVT_DETECT_MODE_TYPE_VOL] 	= "vol",
	[PVT_DETECT_MODE_TYPE_P_LVT]	= "p-lvt",
	[PVT_DETECT_MODE_TYPE_P_ULVT]	= "p-ulvt",
	[PVT_DETECT_MODE_TYPE_P_SVT]	= "p-svt",
};

static const char * const pvt_alarm[] = {
	[PVT_ALARM_HYST_H_TYPE]	= "hyst_h",
	[PVT_ALARM_HYST_L_TYPE] = "hyst_l",
	[PVT_ALARM_HYST_R_TYPE]	= "hyst_r",
	[PVT_ALARM_HYST_F_TYPE]	= "hyst_f",
};

static pvt_detect_mode_t semidrive_pvt_detect_mode_get(const char *t)
{
	pvt_detect_mode_t i;

	for (i = 0; i < ARRAY_SIZE(pvt_detect_mode); i++)
		if (!strcasecmp(t, pvt_detect_mode[i]))
			return i;

	return PVT_DETECT_MODE_TYPE_INVALID;
}

static pvt_alarm_t semidrive_pvt_alarm_type_get(const char *t)
{
	pvt_alarm_t i;

	for (i = 0; i < ARRAY_SIZE(pvt_alarm); i++)
		if (!strcasecmp(t, pvt_alarm[i]))
			return i;

	return PVT_ALARM_TYPE_INVALID;
}

static s16 semidrive_pvt_temp_to_dout(int temp)
{
	if ((temp < PVT_TEMP_MIN) || (temp > PVT_TEMP_MAX))
		return -EINVAL;

	return temp_to_dout[temp + PVT_TEMP_OFFSET];
}

static void semidrive_pvt_set_alarm_threshold_temp(struct semidrive_pvt_data *data, int id)
{
	int dout_h_temp = 0;
	int dout_l_temp = 0;

	dout_h_temp = semidrive_pvt_temp_to_dout(to_celsius(data->alarm[id].alarm_high_temp));
	dout_l_temp = semidrive_pvt_temp_to_dout(to_celsius(data->alarm[id].alarm_low_temp));

	switch (data->alarm[id].alarm_hyst_type) {
	case PVT_ALARM_HYST_H_TYPE:
		dout_l_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_H_THRESH_L_MASK, dout_l_temp);
		dout_h_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_H_THRESH_H_MASK, dout_h_temp);
		regmap_write(data->regmap, SEMIDRIVE_REG_PVT_HYST_H(id), dout_l_temp | dout_h_temp);
		break;
	case PVT_ALARM_HYST_L_TYPE:
		dout_l_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_L_THRESH_L_MASK, dout_l_temp);
		dout_h_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_L_THRESH_H_MASK, dout_h_temp);
		regmap_write(data->regmap, SEMIDRIVE_REG_PVT_HYST_L(id), dout_l_temp | dout_h_temp);
		break;
	case PVT_ALARM_HYST_R_TYPE:
		dout_l_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_R_HYST_MASK, dout_l_temp);
		dout_h_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_R_ALARM_MASK, dout_h_temp);
		regmap_write(data->regmap, SEMIDRIVE_REG_PVT_HYST_R(id), dout_l_temp | dout_h_temp);
		break;
	case PVT_ALARM_HYST_F_TYPE:
		dout_l_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_F_ALARM_MASK, dout_l_temp);
		dout_h_temp = FIELD_PREP(SEMIDRIVE_REG_PVT_HYST_F_HYST_MASK, dout_h_temp);
		regmap_write(data->regmap, SEMIDRIVE_REG_PVT_HYST_F(id), dout_h_temp | dout_l_temp);
		break;
	}
}

static void semidrive_pvt_clr_alarm_irq_pending(struct semidrive_pvt_data *data, int id)
{
	regmap_update_bits(data->regmap, SEMIDRIVE_REG_PVT_INT_CLR(id),
			   BIT(data->alarm[id].alarm_hyst_type),
			   BIT(data->alarm[id].alarm_hyst_type));
}

static void semidrive_pvt_enable_alarm_irq(struct semidrive_pvt_data *data, int id)
{
	regmap_write(data->regmap, SEMIDRIVE_REG_PVT_INT_EN(id),
		     BIT(data->alarm[id].alarm_hyst_type));
}

static void semidrive_pvt_disable_alarm_irq(struct semidrive_pvt_data *data, int id)
{
	regmap_write(data->regmap, SEMIDRIVE_REG_PVT_INT_EN(id), 0);
}

static void semidrive_pvt_enable_alarm(struct semidrive_pvt_data *data, int id)
{
	/* set the alarm threshold */
	semidrive_pvt_set_alarm_threshold_temp(data, id);

	/* clear irq pending before enable irq */
	semidrive_pvt_clr_alarm_irq_pending(data, id);

	/* enable alarm mode irq */
	regmap_update_bits(data->regmap, SEMIDRIVE_REG_PVT_CTRL,
			   (data->alarm[id].alarm_hyst_type >= PVT_ALARM_HYST_R_TYPE) ?
			   SEMIDRIVE_REG_PVT_CTRL_RF_MODE_EN(id) :
			   SEMIDRIVE_REG_PVT_CTRL_HL_MODE_EN(id),
			   (data->alarm[id].alarm_hyst_type >= PVT_ALARM_HYST_R_TYPE) ?
			   SEMIDRIVE_REG_PVT_CTRL_RF_MODE_EN(id) :
			   SEMIDRIVE_REG_PVT_CTRL_HL_MODE_EN(id));

	/* enable irq control */
	semidrive_pvt_enable_alarm_irq(data, id);
}

static irqreturn_t semidrive_pvt_alarm_irq(int irq, void *dev)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t semidrive_pvt_alarm_irq_thread(int irq, void *dev)
{
	struct semidrive_pvt_data *data = dev;
	int idx;

	mutex_lock(&data->lock);
	for (idx = 0; idx < data->alarm_num; idx++) {
		if (irq == data->alarm[idx].irq) {
			semidrive_pvt_disable_alarm_irq(data, idx);
			semidrive_pvt_clr_alarm_irq_pending(data, idx);

			if (data->alarm[idx].alarm_handler)
				data->alarm[idx].alarm_handler(data);

			dev_info(data->dev, "Temp %s(%dC) occurred",
			       data->alarm[idx].name,
			       data->alarm[idx].alarm_high_temp);
			break;
		}
	}
	mutex_unlock(&data->lock);

	thermal_zone_device_update(data->tz, THERMAL_DEVICE_ENABLED);

	return IRQ_HANDLED;
}


static void semidrive_pvt_restore_alarm_irq(struct semidrive_pvt_data * data, int ctemp)
{
	int idx;
	int val;

	if (!data->alarm_init_done)
		return;

	mutex_lock(&data->lock);
	for (idx = 0; idx < data->alarm_num; idx++) {
		regmap_read(data->regmap, SEMIDRIVE_REG_PVT_INT_EN(idx), &val);
		if (!(val & SEMIDRIVE_REG_PVT_INT_EN_MASK)) {
			switch (data->alarm[idx].alarm_hyst_type) {
			case PVT_ALARM_HYST_H_TYPE:
			case PVT_ALARM_HYST_R_TYPE:
				if (ctemp < data->alarm[idx].alarm_low_temp) {
					semidrive_pvt_clr_alarm_irq_pending(data, idx);
					semidrive_pvt_enable_alarm_irq(data, idx);
					dev_dbg(data->dev,
						"alarm%d: enable irq with alarm type: [%d]\n",
						idx, data->alarm[idx].alarm_hyst_type);
				}
				break;
			case PVT_ALARM_HYST_L_TYPE:
			case PVT_ALARM_HYST_F_TYPE:
				if (ctemp > data->alarm[idx].alarm_high_temp) {
					semidrive_pvt_clr_alarm_irq_pending(data, idx);
					semidrive_pvt_enable_alarm_irq(data, idx);
					dev_dbg(data->dev,
						"alarm%d: enable irq with alarm type: [%d]\n",
						idx, data->alarm[idx].alarm_hyst_type);
				}
				break;
			}
		}
	}
	mutex_unlock(&data->lock);
}

static int semidrive_pvt_get_temp(void *dat, int *temp)
{
	struct semidrive_pvt_data *data = dat;
	int val;

	regmap_read(data->regmap, SEMIDRIVE_REG_PVT_DOUT, &val);
	val = FIELD_GET(SEMIDRIVE_REG_PVT_DOUT_MASK, val);

	*temp = to_milli_celsius(dout_to_temp[val]);
	semidrive_pvt_restore_alarm_irq(data, *temp);

	dev_dbg(data->dev, "temp: %d\n", *temp);

	return 0;
}

int semidrive_pvt_uninit(struct semidrive_pvt_data *data)
{
	if (data->control_mode == PVT_CONTROL_MODE_EFUSE)
		return 0;

	regmap_update_bits(data->regmap, SEMIDRIVE_REG_PVT_CTRL,
			   SEMIDRIVE_REG_PVT_CTRL_SENS_EN, 0);

	return 0;
}

static void semidrive_pvt_set_mode(struct semidrive_pvt_data *data)
{
	int mode = 0;

	if (data->detect_mode == PVT_DETECT_MODE_TYPE_TEMP)
		mode = 0x0;
	else if (data->detect_mode == PVT_DETECT_MODE_TYPE_VOL)
		mode = 0x1;
	else if (data->detect_mode == PVT_DETECT_MODE_TYPE_P_LVT)
		mode = 0x2;
	else if (data->detect_mode == PVT_DETECT_MODE_TYPE_P_ULVT)
		mode = 0x4;
	else if (data->detect_mode == PVT_DETECT_MODE_TYPE_P_SVT)
		mode = 0x6;

	regmap_update_bits(data->regmap, SEMIDRIVE_REG_PVT_CTRL,
			   SEMIDRIVE_REG_PVT_CTRL_PVMODE,
			   FIELD_PREP(SEMIDRIVE_REG_PVT_CTRL_PVMODE, mode));
}

int semidrive_pvt_init(struct semidrive_pvt_data *data)
{
	if (data->control_mode == PVT_CONTROL_MODE_EFUSE)
		return 0;

	regmap_update_bits(data->regmap, SEMIDRIVE_REG_PVT_CTRL,
			   SEMIDRIVE_REG_PVT_CTRL_MODE, SEMIDRIVE_REG_PVT_CTRL_MODE);

	regmap_update_bits(data->regmap, SEMIDRIVE_REG_PVT_CTRL,
			   SEMIDRIVE_REG_PVT_CTRL_SENS_EN, SEMIDRIVE_REG_PVT_CTRL_SENS_EN);

	semidrive_pvt_set_mode(data);

	return 0;
}

static const struct regmap_config semidrive_pvt_regmap_config = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = SEMIDRIVE_REG_PVT_INT_CLR(1),
};

static int semidrive_pvt_config_alarm(struct semidrive_pvt_data *data, struct platform_device *pdev)
{
	struct device_node *child;
	struct device_node *parent = pdev->dev.of_node;
	const char *alarm_type;
	int idx;
	int err;

	if (!data || !parent)
		return -ENODEV;

	if (of_property_read_u32(parent, "alarm_num", &data->alarm_num)) {
		data->alarm_num = 0;
		return 0;
	}

	if (data->alarm_num > PVT_ALARM_NUM_MAX) {
		dev_err(&pdev->dev, "invalid alarm number: %d\n", data->alarm_num);
		return -EINVAL;
	}

	data->alarm = devm_kcalloc(&pdev->dev, data->alarm_num,
				   sizeof(struct semidrive_alarm), GFP_KERNEL);
	if (!data->alarm)
		return -ENOMEM;

	for (idx = 0; idx < data->alarm_num; idx++) {
		data->alarm[idx].irq = irq_of_parse_and_map(parent, idx);
		if (data->alarm[idx].irq <= 0) {
			dev_err(&pdev->dev, "failed to map irq: %d\n", data->alarm[idx].irq);
			return -EINVAL;
		}

		snprintf(data->alarm[idx].name, ALARM_NAME_LEN, "alarm%d", idx);

		child = of_find_node_by_name(parent, data->alarm[idx].name);
		if (!child) {
			pr_debug("unable to find alarm node\n");
			return -EINVAL;
		}

		if (of_property_read_s32(child, "alarm_low_temp",
					&data->alarm[idx].alarm_low_temp)) {
			dev_err(&pdev->dev, "failed to get alarm_low_temp\n");
			return -EINVAL;
		}
		if ((to_celsius(data->alarm[idx].alarm_low_temp) < PVT_TEMP_MIN) ||
		    (to_celsius(data->alarm[idx].alarm_low_temp) > PVT_TEMP_MAX)) {
			dev_err(data->dev, "invalid alarm low temp: %d\n",
					data->alarm[idx].alarm_low_temp);
			return -EINVAL;
		}

		if (of_property_read_s32(child, "alarm_high_temp",
					&data->alarm[idx].alarm_high_temp)) {
			dev_err(&pdev->dev, "failed to get alarm_high_temp failed\n");
			return -EINVAL;
		}
		if ((to_celsius(data->alarm[idx].alarm_high_temp) < PVT_TEMP_MIN) ||
		    (to_celsius(data->alarm[idx].alarm_high_temp) > PVT_TEMP_MAX)) {
			dev_err(data->dev, "invalid alarm high temp: %d\n",
					data->alarm[idx].alarm_high_temp);
			return -EINVAL;
		}

		if (of_property_read_string(child, "alarm_hyst_type", &alarm_type)) {
			dev_err(&pdev->dev, "failed to get alarm_temp_hysteresis\n");
			return -EINVAL;
		}
		data->alarm[idx].alarm_hyst_type = semidrive_pvt_alarm_type_get(alarm_type);
		if (data->alarm[idx].alarm_hyst_type == PVT_ALARM_TYPE_INVALID) {
			dev_err(&pdev->dev, "invalid alarm hyst type: [%s]\n", alarm_type);
			return -EINVAL;
		}

		of_node_put(child);

		err = devm_request_threaded_irq(&pdev->dev, data->alarm[idx].irq,
				semidrive_pvt_alarm_irq,
				semidrive_pvt_alarm_irq_thread,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"semidrive_pvt_sensor", data);
		if (err < 0) {
			dev_err(&pdev->dev, "failed to request alarm irq: %d\n", err);
			return err;
		}

		semidrive_pvt_enable_alarm(data, idx);
	}

	data->alarm_init_done = true;

	return 0;
}

static int semidrive_pvt_map_dt_data(struct semidrive_pvt_data *data, struct platform_device *pdev)
{
	struct device_node *np;
	struct resource res;
	const char *detect_mode;
	void __iomem *base;

	if (!data || !pdev->dev.of_node)
		return -ENODEV;

	np = pdev->dev.of_node;

	data->id = of_alias_get_id(np, "tsensor");
	if (data->id < 0)
		data->id = 0;

	if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
		 dev_err(&pdev->dev, "failed to get Resource 0\n");
		 return -ENODEV;
	}

	base = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
	if (!base) {
		dev_err(&pdev->dev, "failed to ioremap() io memory region.\n");
		return -EINVAL;
	}
	data->regmap = devm_regmap_init_mmio(&pdev->dev,
						 base,
						 &semidrive_pvt_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&pdev->dev, "failed to request regmap\n");
		return PTR_ERR(data->regmap);
	}
	/* getting the pvt sensor mode :T/V/P */
	if (of_property_read_string(np, "pvt_sensor_mode", &detect_mode)) {
		dev_err(&pdev->dev, "failed to get pvt_sensor_mode\n");
		return -EINVAL;
	}
	data->detect_mode = semidrive_pvt_detect_mode_get(detect_mode);
	if (data->detect_mode == PVT_DETECT_MODE_TYPE_INVALID) {
		dev_err(&pdev->dev, "invalid pvt sensor mode: [%s]\n", detect_mode);
		return -EINVAL;
	}

	/* control mode: 0: from efuse, 1: from register */
	if (of_property_read_u32(np, "control_mode", &data->control_mode))
		data->control_mode = PVT_CONTROL_MODE_REGISTER;

	if (data->control_mode == PVT_CONTROL_MODE_EFUSE)
		dev_info(&pdev->dev, "pvt temp sensor is enabled by efuse.\n");

	return 0;
}

static const struct thermal_zone_of_device_ops semidrive_pvt_ops = {
	.get_temp = semidrive_pvt_get_temp,
};

static int semidrive_pvt_probe(struct platform_device *pdev)
{
	int err = 0;
	struct semidrive_pvt_data *data;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "semidrive_pvt device tree err!\n");
		return -EBUSY;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(data))
		return -ENOMEM;

	platform_set_drvdata(pdev, data);

	data->dev = &pdev->dev;
	err = semidrive_pvt_map_dt_data(data, pdev);
	if (err)
		return err;

	mutex_init(&data->lock);
	semidrive_pvt_init(data);

	data->tz = devm_thermal_zone_of_sensor_register(&pdev->dev, data->id,
						       data,
						       &semidrive_pvt_ops);
	if (IS_ERR(data->tz)) {
		dev_err(&pdev->dev, "failed to register pvt sensor\n");
		return -ENOMEM;
	}

	err = semidrive_pvt_config_alarm(data, pdev);
	if (err) {
		dev_err(&pdev->dev, "failed to configure pvt alarm\n");
		return err;
	}

	return 0;
}

static int semidrive_pvt_remove(struct platform_device *pdev)
{
	struct semidrive_pvt_data *data = platform_get_drvdata(pdev);

	semidrive_pvt_uninit(data);

	return 0;
}

#ifdef CONFIG_PM
static int semidrive_pvt_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct semidrive_pvt_data *data = platform_get_drvdata(pdev);

	semidrive_pvt_uninit(data);

	return 0;
}

static int semidrive_pvt_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct semidrive_pvt_data *data = platform_get_drvdata(pdev);

	semidrive_pvt_init(data);

	return 0;
}

static SIMPLE_DEV_PM_OPS(semidrive_pvt_pm,
			 semidrive_pvt_suspend, semidrive_pvt_resume);
#endif

#ifdef CONFIG_OF
/* Translate OpenFirmware node properties into platform_data */
static struct of_device_id semidrive_pvt_of_match[] = {
	{.compatible = "semidrive, kunlun-pvt-sensor",},
	{},
};

MODULE_DEVICE_TABLE(of, semidrive_pvt_of_match);
#endif

static struct platform_driver semidrive_pvt_driver = {
	.probe  = semidrive_pvt_probe,
	.remove = semidrive_pvt_remove,
	.driver = {
		.name  = "kunlun-pvt-sensor",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm    = &semidrive_pvt_pm,
#endif
		.of_match_table = of_match_ptr(semidrive_pvt_of_match),
	},
};
static int __init semidrive_pvt_sensor_init(void)
{
	return platform_driver_register(&semidrive_pvt_driver);
}

static void __exit semidrive_pvt_sensor_exit(void)
{
	platform_driver_unregister(&semidrive_pvt_driver);
}

device_initcall_sync(semidrive_pvt_sensor_init);
module_exit(semidrive_pvt_sensor_exit);
MODULE_DESCRIPTION("Semidrive pvt sensor driver");
MODULE_AUTHOR("Wei Yin <wei.yin@semidrive.com>");
MODULE_AUTHOR("Xingyu Chen <xingyu.chen@semidrive.com>");
MODULE_LICENSE("GPL v2");
