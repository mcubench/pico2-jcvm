#!/usr/bin/env python3
"""Review finding #17: LOAD initializes a package's static arrays.

The tables package has no applet and is only loaded. Its StaticField
component carries an array_init entry for Tables.TABLE. The client package,
installed afterwards, must read TABLE[1] == 0x22. Build the max_crypto PC VM
first.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/integration"))
import pc_vpcd_multipackage_test as vpcd  # noqa: E402
from cap_package_method_ref_vpcd_test import empty_card_session  # noqa: E402

BUILD = ROOT / "build/load-only-static-fixture"
TABLES_AID = bytes.fromhex("A00000015100000A")


def main() -> int:
    subprocess.run([str(ROOT / "tools/build-load-only-static-fixture")],
                   cwd=ROOT, check=True, timeout=300)
    tables_cap = BUILD / "tables-cap/lostatic/tables/javacard/tables.cap"
    client_cap = BUILD / "client-cap/lostatic/client/javacard/client.cap"
    with empty_card_session("load-only-static-vpcd") as (connection, scp):
        # load_code() requires an Applet component, so pack the library here.
        with ZipFile(tables_cap) as cap:
            folder = "lostatic/tables/javacard/"
            tables_code = b"".join(
                cap.read(folder + name + ".cap") for name in vpcd.COMPONENTS
                if folder + name + ".cap" in cap.namelist())
        vpcd.load_package(scp, tables_code, TABLES_AID)
        code, package_aid, applets = vpcd.load_code(client_cap)
        vpcd.load_package(scp, code, package_aid)
        vpcd.install_applet(scp, package_aid, applets[0], applets[0])
        select = bytes((0x00, 0xA4, 0x04, 0x00, len(applets[0]))) + applets[0]
        response = vpcd.exchange(connection, select, "SELECT client")
        if response[-2:] != b"\x90\x00":
            raise RuntimeError(f"SELECT client failed: {response.hex().upper()}")
        response = vpcd.exchange(connection, bytes.fromhex("8010000000"),
                                 "read TABLE[1] of the LOAD-only package")
    if response.hex().upper() == "229000":
        print("TEST:PASS LOAD initialized the library's static array")
        print("TEST:SUMMARY pass=1 fail=0")
        return 0
    print("TEST:FAIL client read of the LOAD-only package's static array "
          f"returned {response.hex().upper()}, expected 229000 (EE = null)")
    print("TEST:SUMMARY pass=0 fail=1")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
