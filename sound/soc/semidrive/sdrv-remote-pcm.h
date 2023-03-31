/*
 * sdrv-remote-pcm.h
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
#ifndef __SDRV_REMOTE_PCM_H__
#define __SDRV_REMOTE_PCM_H__

#include <linux/module.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>

#define IPCC_LEN_MAX 32
#define WORK_NUM_MAX 20
#define SDRV_STREAM_TYPE_NUM 2
#define SND_PCM_OPS_NUM 12
#define I2S_REMOTE_TIMEOUT 500
#define SDRV_AUDIO_USE_PCIE_DMA 0

typedef enum {
	PCM_TX_START = 0,
	PCM_TX_STOP,
	PCM_TX_PAUSE,
	PCM_TX_HW_PARAMS,
	PCM_TX_REQUEST_DATA,
	PCM_TX_HEART_BEAT,

	PCM_RX_START,
	PCM_RX_STOP,
	PCM_RX_PAUSE,
	PCM_RX_HW_PARAMS,
	PCM_RX_REQUEST_DATA,
	PCM_RX_HEART_BEAT,
} snd_pcm_ops_t;

struct ipcc_data {
	char data[IPCC_LEN_MAX];
	int32_t data_len;
	uint8_t cmd;
	uint8_t param;
	uint8_t reserved0;
	uint8_t reserved1;
} __attribute__((packed));

typedef enum {
	AM_STREAM_TYPE_PLAYBACK = 0,
	AM_STREAM_TYPE_CAPTURE = 1,
} am_stream_type_t;

typedef enum {
	AM_OP_PCM_STREAM_REQ = 15, /* the same to OP_PCM_STREAM_REQ */
	AM_OP_PCM_STREAM_IND,
	AM_OP_NUMB
} am_opcode_t;

typedef enum {
	AM_STREAM_START,
	AM_STREAM_STOP,
	AM_STREAM_PAUSE,
	AM_STREAM_SET_PARAMS,
	AM_STREAM_REQUEST,
	AM_STREAM_BEAT,
	AM_STREAM_SUSPEND,
	AM_STREAM_RESUME,
} am_stream_cmd_t;

typedef enum {
	AM_STREAM_STATUS_CLOSED,
	AM_STREAM_STATUS_STOPPED,
	AM_STREAM_STATUS_OPENED,
	AM_STREAM_STATUS_STARTED,
	AM_STREAM_STATUS_REQUESTED,
} am_stream_status_t;

typedef enum {
	AM_STREAM_MODE,
	AM_SHARING_MODE,
} am_stream_mode_t;

typedef struct {
	uint8_t op_code;
	uint8_t stream_cmd;
	uint8_t stream_id;
	uint8_t stream_type;
	uint32_t buffer_tail_ptr;
	uint8_t  status;
	uint8_t  reserved[3];
} am_stream_ctrl_t;

typedef struct {
	uint8_t stream_fmt;
	uint8_t stream_channels;
	uint16_t stream_rate;
	uint32_t buffer_addr;
	uint32_t period_size;
	uint32_t period_count;
} am_stream_param_t;

typedef struct {
	am_stream_ctrl_t stream_ctrl;
	am_stream_param_t stream_params;
} am_stream_info_t;

struct i2s_remote_msg {
	struct ipcc_data request;
	struct ipcc_data response;
};

struct work_of_remote {
	void *runtime;
	struct i2s_remote_msg msg;
	struct work_struct work;
};

struct i2s_remote_info;
typedef void (*transfer_cb)(void *args, int id);

struct i2s_remote_info {
	struct mutex tx_lock;
	spinlock_t lock[SDRV_STREAM_TYPE_NUM];
	spinlock_t wq_lock;

	struct workqueue_struct *sdrv_remote_wq;
	struct work_of_remote work_list[WORK_NUM_MAX];
	int work_write_index;
	int work_read_index;
	struct i2s_remote_msg remote_msg[SND_PCM_OPS_NUM];

	int stream_id;
	int prealloc_buffer_size;
	am_stream_param_t param[SDRV_STREAM_TYPE_NUM];
	int buffer_tail[SDRV_STREAM_TYPE_NUM]; /* playback & capture */
	unsigned int buffer_wr_ptr[SDRV_STREAM_TYPE_NUM];
	unsigned int buffer_rd_ptr[SDRV_STREAM_TYPE_NUM];
	void *buffer_base_addr[SDRV_STREAM_TYPE_NUM];
	int time_interval;

	void *callback_param[SDRV_STREAM_TYPE_NUM];
	transfer_cb callback[SDRV_STREAM_TYPE_NUM];

	int status[SDRV_STREAM_TYPE_NUM];

	struct timer_list unilink_beat_timer;
	int timer_cnt;

	struct hrtimer hrtime;

	struct completion cmd_complete;
	struct completion open_complete[SDRV_STREAM_TYPE_NUM];

	uint8_t mode;
};

struct sdrv_remote_pcm {
	struct platform_device *pdev;
	struct snd_soc_dai_driver dai_drv;
	struct i2s_remote_info i2s_info;
	int virt_i2s_switch;
};

int sdrv_remote_pcm_new(struct snd_soc_pcm_runtime *rtd);
void sdrv_pcm_free_dma_buffer(struct snd_pcm *pcm);
int sdrv_snd_pcm_mmap(struct snd_pcm_substream *substream,
		      struct vm_area_struct *vma);
int sdrv_snd_pcm_trigger(struct snd_pcm_substream *substream, int cmd);
snd_pcm_uframes_t sdrv_snd_pcm_pointer(struct snd_pcm_substream *substream);
int sdrv_snd_pcm_hw_free(struct snd_pcm_substream *substream);
int sdrv_snd_pcm_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params);
int sdrv_snd_pcm_prepare(struct snd_pcm_substream *substream);
int sdrv_snd_pcm_close(struct snd_pcm_substream *substream);
int sdrv_snd_pcm_open(struct snd_pcm_substream *substream);
void sdrv_remote_pcm_dma_completed(void *arg, int id);
int sdrv_remote_pcm_beat_timer_setup(struct snd_pcm_substream *substream);
int sdrv_remote_pcm_beat_timer_cancel(struct snd_pcm_substream *substream);
void sdrv_remote_pcm_send_beat(struct i2s_remote_info *i2s_info,uint8_t type);
int sdrv_remote_pcm_trigger(struct sdrv_remote_pcm *remote_pcm,
				   uint8_t type, uint8_t cmd);

#endif