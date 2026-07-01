#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_CALL_TAG "\033[1;34m[DASICS-CALL-JR-RETURNPC]\033[0m"
#define DASICS_CALL_BEGIN_TAG "\033[1;31m[DASICS-CALL-JR-RETURNPC]"
#define DASICS_CALL_SUMMARY_TAG "\033[1;32m[DASICS-CALL-JR-RETURNPC]"
#define DASICS_CALL_COLOR_END "\033[0m"

static volatile unsigned long call_jr_returnpc_hits;

static unsigned long read_dasics_return_pc(void)
{
    return csr_read(0x8b1);
}

static __attribute__((noinline, used)) void call_jr_returnpc_target(void)
{
    call_jr_returnpc_hits++;
}

static __attribute__((noinline, used)) unsigned long call_jr_returnpc_invoke(void)
{
    unsigned long expected_return_pc;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, call_jr_returnpc_target\n"
        ".word 0x0005108b\n"
        "1:\n"
        "la %0, 1b\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        : "=r"(expected_return_pc)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    return expected_return_pc;
}

static int run_trusted_call_jr_returnpc_check(unsigned long *total)
{
    unsigned long hits_before = call_jr_returnpc_hits;
    unsigned long expected_return_pc = call_jr_returnpc_invoke();
    unsigned long return_pc = read_dasics_return_pc();
    int pass = call_jr_returnpc_hits == hits_before + 1 &&
               return_pc == expected_return_pc;

    (*total)++;
    printf(DASICS_CALL_TAG " case=CALL-JR-RETURNPC-TRUSTED hits_before=%lu hits_after=%lu return_pc=0x%lx expect=0x%lx result=%s\n",
           hits_before, call_jr_returnpc_hits, return_pc,
           expected_return_pc, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;

    printf(DASICS_CALL_BEGIN_TAG " trusted call jr returnpc smoke begin" DASICS_CALL_COLOR_END "\n");

    failures += run_trusted_call_jr_returnpc_check(&total);

    printf(DASICS_CALL_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_CALL_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
