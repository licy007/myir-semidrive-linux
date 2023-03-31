/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef __DRIVERS_PINCTRL_COMMON_H
#define __DRIVERS_PINCTRL_COMMON_H

#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>

#define 	DUMMY_PINCTRL  (1)

struct platform_device;

extern struct pinmux_ops sd_pmx_ops;

/**
 * struct sd_pin - describes a single kunlun pin
 * @mux_mode: the mux mode for this pin.
 * @config: the config for this pin.
 * @direct_val: output direction for this pin.
 * @config_need: whether config_reg setup needed.
 */
struct sd_pin_memmap {
	unsigned int mux_mode;
    unsigned long config;
    unsigned int direct_val;
    unsigned int config_need;
};

struct sd_pin {
	unsigned int pin;
	struct sd_pin_memmap pin_memmap;
};

/**
 * struct sd_pin_reg - describe a pin reg map
 * @mux_reg: mux register offset
 * @conf_reg: config register offset
 */
struct sd_pin_reg {
	s16 mux_reg;
	s16 conf_reg;
};

/* decode a generic config into raw register value */
struct sd_cfg_params_decode {
	enum pin_config_param param;
	u32 mask;
	u8 shift;
	bool invert;
};

struct sd_pinctrl_soc_info {
	struct device *dev;
	const struct pinctrl_pin_desc *pins;
	unsigned int npins;
	struct sd_pin_reg *pin_regs;
	unsigned int group_index;
	unsigned int flags;
	const char *gpr_compatible;
	struct mutex mutex;

	/* MUX_MODE shift and mask in case SHARE_MUX_CONF_REG */
	unsigned int mux_mask;
	u8 mux_shift;
	u32 ibe_bit;
	u32 obe_bit;

	/* generic pinconf */
	bool generic_pinconf;
	const struct pinconf_generic_params *custom_params;
	unsigned int num_custom_params;
	struct sd_cfg_params_decode *decodes;
	unsigned int num_decodes;
	void (*fixup)(unsigned long *configs, unsigned int num_configs,
		      u32 *raw_config);

	int (*gpio_set_direction)(struct pinctrl_dev *pctldev,
				  struct pinctrl_gpio_range *range,
				  unsigned offset,
				  bool input);
};

/**
 * @dev: a pointer back to containing device
 * @base: the offset to the controller in virtual memory
 */
struct sd_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pctl;
	void __iomem *base;
	void __iomem *input_sel_base;
	struct sd_pinctrl_soc_info *info;
};

#define SD_PINCTRL_PIN(pin) PINCTRL_PIN(pin, #pin)

int sd_pinctrl_probe(struct platform_device *pdev,
			struct sd_pinctrl_soc_info *info);
int sd_pinctrl_suspend(struct device *dev);
int sd_pinctrl_resume(struct device *dev);



#ifdef CONFIG_PINCTRL_SD_MEMMAP
int sd_pmx_set_one_pin_mem(struct sd_pinctrl *ipctl, struct sd_pin *pin);
int sd_pinconf_backend_get_mem(struct pinctrl_dev *pctldev, unsigned pin_id,
		unsigned long *config);
int sd_pinconf_backend_set_mem(struct pinctrl_dev *pctldev, unsigned pin_id,
		unsigned long *configs, unsigned num_configs);
int sd_pinctrl_parse_pin_mem(struct sd_pinctrl_soc_info *info,
		unsigned int *pin_id, struct sd_pin *pin, const __be32 **list_p,
		u32 generic_config);
#else
static inline int sd_pmx_set_one_pin_mem(struct sd_pinctrl *ipctl, struct sd_pin *pin)
{
	return 0;
}
static inline int sd_pinconf_backend_get_mem(struct pinctrl_dev *pctldev, unsigned pin_id,
		unsigned long *config)
{
	return 0;
}
static inline int sd_pinconf_backend_set_mem(struct pinctrl_dev *pctldev, unsigned pin_id,
		unsigned long *configs, unsigned num_configs)
{
	return 0;
}
static inline int sd_pinctrl_parse_pin_mem(struct sd_pinctrl_soc_info *info,
		unsigned int *pin_id, struct sd_pin *pin, const __be32 **list_p,
		u32 generic_config)
{
	return 0;
}
#endif


#endif /* __DRIVERS_PINCTRL_COMMON_H */
