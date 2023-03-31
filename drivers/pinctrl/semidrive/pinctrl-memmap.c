/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/slab.h>

#include "../core.h"
#include "pinctrl-common.h"

int sd_pmx_set_one_pin_mem(struct sd_pinctrl *ipctl, struct sd_pin *pin)
{
#ifndef DUMMY_PINCTRL
	const struct sd_pinctrl_soc_info *info = ipctl->info;
	unsigned int pin_id = pin->pin;
	struct sd_pin_reg *pin_reg;
	struct sd_pin_memmap *pin_memmap;
    	unsigned int mux_val;
	pin_reg = &info->pin_regs[pin_id];
	pin_memmap = &pin->pin_memmap;

	if (pin_reg->mux_reg == -1) {
		dev_err(ipctl->dev, "Pin(%s) does not support mux function\n",
			info->pins[pin_id].name);
		return 0;
	}

    mux_val = ((pin_memmap->mux_mode & 0xffffffef) | (pin_memmap->direct_val << 4));

	if (pin_reg && pin_reg->mux_reg) {
		/*
		* changed to dummy pinctrl driver for vm
    		* writel(mux_val, ipctl->base + pin_reg->mux_reg);
		* dev_err(ipctl->dev, "write: offset 0x%x val 0x%x\n", pin_reg->mux_reg, mux_val);
		*/
	}
#endif

	return 0;
}

int sd_pinconf_backend_get_mem(struct pinctrl_dev *pctldev,
			    unsigned pin_id, unsigned long *config)
{
#ifndef DUMMY_PINCTRL
	struct sd_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	struct sd_pinctrl_soc_info *info = ipctl->info;
	const struct sd_pin_reg *pin_reg = &info->pin_regs[pin_id];

	if (pin_reg->conf_reg == -1) {
		dev_err(info->dev, "Pin(%s) does not support config function\n",
			info->pins[pin_id].name);
		return -EINVAL;
	}

	if (pin_reg && pin_reg->conf_reg) {
		/*
		 * changed to dummy pinctrl driver for vm
		 * *config = readl(ipctl->base + pin_reg->conf_reg);
		 */
	}
#endif

	return 0;
}

int sd_pinconf_backend_set_mem(struct pinctrl_dev *pctldev,
			    unsigned pin_id, unsigned long *configs,
			    unsigned num_configs)
{
#ifndef DUMMY_PINCTRL
	struct sd_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	struct sd_pinctrl_soc_info *info = ipctl->info;
	const struct sd_pin_reg *pin_reg = &info->pin_regs[pin_id];
	int i;

	if (pin_reg->conf_reg == -1) {
		dev_err(info->dev, "Pin(%s) does not support config function\n",
			info->pins[pin_id].name);
		return -EINVAL;
	}

	//dev_err(ipctl->dev, "pinconf set pin %s\n", info->pins[pin_id].name);

	for (i = 0; i < num_configs; i++) {
		if (pin_reg && pin_reg->conf_reg) {
			/*
			* changed to dummy pinctrl driver for vm
        		* writel(configs[i], ipctl->base + pin_reg->conf_reg);
        		* dev_err(ipctl->dev, "[set_mem] write: offset 0x%x val 0x%lx\n", pin_reg->conf_reg, configs[i]);
			*/
		}
	} /* for each config */
#endif

	return 0;
}

/*
* kunlun x9: DTS <mux_reg conf_reg mux_mode config direction>
*/
int sd_pinctrl_parse_pin_mem(struct sd_pinctrl_soc_info *info,
			  unsigned int *grp_pin_id, struct sd_pin *pin,
			  const __be32 **list_p, u32 generic_config)
{
#ifndef DUMMY_PINCTRL
	struct sd_pin_memmap *pin_memmap = &pin->pin_memmap;
	u32 mux_reg = be32_to_cpu(*((*list_p)++));  /* [0] mux_reg */
	u32 conf_reg;
	//u32 config;
	unsigned int pin_id;
	struct sd_pin_reg *pin_reg;

    	//dev_err(info->dev, "sd_pinctrl_parse_pin_mem Enter");
    	conf_reg = be32_to_cpu(*((*list_p)++)); /* [1] config_reg */
    	if (!conf_reg) {
        	// no config_reg need to be setup
        	conf_reg = -1;
        	pin_memmap->config_need = 0;
	} else {
        	pin_memmap->config_need = 1;
	}

	if (mux_reg) {
		pin_id = (mux_reg - 0x200) / 4;
		pin_reg = &info->pin_regs[pin_id];
	} else {
		pin_id = 0;
		pin_reg = 0;
	}
	//dev_err(info->dev, "sd_pinctrl_parse_pin_mem: pin_id [%d], mux_reg [0x%08x]", pin_id, mux_reg);

	pin->pin = pin_id;
	*grp_pin_id = pin_id;
	if (pin_reg) {
		pin_reg->mux_reg = mux_reg;
		pin_reg->conf_reg = conf_reg;
	}
    pin_memmap->mux_mode = be32_to_cpu(*((*list_p)++)); /* [2] mux_mode */
	pin_memmap->config = be32_to_cpu(*((*list_p)++)); /* [3] config */
	pin_memmap->direct_val = be32_to_cpu((*(*list_p)++)); /* [4] output direction */

	//if (mux_reg)
	//	dev_err(info->dev, "%s: 0x%x 0x%08lx, direction[%d]", info->pins[pin_id].name,
	//		pin_memmap->mux_mode, pin_memmap->config, pin_memmap->direct_val);
#endif

	return 0;
}
