#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_CONTEXT_SECTION_TAG "\033[1;34m[DASICS-CONTEXT-SECTION-LOAD]\033[0m"
#define DASICS_CONTEXT_SECTION_BEGIN_TAG "\033[1;31m[DASICS-CONTEXT-SECTION-LOAD]"
#define DASICS_CONTEXT_SECTION_SUMMARY_TAG "\033[1;32m[DASICS-CONTEXT-SECTION-LOAD]"
#define DASICS_CONTEXT_SECTION_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_JUMP_CFG_VALID 0x1UL
#define CONTEXT_SECTION_ULIB_MARKER 0x554c494253454354UL
#define CONTEXT_SECTION_FREEZONE_MARKER 0x465245455a4f4e45UL

extern char context_section_ulib_marker_entry[];
extern char context_section_ulib_entry[];
extern char context_section_freezone_entry[];
extern char context_section_freezone_return_pc[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ufreezonetext,\"ax\",@progbits\n"
".balign 8\n"
".global context_section_freezone_entry\n"
".type context_section_freezone_entry, @function\n"
"context_section_freezone_entry:\n"
"  li a0, " XSTR(CONTEXT_SECTION_FREEZONE_MARKER) "\n"
"  ret\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global context_section_ulib_marker_entry\n"
".type context_section_ulib_marker_entry, @function\n"
"context_section_ulib_marker_entry:\n"
"  li a0, " XSTR(CONTEXT_SECTION_ULIB_MARKER) "\n"
"  ret\n"
".balign 8\n"
".global context_section_ulib_entry\n"
".type context_section_ulib_entry, @function\n"
"context_section_ulib_entry:\n"
"  mv t2, ra\n"
"  jal ra, context_section_freezone_entry\n"
".global context_section_freezone_return_pc\n"
"context_section_freezone_return_pc:\n"
"  mv ra, t2\n"
"  ret\n"
".section .text,\"ax\",@progbits\n"
".option pop\n"
);

static unsigned long read_jump_bound_lo(void)
{
    return csr_read(0x8c0);
}

static unsigned long read_jump_bound_hi(void)
{
    return csr_read(0x8c1);
}

static unsigned long read_jump_cfg(void)
{
    return csr_read(0x8c8);
}

static unsigned long invoke_context_section_ulib_marker_entry(void)
{
    unsigned long result;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, context_section_ulib_marker_entry\n"
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

static unsigned long invoke_context_section_ulib_entry(void)
{
    unsigned long result;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, context_section_ulib_entry\n"
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

static int record_bool_case(const char *case_name, int pass,
                            unsigned long *total)
{
    (*total)++;
    printf(DASICS_CONTEXT_SECTION_TAG " case=%s value=0x%lx expect=0x1 result=%s\n",
           case_name, pass ? 1UL : 0UL, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_value_case(const char *case_name, unsigned long value,
                             unsigned long expect, unsigned long *total)
{
    int pass = value == expect;

    (*total)++;
    printf(DASICS_CONTEXT_SECTION_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    unsigned long jump_lo = read_jump_bound_lo();
    unsigned long jump_hi = read_jump_bound_hi();
    unsigned long jump_cfg = read_jump_cfg();
    unsigned long ulib_result;
    unsigned long freezone_result;
    int failures = 0;

    printf(DASICS_CONTEXT_SECTION_BEGIN_TAG " UCAS OS section load smoke begin" DASICS_CONTEXT_SECTION_COLOR_END "\n");

    ulib_result = invoke_context_section_ulib_marker_entry();
    failures += record_value_case("CONTEXT-SECTION-ULIB-CALL-TARGET-LOAD",
                                  ulib_result, CONTEXT_SECTION_ULIB_MARKER,
                                  &total);
    failures += record_bool_case("CONTEXT-SECTION-FREEZONE-LOAD",
                                  (jump_cfg & DASICS_JUMP_CFG_VALID) &&
                                  jump_lo <= (unsigned long)context_section_freezone_entry &&
                                  (unsigned long)context_section_freezone_entry < jump_hi,
                                  &total);

    csr_write(0x8b2, (unsigned long)context_section_freezone_return_pc);
    freezone_result = invoke_context_section_ulib_entry();
    failures += record_value_case("CONTEXT-SECTION-UMAIN-ULIB-FREEZONE-CALL",
                                  freezone_result, CONTEXT_SECTION_FREEZONE_MARKER,
                                  &total);

    printf(DASICS_CONTEXT_SECTION_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_CONTEXT_SECTION_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
