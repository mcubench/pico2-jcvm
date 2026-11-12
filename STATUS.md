# JCVM project status

Updated: 2026-11-12 11:47 UTC

## Goal

Complete the RP2350 Pico 2 Java Card VM: interpret unchanged Java Card CAPs,
provide native Java Card API and cryptography, support GlobalPlatform
installation and PC/SC, preserve card state, and verify the end-to-end workflow
on real hardware and Windows. The project is **not complete**; see [TODO.md](TODO.md)
for the acceptance backlog and [.agent/HANDOFF.md](.agent/HANDOFF.md) for the
current physical-test handoff.

## Current build and source

- ARM and RISC-V `max_crypto` wrappers pass with object deletion enabled and
  heap-GC debug disabled. ARM firmware ends at `0x100F474C`, leaving 47,284
  bytes before the configured `0x10100000` limit.
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

- Three Pico USB devices are attached to the guest, but the latest elevated
  enumeration at 11:25 UTC showed all as runtime `2e8a:0009`; no BOOTSEL `2e8a:000f` device
  is visible. This conflicts with the user's report that all three were
  reinserted in BOOTSEL. Identity-pinned flash attempts found D92 and 8E but
  could not access their reset interfaces; chip ID 2BED was not connected.
  No image was written.
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
- The current guest still shows three `2e8a:0009` runtime devices; no physical
  test ran in this turn. BOOTSEL passthrough and all physical acceptance remain
  open.
- A bounded 8-second monitor on D92 after the reset attempts exited 0 with no
  serial output. This did not constitute a boot or runtime health pass.

## Highest-priority remaining work

1. **RESOURCE-01 / RESOURCE-02:** flash the quiet CCID trace image after a guest-visible
   BOOTSEL interface returns, localize the post-applet response stall, fix
   remaining reclamation/resource failures, then complete unchanged JCAlgTest
   1.8.4/jc305 options 2 and 1 on identified physical devices with separate
   telemetry.
2. **HOST-02 / CTX-01/03:** finish Windows cold-reconnect measurements and SIO,
   firewall and cross-context acceptance.
3. **STORE/PERSIST, API, GP and AC:** complete storage, power-loss, current-image
   CAP coverage, installation/selection and end-to-end acceptance.
4. **HMAC and CAPIN:** complete support acceptance and the remaining CAP input
   validation/security cases listed in [TODO.md](TODO.md).

## Acceptance

Project acceptance remains open. Selected host tests and earlier physical
firmware/APDU checks pass, but current physical PC/SC access is unavailable,
the latest trace image has not been flashed, both long JCAlgTest campaigns are
incomplete, and Windows end-to-end and cold-power acceptance are not complete.
Do not infer completion from a successful build, USB enumeration, or startup
heartbeat.
