#!/usr/bin/env python3
"""Exercise dynamic-package opcode rejection through the real PC VM."""

from __future__ import annotations

import hashlib
import argparse
import subprocess
import sys
from pathlib import Path
from fixture_paths import fixture_path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
CAP_PATH = fixture_path("jcalgtest.cap", ROOT)
EXPECTED_CAP_SHA256 = (
    "cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad"
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
    # Method follows Header, Directory, Applet, Import, ConstantPool, Class.
    method_start = sum(len(parts[name]) for name in
                       ("Header", "Directory", "Applet", "Import", "ConstantPool", "Class"))
    code_index = method_start + bytecode_index
    mutable = bytearray(ordered)
    if mutable[code_index] != method_component[bytecode_index]:
        raise ValueError("assembled Method component offset mismatch")
    if mutation in ("impdep1", "impdep2"):
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
    else:
        raise ValueError(f"unsupported mutation: {mutation}")
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
        "--mutation", choices=("impdep1", "impdep2", "truncated-sipush",
                                "fall-through",
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
                                "switch-into-operand"),
        default="impdep1",
    )
    args = parser.parse_args()
    code = build_malformed_load_code(args.mutation)
    log = ROOT / f"artifacts/logs/rv28-malformed-{args.mutation}-vpcd-20261106.log"
    select_status = "9000" if args.mutation in (
        "truncated-sipush", "fall-through", "exception-catch-cp-out-of-range",
        "exception-handler-target-into-operand",
        "exception-handler-range-into-operand",
        "exception-handler-range-out-of-bounds",
        "exception-handler-range-start-out-of-bounds",
    ) else "6F00"
    process_status = "6F00" if args.mutation in (
        "truncated-sipush", "fall-through", "exception-catch-cp-out-of-range",
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
        "--expect-select-status", select_status,
        "--apdu", "B0E1000000", "--expect-apdu-response", process_status,
        "--apdu", "00A4040008A00000015100000000",
        "--expect-apdu-response", "6F0A8408A0000001510000009000",
        "--apdu", "00A404000A" + APPLET_AID.hex() + "00",
        "--expect-apdu-response", select_status,
        "--apdu", "00A4040008A00000015100000000",
        "--expect-apdu-response", "6F0A8408A0000001510000009000",
        "--log", str(log),
    ]
    result = subprocess.run(
        [str(ROOT / "tools/test-pc-vpcd-applet"), *commands],
        cwd=ROOT,
        check=False,
    )
    if result.returncode != 0:
        print(f"MALFORMED_CAP:FAIL vpcd_exit={result.returncode}", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
