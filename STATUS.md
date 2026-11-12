# JCVM project status

Updated: 2026-11-12 10:53 UTC

## Goal

Complete the RP2350 Pico 2 Java Card VM: interpret unchanged Java Card CAPs,
provide native Java Card API and cryptography, support GlobalPlatform
installation and PC/SC, preserve card state, and verify the end-to-end workflow
on real hardware and Windows. The project is **not complete**; see [TODO.md](TODO.md)
for the acceptance backlog and [.agent/HANDOFF.md](.agent/HANDOFF.md) for the
current physical-test handoff.

## Current build and source

- ARM and RISC-V `max_crypto` wrappers pass. The ARM full firmware ends at
  `0x100F5018`, leaving 45,032 bytes before the configured `0x10100000` limit.
- The current diagnostic empty-card image is
  `build/arm/pico2-jcvm-max_crypto-object-deletion-post-apdu-trace-empty-card-5c78e647.uf2`
  (4,105,216 bytes, SHA-256
  `05d24b83afa967f023bdc8e090e060a0db776225b0ba6d03b7fd62fbad35e4ab`). It
  enables object deletion and heap-GC debug telemetry and has an erased
  persistent-storage region. CPLC source ID is `42B8697484B7CF78`.
- The diagnostic image is built from a dirty worktree containing unrelated
  pre-existing changes. It is for diagnosis, not a clean release artifact.
- Commit `5c78e647` adds gated markers around applet return, `fs_sync()` and
  CCID response submission. Focused tests pass: CCID progress flow 21/21 and
  APDU routing 17/17. The diagnostic image has not been flashed yet.
- Current-tree persistent-transaction, persistent-inventory and RP2350
  filesystem-overlay suites pass (`TEST:SUMMARY fail=0`); overlay tests include
  modeled power cuts and full-index compaction. This is host-only evidence.

## Hardware and current evidence

- Three Pico USB devices are attached to the guest, but the latest elevated
  enumeration showed all as runtime `2e8a:0009`; no BOOTSEL `2e8a:000f` device
  is visible. This conflicts with the user's report that all three were
  reinserted in BOOTSEL. Identity-pinned flash attempts found D92 and 8E but
  could not access their reset interfaces; chip ID 2BED was not connected.
  No image was written.
- The approved helper granted two runtime USB nodes, while `pcsc_scan -r`
  listed only VPCD readers. Physical APDU testing is currently unavailable.
- The corrected D92 RESOURCE-01 all-algorithms run produced 51 support rows
  before `SCARD_E_NOT_TRANSACTED` on `B0 75 12 00 01 16 00`. Serial evidence
  shows that APDU returned from the applet with `9000 len=29`, but no following
  CCID response record. New telemetry is ready to locate the stall; it has not
  been observed on hardware.
- Earlier 8E all-algorithms testing had heap/body read failures and an OOM
  panic. D92 performance-static and other all-algorithms attempts were
  interrupted or failed with PC/SC errors. Neither long option-1 nor option-2
  campaign completed; these are not acceptance passes.
- A bounded 8-second monitor on D92 after the reset attempts exited 0 with no
  serial output. This did not constitute a boot or runtime health pass.

## Highest-priority remaining work

1. **RESOURCE-01 / RESOURCE-02:** flash the trace image after a guest-visible
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
