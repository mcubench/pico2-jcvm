#!/usr/bin/env python3
"""Check a returned Windows campaign (run-all.ps1) or single-mode result.

Usage: tools/check-windows-results.py RESULT.zip|DIR [...]
Prints one CHECK:<PASS|FAIL> line per criterion and exits 1 on any failure.
Criteria are the closure conditions in .agent/EXT_REQUESTS.md.
"""
import pathlib
import re
import subprocess
import sys
import tempfile
import zipfile

HERE = pathlib.Path(__file__).resolve().parent
JCMATHLIB_OK = "684886E0A3763013DC98C3837E8A6B6C30CCD899257A5CD89DA6BE8E04F44EED 9000"
failures = 0


def check(ok: bool, name: str, detail: str = "") -> None:
    global failures
    failures += not ok
    print(f"CHECK:{'PASS' if ok else 'FAIL'} {name}{' ' + detail if detail else ''}")


def kv(path: pathlib.Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    return dict(line.split("=", 1) for line in path.read_text(errors="replace").splitlines()
                if "=" in line and not line.startswith("#"))


def unpack(path: pathlib.Path, into: pathlib.Path) -> None:
    with zipfile.ZipFile(path) as archive:
        archive.extractall(into)
    for inner in list(into.rglob("*.zip")):  # campaign ZIP holds per-mode ZIPs
        target = inner.with_suffix("")
        target.mkdir(exist_ok=True)
        unpack(inner, target)


def check_run(run: pathlib.Path) -> None:
    ident = kv(run / "run-identity.txt")
    mode = ident.get("mode", "?")
    tag = f"{mode}[{run.name}]"
    check(ident.get("client_exit_code") == "0", f"{tag}.client_exit", ident.get("client_exit_code", "missing"))
    for name in ("health-before", "health-after", "health-after-uninstall"):
        health = kv(run / f"{name}.txt")
        if not health and name == "health-after-uninstall":
            continue
        check(health.get("health_boot_count") == "1", f"{tag}.{name}.no_reset",
              f"boot_count={health.get('health_boot_count', 'missing')}")
        check(health.get("health_sw_error") == "0", f"{tag}.{name}.no_internal_error_sw",
              f"count={health.get('health_sw_error', 'missing')} last_apdu={health.get('health_last_error_apdu', '-')}"
              f" cla_ins={health.get('health_last_error_cla_ins', '-')} sw={health.get('health_last_error_sw', '-')}")
    serial = run / "serial.log"
    if serial.is_file():
        faults = [l for l in serial.read_text(errors="replace").splitlines() if "FAULT" in l]
        check(not faults, f"{tag}.serial_no_fault", faults[0][:120] if faults else "")
    else:
        print(f"CHECK:INFO {tag}.serial_missing (health record is the reset evidence)")
    if mode == "pqc":
        csvs = [str(p) for p in run.glob("*ALGSUPPORT*.csv")]
        rc = subprocess.run([sys.executable, str(HERE / "check-jcalgtest-pqc-support.py"), *csvs]).returncode if csvs else 1
        check(rc == 0, f"{tag}.pqc_rows_yes")
    elif mode == "pqcperf":
        lists = list(run.glob("*PQCONLY*already_measured.list"))
        count = len(lists[0].read_text().split("\n")) - 1 if lists else 0
        check(count == 33, f"{tag}.pqcperf_measurements", f"count={count}")
    elif mode == "jcmathlib":
        out = run / "jcmathlib-apdu.txt"
        check(out.is_file() and JCMATHLIB_OK in out.read_text(errors="replace"), f"{tag}.jcmathlib_shift")
    elif mode in ("all", "symmetric"):
        logs = "".join(p.read_text(errors="replace") for p in run.glob("ALGTEST_log_*.log"))
        check("Testing complete" in logs or mode == "symmetric", f"{tag}.testing_complete")
        check(not re.search(r"SW\s*=\s*6f00", logs, re.I), f"{tag}.log_no_6f00")
    if mode == "all":
        secs = ident.get("uninstall_seconds", "")
        check(bool(secs) and float(secs) < 10, f"{tag}.gp_delete_time", f"seconds={secs or 'missing'}")
        listing = run / "gp-list-after-uninstall.txt"
        text = listing.read_text(errors="replace") if listing.is_file() else ""
        check("ISD:" in text and "APP:" not in text and "PKG:" not in text, f"{tag}.gp_list_isd_only")


def check_reconnect(csv: pathlib.Path) -> None:
    rows = [l for l in csv.read_text(errors="replace").splitlines()[1:] if l.strip()]
    good = [l for l in rows if "9000" in l]
    check(len(good) >= 3, "reconnect.three_cold_timings", f"rows={len(rows)} ok={len(good)}")


def main(args: list[str]) -> int:
    if not args:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as tmp:
        for i, arg in enumerate(args):
            root = pathlib.Path(arg)
            if root.suffix == ".zip":
                dest = pathlib.Path(tmp) / str(i)
                dest.mkdir()
                unpack(root, dest)
                root = dest
            runs = sorted({p.parent for p in root.rglob("run-identity.txt")})
            check(bool(runs), f"{arg}.has_runs", f"runs={len(runs)}")
            for run in runs:
                check_run(run)
            for csv in root.rglob("reconnect.csv"):
                check_reconnect(csv)
    print(f"CHECK:SUMMARY fail={failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
