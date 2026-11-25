#!/usr/bin/env python3
"""Check the PQC rows of a JCAlgTest 1.8.6/jc32 support CSV.

Every row of the three PQC sections must report "yes", and each section must
have its full row count (6 parameter sets x expanded/seed keys, 3 ML-KEM KEM
instances, 4 ML-DSA Signature cipher variants).
Usage: tools/check-jcalgtest-pqc-support.py RESULT.csv [...]
"""
import sys

SECTIONS = {
    "javacardx.security.pqc.PQCKeyFactory": 12,
    "javacardx.crypto.kem.KEM ALG_ML_KEM": 3,
    "javacard.security.Signature ML-DSA": 4,
}


def main(paths: list[str]) -> int:
    if not paths:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    rows: dict[str, list[tuple[str, str]]] = {}
    for path in paths:
        section = None
        with open(path, errors="replace") as handle:
            for line in handle:
                line = line.strip()
                if ";" not in line:
                    section = line or None
                elif section in SECTIONS:
                    name, status = line.split(";")[:2]
                    rows.setdefault(section, []).append((name, status))
    counts = {section: len(rows.get(section, [])) for section in SECTIONS}
    bad = [(section, name, status) for section, items in rows.items()
           for name, status in items if status != "yes"]
    ok = not bad and counts == SECTIONS
    print(f"PQC_SUPPORT:{'PASS' if ok else 'FAIL'} rows={sum(counts.values())} "
          f"counts={counts} not_yes={bad}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
