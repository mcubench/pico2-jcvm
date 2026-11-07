#!/usr/bin/env python3
"""Run deterministic AES/DES cipher and MAC vectors against the PC VM."""

from __future__ import annotations

import sys
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.cmac import CMAC
from cryptography.hazmat.decrepit.ciphers.algorithms import TripleDES


ROOT = Path(__file__).resolve().parents[2]
APPLET_AID = "A00000015100020101"
AES_KEY = bytes.fromhex("0102030405060708090A0B0C0D0E0F10")
AES_MAC_KEY = bytes.fromhex("2B7E151628AED2A6ABF7158809CF4F3C")
DES_KEY = bytes.fromhex("133457799BBCDFF1")
MAC_DES_KEY = bytes.fromhex("0102030405060708")
MAC3_KEY = bytes.fromhex("0102030405060708090A0B0C0D0E0F10")
AES_MAC_MESSAGE = bytes.fromhex("6BC1BEE22E409F96E93D7E117393172A")


def crypt(block_cipher: algorithms.BlockCipherAlgorithm, mode, data: bytes,
          encrypt: bool) -> bytes:
    context = Cipher(block_cipher, mode).encryptor() if encrypt else Cipher(block_cipher, mode).decryptor()
    return context.update(data) + context.finalize()


def pad(data: bytes, block: int, padding: str) -> bytes:
    if padding == "nopad":
        if len(data) % block:
            raise ValueError("NOPAD vector must be block aligned")
        return data
    remainder = len(data) % block
    if padding == "m1":
        return data + bytes((-remainder) % block)
    if padding == "m2":
        count = block - remainder
        return data + b"\x80" + bytes(count - 1)
    count = block - remainder
    return data + bytes((count,)) * count


def cipher_vectors() -> list[tuple[bytes, bytes, str]]:
    specs = {
        1: (False, True, "nopad"), 2: (False, True, "m1"),
        3: (False, True, "m2"), 4: (False, True, "pkcs5"),
        5: (False, False, "nopad"), 6: (False, False, "m1"),
        7: (False, False, "m2"), 8: (False, False, "pkcs5"),
        13: (True, True, "nopad"), 14: (True, False, "nopad"),
        22: (True, True, "m1"), 23: (True, True, "m2"),
        24: (True, True, "pkcs5"), 25: (True, False, "m1"),
        26: (True, False, "m2"), 27: (True, False, "pkcs5"),
    }
    results = []
    for algorithm, (aes, cbc, padding) in specs.items():
        block = 16 if aes else 8
        plaintext = (bytes(range(block)) if padding == "nopad" else b"abc")
        padded = pad(plaintext, block, padding)
        key = algorithms.AES(AES_KEY) if aes else TripleDES(DES_KEY)
        cipher_mode = modes.CBC(bytes(block)) if cbc else modes.ECB()
        ciphertext = crypt(key, cipher_mode, padded, True)
        encoded = bytes((0x80, 0x10, algorithm, 0, len(plaintext))) + plaintext + b"\x00"
        results.append((encoded, ciphertext + b"\x90\x00", f"cipher {algorithm} encrypt"))
        encoded = bytes((0x80, 0x10, algorithm, 1, len(ciphertext))) + ciphertext + b"\x00"
        # ISO9797-M1 uses zero padding and cannot recover the original length;
        # Java Card Cipher therefore retains the complete padded output.
        decrypted = padded if padding == "m1" else plaintext
        results.append((encoded, decrypted + b"\x90\x00", f"cipher {algorithm} decrypt"))

    for algorithm, aes, block, key_bytes, iv in (
        (13, True, 16, AES_KEY, bytes.fromhex("00112233445566778899aabbccddeeff")),
        (1, False, 8, DES_KEY, bytes.fromhex("0102030405060708")),
    ):
        first = bytes(range(block))
        second = bytes((0x30 + index) & 0xFF for index in range(block))
        primitive = algorithms.AES(key_bytes) if aes else TripleDES(key_bytes)
        expected_first = crypt(primitive, modes.CBC(iv), first, True)
        expected_zero_iv = crypt(primitive, modes.CBC(bytes(block)), second, True)
        expected_supplied_iv = crypt(primitive, modes.CBC(iv), second, True)

        def lifecycle_apdu(operation: int, data: bytes) -> bytes:
            return bytes((0x80, 0x2D, algorithm, operation, len(data))) + data + b"\x00"

        results.append((lifecycle_apdu(0, iv + first),
                        expected_first + b"\x90\x00",
                        f"CBC {algorithm} first doFinal uses supplied IV"))
        results.append((lifecycle_apdu(1, second),
                        expected_zero_iv + b"\x90\x00",
                        f"CBC {algorithm} reused doFinal starts with zero IV"))
        results.append((lifecycle_apdu(2, iv + second),
                        expected_supplied_iv + b"\x90\x00",
                        f"CBC {algorithm} explicit init restores supplied IV"))

    return results


def cbc_mac(key: bytes, data: bytes, block: int, algorithm3: bool,
            iv: bytes | None = None) -> bytes:
    state = bytes(block) if iv is None else iv
    if algorithm3:
        k1, k2 = key[:8], key[8:16]
        primitive = TripleDES(k1)
        for offset in range(0, len(data), block):
            chunk = bytes(a ^ b for a, b in zip(data[offset:offset + block], state))
            state = crypt(primitive, modes.ECB(), chunk, True)
        state = crypt(TripleDES(k2), modes.ECB(), state, False)
        return crypt(primitive, modes.ECB(), state, True)
    primitive = algorithms.AES(key) if block == 16 else TripleDES(key)
    for offset in range(0, len(data), block):
        chunk = bytes(a ^ b for a, b in zip(data[offset:offset + block], state))
        state = crypt(primitive, modes.ECB(), chunk, True)
    return state


def mac_vectors() -> list[tuple[bytes, bytes, str]]:
    results = []
    message = AES_MAC_MESSAGE
    specs = {
        2: (False, "nopad", 8, 8, False),
        3: (False, "m1", 4, 8, False), 4: (False, "m1", 8, 8, False),
        5: (False, "m2", 4, 8, False), 6: (False, "m2", 8, 8, False),
        7: (False, "pkcs5", 4, 8, False), 8: (False, "pkcs5", 8, 8, False),
        18: (True, "nopad", 16, 16, False),
        19: (False, "m2", 4, 8, True), 20: (False, "m2", 8, 8, True),
        47: (False, "m1", 4, 8, True), 48: (False, "m1", 8, 8, True),
    }
    for algorithm, (aes, padding, result_len, block, alg3) in specs.items():
        source = message if padding == "nopad" else b"abc"
        key = AES_MAC_KEY if aes else MAC3_KEY if alg3 else MAC_DES_KEY
        prepared = pad(source, block, padding)
        calculated = cbc_mac(key, prepared, block, alg3)[:result_len]
        apdu = bytes((0x80, 0x11, algorithm, 0, len(source))) + source + b"\x00"
        results.append((apdu, calculated + b"\x90\x00", f"MAC {algorithm}"))
        verify_input = source + calculated
        apdu = bytes((0x80, 0x11, algorithm, 1, len(verify_input))) + verify_input + b"\x00"
        results.append((apdu, b"\x01\x90\x00", f"MAC {algorithm} verify"))
        incorrect = bytearray(calculated)
        incorrect[0] ^= 1
        verify_input = source + incorrect
        apdu = bytes((0x80, 0x11, algorithm, 1, len(verify_input))) + verify_input + b"\x00"
        results.append((apdu, b"\x00\x90\x00", f"MAC {algorithm} reject"))

    # The native Signature object keeps only a 64-byte update buffer. A
    # one-shot call must still accept the long input directly, and repeated
    # sign() calls on the same instance must reset their MAC state.
    long_message = bytes(range(128))
    long_mac = cbc_mac(MAC_DES_KEY, long_message, 8, algorithm3=False)[:4]
    apdu = bytes((0x80, 0x11, 3, 2, len(long_message))) + long_message + b"\x00"
    results.append((apdu, long_mac + b"\x90\x00",
                    "DES MAC M1 128-byte repeated sign"))
    apdu = bytes((0x80, 0x11, 3, 3, len(long_message))) + long_message + b"\x00"
    results.append((apdu, long_mac + b"\x90\x00",
                    "DES MAC M1 buffered update plus direct sign input"))

    cmac = CMAC(algorithms.AES(AES_MAC_KEY))
    cmac.update(AES_MAC_MESSAGE)
    expected = cmac.finalize()
    apdu = bytes((0x80, 0x11, 49, 0, len(AES_MAC_MESSAGE))) + AES_MAC_MESSAGE + b"\x00"
    results.append((apdu, expected + b"\x90\x00", "AES-CMAC 49"))
    verify_input = AES_MAC_MESSAGE + expected
    apdu = bytes((0x80, 0x11, 49, 1, len(verify_input))) + verify_input + b"\x00"
    results.append((apdu, b"\x01\x90\x00", "AES-CMAC verify"))
    incorrect = bytearray(expected)
    incorrect[0] ^= 1
    verify_input = AES_MAC_MESSAGE + incorrect
    apdu = bytes((0x80, 0x11, 49, 1, len(verify_input))) + verify_input + b"\x00"
    results.append((apdu, b"\x00\x90\x00", "AES-CMAC reject"))

    # The Java Card Signature overload taking init parameters must apply the
    # caller-provided IV to the first MAC block, for both sign and verify.
    for algorithm, key, block, padding, message, iv in (
        (2, MAC_DES_KEY, 8, "nopad", AES_MAC_MESSAGE[:8], bytes.fromhex("0102030405060708")),
        (6, MAC_DES_KEY, 8, "m2", b"abc", bytes.fromhex("8877665544332211")),
        (18, AES_MAC_KEY, 16, "nopad", AES_MAC_MESSAGE, bytes.fromhex("00112233445566778899aabbccddeeff")),
    ):
        prepared = pad(message, block, padding)
        calculated = cbc_mac(key, prepared, block, algorithm3=False, iv=iv)
        signed_input = iv + message
        apdu = bytes((0x80, 0x11, algorithm, 4, len(signed_input))) + signed_input + b"\x00"
        results.append((apdu, calculated + b"\x90\x00", f"MAC {algorithm} sign with IV"))
        verified_input = iv + message + calculated
        apdu = bytes((0x80, 0x11, algorithm, 5, len(verified_input))) + verified_input + b"\x00"
        results.append((apdu, b"\x01\x90\x00", f"MAC {algorithm} verify with IV"))
    return results


def main() -> int:
    cipher_count = len(cipher_vectors())
    mac_count = len(mac_vectors())
    vectors = cipher_vectors() + mac_vectors()
    log = ROOT / "artifacts/logs/symm-crypto-kat-vpcd-20261026.log"
    command = [
        sys.executable, str(ROOT / "tests/integration/pc_vpcd_applet_test.py"),
        "--emulator", str(ROOT / "build/choupi-pc/choupi"),
        "--memory", str(ROOT / "build/tests/jcalgtest/algtest-v184-current-empty-flash.bin"),
        "--cap-code", str(ROOT / "build/symm-crypto-probe/load-code.bin"),
        "--rom-mask", str(ROOT / "build/choupi-pc/flash"),
        "--applet-aid", APPLET_AID, "--package-id", "56", "--passes", "1",
        "--flash-copy", str(ROOT / "build/symm-crypto-probe/flash-vpcd-current.bin"),
        "--log", str(log),
    ]
    for apdu, expected, _label in vectors:
        command.extend(("--apdu", apdu.hex(), "--expect-apdu-response", expected.hex()))
    import subprocess
    completed = subprocess.run(command, cwd=ROOT, check=False)
    if completed.returncode:
        return completed.returncode
    print(f"TEST:SUMMARY cipher_vectors={cipher_count} mac_vectors={mac_count} "
          f"iv_vectors=6 total={len(vectors)} fail=0")
    print(f"TEST:LOG {log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
