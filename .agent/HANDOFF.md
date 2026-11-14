# Handoff — next steps only

Updated: 2026-11-15.

## Current external test image
- ARM `max_crypto`, normal mode: object deletion ON; all debug/trace OFF; serial wait 0 ms; empty storage; no applet preinstalled.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-7a396462-dirty.uf2`
- UF2 SHA-256: `135d5d3a64c90572bc0a6166828cbde00c3b1705292f8fd32e1d6984bfc99dc3`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-7a396462-dirty.zip`
- Package SHA-256: `ea7f322fcbd77c96e44914203dd55ad1194a03b670230de103dad3e9b66fa5c4`
- External request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-7A396462` in `.agent/EXT_REQUESTS.md`.
- Source HEAD `7a396462e1b5e60ae44e5ca7094de2656fe74dfa`; tracked dirty diff and CHOUPI OS submodule are disclosed in package provenance. ARM `max_crypto` build, object-deletion ON/debug OFF provenance, empty-card packaging, fixture verification, checksums, ZIP integrity and local UF2 metadata passed. No physical flash/test was performed.

## Next actions
1. Await the external Windows result for this image. Record exact UF2 hash, Windows version, reader/ATR, chip ID, full per-device results and serial log.
2. MEM-01 T7 runtime VPCD proof and T8 physical acceptance remain open. Keep the unchanged 1.8.4/jc305 CAP and use the supplied 1.8.5 client for new JCAlgTest runs.
3. CAPIN-01 APDU-level PC VPCD validation passed: malformed final LOAD returned `6A80`; GET STATUS excluded it and retained an unrelated package; unrelated DELETE returned `9000`. Evidence is in `artifacts/logs/cap-missing-import-apdu-20261114T230132Z.log`. Physical acceptance remains open.

## Last hardware / PCSC state
- Approved helper granted ACLs for three USB nodes; elevated `tools/list-readers --timeout 8` timed out, as did `timeout 12s pcsc_scan -r` with no output. No usable PC/SC reader inventory was obtained; do not attribute the USB nodes to JCVM boards.
- Do not repeat unbounded PCSC polling or claim physical acceptance without reader enumeration, identity, flash, and finite serial logs.
- `./tools/test-rp2350-fs-overlay` passed on current source with `TEST:SUMMARY fail=0`; fake-flash evidence only, not T7 client acceptance. `tools/list-readers --timeout 12` again timed out, so no JCAlgTest VPCD PC/SC campaign started.
- The wired `./tools/test-cap-malformed-vpcd --mutation external-superclass-without-import` entrypoint passed under elevated localhost-socket execution and runs raw GP LOAD/GET STATUS/DELETE VPCD validation. Earlier direct startup-path evidence remains in the preceding progress record.
