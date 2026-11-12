# Project TODO

Updated: 2026-11-12. Active backlog for `plan.md`. Implementation and acceptance
are separate: keep tasks open while required validation is missing. Follow
`AGENTS.md` for native API boundaries, unchanged fixtures, build/hardware gates
and commits. Use unchanged JCAlgTest **1.8.4 / jc305** and its matching client;
run `./tools/fixture-path --verify` before acceptance. 1.8.2 results are historical.

## Priorities and evidence

1. RESOURCE-01; RESOURCE-02 focused fixes await full-suite/physical acceptance.
2. HOST-02 Windows timings; CTX-01/03 SIO/firewall/context acceptance.
3. STORE-01, PERSIST-01/02/03, API-01/03, GP-01 and AC-01/02 integration gates.
4. HMAC-01 acceptance and offline-only JC-RESET-01 diagnosis.
5. CAPIN-01/02 then CAPIN-03/04; relative priority against resource work remains the user's decision.

Device availability changes: consult the latest [.agent/HANDOFF.md](.agent/HANDOFF.md)
and [.agent/QKB.md](.agent/QKB.md), then refresh authorized USB/PCSC access.
The 2026-11-11 20:10 CET handoff reported all three readers and CPLC verification;
older “no readers” observations below describe individual attempts, not current state.
A serial heartbeat or USB enumeration alone does not establish API/PCSC acceptance.

Full pre-edit text (including existing uncommitted edits), completed tasks,
measurements and detailed acceptance cases are preserved in the
[backup](.agent/archive/2026-11-11-todo-compaction/TODO.md), with a
[SHA-256 manifest](.agent/archive/2026-11-11-todo-compaction/SHA256SUMS) and
[task/status map](.agent/archive/2026-11-11-todo-compaction/MIGRATION.md).
Linked detailed cases remain requirements unless explicitly superseded here;
archive device states and old measurements are historical. Detailed new runs go
in [progress.md](progress.md); reusable diagnoses go in QKB.

## Resource lifecycle

### RESOURCE-02

- [ ] **Accept the implemented C1–C5 leak fixes.** Native buffer reuse, transient identity, bulk writes, rollback-safe identities, missing-body recovery, tag encoding and NO_RESOURCE mapping are implemented. Current 64-init plateau: AES 39 records / 589 bytes; RSA-2048 46 / 1,155; alias control 18 / 433 with one setup-time 4-byte unreachable tag; repeat deltas zero. No pre-fix linear-growth baseline was captured. Saved-state APDU 410 returns `9000`; transaction and multipackage regressions pass. Remaining: unchanged full 1.8.4 suite with inventory sampling under RESOURCE-01 and required physical acceptance. Keep this open until those gates pass.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#resource-02).

### RESOURCE-01

- [ ] **Highest priority: stop long-run NO_RESOURCE failures and establish reset causality.** Follow the [staged plan](docs/resource-01-implementation-plan.md). Focused reuse/replacement regressions pass, but full unchanged JCAlgTest runs still show persistent OutOfFlash/F205. RAM GC alone cannot reclaim durable records. Physical reset causality remains unproven; PC and Pico storage backends differ.
- Normal firmware keeps object deletion disabled; explicit diagnostic builds enable deferred request-gated servicing. Applet/package-static roots, foreign aliases, peer cached statics, shareable proxies and RMI roots have focused host coverage. The shared coordinator limits total erasures across both namespaces to eight per APDU; tests cover 8/8/6 batches. Host fake-flash covers 1,000 cuts; physical interruption/timing acceptance is open.
- Successful persistent-array restoration traces are now compiled only with `PICO2_DEBUG_PERSISTENT_GC=1`; failure diagnostics remain enabled. The RP2350 fake-flash suite passes `fail=0` with 134 output lines after the change. This reduces log/flush overhead, but runtime performance improvement has not been measured.
- 2026-11-12: Added a fail-closed guard against republishing an alias from a retained array handle when its persistent body tag is missing. A host regression reproduces the stale-handle sequence; persistent-transaction, object-deletion VPCD, persistent-inventory, RP2350 overlay, and ARM/RISC-V max_crypto builds pass. This prevents a dangling alias but does not establish who removed the body observed in the physical trace; long-run and physical acceptance remain open.
- Next: finish complete root and safety coverage with PERSIST-01/02/03; repeat unchanged 1.8.4/jc305 reset/free-cache workloads with inventory and allocator telemetry. Investigate residual full-suite failures after the host compaction optimization (PERF-01/02); old 40–100-second compaction observations are historical, not current timing.
- 2026-11-12 physical retry: the guest enumerated three attached devices at USB paths `1-3`, `1-4`, and `1-5`, each `2e8a:0009` runtime. The approved PC/SC helper found zero readers; picotool found no accessible BOOTSEL device/reset interface. The bounded repository flash wrapper tried all three and none entered BOOTSEL, so no image was written. Resume only after passthrough exposes `2e8a:000f` or a named reader; then use the saved candidate-trace image documented in `.agent/HANDOFF.md`.
- 2026-11-12 trace analysis: after the first missing durable array-body read, 277 heap-GC summaries show zero handle reclamation (276 with an edge failure); totals grow from 118 arrays / 22 instances at sequence 6 to 365 / 185 at sequence 291. First F205 is at sequence 87. The incomplete persistent walk therefore explains continued handle growth, but the missing body's deletion source remains unproven.
- Acceptance: same-key reuse, key replacement/deletion and cache resets plateau in retained RAM, handles and persistent records. A request before cache clearing must reclaim the newly unreachable graph before the next `process()` call while preserving live aliases/other owners. Demonstrate interrupted-reclamation recovery. Complete long physical runs without NO_RESOURCE, FAULT, unexpected reset or transport loss; record duration, iterations and high-water marks. Run options 2 and 1 on separate identified tokens when possible, with per-device telemetry. Coordinate STORE-01, API-01/03 and offline-only JC-RESET-01; do not enable production reclamation before safety acceptance.

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
  succeeds, the multipackage VPCD regression and the unchanged JCAlgTest 1.8.4
  install still pass, and ARM and RISC-V builds pass. Physical validation is
  separate and stays open until run.
- One CAPIN item per commit series; do not combine with RESOURCE work.


### CAPIN-01

- [ ] **P1: Validate a CAP completely before LOAD stores and enables it; add
  resolver null guards.** Findings B1, B2; extends R55.
  - Progress 2026-11-12: unknown component tags now fail during LOAD before
    package storage; import lookups validate missing/truncated Import records,
    and class/static resolvers guard absent Export/Class components. ARM and
    RISC-V builds plus malformed-CAP, unchanged JCAlgTest install/select, and
    CAP upgrade VPCD tests pass. Still open: conditional required-component
    rules, library Import/Export dependency validation, external superclass
    without Import fixture, install-AID mismatch fixture, and physical test.
  1. In `CapLoadSession::finishLoad` (`src/cap_load.cpp`), construct
     `JC_Cap` with payload validation over the RAM load buffer and run the
     import-independent part of `validate_cap_imports` before
     `choupi_store_cap_package`. Reject unknown or duplicate components,
     unsupported CAP versions and lengths above 65,535 on both backends.
  2. Required-component rules: Class, Method and ConstantPool whenever
     referenced; Import whenever any class record or ConstantPool entry is
     external; Export whenever the package can be imported from.
  3. Run ConstantPool and link validation for library packages too (at LOAD, or
     over the dependency closure at INSTALL).
  4. Null guards that throw `SecurityException` at
     `jc_import.cpp:102-106`, `jc_cp.cpp:613/881/914`, `bc_static.cpp:66` for
     CAPs stored by older firmware.
  5. Compare the INSTALL-for-load AID with the Header AID; decide and record
     what happens to a slot whose enable transaction failed after the store.
  Fixtures: extra tag-12 component; library with Export but no Class plus an
  importer; class with external superclass and no Import. Expect `6A80` at
  LOAD, no slot consumed, and DELETE of an unrelated package still `9000`.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-01).

### CAPIN-02

- [ ] **P1: Allow-list the ROM packages an uploaded CAP may import.** Finding
  A1. Add the list of public Java Card / GlobalPlatform API AIDs and enforce it
  in `validate_cap_imports` and again at runtime external resolution
  (`invokestatic`, `new`, static field and classref resolution) when the
  caller is a dynamic package. The native bridge `A0000000600001` and other
  internal ROM packages must be refused. Replace the `// TODO: PANIC`
  reference-argument path in the generated native dispatcher with a throw.
  First dump the bridge package's Export table and record whether it is
  importable today (not verified by the review). Fixture: a CAP importing the
  bridge AID with an external StaticMethodref must fail INSTALL.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-02).

### CAPIN-03

- [ ] **P1: Keep the executing-package record in step with frames.** Finding
  A3. Store the package ID in `Frame` and derive `getCurrentPackageID()` from
  the top frame, or pop `packagesID` on every frame pop including
  `Stack::unwindToDepth` and all `doThrow` paths. Add an invariant check that
  both depths agree at interpreter entry and exit. Fixture: a nested
  same-context run (RMI `processRemoteInvoke`) whose method overflows its
  operand stack; afterwards the caller's `impdep1` must raise
  `SecurityException` and a retained context must not grow per failed APDU.
  The exploit chain was not traced end to end; record what the fixture shows.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#capin-03).

### CAPIN-04

- [ ] **P1: Bound frame depth; no allocation inside `noexcept`.** Finding C4,
  trigger for R43. Add a hard maximum frame depth in `Stack::push_Frame` that
  raises a controlled stack-overflow error; remove `noexcept` from
  `Context::changePackageID` or make it non-allocating (resolved by CAPIN-03 if
  the package moves into `Frame`). Fixture: static method with
  `nargs=max_locals=max_stack=0` that invokes itself; expect a status word, not
  process termination or reset.

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

- [ ] **P2: Validate control flow and exception tables once at LOAD.** Finding
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
    confirm on the RISC-V build.

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
- Remaining: physical multi-owner/foreign-alias inventory, real per-record interruption and restart/retry, complete retained-context/native-root audit, stable handles and physical APDU timing. Acceptance: all live roots/aliases and other owners survive; interrupted batches resume safely; incomplete reachability never permits deletion. Coordinate PERSIST-02/03; normal builds continue to report object deletion unsupported.

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
  Reproduce offline with the unchanged pinned JCAlgTest 1.8.4 / jc305 fixture
  and matching client on a fresh VPCD image; add focused repeated factory-call
  and resource-accounting probes if needed. Capture APDU progress, SW/VM fault,
  allocator/persistent capacity, USB/CCID reset and host reader state to
  distinguish causes. **Do not test with real tokens or perform physical
  reproduction under this task.**
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

- [ ] **Resolve JCAlgTest 1.8.4 static-performance cases that could not be measured.**
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
  Remaining: rerun the complete unchanged 1.8.4 static suite on the combined
  final image. The attempted final run did not start because authorized host
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

- [ ] **Diagnose recurring physical USB/PCSC reader disappearance.**
  Status: reader loss has occurred after flashes, long APDUs and reconnects;
  an absent guest USB device is distinct from visible USB with no PCSC reader,
  and from an APDU returning a card error. No common cause is established.
  Remaining: capture exact board/image, USB visibility, PCSC registration,
  firmware APDU telemetry and bounded failure/recovery evidence per layer.
  Acceptance: identify a reproducible cause/fix or a concrete host limitation;
  never attribute absent enumeration to crypto or a reader-count firmware limit.
  Follow AGENTS host-access rules from the first USB/PCSC operation, use exact
  current reader names, and fail over only to another identified available board.
  Evidence: QKB USB/PCSC records. Old available/empty-card states are not current.

[Detailed cases and prior evidence](.agent/archive/2026-11-11-todo-compaction/TODO.md#host-01).

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
  Remaining: for each pass build both architectures, verify the intended flashed
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

- [ ] **RV-20261106-16 — Measure repeated branch-boundary rescans.** Owner: HARD-01 / API-03. Compare equivalent loops at increasing method offsets; record device latency and RAM cost before introducing a validated instruction-start cache. Keep boundary checks and package invalidation intact. [R16](docs/review_20261106.md#r16--p2-branch-validation-rescans-from-method-start-on-every-taken-branch)
- [ ] **RV-20261106-17 — Measure handle lookup and worst-case GC scaling.** Owner: PERSIST-01 / API-01 / EMU-01. Include forward/reverse chains, wide graphs and sparse slot tables; record heap size, GC duration, watchdog margin and p95/p99 APDU latency. Current list lookup and repeated mark passes have linear/quadratic scaling risks. [R17](docs/review_20261106.md#r17--p2-stable-handle-lookup-and-gc-have-adverse-scaling)
- [ ] **RV-20261106-18 — Disclose Signature cumulative limits and peak memory.** Owner: PROFILE-01 / API-01. Document the current 32,767-byte buffering ceiling; test multipart boundaries and failed growth. Evaluate incremental native hash/MAC state with correct reset/abort semantics as separate implementation work. Preserve the already disclosed MessageDigest limit. [R18](docs/review_20261106.md#r18--p2-signature-update-is-whole-message-buffering-with-a-32767-byte-ceiling)
- [ ] **RV-20261106-19 — Bound service gaps inside long native loops.** Owner: HOST-01 / HARD-01 / API-03. Measure maximum-size persistent Util copy/fill, crypto workspace copies and cleanup under capacity pressure; verify CCID progress, immutable deadline and watchdog margin before choosing callback cadence. [R19](docs/review_20261106.md#r19--p2-lengthy-native-copyfill-loops-lack-cooperative-progressdeadline-checks)

- [ ] **RV-20261106-21 — Synchronize profile/trust statements with current code and limits.** Owner: PROFILE-01 / ARCH-08. Replace blanket multi-applet exclusions with a capability/remaining-gap/evidence matrix; disclose Signature limits and SCP03 subset, preserve RISC-V build-only distinction and separate helper/emulator/hardware evidence. Review R01–R20 remain open until validated. [R21](docs/review_20261106.md#r21--p3-profiletrust-documentation-has-fallen-behind-implemented-behavior)

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
- [ ] **RV-20261106-43 — Contain allocation failure during package-stack insertion at method entry.** P1; owner: HARD-01 / CTX-01 / EMU-01. Remove allocating-noexcept termination and make frame/package entry coherent on failure. Inject failure after frame creation, verify retained callers/packages and a following APDU; audit analogous noexcept allocation paths. Measure per-call allocation and method-heavy latency before choosing storage changes. [R43](docs/review_20261106.md#r43--p1-method-entry-package-bookkeeping-can-terminate-on-allocation-failure)

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

- [ ] **RV-20261106-55 — Validate loaded-library dependency readiness before applet execution.** P2; owner: GP-01 / PROFILE-01 / HARD-01. Cover A→B→C with missing/wrong-version/present C and C used only inside B's method body. Validate reachable dependencies with bounded traversal or an earlier library linking boundary, preserve supported load order, define cycle handling and demonstrate correction permits INSTALL. Direct-import and deletion guards are insufficient evidence. [R55](docs/review_20261106.md#r55--p2-install-preflight-does-not-validate-the-dependency-closure-of-loaded-libraries)
- [ ] **RV-20261106-56 — Avoid repeated full-store orphan sweeps for already clean free IDs.** P2; owner: STORE-01 / HOST-01 / PERSIST-03. Measure scan counts/manager latency for 0/1/7 free slots and sparse/dense clean/orphaned stores. Batch or cache only with validated recovery invariants; cover interrupted publication and package-ID reuse. Traversal counts are source-inferred; no flash erase count or speedup is claimed. [R56](docs/review_20261106.md#r56--p2-orphan-cleanup-repeats-full-storage-walks-for-ordinary-free-package-slots)

Extend RV-20261106-29's allocation-failure resource audit to RSA CRT modulus/exponentiation helpers in `native_rsa_math.hpp`, including vector failures while raw MPIs are live. This is the existing exception-cleanup issue, not another task.

- [ ] **RV-20261106-57 — Define and validate ECDSA with enabled transient EC private keys.** P2; owner: API-03 / PROFILE-01 / CTX-03. Native initialization currently accepts only persistent type 12. Support or disclose types 30/31 explicitly; compare identical complete key material across lifetimes/digests, independent verification, clear-event rejection and reinit. Validate downstream component use; keep optional-K handling separate. [R57](docs/review_20261106.md#r57--p2-ecdsa-initialization-rejects-enabled-transient-ec-private-key-types)
- [ ] **RV-20261106-58 — Implement or disclose the existing-key KeyPair constructor overload.** P2; owner: API-03 / PROFILE-01. Add supported behavior through native binding/VM code, preserving supplied references. Cover initialized/uninitialized compatible RSA/EC pairs, getter identity, mismatched sizes/algorithms, generation and transient-private variants. Current native generation/factory tests cover only algorithm/length construction. [R58](docs/review_20261106.md#r58--p2-the-keypair-constructor-taking-existing-keys-remains-an-unbound-stub)

Extend RV-20261106-28's EC matrix to allocatable versus operational key sizes/domains, including enabled 320/512-bit key storage with no native named group. Disclose storage-only cases rather than inferring full algorithm support from key construction.

The report and additional task list now contain 59 matching entries. The coordinated collector now traces retained package heaps from non-foreign roots, follows foreign array/instance edges only from marked source objects, and sweeps after a fixed point. Focused tests pass for two- and three-heap cycles, rooted chains/diamonds, collection order, eight repeated explicit-heap cycles with stable slot count, fail-closed ambiguous package ownership, and production `collectRetainedHeaps()` handling peer cached-static/shareable-proxy roots plus eight repeated four-context diamond cycles with a stable four-slot count. Production entrypoint tests also cover foreign volatile instance cycles, fail-closed duplicate package contexts, and a reachable foreign edge whose target context is omitted from the collection request; both heaps remain intact on the incomplete trace. Firmware builds pass with object deletion enabled for both architectures. Keep R59 open for physical reset/deletion validation, broader runtime-context edge cases, and bounded actual RAM measurement.

- [ ] **RV-20261106-59 — Prevent unreachable foreign-array cycles from becoming permanent GC roots.** P2; owner: PERSIST-01 / CTX-01 / HARD-01. Separate registry edges from reachable roots using a validated cross-heap lifetime strategy. Exercise two/three-heap cycles and diamonds with/without real roots, different collection orders, edge breaking, package deletion and handle reuse. Use production Context enumeration and measure reclaimed/live counts plus bounded repeated-work RAM growth while preserving live foreign references and owner collision isolation. [R59](docs/review_20261106.md#r59--p2-foreign-array-root-routing-can-keep-an-unreachable-cross-heap-cycle-alive)

Extend RV-20261106-17's GC measurements to incoming foreign-array registry traversal. Existing overwrite/delete helper passes do not establish collection of graphs whose only reachability is cyclic foreign edges.

- [ ] **RV-20261106-60 — Audit acceptance of the corrected positive-depth dup_x implementation.** P1; owner: HARD-01 / PROFILE-01 / CTX-01. The RMI stack-helper suite covers all M/N byte combinations, exact values, underflow/overflow recovery, reference-owner metadata, and int pairs; `./tools/test-rmi-registry` passes. The generated VPCD CAP now executes all 24 legal `dup_x` operands through applet INSTALL and passes the constructor depth check; invalid `0x17` reaches interpreter dispatch and is rejected with `vm=6F00` before applet publication. Remaining: exercise invalid and typed edge cases plus caught-failure recovery/following APDU through the interpreter, and obtain physical acceptance. The latest BOOTSEL refresh still sees three runtime `2e8a:0009` devices and no ROM `2e8a:000f`, so no physical flash was performed. Use integer-range checks before pointer arithmetic as in R03. [R60](docs/review_20261106.md#r60--p1-positive-depth-dup_x-duplicates-the-wrong-stack-segment)

- [ ] **RV-20261106-12 — Accept 13-byte no-data case-3 SCP03 commands.** Owner: GP-01. Test valid independent MAC vectors with/without Le, malformed lengths and failed-MAC recovery; derive minimum length from supported APDU cases. [R12](docs/review_20261106.md#r12--p2-scp03-rejects-a-valid-no-data-case-3-protected-command)

- [ ] **RV-20261106-13 — Define and enforce a total VPCD frame deadline.** Owner: EMU-01. Cover slow-drip header/payload and EINTR, not just idle/stopped input; record elapsed upper bounds and successful reconnect. Distinguish intentional inactivity timeout if retained. [R13](docs/review_20261106.md#r13--p2-vpcd-read-timeout-is-an-inactivity-timeout-not-a-frame-deadline)

- [ ] **RV-20261106-14 — Apply execution budgets to the PC interpreter.** Owner: EMU-01 / HARD-01. Use monotonic time and preserve nested deadlines; recover an infinite interpreted call in the same process, restore transactions/callers and accept a subsequent command. Killing a timed-out emulator does not close this. [R14](docs/review_20261106.md#r14--p2-the-pc-interpreter-has-no-equivalent-execution-deadline)

- [ ] **RV-20261106-15 — Finish review-harness validation after the allocation-link fix.** Owner: HARD-01 / PERSIST-01. Standalone allocation linkage is resolved by MIG-20261107-01 (nine assertions pass). Remaining: validate/repair the separate review persistence fixture’s CAP dependencies and reference-slot assumptions, then include applicable targets in routine offline checks. [R15](docs/review_20261106.md#r15--p2-allocation-regression-cannot-currently-link)

- [ ] **RV-20261106-27 — Preserve reference-array component type namespaces.** P2; owner: CTX-01 / PROFILE-01 / PERSIST-02. Retain the defining CAP or stable resolved component identity through allocation/persistence; exercise imported-library allocation followed by applet stores/casts using deliberately colliding CP indexes, then restore. [R27](docs/review_20261106.md#r27--p2-reference-array-component-types-lose-their-defining-cap-namespace)

- [ ] **RV-20261106-30 — Prevent exception diagnostics from reading arbitrary field kinds as short.** P2; owner: HARD-01 / PROFILE-01 / CTX-01. Exercise fresh-boot hardware throws of custom exceptions with byte/int/reference/short first fields, exact/superclass catches and a following successful APDU; trace-budget state must not alter behavior. [R30](docs/review_20261106.md#r30--p2-hardware-exception-diagnostics-assume-every-exceptions-first-field-is-a-short)

- [x] **RV-20261106-31 — Preserve new instance fields when first persisted after an unrelated rollback.** P1; owner: PERSIST-02/03. `getFields()` now returns authoritative nonpersistent fields before consulting the persistent rollback generation. A production-handler regression first reproduced the loss (2 failures), then passed with a real rollback, pre-rollback and post-rollback instances, scalar/reference fields, immediate and fresh-heap value/identity checks, and failed-promotion retry. ARM and RISC-V `max_crypto` wrappers pass. Physical validation remains under the PERSIST-02/03 carry-forward gate because the VM did not expose BOOTSEL. [R31](docs/review_20261106.md#r31--p1-an-earlier-rollback-can-erase-a-new-objects-fields-on-its-first-persistence)

## Physical carry-forward from scoped completed fixes

- [ ] **API-03 / PERSIST-02: retain outstanding physical gates from RV-20261106-09/10/22/23/24/26/52/53/54.** Software checks are completed and archived; confirm current-image Cipher padding/reuse, object identity/rollback/retry, boolean restore, transient allocation/activity and resource reasons as applicable. Do not reopen the implemented fixes as missing features. Detailed original scope and evidence remain in the backup.

## Remaining performance work

PERF-20261110-01–04 are completed with measurements in the backup and progress;
metadata caching reduced filesystem reads but showed no physical latency gain.
Unmeasured costs below remain hypotheses. Keep each optimization a separately
validated/measured step. PERF-05 overlaps RV-20261106-17; PERF-07 overlaps PERSIST-01/03.

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
  (ARM and RISC-V); full CHOUPI cycle `TEST:SUMMARY` passes.
- [ ] **PERF-20261110-08 — Reduce PC FS write amplification.** P3; owner:
  EMU-01. `FileSystem::edit_at` (`choupi/os/src/fs/mod.rs`) appends a full copy
  of the record for every edit, and `Sector::lock` scans every outstanding
  lock linearly (16-bucket set; FS file set uses 32 buckets). After 02–03
  land, re-measure; only optimize if compaction or lookups still dominate VPCD
  APDU time. Acceptance: native FS bench (patch in the artifact folder above)
  and `fs::` tests pass with before/after numbers.
