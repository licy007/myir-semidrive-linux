/*
* SEMIDRIVE Copyright Statement
* Copyright (c) SEMIDRIVE. All rights reserved
* This software and all rights therein are owned by SEMIDRIVE,
* and are protected by copyright law and other relevant laws, regulations and protection.
* Without SEMIDRIVE’s prior written consent and /or related rights,
* please do not use this software or any potion thereof in any form or by any means.
* You may not reproduce, modify or distribute this software except in compliance with the License.
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* You should have received a copy of the License along with this program.
* If not, see <http://www.semidrive.com/licenses/>.
*/
#ifndef __SDRV_CRC_H__
#define __SDRV_CRC_H__

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <asm/io.h>
#include <linux/iommu.h>
#include <linux/wait.h>
#include <uapi/linux/sd-disp-crc-dcmd.h>

#define CRC32_REG_OFFSET    0xE000
#define DISP_CRC_REG_CTRL_SIZE  (0x10000)

struct crc32_block_reg {
	uint32_t crc32_block_ctrl0;
	uint32_t crc32_block_ctrl1;
	uint32_t reserved3;
	uint32_t crc32_block_result_data;
	uint32_t reserved4[4];
};

struct crc32_reg {
	uint32_t crc32_ctrl;
	uint32_t crc32_int_st;
	uint32_t reserved1;
	uint32_t reserved2;
	struct crc32_block_reg crc32_blk[8];
};

struct dc_reg {
	uint32_t reserved_0x0;
	uint32_t dc_flc_ctrl;
};

struct dc_reg_status {
	void __iomem *regs;
	struct crc32_reg  *crc_reg;
	bool  tmp_status[8];
};

enum disp_crc_status {
	   CRC_ERR = 0,
	   CRC_OK = 1
};

struct crc32block {
	uint16_t cfg_start_x;
	uint16_t cfg_start_y;
	uint16_t cfg_end_x;
	uint16_t cfg_end_y;
	uint16_t stride;
};

struct saved_roi_parms{
	struct roi_params saved_roi_params[8];
	int saved_valid_roi_cnt;
};

struct disp_crc {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev cdev;
	struct miscdevice mdev;
	int dc_id;
	struct dc_reg_status dc_reg;
	const char *name;
	const struct disp_crc_ops *ops;
	int du_inited;
	struct saved_roi_parms saved_roi;
	int start_crc;
	struct mutex m_lock;
};

struct disp_crc_ops {
	int (*set_roi)(struct disp_crc *, struct roi_params *, uint32_t );
	int (*start_calc)(struct disp_crc *);
	int (*get_hcrc)(struct disp_crc *, uint32_t , bool *, uint32_t *);
	int (*wait_done)(struct disp_crc *, uint32_t );
	int (*clear_done)(struct disp_crc *);
};

extern const struct disp_crc_ops disp_crc_ops;

#endif //__SDRV_CRC_H__
