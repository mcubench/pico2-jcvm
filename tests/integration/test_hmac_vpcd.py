#!/usr/bin/env python3
"""Run native HMAC known-answer vectors through the PC VPCD CAP path."""

from __future__ import annotations

import hashlib
import hmac
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
APPLET_AID = "A00000015100020201"
KEY = bytes((0x0B,)) * 20
MESSAGE = b"Hi There"
ALGORITHMS = {
    24: "sha1",
    25: "sha256",
    26: "sha384",
    27: "sha512",
    28: "md5",
    29: "ripemd160",
}


def main() -> int:
    timestamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    log = ROOT / f"artifacts/logs/hmac-kat-vpcd-{timestamp}.log"
    command = [
        sys.executable, str(ROOT / "tests/integration/pc_vpcd_applet_test.py"),
        "--emulator", str(ROOT / "build/choupi-pc/choupi"),
        "--memory", str(ROOT / "build/tests/hmac/hmac-empty-flash.bin"),
        "--cap-code", str(ROOT / "build/hmac-probe/load-code.bin"),
        "--rom-mask", str(ROOT / "build/choupi-pc/flash"),
        "--applet-aid", APPLET_AID, "--package-id", "57", "--passes", "1",
        "--connect-timeout", "300", "--apdu-timeout", "60",
        "--flash-copy", str(ROOT / "build/hmac-probe/flash-vpcd.bin"),
        "--log", str(log),
    ]
    results = 0
    for algorithm, digest_name in ALGORITHMS.items():
        try:
            digest = hashlib.new(digest_name)
        except ValueError as error:
            raise SystemExit(f"host hashlib lacks {digest_name}: {error}") from error
        expected_mac = hmac.new(KEY, MESSAGE, digest_name).digest()
        for mode, label in ((0, "one-shot"), (1, "split-update")):
            apdu = bytes((0x80, 0x23, algorithm, mode, len(MESSAGE))) + MESSAGE + b"\x00"
            expected = expected_mac + b"\x01\x01\x01\x90\x00"
            command.extend(("--apdu", apdu.hex(), "--expect-apdu-response", expected.hex()))
            results += 1
    completed = subprocess.run(command, cwd=ROOT, check=False)
    if completed.returncode:
        return completed.returncode
    print(f"TEST:SUMMARY hmac_algorithms={len(ALGORITHMS)} vectors={results} fail=0")
    print(f"TEST:LOG {log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
