#!/usr/bin/env python3
"""Time focused JCAlgTest APDUs on the PC VM.

Build first with ./tools/build-pc --profile max_crypto --rp2350-overlay.
Run with ./tools/env python3 tests/integration/jcalgtest_bytecode_benchmark.py
OUT_DIR. Each invocation starts from a new emulator store.
"""

import argparse
import os
from pathlib import Path
import socket
import statistics
import subprocess
import time

import jcalgtest_apdu_replay_vpcd_test as replay
import pc_vpcd_multipackage_test as vpcd
from fixture_paths import fixture_path


# Config field at offsets 18–19 is the repetition count. These are the
# JCAlgTest SWALGS and AES-128 ECB doFinal configs with a 16-byte payload.
CASES = {
    "xor": (0xC0, 0xA5, "0016FFFFFFFFFFFFFFFF00010010FFFFFFFF00320001"),
    "sw-aes": (0xC0, 0xA5, "0016FFFFFFFFFFFFFFFF00020010FFFFFFFF00320001"),
    "native-aes": (0x31, 0x43, "0011000EFFFF000F008000070010FFFF000200320001"),
}


def command(ins, config, repetitions):
    payload = bytes.fromhex(config)
    payload = payload[:18] + repetitions.to_bytes(2, "big") + payload[20:]
    return bytes((0xB0, ins, 0, 0, len(payload))) + payload + b"\x00"


def exchange(connection, apdu, label, expected=None):
    start = time.perf_counter_ns()
    response = vpcd.exchange(connection, apdu, label)
    elapsed_ms = (time.perf_counter_ns() - start) / 1e6
    if response[-2:] != b"\x90\x00" or (
        expected is not None and response[:-2] != expected
    ):
        raise RuntimeError(f"{label}: unexpected response {response.hex().upper()}")
    print(f"{label} apdu={apdu.hex().upper()} response={response.hex().upper()} "
          f"elapsed_ms={elapsed_ms:.3f}", flush=True)
    return elapsed_ms


def benchmark(connection, profile_capture):
    for name, (prepare, measure, config) in CASES.items():
        exchange(connection, command(prepare, config, 50), f"{name} prepare", b"\xaa")
        baselines = [
            exchange(connection, command(measure, config, 0), f"{name} baseline", b"\xaa")
            for _ in range(1 if profile_capture else 3)
        ]
        for iterations in ((50,) if profile_capture else (1, 5, 50)):
            samples = [
                exchange(connection, command(measure, config, iterations),
                         f"{name} repeats={iterations}", b"\xaa")
                for _ in range(1 if profile_capture else 3)
            ]
            if not profile_capture:
                net = statistics.median(samples) - statistics.median(baselines)
                print(f"MEDIAN {name} repeats={iterations} net_ms={net:.3f} "
                      f"net_ms_per_op={net / iterations:.3f}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out_dir", type=Path)
    emulator_options = parser.add_mutually_exclusive_group()
    emulator_options.add_argument("--emulator", type=Path,
                                  help="run all samples using an alternate build")
    emulator_options.add_argument("--profile-emulator", type=Path,
                                  help="use an instrumented binary for one 50-repeat APDU per case")
    parser.add_argument("--omit-descriptor", action="store_true",
                        help="use the same component set as standard GPPro LOAD")
    args = parser.parse_args()
    out = args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = replay.ROOT
    build = root / "build/choupi-pc-rp2350-overlay"
    executable = (args.emulator or args.profile_emulator or build / "choupi").resolve()
    cap_path = fixture_path("jcalgtest.cap")
    code_path = replay.ensure_algtest_load_code(root)
    code, package_aid, applets = vpcd.load_code(cap_path)
    if code != code_path.read_bytes() or replay.APPLET_AID not in applets:
        raise RuntimeError("CAP load image or applet identity mismatch")
    if args.omit_descriptor:
        code, _, _ = vpcd.load_code(cap_path, include_descriptor=False)
    code_path = out / "load-code.bin"
    code_path.write_bytes(code)
    for path in (executable, build / "flash", cap_path, code_path,
                 fixture_path("jcalgtest.client.1.8.5-20261121")):
        print(f"SHA256 {replay.sha256_file(path)} {path}", flush=True)

    memory = out / "store.bin"
    memory.touch()
    log_path = out / "emulator.log"
    with socket.socket() as listener, log_path.open("wb") as log:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        host, port = listener.getsockname()
        emulator = [
            str(executable), "-m", str(memory), "-s",
            "--vpcd", f"{host}:{port}", "--applet-aid", replay.APPLET_AID.hex(),
            "--rom-mask", str(build / "flash"), "--package-id", "63",
            "--cap-code", str(code_path),
        ]
        env = os.environ.copy()
        env.pop("CHOUPI_VM_PROGRESS_INTERVAL", None)
        env.pop("CHOUPI_VM_TRACE_APDU", None)
        process = subprocess.Popen(emulator, env=env, stdout=log, stderr=log)
        try:
            with replay.accept_emulator(listener, process, 30, log_path) as connection:
                connection.settimeout(90)
                connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                vpcd.send_message(connection, b"\x01")
                vpcd.send_message(connection, b"\x04")
                if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                    raise RuntimeError("unexpected VPCD ATR")
                replay.select_isd(connection)
                scp = vpcd.Scp03(connection)
                scp.authenticate()
                vpcd.load_package(scp, code, package_aid)
                vpcd.install_applet(scp, package_aid, replay.APPLET_AID, replay.APPLET_AID)
                exchange(connection, bytes.fromhex("00A404000A") + replay.APPLET_AID
                         + b"\x00", "SELECT")
                exchange(connection, bytes.fromhex("B0E1000000"),
                         "GETVERSION", b"1.8.5_jc305")
                benchmark(connection, args.profile_emulator is not None)
            status = process.wait(timeout=10)
            if status != 0:
                raise RuntimeError(f"emulator exited with status {status}")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    main()
