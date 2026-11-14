#!/usr/bin/env python3
"""Exercise malformed CAP LOAD rejection and recovery over raw VPCD APDUs."""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from cap_load_code import ensure_algtest_load_code  # noqa: E402
from cap_malformed_vpcd_test import build_malformed_load_code  # noqa: E402
from fixture_paths import fixture_path  # noqa: E402
from pc_vpcd_multipackage_test import (  # noqa: E402
    ISD_AID,
    Scp03,
    accept_vpcd,
    ber_length,
    delete_object,
    get_status,
    load_code,
    load_package,
    receive_message,
    send_message,
)


def encoded_aid(aid: bytes) -> bytes:
    return bytes((0x4F, len(aid))) + aid


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args()

    subprocess.run(
        [str(ROOT / "tools/build-pc"), "--profile", "max_crypto"],
        cwd=ROOT, check=True, timeout=600,
    )
    ensure_algtest_load_code(ROOT)

    cap_path = fixture_path("jcalgtest.cap", ROOT)
    core_cap = (fixture_path("api.caps", ROOT) / "AlgTestApiCore_305.cap")
    dummy_code = ROOT / "build/tests/jcalgtest/algtest-v184-load-code.bin"
    rom_mask = ROOT / "build/choupi-pc/flash"
    emulator = ROOT / "build/choupi-pc/choupi"
    for path in (cap_path, core_cap, dummy_code, rom_mask, emulator):
        if not path.is_file():
            raise RuntimeError(f"required test input is missing: {path}")

    malformed_code = build_malformed_load_code(
        "external-superclass-without-import"
    ).read_bytes()
    _, malformed_package, _ = load_code(cap_path)
    core_code, core_package, _ = load_code(core_cap)
    if malformed_package == core_package:
        raise RuntimeError("unrelated control package AID collides with malformed package")

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    memory = ROOT / "build/cap-missing-import-apdu-vpcd" / f"{timestamp}.bin"
    memory.parent.mkdir(parents=True, exist_ok=True)
    memory.write_bytes(b"")
    registry_path = Path(f"{memory}.cap-registry")
    registry_path.unlink(missing_ok=True)
    emulator_log = ROOT / f"artifacts/logs/cap-missing-import-apdu-{timestamp}.log"
    emulator_log.parent.mkdir(parents=True, exist_ok=True)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        host, port = listener.getsockname()
        command = [
            str(ROOT / "tools/env"), str(emulator), "-m", str(memory), "-s",
            "--vpcd", f"{host}:{port}", "--applet-aid",
            bytes.fromhex("4A43416C675465737431").hex(), "--package-id", "63",
            "--cap-code", str(dummy_code), "--rom-mask", str(rom_mask),
        ]
        child_env = os.environ.copy()
        child_env["PICO2_TEST_OBJECT_DELETION"] = "1"
        with emulator_log.open("wb") as log:
            process = subprocess.Popen(
                command, cwd=ROOT, stdout=log, stderr=log,
                start_new_session=True, env=child_env,
            )

        connection = None
        try:
            connection, _ = accept_vpcd(
                listener, process, args.timeout, emulator_log,
                "missing-Import APDU test",
            )
            connection.settimeout(args.timeout)
            send_message(connection, b"\x01")
            send_message(connection, b"\x04")
            atr = receive_message(connection)
            if atr != bytes.fromhex("3B800181"):
                raise RuntimeError(f"unexpected VPCD ATR {atr.hex().upper()}")
            print("TEST:PASS ATR 3B800181", flush=True)

            send_message(connection, bytes.fromhex("00A4040000"))
            response = receive_message(connection)
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT ISD failed: {response.hex().upper()}")
            scp = Scp03(connection)
            scp.authenticate()
            print("TEST:PASS SCP03 authentication", flush=True)

            load_package(scp, core_code, core_package)
            packages = get_status(scp, 0x20)
            if encoded_aid(core_package) not in packages:
                raise RuntimeError("unrelated control package is absent before malformed LOAD")
            print(
                f"TEST:PASS valid unrelated package loaded aid={core_package.hex().upper()}",
                flush=True,
            )

            init_data = (bytes((len(malformed_package),)) + malformed_package +
                         bytes((len(ISD_AID),)) + ISD_AID + b"\x00\x00\x00")
            scp.command(0xE6, 0x02, 0x00, init_data,
                        f"INSTALL [for load] {malformed_package.hex().upper()}")
            load_block = b"\xC4" + ber_length(len(malformed_code)) + malformed_code
            offset = 0
            block_number = 0
            final_response = b""
            while offset < len(load_block):
                chunk = load_block[offset:offset + 239]
                offset += len(chunk)
                final = offset == len(load_block)
                expected = (0x6A80,) if final else (0x9000, 0x6310)
                final_response = scp.command(
                    0xE8, 0x80 if final else 0x00, block_number, chunk,
                    f"malformed LOAD block {block_number}", expected,
                )
                print(
                    f"TEST:TRACE malformed-load block={block_number} "
                    f"bytes={len(chunk)} final={int(final)} "
                    f"sw={final_response[-2:].hex().upper()}",
                    flush=True,
                )
                block_number += 1
            if final_response[-2:] != b"\x6A\x80":
                raise RuntimeError(
                    "missing-Import LOAD did not return 6A80: "
                    f"{final_response.hex().upper()}"
                )
            print("TEST:PASS malformed external-superclass LOAD returned 6A80", flush=True)

            packages_after_rejection = get_status(scp, 0x20)
            if encoded_aid(malformed_package) in packages_after_rejection:
                raise RuntimeError("rejected package appears in GET STATUS")
            if encoded_aid(core_package) not in packages_after_rejection:
                raise RuntimeError("unrelated package disappeared after LOAD rejection")
            print("TEST:PASS GET STATUS excludes malformed package and retains control package", flush=True)

            delete_response = delete_object(
                scp, core_package, True,
                "DELETE unrelated package after malformed LOAD",
            )
            if delete_response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    "unrelated package DELETE did not return 9000: "
                    f"{delete_response.hex().upper()}"
                )
            packages_after_delete = get_status(scp, 0x20)
            if (encoded_aid(malformed_package) in packages_after_delete or
                    encoded_aid(core_package) in packages_after_delete):
                raise RuntimeError("package remains in GET STATUS after DELETE")
            print("TEST:PASS unrelated package DELETE returned 9000", flush=True)
            print("CAP_LOAD_APDU:PASS status=6A80 no_package=1 unrelated_delete=9000")
            print("TEST:SUMMARY pass=1 fail=0")

            connection.close()
            connection = None
            process.wait(timeout=args.timeout)
            if process.returncode != 0:
                raise RuntimeError(f"emulator exited with status {process.returncode}")
        finally:
            if connection is not None:
                connection.close()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)

    print(f"CAP_LOAD_APDU:ARTIFACTS log={emulator_log.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
