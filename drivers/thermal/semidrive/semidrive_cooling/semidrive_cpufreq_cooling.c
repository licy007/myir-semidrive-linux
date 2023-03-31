#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/cpu.h>
#include <linux/cpu_cooling.h>

#define CPU_CLUSTER_ID_MAX 1

struct semidrive_cpufreq_cooling {
	int cluster_id;
	struct thermal_cooling_device *cdev;
};

static int semidrive_cpufreq_cooling_dt_parse(struct platform_device *pdev)
{
	struct semidrive_cpufreq_cooling *cool = platform_get_drvdata(pdev);

	if(of_property_read_u32(pdev->dev.of_node, "cluster_id", &cool->cluster_id)) {
		dev_err(&pdev->dev, "failed to get cluster_id: %d\n", cool->cluster_id);
		return -EINVAL;
	}
	if (cool->cluster_id > CPU_CLUSTER_ID_MAX) {
		dev_err(&pdev->dev, "invalid cluster id: %d\n", cool->cluster_id);
		return -EINVAL;
	}

	return 0;
}

static int semidrive_cpufreq_cooling_probe(struct platform_device *pdev)
{
	int ret;
	int cpu;
	struct cpufreq_policy *policy;
	struct semidrive_cpufreq_cooling *cool;

	cool = devm_kzalloc(&pdev->dev, sizeof(*cool), GFP_KERNEL);
	if (!cool)
		return -ENOMEM;
	platform_set_drvdata(pdev, cool);

	ret = semidrive_cpufreq_cooling_dt_parse(pdev);
	if (ret)
		return ret;

	/* assume that the cluster shares the same cpufreq_policy object */
	for_each_possible_cpu(cpu) {
		if (topology_physical_package_id(cpu) == cool->cluster_id)
			break;
	}

	policy = cpufreq_cpu_get(cpu);
	if (!policy || !policy->freq_table) {
		dev_info(&pdev->dev,
			 "frequency policy is not initialized. Deferring probe\n");
		return -EPROBE_DEFER;
	}

	cool->cdev = of_cpufreq_cooling_register(pdev->dev.of_node, policy);
	if (IS_ERR(cool->cdev)) {
		dev_err(&pdev->dev, "failed to register cpufreq cooling device\n");
		return PTR_ERR(cool->cdev);
	}

	return 0;
}

static int semidrive_cpufreq_cooling_remove(struct platform_device *pdev)
{
	struct semidrive_cpufreq_cooling *cool = platform_get_drvdata(pdev);

	cpufreq_cooling_unregister(cool->cdev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id semidrive_cpufreq_cooling_of_match[] = {
	{ .compatible = "semidrive, cpufreq-cooling", },
	{ },
};
MODULE_DEVICE_TABLE(of, semidrive_cpufreq_cooling_of_match);
#else
#endif

static struct platform_driver semidrive_cpufreq_cooling_driver = {
	.probe  = semidrive_cpufreq_cooling_probe,
	.remove = semidrive_cpufreq_cooling_remove,
	.driver = {
		.name   = "cpufreq-cooling",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(semidrive_cpufreq_cooling_of_match),
#endif
	}
};

module_platform_driver(semidrive_cpufreq_cooling_driver);

MODULE_DESCRIPTION("semidrive cpufreq cooling driver");
MODULE_AUTHOR("Wei Yin <wei.yin@semidrive.com>");
MODULE_AUTHOR("Xingyu Chen <xingyu.chen@semidrive.com>");
MODULE_LICENSE("GPL v2");
