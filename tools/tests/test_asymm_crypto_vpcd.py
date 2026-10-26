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
    for selector in range(3):
        command = bytes((0x80, 0x16, selector, 0, 0))
        results.append((command, b"\x01\x90\x00",
                        f"EC prime-field private key selector={selector}"))
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
    rsa768 = _rsa_fixture(768)
    n, d, e = rsa768
    modulus = n.to_bytes(96, "big")
    private_exponent = d.to_bytes(96, "big")
    public_exponent = e.to_bytes(3, "big")
    signature_algorithms = {
        39: (hashlib.sha224, "302d300d06096086480165030402040500041c"),
        40: (hashlib.sha256, "3031300d060960864801650304020105000420"),
        41: (hashlib.sha384, "3041300d060960864801650304020205000430"),
        42: (hashlib.sha512, "3051300d060960864801650304020305000440"),
    }
    for algorithm, (digest_function, prefix_hex) in signature_algorithms.items():
        message = f"JCVM RSA SHA{algorithm} PKCS1".encode("ascii")
        digest = digest_function(message).digest()
        digest_info = bytes.fromhex(prefix_hex) + digest
        encoded = b"\x00\x01" + b"\xff" * (96 - len(digest_info) - 3) + b"\x00" + digest_info
        expected_signature = pow(int.from_bytes(encoded, "big"), d, n).to_bytes(96, "big")
        sign_data = modulus + private_exponent + message
        results.append((bytes((0x80, 0x15, algorithm, 0, len(sign_data))) + sign_data + b"\x00",
                        expected_signature + b"\x90\x00", f"RSA SHA-{digest_function().name} PKCS1 sign"))
        verify_data = modulus + bytes((len(public_exponent),)) + public_exponent + message + expected_signature
        results.append((bytes((0x80, 0x15, algorithm, 1, len(verify_data))) + verify_data + b"\x00",
                        b"\x01\x90\x00", f"RSA SHA-{digest_function().name} PKCS1 verify"))
        invalid_signature = bytearray(expected_signature)
        invalid_signature[-1] ^= 1
        verify_bad = modulus + bytes((len(public_exponent),)) + public_exponent + message + invalid_signature
        results.append((bytes((0x80, 0x15, algorithm, 1, len(verify_bad))) + verify_bad + b"\x00",
                        b"\x00\x90\x00", f"RSA SHA-{digest_function().name} reject bad signature"))
        # Short APDUs cap the data field at 255 bytes. A 768-bit RSA key plus
        # SHA-512 hash and signature takes 256 bytes before APDU metadata.
        if algorithm != 42:
            prehash_sign = modulus + private_exponent + digest
            results.append((bytes((0x80, 0x15, algorithm, 2, len(prehash_sign))) + prehash_sign + b"\x00",
                            expected_signature + b"\x90\x00", f"RSA SHA-{digest_function().name} precomputed sign"))
            prehash_verify = modulus + bytes((len(public_exponent),)) + public_exponent + digest + expected_signature
            results.append((bytes((0x80, 0x15, algorithm, 3, len(prehash_verify))) + prehash_verify + b"\x00",
                            b"\x01\x90\x00", f"RSA SHA-{digest_function().name} precomputed verify"))
    pss_algorithms = {
        21: (hashlib.sha1, 512, 20),
        43: (hashlib.sha224, 768, 0),
        44: (hashlib.sha256, 768, 0),
        45: (hashlib.sha384, 896, 0),
        46: (hashlib.sha512, 736, 0),
    }
    for algorithm, (digest_function, bits, parameter_salt_length) in pss_algorithms.items():
        n, d, e = _rsa_fixture(bits)
        key_length = (bits + 7) // 8
        modulus = n.to_bytes(key_length, "big")
        private_exponent = d.to_bytes(key_length, "big")
        public_exponent = e.to_bytes(3, "big")
        message = f"JCVM PSS algorithm={algorithm}".encode("ascii")

        # Exercise signing with each algorithm's default salt length where the
        # short APDU can carry the fixture key; SHA-512 uses a zero-salt
        # parameterized precomputed-hash case to fit the short APDU limit.
        if algorithm != 46:
            sign_data = modulus + private_exponent + message
            command = bytes((0x80, 0x15, algorithm, 0, len(sign_data))) + sign_data + b"\x00"
            results.append((command, b"\x01\x90\x00",
                            f"RSA SHA-{digest_function().name} PSS default-salt sign/verify"))

        digest = digest_function(message).digest()
        salt = bytes(range(parameter_salt_length))
        em_bits = bits - 1
        em_length = (em_bits + 7) // 8
        db_length = em_length - len(digest) - 1
        h = digest_function(bytes(8) + digest + salt).digest()
        db = bytes(db_length - len(salt) - 1) + b"\x01" + salt
        mask_input = h
        mask = bytearray()
        counter = 0
        while len(mask) < db_length:
            mask.extend(digest_function(mask_input + counter.to_bytes(4, "big")).digest())
            counter += 1
        masked_db = bytearray(a ^ b for a, b in zip(db, mask[:db_length]))
        unused_bits = 8 * em_length - em_bits
        masked_db[0] &= 0xff >> unused_bits
        encoded = bytes(masked_db) + h + b"\xbc"
        encoded = encoded.rjust(key_length, b"\x00")
        signature_value = pow(int.from_bytes(encoded, "big"), d, n).to_bytes(key_length, "big")

        if algorithm == 46:
            # PSS init parameters are a two-byte Java Card short. Zero salt
            # keeps the 736-bit SHA-512 vector below short-APDU Lc=256.
            sign_data = modulus + private_exponent + digest
            command = bytes((0x80, 0x15, algorithm, 6, len(sign_data))) + sign_data + b"\x00"
            results.append((command, b"\x01\x90\x00", "RSA SHA-512 PSS precomputed sign, salt=0"))
            verify_data = modulus + bytes((len(public_exponent),)) + public_exponent + digest + signature_value
            command = bytes((0x80, 0x15, algorithm, 7, len(verify_data))) + verify_data + b"\x00"
        else:
            verify_data = modulus + bytes((len(public_exponent),)) + public_exponent + message + signature_value
            verify_operation = 1 if parameter_salt_length == len(digest) else 5
            command = bytes((0x80, 0x15, algorithm, verify_operation, len(verify_data))) + verify_data + b"\x00"
        results.append((command, b"\x01\x90\x00",
                        f"RSA SHA-{digest_function().name} PSS known signature, salt={parameter_salt_length}"))
        invalid_signature = bytearray(signature_value)
        invalid_signature[-1] ^= 1
        if algorithm == 46:
            invalid_data = modulus + bytes((len(public_exponent),)) + public_exponent + digest + invalid_signature
            operation = 7
        else:
            invalid_data = modulus + bytes((len(public_exponent),)) + public_exponent + message + invalid_signature
            operation = verify_operation
        command = bytes((0x80, 0x15, algorithm, operation, len(invalid_data))) + invalid_data + b"\x00"
        results.append((command, b"\x00\x90\x00",
                        f"RSA SHA-{digest_function().name} PSS reject modified signature"))

    # ISO 9796-2 scheme 1 with an implicit trailer. Build reference blocks
    # independently, covering both full and partial message recovery.
    n, d, e = _rsa_fixture(512)
    modulus = n.to_bytes(64, "big")
    private_exponent = d.to_bytes(64, "big")
    public_exponent = e.to_bytes(3, "big")
    for message in (b"JCVM ISO9796-2", bytes(range(70))):
        digest = hashlib.sha1(message).digest()
        delta = 64 - len(digest) - 1
        excess_bits = (len(digest) + len(message)) * 8 + 8 + 4 - 512
        recovered_length = (len(message) - ((excess_bits + 7) // 8)
                            if excess_bits > 0 else len(message))
        block = bytearray(64)
        block[delta:delta + len(digest)] = digest
        message_offset = delta - recovered_length
        block[message_offset:message_offset + recovered_length] = message[:recovered_length]
        if message_offset > 1:
            block[1:message_offset] = b"\xbb" * (message_offset - 1)
            block[message_offset - 1] ^= 1
            block[0] = 0x6b if excess_bits > 0 else 0x4b
        else:
            block[0] = 0x6a if excess_bits > 0 else 0x4a
        block[-1] = 0xbc
        signature_value = pow(int.from_bytes(block, "big"), d, n).to_bytes(64, "big")
        sign_data = modulus + private_exponent + message
        results.append((bytes((0x80, 0x15, 9, 0, len(sign_data))) + sign_data + b"\x00",
                        signature_value + b"\x90\x00",
                        f"RSA SHA1 ISO9796-2 sign bytes={len(message)}"))
        verify_data = modulus + bytes((len(public_exponent),)) + public_exponent + message + signature_value
        results.append((bytes((0x80, 0x15, 9, 1, len(verify_data))) + verify_data + b"\x00",
                        b"\x01\x90\x00",
                        f"RSA SHA1 ISO9796-2 verify bytes={len(message)}"))
        altered = bytearray(message)
        altered[-1] ^= 1
        invalid_data = modulus + bytes((len(public_exponent),)) + public_exponent + altered + signature_value
        results.append((bytes((0x80, 0x15, 9, 1, len(invalid_data))) + invalid_data + b"\x00",
                        b"\x00\x90\x00",
                        f"RSA SHA1 ISO9796-2 reject altered message bytes={len(message)}"))
        sign_data = modulus + private_exponent + message
        results.append((bytes((0x80, 0x15, 30, 0, len(sign_data))) + sign_data + b"\x00",
                        signature_value + recovered_length.to_bytes(2, "big") + b"\x90\x00",
                        f"RSA SHA1 ISO9796-2 MR sign bytes={len(message)}"))
        suffix = message[recovered_length:]
        mr_verify_data = modulus + bytes((len(public_exponent),)) + public_exponent + message + signature_value
        results.append((bytes((0x80, 0x15, 30, 1, len(mr_verify_data))) + mr_verify_data + b"\x00",
                        b"\x01" + recovered_length.to_bytes(2, "big") + b"\x90\x00",
                        f"RSA SHA1 ISO9796-2 MR verify bytes={len(message)} recovered={recovered_length}"))
        if suffix:
            results.append((bytes((0x80, 0x15, 30, 2, len(mr_verify_data))) + mr_verify_data + b"\x00",
                            b"\x00" + recovered_length.to_bytes(2, "big") + b"\x90\x00",
                            f"RSA SHA1 ISO9796-2 MR reject altered suffix bytes={len(message)}"))
    return results


def _rsa_512_fixture() -> tuple[int, int, int]:
    return _rsa_fixture(512)


def _rsa_fixture(bits: int) -> tuple[int, int, int]:
    rng = random.Random(0x52434152 + bits)
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
            prime_bits = bits // 2
            candidate = rng.getrandbits(prime_bits) | (1 << (prime_bits - 1)) | 1
            if probable_prime(candidate):
                return candidate

    e = 65537
    while True:
        p, q = next_prime(), next_prime()
        phi = (p - 1) * (q - 1)
        if p != q and math.gcd(e, phi) == 1:
            n = p * q
            if n.bit_length() == bits:
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
    rsa_signature_vectors = sum("RSA SHA" in label for _, _, label in cases)
    print(f"TEST:SUMMARY asymmetric_vectors={len(cases)} sha_vectors=10 rsa_key_builder_vectors=5 ec_key_builder_vectors=3 rsa_signature_vectors={rsa_signature_vectors} fail=0")
    print(f"TEST:LOG {log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
