#!/usr/bin/env python3
"""Install JCAlgTest and exercise it through the PC VPCD bridge."""

from __future__ import annotations

import argparse
import os
import re
import signal
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cap_load_code import ensure_algtest_v182_load_code


DEFAULT_APPLET_AID = bytes.fromhex("4A43416C675465737431")


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


def exchange(
    connection: socket.socket,
    command: bytes,
    label: str,
    expected_status: bytes = b"\x90\x00",
) -> bytes:
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
    if len(expected_status) != 2:
        raise ValueError("expected_status must contain exactly SW1/SW2")
    if len(response) < 2 or response[-2:] != expected_status:
        raise RuntimeError(
            f"{label} status mismatch: expected={expected_status.hex().upper()} "
            f"actual={response[-2:].hex().upper()}"
        )
    phase_name = label.replace(" ", "_")
    print(f"VPCD:PHASE name={phase_name} status=ok elapsed_s={elapsed:.3f}")
    print(f"TEST:PASS {label} response={response.hex().upper()}")
    return response


def accept_emulator_connection(
    listener: socket.socket,
    process: subprocess.Popen[bytes],
    timeout: float,
    log_path: Path,
    log_start: int,
    phase: str,
) -> tuple[socket.socket, tuple[str, int]]:
    """Accept VPCD while detecting emulator exit or an INSTALL failure early."""
    deadline = time.monotonic() + timeout
    failure_markers = (
        "ERROR: PC applet INSTALL failed",
        "VPCD:INSTALL_FAIL",
    )
    while True:
        return_code = process.poll()
        if return_code is not None:
            reason = f"emulator exited with status {return_code}"
        else:
            reason = ""
        try:
            with log_path.open("rb") as captured_log:
                captured_log.seek(log_start)
                log_text = captured_log.read().decode("utf-8", errors="replace")
        except FileNotFoundError:
            log_text = ""
        if not reason:
            marker = next((item for item in failure_markers if item in log_text), None)
            if marker is not None:
                reason = f"emulator reported {marker} before VPCD connect"
        if reason:
            raise RuntimeError(
                f"{phase}: {reason}; log tail:\n{log_text[-4000:]}"
            )

        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise socket.timeout(
                f"timed out waiting {timeout:.1f}s for {phase} VPCD connect"
            )
        listener.settimeout(min(remaining, 0.2))
        try:
            return listener.accept()
        except socket.timeout:
            continue


def run_emulator(
    args: argparse.Namespace, memory: Path, pass_number: int,
    pre_restart: bool = False,
    resume: bool = False,
) -> None:
    pass_started = time.monotonic()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        host, port = listener.getsockname()

        command = [
            str(args.emulator),
            "-m",
            str(memory),
            "-s",
            "--vpcd",
            f"{host}:{port}",
            "--applet-aid",
            args.applet_aid.hex(),
            "--rom-mask",
            str(args.rom_mask),
        ]
        if resume:
            # Resume from the emulator's persisted CAP registry and flash image.
            # Explicit --class-index reconstructs an instance outside the
            # journal path and would mask a failed install-recovery replay.
            command.append("--resume")
        else:
            command.extend(("--package-id", str(args.package_id),
                            "--cap-code", str(args.cap_code),
                            "--install", "-s"))
        if args.install_params is not None and not resume:
            command.extend(("--install-params", args.install_params))
            print(
                "VPCD:INSTALL_PARAMETERS "
                f"length={len(bytes.fromhex(args.install_params))} "
                f"hex={args.install_params}"
            )
        print(f"VPCD:START pass={pass_number}")
        log_start = args.log.stat().st_size if args.log.exists() else 0
        child_env = os.environ.copy()
        if args.inject_install_recovery_gap and (
            not args.verify_install_process_restart or pre_restart
        ):
            if not args.verify_install_after_publication_restart:
                child_env["CHOUPI_TEST_SKIP_CAP_INSTANCE_STORE_ONCE"] = "1"
            child_env["CHOUPI_TEST_LEAVE_INSTALL_JOURNAL_ONCE"] = "1"
        if pre_restart:
            child_env["CHOUPI_TEST_SKIP_INSTALL_RECOVERY_ONCE"] = "1"
        if args.install_params is not None and not resume:
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
                connection, _ = accept_emulator_connection(
                    listener, process, args.connect_timeout, args.log,
                    log_start, f"pass {pass_number}",
                )
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

                if pre_restart:
                    select = bytes((0x00, 0xA4, 0x04, args.select_p2,
                                    len(args.applet_aid)))
                    select += args.applet_aid + b"\x00"
                    send_message(connection, select)
                    skipped_select = receive_message(connection)
                    if skipped_select[-2:] != b"\x6a\x82":
                        raise RuntimeError(
                            "applet should remain unregistered until install "
                            "journal recovery; expected SELECT 6A82, got "
                            f"{skipped_select.hex().upper()}"
                        )
                    print(
                        "TEST:PASS pre-recovery SELECT reports applet not "
                        "registered (6A82)"
                    )
                    return

                select = bytes((0x00, 0xA4, 0x04, args.select_p2,
                                len(args.applet_aid)))
                select += args.applet_aid + b"\x00"
                select_response = exchange(
                    connection, select, "target applet SELECT",
                    expected_status=args.expect_select_status,
                )
                if args.expect_select_data is not None:
                    actual = select_response[:-2]
                    if actual != args.expect_select_data:
                        raise RuntimeError(
                            "SELECT response data mismatch: "
                            f"expected={args.expect_select_data.hex().upper()} "
                            f"actual={actual.hex().upper()}"
                        )

                if args.select_only:
                    return

                custom_apdus = args.resume_apdu if resume else args.apdu
                expected_custom_responses = (
                    args.expect_resume_apdu_response if resume
                    else args.expect_apdu_response
                )
                if args.apdu or args.resume_apdu:
                    for index, apdu in enumerate(custom_apdus, start=1):
                        expected_response = (
                            expected_custom_responses[index - 1]
                            if expected_custom_responses is not None else None
                        )
                        response = exchange(connection, apdu,
                                            f"custom APDU {index}",
                                            expected_status=(
                                                expected_response[-2:]
                                                if expected_response is not None and
                                                len(expected_response) >= 2
                                                else b"\x90\x00"
                                            ))
                        if expected_response is not None and \
                                response != expected_response:
                            raise RuntimeError(
                                f"custom APDU {index} response mismatch: "
                                f"expected={expected_response.hex().upper()} "
                                f"actual={response.hex().upper()}"
                            )
                        if not resume and index in args.reset_after_custom_apdu:
                            send_message(connection, b"\x02")  # reader reset
                            send_message(connection, b"\x04")  # ATR request
                            atr = receive_message(connection)
                            if atr != bytes.fromhex("3B800181"):
                                raise RuntimeError(
                                    "unexpected ATR after VPCD reset: "
                                    f"{atr.hex().upper()}"
                                )
                            print("TEST:PASS VPCD reader reset ATR=3B800181")
                            select_response = exchange(
                                connection, select,
                                f"target applet SELECT after reset {index}")
                            if select_response[-2:] != b"\x90\x00":
                                raise RuntimeError(
                                    "applet SELECT after reset failed: "
                                    f"{select_response.hex().upper()}"
                                )
                    return

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
                        "JCAlgTest secure-random support APDU",
                        0x16,
                        "B07516000302000000",
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
                    if class_id == 0x16 and result[1] != 0x00:
                        raise RuntimeError(
                            "secure-random was not reported supported: "
                            f"{result.hex().upper()}"
                        )

                random_oneshot = exchange(
                    connection,
                    bytes.fromhex("B0752B00010200"),
                    "JCAlgTest unsupported RandomData.OneShot factory",
                )
                if random_oneshot[0] != 0x2B or random_oneshot[1] != 0x03:
                    raise RuntimeError(
                        "unsupported RandomData.OneShot factory must report "
                        "NO_SUCH_ALGORITHM (3): "
                        f"{random_oneshot.hex().upper()}"
                    )
                print("TEST:PASS RandomData.OneShot reports NO_SUCH_ALGORITHM")
            finally:
                if connection is not None:
                    connection.close()
                stop_started = time.monotonic()
                try:
                    # Closing the VPCD socket lets the emulator finish its
                    # transport loop and save flash before we send a signal.
                    return_code = process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    return_code = stop_emulator(process)
                print(
                    "VPCD:PHASE name=emulator_stop status=done "
                    f"elapsed_s={time.monotonic() - stop_started:.3f}"
                )
            if return_code not in (0, -signal.SIGINT, 128 + signal.SIGINT):
                raise RuntimeError(
                    f"PC emulator exited with status {return_code}; inspect {args.log}"
                )
            if args.install_params is not None and not resume:
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
    default_memory = repo / "build/algtest-v182-vpcd-empty-input.bin"
    parser.add_argument(
        "--memory", type=Path, default=None,
        help=("input flash image; by default the test creates a fresh blank "
              "image at " + str(default_memory)),
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
        "--inject-install-recovery-gap",
        action="store_true",
        help=(
            "host-only fault injection: skip the first CAP instance write and "
            "leave its committed install journal for VPCD power-on recovery"
        ),
    )
    parser.add_argument(
        "--verify-install-process-restart",
        action="store_true",
        help=(
            "install with registry publication skipped, exit the emulator, "
            "then verify journal recovery in a fresh VPCD process"
        ),
    )
    parser.add_argument(
        "--verify-install-after-publication-restart",
        action="store_true",
        help=(
            "leave the journal after CAP registry publication, then verify "
            "idempotent recovery in a fresh process"
        ),
    )
    parser.add_argument(
        "--verify-install-metadata-error-restart",
        action="store_true",
        help=(
            "make CAP instance publication return an error after filesystem "
            "commit, then verify journal recovery in a fresh process"
        ),
    )
    parser.add_argument(
        "--verify-install-journal-clear-restart",
        action="store_true",
        help=(
            "fail the journal-clear transaction sync after CAP instance "
            "publication, then verify idempotent recovery in a fresh process"
        ),
    )
    parser.add_argument(
        "--install-params",
        help="hex-encoded applet installation parameters passed to the VM",
    )
    parser.add_argument(
        "--applet-aid",
        default=DEFAULT_APPLET_AID.hex(),
        help="hex-encoded applet AID to install and select",
    )
    parser.add_argument(
        "--package-id",
        type=int,
        default=63,
        choices=range(56, 64),
        help="dynamic package ID used by the host emulator (default: 63)",
    )
    parser.add_argument(
        "--select-only",
        action="store_true",
        help="stop after a successful SELECT instead of sending the JCAlgTest GETVERSION APDU",
    )
    parser.add_argument(
        "--reset-after-custom-apdu",
        type=int,
        action="append",
        default=[],
        help="send VPCD reader reset after this one-based custom APDU index",
    )
    parser.add_argument(
        "--select-p2",
        type=lambda value: int(value, 0),
        choices=(0x00, 0x10),
        default=0x00,
        help="SELECT P2: 0x10 requests the Java Card RMI interface reference form",
    )
    parser.add_argument(
        "--expect-select-data",
        help="expected SELECT response data (hex, excluding SW1/SW2)",
    )
    parser.add_argument(
        "--expect-select-status",
        default="9000",
        help="expected SW1/SW2 for the initial applet SELECT (default: 9000)",
    )
    parser.add_argument(
        "--apdu",
        action="append",
        default=[],
        help="send a raw APDU after SELECT; repeat to send multiple APDUs",
    )
    parser.add_argument(
        "--expect-apdu-response",
        action="append",
        help="expected response for each --apdu, including SW1/SW2; repeat in matching order",
    )
    parser.add_argument(
        "--resume-apdu",
        action="append",
        default=[],
        help="send APDUs after restarting the emulator and restoring the installed applet",
    )
    parser.add_argument(
        "--expect-resume-apdu-response",
        action="append",
        help="expected response for each --resume-apdu, including SW1/SW2",
    )
    parser.add_argument(
        "--flash-copy",
        type=Path,
        default=None,
        help="writable emulator image (default: derived from --memory; JCAlgTest uses its historical filename)",
    )
    parser.add_argument(
        "--log",
        type=Path,
        default=repo / "artifacts/logs/host-vpcd-jcalgtest-20261014.log",
    )
    args = parser.parse_args()
    args.uses_generated_empty_memory = args.memory is None
    if args.memory is None:
        args.memory = default_memory
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
    try:
        args.applet_aid = bytes.fromhex(args.applet_aid)
        args.apdu = [bytes.fromhex(value) for value in args.apdu]
        args.resume_apdu = [bytes.fromhex(value) for value in args.resume_apdu]
        args.expect_apdu_response = (
            [bytes.fromhex(value) for value in args.expect_apdu_response]
            if args.expect_apdu_response is not None else None
        )
        args.expect_resume_apdu_response = (
            [bytes.fromhex(value) for value in args.expect_resume_apdu_response]
            if args.expect_resume_apdu_response is not None else None
        )
        args.expect_select_data = (
            bytes.fromhex(args.expect_select_data)
            if args.expect_select_data is not None else None
        )
        args.expect_select_status = bytes.fromhex(args.expect_select_status)
    except ValueError as error:
        parser.error(f"AID, APDUs, and expected SELECT data must be valid hexadecimal: {error}")
    if not 5 <= len(args.applet_aid) <= 16:
        parser.error("--applet-aid must contain 5 to 16 bytes")
    if len(args.expect_select_status) != 2:
        parser.error("--expect-select-status must contain exactly four hex digits")
    if any(len(apdu) < 4 for apdu in args.apdu + args.resume_apdu):
        parser.error("each --apdu must contain at least four bytes")
    if args.expect_apdu_response is not None and \
            len(args.expect_apdu_response) != len(args.apdu):
        parser.error("--expect-apdu-response must be repeated once for each --apdu")
    if args.expect_resume_apdu_response is not None and \
            len(args.expect_resume_apdu_response) != len(args.resume_apdu):
        parser.error("--expect-resume-apdu-response must be repeated once for each --resume-apdu")
    args.repo = args.repo.resolve()
    default_cap_code = repo / "build/algtest-v182-load-code.bin"
    if args.cap_code == default_cap_code:
        ensure_algtest_v182_load_code(args.repo)
    for name in ("emulator", "memory", "cap_code", "rom_mask"):
        setattr(args, name, getattr(args, name).resolve())
        if name == "memory" and args.uses_generated_empty_memory:
            args.memory.parent.mkdir(parents=True, exist_ok=True)
            args.memory.write_bytes(b"")
        if not getattr(args, name).is_file():
            parser.error(f"--{name.replace('_', '-')} not found: {getattr(args, name)}")
    if args.flash_copy is None:
        if args.memory == default_memory.resolve():
            args.flash_copy = repo / "build/algtest-v182-vpcd-flash.bin"
        else:
            args.flash_copy = args.memory.with_name(args.memory.stem + "-vpcd-copy.bin")
    args.flash_copy = args.flash_copy.resolve()
    if args.flash_copy == args.memory:
        parser.error("--flash-copy must differ from --memory; input images are preserved")
    args.log = args.log.resolve()
    args.flash_copy.parent.mkdir(parents=True, exist_ok=True)
    args.log.parent.mkdir(parents=True, exist_ok=True)
    return args


def main() -> int:
    args = parse_args()
    if args.verify_install_process_restart:
        args.inject_install_recovery_gap = True
        if args.passes != 1:
            print("TEST:FAIL process-restart recovery requires --passes 1",
                  file=sys.stderr)
            return 1
    if args.verify_install_after_publication_restart:
        args.verify_install_process_restart = True
        args.inject_install_recovery_gap = True
        if args.passes != 1:
            print("TEST:FAIL process-restart recovery requires --passes 1",
                  file=sys.stderr)
            return 1
    if args.verify_install_metadata_error_restart:
        args.verify_install_process_restart = True
        args.inject_install_recovery_gap = True
        if args.passes != 1:
            print("TEST:FAIL process-restart recovery requires --passes 1",
                  file=sys.stderr)
            return 1
    if args.verify_install_journal_clear_restart:
        args.verify_install_process_restart = True
        if args.passes != 1:
            print("TEST:FAIL process-restart recovery requires --passes 1",
                  file=sys.stderr)
            return 1
    recovery_log_start = args.log.stat().st_size if args.log.exists() else 0
    shutil.copyfile(args.memory, args.flash_copy)
    # A prior emulator run may have left a CAP-registry sidecar next to this
    # scratch copy. It belongs to the old flash bytes, not the freshly copied
    # image, and must not be used to resume the new test state.
    Path(f"{args.flash_copy}.cap-registry").unlink(missing_ok=True)
    try:
        if args.verify_install_journal_clear_restart:
            install_command = [
                str(args.emulator), "-m", str(args.flash_copy), "-s",
                "--applet-aid", args.applet_aid.hex(),
                "--package-id", str(args.package_id),
                "--cap-code", str(args.cap_code),
                "--rom-mask", str(args.rom_mask), "--install",
            ]
            failure_env = os.environ.copy()
            failure_env[
                "CHOUPI_TEST_FAIL_INSTALL_JOURNAL_CLEAR_SYNC_ONCE"
            ] = "1"
            with args.log.open("ab") as log:
                log.write(b"\n--- install with journal clear sync failure ---\n")
                result = subprocess.run(
                    install_command, cwd=args.repo, stdout=log, stderr=log,
                    env=failure_env, timeout=args.apdu_timeout * 4,
                    check=False,
                )
            with args.log.open("rb") as log:
                log.seek(recovery_log_start)
                install_log = log.read().decode("utf-8", errors="replace")
            marker = "VPCD:TEST_FAILED_INSTALL_JOURNAL_CLEAR_SYNC"
            if marker not in install_log:
                raise RuntimeError(
                    "journal clear sync failure was not injected; "
                    f"exit={result.returncode}, inspect {args.log}"
                )
            if "ERROR: PC applet INSTALL failed" not in install_log:
                raise RuntimeError("journal clear sync failure did not fail INSTALL")
            package_match = re.findall(
                r"VPCD:TEST_ARMED_INSTALL_JOURNAL_CLEAR_SYNC package=(\d+)",
                install_log,
            )
            if not package_match:
                raise RuntimeError("journal clear sync injection omitted package ID")
            args.package_id = int(package_match[-1])
            args.install_class_index = 0
            print(
                "TEST:PASS journal cleanup sync failed after registry "
                "publication; recovery is pending in the saved image"
            )
        elif args.verify_install_metadata_error_restart:
            install_command = [
                str(args.emulator), "-m", str(args.flash_copy), "-s",
                "--applet-aid", args.applet_aid.hex(),
                "--package-id", str(args.package_id),
                "--cap-code", str(args.cap_code),
                "--rom-mask", str(args.rom_mask), "--install",
            ]
            failure_env = os.environ.copy()
            failure_env["CHOUPI_TEST_FAIL_CAP_INSTANCE_STORE_ONCE"] = "1"
            with args.log.open("ab") as log:
                log.write(b"\n--- install with CAP metadata write error ---\n")
                result = subprocess.run(
                    install_command, cwd=args.repo, stdout=log, stderr=log,
                    env=failure_env, timeout=args.apdu_timeout * 4,
                    check=False,
                )
            with args.log.open("rb") as log:
                log.seek(recovery_log_start)
                install_log = log.read().decode("utf-8", errors="replace")
            package_match = re.findall(
                r"VPCD:TEST_FAILED_CAP_INSTANCE_STORE package=(\d+)",
                install_log,
            )
            if not package_match:
                raise RuntimeError(
                    "injected CAP metadata write error was not observed; "
                    f"exit={result.returncode}, inspect {args.log}"
                )
            if "ERROR: PC applet INSTALL failed" not in install_log:
                raise RuntimeError("CAP metadata failure did not fail INSTALL")
            args.package_id = int(package_match[-1])
            args.install_class_index = 0
            print(
                "TEST:PASS CAP metadata publication returned an error after "
                f"filesystem commit package={args.package_id}; "
                f"emulator_exit={result.returncode}"
            )
        elif args.verify_install_process_restart:
            run_emulator(args, args.flash_copy, 0, pre_restart=True)
            with args.log.open("rb") as log:
                log.seek(recovery_log_start)
                install_log = log.read().decode("utf-8", errors="replace")
            required_install_markers = [
                "VPCD:TEST_LEFT_INSTALL_JOURNAL package=",
                "VPCD:TEST_SKIPPED_INSTALL_RECOVERY_ONCE",
            ]
            if not args.verify_install_after_publication_restart:
                required_install_markers.append(
                    "VPCD:TEST_SKIPPED_CAP_INSTANCE_STORE package="
                )
            missing = [marker for marker in required_install_markers
                       if marker not in install_log]
            if missing:
                raise RuntimeError(
                    f"pre-restart install markers missing: {missing}; inspect {args.log}"
                )
            class_match = re.findall(
                r"VPCD:APPLET_INSTALLED(?: package-id=\d+)? class-index=(\d+)",
                install_log,
            )
            package_match = re.findall(
                r"VPCD:TEST_LEFT_INSTALL_JOURNAL package=(\d+)", install_log
            )
            if not class_match:
                raise RuntimeError("pre-restart install did not report applet class")
            if not package_match:
                raise RuntimeError("pre-restart install did not report package ID")
            args.install_class_index = int(class_match[-1])
            args.package_id = int(package_match[-1])
            print(
                "TEST:PASS install process exited with pending journal "
                f"package={args.package_id} class-index={args.install_class_index}"
            )
        resume_installed = args.verify_install_process_restart
        for pass_number in range(1, args.passes + 1):
            pass_log_start = args.log.stat().st_size if args.log.exists() else 0
            run_emulator(args, args.flash_copy, pass_number,
                         resume=resume_installed)
            if not resume_installed:
                with args.log.open("rb") as log:
                    log.seek(pass_log_start)
                    pass_log = log.read().decode("utf-8", errors="replace")
                package_match = re.findall(
                    r"VPCD:APPLET_INSTALLED package-id=(\d+) class-index=(\d+)",
                    pass_log,
                )
                if not package_match:
                    raise RuntimeError(
                        "initial install did not report its resolved package ID "
                        "and applet class index"
                    )
                args.package_id, args.install_class_index = map(
                    int, package_match[-1]
                )
                resume_installed = True
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"TEST:FAIL {error}", file=sys.stderr)
        return 1
    if args.inject_install_recovery_gap:
        with args.log.open("rb") as log:
            log.seek(recovery_log_start)
            log_text = log.read().decode("utf-8", errors="replace")
        expected = ["VPCD:INSTALL_JOURNAL_RECOVERED package="]
        if args.verify_install_metadata_error_restart:
            expected.append("VPCD:TEST_FAILED_CAP_INSTANCE_STORE package=")
        else:
            expected.append("VPCD:TEST_LEFT_INSTALL_JOURNAL package=")
        if not (args.verify_install_after_publication_restart or
                args.verify_install_metadata_error_restart):
            expected.append("VPCD:TEST_SKIPPED_CAP_INSTANCE_STORE package=")
        missing = [marker for marker in expected if marker not in log_text]
        if missing:
            print(f"TEST:FAIL install recovery markers missing: {missing}",
                  file=sys.stderr)
            return 1
        if args.verify_install_metadata_error_restart:
            print("TEST:PASS install journal recovered after metadata write error")
        elif args.verify_install_after_publication_restart:
            print("TEST:PASS install journal recovered after registry publication")
        elif args.verify_install_journal_clear_restart:
            print("TEST:PASS install journal recovered after clear sync failure")
        else:
            print("TEST:PASS install journal recovered after skipped registry write")
    if args.verify_install_journal_clear_restart:
        with args.log.open("rb") as log:
            log.seek(recovery_log_start)
            log_text = log.read().decode("utf-8", errors="replace")
        required = [
            "VPCD:TEST_FAILED_INSTALL_JOURNAL_CLEAR_SYNC",
            "VPCD:INSTALL_JOURNAL_RECOVERED package=",
        ]
        missing = [marker for marker in required if marker not in log_text]
        if missing:
            print(f"TEST:FAIL journal-clear restart markers missing: {missing}",
                  file=sys.stderr)
            return 1
        print("TEST:PASS install journal recovered after clear sync failure")
    if args.select_only:
        total_checks = args.passes * 2  # ATR and target-app SELECT.
    elif args.apdu or args.resume_apdu:
        total_checks = args.passes * 2 + len(args.apdu) + len(args.resume_apdu)
    else:
        total_checks = args.passes * 15  # ATR, SELECT, APDUs and support assertions.
    pre_restart_checks = (
        2 if args.verify_install_process_restart and
        not args.verify_install_metadata_error_restart else 0
    )
    print(
        "TEST:SUMMARY "
        f"pass={total_checks + pre_restart_checks} fail=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
