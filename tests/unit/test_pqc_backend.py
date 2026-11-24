#!/usr/bin/env python3
"""Run the NIST ACVP ML-KEM/ML-DSA subset through choupi/src/pqc on the host.

Usage: test_pqc_backend.py <pqc_backend_test binary> [vectors.json]
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
VECTORS = ROOT / "tests/fixtures/external/nist-acvp/975de31e/pqc-subset.json"


def main():
    binary = sys.argv[1]
    vectors = json.loads(pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else VECTORS).read_text())
    proc = subprocess.Popen([binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    failures, count, peaks = [], 0, {}

    def run(op, s, *args):
        proc.stdin.write(" ".join([op, str(s)] + [a if a else "-" for a in args]) + "\n")
        proc.stdin.flush()
        fields = proc.stdout.readline().split()
        peak = int(fields.pop()[5:])
        peaks[(op, s)] = max(peaks.get((op, s), 0), peak)
        return int(fields[0]), fields[1:]

    def check(name, cond):
        nonlocal count
        count += 1
        print(("TEST:PASS " if cond else "TEST:FAIL ") + name)
        if not cond:
            failures.append(name)

    for g in vectors["groups"]:
        k, s = g["kind"], g["set"]
        for c in g["cases"]:
            name = f"{k}-{s}-tc{c['tcId']}"
            if k == "mlkem-keygen":
                rc, out = run("mlkem_keygen", s, c["d"] + c["z"])
                check(name, rc == 0 and out == [c["ek"], c["dk"]])
            elif k == "mlkem-encaps":
                rc, out = run("mlkem_encaps", s, c["ek"], c["m"])
                check(name, rc == 0 and out == [c["c"], c["k"]])
            elif k == "mlkem-decaps":
                rc, out = run("mlkem_decaps", s, c["dk"], c["c"])
                check(name, rc == 0 and out == [c["k"]])
            elif k == "mlkem-encapsulationKeyCheck":
                rc, _ = run("mlkem_check_ek", s, c["ek"])
                check(name, (rc == 0) == c["testPassed"])
            elif k == "mlkem-decapsulationKeyCheck":
                rc, _ = run("mlkem_check_dk", s, c["dk"])
                check(name, (rc == 0) == c["testPassed"])
            elif k == "mldsa-keygen":
                rc, out = run("mldsa_keygen", s, c["seed"])
                check(name, rc == 0 and out == [c["pk"], c["sk"]])
                rc, out = run("mldsa_pk_from_sk", s, c["sk"])
                check(name + "-pk_from_sk", rc == 0 and out == [c["pk"]])
            elif k == "mldsa-siggen":
                mu = c.get("mu")
                if mu is None:
                    rc, out = run("mldsa_mu_sk", s, c["sk"], c.get("context", ""), c["message"])
                    mu = out[0]
                rnd = c.get("rnd", "00" * 32)  # deterministic variant: rnd = 0^32
                rc, out = run("mldsa_sign_mu", s, c["sk"], mu, rnd)
                check(name, rc == 0 and out == [c["signature"]])
            elif k == "mldsa-sigver":
                mu = c.get("mu")
                if mu is None:
                    rc, out = run("mldsa_mu_pk", s, c["pk"], c.get("context", ""), c["message"])
                    mu = out[0]
                rc, _ = run("mldsa_verify_mu", s, c["pk"], mu, c["signature"])
                check(name, (rc == 0) == c["testPassed"])
    proc.stdin.close()
    proc.wait()
    for (op, s), peak in sorted(peaks.items()):
        print(f"PEAK {op} {s} {peak}")
    print(f"TEST:SUMMARY pass={count - len(failures)} fail={len(failures)}")
    return 1 if failures or proc.returncode else 0


if __name__ == "__main__":
    sys.exit(main())
