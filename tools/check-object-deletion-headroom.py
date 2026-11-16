#!/usr/bin/env python3
"""Validate pre-cleanup storage telemetry and enforce the 90% T10 headroom gate."""

import argparse
import re
import sys
from pathlib import Path

SAMPLE = re.compile(
    r"OBJECT_DELETION:PRE_CLEANUP\b.*?files=(\d+)\s+bytes=(\d+)/(\d+)"
)


def inspect(path: Path) -> tuple[int, int, int, int]:
    samples = []
    malformed = 0
    for line_number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        if "OBJECT_DELETION:PRE_CLEANUP" not in line:
            continue
        match = SAMPLE.search(line)
        if not match:
            malformed += 1
            print(f"HEADROOM:INVALID path={path} line={line_number}", file=sys.stderr)
            continue
        files, used, capacity = map(int, match.groups())
        if capacity <= 0 or used > capacity:
            malformed += 1
            print(
                f"HEADROOM:INVALID path={path} line={line_number} files={files} "
                f"bytes={used}/{capacity}",
                file=sys.stderr,
            )
            continue
        samples.append((files, used, capacity))
    if malformed:
        raise ValueError(f"{malformed} malformed pre-cleanup telemetry record(s)")
    if not samples:
        raise ValueError("no OBJECT_DELETION:PRE_CLEANUP telemetry found")
    capacities = {sample[2] for sample in samples}
    if len(capacities) != 1:
        raise ValueError(f"multiple storage capacities reported: {sorted(capacities)}")
    max_files = max(sample[0] for sample in samples)
    max_used = max(sample[1] for sample in samples)
    return len(samples), max_files, max_used, samples[0][2]


def exceeds_limit(used: int, capacity: int) -> bool:
    return used * 10 > capacity * 9


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="merged serial/emulator log")
    args = parser.parse_args()
    try:
        samples, max_files, max_used, capacity = inspect(args.log)
    except (OSError, ValueError) as exc:
        print(f"HEADROOM:FAIL reason={exc}")
        return 1
    percent_milli = max_used * 100_000 // capacity
    print(
        f"HEADROOM:{'FAIL' if exceeds_limit(max_used, capacity) else 'PASS'} "
        f"samples={samples} peak_files={max_files} peak_bytes={max_used}/{capacity} "
        f"peak_percent={percent_milli / 1000:.3f} threshold=90%"
    )
    return int(exceeds_limit(max_used, capacity))


if __name__ == "__main__":
    raise SystemExit(main())
