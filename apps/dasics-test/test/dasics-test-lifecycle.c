#define _GNU_SOURCE
#include <stdio.h>

#ifndef DASICS_LINUX_DUAL_EXEC
#define DASICS_LINUX_DUAL_EXEC 0
#endif

#if !DASICS_LINUX_DUAL_EXEC || !defined(DASICS_N_EXTENSION_PROFILE)
int main(void)
{
    fprintf(stderr, "[DASICS-LIFECYCLE] Linux N profile required result=FAIL\n");
    return 2;
}
#else
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ucsr.h"
#include "udasics.h"

#define LIFECYCLE_TOTAL 11UL
#define LIFECYCLE_MAGIC 0x4c4946454359434cUL
#define LIB_HIGH_MASK (0xfUL << 60)
#define JUMP_HIGH_MASK (1UL << 48)

/* Observation must precede the library constructor's N CSR writes. Neither
 * child deliberately triggers a fault, so no user recovery handler is needed. */
int dasics_n_extension_auto_init(void) { return 0; }

struct lifecycle_state {
    unsigned long lib_cfg, lib_lo, lib_hi, jump_cfg, jump_lo, jump_hi;
    unsigned long maincall, return_pc, active_return_pc, freason;
    unsigned long n[9];
    unsigned long umain_cfg;
};

struct lifecycle_report {
    unsigned long magic;
    pid_t pid;
    unsigned long role, traps;
    struct lifecycle_state state;
};

static void read_csrs(struct lifecycle_state *state)
{
    state->lib_cfg = csr_read(0x880);
    state->lib_lo = csr_read(0x8ae);
    state->lib_hi = csr_read(0x8af);
    state->jump_cfg = csr_read(0x8c8);
    state->jump_lo = csr_read(0x8c6);
    state->jump_hi = csr_read(0x8c7);
    state->maincall = csr_read(0x8b0);
    state->return_pc = csr_read(0x8b1);
    state->active_return_pc = csr_read(0x8b2);
    state->freason = csr_read(0x8b3);
    state->n[0] = csr_read(CSR_USTATUS);
    state->n[1] = csr_read(CSR_UIE);
    state->n[2] = csr_read(CSR_UTVEC);
    state->n[3] = csr_read(CSR_USCRATCH);
    state->n[4] = csr_read(CSR_UEPC);
    state->n[5] = csr_read(CSR_UCAUSE);
    state->n[6] = csr_read(CSR_UTVAL);
    state->n[7] = csr_read(CSR_UIP);
    state->n[8] = csr_read(CSR_UTIMER);
}

static void read_state(struct lifecycle_state *state)
{
    read_csrs(state);
    state->umain_cfg = dasics_linux_query_umaincfg();
}

static int n_clear(const struct lifecycle_state *state)
{
    unsigned long combined = 0;
    for (unsigned int i = 0; i < 9; i++)
        combined |= state->n[i];
    return combined == 0;
}

static int fdi_clear(const struct lifecycle_state *state)
{
    /* Exec may populate low data/jump slots from ELF sections. Only these
     * unused high slots and return/call/reason registers must be zero. */
    return !state->lib_lo && !state->lib_hi && !state->jump_lo && !state->jump_hi &&
        !(state->lib_cfg & LIB_HIGH_MASK) && !(state->jump_cfg & JUMP_HIGH_MASK) &&
        !state->maincall && !state->return_pc && !state->active_return_pc && !state->freason;
}

static int same_fdi(const struct lifecycle_state *left, const struct lifecycle_state *right)
{
    return left->lib_cfg == right->lib_cfg && left->lib_lo == right->lib_lo &&
        left->lib_hi == right->lib_hi && left->jump_cfg == right->jump_cfg &&
        left->jump_lo == right->jump_lo && left->jump_hi == right->jump_hi &&
        left->maincall == right->maincall && left->return_pc == right->return_pc &&
        left->active_return_pc == right->active_return_pc && left->freason == right->freason &&
        left->umain_cfg == right->umain_cfg;
}

static int transfer(int fd, void *buffer, size_t size, int writing)
{
    size_t done = 0;
    while (done < size) {
        ssize_t count = writing ? write(fd, (char *)buffer + done, size - done) :
                                  read(fd, (char *)buffer + done, size - done);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return 0;
        done += (size_t)count;
    }
    return 1;
}

static int waited(pid_t pid, int *status)
{
    pid_t result;
    do {
        result = waitpid(pid, status, 0);
    } while (result < 0 && errno == EINTR);
    return result == pid && WIFEXITED(*status) && WEXITSTATUS(*status) == 0;
}

static void producer_expected(struct lifecycle_state *state,
                              const struct lifecycle_state *baseline)
{
    *state = *baseline;
    state->lib_cfg = (baseline->lib_cfg & ~LIB_HIGH_MASK) | (0xbUL << 60);
    state->lib_lo = 0x12345000;
    state->lib_hi = 0x12346000;
    state->jump_cfg = baseline->jump_cfg | JUMP_HIGH_MASK;
    state->jump_lo = 0x22345000;
    state->jump_hi = 0x22346000;
    state->maincall = 0x33445000;
    state->return_pc = 0x43445006;
    state->active_return_pc = 0x53445006;
    state->freason = 5;
    state->n[0] = 0x10;
    state->n[1] = 0x10;
    state->n[2] = 0x12348000;
    state->n[3] = 0x12349000;
    state->n[4] = 0x1234a000;
    state->n[5] = 24;
    state->n[6] = 0x1234b000;
    state->n[7] = 0x10;
    state->n[8] = 1;
}

static void producer(int fd, const struct lifecycle_state *expected)
{
    struct lifecycle_report report = { .magic = LIFECYCLE_MAGIC, .role = 1 };
    struct lifecycle_state final;
    alarm(30);
    report.pid = getpid();
    /* Global UIE stays disabled while the timer becomes pending. UTVEC is
     * deliberately a sentinel, never a live handler address. */
    csr_write(CSR_USTATUS, 0);
    csr_write(CSR_UTIMER, 0);
    csr_write(CSR_UIE, expected->n[1]);
    csr_write(CSR_UTVEC, expected->n[2]);
    csr_write(CSR_USCRATCH, expected->n[3]);
    csr_write(CSR_UEPC, expected->n[4]);
    csr_write(CSR_UCAUSE, expected->n[5]);
    csr_write(CSR_UTVAL, expected->n[6]);
    csr_write(CSR_UIP, 0);
    csr_write(CSR_USTATUS, expected->n[0]);
    csr_write(0x8ae, expected->lib_lo);
    csr_write(0x8af, expected->lib_hi);
    csr_write(0x8c6, expected->jump_lo);
    csr_write(0x8c7, expected->jump_hi);
    csr_write(0x8b0, expected->maincall);
    csr_write(0x8b1, expected->return_pc);
    csr_write(0x8b2, expected->active_return_pc);
    csr_write(0x8b3, expected->freason);
    csr_write(0x880, expected->lib_cfg);
    csr_write(0x8c8, expected->jump_cfg);
    csr_write(CSR_UTIMER, expected->n[8]);
    for (unsigned long i = 0; i < 10000 && !(csr_read(CSR_UIP) & 0x10); i++)
        asm volatile("nop" ::: "memory");
    read_state(&report.state);
    report.traps = dasics_n_extension_trap_count;
    int sent = transfer(fd, &report, sizeof(report), 1);
    close(fd);
    /* Check again after the reporting syscalls; exit while the sentinels are
     * still visible. _exit bypasses any userspace destructor cleanup. */
    final.umain_cfg = dasics_linux_query_umaincfg();
    read_csrs(&final);
    int pass = sent && same_fdi(&report.state, expected) && same_fdi(&final, expected) &&
        !memcmp(report.state.n, expected->n, sizeof(final.n)) &&
        !memcmp(final.n, expected->n, sizeof(final.n)) &&
        !report.traps && !dasics_n_extension_trap_count;
    _exit(pass ? 0 : 1);
}

static int parse_hex(const char *text, unsigned long *value)
{
    char *end;
    if (!text || !*text || *text == '-')
        return 0;
    errno = 0;
    *value = strtoul(text, &end, 16);
    return !errno && !*end;
}

static int consumer(int argc, char *argv[])
{
    struct lifecycle_report report = { .magic = LIFECYCLE_MAGIC, .role = 2 };
    /* This is the first consumer observation, before any application CSR write
     * or runtime initialization. The exec path retains UENA and ELF defaults. */
    read_state(&report.state);
    report.traps = dasics_n_extension_trap_count;
    report.pid = getpid();
    unsigned long fd, lib_cfg, jump_cfg;
    if (argc != 5 || !parse_hex(argv[2], &fd) || fd < 3 || fd > INT_MAX ||
        !parse_hex(argv[3], &lib_cfg) || !parse_hex(argv[4], &jump_cfg))
        _exit(2);
    int pass = fdi_clear(&report.state) && n_clear(&report.state) &&
        report.state.umain_cfg == 2 && report.state.lib_cfg == lib_cfg &&
        report.state.jump_cfg == jump_cfg && !report.traps;
    int sent = transfer((int)fd, &report, sizeof(report), 1);
    close((int)fd);
    _exit(pass && sent ? 0 : 1);
}

static int record(const char *name, int pass, unsigned long *total)
{
    (*total)++;
    dprintf(STDOUT_FILENO, "[DASICS-LIFECYCLE] case=%s value=0x%lx expect=0x1 result=%s\n",
            name, (unsigned long)!!pass, pass ? "PASS" : "FAIL");
    return !pass;
}

static void observation(const char *role, const struct lifecycle_report *report, int status)
{
    const struct lifecycle_state *state = &report->state;
    dprintf(STDOUT_FILENO, "DASICS_LIFECYCLE_STATE role=%s pid=%ld wait_status=%d "
            "umaincfg=0x%lx libcfg=0x%lx lib15lo=0x%lx lib15hi=0x%lx "
            "jumpcfg=0x%lx jump3lo=0x%lx jump3hi=0x%lx maincall=0x%lx "
            "returnpc=0x%lx active_returnpc=0x%lx freason=0x%lx "
            "ustatus=0x%lx uie=0x%lx utvec=0x%lx uscratch=0x%lx uepc=0x%lx "
            "ucause=0x%lx utval=0x%lx uip=0x%lx utimer=0x%lx runtime_traps=%lu\n",
            role, (long)report->pid, status, state->umain_cfg, state->lib_cfg,
            state->lib_lo, state->lib_hi, state->jump_cfg, state->jump_lo, state->jump_hi,
            state->maincall, state->return_pc, state->active_return_pc, state->freason,
            state->n[0], state->n[1], state->n[2], state->n[3], state->n[4], state->n[5],
            state->n[6], state->n[7], state->n[8], report->traps);
}

int main(int argc, char *argv[])
{
    if (argc >= 2 && !strcmp(argv[1], "--consumer"))
        return consumer(argc, argv);
    if (argc != 1)
        return 2;
    struct lifecycle_state baseline, expected, actual;
    struct lifecycle_report report = {0};
    unsigned long total = 0;
    int failures = 0, affinity_saved = 0, cpu = -1;
    int channel[2], status = -1, received = 0, exited = 0;
    pid_t pid;
    cpu_set_t original, one;
    read_state(&baseline);
    dprintf(STDOUT_FILENO, "[DASICS-LIFECYCLE] exit-then-exec observation begin\n");
    failures += record("PROFILE", baseline.umain_cfg == 2 && fdi_clear(&baseline) &&
                       n_clear(&baseline) && !dasics_n_extension_trap_count, &total);
    if (failures)
        goto out;
    affinity_saved = sched_getaffinity(0, sizeof(original), &original) == 0;
    CPU_ZERO(&one);
    if (affinity_saved) {
        for (int i = 0; i < CPU_SETSIZE; i++) {
            if (CPU_ISSET(i, &original)) {
                cpu = i;
                CPU_SET(i, &one);
                break;
            }
        }
    }
    int pinned = cpu >= 0 && sched_setaffinity(0, sizeof(one), &one) == 0;
    failures += record("CPU-AFFINITY", pinned, &total);
    if (!pinned)
        goto out;
    producer_expected(&expected, &baseline);
    if (pipe(channel)) {
        failures++;
        goto out;
    }
    pid = fork();
    if (!pid) {
        close(channel[0]);
        producer(channel[1], &expected);
        _exit(125);
    }
    close(channel[1]);
    if (pid > 0) {
        received = transfer(channel[0], &report, sizeof(report), 0);
        exited = waited(pid, &status);
    }
    close(channel[0]);
    int producer_valid = received && report.magic == LIFECYCLE_MAGIC && report.role == 1 && report.pid == pid;
    observation("producer", &report, status);
    failures += record("PRODUCER-FDI-SENTINEL", producer_valid && same_fdi(&report.state, &expected), &total);
    failures += record("PRODUCER-N-SENTINEL", producer_valid && !report.traps &&
                       !memcmp(report.state.n, expected.n, sizeof(expected.n)), &total);
    failures += record("PRODUCER-EXIT", producer_valid && exited, &total);
    read_state(&actual);
    failures += record("PARENT-STATE-PRESERVED", !memcmp(&actual, &baseline, sizeof(actual)), &total);
    if (failures)
        goto out;

    /* Exec also clears/rebuilds state. This observable boundary does not
     * isolate the internal cleanup performed by any particular exit hook. */
    char descriptor[32], lib_cfg[32], jump_cfg[32];
    if (pipe(channel)) {
        failures++;
        goto out;
    }
    snprintf(descriptor, sizeof(descriptor), "%x", (unsigned int)channel[1]);
    snprintf(lib_cfg, sizeof(lib_cfg), "%lx", baseline.lib_cfg);
    snprintf(jump_cfg, sizeof(jump_cfg), "%lx", baseline.jump_cfg);
    memset(&report, 0, sizeof(report));
    status = -1;
    pid = fork();
    if (!pid) {
        char *args[] = {"dasics-test-lifecycle", "--consumer", descriptor, lib_cfg, jump_cfg, "-dasics", NULL};
        char *environment[] = {"PATH=/bin:/sbin:/dasics", NULL};
        close(channel[0]);
        alarm(30);
        execve("/proc/self/exe", args, environment);
        _exit(127);
    }
    close(channel[1]);
    received = exited = 0;
    if (pid > 0) {
        received = transfer(channel[0], &report, sizeof(report), 0);
        exited = waited(pid, &status);
    }
    close(channel[0]);
    int consumer_valid = received && report.magic == LIFECYCLE_MAGIC && report.role == 2 && report.pid == pid;
    observation("consumer", &report, status);
    failures += record("CONSUMER-FDI-CLEAR", consumer_valid && fdi_clear(&report.state), &total);
    failures += record("CONSUMER-N-CLEAR", consumer_valid && n_clear(&report.state) && !report.traps, &total);
    failures += record("CONSUMER-EXEC-CONFIG", consumer_valid && report.state.umain_cfg == 2 &&
                       report.state.lib_cfg == baseline.lib_cfg && report.state.jump_cfg == baseline.jump_cfg, &total);
    failures += record("CONSUMER-EXIT", consumer_valid && exited, &total);
out:
    failures += record("CPU-AFFINITY-RESTORE", !affinity_saved ||
                       sched_setaffinity(0, sizeof(original), &original) == 0, &total);
    if (total != LIFECYCLE_TOTAL)
        failures++;
    dprintf(STDOUT_FILENO, "[DASICS-LIFECYCLE] summary total=%lu failed=%d result=%s\n",
            total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#endif
