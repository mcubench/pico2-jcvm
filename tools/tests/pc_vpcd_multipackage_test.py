#!/usr/bin/env python3
"""Exercise multiple upstream CAP packages through authenticated raw VPCD."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
from pathlib import Path
from zipfile import ZipFile

from cryptography.hazmat.primitives import cmac
from cryptography.hazmat.primitives.ciphers import algorithms


COMPONENTS = (
    "Header", "Directory", "Applet", "Import", "ConstantPool", "Class",
    "Method", "StaticField", "RefLocation",
)
ISD_AID = bytes.fromhex("A000000151000000")
STATIC_KEY = bytes.fromhex("404142434445464748494A4B4C4D4E4F")
EMPTY_CARD_AID = bytes.fromhex("4A43416C675465737431")


def receive_exact(connection: socket.socket, length: int) -> bytes:
    data = bytearray()
    while len(data) < length:
        chunk = connection.recv(length - len(data))
        if not chunk:
            raise RuntimeError("VPCD connection closed while reading a frame")
        data.extend(chunk)
    return bytes(data)


def receive_message(connection: socket.socket) -> bytes:
    size = int.from_bytes(receive_exact(connection, 2), "big")
    if size == 0:
        raise RuntimeError("VPCD returned an empty frame")
    return receive_exact(connection, size)


def send_message(connection: socket.socket, payload: bytes) -> None:
    connection.sendall(len(payload).to_bytes(2, "big") + payload)


def exchange(connection: socket.socket, command: bytes, label: str) -> bytes:
    send_message(connection, command)
    response = receive_message(connection)
    if len(response) < 2:
        raise RuntimeError(f"{label}: response is shorter than SW1/SW2")
    return response


def aes_cmac(key: bytes, message: bytes) -> bytes:
    mac = cmac.CMAC(algorithms.AES(key))
    mac.update(message)
    return mac.finalize()


def scp03_kdf(key: bytes, constant: int, context: bytes,
              output_bits: int) -> bytes:
    if len(context) != 16:
        raise ValueError("SCP03 context must be 16 bytes")
    data = bytearray(32)
    data[11] = constant
    data[13:15] = output_bits.to_bytes(2, "big")
    data[15] = 1
    data[16:] = context
    return aes_cmac(key, bytes(data))


class Scp03:
    def __init__(self, connection: socket.socket):
        self.connection = connection
        self.chaining_value = bytes(16)

    def authenticate(self) -> None:
        host_challenge = bytes.fromhex("0123456789ABCDEF")
        response = exchange(
            self.connection,
            bytes.fromhex("8050000008") + host_challenge + b"\x00",
            "INITIALIZE UPDATE",
        )
        if response[-2:] != b"\x90\x00" or len(response) != 31:
            raise RuntimeError(
                f"INITIALIZE UPDATE failed: {response.hex().upper()}"
            )
        card_challenge = response[13:21]
        context = host_challenge + card_challenge
        session_mac = scp03_kdf(STATIC_KEY, 0x06, context, 128)
        host_cryptogram = scp03_kdf(session_mac, 0x01, context, 64)[:8]
        auth_data = host_cryptogram
        auth_header = bytes.fromhex("8482010010")
        mac_input = (self.chaining_value + auth_header[:4] +
                     bytes((len(auth_data) + 8,)) + auth_data)
        full_mac = aes_cmac(session_mac, mac_input)
        self.chaining_value = full_mac
        response = exchange(
            self.connection,
            auth_header + auth_data + full_mac[:8] + b"\x00",
            "EXTERNAL AUTHENTICATE",
        )
        if response[-2:] != b"\x90\x00":
            raise RuntimeError(
                f"EXTERNAL AUTHENTICATE failed: {response.hex().upper()}"
            )
        self.session_mac = session_mac

    def command(self, ins: int, p1: int, p2: int, data: bytes,
                label: str) -> bytes:
        if len(data) > 239:
            raise ValueError("short SCP03 command data exceeds one block")
        cla = 0x84
        lc = len(data) + 8
        header = bytes((cla, ins, p1, p2, lc))
        mac_input = (self.chaining_value + header[:4] + bytes((lc,)) + data)
        full_mac = aes_cmac(self.session_mac, mac_input)
        self.chaining_value = full_mac
        response = exchange(self.connection, header + data + full_mac[:8] + b"\x00",
                            label)
        if response[-2:] not in (b"\x90\x00", b"\x63\x10"):
            raise RuntimeError(f"{label} failed: {response.hex().upper()}")
        return response


def ber_length(length: int) -> bytes:
    if length < 0x80:
        return bytes((length,))
    if length <= 0xff:
        return bytes((0x81, length))
    if length <= 0xffff:
        return bytes((0x82, length >> 8, length & 0xff))
    raise ValueError("CAP component image is too large")


def lv(value: bytes) -> bytes:
    if len(value) > 0xff:
        raise ValueError("AID exceeds short LV length")
    return bytes((len(value),)) + value


def load_code(cap_path: Path) -> tuple[bytes, bytes, list[bytes]]:
    with ZipFile(cap_path) as cap:
        header_name = next(name for name in cap.namelist()
                           if name.endswith("/Header.cap"))
        applet_name = next(name for name in cap.namelist()
                           if name.endswith("/Applet.cap"))
        folder = header_name.rsplit("/", 1)[0]
        chunks = []
        for component in COMPONENTS:
            name = f"{folder}/{component}.cap"
            if name in cap.namelist():
                chunks.append(cap.read(name))
        code = b"".join(chunks)
        header = cap.read(header_name)
        applet_component = cap.read(applet_name)
    if len(header) < 13 or header[0] != 1:
        raise RuntimeError(f"invalid CAP Header component in {cap_path}")
    aid_length = header[12]
    if aid_length < 5 or aid_length > 16 or len(header) < 13 + aid_length:
        raise RuntimeError(f"invalid package AID in {cap_path}")
    package_aid = header[13:13 + aid_length]
    if len(applet_component) < 4 or applet_component[0] != 3:
        raise RuntimeError(f"invalid CAP Applet component in {cap_path}")
    applet_count = applet_component[3]
    cursor = 4
    applet_aids = []
    for _ in range(applet_count):
        if cursor >= len(applet_component):
            raise RuntimeError(f"truncated Applet component in {cap_path}")
        aid_len = applet_component[cursor]
        cursor += 1
        if aid_len < 5 or aid_len > 16 or cursor + aid_len + 2 > len(applet_component):
            raise RuntimeError(f"invalid applet AID in {cap_path}")
        applet_aids.append(applet_component[cursor:cursor + aid_len])
        cursor += aid_len + 2
    return code, package_aid, applet_aids


def load_package(scp: Scp03, code: bytes, package_aid: bytes) -> None:
    init = (lv(package_aid) + lv(ISD_AID) + b"\x00\x00\x00")
    scp.command(0xE6, 0x02, 0x00, init, f"LOAD init {package_aid.hex()}")
    block = b"\xC4" + ber_length(len(code)) + code
    offset = 0
    block_number = 0
    while offset < len(block):
        chunk = block[offset:offset + 239]
        offset += len(chunk)
        final = offset == len(block)
        scp.command(0xE8, 0x80 if final else 0x00, block_number,
                    chunk, f"LOAD block {package_aid.hex()} #{block_number}")
        block_number += 1


def install_applet(scp: Scp03, package_aid: bytes, applet_aid: bytes,
                   instance_aid: bytes) -> None:
    data = (lv(package_aid) + lv(applet_aid) + lv(instance_aid) +
            bytes.fromhex("03000000") + b"\x00\x00")
    scp.command(0xE6, 0x0C, 0x00, data,
                f"INSTALL {instance_aid.hex()}")


def get_status(scp: Scp03, p1: int) -> bytes:
    data = bytes.fromhex("4F00")
    response = scp.command(0xF2, p1, 0x02, data, f"GET STATUS P1={p1:02X}")
    page = bytearray(response[:-2])
    while response[-2:] == b"\x63\x10":
        response = scp.command(0xF2, p1, 0x03, data,
                               f"GET STATUS next P1={p1:02X}")
        page.extend(response[:-2])
    return bytes(page)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()
    root = args.root.resolve()
    cap_dir = root / "AlgTest_JavaCard/deploy/caps"
    core_cap = cap_dir / "AlgTestApiCore_305.cap"
    multi_cap = cap_dir / "AlgTestApiMulti_305.cap"
    rom_mask = root / "build/choupi-pc/flash"
    emulator = root / "build/choupi-pc/choupi"
    dummy_code = root / "build/algtest-v182-load-code.bin"
    for required in (core_cap, multi_cap, rom_mask, emulator, dummy_code):
        if not required.is_file():
            raise RuntimeError(f"required input is missing: {required}")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(args.timeout)
        host, port = listener.getsockname()
        memory = root / "artifacts/results/host-vpcd-multipackage-test.bin"
        memory.parent.mkdir(parents=True, exist_ok=True)
        memory.write_bytes(b"")
        log_path = root / "artifacts/logs/host-vpcd-multipackage-apdu-20261025.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        command = [
            str(root / "tools/env"), str(emulator), "-m", str(memory), "-s",
            "--vpcd", f"{host}:{port}", "--applet-aid",
            EMPTY_CARD_AID.hex(), "--package-id", "63", "--cap-code",
            str(dummy_code), "--rom-mask", str(rom_mask),
        ]
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, cwd=root, stdout=log,
                                       stderr=log, start_new_session=True)
        connection = None
        try:
            connection, _ = listener.accept()
            connection.settimeout(args.timeout)
            send_message(connection, b"\x01")
            send_message(connection, b"\x04")
            atr = receive_message(connection)
            if atr != bytes.fromhex("3B800181"):
                raise RuntimeError(f"unexpected VPCD ATR: {atr.hex().upper()}")
            print(f"TEST:PASS ATR {atr.hex().upper()}")

            select_isd = exchange(
                connection, bytes.fromhex("00A4040000"), "SELECT default ISD"
            )
            if select_isd[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"SELECT default ISD failed: {select_isd.hex().upper()}"
                )
            print("TEST:PASS SELECT default ISD")

            scp = Scp03(connection)
            scp.authenticate()
            print("TEST:PASS SCP03 authentication")

            core_code, core_package, core_applets = load_code(core_cap)
            multi_code, multi_package, multi_applets = load_code(multi_cap)
            if not core_applets or len(multi_applets) < 2:
                raise RuntimeError("Core/Multi CAPs do not contain expected applets")
            load_package(scp, core_code, core_package)
            install_applet(scp, core_package, core_applets[0], core_applets[0])
            load_package(scp, multi_code, multi_package)
            install_applet(scp, multi_package, multi_applets[0], multi_applets[0])
            install_applet(scp, multi_package, multi_applets[1], multi_applets[1])
            print("TEST:PASS loaded Core and Multi packages; installed 3 instances")

            packages = get_status(scp, 0x20)
            applets = get_status(scp, 0x40)
            for aid, label, registry in (
                (core_package, "Core package", packages),
                (multi_package, "Multi package", packages),
                (core_applets[0], "Core applet", applets),
                (multi_applets[0], "Multi applet A", applets),
                (multi_applets[1], "Multi applet B", applets),
            ):
                if aid not in registry:
                    raise RuntimeError(f"{label} absent from GET STATUS")
            print("TEST:PASS GET STATUS lists both packages and all 3 instances")

            unknown_aid = bytes.fromhex("F000000001")
            unknown_select = (bytes((0x00, 0xA4, 0x04, 0x00, len(unknown_aid))) +
                              unknown_aid + b"\x00")
            response = exchange(connection, unknown_select, "SELECT unknown AID")
            if response[-2:] != b"\x6a\x82":
                raise RuntimeError(
                    f"unknown AID returned {response.hex().upper()}, expected 6A82"
                )
            print("TEST:PASS unknown AID rejected with 6A82")

            for aid, label in (
                (core_applets[0], "Core"),
                (multi_applets[0], "Multi A"),
                (multi_applets[1], "Multi B"),
            ):
                command_apdu = bytes((0x00, 0xA4, 0x04, 0x00, len(aid))) + aid + b"\x00"
                response = exchange(connection, command_apdu, f"SELECT {label}")
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(f"SELECT {label} failed: {response.hex().upper()}")
                print(f"TEST:PASS SELECT {label}")
            print("TEST:SUMMARY pass=9 fail=0")
            return 0
        finally:
            if connection is not None:
                connection.close()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            print(f"VPCD:LOG {log_path.relative_to(root)}")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, socket.timeout, subprocess.SubprocessError) as exc:
        print(f"TEST:FAIL {exc}", file=sys.stderr)
        raise SystemExit(1)
