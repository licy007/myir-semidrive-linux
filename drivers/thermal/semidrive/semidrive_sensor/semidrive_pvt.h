#ifndef __SEMIDRIVE_PVT_H__
#define __SEMIDRIVE_PVT_H__

#include <linux/bitfield.h>
#include <linux/regmap.h>
#include <linux/bitops.h>

#define SEMIDRIVE_REG_PVT_CTRL					0x0
	#define SEMIDRIVE_REG_PVT_CTRL_MODE			BIT(0)
	#define SEMIDRIVE_REG_PVT_CTRL_PVMODE			GENMASK(3, 1)
	#define	SEMIDRIVE_REG_PVT_CTRL_SENS_EN			BIT(4)
	#define SEMIDRIVE_REG_PVT_CTRL_HL_MODE_EN(x)		BIT(16 + (x * 2))
	#define SEMIDRIVE_REG_PVT_CTRL_RF_MODE_EN(x)		BIT(17 + (x * 2))

#define SEMIDRIVE_REG_PVT_DOUT					0x4
	#define SEMIDRIVE_REG_PVT_DOUT_VALID			BIT(0)
	#define	SEMIDRIVE_REG_PVT_DOUT_MASK			GENMASK(10, 1)
#define SEMIDRIVE_REG_PVT_HYST_H(x)				(0x8 + (x) * 32)
	#define SEMIDRIVE_REG_PVT_HYST_H_THRESH_L_MASK		GENMASK(19, 10)
	#define SEMIDRIVE_REG_PVT_HYST_H_THRESH_H_MASK		GENMASK(9, 0)

#define SEMIDRIVE_REG_PVT_HYST_L(x)				(0xc + (x) * 32)
	#define	SEMIDRIVE_REG_PVT_HYST_L_THRESH_L_MASK		GENMASK(19, 10)
	#define SEMIDRIVE_REG_PVT_HYST_L_THRESH_H_MASK		GENMASK(9, 0)

#define SEMIDRIVE_REG_PVT_HYST_R(x)				(0x10 + (x) * 32)
	#define SEMIDRIVE_REG_PVT_HYST_R_HYST_MASK		GENMASK(19, 10)
	#define SEMIDRIVE_REG_PVT_HYST_R_ALARM_MASK		GENMASK(9, 0)

#define SEMIDRIVE_REG_PVT_HYST_F(x)				(0x14 + (x) * 32)
	#define SEMIDRIVE_REG_PVT_HYST_F_HYST_MASK		GENMASK(19, 10)
	#define SEMIDRIVE_REG_PVT_HYST_F_ALARM_MASK		GENMASK(9, 0)

#define SEMIDRIVE_REG_PVT_HYST_TIME(x)				(0x18 + (x) * 32)

#define SEMIDRIVE_REG_PVT_INT_EN(x)				(0x1c + (x) * 32)
	#define SEMIDRIVE_REG_PVT_INT_EN_HYST_F			BIT(3)
	#define	SEMIDRIVE_REG_PVT_INT_EN_HYST_R			BIT(2)
	#define	SEMIDRIVE_REG_PVT_INT_EN_HYST_L			BIT(1)
	#define	SEMIDRIVE_REG_PVT_INT_EN_HYST_H			BIT(0)
	#define SEMIDRIVE_REG_PVT_INT_EN_MASK			GENMASK(3, 0)

#define SEMIDRIVE_REG_PVT_INT_STATUS(x)				(0x20 + (x) * 32)
	#define SEMIDRIVE_REG_PVT_STATUS_HYST_F			BIT(3)
	#define	SEMIDRIVE_REG_PVT_STATUS_HYST_R			BIT(2)
	#define	SEMIDRIVE_REG_PVT_STATUS_HYST_L			BIT(1)
	#define	SEMIDRIVE_REG_PVT_STATUS_HYST_H			BIT(0)

#define SEMIDRIVE_REG_PVT_INT_CLR(x)				(0x24 + (x) * 32)
	#define SEMIDRIVE_REG_PVT_INT_CLR_HYST_F		BIT(3)
	#define	SEMIDRIVE_REG_PVT_INT_CLR_HYST_R		BIT(2)
	#define	SEMIDRIVE_REG_PVT_INT_CLR_HYST_L		BIT(1)
	#define	SEMIDRIVE_REG_PVT_INT_CLR_HYST_H		BIT(0)

/* The thermal framework expects milli-celsius */
#define to_milli_celsius(temp)					((temp) * 1000)
#define to_celsius(milli_temp)					((milli_temp) / 1000)

#define PVT_ALARM_NUM_MAX					2
#define ALARM_NAME_LEN						10
#define PVT_TEMP_OFFSET						62
#define PVT_TEMP_MIN						-62
#define PVT_TEMP_MAX						155

typedef enum pvt_detect_mode {
	PVT_DETECT_MODE_TYPE_TEMP = 0,
	PVT_DETECT_MODE_TYPE_VOL,
	PVT_DETECT_MODE_TYPE_P_LVT,
	PVT_DETECT_MODE_TYPE_P_ULVT,
	PVT_DETECT_MODE_TYPE_P_SVT,
	PVT_DETECT_MODE_TYPE_INVALID
} pvt_detect_mode_t;

typedef enum pvt_alarm {
	PVT_ALARM_HYST_H_TYPE = 0,
	PVT_ALARM_HYST_L_TYPE,
	PVT_ALARM_HYST_R_TYPE,
	PVT_ALARM_HYST_F_TYPE,
	PVT_ALARM_TYPE_INVALID
} pvt_alarm_t;

typedef enum pvt_control_mode {
	PVT_CONTROL_MODE_EFUSE = 0,
	PVT_CONTROL_MODE_REGISTER,
} pvt_control_mode_t;

struct semidrive_pvt_data;
struct semidrive_alarm {
	int irq;
	char name[ALARM_NAME_LEN];
	int alarm_low_temp;
	int alarm_high_temp;
	int alarm_hyst_type;
	int (*alarm_handler) (struct semidrive_pvt_data *data);
};

struct semidrive_pvt_data {
	int id;
	int alarm_num;
	int alarm_init_done;
	struct mutex lock;
	pvt_detect_mode_t detect_mode;
	pvt_control_mode_t control_mode;
	struct device *dev;
	struct regmap *regmap;
	struct pvt_info_attr *ths_info_attrs;
	struct thermal_zone_device *tz;
	struct semidrive_alarm *alarm;
};
#endif
