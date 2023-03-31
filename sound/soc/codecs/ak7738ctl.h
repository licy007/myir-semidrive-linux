 /* ak7738ctl.h
 *
 * Copyright (C) 2015 Asahi Kasei Microdevices Corporation.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *      15/04/24	     1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DSP_AK7738_H__
#define __DSP_AK7738_H__

/* IO CONTROL definition of AK7738 */
#define AK7738_IOCTL_MAGIC     's'

#define AK7738_MAGIC	0xD0
#define MAX_WREG		32
#define MAX_WCRAM		48

enum ak7738_ram_type {
	RAMTYPE_PRAM = 0,
	RAMTYPE_CRAM,
	RAMTYPE_OFREG,
};

enum ak7738_status {
	POWERDOWN = 0,
	SUSPEND,
	STANDBY,
	DOWNLOAD,
	DOWNLOAD_FINISH,
	RUN1,
	RUN2,
	RUN3,
};

typedef struct _REG_CMD {
	unsigned char addr;
	unsigned char data;
} REG_CMD;

struct ak7738_wreg_handle {
	REG_CMD *regcmd;
	int len;
};

struct ak7738_wcram_handle{
	int    dsp;
	int    addr;
	unsigned char *cram;
	int    len;
};

#define AK7738_IOCTL_SETSTATUS	_IOW(AK7738_MAGIC, 0x10, int)
#define AK7738_IOCTL_SETMIR		_IOW(AK7738_MAGIC, 0x12, int)
#define AK7738_IOCTL_GETMIR		_IOR(AK7738_MAGIC, 0x13, unsigned long)
#define AK7738_IOCTL_WRITEREG	_IOW(AK7738_MAGIC, 0x14, struct ak7738_wreg_handle)
#define AK7738_IOCTL_WRITECRAM	_IOW(AK7738_MAGIC, 0x15, struct ak7738_wcram_handle)

#endif

