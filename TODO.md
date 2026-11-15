# Project TODO

Updated: 2026-11-14 (rule-conflict decisions applied). Active backlog for `plan.md`. Implementation and acceptance
are separate: keep tasks open while required validation is missing. Follow
`AGENTS.md` for native API boundaries, unchanged fixtures, build/hardware gates
and commits. Use the unchanged JCAlgTest **1.8.4 / jc305 CAP** with the **1.8.5 host client** for all new runs (see `AGENTS.md`; older entries below that say "1.8.4 client" are historical);
run `./tools/fixture-path --verify` before acceptance. 1.8.2 results are historical.

## Priorities and evidence

0. **MEM-01 (the only P0, user directive 2026-11-13): fix persistent/RAM memory-exhaustion defects from [review_memory_analysis_opus55_20261113](docs/review_memory_analysis_opus55_20261113.md).** See [MEM-01](#mem-01).
1. **BOOTSEL-01 (P1):** finish the remaining BOOTSEL acceptance opportunistically, when a board is already in hand.
2. RESOURCE-01 and RESOURCE-02 are gated on MEM-01; their physical evidence comes from MEM-01 T8.
3. HOST-01 owns all CCID stalls and PC/SC reader loss (a T8 blocker). Its sub-task [LONGOP-01](#longop-01) (deterministic RSA-1984 ×50 reader loss / panic) is P0 because it stops every option-2 campaign. Also: HOST-02 Windows timings; CTX-01/03 SIO/firewall/context acceptance.
4. STORE-01, PERSIST-01/02/03, API-01/03, GP-01 and AC-01/02 integration gates.
5. HMAC-01 acceptance and JC-RESET-01 diagnosis (feeds evidence to HOST-01).
6. CAPIN-01 then CAPIN-03/04; relative priority against resource work remains the user's decision.

Board state is volatile. Never treat a dated "boards disconnected" note as a standing
rule. Before hardware work, run the approved helper, list readers with
`./tools/list-readers`, and use any identified board (failover pool); the
HANDOFF board-state section records the last observation. A serial heartbeat or
USB enumeration alone does not establish API/PCSC acceptance.

### Current images (update on every storage fix; the only place that names a "current" image)

| Purpose | Image | SHA-256 | Notes |
|---|---|---|---|
| Latest external image (`max_crypto`, deletion ON, debug OFF, empty card) | `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-d508cae8-current.uf2` | `91288e4963b1baa3d6959dcf1da30178c4d5ffbac5c22d83e7a0c1a45c931e96` | source HEAD `d508cae8` plus dirty worktree; schema-v5 arbitrary-prefix tag storage; INV-28 96 KiB / 8,192-slot overlay; ARM normal build passed; no physical validation |
| Latest external (Windows) package | `build/external-test/pico2-jcvm-maxcrypto-deletion-d508cae8.zip` | `63fbae1ce480594952452533f78b6b71029adf0bcd49a6100745f827c4260a02` | EXT `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8`; package includes flash/test steps |
| Diagnostic | build per task (`mode=diagnostic`); record path and hash in `progress.md` | — | Never acceptance evidence |

### REF-185-01

- [x] **Regenerate the jCardSim support reference with the 1.8.5 client.**
  Generated `tests/fixtures/reference/jcalgtest/1.8.5/jcardsim-alg-support-extended.csv`
  with the supplied unchanged 1.8.5 client JAR (SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`)
  and the unchanged 1.8.4/jc305 CAP. The CSV SHA-256 is
  `8a8b7d75f77dbb77707dafdb6d100ac27c920a986c8509b8e7a6df09b6552746`.
  The prior 1.8.4-generated CSV remains intact. Both runs report 4,406
  yes/no result rows and their section/algorithm/status distributions are
  identical; timing columns differ as expected. The new CSV's self-reported
  client version field says 1.8.4, so use the selected JAR hash as client
  identity. Pinned both JAR and new CSV in `tests/fixtures/manifest.json`, made
  the 1.8.5 reference the active catalog alias, and updated the VPCD comparator,
  physical runbook, Windows packagers and their instructions. The known
  3072/4096 RSA label issue remains a client bug and is not fixed by refreshing
  this support reference.

Full pre-edit text (including existing uncommitted edits), completed tasks,
measurements and detailed acceptance cases are preserved in the
[backup](.agent/archive/2026-11-11-todo-compaction/TODO.md), with a
[SHA-256 manifest](.agent/archive/2026-11-11-todo-compaction/SHA256SUMS) and
[task/status map](.agent/archive/2026-11-11-todo-compaction/MIGRATION.md).
Linked detailed cases remain requirements unless explicitly superseded here;
archive device states and old measurements are historical. Detailed new runs go
in [progress.md](progress.md); reusable diagnoses go in QKB.

## Memory exhaustion fixes — P0 (2026-11-13)

### MEM-01

Source: [docs/review_memory_analysis_opus55_20261113.md](docs/review_memory_analysis_opus55_20261113.md)
(static review; nothing in it is validated yet). Key conclusion: observed
`F205`/OOM came from corrupted state (pre-`92c91943` rehash damage on D92,
pre-`36fcfe69` alias erasure on 8E) amplified by current design defects that
make one bad record permanent. Clean-store 2BED runs drained the deletion
backlog and never hit `F205`. Commit granularity follows `AGENTS.md` (one commit per logical step).

**MEM-01 current status (2026-11-14, consolidated):** T1–T6 are complete offline (T6 root cause: the old prefix guard missed the 6-byte `FF FF id` alias-body suffix, so a reachable 8E body was erased). Open items:
- **T7 runtime proof.** A fresh unchanged-client option-2 VPCD run is underway; the previous clean-store run hit `F205` at sync ≈6,300 (`bytes=98246`). The pre-fix run hit `FS_WRITE_FAIL` at sync seq ≈1,750 (`bytes=94311`); the earlier post-fix live run stopped at ≈800. Long VPCD runs are permitted under the `AGENTS.md` long-run rule; first check whether the target state is reachable more cheaply (see the T7 extensions below). Bounded decision check: unchanged option-2 VPCD to sync seq ≥2,500, logging live records and live bytes per compaction. Pass: no `FS_WRITE_FAIL`/`F205`, live bytes flat over the last 1,000 syncs, `reachable_alias` bounded. If bytes still grow, use `PICO2_GC_SCAN_DIAGNOSTICS=1` `ALIAS_ROOT` counts.
- 2026-11-14 follow-up: `./tools/test-rp2350-fs-overlay` passes `TEST:SUMMARY fail=0` on the current worktree, including the mixed high-occupancy/alias graph scenario documented below. This remains fake-flash evidence. The authorized helper granted three USB-node ACLs, but `./tools/list-readers` timed out after 20 seconds with no reader inventory, so unchanged-client VPCD option 2 was not started. T7 remains open.
- 2026-11-15 follow-up: the authorized helper found four matching Pico CCID USB devices and refused ACL changes (it supports 1–3). A subsequent elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 8` timed out after 8 seconds with no reader inventory. No fresh SmartcardIO option-2 run can start until bounded reader enumeration works; continue offline and avoid repeated unbounded PC/SC polling.
- 2026-11-15 diagnostic follow-up: the 68.6%-occupied fake-flash heap/reclaimer graph passes 12 compactions and reports 309 peak RAM handles, but emits no `HEAP_INVENTORY` with `PICO2_GC_SCAN_DIAGNOSTICS=1`. The diagnostic is `PC_VERSION`-only and additionally requires `static_roots >= 20`; this fixture supplies its 20 roots through an explicit vector, so that filter excludes the exact T7 graph. This run remains offline harness evidence, not the unchanged-client VPCD acceptance.
- 2026-11-15 fresh-client VPCD T7 result: ran the supplied 1.8.5 host client (`b6a753be…eacec3`) with the unchanged 1.8.4/jc305 CAP (`cac42cd3…dbfad`) from an empty card against a fresh PC VPCD + RP2350 filesystem overlay. The client reached about 490 method groups / 6,300 syncs, then failed with `SystemException_NO_RESOURCE` / `F205` in persistent array allocation. The overlay was at 95,823/98,304 bytes at sync 6,300; a subsequent allocation failed at 98,246/98,304 bytes and repeated failures continued near full capacity. Auto-reclaim/compaction had status 0 but removed only a few records; `reachable_alias` remained bounded, while the bulk of persistent records appeared reachable. This fails T7's no-`F205` and ≤90% peak gates and activates T10 immediately. Failure logs: `artifacts/results/jcalgtest-support-vpcd-20261115T084412Z/{client.log,emulator.log,ALGTEST_log_1794732353834.log}`. The emulator's later ptrace shutdown panic followed wrapper termination and is not the applet's `F205` cause.
- 2026-11-15 compliance correction: the temporary 112 KiB change in `d73e3411` violated INV-28 and was withdrawn; do not use its image. The current source, tests, and external image retain the specified 96 KiB / 8,192-slot overlay. See `progress.md` step `20261115-1228-spec28-restore-overlay-limit` and current external request `WIN-OBJECT-DELETION-LOW-WATERMARK-D508CAE8`.
- 2026-11-15 captured-transcript follow-up: replayed the successful 1,968-APDU prefix of the 1.8.5 client / 1.8.4-jc305 option-2 session three times (5,904 APDUs) through the PC emulator linked to the RP2350 filesystem overlay. All APDUs completed; the only six status differences were the known two pre-selection GET DATA differences repeated three times. There were no `F205`, `NO_RESOURCE`, `FS_WRITE_FAIL`, `FS_COMPACT_REJECT`, `alias-protect`, or `FAULT` reports; all sampled overlay syncs returned status 0. The run made 3,384 deletion calls, erased 6,849 applet tags, and bounded `reachable_alias` at 29. Heap inventory began at 103 arrays/22 instances and later remained near 105/24. Storage is cyclic rather than flat across adjacent syncs: seq 1900/2000 was 79,459/15,228 bytes, seq 3900/4000 was 81,775/14,687 bytes, and seq 5900 was 82,693 bytes (84% of 98,304-byte capacity), showing repeated reclamation and similar same-phase peaks. This is a replayed captured prefix, not a new client campaign; it ends before the source run's failing/unrecorded tail and does not satisfy the flat-bytes-over-last-1,000-syncs gate. Keep T7 open for a fresh unchanged-client VPCD option-2 run and physical T8. Replay source SHA-256 is `c077a5923f3f04b6644472860cd0c6b1820b4a13be419e5c69fa122709bf68f3`; outputs are in `artifacts/results/t7-option2-aes192-loop3-20261115/` and `artifacts/results/resource01-apdu-replay-vpcd-20261115T002517Z/`.
- 2026-11-15 full captured-sequence follow-up: from an empty card and fresh overlay, replayed all 1,972 commands in the 2BED26 option-2 source log. All 1,968 commands with captured responses matched except the two known pre-selection GET DATA differences; the final four commands, whose source responses were absent during reader loss, returned `9000`, `AA9000`, `9000`, `AA9000` (reset, measure, reset, prepare). No resource, filesystem, alias-protection or runtime-fault marker appeared; all 29 sampled overlay syncs returned status 0. Deletion logged 1,127 calls/status rows and erased 2,260 applet tags, with max batch 8. This narrows the unresolved symptom away from those applet operations in the emulator but cannot exercise or explain physical CCID/PCSC delivery; T7 fresh-client and T8 physical gates remain open. Detailed inputs/output: `artifacts/results/t7-option2-aes192-full-capture-20261115/replay-summary.txt` and `emulator.log`.
- 2026-11-15 regression refresh on the current shared source: `./tools/test-rp2350-fs-overlay`, `./tools/test-persistent-transaction`, `./tools/test-persistent-inventory`, and `./tools/test-rmi-registry` all exited 0. Overlay, transaction, and inventory suites reported `TEST:SUMMARY fail=0`; RMI registry emitted passing assertions. The overlay suite's repeated `persist_reject` lines are expected power-cut injection diagnostics; the suite ended with `package_delete_compaction_recovers_atomically_after_power_cuts`, full-index recovery tests, and `TEST:SUMMARY fail=0`. This is offline regression coverage and does not replace fresh-client T7 or physical T8.
- 2026-11-15 repeated-watermark follow-up: resumed from the prior 5,904-command overlay state and replayed two more 1,968-APDU prefixes (3,936 commands) plus a 200-command continuation. Both runs completed with `resource_failure=0`, no unexpected statuses or storage/runtime faults, and every sampled sync status 0. The two-cycle replay reached a sampled pre-cleanup 81,775 bytes at local sync 3,900; after its next cleanup, a fresh emulator restore began at 367 files/15,456 bytes. That is within 54–769 bytes of prior post-cleanup samples (367 files/14,687 at sync 4,000; 367 files/15,275 at sync 2,000; 367 files/15,510 at the prior persisted starting point). This supports a stable post-compaction live set despite expected sawtooth raw log use, but remains synthetic captured traffic and does not meet the unchanged-client or physical acceptance gates. Detailed logs: `artifacts/results/t7-option2-aes192-post5904-loop2-20261115/` and `artifacts/results/t7-option2-aes192-post5904-watermark-20261115/`.
- 2026-11-15 low-watermark captured-option-2 replay on the updated code: replayed all 1,972 APDUs from the captured 1.8.5-client/1.8.4-jc305 option-2 log against a fresh PC VPCD emulator linked to the RP2350 overlay, with automatic deletion enabled. All captured statuses matched except the two known pre-selection GET DATA differences; four source-unrecorded tail APDUs returned `9000`, `AA9000`, `9000`, `AA9000`. The new automatic trigger fired at 77,889/98,304 bytes (79.2%) and compaction succeeded (`erased=8`); sampled sync seq 1900 returned status 0 with 428 files / 17,177 bytes. All 29 sync samples returned status 0; 1,127 deletion/heap-GC service calls ran, with no `F205`, `NO_RESOURCE`, `FS_WRITE_FAIL`, `FS_COMPACT_REJECT`, `alias-protect`, or `FAULT`. The saved RP2350 overlay and CAP registry restored in a new emulator; its final four APDUs also returned `9000`, `AA9000`, `9000`, `AA9000`. This is captured-transcript replay, not a fresh client campaign or physical T7/T8 acceptance. Result/logs: `artifacts/results/mem01-lowwatermark-option2-20261115/` (regenerated fresh-run overlay SHA-256 `c6174a01c0e1d6cbf34cee1e58310adc2a653849547a51831ddcdc3dd01e4b7e`), replay `artifacts/results/resource01-apdu-replay-vpcd-20261115T075744Z/`, restore `artifacts/results/resource01-apdu-replay-vpcd-20261115T080057Z/`.
- 2026-11-15 repeated low-watermark replay from the saved compacted state: replayed captured option-2 APDUs 3–1972 (1,970 commands), all with expected statuses (zero differences). A second automatic trigger occurred at 78,211/98,304 bytes (`available=20,093`); compaction succeeded and sampled sync 1800 returned status 0 with 367 files / 15,221 bytes. The remaining sync samples were also status 0; no resource, compaction, alias-protection or fault markers appeared. A subsequent fresh emulator restored the saved overlay and passed the final four APDUs (`9000`, `AA9000`, `9000`, `AA9000`). This is a second captured replay cycle, not fresh-client/physical acceptance. The regenerated fresh-run overlay is `artifacts/results/mem01-lowwatermark-option2-20261115/overlay.flash` (SHA-256 `c6174a01c0e1d6cbf34cee1e58310adc2a653849547a51831ddcdc3dd01e4b7e`); the second-cycle state is `overlay-loop2.flash` (SHA-256 `79d91adbd5efe451143c485861e6af9334649773a9b578bf364c1bc8a403c2ad`). Logs: replay `artifacts/results/resource01-apdu-replay-vpcd-20261115T080613Z/`, restore `...T080913Z/`; a deterministic rerun of the fresh cycle is logged in `...T081044Z/`.
- 2026-11-15 third captured-cycle replay: resumed from the second-cycle persistent state and replayed APDUs 3–1972 (1,970 commands) with zero status differences. The automatic trigger fired at 78,213/98,304 bytes (`available=20,091`); compaction status 0 erased 8, and sync 1800 returned 367 files / 15,219 bytes. This post-compaction set is 2 bytes below cycle two (15,221 bytes). All 29 sync samples returned status 0, 1,132 deletion service calls ran, and there were no resource/compaction/alias/fault markers. A new emulator restored this state and passed the final four APDUs. This three-cycle captured replay establishes repeatable VPCD overlay behavior only; fresh host client option 2 and physical T7/T8 remain open. Third-cycle overlay `artifacts/results/mem01-lowwatermark-option2-20261115/overlay-loop3.flash` SHA-256 `2e5871ae4e5414129037452315391b876024f1c7bfbb3d43ef0fcc939a50b9dd`; logs `artifacts/results/resource01-apdu-replay-vpcd-20261115T081711Z/` and restore `...T082317Z/`.
- **Reviewer note 2026-11-15 (Claude Opus 5.5):**
  - **The T4/T5 leak fix now has runtime-level evidence.** In the captured-replay runs, the post-cleanup store returns to 367 files / 15.3–15.5 KB every cycle. Before the fix, the store grew until `FS_WRITE_FAIL` at sync ≈1,750. The fresh-client T7 run is still required, but the leak is no longer the main risk.
  - **New risk, M13 (headroom between cleanups):** raw use sawtooths up to 2,467 files / 81,775 bytes, about 83% of the 98,304-byte arena, before each applet-requested cleanup. That leaves about 16.5 KB of margin. Storage is reclaimed only when the applet calls `requestObjectDeletion()`, at most `kObjectDeletionMaxRecordsPerApdu=8` records per APDU. Option 1 (larger keys), or any applet that rarely requests deletion, can therefore hit `FS_WRITE_FAIL`/`F205` with a correct reclaimer.
- [ ] **T10 exhaustion headroom (M13).** (a) In every T7/T8 run, record the maximum pre-cleanup raw bytes and files per deletion cycle, and fail the run if the peak exceeds 90% of the arena. (b) Add an exhaustion-triggered reclaim: when a persistent write would fail, and only outside a transaction and at a safe point, run reclaim batches for the current applet plus a compaction, then retry the write once. Java Card permits the implementation to reclaim unreachable objects at any time. Test: a fake-flash/VPCD case where an applet never calls `requestObjectDeletion()` and allocates garbage in a loop; it must keep running with no `F205`, and live data must survive. Do this after the fresh-client T7 run unless T7 or T8 hits the limit first.
- 2026-11-15 safety prerequisite: `Context::collectHeaps()` rejects non-empty Java stacks, and reclamation is serviced at the empty-stack APDU boundary. Added a shared policy guard and tests so a pending deletion request remains latched while a persistent write transaction is active; this prevents reclamation from invalidating transaction rollback data. This does not yet implement low-watermark triggering, reclaim-on-exhaustion, or a write retry. A direct retry from `fs_write` is not safe without a broader APDU replay/transaction design.
- 2026-11-15 low-watermark follow-up: automatic deletion now starts at an empty-stack, transaction-free APDU boundary when free persistent space is at or below 20 KiB. While this automatic sweep is pending, deleted records are compacted at the boundary when free space remains below the watermark, so tombstones stop consuming the arena. The fake-flash RP2350 overlay test creates 8,240 unreachable objects over 1,030 APDUs, reclaims all 8,240 after a 695-boundary drain, preserves the live root, and peaks at 77,852 / 98,304 bytes (79.2%). The helper test (13 checks) and ARM `max_crypto` build pass. This is offline evidence; fresh-client VPCD and physical T7/T8 acceptance are still open. T10's direct failed-write reclaim/retry remains unimplemented because collection cannot safely run on the live Java stack.
- 2026-11-15 T10 storage-headroom follow-up: added v5 arbitrary-suffix parent-prefix encoding for persistent tags, retaining v2/v3 and v4 bitmap snapshot reads and leaving INV-28 at 96 KiB / 8,192 slots. The overlay regression saves 17 bytes on a 35-byte tag with a 20-byte prefix and verifies parent deletion, compaction, sync and restore; current mixed-live fixture remains 71,274 bytes / 98,304 with 27,030 free and passes 12 compactions. A fresh 1.8.5-client option-2 VPCD run is underway from an empty card to compare against the prior F205 at sync ≈6,300. Do not call T7/T10 fixed until that run reaches the prior failure point without F205 and physical T8 passes.
- **T8 physical acceptance.** See T8 below.

Superseded reviewer/status blocks are archived in `.agent/archive/2026-11-14-todo-conflict-cleanup/TODO-mem01-superseded-status-blocks.md`.

- [x] **T1 overlay integrity check + repair (M6/M7), offline safety coverage complete.** Snapshot integrity is checked at load/compaction; provable index damage is repaired or boundedly quarantined; ambiguous state remains fail-closed. Regression coverage includes duplicate/orphan/overlap/hash corruption, quarantined descendants and tombstones, prior-COW fallback, CRC-invalid copies, and two CRC-valid copies with overlapping record intervals. In each unrecoverable case `fs_init()` fails without mutating flash instead of treating the store as empty. Physical recovery acceptance remains under T8.
- 2026-11-14 T1 follow-up: integrity failures identify reason and slot in `FS_INTEGRITY_FAIL`. Pure probe-chain placement errors are rebuilt from self-describing arena entries. A stored-hash mismatch is repaired only for self-contained plain/compressed tags after record bounds, contiguous non-overlapping intervals, and unique canonical live identities verify. Parent-slot references are remapped. A hash mismatch on a parent-encoded record is ambiguous with a damaged parent pointer, so it is rejected and uses the prior COW generation. Regression tests cover orphan repair with a parent-encoded child, self-contained hash repair with a parent-encoded child plus unrelated value, ambiguous parent-pointer fallback, and overlapping-record fallback; values survive compaction/reload. `erase_overlay_slot` and `append_overlay` already fail before mutation on shared offsets. T1 remains open for quarantine of individually unreconstructable records and recovery of a damaged arena when no valid prior COW generation exists. RP2350 fake-flash suite, ARM and RISC-V builds pass. No long emulator or physical run.
- 2026-11-14 T1 quarantine: when no strict COW generation restores, a CRC-valid v4 snapshot may salvage an undecodable non-tombstone record only after every record boundary and the complete arena interval are proven contiguous and non-overlapping. It drops that record and dependent parent-encoded descendants, packs survivors, rebuilds the hash table and remaps parent slots. Valid parent graphs and unrelated values survive sync/reload/compaction. Tombstones, ambiguous parent pointers/hashes, bad bounds, gaps, overlaps and duplicate identities remain fail-closed. Fake-flash regression covers a single undecodable record with a retained parent graph and no usable older snapshot, plus an invalid parent reference that must not be quarantined. `./tools/test-rp2350-fs-overlay`, `./tools/build arm`, and `./tools/build riscv` pass. No hardware or emulator campaign. T1 remains open for broader corruption matrices, including multiple quarantined descendants and tombstones, and for deciding behavior on ambiguous arena damage with no good COW copy.
- 2026-11-14 T1 quarantine matrix: fake-flash coverage now corrupts a self-contained parent tag and confirms its parent-encoded child is also dropped while an unrelated record remains readable; it separately corrupts a tombstone and confirms startup fails closed instead of removing it. Both checks pass in `./tools/test-rp2350-fs-overlay`. This completes the planned multiple-descendant and tombstone cases for bounded quarantine. T1 stays open for arena layouts with unprovable intervals: preserve the prior strict COW copy if one validates, otherwise fail closed; do not guess record boundaries.
- 2026-11-14 T1 fail-closed policy: `filesystem_restore_fails_closed_without_mutating_two_invalid_snapshots` corrupts both committed snapshot payload CRCs and asserts `fs_init()` fails without changing flash. The follow-up `filesystem_restore_fails_closed_for_two_ambiguous_cow_copies_without_mutation` keeps both snapshot/header CRCs valid while making the record intervals overlap; both copies are rejected and flash remains byte-identical. Together with the prior COW fallback and bounded-quarantine tests, offline T1 policy is complete: use a valid older copy, salvage only when all record intervals are provable, and otherwise preserve flash and fail closed. Hardware recovery behavior remains unvalidated.
- 2026-11-14 T2 completion: the reclaimer calls the T1 index-repair hook after it encounters an unresolved enumerable candidate. A successful repair restarts the scan; a candidate still unresolved is left untouched while later candidates continue through bounded batches. The test-only hidden-existence injection verifies 19 of 20 applet and package-static records are deleted while the hidden record survives. A separate orphan-probe test repairs and reclaims all 20. `OBJECT_DELETION:INTEGRITY` reports the first skipped tag once per reclaimer invocation to avoid serial-log amplification; read failures are attributed to alias-chain, root-scan, or candidate-scan stages. Fake-flash overlay, inventory and transaction tests pass; ARM/RISC-V builds pass. No hardware or VPCD campaign.
- [x] **T2 reclaimer survives an unresolvable candidate (M3/M10).** Both applet and package-static reclaimer batches now keep unresolved candidates intact, report one bounded `OBJECT_DELETION:INTEGRITY` line with namespace/tag, and continue erasing resolvable entries across batches. A skipped candidate triggers the T1 integrity check; a repairable probe-chain or stored-hash index fault is repaired and enumeration restarts. `ReadFailure` diagnostics distinguish `alias_chain`, `root_scan`, and `candidate_scan`. Batch sorting still erases the longest tags first. RP2350 overlay tests verify index repair reclaims all 20 records and an independently unresolved candidate is preserved while the other 19 are erased for both applet and package-static namespaces. This is host fake-flash coverage, not long-run JCAlgTest or physical acceptance.
- [ ] **T7 offline long-run harness on the real overlay backend.** PC VPCD build linking `src/pico_filesystem.cpp` over fake flash (96 KiB / 8,192 slots); unchanged option 2 (1.8.5 client, 1.8.4 jc305 CAP) with `PICO2_TEST_OBJECT_DELETION=1`; assert no SKIP, backlog drains, plateau, zero compaction rejects, zero `alias-protect`, bounded RAM handles. Partial evidence: RP2350 overlay unit harness exercised 1,030 direct-API object-deletion cycles × 8 persistent objects. It passed automatic index compaction, retained one live record, ended with 57 index slots, and reported 35,932 bytes minimum available. This synthetic test does not replace unchanged JCAlgTest option 2, and does not measure RAM handles or alias-protect events. **Review 2026-11-14:** the churn test cannot reproduce the observed failures yet: it keeps only 1 live record (the arena never holds many survivors, whereas M1/M6 damage appeared when compacting many live, mixed-size records), it uses only flat 6-byte field tags (no `FF FE`/`FF FF`/`FF FD` bodies, aliases or compressed tags, so the children-first/alias paths are not reached), it creates a fresh `jcvm::Context` every cycle (RAM handles reset), it reloads only once at the end, and it never calls `overlay_integrity_valid()` directly. Cheap extensions to make before or alongside the VPCD wiring: (a) hold 60–75% of arena and index live, with interleaved survivors and garbage of mixed sizes (1–300 B) and real object/array/alias graphs; (b) assert `overlay_integrity_valid()` after every cycle and run `fs_init()` reload every 50 cycles; (c) run long enough for ≥10 compactions and report the compaction count plus the integrity-check time per compaction (this covers the cost measurement); (d) replace `available != 0` with a steady-state lower bound after the first compaction; (e) interrupt a snapshot publish at several byte offsets (fake-flash write cutoff) and check that reload yields the previous or the new snapshot, never FAULT.
- 2026-11-14 T7 offline extension: added a separate RP2350 overlay scenario with 320 live slots (20 parents, 280 parent-encoded children, 20 unrelated values), mixed 180–300-byte child payloads, 12 mixed-size garbage batches, and 12 compactions. Live arena use held at 71,274/98,304 bytes (72.5%); available capacity never fell below 27,030 bytes. Integrity passed after every compaction (196 µs average, 240 µs max in this run), all live values survived, and sync/reload every four cycles succeeded. This covers mixed high occupancy, repeated integrity checks, ≥10 compactions, a capacity floor, and periodic reloads for the overlay storage path. It still does not exercise the reclaimer and real heap alias graphs together at this occupancy, interrupted snapshot writes in this scenario, or the unchanged JCAlgTest 1.8.5 option-2 VPCD run. T7 remains open.
- 2026-11-14 T7 interrupted-publish extension: on that same 72.5%-occupied mixed live set, interrupted one 157,184-mutation snapshot publish at five offsets (start, quarter, midpoint, three-quarter, final mutation). Every reboot recovered a complete old or new state; all 300 payload records remained readable and integrity passed. T7 still lacks the reclaimer plus real heap alias graph at high occupancy, bounded RAM-handle evidence, and the unchanged 1.8.5 option-2 VPCD run.
- 2026-11-14 T7 heap/reclaimer integration: in one long-lived `Context`, 20 persistent reference-array roots protect 280 aliased byte-array children while 12 cycles each add and reclaim 8 unreachable mixed-size arrays. Initial live store is 67,458/98,304 bytes (68.6%); all 12 reclaimer batches erased 8/8, RAM GC reclaimed 8 arrays each cycle, and handle high-water stayed at 308 for 300 live arrays. Twelve compactions passed integrity (latest run: 613 µs average / 1,024 µs max); minimum available capacity was 30,846 bytes; 581 initial live index slots, 96 records erased over 12 batches (8 candidates each), sync/reload every four cycles and all child values passed. `./tools/test-persistent-transaction` also passes. This closes the high-occupancy reclaimer/heap-graph offline case, but does not capture runtime `alias-protect` diagnostics or replace the unchanged JCAlgTest 1.8.5 option-2 VPCD run. T7 remains open for those acceptance items.
- 2026-11-14 T7 live-child reassignment follow-up: extended that same scenario to replace one persistent child alias in each of the 12 cycles, while reclaiming the eight unrelated garbage arrays. Every stale body was erased after the alias changed, all 12 new child values survived graph verification/reload, and total overlay slots stayed at 581. Each cycle erased exactly 9 records (one superseded body plus eight garbage); final metrics were 67,458 initial bytes, 30,752-byte minimum free capacity, 309 peak handles, and 370 µs average / 1,296 µs max integrity check. `./tools/test-rp2350-fs-overlay` passes. This directly exercises the old-body accumulation pattern alongside mixed live heap graphs and compaction; it remains synthetic fake-flash coverage, not the unchanged JCAlgTest option-2 VPCD acceptance.
- 2026-11-14 reclaimer RAM reduction: firmware no longer materializes the diagnostic reachable-alias target list. Each `Tag` occupies 129 bytes; the 280-target T7 heap graph would require at least 36,120 bytes of vector elements (allocator growth may reserve more). Alias resolution and heap extension still run; PC/VPCD can opt into the target list through its existing scan summary. Persistent transaction, inventory and overlay suites pass, as do ARM/RISC-V `max_crypto` builds with object deletion enabled. This is a statically derived allocation bound, not a measured on-device heap delta; physical acceptance remains open.
- 2026-11-14 foreign alias protection now stores membership prefixes in a packed `[length, bytes...]` byte vector and omits any prefix already covered by a shorter retained prefix. This replaces one 129-byte `Tag` slot per foreign alias target with `len + 1` bytes and skips duplicate/descendant targets covered by an earlier prefix; membership results remain unchanged. Persistent transaction, inventory and overlay suites plus ARM/RISC-V object-deletion-enabled builds pass. No physical free-heap measurement; T7 client acceptance remains open.
- [x] **T3 bounded RAM GC under incomplete persistent trace (M4).** Implemented a conservative partial sweep for persistent-edge load failures: re-trace with all persistent-tagged objects as roots, preserve those objects, and reclaim only untagged volatile objects when roots are complete and no volatile-edge/invalid-reference failures occurred. Transaction test passed 50 cycles × 200 arrays with one malformed persistent edge; 200 volatile arrays were reclaimed each cycle. ARM and RISC-V wrapper builds pass. This does not replace T7's real-overlay long-run harness or physical acceptance.
- [x] **T4 protect the live graph reached through durable aliases; accept `FF FD` (M5).** Applet reclamation resolves reachable aliases into heap handles and retraces their graphs, rather than preserving every record sharing the target prefix. The direct body guard accepts migrated `FF FD` array bodies. Persistent transaction regressions preserve missed-edge object/array subtrees and reclaim stale descendants; a 500× persistent-field reassignment test confirms bounded records and bytes. ARM/RISC-V builds pass. T7 long-run acceptance remains open.
- [x] **T5 trace package-static aliases without retaining historical prefixes (M8).** Package-static reclamation uses the same alias-root retrace as applet reclamation. The missed-edge package-static test preserves the active target and descendant while reclaiming a detached sibling; 500× reassignment coverage also passes. ARM/RISC-V builds pass. T7 long-run acceptance remains open.
- [x] **T6 identify why the 8E body was omitted from reachability (M2 root cause).** The archived 8E log shows the reachable alias source `03013894FFFFFE000000420007`, but its body `03013894FFFFFE000000420007FFFF00000045` was classified unreachable and erased. The body appends the six-byte `FFFF00000045` suffix; the old heap-prefix guard recognized exact tags and direct two-byte records, not this alias-body encoding. The heap then failed loading the source field through that missing body. The current alias-chain guard resolves this encoding and its regression preserves the body subtree. Keep clean post-fix `alias-protect` telemetry under T7 acceptance; do not conflate that runtime check with identifying the historical root cause.
- [ ] **T8 physical acceptance.** MEM-01 T8 is the single owner of the physical option 1/2 campaigns; RESOURCE-01/02 cite its results. **Exploratory** physical runs are allowed whenever a board is available (in parallel with T7). Only runs after T7 passes count as **T8 acceptance**, and they must use a normal `max_crypto` image (SPEC INV-24: object deletion ON, debug OFF) on boards wiped with an empty-card UF2 (D92 mandatory). Record initial overlay counts and the client JAR SHA-256, and run options 2 and 1 on separate identified boards. CCID response stalls and reader loss (M11) are owned by HOST-01, which is a T8 blocker.
- 2026-11-14 partial T8 option-2 run on 8E (`8E464F4E2BBC0A19`): the max_crypto/object-deletion image and unchanged JCAlgTest 1.8.4/jc305 CAP were exercised with the supplied 1.8.5 client JAR. Over 32 minutes, 92 method groups and 668 CSV rows were recorded; 717 reset/deletion requests and 833 heap-GC samples were logged. Live counts stayed within 51–115 arrays and 27–28 instances, with no `F205`, `6F00`, or `NO_RESOURCE`. The run did not pass: PC/SC returned `SCARD_E_NOT_TRANSACTED` / `SCARD_E_READER_UNAVAILABLE` around `TYPE_DES LENGTH_DES3_3KEY ALG_DES_ECB_ISO9797_M2 Cipher_init()`. The 8E serial monitor still passed and showed the last `B0E2` response as `9000`, while PC/SC subsequently stopped publishing the Pico reader even though USB `2e8a:0009` remained visible. The approved PC/SC helper/service refresh did not restore it. This is partial storage evidence, not T8 acceptance; keep storage and CCID/PCSC failure classes separate. Logs/results: `artifacts/results/jcalgtest-1.8.5-physical-20261114/8E464F4E2BBC0A19/option2-performance/`.
- 2026-11-14 exploratory T8 option-2 run on D92 (`D92E743D5F640C0F`): debug ARM image SHA-256 `079ae6883c60b2fe12013c3e84bac0f5a25f35554f1215ddb96304d67c01225a`; unchanged 1.8.4/jc305 CAP SHA-256 `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`; supplied 1.8.5 client JAR SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`. After about 45 minutes the client failed at `TYPE_AES LENGTH_AES_192 ALG_AES_BLOCK_128_ECB_NOPAD Cipher_init()` with `SCARD_E_NOT_TRANSACTED` (121 method groups, 289 CSV rows). Firmware returned VM status `9000`; the final CCID trace stayed at `stage=6`, `pending=1`, `in_busy=0`, `submit_errors=0` after `fs_sync` took 1,269,927 us. No IN completion, `CCID_ERRORS`, `CCID_RESPONSE_WAIT` or `CCID_EXTENSION_RECOVER` was recorded. Across 1,101 heap-GC samples, live counts moved through `(30,16)`, `(95,17/18/19)`, `(103,22)`, `(105,24)` and then plateaued; there was no `F205`, `NO_RESOURCE`, `FS_WRITE_FAIL`, `FS_COMPACT_REJECT`, heap trace skip or firmware fault. All three runtime USB devices remained visible, but PC/SC did not publish readers after the failure or approved helper refresh. This is partial memory evidence and a repeated HOST-01 transport failure, not T8 acceptance. Logs: `artifacts/results/jcalgtest-1.8.5-physical-20261114-ccid-debug/D92E743D5F640C0F/option2-performance/`.
- Rule: do not run acceptance on a board whose store was written by an image older than the newest storage fix. Keep storage, CCID-delivery and host/PCSC failures as separate classes.

## BOOTSEL recovery — P1

### BOOTSEL-01

- [ ] **P1 (opportunistic, when a board is in hand): Complete reliable software entry into BOOTSEL for identity-pinned Pico 2 boards.** Offline firmware integration is implemented. On 2026-11-12, picotool could not locate its reset interface on D92, 8E or 2BED; identity-pinned CDC-1200 fallback entered BOOTSEL in 4, 3 and 3 seconds respectively, and all three firmware-only flashes verified. All boards passed finite runtime checks; PC/SC listed all three and GPPro listing on D92 succeeded. This validates the CDC fallback from idle on three boards, not the vendor-reset request or recovery during long synchronous work. Both remain acceptance gaps.
- Confirmed source findings: the earlier implementation exposed only CDC/CCID and consumed CDC reset requests only in the outer main loop; synchronous APDU work could delay recovery. The SDK vendor reset descriptor/driver and cooperative CDC reset servicing have since been integrated, with offline unit coverage. The exact historical D92 failure remains unproven. See [.agent/QKB.md](.agent/QKB.md#bootsel-reset-integration-audit--2026-11-12-offline) and audit commit `31cb571c`.
- Implementation: **done offline**: the SDK `pico_usb_reset` vendor reset descriptor and driver are integrated as interface 3; CDC/CCID remain interfaces 0–2. CDC-1200 requests are consumed after TinyUSB returns at the main loop and interpreter cooperative points. The SDK's competing CDC callback is disabled while CHOUPI's deferred callback remains active. Reset request unit coverage passes. Preserve identity pinning across USB topology changes; never reset or flash a different board as fallback.
- Validation: verify descriptors and driver dispatch, CDC callback-to-reset servicing, busy-work recovery and identity-selection regressions offline. Build ARM with the repository wrapper and resolve errors/warnings before flashing (RISC-V is out of scope). When an identified board is explicitly available, validate both picotool and CDC-1200 BOOTSEL entry from idle and long-running workloads; record request/re-enumeration times, chip ID, firmware hash, logs and finite post-flash serial health. Treat timeout, `TEST:FAIL`, `FAULT` or nonzero status as failure. Keep unrelated boards/campaigns running and preserve installed CAP storage.
- Remaining acceptance: exercise the vendor reset descriptor directly and
  CDC-1200 reset during a long synchronous applet operation, then capture the
  BOOTSEL re-enumeration latency. On 2026-11-14, host enumeration did not match
  the report of three boards in BOOTSEL: it found one BOOTSEL board (D92) and
  one runtime board (8E). The D92 image write verified fully, but D92 did not
  return as a serial device; it is currently absent from both modes. Failover
  to 8E succeeded: picotool entered BOOTSEL, flashed and verified the image,
  and the 18-second finite monitor repeatedly reported `TEST:PASS`. A second
  direct flash and 8-second monitor also passed. The occasional picotool
  status-134-after-verification case was not reproduced on this run; the flash
  wrapper now has a bounded, identity-pinned runtime-reenumeration recovery
  path with offline coverage. CDC-1200 during a long synchronous operation
  remains untested. Additional 2026-11-14 idle-path evidence: direct CDC-1200
  on 8E entered BOOTSEL for the same chip in 3,119 ms; the image was restored
  through the wrapper, ARM/RISC-V builds passed, and the 18-second monitor
  passed. The poller now uses a wall-clock deadline and avoids a duplicate
  picotool probe; the refreshed identity-pinned target-selection suite passes
  21/21, and another 18-second cycle on 8E passed. Offline reset/CCID progress
  remains 30/30. See
  `BOOTSEL-01-FLASH-RECOVERY-20261114` in `progress.md`.

### BUILD-DBG-01

- [x] **Remove verbose APDU and persistence traces from debug-off firmware.** On
  2026-11-12, ELF inspection found `APDU_PROCESS` and `PERSIST_REF` strings
  compiled into a debug-off ARM image. Both are now gated by their existing
  debug options. ARM and RISC-V `max_crypto` builds with object deletion ON
  pass; `./tools/test-no-debug-traces` confirms the ARM ELF retains concise
  object-deletion events and omits both verbose traces. A fresh empty-card UF2
  is referenced in `.agent/EXT_REQUESTS.md`. The package itself was not flashed
  locally because it erases persistent storage; its matching firmware-only
  payload was flashed while preserving card data. See
  `BUILD-DBG-01-NODEBUG-MARKERS` in `progress.md`.

## Resource lifecycle

- 2026-11-15 low-watermark auto-GC VPCD regression: after each APDU boundary, PC_VPCD with the RP2350 overlay detected low free space, requested deletion automatically, erased abandoned persistent arrays, and compacted tombstones based on free space captured before the erase batch. The run performed 512 replacements plus a live-root read: 515/515 APDUs passed, one AUTO_TRIGGER and one AUTO_COMPACT were logged, and no FS_WRITE_FAIL/F205/NO_RESOURCE/FAULT occurred. This is focused fixture evidence only; unchanged-client option 2, RAM-handle bounds, alias-protect telemetry, multi-compaction steady-state stress, and physical T8 remain open. Log: `artifacts/logs/object-deletion-vpcd-20261115T073454Z.log`.

### RESOURCE-02

- [ ] **Accept the implemented C1–C5 leak fixes.** Native buffer reuse, transient identity, bulk writes, rollback-safe identities, missing-body recovery, tag encoding and NO_RESOURCE mapping are implemented. Current 64-init plateau: AES 39 records / 589 bytes; RSA-2048 46 / 1,155; alias control 18 / 433 with one setup-time 4-byte unreachable tag; repeat deltas zero. No pre-fix linear-growth baseline was captured. Saved-state APDU 410 returns `9000`; transaction and multipackage regressions pass. Remaining: full unchanged 1.8.4/jc305 CAP suite with the 1.8.5 client and inventory sampling (physical evidence from MEM-01 T8) and required physical acceptance. Keep this open until those gates pass.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#resource-02).

### RESOURCE-01

- 2026-11-13 D92 evidence: option-2 logs show object-deletion requests repeatedly skipped because `enumerate_applet_context_root()` returned false (`applet_roots_complete=0`); heap-GC reporting encodes that boolean as generic `VisitResult::ReadFailure` (6), not a proven filesystem read error. Separately, compaction rejects `tag_reconstruction` at 2,468 entries / 98,273 bytes. Added a debug-only one-time root-scan failure trace distinguishing null restored reference, instance-load exception, and missing/malformed persistent applet-root record, with owner/package/reference/result/count metadata. It is built into the ARM diagnostic image but not flashed. The current guest host path reports zero Pico devices; root-scan and compaction causes remain open.
- 2026-11-13 follow-up: the D92 log's first reclaimer `applet_result=6` at xfr=142 (after three applet erasures) precedes `applet_roots_complete=0` on the following APDU. The latter is not proven to be the original failure. `enumerate_applet_context_root()` now preserves its exact inventory result and maps missing/mismatched root count/metadata to `IncompleteReachability`/`InvalidTag`; object-deletion skip and inventory logs retain that result. ARM/RISC-V diagnostic builds and the VPCD object-deletion regression pass. A refreshed Windows package with this change is listed in `.agent/EXT_REQUESTS.md`; no physical run has validated it. Instrument the reclaimer's repeated heap-root trace next if the external run shows the applet-root scan completes.

- 2026-11-13 offline storage hardening: `reconstruct_overlay_tag()` now checks every reconstructed canonical tag against the index's stored hash. A stale parent-slot reference that points to another structurally valid same-length record fails closed with `hash_mismatch` before compaction moves data. The RP2350 fake-flash regression restores the original pointer afterward and verifies the value remains readable. This is defensive integrity validation; it does not reproduce or fix the historical 2,468-record failure. No hardware access was possible.
- [ ] **Stop long-run NO_RESOURCE failures and finish physical acceptance (gated on MEM-01).** Physical option 1/2 evidence comes from MEM-01 T8. The [staged plan](docs/archive/resource-01-implementation-plan.md) is archived and largely overtaken by MEM-01. Focused reuse/replacement regressions pass; prior unchanged JCAlgTest runs still show persistent OutOfFlash/F205. A specific same-owner alias/body deletion bug is now reproduced and guarded (evidence below); this does not yet establish that it is the only source of the historical long-run failures. PC and Pico storage backends differ.
- Object deletion is ON in the normal build (SPEC INV-24; `mode=normal`: deletion ON, debug OFF), which enables deferred request-gated servicing; campaigns use a normal `max_crypto` image. Applet/package-static roots, foreign aliases, peer cached statics, shareable proxies and RMI roots have focused host coverage. The shared coordinator limits total erasures across both namespaces to eight per APDU; tests cover 8/8/6 batches. Host fake-flash covers 1,000 cuts; physical interruption/timing acceptance is open.
- Successful persistent-array restoration traces are now compiled only with `PICO2_DEBUG_PERSISTENT_GC=1`; failure diagnostics remain enabled. The RP2350 fake-flash suite passes `fail=0` with 134 output lines after the change. This reduces log/flush overhead, but runtime performance improvement has not been measured.
- 2026-11-12: Added a fail-closed guard against republishing an alias from a retained array handle when its persistent body tag is missing. A host regression reproduces the stale-handle sequence; persistent-transaction, object-deletion VPCD, persistent-inventory, RP2350 overlay, and ARM/RISC-V max_crypto builds pass. This prevents a dangling alias but does not establish who removed the body observed in the physical trace; long-run and physical acceptance remain open.
- Next: physical option 1/2 evidence comes from MEM-01 T8 (unchanged 1.8.4/jc305 CAP, 1.8.5 client, inventory and allocator telemetry). Then finish complete root and safety coverage with PERSIST-01/02/03 and investigate residual full-suite failures after the host compaction optimization (PERF-01/02); old 40–100-second compaction observations are historical, not current timing.
- 2026-11-12 physical retry: the guest enumerated three attached devices at USB paths `1-3`, `1-4`, and `1-5`, each `2e8a:0009` runtime. The approved PC/SC helper found zero readers; picotool found no accessible BOOTSEL device/reset interface. The bounded repository flash wrapper tried all three and none entered BOOTSEL, so no image was written. Resume only after passthrough exposes `2e8a:000f` or a named reader; then use the image named in the "Current images" table at the top.
- 2026-11-12: The prepared empty-card no-debug `max_crypto` object-deletion image was successfully flashed and verified by exact RP2350 chip ID on D92, 8E and 2BED after all three appeared as `2e8a:000f`. All three passed finite serial runtime monitors and GPPro exact-reader `--list` returned only the ISD. Per-board logs: `artifacts/results/maxcrypto-gc-nodebug-flash-20261112/`. This is flash/boot/empty-card validation; it is not the long JCAlgTest GC/Windows acceptance campaign.
- 2026-11-12 trace analysis: after the first missing durable array-body read, 277 heap-GC summaries show zero handle reclamation (276 with an edge failure); totals grow from 118 arrays / 22 instances at sequence 6 to 365 / 185 at sequence 291. First F205 is at sequence 87. The incomplete persistent walk therefore explains continued handle growth, but the missing body's deletion source remains unproven.
- 2026-11-12 current-tree regression refresh: persistent transaction, persistent inventory, RMI registry, and RP2350 filesystem-overlay tests pass. A focused three-cycle byte-array object-deletion VPCD run passes all 9 APDU checks and reclaims 3 applet records. These are host results only; BOOTSEL is still not visible in guest USB enumeration, so physical candidate tracing and long-run acceptance remain open.
- 2026-11-12 10:56 UTC recheck after CCID trace/status changes: `./tools/test-persistent-transaction`, `./tools/test-persistent-inventory`, and `./tools/test-rp2350-fs-overlay` all pass with `TEST:SUMMARY fail=0`. Overlay coverage includes 1,000 randomized multi-record cuts, 1,000 reclamation cuts, interruption recovery and full-index compaction. Host evidence only; the trace image still has not been flashed.
- 2026-11-12 current shared-tree regression pass: `./tools/test-persistent-inventory`, `./tools/test-persistent-transaction`, and `./tools/test-rp2350-fs-overlay` pass with `TEST:SUMMARY fail=0`. The bounded VPCD object-deletion probe passes 3 byte-array cycles (9 APDU checks, 3 records reclaimed), 2 RSA key replacements each at 512 and 2048 bits (9 checks and 20 records each), one 4096-bit RSA replacement (6 checks, 10 records), and one live-static RSA cycle (8 checks, 10 obsolete records reclaimed after clearing the static root). Logs are under `artifacts/logs/object-deletion-vpcd-20261112T113705Z.log` through `114400Z.log`. This is PC VPCD evidence only; physical response-stall and long-run acceptance remain open.
- 2026-11-12 RMI root refresh: current `./tools/test-rmi-registry` and `./tools/test-rmi-cap-metadata` pass against the shared worktree. Registry coverage includes retained context cycles/diamonds, peer static and proxy roots, fail-closed ambiguous roots, transient-array allocation lifecycle, and owner-slot reuse. Host unit evidence only; no physical JCAlgTest or USB validation is implied.
- 2026-11-12 campaign correction: the option-1/2 processes are no longer live and neither campaign passed. On pre-fix 8E, option 1 ended with `SCARD_E_NOT_TRANSACTED`, heap read failures and OOM; D92 option 2 was interrupted (`KeyboardInterrupt`). A D92 option-1 attempt was also interrupted; the corrected alias-guard run produced 51 algorithm rows, then failed with `SCARD_E_NOT_TRANSACTED` on `B0 75 12 00 01 16 00`. Its serial log ends at `CHOUPI:APDU_PROCESS stage=end sw=9000 len=29`; source places that print immediately before the applet function returns. Thus the log does not prove control returned to the CCID caller. The log has no later CCID response snapshot. Logs are under `artifacts/results/jcalgtest-1.8.4-resource01-20261112/`. Commit `028ba49a` separates the CCID response markers from per-object heap-GC tracing. Diagnostic image at that time (historical; see "Current images" table): `build/arm/pico2-jcvm-max_crypto-object-deletion-ccid-trace-empty-card-48e9a50b.uf2` (4,100,608 bytes, SHA-256 `32e4977ed65bc3087fbf5e2c7de8f5066c62c89dacb9b2a9f98b37c3b2a2124e`, CPLC source ID `48E9A50BB405634E`). It is built with object deletion on, CCID tracing on, heap-GC tracing off. Fresh guest enumeration still shows runtime USB devices rather than BOOTSEL, so no board received the image. Long-run physical acceptance remains open.
- 2026-11-12 RESOURCE-01 cause confirmed: the 8E candidate-trace run showed the live alias source `03013894FFFFFE000000420007` pointing to body `03013894FFFFFE000000420007FFFF00000045`; the reclaimer nevertheless emitted that body as `stage=erase`, then strict GC failed to read it. The alias source remained reachable but same-owner serialized aliases were not considered as body roots when the in-memory field walk omitted the edge. Added a conservative guard in commit `36fcfe69` to preserve any same-owner body directly targeted by an alias under a reachable persistent prefix; malformed or unreadable chains fail closed. The focused zero-field/missed-edge regression passes. Current D92 run uses corrected image SHA-256 `1837533bb67c06d8eac30f4b22cbc8552b7134b21f56f8a31f3770c31a4dde62`; at 2026-11-12 09:43 UTC it had 1,092 alias protections and 184 unrelated erasures, with zero `HEAP_GC_FS_READ_FAIL`, deletion skips, `FAULT`, `F205` or `NO_RESOURCE` in the captured serial log. The unchanged 1.8.4 `ALG_SUPPORT_EXTENDED` run is still active and incomplete; this early sample is not full acceptance. The image's finite monitor passed and GPPro installed the unchanged fixture successfully. The guest currently enumerates three runtime `2e8a:0009` devices rather than BOOTSEL; the 8E identity-pinned flash attempt could not find a reset interface, and 2BED was not attached. Keep option 2 plus complete option 1, longer duration and second-board acceptance open.
- 2026-11-12 D92 follow-up: the corrected option-1 client stopped after about 13 minutes with `SCARD_E_NOT_TRANSACTED` for `B0 75 12 00 01 16 00`. The preceding `INS_CARD_RESET` and 3-record deletion batch returned `9000`. The final serial line is `CHOUPI:APDU_PROCESS stage=end sw=9000 len=29`, printed immediately before the applet function's `return`; this old image did not include the later `CCID_APPLET stage=vm_return` marker, so whether control reached its caller is unproven. There is no later CCID response snapshot; `fs_sync()` and transfer submission/completion remain candidate locations only if the call returned. This corrected image has no GC read errors, deletion skips, `FAULT`, `F205` or `NO_RESOURCE`. The partial CSV contains 51 `ALG_` rows and is not acceptance. PC/SC then listed only VPCD while three USB nodes remained runtime `2e8a:0009`. Physical options 1 and 2 remain open; capture with a timing diagnostic image (see "Current images" table) before assigning a cause.
- Post-applet response stall (owned by HOST-01; evidence recorded here): debug-gated telemetry around applet return, `fs_sync()` begin/end and CCID response submission is implemented and built; existing periodic stage telemetry covers IN completion. Relevant CCID tests pass 21/21 and 17/17. The trace image has not been physically flashed, so the location remains unknown until a guest-visible BOOTSEL/reader is available. Keep the earlier 8E heap/body failure separate: that run had `HEAP_GC_FS_READ_FAIL` and `*** PANIC *** Out of memory`; the corrected D92 trace did not.
- Acceptance: same-key reuse, key replacement/deletion and cache resets plateau in retained RAM, handles and persistent records. A request before cache clearing must reclaim the newly unreachable graph before the next `process()` call while preserving live aliases/other owners. Demonstrate interrupted-reclamation recovery. Complete long physical runs without NO_RESOURCE, FAULT, unexpected reset or transport loss; record duration, iterations and high-water marks. Physical options 2 and 1 run under MEM-01 T8 on separate identified tokens when possible, with per-device telemetry. Coordinate STORE-01, API-01/03 and offline-only JC-RESET-01; keep the default build's reclamation OFF until the MEM-01 T8 + PERSIST-01 gate (the acceptance image enables it).

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#resource-01).

## CAP input-handling review — 2026-11-10

Source: [docs/review_fable52_20261110.md](docs/review_fable52_20261110.md)
(finding IDs A1–D2). The review was static reading only: no finding was
reproduced, and several are marked pass-reported there. Rules for every CAPIN
task:

- Start by writing the failing fixture and recording the observed behaviour in
  `progress.md`. If the finding does not reproduce, record that and close the
  item as not reproduced instead of changing code.
- Hostile CAPs and bytecode are repository-owned test inputs under
  `tests/fixtures/local` or generated under `build/`. Supplied CAP/JAR bytes
  stay unchanged; fixes are native C++ VM or loader code.
- Acceptance for each item: the fixture gets a controlled status word on PC
  VPCD (under ASan where memory safety is the claim), the next valid APDU
  succeeds, the multipackage VPCD regression and the unchanged JCAlgTest 1.8.4/jc305
  CAP install still pass, and the ARM build passes. Physical validation is
  separate and stays open until run.
- One CAPIN item per commit series; do not combine with RESOURCE work.


### CAPIN-01

- [ ] **P1: Validate a CAP completely before LOAD stores and enables it; add
  resolver null guards.** Findings B1, B2; extends R55.
  - Progress 2026-11-12: unknown component tags now fail during LOAD before
    package storage; import lookups validate missing/truncated Import records,
    and class/static resolvers guard absent Export/Class components. ARM and
    RISC-V builds plus malformed-CAP, unchanged JCAlgTest install/select, and
    CAP upgrade VPCD tests pass. The LOAD package-AID/Header-AID mismatch is
    now rejected before persistence and covered by a VPCD regression. Still
    open: required-component presence checks, library Import/Export dependency
    validation, external superclass without Import fixture, and physical test.
  - Progress 2026-11-12 12:43 UTC: Oracle Java Card 3.0.5 identifies Import,
    Constant Pool, Method, Static Field and Reference Location as required;
    Applet and Export are conditional, Debug is optional. A new red VPCD
    regression removes Reference Location from unchanged JCAlgTest 1.8.4 CAP
    bytes. LOAD accepts and persists that malformed package (`packages=1`);
    required-component checks must reject before storage. See
    [Oracle JCVM 3.0.5 §6.2](https://docs.oracle.com/cd/E59935_01/JCVMS.pdf).
  - Progress 2026-11-12 13:10 UTC: LOAD now explicitly requires Import,
    Class, Constant Pool, Method, Static Field and Reference Location alongside
    Header/Directory; Applet remains optional and Export is still conditional.
    Generated Import/StaticField/ReferenceLocation omission cases all reject
    before persistence. The ReferenceLocation case was repeated on physical
    D92: final LOAD response `6A80`, no package in subsequent GET STATUS, then
    the unchanged 1.8.4 CAP installed and selected successfully. ARM/RISC-V
    builds and unchanged JCAlgTest install/select, CAP upgrade and
    multi-package VPCD regressions pass. See
    `CAPIN-01-REQUIRED-COMPONENTS` in `progress.md` and physical logs under
    `artifacts/results/capin-required-component-20261112/D92E743D5F640C0F/`.
    Progress 2026-11-12 15:04 UTC: Added LOAD-time validation for external
    superclass and interface package tokens in Class records. The unchanged
    JCAlgTest 1.8.4 CAP mutation changes the applet superclass reference from
    `8203` to invalid external token `FF00`; it is rejected before package
    storage. Adjacent missing-Import case also passes. ARM and RISC-V builds
    pass. The valid multi-package VPCD run loaded 3 packages and installed 21
    instances, then timed out during later package lookup cleanup; that suite
    is not a complete pass. Guest enumeration after the user's BOOTSEL report
    still showed three runtime `2e8a:0009` devices, so no physical test/flash
    was attempted. Library dependency closure, Export consistency, null guards
    for legacy stored CAPs, and broad physical acceptance were open at that
    point; see the newer progress below for closure validation.
  - Progress 2026-11-12 16:18 UTC: INSTALL now validates each reachable
    dynamic package's imports and link metadata once, using an eight-slot
    bounded worklist that terminates on dependency cycles. A local RMI CAP
    fixture adds an unused missing dependency to a library; before the fix,
    INSTALL incorrectly returned `9000`; afterward it returns `6A88`, while
    valid external-interface installation remains `9000`. The supplied CAPs
    remain unchanged. PC VPCD suite passes 4/4; ARM and RISC-V builds pass.
    Physical validation was intentionally skipped because the user disconnected
    the devices. Export-table consistency beyond references reached through
    Constant Pool remains open.
  - Progress 2026-11-13 11:21 UTC: LOAD now checks every exported static-field
    offset against the static image and every exported static-method offset
    against the Method component, including methods that no ConstantPool entry
    references. When Descriptor is present, a fixed 8 KiB bitmap confirms each
    method offset occurs in its method records; without Descriptor, validation
    checks bounds and a complete method header only. Malformed zero and
    unreferenced out-of-range Export method offsets reject final LOAD with
    `6A80` before persistence. Unit validation passes 42/42; both Descriptor
    modes of the SIO VPCD suite pass 35/35; ARM/RISC-V builds pass. Physical
    validation was not run. Export class-offset-to-Class-record validation and
    exact starts in Descriptor-free CAPs were still open at that point; they
    are addressed by the following progress record.
  - Progress 2026-11-13 12:35 UTC: LOAD now validates every Export
    `class_offset` against the start of an actual class or interface record in
    the Class component for CAP 2.1 and 2.2, including Descriptor-free CAPs.
    Offsets into record bodies and beyond the component reject before storage.
    Unit checks pass 45/45; malformed LOAD regressions reject with `6A80` in
    Descriptor-free and Descriptor-bearing SIO VPCD runs; PC, ARM and RISC-V
    builds pass. No physical validation was run. Exact Class record boundaries
    are now established from Class framing, so they do not depend on Descriptor.
  - Progress 2026-11-13 13:18 UTC: Export validation now also rejects an empty
    Export table, nonzero static-member counts on interface exports, and applet
    package Export entries that do not target a shareable interface. A mutated
    applet CAP pointing its Export at a valid class_info record rejects at LOAD
    with `6A80`. Unit checks pass 50/50; valid SIO VPCD suites pass 35/35 with
    Descriptor omitted and included; ARM/RISC-V/PC builds pass. This follows
    JCVM 3.0.5 §6.13. Physical validation was not run. Other Export invariants
    remain open.
  - Progress 2026-11-13 13:39 UTC: Export validation now rejects duplicate
    class targets and duplicate static-field/static-method offset mappings.
    Unit checks pass 53/53, a malformed duplicate-class CAP rejects during
    LOAD with `6A80`, and valid SIO VPCD suites pass 35/35 with Descriptor
    omitted and included. PC, ARM and RISC-V max_crypto builds pass. This
    enforces one-to-one Export token mappings for classes and static members.
    Physical validation was not run. Descriptor-free exact method-start
    validation and malformed legacy stored-CAP runtime coverage remain open.
  - Progress 2026-11-13 13:54 UTC: When Descriptor is present, Export class
    tokens and offsets must resolve to a public Descriptor class, and each
    exported static-method token must resolve to that class's matching public
    or protected static method/constructor offset. Unit checks pass 58/58 and
    the valid Descriptor-bearing SIO VPCD suite passes 35/35. PC, ARM and
    RISC-V max_crypto builds pass. This follows JCVM 3.0.5 §§6.13–6.14.
    Physical validation was not run; Descriptor-free method-start and legacy
    persisted-CAP runtime coverage remain open.
  - Progress 2026-11-13 14:43 UTC: For library CAPs with Descriptor, LOAD now
    requires a complete one-to-one Export mapping for every public Descriptor
    class/interface, including matching token, Class record offset and
    interface kind. Package-private classes need no Export entry; applet
    package shareable-interface rules remain unchanged. Unit checks pass
    61/61; a malformed library CAP omitting a public Export class rejects
    LOAD with `6A80`; valid SIO VPCD suites pass 35/35 both with Descriptor
    omitted (normal GPPro path) and included. ARM/RISC-V/PC max_crypto builds
    pass. This completeness check requires Descriptor and therefore cannot
    cover Descriptor-free CAP LOADs. Legacy persisted-CAP runtime coverage
    and physical acceptance remain open.
  - Progress 2026-11-12 12:10 UTC: Added a VPCD LOAD AID override for a test
    that requests the unchanged JCAlgTest CAP under a different package AID.
    The red regression reproduces the defect: LOAD succeeds, the package is
    persisted (`registry_saved_packages=1`), then INSTALL fails at stage 4
    with `6A82`. The LOAD/Header AID check is the next fix; test and source
    hashes remain unchanged. See the new `CAPIN-01-LOAD-HEADER-AID-MISMATCH`
    progress entry and its log.
  1. In `CapLoadSession::finishLoad` (`src/cap_load.cpp`), construct
     `JC_Cap` with payload validation over the RAM load buffer and run the
     import-independent part of `validate_cap_imports` before
     `choupi_store_cap_package`. Reject unknown or duplicate components,
     unsupported CAP versions and lengths above 65,535 on both backends.
  2. Done for required Import, Class, Constant Pool, Method, Static Field and
     Reference Location components. Keep Applet conditional on applet
     declarations; enforce Export only when the package exports public elements.
  3. Done at INSTALL: run ConstantPool and link validation over the complete
     reachable dynamic-package dependency closure. CAP packages can still be
     LOADed before dependencies are present; closure validation therefore runs
     at INSTALL. Export class offsets, static-field offsets and static-method
     offsets are validated; broader Export-table consistency remains open.
  4. Done: old stored CAP paths now fail with `SecurityException` when Import,
     Export, or Class components are absent or their requested token/offset is
     out of bounds. `jc_import.cpp` validates Import presence and record bounds;
     `bc_static.cpp` checks Export presence/count; `jc_cp.cpp` now checks Class
    presence/size before class-index and classref access. Persisted-CAP restart
    regressions now cover an unknown Class component tag and removal of the
    required Import and Class components while updating Directory sizes and
    retaining a valid VPCD sidecar checksum. Each restarts, gets `6F00` on
    applet SELECT, then `9000` from card-manager SELECT. This verifies malformed
    stored CAPs fail closed without wedging the card-manager path. Export is
    optional for applet packages; library Export completeness is tested
    separately when Descriptor is present. Physical behavior remains open.
  5. Done: LOAD compares the requested package AID with the CAP Header AID
     before cleanup, slot allocation or storage (`load-header-aid-mismatch`,
     `6A80`, no package registry entry). If storage succeeds but package-enable
     transaction does not, lifecycle recovery removes the unenabled staged CAP
     on the next recovery pass; `lifecycle_recovery_removes_unenabled_staged_load`
     verifies repeated recovery is idempotent.
  Fixtures: extra tag-12 component; library with Export but no Class plus an
  importer; class with external superclass and no Import. The exact
  `external-superclass-without-import` mutation is generated from unchanged
  JCAlgTest CAP bytes (external superclass ref `8203`, Import removed). On
  2026-11-14 the direct PC VPCD startup path rejected it before registry
  publication. A separate raw GlobalPlatform APDU VPCD test now verifies the
  final LOAD response is `6A80`, GET STATUS omits the malformed package and
  retains an unrelated control package, and DELETE of that control package
  returns `9000` and removes it. Log:
  `artifacts/logs/cap-missing-import-apdu-20261114T230132Z.log`; run via
  `./tools/test-cap-malformed-vpcd --mutation external-superclass-without-import`.
  The bounded PC/SC reader listing separately timed out; VPCD does not
  establish physical acceptance.
  External superclass package-token range validation is covered by
  `external-superclass-import-token-out-of-range`; transitive library
  dependency closure is covered by `missing-library-dependency`. Export
  consistency remains open.
  - 2026-11-15 validation refresh: `./tools/test-cap-malformed-vpcd
    --mutation external-superclass-without-import` passed the raw LOAD APDU
    rejection (`6A80`), absence from GET STATUS, and unrelated control-package
    DELETE (`9000`). The adjacent CAPIN-02 test
    `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage
    --cap-import-policy` passed the expected INSTALL denial (`6982`) for the
    forbidden native-bridge import. Restarted malformed stored-CAP tests for
    missing Import and Class each passed `TEST:SUMMARY pass=5 fail=0`: applet
    SELECT failed closed with `6F00`, then Card Manager SELECT succeeded.
    `./tools/test-pc-vpcd-applet --passes 1 --select-only` also loaded and
    selected the unchanged JCAlgTest CAP (`9000`) on the Descriptor-free path.
    Current CAP helper suites pass: Export/import validation 61/61, method
    header bounds 7/7, and method-body bounds including Descriptor-free
    structural starts 11/11. No physical acceptance; bounded reader listing
    still times out. Remaining CAPIN-01 work includes physical validation and
    broader Export consistency.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-01).

### CAPIN-02

- [x] **P1: Allow-list the ROM packages an uploaded CAP may import.** On
  2026-11-13, added exact public Java Card / GlobalPlatform package AIDs with
  the CHOUPI AID aliases, checked dynamic callers in the central package
  resolver and returned `6982` for forbidden imports during INSTALL. The same
  resolver guard protects runtime `invokestatic`, `new`, static-field and
  classref lookups. The native bridge (`A0000000600001`) and all unlisted ROM
  packages are denied. Generated native dispatcher array-type mismatches now
  throw `SecurityException`. Export table dump:
  `artifacts/reviews/cap-import-policy/nativeimpl-export-table-20261113.txt`.
  Before the fix its two exported classes/68 total methods were visible, and a
  hostile CAP with an external `StaticMethodref` installed with `9000`; after
  the fix it fails INSTALL with `6982` and creates no applet instance.
  Validation: hostile-CAP VPCD test, multi-package install smoke, and the
  35-case SIO suite pass; ARM and RISC-V `max_crypto` wrapper builds pass.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-02).

### CAPIN-03

- [ ] **P1: Complete end-to-end validation of frame-owned executing-package state.** Finding A3. Implemented: the package ID is stored in each `Frame`; `Context::getCurrentPackageID()` reads the top frame, with the applet package as the empty-stack fallback. Normal returns, exception pops, `unwindToDepth()` and `unwind()` therefore restore package identity automatically. Removed the separately allocated `packagesID` list and its missed-pop failure mode. A sanitizer-backed unit test reproduced the stale package after partial unwind before the fix and passes for normal return, partial unwind and full unwind after it. ARM/RISC-V builds and the official RMI client VPCD regression pass 18/18. Still required: the hostile nested same-context `processRemoteInvoke` stack-overflow fixture, caller `impdep1` rejection, and retained-context size check across repeated failed APDUs; record exact outcomes before closing this item.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-03).

### CAPIN-04

- [ ] **P1: Validate bounded frame depth and controlled APDU failure.** Finding C4, trigger for R43. Implemented a hard 64-frame limit in `Stack::push_Frame`; a 65th zero-word frame throws `StackOverflowException`, even though the method consumes no `jc_stack` words. CAPIN-03 moved package identity into the frame, so `Context::changePackageID` no longer allocates and safely remains `noexcept`. The sanitizer-backed regression fails before the bound (zero-word frames grow without limit) and passes afterward with frame depth exactly 64. ARM/RISC-V builds and official RMI client VPCD pass 18/18. Remaining: execute a recursive uploaded CAP method via APDU and verify a controlled status word plus successful subsequent APDU; confirm the selected limit is sufficient for supported applets.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-04).

### CAPIN-05

- [ ] **P1: Deny dynamic-package access to private state of JCRE-owned
  objects.** Finding A2.
  1. `getfield`/`putfield` (all variants) from a dynamic package on a global
     or JCRE-owned instance throws `SecurityException`.
  2. Load-time: reject external InstanceFieldrefs into ROM packages unless a
     ROM metadata table marks the token public or protected.
  3. `invokespecial`: the receiver must be assignable to the constructor's
     class and must not be a global instance.
  4. AID backing arrays are not writable by dynamic code.
  Fixture: `JCSystem.getAID(); getfield_a <AID token 0>; bastore`, then a
  Shareable call; expect `SecurityException` and an unchanged
  `AID.equals` result. Coordinate with CTX-01/03.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-05).

### CAPIN-06

- [ ] **P1: Check the receiver in virtual and interface dispatch.** Finding
  A4 (pass-reported; reproduce first). `invokevirtual`: receiver assignable to
  the reference's class (arrays only for `Object`); package tokens resolve only
  while the class's package equals the referencing package.
  `invokeinterface`: operand `nargs` equals the target header `nargs`; reject
  array receivers. Mirror the static parts in the loader. Fixtures: package
  token against a ROM receiver; mismatched `nargs`.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-06).

### CAPIN-07

- [ ] **P2: Validate class-record and Export references at LOAD.** Finding B3.
  Build the class record-start bitmap once; require `super_class_ref`,
  implemented-interface and superinterface references, and Export
  `class_offset` to hit a record start of the right kind; require the
  superclass offset to be lower than the class's own; bound virtual-table
  method offsets and interface `index[]` values. Give
  `ConstantPool_Handler::resolveClassref` a full-record bound. Fixture: super
  reference pointing mid-record, and a self-referencing superclass.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-07).

### CAPIN-08

- [ ] **P2: Validate control flow and exception tables once at LOAD.** Prerequisite:
  RV-20261106-16 latency/RAM measurement before keeping a branch-result cache. Finding
  B4; closes R16. Call `cap_instruction_boundaries::validate_control_flow` for
  every enumerated method body and validate each exception-handler entry
  (range inside one method, handler at an instruction start, catch index a
  Classref). Keep the result so taken branches no longer rescan from the method
  start. Depends on CAPIN-10 (decoder widths) and CAPIN-01. Record the LOAD
  time for the JCAlgTest 1.8.4 CAP before and after.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-08).

### CAPIN-09

- [ ] **P2: Fix `itableswitch` operand widths.** Finding C1.
  `bc_branching.cpp` reads `low`/`high` with `getNextShort()`; they are four
  bytes each. Add a known-answer fixture with negative and large case values
  and cover `ilookupswitch` in the same test.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-09).

### CAPIN-10

- [ ] **P2: Fix `jsr`/`ret` and make decoder and executor agree.** Finding C2.
  Width 3 for opcode `0x71` in `src/cap_instruction_boundaries.hpp` with a
  signed 16-bit displacement in `validate_control_flow`; signed offset in
  `bc_jsr`; save the return address before branching so `ret` continues after
  the `jsr`. Add a unit test that compares decoder width with the bytes each
  executor handler consumes for all 256 opcodes.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-10).

### CAPIN-11

- [ ] **P2: Correct install-parameter delivery and external static offsets.**
  Findings C3, C5.
  1. `interpretor.cpp`: pass `bLength` without the `int8_t` cast (128–255
     currently become negative).
  2. Check how the supplied fixtures call `register`; then decide whether
     `Applet.install` must receive `Li|instance AID|Lc|control info|La|applet
     data` as the JCRE specifies, and record the decision before changing the
     layout. Validate the privileges field length.
  3. `bc_static.cpp:69`: use `Export_Handler::getExportedStaticFieldOffset`
     (byte-swapped, bounds-checked); deny `putstatic` into ROM package statics
     from dynamic callers.
  Fixtures: applet echoing `bArray`/`bOffset`/`bLength` with 0, 127, 128 and
  255 bytes; library with two exported statics read from an importer.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-11).

### CAPIN-12

- [ ] **P3: Consistency, cost and portability follow-ups.** Findings B5, C6,
  D1, D2. Each bullet is its own commit:
  - Descriptor↔Class cross-check independent of component order and CAP minor
    version; StaticField image size equation and `array_init_count` bound;
    delete the unused wrong `jc_cap_static_field_component` accessors.
  - `Stack::getCurrentFrame()` on an empty list; `run()` token-entry path
    outside `try`; `athrow` of a non-Throwable; `Frame::pop_Value` ordering;
    argument slots cleared and `nargs` matched on non-caller-stack entry.
  - One AID namespace for instance, package, ROM and ISD AIDs;
    `runtime_install_method` package check; stray LOAD block must not wipe the
    session view.
  - After CAPIN-01: validate once at LOAD and cache one validated `JC_Cap` view
    per slot keyed by CRC/generation instead of revalidating in every
    `getCap()`; measure before and after.
  - Replace `uint16_t&` views over packed CAP tables with byte-wise reads;
    (RISC-V is out of scope; ARM only.)

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-12).

### RV-20261103-11

- [ ] **P1: Cipher key copies survive original transient-key clearing.**
  Owner: PROFILE-01, HARD-01. Evidence: Source; [review R11](docs/review_20261103.md#r11--cipher-copies-outlive-transient-key-clear-events).
  Acceptance: retain checked key identity and appropriate lifetime metadata, or use workspace that clears with the key. Test retained Cipher objects across deselect, reader reset and fresh restore for symmetric and RSA transient keys. Modification of a key after `init()` is separately unspecified; this finding concerns defined lifecycle clear events.
  Progress: AES and RSA private Cipher workspaces now allocate sensitive copies as transient arrays with the source key's clear event and validate a transient marker before use. The current `./tools/test-cipher-lifecycle-vpcd` run passes 31/31 across deselect, reader reset and fresh emulator-process restore; deselect returns `UNINITIALIZED_KEY` (`42`), while reset and fresh restore return `INVALID_INIT` (`44`). Physical runs on 2026-11-07 did not reach the Cipher cases: after GPPro installed the probe CAP, SELECT checks passed but open-channel card-manager GET DATA returned `6982` instead of `9000`. The physical probe now explicitly resets the card after GPPro installation to clear any prior SCP03 session before testing; this setup change compiled but could not be reverified because `pcsc_scan -r` no longer published readers. D92 and failover board 2BED were firmware-only reflashed and both passed finite serial checks; card storage was preserved. Keep RV-11 open for the corrected physical AES/RSA lifecycle run.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#rv-20261103-11).

### RV-20261103-12

- [ ] **P1: Reset preserves deselect-cleared sensitive data.**
  Owner: CTX-03, PERSIST-02. Evidence: Reproduced; [review R12](docs/review_20261103.md#r12--reset-does-not-clear-clear_on_deselect-arrays).
  Acceptance: reset must clear both classes across active and inactive contexts without invoking applet `deselect()` as a reset callback. Test reset directly from the active applet, inactive package contexts and transient-key flags.
  Progress: the runtime reset callback clears both event classes in selected and inactive applet heaps. Fixed a VPCD regression where selected-applet tracking had reintroduced `deselect()` calls on POWER_OFF, POWER_RESET and disconnect; those boundaries now drop selection and invoke reset cleanup directly. Standalone VPCD session tests pass 29/29 and the same-CAP transient regression passes 6/6, including active reset, inactive writer-context clearing, and an unchanged persistent deselect counter. Physical validation remains pending: three USB CCID devices enumerate, but only the two VPCD readers currently appear in PC/SC.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#rv-20261103-12).

### RV-20261103-28

- [ ] **P1: Unbounded fetch, unrestricted native opcode and unchecked field kinds.**
  Owner: HARD-01. Evidence: Source; [review R28](docs/review_20261103.md#r28--uploaded-cap-execution-does-not-have-a-complete-trust-boundary).
  Acceptance: define the uploaded-CAP trust model explicitly; enforce execution bounds and privileged-opcode origin regardless of parser success, and add verification or dynamic field-kind/access checks. Test malformed instructions and nested linkage under sanitizers, then controlled physical failure/recovery. Keep valid upstream CAPs unchanged.
  Progress: Added `docs/CAP_EXECUTION_TRUST_MODEL.md`; deny `impdep1/2` from dynamic package IDs 56–63; reject truncated normal/extended method headers; and bound each frame to its Descriptor-recorded bytecode body. Uploaded packages fail closed if the method range is missing or invalid. The next descriptor method offset caps each body; extended headers are four bytes, and the corrected body start makes all 978 deployed 3.0.5 method bodies end exactly at the next method with no clamps. Dynamic-package taken branches now validate targets against decoded instruction starts, including switch destinations; trusted ROM branches retain prior behavior. Focused tests pass: native-opcode 5/5, method-header 7/7, body-range 11/11, PC bounds 19/19, and instruction-boundary 13/13, including 100,000 deterministic ASan/UBSan malformed bytecode inputs. An offline scan validated 8,720 branch/switch destinations across 978 concrete methods in 11 deployed 3.0.5 API CAPs; no out-of-range or misaligned targets were found, with zero byte-count clamps after correcting the extended-header size. ARM, RISC-V, and PC builds pass. Earlier VPCD attempts timed out before connection or hit a Rust host-emulator ptrace stop. Updated 2026-11-06: the rebuilt PC VM passes the unchanged upstream JCAlgTest install/APDU regression 15/15, the malformed impdep1 recovery case 6/6, and an upstream Crypto API CAP install/select 2/2; the exact upstream `Cipher.crypt()` API probe now passes 3/3 with response `000020019000`, validating execution of the 20-local extended-header method. Logs and remaining limits are in progress.md and .agent/HANDOFF.md. Instance field accessors now validate runtime field-kind tags, new fields start explicitly uninitialized, and the trailing word of an int is marked and checked; focused native tests and VPCD cross-AID/multipackage suites pass. `Static_Handler` now validates primitive widths against the CAP StaticField image, confines reference access to its leading reference region, and checks persisted primitive tags on later accesses. The Descriptor-present cross-AID VPCD suite passes 34/34; malformed static and instance declared-type cases each fail closed (1/1). Descriptor-backed first-store checks now cover all instance byte/short/int/reference bytecodes and static accesses when Descriptor is present. Standard GPPro LOAD omits Descriptor, so declared-kind checking is unavailable for those packages; Descriptor-free method execution uses bounded ranges inferred from enumerated callable offsets where available; private/internal StaticMethodref exact starts remain unauthenticated. Malformed runtime checks also cover impdep2, taken short/wide conditional branches, JSR into its displacement operand, RET reading a non-return-address local, out-of-range INVOKESTATIC/INVOKEVIRTUAL/INVOKESPECIAL/INVOKEINTERFACE CP indexes, and ITABLE/SLOOKUP/ILOOKUP switch destinations into operands; all invocation variants pass VPCD 6/6 with manager recovery. `ConstantPool_Handler::getCPEntry()` now checks the declared CP count explicitly and raises `SecurityException` regardless of `JCVM_ARRAY_SIZE_CHECK`; its CP accessor call chain is no longer conditionally `noexcept`. ARM, RISC-V, and PC builds pass, and a fresh JCAlgTest VPCD baseline passes 15/15. Pico `8E464F4E2BBC0A19` was flashed with the current ARM image; the bounded runtime monitor passed. This generic monitor does not exercise malformed CAP dispatch on hardware. A malformed exception-handler catch-type CP index now has a PC VPCD runtime recovery case (6/6); malformed handler-linkage variants, Descriptor-free operand linkage validation, sanitizer runtime recovery, and physical hostile-CAP recovery remain open. One identified Pico passed the current ARM max_crypto flash cycle, finite serial monitor, PC/SC reader enumeration, and GPPro empty-card listing on 2026-11-05. Physical JCSystem transaction/persistence acceptance also passed on Pico 2 `D92E743D5F640C0F`: 13/13 checks, including large persistent-array length, BUFFER_FULL rollback, CCID power-cycle persistence, and commit capacity. Standard GPPro CLI LOAD omits Descriptor and failed this CAP with `6A80`; the test-only `tools/load-cap-with-descriptor` uses GPPro APIs with `CAPFile.getCode(true)` and leaves the upstream CAP unchanged. Keep this workaround limited to test/development loads. The automated script needs a fresh end-to-end rerun because PC/SC enumeration disappeared after the successful manual run.
  Update 2026-11-06: ConstantPool structural parsing now rejects entry tags outside 1–6; ASan/UBSan helper tests pass 7/7, the CAP parser suite passes 4,208 framing checks over 48 files plus 100,000 fuzz inputs, and the deployed 3.0.5 method scan remains clean (978 methods, 8,720 branch targets). ARM and RISC-V max_crypto wrappers pass. Semantic target compatibility, malformed interpreter execution, and controlled physical failure/recovery remain open; this parser-focused step was not flashed.
  Update 2026-11-06: Install preflight now rejects external ConstantPool references whose package token is outside the declaring Import component. Focused ASan/UBSan tests pass 11/11; PC emulator max_crypto build passes; the unchanged upstream JCAlgTest VPCD run passes 15/15 including install, restore, SELECT, JCSystem, SHA-256, AES, RSA, random and CRC probes. ARM and RISC-V max_crypto wrappers pass. No physical card was flashed; external class/field/method export-token compatibility and malformed interpreter execution remain open.
  Update 2026-11-06: Install preflight now bounds each external ConstantPool class token against the imported package Export table after JC305 class/interface token mapping. Focused sanitizer tests pass 16/16; the unchanged JCAlgTest VPCD install/run passes 15/15; PC, ARM and RISC-V max_crypto builds pass. Member-token and internal-reference semantic validation plus malformed runtime execution remain open. This step was not flashed to physical hardware.
  Update 2026-11-06: External static-field tokens are checked against Export.static_field_count; static-method tokens accept the applicable JC305 static or constructor mapping only when within Export.static_method_count. Sanitizer helper tests pass 18/18, JCAlgTest VPCD passes 15/15, and PC/ARM/RISC-V max_crypto builds pass. Instance-field and virtual/super-method token checks, internal targets, and malformed runtime execution remain open. An initial ARM build caught a temporary-reference lifetime warning; named Package/Export_Handler objects fixed it. No physical flash.
  Update 2026-11-06: External instance-field tokens are bounded against the exported class's declared instance layout and reference-range metadata; external virtual-method tokens use the production class resolver with the original imported token mapping. External SuperMethodref entries are rejected because the format requires a package-local class reference. Sanitizer helper tests pass 21/21, unchanged JCAlgTest VPCD execution passes 15/15, and PC/ARM/RISC-V max_crypto builds pass. Internal CP targets and malformed interpreter runtime execution remain open. No physical flash.
  Update 2026-11-06: Internal StaticFieldref offsets are bounded by StaticField.image_size, and internal StaticMethodref offsets by the Method component's executable method-body area. Sanitizer helper tests pass 25/25; unchanged JCAlgTest VPCD execution passes 15/15; PC/ARM/RISC-V max_crypto builds pass. Internal class and instance-member target validation and malformed runtime execution remain open. No physical flash.
  Update 2026-11-06: Internal ConstantPool class references must target an exact Class-component record start; InstanceFieldref/VirtualMethodref/SuperMethodref reject interface records. The record map handles both CAP 2.1 layout and CAP 2.2 signature-pool prefix. Sanitizer helper tests pass 30/30; unchanged JCAlgTest VPCD execution passes 15/15; PC/ARM/RISC-V max_crypto builds pass. Internal class/member token semantics beyond exact class-kind/bounds and malformed interpreter execution remain open. No physical flash.
  Update 2026-11-06: Internal InstanceFieldref tokens are bounded by their target class layout; VirtualMethodref and SuperMethodref tokens now resolve through the production class-method resolver (SuperMethodref via the named class's immediate superclass). Sanitizer helper suite remains 30/30, JCAlgTest VPCD passes 15/15, and PC/ARM/RISC-V max_crypto builds pass. Exact StaticMethodref method-header starts, malformed bytecode execution, and physical hostile-CAP recovery remain open.
  Update 2026-11-06: Corrected the internal StaticMethodref range to CAP Method-component coordinates: handler-table byte base plus one-based offset into the methods area, with an inclusive final offset. Helper suite passes 32/32; PC/ARM/RISC-V builds and JCAlgTest VPCD 15/15 pass. Exact method-header starts still require Descriptor evidence or a stronger descriptor-free verification source.
  Update 2026-11-06: When Descriptor is present, every internal StaticMethodref must now match exactly one Descriptor method record's Method-component offset. Descriptor-free LOADs retain only component-range/header checks; exact starts remain open there. Helper suite passes 35/35; PC/ARM/RISC-V builds and JCAlgTest VPCD 15/15 pass.
  Update 2026-11-06: Added a dynamic-package `impdep1` runtime fixture from an unchanged upstream JCAlgTest CAP (verified SHA-256); VPCD rejects applet SELECT with `6F00`, direct processing returns `6999`, and ISD selection succeeds after both failures. Malformed run passes 6/6; unchanged JCAlgTest regression passes 15/15. Emulator-only evidence; sanitizer and physical hostile-CAP recovery remain open. Continue with malformed operands and branches.
  Update 2026-11-06: Replaced the final `BC_RETURN` byte in the Descriptor-bounded process body with `BC_SIPUSH`, requiring two unavailable operand bytes. SELECT remains `9000`, malformed process returns `6F00`, and later ISD/applet selections succeed; VPCD test passes 6/6. Emulator-only; physical/sanitizer recovery remains open. Next add malformed branch-target runtime coverage.
  Update 2026-11-06: A dynamic JCVM `GOTO +1` targeting its own operand is rejected (`6F00` during SELECT, `6999` during process); ISD selection continues to succeed. VPCD test passes 6/6. The first fixture encoded a self-loop and hit the bounded timeout; corrected two-byte GOTO encoding passes. Emulator-only; physical and sanitizer recovery remain open.
  Update 2026-11-06: Dynamic `GOTO_W +32767` beyond the process frame is rejected (`6F00` on SELECT, `6999` on process), and ISD selection still succeeds. VPCD test passes 6/6. Emulator-only; sanitizer and physical hostile-CAP recovery remain open.
  Update 2026-11-06: Enabled ASan/UBSan in the PC-bound and method-body-bound helper wrappers. PC bounds pass 19/19 and Descriptor/Descriptor-free method ranges pass 11/11 under both sanitizers. Instruction-boundary suite passes 13/13, including 100,000 deterministic malformed inputs under ASan/UBSan. No source firmware changes or physical test.
  Update 2026-11-06: Dynamic `STABLESWITCH` default/case targets into the switch displacement operand fail closed (`6F00`/`6999`), with subsequent ISD selection passing; VPCD test passes 6/6. No physical card used.

Additional scoped investigations: GP-01's imported-library deletion policy is
now implemented and VPCD-tested: deleting a loaded package referenced by any
other loaded package's Import component is rejected with `6985`; the client
continues to select and invoke its service. The opt-in regression and default
suite pass. It then deletes the client and service importers and confirms the
library package DELETE succeeds with `9000`. Physical CCID validation remains
pending. A separate VPCD case confirms that a loaded but uninstalled service
also blocks library DELETE (`6985`), and removing that package permits library
DELETE (`9000`). CTX-01/STORE-01 should
establish bounded lifetime/reclamation for user-created global arrays. These
remain source-based follow-ups without a new physical reproduction in this
review.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#rv-20261103-28).

### STORE-01

- [ ] **Validate package-delete compaction over repeated physical API sweeps.**
  Status: package-state compaction is implemented, and new writes now trigger
  compaction when deleted records block a write at the index or payload limit.
  Compaction retains and remaps legacy parent-slot references. Full-index
  fake-flash coverage passes, including restart. Physical testing on
  `2BED26ED20556535` reproduced `FS_WRITE_FAIL files=8192/8192 free_slots=0`
  and the fixed firmware compacted to 696 records / 16,749 bytes; GPPro client
  and service applet INSTALLs then returned `9000`. This does not establish
  that every full card has reclaimable tombstones; a fully live set still
  requires owner-scoped cleanup or a capacity change.
  Earlier 77,824-byte arena exhaustion is distinct from later v4
  capacity/index pressure and from persistent-object reclamation.
  Remaining: repeat upstream CAP install/run/uninstall cycles; monitor payload
  usage and the 8,192-file limit, reclaim index slots after package deletion,
  and identify whether live test data legitimately exceeds capacity.
  Acceptance: complete bounded long sweeps with cleanup and storage diagnostics;
  explain residual failures without assuming every `NO_MEMORY` is a GC defect.
  Dependencies: API-01 measures in-run pressure; PERSIST-01 owns object deletion.
  Evidence: original max_crypto storage-capacity section and current QKB/progress.
  Latest max-profile evidence (2026-11-03): physical API 3.0.5 Security and
  Crypto sweeps completed on identified boards; the result sets matched VPCD.
  Crypto runner cleanup reported reader loss on both boards, and the test
  package remained listed on 2BED. Two targeted GPPro DELETE attempts timed
  out after sending DELETE; reflashing restored the reader but preserved the
  package. This leaves repeated install/run/uninstall acceptance open; see
  `.agent/QKB.md` and `.agent/HANDOFF.md` for exact board IDs and artifacts.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#store-01).

### PERSIST-01

- [ ] **Complete safety acceptance before enabling normal persistent reclamation.** Owner-scoped inventory and bounded reclaimer primitives exist; incomplete roots, malformed/unreadable aliases, cursor errors or transactions fail closed. Stable owner IDs, direct applet roots and cross-namespace alias protection are required. Current diagnostic root/budget progress is summarized under RESOURCE-01.
- Evidence: a physical single-owner hook erased exactly one orphan while preserving the live root; host tests cover owner isolation, sibling roots, bounded batches, erase failures/retry and 1,000 modeled power cuts using the production reclaimer. A cross-owner two-record alias cycle now returns `InvalidTag` before any erase; after removing the corrupt cycle, a retry reclaims both target records. These do not establish physical multi-owner or power-loss safety.
- Remaining: physical multi-owner/foreign-alias inventory, real per-record interruption and restart/retry, complete retained-context/native-root audit, stable handles and physical APDU timing. Acceptance: all live roots/aliases and other owners survive; interrupted batches resume safely; incomplete reachability never permits deletion. Coordinate PERSIST-02/03; object deletion is enabled in the normal build (SPEC INV-24), so these safety gates apply to the default image.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#persist-01).

### PERSIST-02

- [ ] **Verify applet/alias recovery after actual USB power removal.**
  Status: latest PC VPCD persistence passes install/commit 3/3 plus fresh-process
  restore and four GC-pressure rounds 38/38. Native tests retain static/APDU/
  native-mirror, result, exception, RMI, transient and instance/array graph roots;
  malformed roots/aliases block collection. Multipackage restart tests cover
  distinct owners, root inventory and retained contexts. These are host checks.
  Physical 2BED scalar and 32,767-byte array checks passed 9/9 across card reset
  and firmware cycle; those do not remove USB power. On 2026-11-02, identified
  runtime boards `8E464F4E2BBC0A19` and `2BED26ED20556535` both passed the
  read-only `tools/test-pico-powerup` registry and JCAlgTest probe; each
  returned ATR `3B800181`, SELECT `9000`, version `1.8.2_jc305`, and JCSystem
  info `9000`. This was not a USB power-removal test and the deployed image hash
  was not identified by the probe. On 2026-11-03, both boards also passed the
  same probe against the installed upstream `1.8.4_jc305` applet after adding
  `JCALGTEST_EXPECTED_VERSION` support; each returned the same ATR, successful
  SELECT, GETVERSION, and JCSystem info. This remains a warm read-only probe,
  not a USB power-removal test.
  Remaining: cold-power restoration of instance identity/fields, nested aliases,
  static roots and multi-instance owner state; verify corrupted/incomplete
  records cannot authorize reclamation after restart.
  Acceptance: record actual disconnect/reconnect and exact image/board identity;
  verify data and isolation after boot, with negative recovery cases as relevant.
  `tools/test-pico-powerup` is read-only and explicitly reports
  `power_cycle=not_verified_by_script`; it cannot itself prove power removal.
  Select `JCALGTEST_EXPECTED_VERSION=1.8.4_jc305` explicitly for current acceptance; the historical script default must not select the baseline.
  Historical AC5 ordinary JCAlgTest power-cycle evidence is retained, but does
  not close these newer alias/reclaimer gates. Coordinate PERSIST-01 and AC-02.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#persist-02).

### PERSIST-03

- [ ] **Finish physical cross-store install/rollback and power-cut recovery.**
  Status: CAP copy-on-write metadata and the CRC-protected INSTALL journal
  implement publication/replay/clear recovery. Fresh-process replay checks pass
  skipped publication 17/17, already-published 17/17, metadata failure 15/15,
  and journal-clear sync failure 17/17. RP2350 host-model tests cover 1,000-cut
  creation, clear and cross-store publication scenarios. These are simulated cuts.
  Remaining: physical failed-INSTALL status/rollback validation and controlled
  interrupted durable updates across CAP registry and filesystem. Preserve
  existing installed data; plan each physical fault-injection experiment first.
  Acceptance: after each interruption, recover a consistent prior/new install,
  preserve unrelated state, replay idempotently, and return documented failure
  statuses. Do not reopen the implemented journal as a missing feature.
  Evidence: `tools/test-rp2350-fs-overlay`, VPCD recovery records in progress,
  original atomic-recovery and failed-install entries. Coordinate PERSIST-01/02.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#persist-03).

### HMAC-01

- [ ] **Finish native HMAC acceptance.** SHA1/256/384/512, MD5 and RIPEMD160 plus HMAC key variants are implemented in native C++ dispatch. Focused one-shot/split/update/verify/tamper/readback vectors pass (14/14 VPCD checks); both target builds pass. Remaining: lifecycle tests, full unchanged JCAlgTest 1.8.4/jc305 support comparison and physical validation. Full-suite storage exhaustion is owned by RESOURCE-01; host compaction cost has since been addressed by PERF-01/02. Preserve upstream CAPs and do not infer physical HMAC support from emulator results.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#hmac-01).

### JC-RESET-01

- [ ] **Diagnose reader loss during JCAlgTest Signature factory combinations.**
  Source-analysis refinement: `native_signature.cpp::algorithmForTuple()`
  rejects the reported RSA/OAEP/SHA3 tuple before `createSignature()` allocates
  an instance or buffers. `bc_object.cpp::dispatchJavaException()` routes this
  through `Heap::addRuntimeCryptoException()`, which caches the exception.
  Thus a fresh Signature/exception allocation for every unsupported tuple is
  not supported by current source. Verify these paths against the exact 0.1.1
  build commit before attributing them to the failing image. Earlier successful
  constructors, exception field writes, GC, storage and transport remain in
  scope. The 1.8.4 reference's next tuple is RSA / OAEP-SHA3-512 / ALG_NULL;
  reference CSV order alone does not establish the next actual APDU. Inspect
  client buffering and applet batching: the last saved row is a completion
  boundary, not proof that the very next individual tuple caused the failure.
  Evidence (user report): firmware release `0.1.1` with the `max_crypto`
  profile was being exercised using JCAlgTest `ALG_SUPPORT_EXTENDED` on
  Windows PC/SC. During calls to
  `javacard.crypto.Signature.getInstance(byte messageDigestAlgorithm, byte
  cipherAlgorithm, byte paddingAlgorithm, boolean externalAccess)`, the reader
  stopped appearing to Windows PC/SC. The last saved CSV row was reported as
  line 5627: `SIG_CIPHER_RSA PAD_PKCS1_OAEP_SHA3_384 ALG_SHA3_512;no;
  0.212000;0;0;0`. Unplug/replug restored reader visibility and use without
  reflashing. The exact run CSV and host/device logs are not currently present
  in the repository; preserve this as reported evidence until obtained.
  Hypotheses to discriminate, not established causes: Signature factory or
  native crypto allocation leak/exhaustion across the large combination sweep;
  persistent/transient heap or registry pressure; a VM exception/fault, deadlock
  or watchdog/reset; and CCID/USB or Windows PC/SC transport/driver loss. The
  preceding `no` result is a normal algorithm support outcome and does not by
  itself establish a firmware defect. Replug recovery without reflashing makes
  transport recovery or reset plausible, but does not rule out firmware state
  failure.
  Worker: first obtain and preserve the exact CSV, client/JAR and CAP hashes,
  host logs, image hash/build profile, and device identity if available. Map
  the reported row to the matching client’s actual execution order (including
  whether row numbering includes a header), then inspect the corresponding
  native `Signature.getInstance` dispatch and allocation/lifecycle paths.
  Reproduce offline with the unchanged pinned JCAlgTest 1.8.4 / jc305 CAP
  and the 1.8.5 host client on a fresh VPCD image; add focused repeated factory-call
  and resource-accounting probes if needed. Capture APDU progress, SW/VM fault,
  allocator/persistent capacity, USB/CCID reset and host reader state to
  distinguish causes. Do not start a dedicated physical reproduction under this
  task; reader loss or stalls seen during physical option 1 are recorded under
  HOST-01, which owns transport failures. This task feeds evidence to HOST-01.
  Acceptance: provide a cause or a bounded set of remaining hypotheses with
  evidence; an offline reproducer or explicit non-reproduction bound; any
  required code fix and VPCD regression; and build results. Physical validation,
  if still warranted, requires a separate authorized task.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#jc-reset-01).

### API-01

- [ ] **Diagnose cumulative Security/Crypto memory pressure and finish Security.**
  Status: the current max_crypto source completed the full API 3.0.5 Security
  area on fresh PC JCVM storage over VPCD on 2026-11-03: all 901 calls finished
  (397 OK, 504 expected `UNSUPPORTED_PER_SPEC`), and GPPro removed the test CAP.
  There were no `NO_MEMORY`, `WRONG_RESULT`, `EXC_RUNTIME`, SW, transport, or
  missing-call results. This emulator run did not reproduce cumulative memory
  pressure; it does not replace physical acceptance. Older 2026-11-01 max_crypto
  Crypto results on 2BED were:
  292 OK, 161 unsupported, nine NO_MEMORY, three WRONG_RESULT and one
  EXC_CARDRUNTIME; zero EXC_RUNTIME. Earlier 543 Security / 206 Crypto runtime
  outcomes and isolated null-receiver failures are historical baselines, not
  current-image results. Their cause was not established.
  Focused current RSA/RSA-CRT-512 generation passes; filtered KeyAgreement /
  Cipher returned 50 OK and 15 unsupported without NO_MEMORY, then timed out
  before AEAD. Security on 8E stopped at transient RSA-private-2048 allocation
  with SCARD_E_NOT_TRANSACTED; the reader vanished before cleanup.
  Remaining: complete the current-image physical Security/Crypto sweeps when a
  Pico is available; capture APDU and allocator/persistent state if an unexpected
  failure recurs; check cleanup/registry after interrupted attempts. The VPCD
  Security sweep is complete, but physical memory behavior remains unverified.
  Acceptance: complete current-image physical sweeps, account for every
  unexpected result and resource limit, and do not treat filtered timeouts as
  complete isolation.
  Dependencies: STORE-01, PERSIST-01; IV rows are owned by API-02.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#api-01).

### API-03

- [ ] **Close current-profile physical deploy coverage and remaining retests.**
  Status: Core on 2026-10-31 max_crypto completed 291 rows (272 OK, 15 expected
  unsupported, four optional Ext scenarios unavailable after `6A88`); cleanup
  passed. Older deselect and deleted-context 6F00 defects are fixed.
  Security/Crypto coverage and remaining failures are tracked by API-01/02.
  The previous max_crypto UF2 exceeded the firmware's 1 MiB flash window
  (its end address was `0x101000c4`); the current ARM build uses `-Os`, ends at
  `0x100e75f4`, and the build wrapper rejects firmware/CAP overlap. This exact
  image was flashed to Pico 2 `2BED26ED20556535` on 2026-11-02, flash
  verification reached 100%, and finite runtime monitor passed. Its UF2 is
  1,895,936 bytes (SHA-256
  `16fd3afabc24b447a9eaf94989b676bdaa8e929b399cea5056b5b9be7b906b4c`); linked
  firmware uses 947,700 of 1,048,576 bytes, leaving 100,876 bytes.
  On 2026-11-02, the official JCAlgTest 1.8.2 `ALG_SUPPORT_BASIC` run on this
  image completed 362 rows (148 yes, 214 no, no other statuses). All 142
  expected support identities covered by BASIC reported yes. Two RSA/RSA-CRT
  3072 rows are the known upstream CAP/client defect: the rows are labeled
  3072 but send 4096, correctly receiving no because 4096 is disabled. Three
  expected `Cipher.getInstance` aliases are absent from BASIC and remain to be
  checked by EXTENDED. A separate `ALG_SUPPORT_EXTENDED` run hit its
  900-second wrapper timeout after 2,094 CSV lines; its last in-flight query
  returned `9000` in 163 ms when isolated after reflash. This is an incomplete
  matrix, not evidence of a repeatable APDU hang. The board was recovered by
  chip-ID-targeted flash and passed its monitor.
  Optional Javacardx/Ext/Int/Biometry imports may remain unavailable for the
  declared profile; prior rejection is not evidence of implemented coverage.
  Remaining: finish `ALG_SUPPORT_EXTENDED` with a suitable total deadline;
  explicitly verify the three `Cipher.getInstance` aliases, maintain an
  area/profile/result matrix, retest changed native paths, and account for
  unavailable scenarios and unsupported setup dependencies. Verify current-
  source Service/JavacardxSec results where affected.
  Acceptance: complete bounded runs and cleanup on identified images; require
  every ARM image to fit the 1 MiB firmware window enforced by `tools/build`;
  separate expected unsupported outcomes from wrong results, runtime errors and
  capacity failures. Do not count serial `choupi_runtime` heartbeat as API
  conformance.
  Dependencies: API-01/02, PROFILE-01, GP-01; EMU-01 handles host throughput.
  Evidence: current progress/QKB; original deploy and focused retest entries
  retain exact CSV/log paths, hashes, selected calls and historical baselines.

- [ ] **Resolve JCAlgTest (1.8.4/jc305 CAP) static-performance cases that could not be measured.**
  Status: on Pico 2 `2BED26ED20556535`, the unchanged upstream 1.8.4 client
  completed the full static matrix in 2,209 seconds on the 60-second-budget
  firmware: 1,847 expected `NO_SUCH_ALGORITHM` results and four
  `CANT_BE_MEASURED` cases. The two EEPROM copy calls that had returned `6400`
  at 15 seconds now completed (about 25 seconds per APDU). `SWAlgs_AES()` hit
  the 60-second guard at 60,497 ms. Raising the finite non-install budget to
  120 seconds and servicing watchdog/CCID progress every 4,096 bytecodes made
  the exact 50-iteration SW AES APDU return `AA9000` on hardware.
  The three DES `clearKey()` `6F00` errors were caused by the JC305 DESKey
  interface-token map omitting the inherited Key-method remap. Correcting the
  map to `{1,3,2,0,5,4}` made the exact 64/128/192-bit `clearKey()` measurements
  return `AA9000` on the final ARM image. The final image passed a verified
  flash and finite runtime monitor on 2BED.
  Remaining: rerun the complete static suite (unchanged 1.8.4/jc305 CAP, 1.8.5
  client) on the combined final image. The attempted final run did not start because authorized host
  refresh and PC/SC enumeration found zero physical Pico readers (only the two
  VPCD readers); this is a device-availability gap, not a test failure.
  Do not classify `NO_SUCH_ALGORITHM` results as failures.
  Acceptance: each supported operation returns a valid result and each
  unsupported operation returns the expected client status; full final sweep
  has no unexpected `6F00`/`6400`, reader loss, or `CANT_BE_MEASURED` cases.
  Evidence: full 2BED run in
  `artifacts/results/jcalgtest-184-static-2bed-cooperative-20261103/`, focused
  DES validation in `artifacts/logs/jcalgtest-184-des-clear-2bed-20261103.log`,
  and final flash/runtime log `artifacts/logs/cycle-choupi-20261103T150345Z.log`.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#api-03).

### CTX-01

- [ ] **Finish SIO, proxy and firewall acceptance.** Retained contexts, lazy lookup, typed cross-context calls/returns, owner checks, stale-proxy rejection, same-AID reinstall and scoped foreign static/instance/array retention are implemented with VPCD and some physical checks. No blanket firewall acceptance; channel 0 is the current assignment, and general cross-context calls/logical-channel policy remains incomplete.
- Remaining: audit every foreign-value consumer/root and additional stale-owner cases; same-CAP/multipackage physical lifecycle; GC/reset/DELETE roots and reference isolation; unchanged upstream Multi CAP integration. Preserve fail-closed persistence limits or implement standards-compatible proxy rehydration before broadening support.
- Roll back a newly retained server root if proxy allocation/registration fails; test proxy-table exhaustion and allocation failure without removing pre-existing roots.
- Latest physical APDU checks passed on all three boards, but cleanup lost readers and a clean registry remains unverified. Respect package-static dependencies when ordering service/client deletion. Foreign-static replacement/clear exposes orphan subtrees in inventory; automatic reclamation remains disabled. Complete cleanup and physical coverage on an identified current image.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#ctx-01).

### CTX-02

- [ ] **Finish physical SensitiveResult acceptance across SIO.** Supported producers have interpreted SIO coverage for primitive/object results, true/false, invalid-call clearing and explicit reset. Native tests cover sole-root GC lifetime; no synchronous public CAP GC trigger combines these phases. Checksum and AEAD remain outside producer coverage while unsupported.
- Remaining: broaden exact-reader physical coverage beyond the JCSystem probe, including AID, atomic Util and reset; close JNI argument-conversion invalidation under RV-20261106-25. Acceptance: correct ownership/lifetime across call/return, GC and reset; keep native simulation distinct from interpreted CAP evidence.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#ctx-02).

### CTX-03

- [ ] **Finish one-firewall-context-per-CAP acceptance while retaining per-instance ownership.** Same-CAP sharing, instance lifecycle, transient rules, restart and owner selection have focused VPCD/physical coverage. Owner IDs 1..255 are tested through native registry and fake-flash allocation/exhaustion/reuse; full 255-owner VCR2 deserialization is untested.
- Remaining: review GC/reset paths; full-registry restore; persistence and current-image physical acceptance, including sibling CLEAR_ON_DESELECT allocation through SIO and separate-CAP rejection. Acceptance: sibling instances share static/heap context as specified, separate CAPs stay isolated, AID reporting and instance DELETE preserve intended state. Include multipackage/restart regressions before physical tests. Coordinate CTX-01, STORE-01, PERSIST-01/02 and API-03.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#ctx-03).

### PROFILE-01

- [ ] **Audit the declared API contract and close focused applet-test gaps.**
  Target: [Java Card Classic 3.0.5 / CAP 2.1–2.2](docs/supported-profile.md),
  with explicit exclusions; target compatibility is not certification.
  Status: native JCSystem, APDU, lifecycle, exception and persistence paths have
  focused tests; JCAlgTest install/resume passes 30/30, transaction VPCD 85/85.
  Physical Core coverage does not prove every negative/low-memory path.
  Remaining: map implemented methods to unchanged upstream CAPs or focused
  golden tests; verify select rejection/deselect callbacks, short-APDU state and
  `61xx`/GET RESPONSE, current APDU/buffer restrictions, null/bounds/type errors,
  getAID/AID comparisons, transient allocation errors, install parameters and
  persistent restore. Retain any physical gaps not demonstrated by API-03.
  Acceptance: a method/behavior/evidence matrix covering both error and success
  paths, with source profile and host/physical scope explicit. Do not reimplement
  working lifecycle/API methods solely because old checkboxes were unchecked.
  New API/crypto behavior belongs in native C++; leave upstream CAPs unchanged.
  Dependencies: CTX-01/02, API-03, PERSIST-02/03 for their respective contracts.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#profile-01).

### GP-01

- [ ] **CAP-REINSTALL-01: Reproduce and close stale SIO routes after package DELETE.**
  Owners: GP-01, CTX-01/03. The issue is now reproduced and fixed in VPCD.
  A client retained a server proxy; package DELETE succeeded with the client
  still installed; after same-AID package reinstall the old proxy incorrectly
  dispatched (`45679000`). Proxy routes now record the server package ID, and
  successful package DELETE invalidates those routes across selected and
  inactive contexts. The opt-in regression verifies old proxy `6A0A`, null
  lookup after deletion, then old proxy `6A0A` and fresh service result
  `45679000` after reinstall. The registry unit test confirms another
  package's route remains. A third unrelated applet package now remains
  selectable and executable across successful service package DELETE,
  injected late-cleanup failure/restart, and same-AID service reinstall.
  Same-CAP sibling/restart/delete suites pass 6/6, 7/7, and 9/9. Remaining:
  repeat the package DELETE/reinstall APDUs on physical Pico hardware; PC/SC
  currently enumerates no Pico reader. Keep open until hardware acceptance.
  Evidence: [CAP reinstall review](docs/cap-reinstall-review-20261105.md),
  `.agent/QKB.md`, and `progress.md`.

- [ ] **CAP-REINSTALL-02: Reconcile runtime contexts after a late DELETE failure.**
  Owners: GP-01, PERSIST-02, CTX-03. Reproduced and fixed in PC VPCD and CCID
  paths: persistent DELETE can commit before package/instance metadata cleanup
  returns `6581`, so runtime contexts and their SIO routes must be invalidated
  whenever that commit occurred, regardless of the final status word. Injected
  package and instance cleanup failures both return `6581`; same-process
  reinstall keeps the old proxy blocked and a fresh lookup reaches the new
  service (`45679000`). Both injected cases also pass across PC VPCD restart:
  stale proxies remain blocked, the unrelated client remains selectable, and
  same-AID package reinstall restores fresh lookup. Remaining acceptance:
  repeat the cases on physical Pico hardware. Evidence in `.agent/QKB.md` and
  `progress.md`.

- [ ] **CAP-REINSTALL-03: Verify changed-version CAP replacement end to end.**
  Owners: GP-01, CTX-03, AC-02. Current same-CAP tests reload identical bytes.
  Use two distinguishable CAP versions with the same package/applet/instance
  AIDs, following AGENTS.md fixture and native API restrictions. Verify that
  instance-only DELETE retains the package and duplicate LOAD fails (`6A80`),
  then perform full package DELETE/LOAD/INSTALL and check every status.
  Acceptance: only the new version's behavior appears in the same process and
  after restart, including shorter/longer code and changed static layout; old
  static/instance state is absent after package replacement. Archive CAP hashes,
  APDU outcomes and behavior evidence; validate on physical Pico hardware after
  emulator coverage. Document package versus instance deletion in upgrade
  instructions so a rejected LOAD cannot be mistaken for a successful upgrade.
  VPCD coverage now passes both V1→larger V2 and V2→shorter V1 replacement,
  including duplicate LOAD `6A80`, fresh defaults, changed behavior and state
  restoration after emulator restart. The review document records exact hashes
  and APDU outcomes. Remaining acceptance: repeat on a physical Pico after a
  reader appears in PC/SC.
  Evidence: [CAP reinstall review](docs/cap-reinstall-review-20261105.md).

- [ ] **Finish remaining GET DATA and cold-power registry validation.**
  Status: selected GP GET STATUS/module listing and CPLC/recognition responses
  are implemented; the old exact-64-byte CCID stall is fixed by terminating ZLP.
  Physical first/next query binding and Le-limited multipage AID lists passed.
  Automatic checksum-bound VPCD registry discovery/restart is implemented;
  earlier statements that it is missing are superseded.
  Remaining: validate GP `80 CA` BER-TLV versus ISO `00 CB` value responses and
  dynamic `2F00` with `5C00` across open/authenticated PCSC and physical CCID,
  including `61xx`/GET RESPONSE, malformed data, restart and cleanup. Unit tests
  pass 25/25 and VPCD lifecycle passed 42/42 at implementation; later suite 47/47.
  The dedicated VPCD probe now passes open and SCP03-authenticated GP/ISO card
  recognition, CPLC, and dynamic `2F00` response checks. Physical CCID checks
  remain open. The complete multipackage VPCD suite passes 47/47 with a
  120-second APDU bound, including `61xx`/GET RESPONSE for all 21 installed
  AIDs and identical list reconstruction after emulator restart. The 18-instance
  cascade DELETE exceeds a 60-second bound.
  Verify multi-package registry reconstruction across actual cold power via
  PERSIST-02/AC-02. Selected-profile completion does not close these extensions.
  Acceptance: exact response/status and each AID once, correct continuation,
  preserved registry; no emulator-only claim for hardware CCID behavior.
  CPLC identifiers are synthetic, not factory identity; `0067`, `00E0` and
  unknown unavailable objects remain `6A88` unless separately selected as scope.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#gp-01).

### HOST-01

- [ ] **Diagnose recurring physical USB/PCSC reader disappearance and CCID response stalls (sole owner; blocker for MEM-01 T8).**
  RESOURCE-01 and JC-RESET-01 feed their transport evidence here (e.g. the
  2026-11-14 8E option-2 `SCARD_E_NOT_TRANSACTED` → `SCARD_E_READER_UNAVAILABLE`).
  Status: reader loss has occurred after flashes, long APDUs and reconnects;
  an absent guest USB device is distinct from visible USB with no PCSC reader,
  and from an APDU returning a card error. No common cause for reader
  publication is established; a response-retry defect is now identified.
  Remaining: capture exact board/image, USB visibility, PCSC registration,
  firmware APDU telemetry and bounded failure/recovery evidence per layer.
  New 2026-11-14 D92 evidence: on the debug ARM image, `B043000016` completed
  VM execution with `9000` and `fs_sync` took 1,269,927 us, but response
  submission reported `pending=1`, `in_busy=0`, `submit_errors=0`; no IN
  completion or recovery telemetry followed. Client 1.8.5 failed at AES-192
  `Cipher_init()` with `SCARD_E_NOT_TRANSACTED`. USB runtime devices stayed
  visible; `pcsc_scan -r` did not enumerate a reader after the approved helper
  refresh. The run showed no memory exhaustion statuses. See QKB D92 response
  stall entry and `artifacts/results/jcalgtest-1.8.5-physical-20261114-ccid-debug/D92E743D5F640C0F/option2-performance/`.
  2026-11-14 follow-up: `service_pending_final_response()` used a zero retry
  timestamp as "immediately due" but compared it with signed `time_us_32()`
  arithmetic. At uptime ≥2^31 us (35m 47.483648s), `int32_t(now - 0) < 0`, so
  a new response returns before submission and before response-wait telemetry.
  D92's ≈45-minute failure and pending/idle/no-submit-error trace match this
  path. Replaced the zero sentinel with an explicit armed flag; unit checks
  cover long uptime and 32-bit wraparound (35/35 pass), and ARM acceptance build
  passes. Fresh no-debug, deletion-enabled image is `WIN-OBJECT-DELETION-NODEBUG-CCID-DEADLINE-805420E6` in
  `.agent/EXT_REQUESTS.md`. Source-level diagnosis and fix are not yet physical
  confirmation; leave HOST-01 open through a sustained fixed-image option-2 run.
  2026-11-15 PC/SC trace: USB enumeration showed three `2e8a:0009 Pico2
  JCVM CCID` devices. SmartcardIO connected successfully to
  `/run/pcscd/pcscd.comm` and sent its PC/SC requests, then blocked waiting for
  the response to the reader-list request until the eight-second client bound
  killed it. `pcscd` remained alive about 1:20 after the client exited (main
  thread reported 1.6% CPU); this is a daemon-side/no-response observation, not
  proof of a specific USB driver fault. Trace:
  `artifacts/logs/smartcardio-list-readers-strace-20261115.log`.
  2026-11-15 external Windows report: the superseded `805420e6` image (UF2
  SHA-256 `5fa91117aeb6b2ac1c6ebb537447090016b27f0717bd14e528cd5d9437e4a6de`)
  reached a 50-iteration RSA-1984 NOPAD `Cipher_doFinal()` measurement after
  its one-operation baseline APDU returned `AA9000`; the repeated measurement
  then ended in Windows PC/SC `0x8010002f` (`SCARD_E_COMM_DATA_LOST`), followed
  by `0x1f` on SELECT and `SCARD_E_SERVICE_STOPPED`. The report's client JAR
  differs from the pinned 1.8.5 JAR in 3 of 66 classes (`AlgTestJClient`,
  `CardMngr`, `PerformanceTesting`). No serial log or chip ID was provided, so
  firmware reset/fault versus host/CCID transport loss cannot be distinguished.
  This was the superseded image; current-image evidence is required. Exact hashes
  and timing are in `.agent/QKB.md`; raw evidence was archived from `inbox/` to
  `artifacts/results/windows-maxcrypto-20261114-805420e6/`. The follow-up on
  `25216292-dirty` reproduced it; diagnosis and fix are tracked in
  [LONGOP-01](#longop-01).
  Acceptance: identify a reproducible cause/fix or a concrete host limitation;
  never attribute absent enumeration to crypto or a reader-count firmware limit.
  Follow AGENTS host-access rules from the first USB/PCSC operation, use exact
  current reader names, and fail over only to another identified available board.
  Evidence: QKB USB/PCSC records. Old available/empty-card states are not current.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#host-01).

### LONGOP-01

- [ ] **P0 sub-task of HOST-01 (T8 blocker): the RSA-1984 private-key ×50 measure APDU loses the reader every time.** Opened 2026-11-15.
  **Symptom.** In JCAlgTest option 2, the first RSA-1984 private-key `B043000016` with 50 repetitions (`… 00 32 00 01 00`) fails with Windows `0x8010002f`. This happened 3 times out of 3, on `805420e6` (NOPAD) and twice on `25216292-dirty` (PKCS1), including once from a fresh boot after only SELECT, `B031` and five baselines.
  - Afterwards, applet SELECT gives `0x1f` or `0x8010002f`, and in 2 of 3 runs the reader disappears (`SCARD_E_SERVICE_STOPPED`). A replug restores the reader, and applet SELECT returns `9000`. No persistent damage was observed.
  - Single-op baselines of the same operation pass (`AA 9000`, 2.2–3.6 s). ×50 loops up to RSA-1536 pass (≈23.7 s). A 44.3 s RSA-1984 keygen APDU passes; keygen sends extensions through its RNG callback.
  - Expected duration of the failing APDU is ≈43–50 s (1984 non-CRT ≈0.8–0.95 s/op, extrapolated from 152/267/430 ms at 1024/1280/1536).
  - Serial (attempt 1 only): the last record is the `B0E2` that preceded the ×50 APDU, then `*** PANIC ***` with no message. Panic messages are lost because USB is no longer serviced after the panic.
  - Evidence: `artifacts/results/windows-maxcrypto-20261114-805420e6/` and `artifacts/results/windows-maxcrypto-20261115-25216292-dirty/` (`readme.txt` is the authoritative report; client JAR unverified or not pinned, so not acceptance). Analysis: QKB "RSA-1984 ×50 measure APDU".
  **Confirmed defect (D1, no time extension in modexp).** RSA modexp (`choupi/src/native_rsa_cipher.cpp` `modExp` / `rsaCrtModExp` → `mbedtls_mpi_exp_mod`) has no `choupi_native_crypto_progress()` call. The interpreter calls `choupi_vm_cooperative_progress()` only every 4096 bytecodes (`choupi/src/interpretor.cpp`), and a JCAlgTest ×N loop runs far fewer. Result: no CCID time extensions and no USB servicing for the whole APDU, contrary to SPEC "Deliberately unspecified" (long native operations cooperate with CCID time extension and the watchdog). The watchdog is kicked per bytecode, so it is not hit at ≈1 s/op.
  **Suspected defect (D2, RAM).** `doFinalRsaCipher` allocates a new `heap.addArray(modulusLength)` per call that lives until object deletion. Mbed TLS bignum temporaries and growth of the `arrays` handle vector add to it. The Pico SDK `PICO_MALLOC_PANIC=1` (default) turns a failed malloc into `panic("Out of memory")` (earlier precedent: 8E run, TODO RESOURCE-01). Malloc arena is ≈188 KB (`end` 0x20051e74 → `__StackLimit` 0x20080000 in `build/arm/pico2_choupi.elf.map`). Not yet established whether the panic is out-of-memory or a consequence of the host abort or reset during D1.
  **Fast reproduction (no 80-minute campaign).** Use the client's resume list `artifacts/results/windows-maxcrypto-20261115-25216292-dirty/pico2_maxcrypto_20261115_25216292-dirty____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED__already_measured.list`. Its last entry is RSA-1984 NOPAD `Cipher_init()`, so the next measured operation is RSA-1984 PKCS1 `Cipher_doFinal()`.
  - Copy it next to the client and rename the prefix to the card name you will type at "Specify type of your card". Answer `y` to "use it".
  - Keep a pristine copy; the client updates the list.
  - The failure is reached in ≈2 min (SELECT, `B031` keygen ≤45 s, 5 baselines, ×50).
  - Debug only: partial CSV, not campaign evidence.
  - [ ] **T1 Offline heap measurement (D2).** Run the PC/VPCD build with the unchanged 1.8.4/jc305 CAP and the pinned 1.8.5 client (`--client-jar`, SHA-256 `b6a753be…`), using the resume list or `tools/test-jcalgtest-apdu-replay-vpcd` with the exact APDU sequence from the archived log. Record peak C-heap bytes and live JC array/handle counts before, during and after the ×50 APDU, and also for RSA-1536 ×50 as the passing control. Decide whether the ×50 peak can exceed the ≈188 KB Pico arena. Record per AGENTS "Long simulator runs" rules (target scenario, why the list beats a full run).
  - [ ] **T2 Physical reproduction on Linux.** Use a normal current image, empty-card UF2 then GPPro install of the unchanged CAP, the pinned client, the resume list and timestamped serial for the whole run. Record the chip ID, image hash and JAR hash. Decide between:
    - card completes (`CCID_RESPONSE apdu=B043000016 len=3 sw=9000` after ≈45 s) and the host gives up (D1 only);
    - `*** PANIC ***` / reboot banner (D2 or a reset-handling bug);
    - success on libccid, which shows host-dependent tolerance.
    Note `USB_PROGRESS extensions=` for the APDU (expected absent/0 before the fix).
  - [ ] **T3 Fix D1.** Make long native work cooperate with CCID and the watchdog without per-bytecode cost.
    - (a) Make cooperative progress time-based: also call it after any native API return when ≥≈500 ms passed since the last call.
    - (b) Make it reach inside single long modexps. Options: an Mbed TLS-compatible way to get callbacks during `exp_mod`, splitting the exponentiation in native glue using vetted Mbed TLS primitives (INV-04: no hand-written crypto beyond glue), or an equivalent bounded service point.
    - Core 1 offload or heartbeat is **not allowed without a user-approved design** (INV-05).
    - Add unit coverage in `tests/unit/ccid_progress_flow_test.cpp` (or a new test). Show `extensions>0` and a completed ×50 APDU on hardware.
  - [ ] **T4 Fix D2 if T1/T2 confirm it (and per INV-16 regardless).** Stop allocating a JC heap array per RSA `doFinal`: write into a reused native or session buffer, then copy to `output` after length checks. Check that the same pattern is absent from the RSA signature, OAEP and other per-call native paths. Determine whether the array is persistent; if so, it violates INV-16 today. Evaluate turning allocation failure in native crypto into a controlled CryptoException or `6F00` instead of an SDK panic. Do not simply disable `PICO_MALLOC_PANIC` without an audited failure path.
  - [ ] **T5 Related long-op audit.** `gp --uninstall` of JCAlgTest after the failure took exactly 15 s 11 ms and then returned `0x1f`, which matches the 15 s watchdog. Check DELETE and other flash-heavy card-manager paths for watchdog and time-extension cooperation. One observation only; reproduce before fixing.
  - [ ] **T6 Acceptance.**
    - Linux physical, then Windows external (request `WIN-RSA1984-X50-SERIAL-20261115`): resume-list reproduction passes RSA-1984 and RSA-2048 private ×50 Cipher and Signature operations with the reader retained.
    - A timestamped serial log shows extensions, no panic and no reboot.
    - Then a full option-2 run (MEM-01 T8) passes this section.
    - Normal `max_crypto` image only (INV-24); record image, JAR and CAP hashes and the chip ID.

### HOST-02

- [ ] **P1: finish no-console Windows startup acceptance.** Normal firmware has zero CDC/DTR wait and no extra 500 ms sleep; diagnostic waits are explicit, bounded and duration-named. Both architecture normal/30000 ms diagnostic/normal rebuilds pass; DTR capture, no-DTR deadline and physical runtime/read-only GP checks pass.
- Remaining: execute `tools/measure-windows-reconnect.ps1` for at least three cold reconnects each on current and original images, with all COM/serial clients closed. Record PnP arrival, reader-list and ISD SELECT latency, exact image hashes, reader method, OS/driver state and residual host overhead. Script execution remains unverified on Windows; user-reported near-instant startup has no measured bound.
- Acceptance: retain normal/diagnostic build-reset checks, finite runtime PASS and PC/SC/APDU checks; monitor-assisted cycles cannot close the Windows no-console timing gate. See [workflow](docs/usb-startup-diagnostics.md); coordinate HOST-01/AC-01.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#host-02).

### AC-01

- [ ] **Verify Windows PCSC/GPPro and both packaged profile workflows.**
  Status: a user-provided Windows 10 run with GlobalPlatformPro 20.01.23 sees
  both readers (`Pico2 JCVM CCID 0/1`), connects at T=1 with ATR `3B800181`,
  selects the ISD, and reads CPLC and Card Data successfully (`9000`). Optional
  IIN/CIN/Card Capabilities/Key Info Template return `6A88`. `certutil -scinfo`
  separately reads the ATR and reports the card available, then fails at
  `SCardGetCardTypeProviderName` with `ERROR_FILE_NOT_FOUND`; this is missing
  Windows ATR card-type/provider discovery, not failed PC/SC APDU exchange.
  The old GPPro renders CPLC date `6304` as 2016-10-30; the repository-pinned
  newer GPPro resolves it as 2026-10-31. This is a decoder date-epoch
  difference. The supplied report establishes basic Windows CCID/GPPro
  communication only; packaged symmetric and combined-profile workflows and
  their complete JCAlgTest comparison remain unverified.
  Remaining: run [combined-profile guide](docs/windows-asymm-symm-test.md) and
  [symmetric-only guide](docs/symm-crypto-windows-test.md) on the actual Windows
  host. Check reader/driver behavior, explicit unchanged CAP install, complete
  profile reports, expected-support comparison and cleanup.
  Acceptance: archive per-profile CSVs/logs and image hashes; retain the upstream
  RSA/RSA-CRT-3072 labeling mismatch. Packaging/building alone is not a Windows pass.
  No automatic machine-configuration changes are authorized by this checklist.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#ac-01).

### AC-02

- [ ] **Complete the plan's three consecutive clean-flash integration passes.**
  Status: individual Linux AC2/3/4 and historical ordinary-power-cycle AC5 checks
  have evidence. A complete set of three consecutive integration passes has not
  been established; newer persistence/reclamation gates are separately open.
  Remaining: for each pass build ARM, verify the intended flashed
  image and initial card state, explicitly install the CAP, complete the support
  report/comparison, remove USB power, and
  repeat post-boot applet execution/report. Preserve unrelated installed artifacts
  and follow the authorized safe preparation procedure; never erase flash/OTP.
  Acceptance: three separately recorded complete runs meeting `plan.md` AC1–AC5,
  with platform-specific exclusions/known CAP defects explicit. Include instance
  fields/registry checks from PERSIST-02; Windows AC1 is owned by AC-01.
  Build/flash/monitor, card reset, and a partial CSV alone do not close a pass.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#ac-02).

### PROFILE-02

- [ ] **Add extended APDUs only if the selected profile requires them.**
  Status: extended frames are rejected with `6700`; APDU API state checks exist.
  General T=1 WTX and logical channels remain excluded. Native-operation CCID
  time-extension support exists and must not be confused with T=1 WTX.
  Acceptance if selected: explicit protocol/API requirements and focused tests,
  then build/physical validation. Otherwise retain documented exclusions; do not
  broaden scope merely to remove expected unsupported rows. See PROFILE-01.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#profile-02).

### HARD-01

- [ ] **Complete deferred robustness and private-operation hardening (plan WP9).**
  The [2026-10-23 malformed-CAP review](artifacts/reviews/malformed-cap-review-20261023.md)
  is historical source-inspection evidence for this task; its findings require
  current-source verification before treating them as open defects.
  Status: framed/count/type/Class/Directory CAP validation and APDU fuzzing exist.
  Latest CAP sanitizer run passes 4,208 checks / 48 supplied CAPs / 100,000 random
  inputs; short APDU fuzzing covered 250,000 inputs. Directory validation respects
  omitted GP LOAD components. Physical final-image parser validation is pending.
  Remaining: broader nested-table/linkage validation and fuzzing, interrupted
  persistent-write/reclaimer recovery, and private RSA input blinding / full CRT
  fault verification. Complete verification needs a matching public exponent;
  residue checks alone are insufficient. Preserve existing scratch zeroization.
  Field-kind hardening remains incomplete: runtime instance tags reject a
  conflicting access after the first store, but the first store is not checked
  against a CAP-declared field kind. Descriptor contains field type records,
  but it is optional and standard GPPro LOAD omits it. Class and StaticField
  components expose layout/reference counts, not each primitive field kind.
  Do not infer first-store types from bytecode or weaken the dynamic-CAP method
  bound policy. Design a verifiable metadata/trust source before enforcing
  declared kinds for Descriptor-free loads; use Descriptor-backed fixtures to
  cover the available metadata path in the meantime.
  Acceptance: named threat/failure model and bounded negative tests, explicit
  imported/generated-key limits, unchanged valid CAP behavior and recovery
  evidence. Coordinate PERSIST-01/03 rather than duplicate their work.
  CAP 2.3/VMMT support and certification-grade security remain separately scoped;
  do not enable unsupported formats or claim certification from this checklist.

[Future language backends](docs/FUTURE_JCVM_LANGUAGE_BACKENDS.md), versioned
backend interfaces and a Rust rewrite remain future analysis requiring a separate
user request. Architecture maintenance does not authorize their implementation.
Atomic PC image/sidecar replacement and host power-loss durability are optional
future hardening; the weaker checkpoint contract is documented in
[FILESYSTEM_ABI.md](docs/FILESYSTEM_ABI.md), not an unfinished ARCH-02 deliverable.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#hard-01).

## Open review acceptance — 2026-11-06

Source corrections exist for R01/02/03/05/06/07/08/60; check the [review status table](docs/review_20261106.md) before implementing. These remain acceptance tasks, not instructions to duplicate fixes.

- [ ] **RV-20261106-01 — Contain opcode-fetch exceptions inside interpreter recovery.** Owner: HARD-01 / RV-20261103-28. Source fix and PC VPCD recovery pass: fall-through, truncated operand, and zero-length Descriptor method-body mutations each return a controlled status; manager reselection and subsequent GP GET DATA succeed (`9000`). Keep open for physical acceptance and remaining caller-state coverage. [R01](docs/review_20261106.md#r01--p1-opcode-fetch-failure-escapes-the-interpreters-exception-boundary)
- [ ] **RV-20261106-02 — Correct nested frame unwinding direction.** Owner: CTX-01 / HARD-01. Preserve caller PCs/locals/roots when trimming nonzero-depth stacks; cover Java/ISO errors, allocation failure and timeout through multiple nested callbacks. Historical `pop_back()` versus `emplace_front()` defect; current source now uses front removal. [R02](docs/review_20261106.md#r02--p1-nested-failure-cleanup-removes-callers-instead-of-callees)
- [ ] **RV-20261106-03 — Check local/stack bounds before metadata and pointer access.** Owner: HARD-01. Check complete two-word ranges before mutation; sanitize high local indices at nonzero frame bases and stack-capacity boundaries. Source-confirmed ordering defect; sanitizer reproducer pending. [R03](docs/review_20261106.md#r03--p1-local-metadata-is-indexed-before-the-local-variable-bounds-check)
- [ ] **RV-20261106-04 — Give PIN validation/retry state its required native lifecycle.** Owner: PROFILE-01 / CTX-03 / PERSIST-02. Check wrong-PIN + abort, successful PIN + reset/cold restore, unblock + abort and OwnerPINx variants against the matching generated ROM. Retry decrements must survive applet abort; validation must clear on reset. Follow repository native C++ API policy. [R04](docs/review_20261106.md#r04--p1-ownerpin-validation-and-retry-state-use-ordinary-persistent-fields)
- [ ] **RV-20261106-05 — Keep persistent field caches unchanged when a write fails.** Owner: PERSIST-03. Fault-inject capacity/allocation/serialization/backend errors for scalar/int/reference stores; catch and read immediately, commit, abort, collect and restore. Prove cache/storage agreement and preserved old roots, including failure before snapshot insertion. [R05](docs/review_20261106.md#r05--p1-failed-persistent-setters-publish-cache-changes-before-storage-succeeds)
- [ ] **RV-20261106-06 — Validate field receiver assignability and access.** Owner: HARD-01 / PROFILE-01. Cover all getter/setter encodings, unrelated same-layout classes, imported/inherited fields and ROM API object receivers. Declared field-kind and owner checks alone are insufficient. [R06](docs/review_20261106.md#r06--p1-instance-field-resolution-does-not-verify-the-receivers-class)
- [ ] **RV-20261106-07 — Bound every superclass traversal and reject cycles.** Owner: HARD-01. Audit allocation sizing, class casts, Object lookup and field flattening, including checked size accumulation. Test self/two-class cycles and deep chains with bounded same-process recovery on PC and hardware. Hostile-CAP reachability/physical timing remain unmeasured. [R07](docs/review_20261106.md#r07--p1-several-superclass-walks-can-loop-inside-one-native-operation)
- [ ] **RV-20261106-08 — Make all Cipher operational bytes consistent with native session lifetime.** Owner: PROFILE-01 / PERSIST-02. Audit IV, saved IV, buffered input, CTR stream and grown buffers; test update/abort/final, reset and fresh restore and inspect persistent records for retained operational data. Preserve existing transient key-workspace safeguards. [R08](docs/review_20261106.md#r08--p1-cipher-operational-byte-arrays-still-have-persistenttransactional-lifetime)
  Progress: Operational Cipher arrays now use reset-cleared transient storage. CBC/CTR abort, buffer-growth, fresh-process restore, and five raw-image sentinel cases pass in VPCD. Remaining: inspect older persistent Cipher instances from pre-transient firmware. A grown persistent array can have a separate body tag; rebinding its field to a transient array may leave that body record orphaned. This is a source-level risk pending a legacy-seeded reproducer and full tag inventory; do not claim upgrade cleanup yet. Physical acceptance remains pending PC/SC reader publication.
- [ ] **RV-20261106-20 — Remove ARM cycle/smoke-image ambiguity.** Owner: HOST-01 / AC-02. Align wrapper target names and instructions; require exact full-runtime image/profile/hash and runtime marker in JCVM acceptance. Current `cycle arm` selects the smoke image; `cycle choupi` selects the full VM. [R20](docs/review_20261106.md#r20--p1-the-documented-arm-cycle-flashes-the-smoke-firmware)

- [ ] **RV-20261106-16 — Measure repeated branch-boundary rescans.** Prerequisite of CAPIN-08 (its branch-result cache must wait for this measurement). Owner: HARD-01 / API-03. Compare equivalent loops at increasing method offsets; record device latency and RAM cost before introducing a validated instruction-start cache. Keep boundary checks and package invalidation intact. [R16](docs/review_20261106.md#r16--p2-branch-validation-rescans-from-method-start-on-every-taken-branch)
- [ ] **RV-20261106-17 — Measure handle lookup and worst-case GC scaling.** Owner: PERSIST-01 / API-01 / EMU-01. Include forward/reverse chains, wide graphs and sparse slot tables; record heap size, GC duration, watchdog margin and p95/p99 APDU latency. Current list lookup and repeated mark passes have linear/quadratic scaling risks. [R17](docs/review_20261106.md#r17--p2-stable-handle-lookup-and-gc-have-adverse-scaling)
- [ ] **RV-20261106-18 — Disclose Signature cumulative limits and peak memory.** Owner: PROFILE-01 / API-01. Document the current 32,767-byte buffering ceiling; test multipart boundaries and failed growth. Evaluate incremental native hash/MAC state with correct reset/abort semantics as separate implementation work. Preserve the already disclosed MessageDigest limit. [R18](docs/review_20261106.md#r18--p2-signature-update-is-whole-message-buffering-with-a-32767-byte-ceiling)
- [ ] **RV-20261106-19 — Bound service gaps inside long native loops.** Owner: HOST-01 / HARD-01 / API-03. Measure maximum-size persistent Util copy/fill, crypto workspace copies and cleanup under capacity pressure; verify CCID progress, immutable deadline and watchdog margin before choosing callback cadence. [R19](docs/review_20261106.md#r19--p2-lengthy-native-copyfill-loops-lack-cooperative-progressdeadline-checks)

- [ ] **RV-20261106-21 — Synchronize profile/trust statements with current code and limits.** Owner: PROFILE-01 / ARCH-08. Replace blanket multi-applet exclusions with a capability/remaining-gap/evidence matrix; disclose Signature limits and SCP03 subset, state RISC-V as out of scope and separate helper/emulator/hardware evidence. Review R01–R20 remain open until validated. [R21](docs/review_20261106.md#r21--p3-profiletrust-documentation-has-fallen-behind-implemented-behavior)

- [x] **RV-20261106-32 — Propagate emulator application failures to the parent process exit.** P1; owner: EMU-01 / HOST-01. Completed 2026-11-12: startup, transport, installation and save status now pass through C++ and Rust context/process exit handling. `./tools/test-emulator-exit-status` passes seven cases covering absent restored instance, refused connection, malformed transport, failed install, both image/sidecar save failures, and clean shutdown. [R32](docs/review_20261106.md#r32--p1-emulator-failures-are-discarded-across-the-startup-and-process-boundary)
- [x] **RV-20261106-33 — Protect shared PC and firmware generation from concurrent writers.** P2; owner: HOST-01 / PROFILE-01. Completed 2026-11-12: `tools/build` and standalone `tools/build-pc` now use one shared lock with safe nested descriptor reuse; profile, ROM and JNI hashes are checked into architecture provenance. Three concurrent wrapper builds (ARM `base`, standalone PC `max_crypto`, RISC-V `symm_crypto`) passed with disjoint lock-owner intervals and matching generation provenance. [R33](docs/review_20261106.md#r33--p2-standalone-pc-builds-bypass-the-lock-protecting-shared-generated-artifacts)
- [ ] **RV-20261106-34 — Keep explicit flash device identity mandatory through discovery and re-enumeration.** P1; owner: HOST-01 / AC-02. Offline fix is in `tools/lib/flash_usb_target.sh`: absent/duplicate serials fail closed; the selected runtime serial or chip ID is checked after re-enumeration; BOOTSEL transitions require a matching ROM chip ID; the flash log prints the selected path, bus/address, serial and chip ID. Nine synthetic sysfs cases pass, including absent A/sole B, duplicate identities, BOOTSEL ID mismatch, and runtime/BOOTSEL replacement at a stable path. Still open: physical flash verification on an identified Pico with the selected identity logged. [R34](docs/review_20261106.md#r34--p1-an-explicit-flash-serial-can-fall-back-to-a-different-board)
- [ ] **RV-20261106-35 — Pin one reader through every power-up acceptance phase.** P2; owner: HOST-01 / PERSIST-03 / AC-02. Forward the exact reader to the existing APDU probe, record actual selected identity and reject ambiguous fallback. Test opposing registry/APDU outcomes across two readers; retain the honest power-cycle-not-verified marker. [R35](docs/review_20261106.md#r35--p2-the-power-up-acceptance-script-can-combine-evidence-from-different-readers)
- [ ] **RV-20261106-36 — Align physical CCID and VPCD reset/power callback semantics.** P2; owner: CTX-03 / HOST-01 / PERSIST-03. Prevent reset/power-off from invoking applet deselect code. Verify persistent callback counter, reset/deselect transients, transactions and native session cleanup through logical off/on and reader reset on each backend; test supply loss separately. [R36](docs/review_20261106.md#r36--p2-physical-ccid-power-off-executes-an-applet-callback-unlike-vpcd-power-off)
  Offline progress (2026-11-07): physical CCID POWER_OFF now uses the reset cleanup path, which clears selection, applet transactions, both transient classes, shareable session state, secure-channel state and pending transfers without running Java deselect. VPCD unit tests pass 29/29; the focused JCSystem case passes power-off/on and reader-reset callback-count plus both transient-class checks. ARM and RISC-V max_crypto builds pass. Physical CCID and supply-loss checks remain pending. A full APDU-suite run without light-install exceeded its 240 s bound; a light-install full run is invalid for capacity acceptance (case 83 expects the 32,767-byte field). See QKB/progress for scope.

Extend RV-20261106-21 to reconcile `docs/NATIVE_PROGRESS_CALLBACKS.md` with immutable execution deadlines and current timeout constants. This is part of the existing documentation task rather than an additional finding/task.

- [ ] **RV-20261106-37 — Make EC cofactor requirements algorithm-specific.** P2; owner: API-03 / PROFILE-01. Allow fully populated EC keys and ECDSA to initialize without optional K; preserve DHC's required-cofactor check and R28's validation of inconsistent supplied K. Cover generated/manual public and private keys, missing/correct/invalid K and non-DHC agreement. [R37](docs/review_20261106.md#r37--p2-ec-initialization-and-ecdsa-require-an-optional-cofactor)
- [ ] **RV-20261106-38 — Honor or reject preinitialized key-pair parameters explicitly.** P2; owner: API-03 / PROFILE-01. Preserve supported RSA exponents and EC domains rather than silently replacing them. Test default/nondefault/invalid exponents and supported/invalid/unsupported EC domains, parameter readback, pair consistency and repeated generation. A named-curve restriction may reject unsupported domains explicitly. [R38](docs/review_20261106.md#r38--p2-key-pair-generation-silently-replaces-preinitialized-parameters)
- [ ] **RV-20261106-39 — Reset CBC MAC IV to zero after finalization.** P2; owner: API-03 / PROFILE-01. Cover sign, successful/false verify, multipart input and explicit reinit on the same object after a nonzero-IV init. Compare first output with that IV and next output with an independent zero-IV reference; default-zero repeated-message tests are insufficient. [R39](docs/review_20261106.md#r39--p2-cbc-mac-finalization-preserves-the-nonzero-iv-for-the-next-message)
- [ ] **RV-20261106-40 — Restore persistent-key crypto initialization across reset.** P2; owner: CTX-03 / PROFILE-01 / PERSIST-02. Separate retained key/mode/configuration from discarded intermediates. Test retained Cipher and Signature objects without reinit after logical reset and cold restore, partial input, zero-IV reset, inactive applets and output-length metadata. Keep cleared transient keys unusable. [R40](docs/review_20261106.md#r40--p2-card-reset-discards-persistent-key-crypto-initialization-instead-of-restoring-it)

Extend RV-20261106-29's allocation fault audit to EC key-pair `curveA` while `mpiBytes()` allocates. This is an instance of the existing resource-cleanup finding, not a separate TODO.

- [ ] **RV-20261106-41 — Validate receiveBytes state and offsets despite fully buffered input.** P2; owner: PROFILE-01 / HOST-01 / HARD-01. Enforce checks through native VM dispatch. Test calls before primary receive, after outgoing, negative/oversized offsets, zero remaining input, exact Java exception reasons and recovery. Valid post-receive calls may return zero. [R41](docs/review_20261106.md#r41--p2-receivebytes-ignores-its-state-and-offset-contract)
- [ ] **RV-20261106-42 — Enforce outgoing API length limits and preserve BAD_LENGTH reasons.** P2; owner: PROFILE-01 / HOST-01. Separate applet capability and chaining limits from the 1,022-byte buffer capacity; test -1/0/252/253/256/257/1022/1023, chaining modes and invalid states. Catch/read reasons rather than asserting only APDU failure; explicitly disclose any supported long-response extension. [R42](docs/review_20261106.md#r42--p2-outgoing-length-validation-follows-buffer-capacity-rather-than-the-declared-apdu-contract)
- [ ] **RV-20261106-43 — Contain allocation failure during package-stack insertion at method entry.** Implemented via CAPIN-03/04 (package list removed; `changePackageID` no longer allocates); **acceptance only** (failure injection and following-APDU check). P1; owner: HARD-01 / CTX-01 / EMU-01. Remove allocating-noexcept termination and make frame/package entry coherent on failure. Inject failure after frame creation, verify retained callers/packages and a following APDU; audit analogous noexcept allocation paths. Measure per-call allocation and method-heavy latency before choosing storage changes. [R43](docs/review_20261106.md#r43--p1-method-entry-package-bookkeeping-can-terminate-on-allocation-failure)

- [ ] **RV-20261106-44 — Preserve recovery state when rollback itself fails.** P1; owner: PERSIST-03 / STORE-01 / HARD-01. Software fix is implemented: rollback retains its journal and active guard after erase/write/sync failure, blocks persistent reads/writes and new commits, and retries recovery before applet APDUs (returns `6581` while recovery still fails). Regression injections cover restore erase/write failure, rollback sync failure, commit plus rollback sync failure, no partial durable publication, successful retry, and simulated cold reset restoring the prior committed state. Remaining acceptance: validate recovery on hardware. [R44](docs/review_20261106.md#r44--p1-failed-rollback-discards-the-recovery-journal-and-reopens-ordinary-writes)
- [ ] **RV-20261106-45 — Make snapshot publication and capacity charging exception-safe.** P2; owner: PERSIST-02/03 / HARD-01. Inject vector growth failure after record read/charge, verify unchanged journal/capacity, then retry the same tag and commit/abort. Cover old/new records and prior successful snapshots; distinguish this from instance-cache rollback. [R45](docs/review_20261106.md#r45--p2-snapshot-insertion-failure-leaves-commit-capacity-charged-without-a-snapshot)
- [ ] **RV-20261106-46 — Avoid owning-array reconstruction for each persistent element check.** P2; owner: API-01 / PERSIST-01 / HARD-01. Preserve header/type/size/provenance validation with bounded metadata access. Measure increasing primitive/reference scans, allocator calls, temporary bytes and latency; reference validation currently initializes an entire owner vector per element. Cover low-memory reads and malformed records with secure checks enabled. [R46](docs/review_20261106.md#r46--p2-persistent-array-element-validation-allocates-a-temporary-array-on-every-access)
- [ ] **RV-20261106-47 — Invalidate transaction-created object references on abort or force safe session recovery.** P2; owner: PERSIST-02/03 / CTX-01 / HARD-01. Track allocation lifetime through native runtime state. Test aliases in locals/transient arrays, abort, null comparison and dereference, preexisting objects, commit and nested/SIO returns; prove stale handles cannot alias later allocations. Ordinary record rollback alone does not close this. [R47](docs/review_20261106.md#r47--p2-transaction-abort-leaves-references-to-newly-allocated-objects-usable)

- [ ] **RV-20261106-48 — Implement native primitive array packing and unpacking contracts.** P2; owner: PROFILE-01 / API-03. Cover all byte/short/int cross-type directions, big-endian bit preservation, converted destination range/end offset, invalid multiples, zero length and same-array overlap. Reject booleans/references with the documented utility error; same-type copying is insufficient. [R48](docs/review_20261106.md#r48--p2-arraycopyrepack-rejects-the-cross-type-conversions-it-is-intended-to-perform)
- [ ] **RV-20261106-49 — Give atomic ArrayLogic copy/fill a native transaction boundary.** P2; owner: PERSIST-02/03 / PROFILE-01 / HARD-01. Enforce capacity and all-or-nothing persistent updates outside/inside caller transactions without committing the caller. Inject failures at every destination write and check unchanged destination on rejection, catch/recovery and fresh restore; validate physical interruption separately. [R49](docs/review_20261106.md#r49--p2-atomic-generic-copy-and-fill-have-no-standalone-atomic-transaction-boundary)
- [ ] **RV-20261106-50 — Keep generic NonAtomic writes outside applet rollback.** P2; owner: PERSIST-02/03 / PROFILE-01. Use distinct native storage behavior for byte/short/int and reconcile preexisting snapshots. Cover both orderings of atomic/nonatomic writes to one array, capacity exhaustion, overlap, abort and restore; do not route through ordinary journaled setters. [R50](docs/review_20261106.md#r50--p2-generic-nonatomic-copy-and-fill-participate-in-applet-rollback)
- [ ] **RV-20261106-51 — Dispatch native utility errors as catchable Java UtilException with reasons.** P2; owner: PROFILE-01 / HARD-01 / CTX-01. Provide ILLEGAL_VALUE versus TYPE_MISMATCHED through native exception dispatch. Test exact/superclass catches for invalid types and mismatched fills, assert reasons and complete valid work in the same and next APDU. [R51](docs/review_20261106.md#r51--p2-native-utility-errors-cannot-be-caught-as-the-declared-utilexception)

Extend R19/R46 measurements to the generic copy helper's whole-input `uint32_t` vector, including byte copies. Scratch overhead is source-observed; peak memory and throughput remain unmeasured.

Extend RV-20261106-11 to repeated distinct-AID install/lookup/delete and global AID cache lifetime/lookup costs. Reclaim only unreferenced entries while preserving live runtime AID references and identity semantics; no memory-exhaustion rate was measured.

- [ ] **RV-20261106-55 — Validate loaded-library dependency readiness before applet execution.** Implemented via CAPIN-01 (bounded eight-slot dependency-closure worklist); **acceptance only** (remaining A→B→C cases). P2; owner: GP-01 / PROFILE-01 / HARD-01. Cover A→B→C with missing/wrong-version/present C and C used only inside B's method body. Validate reachable dependencies with bounded traversal or an earlier library linking boundary, preserve supported load order, define cycle handling and demonstrate correction permits INSTALL. Direct-import and deletion guards are insufficient evidence. [R55](docs/review_20261106.md#r55--p2-install-preflight-does-not-validate-the-dependency-closure-of-loaded-libraries)
- [ ] **RV-20261106-56 — Avoid repeated full-store orphan sweeps for already clean free IDs.** P2; owner: STORE-01 / HOST-01 / PERSIST-03. Measure scan counts/manager latency for 0/1/7 free slots and sparse/dense clean/orphaned stores. Batch or cache only with validated recovery invariants; cover interrupted publication and package-ID reuse. Traversal counts are source-inferred; no flash erase count or speedup is claimed. [R56](docs/review_20261106.md#r56--p2-orphan-cleanup-repeats-full-storage-walks-for-ordinary-free-package-slots)

Extend RV-20261106-29's allocation-failure resource audit to RSA CRT modulus/exponentiation helpers in `native_rsa_math.hpp`, including vector failures while raw MPIs are live. This is the existing exception-cleanup issue, not another task.

- [ ] **RV-20261106-57 — Define and validate ECDSA with enabled transient EC private keys.** P2; owner: API-03 / PROFILE-01 / CTX-03. Native initialization currently accepts only persistent type 12. Support or disclose types 30/31 explicitly; compare identical complete key material across lifetimes/digests, independent verification, clear-event rejection and reinit. Validate downstream component use; keep optional-K handling separate. [R57](docs/review_20261106.md#r57--p2-ecdsa-initialization-rejects-enabled-transient-ec-private-key-types)
- [ ] **RV-20261106-58 — Implement or disclose the existing-key KeyPair constructor overload.** P2; owner: API-03 / PROFILE-01. Add supported behavior through native binding/VM code, preserving supplied references. Cover initialized/uninitialized compatible RSA/EC pairs, getter identity, mismatched sizes/algorithms, generation and transient-private variants. Current native generation/factory tests cover only algorithm/length construction. [R58](docs/review_20261106.md#r58--p2-the-keypair-constructor-taking-existing-keys-remains-an-unbound-stub)

Extend RV-20261106-28's EC matrix to allocatable versus operational key sizes/domains, including enabled 320/512-bit key storage with no native named group. Disclose storage-only cases rather than inferring full algorithm support from key construction.

The report and additional task list now contain 59 matching entries. The coordinated collector now traces retained package heaps from non-foreign roots, follows foreign array/instance edges only from marked source objects, and sweeps after a fixed point. Focused tests pass for two- and three-heap cycles, rooted chains/diamonds, collection order, eight repeated explicit-heap cycles with stable slot count, fail-closed ambiguous package ownership, and production `collectRetainedHeaps()` handling peer cached-static/shareable-proxy roots plus eight repeated four-context diamond cycles with a stable four-slot count. Production entrypoint tests also cover foreign volatile instance cycles, fail-closed duplicate package contexts, and a reachable foreign edge whose target context is omitted from the collection request; both heaps remain intact on the incomplete trace. Firmware builds pass with object deletion enabled for both architectures. Keep R59 open for physical reset/deletion validation, broader runtime-context edge cases, and bounded actual RAM measurement.

- [ ] **RV-20261106-59 — Prevent unreachable foreign-array cycles from becoming permanent GC roots.** P2; owner: PERSIST-01 / CTX-01 / HARD-01. Separate registry edges from reachable roots using a validated cross-heap lifetime strategy. Exercise two/three-heap cycles and diamonds with/without real roots, different collection orders, edge breaking, package deletion and handle reuse. Use production Context enumeration and measure reclaimed/live counts plus bounded repeated-work RAM growth while preserving live foreign references and owner collision isolation. [R59](docs/review_20261106.md#r59--p2-foreign-array-root-routing-can-keep-an-unreachable-cross-heap-cycle-alive)

Extend RV-20261106-17's GC measurements to incoming foreign-array registry traversal. Existing overwrite/delete helper passes do not establish collection of graphs whose only reachability is cyclic foreign edges.

- [ ] **RV-20261106-60 — Audit acceptance of the corrected positive-depth dup_x implementation.** P1; owner: HARD-01 / PROFILE-01 / CTX-01. The RMI stack-helper suite covers all M/N byte combinations, exact values, underflow/overflow recovery, reference-owner metadata, and int pairs; `./tools/test-rmi-registry` passes. The generated VPCD CAP now executes all 24 legal `dup_x` operands through applet INSTALL and passes the constructor depth check; invalid `0x17` reaches interpreter dispatch and is rejected with `vm=6F00` before applet publication. The VPCD interpreter now executes all 24 legal operands and catches all 232 invalid byte values in one APDU; a separate caught-invalid APDU and the next APDU both return `9000`. Interpreter typed cases preserve an APDU reference identity, duplicate a complete int pair, and duplicate an int pair above a short (`dup_x 0x23`). A cross-AID SIO VPCD CAP now sends a client-owned array reference through `dup_x 0x12`, returns it from the service, and verifies identity at the client; the focused probe passes. Remaining: obtain physical acceptance. The latest elevated USB refresh still sees three runtime `2e8a:0009` devices and no ROM `2e8a:000f`, so no physical flash was performed. Use integer-range checks before pointer arithmetic as in R03. [R60](docs/review_20261106.md#r60--p1-positive-depth-dup_x-duplicates-the-wrong-stack-segment)

- [ ] **RV-20261106-12 — Accept 13-byte no-data case-3 SCP03 commands.** Owner: GP-01. Test valid independent MAC vectors with/without Le, malformed lengths and failed-MAC recovery; derive minimum length from supported APDU cases. [R12](docs/review_20261106.md#r12--p2-scp03-rejects-a-valid-no-data-case-3-protected-command)

- [ ] **RV-20261106-13 — Define and enforce a total VPCD frame deadline.** Owner: EMU-01. Cover slow-drip header/payload and EINTR, not just idle/stopped input; record elapsed upper bounds and successful reconnect. Distinguish intentional inactivity timeout if retained. [R13](docs/review_20261106.md#r13--p2-vpcd-read-timeout-is-an-inactivity-timeout-not-a-frame-deadline)

- [ ] **RV-20261106-14 — Apply execution budgets to the PC interpreter.** Owner: EMU-01 / HARD-01. Use monotonic time and preserve nested deadlines; recover an infinite interpreted call in the same process, restore transactions/callers and accept a subsequent command. Killing a timed-out emulator does not close this. [R14](docs/review_20261106.md#r14--p2-the-pc-interpreter-has-no-equivalent-execution-deadline)

- [ ] **RV-20261106-15 — Finish review-harness validation after the allocation-link fix.** Owner: HARD-01 / PERSIST-01. Standalone allocation linkage is resolved by MIG-20261107-01 (nine assertions pass). Remaining: validate/repair the separate review persistence fixture’s CAP dependencies and reference-slot assumptions, then include applicable targets in routine offline checks. [R15](docs/review_20261106.md#r15--p2-allocation-regression-cannot-currently-link)

- [ ] **RV-20261106-27 — Preserve reference-array component type namespaces.** P2; owner: CTX-01 / PROFILE-01 / PERSIST-02. Retain the defining CAP or stable resolved component identity through allocation/persistence; exercise imported-library allocation followed by applet stores/casts using deliberately colliding CP indexes, then restore. [R27](docs/review_20261106.md#r27--p2-reference-array-component-types-lose-their-defining-cap-namespace)

- [ ] **RV-20261106-30 — Prevent exception diagnostics from reading arbitrary field kinds as short.** P2; owner: HARD-01 / PROFILE-01 / CTX-01. Exercise fresh-boot hardware throws of custom exceptions with byte/int/reference/short first fields, exact/superclass catches and a following successful APDU; trace-budget state must not alter behavior. [R30](docs/review_20261106.md#r30--p2-hardware-exception-diagnostics-assume-every-exceptions-first-field-is-a-short)

- [x] **RV-20261106-31 — Preserve new instance fields when first persisted after an unrelated rollback.** P1; owner: PERSIST-02/03. `getFields()` now returns authoritative nonpersistent fields before consulting the persistent rollback generation. A production-handler regression first reproduced the loss (2 failures), then passed with a real rollback, pre-rollback and post-rollback instances, scalar/reference fields, immediate and fresh-heap value/identity checks, and failed-promotion retry. ARM and RISC-V `max_crypto` wrappers pass. Physical validation remains under the PERSIST-02/03 carry-forward gate because the VM did not expose BOOTSEL. [R31](docs/review_20261106.md#r31--p1-an-earlier-rollback-can-erase-a-new-objects-fields-on-its-first-persistence)

## Physical carry-forward from scoped completed fixes

For the current external-test image and its request ID, see the "Current
images" table at the top of this file.

- [ ] **API-03 / PERSIST-02: retain outstanding physical gates from RV-20261106-09/10/22/23/24/26/52/53/54.** Software checks are completed and archived; confirm current-image Cipher padding/reuse, object identity/rollback/retry, boolean restore, transient allocation/activity and resource reasons as applicable. Do not reopen the implemented fixes as missing features. Detailed original scope and evidence remain in the backup.

## Remaining performance work

PERF-20261110-01–04 are completed with measurements in the backup and progress;
metadata caching reduced filesystem reads but showed no physical latency gain.
Unmeasured costs below remain hypotheses. Keep each optimization a separately
validated/measured step. PERF-05 overlaps RV-20261106-17; PERF-07 overlaps PERSIST-01/03 (ARM measurements only).

- [ ] **PERF-20261110-05 — Constant-time heap handle lookup.** P2; owner:
  HARD-01. `Heap::SharedStorage` uses `List` (`std::list`) whose `at()` walks
  from the head on every `getArray`/`getInstance`; `addArray`/`addInstance`
  scan for a free slot. Replace with an indexed container (e.g. `std::vector`
  plus a free-slot list) while preserving handle stability, the 0x3fff limit,
  the shared-storage facade semantics, the GC mark bitmaps, and foreign-reference
  registration. Avoid needless `shared_ptr` copies on hot bytecode paths where
  lifetime allows. Acceptance: heap/GC/RMI unit tests, VPCD suites, and a measured
  array-access microbenchmark at 16, 256, and 4,096 live handles on PC and Pico.
- [ ] **PERF-20261110-06 — Reduce transaction snapshot lookup cost.** P3;
  owner: PERSIST-01. `snapshot_persistent_tag` and
  `preserve_non_atomic_array_byte_in_snapshot` scan all snapshots linearly
  on every persistent write, which is O(n²) per large transaction. Index snapshots by
  tag (hash or sorted) without changing rollback order or capacity
  accounting. Acceptance: transaction probe and commit-capacity tests pass;
  report timing for a transaction touching 256 records.
- [ ] **PERF-20261110-07 — Make Pico `fs_sync` incremental.** P2; owner:
  PERSIST-01 / HOST-01. `persist_overlay()` (`src/pico_filesystem.cpp`)
  erases and reprograms the whole FS snapshot (bitmap + index + up to 96 KiB
  payload) after every APDU that dirtied persistent state, so a one-byte
  change costs about 1 s of flash time, assumed rather than measured. Each
  slot is erased once every two such APDUs, which is an endurance risk. Measure
  first (timing per `fs_sync`, bytes erased/programmed). Then design an append-only
  journal or sector-granular dirty write-back that keeps power-loss atomicity
  across the two slots. `append_overlay` also memmoves up to 96 KiB and loops
  over 8,192 entries on each record resize; include it in the measurement.
  Acceptance: power-loss/rollback tests (`RP2350_FS_OVERLAY_TEST`) pass;
  measured per-APDU sync time and erase count before/after on physical Pico 2
  (ARM only; RISC-V is out of scope); full CHOUPI cycle `TEST:SUMMARY` passes.
- [ ] **PERF-20261110-08 — Reduce PC FS write amplification.** P3; owner:
  EMU-01. `FileSystem::edit_at` (`choupi/os/src/fs/mod.rs`) appends a full copy
  of the record for every edit, and `Sector::lock` scans every outstanding
  lock linearly (16-bucket set; FS file set uses 32 buckets). After 02–03
  land, re-measure; only optimize if compaction or lookups still dominate VPCD
  APDU time. Acceptance: native FS bench (patch in the artifact folder above)
  and `fs::` tests pass with before/after numbers.
