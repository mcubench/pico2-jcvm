#!/usr/bin/env python3
"""CAPIN-07: an implemented-interface index[] value beyond the class's
public virtual method tokens must be rejected at LOAD.

Uses a build copy of the RMI array fixture load file (its remote class
implements an internal interface). The fixture CAP is not modified.
"""

from __future__ import annotations

import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOAD_CODE = ROOT / "build/tests/fixtures/rmi-array-fixture/rmi-fixture-load-code.bin"
APPLET_AID = "A00000015100000101"


def components(code: bytes) -> dict[int, tuple[int, int]]:
    """Map component tag to (offset of its info, info size)."""
    result = {}
    offset = 0
    while offset + 3 <= len(code):
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        result[code[offset]] = (offset + 3, size)
        offset += 3 + size
    return result


def first_index_byte(code: bytes) -> int:
    """Absolute offset of the first index[] byte of an implemented interface."""
    comps = components(code)
    header_offset, _ = comps[1]
    minor = code[header_offset + 4]
    class_offset, class_size = comps[6]
    info = code[class_offset:class_offset + class_size]
    offset = 2 + int.from_bytes(info[0:2], "big") if minor >= 2 else 0
    while offset < len(info):
        flags, count = info[offset] >> 4, info[offset] & 0x0F
        if flags & 0x8:  # interface record
            end = offset + 1 + 2 * count
            if flags & 0x2:  # remote: name follows
                end += 1 + info[end]
            offset = end
            continue
        public_count, package_count = info[offset + 7], info[offset + 9]
        entry = offset + 10 + 2 * (public_count + package_count)
        for _ in range(count):
            index_count = info[entry + 2]
            if index_count:
                return class_offset + entry + 3
            entry += 3 + index_count
        if flags & 0x2:
            break  # remote class info follows; fixture parser stops here
        offset = entry
    raise RuntimeError("no implemented interface with index[] entries found")


def main() -> int:
    subprocess.run([str(ROOT / "tools/build-rmi-array-fixture")], cwd=ROOT,
                   check=True, timeout=300)
    code = bytearray(LOAD_CODE.read_bytes())
    index = first_index_byte(bytes(code))
    original = code[index]
    code[index] = 0x7F
    mutated = ROOT / "build/tests/fixtures/rmi-array-fixture/rmi-index-out-of-range.bin"
    mutated.write_bytes(code)
    print(f"INTERFACE_INDEX:MUTATED byte=0x{index:X} original=0x{original:02X} new=0x7F")

    started = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    log = ROOT / f"artifacts/logs/cap-interface-index-vpcd-{started}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [str(ROOT / "tools/test-pc-vpcd-applet"), "--passes", "1",
         "--cap-code", str(mutated), "--applet-aid", APPLET_AID,
         "--package-id", "63", "--log", str(log)],
        cwd=ROOT, check=False, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True)
    text = result.stdout
    if log.exists():
        text += log.read_text(encoding="utf-8", errors="replace")
    if (result.returncode == 0 or
            "ERROR: PC CAP LOAD data rejected" not in text or
            "ERROR: PC applet INSTALL failed" in text or
            re.search(r"VPCD:CAP_REGISTRY_SAVED packages=[1-9]\d*", text)):
        print(f"INTERFACE_INDEX:FAIL expected rejection before CAP storage; log={log}",
              file=sys.stderr)
        return 1
    print(f"INTERFACE_INDEX:PASS load_rejected log={log.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
