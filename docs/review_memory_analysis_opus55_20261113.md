# Memory exhaustion review — RESOURCE-01 / MEM-01

Date: 2026-11-13. Reviewer: Claude Opus 5.5 (`claude-opus-5-5`).
Scope: static code review plus existing serial/client logs. **No code was
changed, built, flashed or executed for this review.** Status labels below:
*confirmed (code)* = defect follows directly from reading current source;
*confirmed (log)* = defect is visible in archived serial output;
*inferred* = consistent with evidence but not reproduced.

Backlog item: [MEM-01 in TODO.md](../TODO.md#mem-01). Related history:
[resource-01-implementation-plan.md](resource-01-implementation-plan.md),
[.agent/QKB.md](../.agent/QKB.md) RESOURCE-01 sections.

## 1. Summary

There is no single memory bug. Two historical *corruption* bugs (already
fixed) damaged persistent state, and several current *design* defects turn
any single inconsistent record into permanent, non-recoverable exhaustion of
either the flash overlay (`F205` / `NO_RESOURCE`) or RAM (`*** PANIC *** Out
of memory`).

| ID | Defect | Status | Effect |
|----|--------|--------|--------|
| M1 | Compaction rehash lost/duplicated index entries | fixed `92c91943` (11-12 22:49 UTC) | corrupt overlay index |
| M2 | Reclaimer erased reachable alias targets | partially guarded `36fcfe69` | live data loss |
| M3 | One unresolvable candidate stops reclamation forever | confirmed (code + D92 log) | flash exhaustion |
| M4 | One heap-trace edge failure disables RAM GC forever | confirmed (code + 8E log) | RAM OOM panic |
| M5 | Alias guard protects only the body, not its subtree | confirmed (code) | silent live-field loss |
| M6 | Erase/append offset fix-up assumes unique record offsets | confirmed (code) | index desync → compaction rejects |
| M7 | Snapshot load does not validate index consistency | confirmed (code) | corruption survives reboots/firmware-only flashes |
| M8 | Package-static reclaimer lacks the alias guard | confirmed (code) | live data loss if an edge is missed |
| M9 | `append_overlay` mutates before `parent_missing` reject | confirmed (code), legacy-only | index desync |
| M10 | Legacy-format gaps (`FF FD` bodies, parent-encoded children) | confirmed (code), legacy-only | M3 trigger |
| M11 | Whole-store snapshot publish after every dirty APDU (0.4–1.5 s) | confirmed (log) | CCID stalls; not exhaustion |

## 2. Evidence that framed the review

### 2.1 D92 option-2, 1.8.5 client, firmware `f0ad042b…` (source `350cbcfc`)

`artifacts/results/jcalgtest-1.8.5-ccid-recovery-20261113/D92E743D5F640C0F/serial.log`

- Lines 27–3527: 126 consecutive `OBJECT_DELETION:` batches, each
  `applet_result=1 erased_applet=8 pending=1` — the backlog never drained.
- Line 3554 (xfr=143): `applet_result=6 package_result=6 erased_applet=3` —
  three erasures, then `VisitResult::ReadFailure` inside the reclaimer.
- From line 3580: every later request logs
  `OBJECT_DELETION:SKIP … applet_roots_complete=0` (1,133 skips) plus
  `CHOUPI:HEAP_GC_SKIP … applet_result=6`. Nothing is reclaimed again.
- Line 14062 onward: `FS_COMPACT_REJECT status=131 reason=tag_reconstruction
  files=2468 bytes=98273` (295×), each followed by `FS_WRITE_FAIL` and
  `F205` (`PERSIST_WRITE_FAIL` 295×).
- The log starts at `rx_events=313 xfr=16`, and the first batch already has
  a backlog: the store was **not fresh**. D92 was empty-carded on 11-12 and
  then received several firmware-only (storage-preserving) flashes, including
  pre-`92c91943` images that ran compactions with the M1 rehash bug.

### 2.2 2BED option-2 runs (same firmware family)

`artifacts/results/jcalgtest-1.8.5-compaction-fix-20261113/2BED26ED20556535/serial.log`
and `…-ccid-recovery-20261113/2BED26ED20556535/serial.log`:
793 / 461 batches with `pending=0`; 236 / 99 capped batches always drained;
zero `SKIP`, zero `F205`. These runs failed on CCID response delivery, not
memory. **On the evidence available, current code does not exhaust storage
on a clean store**; every observed exhaustion is on corrupted state or a
pre-fix image.

### 2.3 8E option-1 (pre-alias-guard)

`artifacts/results/jcalgtest-1.8.4-resource01-20261112/8E464F4E2BBC0A19/option1-all-algorithms/serial.log`:
2,245 `HEAP_GC_FS_READ_FAIL`; final summaries
`arrays=85/347 reclaimed=0 instances=21/91 reclaimed=0 edge_failures=5`;
`Out of memory` at line 12905. 262 unreachable arrays were retained solely
because the trace was incomplete.

### 2.4 Other
- `fs_sync()` per APDU: 2BED avg 0.40–0.54 s (max 1.40 s), D92 avg ~1.0 s (max 1.48 s).
- Windows `580f3f40` run predates M1 fix and CCID recovery; Windows `d63877f6`
  (with fixes) shows no memory status words, only `SCARD_E_COMM_DATA_LOST`.

## 3. Findings

### M1 — compaction rehash (historical, fixed)
`compact_deleted_overlay_records()` (`src/pico_filesystem.cpp`, rehash phase)
used to start path moves mid-chain, leaving a stale duplicate slot or losing a
live mapping. Fixed by path-head-first moves in `92c91943`. **Damage written
before the fix is still present on any board that was not wiped afterwards**
(D92, possibly 8E/2BED/4CDA). M6/M7 explain why that damage never heals.

### M2 — reachable alias target erased (historical, partly guarded)
The 8E candidate trace showed body
`03013894FFFFFE000000420007FFFF00000045` erased while its source field
`…0007` was reachable. `36fcfe69` added `reachableAliasTargetsBody()`
(`choupi/src/persistent_inventory.cpp:70`). **The reason the heap trace
omitted the edge was never identified**; see M5 for why the guard is
incomplete. Determining the omission cause remains open (task T6).

### M3 — single unresolvable candidate blocks reclamation forever
`reclaimUnreachableAppletTags()` `choupi/src/persistent_inventory.cpp:763`
and `reclaimUnreachablePackageStaticTags()` `:849`:

```cpp
if (... || fs_exists(candidate.value, candidate.len) == 0) {
  return VisitResult::ReadFailure;
}
```

Candidates come from `fs_next_tag()` (slot-order scan, tag reconstructed from
stored key bytes, `src/pico_filesystem.cpp:1578`), while `fs_exists()` uses
the hash probe chain (`find_overlay()`, `:700`). Any disagreement — orphaned
slot outside its probe chain, duplicate slot whose bytes now decode to another
tag (M6), parent-encoded child of an erased parent (M10) — yields a candidate
that is enumerated on every pass but can never be found or erased. The
reclaimer erases the candidates before it, returns `ReadFailure`, and repeats
that on every later request. Matches D92 line 3554 exactly
(`erased_applet=3`, result 6).

Failure scenario: overlay contains slot S whose reconstructed tag T is not
reachable via `hash(T)` probing → every deletion request erases ≤7 records
then fails at T → backlog stops shrinking → overlay fills → `F205`.

### M4 — RAM GC is all-or-nothing on edge failure
`choupi/src/main.cpp` post-APDU collector (`HEAP_GC_SKIP` at `:1158`, and the
`root_enumeration_failed` paths at `:1167`, `:1191`, `:1204`) skips the sweep
whenever `traceAndMark()` reports any invalid reference, edge-load failure or
root failure. A single missing persistent body therefore keeps **every**
unreachable RAM handle alive for the rest of the session. Confirmed by the 8E
log (§2.3) ending in `Out of memory`.

Keeping persistent records on an incomplete trace is correct. Retaining
volatile (non-persistent, untagged) RAM objects is not necessary for safety
only if they cannot be reached through the failed edge; the fix must define
that safely (see T3).

### M5 — alias guard protects only the body record
`Heap::hasReachablePersistentTagPrefix()` (`choupi/src/heap.cpp:1742`)
protects a candidate only if it is the exact tag of a **marked** object, a
direct `+2`-byte child of a marked object (`:1752`), or an applet root prefix.
`reachableAliasTargetsBody()` rescues an unmarked body `B` when a reachable
field aliases it, but `B`'s own fields `B‖ii`, reference-array elements and
nested bodies `B‖f‖FFFE‖id` are still candidates because `B` is not marked.
Missing field records decode as null/uninitialized
(`computeTagForRead`, `choupi/src/jc_handlers/flashmemory.cpp:1010`), so a
live object (e.g. a key whose component arrays are fields) can silently lose
state. *Inferred* link: Windows `6F00` / `CANT_BE_MEASURED` on prepare.

Also: `reachableAliasTargetsBody()` accepts only `FF FE` / `FF FF` markers
(`persistent_inventory.cpp:74`), not migrated object bodies `FF FD`
(`flashmemory.cpp:533`).

### M6 — offset fix-up assumes unique record offsets
`erase_overlay_slot()` (`src/pico_filesystem.cpp:1731`, fix-up loop `:1760`)
and `append_overlay()` (fix-up loop `:935`) memmove the record out and then
shift every other slot with `offset > old_offset`. A **second slot sharing
`old_offset`** (M1 duplicate) is skipped (`!=` the edited slot, offset not
greater) and now points at the next record's bytes. Results:
- `reconstruct_overlay_tag()` fails `plain_header` / `compressed_header` /
  (since `c1d14d6e`) `hash_mismatch` → compaction preflight returns
  `tag_reconstruction` (`:1143`) on every attempt → no space is ever
  recovered → `F205`. Matches D92 (§2.1).
- Before `c1d14d6e`, the stale duplicate could decode as a *different* valid
  tag of the same length → duplicate candidate → M3.

### M7 — snapshot load accepts an inconsistent index
`restore_overlay_slot()` (`src/pico_filesystem.cpp:452–468`) validates each
entry's bounds independently. It does not check: overlapping/shared record
offsets; `hash(reconstructed tag) == stored hash`; each used slot reachable
from its home bucket without crossing an unused slot; at most one live slot
per tag; `bytes_used` equal to the sum of record sizes. Corruption therefore
persists across reboot and firmware-only flashes, and is never repaired.

### M8 — package-static reclaimer has no alias guard
`visitUnreachablePackageStaticTag()` (`choupi/src/persistent_inventory.cpp:598`)
uses only `hasReachablePersistentTagPrefix` + foreign aliases. The M2 class of
bug (missed trace edge) would erase live package-static bodies with no guard.

### M9 — `append_overlay` mutates before rejecting `parent_missing`
`src/pico_filesystem.cpp:952`: the old record has already been memmoved out
and other offsets shifted, but on `find_parent_slot()` failure the function
returns `false` without updating `file`'s offset or `overlay_bytes_used`.
Only reachable for legacy parent-encoded records (`kOverlayParentTag`; not
created by production since `b88c9398`, 10-25).

### M10 — legacy-format gaps
- `stored_tag_matches()` requires a live parent (`:658`); erasing a parent
  makes parent-encoded children unfindable while `fs_next_tag()` still lists
  them → M3. The reclaimer erases in cursor order, not children-first (unlike
  `erase_overlay_records()`, which sorts longest tags first, `:1829`).
- `FF FD` migrated bodies not handled by the alias guard (M5).
Not applicable to stores created after 10-25, but relevant for any board
carrying older data.

### M11 — per-APDU full snapshot publish (performance, not exhaustion)
Every reclaimer erase dirties the overlay; `fs_sync()` then rewrites the
whole 160 KiB copy-on-write slot (0.4–1.5 s). Correlates with CCID
response-stall failures and adds flash wear on multi-hour campaigns. Track
under the CCID/transport work; listed here because deletion batches drive it.

## 4. Ruled out during this review

- Iterator invalidation in `Heap::traceAndMark()` — containers are
  `std::list` (`choupi/src/jcvm_types/list.hpp:41`).
- RAM blow-up from persistent array contents — restore is lazy
  (`flashmemory.cpp` array restore creates handle with size only).
- `JC_Array::setPersistentTag` retarget — it overwrites (`jc_array.cpp:198`).
- Legacy `(i,i)` vs `(0,i)` field-tag leaks and stale
  `getPersistentReference(field_tag)` hits — require pre-10-25 direct/legacy
  records; not present in a store wiped on 11-12.
- Handle limit — `kMaximumLocalArrayHandles = 0x3fff`; RAM runs out first.
- Parent-encoding as D92's cause — empty-card UF2 wipes
  `0x10100000–0x10200000`, and production stopped writing parent tags on 10-25.

## 5. Fix plan (worker tasks)

All tasks are offline-first. Follow AGENTS.md: one logical step per commit,
ARM+RISC-V builds clean, progress.md record. Do not modify supplied CAP/JAR
fixtures.

### T1 — Overlay integrity check and repair (M6, M7, M1 damage)  **first**
- Add `fs_check_integrity()` in `src/pico_filesystem.cpp`, run at snapshot
  load and as compaction preflight: unique non-overlapping offsets; sum of
  record sizes == `bytes_used`; stored hash == hash(reconstructed tag); every
  used slot reachable from its home bucket; ≤1 live slot per tag; parent
  slots valid.
- On failure: log one concise `FS_INTEGRITY_FAIL reason=… slot=…` line, then
  rebuild the index from the byte arena where records are self-describing,
  quarantining (dropping) only entries that cannot be reconstructed. Never
  silently drop a record that decodes and hash-matches.
- Make the M6 fix-up loops reject (fail closed, before mutation) when another
  slot shares the edited record's offset.
- Tests (`tests/unit/rp2350_fs_overlay`): inject duplicate-offset slot,
  orphaned slot outside probe chain, hash mismatch, overlapping records;
  assert detection, repair, and that compaction then succeeds and all live
  values survive snapshot/restore.

### T2 — Reclaimer progress despite one bad candidate (M3, M10)
- In both reclaimers, treat a candidate that is enumerable but not
  resolvable as an **integrity event**: record it, skip it, continue the
  batch, and trigger T1 repair before the next pass; do not erase anything
  whose state is ambiguous.
- Ensure the batch erases descendants before ancestors (longest tag first)
  so parent-encoded children are never orphaned.
- Distinguish `ReadFailure` causes in the `OBJECT_DELETION:` line
  (unresolvable candidate vs alias-chain read vs root scan).
- Tests: host reclaimer test with an injected unresolvable candidate among
  20 garbage records — all 19 others must be erased across batches and
  `pending` must reach 0.

### T3 — Bounded RAM GC under incomplete persistent trace (M4)
- Keep persistent erasure fail-closed. For RAM, still sweep objects that
  are provably not reachable through the failed edges: e.g. treat each
  failed edge's owner subtree conservatively (keep everything reachable from
  owners whose edge failed, and every persistent-tagged object), and sweep
  remaining untagged, unmarked volatile objects.
- Emit `HEAP_GC_PARTIAL` with counts instead of a blanket `HEAP_GC_SKIP`.
- Tests: heap unit test with one missing persistent body plus 200 garbage
  volatile arrays across 50 GC cycles — retained count must stay bounded.

### T4 — Guard the whole subtree of a rescued body (M5) and FD bodies
- When `reachableAliasTargetsBody()` rescues body `B`, protect every tag
  with prefix `B` for this pass (as `hasForeignAliasRoot` already does for
  foreign targets), or better, add `B` to the trace roots and re-trace.
- Accept `FF FD` markers.
- Test: reachable field aliasing an object body whose heap edge is omitted;
  the body's fields and a nested array body must survive reclamation and
  read back identical values.

### T5 — Alias guard for package-static reclamation (M8)
- Mirror T4 in `visitUnreachablePackageStaticTag()`.
- Test: package-static reference to object whose edge is omitted from the
  trace; nothing under it is erased.

### T6 — Find why the trace omitted the 8E edge (M2 root cause)
- Using the RP2350 overlay backend (T7), replay the 8E sequence with
  `PICO2_DEBUG_HEAP_GC`-equivalent tracing and compare the strict flash walk
  against the alias chain for every rescued body. Every `alias-protect`
  event should be zero on a correct trace; non-zero means a remaining trace
  bug. Report the first instance with tag, field index and class.

### T7 — Offline long-run reproduction on the real storage backend
- Add a PC VPCD build that links `src/pico_filesystem.cpp` over the existing
  fake flash (same 96 KiB arena, 8,192 slots, persistent snapshot file)
  instead of the PC filesystem.
- Run unchanged JCAlgTest option 2 (1.8.5 client / 1.8.4 jc305 CAP) with
  `PICO2_TEST_OBJECT_DELETION=1`; assert: no `SKIP`, `pending` reaches 0
  between methods, live record count plateaus, zero compaction rejects, zero
  `alias-protect` events, RAM handle count bounded.
- This is the acceptance harness for T1–T6 and removes dependence on
  physical boards for storage bugs.

### T8 — Physical acceptance (after T1–T7)
- Wipe every test board with an empty-card UF2 first (D92 mandatory) and
  record the initial overlay file count/bytes in run metadata.
- Run option 2 and option 1 on separate identified boards per AGENTS.md.
- Keep CCID response-stall work (M11, CCID-STALL) separate from MEM-01.

### T9 (process) — keep evidence actionable
- One progress.md record and one commit per logical step; stop separate
  "backfill SHA" commits (65% of 263 commits since 11-10 were bookkeeping).
- Keep failure classes separate: storage (MEM-01), CCID delivery
  (CCID-STALL), host/PCSC/BOOTSEL availability.
- Never run acceptance on a board whose store was written by an image older
  than the newest storage fix.

## 6. Priority / ordering

T1 → T2 → T7 → T3 → T4 → T5 → T6 → T8. T1/T2/T3 remove the permanent
failure modes; T7 provides offline proof; T4/T5/T6 close live-data-loss
risks; T8 is physical acceptance.
