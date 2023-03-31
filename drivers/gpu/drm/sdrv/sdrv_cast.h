/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#ifndef _sdrv_cast_H_
#define _sdrv_cast_H_

#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/init.h>
#include <linux/module.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>
#include "sdrv_dpc.h"
#include "sdrv_plane.h"
#include <linux/errno.h>
#include <linux/kallsyms.h>

#define IPv4_ADDR_LEN 16
#define MAX_BUF_COUNT 8

// part of display module
struct sdrv_disp_control {
	struct drm_crtc *crtc;

};

struct disp_frame_info {
	u32 addr_l;
	u32 addr_h;
	u32 format;
	u16 width;
	u16 height;
	u16 pos_x;
	u16 pos_y;
	u16 pitch;
	u16 mask_id;
	u32 cmd;
} __attribute__ ((packed));

/* Do not exceed 32 bytes so far */
struct disp_ioctl_cmd {
	u32 op;
	union {
		u8 data[28];
		struct disp_frame_info fi;
	} u;
};

struct disp_ioctl_ret {
	u32 op;
	union {
		u8 data[8];
	} u;
};

struct buffer_item {
	struct list_head list;
	struct disp_frame_info info;
};

struct disp_recovery_info {
	u16 rec_id;
	char data[22];
} __attribute__ ((packed));

struct sdcast_info {
	struct socket *sock;
	bool socket_connect;
	char consumer_ipv4[IPv4_ADDR_LEN];
	short consumer_port;
	short producer_port;
	int cycle_buffer;
	spinlock_t lock;
	int streaming;
	struct list_head buffer_list;
	struct list_head idle_list;
	struct buffer_item raw_items[MAX_BUF_COUNT];
	int first_start;
	int dpc_type;
};

#define DISP_CMD_START					_IOWR('d', 0, struct disp_frame_info)
#define DISP_CMD_SET_FRAMEINFO			_IOWR('d', 1, struct disp_frame_info)
#define DISP_CMD_SHARING_WITH_MASK		_IOWR('d', 2, struct disp_frame_info)
#define DISP_CMD_CLEAR					_IOWR('d', 3, struct disp_frame_info)
#define DISP_CMD_RECVMSG				_IOWR('d', 4, struct disp_frame_info)
#define DISP_CMD_DCRECOVERY				_IOWR('d', 5, struct disp_recovery_info)

#endif //_sdrv_unilink_H_
