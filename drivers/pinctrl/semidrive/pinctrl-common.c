/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/slab.h>
#include <linux/regmap.h>

#include "../core.h"
#include "../pinconf.h"
#include "../pinmux.h"
#include "pinctrl-common.h"

static inline const struct group_desc *sd_pinctrl_find_group_by_name(
				struct pinctrl_dev *pctldev,
				const char *name)
{
	const struct group_desc *grp = NULL;
	int i;

	for (i = 0; i < pctldev->num_groups; i++) {
		grp = pinctrl_generic_get_group(pctldev, i);
		if (grp && !strcmp(grp->name, name))
			break;
	}

	return grp;
}

static void sd_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
		   unsigned offset)
{
	seq_printf(s, "%s", dev_name(pctldev->dev));
}

static int sd_dt_node_to_map(struct pinctrl_dev *pctldev,
			struct device_node *np,
			struct pinctrl_map **map, unsigned *num_maps)
{
	struct sd_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	struct sd_pinctrl_soc_info *info = ipctl->info;
	const struct group_desc *grp;
	struct pinctrl_map *new_map;
	struct device_node *parent;
	int map_num = 1;
	int i, j;

	/*
	 * first find the group of this node and check if we need create
	 * config maps for pins
	 */
	grp = sd_pinctrl_find_group_by_name(pctldev, np->name);
	if (!grp) {
		dev_err(info->dev, "unable to find group for node %s\n",
			np->name);
		return -EINVAL;
	}

    for (i = 0; i < grp->num_pins; i++) {
        struct sd_pin *pin = &((struct sd_pin *)(grp->data))[i];

        if (pin->pin_memmap.config_need)
            map_num++;
    }

	new_map = kmalloc(sizeof(struct pinctrl_map) * map_num, GFP_KERNEL);
	if (!new_map)
		return -ENOMEM;

	*map = new_map;
	*num_maps = map_num;

	/* create mux map */
	parent = of_get_parent(np);
	if (!parent) {
		kfree(new_map);
		return -EINVAL;
	}
	new_map[0].type = PIN_MAP_TYPE_MUX_GROUP;
	new_map[0].data.mux.function = parent->name;
	new_map[0].data.mux.group = np->name;
	of_node_put(parent);

	/* create config map */
	new_map++;
	for (i = j = 0; i < grp->num_pins; i++) {
		struct sd_pin *pin = &((struct sd_pin *)(grp->data))[i];

        if (pin->pin_memmap.config_need) {
			new_map[j].type = PIN_MAP_TYPE_CONFIGS_PIN;
			new_map[j].data.configs.group_or_pin =
					pin_get_name(pctldev, pin->pin);
			new_map[j].data.configs.configs =
					&pin->pin_memmap.config;
			new_map[j].data.configs.num_configs = 1;
			j++;
		}
	}

	//dev_err(pctldev->dev, "maps: function %s group %s num %d\n",
	//	(*map)->data.mux.function, (*map)->data.mux.group, map_num);

	return 0;
}

static void sd_dt_free_map(struct pinctrl_dev *pctldev,
				struct pinctrl_map *map, unsigned num_maps)
{
	kfree(map);
}

static const struct pinctrl_ops sd_pctrl_ops = {
	.get_groups_count = pinctrl_generic_get_group_count,
	.get_group_name = pinctrl_generic_get_group_name,
	.get_group_pins = pinctrl_generic_get_group_pins,
	.pin_dbg_show = sd_pin_dbg_show,
	.dt_node_to_map = sd_dt_node_to_map,
	.dt_free_map = sd_dt_free_map,

};

static int sd_pmx_set(struct pinctrl_dev *pctldev, unsigned selector,
		       unsigned group)
{
	struct sd_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	//struct sd_pinctrl_soc_info *info = ipctl->info;
	unsigned int npins;
	int i, err;
	struct group_desc *grp = NULL;
	struct function_desc *func = NULL;

	/*
	 * Configure the mux mode for each pin in the group for a specific
	 * function.
	 */
	grp = pinctrl_generic_get_group(pctldev, group);
	if (!grp)
		return -EINVAL;

	func = pinmux_generic_get_function(pctldev, selector);
	if (!func)
		return -EINVAL;

	npins = grp->num_pins;

	//dev_dbg(ipctl->dev, "enable function %s group %s\n",
	//	func->name, grp->name);

	for (i = 0; i < npins; i++) {
		struct sd_pin *pin = &((struct sd_pin *)(grp->data))[i];

		err = sd_pmx_set_one_pin_mem(ipctl, pin);
		if (err)
			return err;
	}

	return 0;
}

struct pinmux_ops sd_pmx_ops = {
	.get_functions_count = pinmux_generic_get_function_count,
	.get_function_name = pinmux_generic_get_function_name,
	.get_function_groups = pinmux_generic_get_function_groups,
	.set_mux = sd_pmx_set,
};

/* decode generic config into raw register values */
static u32 sd_pinconf_decode_generic_config(struct sd_pinctrl *ipctl,
					      unsigned long *configs,
					      unsigned int num_configs)
{
	struct sd_pinctrl_soc_info *info = ipctl->info;
	struct sd_cfg_params_decode *decode;
	enum pin_config_param param;
	u32 raw_config = 0;
	u32 param_val;
	int i, j;

	WARN_ON(num_configs > info->num_decodes);

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		param_val = pinconf_to_config_argument(configs[i]);
		decode = info->decodes;
		for (j = 0; j < info->num_decodes; j++) {
			if (param == decode->param) {
				if (decode->invert)
					param_val = !param_val;
				raw_config |= (param_val << decode->shift)
					      & decode->mask;
				break;
			}
			decode++;
		}
	}

	if (info->fixup)
		info->fixup(configs, num_configs, &raw_config);

	return raw_config;
}

static u32 sd_pinconf_parse_generic_config(struct device_node *np,
					    struct sd_pinctrl *ipctl)
{
	struct sd_pinctrl_soc_info *info = ipctl->info;
	struct pinctrl_dev *pctl = ipctl->pctl;
	unsigned int num_configs;
	unsigned long *configs;
	int ret;

	if (!info->generic_pinconf)
		return 0;

	ret = pinconf_generic_parse_dt_config(np, pctl, &configs,
					      &num_configs);
	if (ret)
		return 0;

	return sd_pinconf_decode_generic_config(ipctl, configs, num_configs);
}

static int sd_pinconf_get(struct pinctrl_dev *pctldev,
			     unsigned pin_id, unsigned long *config)
{
	//struct sd_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	//const struct sd_pinctrl_soc_info *info = ipctl->info;

	return sd_pinconf_backend_get_mem(pctldev, pin_id, config);
}

static int sd_pinconf_set(struct pinctrl_dev *pctldev,
			     unsigned pin_id, unsigned long *configs,
			     unsigned num_configs)
{
	//struct sd_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	//const struct sd_pinctrl_soc_info *info = ipctl->info;

	return sd_pinconf_backend_set_mem(pctldev, pin_id, configs, num_configs);
}

static void sd_pinconf_dbg_show(struct pinctrl_dev *pctldev,
				   struct seq_file *s, unsigned pin_id)
{
	//struct sd_pinctrl *ipctl = pinctrl_dev_get_drvdata(pctldev);
	//const struct sd_pinctrl_soc_info *info = ipctl->info;
	unsigned long config;
	int ret;

	ret = sd_pinconf_backend_get_mem(pctldev, pin_id, &config);

	if (ret) {
		seq_printf(s, "N/A");
		return;
	}

	seq_printf(s, "0x%lx", config);
}

static void sd_pinconf_group_dbg_show(struct pinctrl_dev *pctldev,
					 struct seq_file *s, unsigned group)
{
	struct group_desc *grp;
	unsigned long config;
	const char *name;
	int i, ret;

	if (group >= pctldev->num_groups)
		return;

	seq_printf(s, "\n");
	grp = pinctrl_generic_get_group(pctldev, group);
	if (!grp)
		return;

	for (i = 0; i < grp->num_pins; i++) {
		struct sd_pin *pin = &((struct sd_pin *)(grp->data))[i];

		name = pin_get_name(pctldev, pin->pin);
		ret = sd_pinconf_get(pctldev, pin->pin, &config);
		if (ret)
			return;
		seq_printf(s, "  %s: 0x%lx\n", name, config);
	}
}

static const struct pinconf_ops sd_pinconf_ops = {
	.pin_config_get = sd_pinconf_get,
	.pin_config_set = sd_pinconf_set,
	.pin_config_dbg_show = sd_pinconf_dbg_show,
	.pin_config_group_dbg_show = sd_pinconf_group_dbg_show,
};


/*  5 u32 * 4 */
#define SD_PIN_SIZE 20

static int sd_pinctrl_parse_groups(struct device_node *np,
				    struct group_desc *grp,
				    struct sd_pinctrl *ipctl,
				    u32 index)
{
	struct sd_pinctrl_soc_info *info = ipctl->info;
	int size, pin_size;
	const __be32 *list, **list_p;
	u32 config;
	int i;

	//dev_err(info->dev, "group(%d): %s\n", index, np->name);

    pin_size = SD_PIN_SIZE;

	if (info->generic_pinconf)
		pin_size -= 4;

	/* Initialise group */
	grp->name = np->name;

	/*
	 * the binding format is kunlun,pins = <PIN_FUNC_ID CONFIG ...>,
	 * do sanity check and calculate pins number
	 *
	 * First try legacy 'kunlun,pins' property, then fall back to the
	 * generic 'pinmux'.
	 *
	 * Note: for generic 'pinmux' case, there's no CONFIG part in
	 * the binding format.
	 */
	list = of_get_property(np, "kunlun,pins", &size);
	if (!list) {
		list = of_get_property(np, "pinmux", &size);
		if (!list) {
			dev_err(info->dev,
				"no kunlun,pins and pins property in node %pOF\n", np);
			return -EINVAL;
		}
	}

	list_p = &list;

    //dev_err(info->dev, "parse_groups: size [%d], pin_size [%d]\n", size, pin_size);

	/* we do not check return since it's safe node passed down */
	if (!size || size % pin_size) {
		dev_err(info->dev, "Invalid kunlun,pins or pins property in node %pOF\n", np);
		return -EINVAL;
	}

	/* first try to parse the generic pin config */
	config = sd_pinconf_parse_generic_config(np, ipctl);

	grp->num_pins = size / pin_size;
	grp->data = devm_kzalloc(info->dev, grp->num_pins *
				 sizeof(struct sd_pin), GFP_KERNEL);
	grp->pins = devm_kzalloc(info->dev, grp->num_pins *
				 sizeof(unsigned int), GFP_KERNEL);
	if (!grp->pins || !grp->data)
		return -ENOMEM;

	for (i = 0; i < grp->num_pins; i++) {
		struct sd_pin *pin = &((struct sd_pin *)(grp->data))[i];

		sd_pinctrl_parse_pin_mem(info, &grp->pins[i],
			pin, list_p, config);
	}

	return 0;
}

static int sd_pinctrl_parse_functions(struct device_node *np,
				       struct sd_pinctrl *ipctl,
				       u32 index)
{
	struct pinctrl_dev *pctl = ipctl->pctl;
	struct sd_pinctrl_soc_info *info = ipctl->info;
	struct device_node *child;
	struct function_desc *func;
	struct group_desc *grp;
	u32 i = 0;

	//dev_err(info->dev, "parse function(%d): %s\n", index, np->name);

	func = pinmux_generic_get_function(pctl, index);
	if (!func)
		return -EINVAL;

	/* Initialise function */
	func->name = np->name;
	func->num_group_names = of_get_child_count(np);
	if (func->num_group_names == 0) {
		dev_err(info->dev, "no groups defined in %pOF\n", np);
		return -EINVAL;
	}
	func->group_names = devm_kcalloc(info->dev, func->num_group_names,
					 sizeof(char *), GFP_KERNEL);
	if (!func->group_names)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		func->group_names[i] = child->name;

		grp = devm_kzalloc(info->dev, sizeof(struct group_desc),
				   GFP_KERNEL);
		if (!grp)
			return -ENOMEM;

		mutex_lock(&info->mutex);
		radix_tree_insert(&pctl->pin_group_tree,
				  info->group_index++, grp);
		mutex_unlock(&info->mutex);

		sd_pinctrl_parse_groups(child, grp, ipctl, i++);
	}

	return 0;
}

/*
 * Check if the DT contains pins in the direct child nodes. This indicates the
 * newer DT format to store pins. This function returns true if the first found
 * kunlun,pins property is in a child of np. Otherwise false is returned.
 */
static bool sd_pinctrl_dt_is_flat_functions(struct device_node *np)
{
	struct device_node *function_np;
	struct device_node *pinctrl_np;

	for_each_child_of_node(np, function_np) {
		if (of_property_read_bool(function_np, "kunlun,pins"))
			return true;

		for_each_child_of_node(function_np, pinctrl_np) {
			if (of_property_read_bool(pinctrl_np, "kunlun,pins"))
				return false;
		}
	}

	return true;
}

static int sd_pinctrl_probe_dt(struct platform_device *pdev,
				struct sd_pinctrl *ipctl)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct pinctrl_dev *pctl = ipctl->pctl;
	struct sd_pinctrl_soc_info *info = ipctl->info;
	u32 nfuncs = 0;
	u32 i = 0;
	bool flat_funcs;

	if (!np)
		return -ENODEV;

	flat_funcs = sd_pinctrl_dt_is_flat_functions(np);
	if (flat_funcs) {
		nfuncs = 1;
	} else {
		nfuncs = of_get_child_count(np);
		if (nfuncs <= 0) {
			dev_err(&pdev->dev, "no functions defined\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < nfuncs; i++) {
		struct function_desc *function;

		function = devm_kzalloc(&pdev->dev, sizeof(*function),
					GFP_KERNEL);
		if (!function)
			return -ENOMEM;

		mutex_lock(&info->mutex);
		radix_tree_insert(&pctl->pin_function_tree, i, function);
		mutex_unlock(&info->mutex);
	}
	pctl->num_functions = nfuncs;

	info->group_index = 0;
	if (flat_funcs) {
		pctl->num_groups = of_get_child_count(np);
	} else {
		pctl->num_groups = 0;
		for_each_child_of_node(np, child)
			pctl->num_groups += of_get_child_count(child);
	}

	if (flat_funcs) {
		sd_pinctrl_parse_functions(np, ipctl, 0);
	} else {
		i = 0;
		for_each_child_of_node(np, child)
			sd_pinctrl_parse_functions(child, ipctl, i++);
	}

	return 0;
}

/*
 * sd_free_resources() - free memory used by this driver
 * @info: info driver instance
 */
static void sd_free_resources(struct sd_pinctrl *ipctl)
{
	if (ipctl->pctl)
		pinctrl_unregister(ipctl->pctl);
}

int sd_pinctrl_probe(struct platform_device *pdev,
		      struct sd_pinctrl_soc_info *info)
{
	struct regmap_config config = { .name = "gpr" };
	struct device_node *dev_np = pdev->dev.of_node;
	struct pinctrl_desc *sd_pinctrl_desc;
	struct device_node *np;
	struct sd_pinctrl *ipctl;
#ifndef DUMMY_PINCTRL
	struct resource *res;
#endif
	struct regmap *gpr;
	int ret, i;

	if (!info || !info->pins || !info->npins) {
		dev_err(&pdev->dev, "wrong pinctrl info\n");
		return -EINVAL;
	}
	info->dev = &pdev->dev;

	if (info->gpr_compatible) {
		gpr = syscon_regmap_lookup_by_compatible(info->gpr_compatible);
		if (!IS_ERR(gpr))
			regmap_attach_dev(&pdev->dev, gpr, &config);
	}

	/* Create state holders etc for this driver */
	ipctl = devm_kzalloc(&pdev->dev, sizeof(*ipctl), GFP_KERNEL);
	if (!ipctl)
		return -ENOMEM;

	info->pin_regs = devm_kmalloc(&pdev->dev, sizeof(*info->pin_regs) *
				      info->npins, GFP_KERNEL);
	if (!info->pin_regs)
		return -ENOMEM;

	for (i = 0; i < info->npins; i++) {
		info->pin_regs[i].mux_reg = -1;
		info->pin_regs[i].conf_reg = -1;
	}

#ifndef DUMMY_PINCTRL
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ipctl->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ipctl->base))
		return PTR_ERR(ipctl->base);
#endif

	if (of_property_read_bool(dev_np, "kunlun,input-sel")) {
		np = of_parse_phandle(dev_np, "kunlun,input-sel", 0);
		if (!np) {
			dev_err(&pdev->dev, "pinctrl kunlun,input-sel property not found\n");
			return -EINVAL;
		}

		ipctl->input_sel_base = of_iomap(np, 0);
		of_node_put(np);
		if (!ipctl->input_sel_base) {
			dev_err(&pdev->dev,
				"pinctrl input select base address not found\n");
			return -ENOMEM;
		}
	}

	sd_pinctrl_desc = devm_kzalloc(&pdev->dev, sizeof(*sd_pinctrl_desc),
					GFP_KERNEL);
	if (!sd_pinctrl_desc)
		return -ENOMEM;

	sd_pinctrl_desc->name = dev_name(&pdev->dev);
	sd_pinctrl_desc->pins = info->pins;
	sd_pinctrl_desc->npins = info->npins;
	sd_pinctrl_desc->pctlops = &sd_pctrl_ops;
	sd_pinctrl_desc->pmxops = &sd_pmx_ops;
	sd_pinctrl_desc->confops = &sd_pinconf_ops;
	sd_pinctrl_desc->owner = THIS_MODULE;

	/* for generic pinconf */
	sd_pinctrl_desc->custom_params = info->custom_params;
	sd_pinctrl_desc->num_custom_params = info->num_custom_params;

	/* platform specific callback */
	sd_pmx_ops.gpio_set_direction = info->gpio_set_direction;

	mutex_init(&info->mutex);

	ipctl->info = info;
	ipctl->dev = info->dev;
	platform_set_drvdata(pdev, ipctl);
	ret = devm_pinctrl_register_and_init(&pdev->dev,
					     sd_pinctrl_desc, ipctl,
					     &ipctl->pctl);
	if (ret) {
		dev_err(&pdev->dev, "could not register SD pinctrl driver\n");
		goto free;
	}

	ret = sd_pinctrl_probe_dt(pdev, ipctl);
	if (ret) {
		dev_err(&pdev->dev, "fail to probe dt properties\n");
		goto free;
	}

	dev_info(&pdev->dev, "initialized Semidrive pinctrl driver\n");

	return pinctrl_enable(ipctl->pctl);

free:
	sd_free_resources(ipctl);

	return ret;
}

int sd_pinctrl_suspend(struct device *dev)
{
       struct sd_pinctrl *ipctl = dev_get_drvdata(dev);

       if (!ipctl)
               return -EINVAL;

       return pinctrl_force_sleep(ipctl->pctl);
}

int sd_pinctrl_resume(struct device *dev)
{
       struct sd_pinctrl *ipctl = dev_get_drvdata(dev);

       if (!ipctl)
               return -EINVAL;

       return pinctrl_force_default(ipctl->pctl);
}
