#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_AMO_TAG "\033[1;34m[DASICS-AMO-PERMISSION]\033[0m"
#define DASICS_AMO_BEGIN_TAG "\033[1;31m[DASICS-AMO-PERMISSION]"
#define DASICS_AMO_SUMMARY_TAG "\033[1;32m[DASICS-AMO-PERMISSION]"
#define DASICS_AMO_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_LIB_CFG_WRITE 0x1UL
#define DASICS_LIB_CFG_READ 0x2UL
#define DASICS_LIB_CFG_VALID 0x8UL
#define DASICS_FREASON_LOAD 2UL
#define DASICS_FREASON_STORE 3UL

#define AMO_INITIAL 0x1111111111111111UL
#define AMO_ADDEND 0x10UL
#define AMO_SC_VALUE 0x2222222222222222UL
#define FAULT_LR_MARKER 0x4c524641554c5400UL
#define FAULT_SC_MARKER 0x53434641554c5400UL
#define FAULT_AMO_MARKER 0x414d4f4641554c54UL

static volatile unsigned long amo_rw_data __attribute__((aligned(8))) = AMO_INITIAL;
static volatile unsigned long amo_denied_data __attribute__((aligned(8))) = AMO_INITIAL;

extern char amo_lr_untrusted_entry[];
extern char amo_sc_untrusted_entry[];
extern char amo_add_untrusted_entry[];
extern char amo_lr_denied_untrusted_entry[];
extern char amo_sc_denied_untrusted_entry[];
extern char amo_add_denied_untrusted_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global amo_lr_untrusted_entry\n"
".type amo_lr_untrusted_entry, @function\n"
"amo_lr_untrusted_entry:\n"
"  la t0, amo_rw_data\n"
"  .insn r 0x2f, 3, 8, a0, t0, x0\n"
"  ret\n"
".balign 8\n"
".global amo_sc_untrusted_entry\n"
".type amo_sc_untrusted_entry, @function\n"
"amo_sc_untrusted_entry:\n"
"  la t0, amo_rw_data\n"
"  .insn r 0x2f, 3, 8, a0, t0, x0\n"
"  li t1, " XSTR(AMO_SC_VALUE) "\n"
"  .insn r 0x2f, 3, 12, a0, t0, t1\n"
"  ret\n"
".balign 8\n"
".global amo_add_untrusted_entry\n"
".type amo_add_untrusted_entry, @function\n"
"amo_add_untrusted_entry:\n"
"  la t0, amo_rw_data\n"
"  li t1, " XSTR(AMO_ADDEND) "\n"
"  .insn r 0x2f, 3, 0, a0, t0, t1\n"
"  ret\n"
".balign 8\n"
".global amo_lr_denied_untrusted_entry\n"
".type amo_lr_denied_untrusted_entry, @function\n"
"amo_lr_denied_untrusted_entry:\n"
"  li a0, " XSTR(FAULT_LR_MARKER) "\n"
"  la t0, amo_denied_data\n"
"  .insn r 0x2f, 3, 8, t1, t0, x0\n"
"  ret\n"
".balign 8\n"
".global amo_sc_denied_untrusted_entry\n"
".type amo_sc_denied_untrusted_entry, @function\n"
"amo_sc_denied_untrusted_entry:\n"
"  li a0, " XSTR(FAULT_SC_MARKER) "\n"
"  la t0, amo_denied_data\n"
"  li t1, " XSTR(AMO_SC_VALUE) "\n"
"  .insn r 0x2f, 3, 12, t2, t0, t1\n"
"  ret\n"
".balign 8\n"
".global amo_add_denied_untrusted_entry\n"
".type amo_add_denied_untrusted_entry, @function\n"
"amo_add_denied_untrusted_entry:\n"
"  li a0, " XSTR(FAULT_AMO_MARKER) "\n"
"  la t0, amo_denied_data\n"
"  li t1, " XSTR(AMO_ADDEND) "\n"
"  .insn r 0x2f, 3, 0, t2, t0, t1\n"
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

DEFINE_DASICS_CALL_INVOKER(invoke_amo_lr, amo_lr_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_amo_sc, amo_sc_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_amo_add, amo_add_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_amo_lr_denied, amo_lr_denied_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_amo_sc_denied, amo_sc_denied_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_amo_add_denied, amo_add_denied_untrusted_entry)

static unsigned long lib_cfg_slot(unsigned int index, unsigned long cfg)
{
    return cfg << (index * 4);
}

static void clear_lib_permission_csrs(void)
{
    csr_write(0x880, 0);
    csr_write(0x890, 0);
    csr_write(0x891, 0);
    csr_write(0x892, 0);
    csr_write(0x893, 0);
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

static void configure_split_rw_slots(const volatile void *base)
{
    unsigned long addr = (unsigned long)base;
    unsigned long cfg = lib_cfg_slot(0, DASICS_LIB_CFG_VALID | DASICS_LIB_CFG_READ) |
                        lib_cfg_slot(1, DASICS_LIB_CFG_VALID | DASICS_LIB_CFG_WRITE);

    csr_write(0x890, addr);
    csr_write(0x891, addr + 8);
    csr_write(0x892, addr);
    csr_write(0x893, addr + 8);
    csr_write(0x880, cfg);
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
    printf(DASICS_AMO_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(const char *case_name, unsigned long expect,
                               unsigned long *total)
{
    unsigned long freason = read_dasics_freason();
    int pass = freason == expect;

    (*total)++;
    printf(DASICS_AMO_TAG " case=%s freason=0x%lx expect=0x%lx result=%s\n",
           case_name, freason, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;
    unsigned long result;

    printf(DASICS_AMO_BEGIN_TAG " amo permission smoke begin" DASICS_AMO_COLOR_END "\n");

    amo_rw_data = AMO_INITIAL;
    configure_slot0(&amo_rw_data, sizeof(amo_rw_data),
                    DASICS_LIB_CFG_READ | DASICS_LIB_CFG_WRITE);
    result = invoke_amo_lr();
    failures += record_value_case("AMO-PERMISSION-ALLOW-LR-RW-BOUND",
                                  result, AMO_INITIAL, &total);
    failures += record_freason_case("AMO-PERMISSION-ALLOW-LR-FREASON", 0, &total);

    amo_rw_data = AMO_INITIAL;
    configure_slot0(&amo_rw_data, sizeof(amo_rw_data),
                    DASICS_LIB_CFG_READ | DASICS_LIB_CFG_WRITE);
    result = invoke_amo_sc();
    failures += record_value_case("AMO-PERMISSION-ALLOW-SC-STATUS", result, 0, &total);
    failures += record_value_case("AMO-PERMISSION-ALLOW-SC-DATA",
                                  amo_rw_data, AMO_SC_VALUE, &total);
    failures += record_freason_case("AMO-PERMISSION-ALLOW-SC-FREASON", 0, &total);

    amo_rw_data = AMO_INITIAL;
    configure_slot0(&amo_rw_data, sizeof(amo_rw_data),
                    DASICS_LIB_CFG_READ | DASICS_LIB_CFG_WRITE);
    result = invoke_amo_add();
    failures += record_value_case("AMO-PERMISSION-ALLOW-RMW-OLD",
                                  result, AMO_INITIAL, &total);
    failures += record_value_case("AMO-PERMISSION-ALLOW-RMW-DATA",
                                  amo_rw_data, AMO_INITIAL + AMO_ADDEND, &total);
    failures += record_freason_case("AMO-PERMISSION-ALLOW-RMW-FREASON", 0, &total);

    amo_denied_data = AMO_INITIAL;
    clear_lib_permission_csrs();
    result = invoke_amo_lr_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-LR-NOBOUND",
                                  result, FAULT_LR_MARKER, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-LR-NOBOUND",
                                    DASICS_FREASON_LOAD, &total);

    amo_denied_data = AMO_INITIAL;
    clear_lib_permission_csrs();
    result = invoke_amo_sc_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-SC-NOBOUND",
                                  result, FAULT_SC_MARKER, &total);
    failures += record_value_case("AMO-PERMISSION-FAULT-SC-DATA-PRESERVE",
                                  amo_denied_data, AMO_INITIAL, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-SC-NOBOUND",
                                    DASICS_FREASON_STORE, &total);

    amo_denied_data = AMO_INITIAL;
    clear_lib_permission_csrs();
    result = invoke_amo_add_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-RMW-NOBOUND",
                                  result, FAULT_AMO_MARKER, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-RMW-NOBOUND",
                                    DASICS_FREASON_STORE, &total);

    amo_denied_data = AMO_INITIAL;
    configure_slot0(&amo_denied_data, sizeof(amo_denied_data), DASICS_LIB_CFG_WRITE);
    result = invoke_amo_lr_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-LR-W-ONLY",
                                  result, FAULT_LR_MARKER, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-LR-W-ONLY",
                                    DASICS_FREASON_LOAD, &total);

    amo_denied_data = AMO_INITIAL;
    configure_slot0(&amo_denied_data, sizeof(amo_denied_data), DASICS_LIB_CFG_READ);
    result = invoke_amo_sc_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-SC-R-ONLY",
                                  result, FAULT_SC_MARKER, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-SC-R-ONLY",
                                    DASICS_FREASON_STORE, &total);

    amo_denied_data = AMO_INITIAL;
    configure_slot0(&amo_denied_data, sizeof(amo_denied_data), DASICS_LIB_CFG_READ);
    result = invoke_amo_add_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-RMW-R-ONLY",
                                  result, FAULT_AMO_MARKER, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-RMW-R-ONLY",
                                    DASICS_FREASON_STORE, &total);

    amo_denied_data = AMO_INITIAL;
    configure_slot0(&amo_denied_data, sizeof(amo_denied_data), DASICS_LIB_CFG_WRITE);
    result = invoke_amo_add_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-RMW-W-ONLY",
                                  result, FAULT_AMO_MARKER, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-RMW-W-ONLY",
                                    DASICS_FREASON_STORE, &total);

    amo_denied_data = AMO_INITIAL;
    configure_split_rw_slots(&amo_denied_data);
    result = invoke_amo_add_denied();
    failures += record_value_case("AMO-PERMISSION-FAULT-RMW-SPLIT-BOUND",
                                  result, FAULT_AMO_MARKER, &total);
    failures += record_freason_case("AMO-PERMISSION-FREASON-RMW-SPLIT-BOUND",
                                    DASICS_FREASON_STORE, &total);

    clear_lib_permission_csrs();

    printf(DASICS_AMO_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_AMO_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
