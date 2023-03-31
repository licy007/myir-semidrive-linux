/*
 *        LICENSE:
 *        (C)Copyright 2021-2022 Marvell.
 *
 *        All Rights Reserved
 *
 *        THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF MARVELL
 *        The copyright notice above does not evidence any
 *        actual or intended publication of such source code.
 *
 *        This Module contains Proprietary Information of Marvell
 *        and should be treated as Confidential.
 *
 *        The information in this file is provided for the exclusive use of
 *        the licensees of Marvell. Such users have the right to use, modify,
 *        and incorporate this code into products for purposes authorized by
 *        the license agreement provided they include this notice and the
 *        associated copyright notice with any such product.
 *
 *        The information in this file is provided "AS IS" without warranty.
 *
 * Contents: DPM (Device Power Management) interface functions as called from Linux kernel
 *
 * Author: Naresh Bhat
 * Date  : 2021-06-30
 *  */

#if CONFIG_PM
#ifndef H_OAK_DPM
#define H_OAK_DPM

#include <linux/pm_runtime.h>
#include "oak_unimac.h"

/* Name        : oak_dpm_create_sysfs
 * Returns     : void
 * Parameters  : oak_t *np
 * Description : This function creates sysfs entry for setting device power
 *               states D0, D1, D2 and D3.
 * */
void oak_dpm_create_sysfs(oak_t *np);

/* Name        : oak_dpm_remove_sysfs
 * Returns     : void
 * Parameters  : oak_t *np
 * Description : This function removes sysfs entry of device power states
 * */
void oak_dpm_remove_sysfs(oak_t *np);

#if CONFIG_PM_SLEEP

/* Name        : oak_dpm_suspend
 * Returns     : int
 * Parameters  : struct device* dev
 * Description : This function is called when system goes into suspend state
 *               It puts the device into sleep state
 * */
int __maybe_unused oak_dpm_suspend(struct device* dev);

/* Name        : oak_dpm_resume
 * Returns     : int
 * Parameters  : struct device* dev
 * Description : This function called when system goes into resume state and put
 *               the device into active state
 * */
int __maybe_unused oak_dpm_resume(struct device* dev);

/* Name        : oak_dpm_set_power_state
 * Returns     : void
 * Parameters  : struct device *dev, pci_power_t state
 * Description : This function set the device power state
 * */
void oak_dpm_set_power_state(struct device *dev, pci_power_t state);

#endif /* End of PM_SLEEP */
#endif /* End of H_OAK_DPM */
#endif /* End of PM */
