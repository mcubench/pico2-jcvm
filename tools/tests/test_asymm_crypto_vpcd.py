#!/usr/bin/env python3
"""Run digest, RSA key-storage, and RSA cipher vectors through the PC VM."""

from __future__ import annotations

import hashlib
import math
import random
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
    rsa = _rsa_512_fixture()
    messages = {
        10: b"JCVM RSA PKCS1",
        12: bytes(63) + b"\x2a",
        15: b"JCVM RSA OAEP",
    }
    for algorithm, message in messages.items():
        data = rsa[0].to_bytes(64, "big") + b"\x01\x00\x01" + \
            rsa[1].to_bytes(64, "big") + message
        command = bytes((0x80, 0x14, algorithm, 0, len(data))) + data + b"\x00"
        results.append((command, message + b"\x90\x00",
                        f"RSA cipher algorithm={algorithm} bytes={len(message)}"))
    n, d, e = rsa
    message = b"JCVM RSA SHA1 PKCS1"
    digest = hashlib.sha1(message).digest()
    digest_info = bytes.fromhex("3021300906052b0e03021a05000414") + digest
    encoded = b"\x00\x01" + b"\xff" * (64 - len(digest_info) - 3) + b"\x00" + digest_info
    expected_signature = pow(int.from_bytes(encoded, "big"), d, n).to_bytes(64, "big")
    modulus = n.to_bytes(64, "big")
    private_exponent = d.to_bytes(64, "big")
    public_exponent = e.to_bytes(3, "big")
    sign_data = modulus + private_exponent + message
    results.append((bytes((0x80, 0x15, 10, 0, len(sign_data))) + sign_data + b"\x00",
                    expected_signature + b"\x90\x00", "RSA SHA1 PKCS1 sign"))
    verify_data = modulus + bytes((len(public_exponent),)) + public_exponent + message + expected_signature
    results.append((bytes((0x80, 0x15, 10, 1, len(verify_data))) + verify_data + b"\x00",
                    b"\x01\x90\x00", "RSA SHA1 PKCS1 verify"))
    rejected_signature = bytearray(expected_signature)
    rejected_signature[-1] ^= 1
    verify_bad = modulus + bytes((len(public_exponent),)) + public_exponent + message + rejected_signature
    results.append((bytes((0x80, 0x15, 10, 1, len(verify_bad))) + verify_bad + b"\x00",
                    b"\x00\x90\x00", "RSA SHA1 PKCS1 reject bad signature"))
    verify_out_of_range = modulus + bytes((len(public_exponent),)) + public_exponent + message + modulus
    results.append((bytes((0x80, 0x15, 10, 1, len(verify_out_of_range))) + verify_out_of_range + b"\x00",
                    b"\x00\x90\x00", "RSA SHA1 PKCS1 reject signature equal to modulus"))
    prehash_sign = modulus + private_exponent + digest
    results.append((bytes((0x80, 0x15, 10, 2, len(prehash_sign))) + prehash_sign + b"\x00",
                    expected_signature + b"\x90\x00", "RSA SHA1 PKCS1 precomputed sign"))
    prehash_verify = modulus + bytes((len(public_exponent),)) + public_exponent + digest + expected_signature
    results.append((bytes((0x80, 0x15, 10, 3, len(prehash_verify))) + prehash_verify + b"\x00",
                    b"\x01\x90\x00", "RSA SHA1 PKCS1 precomputed verify"))
    return results


def _rsa_512_fixture() -> tuple[int, int, int]:
    rng = random.Random(0x52434152)
    small_primes = (3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47)

    def probable_prime(candidate: int) -> bool:
        if any(candidate % prime == 0 for prime in small_primes):
            return False
        d = candidate - 1
        shifts = 0
        while d & 1 == 0:
            shifts += 1
            d >>= 1
        for base in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
            value = pow(base, d, candidate)
            if value in (1, candidate - 1):
                continue
            for _ in range(shifts - 1):
                value = (value * value) % candidate
                if value == candidate - 1:
                    break
            else:
                return False
        return True

    def next_prime() -> int:
        while True:
            candidate = rng.getrandbits(256) | (1 << 255) | 1
            if probable_prime(candidate):
                return candidate

    e = 65537
    while True:
        p, q = next_prime(), next_prime()
        phi = (p - 1) * (q - 1)
        if p != q and math.gcd(e, phi) == 1:
            n = p * q
            if n.bit_length() == 512:
                return n, pow(e, -1, phi), e


def main() -> int:
    log = ROOT / "artifacts/logs/asymm-crypto-digest-vpcd-20261024.log"
    command = [
        str(ROOT / "tools/env"), "python3",
        str(ROOT / "tools/tests/pc_vpcd_applet_test.py"),
        "--emulator", str(ROOT / "build/choupi-pc/choupi"),
        "--memory", str(ROOT / "build/algtest-v182-current-empty-flash.bin"),
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
    print(f"TEST:SUMMARY asymmetric_vectors={len(cases)} sha_vectors=10 rsa_key_builder_vectors=5 rsa_signature_vectors=6 fail=0")
    print(f"TEST:LOG {log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
