/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYS_COUNTER_H_
#define _SYS_COUNTER_H_

uint32_t semidrive_get_syscntr(void);
uint64_t semidrive_get_syscntr_64(void);
uint32_t semidrive_get_syscntr_freq(void);
uint64_t semidrive_get_systime_us(void);

#endif /* _SYS_COUNTER_H_ */
