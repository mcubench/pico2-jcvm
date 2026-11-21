#!/usr/bin/env python3
"""CAPIN-07: an Export class_offset that is not a class record start.

Loads a build copy of the same-CAP sibling fixture (it has an Export
component) with the first Export class_offset set to 1, then expects
LOAD to reject it and the card manager to keep answering.
"""

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

EXPORT_TAG = 10
DESCRIPTOR_TAG = 11


def omit_descriptor(code: bytes) -> bytes:
    """Remove the optional Descriptor and clear its Directory size."""
    components = []
    offset = 0
    while offset < len(code):
        tag = code[offset]
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        component = bytearray(code[offset:offset + 3 + size])
        if tag == 2:
            descriptor_size = 3 + 2 * (DESCRIPTOR_TAG - 1)
            component[descriptor_size:descriptor_size + 2] = b"\0\0"
        if tag != DESCRIPTOR_TAG:
            components.append(component)
        offset += 3 + size
    return b"".join(components)


def mutate_export_offset(code: bytes) -> bytes:
    """Set the first Export class_offset to 1 in a load file's components."""
    offset = 0
    while offset + 3 <= len(code):
        tag = code[offset]
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        if tag == EXPORT_TAG:
            if size < 3 or code[offset + 3] == 0:
                raise RuntimeError("Export component has no class entry")
            mutated = bytearray(code)
            mutated[offset + 4:offset + 6] = (1).to_bytes(2, "big")
            return bytes(mutated)
        offset += 3 + size
    raise RuntimeError("fixture load file has no Export component")


def main() -> int:
    vpcd.ensure_algtest_load_code(ROOT)
    subprocess.run([str(ROOT / "tools/build-same-cap-sibling-fixture")],
                   cwd=ROOT, check=True, timeout=180)
    cap_path = ROOT / "build/same-cap-sibling-fixture/cap/samecap/javacard/samecap.cap"
    code, package_aid, _ = vpcd.load_code(cap_path)
    without_descriptor = omit_descriptor(code)

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/tests/jcalgtest/algtest-v184-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")

    started = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    memory = ROOT / "build/same-cap-sibling-fixture/export-offset-vpcd.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/cap-export-offset-vpcd-{started}.log"
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
                                       stderr=log, start_new_session=True,
                                       env=os.environ.copy())
        connection = None
        try:
            connection, _ = vpcd.accept_vpcd(listener, process, 45.0,
                                              log_path, "Export offset")
            connection.settimeout(30.0)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("unexpected VPCD ATR")
            if vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                             "SELECT ISD")[-2:] != b"\x90\x00":
                raise RuntimeError("SELECT ISD failed")
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            for case, valid_code in (("with Descriptor", code),
                                     ("without Descriptor", without_descriptor)):
                vpcd.load_package(scp, mutate_export_offset(valid_code),
                                  package_aid, expected_final_statuses=(0x6A80,))
                if package_aid in vpcd.get_status(scp, 0x20):
                    raise RuntimeError(f"package published after failed LOAD {case}")
                print(f"TEST:PASS final LOAD returned 6A80 {case}")
            vpcd.load_package(scp, without_descriptor, package_aid)
            if package_aid not in vpcd.get_status(scp, 0x20):
                raise RuntimeError("valid CAP without Descriptor was not published")
            print("TEST:PASS valid LOAD without Descriptor after rejection")
            if vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                             "SELECT ISD after rejection")[-2:] != b"\x90\x00":
                print("TEST:FAIL card manager did not answer after rejection")
                return 1
            print("TEST:PASS card manager answers after the rejection")
        finally:
            if connection is not None:
                connection.close()
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
    print("CAP_EXPORT_OFFSET:PASS export_class_offset_rejected log="
          f"{log_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
