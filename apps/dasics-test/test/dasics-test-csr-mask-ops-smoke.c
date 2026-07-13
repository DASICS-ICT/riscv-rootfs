#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define FDI_CSR_MASK_OPS_VERSION 1UL
#define FDI_CSR_MASK_OPS_USER_TOTAL 138UL

/* Supervisor-encoded main CSRs are exercised by the UCAS OS boot tests. */
#define FDI_USER_CSR_LIST(M) \
    M(0x880, FDILibCfg, ~0UL, 0) \
    M(0x890, FDILibBoundLo0, ~7UL, 1) \
    M(0x891, FDILibBoundHi0, ~7UL, 2) \
    M(0x892, FDILibBoundLo1, ~7UL, 3) \
    M(0x893, FDILibBoundHi1, ~7UL, 4) \
    M(0x894, FDILibBoundLo2, ~7UL, 5) \
    M(0x895, FDILibBoundHi2, ~7UL, 6) \
    M(0x896, FDILibBoundLo3, ~7UL, 7) \
    M(0x897, FDILibBoundHi3, ~7UL, 8) \
    M(0x898, FDILibBoundLo4, ~7UL, 9) \
    M(0x899, FDILibBoundHi4, ~7UL, 10) \
    M(0x89a, FDILibBoundLo5, ~7UL, 11) \
    M(0x89b, FDILibBoundHi5, ~7UL, 12) \
    M(0x89c, FDILibBoundLo6, ~7UL, 13) \
    M(0x89d, FDILibBoundHi6, ~7UL, 14) \
    M(0x89e, FDILibBoundLo7, ~7UL, 15) \
    M(0x89f, FDILibBoundHi7, ~7UL, 16) \
    M(0x8a0, FDILibBoundLo8, ~7UL, 17) \
    M(0x8a1, FDILibBoundHi8, ~7UL, 18) \
    M(0x8a2, FDILibBoundLo9, ~7UL, 19) \
    M(0x8a3, FDILibBoundHi9, ~7UL, 20) \
    M(0x8a4, FDILibBoundLo10, ~7UL, 21) \
    M(0x8a5, FDILibBoundHi10, ~7UL, 22) \
    M(0x8a6, FDILibBoundLo11, ~7UL, 23) \
    M(0x8a7, FDILibBoundHi11, ~7UL, 24) \
    M(0x8a8, FDILibBoundLo12, ~7UL, 25) \
    M(0x8a9, FDILibBoundHi12, ~7UL, 26) \
    M(0x8aa, FDILibBoundLo13, ~7UL, 27) \
    M(0x8ab, FDILibBoundHi13, ~7UL, 28) \
    M(0x8ac, FDILibBoundLo14, ~7UL, 29) \
    M(0x8ad, FDILibBoundHi14, ~7UL, 30) \
    M(0x8ae, FDILibBoundLo15, ~7UL, 31) \
    M(0x8af, FDILibBoundHi15, ~7UL, 32) \
    M(0x8b0, FDIMainCall, ~0UL, 33) \
    M(0x8b1, FDIReturnPC, ~0UL, 34) \
    M(0x8b2, FDIActiveReturn, ~0UL, 35) \
    M(0x8b3, FDIFReason, 0x7UL, 36) \
    M(0x8c0, FDIJumpBoundLo0, ~7UL, 37) \
    M(0x8c1, FDIJumpBoundHi0, ~7UL, 38) \
    M(0x8c2, FDIJumpBoundLo1, ~7UL, 39) \
    M(0x8c3, FDIJumpBoundHi1, ~7UL, 40) \
    M(0x8c4, FDIJumpBoundLo2, ~7UL, 41) \
    M(0x8c5, FDIJumpBoundHi2, ~7UL, 42) \
    M(0x8c6, FDIJumpBoundLo3, ~7UL, 43) \
    M(0x8c7, FDIJumpBoundHi3, ~7UL, 44) \
    M(0x8c8, FDIJumpCfg, ~0UL, 45)

#define csr_swap(reg, val) ({ unsigned long __tmp; \
    asm volatile ("csrrw %0, " #reg ", %1" : "=r"(__tmp) : "r"(val)); \
    __tmp; })

#define csr_set(reg, val) ({ unsigned long __tmp; \
    asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "r"(val)); \
    __tmp; })

#define csr_clear(reg, val) ({ unsigned long __tmp; \
    asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "r"(val)); \
    __tmp; })

struct fdi_csr_case {
    const char *name;
    unsigned long addr;
    unsigned long mask;
    unsigned long index;
};

static const struct fdi_csr_case fdi_user_csrs[] = {
#define FDI_CSR_ENTRY(addr, name, mask, index) { #name, addr, mask, index },
    FDI_USER_CSR_LIST(FDI_CSR_ENTRY)
#undef FDI_CSR_ENTRY
};

static unsigned long fdi_csr_read(unsigned long addr)
{
    switch (addr) {
#define FDI_CSR_READ(addr, name, mask, index) case addr: return csr_read(addr);
    FDI_USER_CSR_LIST(FDI_CSR_READ)
#undef FDI_CSR_READ
    default:
        return 0;
    }
}

static void fdi_csr_write(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define FDI_CSR_WRITE(addr, name, mask, index) case addr: csr_write(addr, value); break;
    FDI_USER_CSR_LIST(FDI_CSR_WRITE)
#undef FDI_CSR_WRITE
    default:
        break;
    }
}

static unsigned long fdi_csr_swap(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define FDI_CSR_SWAP(addr, name, mask, index) case addr: return csr_swap(addr, value);
    FDI_USER_CSR_LIST(FDI_CSR_SWAP)
#undef FDI_CSR_SWAP
    default:
        return 0;
    }
}

static unsigned long fdi_csr_set(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define FDI_CSR_SET(addr, name, mask, index) case addr: return csr_set(addr, value);
    FDI_USER_CSR_LIST(FDI_CSR_SET)
#undef FDI_CSR_SET
    default:
        return 0;
    }
}

static unsigned long fdi_csr_clear(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define FDI_CSR_CLEAR(addr, name, mask, index) case addr: return csr_clear(addr, value);
    FDI_USER_CSR_LIST(FDI_CSR_CLEAR)
#undef FDI_CSR_CLEAR
    default:
        return 0;
    }
}

static int record_operation(const struct fdi_csr_case *csr, const char *operation,
                            unsigned long operand, unsigned long old,
                            unsigned long expected_old, unsigned long readback,
                            unsigned long expected_readback, unsigned long *total)
{
    int pass = old == expected_old && readback == expected_readback;

    (*total)++;
    if (!pass) {
        printf("FDI_CSR_MASK_OPS case=%s csr=%s addr=0x%lx mask=0x%lx "
               "operand=0x%lx old=0x%lx expected_old=0x%lx read=0x%lx "
               "expected_read=0x%lx result=FAIL\n",
               operation, csr->name, csr->addr, csr->mask, operand, old,
               expected_old, readback, expected_readback);
    }

    return pass ? 0 : 1;
}

static void clear_user_csrs(void)
{
    for (unsigned long i = 0; i < ARRAY_SIZE(fdi_user_csrs); i++) {
        fdi_csr_write(fdi_user_csrs[i].addr, 0);
    }
}

static int run_csr_operations(const struct fdi_csr_case *csr,
                              unsigned long *total)
{
    unsigned long failures = 0;
    /* Isolate one writable bit so both set and clear must change state. */
    unsigned long writable_bit = csr->mask & (0UL - csr->mask);
    unsigned long write_operand = (0xa500000000000007UL ^
                                   (csr->index << 8)) & ~writable_bit;
    unsigned long expected = write_operand & csr->mask;
    unsigned long set_operand = (~expected & csr->mask) | ~csr->mask;
    unsigned long clear_operand = writable_bit | ~csr->mask;
    unsigned long old = fdi_csr_swap(csr->addr, write_operand);
    unsigned long readback = fdi_csr_read(csr->addr);

    failures += record_operation(csr, "CSRRW", write_operand, old, 0,
                                 readback, expected, total);

    old = fdi_csr_set(csr->addr, set_operand);
    expected |= set_operand & csr->mask;
    readback = fdi_csr_read(csr->addr);
    failures += record_operation(csr, "CSRRS", set_operand, old,
                                 write_operand & csr->mask, readback,
                                 expected, total);

    old = fdi_csr_clear(csr->addr, clear_operand);
    {
        unsigned long expected_old = expected;
        expected &= ~(clear_operand & csr->mask);
        readback = fdi_csr_read(csr->addr);
        failures += record_operation(csr, "CSRRC", clear_operand, old,
                                     expected_old, readback, expected, total);
    }

    return failures;
}

int main(void)
{
    unsigned long total = 0;
    unsigned long failures = 0;

    printf("FDI_CSR_MASK_OPS_BEGIN version=%lu privilege=user expected_total=%lu\n",
           FDI_CSR_MASK_OPS_VERSION, FDI_CSR_MASK_OPS_USER_TOTAL);

    /* Establish an operation baseline; reset evidence is collected in M-mode. */
    clear_user_csrs();
    for (unsigned long i = 0; i < ARRAY_SIZE(fdi_user_csrs); i++) {
        failures += run_csr_operations(&fdi_user_csrs[i], &total);
    }
    clear_user_csrs();

    if (total != FDI_CSR_MASK_OPS_USER_TOTAL) {
        failures++;
    }

    printf("FDI_CSR_MASK_OPS_SIGNATURE version=%lu privilege=user total=%lu "
           "failed=%lu result=%s\n",
           FDI_CSR_MASK_OPS_VERSION, total, failures,
           failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
