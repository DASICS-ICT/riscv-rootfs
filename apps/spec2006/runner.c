#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct spec_command {
    const char *const *argv;
    const char *const *fdi_argv;
    const char *const *diagnostic_argv;
    const char *input;
    const char *output;
    const char *error;
};

struct spec_program {
    const char *name;
    const struct spec_command *commands;
    size_t count;
};

#include "generated_catalog.h"

/* Paired children use this descriptor only for LibDASICS execution evidence. */
#define SPEC_RUNTIME_FD 198

/* SIGCHLD stays blocked until wait4 consumes each child's status. */
static volatile sig_atomic_t interrupted;

static void on_interrupt(int signo)
{
    interrupted = signo;
}

static uint64_t monotonic_ns(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) {
        perror("clock_gettime");
        exit(2);
    }
    return (uint64_t)now.tv_sec * 1000000000ULL + now.tv_nsec;
}

static uint64_t timeval_us(struct timeval value)
{
    return (uint64_t)value.tv_sec * 1000000ULL + value.tv_usec;
}

static int valid_runid(const char *value)
{
    return strlen(value) == 32 && strspn(value, "0123456789abcdef") == 32;
}

int main(int argc, char **argv)
{
    const struct spec_program *program = NULL;
    struct sigaction action = {0};
    sigset_t child_signal, old_mask;
    uint64_t total_wall = 0, total_user = 0, total_sys = 0;
    uint64_t first_start = 0, last_end = 0;
    long peak_rss = 0;
    size_t completed = 0;
    int failed = 0;
    const char *mode_record = "";
    int fdi = 0;
    int diagnostic = 0;

    if (argc != (SPEC_PAIRED ? 4 : 3) || !valid_runid(argv[2])) {
        fprintf(stderr, "usage: runner PROGRAM LOWERCASE_HEX32_RUNID [ordinary|fdi|diagnostic]\n");
        return 2;
    }
    if (SPEC_PAIRED) {
        if (!strcmp(argv[3], "ordinary"))
            mode_record = " mode=ordinary";
        else if (!strcmp(argv[3], "fdi")) {
            mode_record = " mode=fdi";
            fdi = 1;
        } else if (!strcmp(argv[3], "diagnostic")) {
            mode_record = " mode=diagnostic";
            fdi = diagnostic = 1;
        } else
            return 2;
    }
    for (size_t i = 0; i < sizeof(spec_programs) / sizeof(spec_programs[0]); ++i)
        if (!strcmp(argv[1], spec_programs[i].name))
            program = &spec_programs[i];
    if (!program)
        return 2;
    if (diagnostic && !program->commands[0].diagnostic_argv)
        return 2;
    if (SPEC_PAIRED) {
        /* Reserve the number before opening pipes so metadata cannot replace an error pipe. */
        int reservation = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (reservation < 0)
            return 2;
        if (reservation != SPEC_RUNTIME_FD) {
            int reserved = dup3(reservation, SPEC_RUNTIME_FD, O_CLOEXEC);
            close(reservation);
            if (reserved < 0)
                return 2;
        }
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    action.sa_handler = on_interrupt;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigemptyset(&child_signal);
    sigaddset(&child_signal, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &child_signal, &old_mask))
        return 2;

    printf("SPEC_TRAIN_BEGIN schema=1 program=%s runid=%s%s calls=%zu\n",
           program->name, argv[2], mode_record, program->count);
    for (size_t i = 0; i < program->count; ++i) {
        const struct spec_command *command = &program->commands[i];
        struct rusage usage = {0};
        int status = 0, execution_errno = 0, error_pipe[2];
        int runtime_log = -1;
        if (fdi) {
            char name[64];
            snprintf(name, sizeof(name), "runtime-%03zu.log", i + 1);
            runtime_log = open(name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            if (runtime_log < 0) {
                perror("prepare runtime log");
                failed = 1;
                break;
            }
        }
        int in = open(command->input ? command->input : "/dev/null", O_RDONLY);
        int out = open(command->output, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int err = open(command->error, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (in < 0 || out < 0 || err < 0 || pipe2(error_pipe, O_CLOEXEC)) {
            perror("prepare child files");
            if (in >= 0) close(in);
            if (out >= 0) close(out);
            if (err >= 0) close(err);
            if (runtime_log >= 0) close(runtime_log);
            failed = 1;
            break;
        }

        /* This interval contains fork/exec and execution, not input staging or output checks. */
        uint64_t start = monotonic_ns();
        if (!first_start)
            first_start = start;
        pid_t child = fork();
        if (!child) {
            int child_errno;
            struct sigaction default_action = {0};
            default_action.sa_handler = SIG_DFL;
            sigaction(SIGINT, &default_action, NULL);
            sigaction(SIGTERM, &default_action, NULL);
            close(error_pipe[0]);
            setpgid(0, 0);
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            if (dup2(in, STDIN_FILENO) < 0 || dup2(out, STDOUT_FILENO) < 0 ||
                dup2(err, STDERR_FILENO) < 0)
                goto child_error;
            /* Runtime metadata uses a dedicated descriptor, outside SPEC outputs. */
            if (fdi && dup2(runtime_log, SPEC_RUNTIME_FD) < 0)
                goto child_error;
            if (SPEC_PAIRED && !fdi)
                close(SPEC_RUNTIME_FD);
            close(in);
            close(out);
            close(err);
            if (runtime_log >= 0)
                close(runtime_log);
            /* Only the benchmark child receives the kernel's final opt-in argument. */
            const char *const *child_argv = diagnostic ? command->diagnostic_argv :
                                           fdi ? command->fdi_argv : command->argv;
            execv(child_argv[0], (char *const *)child_argv);
child_error:
            child_errno = errno;
            (void)!write(error_pipe[1], &child_errno, sizeof(child_errno));
            _exit(127);
        }
        close(in);
        close(out);
        close(err);
        if (runtime_log >= 0) close(runtime_log);
        close(error_pipe[1]);
        if (child < 0) {
            perror("fork");
            close(error_pipe[0]);
            failed = 1;
            break;
        }
        setpgid(child, child);
        uint64_t next_progress = start + 300000000000ULL;
        int reaped = 0;
        while (!reaped) {
            pid_t observed = wait4(child, &status, WNOHANG, &usage);
            if (observed == child) {
                reaped = 1;
                break;
            }
            if (observed < 0 && errno != EINTR) {
                perror("wait4");
                failed = 1;
                break;
            }
            if (interrupted)
                kill(-child, interrupted);
            uint64_t now = monotonic_ns();
            if (now >= next_progress) {
                printf("SPEC_TRAIN_PROGRESS schema=1 program=%s runid=%s%s index=%zu elapsed_ns=%" PRIu64 "\n",
                       program->name, argv[2], mode_record, i + 1, now - start);
                next_progress = now + 300000000000ULL;
            }
            uint64_t remaining = next_progress - now;
            struct timespec timeout = {(time_t)(remaining / 1000000000ULL),
                                       (long)(remaining % 1000000000ULL)};
            int received = sigtimedwait(&child_signal, NULL, &timeout);
            if (received < 0 && errno != EAGAIN && errno != EINTR) {
                perror("sigtimedwait");
                kill(-child, SIGKILL);
                while (wait4(child, &status, 0, &usage) < 0 && errno == EINTR) {}
                failed = 1;
                reaped = 1;
            }
        }
        /* Capture the endpoint before reading metadata or printing UART records. */
        uint64_t end = monotonic_ns();
        last_end = end;
        ssize_t count;
        do {
            count = read(error_pipe[0], &execution_errno, sizeof(execution_errno));
        } while (count < 0 && errno == EINTR);
        close(error_pipe[0]);
        if (count != 0 && count != sizeof(execution_errno)) {
            execution_errno = EIO;
            failed = 1;
        }
        uint64_t wall = end - start;
        uint64_t user = timeval_us(usage.ru_utime), sys = timeval_us(usage.ru_stime);
        int exit_code = reaped && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        int signal_number = reaped && WIFSIGNALED(status) ? WTERMSIG(status) : 0;
        total_wall += wall;
        total_user += user;
        total_sys += sys;
        if (usage.ru_maxrss > peak_rss)
            peak_rss = usage.ru_maxrss;
        ++completed;
        printf("SPEC_TRAIN_CASE schema=1 program=%s runid=%s%s index=%zu wall_ns=%" PRIu64
               " user_us=%" PRIu64 " sys_us=%" PRIu64 " maxrss_kb=%ld exit=%d signal=%d exec_errno=%d\n",
               program->name, argv[2], mode_record, i + 1, wall, user, sys, usage.ru_maxrss,
               exit_code, signal_number, execution_errno);
        if (exit_code || signal_number || execution_errno || interrupted || failed) {
            failed = 1;
            break;
        }
    }
    printf("SPEC_TRAIN_END schema=1 program=%s runid=%s%s completed=%zu wall_ns=%" PRIu64
           " elapsed_ns=%" PRIu64 " user_us=%" PRIu64 " sys_us=%" PRIu64 " maxrss_kb=%ld status=%s\n",
           program->name, argv[2], mode_record, completed, total_wall,
           last_end && first_start ? last_end - first_start : 0, total_user, total_sys,
           peak_rss, failed ? "FAIL" : "OK");
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    if (SPEC_PAIRED)
        close(SPEC_RUNTIME_FD);
    return failed ? 1 : 0;
}
