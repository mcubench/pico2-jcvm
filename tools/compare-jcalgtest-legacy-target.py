#!/usr/bin/env python3
"""Compare an older JCAlgTest support export with generated profile support."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TARGET = next(
    ROOT.glob("tests/fixtures/reference/cards/provided-20261107/NXP_JCOP4_J3R180_P71_ALGSUPPORT*.csv"), None
)
DEFAULT_EXPECTED = ROOT / "build/javacard-support/jcalgtest-supported.csv"
SUPPORT_SECTIONS = {
    "javacard.security.KeyBuilder",
    "javacardx.crypto.Cipher",
    "javacard.security.Signature",
    "javacard.security.KeyAgreement",
    "javacard.security.KeyPair",
}
CIPHER_FACTORY_SECTION = (
    "javacardx.crypto.Cipher.getInstance(byte cipherAlgorithm, "
    "byte paddingAlgorithm, boolean externalAccess)"
)


def canonical_section(section: str) -> str:
    if section == "javacard.crypto.Signature":
        return "javacard.security.Signature"
    return section


def target_support(path: Path) -> tuple[set[tuple[str, str]], set[str]]:
    support: set[tuple[str, str]] = set()
    ec_generation_sizes: set[str] = set()
    section = ""
    with path.open("r", newline="", encoding="utf-8-sig") as source:
        for row_number, row in enumerate(csv.reader(source, delimiter=";"), 1):
            cells = [cell.strip() for cell in row]
            if len(cells) == 1 and cells[0]:
                section = cells[0]
                continue
            if len(cells) < 2 or not cells[0]:
                continue
            status = cells[1].lower()
            if status not in ("yes", "no"):
                continue
            if status != "yes":
                continue

            if section.startswith("javacard.security.KeyPair ALG_"):
                if "ALG_EC_FP on-card generation" in section:
                    if not cells[0].startswith("ALG_EC_FP LENGTH_EC_FP_"):
                        raise ValueError(
                            f"{path}:{row_number}: unexpected EC KeyPair row {cells[0]!r}"
                        )
                    ec_generation_sizes.add(cells[0].removeprefix("ALG_EC_FP LENGTH_EC_FP_"))
                    support.add(("javacard.security.KeyPair", "ALG_EC_FP"))
                elif "ALG_RSA on-card generation" in section or \
                        "ALG_RSA_CRT on-card generation" in section:
                    support.add(("javacard.security.KeyPair", cells[0]))
                continue

            canonical = canonical_section(section)
            if canonical not in SUPPORT_SECTIONS:
                continue
            algorithm = cells[0]
            if canonical == "javacard.security.KeyAgreement" and "/" in algorithm:
                algorithm = algorithm.split("/", 1)[0]
            support.add((canonical, algorithm))
            if canonical == "javacardx.crypto.Cipher":
                aliases = {
                    "ALG_AES_BLOCK_128_CBC_NOPAD": "CIPHER_AES_CBC PAD_NOPAD",
                    "ALG_DES_CBC_NOPAD": "CIPHER_DES_CBC PAD_NOPAD",
                }
                if algorithm in aliases:
                    support.add((CIPHER_FACTORY_SECTION, aliases[algorithm]))
    if not support:
        raise ValueError(f"{path}: no positive support rows found")
    return support, ec_generation_sizes


def expected_support(path: Path) -> set[tuple[str, str]]:
    with path.open("r", newline="", encoding="utf-8-sig") as source:
        rows = csv.reader(source, delimiter=";")
        try:
            headers = [value.strip() for value in next(rows)]
        except StopIteration as error:
            raise ValueError(f"{path}: expected-support file is empty") from error
        try:
            section_index = headers.index("section")
            algorithm_index = headers.index("algorithm_name")
        except ValueError as error:
            raise ValueError(
                f"{path}: expected section and algorithm_name columns"
            ) from error
        result: set[tuple[str, str]] = set()
        for row_number, row in enumerate(rows, 2):
            cells = [value.strip() for value in row]
            if not cells or not any(cells):
                continue
            if max(section_index, algorithm_index) >= len(cells):
                raise ValueError(f"{path}:{row_number}: incomplete support row")
            identity = (cells[section_index], cells[algorithm_index])
            if not all(identity):
                raise ValueError(f"{path}:{row_number}: empty support identity")
            if identity in result:
                raise ValueError(f"{path}:{row_number}: duplicate identity {identity!r}")
            result.add(identity)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "target_csv", nargs="?", type=Path, default=DEFAULT_TARGET,
        help="legacy JCAlgTest export (default: supplied NXP JCOP4 file)",
    )
    parser.add_argument(
        "--expected-support", type=Path, default=DEFAULT_EXPECTED,
        help="generated profile support CSV (default: %(default)s)",
    )
    args = parser.parse_args()
    if args.target_csv is None:
        parser.error("legacy target CSV was not found; provide its path")
    try:
        actual, ec_sizes = target_support(args.target_csv)
        profile_expected = expected_support(args.expected_support)
    except (OSError, ValueError, csv.Error) as error:
        parser.error(str(error))

    comparable_sections = SUPPORT_SECTIONS | {CIPHER_FACTORY_SECTION}
    expected = {
        identity for identity in profile_expected
        if identity[0] in comparable_sections
    }

    missing = sorted(actual - expected)
    extra = sorted(expected - actual)
    print(f"TARGET:JCAlgTest_VERSION=1.7.9")
    print(f"TARGET:SUPPORTED_IDENTITIES={len(actual)}")
    print(f"TARGET:EC_KEYPAIR_GENERATION_BITS={','.join(sorted(ec_sizes, key=int))}")
    print(f"PROFILE:ALL_EXPECTED_IDENTITIES={len(profile_expected)}")
    print(f"PROFILE:COMPARABLE_EXPECTED_IDENTITIES={len(expected)}")
    if missing or extra:
        print(f"TARGET_COMPARE:FAIL missing={len(missing)} extra={len(extra)}")
        for section, algorithm in missing[:30]:
            print(f"TARGET:SUPPORTED_BUT_NOT_SELECTED {section};{algorithm}")
        for section, algorithm in extra[:30]:
            print(f"PROFILE:SELECTED_BUT_NOT_IN_TARGET {section};{algorithm}")
        return 1
    print("TARGET_COMPARE:PASS exact_positive_support_set")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
