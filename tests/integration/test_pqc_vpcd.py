#!/usr/bin/env python3
"""Run the PQC probe vectors (tests/integration/pqc_vectors.py) on the PC
emulator through VPCD. Expects build/choupi-pc from a profile with ML-KEM and
ML-DSA enabled and build/pqc-probe from tools/build-pqc-probe."""
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/integration"))
import pqc_vectors  # noqa: E402

APPLET_AID = "A00000015100030101"


def main() -> int:
    cases = pqc_vectors.vectors()
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    log = ROOT / f"artifacts/logs/pqc-vpcd-{stamp}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(ROOT / "tools/env"), "python3",
        str(ROOT / "tests/integration/pc_vpcd_applet_test.py"),
        "--emulator", str(ROOT / "build/choupi-pc/choupi"),
        "--cap-code", str(ROOT / "build/pqc-probe/load-code.bin"),
        "--rom-mask", str(ROOT / "build/choupi-pc/flash"),
        "--applet-aid", APPLET_AID, "--package-id", "56", "--passes", "1",
        "--flash-copy", str(ROOT / "build/pqc-probe/flash-vpcd.bin"),
        "--log", str(log), "--connect-timeout", "300", "--apdu-timeout", "600",
    ]
    for apdu, expected, _label in cases:
        command.extend(("--apdu", apdu.hex(), "--expect-apdu-response", expected.hex()))
    completed = subprocess.run(command, cwd=ROOT, check=False,
                               stdout=subprocess.PIPE, text=True)
    failed = None
    for line in completed.stdout.splitlines():
        if line.startswith("TEST:FAIL custom APDU "):
            index = int(line.split()[3]) - 1
            failed = index
            print(f"{line}\n  label: {cases[index][2]}")
        elif line.startswith(("TEST:FAIL", "TEST:SUMMARY")):
            print(line)
    print(f"TEST:LOG {log}")
    if completed.returncode or failed is not None:
        return 1
    print(f"PQC_VPCD:PASS cases={len(cases)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
