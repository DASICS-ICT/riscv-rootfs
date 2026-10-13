// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>

#include "fault_recovery_utmod.h"

u64 fault_recovery_fault_load(const u64 *address, u64 *after)
{
	u64 value = READ_ONCE(*address);

	WRITE_ONCE(*after, 1);
	return value;
}
EXPORT_SYMBOL(fault_recovery_fault_load);

int fault_recovery_fault_store(u64 *address, u64 *after)
{
	WRITE_ONCE(*address, 0xd4);
	WRITE_ONCE(*after, 1);
	return 0;
}
EXPORT_SYMBOL(fault_recovery_fault_store);

int fault_recovery_fault_jump(void (*target)(void), u64 *after)
{
	target();
	WRITE_ONCE(*after, 1);
	return 0;
}
EXPORT_SYMBOL(fault_recovery_fault_jump);

asm(
".pushsection .text\n"
".balign 4\n"
".macro fault_recovery_stack_fault name, offset\n"
".global \\name\n"
".type \\name, @function\n"
"\\name:\n"
"addi t0, sp, \\offset\n"
"sd zero, 0(t0)\n"
"li t1, 1\n"
"sd t1, 0(a0)\n"
"li a0, 0\n"
"ret\n"
".size \\name, .-\\name\n"
".endm\n"
"fault_recovery_stack_fault fault_recovery_fault_stack_low, -1024\n"
"fault_recovery_stack_fault fault_recovery_fault_stack_high, 1024\n"
".popsection\n");

EXPORT_SYMBOL(fault_recovery_fault_stack_low);
EXPORT_SYMBOL(fault_recovery_fault_stack_high);

static int __init fault_recovery_utmod_init(void)
{
	return 0;
}

static void __exit fault_recovery_utmod_exit(void)
{
}

module_init(fault_recovery_utmod_init);
module_exit(fault_recovery_utmod_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DASICS fault recovery untrusted fault targets");
