#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ucsr.h"

#define DASICS_CBO_TAG "\033[1;34m[DASICS-CBO-PERMISSION]\033[0m"
#define DASICS_CBO_BEGIN_TAG "\033[1;31m[DASICS-CBO-PERMISSION]"
#define DASICS_CBO_SUMMARY_TAG "\033[1;32m[DASICS-CBO-PERMISSION]"
#define DASICS_CBO_COLOR_END "\033[0m"

#if defined(DASICS_N_EXTENSION_PROFILE) && DASICS_LINUX_DUAL_EXEC && DASICS_VALIDATION_PROFILE
#include <setjmp.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include "udasics.h"
#include "fdi_verify_client.h"

#define DASICS_LIB_CFG_WRITE 0x1UL
#define DASICS_LIB_CFG_READ 0x2UL
#define DASICS_LIB_CFG_VALID 0x8UL
#define DASICS_FREASON_STORE 3UL
#define CBO_LINE 64UL
#define CBO_OK_MARKER 0x43424f4f4b000000UL
#define FAULT_CBO_MARKER 0x43424f4641554c54UL

enum cbo_operation { CBO_ZERO, CBO_INVAL, CBO_CLEAN, CBO_FLUSH, CBO_OPERATIONS };
enum cbo_permission { CBO_R, CBO_W, CBO_RW, CBO_NONE, CBO_HALFLINE, CBO_SPLIT, CBO_PERMISSIONS };
static const char *const cbo_names[] = {"ZERO", "INVAL", "CLEAN", "FLUSH"};
static const char *const permission_names[] = {"R", "W", "RW", "NONE", "HALFLINE", "SPLIT"};

/* The middle block is the operand; both neighboring blocks are byte guards. */
static volatile unsigned char cbo_arena[3 * CBO_LINE] __attribute__((aligned(64)));
static volatile sig_atomic_t cbo_sigill;
static volatile unsigned long cbo_expected_illegal_site;
/* Recover failed legality probes without entering the out-of-bound vDSO. */
static sigjmp_buf cbo_illegal_resume;

#define DECLARE_CBO(name) \
    extern void cbo_trusted_##name(volatile unsigned char *address); \
    extern char cbo_trusted_##name##_site[], cbo_trusted_##name##_recovery[]; \
    extern char cbo_##name##_untrusted_entry[], cbo_##name##_site[], cbo_##name##_recovery[];
DECLARE_CBO(zero)
DECLARE_CBO(inval)
DECLARE_CBO(clean)
DECLARE_CBO(flush)

/* CBO has rd=x0. The untrusted continuation also returns its actual source
 * register so a fault cannot silently corrupt the operand or result marker. */
#define DEFINE_CBO(name, operation) \
    ".section .text,\"ax\",@progbits\n.balign 8\n" \
    ".global cbo_trusted_" #name "\n.type cbo_trusted_" #name ",@function\n" \
    "cbo_trusted_" #name ":\n" \
    ".global cbo_trusted_" #name "_site\ncbo_trusted_" #name "_site:\n" \
    ".insn r 0x0f, 2, 0, x0, a0, x" #operation "\n" \
    ".global cbo_trusted_" #name "_recovery\ncbo_trusted_" #name "_recovery:\nret\n" \
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n" \
    ".global cbo_" #name "_untrusted_entry\n.type cbo_" #name "_untrusted_entry,@function\n" \
    "cbo_" #name "_untrusted_entry:\nmv t0,a1\nli a0,0x43424f4641554c54\n" \
    ".global cbo_" #name "_site\ncbo_" #name "_site:\n" \
    ".insn r 0x0f, 2, 0, x0, t0, x" #operation "\n" \
    "li a0,0x43424f4f4b000000\n" \
    ".global cbo_" #name "_recovery\ncbo_" #name "_recovery:\nmv a1,t0\nret\n"

asm (
    ".option push\n.option norvc\n.option norelax\n"
    DEFINE_CBO(zero, 4)
    DEFINE_CBO(inval, 0)
    DEFINE_CBO(clean, 1)
    DEFINE_CBO(flush, 2)
    ".section .text,\"ax\",@progbits\n.option pop\n"
);

struct cbo_probe {
    void (*trusted)(volatile unsigned char *);
    char *trusted_site, *trusted_recovery;
    char *entry, *site, *recovery;
};
static const struct cbo_probe cbo_probes[] = {
#define CBO_PROBE(name) {cbo_trusted_##name, cbo_trusted_##name##_site, cbo_trusted_##name##_recovery, \
                        cbo_##name##_untrusted_entry, cbo_##name##_site, cbo_##name##_recovery}
    CBO_PROBE(zero), CBO_PROBE(inval), CBO_PROBE(clean), CBO_PROBE(flush),
#undef CBO_PROBE
};

static void cbo_sigill_handler(int signo, siginfo_t *info, void *context)
{
    ucontext_t *uc = context;

    if (signo != SIGILL || !info || info->si_code != ILL_ILLOPC ||
        !cbo_expected_illegal_site || cbo_sigill ||
        (unsigned long)info->si_addr != cbo_expected_illegal_site ||
        uc->uc_mcontext.__gregs[REG_PC] != cbo_expected_illegal_site)
        _exit(111);
    cbo_sigill = 1;
    siglongjmp(cbo_illegal_resume, 1);
}

static int trusted_cbo(enum cbo_operation op, volatile unsigned char *address)
{
    const struct cbo_probe *probe = &cbo_probes[op];

    cbo_sigill = 0;
    if (sigsetjmp(cbo_illegal_resume, 1) == 0) {
        cbo_expected_illegal_site = (unsigned long)probe->trusted_site;
        asm volatile ("fence rw,rw" ::: "memory");
        probe->trusted(address);
    }
    asm volatile ("fence rw,rw" ::: "memory");
    cbo_expected_illegal_site = 0;
    return !cbo_sigill;
}

static unsigned char initial_byte(unsigned int index)
{
    return (unsigned char)(0x21 + index % 127);
}

static void fill_arena(void)
{
    for (unsigned int i = 0; i < sizeof(cbo_arena); i++)
        cbo_arena[i] = initial_byte(i);
}

static int arena_matches(int zeroed)
{
    for (unsigned int i = 0; i < sizeof(cbo_arena); i++) {
        unsigned char expected = zeroed && i >= CBO_LINE && i < 2 * CBO_LINE ?
                                 0 : initial_byte(i);
        if (cbo_arena[i] != expected)
            return 0;
    }
    return 1;
}

static int check_trusted_legality(void)
{
    /* Clean/flush before inval makes the known bytes durable: invalidating a
     * dirty block could otherwise legitimately expose older backing memory. */
    static const enum cbo_operation order[] = {CBO_CLEAN, CBO_FLUSH, CBO_INVAL, CBO_ZERO};

    fill_arena();
    for (unsigned int i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        enum cbo_operation op = order[i];
        int legal = trusted_cbo(op, cbo_arena + CBO_LINE);
        int data_ok = arena_matches(op == CBO_ZERO);
        int pass = legal && data_ok;

        printf(DASICS_CBO_TAG " prerequisite=TRUSTED-%s legal=%d expect_legal=1 data_ok=%d expect_data_ok=1 result=%s\n",
               cbo_names[op], legal, data_ok, pass ? "PASS" : "FAIL");
        if (!pass)
            return 0;
    }
    return 1;
}

struct cbo_owned_state {
    unsigned long libcfg, bound0_lo, bound0_hi, bound1_lo, bound1_hi;
    unsigned long return_pc, freason, uscratch;
};

static struct cbo_owned_state read_owned_state(void)
{
    struct cbo_owned_state state = {
        .libcfg = csr_read(0x880),
        .bound0_lo = csr_read(0x890), .bound0_hi = csr_read(0x891),
        .bound1_lo = csr_read(0x892), .bound1_hi = csr_read(0x893),
        .return_pc = csr_read(0x8b1), .freason = csr_read(0x8b3),
        .uscratch = csr_read(CSR_USCRATCH),
    };
    return state;
}

static int restore_owned_state(const struct cbo_owned_state *expected)
{
    /* Revoke validity while restoring bounds; retain the runtime's trap
     * records and post-trap USTATUS rather than resetting its observations. */
    csr_write(0x880, 0);
    csr_write(0x890, expected->bound0_lo);
    csr_write(0x891, expected->bound0_hi);
    csr_write(0x892, expected->bound1_lo);
    csr_write(0x893, expected->bound1_hi);
    csr_write(0x8b1, expected->return_pc);
    csr_write(0x8b3, expected->freason);
    csr_write(CSR_USCRATCH, expected->uscratch);
    csr_write(0x880, expected->libcfg);
    struct cbo_owned_state actual = read_owned_state();
    int pass = !memcmp(&actual, expected, sizeof(actual));

    printf(DASICS_CBO_TAG " cleanup=STATE-RESTORE libcfg=0x%lx expect_libcfg=0x%lx "
           "bound0_lo=0x%lx expect_bound0_lo=0x%lx bound0_hi=0x%lx expect_bound0_hi=0x%lx "
           "bound1_lo=0x%lx expect_bound1_lo=0x%lx bound1_hi=0x%lx expect_bound1_hi=0x%lx "
           "return_pc=0x%lx expect_return_pc=0x%lx freason=0x%lx expect_freason=0x%lx "
           "uscratch=0x%lx expect_uscratch=0x%lx result=%s\n",
           actual.libcfg, expected->libcfg, actual.bound0_lo, expected->bound0_lo,
           actual.bound0_hi, expected->bound0_hi, actual.bound1_lo, expected->bound1_lo,
           actual.bound1_hi, expected->bound1_hi, actual.return_pc, expected->return_pc,
           actual.freason, expected->freason, actual.uscratch, expected->uscratch,
           pass ? "PASS" : "FAIL");
    return pass;
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

static int configure_permission(enum cbo_permission mode)
{
    unsigned long base = (unsigned long)(cbo_arena + CBO_LINE);
    unsigned long cfg = 0, lo = 0, hi = 0, lo1 = 0, hi1 = 0;

    clear_lib_permission_csrs();
    if (mode != CBO_NONE) {
        unsigned long access = mode == CBO_R ? DASICS_LIB_CFG_READ :
                               (mode == CBO_W || mode == CBO_HALFLINE ? DASICS_LIB_CFG_WRITE :
                                DASICS_LIB_CFG_READ | DASICS_LIB_CFG_WRITE);
        lo = base;
        hi = base + (mode == CBO_HALFLINE || mode == CBO_SPLIT ? CBO_LINE / 2 : CBO_LINE);
        cfg = DASICS_LIB_CFG_VALID | access;
        if (mode == CBO_SPLIT) {
            lo1 = base + CBO_LINE / 2;
            hi1 = base + CBO_LINE;
            cfg |= (DASICS_LIB_CFG_VALID | access) << 4;
        }
    }
    csr_write(0x890, lo);
    csr_write(0x891, hi);
    csr_write(0x892, lo1);
    csr_write(0x893, hi1);
    csr_write(0x880, cfg);
    return csr_read(0x880) == cfg && csr_read(0x890) == lo && csr_read(0x891) == hi &&
           csr_read(0x892) == lo1 && csr_read(0x893) == hi1;
}

/* Keep one return symbol so host checks can bind observations to this ELF. */
static __attribute__((noinline)) unsigned long invoke_cbo(const struct cbo_probe *probe, volatile unsigned char *address,
                                unsigned long *source, unsigned long *return_pc)
{
    unsigned long result, observed_source, expected_return_pc;

    /* The handler skips the success marker on a fault. The trusted wrapper
     * clears unused recovery state when the instruction completes normally. */
    csr_write(CSR_USCRATCH, (unsigned long)probe->recovery);
    asm volatile (
        ".option push\n.option norvc\n.option norelax\n"
        "addi sp,sp,-16\nsd ra,8(sp)\nmv a0,%3\nmv a1,%4\n"
        ".word 0x000510fb\n"
        ".global cbo_probe_return\ncbo_probe_return:\n"
        "1:\ncsrw uscratch,zero\nmv %0,a0\nmv %1,a1\nla %2,1b\n"
        "ld ra,8(sp)\naddi sp,sp,16\n.option pop\n"
        : "=r"(result), "=r"(observed_source), "=r"(expected_return_pc)
        : "r"(probe->entry), "r"(address)
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
    *source = observed_source;
    *return_pc = expected_return_pc;
    return result;
}

static int permission_allows(enum cbo_operation op, enum cbo_permission mode)
{
    if (mode == CBO_R)
        return op == CBO_CLEAN || op == CBO_FLUSH;
    return mode == CBO_W || mode == CBO_RW;
}

static int run_case(const char *name, const char *reason_name, enum cbo_operation op,
                    enum cbo_permission mode, unsigned int offset, unsigned long *total)
{
    const struct cbo_probe *probe = &cbo_probes[op];
    volatile unsigned char *address = cbo_arena + CBO_LINE + offset;
    int allow = permission_allows(op, mode);
    unsigned long expected = allow ? CBO_OK_MARKER : FAULT_CBO_MARKER;
    unsigned long expected_reason = allow ? 0 : DASICS_FREASON_STORE;
    unsigned long source, expected_return_pc;

    fill_arena();
    /* Preserve both the block and its guards across an allowed invalidate. */
    for (unsigned int i = 0; i < 3; i++) {
        if (!trusted_cbo(CBO_FLUSH, cbo_arena + i * CBO_LINE)) {
            printf(DASICS_CBO_TAG " prerequisite=PREPARE-FLUSH legal=0 expect_legal=1 result=FAIL\n");
            return -1;
        }
    }
    int configured = configure_permission(mode);
    unsigned long before = dasics_n_extension_trap_count;
    unsigned long result = invoke_cbo(probe, address, &source, &expected_return_pc);
    asm volatile ("fence rw,rw" ::: "memory");
    unsigned long reason = csr_read(0x8b3);
    unsigned long after = dasics_n_extension_trap_count;
    unsigned long return_pc = csr_read(0x8b1);
    int data_ok = arena_matches(allow && op == CBO_ZERO);
    int gpr_ok = source == (unsigned long)address;
    int trap_ok = after == before + !allow && csr_read(CSR_USCRATCH) == 0;

    if (!allow) {
        if (before >= DASICS_N_EXTENSION_TRAP_RECORDS) {
            trap_ok = 0;
        } else {
            const volatile dasics_n_extension_trap_record_t *record =
                &dasics_n_extension_trap_records[before];
            trap_ok = trap_ok && record->ucause == DASICS_N_EXTENSION_UCHECK_CAUSE &&
                record->uepc == (unsigned long)probe->site && record->utval == (unsigned long)address &&
                record->dfreason == DASICS_FREASON_STORE &&
                record->uscratch == (unsigned long)probe->recovery &&
                record->recovery == (unsigned long)probe->recovery;
            gpr_ok = gpr_ok && record->gpr[5] == (unsigned long)address &&
                     record->gpr[10] == FAULT_CBO_MARKER;
        }
    }
    int value_pass = result == expected && data_ok && gpr_ok;
    int reason_pass = configured && reason == expected_reason && trap_ok && return_pc == expected_return_pc;
    (*total)++;
    printf(DASICS_CBO_TAG " case=%s value=0x%lx expect=0x%lx source=0x%lx expect_source=0x%lx data_ok=%d gpr_ok=%d result=%s\n",
           name, result, expected, source, (unsigned long)address, data_ok, gpr_ok,
           value_pass ? "PASS" : "FAIL");
    (*total)++;
    printf(DASICS_CBO_TAG " case=%s value=0x%lx expect=0x%lx configured=%d traps=%lu->%lu trap_ok=%d return_pc=0x%lx expect_return_pc=0x%lx result=%s\n",
           reason_name, reason, expected_reason, configured, before, after, trap_ok,
           return_pc, expected_return_pc, reason_pass ? "PASS" : "FAIL");
    return !value_pass + !reason_pass;
}

static int run_legacy(unsigned long *total)
{
    static const struct {
        enum cbo_operation op;
        enum cbo_permission mode;
        const char *name, *reason_name;
    } cases[] = {
        {CBO_ZERO, CBO_W, "CBO-PERMISSION-ALLOW-ZERO-W", "CBO-PERMISSION-ALLOW-ZERO-FREASON"},
        {CBO_INVAL, CBO_W, "CBO-PERMISSION-ALLOW-INVAL-W", "CBO-PERMISSION-ALLOW-INVAL-FREASON"},
        {CBO_CLEAN, CBO_R, "CBO-PERMISSION-ALLOW-CLEAN-R", "CBO-PERMISSION-ALLOW-CLEAN-FREASON"},
        {CBO_FLUSH, CBO_W, "CBO-PERMISSION-ALLOW-FLUSH-W", "CBO-PERMISSION-ALLOW-FLUSH-FREASON"},
        {CBO_ZERO, CBO_NONE, "CBO-PERMISSION-FAULT-ZERO-NOBOUND", "CBO-PERMISSION-FREASON-ZERO-NOBOUND"},
        {CBO_ZERO, CBO_HALFLINE, "CBO-PERMISSION-FAULT-ZERO-PARTIAL-LINE", "CBO-PERMISSION-FREASON-ZERO-PARTIAL-LINE"},
    };
    int failures = 0;

    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int ret = run_case(cases[i].name, cases[i].reason_name, cases[i].op, cases[i].mode, 0, total);
        if (ret < 0)
            return ret;
        failures += ret;
    }
    return failures;
}

static int run_extended(unsigned long *total)
{
    static const unsigned int offsets[] = {1, CBO_LINE - 1};
    char name[96], reason_name[112];
    int failures = 0;

    for (unsigned int op = 0; op < CBO_OPERATIONS; op++) {
        for (unsigned int mode = 0; mode < CBO_PERMISSIONS; mode++) {
            snprintf(name, sizeof(name), "CBO-PERMISSION-MATRIX-%s-%s", cbo_names[op], permission_names[mode]);
            snprintf(reason_name, sizeof(reason_name), "%s-FREASON", name);
            int ret = run_case(name, reason_name, op, mode, 0, total);
            if (ret < 0)
                return ret;
            failures += ret;
        }
    }
    for (unsigned int op = 0; op < CBO_OPERATIONS; op++) {
        for (unsigned int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
            snprintf(name, sizeof(name), "CBO-PERMISSION-OFFSET-%s-%u-RW", cbo_names[op], offsets[i]);
            snprintf(reason_name, sizeof(reason_name), "%s-FREASON", name);
            int ret = run_case(name, reason_name, op, CBO_RW, offsets[i], total);
            if (ret < 0)
                return ret;
            failures += ret;
        }
    }
    return failures;
}

int main(int argc, char *argv[])
{
    struct sigaction action = {0}, previous;
    unsigned long total = 0;
    int extended = argc == 2 && argv && argv[1] && !strcmp(argv[1], "--isa-extended");
    int failures = 1, status = 2;
    int handler_installed = 0;
    struct fdi_verify_client environment = {.fd = -1};

    if (argc != 1 && !extended) {
        fprintf(stderr, DASICS_CBO_TAG " argument-contract argc=%d result=FAIL\n", argc);
        return 2;
    }
    struct cbo_owned_state original = read_owned_state();
    printf(DASICS_CBO_BEGIN_TAG " cbo permission smoke begin" DASICS_CBO_COLOR_END "\n");
    printf(DASICS_CBO_TAG " process=SELF pid=%ld\n", (long)getpid());
    unsigned long cfg = dasics_linux_query_umaincfg();
    if (cfg != DASICS_UCFG_ENA) {
        printf(DASICS_CBO_TAG " prerequisite=UENA value=0x%lx expect=0x%lx result=FAIL\n", cfg, DASICS_UCFG_ENA);
        goto done;
    }
    if (fdi_verify_begin(&environment, FDI_VERIFY_CBO)) {
        printf(DASICS_CBO_TAG " prerequisite=ENVIRONMENT errno=%d result=FAIL\n", errno);
        goto done;
    }
    printf("DASICS_CBO_ENV version=%llu pid=%llu cookie=0x%llx menvcfg=0x%llx "
           "senvcfg=0x%llx shadow_envcfg=0x%llx saved_envcfg=0x%llx "
           "cbom_block_size=%llu cboz_block_size=%llu result=PASS\n",
           environment.observed.version, environment.observed.pid, environment.observed.cookie,
           environment.observed.menvcfg, environment.observed.senvcfg,
           environment.observed.shadow_envcfg, environment.observed.saved_envcfg,
           environment.observed.cbom_block_size, environment.observed.cboz_block_size);
    action.sa_sigaction = cbo_sigill_handler;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGILL, &action, &previous)) {
        printf(DASICS_CBO_TAG " prerequisite=SIGILL-HANDLER result=FAIL\n");
        goto done;
    }
    handler_installed = 1;
    if (check_trusted_legality()) {
        failures = extended ? run_extended(&total) : run_legacy(&total);
        unsigned long expected_traps = extended ? 14 : 2;
        if (failures < 0) {
            failures = 1;
        } else {
            if (dasics_n_extension_trap_count != expected_traps)
                failures++;
            status = failures ? 1 : 0;
        }
    }
done:
    if (handler_installed && sigaction(SIGILL, &previous, NULL)) {
        failures++;
        status = 2;
    }
    if (environment.fd >= 0) {
        int observed = fdi_verify_query(&environment) == 0;
        int illegal_ok = observed && environment.observed.illegal_count == 0;
        printf("DASICS_CBO_ILLEGAL pid=%llu cookie=0x%llx count=%llu expected=0 result=%s\n",
               environment.observed.pid, environment.observed.cookie,
               environment.observed.illegal_count, illegal_ok ? "PASS" : "FAIL");
        if (!illegal_ok) {
            failures++;
            status = 1;
        }
        int ended = fdi_verify_end(&environment) == 0;
        printf("DASICS_CBO_ENV_RESTORE pid=%llu cookie=0x%llx active=%llu "
               "senvcfg=0x%llx shadow_envcfg=0x%llx saved_envcfg=0x%llx result=%s\n",
               environment.observed.pid, environment.observed.cookie, environment.observed.active,
               environment.observed.senvcfg, environment.observed.shadow_envcfg,
               environment.observed.saved_envcfg, ended ? "PASS" : "FAIL");
        if (!ended) {
            failures++;
            status = 1;
        }
    }
    if (!restore_owned_state(&original)) {
        failures++;
        if (!status)
            status = 1;
    }
    printf(DASICS_CBO_TAG " scope=permission-and-zero-data block_bytes=64 cbie_required=INVAL cache_maintenance_effect=NOT_OBSERVED\n");
    printf(DASICS_CBO_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_CBO_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");
    return status;
}
#else
int main(void)
{
    fprintf(stderr, DASICS_CBO_TAG " requires Linux N-profile result=FAIL\n");
    return 2;
}
#endif
