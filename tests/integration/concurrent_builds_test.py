#!/usr/bin/env python3
"""Build PC, ARM and RISC-V profiles concurrently and verify lock provenance."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
COMMANDS = {
    "arm-base": [str(ROOT / "tools/build"), "arm", "--profile", "base"],
    "pc-max": [str(ROOT / "tools/build-pc"), "--profile", "max_crypto"],
    "riscv-symm": [str(ROOT / "tools/build"), "riscv", "--profile", "symm_crypto"],
}


def read_provenance(path: Path) -> dict[str, str]:
    return dict(line.split("=", 1) for line in path.read_text().splitlines()
                if "=" in line)


def generation_for(log: str, profile: str) -> dict[str, str]:
    pattern = re.compile(
        rf"^BUILD:GENERATION profile={re.escape(profile)} "
        r"support_selection_sha256=([0-9a-f]{64}) "
        r"rommask_sha256=([0-9a-f]{64}) "
        r"jni_header_sha256=([0-9a-f]{64})$",
        re.MULTILINE,
    )
    matches = pattern.findall(log)
    if not matches:
        raise RuntimeError(f"expected a generation record for {profile}, got none")
    if len(set(matches)) != 1:
        raise RuntimeError(f"generation records disagree for {profile}: {matches}")
    support, rommask, jni = matches[0]
    return {"support_selection_sha256": support,
            "rommask_sha256": rommask,
            "jni_header_sha256": jni}


def main() -> int:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    log_dir = ROOT / "artifacts/logs" / f"concurrent-builds-{stamp}"
    log_dir.mkdir(parents=True, exist_ok=False)

    processes: dict[str, tuple[subprocess.Popen[bytes], object]] = {}
    for name, command in COMMANDS.items():
        log_stream = (log_dir / f"{name}.log").open("wb")
        process = subprocess.Popen(command, cwd=ROOT, stdout=log_stream,
                                   stderr=subprocess.STDOUT)
        processes[name] = (process, log_stream)

    outcomes: dict[str, str] = {}
    logs: dict[str, str] = {}
    for name, (process, log_stream) in processes.items():
        try:
            status = process.wait(timeout=1800)
        except subprocess.TimeoutExpired as error:
            process.kill()
            process.wait()
            raise RuntimeError(f"{name} build timed out; see {log_dir}") from error
        finally:
            log_stream.close()
        output = (log_dir / f"{name}.log").read_text(errors="replace")
        logs[name] = output
        if status != 0:
            raise RuntimeError(f"{name} exited {status}; see {log_dir}/{name}.log\n{output[-4000:]}")
        outcomes[name] = "pass"

    expected_markers = {
        "arm-base": ("BUILD:PASS arch=arm", "profile=base"),
        "pc-max": ("ROMMASK:PASS", "profile=max_crypto"),
        "riscv-symm": ("BUILD:PASS arch=riscv", "profile=symm_crypto"),
    }
    owner_intervals: list[tuple[int, int, str]] = []
    for name, output in logs.items():
        for marker in expected_markers[name]:
            if marker not in output:
                raise RuntimeError(f"{name} log missing {marker!r}; see {log_dir}")
        acquired = re.findall(
            r"^BUILD:LOCK acquired scope=(.*?) inherited=0 at_ns=(\d+)$",
            output, re.MULTILINE)
        released = re.findall(
            r"^BUILD:LOCK released scope=(.*?) at_ns=(\d+)$",
            output, re.MULTILINE)
        if len(acquired) != 1 or len(released) != 1:
            raise RuntimeError(f"{name} missing single lock acquire/release record")
        owner_intervals.append((int(acquired[0][1]), int(released[0][1]), name))

    owner_intervals.sort()
    for previous, current in zip(owner_intervals, owner_intervals[1:]):
        if previous[1] > current[0]:
            raise RuntimeError(f"overlapping shared-build lock intervals: {previous} / {current}")

    for name, profile, provenance_path in (
        ("arm-base", "base", ROOT / "build/arm/build-provenance.txt"),
        ("riscv-symm", "symm_crypto", ROOT / "build/riscv/build-provenance.txt"),
    ):
        expected = generation_for(logs[name], profile)
        provenance = read_provenance(provenance_path)
        if provenance.get("profile") != profile:
            raise RuntimeError(f"{name} final provenance has profile {provenance.get('profile')}")
        for key, value in expected.items():
            if provenance.get(key) != value:
                raise RuntimeError(
                    f"{name} {key} mismatch: log={value} provenance={provenance.get(key)}")

    # The last owner must have produced the final shared PC artifacts.
    final_owner = owner_intervals[-1][2]
    final_profile = {"arm-base": "base", "pc-max": "max_crypto",
                     "riscv-symm": "symm_crypto"}[final_owner]
    final_generation = generation_for(logs[final_owner], final_profile)
    generation_dir = ROOT / "build/choupi-pc"
    check = subprocess.run(["sha256sum", "--check", "generation.sha256"],
                          cwd=generation_dir, text=True, capture_output=True,
                          check=False)
    if check.returncode != 0:
        raise RuntimeError(f"final PC generation manifest failed: {check.stdout}{check.stderr}")
    actual_rom = hashlib.sha256((generation_dir / "flash").read_bytes()).hexdigest()
    actual_jni = hashlib.sha256((generation_dir / "jni.hpp").read_bytes()).hexdigest()
    if actual_rom != final_generation["rommask_sha256"] or actual_jni != final_generation["jni_header_sha256"]:
        raise RuntimeError("final PC artifacts do not match the final lock owner's generation record")
    support_manifest = (generation_dir / "support-config.sha256").read_text().split()
    if not support_manifest or support_manifest[0] != final_generation["support_selection_sha256"]:
        raise RuntimeError("final support-selection manifest does not match the final lock owner")

    summary = {
        "commands": {name: COMMANDS[name] for name in COMMANDS},
        "outcomes": outcomes,
        "lock_intervals_ns": [
            {"start": start, "end": end, "owner": name}
            for start, end, name in owner_intervals
        ],
        "final_pc_profile": final_profile,
        "logs": {name: str((log_dir / f"{name}.log").relative_to(ROOT)) for name in COMMANDS},
    }
    summary_path = log_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    print(f"CONCURRENT_BUILDS:PASS profiles=base,max_crypto,symm_crypto artifacts={log_dir.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"CONCURRENT_BUILDS:FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
