#!/usr/bin/env python3
"""Exercise JCSystem transactions and Java exception reasons through VPCD."""

from __future__ import annotations

import argparse
import signal
import socket
import subprocess
import time
from pathlib import Path


AID = bytes.fromhex("4A43416C675465737431")


def receive_exact(connection: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        part = connection.recv(size - len(data))
        if not part:
            raise RuntimeError("VPCD peer closed the connection")
        data.extend(part)
    return bytes(data)


def receive_message(connection: socket.socket) -> bytes:
    size = int.from_bytes(receive_exact(connection, 2), "big")
    if size == 0:
        raise RuntimeError("VPCD peer sent an empty frame")
    return receive_exact(connection, size)


def send_message(connection: socket.socket, value: bytes) -> None:
    connection.sendall(len(value).to_bytes(2, "big") + value)


def exchange(connection: socket.socket, apdu: bytes, label: str) -> bytes:
    send_message(connection, apdu)
    response = receive_message(connection)
    if response[-2:] != b"\x90\x00":
        raise RuntimeError(f"{label}: SW={response[-2:].hex().upper()} data={response.hex().upper()}")
    print(f"TEST:APDU {label} response={response.hex().upper()}")
    return response[:-2]


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=float, default=90)
    parser.add_argument("--log", type=Path)
    parser.add_argument(
        "--case", type=int, action="append", choices=range(1, 79),
        help="run only this numbered APDU case; may be repeated",
    )
    parser.add_argument(
        "--restore-existing", action="store_true",
        help="reuse saved emulator memory and restore the installed applet",
    )
    parser.add_argument(
        "--memory", type=Path,
        default=repo / "artifacts/results/jcsystem-transaction-probe/FLASH_MEMORY",
        help="emulator flash image to initialize or restore",
    )
    parser.add_argument(
        "--install-params",
        help="hex bytes passed as the applet's proprietary install parameters",
    )
    parser.add_argument(
        "--expect-register-reason", type=int, choices=(0, 1, 4), default=0,
        help="expected explicit Applet.register(AID) failure reason reported by case 70",
    )
    args = parser.parse_args()
    build = repo / "build/jcsystem-transaction-probe"
    emulator = repo / "build/choupi-pc/choupi"
    memory = args.memory.resolve()
    cap_code = build / "load-code.bin"
    rom_mask = repo / "build/choupi-pc/flash"
    for path in (emulator, cap_code, rom_mask):
        if not path.is_file():
            raise SystemExit(f"required input is missing: {path}; run tools/build-jcsystem-transaction-probe")
    memory.parent.mkdir(parents=True, exist_ok=True)
    if args.restore_existing:
        if not memory.is_file() or memory.stat().st_size == 0:
            raise SystemExit(
                f"saved memory is missing or empty: {memory}; run an installing probe first"
            )
    else:
        memory.write_bytes(b"")
    log_path = args.log or (repo / "artifacts/logs/jcsystem-transaction-apdu-vpcd-20261016.log")
    log_path.parent.mkdir(parents=True, exist_ok=True)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(args.timeout)
        host, port = listener.getsockname()
        command = [
            str(emulator), "-m", str(memory), "-s", "--vpcd", f"{host}:{port}",
            "--applet-aid", AID.hex(), "--package-id", "63", "--cap-code",
            str(cap_code), "--rom-mask", str(rom_mask),
        ]
        if args.restore_existing:
            command.extend(("--class-index", "0"))
        else:
            command.append("--install")
            if args.install_params is not None:
                if len(args.install_params) % 2 or any(
                    char not in "0123456789abcdefABCDEF"
                    for char in args.install_params
                ):
                    raise SystemExit("--install-params must be even-length hex")
                command.extend(("--install-params", args.install_params))
        command.append("-s")
        started = time.monotonic()
        with log_path.open("w+b") as log:
            process = subprocess.Popen(
                command, cwd=repo, stdout=log, stderr=log,
                start_new_session=True,
            )
            connection = None
            try:
                deadline = time.monotonic() + args.timeout
                while connection is None:
                    if process.poll() is not None:
                        log.flush()
                        log.seek(0)
                        details = log.read().decode("utf-8", errors="replace")[-2000:]
                        raise RuntimeError(
                            f"emulator exited before VPCD connect with status {process.returncode}:\n{details}"
                        )
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("timed out waiting for emulator VPCD connect")
                    listener.settimeout(min(1.0, remaining))
                    try:
                        connection, _ = listener.accept()
                    except socket.timeout:
                        continue
                connection.settimeout(args.timeout)
                send_message(connection, b"\x01")
                send_message(connection, b"\x04")
                atr = receive_message(connection)
                if atr != bytes.fromhex("3B800181"):
                    raise RuntimeError(f"unexpected ATR {atr.hex().upper()}")
                print(f"TEST:PASS ATR response={atr.hex().upper()}")
                select = b"\x00\xA4\x04\x00" + bytes((len(AID),)) + AID + b"\x00"
                exchange(connection, select, "transaction probe SELECT")

                cases = (
                    (1, bytes.fromhex("0100"), "begin/depth/commit lifecycle"),
                    (2, b"\x02", "commit without transaction reason"),
                    (3, b"\x02", "abort without transaction reason"),
                    (4, bytes.fromhex("0100"), "nested begin reason and cleanup"),
                    (5, b"\x11", "abort restores persistent applet field"),
                    (7, b"\x11", "aborted value survives next APDU"),
                    (6, b"\x33", "commit preserves persistent applet field"),
                    (7, b"\x33", "committed value survives next APDU"),
                    (
                        8,
                        bytes.fromhex(
                            "BA7816BF8F01CFEA414140DE5DAE2223"
                            "B00361A396177A9CB410FF61F20015AD"
                        ),
                        "SHA-256 known-answer for abc",
                    ),
                    (
                        9,
                        bytes.fromhex(
                            "BA7816BF8F01CFEA414140DE5DAE2223"
                            "B00361A396177A9CB410FF61F20015AD"
                        ),
                        "SHA-256 update then doFinal state retention",
                    ),
                    (
                        10,
                        bytes(22),
                        "JCSystem scalar/extended memory overloads and commit capacity",
                    ),
                    (11, b"\x03", "unsupported initialized digest reports NO_SUCH_ALGORITHM"),
                    (12, b"\x32", "50 CryptoException throws reuse one instance and update reason"),
                    (
                        13,
                        bytes((3, 3, 2, 0, 1, 0, 0,
                               2, 2, 0, 2, 1, 3, 1, 3, 0)),
                        "OwnerPINx try limit and predecrement behavior",
                    ),
                    (14, b"\x01", "static reference identity after restore"),
                    (15, b"\x01", "persistent array child identity after restore"),
                    (16, b"\x01", "persistent object array-field alias identity and mutation"),
                    (17, b"\x01", "checkcast byte array restored from Object[]"),
                    (18, b"\x01", "mismatched array checkcast throws ClassCastException"),
                    (19, b"\x01", "null array access throws catchable NullPointerException"),
                    (20, b"\x01", "array access throws catchable ArrayIndexOutOfBoundsException"),
                    (21, b"\x01", "division by zero throws catchable ArithmeticException"),
                    (22, b"\x01", "negative array size throws catchable NegativeArraySizeException"),
                    (23, b"\x01", "reference array store throws catchable ArrayStoreException"),
                    (24, b"\x01", "JCSystem invalid memory type throws SystemException with reason"),
                    (25, b"\x01", "java.lang.Object root class can be instantiated"),
                    (26, b"", "APDU incoming state violation returns without sending on the invalid APDU state"),
                    (27, b"\x01", "caught APDUException preserves the ILLEGAL_USE reason"),
                    (28, b"\x01", "transient byte/object arrays preserve clear events and values"),
                    (29, b"\x01", "invalid transient array event throws SystemException.ILLEGAL_VALUE"),
                    (30, b"\x01", "prepare transient values for deselect lifecycle check"),
                    (31, b"\x03", "CLEAR_ON_DESELECT clears while CLEAR_ON_RESET survives applet reselection"),
                    (32, b"\x01", "prepare transient values for card reset check"),
                    (33, b"\x03", "card reset clears both CLEAR_ON_DESELECT and CLEAR_ON_RESET arrays"),
                    (34, b"\x01", "unsupported object deletion throws SystemException.ILLEGAL_USE"),
                    (35, b"\x01", "isAppletActive recognizes the current applet and rejects an unknown AID"),
                    (36, b"\x01", "global byte array access and CLEAR_ON_RESET classification"),
                    (37, b"\x03", "global array invalid type and negative size exceptions"),
                    (38, b"\x01", "global array references cannot be stored in instance fields"),
                    (39, b"\x01", "global array references cannot be stored in static fields"),
                    (40, b"\x01", "global array references cannot be stored in array components"),
                    (41, b"\x01", "applet objects cannot escape through global object arrays"),
                    (42, b"\x01", "ordinary byte arrays are not integrity-sensitive"),
                    (43, b"\x03", "assertIntegrity rejects unsupported arrays and null"),
                    (44, b"\x0f", "integrity-sensitive array creation validation and unsupported reason"),
                    (45, b"\x03", "clearArray rejects non-sensitive arrays and null"),
                    (46, b"\x01", "integrity-sensitive array support is reported as false"),
                    (47, b"\x03", "integrity-sensitive query handles instances and null"),
                    (48, b"\x03", "shareable lookup returns null for absent servers and the default applet method"),
                    (49, b"\x01", "transient allocation batch 1 is reclaimed after APDU"),
                    (50, b"\x01", "transient allocation batch 2 reuses reclaimed slots"),
                    (51, b"\x01", "transient allocation batch 3 keeps heap size bounded"),
                    (52, b"\x01", "unreferenced object batch 1 is reclaimed after APDU"),
                    (53, b"\x01", "unreferenced object batch 2 reuses instance slots"),
                    (54, b"\x01", "unreferenced object batch 3 keeps instance heap bounded"),
                    (55, b"", "reselect invokes applet deselect and select callbacks exactly once"),
                    (56, b"\x01", "rejected applet selection clears selection until a later successful select"),
                    (57, b"", "card reset deselects the active applet before reselection"),
                    (58, b"\x33", "committed persistent applet field survives emulator process restart"),
                    (59, b"\x01", "APDU.getCurrentAPDU returns the active process APDU object"),
                    (60, b"\x01", "APDU.getCurrentAPDU is unavailable during applet select callback"),
                    (61, b"\x01", "APDU.getCurrentAPDUBuffer returns the active process buffer"),
                    (62, b"\x01", "APDU.getCurrentAPDUBuffer is unavailable during applet select callback"),
                    (63, b"\x01", "APDU.waitExtension rejects no-chaining mode"),
                    (64, b"\x01", "APDU.waitExtension is accepted for normal outgoing mode"),
                    (65, b"\x01", "JCSystem.lookupAID returns only a matching installed AID"),
                    (66, b"\x0f", "JCSystem.lookupAID validates null and byte-array bounds"),
                    (67, b"\x04", "Applet.register() is rejected outside installation"),
                    (68, b"\x04", "Applet.register(AID) is rejected outside installation"),
                    (69, b"\x01", "Applet.register(AID) rejects an invalid AID length"),
                    (70, bytes((args.expect_register_reason,)), "Applet.register(AID) reports the expected install-time validation reason"),
                    (71, b"\x0b", "Applet.selectingApplet is true during select and process, false during deselect"),
                    (73, b"\x0b", "Applet.selectingApplet is false during deselect and resumes for the next selection"),
                    (74, b"", "unknown AID SELECT deselects the applet and returns file not found"),
                    (75, b"\x01", "AID byte-array equality returns false for null and compares valid full and partial AIDs"),
                    (76, b"\x01", "JCSystem.getAID is null before register and isAppletActive is true after register during install"),
                    (77, b"\x0f", "JCSystem.isAppletActive is true during install/select/deselect/process callbacks"),
                    (78, b"\x01", "UserException.throwIt is caught and preserves its reason"),
                )
                selected_cases = (
                    [case for case in cases if case[0] in args.case]
                    if args.case
                    else cases
                )
                for p1, expected, label in selected_cases:
                    if p1 == 77:
                        exchange(
                            connection,
                            bytes.fromhex("00A4040008A00000015100000000"),
                            "deselect applet while checking active state",
                        )
                        exchange(connection, select, "select applet for active lifecycle check")
                        observed = exchange(
                            connection,
                            bytes((0x80, 0x00, 77, 0x00, 0x00)),
                            label,
                        )
                        if observed != expected:
                            raise RuntimeError(
                                f"{label}: expected {expected.hex().upper()}, got {observed.hex().upper()}"
                            )
                        print(f"TEST:PASS {label}")
                        continue
                    if p1 == 74:
                        unknown_select = bytes.fromhex("00A4040007F000000001234500")
                        send_message(connection, unknown_select)
                        unknown_response = receive_message(connection)
                        if unknown_response != b"\x6a\x82":
                            raise RuntimeError(
                                f"{label}: expected 6A82, got {unknown_response.hex().upper()}"
                            )
                        print(f"TEST:APDU {label} response={unknown_response.hex().upper()}")
                        send_message(connection, bytes((0x80, 0x00, 55, 0x00, 0x00)))
                        inactive_response = receive_message(connection)
                        if inactive_response != b"\x69\x99":
                            raise RuntimeError(
                                "APDU after unknown AID SELECT was dispatched: "
                                f"{inactive_response.hex().upper()}"
                            )
                        print(f"TEST:PASS {label}")
                        continue
                    if p1 == 71:
                        exchange(
                            connection,
                            bytes((0x80, 0x00, 72, 0x00, 0x00)),
                            "reset selectingApplet lifecycle flags",
                        )
                        exchange(
                            connection,
                            bytes.fromhex("00A4040008A00000015100000000"),
                            "select the ISD to trigger applet deselection",
                        )
                        exchange(connection, select, "select applet for selectingApplet check")
                        observed = exchange(
                            connection,
                            bytes((0x80, 0x00, 71, 0x00, 0x00)),
                            "read selectingApplet lifecycle flags",
                        )
                        if observed != expected:
                            raise RuntimeError(
                                f"{label}: expected {expected.hex().upper()}, got {observed.hex().upper()}"
                            )
                        print(f"TEST:PASS {label}")
                        continue
                    if p1 == 73:
                        exchange(
                            connection,
                            bytes((0x80, 0x00, 72, 0x00, 0x00)),
                            "reset selectingApplet lifecycle flags",
                        )
                        exchange(
                            connection,
                            bytes.fromhex("00A4040008A00000015100000000"),
                            "select the ISD to trigger applet deselection",
                        )
                        exchange(connection, select, "reselect applet after ISD")
                        observed = exchange(
                            connection,
                            bytes((0x80, 0x00, 71, 0x00, 0x00)),
                            "read selectingApplet lifecycle flags",
                        )
                        if observed != expected:
                            raise RuntimeError(
                                f"{label}: expected {expected.hex().upper()}, got {observed.hex().upper()}"
                            )
                        print(f"TEST:PASS {label}")
                        continue
                    if p1 == 57:
                        before = exchange(
                            connection,
                            bytes((0x80, 0x00, 55, 0x00, 0x00)),
                            "read applet lifecycle callback counts before reset",
                        )
                        if len(before) != 4:
                            raise RuntimeError(
                                f"callback state before reset: expected 4 bytes, got {before.hex().upper()}"
                            )
                        send_message(connection, b"\x02")
                        exchange(connection, select, "select applet after card reset")
                        after = exchange(
                            connection,
                            bytes((0x80, 0x00, 55, 0x00, 0x00)),
                            "read applet lifecycle callback counts after reset",
                        )
                        expected_counts = bytes(((before[0] + 1) & 0xff,
                                                 (before[1] + 1) & 0xff))
                        if (
                            len(after) != 4
                            or after[:2] != expected_counts
                            or after[2] != 0
                            or after[3] != 0
                        ):
                            raise RuntimeError(
                                f"{label}: expected callback state {expected_counts.hex().upper()}0000, got {after.hex().upper()}"
                            )
                        print(f"TEST:PASS {label}")
                        continue
                    if p1 == 56:
                        exchange(
                            connection,
                            bytes((0x80, 0x00, p1, 0x00, 0x00)),
                            "arm applet to reject next selection",
                        )
                        send_message(connection, select)
                        rejected = receive_message(connection)
                        if rejected[-2:] != b"\x69\x99":
                            raise RuntimeError(
                                f"rejected SELECT: expected SW=6999, got {rejected.hex().upper()}"
                            )
                        print(f"TEST:APDU rejected SELECT response={rejected.hex().upper()}")
                        send_message(
                            connection,
                            bytes((0x80, 0x00, 55, 0x00, 0x00)),
                        )
                        not_selected = receive_message(connection)
                        if not_selected[-2:] != b"\x69\x99":
                            raise RuntimeError(
                                "APDU after rejected SELECT was dispatched: "
                                f"{not_selected.hex().upper()}"
                            )
                        print(f"TEST:APDU APDU after rejected SELECT response={not_selected.hex().upper()}")
                        exchange(connection, select, "select applet after rejected selection")
                        reselection_state = exchange(
                            connection,
                            bytes((0x80, 0x00, 55, 0x00, 0x00)),
                            "read selection state after rejected reselect",
                        )
                        if (
                            len(reselection_state) != 4
                            or reselection_state[2] != 0
                            or reselection_state[3] != 0
                        ):
                            raise RuntimeError(
                                "reselection flag persisted after a rejected select: "
                                f"{reselection_state.hex().upper()}"
                            )
                        print(f"TEST:PASS {label}")
                        continue
                    if p1 == 31:
                        exchange(connection, select, "reselect applet for transient lifecycle check")
                    if p1 == 33:
                        send_message(connection, b"\x02")
                        exchange(connection, select, "reselect applet after card reset")
                    if p1 == 55:
                        before = exchange(
                            connection,
                            bytes((0x80, 0x00, p1, 0x00, 0x00)),
                            "read applet lifecycle callback counts before reselect",
                        )
                        if len(before) != 4:
                            raise RuntimeError(
                                f"callback state before reselect: expected 4 bytes, got {before.hex().upper()}"
                            )
                        exchange(connection, select, "reselect applet for callback check")
                        after = exchange(
                            connection,
                            bytes((0x80, 0x00, p1, 0x00, 0x00)),
                            "read applet lifecycle callback counts after reselect",
                        )
                        expected_counts = bytes(((before[0] + 1) & 0xff,
                                                 (before[1] + 1) & 0xff))
                        if (
                            len(after) != 4
                            or after[:2] != expected_counts
                            or after[2] != 1
                            or after[3] != 1
                        ):
                            raise RuntimeError(
                                f"{label}: expected callback state {expected_counts.hex().upper()}0101, got {after.hex().upper()}"
                            )
                        print(f"TEST:PASS {label}")
                        continue
                    data = exchange(
                        connection,
                        bytes((0x80, 0x00, p1, 0x00, 0x00)),
                        label,
                    )
                    if data != expected:
                        raise RuntimeError(
                            f"{label}: expected {expected.hex().upper()}, got {data.hex().upper()}"
                        )
                    print(f"TEST:PASS {label}")
                print(f"TEST:SUMMARY pass={len(selected_cases) + 2} fail=0 elapsed_s={time.monotonic() - started:.3f}")
            finally:
                if connection is not None:
                    connection.close()
                if process.poll() is None:
                    try:
                        # Let the VPCD EOF path return through starting_jcre()
                        # so --save writes flash before this emulator exits.
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.send_signal(signal.SIGTERM)
                        try:
                            process.wait(timeout=3)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait(timeout=2)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"TEST:FAIL {error}")
        raise SystemExit(1)
