#!/usr/bin/env python3
"""Exercise successful cross-AID Shareable dispatch through the PC VPCD."""

from __future__ import annotations

import socket
import subprocess
import sys
import os
from datetime import datetime, timezone
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402


COMPONENTS = (
    "Header", "Directory", "Applet", "Import", "ConstantPool", "Class",
    "Method", "StaticField", "RefLocation", "Export", "Descriptor",
)
SERVICE_PACKAGE = bytes.fromhex("A000000151000003")
SERVICE_APPLET = bytes.fromhex("A00000015100000301")
CLIENT_PACKAGE = bytes.fromhex("A000000151000004")
CLIENT_APPLET = bytes.fromhex("A00000015100000401")
CONNECT_TIMEOUT = float(os.environ.get("SIO_CROSS_AID_CONNECT_TIMEOUT_S", "45"))
APDU_TIMEOUT = float(os.environ.get("SIO_CROSS_AID_APDU_TIMEOUT_S", "30"))
SHUTDOWN_TIMEOUT = float(os.environ.get("SIO_CROSS_AID_SHUTDOWN_TIMEOUT_S", "30"))


def cap_components(path: Path) -> tuple[bytes, bytes, list[bytes]]:
    components = COMPONENTS
    # GPPro's standard CAPFile.getCode() omits this optional component. Set the
    # override to retain coverage for installations that include it explicitly.
    if os.environ.get("SIO_CROSS_AID_INCLUDE_DESCRIPTOR") != "1":
        components = tuple(name for name in COMPONENTS if name != "Descriptor")
    with ZipFile(path) as cap:
        names = set(cap.namelist())
        header_name = next(n for n in names if n.endswith("/Header.cap"))
        folder = header_name.rsplit("/", 1)[0]
        code = b"".join(
            cap.read(f"{folder}/{name}.cap")
            for name in components
            if f"{folder}/{name}.cap" in names
        )
        header = cap.read(header_name)
        aid_length = header[12]
        package_aid = header[13:13 + aid_length]
        applet_name = f"{folder}/Applet.cap"
        applet_data = cap.read(applet_name) if applet_name in names else b""
    if not applet_data:
        return code, package_aid, []
    if len(applet_data) < 4 or applet_data[0] != 3:
        raise RuntimeError(f"invalid Applet component in {path}")
    applets = []
    offset = 4
    for _ in range(applet_data[3]):
        length = applet_data[offset]
        offset += 1
        applets.append(applet_data[offset:offset + length])
        offset += length + 2
    return code, package_aid, applets


def install(scp: vpcd.Scp03, cap: tuple[bytes, bytes, list[bytes]]) -> None:
    code, package_aid, applets = cap
    if len(applets) != 1:
        raise RuntimeError(f"expected one fixture applet, got {len(applets)}")
    vpcd.load_package(scp, code, package_aid)
    vpcd.install_applet(scp, package_aid, applets[0], applets[0])


def main() -> int:
    if os.environ.get("SIO_CROSS_AID_REUSE_FIXTURE") != "1":
        subprocess.run([str(ROOT / "tools/build-sio-cross-aid-fixture")],
                       cwd=ROOT, check=True, timeout=180)
    build = ROOT / "build/sio-cross-aid-fixture"
    contract = cap_components(
        build / "contract-cap/sio/contract/javacard/contract.cap"
    )
    service = cap_components(
        build / "service-cap/sio/service/javacard/service.cap"
    )
    client = cap_components(
        build / "client-cap/sio/client/javacard/client.cap"
    )
    if contract[2] or service[1:] != (SERVICE_PACKAGE, [SERVICE_APPLET]) or \
            client[1:] != (CLIENT_PACKAGE, [CLIENT_APPLET]):
        raise RuntimeError("fixture package/applet AID validation failed")

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/algtest-v182-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")

    started = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    memory = build / "sio-cross-aid-vpcd.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/sio-cross-aid-vpcd-{started}.log"
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
            connection, _ = vpcd.accept_vpcd(listener, process, CONNECT_TIMEOUT,
                                              log_path, "cross-AID SIO")
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            atr = vpcd.receive_message(connection)
            if atr != bytes.fromhex("3B800181"):
                raise RuntimeError(f"unexpected VPCD ATR {atr.hex().upper()}")
            response = vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                                     "SELECT ISD")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT ISD failed: {response.hex().upper()}")
            scp = vpcd.Scp03(connection)
            scp.authenticate()

            vpcd.load_package(scp, contract[0], contract[1])
            print("TEST:PASS loaded Shareable contract and Export component")
            install(scp, service)
            print("TEST:PASS installed cross-AID service applet")
            install(scp, client)
            print("TEST:PASS installed cross-AID client applet")

            select_client = bytes((0x00, 0xA4, 0x04, 0x00, len(CLIENT_APPLET))) + CLIENT_APPLET
            response = vpcd.exchange(connection, select_client, "SELECT client")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT client failed: {response.hex().upper()}")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "cross-AID Shareable invocation")
            expected = bytes.fromhex("45679000")
            if response != expected:
                raise RuntimeError(
                    f"cross-AID SIO result {response.hex().upper()}, expected "
                    f"{expected.hex().upper()}"
                )
            print("TEST:PASS cross-AID SIO byte/short args, global AID and "
                  "null array; response=45679000")
            response = vpcd.exchange(connection, bytes.fromhex("8015000000"),
                                     "retain Shareable proxy across APDUs")
            if response != bytes.fromhex("6A109000"):
                raise RuntimeError(
                    f"proxy retention result {response.hex().upper()}, "
                    "expected 6A109000"
                )
            connection.close()
            connection = None
            process.wait(timeout=SHUTDOWN_TIMEOUT)
            if process.returncode != 0:
                raise RuntimeError(
                    f"first emulator exited with {process.returncode} before restart"
                )
            with log_path.open("ab") as log:
                process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                           stderr=log, start_new_session=True)
            connection, _ = vpcd.accept_vpcd(
                listener, process, CONNECT_TIMEOUT, log_path,
                "cross-AID SIO restart"
            )
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            reset_atr = vpcd.receive_message(connection)
            if reset_atr != bytes.fromhex("3B800181"):
                raise RuntimeError(
                    f"restart ATR {reset_atr.hex().upper()}, expected 3B800181"
                )
            response = vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                                     "SELECT ISD after emulator restart")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"SELECT ISD after restart {response.hex().upper()}"
                )
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            response = vpcd.exchange(connection, select_client,
                                     "reselect client after emulator restart")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"client reselect after restart {response.hex().upper()}"
                )
            response = vpcd.exchange(
                connection, bytes.fromhex("8019000000"),
                "reject aliased stale proxy after emulator restart"
            )
            if response != bytes.fromhex("6A0B9000"):
                raise RuntimeError(
                    f"stale proxy alias after restart {response.hex().upper()}, "
                    "expected caught fail-closed SecurityException 6A0B9000"
                )
            print("TEST:PASS stale proxy restored through an object alias "
                  "fails closed")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "fresh cross-AID lookup after emulator restart")
            if response != bytes.fromhex("45679000"):
                raise RuntimeError(
                    f"fresh lookup after restart {response.hex().upper()}, "
                    "expected 45679000"
                )
            print("TEST:PASS fresh cross-AID lookup works after emulator restart")
            response = vpcd.exchange(connection, bytes.fromhex("8018000000"),
                                     "reject stale proxy virtual call after restart")
            if response != bytes.fromhex("6A0B9000"):
                raise RuntimeError(
                    f"stale proxy virtual call {response.hex().upper()}, "
                    "expected caught SecurityException 6A0B9000"
                )
            print("TEST:PASS stale proxy virtual dispatch fails closed")
            response = vpcd.exchange(connection, bytes.fromhex("8016000000"),
                                     "reject stale proxy after emulator restart")
            if response != bytes.fromhex("6A0A9000"):
                raise RuntimeError(
                    f"stale proxy after restart {response.hex().upper()}, "
                    "expected caught fail-closed SecurityException 6A0A9000"
                )
            print("TEST:PASS stale proxy after process restart fails closed")
            vpcd.send_message(connection, b"\x02")
            vpcd.send_message(connection, b"\x04")
            reset_atr = vpcd.receive_message(connection)
            if reset_atr != bytes.fromhex("3B800181"):
                raise RuntimeError(
                    f"power-cycle ATR {reset_atr.hex().upper()}, expected 3B800181"
                )
            response = vpcd.exchange(connection, select_client,
                                     "reselect client after card reset")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"client reselect after reset {response.hex().upper()}"
                )
            response = vpcd.exchange(connection, bytes.fromhex("8016000000"),
                                     "reject stale proxy after reset")
            if response != bytes.fromhex("6A0A9000"):
                raise RuntimeError(
                    f"stale proxy after reset {response.hex().upper()}, "
                    "expected caught SecurityException 6A0A9000"
                )
            print("TEST:PASS stale proxy remains blocked after card reset")
            response = vpcd.exchange(connection, bytes.fromhex("8013000000"),
                                     "cross-AID int argument and return")
            if response != bytes.fromhex("765432109000"):
                raise RuntimeError(
                    f"cross-AID int result {response.hex().upper()}, "
                    "expected 765432109000"
                )
            print("TEST:PASS cross-AID int argument and return; "
                  "response=765432109000")
            response = vpcd.exchange(connection, bytes.fromhex("8014000000"),
                                     "cross-context exception unwinding")
            if response != bytes.fromhex("6A079000"):
                raise RuntimeError(
                    f"cross-context exception result {response.hex().upper()}, "
                    "expected 6A079000"
                )
            print("TEST:PASS server exception is caught in client; "
                  "response=6A079000")
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "context restored after server exception")
            if response != bytes.fromhex("45679000"):
                raise RuntimeError(
                    f"post-exception SIO result {response.hex().upper()}, "
                    "expected 45679000"
                )
            print("TEST:PASS client/service contexts restored after exception")
            response = vpcd.exchange(connection, bytes.fromhex("8011000000"),
                                     "denied Shareable lookup")
            if response != bytes.fromhex("6A019000"):
                raise RuntimeError(
                    f"denied lookup result {response.hex().upper()}, "
                    "expected 6A019000"
                )
            print("TEST:PASS service denial returns null; response=6A019000")
            response = vpcd.exchange(connection, bytes.fromhex("8012000000"),
                                     "applet-local reference firewall")
            if response != bytes.fromhex("6A039000"):
                raise RuntimeError(
                    f"applet-local reference result {response.hex().upper()}, "
                    "expected 6A039000"
                )
            print("TEST:PASS applet-local array rejected across contexts; "
                  "response=6A039000")
            response = vpcd.exchange(connection, bytes.fromhex("8017000000"),
                                     "proxy Object.equals firewall")
            if response != bytes.fromhex("6A099000"):
                raise RuntimeError(
                    f"proxy Object.equals result {response.hex().upper()}, "
                    "expected 6A099000"
                )
            print("TEST:PASS Object.equals on proxy is blocked; "
                  "response=6A099000")
            print("TEST:SUMMARY pass=15 fail=0")
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
            print(f"VPCD:LOG {log_path.relative_to(ROOT)}")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, socket.timeout,
            subprocess.SubprocessError) as exc:
        print(f"TEST:FAIL {exc}", file=sys.stderr)
        raise SystemExit(1)
