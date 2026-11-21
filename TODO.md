# Project TODO

Updated 2026-11-21. Follow [AGENTS.md](AGENTS.md) and [SPEC.md](SPEC.md) for
scope, fixtures and acceptance. Record evidence in [progress.md](progress.md).
Detailed cases and earlier outcomes are in the [previous backlog](.agent/archive/2026-11-21-backlog-compaction/TODO.md)
and its [archive index](.agent/archive/2026-11-21-backlog-compaction/README.md).
Open acceptance criteria in the archived backlog still apply where relevant.
Use [HANDOFF](.agent/HANDOFF.md) for last observations and
[external requests](.agent/EXT_REQUESTS.md) for pending external work.

## Next work

- Review Windows option 1/2 results for `v0.2-rc1` and get user authorization before publishing. See WIN-MAXCRYPTO-RC-V0.2-RC1.
- Finish CAPIN-01/07 LOAD-time validation and the remaining CAPIN-05/06 negative fixtures.
- Clear the ARM warning gate for the pending crypto KAT and streaming digest checks.
- Schedule BOOTSEL and power-loss acceptance when an identified board is available.

## CAP and interpreter safety

### CAPIN-01

- [ ] Finish validation before LOAD publishes a package. A scoped physical pass exists (`8d73cb7c`). Carry forward unresolved loader/linkage cases, dependency chains (RV-20261106-55), and malformed LOAD recovery from the [detailed checklist](.agent/archive/2026-11-21-backlog-compaction/TODO.md#capin-01).

### CAPIN-03

- [ ] Validate frame-owned running-package state across nested calls, exceptions, SIO and returning to the caller. Frame storage is implemented.

### CAPIN-04

- [ ] Verify the implemented 64-frame limit gives a controlled APDU failure and leaves subsequent APDUs usable. Include method-entry failure injection (RV-20261106-43).

### CAPIN-05

- [ ] Add the crafted external ROM InstanceFieldref rejection fixture. Runtime guards and INSTALL rejection are implemented. The crafted constructor test and deferred VPCD regression suites passed (`20261120-2135-capin05-06-receiver-fixtures`, `20261121-1230-vpcd-suites-after-pcscd`).

### CAPIN-06

- [ ] Mirror static dispatch checks in the loader and add package-token and interface `nargs` mismatch fixtures. Receiver checks and mismatch/array fixtures exist.

### CAPIN-07

- [ ] Move class, superclass, superinterface, Export, vtable and interface index checks to LOAD under CAPIN-01. The INSTALL guards and negative fixtures exist through `9840cb02`. Resolve the full-record bound in `resolveClassref` with the SIO SensitiveResult regression covered.

### CAPIN-08

- [ ] Validate branches and handlers at LOAD. Measure branch-boundary rescans (RV-20261106-16) before adding a cache.

### CAPIN-09

- [ ] Finish `itableswitch` operand-width work and its signed-boundary and truncation cases.

### CAPIN-10

- [ ] Align `jsr`/`ret` handling between decoder and executor and verify malformed subroutines fail safely.

### CAPIN-11

- [ ] Finish install-parameter delivery and external static-field offset coverage.

### CAPIN-12

- [ ] Finish consistency, cost and portability follow-ups (B5, C6 and other cases in the detailed checklist).

### RV-20261103-28

- [ ] Finish malformed-bytecode and field-kind acceptance. Keep direct native opcode execution forbidden for uploaded CAPs.

### HARD-01

- [ ] Finish WP9 parser/linkage fuzzing, malformed-input recovery on ARM, interruption checks and private-operation hardening. See the review follow-ups below.

## Storage and resource acceptance

### RESOURCE-01

- [ ] Carry forward interrupted-reclamation recovery under PERSIST-03 and remaining mixed-owner/root safety under PERSIST-01. MEM-01 T8 supplies the completed Linux physical option 1/2 long-run gate.

### RESOURCE-02

- [ ] Reconcile the C1–C5 scoped reuse/replacement and inventory acceptance with T8. Reuse T8 for the long campaign; capture any missing per-case physical evidence, including retained handles and flash deltas. The focused 64-init inventory already plateaus offline.

### STORE-01

- [ ] Verify physical package DELETE compaction over repeated API sweeps, with inventory before and after and live records preserved.

### PERSIST-01

- [ ] Finish reclamation safety acceptance across applet/package-static roots, SIO/foreign roots, aliases, reset and transaction boundaries. Normal builds already enable request-gated deletion.

### PERSIST-02

- [ ] Verify applet, alias, transaction and transient-state recovery after actual USB power removal on Pico 2. Preserve the pending physical checks for RV-20261106-09/10/22/23/24/26/52/53/54 with API-03.

### PERSIST-03

- [ ] Finish physical cross-store INSTALL/DELETE/rollback and interruption recovery on a fresh normal image. Inject cuts around journal and CAP metadata publication, verify fail-closed behavior and post-reboot state, and record flash traffic.

## API, firewall and card management

### API-01

- [ ] Finish current-image physical Security/Crypto sweeps with the supplied API client/CAPs. Classify cumulative memory pressure separately from unsupported setup dependencies.

### API-03

- [ ] Finish physical deploy coverage, Service/JavacardxSec retests and `ALG_SUPPORT_EXTENDED` including the three Cipher aliases. Maintain area/profile results and use the pinned current JCAlgTest CAP/client. Reuse T8 for completed static performance campaign evidence and investigate only outstanding unexpected results.

### HMAC-01

- [ ] Finish any outstanding HMAC lifecycle, vector and extended support coverage. The 18 previously failing HMAC key/AES MAC measurements passed on F42D (`4d0681aa`).

### PROFILE-01

- [ ] Audit advertised API capabilities against actual behavior and tests. Finish physical KATs for newly enabled crypto, the 5 KiB/multipart digest checks and deferred RSA/RSA-CRT 4096 validation. See progress for committed implementations and [external requests](.agent/EXT_REQUESTS.md) for XDH ABI inputs.
- Extended APDUs, T=1 WTX and logical channels remain excluded under INV-09. PROFILE-02 is deferred unless product scope changes.

### CTX-01

- [ ] Finish firewall/SIO/proxy acceptance across foreign-value consumers, GC/reset/DELETE roots, stale owners and physical same-CAP/multipackage lifecycle. Include unchanged upstream Multi CAP coverage where available.

### CTX-02

- [ ] Broaden physical SensitiveResult/SIO validation beyond JCSystem to AID, atomic Util and reset. Cover JNI conversion invalidation (RV-20261106-25).

### CTX-03

- [ ] Finish same-CAP firewall-context and per-instance ownership acceptance across GC, reset, full-registry restore and persistence. Verify sibling CLEAR_ON_DESELECT allocation through SIO and separate-CAP rejection.

### GP-01

- [ ] CAP-REINSTALL-01/02/03: finish physical stale-route, late DELETE failure and changed-version replacement checks. Verify fresh lookup and failed/successful APDUs around reinstall and cold boot.
- [ ] Finish physical GET DATA BER-TLV/value response coverage and cold-power registry checks. Preserve pagination and exact AID/continuation assertions.

## Host and integration

### BOOTSEL-01

- [ ] Finish reliable software entry into BOOTSEL and recovery on a chip-ID-pinned Pico 2, including recovery under load. Follow the physical-token procedure and retain bounded logs.

### HOST-01

- [ ] Own remaining CCID stalls and reader loss. Correlate exact-reader PC/SC state, USB visibility, VM return, filesystem sync and CCID submission/completion before assigning a cause.

### LONGOP-01

- [ ] Track residual Windows long-operation behavior under HOST-01 and the RC request. The historical RSA ×50 failure is no longer a demonstrated Linux T8 blocker. Focused physical ×50 replay passed and T8 completed. Preserve the distinct historical DELETE transport failure and direct-replay limits in the archive.

### JC-RESET-01

- [ ] Keep the focused Signature factory/reader-loss diagnosis under HOST-01. Reproduce only with an identified current image and pinned fixtures before attributing the historical boundary failure to a tuple.

### HOST-02

- [ ] Complete three or more no-console Windows cold-reconnect timings per comparison image, including PnP, reader and ISD timing.

### AC-01

- [ ] Verify Windows PC/SC, GlobalPlatformPro and both packaged profile workflows. Track the current RC request separately.

### AC-02

- [ ] Finish the plan's three consecutive clean-flash physical integration passes with exact image and token evidence. Carry forward the current ARM-only scope and warning gate.

## Open review acceptance

See the [review](docs/review_20261106.md) and [detailed cases](.agent/archive/2026-11-21-backlog-compaction/TODO.md#open-review-acceptance--2026-11-06).

- [ ] **RV-20261106-01 — Contain opcode-fetch exceptions inside interpreter recovery.** Owner: HARD-01 / RV-20261103-28. Source fix and PC VPCD recovery pass: fall-through, truncated operand, and zero-length Descriptor method-body mutations each return a controlled status; manager reselection and subsequent GP GET DATA succeed (`9000`). Keep open for physical acceptance and remaining caller-state coverage. [R01](docs/review_20261106.md#r01--p1-opcode-fetch-failure-escapes-the-interpreters-exception-boundary)

- [ ] **RV-20261106-02 — Correct nested frame unwinding direction.** Owner: CTX-01 / HARD-01. Preserve caller PCs/locals/roots when trimming nonzero-depth stacks; cover Java/ISO errors, allocation failure and timeout through multiple nested callbacks. Historical `pop_back()` versus `emplace_front()` defect; current source now uses front removal. [R02](docs/review_20261106.md#r02--p1-nested-failure-cleanup-removes-callers-instead-of-callees)

- [ ] **RV-20261106-03 — Check local/stack bounds before metadata and pointer access.** Owner: HARD-01. Check complete two-word ranges before mutation; sanitize high local indices at nonzero frame bases and stack-capacity boundaries. Source-confirmed ordering defect; sanitizer reproducer pending. [R03](docs/review_20261106.md#r03--p1-local-metadata-is-indexed-before-the-local-variable-bounds-check)

- [ ] **RV-20261106-05 — Keep persistent field caches unchanged when a write fails.** Owner: PERSIST-03. Fault-inject capacity/allocation/serialization/backend errors for scalar/int/reference stores; catch and read immediately, commit, abort, collect and restore. Prove cache/storage agreement and preserved old roots, including failure before snapshot insertion. [R05](docs/review_20261106.md#r05--p1-failed-persistent-setters-publish-cache-changes-before-storage-succeeds)

- [ ] **RV-20261106-07 — Bound every superclass traversal and reject cycles.** 2026-11-20 (`20261120-2125-rv07-hierarchy-cycle-fixture`): source audit found every superclass walk bounded (64 or 256 levels; interface casts, field resolution, method lookup, instance size, exception classification, RMI, Shareable checks). New crafted-CAP fixture `superclass-self-cycle` (applet class names itself as superclass): LOAD is accepted, INSTALL fails promptly with `6A80`, SecurityException at `new`, about 1 s on the PC VM. Remaining: two-class cycles are covered by the CAPIN-07 lower-offset rule at INSTALL validation; physical timing. Owner: HARD-01. Audit allocation sizing, class casts, Object lookup and field flattening, including checked size accumulation. Test self/two-class cycles and deep chains with bounded same-process recovery on PC and hardware. Hostile-CAP reachability/physical timing remain unmeasured. [R07](docs/review_20261106.md#r07--p1-several-superclass-walks-can-loop-inside-one-native-operation)

- [ ] **RV-20261106-08 — Make all Cipher operational bytes consistent with native session lifetime.** Owner: PROFILE-01 / PERSIST-02. Audit IV, saved IV, buffered input, CTR stream and grown buffers; test update/abort/final, reset and fresh restore and inspect persistent records for retained operational data. Preserve existing transient key-workspace safeguards. [R08](docs/review_20261106.md#r08--p1-cipher-operational-byte-arrays-still-have-persistenttransactional-lifetime)

- [ ] **RV-20261106-20 — Remove ARM cycle/smoke-image ambiguity.** Owner: HOST-01 / AC-02. Align wrapper target names and instructions; require exact full-runtime image/profile/hash and runtime marker in JCVM acceptance. Current `cycle arm` selects the smoke image; `cycle choupi` selects the full VM. [R20](docs/review_20261106.md#r20--p1-the-documented-arm-cycle-flashes-the-smoke-firmware)

- [ ] **RV-20261106-16 — Measure repeated branch-boundary rescans.** Prerequisite of CAPIN-08 (its branch-result cache must wait for this measurement). Owner: HARD-01 / API-03. Compare equivalent loops at increasing method offsets; record device latency and RAM cost before introducing a validated instruction-start cache. Keep boundary checks and package invalidation intact. [R16](docs/review_20261106.md#r16--p2-branch-validation-rescans-from-method-start-on-every-taken-branch)

- [ ] **RV-20261106-17 — Measure handle lookup and worst-case GC scaling.** Owner: PERSIST-01 / API-01 / EMU-01. Include forward/reverse chains, wide graphs and sparse slot tables; record heap size, GC duration, watchdog margin and p95/p99 APDU latency. Current list lookup and repeated mark passes have linear/quadratic scaling risks. [R17](docs/review_20261106.md#r17--p2-stable-handle-lookup-and-gc-have-adverse-scaling)

- [ ] **RV-20261106-18 — Disclose Signature cumulative limits and peak memory.** Owner: PROFILE-01 / API-01. Document the current 32,767-byte buffering ceiling; test multipart boundaries and failed growth. Evaluate incremental native hash/MAC state with correct reset/abort semantics as separate implementation work. [R18](docs/review_20261106.md#r18--p2-signature-update-is-whole-message-buffering-with-a-32767-byte-ceiling)

- [ ] **RV-20261106-19 — Bound service gaps inside long native loops.** Owner: HOST-01 / HARD-01 / API-03. Measure maximum-size persistent Util copy/fill, crypto workspace copies and cleanup under capacity pressure; verify CCID progress, immutable deadline and watchdog margin before choosing callback cadence. [R19](docs/review_20261106.md#r19--p2-lengthy-native-copyfill-loops-lack-cooperative-progressdeadline-checks)

- [ ] **RV-20261106-21 — Synchronize profile/trust statements with current code and limits.** Owner: PROFILE-01 / ARCH-08. Replace blanket multi-applet exclusions with a capability/remaining-gap/evidence matrix; disclose Signature limits and SCP03 subset, state RISC-V as out of scope and separate helper/emulator/hardware evidence. Review R01–R20 remain open until validated. [R21](docs/review_20261106.md#r21--p3-profiletrust-documentation-has-fallen-behind-implemented-behavior)

- [ ] **RV-20261106-34 — Keep explicit flash device identity mandatory through discovery and re-enumeration.** P1; owner: HOST-01 / AC-02. Offline fix is in `tools/lib/flash_usb_target.sh`: absent/duplicate serials fail closed; the selected runtime serial or chip ID is checked after re-enumeration; BOOTSEL transitions require a matching ROM chip ID; the flash log prints the selected path, bus/address, serial and chip ID. Nine synthetic sysfs cases pass, including absent A/sole B, duplicate identities, BOOTSEL ID mismatch, and runtime/BOOTSEL replacement at a stable path. Still open: physical flash verification on an identified Pico with the selected identity logged. [R34](docs/review_20261106.md#r34--p1-an-explicit-flash-serial-can-fall-back-to-a-different-board)

- [ ] **RV-20261106-35 — Pin one reader through every power-up acceptance phase.** P2; owner: HOST-01 / PERSIST-03 / AC-02. Forward the exact reader to the existing APDU probe, record actual selected identity and reject ambiguous fallback. Test opposing registry/APDU outcomes across two readers; retain the honest power-cycle-not-verified marker. [R35](docs/review_20261106.md#r35--p2-the-power-up-acceptance-script-can-combine-evidence-from-different-readers)

- [ ] **RV-20261106-36 — Align physical CCID and VPCD reset/power callback semantics.** P2; owner: CTX-03 / HOST-01 / PERSIST-03. Prevent reset/power-off from invoking applet deselect code. Verify persistent callback counter, reset/deselect transients, transactions and native session cleanup through logical off/on and reader reset on each backend; test supply loss separately. [R36](docs/review_20261106.md#r36--p2-physical-ccid-power-off-executes-an-applet-callback-unlike-vpcd-power-off)

- [ ] **RV-20261106-37 — Make EC cofactor requirements algorithm-specific.** P2; owner: API-03 / PROFILE-01. Allow fully populated EC keys and ECDSA to initialize without optional K; preserve DHC's required-cofactor check and R28's validation of inconsistent supplied K. Cover generated/manual public and private keys, missing/correct/invalid K and non-DHC agreement. [R37](docs/review_20261106.md#r37--p2-ec-initialization-and-ecdsa-require-an-optional-cofactor)

- [ ] **RV-20261106-38 — Honor or reject preinitialized key-pair parameters explicitly.** P2; owner: API-03 / PROFILE-01. Preserve supported RSA exponents and EC domains rather than silently replacing them. Test default/nondefault/invalid exponents and supported/invalid/unsupported EC domains, parameter readback, pair consistency and repeated generation. A named-curve restriction may reject unsupported domains explicitly. [R38](docs/review_20261106.md#r38--p2-key-pair-generation-silently-replaces-preinitialized-parameters)

- [ ] **RV-20261106-39 — Reset CBC MAC IV to zero after finalization.** P2; owner: API-03 / PROFILE-01. Cover sign, successful/false verify, multipart input and explicit reinit on the same object after a nonzero-IV init. Compare first output with that IV and next output with an independent zero-IV reference; default-zero repeated-message tests are insufficient. [R39](docs/review_20261106.md#r39--p2-cbc-mac-finalization-preserves-the-nonzero-iv-for-the-next-message)

- [ ] **RV-20261106-40 — Restore persistent-key crypto initialization across reset.** P2; owner: CTX-03 / PROFILE-01 / PERSIST-02. Separate retained key/mode/configuration from discarded intermediates. Test retained Cipher and Signature objects without reinit after logical reset and cold restore, partial input, zero-IV reset, inactive applets and output-length metadata. Keep cleared transient keys unusable. [R40](docs/review_20261106.md#r40--p2-card-reset-discards-persistent-key-crypto-initialization-instead-of-restoring-it)

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

- [ ] **RV-20261106-55 — Validate loaded-library dependency readiness before applet execution.** Implemented via CAPIN-01 (bounded eight-slot dependency-closure worklist); **acceptance only** (remaining A→B→C cases). P2; owner: GP-01 / PROFILE-01 / HARD-01. Cover A→B→C with missing/wrong-version/present C and C used only inside B's method body. Validate reachable dependencies with bounded traversal or an earlier library linking boundary, preserve supported load order, define cycle handling and demonstrate correction permits INSTALL. Direct-import and deletion guards are insufficient evidence. [R55](docs/review_20261106.md#r55--p2-install-preflight-does-not-validate-the-dependency-closure-of-loaded-libraries)

- [ ] **RV-20261106-56 — Avoid repeated full-store orphan sweeps for already clean free IDs.** P2; owner: STORE-01 / HOST-01 / PERSIST-03. Measure scan counts/manager latency for 0/1/7 free slots and sparse/dense clean/orphaned stores. Batch or cache only with validated recovery invariants; cover interrupted publication and package-ID reuse. Traversal counts are source-inferred; no flash erase count or speedup is claimed. [R56](docs/review_20261106.md#r56--p2-orphan-cleanup-repeats-full-storage-walks-for-ordinary-free-package-slots)

- [ ] **RV-20261106-57 — Define and validate ECDSA with enabled transient EC private keys.** P2; owner: API-03 / PROFILE-01 / CTX-03. Native initialization currently accepts only persistent type 12. Support or disclose types 30/31 explicitly; compare identical complete key material across lifetimes/digests, independent verification, clear-event rejection and reinit. Validate downstream component use; keep optional-K handling separate. [R57](docs/review_20261106.md#r57--p2-ecdsa-initialization-rejects-enabled-transient-ec-private-key-types)

- [ ] **RV-20261106-58 — Implement or disclose the existing-key KeyPair constructor overload.** P2; owner: API-03 / PROFILE-01. Add supported behavior through native binding/VM code, preserving supplied references. Cover initialized/uninitialized compatible RSA/EC pairs, getter identity, mismatched sizes/algorithms, generation and transient-private variants. Current native generation/factory tests cover only algorithm/length construction. [R58](docs/review_20261106.md#r58--p2-the-keypair-constructor-taking-existing-keys-remains-an-unbound-stub)

- [ ] **RV-20261106-59 — Prevent unreachable foreign-array cycles from becoming permanent GC roots.** P2; owner: PERSIST-01 / CTX-01 / HARD-01. Separate registry edges from reachable roots using a validated cross-heap lifetime strategy. Exercise two/three-heap cycles and diamonds with/without real roots, different collection orders, edge breaking, package deletion and handle reuse. Use production Context enumeration and measure reclaimed/live counts plus bounded repeated-work RAM growth while preserving live foreign references and owner collision isolation. [R59](docs/review_20261106.md#r59--p2-foreign-array-root-routing-can-keep-an-unreachable-cross-heap-cycle-alive)

- [ ] **RV-20261106-60 — Audit acceptance of the corrected positive-depth dup_x implementation.** P1; owner: HARD-01 / PROFILE-01 / CTX-01. The RMI stack-helper suite covers all M/N byte combinations, exact values, underflow/overflow recovery, reference-owner metadata, and int pairs; `./tools/test-rmi-registry` passes. The generated VPCD CAP now executes all 24 legal `dup_x` operands through applet INSTALL and passes the constructor depth check; invalid `0x17` reaches interpreter dispatch and is rejected with `vm=6F00` before applet publication. The VPCD interpreter now executes all 24 legal operands and catches all 232 invalid byte values in one APDU; a separate caught-invalid APDU and the next APDU both return `9000`. Interpreter typed cases preserve an APDU reference identity, duplicate a complete int pair, and duplicate an int pair above a short (`dup_x 0x23`). A cross-AID SIO VPCD CAP now sends a client-owned array reference through `dup_x 0x12`, returns it from the service, and verifies identity at the client; the focused probe passes. Remaining: obtain physical acceptance. The latest elevated USB refresh still sees three runtime `2e8a:0009` devices and no ROM `2e8a:000f`, so no physical flash was performed. Use integer-range checks before pointer arithmetic as in R03. [R60](docs/review_20261106.md#r60--p1-positive-depth-dup_x-duplicates-the-wrong-stack-segment)

- [ ] **RV-20261106-12 — Accept 13-byte no-data case-3 SCP03 commands.** Owner: GP-01. Test valid independent MAC vectors with/without Le, malformed lengths and failed-MAC recovery; derive minimum length from supported APDU cases. [R12](docs/review_20261106.md#r12--p2-scp03-rejects-a-valid-no-data-case-3-protected-command)

- [ ] **RV-20261106-13 — Define and enforce a total VPCD frame deadline.** Owner: EMU-01. Cover slow-drip header/payload and EINTR, not just idle/stopped input; record elapsed upper bounds and successful reconnect. Distinguish intentional inactivity timeout if retained. [R13](docs/review_20261106.md#r13--p2-vpcd-read-timeout-is-an-inactivity-timeout-not-a-frame-deadline)

- [ ] **RV-20261106-14 — Apply execution budgets to the PC interpreter.** Owner: EMU-01 / HARD-01. Use monotonic time and preserve nested deadlines; recover an infinite interpreted call in the same process, restore transactions/callers and accept a subsequent command. Killing a timed-out emulator does not close this. [R14](docs/review_20261106.md#r14--p2-the-pc-interpreter-has-no-equivalent-execution-deadline)

- [ ] **RV-20261106-15 — Finish review-harness validation after the allocation-link fix.** Owner: HARD-01 / PERSIST-01. Standalone allocation linkage is resolved by MIG-20261107-01 (nine assertions pass). Remaining: validate/repair the separate review persistence fixture’s CAP dependencies and reference-slot assumptions, then include applicable targets in routine offline checks. [R15](docs/review_20261106.md#r15--p2-allocation-regression-cannot-currently-link)

- [ ] **RV-20261106-27 — Preserve reference-array component type namespaces.** P2; owner: CTX-01 / PROFILE-01 / PERSIST-02. Retain the defining CAP or stable resolved component identity through allocation/persistence; exercise imported-library allocation followed by applet stores/casts using deliberately colliding CP indexes, then restore. [R27](docs/review_20261106.md#r27--p2-reference-array-component-types-lose-their-defining-cap-namespace)

- [ ] **RV-20261106-30 — Prevent exception diagnostics from reading arbitrary field kinds as short.** P2; owner: HARD-01 / PROFILE-01 / CTX-01. Exercise fresh-boot hardware throws of custom exceptions with byte/int/reference/short first fields, exact/superclass catches and a following successful APDU; trace-budget state must not alter behavior. [R30](docs/review_20261106.md#r30--p2-hardware-exception-diagnostics-assume-every-exceptions-first-field-is-a-short)

## Performance

- [ ] **PERF-20261110-05 — Constant-time heap handle lookup.** P2; owner: (2026-11-20: also a RAM item — each `std::list` slot costs a 24 B node per handle ever allocated; an indexed container would save ~16 B per slot, relevant to the option-1 OOM margin.)
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

- [ ] **PERF-20261110-08 — Reduce PC FS write amplification.** P3; owner:
  EMU-01. `FileSystem::edit_at` (`choupi/os/src/fs/mod.rs`) appends a full copy
  of the record for every edit, and `Sector::lock` scans every outstanding
  lock linearly (16-bucket set; FS file set uses 32 buckets). After 02–03
  land, re-measure; only optimize if compaction or lookups still dominate VPCD
  APDU time. Acceptance: native FS bench (patch in the artifact folder above)
  and `fs::` tests pass with before/after numbers.
