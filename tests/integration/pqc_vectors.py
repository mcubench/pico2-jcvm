#!/usr/bin/env python3
"""APDU vectors for the Java Card 3.2 PQC probe (tests/fixtures/local/pqc_applet).

Known-answer checks use the deterministic API paths with the committed NIST
ACVP subset: ML-KEM / ML-DSA key generation through seed expansion
(PQCKeyFactory.generateKeyPair(seed, pub, priv)), ML-KEM decapsulation, the
FIPS 203 encapsulation/decapsulation key checks, and ML-DSA verification
(pure with context, and external mu). Hedged signing and encapsulation draw
fresh randomness on the card, so they are checked by round trips. Large
outputs are compared through the probe's SHA-256 command.

vectors() returns (command bytes, expected response bytes, label) tuples; the
VPCD and PC/SC runners both consume it.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VECTORS = ROOT / "tests/fixtures/external/nist-acvp/975de31e/pqc-subset.json"

OK = b"\x90\x00"
NAME = {512: 0x0501, 768: 0x0502, 1024: 0x0503, 44: 0x0701, 65: 0x0702, 87: 0x0703}
KEM_SIZES = {512: (800, 1632, 768), 768: (1184, 2400, 1088), 1024: (1568, 3168, 1568)}
DSA_SIZES = {44: (1312, 2560, 2420), 65: (1952, 4032, 3309), 87: (2592, 4896, 4627)}
ATTR_PUBLIC, ATTR_PRIVATE, ATTR_SEED = 0x0000, 0x8000, 0x4000
ALG_ML_KEM, ALG_DHKEM = 2, 1
MODE_ENC, MODE_DEC, MODE_EXPAND = 1, 2, 0x1000
SIG_ML_DSA, SIG_ML_DSA_MU, SIG_ML_DSA_SEED, SIG_ML_DSA_MU_SEED = 21, 22, 23, 24
MODE_SIGN, MODE_VERIFY = 1, 2
# BUF layout in the probe applet (12288 bytes).
A, B, C, D = 0, 2600, 7300, 12032
SW_CRYPTO = 0x9C00
ILLEGAL_VALUE, UNINITIALIZED_KEY, NO_SUCH_ALGORITHM, INVALID_INIT, ILLEGAL_USE = 1, 2, 3, 4, 5


def sw(value: int) -> bytes:
    return value.to_bytes(2, "big")


def crypto(reason: int) -> bytes:
    return sw(SW_CRYPTO + reason)


def s16(*values: int) -> bytes:
    return b"".join((v & 0xFFFF).to_bytes(2, "big") for v in values)


def apdu(ins: int, p1: int = 0, p2: int = 0, data: bytes = b"", le: bool = False) -> bytes:
    command = bytes((0x80, ins, p1 & 0xFF, p2 & 0xFF))
    if data:
        command += bytes((len(data),)) + data
    if le:
        command += b"\x00"
    return command


class Builder:
    def __init__(self) -> None:
        self.cases: list[tuple[bytes, bytes, str]] = []

    def add(self, command: bytes, expected: bytes, label: str) -> None:
        self.cases.append((command, expected, label))

    def write(self, offset: int, data: bytes, label: str) -> None:
        for at in range(0, len(data), 240):
            chunk = data[at:at + 240]
            self.add(apdu(0x10, (offset + at) >> 8, (offset + at) & 0xFF, chunk), OK,
                     f"{label} write {at}/{len(data)}")

    def hash_is(self, offset: int, data: bytes, label: str) -> None:
        self.add(apdu(0x12, data=s16(offset, len(data)), le=True),
                 hashlib.sha256(data).digest() + OK, label)

    def read_is(self, offset: int, data: bytes, label: str) -> None:
        self.add(apdu(0x11, offset >> 8, offset & 0xFF, bytes((len(data),)), le=True),
                 data + OK, label)

    def build(self, slot: int, name: int, attributes: int, label: str,
              expected: bytes = OK) -> None:
        self.add(apdu(0x20, slot, 0, s16(name, attributes)), expected, label)

    def set_encoded(self, slot: int, offset: int, length: int, label: str,
                    expected: bytes = OK) -> None:
        self.add(apdu(0x21, slot, 0, s16(offset, length)), expected, label)

    def get_encoded(self, slot: int, offset: int, length: int, label: str) -> None:
        self.add(apdu(0x22, slot, 0, s16(offset), le=True), s16(offset + length) + OK,
                 label)

    def compare(self, off1: int, off2: int, length: int, equal: bool, label: str) -> None:
        self.add(apdu(0x13, data=s16(off1, off2, length), le=True),
                 bytes((1 if equal else 0,)) + OK, label)

    def info(self, slot: int, initialized: bool, kem: bool, number: int, length: int,
             name: int, private: bool, label: str) -> None:
        flags = (1 if private else 2) | (4 if kem and private else 0)
        expected = bytes((1 if initialized else 0, 44 if kem else 45)) + s16(
            number, length, name) + bytes((flags,)) + OK
        self.add(apdu(0x25, slot, le=True), expected, label)


def kem_vectors(b: Builder, data: dict) -> None:
    by_kind = {}
    for group in data["groups"]:
        by_kind.setdefault((group["kind"], group["set"]), []).extend(group["cases"])
    for number in (512, 768, 1024):
        name = NAME[number]
        ek_len, dk_len, ct_len = KEM_SIZES[number]
        tag = f"ML-KEM-{number}"
        b.add(apdu(0x29, data=s16(name), le=True), s16(name) + OK,
              f"{tag} NamedParameterSpec.getInstance")
        b.build(0, name, ATTR_PUBLIC, f"{tag} buildKey public")
        b.build(1, name, ATTR_PRIVATE, f"{tag} buildKey private expanded")
        b.build(2, name, ATTR_PRIVATE | ATTR_SEED, f"{tag} buildKey private seed")
        b.info(2, False, True, number, 64, name, True, f"{tag} seed key info (empty)")
        # KeyGen KAT: d || z seed -> ek, dk (FIPS 203 Algorithm 16).
        for case in by_kind[("mlkem-keygen", number)]:
            label = f"{tag} keygen tc{case['tcId']}"
            ek, dk = bytes.fromhex(case["ek"]), bytes.fromhex(case["dk"])
            b.write(A, bytes.fromhex(case["d"] + case["z"]), label)
            b.set_encoded(2, A, 64, f"{label} setEncoded seed")
            b.add(apdu(0x24, 2, 0, bytes((0, 1))), OK, f"{label} expand seed")
            b.get_encoded(0, A, ek_len, f"{label} getEncoded ek")
            b.hash_is(A, ek, f"{label} ek matches ACVP")
            b.get_encoded(1, B, dk_len, f"{label} getEncoded dk")
            b.hash_is(B, dk, f"{label} dk matches ACVP")
            b.add(apdu(0x26, 2, 0, s16(C), le=True), s16(C + ek_len) + OK,
                  f"{label} getPublicKey from seed")
            b.hash_is(C, ek, f"{label} seed getPublicKey matches ek")
            b.add(apdu(0x26, 1, 0, s16(C), le=True), s16(C + ek_len) + OK,
                  f"{label} getPublicKey from expanded")
            b.hash_is(C, ek, f"{label} expanded getPublicKey matches ek")
        b.info(0, True, True, number, ek_len, name, False, f"{tag} public key info")
        b.info(1, True, True, number, dk_len, name, True, f"{tag} expanded key info")
        # Decapsulation KAT (includes implicit-rejection cases).
        b.add(apdu(0x30, data=s16(ALG_ML_KEM, name, MODE_DEC), le=True), b"\x02" + OK,
              f"{tag} KEM.getInstance decapsulation")
        for case in by_kind[("mlkem-decaps", number)]:
            label = f"{tag} decaps tc{case['tcId']}"
            b.write(A, bytes.fromhex(case["dk"]), label)
            b.set_encoded(1, A, dk_len, f"{label} setEncoded dk")
            b.write(B, bytes.fromhex(case["c"]), label)
            b.add(apdu(0x32, 1, 0, s16(B, C), le=True), s16(32) + OK, f"{label} decapsulate")
            b.read_is(C, bytes.fromhex(case["k"]), f"{label} shared secret matches ACVP")
            b.add(apdu(0x32, 1, 1, s16(B, 0, 32), le=True), s16(32) + OK,
                  f"{label} decapsulate into AES-256 key")
            b.add(apdu(0x33, le=True), bytes.fromhex(case["k"]) + OK,
                  f"{label} AES key holds shared secret")
        b.add(apdu(0x32, 2, 0, s16(B, C), le=True), crypto(ILLEGAL_VALUE),
              f"{tag} decapsulate seed key without MODE_EXPAND")
        b.add(apdu(0x32, 1, 1, s16(B, 0, 16), le=True), crypto(ILLEGAL_VALUE),
              f"{tag} decapsulate 16 bytes into AES-256 key")
        b.add(apdu(0x31, 1, 0, s16(0, 0, B, C), le=True), b"\x9E\x03",
              f"{tag} decapsulation-only KEM is not an Encapsulator")
        # FIPS 203 7.2 / 7.3 key checks through setEncoded.
        for kind, slot, field in (("mlkem-encapsulationKeyCheck", 0, "ek"),
                                  ("mlkem-decapsulationKeyCheck", 1, "dk")):
            for case in by_kind[(kind, number)]:
                label = f"{tag} {field} check tc{case['tcId']}"
                value = bytes.fromhex(case[field])
                b.write(A, value, label)
                b.set_encoded(slot, A, len(value), f"{label} setEncoded",
                              OK if case["testPassed"] else crypto(ILLEGAL_VALUE))
        # Fresh key pair, encapsulate (random) and decapsulate both ways.
        b.add(apdu(0x23, 0, 1), OK, f"{tag} PQCKeyFactory.generateKeyPair")
        b.add(apdu(0x30, data=s16(ALG_ML_KEM, name, MODE_ENC | MODE_DEC | MODE_EXPAND),
                   le=True), b"\x03" + OK, f"{tag} KEM.getInstance dual+expand")
        b.add(apdu(0x31, 1, 0, s16(0, 0, B, C), le=True), s16(ct_len, ct_len, 32) + OK,
              f"{tag} encapsulate to PublicKey")
        b.add(apdu(0x32, 1, 0, s16(B, C + 32), le=True), s16(32) + OK,
              f"{tag} decapsulate fresh ciphertext")
        b.compare(C, C + 32, 32, True, f"{tag} encapsulate/decapsulate secrets agree")
        b.get_encoded(0, A, ek_len, f"{tag} export fresh ek")
        b.add(apdu(0x31, 0, 1, s16(A, ek_len, B, 0, 32), le=True),
              s16(ct_len, ct_len, 32) + OK, f"{tag} encapsulate ek bytes into AES key")
        b.add(apdu(0x32, 1, 0, s16(B, C), le=True), s16(32) + OK,
              f"{tag} decapsulate AES-key ciphertext")
        b.add(apdu(0x31, 0, 0, s16(A, ek_len - 1, B, C), le=True), crypto(ILLEGAL_VALUE),
              f"{tag} encapsulate rejects short ek")
        b.add(apdu(0x23, 0, 2), OK, f"{tag} generateKeyPair into seed private key")
        b.add(apdu(0x31, 1, 0, s16(0, 0, B, C), le=True), s16(ct_len, ct_len, 32) + OK,
              f"{tag} encapsulate to seed-pair public key")
        b.add(apdu(0x32, 2, 0, s16(B, C + 32), le=True), s16(32) + OK,
              f"{tag} decapsulate with seed key under MODE_EXPAND")
        b.compare(C, C + 32, 32, True, f"{tag} seed-key secrets agree")
        b.add(apdu(0x28, 0, 1), OK, f"{tag} KeyPair(pub, priv).genKeyPair")
        b.add(apdu(0x31, 1, 0, s16(0, 0, B, C), le=True), s16(ct_len, ct_len, 32) + OK,
              f"{tag} encapsulate after KeyPair.genKeyPair")
        b.add(apdu(0x32, 1, 0, s16(B, C + 32), le=True), s16(32) + OK,
              f"{tag} decapsulate after KeyPair.genKeyPair")
        b.compare(C, C + 32, 32, True, f"{tag} KeyPair secrets agree")
        # API error paths.
        b.set_encoded(0, A, ek_len - 1, f"{tag} setEncoded wrong length",
                      crypto(ILLEGAL_VALUE))
        b.add(apdu(0x27, 1), OK, f"{tag} clearKey")
        b.info(1, False, True, number, dk_len, name, True, f"{tag} cleared key info")
        b.add(apdu(0x22, 1, 0, s16(A), le=True), crypto(UNINITIALIZED_KEY),
              f"{tag} getEncoded uninitialized")
        b.add(apdu(0x32, 1, 0, s16(B, C), le=True), crypto(UNINITIALIZED_KEY),
              f"{tag} decapsulate uninitialized key")
        b.build(1, name, ATTR_PUBLIC | ATTR_SEED, f"{tag} buildKey seed public",
                crypto(ILLEGAL_VALUE))
        b.add(apdu(0x30, data=s16(ALG_ML_KEM, name, MODE_ENC | MODE_EXPAND), le=True),
              crypto(ILLEGAL_VALUE), f"{tag} KEM MODE_EXPAND without decapsulation")
        b.add(apdu(0x60), OK, f"{tag} release objects")


def dsa_vectors(b: Builder, data: dict) -> None:
    by_kind = {}
    for group in data["groups"]:
        key = (group["kind"], group["set"], group.get("signatureInterface"),
               group.get("externalMu"))
        by_kind.setdefault(key, []).extend(group["cases"])
    for number in (44, 65, 87):
        name = NAME[number]
        pk_len, sk_len, sig_len = DSA_SIZES[number]
        tag = f"ML-DSA-{number}"
        b.add(apdu(0x29, data=s16(name), le=True), s16(name) + OK,
              f"{tag} NamedParameterSpec.getInstance")
        b.build(0, name, ATTR_PUBLIC, f"{tag} buildKey public")
        b.build(1, name, ATTR_PRIVATE, f"{tag} buildKey private expanded")
        b.build(2, name, ATTR_PRIVATE | ATTR_SEED, f"{tag} buildKey private seed")
        # KeyGen KAT: xi -> pk, sk (FIPS 204 Algorithm 6).
        keygen = by_kind[("mldsa-keygen", number, None, None)]
        for case in keygen:
            label = f"{tag} keygen tc{case['tcId']}"
            b.write(A, bytes.fromhex(case["seed"]), label)
            b.set_encoded(2, A, 32, f"{label} setEncoded seed")
            b.add(apdu(0x24, 2, 0, bytes((0, 1))), OK, f"{label} expand seed")
            b.get_encoded(0, A, pk_len, f"{label} getEncoded pk")
            b.hash_is(A, bytes.fromhex(case["pk"]), f"{label} pk matches ACVP")
            b.get_encoded(1, B, sk_len, f"{label} getEncoded sk")
            b.hash_is(B, bytes.fromhex(case["sk"]), f"{label} sk matches ACVP")
        b.info(0, True, False, number, pk_len, name, False, f"{tag} public key info")
        b.info(1, True, False, number, sk_len, name, True, f"{tag} private key info")
        # SigVer KAT: pure ML-DSA with context, and external mu.
        for interface, mu, cipher in (("external", None, SIG_ML_DSA),
                                      ("internal", True, SIG_ML_DSA_MU)):
            for case in by_kind[("mldsa-sigver", number, interface, mu)]:
                label = f"{tag} sigver {interface} tc{case['tcId']}"
                message = bytes.fromhex(case["mu"] if mu else case["message"])
                context = bytes.fromhex(case.get("context", ""))
                assert C + len(message) <= D and D + len(context) <= 12288, label
                b.write(A, bytes.fromhex(case["pk"]), label)
                b.set_encoded(0, A, pk_len, f"{label} setEncoded pk")
                b.write(B, bytes.fromhex(case["signature"]), label)
                b.write(C, message, label)
                b.write(D, context, label)
                b.add(apdu(0x40, cipher, le=True), bytes((cipher,)) + OK,
                      f"{label} Signature.getInstance")
                if mu:
                    b.add(apdu(0x41, 0, MODE_VERIFY), OK, f"{label} init verify")
                else:
                    b.add(apdu(0x41, 0, MODE_VERIFY, s16(D, len(context))), OK,
                          f"{label} init verify with context")
                expected = bytes((1 if case["testPassed"] else 0,)) + OK
                b.add(apdu(0x43, data=s16(C, len(message), B, sig_len, 0), le=True),
                      expected, f"{label} verify")
                if not mu and len(message) > 7:
                    b.add(apdu(0x43, data=s16(C, len(message), B, sig_len, 7), le=True),
                          expected, f"{label} verify with update() chunks")
        # Hedged signing round trips with the keygen key (slots 0 / 1 / 2).
        case = keygen[0]
        b.write(A, bytes.fromhex(case["seed"]), f"{tag} reload")
        b.set_encoded(2, A, 32, f"{tag} reload seed")
        b.add(apdu(0x24, 2, 0, bytes((0, 1))), OK, f"{tag} reload expand")
        message = bytes(range(256)) * 3
        context = b"Java Card 3.2 PQC probe"
        b.write(C, message, f"{tag} message")
        b.write(D, context, f"{tag} context")
        for cipher, slot in ((SIG_ML_DSA, 1), (SIG_ML_DSA_SEED, 2)):
            label = f"{tag} sign cipher={cipher} slot={slot}"
            b.add(apdu(0x40, cipher, le=True), bytes((cipher,)) + OK,
                  f"{label} Signature.getInstance")
            b.add(apdu(0x41, slot, MODE_SIGN, s16(D, len(context))), OK, f"{label} init")
            b.add(apdu(0x42, data=s16(C, len(message), B, 100), le=True),
                  s16(sig_len, sig_len) + OK, f"{label} sign (update chunks)")
            b.add(apdu(0x41, 0, MODE_VERIFY, s16(D, len(context))), OK,
                  f"{label} init verify")
            b.add(apdu(0x43, data=s16(C, len(message), B, sig_len, 0), le=True),
                  b"\x01" + OK, f"{label} verify own signature")
            b.add(apdu(0x14, (B + 10) >> 8, (B + 10) & 0xFF), OK, f"{label} corrupt")
            b.add(apdu(0x43, data=s16(C, len(message), B, sig_len, 0), le=True),
                  b"\x00" + OK, f"{label} verify rejects corrupted signature")
            b.add(apdu(0x43, data=s16(C, len(message), B, sig_len - 1, 0), le=True),
                  b"\x00" + OK, f"{label} verify rejects short signature")
        # External mu: sign mu (random-looking bytes), verify, reject context.
        mu_value = hashlib.shake_256(b"mu" + bytes((number,))).digest(64)
        b.write(C, mu_value, f"{tag} mu")
        b.add(apdu(0x40, SIG_ML_DSA_MU, le=True), bytes((SIG_ML_DSA_MU,)) + OK,
              f"{tag} external-mu Signature.getInstance")
        b.add(apdu(0x41, 1, MODE_SIGN, s16(D, 1)), crypto(ILLEGAL_VALUE),
              f"{tag} external-mu init with context")
        b.add(apdu(0x41, 2, MODE_SIGN), crypto(ILLEGAL_USE),
              f"{tag} external-mu init with seed key (no EXPAND_SEED)")
        b.add(apdu(0x41, 1, MODE_SIGN), OK, f"{tag} external-mu init sign")
        b.add(apdu(0x42, data=s16(C, 64, B, 0), le=True), s16(sig_len, sig_len) + OK,
              f"{tag} external-mu sign")
        b.add(apdu(0x42, data=s16(C, 64, B, 10), le=True), crypto(ILLEGAL_USE),
              f"{tag} external-mu update")
        b.add(apdu(0x41, 0, MODE_VERIFY), OK, f"{tag} external-mu init verify")
        b.add(apdu(0x43, data=s16(C, 64, B, sig_len, 0), le=True), b"\x01" + OK,
              f"{tag} external-mu verify")
        b.add(apdu(0x40, SIG_ML_DSA, le=True), bytes((SIG_ML_DSA,)) + OK,
              f"{tag} plain Signature.getInstance")
        b.add(apdu(0x41, 2, MODE_SIGN), crypto(ILLEGAL_USE),
              f"{tag} plain init with seed key")
        b.add(apdu(0x41, 0, MODE_SIGN), crypto(ILLEGAL_VALUE),
              f"{tag} sign init with public key")
        # Secret-key import validation (tr inconsistent with the key).
        b.get_encoded(1, A, sk_len, f"{tag} export sk")
        b.add(apdu(0x14, 0, 70), OK, f"{tag} corrupt tr")
        b.set_encoded(1, A, sk_len, f"{tag} setEncoded rejects inconsistent sk",
                      crypto(ILLEGAL_VALUE))
        b.add(apdu(0x28, 0, 1), OK, f"{tag} KeyPair(pub, priv).genKeyPair")
        b.add(apdu(0x40, SIG_ML_DSA, le=True), bytes((SIG_ML_DSA,)) + OK,
              f"{tag} KeyPair Signature.getInstance")
        b.add(apdu(0x41, 1, MODE_SIGN), OK, f"{tag} KeyPair init sign")
        b.add(apdu(0x42, data=s16(C, 33, B, 0), le=True), s16(sig_len, sig_len) + OK,
              f"{tag} KeyPair sign")
        b.add(apdu(0x41, 0, MODE_VERIFY), OK, f"{tag} KeyPair init verify")
        b.add(apdu(0x43, data=s16(C, 33, B, sig_len, 0), le=True), b"\x01" + OK,
              f"{tag} KeyPair verify")
        b.add(apdu(0x60), OK, f"{tag} release objects")


def vectors(kem: bool = True, dsa: bool = True) -> list[tuple[bytes, bytes, str]]:
    data = json.loads(VECTORS.read_text())
    b = Builder()
    b.add(apdu(0x30, data=s16(ALG_DHKEM, NAME[512], MODE_ENC), le=True),
          crypto(NO_SUCH_ALGORITHM), "KEM.getInstance ALG_DHKEM unsupported")
    b.add(apdu(0x29, data=s16(0x0103), le=True), crypto(NO_SUCH_ALGORITHM),
          "NamedParameterSpec.getInstance X25519 unsupported")
    if kem:
        kem_vectors(b, data)
    if dsa:
        dsa_vectors(b, data)
    return b.cases


if __name__ == "__main__":
    cases = vectors()
    print(f"{len(cases)} cases")
