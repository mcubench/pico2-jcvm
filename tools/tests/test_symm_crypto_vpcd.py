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
        results.append((encoded, plaintext + b"\x90\x00", f"cipher {algorithm} decrypt"))
    return results


def cbc_mac(key: bytes, data: bytes, block: int, algorithm3: bool) -> bytes:
    state = bytes(block)
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
    return results


def main() -> int:
    vectors = cipher_vectors() + mac_vectors()
    log = ROOT / "artifacts/logs/symm-crypto-kat-vpcd-20261024.log"
    command = [
        sys.executable, str(ROOT / "tools/tests/pc_vpcd_applet_test.py"),
        "--emulator", str(ROOT / "build/choupi-pc/choupi"),
        "--memory", str(ROOT / "build/algtest-v182-empty-flash.bin"),
        "--cap-code", str(ROOT / "build/symm-crypto-probe/load-code.bin"),
        "--rom-mask", str(ROOT / "build/choupi-pc/flash"),
        "--applet-aid", APPLET_AID, "--package-id", "56", "--passes", "1",
        "--flash-copy", str(ROOT / "build/symm-crypto-probe/flash-vpcd.bin"),
        "--log", str(log),
    ]
    for apdu, expected, _label in vectors:
        command.extend(("--apdu", apdu.hex(), "--expect-apdu-response", expected.hex()))
    import subprocess
    completed = subprocess.run(command, cwd=ROOT, check=False)
    if completed.returncode:
        return completed.returncode
    print(f"TEST:SUMMARY symmetric_cipher_vectors=32 mac_sign_vectors=13 mac_verify_vectors=26 total={len(vectors)} fail=0")
    print(f"TEST:LOG {log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
