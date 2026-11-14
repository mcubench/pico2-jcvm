# Handoff — next steps only

Updated: 2026-11-14.

## Current external test image
- ARM `max_crypto`, normal mode: object deletion ON; all debug/trace OFF; serial wait 0 ms; empty storage; no applet preinstalled.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-219c1d1f-dirty.uf2`
- UF2 SHA-256: `74b27919ac2165b750734eb2fae7bd29c87bdf536ee225fea8f896bdad5f20da`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-219c1d1f-dirty.zip`
- Package SHA-256: `8ea646a8e2d637f7b69f50ed1fc6e4a7a9dc343a61487a26f9772a9ce4c9218`
- External request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-219C1D1F` in `.agent/EXT_REQUESTS.md`.
- Source HEAD `219c1d1f62277bf3`; dirty source diff is disclosed in package provenance. ARM build, empty-card packaging, fixture manifest check, package checksums, ZIP integrity and local UF2 metadata passed. No physical flash/test was performed.

## Next actions
1. Await the external Windows result for this image. Record exact UF2 hash, Windows version, reader/ATR, chip ID, full per-device results and serial log.
2. MEM-01 T7 runtime VPCD proof and T8 physical acceptance remain open. Keep the unchanged 1.8.4/jc305 CAP and use the supplied 1.8.5 client for new JCAlgTest runs.
3. CAPIN-01 APDU-level PC VPCD validation passed: malformed final LOAD returned `6A80`; GET STATUS excluded it and retained an unrelated package; unrelated DELETE returned `9000`. Evidence is in `artifacts/logs/cap-missing-import-apdu-20261114T230132Z.log`. Physical acceptance remains open.

## Last hardware / PCSC state
- Approved helper granted ACLs for three USB nodes, but bounded `tools/list-readers` timed out at 12 s and 15 s. No usable reader inventory was obtained; do not attribute the nodes to JCVM boards.
- Do not repeat unbounded PCSC polling or claim physical acceptance without reader enumeration, identity, flash, and finite serial logs.
- `./tools/test-rp2350-fs-overlay` passed on current source with `TEST:SUMMARY fail=0`; fake-flash evidence only, not T7 client acceptance. `tools/list-readers --timeout 12` again timed out, so no JCAlgTest VPCD PC/SC campaign started.
- The wired `./tools/test-cap-malformed-vpcd --mutation external-superclass-without-import` entrypoint passed under elevated localhost-socket execution and runs raw GP LOAD/GET STATUS/DELETE VPCD validation. Earlier direct startup-path evidence remains in the preceding progress record.
