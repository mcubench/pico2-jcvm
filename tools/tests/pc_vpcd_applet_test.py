#!/usr/bin/env python3
"""Install JCAlgTest and exercise it through the PC VPCD bridge."""

from __future__ import annotations

import argparse
import os
import signal
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path


APPLET_AID = bytes.fromhex("4A43416C675465737431")


def receive_exact(connection: socket.socket, length: int) -> bytes:
    data = bytearray()
    while len(data) < length:
        chunk = connection.recv(length - len(data))
        if not chunk:
            raise RuntimeError("VPCD connection closed during a frame")
        data.extend(chunk)
    return bytes(data)


def receive_message(connection: socket.socket) -> bytes:
    length = int.from_bytes(receive_exact(connection, 2), "big")
    if length == 0:
        raise RuntimeError("VPCD peer returned an empty frame")
    return receive_exact(connection, length)


def send_message(connection: socket.socket, message: bytes) -> None:
    if not message or len(message) > 0xFFFF:
        raise ValueError("VPCD messages must contain 1..65535 bytes")
    connection.sendall(len(message).to_bytes(2, "big") + message)


def exchange(connection: socket.socket, command: bytes, label: str) -> bytes:
    started = time.monotonic()
    try:
        send_message(connection, command)
        response = receive_message(connection)
    except Exception:
        elapsed = time.monotonic() - started
        phase_name = label.replace(" ", "_")
        print(f"VPCD:PHASE name={phase_name} status=fail elapsed_s={elapsed:.3f}")
        raise
    elapsed = time.monotonic() - started
    if len(response) < 2 or response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label} failed: {response.hex().upper()}")
    phase_name = label.replace(" ", "_")
    print(f"VPCD:PHASE name={phase_name} status=ok elapsed_s={elapsed:.3f}")
    print(f"TEST:PASS {label} response={response.hex().upper()}")
    return response


def run_emulator(args: argparse.Namespace, memory: Path, pass_number: int) -> None:
    pass_started = time.monotonic()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(args.connect_timeout)
        host, port = listener.getsockname()

        command = [
            str(args.emulator),
            "-m",
            str(memory),
            "-s",
            "--vpcd",
            f"{host}:{port}",
            "--applet-aid",
            APPLET_AID.hex(),
            "--package-id",
            "63",
            "--cap-code",
            str(args.cap_code),
            "--rom-mask",
            str(args.rom_mask),
            "--install",
            "-s",
        ]
        if args.install_params is not None:
            command.extend(("--install-params", args.install_params))
            print(
                "VPCD:INSTALL_PARAMETERS "
                f"length={len(bytes.fromhex(args.install_params))} "
                f"hex={args.install_params}"
            )
        print(f"VPCD:START pass={pass_number}")
        log_start = args.log.stat().st_size if args.log.exists() else 0
        child_env = os.environ.copy()
        if args.install_params is not None:
            child_env["CHOUPI_EXPECT_INSTALL_PARAMS"] = args.install_params
        with args.log.open("ab") as log:
            log.write(f"\n--- VPCD pass {pass_number} ---\n".encode())
            process = subprocess.Popen(
                command,
                cwd=args.repo,
                stdout=log,
                stderr=log,
                start_new_session=True,
                env=child_env,
            )
            connection = None
            try:
                connect_started = time.monotonic()
                connection, _ = listener.accept()
                print(
                    "VPCD:PHASE name=connect status=ok "
                    f"elapsed_s={time.monotonic() - connect_started:.3f}"
                )
                connection.settimeout(args.apdu_timeout)

                atr_started = time.monotonic()
                send_message(connection, b"\x01")  # power on
                send_message(connection, b"\x04")  # ATR request
                atr = receive_message(connection)
                print(
                    "VPCD:PHASE name=ATR status=ok "
                    f"elapsed_s={time.monotonic() - atr_started:.3f}"
                )
                if atr != bytes.fromhex("3B800181"):
                    raise RuntimeError(f"unexpected ATR: {atr.hex().upper()}")
                print(f"TEST:PASS atr response={atr.hex().upper()}")

                select = bytes((0x00, 0xA4, 0x04, 0x00, len(APPLET_AID)))
                select += APPLET_AID + b"\x00"
                exchange(connection, select, "JCAlgTest applet SELECT")

                version = exchange(
                    connection,
                    bytes.fromhex("B0E1000000"),
                    "JCAlgTest GETVERSION APDU",
                )
                if version[:-2] != b"1.8.2_jc305":
                    raise RuntimeError(
                        "GETVERSION response is not exactly 1.8.2_jc305: "
                        f"{version.hex().upper()}"
                    )

                exchange(
                    connection,
                    bytes.fromhex("B0730100010100"),
                    "JCAlgTest JCSystem info APDU",
                )
                sha256_digest = exchange(
                    connection,
                    bytes.fromhex("B07515000304000000"),
                    "JCAlgTest supported-algorithm APDU",
                )
                if sha256_digest[:2] != bytes.fromhex("1500"):
                    raise RuntimeError(
                        "MessageDigest ALG_SHA_256 was not reported supported: "
                        f"{sha256_digest.hex().upper()}"
                    )

                # Exercise the algorithm after the support probe. The prepared
                # settings select MessageDigest.doFinal with one input byte.
                digest_settings = bytes.fromhex(
                    "0015 0004 FFFF FFFF FFFF 0006 0001 FFFF FFFF 0001 0001"
                )
                exchange(
                    connection,
                    bytes((0xB0, 0x34, 0x00, 0x00, len(digest_settings)))
                    + digest_settings
                    + b"\x00",
                    "JCAlgTest SHA-256 prepare APDU",
                )
                sha256_dofinal = exchange(
                    connection,
                    bytes((0xB0, 0x41, 0x00, 0x00, len(digest_settings)))
                    + digest_settings
                    + b"\x00",
                    "JCAlgTest SHA-256 doFinal APDU",
                )
                if sha256_dofinal[0] != 0xAA:
                    raise RuntimeError(
                        "MessageDigest.doFinal did not report success: "
                        f"{sha256_dofinal.hex().upper()}"
                    )

                # Match the official ALG_PERFORMANCE_STATIC SHA-256 config:
                # 256 input bytes and 50 doFinal calls in one measured APDU.
                sha256_static_settings = bytes.fromhex(
                    "0015 0004 FFFF FFFF FFFF 0006 0100 FFFF FFFF 0032 0001"
                )
                exchange(
                    connection,
                    bytes((0xB0, 0x34, 0x00, 0x00,
                           len(sha256_static_settings)))
                    + sha256_static_settings
                    + b"\x00",
                    "JCAlgTest SHA-256 static performance prepare APDU",
                )
                sha256_static = exchange(
                    connection,
                    bytes((0xB0, 0x41, 0x00, 0x00,
                           len(sha256_static_settings)))
                    + sha256_static_settings
                    + b"\x00",
                    "JCAlgTest SHA-256 static performance APDU",
                )
                if sha256_static[0] != 0xAA:
                    raise RuntimeError(
                        "SHA-256 static doFinal loop did not report success: "
                        f"{sha256_static.hex().upper()}"
                    )

                aes_cipher = exchange(
                    connection,
                    bytes.fromhex("B0751100030D000000"),
                    "JCAlgTest AES-CBC-NOPAD support APDU",
                )
                if aes_cipher[:2] != b"\x11\x00":
                    raise RuntimeError(
                        "AES-CBC-NOPAD was not reported supported: "
                        f"{aes_cipher.hex().upper()}"
                    )

                for label, class_id, command in (
                    (
                        "JCAlgTest RSA-SHA1 signature support APDU",
                        0x12,
                        "B0751200030A000000",
                    ),
                    (
                        "JCAlgTest pseudo-random support APDU",
                        0x16,
                        "B07516000301000000",
                    ),
                    (
                        "JCAlgTest CRC16 checksum support APDU",
                        0x17,
                        "B07517000301000000",
                    ),
                ):
                    result = exchange(
                        connection, bytes.fromhex(command), label
                    )
                    if result[0] != class_id or result[1] == 0xF0:
                        raise RuntimeError(
                            f"{label} returned an incomplete result: "
                            f"{result.hex().upper()}"
                        )
            finally:
                if connection is not None:
                    connection.close()
                stop_started = time.monotonic()
                return_code = stop_emulator(process)
                print(
                    "VPCD:PHASE name=emulator_stop status=done "
                    f"elapsed_s={time.monotonic() - stop_started:.3f}"
                )
            if return_code not in (0, -signal.SIGINT, 128 + signal.SIGINT):
                raise RuntimeError(
                    f"PC emulator exited with status {return_code}; inspect {args.log}"
                )
            if args.install_params is not None:
                with args.log.open("rb") as captured_log:
                    captured_log.seek(log_start)
                    run_log = captured_log.read().decode(
                        "utf-8", errors="replace"
                    )
                expected_line = (
                    "VPCD:INSTALL_VM_ARGS status=PASS offset=0 "
                    f"length={len(bytes.fromhex(args.install_params))}"
                )
                if expected_line not in run_log:
                    raise RuntimeError(
                        "VM did not confirm the expected install argument array; "
                        f"inspect {args.log}"
                    )
                print(
                    "TEST:PASS vm_install_parameters "
                    f"offset=0 length={len(bytes.fromhex(args.install_params))}"
                )
        print(
            f"VPCD:PHASE name=pass_total status=done "
            f"elapsed_s={time.monotonic() - pass_started:.3f}"
        )
        print(f"TEST:PASS persisted applet emulator pass={pass_number}")


def stop_emulator(process: subprocess.Popen[bytes]) -> int:
    """Stop the long-running emulator and reap its whole process group."""
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGINT)
        except ProcessLookupError:
            return process.wait(timeout=2)
        try:
            return process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                return process.wait(timeout=2)
            try:
                return process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    return process.wait(timeout=2)
                return process.wait(timeout=2)
    return process.wait(timeout=2)


def parse_args() -> argparse.Namespace:
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=repo)
    parser.add_argument(
        "--emulator", type=Path, default=repo / "build/choupi-pc/choupi"
    )
    parser.add_argument(
        "--memory", type=Path, default=repo / "build/algtest-v182-empty-flash.bin"
    )
    parser.add_argument(
        "--cap-code", type=Path, default=repo / "build/algtest-v182-load-code.bin"
    )
    parser.add_argument(
        "--rom-mask", type=Path, default=repo / "build/choupi-pc/flash"
    )
    parser.add_argument(
        "--apdu-timeout",
        type=float,
        default=20.0,
        help="maximum seconds to wait for each emulator response (default: 20)",
    )
    parser.add_argument(
        "--connect-timeout",
        type=float,
        default=60.0,
        help="maximum seconds to wait for emulator VPCD connection (default: 60)",
    )
    parser.add_argument(
        "--passes",
        type=int,
        choices=(1, 2),
        default=2,
        help="number of clean emulator passes (default: 2)",
    )
    parser.add_argument(
        "--install-params",
        help="hex-encoded applet installation parameters passed to the VM",
    )
    parser.add_argument(
        "--flash-copy",
        type=Path,
        default=repo / "build/algtest-v182-vpcd-flash.bin",
    )
    parser.add_argument(
        "--log",
        type=Path,
        default=repo / "artifacts/logs/host-vpcd-jcalgtest-20261014.log",
    )
    args = parser.parse_args()
    if args.apdu_timeout <= 0:
        parser.error("--apdu-timeout must be positive")
    if args.install_params is not None:
        try:
            install_params = bytes.fromhex(args.install_params)
        except ValueError:
            parser.error(
                "--install-params must contain an even number of hex digits"
            )
        if len(install_params) > 0xff:
            parser.error("--install-params must be at most 255 bytes")
        args.install_params = install_params.hex().upper()
    args.repo = args.repo.resolve()
    for name in ("emulator", "memory", "cap_code", "rom_mask"):
        setattr(args, name, getattr(args, name).resolve())
        if not getattr(args, name).is_file():
            parser.error(f"--{name.replace('_', '-')} not found: {getattr(args, name)}")
    args.flash_copy = args.flash_copy.resolve()
    args.log = args.log.resolve()
    args.flash_copy.parent.mkdir(parents=True, exist_ok=True)
    args.log.parent.mkdir(parents=True, exist_ok=True)
    return args


def main() -> int:
    args = parse_args()
    shutil.copyfile(args.memory, args.flash_copy)
    try:
        for pass_number in range(1, args.passes + 1):
            run_emulator(args, args.flash_copy, pass_number)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"TEST:FAIL {error}", file=sys.stderr)
        return 1
    print(f"TEST:SUMMARY pass={args.passes * 11} fail=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
