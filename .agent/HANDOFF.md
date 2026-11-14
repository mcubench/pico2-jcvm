# Handoff — next steps only

Updated: 2026-11-15.

## Current external test image
- ARM `max_crypto`, normal mode: object deletion ON; debug/trace OFF; serial wait 0 ms; empty card; no applet preinstalled.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-2ba049c8-dirty.uf2`
- UF2 SHA-256: `049b4eadf11a40ccae797a6564f4fb4baaeeb5eb017ec89bc8758497a4d2c5c5`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-2ba049c8-dirty.zip`
- Package SHA-256: `a60883c36effa810c54a392df1c7241ac03c3ecc332abbf0a95f4e888f8e3556`
- External request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-2BA049C8` in `.agent/EXT_REQUESTS.md`.
- Built from HEAD `2ba049c88e32c4eb`, worktree dirty; full tracked-diff and submodule identity are in package provenance. ARM build passes, firmware ends `0x100f7744` with 35,004 bytes remaining. Fixture verification, package hashes and ZIP integrity pass. No physical flash/test was performed.

## Next actions
1. Await the external Windows run. Match every result to the UF2 hash; collect Windows version, reader/ATR, chip ID, complete client results and serial log.
2. MEM-01 T7 VPCD and T8 physical acceptance remain open. Use the supplied 1.8.5 client with unchanged 1.8.4/jc305 CAP.
3. CAPIN-01 APDU-level PC VPCD validation passed: malformed LOAD returned `6A80`, GET STATUS excluded it while retaining the control package, and control DELETE returned `9000`. Physical acceptance remains open.

## Hardware access
- At 2026-11-15 00:51 CET, the approved helper granted ACLs for three USB nodes and `pcscd.service` was active, but elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 8` timed out. No usable reader inventory was obtained. Do not attribute USB nodes to JCVM boards without bounded enumeration.
- Do not claim physical acceptance without reader enumeration, identity, flash, and finite serial logs.
