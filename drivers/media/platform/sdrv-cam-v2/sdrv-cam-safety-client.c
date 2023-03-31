/*
 * sdrv-port-client.c
 *
 * Semidrive platform v4l2 core file
 *
 * Copyright (C) 2021, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/kthread.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-rect.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-sg.h>
#include <media/videobuf2-dma-contig.h>

#include <linux/mailbox_client.h>

#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/net.h>
#include <net/net_namespace.h>
#include <uapi/linux/in.h>
#include "cam_isr_log.h"

#define SDRV_DUMMY_NAME "sdrv-dummy"

#define CLIENT_PORT 17218

#define FPS_DEBUG 0

#define MBOX_RPROC_AP1	3
#define MBOX_RPROC_AP2	4

/* Let these definition compatiable with another OS side */
enum scs_op_type {
	SCS_OP_DEV_OPEN = 0,
	SCS_OP_ENUM_FORMAT = 1,
	SCS_OP_ENUM_FRAMESIZE = 2,
	SCS_OP_GET_FORMAT = 3,
	SCS_OP_SET_FORMAT = 4,
	SCS_OP_QUEUE_SETUP = 5,
	SCS_OP_GET_BUFINFO = 6,
	SCS_OP_SET_BUFINFO = 7,
	SCS_OP_QBUF = 8,
	SCS_OP_STREAM_ON = 9,
	SCS_OP_DQBUF = 10,
	SCS_OP_STREAM_OFF = 11,
	SCS_OP_DEV_CLOSE = 12,
	SCS_OP_ENUM_FRAMEINTERVALS = 13,
};

/* Do not exceed 16 bytes so far */
struct scs_ioctl_cmd {
    u16 op;
    u16 instance;
    union {
        struct {
            u16 index;
            u32 fmt;
        } fmt;
        struct {
            u16 type;
            u16 width;
            u16 height;
            u32 pixelformat;
        } s_fmt;
        struct {
            u16 type;
            u16 width;
            u16 height;
            u16 index;
        } fsz;
        struct {
			u16 type;
			u16 index;
		} fitv;
		struct {
            u8 num_bufs;
            u8 num_planes;
            u8 m_type;
            uint32_t buf_size;
        } queue;
        struct {
            u32 index;
            u32 addr[2];
        } s_bufinfo;
        u8 data[12];
    } msg;
};


/* Do not exceed 22 bytes so far */
struct scs_ioctl_result {
    u16 op;
    u16 instance;
    union {
        /** used for get_version */
        struct {
            u16 index;
            u32 fmt;
        } fmt;
        /** used for get_config */
        struct {
            u16 type;
            u16 width;
            u16 height;
            u16 index;
        } fsz;
        struct {
            u16 type;
            u16 width;
            u16 height;
            u32 pixelformat;
        } g_fmt;
		struct {
			u16 type;
			u16 index;
			u32 numerator;
			u32 denominator;
		} fitv;
        struct {
            u32 index;
            u32 timestamp;
            u32 timestamp_h;
			u32 addr[2];
        } g_bufinfo;

        struct {
            u8 bufs;
            u8 planes;
            u32 size[2];
        } queue;
        u8 data[18];
    } msg;
};


struct csi_dummy {
	struct device *dev;
	u16 instance;
	char type[16];

	struct mutex lock;
	struct mutex q_lock;
	spinlock_t buf_lock;

	struct mbox_client client;
	struct mbox_chan *mbox;

	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct v4l2_async_notifier notifier;
	struct v4l2_fwnode_endpoint vep;

	struct v4l2_format active_fmt;

	struct video_device vdev;
	struct vb2_queue queue;
	struct list_head buf_list;

	unsigned int mbus_type;

	struct list_head kstreams;
	unsigned int kstream_nums;

	uint32_t fcnt;
	union {
		uint64_t data64;
		uint8_t data8[8];
	} ts;

	bool use_socket;
#define IPv4_ADDR_LEN 16
	char server_ip[IPv4_ADDR_LEN];
	int server_port;
	struct sockaddr_in server_addr;
	struct socket *sock;
	struct task_struct *msg_handler;
	int pending_exit;
	struct completion arrived;
	struct scs_ioctl_result arrived_msg;
	/* for suspend and resume */
	struct file *file; /* for close when suspend,open when resume */
	atomic_t at_suspend, at_stream_off; /* close when suspend,stream off both set */
	atomic_t at_user;	/* for open after resume */
	u16 rproc;	/* scs_rpmsg_client_enum_t  */
};

struct sdrv_dummy_buffer {
	struct vb2_v4l2_buffer vbuf;
	dma_addr_t paddr[3];
	struct list_head queue;
};

#if FPS_DEBUG
static int _fps_fn = 0;
#endif

static int sdrv_dummy_video_open(struct file *file);
static int sdrv_dummy_video_release(struct file *file);
static int sdrv_dummy_video_s_fmt(struct file *file, void *fh, struct v4l2_format *f);

static int sdrv_dummy_send_message(struct csi_dummy *csi,
		struct scs_ioctl_cmd *ctl, struct scs_ioctl_result *rs)
{
	struct msghdr msg;
    struct kvec iov;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &csi->server_addr;
	msg.msg_namelen = sizeof(csi->server_addr);
	iov.iov_base = ctl;
	iov.iov_len = sizeof(*ctl);
	ret = kernel_sendmsg(csi->sock, &msg, &iov, 1, iov.iov_len);
	if (ret < 0) {
		dev_err(csi->dev, "fail to send, ret %d\n", ret);
		return ret;
	}

	wait_for_completion(&csi->arrived);
	memcpy(rs, &csi->arrived_msg, sizeof(*rs));

	/*
	 * Return code for SCS_OP_DEV_OPEN is in scs_ioctl_result.
	 */
	if (ctl->op == SCS_OP_DEV_OPEN) {
		ret = (int8_t)rs->msg.data[0];
	} else {
		ret = 0;
	}

	return ret;
}

static int sdrv_dummy_ioctl(struct csi_dummy *csi, struct scs_ioctl_cmd *c,
			    struct scs_ioctl_result *data)
{
	struct rpc_ret_msg result = { };
	struct rpc_req_msg request;
	struct scs_ioctl_cmd *ctl =
	    DCF_RPC_PARAM(request, struct scs_ioctl_cmd);
	struct scs_ioctl_result *rs =
	    (struct scs_ioctl_result *)&result.result[0];
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_SCS_IOCTL);
	//ctl->op = command;
	memcpy(ctl, c, sizeof(*c));
	ctl->instance = ((csi->rproc << 8) & 0xff00) | (csi->instance & 0x00ff);


	dev_dbg(csi->dev, "%s(): instance=%d, op=%d\n", __func__, ctl->instance, ctl->op);
	if (csi->use_socket) {
		ctl->instance = csi->instance;

		/*
		 * Those pointers being valid depends on rpmsg design - it's ugly for
		 * us to implement socket call like this, although it somehow works.
		 */
		ret = sdrv_dummy_send_message(csi, ctl, rs);
	} else {
	ret = semidrive_rpcall(&request, &result);
	}
	dev_dbg(csi->dev, "%s(): ret=%d, result.retcode=%d\n", __func__, ret, result.retcode);
	if ((ret < 0) || (result.retcode != 0)) {
		ret |= result.retcode;
		ret = (ret > 0) ? -ret : ret;
		dev_err(csi->dev, "rpcall failed=%d %d\n", ret, result.retcode);
		goto fail_call;
	}

	memcpy(data, rs, sizeof(*rs));

fail_call:

	return ret;
}

/* Function: Need open file again after resume
 * Return: 0: success, other: fail
 */
static inline int sdrv_dummy_check_user(struct csi_dummy *csi)
{
	int ret;

	if (atomic_read(&csi->at_user) > 0)
		return 0;
	ret = -1;
	if (csi->file) {
		ret = sdrv_dummy_video_open(csi->file);
		ret |= sdrv_dummy_video_s_fmt(csi->file, NULL, &csi->active_fmt);
	}

	return ret;
}

static int sdrv_dummy_queue_setup(struct vb2_queue *q,
				  unsigned int *num_buffers,
				  unsigned int *num_planes,
				  unsigned int sizes[],
				  struct device *alloc_devs[])
{
	int ret;
	unsigned int i;
	struct csi_dummy *csi = vb2_get_drv_priv(q);
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	struct v4l2_pix_format_mplane *format = &csi->active_fmt.fmt.pix_mp;

	ret = sdrv_dummy_check_user(csi);
	if (ret) {
		pr_err("sdrv-dummy:fail,not open %d, %d\n", __LINE__, ret);
		return ret;
	}
	op_ctl.op = SCS_OP_QUEUE_SETUP;
	op_ctl.msg.queue.num_bufs = (u8)*num_buffers;
	op_ctl.msg.queue.num_planes = (u8)*num_planes;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "queue setup failed: %d\n", ret);
		return ret;
	}
	*num_buffers = (unsigned int)op_ret.msg.queue.bufs;
	*num_planes = format->num_planes = (unsigned int)op_ret.msg.queue.planes;
	for (i = 0; i < *num_planes; i++)
		sizes[i] = format->plane_fmt[i].sizeimage = op_ret.msg.queue.size[i];

	dev_info(csi->dev, "*num_buffers=%d, *num_planes=%d, sizes=(%d %d %d)\n",
		*num_buffers, *num_planes, sizes[0], sizes[1], sizes[2]);

	dev_info(csi->dev, "queue setup ok: %d\n", ret);

	return 0;
}

static int sdrv_dummy_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct sdrv_dummy_buffer *kbuffer =
	    container_of(vbuf, struct sdrv_dummy_buffer, vbuf);

	unsigned int i;

	struct csi_dummy *csi = vb2_get_drv_priv(vb->vb2_queue);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	const struct v4l2_pix_format_mplane *format =
	    &csi->active_fmt.fmt.pix_mp;

	ret = sdrv_dummy_check_user(csi);
	if (ret) {
		pr_err("sdrv-dummy:fail,not open %d, %d\n", __LINE__, ret);
		return ret;
	}

	for (i = 0; i < format->num_planes; i++) {
		kbuffer->paddr[i] = vb2_dma_contig_plane_dma_addr(vb, i);
		dev_info(csi->dev, "%s(): kbuffer->paddr[%d]=0x%x\n", __func__, i,
		       (unsigned int)kbuffer->paddr[i]);
		if (!kbuffer->paddr[i])
			return -EFAULT;
	}

	op_ctl.op = SCS_OP_SET_BUFINFO;
	dev_info(csi->dev, "vb->index = %d\n", vb->index);
	op_ctl.msg.s_bufinfo.index = vb->index;
	op_ctl.msg.s_bufinfo.addr[0] = (u32)kbuffer->paddr[0];
	op_ctl.msg.s_bufinfo.addr[1] = (u32)kbuffer->paddr[1];

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "get_format failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev, "set_bufinfo ok: %d\n", ret);

	return 0;
}

static int sdrv_dummy_buf_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static void sdrv_dummy_buf_queue(struct vb2_buffer *vb)
{
	unsigned long flags;
	int ret;
	struct csi_dummy *csi = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct sdrv_dummy_buffer *kbuffer = container_of(vbuf,
							 struct
							 sdrv_dummy_buffer,
							 vbuf);
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	dev_dbg(csi->dev, "%s(): csi->buf_list add vb->index=%d\n", __func__,
		vb->index);

	op_ctl.op = SCS_OP_QBUF;
	op_ctl.msg.s_bufinfo.index = vb->index;

	/* add to buffer queue first as dequeue operation is asynchronous */
	spin_lock_irqsave(&csi->buf_lock, flags);
	list_add_tail(&kbuffer->queue, &csi->buf_list);
	spin_unlock_irqrestore(&csi->buf_lock, flags);

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "qbuf failed: %d\n", ret);
		return;
	}
	dev_dbg(csi->dev, "qbuf ok: %d, op_ret.msg.g_bufinfo.index=%d\n", ret,
		op_ret.msg.g_bufinfo.index);
}

static int sdrv_dummy_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct csi_dummy *csi = vb2_get_drv_priv(vq);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	ret = sdrv_dummy_check_user(csi);
	if (ret) {
		pr_err("sdrv-dummy:fail,not open %d, %d\n", __LINE__, ret);
		return ret;
	}

	csi->fcnt = 0;
	op_ctl.op = SCS_OP_STREAM_ON;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "stream on failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev, "stream on ok: %d\n", ret);
	camera_heartbeat_clear();
	atomic_set(&csi->at_suspend, 0);

	return 0;
}

static void sdrv_dummy_stop_streaming(struct vb2_queue *vq)
{
	struct csi_dummy *csi = vb2_get_drv_priv(vq);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	struct sdrv_dummy_buffer *kbuf, *node;
	unsigned long flags;

	op_ctl.op = SCS_OP_STREAM_OFF;
	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "stream off failed: %d\n", ret);
		return;
	}

	spin_lock_irqsave(&csi->buf_lock, flags);

	list_for_each_entry_safe(kbuf, node, &csi->buf_list, queue) {
		dev_info(csi->dev, "list: kbuf->vbuf.vb2_buf.index=%d\n",
			kbuf->vbuf.vb2_buf.index);
		vb2_buffer_done(&kbuf->vbuf.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del(&kbuf->queue);
	}

	spin_unlock_irqrestore(&csi->buf_lock, flags);

	atomic_set(&csi->at_stream_off, 1);
	if (atomic_read(&csi->at_suspend) && atomic_read(&csi->at_stream_off))
		ret = sdrv_dummy_video_release(csi->file);

	pr_info("sdrv-dummy,stream off,ret:%d\n", ret);
}

static const struct vb2_ops sdrv_dummy_video_vb2_q_ops = {
	.queue_setup = sdrv_dummy_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_init = sdrv_dummy_buf_init,
	.buf_prepare = sdrv_dummy_buf_prepare,
	.buf_queue = sdrv_dummy_buf_queue,
	.start_streaming = sdrv_dummy_start_streaming,
	.stop_streaming = sdrv_dummy_stop_streaming,
};

static int sdrv_dummy_video_open(struct file *file)
{
	struct csi_dummy *csi = video_drvdata(file);
	struct v4l2_fh *vfh;
	int ret = 0;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	mutex_lock(&csi->lock);

	vfh = kzalloc(sizeof(*vfh), GFP_KERNEL);
	if (!vfh) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	v4l2_fh_init(vfh, &csi->vdev);
	v4l2_fh_add(vfh);

	file->private_data = vfh;

	mutex_unlock(&csi->lock);

	atomic_inc(&csi->at_user);

	if (atomic_read(&csi->at_user) > 1) {
		pr_err("sdrv-dummy:have opened\n");
		return 0;
	}

	op_ctl.op = SCS_OP_DEV_OPEN;
	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		atomic_dec(&csi->at_user);
		dev_err(csi->dev, "dev_open failed: %d\n", ret);
		goto err;
	}
	atomic_set(&csi->at_suspend, 0);
	atomic_set(&csi->at_stream_off, 0);
	csi->file = file;
	dev_info(csi->dev, "dev_open ok,ret:%d,user:%d\n",
				ret, atomic_read(&csi->at_user));

	return 0;

err:
	v4l2_fh_release(file);
	return ret;
err_alloc:
	mutex_unlock(&csi->lock);

	return ret;
}

static int sdrv_dummy_video_release(struct file *file)
{
	struct csi_dummy *csi = video_drvdata(file);
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	int ret = 0;

	if (atomic_read(&csi->at_user) == 0) {
		pr_err("sdrv-dummy:have closed");
		return -1;
	}

	atomic_dec(&csi->at_user);

	if (atomic_read(&csi->at_user) == 0) {
		op_ctl.op = SCS_OP_DEV_CLOSE;
		ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
		if (ret < 0) {
			atomic_inc(&csi->at_user);
			dev_err(csi->dev, "dev_close failed: %d\n", ret);
			goto err;
		}

		pr_info("dev_close ok,ret:%d,user:%d\n",
			ret, atomic_read(&csi->at_user));
		atomic_set(&csi->at_suspend, 0);
		atomic_set(&csi->at_stream_off, 0);
	}

	vb2_fop_release(file);
	file->private_data = NULL;

err:
	return ret;
}

static const struct v4l2_file_operations sdrv_dummy_vid_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = sdrv_dummy_video_open,
	.release = sdrv_dummy_video_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.read = vb2_fop_read,
};

static int sdrv_dummy_video_querycap(struct file *file, void *fh,
				     struct v4l2_capability *cap)
{
	struct csi_dummy *csi = video_drvdata(file);

	strlcpy(cap->driver, SDRV_DUMMY_NAME, sizeof(cap->driver));
	strlcpy(cap->card, SDRV_DUMMY_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(csi->dev));

	return 0;
}

struct sdrv_dummy_pix_format {
	u32 pixfmt;
	u32 mbus_code;
};

struct sdrv_dummy_pix_format pixfmts[] = {
	{
	 .pixfmt = V4L2_PIX_FMT_YUYV,	/* YUYV */
	 .mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
	 },
	{
	 .pixfmt = V4L2_PIX_FMT_UYVY,	/* UYVY */
	 .mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
	 },
};

static int sdrv_dummy_video_enum_fmt(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	struct csi_dummy *csi = video_drvdata(file);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	op_ctl.op = SCS_OP_ENUM_FORMAT;
	op_ctl.msg.fmt.index = f->index;
	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "enum_format done: %d\n", ret);
		return -EINVAL;
	}
	dev_info(csi->dev, "enum_format ok: %d, op_ret.msg.fmt.fmt=0x%x\n", ret,
		op_ret.msg.fmt.fmt);

	if (f->index <= op_ret.msg.fmt.index)
		f->pixelformat = op_ret.msg.fmt.fmt;
	else
		return -EINVAL;

	return 0;
}

static int sdrv_dummy_video_g_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct csi_dummy *csi = video_drvdata(file);

	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	struct v4l2_format *af = &csi->active_fmt;

	#if 0
	u32 pad;
	int ret = 0;
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	#endif

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	if (af->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	    af->fmt.pix_mp.width &&
	    af->fmt.pix_mp.height && af->fmt.pix_mp.num_planes) {
		*f = *af;
		return 0;
	}

	op_ctl.op = SCS_OP_GET_FORMAT;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "get_format failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev,
		"get_format ok: %d, op_ret.msg.g_fmt.type=%d, op_ret.msg.g_fmt.pixelformat=0x%x\n",
		ret, op_ret.msg.g_fmt.type, op_ret.msg.g_fmt.pixelformat);

	af->type = op_ret.msg.g_fmt.type;
	if (af->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		af->fmt.pix_mp.width = op_ret.msg.g_fmt.width;
		af->fmt.pix_mp.height = op_ret.msg.g_fmt.height;
		af->fmt.pix_mp.pixelformat = op_ret.msg.g_fmt.pixelformat;
	}

	*f = *af;

	return 0;
}

static int sdrv_dummy_video_s_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct csi_dummy *csi = video_drvdata(file);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	op_ctl.op = SCS_OP_SET_FORMAT;

	op_ctl.msg.s_fmt.type = f->type;
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		op_ctl.msg.s_fmt.width = f->fmt.pix_mp.width;
		op_ctl.msg.s_fmt.height = f->fmt.pix_mp.height;
		op_ctl.msg.s_fmt.pixelformat = f->fmt.pix_mp.pixelformat;
	}
	dev_info(csi->dev, "set_format: (%dx%d), type %d, fmt 0x%08x\n",
		op_ctl.msg.s_fmt.width, op_ctl.msg.s_fmt.height,
		op_ctl.msg.s_fmt.type, op_ctl.msg.s_fmt.pixelformat);

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "set_format failed: %d\n", ret);
		return ret;
	}
	if (op_ret.msg.g_fmt.pixelformat != f->fmt.pix_mp.pixelformat) {
		dev_err(csi->dev, "set_format: 0x%x failed!\n",
			f->fmt.pix_mp.pixelformat);
		return -1;
	}
	dev_info(csi->dev, "set_format ok: %d, op_ret.msg.gs_fmt.size=%d %d\n",
		ret, op_ret.msg.g_fmt.width, op_ret.msg.g_fmt.height);

	csi->active_fmt = *f;

	return 0;
}

static int sdrv_dummy_video_try_fmt(struct file *file,
				    void *fh, struct v4l2_format *f)
{
	struct csi_dummy *csi = video_drvdata(file);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	op_ctl.op = SCS_OP_SET_FORMAT;

	op_ctl.msg.s_fmt.type = f->type;
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		op_ctl.msg.s_fmt.width = f->fmt.pix_mp.width;
		op_ctl.msg.s_fmt.height = f->fmt.pix_mp.height;
		op_ctl.msg.s_fmt.pixelformat = f->fmt.pix_mp.pixelformat;
	}
	dev_info(csi->dev, "try_format: (%dx%d), type %d, fmt 0x%08x\n",
		op_ctl.msg.s_fmt.width, op_ctl.msg.s_fmt.height,
		op_ctl.msg.s_fmt.type, op_ctl.msg.s_fmt.pixelformat);

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "try_format failed: %d\n", ret);
		return ret;
	}
	if (op_ret.msg.g_fmt.pixelformat != f->fmt.pix_mp.pixelformat) {
		dev_err(csi->dev, "try_format: 0x%x failed!\n",
			f->fmt.pix_mp.pixelformat);
		return -1;
	}
	dev_info(csi->dev, "try_format ok: %d, op_ret.msg.gs_fmt.width=%d %d\n",
		ret, op_ret.msg.g_fmt.width, op_ret.msg.g_fmt.height);

	csi->active_fmt = *f;

	return 0;
}

static int sdrv_dummy_g_selection(struct file *file, void *fh,
				  struct v4l2_selection *s)
{
	return 0;
}

static int sdrv_dummy_s_selection(struct file *file, void *fh,
				  struct v4l2_selection *s)
{
	return 0;
}

static int sdrv_dummy_cropcap(struct file *file, void *fh,
			      struct v4l2_cropcap *crop)
{
	return 0;
}

static int sdrv_dummy_enum_input(struct file *file, void *fh,
				 struct v4l2_input *i)
{
	return 0;
}

static int sdrv_dummy_g_input(struct file *file, void *fh, unsigned int *i)
{
	return 0;
}

static int sdrv_dummy_s_input(struct file *file, void *fh, unsigned int i)
{
	return 0;
}

static int sdrv_dummy_querystd(struct file *file, void *fh, v4l2_std_id *a)
{
	return 0;
}

static int sdrv_dummy_g_std(struct file *file, void *fh, v4l2_std_id *a)
{
	return 0;
}

static int sdrv_dummy_s_std(struct file *file, void *fh, v4l2_std_id a)
{
	return 0;
}

static int sdrv_dummy_g_parm(struct file *file, void *fh,
			     struct v4l2_streamparm *a)
{
	return 0;
}

static int sdrv_dummy_s_parm(struct file *file, void *fh,
			     struct v4l2_streamparm *a)
{
	return 0;
}

static int sdrv_dummy_enum_framesizes(struct file *file, void *fh,
				      struct v4l2_frmsizeenum *fsize)
{
	struct csi_dummy *csi = video_drvdata(file);

	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	op_ctl.op = SCS_OP_ENUM_FRAMESIZE;
	op_ctl.msg.fsz.index = fsize->index;
	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "enum_framesize done: %d\n", ret);
		return -EINVAL;
	}
	dev_info(csi->dev, "enum_framesize ok: %d, op_ret.msg.fsz (%d, %d-%d)\n",
		ret, op_ret.msg.fsz.type, op_ret.msg.fsz.width,
		op_ret.msg.fsz.height);

	if (fsize->index <= op_ret.msg.fsz.index) {
		fsize->type = op_ret.msg.fsz.type;
		if (fsize->type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			fsize->discrete.width = op_ret.msg.fsz.width;
			fsize->discrete.height = op_ret.msg.fsz.height;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static int sdrv_dummy_enum_frameintervals(struct file *file, void *fh,
					  struct v4l2_frmivalenum *fival)
{
	struct v4l2_fract interval;
	struct csi_dummy *csi = video_drvdata(file);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	op_ctl.op = SCS_OP_ENUM_FRAMEINTERVALS;
	op_ctl.msg.fitv.index = fival->index;
	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "enum_frameinterval done: %d\n", ret);
		return -EINVAL;
	}
	dev_info(csi->dev, "enum_frameinterval ok: %d, denominator (%d)\n",
			ret, op_ret.msg.fitv.type, op_ret.msg.fitv.denominator);

	if (fival->index > op_ret.msg.fitv.index)
		return -EINVAL;

	interval.numerator = 1;
	interval.denominator = op_ret.msg.fitv.denominator;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete = interval;

	return 0;
}

static long sdrv_dummy_vidioc_default(struct file *file, void *fh,
				      bool valid_prio, unsigned int cmd,
				      void *arg)
{
	return 0;
}

static const struct v4l2_ioctl_ops sdrv_dummy_vid_ioctl_ops = {
	.vidioc_querycap = sdrv_dummy_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = sdrv_dummy_video_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = sdrv_dummy_video_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = sdrv_dummy_video_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = sdrv_dummy_video_try_fmt,

	.vidioc_g_selection = sdrv_dummy_g_selection,
	.vidioc_s_selection = sdrv_dummy_s_selection,

	.vidioc_cropcap = sdrv_dummy_cropcap,

	.vidioc_enum_input = sdrv_dummy_enum_input,
	.vidioc_g_input = sdrv_dummy_g_input,
	.vidioc_s_input = sdrv_dummy_s_input,

	.vidioc_querystd = sdrv_dummy_querystd,
	.vidioc_g_std = sdrv_dummy_g_std,
	.vidioc_s_std = sdrv_dummy_s_std,

	.vidioc_g_parm = sdrv_dummy_g_parm,
	.vidioc_s_parm = sdrv_dummy_s_parm,

	.vidioc_enum_framesizes = sdrv_dummy_enum_framesizes,
	.vidioc_enum_frameintervals = sdrv_dummy_enum_frameintervals,

	.vidioc_default = sdrv_dummy_vidioc_default,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
};

static void sdrv_dummy_release(struct video_device *vdev)
{
	struct csi_dummy *csi = video_get_drvdata(vdev);

	mutex_destroy(&csi->q_lock);
	mutex_destroy(&csi->lock);
}

int sdrv_dummy_video_register(struct video_device *vdev)
{
	struct csi_dummy *csi = container_of(vdev,
					     struct csi_dummy, vdev);
	int ret;
	struct vb2_queue *q;

	q = &csi->queue;

	mutex_init(&csi->lock);
	mutex_init(&csi->q_lock);

	INIT_LIST_HEAD(&csi->buf_list);
	spin_lock_init(&csi->buf_lock);

	q->drv_priv = csi;
	q->mem_ops = &vb2_dma_contig_memops;
	q->ops = &sdrv_dummy_video_vb2_q_ops;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_DMABUF | VB2_MMAP | VB2_READ | VB2_USERPTR;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->buf_struct_size = sizeof(struct sdrv_dummy_buffer);
	q->dev = csi->dev;
	q->lock = &csi->q_lock;
	ret = vb2_queue_init(q);
	if (ret < 0) {
		dev_err(csi->dev, "Failed to init vb2 queue: %d\n", ret);
		goto error_vb2_init;
	}

	vdev->fops = &sdrv_dummy_vid_ops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING |
	    V4L2_CAP_READWRITE;
	vdev->ioctl_ops = &sdrv_dummy_vid_ioctl_ops;
	vdev->release = sdrv_dummy_release;
	vdev->v4l2_dev = &csi->v4l2_dev;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->queue = q;
	snprintf(vdev->name, ARRAY_SIZE(vdev->name), csi->type);
	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_err(csi->dev, "Failed to register video device: %d\n", ret);
		goto error_video_register;
	}

	video_set_drvdata(vdev, csi);

	return ret;

error_video_register:
	mutex_destroy(&csi->lock);
	mutex_destroy(&csi->q_lock);
	vb2_queue_release(vdev->queue);
error_vb2_init:

	return ret;
}

static inline void sdrv_dummy_on_img_acquired(
		struct csi_dummy *csi, uint8_t img_id, uint8_t index)
{
	struct sdrv_dummy_buffer *kbuf;
	struct vb2_v4l2_buffer *vbuf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&csi->buf_lock, flags);

	list_for_each_entry(kbuf, &csi->buf_list, queue) {
		if (kbuf->vbuf.vb2_buf.index == index) {
			vbuf = &kbuf->vbuf;
			//vbuf->vb2_buf.timestamp = ktime_get_ns();
			vbuf->vb2_buf.timestamp = csi->ts.data64 * 1000;
			dev_dbg(csi->dev, "fcnt %d, vb2_buf.index=%d, times = 0x%llx\n",
				csi->fcnt, kbuf->vbuf.vb2_buf.index, vbuf->vb2_buf.timestamp);

#if (IS_ENABLED(CONFIG_ARM64))
			/*
			 * Assure cache consistency before returning buffer to
			 * video core.
			 */
			if (is_device_dma_coherent(csi->dev)) {
				/* mark incoherent to do right cache ops */
				csi->dev->archdata.dma_coherent = false;

				dma_sync_single_for_cpu(csi->dev,
						kbuf->paddr[0],
						vbuf->vb2_buf.planes[0].length,
						DMA_FROM_DEVICE);

				/* restore to map page cacheable */
				csi->dev->archdata.dma_coherent = true;
			}
#endif
			vb2_buffer_done(&vbuf->vb2_buf, VB2_BUF_STATE_DONE);
			list_del(&kbuf->queue);
			break;
		}
	}

	spin_unlock_irqrestore(&csi->buf_lock, flags);
	csi->fcnt++;
	camera_heartbeat_inc();

#if FPS_DEBUG
    if (vbuf) {
        if (!(_fps_fn % 25)) {
            static u64 last = 0;
            u64 fps_ms = 25000000000000ul / (vbuf->vb2_buf.timestamp - last);
            pr_err("fn %llu fps %llu.%03llu\n", _fps_fn, fps_ms / 1000, fps_ms % 1000);
            last = vbuf->vb2_buf.timestamp;
        }
        _fps_fn++;
    }
#endif
}

static void sdrv_dummy_notify(struct mbox_client *client, void *mssg)
{
	struct csi_dummy *csi = container_of(client,
					     struct csi_dummy, client);
	sd_msghdr_t *msghdr = (sd_msghdr_t *)mssg;
	int i;

	dev_dbg(csi->dev, "%s++\n", __func__);

	if (!msghdr) {
		dev_err(csi->dev, "%s NULL mssg\n", __func__);
		return;
	}

	dev_dbg(csi->dev, "fcnt %d dat_len=%d, data[0-3]=0x%02x %02x %02x %02x\n",
		csi->fcnt, msghdr->dat_len, msghdr->data[0],
		msghdr->data[1], msghdr->data[2], msghdr->data[3]);

	for (i = 0; i < 8; i++)
		csi->ts.data8[i] = msghdr->data[i + 2];

	sdrv_dummy_on_img_acquired(csi, msghdr->data[0], msghdr->data[1]);
	dev_dbg(csi->dev, "%s\n", __func__);
}

static int sdrv_dummy_message_handler(void *data)
{
	struct csi_dummy *csi = data;
	struct msghdr msg;
	struct kvec iov;
	/*
	 * Old rpmsg protocol has a header, which is used to distinguish between
	 * several servers. We leave this issue to server enumeration service. For
	 * now we *know* there's only 1 server and dedicate ourself to camera
	 * business.
	 */
	struct scs_ioctl_result rs;
	int recvlen;

	while (!csi->pending_exit) {
		memset(&msg, 0, sizeof(msg));
		memset(&rs, 0, sizeof(rs));
		iov.iov_base = &rs;
		iov.iov_len = sizeof(rs);
		recvlen = kernel_recvmsg(csi->sock, &msg, &iov, 1, iov.iov_len, 0);
		if (recvlen < 0) {
			dev_err(csi->dev, "fail to recv msg, ret %d\n", recvlen);
			csi->pending_exit = 0;
			return 0;
		}

		/*
		 * SCS_OP_DQBUF is started by server, while other messages are replies
		 * to us.
		 */
		if (rs.op == SCS_OP_DQBUF) {
			csi->ts.data64 = ((uint64_t)rs.msg.g_bufinfo.timestamp_h << 32);
			csi->ts.data64 |= (uint64_t)rs.msg.g_bufinfo.timestamp;
			sdrv_dummy_on_img_acquired(csi, rs.instance, rs.msg.g_bufinfo.index);
		} else {
			memcpy(&csi->arrived_msg, &rs, sizeof(csi->arrived_msg));
			complete(&csi->arrived);
		}
	}

	return 0;
}

static int sdrv_dummy_create_msg_handler(struct csi_dummy *csi)
{
	struct sockaddr_in addr;
	int ret = 0;

	ret = sock_create_kern(&init_net,
			PF_INET, SOCK_DGRAM, IPPROTO_UDP, &csi->sock);
	if (ret < 0) {
		dev_err(csi->dev, "fail to create socket\n");
		return ret;
		}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(CLIENT_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = kernel_bind(csi->sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		dev_err(csi->dev, "fail to bind port %d\n", CLIENT_PORT);
		sock_release(csi->sock);
		return ret;
	}

	csi->pending_exit = 0;
	init_completion(&csi->arrived);
	csi->msg_handler = kthread_run(
			sdrv_dummy_message_handler, csi, "cam-udp-%d", CLIENT_PORT);

	return 0;
}

static int sdrv_dummy_request_irq(struct csi_dummy *csi)
{
	struct mbox_client *client;
	int ret = 0;

	client = &csi->client;
	/* Initialize the mbox channel used by touchscreen */
	client->dev = csi->dev;
	client->tx_done = NULL;
	client->rx_callback = sdrv_dummy_notify;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	csi->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(csi->mbox)) {
		ret = -EBUSY;
		dev_err(csi->dev, "mbox_request_channel failed: %ld\n",
			PTR_ERR(csi->mbox));
	}

	return ret;
}

static void sdrv_dummy_parse_xfer_type(
		struct csi_dummy *csi, const struct device_node *np)
{
	const char *ip;
	uint32_t port, part;
	char *dot, *start;
	uint8_t parts[4];
	int ret, i;

	csi->use_socket = 0;

	ret = of_property_read_string(np, "server-ip", &ip);
	if (ret < 0) {
		return;
	}

	ret = of_property_read_u32(np, "server-port", &port);
	if (ret < 0) {
		dev_warn(csi->dev, "don't have a port num? use rpmsg\n");
		return;
	}

	/* suppose ip is a IPv4 address written in dotted decimal notation */
	strncpy(csi->server_ip, ip, IPv4_ADDR_LEN-1);
	csi->server_ip[IPv4_ADDR_LEN-1] = '\0';

	dot = csi->server_ip-1;
	for (i = 0; i < 3; i++) {
		start = dot+1;
		dot = strstr(start, ".");
		if (!dot) {
			dev_warn(csi->dev,
					"invalid IPv4 address '%s', use rpmsg\n", csi->server_ip);
			return;
		}

		*dot = '\0';
		if (kstrtou32(start, 10, &part) < 0) {
			*dot = '.';
			dev_warn(csi->dev,
					"invalid IPv4 address '%s', use rpmsg\n", csi->server_ip);
			return;
		}
		parts[i] = part;
		*dot = '.';
	}

	start = dot+1;
	if (kstrtou32(start, 10, &part) < 0) {
		dev_warn(csi->dev,
				"invalid IPv4 address '%s', use rpmsg\n", csi->server_ip);
		return;
	}
	parts[3] = part;

	csi->server_port = port;

	csi->server_addr.sin_family = PF_INET;
	csi->server_addr.sin_port = htons(csi->server_port);
	csi->server_addr.sin_addr.s_addr =
		((parts[3] << 24) | (parts[2] << 16) | (parts[1] << 8) | parts[0]);
	csi->use_socket = 1;
	dev_info(csi->dev,
			"camera server at %s:%d\n", csi->server_ip, csi->server_port);
}

static int sdrv_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct csi_dummy *csi;
	const char *type;
	int ret;

	dev_info(dev, "%s(): .\n", __func__);
	csi = devm_kzalloc(dev, sizeof(*csi), GFP_KERNEL);
	if (!csi)
		return -ENOMEM;

	atomic_set(&csi->at_suspend, 0);
	atomic_set(&csi->at_stream_off, 0);
	atomic_set(&csi->at_user, 0);
	csi->dev = dev;
	platform_set_drvdata(pdev, csi);

	sdrv_dummy_parse_xfer_type(csi, pdev->dev.of_node);

	/* Read instance from dts */
	ret = of_property_read_u16(pdev->dev.of_node, "dev_id", &csi->instance);
	if (ret < 0) {
		dev_err(&pdev->dev, "Missing dev_id, use 0\n");
		csi->instance = 0;
	}
	ret = of_property_read_string(dev->of_node, "type", &type);
	if (ret < 0) {
		dev_err(dev, "Missing type\n");
		return -EINVAL;
	}

	ret = of_property_read_u16(pdev->dev.of_node,
			"rproc", &csi->rproc);
	if (ret < 0)
		dev_err(&pdev->dev, "Missing rproc, use ap1\n");

	if ((csi->rproc != MBOX_RPROC_AP1) && (csi->rproc != MBOX_RPROC_AP2)) {
		dev_err(&pdev->dev, "Can't find rproc %d, use ap1\n",
		csi->rproc);
		csi->rproc = MBOX_RPROC_AP1;
	}

	strcpy(csi->type, type);
	dev_err(dev, "%s(): csi->type=%s.\n", __func__, csi->type);
	ret = v4l2_device_register(csi->dev, &csi->v4l2_dev);
	if (ret < 0)
		goto err_register_v4l2;

	ret = sdrv_dummy_video_register(&csi->vdev);
	if (ret < 0)
		goto err_register_video;

	if (csi->use_socket) {
		ret = sdrv_dummy_create_msg_handler(csi);
	} else {
	ret = sdrv_dummy_request_irq(csi);
	}
	if (ret < 0)
		goto err_register_video;

	dev_info(dev, "%s(): exit.\n", __func__);
	return 0;

err_register_video:
	video_unregister_device(&csi->vdev);
err_register_v4l2:
	v4l2_device_unregister(&csi->v4l2_dev);

	return ret;
}

static int sdrv_dummy_remove(struct platform_device *pdev)
{
	struct csi_dummy *csi;

	csi = platform_get_drvdata(pdev);

	video_unregister_device(&csi->vdev);

	v4l2_device_unregister(&csi->v4l2_dev);

	return 0;
}

int sdrv_dummy_pm_suspend(struct device *dev)
{
	struct platform_device *pdev;
	struct csi_dummy *csi;
	int ret = -1;

	pdev = to_platform_device(dev);
	csi = platform_get_drvdata(pdev);

	atomic_set(&csi->at_suspend, 1);
	// close when suspend and stream off
	if (atomic_read(&csi->at_suspend) && atomic_read(&csi->at_stream_off))
		ret = sdrv_dummy_video_release(csi->file);

	pr_info("sdrv-dummy: suspend, ret:%d\n", ret);

	return 0;
}

int sdrv_dummy_pm_resume(struct device *dev)
{
	struct platform_device *pdev;
	struct csi_dummy *csi;

	pdev = to_platform_device(dev);
	csi = platform_get_drvdata(pdev);
	atomic_set(&csi->at_suspend, 0);

	pr_info("sdrv-dummy: resume\n");

	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops pm_ops = {
	.suspend = sdrv_dummy_pm_suspend,
	.resume = sdrv_dummy_pm_resume,
};
#else
int sdrv_dummy_dev_suspend(struct device *dev, pm_message_t state)
{
	sdrv_dummy_pm_suspend(dev);
	return 0;
}
int sdrv_dummy_dev_resume(struct device *dev)
{
	sdrv_dummy_pm_resume(dev);
	return 0;
}
#endif // CONFIG_PM

static const struct of_device_id sdrv_dummy_dt_match[] = {
	{.compatible = "sd,sdrv_dummy"},
	{},
};

static struct platform_driver sdrv_dummy_driver = {
	.probe = sdrv_dummy_probe,
	.remove = sdrv_dummy_remove,
	.driver = {
		.name = "sdrv-dummy",
		.of_match_table = sdrv_dummy_dt_match,
#ifdef CONFIG_PM
		.pm = &pm_ops,
#else
		.pm = NULL,
		.suspend = sdrv_dummy_dev_suspend,
		.resume = sdrv_dummy_dev_resume,
#endif // CONFIG_PM
	},
};

module_platform_driver(sdrv_dummy_driver);

MODULE_ALIAS("platform:sdrv-dummy");
MODULE_DESCRIPTION("Semidrive Camera driver");
MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_LICENSE("GPL v2");
