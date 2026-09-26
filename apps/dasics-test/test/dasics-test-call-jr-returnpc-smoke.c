#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_CALL_TAG "\033[1;34m[DASICS-CALL-JR-RETURNPC]\033[0m"
#define DASICS_CALL_BEGIN_TAG "\033[1;31m[DASICS-CALL-JR-RETURNPC]"
#define DASICS_CALL_SUMMARY_TAG "\033[1;32m[DASICS-CALL-JR-RETURNPC]"
#define DASICS_CALL_COLOR_END "\033[0m"

/* Imm_DASICSJ maps offset[22:1] to instruction fields 31, 7, 20:15,
 * 11:8 and 30:21. Local label differences need no shifted relocations. */
#define DASICS_CALL_J_WORD(offset) \
    ".word (0x7b | ((((" offset ") >> 22) & 1) << 31)" \
    " | ((((" offset ") >> 21) & 1) << 7)" \
    " | ((((" offset ") >> 15) & 0x3f) << 15)" \
    " | ((((" offset ") >> 11) & 0xf) << 8)" \
    " | ((((" offset ") >> 1) & 0x3ff) << 21))\n"

static volatile unsigned long call_jr_returnpc_hits;

static unsigned long read_dasics_return_pc(void)
{
    return csr_read(0x8b1);
}

static __attribute__((noinline, used)) void call_jr_returnpc_target(void)
{
    call_jr_returnpc_hits++;
}

static __attribute__((noinline, used)) unsigned long call_jr_returnpc_invoke(unsigned long *link)
{
    unsigned long expected_return_pc;
    unsigned long observed_link;

    asm volatile (
        ".option push\n"
        ".option norvc\n"
        ".option norelax\n"
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, call_jr_returnpc_target\n"
        ".word 0x000510fb\n"
        "1:\n"
        "mv %1, ra\n"
        "la %0, 1b\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        ".option pop\n"
        : "=r"(expected_return_pc), "=r"(observed_link)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    *link = observed_link;
    return expected_return_pc;
}

static int run_trusted_call_jr_returnpc_check(unsigned long *total)
{
    unsigned long hits_before = call_jr_returnpc_hits;
    unsigned long link;
    unsigned long expected_return_pc = call_jr_returnpc_invoke(&link);
    unsigned long return_pc = read_dasics_return_pc();
    int pass = call_jr_returnpc_hits == hits_before + 1 &&
               link == expected_return_pc &&
               return_pc == expected_return_pc;

    (*total)++;
    printf(DASICS_CALL_TAG " case=CALL-JR-RETURNPC-TRUSTED hits_before=%lu hits_after=%lu link=0x%lx return_pc=0x%lx expect=0x%lx result=%s\n",
           hits_before, call_jr_returnpc_hits, link, return_pc,
           expected_return_pc, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int run_trusted_call_j_returnpc_check(unsigned long *total, int negative)
{
    unsigned long hits, link, return_pc, expected_return_pc;

    /* Both paths return with ordinary JALR and capture x1 before restoring
     * the caller's x1. The label immediately after the word is its PC + 4. */
    if (!negative) {
        asm volatile (
            ".option push\n.option norvc\n.option norelax\n"
            "addi sp, sp, -16\nsd ra, 8(sp)\n"
            "li %0, 0\n"
            "1:\n"
            DASICS_CALL_J_WORD("3f - 1b")
            "2:\n"
            "mv %1, ra\ncsrr %2, 0x8b1\nla %3, 2b\n"
            "j 4f\n"
            "3:\n"
            "addi %0, %0, 1\njalr zero, 0(ra)\n"
            "4:\n"
            "ld ra, 8(sp)\naddi sp, sp, 16\n"
            ".option pop\n"
            : "=&r"(hits), "=&r"(link), "=&r"(return_pc), "=&r"(expected_return_pc)
            :
            : "ra", "memory");
    } else {
        asm volatile (
            ".option push\n.option norvc\n.option norelax\n"
            "addi sp, sp, -16\nsd ra, 8(sp)\n"
            "li %0, 0\nj 3f\n"
            "1:\n"
            "addi %0, %0, 1\njalr zero, 0(ra)\n"
            "3:\n"
            DASICS_CALL_J_WORD("1b - 3b")
            "2:\n"
            "mv %1, ra\ncsrr %2, 0x8b1\nla %3, 2b\n"
            "ld ra, 8(sp)\naddi sp, sp, 16\n"
            ".option pop\n"
            : "=&r"(hits), "=&r"(link), "=&r"(return_pc), "=&r"(expected_return_pc)
            :
            : "ra", "memory");
    }

    int pass = hits == 1 && link == expected_return_pc &&
               return_pc == expected_return_pc;
    (*total)++;
    printf(DASICS_CALL_TAG " case=CALL-J-RETURNPC-%s hits=%lu link=0x%lx return_pc=0x%lx expect=0x%lx result=%s\n",
           negative ? "NEGATIVE" : "POSITIVE", hits, link, return_pc,
           expected_return_pc, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;

    printf(DASICS_CALL_BEGIN_TAG " trusted call jr returnpc smoke begin" DASICS_CALL_COLOR_END "\n");

    failures += run_trusted_call_jr_returnpc_check(&total);
    failures += run_trusted_call_j_returnpc_check(&total, 0);
    failures += run_trusted_call_j_returnpc_check(&total, 1);

    printf(DASICS_CALL_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_CALL_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
