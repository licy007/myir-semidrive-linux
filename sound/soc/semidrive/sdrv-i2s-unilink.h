/*
 * sdrv-i2s-unilink.h
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
#ifndef __SDRV_I2S_UNILINK_H__
#define __SDRV_I2S_UNILINK_H__

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

#include "sdrv-remote-pcm.h"

#define SND_UNILINK_DEVICE_MAX 8

struct sdrv_i2s_unilink {
	struct mutex tx_lock;
	struct device *dev;
	struct i2s_remote_info *unilink_i2s_info[SND_UNILINK_DEVICE_MAX];

	int server_port;
	int local_port;
	struct sockaddr_in server_addr;
	struct socket *sock;
	struct task_struct *msg_handler;

	uint32_t buffer_size;
	uint64_t local_buffer_addr
		[2][SND_UNILINK_DEVICE_MAX]; /* playback & capture */
	uint64_t remote_buffer_addr[2][SND_UNILINK_DEVICE_MAX];
};

int sdrv_i2s_unilink_send_message(struct sdrv_i2s_unilink *i2s_unilink,
				  uint8_t *buffer, uint16_t len);

#endif