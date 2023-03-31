/*
 * sdrv-remote-pcm.c
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
#include "sdrv-remote-pcm.h"

static void sdrv_worker_start_transfer(struct i2s_remote_info *i2s_info,
				       struct i2s_remote_msg *rpc_msg)
{
	unsigned long flags;
	int index = 0;

	spin_lock_irqsave(&i2s_info->wq_lock, flags);
	if (i2s_info->work_write_index != i2s_info->work_read_index) {
		index = i2s_info->work_write_index;
		memcpy(&i2s_info->work_list[index].msg, rpc_msg,
		       sizeof(struct ipcc_data));
		queue_work(i2s_info->sdrv_remote_wq,
			   &i2s_info->work_list[index].work);
		i2s_info->work_write_index++;
		i2s_info->work_write_index %= WORK_NUM_MAX;
	}
	spin_unlock_irqrestore(&i2s_info->wq_lock, flags);
}

void sdrv_remote_pcm_dma_completed(void *arg, int id)
{
	struct i2s_remote_info *i2s_info = (struct i2s_remote_info *)arg;
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)i2s_info->callback_param[id];

	snd_pcm_period_elapsed(substream);
}

int sdrv_snd_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	flush_workqueue(i2s_info->sdrv_remote_wq);

	i2s_info->status[substream->stream] = AM_STREAM_STATUS_CLOSED;

	return ret;
}

int sdrv_snd_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;
	struct i2s_remote_msg *remote_msg;
	am_stream_info_t *stream_hw_params;
	int err = 0;

	if(i2s_info->status[substream->stream] == AM_STREAM_STATUS_OPENED ||
		i2s_info->mode == AM_STREAM_MODE) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			remote_msg = &i2s_info->remote_msg[PCM_TX_HW_PARAMS];
		} else {
			remote_msg = &i2s_info->remote_msg[PCM_RX_HW_PARAMS];
		}

		stream_hw_params = (am_stream_info_t *)&remote_msg->request.data[0];

		reinit_completion(&i2s_info->open_complete[substream->stream]);

		stream_hw_params->stream_params.period_size =
			snd_pcm_lib_period_bytes(substream);
		stream_hw_params->stream_params.period_count =
			snd_pcm_lib_buffer_bytes(substream) /
			stream_hw_params->stream_params.period_size;
		sdrv_worker_start_transfer(i2s_info, remote_msg);

		i2s_info->param[substream->stream].period_count =
			stream_hw_params->stream_params.period_count;
		i2s_info->param[substream->stream].period_size =
			stream_hw_params->stream_params.period_size;

		i2s_info->buffer_wr_ptr[substream->stream] = 0;
		i2s_info->buffer_rd_ptr[substream->stream] = 0;

		dev_info(
			cpu_dai->dev,
			"%s period_count: %u, period_size: %u,stream %u\n",
			__func__, stream_hw_params->stream_params.period_count,
			stream_hw_params->stream_params.period_size,substream->stream);

		err = wait_for_completion_timeout(
			&i2s_info->open_complete[substream->stream],
			msecs_to_jiffies(I2S_REMOTE_TIMEOUT)); /* wait 500 ms */
		if (!err) {
			dev_err(cpu_dai->dev,
				"%s can not receive open complete.\n",
				__func__);
		}
	}

	i2s_info->buffer_tail[substream->stream] = 0;

	return 0;
}

int sdrv_snd_pcm_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;
	struct i2s_remote_msg *remote_msg;
	am_stream_info_t *stream_hw_params;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		remote_msg = &i2s_info->remote_msg[PCM_TX_HW_PARAMS];
		i2s_info->time_interval = (1000*params_period_size(params))
									/ params_rate(params);
	} else {
		remote_msg = &i2s_info->remote_msg[PCM_RX_HW_PARAMS];
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);

	i2s_info->buffer_tail[substream->stream] = 0;

	stream_hw_params = (am_stream_info_t *)&remote_msg->request.data[0];

	stream_hw_params->stream_ctrl.op_code = AM_OP_PCM_STREAM_REQ;
	stream_hw_params->stream_ctrl.stream_type = substream->stream;
	stream_hw_params->stream_ctrl.stream_id =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
			i2s_info->stream_id :
			      (i2s_info->stream_id + 1);
	stream_hw_params->stream_ctrl.stream_cmd = AM_STREAM_SET_PARAMS;
	stream_hw_params->stream_ctrl.buffer_tail_ptr =
		i2s_info->buffer_tail[substream->stream];

	stream_hw_params->stream_params.stream_rate =
		params_rate(params); /* sample rate */
	stream_hw_params->stream_params.stream_fmt =
		params_format(params); /* sample format */
	stream_hw_params->stream_params.stream_channels =
		params_channels(params); /* sample channels */
	stream_hw_params->stream_params.buffer_addr =
		substream->dma_buffer.addr; /* physical address */

	dev_info(
	    cpu_dai->dev,
	    "%s srate: %u, channels: %u, sample_size: %u, stream: %u,interval: %u,period_size: %u \n",
	    __func__, params_rate(params), params_channels(params),
		params_format(params),substream->stream,i2s_info->time_interval,
		params_period_size(params));

	i2s_info->param[substream->stream].stream_fmt = params_format(params);
	i2s_info->param[substream->stream].stream_channels =
		params_channels(params);
	i2s_info->param[substream->stream].stream_rate = params_rate(params);

	remote_msg->request.data_len = sizeof(am_stream_info_t);
	remote_msg->request.cmd = AM_OP_PCM_STREAM_REQ;

	return 0;
}

int sdrv_snd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

snd_pcm_uframes_t sdrv_snd_pcm_pointer(struct snd_pcm_substream *substream)
{
	unsigned int pos = 0;
	int buffer_tail = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	buffer_tail = i2s_info->buffer_tail[substream->stream];
	pos = buffer_tail * snd_pcm_lib_period_bytes(substream);

	return bytes_to_frames(runtime, pos);
}

int sdrv_remote_pcm_trigger(struct sdrv_remote_pcm *remote_pcm,
				   uint8_t type, uint8_t cmd)
{
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;
	struct i2s_remote_msg *remote_msg;
	am_stream_ctrl_t *stream_ctrl;

	if (type == SNDRV_PCM_STREAM_PLAYBACK) {
		remote_msg = &i2s_info->remote_msg[cmd]; /* pcm tx */
	} else {
		remote_msg = &i2s_info->remote_msg[cmd + 6]; /* pcm rx */
	}

	stream_ctrl = (am_stream_ctrl_t *)&remote_msg->request.data[0];

	stream_ctrl->op_code = AM_OP_PCM_STREAM_REQ;
	stream_ctrl->stream_type = type;
	stream_ctrl->stream_id = (type == SNDRV_PCM_STREAM_PLAYBACK) ?
					 i2s_info->stream_id :
					       (i2s_info->stream_id + 1);
	stream_ctrl->stream_cmd = cmd;
	stream_ctrl->buffer_tail_ptr = i2s_info->buffer_tail[type];

	remote_msg->request.data_len = sizeof(am_stream_ctrl_t);
	remote_msg->request.cmd = AM_OP_PCM_STREAM_REQ;

	if (cmd == AM_STREAM_START) {
		i2s_info->status[type] = AM_STREAM_STATUS_STARTED;
	} else if (cmd == AM_STREAM_STOP) {
		i2s_info->status[type] = AM_STREAM_STATUS_STOPPED;
	}

	sdrv_worker_start_transfer(i2s_info, remote_msg);

	return 0;
}

void sdrv_remote_pcm_send_beat(struct i2s_remote_info *i2s_info,uint8_t type)
{
	struct i2s_remote_msg *remote_msg;
	am_stream_ctrl_t *stream_ctrl;

	if (type == SNDRV_PCM_STREAM_PLAYBACK) {
		remote_msg = &i2s_info->remote_msg[PCM_TX_HEART_BEAT]; /* pcm tx */
	} else {
		remote_msg = &i2s_info->remote_msg[PCM_RX_HEART_BEAT]; /* pcm rx */
	}

	stream_ctrl = (am_stream_ctrl_t *)&remote_msg->request.data[0];

	stream_ctrl->op_code = AM_OP_PCM_STREAM_REQ;
	stream_ctrl->stream_type = type;
	stream_ctrl->stream_id = (type == SNDRV_PCM_STREAM_PLAYBACK) ?
					i2s_info->stream_id :
						(i2s_info->stream_id + 1);
	stream_ctrl->stream_cmd = AM_STREAM_BEAT;
	stream_ctrl->buffer_tail_ptr = i2s_info->buffer_tail[type];
	stream_ctrl->status = i2s_info->status[type];

	remote_msg->request.data_len = sizeof(am_stream_ctrl_t);
	remote_msg->request.cmd = AM_OP_PCM_STREAM_REQ;

	sdrv_worker_start_transfer(i2s_info, remote_msg);
}

static void sdrv_remote_pcm_beat_callback(unsigned long data)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;
	struct i2s_remote_msg *remote_msg;
	am_stream_ctrl_t *stream_ctrl;
	uint8_t type = substream->stream;

	sdrv_remote_pcm_send_beat(i2s_info,type);
}

int sdrv_remote_pcm_beat_timer_setup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	i2s_info->unilink_beat_timer.expires = jiffies + msecs_to_jiffies(i2s_info->time_interval);

	setup_timer(&i2s_info->unilink_beat_timer, sdrv_remote_pcm_beat_callback,(unsigned long)substream);

	mod_timer(&i2s_info->unilink_beat_timer,jiffies + msecs_to_jiffies(i2s_info->time_interval*2));

	return 0;
}

int sdrv_remote_pcm_beat_timer_cancel(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;

	del_timer_sync(&i2s_info->unilink_beat_timer);

	return 0;
}

int sdrv_snd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
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
		sdrv_remote_pcm_trigger(remote_pcm, substream->stream,
					AM_STREAM_START);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		i2s_info->callback[substream->stream] = NULL;
		i2s_info->status[substream->stream] = AM_STREAM_STATUS_STOPPED;
		sdrv_remote_pcm_trigger(remote_pcm, substream->stream,
					AM_STREAM_STOP);
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

int sdrv_snd_pcm_mmap(struct snd_pcm_substream *substream,
		      struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_wc(substream->pcm->card->dev, vma, runtime->dma_area,
			   runtime->dma_addr, runtime->dma_bytes);
}

static int sdrv_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream,
					   int size)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buffer = &substream->dma_buffer;

	buffer->dev.type = SNDRV_DMA_TYPE_DEV;
	buffer->dev.dev = pcm->card->dev;
	buffer->private_data = NULL;
	buffer->area =
		dma_alloc_wc(pcm->card->dev, size, &buffer->addr, GFP_KERNEL);

	if (!buffer->area) {
		return -ENOMEM;
	}

	buffer->bytes = size;

	return 0;
}

void sdrv_pcm_free_dma_buffer(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream = NULL;
	struct snd_dma_buffer *buffer = NULL;
	int stream;

	for (stream = SNDRV_PCM_STREAM_PLAYBACK; stream < SNDRV_PCM_STREAM_LAST;
	     stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buffer = &substream->dma_buffer;
		if (!buffer->area)
			continue;

		dma_free_wc(pcm->card->dev, buffer->bytes, buffer->area,
			    buffer->addr);
		buffer->area = NULL;
	}
}

int sdrv_remote_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sdrv_remote_pcm *remote_pcm = dev_get_drvdata(cpu_dai->dev);
	struct i2s_remote_info *i2s_info = &remote_pcm->i2s_info;
	struct device *dev = cpu_dai->dev;
	int ret = 0;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "%s dma alloc failed.\n", __func__);
		return ret;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = sdrv_pcm_preallocate_dma_buffer(
			pcm, SNDRV_PCM_STREAM_PLAYBACK,
			i2s_info->prealloc_buffer_size);
		if (ret) {
			dev_err(dev,
				"%s playback couldn't sdrv_pcm_preallocate_dma_buffer.\n",
				__func__);
			goto err;
		}
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = sdrv_pcm_preallocate_dma_buffer(
			pcm, SNDRV_PCM_STREAM_CAPTURE,
			i2s_info->prealloc_buffer_size);
		if (ret) {
			dev_err(dev,
				"%s capture couldn't sdrv_pcm_preallocate_dma_buffer.\n",
				__func__);
			goto err;
		}
	}

	dev_info(dev, "%s dma alloc success.\n", __func__);

err:
	if (ret) {
		sdrv_pcm_free_dma_buffer(pcm);
	}

	return 0;
}
