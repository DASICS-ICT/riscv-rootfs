#define _GNU_SOURCE
#include <stdio.h>

#if !defined(DASICS_N_EXTENSION_PROFILE) || !DASICS_LINUX_DUAL_EXEC || !DASICS_VALIDATION_PROFILE
int main(void)
{
    fprintf(stderr, "[DASICS-VALIDATION] validation N profile required result=FAIL\n");
    return 2;
}
#else
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "udasics.h"
#include "fdi_verify_client.h"

#define REPORT_MAGIC 0x5645524946593031UL
#define CHILD_TIMEOUT_MS 60000
#define EXPECTED_TOTAL 9
#define CBIE_MASK 0x30UL

/* This program checks the software observation interface, not ISA coverage.
 * All instruction probes stay in trusted text and must generate no U24 trap. */
static unsigned long original_mask;
static unsigned int total, failed;
static volatile unsigned char inval_line[64] __attribute__((aligned(64)));
static volatile sig_atomic_t sigill_count, sigill_number, sigill_code, probe_active;
static volatile unsigned long sigill_pc, sigill_address;
/* Keep SIGILL recovery in trusted text; the vDSO restorer is outside MainBound. */
static sigjmp_buf sigill_resume;

extern void validation_trusted_inval(volatile unsigned char *address);
extern char validation_inval_site[], validation_inval_recovery[], validation_nop_site[];
asm (
    ".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global validation_trusted_inval\n.type validation_trusted_inval,@function\n"
    "validation_trusted_inval:\n.global validation_inval_site\nvalidation_inval_site:\n"
    ".insn r 0x0f, 2, 0, x0, a0, x0\n"
    ".global validation_inval_recovery\nvalidation_inval_recovery:\nret\n"
    ".size validation_trusted_inval,.-validation_trusted_inval\n.balign 8\n"
    ".global validation_nop_site\nvalidation_nop_site:\n.word 0x00000013\nret\n"
    ".option pop\n"
);

struct child_report {
    unsigned long magic, phase, traps;
    pid_t pid;
    int result, request_error, close_error;
    unsigned long signals, signal_number, signal_code, signal_pc, signal_address;
    struct fdi_verify_result observed;
};

static int close_fd(int *fd)
{
    int result = 0;
    if (*fd >= 0) {
        result = close(*fd);
        *fd = -1;
    }
    return result;
}

static int64_t milliseconds(void)
{
    struct timespec now;
    /* The vDSO reads VVAR outside the trusted executable mapping. */
    if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &now))
        return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int read_until(int fd, void *buffer, size_t size)
{
    int64_t start = milliseconds();
    size_t done = 0;
    if (start < 0)
        return 0;
    while (done < size) {
        int64_t now = milliseconds();
        int64_t left = start + CHILD_TIMEOUT_MS - now;
        struct pollfd item = {.fd = fd, .events = POLLIN};
        if (now < 0 || left <= 0)
            return 0;
        int ready = poll(&item, 1, left > INT_MAX ? INT_MAX : (int)left);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready <= 0 || !(item.revents & (POLLIN | POLLHUP)))
            return 0;
        ssize_t count = read(fd, (char *)buffer + done, size - done);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return 0;
        done += (size_t)count;
    }
    return 1;
}

static int write_all(int fd, const void *buffer, size_t size)
{
    size_t done = 0;
    while (done < size) {
        ssize_t count = write(fd, (const char *)buffer + done, size - done);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return 0;
        done += (size_t)count;
    }
    return 1;
}

static void stop_child(pid_t *pid)
{
    if (*pid > 0) {
        kill(*pid, SIGKILL);
        while (waitpid(*pid, NULL, 0) < 0 && errno == EINTR) { }
        *pid = -1;
    }
}

static int wait_child(pid_t *pid, int *status)
{
    int64_t start = milliseconds();
    while (*pid > 0 && start >= 0) {
        pid_t result = waitpid(*pid, status, WNOHANG);
        if (result == *pid) {
            *pid = -1;
            return WIFEXITED(*status) && WEXITSTATUS(*status) == 0;
        }
        if (result < 0 && errno != EINTR) {
            if (errno == ECHILD)
                *pid = -1;
            break;
        }
        int64_t now = milliseconds();
        if (now < 0 || now - start >= CHILD_TIMEOUT_MS)
            break;
        struct timespec delay = {.tv_nsec = 10000000};
        while (nanosleep(&delay, &delay) && errno == EINTR) { }
    }
    stop_child(pid);
    return 0;
}

static void signal_handler(int number, siginfo_t *info, void *context)
{
    ucontext_t *uc = context;
    if (!probe_active || sigill_count || number != SIGILL || !info ||
        info->si_code != ILL_ILLOPC ||
        (unsigned long)info->si_addr != (unsigned long)validation_inval_site ||
        uc->uc_mcontext.__gregs[REG_PC] != (unsigned long)validation_inval_site)
        _exit(119);
    sigill_count = 1;
    sigill_number = number;
    sigill_code = info->si_code;
    sigill_pc = uc->uc_mcontext.__gregs[REG_PC];
    sigill_address = (unsigned long)info->si_addr;
    siglongjmp(sigill_resume, 1);
}

static int probe_inval(int expected_signals, struct child_report *report)
{
    struct sigaction action = {.sa_sigaction = signal_handler, .sa_flags = SA_SIGINFO};
    struct sigaction previous;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGILL, &action, &previous))
        return 0;
    sigill_count = sigill_number = sigill_code = 0;
    sigill_pc = sigill_address = 0;
    if (sigsetjmp(sigill_resume, 1) == 0) {
        probe_active = 1;
        /* This zero-initialized line has no test data to lose on legal inval. */
        asm volatile("fence rw,rw" ::: "memory");
        validation_trusted_inval(inval_line);
    }
    asm volatile("fence rw,rw" ::: "memory");
    probe_active = 0;
    report->signals = sigill_count;
    report->signal_number = sigill_number;
    report->signal_code = sigill_code;
    report->signal_pc = sigill_pc;
    report->signal_address = sigill_address;
    return sigaction(SIGILL, &previous, NULL) == 0 && sigill_count == expected_signals &&
        (!expected_signals || (sigill_number == SIGILL && sigill_code == ILL_ILLOPC &&
         sigill_pc == (unsigned long)validation_inval_site &&
         sigill_address == (unsigned long)validation_inval_site));
}

static int restored(const struct fdi_verify_result *observed)
{
    return observed->saved_envcfg == original_mask &&
        (observed->senvcfg & FDI_VERIFY_ENVCFG_MASK) == original_mask &&
        (observed->shadow_envcfg & FDI_VERIFY_ENVCFG_MASK) == original_mask &&
        !observed->illegal_count;
}

static int enabled(const struct fdi_verify_result *observed)
{
    return observed->active == 1 && observed->flags == FDI_VERIFY_CBO &&
        observed->saved_envcfg == original_mask &&
        (observed->menvcfg & FDI_VERIFY_ENVCFG_MASK) == FDI_VERIFY_ENVCFG_MASK &&
        (observed->senvcfg & FDI_VERIFY_ENVCFG_MASK) == FDI_VERIFY_ENVCFG_MASK &&
        (observed->shadow_envcfg & FDI_VERIFY_ENVCFG_MASK) == FDI_VERIFY_ENVCFG_MASK &&
        observed->cbom_block_size == 64 && observed->cboz_block_size == 64 &&
        !observed->illegal_count;
}

static int begin_existing(struct fdi_verify_client *client, unsigned long flags)
{
    struct fdi_verify_result request = {.version = FDI_VERIFY_VERSION, .flags = flags};
    if (ioctl(client->fd, FDI_VERIFY_BEGIN, &request))
        return 0;
    if (request.version != FDI_VERIFY_VERSION || request.flags != flags ||
        request.active != 1 || !request.cookie || request.pid != (unsigned long)getpid())
        return 0;
    client->observed = request;
    return 1;
}

static int rejected(int fd, unsigned long command, struct fdi_verify_result *request, int expected)
{
    errno = 0;
    return ioctl(fd, command, request) == -1 && errno == expected;
}

static int finish(struct fdi_verify_client *client)
{
    return fdi_verify_end(client) == 0 && restored(&client->observed);
}

static int record(const char *name, int pass)
{
    total++;
    failed += !pass;
    printf("[DASICS-VALIDATION] case=%s result=%s\n", name, pass ? "PASS" : "FAIL");
    return pass;
}

static void state_output(const char *role, const struct fdi_verify_result *state)
{
    printf("DASICS_VERIFY_STATE role=%s pid=%llu cookie=0x%llx flags=0x%llx active=%llu "
           "menvcfg=0x%llx senvcfg=0x%llx shadow=0x%llx saved=0x%llx "
           "expect_saved=0x%lx cbom=%llu cboz=%llu illegal=%llu\n",
           role, state->pid, state->cookie, state->flags, state->active, state->menvcfg,
           state->senvcfg, state->shadow_envcfg, state->saved_envcfg, original_mask,
           state->cbom_block_size, state->cboz_block_size, state->illegal_count);
}

static void initialize_report(struct child_report *report, unsigned long phase)
{
    memset(report, 0, sizeof(*report));
    report->magic = REPORT_MAGIC;
    report->phase = phase;
    report->pid = getpid();
}

static int valid_report(const struct child_report *report, pid_t pid, unsigned long phase)
{
    return report->magic == REPORT_MAGIC && report->pid == pid && report->phase == phase &&
        report->result == 1 && report->traps == 0;
}

static void report_output(const char *role, const struct child_report *report, int status)
{
    printf("DASICS_VERIFY_CHILD role=%s pid=%ld phase=%lu wait_status=%d result=%d "
           "request_errno=%d close_errno=%d signals=%lu signo=%lu code=%lu "
           "pc=0x%lx address=0x%lx expected_pc=0x%lx traps=%lu\n",
           role, (long)report->pid, report->phase, status, report->result,
           report->request_error, report->close_error, report->signals, report->signal_number,
           report->signal_code, report->signal_pc, report->signal_address,
           (unsigned long)validation_inval_site, report->traps);
    if (report->observed.cookie)
        state_output(role, &report->observed);
}

static void send_and_exit(int output, struct child_report *report)
{
    report->traps = dasics_n_extension_trap_count;
    int sent = write_all(output, report, sizeof(*report));
    int closed = close(output) == 0;
    _exit(sent && closed && report->result && !report->traps ? 0 : 1);
}

static int profile(void)
{
    struct fdi_verify_client client = {.fd = -1};
    int pass = geteuid() == 0 && dasics_linux_query_umaincfg() == 2 &&
        dasics_n_extension_trap_count == 0 && fdi_verify_begin(&client, 0) == 0;
    if (pass) {
        original_mask = client.observed.saved_envcfg;
        state_output("baseline", &client.observed);
        pass = original_mask == 0xc0 && !(original_mask & CBIE_MASK) &&
            restored(&client.observed);
        pass &= finish(&client);
    }
    pass &= close_fd(&client.fd) == 0;
    return record("PROFILE", pass);
}

static int normal_window(void)
{
    struct fdi_verify_client client = {.fd = -1};
    int pass = fdi_verify_begin(&client, FDI_VERIFY_CBO) == 0;
    if (pass) {
        struct fdi_verify_result before = client.observed;
        state_output("begin", &before);
        pass = enabled(&before) && sched_yield() == 0 && fdi_verify_query(&client) == 0 &&
            enabled(&client.observed) && client.observed.cookie == before.cookie;
        state_output("scheduled-query", &client.observed);
        pass &= finish(&client);
        pass &= (client.observed.senvcfg & ~FDI_VERIFY_ENVCFG_MASK) ==
                    (before.senvcfg & ~FDI_VERIFY_ENVCFG_MASK) &&
                (client.observed.shadow_envcfg & ~FDI_VERIFY_ENVCFG_MASK) ==
                    (before.shadow_envcfg & ~FDI_VERIFY_ENVCFG_MASK);
        state_output("end", &client.observed);
    }
    pass &= close_fd(&client.fd) == 0;
    return record("BEGIN-QUERY-END", pass);
}

static void request_rejection(void)
{
    struct fdi_verify_client client = {.fd = -1};
    int pass = fdi_verify_begin(&client, 0) == 0;
    if (pass) {
        struct fdi_verify_result invalid = {.version = FDI_VERIFY_VERSION + 1};
        int version = rejected(client.fd, FDI_VERIFY_QUERY, &invalid, EINVAL);
        invalid = (struct fdi_verify_result){.version = FDI_VERIFY_VERSION,
            .kind = FDI_VERIFY_CSR_READ, .site = (unsigned long)validation_nop_site};
        int instruction = rejected(client.fd, FDI_VERIFY_ARM, &invalid, EINVAL);
        invalid.site++;
        int alignment = rejected(client.fd, FDI_VERIFY_ARM, &invalid, EINVAL);
        pass = version && instruction && alignment && fdi_verify_query(&client) == 0 &&
            restored(&client.observed) && !client.observed.kind && !client.observed.site;
        printf("DASICS_VERIFY_REJECT version=%d instruction=%d alignment=%d expect_errno=%d\n",
               version, instruction, alignment, EINVAL);
        pass &= finish(&client);
    }
    pass &= close_fd(&client.fd) == 0;
    record("REQUEST-REJECTION", pass);
}

static void dup_close(void)
{
    struct fdi_verify_client client = {.fd = -1};
    int alias = -1;
    int pass = fdi_verify_begin(&client, FDI_VERIFY_CBO) == 0;
    if (pass) {
        pass = enabled(&client.observed) && (alias = dup(client.fd)) >= 0;
        if (pass) {
            pass = close_fd(&alias) == 0;
            struct fdi_verify_result request = {.version = FDI_VERIFY_VERSION};
            pass &= rejected(client.fd, FDI_VERIFY_QUERY, &request, ESTALE);
            if (begin_existing(&client, 0)) {
                state_output("dup-close-restored", &client.observed);
                pass &= restored(&client.observed);
                pass &= finish(&client);
            } else {
                pass = 0;
            }
        }
    }
    pass &= close_fd(&alias) == 0;
    pass &= close_fd(&client.fd) == 0;
    record("DUP-CLOSE", pass);
}

static void fork_child(int inherited, int output, int control)
{
    struct child_report report;
    struct fdi_verify_result request = {.version = FDI_VERIFY_VERSION};
    initialize_report(&report, 1);
    alarm(CHILD_TIMEOUT_MS / 1000);
    errno = 0;
    int denied = ioctl(inherited, FDI_VERIFY_QUERY, &request) == -1;
    report.request_error = errno;
    report.result = denied && report.request_error == EPERM && probe_inval(1, &report);
    report.traps = dasics_n_extension_trap_count;
    if (!write_all(output, &report, sizeof(report)))
        _exit(120);
    unsigned char acknowledgement = 0;
    if (!read_until(control, &acknowledgement, 1) || acknowledgement != 1)
        _exit(121);
    initialize_report(&report, 2);
    errno = 0;
    int closed = close(inherited) == 0;
    report.close_error = closed ? 0 : errno;
    report.result = closed && close(control) == 0;
    send_and_exit(output, &report);
}

static void fork_windows(void)
{
    struct fdi_verify_client client = {.fd = -1};
    struct child_report first = {0}, second = {0};
    int output[2] = {-1, -1}, control[2] = {-1, -1};
    pid_t child = -1, child_id = -1;
    int isolation = 0, revoked = 0, status = -1, cleanup = 1;
    if (fdi_verify_begin(&client, FDI_VERIFY_CBO) || !enabled(&client.observed) ||
        pipe(output) || pipe(control))
        goto out;
    child = fork();
    child_id = child;
    if (child == 0) {
        if (close(output[0]) || close(control[1]))
            _exit(122);
        fork_child(client.fd, output[1], control[0]);
    }
    if (child < 0)
        goto out;
    if (close_fd(&output[1]) || close_fd(&control[0]))
        goto out;
    int received = read_until(output[0], &first, sizeof(first));
    isolation = received && valid_report(&first, child_id, 1) &&
        first.request_error == EPERM && first.signals == 1 &&
        fdi_verify_query(&client) == 0 && enabled(&client.observed);
    state_output("fork-parent-window", &client.observed);
    unsigned char acknowledgement = 1;
    if (!received || !write_all(control[1], &acknowledgement, 1))
        goto out;
    received = read_until(output[0], &second, sizeof(second));
    int exited = wait_child(&child, &status);
    revoked = received && valid_report(&second, child_id, 2) && exited && sched_yield() == 0;
    struct fdi_verify_result request = {.version = FDI_VERIFY_VERSION};
    revoked &= rejected(client.fd, FDI_VERIFY_QUERY, &request, ESTALE);
    if (begin_existing(&client, 0)) {
        state_output("fork-parent-restored", &client.observed);
        revoked &= restored(&client.observed);
        revoked &= finish(&client);
    } else {
        revoked = 0;
    }
    isolation &= exited;
out:
    stop_child(&child);
    cleanup &= close_fd(&output[0]) == 0;
    cleanup &= close_fd(&output[1]) == 0;
    cleanup &= close_fd(&control[0]) == 0;
    cleanup &= close_fd(&control[1]) == 0;
    cleanup &= close_fd(&client.fd) == 0;
    report_output("fork-before-close", &first, status);
    report_output("fork-after-close", &second, status);
    record("FORK-ISOLATION", isolation && cleanup);
    record("INHERITED-CLOSE", revoked && cleanup);
}

static void failed_exec(void)
{
    struct fdi_verify_client client = {.fd = -1};
    int pass = fdi_verify_begin(&client, FDI_VERIFY_CBO) == 0;
    if (pass) {
        unsigned long cookie = client.observed.cookie;
        pass = enabled(&client.observed);
        char *arguments[] = {"missing-validation-image", "-dasics", NULL};
        char *environment[] = {"PATH=/bin:/dasics", NULL};
        errno = 0;
        int result = execve("/__fdi_validation_missing__/image", arguments, environment);
        int error = errno;
        pass &= result == -1 && error == ENOENT && fdi_verify_query(&client) == 0 &&
            client.observed.cookie == cookie && enabled(&client.observed);
        printf("DASICS_VERIFY_EXEC outcome=failed errno=%d expect_errno=%d\n", error, ENOENT);
        state_output("failed-exec", &client.observed);
        pass &= finish(&client);
    }
    pass &= close_fd(&client.fd) == 0;
    record("FAILED-EXEC", pass);
}

static int parse_number(const char *text, unsigned long *value)
{
    char *end;
    errno = 0;
    *value = strtoul(text, &end, 16);
    return !errno && text[0] && !*end;
}

static void exec_observer(int argc, char **argv)
{
    unsigned long output_value, inherited_value;
    struct child_report report;
    struct fdi_verify_client client = {.fd = -1};
    if (argc != 5 || !parse_number(argv[2], &output_value) || output_value > INT_MAX ||
        !parse_number(argv[3], &inherited_value) || inherited_value > INT_MAX ||
        !parse_number(argv[4], &original_mask) || original_mask != 0xc0)
        _exit(123);
    int inherited = (int)inherited_value;
    initialize_report(&report, 4);
    alarm(CHILD_TIMEOUT_MS / 1000);
    /* Keep the old fd open until this real instruction proves exec restored
     * CBIE. Closing it earlier would let close cleanup hide an exec defect. */
    int protection = probe_inval(1, &report);
    struct fdi_verify_result request = {.version = FDI_VERIFY_VERSION};
    errno = 0;
    int denied = ioctl(inherited, FDI_VERIFY_QUERY, &request) == -1;
    report.request_error = errno;
    errno = 0;
    int closed = close_fd(&inherited) == 0;
    report.close_error = closed ? 0 : errno;
    report.result = protection && denied && report.request_error == EPERM && closed &&
        dasics_linux_query_umaincfg() == 2;
    if (fdi_verify_begin(&client, 0) == 0) {
        report.observed = client.observed;
        report.result &= restored(&client.observed);
        report.result &= finish(&client);
    } else {
        report.result = 0;
    }
    report.result &= close_fd(&client.fd) == 0;
    send_and_exit((int)output_value, &report);
}

static void exec_producer(int output)
{
    struct fdi_verify_client client = {.fd = -1};
    struct child_report report;
    initialize_report(&report, 3);
    alarm(CHILD_TIMEOUT_MS / 1000);
    report.result = fdi_verify_begin(&client, FDI_VERIFY_CBO) == 0;
    if (report.result) {
        report.result = enabled(&client.observed) && probe_inval(0, &report) &&
            fdi_verify_query(&client) == 0 && enabled(&client.observed);
        report.observed = client.observed;
        int flags = fcntl(client.fd, F_GETFD);
        report.result &= flags >= 0 && fcntl(client.fd, F_SETFD, flags & ~FD_CLOEXEC) == 0;
    }
    report.traps = dasics_n_extension_trap_count;
    if (!report.result) {
        close_fd(&client.fd);
        send_and_exit(output, &report);
    }
    if (!write_all(output, &report, sizeof(report)))
        _exit(124);
    char output_text[32], inherited_text[32], original_text[32];
    snprintf(output_text, sizeof(output_text), "%x", (unsigned int)output);
    snprintf(inherited_text, sizeof(inherited_text), "%x", (unsigned int)client.fd);
    snprintf(original_text, sizeof(original_text), "%lx", original_mask);
    char *arguments[] = {"dasics-test-validation-smoke", "--exec-observer", output_text,
        inherited_text, original_text, "-dasics", NULL};
    char *environment[] = {"PATH=/bin:/sbin:/dasics", NULL};
    execve("/proc/self/exe", arguments, environment);
    close_fd(&client.fd);
    _exit(127);
}

static void successful_exec(void)
{
    struct child_report before = {0}, after = {0};
    int channel[2] = {-1, -1}, status = -1, pass = 0;
    pid_t child = -1, child_id = -1;
    if (pipe(channel))
        goto out;
    child = fork();
    child_id = child;
    if (child == 0) {
        if (close(channel[0]))
            _exit(125);
        exec_producer(channel[1]);
    }
    if (child < 0 || close_fd(&channel[1]))
        goto out;
    int first = read_until(channel[0], &before, sizeof(before));
    int second = first && read_until(channel[0], &after, sizeof(after));
    int exited = wait_child(&child, &status);
    pass = first && second && exited && valid_report(&before, child_id, 3) &&
        valid_report(&after, child_id, 4) && enabled(&before.observed) &&
        before.signals == 0 && after.signals == 1 && after.request_error == EPERM &&
        restored(&after.observed);
out:
    stop_child(&child);
    int cleanup = close_fd(&channel[0]) == 0;
    cleanup &= close_fd(&channel[1]) == 0;
    report_output("exec-before", &before, status);
    report_output("exec-after", &after, status);
    record("SUCCESSFUL-EXEC", pass && cleanup);
}

static void capability_child(int output)
{
    struct child_report report;
    struct fdi_verify_result request = {.version = FDI_VERIFY_VERSION, .flags = FDI_VERIFY_CBO};
    initialize_report(&report, 5);
    alarm(CHILD_TIMEOUT_MS / 1000);
    int descriptor = open("/dev/fdi-verify", O_RDWR | O_CLOEXEC);
    report.result = descriptor >= 0 && setuid(65534) == 0 && getuid() == 65534 && geteuid() == 65534;
    if (report.result) {
        errno = 0;
        int denied = ioctl(descriptor, FDI_VERIFY_BEGIN, &request) == -1;
        report.request_error = errno;
        report.result = denied && report.request_error == EPERM && probe_inval(1, &report);
    }
    errno = 0;
    int closed = close_fd(&descriptor) == 0;
    report.close_error = closed ? 0 : errno;
    report.result &= closed;
    send_and_exit(output, &report);
}

static void capability(void)
{
    struct child_report report = {0};
    struct fdi_verify_client client = {.fd = -1};
    int channel[2] = {-1, -1}, status = -1, pass = 0;
    pid_t child = -1, child_id = -1;
    if (pipe(channel))
        goto out;
    child = fork();
    child_id = child;
    if (child == 0) {
        if (close(channel[0]))
            _exit(126);
        capability_child(channel[1]);
    }
    if (child < 0 || close_fd(&channel[1]))
        goto out;
    int received = read_until(channel[0], &report, sizeof(report));
    int exited = wait_child(&child, &status);
    pass = received && exited && valid_report(&report, child_id, 5) &&
        report.request_error == EPERM && report.signals == 1 && geteuid() == 0;
out:
    stop_child(&child);
    int cleanup = close_fd(&channel[0]) == 0;
    cleanup &= close_fd(&channel[1]) == 0;
    if (fdi_verify_begin(&client, 0) == 0) {
        state_output("capability-parent-restored", &client.observed);
        cleanup &= restored(&client.observed);
        cleanup &= finish(&client);
    } else {
        cleanup = 0;
    }
    cleanup &= close_fd(&client.fd) == 0;
    report_output("capability", &report, status);
    record("CAPABILITY", pass && cleanup);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "--exec-observer"))
        exec_observer(argc, argv);
    if (argc != 1)
        return 2;
    struct sigaction ignored = {.sa_handler = SIG_IGN}, previous;
    sigemptyset(&ignored.sa_mask);
    if (sigaction(SIGPIPE, &ignored, &previous))
        return 2;
    printf("[DASICS-VALIDATION] interface lifecycle checks begin pid=%ld\n", (long)getpid());
    if (!profile() || !normal_window())
        goto out;
    request_rejection();
    dup_close();
    fork_windows();
    failed_exec();
    successful_exec();
    capability();
out:
    if (sigaction(SIGPIPE, &previous, NULL) || dasics_n_extension_trap_count || total != EXPECTED_TOTAL)
        failed++;
    printf("[DASICS-VALIDATION] summary total=%u failed=%u user_traps=%lu result=%s\n",
           total, failed, (unsigned long)dasics_n_extension_trap_count, failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
#endif
