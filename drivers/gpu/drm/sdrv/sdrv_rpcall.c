/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>
#include "sdrv_dpc.h"
#include "sdrv_plane.h"
#define SYS_PROP_DC_STATUS			(4)

#define FILL_IOC_DATA(ctl, arg, size) \
	memcpy(&ctl->u.data[0], arg, size)

static int sdrv_disp_ioctl(struct sdrv_disp_control *disp, u32 command,
								void *arg)
{
	struct rpc_ret_msg result = {0};
	struct rpc_req_msg request;
	struct disp_ioctl_cmd *ctl = DCF_RPC_PARAM(request, struct disp_ioctl_cmd);
	unsigned int size = _IOC_SIZE(command);
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_DC_IOCTL);
	ctl->op = command;
	switch(command) {
	case DISP_CMD_SET_FRAMEINFO:
		if (size != sizeof(struct disp_frame_info)) {
			ret = -EINVAL;
			goto fail_call;
		}

		FILL_IOC_DATA(ctl, arg, size);

		break;
	case DISP_CMD_SHARING_WITH_MASK:
		if (size != sizeof(struct disp_frame_info)) {
			ret = -EINVAL;
			goto fail_call;
		}

		FILL_IOC_DATA(ctl, arg, size);
		break;

	case DISP_CMD_START:
		PRINT("got cmd DISP_CMD_START ioctl\n");
		break;
	case DISP_CMD_CLEAR:
		PRINT("got cmd DISP_CMD_CLEAR ioctl\n");
		break;
	case DISP_CMD_DCRECOVERY:
		if (size != sizeof(struct disp_recovery_info)) {
			ret = -EINVAL;
			goto fail_call;
		}

		FILL_IOC_DATA(ctl, arg, size);
		break;
	default:
		break;
	}


	ret = semidrive_rpcall(&request, &result);
	if (ret < 0 || result.retcode < 0) {
		goto fail_call;
	}

	return 0;

fail_call:

	return ret;
}

int sdrv_rpcall_dcrecovery(struct kunlun_crtc *kcrtc)
{
	int ret;
	struct sdrv_disp_control disp;
	struct disp_recovery_info rec_info;
	static u16 rec_id = 0;
	int cmd = DISP_CMD_DCRECOVERY;

	const char *slave_name = NULL;
	const char *master_name = NULL;

	memset(&rec_info, 0, sizeof(struct disp_recovery_info));

	master_name = dev_name(&kcrtc->master->dev);

	rec_info.rec_id = rec_id;

	if (kcrtc->slave) {
		slave_name = dev_name(&kcrtc->slave->dev);
		sprintf(rec_info.data, "%s %s", master_name, slave_name);
	}
	else
		sprintf(rec_info.data, "%s", master_name);

	pr_info("%s :%s recid:%d\n", __func__, rec_info.data, rec_id);
	ret = sdrv_disp_ioctl(&disp, cmd, (void*)&rec_info);
	if (ret < 0) {
		PRINT("ioctl failed=%d\n", ret);
	}

	rec_id++;
	if (rec_id >= 0xFFFE)
		rec_id = 0;

	return ret;
}

int sdrv_rpcall_start(bool enable) {
	int ret;
	struct sdrv_disp_control disp;
	int cmd = enable? DISP_CMD_START: DISP_CMD_CLEAR;

	ret = sdrv_disp_ioctl(&disp, cmd, (void*)NULL);
	if (ret < 0) {
		PRINT("ioctl failed=%d\n", ret);
	}
	DRM_INFO("sdrv_rpcall_start %d\n", enable);
	return ret;
}

int sdrv_set_frameinfo(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count)
{
	struct sdrv_disp_control disp;
	struct disp_frame_info fi;
	struct dpc_layer *layer = NULL;
	bool is_fbdc_cps = false;
	int i;
	int ret;

	if (count == 0)
		return 0;
	if (count > 1){
		DRM_ERROR("rpcall display only support single layer, input %d layers", count);
		return -EINVAL;
	}

	for (i = 0; i < dpc->num_pipe; i++) {
		layer = &dpc->pipes[i]->layer_data;

		if (!layer->enable)
			continue;

		if (layer->ctx.is_fbdc_cps)
			is_fbdc_cps = true;
	}

	if (is_fbdc_cps) {
		DRM_ERROR("fbdc layer cannot support rpcall display");
		return -EINVAL;
	}

	if (!layer) {
		DRM_ERROR("layer is null pointer");
		return -1;
	}

	if (layer->src_w <= 0 || layer->src_h <= 0) {
		DRM_WARN("layer input parameter illegal, w:%d, h%d\n", layer->src_w, layer->src_h);
		return 0;
	}

	if (layer->format == DRM_FORMAT_XRGB8888) {
		DRM_INFO("%s skip layer, format[%c%c%c%c]\n", __func__,
			layer->format & 0xff, (layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff,(layer->format >> 24) & 0xff);
		return 0;
	}

	fi.width = layer->src_w;
	fi.height = layer->src_h;
	fi.pitch = layer->pitch[0];
	fi.addr_h = layer->addr_h[0];
	fi.addr_l = layer->addr_l[0];
	fi.pos_x = layer->dst_x;
	fi.pos_y = layer->dst_y;
	fi.format = layer->format;
	fi.mask_id = 0;

	DRM_DEBUG("format[%c%c%c%c]\n",
		layer->format & 0xff, (layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff,(layer->format >> 24) & 0xff);
	DRM_DEBUG("w:%d h:%d pitch:%d addrh:0x%x addrl:0x%x posx:%d posy:%d format:%x mk:%d\n",
		fi.width, fi.height, fi.pitch, fi.addr_h, fi.addr_l, fi.pos_x, fi.pos_y, fi.format, fi.mask_id);

	ret = sdrv_disp_ioctl(&disp, DISP_CMD_SHARING_WITH_MASK, (void*)&fi);
	if (ret < 0) {
		PRINT("ioctl failed=%d\n", ret);
	}

	return ret;
}

