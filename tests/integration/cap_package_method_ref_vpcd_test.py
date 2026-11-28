#!/usr/bin/env python3
"""Review finding #3: LOAD rejects package-private ROM method references.

Package-visible virtual method tokens (bit 0x80) are valid only inside their
own package (JCVMS 3.0.5 sections 4.3.7.6 and 6.8.2). The test sets that bit
on one external VirtualMethodref of the transaction probe and expects LOAD to
fail. Build the max_crypto PC VM first.
"""

from __future__ import annotations

import os
import socket
import subprocess
import sys
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/integration"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402

CONNECT_TIMEOUT = float(os.environ.get("CAP_PACKAGE_METHOD_CONNECT_TIMEOUT_S", "45"))
APDU_TIMEOUT = float(os.environ.get("CAP_PACKAGE_METHOD_APDU_TIMEOUT_S", "60"))
CONSTANT_POOL = "algtest/javacard/ConstantPool.cap"
VIRTUAL_METHODREF = 6


def mark_package_method_ref(source: Path, target: Path) -> int:
    """Copy the CAP, setting bit 0x80 on the first external virtual token."""
    with ZipFile(source) as cap:
        entries = [(info, cap.read(info.filename)) for info in cap.infolist()]
    pool = bytearray(next(data for info, data in entries
                          if info.filename == CONSTANT_POOL))
    count = int.from_bytes(pool[3:5], "big")
    for index in range(count):
        entry = 5 + index * 4
        external = pool[entry + 1] & 0x80
        if pool[entry] == VIRTUAL_METHODREF and external and pool[entry + 3] < 0x80:
            pool[entry + 3] |= 0x80
            break
    else:
        raise RuntimeError("no external VirtualMethodref in the probe constant pool")
    with ZipFile(target, "w", ZIP_DEFLATED) as out:
        for info, data in entries:
            out.writestr(info, bytes(pool) if info.filename == CONSTANT_POOL else data)
    return index


@contextmanager
def empty_card_session(log_name: str):
    """Start the PC VM on an empty card and yield an authenticated SCP03."""
    vpcd.ensure_algtest_load_code(ROOT)
    emulator = ROOT / "build/choupi-pc/choupi"
    rom_mask = ROOT / "build/choupi-pc/flash"
    dummy = ROOT / "build/tests/jcalgtest/algtest-v184-load-code.bin"
    for required in (emulator, rom_mask, dummy):
        if not required.is_file():
            raise RuntimeError(f"required PC build artifact missing: {required}")
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    memory = ROOT / f"build/{log_name}.bin"
    memory.write_bytes(b"")
    Path(f"{memory}.cap-registry").unlink(missing_ok=True)
    log_path = ROOT / f"artifacts/logs/{log_name}-{timestamp}.log"
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
        try:
            connection, _ = vpcd.accept_vpcd(listener, process, CONNECT_TIMEOUT,
                                             log_path, log_name)
            connection.settimeout(APDU_TIMEOUT)
            vpcd.send_message(connection, b"\x01")
            vpcd.send_message(connection, b"\x04")
            if vpcd.receive_message(connection) != bytes.fromhex("3B800181"):
                raise RuntimeError("unexpected VPCD ATR")
            vpcd.exchange(connection, bytes.fromhex("00A4040000"), "SELECT ISD")
            scp = vpcd.Scp03(connection)
            scp.authenticate()
            yield connection, scp
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()


def main() -> int:
    subprocess.run([str(ROOT / "tools/build-jcsystem-transaction-probe")],
                   cwd=ROOT, check=True, timeout=300)
    probe = ROOT / "build/jcsystem-transaction-probe/output/JCTransactionProbe.cap"
    patched = ROOT / "build/jcsystem-transaction-probe/package-method-ref.cap"
    index = mark_package_method_ref(probe, patched)
    code, package_aid, _ = vpcd.load_code(patched)
    with empty_card_session("cap-package-method-ref-vpcd") as (_, scp):
        try:
            vpcd.load_package(scp, code, package_aid, (0x9000, 0x6310))
        except RuntimeError as rejected:
            print(f"TEST:PASS LOAD rejected constant-pool entry {index} "
                  f"with bit 0x80: {rejected}")
            print("TEST:SUMMARY pass=1 fail=0")
            return 0
    print(f"TEST:FAIL LOAD accepted constant-pool entry {index} that "
          "names a package-private method of another package")
    print("TEST:SUMMARY pass=0 fail=1")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
