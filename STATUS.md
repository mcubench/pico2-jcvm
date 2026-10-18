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

## Main gaps

1. **Applet contexts and firewall:** only one applet context is modeled. There
   is no general multi-applet registry/context switching, logical-channel
   support, or custom shareable-interface callback and ownership model. The
   current shareable lookup fallback is verified only for the default-null
   probe applet.
2. **Memory lifecycle:** heap reclamation is disabled. Repeated allocations can
   exhaust the heap; the physical static benchmark has a documented
   heap-retention failure.
3. **Physical end-to-end validation:** the physical JCAlgTest support sweep has
   stopped early with reader disconnects, and the full static performance run
   is not passing. Do not report either suite as complete.
4. **Profile/API completeness:** several APIs are deliberately unsupported,
   including object deletion and integrity-sensitive arrays. Remaining runtime
   behavior needs systematic comparison against a selected Java Card profile.
5. **Card-management and persistence assurance:** broader CAP validation,
   package/applet lifecycle, GlobalPlatform management, and power-loss recovery
   need end-to-end validation before claiming general card compatibility.
6. **Conformance and hardening:** no full Java Card conformance suite or
   certification-grade security assessment has been completed.

## Next priorities

1. Fix and regression-test heap reclamation/retention, then retry the physical
   static benchmark.
2. Diagnose physical CCID reader loss during long JCAlgTest support sweeps.
3. Define the supported Java Card version/profile, then audit runtime APIs,
   applet lifecycle, and firewall behavior against it.
4. Expand power-cycle, CAP-validation, and host/physical regression coverage.

## Project notes

- [readme.md](readme.md): build, flash, PC/SC, install, and JCAlgTest runbook.
- [plan.md](plan.md): project scope and acceptance criteria; some early
  baseline sections are historical and predate the current implementation.
- [.agent/HANDOFF.md](.agent/HANDOFF.md): chronological session handoff and
  latest concrete work.
- [.agent/QKB.md](.agent/QKB.md): recurring issues, evidence, and known fixes.
- [progress.md](progress.md): append-only validation and commit record.
