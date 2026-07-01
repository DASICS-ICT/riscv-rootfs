#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_JUMP_BRANCH_TAG "\033[1;34m[DASICS-JUMP-BRANCH-TARGET-PERMISSION]\033[0m"
#define DASICS_JUMP_BRANCH_BEGIN_TAG "\033[1;31m[DASICS-JUMP-BRANCH-TARGET-PERMISSION]"
#define DASICS_JUMP_BRANCH_SUMMARY_TAG "\033[1;32m[DASICS-JUMP-BRANCH-TARGET-PERMISSION]"
#define DASICS_JUMP_BRANCH_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_JUMP_CFG_VALID(index) (1UL << ((index) * 16))
#define DASICS_FREASON_JUMP 4UL
#define ALLOWED_JUMP_MARKER 0x4a554d50414c4c4f
#define ALLOWED_BRANCH_MARKER 0x4252414e414c4c4f
#define FAULT_JUMP_MARKER 0x4a554d504641554c
#define FAULT_BRANCH_MARKER 0x4252414e4641554c
#define UNEXPECTED_TARGET_MARKER 0x5441524745544558

extern char jump_bound_allowed_untrusted_entry[];
extern char branch_bound_allowed_untrusted_entry[];
extern char jump_trusted_fault_untrusted_entry[];
extern char branch_nobound_fault_untrusted_entry[];
extern char jump_bound_allowed_return_pc[];
extern char jump_branch_allowed_freezone_target[];
extern char jump_branch_allowed_branch_target[];

unsigned long __attribute__((noinline, used)) trusted_illegal_jump_target(void)
{
    return UNEXPECTED_TARGET_MARKER;
}

asm (
".option push\n"
".option norvc\n"
".section .ufreezonetext,\"ax\",@progbits\n"
".balign 8\n"
".global jump_branch_allowed_freezone_target\n"
".type jump_branch_allowed_freezone_target, @function\n"
"jump_branch_allowed_freezone_target:\n"
"  li a0, " XSTR(ALLOWED_JUMP_MARKER) "\n"
"  ret\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global jump_bound_allowed_untrusted_entry\n"
".type jump_bound_allowed_untrusted_entry, @function\n"
"jump_bound_allowed_untrusted_entry:\n"
"  addi sp, sp, -16\n"
"  sd ra, 8(sp)\n"
"  jal ra, jump_branch_allowed_freezone_target\n"
".global jump_bound_allowed_return_pc\n"
"jump_bound_allowed_return_pc:\n"
"  ld ra, 8(sp)\n"
"  addi sp, sp, 16\n"
"  ret\n"
".balign 8\n"
".global branch_bound_allowed_untrusted_entry\n"
".type branch_bound_allowed_untrusted_entry, @function\n"
"branch_bound_allowed_untrusted_entry:\n"
"  li t0, 1\n"
"  beq t0, t0, jump_branch_allowed_branch_target\n"
"  ret\n"
".balign 8\n"
".global jump_branch_allowed_branch_target\n"
"jump_branch_allowed_branch_target:\n"
"  li a0, " XSTR(ALLOWED_BRANCH_MARKER) "\n"
"  ret\n"
".balign 8\n"
".global jump_trusted_fault_untrusted_entry\n"
".type jump_trusted_fault_untrusted_entry, @function\n"
"jump_trusted_fault_untrusted_entry:\n"
"  li a0, " XSTR(FAULT_JUMP_MARKER) "\n"
"  jal ra, trusted_illegal_jump_target\n"
"  ret\n"
".balign 8\n"
".global branch_nobound_fault_untrusted_entry\n"
".type branch_nobound_fault_untrusted_entry, @function\n"
"branch_nobound_fault_untrusted_entry:\n"
"  li t0, 1\n"
"  li a0, " XSTR(FAULT_BRANCH_MARKER) "\n"
"  beq t0, t0, jump_branch_disallowed_branch_target\n"
"  ret\n"
".balign 8\n"
".global jump_branch_disallowed_branch_target\n"
"jump_branch_disallowed_branch_target:\n"
"  li a0, " XSTR(UNEXPECTED_TARGET_MARKER) "\n"
"  ret\n"
".section .text,\"ax\",@progbits\n"
".option pop\n"
);

static unsigned long align_down_8(unsigned long value)
{
    return value & ~7UL;
}

static void clear_jump_branch_target_permission_csrs(void)
{
    csr_write(0x8b2, 0);
    csr_write(0x8c0, 0);
    csr_write(0x8c1, 0);
    csr_write(0x8c2, 0);
    csr_write(0x8c3, 0);
    csr_write(0x8c4, 0);
    csr_write(0x8c5, 0);
    csr_write(0x8c6, 0);
    csr_write(0x8c7, 0);
    csr_write(0x8c8, 0);
}

static void configure_single_jump_bound(char *target)
{
    unsigned long target_lo = align_down_8((unsigned long)target);

    clear_jump_branch_target_permission_csrs();
    csr_write(0x8c0, target_lo);
    csr_write(0x8c1, target_lo + 8);
    csr_write(0x8c8, DASICS_JUMP_CFG_VALID(0));
}

static void configure_allowed_jump_bound_case(void)
{
    configure_single_jump_bound(jump_branch_allowed_freezone_target);
    csr_write(0x8b2, (unsigned long)jump_bound_allowed_return_pc);
    csr_write(0x8b3, 0);
}

static void configure_allowed_branch_bound_case(void)
{
    configure_single_jump_bound(jump_branch_allowed_branch_target);
    csr_write(0x8b3, 0);
}

static void configure_fault_case(void)
{
    clear_jump_branch_target_permission_csrs();
    csr_write(0x8b3, 0);
}

static unsigned long read_dasics_freason(void)
{
    return csr_read(0x8b3);
}

#define DEFINE_DASICS_CALL_INVOKER(function_name, entry_symbol) \
static unsigned long function_name(void) \
{ \
    unsigned long result; \
    asm volatile ( \
        "addi sp, sp, -16\n" \
        "sd ra, 8(sp)\n" \
        "la a0, " #entry_symbol "\n" \
        ".word 0x0005108b\n" \
        "mv %0, a0\n" \
        "ld ra, 8(sp)\n" \
        "addi sp, sp, 16\n" \
        : "=r"(result) \
        : \
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", \
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory"); \
    return result; \
}

DEFINE_DASICS_CALL_INVOKER(invoke_jump_bound_allowed_untrusted_entry,
                           jump_bound_allowed_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_branch_bound_allowed_untrusted_entry,
                           branch_bound_allowed_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_jump_trusted_fault_untrusted_entry,
                           jump_trusted_fault_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_branch_nobound_fault_untrusted_entry,
                           branch_nobound_fault_untrusted_entry)

static int record_value_case(const char *case_name, unsigned long value,
                             unsigned long expect, unsigned long *total)
{
    int pass = value == expect;

    (*total)++;
    printf(DASICS_JUMP_BRANCH_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(unsigned long *total)
{
    unsigned long freason = read_dasics_freason();
    int pass = freason == DASICS_FREASON_JUMP;

    (*total)++;
    printf(DASICS_JUMP_BRANCH_TAG " case=JUMP-BRANCH-TARGET-PERMISSION-FREASON-JUMP read=0x%lx expect=0x%lx result=%s\n",
           freason, DASICS_FREASON_JUMP, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    unsigned long allowed_jump_result;
    unsigned long allowed_branch_result;
    unsigned long fault_jump_result;
    unsigned long fault_branch_result;
    int failures = 0;

    printf(DASICS_JUMP_BRANCH_BEGIN_TAG " jump branch target permission smoke begin" DASICS_JUMP_BRANCH_COLOR_END "\n");

    configure_allowed_jump_bound_case();
    allowed_jump_result = invoke_jump_bound_allowed_untrusted_entry();

    configure_allowed_branch_bound_case();
    allowed_branch_result = invoke_branch_bound_allowed_untrusted_entry();

    configure_fault_case();
    fault_jump_result = invoke_jump_trusted_fault_untrusted_entry();

    configure_fault_case();
    fault_branch_result = invoke_branch_nobound_fault_untrusted_entry();

    failures += record_value_case("JUMP-BRANCH-TARGET-PERMISSION-ALLOW-JUMP-BOUND",
                                  allowed_jump_result, ALLOWED_JUMP_MARKER, &total);
    failures += record_value_case("JUMP-BRANCH-TARGET-PERMISSION-ALLOW-BRANCH-BOUND",
                                  allowed_branch_result, ALLOWED_BRANCH_MARKER, &total);
    failures += record_value_case("JUMP-BRANCH-TARGET-PERMISSION-FAULT-JUMP-TRUSTED",
                                  fault_jump_result, FAULT_JUMP_MARKER, &total);
    failures += record_value_case("JUMP-BRANCH-TARGET-PERMISSION-FAULT-BRANCH-NOBOUND",
                                  fault_branch_result, FAULT_BRANCH_MARKER, &total);
    failures += record_freason_case(&total);

    printf(DASICS_JUMP_BRANCH_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_JUMP_BRANCH_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
