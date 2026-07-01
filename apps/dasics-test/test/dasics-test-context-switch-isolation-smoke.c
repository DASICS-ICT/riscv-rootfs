#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_CONTEXT_SWITCH_TAG "\033[1;34m[DASICS-CONTEXT-SWITCH-ISOLATION]\033[0m"
#define DASICS_CONTEXT_SWITCH_BEGIN_TAG "\033[1;31m[DASICS-CONTEXT-SWITCH-ISOLATION]"
#define DASICS_CONTEXT_SWITCH_SUMMARY_TAG "\033[1;32m[DASICS-CONTEXT-SWITCH-ISOLATION]"
#define DASICS_CONTEXT_SWITCH_COLOR_END "\033[0m"

#define SENTINEL_LIB_CFG 0xbUL
#define SENTINEL_LIB_LO 0x12345000UL
#define SENTINEL_LIB_HI 0x12346000UL
#define SENTINEL_JUMP_CFG 0x1UL
#define SENTINEL_JUMP_LO 0x22345000UL
#define SENTINEL_JUMP_HI 0x22346000UL
#define SENTINEL_RETURN_PC 0x33445000UL
#define SENTINEL_FREASON 0x4UL

static unsigned long read_lib_cfg(void)
{
    return csr_read(0x880);
}

static unsigned long read_lib_bound15_lo(void)
{
    return csr_read(0x8ae);
}

static unsigned long read_lib_bound15_hi(void)
{
    return csr_read(0x8af);
}

static unsigned long read_return_pc(void)
{
    return csr_read(0x8b1);
}

static unsigned long read_freason(void)
{
    return csr_read(0x8b3);
}

static unsigned long read_jump_bound3_lo(void)
{
    return csr_read(0x8c6);
}

static unsigned long read_jump_bound3_hi(void)
{
    return csr_read(0x8c7);
}

static unsigned long read_jump_cfg(void)
{
    return csr_read(0x8c8);
}

static void write_isolation_sentinel(void)
{
    csr_write(0x880, SENTINEL_LIB_CFG);
    csr_write(0x8ae, SENTINEL_LIB_LO);
    csr_write(0x8af, SENTINEL_LIB_HI);
    csr_write(0x8b1, SENTINEL_RETURN_PC);
    csr_write(0x8b3, SENTINEL_FREASON);
    csr_write(0x8c6, SENTINEL_JUMP_LO);
    csr_write(0x8c7, SENTINEL_JUMP_HI);
    csr_write(0x8c8, SENTINEL_JUMP_CFG);
}

static int record_bool_case(const char *case_name, int pass,
                            unsigned long *total)
{
    (*total)++;
    printf(DASICS_CONTEXT_SWITCH_TAG " case=%s value=0x%lx expect=0x1 result=%s\n",
           case_name, pass ? 1UL : 0UL, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;
    int no_sentinel_leak;
    int clean_default_cfg;
    int sentinel_written;

    printf(DASICS_CONTEXT_SWITCH_BEGIN_TAG " UCAS OS context switch isolation smoke begin" DASICS_CONTEXT_SWITCH_COLOR_END "\n");

    no_sentinel_leak = read_lib_cfg() != SENTINEL_LIB_CFG &&
                       read_lib_bound15_lo() != SENTINEL_LIB_LO &&
                       read_lib_bound15_hi() != SENTINEL_LIB_HI &&
                       read_return_pc() != SENTINEL_RETURN_PC &&
                       read_freason() != SENTINEL_FREASON &&
                       read_jump_bound3_lo() != SENTINEL_JUMP_LO &&
                       read_jump_bound3_hi() != SENTINEL_JUMP_HI &&
                       read_jump_cfg() != SENTINEL_JUMP_CFG;
    clean_default_cfg = read_lib_cfg() == 0 && read_jump_cfg() == 0;

    failures += record_bool_case("CONTEXT-SWITCH-BOUND-ISOLATION-NO-LEAK",
                                  no_sentinel_leak, &total);
    failures += record_bool_case("CONTEXT-EXEC-CLEAR-OLD-STATE",
                                  clean_default_cfg, &total);

    write_isolation_sentinel();
    sentinel_written = read_lib_cfg() == SENTINEL_LIB_CFG &&
                       read_lib_bound15_lo() == SENTINEL_LIB_LO &&
                       read_lib_bound15_hi() == SENTINEL_LIB_HI &&
                       read_return_pc() == SENTINEL_RETURN_PC &&
                       read_freason() == SENTINEL_FREASON &&
                       read_jump_bound3_lo() == SENTINEL_JUMP_LO &&
                       read_jump_bound3_hi() == SENTINEL_JUMP_HI &&
                       read_jump_cfg() == SENTINEL_JUMP_CFG;
    failures += record_bool_case("CONTEXT-EXIT-CLEAR-BOUND-TABLE-SENTINEL-WRITE",
                                  sentinel_written, &total);

    printf(DASICS_CONTEXT_SWITCH_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_CONTEXT_SWITCH_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
