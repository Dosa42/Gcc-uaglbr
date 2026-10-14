#!/usr/bin/env python3
"""Fail CI on missing test coverage, harness errors or unexpected outcomes."""

import argparse
from collections import Counter
from pathlib import Path
import re

RESULT = re.compile(r"^(PASS|FAIL|XPASS|XFAIL|UNRESOLVED|UNSUPPORTED|UNTESTED|ERROR):")
BAD = {"FAIL", "XPASS", "UNRESOLVED", "ERROR"}


def inspect_summary(path):
    counts = Counter()
    problems = []
    for line in path.read_text(errors="replace").splitlines():
        match = RESULT.match(line)
        if match:
            status = match.group(1)
            counts[status] += 1
            if status in BAD:
                problems.append(line)
    if not counts["PASS"]:
        problems.append("No PASS results: selector may match no tests or all tests were skipped.")
    return counts, problems


def check(build, scope):
    suites = [build / "gcc/testsuite/gcc/gcc.sum", build / "gcc/testsuite/g++/g++.sum"]
    missing = [str(path) for path in suites if not path.is_file()]
    if scope == "full":
        runtime = list(build.glob("*/libstdc++-v3/testsuite/libstdc++.sum"))
        if not runtime:
            missing.append("libstdc++-v3/testsuite/libstdc++.sum")
        suites.extend(runtime)
    if missing:
        for name in missing:
            print(f"Missing required summary: {name}")
        return 1
    failed = False
    for path in suites:
        counts, problems = inspect_summary(path)
        print(f"{path.relative_to(build)}: {dict(sorted(counts.items()))}")
        for problem in problems[:100]:
            print(f"  {problem}")
        if len(problems) > 100:
            print(f"  ... {len(problems) - 100} more; see the uploaded .sum file")
        failed |= bool(problems)
    return int(failed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--scope", choices=("selected", "full"), default="selected")
    args = parser.parse_args()
    raise SystemExit(check(args.build, args.scope))
