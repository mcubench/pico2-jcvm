"""Helpers for composing complete unchanged CAP component LOAD inputs."""

from pathlib import Path
from fixture_paths import fixture_path
from zipfile import ZipFile


COMPONENTS = (
    ("Header", 1),
    ("Directory", 2),
    ("Applet", 3),
    ("Import", 4),
    ("ConstantPool", 5),
    ("Class", 6),
    ("Method", 7),
    ("StaticField", 8),
    ("RefLocation", 9),
    ("Export", 10),
    ("Descriptor", 11),
)


def build_algtest_load_code(repo_root: Path) -> Path:
    """Extract the upstream AlgTest 1.8.4 CAP's ordered load components.

    Preserve the CAP archive unchanged. Descriptor is required because uploaded
    method execution uses its method-body ranges.
    """
    repo_root = repo_root.resolve()
    source = fixture_path("jcalgtest.cap", repo_root)
    target = repo_root / "build/tests/jcalgtest/algtest-v184-load-code.bin"
    target.parent.mkdir(parents=True, exist_ok=True)

    with ZipFile(source) as archive:
        header = [name for name in archive.namelist()
                  if name.endswith("/Header.cap")]
        if len(header) != 1:
            raise ValueError(f"expected one Header.cap in {source}")
        folder = header[0].rsplit("/", 1)[0]
        chunks = []
        for name, tag in COMPONENTS:
            member = f"{folder}/{name}.cap"
            try:
                chunk = archive.read(member)
            except KeyError:
                if name == "Export":
                    continue
                raise ValueError(f"CAP is missing required component {member}")
            if len(chunk) < 4 or chunk[0] != tag:
                raise ValueError(f"invalid {member} component header")
            declared = int.from_bytes(chunk[1:3], "big")
            if declared + 3 != len(chunk):
                raise ValueError(
                    f"{member} size mismatch: declared={declared} "
                    f"actual={len(chunk) - 3}"
                )
            chunks.append(chunk)

    if not any(chunk[0] == 11 for chunk in chunks):
        raise ValueError("upstream CAP is missing Descriptor.cap")
    target.write_bytes(b"".join(chunks))
    return target


def ensure_algtest_load_code(repo_root: Path) -> Path:
    """Re-extract the small input so stale timestamps cannot select old bytes."""
    return build_algtest_load_code(repo_root)


def main() -> int:
    import sys

    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
    target = build_algtest_load_code(root)
    import hashlib

    data = target.read_bytes()
    print(f"CAP_LOAD_CODE:PASS path={target} bytes={len(data)} "
          f"sha256={hashlib.sha256(data).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
