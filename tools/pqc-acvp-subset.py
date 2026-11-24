#!/usr/bin/env python3
"""Extract a small, deterministic subset of NIST ACVP ML-KEM/ML-DSA vectors.

Input: a directory with the ACVP-Server gen-val/json-files folders
ML-KEM-keyGen-FIPS203, ML-KEM-encapDecap-FIPS203, ML-DSA-keyGen-FIPS204,
ML-DSA-sigGen-FIPS204 and ML-DSA-sigVer-FIPS204 (prompt.json and
expectedResults.json each). Output: one JSON file with prompt fields merged
with expected results, keeping the smallest cases per group.
"""
import argparse
import hashlib
import json
import pathlib

SETS = {"ML-KEM-512": 512, "ML-KEM-768": 768, "ML-KEM-1024": 1024,
        "ML-DSA-44": 44, "ML-DSA-65": 65, "ML-DSA-87": 87}


def load(root, name):
    d = root / name
    prompt = json.loads((d / "prompt.json").read_text())
    expected = json.loads((d / "expectedResults.json").read_text())
    results = {t["tcId"]: t for g in expected["testGroups"] for t in g["tests"]}
    digests = {f: hashlib.sha256((d / f).read_bytes()).hexdigest()
               for f in ("prompt.json", "expectedResults.json")}
    return prompt["testGroups"], results, digests


def size(t):
    return sum(len(v) for v in t.values() if isinstance(v, str))


def pick(group, results, count, fields, passed_mix=False):
    tests = sorted(group["tests"], key=lambda t: (size(t), t["tcId"]))
    if passed_mix:
        ok = [t for t in tests if results[t["tcId"]].get("testPassed")]
        bad = [t for t in tests if not results[t["tcId"]].get("testPassed")]
        tests = (ok[:1] + bad[:1] + ok[1:] + bad[1:])
    out = []
    for t in tests[:count]:
        case = {k: t[k] for k in fields if k in t}
        case.update({k: v for k, v in results[t["tcId"]].items()})
        out.append(case)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("acvp_dir", type=pathlib.Path)
    ap.add_argument("--revision", required=True, help="ACVP-Server commit")
    ap.add_argument("-o", "--output", type=pathlib.Path, required=True)
    a = ap.parse_args()
    out = {"source": "https://github.com/usnistgov/ACVP-Server/tree/"
                     f"{a.revision}/gen-val/json-files",
           "revision": a.revision, "source_sha256": {}, "groups": []}

    def add(kind, group, cases):
        out["groups"].append({"kind": kind, "set": SETS[group["parameterSet"]],
                              **{k: v for k, v in group.items()
                                 if k not in ("tests", "tgId", "parameterSet",
                                              "testType")},
                              "cases": cases})

    groups, res, dig = load(a.acvp_dir, "ML-KEM-keyGen-FIPS203")
    out["source_sha256"]["ML-KEM-keyGen-FIPS203"] = dig
    for g in groups:
        add("mlkem-keygen", g, pick(g, res, 2, ["tcId", "d", "z"]))
    groups, res, dig = load(a.acvp_dir, "ML-KEM-encapDecap-FIPS203")
    out["source_sha256"]["ML-KEM-encapDecap-FIPS203"] = dig
    for g in groups:
        f = g["function"]
        if f == "encapsulation":
            add("mlkem-encaps", g, pick(g, res, 2, ["tcId", "ek", "m"]))
        elif f == "decapsulation":
            add("mlkem-decaps", g, pick(g, res, 2, ["tcId", "dk", "c"]))
        else:
            add("mlkem-" + f, g, pick(g, res, 2, ["tcId", "ek", "dk"], True))
    groups, res, dig = load(a.acvp_dir, "ML-DSA-keyGen-FIPS204")
    out["source_sha256"]["ML-DSA-keyGen-FIPS204"] = dig
    for g in groups:
        add("mldsa-keygen", g, pick(g, res, 2, ["tcId", "seed"]))
    groups, res, dig = load(a.acvp_dir, "ML-DSA-sigGen-FIPS204")
    out["source_sha256"]["ML-DSA-sigGen-FIPS204"] = dig
    for g in groups:
        # HashML-DSA and Sign_internal on a raw message are not reachable
        # through the Java Card 3.2 preview API.
        if g.get("preHash") == "preHash" or g.get("externalMu") is False:
            continue
        add("mldsa-siggen", g, pick(g, res, 1, ["tcId", "message", "mu", "rnd",
                                               "sk", "context"]))
    groups, res, dig = load(a.acvp_dir, "ML-DSA-sigVer-FIPS204")
    out["source_sha256"]["ML-DSA-sigVer-FIPS204"] = dig
    for g in groups:
        if g.get("preHash") == "preHash" or g.get("externalMu") is False:
            continue
        add("mldsa-sigver", g, pick(g, res, 2, ["tcId", "pk", "message", "mu",
                                               "context", "signature"], True))
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(out, indent=1) + "\n")


if __name__ == "__main__":
    main()
