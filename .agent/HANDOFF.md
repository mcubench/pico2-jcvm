# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Reread QKB: prior `found 0` / `list() failed` meant no attached board was running JCVM firmware, not no physical boards. F42DD9D07D6D2B07 is currently the identified working token; board state is volatile.
- T5 near-capacity DELETE passed on normal `max_crypto`: fixture left 3,820 B persistent free, then full JCVM cycle preserved data. At 94,479 B / 160 files, applet E4 took 2.673 s and package E4 1.696 s; both returned `9000`. Serial recorded compaction and no fault/time extension. Unchanged JCAlgTest remains installed.
- Firmware UF2 SHA-256 `fe9b190c073170a15d99626366872c89e2f6ca0e9a01c8da1fea0f199f808255`; detailed logs are in `.agent/QKB.md` and `artifacts/results/filesystem-capacity-delete-20261117T1748Z/F42DD9D07D6D2B07/`.
- LONGOP-01 T1/D2, T2, T5, T6 and MEM-01 T7/T8 remain open. T5 still needs other synchronous flash-heavy card-manager paths and explanation of the earlier 15 s Windows result.
- T6 direct RSA APDUs are not unchanged-client resume-list acceptance or full option-2. Prior pinned 1.8.5 resume-list attempt timed out at 300 s before RSA-2048.

## Next actions
1. Inspect synchronous flash-heavy INSTALL/card-manager paths and compare timing with near-capacity DELETE; start finite serial capture before GPPro/client operations.
2. Diagnose the earlier transient OOM after RSA work from a fresh capture.
3. Continue normal-image empty-card option-2 acceptance after the focused blockers are understood.

Before hardware work, refresh the approved reader helper, list exact readers, and follow `AGENTS.md`; never infer current board state from this handoff.
