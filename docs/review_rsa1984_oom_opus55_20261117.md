# RSA-1984 OOM / object-deletion static review (2026-11-17)

Part 1 is the original review brief (copied verbatim from `.agent/code_review_instructions_memory_20261117.md`). Part 2 is the analysis.

# Part 1: Review instructions (verbatim)

## Static code review: RSA-1984 OOM / non-reclaiming object deletion (2026-11-17)

Use: give a large model the files below plus the prompt. It reads code only; it cannot build or run anything.

## Context to attach

Required, in this order:
1. `SPEC.md` (invariants; INV-16 allocation discipline, INV-24, INV-27).
2. `choupi/src/main.cpp`: `B0E2` / `requestObjectDeletion` path (about lines 449, 541, 849-1000: `available_before_reclaim`, `PRE_CLEANUP`, `PersistentReclaimer::reclaimUnreachableObjectDeletionBatch`, `enumerate_persistent_root_vector`).
3. `choupi/src/persistent_inventory.cpp` and `.hpp` (reclaimer and root marking; most important).
4. `choupi/src/heap.cpp` and `heap.hpp` (`addArray`, instance handles, GC).
5. `choupi/src/interpretor.cpp` and `choupi/src/vm_cap_initialization.cpp` (object creation).
6. `choupi/src/native_rsa_cipher.cpp`, `native_cipher.cpp`, `native_message_digest.cpp` (native paths calling `heap.addArray`).
7. `src/pico_filesystem.cpp` (persistent file creation paths only; compaction is already ruled out as a heap user).
8. `artifacts/backups/testing-data-migration-20261107/AlgTest_JavaCard/src305/algtest/AlgPerformanceTest.java` and `JCAlgTestApplet.java` (backup copy; confirm it matches the supplied 1.8.4/jc305 CAP).
9. Relevant part of `docs/review_memory_analysis_opus55_20261113.md`.

Evidence to attach:
- `artifacts/results/windows-maxcrypto-20261117-c050a193-deletion/serial_log.txt` (only the last 600 lines, ending in `PANIC / Out of memory`).
- T7 block and LONGOP-01 section of `TODO.md`.
- `progress.md` records: "Measure RSA-1536 target control", "Extend LONGOP-01 physical RSA matrix", "Recover Pico after GPPro preflight OOM".
- Linux comparison: `artifacts/results/longop01-rsa1984-physical-20261117T164222Z/F42DD9D07D6D2B07/serial.log`.

Notes:
- Keep `persistent_inventory.cpp` and `heap.cpp` whole; root and reachability logic is spread through them.
- If the context window is small, send questions 2 and 3 with the reclaimer, heap and `main.cpp` first, then 1, 4 and 5 with the native and interpreter files.
- Tell the model the Windows serial log is only the tail of the run.
- Reject hypotheses that lack file:line citations.

## Prompt

```
You are doing a static code review of a native Java Card VM for the RP2350 (Cortex-M33) that interprets installed CAP files.
You cannot run anything. Reason from the code and the logs below only. Cite file:line for every claim and say whether it is
verified by reading code or inferred. Do not propose edits beyond a minimal sketch. Do not weaken any SPEC invariant.

PROBLEM
A JCAlgTest option-2 run on a Windows host panicked "Out of memory" (malloc failure, PICO_MALLOC_PANIC) on firmware c050a193
during an RSA-1984 PKCS1 x50 B043 measure APDU, right after a B0E2 reset APDU. On a clean Linux card (about 450 persistent
files, arrays_live 65-129, instances_live 32) the same operation passes with about 70-100 KB free, so the call itself is not
too big. On the Windows card the log shows, at every B0E2 pass: files=5096..5116 (growing), bytes=65 KB/98 KB, arrays_live=201,
instances_live=73, static_roots=65, applet_roots=1, and arrays_reclaimed=0 instances_reclaimed=0 erased_applet=0
erased_package=0 pending=0. PRE_CLEANUP available was about 33 KB (Linux about 82 KB), using the same mallinfo-based metric.

KNOWN / RULED OUT
- src/pico_filesystem.cpp uses fixed static tables and in-place compaction; no heap allocation. Not the cause.
- The B0E2 APDU does reach the card and does trigger the deletion pass (1,874 of 1,879 acked 9000).
- JCAlgTest by design holds a bounded set of objects: the roughly 100 fields of AlgPerformanceTest (cached RSA key pairs
  m_cached_keyPair_*, m_key1/2, m_cipher, ...). Each prepare re-assigns a field, so replaced objects become garbage;
  B0E2 nulls some cache fields then calls JCSystem.requestObjectDeletion(). So the applet cannot legitimately keep 5,000 files alive.
- D1 (no CCID time extension in modexp) is fixed in a newer commit and is not this problem.

QUESTIONS (answer each, ranked by likelihood)
1. What does "files" count in the PRE_CLEANUP line, and which VM operations create persistent files? Enumerate every call
   site that creates one (EEPROM arrays, key components, instances, CAP/package data). Which of them are per-API-call rather than per-object?
2. Trace requestObjectDeletion -> PersistentReclaimer. How are roots chosen (static_roots=65, applet_roots=1)? Can an object
   that JCAlgTest dropped (field re-assigned or nulled) still be reachable from a VM root, a native handle table, a key-component
   cache, the APDU/applet context, or an uncleared slot? Look for over-marking, missing unmarking, handle-table slots not cleared
   on re-assignment, and exception paths that skip cleanup.
3. Why would a pass report arrays_reclaimed=0 and erased_*=0 while files grow? Check for early-outs: batch limit
   (kObjectDeletionMaxRecordsPerApdu), same_context checks, owner/package filters, pending flags, stack_empty conditions,
   or reclaim logic that applies only to some object kinds.
4. Does every native key/cipher/signature API allocate JC heap arrays (heap.addArray) or persistent files per call or per
   init/genKeyPair/doFinal, and are these released or reused? Quantify bytes per call for RSA-1984 doFinal and genKeyPair.
5. What on the Windows-run path consumes malloc heap in proportion to live JC objects or files (handle vectors "arrays"
   and "instances", per-object metadata, mbedtls contexts held in native object tables, heap-resident caches of persistent
   contents)? Give a formula or estimate for the 49 KB difference between the two cards.
6. Can heap fragmentation alone explain the panic given free bytes are ample in total? Which allocation would be the largest
   contiguous request during RSA-1984 doFinal (bignum limbs, window tables) and where is it requested?
7. Anything else that violates INV-16 (allocation discipline) on these paths.

OUTPUT
A ranked hypothesis list. For each: the mechanism, the exact code evidence (file:line), what log lines would confirm or refute it,
a cheap offline test (unit or VPCD) that could prove it, and a minimal fix sketch. Separate verified facts from guesses.
End with the three most informative extra log lines to add to firmware (diagnostic mode only) to settle the open points.
```

---

# Part 2: Analysis (claude-opus-5-5, 2026-11-17)

Static review only: source reading plus the existing logs. Nothing was built,
run, or changed. Firmware under study: c050a193. HEAD differences are noted
where they matter. "Verified" means read in code or seen in a log; "inferred"
means reasoned but not measured.

## Corrections to the brief

| Brief says | What the code and logs show |
|---|---|
| PRE_CLEANUP `available` is mallinfo-based | It is free space in the persistent overlay: `98304 − overlay_bytes_used` (`src/pico_filesystem.cpp:2248-2256`). 98304 − 64798 = 33506. The "49 KB difference" is in overlay bytes. **The Windows log contains no malloc measurement at all.** (verified) |
| `files` = live persistent objects | `files` is `overlay_file_count`, which includes tombstones. An erase leaves its key bytes behind and sets Deleted (`pico_filesystem.cpp:2612-2616`, `:2637`). Only compaction lowers the count. On Linux the count stays at 460 after erasing 8 and then 4 (Linux `serial.log` lines 39-69). (verified) |
| `erased=0` at every pass | Wrong. After the B031 prepare (Windows log line 341), the passes erased 8, 8 and 4 (lines 359, 383, 416). That is exactly the +20 files the prepare created. Linux also adds +20 files and +446 bytes per prepare. (verified) |
| B0E2 nulls cache fields | Only with P1=07. The client sends `B0E2 00`, which only calls `requestObjectDeletion()` (`JCAlgTestApplet.java:378-385`). The cached RSA key pairs for every length tested so far stay alive by design. (verified) |
| Panic during "the" B043 | Five baseline B043s ran with an iteration count of 0 (`…00 00 00 01`) and passed. The **first B043 with ×50** (`…00 32…`) panicked (client log `ALGTEST_log_1794927816233.log` lines 30929-31036). (verified) |

## Ranked hypotheses

### H1. Most likely: each RSA `doFinal` leaks one JC heap array within the APDU

- **Mechanism (verified by reading code):** `doFinalRsaCipher` allocates a new
  array with `heap.addArray(modulusLength, BYTE)` on every call. It is never
  stored or released (`choupi/src/native_rsa_cipher.cpp:587` at c050a193,
  still `:617` at HEAD).
- **When it is freed:** only by the after-APDU collector
  `collectUnreachableFrom` (`choupi/src/main.cpp:1328`), which runs only when
  the Java stack is empty.
- **Effect in the ×50 measure:** the loop in `AlgPerformanceTest.java:1296-1322`
  makes 51 calls in one APDU (1 prepare + 50), so 51 dead arrays pile up.
- **Size (measured earlier on target):** `progress.md:31108`, physical RSA-1984
  ×50: available transient bytes went from 117,064 to a minimum of 81,376.
  That is about 35.7 KB, roughly 700 B per call. The GC afterwards reclaimed
  51 arrays.
- **Why it hit at that APDU:** this matches "baseline ×0 passes, ×50 panics".
  The same log shows the single prepare leak being collected
  (`arrays_reclaimed=1`, Windows log line ~357).
- **Would be confirmed by:** a transient-heap reading at B043 entry that falls
  about 700 B per iteration.
- **Cheap offline test:** the PC instrumentation already exists
  (`PICO2_RSA_HEAP_DIAGNOSTICS`, HEAD `native_rsa_cipher.cpp:603-650`,
  `main.cpp:909-921`). It already showed live arrays going 112 → 163 → 112
  (TODO.md:1021). A unit test could assert that live handles are unchanged
  after N `doFinal` calls.
- **Fix sketch:** have `transformRsa` write into a native `std::vector`
  (already zeroized by `RsaSensitiveBufferGuard`) and copy straight into
  `output`. Never allocate a JC array per call.
- **Invariant note:** INV-16's text covers *persistent* storage, and this array
  is never written to flash. So it is not a literal INV-16 violation, but it
  goes against "prefer preventing an allocation". Whether that counts is for
  the user to rule.

### H2. Likely contributor: the Windows card enters the APDU with less RAM headroom

- Live objects at the B0E2 passes: Windows 201 arrays / 73 instances, against
  131 / 34 (later Linux run) or 65-129 / 32 (this Linux log).
- Each `JC_Array` carries two 129-byte `fs::Tag` members
  (`choupi/src/jc_types/jc_array.hpp:71,83`) plus a backing buffer of
  data + 3 bytes (`jc_array.cpp:121-133`, `:75-84`).
- No call site was found that frees the RAM backing after `writeArray`
  serializes the array (`choupi/src/jc_handlers/flashmemory.cpp` ~`:983`).
  Inferred from grep, not proven. Transient workspaces such as MessageDigest's
  2046-byte buffer (`native_message_digest.cpp:29,142`) also stay in RAM while
  their owner is cached.
- Rough estimate: 70 arrays × ~500-700 B + 39 instances × ~200-400 B ≈
  45-60 KB less headroom. Subtract the ~36 KB H1 swing, and the margin is
  plausibly gone. Inferred; there is no target telemetry for c050a193.
- **Fix:** H1 first; then drop the RAM backing once an array is flash-backed.

### H3. Real, but not the OOM cause: tombstones accumulate in the overlay

- Each prepare/reclaim cycle leaves about 20 tombstone slots and ~174 B net
  (64798 → 64972 bytes across the cycle). Over hours this builds up to
  ~5,000 slots.
- Compaction runs only when `available ≤ 20 KB` and at least one record was
  erased (`src/object_deletion_policy.hpp:9,31-35`), or on demand when the
  overlay is full (`pico_filesystem.cpp:2507`).
- The overlay is a static array (`pico_filesystem.cpp:173-181`), so this is not
  malloc. It does explain the misleading `files` number.
- **Test:** a fake-flash test that loops create/erase and asserts the slot
  count.

### H4. Unlikely as the sole cause: fragmentation

- The largest contiguous request in RSA-1984 `doFinal` is about 620 B: the CRT
  exponent vector in `transformRsa` (`native_rsa_cipher.cpp:229`).
- mbedtls modular exponentiation uses window size 3 (SDK
  `lib/mbedtls/include/mbedtls/bignum.h:63`), so 8 table entries of about
  130-260 B each.
- The heap's `List` is a `std::list` (`choupi/src/jcvm_types/list.hpp:41`).
  Slots are reset but nodes are never freed.
- Fragmentation can only matter on top of H1/H2 exhaustion. To tell them apart,
  log the size of the failing request.

## Other answers

- **Q1, what `files` counts and who creates records:** all used overlay slots,
  tombstones included (see corrections). Record tags: package statics
  `02 pkg slot`, applet fields `03 owner pkg class field`, applet root
  `… ff`, array bodies `field_tag ff ff id32`, object bodies
  `field_tag ff fe id32`, plus +2-byte field/element records
  (`flashmemory.cpp:492-540`, `:988-1017`; `choupi/os/src/filename.rs:133-179`).
  Records are created when objects are stored into persistent fields. The
  per-prepare +20 comes from `Cipher.getInstance` assigning a new `m_cipher`;
  `doFinal` creates no persistent records.
- **Q2, roots:** `static_roots=65` counts the package's static reference slots
  (`main.cpp:409-417`, `:511-514`). `applet_roots=1` is the applet instance
  root (`main.cpp:273-357`). Retained/VM roots are added to the root list
  without being counted (`main.cpp:505-515`). Reachability keeps only an
  object's exact tag and records exactly 2 bytes longer
  (`choupi/src/heap.cpp:1822-1873`). Since 20 of 20 records were reclaimed,
  there is no evidence of over-marking.
- **Q3, early-outs:** the batch limit is 8 records per APDU
  (`persistent_inventory.hpp`, `kObjectDeletionMaxRecordsPerApdu`). It shows
  up as `pending=1` and resumes correctly. `erased=0` otherwise means there was
  no garbage.
- **Q4, per-call cost:** RSA `doFinal`: 0 persistent records, 1 leaked JC array
  (~700 B on target, see H1). Freed per call: modulus/input/result vectors
  (248 B each), exponent vector (620 B), CRT components (5×124 B), mbedtls MPIs.
  `genKeyPair` was not analysed in depth (`native_key` / `native_ec_keypair`
  generation not traced).
- **Q5, formula:** heap used ≈ Σ live arrays (sizeof(JC_Array) ≈ 330 B +
  control block + backing + malloc overhead) + Σ instances + list nodes. The
  49 KB figure in the brief is an overlay-byte difference, not malloc.
- **Q6:** see H4.
- **Q7, robustness:** `PICO_MALLOC_PANIC` is left at its SDK default of 1
  (`.local-toolchain/sdk/src/rp2_common/pico_malloc/malloc.c:60-66`). So every
  `catch (std::bad_alloc)` in the VM is dead code on target (e.g.
  `Heap::addTransientArrayResult`). Any OOM is a hard panic that kills the
  card, instead of a SystemException the applet could see. No path collects
  garbage mid-APDU and retries an allocation.

## Three diagnostic log lines (diagnostic builds only)

1. At B043 entry and every 10 `doFinal` calls:
   `choupi_available_transient_memory_bytes()`, mallinfo used/free, live array
   handles and slots. Settles H1 and H2.
2. At PRE_CLEANUP: mallinfo used/free/largest free block, plus `files_live` and
   `files_tombstone` reported separately. Removes the metric confusion and
   quantifies H2.
3. On the target's allocation-failure path (`PICO_DEBUG_MALLOC` or a panic
   hook): the requested size and mallinfo at failure. Separates exhaustion
   from fragmentation (H4).
