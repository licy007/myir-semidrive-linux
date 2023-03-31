/*
 * sdrv-rpmsg-pcm.c
 * Copyright (C) 2022 semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "sdrv-common.h"
#include "sdrv-i2s-rpmsg.h"

sdrv_i2s_rpmsg_dev_t g_rpmsg_device = { 0 };
EXPORT_SYMBOL(g_rpmsg_device);

static struct snd_pcm_hardware sdrv_rpmsg_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SND_FORMATS,
	.rates = SND_RATE,
	.rate_min = SND_RATE_MIN,
	.rate_max = SND_RATE_MAX,
	.channels_min = SND_CHANNELS_MIN,
	.channels_max = SND_CHANNELS_MAX,
	.period_bytes_min = MIN_PERIOD_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = MIN_PERIODS,
	.periods_max = MAX_PERIODS,
	.buffer_bytes_max = MAX_ABUF_SIZE,
	.fifo_size = 0,
};

static void sdrv_rpmsg_pcm_work(struct work_struct *work)
{
	struct work_of_remote *work_of_remote;
	struct i2s_remote_info *i2s_info;
	unsigned long flags;
	int err = 0;
	am_stream_ctrl_t *stream_ctrl;

	work_of_remote = container_of(work, struct work_of_remote, work);
	i2s_info = (struct i2s_remote_info *)work_of_remote->runtime;

	mutex_lock(&i2s_info->tx_lock);

	reinit_completion(&i2s_info->cmd_complete); /* for get response */

	err = rpmsg_send(g_rpmsg_device.endpoint[0],
			 (void *)&work_of_remote->msg.request,
			 sizeof(struct ipcc_data));
	if (err) {
		mutex_unlock(&i2s_info->tx_lock);
		return;
	}

	stream_ctrl = (am_stream_ctrl_t *)&work_of_remote->msg.request.data[0];

	/* wait response from remote */
	err = wait_for_completion_timeout(&i2s_info->cmd_complete,
					  msecs_to_jiffies(I2S_REMOTE_TIMEOUT));

	if (!err) {
		dev_err(&g_rpmsg_device.rpdev[i2s_info->stream_id / 2]->dev,
			"%s can not receive remote response.\n", __func__);
	}

	if (stream_ctrl->stream_cmd == AM_STREAM_SET_PARAMS) { /* open */
		complete(&i2s_info->open_complete[stream_ctrl->stream_type]);
	}

	mutex_unlock(&i2s_info->tx_lock);

	spin_lock_irqsave(&i2s_info->wq_lock, flags);
	i2s_info->work_read_index++;
	i2s_info->work_read_index %= WORK_NUM_MAX;
	spin_unlock_irqrestore(&i2s_info->wq_lock, flags);
}

static int snd_soc_rpmsg_dai_probe(struct snd_soc_dai *dai)
{
	return 0;
}

static int snd_soc_rpmsg_dai_remove(struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_ops sdrv_rpmsg_dai_ops = {
	.startup = NULL,
};

static struct snd_soc_dai_driver sdrv_rpmsg_pcm_dai_template = {
    .name = "snd-rpmsg-pcm-dai0",
    .probe = snd_soc_rpmsg_dai_probe,
    .remove = snd_soc_rpmsg_dai_remove,
    .playback =
        {
            .stream_name = "rpmsg pcm playback",
            .formats = SND_FORMATS,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .channels_min = SND_CHANNELS_MIN,
            .channels_max = SND_CHANNELS_MAX,
        },
    .capture =
        {
            .stream_name = "rpmsg pcm capture",
            .formats = SND_FORMATS,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .channels_min = SND_CHANNELS_MIN,
            .channels_max = SND_CHANNELS_MAX,
        },
    .ops = &sdrv_rpmsg_dai_ops,
    .symmetric_rates = 0,
};

static int sdrv_snd_rpmsg_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;
	int ret = 0;

	sdrv_rpmsg_pcm_hardware.buffer_bytes_max =
		i2s_info->prealloc_buffer_size;

	g_rpmsg_device.rpmsg_i2s_info[i2s_info->stream_id / 2] = i2s_info;

	snd_soc_set_runtime_hwparams(substream, &sdrv_rpmsg_pcm_hardware);

	i2s_info->status[substream->stream] = AM_STREAM_STATUS_OPENED;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	return ret;
}

static struct snd_pcm_ops sdrv_rpmsg_pcm_ops = {
	.open = sdrv_snd_rpmsg_pcm_open,
	.close = sdrv_snd_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = sdrv_snd_pcm_hw_params,
	.hw_free = sdrv_snd_pcm_hw_free,
	.prepare = sdrv_snd_pcm_prepare,
	.trigger = sdrv_snd_pcm_trigger,
	.mmap = sdrv_snd_pcm_mmap,
	.pointer = sdrv_snd_pcm_pointer,
};

static const struct snd_soc_platform_driver sdrv_rpmsg_pcm_soc_platform = {
	.ops = &sdrv_rpmsg_pcm_ops,
	.pcm_new = sdrv_remote_pcm_new,
	.pcm_free = sdrv_pcm_free_dma_buffer,
};

static const struct snd_soc_component_driver sdrv_soc_componet = {
	.name = "sdrv-remote-pcm-component",
};

static int sdrv_rpmsg_pcm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	int index = 0;
	struct sdrv_remote_pcm *remote_pcm;
	struct i2s_remote_info *i2s_info;
	unsigned int value = 0;
	unsigned int stream_id = 0;

	remote_pcm =
		devm_kzalloc(dev, sizeof(struct sdrv_remote_pcm), GFP_KERNEL);

	if (!remote_pcm) {
		return -ENOMEM;
	}

	remote_pcm->pdev = pdev;
	i2s_info = &remote_pcm->i2s_info;

	ret = device_property_read_u32(dev, "semidrive,stream-id", &value);
	if (ret) {
		dev_err(dev, "%s please check stream id.\n", __func__);
		return -EINVAL;
	}

	stream_id = __ffs(value);/* use bitmap represent stream id */
	stream_id *= 2;/* stream_id for playback & capture */
	i2s_info->stream_id = stream_id;/* unique id for stream */

	if (i2s_info->stream_id >= (SND_RPMSG_DEVICE_MAX * 2)) {
		dev_err(dev, "%s not support this stream id.\n", __func__);
		return -EINVAL;
	}

	i2s_info->prealloc_buffer_size = MAX_ABUF_SIZE;
	ret = device_property_read_u32(dev, "semidrive,stream-buffer-size",
				       &value);
	if (!ret) {
		i2s_info->prealloc_buffer_size =
			value; /* prealloc dma buffer size */
	}

	/* setup workqueue */
	i2s_info->sdrv_remote_wq = alloc_ordered_workqueue(
		"sdrv_rpmsg_pcm", WQ_HIGHPRI | WQ_UNBOUND | WQ_FREEZABLE);

	if (!i2s_info->sdrv_remote_wq) {
		dev_err(dev, "%s workqueue create failed.\n", __func__);
		return -ENOMEM;
	}

	i2s_info->mode = AM_STREAM_MODE;
	i2s_info->work_read_index = 0;
	i2s_info->work_write_index = 1;

	for (index = 0; index < WORK_NUM_MAX;
	     index++) { /* add worker to queue */
		INIT_WORK(&i2s_info->work_list[index].work,
			  sdrv_rpmsg_pcm_work);
		i2s_info->work_list[index].runtime = (void *)i2s_info;
	}

	mutex_init(&i2s_info->tx_lock); /* init lock */
	spin_lock_init(
		&i2s_info->lock[SNDRV_PCM_STREAM_PLAYBACK]); /* playback */
	spin_lock_init(&i2s_info->lock[SNDRV_PCM_STREAM_CAPTURE]); /* capture */
	spin_lock_init(&i2s_info->wq_lock);
	init_completion(&i2s_info->cmd_complete);
	init_completion(&i2s_info->open_complete[SNDRV_PCM_STREAM_PLAYBACK]);
	init_completion(&i2s_info->open_complete[SNDRV_PCM_STREAM_CAPTURE]);

	platform_set_drvdata(pdev, remote_pcm); /* set driver_data */

	ret = devm_snd_soc_register_platform(dev, &sdrv_rpmsg_pcm_soc_platform);
	if (ret) {
		destroy_workqueue(i2s_info->sdrv_remote_wq);
		dev_err(dev, "%s devm_snd_soc_register_platform fail %d.\n",
			__func__, ret);
		return ret;
	}
	memcpy(&remote_pcm->dai_drv, &sdrv_rpmsg_pcm_dai_template,
	       sizeof(struct snd_soc_dai_driver));

	ret = device_property_read_string(&pdev->dev, "sdrv,rpmsg-dai-name", &remote_pcm->dai_drv.name);
	if (ret < 0) {
		dev_info(&pdev->dev, "snd-rpmsg-pcm-dai '%s'\n",remote_pcm->dai_drv.name);
	}else{
		dev_info(&pdev->dev, "set snd-rpmsg-pcm-dai to '%s'\n",remote_pcm->dai_drv.name);
	}

	ret = devm_snd_soc_register_component(dev, &sdrv_soc_componet,
					      &remote_pcm->dai_drv, 1);
	if (ret) {
		destroy_workqueue(i2s_info->sdrv_remote_wq);
		dev_err(dev, "%s devm_snd_soc_register_component fail %d.\n",
			__func__, ret);
		return ret;
	}

	dev_info(&pdev->dev, "%s pcm register ok = %s stream_id = %d.\n", __func__,
		 pdev->name,i2s_info->stream_id);

	return ret;
}

static int sdrv_rpmsg_pcm_remove(struct platform_device *pdev)
{
	struct sdrv_remote_pcm *remote_pcm = platform_get_drvdata(pdev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	if (i2s_info->sdrv_remote_wq) {
		destroy_workqueue(i2s_info->sdrv_remote_wq);
	}

	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static const struct of_device_id sdrv_rpmsg_pcm_of_match[] = {
	{
		.compatible = "semidrive,x9-rpmsg-i2s",
	},
	{},
};

#ifdef CONFIG_PM_SLEEP
static int sdrv_rpmsg_pcm_suspend(struct device *dev)
{
	return 0;
}

static int sdrv_rpmsg_pcm_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops rpmsg_pcm_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	sdrv_rpmsg_pcm_suspend, sdrv_rpmsg_pcm_resume) };
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver sdrv_rpmsg_pcm_driver = {
    .driver =
        {
            .name = "x9-rpmsg-i2s",
            .of_match_table = sdrv_rpmsg_pcm_of_match,
#ifdef CONFIG_PM_SLEEP
            .pm = &rpmsg_pcm_pm_ops,
#endif
        },
    .probe = sdrv_rpmsg_pcm_probe,
    .remove = sdrv_rpmsg_pcm_remove,
};

module_platform_driver(sdrv_rpmsg_pcm_driver);

MODULE_AUTHOR("Yuansong Jiao <yuansong.jiao@semidrive.com>");
MODULE_DESCRIPTION("X9 rpmsg pcm ALSA SoC device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 rpmsg pcm asoc device");