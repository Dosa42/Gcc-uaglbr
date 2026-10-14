import os
from pathlib import Path
import subprocess
import unittest


class BuildOptions(unittest.TestCase):
    def run_validation(self, **options):
        environment = os.environ.copy()
        for name in ("GCC_BUILD_MODE", "GCC_TEST_SCOPE", "GCC_C_TESTS", "GCC_CXX_TESTS",
                     "GCC_JOBS", "GCC_TEST_JOBS"):
            environment.pop(name, None)
        environment.update(options)
        return subprocess.run(
            ["bash", str(Path(__file__).with_name("build-gcc.sh")), "validate"],
            env=environment, capture_output=True, text=True, check=False,
        ).returncode

    def test_default_and_targeted_selectors(self):
        self.assertEqual(self.run_validation(), 0)
        self.assertEqual(self.run_validation(
            GCC_BUILD_MODE="bootstrap", GCC_TEST_SCOPE="full",
            GCC_C_TESTS="dg.exp=pr12345*.c", GCC_CXX_TESTS="dg.exp=template/foo?.C",
        ), 0)

    def test_shell_make_and_multiple_selectors_are_rejected(self):
        for selector in ("dg.exp=$(id)", "dg.exp=`id`", "dg.exp=one.c;id",
                         "dg.exp=one.c dg.exp=two.c", "dg.exp=one.c\n", "dg.exp="):
            with self.subTest(selector=selector):
                self.assertNotEqual(self.run_validation(GCC_C_TESTS=selector), 0)

    def test_invalid_mode_scope_and_job_count(self):
        for options in ({"GCC_BUILD_MODE": "typo"}, {"GCC_TEST_SCOPE": "typo"},
                        {"GCC_JOBS": "0"}, {"GCC_TEST_JOBS": "-1"}):
            with self.subTest(options=options):
                self.assertNotEqual(self.run_validation(**options), 0)


if __name__ == "__main__":
    unittest.main()
