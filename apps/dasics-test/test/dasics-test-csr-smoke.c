#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define DASICS_CSR_TAG "\033[1;34m[DASICS-CSR]\033[0m"
#define DASICS_CSR_BEGIN_TAG "\033[1;31m[DASICS-CSR]"
#define DASICS_CSR_SUMMARY_TAG "\033[1;32m[DASICS-CSR]"
#define DASICS_CSR_COLOR_END "\033[0m"

#ifndef DASICS_CSR_SMOKE_VERBOSE
#define DASICS_CSR_SMOKE_VERBOSE 0
#endif

#define DASICS_U_CSR_LIST(M) \
    M(0x880, DasicsLibCfg) \
    M(0x890, DasicsLibBoundLo0) \
    M(0x891, DasicsLibBoundHi0) \
    M(0x892, DasicsLibBoundLo1) \
    M(0x893, DasicsLibBoundHi1) \
    M(0x894, DasicsLibBoundLo2) \
    M(0x895, DasicsLibBoundHi2) \
    M(0x896, DasicsLibBoundLo3) \
    M(0x897, DasicsLibBoundHi3) \
    M(0x898, DasicsLibBoundLo4) \
    M(0x899, DasicsLibBoundHi4) \
    M(0x89a, DasicsLibBoundLo5) \
    M(0x89b, DasicsLibBoundHi5) \
    M(0x89c, DasicsLibBoundLo6) \
    M(0x89d, DasicsLibBoundHi6) \
    M(0x89e, DasicsLibBoundLo7) \
    M(0x89f, DasicsLibBoundHi7) \
    M(0x8a0, DasicsLibBoundLo8) \
    M(0x8a1, DasicsLibBoundHi8) \
    M(0x8a2, DasicsLibBoundLo9) \
    M(0x8a3, DasicsLibBoundHi9) \
    M(0x8a4, DasicsLibBoundLo10) \
    M(0x8a5, DasicsLibBoundHi10) \
    M(0x8a6, DasicsLibBoundLo11) \
    M(0x8a7, DasicsLibBoundHi11) \
    M(0x8a8, DasicsLibBoundLo12) \
    M(0x8a9, DasicsLibBoundHi12) \
    M(0x8aa, DasicsLibBoundLo13) \
    M(0x8ab, DasicsLibBoundHi13) \
    M(0x8ac, DasicsLibBoundLo14) \
    M(0x8ad, DasicsLibBoundHi14) \
    M(0x8ae, DasicsLibBoundLo15) \
    M(0x8af, DasicsLibBoundHi15) \
    M(0x8b0, DasicsMainCall) \
    M(0x8b1, DasicsReturnPC) \
    M(0x8b2, DasicsActiveZoneReturnPC) \
    M(0x8b3, DasicsFReason) \
    M(0x8c0, DasicsJumpBoundLo0) \
    M(0x8c1, DasicsJumpBoundHi0) \
    M(0x8c2, DasicsJumpBoundLo1) \
    M(0x8c3, DasicsJumpBoundHi1) \
    M(0x8c4, DasicsJumpBoundLo2) \
    M(0x8c5, DasicsJumpBoundHi2) \
    M(0x8c6, DasicsJumpBoundLo3) \
    M(0x8c7, DasicsJumpBoundHi3) \
    M(0x8c8, DasicsJumpCfg)

struct csr_case {
    const char *id;
    const char *name;
    unsigned long addr;
    unsigned long write;
    unsigned long expect;
};

struct csr_addr_name {
    const char *name;
    unsigned long addr;
};

struct bound_pair {
    const char *lo_name;
    unsigned long lo_addr;
    const char *hi_name;
    unsigned long hi_addr;
};

static unsigned long dasics_read_csr(unsigned long addr)
{
    switch (addr) {
#define READ_CASE(addr, name) case addr: return csr_read(addr);
    DASICS_U_CSR_LIST(READ_CASE)
#undef READ_CASE
    default:
        return 0;
    }
}

static void dasics_write_csr(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define WRITE_CASE(addr, name) case addr: csr_write(addr, value); break;
    DASICS_U_CSR_LIST(WRITE_CASE)
#undef WRITE_CASE
    default:
        break;
    }
}

static int check_csr(const struct csr_case *test, unsigned long *total)
{
    dasics_write_csr(test->addr, test->write);
    unsigned long read = dasics_read_csr(test->addr);
    int pass = read == test->expect;

    (*total)++;
    if (DASICS_CSR_SMOKE_VERBOSE || !pass) {
        printf(DASICS_CSR_TAG " case=%s csr=%s addr=0x%lx write=0x%lx read=0x%lx expect=0x%lx result=%s\n",
               test->id, test->name, test->addr, test->write, read, test->expect,
               pass ? "PASS" : "FAIL");
    }

    return pass ? 0 : 1;
}

static int check_reset_csr(const struct csr_addr_name *csr, unsigned long *total)
{
    unsigned long read = dasics_read_csr(csr->addr);
    int pass = read == 0;

    (*total)++;
    if (DASICS_CSR_SMOKE_VERBOSE || !pass) {
        printf(DASICS_CSR_TAG " case=CSR-SMOKE-001 csr=%s addr=0x%lx write=0x0 read=0x%lx expect=0x0 result=%s\n",
               csr->name, csr->addr, read, pass ? "PASS" : "FAIL");
    }

    return pass ? 0 : 1;
}

static void clear_dasics_csrs(void)
{
#define CLEAR_CASE(addr, name) dasics_write_csr(addr, 0);
    DASICS_U_CSR_LIST(CLEAR_CASE)
#undef CLEAR_CASE
}

static int run_reset_checks(unsigned long *total)
{
    static const struct csr_addr_name reset_csrs[] = {
#define RESET_ENTRY(addr, name) { #name, addr },
        DASICS_U_CSR_LIST(RESET_ENTRY)
#undef RESET_ENTRY
    };

    int failures = 0;
    for (unsigned int i = 0; i < ARRAY_SIZE(reset_csrs); i++) {
        failures += check_reset_csr(&reset_csrs[i], total);
    }
    return failures;
}

static int run_lib_bound_checks(unsigned long *total)
{
    static const struct bound_pair bounds[] = {
        { "DasicsLibBoundLo0", 0x890, "DasicsLibBoundHi0", 0x891 },
        { "DasicsLibBoundLo1", 0x892, "DasicsLibBoundHi1", 0x893 },
        { "DasicsLibBoundLo2", 0x894, "DasicsLibBoundHi2", 0x895 },
        { "DasicsLibBoundLo3", 0x896, "DasicsLibBoundHi3", 0x897 },
        { "DasicsLibBoundLo4", 0x898, "DasicsLibBoundHi4", 0x899 },
        { "DasicsLibBoundLo5", 0x89a, "DasicsLibBoundHi5", 0x89b },
        { "DasicsLibBoundLo6", 0x89c, "DasicsLibBoundHi6", 0x89d },
        { "DasicsLibBoundLo7", 0x89e, "DasicsLibBoundHi7", 0x89f },
        { "DasicsLibBoundLo8", 0x8a0, "DasicsLibBoundHi8", 0x8a1 },
        { "DasicsLibBoundLo9", 0x8a2, "DasicsLibBoundHi9", 0x8a3 },
        { "DasicsLibBoundLo10", 0x8a4, "DasicsLibBoundHi10", 0x8a5 },
        { "DasicsLibBoundLo11", 0x8a6, "DasicsLibBoundHi11", 0x8a7 },
        { "DasicsLibBoundLo12", 0x8a8, "DasicsLibBoundHi12", 0x8a9 },
        { "DasicsLibBoundLo13", 0x8aa, "DasicsLibBoundHi13", 0x8ab },
        { "DasicsLibBoundLo14", 0x8ac, "DasicsLibBoundHi14", 0x8ad },
        { "DasicsLibBoundLo15", 0x8ae, "DasicsLibBoundHi15", 0x8af },
    };

    int failures = 0;
    for (unsigned int i = 0; i < ARRAY_SIZE(bounds); i++) {
        unsigned long lo_value = 0x1000000000000000UL + ((unsigned long)i << 12) + 0x123UL;
        unsigned long hi_value = 0x2000000000000000UL + ((unsigned long)i << 12) + 0x456UL;
        struct csr_case lo_case = { "CSR-SMOKE-007", bounds[i].lo_name, bounds[i].lo_addr, lo_value, lo_value };
        struct csr_case hi_case = { "CSR-SMOKE-007", bounds[i].hi_name, bounds[i].hi_addr, hi_value, hi_value };

        failures += check_csr(&lo_case, total);
        failures += check_csr(&hi_case, total);
    }
    return failures;
}

static int run_jump_bound_checks(unsigned long *total)
{
    static const struct bound_pair bounds[] = {
        { "DasicsJumpBoundLo0", 0x8c0, "DasicsJumpBoundHi0", 0x8c1 },
        { "DasicsJumpBoundLo1", 0x8c2, "DasicsJumpBoundHi1", 0x8c3 },
        { "DasicsJumpBoundLo2", 0x8c4, "DasicsJumpBoundHi2", 0x8c5 },
        { "DasicsJumpBoundLo3", 0x8c6, "DasicsJumpBoundHi3", 0x8c7 },
    };

    int failures = 0;
    for (unsigned int i = 0; i < ARRAY_SIZE(bounds); i++) {
        unsigned long lo_value = 0x3000000000000000UL + ((unsigned long)i << 12) + 0x321UL;
        unsigned long hi_value = 0x4000000000000000UL + ((unsigned long)i << 12) + 0x654UL;
        struct csr_case lo_case = { "CSR-SMOKE-009", bounds[i].lo_name, bounds[i].lo_addr, lo_value, lo_value };
        struct csr_case hi_case = { "CSR-SMOKE-009", bounds[i].hi_name, bounds[i].hi_addr, hi_value, hi_value };

        failures += check_csr(&lo_case, total);
        failures += check_csr(&hi_case, total);
    }
    return failures;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;

    printf(DASICS_CSR_BEGIN_TAG " user smoke begin" DASICS_CSR_COLOR_END "\n");

    failures += run_reset_checks(&total);

    static const struct csr_case rw_cases[] = {
        { "CSR-SMOKE-005", "DasicsFReason", 0x8b3, 0x123456789abcdef7UL, 0x7UL },
        { "CSR-SMOKE-006", "DasicsLibCfg", 0x880, 0x0123456789abcdefUL, 0x0123456789abcdefUL },
        { "CSR-SMOKE-008", "DasicsJumpCfg", 0x8c8, 0x0001000100010001UL, 0x0001000100010001UL },
        { "CSR-SMOKE-010", "DasicsMainCall", 0x8b0, 0x5000000000000010UL, 0x5000000000000010UL },
        { "CSR-SMOKE-010", "DasicsReturnPC", 0x8b1, 0x5000000000000020UL, 0x5000000000000020UL },
        { "CSR-SMOKE-010", "DasicsActiveZoneReturnPC", 0x8b2, 0x5000000000000030UL, 0x5000000000000030UL },
    };

    for (unsigned int i = 0; i < ARRAY_SIZE(rw_cases); i++) {
        failures += check_csr(&rw_cases[i], &total);
    }

    failures += run_lib_bound_checks(&total);
    failures += run_jump_bound_checks(&total);

    clear_dasics_csrs();

    printf(DASICS_CSR_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_CSR_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
