# Handoff — next steps only

Updated: 2026-11-15.

## Current external test image
- ARM `max_crypto`, normal mode: object deletion ON; debug/trace OFF; serial wait 0 ms; empty card; no applet preinstalled.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-c3309a61-dirty.uf2`
- UF2 SHA-256: `12fc079ec6927e13aad2cc59acf41adade7cfc955f4bdceb81cf9cfdb41f679d`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-c3309a61-dirty.zip`
- Package SHA-256: `c543d8e739b6e5d73e954e95e0d36dff431f27f00abdfdb934a786e14d79f9c5`
- External request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-C3309A61` in `.agent/EXT_REQUESTS.md`.
- Built from HEAD `c3309a61987c674c`, worktree dirty; package provenance records tracked diff and submodule identities. ARM build passes, firmware ends `0x100f7744` with 35,004 bytes remaining. Fixture verification and ZIP integrity pass. No physical flash/test was performed.

## Next actions
1. Await the external Windows run. Match every result to the UF2 hash; collect Windows version, reader/ATR, chip ID, complete client results and serial log.
2. MEM-01 T7 VPCD and T8 physical acceptance remain open. Use the supplied 1.8.5 client with unchanged 1.8.4/jc305 CAP.
   The offline overlay heap graph passes (68.6% live occupancy, 12 compactions, 309 peak handles), but its vector roots do not satisfy the optional heap-inventory diagnostic static-root threshold; see QKB before changing diagnostics.
3. CAPIN-01 APDU-level PC VPCD validation passed: malformed LOAD returned `6A80`, GET STATUS excluded it while retaining the control package, and control DELETE returned `9000`. Physical acceptance remains open.

## Hardware access
- At 2026-11-15 00:51 CET, the approved helper granted ACLs for three USB nodes and `pcscd.service` was active, but elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 8` timed out. No usable reader inventory was obtained. Do not attribute USB nodes to JCVM boards without bounded enumeration.
- Do not claim physical acceptance without reader enumeration, identity, flash, and finite serial logs.
