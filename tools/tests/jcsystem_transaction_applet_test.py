#!/usr/bin/env python3
"""Exercise JCSystem transactions and Java exception reasons through VPCD."""

from __future__ import annotations

import argparse
import signal
import socket
import subprocess
import time
from pathlib import Path


AID = bytes.fromhex("4A43416C675465737431")


def receive_exact(connection: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        part = connection.recv(size - len(data))
        if not part:
            raise RuntimeError("VPCD peer closed the connection")
        data.extend(part)
    return bytes(data)


def receive_message(connection: socket.socket) -> bytes:
    size = int.from_bytes(receive_exact(connection, 2), "big")
    if size == 0:
        raise RuntimeError("VPCD peer sent an empty frame")
    return receive_exact(connection, size)


def send_message(connection: socket.socket, value: bytes) -> None:
    connection.sendall(len(value).to_bytes(2, "big") + value)


def exchange(connection: socket.socket, apdu: bytes, label: str) -> bytes:
    send_message(connection, apdu)
    response = receive_message(connection)
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label}: SW={response[-2:].hex().upper()} data={response.hex().upper()}")
    print(f"TEST:PASS {label} response={response.hex().upper()}")
    return response[:-2]


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=float, default=90)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    build = repo / "build/jcsystem-transaction-probe"
    emulator = repo / "build/choupi-pc/choupi"
    memory = repo / "artifacts/results/jcsystem-transaction-probe/FLASH_MEMORY"
    cap_code = build / "load-code.bin"
    rom_mask = repo / "build/choupi-pc/flash"
    for path in (emulator, cap_code, rom_mask):
        if not path.is_file():
            raise SystemExit(f"required input is missing: {path}; run tools/build-jcsystem-transaction-probe")
    memory.parent.mkdir(parents=True, exist_ok=True)
    memory.write_bytes(b"")
    log_path = args.log or (repo / "artifacts/logs/jcsystem-transaction-apdu-vpcd-20261016.log")
    log_path.parent.mkdir(parents=True, exist_ok=True)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(args.timeout)
        host, port = listener.getsockname()
        command = [
            str(emulator), "-m", str(memory), "-s", "--vpcd", f"{host}:{port}",
            "--applet-aid", AID.hex(), "--package-id", "63", "--cap-code",
            str(cap_code), "--rom-mask", str(rom_mask), "--install", "-s",
        ]
        started = time.monotonic()
        with log_path.open("w+b") as log:
            process = subprocess.Popen(
                command, cwd=repo, stdout=log, stderr=log,
                start_new_session=True,
            )
            connection = None
            try:
                deadline = time.monotonic() + args.timeout
                while connection is None:
                    if process.poll() is not None:
                        log.flush()
                        log.seek(0)
                        details = log.read().decode("utf-8", errors="replace")[-2000:]
                        raise RuntimeError(
                            f"emulator exited before VPCD connect with status {process.returncode}:\n{details}"
                        )
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("timed out waiting for emulator VPCD connect")
                    listener.settimeout(min(1.0, remaining))
                    try:
                        connection, _ = listener.accept()
                    except socket.timeout:
                        continue
                connection.settimeout(args.timeout)
                send_message(connection, b"\x01")
                send_message(connection, b"\x04")
                atr = receive_message(connection)
                if atr != bytes.fromhex("3B800181"):
                    raise RuntimeError(f"unexpected ATR {atr.hex().upper()}")
                print(f"TEST:PASS ATR response={atr.hex().upper()}")
                select = b"\x00\xA4\x04\x00" + bytes((len(AID),)) + AID + b"\x00"
                exchange(connection, select, "transaction probe SELECT")

                cases = (
                    (1, bytes.fromhex("0100"), "begin/depth/commit lifecycle"),
                    (2, b"\x02", "commit without transaction reason"),
                    (3, b"\x02", "abort without transaction reason"),
                    (4, bytes.fromhex("0100"), "nested begin reason and cleanup"),
                    (5, b"\x11", "abort restores persistent applet field"),
                    (7, b"\x11", "aborted value survives next APDU"),
                    (6, b"\x33", "commit preserves persistent applet field"),
                    (7, b"\x33", "committed value survives next APDU"),
                    (
                        8,
                        bytes.fromhex(
                            "BA7816BF8F01CFEA414140DE5DAE2223"
                            "B00361A396177A9CB410FF61F20015AD"
                        ),
                        "SHA-256 known-answer for abc",
                    ),
                    (
                        9,
                        bytes.fromhex(
                            "BA7816BF8F01CFEA414140DE5DAE2223"
                            "B00361A396177A9CB410FF61F20015AD"
                        ),
                        "SHA-256 update then doFinal state retention",
                    ),
                )
                for p1, expected, label in cases:
                    data = exchange(
                        connection,
                        bytes((0x80, 0x00, p1, 0x00, 0x00)),
                        label,
                    )
                    if data != expected:
                        raise RuntimeError(
                            f"{label}: expected {expected.hex().upper()}, got {data.hex().upper()}"
                        )
                print(f"TEST:SUMMARY pass={len(cases) + 2} fail=0 elapsed_s={time.monotonic() - started:.3f}")
            finally:
                if connection is not None:
                    connection.close()
                if process.poll() is None:
                    process.send_signal(signal.SIGTERM)
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=2)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"TEST:FAIL {error}")
        raise SystemExit(1)
