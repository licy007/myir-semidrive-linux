#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/cpumask.h>
#include <linux/workqueue.h>
#include <linux/thermal.h>

#define CPU_CLUSTER_MAX_NUM		2

/* the each cluster has a unque struct sdrv_cpucore_cooling instance, but
 * the cpu_online_mask is shared by all clusters, so we have to introduce the
 * lock to protect it
 */
static DEFINE_MUTEX(cluster_lock);

struct sdrv_cpucore_cooling {
	int cluster_id;
	unsigned long max_cpucore_nr;
	unsigned long max_online_nr;
	unsigned long cpucore_state;
	cpumask_t cpumask;
	struct work_struct dwork;
	struct thermal_cooling_device *cdev;
};

static int sdrv_get_online_cpu_unlock(struct sdrv_cpucore_cooling *cool)
{
	cpumask_t temp;

	cpumask_and(&temp, &cool->cpumask, cpu_online_mask);
	return cpumask_weight(&temp);
}

static int sdrv_get_online_cpu(struct sdrv_cpucore_cooling *cool)
{
	int online_nr;

	mutex_lock(&cluster_lock);
	online_nr = sdrv_get_online_cpu_unlock(cool);
	mutex_unlock(&cluster_lock);

	return online_nr;
}

static int sdrv_update_online_cpucore(struct sdrv_cpucore_cooling *cool,
				      unsigned long state)
{
	int cur_online_nr;

	cool->max_online_nr = cool->max_cpucore_nr - state;
	cool->cpucore_state = state;
	cur_online_nr = sdrv_get_online_cpu(cool);

	if (cur_online_nr  == cool->max_online_nr)
		return 0;

	schedule_work(&cool->dwork);

	return 0;
}

static int sdrv_cpucore_online(struct sdrv_cpucore_cooling *cool)
{
	int ret;
	unsigned int cpu;

	mutex_lock(&cluster_lock);
	for_each_cpu(cpu, &cool->cpumask) {
		if (cpu_online(cpu))
			continue;

		ret = device_online(get_cpu_device(cpu));
		if (ret) {
			mutex_unlock(&cluster_lock);
			return ret;
		}

		if (sdrv_get_online_cpu_unlock(cool) >= cool->max_online_nr)
			break;
	}
	mutex_unlock(&cluster_lock);

	return 0;
}

static int sdrv_cpucore_offline(struct sdrv_cpucore_cooling *cool)
{
	int ret;
	unsigned int cpu;

	mutex_lock(&cluster_lock);
	for_each_cpu_and(cpu, &cool->cpumask, cpu_online_mask) {
		if (!cpu)
			continue;

		ret = device_offline(get_cpu_device(cpu));
		if (ret) {
			mutex_unlock(&cluster_lock);
			return ret;
		}

		if (sdrv_get_online_cpu_unlock(cool) <= cool->max_online_nr)
			break;

	}
	mutex_unlock(&cluster_lock);

	return 0;
}

static int sdrv_cpucore_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct sdrv_cpucore_cooling *cool = cdev->devdata;

	/* the cpu0 is as bootcpu for ARM, and it does not support hotplug */
	*state = cool->max_cpucore_nr - 1;

	return 0;
}

static int sdrv_cpucore_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct sdrv_cpucore_cooling *cool = cdev->devdata;

	*state = cool->cpucore_state;

	return 0;
}

static int sdrv_cpucore_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct sdrv_cpucore_cooling *cool = cdev->devdata;

	if (WARN_ON(state >= cool->max_cpucore_nr))
		return -EINVAL;

	return sdrv_update_online_cpucore(cool, state);
}

static struct thermal_cooling_device_ops cpucore_cooling_ops = {
	.get_max_state = sdrv_cpucore_get_max_state,
	.get_cur_state = sdrv_cpucore_get_cur_state,
	.set_cur_state = sdrv_cpucore_set_cur_state,
};

static int sdrv_cpucore_cooling_dt_parse(struct platform_device *pdev)
{
	struct sdrv_cpucore_cooling *cool = platform_get_drvdata(pdev);

	if (of_property_read_u32(pdev->dev.of_node, "cluster_id",
				&cool->cluster_id)) {
		dev_err(&pdev->dev, "failed to get cluster_id: %d\n",
			cool->cluster_id);
		return -EINVAL;
	}
	if (cool->cluster_id >= CPU_CLUSTER_MAX_NUM) {
		dev_err(&pdev->dev, "invalid cluster id: %d\n",
			cool->cluster_id);
		return -EINVAL;
	}

	return 0;
}

static void sdrv_cpucore_work(struct work_struct *work)
{
	struct sdrv_cpucore_cooling *cool;
	int cur_online_nr;

	cool = container_of(work, struct sdrv_cpucore_cooling, dwork);
	cur_online_nr = sdrv_get_online_cpu(cool);

	if (cool->max_online_nr > cur_online_nr)
		sdrv_cpucore_online(cool);
	else if (cool->max_online_nr < cur_online_nr)
		sdrv_cpucore_offline(cool);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	unsigned long state;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdrv_cpucore_cooling *cool = platform_get_drvdata(pdev);

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;
	if (state >= cool->max_cpucore_nr) {
		dev_err(dev, "invalid state [%ld]\n", state);
		return -EINVAL;
	}
	sdrv_update_online_cpucore(cool, state);

	return count;
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdrv_cpucore_cooling *cool = platform_get_drvdata(pdev);

	return sprintf(buf, "offline: %ld\n", cool->cpucore_state);
}
static DEVICE_ATTR_RW(state);

static int sdrv_cpucore_cooling_probe(struct platform_device *pdev)
{
	int ret;
	int cpu;
	struct sdrv_cpucore_cooling *cool;
	char dev_name[THERMAL_NAME_LENGTH] = {0};

	cool = devm_kzalloc(&pdev->dev, sizeof(*cool), GFP_KERNEL);
	if (!cool)
		return -ENOMEM;
	platform_set_drvdata(pdev, cool);

	ret = sdrv_cpucore_cooling_dt_parse(pdev);
	if (ret)
		return ret;

	for_each_possible_cpu(cpu) {
		if (topology_physical_package_id(cpu) == cool->cluster_id) {
			cool->max_cpucore_nr++;
			cpumask_set_cpu(cpu, &cool->cpumask);
		}
	}

	if (!cool->max_cpucore_nr) {
		dev_err(&pdev->dev,
			"no valid cpu core found in cluster-%d\n",
			cool->cluster_id);
		return -EINVAL;
	}

	snprintf(dev_name, sizeof(dev_name), "thermal-cpucore-%d",
		 cool->cluster_id);

	INIT_WORK(&cool->dwork, sdrv_cpucore_work);

	ret = device_create_file(&pdev->dev, &dev_attr_state);
	if (ret) {
		dev_err(&pdev->dev, "create sysfs file failed\n");
		return ret;
	}

	cool->cdev = thermal_of_cooling_device_register(pdev->dev.of_node,
							dev_name,
							cool,
							&cpucore_cooling_ops);
	if (IS_ERR(cool->cdev)) {
		dev_err(&pdev->dev,
			"failed to register cpucore cooling device\n");
		return PTR_ERR(cool->cdev);
	}

	return 0;
}

static int sdrv_cpucore_cooling_remove(struct platform_device *pdev)
{
	struct sdrv_cpucore_cooling *cool = platform_get_drvdata(pdev);

	cancel_work_sync(&cool->dwork);
	thermal_cooling_device_unregister(cool->cdev);
	device_remove_file(&pdev->dev, &dev_attr_state);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sdrv_cpucore_cooling_of_match[] = {
	{ .compatible = "semidrive, cpucore-cooling", },
	{ },
};
MODULE_DEVICE_TABLE(of, sdrv_cpucore_cooling_of_match);
#else
#endif

static struct platform_driver sdrv_cpucore_cooling_driver = {
	.probe  = sdrv_cpucore_cooling_probe,
	.remove = sdrv_cpucore_cooling_remove,
	.driver = {
		.name   = "cpucore-cooling",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sdrv_cpucore_cooling_of_match),
#endif
	}
};

module_platform_driver(sdrv_cpucore_cooling_driver);

MODULE_DESCRIPTION("semidrive cpucore cooling driver");
MODULE_AUTHOR("Xingyu Chen <xingyu.chen@semidrive.com>");
MODULE_LICENSE("GPL v2");
