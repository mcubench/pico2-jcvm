# Superseded MEM-01 status blocks removed from TODO.md on 2026-11-14

Moved verbatim by user decision (rule-conflict sheet item 6-A). Historical only; not instructions.

**Reviewer status 2026-11-14 (after `d06b65a8`..`7f9c6c10`):** T1–T6 are now complete offline. T6's cause is that the old prefix guard covered only exact tags and +2-byte children, not the 6-byte `FF FF id` alias-body suffix, so the 8E body was erased while still reachable; this matches review M2/M5. `d06b65a8` adds offline coverage of the leak mechanism (superseded child bodies under a live prefix at 68.6% occupancy; 12 reclaimed and live slots constant at 581). **MEM-01 offline work is exhausted.** The only open items are runtime evidence, T7 (VPCD option 2 past sync seq ≈1,750, where the pre-fix run failed) and T8 (hardware). ~~Both are blocked on the user's instruction to stop long emulator runs.~~ **Unblocked by user 2026-11-14:** long simulator runs are permitted under the "Long simulator/emulator runs" rule in `AGENTS.md`. Each run needs a stated reason and benefit, and you must first think whether the target scenario (e.g. sync seq >1,750 with high live occupancy and alias bodies) can be reached more cheaply than by a naive long run. Proceed with T7: use the bounded VPCD run described below unless a cheaper method reaches the same state faithfully.

**Reviewer status 2026-11-14 (after `af4659e9`..`c5706f78`):** T2, T3, T4 and T5 are done offline. T1 is done except for one remaining policy: what to do when neither snapshot copy validates and record boundaries cannot be proven. T7 offline coverage is strong (high occupancy, interrupted publishes, heap graph; integrity check ≤1,024 µs). **But the T4/T5 leak fix has no runtime evidence yet.**
- The only post-fix VPCD run (`20261114T063317Z`, 60 s) stopped at sync seq ≈800. That is before the point where the leak failed: the leaking run `20261114T055811Z` hit `FS_WRITE_FAIL` at sync seq ≈1,750 (`bytes=94311`), after about 5 minutes of wall-clock time.
- At equal sync seq the post-fix run shows the **same `files` counts** (323/351/568/837 at seq 200/400/600/800). Its `bytes` are only ~18% lower (29,913 vs 36,297 at seq 800) and still more than double between seq 400 and 800.
- So the post-fix run has not shown a plateau. It may still be leaking more slowly.

**Cheapest decisive check (~6–8 min, compatible with the speed constraint):** rerun the same VPCD option-2 setup, stopping at sync seq ≥2,500, not after a complete option 2.
- Log live record count and live bytes after each compaction, not `files` (which includes tombstones until compaction).
- Pass: no `FS_WRITE_FAIL`/`F205`; live bytes at seq 1,700 well below 94,311 and flat over the last 1,000 syncs; `reachable_alias` bounded in `OBJECT_DELETION:SCAN`.
- If bytes still grow: use `PICO2_GC_SCAN_DIAGNOSTICS=1` `ALIAS_ROOT` counts to find the prefix that keeps growing, before any hardware work.

The historical T6 cause is now identified below. After the bounded T7 runtime check, review any `alias-protect` events there and proceed to T8 hardware runs only after wiping with the empty-card UF2.

**T6 offline evidence review (2026-11-14):** the archived 8E trace shows the
exact body `03013894FFFFFE000000420007FFFF00000045` reported as an unreachable
candidate at serial lines 944–945 and erased at line 1004. Immediately after
that batch, the heap trace cannot read the same body (`fs_exists=0`) while
loading the reachable instance field `03013894FFFFFE000000420007` (lines
1012–1014). This establishes the direct failure chain: the old reclaimer
deleted a body still named by a reachable serialized alias, and the following
heap trace failed because that target no longer existed. The current alias
guard addresses that deletion path and has focused regression coverage. The
tag-format reason the body was not recognized is identified in the T6 finding
below. This archived trace does not provide clean post-fix runtime evidence
with zero `alias-protect` events; keep that check under T7 and do not relabel
the historical failed trace as current runtime acceptance.

**MEM-01 update (2026-11-14):** T1's offline fail-closed policy is now covered
by a two-invalid-COW regression: boot fails and the flash image remains
byte-identical. T1's offline corruption/recovery work is complete. T6's
historical alias-body root cause is identified and covered by the alias-chain
regression. T7 clean post-fix runtime plateau/alias telemetry and T8 physical
acceptance remain open.

**Status 2026-11-14 (review of VPCD runs on the RP2350 overlay backend, `artifacts/results/jcalgtest-support-vpcd-20261113T211216Z`..`20261114T062133Z`):** the T7 VPCD harness (uncommitted: `src/pc_rp2350_overlay_fs.cpp`, `CHOUPI_PC_RP2350_FS_OVERLAY`) **reproduces storage exhaustion on a clean store**. Option 2 runs reach ~1,256 CSV lines, then the arena is full (`FS_WRITE_FAIL … files=2212/8192 bytes=98287/98304`), followed by `6F00` and ~177–256 `F205`. Every reclaimer scan reports `candidates=0`; the growth is `reachable_alias` records (18 → 51 → 76 …) under one live object prefix `03013894FFFFFE000000420004FFFE0000004E` (superseded bodies written when a field of a live persistent object is reassigned). **Cause: the prefix-wide subtree keep added for T4/T5 (`36fcfe69`, `6c8e494b`, `b219daf2`) retains every historical body under a live prefix forever.** The arena (96 KiB), not the index, is the binding limit.

**Implementation update 2026-11-14:** T4/T5 now resolve reachable durable aliases into heap roots and trace their graphs; the prefix-wide keep is removed. The transient-array identity check was widened so marked arrays with durable tags count as reachable for this collection pass, which ended the repeated alias-retrace loop. `./tools/test-persistent-transaction` passes the existing T4/T5 missing-edge cases and a new 500× field-reassignment regression: after each reassignment, the old records are reclaimed; the applet namespace settles at 5 records / 121 tag-plus-payload bytes, and a fresh heap reads back the last child. `./tools/test-persistent-inventory`, ARM and RISC-V builds also pass. The PC VPCD harness is now wired to the RP2350 overlay implementation and builds with `./tools/build-pc --profile max_crypto --rp2350-overlay`. A bounded 60-second VPCD run (`artifacts/results/jcalgtest-support-vpcd-20261114T063317Z`) made 445 deletion calls, erased 564 persistent records, first reported 5 stale-alias candidates at scan 400, and had no `FS_WRITE_FAIL`/`F205`; the synchronized overlay was 29,913 bytes at event 800. It timed out before option 2 completed, so a storage plateau and full-run pass are **not established**. No physical board was accessed. T4/T5 implementation is complete offline; T7 remains the acceptance gate. The user requested offline implementation and reported the emulator is too slow; do not extend this into a long VPCD campaign unless asked.

Status 2026-11-13 20:45 UTC (review of commits `dce9f08f`..`b219daf2`): T3, T4,
T5 done offline; T1 and T2 partial; T7 now has direct-overlay synthetic
coverage but the JCAlgTest/VPCD harness remains unimplemented; T6 and T8 not
started. **Next, in this
order:** (1) **T7** harness — none of the five fixes has been exercised together
on the real overlay backend over a long run; this is the gating evidence for
MEM-01. (2) **T1 repair/quarantine** — detection alone now makes a damaged
store fail closed at boot (both snapshots rejected → `FAULT FS snapshot
unavailable`); add a test that boots a store with both snapshots damaged and
define the recovery outcome. (3) **T2 remainder** — reason-specific
`ReadFailure` logging and triggering T1 repair. Also measure
`overlay_integrity_valid()` cost at full index (8,192 slots, long probe
chains) since it now runs at every compaction; record it against the CCID
timeout. Do not firmware-only flash new images onto D92 (or any board with
pre-`92c91943` data) — use the empty-card UF2.
