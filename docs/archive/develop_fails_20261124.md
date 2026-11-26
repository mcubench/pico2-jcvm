# Bugs the agents introduced and later fixed (development retrospective, 2026-11-24)

## Scope and method

Source: `progress.md` as of 2026-11-24 (about 2,200 records, 33,800 lines). It was not read
end to end. The list was built by keyword sweeps (`regression`, `root cause`, `caused by`,
`leak`, `6f00`, `violat`, `Java provider`, `Fix ...` headings), by reading the matching
records, and then by reading `git show` of the fixing commit (and, where it could be found,
the introducing commit, usually with `git log -S`). Every hash below was checked with
`git cat-file -t`, and every snippet was copied from the diff of the named commit.

Only defects that an earlier agent change put into the code and that a later step found and
fixed are listed. Unimplemented features, hardware faults and host-tool glitches are not.
Where the introducing commit could not be pinned down, the entry says so.

Caveat: the categories and the choice of "best" examples are one reader's judgement. The
"fixes seen" counts are the fixes this sweep noticed, not a complete census.

## Summary

| # | Category | Fixes seen | Example hashes |
|---|----------|-----------|----------------|
| 1 | Cryptography written as Java on the token instead of native C++ | 6+ | 88c40ef9 -> 031f4173, 5d05e30c -> 425ccfa1 |
| 2 | PIN retry counter and validated flag with the wrong lifetime | 3 | 325e9dba, a266b836 |
| 3 | Memory retained per operation and never released | 4 | cbe76c2d, 7ca591e6 |
| 4 | Native C++ crypto leaking on every call | 3 | 5e95a982, 0018e47f |
| 5 | Exception objects: allocated per throw, then refused under low memory | 3 | 354e36ae, d65eb278 |
| 6 | Reclaimer that erases live data or dies of the memory it tries to free | 5 | 36fcfe69, 9228c583 |
| 7 | Flash overlay index and compaction losing or resurrecting records | 6 | b88c9398, 92c91943 |
| 8 | Transaction rollback that did not really restore or publish | 4 | d78fc7ef, ab8e74f2 |
| 9 | Identity reuse after deletion (stale proxies, recycled owner IDs) | 4 | 0c69bdb2, 4fa2ec43 |
| 10 | Reset and power-off wired to the wrong lifecycle callback | 3 | 0425ad59, 97fc4ab0 |
| 11 | JC305 method-token remaps and key-parameter checks that did not match the spec | 6+ | 22a6255f, c42fab06 |
| 12 | CAP validation written from a wrong reading of the specification | 4 | 618d1f1a, ca72f14b |
| 13 | Hidden assumptions about initial values and record layout | 4 | aac2481f, a811c6f3 |
| 14 | Fixed-width limits (buffer sizes, 16-bit offsets) smaller than the real data | 3 | b9ef7f44, fbba9ef6 |
| 15 | Debug output and test-client mistakes that corrupted the evidence | 4 | ac81707d, 3115ba19 |
| 16 | Safety limits that lag behind the workload or defeat themselves | 4 | 5e5f0389, 74a5fd93 |

---

## 1. Cryptography written as Java on the token instead of native C++

Project rule: Java Card API behaviour and algorithms live in native C++ dispatched from the
VM; no Java provider or adapter classes in firmware. Early steps built selectable Java
"provider" classes (with a thin native bridge) and the rule was enforced later, by an audit
and by the user.

**1a. SHA-256 and the other digest providers.** A generated `Sha256DigestProvider extends
MessageDigest` held the buffering logic in Java and only called a native bridge. The same
pattern was applied to secure random, CBC ciphers, symmetric keys and OwnerPIN. Fixed by
native `MessageDigest` dispatch, removing the generated Java class.
Introduced `88c40ef9`; fixed `031f4173`.

```java
// 88c40ef9 tools/java-support/Sha256DigestProvider.java
final class Sha256DigestProvider extends MessageDigest {
    private final byte[] input = JCSystem.makeTransientByteArray(
        (short)(MAX_INPUT_LENGTH + 2), JCSystem.CLEAR_ON_RESET);
    ...
      if (!Sha256NativeBridge.update(state, inBuff, inOffset, inLength)) {
```

```python
# 031f4173 tools/javacard-support.py: Java provider generation removed,
# API methods declared as native instead
-    digest_provider = ["package javacard.security;", "",
-        "final class MessageDigestProvider {", ...
+    "javacard/security|MessageDigest|MessageDigest/doFinal|instance",
```

**1b. Symmetric MAC and RSA signature providers.** `SymmetricMacProvider`/`SignatureProvider`
(from `5d05e30c`) and a not-yet-committed RSA signature provider were generated into the ROM
mask. The 2026-10-25 audit recorded "violates the user's explicit native-only requirement"
before it was committed (`ba0958c6`); the MAC providers were replaced in `425ccfa1` and the
RSA signature went native in `2a905d71`, which also deletes the generated provider file.

```python
# 2a905d71 tools/javacard-support.py
+    rsa_signature_provider = nativeimpl_dir / "RSASignatureProvider.java"
+    rsa_signature_provider.unlink(missing_ok=True)
```

Other ROM Java bodies are still tracked as an INV-03 gap (record of 2026-11-23).

## 2. PIN retry counter and validated flag with the wrong lifetime

OwnerPIN's try counter, validated flag and blocked state must not take part in
transactions, the decrement must be durable before the comparison result is visible, and
validation must clear on reset.

**2a. Counter stored as an ordinary persistent field.** Review R04 (docs/archive/review_20261106.md)
found that a wrong-PIN decrement inside a transaction could be undone by abort and that a
validated PIN could survive reset. Fixed by native OwnerPIN state plus a one-record durable
commit. Fixed in `325e9dba` (the ROM `OwnerPIN` was ordinary Java fields from the start;
introducing commit not pinned).

```cpp
// 325e9dba flashmemory.cpp setPersistentFieldDurable_Byte
  throw_persistent_write_status(
      fs_write(tag.value, tag.len, data, sizeof(data)));
  if (fs_commit_tag(tag.value, tag.len) != 0) {
    // could not be made durable now: fail closed
    ...setPendingSystemExceptionReason(5); // NO_RESOURCE
    throw Exceptions::SystemException;
  }
```

**2b. The fix over-reached.** The first fix also routed OwnerPINx *administrative*
`setTriesRemaining` through the durable, non-transactional path, but the spec says that
update is transactional. A second step restored it and made only the presentation path
(`decrementTriesRemaining`, the restore-one-try step of `check`) call the native setter.
Introduced `325e9dba`; fixed `a266b836`, verified across cold restarts on hardware.

```java
// a266b836 OwnerPINxWithPredecrementImpl.java
-      setTriesRemainingForSubclass((byte)(remaining - 1));
+      // PIN presentation: durable and outside any transaction
+      NativeImplementation.ownerPinSetTries(this, (byte)(remaining - 1));
```

## 3. Memory retained per operation and never released

Allocations made when an object is created or read must be freed when the object is
garbage or no longer needs them. Found on hardware as heap exhaustion in long JCAlgTest runs.

**3a. A persistent reference field was re-read from flash on every `GETFIELD_A`.** Each read
built a fresh heap wrapper, so a crypto object holding a transient array leaked one backing
array per operation and lost object identity. JCAlgTest static SHA sweep died with
`malloc 2060 failed`. Introduced `a740f5ec` (an earlier "repair persistent instance reference
access"); fixed `cbe76c2d`.

```cpp
// cbe76c2d jc_instance.cpp getField_Reference
-    fs::Tag tag = FlashMemory_Handler::computeTag(this->recomputeOriginalTag(), index);
-    return FlashMemory_Handler::getPersistentField_Reference(tag, this->getOwner());
+    // restored once into the instance's field cache; re-reading allocates
+    const auto fields = this->getFields();
+    return jref_t(fields->at(index).value);
```

**3b. RAM copy of every array kept after it was written to flash.** After a non-transient
array was serialized, all reads went to flash, yet the RAM copy (kept only for rollback)
stayed. Found by the RSA-1984 OOM code review. Regenerating one RSA-1984 key set in a session
cost 2,024 B of RAM; after the fix 480 B. Fixed `7ca591e6`.

```cpp
// 7ca591e6 flashmemory.cpp writeArray
  if (!persistent_write_transaction_active) {
    array.releaseCommittedLiveData();   // shrink RAM backing to [tag len, tag]
  }
```

## 4. Native C++ crypto leaking on every call

The VM must not pay RAM or flash per operation. These leaks sat in new native code and were
found through hardware option-1 campaigns.

**4a. mbedtls curve parameters marked "static".** `loadSmallCurve` (secp128r1/160r1) read
parameters into heap MPIs but set `h = 1`, which tells `mbedtls_ecp_group_free` the tables are
static. Every ECDSA/KeyPair/KeyAgreement call on those curves leaked 7 allocations (~168 B).
JCAlgTest option 1 panicked "Out of memory" in the EC_FP_128 loop (external run 853543d6).
A LeakSanitizer unit test showed 16,384 B leaked in 896 allocations before the fix.
Introduced `851104db`; fixed `5e95a982`.

```cpp
// 5e95a982 native_ec_group.hpp
-  group.MBEDTLS_PRIVATE(h) = 1;
+  // h = 1 means P/A/B/G/N are static tables and must not be freed.
+  // These are heap MPIs, so h must stay 0.
+  group.MBEDTLS_PRIVATE(h) = 0;
```

**4b. RSA cipher workspace replaced on every init.** The transient exponent array was sized
by key kind, so alternating public and CRT-private keys on one Cipher replaced it each time,
leaving two dead array records per JCAlgTest decrypt measurement (`erased_applet=2` in the
serial log). Size it once for the CRT layout and keep the key kind in a session byte.
Introduced `67830a94`; fixed `0018e47f`.

```cpp
// 0018e47f native_rsa_cipher.cpp
+  // Workspace is sized for the CRT layout (5 * ceil(k/2) bytes) whatever the
+  // key type, so alternating public/private/CRT keys reuse it.
+  const uint32_t exponentLength =
+      crtPrivate ? static_cast<uint32_t>(crtPartLength) * 5U : keyLength;
```

## 5. Exception objects: allocated per throw, then refused under low memory

Java Card needs a runtime-owned, reusable exception instance. Two opposite mistakes in the
same code.

**5a. One new `CryptoException` per `throwIt()`.** The unsupported-algorithm sweep throws
thousands of them; instances were never reclaimed. Hardware log: `malloc 12 failed to
allocate memory`. A tried-and-reverted Java static cache failed because VM statics persist
through flash. Fixed with a per-heap VM-owned root, `354e36ae`.

```cpp
// 354e36ae heap.cpp
jref_t Heap::addRuntimeCryptoException(...) {
  jref_t &root = classIndex == 234 ? this->runtime_crypto_exception_rom
                                   : this->runtime_crypto_exception_imported;
  if (root.isNullPointer()) root = this->addInstance(packageID, classIndex);
  return root;
}
```

**5b. The exception instance was refused by the same RAM reserve that raises it** (the
requested pair). Commit `a1639b68` added a 16 KiB Java allocation reserve that throws
`SystemException.NO_RESOURCE`, but creating that exception's instance went through the same
check, so the interpreter returned an uncatchable `6F00` (JCAlgTest only catches
`SystemException`/`CryptoException`). Windows run be737f0e showed 20 `6f00` rows in
RSA-CRT-4096; reproduced on hardware, then 0 after the fix. Introduced `a1639b68`; fixed
`d65eb278`.

```cpp
// d65eb278 heap.cpp
-  ensureJavaAllocationReserve();
+  if (enforce_reserve) ensureJavaAllocationReserve();
...
-  const jref_t reference = this->addInstance(packageID, classIndex);
+  // the reserve itself raises NO_RESOURCE; refusing its instance made 6F00
+  const jref_t reference = this->addInstance(packageID, classIndex, false);
```

## 6. Reclaimer that erases live data or dies of the memory it tries to free

Garbage reclamation is the riskiest component: a missed edge deletes live data, and its own
scratch memory can exhaust the heap.

**6a. Live array body erased.** On an 8E campaign the heap walk missed an edge, so the
reclaimer erased the body of an array that a reachable durable alias still pointed to
(`03013894...45`), later failing strict GC reads. Fix: protect any body named by a valid alias
chain under a reachable prefix and fail closed on unreadable chains. Fixed `36fcfe69`
(introduced with the original reclaimer; not pinned).

```cpp
// 36fcfe69 persistent_reclaimer.cpp
// A reachable serialized field alias is itself a durable reference. Keep the
// body if the in-memory graph missed that edge.
bool reachableAliasTargetsBody(const Tag &candidate, Heap &heap,
                               VisitResult &failure) { ... }
```

**6b. The reclaimer panicked on out-of-memory.** The T8 option-1 run on 07E7 died with
"Out of memory" inside the reclaim pass: the SDK `malloc` wrapper panics on every failed
allocation, so the scan's fail-closed `std::bad_alloc` handling never ran. Fix: a
`RecoverableAllocationScope` makes `operator new` throw inside the deletion pass only.
Fixed `9228c583`.

```cpp
// 9228c583 main.cpp
try {
  const jcvm::RecoverableAllocationScope allocation_scope;
  return jcvm::PersistentReclaimer::reclaimUnreachableObjectDeletionBatch(...);
} catch (const std::bad_alloc &) { /* keep the request, retry later */ }
```

## 7. Flash overlay index and compaction losing or resurrecting records

The overlay filesystem (hash table plus tombstones, compacted in place) is VM-integrity
critical (INV-14 area). Bugs were found by physical transaction probes and near-capacity
fake-flash stress tests.

**7a. Stale tombstone hid a newer array body.** After a transaction write, lookup returned
the first matching slot, a legacy parent-slot tombstone, instead of the newest live record, so
a persistent array length read failed on hardware. Fix: prefer the newest live record, reject
deleted parents, stop encoding new tags with parent-slot references. Fixed `b88c9398`.

```cpp
// b88c9398 choupi_pico2.cpp find_overlay
+  OverlayFile *newest_live = nullptr;
+  OverlayFile *newest_tombstone = nullptr;
   ...
-  if (!overlay_is_used(file)) { return nullptr; }
+  if (!overlay_is_used(file)) { break; }   // then pick newest live over tombstone
```

**7b. In-place compaction rehash dropped live records.** Moving slots from an interior point
of a move-path left a duplicate in a destination that a later predecessor overwrote. Found by
a regression with 2,400 records and 32 delete/add rounds; it plausibly explains the older
D92 `F205` failure class. Fix: move paths from sources with no incoming edge first, cycles
last. Introduced `937b0547`; fixed `92c91943`.

```cpp
// 92c91943 pico_filesystem.cpp
// Move open paths from their sources with no incoming edge first, then the
// remaining cycles. Starting at an interior source would leave a duplicate.
for (...) { if (... || (bitmap[start / 8] & (1u << (start % 8))) != 0) continue;
            move_mapping_component(start); }
for (...) { ... move_mapping_component(start); }   // cycles
```

## 8. Transaction rollback that did not really restore or publish

Found by review tasks RV-31 and RV-44 and by tests written to fail first.

**8a. Rollback invalidated fields of objects that were never persisted.** `getFields()`
compared the rollback generation even for objects only in RAM, so an unrelated abort made
their in-memory fields be treated as stale and reloaded (lost). The new test failed before the
fix in both promotion scenarios. Fixed `d78fc7ef` (generation logic from the original VM
import `29ae3d63`).

```cpp
// d78fc7ef jc_instance.cpp getFields
+  // A nonpersistent instance's fields are authoritative in-memory state.
+  if (!this->isPersistent()) {
+    return this->fields;
+  }
```

**8b. Rollback "succeeded" without writing anything on the Pico.** The Pico filesystem
defers `fs_sync()` while a Java Card transaction is active, and rollback ran its final sync
with the transaction flag still set, so the restored state was never published but the code
reported success (the test stub synced immediately, hiding it). Fix: drop the flag for that
one sync, reinstate it and keep the journal if it fails. Fixed `ab8e74f2` (rollback code
from `bd1750c3`, deferral added later; exact introduction not pinned).

```cpp
// ab8e74f2 flashmemory.cpp rollback_persistent_writes
+  persistent_write_transaction_active = false;   // let the one sync really run
   if (fs_sync() != 0) {
+    persistent_write_transaction_active = true;  // keep journal recoverable
     persistent_write_recovery_required = true;
```

## 9. Identity reuse after deletion (stale proxies, recycled owner IDs)

After DELETE and reinstall, handles, owner IDs and proxy routes must not alias the new
object. Both were found by new integration scenarios, not by users.

**9a. Stale Shareable proxy dispatched into the reinstalled service.** After deleting a
service and reinstalling the same AID, a client's cached proxy reached the *new* instance
(`45679000` instead of failing closed). The forget routine only cleared proxies in contexts
that contained the deleted server. Fixed `0c69bdb2`.

```cpp
// 0c69bdb2 context.cpp forgetAppletInstance
+  // A client context can cache a proxy for an applet in a different package.
+  // Invalidate those routes independently of whether this Context owns it.
+  for (auto &instance : this->applet_instances)
+    for (auto &proxy : instance.shareable_proxies)
+      if (proxy.server_aid_length == aid_length && std::equal(...))
+        proxy = ShareableProxyEntry{};
```

**9b. Owner ID reused while the old owner's objects were alive.** A same-process VPCD
delete/reinstall test showed the new applet getting owner 1 again while its package-shared
array was live; a separate allocator in `main_pc.cpp` never called the retention callback.
Fix: owner IDs stay reserved while the heap holds objects of that owner (IDs went `[1,2]` to
`[3,2]`). Fixed `4fa2ec43`. A related crash from mixing roots of separate applet heaps after
DELETE was fixed in `1a074dd9`.

```cpp
// 4fa2ec43 context.cpp
+bool Context::isAppletOwnerRetained(const japplet_ID_t owner) noexcept {
+  for (const auto &instance : this->applet_instances)
+    if (instance.owner == owner &&
+        (instance.aid_length != 0 ||
+         (instance.owner_heap && instance.owner_heap->hasLiveObjects())))
+      return true;
```

## 10. Reset and power-off wired to the wrong lifecycle callback

Card reset and power loss are not an applet `deselect()`: no applet bytecode should run, only
transient state clearing. Review R36 found both transports calling `deselect()`.

**10a. VPCD session.** Power-off, reset and disconnect ran applet `deselect()` through
`clear_or_deselect` (from the session plumbing in `b694037c`). Fixed `0425ad59` with a
selection-forgetting path.

```cpp
// 0425ad59 vpcd_session.cpp
-      clear_or_deselect(nullptr, 0);
+      // Power loss is a card reset boundary. Drop selection without running
+      // applet bytecode; the clear callback wipes transient runtime state.
+      forget_selection();
```

**10b. Physical CCID.** `PC_to_RDR_IccPowerOff` ran applet code while the card was losing
power. Fixed `97fc4ab0`.

```cpp
// 97fc4ab0 ccid_device.cpp
-    (void)choupi_vm_deselect_applet(0);
+    // Power-off is a reset boundary, not an applet lifecycle transition.
+    choupi_vm_clear_selected_applet();
```

## 11. JC305 method-token remaps and key-parameter checks that did not match the spec

Applets compiled for JC 3.0.5 call API methods by token; the embedded API exports them in a
different order, so each interface needs a hand-written remap. Wrong remaps surface as
`6F00` in the wrong method, and were misdiagnosed more than once.

**11a. DESKey `clearKey()` returned `6F00`.** The remap swapped only `getKey/setKey` and left
the inherited `Key` methods in the old order, so JCAlgTest `clearKey()` called the wrong
method for DES 64/128/192. First blamed on persistent flash writes, then fixed by the map.
Introduced `24d1e39d`; fixed `22a6255f`; all three rows pass on hardware.

```cpp
// 22a6255f jc_cp.cpp
-      0, 1, 2, 3, 5, 4};
+      1, 3, 2, 0, 5, 4};   // inherited prefix remapped as well as get/set
```

**11b. HMAC keys, AES MAC and a missing HMACKey token class.** The external Windows run
d7e90a18 reported 18 `ILLEGAL_VALUE`: block-size HMAC keys were rejected (the length is bits
in `KeyBuilder` but bytes in the `LENGTH_HMAC_*_BLOCK_n` constants), AES-192/256 were refused
by the AES MAC, and class token 23 (HMACKey) was missing from the Key remap. Introduced with
the HMAC support `558d0ccf`; fixed `c42fab06`.

```cpp
// c42fab06
-       class_token == 24 || class_token == 28 || class_token == 29) &&
+       class_token == 23 || class_token == 24 || class_token == 28 ||
+       class_token == 29) &&
...
-      : (aes ? aesType && bits == 128
+      : (aes ? aesType && (bits == 128 || bits == 192 || bits == 256)
```

## 12. CAP validation written from a wrong reading of the specification

New fail-closed validation for uploaded CAPs was added from memory of the spec and then
contradicted both the spec and valid packages.

**12a. Extended method header is four bytes, not five.** The bounds code assumed five, so
extended methods were measured wrongly and a report of "three overlong Descriptor counts" was
an artifact of it. The fix cited JCVMS 3.0.5 section 6.10.3 and rescanned 11 deployed CAPs
(978 methods, zero out-of-range). Introduced `9246f283`; fixed `618d1f1a`.

```cpp
// 618d1f1a cap_method_bounds.hpp
-  return (first_header_byte & 0x80u) != 0 ? 5u : 2u;
+  // extended_method_header_info is four bytes in JCVMS 3.0.5 section 6.10.3:
+  // flags/padding, max_stack, nargs, and max_locals. Normal headers are two.
+  return (first_header_byte & 0x80u) != 0 ? 4u : 2u;
```

**12b. Descriptor method offset used the wrong base.** Bounding execution to Descriptor
method bodies compared the bytecode-start offset where the Descriptor stores the method-header
offset, so a valid Descriptor-backed install failed with `6A80` / VM `6F00` in VPCD.
Introduced `12afae6d`; fixed `ca72f14b`.

```cpp
// ca72f14b jc_method.cpp
-  const std::size_t method_stream_offset =
-      static_cast<std::size_t>(new_pc_address - method_begin_address);
+  const std::size_t method_header_offset =
+      static_cast<std::size_t>(method_to_call_address - method_begin_address);
```

## 13. Hidden assumptions about initial values and record layout

Code relied on a value or layout that was never established.

**13a. Zero as "no deadline" for the CCID response retry timer.** A retry deadline of 0 meant
"retry now", but `(int32_t)(now - 0) < 0` is true once `time_us_32()` passes about 35 minutes,
so a freshly queued response waited roughly 35 more minutes. It only appeared in long
campaigns. Fix: an explicit "armed" flag plus tests for long uptime and 32-bit wraparound.
Introduced `0d4b271f`; fixed `aac2481f`.

```cpp
// aac2481f ccid_device.cpp
-  if (static_cast<int32_t>(now - pending_response_retry_after_us) < 0) return;
+  if (choupi::pending_ccid_retry_deadline_waiting(
+          pending_response_retry_deadline_armed, now,
+          pending_response_retry_after_us))
+    return;
```

**13b. Persistent boolean arrays were restored as if they had a clear-event byte.**
Restoration read `array_flash[pos]` that does not exist for persistent arrays (and could read
past a zero-length array's header) with no exact-length check. Fixed `a811c6f3` with an exact
record-length check per element type (introduction not pinned). Related same-day slip:
the array identity counter was written to an invalid filesystem tag namespace (`0x04`),
introduced `2f8cb6a8`, fixed `7cd24906` (`0x02, 0xfe, ...`). Also in this family, a Java
field's implicit zero was not materialized (`MessageDigest.OneShot.inputLength`), seen in the
2026-10-15 record, with its regression test in `8e2861ea`.

```cpp
// a811c6f3 flashmemory.cpp
-    event = static_cast<ClearEvent>(array_flash[pos]);
+    // Persistent arrays have no clear-event byte; their payload starts at 3.
+    event = ClearEvent::None;
...
+  if (length != expected_length) { throw Exceptions::SecurityException; }
```

## 14. Fixed-width limits smaller than the real data

**14a. 96-byte CCID response buffer.** RSA SHA-2 signing "failed with 6F00" on the physical
card and the first suspicion was RSA arithmetic. The response buffer was 96 bytes, too small
for a signature longer than about 94 bytes (RSA keys above 752 bits). Fixed `b9ef7f44` (buffer now 1,024 bytes for 4096-bit
signatures plus status). Introduced with the CCID APDU path (not pinned).

```cpp
// b9ef7f44 ccid_device.cpp
+// A 4096-bit RSA signature is 512 bytes; include room for the APDU status
+constexpr size_t kMaxApduResponse = 1024;
+constexpr size_t kMaxCcidResponse = kMaxApduResponse + 10;
-    uint8_t response[96] = {0x6d, 0x00};
+    uint8_t response[kMaxApduResponse] = {0x6d, 0x00};
```

**14b. Overlay enlarged beyond its 16-bit offsets, and exhaustion reported as `6F00`.**
`90f3f722` grew the overlay arena to 77,824 bytes while `OverlayFile`/`FilesystemRecord`
store offsets in `uint16_t`; the trial gave snapshot tag-reconstruction failures and DELETE
`6581`. In the same step, `fbba9ef6` also mapped filesystem "out of flash" to
`SystemException.NO_RESOURCE` instead of `IOException`, which JCAlgTest saw as `6F00`. The
fix added a compile-time guard.

```cpp
// fbba9ef6 src/choupi_pico2.cpp (and flashmemory.cpp: status 1 -> NO_RESOURCE)
+constexpr size_t kMaxOverlayBytes = UINT16_MAX;
+static_assert(kMaxOverlayBytes <= UINT16_MAX,
+              "overlay offsets must fit their 16-bit representation");
```

## 15. Debug output and test-client mistakes that corrupted the evidence

Evidence only counts from a normal image and a correct client; these slips cost long runs.

**15a. Verbose traces left in the "no-debug" firmware.** The per-APDU `APDU_PROCESS` and
`PERSIST_REF` printouts were compiled in a build presented as the debug-off acceptance image.
A new `tools/test-no-debug-traces` failed before the fix because both strings were in the
ELF. Fixed `ac81707d`.

```cpp
// ac81707d choupi/src/main.cpp
-#ifndef PC_VERSION
+#if !defined(PC_VERSION) && defined(CHOUPI_DEBUG_CCID_APPLET)
   std::printf("CHOUPI:APDU_PROCESS stage=begin\r\n");
```

**15b. Probe sent a four-byte APDU without Le.** The physical RSA-3072 preflight and lifecycle
checks failed before key generation while the simulator, which encoded the command with Le,
passed; the fault was in the test client, not the VM. Fixed `3115ba19`. A neighbouring
incident (`8d73cb7c`): GPPro run with an empty reader variable connected to the live T8
campaign card, deselected the applet and invalidated that 3,391-second run (no code change,
rule added to the knowledge base).

```java
// 3115ba19 (physical client)
-      expect(channel, new CommandAPDU(0x80, 0x21, 0, 0, 0),
+      expect(channel, new CommandAPDU(0x80, 0x21, 0, 0, 256),
```

## 16. Safety limits that lag behind the workload or defeat themselves

A guard sized for the largest operation measured so far fails the moment a bigger one shows
up, and its failure mode is a cryptic status word.

**16a. Applet execution deadline of 600 s versus RSA-4096 key generation** (the requested
case). The top-level deadline came from the 2026-11-04 execution-budget step
(`42512434`), when RSA-3072 (73 s) was the largest key. JCAlgTest generates two RSA-CRT-4096
pairs in one prepare APDU, which took 1,031 s on EE11. At expiry the keygen RNG callback
fails and `genKeyPair` throws `CryptoException.ILLEGAL_USE`, as the Windows be737f0e run
reported for RSA-CRT-4096 `setDP1` and PSS sign. Fixed `5e5f0389`, to 3,600 s, kept below the
4,295 s wrap of the 32-bit microsecond clock.

```cpp
// 5e5f0389 choupi/src/interpretor.cpp
-static constexpr uint32_t kAppletExecutionBudgetUs = 600000000u;
+// One APDU may generate several RSA-4096 key pairs; 600 s expired mid-generation.
+// Stays below the 4,295 s wrap of the 32-bit us clock.
+static constexpr uint32_t kAppletExecutionBudgetUs = 3600000000u;
```

**16b. The same limit, three earlier times.** The non-install applet budget was 15 s, then
60 s, then 120 s: JCAlgTest static `Util_arrayCopy_EEPROM` and `SWAlgs_AES()` (60,497 ms)
returned `6400` on hardware. The step that fixed it also added cooperative CCID/watchdog
service every 4,096 bytecodes without refreshing the deadline, so a long operation does not
drop the reader. Fixed `74a5fd93`, later superseded by `5e5f0389`.

```cpp
// 74a5fd93 interpretor.cpp
-      this->installMethodEntry ? 5000000u : 15000000u;
+      this->installMethodEntry ? 5000000u : 120000000u;
...
+    if ((instruction_count & 0x0fffu) == 0) choupi_vm_cooperative_progress();
```

(Category 5b, the reserve that refused its own exception, is the other requested case and
belongs to the same pattern: a protective limit whose error path needs the resource it
guards.)
