#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"
#include "uattr.h"
#include "udasics.h"

#define CSR_TAG "[DASICS-CSR-UNIFIED]"
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define CSR_UNIFIED_TOTAL 233UL

#if !defined(DASICS_N_EXTENSION_PROFILE) || !DASICS_LINUX_DUAL_EXEC
int main(void)
{
    fprintf(stderr, CSR_TAG " requires Linux N-profile result=FAIL\n");
    return 2;
}
#else

#include "fdi_verify_client.h"

/* Architectural user-bank masks; configuration and address CSRs have distinct
 * writable masks even though all operations use the same XLEN interface. */
#define USER_CSR_LIST(M) \
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

struct csr_case {
    const char *name;
    unsigned long addr, mask, index;
};

static const struct csr_case user_csrs[] = {
#define CSR_ENTRY(addr, name, mask, index) {#name, addr, mask, index},
    USER_CSR_LIST(CSR_ENTRY)
#undef CSR_ENTRY
};

static unsigned long read_csr(unsigned long addr)
{
    switch (addr) {
#define CSR_READ(addr, name, mask, index) case addr: return csr_read(addr);
    USER_CSR_LIST(CSR_READ)
#undef CSR_READ
    default: return 0;
    }
}

static void write_csr(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define CSR_WRITE(addr, name, mask, index) case addr: csr_write(addr, value); break;
    USER_CSR_LIST(CSR_WRITE)
#undef CSR_WRITE
    }
}

#define CSR_RMW(mnemonic, addr, value) ({ unsigned long old; \
    asm volatile(mnemonic " %0," CSR_STRINGIFY(addr) ",%1" \
                 : "=r"(old) : "r"(value) : "memory"); old; })

static unsigned long swap_csr(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define CSR_SWAP(addr, name, mask, index) case addr: return CSR_RMW("csrrw", addr, value);
    USER_CSR_LIST(CSR_SWAP)
#undef CSR_SWAP
    default: return 0;
    }
}

static unsigned long set_csr(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define CSR_SET(addr, name, mask, index) case addr: return CSR_RMW("csrrs", addr, value);
    USER_CSR_LIST(CSR_SET)
#undef CSR_SET
    default: return 0;
    }
}

static unsigned long clear_csr(unsigned long addr, unsigned long value)
{
    switch (addr) {
#define CSR_CLEAR(addr, name, mask, index) case addr: return CSR_RMW("csrrc", addr, value);
    USER_CSR_LIST(CSR_CLEAR)
#undef CSR_CLEAR
    default: return 0;
    }
}

static void clear_user_csrs(void)
{
    for (unsigned long i = 0; i < ARRAY_SIZE(user_csrs); i++)
        write_csr(user_csrs[i].addr, 0);
}

static void restore_user_csrs(const unsigned long *saved)
{
    /* Restore addresses and special entries before publishing permissions. */
    write_csr(0x880, 0);
    write_csr(0x8c8, 0);
    for (unsigned long i = 0; i < ARRAY_SIZE(user_csrs); i++)
        if (user_csrs[i].addr != 0x880 && user_csrs[i].addr != 0x8c8)
            write_csr(user_csrs[i].addr, saved[i]);
    write_csr(0x8c8, saved[45]);
    write_csr(0x880, saved[0]);
}

static unsigned long independent_pattern(const struct csr_case *csr)
{
    unsigned long index = csr->index;
    if (index >= 1 && index <= 32) {
        unsigned long pair = (index - 1) / 2;
        return (index & 1 ? 0x1000000000000127UL : 0x2000000000000457UL) + (pair << 12);
    }
    if (index >= 37 && index <= 44) {
        unsigned long pair = (index - 37) / 2;
        return (index & 1 ? 0x3000000000000327UL : 0x4000000000000657UL) + (pair << 12);
    }
    switch (index) {
    case 0: return 0x0123456789abcdefUL;
    case 33: return 0x5000000000000017UL;
    case 34: return 0x5000000000000027UL;
    case 35: return 0x5000000000000037UL;
    case 36: return 0x123456789abcdef7UL;
    default: return 0xfedcba9876543217UL;
    }
}

static int record_operation(const struct csr_case *csr, const char *operation,
                             unsigned long operand, unsigned long old,
                             unsigned long old_expected, unsigned long readback,
                             unsigned long read_expected, const char *old_source,
                             unsigned long *total)
{
    int pass = old == old_expected && readback == read_expected;
    (*total)++;
    printf(CSR_TAG " case=%s-%s csr=%s addr=0x%lx mask=0x%lx operand=0x%lx "
           "old=0x%lx old_expected=0x%lx old_source=%s read=0x%lx read_expected=0x%lx "
           "result=%s\n", operation, csr->name, csr->name, csr->addr, csr->mask,
           operand, old, old_expected, old_source, readback, read_expected, pass ? "PASS" : "FAIL");
    return !pass;
}

static int run_patterns(unsigned long *total)
{
    int failures = 0;
    for (unsigned long i = 0; i < ARRAY_SIZE(user_csrs); i++) {
        const struct csr_case *csr = &user_csrs[i];
        unsigned long value = independent_pattern(csr);
        write_csr(csr->addr, value);
        unsigned long readback = read_csr(csr->addr);
        int pass = readback == (value & csr->mask);
        (*total)++;
        printf(CSR_TAG " case=WRITE-PATTERN-%s csr=%s addr=0x%lx write=0x%lx "
               "read=0x%lx read_expected=0x%lx result=%s\n",
               csr->name, csr->name, csr->addr, value, readback,
               value & csr->mask, pass ? "PASS" : "FAIL");
        failures += !pass;
    }
    return failures;
}

static int run_rmw(unsigned long *total)
{
    int failures = 0;
    /* The old value returned by CSRRW observes the preceding software clear.
     * These 46 observations replace a separate cleared-value readback pass. */
    clear_user_csrs();
    for (unsigned long i = 0; i < ARRAY_SIZE(user_csrs); i++) {
        const struct csr_case *csr = &user_csrs[i];
        unsigned long writable_bit = csr->mask & (0UL - csr->mask);
        unsigned long operand = (0xa500000000000007UL ^ (csr->index << 8)) & ~writable_bit;
        unsigned long expected = operand & csr->mask;
        unsigned long set_operand = (~expected & csr->mask) | ~csr->mask;
        unsigned long clear_operand = writable_bit | ~csr->mask;
        unsigned long old = swap_csr(csr->addr, operand);
        failures += record_operation(csr, "RMW-CSRRW", operand, old, 0,
                                     read_csr(csr->addr), expected, "instruction", total);
        old = set_csr(csr->addr, set_operand);
        unsigned long set_expected = expected | (set_operand & csr->mask);
        failures += record_operation(csr, "RMW-CSRRS", set_operand, old, expected,
                                     read_csr(csr->addr), set_expected, "instruction", total);
        old = clear_csr(csr->addr, clear_operand);
        expected = set_expected & ~(clear_operand & csr->mask);
        failures += record_operation(csr, "RMW-CSRRC", clear_operand, old, set_expected,
                                     read_csr(csr->addr), expected, "instruction", total);
    }
    return failures;
}

static int run_zero_operands(unsigned long *total)
{
    const struct csr_case *csr = &user_csrs[36];
    unsigned long saved = csr_read(0x8b3), old;
    int failures = 0;
#define ZERO_RECORD(name, operand, old_expected, read_expected, source) \
    record_operation(csr, name, operand, old, old_expected, csr_read(0x8b3), read_expected, source, total)
    csr_write(0x8b3, 5);
    asm volatile("csrrw %0,0x8b3,zero" : "=r"(old) :: "memory");
    failures += ZERO_RECORD("CSRRW-RS1-X0", 0, 5, 0, "instruction");
    csr_write(0x8b3, 5);
    asm volatile("csrrs %0,0x8b3,zero" : "=r"(old) :: "memory");
    failures += ZERO_RECORD("CSRRS-RS1-X0", 0, 5, 5, "instruction");
    csr_write(0x8b3, 5);
    asm volatile("csrrc %0,0x8b3,zero" : "=r"(old) :: "memory");
    failures += ZERO_RECORD("CSRRC-RS1-X0", 0, 5, 5, "instruction");
    csr_write(0x8b3, 5);
    asm volatile("li t0,0\ncsrrs %0,0x8b3,t0" : "=r"(old) :: "t0", "memory");
    failures += ZERO_RECORD("CSRRS-RS1-T0-ZERO", 0, 5, 5, "instruction");
    csr_write(0x8b3, 5);
    asm volatile("li t0,0\ncsrrc %0,0x8b3,t0" : "=r"(old) :: "t0", "memory");
    failures += ZERO_RECORD("CSRRC-RS1-T0-ZERO", 0, 5, 5, "instruction");
    csr_write(0x8b3, 5);
    old = csr_read(0x8b3);
    asm volatile("li t0,2\ncsrrs zero,0x8b3,t0" ::: "t0", "memory");
    failures += ZERO_RECORD("CSRRS-RD-X0-SET", 2, 5, 7, "separate-read");
    old = csr_read(0x8b3);
    asm volatile("li t0,4\ncsrrc zero,0x8b3,t0" ::: "t0", "memory");
    failures += ZERO_RECORD("CSRRC-RD-X0-CLEAR", 4, 7, 3, "separate-read");
    old = csr_read(0x8b3);
    csr_write(0x8b3, saved);
    failures += ZERO_RECORD("CSR-ZERO-RESTORE", saved, 3, saved & 7UL, "separate-read");
#undef ZERO_RECORD
    return failures;
}

enum immediate_operation { IMM_WRITE, IMM_SET, IMM_CLEAR };

#define IMMEDIATE_READ(mnemonic, addr, zimm) ({ unsigned long old; \
    asm volatile(mnemonic " %0," CSR_STRINGIFY(addr) "," CSR_STRINGIFY(zimm) \
                 : "=r"(old) :: "memory"); old; })
#define IMMEDIATE_DISCARD(mnemonic, addr, zimm) \
    asm volatile(mnemonic " zero," CSR_STRINGIFY(addr) "," CSR_STRINGIFY(zimm) ::: "memory")
#define IMMEDIATE_OPERATION(addr, mnemonic, selector) \
    if (operation == selector) { \
        if (rd_zero) { \
            unsigned long old = csr_read(addr); \
            if (zimm == 0) { IMMEDIATE_DISCARD(mnemonic, addr, 0); } \
            else { IMMEDIATE_DISCARD(mnemonic, addr, 31); } \
            return old; \
        } \
        if (zimm == 0) { return IMMEDIATE_READ(mnemonic, addr, 0); } \
        return IMMEDIATE_READ(mnemonic, addr, 31); \
    }
#define IMMEDIATE_CSR(addr) \
    case addr: \
        IMMEDIATE_OPERATION(addr, "csrrwi", IMM_WRITE) \
        IMMEDIATE_OPERATION(addr, "csrrsi", IMM_SET) \
        IMMEDIATE_OPERATION(addr, "csrrci", IMM_CLEAR) \
        return 0;

static unsigned long immediate_csr(unsigned long addr, enum immediate_operation operation,
                                    unsigned int zimm, int rd_zero)
{
    switch (addr) {
        IMMEDIATE_CSR(0x880)
        IMMEDIATE_CSR(0x890)
        IMMEDIATE_CSR(0x8b3)
    default: return 0;
    }
}

static int run_immediates(unsigned long *total)
{
    static const unsigned long indices[] = {0, 1, 36};
    static const unsigned int immediates[] = {0, 31};
    static const char *const names[] = {"CSRRWI", "CSRRSI", "CSRRCI"};
    int failures = 0;
    /* State observations cannot distinguish an internal write strobe that
     * writes the same value. rd=x0 cases report a separate pre-instruction read. */
    for (unsigned long i = 0; i < ARRAY_SIZE(indices); i++) {
        const struct csr_case *csr = &user_csrs[indices[i]];
        unsigned long initial = 0xa5a5000012345015UL & csr->mask;
        for (unsigned int operation = IMM_WRITE; operation <= IMM_CLEAR; operation++) {
            for (unsigned int z = 0; z < ARRAY_SIZE(immediates); z++) {
                for (int rd_zero = 0; rd_zero < 2; rd_zero++) {
                    unsigned int zimm = immediates[z];
                    unsigned long expected = operation == IMM_WRITE ? zimm & csr->mask :
                        operation == IMM_SET ? initial | (zimm & csr->mask) :
                        initial & ~(zimm & csr->mask);
                    char name[64];
                    write_csr(csr->addr, initial);
                    unsigned long old = immediate_csr(csr->addr, operation, zimm, rd_zero);
                    snprintf(name, sizeof(name), "IMM-%s-ZIMM%u-RD-%s", names[operation], zimm,
                             rd_zero ? "X0" : "NONZERO");
                    failures += record_operation(csr, name, zimm, old, initial,
                        read_csr(csr->addr), expected, rd_zero ? "separate-read" : "instruction", total);
                }
            }
        }
    }
    return failures;
}

extern unsigned long csr_unified_untrusted_read(unsigned long seed);
extern unsigned long csr_unified_untrusted_write(unsigned long attack);
extern char csr_unified_read_site[], csr_unified_write_site[];

/* Ordinary CALL plus ActiveReturn authorizes the return without modifying
 * ReturnPC. Both protected CSR words are fixed 32-bit instructions. */
asm(".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global csr_unified_untrusted_read\n.type csr_unified_untrusted_read,@function\n"
    "csr_unified_untrusted_read:\naddi sp,sp,-16\nsd ra,8(sp)\n"
    "la t0,1f\ncsrw 0x8b2,t0\ncall csr_unified_read_probe\n"
    "1:\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size csr_unified_untrusted_read,.-csr_unified_untrusted_read\n.balign 8\n"
    ".global csr_unified_untrusted_write\n.type csr_unified_untrusted_write,@function\n"
    "csr_unified_untrusted_write:\naddi sp,sp,-16\nsd ra,8(sp)\n"
    "la t0,1f\ncsrw 0x8b2,t0\ncall csr_unified_write_probe\n"
    "1:\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size csr_unified_untrusted_write,.-csr_unified_untrusted_write\n"
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".type csr_unified_read_probe,@function\ncsr_unified_read_probe:\n"
    ".global csr_unified_read_site\ncsr_unified_read_site:\ncsrr a0,0x8b3\nret\n"
    ".size csr_unified_read_probe,.-csr_unified_read_probe\n.balign 8\n"
    ".type csr_unified_write_probe,@function\ncsr_unified_write_probe:\n"
    ".global csr_unified_write_site\ncsr_unified_write_site:\ncsrw 0x880,a0\nret\n"
    ".size csr_unified_write_probe,.-csr_unified_write_probe\n"
    ".section .text,\"ax\",@progbits\n.option pop\n");

static int run_protection(unsigned long *total)
{
    const unsigned long seed = 0xfeedfacecafebeefUL, cfg = 0x0123456789abcdefUL;
    unsigned long before = dasics_n_extension_trap_count;
    unsigned long return_pc = csr_read(0x8b1);
    struct fdi_verify_client observer = {.fd = -1};
    int failures = 0;
    int operation = fdi_verify_begin(&observer, 0);
    int saved_errno = operation ? errno : 0;
    printf(CSR_TAG " prerequisite=CAUSE2-OBSERVER-BEGIN flags=0x%llx version=%llu "
           "pid=%llu cookie=0x%llx active=%llu rc=%d errno=%d result=%s\n",
           observer.observed.flags, observer.observed.version, observer.observed.pid,
           observer.observed.cookie, observer.observed.active, operation, saved_errno,
           operation ? "FAIL" : "PASS");
    if (operation)
        return 1;

    /* Each ARM starts a fresh cause-2 window. The trusted ioctl path must not
     * change the protected CSR state or redirect a DASICS U24 exception. */
    csr_write(0x8b3, 5);
    operation = fdi_verify_arm(&observer, FDI_VERIFY_CSR_READ, csr_unified_read_site);
    saved_errno = operation ? errno : 0;
    int armed = !operation && observer.observed.kind == FDI_VERIFY_CSR_READ &&
        observer.observed.site == (unsigned long)csr_unified_read_site &&
        observer.observed.instruction == 0x8b302573UL &&
        observer.observed.illegal_count == 0;
    printf(CSR_TAG " prerequisite=CAUSE2-OBSERVER-ARM probe=CSR-PROT-U-UNTRUSTED-R "
           "pid=%llu cookie=0x%llx kind=%llu site=0x%llx instruction=0x%llx "
           "count=%llu rc=%d errno=%d result=%s\n",
           observer.observed.pid, observer.observed.cookie, observer.observed.kind,
           observer.observed.site, observer.observed.instruction, observer.observed.illegal_count,
           operation, saved_errno, armed ? "PASS" : "FAIL");
    if (!armed) {
        failures++;
        goto cleanup;
    }
    unsigned long result = csr_unified_untrusted_read(seed);
    unsigned long reason = csr_read(0x8b3), observed_return = csr_read(0x8b1);
    operation = fdi_verify_query(&observer);
    saved_errno = operation ? errno : 0;
    int observed = !operation && observer.observed.kind == FDI_VERIFY_CSR_READ &&
        observer.observed.site == (unsigned long)csr_unified_read_site &&
        observer.observed.instruction == 0x8b302573UL && observer.observed.illegal_count == 1 &&
        observer.observed.cause == 2 && observer.observed.epc == (unsigned long)csr_unified_read_site &&
        observer.observed.gpr[10] == seed && observer.observed.freason == 5 &&
        observer.observed.return_pc == return_pc;
    int pass = result == seed && reason == 5 && observed_return == return_pc &&
        dasics_n_extension_trap_count == before && observed;
    (*total)++;
    failures += !pass;
    printf(CSR_TAG " case=CSR-PROT-U-UNTRUSTED-R csr=FDIFReason addr=0x8b3 "
           "read=0x%lx read_expected=0x%lx freason=0x%lx freason_expected=0x5 "
           "returnpc=0x%lx returnpc_expected=0x%lx u24=%lu u24_expected=%lu "
           "site=0x%lx cause2=%s observer_rc=%d observer_errno=%d result=%s\n",
           result, seed, reason, observed_return, return_pc,
           dasics_n_extension_trap_count, before, (unsigned long)csr_unified_read_site,
           observed ? "OBSERVED" : "INVALID", operation, saved_errno, pass ? "PASS" : "FAIL");
    if (!operation)
        fdi_verify_print("CSR-PROT-U-UNTRUSTED-R", &observer.observed);
    if (operation)
        goto cleanup;

    csr_write(0x880, cfg);
    csr_write(0x8b3, 5);
    operation = fdi_verify_arm(&observer, FDI_VERIFY_CSR_WRITE, csr_unified_write_site);
    saved_errno = operation ? errno : 0;
    armed = !operation && observer.observed.kind == FDI_VERIFY_CSR_WRITE &&
        observer.observed.site == (unsigned long)csr_unified_write_site &&
        observer.observed.instruction == 0x88051073UL &&
        observer.observed.illegal_count == 0;
    printf(CSR_TAG " prerequisite=CAUSE2-OBSERVER-ARM probe=CSR-PROT-U-UNTRUSTED-W "
           "pid=%llu cookie=0x%llx kind=%llu site=0x%llx instruction=0x%llx "
           "count=%llu rc=%d errno=%d result=%s\n",
           observer.observed.pid, observer.observed.cookie, observer.observed.kind,
           observer.observed.site, observer.observed.instruction, observer.observed.illegal_count,
           operation, saved_errno, armed ? "PASS" : "FAIL");
    if (!armed) {
        failures++;
        goto cleanup;
    }
    result = csr_unified_untrusted_write(~0UL);
    unsigned long readback = csr_read(0x880);
    reason = csr_read(0x8b3);
    observed_return = csr_read(0x8b1);
    operation = fdi_verify_query(&observer);
    saved_errno = operation ? errno : 0;
    observed = !operation && observer.observed.kind == FDI_VERIFY_CSR_WRITE &&
        observer.observed.site == (unsigned long)csr_unified_write_site &&
        observer.observed.instruction == 0x88051073UL && observer.observed.illegal_count == 1 &&
        observer.observed.cause == 2 && observer.observed.epc == (unsigned long)csr_unified_write_site &&
        observer.observed.gpr[10] == ~0UL && observer.observed.freason == 5 &&
        observer.observed.return_pc == return_pc;
    pass = readback == cfg && result == ~0UL && reason == 5 &&
        observed_return == return_pc && dasics_n_extension_trap_count == before && observed;
    (*total)++;
    failures += !pass;
    printf(CSR_TAG " case=CSR-PROT-U-UNTRUSTED-W csr=FDILibCfg addr=0x880 "
           "write=0x%lx read=0x%lx read_expected=0x%lx source=0x%lx source_expected=0x%lx "
           "freason=0x%lx freason_expected=0x5 returnpc=0x%lx returnpc_expected=0x%lx "
           "u24=%lu u24_expected=%lu site=0x%lx cause2=%s "
           "observer_rc=%d observer_errno=%d result=%s\n",
           ~0UL, readback, cfg, result, ~0UL, reason, observed_return, return_pc,
           dasics_n_extension_trap_count, before, (unsigned long)csr_unified_write_site,
           observed ? "OBSERVED" : "INVALID", operation, saved_errno, pass ? "PASS" : "FAIL");
    if (!operation)
        fdi_verify_print("CSR-PROT-U-UNTRUSTED-W", &observer.observed);

cleanup:
    operation = fdi_verify_end(&observer);
    saved_errno = operation ? errno : 0;
    failures += !!operation;
    printf(CSR_TAG " cleanup=CAUSE2-OBSERVER-END version=%llu flags=0x%llx pid=%llu "
           "cookie=0x%llx active=%llu senvcfg=0x%llx shadow_envcfg=0x%llx "
           "saved_envcfg=0x%llx fd=%d rc=%d errno=%d result=%s\n",
           observer.observed.version, observer.observed.flags, observer.observed.pid,
           observer.observed.cookie, observer.observed.active, observer.observed.senvcfg,
           observer.observed.shadow_envcfg, observer.observed.saved_envcfg, observer.fd,
           operation, saved_errno, operation ? "FAIL" : "PASS");
    return failures;
}

int main(void)
{
    unsigned long saved[ARRAY_SIZE(user_csrs)], total = 0;
    unsigned long initial_traps = dasics_n_extension_trap_count;
    unsigned long initial_ustatus = csr_read(CSR_USTATUS);
    unsigned long cfg = dasics_linux_query_umaincfg();
    int failures = 0;
    int profile = cfg == DASICS_UCFG_ENA && initial_traps == 0;
    printf(CSR_TAG " begin version=1 expected_total=%lu\n", CSR_UNIFIED_TOTAL);
    total++;
    failures += !profile;
    printf(CSR_TAG " case=PROFILE umaincfg=0x%lx umaincfg_expected=0x2 "
           "u24=%lu u24_expected=0 result=%s\n", cfg, initial_traps, profile ? "PASS" : "FAIL");
    if (profile) {
        for (unsigned long i = 0; i < ARRAY_SIZE(user_csrs); i++)
            saved[i] = read_csr(user_csrs[i].addr);
        failures += run_patterns(&total);
        failures += run_rmw(&total);
        clear_user_csrs();
        failures += run_zero_operands(&total);
        failures += run_immediates(&total);
        restore_user_csrs(saved);
        failures += run_protection(&total);
        restore_user_csrs(saved);
        int restored = 1;
        for (unsigned long i = 0; i < ARRAY_SIZE(user_csrs); i++) {
            unsigned long actual = read_csr(user_csrs[i].addr);
            if (actual != saved[i]) {
                restored = 0;
                printf(CSR_TAG " diagnostic=RESTORE csr=%s read=0x%lx read_expected=0x%lx result=FAIL\n",
                       user_csrs[i].name, actual, saved[i]);
            }
        }
        total++;
        failures += !restored;
        printf(CSR_TAG " case=STATE-RESTORE value=0x%lx expect=0x1 count=46 result=%s\n",
               (unsigned long)restored, restored ? "PASS" : "FAIL");
        unsigned long traps = dasics_n_extension_trap_count, status = csr_read(CSR_USTATUS);
        int runtime_ok = traps == initial_traps && status == initial_ustatus;
        total++;
        failures += !runtime_ok;
        printf(CSR_TAG " case=N-RUNTIME-GUARD u24=%lu u24_expected=%lu "
               "ustatus=0x%lx ustatus_expected=0x%lx result=%s\n",
               traps, initial_traps, status, initial_ustatus, runtime_ok ? "PASS" : "FAIL");
    }
    if (total != CSR_UNIFIED_TOTAL)
        failures++;
    printf(CSR_TAG " scope=user-bank-state-and-protection cause2_site_evidence=REQUIRED_KERNEL_OBSERVER "
           "clear_readback=CSRRW_OLD_ZERO internal_strobes=NOT_OBSERVED\n");
    printf(CSR_TAG " summary total=%lu failed=%d result=%s\n", total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#endif
