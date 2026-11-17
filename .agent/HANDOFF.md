# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Reread QKB: prior `found 0` / `list() failed` meant no attached board was running JCVM firmware, not no physical boards. F42DD9D07D6D2B07 is the identified working token; board state is volatile.
- T5 near-capacity DELETE, INSTALL and LOAD are measured. At 94,479 B / 160 files, E4 applet/package took 2.673 s / 1.696 s; E6 took 2.950 s; final E8 took 2.534 s. All returned `9000`, serial showed compaction and no fault/time extension. Temporary API CAP and local probe were deleted; JCAlgTest remains installed.
- E6 and final E8 dispatch have no `NativeOperationScope`. E6 runs applet install synchronously and commits journal/CAP metadata/journal-clear updates; E8 writes the CAP slot and commits package enable. Near-capacity timings are below 3 s. A baseline API CAP package survived a full normal JCVM cycle and was deleted afterward; this does not test interrupted journal recovery. Evidence is indexed in `.agent/QKB.md` and `artifacts/results/filesystem-capacity-{install,load}-*/` plus `artifacts/results/cap-lifecycle-cycle-20261117T1813Z/F42DD9D07D6D2B07/`.
- A normal empty-card option-2 campaign is now running on F42DD9D07D6D2B07 with the 1.8.5 client and unchanged 1.8.4/jc305 CAP. Client output and continuous serial are under `artifacts/results/option2-acceptance-20261117T1830Z/F42DD9D07D6D2B07/`; the active attempt is `client-output-attempt2/`. The first attempt stopped at the menu because GNU timeout put Java in a background terminal group; `tools/run-jcalgtest` now uses `timeout --foreground` for its interactive branch, and menu choices 2 then 2 were accepted.
- LONGOP-01 T1/D2, T2, T5, T6 and MEM-01 T7/T8 remain open. Four VPCD journal-recovery fault modes now pass with a local CAP. Physical power-cut acceptance remains PERSIST-03; the archived 15 s Windows uninstall followed the RSA PC/SC loss; no serial was captured during E4, so firmware watchdog attribution is unproven. Near-capacity E4/E6/E8 and package cycle persistence pass.
- T6 direct RSA APDUs are not unchanged-client resume-list acceptance or full option-2. The prior pinned 1.8.5 resume-list attempt timed out at 300 s before RSA-2048.

- 2026-11-17 22:08: RSA OOM fix series committed (`d20ede50` doFinal scratch array, `7ca591e6` array RAM copy, `c3565e67` exact-length array tags, `de729d28` tombstone compaction in applet-requested deletion only, per user directive: no GC/compaction without an external request). Windows package for `de729d28` is ready (`WIN-MEM01-MAXCRYPTO-DE729D28` in `.agent/EXT_REQUESTS.md`). Board state: 07E74C5D7A038DEC runs that exact empty-card UF2 with JCAlgTest installed and one RSA probe run; F42DD9D07D6D2B07 runs a `de729d28`-equivalent dev build with long-run JCAlgTest state; EE116F37C7E4A04E is in BOOTSEL. The earlier F42D option-2 attempt (`option2-acceptance-20261117T1830Z`, attempt 2) stopped at 20:40 and was not resumed.

## Next actions
1. Monitor the active F42D campaign to client completion or its 10,800 s bound, then inspect the 1.8.5 result, telemetry and headroom gate; keep the output separate from the failed menu-only attempt.
2. Run the `1 -> All algorithms` campaign on another identified Pico 2 if one is available, with its own empty-card image, result directory and serial log.
3. Reproduce the RSA-loss → GPPro `gp --list` → authenticated `gp --uninstall` sequence with timestamped serial and APDU logs; the archived 15 s Windows transmit failure has no synchronized firmware evidence.

Before hardware work, refresh the approved reader helper, list exact readers, and follow `AGENTS.md`; never infer current board state from this handoff.
