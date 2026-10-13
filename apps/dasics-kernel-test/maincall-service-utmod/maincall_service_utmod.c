// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>

#include "maincall_service_utmod.h"

asm(
".pushsection .text\n"
".balign 4\n"
".macro maincall_service_call name, service, query, last\n"
".global \\name\n"
".type \\name, @function\n"
"\\name:\n"
"addi sp, sp, -16\n"
"sd ra, 8(sp)\n"
"mv t0, a0\n"
"li a0, \\service\n"
"li a1, \\query\n"
"li a2, 0\n"
"li a3, 0\n"
"li a4, 0\n"
"li a5, 0\n"
"li a6, 0\n"
"li a7, \\last\n"
"jalr ra, t0, 0\n"
"ld ra, 8(sp)\n"
"addi sp, sp, 16\n"
"ret\n"
".size \\name, .-\\name\n"
".endm\n"
"maincall_service_call maincall_service_valid_service, 1, 1, 0\n"
".global maincall_service_full_registers\n"
".type maincall_service_full_registers, @function\n"
"maincall_service_full_registers:\n"
"addi sp, sp, -16\n"
"sd ra, 8(sp)\n"
"mv t0, a0\n"
"li a0, 1\n"
"li a1, 2\n"
"li a2, 2\n"
"li a3, 3\n"
"li a4, 4\n"
"li a5, 5\n"
"li a6, 6\n"
"li a7, 7\n"
"jalr ra, t0, 0\n"
"ld ra, 8(sp)\n"
"addi sp, sp, 16\n"
"ret\n"
".size maincall_service_full_registers, .-maincall_service_full_registers\n"
"maincall_service_call maincall_service_unknown_service, 57005, 0, 0\n"
"maincall_service_call maincall_service_bad_arguments, 1, 1, 1\n"
".global maincall_service_invalid_source\n"
".type maincall_service_invalid_source, @function\n"
"maincall_service_invalid_source:\n"
"mv t0, a0\n"
"li a0, 1\n"
"li a1, 1\n"
"li a2, 0\n"
"li a3, 0\n"
"li a4, 0\n"
"li a5, 0\n"
"li a6, 0\n"
"li a7, 0\n"
"li ra, 1\n"
"jr t0\n"
".size maincall_service_invalid_source, .-maincall_service_invalid_source\n"
".global maincall_service_untrusted_stack\n"
".type maincall_service_untrusted_stack, @function\n"
"maincall_service_untrusted_stack:\n"
"addi sp, sp, -32\n"
"sd ra, 24(sp)\n"
"sd s0, 16(sp)\n"
"sd s1, 8(sp)\n"
"mv s0, sp\n"
"mv t0, a0\n"
"mv s1, a1\n"
"li a0, 1\n"
"li a1, 1\n"
"li a2, 0\n"
"li a3, 0\n"
"li a4, 0\n"
"li a5, 0\n"
"li a6, 0\n"
"li a7, 0\n"
"mv sp, s1\n"
"jalr ra, t0, 0\n"
"mv sp, s0\n"
"ld s1, 8(sp)\n"
"ld s0, 16(sp)\n"
"ld ra, 24(sp)\n"
"addi sp, sp, 32\n"
"ret\n"
".size maincall_service_untrusted_stack, .-maincall_service_untrusted_stack\n"
".popsection\n");

EXPORT_SYMBOL(maincall_service_valid_service);
EXPORT_SYMBOL(maincall_service_full_registers);
EXPORT_SYMBOL(maincall_service_unknown_service);
EXPORT_SYMBOL(maincall_service_bad_arguments);
EXPORT_SYMBOL(maincall_service_invalid_source);
EXPORT_SYMBOL(maincall_service_untrusted_stack);

static int __init maincall_service_utmod_init(void)
{
	return 0;
}

static void __exit maincall_service_utmod_exit(void)
{
}

module_init(maincall_service_utmod_init);
module_exit(maincall_service_utmod_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DASICS maincall service untrusted maincall request targets");
