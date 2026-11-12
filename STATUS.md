# JCVM project status

Updated: 2026-11-12 16:18 UTC

## Goal

Complete the RP2350 Pico 2 Java Card VM: interpret unchanged Java Card CAPs,
provide native Java Card API and cryptography, support GlobalPlatform
installation and PC/SC, preserve card state, and verify the end-to-end workflow
on real hardware and Windows. The project is **not complete**; see [TODO.md](TODO.md)
for the acceptance backlog and [.agent/HANDOFF.md](.agent/HANDOFF.md) for the
current physical-test handoff.

## Current build and source

- CAPIN-01: GP LOAD now rejects a package AID that differs from the CAP Header
  before cleanup, slot allocation or persistence. The VPCD regression fails
  before the fix and passes after it; unchanged JCAlgTest install/select,
  CAP upgrade and 81-file fixture verification pass. ARM/RISC-V builds pass.
  Other CAPIN-01 structural/dependency checks and physical acceptance remain.
  Required Import, Static Field and Reference Location presence is now enforced
  before package storage. Omission regressions pass for all three. The Reference
  Location case returned `6A80` on physical D92; a following valid unchanged
  JCAlgTest 1.8.4 install and SELECT succeeded. External class superclass and
  interface package tokens are now range-checked against Import before storage;
  the malformed `FF00` superclass reference is rejected in VPCD. ARM and
  RISC-V builds pass. INSTALL now validates the full reachable dynamic-package
  dependency closure; a VPCD regression rejects a transitive missing library
  import with `6A88`, while valid external-interface install remains `9000`.
  Export-table consistency beyond resolved references, legacy CAP null guards
  and full physical acceptance remain open.

- Fresh external-test ARM image: [empty-card max_crypto with object deletion enabled and debug tracing disabled](artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-no-debug-empty-card-580f3f40.uf2). SHA-256 `f17a1fae0c0105475092d81595d2899eb671f8b9686656267c5d2137709ab4b7`, 4,099,072 bytes; firmware-only UF2 hash `f1d71864a0620ee28a1c4a93b1ff3dd768c9c30757227c87c83873963713ce52`. Source HEAD `580f3f40`, dirty tree; CPLC source ID `580F3F409C898F6E`. ARM/RISC-V max_crypto builds pass with object deletion ON and debug switches OFF; `./tools/test-no-debug-traces` passes. Firmware ends at `0x100f4460`, leaving 48,032 bytes in the 1 MiB firmware window. The package erases persistent storage and has no preinstalled applet; it was not flashed locally. Windows instructions and hash are in `.agent/EXT_REQUESTS.md`.
  The matching firmware-only UF2 was flashed by exact chip ID to D92
  (`D92E743D5F640C0F`), 8E (`8E464F4E2BBC0A19`) and 2BED
  (`2BED26ED20556535`) using the CDC-1200 BOOTSEL fallback (4/3/3 seconds).
  All three returned `FLASH:PASS`; their finite serial checks passed. PC/SC
  listed all three and GPPro `--list` succeeded on D92, showing the preserved
  JCAlgTest package/applet. Per-board logs are in
  `artifacts/results/maxcrypto-gc-nodebug-flash-20261112-current/`. The user
  then disconnected the physical devices; Windows validation remains open.

- ARM and RISC-V `max_crypto` wrappers pass with object deletion enabled and
  heap-GC debug disabled. ARM firmware ends at `0x100F474C`, leaving 47,284
  bytes before the configured `0x10100000` limit.
- Current dirty-tree ARM CAPIN test image on D92 is `build/arm/pico2_choupi.uf2`
  (SHA-256 `75741e95b27dcce01e069623255371c526655c3f0e3209b51384684e42291a8d`,
  CPLC source ID `93D38F8AF8CF06EF`). Its malformed Reference Location LOAD
  test returned `6A80`; unchanged JCAlgTest 1.8.4 installation and applet
  selection passed. Physical test logs are under
  `artifacts/results/capin-required-component-20261112/D92E743D5F640C0F/`.
- The current diagnostic empty-card image is
  `build/arm/pico2-jcvm-max_crypto-object-deletion-ccid-trace-empty-card-48e9a50b.uf2`
  (4,100,608 bytes, SHA-256
  `32e4977ed65bc3087fbf5e2c7de8f5066c62c89dacb9b2a9f98b37c3b2a2124e`). It
  enables object deletion and the five CCID response-path markers, keeps
  per-object heap-GC tracing disabled, and has erased persistent storage. CPLC
  source ID is `48E9A50BB405634E`.
- The diagnostic image is built from a dirty worktree containing unrelated
  pre-existing changes. It is for diagnosis, not a clean release artifact.
- Commit `028ba49a` separates CCID markers from verbose heap-GC tracing; both
  architecture wrappers pass. Focused CCID tests remain 21/21 and 17/17. The
  current image has not been flashed.
- Current-tree persistent-transaction, persistent-inventory and RP2350
  filesystem-overlay suites pass (`TEST:SUMMARY fail=0`); overlay tests include
  modeled power cuts and full-index compaction. This is host-only evidence.
- Current shared-tree object-deletion VPCD runs pass for three byte-array
  deletion cycles; RSA public-key replacement at 512, 2048 and 4096 bits; and
  a live package-static RSA root. Across the RSA cases 50 applet-owned records
  were reclaimed, while the live-static case confirmed the root survived until
  it was cleared. These runs use the PC VPCD backend and are not physical
  acceptance.
- Current shared-tree RMI registry tests pass: `./tools/test-rmi-registry`
  (including cycles, diamonds, peer static/proxy roots, fail-closed ambiguous
  roots and transient-array lifecycle) and `./tools/test-rmi-cap-metadata`.

## Hardware and current evidence

- At the latest physical check, the identity-pinned CDC-1200 path entered
  BOOTSEL on D92, 8E and 2BED in 4/3/3 seconds. Firmware-only
  max_crypto/object-deletion images flashed and verified on all three, preserving
  card stores. All finite serial monitors passed. After the approved helper
  refreshed USB node access, PC/SC listed all three. GPPro `--list` on D92
  succeeded and showed the preserved JCAlgTest package/applet. The user has
  since physically disconnected the boards; no further hardware operation is
  authorized until they report the boards available.
- The corrected D92 RESOURCE-01 all-algorithms run produced 51 support rows
  before `SCARD_E_NOT_TRANSACTED` on `B0 75 12 00 01 16 00`. Its last serial
  line is `APDU_PROCESS stage=end sw=9000 len=29`, printed immediately before
  the applet function returns. The old firmware lacked the later CCID-caller
  marker, so whether control returned to that caller is unproven. New telemetry
  is ready to locate the stall; it has not been observed on hardware.
- Earlier 8E all-algorithms testing had heap/body read failures and an OOM
  panic. D92 performance-static and other all-algorithms attempts were
  interrupted or failed with PC/SC errors. Neither long option-1 nor option-2
  campaign completed; these are not acceptance passes.
- The 2026-11-12 option-2 retry completed 26 measurements on 8E and 32 on D92
  before PC/SC communication/reader errors during DES tests. The 8E attempt
  overlapped a `pcscd` restart from the authorized helper; D92 did not. Serial
  logs show no `FAULT` or `6F00`, but neither run completed or produced an
  object-deletion event. Keep these as partial evidence only.
- The fresh external-test empty-card image and hash are in
  `.agent/EXT_REQUESTS.md`. Its packaged empty-storage image was not flashed
  locally; the matching firmware-only payload was flashed while preserving
  each local card store.
- Per-board logs and current image measurements:
  `artifacts/results/maxcrypto-gc-nodebug-flash-20261112-current/`. Other
  physical acceptance remains open.

## Highest-priority remaining work

1. **BOOTSEL-01 (P0):** offline firmware integration is implemented. The SDK
   vendor reset descriptor/driver is interface 3, and CDC-1200 reset servicing
   is deferred to cooperative points. Reset/CCID unit coverage passes 26/26,
   identity-pinned target selection passes 11/11, and ARM/RISC-V builds pass.
   Idle CDC reset/re-enumeration passed on three boards in 3–4 seconds. Still
   needed: direct vendor-reset validation and reset during long synchronous
   applet work. The boards are currently disconnected.
2. **RESOURCE-01 / RESOURCE-02:** flash the quiet CCID trace image after a guest-visible
   BOOTSEL interface returns, localize the post-applet response stall, fix
   remaining reclamation/resource failures, then complete unchanged JCAlgTest
   1.8.4/jc305 options 2 and 1 on identified physical devices with separate
   telemetry.
3. **HOST-02 / CTX-01/03:** finish Windows cold-reconnect measurements and SIO,
   firewall and cross-context acceptance.
4. **STORE/PERSIST, API, GP and AC:** complete storage, power-loss, current-image
   CAP coverage, installation/selection and end-to-end acceptance.
5. **HMAC and CAPIN:** complete support acceptance and the remaining CAP input
   validation/security cases listed in [TODO.md](TODO.md).

## Acceptance

Project acceptance remains open. Selected host tests and earlier physical
firmware/APDU checks pass, but current physical PC/SC access is unavailable,
the latest trace image has not been flashed, both long JCAlgTest campaigns are
incomplete, and Windows end-to-end and cold-power acceptance are not complete.
Do not infer completion from a successful build, USB enumeration, or startup
heartbeat.
