# Review: slow JCAlgTest baselines and per-prepare garbage (2026-11-19)

Scope: why a JCAlgTest EC_FP_384 Signature prepare leaves ~118 dead persistent
records, why a no-op baseline APDU costs 0.8 s (EC) to 9.4–9.9 s (RSA-1280
Cipher in the Windows 621ba60c run), and the plan to remove unnecessary work.
Applet: unchanged JCAlgTest 1.8.5 / jc305 CAP, client 1.8.5.

## 1. What the applet does

- `getECCKeyPairObject()` runs `new KeyPair(typeECC, keyLength)` and
  `EC_Consts.ensureInitializedECCurve(...)` on **every** prepare. EC pairs are
  not cached; RSA pairs are (`getKeyPairObject` keeps `m_cached_keyPair_RSA_*`).
- `prepare_class_Signature` calls `Signature.getInstance` twice per prepare;
  `prepare_class_Cipher` calls `Cipher.getInstance` once.
- `KeyPair.getPrivate()` / `getPublic()` allocate nothing: the VM returns the
  stored field (`choupi/src/native_ec_keypair.cpp`).
- `TestSettings.parse()` calls `clear()` and then sets every field of the
  persistent `m_testSettings`: ~26 persistent field writes per measure APDU,
  including each baseline (`numRepeatWholeOperation = 0`).
- The client sends `B0E2` (`requestObjectDeletion`) before every baseline and
  every measurement; only the measure APDU is timed.

## 2. How the VM stores persistent objects

- Every persistent instance field and every persistent array is its own record
  in the filesystem (`JC_Instance::setField_*` →
  `FlashMemory_Handler::setPersistentField_*` → `fs_write`).
- A NativeKey has 22 fields. An EC key writes ~11 field records plus 8 arrays
  (7 components + lengths). 4 EC keys + 2 KeyPairs + 2 Signatures ≈ 110–120
  records, matching the 118 records reclaimed after a replacing EC384 prepare.
- Records live in a 96 KB RAM overlay (`src/pico_filesystem.cpp`).
  `fs_sync()` runs after every applet APDU (`src/ccid_device.cpp`). If the
  overlay is dirty, `persist_overlay()` erases and rewrites the **whole**
  snapshot (bitmap + index + all record bytes) into the other A/B flash slot.
  Cost scales with total store size, not with what changed.
- Java Card semantics: objects from `new`, `KeyPair`, `getInstance` and
  non-transient key types are persistent and are stored in applet fields, so
  they must survive reset. They cannot be RAM-only. Implementation-internal
  working state can be.

## 3. Measurements (07E74C5D7A038DEC, image `da180d19…dbf9`, 1.8.5 CAP)

Store: 546 files, 15.2 KB. Tool: `tests/hardware/ApduTimer.java`.

| APDU | Time |
|---|---|
| GETVERSION (no persistent write) | 25–27 ms |
| EC384 sign baseline `B049`, repeat 0 | 810–825 ms |
| EC384 sign prepare `B032` | 1230 ms |
| first `B0E2` after prepare (118 records reclaimed) | 1494 ms |
| later `B0E2` | 349 ms |

- ~0.79 s of each baseline is persistent-write + whole-store sync overhead.
- Reclaim of 118 records: `reclaim_us=785210` (~6.6 ms/record) with
  `prefix_queries=11061`, i.e. dominated by per-record reachability lookups
  in RAM, not by flash.
- RSA-1280 baselines of 9.4–9.9 s (rising) are consistent with the same
  overhead on a much larger store (cached RSA pairs). Not yet confirmed: needs
  the Windows serial log or the step-1 counters.
- Flash wear: each dirty APDU erases the whole written slot (A/B alternate);
  a full option-1 run has thousands of APDUs.

Logs: `artifacts/results/baseline-sync-cost-20261119/07E74C5D7A038DEC/`
(`*-noLe-invalid.log` is an operator error: missing Le → APDUException F501).
Earlier first-baseline skew: `artifacts/results/first-baseline-ec384-20261118/`.

## 4. Plan (VM only; CAP unchanged)

1. **Instrument.** Per APDU: persistent write count, `fs_write` time,
   `fs_sync` time and bytes written. Replay EC384 and a large-store RSA case.
2. **Skip net-unchanged writes.** At APDU commit, drop records whose final
   value equals the committed value; if nothing net changed, do not rewrite
   flash. Baselines should drop to ~30–50 ms regardless of store size.
3. **Incremental journal sync** (deferred): append changed records with CRC,
   rewrite the snapshot only when the journal fills. Needs tear tests.
4. **Fewer records per native object.** Store a NativeKey's components as one
   record; keep Signature/Cipher working state RAM-only. Target ~30 records
   per EC prepare instead of 118.
5. **Cheaper reclaim.** Replace per-candidate prefix queries with one marked
   reachable set so reclaim cost is not ~6.6 ms per record.

Applet-side suggestions for a future JCAlgTest release (not applied): cache EC
key pairs like RSA pairs; keep `TestSettings` in a transient array.

## 5. Implementation results (2026-11-19)

Steps 1, 2, 4 and 5 implemented; step 3 deferred. Same EC384 sign replay on
07E74C5D7A038DEC (one prepare, then `B0E2` + baseline pairs), 1.8.5 CAP.

| Measure | Before | After |
|---|---|---|
| Steady baseline (`B049`, repeat 0) | 806–843 ms | 229–236 ms |
| First baseline after prepare | ~800 ms | ~705 ms |
| Dead records per replacing prepare | 118 | 52 |
| Prepare (`B032`) | 1230 ms | ~1000–1040 ms |
| First `B0E2` after prepare | 1494 ms | 729 ms |
| Steady `B0E2` | 349 ms | 191 ms |
| Store after prepare | 545 files / 19.1 KB | 411 files / 14.9 KB |

The steady baseline that remains is real applet work:
`perftest_class_Signature` signs once before its loop even when repeat is 0.

| Step | Commit subject | Effect |
|---|---|---|
| 1 | Print per-APDU filesystem write and sync telemetry | `CHOUPI:FS_APDU` line per applet APDU |
| 2a | Rewrite same-size overlay records in place | no arena move / index rescan per field write |
| 2b | Skip the flash snapshot rewrite when an APDU's net change is empty | baseline sync ~0.52 s → ~0.15 ms |
| 4a | Store an EC key's components in one persistent record | 21 → 7 records per EC key; legacy layout still read/written |
| 4b | Allocate Signature working buffers on first use | ECDSA Signature stores no IV / recovery-hash buffer |
| 5 | Reclaim each deletion batch from one complete inventory scan | 52-record reclaim 285 → 169 ms |
| 3 | Commit tracked changes to a flash journal instead of rewriting the snapshot | a real change commits in ~1 ms (256 B) instead of ~0.5 s |

Step 3 (commit journal, 2026-11-19): with the journal, the first baseline
and the first operation measurement of a method no longer pay a snapshot
rewrite. Replay on 07E7: baselines 234–237 ms, operations (repeat 2)
508–510 ms, all five samples equal; each first sample journals 256 B in about
1 ms. Full snapshots, written only by a prepare or a deletion pass that
erases many records, now also erase the 16 KB journal region (40 KB, about
0.65 s); JCAlgTest does not time them. A reboot replays the journal (checked
on 07E7). Power-cut atomicity is covered by unit tests that cut at every
mutation of an append and during a full snapshot.

Remaining limitations:

- One inventory scan still costs ~85 ms for ~712 tags: `fs_next_tag`
  rebuilds and hash-verifies every used slot's tag. Each erase costs ~1.6 ms
  (`erase_overlay_slot` moves the arena tail and rescans the index).
- Per EC384 prepare 52 records remain: 4 keys × 7, 2 KeyPairs × 5,
  2 Signatures × 6–8 (the message buffer is allocated by the first sign).
- Firmware without the journal ignores journal entries and boots the last
  snapshot, so a downgrade loses commits made since that snapshot.
