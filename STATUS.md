# Project status

Updated: 2026-10-19

## Summary

CHOUPI is an experimental Java Card VM port targeting the Raspberry Pi Pico 2
(RP2350, Cortex-M33). It runs the JCAlgTest 1.8.2 JC305 applet on the PC/VPCD
path and implements a tested subset of Java Card runtime behavior. It is **not
a complete or Java Card certified VM**. Unsupported algorithms may correctly
report `NO_SUCH_ALGORITHM`; implementing every algorithm is not a project goal.

## Verified

- PC, RP2350 ARM, and RP2350 RISC-V build wrappers pass. ARM is the physical
  target; RISC-V builds are maintained as an additional check.
- The JCSystem/APDU probe regression passed on VPCD: 51 cases, no failures
  (`TEST:SUMMARY pass=51 fail=0 elapsed_s=138.095`). Coverage includes APDU
  handling, Java exceptions, transactions, persistent references, transient
  arrays and reset/deselect lifetimes, memory queries, and selected
  `JCSystem` APIs. Focused lifecycle checks confirm that reselecting an applet
  invokes `deselect()` and `select()` exactly once, rejected selection returns
  `6999` and leaves the applet unavailable for APDUs, a later SELECT works,
  and card reset invokes deselect before the next successful select. A fresh
  VPCD emulator process also restored a committed instance field (`0x33`) and
  persistent static/array reference identities across process restart; that
  dedicated restore run passed seven checks (`TEST:SUMMARY pass=7 fail=0`).
- Native transaction and storage recovery checks pass: injected record-write
  and snapshot-commit failures roll back cleanly, and the filesystem overlay
  test passes 1,000 randomized multi-record power-cut cases plus reset, CRC
  fallback, and tombstone restart checks. Authenticated GP DELETE and
  Java-level power-cycle behavior still lack end-to-end regression coverage.
- GET DATA (`0066`) now returns Card Recognition Data both before
  authentication, for card discovery, and through the authenticated
  card-manager path. Its exact encoding and status-word behavior pass 11 host
  unit checks. CPLC (`9F7F`) remains unavailable because the firmware has no
  manufacturing-data source. A direct real-reader probe sent both APDUs
  without selecting an applet: Card Data returned the expected TLV with `9000`,
  and CPLC returned `6A88`. A second physical probe established SCP03 S8,
  verified the card cryptogram, authenticated with C-MAC, and returned the
  same Card Recognition Data over protected GET DATA with `9000`.
- SHA-256 has known-answer and repeated-call coverage. The CCID/PCSC interface
  and physical reader have passed bounded probes in earlier sessions.
- Heap tracing now has a guarded sweep path in firmware: it runs after APDU
  interpreter scopes return and only reclaims after a complete root walk.
  Persistent-tagged objects are conservatively pinned, global arrays are
  excluded, and stable handle slots are reused. Exception exits now unwind Java
  frames so the post-APDU collector can run. Collector marking now streams
  roots and children through stable-slot bitmaps without temporary root,
  pending, or child-reference vectors. Physical static testing passed the
  earlier 512-row OOM point and reached 544 rows without a firmware panic, then
  hit an intermittent PC/SC reader loss. After restarting `pcscd.service`, a
  focused retry selected the applet (`9000`) but the host did not send the next
  GETVERSION APDU. Concurrent serial diagnostics confirm one XfrBlock (SELECT)
  and no second XfrBlock; runtime self-checks pass and USB remains enumerated.
  The latest cycle again passed both builds and the 18-second ARM runtime
  capture; a bounded PC/SC retry reproduced SELECT `9000` followed by no
  GETVERSION and timed out (2026-10-19).
  The latest CHOUPI-specific cycle also passed after the GET DATA change:
  ARM/RISC-V build gates passed, ARM firmware flashed, and serial output
  repeatedly reported `TEST:PASS choupi_runtime` with no `FAULT`
  (`artifacts/logs/cycle-choupi-20261019T061401Z.log`).
  The VPCD bridge is installed, but it cannot claim the Pico USB interface
  while `pcscd` owns it (`libusb ... failed: -6`). The static run remains
  blocked at host reader ownership/PC/SC transport, not by an observed VM
  algorithm failure.
  A physical 50-request M1 stress plus 50-call DES3 measurement passed without
  a GC skip.
  VPCD allocation batches for arrays and objects passed. The historical
  physical static run after the DESKey mapping completed 7,785 rows, but had
  1,852 `6F00`/`CANT_BE_MEASURED` results and was not a passing matrix. The
  ROM/imported exception class-view fix is now verified on hardware: M1 returns
  `03 9000` and JCAlgTest records the expected `NO_SUCH_ALGORITHM`. The latest
  post-fix matrix remains incomplete due to the host USB/PC/SC interruption;
  the latest serial capture shows no collector allocation failure or panic;
  the reader path needs PC/SC daemon recovery before another static run.

## Main gaps

1. **Applet contexts and firewall:** only one applet context is modeled. There
   is no general multi-applet registry/context switching, logical-channel
   support, or custom shareable-interface callback and ownership model. The
   current shareable lookup fallback is verified only for the default-null
   probe applet.
2. **Memory lifecycle:** basic stable-slot reclamation is enabled and tested,
   but persistent objects are pinned until deletion is implemented. The root
   walk and object ownership model still need broader audits, especially for
   native-held references and future multi-context operation.
3. **Physical end-to-end validation:** the latest static run reached 544 rows,
   passed the earlier collector OOM point, and then stopped when PC/SC lost the
   reader. A historical static run completed its matrix with many `6F00`
   results. Do not report either matrix as passing.
4. **Profile/API completeness:** several APIs are deliberately unsupported,
   including object deletion and integrity-sensitive arrays. Remaining runtime
   behavior needs systematic comparison against a selected Java Card profile.
5. **Card-management and persistence assurance:** storage rollback and
   randomized power-cut recovery have focused native coverage, applet
   persistence has a two-process VPCD check, and GET DATA Card Recognition Data
   has encoder/status unit coverage and a direct physical open-channel APDU
   check. Authenticated GP DELETE, post-delete GET STATUS, broader CAP restore,
   authenticated GP DELETE and end-to-end power-cycle validation remain open
   before claiming general card compatibility.
6. **Conformance and hardening:** no full Java Card conformance suite or
   certification-grade security assessment has been completed.

## Next priorities

1. Restore reliable consecutive APDUs through physical PC/SC, then rerun the
   static matrix. The latest trace isolates the loss after SELECT: the Pico
   answered `9000`, and the host did not send GETVERSION. The streamed
   collector passed 544 rows without the earlier allocation failures; the
   matrix remains incomplete. The direct VPCD bridge also needs an available
   USB interface and a live VPCD endpoint.
2. Define the supported Java Card version/profile, then audit runtime APIs,
   applet lifecycle, and firewall behavior against it.
3. Expand power-cycle, CAP-validation, and host/physical regression coverage.

## Project notes

- [readme.md](readme.md): build, flash, PC/SC, install, and JCAlgTest runbook.
- [plan.md](plan.md): project scope and acceptance criteria; some early
  baseline sections are historical and predate the current implementation.
- [.agent/HANDOFF.md](.agent/HANDOFF.md): chronological session handoff and
  latest concrete work.
- [.agent/QKB.md](.agent/QKB.md): recurring issues, evidence, and known fixes.
- [progress.md](progress.md): append-only validation and commit record.
