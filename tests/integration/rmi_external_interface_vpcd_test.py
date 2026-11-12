#!/usr/bin/env python3
"""Exercise CAP 2.2 external remote-interface links through real INSTALL."""

from __future__ import annotations

import os
import socket
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/integration"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402


COMPONENTS = (
    "Header", "Directory", "Applet", "Import", "ConstantPool", "Class",
    "Method", "StaticField", "RefLocation", "Descriptor",
)


def cap_components(path: Path, include_export: bool = False
                   ) -> tuple[bytes, bytes, list[bytes]]:
    with ZipFile(path) as cap:
        header_name = next(name for name in cap.namelist()
                           if name.endswith("/Header.cap"))
        folder = header_name.rsplit("/", 1)[0]
        names = set(cap.namelist())
        components = list(COMPONENTS)
        if include_export:
            components.insert(components.index("Descriptor"), "Export")
        code = b"".join(
            cap.read(f"{folder}/{component}.cap")
            for component in components
            if f"{folder}/{component}.cap" in names
        )
        header = cap.read(header_name)
        package_aid_length = header[12]
        package_aid = header[13:13 + package_aid_length]
        applet_name = f"{folder}/Applet.cap"
        applets = []
        if applet_name in names:
            applet = cap.read(applet_name)
            if len(applet) < 4 or applet[0] != 3:
                raise RuntimeError(f"invalid Applet component in {path}")
            cursor = 4
            for _ in range(applet[3]):
                aid_length = applet[cursor]
                cursor += 1
                applets.append(applet[cursor:cursor + aid_length])
                cursor += aid_length + 2
    return code, package_aid, applets


def service_variant(source: Path, target: Path, export_token: int) -> None:
    with ZipFile(source) as cap:
        files = {name: cap.read(name) for name in cap.namelist()}
    class_name = next(name for name in files if name.endswith("/Class.cap"))
    class_data = bytearray(files[class_name])
    marker = b"CounterImpl\x01\x82\x00"
    if class_data.count(marker) != 1:
        raise RuntimeError("service Class.cap lacks unique external-ref fixture")
    token_offset = class_data.index(marker) + len(b"CounterImpl\x01") + 1
    class_data[token_offset] = export_token
    files[class_name] = bytes(class_data)
    target.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(target, "w", ZIP_DEFLATED) as cap:
        for name, data in files.items():
            cap.writestr(name, data)


def contract_with_missing_library_import(source: Path, target: Path) -> None:
    """Add an otherwise-unused import to the library package's CAP copy."""
    with ZipFile(source) as cap:
        files = {name: cap.read(name) for name in cap.namelist()}
    import_name = next(name for name in files if name.endswith("/Import.cap"))
    directory_name = next(name for name in files
                          if name.endswith("/Directory.cap"))
    imports = bytearray(files[import_name])
    directory = bytearray(files[directory_name])
    if len(imports) != 24 or imports[:4] != bytes.fromhex("04001502"):
        raise RuntimeError("contract fixture Import.cap layout changed")
    if directory[33] != 2:
        raise RuntimeError("contract fixture Directory import count changed")

    # Import a package that is absent from the card. The contract library does
    # not reference it from its Constant Pool, so validation only catches this
    # when INSTALL checks the complete dependency closure.
    missing_package = bytes.fromhex("000107A00000015100FE")
    imports[1] = 0x00
    imports[2] = 0x1f
    imports[3] = 0x03
    imports.extend(missing_package)
    directory[9:11] = bytes.fromhex("001f")
    directory[33] = 3
    files[import_name] = bytes(imports)
    files[directory_name] = bytes(directory)

    target.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(target, "w", ZIP_DEFLATED) as cap:
        for name, data in files.items():
            cap.writestr(name, data)


def start_case(label: str, root: Path, emulator: Path, rom_mask: Path,
               dummy_code: Path, contract_code: bytes, contract_aid: bytes,
               service_code: bytes, service_aid: bytes, applet_aid: bytes,
               expected_status: int, started_at: str) -> None:
    memory = root / f"build/rmi-external-fixture/{label}.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = root / f"artifacts/logs/rmi-external-link-{label}-{started_at}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        host, port = listener.getsockname()
        command = [
            str(root / "tools/env"), str(emulator), "-m", str(memory), "-s",
            "--vpcd", f"{host}:{port}", "--applet-aid",
            vpcd.EMPTY_CARD_AID.hex(), "--package-id", "63", "--cap-code",
            str(dummy_code), "--rom-mask", str(rom_mask),
        ]
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, cwd=root, stdout=log,
                                       stderr=log, start_new_session=True,
                                       env=os.environ.copy())
        connection = None
        try:
            connection, _ = vpcd.accept_vpcd(
                listener, process, 45, log_path, label
            )
            connection.settimeout(45)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            atr = vpcd.receive_message(connection)
            if atr != bytes.fromhex("3B800181"):
                raise RuntimeError(f"{label}: unexpected ATR {atr.hex()}")
            selected = vpcd.exchange(
                connection, bytes.fromhex("00A4040000"), f"{label} SELECT ISD"
            )
            if selected[-2:] != b"\x90\x00":
                raise RuntimeError(f"{label}: SELECT ISD {selected.hex()}")
            scp = vpcd.Scp03(connection)
            scp.authenticate()

            vpcd.load_package(scp, contract_code, contract_aid)
            print(f"TEST:PASS {label} loaded interface/export package")
            vpcd.load_package(scp, service_code, service_aid)
            data = (vpcd.lv(service_aid) + vpcd.lv(applet_aid) +
                    vpcd.lv(applet_aid) + bytes.fromhex("03000000") + b"\x00\x00")
            response = scp.command(
                0xE6, 0x0C, 0x00, data, f"{label} INSTALL",
                expected_statuses=(0x9000, 0x6F00, 0x6A80, 0x6A82, 0x6A88,
                                   0x6985),
            )
            actual_status = int.from_bytes(response[-2:], "big")
            if actual_status != expected_status:
                raise RuntimeError(
                    f"{label}: INSTALL status {actual_status:04X}, expected "
                    f"{expected_status:04X}; response={response.hex().upper()}"
                )
            print(f"TEST:PASS {label} INSTALL status={actual_status:04X}")
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


def main() -> int:
    vpcd.ensure_algtest_load_code(ROOT)
    root = ROOT
    subprocess.run([str(root / "tools/build-rmi-external-fixture")],
                   cwd=root, check=True)
    build = root / "build/rmi-external-fixture"
    contract_cap = build / "contract-cap/rmi/contract/javacard/contract.cap"
    service_cap = build / "service-cap/rmi/service/javacard/service.cap"
    contract_code, contract_aid, contract_applets = cap_components(
        contract_cap, include_export=True
    )
    if contract_applets:
        raise RuntimeError("contract fixture unexpectedly contains an applet")
    valid_code, service_aid, service_applets = cap_components(service_cap)
    if len(service_applets) != 1:
        raise RuntimeError("service fixture must contain exactly one applet")

    missing_dependency_cap = build / "contract-missing-dependency.cap"
    contract_with_missing_library_import(contract_cap,
                                         missing_dependency_cap)
    missing_dependency_code, missing_dependency_aid, missing_applets = \
        cap_components(missing_dependency_cap, include_export=True)
    if (missing_dependency_aid != contract_aid or missing_applets or
            missing_dependency_code == contract_code):
        raise RuntimeError("invalid missing-library-dependency fixture")

    variants = []
    for label, export_token, expected in (
        ("valid-interface", 0, 0x9000),
        ("exported-class", 1, 0x6F00),
        ("missing-export", 2, 0x6F00),
    ):
        if export_token == 0:
            code = valid_code
        else:
            variant_cap = build / f"{label}.cap"
            service_variant(service_cap, variant_cap, export_token)
            code, variant_aid, variant_applets = cap_components(variant_cap)
            if variant_aid != service_aid or variant_applets != service_applets:
                raise RuntimeError("patched fixture changed package/applet AIDs")
        variants.append((label, code, expected))

    for label, service_code, expected in variants:
        start_case(
            label, root, root / "build/choupi-pc/choupi",
            root / "build/choupi-pc/flash",
            root / "build/tests/jcalgtest/algtest-v184-load-code.bin", contract_code,
            contract_aid, service_code, service_aid, service_applets[0],
            expected, datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        )
    start_case(
        "missing-library-dependency", root, root / "build/choupi-pc/choupi",
        root / "build/choupi-pc/flash",
        root / "build/tests/jcalgtest/algtest-v184-load-code.bin",
        missing_dependency_code, contract_aid, valid_code, service_aid,
        service_applets[0], 0x6A88,
        datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
    )
    print("TEST:SUMMARY pass=4 fail=0")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, socket.timeout, subprocess.SubprocessError) as exc:
        print(f"TEST:FAIL {exc}", file=sys.stderr)
        raise SystemExit(1)
