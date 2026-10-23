#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "udasics.h"
#include "ucsr.h"

static __thread volatile unsigned long hits, bad_context, last_epc;
static volatile unsigned long release_loop;
static unsigned failures;
static const unsigned long budget = 50000;
static __thread volatile unsigned long capture_extended, extended_events;

struct timer_state {
    unsigned long hits, timer, pending, uie, ustatus, freason, uscratch;
};

struct timer_entry {
    unsigned long sequence, cause, epc, tval, ustatus, freason, uscratch, timer, pending, sp;
};

/* Only the selected parent-thread probes collect these observations. The
 * common entry has already cancelled the timer before calling this handler. */
static __thread volatile struct timer_entry extended_entry;

/* Called after the assembly entry saves all integer GPRs and clears the timer.
 * No libc, FP/vector operations, or changes to uepc/uscratch in this callback. */
void dasics_utimer_handler(const unsigned long *frame)
{
    last_epc = csr_read(CSR_UEPC);
    if (csr_read(CSR_UCAUSE) != UTIMER_CAUSE || csr_read(CSR_UTVAL) ||
        (csr_read(CSR_USTATUS) & 1) || !frame[1])
        bad_context++;
    if (capture_extended) {
        extended_entry.sequence = hits + 1;
        extended_entry.cause = csr_read(CSR_UCAUSE);
        extended_entry.epc = last_epc;
        extended_entry.tval = csr_read(CSR_UTVAL);
        extended_entry.ustatus = csr_read(CSR_USTATUS);
        extended_entry.freason = csr_read(CSR_DFREASON);
        extended_entry.uscratch = csr_read(CSR_USCRATCH);
        extended_entry.timer = csr_read(CSR_UTIMER);
        extended_entry.pending = csr_read(CSR_UIP) & UTIMER_IRQ;
        extended_entry.sp = frame[1];
        extended_events++;
        if ((extended_entry.ustatus & 0x11) != 0x10)
            bad_context++;
    }
    hits++;
    release_loop = 1;
}

static void check(const char *name, int ok)
{
    failures += !ok;
    printf("UTIMER_CASE name=%s result=%s hits=%lu bad_context=%lu uepc=0x%lx\n",
           name, ok ? "PASS" : "FAIL", hits, bad_context, last_epc);
}

static void enable(void)
{
    csr_write(CSR_UIE, UTIMER_IRQ);
    csr_write(CSR_USTATUS, 1);
}

static int wait_hits(unsigned long expected)
{
    for (unsigned long i = 0; i < 10000000 && hits < expected; i++)
        asm volatile("nop" ::: "memory");
    return hits == expected && !bad_context;
}

static int wait_pending(void)
{
    for (unsigned long i = 0; i < 10000000; i++)
        if (csr_read(CSR_UTIMER) == 1)
            return (csr_read(CSR_UIP) & UTIMER_IRQ) != 0;
    return 0;
}

static void sample_state(struct timer_state *state)
{
    state->hits = hits;
    state->timer = csr_read(CSR_UTIMER);
    state->pending = csr_read(CSR_UIP) & UTIMER_IRQ;
    state->uie = csr_read(CSR_UIE);
    state->ustatus = csr_read(CSR_USTATUS);
    state->freason = csr_read(CSR_DFREASON);
    state->uscratch = csr_read(CSR_USCRATCH);
}

static void print_state(const char *stage, const struct timer_state *state)
{
    printf("UTIMER_EXT_STATE stage=%s hits=%lu timer=0x%lx pending=0x%lx uie=0x%lx "
           "ustatus=0x%lx freason=0x%lx uscratch=0x%lx bad_context=%lu\n",
           stage, state->hits, state->timer, state->pending, state->uie,
           state->ustatus, state->freason, state->uscratch, bad_context);
}

static int preserved(const struct timer_state *state, unsigned long scratch)
{
    return state->ustatus == 0x11 && state->freason == 5 && state->uscratch == scratch;
}

static int irq_observation(const char *event, unsigned long before,
                           const struct timer_state *post, unsigned long scratch)
{
    struct timer_entry entry = extended_entry;
    int pass = post->hits == before + 1 && entry.sequence == post->hits &&
        entry.cause == UTIMER_CAUSE && entry.epc && !(entry.epc & 1) && !entry.tval &&
        entry.ustatus == 0x10 && entry.freason == 5 && entry.uscratch == scratch &&
        !entry.timer && !entry.pending && entry.sp && !bad_context &&
        !post->timer && !post->pending && post->uie == UTIMER_IRQ && preserved(post, scratch);
    printf("UTIMER_EXT_IRQ event=%s before=%lu after=%lu sequence=%lu cause=0x%lx "
           "epc=0x%lx tval=0x%lx entry_ustatus=0x%lx entry_freason=0x%lx "
           "entry_uscratch=0x%lx callback_timer=0x%lx callback_pending=0x%lx sp=0x%lx result=%s\n",
           event, before, post->hits, entry.sequence, entry.cause, entry.epc, entry.tval,
           entry.ustatus, entry.freason, entry.uscratch, entry.timer, entry.pending,
           entry.sp, pass ? "PASS" : "FAIL");
    return pass;
}

static int stable_pending(const char *first_name, const char *stable_name,
                          unsigned long before, unsigned long scratch)
{
    struct timer_state first, stable;
    csr_write(CSR_UIE, 0);
    dasics_utimer_arm(2);
    int pending = wait_pending();
    sample_state(&first);
    int scheduled = sched_yield() == 0;
    sample_state(&stable);
    print_state(first_name, &first);
    print_state(stable_name, &stable);
    return pending && scheduled && first.hits == before && stable.hits == before &&
        first.timer == 1 && stable.timer == 1 && first.pending == UTIMER_IRQ &&
        stable.pending == UTIMER_IRQ && !first.uie && !stable.uie &&
        preserved(&first, scratch) && preserved(&stable, scratch);
}

/* This is a finite userspace observation interval, not a cycle-count oracle. */
static int no_old_irq(unsigned long before)
{
    if (sched_yield())
        return 0;
    for (unsigned long index = 0; index < 100000 && hits == before; index++)
        asm volatile("nop" ::: "memory");
    return hits == before && !bad_context;
}

static void extended_checks(enum dasics_linux_exec_mode mode)
{
    struct timer_state saved, post, cleared, reloaded, quiet, restored;
    sample_state(&saved);
    printf("UTIMER_EXT_BEGIN version=1 mode=%s scope=user-observable-state\n",
           dasics_linux_exec_mode_name(mode));
    print_state("baseline", &saved);
    int initial = !saved.timer && !saved.pending && saved.uie == UTIMER_IRQ &&
        saved.ustatus == 0x11 && !dasics_n_extension_trap_count;
    csr_write(CSR_UIE, 0);
    dasics_utimer_cancel();
    csr_write(CSR_DFREASON, 5);
    capture_extended = 1;
    extended_events = 0;
    enable();
    unsigned long before = hits;
    dasics_utimer_arm(budget);
    int delivered = wait_hits(before + 1);
    sample_state(&post);
    print_state("irq-post", &post);
    int observed = irq_observation("state-preserve", before, &post, saved.uscratch);
    check("irq-state-preserve", initial && delivered && observed);

    before = hits;
    int pending = stable_pending("cancel-pending-first", "cancel-pending-stable", before, saved.uscratch);
    dasics_utimer_cancel();
    sample_state(&cleared);
    print_state("cancel-cleared", &cleared);
    csr_write(CSR_UIE, UTIMER_IRQ);
    int quiet_ok = no_old_irq(before);
    sample_state(&quiet);
    print_state("cancel-unmasked", &quiet);
    check("pending-cancel", pending && quiet_ok && cleared.hits == before && quiet.hits == before &&
          !cleared.timer && !cleared.pending && !cleared.uie && !quiet.timer && !quiet.pending &&
          quiet.uie == UTIMER_IRQ && preserved(&cleared, saved.uscratch) && preserved(&quiet, saved.uscratch));

    before = hits;
    pending = stable_pending("reload-pending-first", "reload-pending-stable", before, saved.uscratch);
    const unsigned long long_budget = 1UL << 40;
    dasics_utimer_arm(long_budget);
    sample_state(&reloaded);
    print_state("reloaded", &reloaded);
    csr_write(CSR_UIE, UTIMER_IRQ);
    quiet_ok = no_old_irq(before);
    sample_state(&quiet);
    print_state("reload-unmasked", &quiet);
    int reloaded_ok = pending && quiet_ok && reloaded.hits == before && quiet.hits == before &&
        reloaded.timer > 1 && reloaded.timer <= long_budget && quiet.timer > 1 &&
        quiet.timer <= reloaded.timer && !reloaded.pending && !reloaded.uie &&
        !quiet.pending && quiet.uie == UTIMER_IRQ && preserved(&reloaded, saved.uscratch) &&
        preserved(&quiet, saved.uscratch);
    /* Replace the long nonpending budget explicitly before expecting a new IRQ. */
    dasics_utimer_arm(budget);
    delivered = wait_hits(before + 1);
    sample_state(&post);
    print_state("reload-irq-post", &post);
    observed = irq_observation("reload-delivery", before, &post, saved.uscratch);
    check("pending-reload", reloaded_ok && delivered && observed);

    csr_write(CSR_UIE, 0);
    dasics_utimer_cancel();
    capture_extended = 0;
    csr_write(CSR_DFREASON, saved.freason);
    csr_write(CSR_USCRATCH, saved.uscratch);
    csr_write(CSR_USTATUS, saved.ustatus);
    csr_write(CSR_UIE, saved.uie);
    sample_state(&restored);
    print_state("restored", &restored);
    check("extended-state-restore", !restored.timer && !restored.pending &&
          restored.freason == saved.freason && restored.uscratch == saved.uscratch &&
          restored.uie == saved.uie && restored.ustatus == saved.ustatus &&
          restored.hits == saved.hits + 2 && extended_events == 2 && !bad_context &&
          !dasics_n_extension_trap_count);
    printf("UTIMER_EXT_END version=1 events=%lu u24=%lu scope=finite-software-observation "
           "internal_cycle_boundary=NOT_OBSERVED\n", extended_events, dasics_n_extension_trap_count);
}

/* The loop is intentionally in the same untrusted section as existing tests. */
int ATTR_ULIB_TEXT untrusted_loop(void)
{
    while (!release_loop)
        asm volatile("nop" ::: "memory");
    asm volatile(".global utimer_loop_end\nutimer_loop_end:");
    return 7;
}

static void *worker(void *unused)
{
    (void)unused;
    int ok = csr_read(CSR_UTIMER) == 0;
    enable();
    for (int i = 0; i < 4; i++) {
        csr_write(CSR_USTATUS, 0);
        dasics_utimer_arm(2);
        ok &= wait_pending();
        sched_yield();
        ok &= csr_read(CSR_UTIMER) == 1;
        csr_write(CSR_USTATUS, 1);
        ok &= wait_hits(i + 1);
    }
    dasics_utimer_cancel();
    return (void *)(uintptr_t)!ok;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--child-exec"))
        return csr_read(CSR_UTIMER) != 0;

    enum dasics_linux_exec_mode mode;
    int extended = 0, mode_argc = 1;
    char *mode_argv[] = {argv[0], NULL, NULL};
    for (int index = 1; index < argc; index++) {
        if (!strcmp(argv[index], "--isa-extended")) {
            if (extended++) return 2;
        } else if (mode_argc == 1) {
            mode_argv[mode_argc++] = argv[index];
        } else {
            return 2;
        }
    }
    if (dasics_linux_parse_exec_mode(mode_argc, mode_argv, &mode)) return 2;
    printf("UTIMER_BEGIN mode=%s\n", dasics_linux_exec_mode_name(mode));
    check("reset", csr_read(CSR_UTIMER) == 0);
    csr_write(CSR_USTATUS, 0);
    dasics_utimer_arm(1);
    unsigned long old, sampled, exchanged, cleared;
    asm volatile("csrrs %0, 0x045, %1" : "=r"(old) : "r"(1UL << 40) : "memory");
    sampled = csr_read(CSR_UTIMER);
    asm volatile("csrrw %0, 0x045, %1" : "=r"(exchanged) : "r"(1UL) : "memory");
    asm volatile("csrrci %0, 0x045, 1" : "=r"(cleared) :: "memory");
    check("csr-rmw-64", old == 1 && sampled > (1UL << 39) &&
          sampled <= ((1UL << 40) + 1) && exchanged > (1UL << 39) &&
          exchanged <= sampled && cleared == 1 && csr_read(CSR_UTIMER) == 0);
    enable();
    dasics_utimer_arm(budget);
    check("one-shot", wait_hits(1) && csr_read(CSR_UTIMER) == 0);

    unsigned long before = hits;
    for (int i = 1; i <= 10; i++) {
        dasics_utimer_arm(budget);
        if (!wait_hits(before + i)) break;
    }
    check("rearm", hits == before + 10 && !bad_context);
    before = hits;
    dasics_utimer_arm(1UL << 32);
    dasics_utimer_cancel();
    for (unsigned i = 0; i < 100000; i++) asm volatile("nop");
    check("cancel", hits == before && csr_read(CSR_UTIMER) == 0 &&
          !(csr_read(CSR_UIP) & UTIMER_IRQ));

    csr_write(CSR_USTATUS, 0);
    dasics_utimer_arm(2);
    int pending = wait_pending();
    csr_write(CSR_UIP, 0);
    check("uie-mask-pending", pending && hits == before &&
          (csr_read(CSR_UIP) & UTIMER_IRQ));
    csr_write(CSR_USTATUS, 1);
    check("uie-unmask", wait_hits(before + 1));
    before = hits;
    csr_write(CSR_UIE, 0);
    dasics_utimer_arm(2);
    pending = wait_pending();
    check("utie-mask-pending", pending && hits == before);
    csr_write(CSR_UIE, UTIMER_IRQ);
    check("utie-unmask", wait_hits(before + 1));

    /* A pending-but-masked timer is deterministic across scheduling/fork. */
    csr_write(CSR_USTATUS, 0);
    dasics_utimer_arm(1);
    pid_t child = fork();
    if (child == 0) _exit(csr_read(CSR_UTIMER) != 0);
    int status = -1;
    if (child > 0) waitpid(child, &status, 0);
    check("fork", child > 0 && WIFEXITED(status) && !WEXITSTATUS(status) &&
          csr_read(CSR_UTIMER) == 1);
    char *bad_argv[] = {"missing", NULL};
    errno = 0;
    execv("/definitely-no-utimer-executable", bad_argv);
    check("failed-exec", errno == ENOENT && csr_read(CSR_UTIMER) == 1);
    dasics_utimer_cancel();

    child = fork();
    if (child == 0) {
        csr_write(CSR_USTATUS, 0);
        dasics_utimer_arm(1);
        char *args[] = {argv[0], "--child-exec", NULL};
        execv(argv[0], args);
        _exit(100);
    }
    status = -1;
    if (child > 0) waitpid(child, &status, 0);
    check("successful-exec", child > 0 && WIFEXITED(status) && !WEXITSTATUS(status));

    cpu_set_t available, one;
    int affinity = sched_getaffinity(0, sizeof(available), &available);
    CPU_ZERO(&one);
    if (!affinity) {
        for (int i = 0; i < CPU_SETSIZE; i++) if (CPU_ISSET(i, &available)) {
            CPU_SET(i, &one); break;
        }
        affinity = sched_setaffinity(0, sizeof(one), &one);
    }
    pthread_t thread;
    before = hits;
    csr_write(CSR_USTATUS, 0);
    dasics_utimer_arm(1);
    int created = pthread_create(&thread, NULL, worker, NULL);
    void *thread_result = (void *)1;
    if (!created) pthread_join(thread, &thread_result);
    check("thread-isolation", !affinity && !created && !thread_result &&
          hits == before && csr_read(CSR_UTIMER) == 1);
    dasics_utimer_cancel();

    enable();
    release_loop = 0;
    before = hits;
    int bound = dasics_libcfg_alloc(DASICS_LIBCFG_R,
        DASICS_BOUND_ALIGN_DOWN(&release_loop), DASICS_BOUND_ALIGN_UP(&release_loop + 1));
    extern char utimer_loop_end[];
    int jump = dasics_jumpcfg_alloc((uintptr_t)untrusted_loop,
                                    DASICS_BOUND_ALIGN_UP(utimer_loop_end + 1));
    dasics_utimer_arm(budget);
    int value = bound < 0 || jump < 0 ? -1 : mode == DASICS_LINUX_EXEC_OFF
        ? untrusted_loop() : lib_call(untrusted_loop);
    dasics_utimer_cancel();
    if (bound >= 0) dasics_libcfg_free(bound);
    if (jump >= 0) dasics_jumpcfg_free(jump);
    check("untrusted-loop", value == 7 && hits == before + 1 && !bad_context &&
          last_epc >= (uintptr_t)untrusted_loop && last_epc < (uintptr_t)utimer_loop_end &&
          dasics_n_extension_trap_count == 0);
    if (extended)
        extended_checks(mode);
    csr_write(CSR_UIE, 0);
    printf("UTIMER_SUITE mode=%s cases=%u failed=%u result=%s\n",
           dasics_linux_exec_mode_name(mode), extended ? 18U : 14U, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
