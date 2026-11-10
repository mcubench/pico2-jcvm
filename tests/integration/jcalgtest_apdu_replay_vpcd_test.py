#!/usr/bin/env python3
"""Replay a captured upstream JCAlgTest APDU log against PC VPCD."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import signal
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/integration"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402
from cap_load_code import ensure_algtest_load_code  # noqa: E402

APPLET_AID = bytes.fromhex("4A43416C675465737431")
ISD_AID = vpcd.ISD_AID
HEADER_RE = re.compile(r"^(?:CommmandAPDU|CommandAPDU):")
SW_RE = re.compile(r"\bSW=([0-9a-fA-F]{4})\b")
# GET DATA responses differ in the replay harness before the first application
# SELECT establishes its expected card-management state. These commands are
# outside the persistent-storage regression workload; keep them visible in the
# log without obscuring failures in captured applet operations.
KNOWN_REPLAY_STATUS_DIFFERENCES = {
    bytes.fromhex("80CA9F7F00"): 0x6982,
    bytes.fromhex("80CA006600"): 0x6982,
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_apdus(path: Path):
    lines = path.read_text(errors="replace").splitlines()
    index = 0
    while index < len(lines):
        if not HEADER_RE.match(lines[index]):
            index += 1
            continue
        if index + 1 >= len(lines):
            raise RuntimeError(f"truncated command after log line {index + 1}")
        encoded = re.sub(r"\s+", "", lines[index + 1])
        if not encoded or len(encoded) % 2 or not re.fullmatch(r"(?i)[0-9a-f]+", encoded):
            index += 1
            continue
        command = bytes.fromhex(encoded)
        expected_sw = None
        if index + 2 < len(lines):
            status = SW_RE.search(lines[index + 2])
            if status:
                expected_sw = int(status.group(1), 16)
        yield command, expected_sw, index + 1
        index += 2


def select_isd(connection: socket.socket) -> None:
    response = vpcd.exchange(connection, bytes.fromhex("00A4040000"), "SELECT ISD")
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"SELECT ISD failed: {response.hex().upper()}")


def start_emulator(memory: Path, log_path: Path, cap_code: Path,
                   rom_mask: Path, listener: socket.socket,
                   restore: tuple[int, int, bytes, bytes] | None):
    host, port = listener.getsockname()
    command = [
        str(ROOT / "tools/env"), str(ROOT / "build/choupi-pc/choupi"),
        "-m", str(memory), "-s", "--vpcd", f"{host}:{port}",
        "--applet-aid", APPLET_AID.hex(), "--rom-mask", str(rom_mask),
    ]
    if restore is not None:
        package_id, class_index, applet_aid, instance_aid = restore
        command.extend([
            "--restore-applet", str(package_id), str(class_index),
            applet_aid.hex(), instance_aid.hex(), str(cap_code),
        ])
    else:
        command.extend(["--package-id", "63", "--cap-code", str(cap_code)])
    env = os.environ.copy()
    with log_path.open("ab") as log:
        return subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=log,
                                start_new_session=True, env=env)


def accept_emulator(listener: socket.socket, process: subprocess.Popen[bytes],
                    timeout: float, log_path: Path) -> socket.socket:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        code = process.poll()
        if code is not None:
            tail = log_path.read_text(errors="replace")[-3000:]
            raise RuntimeError(f"emulator exited {code} before connect:\n{tail}")
        listener.settimeout(min(0.25, max(0.01, deadline - time.monotonic())))
        try:
            connection, _ = listener.accept()
            return connection
        except socket.timeout:
            pass
    raise TimeoutError(f"emulator did not connect within {timeout}s")


def run(args: argparse.Namespace) -> int:
    source_log = args.client_log.resolve()
    if not source_log.is_file() or not source_log.stat().st_size:
        raise RuntimeError(f"JCAlgTest client log is missing or empty: {source_log}")
    stop_hex = re.sub(r"\s+", "", args.stop_after_apdu).upper()
    if not re.fullmatch(r"[0-9A-F]{10,510}", stop_hex):
        raise ValueError("--stop-after-apdu must be a complete APDU hex string")
    stop_bytes = bytes.fromhex(stop_hex)

    load_image = ensure_algtest_load_code(ROOT)
    parsed_code, package_aid, applets = vpcd.load_code(args.cap)
    if parsed_code != load_image.read_bytes():
        raise RuntimeError("extracted CAP load image differs from validated fixture")
    if APPLET_AID not in applets:
        raise RuntimeError("upstream JCAlgTest CAP does not declare the requested applet AID")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    memory = args.out_dir / "empty-card.bin"
    if args.resume_card is None:
        memory.write_bytes(b"")
        restore_metadata = None
    else:
        resume_card = args.resume_card.resolve()
        resume_registry = Path(f"{resume_card}.cap-registry")
        metadata_log = resume_card.parent / "emulator.log"
        if not resume_card.is_file() or not resume_registry.is_file():
            raise RuntimeError(
                f"resume requires both {resume_card} and {resume_registry}")
        if not metadata_log.is_file():
            raise RuntimeError(f"resume applet metadata log is missing: {metadata_log}")
        metadata_match = re.search(
            r"VPCD:APPLET_INSTANCE package-id=(\d+) class-index=(\d+) "
            r"applet-aid=([0-9A-Fa-f]+) instance-aid=([0-9A-Fa-f]+)",
            metadata_log.read_text(errors="replace"),
        )
        if metadata_match is None:
            raise RuntimeError(
                f"resume applet metadata was not found in {metadata_log}")
        restore_metadata = (
            int(metadata_match.group(1)), int(metadata_match.group(2)),
            bytes.fromhex(metadata_match.group(3)),
            bytes.fromhex(metadata_match.group(4)),
        )
        memory.write_bytes(resume_card.read_bytes())
        Path(f"{memory}.cap-registry").write_bytes(
            resume_registry.read_bytes())
    emulator_log = args.out_dir / "emulator.log"
    client_log = args.out_dir / "replay.log"
    source_copy = args.out_dir / "source-client.log"
    source_copy.write_bytes(source_log.read_bytes())

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    for required in (emulator, rom_mask, args.cap, load_image):
        if not required.is_file() or not required.stat().st_size:
            raise RuntimeError(f"required VPCD replay input is missing: {required}")

    input_hashes = {
        "emulator": sha256_file(emulator),
        "rom_mask": sha256_file(rom_mask),
        "cap": sha256_file(args.cap),
        "load_image": sha256_file(load_image),
    }
    if args.resume_card is not None:
        input_hashes["card"] = sha256_file(args.resume_card.resolve())
        input_hashes["registry"] = sha256_file(
            Path(f"{args.resume_card.resolve()}.cap-registry"))
    print("REPLAY:INPUTS " + " ".join(
        f"{name}_sha256={digest}" for name, digest in input_hashes.items()),
        flush=True)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        process = start_emulator(memory, emulator_log, load_image, rom_mask,
                                 listener, restore_metadata)
        connection = None
        target_seen = False
        resource_failure = False
        changed_statuses = 0
        unexpected_statuses: list[str] = []
        replayed = 0
        try:
            connection = accept_emulator(listener, process, args.connect_timeout,
                                         emulator_log)
            connection.settimeout(args.apdu_timeout)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            atr = vpcd.receive_message(connection)
            if atr != bytes.fromhex("3B800181"):
                raise RuntimeError(f"unexpected VPCD ATR: {atr.hex().upper()}")
            print(f"REPLAY:ATR {atr.hex().upper()}", flush=True)

            select_isd(connection)
            if args.resume_card is None:
                scp = vpcd.Scp03(connection)
                scp.authenticate()
                vpcd.load_package(scp, parsed_code, package_aid)
                vpcd.install_applet(scp, package_aid, APPLET_AID, APPLET_AID)
                print("REPLAY:INSTALL upstream JCAlgTest 1.8.4/jc305 complete",
                      flush=True)
            else:
                select_applet = (bytes.fromhex("00A404000A") + APPLET_AID +
                                 b"\x00")
                selected = vpcd.exchange(connection, select_applet,
                                          "resume JCAlgTest applet SELECT")
                if selected[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        f"resumed applet SELECT failed: {selected.hex().upper()}")
                print(f"REPLAY:RESUME prior_card={resume_card} "
                      f"start_apdu={args.start_at_apdu}", flush=True)

            for command, original_sw, source_line in parse_apdus(source_log):
                replayed += 1
                if replayed < args.start_at_apdu:
                    continue
                response = vpcd.exchange(connection, command,
                                         f"replay APDU {replayed}")
                sw = int.from_bytes(response[-2:], "big")
                difference = original_sw is not None and sw != original_sw
                changed_statuses += int(difference)
                deletion_support_improvement = (
                    os.environ.get("PICO2_TEST_OBJECT_DELETION") == "1"
                    and command == bytes.fromhex("B0E2000000")
                    and original_sw == 0xF206 and sw == 0x9000
                )
                suffix = (f" original_sw={original_sw:04X} status_changed=1"
                          if difference else
                          (f" original_sw={original_sw:04X} status_changed=0"
                           if original_sw is not None else " original_sw=unknown"))
                print(
                    f"REPLAY:{replayed:04d} source_line={source_line} "
                    f"command={command.hex().upper()} response={response.hex().upper()} "
                    f"sw={sw:04X}{suffix}",
                    flush=True,
                )
                if command == stop_bytes:
                    target_seen = True
                if sw == 0xF205:
                    resource_failure = True
                    print(f"REPLAY:RESOURCE_FAILURE apdu={replayed:04d}", flush=True)
                # The source transcript contains historical F205 failures;
                # improvements from those are allowed. For every other
                # captured response, preserve its status, and never accept an
                # uncaught 6F00 as a successful replay.
                if (sw == 0x6F00 or
                        (original_sw is not None and original_sw != 0xF205
                         and sw != original_sw and
                         KNOWN_REPLAY_STATUS_DIFFERENCES.get(command) != sw and
                         not deletion_support_improvement)):
                    unexpected = (
                        f"apdu={replayed:04d} expected="
                        f"{original_sw:04X}" if original_sw is not None
                        else f"apdu={replayed:04d} expected=unknown"
                    ) + f" actual={sw:04X}"
                    unexpected_statuses.append(unexpected)
                    print(f"REPLAY:UNEXPECTED_STATUS {unexpected}", flush=True)
                elif (original_sw is not None and sw != original_sw):
                    label = ("TEST_ONLY_DELETION_IMPROVEMENT"
                             if deletion_support_improvement else
                             "KNOWN_STATUS_DIFFERENCE")
                    print(f"REPLAY:{label} apdu={replayed:04d} "
                          f"command={command.hex().upper()} "
                          f"expected={original_sw:04X} actual={sw:04X}",
                          flush=True)
                if target_seen:
                    break

            print(
                f"REPLAY:SUMMARY commands={replayed} target_seen={int(target_seen)} "
                f"resource_failure={int(resource_failure)} "
                f"changed_statuses={changed_statuses}",
                flush=True,
            )
            if not target_seen:
                raise RuntimeError(f"target APDU not found: {stop_hex}")
            if resource_failure:
                raise RuntimeError("replayed workload returned F205/NO_RESOURCE")
            if unexpected_statuses:
                raise RuntimeError(
                    f"replay had {len(unexpected_statuses)} unexpected status(es): "
                    + "; ".join(unexpected_statuses[:8])
                )
        finally:
            if connection is not None:
                connection.close()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    print(f"REPLAY:ARTIFACTS {args.out_dir}", flush=True)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-log", type=Path, required=True)
    parser.add_argument("--stop-after-apdu", required=True)
    parser.add_argument("--cap", type=Path, default=ROOT / "tests/fixtures/external/jcalgtest/1.8.4/AlgTest_v1.8.4_jc305.cap")
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--start-at-apdu", type=int, default=1)
    parser.add_argument("--resume-card", type=Path,
                        help="saved VPCD card image, registry, and emulator.log")
    # Restoring a large persistent JCAlgTest state can take over two minutes
    # before the emulator opens its VPCD connection, even when it later runs
    # the target APDU successfully.
    parser.add_argument("--connect-timeout", type=float, default=300)
    parser.add_argument("--apdu-timeout", type=float, default=600)
    args = parser.parse_args()
    if args.start_at_apdu < 1:
        parser.error("--start-at-apdu must be at least 1")
    if args.resume_card is not None and args.start_at_apdu == 1:
        parser.error("--resume-card requires --start-at-apdu greater than 1")
    try:
        return run(args)
    except Exception as exc:
        print(f"REPLAY:FAIL {exc}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
