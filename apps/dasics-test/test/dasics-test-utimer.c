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

/* Called after the assembly entry saves all integer GPRs and clears the timer.
 * No libc, FP/vector operations, or changes to uepc/uscratch in this callback. */
void dasics_utimer_handler(const unsigned long *frame)
{
    last_epc = csr_read(CSR_UEPC);
    if (csr_read(CSR_UCAUSE) != UTIMER_CAUSE || csr_read(CSR_UTVAL) ||
        (csr_read(CSR_USTATUS) & 1) || !frame[1])
        bad_context++;
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
    if (dasics_linux_parse_exec_mode(argc, argv, &mode)) return 2;
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
    csr_write(CSR_UIE, 0);
    printf("UTIMER_SUITE mode=%s cases=14 failed=%u result=%s\n",
           dasics_linux_exec_mode_name(mode), failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
