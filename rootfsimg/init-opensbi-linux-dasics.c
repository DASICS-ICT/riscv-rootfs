#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct dasics_test_invocation {
	const char *name;
	const char *path;
};

static const struct dasics_test_invocation dasics_test_invocations[] = {
	{ "dasics_test_csr_smoke", "/dasics/dasics-test-csr-smoke" },
	{ "dasics_test_csr_mask_ops_smoke",
	  "/dasics/dasics-test-csr-mask-ops-smoke" },
	{ "dasics_test_call_jr_returnpc_smoke",
	  "/dasics/dasics-test-call-jr-returnpc-smoke" },
	{ "dasics_test_maincfg_toggle_smoke",
	  "/dasics/dasics-test-maincfg-toggle-smoke" },
	{ "dasics_test_jump_branch_target_permission_smoke",
	  "/dasics/dasics-test-jump-branch-target-permission-smoke" },
	{ "dasics_test_load_store_permission_smoke",
	  "/dasics/dasics-test-load-store-permission-smoke" },
	{ "dasics_test_ecall_fault_smoke",
	  "/dasics/dasics-test-ecall-fault-smoke" },
	{ "dasics_test_ecall_close_smoke",
	  "/dasics/dasics-test-ecall-close-smoke" },
	{ "dasics_test_syscall_buffer_permission_smoke",
	  "/dasics/dasics-test-syscall-buffer-permission-smoke" },
	{ "dasics_test_dynamic_bound_free_smoke",
	  "/dasics/dasics-test-dynamic-bound-free-smoke" },
	{ "dasics_test_dynamic_bound_replacement_smoke",
	  "/dasics/dasics-test-dynamic-bound-replacement-smoke" },
	{ "dasics_test_context_section_load_smoke",
	  "/dasics/dasics-test-context-section-load-smoke" },
	{ "dasics_test_context_switch_isolation_smoke",
	  "/dasics/dasics-test-context-switch-isolation-smoke" },
	{ "dasics_test_context_switch_isolation_smoke",
	  "/dasics/dasics-test-context-switch-isolation-smoke" },
	{ "dasics_test_returnpc_trap_preserve_smoke",
	  "/dasics/dasics-test-returnpc-trap-preserve-smoke" },
	{ "dasics_test_rwx", "/dasics/dasics-test-rwx" },
	{ "dasics_test_jump", "/dasics/dasics-test-jump" },
	{ "dasics_test_ofb", "/dasics/dasics-test-ofb" },
	{ "dasics_test_free", "/dasics/dasics-test-free" },
	{ "dasics_test_syscall", "/dasics/dasics-test-syscall" },
};

static int run_test(const struct dasics_test_invocation *test,
		    unsigned long invocation)
{
	char *const argv[] = { (char *)test->path, NULL };
	char *const envp[] = { "PATH=/dasics", NULL };
	pid_t pid;
	int status;

	printf("DASICS_LINUX_CASE_BEGIN name=%s invocation=%lu\n",
	       test->name, invocation);
	pid = fork();
	if (pid < 0) {
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "status=fork-error errno=%d result=FAIL\n",
		       test->name, invocation, errno);
		return 1;
	}
	if (!pid) {
		execve(test->path, argv, envp);
		dprintf(STDERR_FILENO,
			"DASICS_LINUX_EXEC name=%s errno=%d result=FAIL\n",
			test->name, errno);
		_exit(127);
	}

	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR) {
			printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
			       "status=wait-error errno=%d result=FAIL\n",
			       test->name, invocation, errno);
			return 1;
		}
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "status=0 result=PASS\n",
		       test->name, invocation);
		return 0;
	}

	if (WIFEXITED(status))
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "status=%d result=FAIL\n",
		       test->name, invocation, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "signal=%d result=FAIL\n",
		       test->name, invocation, WTERMSIG(status));
	else
		printf("DASICS_LINUX_CASE_END name=%s invocation=%lu "
		       "status=unknown result=FAIL\n",
		       test->name, invocation);
	return 1;
}

int main(void)
{
	unsigned long failures = 0;
	unsigned long i;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	printf("DASICS_LINUX_SUITE_BEGIN version=1 unique_total=19 "
	       "invocations=20\n");

	for (i = 0; i < sizeof(dasics_test_invocations) /
			    sizeof(dasics_test_invocations[0]); i++)
		failures += run_test(&dasics_test_invocations[i], i + 1);

	printf("DASICS_LINUX_SUITE_SUMMARY version=1 unique_total=19 "
	       "invocations=20 failed=%lu result=%s\n",
	       failures, failures ? "FAIL" : "PASS");
	sync();
	reboot(RB_POWER_OFF);
	for (;;)
		pause();
}
