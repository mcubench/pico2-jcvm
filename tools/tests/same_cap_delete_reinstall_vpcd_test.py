#!/usr/bin/env python3
"""Check owner-ID safety across same-CAP applet delete/reinstall."""

from __future__ import annotations

import socket
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402

CONNECT_TIMEOUT = 45.0
APDU_TIMEOUT = 30.0


def logged_owner_ids(log_path: Path, instance_aid: bytes) -> list[int]:
    log = log_path.read_text(encoding="utf-8", errors="replace")
    aid_hex = instance_aid.hex().upper()
    pattern = re.compile(
        rf"VPCD:APPLET_INSTANCE package-id=\d+ class-index=\d+ "
        rf"applet-aid=[0-9A-F]+ instance-aid={aid_hex} owner-id=(\d+)"
    )
    return [int(match.group(1)) for match in pattern.finditer(log)]


def wait_for_owner_ids(log_path: Path, instance_aid: bytes,
                       count: int) -> list[int]:
    deadline = time.monotonic() + 5.0
    owners = []
    while time.monotonic() < deadline:
        owners = logged_owner_ids(log_path, instance_aid)
        if len(owners) >= count:
            return owners
        time.sleep(0.02)
    raise RuntimeError(
        f"timed out waiting for {count} owner records for "
        f"{instance_aid.hex().upper()}; found {owners}"
    )


def select(connection: socket.socket, aid: bytes, label: str) -> None:
    command = bytes((0x00, 0xA4, 0x04, 0x00, len(aid))) + aid + b"\x00"
    response = vpcd.exchange(connection, command, label)
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label} failed: {response.hex().upper()}")


def main() -> int:
    subprocess.run([str(ROOT / "tools/build-same-cap-sibling-fixture")],
                   cwd=ROOT, check=True, timeout=180)
    cap_path = ROOT / "build/same-cap-sibling-fixture/cap/samecap/javacard/samecap.cap"
    code, package_aid, applet_aids = vpcd.load_code(cap_path)
    if len(applet_aids) != 2:
        raise RuntimeError(f"expected two sibling applets, got {len(applet_aids)}")
    writer_aid, reader_aid = applet_aids

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/algtest-v182-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    build = ROOT / "build/same-cap-sibling-fixture"
    memory = build / "delete-reinstall-vpcd.bin"
    memory.write_bytes(b"")
    registry_path = Path(f"{memory}.cap-registry")
    registry_path.unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/same-cap-delete-reinstall-vpcd-{timestamp}.log"
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
            connection, _ = vpcd.accept_vpcd(
                listener, process, CONNECT_TIMEOUT, log_path,
                "same-CAP delete/reinstall")
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("unexpected VPCD ATR")
            vpcd.exchange(connection, bytes.fromhex("00A4040000"), "SELECT ISD")
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            vpcd.load_package(scp, code, package_aid)
            vpcd.install_applet(scp, package_aid, writer_aid, writer_aid)
            vpcd.install_applet(scp, package_aid, reader_aid, reader_aid)

            writer_owners = wait_for_owner_ids(log_path, writer_aid, 1)
            reader_owners = wait_for_owner_ids(log_path, reader_aid, 1)
            old_writer_owner, reader_owner = writer_owners[0], reader_owners[0]
            if old_writer_owner == reader_owner:
                raise RuntimeError(
                    "sibling applets unexpectedly share an owner ID: "
                    f"writer={old_writer_owner}, reader={reader_owner}"
                )
            initial_owners = [old_writer_owner, reader_owner]
            print(f"TEST:PASS installed sibling owners {initial_owners}")

            select(connection, reader_aid, "SELECT reader before deletion")
            response = vpcd.exchange(
                connection, bytes.fromhex("8021000000"),
                "verify package-shared static array before deletion")
            if response != bytes.fromhex("019000"):
                raise RuntimeError(
                    f"shared array probe before DELETE: {response.hex().upper()}"
                )

            vpcd.exchange(connection, bytes.fromhex("00A4040000"), "reselect ISD")
            vpcd.delete_object(scp, writer_aid, False, "DELETE writer instance")
            vpcd.install_applet(scp, package_aid, writer_aid, writer_aid)
            writer_owners = wait_for_owner_ids(log_path, writer_aid, 2)
            reader_owners = wait_for_owner_ids(log_path, reader_aid, 1)
            new_writer_owner = writer_owners[-1]
            after_reinstall = [new_writer_owner, reader_owners[-1]]
            if new_writer_owner == old_writer_owner:
                raise RuntimeError(
                    "reinstalled applet reused an owner ID whose package-shared "
                    f"objects should still be live: old={old_writer_owner}, "
                    f"new map={after_reinstall}"
                )
            if after_reinstall[1] != reader_owner:
                raise RuntimeError(
                    f"reinstall changed surviving sibling owner: {after_reinstall}"
                )
            print(f"TEST:PASS DELETE/reinstall owner IDs {after_reinstall}")

            select(connection, reader_aid, "SELECT reader after reinstall")
            response = vpcd.exchange(
                connection, bytes.fromhex("8021000000"),
                "verify retained package-shared array after reinstall")
            if response != bytes.fromhex("019000"):
                raise RuntimeError(
                    "shared array identity changed after sibling reinstall: "
                    f"{response.hex().upper()}"
                )
            print("TEST:PASS shared object remains valid after sibling reinstall")

            pass_count = 4
            writer_owner_history = [old_writer_owner, new_writer_owner]
            for iteration in range(2):
                vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                              f"reselect ISD for reinstall {iteration + 2}")
                vpcd.delete_object(
                    scp, writer_aid, False,
                    f"DELETE writer instance {iteration + 2}")
                vpcd.install_applet(scp, package_aid, writer_aid, writer_aid)
                writer_owners = wait_for_owner_ids(
                    log_path, writer_aid, iteration + 3)
                repeated_owner = writer_owners[-1]
                if repeated_owner == old_writer_owner or repeated_owner == reader_owner:
                    raise RuntimeError(
                        "repeated reinstall reused an owner ID still assigned "
                        f"to a live sibling/object: history={writer_owners}, "
                        f"reader={reader_owner}"
                    )
                writer_owner_history.append(repeated_owner)
                select(connection, reader_aid,
                       f"SELECT reader after reinstall {iteration + 2}")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8021000000"),
                    f"verify shared array after reinstall {iteration + 2}")
                if response != bytes.fromhex("019000"):
                    raise RuntimeError(
                        "shared array lost after repeated reinstall: "
                        f"{response.hex().upper()}"
                    )
                pass_count += 2
            print(
                "TEST:PASS repeated DELETE/reinstall owners "
                f"{writer_owner_history}; retained owner={old_writer_owner}"
            )

            vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                          "reselect ISD before package DELETE")
            vpcd.delete_object(scp, writer_aid, False,
                               "DELETE final writer instance")
            vpcd.delete_object(scp, reader_aid, False,
                               "DELETE reader instance")
            vpcd.delete_object(scp, package_aid, True,
                               "DELETE package and shared heap")
            vpcd.load_package(scp, code, package_aid)
            vpcd.install_applet(scp, package_aid, writer_aid, writer_aid)
            writer_owners = wait_for_owner_ids(
                log_path, writer_aid, len(writer_owner_history) + 1)
            package_reinstall_owner = writer_owners[-1]
            if package_reinstall_owner != old_writer_owner:
                raise RuntimeError(
                    "owner ID was not reusable after package DELETE reclaimed "
                    f"the package heap: expected={old_writer_owner}, "
                    f"got={package_reinstall_owner}, history={writer_owners}"
                )
            pass_count += 1
            print(
                "TEST:PASS package DELETE/reinstall reclaimed owner ID "
                f"{package_reinstall_owner}"
            )

            print(f"TEST:SUMMARY pass={pass_count} fail=0")
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
