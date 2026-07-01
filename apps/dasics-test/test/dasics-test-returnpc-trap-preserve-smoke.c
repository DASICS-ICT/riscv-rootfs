#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_RETURNPC_TRAP_TAG "\033[1;34m[DASICS-RETURNPC-TRAP-PRESERVE]\033[0m"
#define DASICS_RETURNPC_TRAP_BEGIN_TAG "\033[1;31m[DASICS-RETURNPC-TRAP-PRESERVE]"
#define DASICS_RETURNPC_TRAP_SUMMARY_TAG "\033[1;32m[DASICS-RETURNPC-TRAP-PRESERVE]"
#define DASICS_RETURNPC_TRAP_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_FREASON_LOAD 2UL
#define RETURNPC_TRAP_FAULT_MARKER 0x5250545241464c54UL
#define RETURNPC_TRAP_SECOND_MARKER 0x525054525345434eUL

static volatile unsigned long returnpc_trap_denied_data __attribute__((aligned(8))) = 0x1020304050607080UL;

extern char returnpc_trap_fault_entry[];
extern char returnpc_trap_second_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global returnpc_trap_fault_entry\n"
".type returnpc_trap_fault_entry, @function\n"
"returnpc_trap_fault_entry:\n"
"  li a0, " XSTR(RETURNPC_TRAP_FAULT_MARKER) "\n"
"  la t0, returnpc_trap_denied_data\n"
"  ld t1, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global returnpc_trap_second_entry\n"
".type returnpc_trap_second_entry, @function\n"
"returnpc_trap_second_entry:\n"
"  li a0, " XSTR(RETURNPC_TRAP_SECOND_MARKER) "\n"
"  ret\n"
".section .text,\"ax\",@progbits\n"
".option pop\n"
);

static void clear_load_permission_state(void)
{
    csr_write(0x880, 0);
    csr_write(0x890, 0);
    csr_write(0x891, 0);
    csr_write(0x8b3, 0);
}

static unsigned long read_return_pc(void)
{
    return csr_read(0x8b1);
}

static unsigned long read_freason(void)
{
    return csr_read(0x8b3);
}

static unsigned long invoke_returnpc_trap_fault(unsigned long *expected_return_pc)
{
    unsigned long result;
    unsigned long expected;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, returnpc_trap_fault_entry\n"
        ".word 0x0005108b\n"
        "1:\n"
        "mv %0, a0\n"
        "la %1, 1b\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        : "=r"(result), "=r"(expected)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    *expected_return_pc = expected;
    return result;
}

static unsigned long invoke_returnpc_trap_second(unsigned long *expected_return_pc)
{
    unsigned long result;
    unsigned long expected;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, returnpc_trap_second_entry\n"
        ".word 0x0005108b\n"
        "1:\n"
        "mv %0, a0\n"
        "la %1, 1b\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        : "=r"(result), "=r"(expected)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    *expected_return_pc = expected;
    return result;
}

static int record_value_case(const char *case_name, unsigned long value,
                             unsigned long expect, unsigned long *total)
{
    int pass = value == expect;

    (*total)++;
    printf(DASICS_RETURNPC_TRAP_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    unsigned long expected_return_pc = 0;
    unsigned long result;
    int failures = 0;

    printf(DASICS_RETURNPC_TRAP_BEGIN_TAG " DASICS returnpc trap preserve smoke begin" DASICS_RETURNPC_TRAP_COLOR_END "\n");

    clear_load_permission_state();
    result = invoke_returnpc_trap_fault(&expected_return_pc);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-LOAD-FAULT-MARKER",
                                  result, RETURNPC_TRAP_FAULT_MARKER, &total);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-FREASON-LOAD",
                                  read_freason(), DASICS_FREASON_LOAD, &total);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-AFTER-FAULT",
                                  read_return_pc(), expected_return_pc, &total);

    result = invoke_returnpc_trap_second(&expected_return_pc);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-SECOND-CALL",
                                  result, RETURNPC_TRAP_SECOND_MARKER, &total);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-AFTER-SECOND-CALL",
                                  read_return_pc(), expected_return_pc, &total);

    printf(DASICS_RETURNPC_TRAP_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_RETURNPC_TRAP_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
