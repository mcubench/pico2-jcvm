#!/usr/bin/env python3
"""Run JCAlgTest with a bounded per-APDU and whole-suite deadline."""

from __future__ import annotations

import argparse
import queue
import re
import os
import subprocess
import sys
import threading
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apdu-timeout", type=float)
    parser.add_argument("--suite-timeout", type=float, required=True)
    parser.add_argument("--input-line", action="append", default=[])
    parser.add_argument("--reader-index")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if ((args.apdu_timeout is not None and args.apdu_timeout <= 0) or
            args.suite_timeout <= 0):
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
        bufsize=0,
    )
    assert process.stdin is not None
    assert process.stdout is not None

    lines: queue.Queue[str | None] = queue.Queue()

    def read_output() -> None:
        try:
            while True:
                output_chunk = os.read(process.stdout.fileno(), 4096)
                if not output_chunk:
                    break
                lines.put(output_chunk)
        finally:
            lines.put(None)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    start = time.monotonic()
    pending_apdu: float | None = None
    command_line = ""
    output_closed = False
    reason: str | None = None
    pending_inputs = list(args.input_line)
    reader_answer_pending = args.reader_index
    transcript = ""
    prompt_cursor = 0
    partial_line = ""
    prompt_patterns = (
        re.compile(r"Select index of target reader[^\r\n]*?:", re.IGNORECASE),
        re.compile(r"Test option number:\s*", re.IGNORECASE),
        re.compile(r"Specify type of your card[^\r\n]*?:", re.IGNORECASE),
        re.compile(r"\(y/n\)", re.IGNORECASE),
        re.compile(r"'r' to retry[^\r\n]*'s' to skip", re.IGNORECASE),
    )

    def close_stdin() -> None:
        if process.stdin is not None and not process.stdin.closed:
            process.stdin.close()

    def answer_ready_prompts() -> None:
        nonlocal transcript, prompt_cursor, reader_answer_pending
        for pattern in prompt_patterns:
            match = pattern.search(transcript, prompt_cursor)
            if match is None:
                continue
            if pattern is prompt_patterns[0] and reader_answer_pending is not None:
                answer = reader_answer_pending
                reader_answer_pending = None
            elif pattern is not prompt_patterns[0] and pending_inputs:
                answer = pending_inputs.pop(0)
            else:
                continue
            process.stdin.write((answer + "\n").encode())
            process.stdin.flush()
            prompt_cursor = match.end()
            if not pending_inputs:
                close_stdin()
            return

    while not output_closed or process.poll() is None:
        now = time.monotonic()
        if now - start >= args.suite_timeout:
            reason = f"suite exceeded {args.suite_timeout:g}s"
            break
        if (pending_apdu is not None and args.apdu_timeout is not None and
                now - pending_apdu >= args.apdu_timeout):
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
        decoded = output_line.decode(errors="replace")
        sys.stdout.write(decoded)
        sys.stdout.flush()
        transcript += decoded
        if len(transcript) > 16384:
            trim = len(transcript) - 8192
            transcript = transcript[trim:]
            prompt_cursor = max(0, prompt_cursor - trim)
        answer_ready_prompts()

        partial_line += decoded
        while "\n" in partial_line:
            output_line_text, partial_line = partial_line.split("\n", 1)
            if "CommmandAPDU:" in output_line_text or "CommandAPDU:" in output_line_text:
                pending_apdu = time.monotonic()
                command_line = output_line_text
            elif "ResponseAPDU:" in output_line_text:
                pending_apdu = None

    if reason is not None:
        print(f"JCALGTEST_WATCHDOG:TIMEOUT {reason}", flush=True)
        close_stdin()
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        reader.join(timeout=1)
        return 124

    close_stdin()
    return process.wait()


if __name__ == "__main__":
    raise SystemExit(main())
