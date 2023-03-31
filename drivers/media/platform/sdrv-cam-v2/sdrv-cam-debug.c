/*
 * sdrv-debug.c
 *
 * Semidrive platform v4l2 core file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include "sdrv-cam-core.h"
#include "sdrv-cam-debug.h"
#include "csi-controller-os.h"

static int print_level = CAM_ERR;

char *cam_printk[4] = {
	KERN_ERR,
	NULL,
	NULL,
	NULL,
};

static int cam_debug_show(struct seq_file *m, void *v)
{
    struct csi_core *csi = m->private;

	struct kstream_device *kstream;
    int i = 0;

    seq_printf(m, "\n");
    seq_printf(m, "---------------------------------------CSI---------------------------------------\n");
    seq_printf(m, "sdrv-cam debug level: %s\n",
                            print_level == CAM_EMERG ? "EMERG"
                            : print_level == CAM_ERR ? "ERR"
                            : print_level == CAM_WARNING ? "WAR"
                            : print_level == CAM_INFO ? "INFO"
                            : print_level == CAM_DEBUG ? "DEBUG"
                            : "undefined");
    seq_printf(m, "%6s%5s%10s%8s%7s%9s%7s%9s%9s%8s\n", "csi_id", "sync", "mbus_type",
                                                        "frm_cnt","bt_err","bt_fatal",
                                                        "bt_cof","crop_err","overflow",
                                                        "bus_err");
    seq_printf(m, "%6d", csi->host_id);
    seq_printf(m, "%5d", csi->sync);
    seq_printf(m, "%10s", csi->mbus_type == V4L2_MBUS_PARALLEL ? "PARALLEL"
                            : csi->mbus_type == V4L2_MBUS_BT656 ? "BT656"
                            : csi->mbus_type == V4L2_MBUS_CSI1 ? "CSI1"
                            : csi->mbus_type == V4L2_MBUS_CCP2 ? "CCP2"
                            : csi->mbus_type == V4L2_MBUS_CSI2 ? "CSI2"
                            : "undefined");
    seq_printf(m, "%8lld", csi->ints.frm_cnt);
    seq_printf(m, "%7d", csi->ints.bt_err);
    seq_printf(m, "%9d", csi->ints.bt_fatal);
    seq_printf(m, "%7d", csi->ints.bt_cof);
    seq_printf(m, "%9d", csi->ints.crop_err);
    seq_printf(m, "%9d", csi->ints.overflow);
    seq_printf(m, "%8d", csi->ints.bus_err);
    seq_printf(m, "\n");

    seq_printf(m, "%9s%13s%16s%18s%11s%9s%8s\n", "ref_count", "kstream_nums", "mipi_stream_num",
                                                "active_stream_num", "skip_frame",
                                                "img_sync", "img_cnt");
    seq_printf(m, "%9d", atomic_read(&csi->ref_count));
    seq_printf(m, "%13d", csi->kstream_nums);
    seq_printf(m, "%16d", csi->mipi_stream_num);
    seq_printf(m, "%18d", atomic_read(&csi->active_stream_num));
    seq_printf(m, "%11d", csi->skip_frame);
    seq_printf(m, "%9d", csi->img_sync);
    seq_printf(m, "%8d", csi->img_cnt);

    seq_printf(m, "\n");
    seq_printf(m, "---------------------------------------CSI-IPI---------------------------------------\n");
    seq_printf(m, "%6s%10s%15s%10s%11s\n", "ipi_id", "state", "Frame_interval",
                                            "frame_cnt","frame_loss");
	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
	    i++;
		if (kstream->state != RUNNING && kstream->state != IDLE)
			continue;
        seq_printf(m, "%6d", i - 1);
        seq_printf(m, "%10s",
                                kstream->state == STOPPED ? "STOPPED"
                                : kstream->state == INITING ? "INITING"
                                : kstream->state == INITIALED ? "INITIALED"
                                : kstream->state == RUNNING ? "RUNNING"
                                : kstream->state == IDLE ? "IDLE"
                                : kstream->state == STOPPING ? "STOPPING"
                                : "undefined");
        seq_printf(m, "%9lld.%03lldms", kstream->frame_interval / 1000 / 1000,
        kstream->frame_interval / 1000 % 1000);
        seq_printf(m, "%10d", kstream->frame_cnt);
        seq_printf(m, "%11d", kstream->frame_loss_cnt);
        seq_printf(m, "\n");
	}

    seq_printf(m, "%8s%9s%7s%7s%27s\n","sensor_w","sensor_h",
                                        "csi_w","csi_h",
                                         "crop[    x    y    w    h]");
	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
	    i++;
		if (kstream->state != RUNNING && kstream->state != IDLE)
			continue;
        seq_printf(m, "%8d", kstream->mbus_fmt[0].width);
        seq_printf(m, "%9d", kstream->mbus_fmt[0].height);
        seq_printf(m, "%6d", kstream->mbus_fmt[1].width);
        seq_printf(m, "%6d", kstream->mbus_fmt[1].height);
        seq_printf(m, "%11d", kstream->crop.left);
        seq_printf(m, "%5d", kstream->crop.top);
        seq_printf(m, "%5d", kstream->crop.width);
        seq_printf(m, "%5d", kstream->crop.height);
        seq_printf(m, "\n");
	}

    seq_printf(m, "\n");

    return 0;
}

static ssize_t cam_debug_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *f_pos)
{
    int num = 0;
    int level = 0;
    char tmp[50];
    struct seq_file *m = file->private_data;
    struct csi_core *csi = m->private;

    if (copy_from_user(tmp, buffer, count)) {
        return -EFAULT;
    }

    num = sscanf(tmp, "%d", &level);

    if(num != 1) {
        return -EFAULT;
	}

	switch (level) {
	case CAM_EMERG:
        cam_err_loglevel = NULL;
        cam_warn_loglevel = NULL;
        cam_info_loglevel = NULL;
        cam_debug_loglevel = NULL;
	    break;
	case CAM_ERR:
	    cam_err_loglevel = KERN_ERR;
	    cam_warn_loglevel = NULL;
	    cam_info_loglevel = NULL;
	    cam_debug_loglevel = NULL;
		break;
	case CAM_WARNING:
	    cam_err_loglevel = KERN_ERR;
	    cam_warn_loglevel = KERN_WARNING;
	    cam_info_loglevel = NULL;
	    cam_debug_loglevel = NULL;
		break;
	case CAM_INFO:
	    cam_err_loglevel = KERN_ERR;
	    cam_warn_loglevel = KERN_WARNING;
	    cam_info_loglevel = KERN_INFO;
	    cam_debug_loglevel = NULL;
		break;
	case CAM_DEBUG:
	    cam_err_loglevel = KERN_ERR;
	    cam_warn_loglevel = KERN_WARNING;
	    cam_info_loglevel = KERN_INFO;
	    cam_debug_loglevel = KERN_DEBUG;
		break;
	default:
        cam_dev_loge(csi->dev, "Parameter error %d\n", level);
		break;
	}

    print_level = level;

    return count;
}

static int cam_debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, cam_debug_show, inode->i_private);
}

static struct file_operations cam_debug_fops = {
    .owner   = THIS_MODULE,
    .open    = cam_debug_open,
    .release = single_release,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .write   = cam_debug_write,
};

int cam_debug_init(struct platform_device *pdev)
{
    int ret = 0;
    struct csi_core *csi = pdev->dev.driver_data;

    csi->p_dentry = debugfs_create_file(dev_name(&pdev->dev), 0644, NULL,
        platform_get_drvdata(pdev), &cam_debug_fops);

    if (!csi->p_dentry) {
        cam_dev_loge(&pdev->dev, "debug_create failed\n");
        ret = -ENODEV;
    }

    return 0;
}

void cam_debug_exit(struct platform_device *pdev)
{

    struct csi_core *csi = pdev->dev.driver_data;

    if (csi->p_dentry) {
        debugfs_remove(csi->p_dentry);
        csi->p_dentry = NULL;
    }

    return;
}
