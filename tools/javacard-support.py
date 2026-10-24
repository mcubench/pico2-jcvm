#!/usr/bin/env python3
"""Validate Java Card firmware capabilities and emit build/support metadata."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import sys
from typing import Any


CAPABILITIES: dict[str, tuple[str, str]] = {
    "owner_pin": ("javacard.framework.OwnerPINBuilder", "OWNER_PIN"),
    "owner_pin_x": ("javacard.framework.OwnerPINBuilder", "OWNER_PIN_X"),
    "owner_pin_x_with_predecrement": (
        "javacard.framework.OwnerPINBuilder", "OWNER_PIN_X_WITH_PREDECREMENT"
    ),
    "key_aes_128": ("javacard.security.KeyBuilder", "TYPE_AES LENGTH_AES_128"),
    "key_aes_192": ("javacard.security.KeyBuilder", "TYPE_AES LENGTH_AES_192"),
    "key_aes_256": ("javacard.security.KeyBuilder", "TYPE_AES LENGTH_AES_256"),
    "key_des": ("javacard.security.KeyBuilder", "TYPE_DES LENGTH_DES"),
    "key_des3_2key": ("javacard.security.KeyBuilder", "TYPE_DES LENGTH_DES3_2KEY"),
    "key_des3_3key": ("javacard.security.KeyBuilder", "TYPE_DES LENGTH_DES3_3KEY"),
    "sha256": ("javacard.security.MessageDigest", "ALG_SHA_256"),
    "sha1": ("javacard.security.MessageDigest", "ALG_SHA"),
    "sha224": ("javacard.security.MessageDigest", "ALG_SHA_224"),
    "sha384": ("javacard.security.MessageDigest", "ALG_SHA_384"),
    "sha512": ("javacard.security.MessageDigest", "ALG_SHA_512"),
    "cipher_rsa_pkcs1": ("javacardx.crypto.Cipher", "ALG_RSA_PKCS1"),
    "cipher_rsa_nopad": ("javacardx.crypto.Cipher", "ALG_RSA_NOPAD"),
    "cipher_rsa_oaep": ("javacardx.crypto.Cipher", "ALG_RSA_PKCS1_OAEP"),
    "secure_random": ("javacard.security.RandomData", "ALG_SECURE_RANDOM"),
    "cipher_aes_cbc_nopad": (
        "javacardx.crypto.Cipher", "ALG_AES_BLOCK_128_CBC_NOPAD"
    ),
    "cipher_des_cbc_nopad": ("javacardx.crypto.Cipher", "ALG_DES_CBC_NOPAD"),
    "cipher_getinstance_aes_cbc_nopad": (
        "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)",
        "CIPHER_AES_CBC PAD_NOPAD",
    ),
    "cipher_getinstance_des_cbc_nopad": (
        "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)",
        "CIPHER_DES_CBC PAD_NOPAD",
    ),
}

for _family, _rows in {
    "des": {
        "cipher_des_cbc_iso9797_m1": "ALG_DES_CBC_ISO9797_M1",
        "cipher_des_cbc_iso9797_m2": "ALG_DES_CBC_ISO9797_M2",
        "cipher_des_cbc_pkcs5": "ALG_DES_CBC_PKCS5",
        "cipher_des_ecb_nopad": "ALG_DES_ECB_NOPAD",
        "cipher_des_ecb_iso9797_m1": "ALG_DES_ECB_ISO9797_M1",
        "cipher_des_ecb_iso9797_m2": "ALG_DES_ECB_ISO9797_M2",
        "cipher_des_ecb_pkcs5": "ALG_DES_ECB_PKCS5",
    },
    "aes": {
        "cipher_aes_ecb_nopad": "ALG_AES_BLOCK_128_ECB_NOPAD",
        "cipher_aes_cbc_iso9797_m1": "ALG_AES_CBC_ISO9797_M1",
        "cipher_aes_cbc_iso9797_m2": "ALG_AES_CBC_ISO9797_M2",
        "cipher_aes_cbc_pkcs5": "ALG_AES_CBC_PKCS5",
        "cipher_aes_ecb_iso9797_m1": "ALG_AES_ECB_ISO9797_M1",
        "cipher_aes_ecb_iso9797_m2": "ALG_AES_ECB_ISO9797_M2",
        "cipher_aes_ecb_pkcs5": "ALG_AES_ECB_PKCS5",
    },
}.items():
    for _key, _algorithm in _rows.items():
        CAPABILITIES[_key] = ("javacardx.crypto.Cipher", _algorithm)

for _family in ("aes", "des"):
    for _lifetime, _suffix in (("transient_reset", "TRANSIENT_RESET"),
                               ("transient_deselect", "TRANSIENT_DESELECT")):
        CAPABILITIES[f"key_{_family}_{_lifetime}"] = (
            "javacard.security.KeyBuilder", f"TYPE_{_family.upper()}_{_suffix}"
        )

for _name in (
    "DES_MAC8_NOPAD",
    "DES_MAC4_ISO9797_M1", "DES_MAC8_ISO9797_M1",
    "DES_MAC4_ISO9797_M2", "DES_MAC8_ISO9797_M2",
    "DES_MAC4_PKCS5", "DES_MAC8_PKCS5",
    "AES_MAC_128_NOPAD",
    "DES_MAC4_ISO9797_1_M2_ALG3", "DES_MAC8_ISO9797_1_M2_ALG3",
    "DES_MAC4_ISO9797_1_M1_ALG3", "DES_MAC8_ISO9797_1_M1_ALG3",
    "AES_CMAC_128",
):
    CAPABILITIES[f"signature_{_name.lower()}"] = (
        "javacard.security.Signature", f"ALG_{_name}"
    )

_rsa_lengths = (512, 736, 768, 896, 1024, 1280, 1536, 1984, 2048, 3072, 4096)
for _key_type, _row_type in (
    ("public", "RSA_PUBLIC"),
    ("private", "RSA_PRIVATE"),
    ("crt_private", "RSA_CRT_PRIVATE"),
):
    for _bits in _rsa_lengths:
        _key = f"key_rsa_{_key_type}_{_bits}"
        CAPABILITIES[_key] = (
            "javacard.security.KeyBuilder",
            f"TYPE_{_row_type} LENGTH_RSA_{_bits}",
        )
for _key_type, _row_type in (
    ("private", "RSA_PRIVATE"),
    ("crt_private", "RSA_CRT_PRIVATE"),
):
    for _lifetime in ("transient_reset", "transient_deselect"):
        _key = f"key_rsa_{_key_type}_{_lifetime}"
        _suffix = _lifetime.upper()
        CAPABILITIES[_key] = (
            "javacard.security.KeyBuilder", f"TYPE_{_row_type}_{_suffix}"
        )


def validate_capabilities(value: Any, path: Path) -> dict[str, bool]:
    if not isinstance(value, dict):
        raise ValueError(f"{path}: profile must be an object")
    unknown = sorted(set(value) - set(CAPABILITIES))
    missing = sorted(set(CAPABILITIES) - set(value))
    if unknown:
        raise ValueError(f"{path}: unknown capability keys: {', '.join(unknown)}")
    if missing:
        raise ValueError(f"{path}: missing capability keys: {', '.join(missing)}")
    invalid = sorted(key for key, enabled in value.items() if type(enabled) is not bool)
    if invalid:
        raise ValueError(f"{path}: capability values must be booleans: {', '.join(invalid)}")

    aes_key = any(value[key] for key in (
        "key_aes_128", "key_aes_192", "key_aes_256",
        "key_aes_transient_reset", "key_aes_transient_deselect",
    ))
    des_key = any(value[key] for key in (
        "key_des", "key_des3_2key", "key_des3_3key",
        "key_des_transient_reset", "key_des_transient_deselect",
    ))
    aes_cipher = any(key.startswith("cipher_aes_") and enabled
                     for key, enabled in value.items())
    des_cipher = any(key.startswith("cipher_des_") and enabled
                     for key, enabled in value.items())
    aes_mac = any(key.startswith("signature_aes_") and enabled
                  for key, enabled in value.items())
    des_mac = any(key.startswith("signature_des_") and enabled
                  for key, enabled in value.items())
    if not aes_key and (aes_cipher or aes_mac):
        raise ValueError(f"{path}: AES CBC requires at least one enabled AES key capability")
    if not des_key and (des_cipher or des_mac):
        raise ValueError(f"{path}: DES CBC requires at least one enabled DES key capability")
    rsa_cipher = any(value[key] for key in (
        "cipher_rsa_pkcs1", "cipher_rsa_nopad", "cipher_rsa_oaep"))
    rsa_public = any(value[f"key_rsa_public_{bits}"] for bits in _rsa_lengths)
    rsa_private = any(value[f"key_rsa_private_{bits}"] for bits in _rsa_lengths)
    if rsa_cipher and (not rsa_public or not rsa_private):
        raise ValueError(f"{path}: RSA ciphers require enabled public and private RSA key capabilities")
    if value["cipher_rsa_oaep"] and not value["sha1"]:
        raise ValueError(f"{path}: RSA OAEP requires SHA-1 support")
    return value


def load_config(path: Path, requested_profile: str | None = None) -> tuple[str, dict[str, bool]]:
    try:
        value: Any = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"{path}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"{path}: top-level JSON value must be an object")

    if "profiles" not in value:
        if requested_profile not in (None, "current"):
            raise ValueError(f"{path}: flat configuration supports only the current profile")
        return "current", validate_capabilities(value, path)

    unknown_fields = sorted(set(value) - {"default_profile", "profiles"})
    if unknown_fields:
        raise ValueError(f"{path}: unknown configuration keys: {', '.join(unknown_fields)}")
    profiles = value["profiles"]
    if not isinstance(profiles, dict) or not profiles:
        raise ValueError(f"{path}: profiles must be a non-empty object")
    invalid_names = sorted(name for name in profiles if not isinstance(name, str) or not name)
    if invalid_names:
        raise ValueError(f"{path}: profile names must be non-empty strings")
    default_profile = value.get("default_profile")
    if not isinstance(default_profile, str) or default_profile not in profiles:
        raise ValueError(f"{path}: default_profile must name a configured profile")
    profile = requested_profile or default_profile
    if profile not in profiles:
        raise ValueError(f"{path}: unknown profile '{profile}'; choose from {', '.join(sorted(profiles))}")
    return profile, validate_capabilities(profiles[profile], path)


def emit(config: dict[str, bool], out_dir: Path, profile: str) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    with (out_dir / "jcalgtest-supported.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output, delimiter=";", lineterminator="\n")
        writer.writerow(("section", "algorithm_name"))
        for key, row in CAPABILITIES.items():
            if config[key]:
                writer.writerow(row)

    cmake = ["# Generated by tools/javacard-support.py; do not edit.",
             f'set(JC_SUPPORT_PROFILE "{profile}")']
    header = ["/* Generated by tools/javacard-support.py; do not edit. */", "#pragma once",
              f'#define JC_SUPPORT_PROFILE "{profile}"']
    for key, enabled in config.items():
        cmake.append(f"set(JC_SUPPORT_{key.upper()} {'ON' if enabled else 'OFF'})")
        header.append(f"#define JC_SUPPORT_{key.upper()} {1 if enabled else 0}")
    (out_dir / "javacard-support.cmake").write_text("\n".join(cmake) + "\n", encoding="utf-8")
    (out_dir / "javacard-support.h").write_text("\n".join(header) + "\n", encoding="utf-8")
    (out_dir / "javacard-support.json").write_text(
        json.dumps(config, indent=2) + "\n", encoding="utf-8"
    )
    (out_dir / "profile.txt").write_text(profile + "\n", encoding="utf-8")
    java_dir = out_dir / "java/javacard/security"
    java_dir.mkdir(parents=True, exist_ok=True)
    nativeimpl_dir = out_dir / "java/fr/gouv/ssi/nativeimpl"
    nativeimpl_dir.mkdir(parents=True, exist_ok=True)
    random_provider = [
        "package javacard.security;",
        "",
        "final class RandomDataProvider {",
        "  private RandomDataProvider() {}",
        "  static RandomData getInstance(byte algorithm) throws CryptoException {",
    ]
    if config["secure_random"]:
        random_provider.extend([
            "    if (algorithm == RandomData.ALG_SECURE_RANDOM) {",
            "      return new SecureRandomData();",
            "    }",
        ])
    random_provider.extend([
        "    CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
        "    return null;",
        "  }",
        "}",
    ])
    (java_dir / "RandomDataProvider.java").write_text(
        "\n".join(random_provider) + "\n", encoding="utf-8"
    )
    if config["secure_random"]:
        template = Path(__file__).resolve().parent / "java-support/SecureRandomData.java"
        provider_path = java_dir / "SecureRandomData.java"
        provider_path.write_text(template.read_text(encoding="utf-8"), encoding="utf-8")
        (out_dir / "java/fr/gouv/ssi/nativeimpl/RandomDataNativeBridge.java").write_text(
            (Path(__file__).resolve().parent / "java-support/RandomDataNativeBridge.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        (java_dir / "SecureRandomData.java").unlink(missing_ok=True)
        (out_dir / "java/fr/gouv/ssi/nativeimpl/RandomDataNativeBridge.java").unlink(missing_ok=True)
    key_provider = [
        "package javacard.security;",
        "",
        "import fr.gouv.ssi.nativeimpl.RSAKeyProvider;",
        "",
        "final class KeyBuilderProvider {",
        "  private KeyBuilderProvider() {}",
        "  static Key buildKey(byte type, short length, boolean encrypted)",
        "      throws CryptoException {",
        "    if (encrypted) CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
    ]
    aes_lengths = {
        "key_aes_128": "KeyBuilder.LENGTH_AES_128",
        "key_aes_192": "KeyBuilder.LENGTH_AES_192",
        "key_aes_256": "KeyBuilder.LENGTH_AES_256",
    }
    des_lengths = {
        "key_des": "KeyBuilder.LENGTH_DES",
        "key_des3_2key": "KeyBuilder.LENGTH_DES3_2KEY",
        "key_des3_3key": "KeyBuilder.LENGTH_DES3_3KEY",
    }
    aes_types = {
        "key_aes_128": ("KeyBuilder.TYPE_AES", "KeyBuilder.LENGTH_AES_128"),
        "key_aes_192": ("KeyBuilder.TYPE_AES", "KeyBuilder.LENGTH_AES_192"),
        "key_aes_256": ("KeyBuilder.TYPE_AES", "KeyBuilder.LENGTH_AES_256"),
    }
    des_types = {
        "key_des": ("KeyBuilder.TYPE_DES", "KeyBuilder.LENGTH_DES"),
        "key_des3_2key": ("KeyBuilder.TYPE_DES", "KeyBuilder.LENGTH_DES3_2KEY"),
        "key_des3_3key": ("KeyBuilder.TYPE_DES", "KeyBuilder.LENGTH_DES3_3KEY"),
    }
    for family, length_rows, types in (("AES", aes_lengths, aes_types),
                                       ("DES", des_lengths, des_types)):
        transient_rows = {
            "key_" + family.lower() + "_transient_reset":
                f"KeyBuilder.TYPE_{family}_TRANSIENT_RESET",
            "key_" + family.lower() + "_transient_deselect":
                f"KeyBuilder.TYPE_{family}_TRANSIENT_DESELECT",
        }
        for key, memory_type in transient_rows.items():
            if config[key]:
                for length_key, (_, length) in types.items():
                    if config[length_key]:
                        key_provider.extend([
                            f"    if (type == {memory_type} && length == {length})",
                            f"      return new {family}KeyProvider(type, length);",
                        ])
        for key, (memory_type, length) in types.items():
            if config[key]:
                key_provider.extend([
                    f"    if (type == {memory_type} && length == {length})",
                    f"      return new {family}KeyProvider(type, length);",
                ])
    for kind, type_name in (
        ("public", "TYPE_RSA_PUBLIC"),
        ("private", "TYPE_RSA_PRIVATE"),
        ("crt_private", "TYPE_RSA_CRT_PRIVATE"),
    ):
        for bits in _rsa_lengths:
            key = f"key_rsa_{kind}_{bits}"
            if config[key]:
                key_provider.extend([
                    f"    if (type == KeyBuilder.{type_name} && length == KeyBuilder.LENGTH_RSA_{bits})",
                    "      return new RSAKeyProvider(type, length);",
                ])
    for kind, type_name in (
        ("private", "TYPE_RSA_PRIVATE"),
        ("crt_private", "TYPE_RSA_CRT_PRIVATE"),
    ):
        for life, suffix in (("transient_reset", "TRANSIENT_RESET"),
                             ("transient_deselect", "TRANSIENT_DESELECT")):
            capability = f"key_rsa_{kind}_{life}"
            if not config[capability]:
                continue
            transient_type = f"{type_name}_{suffix}"
            for bits in _rsa_lengths:
                if config[f"key_rsa_{kind}_{bits}"]:
                    key_provider.extend([
                        f"    if (type == KeyBuilder.{transient_type} && length == KeyBuilder.LENGTH_RSA_{bits})",
                        "      return new RSAKeyProvider(type, length);",
                    ])
    key_provider.extend([
        "    CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
        "    return null;",
        "  }",
        "}",
    ])
    (java_dir / "KeyBuilderProvider.java").write_text(
        "\n".join(key_provider) + "\n", encoding="utf-8"
    )
    key_base = Path(__file__).resolve().parent / "java-support/KeyProviderBase.java"
    (java_dir / "KeyProviderBase.java").unlink(missing_ok=True)
    (java_dir / "AESKeyProvider.java").unlink(missing_ok=True)
    (java_dir / "DESKeyProvider.java").unlink(missing_ok=True)
    if any(config[key] for key in (*aes_lengths, "key_aes_transient_reset",
                                    "key_aes_transient_deselect")):
        (java_dir / "AESKeyProvider.java").write_text(
            (Path(__file__).resolve().parent / "java-support/AESKeyProvider.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    if any(config[key] for key in (*des_lengths, "key_des_transient_reset",
                                    "key_des_transient_deselect")):
        (java_dir / "DESKeyProvider.java").write_text(
            (Path(__file__).resolve().parent / "java-support/DESKeyProvider.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    if any(config[key] for key in (*aes_lengths, *des_lengths,
                                   "key_aes_transient_reset",
                                   "key_aes_transient_deselect",
                                   "key_des_transient_reset",
                                   "key_des_transient_deselect")):
        (java_dir / "KeyProviderBase.java").write_text(
            key_base.read_text(encoding="utf-8"), encoding="utf-8"
        )
    rsa_key_provider = out_dir / "java/fr/gouv/ssi/nativeimpl/RSAKeyProvider.java"
    rsa_key_enabled = any(key.startswith("key_rsa_") and enabled
                          for key, enabled in config.items())
    (java_dir / "RSAKeyProvider.java").unlink(missing_ok=True)
    if rsa_key_enabled:
        rsa_key_provider.parent.mkdir(parents=True, exist_ok=True)
        rsa_key_provider.write_text(
            (Path(__file__).resolve().parent / "java-support/RSAKeyProvider.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        rsa_key_provider.unlink(missing_ok=True)
    cipher_dir = out_dir / "java/javacardx/crypto"
    cipher_dir.mkdir(parents=True, exist_ok=True)
    cipher_provider = [
        "package javacardx.crypto;",
        "",
        "import javacard.security.CryptoException;",
        "",
        "final class CipherProvider {",
        "  private CipherProvider() {}",
        "  static Cipher getInstance(byte algorithm, boolean externalAccess)",
        "      throws CryptoException {",
        "    if (externalAccess) CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
    ]
    cipher_specs = {
        "cipher_des_cbc_nopad": ("ALG_DES_CBC_NOPAD", "CIPHER_DES_CBC", "PAD_NOPAD", False, True),
        "cipher_des_cbc_iso9797_m1": ("ALG_DES_CBC_ISO9797_M1", "CIPHER_DES_CBC", "PAD_ISO9797_M1", False, True),
        "cipher_des_cbc_iso9797_m2": ("ALG_DES_CBC_ISO9797_M2", "CIPHER_DES_CBC", "PAD_ISO9797_M2", False, True),
        "cipher_des_cbc_pkcs5": ("ALG_DES_CBC_PKCS5", "CIPHER_DES_CBC", "PAD_PKCS5", False, True),
        "cipher_des_ecb_nopad": ("ALG_DES_ECB_NOPAD", "CIPHER_DES_ECB", "PAD_NOPAD", False, False),
        "cipher_des_ecb_iso9797_m1": ("ALG_DES_ECB_ISO9797_M1", "CIPHER_DES_ECB", "PAD_ISO9797_M1", False, False),
        "cipher_des_ecb_iso9797_m2": ("ALG_DES_ECB_ISO9797_M2", "CIPHER_DES_ECB", "PAD_ISO9797_M2", False, False),
        "cipher_des_ecb_pkcs5": ("ALG_DES_ECB_PKCS5", "CIPHER_DES_ECB", "PAD_PKCS5", False, False),
        "cipher_aes_cbc_nopad": ("ALG_AES_BLOCK_128_CBC_NOPAD", "CIPHER_AES_CBC", "PAD_NOPAD", True, True),
        "cipher_aes_ecb_nopad": ("ALG_AES_BLOCK_128_ECB_NOPAD", "CIPHER_AES_ECB", "PAD_NOPAD", True, False),
        "cipher_aes_cbc_iso9797_m1": ("ALG_AES_CBC_ISO9797_M1", "CIPHER_AES_CBC", "PAD_ISO9797_M1", True, True),
        "cipher_aes_cbc_iso9797_m2": ("ALG_AES_CBC_ISO9797_M2", "CIPHER_AES_CBC", "PAD_ISO9797_M2", True, True),
        "cipher_aes_cbc_pkcs5": ("ALG_AES_CBC_PKCS5", "CIPHER_AES_CBC", "PAD_PKCS5", True, True),
        "cipher_aes_ecb_iso9797_m1": ("ALG_AES_ECB_ISO9797_M1", "CIPHER_AES_ECB", "PAD_ISO9797_M1", True, False),
        "cipher_aes_ecb_iso9797_m2": ("ALG_AES_ECB_ISO9797_M2", "CIPHER_AES_ECB", "PAD_ISO9797_M2", True, False),
        "cipher_aes_ecb_pkcs5": ("ALG_AES_ECB_PKCS5", "CIPHER_AES_ECB", "PAD_PKCS5", True, False),
    }
    for key, (alg, cipher, pad, aes, cbc) in cipher_specs.items():
        if config[key]:
            cipher_provider.append(
                f"    if (algorithm == Cipher.{alg}) return new SymmetricCipherProvider(Cipher.{alg}, Cipher.{cipher}, Cipher.{pad}, {str(aes).lower()}, {str(cbc).lower()});"
            )
    rsa_cipher_specs = {
        "cipher_rsa_pkcs1": ("ALG_RSA_PKCS1", "PAD_PKCS1"),
        "cipher_rsa_nopad": ("ALG_RSA_NOPAD", "PAD_NOPAD"),
        "cipher_rsa_oaep": ("ALG_RSA_PKCS1_OAEP", "PAD_PKCS1_OAEP"),
    }
    for key, (alg, pad) in rsa_cipher_specs.items():
        if config[key]:
            cipher_provider.append(
                f"    if (algorithm == Cipher.{alg}) return new RSACipherProvider(Cipher.{alg}, Cipher.{pad});"
            )
    cipher_provider.extend([
        "    CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
        "    return null;",
        "  }",
        "  static Cipher getInstance(byte cipherAlgorithm, byte paddingAlgorithm,",
        "                            boolean externalAccess) throws CryptoException {",
        "    if (externalAccess) CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
    ])
    for key, (alg, cipher, pad, aes, cbc) in cipher_specs.items():
        getinstance = (
            config["cipher_getinstance_aes_cbc_nopad"] if aes
            else config["cipher_getinstance_des_cbc_nopad"]
        ) if key.endswith("_nopad") and cbc else config[key]
        if getinstance:
            cipher_provider.append(
                f"      if (cipherAlgorithm == Cipher.{cipher} && paddingAlgorithm == Cipher.{pad}) return new SymmetricCipherProvider(Cipher.{alg}, Cipher.{cipher}, Cipher.{pad}, {str(aes).lower()}, {str(cbc).lower()});"
            )
    for key, (alg, pad) in rsa_cipher_specs.items():
        if config[key]:
            cipher_provider.append(
                f"      if (cipherAlgorithm == Cipher.CIPHER_RSA && paddingAlgorithm == Cipher.{pad}) return new RSACipherProvider(Cipher.{alg}, Cipher.{pad});"
            )
    cipher_provider.extend([
        "    CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
        "    return null;",
        "  }",
        "}",
    ])
    (cipher_dir / "CipherProvider.java").write_text(
        "\n".join(cipher_provider) + "\n", encoding="utf-8"
    )
    cipher_enabled = any(config[key] for key in cipher_specs) or any(config[key] for key in (
        "cipher_getinstance_aes_cbc_nopad", "cipher_getinstance_des_cbc_nopad"))
    if cipher_enabled:
        (nativeimpl_dir / "CbcCipherNativeBridge.java").write_text(
            (Path(__file__).resolve().parent / "java-support/CbcCipherNativeBridge.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        (nativeimpl_dir / "CbcCipherNativeBridge.java").unlink(missing_ok=True)
    if cipher_enabled:
        (cipher_dir / "SymmetricCipherProvider.java").write_text(
            (Path(__file__).resolve().parent / "java-support/SymmetricCipherProvider.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        (cipher_dir / "SymmetricCipherProvider.java").unlink(missing_ok=True)
    for stale in ("AESCipherProvider.java", "DESCipherProvider.java", "CbcCipherProvider.java"):
        (cipher_dir / stale).unlink(missing_ok=True)
    rsa_cipher_enabled = any(config[key] for key in rsa_cipher_specs)
    rsa_cipher_provider = cipher_dir / "RSACipherProvider.java"
    rsa_cipher_bridge = nativeimpl_dir / "RsaCipherNativeBridge.java"
    if rsa_cipher_enabled:
        rsa_cipher_provider.write_text(
            (Path(__file__).resolve().parent / "java-support/RSACipherProvider.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        rsa_cipher_bridge.write_text(
            (Path(__file__).resolve().parent / "java-support/RsaCipherNativeBridge.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        rsa_cipher_provider.unlink(missing_ok=True)
        rsa_cipher_bridge.unlink(missing_ok=True)
    signature_specs = {
        "signature_des_mac8_nopad": ("ALG_DES_MAC8_NOPAD", "SIG_CIPHER_DES_MAC8", "PAD_NOPAD", 8, False),
        "signature_des_mac4_iso9797_m1": ("ALG_DES_MAC4_ISO9797_M1", "SIG_CIPHER_DES_MAC4", "PAD_ISO9797_M1", 4, False),
        "signature_des_mac8_iso9797_m1": ("ALG_DES_MAC8_ISO9797_M1", "SIG_CIPHER_DES_MAC8", "PAD_ISO9797_M1", 8, False),
        "signature_des_mac4_iso9797_m2": ("ALG_DES_MAC4_ISO9797_M2", "SIG_CIPHER_DES_MAC4", "PAD_ISO9797_M2", 4, False),
        "signature_des_mac8_iso9797_m2": ("ALG_DES_MAC8_ISO9797_M2", "SIG_CIPHER_DES_MAC8", "PAD_ISO9797_M2", 8, False),
        "signature_des_mac4_pkcs5": ("ALG_DES_MAC4_PKCS5", "SIG_CIPHER_DES_MAC4", "PAD_PKCS5", 4, False),
        "signature_des_mac8_pkcs5": ("ALG_DES_MAC8_PKCS5", "SIG_CIPHER_DES_MAC8", "PAD_PKCS5", 8, False),
        "signature_aes_mac_128_nopad": ("ALG_AES_MAC_128_NOPAD", "SIG_CIPHER_AES_MAC128", "PAD_NOPAD", 16, True),
        "signature_des_mac4_iso9797_1_m2_alg3": ("ALG_DES_MAC4_ISO9797_1_M2_ALG3", "SIG_CIPHER_DES_MAC4", "PAD_ISO9797_1_M2_ALG3", 4, False),
        "signature_des_mac8_iso9797_1_m2_alg3": ("ALG_DES_MAC8_ISO9797_1_M2_ALG3", "SIG_CIPHER_DES_MAC8", "PAD_ISO9797_1_M2_ALG3", 8, False),
        "signature_des_mac4_iso9797_1_m1_alg3": ("ALG_DES_MAC4_ISO9797_1_M1_ALG3", "SIG_CIPHER_DES_MAC4", "PAD_ISO9797_1_M1_ALG3", 4, False),
        "signature_des_mac8_iso9797_1_m1_alg3": ("ALG_DES_MAC8_ISO9797_1_M1_ALG3", "SIG_CIPHER_DES_MAC8", "PAD_ISO9797_1_M1_ALG3", 8, False),
        "signature_aes_cmac_128": ("ALG_AES_CMAC_128", "SIG_CIPHER_AES_CMAC128", "PAD_ISO9797_M2", 16, True),
    }
    signature_dir = out_dir / "java/javacard/security"
    signature_provider = [
        "package javacard.security;", "", "final class SignatureProvider {",
        "  private SignatureProvider() {}",
        "  static Signature getInstance(byte algorithm, boolean externalAccess) throws CryptoException {",
        "    if (externalAccess) CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
    ]
    for key, (alg, cipher, pad, length, aes) in signature_specs.items():
        if config[key]:
            signature_provider.append(
                f"    if (algorithm == Signature.{alg}) return new SymmetricMacProvider(Signature.{alg}, Signature.{cipher}, javacardx.crypto.Cipher.{pad}, (short){length}, {str(aes).lower()});"
            )
    signature_provider.extend([
        "    CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);", "    return null;", "  }",
        "  static Signature getInstance(byte digest, byte cipher, byte padding, boolean externalAccess) throws CryptoException {",
        "    if (externalAccess || digest != MessageDigest.ALG_NULL) CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
    ])
    for key, (alg, cipher, pad, length, aes) in signature_specs.items():
        if config[key]:
            signature_provider.append(
                f"    if (cipher == Signature.{cipher} && padding == javacardx.crypto.Cipher.{pad}) return new SymmetricMacProvider(Signature.{alg}, Signature.{cipher}, javacardx.crypto.Cipher.{pad}, (short){length}, {str(aes).lower()});"
            )
    signature_provider.extend([
        "    CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);", "    return null;", "  }", "}",
    ])
    (signature_dir / "SignatureProvider.java").write_text("\n".join(signature_provider) + "\n", encoding="utf-8")
    for filename in ("SymmetricMacProvider.java",):
        path = signature_dir / filename
        if any(config[key] for key in signature_specs):
            path.write_text((Path(__file__).resolve().parent / "java-support" / filename).read_text(encoding="utf-8"), encoding="utf-8")
        else:
            path.unlink(missing_ok=True)
    mac_bridge = nativeimpl_dir / "MacNativeBridge.java"
    if any(config[key] for key in signature_specs):
        mac_bridge.write_text((Path(__file__).resolve().parent / "java-support/MacNativeBridge.java").read_text(encoding="utf-8"), encoding="utf-8")
    else:
        mac_bridge.unlink(missing_ok=True)
    digest_provider = [
        "package javacard.security;",
        "",
        "final class MessageDigestProvider {",
        "  private MessageDigestProvider() {}",
        "  static MessageDigest getInstance(byte algorithm, boolean externalAccess)",
        "      throws CryptoException {",
        "    if (externalAccess) CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
    ]
    if config["sha256"]:
        digest_provider.extend([
            "    if (algorithm == MessageDigest.ALG_SHA_256)",
            "      return new Sha256DigestProvider();",
        ])
    generic_digests = {
        "sha1": "ALG_SHA",
        "sha224": "ALG_SHA_224",
        "sha384": "ALG_SHA_384",
        "sha512": "ALG_SHA_512",
    }
    for key, algorithm in generic_digests.items():
        if config[key]:
            digest_provider.extend([
                f"    if (algorithm == MessageDigest.{algorithm})",
                f"      return new MbedTlsDigestProvider(MessageDigest.{algorithm});",
            ])
    digest_provider.extend([
        "    CryptoException.throwIt(CryptoException.NO_SUCH_ALGORITHM);",
        "    return null;",
        "  }",
        "}",
    ])
    (java_dir / "MessageDigestProvider.java").write_text(
        "\n".join(digest_provider) + "\n", encoding="utf-8"
    )
    digest_path = java_dir / "Sha256DigestProvider.java"
    if config["sha256"]:
        digest_path.write_text(
            (Path(__file__).resolve().parent / "java-support/Sha256DigestProvider.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        (nativeimpl_dir / "Sha256NativeBridge.java").write_text(
            (Path(__file__).resolve().parent / "java-support/Sha256NativeBridge.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        digest_path.unlink(missing_ok=True)
        (nativeimpl_dir / "Sha256NativeBridge.java").unlink(missing_ok=True)
    generic_digest_path = java_dir / "MbedTlsDigestProvider.java"
    if any(config[key] for key in generic_digests):
        generic_digest_path.write_text(
            (Path(__file__).resolve().parent / "java-support/MbedTlsDigestProvider.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        (nativeimpl_dir / "MbedTlsDigestNativeBridge.java").write_text(
            (Path(__file__).resolve().parent / "java-support/MbedTlsDigestNativeBridge.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        generic_digest_path.unlink(missing_ok=True)
        (nativeimpl_dir / "MbedTlsDigestNativeBridge.java").unlink(missing_ok=True)
    framework_dir = out_dir / "java/javacard/framework"
    framework_dir.mkdir(parents=True, exist_ok=True)
    pin_provider = [
        "package javacard.framework;",
        "",
        "final class OwnerPINBuilderProvider {",
        "  private OwnerPINBuilderProvider() {}",
        "  static PIN build(byte tryLimit, byte maxPINSize, byte type)",
        "      throws PINException {",
        "    switch (type) {",
    ]
    pin_cases = (
        ("owner_pin", "OwnerPINBuilder.OWNER_PIN", "new OwnerPIN(tryLimit, maxPINSize)"),
        ("owner_pin_x", "OwnerPINBuilder.OWNER_PIN_X", "new OwnerPINxImpl(tryLimit, maxPINSize)"),
        ("owner_pin_x_with_predecrement", "OwnerPINBuilder.OWNER_PIN_X_WITH_PREDECREMENT", "new OwnerPINxWithPredecrementImpl(tryLimit, maxPINSize)"),
    )
    for key, identifier, implementation in pin_cases:
        pin_provider.extend([f"    case {identifier}:"])
        if config[key]:
            pin_provider.append(f"      return {implementation};")
        else:
            pin_provider.extend([
                "      SystemException.throwIt(SystemException.ILLEGAL_USE);",
                "      return null;",
            ])
    pin_provider.extend([
        "    default:",
        "      PINException.throwIt(PINException.ILLEGAL_VALUE);",
        "      return null;",
        "    }",
        "  }",
        "}",
    ])
    (framework_dir / "OwnerPINBuilderProvider.java").write_text(
        "\n".join(pin_provider) + "\n", encoding="utf-8"
    )
    pin_templates = (
        ("owner_pin_x", "OwnerPINxImpl.java"),
        ("owner_pin_x_with_predecrement", "OwnerPINxWithPredecrementImpl.java"),
    )
    for key, filename in pin_templates:
        path = framework_dir / filename
        if config[key]:
            path.write_text(
                (Path(__file__).resolve().parent / "java-support" / filename).read_text(encoding="utf-8"),
                encoding="utf-8",
            )
        else:
            path.unlink(missing_ok=True)
    base_path = framework_dir / "OwnerPINxBase.java"
    if config["owner_pin_x"] or config["owner_pin_x_with_predecrement"]:
        base_path.write_text(
            (Path(__file__).resolve().parent / "java-support/OwnerPINxBase.java").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
    else:
        base_path.unlink(missing_ok=True)


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=root / "config/javacard-support.json")
    parser.add_argument("--out-dir", type=Path, default=root / "build/javacard-support")
    parser.add_argument("--profile", help="named profile from the support configuration")
    args = parser.parse_args()
    try:
        profile, config = load_config(args.config, args.profile)
        emit(config, args.out_dir, profile)
    except ValueError as error:
        parser.error(str(error))
    print(f"JAVACARD_SUPPORT:PASS profile={profile} capabilities={len(config)} enabled={sum(config.values())}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
