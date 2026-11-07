#!/usr/bin/env python3
"""Validate Java Card AES-CTR single-shot and streaming operations on VPCD."""

from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
import subprocess
import sys

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes


ROOT = Path(__file__).resolve().parents[2]
APPLET_AID = "A00000015100020101"
KEY = bytes.fromhex("2B7E151628AED2A6ABF7158809CF4F3C")
COUNTER = bytes.fromhex("F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF")
PLAINTEXT = bytes.fromhex(
    "6BC1BEE22E409F96E93D7E117393172A"
    "AE2D8A571E03AC9C9EB76FAC45AF8E51"
    "30C81C46A35CE411E5FBC1191A0A52EF"
    "F69F2445DF4F9B17AD2B417BE66C3710"
)


def apdu(operation: int, data: bytes) -> bytes:
    return bytes((0x80, 0x10, 0xF0, operation, len(data))) + data + b"\x00"


def main() -> int:
    encryptor = Cipher(algorithms.AES(KEY), modes.CTR(COUNTER)).encryptor()
    ciphertext = encryptor.update(PLAINTEXT) + encryptor.finalize()
    vectors = [
        (apdu(0, COUNTER + PLAINTEXT), ciphertext + b"\x90\x00", "single encrypt"),
        (apdu(1, COUNTER + ciphertext), PLAINTEXT + b"\x90\x00", "single decrypt"),
        (apdu(2, COUNTER + PLAINTEXT), ciphertext + b"\x90\x00", "stream encrypt"),
        (apdu(3, COUNTER + ciphertext), PLAINTEXT + b"\x90\x00", "stream decrypt"),
        (apdu(4, COUNTER + PLAINTEXT), ciphertext + b"\x90\x00", "paired single encrypt"),
        (apdu(5, COUNTER + ciphertext), PLAINTEXT + b"\x90\x00", "paired single decrypt"),
        (apdu(6, COUNTER + PLAINTEXT), ciphertext + b"\x90\x00", "paired stream encrypt"),
        (apdu(7, COUNTER + ciphertext), PLAINTEXT + b"\x90\x00", "paired stream decrypt"),
    ]
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    log = ROOT / "artifacts/logs" / f"aes-ctr-vpcd-{timestamp}.log"
    command = [
        sys.executable, str(ROOT / "tests/integration/pc_vpcd_applet_test.py"),
        "--emulator", str(ROOT / "build/choupi-pc/choupi"),
        "--memory", str(ROOT / "build/tests/jcalgtest/algtest-v184-current-empty-flash.bin"),
        "--cap-code", str(ROOT / "build/symm-crypto-probe/load-code.bin"),
        "--rom-mask", str(ROOT / "build/choupi-pc/flash"),
        "--applet-aid", APPLET_AID, "--package-id", "56", "--passes", "1",
        "--flash-copy", str(ROOT / "build/symm-crypto-probe/flash-vpcd-ctr.bin"),
        "--log", str(log),
    ]
    for command_apdu, expected, _label in vectors:
        command.extend(("--apdu", command_apdu.hex(), "--expect-apdu-response", expected.hex()))
    result = subprocess.run(command, cwd=ROOT, check=False)
    if result.returncode:
        return result.returncode
    print("TEST:SUMMARY aes_ctr_vectors=8 fail=0")
    print(f"TEST:LOG {log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
