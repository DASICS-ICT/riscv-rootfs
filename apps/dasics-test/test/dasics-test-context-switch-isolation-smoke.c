#ifndef DASICS_LINUX_DUAL_EXEC
#define DASICS_LINUX_DUAL_EXEC 0
#endif

#if DASICS_LINUX_DUAL_EXEC
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#if DASICS_LINUX_DUAL_EXEC
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "ucsr.h"
#include "udasics.h"

#define DASICS_CONTEXT_SWITCH_TAG "\033[1;34m[DASICS-CONTEXT-SWITCH-ISOLATION]\033[0m"
#define DASICS_CONTEXT_SWITCH_BEGIN_TAG "\033[1;31m[DASICS-CONTEXT-SWITCH-ISOLATION]"
#define DASICS_CONTEXT_SWITCH_SUMMARY_TAG "\033[1;32m[DASICS-CONTEXT-SWITCH-ISOLATION]"
#define DASICS_CONTEXT_SWITCH_COLOR_END "\033[0m"

#define SENTINEL_LIB_CFG 0xbUL
#define SENTINEL_LIB_LO 0x12345000UL
#define SENTINEL_LIB_HI 0x12346000UL
#define SENTINEL_JUMP_CFG 0x1UL
#define SENTINEL_JUMP_LO 0x22345000UL
#define SENTINEL_JUMP_HI 0x22346000UL
#define SENTINEL_RETURN_PC 0x33445000UL
#define SENTINEL_FREASON 0x4UL

#if DASICS_LINUX_DUAL_EXEC
#define DASICS_EXEC_CONTRACT_TOTAL 22UL
#define DASICS_CANONICAL_PATH \
    "/dasics/dasics-test-context-switch-isolation-smoke"
#define DASICS_ALIAS_PATH "/dasics/dasics-explicit-optin-alias"
#define DASICS_INVALID_ELF_PATH "/dasics/dasics-invalid-exec"
#define DASICS_STAGE_OFF "--internal-stage=off"
#define DASICS_STAGE_ALIAS_ON "--internal-stage=alias-on"
#define DASICS_STAGE_NONFINAL "--internal-stage=nonfinal"
#define CLONE_STACK_SIZE (16UL * 1024UL)
#endif

static unsigned long read_lib_cfg(void)
{
    return csr_read(0x880);
}

static unsigned long read_lib_bound15_lo(void)
{
    return csr_read(0x8ae);
}

static unsigned long read_lib_bound15_hi(void)
{
    return csr_read(0x8af);
}

static unsigned long read_return_pc(void)
{
    return csr_read(0x8b1);
}

static unsigned long read_freason(void)
{
    return csr_read(0x8b3);
}

static unsigned long read_jump_bound3_lo(void)
{
    return csr_read(0x8c6);
}

static unsigned long read_jump_bound3_hi(void)
{
    return csr_read(0x8c7);
}

static unsigned long read_jump_cfg(void)
{
    return csr_read(0x8c8);
}

static void write_isolation_sentinel(void)
{
    csr_write(0x880, SENTINEL_LIB_CFG);
    csr_write(0x8ae, SENTINEL_LIB_LO);
    csr_write(0x8af, SENTINEL_LIB_HI);
    csr_write(0x8b1, SENTINEL_RETURN_PC);
    csr_write(0x8b3, SENTINEL_FREASON);
    csr_write(0x8c6, SENTINEL_JUMP_LO);
    csr_write(0x8c7, SENTINEL_JUMP_HI);
    csr_write(0x8c8, SENTINEL_JUMP_CFG);
}

static int record_bool_case(const char *case_name, int pass,
                            unsigned long *total)
{
    (*total)++;
    printf(DASICS_CONTEXT_SWITCH_TAG
           " case=%s value=0x%lx expect=0x1 result=%s\n",
           case_name, pass ? 1UL : 0UL, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

static int run_context_switch_checks(void)
{
    unsigned long total = 0;
    int failures = 0;
    int no_sentinel_leak;
    int clean_default_cfg;
    int sentinel_written;

    printf(DASICS_CONTEXT_SWITCH_BEGIN_TAG
           " UCAS OS context switch isolation smoke begin"
           DASICS_CONTEXT_SWITCH_COLOR_END "\n");
    no_sentinel_leak = read_lib_cfg() != SENTINEL_LIB_CFG &&
                       read_lib_bound15_lo() != SENTINEL_LIB_LO &&
                       read_lib_bound15_hi() != SENTINEL_LIB_HI &&
                       read_return_pc() != SENTINEL_RETURN_PC &&
                       read_freason() != SENTINEL_FREASON &&
                       read_jump_bound3_lo() != SENTINEL_JUMP_LO &&
                       read_jump_bound3_hi() != SENTINEL_JUMP_HI &&
                       read_jump_cfg() != SENTINEL_JUMP_CFG;
    clean_default_cfg = read_lib_cfg() == 0 && read_jump_cfg() == 0;
    failures += record_bool_case("CONTEXT-SWITCH-BOUND-ISOLATION-NO-LEAK",
                                  no_sentinel_leak, &total);
    failures += record_bool_case("CONTEXT-EXEC-CLEAR-OLD-STATE",
                                  clean_default_cfg, &total);

    write_isolation_sentinel();
    sentinel_written = read_lib_cfg() == SENTINEL_LIB_CFG &&
                       read_lib_bound15_lo() == SENTINEL_LIB_LO &&
                       read_lib_bound15_hi() == SENTINEL_LIB_HI &&
                       read_return_pc() == SENTINEL_RETURN_PC &&
                       read_freason() == SENTINEL_FREASON &&
                       read_jump_bound3_lo() == SENTINEL_JUMP_LO &&
                       read_jump_bound3_hi() == SENTINEL_JUMP_HI &&
                       read_jump_cfg() == SENTINEL_JUMP_CFG;
    failures += record_bool_case(
        "CONTEXT-EXIT-CLEAR-BOUND-TABLE-SENTINEL-WRITE",
        sentinel_written, &total);
    printf(DASICS_CONTEXT_SWITCH_SUMMARY_TAG
           " summary total=%lu failed=%d result=%s"
           DASICS_CONTEXT_SWITCH_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}

#if DASICS_LINUX_DUAL_EXEC
struct dasics_stable_state {
    unsigned long umain_cfg;
    unsigned long lib_cfg;
    unsigned long lib_bound15_lo;
    unsigned long lib_bound15_hi;
    unsigned long jump_cfg;
    unsigned long jump_bound3_lo;
    unsigned long jump_bound3_hi;
};

static struct dasics_stable_state clone_expected_state;
static unsigned char clone_stack[CLONE_STACK_SIZE]
    __attribute__((aligned(16)));
static int clone_child_result;
static int clone_child_release;
static struct dasics_stable_state thread_expected_state;
static int thread_child_release;
static int thread_child_result;

static void read_stable_state(struct dasics_stable_state *state)
{
    state->umain_cfg = dasics_linux_query_umaincfg();
    state->lib_cfg = read_lib_cfg();
    state->lib_bound15_lo = read_lib_bound15_lo();
    state->lib_bound15_hi = read_lib_bound15_hi();
    state->jump_cfg = read_jump_cfg();
    state->jump_bound3_lo = read_jump_bound3_lo();
    state->jump_bound3_hi = read_jump_bound3_hi();
}

static int stable_state_matches(const struct dasics_stable_state *expected)
{
    struct dasics_stable_state actual;

    read_stable_state(&actual);
    return !memcmp(&actual, expected, sizeof(actual));
}

static int stable_state_is_clear(void)
{
    struct dasics_stable_state state;
    static const struct dasics_stable_state empty;

    read_stable_state(&state);
    return !memcmp(&state, &empty, sizeof(state));
}

static int record_exec_case(const char *case_name, int pass)
{
    dprintf(STDOUT_FILENO,
            "DASICS_EXEC_OPTION_CASE version=1 case=%s result=%s\n",
            case_name, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

static int wait_child(pid_t pid)
{
    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return 0;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int clone_child(void *unused)
{
    int pass;

    (void)unused;
    while (!__atomic_load_n(&clone_child_release, __ATOMIC_ACQUIRE))
        syscall(SYS_sched_yield);
    pass = stable_state_matches(&clone_expected_state);
    clone_child_result = pass;
    record_exec_case("shared-mm-child-inherits-state", pass);
    return pass ? 0 : 1;
}

static void *thread_child(void *unused)
{
    (void)unused;
    while (!__atomic_load_n(&thread_child_release, __ATOMIC_ACQUIRE))
        sched_yield();
    thread_child_result = stable_state_matches(&thread_expected_state);
    return NULL;
}

static int run_failed_exec_checks(
    const struct dasics_stable_state *expected)
{
    char *const envp[] = { "PATH=/dasics", NULL };
    char *const invalid_last_argv[] = {
        (char *)DASICS_CANONICAL_PATH, (char *)1, NULL
    };
    char *const invalid_elf_argv[] = {
        (char *)DASICS_INVALID_ELF_PATH, "-dasics", NULL
    };
    char **invalid_argv = (char **)1;
    char *oversized;
    char *oversized_argv[3];
    long page_size;
    size_t oversized_length;
    long result;
    int failures = 0;
    int saved_errno;

    errno = 0;
    result = syscall(SYS_execve, DASICS_CANONICAL_PATH,
                     invalid_argv, envp);
    saved_errno = errno;
    failures += record_exec_case(
        "invalid-argv-array-efault-preserves-state",
        result == -1 && saved_errno == EFAULT &&
        stable_state_matches(expected));

    errno = 0;
    result = syscall(SYS_execve, DASICS_CANONICAL_PATH,
                     invalid_last_argv, envp);
    saved_errno = errno;
    failures += record_exec_case(
        "invalid-last-string-efault-preserves-state",
        result == -1 && saved_errno == EFAULT &&
        stable_state_matches(expected));

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        page_size = 4096;
    oversized_length = (size_t)page_size * 32;
    oversized = malloc(oversized_length + 1);
    if (oversized) {
        memset(oversized, 'x', oversized_length + 1);
        oversized_argv[0] = (char *)DASICS_CANONICAL_PATH;
        oversized_argv[1] = oversized;
        oversized_argv[2] = NULL;
        errno = 0;
        result = syscall(SYS_execve, DASICS_CANONICAL_PATH,
                         oversized_argv, envp);
        saved_errno = errno;
    } else {
        result = 0;
        saved_errno = 0;
    }
    failures += record_exec_case(
        "oversized-argument-e2big-preserves-state",
        result == -1 && saved_errno == E2BIG &&
        stable_state_matches(expected));
    free(oversized);

    errno = 0;
    result = execve(DASICS_INVALID_ELF_PATH, invalid_elf_argv, envp);
    saved_errno = errno;
    failures += record_exec_case(
        "invalid-elf-enoexec-preserves-state",
        result == -1 && saved_errno == ENOEXEC &&
        stable_state_matches(expected));
    return failures;
}

static int run_fork_checks(const struct dasics_stable_state *expected)
{
    int release_pipe[2];
    char release = 1;
    pid_t pid;
    int failures = 0;
    int child_pass;

    if (pipe(release_pipe)) {
        record_exec_case("fork-parent-immediate-state", 0);
        record_exec_case("fork-child-inherits-state", 0);
        record_exec_case("fork-child-completes", 0);
        record_exec_case("fork-parent-after-switch-state", 0);
        return 4;
    }

    pid = fork();
    if (pid == 0) {
        int pass;

        close(release_pipe[1]);
        pass = read(release_pipe[0], &release, 1) == 1 &&
               stable_state_matches(expected);
        record_exec_case("fork-child-inherits-state", pass);
        close(release_pipe[0]);
        _exit(pass ? 0 : 1);
    }

    close(release_pipe[0]);
    failures += record_exec_case(
        "fork-parent-immediate-state",
        pid > 0 && stable_state_matches(expected));
    if (pid > 0)
        (void)write(release_pipe[1], &release, 1);
    close(release_pipe[1]);
    child_pass = pid > 0 && wait_child(pid);
    failures += record_exec_case("fork-child-completes", child_pass);
    failures += record_exec_case(
        "fork-parent-after-switch-state",
        pid > 0 && stable_state_matches(expected));
    return failures;
}

static int run_shared_mm_checks(
    const struct dasics_stable_state *expected)
{
    pid_t pid;
    int failures = 0;
    int child_pass;

    clone_expected_state = *expected;
    clone_child_result = 0;
    clone_child_release = 0;
    pid = clone(clone_child, clone_stack + sizeof(clone_stack),
                CLONE_VM | SIGCHLD, NULL);
    failures += record_exec_case(
        "shared-mm-parent-immediate-state",
        pid > 0 && stable_state_matches(expected));
    __atomic_store_n(&clone_child_release, 1, __ATOMIC_RELEASE);
    child_pass = pid > 0 && wait_child(pid) && clone_child_result;
    failures += record_exec_case("shared-mm-child-completes", child_pass);
    failures += record_exec_case(
        "shared-mm-parent-after-switch-state",
        pid > 0 && stable_state_matches(expected));
    return failures;
}

static int run_thread_checks(const struct dasics_stable_state *expected)
{
    pthread_t thread;
    int failures = 0;
    int create_result;
    int join_result = -1;

    thread_expected_state = *expected;
    thread_child_release = 0;
    thread_child_result = 0;
    create_result = pthread_create(&thread, NULL, thread_child, NULL);
    failures += record_exec_case(
        "thread-parent-immediate-state",
        create_result == 0 && stable_state_matches(expected));
    __atomic_store_n(&thread_child_release, 1, __ATOMIC_RELEASE);
    if (!create_result)
        join_result = pthread_join(thread, NULL);
    failures += record_exec_case(
        "thread-child-inherits-state",
        create_result == 0 && join_result == 0 && thread_child_result);
    failures += record_exec_case(
        "thread-parent-after-join-state",
        create_result == 0 && join_result == 0 &&
        stable_state_matches(expected));
    return failures;
}

static int run_empty_argv_check(
    const struct dasics_stable_state *expected)
{
    char *const envp[] = { "PATH=/dasics", NULL };
    pid_t pid = fork();
    int child_pass;
    int failures = 0;

    if (pid == 0) {
        syscall(SYS_execve, DASICS_CANONICAL_PATH, NULL, envp);
        _exit(127);
    }
    child_pass = pid > 0 && wait_child(pid);
    failures += record_exec_case("empty-argv-child-completes", child_pass);
    failures += record_exec_case(
        "empty-argv-parent-state",
        pid > 0 && stable_state_matches(expected));
    return failures;
}

static int run_empty_argv_stage(int argc, char *argv[])
{
    int pass = argc == 1 && argv && argv[0] && !argv[0][0] &&
               stable_state_is_clear();

    record_exec_case("empty-argv-normalized-off", pass);
    return pass ? 0 : 1;
}

static int run_initial_on_stage(int argc, char *argv[])
{
    char *const envp[] = { "PATH=/dasics", NULL };
    char *const next_argv[] = {
        (char *)DASICS_CANONICAL_PATH, (char *)DASICS_STAGE_OFF, NULL
    };
    struct dasics_stable_state expected;
    int failures = 0;

    failures += record_exec_case(
        "tail-marker-consumed-alias-on",
        argc == 1 && argv && argv[0] &&
        !strcmp(argv[0], DASICS_ALIAS_PATH) &&
        dasics_linux_query_umaincfg() == DASICS_UCFG_ENA);

    write_isolation_sentinel();
    read_stable_state(&expected);
    failures += run_failed_exec_checks(&expected);
    failures += run_fork_checks(&expected);
    failures += run_shared_mm_checks(&expected);
    failures += run_thread_checks(&expected);
    failures += run_empty_argv_check(&expected);
    if (failures)
        return 1;

    execve(DASICS_CANONICAL_PATH, next_argv, envp);
    record_exec_case("protected-to-ordinary-exec", 0);
    return 1;
}

static int run_off_stage(int argc, char *argv[])
{
    char *const envp[] = { "PATH=/dasics", NULL };
    char *const next_argv[] = {
        (char *)DASICS_ALIAS_PATH, (char *)DASICS_STAGE_ALIAS_ON,
        "-dasics", NULL
    };
    int pass = argc == 2 && argv && argv[0] && argv[1] && !argv[2] &&
               !strcmp(argv[0], DASICS_CANONICAL_PATH) &&
               !strcmp(argv[1], DASICS_STAGE_OFF) &&
               stable_state_is_clear();

    if (record_exec_case("protected-to-ordinary-exec-clears-state", pass))
        return 1;
    execve(DASICS_ALIAS_PATH, next_argv, envp);
    record_exec_case("ordinary-to-alias-optin-exec", 0);
    return 1;
}

static int run_alias_on_stage(int argc, char *argv[])
{
    char *const envp[] = { "PATH=/dasics", NULL };
    char *const next_argv[] = {
        (char *)DASICS_CANONICAL_PATH, "-dasics",
        (char *)DASICS_STAGE_NONFINAL, NULL
    };
    int pass = argc == 2 && argv && argv[0] && argv[1] && !argv[2] &&
               !strcmp(argv[0], DASICS_ALIAS_PATH) &&
               !strcmp(argv[1], DASICS_STAGE_ALIAS_ON) &&
               dasics_linux_query_umaincfg() == DASICS_UCFG_ENA;

    if (record_exec_case("alias-tail-marker-consumed-on", pass))
        return 1;
    execve(DASICS_CANONICAL_PATH, next_argv, envp);
    record_exec_case("alias-optin-to-nonfinal-exec", 0);
    return 1;
}

static int run_nonfinal_stage(int argc, char *argv[])
{
    int pass = argc == 3 && argv && argv[0] && argv[1] && argv[2] &&
               !argv[3] && !strcmp(argv[0], DASICS_CANONICAL_PATH) &&
               !strcmp(argv[1], "-dasics") &&
               !strcmp(argv[2], DASICS_STAGE_NONFINAL) &&
               stable_state_is_clear();

    if (record_exec_case("nonfinal-marker-preserved-off", pass))
        return 1;
    dprintf(STDOUT_FILENO,
            "DASICS_EXEC_OPTION_CONTRACT version=1 total=%lu "
            "failed=0 result=PASS\n",
            DASICS_EXEC_CONTRACT_TOTAL);
    return run_context_switch_checks();
}
#endif

int main(int argc, char *argv[])
{
#if DASICS_LINUX_DUAL_EXEC
    if (argc == 1 && argv && argv[0] && !argv[0][0])
        return run_empty_argv_stage(argc, argv);
    if (argc == 1)
        return run_initial_on_stage(argc, argv);
    if (argc == 2 && argv && argv[1] &&
        !strcmp(argv[1], DASICS_STAGE_OFF))
        return run_off_stage(argc, argv);
    if (argc == 2 && argv && argv[1] &&
        !strcmp(argv[1], DASICS_STAGE_ALIAS_ON))
        return run_alias_on_stage(argc, argv);
    if (argc == 3 && argv && argv[2] &&
        !strcmp(argv[2], DASICS_STAGE_NONFINAL))
        return run_nonfinal_stage(argc, argv);

    fprintf(stderr, DASICS_CONTEXT_SWITCH_TAG
            " argument-contract argc=%d result=FAIL\n", argc);
    return 2;
#else
    (void)argc;
    (void)argv;
    return run_context_switch_checks();
#endif
}
