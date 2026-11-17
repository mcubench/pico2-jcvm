# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Reread QKB: prior `found 0` / `list() failed` meant no attached board was running JCVM firmware, not no physical boards. F42DD9D07D6D2B07 is the identified working token; board state is volatile.
- T5 near-capacity DELETE and INSTALL are measured. At 94,479 B / 160 files, applet/package E4 took 2.673 s / 1.696 s; second-instance E6 took 2.950 s. All returned `9000`; serial showed compaction and no fault/time extension. Local probe was removed; JCAlgTest remains installed.
- E6 source path has no `NativeOperationScope`: applet install runs synchronously; persistent journal commit writes a full snapshot, CAP owner metadata uses a separate flash-safe callback, and journal clear commits another snapshot. Firmware hash, logs and details are in `.agent/QKB.md` and `artifacts/results/filesystem-capacity-install-20261117T1757Z/F42DD9D07D6D2B07/`.
- LONGOP-01 T1/D2, T2, T5, T6 and MEM-01 T7/T8 remain open. T5 still needs other synchronous flash-heavy paths and an explanation of the earlier 15 s Windows result.
- T6 direct RSA APDUs are not unchanged-client resume-list acceptance or full option-2. The prior pinned 1.8.5 resume-list attempt timed out at 300 s before RSA-2048.

## Next actions
1. Continue auditing synchronous flash-heavy LOAD and lifecycle recovery paths for unmeasured watchdog exposure.
2. Diagnose the earlier transient OOM after RSA work from a fresh capture.
3. Continue normal-image empty-card option-2 acceptance after the focused blockers are understood.

Before hardware work, refresh the approved reader helper, list exact readers, and follow `AGENTS.md`; never infer current board state from this handoff.
