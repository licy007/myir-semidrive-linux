/*
 * sdrv-i2s-unilink.c
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
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/ktime.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <net/net_namespace.h>
#include <uapi/linux/in.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include "sdrv-i2s-unilink.h"

#define SDRV_I2S_UNILINK_DEFAULT_PORT 15535
#define SDRV_I2S_UNILINK_DEFAULT_IP "170.20.2.35"
#define SDRV_I2S_UNILINK_DEFAULT_BUF_SIZE 4096

extern struct sdrv_i2s_unilink *g_i2s_unilink_socket[2];

int sdrv_i2s_unilink_send_message(struct sdrv_i2s_unilink *i2s_unilink,
				  uint8_t *buffer, uint16_t len)
{
	struct msghdr msg;
	struct kvec iov;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &i2s_unilink->server_addr;
	msg.msg_namelen = sizeof(i2s_unilink->server_addr);

	iov.iov_base = buffer;
	iov.iov_len = len;

	mutex_lock(&i2s_unilink->tx_lock);

	ret = kernel_sendmsg(i2s_unilink->sock, &msg, &iov, 1, iov.iov_len);
	if (ret < 0) {
		dev_err(i2s_unilink->dev, "fail to send, ret %d\n", ret);
		mutex_unlock(&i2s_unilink->tx_lock);
		return ret;
	}

	mutex_unlock(&i2s_unilink->tx_lock);

	return 0;
}

static int sdrv_i2s_unilink_reponse(struct sdrv_i2s_unilink *i2s_unilink,
				    am_stream_ctrl_t *stream_param, int status)
{
	am_stream_ctrl_t stream_ctrl;

	stream_ctrl.op_code = AM_OP_PCM_STREAM_IND;
	stream_ctrl.stream_type = stream_param->stream_type;
	stream_ctrl.stream_id = stream_param->stream_id;
	stream_ctrl.stream_cmd = stream_param->stream_cmd;
	stream_ctrl.buffer_tail_ptr = stream_param->buffer_tail_ptr;
	stream_ctrl.status = status;

	sdrv_i2s_unilink_send_message(i2s_unilink, (uint8_t *)&stream_ctrl,
				      sizeof(am_stream_ctrl_t));

	return 0;
}

static int sdrv_i2s_unilink_message_handler(void *data)
{
	struct sdrv_i2s_unilink *i2s_unilink = (struct sdrv_i2s_unilink *)data;
	struct msghdr msg;
	struct kvec iov;
	int recv_len;
	int param_err = 0;
	struct ipcc_data *ipcc_msg;
	am_stream_ctrl_t *stream_ctrl;
	struct i2s_remote_info *i2s_info;
	struct sdrv_remote_pcm *remote_pcm;
	int stream_id;
	int index;
	int stream_type;
	int offset;
	unsigned long flags;

#if (!SDRV_AUDIO_USE_PCIE_DMA)
	int recv_buffer_size =
		sizeof(struct ipcc_data) + i2s_unilink->buffer_size;
#else
	int recv_buffer_size = sizeof(struct ipcc_data);
#endif
	uint8_t *recv_buffer =
		devm_kzalloc(i2s_unilink->dev, recv_buffer_size, GFP_KERNEL);
	if (!recv_buffer) {
		dev_err(i2s_unilink->dev,
			"fail to alloc unilink recv buffer\n");
		return -1;
	}

	for (;;) {
		memset(&msg, 0, sizeof(msg));
		iov.iov_base = recv_buffer;
		iov.iov_len = recv_buffer_size;

		recv_len = kernel_recvmsg(i2s_unilink->sock, &msg, &iov, 1,
					  iov.iov_len, 0);
		if (recv_len < 0) {
			dev_err(i2s_unilink->dev, "fail to recv msg, ret %d\n",
				recv_len);
		}

		i2s_info = NULL;
		ipcc_msg = (struct ipcc_data *)recv_buffer;
		stream_ctrl =
			(am_stream_ctrl_t *)&ipcc_msg->data[0];

		stream_id = stream_ctrl->stream_id / SDRV_STREAM_TYPE_NUM;
		stream_type = (stream_ctrl->stream_type + 1) % SDRV_STREAM_TYPE_NUM;

		if (stream_ctrl->stream_type >= SDRV_STREAM_TYPE_NUM &&
			stream_id >= SND_UNILINK_DEVICE_MAX) {
			dev_err(i2s_unilink->dev,
				"unexpected stream_type =%d,stream_id = %d\n",
				stream_ctrl->stream_type, stream_id);
			continue;
		}

		i2s_info = i2s_unilink->unilink_i2s_info[stream_id];

		if (i2s_info && i2s_info->mode == AM_STREAM_MODE) {
			if (stream_ctrl->op_code ==
				AM_OP_PCM_STREAM_IND) { /* far end response */
				complete(&i2s_info->cmd_complete);
			}

			index = stream_ctrl->stream_type;

			if (index == SNDRV_PCM_STREAM_PLAYBACK) {/* playback*/
				if (stream_ctrl->op_code ==
					AM_OP_PCM_STREAM_REQ) {
					spin_lock_irqsave(
						&i2s_info->lock[index],
						flags);
					i2s_info->buffer_rd_ptr[index]++;
					i2s_info->buffer_tail[index] = i2s_info->buffer_rd_ptr[index] %
					i2s_info->param[index].period_count;
					spin_unlock_irqrestore(&i2s_info->lock[index], flags);

					if (i2s_info->callback[index]) {
						i2s_info->callback[index](
							(void *)i2s_info,
							index);
					}
				}
			} else {/* capture */
				if (stream_ctrl->op_code == AM_OP_PCM_STREAM_REQ) {
#if (!SDRV_AUDIO_USE_PCIE_DMA)
					offset =
						i2s_info->buffer_tail
							[index] *
						i2s_info->param[index]
							.period_size;
					memcpy((char *)i2s_unilink->local_buffer_addr
								[index]
								[stream_id] +
							offset,
						recv_buffer +
							sizeof(struct ipcc_data),
						i2s_info->param[index]
							.period_size);
#endif
					spin_lock_irqsave(&i2s_info->lock[index],
						flags);
					i2s_info->buffer_tail[index]++;
					if (i2s_info->buffer_tail[index] >=
						i2s_info->param[index]
							.period_count) {
						i2s_info->buffer_tail
							[index] = 0;
					}
					spin_unlock_irqrestore(
					&i2s_info->lock[index], flags);

					if (i2s_info->callback[index]) {
						i2s_info->callback[index](
							(void *)i2s_info,
							index);
					}
				}
			}
		} else {
			if (stream_ctrl->op_code == AM_OP_PCM_STREAM_IND) { /* far end response */
				if (stream_ctrl->stream_cmd != AM_STREAM_BEAT &&
					stream_ctrl->stream_cmd != AM_STREAM_REQUEST) {
					complete(&i2s_info->cmd_complete);
				}

				if (stream_ctrl->stream_type ==
					SNDRV_PCM_STREAM_PLAYBACK) { /* playback response */
					if ((stream_ctrl->stream_cmd ==
						AM_STREAM_BEAT) &&
						i2s_info->status[stream_ctrl->stream_type] >=
							AM_STREAM_STATUS_STARTED) {

						spin_lock_irqsave(
							&i2s_info->lock
									[stream_ctrl
										->stream_type],
							flags);
						i2s_info->buffer_rd_ptr
							[stream_ctrl
									->stream_type]++;
						i2s_info->buffer_tail
							[stream_ctrl
									->stream_type] =
							i2s_info->buffer_rd_ptr
								[stream_ctrl
										->stream_type] %
							i2s_info->param
								[stream_ctrl
										->stream_type]
									.period_count;

						spin_unlock_irqrestore(
							&i2s_info->lock
									[stream_ctrl
										->stream_type],
							flags);

						if (i2s_info->callback
								[stream_ctrl
										->stream_type]) {
							i2s_info->callback[stream_ctrl
											->stream_type](
								(void *)i2s_info,
								stream_ctrl
									->stream_type);
						}
					}
				}
			} else if (stream_ctrl->op_code ==
				AM_OP_PCM_STREAM_REQ) { /* request */

				if (stream_ctrl->stream_cmd != AM_STREAM_REQUEST) {
					int status =
						(i2s_info == NULL) ?
							AM_STREAM_STATUS_CLOSED :
								i2s_info->status[stream_type];
					sdrv_i2s_unilink_reponse(i2s_unilink,
								stream_ctrl, status);
				}

				if (i2s_info &&
					(stream_ctrl->stream_cmd == AM_STREAM_REQUEST) &&
					(i2s_info->status[stream_type] >=
					AM_STREAM_STATUS_STARTED)) { /* remote capture <- local playback */
					/* remote playback -> local capure */

					if (stream_type ==
						SNDRV_PCM_STREAM_PLAYBACK) { /* from remote capture */

						remote_pcm = container_of(i2s_info,
										struct sdrv_remote_pcm,i2s_info);

						if (remote_pcm->virt_i2s_switch == 0) {
							spin_lock_irqsave(&i2s_info->lock[stream_type],
								flags);
							i2s_info->buffer_rd_ptr[stream_type]++;

							i2s_info->buffer_tail[stream_type] =
								i2s_info->buffer_rd_ptr
									[stream_type] %
								i2s_info->param[stream_type]
									.period_count;
							spin_unlock_irqrestore(
								&i2s_info->lock[stream_type], flags);
						} else {
							continue;
						}
					} else { /* from remote playback */
#if (!SDRV_AUDIO_USE_PCIE_DMA)
						offset =
							i2s_info->buffer_tail
								[stream_type] *
							i2s_info->param[stream_type]
								.period_size;
						memcpy((char *)i2s_unilink->local_buffer_addr
									[stream_type]
									[stream_id] +
								offset,
							recv_buffer +
								sizeof(struct ipcc_data),
							i2s_info->param[stream_type]
								.period_size);
#endif
						spin_lock_irqsave(&i2s_info->lock[stream_type],
							flags);
						i2s_info->buffer_tail[stream_type]++;
						if (i2s_info->buffer_tail[stream_type] >=
							i2s_info->param[stream_type]
								.period_count) {
							i2s_info->buffer_tail
								[stream_type] = 0;
						}
						spin_unlock_irqrestore(
						&i2s_info->lock[stream_type], flags);
					}

					if (i2s_info->callback[stream_type]) {
						i2s_info->callback[stream_type](
							(void *)i2s_info, stream_type);
					}
				} else {
					if (stream_ctrl->stream_cmd ==
						AM_STREAM_SET_PARAMS) { /* notify remote buffer address */
						uint8_t *stream_ptr =
							(uint8_t *)stream_ctrl;
						am_stream_param_t *stream_param =
							(am_stream_param_t
								*)(stream_ptr +
									sizeof(am_stream_ctrl_t));
						i2s_unilink->remote_buffer_addr
							[stream_ctrl->stream_type]
							[stream_id] =
							stream_param->buffer_addr;
						if (i2s_info &&
							i2s_info->status[stream_type] >=
								AM_STREAM_STATUS_OPENED) { /* check param sanity */
							param_err = 0;
							if (i2s_info->param[stream_type]
									.period_size !=
								stream_param->period_size) {
								param_err |= 0x01;
							}
							if (i2s_info->param[stream_type]
									.stream_rate !=
								stream_param->stream_rate) {
								param_err |= 0x02;
							}
							if (i2s_info->param[stream_type]
									.stream_channels !=
								stream_param
									->stream_channels) {
								param_err |= 0x04;
							}
							if (i2s_info->param[stream_type]
									.stream_fmt !=
								stream_param->stream_fmt) {
								param_err |= 0x08;
							}
							if (param_err != 0)
								dev_err(i2s_unilink->dev,
									"virtual i2s param not match! (err:%d)\n",
									param_err);
						}
					}
				}
			}
		}
	}

	return 0;
}

static int
sdrv_i2s_unilink_create_msg_handler(struct sdrv_i2s_unilink *i2s_unilink)
{
	struct sockaddr_in addr;
	int ret = 0;

	ret = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP,
			       &i2s_unilink->sock);
	if (ret < 0) {
		dev_err(i2s_unilink->dev, "fail to create socket\n");
		return ret;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(i2s_unilink->local_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = kernel_bind(i2s_unilink->sock, (struct sockaddr *)&addr,
			  sizeof(addr));
	if (ret < 0) {
		dev_err(i2s_unilink->dev, "fail to bind port %d\n",
			i2s_unilink->local_port);
		sock_release(i2s_unilink->sock);
		i2s_unilink->sock = NULL;
		return ret;
	}

	i2s_unilink->msg_handler =
		kthread_run(sdrv_i2s_unilink_message_handler, i2s_unilink,
			    "i2s-unilink-udp-%d", i2s_unilink->local_port);

	if (!i2s_unilink->msg_handler) {
		dev_err(i2s_unilink->dev, "kthread run failed (%d)\n",
			i2s_unilink->local_port);
		sock_release(i2s_unilink->sock);
		i2s_unilink->sock = NULL;
		ret = -1;
	}

	return ret;
}

static int sdrv_i2s_unilink_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdrv_i2s_unilink *i2s_unilink;
	int ret;
	const char *ip;
	uint32_t port;
	uint32_t mode = 0;
	uint32_t buf_size = 0;

	i2s_unilink = devm_kzalloc(dev, sizeof(*i2s_unilink), GFP_KERNEL);
	if (!i2s_unilink)
		return -ENOMEM;

	i2s_unilink->dev = dev;
	platform_set_drvdata(pdev, i2s_unilink);

	if (of_property_read_string(dev->of_node, "server-ip", &ip)) {
		i2s_unilink->server_addr.sin_addr.s_addr =
			in_aton(SDRV_I2S_UNILINK_DEFAULT_IP);
		dev_info(dev, "sdrv_i2s_unilink use default server-ip:(%s).\n",
			 SDRV_I2S_UNILINK_DEFAULT_IP);
	} else {
		i2s_unilink->server_addr.sin_addr.s_addr = in_aton(ip);
	}

	if (of_property_read_u32(dev->of_node, "server-port", &port)) {
		port = SDRV_I2S_UNILINK_DEFAULT_PORT;
		dev_info(dev,
			 "sdrv_i2s_unilink use default server-port:(%d).\n",
			 SDRV_I2S_UNILINK_DEFAULT_PORT);
	}
	i2s_unilink->server_port = port;

	if (of_property_read_u32(dev->of_node, "local-port", &port)) {
		port = SDRV_I2S_UNILINK_DEFAULT_PORT;
	}
	i2s_unilink->local_port = port;

	if (of_property_read_u32(dev->of_node, "buffer-size", &buf_size)) {
		buf_size = SDRV_I2S_UNILINK_DEFAULT_BUF_SIZE;
	}
	i2s_unilink->buffer_size = buf_size;

	if (of_property_read_u32(dev->of_node, "audio-mode", &mode)) {
		dev_info(dev, "sdrv_i2s_unilink use default audio-mode:(%d).\n",
			 mode);
	}

	i2s_unilink->server_addr.sin_family = PF_INET;
	i2s_unilink->server_addr.sin_port = htons(i2s_unilink->server_port);

	ret = sdrv_i2s_unilink_create_msg_handler(i2s_unilink);

	g_i2s_unilink_socket[mode] = i2s_unilink;
	mutex_init(&i2s_unilink->tx_lock); /* init lock */

	return ret;
}

static int sdrv_i2s_unilink_remove(struct platform_device *pdev)
{
	struct sdrv_i2s_unilink *i2s_unilink = platform_get_drvdata(pdev);

	if (i2s_unilink->msg_handler) {
		kthread_stop(i2s_unilink->msg_handler);
		i2s_unilink->msg_handler = NULL;
	}

	if (i2s_unilink->sock) {
		sock_release(i2s_unilink->sock);
		i2s_unilink->sock = NULL;
	}

	return 0;
}

static const struct of_device_id sdrv_i2s_unilink_dt_match[] = {
	{ .compatible = "semidrive,sdrv-i2s-unilink" },
	{},
};

#ifdef CONFIG_PM_SLEEP
static int sdrv_i2s_unilink_suspend(struct device *dev)
{
	return 0;
}

static int sdrv_i2s_unilink_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sdrv_i2s_unilink_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdrv_i2s_unilink_suspend,
				sdrv_i2s_unilink_resume)
};
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver sdrv_i2s_unilink_driver = {
	.probe = sdrv_i2s_unilink_probe,
	.remove = sdrv_i2s_unilink_remove,
	.driver = {
		   .name = "sdrv_i2s_unilink",
		   .of_match_table = sdrv_i2s_unilink_dt_match,
#ifdef CONFIG_PM_SLEEP
            .pm = &sdrv_i2s_unilink_pm_ops,
#endif
		   },
};

module_platform_driver(sdrv_i2s_unilink_driver);

MODULE_AUTHOR("Yuansong Jiao <yuansong.jiao@semidrive.com>");
MODULE_DESCRIPTION("X9 unilink i2s device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:x9 unilink i2s device");