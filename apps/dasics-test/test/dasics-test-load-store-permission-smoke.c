#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_LOAD_STORE_TAG "\033[1;34m[DASICS-LOAD-STORE-PERMISSION]\033[0m"
#define DASICS_LOAD_STORE_BEGIN_TAG "\033[1;31m[DASICS-LOAD-STORE-PERMISSION]"
#define DASICS_LOAD_STORE_SUMMARY_TAG "\033[1;32m[DASICS-LOAD-STORE-PERMISSION]"
#define DASICS_LOAD_STORE_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_LIB_CFG_WRITE 0x1UL
#define DASICS_LIB_CFG_READ 0x2UL
#define DASICS_LIB_CFG_VALID 0x8UL
#define DASICS_FREASON_LOAD 2UL
#define DASICS_FREASON_STORE 3UL

#define READ_ALLOWED_BYTE 0x5aUL
#define STORE_ALLOWED_BYTE 0x7cUL
#define STORE_DENIED_INITIAL 0x33UL
#define STORE_DENIED_ATTEMPT 0x7dUL
#define SPLIT_READ_VALUE 0x1122334455667788UL
#define PARTIAL_STORE_INITIAL 0x0102030405060708UL
#define PARTIAL_STORE_ATTEMPT 0xaabbccddeeff0011UL
#define FAULT_LOAD_MARKER 0x4c4f41444641554cUL
#define FAULT_STORE_MARKER 0x53544f524641554cUL

static volatile unsigned char load_store_read_allowed_data[8] __attribute__((aligned(8))) = {
    READ_ALLOWED_BYTE,
};
static volatile unsigned char load_store_write_allowed_data[8] __attribute__((aligned(8)));
static volatile unsigned char load_store_load_denied_data[8] __attribute__((aligned(8))) = {
    0x91,
};
static volatile unsigned char load_store_store_denied_data[8] __attribute__((aligned(8))) = {
    STORE_DENIED_INITIAL,
};
static volatile unsigned long load_store_split_read_data __attribute__((aligned(8))) = SPLIT_READ_VALUE;
static volatile unsigned long load_store_partial_store_data __attribute__((aligned(8))) = PARTIAL_STORE_INITIAL;

extern char load_allowed_untrusted_entry[];
extern char store_allowed_untrusted_entry[];
extern char split_read_allowed_untrusted_entry[];
extern char load_denied_untrusted_entry[];
extern char store_denied_untrusted_entry[];
extern char partial_store_denied_untrusted_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global load_allowed_untrusted_entry\n"
".type load_allowed_untrusted_entry, @function\n"
"load_allowed_untrusted_entry:\n"
"  la t0, load_store_read_allowed_data\n"
"  lbu a0, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global store_allowed_untrusted_entry\n"
".type store_allowed_untrusted_entry, @function\n"
"store_allowed_untrusted_entry:\n"
"  la t0, load_store_write_allowed_data\n"
"  li t1, " XSTR(STORE_ALLOWED_BYTE) "\n"
"  sb t1, 0(t0)\n"
"  li a0, " XSTR(STORE_ALLOWED_BYTE) "\n"
"  ret\n"
".balign 8\n"
".global split_read_allowed_untrusted_entry\n"
".type split_read_allowed_untrusted_entry, @function\n"
"split_read_allowed_untrusted_entry:\n"
"  la t0, load_store_split_read_data\n"
"  ld a0, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global load_denied_untrusted_entry\n"
".type load_denied_untrusted_entry, @function\n"
"load_denied_untrusted_entry:\n"
"  li a0, " XSTR(FAULT_LOAD_MARKER) "\n"
"  la t0, load_store_load_denied_data\n"
"  lbu t1, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global store_denied_untrusted_entry\n"
".type store_denied_untrusted_entry, @function\n"
"store_denied_untrusted_entry:\n"
"  li a0, " XSTR(FAULT_STORE_MARKER) "\n"
"  la t0, load_store_store_denied_data\n"
"  li t1, " XSTR(STORE_DENIED_ATTEMPT) "\n"
"  sb t1, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global partial_store_denied_untrusted_entry\n"
".type partial_store_denied_untrusted_entry, @function\n"
"partial_store_denied_untrusted_entry:\n"
"  li a0, " XSTR(FAULT_STORE_MARKER) "\n"
"  la t0, load_store_partial_store_data\n"
"  li t1, " XSTR(PARTIAL_STORE_ATTEMPT) "\n"
"  sd t1, 0(t0)\n"
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

DEFINE_DASICS_CALL_INVOKER(invoke_load_allowed, load_allowed_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_store_allowed, store_allowed_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_split_read_allowed, split_read_allowed_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_load_denied, load_denied_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_store_denied, store_denied_untrusted_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_partial_store_denied, partial_store_denied_untrusted_entry)

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

static void configure_split_read_slots(const volatile void *base)
{
    unsigned long addr = (unsigned long)base;
    unsigned long cfg = lib_cfg_slot(0, DASICS_LIB_CFG_VALID | DASICS_LIB_CFG_READ) |
                        lib_cfg_slot(1, DASICS_LIB_CFG_VALID | DASICS_LIB_CFG_READ);

    csr_write(0x890, addr);
    csr_write(0x891, addr + 4);
    csr_write(0x892, addr + 4);
    csr_write(0x893, addr + 8);
    csr_write(0x880, cfg);
    csr_write(0x8b3, 0);
}

static void configure_partial_store_slot(const volatile void *base)
{
    unsigned long addr = (unsigned long)base;

    csr_write(0x890, addr);
    csr_write(0x891, addr + 4);
    csr_write(0x880, lib_cfg_slot(0, DASICS_LIB_CFG_VALID | DASICS_LIB_CFG_WRITE));
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
    printf(DASICS_LOAD_STORE_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(const char *case_name, unsigned long expect,
                               unsigned long *total)
{
    unsigned long freason = read_dasics_freason();
    int pass = freason == expect;

    (*total)++;
    printf(DASICS_LOAD_STORE_TAG " case=%s freason=0x%lx expect=0x%lx result=%s\n",
           case_name, freason, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;
    unsigned long result;

    printf(DASICS_LOAD_STORE_BEGIN_TAG " load store permission smoke begin" DASICS_LOAD_STORE_COLOR_END "\n");

    configure_slot0(load_store_read_allowed_data, sizeof(load_store_read_allowed_data),
                    DASICS_LIB_CFG_READ);
    result = invoke_load_allowed();
    failures += record_value_case("LOAD-STORE-PERMISSION-ALLOW-READ-BOUND",
                                  result, READ_ALLOWED_BYTE, &total);
    failures += record_freason_case("LOAD-STORE-PERMISSION-ALLOW-READ-FREASON",
                                    0, &total);

    load_store_write_allowed_data[0] = 0;
    configure_slot0(load_store_write_allowed_data, sizeof(load_store_write_allowed_data),
                    DASICS_LIB_CFG_WRITE);
    result = invoke_store_allowed();
    failures += record_value_case("LOAD-STORE-PERMISSION-ALLOW-WRITE-BOUND",
                                  result, STORE_ALLOWED_BYTE, &total);
    failures += record_value_case("LOAD-STORE-PERMISSION-ALLOW-WRITE-DATA",
                                  load_store_write_allowed_data[0],
                                  STORE_ALLOWED_BYTE, &total);
    failures += record_freason_case("LOAD-STORE-PERMISSION-ALLOW-WRITE-FREASON",
                                    0, &total);

    configure_split_read_slots(&load_store_split_read_data);
    result = invoke_split_read_allowed();
    failures += record_value_case("LOAD-STORE-PERMISSION-ALLOW-SPLIT-READ-BOUND",
                                  result, SPLIT_READ_VALUE, &total);
    failures += record_freason_case("LOAD-STORE-PERMISSION-ALLOW-SPLIT-READ-FREASON",
                                    0, &total);

    clear_lib_permission_csrs();
    result = invoke_load_denied();
    failures += record_value_case("LOAD-STORE-PERMISSION-FAULT-LOAD-NOBOUND",
                                  result, FAULT_LOAD_MARKER, &total);
    failures += record_freason_case("LOAD-STORE-PERMISSION-FREASON-LOAD",
                                    DASICS_FREASON_LOAD, &total);

    load_store_store_denied_data[0] = STORE_DENIED_INITIAL;
    clear_lib_permission_csrs();
    result = invoke_store_denied();
    failures += record_value_case("LOAD-STORE-PERMISSION-FAULT-STORE-NOBOUND",
                                  result, FAULT_STORE_MARKER, &total);
    failures += record_value_case("LOAD-STORE-PERMISSION-FAULT-STORE-DATA-PRESERVE",
                                  load_store_store_denied_data[0],
                                  STORE_DENIED_INITIAL, &total);
    failures += record_freason_case("LOAD-STORE-PERMISSION-FREASON-STORE",
                                    DASICS_FREASON_STORE, &total);

    load_store_partial_store_data = PARTIAL_STORE_INITIAL;
    configure_partial_store_slot(&load_store_partial_store_data);
    result = invoke_partial_store_denied();
    failures += record_value_case("LOAD-STORE-PERMISSION-FAULT-PARTIAL-STORE-BOUND",
                                  result, FAULT_STORE_MARKER, &total);
    failures += record_value_case("LOAD-STORE-PERMISSION-FAULT-PARTIAL-STORE-DATA-PRESERVE",
                                  load_store_partial_store_data,
                                  PARTIAL_STORE_INITIAL, &total);
    failures += record_freason_case("LOAD-STORE-PERMISSION-FREASON-PARTIAL-STORE",
                                    DASICS_FREASON_STORE, &total);

    clear_lib_permission_csrs();

    printf(DASICS_LOAD_STORE_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_LOAD_STORE_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
