#!/usr/bin/env python3
"""Exercise dynamic-package opcode rejection through the real PC VM."""

from __future__ import annotations

import hashlib
import argparse
import re
import subprocess
import sys
from pathlib import Path
from fixture_paths import fixture_path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
CAP_PATH = fixture_path("jcalgtest.cap", ROOT)
EXPECTED_CAP_SHA256 = (
    "fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb"
)
APPLET_AID = bytes.fromhex("4A43416C675465737431")
PROCESS_METHOD_OFFSET = 0x3E63
COMPONENTS = (
    ("Header", 1),
    ("Directory", 2),
    ("Applet", 3),
    ("Import", 4),
    ("ConstantPool", 5),
    ("Class", 6),
    ("Method", 7),
    ("StaticField", 8),
    ("RefLocation", 9),
    ("Export", 10),
    ("Descriptor", 11),
)


def component(archive: ZipFile, folder: str, name: str, tag: int) -> bytes:
    data = archive.read(f"{folder}/{name}.cap")
    if len(data) < 4 or data[0] != tag:
        raise ValueError(f"invalid {name}.cap component")
    if int.from_bytes(data[1:3], "big") + 3 != len(data):
        raise ValueError(f"{name}.cap declared size mismatch")
    return data


def descriptor_methods(descriptor: bytes) -> list[list[tuple[int, int]]]:
    payload = descriptor[3:]
    if not payload:
        raise ValueError("empty Descriptor component")
    owner_methods: list[list[tuple[int, int]]] = []
    offset = 1
    for _ in range(payload[0]):
        if offset > len(payload) or len(payload) - offset < 9:
            raise ValueError("truncated Descriptor class record")
        interfaces = payload[offset + 4]
        fields = int.from_bytes(payload[offset + 5 : offset + 7], "big")
        methods = int.from_bytes(payload[offset + 7 : offset + 9], "big")
        method_start = offset + 9 + interfaces * 2 + fields * 7
        if method_start > len(payload) or methods > (len(payload) - method_start) // 12:
            raise ValueError("truncated Descriptor method table")
        owner_methods.append([
            (
                int.from_bytes(payload[method_start + index * 12 + 2 :
                                       method_start + index * 12 + 4], "big"),
                int.from_bytes(payload[method_start + index * 12 + 6 :
                                       method_start + index * 12 + 8], "big"),
            )
            for index in range(methods)
        ])
        offset = method_start + methods * 12
    if offset > len(payload):
        raise ValueError("Descriptor records exceed component bounds")
    return owner_methods


def class_public_method_tables(class_component: bytes) -> list[list[int]]:
    header = component_version_header(class_component)
    if header != (2, 1):
        raise ValueError(f"fixture parser expects CAP 2.1, got {header}")
    data = class_component[3:]
    tables: list[list[int]] = []
    offset = 0
    while offset < len(data):
        if len(data) - offset < 10:
            raise ValueError("truncated Class info record")
        flags = data[offset]
        interface_count = flags & 0x0F
        public_count = data[offset + 7]
        package_count = data[offset + 9]
        table_end = offset + 10 + 2 * (public_count + package_count)
        if table_end > len(data):
            raise ValueError("truncated Class method table")
        public = [
            int.from_bytes(data[offset + 10 + index * 2 :
                                offset + 12 + index * 2], "big")
            for index in range(public_count)
        ]
        cursor = table_end
        for _ in range(interface_count):
            if len(data) - cursor < 3:
                raise ValueError("truncated implemented-interface entry")
            record_size = 3 + data[cursor + 2]
            if record_size > len(data) - cursor:
                raise ValueError("truncated implemented-interface map")
            cursor += record_size
        if flags >> 4 & 0x02:
            raise ValueError("fixture has an unsupported remote class record")
        tables.append(public)
        offset = cursor
    return tables


def component_version_header(class_component: bytes) -> tuple[int, int]:
    with ZipFile(CAP_PATH) as archive:
        header = archive.read("algtest/javacard/Header.cap")
    if len(header) < 9:
        raise ValueError("truncated CAP Header component")
    return header[8], header[7]


def applet_class_superclass_ref(class_component: bytes,
                               target_index: int) -> bytes:
    if component_version_header(class_component) != (2, 1):
        raise ValueError("fixture parser expects CAP 2.1")
    data = class_component[3:]
    offset = 0
    for class_index in range(target_index + 1):
        if len(data) - offset < 10 or data[offset] & 0x80:
            raise ValueError("invalid class record before applet class")
        flags = data[offset]
        public_count = data[offset + 7]
        package_count = data[offset + 9]
        superclass = data[offset + 1 : offset + 3]
        end = offset + 10 + 2 * (public_count + package_count)
        for _ in range(flags & 0x0F):
            if end + 3 > len(data):
                raise ValueError("truncated implemented-interface record")
            end += 3 + data[end + 2]
        if end > len(data):
            raise ValueError("Class info exceeds component bounds")
        if class_index == target_index:
            return superclass
        offset = end
    raise ValueError("applet class record not found")


def constant_pool_index(constant_pool: bytes, entry: bytes) -> int:
    """Return the index of an exact 4-byte constant pool entry."""
    count = int.from_bytes(constant_pool[3:5], "big")
    for index in range(count):
        if constant_pool[5 + 4 * index : 9 + 4 * index] == entry:
            return index
    raise ValueError(f"constant pool entry {entry.hex().upper()} not found")


# CAPIN-05/06 receiver fixtures: the opcode the VM must reject with
# SecurityException (exception id 29) at the start of process().
RECEIVER_CHECK_OPCODES = {
    "invokespecial-global-receiver": 0x8C,
    "invokevirtual-receiver-mismatch": 0x8B,
    "invokeinterface-array-receiver": 0x8E,
    # RV-20261106-06: field receivers must be the declaring class or a
    # subclass, for every get/put encoding.
    "getfield-rom-receiver": 0x85,
    "getfield-unrelated-receiver": 0x85,
    "putfield-unrelated-receiver": 0x89,
}

# Internal InstanceFieldref (CP 6): a short field, token 2, of JCAlgTest class
# offset 0x00C6, which is unrelated to the applet class (offset 0x0094).
UNRELATED_SHORT_FIELD = bytes((0x02, 0x00, 0xC6, 0x02))


def build_malformed_load_code(mutation: str) -> Path:
    raw_cap = CAP_PATH.read_bytes()
    digest = hashlib.sha256(raw_cap).hexdigest()
    if digest != EXPECTED_CAP_SHA256:
        raise ValueError(
            f"upstream CAP changed: expected SHA-256 {EXPECTED_CAP_SHA256}, "
            f"got {digest}"
        )

    with ZipFile(CAP_PATH) as archive:
        folder = "algtest/javacard"
        parts: dict[str, bytes] = {}
        for name, tag in COMPONENTS:
            try:
                parts[name] = component(archive, folder, name, tag)
            except KeyError:
                if name != "Export":
                    raise

    applet = parts["Applet"]
    if applet[3] != 1:
        raise ValueError("expected one applet in the upstream CAP")
    aid_length = applet[4]
    install_offset_index = 5 + aid_length
    install_method_offset = int.from_bytes(
        applet[install_offset_index : install_offset_index + 2], "big"
    )
    owners = descriptor_methods(parts["Descriptor"])
    owner_index = next(
        (index for index, methods in enumerate(owners)
         if install_method_offset in {offset for offset, _ in methods} and
         PROCESS_METHOD_OFFSET in {offset for offset, _ in methods}),
        None,
    )
    if owner_index is None:
        raise ValueError("process/install methods not in the same Descriptor class")
    public_tables = class_public_method_tables(parts["Class"])
    if owner_index >= len(public_tables) or PROCESS_METHOD_OFFSET not in public_tables[owner_index]:
        raise ValueError("process target is not a public virtual method start")

    process_length = next(
        (length for methods in owners for offset, length in methods
         if offset == PROCESS_METHOD_OFFSET),
        None,
    )
    if process_length is None or process_length == 0:
        raise ValueError("process method has no Descriptor bytecode length")

    method_component = parts["Method"]
    handler_count = method_component[3]
    handler_bytes = handler_count * 8
    if PROCESS_METHOD_OFFSET <= handler_bytes:
        raise ValueError("process target falls in the Method handler table")
    # CAP method offsets are one-based within the Method info area after the
    # component tag/size and handler_count; translate to the .cap file bytes.
    method_header_index = PROCESS_METHOD_OFFSET + 3
    if method_header_index >= len(method_component):
        raise ValueError("process method header outside Method component")
    header_size = 4 if method_component[method_header_index] & 0x80 else 2
    bytecode_index = method_header_index + header_size
    body_end = bytecode_index + process_length
    if body_end > len(method_component):
        raise ValueError("process bytecode body exceeds Method component")

    ordered = b"".join(parts[name] for name, _ in COMPONENTS if name in parts)
    missing_required_components = {
        "missing-import-component": "Import",
        "external-superclass-without-import": "Import",
        "missing-static-field-component": "StaticField",
        "missing-reference-location": "RefLocation",
    }
    if mutation in missing_required_components:
        component_name = missing_required_components[mutation]
        superclass_ref = None
        if mutation == "external-superclass-without-import":
            superclass_ref = applet_class_superclass_ref(
                parts["Class"], owner_index
            )
            if superclass_ref == b"\xff\xff" or not superclass_ref[0] & 0x80:
                raise ValueError(
                    "upstream applet superclass is not an external Classref"
                )
        missing = parts.pop(component_name)
        output = ROOT / f"build/rv28-malformed-{mutation}-load-code.bin"
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(b"".join(
            parts[name] for name, _ in COMPONENTS if name in parts
        ))
        print(
            f"MALFORMED_CAP:PASS sha256={digest} removed={component_name} "
            f"bytes={len(missing)} type={mutation} "
            + (f"superclass_ref={superclass_ref.hex().upper()} "
               if superclass_ref is not None else "")
            + "source_cap_unchanged=true"
        )
        return output
    # Method follows Header, Directory, Applet, Import, ConstantPool, Class.
    method_start = sum(len(parts[name]) for name in
                       ("Header", "Directory", "Applet", "Import", "ConstantPool", "Class"))
    code_index = method_start + bytecode_index
    mutable = bytearray(ordered)
    if mutable[code_index] != method_component[bytecode_index]:
        raise ValueError("assembled Method component offset mismatch")
    if mutation in ("external-superclass-import-token-out-of-range",
                    "superclass-self-cycle", "superclass-mid-record"):
        class_data = parts["Class"][3:]
        class_offset = 0
        target_superclass = None
        for class_index in range(owner_index + 1):
            if len(class_data) - class_offset < 10:
                raise ValueError("truncated Class info before applet class")
            flags = class_data[class_offset]
            if flags & 0x80:
                raise ValueError("fixture parser expected class records only")
            if class_index == owner_index:
                target_superclass = class_data[class_offset + 1 : class_offset + 3]
                break
            public_count = class_data[class_offset + 7]
            package_count = class_data[class_offset + 9]
            class_end = class_offset + 10 + 2 * (public_count + package_count)
            for _ in range(flags & 0x0F):
                if class_end + 3 > len(class_data):
                    raise ValueError("truncated implemented-interface record")
                class_end += 3 + class_data[class_end + 2]
            if class_end > len(class_data):
                raise ValueError("Class info exceeds component bounds")
            class_offset = class_end
        if target_superclass is None or len(target_superclass) != 2:
            raise ValueError("applet Class superclass reference not found")
        if target_superclass == b"\xff\xff":
            raise ValueError("applet class unexpectedly is java.lang.Object")
        mutation_index = (
            sum(len(parts[name]) for name, _ in COMPONENTS[:5])
            + 3 + class_offset + 1
        )
        if mutation == "superclass-self-cycle":
            # RV-20261106-07: the applet class names itself as superclass.
            replacement = class_offset.to_bytes(2, "big")
        elif mutation == "superclass-mid-record":
            # CAPIN-07: an earlier offset that is not a class record start.
            replacement = (1).to_bytes(2, "big")
        else:
            replacement = b"\xff\x00"  # external package token 127, class token 0
    elif mutation == "unknown-component-tag":
        mutation_index = len(mutable)
        replacement = bytes((12, 0, 1, 0))
    elif mutation == "load-header-aid-mismatch":
        mutation_index = 0
        replacement = b""
    elif mutation in ("impdep1", "impdep2"):
        mutation_index = code_index
        replacement = bytes((0xFE if mutation == "impdep1" else 0xFF,))
        if mutable[mutation_index] == replacement[0]:
            raise ValueError(f"upstream process already starts with {mutation}")
    elif mutation == "truncated-sipush":
        mutation_index = method_start + body_end - 1
        replacement = bytes((0x13,))  # SIPUSH requires two operand bytes.
        if mutable[mutation_index] != 0x7A:
            raise ValueError("expected process method to end with BC_RETURN")
    elif mutation == "fall-through":
        mutation_index = method_start + body_end - 1
        replacement = bytes((0x00,))  # NOP at the final byte exhausts the body.
        if mutable[mutation_index] != 0x7A:
            raise ValueError("expected process method to end with BC_RETURN")
    elif mutation == "zero-process-body-length":
        descriptor = parts["Descriptor"]
        payload = descriptor[3:]
        descriptor_offset = 1
        length_field_offset = None
        for class_index, methods in enumerate(owners):
            if descriptor_offset > len(payload) or len(payload) - descriptor_offset < 9:
                raise ValueError("truncated Descriptor class record")
            interfaces = payload[descriptor_offset + 4]
            fields = int.from_bytes(
                payload[descriptor_offset + 5 : descriptor_offset + 7], "big"
            )
            method_count = int.from_bytes(
                payload[descriptor_offset + 7 : descriptor_offset + 9], "big"
            )
            method_records = descriptor_offset + 9 + interfaces * 2 + fields * 7
            if method_count != len(methods):
                raise ValueError("Descriptor method table changed during parse")
            for method_index, (offset, length) in enumerate(methods):
                if class_index == owner_index and offset == PROCESS_METHOD_OFFSET:
                    if length == 0:
                        raise ValueError("upstream process body is already empty")
                    length_field_offset = (
                        3 + method_records + method_index * 12 + 6
                    )
                    break
            if length_field_offset is not None:
                break
            descriptor_offset = method_records + method_count * 12
        if length_field_offset is None:
            raise ValueError("process method Descriptor length field not found")
        descriptor_component_index = next(
            index for index, (name, _) in enumerate(COMPONENTS)
            if name == "Descriptor"
        )
        descriptor_start = sum(
            len(parts[name]) if name in parts else 0
            for name, _ in COMPONENTS[:descriptor_component_index]
        )
        mutation_index = descriptor_start + length_field_offset
        replacement = b"\x00\x00"
    elif mutation == "branch-into-operand":
        mutation_index = code_index
        replacement = bytes((0x70, 0x01))  # GOTO +1 targets its operand byte.
        if process_length < len(replacement):
            raise ValueError("process body is too short for branch fixture")
    elif mutation == "branch-outside-method":
        mutation_index = code_index
        replacement = bytes((0xA8, 0x7F, 0xFF))  # GOTO_W beyond this frame.
        if process_length < len(replacement):
            raise ValueError("process body is too short for wide-branch fixture")
    elif mutation == "conditional-branch-into-operand":
        mutation_index = code_index
        # Push zero so IFEQ is taken; its +1 displacement targets its operand.
        replacement = bytes((0x10, 0x00, 0x60, 0x01))
        if process_length < len(replacement):
            raise ValueError("process body is too short for conditional-branch fixture")
    elif mutation == "wide-conditional-branch-into-operand":
        mutation_index = code_index
        # Push zero so IFEQ_W is taken into its first displacement byte.
        replacement = bytes((0x10, 0x00, 0x98, 0x00, 0x01))
        if process_length < len(replacement):
            raise ValueError("process body is too short for wide conditional fixture")
    elif mutation == "jsr-into-operand":
        mutation_index = code_index
        # JSR +1 targets its first displacement byte instead of an opcode.
        replacement = bytes((0x71, 0x00, 0x01))
        if process_length < len(replacement):
            raise ValueError("process body is too short for JSR fixture")
    elif mutation == "ret-non-return-address":
        mutation_index = code_index
        # Process local 0 is the receiver/reference argument, not a return PC.
        replacement = bytes((0x72, 0x00))
        if process_length < len(replacement):
            raise ValueError("process body is too short for RET fixture")
    elif mutation in (
        "invoke-static-cp-out-of-range",
        "invoke-virtual-cp-out-of-range",
        "invoke-special-cp-out-of-range",
        "invoke-interface-cp-out-of-range",
    ):
        mutation_index = code_index
        # Each instruction is complete, but its CP index is outside the pool.
        if mutation == "invoke-static-cp-out-of-range":
            replacement = bytes((0x8D, 0xFF, 0xFF))
        elif mutation == "invoke-virtual-cp-out-of-range":
            replacement = bytes((0x8B, 0xFF, 0xFF))
        elif mutation == "invoke-special-cp-out-of-range":
            replacement = bytes((0x8C, 0xFF, 0xFF))
        else:
            replacement = bytes((0x8E, 0x01, 0xFF, 0xFF, 0x00))
        if process_length < len(replacement):
            raise ValueError("process body is too short for invocation fixture")
    elif mutation == "exception-catch-cp-out-of-range":
        handler_bytes = handler_count * 8
        process_methods_offset = PROCESS_METHOD_OFFSET - handler_bytes - 1
        process_body_methods_offset = process_methods_offset + header_size
        handler_index = None
        handler_relative_offset = None
        for index in range(handler_count):
            entry_index = 4 + index * 8
            start_offset = int.from_bytes(
                method_component[entry_index : entry_index + 2], "big"
            )
            active_flags = int.from_bytes(
                method_component[entry_index + 2 : entry_index + 4], "big"
            )
            start_methods_offset = start_offset - handler_bytes - 1
            relative = start_methods_offset - process_body_methods_offset
            if (active_flags & 0x7FFF) and 0 <= relative < process_length:
                handler_index = index
                handler_relative_offset = relative
                break
        if handler_index is None or handler_relative_offset is None:
            raise ValueError("no active process exception handler was found")
        if process_length - handler_relative_offset < 3:
            raise ValueError("active handler starts too close to process end")
        mutation_index = code_index + handler_relative_offset
        replacement = bytes((0x8D, 0xFF, 0xFF))
        catch_index = method_start + 4 + handler_index * 8 + 6
        mutable[catch_index : catch_index + 2] = b"\xFF\xFF"
        print(
            f"MALFORMED_CAP:HANDLER index={handler_index} "
            f"relative_body_offset={handler_relative_offset} "
            "catch_type_index=0xFFFF"
        )
    elif mutation == "exception-handler-target-into-operand":
        handler_bytes = handler_count * 8
        process_methods_offset = PROCESS_METHOD_OFFSET - handler_bytes - 1
        process_body_methods_offset = process_methods_offset + header_size
        handler_index = None
        handler_relative_offset = None
        for index in range(handler_count):
            entry_index = 4 + index * 8
            start_offset = int.from_bytes(
                method_component[entry_index : entry_index + 2], "big"
            )
            active_flags = int.from_bytes(
                method_component[entry_index + 2 : entry_index + 4], "big"
            )
            start_methods_offset = start_offset - handler_bytes - 1
            relative = start_methods_offset - process_body_methods_offset
            if (active_flags & 0x7FFF) and 0 <= relative < process_length:
                handler_index = index
                handler_relative_offset = relative
                break
        if handler_index is None or handler_relative_offset is None:
            raise ValueError("no active process exception handler was found")
        if process_length - handler_relative_offset < 3:
            raise ValueError("active handler starts too close to process end")
        mutation_index = code_index + handler_relative_offset
        # INVOKESTATIC with CP index 0 triggers a catch-all handler; its first
        # operand byte is also a NOP opcode, used as the malformed destination.
        replacement = bytes((0x8D, 0x00, 0x00))
        entry_index = method_start + 4 + handler_index * 8
        target_methods_offset = (
            process_body_methods_offset + handler_relative_offset + 1
        )
        target_component_offset = target_methods_offset + handler_bytes + 1
        mutable[entry_index + 4 : entry_index + 6] = (
            target_component_offset.to_bytes(2, "big")
        )
        mutable[entry_index + 6 : entry_index + 8] = b"\x00\x00"
        print(
            f"MALFORMED_CAP:HANDLER index={handler_index} "
            f"relative_body_offset={handler_relative_offset} "
            f"handler_component_offset=0x{target_component_offset:04X} "
            "catch_type_index=0x0000 target=operand"
        )
    elif mutation == "exception-handler-range-into-operand":
        handler_bytes = handler_count * 8
        process_methods_offset = PROCESS_METHOD_OFFSET - handler_bytes - 1
        process_body_methods_offset = process_methods_offset + header_size
        handler_index = None
        handler_relative_offset = None
        for index in range(handler_count):
            entry_index = 4 + index * 8
            start_offset = int.from_bytes(
                method_component[entry_index : entry_index + 2], "big"
            )
            active_flags = int.from_bytes(
                method_component[entry_index + 2 : entry_index + 4], "big"
            )
            start_methods_offset = start_offset - handler_bytes - 1
            relative = start_methods_offset - process_body_methods_offset
            if (active_flags & 0x7FFF) and 0 <= relative < process_length:
                handler_index = index
                handler_relative_offset = relative
                break
        if handler_index is None or handler_relative_offset is None:
            raise ValueError("no active process exception handler was found")
        if process_length - handler_relative_offset < 5:
            raise ValueError("active handler starts too close to process end")
        mutation_index = code_index + handler_relative_offset
        # Start the protected range at BSPUSH's operand, then throw from the
        # following INVOKESTATIC. The range is active but malformed.
        replacement = bytes((0x10, 0x00, 0x8D, 0x00, 0x00))
        entry_index = method_start + 4 + handler_index * 8
        range_start = (
            process_body_methods_offset + handler_relative_offset + 1
        )
        mutable[entry_index : entry_index + 2] = (
            (range_start + handler_bytes + 1).to_bytes(2, "big")
        )
        old_range_flags = int.from_bytes(
            mutable[entry_index + 2 : entry_index + 4], "big"
        )
        mutable[entry_index + 2 : entry_index + 4] = (
            (old_range_flags & 0x8000 | 4).to_bytes(2, "big")
        )
        mutable[entry_index + 6 : entry_index + 8] = b"\x00\x00"
        print(
            f"MALFORMED_CAP:HANDLER index={handler_index} "
            f"range_start=0x{range_start:04X} length=4 "
            "catch_type_index=0x0000 start=operand"
        )
    elif mutation == "exception-handler-range-out-of-bounds":
        handler_bytes = handler_count * 8
        process_methods_offset = PROCESS_METHOD_OFFSET - handler_bytes - 1
        process_body_methods_offset = process_methods_offset + header_size
        handler_index = None
        handler_relative_offset = None
        for index in range(handler_count):
            entry_index = 4 + index * 8
            start_offset = int.from_bytes(
                method_component[entry_index : entry_index + 2], "big"
            )
            active_flags = int.from_bytes(
                method_component[entry_index + 2 : entry_index + 4], "big"
            )
            start_methods_offset = start_offset - handler_bytes - 1
            relative = start_methods_offset - process_body_methods_offset
            if (active_flags & 0x7FFF) and 0 <= relative < process_length:
                handler_index = index
                handler_relative_offset = relative
                break
        if handler_index is None or handler_relative_offset is None:
            raise ValueError("no active process exception handler was found")
        if process_length - handler_relative_offset < 3:
            raise ValueError("active handler starts too close to process end")
        mutation_index = code_index + handler_relative_offset
        replacement = bytes((0x8D, 0x00, 0x00))
        entry_index = method_start + 4 + handler_index * 8
        old_range_flags = int.from_bytes(
            mutable[entry_index + 2 : entry_index + 4], "big"
        )
        mutable[entry_index + 2 : entry_index + 4] = (
            (old_range_flags & 0x8000 | 0x7FFF).to_bytes(2, "big")
        )
        mutable[entry_index + 6 : entry_index + 8] = b"\x00\x00"
        print(
            f"MALFORMED_CAP:HANDLER index={handler_index} "
            f"relative_body_offset={handler_relative_offset} "
            "length=32767 catch_type_index=0x0000 range=end-out-of-bounds"
        )
    elif mutation == "exception-handler-range-start-out-of-bounds":
        handler_bytes = handler_count * 8
        process_methods_offset = PROCESS_METHOD_OFFSET - handler_bytes - 1
        process_body_methods_offset = process_methods_offset + header_size
        handler_index = None
        handler_relative_offset = None
        for index in range(handler_count):
            entry_index = 4 + index * 8
            start_offset = int.from_bytes(
                method_component[entry_index : entry_index + 2], "big"
            )
            active_flags = int.from_bytes(
                method_component[entry_index + 2 : entry_index + 4], "big"
            )
            start_methods_offset = start_offset - handler_bytes - 1
            relative = start_methods_offset - process_body_methods_offset
            if (active_flags & 0x7FFF) and 0 <= relative < process_length:
                handler_index = index
                handler_relative_offset = relative
                break
        if handler_index is None or handler_relative_offset is None:
            raise ValueError("no active process exception handler was found")
        if process_length - handler_relative_offset < 3:
            raise ValueError("active handler starts too close to process end")
        mutation_index = code_index + handler_relative_offset
        replacement = bytes((0x8D, 0x00, 0x00))
        entry_index = method_start + 4 + handler_index * 8
        mutable[entry_index : entry_index + 2] = b"\xFF\xFF"
        mutable[entry_index + 6 : entry_index + 8] = b"\x00\x00"
        print(
            f"MALFORMED_CAP:HANDLER index={handler_index} "
            f"relative_body_offset={handler_relative_offset} "
            "start_pc=0xFFFF catch_type_index=0x0000"
        )
    elif mutation == "itableswitch-into-operand":
        mutation_index = code_index
        # IIPUSH 0 then ITABLESWITCH; default and selected case target operand.
        replacement = bytes((
            0x14, 0x00, 0x00, 0x00, 0x00,
            0x74, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x01,
        ))
        if process_length < len(replacement):
            raise ValueError("process body is too short for itableswitch fixture")
    elif mutation == "slookupswitch-into-operand":
        mutation_index = code_index
        # SIPUSH 0 then one sorted SLOOKUPSWITCH pair targeting the table.
        replacement = bytes((
            0x13, 0x00, 0x00,
            0x75, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x01,
        ))
        if process_length < len(replacement):
            raise ValueError("process body is too short for slookupswitch fixture")
    elif mutation == "ilookupswitch-into-operand":
        mutation_index = code_index
        # IIPUSH 0 then one sorted ILOOKUPSWITCH pair targeting the table.
        replacement = bytes((
            0x14, 0x00, 0x00, 0x00, 0x00,
            0x76, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        ))
        if process_length < len(replacement):
            raise ValueError("process body is too short for ilookupswitch fixture")
    elif mutation == "switch-into-operand":
        mutation_index = code_index
        # Push index 0, then a one-entry STABLESWITCH. Both default and case
        # destinations point into the first byte of the default displacement.
        replacement = bytes((
            0x10, 0x00, 0x73, 0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01,
        ))
        if process_length < len(replacement):
            raise ValueError("process body is too short for switch fixture")
    elif mutation == "invokespecial-global-receiver":
        # CAPIN-05 (3): aload_1 pushes the APDU object (a JCRE entry point,
        # global instance); invokespecial java.lang.Object.<init> on it from
        # uploaded code must throw SecurityException. Without the check the
        # constructor would run and process() would continue.
        cp_index = constant_pool_index(parts["ConstantPool"],
                                       bytes((0x06, 0x80, 0x00, 0x00)))
        mutation_index = code_index
        replacement = bytes((0x19, 0x8C)) + cp_index.to_bytes(2, "big")
    elif mutation == "invokevirtual-receiver-mismatch":
        # CAPIN-06: javacardx.crypto.Cipher.init(Key, byte) (JC305 token 3,
        # nargs 3) with the APDU object as receiver; the receiver is not a
        # Cipher, so uploaded code must get SecurityException instead of a
        # dispatch through the APDU class's method table.
        cp_index = constant_pool_index(parts["ConstantPool"],
                                       bytes((0x03, 0x83, 0x01, 0x03)))
        mutation_index = code_index
        replacement = (bytes((0x19, 0x19, 0x19, 0x8B)) +
                       cp_index.to_bytes(2, "big"))
    elif mutation == "invokeinterface-array-receiver":
        # CAPIN-06: a new byte[4] as receiver of javacard.security.Key
        # (class token 0) method 1; arrays implement no interface.
        cp_index = constant_pool_index(parts["ConstantPool"],
                                       bytes((0x01, 0x81, 0x00, 0x00)))
        mutation_index = code_index
        replacement = (bytes((0x10, 0x04, 0x90, 0x0B, 0x8E, 0x01)) +
                       cp_index.to_bytes(2, "big") + bytes((0x01,)))
    elif mutation in ("getfield-rom-receiver", "getfield-unrelated-receiver",
                      "putfield-unrelated-receiver"):
        cp_index = constant_pool_index(parts["ConstantPool"],
                                       UNRELATED_SHORT_FIELD)
        if cp_index > 0xFF:
            raise ValueError("short-form field fixture needs a one-byte CP index")
        mutation_index = code_index
        if mutation == "getfield-rom-receiver":
            # aload_1 (APDU object, a ROM class); getfield_s
            replacement = bytes((0x19, 0x85, cp_index))
        elif mutation == "getfield-unrelated-receiver":
            # aload_0 (the applet); getfield_s of an unrelated class's field
            replacement = bytes((0x18, 0x85, cp_index))
        else:
            # aload_0; sconst_0; putfield_s of an unrelated class's field
            replacement = bytes((0x18, 0x03, 0x89, cp_index))
    else:
        raise ValueError(f"unsupported mutation: {mutation}")
    if mutation in RECEIVER_CHECK_OPCODES and process_length < len(replacement):
        raise ValueError(f"process body is too short for {mutation} fixture")
    original_bytes = bytes(mutable[mutation_index :
                                   mutation_index + len(replacement)])
    mutable[mutation_index : mutation_index + len(replacement)] = replacement

    output = ROOT / f"build/rv28-malformed-{mutation}-load-code.bin"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(mutable)
    print(
        f"MALFORMED_CAP:PASS sha256={digest} class={owner_index} "
        f"method_offset=0x{PROCESS_METHOD_OFFSET:04X} "
        f"body_byte_index=0x{mutation_index:X} original={original_bytes.hex().upper()} "
        f"mutated={replacement.hex().upper()} type={mutation} "
        "source_cap_unchanged=true"
    )
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mutation", choices=("unknown-component-tag", "load-header-aid-mismatch", "missing-reference-location", "missing-import-component", "external-superclass-without-import", "missing-static-field-component", "external-superclass-import-token-out-of-range", "impdep1", "impdep2", "truncated-sipush",
                                "fall-through", "zero-process-body-length",
                                "branch-into-operand", "branch-outside-method",
                                "conditional-branch-into-operand",
                                "wide-conditional-branch-into-operand",
                                "jsr-into-operand", "ret-non-return-address",
                                "invoke-static-cp-out-of-range",
                                "invoke-virtual-cp-out-of-range",
                                "invoke-special-cp-out-of-range",
                                "invoke-interface-cp-out-of-range",
                                "exception-catch-cp-out-of-range",
                                "exception-handler-target-into-operand",
                                "exception-handler-range-into-operand",
                                "exception-handler-range-out-of-bounds",
                                "exception-handler-range-start-out-of-bounds",
                                "itableswitch-into-operand",
                                "slookupswitch-into-operand",
                                "ilookupswitch-into-operand",
                                "switch-into-operand",
                                "superclass-self-cycle",
                                "superclass-mid-record",
                                *RECEIVER_CHECK_OPCODES),
        default="impdep1",
    )
    args = parser.parse_args()
    if args.mutation == "external-superclass-without-import":
        # Exercise the real GlobalPlatform LOAD APDU, then prove the card
        # manager still answers GET STATUS and DELETE after rejecting it.
        return subprocess.run(
            [
                sys.executable,
                str(ROOT / "tests/integration/cap_missing_import_apdu_vpcd.py"),
            ],
            cwd=ROOT,
            check=False,
        ).returncode
    code = build_malformed_load_code(args.mutation)
    log = ROOT / f"artifacts/logs/rv28-malformed-{args.mutation}-vpcd-20261106.log"
    load_rejection_mutations = (
        "unknown-component-tag", "load-header-aid-mismatch",
        "missing-reference-location", "missing-import-component",
        "external-superclass-without-import",
        "missing-static-field-component",
        "external-superclass-import-token-out-of-range",
    )
    if args.mutation in load_rejection_mutations:
        log.unlink(missing_ok=True)
    select_status = "9000" if args.mutation in (
        "truncated-sipush", "fall-through",
        "exception-catch-cp-out-of-range",
        "exception-handler-target-into-operand",
        "exception-handler-range-into-operand",
        "exception-handler-range-out-of-bounds",
        "exception-handler-range-start-out-of-bounds",
    ) else "6F00"
    process_status = "6F00" if args.mutation in (
        "truncated-sipush", "fall-through",
        "exception-catch-cp-out-of-range",
        "exception-handler-target-into-operand",
        "exception-handler-range-into-operand",
        "exception-handler-range-out-of-bounds",
        "exception-handler-range-start-out-of-bounds",
    ) else "6999"
    commands = [
        "--passes", "1",
        # First faulting APDUs include a cold restore and package context
        # setup. Keep the bound generous enough to distinguish slow recovery
        # from an interpreter that never answers.
        "--apdu-timeout", "60",
        "--cap-code", str(code),
        "--log", str(log),
    ]
    if args.mutation == "load-header-aid-mismatch":
        commands.extend(("--load-package-aid-override",
                         "4A43416C6754657375"))
    install_rejection_mutations = ("superclass-self-cycle",
                                   "superclass-mid-record")
    if args.mutation in install_rejection_mutations:
        log.unlink(missing_ok=True)
    if args.mutation not in load_rejection_mutations + install_rejection_mutations:
        commands.extend((
            "--expect-select-status", select_status,
            "--apdu", "B0E1000000", "--expect-apdu-response", process_status,
            "--apdu", "00A4040008A00000015100000000",
            "--expect-apdu-response", "6F0A8408A0000001510000009000",
            "--apdu", "00A404000A" + APPLET_AID.hex() + "00",
            "--expect-apdu-response", select_status,
            "--apdu", "00A4040008A00000015100000000",
            "--expect-apdu-response", "6F0A8408A0000001510000009000",
            "--apdu", "80CA006600",
            "--expect-apdu-response",
            "6618731606072A864886FC6B01640B06092A864886FC6B0403109000",
        ))
    run_kwargs = {"cwd": ROOT, "check": False}
    if args.mutation in load_rejection_mutations + install_rejection_mutations:
        # The inner generic VPCD runner correctly reports that no applet can
        # start after LOAD rejection. Capture that expected negative result so
        # the outer acceptance check is the only PASS/FAIL summary shown.
        run_kwargs.update(stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True)
    result = subprocess.run(
        [str(ROOT / "tools/test-pc-vpcd-applet"), *commands], **run_kwargs
    )
    if args.mutation in install_rejection_mutations:
        # RV-20261106-07 / CAPIN-07: INSTALL-time class-reference validation
        # (stage 9) rejects a superclass that is not an earlier class record,
        # before any bytecode of the package runs. (Before that check, the
        # bounded superclass walks failed `new` with SecurityException.)
        run_log = log.read_text(encoding="utf-8", errors="replace")
        if not re.search(r"PC applet INSTALL failed stage=9 sw=6F00 vm=0000 "
                         r"exception=0\b", run_log):
            if result.stdout:
                print(result.stdout, file=sys.stderr, end="")
            print(f"HIERARCHY_CYCLE:FAIL mutation={args.mutation}",
                  file=sys.stderr)
            return 1
        print(f"HIERARCHY_CYCLE:PASS mutation={args.mutation} "
              "install_rejected stage=9 class_reference_validation")
        return 0
    if args.mutation in load_rejection_mutations:
        run_log = log.read_text(encoding="utf-8", errors="replace")
        load_rejected = "ERROR: PC CAP LOAD data rejected" in run_log
        registry_match = re.search(
            r"VPCD:CAP_REGISTRY_SAVED packages=(\d+)", run_log
        )
        stored_packages = int(registry_match.group(1)) if registry_match else -1
        registry_empty = stored_packages in (-1, 0)
        if not load_rejected or not registry_empty or \
                "ERROR: PC applet INSTALL failed" in run_log:
            if args.mutation == "unknown-component-tag" and result.stdout:
                print(result.stdout, file=sys.stderr, end="")
            print(
                "CAP_LOAD_VALIDATION:FAIL "
                f"load_rejected={int(load_rejected)} "
                f"registry_saved_packages={stored_packages} "
                f"install_rejected={int('ERROR: PC applet INSTALL failed' in run_log)}",
                file=sys.stderr,
            )
            return 1
        if args.mutation == "unknown-component-tag":
            reason = "unknown_component"
        elif args.mutation == "load-header-aid-mismatch":
            reason = "package_aid_header_mismatch"
        elif args.mutation == "external-superclass-import-token-out-of-range":
            reason = "external_superclass_import_token_out_of_range"
        else:
            component_name = {
                "missing-reference-location": "reference_location",
                "missing-import-component": "import",
                "external-superclass-without-import": "import",
                "missing-static-field-component": "static_field",
            }[args.mutation]
            reason = f"missing_required_{component_name}"
        print("CAP_LOAD_VALIDATION:PASS "
              f"{reason}_rejected_before_store registry_saved_packages={stored_packages}")
        return 0
    if result.returncode != 0:
        print(f"MALFORMED_CAP:FAIL vpcd_exit={result.returncode}", file=sys.stderr)
        return result.returncode
    if args.mutation in RECEIVER_CHECK_OPCODES:
        # 6F00 alone does not show which check fired: require the VM to
        # report SecurityException at the crafted invoke opcode.
        opcode = RECEIVER_CHECK_OPCODES[args.mutation]
        marker = re.compile(
            rf"VPCD:APPLET_PROCESS status=6F00 exception=29 opcode={opcode:02X}\b"
        )
        run_log = log.read_text(encoding="utf-8", errors="replace")
        if not marker.search(run_log):
            print(f"RECEIVER_CHECK:FAIL mutation={args.mutation} "
                  f"expected exception=29 opcode={opcode:02X}", file=sys.stderr)
            return 1
        print(f"RECEIVER_CHECK:PASS mutation={args.mutation} "
              f"exception=29 opcode={opcode:02X}")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
