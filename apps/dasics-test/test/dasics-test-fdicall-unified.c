#ifndef DASICS_LINUX_DUAL_EXEC
#define DASICS_LINUX_DUAL_EXEC 0
#endif
#ifndef DASICS_VALIDATION_PROFILE
#define DASICS_VALIDATION_PROFILE 0
#endif
#if DASICS_LINUX_DUAL_EXEC
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#if DASICS_LINUX_DUAL_EXEC
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include "udasics.h"
#if DASICS_VALIDATION_PROFILE
#include "fdi_verify_client.h"
#endif
#endif

#include "ucsr.h"

#define DASICS_CALL_TAG "\033[1;34m[DASICS-FDICALL-UNIFIED]\033[0m"
#define DASICS_CALL_BEGIN_TAG "\033[1;31m[DASICS-FDICALL-UNIFIED]"
#define DASICS_CALL_SUMMARY_TAG "\033[1;32m[DASICS-FDICALL-UNIFIED]"
#define DASICS_CALL_COLOR_END "\033[0m"

/* Imm_DASICSJ maps offset[22:1] to instruction fields 31, 7, 20:15,
 * 11:8 and 30:21. Local label differences need no shifted relocations. */
#define DASICS_CALL_J_WORD(offset) \
    ".word (0x7b | ((((" offset ") >> 22) & 1) << 31)" \
    " | ((((" offset ") >> 21) & 1) << 7)" \
    " | ((((" offset ") >> 15) & 0x3f) << 15)" \
    " | ((((" offset ") >> 11) & 0xf) << 8)" \
    " | ((((" offset ") >> 1) & 0x3ff) << 21))\n"

static volatile unsigned long call_jr_returnpc_hits;

static unsigned long read_dasics_return_pc(void)
{
    return csr_read(0x8b1);
}

static __attribute__((noinline, used)) void call_jr_returnpc_target(void)
{
    call_jr_returnpc_hits++;
}

static __attribute__((noinline, used)) unsigned long call_jr_returnpc_invoke(unsigned long *link)
{
    unsigned long expected_return_pc;
    unsigned long observed_link;

    asm volatile (
        ".option push\n"
        ".option norvc\n"
        ".option norelax\n"
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, call_jr_returnpc_target\n"
        ".word 0x000510fb\n"
        "1:\n"
        "mv %1, ra\n"
        "la %0, 1b\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        ".option pop\n"
        : "=r"(expected_return_pc), "=r"(observed_link)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    *link = observed_link;
    return expected_return_pc;
}

static int run_trusted_call_jr_returnpc_check(unsigned long *total)
{
    unsigned long hits_before = call_jr_returnpc_hits;
    unsigned long link;
    unsigned long expected_return_pc = call_jr_returnpc_invoke(&link);
    unsigned long return_pc = read_dasics_return_pc();
    int pass = call_jr_returnpc_hits == hits_before + 1 &&
               link == expected_return_pc &&
               return_pc == expected_return_pc;

    (*total)++;
    printf(DASICS_CALL_TAG " case=CALL-JR-RETURNPC-TRUSTED hits_before=%lu hits_after=%lu link=0x%lx return_pc=0x%lx expect=0x%lx result=%s\n",
           hits_before, call_jr_returnpc_hits, link, return_pc,
           expected_return_pc, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int run_trusted_call_j_returnpc_check(unsigned long *total, int negative)
{
    unsigned long hits, link, return_pc, expected_return_pc;

    /* Both paths return with ordinary JALR and capture x1 before restoring
     * the caller's x1. The label immediately after the word is its PC + 4. */
    if (!negative) {
        asm volatile (
            ".option push\n.option norvc\n.option norelax\n"
            "addi sp, sp, -16\nsd ra, 8(sp)\n"
            "li %0, 0\n"
            "1:\n"
            DASICS_CALL_J_WORD("3f - 1b")
            "2:\n"
            "mv %1, ra\ncsrr %2, 0x8b1\nla %3, 2b\n"
            "j 4f\n"
            "3:\n"
            "addi %0, %0, 1\njalr zero, 0(ra)\n"
            "4:\n"
            "ld ra, 8(sp)\naddi sp, sp, 16\n"
            ".option pop\n"
            : "=&r"(hits), "=&r"(link), "=&r"(return_pc), "=&r"(expected_return_pc)
            :
            : "ra", "memory");
    } else {
        asm volatile (
            ".option push\n.option norvc\n.option norelax\n"
            "addi sp, sp, -16\nsd ra, 8(sp)\n"
            "li %0, 0\nj 3f\n"
            "1:\n"
            "addi %0, %0, 1\njalr zero, 0(ra)\n"
            "3:\n"
            DASICS_CALL_J_WORD("1b - 3b")
            "2:\n"
            "mv %1, ra\ncsrr %2, 0x8b1\nla %3, 2b\n"
            "ld ra, 8(sp)\naddi sp, sp, 16\n"
            ".option pop\n"
            : "=&r"(hits), "=&r"(link), "=&r"(return_pc), "=&r"(expected_return_pc)
            :
            : "ra", "memory");
    }

    int pass = hits == 1 && link == expected_return_pc &&
               return_pc == expected_return_pc;
    (*total)++;
    printf(DASICS_CALL_TAG " case=CALL-J-RETURNPC-%s hits=%lu link=0x%lx return_pc=0x%lx expect=0x%lx result=%s\n",
           negative ? "NEGATIVE" : "POSITIVE", hits, link, return_pc,
           expected_return_pc, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

#if DASICS_LINUX_DUAL_EXEC && DASICS_VALIDATION_PROFILE
#define CALL_ILLEGAL_GPR_SEED 0x1122334455667788
#define CALL_ILLEGAL_RETURNPC_SEED 0x33445000UL
#define CALL_ILLEGAL_FREASON_SEED 0x5UL
#define CALL_ILLEGAL_RESULT_MAGIC 0x4644494e454731UL
#define CALL_STRINGIFY_(value) #value
#define CALL_STRINGIFY(value) CALL_STRINGIFY_(value)

struct call_illegal_pair {
    unsigned long gpr;
    unsigned long hits;
};

struct call_illegal_result {
    unsigned long magic, index, hits, gpr, return_pc, freason;
    unsigned long signal_count, signal_number, signal_code, signal_pc, signal_gpr, site;
    unsigned long umaincfg, ucheck_count;
    unsigned long jump_cfg, jump_lo, jump_hi, continuation_configured;
    unsigned long observer_begin_ok, observer_arm_ok, observer_query_ok, observer_end_ok;
    unsigned long observer_errno;
    /* QUERY is retained before END replaces the client's active snapshot. */
    struct fdi_verify_result observed;
};

extern struct call_illegal_pair call_illegal_invoke_j(void);
extern struct call_illegal_pair call_illegal_invoke_jr(void);
extern char call_illegal_ulib_begin[], call_illegal_ulib_end[];
extern char call_illegal_j_site[], call_illegal_jr_site[];

/* Ordinary entry preserves the seeded ReturnPC. The untrusted probes touch
 * no memory, and their local target can safely fall through to recovery even
 * if a broken decoder executes the forbidden call and overwrites x1. */
asm(
    ".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global call_illegal_invoke_j\n.type call_illegal_invoke_j,@function\n"
    "call_illegal_invoke_j:\n"
    "addi sp,sp,-16\nsd ra,8(sp)\n"
    "la t0,1f\ncsrw 0x8b2,t0\ncall call_illegal_untrusted_j\n"
    "1:\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size call_illegal_invoke_j,.-call_illegal_invoke_j\n"
    ".balign 8\n.global call_illegal_invoke_jr\n.type call_illegal_invoke_jr,@function\n"
    "call_illegal_invoke_jr:\n"
    "addi sp,sp,-16\nsd ra,8(sp)\n"
    "la t0,1f\ncsrw 0x8b2,t0\ncall call_illegal_untrusted_jr\n"
    "1:\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size call_illegal_invoke_jr,.-call_illegal_invoke_jr\n"
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".global call_illegal_ulib_begin\ncall_illegal_ulib_begin:\n"
    ".type call_illegal_untrusted_j,@function\ncall_illegal_untrusted_j:\n"
    "mv t4,ra\nli ra," CALL_STRINGIFY(CALL_ILLEGAL_GPR_SEED) "\nli a1,0\n"
    ".global call_illegal_j_site\ncall_illegal_j_site:\n"
    DASICS_CALL_J_WORD("2f - call_illegal_j_site")
    "j 3f\n2:\nli a1,1\n3:\nmv a0,ra\nmv ra,t4\nret\n"
    ".size call_illegal_untrusted_j,.-call_illegal_untrusted_j\n"
    ".balign 8\n.type call_illegal_untrusted_jr,@function\ncall_illegal_untrusted_jr:\n"
    "mv t4,ra\nli ra," CALL_STRINGIFY(CALL_ILLEGAL_GPR_SEED) "\nli a1,0\nla t3,2f\n"
    ".global call_illegal_jr_site\ncall_illegal_jr_site:\n"
    ".word 0x000e10fb\n"
    "j 3f\n2:\nli a1,1\n3:\nmv a0,ra\nmv ra,t4\nret\n"
    ".size call_illegal_untrusted_jr,.-call_illegal_untrusted_jr\n"
    ".balign 8\n.global call_illegal_ulib_end\ncall_illegal_ulib_end:\n"
    ".section .text,\"ax\",@progbits\n.option pop\n");

/* Only the disabled child installs this handler. Linux handles protected
 * .ulibtext illegal instructions internally, without a userspace signal. */
static volatile sig_atomic_t call_illegal_signals, call_illegal_signal_number, call_illegal_signal_code;
static volatile unsigned long call_illegal_signal_pc, call_illegal_signal_gpr;
static volatile unsigned long call_illegal_expected_site;
/* The descriptor is opened only after fork/exec, never shared with the parent.
 * Fatal child signals make a best-effort END before process-exit cleanup. */
static volatile sig_atomic_t call_illegal_observer_fd = -1;

static void call_illegal_abort(int signo)
{
    int fd = call_illegal_observer_fd;
    if (fd >= 0) {
        struct fdi_verify_result ended = {.version = FDI_VERIFY_VERSION};
        (void)ioctl(fd, FDI_VERIFY_END, &ended);
        close(fd);
        call_illegal_observer_fd = -1;
    }
    _exit(signo == SIGALRM ? 104 : 101);
}

static void call_illegal_sigill(int signo, siginfo_t *info, void *context)
{
    ucontext_t *uc = context;
    if (signo != SIGILL || !info || !uc || info->si_signo != SIGILL ||
        info->si_code != ILL_ILLOPC || call_illegal_signals ||
        (unsigned long)info->si_addr != call_illegal_expected_site ||
        uc->uc_mcontext.__gregs[REG_PC] != call_illegal_expected_site)
        call_illegal_abort(signo);
    call_illegal_signals = 1;
    call_illegal_signal_number = signo;
    call_illegal_signal_code = info->si_code;
    call_illegal_signal_pc = uc->uc_mcontext.__gregs[REG_PC];
    call_illegal_signal_gpr = uc->uc_mcontext.__gregs[REG_RA];
    uc->uc_mcontext.__gregs[REG_PC] += 4;
}

static int call_illegal_observer_valid(const struct call_illegal_result *result, pid_t pid)
{
    const struct fdi_verify_result *record = &result->observed;
    unsigned long site = (unsigned long)((result->index & 1) ?
        call_illegal_jr_site : call_illegal_j_site);
    unsigned long instruction = (result->index & 1) ? 0x000e10fbUL : 0x0080007bUL;
    return result->observer_begin_ok == 1 && result->observer_arm_ok == 1 &&
        result->observer_query_ok == 1 && result->observer_end_ok == 1 &&
        !result->observer_errno && result->site == site &&
        record->version == FDI_VERIFY_VERSION && record->flags == 0 &&
        record->active == 1 && record->cookie && record->pid == (unsigned long)pid &&
        record->kind == FDI_VERIFY_FDICALL && record->site == site &&
        record->instruction == instruction && record->illegal_count == 1 &&
        record->cause == 2 && record->epc == site &&
        record->freason == CALL_ILLEGAL_FREASON_SEED &&
        record->return_pc == CALL_ILLEGAL_RETURNPC_SEED &&
        record->gpr[0] == 0 && record->gpr[1] == CALL_ILLEGAL_GPR_SEED &&
        record->gpr[11] == 0 &&
        (!(result->index & 1) || record->gpr[28] == site + 8);
}

static int call_illegal_observe(unsigned int index, struct call_illegal_result *result)
{
    int disabled = index >= 2;
    int jr = index & 1;
    struct sigaction action = {0}, previous;
    unsigned long old_return_pc = csr_read(0x8b1), old_freason = csr_read(0x8b3);
    unsigned long old_active = csr_read(0x8b2), old_jump_cfg = csr_read(0x8c8);
    unsigned long old_jump_lo = csr_read(0x8c6), old_jump_hi = csr_read(0x8c7);
    unsigned long jump_lo = (unsigned long)call_illegal_ulib_begin & ~7UL;
    unsigned long jump_hi = ((unsigned long)call_illegal_ulib_end + 7UL) & ~7UL;
    unsigned long jump_cfg = (old_jump_cfg & ~(0xffffUL << 48)) | (1UL << 48);
    struct call_illegal_pair pair = {0};
    struct fdi_verify_client observer = {.fd = -1};
    int restored = 1;

    memset(result, 0, sizeof(*result));
    result->index = index;
    result->site = (unsigned long)(jr ? call_illegal_jr_site : call_illegal_j_site);
    result->umaincfg = dasics_linux_query_umaincfg();
    if (result->umaincfg != (disabled ? 0UL : 2UL))
        return 0;
    call_illegal_signals = 0;
    call_illegal_signal_number = 0;
    call_illegal_signal_code = 0;
    call_illegal_signal_pc = call_illegal_signal_gpr = 0;
    call_illegal_expected_site = result->site;
    if (disabled) {
        action.sa_sigaction = call_illegal_sigill;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGILL, &action, &previous))
            return 0;
    }
    /* Slot 3 permits only the probes' local cleanup jump. The return to
     * trusted code is authorized by ActiveZoneReturnPC in the wrapper. */
    csr_write(0x8c6, jump_lo);
    csr_write(0x8c7, jump_hi);
    csr_write(0x8c8, jump_cfg);
    result->jump_cfg = csr_read(0x8c8);
    result->jump_lo = csr_read(0x8c6);
    result->jump_hi = csr_read(0x8c7);
    result->continuation_configured = result->jump_cfg == jump_cfg &&
        result->jump_lo == jump_lo && result->jump_hi == jump_hi;
    csr_write(0x8b1, CALL_ILLEGAL_RETURNPC_SEED);
    csr_write(0x8b3, CALL_ILLEGAL_FREASON_SEED);
    if (result->continuation_configured) {
        if (fdi_verify_begin(&observer, 0)) {
            result->observer_errno = errno;
        } else {
            result->observer_begin_ok = 1;
            call_illegal_observer_fd = observer.fd;
            if (fdi_verify_arm(&observer, FDI_VERIFY_FDICALL, (const void *)result->site)) {
                result->observer_errno = errno;
            } else {
                result->observer_arm_ok = 1;
                pair = jr ? call_illegal_invoke_jr() : call_illegal_invoke_j();
                if (fdi_verify_query(&observer)) {
                    result->observer_errno = errno;
                } else {
                    result->observer_query_ok = 1;
                    result->observed = observer.observed;
                }
            }
        }
    }
    result->hits = pair.hits;
    result->gpr = pair.gpr;
    result->return_pc = csr_read(0x8b1);
    result->freason = csr_read(0x8b3);
    result->signal_count = call_illegal_signals;
    result->signal_number = call_illegal_signal_number;
    result->signal_code = call_illegal_signal_code;
    result->signal_pc = call_illegal_signal_pc;
    result->signal_gpr = call_illegal_signal_gpr;
#ifdef DASICS_N_EXTENSION_PROFILE
    result->ucheck_count = dasics_n_extension_trap_count;
#endif
    /* END is attempted for every opened lease, including ARM/QUERY errors.
     * No descriptor alias is created, and END also closes the sole fd. */
    if (observer.fd >= 0) {
        if (fdi_verify_end(&observer)) {
            if (!result->observer_errno)
                result->observer_errno = errno;
        } else {
            result->observer_end_ok = 1;
        }
        call_illegal_observer_fd = -1;
    }
    csr_write(0x8c8, old_jump_cfg);
    csr_write(0x8c6, old_jump_lo);
    csr_write(0x8c7, old_jump_hi);
    csr_write(0x8b1, old_return_pc);
    csr_write(0x8b2, old_active);
    csr_write(0x8b3, old_freason);
    if (disabled)
        restored = sigaction(SIGILL, &previous, NULL) == 0;
    restored = restored && csr_read(0x8c8) == old_jump_cfg &&
        csr_read(0x8c6) == old_jump_lo && csr_read(0x8c7) == old_jump_hi &&
        csr_read(0x8b1) == old_return_pc && csr_read(0x8b2) == old_active &&
        csr_read(0x8b3) == old_freason;
    result->magic = CALL_ILLEGAL_RESULT_MAGIC;
    return restored && result->continuation_configured &&
        call_illegal_observer_valid(result, getpid()) &&
        !result->hits && result->gpr == CALL_ILLEGAL_GPR_SEED &&
        result->return_pc == CALL_ILLEGAL_RETURNPC_SEED &&
        result->freason == CALL_ILLEGAL_FREASON_SEED && !result->ucheck_count &&
        (disabled ? result->signal_count == 1 && result->signal_number == SIGILL &&
         result->signal_code == ILL_ILLOPC &&
         result->signal_pc == result->site && result->signal_gpr == CALL_ILLEGAL_GPR_SEED :
         result->signal_count == 0);
}

static void call_illegal_child(unsigned int index, int fd)
{
    struct call_illegal_result result;
    int pass;
    ssize_t written;
    struct sigaction timeout = {.sa_handler = call_illegal_abort};
    /* A process-bound cause2 observer must begin/arm/query/end in this child.
     * Forked children cannot borrow a parent's lease or exception record. */
    sigemptyset(&timeout.sa_mask);
    if (sigaction(SIGALRM, &timeout, NULL))
        _exit(104);
    alarm(10);
    pass = call_illegal_observe(index, &result);
    do {
        written = write(fd, &result, sizeof(result));
    } while (written < 0 && errno == EINTR);
    close(fd);
    /* Forked and re-executed children never emit an N runtime destructor. */
    _exit(pass && written == (ssize_t)sizeof(result) ? 0 : 1);
}

static int run_illegal_call_case(unsigned int index, unsigned long *total)
{
    static const char *names[] = {"UNTRUSTED-J", "UNTRUSTED-JR", "DISABLED-J", "DISABLED-JR"};
    static const char *checks[] = {"TARGET", "GPR", "RETURNPC", "FREASON"};
    const unsigned long expected[] = {0, CALL_ILLEGAL_GPR_SEED,
        CALL_ILLEGAL_RETURNPC_SEED, CALL_ILLEGAL_FREASON_SEED};
    struct call_illegal_result result = {0};
    int channel[2], status = -1, child_ok = 0, failures = 0;
    pid_t pid = -1, waited;
    size_t received = 0;
    if (!pipe(channel)) {
        pid = fork();
        if (pid == 0) {
            close(channel[0]);
            if (index >= 2) {
                char descriptor[24];
                char *args[] = {"dasics-test-fdicall-unified",
                    index == 2 ? "--internal-disabled-j" : "--internal-disabled-jr", descriptor, NULL};
                snprintf(descriptor, sizeof(descriptor), "%d", channel[1]);
                alarm(10);
                /* Exec without -dasics resets UENA and exec metadata independently. */
                execv("/proc/self/exe", args);
                _exit(102);
            }
            call_illegal_child(index, channel[1]);
        }
        close(channel[1]);
        if (pid > 0) {
            while (received < sizeof(result)) {
                ssize_t count = read(channel[0], (char *)&result + received, sizeof(result) - received);
                if (count < 0 && errno == EINTR)
                    continue;
                if (count <= 0)
                    break;
                received += count;
            }
            do {
                waited = waitpid(pid, &status, 0);
            } while (waited < 0 && errno == EINTR);
            child_ok = waited == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                received == sizeof(result) && result.magic == CALL_ILLEGAL_RESULT_MAGIC &&
                result.index == index && call_illegal_observer_valid(&result, pid);
        }
        close(channel[0]);
    }
    int record_complete = received == sizeof(result) &&
        result.magic == CALL_ILLEGAL_RESULT_MAGIC && result.index == index;
    int observer_ok = record_complete && call_illegal_observer_valid(&result, pid);
    printf("FDICALL_OBSERVER_STATUS version=1 case=%s pid=%ld begin=%lu arm=%lu "
           "query=%lu end=%lu error=%lu result=%s\n",
           names[index], (long)pid, result.observer_begin_ok, result.observer_arm_ok,
           result.observer_query_ok, result.observer_end_ok, result.observer_errno,
           observer_ok ? "PASS" : "FAIL");
    if (record_complete && result.observer_query_ok)
        fdi_verify_print(names[index], &result.observed);
    unsigned long actual[] = {result.hits, result.gpr, result.return_pc, result.freason};
    for (unsigned int i = 0; i < 4; i++) {
        int pass = child_ok && actual[i] == expected[i];
        (*total)++;
        failures += !pass;
        printf(DASICS_CALL_TAG " case=CALL-ILLEGAL-%s-%s value=0x%lx expect=0x%lx child_ok=%d result=%s\n",
               names[index], checks[i], actual[i], expected[i], child_ok, pass ? "PASS" : "FAIL");
    }
    printf("DASICS_CALL_ILLEGAL_OBSERVATION version=1 case=%s pid=%ld wait_status=%d "
           "signal_count=%lu signal=%lu si_code=%lu signal_pc=0x%lx signal_gpr=0x%lx site=0x%lx "
           "umaincfg=0x%lx ucheck_count=%lu jump_cfg=0x%lx jump_lo=0x%lx jump_hi=0x%lx "
           "continuation_configured=%lu cause2=%s result=%s\n",
           names[index], (long)pid, status, result.signal_count, result.signal_number, result.signal_code,
           result.signal_pc, result.signal_gpr, result.site, result.umaincfg, result.ucheck_count,
           result.jump_cfg, result.jump_lo, result.jump_hi, result.continuation_configured,
           observer_ok ? "OBSERVED" : "NOT_OBSERVED",
           child_ok ? "PASS" : "FAIL");
    return failures;
}
#endif



static volatile unsigned long fdicall_fault_destination_after;
#define DASICS_RETURNPC_TRAP_TAG "\033[1;34m[DASICS-FDICALL-UNIFIED]\033[0m"
#define DASICS_RETURNPC_TRAP_BEGIN_TAG "\033[1;31m[DASICS-FDICALL-UNIFIED]"
#define DASICS_RETURNPC_TRAP_SUMMARY_TAG "\033[1;32m[DASICS-FDICALL-UNIFIED]"
#define DASICS_RETURNPC_TRAP_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define DASICS_FREASON_LOAD 2UL
#define RETURNPC_TRAP_FAULT_MARKER 0x5250545241464c54UL
#define RETURNPC_TRAP_SECOND_MARKER 0x525054525345434eUL

static volatile unsigned long returnpc_trap_denied_data __attribute__((aligned(8))) = 0x1020304050607080UL;

extern char returnpc_trap_fault_entry[];
extern char returnpc_trap_second_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global returnpc_trap_fault_entry\n"
".type returnpc_trap_fault_entry, @function\n"
"returnpc_trap_fault_entry:\n"
"  li a0, " XSTR(RETURNPC_TRAP_FAULT_MARKER) "\n"
"  la t0, returnpc_trap_denied_data\n"
"  li t1, 0x6c6f616473656564\n"
".global fdicall_load_fault_site\n"
"fdicall_load_fault_site:\n"
"  ld t1, 0(t0)\n"
".global fdicall_load_fault_recovery\n"
"fdicall_load_fault_recovery:\n"
"  mv a1, t1\n"
"  ret\n"
".balign 8\n"
".global returnpc_trap_second_entry\n"
".type returnpc_trap_second_entry, @function\n"
"returnpc_trap_second_entry:\n"
"  li a0, " XSTR(RETURNPC_TRAP_SECOND_MARKER) "\n"
"  ret\n"
".section .text,\"ax\",@progbits\n"
".option pop\n"
);

static void clear_load_permission_state(void)
{
    csr_write(0x880, 0);
    csr_write(0x890, 0);
    csr_write(0x891, 0);
    csr_write(0x8b3, 0);
}

static unsigned long read_return_pc(void)
{
    return csr_read(0x8b1);
}

static unsigned long read_freason(void)
{
    return csr_read(0x8b3);
}

static __attribute__((noinline)) unsigned long invoke_returnpc_trap_fault(unsigned long *expected_return_pc)
{
    unsigned long result;
    unsigned long expected;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, returnpc_trap_fault_entry\n"
        ".word 0x000510fb\n"
        ".global fdicall_fault_call_return\n"
        "fdicall_fault_call_return:\n"
        "1:\n"
        "la t0,fdicall_fault_destination_after\n"
        "sd a1,0(t0)\n"
        "mv %0, a0\n"
        "la %1, 1b\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        : "=r"(result), "=r"(expected)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    *expected_return_pc = expected;
    return result;
}

static __attribute__((noinline)) unsigned long invoke_returnpc_trap_second(unsigned long *expected_return_pc)
{
    unsigned long result;
    unsigned long expected;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, returnpc_trap_second_entry\n"
        ".word 0x000510fb\n"
        ".global fdicall_clean_call_return\n"
        "fdicall_clean_call_return:\n"
        "1:\n"
        "mv %0, a0\n"
        "la %1, 1b\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        : "=r"(result), "=r"(expected)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    *expected_return_pc = expected;
    return result;
}

/* Every inherited value assertion also requires the actual user trap context. */
static int fdicall_context_ok;
static int record_value_case(const char *case_name, unsigned long value,
                             unsigned long expect, unsigned long *total)
{
    int pass = value == expect && fdicall_context_ok;

    (*total)++;
    printf(DASICS_RETURNPC_TRAP_TAG " case=%s value=0x%lx expect=0x%lx context_ok=%d result=%s\n",
           case_name, value, expect, fdicall_context_ok, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}


#if defined(DASICS_N_EXTENSION_PROFILE) && DASICS_LINUX_DUAL_EXEC && DASICS_VALIDATION_PROFILE
#define FDICALL_TOTAL 35UL
#define FDICALL_RA_SEED 0x1122334455667788UL
#define FDICALL_CANARY_SEED 0x2233445566778899UL
#define FDICALL_LOAD_SEED 0x6c6f616473656564UL

struct fdicall_legal_result {
    unsigned long hits, link, return_pc, freason, target, ra, canary, source;
};

typedef void (*fdicall_legal_fn)(struct fdicall_legal_result *);
extern void fdicall_jr_ra_odd(struct fdicall_legal_result *);
extern void fdicall_jr_t2_even(struct fdicall_legal_result *);
extern void fdicall_jr_t2_odd(struct fdicall_legal_result *);
extern void fdicall_jr_x0_even(struct fdicall_legal_result *);
extern void fdicall_jr_x0_odd(struct fdicall_legal_result *);
extern void fdicall_j_max(struct fdicall_legal_result *);
extern void fdicall_j_min(struct fdicall_legal_result *);
extern char fdicall_jr_target[];
extern char fdicall_j_max_site[], fdicall_j_max_return[], fdicall_j_max_target[];
extern char fdicall_j_min_site[], fdicall_j_min_return[], fdicall_j_min_target[];
extern char fdicall_jr_ra_odd_site[], fdicall_jr_ra_odd_return[];
extern char fdicall_jr_t2_even_site[], fdicall_jr_t2_even_return[];
extern char fdicall_jr_t2_odd_site[], fdicall_jr_t2_odd_return[];
extern char fdicall_jr_x0_even_site[], fdicall_jr_x0_even_return[];
extern char fdicall_jr_x0_odd_site[], fdicall_jr_x0_odd_return[];
extern char fdicall_load_fault_site[], fdicall_load_fault_recovery[];
extern char fdicall_fault_call_return[], fdicall_clean_call_return[];

/* All legal targets below are trusted .text. These checks concern instruction
 * encoding and return state; they do not claim trusted/untrusted transitions.
 * t5 holds a safe continuation independent of the GPR/CSR being checked. */
#define LEGAL_START(name, target, odd) \
    ".section .text,\"ax\",@progbits\n.balign 8\n" \
    ".global " #name "\n.type " #name ",@function\n" #name ":\n" \
    "addi sp,sp,-16\nsd ra,8(sp)\nmv t6,a0\nli a1,0\n" \
    "li ra,0x1122334455667788\nli t2,0x2233445566778899\nli t4,0\n" \
    "la t5," #name "_return\nla t3," #target "\nori t3,t3," #odd "\n"
#define LEGAL_FINISH(name, link) \
    ".global " #name "_return\n" #name "_return:\n" \
    "sd a1,0(t6)\nsd " #link ",8(t6)\ncsrr t0,0x8b1\nsd t0,16(t6)\n" \
    "csrr t0,0x8b3\nsd t0,24(t6)\nsd t4,32(t6)\nsd ra,40(t6)\n" \
    "sd t2,48(t6)\nsd t3,56(t6)\nld ra,8(sp)\naddi sp,sp,16\nret\n" \
    ".size " #name ",.-" #name "\n"
#define LEGAL_JR(name, word, target_odd, link) \
    LEGAL_START(name, fdicall_jr_target, target_odd) \
    ".global " #name "_site\n" #name "_site:\n.word " #word "\n" \
    LEGAL_FINISH(name, link)

asm(
    ".option push\n.option norvc\n.option norelax\n"
    LEGAL_JR(fdicall_jr_ra_odd, 0x000e10fb, 1, ra)
    LEGAL_JR(fdicall_jr_t2_even, 0x000e13fb, 0, t2)
    LEGAL_JR(fdicall_jr_t2_odd, 0x000e13fb, 1, t2)
    LEGAL_JR(fdicall_jr_x0_even, 0x000e107b, 0, zero)
    LEGAL_JR(fdicall_jr_x0_odd, 0x000e107b, 1, zero)
    ".balign 8\n.global fdicall_jr_target\n.type fdicall_jr_target,@function\n"
    "fdicall_jr_target:\nauipc t4,0\naddi a1,a1,1\njalr zero,0(t5)\n"
    ".size fdicall_jr_target,.-fdicall_jr_target\n"

    LEGAL_START(fdicall_j_max, fdicall_j_max_target, 0)
    ".balign 8\n.global fdicall_j_max_site\nfdicall_j_max_site:\n"
    DASICS_CALL_J_WORD("fdicall_j_max_target - fdicall_j_max_site")
    LEGAL_FINISH(fdicall_j_max, ra)
    ".org fdicall_j_max_site+0x100\n"
    ".global fdicall_j_min_target\n.type fdicall_j_min_target,@function\n"
    "fdicall_j_min_target:\nauipc t4,0\naddi a1,a1,1\njalr zero,0(t5)\n"
    ".size fdicall_j_min_target,.-fdicall_j_min_target\n"
    /* IALIGN=16 in the fixed C-capable core permits these 32-bit words at
     * the maximum positive target's two-byte-aligned address. */
    ".org fdicall_j_max_site+0x3ffffe\n"
    ".global fdicall_j_max_target\n.type fdicall_j_max_target,@function\n"
    "fdicall_j_max_target:\nauipc t4,0\naddi a1,a1,1\njalr zero,0(t5)\n"
    ".size fdicall_j_max_target,.-fdicall_j_max_target\n"
    LEGAL_START(fdicall_j_min, fdicall_j_min_target, 0)
    /* Preserve the seeded registers while skipping zero-filled layout
     * padding. The negative-boundary site stays exactly 4 MiB after target. */
    ".global fdicall_j_min_dispatch\nfdicall_j_min_dispatch:\nj fdicall_j_min_site\n"
    ".org fdicall_j_min_target+0x400000\n"
    ".global fdicall_j_min_site\nfdicall_j_min_site:\n"
    DASICS_CALL_J_WORD("fdicall_j_min_target - fdicall_j_min_site")
    LEGAL_FINISH(fdicall_j_min, ra)
    ".option pop\n"
);

struct fdicall_legal_case {
    const char *name;
    fdicall_legal_fn invoke;
    char *site, *ret, *target;
    unsigned int rd, odd;
    long offset;
    uint32_t encoding;
};

static uint32_t instruction_word(const char *site)
{
    const volatile unsigned char *bytes = (const volatile unsigned char *)site;
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static long call_immediate(uint32_t instruction, int relative)
{
    /* Mirror the checked instruction field layout, not the target result:
     * comparing both against the labels detects truncation/sign mistakes. */
    unsigned long bits = relative ?
        ((unsigned long)(instruction >> 31) << 22) |
        (((instruction >> 7) & 1UL) << 21) |
        (((instruction >> 15) & 0x3fUL) << 15) |
        (((instruction >> 8) & 0xfUL) << 11) |
        (((instruction >> 21) & 0x3ffUL) << 1) : instruction >> 20;
    unsigned int width = relative ? 23 : 12;
    return bits & (1UL << (width - 1)) ? (long)bits - (1L << width) : (long)bits;
}

static int run_legal_extensions(unsigned long *total)
{
    static const struct fdicall_legal_case cases[] = {
        {"JR-RA-ODD", fdicall_jr_ra_odd, fdicall_jr_ra_odd_site,
         fdicall_jr_ra_odd_return, fdicall_jr_target, 1, 1, 0, 0x000e10fb},
        {"JR-T2-EVEN", fdicall_jr_t2_even, fdicall_jr_t2_even_site,
         fdicall_jr_t2_even_return, fdicall_jr_target, 7, 0, 0, 0x000e13fb},
        {"JR-T2-ODD", fdicall_jr_t2_odd, fdicall_jr_t2_odd_site,
         fdicall_jr_t2_odd_return, fdicall_jr_target, 7, 1, 0, 0x000e13fb},
        {"JR-X0-EVEN", fdicall_jr_x0_even, fdicall_jr_x0_even_site,
         fdicall_jr_x0_even_return, fdicall_jr_target, 0, 0, 0, 0x000e107b},
        {"JR-X0-ODD", fdicall_jr_x0_odd, fdicall_jr_x0_odd_site,
         fdicall_jr_x0_odd_return, fdicall_jr_target, 0, 1, 0, 0x000e107b},
        {"J-MAX-POSITIVE", fdicall_j_max, fdicall_j_max_site,
         fdicall_j_max_return, fdicall_j_max_target, 1, 0, 0x3ffffe, 0x7fff8ffb},
        {"J-MIN-NEGATIVE", fdicall_j_min, fdicall_j_min_site,
         fdicall_j_min_return, fdicall_j_min_target, 1, 0, -0x400000, 0x8000007b},
    };
    int failures = 0;
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const struct fdicall_legal_case *test = &cases[i];
        struct fdicall_legal_result observed = {0};
        unsigned long ret = (unsigned long)test->ret;
        unsigned long target = (unsigned long)test->target;
        unsigned long link_expected = test->rd ? ret : 0;
        unsigned long ra_expected = test->rd == 1 ? ret : FDICALL_RA_SEED;
        unsigned long canary_expected = test->rd == 7 ? ret : FDICALL_CANARY_SEED;
        unsigned long before = dasics_n_extension_trap_count;
        csr_write(0x8b3, 5);
        alarm(10);
        test->invoke(&observed);
        alarm(0);
        long offset = (long)target - (long)(unsigned long)test->site;
        uint32_t instruction = instruction_word(test->site);
        long immediate = call_immediate(instruction, test->offset != 0);
        int pass = observed.hits == 1 && observed.link == link_expected &&
            observed.return_pc == ret && ret == (unsigned long)test->site + 4 &&
            observed.freason == 5 && observed.target == target &&
            observed.ra == ra_expected && observed.canary == canary_expected &&
            observed.source == (target | test->odd) &&
            dasics_n_extension_trap_count == before &&
            instruction == test->encoding && immediate == test->offset &&
            (!test->offset || offset == test->offset);
        (*total)++;
        failures += !pass;
        printf(DASICS_CALL_TAG " case=%s value=0x%lx expect=0x1 target_class=trusted "
               "hits=%lu hits_expected=1 site=0x%lx target=0x%lx target_expected=0x%lx "
               "requested_target=0x%lx requested_target_expected=0x%lx rd=%u "
               "link=0x%lx link_expected=0x%lx returnpc=0x%lx returnpc_expected=0x%lx "
               "ra=0x%lx ra_expected=0x%lx canary=0x%lx canary_expected=0x%lx "
               "freason=0x%lx freason_expected=0x5 pc_distance=%ld "
               "instruction=0x%08x instruction_expected=0x%08x "
               "immediate=%ld immediate_expected=%ld "
               "u24=%lu u24_expected=%lu result=%s\n",
               test->name, (unsigned long)pass, observed.hits, (unsigned long)test->site,
               observed.target, target, observed.source, target | test->odd, test->rd,
               observed.link, link_expected, observed.return_pc, ret, observed.ra, ra_expected,
               observed.canary, canary_expected, observed.freason, offset,
               (unsigned int)instruction, (unsigned int)test->encoding, immediate, test->offset,
               dasics_n_extension_trap_count, before, pass ? "PASS" : "FAIL");
    }
    return failures;
}

static int run_preservation(unsigned long *total)
{
    unsigned long before = dasics_n_extension_trap_count, expected = 0;
    unsigned long memory_before = returnpc_trap_denied_data;
    clear_load_permission_state();
    csr_write(CSR_USCRATCH, (unsigned long)fdicall_load_fault_recovery);
    unsigned long result = invoke_returnpc_trap_fault(&expected);
    unsigned long after = dasics_n_extension_trap_count;
    unsigned long return_pc = read_return_pc(), reason = read_freason();
    unsigned long cause = 0, epc = 0, tval = 0, recovery = 0, fault_gpr = 0;
    fdicall_context_ok = 0;
    if (before < DASICS_N_EXTENSION_TRAP_RECORDS && after == before + 1) {
        const volatile dasics_n_extension_trap_record_t *record =
            &dasics_n_extension_trap_records[before];
        cause = record->ucause;
        epc = record->uepc;
        tval = record->utval;
        recovery = record->recovery;
        fault_gpr = record->gpr[6];
        fdicall_context_ok = cause == 24 && epc == (unsigned long)fdicall_load_fault_site &&
            tval == (unsigned long)&returnpc_trap_denied_data &&
            recovery == (unsigned long)fdicall_load_fault_recovery &&
            record->uscratch == recovery && record->dfreason == 2 &&
            record->gpr[1] == (unsigned long)fdicall_fault_call_return &&
            record->gpr[5] == tval && fault_gpr == FDICALL_LOAD_SEED &&
            (record->ustatus & 0x11UL) == 0x10UL &&
            fdicall_fault_destination_after == FDICALL_LOAD_SEED &&
            returnpc_trap_denied_data == memory_before && csr_read(CSR_USCRATCH) == 0 &&
            expected == (unsigned long)fdicall_fault_call_return;
    }
    printf("FDICALL_U24_OBSERVATION version=1 phase=load-fault "
           "cause=0x%lx cause_expected=0x18 epc=0x%lx epc_expected=0x%lx "
           "tval=0x%lx tval_expected=0x%lx recovery=0x%lx recovery_expected=0x%lx "
           "gpr=0x%lx gpr_expected=0x%lx gpr_after=0x%lx gpr_after_expected=0x%lx "
           "memory=0x%lx memory_expected=0x%lx u24=%lu u24_expected=%lu result=%s\n",
           cause, epc, (unsigned long)fdicall_load_fault_site,
           tval, (unsigned long)&returnpc_trap_denied_data,
           recovery, (unsigned long)fdicall_load_fault_recovery, fault_gpr, FDICALL_LOAD_SEED,
           fdicall_fault_destination_after, FDICALL_LOAD_SEED,
           returnpc_trap_denied_data, memory_before, after, before + 1,
           fdicall_context_ok ? "PASS" : "FAIL");
    int failures = 0;
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-LOAD-FAULT-MARKER",
                                 result, RETURNPC_TRAP_FAULT_MARKER, total);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-FREASON-LOAD", reason, 2, total);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-AFTER-FAULT", return_pc, expected, total);
    result = invoke_returnpc_trap_second(&expected);
    fdicall_context_ok = fdicall_context_ok && expected == (unsigned long)fdicall_clean_call_return &&
        dasics_n_extension_trap_count == after && returnpc_trap_denied_data == memory_before &&
        csr_read(CSR_USCRATCH) == 0;
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-SECOND-CALL",
                                 result, RETURNPC_TRAP_SECOND_MARKER, total);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-AFTER-SECOND-CALL",
                                 read_return_pc(), expected, total);
    failures += record_value_case("CONTEXT-TRAP-RETURNPC-PRESERVE-SECOND-FREASON",
                                 read_freason(), 2, total);
    return failures;
}

struct fdicall_owned_state {
    unsigned long cfg, lo0, hi0, return_pc, active, freason;
    unsigned long jump_cfg, jump_lo3, jump_hi3, uscratch;
};

static struct fdicall_owned_state read_owned_state(void)
{
    struct fdicall_owned_state state = {
        csr_read(0x880), csr_read(0x890), csr_read(0x891), csr_read(0x8b1),
        csr_read(0x8b2), csr_read(0x8b3), csr_read(0x8c8), csr_read(0x8c6),
        csr_read(0x8c7), csr_read(CSR_USCRATCH)
    };
    return state;
}

static int restore_owned_state(const struct fdicall_owned_state *saved)
{
    csr_write(0x880, 0);
    csr_write(0x8c8, 0);
    csr_write(0x890, saved->lo0); csr_write(0x891, saved->hi0);
    csr_write(0x8c6, saved->jump_lo3); csr_write(0x8c7, saved->jump_hi3);
    csr_write(0x8b1, saved->return_pc); csr_write(0x8b2, saved->active);
    csr_write(0x8b3, saved->freason); csr_write(CSR_USCRATCH, saved->uscratch);
    csr_write(0x8c8, saved->jump_cfg); csr_write(0x880, saved->cfg);
    struct fdicall_owned_state actual = read_owned_state();
    return memcmp(&actual, saved, sizeof(actual)) == 0;
}

int main(int argc, char *argv[])
{
    unsigned long total = 0;
    int failures = 0;
    if (argc == 3 && (!strcmp(argv[1], "--internal-disabled-j") ||
                      !strcmp(argv[1], "--internal-disabled-jr"))) {
        char *end;
        errno = 0;
        long fd = strtol(argv[2], &end, 10);
        if (errno || *end || fd < 3 || fd > INT_MAX)
            _exit(103);
        call_illegal_child(!strcmp(argv[1], "--internal-disabled-j") ? 2 : 3, (int)fd);
    }
    if (argc != 1) {
        fprintf(stderr, DASICS_CALL_TAG " argument-contract result=FAIL\n");
        return 2;
    }
    struct fdicall_owned_state saved = read_owned_state();
    unsigned long cfg = dasics_linux_query_umaincfg();
    int profile_ok = cfg == 2 && dasics_n_extension_trap_count == 0;
    printf(DASICS_CALL_TAG " begin version=1 expected_total=%lu\n", FDICALL_TOTAL);
    total++;
    failures += !profile_ok;
    printf(DASICS_CALL_TAG " case=PROFILE umaincfg=0x%lx umaincfg_expected=0x2 u24=%lu u24_expected=0 result=%s\n",
           cfg, dasics_n_extension_trap_count, profile_ok ? "PASS" : "FAIL");
    if (profile_ok) {
        csr_write(0x8b3, 5);
        failures += run_trusted_call_jr_returnpc_check(&total);
        failures += run_trusted_call_j_returnpc_check(&total, 0);
        failures += run_trusted_call_j_returnpc_check(&total, 1);
        failures += run_legal_extensions(&total);
        /* Parent N records are still empty when forked negative probes begin. */
        for (unsigned int index = 0; index < 4; index++)
            failures += run_illegal_call_case(index, &total);
        failures += run_preservation(&total);
        int restored = restore_owned_state(&saved);
        total++;
        failures += !restored;
        printf(DASICS_CALL_TAG " case=STATE-RESTORE value=0x%lx expect=0x1 result=%s\n",
               (unsigned long)restored, restored ? "PASS" : "FAIL");
        unsigned long traps = dasics_n_extension_trap_count, status = csr_read(CSR_USTATUS);
        int runtime_ok = traps == 1 && status == 0x11;
        total++;
        failures += !runtime_ok;
        printf(DASICS_CALL_TAG " case=N-RUNTIME-GUARD u24=%lu u24_expected=1 ustatus=0x%lx ustatus_expected=0x11 result=%s\n",
               traps, status, runtime_ok ? "PASS" : "FAIL");
    }
    if (total != FDICALL_TOTAL)
        failures++;
    printf(DASICS_CALL_TAG " scope=software-call-and-side-effects j_boundary_target_class=trusted "
           "cause2_evidence=PROCESS_BOUND_OBSERVER observer_lease=CHILD_LOCAL\n");
    printf(DASICS_CALL_TAG " summary total=%lu failed=%d result=%s\n",
           total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#else
int main(void)
{
    fprintf(stderr, DASICS_CALL_TAG " requires Linux N-profile and validation observer result=FAIL\n");
    return 2;
}
#endif
