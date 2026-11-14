# Handoff — next steps only

Updated: 2026-11-14.

## Current external image
- Fresh ARM `max_crypto`, normal mode: object deletion ON, all debug/trace
  options OFF, serial wait 0 ms, empty storage, no preinstalled applet.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-50114a30-dirty.uf2`
- UF2 SHA-256: `30dcd9f581cdea7729f66e9c08c832438f8d01ff32c3e56c37bbaae45d46287f`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-50114a30-dirty.zip`
- Package SHA-256: `2d3735a37acd34d81620e893c79071b533fbeec3ccc13c2c47e526a93338ea44`
- Request: `WIN-OBJECT-DELETION-NORMAL-DIRTY-50114A30` in `.agent/EXT_REQUESTS.md`.
- Built from current dirty worktree; packaged provenance records root tracked-diff
  hash and CHOUPI OS submodule state. Package integrity and fixture manifest
  verification passed. No physical hardware validation was performed.

## Next actions
1. CAPIN-01: run the new `external-superclass-without-import` mutation through
   `tools/test-cap-malformed-vpcd`; expect LOAD `6A80`, no stored package, and
   unrelated package deletion still `9000`. Fixture generation passes offline.
2. MEM-01 T7 remains open: complete unchanged JCAlgTest option-2 proof on the
   RP2350 overlay backend through sync sequence 2,500, with live-store,
   compaction, alias and RAM-handle metrics. User previously stopped long
   emulator runs; do not resume them without direction.
3. Await Windows results for the image listed above. For physical work, refresh
   helper and reader inventory with repository wrappers; current bounded PC/SC
   listings timed out despite three ACL-granted USB nodes.

## Latest reader observation
- Approved helper granted ACLs for three USB nodes (`/dev/bus/usb/001/074`,
  `/001/078`, `/001/077`), but bounded `tools/list-readers` timed out after
  12 seconds and again after 15 seconds. No inventory was available; do not
  claim these nodes are JCVM readers. Continue offline and avoid repeated
  polling or `pcsc_scan`.
- `./tools/test-rp2350-fs-overlay` passed on the current source with
  `TEST:SUMMARY fail=0`; this remains fake-flash evidence and does not close
  MEM-01 T7 runtime acceptance.
- The generated CAPIN-01 fixture contains unchanged CAP data except for the
  omitted Import component; the upstream CAP hash remains pinned. No VPCD
  acceptance run has yet verified its APDU response or package-store state.

## Workspace notes
- Preserve all pre-existing dirty files. At build time there were unrelated
  active changes in TODO, review probes, CHOUPI, tests and tool wrappers.
- Avoid `pcsc_scan`; it has hung and consumed CPU in this VM. Use bounded
  `tools/list-readers` and the approved PC/SC helper only.
- Do not claim physical acceptance without exact reader enumeration, board
  identity, flash, and finite serial-log evidence.
