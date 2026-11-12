# JCVM project status

Updated: 2026-11-12 13:57 UTC

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
  JCAlgTest 1.8.4 install and SELECT succeeded. Other dependency validation and
  full physical acceptance remain open.

- Fresh external-test ARM image: [empty-card max_crypto with object deletion enabled and debug tracing disabled](artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-no-debug-empty-card-31cb571c.uf2). SHA-256 `b57c7b07b0c0673b8ae7d3ba0a8de5c8f79290e16d70b63cf2241aef754b0e60`, 4,098,048 bytes. ARM and RISC-V wrappers pass with `object_deletion=ON` and heap-GC, CCID-applet and persistent-GC debug switches OFF; RISC-V builds the VM core only. The build from source HEAD `31cb571c` uses a dirty worktree and CPLC source ID `31CB571C064FA40F`. `./tools/test-no-debug-traces` confirms the ARM ELF omits verbose APDU/persistent-reference trace strings while retaining concise object-deletion events. The image clears persistent storage and has no preinstalled applet. Firmware SHA-256 is `7e477e96b137e40aef71c15134fcc5625bd7f1c5bda4177376d8f974dedc10ee`; this is not a release build.
  The prior `ed5716bb` package is superseded because it retained those verbose
  traces despite the debug switches being off.
  Correction: On 2026-11-12 the image was flashed through the identity-pinned
  wrapper to D92 (`D92E743D5F640C0F`), 8E (`8E464F4E2BBC0A19`) and 2BED
  (`2BED26ED20556535`), each with `FLASH:PASS`. All three finite serial monitors
  passed `TEST:PASS choupi_runtime`; exact-reader GPPro `--list` showed only the
  ISD, confirming the cards are empty. Per-device logs:
  `artifacts/results/maxcrypto-gc-nodebug-flash-20261112/`. Windows validation
  remains open.

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

- Three Pico USB devices are attached to the guest and now boot as runtime
  `2e8a:0009`. All were positively enumerated in BOOTSEL before flashing and
  subsequently identified under PC/SC as D92, 8E and 2BED. All three received
  the no-debug `max_crypto` object-deletion image and passed finite runtime and
  GPPro empty-card checks (evidence above).
- The approved helper granted two runtime USB nodes, while `pcsc_scan -r`
  listed only VPCD readers. Physical APDU testing is currently unavailable.
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
- The user reports all three tokens are now in BOOTSEL. The latest guest USB
  enumeration still showed three runtime `2e8a:0009` devices and no BOOTSEL
  `2e8a:000f`; after the latest test failure, PC/SC listed only D92. Do not
  flash until the guest can confirm the target's BOOTSEL identity.
- The 2026-11-12 option-2 retry completed 26 measurements on 8E and 32 on D92
  before PC/SC communication/reader errors during DES tests. The 8E attempt
  overlapped a `pcscd` restart from the authorized helper; D92 did not. Serial
  logs show no `FAULT` or `6F00`, but neither run completed or produced an
  object-deletion event. Keep these as partial evidence only.
- The current external-test image was refreshed after the debug-off build was
  found to contain verbose APDU and persistent-reference trace strings. The
  new artifact and hash are in `.agent/EXT_REQUESTS.md`.
- The current guest sees three `2e8a:0009` runtime devices. D92 was flashed with
  the current CAP loader test image; 8E and 2BED retain the no-debug object-
  deletion image. D92 passed the malformed LOAD rejection and valid JCAlgTest
  install/select check. Other physical acceptance remains open.
- A bounded 8-second monitor on D92 after the reset attempts exited 0 with no
  serial output. This did not constitute a boot or runtime health pass.

## Highest-priority remaining work

1. **BOOTSEL-01 (P0):** implement the SDK vendor reset interface and bounded
   CDC-1200 reset servicing offline, then validate descriptors, cooperative
   request delivery and identity-pinned selection. Physical validation remains
   gated on guest-visible BOOTSEL identity.
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
