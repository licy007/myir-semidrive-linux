/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2016, Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SD_DISP_CRC_DCMD_H
#define _SD_DISP_CRC_DCMD_H
// #include <linux/types.h>

struct roi_params {
    unsigned int win_num;
    unsigned int start_x;
    unsigned int start_y;
    unsigned int end_x;
    unsigned int end_y;
};

struct hcrc_data {
    bool status[8];
    unsigned int result[8];
};


struct disp_crc_set_roi_ipc {
    struct roi_params roi_parms[8];
    unsigned int valid_roi_cnt;
};

struct disp_crc_get_hcrc_ipc {
    unsigned int get_hcrc_result;
    struct hcrc_data hcrc;
};

struct scrc_input{
    int fd;
    unsigned long dma_addr;
    struct sg_table *sgt;
    struct dma_buf_attachment *attach;
    unsigned long vaddr;
    unsigned long size;
    int format;
    int stride;
};

struct disp_crc_get_scrc_ipc {
    unsigned int result[8];
    struct scrc_input input;
};

#define DISP_CRC_IOCTL_BASE             'c'

#define DISP_CRC_IO(nr)              _IO(DISP_CRC_IOCTL_BASE,nr)
#define DISP_CRC_IOR(nr,type)        _IOR(DISP_CRC_IOCTL_BASE,nr,type)
#define DISP_CRC_IOW(nr,type)        _IOW(DISP_CRC_IOCTL_BASE,nr,type)
#define DISP_CRC_IOWR(nr,type)       _IOWR(DISP_CRC_IOCTL_BASE,nr,type)

#define DISP_CRC_SET_ROI_CMD_CODE        1
#define DISP_CRC_GET_HCRC_CMD_CODE       2
#define DISP_CRC_START_CALC_CMD_CODE     3
#define DISP_CRC_GET_SCRC_CMD_CODE       4

#define DCMD_DISP_CRC_SET_ROI       DISP_CRC_IOWR(DISP_CRC_SET_ROI_CMD_CODE, struct disp_crc_set_roi_ipc)
#define DCMD_DISP_CRC_GET_HCRC      DISP_CRC_IOWR(DISP_CRC_GET_HCRC_CMD_CODE, struct disp_crc_get_hcrc_ipc)
#define DCMD_DISP_CRC_START_CALC    DISP_CRC_IOWR(DISP_CRC_START_CALC_CMD_CODE, int)
#define DCMD_DISP_CRC_GET_SCRC      DISP_CRC_IOWR(DISP_CRC_GET_SCRC_CMD_CODE, struct disp_crc_get_scrc_ipc)

#endif /* _SD_DISP_CRC_DCMD_H */
