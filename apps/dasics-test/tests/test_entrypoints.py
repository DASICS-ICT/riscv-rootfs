"""Check suite selection and exit propagation without executing target code."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]


class EntryPoints(unittest.TestCase):
    def test_linux_builds_each_canonical_program_once(self):
        with tempfile.TemporaryDirectory(prefix="dasics-make-check-") as directory:
            result = subprocess.run(
                ["make", "-n", "opensbi-linux-dasics", "DASICS_LINUX_BUILD_DIR=" + directory],
                cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                check=True,
            )
        names = []
        for line in result.stdout.splitlines():
            if "gcc " in line and " -c " in line and "/test/" in line:
                names.append(re.search(r"/test/([^ ]+)\.c", line).group(1))
        self.assertEqual(len(names), 20)
        self.assertEqual(len(set(names)), 20)
        self.assertNotIn("dasics-test-csr-smoke", names)
        self.assertIn("dasics-test-csr-unified", names)
        self.assertIn("dasics-test-validation-smoke", names)
        self.assertIn("dasics-test-utimer", names)
        demos = {"fdi-test-" + name for name in
                 ("ofb", "jump", "cfi", "maincall", "finegrain", "flexibility")}
        self.assertEqual({name for name in names if name.startswith("fdi-test-")}, demos)
        template = (ROOT / "rootfsimg/initramfs-opensbi-linux-dasics.txt").read_text()
        packaged = re.findall(r"^file /dasics/((?:dasics|fdi)-test-\S+) ", template, re.M)
        self.assertEqual(set(packaged), set(names))

    def test_s_only_keeps_its_privilege_profile(self):
        with tempfile.TemporaryDirectory(prefix="dasics-make-check-") as directory:
            result = subprocess.run(
                ["make", "-n", "-C", "apps/dasics-test", "build-only",
                 "RISCV_ROOTFS_HOME=" + str(ROOT), "DST_DIR=" + directory,
                 "DASICS_S_TRAP_ONLY=1", "DASICS_N_EXTENSION_PROFILE=0"],
                cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                check=True,
            )
        self.assertIn("/test/dasics-test-csr-smoke.c", result.stdout)
        self.assertNotIn("/test/dasics-test-csr-unified.c", result.stdout)
        self.assertNotIn("/test/fdi-test-", result.stdout)
        self.assertIn("-DDASICS_S_TRAP_ONLY", result.stdout)

    def run_user_suite(self, *arguments, fail_demo=False):
        with tempfile.TemporaryDirectory(prefix="dasics-suite-check-") as directory:
            temporary = Path(directory)
            source = temporary / "suite.c"
            source.write_text(
                '#define main guest_main\n#define execve record_execve\n' +
                '#include "' + str(ROOT / "rootfsimg/init-opensbi-linux-dasics.c") + '"\n' +
                '#undef main\n#undef execve\n' +
                'int record_execve(const char *path, char *const argv[], char *const envp[]) {\n' +
                '    (void)envp;\n' +
                '    printf("GUEST_ARGV");\n' +
                '    for (unsigned int i = 0; argv[i]; i++) printf("\\t%s", argv[i]);\n' +
                '    printf("\\n");\n' +
                '    _exit(' + ('!strcmp(path, "/dasics/fdi-test-flexibility") ? 7 : 0'
                                if fail_demo else '((void)path, 0)') + ');\n}\n' +
                'int main(int argc, char *argv[]) { return guest_main(argc, argv); }\n'
            )
            binary = temporary / "suite"
            subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-o", binary, source], check=True,
                           text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            return subprocess.run([binary, *arguments], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def test_guest_suite_covers_each_demo_mode_and_all_functions(self):
        result = self.run_user_suite("--all")
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = [line.split("\t")[1:] for line in result.stdout.splitlines()
                 if line.startswith("GUEST_ARGV\t")]
        self.assertEqual(len(calls), 237)
        self.assertEqual(len({args[0] for args in calls}), 20)
        expected = []
        for name in ("ofb", "jump", "cfi", "maincall", "finegrain"):
            for mode in ("off", "on"):
                expected.append(["/dasics/fdi-test-" + name, "--expect=" + mode] +
                                (["-dasics"] if mode == "on" else []))
        for function in range(100):
            for mode in ("off", "on"):
                expected.append(["/dasics/fdi-test-flexibility", "--expect=" + mode,
                                 "--function=" + str(function)] +
                                (["-dasics"] if mode == "on" else []))
        self.assertEqual(calls[27:], expected)
        self.assertIn("unique_total=20 invocations=237 failed=0 result=PASS", result.stdout)

    def test_guest_suite_demo_failure_propagates(self):
        result = self.run_user_suite("--all", fail_demo=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("invocations=237 failed=200 result=FAIL", result.stdout)
        self.assertNotIn("failed=0 result=PASS", result.stdout)

    def test_guest_suite_preserves_default_and_selects_demos(self):
        result = self.run_user_suite()
        self.assertEqual(result.returncode, 0)
        self.assertIn("version=3 unique_total=14 invocations=27 failed=0 result=PASS", result.stdout)
        self.assertNotIn("GUEST_ARGV\t/dasics/fdi-test-", result.stdout)
        result = self.run_user_suite("--demos")
        self.assertEqual(result.returncode, 0)
        self.assertIn("unique_total=6 invocations=210 failed=0 result=PASS", result.stdout)
        self.assertNotIn("GUEST_ARGV\t/dasics/dasics-test-", result.stdout)

    def test_guest_suite_rejects_invalid_selection_before_execution(self):
        for arguments in (("--unknown",), ("--all", "--demos")):
            with self.subTest(arguments=arguments):
                result = self.run_user_suite(*arguments)
                self.assertEqual(result.returncode, 2)
                self.assertNotIn("GUEST_ARGV", result.stdout)

    def run_all(self, user_status, kernel_status):
        with tempfile.TemporaryDirectory(prefix="dasics-runner-check-") as directory:
            temporary = Path(directory)
            invocations = temporary / "calls"
            for name, status in (("suite", user_status), ("kernel-runner", kernel_status)):
                executable = temporary / name
                executable.write_text(
                    "#!/bin/sh\nprintf '%s\\n' \"" + name + " $*\" >> '" + str(invocations) +
                    "'\nexit " + str(status) + "\n"
                )
                executable.chmod(0o755)
            script = (ROOT / "rootfsimg/dasics-run-all.sh").read_text()
            result = subprocess.run(
                ["sh", "-c", script.replace("/dasics/", str(temporary) + "/")],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            )
            return result, invocations.read_text().splitlines()

    def test_automatic_runner_delegates_once(self):
        result, calls = self.run_all(0, 0)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(calls, ["kernel-runner all --rounds=2", "suite --all"])
        self.assertIn("DASICS_ALL_CLEANUP=PASS", result.stdout)
        self.assertIn("DASICS_ALL_RESULT=PASS", result.stdout)

    def test_user_failure_is_not_masked(self):
        result, calls = self.run_all(3, 0)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, ["kernel-runner all --rounds=2", "suite --all"])
        self.assertNotIn("DASICS_ALL_RESULT=PASS", result.stdout)

    def test_kernel_failure_is_not_masked(self):
        result, calls = self.run_all(0, 7)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(calls, ["kernel-runner all --rounds=2"])
        self.assertNotIn("DASICS_ALL_CLEANUP=PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
