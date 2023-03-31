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
#ifndef __G2D_COMMON_H
#define __G2D_COMMON_H

/*LEN:"1"means one pair of 32-bit register addr + 32-bit register value*/
#define G2D_CMDFILE_MAX_LEN		512
#define G2D_CMDFILE_MAX_NUM		10

#define G2D_CMDFILE_MAX_MEM		(G2D_CMDFILE_MAX_LEN * G2D_CMDFILE_MAX_NUM * 2)
#define G2D_CMDFILE_SECTION_LEN (G2D_CMDFILE_MAX_LEN * 2)

#define G2D_INT_MASK_RDMA       BIT(0)
#define G2D_INT_MASK_RLE0       BIT(1)
#define G2D_INT_MASK_MLC        BIT(2)
#define G2D_INT_MASK_RLE1       BIT(3)
#define G2D_INT_MASK_WDMA       BIT(4)
#define G2D_INT_MASK_TASK_DONE  BIT(5)
#define G2D_INT_MASK_FRM_DONE   BIT(6)

#define PIPE_MAX 8

typedef struct {
	unsigned char dirty;
	unsigned int len;
	unsigned int *arg;
} cmdfile_info;

enum {
	G2D_GPIPE0 = 0,
	G2D_GPIPE1 = 1,
	G2D_GPIPE2 = 2,
	G2D_SPIPE0 = 3,
	G2D_SPIPE1 = 4,
};

enum {
	G2D_FUN_BLEND	 = 0b001,
	G2D_FUN_ROTATION = 0b010,
};

enum {
	G2D_CPU_WRITE,
	G2D_CMD_WRITE,
};

static inline unsigned int get_l_addr(unsigned long addr)
{
	unsigned int addr_l;

	addr_l = (unsigned int)(addr & 0x00000000FFFFFFFF);
	return addr_l;
}

static inline unsigned int get_h_addr(unsigned long addr)
{
	unsigned int addr_h;

	addr_h = (unsigned int)((addr & 0xFFFFFFFF00000000) >> 32);
	return addr_h;
}

static inline unsigned long
	get_full_addr(unsigned int haddr, unsigned int laddr)
{
	unsigned long addr;

	addr = (unsigned long)haddr;
	addr = (addr << 32) | (laddr & 0xFFFFFFFF);

	return addr;
}

static inline unsigned long
	get_real_addr(unsigned long input_addr)
{
	unsigned long addr;

	addr = input_addr;

	return addr;
}

static inline int g2d_cwrite(cmdfile_info *cmd_info, int cmd_id,
		unsigned int reg, unsigned int value)
{
	cmdfile_info *cmd = &cmd_info[cmd_id];

	if (cmd_id >= G2D_CMDFILE_MAX_NUM) {
		pr_err("[g2d] cmd_id too big %d > %d\n",
			cmd_id, G2D_CMDFILE_MAX_NUM);
		return -1;
	}

	cmd->arg[cmd->len++] = reg;
	cmd->arg[cmd->len++] = value;

	if (!cmd->dirty)
		cmd->dirty = 1;

	return 0;
}
static inline void pipe_write_reg(cmdfile_info *cmd_info, int cmdid,
	 unsigned char pipe_id, unsigned int reg, unsigned int value)
{
	unsigned long reg_offset;

	reg_offset = pipe_id * 0x1000 + 0x2000;
	g2d_cwrite(cmd_info, cmdid, reg_offset + reg, value);
}

#endif
