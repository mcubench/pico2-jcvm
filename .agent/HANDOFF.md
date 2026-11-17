# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Reread QKB: prior `found 0` / `list() failed` meant no attached board was running JCVM firmware, not no physical boards. F42DD9D07D6D2B07 is the identified working token; board state is volatile.
- T5 near-capacity DELETE, INSTALL and LOAD are measured. At 94,479 B / 160 files, E4 applet/package took 2.673 s / 1.696 s; E6 took 2.950 s; final E8 took 2.534 s. All returned `9000`, serial showed compaction and no fault/time extension. Temporary API CAP and local probe were deleted; JCAlgTest remains installed.
- E6 and final E8 dispatch have no `NativeOperationScope`. E6 runs applet install synchronously and commits journal/CAP metadata/journal-clear updates; E8 writes the CAP slot and commits package enable. Near-capacity timings are below 3 s. A baseline API CAP package survived a full normal JCVM cycle and was deleted afterward; this does not test interrupted journal recovery. Evidence is indexed in `.agent/QKB.md` and `artifacts/results/filesystem-capacity-{install,load}-*/` plus `artifacts/results/cap-lifecycle-cycle-20261117T1813Z/F42DD9D07D6D2B07/`.
- LONGOP-01 T1/D2, T2, T5, T6 and MEM-01 T7/T8 remain open. Four VPCD journal-recovery fault modes now pass with a local CAP. Physical power-cut acceptance remains PERSIST-03; the archived 15 s Windows uninstall followed the RSA PC/SC loss; no serial was captured during E4, so firmware watchdog attribution is unproven. Near-capacity E4/E6/E8 and package cycle persistence pass.
- T6 direct RSA APDUs are not unchanged-client resume-list acceptance or full option-2. The prior pinned 1.8.5 resume-list attempt timed out at 300 s before RSA-2048.

## Next actions
1. Reproduce the RSA-loss → GPPro `gp --list` → authenticated `gp --uninstall` sequence with timestamped serial and APDU logs; the archived 15 s Windows transmit failure has no synchronized firmware evidence.
2. Diagnose the earlier transient OOM after RSA work from a fresh capture.
3. Continue normal-image empty-card option-2 acceptance after the focused blockers are understood.

Before hardware work, refresh the approved reader helper, list exact readers, and follow `AGENTS.md`; never infer current board state from this handoff.
