#!/usr/bin/env python3
"""Check the ROM export tokens of the Java Card 3.2 preview PQC packages.

Applets built with the Oracle SDK 26.1 link to javacardx.crypto.kem and
javacardx.security.pqc by token. These packages get no token remapping, so the
ROM JCA must assign the official tokens. The expected values below come from
the SDK 26.1 export files (api_export_files_preview in lib/tools.jar, read
with exp2text). Interface methods inherited from javacard.security.Key are
remapped at run time (jc_cp.cpp) and are not compared here.
"""
import re
import sys
from pathlib import Path

EXPECTED = {
    "javacardx/crypto/kem/javacard/kem.jca": {
        "KEM$Decapsulator": (0, {
            "decapsulate(Ljavacard/security/PrivateKey;[BS[BS)S": 0,
            "decapsulate(Ljavacard/security/PrivateKey;[BSLjavacard/security/SecretKey;SS)S": 1,
            "sharedSecretSize()S": 2, "cipherTextSize()S": 3}),
        "KEM$Encapsulator": (1, {
            "encapsulate([BSS[BS[BS)S": 0,
            "encapsulate([BSS[BSLjavacard/security/SecretKey;SS)S": 1,
            "encapsulate(Ljavacard/security/PublicKey;[BS[BS)S": 2,
            "encapsulate(Ljavacard/security/PublicKey;[BSLjavacard/security/SecretKey;SS)S": 3,
            "sharedSecretSize()S": 4, "cipherTextSize()S": 5}),
        "KEM": (2, {
            "getInstance(SLjavacard/security/AlgorithmParameterSpec;S)Ljavacardx/crypto/kem/KEM;": 0}),
    },
    "javacardx/security/pqc/javacard/pqc.jca": {
        "EncodableKey": (0, {
            "getParams()Ljavacard/security/NamedParameterSpec;": 4,
            "getEncodingLength()S": 5, "getEncoded([BS)S": 6,
            "setEncoded([BSS)V": 7}),
        "MLDSAPrivateKey": (1, {}),
        "MLDSAPublicKey": (2, {}),
        "MLKEMPrivateKey": (3, {"getPublicKey([BS)S": 8}),
        "MLKEMPublicKey": (4, {}),
        "PQCKeyFactory": (5, {
            "buildKey(SS)Ljavacard/security/Key;": 0,
            "generateKeyPair(Ljavacard/security/PublicKey;Ljavacard/security/PrivateKey;)V": 1,
            "generateKeyPair(Ljavacard/security/PrivateKey;Ljavacard/security/PublicKey;Ljavacard/security/PrivateKey;)V": 2}),
    },
}

TYPE_RE = re.compile(r"^\t\.(?:class|interface) (?:(?:public|abstract|final) )*(\S+) (\d+)")
METHOD_RE = re.compile(r"^\t\t\.method (?:(?:public|abstract|final|static) )*(\S+) (\d+) \{")


def main() -> int:
    root = Path(sys.argv[1])
    errors = []
    for relative, classes in EXPECTED.items():
        current, found = None, {}
        for line in (root / relative).read_text().splitlines():
            if m := TYPE_RE.match(line):
                current = m.group(1)
                found[current] = (int(m.group(2)), {})
            elif current and (m := METHOD_RE.match(line)):
                found[current][1][m.group(1)] = int(m.group(2))
        for name, (token, methods) in classes.items():
            if name not in found:
                errors.append(f"{relative}: missing {name}")
                continue
            if found[name][0] != token:
                errors.append(f"{relative}: {name} token {found[name][0]} != {token}")
            for method, method_token in methods.items():
                actual = found[name][1].get(method)
                if actual != method_token:
                    errors.append(f"{relative}: {name}.{method} token {actual} != {method_token}")
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    if errors:
        return 2
    print("PQC_API_TOKENS:PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
