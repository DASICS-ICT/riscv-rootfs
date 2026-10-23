#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define FLEXIBILITY_FUNCTIONS 100

struct dasics_test {
    const char *name;
    const char *mode;
    const char *argv[5];
};

/* Each mode uses the same ELF as its single-case invocation. */
static const struct dasics_test tests[] = {
    { "validation", "on", { "/dasics/dasics-test-validation-smoke", "-dasics", NULL } },
    { "csr-unified", "on", { "/dasics/dasics-test-csr-unified", "-dasics", NULL } },
    { "fdicall-unified", "on", { "/dasics/dasics-test-fdicall-unified", "-dasics", NULL } },
    { "control-direct", "on", { "/dasics/dasics-test-control-flow-unified", "--scenario=direct", "-dasics", NULL } },
    { "control-special", "on", { "/dasics/dasics-test-control-flow-unified", "--scenario=special", "-dasics", NULL } },
    { "control-library-off", "off", { "/dasics/dasics-test-control-flow-unified", "--scenario=library", "--expect=off", NULL } },
    { "control-library-on", "on", { "/dasics/dasics-test-control-flow-unified", "--scenario=library", "--expect=on", "-dasics", NULL } },
    { "control-sections", "on", { "/dasics/dasics-test-control-flow-unified", "--scenario=sections", "-dasics", NULL } },
    { "memory-isa-8", "on", { "/dasics/dasics-test-memory-isa", "--width=8", "--scenario=matrix", "-dasics", NULL } },
    { "memory-isa-16", "on", { "/dasics/dasics-test-memory-isa", "--width=16", "--scenario=matrix", "-dasics", NULL } },
    { "memory-isa-32", "on", { "/dasics/dasics-test-memory-isa", "--width=32", "--scenario=matrix", "-dasics", NULL } },
    { "memory-isa-64", "on", { "/dasics/dasics-test-memory-isa", "--width=64", "--scenario=matrix", "-dasics", NULL } },
    { "memory-isa-slots", "on", { "/dasics/dasics-test-memory-isa", "--width=64", "--scenario=slots", "-dasics", NULL } },
    { "memory-library-off", "off", { "/dasics/dasics-test-memory-library", "--expect=off", NULL } },
    { "memory-library-on", "on", { "/dasics/dasics-test-memory-library", "--expect=on", "-dasics", NULL } },
    { "dynamic-direct", "on", { "/dasics/dasics-test-dynamic-unified", "--direct", "-dasics", NULL } },
    { "dynamic-library-off", "off", { "/dasics/dasics-test-dynamic-unified", "--library-off", NULL } },
    { "dynamic-library-on", "on", { "/dasics/dasics-test-dynamic-unified", "--library-on", "-dasics", NULL } },
    { "ecall-unified", "on", { "/dasics/dasics-test-ecall-unified", "-dasics", NULL } },
    { "maincfg-unified", "on", { "/dasics/dasics-test-maincfg-toggle-smoke", "--isa-extended", "-dasics", NULL } },
    { "context-switch-unified", "on", { "/dasics/dasics-explicit-optin-alias", "-dasics", NULL } },
    { "lifecycle", "on", { "/dasics/dasics-test-lifecycle", "-dasics", NULL } },
    { "utimer-unified-off", "off", { "/dasics/dasics-test-utimer", "--expect=off", "--isa-extended", NULL } },
    { "utimer-unified-on", "on", { "/dasics/dasics-test-utimer", "--expect=on", "--isa-extended", "-dasics", NULL } },
    { "amo32", "on", { "/dasics/dasics-test-amo-permission-smoke", "--width=32", "-dasics", NULL } },
    { "amo64", "on", { "/dasics/dasics-test-amo-permission-smoke", "--width=64", "-dasics", NULL } },
    { "cbo", "on", { "/dasics/dasics-test-cbo-permission-smoke", "--isa-extended", "-dasics", NULL } },
};

static const struct dasics_test demo_tests[] = {
    { "demo-ofb-off", "off", { "/dasics/fdi-test-ofb", "--expect=off", NULL } },
    { "demo-ofb-on", "on", { "/dasics/fdi-test-ofb", "--expect=on", "-dasics", NULL } },
    { "demo-jump-off", "off", { "/dasics/fdi-test-jump", "--expect=off", NULL } },
    { "demo-jump-on", "on", { "/dasics/fdi-test-jump", "--expect=on", "-dasics", NULL } },
    { "demo-cfi-off", "off", { "/dasics/fdi-test-cfi", "--expect=off", NULL } },
    { "demo-cfi-on", "on", { "/dasics/fdi-test-cfi", "--expect=on", "-dasics", NULL } },
    { "demo-maincall-off", "off", { "/dasics/fdi-test-maincall", "--expect=off", NULL } },
    { "demo-maincall-on", "on", { "/dasics/fdi-test-maincall", "--expect=on", "-dasics", NULL } },
    { "demo-finegrain-off", "off", { "/dasics/fdi-test-finegrain", "--expect=off", NULL } },
    { "demo-finegrain-on", "on", { "/dasics/fdi-test-finegrain", "--expect=on", "-dasics", NULL } },
};

static const char *const demo_modes[] = { "off", "on" };

static unsigned long unique_programs(const struct dasics_test *cases, unsigned long count)
{
    unsigned long total = 0;
    for (unsigned long i = 0; i < count; i++) {
        unsigned long j;
        for (j = 0; j < i; j++)
            if (!strcmp(cases[i].argv[0], cases[j].argv[0]))
                break;
        if (j == i)
            total++;
    }
    return total;
}

static int run_test(const struct dasics_test *test, unsigned long invocation)
{
    char *const environment[] = { "PATH=/bin:/sbin:/usr/bin:/dasics", NULL };
    pid_t pid;
    int status;

    printf("DASICS_LINUX_CASE_BEGIN name=%s invocation=%lu dasics=%s\n",
           test->name, invocation, test->mode);
    pid = fork();
    if (pid < 0) {
        printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
               "status=fork-error errno=%d result=FAIL\n", test->name, invocation, errno);
        return 1;
    }
    if (!pid) {
        execve(test->argv[0], (char *const *)test->argv, environment);
        dprintf(STDERR_FILENO, "DASICS_LINUX_EXEC name=%s errno=%d result=FAIL\n",
                test->name, errno);
        _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
                   "status=wait-error errno=%d result=FAIL\n", test->name, invocation, errno);
            return 1;
        }
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        printf("DASICS_LINUX_CASE_END name=%s invocation=%lu dasics=%s "
               "status=%d result=%s\n", test->name, invocation, test->mode,
               code, code ? "FAIL" : "PASS");
        return code != 0;
    }
    printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
           "signal=%d result=FAIL\n", test->name, invocation,
           WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    return 1;
}

int main(int argc, char *argv[])
{
    unsigned long failures = 0;
    unsigned long invocation = 0;
    unsigned long total = 0, programs = 0;
    int with_basic = 1, with_demos = 0;
    if (argc == 2 && !strcmp(argv[1], "--all")) {
        with_demos = 1;
    } else if (argc == 2 && !strcmp(argv[1], "--demos")) {
        with_basic = 0;
        with_demos = 1;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--all|--demos]\n", argv[0]);
        return 2;
    }
    if (with_basic) {
        total += ARRAY_SIZE(tests);
        programs += unique_programs(tests, ARRAY_SIZE(tests));
    }
    if (with_demos) {
        total += ARRAY_SIZE(demo_tests) + FLEXIBILITY_FUNCTIONS * ARRAY_SIZE(demo_modes);
        programs += unique_programs(demo_tests, ARRAY_SIZE(demo_tests)) + 1;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("DASICS_LINUX_SUITE_BEGIN version=%u unique_total=%lu invocations=%lu\n",
           with_demos ? 4 : 3, programs, total);
    if (with_basic)
        for (unsigned long i = 0; i < ARRAY_SIZE(tests); i++)
            failures += run_test(&tests[i], ++invocation);
    if (with_demos) {
        for (unsigned long i = 0; i < ARRAY_SIZE(demo_tests); i++)
            failures += run_test(&demo_tests[i], ++invocation);
        for (unsigned long function = 0; function < FLEXIBILITY_FUNCTIONS; function++) {
            for (unsigned long mode = 0; mode < ARRAY_SIZE(demo_modes); mode++) {
                char name[64], expectation[16], selection[32];
                snprintf(name, sizeof(name), "demo-flexibility-%lu-%s", function, demo_modes[mode]);
                snprintf(expectation, sizeof(expectation), "--expect=%s", demo_modes[mode]);
                snprintf(selection, sizeof(selection), "--function=%lu", function);
                /* A new process owns each function's bounds and exception records. */
                const struct dasics_test test = {
                    name, demo_modes[mode], { "/dasics/fdi-test-flexibility", expectation,
                        selection, mode ? "-dasics" : NULL, NULL }
                };
                failures += run_test(&test, ++invocation);
            }
        }
    }
    printf("DASICS_LINUX_SUITE_SUMMARY version=%u unique_total=%lu "
           "invocations=%lu failed=%lu result=%s\n", with_demos ? 4 : 3, programs, invocation,
           failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
