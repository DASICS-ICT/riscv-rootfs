#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#define DASICS_DIRECTED_TESTS(X) \
	X("dasics_test_csr_smoke", \
	  "/dasics/dasics-test-csr-smoke", 1) \
	X("dasics_test_csr_mask_ops_smoke", \
	  "/dasics/dasics-test-csr-mask-ops-smoke", 1) \
	X("dasics_test_call_jr_returnpc_smoke", \
	  "/dasics/dasics-test-call-jr-returnpc-smoke", 1) \
	X("dasics_test_maincfg_toggle_smoke", \
	  "/dasics/dasics-test-maincfg-toggle-smoke", 1) \
	X("dasics_test_jump_branch_target_permission_smoke", \
	  "/dasics/dasics-test-jump-branch-target-permission-smoke", 1) \
	X("dasics_test_load_store_permission_smoke", \
	  "/dasics/dasics-test-load-store-permission-smoke", 1) \
	X("dasics_test_ecall_fault_smoke", \
	  "/dasics/dasics-test-ecall-fault-smoke", 1) \
	X("dasics_test_ecall_close_smoke", \
	  "/dasics/dasics-test-ecall-close-smoke", 1) \
	X("dasics_test_syscall_buffer_permission_smoke", \
	  "/dasics/dasics-test-syscall-buffer-permission-smoke", 1) \
	X("dasics_test_dynamic_bound_free_smoke", \
	  "/dasics/dasics-test-dynamic-bound-free-smoke", 1) \
	X("dasics_test_dynamic_bound_replacement_smoke", \
	  "/dasics/dasics-test-dynamic-bound-replacement-smoke", 1) \
	X("dasics_test_context_section_load_smoke", \
	  "/dasics/dasics-test-context-section-load-smoke", 1) \
	X("dasics_test_context_switch_isolation_smoke", \
	  "/dasics/dasics-explicit-optin-alias", 2) \
	X("dasics_test_returnpc_trap_preserve_smoke", \
	  "/dasics/dasics-test-returnpc-trap-preserve-smoke", 1)

#define DASICS_COMPLETE_TESTS(X) \
	X("dasics_test_rwx", "/dasics/dasics-test-rwx") \
	X("dasics_test_jump", "/dasics/dasics-test-jump") \
	X("dasics_test_ofb", "/dasics/dasics-test-ofb") \
	X("dasics_test_free", "/dasics/dasics-test-free") \
	X("dasics_test_syscall", "/dasics/dasics-test-syscall")

struct dasics_directed_test {
	const char *name;
	const char *path;
	unsigned int repetitions;
};

struct dasics_complete_test {
	const char *name;
	const char *path;
};

#define DIRECTED_ENTRY(name, path, repetitions) { name, path, repetitions },
static const struct dasics_directed_test dasics_directed_tests[] = {
	DASICS_DIRECTED_TESTS(DIRECTED_ENTRY)
};
#undef DIRECTED_ENTRY

#define COMPLETE_ENTRY(name, path) { name, path },
static const struct dasics_complete_test dasics_complete_tests[] = {
	DASICS_COMPLETE_TESTS(COMPLETE_ENTRY)
};
#undef COMPLETE_ENTRY

#define COUNT_DIRECTED_UNIQUE(name, path, repetitions) + 1
#define COUNT_DIRECTED_INVOCATIONS(name, path, repetitions) + repetitions
#define COUNT_COMPLETE_UNIQUE(name, path) + 1
enum {
	DASICS_DIRECTED_UNIQUE_TOTAL =
		0 DASICS_DIRECTED_TESTS(COUNT_DIRECTED_UNIQUE),
	DASICS_DIRECTED_INVOCATION_TOTAL =
		0 DASICS_DIRECTED_TESTS(COUNT_DIRECTED_INVOCATIONS),
	DASICS_COMPLETE_UNIQUE_TOTAL =
		0 DASICS_COMPLETE_TESTS(COUNT_COMPLETE_UNIQUE),
	DASICS_UNIQUE_TOTAL = DASICS_DIRECTED_UNIQUE_TOTAL +
		DASICS_COMPLETE_UNIQUE_TOTAL,
	DASICS_INVOCATION_TOTAL = DASICS_DIRECTED_INVOCATION_TOTAL +
		2 * DASICS_COMPLETE_UNIQUE_TOTAL,
};
#undef COUNT_DIRECTED_UNIQUE
#undef COUNT_DIRECTED_INVOCATIONS
#undef COUNT_COMPLETE_UNIQUE

_Static_assert(DASICS_UNIQUE_TOTAL == 19, "unexpected DASICS program count");
_Static_assert(DASICS_INVOCATION_TOTAL == 25,
	       "unexpected DASICS invocation count");

static int run_test(const char *name, const char *path, const char *expect,
		    unsigned long invocation)
{
	const char *dasics;
	char *argv[4] = { (char *)path, NULL, NULL, NULL };
	char *const envp[] = { "PATH=/dasics", NULL };
	pid_t pid;
	int status;

	if (!expect) {
		dasics = "on";
		argv[1] = "-dasics";
	} else if (!strcmp(expect, "--expect=off")) {
		dasics = "off";
		argv[1] = (char *)expect;
	} else if (!strcmp(expect, "--expect=on")) {
		dasics = "on";
		argv[1] = (char *)expect;
		argv[2] = "-dasics";
	} else {
		printf("DASICS_LINUX_RUNNER_CONFIG name=%s invocation=%lu "
		       "detail=invalid-expect result=FAIL\n",
		       name, invocation);
		return 1;
	}

	printf("DASICS_LINUX_CASE_BEGIN name=%s invocation=%lu dasics=%s\n",
	       name, invocation, dasics);
	pid = fork();
	if (pid < 0) {
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "dasics=%s status=fork-error errno=%d result=FAIL\n",
		       name, invocation, dasics, errno);
		return 1;
	}
	if (!pid) {
		execve(path, argv, envp);
		dprintf(STDERR_FILENO,
			"DASICS_LINUX_EXEC name=%s dasics=%s errno=%d result=FAIL\n",
			name, dasics, errno);
		_exit(127);
	}

	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR) {
			printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
			       "dasics=%s status=wait-error errno=%d result=FAIL\n",
			       name, invocation, dasics, errno);
			return 1;
		}
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "dasics=%s status=0 result=PASS\n",
		       name, invocation, dasics);
		return 0;
	}

	if (WIFEXITED(status))
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "dasics=%s status=%d result=FAIL\n",
		       name, invocation, dasics, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "dasics=%s signal=%d result=FAIL\n",
		       name, invocation, dasics, WTERMSIG(status));
	else
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "dasics=%s status=unknown result=FAIL\n",
		       name, invocation, dasics);
	return 1;
}

int main(void)
{
	unsigned long invocation = 0;
	unsigned long failures = 0;
	unsigned long i;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	printf("DASICS_LINUX_SUITE_BEGIN version=2 unique_total=%u "
	       "invocations=%u\n", DASICS_UNIQUE_TOTAL,
	       DASICS_INVOCATION_TOTAL);

	for (i = 0; i < ARRAY_SIZE(dasics_directed_tests); i++) {
		unsigned int repeat;

		for (repeat = 0;
		     repeat < dasics_directed_tests[i].repetitions; repeat++) {
			invocation++;
			failures += run_test(dasics_directed_tests[i].name,
					     dasics_directed_tests[i].path,
					     NULL, invocation);
		}
	}

	for (i = 0; i < ARRAY_SIZE(dasics_complete_tests); i++) {
		invocation++;
		failures += run_test(dasics_complete_tests[i].name,
				     dasics_complete_tests[i].path,
				     "--expect=off", invocation);
		invocation++;
		failures += run_test(dasics_complete_tests[i].name,
				     dasics_complete_tests[i].path,
				     "--expect=on", invocation);
	}

	printf("DASICS_LINUX_SUITE_SUMMARY version=2 unique_total=%u "
	       "invocations=%lu failed=%lu result=%s\n",
	       DASICS_UNIQUE_TOTAL, invocation, failures,
	       failures ? "FAIL" : "PASS");
	sync();
	reboot(RB_POWER_OFF);
	for (;;)
		pause();
}
