# Project status

Updated: 2026-10-18

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
  `JCSystem` APIs.
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
  hit an intermittent PC/SC reader loss. A focused retry showed SELECT reached
  the firmware but GETVERSION did not; a later Java PC/SC scan could not see
  the reader although the USB helper had found the device.
  A physical 50-request M1 stress plus 50-call DES3 measurement passed without
  a GC skip.
  VPCD allocation batches for arrays and objects passed. The historical
  physical static run after the DESKey mapping completed 7,785 rows, but had
  1,852 `6F00`/`CANT_BE_MEASURED` results and was not a passing matrix. The
  ROM/imported exception class-view fix is now verified on hardware: M1 returns
  `03 9000` and JCAlgTest records the expected `NO_SUCH_ALGORITHM`. The latest
  post-fix matrix remains incomplete due to the host USB/PC/SC interruption;
  the latest serial capture shows no collector allocation failure or panic.

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
5. **Card-management and persistence assurance:** broader CAP validation,
   package/applet lifecycle, GlobalPlatform management, and power-loss recovery
   need end-to-end validation before claiming general card compatibility.
6. **Conformance and hardening:** no full Java Card conformance suite or
   certification-grade security assessment has been completed.

## Next priorities

1. Diagnose the intermittent physical USB/PC/SC loss during consecutive
   applet APDUs, then rerun the static matrix. The streamed collector passed
   544 rows without the earlier 3,244-byte worklist or 2,048-byte root-vector
   allocation failures; the matrix remains incomplete.
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
