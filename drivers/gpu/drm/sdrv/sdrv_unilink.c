/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include "sdrv_cast.h"

#define BUFFER_SIZE sizeof(struct disp_frame_info)

#ifdef CONFIG_SEMIDRIVE_ULINK
#define USE_PCIE_DMA 0
#else
#define USE_PCIE_DMA 1
#endif

#if !USE_PCIE_DMA
#include <linux/soc/semidrive/ulink.h>
#endif

#if USE_PCIE_DMA
struct kunlun_pcie {
    int pcie_index;
    int phy_refclk_sel;
    u32 pcie_cap;
    u32 ctrl_ncr_base;
    u32 phy_ncr_base;
    u32 phy_base;
    u32 dbi;
    u64 ecam_base;
    void __iomem *dma_base;
};
static struct kunlun_pcie pcie_ep;
#endif

#if USE_PCIE_DMA

#define PCIE_DMA_BASE 0x310c8000

#define  DMA_READ_ENGINE_EN (0X2C)
#define  DMA_READ_DOORBELL (0x30)
#define  DMA_READ_INT_STS (0Xa0)
#define  DMA_READ_INT_MASK (0Xa8)
#define  DMA_READ_INT_CLR (0Xac)

#define  DMA_CTRL1_RDCH (0x0)
#define  DMA_SIZE_RDCH (0x8)
#define  DMA_SRC_LOW_RDCH (0xc)
#define  DMA_SRC_HI_RDCH (0x10)
#define  DMA_DST_LOW_RDCH (0x14)
#define  DMA_DST_HI_RDCH (0x18)

#define  DMA_WRITE_ENGINE_EN (0XC)
#define  DMA_WRITE_DOORBELL (0x10)
#define  DMA_WRITE_INT_STS (0X4c)
#define  DMA_WRITE_INT_MASK (0X54)
#define  DMA_WRITE_INT_CLR (0X58)

#define  DMA_CTRL1_WRCH (0x0)
#define  DMA_SIZE_WRCH (0x8)
#define  DMA_SRC_LOW_WRCH (0xc)
#define  DMA_SRC_HI_WRCH (0x10)
#define  DMA_DST_LOW_WRCH (0x14)
#define  DMA_DST_HI_WRCH (0x18)

int pcie_dma_read(void *dma_base, int dma_channel, u64 dst, u64 src, u32 len)
{
    u32 val;
    int count = len/1024;
    void *dma_channel_base = dma_base + 0x300 + 0x200 * dma_channel;

    if (count < 100)
        count = 100;

    val = readl(dma_base + DMA_READ_ENGINE_EN);
    val |= 0x1;
    writel(val, dma_base + DMA_READ_ENGINE_EN);
    writel(0x8, dma_channel_base + DMA_CTRL1_RDCH);

    writel(lower_32_bits(src), dma_channel_base + DMA_SRC_LOW_RDCH);
    writel(upper_32_bits(src), dma_channel_base + DMA_SRC_HI_RDCH);

    writel(lower_32_bits(dst), dma_channel_base + DMA_DST_LOW_RDCH);
    writel(upper_32_bits(dst), dma_channel_base + DMA_DST_HI_RDCH);

    writel(len, dma_channel_base + DMA_SIZE_RDCH);
    writel(0, dma_base + DMA_READ_INT_MASK);

    writel(dma_channel, dma_base + DMA_READ_DOORBELL);

    while (count--) {
        val = readl(dma_base + DMA_READ_INT_STS);
        if (val & (1 << (dma_channel + 16))) {
            DRM_INFO("read abort\n");
            writel(1 << (dma_channel + 16), dma_base + DMA_READ_INT_CLR);
            return -1;
        }
        if (val & (1 << dma_channel)) {
            writel((1 << dma_channel), dma_base + DMA_READ_INT_CLR);
            return 0;
        }
        udelay(3);
    }
    if (count  <= 0) {
        DRM_INFO("read poll time out\n");
        return -2;
    }
    return 0;
}

int kunlun_pcie_dma_read(struct kunlun_pcie *kunlun_pcie,
               int dma_channel, u64 dst, u64 src, u32 len)
{
    if (kunlun_pcie->dma_base == 0)
        return -1;

    return pcie_dma_read((void *)kunlun_pcie->dma_base, dma_channel, dst, src, len);
}

int pcie_dma_write(void *dma_base, int dma_channel, u64 dst, u64 src, u64 len)
{
    u32 val;
    int count = len/1024;
    void *dma_channel_base = dma_base + 0x200 + 0x200 * dma_channel;

    if (count < 100)
        count = 100;

    val = readl(dma_base + DMA_WRITE_ENGINE_EN);
    val |= 0x1;
    writel(val, dma_base + DMA_WRITE_ENGINE_EN);
    writel(0x8, dma_channel_base + DMA_CTRL1_WRCH);

    writel(lower_32_bits(src), dma_channel_base + DMA_SRC_LOW_WRCH);
    writel(upper_32_bits(src), dma_channel_base + DMA_SRC_HI_WRCH);

    writel(lower_32_bits(dst), dma_channel_base + DMA_DST_LOW_WRCH);
    writel(upper_32_bits(dst), dma_channel_base + DMA_DST_HI_WRCH);

    writel(len, dma_channel_base + DMA_SIZE_WRCH);
    writel(0, dma_base + DMA_WRITE_INT_MASK);

    writel(dma_channel, dma_base + DMA_WRITE_DOORBELL);

    while (count--) {
        val = readl(dma_base + DMA_WRITE_INT_STS);
        if (val & (1 << (dma_channel + 16))) {
            DRM_INFO("write abort\n");
            writel(1 << (dma_channel + 16), dma_base + DMA_WRITE_INT_CLR);
            return -1;
        }
        if (val & (1 << dma_channel)) {
            writel(1 << (dma_channel), dma_base + DMA_WRITE_INT_CLR);
            return 0;
        }
        udelay(3);
    }
    if (count  <= 0) {
        DRM_INFO("write poll time out\n");
        return -2;
    }
    return 0;
}

int kunlun_pcie_dma_write(struct kunlun_pcie *kunlun_pcie,
               int dma_channel, u64 dst, u64 src, u32 len)
{
	int ret = 0;
    if (kunlun_pcie->dma_base == 0)
        return -1;

    ret = pcie_dma_write((void *)kunlun_pcie->dma_base, dma_channel, dst, src, len);
	return ret;
}

#endif

static int sdrv_socket_connect(struct sdcast_info *info)
{
	int ret = 0;
	// ip addr and port
	unsigned short port_num = info->producer_port;
	static struct sockaddr_in s_addr;

	if (info->socket_connect)
		return 0;

	if (info->sock != NULL) {
		DRM_ERROR("client: already connect sock!\n");
		goto connect_sock;
	}

	info->sock = (struct socket *)kmalloc(sizeof(struct socket), GFP_KERNEL);
	if (!info->sock) {
		DRM_ERROR("sock kmalloc filed \n");
		return -1;
	}
	// 创建一个
	ret = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP, &info->sock);
	if (ret < 0) {
		DRM_ERROR("client:socket create error!\n");
		if (info->sock) {
			kfree(info->sock);
			info->sock = NULL;
		}
		return ret;
	}
	DRM_INFO("client: socket create ok!\n");

	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = PF_INET;
	s_addr.sin_port = htons(port_num);
	s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//连接
connect_sock:
	ret = kernel_bind(info->sock, (struct sockaddr *)&s_addr, sizeof(s_addr));
	if (ret < 0) {
		DRM_ERROR("client: connect error! %d\n", ret);
		sock_release(info->sock);
		info->socket_connect = false;
		info->sock = NULL;
		return ret;
	}
	info->socket_connect = true;
	DRM_INFO("client: connect ok!\n");

	return ret;
}

extern int sdrv_g2d_dma_copy(dma_addr_t dst, dma_addr_t src, size_t data_size);

static int sdrv_unilink_sdcast_dma_copy(struct sdcast_info *info, dma_addr_t dst,
									dma_addr_t src, size_t data_size)
{
	int ret = 0;

	if (info->dpc_type == DPC_TYPE_VIDEO) {

		ret = sdrv_g2d_dma_copy(dst, src, data_size);
		if (ret) {
			DRM_ERROR("sdrv g2d dma copy failed\n");
			return ret;
		}

	} else if (info->dpc_type == DPC_TYPE_UNILINK) {

#if USE_PCIE_DMA
		kunlun_pcie_dma_write(&pcie_ep, 6, dst, src, data_size);
#else
		kunlun_pcie_dma_copy(src, dst, data_size, DIR_PUSH);
#endif

	} else {
		DRM_ERROR("dpc type invalid ,type = %d\n", info->dpc_type);
		return -1;
	}

	return ret;
}

static int sdrv_unilink_save_frameinfo(struct sdcast_info *info, struct disp_frame_info *fi)
{
	int ret = 0;
	unsigned int dma_len = 0;
	struct buffer_item *buf;
	unsigned long flags;

	if (fi == NULL) {
		DRM_ERROR("fi is null \n");
		return -1;
	}

	dma_len = fi->height * fi->pitch;
	DRM_DEBUG("dma_len = %d\n", dma_len);
	if (dma_len <= 0) {
		DRM_WARN("frame len is invaild, height = %d, pitch = %d\n", fi->height, fi->pitch);
		return -1;
	}

	if (info->cycle_buffer) {

		if (!info->streaming) {
			DRM_WARN("vcrtc sharing not started\n");
			return ret;
		}

		buf = NULL;
		spin_lock_irqsave(&info->lock, flags);
		if (!list_empty(&info->idle_list)) {
			buf = list_first_entry(&info->idle_list, struct buffer_item, list);
			list_del(&buf->list);
		}
		spin_unlock_irqrestore(&info->lock, flags);
		if (!buf) {
			/*
			* No idle buffer available, pop the first in the buffer list
			*/
			DRM_DEBUG("idle list is null, pop the first buffer list\n");
			spin_lock_irqsave(&info->lock, flags);
			if (!list_empty(&info->buffer_list)) {
				buf = list_first_entry(&info->buffer_list, struct buffer_item, list);
				list_del(&buf->list);
			}
			spin_unlock_irqrestore(&info->lock, flags);

			if (!buf) {
				DRM_ERROR("unilink emergency : please check buffer and idle list\n");
				return -1;
			}
		}
		memcpy(&buf->info, (void *)fi, BUFFER_SIZE);
		spin_lock_irqsave(&info->lock, flags);
		list_add_tail(&buf->list, &info->buffer_list);
		spin_unlock_irqrestore(&info->lock, flags);
	}

	return ret;
}

static int sdrv_unilink_socket_send(struct sdcast_info *info, struct disp_frame_info *fi)
{
	int ret = 0;
	int dma_len = 0;
	unsigned int size = BUFFER_SIZE;
	char *send_buf = NULL;
	struct kvec send_vec;
	struct msghdr send_msg;
	static struct sockaddr_in server_addr;
	struct buffer_item *buf;
	unsigned long flags;

	if (fi == NULL) {
		DRM_ERROR("fi is null \n");
		return -1;
	}
	/* kmalloc a send buffer*/
	send_buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	if (send_buf == NULL) {
		DRM_ERROR("client: send_buf kmalloc error!\n");
		return -1;
	}

	sdrv_socket_connect(info);

	if (!info->sock) {
		DRM_ERROR("sock is null, please init sock frist \n");
		ret = -1;
		goto fail_call;
	}

	memcpy(send_buf, (void *)fi, size);

	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(info->consumer_port);
	server_addr.sin_addr.s_addr = in_aton(info->consumer_ipv4);
	memset(&send_msg, 0, sizeof(send_msg));
	send_msg.msg_name = &server_addr;
	send_msg.msg_namelen = sizeof(server_addr);
	memset(&send_vec, 0, sizeof(send_vec));
	send_vec.iov_base = send_buf;
	send_vec.iov_len = size;

	if (!info->socket_connect) {
		ret = -1;
		goto fail_call;
	}

	dma_len = fi->height * fi->pitch;
	DRM_DEBUG("dma_len = %d\n", dma_len);
	if (dma_len > 0) {
		if (info->cycle_buffer) {
			buf = NULL;
			spin_lock_irqsave(&info->lock, flags);
			if (!list_empty(&info->buffer_list)) {
				buf = list_first_entry(&info->buffer_list, struct buffer_item, list);
				list_del(&buf->list);
			}
			spin_unlock_irqrestore(&info->lock, flags);

			if (!buf) {
				/*
				 * Maybe consumer is not started, so we don't complain here.
				 */
				DRM_INFO("Maybe consumer is not started, so we don't complain here.\n");
				goto fail_call;
			}

			DRM_DEBUG("copy from 0x%x to 0x%llx size %u\n", fi->addr_l, buf->info.addr_l, dma_len);

			ret = sdrv_unilink_sdcast_dma_copy(info, buf->info.addr_l, fi->addr_l, dma_len);
			if (ret)
				goto fail_call;

			((struct disp_frame_info *)send_buf)->addr_l = buf->info.addr_l;

			spin_lock_irqsave(&info->lock, flags);
			list_add_tail(&buf->list, &info->idle_list);
			spin_unlock_irqrestore(&info->lock, flags);
		} else {
#if USE_PCIE_DMA
			kunlun_pcie_dma_write(&pcie_ep, 1, 0x7b570000, fi->addr_l, dma_len);
#else
			kunlun_pcie_dma_copy(fi->addr_l, 0x7b570000, dma_len, DIR_PUSH);
#endif
		}
	}

	if (fi->cmd == DISP_CMD_SET_FRAMEINFO) {
		DRM_DEBUG("send buf 0x%x\n", ((struct disp_frame_info *)send_buf)->addr_l);
	}

	// send msg
	ret = kernel_sendmsg(info->sock, &send_msg, &send_vec, 1, size);
	if (ret < 0) {
		DRM_ERROR("client: kernel_sendmsg error! ret = %d \n", ret);
		ret = -1;
		goto fail_call;
	} else if(ret != size){
		DRM_WARN("client: ret!=BUFFER_SIZE");
	}
	if (fi->cmd == DISP_CMD_CLEAR) {
		DRM_INFO("DISP_CMD_CLEAR is send ok \n");
	}
    if (fi->cmd == DISP_CMD_START) {
        DRM_INFO("DISP_CMD_START is send ok \n");
    }

	DRM_DEBUG("client: send ok! size = %d , ret = %d, cmd = 0x%x\n", size, ret, fi->cmd);

fail_call:
	if (send_buf) {
		kfree(send_buf);
	}
	return ret;
}

static int sdrv_unilink_disp_ioctl(struct sdcast_info *info, u32 command,
								void *arg)
{
	int ret = 0;
	struct disp_frame_info *fi = (struct disp_frame_info *)arg;

	if (arg == NULL) {
		DRM_ERROR("arg == null  break disp ioctl \n");
		return -1;
	}

	fi->cmd = command;

	switch(command) {
	case DISP_CMD_SET_FRAMEINFO:
		ret = sdrv_unilink_save_frameinfo(info, fi);
		if (ret < 0) {
			DRM_ERROR("sdrv_unilink_save_frameinfo error \n");
			break;
		}
		break;
	case DISP_CMD_SHARING_WITH_MASK:
		DRM_WARN("client: DISP_CMD_SHARING_WITH_MASK");
		break;

	case DISP_CMD_START:
		DRM_WARN("client: DISP_CMD_START");
		ret = sdrv_socket_connect(info);
		if (ret != 0) {
			DRM_ERROR("sdrv socket connect filed \n");
			ret = -ENOTCONN;
			break;
		}
		ret = sdrv_unilink_socket_send(info, fi);
		if (ret < 0) {
			DRM_ERROR("sdrv_unilink_socket_send error \n");
			break;
		}
		break;
	case DISP_CMD_CLEAR:
		DRM_INFO("client: DISP_CMD_CLEAR");
		//todo send clear msg
		ret = sdrv_unilink_socket_send(info, fi);
		if (ret < 0) {
			DRM_ERROR("DISP CMD CLEAR socket msg send failed, ret < 0 \n");
			break;
		}
		//todo set socket shutdown flag true
		break;
	default:
		DRM_WARN("client: default");
		break;
	}

	return ret;
}

/* static struct task_struct *unilink_TaskStruct; */
int sdrv_unilink_rpcall_start(struct sdrv_dpc *dpc, bool enable)
{
	int ret;
	struct disp_frame_info fi;
	int cmd = enable? DISP_CMD_START: DISP_CMD_CLEAR;
	struct buffer_item *buf;
	unsigned long flags;
	struct dp_dummy_data *unilink = (struct dp_dummy_data *)dpc->priv;
	struct sdcast_info *info = unilink->cast_info;

	if (info->cycle_buffer && !info->first_start && enable) {
		/*
		 * default unilink's connector is connected, so system boot is called kunlun_crtc_atomic_enable.
		 * but user space is not start, so skip the system boot kunlun_crtc_atomic_enable....
		 */
		DRM_INFO("system boot should be skipped, first_start = %d, enable = %d\n",
			 info->first_start, enable);
		return 0;
	}

#if USE_PCIE_DMA
	if (enable) {
		pcie_ep.dma_base = ioremap_nocache(PCIE_DMA_BASE, 0x2000);
	}
#endif

	if (info->cycle_buffer) {
		spin_lock_irqsave(&info->lock, flags);
		info->streaming = enable;
		if (!info->streaming) {
			/*
			 * Need to delete save buffer
			 */
			while (!list_empty(&info->buffer_list)) {
				buf = list_first_entry(&info->buffer_list, struct buffer_item, list);
				list_del(&buf->list);
				list_add_tail(&buf->list, &info->idle_list);
			}
		}

		spin_unlock_irqrestore(&info->lock, flags);

	}

	DRM_INFO("unilink enable = %d, cmd = 0x%x \n", enable, cmd);
	memset((void*)&fi, 0, sizeof(fi));
	ret = sdrv_unilink_disp_ioctl(info, cmd, (void *)&fi);
	if (ret < 0) {
		PRINT("ioctl failed=%d\n", ret);
	} else {
		ret = 0;
	}

	return ret;
}

int sdrv_unilink_set_frameinfo(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count)
{
	struct disp_frame_info fi;
	struct dpc_layer *layer = NULL;
	bool is_fbdc_cps = false;
	int i;
	int ret;
	struct dp_dummy_data *unilink = (struct dp_dummy_data *)dpc->priv;
	struct sdcast_info *info = unilink->cast_info;
	static int boot_frame_cnt = 0;

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

	if (layer->nplanes > 1) {
		DRM_WARN("Currently only interleaved formats are supported, fmt[%c%c%c%c]\n",
		 layer->format & 0xff, (layer->format >> 8) & 0xff,
		 (layer->format >> 16) & 0xff,(layer->format >> 24) & 0xff);

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

	DRM_DEBUG("w:%d h:%d pitch:%d addrh:0x%x addrl:0x%x posx:%d posy:%d format:[%c%c%c%c] mk:%d\n",
		fi.width, fi.height, fi.pitch, fi.addr_h, fi.addr_l, fi.pos_x, fi.pos_y, layer->format & 0xff,
		(layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff,(layer->format >> 24) & 0xff, fi.mask_id);

	if (boot_frame_cnt < 4) {
		boot_frame_cnt++;
	}

	/*
	 * system boot kunlun_crtc_atomic_enable is skiped,
	 * So the first start of user space has to send ..
	 */
	if ((!info->first_start) && (boot_frame_cnt > 3)) {
		info->first_start = true;
		DRM_INFO("send unilink rpcall start in the usr space first start..\n");
		sdrv_unilink_rpcall_start(dpc, true);
	}

	ret = sdrv_unilink_disp_ioctl(info, DISP_CMD_SET_FRAMEINFO, (void *)&fi);
	if (ret < 0) {
		DRM_ERROR("ioctl failed=%d\n", ret);
	} else {
		ret = 0;
	}

	return ret;
}

static int sdrv_unilink_send_frameinfo(struct sdcast_info *info, struct disp_frame_info *fi)
{
	int ret = 0;
	struct kvec send_vec;
	struct msghdr send_msg;
	static struct sockaddr_in server_addr;

	if (fi == NULL) {
		DRM_ERROR("fi is null \n");
		return -1;
	}

	sdrv_socket_connect(info);

	if (!info->sock) {
		DRM_ERROR("sock is null, please init sock frist \n");
		return -1;
	}

	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(info->consumer_port);
	server_addr.sin_addr.s_addr = in_aton(info->consumer_ipv4);
	memset(&send_msg, 0, sizeof(send_msg));
	send_msg.msg_name = &server_addr;
	send_msg.msg_namelen = sizeof(server_addr);
	memset(&send_vec, 0, sizeof(send_vec));
	send_vec.iov_base = (void *)fi;
	send_vec.iov_len = BUFFER_SIZE;

	if (!info->socket_connect) {
		DRM_ERROR("socket not connected\n");
		return -1;
	}

	DRM_DEBUG("debug >>> fi addr = 0x%x\n", fi->addr_l);
	ret = kernel_sendmsg(info->sock, &send_msg, &send_vec, 1, BUFFER_SIZE);
	if (ret < 0) {
		DRM_ERROR("client: kernel_sendmsg error! ret = %d \n", ret);
		return -1;
	} else if (ret != BUFFER_SIZE) {
		DRM_WARN("client: ret != BUFFER_SIZE\n");
	}

	return 0;
}

static int sdrv_unilink_msg_handler(void * data)
{
	int ret = 0;
	struct msghdr msg;
	struct kvec iov;
	struct buffer_item *buf;
	unsigned long flags;
	int recvlen;
	struct disp_frame_info fi;
	uint32_t tmp_addr;
	unsigned int dma_len;
	struct sdrv_dpc *dpc = (struct sdrv_dpc *)data;
	struct dp_dummy_data *unilink = (struct dp_dummy_data *)dpc->priv;
	struct sdcast_info *info = unilink->cast_info;

	DRM_INFO("display sharing task enter\n");

	for (;;) {
		memset(&msg, 0, sizeof(msg));
		memset(&fi, 0, sizeof(fi));
		iov.iov_base = &fi;
		iov.iov_len = sizeof(fi);
		recvlen = kernel_recvmsg(info->sock, &msg, &iov, 1, iov.iov_len, 0);
		if (recvlen < 0) {
			DRM_ERROR("fail to recv msg: %d\n", recvlen);
			break;
		}

		DRM_DEBUG("recv buf 0x%x\n", fi.addr_l);

		buf = NULL;
		spin_lock_irqsave(&info->lock, flags);
		if (!list_empty(&info->buffer_list)) {
			buf = list_first_entry(&info->buffer_list, struct buffer_item, list);
		}
		spin_unlock_irqrestore(&info->lock, flags);

		if (!buf) {
			DRM_DEBUG("Maybe vcrtc is not started, so we don't complain here.\n");
			fi.cmd = DISP_CMD_SET_FRAMEINFO;
			fi.width = 1920;
			fi.height = 720;
			goto send_bak;
		}

		dma_len = buf->info.height * buf->info.pitch;

		DRM_DEBUG("debug >>> copy from 0x%x to 0x%llx size %u\n",
			buf->info.addr_l, fi.addr_l, dma_len);

		ret = sdrv_unilink_sdcast_dma_copy(info, fi.addr_l, buf->info.addr_l, dma_len);
		if (ret) {
			DRM_ERROR("unilink sdcast dma copy failed\n");
		}

		tmp_addr = fi.addr_l;
		memcpy((void *)&fi, &buf->info, BUFFER_SIZE);
		fi.addr_l = tmp_addr;

		spin_lock_irqsave(&info->lock, flags);
		if (!list_is_last(&buf->list, &info->buffer_list)) {
			list_del(&buf->list);
			list_add_tail(&buf->list, &info->idle_list);
		}
		spin_unlock_irqrestore(&info->lock, flags);

send_bak:
		ret = sdrv_unilink_send_frameinfo(info, &fi);
		if (ret) {
			DRM_WARN("unilink socket send frameinfo to vcam failed\n");
		}
	}

	DRM_INFO("display sharing task leave\n");

	return 0;
}

int sdrv_unilink_init_socket_info(struct sdrv_dpc *dpc, struct device_node *np)
{
	const char *ip;
	uint32_t port;
	int i;
	unsigned long flags;
	struct sdcast_info *info;
	struct dp_dummy_data *unilink = (struct dp_dummy_data *)dpc->priv;

	info = devm_kzalloc(&dpc->dev, sizeof(struct sdcast_info), GFP_KERNEL);
	if (!info) {
		DRM_ERROR("kalloc info failed\n");
		return -1;
	}

	unilink->cast_info = info;

	spin_lock_init(&info->lock);
	INIT_LIST_HEAD(&info->buffer_list);
	INIT_LIST_HEAD(&info->idle_list);

	if (!of_property_read_string(np, "consumer-ip", &ip)) {
		strncpy(info->consumer_ipv4, ip, IPv4_ADDR_LEN-1);
	} else {
		strncpy(info->consumer_ipv4, "172.20.2.32", IPv4_ADDR_LEN-1);
	}
	info->consumer_ipv4[IPv4_ADDR_LEN-1] = '\0';

	if (!of_property_read_u32(np, "consumer-port", &port)) {
		info->consumer_port = port;
	} else {
		info->consumer_port = 8888;
	}

	if (!of_property_read_u32(np, "producer-port", &port)) {
		info->producer_port = port;
	} else {
		info->producer_port = 8888;
	}

	info->cycle_buffer = !!of_find_property(np, "cycle-buffer", NULL);

	info->dpc_type = dpc->dpc_type;

	DRM_INFO("target %s:%d local port %d cycle buffer %d dpc_type %d\n",
			info->consumer_ipv4, info->consumer_port, info->producer_port, info->cycle_buffer, info->dpc_type);

	if (info->cycle_buffer) {
		sdrv_socket_connect(unilink->cast_info);

		if (!info->sock) {
			DRM_ERROR("sock is null, fallback to use fixed buffer\n");
			info->cycle_buffer = 0;
			return 0;
		}

		spin_lock_irqsave(&info->lock, flags);
		info->streaming = 0;
		for (i = 0; i < MAX_BUF_COUNT; i++) {
			INIT_LIST_HEAD(&info->raw_items[i].list);
			list_add_tail(&info->raw_items[i].list, &info->idle_list);
		}
		spin_unlock_irqrestore(&info->lock, flags);

		/* socket is never released so leave task running freely */
		kthread_run(sdrv_unilink_msg_handler, dpc, "disp-sharing-task");
	}

	return 0;
}

