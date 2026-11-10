# CAP load, install and bytecode input-handling review

Date: 2026-11-10. Reviewer: Claude Fable 5.2. Scope: static review of how
untrusted inputs are handled in GlobalPlatform LOAD, INSTALL, the runtime CAP
component resolvers and the bytecode interpreter. The trust boundary is the one
in [CAP_EXECUTION_TRUST_MODEL.md](CAP_EXECUTION_TRUST_MODEL.md): uploaded CAP
bytes (package IDs 56–63) and LOAD/INSTALL APDU fields are hostile even after
SCP03.

Backlog tasks: [CAPIN-01 … CAPIN-12](../TODO.md#cap-input-handling-review--2026-11-10).

## Method and evidence limits

- **Static reading only.** No build, unit test, VPCD run, fuzzing, flash or
  hardware check was performed. No runtime source was changed.
- Line numbers refer to the working tree on 2026-11-10 at root commit
  `eb59b378` plus the uncommitted changes of a concurrent worker session; they
  will drift.
- The review ran as four parallel reading passes (LOAD path, INSTALL and
  resolvers, interpreter core, object/invoke/array bytecodes). The reviewer
  then re-read the cited code for the items marked **verified** below. Items
  marked **pass-reported** come from one reading pass and were not re-read.
- No finding was reproduced. Every "effect" is the expected outcome from the
  code path, and each task must start by writing the failing fixture.

## Verdict

No path was found by which a hostile CAP corrupts VM memory directly. The
operand stack and locals carry per-slot kind and owner tags, opcode and operand
fetches are bounded by the method body, frame metadata is out of band, and
array and static accesses are type- and bounds-checked.

The remaining problems fall into four groups:

1. **Trust-boundary bypasses** (A1–A4): the checks exist but are keyed on the
   wrong thing or skipped for ROM targets.
2. **Fail-open LOAD** (B1–B5): a CAP is stored and enabled before it is
   validated, and several inner references are never validated.
3. **Plain bugs that affect legitimate CAPs** (C1–C6).
4. **Cost and portability** (D1–D2).

## A. Trust-boundary bypasses

### A1. Uploaded CAPs can import the internal native-bridge ROM package — verified

- **Where:** `choupi/src/jc_handlers/jc_import.cpp:134-160`
  (`Import_Handler::getPackageIndex` matches any resident package),
  `src/cap_load.cpp:629-831` (INSTALL preflight has no allow-list),
  `choupi/src/jc_bytecodes/bc_invoke.cpp:815-854`,
  `choupi/src/jc_bytecodes/bc_others.cpp:92`.
- **Input:** a CAP whose Import component lists `A0000000600001` v1.0
  (`fr/gouv/ssi/nativeimpl`) and whose ConstantPool has an external
  StaticMethodref to class 0, token N.
- **Path:** preflight checks only `token < static_method_count`. `invokestatic`
  enters the ROM trampoline (`aload…; sspush idx; impdep1`). The `impdep1`
  policy check looks at the *executing* package, which is now ROM, so it
  passes. The applet therefore chooses which public static native runs and all
  of its arguments, for example `setSensitiveResult*`, `setAPDUBuffer`,
  `setAPDUOutgoing`, `getRemoteObject`, `processRemoteInvoke`.
- **Related:** the generated native dispatcher leaves reference-argument type
  checks as `// TODO: PANIC` (`build/arm/choupi-generated/jni.hpp:678`,
  generator in `choupi/javacard-sdk/rommask/.../JCNativeFunctions.java`). The
  natives that were sampled re-check class and array type themselves, so no
  memory-safety break was found.
- **Not verified:** the bridge package's Export table was not dumped. That the
  package is importable is inferred from its `.jca` and from the AID
  special-case at `bc_invoke.cpp:851-854`.
- **Fix:** an allow-list of importable ROM AIDs (public Java Card and
  GlobalPlatform API only) in `validate_cap_imports`, repeated at runtime
  external resolution (`invokestatic`, `new`, `getstatic`/`putstatic`,
  classref) when the caller is a dynamic package. Replace the dispatcher TODO
  with a throw.
- **Test:** a CAP importing the bridge AID must fail INSTALL with a controlled
  status word, and the next valid APDU must succeed.

### A2. Applets can read and write private fields of JCRE-owned global objects — verified

- **Where:** `choupi/src/jc_bytecodes/bc_object_field.cpp:54-60`
  (`validateDeclaredInstanceType` returns early when the declaring class is
  not in a dynamic package) and `:144-192`; `src/cap_load.cpp:751-775` (the
  loader only checks `token < declared_instance_size`);
  `choupi/src/heap.cpp:603-640`, `:1711`;
  `choupi/src/jc_bytecodes/bc_array_access.cpp:561-588`.
- **Input:** an external `InstanceFieldref {javacard.framework.AID, token 0}`
  and the sequence
  `invokestatic JCSystem.getAID; getfield_a <ref>; sspush 0; bspush X; bastore`.
- **Path:** the receiver is a real `AID`, so the assignability check passes.
  The field holds the global retained byte array (owner `0xFE`), which
  `bastore` accepts. AID objects are cached per AID and handed to Shareable
  servers as `clientAID` (`choupi/src/main.cpp:1590-1598`); ROM `AID.equals`
  compares `this.aid`. A client can rewrite its own AID bytes to impersonate
  another client, or corrupt an AID object obtained through `lookupAID`.
- **Variants (pass-reported):** `putfield_a` can replace the field with another
  global array. `invokespecial` of external `AID.<init>` on the global object
  is accepted (`bc_invoke.cpp:597-760`): there is no receiver-class or
  initialisation check.
- **Fix:** throw `SecurityException` on `getfield`/`putfield` from a dynamic
  package when the receiver is a global or JCRE-owned instance; reject external
  InstanceFieldrefs to ROM packages at load unless a ROM metadata table marks
  the token public or protected; make AID backing arrays read-only to dynamic
  code; in `invokespecial` require the receiver to be assignable to the
  constructor's class and never a global instance.
- **Test:** a CAP that mutates its own AID and then calls a Shareable server;
  expect `SecurityException` and an unchanged `equals` result.

### A3. Package bookkeeping is not unwound together with frames — verified structurally

- **Where:** `choupi/src/stack.hpp:167-179` (`unwindToDepth` pops frames only),
  `choupi/src/context.cpp:1102-1113` (`packagesID` is pushed in
  `Method_Handler::callMethod`, `jc_method.cpp:386`, and popped only by the
  return bytecodes and one `doThrow` path), all `unwindToDepth` call sites in
  `choupi/src/interpretor.cpp`, nested interpreter entries at
  `choupi/src/jni.cpp:1206` and `choupi/src/jc_bytecodes/bc_invoke.cpp:1323`.
- **Input:** a method reached through a nested same-context interpreter run
  (RMI `processRemoteInvoke`) that aborts fatally, for example by overflowing
  its operand stack or running off the end of its body.
- **Path:** frames are unwound to the caller's depth but the stale package
  entry stays on top of `packagesID`. The native returns normally. Each later
  return pops one entry too few, so applet code can resume while
  `getCurrentPackageID()` reports a ROM package. The dynamic-package policy
  (`impdep1`/`impdep2`, instruction-start checks) and constant-pool resolution
  all key on that value.
- **Not verified:** the full exploit chain. The shape of the ROM native stub
  frame and RMI reachability were not traced; nothing was executed.
- **Lesser effect:** every failed top-level APDU leaves list nodes behind in a
  retained context.
- **Fix:** store the package ID in `Frame` and derive the current package from
  the top frame, or make every frame pop also pop `packagesID`. Add an
  invariant check that the two depths agree.
- **Test:** an RMI method that overflows its stack, after which the caller
  executes `impdep1` and must get `SecurityException`.

### A4. Virtual and interface dispatch trust the reference instead of the receiver — pass-reported

- **`invokevirtual`:** `bc_invoke.cpp:478-490` and
  `choupi/src/jc_handlers/jc_class.cpp:614-653`. The declared class supplies
  only `nargs`; the token is then looked up in the receiver's concrete class
  tables. `getPackageMethodOffset` does not require the caller and the class to
  share a package, and it walks into superclasses in other packages with the
  same package token. A `VirtualMethodref {own class, token 0x80|k}` with a ROM
  object as receiver can invoke ROM package-private methods; protected methods
  are reachable the same way through the public table. With an array receiver
  the declared class's method runs with `this` set to the array.
- **`invokeinterface`:** `bc_invoke.cpp:1136-1150`, `:1421-1455`. Null and
  proxy checks look at `TOS - operand nargs`, but the frame is built from the
  method header's `nargs`. Only the Shareable-proxy path compares the two.
  Typed locals and the field-access class check currently stop exploitation.
- **Fix:** require the receiver to be assignable to the reference's class
  (arrays only for `Object`); resolve package tokens only while the class's
  package equals the referencing package; require
  `operand nargs == header nargs`; reject array receivers for interface calls.
  Mirror the checks in the loader where they are static.

## B. Fail-open LOAD and missing structural validation

### B1. LOAD stores and enables a CAP after outer framing checks only — verified

- **Where:** `src/cap_load.cpp:852-906` (`validate_cap_code`), `:1038-1099`
  (`finishLoad`), `:1187` (the only `validate_cap_imports` call).
- **What is checked at LOAD:** component framing, Header magic and AID, Applet
  records.
- **What is accepted and persisted with `9000`:** unknown or duplicate
  components, an unsupported CAP version, malformed Class/ConstantPool/
  Descriptor/StaticField payloads, missing Method or Class components. Full
  `JC_Cap(..., validate_payloads=true)` validation first runs at INSTALL, after
  the slot was written and `enablePackage` committed.
- **Knock-on:** `Import_Handler::getPackageIndex` and
  `cap_package_dependencies::findImporter` (`src/cap_package_dependencies.hpp:33-44`)
  call `getCap()` on every resident slot. One bad stored CAP turns unrelated
  import lookups into internal errors and makes DELETE of other packages return
  `6581`.
- **Libraries:** a library package is never link-validated (R55 of
  [review_20261106.md](review_20261106.md), still open).
- **Also (pass-reported):** the PC store accepts a 65,536-byte CAP that
  `getCap()` then rejects (`flashmemory.cpp:2677`); the INSTALL-for-load AID is
  overwritten by the Header AID without an equality check
  (`cap_load.cpp:1046-1048`); if the enable transaction fails after the store,
  the slot stays written but disabled and blocks reload of that AID until
  DELETE.
- **Fix:** in `finishLoad`, construct `JC_Cap` over the RAM buffer and run the
  import-independent part of the preflight before `choupi_store_cap_package`.
  Run ConstantPool and link validation for every loaded package.

### B2. Null component dereferences reachable from stored CAPs — verified by pass, lines re-read in part

- `choupi/src/jc_handlers/jc_import.cpp:102-106` reads `import->count` when the
  CAP has no Import component. Trigger: a class record with an external
  `super_class_ref` and no Import component; only ConstantPool external tokens
  are screened.
- `choupi/src/jc_handlers/jc_cp.cpp:613`, `:881`, `:914` call
  `getClass()->…` unchecked. Trigger: a library with Header and Export
  (`class_count=1`) but no Class component, plus an importer with an external
  Classref.
- `choupi/src/jc_bytecodes/bc_static.cpp:66-67` uses `getExport()` unchecked.
- **Effect:** segmentation fault on PC VPCD. Device behaviour was not
  established.
- **Fix:** required-component rules at LOAD (Class/Method/ConstantPool whenever
  referenced, Export whenever imported from), plus null guards that throw
  `SecurityException` in the resolvers for CAPs stored by older firmware.

### B3. Class-record references are validated only when a ConstantPool entry names them — pass-reported

- **Unchecked:** `super_class_ref`, implemented-interface references,
  superinterface references, virtual-table method offsets, interface `index[]`
  values, Export `class_offset`. `choupi/src/jc_handlers/jc_cap.cpp:44-104` and
  `:306-394` check framing only.
- **Runtime:** `resolveClassref` (`jc_cp.cpp:881-916`) requires only that a
  10-byte prefix fits. A mid-record offset yields a `jc_cap_class_info` whose
  counts are arbitrary bytes; the table accessors
  (`choupi/src/jc_cap/jc_cap_class.hpp:238-330`) are bounded by those counts,
  not by the component. Expected effect: up to about 1 KiB of over-read past
  the Class component. Hierarchy order is not enforced; cycles are bounded only
  by the 256-step loops.
- **Fix:** build the record-start bitmap once at LOAD and require every class
  reference in every record and every Export `class_offset` to hit a record
  start of the right kind. Require the superclass offset to be lower than the
  class's own. Give `resolveClassref` a full-record bound.

### B4. Control flow and exception tables are never validated ahead of execution — verified

- `cap_instruction_boundaries::validate_control_flow`
  (`src/cap_instruction_boundaries.hpp:206`) is referenced only from
  `tests/unit/cap_instruction_boundaries_test.cpp`.
- Branch targets are checked only when taken, each with a rescan from the
  method start (R16, still open).
- Exception-handler entries are checked lazily in
  `choupi/src/jc_bytecodes/bc_exceptions.cpp:303-365`; one malformed entry
  anywhere in the package aborts all exception dispatch.
- **Fix:** run per-method control-flow validation once at LOAD for every
  enumerated method body, and validate each handler entry then (range inside
  one method, handler at an instruction start, catch index a Classref).

### B5. Descriptor and StaticField consistency gaps — pass-reported

- The Descriptor↔Class cross-check runs only if Class precedes Descriptor in
  the stream and is disabled when `cap_minor != 1`
  (`jc_cap.cpp:636-639`, `:758-766`).
- The StaticField validator does not require
  `reference_count*2 + default + non_default == image_size` nor
  `array_init_count <= reference_count` (`jc_cap.cpp:562-587`);
  `initializeCapStaticArrays` bounds an array-init slot by `image_size`
  (`choupi/src/vm_cap_initialization.cpp:76-79`).
- `jc_cap_static_field_component::default_value_count()` and
  `non_default_values()` compute wrong offsets. They are unused; delete them.
- Descriptor-free CAPs skip declared field-type checks
  (`bc_object_field.cpp:63`); typing becomes first-write-wins. Documented gap,
  self-inflicted confusion only.

## C. Bugs that affect legitimate CAPs

### C1. `itableswitch` reads `low` and `high` as 16-bit — verified

`choupi/src/jc_bytecodes/bc_branching.cpp:1157-1161` uses `getNextShort()` for
both fields. JCVMS defines them as four bytes each, and the boundary decoder
already uses the 11-byte header. Every int table switch computes a wrong range
and reads its jump table four bytes early. Fix: `getNextInt()`; known-answer
test with negative and large case values. Check `ilookupswitch` in the same
test.

### C2. `jsr`/`ret` are wrong in three ways — verified

- The boundary decoder gives opcode `0x71` width 2
  (`src/cap_instruction_boundaries.hpp:63-67`); the executor reads a 16-bit
  operand (`bc_branching.cpp:911`). After any `jsr`, instruction-start checks
  are shifted by one byte: valid code is rejected and a mid-operand target can
  be accepted.
- The offset is held in a `uint16_t` (`:906`), so a backward `jsr` is
  impossible.
- The return address is saved after the branch (`:917-919`), so `ret` returns
  to the subroutine start instead of the instruction after `jsr`.

Fix all three and add a decoder-versus-executor width-consistency test over all
256 opcodes.

### C3. Install parameters reach `Applet.install` incorrectly — verified in part

- `choupi/src/interpretor.cpp:187` casts the length with
  `static_cast<int8_t>`; 128–255 bytes become a negative `bLength`.
  **Verified.**
- The applet receives the raw install-parameter field at `bOffset=0`
  (`src/cap_load.cpp:1151-1160`, `interpretor.cpp:178-187`). JCRE and
  GlobalPlatform specify `Li|instance AID|Lc|control info|La|applet data`; the
  standard idiom `register(bArray, (short)(bOffset+1), bArray[bOffset])`
  would misread it. **Pass-reported; not checked against the supplied fixtures'
  expectations — existing JCAlgTest installs pass, so confirm how they call
  `register` before changing the layout.**
- The privileges LV is accepted at any length and ignored.

### C4. Unbounded recursion terminates the VM — pass-reported

`choupi/src/stack.cpp:124-129` bounds stack words only. A static method with
`nargs=max_locals=max_stack=0` that invokes itself always fits; each call
allocates a frame node and a `packagesID` node. `Context::changePackageID` is
`noexcept` around an allocating `push_front`, so `bad_alloc` ends in
`std::terminate`. This is a concrete trigger for open R43. Fix: a hard maximum
frame depth checked in `push_Frame`; no allocation inside `noexcept`.

### C5. External static-field offset is used without a byte swap — verified

`choupi/src/jc_bytecodes/bc_static.cpp:69` returns `field_offsets[token]` raw,
while `Export_Handler::getExportedStaticFieldOffset`
(`choupi/src/jc_handlers/jc_export.cpp:107-122`) converts with `HTONS` and is
never called. Offset `0x0004` becomes `0x0400`. Related, pass-reported: nothing
stops dynamic code from `putstatic` into ROM package statics.

### C6. Smaller items — pass-reported

- `Stack::getCurrentFrame()` dereferences an empty frame list in the `run()`
  catch paths after `doThrow` popped every frame (`stack.cpp:976`,
  `interpretor.cpp:551-553`, `:586-588`, `:617-619`).
- `run()` is `noexcept`, but the token-entry path at `interpretor.cpp:253-256`
  is outside any `try`.
- `athrow` does not check that the object is a `Throwable`
  (`bc_exceptions.cpp:178-200`).
- `Frame::pop_Value` decrements `tos` before the underflow check
  (`frame.cpp:62-70`).
- Non-caller-stack entry leaves argument slots uncleared and does not require
  the entry method's `nargs` to match (`stack.cpp:150-156`,
  `interpretor.cpp:303-314`).
- Instance AIDs are checked for uniqueness only against other dynamic
  instances, not package, ROM or ISD AIDs (`src/cap_store.cpp:710-734`).
- `runtime_install_method` persists under the registered class's package
  without comparing it with the installing package (`main.cpp:2218-2227`).
- A stray LOAD block calls `reset()` and wipes the session's installed/loaded
  view (`cap_load.cpp:1024-1028`).
- The execution budget is 600 s (`interpretor.cpp:48`) while a comment says
  120 s; there is none on PC (R14).

## D. Cost and portability

### D1. Full CAP revalidation on every `getCap()` — verified

`choupi/src/jc_handlers/flashmemory.cpp:2672-2678` constructs `JC_Cap` with
payload validation on each call (bitmap fills and several Descriptor passes),
from 53 call sites, including once per hierarchy step inside one opcode, where
the execution budget is not polled. `valid_descriptor_static_method_offsets` is
O(ConstantPool entries × Descriptor methods). Method body bounds are re-derived
on every call (`jc_method.cpp:344-369`). An attacker-sized Descriptor maximises
the cost per bytecode. Fix after B1: validate once at LOAD, then keep one
validated view per slot keyed by the slot CRC or generation. Not measured.

### D2. Unaligned `uint16_t` views over packed CAP tables — pass-reported

`JCVMArray<const uint16_t>` over class method tables and export offsets
(`jc_cap_class.hpp:303-311`, `jc_cap_export.hpp:52-58`) hands out `uint16_t&`
at odd addresses. This is undefined behaviour and may trap on Hazard3. Not
checked on hardware.

## Checked and found sound

- SCP03 `verifyCommand` bounds; `read_aid`, `read_lv`, `read_ber_length`; LOAD
  block sequencing and buffer bound; DELETE parsing.
- Outer component framing; Import, Applet, Export, Directory, RefLocation and
  ConstantPool tag/count framing; Class 2.1/2.2 record framing.
- Store write-verify plus CRC: no divergence between the validated RAM buffer
  and the flash image.
- Opcode and operand fetch bounds per method body; all four switches read
  through the bounded PC; exception-handler range and target checks.
- Frame metadata out of band; local index validation including int straddling;
  kind and owner tags on stack and locals (no short-to-reference forgery
  found); `dup_x`/`swap_x`.
- Division, remainder, shift and negation edge cases; rejection of undefined
  opcodes; `impdep1`/`impdep2` blocked while a dynamic package executes.
- Static reference/primitive segregation (`validateStaticSlot`): a
  `putstatic_s` followed by `getstatic_a` on one slot is rejected.
- Array load/store type and bounds checks; `newarray` type and negative count;
  jref decoding and handle-table bounds.
- `register()` AID equality, double-register rejection, rollback when install
  throws or never registers.
- R01, R02, R03 and R60 of [review_20261106.md](review_20261106.md) are fixed.

## Not covered

- `checkcast`/`instanceof` bodies; `aaload`/`aastore` assignability (R27 still
  appears present at `anewarray`); `new` on abstract or interface classes.
- Shareable argument translation; GC stale-handle reuse; heap quotas.
- Most crypto natives' argument validation.
- `pico_filesystem.cpp` slot internals, the transaction layer, GET STATUS.
- PC-only paths in `main_pc.cpp` and `pc_rommask.cpp`.

## Recurring causes

1. **Validation at the point of use instead of at the trust boundary.** LOAD
   stores first; INSTALL and the interpreter validate lazily. Anything not
   reached by the lazy path (libraries, class-record references, untaken
   branches, handler tables) is never validated.
2. **Framing checked, references not.** Validators prove that a table fits in
   its component, then resolvers trust offsets and counts stored inside it.
3. **Security checks that skip ROM targets or key on the executing package.**
   The ROM is the asset being protected, so "not dynamic → return" is the wrong
   default (A1, A2), and "current package" changes as soon as a ROM trampoline
   is entered (A1, A3).
4. **Two implementations of one format.** The instruction-width decoder and
   the executor disagree (C1, C2); a raw table read sits beside a correct
   accessor (C5).
5. **Parallel state updated on different paths.** Frames and `packagesID` (A3).
6. **Optional component pointers dereferenced without a null check** (B2).

These are recorded in `.agent/QKB.md` with the rule to apply.

## Recommended order

1. A1 with B1 and B2: one load-time gate (import allow-list, required
   components, full validation before store) plus resolver null guards.
2. A3 and C4: frame/package coupling and a frame-depth limit.
3. A2 and A4: field and dispatch access checks.
4. C1, C2, C3, C5: small fixes, each with a known-answer test.
5. B3, B4, B5, then D1 and D2.

Every fix needs a malformed-CAP or hostile-bytecode fixture run under PC VPCD
(with ASan where memory safety is the claim) that expects a controlled status
word and a working next APDU, followed by the ARM and RISC-V build gates.
Hostile fixtures are repository-owned test inputs under `tests/fixtures/local`
or generated under `build/`; supplied CAPs stay unchanged.
