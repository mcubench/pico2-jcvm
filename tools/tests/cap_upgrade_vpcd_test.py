#!/usr/bin/env python3
"""Exercise changed-version same-AID CAP replacement and restart persistence."""

from __future__ import annotations

import hashlib
import os
import socket
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402

PACKAGE_AID = bytes.fromhex("A000000151000006")
APPLET_AID = bytes.fromhex("A00000015100000601")
CONNECT_TIMEOUT = float(os.environ.get("CAP_UPGRADE_CONNECT_TIMEOUT_S", "45"))
APDU_TIMEOUT = float(os.environ.get("CAP_UPGRADE_APDU_TIMEOUT_S", "30"))


def select(connection: socket.socket, aid: bytes, label: str) -> bytes:
    command = bytes((0x00, 0xA4, 0x04, 0x00, len(aid))) + aid + b"\x00"
    return vpcd.exchange(connection, command, label)


def select_isd(connection: socket.socket, label: str) -> None:
    response = vpcd.exchange(connection, bytes.fromhex("00A4040000"), label)
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label}: {response.hex().upper()}")


def read_version(connection: socket.socket, expected: bytes, label: str) -> None:
    response = vpcd.exchange(connection, bytes.fromhex("8010000000"), label)
    if response != expected + b"\x90\x00":
        expected_response = (expected + b"\x90\x00").hex().upper()
        raise RuntimeError(
            f"{label}: {response.hex().upper()}, expected "
            f"{expected_response}"
        )


def set_version_state(connection: socket.socket, expected: bytes,
                       label: str) -> None:
    response = vpcd.exchange(connection, bytes.fromhex("8011000000"), label)
    if response != b"\x90\x00":
        raise RuntimeError(f"{label}: {response.hex().upper()}")
    read_version(connection, expected, label + " read back")


def accept_vpcd(listener: socket.socket, process: subprocess.Popen[bytes],
                timeout: float, log_path: Path, phase: str
                ) -> tuple[socket.socket, object]:
    deadline = __import__("time").monotonic() + timeout
    while True:
        code = process.poll()
        if code is not None:
            tail = log_path.read_text(errors="replace")[-2000:]
            raise RuntimeError(
                f"{phase}: emulator exited {code} before VPCD connect; {tail}"
            )
        remaining = deadline - __import__("time").monotonic()
        if remaining <= 0:
            raise socket.timeout(f"{phase}: VPCD connection timed out")
        listener.settimeout(min(remaining, 0.25))
        try:
            return listener.accept()
        except socket.timeout:
            continue


def stop_emulator(connection: socket.socket,
                  process: subprocess.Popen[bytes]) -> None:
    connection.close()
    process.wait(timeout=30)
    if process.returncode != 0:
        raise RuntimeError(f"emulator exit status {process.returncode}")


def main() -> int:
    build = ROOT / "build/cap-upgrade-fixture"
    cap_paths = [build / f"{v}-cap/capupgrade/javacard/capupgrade.cap"
                 for v in ("v1", "v2")]
    parsed = [vpcd.load_code(path) for path in cap_paths]
    for code, package_aid, applets in parsed:
        if package_aid != PACKAGE_AID or applets != [APPLET_AID]:
            raise RuntimeError(
                f"CAP AIDs differ from fixture constants: "
                f"{package_aid.hex().upper()} "
                f"{[aid.hex().upper() for aid in applets]}"
            )
    v1_code, v2_code = parsed[0][0], parsed[1][0]
    if len(v2_code) <= len(v1_code):
        raise RuntimeError(
            f"V2 CAP load image must be longer: V1={len(v1_code)} "
            f"V2={len(v2_code)}"
        )

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/algtest-v182-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")

    started = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    memory = build / "upgrade-vpcd.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/cap-upgrade-vpcd-{started}.log"
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
        emulator_env = os.environ.copy()
        emulator_env["PICO2_PERSISTENT_INVENTORY"] = "once"
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                       stderr=log, start_new_session=True,
                                       env=emulator_env)
        connection: socket.socket | None = None
        try:
            connection, _ = accept_vpcd(listener, process, CONNECT_TIMEOUT,
                                         log_path, "initial emulator")
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("initial VPCD ATR mismatch")
            select_isd(connection, "SELECT ISD")
            scp = vpcd.Scp03(connection)
            scp.authenticate()

            v1_code, _, _ = parsed[0]
            vpcd.load_package(scp, v1_code, PACKAGE_AID)
            vpcd.install_applet(scp, PACKAGE_AID, APPLET_AID, APPLET_AID)
            print("TEST:PASS LOAD/INSTALL V1")
            if select(connection, APPLET_AID, "SELECT V1") != b"\x90\x00":
                raise RuntimeError("V1 SELECT did not return 9000")
            read_version(connection, bytes.fromhex("A11234"), "V1 defaults")
            set_version_state(connection, bytes.fromhex("A15566"), "mutate V1")

            select_isd(connection, "reselect ISD before instance DELETE")
            response = vpcd.delete_object(
                scp, APPLET_AID, False, "DELETE V1 applet instance"
            )
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"instance DELETE: {response.hex().upper()}")
            duplicate_init = (vpcd.lv(PACKAGE_AID) + vpcd.lv(vpcd.ISD_AID) +
                              b"\x00\x00\x00")
            begin = scp.command(
                0xE6, 0x02, 0x00, duplicate_init,
                "begin duplicate V2 LOAD while package retained"
            )
            if begin[-2:] != b"\x90\x00":
                raise RuntimeError(f"duplicate LOAD init: {begin.hex().upper()}")
            duplicate_block = b"\xC4" + vpcd.ber_length(len(parsed[1][0])) + parsed[1][0]
            duplicate_status = None
            for block_number, offset in enumerate(range(0, len(duplicate_block), 239)):
                chunk = duplicate_block[offset:offset + 239]
                final = offset + len(chunk) == len(duplicate_block)
                response = scp.command(
                    0xE8, 0x80 if final else 0x00, block_number, chunk,
                    f"duplicate V2 LOAD block #{block_number}",
                    (0x9000, 0x6A80)
                )
                if response[-2:] == b"\x6A\x80":
                    duplicate_status = response[-2:]
                    break
                if final:
                    duplicate_status = response[-2:]
            if duplicate_status != b"\x6A\x80":
                raise RuntimeError(
                    "duplicate LOAD blocks were not rejected with 6A80; "
                    f"final status={None if duplicate_status is None else duplicate_status.hex().upper()}"
                )
            print("TEST:PASS instance DELETE retains package; duplicate LOAD returns 6A80")

            response = vpcd.delete_object(
                scp, PACKAGE_AID, True, "DELETE V1 package"
            )
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"package DELETE: {response.hex().upper()}")
            v2_code, _, _ = parsed[1]
            vpcd.load_package(scp, v2_code, PACKAGE_AID)
            vpcd.install_applet(scp, PACKAGE_AID, APPLET_AID, APPLET_AID)
            print("TEST:PASS DELETE package then LOAD/INSTALL longer V2")
            if select(connection, APPLET_AID, "SELECT V2") != b"\x90\x00":
                raise RuntimeError("V2 SELECT did not return 9000")
            read_version(connection, bytes.fromhex("B2223344556677"),
                         "V2 fresh defaults after replacement")
            set_version_state(connection, bytes.fromhex("B271728391A2B3"),
                              "mutate V2")
            print("TEST:PASS replacement used V2 code/layout and fresh V2 state")

            stop_emulator(connection, process)
            connection = None
            with log_path.open("ab") as log:
                process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                           stderr=log, start_new_session=True,
                                           env=emulator_env)
            connection, _ = accept_vpcd(listener, process, CONNECT_TIMEOUT,
                                         log_path, "restart")
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("restart VPCD ATR mismatch")
            select_isd(connection, "SELECT ISD after restart")
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            if select(connection, APPLET_AID, "SELECT V2 after restart") != b"\x90\x00":
                raise RuntimeError("restarted V2 SELECT did not return 9000")
            read_version(connection, bytes.fromhex("B271728391A2B3"),
                         "V2 mutated state after restart")
            print("TEST:PASS V2 package and persistent fields restore after restart")

            select_isd(connection, "reselect ISD before shorter V1 replacement")
            response = vpcd.delete_object(
                scp, PACKAGE_AID, True, "DELETE V2 package"
            )
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"V2 package DELETE: {response.hex().upper()}")
            v1_code, _, _ = parsed[0]
            vpcd.load_package(scp, v1_code, PACKAGE_AID)
            vpcd.install_applet(scp, PACKAGE_AID, APPLET_AID, APPLET_AID)
            if select(connection, APPLET_AID, "SELECT shorter V1") != b"\x90\x00":
                raise RuntimeError("shorter V1 SELECT did not return 9000")
            read_version(connection, bytes.fromhex("A11234"),
                         "V1 fresh defaults after shorter replacement")
            set_version_state(connection, bytes.fromhex("A15566"),
                              "mutate shorter V1")
            print("TEST:PASS shorter V1 replacement clears V2 state and code tail")

            stop_emulator(connection, process)
            connection = None
            with log_path.open("ab") as log:
                process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                           stderr=log, start_new_session=True,
                                           env=emulator_env)
            connection, _ = accept_vpcd(listener, process, CONNECT_TIMEOUT,
                                         log_path, "shorter V1 restart")
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("shorter V1 restart VPCD ATR mismatch")
            select_isd(connection, "SELECT ISD after shorter V1 restart")
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            if select(connection, APPLET_AID,
                      "SELECT shorter V1 after restart") != b"\x90\x00":
                raise RuntimeError("restarted shorter V1 SELECT did not return 9000")
            read_version(connection, bytes.fromhex("A15566"),
                         "shorter V1 mutated state after restart")
            print("TEST:PASS shorter V1 behavior and state restore after restart")
            print("TEST:SUMMARY pass=1 fail=0")
        finally:
            if connection is not None:
                connection.close()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)

    print(f"TEST:CAP v1 bytes={len(parsed[0][0])} sha256={hashlib.sha256(parsed[0][0]).hexdigest()}")
    print(f"TEST:CAP v2 bytes={len(parsed[1][0])} sha256={hashlib.sha256(parsed[1][0]).hexdigest()}")
    print(f"TEST:LOG {log_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
