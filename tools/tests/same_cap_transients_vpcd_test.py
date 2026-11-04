#!/usr/bin/env python3
"""Check transient clear events while switching between same-CAP applets."""

from __future__ import annotations

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
WRITER_AID = bytes.fromhex("A00000015100000601")
READER_AID = bytes.fromhex("A00000015100000602")
CONNECT_TIMEOUT = float(os.environ.get("SAME_CAP_CONNECT_TIMEOUT_S", "45"))
APDU_TIMEOUT = float(os.environ.get("SAME_CAP_APDU_TIMEOUT_S", "30"))


def select(connection: socket.socket, aid: bytes, label: str) -> None:
    command = bytes((0x00, 0xA4, 0x04, 0x00, len(aid))) + aid + b"\x00"
    response = vpcd.exchange(connection, command, label)
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label} failed: {response.hex().upper()}")


def main() -> int:
    vpcd.ensure_algtest_v182_load_code(ROOT)
    subprocess.run([str(ROOT / "tools/build-same-cap-transient-fixture")],
                   cwd=ROOT, check=True, timeout=180)
    cap_path = (ROOT / "build/same-cap-transient-fixture/cap/"
                "samecaptransient/javacard/samecaptransient.cap")
    code, package_aid, applet_aids = vpcd.load_code(cap_path)
    if package_aid != PACKAGE_AID or applet_aids != [WRITER_AID, READER_AID]:
        raise RuntimeError(
            "fixture AIDs differ from expected values: "
            f"package={package_aid.hex().upper()} "
            f"applets={[aid.hex().upper() for aid in applet_aids]}"
        )

    cap_components = ROOT / "build/same-cap-transient-fixture/cap-components.bin"
    cap_components.write_bytes(code)

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/algtest-v182-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    memory = ROOT / "build/same-cap-transient-fixture/transient-vpcd.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/same-cap-transient-vpcd-{timestamp}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        host, port = listener.getsockname()
        resume_command = [
            str(emulator), "-m", str(memory), "-s", "--vpcd",
            f"{host}:{port}", "--applet-aid", WRITER_AID.hex(),
            "--restore-applet", "56", "22", WRITER_AID.hex(),
            WRITER_AID.hex(), str(cap_components),
            "--restore-applet", "56", "0", READER_AID.hex(),
            READER_AID.hex(), str(cap_components), "--rom-mask", str(rom_mask),
        ]
        command = [
            str(ROOT / "tools/env"), str(emulator), "-m", str(memory), "-s",
            "--vpcd", f"{host}:{port}", "--applet-aid",
            vpcd.EMPTY_CARD_AID.hex(), "--package-id", "63", "--cap-code",
            str(dummy), "--rom-mask", str(rom_mask),
        ]
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                       stderr=log, start_new_session=True)
        connection = None
        try:
            connection, _ = vpcd.accept_vpcd(
                listener, process, CONNECT_TIMEOUT, log_path,
                "same-CAP transient lifecycle")
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("unexpected VPCD ATR")
            response = vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                                     "SELECT ISD")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT ISD failed: {response.hex().upper()}")
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            vpcd.load_package(scp, code, package_aid)
            vpcd.install_applet(scp, package_aid, WRITER_AID, WRITER_AID)
            vpcd.install_applet(scp, package_aid, READER_AID, READER_AID)
            print("TEST:PASS installed same-CAP transient writer and reader")

            select(connection, WRITER_AID, "SELECT transient writer")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "set both transient arrays")
            if response != bytes.fromhex("5A6B9000"):
                raise RuntimeError(
                    f"transient write returned {response.hex().upper()}"
                )
            print("TEST:PASS writer initialized both transient arrays")

            response = vpcd.exchange(connection, bytes.fromhex("8012000000"),
                                     "read deselect callback count")
            if response != bytes.fromhex("009000"):
                raise RuntimeError(
                    f"unexpected initial deselect count {response.hex().upper()}"
                )
            vpcd.send_message(connection, b"\x02")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("unexpected direct-reset ATR")
            select(connection, WRITER_AID, "SELECT writer after active reset")
            response = vpcd.exchange(connection, bytes.fromhex("8011000000"),
                                     "read active transient arrays after reset")
            if response != bytes.fromhex("00009000"):
                raise RuntimeError(
                    "direct reset did not clear both transient classes: "
                    f"{response.hex().upper()}"
                )
            response = vpcd.exchange(connection, bytes.fromhex("8012000000"),
                                     "check reset skipped deselect callback")
            if response != bytes.fromhex("009000"):
                raise RuntimeError(
                    "reset invoked deselect callback: "
                    f"{response.hex().upper()}"
                )
            print("TEST:PASS active reset cleared both classes without deselect")

            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "reinitialize transient arrays")
            if response != bytes.fromhex("5A6B9000"):
                raise RuntimeError(
                    f"transient reinitialization returned {response.hex().upper()}"
                )

            select(connection, READER_AID, "switch to sibling transient reader")
            response = vpcd.exchange(connection, bytes.fromhex("8020000000"),
                                     "read transients after sibling switch")
            if response != bytes.fromhex("006B9000"):
                raise RuntimeError(
                    "same-package switch should clear only CLEAR_ON_DESELECT: "
                    f"got {response.hex().upper()}, expected 006B9000"
                )
            print("TEST:PASS sibling switch cleared deselect array and retained reset array")

            vpcd.send_message(connection, b"\x02")
            vpcd.send_message(connection, b"\x04")
            atr = vpcd.receive_message(connection)
            if atr != bytes.fromhex("3B800181"):
                raise RuntimeError(f"card reset ATR {atr.hex().upper()}")
            select(connection, WRITER_AID, "SELECT writer after reset")
            response = vpcd.exchange(connection, bytes.fromhex("8011000000"),
                                     "read transients after card reset")
            if response != bytes.fromhex("00009000"):
                raise RuntimeError(
                    "card reset should clear both transient arrays: "
                    f"{response.hex().upper()}"
                )
            print("TEST:PASS card reset cleared both sibling transient arrays")
            connection.close()
            connection = None
            process.terminate()
            process.wait(timeout=10)
            with log_path.open("ab") as log:
                process = subprocess.Popen(resume_command, cwd=ROOT, stdout=log,
                                           stderr=log, start_new_session=True)
            connection, _ = vpcd.accept_vpcd(
                listener, process, CONNECT_TIMEOUT, log_path,
                "same-CAP fresh-process restore")
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("unexpected fresh-process ATR")
            select(connection, WRITER_AID, "SELECT writer after fresh restore")
            response = vpcd.exchange(connection, bytes.fromhex("8013000000"),
                                     "read restored transient array lengths")
            if response != bytes.fromhex("04049000"):
                raise RuntimeError(
                    "restored transient arrays reported wrong logical lengths: "
                    f"{response.hex().upper()}"
                )
            response = vpcd.exchange(connection, bytes.fromhex("8011000000"),
                                     "read zeroed arrays after fresh restore")
            if response != bytes.fromhex("00009000"):
                raise RuntimeError(
                    "restored transient arrays were not zero initialized: "
                    f"{response.hex().upper()}"
                )
            print("TEST:PASS fresh restore preserves transient lengths and zeros")
            print("TEST:SUMMARY pass=6 fail=0")
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
    print(f"TEST:LOG {log_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
