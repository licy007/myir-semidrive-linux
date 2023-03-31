/*
 * sdrv-i2s-rpmsg.h
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

#ifndef __SDRV_I2S_RPMSG_H__
#define __SDRV_I2S_RPMSG_H__

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/soc/semidrive/ipcc.h>

#include "sdrv-remote-pcm.h"

#define SND_RPMSG_DEVICE_MAX 16

typedef struct {
	struct rpmsg_device *rpdev[SND_RPMSG_DEVICE_MAX];
	struct rpmsg_endpoint *endpoint[SND_RPMSG_DEVICE_MAX];
	struct i2s_remote_info *rpmsg_i2s_info[SND_RPMSG_DEVICE_MAX];
	uint32_t rpmsg_device_cnt;
	uint16_t stream_id_cnt;
} sdrv_i2s_rpmsg_dev_t;

#endif