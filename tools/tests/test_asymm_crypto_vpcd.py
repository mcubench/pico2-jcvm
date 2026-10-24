#!/usr/bin/env python3
"""Run SHA-1/SHA-2 known-answer vectors through the PC VM."""

from __future__ import annotations

import hashlib
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
APPLET_AID = "A00000015100020101"


def vectors() -> list[tuple[bytes, bytes, str]]:
    algorithms = {
        1: hashlib.sha1,
        4: hashlib.sha256,
        5: hashlib.sha384,
        6: hashlib.sha512,
        7: hashlib.sha224,
    }
    messages = (b"abc", bytes(range(128)))
    results = []
    for algorithm, digest in algorithms.items():
        for message in messages:
            command = bytes((0x80, 0x12, algorithm, 0, len(message)))
            command += message + b"\x00"
            results.append((command, digest(message).digest() + b"\x90\x00",
                            f"SHA algorithm={algorithm} bytes={len(message)}"))
    for selector in range(5):
        command = bytes((0x80, 0x13, selector, 0, 0))
        results.append((command, b"\x01\x90\x00", f"RSA key builder selector={selector}"))
    return results


def main() -> int:
    log = ROOT / "artifacts/logs/asymm-crypto-digest-vpcd-20261024.log"
    command = [
        str(ROOT / "tools/env"), "python3",
        str(ROOT / "tools/tests/pc_vpcd_applet_test.py"),
        "--emulator", str(ROOT / "build/choupi-pc/choupi"),
        "--memory", str(ROOT / "build/algtest-v182-empty-flash.bin"),
        "--cap-code", str(ROOT / "build/symm-crypto-probe/load-code.bin"),
        "--rom-mask", str(ROOT / "build/choupi-pc/flash"),
        "--applet-aid", APPLET_AID, "--package-id", "56", "--passes", "1",
        "--flash-copy", str(ROOT / "build/symm-crypto-probe/flash-asymm-vpcd.bin"),
        "--log", str(log),
    ]
    cases = vectors()
    for apdu, expected, _label in cases:
        command.extend(("--apdu", apdu.hex(), "--expect-apdu-response", expected.hex()))
    completed = subprocess.run(command, cwd=ROOT, check=False)
    if completed.returncode:
        return completed.returncode
    print(f"TEST:SUMMARY asymmetric_vectors={len(cases)} sha_vectors=10 rsa_key_builder_vectors=5 fail=0")
    print(f"TEST:LOG {log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
