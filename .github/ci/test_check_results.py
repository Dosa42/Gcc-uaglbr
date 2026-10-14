import contextlib
import io
from pathlib import Path
import tempfile
import unittest

from check_results import check, inspect_summary


class ResultChecks(unittest.TestCase):
    def test_unexpected_results_fail(self):
        for status in ("FAIL", "XPASS", "UNRESOLVED", "ERROR"):
            with self.subTest(status=status), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "gcc.sum"
                path.write_text(f"PASS: good.c\n{status}: broken.c\n")
                _, problems = inspect_summary(path)
                self.assertEqual(problems, [f"{status}: broken.c"])

    def test_expected_failures_and_unsupported_are_allowed_with_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gcc.sum"
            path.write_text("PASS: ok.c\nXFAIL: known.c\nUNSUPPORTED: target.c\n")
            self.assertEqual(inspect_summary(path)[1], [])

    def test_empty_or_only_skipped_tests_fail(self):
        for content in ("", "UNSUPPORTED: target.c\n", "XFAIL: known.c\n"):
            with self.subTest(content=content), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "gcc.sum"
                path.write_text(content)
                self.assertTrue(inspect_summary(path)[1])

    def test_both_languages_and_full_runtime_summary_are_required(self):
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()):
            build = Path(directory)
            self.assertEqual(check(build, "selected"), 1)
            for relative in ("gcc/testsuite/gcc/gcc.sum", "gcc/testsuite/g++/g++.sum"):
                path = build / relative
                path.parent.mkdir(parents=True)
                path.write_text("PASS: example\n")
            self.assertEqual(check(build, "selected"), 0)
            self.assertEqual(check(build, "full"), 1)
            runtime = build / "x86_64-pc-linux-gnu/libstdc++-v3/testsuite/libstdc++.sum"
            runtime.parent.mkdir(parents=True)
            runtime.write_text("PASS: runtime\n")
            self.assertEqual(check(build, "full"), 0)


if __name__ == "__main__":
    unittest.main()
