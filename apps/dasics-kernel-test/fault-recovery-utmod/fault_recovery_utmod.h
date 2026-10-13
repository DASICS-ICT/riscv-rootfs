/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _FAULT_RECOVERY_UTMOD_H
#define _FAULT_RECOVERY_UTMOD_H

#include <linux/types.h>

u64 fault_recovery_fault_load(const u64 *address, u64 *after);
int fault_recovery_fault_store(u64 *address, u64 *after);
int fault_recovery_fault_jump(void (*target)(void), u64 *after);
int fault_recovery_fault_stack_low(u64 *after);
int fault_recovery_fault_stack_high(u64 *after);

#endif
