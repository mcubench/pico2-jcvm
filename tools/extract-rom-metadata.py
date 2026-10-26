#!/usr/bin/env python3
"""Extract generated ROM package/class/field indexes for native modules."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--jni-header", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    header = args.jni_header.read_text(encoding="utf-8")
    definitions = re.findall(
        r"^#define ROM_(?:PACKAGE_INDEX|CLASS_INDEX|CLASS_EXPORT_TOKEN|FIELD_INDEX)_[A-Z0-9_]+ 0x[0-9A-Fa-f]+$",
        header,
        flags=re.MULTILINE,
    )
    if not definitions:
        parser.error(f"no ROM metadata macros found in {args.jni_header}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "/* Generated from the ROM-mask JNI header; do not edit. */\n#pragma once\n"
        + "\n".join(definitions)
        + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
