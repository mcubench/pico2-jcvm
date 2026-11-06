#!/usr/bin/env python3
"""Exercise cross-AID Shareable dispatch through the PC VPCD.

Build the PC VM with ``symm_crypto`` or ``max_crypto`` first; the fixture's
AES-MAC Signature cases are intentionally unsupported by the ``current``
profile.
"""

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
CONNECT_TIMEOUT = float(os.environ.get("SIO_CROSS_AID_CONNECT_TIMEOUT_S", "90"))
APDU_TIMEOUT = float(os.environ.get("SIO_CROSS_AID_APDU_TIMEOUT_S", "120"))
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


def replace_static_field_descriptor_type(code: bytes, static_offset: int,
                                         field_type: int) -> bytes:
    """Change one internal static field's declared type in a test CAP image."""
    mutated = bytearray(code)
    component_offset = 0
    while component_offset < len(mutated):
        if len(mutated) - component_offset < 3:
            raise RuntimeError("truncated test CAP component header")
        tag = mutated[component_offset]
        size = int.from_bytes(mutated[component_offset + 1:component_offset + 3],
                              "big")
        payload_offset = component_offset + 3
        payload_end = payload_offset + size
        if payload_end > len(mutated):
            raise RuntimeError("truncated test CAP component payload")
        if tag == 11:
            payload = payload_offset
            if size < 1:
                raise RuntimeError("empty Descriptor component")
            class_count = mutated[payload]
            offset = payload + 1
            for _ in range(class_count):
                if payload_end - offset < 9:
                    raise RuntimeError("truncated Descriptor class record")
                interfaces = mutated[offset + 4]
                fields = int.from_bytes(mutated[offset + 5:offset + 7], "big")
                methods = int.from_bytes(mutated[offset + 7:offset + 9], "big")
                fields_offset = offset + 9 + interfaces * 2
                record_end = fields_offset + fields * 7 + methods * 12
                if record_end > payload_end:
                    raise RuntimeError("truncated Descriptor class tables")
                for field_index in range(fields):
                    field = fields_offset + field_index * 7
                    is_static = (mutated[field + 1] & 0x08) != 0
                    is_internal = (mutated[field + 2] & 0x80) == 0
                    declared_offset = int.from_bytes(
                        mutated[field + 3:field + 5], "big")
                    if is_static and is_internal and declared_offset == static_offset:
                        mutated[field + 5:field + 7] = field_type.to_bytes(2, "big")
                        return bytes(mutated)
                offset = record_end
            raise RuntimeError(
                f"no internal static field at image offset {static_offset}")
        component_offset = payload_end
    raise RuntimeError("test CAP has no Descriptor component")


def replace_instance_field_descriptor_type(code: bytes, class_offset: int,
                                           token: int, field_type: int) -> bytes:
    """Change one instance field's declared type in a test CAP image."""
    mutated = bytearray(code)
    component_offset = 0
    while component_offset < len(mutated):
        if len(mutated) - component_offset < 3:
            raise RuntimeError("truncated test CAP component header")
        tag = mutated[component_offset]
        size = int.from_bytes(mutated[component_offset + 1:component_offset + 3],
                              "big")
        payload_offset = component_offset + 3
        payload_end = payload_offset + size
        if payload_end > len(mutated):
            raise RuntimeError("truncated test CAP component payload")
        if tag == 11:
            if size < 1:
                raise RuntimeError("empty Descriptor component")
            class_count = mutated[payload_offset]
            offset = payload_offset + 1
            for _ in range(class_count):
                if payload_end - offset < 9:
                    raise RuntimeError("truncated Descriptor class record")
                interfaces = mutated[offset + 4]
                fields = int.from_bytes(mutated[offset + 5:offset + 7], "big")
                methods = int.from_bytes(mutated[offset + 7:offset + 9], "big")
                fields_offset = offset + 9 + interfaces * 2
                record_end = fields_offset + fields * 7 + methods * 12
                if record_end > payload_end:
                    raise RuntimeError("truncated Descriptor class tables")
                for field_index in range(fields):
                    field = fields_offset + field_index * 7
                    is_static = (mutated[field + 1] & 0x08) != 0
                    field_class = int.from_bytes(
                        mutated[field + 2:field + 4], "big")
                    field_token = mutated[field + 4]
                    if not is_static and field_class == class_offset and \
                            field_token == token:
                        mutated[field + 5:field + 7] = field_type.to_bytes(2, "big")
                        return bytes(mutated)
                offset = record_end
            raise RuntimeError(
                f"no instance field at class offset {class_offset}, token {token}")
        component_offset = payload_end
    raise RuntimeError("test CAP has no Descriptor component")


def main() -> int:
    vpcd.ensure_algtest_v182_load_code(ROOT)
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
        emulator_env = os.environ.copy()
        emulator_env["CHOUPI_TRACE_REFERENCE_COLLISIONS"] = "1"
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                       stderr=log, env=emulator_env)
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
            if os.environ.get("SIO_CROSS_AID_TEST_BAD_INSTANCE_TYPE") == "1":
                bad_service_code = replace_instance_field_descriptor_type(
                    service[0], 0x0013, 0, 0x8005)
                vpcd.load_package(scp, bad_service_code, service[1])
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6A80,))
                if response[-2:] != bytes.fromhex("6A80"):
                    raise RuntimeError(
                        "CAP with an int declaration for a reference instance "
                        f"field returned {response[-2:].hex().upper()}, "
                        "expected fail-closed INSTALL status 6A80"
                    )
                print("TEST:PASS rejected a reference store whose Descriptor "
                      "declares int")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_BAD_STATIC_TYPE") == "1":
                bad_service_code = replace_static_field_descriptor_type(
                    service[0], 0, 0x8005)
                vpcd.load_package(scp, bad_service_code, service[1])
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6F00,))
                if response[-2:] != bytes.fromhex("6F00"):
                    raise RuntimeError(
                        "CAP with a primitive declaration for a reference "
                        f"static slot returned {response[-2:].hex().upper()}, "
                        "expected fail-closed VM status 6F00"
                    )
                print("TEST:PASS rejected a static reference initialization "
                      "whose Descriptor declares int")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            install(scp, service)
            print("TEST:PASS installed cross-AID service applet")
            install(scp, client)
            print("TEST:PASS installed cross-AID client applet")

            select_client = bytes((0x00, 0xA4, 0x04, 0x00, len(CLIENT_APPLET))) + CLIENT_APPLET
            response = vpcd.exchange(connection, select_client, "SELECT client")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT client failed: {response.hex().upper()}")
            if os.environ.get("SIO_CROSS_AID_ONLY_GLOBAL_ARRAY") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("8030000000"),
                    "global array SensitiveResult survives SIO context switch"
                )
                if response != bytes.fromhex("6A709000"):
                    raise RuntimeError(
                        "global array SensitiveResult assertion returned "
                        f"{response.hex().upper()}, expected 6A709000"
                    )
                print("TEST:PASS interpreted JCSystem.makeGlobalArray result "
                      "is assertable after the SIO context switch")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_SIGNATURE") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("8028000000"),
                    "AES-MAC Signature SensitiveResult across SIO"
                )
                if response != bytes.fromhex("6A509000"):
                    raise RuntimeError(
                        "isolated Signature SensitiveResult returned "
                        f"{response.hex().upper()}, expected 6A509000"
                    )
                print("TEST:PASS isolated AES-MAC Signature SIO assertion")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_KEYBUILDER") == "1":
                for command, expected, description in (
                    ("8032000000", "6A739000",
                     "KeyBuilder success records its created key"),
                    ("8033000000", "6A759000",
                     "failed KeyBuilder clears the stale object result"),
                ):
                    response = vpcd.exchange(
                        connection, bytes.fromhex(command), description
                    )
                    if response != bytes.fromhex(expected):
                        raise RuntimeError(
                            f"{description} returned {response.hex().upper()}, "
                            f"expected {expected}"
                        )
                print("TEST:PASS KeyBuilder publishes successful object result "
                      "and clears stale result on failure")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_PIN") == "1":
                for command, expected, description in (
                    ("8036000000", "6A7C9000",
                     "PIN byte/boolean results across SIO"),
                    ("8038000000", "6A829000",
                     "OwnerPIN invokevirtual results across SIO"),
                    ("8037000000", "6A7F9000",
                     "OwnerPINx byte results across SIO"),
                ):
                    response = vpcd.exchange(
                        connection, bytes.fromhex(command), description
                    )
                    if response != bytes.fromhex(expected):
                        raise RuntimeError(
                            f"{description} returned {response.hex().upper()}, "
                            f"expected {expected}"
                        )
                print("TEST:PASS PIN/OwnerPINx SensitiveResult assertions across SIO")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_RANDOM") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("8039000000"),
                    "RandomData SensitiveResult across SIO"
                )
                if response != bytes.fromhex("6A859000"):
                    raise RuntimeError(
                        "RandomData SensitiveResult returned "
                        f"{response.hex().upper()}, expected 6A859000"
                    )
                print("TEST:PASS RandomData getAlgorithm/nextBytes results across SIO")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_SENSITIVE_ARRAYS") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("803A000000"),
                    "SensitiveArrays SensitiveResult across SIO"
                )
                if response != bytes.fromhex("6A889000"):
                    raise RuntimeError(
                        "SensitiveArrays SensitiveResult returned "
                        f"{response.hex().upper()}, expected 6A889000"
                    )
                print("TEST:PASS SensitiveArrays false results across SIO")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_AID") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("803B000000"),
                    "AID SensitiveResult across SIO"
                )
                if response != bytes.fromhex("6A319000"):
                    raise RuntimeError(
                        f"AID SensitiveResult returned {response.hex().upper()}, "
                        "expected 6A319000"
                    )
                print("TEST:PASS AID true/false results across SIO")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_UTIL_ATOMIC") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("803D000000"),
                    "atomic Util SensitiveResult across SIO"
                )
                if response != bytes.fromhex("6A3D9000"):
                    raise RuntimeError(
                        "atomic Util SensitiveResult returned "
                        f"{response.hex().upper()}, expected 6A3D9000"
                    )
                print("TEST:PASS atomic Util copy/fill/compare results and "
                      "failed-copy reset across SIO")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_RESET") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("803E000000"),
                    "caller reset clears SensitiveResult after SIO"
                )
                if response != bytes.fromhex("6A3E9000"):
                    raise RuntimeError(
                        "SensitiveResult.reset after SIO returned "
                        f"{response.hex().upper()}, expected 6A3E9000"
                    )
                print("TEST:PASS caller asserts SIO result, resets it, then "
                      "observes the result as unassigned")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_JCSYSTEM") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("803C000000"),
                    "JCSystem lookupAID/getAppletShareableInterfaceObject SensitiveResult"
                )
                if response != bytes.fromhex("6A8B9000"):
                    raise RuntimeError(
                        "JCSystem SensitiveResult returned "
                        f"{response.hex().upper()}, expected 6A8B9000"
                    )
                print("TEST:PASS JCSystem lookupAID and shareable results")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_INHERITED") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("802C000000"),
                    "check inherited interfaces through same-package and imported superclasses"
                )
                if response != bytes.fromhex("6A6A9000"):
                    raise RuntimeError(
                        "inherited superclass interface checks returned "
                        f"{response.hex().upper()}, expected 6A6A9000"
                    )
                print("TEST:PASS inherited interface checkcast and instanceof")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_ONLY_UTIL") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("802D000000"),
                    "prevalidate Util non-atomic copy and fill ranges"
                )
                if response != bytes.fromhex("6A709000"):
                    raise RuntimeError(
                        "non-atomic Util validation returned "
                        f"{response.hex().upper()}, expected 6A709000"
                    )
                print("TEST:PASS invalid Util copy/fill calls preserve arrays, "
                      "throw expected exceptions, and clear SensitiveResult")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
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
            response = vpcd.exchange(
                connection, bytes.fromhex("8021000000"),
                "SensitiveResult from service API survives SIO context switch"
            )
            if response != bytes.fromhex("6A289000"):
                raise RuntimeError(
                    "cross-context SensitiveResult assertion returned "
                    f"{response.hex().upper()}, expected 6A289000"
                )
            print("TEST:PASS interpreted client applet asserts service "
                  "Util.arrayCopyNonAtomic result after SIO returns")
            response = vpcd.exchange(
                connection, bytes.fromhex("8022000000"),
                "failed sensitive service API leaves result unassigned"
            )
            if response != bytes.fromhex("6A2B9000"):
                raise RuntimeError(
                    "failed cross-context sensitive API assertion returned "
                    f"{response.hex().upper()}, expected 6A2B9000"
                )
            print("TEST:PASS failed service Util.arrayCopyNonAtomic clears "
                  "SensitiveResult before the caller assertion")
            response = vpcd.exchange(
                connection, bytes.fromhex("8023000000"),
                "SensitiveResult object results from AID APIs in service"
            )
            if response != bytes.fromhex("6A319000"):
                raise RuntimeError(
                    "service AID SensitiveResult checks returned "
                    f"{response.hex().upper()}, expected 6A319000"
                )
            print("TEST:PASS interpreted service asserts getAID and "
                  "getPreviousContextAID object results")
            response = vpcd.exchange(
                connection, bytes.fromhex("8024000000"),
                "caller AID SensitiveResult survives a passive SIO call"
            )
            if response != bytes.fromhex("6A359000"):
                raise RuntimeError(
                    "caller AID SensitiveResult did not survive passive SIO: "
                    f"{response.hex().upper()}, expected 6A359000"
                )
            print("TEST:PASS caller getAID object result survives SIO when "
                  "service invokes no sensitive API")
            response = vpcd.exchange(
                connection, bytes.fromhex("8025000000"),
                "SHA-256 doFinal SensitiveResult survives SIO"
            )
            if response != bytes.fromhex("6A379000"):
                raise RuntimeError(
                    "SHA-256 doFinal SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A379000"
                )
            print("TEST:PASS native SHA-256 doFinal output-length result is "
                  "assertable by the caller after SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("8026000000"),
                "failed SHA-256 doFinal clears prior SensitiveResult"
            )
            if response != bytes.fromhex("6A409000"):
                raise RuntimeError(
                    "failed SHA-256 doFinal left a stale SensitiveResult: "
                    f"{response.hex().upper()}, expected 6A409000"
                )
            print("TEST:PASS invalid SHA-256 doFinal clears prior object "
                  "SensitiveResult before SIO returns")
            response = vpcd.exchange(
                connection, bytes.fromhex("8027000000"),
                "Cipher update/final SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A429000"):
                raise RuntimeError(
                    "Cipher SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A429000"
                )
            print("TEST:PASS native AES Cipher update and doFinal results "
                  "are assertable across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("8028000000"),
                "Signature sign/verify SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A509000"):
                raise RuntimeError(
                    "Signature SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A509000"
                )
            print("TEST:PASS native Signature sign length and verify result "
                  "are assertable across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("8029000000"),
                "Signature verify false SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A569000"):
                raise RuntimeError(
                    "Signature false SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A569000"
                )
            print("TEST:PASS native Signature verify false result is "
                  "assertable across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("802A000000"),
                "reject equal compact object handle from foreign SIO owner"
            )
            if response != bytes.fromhex("6A599000"):
                raise RuntimeError(
                    "foreign-owner SensitiveResult collision returned "
                    f"{response.hex().upper()}, expected 6A599000"
                )
            print("TEST:PASS interpreted SIO SensitiveResult rejects the "
                  "server applet object when its compact handle collides "
                  "with the client applet object")
            response = vpcd.exchange(
                connection, bytes.fromhex("8030000000"),
                "global array SensitiveResult survives SIO context switch"
            )
            if response != bytes.fromhex("6A709000"):
                raise RuntimeError(
                    "global array SensitiveResult assertion returned "
                    f"{response.hex().upper()}, expected 6A709000"
                )
            print("TEST:PASS interpreted JCSystem.makeGlobalArray result "
                  "is assertable after the SIO context switch")
            response = vpcd.exchange(
                connection, bytes.fromhex("8032000000"),
                "KeyBuilder success records its created key"
            )
            if response != bytes.fromhex("6A739000"):
                raise RuntimeError(
                    "KeyBuilder SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A739000"
                )
            response = vpcd.exchange(
                connection, bytes.fromhex("8033000000"),
                "failed KeyBuilder clears stale object result"
            )
            if response != bytes.fromhex("6A759000"):
                raise RuntimeError(
                    "failed KeyBuilder SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A759000"
                )
            print("TEST:PASS KeyBuilder publishes successful object result and "
                  "clears stale result on failure")
            response = vpcd.exchange(
                connection, bytes.fromhex("8036000000"),
                "PIN SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A7C9000"):
                raise RuntimeError(
                    f"PIN SensitiveResult returned {response.hex().upper()}, "
                    "expected 6A7C9000"
                )
            print("TEST:PASS PIN byte/boolean result assertions across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("8038000000"),
                "OwnerPIN invokevirtual SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A829000"):
                raise RuntimeError(
                    f"OwnerPIN invokevirtual SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A829000"
                )
            print("TEST:PASS OwnerPIN invokevirtual byte/boolean result assertions across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("8037000000"),
                "OwnerPINx SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A7F9000"):
                raise RuntimeError(
                    f"OwnerPINx SensitiveResult returned {response.hex().upper()}, "
                    "expected 6A7F9000"
                )
            print("TEST:PASS OwnerPINx and predecrement byte results across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("8039000000"),
                "RandomData SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A859000"):
                raise RuntimeError(
                    f"RandomData SensitiveResult returned {response.hex().upper()}, "
                    "expected 6A859000"
                )
            print("TEST:PASS RandomData getAlgorithm/nextBytes results across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("803A000000"),
                "SensitiveArrays SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A889000"):
                raise RuntimeError(
                    f"SensitiveArrays SensitiveResult returned {response.hex().upper()}, "
                    "expected 6A889000"
                )
            print("TEST:PASS SensitiveArrays false results across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("803B000000"),
                "AID SensitiveResult across SIO"
            )
            if response != bytes.fromhex("6A319000"):
                raise RuntimeError(
                    f"AID SensitiveResult returned {response.hex().upper()}, "
                    "expected 6A319000"
                )
            print("TEST:PASS AID partialEquals, equals and RIDEquals true/false results across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("803D000000"),
                "atomic Util SensitiveResult true/false results across SIO"
            )
            if response != bytes.fromhex("6A3D9000"):
                raise RuntimeError(
                    "atomic Util SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A3D9000"
                )
            print("TEST:PASS atomic Util arrayCopy/arrayFill/arrayCompare and "
                  "failed-copy SensitiveResult across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("803E000000"),
                "caller SensitiveResult.reset after SIO"
            )
            if response != bytes.fromhex("6A3E9000"):
                raise RuntimeError(
                    "SensitiveResult.reset after SIO returned "
                    f"{response.hex().upper()}, expected 6A3E9000"
                )
            print("TEST:PASS caller resets the SIO SensitiveResult after "
                  "asserting it")
            response = vpcd.exchange(
                connection, bytes.fromhex("803C000000"),
                "JCSystem lookupAID/getAppletShareableInterfaceObject SensitiveResult"
            )
            if response != bytes.fromhex("6A8B9000"):
                raise RuntimeError(
                    "JCSystem SensitiveResult returned "
                    f"{response.hex().upper()}, expected 6A8B9000"
                )
            print("TEST:PASS JCSystem lookupAID and shareable object results across SIO")
            response = vpcd.exchange(
                connection, bytes.fromhex("802B000000"),
                "propagate exact exception type and reason across SIO"
            )
            if response != bytes.fromhex("6A669000"):
                raise RuntimeError(
                    "SIO exception propagation result "
                    f"{response.hex().upper()}, expected both exception "
                    "types/reason and a successful follow-up call 6A669000"
                )
            print("TEST:PASS SIO preserves CryptoException reason and concrete "
                  "ArrayIndexOutOfBoundsException identity, then allows a "
                  "successful follow-up call")
            response = vpcd.exchange(
                connection, bytes.fromhex("802C000000"),
                "check implemented interfaces inherited through local and imported superclasses"
            )
            if response != bytes.fromhex("6A6A9000"):
                raise RuntimeError(
                    "inherited superclass interface checks returned "
                    f"{response.hex().upper()}, expected 6A6A9000"
                )
            print("TEST:PASS checkcast and instanceof find interfaces declared "
                  "on same-package and imported superclasses")
            response = vpcd.exchange(
                connection, bytes.fromhex("801A000000"),
                "cross-context private array reference firewall"
            )
            if response != bytes.fromhex("5A319000"):
                raise RuntimeError(
                    f"private-array firewall result {response.hex().upper()}, "
                    "expected caught SecurityException 5A319000"
                )
            print("TEST:PASS private byte[] argument crosses as an owned "
                  "reference and its server-side array access is denied")
            response = vpcd.exchange(
                connection, bytes.fromhex("801B000000"),
                "global Object[] identity and forbidden global-array retention"
            )
            if response != bytes.fromhex("6B339000"):
                raise RuntimeError(
                    f"global Object[] reference result {response.hex().upper()}, "
                    "expected identity and three caught SecurityExceptions "
                    "6B339000"
                )
            print("TEST:PASS global Object[] preserves foreign identity against "
                  "the service local reference; retaining the global-array "
                  "reference is rejected and foreign use is blocked")
            response = vpcd.exchange(
                connection, bytes.fromhex("801C000000"),
                "return caller-owned array through SIO"
            )
            if response != bytes.fromhex("6A209000"):
                raise RuntimeError(
                    f"SIO caller-owned reference return {response.hex().upper()}, "
                    "expected same accessible array 6A209000"
                )
            print("TEST:PASS SIO returns caller-owned byte[] with identity and "
                  "owner restored for client access")
            response = vpcd.exchange(
                connection, bytes.fromhex("801D000000"),
                "compare opaque caller references through SIO"
            )
            if response != bytes.fromhex("6A229000"):
                raise RuntimeError(
                    f"SIO reference identity comparison {response.hex().upper()}, "
                    "expected same foreign and null references 6A229000"
                )
            print("TEST:PASS if_acmp compares opaque references by owner and "
                  "handle; ifnull accepts owner-tagged references")
            response = vpcd.exchange(
                connection, bytes.fromhex("801E000000"),
                "checkcast and instanceof reject foreign private references"
            )
            if response != bytes.fromhex("6A249000"):
                raise RuntimeError(
                    f"foreign cast/instanceof result {response.hex().upper()}, "
                    "expected caught SecurityException 6A249000"
                )
            print("TEST:PASS checkcast and instanceof reject owner-tagged "
                  "foreign references with catchable SecurityException")
            response = vpcd.exchange(
                connection, bytes.fromhex("801F000000"),
                "native Util API rejects an owner-tagged foreign array"
            )
            if response != bytes.fromhex("6A269000"):
                raise RuntimeError(
                    f"foreign array passed to native Util API returned "
                    f"{response.hex().upper()}, expected caught "
                    "SecurityException 6A269000"
                )
            print("TEST:PASS native Util arrayCopy rejects foreign array "
                  "before resolving it in the server heap")
            response = vpcd.exchange(
                connection, bytes.fromhex("8020000000"),
                "retain ordinary foreign object reference in applet state"
            )
            if response != bytes.fromhex("6B439000"):
                raise RuntimeError(
                    f"foreign object retention result {response.hex().upper()}, "
                    "expected applet-owned wildcard Object[] retention to "
                    "succeed while static/instance stores fail (6B439000)"
                )
            response = vpcd.exchange(
                connection, bytes.fromhex("802E000000"),
                "read foreign byte array after source APDU and GC"
            )
            if response != bytes.fromhex("6A2E9000"):
                raise RuntimeError(
                    f"foreign array after GC {response.hex().upper()}, "
                    "expected retained data and identity 6A2E9000"
                )
            print("TEST:PASS foreign array stored in service Object[] survives "
                  "client local/global roots clearing and GC")
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
            first_run_log = log_path.read_text(errors="replace")
            collision_lines = [
                line for line in first_run_log.splitlines()
                if "JCVM:REFERENCE_HANDLE_COLLISION " in line
            ]
            if not collision_lines:
                raise RuntimeError(
                    "SIO identity fixture did not produce an equal compact "
                    "handle owned by different packages"
                )
            print("TEST:PASS equal compact reference handles remain distinct "
                  f"across owners; trace={collision_lines[0].strip()}")
            with log_path.open("ab") as log:
                process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                           stderr=log, env=emulator_env)
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
            response = vpcd.exchange(
                connection, bytes.fromhex("802F000000"),
                "ensure foreign array field restores as null after process restart"
            )
            if response != bytes.fromhex("6A6E9000"):
                raise RuntimeError(
                    "volatile foreign array field after process restart "
                    f"returned {response.hex().upper()}, expected "
                    "null marker 6A6E9000"
                )
            print("TEST:PASS volatile foreign array field restores as null "
                  "after emulator process restart")
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
            response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                     "lookup Shareable proxy before card reset")
            if response != bytes.fromhex("45679000"):
                raise RuntimeError(
                    f"lookup before reset {response.hex().upper()}, "
                    "expected 45679000"
                )
            response = vpcd.exchange(connection, bytes.fromhex("8015000000"),
                                     "retain Shareable proxy before card reset")
            if response != bytes.fromhex("6A109000"):
                raise RuntimeError(
                    f"proxy retention before reset {response.hex().upper()}, "
                    "expected 6A109000"
                )
            print("TEST:PASS fresh Shareable proxy retained before card reset")
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
                                     "opaque reference equality across contexts")
            if response != bytes.fromhex("6A059000"):
                raise RuntimeError(
                    f"opaque reference comparison result {response.hex().upper()}, "
                    "expected 6A059000"
                )
            print("TEST:PASS opaque references pass identity/null comparisons; "
                  "dereference remains blocked; response=6A059000")
            response = vpcd.exchange(connection, bytes.fromhex("8017000000"),
                                     "proxy Object.equals firewall")
            if response != bytes.fromhex("6A099000"):
                raise RuntimeError(
                    f"proxy Object.equals result {response.hex().upper()}, "
                    "expected 6A099000"
                )
            print("TEST:PASS Object.equals on proxy is blocked; "
                  "response=6A099000")
            response = vpcd.exchange(
                connection, bytes.fromhex("8010000000"),
                "refresh Shareable before service DELETE"
            )
            if response != bytes.fromhex("45679000"):
                raise RuntimeError(
                    f"pre-DELETE lookup {response.hex().upper()}, expected 45679000"
                )
            response = vpcd.exchange(
                connection, bytes.fromhex("8015000000"),
                "retain fresh proxy before service DELETE"
            )
            if response != bytes.fromhex("6A109000"):
                raise RuntimeError(
                    f"pre-DELETE proxy retention {response.hex().upper()}, "
                    "expected 6A109000"
                )
            response = vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                                     "SELECT ISD before DELETE")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"SELECT ISD before DELETE {response.hex().upper()}"
                )
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            response = vpcd.delete_object(
                scp, SERVICE_APPLET, False, "DELETE cross-AID service applet"
            )
            if response != bytes.fromhex("9000"):
                raise RuntimeError(
                    f"DELETE service applet {response.hex().upper()}, expected 9000"
                )
            print("TEST:PASS deleted cross-AID service applet through card manager")
            response = vpcd.exchange(connection, select_client,
                                     "reselect client after service DELETE")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"reselect client after DELETE {response.hex().upper()}"
                )
            response = vpcd.exchange(
                connection, bytes.fromhex("8016000000"),
                "reject retained interface proxy after service DELETE"
            )
            if response != bytes.fromhex("6A0A9000"):
                raise RuntimeError(
                    f"interface dispatch after service DELETE "
                    f"{response.hex().upper()}, expected caught SecurityException "
                    "6A0A9000"
                )
            print("TEST:PASS retained interface proxy fails closed after DELETE")
            response = vpcd.exchange(
                connection, bytes.fromhex("8010000000"),
                "fresh lookup after service DELETE"
            )
            if response != bytes.fromhex("6A049000"):
                raise RuntimeError(
                    f"fresh lookup after service DELETE {response.hex().upper()}, "
                    "expected no Shareable service 6A049000"
                )
            print("TEST:PASS fresh lookup returns no service after DELETE")
            response = vpcd.exchange(connection, bytes.fromhex("00A4040000"),
                                     "SELECT ISD before reinstall")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"SELECT ISD before reinstall {response.hex().upper()}"
                )
            reinstall_scp = vpcd.Scp03(connection)
            reinstall_scp.authenticate()
            reinstall_data = (
                vpcd.lv(SERVICE_PACKAGE) + vpcd.lv(SERVICE_APPLET) +
                vpcd.lv(SERVICE_APPLET) + bytes.fromhex("03000000") +
                b"\x00\x00"
            )
            reinstall_response = reinstall_scp.command(
                0xE6, 0x0C, 0x00, reinstall_data,
                "reinstall service applet with same AID",
                expected_statuses=(0x9000,)
            )
            if reinstall_response != bytes.fromhex("9000"):
                raise RuntimeError(
                    f"same-AID reinstall returned "
                    f"{reinstall_response.hex().upper()}, expected 9000"
                )
            print("TEST:PASS reinstalled deleted service applet with same AID")
            select_service = (
                bytes((0x00, 0xA4, 0x04, 0x00, len(SERVICE_APPLET))) +
                SERVICE_APPLET
            )
            response = vpcd.exchange(
                connection, select_service,
                "select service after same-AID reinstall"
            )
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"service SELECT after reinstall {response.hex().upper()}"
                )
            print("TEST:PASS reinstalled service applet is directly selectable")
            response = vpcd.exchange(
                connection, select_client,
                "reselect client after same-AID service reinstall"
            )
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(
                    f"client reselect after service reinstall "
                    f"{response.hex().upper()}"
                )
            response = vpcd.exchange(
                connection, bytes.fromhex("8016000000"),
                "reject old proxy after same-AID service reinstall"
            )
            if response != bytes.fromhex("6A0A9000"):
                raise RuntimeError(
                    f"old proxy after same-AID reinstall "
                    f"{response.hex().upper()}, expected 6A0A9000"
                )
            print("TEST:PASS old proxy remains blocked after same-AID reinstall")
            response = vpcd.exchange(
                connection, bytes.fromhex("8010000000"),
                "fresh lookup after same-AID service reinstall"
            )
            if response != bytes.fromhex("45679000"):
                raise RuntimeError(
                    f"fresh lookup after same-AID reinstall "
                    f"{response.hex().upper()}, expected 45679000"
                )
            print("TEST:PASS fresh proxy dispatch works after same-AID reinstall")
            print("TEST:SUMMARY pass=34 fail=0")
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
