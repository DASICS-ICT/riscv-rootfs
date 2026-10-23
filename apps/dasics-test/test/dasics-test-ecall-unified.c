#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "udasics.h"

#define ECALL_TAG "[DASICS-ECALL-UNIFIED]"
#define ECALL_TOTAL 16UL
#define ECALL_GETPID 306UL
#define ECALL_SEED 0x4543414c4c464c54UL
#define ECALL_CONTINUED 0x4543414c4c524543UL
#define ECALL_REASON_SEED 5UL
#define ECALL_CHILD_MAGIC 0x4543414c4348494cUL

#if !defined(DASICS_N_EXTENSION_PROFILE) || !DASICS_LINUX_DUAL_EXEC
int main(void)
{
    fprintf(stderr, ECALL_TAG " requires Linux N-profile result=FAIL\n");
    return 2;
}
#else

struct ecall_result {
    unsigned long value, continued;
};

struct owned_state {
    unsigned long return_pc, active_return, freason, uscratch;
};

struct ecall_counts {
    unsigned long traps, syscalls, permitted, denied;
};

/* CUET has a one-way per-task enable interface. The child owns that transition;
 * its fixed-size report lets the parent verify isolation after waitpid. */
struct child_result {
    unsigned long magic, pid, config, reason, status_before, status_after;
    long enable;
    struct ecall_result probe;
    struct owned_state saved, after, restored;
    struct ecall_counts before_counts, after_counts;
    int restore_ok;
};

extern struct ecall_result ecall_unified_invoke(unsigned long seed);
extern char ecall_unified_site[], ecall_unified_recovery[];

/* Ordinary CALL plus ActiveReturn retains ReturnPC. uscratch selects the same
 * continuation as an ordinary ECALL return, and is cleared on both routes. */
asm(".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global ecall_unified_invoke\n.type ecall_unified_invoke,@function\n"
    "ecall_unified_invoke:\naddi sp,sp,-16\nsd ra,8(sp)\n"
    "la t0,1f\ncsrw 0x8b2,t0\n"
    "la t0,ecall_unified_recovery\ncsrw 0x040,t0\n"
    "call ecall_unified_untrusted\n"
    "1:\ncsrw 0x040,zero\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size ecall_unified_invoke,.-ecall_unified_invoke\n"
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".type ecall_unified_untrusted,@function\necall_unified_untrusted:\n"
    "li a7,306\n.global ecall_unified_site\necall_unified_site:\necall\n"
    ".global ecall_unified_recovery\necall_unified_recovery:\n"
    "li a1,0x4543414c4c524543\nret\n"
    ".size ecall_unified_untrusted,.-ecall_unified_untrusted\n"
    ".section .text,\"ax\",@progbits\n.option pop\n");

static unsigned long trusted_getpid(void)
{
    register unsigned long a0 asm("a0") = 0;
    register unsigned long a7 asm("a7") = ECALL_GETPID;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static struct owned_state read_owned(void)
{
    return (struct owned_state){csr_read(CSR_DRETURNPC), csr_read(CSR_DFZRETURN),
        csr_read(CSR_DFREASON), csr_read(CSR_USCRATCH)};
}

static int owned_equal(const struct owned_state *left, const struct owned_state *right)
{
    return left->return_pc == right->return_pc && left->active_return == right->active_return &&
        left->freason == right->freason && left->uscratch == right->uscratch;
}

static struct owned_state restore_owned(const struct owned_state *saved)
{
    csr_write(CSR_DRETURNPC, saved->return_pc);
    csr_write(CSR_DFZRETURN, saved->active_return);
    csr_write(CSR_DFREASON, saved->freason);
    csr_write(CSR_USCRATCH, saved->uscratch);
    return read_owned();
}

static struct ecall_counts read_counts(void)
{
    return (struct ecall_counts){dasics_n_extension_trap_count,
        dasics_n_extension_syscall_trap_count, dasics_n_extension_syscall_permitted_count,
        dasics_n_extension_syscall_denied_count};
}

static int counts_equal(const struct ecall_counts *left, const struct ecall_counts *right)
{
    return left->traps == right->traps && left->syscalls == right->syscalls &&
        left->permitted == right->permitted && left->denied == right->denied;
}

static int record_case(const char *name, unsigned long actual, unsigned long expected,
                       int context_ok, unsigned long *total)
{
    int pass = actual == expected && context_ok;
    (*total)++;
    printf(ECALL_TAG " case=%s value=0x%lx expect=0x%lx context_ok=%d result=%s\n",
           name, actual, expected, context_ok, pass ? "PASS" : "FAIL");
    return !pass;
}

static void print_counts(const char *phase, const struct ecall_counts *actual,
                          const struct ecall_counts *expected)
{
    printf("DASICS_ECALL_COUNTS phase=%s traps=%lu expect_traps=%lu syscalls=%lu "
           "expect_syscalls=%lu permitted=%lu expect_permitted=%lu denied=%lu expect_denied=%lu\n",
           phase, actual->traps, expected->traps, actual->syscalls, expected->syscalls,
           actual->permitted, expected->permitted, actual->denied, expected->denied);
}

static void print_state(const char *phase, const struct owned_state *actual,
                         const struct owned_state *expected, int available)
{
    printf("DASICS_ECALL_STATE phase=%s available=%d returnpc=0x%lx expect_returnpc=0x%lx "
           "active_return=0x%lx expect_active_return=0x%lx freason=0x%lx expect_freason=0x%lx "
           "uscratch=0x%lx expect_uscratch=0x%lx result=%s\n",
           phase, available, actual->return_pc, expected->return_pc, actual->active_return,
           expected->active_return, actual->freason, expected->freason,
           actual->uscratch, expected->uscratch,
           available && owned_equal(actual, expected) ? "PASS" : "FAIL");
}

static void run_child(int descriptor)
{
    struct child_result report = {.magic = ECALL_CHILD_MAGIC};
    size_t sent = 0;
    alarm(10);
    report.pid = getpid();
    report.saved = read_owned();
    report.status_before = csr_read(CSR_USTATUS);
    report.before_counts = read_counts();
    report.enable = dasics_linux_enable_cuet();
    report.config = dasics_linux_query_umaincfg();
    if (report.enable == (DASICS_UCFG_ENA | DASICS_UCFG_CUET) &&
        report.config == (DASICS_UCFG_ENA | DASICS_UCFG_CUET)) {
        csr_write(CSR_DFREASON, ECALL_REASON_SEED);
        report.probe = ecall_unified_invoke(ECALL_SEED);
    }
    report.reason = csr_read(CSR_DFREASON);
    report.after = read_owned();
    report.after_counts = read_counts();
    report.status_after = csr_read(CSR_USTATUS);
    report.restored = restore_owned(&report.saved);
    report.restore_ok = owned_equal(&report.restored, &report.saved);
    while (sent < sizeof(report)) {
        ssize_t count = write(descriptor, (const char *)&report + sent, sizeof(report) - sent);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            break;
        sent += count;
    }
    close(descriptor);
    /* Do not dump the parent's inherited N records a second time. Task exit
     * removes the child's CUET setting; no userspace disable interface exists. */
    _exit(report.restore_ok && sent == sizeof(report) ? 0 : 1);
}

static int run_cuet_child(const struct owned_state *parent_state,
                           const struct ecall_counts *parent_counts, unsigned long *total)
{
    struct child_result report = {0};
    int channel[2], status = -1, process_errno = 0, child_ok = 0;
    pid_t pid = -1, waited = -1;
    size_t received = 0;
    unsigned long parent_status = csr_read(CSR_USTATUS);
    if (!pipe(channel)) {
        fflush(NULL);
        pid = fork();
        if (!pid) {
            close(channel[0]);
            run_child(channel[1]);
        }
        if (pid < 0)
            process_errno = errno;
        close(channel[1]);
        if (pid > 0) {
            while (received < sizeof(report)) {
                ssize_t count = read(channel[0], (char *)&report + received, sizeof(report) - received);
                if (count < 0 && errno == EINTR)
                    continue;
                if (count < 0)
                    process_errno = errno;
                if (count <= 0)
                    break;
                received += count;
            }
            do {
                waited = waitpid(pid, &status, 0);
            } while (waited < 0 && errno == EINTR);
            if (waited < 0)
                process_errno = errno;
            child_ok = received == sizeof(report) && waited == pid &&
                WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                report.magic == ECALL_CHILD_MAGIC && report.pid == (unsigned long)pid &&
                report.restore_ok && owned_equal(&report.saved, parent_state) &&
                owned_equal(&report.restored, &report.saved) &&
                counts_equal(&report.before_counts, parent_counts) &&
                report.status_before == parent_status;
        }
        close(channel[0]);
    } else {
        process_errno = errno;
    }
    unsigned long config = dasics_linux_query_umaincfg();
    unsigned long child_config = DASICS_UCFG_ENA | DASICS_UCFG_CUET;
    struct owned_state after = read_owned();
    struct ecall_counts counts = read_counts();
    int failures = 0;
    printf("DASICS_ECALL_CHILD pid=%ld report_pid=%lu waited=%ld status=%d received=%lu "
           "expect_received=%lu errno=%d magic=0x%lx expect_magic=0x%lx enable=0x%lx "
           "config=0x%lx expect_config=0x%lx value=0x%lx expect_value=0x%lx "
           "continued=0x%lx expect_continued=0x%lx status_before=0x%lx status_after=0x%lx "
           "expect_status=0x%lx returnpc=0x%lx expect_returnpc=0x%lx "
           "uscratch=0x%lx expect_uscratch=0 freason=0x%lx expect_freason=0x%lx "
           "restore_ok=%d child_ok=%d\n",
           (long)pid, report.pid, (long)waited, status, (unsigned long)received,
           (unsigned long)sizeof(report), process_errno, report.magic, ECALL_CHILD_MAGIC,
           (unsigned long)report.enable, report.config, child_config, report.probe.value,
           (unsigned long)pid, report.probe.continued, ECALL_CONTINUED, report.status_before,
           report.status_after, parent_status, report.after.return_pc, parent_state->return_pc,
           report.after.uscratch, report.reason, ECALL_REASON_SEED, report.restore_ok, child_ok);
    print_counts("CHILD-BEFORE", &report.before_counts, parent_counts);
    print_counts("CHILD-AFTER", &report.after_counts, &report.before_counts);
    print_state("CHILD-RESTORE", &report.restored, &report.saved,
                received == sizeof(report) && report.magic == ECALL_CHILD_MAGIC);
    failures += record_case("CUET-ENABLE", report.enable, child_config,
        child_ok && report.config == child_config, total);
    failures += record_case("CUET-GETPID", report.probe.value, (unsigned long)pid,
        child_ok && (long)report.probe.value > 0, total);
    failures += record_case("CUET-FREASON", report.reason, ECALL_REASON_SEED, child_ok, total);
    failures += record_case("CUET-ROUTING", report.after_counts.traps - report.before_counts.traps, 0,
        child_ok && counts_equal(&report.after_counts, &report.before_counts) &&
        report.status_after == report.status_before, total);
    failures += record_case("CUET-CONTROL", report.probe.continued, ECALL_CONTINUED,
        child_ok && report.after.return_pc == report.saved.return_pc && report.after.uscratch == 0, total);
    failures += record_case("PARENT-ISOLATION", config, DASICS_UCFG_ENA,
        child_ok && owned_equal(&after, parent_state) && counts_equal(&counts, parent_counts), total);
    return failures;
}

int main(void)
{
    struct owned_state saved = read_owned();
    struct ecall_counts initial = read_counts(), counts;
    unsigned long initial_status = csr_read(CSR_USTATUS);
    unsigned long config = dasics_linux_query_umaincfg(), total = 0;
    unsigned long pid = getpid(), result, reason;
    unsigned long vector = csr_read(CSR_UTVEC);
    int failures = 0;
    int profile = config == DASICS_UCFG_ENA &&
        (initial_status & DASICS_N_EXTENSION_USTATUS_UIE) &&
        (vector & ~3UL) == (uintptr_t)dasics_ufault_entry &&
        initial.traps < DASICS_N_EXTENSION_TRAP_RECORDS &&
        initial.syscalls < DASICS_N_EXTENSION_SYSCALL_RECORDS && pid > 0;
    printf(ECALL_TAG " begin version=1 expected_total=%lu\n", ECALL_TOTAL);
    printf("DASICS_ECALL_PROFILE pid=%lu umaincfg=0x%lx expect_umaincfg=0x2 "
           "ustatus=0x%lx utvec=0x%lx expect_utvec=0x%lx trap_index=%lu syscall_index=%lu\n",
           pid, config, initial_status, vector, (uintptr_t)dasics_ufault_entry,
           initial.traps, initial.syscalls);
    failures += record_case("PROFILE", config, DASICS_UCFG_ENA, profile, &total);
    if (profile) {
        csr_write(CSR_DFREASON, ECALL_REASON_SEED);
        result = trusted_getpid();
        reason = csr_read(CSR_DFREASON);
        counts = read_counts();
        failures += record_case("TRUSTED-GETPID", result, pid, (long)result > 0, &total);
        failures += record_case("TRUSTED-FREASON", reason, ECALL_REASON_SEED, 1, &total);
        print_counts("TRUSTED", &counts, &initial);
        failures += record_case("TRUSTED-ROUTING", counts.traps - initial.traps, 0,
                                 counts_equal(&counts, &initial), &total);

        csr_write(CSR_DFREASON, 0);
        struct ecall_result probe = ecall_unified_invoke(ECALL_SEED);
        struct owned_state off_state = read_owned();
        counts = read_counts();
        struct ecall_counts expected = {initial.traps + 1, initial.syscalls + 1,
            initial.permitted, initial.denied + 1};
        const volatile dasics_n_extension_trap_record_t *trap =
            &dasics_n_extension_trap_records[initial.traps];
        const volatile dasics_n_extension_syscall_record_t *syscall =
            &dasics_n_extension_syscall_records[initial.syscalls];
        unsigned long site = (uintptr_t)ecall_unified_site;
        unsigned long recovery = (uintptr_t)ecall_unified_recovery;
        int context_ok = counts.traps == initial.traps + 1 && trap->ucause == 24 &&
            trap->ustatus == 0x10 && trap->uepc == site && trap->utval == 0 &&
            trap->dfreason == EXC_DASICS_ECALL_FAULT && trap->uscratch == recovery &&
            trap->recovery == recovery && trap->utvec == vector &&
            trap->gpr[0] == 0 && trap->gpr[10] == ECALL_SEED && trap->gpr[17] == ECALL_GETPID;
        int policy_ok = counts_equal(&counts, &expected) && syscall->ucause == 24 &&
            syscall->ustatus == 0x10 && syscall->uepc == site && syscall->utval == 0 &&
            syscall->dfreason == EXC_DASICS_ECALL_FAULT && syscall->result == (long)ECALL_SEED;
        printf("DASICS_ECALL_U24 sequence=%lu syscall_sequence=%lu site=0x%lx recovery=0x%lx "
               "ucause=0x%lx expect_ucause=0x18 ustatus=0x%lx expect_ustatus=0x10 "
               "uepc=0x%lx expect_uepc=0x%lx utval=0x%lx expect_utval=0 "
               "freason=0x%lx expect_freason=0x1 uscratch=0x%lx expect_uscratch=0x%lx "
               "record_recovery=0x%lx expect_recovery=0x%lx a0=0x%lx expect_a0=0x%lx "
               "a7=0x%lx expect_a7=0x%lx\n",
               initial.traps + 1, initial.syscalls + 1, site, recovery, trap->ucause,
               trap->ustatus, trap->uepc, site, trap->utval, trap->dfreason,
               trap->uscratch, recovery, trap->recovery, recovery, trap->gpr[10], ECALL_SEED,
               trap->gpr[17], ECALL_GETPID);
        printf("DASICS_ECALL_POLICY sequence=%lu ucause=0x%lx expect_ucause=0x18 "
               "ustatus=0x%lx expect_ustatus=0x10 uepc=0x%lx expect_uepc=0x%lx "
               "utval=0x%lx expect_utval=0 freason=0x%lx expect_freason=0x1 "
               "permitted=%lu expect_permitted=0 result=0x%lx expect_result=0x%lx "
               "policy=NONPROXY_DENY\n",
               initial.syscalls + 1, syscall->ucause, syscall->ustatus, syscall->uepc, site,
               syscall->utval, syscall->dfreason, syscall->permitted,
               (unsigned long)syscall->result, ECALL_SEED);
        printf("DASICS_ECALL_CONTROL phase=OFF returnpc=0x%lx expect_returnpc=0x%lx "
               "uscratch=0x%lx expect_uscratch=0 continued=0x%lx expect_continued=0x%lx\n",
               off_state.return_pc, saved.return_pc, off_state.uscratch,
               probe.continued, ECALL_CONTINUED);
        failures += record_case("UNTRUSTED-GETPID-DENY", probe.value, ECALL_SEED, 1, &total);
        failures += record_case("UNTRUSTED-FREASON", off_state.freason, EXC_DASICS_ECALL_FAULT, 1, &total);
        failures += record_case("UNTRUSTED-U24-CONTEXT", trap->ucause, 24, context_ok, &total);
        failures += record_case("UNTRUSTED-SYSCALL-POLICY", syscall->permitted, 0, policy_ok, &total);
        failures += record_case("UNTRUSTED-CONTROL", probe.continued, ECALL_CONTINUED,
            off_state.return_pc == saved.return_pc && off_state.uscratch == 0, &total);
        print_counts("OFF", &counts, &expected);
        failures += run_cuet_child(&off_state, &counts, &total);
        counts = read_counts();
        unsigned long status = csr_read(CSR_USTATUS);
        printf("DASICS_ECALL_RUNTIME ustatus=0x%lx expect_ustatus=0x%lx\n",
               status, initial_status | DASICS_N_EXTENSION_USTATUS_UPIE);
        print_counts("FINAL", &counts, &expected);
        failures += record_case("N-RUNTIME-GUARD", counts.traps - initial.traps, 1,
            counts_equal(&counts, &expected) &&
            status == (initial_status | DASICS_N_EXTENSION_USTATUS_UPIE), &total);
    }
    struct owned_state restored = profile ? restore_owned(&saved) : read_owned();
    int restore_ok = owned_equal(&restored, &saved);
    failures += !restore_ok;
    print_state("PARENT-RESTORE", &restored, &saved, 1);
    printf(ECALL_TAG " cleanup=STATE-RESTORE value=0x%lx expect=0x1 result=%s\n",
           (unsigned long)restore_ok, restore_ok ? "PASS" : "FAIL");
    if (total != ECALL_TOTAL)
        failures++;
    printf(ECALL_TAG " scope=ecall-routing-and-nonproxy-policy cuet_restore=CHILD_EXIT "
           "buffer_permission=SEPARATE_SYSCALL_GROUP\n");
    printf(ECALL_TAG " summary total=%lu failed=%d result=%s\n",
           total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#endif
