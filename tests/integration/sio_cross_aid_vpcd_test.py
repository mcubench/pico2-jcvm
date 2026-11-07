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
sys.path.insert(0, str(ROOT / "tests/integration"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402


COMPONENTS = (
    "Header", "Directory", "Applet", "Import", "ConstantPool", "Class",
    "Method", "StaticField", "RefLocation", "Export", "Descriptor",
)
SERVICE_PACKAGE = bytes.fromhex("A000000151000003")
SERVICE_APPLET = bytes.fromhex("A00000015100000301")
CLIENT_PACKAGE = bytes.fromhex("A000000151000004")
CLIENT_APPLET = bytes.fromhex("A00000015100000401")
CLIENT_SIBLING_APPLET = bytes.fromhex("A00000015100000402")
UNRELATED_PACKAGE = bytes.fromhex("A000000151000007")
UNRELATED_APPLET = bytes.fromhex("A00000015100000701")
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


def replace_applet_install_with_unlisted_body_offset(
    code: bytes, cap_archive: Path
) -> tuple[bytes, int]:
    """Point the test applet at an interior byte that resembles a method header."""
    components: dict[int, tuple[int, int]] = {}
    offset = 0
    while offset < len(code):
        if len(code) - offset < 3:
            raise RuntimeError("truncated CAP component header")
        tag = code[offset]
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        end = offset + 3 + size
        if end > len(code):
            raise RuntimeError("truncated CAP component payload")
        components[tag] = (offset + 3, size)
        offset = end
    if offset != len(code) or 3 not in components or 7 not in components:
        raise RuntimeError("test CAP lacks complete Applet/Method components")

    applet_start, applet_size = components[3]
    method_start, method_size = components[7]
    if applet_size < 1 or method_size < 1:
        raise RuntimeError("empty Applet or Method component")
    aid_length = code[applet_start + 1]
    install_offset_index = applet_start + 2 + aid_length
    if 1 + aid_length + 2 > applet_size:
        raise RuntimeError("truncated Applet entry")
    install_offset = int.from_bytes(
        code[install_offset_index:install_offset_index + 2], "big"
    )

    method_component = code[method_start:method_start + method_size]
    handler_bytes = method_component[0] * 8
    methods = method_component[1 + handler_bytes:]
    install_index = install_offset - handler_bytes - 1
    if install_index < 0 or install_index >= len(methods):
        raise RuntimeError("install offset lies outside Method component")
    header_size = 4 if methods[install_index] & 0x80 else 2
    body_start = install_index + header_size

    with ZipFile(cap_archive) as archive:
        descriptor_name = next(
            name for name in archive.namelist()
            if name.endswith("/Descriptor.cap")
        )
        descriptor = archive.read(descriptor_name)
    if len(descriptor) < 4 or descriptor[0] != 11:
        raise RuntimeError("test archive has invalid Descriptor component")
    descriptor_size = int.from_bytes(descriptor[1:3], "big")
    payload = descriptor[3:]
    if descriptor_size != len(payload) or not payload:
        raise RuntimeError("test Descriptor component size mismatch")
    cursor = 1
    install_length = None
    method_offsets: set[int] = set()
    for _ in range(payload[0]):
        if len(payload) - cursor < 9:
            raise RuntimeError("truncated Descriptor class record")
        interfaces = payload[cursor + 4]
        fields = int.from_bytes(payload[cursor + 5:cursor + 7], "big")
        method_count = int.from_bytes(payload[cursor + 7:cursor + 9], "big")
        records = cursor + 9 + interfaces * 2 + fields * 7
        record_bytes = method_count * 12
        if records > len(payload) or record_bytes > len(payload) - records:
            raise RuntimeError("truncated Descriptor method table")
        for index in range(method_count):
            record = records + index * 12
            entry_offset = int.from_bytes(payload[record + 2:record + 4], "big")
            bytecode_count = int.from_bytes(payload[record + 6:record + 8], "big")
            method_offsets.add(entry_offset)
            if entry_offset == install_offset:
                install_length = bytecode_count
        cursor = records + record_bytes
    if cursor > len(payload) or install_length is None:
        raise RuntimeError("Descriptor has no Applet install method record")

    # Descriptor is used only to construct this test mutation. The CAP sent to
    # the VM omits it, matching the production GPPro LOAD path.
    candidate_offset = None
    body_end = min(body_start + install_length, len(methods) - 2)
    for body_index in range(body_start, body_end):
        component_offset = handler_bytes + 1 + body_index
        if methods[body_index] == 0 and component_offset not in method_offsets:
            candidate_offset = component_offset
            break
    if candidate_offset is None:
        raise RuntimeError("install body has no unlisted header-looking byte")

    mutated = bytearray(code)
    mutated[install_offset_index:install_offset_index + 2] = (
        candidate_offset.to_bytes(2, "big")
    )
    return bytes(mutated), candidate_offset


def zero_first_class_method_offset(code: bytes, cap_archive: Path) -> bytes:
    """Corrupt a Class method-table entry without changing component lengths."""
    with ZipFile(cap_archive) as archive:
        header_name = next(
            name for name in archive.namelist() if name.endswith("/Header.cap")
        )
        header = archive.read(header_name)
    if len(header) < 9:
        raise RuntimeError("truncated CAP Header component")
    minor_version = header[7]

    mutated = bytearray(code)
    offset = 0
    while offset < len(mutated):
        if len(mutated) - offset < 3:
            raise RuntimeError("truncated CAP component header")
        tag = mutated[offset]
        size = int.from_bytes(mutated[offset + 1:offset + 3], "big")
        payload = offset + 3
        end = payload + size
        if end > len(mutated):
            raise RuntimeError("truncated CAP component payload")
        if tag == 6:
            data = payload
            if minor_version >= 2:
                if size < 2:
                    raise RuntimeError("truncated Class signature-pool prefix")
                signature_length = int.from_bytes(mutated[data:data + 2], "big")
                if signature_length > size - 2:
                    raise RuntimeError("Class signature pool exceeds component")
                data += 2 + signature_length
            class_end = end

            # Skip interface_info records; method tables begin in class_info.
            while data < class_end and mutated[data] & 0x80:
                flags = mutated[data] >> 4
                superinterfaces = mutated[data] & 0x0f
                record_size = 1 + superinterfaces * 2
                name_offset = data + record_size
                if flags & 0x02:
                    if name_offset >= class_end:
                        raise RuntimeError("truncated remote interface name")
                    record_size += 1 + mutated[name_offset]
                if record_size > class_end - data:
                    raise RuntimeError("interface_info exceeds Class component")
                data += record_size

            while data < class_end:
                if class_end - data < 10 or mutated[data] & 0x80:
                    raise RuntimeError("invalid Class class_info prefix")
                interface_count = mutated[data] & 0x0f
                public_count = mutated[data + 7]
                package_count = mutated[data + 9]
                method_count = public_count + package_count
                table = data + 10
                table_bytes = method_count * 2
                if table_bytes > class_end - table:
                    raise RuntimeError("Class method table exceeds component")
                if method_count:
                    entry = table
                    if mutated[entry:entry + 2] == b"\x00\x00":
                        raise RuntimeError("first Class method offset is already zero")
                    mutated[entry:entry + 2] = b"\x00\x00"
                    return bytes(mutated)

                cursor = table + table_bytes
                for _ in range(interface_count):
                    if class_end - cursor < 3:
                        raise RuntimeError("truncated class interface map")
                    record_size = 3 + mutated[cursor + 2]
                    if record_size > class_end - cursor:
                        raise RuntimeError("class interface map exceeds component")
                    cursor += record_size
                flags = mutated[data] >> 4
                if flags & 0x02:
                    # The supported fixture has no remote class records; do
                    # not guess their variable-length extent in this mutator.
                    raise RuntimeError("remote class records need a dedicated fixture")
                if cursor <= data or cursor > class_end:
                    raise RuntimeError("invalid Class class_info extent")
                data = cursor
        offset = end
    raise RuntimeError("test CAP has no Class method-table entry")


def remove_all_class_method_tables(code: bytes, cap_archive: Path) -> bytes:
    """Remove complete public/package method-offset tables from Class.cap."""
    with ZipFile(cap_archive) as archive:
        header_name = next(
            name for name in archive.namelist() if name.endswith("/Header.cap")
        )
        header = archive.read(header_name)
    if len(header) < 9:
        raise RuntimeError("truncated CAP Header component")
    minor_version = header[7]

    components: list[bytearray] = []
    offset = 0
    while offset < len(code):
        if len(code) - offset < 3:
            raise RuntimeError("truncated CAP component header")
        tag = code[offset]
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        end = offset + 3 + size
        if end > len(code):
            raise RuntimeError("truncated CAP component payload")
        components.append(bytearray(code[offset:end]))
        offset = end
    if offset != len(code):
        raise RuntimeError("unaccounted CAP code bytes")

    class_index = next((i for i, component in enumerate(components)
                        if component[0] == 6), None)
    directory_index = next((i for i, component in enumerate(components)
                            if component[0] == 2), None)
    if class_index is None or directory_index is None:
        raise RuntimeError("test CAP lacks Class or Directory component")

    class_component = components[class_index]
    class_data = class_component[3:]
    output = bytearray()
    cursor = 0
    if minor_version >= 2:
        if len(class_data) < 2:
            raise RuntimeError("truncated Class signature-pool prefix")
        signature_length = int.from_bytes(class_data[:2], "big")
        if signature_length > len(class_data) - 2:
            raise RuntimeError("Class signature pool exceeds component")
        output.extend(class_data[:2 + signature_length])
        cursor = 2 + signature_length

    while cursor < len(class_data) and class_data[cursor] & 0x80:
        flags = class_data[cursor] >> 4
        superinterfaces = class_data[cursor] & 0x0F
        name_offset = cursor + 1 + superinterfaces * 2
        record_size = 1 + superinterfaces * 2
        if flags & 0x02:
            if name_offset >= len(class_data):
                raise RuntimeError("truncated remote interface name")
            record_size += 1 + class_data[name_offset]
        if record_size > len(class_data) - cursor:
            raise RuntimeError("interface_info exceeds Class component")
        output.extend(class_data[cursor:cursor + record_size])
        cursor += record_size

    class_count = 0
    while cursor < len(class_data):
        if len(class_data) - cursor < 10 or class_data[cursor] & 0x80:
            raise RuntimeError("invalid Class class_info prefix")
        flags = class_data[cursor] >> 4
        if flags & 0x02:
            raise RuntimeError("remote class records need a dedicated fixture")
        interface_count = class_data[cursor] & 0x0F
        public_count = class_data[cursor + 7]
        package_count = class_data[cursor + 9]
        method_count = public_count + package_count
        table_bytes = method_count * 2
        if table_bytes > len(class_data) - cursor - 10:
            raise RuntimeError("Class method table exceeds component")

        prefix = bytearray(class_data[cursor:cursor + 10])
        prefix[7] = 0
        prefix[9] = 0
        output.extend(prefix)
        map_cursor = cursor + 10 + table_bytes
        for _ in range(interface_count):
            if len(class_data) - map_cursor < 3:
                raise RuntimeError("truncated class interface map")
            record_size = 3 + class_data[map_cursor + 2]
            if record_size > len(class_data) - map_cursor:
                raise RuntimeError("class interface map exceeds component")
            output.extend(class_data[map_cursor:map_cursor + record_size])
            map_cursor += record_size
        if map_cursor <= cursor or map_cursor > len(class_data):
            raise RuntimeError("invalid Class class_info extent")
        cursor = map_cursor
        class_count += 1

    if class_count == 0:
        raise RuntimeError("Class component has no class_info records")
    old_size = int.from_bytes(class_component[1:3], "big")
    if old_size != len(class_data) or len(output) >= old_size:
        raise RuntimeError("Class component size mismatch or no table bytes removed")
    class_component[1:3] = len(output).to_bytes(2, "big")
    components[class_index] = class_component[:3] + output

    directory = components[directory_index]
    directory_size_offset = 3 + 5 * 2
    if len(directory) < directory_size_offset + 2:
        raise RuntimeError("Directory lacks Class component size field")
    declared = int.from_bytes(
        directory[directory_size_offset:directory_size_offset + 2], "big"
    )
    if declared != old_size:
        raise RuntimeError("Directory Class size does not match component")
    directory[directory_size_offset:directory_size_offset + 2] = (
        len(output).to_bytes(2, "big")
    )
    return b"".join(bytes(component) for component in components)


def zero_first_internal_static_methodref_offset(code: bytes) -> bytes:
    """Zero an internal StaticMethodref offset in the ConstantPool component."""
    mutated = bytearray(code)
    offset = 0
    while offset < len(mutated):
        if len(mutated) - offset < 3:
            raise RuntimeError("truncated CAP component header")
        tag = mutated[offset]
        size = int.from_bytes(mutated[offset + 1:offset + 3], "big")
        payload = offset + 3
        end = payload + size
        if end > len(mutated):
            raise RuntimeError("truncated CAP component payload")
        if tag == 5:
            if size < 2:
                raise RuntimeError("truncated ConstantPool count")
            entry_count = int.from_bytes(mutated[payload:payload + 2], "big")
            if entry_count > (size - 2) // 4 or 2 + entry_count * 4 != size:
                raise RuntimeError("ConstantPool entry count/size mismatch")
            for index in range(entry_count):
                entry = payload + 2 + index * 4
                if mutated[entry] == 6 and mutated[entry + 1] & 0x80 == 0:
                    if mutated[entry + 2:entry + 4] == b"\x00\x00":
                        raise RuntimeError("internal StaticMethodref is already zero")
                    mutated[entry + 2:entry + 4] = b"\x00\x00"
                    return bytes(mutated)
            raise RuntimeError("ConstantPool has no internal StaticMethodref")
        offset = end
    raise RuntimeError("test CAP has no ConstantPool component")


def remove_all_constant_pool_entries(code: bytes) -> bytes:
    """Keep a valid empty ConstantPool component in a generated test CAP."""
    components: list[bytearray] = []
    offset = 0
    while offset < len(code):
        if len(code) - offset < 3:
            raise RuntimeError("truncated CAP component header")
        tag = code[offset]
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        end = offset + 3 + size
        if end > len(code):
            raise RuntimeError("truncated CAP component payload")
        components.append(bytearray(code[offset:end]))
        offset = end

    cp_index = next((i for i, component in enumerate(components)
                     if component[0] == 5), None)
    directory_index = next((i for i, component in enumerate(components)
                            if component[0] == 2), None)
    if cp_index is None or directory_index is None:
        raise RuntimeError("test CAP lacks ConstantPool or Directory component")
    cp_component = components[cp_index]
    old_size = int.from_bytes(cp_component[1:3], "big")
    if old_size < 2 or (old_size - 2) % 4 != 0:
        raise RuntimeError("ConstantPool count/size mismatch")
    count = int.from_bytes(cp_component[3:5], "big")
    if old_size != 2 + count * 4 or count == 0:
        raise RuntimeError("ConstantPool is empty or count/size mismatch")
    cp_component[1:3] = (2).to_bytes(2, "big")
    components[cp_index] = cp_component[:5]

    directory = components[directory_index]
    directory_size_offset = 3 + 4 * 2
    if len(directory) < directory_size_offset + 2:
        raise RuntimeError("Directory lacks ConstantPool component size field")
    declared = int.from_bytes(
        directory[directory_size_offset:directory_size_offset + 2], "big"
    )
    if declared != old_size:
        raise RuntimeError("Directory ConstantPool size does not match component")
    directory[directory_size_offset:directory_size_offset + 2] = (2).to_bytes(2, "big")
    return b"".join(bytes(component) for component in components)


def inject_zero_exported_static_method_offset(code: bytes) -> bytes:
    """Add one malformed Export method entry to the generated test CAP."""
    components: list[bytearray] = []
    offset = 0
    while offset < len(code):
        if len(code) - offset < 3:
            raise RuntimeError("truncated CAP component header")
        tag = code[offset]
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        payload = offset + 3
        end = payload + size
        if end > len(code):
            raise RuntimeError("truncated CAP component payload")
        components.append(bytearray(code[offset:end]))
        offset = end
    if offset != len(code):
        raise RuntimeError("unaccounted CAP code bytes")

    export_index = next((i for i, component in enumerate(components)
                         if component[0] == 10), None)
    directory_index = next((i for i, component in enumerate(components)
                            if component[0] == 2), None)
    if export_index is None or directory_index is None:
        raise RuntimeError("test CAP lacks Export or Directory component")

    export = components[export_index]
    export_size = int.from_bytes(export[1:3], "big")
    if export_size < 1 or export[3] == 0:
        raise RuntimeError("Export component has no class record")
    cursor = 4
    if len(export) - cursor < 4:
        raise RuntimeError("truncated first Export class record")
    field_count = export[cursor + 2]
    method_count = export[cursor + 3]
    record_size = 4 + 2 * (field_count + method_count)
    if record_size > len(export) - cursor:
        raise RuntimeError("first Export class record exceeds component")

    if method_count:
        method_offset_index = cursor + 4 + 2 * field_count
        if export[method_offset_index:method_offset_index + 2] == b"\x00\x00":
            raise RuntimeError("first exported method offset is already zero")
        export[method_offset_index:method_offset_index + 2] = b"\x00\x00"
    else:
        # Expand the first class record by one u2 method offset and update its
        # component length. Directory component_sizes[9] describes tag 10.
        export[cursor + 3] = 1
        insert_at = cursor + record_size
        export[insert_at:insert_at] = b"\x00\x00"
        export[1:3] = (export_size + 2).to_bytes(2, "big")
        directory = components[directory_index]
        directory_size = int.from_bytes(directory[1:3], "big")
        directory_size_offset = 3 + 9 * 2
        if directory_size < directory_size_offset + 2:
            raise RuntimeError("Directory lacks Export component size field")
        declared = int.from_bytes(
            directory[directory_size_offset:directory_size_offset + 2], "big"
        )
        if declared != export_size:
            raise RuntimeError("Directory Export size does not match component")
        directory[directory_size_offset:directory_size_offset + 2] = (
            (export_size + 2).to_bytes(2, "big")
        )
    return b"".join(bytes(component) for component in components)


def remove_empty_export_class_records(code: bytes) -> bytes:
    """Drop Export class records that have no fields or methods."""
    components: list[bytearray] = []
    offset = 0
    while offset < len(code):
        if len(code) - offset < 3:
            raise RuntimeError("truncated CAP component header")
        tag = code[offset]
        size = int.from_bytes(code[offset + 1:offset + 3], "big")
        end = offset + 3 + size
        if end > len(code):
            raise RuntimeError("truncated CAP component payload")
        components.append(bytearray(code[offset:end]))
        offset = end
    if offset != len(code):
        raise RuntimeError("unaccounted CAP code bytes")

    export_index = next((i for i, component in enumerate(components)
                         if component[0] == 10), None)
    directory_index = next((i for i, component in enumerate(components)
                            if component[0] == 2), None)
    if export_index is None or directory_index is None:
        raise RuntimeError("test CAP lacks Export or Directory component")
    export = components[export_index]
    if len(export) < 4 or export[3] == 0:
        raise RuntimeError("Export component has no class records")
    cursor = 4
    for _ in range(export[3]):
        if len(export) - cursor < 4:
            raise RuntimeError("truncated Export class record")
        fields = export[cursor + 2]
        methods = export[cursor + 3]
        record_size = 4 + 2 * (fields + methods)
        if record_size > len(export) - cursor:
            raise RuntimeError("Export class record exceeds component")
        if fields or methods:
            raise RuntimeError("cannot remove a populated Export class record")
        cursor += record_size
    if cursor != len(export):
        raise RuntimeError("Export component has trailing class-table data")

    old_size = int.from_bytes(export[1:3], "big")
    if old_size != len(export) - 3:
        raise RuntimeError("Export component size mismatch")
    components[export_index] = bytearray(b"\x0A\x00\x01\x00")
    directory = components[directory_index]
    directory_size_offset = 3 + 9 * 2
    if len(directory) < directory_size_offset + 2:
        raise RuntimeError("Directory lacks Export component size field")
    declared = int.from_bytes(
        directory[directory_size_offset:directory_size_offset + 2], "big"
    )
    if declared != old_size:
        raise RuntimeError("Directory Export size does not match component")
    directory[directory_size_offset:directory_size_offset + 2] = b"\x00\x01"
    return b"".join(bytes(component) for component in components)


def restart_vpcd_after_disconnect(
        connection: socket.socket,
        process: subprocess.Popen[bytes],
        listener: socket.socket,
        command: list[str],
        emulator_env: dict[str, str],
        log_path: Path,
        phase: str) -> tuple[subprocess.Popen[bytes], socket.socket]:
    connection.close()
    process.wait(timeout=SHUTDOWN_TIMEOUT)
    if process.returncode != 0:
        raise RuntimeError(
            f"emulator exited with {process.returncode} before {phase}"
        )
    restarted_env = emulator_env.copy()
    restarted_env.pop("CHOUPI_TEST_FAIL_DELETE_PACKAGE_CLEANUP_ONCE", None)
    restarted_env.pop("CHOUPI_TEST_FAIL_DELETE_INSTANCE_CLEANUP_ONCE", None)
    with log_path.open("ab") as log:
        process = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                   stderr=log, env=restarted_env)
    connection, _ = vpcd.accept_vpcd(
        listener, process, CONNECT_TIMEOUT, log_path, phase
    )
    connection.settimeout(APDU_TIMEOUT)
    vpcd.send_message(connection, b"\x01")
    vpcd.send_message(connection, b"\x04")
    atr = vpcd.receive_message(connection)
    if atr != bytes.fromhex("3B800181"):
        raise RuntimeError(f"{phase} ATR {atr.hex().upper()}")
    return process, connection


def main() -> int:
    vpcd.ensure_algtest_load_code(ROOT)
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
    unrelated = None
    if os.environ.get("SIO_CROSS_AID_TEST_PACKAGE_DELETE") == "1":
        subprocess.run([str(ROOT / "tools/build-unrelated-client-fixture")],
                       cwd=ROOT, check=True, timeout=120)
        unrelated = cap_components(
            ROOT / "build/unrelated-client-fixture/cap/unrelated/javacard/unrelated.cap"
        )
    if contract[2] or service[1:] != (SERVICE_PACKAGE, [SERVICE_APPLET]) or \
            client[1:] != (CLIENT_PACKAGE, [CLIENT_APPLET]):
        raise RuntimeError("fixture package/applet AID validation failed")
    if unrelated is not None and unrelated[1:] != (
            UNRELATED_PACKAGE, [UNRELATED_APPLET]):
        raise RuntimeError("unrelated-client fixture AID validation failed")

    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/tests/jcalgtest/algtest-v184-load-code.bin"
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
            if os.environ.get(
                "SIO_CROSS_AID_TEST_DELETE_IMPORTED_UNINSTALLED"
            ) == "1":
                vpcd.load_package(scp, service[0], service[1])
                response = vpcd.delete_object(
                    scp, contract[1], True,
                    "DELETE library imported by loaded, uninstalled service",
                    expected_statuses=(0x9000, 0x6985),
                )
                if response[-2:] != b"\x69\x85":
                    raise RuntimeError(
                        "DELETE of a library imported by a loaded service "
                        f"returned {response[-2:].hex().upper()}, expected 6985"
                    )
                vpcd.delete_object(
                    scp, service[1], True,
                    "DELETE uninstalled importing service package",
                )
                vpcd.delete_object(
                    scp, contract[1], True,
                    "DELETE library after uninstalled importer removal",
                )
                print("TEST:PASS loaded but uninstalled importer blocks library "
                      "DELETE; removing it permits library DELETE")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_EMPTY_EXPORT_TABLE") == "1":
                empty_export_code = remove_empty_export_class_records(service[0])
                vpcd.load_package(scp, empty_export_code, service[1])
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0]
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "INSTALL with an empty Export class table returned "
                        f"{response[-2:].hex().upper()}, expected 9000"
                    )
                print("TEST:PASS loaded and installed service CAP with an "
                      "empty Export class table")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_EMPTY_CLASS_TABLES") == "1":
                empty_class_code = remove_all_class_method_tables(
                    service[0],
                    build / "service-cap/sio/service/javacard/service.cap",
                )
                vpcd.load_package(scp, empty_class_code, service[1])
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6F00,),
                )
                if response[-2:] != bytes.fromhex("6F00"):
                    raise RuntimeError(
                        "INSTALL with all Class method tables omitted returned "
                        f"{response[-2:].hex().upper()}, expected fail-closed 6F00"
                    )
                print("TEST:PASS INSTALL rejects a service CAP whose complete "
                      "Class method tables are omitted (6F00)")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_EMPTY_CONSTANT_POOL") == "1":
                empty_cp_code = remove_all_constant_pool_entries(service[0])
                vpcd.load_package(scp, empty_cp_code, service[1])
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6F00,),
                )
                if response[-2:] != bytes.fromhex("6F00"):
                    raise RuntimeError(
                        "INSTALL with an empty ConstantPool returned "
                        f"{response[-2:].hex().upper()}, expected fail-closed 6F00"
                    )
                vpcd.load_package(scp, client[0], client[1])
                response = vpcd.install_applet(
                    scp, client[1], client[2][0], client[2][0]
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "valid client INSTALL after empty ConstantPool rejection "
                        f"returned {response[-2:].hex().upper()}, expected 9000"
                    )
                print("TEST:PASS INSTALL rejects a service CAP whose ConstantPool "
                      "entries are omitted (6F00)")
                print("TEST:PASS valid client package installs after malformed "
                      "service CAP rejection")
                print("TEST:PASS card manager remains usable after failed INSTALL")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_BAD_EXPORT_METHOD_OFFSET") == "1":
                bad_service_code = inject_zero_exported_static_method_offset(
                    service[0]
                )
                vpcd.load_package(scp, bad_service_code, service[1])
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6A80,),
                )
                if response[-2:] != bytes.fromhex("6A80"):
                    raise RuntimeError(
                        "zero Export static-method offset INSTALL returned "
                        f"{response[-2:].hex().upper()}, expected 6A80"
                    )
                print("TEST:PASS INSTALL rejected a zero Export static-method "
                      "offset before applet execution")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_BAD_STATIC_METHODREF_OFFSET") == "1":
                bad_service_code = zero_first_internal_static_methodref_offset(
                    service[0]
                )
                try:
                    vpcd.load_package(scp, bad_service_code, service[1])
                except RuntimeError as exc:
                    if not any(status in str(exc) for status in ("6A80", "6F00")):
                        raise
                    print("TEST:PASS LOAD rejected a zero internal "
                          "StaticMethodref offset")
                    print("TEST:SUMMARY pass=1 fail=0")
                    return 0
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6F00,),
                )
                if response[-2:] != bytes.fromhex("6F00"):
                    raise RuntimeError(
                        "zero StaticMethodref offset INSTALL returned "
                        f"{response[-2:].hex().upper()}, expected 6F00"
                    )
                print("TEST:PASS INSTALL rejected a zero internal "
                      "StaticMethodref offset during stage-9 package validation")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_BAD_CLASS_METHOD_OFFSET") == "1":
                bad_service_code = zero_first_class_method_offset(
                    service[0],
                    build / "service-cap/sio/service/javacard/service.cap",
                )
                try:
                    vpcd.load_package(scp, bad_service_code, service[1])
                except RuntimeError as exc:
                    if not any(status in str(exc) for status in ("6A80", "6F00")):
                        raise
                    print("TEST:PASS LOAD rejected a zero Class method offset")
                    print("TEST:SUMMARY pass=1 fail=0")
                    return 0
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6A80,),
                )
                if response[-2:] != bytes.fromhex("6A80"):
                    raise RuntimeError(
                        "zero Class method offset INSTALL returned "
                        f"{response[-2:].hex().upper()}, expected 6A80"
                    )
                print("TEST:PASS INSTALL rejected zero Class method offset")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_UNLISTED_INSTALL_OFFSET") == "1":
                bad_service_code, bad_offset = (
                    replace_applet_install_with_unlisted_body_offset(
                        service[0],
                        build / "service-cap/sio/service/javacard/service.cap",
                    )
                )
                vpcd.load_package(scp, bad_service_code, service[1])
                response = vpcd.install_applet(
                    scp, service[1], service[2][0], service[2][0],
                    expected_statuses=(0x6A80,),
                )
                if response[-2:] != bytes.fromhex("6A80"):
                    raise RuntimeError(
                        "unlisted Applet install offset returned "
                        f"{response[-2:].hex().upper()}, expected 6A80"
                    )
                print("TEST:PASS Descriptor-free INSTALL rejected unlisted "
                      f"method-body offset 0x{bad_offset:04X}")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
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
            if os.environ.get(
                    "SIO_CROSS_AID_TEST_UNMATERIALIZED_STATIC_ALIAS_DELETE") == "1":
                sibling_install = vpcd.install_applet(
                    scp, client[1], client[2][0], CLIENT_SIBLING_APPLET
                )
                if sibling_install[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "static-alias sibling INSTALL returned "
                        f"{sibling_install[-2:].hex().upper()}, expected 9000"
                    )
                select_target = bytes((0x00, 0xA4, 0x04, 0x00,
                                       len(CLIENT_APPLET))) + CLIENT_APPLET
                response = vpcd.exchange(
                    connection, select_target,
                    "SELECT target before creating package-static alias",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("target SELECT before static alias failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8042000000"),
                    "persist package-static alias to target applet object",
                )
                if response != bytes.fromhex("6A729000"):
                    raise RuntimeError(
                        "static alias setup returned "
                        f"{response.hex().upper()}, expected 6A729000"
                    )
                select_sibling = bytes((0x00, 0xA4, 0x04, 0x00,
                                        len(CLIENT_SIBLING_APPLET))) + CLIENT_SIBLING_APPLET
                response = vpcd.exchange(
                    connection, select_sibling,
                    "SELECT sibling to move alias into its instance field",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("sibling SELECT before alias transfer failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8044000000"),
                    "move target reference from static into sibling instance field",
                )
                if response != bytes.fromhex("6A749000"):
                    raise RuntimeError(
                        "sibling alias transfer returned "
                        f"{response.hex().upper()}, expected 6A749000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before cold static-alias check",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("SELECT ISD before restart failed")
                process, connection = restart_vpcd_after_disconnect(
                    connection, process, listener, command, emulator_env,
                    log_path, "unmaterialized static-alias DELETE restart",
                )
                print("TEST:PASS emulator restarted with an unselected sibling "
                      "instance-field alias")
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD after cold static-alias restart",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("SELECT ISD after restart failed")
                delete_scp = vpcd.Scp03(connection)
                delete_scp.authenticate()
                response = vpcd.delete_object(
                    delete_scp, CLIENT_APPLET, False,
                    "DELETE target with an unmaterialized sibling instance alias",
                    expected_statuses=(0x6985,),
                )
                if response[-2:] != b"\x69\x85":
                    raise RuntimeError(
                        "DELETE with unmaterialized instance alias returned "
                        f"{response[-2:].hex().upper()}, expected 6985"
                    )
                print("TEST:PASS unselected sibling instance alias blocks applet "
                      "DELETE before either applet has been selected")
                response = vpcd.exchange(
                    connection, select_sibling,
                      "SELECT sibling to clear persistent instance alias",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("sibling SELECT after rejected DELETE failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8045000000"),
                    "clear sibling instance alias",
                )
                if response != bytes.fromhex("6A759000"):
                    raise RuntimeError(
                        "clearing sibling instance alias returned "
                        f"{response.hex().upper()}, expected 6A759000"
                    )
                response = vpcd.exchange(
                    connection, select_target,
                    "SELECT target to prepare sibling persistent array alias",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("target SELECT before array alias failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8042000000"),
                    "persist target object for sibling array alias",
                )
                if response != bytes.fromhex("6A729000"):
                    raise RuntimeError("target object setup for array alias failed")
                response = vpcd.exchange(
                    connection, select_sibling,
                    "SELECT sibling to create persistent array alias",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("sibling SELECT before array alias failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8046000000"),
                    "store target reference in sibling persistent array",
                )
                if response != bytes.fromhex("6A769000"):
                    raise RuntimeError(
                        "persistent array alias setup returned "
                        f"{response.hex().upper()}, expected 6A769000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before cold persistent-array alias check",
                )
                process, connection = restart_vpcd_after_disconnect(
                    connection, process, listener, command, emulator_env,
                    log_path, "unmaterialized persistent-array alias restart",
                )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD after persistent-array alias restart",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("SELECT ISD after array restart failed")
                delete_scp = vpcd.Scp03(connection)
                delete_scp.authenticate()
                response = vpcd.delete_object(
                    delete_scp, CLIENT_APPLET, False,
                    "DELETE target retained in unmaterialized sibling array",
                    expected_statuses=(0x6985,),
                )
                if response[-2:] != b"\x69\x85":
                    raise RuntimeError(
                        "DELETE with sibling array alias returned "
                        f"{response[-2:].hex().upper()}, expected 6985"
                    )
                print("TEST:PASS unselected sibling persistent array alias blocks "
                      "applet DELETE after emulator restart")
                response = vpcd.exchange(
                    connection, select_sibling,
                    "SELECT sibling to clear persistent array alias",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("sibling SELECT to clear array failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8047000000"),
                    "clear sibling persistent array alias",
                )
                if response != bytes.fromhex("6A779000"):
                    raise RuntimeError(
                        "clearing sibling persistent array returned "
                        f"{response.hex().upper()}, expected 6A779000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before retrying persistent-array DELETE",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("SELECT ISD before array DELETE failed")
                delete_scp = vpcd.Scp03(connection)
                delete_scp.authenticate()
                response = vpcd.delete_object(
                    delete_scp, CLIENT_APPLET, False,
                    "DELETE target after clearing sibling array alias",
                    expected_statuses=(0x9000,),
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "DELETE after clearing sibling array alias returned "
                        f"{response[-2:].hex().upper()}, expected 9000"
                    )
                response = vpcd.exchange(
                    connection, select_sibling,
                    "SELECT surviving sibling after target DELETE",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("sibling SELECT after target DELETE failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8041000000"),
                    "verify sibling and service after target DELETE",
                )
                if response != bytes.fromhex("6A6E9000"):
                    raise RuntimeError(
                        "sibling check after DELETE returned "
                        f"{response.hex().upper()}, expected 6A6E9000"
                    )
                print("TEST:PASS clearing sibling array alias permits DELETE and "
                      "leaves the sibling usable")
                print("TEST:SUMMARY pass=2 fail=0")
                return 0
            if os.environ.get("SIO_CROSS_AID_TEST_INSTANCE_DELETE_FOREIGN_ROOT") == "1":
                response = vpcd.install_applet(
                    scp, client[1], client[2][0], CLIENT_SIBLING_APPLET
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "sibling client INSTALL returned "
                        f"{response.hex().upper()}, expected 9000"
                    )
                select_source = bytes((0x00, 0xA4, 0x04, 0x00,
                                       len(CLIENT_APPLET))) + CLIENT_APPLET
                response = vpcd.exchange(
                    connection, select_source, "SELECT source client applet"
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("source client SELECT failed")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8020000000"),
                    "retain source-owned object in service array",
                )
                if response != bytes.fromhex("6B439000"):
                    raise RuntimeError(
                        "source object retention returned "
                        f"{response.hex().upper()}, expected 6B439000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before applet-instance DELETE",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("SELECT ISD before DELETE failed")
                delete_scp = vpcd.Scp03(connection)
                delete_scp.authenticate()
                response = vpcd.delete_object(
                    delete_scp, CLIENT_APPLET, False,
                    "DELETE applet with retained foreign object",
                    expected_statuses=(0x6985,),
                )
                if response[-2:] != b"\x69\x85":
                    raise RuntimeError(
                        "DELETE with retained foreign object returned "
                        f"{response[-2:].hex().upper()}, expected 6985"
                    )
                print("TEST:PASS DELETE is rejected while another applet "
                      "retains an object owned by the target instance")
                select_sibling = bytes((0x00, 0xA4, 0x04, 0x00,
                                        len(CLIENT_SIBLING_APPLET))) + CLIENT_SIBLING_APPLET
                response = vpcd.exchange(
                    connection, select_sibling,
                    "SELECT sibling after rejected DELETE",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "sibling SELECT after rejected DELETE failed: "
                        f"{response.hex().upper()}"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("8040000000"),
                    "clear retained foreign edge from sibling",
                )
                if response != bytes.fromhex("6A709000"):
                    raise RuntimeError(
                        f"clear retained edge returned {response.hex().upper()}"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before retrying DELETE",
                )
                delete_scp = vpcd.Scp03(connection)
                delete_scp.authenticate()
                response = vpcd.delete_object(
                    delete_scp, CLIENT_APPLET, False,
                    "DELETE applet after clearing retained edge",
                    expected_statuses=(0x9000,),
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "DELETE after clearing edge returned "
                        f"{response[-2:].hex().upper()}, expected 9000"
                    )
                response = vpcd.exchange(
                    connection, select_sibling,
                    "SELECT sibling after source DELETE",
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError("sibling was lost after source DELETE")
                response = vpcd.exchange(
                    connection, bytes.fromhex("8041000000"),
                    "verify sibling service remains usable",
                )
                if response != bytes.fromhex("6A6E9000"):
                    raise RuntimeError(
                        "sibling service check returned "
                        f"{response.hex().upper()}, expected 6A6E9000"
                    )
                print("TEST:PASS clearing the edge permits DELETE and leaves "
                      "the sibling applet usable")
                print("TEST:SUMMARY pass=2 fail=0")
                return 0
            if unrelated is not None:
                vpcd.load_package(scp, unrelated[0], unrelated[1])
                response = vpcd.install_applet(
                    scp, unrelated[1], unrelated[2][0], unrelated[2][0]
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "unrelated applet INSTALL returned "
                        f"{response.hex().upper()}, expected 9000"
                    )
                print("TEST:PASS installed an unrelated third-package applet")

            select_client = bytes((0x00, 0xA4, 0x04, 0x00, len(CLIENT_APPLET))) + CLIENT_APPLET
            select_unrelated = (bytes((0x00, 0xA4, 0x04, 0x00,
                                      len(UNRELATED_APPLET))) + UNRELATED_APPLET)

            if os.environ.get("SIO_CROSS_AID_TEST_DELETE_IMPORTED_PACKAGE") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before imported-package DELETE"
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "SELECT ISD before imported-package DELETE returned "
                        f"{response.hex().upper()}"
                    )
                dependency_scp = vpcd.Scp03(connection)
                dependency_scp.authenticate()
                response = vpcd.delete_object(
                    dependency_scp, contract[1], True,
                    "DELETE library package imported by installed service/client",
                    expected_statuses=(0x9000, 0x6985),
                )
                if response[-2:] != b"\x69\x85":
                    raise RuntimeError(
                        "DELETE of a package imported by installed packages "
                        f"returned {response[-2:].hex().upper()}, expected 6985"
                    )
                selected = vpcd.exchange(
                    connection, select_client,
                    "SELECT client after rejected imported-package DELETE"
                )
                if selected[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "client SELECT after rejected package DELETE returned "
                        f"{selected.hex().upper()}"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("8010000000"),
                    "invoke service after rejected imported-package DELETE"
                )
                if response != bytes.fromhex("45679000"):
                    raise RuntimeError(
                        "SIO after rejected package DELETE returned "
                        f"{response.hex().upper()}, expected 45679000"
                    )
                print("TEST:PASS package DELETE was rejected with 6985 while "
                      "installed CAPs import it")
                print("TEST:PASS dependent client and service remain usable")
                manager = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before deleting package importers"
                )
                if manager[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "SELECT ISD before dependency cleanup returned "
                        f"{manager.hex().upper()}"
                    )
                cleanup_scp = vpcd.Scp03(connection)
                cleanup_scp.authenticate()
                for package_aid, label in (
                    (client[1], "DELETE dependent client package"),
                    (service[1], "DELETE dependent service package"),
                ):
                    vpcd.delete_object(cleanup_scp, package_aid, True, label)
                response = vpcd.delete_object(
                    cleanup_scp, contract[1], True,
                    "DELETE library after removing all importers"
                )
                if response[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        "DELETE of an unreferenced library package returned "
                        f"{response[-2:].hex().upper()}, expected 9000"
                    )
                print("TEST:PASS library DELETE succeeds after deleting all "
                      "loaded importers")
                print("TEST:SUMMARY pass=2 fail=0")
                return 0

            def check_unrelated_applet(label: str) -> None:
                selected = vpcd.exchange(
                    connection, select_unrelated, label + " SELECT"
                )
                if selected[-2:] != b"\x90\x00":
                    raise RuntimeError(
                        f"{label} SELECT returned {selected.hex().upper()}"
                    )
                executed = vpcd.exchange(
                    connection, bytes.fromhex("8010000000"), label + " APDU"
                )
                if executed != bytes.fromhex("5A9000"):
                    raise RuntimeError(
                        f"{label} APDU returned {executed.hex().upper()}, "
                        "expected 5A9000"
                    )
                print(f"TEST:PASS unrelated applet remains selectable and executable: {label}")

            response = vpcd.exchange(connection, select_client, "SELECT client")
            if response[-2:] != b"\x90\x00":
                raise RuntimeError(f"SELECT client failed: {response.hex().upper()}")
            if os.environ.get("SIO_CROSS_AID_TEST_PACKAGE_DELETE") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("8015000000"),
                    "retain proxy before package DELETE"
                )
                if response != bytes.fromhex("6A109000"):
                    raise RuntimeError(
                        "proxy retention before package DELETE returned "
                        f"{response.hex().upper()}, expected 6A109000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before package DELETE"
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        f"SELECT ISD before package DELETE returned "
                        f"{response.hex().upper()}"
                    )
                delete_scp = vpcd.Scp03(connection)
                delete_scp.authenticate()
                response = vpcd.delete_object(
                    delete_scp, SERVICE_PACKAGE, True,
                    "DELETE cross-AID service package with related objects",
                    expected_statuses=(0x9000, 0x6581, 0x6985, 0x6A80),
                )
                injected_late_failure = (
                    os.environ.get(
                        "CHOUPI_TEST_FAIL_DELETE_PACKAGE_CLEANUP_ONCE"
                    ) == "1" and response == bytes.fromhex("6581")
                )
                if response == bytes.fromhex("9000") or injected_late_failure:
                    if injected_late_failure:
                        if "VPCD:TEST_FAILED_DELETE_PACKAGE_CLEANUP" not in \
                                log_path.read_text(errors="replace"):
                            raise RuntimeError(
                                "late package DELETE failure was not injected"
                            )
                        print("TEST:PASS late package cleanup failure returned "
                              "6581 after durable DELETE")
                        process, connection = restart_vpcd_after_disconnect(
                            connection, process, listener, command,
                            emulator_env, log_path,
                            "package DELETE failure restart",
                        )
                        print("TEST:PASS VPCD restarted after committed package "
                              "DELETE cleanup failure")
                    else:
                        print("TEST:PASS package DELETE succeeded with "
                              "dependent client still installed")
                    if unrelated is not None:
                        check_unrelated_applet(
                            "after package DELETE and optional restart"
                        )
                    response = vpcd.exchange(
                        connection, select_client,
                        "reselect client after package DELETE"
                    )
                    if response[-2:] != bytes.fromhex("9000"):
                        raise RuntimeError(
                            "client SELECT after package DELETE returned "
                            f"{response.hex().upper()}"
                        )
                    response = vpcd.exchange(
                        connection, bytes.fromhex("8016000000"),
                        "reject retained proxy after package DELETE"
                    )
                    if response != bytes.fromhex("6A0A9000"):
                        raise RuntimeError(
                            "retained proxy after package DELETE returned "
                            f"{response.hex().upper()}, expected 6A0A9000"
                        )
                    response = vpcd.exchange(
                        connection, bytes.fromhex("8010000000"),
                        "fresh lookup after package DELETE"
                    )
                    if response != bytes.fromhex("6A049000"):
                        raise RuntimeError(
                            "fresh lookup after package DELETE returned "
                            f"{response.hex().upper()}, expected 6A049000"
                        )
                    response = vpcd.exchange(
                        connection, bytes.fromhex("00A4040000"),
                        "SELECT ISD before package reinstall"
                    )
                    if response[-2:] != bytes.fromhex("9000"):
                        raise RuntimeError(
                            "SELECT ISD before package reinstall returned "
                            f"{response.hex().upper()}"
                        )
                    reinstall_scp = vpcd.Scp03(connection)
                    reinstall_scp.authenticate()
                    vpcd.load_package(reinstall_scp, service[0], service[1])
                    response = vpcd.install_applet(
                        reinstall_scp, service[1], service[2][0], service[2][0]
                    )
                    if response[-2:] != bytes.fromhex("9000"):
                        raise RuntimeError(
                            "service applet INSTALL after package DELETE "
                            f"returned {response[-2:].hex().upper()}, expected 9000"
                        )
                    select_service = bytes((0x00, 0xA4, 0x04, 0x00,
                                            len(SERVICE_APPLET))) + SERVICE_APPLET
                    response = vpcd.exchange(
                        connection, select_service,
                        "select reinstalled service applet"
                    )
                    if response[-2:] != bytes.fromhex("9000"):
                        raise RuntimeError(
                            "reinstalled service SELECT returned "
                            f"{response.hex().upper()}, expected 9000"
                        )
                    response = vpcd.exchange(
                        connection, select_client,
                        "reselect client after service package reinstall"
                    )
                    if response[-2:] != bytes.fromhex("9000"):
                        raise RuntimeError(
                            "client SELECT after service package reinstall "
                            f"returned {response.hex().upper()}"
                        )
                    if unrelated is not None:
                        check_unrelated_applet(
                            "after same-AID service package reinstall"
                        )
                        response = vpcd.exchange(
                            connection, select_client,
                            "reselect proxy-owning client after unrelated applet"
                        )
                        if response[-2:] != bytes.fromhex("9000"):
                            raise RuntimeError(
                                "client SELECT after unrelated applet returned "
                                f"{response.hex().upper()}"
                            )
                    response = vpcd.exchange(
                        connection, bytes.fromhex("8016000000"),
                        "reject old proxy after service package reinstall"
                    )
                    if response != bytes.fromhex("6A0A9000"):
                        raise RuntimeError(
                            "old proxy after package reinstall returned "
                            f"{response.hex().upper()}, expected 6A0A9000"
                        )
                    response = vpcd.exchange(
                        connection, bytes.fromhex("8010000000"),
                        "fresh lookup after service package reinstall"
                    )
                    if response != bytes.fromhex("45679000"):
                        raise RuntimeError(
                            "fresh lookup after package reinstall returned "
                            f"{response.hex().upper()}, expected 45679000"
                        )
                    print("TEST:PASS old proxy remains blocked and fresh lookup "
                          "reaches service after package reinstall")
                else:
                    print("TEST:INFO package DELETE refused with "
                          f"{response[-2:].hex().upper()}; no stale-route path "
                          "was exercised")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
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
            if os.environ.get("SIO_CROSS_AID_TEST_TYPED_ARRAY_ONLY") == "1":
                response = vpcd.exchange(
                    connection, bytes.fromhex("8015000000"),
                    "create Shareable proxy for typed-array test"
                )
                if response != bytes.fromhex("6A109000"):
                    raise RuntimeError(
                        "typed-array proxy setup returned "
                        f"{response.hex().upper()}, expected 6A109000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("8031000000"),
                    "store foreign Shareable proxy in typed Shareable[]"
                )
                if response != bytes.fromhex("6B509000"):
                    raise RuntimeError(
                        "typed foreign reference-array store returned "
                        f"{response.hex().upper()}, expected 6B509000"
                    )
                print("TEST:PASS typed Shareable[] retained the foreign proxy")
                response = vpcd.exchange(
                    connection, bytes.fromhex("803F000000"),
                    "reject incompatible foreign object from typed Shareable[]"
                )
                if response != bytes.fromhex("6B529000"):
                    raise RuntimeError(
                        "incompatible foreign reference-array store returned "
                        f"{response.hex().upper()}, expected catchable "
                        "ArrayStoreException 6B529000"
                    )
                print("TEST:PASS typed Shareable[] rejects an incompatible "
                      "foreign applet object with ArrayStoreException")
                print("TEST:SUMMARY pass=2 fail=0")
                return 0
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
            response = vpcd.exchange(
                connection, bytes.fromhex("8031000000"),
                "store a foreign Shareable proxy in a typed Shareable array"
            )
            if response != bytes.fromhex("6B509000"):
                raise RuntimeError(
                    "typed foreign reference-array store returned "
                    f"{response.hex().upper()}, expected assignable proxy "
                    "retention and identity 6B509000"
                )
            print("TEST:PASS retained foreign Shareable proxy is type-checked "
                  "and stored in a typed Shareable[]")
            response = vpcd.exchange(
                connection, bytes.fromhex("803F000000"),
                "reject incompatible foreign object from typed Shareable[]"
            )
            if response != bytes.fromhex("6B529000"):
                raise RuntimeError(
                    "incompatible foreign reference-array store returned "
                    f"{response.hex().upper()}, expected catchable "
                    "ArrayStoreException 6B529000"
                )
            print("TEST:PASS typed Shareable[] rejects an incompatible foreign "
                  "applet object with ArrayStoreException")
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
                scp, SERVICE_APPLET, False, "DELETE cross-AID service applet",
                expected_statuses=(0x9000, 0x6581),
            )
            injected_instance_failure = (
                os.environ.get(
                    "CHOUPI_TEST_FAIL_DELETE_INSTANCE_CLEANUP_ONCE"
                ) == "1" and response == bytes.fromhex("6581")
            )
            if injected_instance_failure:
                if "VPCD:TEST_FAILED_DELETE_INSTANCE_CLEANUP" not in \
                        log_path.read_text(errors="replace"):
                    raise RuntimeError(
                        "late instance DELETE failure was not injected"
                    )
                print("TEST:PASS late instance cleanup failure returned "
                      "6581 after durable DELETE")
                process, connection = restart_vpcd_after_disconnect(
                    connection, process, listener, command, emulator_env,
                    log_path, "instance DELETE failure restart",
                )
                print("TEST:PASS VPCD restarted after committed instance "
                      "DELETE cleanup failure")
                response = vpcd.exchange(
                    connection, select_client,
                    "reselect client after instance DELETE failure restart"
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "client SELECT after instance DELETE restart returned "
                        f"{response.hex().upper()}"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("8016000000"),
                    "reject stale proxy after instance DELETE restart"
                )
                if response != bytes.fromhex("6A0A9000"):
                    raise RuntimeError(
                        "old proxy after instance DELETE restart returned "
                        f"{response.hex().upper()}, expected 6A0A9000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD to finish instance DELETE after restart"
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "SELECT ISD after instance DELETE restart returned "
                        f"{response.hex().upper()}"
                    )
                recovery_scp = vpcd.Scp03(connection)
                recovery_scp.authenticate()
                response = vpcd.delete_object(
                    recovery_scp, SERVICE_PACKAGE, True,
                    "delete recovered service package after restart",
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "package DELETE after instance recovery returned "
                        f"{response[-2:].hex().upper()}, expected 9000"
                    )
                response = vpcd.exchange(
                    connection, select_client,
                    "reselect client after recovered service package DELETE"
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "client SELECT after recovered package DELETE returned "
                        f"{response.hex().upper()}"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("8010000000"),
                    "fresh lookup after failed service DELETE"
                )
                if response != bytes.fromhex("6A049000"):
                    raise RuntimeError(
                        "fresh lookup after failed service DELETE returned "
                        f"{response.hex().upper()}, expected 6A049000"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("00A4040000"),
                    "SELECT ISD before recovered service package reinstall"
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "SELECT ISD before recovered package reinstall returned "
                        f"{response.hex().upper()}"
                    )
                reinstall_scp = vpcd.Scp03(connection)
                reinstall_scp.authenticate()
                vpcd.load_package(reinstall_scp, service[0], SERVICE_PACKAGE)
                response = vpcd.install_applet(
                    reinstall_scp, SERVICE_PACKAGE, SERVICE_APPLET,
                    SERVICE_APPLET
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "service INSTALL after restarted package recovery returned "
                        f"{response[-2:].hex().upper()}, expected 9000"
                    )
                response = vpcd.exchange(
                    connection, select_client,
                    "reselect client after recovered service package reinstall"
                )
                if response[-2:] != bytes.fromhex("9000"):
                    raise RuntimeError(
                        "client SELECT after instance reinstall returned "
                        f"{response.hex().upper()}"
                    )
                response = vpcd.exchange(
                    connection, bytes.fromhex("8010000000"),
                    "fresh lookup after recovered service package reinstall"
                )
                if response != bytes.fromhex("45679000"):
                    raise RuntimeError(
                        "fresh lookup after instance reinstall returned "
                        f"{response.hex().upper()}, expected 45679000"
                    )
                print("TEST:PASS restart recovery and package reinstall preserve "
                      "blocked proxy and fresh SIO service")
                print("TEST:SUMMARY pass=1 fail=0")
                return 0
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
            print("TEST:SUMMARY pass=35 fail=0")
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
