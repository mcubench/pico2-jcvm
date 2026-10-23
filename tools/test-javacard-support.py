#!/usr/bin/env python3
"""Focused tests for capability validation, metadata generation, and comparison."""

import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
GENERATOR = ROOT / "tools/javacard-support.py"
COMPARATOR = ROOT / "tools/compare-jcalgtest-support.py"
CONFIG = json.loads((ROOT / "config/javacard-support.json").read_text())


class JavaCardSupportTests(unittest.TestCase):
    def run_generator(self, config, out_dir):
        config_path = out_dir.parent / "config.json"
        config_path.write_text(json.dumps(config))
        return subprocess.run(
            [sys.executable, str(GENERATOR), "--config", str(config_path), "--out-dir", str(out_dir)],
            text=True, capture_output=True,
        )

    def test_current_config_generates_exactly_fifteen_expected_rows(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_generator(CONFIG, out)
            self.assertEqual(result.returncode, 0, result.stderr)
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                rows = list(csv.reader(source, delimiter=";"))
            self.assertEqual(len(rows) - 1, 15)
            self.assertEqual(sum(CONFIG.values()), 15)
            self.assertIn("#define JC_SUPPORT_SHA256 1", (out / "javacard-support.h").read_text())
            self.assertTrue((out / "java/javacard/security/SecureRandomData.java").is_file())
            self.assertTrue((out / "java/javacard/security/AESKeyProvider.java").is_file())
            self.assertTrue((out / "java/javacard/security/DESKeyProvider.java").is_file())
            self.assertTrue((out / "java/javacard/security/Sha256DigestProvider.java").is_file())

    def test_minimal_config_generates_empty_supported_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_generator({key: False for key in CONFIG}, out)
            self.assertEqual(result.returncode, 0, result.stderr)
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                self.assertEqual(list(csv.reader(source, delimiter=";")), [["section", "algorithm_name"]])
            self.assertIn("#define JC_SUPPORT_SHA256 0", (out / "javacard-support.h").read_text())
            self.assertFalse((out / "java/javacard/security/SecureRandomData.java").exists())
            self.assertFalse((out / "java/javacard/security/AESKeyProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/DESKeyProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/Sha256DigestProvider.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/AESCipherProvider.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/DESCipherProvider.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/CbcCipherProvider.java").exists())

    def test_no_aes_config_keeps_des_provider_only(self):
        no_aes = dict(CONFIG)
        for key in (
            "key_aes_128", "key_aes_192", "key_aes_256",
            "cipher_aes_cbc_nopad", "cipher_getinstance_aes_cbc_nopad",
        ):
            no_aes[key] = False
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_generator(no_aes, out)
            self.assertEqual(result.returncode, 0, result.stderr)
            java_dir = out / "java/javacard/security"
            self.assertFalse((java_dir / "AESKeyProvider.java").exists())
            self.assertTrue((java_dir / "DESKeyProvider.java").is_file())
            self.assertNotIn("AESKeyProvider", (java_dir / "KeyBuilderProvider.java").read_text())
            cipher_dir = out / "java/javacardx/crypto"
            self.assertFalse((cipher_dir / "AESCipherProvider.java").exists())
            self.assertTrue((cipher_dir / "DESCipherProvider.java").is_file())
            self.assertNotIn("AESCipherProvider", (cipher_dir / "CipherProvider.java").read_text())

    def test_rejects_unknown_missing_non_boolean_and_orphan_cipher(self):
        variants = []
        unknown = dict(CONFIG, typo=True)
        variants.append((unknown, "unknown capability"))
        missing = dict(CONFIG)
        del missing["sha256"]
        variants.append((missing, "missing capability"))
        non_boolean = dict(CONFIG, sha256=1)
        variants.append((non_boolean, "must be booleans"))
        orphan = {key: False for key in CONFIG}
        orphan["cipher_aes_cbc_nopad"] = True
        variants.append((orphan, "requires at least one enabled AES key"))
        for value, diagnostic in variants:
            with self.subTest(diagnostic=diagnostic), tempfile.TemporaryDirectory() as temporary:
                out = Path(temporary) / "generated"
                result = self.run_generator(value, out)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic, result.stderr)

    def test_comparator_accepts_empty_expected_set_but_rejects_unexpected_yes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            expected = directory / "expected.csv"
            expected.write_text("section;algorithm_name\n")
            result = directory / "result.csv"
            result.write_text(
                "AlgTest applet version;1.8.2_jc305\n"
                "algorithm_name;is_supported\n"
                "Example\n"
                "ALGORITHM;no\n"
            )
            reference = directory / "reference.csv"
            reference.write_text(result.read_text())
            command = [sys.executable, str(COMPARATOR), str(result), str(reference), "--expected-support", str(expected)]
            passed = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(passed.returncode, 0, passed.stdout + passed.stderr)
            result.write_text(result.read_text().replace("ALGORITHM;no", "ALGORITHM;yes"))
            failed = subprocess.run(command, text=True, capture_output=True)
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn("CHOUPI:UNEXPECTED_SUPPORT", failed.stdout)


if __name__ == "__main__":
    unittest.main()
