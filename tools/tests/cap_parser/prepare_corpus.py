#!/usr/bin/env python3
"""Extract upstream CAP component images and run the sanitizer harness."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import os
from pathlib import Path
from zipfile import BadZipFile, ZipFile


COMPONENTS = (
    "Header",
    "Directory",
    "Applet",
    "Import",
    "ConstantPool",
    "Class",
    "Method",
    "StaticField",
    "RefLocation",
    "Export",
    "Descriptor",
)


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: prepare_corpus.py TEST_BINARY CAP...", file=sys.stderr)
        return 2

    binary = Path(sys.argv[1])
    archives = [Path(argument) for argument in sys.argv[2:]]
    with tempfile.TemporaryDirectory(prefix="cap-parser-corpus-") as temp:
        raw_files: list[str] = []
        for index, archive in enumerate(archives):
            try:
                with ZipFile(archive) as source:
                    names = set(source.namelist())
                    prefix = next(
                        (
                            name[: -len("Header.cap")]
                            for name in names
                            if name.endswith("/javacard/Header.cap")
                        ),
                        None,
                    )
                    if prefix is None:
                        raise ValueError("missing Header.cap component")
                    components = []
                    for name in COMPONENTS:
                        entry = f"{prefix}{name}.cap"
                        if entry in names:
                            components.append(source.read(entry))
                    if not components:
                        raise ValueError("no CAP components found")
                    output = Path(temp) / f"{index:04d}-{archive.stem}.bin"
                    output.write_bytes(b"".join(components))
                    raw_files.append(str(output))
            except (BadZipFile, OSError, ValueError, StopIteration) as error:
                print(f"CAP_CORPUS:FAIL {archive}: {error}", file=sys.stderr)
                return 1

        result = subprocess.run(
            [
                str(binary),
                *raw_files,
            ],
            check=False,
            env={
                **os.environ,
                "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
            },
        )
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
