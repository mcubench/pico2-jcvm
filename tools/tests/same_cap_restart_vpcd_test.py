#!/usr/bin/env python3
"""Verify same-CAP sibling static state and identity survive emulator restart."""

from __future__ import annotations

import socket
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402

PACKAGE_AID = bytes.fromhex("A000000151000005")
WRITER_AID = bytes.fromhex("A00000015100000501")
READER_AID = bytes.fromhex("A00000015100000502")
CONNECT_TIMEOUT = 45.0
APDU_TIMEOUT = 30.0
SHUTDOWN_TIMEOUT = 30.0


def select(connection: socket.socket, aid: bytes, label: str) -> None:
    command = bytes((0x00, 0xA4, 0x04, 0x00, len(aid))) + aid + b"\x00"
    response = vpcd.exchange(connection, command, label)
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label} failed: {response.hex().upper()}")


def connect(listener: socket.socket, process: subprocess.Popen[bytes],
            log_path: Path, phase: str) -> tuple[socket.socket, bytes]:
    connection, _ = vpcd.accept_vpcd(
        listener, process, CONNECT_TIMEOUT, log_path, phase)
    connection.settimeout(APDU_TIMEOUT)
    vpcd.send_message(connection, b"\x01")
    vpcd.send_message(connection, b"\x04")
    atr = vpcd.receive_message(connection)
    if atr != bytes.fromhex("3B800181"):
        raise RuntimeError(f"{phase} ATR {atr.hex().upper()}")
    return connection, atr


def main() -> int:
    subprocess.run([str(ROOT / "tools/build-same-cap-sibling-fixture")],
                   cwd=ROOT, check=True, timeout=180)
    cap_path = (ROOT / "build/same-cap-sibling-fixture/cap/"
                "samecap/javacard/samecap.cap")
    code, package_aid, applet_aids = vpcd.load_code(cap_path)
    if package_aid != PACKAGE_AID or applet_aids != [WRITER_AID, READER_AID]:
        raise RuntimeError("same-CAP fixture AID mismatch")

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/algtest-v182-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    memory = ROOT / "build/same-cap-sibling-fixture/restart-vpcd.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/same-cap-restart-vpcd-{timestamp}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        host, port = listener.getsockname()
        command = [
            str(ROOT / "tools/env"), str(emulator), "-m", str(memory), "-s",
            "--vpcd", f"{host}:{port}", "--applet-aid",
            vpcd.EMPTY_CARD_AID.hex(), "--package-id", "63", "--cap-code",
            str(dummy), "--rom-mask", str(rom_mask),
        ]
        process = None
        connection = None
        try:
            with log_path.open("wb") as log:
                process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                           stderr=log, start_new_session=True)
            connection, _ = connect(listener, process, log_path,
                                    "same-CAP initial run")
            response = vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                                     "SELECT ISD")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT ISD failed: {response.hex().upper()}")
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            vpcd.load_package(scp, code, package_aid)
            vpcd.install_applet(scp, package_aid, WRITER_AID, WRITER_AID)
            vpcd.install_applet(scp, package_aid, READER_AID, READER_AID)

            select(connection, WRITER_AID, "SELECT writer before restart")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "write shared static state")
            expected = bytes.fromhex("000012350157123456799000")
            if response != expected:
                raise RuntimeError(
                    f"initial writer state {response.hex().upper()}, "
                    f"expected {expected.hex().upper()}"
                )
            select(connection, READER_AID, "SELECT reader before restart")
            response = vpcd.exchange(connection, bytes.fromhex("8020000000"),
                                     "read sibling state before restart")
            if response != expected:
                raise RuntimeError(
                    f"initial sibling state {response.hex().upper()}, "
                    f"expected {expected.hex().upper()}"
                )
            print("TEST:PASS sibling applets shared static state before restart")

            response = vpcd.exchange(connection, bytes.fromhex("8021000000"),
                                     "verify sibling SIO identity before restart")
            if response != bytes.fromhex("019000"):
                raise RuntimeError(
                    f"pre-restart SIO identity {response.hex().upper()}"
                )
            print("TEST:PASS shared array identity before restart")
            connection.close()
            connection = None
            process.wait(timeout=SHUTDOWN_TIMEOUT)
            if process.returncode != 0:
                raise RuntimeError(
                    f"first emulator exited with {process.returncode}"
                )

            with log_path.open("ab") as log:
                process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                           stderr=log, start_new_session=True)
            connection, _ = connect(listener, process, log_path,
                                    "same-CAP restart")
            response = vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                                     "SELECT ISD after restart")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"SELECT ISD after restart {response.hex().upper()}"
                )
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            installed = vpcd.get_status(scp, 0x40)
            if WRITER_AID not in installed or READER_AID not in installed:
                raise RuntimeError("same-CAP applet registrations did not restore")
            print("TEST:PASS both sibling applet registrations restored")

            select(connection, READER_AID, "SELECT reader after restart")
            response = vpcd.exchange(connection, bytes.fromhex("8020000000"),
                                     "read static state after restart")
            if response != expected:
                raise RuntimeError(
                    "same-CAP static scalars/array did not persist: "
                    f"got {response.hex().upper()}, expected {expected.hex().upper()}"
                )
            print("TEST:PASS shared static state restored after restart")
            response = vpcd.exchange(connection, bytes.fromhex("8021000000"),
                                     "verify sibling SIO identity after restart")
            if response != bytes.fromhex("019000"):
                raise RuntimeError(
                    "shared static array identity did not restore after restart: "
                    f"{response.hex().upper()}"
                )
            print("TEST:PASS sibling SIO resolved restored shared array identity")

            select(connection, WRITER_AID, "SELECT writer after restart")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "update restored shared state")
            updated = bytes.fromhex("0000123602581234567A9000")
            if response != updated:
                raise RuntimeError(
                    f"post-restart writer state {response.hex().upper()}"
                )
            select(connection, READER_AID, "SELECT reader for post-restart update")
            response = vpcd.exchange(connection, bytes.fromhex("8020000000"),
                                     "read post-restart sibling update")
            if response != updated:
                raise RuntimeError(
                    f"post-restart sibling state {response.hex().upper()}"
                )
            print("TEST:PASS sibling updates remained shared after restart")
            print("TEST:SUMMARY pass=7 fail=0")
        finally:
            if connection is not None:
                connection.close()
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
    print(f"TEST:LOG {log_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
