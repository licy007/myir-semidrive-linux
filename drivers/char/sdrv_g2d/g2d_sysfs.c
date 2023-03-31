/*
* SEMIDRIVE Copyright Statement
* Copyright (c) SEMIDRIVE. All rights reserved
* This software and all rights therein are owned by SEMIDRIVE,
* and are protected by copyright law and other relevant laws, regulations and protection.
* Without SEMIDRIVE’s prior written consent and /or related rights,
* please do not use this software or any potion thereof in any form or by any means.
* You may not reproduce, modify or distribute this software except in compliance with the License.
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* You should have received a copy of the License along with this program.
* If not, see <http://www.semidrive.com/licenses/>.
*/
#include <linux/kthread.h>
#include "sdrv_g2d.h"
#include "g2d_common.h"

#define to_sdrv_g2d(x) container_of(x, struct sdrv_g2d, mdev)

static enum hrtimer_restart monitor_hrtimer_handler(struct hrtimer *timer)
{
	struct g2d_monitor *monitor =
	 (struct g2d_monitor *)container_of(timer, struct g2d_monitor, timer);

	if (monitor->g2d_on_task)
		monitor->valid_times++;

	monitor->timer_count++;
	if (monitor->timer_count == (1000 / monitor->sampling_time)) {
		monitor->occupancy_rate = (monitor->valid_times * 1000) / monitor->timer_count;
		monitor->timer_count = 0;
		monitor->valid_times = 0;
	}

	hrtimer_forward_now(timer, monitor->timeout);

	return HRTIMER_RESTART;
}

static ssize_t monitor_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct miscdevice *mdev = dev_get_drvdata(dev);
	struct sdrv_g2d *gd = to_sdrv_g2d(mdev);
	struct g2d_monitor *monitor = &gd->monitor;
	int tmp = 0;

	ret = kstrtoint(buf, 16, &tmp);
	if (ret) {
		G2D_ERROR("Invalid input: %s\n", buf);
		return count;
	}

	if (!monitor->is_init) {
		monitor->timeout = ktime_set(0, monitor->sampling_time * 1000 * 1000);
		hrtimer_init(&monitor->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		monitor->timer.function = monitor_hrtimer_handler;
		monitor->is_init = true;
	}

	if (tmp) {
		if (!monitor->is_monitor) {
			hrtimer_start(&monitor->timer, monitor->timeout, HRTIMER_MODE_REL);
			monitor->is_monitor = true;
			G2D_INFO("g2d monitor start\n");
		}
	} else {
		if (monitor->is_monitor) {
			hrtimer_cancel(&monitor->timer);
			monitor->is_monitor = false;
			monitor->occupancy_rate = 0;
			G2D_INFO("g2d monitor cancel\n");
		}
	}

	return count;
}

static ssize_t monitor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *mdev = dev_get_drvdata(dev);
	struct sdrv_g2d *gd = to_sdrv_g2d(mdev);
	struct g2d_monitor *monitor = &gd->monitor;
	int count = 0;

	if (!monitor->is_monitor) {
		count += sprintf(buf + count, "monitor is not running, please echo 1 > monitor frist\n");
		return count;
	}

	count += sprintf(buf + count, "%s : Occupancy rate %d.%d%%\n",
					gd->name, monitor->occupancy_rate / 10, monitor->occupancy_rate % 10);

	return count;
}

static DEVICE_ATTR(monitor, S_IRUGO | S_IWUSR,
		   monitor_show,
		   monitor_store);

static ssize_t samplingtime_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct miscdevice *mdev = dev_get_drvdata(dev);
	struct sdrv_g2d *gd = to_sdrv_g2d(mdev);
	struct g2d_monitor *monitor = &gd->monitor;
	int ret;
	int tmp = 0;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret) {
		G2D_ERROR("Invalid input: %s\n", buf);
		return count;
	}

	if (tmp != monitor->sampling_time) {
		monitor->sampling_time = tmp;
		monitor->timeout = ktime_set(0, monitor->sampling_time * 1000 * 1000);
		monitor->timer_count = 0;
		monitor->occupancy_rate = 0;
		G2D_INFO("set sampling_time = %d\n", monitor->sampling_time);
	}

	return count;
}

static ssize_t samplingtime_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *mdev = dev_get_drvdata(dev);
	struct sdrv_g2d *gd = to_sdrv_g2d(mdev);
	struct g2d_monitor *monitor = &gd->monitor;

	return sprintf(buf, "%d\n", monitor->sampling_time);
}

static DEVICE_ATTR(samplingtime, S_IRUGO | S_IWUSR,
		   samplingtime_show,
		   samplingtime_store);

struct attribute *sdrv_g2d_attrs[] = {
	&dev_attr_monitor.attr,
	&dev_attr_samplingtime.attr,
	NULL,
};
