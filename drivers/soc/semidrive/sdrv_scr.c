/*
 * sdrv_scr.c
 *
 * Copyright(c) 2020 Semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/hwspinlock.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <soc/semidrive/sdrv_scr.h>

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

struct sdrv_scr_device;

struct sdrv_scr_signal {
	struct list_head list;
	struct sdrv_scr_device *dev;
	void __iomem *base;
	uint32_t id;
	uint32_t type;		/* RW/RO/L16/L31/R16W16 */
	uint32_t offset;	/* register offset */
	uint32_t start_bit;	/* start bit in the register */
	uint32_t width;		/* bit width */
};

struct sdrv_scr_device {
	struct list_head list;
	uint32_t id;
	struct device *dev;
	phys_addr_t addr;
	struct hwspinlock *hwlock;
	struct mutex lock;	/*if no hwlock, will using the mutex lock */
	struct list_head signals;
};

static LIST_HEAD(g_sdrv_scr_devices);

static inline uint32_t _bit_mask(uint32_t width)
{
	return (1ul << width) - 1;
}

static inline uint32_t _scr_signal_mask(struct sdrv_scr_signal *signal)
{
	return _bit_mask(signal->width) << signal->start_bit;
}

static inline uint32_t _scr_read_reg(struct sdrv_scr_signal *signal)
{
	return readl(signal->base);
}

static inline void _scr_write_reg(struct sdrv_scr_signal *signal,
				  uint32_t val)
{
	writel(val, signal->base);
}

static inline uint32_t _scr_read_signal(struct sdrv_scr_signal *signal)
{
	return (_scr_read_reg(signal) & _scr_signal_mask(signal)) >>
		signal->start_bit;
}

static inline int _scr_dev_lock(struct sdrv_scr_device *scr)
{
	int ret = 0;

	if (scr->hwlock) {
		/*
		 * Try to acquire the HW spinlock, in 10 ms.
		 */
		ret = __hwspin_lock_timeout(scr->hwlock, 10, 0, NULL);
		if (ret)
			dev_alert(scr->dev,
				  "Cannot lock scr device (%d)!\n", ret);
	} else {
		mutex_lock(&scr->lock);
	}

	return ret;
}

static inline void _scr_dev_unlock(struct sdrv_scr_device *scr)
{
	if (scr->hwlock) {
		__hwspin_unlock(scr->hwlock, 0, NULL);
	} else {
		mutex_unlock(&scr->lock);
	}
}

static inline void
_scr_write_signal(struct sdrv_scr_signal *signal, uint32_t _val)
{
	uint32_t val;

	val = _scr_read_reg(signal);
	val &= ~_scr_signal_mask(signal);
	val |= (_val & _bit_mask(signal->width)) << signal->start_bit;
	_scr_write_reg(signal, val);
}

static struct sdrv_scr_device *_scr_dev(uint32_t scr)
{
	struct sdrv_scr_device *scr_dev;

	list_for_each_entry(scr_dev, &g_sdrv_scr_devices, list) {
		if (scr_dev->id == scr) {
			return scr_dev;
		}
	}

	return NULL;
}

static struct sdrv_scr_signal *_scr_map_signal(uint32_t scr, uint32_t _signal)
{
	struct sdrv_scr_device *scr_dev;
	struct sdrv_scr_signal *signal;

	scr_dev = _scr_dev(scr);
	if (scr_dev != NULL) {
		list_for_each_entry(signal, &scr_dev->signals, list) {
			if (signal->id == _signal) {

				if (_scr_dev_lock(signal->dev))
					return NULL;

				signal->base = ioremap(
					scr_dev->addr + signal->offset, 4);
				if (signal->base)
					return signal;
				else {
					_scr_dev_unlock(signal->dev);
					break;
				}
			}
		}
	}

	return NULL;
}

static void _scr_unmap_signal(struct sdrv_scr_signal *signal)
{
	if (signal) {
		iounmap(signal->base);
		signal->base = NULL;
		_scr_dev_unlock(signal->dev);
	}
}

static bool _scr_signal_is_locked(struct sdrv_scr_signal *signal)
{
	uint32_t val = _scr_read_reg(signal);
	bool locked;

	switch (signal->type) {
	case L16:
		if (signal->start_bit < 16)
			locked = !!(val & (1ul << (signal->start_bit + 16)));
		else
			locked = !!(val & (1ul << signal->start_bit));
		break;
	case L31:
		locked = !!(val & (1ul << 31));
		break;
	default:
		locked = false;
		break;
	}

	return locked;
}

static int _scr_lock_signal(struct sdrv_scr_signal *signal)
{
	uint32_t val = _scr_read_reg(signal);
	int ret = 0;

	switch (signal->type) {
	case L16:
		if (signal->start_bit < 16) {
			val |= _scr_signal_mask(signal) << 16;
		} else {
			/* Sticky bit. */
			val |= _scr_signal_mask(signal);
		}
		_scr_write_reg(signal, val);
		break;
	case L31:
		val |= 1ul << 31;
		_scr_write_reg(signal, val);
		break;
	default:
		ret = -EPERM;
		break;
	}

	return ret;
}

static bool _scr_signal_writable(struct sdrv_scr_signal *signal)
{
	switch (signal->type) {
	case L16:
	case L31:
		return !_scr_signal_is_locked(signal);
	case R16W16:
		return signal->start_bit < 16;
	case RW:
		return true;
	default:
		return false;
	}
}

int sdrv_scr_is_locked(uint32_t scr, uint32_t _signal, bool *locked)
{
	struct sdrv_scr_signal *signal = _scr_map_signal(scr, _signal);

	if (!signal || !locked)
		return -EINVAL;

	//if (_scr_dev_lock(signal->dev))
	//	return -EBUSY;

	*locked = _scr_signal_is_locked(signal);
	//_scr_dev_unlock(signal->dev);

	_scr_unmap_signal(signal);
	return 0;
}
EXPORT_SYMBOL(sdrv_scr_is_locked);

int sdrv_scr_lock(uint32_t scr, uint32_t _signal)
{
	struct sdrv_scr_signal *signal = _scr_map_signal(scr, _signal);
	int ret;

	if (!signal)
		return -EINVAL;

	//if (_scr_dev_lock(signal->dev))
	//	return -EBUSY;

	ret = _scr_lock_signal(signal);
	//_scr_dev_unlock(signal->dev);

	_scr_unmap_signal(signal);
	return ret;
}
EXPORT_SYMBOL(sdrv_scr_lock);

int sdrv_scr_get(uint32_t scr, uint32_t _signal, uint32_t *val)
{
	struct sdrv_scr_signal *signal = _scr_map_signal(scr, _signal);

	if (!signal || !val)
		return -EINVAL;

	//if (_scr_dev_lock(signal->dev))
	//	return -EBUSY;

	*val = _scr_read_signal(signal);
	//_scr_dev_unlock(signal->dev);

	_scr_unmap_signal(signal);
	return 0;
}
EXPORT_SYMBOL(sdrv_scr_get);

int sdrv_scr_set(uint32_t scr, uint32_t _signal, uint32_t val)
{
	struct sdrv_scr_signal *signal = _scr_map_signal(scr, _signal);
	int ret;

	if (!signal)
		return -EINVAL;

	//if (_scr_dev_lock(signal->dev))
	//	return -EBUSY;

	if (_scr_signal_writable(signal)) {
		_scr_write_signal(signal, val);
		ret = 0;
	}
	else
		ret = -EPERM;

	//_scr_dev_unlock(signal->dev);

	_scr_unmap_signal(signal);
	return ret;
}
EXPORT_SYMBOL(sdrv_scr_set);

static void _scr_deinit_signals(struct device *dev)
{
	struct sdrv_scr_device *scr_dev = dev_get_drvdata(dev);
	struct sdrv_scr_signal *signal, *tmp;

	if (scr_dev != NULL) {
		list_for_each_entry_safe(signal, tmp, &scr_dev->signals, list) {
			devm_kfree(dev, signal);
		}
	}
}

static int _scr_init_signals(struct device *dev)
{
	struct sdrv_scr_device *scr_dev = dev_get_drvdata(dev);
	struct sdrv_scr_signal *signal;
	int num;
	int ret;
	int i;

	INIT_LIST_HEAD(&scr_dev->signals);
	num = of_property_count_u32_elems(dev->of_node, "sdrv,scr_signals");

	for (i = 0; i < num; ) {
		signal = devm_kzalloc(dev, sizeof(*signal), GFP_KERNEL);
		if (!signal) {
			ret = -ENOMEM;
			goto fail;
		}

		list_add_tail(&signal->list, &scr_dev->signals);

		signal->dev = scr_dev;
		ret = -EINVAL;  /* assume property error */

		if (of_property_read_u32_index(dev->of_node, "sdrv,scr_signals",
					       i++, &signal->id))
			goto fail;
		if (of_property_read_u32_index(dev->of_node, "sdrv,scr_signals",
					       i++, &signal->type))
			goto fail;
		if (of_property_read_u32_index(dev->of_node, "sdrv,scr_signals",
					       i++, &signal->offset))
			goto fail;
		if (of_property_read_u32_index(dev->of_node, "sdrv,scr_signals",
					       i++, &signal->start_bit))
			goto fail;
		if (of_property_read_u32_index(dev->of_node, "sdrv,scr_signals",
					       i++, &signal->width))
			goto fail;

		dev_info(dev, "signal id 0x%x type %d offset 0x%x "
			 "start_bit %d width %d\n",
			 signal->id, signal->type, signal->offset,
			 signal->start_bit, signal->width);
	}

	return 0;
fail:
	dev_alert(dev, "Failed to init SCR signals (%d)\n", ret);
	return ret;
}

static void _sdrv_scr_deinit(struct device *dev)
{
	struct sdrv_scr_device *scr_dev = dev_get_drvdata(dev);

	_scr_deinit_signals(dev);

	if (scr_dev->hwlock)
		hwspin_lock_free(scr_dev->hwlock);

	devm_kfree(dev, scr_dev);
}

static int sdrv_scr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_scr_device *scr_dev;
	struct resource *res;
	unsigned int hwsem;
	int ret;

	scr_dev = devm_kzalloc(dev, sizeof(*scr_dev), GFP_KERNEL);
	if (!scr_dev)
		return -ENOMEM;

	scr_dev->dev = dev;
	platform_set_drvdata(pdev, scr_dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res != NULL)
		scr_dev->addr = res->start;
	else {
		ret = -EINVAL;
		goto fail;
	}

	dev_info(dev, "scr id %d, addr 0x%llx, size 0x%llx\n",
		 scr_dev->id, scr_dev->addr, resource_size(res));

	if (of_property_read_u32(dev->of_node, "sdrv,scr_id", &scr_dev->id)) {
		dev_warn(dev, "SCR id not defined. Assume SCR_SEC\n");
		scr_dev->id = SCR_SEC;
	}

	if (!of_property_read_u32(dev->of_node, "sdrv,hwsem", &hwsem))
		scr_dev->hwlock = hwspin_lock_request_specific(hwsem);

	if (!scr_dev->hwlock) {
		dev_alert(dev, "hwsem undefined or not available. will use the mutexlock,"
			  "SCR operations are not safe!\n");
		mutex_init(&scr_dev->lock);
	}

	ret = _scr_init_signals(dev);
	if (ret)
		goto fail;

	list_add_tail(&scr_dev->list, &g_sdrv_scr_devices);
	return 0;

fail:
	_sdrv_scr_deinit(dev);
	return ret;
}

static int sdrv_scr_remove(struct platform_device *pdev)
{
	_sdrv_scr_deinit(&pdev->dev);
	return 0;
}

static const struct of_device_id sdrv_scr_of_match[] = {
	{ .compatible = "semidrive,scr", },
	{ /* sentinel */  },
};

static struct platform_driver sdrv_scr_driver = {
	.probe = sdrv_scr_probe,
	.remove = sdrv_scr_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table	= sdrv_scr_of_match,
	},
};

static int __init sdrv_scr_init(void)
{
	return platform_driver_register(&sdrv_scr_driver);
}
arch_initcall(sdrv_scr_init);
