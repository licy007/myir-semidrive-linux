/*
 * sdrv_boardinfo_hwid.h
 *
 *
 * Copyright(c); 2020 Semidrive
 *
 * Author: Alex Chen <qing.chen@semidrive.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __BOARD_INFO_HWID_USR_H__
#define __BOARD_INFO_HWID_USR_H__
#include <linux/types.h>

#define HW_ID_MAGIC 0x5
enum sd_chipid_e {//5 bit
	CHIPID_UNKNOWN,
	CHIPID_X9E,
	CHIPID_X9M,
	CHIPID_X9H,
	CHIPID_X9P,
	CHIPID_X9U,
	CHIPID_G9S,
	CHIPID_G9X,
	CHIPID_G9E,
	CHIPID_V9L,
	CHIPID_V9T,
	CHIPID_V9F,
	CHIPID_D9,
	CHIPID_D9L,
	CHIPID_D9P,
	CHIPID_V9M,
	CHIPID_V9M9156,
	CHIPID_G9V,
	CHIPID_G9Q,
	CHIPID_X9M9474,
	CHIPID_X9HP,
	CHIPID_X9HP9699,
	CHIPID_X9HP9698,
	CHIPID_V9T9752,
	CHIPID_X9U9892,
};

enum sd_board_type_e {//2 bit
	BOARD_TYPE_UNKNOWN,
	BOARD_TYPE_EVB,
	BOARD_TYPE_REF,
	BOARD_TYPE_MS,
};

enum sd_boardid_major_e {//3 bit
	BOARDID_MAJOR_UNKNOWN,
	BOARDID_MAJOR_A,
	BOARDID_MAJOR_G9A
};
enum sd_boardid_ms_major_e {//3 bit
	BOARDID_MAJOR_MPS = 1,
	BOARDID_MAJOR_TI_A01,
	BOARDID_MAJOR_TI_A02,
};
enum sd_boardid_ms_minor_e {//4 bit
	BOARDID_MINOR_UNKNOWN,
	BOARDID_MINOR_
};

struct version1 {//24 bit
	u32 chipid: 5; //chip version
	u32 featurecode: 2; //feature code
	u32 speed_grade: 2; //
	u32 temp_grade: 2; //
	u32 pkg_type: 2; //
	u32 revision: 2; //
	u32 board_type: 2; //
	u32 board_id_major: 3; //major,in ms board, this will be core boardid,
	u32 board_id_minor: 4; //minor,in ms board, this will be base boardid,
} __attribute__((packed));
union version {
	struct version1 v1;
	//struct version1_1 v1_1;
};

// on system for user
struct sd_hwid_usr {
	u32 magic: 4; // must be 0x5
	u32 ver: 4; // encode version
	union version v;
} __attribute__((packed));


enum part_e {
	PART_CHIPID,
	PART_FEATURE,
	PART_SPEED,
	PART_TEMP,
	PART_PKG,
	PART_REV,
	PART_BOARD_TYPE,
	PART_BOARD_ID_MAJ,
	PART_BOARD_ID_MIN,
};

//user api
struct sd_hwid_usr get_fullhw_id(void);
int get_part_id(enum part_e part);
void clear_local_hwid(void);
void dump_all_part_id(void);
#endif
