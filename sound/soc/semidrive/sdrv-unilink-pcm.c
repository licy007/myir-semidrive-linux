/*
 * sdrv-unilink-pcm.c
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
#include <linux/soc/semidrive/ulink.h>
#include <linux/hrtimer.h>

#include "sdrv-common.h"
#include "sdrv-i2s-unilink.h"

struct sdrv_i2s_unilink *g_i2s_unilink_socket[2] = {NULL,NULL};
EXPORT_SYMBOL(g_i2s_unilink_socket);

#if (SDRV_AUDIO_USE_PCIE_DMA)
extern int kunlun_pcie_dma_copy(uint64_t local, uint64_t remote, uint32_t len,
				pcie_dir dir);
#endif

static struct snd_pcm_hardware sdrv_unilink_pcm_hardware = {
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

static void sdrv_unilink_pcm_work(struct work_struct *work)
{
	struct work_of_remote *work_of_remote;
	struct i2s_remote_info *i2s_info;
	am_stream_ctrl_t *stream_ctrl;
	unsigned long flags;
	int err = 0;

	work_of_remote = container_of(work, struct work_of_remote, work);
	i2s_info = (struct i2s_remote_info *)work_of_remote->runtime;
	stream_ctrl = (am_stream_ctrl_t *)&work_of_remote->msg.request.data[0];

	if (stream_ctrl->stream_cmd !=
	    AM_STREAM_BEAT) { /* no need response */
		reinit_completion(
			&i2s_info->cmd_complete); /* for get response */
	}

	err = sdrv_i2s_unilink_send_message(
		g_i2s_unilink_socket[i2s_info->mode], (uint8_t *)&work_of_remote->msg.request,
		sizeof(struct ipcc_data));
	if (err) {
		dev_err(NULL, "%s i2s_unilink send message error(cmd:%d).\n",
				__func__,stream_ctrl->stream_cmd);
	}

	if (stream_ctrl->stream_cmd != AM_STREAM_BEAT) {
		/* wait response from remote */
		err = wait_for_completion_timeout(
			&i2s_info->cmd_complete,
			msecs_to_jiffies(I2S_REMOTE_TIMEOUT));

		if (!err) {
			dev_err(NULL, "%s can not receive remote response(cmd:%d).\n",
				__func__,stream_ctrl->stream_cmd);
		}
	}

	spin_lock_irqsave(&i2s_info->wq_lock, flags);
	i2s_info->work_read_index++;
	i2s_info->work_read_index %= WORK_NUM_MAX;
	spin_unlock_irqrestore(&i2s_info->wq_lock, flags);
}

static int snd_soc_unilink_dai_probe(struct snd_soc_dai *dai)
{
	return 0;
}

static int snd_soc_unilink_dai_remove(struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_ops sdrv_unilink_dai_ops = {
	.startup = NULL,
};

static struct snd_soc_dai_driver sdrv_unilink_pcm_dai = {
    .name = "snd-unilink-pcm-dai0",
    .probe = snd_soc_unilink_dai_probe,
    .remove = snd_soc_unilink_dai_remove,
    .playback =
        {
            .stream_name = "unilink pcm playback",
            .formats = SND_FORMATS,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .channels_min = SND_CHANNELS_MIN,
            .channels_max = SND_CHANNELS_MAX,
        },
    .capture =
        {
            .stream_name = "unilink pcm capture",
            .formats = SND_FORMATS,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .channels_min = SND_CHANNELS_MIN,
            .channels_max = SND_CHANNELS_MAX,
        },
    .ops = &sdrv_unilink_dai_ops,
    .symmetric_rates = 0,
};

static int sdrv_snd_unilink_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	sdrv_unilink_pcm_hardware.buffer_bytes_max =
		i2s_info->prealloc_buffer_size;

	g_i2s_unilink_socket[i2s_info->mode]->unilink_i2s_info[i2s_info->stream_id / 2] =
		i2s_info;

#if (!SDRV_AUDIO_USE_PCIE_DMA)
	i2s_info->buffer_base_addr[substream->stream] = devm_kzalloc(
		cpu_dai->dev,
		sizeof(struct ipcc_data) + g_i2s_unilink_socket[i2s_info->mode]->buffer_size,
		GFP_KERNEL);

	if (!i2s_info->buffer_base_addr[substream->stream]) {
		dev_err(cpu_dai->dev, "failed to alloc data buffer\n");
		return -1;
	}
#endif

	snd_soc_set_runtime_hwparams(substream, &sdrv_unilink_pcm_hardware);

	i2s_info->status[substream->stream] = AM_STREAM_STATUS_OPENED;

	return snd_pcm_hw_constraint_integer(substream->runtime,
					     SNDRV_PCM_HW_PARAM_PERIODS);
}

static int sdrv_snd_unilink_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

#if (!SDRV_AUDIO_USE_PCIE_DMA)
	devm_kfree(cpu_dai->dev, i2s_info->buffer_base_addr[substream->stream]);
	i2s_info->buffer_base_addr[substream->stream] = NULL;
#endif

	sdrv_snd_pcm_close(substream);

	return 0;
}

static int sdrv_snd_unilink_pcm_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	sdrv_snd_pcm_hw_params(substream, params);

#if (SDRV_AUDIO_USE_PCIE_DMA)
	g_i2s_unilink_socket[i2s_info->mode]->local_buffer_addr[substream->stream]
					     [i2s_info->stream_id / 2] =
		(uint64_t)substream->dma_buffer.addr;
#else
	g_i2s_unilink_socket[i2s_info->mode]->local_buffer_addr[substream->stream]
					     [i2s_info->stream_id / 2] =
		(uint64_t)substream->runtime->dma_area;
#endif

	return 0;
}

static int sdrv_unilink_pcm_request_data(struct i2s_remote_info *i2s_info,
					 uint8_t type, uint8_t *buffer, int len)
{
	struct i2s_remote_msg *remote_msg;
	am_stream_ctrl_t *stream_ctrl;
	uint8_t *data_ptr = (uint8_t *)i2s_info->buffer_base_addr[type];
	int transfer_len = 0;

	if (buffer && len > 0) {
		remote_msg = (struct i2s_remote_msg *)data_ptr;
		memcpy(data_ptr + sizeof(struct ipcc_data), buffer, len);
		transfer_len = sizeof(struct ipcc_data) + len;
	} else {
		if (type == SNDRV_PCM_STREAM_PLAYBACK) {
			remote_msg =
				&i2s_info->remote_msg
					 [PCM_TX_REQUEST_DATA]; /* pcm tx */
		} else {
			remote_msg =
				&i2s_info->remote_msg
					 [PCM_RX_REQUEST_DATA]; /* pcm rx */
		}
		transfer_len = sizeof(struct ipcc_data);
	}

	stream_ctrl = (am_stream_ctrl_t *)&remote_msg->request.data[0];

	stream_ctrl->op_code = AM_OP_PCM_STREAM_REQ;
	stream_ctrl->stream_type = type;
	stream_ctrl->stream_id = (type == SNDRV_PCM_STREAM_PLAYBACK) ?
					 i2s_info->stream_id :
					       (i2s_info->stream_id + 1);
	stream_ctrl->stream_cmd = AM_STREAM_REQUEST;
	stream_ctrl->buffer_tail_ptr = i2s_info->buffer_tail[type];

	remote_msg->request.data_len = sizeof(am_stream_ctrl_t);
	remote_msg->request.cmd = AM_OP_PCM_STREAM_REQ;

	stream_ctrl->status = i2s_info->status[type];

	sdrv_i2s_unilink_send_message(g_i2s_unilink_socket[i2s_info->mode],
				      (uint8_t *)&remote_msg->request.data[0],
				      transfer_len);

	return 0;
}

/* calculate the target DMA-buffer position to be written/read */
static void *get_dma_ptr(struct snd_pcm_runtime *runtime, int channel,
			 unsigned long hwoff)
{
	return runtime->dma_area + hwoff +
	       channel * (runtime->dma_bytes / runtime->channels);
}

static int sdrv_unilink_copy_user(struct snd_pcm_substream *substream,
				  int channel, unsigned long pos,
				  void __user *buf, unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;
	int stream_type = substream->stream;
	int stream_id = i2s_info->stream_id / 2;

	if (stream_type == SNDRV_PCM_STREAM_PLAYBACK) {
		/* pcm_write trigger */
		if (copy_from_user(get_dma_ptr(substream->runtime, channel,
					       pos),
				   (void __user *)buf, bytes)) {
			return -EFAULT;
		}

		i2s_info->buffer_wr_ptr[stream_type] +=
				(bytes / i2s_info->param[stream_type].period_size);

#if (SDRV_AUDIO_USE_PCIE_DMA)
		kunlun_pcie_dma_copy(
			g_i2s_unilink_socket[i2s_info->mode]->local_buffer_addr[stream_type]
							     [stream_id] +
				pos,
			g_i2s_unilink_socket[i2s_info->mode]->remote_buffer_addr[(stream_type+1)%2]
							      [stream_id] +
				pos,
			i2s_info->param[stream_type].period_size, DIR_PUSH);
		sdrv_unilink_pcm_request_data(i2s_info, stream_type, NULL, 0);
#else

		int index = i2s_info->buffer_rd_ptr[stream_type] %
			i2s_info->param[stream_type].period_count;
		index *=  i2s_info->param[stream_type].period_size;
		sdrv_unilink_pcm_request_data(
		i2s_info, stream_type,
		(uint8_t *)g_i2s_unilink_socket[i2s_info->mode]
				->local_buffer_addr[stream_type]
							[stream_id] +
			index,
		i2s_info->param[stream_type].period_size);
		if(i2s_info->status[substream->stream] >= AM_STREAM_STATUS_STARTED) {
			if (i2s_info->mode == AM_SHARING_MODE) {
				if (remote_pcm->virt_i2s_switch == 0) {
					mod_timer(&i2s_info->unilink_beat_timer,
						jiffies + msecs_to_jiffies(i2s_info->time_interval*2));
				}
			}
		}
#endif
	} else {
		/* pcm_read trigger */
		if (copy_to_user((void __user *)buf,
				 get_dma_ptr(substream->runtime, channel, pos),
				 bytes)) {
			return -EFAULT;
		}

		sdrv_unilink_pcm_request_data(i2s_info, stream_type, NULL, 0);
	}

	return 0;
}

static int sdrv_unilink_copy_kernel(struct snd_pcm_substream *substream,
				    int channel, unsigned long pos, void *buf,
				    unsigned long bytes)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		memcpy(get_dma_ptr(substream->runtime, channel, pos), buf,
		       bytes);
	} else {
		memcpy(buf, get_dma_ptr(substream->runtime, channel, pos),
		       bytes);
	}

	return 0;
}

static int sdrv_snd_unilink_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		i2s_info->callback[substream->stream] =
			sdrv_remote_pcm_dma_completed;
		i2s_info->callback_param[substream->stream] = (void *)substream;
		i2s_info->status[substream->stream] = AM_STREAM_STATUS_STARTED;

		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (i2s_info->mode == AM_SHARING_MODE) {
				if (remote_pcm->virt_i2s_switch == 0) {
					sdrv_remote_pcm_beat_timer_setup(substream);
				} else {
					hrtimer_start(&i2s_info->hrtime,ms_to_ktime(i2s_info->time_interval),
								HRTIMER_MODE_REL);
				}
			} else {
				sdrv_remote_pcm_trigger(remote_pcm, substream->stream,
					AM_STREAM_START);
			}
		} else {
			if (i2s_info->mode == AM_STREAM_MODE) {
				sdrv_remote_pcm_trigger(remote_pcm, substream->stream,
					AM_STREAM_START);
			}
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		i2s_info->callback[substream->stream] = NULL;
		i2s_info->status[substream->stream] = AM_STREAM_STATUS_STOPPED;
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (i2s_info->mode == AM_SHARING_MODE) {
				if (remote_pcm->virt_i2s_switch == 0) {
					sdrv_remote_pcm_beat_timer_cancel(substream);
				} else {
					hrtimer_cancel(&i2s_info->hrtime);
				}
			} else {
				sdrv_remote_pcm_trigger(remote_pcm, substream->stream,
					AM_STREAM_STOP);
			}
		} else {
			if (i2s_info->mode == AM_STREAM_MODE) {
				sdrv_remote_pcm_trigger(remote_pcm, substream->stream,
					AM_STREAM_STOP);
			}
		}
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	default:
		break;
	}

	return 0;
}

static enum hrtimer_restart sdrv_unilink_period_request_callback(struct hrtimer *timer)
{
	struct i2s_remote_info *i2s_info = container_of(timer,struct i2s_remote_info,hrtime);
	unsigned long flags;

	sdrv_remote_pcm_send_beat(i2s_info,SNDRV_PCM_STREAM_PLAYBACK);

	hrtimer_forward_now(timer,ms_to_ktime(i2s_info->time_interval));

	return HRTIMER_RESTART;
}

static int sdrv_virt_i2s_controls_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *soc_component = snd_soc_kcontrol_component(kcontrol);
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(soc_component->dev);

	ucontrol->value.integer.value[0] = remote_pcm->virt_i2s_switch;

	return 0;
}
static int sdrv_virt_i2s_controls_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *soc_component = snd_soc_kcontrol_component(kcontrol);
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(soc_component->dev);

	remote_pcm->virt_i2s_switch = ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new sdrv_virt_i2s_controls[] = {
	/* Add kcontrol for machine driver. */
	SOC_SINGLE_EXT("Virt-i2s Switch",0,0,1,0,sdrv_virt_i2s_controls_get,
					sdrv_virt_i2s_controls_put),
};

static struct snd_pcm_ops sdrv_unilink_pcm_ops = {
	.open = sdrv_snd_unilink_pcm_open,
	.close = sdrv_snd_unilink_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = sdrv_snd_unilink_pcm_hw_params,
	.hw_free = sdrv_snd_pcm_hw_free,
	.prepare = sdrv_snd_pcm_prepare,
	.trigger = sdrv_snd_unilink_pcm_trigger,
	.mmap = sdrv_snd_pcm_mmap,
	.pointer = sdrv_snd_pcm_pointer,
	.copy_user = sdrv_unilink_copy_user,
	.copy_kernel = sdrv_unilink_copy_kernel,
};

static const struct snd_soc_platform_driver sdrv_unilink_pcm_soc_platform = {
	.ops = &sdrv_unilink_pcm_ops,
	.pcm_new = sdrv_remote_pcm_new,
	.pcm_free = sdrv_pcm_free_dma_buffer,
};

static struct snd_soc_component_driver sdrv_soc_componet = {
	.name = "sdrv-remote-pcm-component",
};

static int sdrv_unilink_pcm_probe(struct platform_device *pdev)
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

	if (i2s_info->stream_id >= (SND_UNILINK_DEVICE_MAX * 2)) {
		dev_err(dev, "%s not support this stream id.\n", __func__);
		return -EINVAL;
	}

	if (of_find_property(dev->of_node, "semidrive,audio-stream", NULL)){
		dev_info(dev, "unilink use in audio stream\n");
		i2s_info->mode = AM_STREAM_MODE;
	} else {
		i2s_info->mode = AM_SHARING_MODE;
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
		"sdrv_unilink_pcm", WQ_HIGHPRI | WQ_UNBOUND | WQ_FREEZABLE);

	if (!i2s_info->sdrv_remote_wq) {
		dev_err(dev, "%s workqueue create failed.\n", __func__);
		return -ENOMEM;
	}

	i2s_info->work_read_index = 0;
	i2s_info->work_write_index = 1;

	for (index = 0; index < WORK_NUM_MAX;
	     index++) { /* add worker to queue */
		INIT_WORK(&i2s_info->work_list[index].work,
			  sdrv_unilink_pcm_work);
		i2s_info->work_list[index].runtime = (void *)i2s_info;
	}

	mutex_init(&i2s_info->tx_lock); /* init lock */
	spin_lock_init(
		&i2s_info->lock[SNDRV_PCM_STREAM_PLAYBACK]); /* playback */
	spin_lock_init(&i2s_info->lock[SNDRV_PCM_STREAM_CAPTURE]); /* capture */
	spin_lock_init(&i2s_info->wq_lock);
	init_completion(&i2s_info->cmd_complete);

	platform_set_drvdata(pdev, remote_pcm); /* set driver_data */

	ret = devm_snd_soc_register_platform(dev,
					     &sdrv_unilink_pcm_soc_platform);
	if (ret) {
		destroy_workqueue(i2s_info->sdrv_remote_wq);
		dev_err(dev, "%s devm_snd_soc_register_platform fail %d.\n",
			__func__, ret);
		return ret;
	}

	if (i2s_info->mode == AM_SHARING_MODE) {
		sdrv_soc_componet.controls = sdrv_virt_i2s_controls;
		sdrv_soc_componet.num_controls = ARRAY_SIZE(sdrv_virt_i2s_controls);
	} else {
		sdrv_soc_componet.controls = NULL;
		sdrv_soc_componet.num_controls = 0;
	}

	ret = devm_snd_soc_register_component(dev, &sdrv_soc_componet,
					      &sdrv_unilink_pcm_dai, 1);
	if (ret) {
		destroy_workqueue(i2s_info->sdrv_remote_wq);
		dev_err(dev, "%s devm_snd_soc_register_component fail %d.\n",
			__func__, ret);
		return ret;
	}

	hrtimer_init(&i2s_info->hrtime,CLOCK_MONOTONIC,HRTIMER_MODE_REL);
	i2s_info->hrtime.function = sdrv_unilink_period_request_callback;

	dev_info(&pdev->dev, "%s pcm register ok = %s stream_id = %d.\n", __func__,
		 pdev->name,i2s_info->stream_id);

	return ret;
}

static int sdrv_unilink_pcm_remove(struct platform_device *pdev)
{
	struct sdrv_remote_pcm *remote_pcm = platform_get_drvdata(pdev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	if (i2s_info->sdrv_remote_wq) {
		destroy_workqueue(i2s_info->sdrv_remote_wq);
	}

	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static const struct of_device_id sdrv_unilink_pcm_of_match[] = {
	{
		.compatible = "semidrive,x9-unilink-i2s",
	},
	{},
};

#ifdef CONFIG_PM_SLEEP
static int sdrv_unilink_pcm_suspend(struct device *dev)
{
	return 0;
}

static int sdrv_unilink_pcm_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops unilink_pcm_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	sdrv_unilink_pcm_suspend, sdrv_unilink_pcm_resume) };
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver sdrv_unilink_pcm_driver = {
    .driver =
        {
            .name = "x9-unilink-i2s",
            .of_match_table = sdrv_unilink_pcm_of_match,
#ifdef CONFIG_PM_SLEEP
            .pm = &unilink_pcm_pm_ops,
#endif
        },
    .probe = sdrv_unilink_pcm_probe,
    .remove = sdrv_unilink_pcm_remove,
};

module_platform_driver(sdrv_unilink_pcm_driver);

MODULE_AUTHOR("Yuansong Jiao <yuansong.jiao@semidrive.com>");
MODULE_DESCRIPTION("X9 unilink pcm ALSA SoC device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 unilink pcm asoc device");