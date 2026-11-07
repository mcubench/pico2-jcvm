#!/usr/bin/env python3
"""Focused tests for capability validation, metadata generation, and comparison."""

import csv
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "tools/javacard-support.py"
COMPARATOR = ROOT / "tools/compare-jcalgtest-support.py"
SUPPORT_CONFIG = json.loads((ROOT / "config/javacard-support.json").read_text())
REFERENCE = ROOT / "tests/fixtures/reference/cards/provided-20261107/NXP_JCOP4_P71_NoECC_ALGSUPPORT__3b_ea_00_00_81_31_fe_45_00_31_c1_73_c8_40_00_00_90_00_7a.csv"
CONFIG = SUPPORT_CONFIG["profiles"]["current"]


class JavaCardSupportTests(unittest.TestCase):
    def run_generator(self, config, out_dir, profile=None):
        config_path = out_dir.parent / "config.json"
        config_path.write_text(json.dumps(config))
        command = [sys.executable, str(GENERATOR), "--config", str(config_path)]
        if profile is not None:
            command.extend(("--profile", profile))
        command.extend(("--out-dir", str(out_dir)))
        return subprocess.run(command, text=True, capture_output=True)

    def run_named_profile(self, profile, out_dir):
        config_path = out_dir.parent / "profiles.json"
        config_path.write_text(json.dumps(SUPPORT_CONFIG))
        return subprocess.run(
            [sys.executable, str(GENERATOR), "--config", str(config_path),
             "--profile", profile, "--out-dir", str(out_dir)],
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
            native_methods = (out / "javacard-native-methods.txt").read_text().splitlines()
            self.assertIn("javacard/security|MessageDigest|MessageDigest/getInstance(BZ)Ljavacard/security/MessageDigest;|static", native_methods)
            self.assertIn("javacard/security|MessageDigest|MessageDigest/doFinal([BSS[BS)S|instance", native_methods)
            self.assertIn("javacard/security|Signature|Signature/getInstance(BZ)Ljavacard/security/Signature;|static", native_methods)
            self.assertIn("javacard/security|Signature|Signature/sign([BSS[BS)S|instance", native_methods)
            self.assertIn("javacard/security|Signature|Signature/beginVerify([BSS)S|instance", native_methods)
            self.assertIn("javacard/security|Signature|Signature/sign([BSS[BS[SS)S|instance", native_methods)
            self.assertIn("javacard/security|Signature|Signature/verify([BSS)Z|instance", native_methods)
            self.assertTrue((out / "java/javacard/security/SecureRandomData.java").is_file())
            self.assertFalse((out / "java/javacard/security/KeyBuilderProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/AESKeyProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/DESKeyProvider.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RSAKeyProvider.java").exists())
            native_methods = (out / "javacard-native-methods.txt").read_text()
            self.assertIn("javacard/security|KeyBuilder|KeyBuilder/buildKey(BSZ)Ljavacard/security/Key;|static", native_methods)
            self.assertIn("javacard/security|NativeKey|NativeKey/setModulus([BSS)V|instance", native_methods)
            self.assertIn("javacardx/crypto|Cipher|Cipher/getInstance(BZ)Ljavacardx/crypto/Cipher;|static", native_methods)
            self.assertIn("javacardx/crypto|Cipher|Cipher/doFinal([BSS[BS)S|instance", native_methods)
            for implementation in (
                    "MessageDigestProvider.java", "Sha256DigestProvider.java",
                    "MbedTlsDigestProvider.java"):
                self.assertFalse((out / "java/javacard/security" / implementation).exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/Sha256NativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/MbedTlsDigestNativeBridge.java").exists())
            self.assertFalse((out / "java/javacard/security/SignatureProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/SymmetricMacProvider.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/MacNativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/CbcCipherNativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RSACipherProvider.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/CipherProvider.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/SymmetricCipherProvider.java").exists())
            self.assertTrue((out / "java/fr/gouv/ssi/nativeimpl/RandomDataNativeBridge.java").is_file())
            self.assertTrue((out / "java/javacard/framework/OwnerPINxBase.java").is_file())
            self.assertTrue((out / "java/javacard/framework/OwnerPINxImpl.java").is_file())
            self.assertTrue((out / "java/javacard/framework/OwnerPINxWithPredecrementImpl.java").is_file())

    def test_regenerating_same_profile_preserves_generated_input_timestamps(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            first = self.run_named_profile("asymm_crypto", out)
            self.assertEqual(first.returncode, 0, first.stderr)
            generated_inputs = (
                out / "javacard-support.cmake",
                out / "javacard-support.h",
                out / "javacard-support.json",
                out / "javacard-native-methods.txt",
                out / "profile.txt",
                out / "jcalgtest-supported.csv",
                out / "java/javacard/security/RandomDataProvider.java",
            )
            for path in generated_inputs:
                self.assertTrue(path.is_file(), path)
                stat = path.stat()
                os.utime(path, ns=(stat.st_atime_ns, stat.st_mtime_ns - 2_000_000_000))
            original_mtimes = {path: path.stat().st_mtime_ns for path in generated_inputs}

            second = self.run_named_profile("asymm_crypto", out)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(
                {path: path.stat().st_mtime_ns for path in generated_inputs},
                original_mtimes,
            )

    def test_minimal_config_generates_empty_supported_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_generator({key: False for key in CONFIG}, out)
            self.assertEqual(result.returncode, 0, result.stderr)
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                self.assertEqual(list(csv.reader(source, delimiter=";")), [["section", "algorithm_name"]])
            self.assertIn("#define JC_SUPPORT_SHA256 0", (out / "javacard-support.h").read_text())
            self.assertFalse((out / "java/javacard/security/SecureRandomData.java").exists())
            self.assertFalse((out / "java/javacard/security/KeyBuilderProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/AESKeyProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/DESKeyProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/Sha256DigestProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/MessageDigestProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/MbedTlsDigestProvider.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/Sha256NativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/MbedTlsDigestNativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/CbcCipherNativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RandomDataNativeBridge.java").exists())
            self.assertFalse((out / "java/javacard/framework/OwnerPINxBase.java").exists())
            self.assertFalse((out / "java/javacard/framework/OwnerPINxImpl.java").exists())
            self.assertFalse((out / "java/javacard/framework/OwnerPINxWithPredecrementImpl.java").exists())

    def test_base_profile_matches_ten_declared_capabilities(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_named_profile("base", out)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("profile=base", result.stdout)
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                rows = list(csv.reader(source, delimiter=";"))
            self.assertEqual({tuple(row) for row in rows[1:]}, {
                ("javacard.framework.OwnerPINBuilder", "OWNER_PIN"),
                ("javacard.framework.OwnerPINBuilder", "OWNER_PIN_X"),
                ("javacard.framework.OwnerPINBuilder", "OWNER_PIN_X_WITH_PREDECREMENT"),
                ("javacard.security.KeyBuilder", "TYPE_AES LENGTH_AES_128"),
                ("javacard.security.KeyBuilder", "TYPE_DES LENGTH_DES3_3KEY"),
                ("javacard.security.RandomData", "ALG_SECURE_RANDOM"),
                ("javacardx.crypto.Cipher", "ALG_AES_BLOCK_128_CBC_NOPAD"),
                ("javacardx.crypto.Cipher", "ALG_DES_CBC_NOPAD"),
                ("javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)", "CIPHER_AES_CBC PAD_NOPAD"),
                ("javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)", "CIPHER_DES_CBC PAD_NOPAD"),
            })
            self.assertEqual((out / "profile.txt").read_text(), "base\n")

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
            self.assertFalse((java_dir / "DESKeyProvider.java").exists())
            self.assertFalse((java_dir / "KeyBuilderProvider.java").exists())
            cipher_dir = out / "java/javacardx/crypto"
            self.assertFalse((cipher_dir / "CipherProvider.java").exists())
            self.assertFalse((cipher_dir / "SymmetricCipherProvider.java").exists())
            native_methods = (out / "javacard-native-methods.txt").read_text()
            self.assertIn("javacardx/crypto|Cipher|Cipher/init(Ljavacard/security/Key;B)V|instance", native_methods)

    def test_symm_crypto_generates_reference_symmetric_capabilities(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_named_profile("symm_crypto", out)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((out / "profile.txt").read_text(), "symm_crypto\n")
            self.assertFalse((out / "java/javacard/security/KeyBuilderProvider.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RSAKeyProvider.java").exists())
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                rows = {tuple(row) for row in list(csv.reader(source, delimiter=";"))[1:]}
            symmetric = {
                row for row in rows
                if row[0] in ("javacardx.crypto.Cipher", "javacard.security.Signature",
                              "javacard.security.KeyBuilder")
            }
            self.assertEqual(sum(row[0] == "javacardx.crypto.Cipher" for row in symmetric), 16)
            self.assertEqual(sum(row[0] == "javacard.security.Signature" for row in symmetric), 13)
            self.assertTrue({
                ("javacard.security.KeyBuilder", "TYPE_AES_TRANSIENT_RESET"),
                ("javacard.security.KeyBuilder", "TYPE_AES_TRANSIENT_DESELECT"),
                ("javacard.security.KeyBuilder", "TYPE_DES_TRANSIENT_RESET"),
                ("javacard.security.KeyBuilder", "TYPE_DES_TRANSIENT_DESELECT"),
            }.issubset(symmetric))
            self.assertFalse((out / "java/javacard/security/SymmetricMacProvider.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/MacNativeBridge.java").exists())

            expected = set()
            with REFERENCE.open(newline="", encoding="utf-8") as source:
                section = ""
                for row in csv.reader(source, delimiter=";"):
                    if len(row) == 1:
                        section = row[0]
                        if section == "javacard.crypto.Signature":
                            section = "javacard.security.Signature"
                    elif len(row) >= 2 and row[1].strip().lower() == "yes" and section in {
                        "javacardx.crypto.Cipher", "javacard.security.Signature",
                        "javacard.security.KeyBuilder",
                    }:
                        name = row[0]
                        symmetric_row = (
                            section == "javacardx.crypto.Cipher" and name.startswith(("ALG_AES_", "ALG_DES_")) or
                            section == "javacard.security.Signature" and (
                                name.startswith("ALG_DES_MAC") or
                                name in {"ALG_AES_MAC_128_NOPAD", "ALG_AES_CMAC_128"}
                            ) or
                            section == "javacard.security.KeyBuilder" and name.startswith(("TYPE_AES", "TYPE_DES"))
                        )
                        if symmetric_row:
                            expected.add((section, name))
            actual = {row for row in rows if row[0] in {
                "javacardx.crypto.Cipher", "javacard.security.Signature",
                "javacard.security.KeyBuilder",
            }}
            self.assertEqual(actual, expected)

    def test_symm_crypto_ctr_is_separately_selectable(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_named_profile("symm_crypto_ctr", out)
            self.assertEqual(result.returncode, 0, result.stderr)
            profile = json.loads((out / "javacard-support.json").read_text())
            self.assertTrue(profile["cipher_aes_ctr"])
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                rows = {tuple(row) for row in list(csv.reader(source, delimiter=";"))[1:]}
            self.assertIn(("javacardx.crypto.Cipher", "ALG_AES_CTR"), rows)
            self.assertIn((
                "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)",
                "CIPHER_AES_CTR PAD_NOPAD",
            ), rows)
            self.assertEqual(len(rows), 48)

    def test_max_crypto_profile_is_union_of_supported_crypto_profiles(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_named_profile("max_crypto", out)
            self.assertEqual(result.returncode, 0, result.stderr)
            profile = json.loads((out / "javacard-support.json").read_text())
            self.assertEqual(sum(profile.values()), 159)
            for key in (
                "signature_hmac_sha1", "signature_hmac_sha_256",
                "signature_hmac_sha_384", "signature_hmac_sha_512",
                "signature_hmac_md5", "signature_hmac_ripemd160",
                "key_hmac_transient_reset", "key_hmac_transient_deselect",
                "key_hmac_sha1_block_64", "key_hmac_sha256_block_64",
                "key_hmac_sha384_block_64", "key_hmac_sha512_block_64",
            ):
                self.assertTrue(profile[key])
            self.assertTrue(profile["cipher_aes_ctr"])
            self.assertTrue(profile["cipher_getinstance_aes_ctr"])
            self.assertTrue(profile["signature_ecdsa_sha_512"])
            self.assertTrue(profile["key_rsa_public_4096"])
            self.assertFalse(profile["key_ec_fp_private_112"])
            self.assertFalse(profile["keypair_rsa_4096"])
            self.assertFalse(profile["keypair_rsa_crt_4096"])

    def test_ctr_pair_factory_can_be_selected_without_single_factory(self):
        pair_only = {key: False for key in CONFIG}
        pair_only["key_aes_128"] = True
        pair_only["cipher_getinstance_aes_ctr"] = True
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_generator(pair_only, out)
            self.assertEqual(result.returncode, 0, result.stderr)
            header = (out / "javacard-support.h").read_text()
            self.assertIn("#define JC_SUPPORT_CIPHER_AES_CTR 0", header)
            self.assertIn("#define JC_SUPPORT_CIPHER_GETINSTANCE_AES_CTR 1", header)
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                rows = {tuple(row) for row in list(csv.reader(source, delimiter=";"))[1:]}
            self.assertNotIn(("javacardx.crypto.Cipher", "ALG_AES_CTR"), rows)
            self.assertIn((
                "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)",
                "CIPHER_AES_CTR PAD_NOPAD",
            ), rows)

    def test_asymm_crypto_selects_native_digest_without_java_provider(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_named_profile("asymm_crypto", out)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((out / "profile.txt").read_text(), "asymm_crypto\n")
            profile = json.loads((out / "javacard-support.json").read_text())
            for bits in (512, 736, 768, 896, 1024, 1280, 1536, 1984, 2048, 3072):
                self.assertTrue(profile[f"keypair_rsa_{bits}"])
                self.assertTrue(profile[f"keypair_rsa_crt_{bits}"])
            self.assertFalse(profile["keypair_rsa_4096"])
            self.assertFalse(profile["keypair_rsa_crt_4096"])
            with (out / "jcalgtest-supported.csv").open(newline="") as source:
                generated = {tuple(row) for row in list(csv.reader(source, delimiter=";"))[1:]}
            for bits in (512, 736, 768, 896, 1024, 1280, 1536, 1984, 2048, 3072):
                self.assertIn(("javacard.security.KeyPair", f"ALG_RSA LENGTH_RSA_{bits}"), generated)
                self.assertIn(("javacard.security.KeyPair", f"ALG_RSA_CRT LENGTH_RSA_{bits}"), generated)
            self.assertTrue(profile["key_ec_fp_private_transient_reset"])
            self.assertTrue(profile["key_ec_fp_private_transient_deselect"])
            for bits in (128, 160, 192, 224, 256, 320, 384, 512, 521):
                self.assertTrue(profile[f"key_ec_fp_private_{bits}"])
            for generated_class in (
                    "MessageDigestProvider.java", "Sha256DigestProvider.java",
                    "MbedTlsDigestProvider.java"):
                self.assertFalse((out / "java/javacard/security" / generated_class).exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/Sha256NativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/MbedTlsDigestNativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RSAKeyProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/KeyBuilderProvider.java").exists())
            native_methods = (out / "javacard-native-methods.txt").read_text()
            self.assertIn("javacard/security|KeyBuilder|KeyBuilder/buildKey(BSZ)Ljavacard/security/Key;|static", native_methods)
            self.assertIn("javacard/security|NativeKey|NativeKey/getModulus([BS)S|instance", native_methods)
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RSASignatureProvider.java").exists())
            self.assertFalse((out / "java/javacard/security/SignatureProvider.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RSACipherProvider.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/RSACipherProvider.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/RsaCipherNativeBridge.java").exists())
            self.assertFalse((out / "java/fr/gouv/ssi/nativeimpl/CbcCipherNativeBridge.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/CipherProvider.java").exists())
            self.assertFalse((out / "java/javacardx/crypto/SymmetricCipherProvider.java").exists())
            header = (out / "javacard-support.h").read_text()
            for capability in ("SHA1", "SHA224", "SHA384", "SHA512"):
                self.assertIn(f"JC_SUPPORT_{capability} 1", header)
            self.assertIn("javacard.security.Signature;ALG_RSA_SHA_ISO9796",
                          (out / "jcalgtest-supported.csv").read_text())
            self.assertIn("javacard.security.Signature;ALG_RSA_SHA_ISO9796_MR",
                          (out / "jcalgtest-supported.csv").read_text())

    def test_rsa_keypair_capability_requires_matching_key_capabilities(self):
        invalid = {key: False for key in CONFIG}
        invalid["keypair_rsa_512"] = True
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "generated"
            result = self.run_generator(invalid, out)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("requires matching public and private RSA keys", result.stderr)

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
        orphan_ctr = {key: False for key in CONFIG}
        orphan_ctr["cipher_aes_ctr"] = True
        variants.append((orphan_ctr, "requires at least one enabled AES key"))
        orphan_ctr_pair = {key: False for key in CONFIG}
        orphan_ctr_pair["cipher_getinstance_aes_ctr"] = True
        variants.append((orphan_ctr_pair, "requires at least one enabled AES key"))
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
                "AlgTest applet version;1.8.4_jc305\n"
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

    def test_comparator_canonicalizes_crypto_factory_aliases(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            expected = directory / "expected.csv"
            expected.write_text(
                "section;algorithm_name\n"
                "javacardx.crypto.Cipher;ALG_AES_BLOCK_128_CBC_NOPAD\n"
                "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess);CIPHER_AES_CBC PAD_NOPAD\n"
                "javacard.security.Signature;ALG_AES_MAC_128_NOPAD\n"
            )
            result = directory / "result.csv"
            result.write_text(
                "AlgTest applet version;1.8.4_jc305\n"
                "algorithm_name;is_supported\n"
                "javacardx.crypto.Cipher\n"
                "ALG_AES_BLOCK_128_CBC_NOPAD;yes\n"
                "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)\n"
                "CIPHER_AES_CBC PAD_NOPAD;yes\n"
                "javacard.crypto.Signature\n"
                "ALG_AES_MAC_128_NOPAD;yes\n"
                "javacard.crypto.Signature.getInstance(byte messageDigestAlgorithm, byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)\n"
                "SIG_CIPHER_AES_MAC128 PAD_NOPAD ALG_NULL;yes\n"
            )
            reference = directory / "reference.csv"
            reference.write_text(result.read_text())
            command = [
                sys.executable, str(COMPARATOR), str(result), str(reference),
                "--expected-support", str(expected),
            ]
            compared = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(compared.returncode, 0, compared.stdout + compared.stderr)
            self.assertIn("COMPARE:PASS rows_statuses_and_declared_support", compared.stdout)


if __name__ == "__main__":
    unittest.main()
