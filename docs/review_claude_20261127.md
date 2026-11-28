# JCVM execution review (Claude) — 2026-11-27

Scope: Java Card VM execution on the Pico 2 firmware. This covers:
- the interpreter, frames and operand stack;
- bytecode semantics;
- object, array and field access, and the firewall;
- exceptions;
- how CAP metadata is consumed at LOAD, INSTALL and run time;
- the JCRE lifecycle and transactions;
- the native API surface that applets can reach.

References:
- Java Card 3.0.5 Classic: VM spec (JCVMS), Runtime Environment spec (JCRE) and API.
- Product invariants in `SPEC.md`.
- Threat model from `docs/CAP_EXECUTION_TRUST_MODEL.md`: uploaded CAPs are untrusted, and ROM-mask packages are trusted.

Baseline: `789aa994`.

Method: source inspection only. Nothing was built, and no tests or hardware were run. Each finding was traced through its callers, and the key code was re-read during consolidation.
- **Confirmed** means the faulty code path was read end to end.
- **Plausible** means the mechanism is clear but some link, such as ROM image contents or exact spec wording, was not verified.

Issues already in `docs/review_20261127.md` are not repeated: the 8-entry PIN table, journal replay capacity, and ArrayLogic scratch allocation.

## Summary

| # | Severity | Area | Title | INV |
|---|---|---|---|---|
| 1 | High | Firewall | Objects held in a static field are usable from another context | INV-19 |
| 2 | High | Firewall/persistence | APDU buffer and install `bArray` are ordinary arrays: they can be stored, and stored copies go to flash | INV-13, INV-19 |
| 3 | Medium | CAP linkage | Uploaded code can call and override package-private methods of ROM classes | INV-18 |
| 4 | Medium | Robustness | Unbounded `jsr` return-address lists can exhaust RAM and panic | INV-22, INV-18 |
| 5 | Medium | Bytecode | `itableswitch` reads `low`/`high` as 16-bit values | INV-07 |
| 6 | Medium | Bytecode | `jsr` handling is wrong: backward offsets are not sign-extended, and the boundary decoder gives `jsr` the wrong width | INV-07, INV-18 |
| 7 | Medium | Transactions | `abortTransaction` does not roll back RAM-resident objects | INV-14 |
| 8 | Medium | Transactions | `JCSystem` transactions are unusable inside `install()` | INV-07 |
| 9 | Medium | Firewall | CLEAR_ON_DESELECT arrays are accessible from a non-selected context | INV-19 |
| 10 | Medium | Exceptions | Exceptions thrown in an SIO server reach the client as SecurityException | INV-07, INV-19 |
| 11 | Medium | Crypto | SecureRandom, key generation, ECDSA k and PQC seeds come from `pico_rand`, not a conditioned TRNG or DRBG | INV-04 |
| 12 | Medium | Crypto | Cipher keeps a CLEAR_ON_DESELECT key across deselect, and ignores `clearKey()` | INV-21 |
| 13 | Medium | Crypto | AES-GCM uses an all-zero nonce by default and after every `doFinal` | INV-20 |
| 14 | Medium | Allocation | HMAC `setKey` with a changed length allocates a new persistent array each time and leaves the old key in flash | INV-16, INV-21 |
| 15 | Medium | JCRE dispatch | SELECT by an unknown AID deselects the current applet | INV-07 |
| 16 | Medium | JCRE dispatch | Response data is dropped when SW ≠ 9000, and `00 C0` and card-manager INS values are intercepted while an applet is selected | INV-07 |
| 17 | Medium | Static init | `array_init` static arrays of library and dependency packages are never created | INV-07 |
| 18 | Low–Medium | Build modes | Unconditional exception trace strings in normal ARM builds; the trace budget never closes | INV-24 |
| 19 | Low–Medium | Firewall | Static reference cache is per heap and goes stale across contexts | INV-19 |
| 20 | Low–Medium | Exceptions | SystemException and APDUException are not creatable from framework ROM frames | INV-07 |
| 21 | Low | Exceptions | Uncaught applet subclass of ISOException gives 6F00 | INV-07 |
| 22 | Low | JCRE | Failing `select()` returns the exception's SW, not 6999 | INV-07 |
| 23 | Low | API | APDU length and reason codes, `receiveBytes`, Util null/length checks in ROM paths | INV-07 |
| 24 | Low | Transactions | Exception inside an atomic `Util` method leaves the framework transaction open | INV-14 |
| 25 | Low | Exceptions | Uncaught-exception diagnostics dereference an empty frame list (UB) | INV-22 |
| 26 | Low | Crypto | Smaller crypto conformance and hardening items | INV-07, INV-20 |
| 27 | Low | Hardening | Nothing checks that thrown objects are Throwable; raw library state is restored from transient arrays without range checks | INV-18 |
| 28 | Low | Dead code | `jc_cap_static_field_component` helpers compute wrong offsets | — |

---

## High

### 1. Objects held in a static field are usable from another context — INV-19

**Status:** plausible-high. Every step was traced; it was not reproduced.

**Spec:** JCRE 3.0.5 §6.1.2–6.1.4. An object is owned by the context that was active when it was created. Static fields are package-scoped, but access to the object they reference is still subject to the firewall.

**Mechanism:** the VM records the owner of an object stored in a static as the package of the code that is executing, not the creating context.
- `bc_putstatic_a` maps the "current" owner to `context.getCurrentPackageID()` (`choupi/src/jc_bytecodes/bc_static.cpp:342-350`).
- `Static_Handler::setPersistentRef` treats `owner == static's package` as package-local. It writes an ordinary record with no context owner (`choupi/src/jc_handlers/jc_static.cpp:357-361`).
- On read, `getPersistentRefWithOwner` sets the default owner to the static's package. It returns the "current" owner (0xff) whenever the executing package matches (`jc_static.cpp:298-323`).
- `getPersistentField_ReferenceImpl` then rebuilds the object into the reader's heap (`choupi/src/jc_handlers/flashmemory.cpp:1387-1600`).

**Trigger:**
1. Uploaded library L has `static byte[] s` and an accessor.
2. Applet A (context P) calls L, which runs `s = new byte[n]` and fills it with secrets.
3. Applet B (context Q) calls the same accessor. It receives A's persistent array as a "current" reference, bound to the same flash body, and can read and write it.
4. The same applies to an applet package that exports a public static method touching its own statics.

**Fix:** record the creating context, meaning the applet or group owner, rather than the code package. Enforce the firewall on objects read back from statics. Add an offline multi-context test with a shared library.

### 2. APDU buffer and install `bArray` are ordinary applet arrays — INV-13, INV-19

**Status:** confirmed.

**Spec:** JCRE 3.0.5 §6.2.2. The APDU buffer and `bArray` are global arrays. Storing them in an instance field, a static field or an array component throws SecurityException. The APDU object is a temporary JCRE entry point object (§6.2.1).

**Mechanism:**
- The APDU buffer is created with `heap.addArray(...)` (`choupi/src/main.cpp:2727`), and the APDU object with `heap.addInstance(...)` (`main.cpp:2804`). The install `bArray` is created the same way (`choupi/src/interpretor.cpp:181`).
- None of them gets `setAsGlobalArray()`, which is set only in `Heap::addGlobalArray`/`addGlobalRetainedArray` (`choupi/src/heap.cpp:162-229`). None is registered as a temporary global instance.
- Every store guard keys on `isGlobalArray()`/`isTemporaryGlobalInstance()`: `jc_instance.cpp:704-709`, `jc_array.cpp:~881` and `jc_static.cpp:340`. Their comments assume the APDU buffer is global.

**Effects:**
- `persistentField = apdu.getBuffer();` succeeds. `setPersistentField_Reference` promotes the buffer to a persistent body (`flashmemory.cpp:1797ff`).
- Afterwards, the per-byte copy and zero-fill of every incoming command (`main.cpp:2744-2750`) goes through persistent setters. Command data then goes to flash: up to 1029 bytes per APDU for extended applets, including PINs or keys sent in commands.
- This breaks INV-13, because transient content becomes persistent. It also breaks the flash-write directive in AGENTS.md.
- `JCSystem.isTransient(buffer)` reports NOT_A_TRANSIENT_OBJECT.
- Interoperability: when a client passes its APDU buffer to an SIO server, the server's `pop_Reference` rejects it as foreign-owned (`choupi/src/stack.cpp:391-424`). The spec says any context may access it.

**Fix:** allocate the APDU buffer and `bArray` as global arrays, and the APDU object as a temporary entry point object. Add tests that storing each of them throws SecurityException.

---

## Medium

### 3. Uploaded code can call and override package-private virtual methods of ROM classes — INV-18

**Status:** confirmed for the mechanism. The impact depends on the ROM package tables, which were not dumped.

**Spec:** JCVMS 3.0.5 §4.3.7.6 and §6.8.2. Package-visible method tokens are valid only within their own package.

**Mechanism:**
- `Class_Handler::getPackageMethodOffset` follows `super_class_ref` across package boundaries. When the token is below `package_method_table_base`, or the entry is `0xFFFF`, it calls `cp_handler.setPackage(token.first)` and continues with no same-package check (`choupi/src/jc_handlers/jc_class.cpp:557-595`).
- INSTALL preflight resolves external `VirtualMethodref` through the same resolver and accepts bit 0x80 tokens (`src/cap_load.cpp:964-976`). Internal refs are handled the same way (`cap_load.cpp:768-790`).
- `invokevirtual` never checks package-method visibility (`choupi/src/jc_bytecodes/bc_invoke.cpp:438-551`).

**Trigger:**
- **(a) Call:** an external `VirtualMethodref` to an exported ROM class with token `0x80|t`. For example, `org.globalplatform.PlateformeGlobale` with package-private `addUniquelyRegisteredServiceName` / `removeUniquelyRegisteredServiceName`.
- **(b) Internal ref:** an internal ref on the applet's own class with `package_method_table_base` above the token, so the lookup walks into the ROM superclass.
- **(c) Override:** ROM code calling its own package-private method on an uploaded subclass resolves through the subclass's package table, for example `RMIService.beginSelectionSession()`.

**Effect:** breaks the encapsulation of trusted ROM code. `impdep` remains blocked, because the ROM method runs as ROM code.

**Fix:**
- Reject external `VirtualMethodref` entries with bit 0x80 at LOAD and at run time.
- Stop the package-table walk when it leaves the package that issued the reference.
- Do not use a subclass's package table for a call issued from another package.

### 4. Unbounded `jsr` return-address lists can exhaust RAM and panic — INV-22, INV-18

**Status:** confirmed for the mechanism. The exact panic depth was not measured.

**Mechanism:**
- `Frame::savePC` reuses only slots whose `isUsed` flag is true, which `ret` sets. Otherwise it calls `old_pcs.push_back`, up to 256 entries per frame (`choupi/src/frame.cpp:203-221`).
- A ReturnAddress discarded with `pop` never frees its slot.
- The allocation is a heap `std::list` node outside any `RecoverableAllocationScope`. The ARM allocator panics on failure (`src/recoverable_allocation.cpp:83-87`).
- 64 frames × 256 entries × about 32 bytes is roughly 512 KB, close to all of the SRAM.

**Trigger:** an uploaded method that loops `jsr T; T: pop` 256 times and then recurses (`invokestatic` itself) about 64 levels deep. This needs well under the execution budget.

**Effect:** OOM panic and reset (FAULT) instead of a status word.

**Fix:** cap the total number of saved PCs across frames, throw before allocating, or allocate inside a recoverable scope. A fixed-size per-frame array is simplest.

### 5. `itableswitch` reads `low`/`high` as 16-bit values — INV-07

**Status:** confirmed.

**Spec:** JCVMS 3.0.5 §7.5.x `itableswitch`: `default` is s2, `low` and `high` are s4.

**Code:** `choupi/src/jc_bytecodes/bc_branching.cpp:1157-1161` uses `pc.getNextShort()` for `low` and `high`. The boundary decoder reads them correctly as s32 (`src/cap_instruction_boundaries.hpp:110-116`), so the interpreter and the decoder disagree.

**Effect:** every valid `itableswitch` selects the wrong entry. It either branches wrongly, or fails with SecurityException (uploaded package) or RuntimeException. This is not a memory-safety issue, because reads and targets stay bounds-checked.

**Fix:** read `low` and `high` with a 32-bit fetch, and add a KAT for int switches.

### 6. `jsr` handling is wrong in both the interpreter and the boundary decoder — INV-07, INV-18

**Status:** confirmed.

**Spec:** JCVMS 3.0.5. `jsr` (0x71) has a signed 16-bit branch offset and is 3 bytes long.

**Interpreter:** `bc_jsr` stores the offset in `uint16_t branch` and passes `branch - 3`, which promotes to a positive int (`bc_branching.cpp:906-915`). A backward `jsr` (for example 0xFFF0) becomes +65517 and is rejected as out of range.

**Decoder:** `instruction_width` gives every opcode in `0x60..0x72` width 2, including `jsr` (`src/cap_instruction_boundaries.hpp:59-66`). `validate_control_flow` reads `jsr`'s displacement as s8 (`:214-218`). `pc_t` uses this decoder at run time to check that every taken branch lands on an instruction start in an uploaded package (`choupi/src/jcvm_types/pc_t.hpp:137,154`). After a `jsr`, decoding falls out of step.
- Valid branches to real instruction starts after a `jsr` can be rejected.
- The second offset byte of the `jsr` is accepted as an "instruction start". That weakens the instruction-boundary guarantee claimed in `CAP_EXECUTION_TRUST_MODEL.md` (overlapping instruction streams become possible).

**Fix:** use an `int16_t` offset in `bc_jsr`. Give 0x71 width 3 with an s16 displacement in the decoder. Add decoder unit tests for `jsr`.

### 7. `abortTransaction` does not roll back RAM-resident objects — INV-14

**Status:** confirmed.

**Spec:** JCRE 3.0.5 §7.6. Every persistent update inside a transaction is rolled back. References to objects created in an aborted transaction become null.

**Mechanism:**
- Rollback works only on flash-record snapshots (`flashmemory.cpp:114-160`, `:342-402`).
- Objects created with `new` live in RAM until they are stored persistently. Their setters write RAM directly when `!isPersistent()` (`choupi/src/jc_types/jc_array.cpp:~720`, instance setters).
- No RAM undo log exists.

**Triggers:**
- `byte[] t = new byte[4]; beginTransaction(); t[0]=1; abortTransaction();` leaves `t[0] == 1`.
- `beginTransaction(); o = new X(); field = o; abortTransaction();` rolls back the field, but the local `o` stays usable rather than becoming null.

**Fix:** journal RAM writes to persistent-class objects during a transaction, or promote them on first write in a transaction. Null out references to objects created in the aborted transaction. Per AGENTS.md, measure the flash traffic of the fix.

### 8. `JCSystem` transactions are unusable inside `install()` — INV-07

**Status:** confirmed.

**Code:**
- INSTALL wraps `runtime_install_method` in a runtime-owned `beginPersistentWriteTransaction()` (`src/cap_load.cpp:1497`).
- `beginAppletWriteTransaction` fails while that transaction is active (`flashmemory.cpp:694-697`), so `JCSystem.beginTransaction` throws IN_PROGRESS.
- `abortTransaction`/`commitTransaction` throw NOT_IN_PROGRESS (`choupi/src/jni.cpp:1835-1843`).
- `getTransactionDepth()` returns 1 although the applet started no transaction (`jni.cpp:1845-1847`).

**Effect:** any applet whose `install()` or constructor uses a transaction fails to install, and depth-dependent logic takes the wrong branch.

**Fix:** let the applet nest its transaction inside the runtime's install transaction, for example with an applet-visible depth that excludes the runtime's.

### 9. CLEAR_ON_DESELECT arrays are accessible from a non-selected context — INV-19

**Status:** confirmed.

**Spec:** JCRE 3.0.5 §6.1.6. A CLEAR_ON_DESELECT transient array can be accessed only while the currently selected context is the owner's context. Otherwise access throws SecurityException.

**Code:**
- Only allocation is guarded (`canAllocateDeselectTransientArray`, `choupi/src/jni.cpp:367`).
- No access path checks the clear event against the selected context: `bc_array_access.cpp`, `JC_Array::get*/set*Entry` and `Heap::getArray`.
- The arrays are cleared only on the owner's deselect (`main.cpp:2575`) or on reset.

**Trigger:** client C is selected and calls server S over SIO. S reads and writes its CLEAR_ON_DESELECT data during the call. Data written there survives into S's next selection.

**Fix:** check the clear event against the selected context on array access, or check the owning context in the array accessors.

### 10. Exceptions thrown in an SIO server reach the client as SecurityException — INV-07, INV-19

**Status:** confirmed.

**Spec:** JCRE §6.2.4. Exceptions from a shareable server method propagate to the client. JCRE system exception instances are temporary entry point objects (JCRE EPOs).

**Code:** the nested server run is checked in `bc_invoke.cpp:1433-1453`. Only `status == 0x6f00 && lastUncaughtExceptionReference() != 0` is copied and rethrown. Every other non-9000 status becomes `throw Exceptions::SecurityException`.
- **Native exceptions** (ArithmeticException, NPE, CryptoException…): `dispatchJavaException` finds no handler. `doThrow` throws `UncaughtJavaException`, which the `catch (...) {}` at `interpretor.cpp:583-585` swallows. The run returns 6F00 with uncaught reference 0.
- **ISOException:** `doThrow` throws `ISOExceptionStatusWord` before it checks the boundary (`bc_exceptions.cpp:478-505`). The server run returns the reason SW, which is then mapped to SecurityException.

**Effect:** a client's `catch (ISOException | ArithmeticException | CryptoException)` around an SIO call never runs.

**Fix:** at an exception boundary, propagate every uncaught Java exception, both native-raised and ISOException, as an exception object.

### 11. SecureRandom, key generation, ECDSA k and PQC seeds come from `pico_rand` — INV-04

**Status:** confirmed for the code path. The real-world bias was not measured.

**Code:** `get_rand_32()` is the only source behind:
- `ALG_SECURE_RANDOM` (`native_random.cpp:68-84`);
- the CTR_DRBG entropy callback used by `ALG_PSEUDO_RANDOM` and `ALG_PRESEEDED_DRBG` (`native_random.cpp:154-161`);
- RSA primes and EC private keys (`native_ec_keypair.cpp:90-97`);
- the ECDSA nonce (`native_ecdsa_signature.cpp:122-131`);
- ECDH (`native_ec_key_agreement.cpp:147`);
- ML-KEM/ML-DSA randomness (`native_pqc.cpp:152`);
- the PSS salt (`native_rsa_signature.cpp:458`, `native_rsa_cipher.cpp:86`).

**Why it matters:** `pico_rand` is xoroshiro128** reseeded with raw ROSC samples mixed by splitmix64. It is not a vetted DRBG.
- The firmware already has a SHA-256-conditioned TRNG (`conditionTrng`, `native_random.cpp:88-141`), but only `ALG_TRNG` uses it.
- Bias in the ECDSA k leads to private-key recovery.

**Fix:** seed an mbedTLS CTR_DRBG or HMAC_DRBG from the conditioned TRNG, and use that DRBG for every security-relevant consumer. A deterministic ECDSA nonce per RFC 6979 is an additional option.

### 12. Cipher keeps a CLEAR_ON_DESELECT key across deselect, and ignores `clearKey()` — INV-21

**Status:** confirmed.

**Code:**
- `allocateKeyWorkspace` reuses an existing transient workspace whenever the length matches, regardless of its clear event (`choupi/src/native_cipher.cpp:455-469`). The RSA path does check the event (`native_rsa_cipher.cpp:379`).
- The key is copied into the cipher at `init()` (`native_cipher.cpp:686-697`).
- `update`/`doFinal` check only the workspace marker (`checkKeyWorkspace`, `:471-476`). They never re-check the key object. RSA Cipher behaves the same way.

**Triggers:**
- `init(persistentAES)` then `init(aesTransientDeselect)`: the deselect key lands in a CLEAR_ON_RESET workspace and survives deselect.
- `init(k); k.clearKey(); doFinal(...)` succeeds with the cleared key. The spec requires `CryptoException.UNINITIALIZED_KEY`, and the key material stays in RAM after the applet asked to zeroize it.

**Fix:** reallocate the workspace when the clear event differs. Re-check the key's initialized state, or a key generation counter, in `update` and `doFinal`.

### 13. AES-GCM uses an all-zero nonce by default and after every `doFinal` — INV-20

**Status:** confirmed for the code. Plausible as a spec breach, since the exact `AEADCipher.init(Key, byte)` wording was not checked.

**Code:**
- `initializeGcm` uses `zeroNonce` (12 × 0x00) when no IV is given (`native_cipher.cpp:1013-1028`).
- `processGcm` restarts the context with the zero nonce after `doFinal` (`:1049-1055`), not with the init nonce.

**Effect:** `init(k)` used twice, or two `doFinal` calls without re-init, encrypt under the same (k, nonce). GCM nonce reuse reveals the GHASH key and allows forgery.

**Fix:** require an explicit nonce (ILLEGAL_USE or INVALID_INIT otherwise), and require re-init after `doFinal` in encrypt mode.

### 14. HMAC `setKey` with a changed length allocates a new persistent array each time — INV-16, INV-21

**Status:** confirmed.

**Code:** `native_key.cpp:727-740` (`setKey`) and `:879-890`. When the length differs, a new KEYDATA array is allocated and the old one is dropped without zeroing.

**Trigger:** alternate `setKey` lengths 10, 11, 10, 11… Each call allocates new persistent storage, and the previous key bytes stay in flash until GC.

**Fix:** allocate at `KeyBuilder` capacity once and track the logical length in a field. Zeroize when a buffer is replaced.

### 15. SELECT by an unknown AID deselects the current applet — INV-07

**Status:** confirmed.

**Spec:** JCRE 3.0.5 §4.5. If no applet matches, the SELECT is forwarded to the currently selected applet, which stays selected.

**Code:**
- `src/ccid_device.cpp:711-717` calls `choupi_vm_deselect_applet()` for every SELECT by DF name before looking anything up.
- `:1167-1170` then returns 6A82 with no applet selected.

**Effect:** the current applet's `deselect()` runs, its CLEAR_ON_DESELECT state is wiped, and the following APDUs find no selected applet.

**Fix:** resolve the AID first. Deselect only when another applet, or the ISD, will be selected. Otherwise forward the command to the current applet.

### 16. Response data dropped when SW ≠ 9000; GET RESPONSE and card-manager INS intercepted — INV-07

**Status:** confirmed for the code. The spec reading is strongly plausible.

**Code:**
- Outgoing data is copied only when `status == 0x9000` (`choupi/src/main.cpp:2871-2883`). `setOutgoingAndSend(...)` followed by `ISOException.throwIt(0x61xx/0x6283/0x63Cx)` returns only the SW. That breaks applet-level 61xx chaining (PIV, OpenPGP and GIDS style) and warning SWs that carry data.
- `00 C0` is always intercepted while an applet is selected. When nothing is pending the reply is 6A86, and the command never reaches applets that implement their own GET RESPONSE (`main.cpp:2688-2704`).
- `src/ccid_apdu_route.hpp:60-76` routes `00 CB` and CLA 80/84 with INS 50/82/CA/CB/D8/E4/E6/E8/F2 to the card manager even while an applet is selected (`src/ccid_device.cpp:755-781`). For example, the PIV `GET DATA 00 CB 3F FF` never reaches the applet.

**Fix:**
- Return data with warning and 61xx SWs.
- Forward `00 C0` to the applet when the VM has no pending data.
- Route to the card manager only when the ISD is selected, or when a secure channel addressed to the ISD is active.

### 17. `array_init` static arrays of library and dependency packages are never created — INV-07

**Status:** confirmed.

**Spec:** JCVMS 3.0.5 §6.11, StaticField `array_init`.

**Code:** `initializeCapStaticArrays` (`choupi/src/vm_cap_initialization.cpp:23`) has one caller, `installAndMakeSelectable` (`src/cap_load.cpp:1516`). It initializes only the package being installed. `finishLoad` (`cap_load.cpp:1306-1372`) does not call it.

**Effect:**
- In a LOADed-only library, `static final byte[] T = {...}` reads as null, so code throws NPE.
- An applet package's arrays exist only after its first INSTALL.

**Fix:** initialize the arrays at the end of LOAD, or lazily on first `getstatic` of the package, inside the LOAD write transaction.

---

## Low–Medium / Low

### 18. Unconditional exception trace strings in normal ARM builds — INV-24

**Status:** confirmed.

**Code:** these traces are guarded only by `#ifndef PC_VERSION`:
- `CHOUPI:ATHROW` printf and fflush on every `athrow` (`bc_exceptions.cpp:155-163`), which includes every `ISOException.throwIt`;
- `EXCEPTION_THROW/DISPATCH/CLASS/FRAME/CATCH_REF` (`bc_exceptions.cpp:226-251, 288-297, 351-370`);
- `interpretor.cpp:543-546, 565-569`.

INV-24 states that non-diagnostic images contain no debug strings.

**Budget bug:** `exception_trace_budget` stops incrementing at 8 (`< 8`), but the FRAME and CATCH_REF traces test `<= 8`. After the first 8 exceptions they print on every frame and handler for every exception, permanently.

**Effects:**
- USB-CDC printf on a hot path.
- Possible stalls when the serial port is open but nobody reads it.
- Reference values printed to the serial port.
- Extra CAP lookups, which can throw, run during dispatch.

`tools/test-no-debug-traces` does not grep for these strings.

**Fix:** guard the traces behind the diagnostic switch, and extend the test.

### 19. Static reference cache is per heap and goes stale across contexts — INV-19

**Status:** confirmed for the code path.

**Code:** `Heap::get/setStaticReferenceRoot` keeps a cache per heap (`choupi/src/heap.cpp:1234-1289`). `getPersistentRefWithOwner` reads the cache before flash (`jc_static.cpp:300`). Nothing invalidates the entries of other heaps.

**Trigger:**
1. Context A writes `L.s = x`.
2. Context B writes `L.s = null`.
3. A reads `L.s` and still gets `x`, which also stays a GC root.

**Fix:** keep one cache per package, or invalidate the entry in every heap on write. This interacts with #1.

### 20. SystemException and APDUException are not creatable from framework ROM frames — INV-07

**Status:** plausible. The ROM Import bytes were not inspected.

**Code:** `runtimeExceptionReference` looks up the exception package in the Import component of the current package (`choupi/src/jc_bytecodes/bc_object.cpp:239-268`). `dispatchJavaException` uses it for SystemException and APDUException (`:341-345, 388-392`).
- While a `javacard.framework` ROM frame is executing, that package does not import itself, so the lookup throws. The throw is swallowed at `interpretor.cpp:583`, and the applet receives an uncatchable 6F00.
- **Example:** on a full heap, `new AID(...)` runs `aid = new byte[length]` (AID.java:82). `bc_newarray` converts the failure to SystemException NO_RESOURCE (`bc_array_access.cpp:122-128`), but the applet can never catch it.
- The CryptoException lookup has the same weakness: it requires the applet to import `javacard.security` (`bc_object.cpp:271-301`).

**Fix:** resolve these classes by ROM package index, as UtilException, PINException and TransactionException already are.

### 21. Uncaught applet subclass of ISOException gives 6F00 — INV-07

**Status:** confirmed.

**Code:** the SW mapping in `doThrow` runs only when the thrown object's package is the framework AID (`bc_exceptions.cpp:476-505`). The `docheckcast` subclass test sits inside that check.

**Effect:** an uncaught `class MyErr extends ISOException` thrown from `process()` returns 6F00 instead of its reason.

**Fix:** test assignability to ISOException regardless of the thrower's package.

### 22. Failing `select()` returns the exception's SW, not 6999 — INV-07

**Status:** confirmed for the code. Medium confidence on the spec wording.

**Code:** `choupi_vm_select_applet` returns any non-9000 status unchanged (`choupi/src/main.cpp:2524-2540`). Only `false`, and a transaction left open, map to 6999.

**Spec:** JCRE §4.5 maps a failed selection to 0x6999 whether `select()` returns false or throws.

### 23. APDU and Util API details — INV-07

**Status:** confirmed for the code.
- **Length limit:** `setOutgoingLength` accepts up to 1022 even for applets that do not support extended length (`choupi/src/context.cpp:1061`). The spec gives BAD_LENGTH above 256 for those applets.
- **Reason codes:** native APDUException reasons are forced to ILLEGAL_USE (`bc_object.cpp:405-408`, `interpretor.cpp:573-574`). For example, `setOutgoingNoChaining()` followed by `setOutgoingLength(253)` should give BAD_LENGTH (3).
- **`APDU.receiveBytes()`** always returns 0. It has no state check (ILLEGAL_USE) and no `bOff` check (BUFFER_BOUNDS).
- **`setOutgoing()` with Le absent:** case 1 or 3, from a non-extended applet, throws ILLEGAL_USE (`context.cpp:1049`). The implicit-Le workaround covers only extended applets. Medium confidence on the spec reading.
- **ROM framework Util paths:** `Util.arrayFillNonAtomic(null, …, 0, …)` and `arrayCompare(null, …, 0)` return without NPE, and negative lengths return without AIOOBE. Applet calls are intercepted and checked correctly (`bc_invoke.cpp:914-1100`), so this affects ROM-internal callers only.

### 24. Exception inside an atomic `Util` method leaves the framework transaction open — INV-14

**Status:** plausible.

**Mechanism:**
- ROM `Util.arrayCopy`, `arrayFill` and `setShort` begin a transaction when the depth is 0, with no try/finally around the copy and commit.
- A copy into a persistent array of about 32.7 KB or more exceeds `kMaxCommitCapacity` (`flashmemory.cpp:75,131`), so BUFFER_FULL is thrown from inside the copy.
- If the applet catches it, the transaction stays open. The next `beginTransaction` throws IN_PROGRESS. At the end of `process()` the applet's later writes are rolled back, and 9000 becomes 6F00 (`main.cpp:2910-2922`).

**Related:** `snapshot_persistent_tag` resizes its buffer to the full record before the capacity charge (`flashmemory.cpp:147-151`). This can hit the panic-on-allocation path, which overlaps the existing review item 3.

### 25. Uncaught-exception diagnostics dereference an empty frame list — INV-22

**Status:** confirmed for the path.

**Code:**
- For uncaught exceptions at top level, `doThrow` pops every frame (`bc_exceptions.cpp:263-265`) and then throws.
- The interpreter's catch calls `stack.getCurrentFrame()`, which is `*(frames.begin())` (`choupi/src/stack.cpp:980`), on the empty list (`interpretor.cpp:567-568, 615-617`).

**Effect:** undefined behaviour on every uncaught exception. Today it only produces garbage diagnostics.

**Fix:** guard the call on `frames.empty()`.

### 26. Smaller crypto conformance and hardening items

All confirmed for the code.
- **ECDSA and transient keys:** ECDSA signing rejects transient EC private keys, types 30 and 31 (`native_ecdsa_signature.cpp:297`). KeyAgreement and KeyPair accept them.
- **ISO 9797 M2 unpadding** accepts padding longer than one block (`native_cipher.cpp:842-849`).
- **Failed `doFinal` leaves input buffered:**
  - Symmetric: input is appended before the output-range checks (`native_cipher.cpp:865` vs `:917,923`), so it is prepended to the next call.
  - RSA: after a padding failure the buffer stays full (`native_rsa_cipher.cpp:297-303,667`), and every later `doFinal` fails until re-init.
- **Signature message limit:** RSA and ECDSA Signature buffer the whole message and cap it at 32767 bytes with ILLEGAL_USE (`native_rsa_signature.cpp:177`, `native_ecdsa_signature.cpp:206`, `native_signature.cpp:950`). Each growth step reallocates.
- **Key class checks:** ECDSA and ECDH check only the key's package, not its class (`native_ecdsa_signature.cpp:58-64`, `native_ec_key_agreement.cpp:197-200`). RSA and KeyPair do check the class. Typed field accessors keep this memory-safe.
- **DER lengths:** the ECDSA DER decoder accepts non-minimal long-form lengths (`native_ecdsa_signature.cpp:262-276`), so signatures are malleable but cannot be forged.
- **Timing:** RSA PKCS#1 v1.5 unpadding exits early (`native_rsa_cipher.cpp:296-305`), which leaks timing. INV-20 is best effort.
- **Unwiped secret copies:**
  - symmetric, AEAD and MAC key vectors (`native_cipher.cpp:518,986,1101`, `native_signature.cpp:954-968`);
  - RSA key generation d, p, q, dp, dq and qInv (`native_ec_keypair.cpp:146-151,263-276`);
  - the EC scalar (`:377`);
  - ECDSA `privateBytes` (`native_ecdsa_signature.cpp:384`);
  - the ECDH scalar and shared secret (`native_ec_key_agreement.cpp:207,231,288-312`);
  - the PQC seed when `storeKey` throws (`native_pqc.cpp:708-715`).

### 27. Hardening against untrusted bytecode — INV-18

**Status:** no exploit found; these are defence-in-depth gaps.
- **Throwable check:** `doThrow` does not check that the thrown object is a Throwable; it rejects only null and arrays (`bc_exceptions.cpp:181-187`). A hostile SIO server can make `copySioExceptionToCaller` instantiate any of its classes in the client heap (`bc_invoke.cpp:160-189`). Reference fields are restricted, so no firewall break was found.
- **Raw library state in transient arrays:** CCM, SHA-3, CTR_DRBG and SHAKE restore raw mbedTLS or library structs from transient arrays held in ROM-private fields:
  - CCM (`native_cipher.cpp:1086-1097`);
  - SHA-3 `index` (`native_message_digest.cpp:126-141`);
  - CTR_DRBG `rk_offset` (`native_random.cpp:186-204`);
  - SHAKE `pos` (`native_pqc.cpp:1062-1072`).

  Today uploaded code cannot reach those fields: `resolveInstanceFieldToken` blocks ROM-declared fields (`bc_object_field.cpp:157-165`). If that guard ever fails, a corrupted CCM `q` (> 16) would let mbedTLS write out of bounds on the stack. Range-check these fields after every load.

### 28. `jc_cap_static_field_component` helpers compute wrong offsets

**Status:** dead code today.

**Code:** `choupi/src/jc_cap/jc_cap_static_field.hpp:62-84`.
- The helpers ignore the variable `value[]` bytes of each `array_init` record.
- They use the big-endian count without `NTOHS`.
- Any future caller would read out of bounds.

**Fix:** delete the helpers. The live parsers in `jc_static.cpp` and `vm_cap_initialization.cpp` are correct.

---

## Areas checked that looked correct

- **Operand stack and locals:**
  - Every word carries a kind and an owner tag. A short or int cannot become a reference.
  - `push_Frame` bounds `nargs + max_locals + max_stack` (256 words) and the depth (64).
  - Local indices, including width 2 for ints, are bounds-checked. dup_x/swap_x validate m and n.
- **PC handling:**
  - Every fetch and branch is bounds-checked. Switch tables are bounded, and `low > high` is rejected for `stableswitch`.
  - Falling off the end of a method gives SecurityException. The interpreter is iterative, so there is no native stack recursion per Java call.
- **Arithmetic:**
  - All short and int ops wrap without C++ UB. Divide and remainder by zero, and MIN / -1, are correct.
  - Shift masks, `sushr` on a negative short, conversions, `sinc`/`iinc` signedness and constant sign extension are correct.
- **Arrays and fields:**
  - Index bounds and element types are checked, and size arithmetic uses 32 bits.
  - `aastore` performs the ArrayStore and owner checks.
  - Field resolution checks receiver class compatibility, blocks ROM fields from uploaded code and keeps type tags. The `getfield_a_this` cache key is complete.
- **impdep guard:**
  - It uses the package of the method's own Method component.
  - Method offsets are always resolved in that package. `fr.gouv.ssi.nativeimpl` is not importable by uploaded CAPs.
- **CAP structure:**
  - Superclass and interface walks are depth-capped. Class component tables are bounds-checked, and constant-pool tags and offsets are validated.
  - Package AID collisions resolve to ROM first, and dynamic package IDs are always 56–63.
- **Exception handlers:** ranges and the handler target are validated as instruction boundaries.
- **Transactions, outside the items above:** nesting and not-in-progress errors; auto-abort after `process()`, `select()`, `deselect()` and reset; commit capacity reporting.
- **New objects and transient arrays:** they are zero-initialized, and transient arrays are cleared on deselect and reset.
- **OwnerPIN:** the durable decrement happens before the compare, and the compare is constant-time. (The eight-entry table issue is in the other review.)
- **Native API ranges:**
  - (buf, off, len) checks use 32-bit sums, and every output range is checked before writing.
  - RSA PKCS#1 v1.5 verify re-encodes and compares in constant time. PSS verification is strict.
  - EC points and scalars are validated. MAC and AEAD tag compares are constant-time.
  - Unsupported algorithms throw NO_SUCH_ALGORITHM.

## Not covered

- Internals of `persistent_inventory.cpp` reachability, beyond deferral and fail-closed on an incomplete trace.
- `lookupAID` and `getPreviousContextAID`.
- JCRMI exception paths.
- ROM image contents (package method tables, Import bytes) behind #3 and #20.
- `native_rsa_math.hpp` (CRT fault check, blinding).
- The CRC16 and CRC32 seed and reflection parameters.
- `src/choupi_pico2.cpp`.

None of the findings was reproduced on hardware or in VPCD. Before fixing #1, #2, #4, #6 and #7, add an offline regression test for each one.
