"""Resolve pinned validation assets independently of the caller's cwd."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def manifest(root: Path = ROOT) -> dict:
    return json.loads((root / "tests/fixtures/manifest.json").read_text())


def fixture_path(key: str, root: Path = ROOT) -> Path:
    root = root.resolve()
    relative = manifest(root)["assets"][key]
    path = (root / relative).resolve()
    if not path.is_relative_to(root):
        raise ValueError(f"fixture escapes repository: {key}")
    if not path.exists():
        raise FileNotFoundError(f"missing fixture {key}: {path}")
    return path


def verify(root: Path = ROOT) -> int:
    root = root.resolve()
    data = manifest(root)
    for record in data["files"]:
        path = (root / record["path"]).resolve()
        if not path.is_relative_to(root):
            raise ValueError(f"fixture escapes repository: {record['path']}")
        contents = path.read_bytes()
        if len(contents) != record["size"] or hashlib.sha256(contents).hexdigest() != record["sha256"]:
            raise ValueError(f"fixture integrity failure: {record['path']}")
    for key in data["assets"]:
        fixture_path(key, root)
    caps = list(fixture_path("api.caps", root).glob("*.cap"))
    if len(caps) != data["active"]["javacard-api"]["cap_count"]:
        raise ValueError("API CAP corpus count changed")
    return len(data["files"])
