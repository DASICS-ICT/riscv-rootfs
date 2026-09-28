#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_CBO_TAG "\033[1;34m[DASICS-CBO-PERMISSION]\033[0m"
#define DASICS_CBO_BEGIN_TAG "\033[1;31m[DASICS-CBO-PERMISSION]"
#define DASICS_CBO_SUMMARY_TAG "\033[1;32m[DASICS-CBO-PERMISSION]"
#define DASICS_CBO_COLOR_END "\033[0m"

#define DASICS_LIB_CFG_WRITE 0x1UL
#define DASICS_LIB_CFG_READ 0x2UL
#define DASICS_LIB_CFG_VALID 0x8UL
#define DASICS_FREASON_STORE 3UL
#define CBO_LINE 64UL
#define CBO_OK_MARKER 0x43424f4f4b000000UL
#define FAULT_CBO_MARKER 0x43424f4641554c54UL

static volatile unsigned char cbo_line[CBO_LINE] __attribute__((aligned(64)));

extern char cbo_zero_untrusted_entry[];
extern char cbo_inval_untrusted_entry[];
extern char cbo_clean_untrusted_entry[];
extern char cbo_flush_untrusted_entry[];
extern char cbo_zero_denied_untrusted_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global cbo_zero_untrusted_entry\n"
".type cbo_zero_untrusted_entry, @function\n"
"cbo_zero_untrusted_entry:\n"
"  la t0, cbo_line\n"
"  .insn r 0x0f, 2, 0, x0, t0, x4\n"
"  li a0, 0x43424f4f4b000000\n"
"  ret\n"
".balign 8\n"
".global cbo_inval_untrusted_entry\n"
".type cbo_inval_untrusted_entry, @function\n"
"cbo_inval_untrusted_entry:\n"
"  la t0, cbo_line\n"
"  .insn r 0x0f, 2, 0, x0, t0, x0\n"
"  li a0, 0x43424f4f4b000000\n"
"  ret\n"
".balign 8\n"
".global cbo_clean_untrusted_entry\n"
".type cbo_clean_untrusted_entry, @function\n"
"cbo_clean_untrusted_entry:\n"
"  la t0, cbo_line\n"
"  .insn r 0x0f, 2, 0, x0, t0, x1\n"
"  li a0, 0x43424f4f4b000000\n"
"  ret\n"
".balign 8\n"
".global cbo_flush_untrusted_entry\n"
".type cbo_flush_untrusted_entry, @function\n"
"cbo_flush_untrusted_entry:\n"
"  la t0, cbo_line\n"
"  .insn r 0x0f, 2, 0, x0, t0, x2\n"
"  li a0, 0x43424f4f4b000000\n"
"  ret\n"
".balign 8\n"
".global cbo_zero_denied_untrusted_entry\n"
".type cbo_zero_denied_untrusted_entry, @function\n"
"cbo_zero_denied_untrusted_entry:\n"
"  li a0, 0x43424f4641554c54\n"
"  la t0, cbo_line\n"
"  .insn r 0x0f, 2, 0, x0, t0, x4\n"
"  ret\n"
".section .text,\"ax\",@progbits\n"
".option pop\n"
);

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

DEFINE_DASICS_CALL_INVOKER(invoke_cbo_zero, cbo_zero_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_cbo_inval, cbo_inval_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_cbo_clean, cbo_clean_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_cbo_flush, cbo_flush_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_cbo_zero_denied, cbo_zero_denied_untrusted_entry)

static unsigned long lib_cfg_slot(unsigned int index, unsigned long cfg)
{
    return cfg << (index * 4);
}

static void clear_lib_permission_csrs(void)
{
    csr_write(0x880, 0);
    csr_write(0x890, 0);
    csr_write(0x891, 0);
    csr_write(0x8b3, 0);
}

static void configure_slot0(const volatile void *base, unsigned long len,
                            unsigned long access)
{
    csr_write(0x890, (unsigned long)base);
    csr_write(0x891, (unsigned long)base + len);
    csr_write(0x880, lib_cfg_slot(0, DASICS_LIB_CFG_VALID | access));
    csr_write(0x8b3, 0);
}

static unsigned long read_dasics_freason(void)
{
    return csr_read(0x8b3);
}

static int record_value_case(const char *case_name, unsigned long value,
                             unsigned long expect, unsigned long *total)
{
    int pass = value == expect;

    (*total)++;
    printf(DASICS_CBO_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(const char *case_name, unsigned long expect,
                               unsigned long *total)
{
    unsigned long freason = read_dasics_freason();
    int pass = freason == expect;

    (*total)++;
    printf(DASICS_CBO_TAG " case=%s freason=0x%lx expect=0x%lx result=%s\n",
           case_name, freason, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;
    unsigned long result;

    printf(DASICS_CBO_BEGIN_TAG " cbo permission smoke begin" DASICS_CBO_COLOR_END "\n");

    configure_slot0(cbo_line, CBO_LINE, DASICS_LIB_CFG_WRITE);
    result = invoke_cbo_zero();
    failures += record_value_case("CBO-PERMISSION-ALLOW-ZERO-W",
                                  result, CBO_OK_MARKER, &total);
    failures += record_freason_case("CBO-PERMISSION-ALLOW-ZERO-FREASON", 0, &total);

    configure_slot0(cbo_line, CBO_LINE, DASICS_LIB_CFG_WRITE);
    result = invoke_cbo_inval();
    failures += record_value_case("CBO-PERMISSION-ALLOW-INVAL-W",
                                  result, CBO_OK_MARKER, &total);
    failures += record_freason_case("CBO-PERMISSION-ALLOW-INVAL-FREASON", 0, &total);

    configure_slot0(cbo_line, CBO_LINE, DASICS_LIB_CFG_READ);
    result = invoke_cbo_clean();
    failures += record_value_case("CBO-PERMISSION-ALLOW-CLEAN-R",
                                  result, CBO_OK_MARKER, &total);
    failures += record_freason_case("CBO-PERMISSION-ALLOW-CLEAN-FREASON", 0, &total);

    configure_slot0(cbo_line, CBO_LINE, DASICS_LIB_CFG_WRITE);
    result = invoke_cbo_flush();
    failures += record_value_case("CBO-PERMISSION-ALLOW-FLUSH-W",
                                  result, CBO_OK_MARKER, &total);
    failures += record_freason_case("CBO-PERMISSION-ALLOW-FLUSH-FREASON", 0, &total);

    clear_lib_permission_csrs();
    result = invoke_cbo_zero_denied();
    failures += record_value_case("CBO-PERMISSION-FAULT-ZERO-NOBOUND",
                                  result, FAULT_CBO_MARKER, &total);
    failures += record_freason_case("CBO-PERMISSION-FREASON-ZERO-NOBOUND",
                                    DASICS_FREASON_STORE, &total);

    configure_slot0(cbo_line, 32, DASICS_LIB_CFG_WRITE);
    result = invoke_cbo_zero_denied();
    failures += record_value_case("CBO-PERMISSION-FAULT-ZERO-PARTIAL-LINE",
                                  result, FAULT_CBO_MARKER, &total);
    failures += record_freason_case("CBO-PERMISSION-FREASON-ZERO-PARTIAL-LINE",
                                    DASICS_FREASON_STORE, &total);

    clear_lib_permission_csrs();

    printf(DASICS_CBO_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_CBO_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
