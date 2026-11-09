#!/usr/bin/env python3
"""Run JCAlgTest with a bounded per-APDU and whole-suite deadline."""

from __future__ import annotations

import argparse
import queue
import subprocess
import sys
import threading
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apdu-timeout", type=float, required=True)
    parser.add_argument("--suite-timeout", type=float, required=True)
    parser.add_argument("--input-line", action="append", default=[])
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.apdu_timeout <= 0 or args.suite_timeout <= 0:
        parser.error("timeouts must be positive")
    if args.command and args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        parser.error("a command is required after --")
    return args


def main() -> int:
    args = parse_args()
    process = subprocess.Popen(
        args.command,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert process.stdin is not None
    assert process.stdout is not None
    for line in args.input_line:
        process.stdin.write(line + "\n")
    process.stdin.close()

    lines: queue.Queue[str | None] = queue.Queue()

    def read_output() -> None:
        try:
            for output_line in process.stdout:
                lines.put(output_line)
        finally:
            lines.put(None)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    start = time.monotonic()
    pending_apdu: float | None = None
    command_line = ""
    output_closed = False
    reason: str | None = None

    while not output_closed or process.poll() is None:
        now = time.monotonic()
        if now - start >= args.suite_timeout:
            reason = f"suite exceeded {args.suite_timeout:g}s"
            break
        if pending_apdu is not None and now - pending_apdu >= args.apdu_timeout:
            reason = (
                f"APDU exceeded {args.apdu_timeout:g}s: "
                f"{command_line.strip()}"
            )
            break
        try:
            output_line = lines.get(timeout=0.2)
        except queue.Empty:
            continue
        if output_line is None:
            output_closed = True
            continue
        sys.stdout.write(output_line)
        sys.stdout.flush()
        if "CommmandAPDU:" in output_line or "CommandAPDU:" in output_line:
            pending_apdu = time.monotonic()
            command_line = output_line
        elif "ResponseAPDU:" in output_line:
            pending_apdu = None

    if reason is not None:
        print(f"JCALGTEST_WATCHDOG:TIMEOUT {reason}", flush=True)
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        reader.join(timeout=1)
        return 124

    return process.wait()


if __name__ == "__main__":
    raise SystemExit(main())
