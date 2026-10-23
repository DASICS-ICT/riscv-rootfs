#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

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

_Static_assert(ARRAY_SIZE(tests) == 27, "unexpected unified invocation count");

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

int main(void)
{
    unsigned long failures = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("DASICS_LINUX_SUITE_BEGIN version=3 unique_total=14 invocations=27\n");
    for (unsigned long i = 0; i < ARRAY_SIZE(tests); i++)
        failures += run_test(&tests[i], i + 1);
    printf("DASICS_LINUX_SUITE_SUMMARY version=3 unique_total=14 "
           "invocations=%lu failed=%lu result=%s\n", (unsigned long)ARRAY_SIZE(tests),
           failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
