#!/usr/bin/env python3
"""Test static scalar/reference sharing between applets in one CAP package."""

from __future__ import annotations

import os
import socket
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/integration"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402

PACKAGE_AID = bytes.fromhex("A000000151000005")
WRITER_AID = bytes.fromhex("A00000015100000501")
READER_AID = bytes.fromhex("A00000015100000502")
CONNECT_TIMEOUT = float(os.environ.get("SAME_CAP_CONNECT_TIMEOUT_S", "45"))
APDU_TIMEOUT = float(os.environ.get("SAME_CAP_APDU_TIMEOUT_S", "30"))


def select(connection: socket.socket, aid: bytes, label: str) -> None:
    command = bytes((0x00, 0xA4, 0x04, 0x00, len(aid))) + aid + b"\x00"
    response = vpcd.exchange(connection, command, label)
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label} failed: {response.hex().upper()}")


def main() -> int:
    vpcd.ensure_algtest_load_code(ROOT)
    if os.environ.get("SAME_CAP_REUSE_FIXTURE") != "1":
        subprocess.run([str(ROOT / "tools/build-same-cap-sibling-fixture")],
                       cwd=ROOT, check=True, timeout=180)
    cap_path = ROOT / "build/same-cap-sibling-fixture/cap/samecap/javacard/samecap.cap"
    code, package_aid, applet_aids = vpcd.load_code(cap_path)
    if package_aid != PACKAGE_AID or applet_aids != [WRITER_AID, READER_AID]:
        raise RuntimeError(
            "fixture AIDs differ from expected values: "
            f"package={package_aid.hex().upper()} "
            f"applets={[aid.hex().upper() for aid in applet_aids]}"
        )

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/tests/jcalgtest/algtest-v184-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")

    started = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    build = ROOT / "build/same-cap-sibling-fixture"
    memory = build / "sibling-vpcd.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/same-cap-sibling-vpcd-{started}.log"
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
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                       stderr=log, start_new_session=True)
        connection = None
        try:
            connection, _ = vpcd.accept_vpcd(listener, process, CONNECT_TIMEOUT,
                                              log_path, "same-CAP sibling")
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
            print("TEST:PASS loaded one CAP and installed its writer and reader applets")

            select(connection, READER_AID, "SELECT reader for shared object identity")
            response = vpcd.exchange(connection, bytes.fromhex("8021000000"),
                                     "compare static array identity across siblings")
            if response != bytes.fromhex("019000"):
                raise RuntimeError(
                    "same-CAP sibling applets did not observe one static array "
                    f"identity: {response.hex().upper()} expected 019000"
                )
            print("TEST:PASS sibling SIO call received the shared static array")

            select(connection, READER_AID, "SELECT reader for static initializers")
            response = vpcd.exchange(connection, bytes.fromhex("8020000000"),
                                     "read CAP static initial values")
            if response != bytes.fromhex("000012340056123456789000"):
                raise RuntimeError(
                    "CAP static primitive initial values were not observed: "
                    f"{response.hex().upper()} expected 000012340056123456789000"
                )
            print("TEST:PASS reader observed default and non-default static values")

            select(connection, WRITER_AID, "SELECT writer")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "write package static state")
            if response != bytes.fromhex("000012350157123456799000"):
                raise RuntimeError(
                    f"first writer response: {response.hex().upper()} "
                    "expected 000012350157123456799000"
                )
            print("TEST:PASS writer changed static scalar and byte array")

            select(connection, READER_AID, "SELECT reader after first write")
            response = vpcd.exchange(connection, bytes.fromhex("8020000000"),
                                     "read sibling package static state")
            if response != bytes.fromhex("000012350157123456799000"):
                raise RuntimeError(
                    "reader did not observe writer's shared array: "
                    f"{response.hex().upper()} expected 000012350157123456799000"
                )
            print("TEST:PASS reader observed first sibling static scalar and array")

            select(connection, WRITER_AID, "reselect writer")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "write sibling static state again")
            if response != bytes.fromhex("0000123602581234567A9000"):
                raise RuntimeError(f"second writer response: {response.hex().upper()}")
            select(connection, READER_AID, "reselect reader")
            response = vpcd.exchange(connection, bytes.fromhex("8020000000"),
                                     "read updated sibling package state")
            if response != bytes.fromhex("0000123602581234567A9000"):
                raise RuntimeError(
                    "reader observed stale or isolated sibling static array: "
                    f"{response.hex().upper()} expected 0000123602581234567A9000"
                )
            print("TEST:PASS retained sibling contexts observe updated package static state")
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
