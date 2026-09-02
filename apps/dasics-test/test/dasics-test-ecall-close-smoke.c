#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"
#include "udasics.h"

#define DASICS_ECALL_CLOSE_TAG "\033[1;34m[DASICS-ECALL-CLOSE]\033[0m"
#define DASICS_ECALL_CLOSE_BEGIN_TAG "\033[1;31m[DASICS-ECALL-CLOSE]"
#define DASICS_ECALL_CLOSE_SUMMARY_TAG "\033[1;32m[DASICS-ECALL-CLOSE]"
#define DASICS_ECALL_CLOSE_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define UCAS_SYSCALL_GETPID 306UL

extern char untrusted_ecall_close_entry[];

asm (
".option push\n"
".option norvc\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global untrusted_ecall_close_entry\n"
".type untrusted_ecall_close_entry, @function\n"
"untrusted_ecall_close_entry:\n"
"  li a0, 0\n"
"  li a7, " XSTR(UCAS_SYSCALL_GETPID) "\n"
"  ecall\n"
"  ret\n"
".section .text,\"ax\",@progbits\n"
".option pop\n"
);

static unsigned long invoke_untrusted_ecall_close(void)
{
    unsigned long result;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, untrusted_ecall_close_entry\n"
        ".word 0x0005108b\n"
        "mv %0, a0\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        : "=r"(result)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    return result;
}

static unsigned long read_dasics_freason(void)
{
    return csr_read(0x8b3);
}

static int record_positive_case(const char *case_name, unsigned long value,
                                unsigned long *total)
{
    int pass = value > 0;

    (*total)++;
    printf(DASICS_ECALL_CLOSE_TAG " case=%s value=0x%lx expect=positive result=%s\n",
           case_name, value, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(const char *case_name, unsigned long expect,
                               unsigned long *total)
{
    unsigned long freason = read_dasics_freason();
    int pass = freason == expect;

    (*total)++;
    printf(DASICS_ECALL_CLOSE_TAG " case=%s freason=0x%lx expect=0x%lx result=%s\n",
           case_name, freason, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;
    unsigned long result;

    printf(DASICS_ECALL_CLOSE_BEGIN_TAG " ecall close smoke begin" DASICS_ECALL_CLOSE_COLOR_END "\n");

#if DASICS_LINUX_DUAL_EXEC
    failures += dasics_linux_enable_cuet() ==
                (long)(DASICS_UCFG_ENA | DASICS_UCFG_CUET) ? 0 : 1;
#endif
    csr_write(0x8b3, 0);
    result = invoke_untrusted_ecall_close();
    failures += record_positive_case("ECALL-CLOSE-UNTRUSTED-U-CUET-ORDINARY-SYSCALL",
                                     result, &total);
    failures += record_freason_case("ECALL-CLOSE-UNTRUSTED-U-CUET-FREASON",
                                    0, &total);

    printf(DASICS_ECALL_CLOSE_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_ECALL_CLOSE_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
