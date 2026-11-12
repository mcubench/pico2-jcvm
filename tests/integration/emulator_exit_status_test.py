#!/usr/bin/env python3
"""Check that host-emulator startup and transport failures reach process exit."""

from __future__ import annotations

import hashlib
import socket
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cap_load_code import ensure_algtest_load_code


ROOT = Path(__file__).resolve().parents[2]
EMULATOR = ROOT / "build/choupi-pc/choupi"
ROM_MASK = ROOT / "build/choupi-pc/flash"
APPLET_AID = "4A43416C675465737431"
ABSENT_AID = "A000000151000001"


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.bind(("127.0.0.1", 0))
        return server.getsockname()[1]


def run_process(name: str, command: list[str], log_dir: Path,
                expected_status: int, marker: str) -> None:
    try:
        result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=90, check=False)
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or b"").decode("utf-8", errors="replace")
        (log_dir / f"{name}.log").write_text(output)
        raise RuntimeError(f"{name}: emulator timed out") from error
    output = result.stdout.decode("utf-8", errors="replace")
    (log_dir / f"{name}.log").write_text(output)
    if result.returncode != expected_status:
        raise RuntimeError(
            f"{name}: expected exit {expected_status}, got {result.returncode};\n"
            f"{output[-5000:]}"
        )
    if marker not in output:
        raise RuntimeError(f"{name}: missing diagnostic {marker!r};\n{output[-5000:]}")
    print(f"TEST:PASS {name} exit={result.returncode}")


def run_socket_case(name: str, cap_code: Path, memory: Path, log_dir: Path,
                    behavior: str, expected_status: int, marker: str) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(15)
        endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
        command = [
            str(EMULATOR), "--memory", str(memory), "--rom-mask", str(ROM_MASK),
            "--vpcd", endpoint, "--applet-aid", APPLET_AID,
            "--package-id", "63", "--cap-code", str(cap_code),
        ]
        if behavior == "idle-close":
            command.extend(("--vpcd-idle-timeout", "1"))
        log_path = log_dir / f"{name}.log"
        with log_path.open("wb") as log_file:
            process = subprocess.Popen(command, cwd=ROOT, stdout=log_file,
                                       stderr=subprocess.STDOUT)
            try:
                connection, _ = listener.accept()
                with connection:
                    if behavior == "malformed-frame":
                        connection.sendall(b"\x00\x00")
                    elif behavior == "idle-close":
                        pass
                    else:
                        raise ValueError(f"unknown socket behavior {behavior}")
                process.wait(timeout=90)
            except Exception:
                process.kill()
                process.wait()
                raise
    output = log_path.read_text(errors="replace")
    if process.returncode != expected_status:
        raise RuntimeError(
            f"{name}: expected exit {expected_status}, got {process.returncode};\n"
            f"{output[-5000:]}"
        )
    if marker not in output:
        raise RuntimeError(f"{name}: missing diagnostic {marker!r};\n{output[-5000:]}")
    print(f"TEST:PASS {name} exit={process.returncode}")


def main() -> int:
    if not EMULATOR.is_file() or not ROM_MASK.is_file():
        raise RuntimeError("missing PC emulator or ROM mask; run tools/build-pc first")
    cap_code = ensure_algtest_load_code(ROOT)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    log_dir = ROOT / "artifacts/results" / f"emulator-exit-status-{stamp}"
    log_dir.mkdir(parents=True, exist_ok=False)
    print(f"EMU_EXIT_STATUS:ARTIFACTS path={log_dir.relative_to(ROOT)}")

    empty_memory = log_dir / "empty.bin"
    empty_memory.write_bytes(b"")
    connection_memory = log_dir / "connection.bin"
    connection_memory.write_bytes(b"")

    run_process(
        "absent-restored-instance",
        [str(EMULATOR), "--memory", str(empty_memory), "--rom-mask", str(ROM_MASK),
         "--vpcd", f"127.0.0.1:{free_port()}", "--applet-aid", ABSENT_AID,
         "--resume"],
        log_dir, 1, "PC storage initialization failed",
    )

    run_process(
        "connection-refused",
        [str(EMULATOR), "--memory", str(connection_memory), "--rom-mask", str(ROM_MASK),
         "--vpcd", f"127.0.0.1:{free_port()}", "--applet-aid", APPLET_AID,
         "--package-id", "63", "--cap-code", str(cap_code)],
        log_dir, 1, "Connection refused",
    )

    malformed_memory = log_dir / "malformed.bin"
    malformed_memory.write_bytes(b"")
    run_socket_case("malformed-transport-frame", cap_code, malformed_memory,
                    log_dir, "malformed-frame", 1, "invalid vpcd frame")

    install_memory = log_dir / "install-failure.bin"
    install_memory.write_bytes(b"")
    run_process(
        "failed-install",
        [str(EMULATOR), "--memory", str(install_memory), "--rom-mask", str(ROM_MASK),
         "--applet-aid", ABSENT_AID, "--package-id", "63", "--cap-code",
         str(cap_code), "--install", "--save"],
        log_dir, 1, "PC applet INSTALL failed",
    )

    image_save = log_dir / "image-save-failure.bin"
    image_save.write_bytes(b"")
    image_save.chmod(0o444)
    try:
        run_process(
            "image-save-failure",
            [str(EMULATOR), "--memory", str(image_save), "--rom-mask", str(ROM_MASK),
             "--save"],
            log_dir, 1, "unable to save memory image",
        )
    finally:
        image_save.chmod(0o644)

    sidecar_memory = log_dir / "sidecar-save-failure.bin"
    sidecar_memory.write_bytes(b"")
    (log_dir / "sidecar-save-failure.bin.cap-registry.tmp").mkdir()
    run_process(
        "sidecar-save-failure",
        [str(EMULATOR), "--memory", str(sidecar_memory), "--rom-mask", str(ROM_MASK),
         "--applet-aid", APPLET_AID, "--package-id", "63", "--cap-code",
         str(cap_code), "--install", "--save"],
        log_dir, 1, "unable to save VPCD CAP registry sidecar",
    )

    clean_memory = log_dir / "clean-shutdown.bin"
    clean_memory.write_bytes(b"")
    run_socket_case("clean-transport-shutdown", cap_code, clean_memory,
                    log_dir, "idle-close", 0, "VPCD:DISCONNECTED result=0")

    digest = hashlib.sha256(EMULATOR.read_bytes()).hexdigest()
    print(f"EMU_EXIT_STATUS:PASS binary_sha256={digest} cases=7")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"EMU_EXIT_STATUS:FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
