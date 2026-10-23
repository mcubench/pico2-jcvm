#!/usr/bin/env python3
"""Compare JCAlgTest v1.8.2 supported-algorithm CSV exports."""

from __future__ import annotations

import argparse
import csv
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Dataset:
    path: Path
    applet_version: str
    statuses: dict[tuple[str, str], str]


def load_dataset(path: Path) -> Dataset:
    with path.open("r", newline="", encoding="utf-8-sig") as source:
        rows = list(csv.reader(source, delimiter=";"))

    applet_version = ""
    header_index = -1
    for index, row in enumerate(rows):
        if len(row) >= 2 and row[0].strip() == "AlgTest applet version":
            applet_version = row[1].strip()
        if row and row[0].strip() == "algorithm_name":
            header_index = index
            break
    if header_index < 0:
        raise ValueError(f"{path}: algorithm_name header is missing")
    if not applet_version:
        raise ValueError(f"{path}: AlgTest applet version is missing")

    headers = [value.strip() for value in rows[header_index]]
    try:
        algorithm_index = headers.index("algorithm_name")
        status_index = headers.index("is_supported")
    except ValueError as error:
        raise ValueError(f"{path}: expected algorithm_name and is_supported columns") from error

    section = ""
    statuses: dict[tuple[str, str], str] = {}
    for row_number, row in enumerate(rows[header_index + 1 :], header_index + 2):
        cells = [value.strip() for value in row]
        if not any(cells):
            continue
        if len(cells) == 1:
            section = cells[0]
            continue
        if algorithm_index >= len(cells) or not cells[algorithm_index]:
            continue
        if status_index >= len(cells) or not cells[status_index]:
            raise ValueError(f"{path}:{row_number}: result status is missing")

        key = (section, cells[algorithm_index])
        if key in statuses:
            raise ValueError(f"{path}:{row_number}: duplicate result {key!r}")
        statuses[key] = cells[status_index].lower()

    if not statuses:
        raise ValueError(f"{path}: no algorithm result rows found")
    return Dataset(path, applet_version, statuses)


def load_expected_support(path: Path) -> set[tuple[str, str]]:
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

        expected: set[tuple[str, str]] = set()
        for row_number, row in enumerate(rows, start=2):
            cells = [value.strip() for value in row]
            if not cells or not any(cells):
                continue
            if max(section_index, algorithm_index) >= len(cells):
                raise ValueError(f"{path}:{row_number}: incomplete support row")
            key = (cells[section_index], cells[algorithm_index])
            if not all(key):
                raise ValueError(f"{path}:{row_number}: empty support identity")
            if key in expected:
                raise ValueError(f"{path}:{row_number}: duplicate support {key!r}")
            expected.add(key)

    return expected


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("choupi_csv", type=Path)
    parser.add_argument("reference_csv", type=Path)
    parser.add_argument(
        "--expected-support",
        type=Path,
        default=Path(__file__).resolve().parent.parent
        / "build/javacard-support/jcalgtest-supported.csv",
        help="generated CHOUPI support identities (default: %(default)s)",
    )
    args = parser.parse_args()

    try:
        choupi = load_dataset(args.choupi_csv)
        reference = load_dataset(args.reference_csv)
        expected_support = load_expected_support(args.expected_support)
    except (OSError, ValueError, csv.Error) as error:
        parser.error(str(error))

    print(f"JCAlgTest applet: CHOUPI={choupi.applet_version} reference={reference.applet_version}")
    if choupi.applet_version != reference.applet_version:
        print("COMPARE:FAIL applet_version_mismatch")
        return 1

    choupi_keys = set(choupi.statuses)
    reference_keys = set(reference.statuses)
    missing_reference = sorted(choupi_keys - reference_keys)
    missing_choupi = sorted(reference_keys - choupi_keys)
    if missing_reference or missing_choupi:
        print(
            "COMPARE:FAIL row_identity_mismatch "
            f"choupi_rows={len(choupi_keys)} reference_rows={len(reference_keys)} "
            f"missing_reference={len(missing_reference)} "
            f"missing_choupi={len(missing_choupi)}"
        )
        for key in missing_reference[:10]:
            print(f"ROW:ONLY_CHOUPI section={key[0]} name={key[1]}")
        for key in missing_choupi[:10]:
            print(f"ROW:ONLY_REFERENCE section={key[0]} name={key[1]}")
        return 1

    choupi_counts = Counter(choupi.statuses.values())
    reference_counts = Counter(reference.statuses.values())
    statuses = choupi.statuses
    ref_statuses = reference.statuses
    unsupported_deltas = sum(
        ref_statuses[key] == "yes" and statuses[key] == "no"
        for key in choupi_keys
    )
    supported_only_choupi = sum(
        statuses[key] == "yes" and ref_statuses[key] != "yes"
        for key in choupi_keys
    )
    choupi_errors = sorted(
        (key, value)
        for key, value in statuses.items()
        if value not in ("yes", "no")
    )
    actual_support = {
        key for key, status in choupi.statuses.items() if status == "yes"
    }
    unexpected_support = sorted(actual_support - expected_support)
    missing_support = sorted(expected_support - actual_support)

    print(f"COMPARE:ROWS matched={len(choupi_keys)}")
    print(
        "CHOUPI:STATUS "
        f"yes={choupi_counts['yes']} no={choupi_counts['no']} "
        f"other={len(choupi_errors)}"
    )
    print(
        "REFERENCE:STATUS "
        f"yes={reference_counts['yes']} no={reference_counts['no']} "
        f"other={sum(v for k, v in reference_counts.items() if k not in ('yes', 'no'))}"
    )
    print(
        "COMPARE:CAPABILITY_DELTA "
        f"reference_yes_choupi_no={unsupported_deltas} "
        f"choupi_yes_reference_not_yes={supported_only_choupi}"
    )
    print(
        "CHOUPI:DECLARED_SUPPORT "
        f"expected={len(expected_support)} actual={len(actual_support)} "
        f"unexpected={len(unexpected_support)} missing={len(missing_support)}"
    )
    for key in unexpected_support[:20]:
        print(f"CHOUPI:UNEXPECTED_SUPPORT section={key[0]} name={key[1]}")
    for key in missing_support[:20]:
        print(f"CHOUPI:MISSING_SUPPORT section={key[0]} name={key[1]}")
    for key, status in choupi_errors[:20]:
        print(f"CHOUPI:ROW_ERROR section={key[0]} name={key[1]} status={status}")
    if len(choupi_errors) > 20:
        print(f"CHOUPI:ROW_ERROR additional={len(choupi_errors) - 20}")

    if choupi_errors or unexpected_support or missing_support:
        print("COMPARE:FAIL choupi_statuses_do_not_match_declared_support")
        return 1
    print("COMPARE:PASS rows_statuses_and_declared_support")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
