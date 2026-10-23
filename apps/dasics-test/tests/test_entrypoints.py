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
        self.assertEqual(len(names), 14)
        self.assertEqual(len(set(names)), 14)
        self.assertNotIn("dasics-test-csr-smoke", names)
        self.assertIn("dasics-test-csr-unified", names)
        self.assertIn("dasics-test-validation-smoke", names)
        self.assertIn("dasics-test-utimer", names)
        template = (ROOT / "rootfsimg/initramfs-opensbi-linux-dasics.txt").read_text()
        packaged = re.findall(r"^file /dasics/(dasics-test-\S+) ", template, re.M)
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
        self.assertIn("-DDASICS_S_TRAP_ONLY", result.stdout)

    def run_all(self, user_status, kernel_status):
        with tempfile.TemporaryDirectory(prefix="dasics-runner-check-") as directory:
            temporary = Path(directory)
            invocations = temporary / "calls"
            for name, status in (("suite", user_status), ("kernel-runner", kernel_status)):
                executable = temporary / name
                executable.write_text(
                    "#!/bin/sh\nprintf '%s\\n' '" + name + "' >> '" + str(invocations) +
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
        self.assertEqual(calls, ["suite", "kernel-runner"])
        self.assertIn("DASICS_ALL_CLEANUP=PASS", result.stdout)
        self.assertIn("DASICS_ALL_RESULT=PASS", result.stdout)

    def test_user_failure_does_not_run_kernel_tests(self):
        result, calls = self.run_all(3, 0)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, ["suite"])
        self.assertNotIn("DASICS_ALL_RESULT=PASS", result.stdout)

    def test_kernel_failure_is_not_masked(self):
        result, calls = self.run_all(0, 7)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(calls, ["suite", "kernel-runner"])
        self.assertNotIn("DASICS_ALL_CLEANUP=PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
