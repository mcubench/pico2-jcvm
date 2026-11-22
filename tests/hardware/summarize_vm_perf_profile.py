#!/usr/bin/env python3
"""Aggregate sampled normal-path phases from the isolated ARM benchmark."""

import argparse
from collections import defaultdict
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
TIMES = ("watchdog", "fetch", "bookkeeping", "decode")
OPCODE_TIMES = ("handler", "branch", "dynamic_cap", "rom_cap")
ARRAY_COUNTERS = ("array_metadata", "array_payload", "array_metadata_calls")
WORKLOADS = ("NATIVE_AES", "SW_AES", "XOR")


def read_profile(path):
    apdus = []
    current = None
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("VM2:APDU "):
            if current is not None:
                raise ValueError("truncated preceding VM2 APDU")
            values = dict(re.findall(r"(\w+)=(\w+)", line))
            ins = values.pop("ins")
            current = (ins, {name: int(value) for name, value in values.items()}, {})
        elif line.startswith("VM2:OP "):
            if current is None:
                raise ValueError("opcode row without a matching APDU")
            _, opcode, count, *times = line.split()
            columns = (OPCODE_TIMES + ARRAY_COUNTERS
                       if len(times) == len(OPCODE_TIMES) + len(ARRAY_COUNTERS)
                       else OPCODE_TIMES)
            current[2][int(opcode, 16)] = (
                int(count), dict(zip(columns, map(int, times), strict=True)))
        elif line == "VM2:END":
            if current is None:
                raise ValueError("unexpected VM2:END")
            apdus.append(current)
            current = None
        elif re.search(r"\b(FAULT|PANIC|TEST:FAIL)\b", line):
            raise ValueError(f"firmware failure in {path}")
    if current is not None:
        raise ValueError("truncated final VM2 APDU")
    return apdus


def labels_from_apdus(apdu_path):
    labels = []
    for line in apdu_path.read_text().splitlines():
        match = re.fullmatch(
            r"APDU:RESULT label=(\w+) round=\d+ sw=([0-9A-F]+) data=([0-9A-F]*) elapsed_ms=\d+",
            line,
        )
        if "APDU:TRANSPORT_FAIL" in line:
            raise ValueError(line)
        if match is None:
            continue
        label, status, data = match.groups()
        if status != "9000":
            raise ValueError(f"{label} failed with {status}")
        if label.startswith(WORKLOADS):
            if data != "AA":
                raise ValueError(f"{label} returned unexpected data {data}")
            labels.append(label)
    return labels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("serial", type=Path)
    parser.add_argument("apdus", type=Path)
    parser.add_argument("--repeats", default=50, type=int)
    args = parser.parse_args()
    names = dict(re.findall(
        r"/\* 0x([0-9A-F]{2}) \*/ BC_(\w+)",
        (ROOT / "choupi/src/jc_bytecodes/bytecode_values.hpp").read_text(),
    ))
    labels = labels_from_apdus(args.apdus)
    profiles = read_profile(args.serial)
    expected_labels = {f"{name}_{args.repeats}" for name in WORKLOADS}
    if not expected_labels.issubset(labels):
        parser.error(f"missing workload labels: {sorted(expected_labels - set(labels))}")
    for label, (ins, times, opcodes) in zip(labels, profiles, strict=True):
        expected = ("31" if label.endswith("_PREP") else "43") if label.startswith("NATIVE") else (
            "c0" if label.endswith("_PREP") else "a5")
        if ins != expected:
            raise ValueError(f"APDU mismatch for {label}: {ins}")
        if not label.endswith(f"_{args.repeats}"):
            continue
        sampled = sum(count for count, _ in opcodes.values())
        if not sampled:
            raise ValueError(f"{label}: no sampled opcodes")
        sums = defaultdict(int)
        for _, op in opcodes.values():
            for name, value in op.items():
                sums[name] += value
        measured = sum(times[name] for name in TIMES) + sums["handler"]
        print(f"{label} ops={times['ops']} sampled={sampled} sampled_us={measured}")
        for name in (*TIMES, *OPCODE_TIMES, *ARRAY_COUNTERS[:2]):
            elapsed = times.get(name, sums[name])
            print(f"  {name:14} {elapsed:8} us {elapsed / measured:7.2%}")
        for opcode, (count, op) in sorted(
            opcodes.items(), key=lambda item: item[1][1]["handler"], reverse=True
        )[:10]:
            hexcode = f"{opcode:02X}"
            print(f"  {hexcode} {names.get(hexcode, '?').lower():20} n={count:5} "
                  f"handler={op['handler']:8} us branch={op['branch']:8} us "
                  f"array_metadata={op.get('array_metadata', 0):8} us "
                  f"array_payload={op.get('array_payload', 0):8} us "
                  f"metadata_calls={op.get('array_metadata_calls', 0)}")


if __name__ == "__main__":
    main()
