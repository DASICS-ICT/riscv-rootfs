#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_DYNAMIC_BOUND_TAG "\033[1;34m[DASICS-DYNAMIC-BOUND-FREE]\033[0m"
#define DASICS_DYNAMIC_BOUND_BEGIN_TAG "\033[1;31m[DASICS-DYNAMIC-BOUND-FREE]"
#define DASICS_DYNAMIC_BOUND_SUMMARY_TAG "\033[1;32m[DASICS-DYNAMIC-BOUND-FREE]"
#define DASICS_DYNAMIC_BOUND_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_LIB_CFG_WRITE 0x1UL
#define DASICS_LIB_CFG_READ 0x2UL
#define DASICS_LIB_CFG_VALID 0x8UL
#define DASICS_LIB_CFG_MASK 0xfUL
#define DASICS_LIB_CFG_SLOT_BITS 4UL
#define DASICS_LIB_ENTRY_NUM 16
#define DASICS_FREASON_LOAD 2UL
#define DASICS_FREASON_STORE 3UL

#define READ_ALLOWED_BYTE 0x6dUL
#define STORE_ALLOWED_BYTE 0x71UL
#define STORE_INITIAL_BYTE 0x29UL
#define STORE_AFTER_FREE_ATTEMPT 0x72UL
#define FAULT_LOAD_MARKER 0x44594e424c464c54UL
#define FAULT_STORE_MARKER 0x44594e4253464c54UL

static volatile unsigned char dynamic_read_data[8] __attribute__((aligned(8))) = {
    READ_ALLOWED_BYTE,
};
static volatile unsigned char dynamic_write_data[8] __attribute__((aligned(8))) = {
    STORE_INITIAL_BYTE,
};

extern char dynamic_bound_load_entry[];
extern char dynamic_bound_store_entry[];
extern char dynamic_bound_load_after_free_entry[];
extern char dynamic_bound_store_after_free_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global dynamic_bound_load_entry\n"
".type dynamic_bound_load_entry, @function\n"
"dynamic_bound_load_entry:\n"
"  la t0, dynamic_read_data\n"
"  lbu a0, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global dynamic_bound_store_entry\n"
".type dynamic_bound_store_entry, @function\n"
"dynamic_bound_store_entry:\n"
"  la t0, dynamic_write_data\n"
"  li t1, " XSTR(STORE_ALLOWED_BYTE) "\n"
"  sb t1, 0(t0)\n"
"  li a0, " XSTR(STORE_ALLOWED_BYTE) "\n"
"  ret\n"
".balign 8\n"
".global dynamic_bound_load_after_free_entry\n"
".type dynamic_bound_load_after_free_entry, @function\n"
"dynamic_bound_load_after_free_entry:\n"
"  li a0, " XSTR(FAULT_LOAD_MARKER) "\n"
"  la t0, dynamic_read_data\n"
"  lbu t1, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global dynamic_bound_store_after_free_entry\n"
".type dynamic_bound_store_after_free_entry, @function\n"
"dynamic_bound_store_after_free_entry:\n"
"  li a0, " XSTR(FAULT_STORE_MARKER) "\n"
"  la t0, dynamic_write_data\n"
"  li t1, " XSTR(STORE_AFTER_FREE_ATTEMPT) "\n"
"  sb t1, 0(t0)\n"
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

DEFINE_DASICS_CALL_INVOKER(invoke_dynamic_load, dynamic_bound_load_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_dynamic_store, dynamic_bound_store_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_dynamic_load_after_free,
                           dynamic_bound_load_after_free_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_dynamic_store_after_free,
                           dynamic_bound_store_after_free_entry)

static unsigned long lib_cfg_slot(unsigned int index, unsigned long cfg)
{
    return cfg << (index * DASICS_LIB_CFG_SLOT_BITS);
}

static void clear_dynamic_bound_csrs(void)
{
    csr_write(0x880, 0);
    csr_write(0x890, 0);
    csr_write(0x891, 0);
    csr_write(0x8b3, 0);
}

static int dynamic_libcfg_alloc_slot0(unsigned long cfg, const volatile void *base,
                                      unsigned long len)
{
    unsigned long libcfg = csr_read(0x880);

    if (libcfg & lib_cfg_slot(0, DASICS_LIB_CFG_VALID)) {
        return -1;
    }

    csr_write(0x890, (unsigned long)base);
    csr_write(0x891, (unsigned long)base + len);
    csr_write(0x880, libcfg | lib_cfg_slot(0, cfg | DASICS_LIB_CFG_VALID));
    csr_write(0x8b3, 0);

    return 0;
}

static int dynamic_libcfg_free(unsigned int index)
{
    unsigned long libcfg;

    if (index >= DASICS_LIB_ENTRY_NUM) {
        return -1;
    }

    libcfg = csr_read(0x880);
    libcfg &= ~lib_cfg_slot(index, DASICS_LIB_CFG_VALID);
    csr_write(0x880, libcfg);
    return 0;
}

static unsigned long dynamic_libcfg_get(unsigned int index)
{
    if (index >= DASICS_LIB_ENTRY_NUM) {
        return (unsigned long)-1;
    }

    return (csr_read(0x880) >> (index * DASICS_LIB_CFG_SLOT_BITS)) &
           DASICS_LIB_CFG_MASK;
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
    printf(DASICS_DYNAMIC_BOUND_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(const char *case_name, unsigned long expect,
                               unsigned long *total)
{
    unsigned long freason = read_dasics_freason();
    int pass = freason == expect;

    (*total)++;
    printf(DASICS_DYNAMIC_BOUND_TAG " case=%s freason=0x%lx expect=0x%lx result=%s\n",
           case_name, freason, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    unsigned long result;
    unsigned long before_cfg;
    unsigned long after_cfg;
    int failures = 0;
    int handle;
    int ret;

    printf(DASICS_DYNAMIC_BOUND_BEGIN_TAG " dynamic bound free smoke begin" DASICS_DYNAMIC_BOUND_COLOR_END "\n");

    clear_dynamic_bound_csrs();
    handle = dynamic_libcfg_alloc_slot0(DASICS_LIB_CFG_READ, dynamic_read_data,
                                        sizeof(dynamic_read_data));
    failures += record_value_case("DYNAMIC-BOUND-ALLOC-READABLE-HANDLE",
                                  handle, 0, &total);
    result = invoke_dynamic_load();
    failures += record_value_case("DYNAMIC-BOUND-ALLOC-READABLE-ALLOW",
                                  result, READ_ALLOWED_BYTE, &total);
    failures += record_freason_case("DYNAMIC-BOUND-ALLOC-READABLE-FREASON",
                                    0, &total);

    ret = dynamic_libcfg_free((unsigned int)handle);
    failures += record_value_case("DYNAMIC-BOUND-FREE-RETURN",
                                  ret, 0, &total);
    failures += record_value_case("DYNAMIC-BOUND-FREE-GET-FAIL",
                                  dynamic_libcfg_get((unsigned int)handle) &
                                  DASICS_LIB_CFG_VALID, 0, &total);
    csr_write(0x8b3, 0);
    result = invoke_dynamic_load_after_free();
    failures += record_value_case("DYNAMIC-BOUND-FREE-ACCESS-FAULT",
                                  result, FAULT_LOAD_MARKER, &total);
    failures += record_freason_case("DYNAMIC-BOUND-FREE-FREASON-LOAD",
                                    DASICS_FREASON_LOAD, &total);

    clear_dynamic_bound_csrs();
    dynamic_write_data[0] = STORE_INITIAL_BYTE;
    handle = dynamic_libcfg_alloc_slot0(DASICS_LIB_CFG_WRITE, dynamic_write_data,
                                        sizeof(dynamic_write_data));
    failures += record_value_case("DYNAMIC-BOUND-ALLOC-WRITABLE-HANDLE",
                                  handle, 0, &total);
    result = invoke_dynamic_store();
    failures += record_value_case("DYNAMIC-BOUND-ALLOC-WRITABLE-ALLOW",
                                  result, STORE_ALLOWED_BYTE, &total);
    failures += record_value_case("DYNAMIC-BOUND-ALLOC-WRITABLE-DATA",
                                  dynamic_write_data[0], STORE_ALLOWED_BYTE,
                                  &total);
    failures += record_freason_case("DYNAMIC-BOUND-ALLOC-WRITABLE-FREASON",
                                    0, &total);

    ret = dynamic_libcfg_free((unsigned int)handle);
    failures += record_value_case("DYNAMIC-BOUND-FREE-WRITABLE-RETURN",
                                  ret, 0, &total);
    csr_write(0x8b3, 0);
    result = invoke_dynamic_store_after_free();
    failures += record_value_case("DYNAMIC-BOUND-FREE-STORE-ACCESS-FAULT",
                                  result, FAULT_STORE_MARKER, &total);
    failures += record_value_case("DYNAMIC-BOUND-FREE-STORE-DATA-PRESERVE",
                                  dynamic_write_data[0], STORE_ALLOWED_BYTE,
                                  &total);
    failures += record_freason_case("DYNAMIC-BOUND-FREE-FREASON-STORE",
                                    DASICS_FREASON_STORE, &total);

    before_cfg = csr_read(0x880);
    ret = dynamic_libcfg_free(DASICS_LIB_ENTRY_NUM);
    after_cfg = csr_read(0x880);
    failures += record_value_case("DYNAMIC-BOUND-INVALID-HANDLE-RETURN",
                                  ret == -1, 1, &total);
    failures += record_value_case("DYNAMIC-BOUND-INVALID-HANDLE-NO-CORRUPTION",
                                  after_cfg, before_cfg, &total);

    before_cfg = csr_read(0x880);
    ret = dynamic_libcfg_free(0);
    after_cfg = csr_read(0x880);
    failures += record_value_case("DYNAMIC-BOUND-DOUBLE-FREE-RETURN",
                                  ret, 0, &total);
    failures += record_value_case("DYNAMIC-BOUND-DOUBLE-FREE-NO-CORRUPTION",
                                  after_cfg, before_cfg, &total);

    clear_dynamic_bound_csrs();

    printf(DASICS_DYNAMIC_BOUND_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_DYNAMIC_BOUND_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
