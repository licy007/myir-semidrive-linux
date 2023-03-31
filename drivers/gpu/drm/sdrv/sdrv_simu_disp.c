#include <linux/inet.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>
#include <net/net_namespace.h>
#include <uapi/linux/in.h>
#include "sdrv_cast.h"

#define DISP_WIDTH 1920u
#define DISP_HEIGHT 720u
#define DISP_STRIDE (DISP_WIDTH * 4u)
#define DISP_PLANES 1u
#define DISP_FORMAT V4L2_PIX_FMT_ARGB32
#define NUM_BUFFERS 3u

struct simulated_display_buffer {
	struct vb2_v4l2_buffer buf;
	dma_addr_t addr;
	struct list_head list;
};

struct simulated_display {
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct vb2_queue vq;
	struct mutex q_lock;
	struct video_device vdev;
	struct list_head input;
	struct list_head output;
	spinlock_t lock;
	struct socket *sock;
	int port;
	struct sockaddr_in addr;
	struct task_struct *handler;
	int client_streaming;
	int server_streaming;
	struct mutex ref_lock;
	int ref_count;
	char video_name[32];
};

static int simulated_display_send_buffer(struct simulated_display *sd,
					 dma_addr_t addr)
{
	struct msghdr msg;
	struct kvec iov;
	struct disp_frame_info fi;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &sd->addr;
	msg.msg_namelen = sizeof(sd->addr);

	memset(&fi, 0, sizeof(fi));
	fi.addr_l = addr;

	iov.iov_base = &fi;
	iov.iov_len = sizeof(fi);

	ret = kernel_sendmsg(sd->sock, &msg, &iov, 1, iov.iov_len);
	if (ret < 0) {
		dev_err(sd->dev, "fail to send msg, ret %d\n", ret);
		return ret;
	}

	return 0;
}

static int simulated_display_queue_setup(struct vb2_queue *q,
					 unsigned int *num_buffers,
					 unsigned int *num_planes,
					 unsigned int sizes[],
					 struct device *alloc_devs[])
{
	struct simulated_display *sd = vb2_get_drv_priv(q);

	dev_info(
		sd->dev,
		"%s() request %u buffers %u planes, "
                "actual %u buffers %u planes, size[0] %u\n",
		__func__, *num_buffers, *num_planes, NUM_BUFFERS, DISP_PLANES,
		sizes[0]);

	*num_buffers = NUM_BUFFERS;
	*num_planes = DISP_PLANES;
	sizes[0] = DISP_HEIGHT * DISP_STRIDE;

	return 0;
}

static int simulated_display_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *buf = to_vb2_v4l2_buffer(vb);
	struct simulated_display_buffer *sdb =
		container_of(buf, struct simulated_display_buffer, buf);
	struct simulated_display *sd = vb2_get_drv_priv(vb->vb2_queue);

	dev_info(sd->dev, "%s()\n", __func__);

	sdb->addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	dev_info(sd->dev, "%s() buf[%u] %px addr 0x%llx\n", __func__, vb->index,
		 sdb, sdb->addr);

	return 0;
}

static void simulated_display_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *buf = to_vb2_v4l2_buffer(vb);
	struct simulated_display_buffer *sdb =
		container_of(buf, struct simulated_display_buffer, buf);
	struct simulated_display *sd = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long flags;
	int started = 1;

	dev_dbg(sd->dev, "Q buf 0x%llx\n", sdb->addr);

	spin_lock_irqsave(&sd->lock, flags);
	if (!sd->server_streaming) {
		/*
		 * save this buffer if server is not started
		 */
		list_add_tail(&sdb->list, &sd->input);
		dev_info(sd->dev, "Haven't received DISP_CMD_START from server, save this buffer\n");
		started = 0;
	} else {
		list_add_tail(&sdb->list, &sd->output);
	}
	spin_unlock_irqrestore(&sd->lock, flags);

	if (started) {
		simulated_display_send_buffer(sd, sdb->addr);
	}
}

static int simulated_display_start_streaming(struct vb2_queue *vq,
					     unsigned int count)
{
	struct simulated_display *sd = vb2_get_drv_priv(vq);
	struct simulated_display_buffer *buf;
	struct list_head list;
	unsigned long flags;

	dev_info(sd->dev, "%s()\n", __func__);

	INIT_LIST_HEAD(&list);
	spin_lock_irqsave(&sd->lock, flags);
	/*
	 * toggle client state, send buffer to server if started
	 */
	sd->client_streaming = 1;
	if (sd->server_streaming) {
		while (!list_empty(&sd->input)) {
			list_move_tail(sd->input.next, &list);
		}
	}
	spin_unlock_irqrestore(&sd->lock, flags);

	while (!list_empty(&list)) {
		buf = list_first_entry(&list, struct simulated_display_buffer,
				       list);
		list_del(&buf->list);
		spin_lock_irqsave(&sd->lock, flags);
		list_add_tail(&buf->list, &sd->output);
		spin_unlock_irqrestore(&sd->lock, flags);
		simulated_display_send_buffer(sd, buf->addr);
	}

	return 0;
}

static void simulated_display_stop_streaming(struct vb2_queue *vq)
{
	struct simulated_display *sd = vb2_get_drv_priv(vq);
	struct simulated_display_buffer *sdb;
	struct list_head list;
	unsigned long flags;
	int count1 = 0, count2 = 0;

	spin_lock_irqsave(&sd->lock, flags);
	sd->client_streaming = 0;
	/*
	 * stash all buffers
	 */
	INIT_LIST_HEAD(&list);
	while (!list_empty(&sd->input)) {
		list_move_tail(sd->input.next, &list);
		count1++;
	}
	while (!list_empty(&sd->output)) {
		list_move_tail(sd->output.next, &list);
		count2++;
	}
	spin_unlock_irqrestore(&sd->lock, flags);

	dev_info(sd->dev, "%s() input %d output %d\n", __func__, count1,
		 count2);

	while (!list_empty(&list)) {
		sdb = list_first_entry(&list, struct simulated_display_buffer,
				       list);
		list_del(&sdb->list);
		dev_dbg(sd->dev, "DQ buf 0x%llx ERR\n", sdb->addr);
		vb2_buffer_done(&sdb->buf.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	/*
	 * normally there will be only 1 buffer pending
	 */
	vb2_wait_for_all_buffers(vq);

	dev_info(sd->dev, "%s() done\n", __func__);
}

static const struct vb2_ops simulated_display_vb2_ops = {
	.queue_setup = simulated_display_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_init = simulated_display_buf_init,
	.start_streaming = simulated_display_start_streaming,
	.stop_streaming = simulated_display_stop_streaming,
	.buf_queue = simulated_display_buf_queue,
};

static int simulated_display_open(struct file *file)
{
	struct simulated_display *sd = video_drvdata(file);
	struct v4l2_fh *vfh;
	int ret = 0;

	dev_info(sd->dev, "%s()\n", __func__);

	mutex_lock(&sd->ref_lock);
	if (sd->ref_count > 0) {
		dev_err(sd->dev, "too many users, open ref_count %d\n",
			sd->ref_count);
		ret = -EBUSY;
		goto unlock;
	}

	vfh = kzalloc(sizeof(*vfh), GFP_KERNEL);
	if (!vfh) {
		dev_err(sd->dev, "not enough memory\n");
		ret = -ENOMEM;
		goto unlock;
	}

	v4l2_fh_init(vfh, &sd->vdev);
	v4l2_fh_add(vfh);
	file->private_data = vfh;
	sd->ref_count++;

unlock:
	mutex_unlock(&sd->ref_lock);

	return ret;
}

static int simulated_display_release(struct file *file)
{
	struct simulated_display *sd = video_drvdata(file);

	dev_info(sd->dev, "%s()\n", __func__);

	mutex_lock(&sd->ref_lock);
	if (sd->ref_count > 0) {
		vb2_fop_release(file);
		file->private_data = NULL;
		sd->ref_count--;
	}
	mutex_unlock(&sd->ref_lock);

	return 0;
}

static const struct v4l2_file_operations simulated_display_v4l2_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = simulated_display_open,
	.release = simulated_display_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.read = vb2_fop_read,
};

static int simulated_display_enum_fmt(struct file *file, void *fh,
				      struct v4l2_fmtdesc *f)
{
	struct simulated_display *sd = video_drvdata(file);

	dev_info(sd->dev, "%s() index %u type %u\n", __func__, f->index,
		 f->type);

	if (f->index != 0 || f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	f->pixelformat = DISP_FORMAT;

	return 0;
}

static int simulated_display_g_fmt(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct simulated_display *sd = video_drvdata(file);

	dev_info(sd->dev, "%s() type %u\n", __func__, f->type);

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	f->fmt.pix_mp.width = DISP_WIDTH;
	f->fmt.pix_mp.height = DISP_HEIGHT;
	f->fmt.pix_mp.pixelformat = DISP_FORMAT;
	f->fmt.pix_mp.plane_fmt[0].sizeimage = DISP_HEIGHT * DISP_STRIDE;
	f->fmt.pix_mp.num_planes = DISP_PLANES;

	return 0;
}

static int simulated_display_s_fmt(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct simulated_display *sd = video_drvdata(file);

	dev_info(sd->dev, "%s() type %u\n", __func__, f->type);

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	f->fmt.pix_mp.width = DISP_WIDTH;
	f->fmt.pix_mp.height = DISP_HEIGHT;
	f->fmt.pix_mp.pixelformat = DISP_FORMAT;
	f->fmt.pix_mp.plane_fmt[0].sizeimage = DISP_HEIGHT * DISP_STRIDE;
	f->fmt.pix_mp.num_planes = DISP_PLANES;

	return 0;
}

static int simulated_display_try_fmt(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	struct simulated_display *sd = video_drvdata(file);

	dev_info(sd->dev, "%s() type %u\n", __func__, f->type);

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	f->fmt.pix_mp.width = DISP_WIDTH;
	f->fmt.pix_mp.height = DISP_HEIGHT;
	f->fmt.pix_mp.pixelformat = DISP_FORMAT;
	f->fmt.pix_mp.plane_fmt[0].sizeimage = DISP_HEIGHT * DISP_STRIDE;
	f->fmt.pix_mp.num_planes = DISP_PLANES;

	return 0;
}

static const struct v4l2_ioctl_ops simulated_display_ioctl_ops = {
	.vidioc_enum_fmt_vid_cap_mplane = simulated_display_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = simulated_display_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = simulated_display_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = simulated_display_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
};

static int simulated_display_parse_dt(struct simulated_display *sd,
				      const struct device_node *np)
{
	const char *ip;
	uint32_t port;
	const char *suffix;

	if (of_property_read_u32(np, "port", &port)) {
		dev_err(sd->dev, "fail to find \"port\"\n");
		return -ENODEV;
	}

	sd->port = port;

	if (of_property_read_string(np, "server-ip", &ip)) {
		dev_err(sd->dev, "fail to find \"server-ip\"\n");
		return -ENODEV;
	}

	if (of_property_read_u32(np, "server-port", &port)) {
		dev_err(sd->dev, "fail to find \"server-port\"\n");
		return -ENODEV;
	}

	sd->addr.sin_family = PF_INET;
	sd->addr.sin_port = htons(port);
	sd->addr.sin_addr.s_addr = in_aton(ip);

	if (!of_property_read_string(np, "name-suffix", &suffix)) {
		snprintf(sd->video_name, 32, "sd-vname-%s", suffix);
	} else {
		snprintf(sd->video_name, 32, "%s", np->full_name);
	}
	sd->video_name[31] = '\0';

	dev_info(sd->dev, "parse server %s:%u local port %d name %s\n",
		 ip, port, sd->port, sd->video_name);

	return 0;
}

static int simulated_display_message_handler(void *data)
{
	struct simulated_display *sd = data;
	struct msghdr msg;
	struct kvec iov;
	struct disp_frame_info fi;
	struct list_head list;
	struct simulated_display_buffer *sdb;
	unsigned long flags;
	int recvlen;
	int found;
	int stopped;

	for (;;) {
		memset(&msg, 0, sizeof(msg));
		memset(&fi, 0, sizeof(fi));
		iov.iov_base = &fi;
		iov.iov_len = sizeof(fi);
		recvlen =
			kernel_recvmsg(sd->sock, &msg, &iov, 1, iov.iov_len, 0);
		if (recvlen < 0) {
			dev_err(sd->dev, "fail to recv msg, ret %d\n", recvlen);
			break;
		}

		if (fi.cmd == DISP_CMD_START) {
			dev_info(sd->dev, "server started\n");
			/*
			 * toggle server state, send buffer to server if we've
                         * had some
			 */
			INIT_LIST_HEAD(&list);
			spin_lock_irqsave(&sd->lock, flags);
			sd->server_streaming = 1;
			if (sd->client_streaming) {
				while (!list_empty(&sd->input)) {
					list_move_tail(sd->input.next, &list);
				}
			}
			spin_unlock_irqrestore(&sd->lock, flags);

			while (!list_empty(&list)) {
				sdb = list_first_entry(
					&list, struct simulated_display_buffer,
					list);
				list_del(&sdb->list);
				spin_lock_irqsave(&sd->lock, flags);
				list_add_tail(&sdb->list, &sd->output);
				spin_unlock_irqrestore(&sd->lock, flags);
				simulated_display_send_buffer(sd, sdb->addr);
			}
		} else if (fi.cmd == DISP_CMD_SET_FRAMEINFO) {
			/*
			 * find buffer from output list
			 */
			found = false;
			stopped = false;
			spin_lock_irqsave(&sd->lock, flags);
			list_for_each_entry (sdb, &sd->output, list) {
				if (fi.addr_l == sdb->addr) {
					found = true;
					break;
				}
			}
			if (found) {
				list_del(&sdb->list);
			}
			stopped = !sd->client_streaming;
			spin_unlock_irqrestore(&sd->lock, flags);

			if (!found) {
				if (unlikely(!stopped)) {
					dev_err(sd->dev,
						"invalid buffer addr 0x%x\n",
						fi.addr_l);
				}
				continue;
			}

			if (unlikely(!fi.width && !fi.height)) {
				dev_err(sd->dev, "DQ buf 0x%llx ERR\n",
					sdb->addr);
				vb2_buffer_done(&sdb->buf.vb2_buf,
						VB2_BUF_STATE_ERROR);
			} else {
				dev_dbg(sd->dev, "DQ buf 0x%llx\n", sdb->addr);
				vb2_buffer_done(&sdb->buf.vb2_buf,
						VB2_BUF_STATE_DONE);
			}
		} else if (fi.cmd == DISP_CMD_CLEAR) {
			dev_info(sd->dev, "server stopped\n");
			spin_lock_irqsave(&sd->lock, flags);
			sd->server_streaming = 0;
			spin_unlock_irqrestore(&sd->lock, flags);
		} else {
			dev_err(sd->dev, "unknown cmd %u\n", fi.cmd);
		}
	}

	sock_release(sd->sock);
	sd->sock = NULL;
	dev_info(sd->dev, "message handler exit\n");

	return 0;
}

static int simulated_display_start_handler(struct simulated_display *sd)
{
	struct sockaddr_in addr;
	int ret;

	ret = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP,
			       &sd->sock);
	if (ret < 0) {
		dev_err(sd->dev, "fail to create socket\n");
		return ret;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(sd->port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = kernel_bind(sd->sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		dev_err(sd->dev, "fail to bind port %d\n", sd->port);
		goto release_sock;
	}

	sd->handler = kthread_run(simulated_display_message_handler, sd,
				  "simu-disp-hdl");
	if (IS_ERR(sd->handler)) {
		dev_err(sd->dev, "fail to create message handler\n");
		ret = PTR_ERR(sd->handler);
		goto release_sock;
	}

	return 0;

release_sock:
	sock_release(sd->sock);
	sd->sock = NULL;
	return ret;
}

static int simulated_display_probe(struct platform_device *pdev)
{
	struct simulated_display *sd;
	struct vb2_queue *q;
	struct video_device *vdev;
	int ret;

	sd = devm_kzalloc(&pdev->dev, sizeof(*sd), GFP_KERNEL);
	if (!sd)
		return -ENOMEM;

	sd->dev = &pdev->dev;

	ret = simulated_display_parse_dt(sd, sd->dev->of_node);
	if (ret < 0)
		return ret;

	ret = v4l2_device_register(sd->dev, &sd->v4l2_dev);
	if (ret < 0) {
		dev_err(sd->dev, "v4l2_device_register() fail: %d\n", ret);
		goto err_v4l2_device;
	}

	q = &sd->vq;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_DMABUF | VB2_MMAP | VB2_USERPTR | VB2_READ;
	q->dev = sd->dev;
	mutex_init(&sd->q_lock);
	q->lock = &sd->q_lock;
	q->ops = &simulated_display_vb2_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->drv_priv = sd;
	q->buf_struct_size = sizeof(struct simulated_display_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = NUM_BUFFERS;
	ret = vb2_queue_init(q);
	if (ret < 0) {
		dev_err(sd->dev, "vb2_queue_init() fail: %d\n", ret);
		goto err_vb2_queue;
	}

	vdev = &sd->vdev;
	vdev->fops = &simulated_display_v4l2_fops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	vdev->v4l2_dev = &sd->v4l2_dev;
	vdev->queue = q;
	snprintf(vdev->name, ARRAY_SIZE(vdev->name), sd->video_name);
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->release = video_device_release_empty;
	vdev->ioctl_ops = &simulated_display_ioctl_ops;
	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_err(sd->dev, "video_register_device() fail: %d\n", ret);
		goto err_video_device;
	}

	INIT_LIST_HEAD(&sd->input);
	INIT_LIST_HEAD(&sd->output);
	spin_lock_init(&sd->lock);
	sd->client_streaming = 0;
	sd->server_streaming = 0;
	mutex_init(&sd->ref_lock);
	sd->ref_count = 0;

	video_set_drvdata(vdev, sd);

	ret = simulated_display_start_handler(sd);
	if (ret < 0)
		goto err_video_device;

	platform_set_drvdata(pdev, sd);
	dev_info(sd->dev, "simulated display ready\n");

	return 0;

err_video_device:
	vb2_queue_release(&sd->vq);

err_vb2_queue:
	mutex_destroy(&sd->q_lock);

err_v4l2_device:
	v4l2_device_unregister(&sd->v4l2_dev);

	return ret;
}

static int simulated_display_remove(struct platform_device *pdev)
{
	struct simulated_display *sd = platform_get_drvdata(pdev);

	mutex_destroy(&sd->q_lock);
	video_unregister_device(&sd->vdev);
	v4l2_device_unregister(&sd->v4l2_dev);

	return 0;
}

static const struct of_device_id simulated_display_dt_match[] = {
	{ .compatible = "sd,sdrv-simu-disp" },
	{},
};

static struct platform_driver simulated_display_driver = {
	.probe = simulated_display_probe,
	.remove = simulated_display_remove,
	.driver = {
		.name = "sdrv-simu-disp",
		.of_match_table = simulated_display_dt_match,
	},
};

module_platform_driver(simulated_display_driver);

MODULE_DESCRIPTION("Semidrive simulated-display driver");
MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_LICENSE("GPL v2");
