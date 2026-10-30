#!/usr/bin/env python3
"""Validate Java Card firmware capabilities and emit build/support metadata."""

from __future__ import annotations

import argparse
import csv
import io
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
    "signature_rsa_sha_pkcs1": (
        "javacard.security.Signature", "ALG_RSA_SHA_PKCS1"
    ),
    "signature_rsa_sha_iso9796": (
        "javacard.security.Signature", "ALG_RSA_SHA_ISO9796"
    ),
    "signature_rsa_sha_iso9796_mr": (
        "javacard.security.Signature", "ALG_RSA_SHA_ISO9796_MR"
    ),
    "signature_rsa_sha_224_pkcs1": (
        "javacard.security.Signature", "ALG_RSA_SHA_224_PKCS1"
    ),
    "signature_rsa_sha_256_pkcs1": (
        "javacard.security.Signature", "ALG_RSA_SHA_256_PKCS1"
    ),
    "signature_rsa_sha_384_pkcs1": (
        "javacard.security.Signature", "ALG_RSA_SHA_384_PKCS1"
    ),
    "signature_rsa_sha_512_pkcs1": (
        "javacard.security.Signature", "ALG_RSA_SHA_512_PKCS1"
    ),
    "signature_rsa_sha_pkcs1_pss": (
        "javacard.security.Signature", "ALG_RSA_SHA_PKCS1_PSS"
    ),
    "signature_rsa_sha_224_pkcs1_pss": (
        "javacard.security.Signature", "ALG_RSA_SHA_224_PKCS1_PSS"
    ),
    "signature_rsa_sha_256_pkcs1_pss": (
        "javacard.security.Signature", "ALG_RSA_SHA_256_PKCS1_PSS"
    ),
    "signature_rsa_sha_384_pkcs1_pss": (
        "javacard.security.Signature", "ALG_RSA_SHA_384_PKCS1_PSS"
    ),
    "signature_rsa_sha_512_pkcs1_pss": (
        "javacard.security.Signature", "ALG_RSA_SHA_512_PKCS1_PSS"
    ),
    "signature_ecdsa_sha": ("javacard.security.Signature", "ALG_ECDSA_SHA"),
    "signature_ecdsa_sha_224": ("javacard.security.Signature", "ALG_ECDSA_SHA_224"),
    "signature_ecdsa_sha_256": ("javacard.security.Signature", "ALG_ECDSA_SHA_256"),
    "signature_ecdsa_sha_384": ("javacard.security.Signature", "ALG_ECDSA_SHA_384"),
    "signature_ecdsa_sha_512": ("javacard.security.Signature", "ALG_ECDSA_SHA_512"),
    "key_agreement_ec_svdp_dh": (
        "javacard.security.KeyAgreement", "ALG_EC_SVDP_DH"
    ),
    "key_agreement_ec_svdp_dhc": (
        "javacard.security.KeyAgreement", "ALG_EC_SVDP_DHC"
    ),
    "key_agreement_ec_svdp_dh_plain": (
        "javacard.security.KeyAgreement", "ALG_EC_SVDP_DH_PLAIN"
    ),
    "key_agreement_ec_svdp_dhc_plain": (
        "javacard.security.KeyAgreement", "ALG_EC_SVDP_DHC_PLAIN"
    ),
    "key_agreement_ec_pace_gm": (
        "javacard.security.KeyAgreement", "ALG_EC_PACE_GM"
    ),
    "key_agreement_ec_svdp_dh_plain_xy": (
        "javacard.security.KeyAgreement", "ALG_EC_SVDP_DH_PLAIN_XY"
    ),
    "keypair_ec_fp": ("javacard.security.KeyPair", "ALG_EC_FP"),
    "secure_random": ("javacard.security.RandomData", "ALG_SECURE_RANDOM"),
    "cipher_aes_cbc_nopad": (
        "javacardx.crypto.Cipher", "ALG_AES_BLOCK_128_CBC_NOPAD"
    ),
    "cipher_aes_ctr": ("javacardx.crypto.Cipher", "ALG_AES_CTR"),
    "cipher_getinstance_aes_ctr": (
        "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)",
        "CIPHER_AES_CTR PAD_NOPAD",
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


def write_if_changed(path: Path, content: str) -> None:
    """Write generated text only when its bytes differ from the current file."""
    encoded = content.encode("utf-8")
    if path.is_file() and path.read_bytes() == encoded:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)


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

_ec_fp_lengths = (112, 128, 160, 192, 224, 256, 320, 384, 512, 521)
for _bits in _ec_fp_lengths:
    CAPABILITIES[f"key_ec_fp_private_{_bits}"] = (
        "javacard.security.KeyBuilder",
        f"TYPE_EC_FP_PRIVATE LENGTH_EC_FP_{_bits}",
    )
for _lifetime in ("transient_reset", "transient_deselect"):
    CAPABILITIES[f"key_ec_fp_private_{_lifetime}"] = (
        "javacard.security.KeyBuilder",
        f"TYPE_EC_FP_PRIVATE_{_lifetime.upper()}",
    )

_rsa_keypair_lengths = (512, 736, 768, 896, 1024, 1280, 1536, 1984, 2048, 3072, 4096)
for _bits in _rsa_keypair_lengths:
    CAPABILITIES[f"keypair_rsa_{_bits}"] = (
        "javacard.security.KeyPair", f"ALG_RSA LENGTH_RSA_{_bits}"
    )
    CAPABILITIES[f"keypair_rsa_crt_{_bits}"] = (
        "javacard.security.KeyPair", f"ALG_RSA_CRT LENGTH_RSA_{_bits}"
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
    aes_cipher = any((key.startswith("cipher_aes_") or
                      key.startswith("cipher_getinstance_aes_")) and enabled
                     for key, enabled in value.items())
    des_cipher = any((key.startswith("cipher_des_") or
                      key.startswith("cipher_getinstance_des_")) and enabled
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
    rsa_signature = any(value[key] for key in (
        "signature_rsa_sha_iso9796", "signature_rsa_sha_iso9796_mr",
        "signature_rsa_sha_pkcs1", "signature_rsa_sha_224_pkcs1",
        "signature_rsa_sha_256_pkcs1", "signature_rsa_sha_384_pkcs1",
        "signature_rsa_sha_512_pkcs1",
        "signature_rsa_sha_pkcs1_pss", "signature_rsa_sha_224_pkcs1_pss",
        "signature_rsa_sha_256_pkcs1_pss", "signature_rsa_sha_384_pkcs1_pss",
        "signature_rsa_sha_512_pkcs1_pss",
    ))
    if rsa_signature and (not rsa_public or not rsa_private):
        raise ValueError(f"{path}: RSA signatures require enabled public and private RSA key capabilities")
    ecdsa_signature = any(value[key] for key in (
        "signature_ecdsa_sha", "signature_ecdsa_sha_224",
        "signature_ecdsa_sha_256", "signature_ecdsa_sha_384",
        "signature_ecdsa_sha_512",
    ))
    ec_key_agreement = any(value[key] for key in (
        "key_agreement_ec_svdp_dh", "key_agreement_ec_svdp_dhc",
        "key_agreement_ec_svdp_dh_plain", "key_agreement_ec_svdp_dhc_plain",
        "key_agreement_ec_pace_gm", "key_agreement_ec_svdp_dh_plain_xy",
    ))
    if ec_key_agreement and not any(
        value[f"key_ec_fp_private_{bits}"] for bits in _ec_fp_lengths
    ):
        raise ValueError(f"{path}: EC key agreement requires an enabled prime-field EC key length")
    if value["keypair_ec_fp"] and not any(
        value[f"key_ec_fp_private_{bits}"] for bits in _ec_fp_lengths
    ):
        raise ValueError(f"{path}: EC key-pair generation requires an enabled prime-field EC key length")
    for bits in _rsa_keypair_lengths:
        if value[f"keypair_rsa_{bits}"] and not (
            value[f"key_rsa_public_{bits}"] and value[f"key_rsa_private_{bits}"]
        ):
            raise ValueError(f"{path}: RSA key-pair generation at {bits} bits requires matching public and private RSA keys")
        if value[f"keypair_rsa_crt_{bits}"] and not (
            value[f"key_rsa_public_{bits}"] and value[f"key_rsa_crt_private_{bits}"]
        ):
            raise ValueError(f"{path}: RSA-CRT key-pair generation at {bits} bits requires matching public and CRT private RSA keys")
    if ((value["key_agreement_ec_svdp_dh"] or
         value["key_agreement_ec_svdp_dhc"]) and not value["sha1"]):
        raise ValueError(f"{path}: EC SVDP-DH and DHC require SHA-1 support")
    if ecdsa_signature and not any(
        value[f"key_ec_fp_private_{bits}"] for bits in _ec_fp_lengths
    ):
        raise ValueError(f"{path}: ECDSA signatures require an enabled prime-field EC key length")
    ec_fp_length_enabled = any(
        value[f"key_ec_fp_private_{bits}"] for bits in _ec_fp_lengths
    )
    if ((value["key_ec_fp_private_transient_reset"] or
         value["key_ec_fp_private_transient_deselect"]) and
            not ec_fp_length_enabled):
        raise ValueError(f"{path}: transient EC private keys require an enabled prime-field EC key length")
    if (value["cipher_rsa_oaep"] or value["signature_rsa_sha_iso9796"] or
            value["signature_rsa_sha_iso9796_mr"] or
            value["signature_rsa_sha_pkcs1"]) and not value["sha1"]:
        raise ValueError(f"{path}: RSA OAEP and SHA-1 signatures require SHA-1 support")
    for capability, digest in (
        ("signature_ecdsa_sha", "sha1"),
        ("signature_ecdsa_sha_224", "sha224"),
        ("signature_ecdsa_sha_256", "sha256"),
        ("signature_ecdsa_sha_384", "sha384"),
        ("signature_ecdsa_sha_512", "sha512"),
        ("signature_rsa_sha_iso9796_mr", "sha1"),
        ("signature_rsa_sha_224_pkcs1", "sha224"),
        ("signature_rsa_sha_256_pkcs1", "sha256"),
        ("signature_rsa_sha_384_pkcs1", "sha384"),
        ("signature_rsa_sha_512_pkcs1", "sha512"),
        ("signature_rsa_sha_pkcs1_pss", "sha1"),
        ("signature_rsa_sha_224_pkcs1_pss", "sha224"),
        ("signature_rsa_sha_256_pkcs1_pss", "sha256"),
        ("signature_rsa_sha_384_pkcs1_pss", "sha384"),
        ("signature_rsa_sha_512_pkcs1_pss", "sha512"),
    ):
        if value[capability] and not value[digest]:
            raise ValueError(f"{path}: {capability} requires {digest} support")
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
    # Generated Java support is an input tree to the API converter. Remove
    # legacy key provider files left by older profiles before emitting outputs.
    for stale in (
        out_dir / "java/javacard/security/KeyBuilderProvider.java",
        out_dir / "java/javacard/security/KeyProviderBase.java",
        out_dir / "java/javacard/security/AESKeyProvider.java",
        out_dir / "java/javacard/security/DESKeyProvider.java",
        out_dir / "java/fr/gouv/ssi/nativeimpl/RSAKeyProvider.java",
    ):
        stale.unlink(missing_ok=True)
    csv_output = io.StringIO(newline="")
    writer = csv.writer(csv_output, delimiter=";", lineterminator="\n")
    writer.writerow(("section", "algorithm_name"))
    for key, row in CAPABILITIES.items():
        if config[key]:
            writer.writerow(row)
    write_if_changed(out_dir / "jcalgtest-supported.csv", csv_output.getvalue())

    cmake = ["# Generated by tools/javacard-support.py; do not edit.",
             f'set(JC_SUPPORT_PROFILE "{profile}")']
    header = ["/* Generated by tools/javacard-support.py; do not edit. */", "#pragma once",
              f'#define JC_SUPPORT_PROFILE "{profile}"']
    for key, enabled in config.items():
        cmake.append(f"set(JC_SUPPORT_{key.upper()} {'ON' if enabled else 'OFF'})")
        header.append(f"#define JC_SUPPORT_{key.upper()} {1 if enabled else 0}")
    rsa_signature_enabled = any(config[key] for key in (
        "signature_rsa_sha_iso9796", "signature_rsa_sha_iso9796_mr",
        "signature_rsa_sha_pkcs1", "signature_rsa_sha_224_pkcs1",
        "signature_rsa_sha_256_pkcs1", "signature_rsa_sha_384_pkcs1",
        "signature_rsa_sha_512_pkcs1",
        "signature_rsa_sha_pkcs1_pss", "signature_rsa_sha_224_pkcs1_pss",
        "signature_rsa_sha_256_pkcs1_pss", "signature_rsa_sha_384_pkcs1_pss",
        "signature_rsa_sha_512_pkcs1_pss",
    ))
    cmake.append(
        f"set(JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY {'ON' if rsa_signature_enabled else 'OFF'})"
    )
    header.append(
        f"#define JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY {1 if rsa_signature_enabled else 0}"
    )
    rsa_keypair_enabled = any(
        config[f"keypair_rsa_{bits}"] or config[f"keypair_rsa_crt_{bits}"]
        for bits in _rsa_keypair_lengths
    )
    cmake.append(
        f"set(JC_SUPPORT_KEYPAIR_RSA_ANY {'ON' if rsa_keypair_enabled else 'OFF'})"
    )
    header.append(
        f"#define JC_SUPPORT_KEYPAIR_RSA_ANY {1 if rsa_keypair_enabled else 0}"
    )
    write_if_changed(out_dir / "javacard-support.cmake", "\n".join(cmake) + "\n")
    write_if_changed(out_dir / "javacard-support.h", "\n".join(header) + "\n")
    write_if_changed(out_dir / "javacard-support.json",
                     json.dumps(config, indent=2) + "\n")
    write_if_changed(
        out_dir / "javacard-native-methods.txt",
        "\n".join((
            "javacard/security|MessageDigest|MessageDigest/getInstance(BZ)Ljavacard/security/MessageDigest;|static",
            "javacard/security|MessageDigest|MessageDigest/getAlgorithm()B|instance",
            "javacard/security|MessageDigest|MessageDigest/getLength()B|instance",
            "javacard/security|MessageDigest|MessageDigest/doFinal([BSS[BS)S|instance",
            "javacard/security|MessageDigest|MessageDigest/update([BSS)V|instance",
            "javacard/security|MessageDigest|MessageDigest/reset()V|instance",
            "javacard/security|Signature|Signature/getInstance(BZ)Ljavacard/security/Signature;|static",
            "javacard/security|Signature|Signature/getInstance(BBBZ)Ljavacard/security/Signature;|static",
            "javacard/security|Signature|Signature/init(Ljavacard/security/Key;B)V|instance",
            "javacard/security|Signature|Signature/init(Ljavacard/security/Key;B[BSS)V|instance",
            "javacard/security|Signature|Signature/setInitialDigest([BSS[BSS)V|instance",
            "javacard/security|Signature|Signature/getAlgorithm()B|instance",
            "javacard/security|Signature|Signature/getMessageDigestAlgorithm()B|instance",
            "javacard/security|Signature|Signature/getCipherAlgorithm()B|instance",
            "javacard/security|Signature|Signature/getPaddingAlgorithm()B|instance",
            "javacard/security|Signature|Signature/getLength()S|instance",
            "javacard/security|Signature|Signature/update([BSS)V|instance",
            "javacard/security|Signature|Signature/sign([BSS[BS)S|instance",
            "javacard/security|Signature|Signature/sign([BSS[BS[SS)S|instance",
            "javacard/security|Signature|Signature/beginVerify([BSS)S|instance",
            "javacard/security|Signature|Signature/signPreComputedHash([BSS[BS)S|instance",
            "javacard/security|Signature|Signature/verify([BSS[BSS)Z|instance",
            "javacard/security|Signature|Signature/verify([BSS)Z|instance",
            "javacard/security|Signature|Signature/verifyPreComputedHash([BSS[BSS)Z|instance",
            "javacard/security|KeyBuilder|KeyBuilder/buildKey(BSZ)Ljavacard/security/Key;|static",
            "javacard/security|KeyBuilder|KeyBuilder/buildKey(BBSZ)Ljavacard/security/Key;|static",
            "javacard/security|NativeKey|NativeKey/isInitialized()Z|instance",
            "javacard/security|NativeKey|NativeKey/clearKey()V|instance",
            "javacard/security|NativeKey|NativeKey/getType()B|instance",
            "javacard/security|NativeKey|NativeKey/getSize()S|instance",
            "javacard/security|NativeKey|NativeKey/setKey([BS)V|instance",
            "javacard/security|NativeKey|NativeKey/getKey([BS)B|instance",
            "javacard/security|NativeKey|NativeKey/setModulus([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getModulus([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setExponent([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getExponent([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setP([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getP([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setQ([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getQ([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setDP1([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getDP1([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setDQ1([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getDQ1([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setPQ([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getPQ([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setFieldFP([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/setFieldF2M(S)V|instance",
            "javacard/security|NativeKey|NativeKey/setFieldF2M(SSS)V|instance",
            "javacard/security|NativeKey|NativeKey/setA([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/setB([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/setG([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/setR([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/setK(S)V|instance",
            "javacard/security|NativeKey|NativeKey/getField([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/getA([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/getB([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/getG([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/getR([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/getK()S|instance",
            "javacard/security|NativeKey|NativeKey/copyDomainParametersFrom(Ljavacard/security/ECKey;)V|instance",
            "javacard/security|NativeKey|NativeKey/setW([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getW([BS)S|instance",
            "javacard/security|NativeKey|NativeKey/setS([BSS)V|instance",
            "javacard/security|NativeKey|NativeKey/getS([BS)S|instance",
            "javacard/security|KeyAgreement|KeyAgreement/getInstance(BZ)Ljavacard/security/KeyAgreement;|static",
            "javacard/security|KeyAgreement|KeyAgreement/init(Ljavacard/security/PrivateKey;)V|instance",
            "javacard/security|KeyAgreement|KeyAgreement/getAlgorithm()B|instance",
            "javacard/security|KeyAgreement|KeyAgreement/generateSecret([BSS[BS)S|instance",
            "javacard/security|KeyPair|KeyPair/<init>(BS)V|instance",
            "javacard/security|KeyPair|KeyPair/genKeyPair()V|instance",
            "javacard/security|KeyPair|KeyPair/getPublic()Ljavacard/security/PublicKey;|instance",
            "javacard/security|KeyPair|KeyPair/getPrivate()Ljavacard/security/PrivateKey;|instance",
            "javacardx/crypto|Cipher|Cipher/getInstance(BZ)Ljavacardx/crypto/Cipher;|static",
            "javacardx/crypto|Cipher|Cipher/getInstance(BBZ)Ljavacardx/crypto/Cipher;|static",
            "javacardx/crypto|Cipher|Cipher/init(Ljavacard/security/Key;B)V|instance",
            "javacardx/crypto|Cipher|Cipher/init(Ljavacard/security/Key;B[BSS)V|instance",
            "javacardx/crypto|Cipher|Cipher/getAlgorithm()B|instance",
            "javacardx/crypto|Cipher|Cipher/getCipherAlgorithm()B|instance",
            "javacardx/crypto|Cipher|Cipher/getPaddingAlgorithm()B|instance",
            "javacardx/crypto|Cipher|Cipher/update([BSS[BS)S|instance",
            "javacardx/crypto|Cipher|Cipher/doFinal([BSS[BS)S|instance",
        )) + "\n"
    )
    write_if_changed(out_dir / "profile.txt", profile + "\n")
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
    write_if_changed(java_dir / "RandomDataProvider.java",
                     "\n".join(random_provider) + "\n")
    if config["secure_random"]:
        template = Path(__file__).resolve().parent / "java-support/SecureRandomData.java"
        provider_path = java_dir / "SecureRandomData.java"
        write_if_changed(provider_path, template.read_text(encoding="utf-8"))
        write_if_changed(
            out_dir / "java/fr/gouv/ssi/nativeimpl/RandomDataNativeBridge.java",
            (Path(__file__).resolve().parent /
             "java-support/RandomDataNativeBridge.java").read_text(encoding="utf-8"),
        )
    else:
        (java_dir / "SecureRandomData.java").unlink(missing_ok=True)
        (out_dir / "java/fr/gouv/ssi/nativeimpl/RandomDataNativeBridge.java").unlink(missing_ok=True)
    cipher_dir = out_dir / "java/javacardx/crypto"
    nativeimpl_dir = out_dir / "java/fr/gouv/ssi/nativeimpl"
    for filename in ("CipherProvider.java", "SymmetricCipherProvider.java",
                     "RSACipherProvider.java"):
        (cipher_dir / filename).unlink(missing_ok=True)
    for filename in ("CbcCipherNativeBridge.java", "RsaCipherNativeBridge.java",
                     "RSACipherProvider.java"):
        (nativeimpl_dir / filename).unlink(missing_ok=True)
    signature_dir = out_dir / "java/javacard/security"
    for filename in ("SignatureProvider.java", "SymmetricMacProvider.java"):
        (signature_dir / filename).unlink(missing_ok=True)
    (nativeimpl_dir / "MacNativeBridge.java").unlink(missing_ok=True)
    rsa_signature_provider = nativeimpl_dir / "RSASignatureProvider.java"
    rsa_signature_provider.unlink(missing_ok=True)
    for generated_digest_class in (
        java_dir / "MessageDigestProvider.java",
        java_dir / "Sha256DigestProvider.java",
        java_dir / "MbedTlsDigestProvider.java",
        nativeimpl_dir / "Sha256NativeBridge.java",
        nativeimpl_dir / "MbedTlsDigestNativeBridge.java",
    ):
        generated_digest_class.unlink(missing_ok=True)
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
    write_if_changed(framework_dir / "OwnerPINBuilderProvider.java",
                     "\n".join(pin_provider) + "\n")
    pin_templates = (
        ("owner_pin_x", "OwnerPINxImpl.java"),
        ("owner_pin_x_with_predecrement", "OwnerPINxWithPredecrementImpl.java"),
    )
    for key, filename in pin_templates:
        path = framework_dir / filename
        if config[key]:
            write_if_changed(
                path,
                (Path(__file__).resolve().parent /
                 "java-support" / filename).read_text(encoding="utf-8"),
            )
        else:
            path.unlink(missing_ok=True)
    base_path = framework_dir / "OwnerPINxBase.java"
    if config["owner_pin_x"] or config["owner_pin_x_with_predecrement"]:
        write_if_changed(
            base_path,
            (Path(__file__).resolve().parent /
             "java-support/OwnerPINxBase.java").read_text(encoding="utf-8"),
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
