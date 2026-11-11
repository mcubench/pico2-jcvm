#!/usr/bin/env python3
"""Exercise multiple upstream CAP packages through authenticated raw VPCD."""

from __future__ import annotations

import argparse
import os
import re
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from fixture_paths import fixture_path
from zipfile import ZipFile

from cryptography.hazmat.primitives import cmac
from cryptography.hazmat.primitives.ciphers import algorithms

from cap_load_code import ensure_algtest_load_code


COMPONENTS = (
    "Header", "Directory", "Applet", "Import", "ConstantPool", "Class",
    "Method", "StaticField", "RefLocation", "Export", "Descriptor",
)
ISD_AID = bytes.fromhex("A000000151000000")
STATIC_KEY = bytes.fromhex("404142434445464748494A4B4C4D4E4F")
EMPTY_CARD_AID = bytes.fromhex("4A43416C675465737431")


def sidecar_owner_ids(data: bytes) -> list[int]:
    if data[:5] != b"VCR1\x02" or len(data) < 14:
        raise RuntimeError("VPCD registry was not written as VCR2")
    offset = 9
    package_count = data[offset]
    offset += 1
    owner_ids = []

    def read_lv() -> None:
        nonlocal offset
        length = data[offset]
        offset += 1 + length

    for _ in range(package_count):
        offset += 1
        read_lv()
        code_length = int.from_bytes(data[offset:offset + 4], "big")
        offset += 4 + code_length
        instance_count = data[offset]
        offset += 1
        for _ in range(instance_count):
            read_lv()
            read_lv()
            offset += 2
            owner_ids.append(data[offset])
            offset += 1
    if offset != len(data) - 4:
        raise RuntimeError("VCR2 registry has malformed owner records")
    return owner_ids


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


def application_template(aid: bytes) -> bytes:
    if not 5 <= len(aid) <= 16:
        raise ValueError("application AID length must be between 5 and 16")
    return bytes((0x61, len(aid) + 2, 0x4f, len(aid))) + aid


def exchange(connection: socket.socket, command: bytes, label: str) -> bytes:
    try:
        send_message(connection, command)
        response = receive_message(connection)
    except (OSError, RuntimeError, socket.timeout) as exc:
        raise RuntimeError(f"{label}: {exc}") from exc
    if len(response) < 2:
        raise RuntimeError(f"{label}: response is shorter than SW1/SW2")
    return response


def collect_response_chain(response: bytes, fetch_next, label: str,
                           expect_chaining: bool = False) -> bytes:
    data = bytearray()
    if expect_chaining and (len(response) < 2 or response[-2] != 0x61):
        raise RuntimeError(f"{label}: expected initial 61xx, got {response.hex().upper()}")
    while True:
        if len(response) < 2:
            raise RuntimeError(f"{label}: response is shorter than SW1/SW2")
        data.extend(response[:-2])
        status = int.from_bytes(response[-2:], "big")
        if status == 0x9000:
            return bytes(data) + b"\x90\x00"
        if status >> 8 != 0x61:
            raise RuntimeError(f"{label}: response chaining failed: {response.hex().upper()}")
        response = fetch_next(status & 0xff)


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
                label: str,
                expected_statuses: tuple[int, ...] = (0x9000, 0x6310)) -> bytes:
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
        status = int.from_bytes(response[-2:], "big")
        if status not in expected_statuses and status >> 8 != 0x61:
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
        print(
            f"TEST:TRACE LOAD package={package_aid.hex().upper()} "
            f"block={block_number} bytes={len(chunk)} final={int(final)}",
            flush=True,
        )
        block_number += 1


def install_applet(scp: Scp03, package_aid: bytes, applet_aid: bytes,
                   instance_aid: bytes,
                   expected_statuses: tuple[int, ...] = (0x9000, 0x6310)
                   ) -> bytes:
    data = (lv(package_aid) + lv(applet_aid) + lv(instance_aid) +
            bytes.fromhex("03000000") + b"\x00\x00")
    return scp.command(0xE6, 0x0C, 0x00, data,
                       f"INSTALL {instance_aid.hex()}", expected_statuses)


def get_status(scp: Scp03, p1: int) -> bytes:
    data, _ = get_status_pages(scp, p1)
    return data


def get_status_pages(scp: Scp03, p1: int) -> tuple[bytes, list[int]]:
    data = bytes.fromhex("4F00")
    response = scp.command(0xF2, p1, 0x02, data, f"GET STATUS P1={p1:02X}")
    page = bytearray(response[:-2])
    statuses = [int.from_bytes(response[-2:], "big")]
    print(f"TEST:TRACE GET STATUS p1={p1:02X} page=1 data={len(response) - 2} sw={statuses[-1]:04X}")
    while response[-2:] == b"\x63\x10":
        response = scp.command(0xF2, p1, 0x03, data,
                               f"GET STATUS next P1={p1:02X}")
        page.extend(response[:-2])
        statuses.append(int.from_bytes(response[-2:], "big"))
        print(f"TEST:TRACE GET STATUS p1={p1:02X} page={len(statuses)} data={len(response) - 2} sw={statuses[-1]:04X}")
    return bytes(page), statuses


def delete_object(scp: Scp03, aid: bytes, related: bool, label: str,
                  expected_statuses: tuple[int, ...] = (0x9000,)) -> bytes:
    data = bytes((0x4f, len(aid))) + aid
    print(f"TEST:TRACE {label}:start", flush=True)
    response = scp.command(0xE4, 0x00, 0x80 if related else 0x00, data,
                           label, expected_statuses)
    print(f"TEST:TRACE {label}:done sw={response[-2:].hex().upper()}",
          flush=True)
    return response


def accept_vpcd(listener: socket.socket, process: subprocess.Popen[bytes],
                timeout: float, log_path: Path, phase: str
                ) -> tuple[socket.socket, object]:
    deadline = time.monotonic() + timeout
    while True:
        return_code = process.poll()
        if return_code is not None:
            tail = log_path.read_text(errors="replace")[-2000:]
            raise RuntimeError(
                f"{phase} emulator exited with {return_code} before VPCD "
                f"connect; log tail:\n{tail}"
            )
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise socket.timeout(f"timed out waiting for {phase} VPCD connect")
        listener.settimeout(min(remaining, 0.25))
        try:
            return listener.accept()
        except socket.timeout:
            continue


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument("--timeout", type=float, default=90)
    parser.add_argument(
        "--install-smoke", action="store_true",
        help="stop after loading the Core and Multi packages and installing "
             "their applets",
    )
    args = parser.parse_args()
    root = args.root.resolve()
    ensure_algtest_load_code(root)
    cap_dir = fixture_path("api.caps", root)
    core_cap = cap_dir / "AlgTestApiCore_305.cap"
    multi_cap = cap_dir / "AlgTestApiMulti_305.cap"
    lookup_fixture = root / "tools/build-lookup-aid-fixture"
    subprocess.run([str(lookup_fixture)], cwd=root, check=True, timeout=120)
    lookup_cap = root / "build/lookup-aid-fixture/cap/lookup/javacard/lookup.cap"
    transaction_fixture = root / "tools/build-jcsystem-transaction-probe"
    subprocess.run([str(transaction_fixture)], cwd=root, check=True, timeout=120)
    transaction_cap = root / "build/jcsystem-transaction-probe/output/JCTransactionProbe.cap"
    rom_mask = root / "build/choupi-pc/flash"
    emulator = root / "build/choupi-pc/choupi"
    dummy_code = root / "build/tests/jcalgtest/algtest-v184-load-code.bin"
    for required in (core_cap, multi_cap, lookup_cap, transaction_cap,
                    rom_mask, emulator, dummy_code):
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
        Path(f"{memory}.cap-registry").unlink(missing_ok=True)
        started_at = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        log_path = root / f"artifacts/logs/host-vpcd-multipackage-apdu-{started_at}.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        restart_log_path = root / (
            f"artifacts/logs/host-vpcd-multipackage-restart-{started_at}.log"
        )
        rejected_logs = []
        command = [
            str(root / "tools/env"), str(emulator), "-m", str(memory), "-s",
            "--vpcd", f"{host}:{port}", "--applet-aid",
            EMPTY_CARD_AID.hex(), "--package-id", "63", "--cap-code",
            str(dummy_code), "--rom-mask", str(rom_mask),
        ]
        emulator_env = os.environ.copy()
        emulator_env["PICO2_PERSISTENT_INVENTORY"] = "once"
        emulator_env["PICO2_TEST_OBJECT_DELETION"] = "1"
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, cwd=root, stdout=log,
                                       stderr=log, start_new_session=True,
                                       env=emulator_env)
        connection = None
        try:
            connection, _ = accept_vpcd(
                listener, process, args.timeout, log_path, "initial"
            )
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

            bootstrap_application_list = application_template(ISD_AID) + b"\x90\x00"
            for cla, ins, label in (
                (0x80, 0xca, "open GP GET DATA list"),
                (0x00, 0xcb, "open ISO GET DATA list"),
            ):
                response = collect_response_chain(
                    exchange(
                        connection,
                        bytes((cla, ins, 0x2f, 0x00, 0x02, 0x5c, 0x00, 0x00)),
                        label,
                    ),
                    lambda le: exchange(
                        connection, bytes((0x00, 0xc0, 0x00, 0x00, le)),
                        label + " GET RESPONSE",
                    ),
                    label,
                )
                if response != bootstrap_application_list:
                    raise RuntimeError(
                        f"{label} returned {response.hex().upper()}, expected "
                        f"{bootstrap_application_list.hex().upper()}"
                    )
            print("TEST:PASS open GP and ISO GET DATA return bootstrap application list")
            for cla, ins, bad_data, label in (
                (0x80, 0xca, None, "missing GP 2F00 tag list"),
                (0x00, 0xcb, None, "missing ISO 2F00 tag list"),
                (0x80, 0xca, b"\x5c\x01", "malformed GP 2F00 tag list"),
                (0x00, 0xcb, b"\x5c\x01", "malformed ISO 2F00 tag list"),
            ):
                command = (bytes((cla, ins, 0x2f, 0x00, 0x00))
                           if bad_data is None else
                           bytes((cla, ins, 0x2f, 0x00, len(bad_data))) +
                           bad_data + b"\x00")
                if exchange(connection, command, label) != b"\x6a\x80":
                    raise RuntimeError(f"{label}: expected 6A80")
            print("TEST:PASS GP and ISO 2F00 reject missing or malformed tag lists")

            scp = Scp03(connection)
            scp.authenticate()
            print("TEST:PASS SCP03 authentication")

            core_code, core_package, core_applets = load_code(core_cap)
            multi_code, multi_package, multi_applets = load_code(multi_cap)
            lookup_code, lookup_package, lookup_applets = load_code(lookup_cap)
            transaction_code, transaction_package, transaction_applets = load_code(
                transaction_cap
            )
            if not core_applets or len(multi_applets) < 2:
                raise RuntimeError("Core/Multi CAPs do not contain expected applets")
            if len(lookup_applets) != 1:
                raise RuntimeError("lookup fixture must contain one applet")
            lookup_second_aid = bytes.fromhex("A00000015100000402")
            load_package(scp, core_code, core_package)
            install_applet(scp, core_package, core_applets[0], core_applets[0])
            load_package(scp, multi_code, multi_package)
            install_applet(scp, multi_package, multi_applets[0], multi_applets[0])
            install_applet(scp, multi_package, multi_applets[1], multi_applets[1])
            if args.install_smoke:
                print("TEST:PASS loaded Core and Multi packages and installed "
                      "three applet instances")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            load_package(scp, lookup_code, lookup_package)
            install_applet(scp, lookup_package, lookup_applets[0],
                           lookup_applets[0])
            install_applet(scp, lookup_package, lookup_applets[0],
                           lookup_second_aid)
            pagination_aids = [
                bytes.fromhex("A000000151000005") + index.to_bytes(2, "big")
                for index in range(16)
            ]
            for pagination_index, instance_aid in enumerate(pagination_aids, 1):
                install_applet(scp, lookup_package, lookup_applets[0],
                               instance_aid)
                print(
                    f"TEST:TRACE pagination install "
                    f"{pagination_index}/{len(pagination_aids)}"
                )
            print("TEST:PASS loaded 3 packages and installed 21 applet instances")

            installed_aids_before_duplicate = get_status(scp, 0x40)
            duplicate_response = install_applet(
                scp, lookup_package, lookup_applets[0], lookup_second_aid,
                (0x6a80,))
            if duplicate_response[-2:] != b"\x6a\x80" or get_status(
                    scp, 0x40) != installed_aids_before_duplicate:
                raise RuntimeError(
                    "duplicate INSTALL did not fail without changing the registry: "
                    f"response={duplicate_response.hex().upper()}"
                )
            print("TEST:PASS duplicate INSTALL rejected before registry mutation")
            retry_instance_aid = bytes.fromhex("A00000015100000601")
            install_applet(scp, lookup_package, lookup_applets[0],
                           retry_instance_aid)
            delete_object(scp, retry_instance_aid, False,
                          "DELETE applet after rejected duplicate retry")
            if get_status(scp, 0x40) != installed_aids_before_duplicate:
                raise RuntimeError(
                    "fresh INSTALL after duplicate rejection changed registry "
                    "after its test instance was deleted"
                )
            print("TEST:PASS fresh INSTALL succeeds after duplicate rejection")

            scp.command(0xF2, 0x20, 0x02, b"\x4f\x00",
                        "GET STATUS continuation baseline")
            scp.command(0xF2, 0x40, 0x03, b"\x4f\x00",
                        "GET STATUS continuation category mismatch",
                        (0x6985,))
            print("TEST:PASS GET STATUS continuation rejects changed category")
            scp.command(0xF2, 0x20, 0x03,
                        b"\x4f\x05" + core_package[:5],
                        "GET STATUS continuation AID mismatch", (0x6985,))
            print("TEST:PASS GET STATUS continuation rejects changed AID search")

            packages = get_status(scp, 0x20)
            applets, applet_page_statuses = get_status_pages(scp, 0x40)
            modules = get_status(scp, 0x10)
            for aid, label, registry in (
                (core_package, "Core package", packages),
                (multi_package, "Multi package", packages),
                (lookup_package, "lookup package", packages),
                (core_applets[0], "Core applet", applets),
                (multi_applets[0], "Multi applet A", applets),
                (multi_applets[1], "Multi applet B", applets),
                (lookup_applets[0], "lookup applet", applets),
                (lookup_second_aid, "second lookup applet instance", applets),
            ):
                if aid not in registry:
                    raise RuntimeError(f"{label} absent from GET STATUS")
            expected_instance_aids = (
                core_applets[0:1] + multi_applets +
                lookup_applets[0:1] + [lookup_second_aid] + pagination_aids
            )
            expected_application_list = (
                application_template(ISD_AID) +
                b"".join(application_template(aid)
                         for aid in expected_instance_aids)
            )
            response = collect_response_chain(
                scp.command(0xca, 0x2f, 0x00, b"\x5c\x00",
                            "authenticated GET DATA list"),
                lambda le: scp.command(0xc0, 0x00, 0x00, b"",
                                       "authenticated GET RESPONSE"),
                "authenticated GET DATA list",
                expect_chaining=True,
            )
            if response != expected_application_list + b"\x90\x00":
                raise RuntimeError(
                    "authenticated GET DATA application list omitted, duplicated, "
                    "or reordered AIDs: " + response.hex().upper()
                )
            print("TEST:PASS authenticated GET DATA lists all 21 installed applications")
            if (len(applet_page_statuses) < 2 or
                    applet_page_statuses[-1] != 0x9000 or
                    any(status != 0x6310
                        for status in applet_page_statuses[:-1])):
                raise RuntimeError(
                    "GET STATUS did not return correctly terminated pages: "
                    f"{[f'{status:04X}' for status in applet_page_statuses]}"
                )
            for aid in expected_instance_aids:
                encoded_aid = bytes((0x4f, len(aid))) + aid
                if applets.count(encoded_aid) != 1:
                    raise RuntimeError(
                        "GET STATUS pagination omitted or duplicated applet "
                        f"AID {aid.hex().upper()}: count="
                        f"{applets.count(encoded_aid)}"
                    )
            print(
                "TEST:PASS GET STATUS paginates all 21 instances without "
                f"duplicates ({len(applet_page_statuses)} pages)"
            )
            for aid, label in (
                (core_package, "Core module package"),
                (multi_package, "Multi module package"),
                (core_applets[0], "Core module AID"),
                (multi_applets[0], "Multi module AID A"),
                (multi_applets[1], "Multi module AID B"),
                (lookup_package, "lookup module package"),
                (lookup_applets[0], "lookup module AID"),
            ):
                if aid not in modules:
                    raise RuntimeError(
                        f"{label} {aid.hex().upper()} absent from module GET STATUS; "
                        f"response={modules.hex().upper()}"
                    )
            print("TEST:PASS GET STATUS lists all 3 packages and 21 instances")
            print("TEST:PASS module GET STATUS lists all packages and module AIDs")

            lookup_select = bytes((0x00, 0xA4, 0x04, 0x00,
                                   len(lookup_applets[0]))) + lookup_applets[0] + b"\x00"
            response = exchange(connection, lookup_select, "SELECT AID lookup applet")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT lookup applet failed: {response.hex().upper()}")
            incoming_data = bytes(range(13))
            incoming_length_command = (
                bytes((0x00, 0x12, 0x00, 0x00, len(incoming_data))) +
                incoming_data + b"\x00"
            )
            incoming_length_response = exchange(
                connection, incoming_length_command,
                "APDU.getIncomingLength reports short APDU Lc",
            )
            if incoming_length_response != b"\x0d\x0d\x05\x01\x90\x00":
                raise RuntimeError(
                    "JC305 APDU getter mapping mismatch: "
                    f"{incoming_length_response.hex().upper()}"
                )
            print("TEST:PASS JC305 APDU length, data offset, and CLA getters")
            lookup_command = (bytes((0x00, 0x10, 0x00, 0x00,
                                     len(core_applets[0]))) +
                              core_applets[0] + b"\x00")
            lookup_first = exchange(
                connection, lookup_command,
                "lookupAID finds a separately installed applet",
            )
            if lookup_first != b"\x01" + core_applets[0] + b"\x90\x00":
                raise RuntimeError(
                    "lookupAID returned unexpected result for installed peer: "
                    f"{lookup_first.hex().upper()}"
                )
            lookup_repeat = exchange(
                connection, lookup_command,
                "lookupAID preserves AID object identity",
            )
            if lookup_repeat != b"\x02" + core_applets[0] + b"\x90\x00":
                raise RuntimeError(
                    "lookupAID did not return its cached object: "
                    f"{lookup_repeat.hex().upper()}"
                )
            lookup_second_select = (
                bytes((0x00, 0xA4, 0x04, 0x00, len(lookup_second_aid))) +
                lookup_second_aid + b"\x00"
            )
            response = exchange(
                connection, lookup_second_select,
                "SELECT second lookup applet context",
            )
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    "SELECT second lookup applet failed: "
                    f"{response.hex().upper()}"
                )
            lookup_cross_context = exchange(
                connection, lookup_command,
                "lookupAID returns the same EPO from a second applet context",
            )
            if lookup_cross_context != b"\x02" + core_applets[0] + b"\x90\x00":
                raise RuntimeError(
                    "lookupAID did not preserve EPO identity across applet "
                    f"contexts: {lookup_cross_context.hex().upper()}"
                )
            print("TEST:PASS JCSystem.lookupAID preserves AID identity across applet contexts")
            unknown_aid = bytes.fromhex("F000000001")
            unknown_command = (bytes((0x00, 0x10, 0x00, 0x00,
                                      len(unknown_aid))) +
                               unknown_aid + b"\x00")
            unknown_lookup = exchange(
                connection, unknown_command,
                "lookupAID returns null for unknown applet",
            )
            if unknown_lookup != b"\x00\x90\x00":
                raise RuntimeError(
                    f"lookupAID unknown result was {unknown_lookup.hex().upper()}"
                )
            print("TEST:PASS JCSystem.lookupAID resolves peer, preserves identity, and returns null for unknown AID")

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
            send_message(connection, b"\x02")
            send_message(connection, b"\x04")
            reset_atr = receive_message(connection)
            if reset_atr != bytes.fromhex("3B800181"):
                raise RuntimeError(
                    f"card reset returned unexpected ATR: {reset_atr.hex().upper()}"
                )
            response = exchange(
                connection, lookup_second_select,
                "reselect lookup applet after card reset",
            )
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    "SELECT lookup applet after card reset failed: "
                    f"{response.hex().upper()}"
                )
            lookup_after_reset = exchange(
                connection, lookup_command,
                "lookupAID preserves EPO identity across card reset",
            )
            if lookup_after_reset != b"\x02" + core_applets[0] + b"\x90\x00":
                raise RuntimeError(
                    "lookupAID did not preserve EPO identity after card reset: "
                    f"{lookup_after_reset.hex().upper()}"
                )
            print("TEST:PASS JCSystem.lookupAID preserves permanent AID data and identity after card reset")
            inventory_logs = log_path.read_text(errors="replace")
            if not re.search(
                    r"PERSISTENT_INVENTORY: owner=\d+.*applet_roots=1 "
                    r"trace_complete=1 owner_scan=1 .*"
                    r"unreachable_complete=1 unreachable_tags=\d+",
                    inventory_logs):
                raise RuntimeError(
                    "inventory did not trace the selected applet's own root "
                    "and complete its owner-tag scan; "
                    f"log={log_path}"
                )
            print("TEST:PASS inventory traces selected applet heap independently")

            # Exit the first process cleanly so it saves VM state, then resume
            # the saved card with all CAP packages and instance records.
            connection.close()
            connection = None
            process.wait(timeout=args.timeout)
            if process.returncode != 0:
                raise RuntimeError(
                    f"first VPCD process exited with {process.returncode}"
                )

            registry_path = Path(f"{memory}.cap-registry")
            registry_bytes = registry_path.read_bytes()
            owner_ids = sidecar_owner_ids(registry_bytes)
            if len(owner_ids) != 21 or any(owner_id == 0 for owner_id in owner_ids) or \
                    len(set(owner_ids)) != len(owner_ids):
                raise RuntimeError(
                    f"VCR2 did not persist 21 distinct applet owners: {owner_ids}"
                )
            print("TEST:PASS VCR2 persists distinct nonzero applet owner IDs")

            def expect_resume_rejected(label: str, sidecar: bytes | None,
                                       expected: str) -> None:
                probe_memory = memory.with_name(
                    f"{memory.stem}-{label}{memory.suffix}"
                )
                probe_memory.write_bytes(memory.read_bytes())
                probe_registry = Path(f"{probe_memory}.cap-registry")
                probe_log = root / (
                    f"artifacts/logs/host-vpcd-multipackage-{label}-"
                    f"{started_at}.log"
                )
                rejected_logs.append(probe_log)
                if sidecar is not None:
                    probe_registry.write_bytes(sidecar)
                probe_command = [
                    str(root / "tools/env"), str(emulator), "-m",
                    str(probe_memory), "--vpcd", "127.0.0.1:9",
                    "--applet-aid", core_applets[0].hex(), "--rom-mask",
                    str(rom_mask), "--resume",
                ]
                with probe_log.open("wb") as output:
                    result = subprocess.run(
                        probe_command, cwd=root, stdout=output,
                        stderr=subprocess.STDOUT, timeout=20, check=False,
                    )
                probe_output = probe_log.read_text(errors="replace")
                probe_registry.unlink(missing_ok=True)
                probe_memory.unlink(missing_ok=True)
                if (expected not in probe_output or
                        "PC storage initialization failed; refusing to start card service"
                        not in probe_output or
                        "VPCD:CONNECTED endpoint=" in probe_output or
                        "ERROR: connecting to vpcd endpoint" in probe_output):
                    raise RuntimeError(
                        f"{label} resume did not report {expected!r}; "
                        f"exit={result.returncode}, log={probe_log}"
                    )
                print(f"TEST:PASS {label} sidecar aborted card startup")

            damaged_registry = bytearray(registry_bytes)
            damaged_registry[-1] ^= 0x01
            expect_resume_rejected(
                "corrupt-sidecar", bytes(damaged_registry),
                "VPCD CAP registry sidecar CRC mismatch",
            )
            expect_resume_rejected(
                "missing-sidecar", None,
                "saved VPCD image has package 56 but registry sidecar is missing",
            )

            restart_command = [
                str(root / "tools/env"), str(emulator), "-m", str(memory), "-s",
                "--vpcd", f"{host}:{port}", "--applet-aid",
                core_applets[0].hex(), "--rom-mask", str(rom_mask), "--resume",
            ]
            with restart_log_path.open("wb") as restart_log:
                process = subprocess.Popen(
                    restart_command, cwd=root, stdout=restart_log,
                    stderr=restart_log, start_new_session=True,
                    env=emulator_env,
                )
            connection, _ = accept_vpcd(
                listener, process, args.timeout, restart_log_path, "restart"
            )
            restored_owners = [
                int(owner_id) for owner_id in re.findall(
                    r"owner-id=(\d+)",
                    restart_log_path.read_text(errors="replace"),
                )
            ]
            if restored_owners != owner_ids:
                raise RuntimeError(
                    "VCR2 applet owner IDs changed across restart: "
                    f"before={owner_ids}, after={restored_owners}"
                )
            print("TEST:PASS VCR2 owner IDs restore unchanged after process restart")
            connection.settimeout(args.timeout)
            send_message(connection, b"\x01")
            send_message(connection, b"\x04")
            atr = receive_message(connection)
            if atr != bytes.fromhex("3B800181"):
                raise RuntimeError(
                    f"restart returned unexpected VPCD ATR: {atr.hex().upper()}"
                )
            print("TEST:PASS fresh VPCD process restored saved VM image")
            select_isd = exchange(
                connection, bytes.fromhex("00A4040000"),
                "restart SELECT default ISD",
            )
            if select_isd[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"restart SELECT ISD failed: {select_isd.hex().upper()}"
                )
            scp = Scp03(connection)
            scp.authenticate()
            print("TEST:PASS restart SCP03 authentication")
            response = collect_response_chain(
                scp.command(0xca, 0x2f, 0x00, b"\x5c\x00",
                            "restart authenticated GET DATA list"),
                lambda le: scp.command(0xc0, 0x00, 0x00, b"",
                                       "restart authenticated GET RESPONSE"),
                "restart authenticated GET DATA list",
                expect_chaining=True,
            )
            if response != expected_application_list + b"\x90\x00":
                raise RuntimeError(
                    "restart GET DATA application list changed: " +
                    response.hex().upper()
                )
            print("TEST:PASS restart GET DATA restores all 21 application AIDs")
            packages = get_status(scp, 0x20)
            applets, restart_page_statuses = get_status_pages(scp, 0x40)
            modules = get_status(scp, 0x10)
            for aid, label, registry in (
                (core_package, "Core package after restart", packages),
                (multi_package, "Multi package after restart", packages),
                (lookup_package, "lookup package after restart", packages),
                (core_applets[0], "Core applet after restart", applets),
                (multi_applets[0], "Multi applet A after restart", applets),
                (multi_applets[1], "Multi applet B after restart", applets),
                (lookup_applets[0], "lookup applet after restart", applets),
                (lookup_second_aid, "second lookup applet after restart", applets),
            ):
                if aid not in registry:
                    raise RuntimeError(f"{label} absent from GET STATUS")
            if restart_page_statuses != applet_page_statuses:
                raise RuntimeError(
                    "GET STATUS page boundaries changed after process restart: "
                    f"before={[f'{status:04X}' for status in applet_page_statuses]}, "
                    f"after={[f'{status:04X}' for status in restart_page_statuses]}"
                )
            for aid in expected_instance_aids:
                encoded_aid = bytes((0x4f, len(aid))) + aid
                if applets.count(encoded_aid) != 1:
                    raise RuntimeError(
                        "restart GET STATUS pagination omitted or duplicated "
                        f"applet AID {aid.hex().upper()}: "
                        f"count={applets.count(encoded_aid)}"
                    )
            print(
                "TEST:PASS restart GET STATUS paginates all 21 instances "
                f"({len(restart_page_statuses)} pages)"
            )
            for aid, label in (
                (core_package, "Core module package after restart"),
                (multi_package, "Multi module package after restart"),
                (core_applets[0], "Core module AID after restart"),
                (multi_applets[0], "Multi module AID A after restart"),
                (multi_applets[1], "Multi module AID B after restart"),
                (lookup_package, "lookup module package after restart"),
                (lookup_applets[0], "lookup module AID after restart"),
            ):
                if aid not in modules:
                    raise RuntimeError(
                        f"{label} {aid.hex().upper()} absent from module GET STATUS "
                        f"after restart; response={modules.hex().upper()}"
                    )
            print("TEST:PASS restart GET STATUS lists all 3 packages and 21 instances")
            print("TEST:PASS restart module GET STATUS lists all packages and module AIDs")
            for aid, label in (
                (core_applets[0], "Core after restart"),
                (multi_applets[0], "Multi A after restart"),
                (multi_applets[1], "Multi B after restart"),
                (lookup_applets[0], "lookup applet after restart"),
                (lookup_second_aid, "second lookup applet after restart"),
            ):
                command_apdu = (
                    bytes((0x00, 0xA4, 0x04, 0x00, len(aid))) + aid + b"\x00"
                )
                response = exchange(connection, command_apdu, f"SELECT {label}")
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        f"{label} failed: {response.hex().upper()}"
                    )
            print("TEST:PASS restart SELECT routes all applet instances")
            lookup_after_restart = exchange(
                connection, lookup_command,
                "lookupAID preserves EPO identity across VM restart",
            )
            if lookup_after_restart != b"\x02" + core_applets[0] + b"\x90\x00":
                raise RuntimeError(
                    "lookupAID did not restore the permanent EPO identity "
                    f"after process restart: {lookup_after_restart.hex().upper()}"
                )
            print("TEST:PASS JCSystem.lookupAID preserves persistent AID identity after restart")
            restart_inventory_logs = restart_log_path.read_text(errors="replace")
            if not re.search(
                    r"PERSISTENT_INVENTORY: owner=\d+.*applet_roots=1 "
                    r"trace_complete=1 owner_scan=1 .*"
                    r"unreachable_complete=1 unreachable_tags=\d+",
                    restart_inventory_logs):
                raise RuntimeError(
                    "restart inventory did not trace the selected applet's "
                    "own root and complete its owner-tag scan; "
                    f"log={restart_log_path}"
                )
            print("TEST:PASS restart inventory traces selected applet heap")

            exchange(connection, bytes.fromhex("00A4040000"), "reselect ISD")
            delete_object(scp, multi_applets[0], False,
                          "DELETE Multi A instance")
            applets = get_status(scp, 0x40)
            if multi_applets[0] in applets or multi_applets[1] not in applets:
                raise RuntimeError("instance DELETE changed the wrong registry entries")
            print("TEST:PASS DELETE removes one instance only")

            refused = delete_object(
                scp, multi_package, False, "DELETE Multi package without cascade",
                (0x6985,),
            )
            if refused[-2:] != b"\x69\x85":
                raise RuntimeError(
                    f"package DELETE should refuse installed instance: {refused.hex().upper()}"
                )
            print("TEST:PASS package DELETE refuses live instances")
            delete_object(scp, multi_package, True,
                          "DELETE Multi package with cascade")
            packages = get_status(scp, 0x20)
            applets = get_status(scp, 0x40)
            if multi_package in packages or any(aid in applets for aid in multi_applets):
                raise RuntimeError("cascade DELETE left Multi registry entries")
            if core_package not in packages or core_applets[0] not in applets:
                raise RuntimeError("cascade DELETE removed unrelated Core entries")
            print("TEST:PASS cascade DELETE removes Multi only")

            delete_object(scp, lookup_package, True,
                          "DELETE lookup package and paginated instances")
            packages = get_status(scp, 0x20)
            applets = get_status(scp, 0x40)
            lookup_instance_aids = (
                lookup_applets + [lookup_second_aid] + pagination_aids
            )
            if lookup_package in packages or any(
                    aid in applets for aid in lookup_instance_aids):
                raise RuntimeError(
                    "lookup package cascade left its package or applet instances"
                )
            if core_package not in packages or core_applets[0] not in applets:
                raise RuntimeError(
                    "lookup package cascade removed unrelated Core entries"
                )
            print(
                "TEST:PASS cascade DELETE removes lookup package and all 18 "
                "instances only"
            )

            delete_object(scp, core_applets[0], False, "DELETE Core instance")
            delete_object(scp, core_package, False, "DELETE Core package")
            if get_status(scp, 0x20) or get_status(scp, 0x40):
                raise RuntimeError("registry is not empty after deleting Core")
            print("TEST:PASS DELETE leaves registry empty")

            # Exercise transient-array lifecycle after the existing registry,
            # lookup, reset, and deletion cases have completed. This keeps the
            # test-only CAP isolated from those earlier scenarios.
            if len(transaction_applets) != 1:
                raise RuntimeError("transaction probe CAP must contain one applet")
            load_package(scp, core_code, core_package)
            install_applet(scp, core_package, core_applets[0], core_applets[0])
            load_package(scp, transaction_code, transaction_package)
            transaction_aid = transaction_applets[0]
            install_applet(scp, transaction_package, transaction_aid,
                           transaction_aid)
            transaction_select = (
                bytes((0x00, 0xA4, 0x04, 0x00, len(transaction_aid))) +
                transaction_aid + b"\x00"
            )
            if exchange(connection, transaction_select,
                        "SELECT transaction probe")[-2:] != b"\x90\x00":
                raise RuntimeError("transaction probe SELECT failed")
            deletion_request = exchange(
                connection, bytes((0x80, 0x00, 34, 0x00, 0x00)),
                "request object deletion with retained peer package",
            )
            if deletion_request != b"\x00\x90\x00":
                raise RuntimeError(
                    "object deletion request with a retained peer package "
                    "returned " + deletion_request.hex().upper()
                )
            deletion_log = restart_log_path.read_text(errors="replace")
            deletion_result = re.search(
                r"OBJECT_DELETION: package=(\d+) owner=(\d+) "
                r"applet_result=(\d+) package_result=(\d+) "
                r"erased_applet=(\d+) erased_package=(\d+)",
                deletion_log,
            )
            if deletion_result is None:
                raise RuntimeError(
                    "deferred object deletion produced no completion diagnostic; "
                    f"log={restart_log_path}"
                )
            if (int(deletion_result.group(3)) != 0 or
                    int(deletion_result.group(4)) != 0 or
                    (int(deletion_result.group(5)) < 1 and
                     int(deletion_result.group(6)) < 1)):
                raise RuntimeError(
                    "object deletion did not reclaim the dropped persistent "
                    f"array cleanly: {deletion_result.group(0)}"
                )
            print("TEST:PASS deferred object deletion reclaims orphan with peer package installed")
            prepared = exchange(
                connection, bytes((0x80, 0x00, 30, 0x00, 0x00)),
                "prepare transient lifecycle values",
            )
            if prepared != b"\x01\x90\x00":
                raise RuntimeError(
                    "transient preparation returned " + prepared.hex().upper()
                )
            core_select = (
                bytes((0x00, 0xA4, 0x04, 0x00, len(core_applets[0]))) +
                core_applets[0] + b"\x00"
            )
            core_after_switch = exchange(
                connection, core_select, "switch to Core applet context"
            )
            if core_after_switch[-2:] != b"\x90\x00":
                raise RuntimeError(
                    "Core SELECT during transient switch returned "
                    + core_after_switch.hex().upper()
                )
            transaction_after_switch = exchange(
                connection, transaction_select,
                "switch back to transaction applet context",
            )
            if transaction_after_switch[-2:] != b"\x90\x00":
                raise RuntimeError(
                    "transaction probe SELECT during transient switch returned "
                    + transaction_after_switch.hex().upper()
                )
            transient_after_switch = exchange(
                connection, bytes((0x80, 0x00, 31, 0x00, 0x00)),
                "check transient lifecycle after cross-AID switch",
            )
            if transient_after_switch != b"\x03\x90\x00":
                raise RuntimeError(
                    "cross-AID switch lost transient lifecycle state: "
                    + transient_after_switch.hex().upper()
                )
            print("TEST:PASS transient lifecycle survives cross-AID switch")
            exchange(connection, bytes.fromhex("00A4040000"),
                     "select ISD before transient fixture cleanup")
            delete_object(scp, transaction_aid, False,
                          "DELETE transaction probe instance")
            delete_object(scp, transaction_package, False,
                          "DELETE transaction probe package")
            surviving_core_select = exchange(
                connection, core_select,
                "SELECT surviving Core after package DELETE",
            )
            if surviving_core_select[-2:] != b"\x90\x00":
                raise RuntimeError(
                    "surviving Core SELECT after package DELETE returned "
                    + surviving_core_select.hex().upper()
                )
            print("TEST:PASS surviving applet SELECT after package DELETE")
            exchange(connection, bytes.fromhex("00A4040000"),
                     "select ISD before final Core cleanup")
            delete_object(scp, core_applets[0], False,
                          "DELETE final Core instance")
            delete_object(scp, core_package, False,
                          "DELETE final Core package")
            if get_status(scp, 0x20) or get_status(scp, 0x40):
                raise RuntimeError(
                    "registry is not empty after transient probe cleanup"
                )
            print("TEST:PASS transient probe cleanup leaves registry empty")
            print("TEST:SUMMARY pass=48 fail=0")
            return 0
        finally:
            if connection is not None:
                connection.close()
            if process.poll() is None:
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            print(f"VPCD:LOG {log_path.relative_to(root)}")
            if restart_log_path.exists():
                print(f"VPCD:RESTART_LOG {restart_log_path.relative_to(root)}")
            for rejected_log in rejected_logs:
                print(f"VPCD:REJECT_LOG {rejected_log.relative_to(root)}")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, socket.timeout, subprocess.SubprocessError) as exc:
        print(f"TEST:FAIL {exc}", file=sys.stderr)
        raise SystemExit(1)
