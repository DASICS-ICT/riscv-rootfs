#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_REPLACEMENT_TAG "\033[1;34m[DASICS-DYNAMIC-BOUND-REPLACEMENT]\033[0m"
#define DASICS_REPLACEMENT_BEGIN_TAG "\033[1;31m[DASICS-DYNAMIC-BOUND-REPLACEMENT]"
#define DASICS_REPLACEMENT_SUMMARY_TAG "\033[1;32m[DASICS-DYNAMIC-BOUND-REPLACEMENT]"
#define DASICS_REPLACEMENT_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_LIB_CFG_READ 0x2UL
#define DASICS_LIB_CFG_VALID 0x8UL
#define DASICS_LIB_CFG_SLOT_BITS 4UL
#define DASICS_LIB_ENTRY_NUM 16
#define DASICS_FREASON_LOAD 2UL

#define REPLACEMENT_VICTIM_SLOT 7UL
#define SLOT_STRIDE 8UL
#define SLOT15_OFFSET 120UL
#define VICTIM_SLOT_OFFSET 56UL
#define SLOT15_VALUE 0x8fUL
#define TARGET_VALUE 0x73UL
#define FAULT_LOAD_MARKER 0x44594e5250464c54UL
#define ALL_READABLE_CFG 0xaaaaaaaaaaaaaaaaUL

static volatile unsigned char replacement_slot_data[DASICS_LIB_ENTRY_NUM][SLOT_STRIDE]
    __attribute__((aligned(8)));
static volatile unsigned char replacement_target_data[SLOT_STRIDE]
    __attribute__((aligned(8))) = {
    TARGET_VALUE,
};

extern char replacement_load_slot15_entry[];
extern char replacement_load_target_entry[];
extern char replacement_load_victim_old_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global replacement_load_slot15_entry\n"
".type replacement_load_slot15_entry, @function\n"
"replacement_load_slot15_entry:\n"
"  la t0, replacement_slot_data\n"
"  lbu a0, " XSTR(SLOT15_OFFSET) "(t0)\n"
"  ret\n"
".balign 8\n"
".global replacement_load_target_entry\n"
".type replacement_load_target_entry, @function\n"
"replacement_load_target_entry:\n"
"  la t0, replacement_target_data\n"
"  lbu a0, 0(t0)\n"
"  ret\n"
".balign 8\n"
".global replacement_load_victim_old_entry\n"
".type replacement_load_victim_old_entry, @function\n"
"replacement_load_victim_old_entry:\n"
"  li a0, " XSTR(FAULT_LOAD_MARKER) "\n"
"  la t0, replacement_slot_data\n"
"  lbu t1, " XSTR(VICTIM_SLOT_OFFSET) "(t0)\n"
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

DEFINE_DASICS_CALL_INVOKER(invoke_load_slot15, replacement_load_slot15_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_load_target, replacement_load_target_entry)
DEFINE_DASICS_CALL_INVOKER(invoke_load_victim_old, replacement_load_victim_old_entry)

static unsigned long lib_cfg_slot(unsigned int index, unsigned long cfg)
{
    return cfg << (index * DASICS_LIB_CFG_SLOT_BITS);
}

static void write_lib_bound_slot(unsigned int index, const volatile void *base,
                                 unsigned long len)
{
    unsigned long lo = (unsigned long)base;
    unsigned long hi = lo + len;

    switch (index) {
    case 0:
        csr_write(0x890, lo);
        csr_write(0x891, hi);
        break;
    case 1:
        csr_write(0x892, lo);
        csr_write(0x893, hi);
        break;
    case 2:
        csr_write(0x894, lo);
        csr_write(0x895, hi);
        break;
    case 3:
        csr_write(0x896, lo);
        csr_write(0x897, hi);
        break;
    case 4:
        csr_write(0x898, lo);
        csr_write(0x899, hi);
        break;
    case 5:
        csr_write(0x89a, lo);
        csr_write(0x89b, hi);
        break;
    case 6:
        csr_write(0x89c, lo);
        csr_write(0x89d, hi);
        break;
    case 7:
        csr_write(0x89e, lo);
        csr_write(0x89f, hi);
        break;
    case 8:
        csr_write(0x8a0, lo);
        csr_write(0x8a1, hi);
        break;
    case 9:
        csr_write(0x8a2, lo);
        csr_write(0x8a3, hi);
        break;
    case 10:
        csr_write(0x8a4, lo);
        csr_write(0x8a5, hi);
        break;
    case 11:
        csr_write(0x8a6, lo);
        csr_write(0x8a7, hi);
        break;
    case 12:
        csr_write(0x8a8, lo);
        csr_write(0x8a9, hi);
        break;
    case 13:
        csr_write(0x8aa, lo);
        csr_write(0x8ab, hi);
        break;
    case 14:
        csr_write(0x8ac, lo);
        csr_write(0x8ad, hi);
        break;
    case 15:
        csr_write(0x8ae, lo);
        csr_write(0x8af, hi);
        break;
    default:
        break;
    }
}

static void clear_replacement_csrs(void)
{
    csr_write(0x880, 0);
    csr_write(0x8b3, 0);
}

static void fill_all_readable_slots(void)
{
    unsigned long cfg = 0;

    for (unsigned int i = 0; i < DASICS_LIB_ENTRY_NUM; i++) {
        replacement_slot_data[i][0] = 0x40 + i;
        write_lib_bound_slot(i, replacement_slot_data[i], SLOT_STRIDE);
        cfg |= lib_cfg_slot(i, DASICS_LIB_CFG_VALID | DASICS_LIB_CFG_READ);
    }

    replacement_slot_data[15][0] = SLOT15_VALUE;
    csr_write(0x880, cfg);
    csr_write(0x8b3, 0);
}

static void replace_victim_slot_with_target(void)
{
    write_lib_bound_slot(REPLACEMENT_VICTIM_SLOT, replacement_target_data,
                         SLOT_STRIDE);
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
    printf(DASICS_REPLACEMENT_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(const char *case_name, unsigned long expect,
                               unsigned long *total)
{
    unsigned long freason = read_dasics_freason();
    int pass = freason == expect;

    (*total)++;
    printf(DASICS_REPLACEMENT_TAG " case=%s freason=0x%lx expect=0x%lx result=%s\n",
           case_name, freason, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    unsigned long result;
    int failures = 0;

    printf(DASICS_REPLACEMENT_BEGIN_TAG " dynamic bound replacement smoke begin" DASICS_REPLACEMENT_COLOR_END "\n");

    clear_replacement_csrs();
    fill_all_readable_slots();

    failures += record_value_case("DYNAMIC-BOUND-REPLACEMENT-FULL-CFG",
                                  csr_read(0x880) == ALL_READABLE_CFG, 1,
                                  &total);
    result = invoke_load_slot15();
    failures += record_value_case("DYNAMIC-BOUND-REPLACEMENT-FULL-HIGH-SLOT-ALLOW",
                                  result, SLOT15_VALUE, &total);
    failures += record_freason_case("DYNAMIC-BOUND-REPLACEMENT-FULL-HIGH-SLOT-FREASON",
                                    0, &total);

    replace_victim_slot_with_target();
    failures += record_value_case("DYNAMIC-BOUND-REPLACEMENT-VICTIM-SLOT",
                                  REPLACEMENT_VICTIM_SLOT, 7, &total);
    result = invoke_load_target();
    failures += record_value_case("DYNAMIC-BOUND-REPLACEMENT-TARGET-ALLOW",
                                  result, TARGET_VALUE, &total);
    failures += record_freason_case("DYNAMIC-BOUND-REPLACEMENT-TARGET-FREASON",
                                    0, &total);

    csr_write(0x8b3, 0);
    result = invoke_load_victim_old();
    failures += record_value_case("DYNAMIC-BOUND-REPLACEMENT-OLD-BOUND-FAULT",
                                  result, FAULT_LOAD_MARKER, &total);
    failures += record_freason_case("DYNAMIC-BOUND-REPLACEMENT-OLD-BOUND-FREASON",
                                    DASICS_FREASON_LOAD, &total);

    csr_write(0x8b3, 0);
    result = invoke_load_slot15();
    failures += record_value_case("DYNAMIC-BOUND-REPLACEMENT-OTHER-SLOT-PRESERVE",
                                  result, SLOT15_VALUE, &total);
    failures += record_freason_case("DYNAMIC-BOUND-REPLACEMENT-OTHER-SLOT-FREASON",
                                    0, &total);

    clear_replacement_csrs();

    printf(DASICS_REPLACEMENT_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_REPLACEMENT_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
