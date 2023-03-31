/*
 * Copyright (C) 2020 Semidrive Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU Lesser General
 * Public License.  You may obtain a copy of the GNU Lesser General
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 *
 * @file linux/sd_rpmsg.h
 *
 * @brief Global header file for semidrive RPMSG
 *
 * @ingroup RPMSG
 */
#ifndef __LINUX_SD_RPMSG_H__
#define __LINUX_SD_RPMSG_H__

/* Category define */

/* rpmsg version */
#define SD_RMPSG_MAJOR		1
#define SD_RMPSG_MINOR		0

enum sd_rpmsg_rprocs {
	SD_RPROC_SAF,
	SD_RPROC_SEC,
	SD_RPROC_MPC,
	SD_RPROC_AP1,
	SD_RPROC_AP2,
};

int sd_rpmsg_register_nb(const char *name, struct notifier_block *nb);
int sd_rpmsg_unregister_nb(const char *name, struct notifier_block *nb);
#endif /* __LINUX_SD_RPMSG_H__ */
