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
