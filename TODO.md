# Project TODO

Updated 2026-11-27. Follow [AGENTS.md](AGENTS.md) and [SPEC.md](SPEC.md) for
scope, fixtures and acceptance. Record evidence in [progress.md](progress.md).
Detailed cases and earlier outcomes are in the [previous backlog](.agent/archive/2026-11-21-backlog-compaction/TODO.md)
and its [archive index](.agent/archive/2026-11-21-backlog-compaction/README.md).
Open acceptance criteria in the archived backlog still apply where relevant.
See the [task dispositions](.agent/archive/2026-11-26-task-dispositions.md)
for completed substeps and their evidence.
Use [HANDOFF](.agent/HANDOFF.md) for last observations and
[external requests](.agent/EXT_REQUESTS.md) for pending external work.

## Priorities (assessed 2026-11-27)

Context: v0.2 is published. Current work targets **v0.3-rc\*** (`v0.3-rc1` =
`b4adbe13`). Pick the highest open item in the order below. Items within a
tier are also roughly ordered. Check `.agent/HANDOFF.md` and live processes
first, because another session may already hold an item.

Ranking rules: (1) a source-level defect against a SPEC invariant comes before
acceptance-only work. (2) A defect that blocks a v0.3 release comes before
general gaps. (3) Batch hardware work so that one normal-image flash covers
several items. (4) Performance work waits for measurements.

### P0 — v0.3-rc blockers and cheap closures

1. **Housekeeping (complete).** Removed the v0.2-rc1 external request after
   v0.2 publication. The reported Oracle converter issue is off-card. The
   JCMathLib F103 fix is recorded in `20261126-1440-jcmathlib-ec-keypair`.
2. **Status audit of P1 review items (complete, source inspection).**
   - RV-02, RV-03 and RV-05: **fixed, needs acceptance**. Frame unwinding
     removes the front frame, local accesses validate their full width before
     metadata access, and persistent field caches publish after storage succeeds.
   - RV-47: **still defective**. Applet abort restores records without
     invalidating transaction-created heap references.
   - RV-48: **still defective**. Repack requires identical element types.
   - RV-49: **still defective**. Copy/fill publish one persistent record,
     but the standalone calls have no commit-capacity preflight.
   - RV-50: **still defective for fill**. Non-atomic repack passes the flag
     to the range writer and updates existing rollback snapshots. Non-atomic
     fill still selects the atomic path.
   - RV-51: **still defective**. The remote exception table recognizes
     UtilException, while local dispatch and native reason handling are missing.
3. **HOST-01 Windows RSA-CRT-2048 `6F00` on `b4adbe13` (waiting on tester).**
   Exact empty-card EE11 replay with jc32 completed APDUs 1–7110. The final
   command returned `AA 9000` with no serial fault. See
   `20261127-0751-rsacrt2048-exact-replay` and external request
   WIN-RSACRT2048-B4ADBE13-20261127 for the Windows serial log and starting state.
4. **RAM-pressure reclamation on EE11 (physical check complete, INV-15/16).**
   Full symmetric KAT passed 208 checks, cleanup restored the ISD-only state,
   and the serial log had no runtime fault. See `20261127-0754-ee11-ram-pressure-kat`.
5. **Review returned external results** (WIN-ECTESTER-9B03A1E0,
   WIN-KEYPAIR-107D7663, WIN-PQC-PERF-58B5EC0A) as soon as they arrive. They
   are blocked until then, so do not wait on them.
6. **One batched physical acceptance session (INV-18/22/27)** on a single
   identified token with a normal image. Every item here already has an
   offline fix and needs only a replay of an existing fixture:
   - CAPIN-07 LOAD rejection and recovery
   - CAPIN-04 frame limit passed on EE11. RV-43 target allocator recovery implemented. APDU allocation injection remains.
   - RV-01 opcode-fetch recovery
   - RV-60 legal `dup_x` matrix and typed cases passed on EE11. Remaining coverage below.
   - RV-44 rollback-failure recovery
   - RV-34 flash-identity logging completed on EE11 (see progress).

### P1 — invariant defects (fix offline, then add them to the next batched hardware run)

7. **Persistence semantics from the AGENTS.md 2026-11-19 directive (INV-14).**
   These are platform guarantees, not extras:
   - RV-47: references to objects created in an aborted transaction become null.
   - RV-49: atomic `Util.arrayCopy`/`arrayFill` have their own transaction boundary.
   - RV-50: NonAtomic writes stay outside rollback.
   - RV-05 and RV-45: caches and capacity stay unchanged when a write fails.

   Measure `sync_bytes` and `journal_bytes` for each change.
8. **Interpreter memory safety (INV-18/22).** RV-03 bounds-before-metadata
   (if the audit says it is still open), RV-02 nested-unwind coverage, RV-30
   exception field kinds, CAPIN-09 `itableswitch` widths and CAPIN-10
   `jsr`/`ret`.
9. **CAP linkage verification (INV-18 commitment).** CAPIN-01 remaining cases
   with the RV-55 A→B→C closure, the CAPIN-05 ROM InstanceFieldref fixture,
   CAPIN-06 loader static dispatch and `nargs` fixtures, CAPIN-11 and RV-27.
   CAPIN-08 waits for the RV-16 measurement.
10. **API contract bugs (INV-07/08).** Each is small and self-contained, so
    they suit short sessions:
    - RV-51 catchable UtilException
    - RV-48 `arrayCopyRepack`
    - RV-39 CBC MAC IV reset
    - RV-37 EC cofactor
    - RV-57 transient EC private key with ECDSA
    - RV-41 `receiveBytes`
    - RV-42 outgoing length limits (recheck after the extended APDU work)
    - RV-12 SCP03 13-byte case 3
    - RV-08 Cipher operational bytes
    - RV-40 crypto initialization across reset
11. **PROFILE-01 ECTester follow-ups (INV-08).** Compressed P-224 and the
    remaining cases out of 309/413. Classify each case as a defect, as
    unsupported and honestly reported, or as a tester artefact.

### P2 — larger tracks (after P0/P1, or in parallel in a second session)

12. **PROFILE-01 Java body removal (INV-03).** 449 concrete Java method bodies
    remain; this is a known invariant gap. Remove them one API class per step,
    keeping CAP linkage and object layouts unchanged.
13. **Power loss and BOOTSEL (INV-13/14).** PERSIST-02, PERSIST-03, BOOTSEL-01,
    RV-35 and RV-36. These need a freshly wiped normal image and real USB power
    removal, so schedule one dedicated session.
14. **Firewall (INV-19).** CTX-01 (including `externalAccess=true`), CTX-02,
    CTX-03, RV-59 and the GP-01 reinstall checks.
15. **Reclamation and storage acceptance.** PERSIST-01, RESOURCE-01/02 and
    STORE-01. Reuse the T8 evidence where possible.
16. **Final acceptance gate.** AC-02 (three clean-flash passes) and AC-01
    (Windows workflows). Run them last in each RC, because they depend on all
    the other items.
17. **Remaining HARD-01 / RV-20261103-28.** Parser fuzzing and malformed-input
    recovery on ARM, after items 8 and 9.

### P3 — measure first, optimise only if justified

- RV-16 (a prerequisite for CAPIN-08), RV-17, RV-46, RV-56, RV-19, and RV-18
  (document the limit now and implement later).
- PERF-20261110-05 also saves RAM (about 16 B per handle). Move it up if item 4
  shows an OOM margin problem. Then PERF-06, PERF-08 and the interpreter
  reprofile.
- Emulator only: RV-13, RV-14 and RV-15. Documentation: RV-20 and RV-21
  (refresh once per RC). HOST-02 cold-reconnect timings.

### Blocked or tracking only (do not pick)

- CRYPTO-XDH-ABI-20261121 waits for external SDK inputs. The ML-DSA physical
  run on 07E7/F42D happens only when one of those tokens is free.
- GP-01 Windows DELETE watchdog confirmation, LONGOP-01 and JC-RESET-01 are
  tracked under HOST-01. HMAC-01 reopens only if a new failure appears.
- RSA/RSA-CRT 4096 physical runs are deferred at the user's request.

## CAP and interpreter safety

### CAPIN-01

- [ ] Finish validation before LOAD publishes a package. A scoped physical pass exists (`8d73cb7c`). Carry forward unresolved loader/linkage cases, dependency chains (RV-20261106-55), and malformed LOAD recovery from the [detailed checklist](.agent/archive/2026-11-21-backlog-compaction/TODO.md#capin-01).

### CAPIN-03

- [ ] Validate frame-owned running-package state across nested calls, exceptions, SIO and returning to the caller. Frame storage is implemented.

### CAPIN-04

- [ ] EE11 verified the 64/65-frame boundary and same-session recovery. Exercise injected method-entry allocation failure through the full APDU path (RV-20261106-43).

### CAPIN-05

- [ ] Add the crafted external ROM InstanceFieldref rejection fixture. Runtime guards and INSTALL rejection are implemented. The crafted constructor test and deferred VPCD regression suites passed (`20261120-2135-capin05-06-receiver-fixtures`, `20261121-1230-vpcd-suites-after-pcscd`).

### CAPIN-06

- [ ] Mirror static dispatch checks in the loader and add package-token and interface `nargs` mismatch fixtures. Receiver checks and mismatch/array fixtures exist.

### CAPIN-07

- [ ] Verify physical LOAD rejection and recovery. Resolve the full-record bound in `resolveClassref` with the SIO SensitiveResult regression covered.

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

- [ ] Remove the remaining built-in Java method bodies under INV-03 while preserving CAP linkage and installed object layouts. The ARM max_crypto ROM inventory after the native JCSystem transaction step has 449 concrete Java method bodies. Convert API behavior to C++ and verify the linked firmware ROM has no executable Java API bodies.

- [ ] Audit advertised API capabilities against actual behavior and tests. Finish physical KATs for newly enabled crypto and deferred RSA/RSA-CRT 4096 validation. See progress for committed implementations and [external requests](.agent/EXT_REQUESTS.md) for XDH ABI inputs.
- [ ] Review external ExtendedLength results and finish the follow-up EC
  comparison (WIN-ECTESTER-9B03A1E0-20261126).
  The implementation landed in `aa65ea94` and `39fd4199`. INV-09 retains the
  T=1 WTX and logical-channel exclusions. PROFILE-02 stays deferred.
- [ ] Review remaining ECTester cases (309/413 on `05fb3d0b`), including
  compressed P-224. See `20261126-1843-ectester-full-05fb3d0b`.
- [ ] Finish ML-DSA physical validation on 07E7/F42D. DHKEM and named-parameter
  XDH remain unavailable. For XDH, SDK exports supply the ABI and the reference
  CAP and expected outputs remain outstanding (CRYPTO-XDH-ABI-20261121).

### CTX-01

- [ ] Support crypto factory `externalAccess=true` for calls from a foreign applet context through authorized Shareable interfaces. Verify access, ownership, deselect/reset lifetime and isolation before advertising support (INV-19).
- [ ] Finish firewall/SIO/proxy acceptance across foreign-value consumers, GC/reset/DELETE roots, stale owners and physical same-CAP/multipackage lifecycle. Include unchanged upstream Multi CAP coverage where available.

### CTX-02

- [ ] Broaden physical SensitiveResult/SIO validation beyond JCSystem to AID, atomic Util and reset. Cover JNI conversion invalidation (RV-20261106-25).

### CTX-03

- [ ] Finish same-CAP firewall-context and per-instance ownership acceptance across GC, reset, full-registry restore and persistence. Verify sibling CLEAR_ON_DESELECT allocation through SIO and separate-CAP rejection.

### GP-01

- [ ] CAP-REINSTALL-01/02/03: finish physical stale-route, late DELETE failure and changed-version replacement checks. Verify fresh lookup and failed/successful APDUs around reinstall and cold boot.
- [ ] Confirm the GP DELETE watchdog fix on Windows. See archived packaging
  evidence under `20261124-2235-gp-delete-handoff` and package `6d3bb526`.
- [ ] Finish physical GET DATA BER-TLV/value response coverage and cold-power registry checks. Preserve pagination and exact AID/continuation assertions.

## Host and integration

### BOOTSEL-01

- [ ] Finish reliable software entry into BOOTSEL and recovery on a chip-ID-pinned Pico 2, including recovery under load. Follow the physical-token procedure and retain bounded logs.

### HOST-01

- [ ] Investigate the Windows RSA-CRT-2048 `6F00`. Scoped simulator and EE11
  replays did not reproduce it (`35f3459b`). Fuller-store resource exhaustion
  remains a hypothesis. Keep Windows RSA-CRT-4096 confirmation for `d65eb278`
  and the unsupported RSA-4096 KeyPair rows open.
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

- [ ] Finish three consecutive clean-flash ARM integration passes, with the
  warning gate and exact image and token evidence. Verify Linux and Windows
  PC/SC ISD listing, baseline CAP installation and selection, a complete support
  run without transport errors, CSV agreement with the manifest and jCardSim,
  and survival and repeatability after physical power removal. See the
  [original AC1–AC5 criteria](docs/archive/plan.md).

## Open review acceptance

See the [review](docs/archive/review_20261106.md) and [detailed cases](.agent/archive/2026-11-21-backlog-compaction/TODO.md#open-review-acceptance--2026-11-06).

- [ ] **RV-20261106-01 — Contain opcode-fetch exceptions inside interpreter recovery.** Owner: HARD-01 / RV-20261103-28. Source fix and PC VPCD recovery pass: fall-through, truncated operand, and zero-length Descriptor method-body mutations each return a controlled status; manager reselection and subsequent GP GET DATA succeed (`9000`). Keep open for physical acceptance and remaining caller-state coverage. [R01](docs/archive/review_20261106.md#r01--p1-opcode-fetch-failure-escapes-the-interpreters-exception-boundary)

- [ ] **RV-20261106-02 — Correct nested frame unwinding direction.** Owner: CTX-01 / HARD-01. Preserve caller PCs/locals/roots when trimming nonzero-depth stacks; cover Java/ISO errors, allocation failure and timeout through multiple nested callbacks. Historical `pop_back()` versus `emplace_front()` defect; current source now uses front removal. [R02](docs/archive/review_20261106.md#r02--p1-nested-failure-cleanup-removes-callers-instead-of-callees)

- [ ] **RV-20261106-03 — Check local/stack bounds before metadata and pointer access.** Owner: HARD-01. Check complete two-word ranges before mutation; sanitize high local indices at nonzero frame bases and stack-capacity boundaries. Source ordering fixed in `b36e9cf1`; full boundary acceptance remains. [R03](docs/archive/review_20261106.md#r03--p1-local-metadata-is-indexed-before-the-local-variable-bounds-check)

- [ ] **RV-20261106-05 — Keep persistent field caches unchanged when a write fails.** Owner: PERSIST-03. Fault-inject capacity/allocation/serialization/backend errors for scalar/int/reference stores; catch and read immediately, commit, abort, collect and restore. Prove cache/storage agreement and preserved old roots, including failure before snapshot insertion. [R05](docs/archive/review_20261106.md#r05--p1-failed-persistent-setters-publish-cache-changes-before-storage-succeeds)

- [ ] **RV-20261106-07 — Bound every superclass traversal and reject cycles.** 2026-11-20 (`20261120-2125-rv07-hierarchy-cycle-fixture`): source audit found every superclass walk bounded (64 or 256 levels; interface casts, field resolution, method lookup, instance size, exception classification, RMI, Shareable checks). New crafted-CAP fixture `superclass-self-cycle` (applet class names itself as superclass): LOAD is accepted, INSTALL fails promptly with `6A80`, SecurityException at `new`, about 1 s on the PC VM. Remaining: two-class cycles are covered by the CAPIN-07 lower-offset rule at INSTALL validation; physical timing. Owner: HARD-01. Audit allocation sizing, class casts, Object lookup and field flattening, including checked size accumulation. Test self/two-class cycles and deep chains with bounded same-process recovery on PC and hardware. Hostile-CAP reachability/physical timing remain unmeasured. [R07](docs/archive/review_20261106.md#r07--p1-several-superclass-walks-can-loop-inside-one-native-operation)

- [ ] **RV-20261106-08 — Make all Cipher operational bytes consistent with native session lifetime.** Owner: PROFILE-01 / PERSIST-02. Audit IV, saved IV, buffered input, CTR stream and grown buffers; test update/abort/final, reset and fresh restore and inspect persistent records for retained operational data. Preserve existing transient key-workspace safeguards. [R08](docs/archive/review_20261106.md#r08--p1-cipher-operational-byte-arrays-still-have-persistenttransactional-lifetime)

- [ ] **RV-20261106-20 — Remove ARM cycle/smoke-image ambiguity.** Owner: HOST-01 / AC-02. Align wrapper target names and instructions; require exact full-runtime image/profile/hash and runtime marker in JCVM acceptance. Current `cycle arm` selects the smoke image; `cycle choupi` selects the full VM. [R20](docs/archive/review_20261106.md#r20--p1-the-documented-arm-cycle-flashes-the-smoke-firmware)

- [ ] **RV-20261106-16 — Measure repeated branch-boundary rescans.** Prerequisite of CAPIN-08 (its branch-result cache must wait for this measurement). Owner: HARD-01 / API-03. Compare equivalent loops at increasing method offsets; record device latency and RAM cost before introducing a validated instruction-start cache. Keep boundary checks and package invalidation intact. [R16](docs/archive/review_20261106.md#r16--p2-branch-validation-rescans-from-method-start-on-every-taken-branch)

- [ ] **RV-20261106-17 — Measure handle lookup and worst-case GC scaling.** Owner: PERSIST-01 / API-01 / EMU-01. Include forward/reverse chains, wide graphs and sparse slot tables; record heap size, GC duration, watchdog margin and p95/p99 APDU latency. Current list lookup and repeated mark passes have linear/quadratic scaling risks. [R17](docs/archive/review_20261106.md#r17--p2-stable-handle-lookup-and-gc-have-adverse-scaling)

- [ ] **RV-20261106-18 — Disclose Signature cumulative limits and peak memory.** Owner: PROFILE-01 / API-01. Document the current 32,767-byte buffering ceiling; test multipart boundaries and failed growth. Evaluate incremental native hash/MAC state with correct reset/abort semantics as separate implementation work. [R18](docs/archive/review_20261106.md#r18--p2-signature-update-is-whole-message-buffering-with-a-32767-byte-ceiling)

- [ ] **RV-20261106-19 — Bound service gaps inside long native loops.** Owner: HOST-01 / HARD-01 / API-03. Measure maximum-size persistent Util copy/fill, crypto workspace copies and cleanup under capacity pressure; verify CCID progress, immutable deadline and watchdog margin before choosing callback cadence. [R19](docs/archive/review_20261106.md#r19--p2-lengthy-native-copyfill-loops-lack-cooperative-progressdeadline-checks)

- [ ] **RV-20261106-21 — Synchronize profile/trust statements with current code and limits.** Owner: PROFILE-01 / ARCH-08. Replace blanket multi-applet exclusions with a capability/remaining-gap/evidence matrix; disclose Signature limits and SCP03 subset, state RISC-V as out of scope and separate helper/emulator/hardware evidence. Review R01–R20 remain open until validated. [R21](docs/archive/review_20261106.md#r21--p3-profiletrust-documentation-has-fallen-behind-implemented-behavior)

- [x] **RV-20261106-34 — Keep explicit flash device identity mandatory through discovery and re-enumeration.** Nine synthetic sysfs cases passed. The v0.3-rc1 flash targeted `EE116F37C7E4A04E`, logged the serial and chip ID, completed CDC BOOTSEL recovery and verified flash. The same chip ID's PC/SC reader returned the expected CPLC source ID. See `20261127-0630-ee11-flash-identity`.

- [ ] **RV-20261106-35 — Pin one reader through every power-up acceptance phase.** P2; owner: HOST-01 / PERSIST-03 / AC-02. Forward the exact reader to the existing APDU probe, record actual selected identity and reject ambiguous fallback. Test opposing registry/APDU outcomes across two readers; retain the honest power-cycle-not-verified marker. [R35](docs/archive/review_20261106.md#r35--p2-the-power-up-acceptance-script-can-combine-evidence-from-different-readers)

- [ ] **RV-20261106-36 — Align physical CCID and VPCD reset/power callback semantics.** P2; owner: CTX-03 / HOST-01 / PERSIST-03. Prevent reset/power-off from invoking applet deselect code. Verify persistent callback counter, reset/deselect transients, transactions and native session cleanup through logical off/on and reader reset on each backend; test supply loss separately. [R36](docs/archive/review_20261106.md#r36--p2-physical-ccid-power-off-executes-an-applet-callback-unlike-vpcd-power-off)

- [ ] **RV-20261106-37 — Make EC cofactor requirements algorithm-specific.** P2; owner: API-03 / PROFILE-01. Allow fully populated EC keys and ECDSA to initialize without optional K; preserve DHC's required-cofactor check and R28's validation of inconsistent supplied K. Cover generated/manual public and private keys, missing/correct/invalid K and non-DHC agreement. [R37](docs/archive/review_20261106.md#r37--p2-ec-initialization-and-ecdsa-require-an-optional-cofactor)

- [ ] **RV-20261106-38 — Honor or reject preinitialized key-pair parameters explicitly.** P2; owner: API-03 / PROFILE-01. Existing-key constructors and preloaded parameter handling landed in `e0614fc3` and `107d7663`. Confirm externally and finish the parameter matrix. Test default/nondefault/invalid exponents and supported/invalid/unsupported EC domains, parameter readback, pair consistency and repeated generation. A named-curve restriction may reject unsupported domains explicitly. [R38](docs/archive/review_20261106.md#r38--p2-key-pair-generation-silently-replaces-preinitialized-parameters)

- [ ] **RV-20261106-39 — Reset CBC MAC IV to zero after finalization.** P2; owner: API-03 / PROFILE-01. Cover sign, successful/false verify, multipart input and explicit reinit on the same object after a nonzero-IV init. Compare first output with that IV and next output with an independent zero-IV reference; default-zero repeated-message tests are insufficient. [R39](docs/archive/review_20261106.md#r39--p2-cbc-mac-finalization-preserves-the-nonzero-iv-for-the-next-message)

- [ ] **RV-20261106-40 — Restore persistent-key crypto initialization across reset.** P2; owner: CTX-03 / PROFILE-01 / PERSIST-02. Separate retained key/mode/configuration from discarded intermediates. Test retained Cipher and Signature objects without reinit after logical reset and cold restore, partial input, zero-IV reset, inactive applets and output-length metadata. Keep cleared transient keys unusable. [R40](docs/archive/review_20261106.md#r40--p2-card-reset-discards-persistent-key-crypto-initialization-instead-of-restoring-it)

- [ ] **RV-20261106-41 — Validate receiveBytes state and offsets despite fully buffered input.** P2; owner: PROFILE-01 / HOST-01 / HARD-01. Enforce checks through native VM dispatch. Test calls before primary receive, after outgoing, negative/oversized offsets, zero remaining input, exact Java exception reasons and recovery. Valid post-receive calls may return zero. [R41](docs/archive/review_20261106.md#r41--p2-receivebytes-ignores-its-state-and-offset-contract)

- [ ] **RV-20261106-42 — Enforce outgoing API length limits and preserve BAD_LENGTH reasons.** P2; owner: PROFILE-01 / HOST-01. Separate applet capability and chaining limits from the 1,022-byte buffer capacity; test -1/0/252/253/256/257/1022/1023, chaining modes and invalid states. Catch/read reasons rather than asserting only APDU failure; explicitly disclose any supported long-response extension. [R42](docs/archive/review_20261106.md#r42--p2-outgoing-length-validation-follows-buffer-capacity-rather-than-the-declared-apdu-contract)

- [ ] **RV-20261106-43 — Contain allocation failure during package-stack insertion at method entry.** The package list was removed and the RMI unit suite confirmed that `changePackageID` does not allocate. An injected frame-insertion allocation failure preserves caller package, frame, locals and arguments on the host. The target method-entry path now makes the frame allocation recoverable and maps failure to the existing stack-overflow path. Exercise the failure through an APDU and audit analogous noexcept allocation paths. Measure per-call allocation and method-heavy latency before choosing storage changes. P1; owner: HARD-01 / CTX-01 / EMU-01. [R43](docs/archive/review_20261106.md#r43--p1-method-entry-package-bookkeeping-can-terminate-on-allocation-failure)

- [ ] **RV-20261106-44 — Preserve recovery state when rollback itself fails.** P1; owner: PERSIST-03 / STORE-01 / HARD-01. Software fix is implemented: rollback retains its journal and active guard after erase/write/sync failure, blocks persistent reads/writes and new commits, and retries recovery before applet APDUs (returns `6581` while recovery still fails). Regression injections cover restore erase/write failure, rollback sync failure, commit plus rollback sync failure, no partial durable publication, successful retry, and simulated cold reset restoring the prior committed state. Remaining acceptance: validate recovery on hardware. [R44](docs/archive/review_20261106.md#r44--p1-failed-rollback-discards-the-recovery-journal-and-reopens-ordinary-writes)

- [ ] **RV-20261106-45 — Make snapshot publication and capacity charging exception-safe.** P2; owner: PERSIST-02/03 / HARD-01. Inject vector growth failure after record read/charge, verify unchanged journal/capacity, then retry the same tag and commit/abort. Cover old/new records and prior successful snapshots; distinguish this from instance-cache rollback. [R45](docs/archive/review_20261106.md#r45--p2-snapshot-insertion-failure-leaves-commit-capacity-charged-without-a-snapshot)

- [ ] **RV-20261106-46 — Avoid owning-array reconstruction for each persistent element check.** P2; owner: API-01 / PERSIST-01 / HARD-01. Preserve header/type/size/provenance validation with bounded metadata access. Measure increasing primitive/reference scans, allocator calls, temporary bytes and latency; reference validation currently initializes an entire owner vector per element. Cover low-memory reads and malformed records with secure checks enabled. [R46](docs/archive/review_20261106.md#r46--p2-persistent-array-element-validation-allocates-a-temporary-array-on-every-access)

- [ ] **RV-20261106-47 — Invalidate transaction-created object references on abort or force safe session recovery.** P2; owner: PERSIST-02/03 / CTX-01 / HARD-01. Track allocation lifetime through native runtime state. Test aliases in locals/transient arrays, abort, null comparison and dereference, preexisting objects, commit and nested/SIO returns; prove stale handles cannot alias later allocations. Ordinary record rollback alone does not close this. [R47](docs/archive/review_20261106.md#r47--p2-transaction-abort-leaves-references-to-newly-allocated-objects-usable)

- [ ] **RV-20261106-48 — Implement native primitive array packing and unpacking contracts.** P2; owner: PROFILE-01 / API-03. Cover all byte/short/int cross-type directions, big-endian bit preservation, converted destination range/end offset, invalid multiples, zero length and same-array overlap. Reject booleans/references with the documented utility error; same-type copying is insufficient. [R48](docs/archive/review_20261106.md#r48--p2-arraycopyrepack-rejects-the-cross-type-conversions-it-is-intended-to-perform)

- [ ] **RV-20261106-49 — Give atomic ArrayLogic copy/fill a native transaction boundary.** P2; owner: PERSIST-02/03 / PROFILE-01 / HARD-01. Enforce capacity and all-or-nothing persistent updates outside/inside caller transactions without committing the caller. Inject failures at every destination write and check unchanged destination on rejection, catch/recovery and fresh restore; validate physical interruption separately. [R49](docs/archive/review_20261106.md#r49--p2-atomic-generic-copy-and-fill-have-no-standalone-atomic-transaction-boundary)

- [ ] **RV-20261106-50 — Keep generic NonAtomic writes outside applet rollback.** P2; owner: PERSIST-02/03 / PROFILE-01. Use distinct native storage behavior for byte/short/int and reconcile preexisting snapshots. Cover both orderings of atomic/nonatomic writes to one array, capacity exhaustion, overlap, abort and restore; do not route through ordinary journaled setters. [R50](docs/archive/review_20261106.md#r50--p2-generic-nonatomic-copy-and-fill-participate-in-applet-rollback)

- [ ] **RV-20261106-51 — Dispatch native utility errors as catchable Java UtilException with reasons.** P2; owner: PROFILE-01 / HARD-01 / CTX-01. Provide ILLEGAL_VALUE versus TYPE_MISMATCHED through native exception dispatch. Test exact/superclass catches for invalid types and mismatched fills, assert reasons and complete valid work in the same and next APDU. [R51](docs/archive/review_20261106.md#r51--p2-native-utility-errors-cannot-be-caught-as-the-declared-utilexception)

- [ ] **RV-20261106-55 — Validate loaded-library dependency readiness before applet execution.** Implemented via CAPIN-01 (bounded eight-slot dependency-closure worklist); **acceptance only** (remaining A→B→C cases). P2; owner: GP-01 / PROFILE-01 / HARD-01. Cover A→B→C with missing/wrong-version/present C and C used only inside B's method body. Validate reachable dependencies with bounded traversal or an earlier library linking boundary, preserve supported load order, define cycle handling and demonstrate correction permits INSTALL. Direct-import and deletion guards are insufficient evidence. [R55](docs/archive/review_20261106.md#r55--p2-install-preflight-does-not-validate-the-dependency-closure-of-loaded-libraries)

- [ ] **RV-20261106-56 — Avoid repeated full-store orphan sweeps for already clean free IDs.** P2; owner: STORE-01 / HOST-01 / PERSIST-03. Measure scan counts/manager latency for 0/1/7 free slots and sparse/dense clean/orphaned stores. Batch or cache only with validated recovery invariants; cover interrupted publication and package-ID reuse. Traversal counts are source-inferred; no flash erase count or speedup is claimed. [R56](docs/archive/review_20261106.md#r56--p2-orphan-cleanup-repeats-full-storage-walks-for-ordinary-free-package-slots)

- [ ] **RV-20261106-57 — Define and validate ECDSA with enabled transient EC private keys.** P2; owner: API-03 / PROFILE-01 / CTX-03. Native initialization currently accepts only persistent type 12. Support or disclose types 30/31 explicitly; compare identical complete key material across lifetimes/digests, independent verification, clear-event rejection and reinit. Validate downstream component use; keep optional-K handling separate. [R57](docs/archive/review_20261106.md#r57--p2-ecdsa-initialization-rejects-enabled-transient-ec-private-key-types)

- [ ] **RV-20261106-58 — Complete validation of the existing-key KeyPair constructor.** P2; owner: API-03 / PROFILE-01. The native overload supports EC FP, RSA, RSA-CRT and PQC. Pico 2 replay covers JCMathLib EC initialization and RSA/RSA-CRT generation with getter identity and exponent retention. Add focused coverage for mismatched sizes and algorithms, transient private-key lifetimes and construction from already initialized keys. [R58](docs/archive/review_20261106.md#r58--p2-the-keypair-constructor-taking-existing-keys-remains-an-unbound-stub)

- [ ] **RV-20261106-59 — Prevent unreachable foreign-array cycles from becoming permanent GC roots.** P2; owner: PERSIST-01 / CTX-01 / HARD-01. Separate registry edges from reachable roots using a validated cross-heap lifetime strategy. Exercise two/three-heap cycles and diamonds with/without real roots, different collection orders, edge breaking, package deletion and handle reuse. Use production Context enumeration and measure reclaimed/live counts plus bounded repeated-work RAM growth while preserving live foreign references and owner collision isolation. [R59](docs/archive/review_20261106.md#r59--p2-foreign-array-root-routing-can-keep-an-unreachable-cross-heap-cycle-alive)

- [ ] **RV-20261106-60 — Audit acceptance of the corrected positive-depth dup_x implementation.** P1; owner: HARD-01 / PROFILE-01 / CTX-01. The RMI stack-helper suite covers all M/N byte combinations, exact values, underflow/overflow recovery, reference-owner metadata, and int pairs; `./tools/test-rmi-registry` passes. The generated VPCD CAP now executes all 24 legal `dup_x` operands through applet INSTALL and passes the constructor depth check; invalid `0x17` reaches interpreter dispatch and is rejected with `vm=6F00` before applet publication. The VPCD interpreter now executes all 24 legal operands and catches all 232 invalid byte values in one APDU; a separate caught-invalid APDU and the next APDU both return `9000`. Interpreter typed cases preserve an APDU reference identity, duplicate a complete int pair, and duplicate an int pair above a short (`dup_x 0x23`). A cross-AID SIO VPCD CAP now sends a client-owned array reference through `dup_x 0x12`, returns it from the service, and verifies identity at the client; the focused probe passes. EE11 passed the 24 legal operands and typed reference/int/mixed cases. Physical invalid-operand and cross-AID SIO coverage remain open. Use integer-range checks before pointer arithmetic as in R03. [R60](docs/archive/review_20261106.md#r60--p1-positive-depth-dup_x-duplicates-the-wrong-stack-segment)

- [ ] **RV-20261106-12 — Accept 13-byte no-data case-3 SCP03 commands.** Owner: GP-01. Test valid independent MAC vectors with/without Le, malformed lengths and failed-MAC recovery; derive minimum length from supported APDU cases. [R12](docs/archive/review_20261106.md#r12--p2-scp03-rejects-a-valid-no-data-case-3-protected-command)

- [ ] **RV-20261106-13 — Define and enforce a total VPCD frame deadline.** Owner: EMU-01. Cover slow-drip header/payload and EINTR, not just idle/stopped input; record elapsed upper bounds and successful reconnect. Distinguish intentional inactivity timeout if retained. [R13](docs/archive/review_20261106.md#r13--p2-vpcd-read-timeout-is-an-inactivity-timeout-not-a-frame-deadline)

- [ ] **RV-20261106-14 — Apply execution budgets to the PC interpreter.** Owner: EMU-01 / HARD-01. Use monotonic time and preserve nested deadlines; recover an infinite interpreted call in the same process, restore transactions/callers and accept a subsequent command. Killing a timed-out emulator does not close this. [R14](docs/archive/review_20261106.md#r14--p2-the-pc-interpreter-has-no-equivalent-execution-deadline)

- [ ] **RV-20261106-15 — Finish review-harness validation after the allocation-link fix.** Owner: HARD-01 / PERSIST-01. Standalone allocation linkage is resolved by MIG-20261107-01 (nine assertions pass). Remaining: validate/repair the separate review persistence fixture’s CAP dependencies and reference-slot assumptions, then include applicable targets in routine offline checks. [R15](docs/archive/review_20261106.md#r15--p2-allocation-regression-cannot-currently-link)

- [ ] **RV-20261106-27 — Preserve reference-array component type namespaces.** P2; owner: CTX-01 / PROFILE-01 / PERSIST-02. Retain the defining CAP or stable resolved component identity through allocation/persistence; exercise imported-library allocation followed by applet stores/casts using deliberately colliding CP indexes, then restore. [R27](docs/archive/review_20261106.md#r27--p2-reference-array-component-types-lose-their-defining-cap-namespace)

- [ ] **RV-20261106-30 — Prevent exception diagnostics from reading arbitrary field kinds as short.** P2; owner: HARD-01 / PROFILE-01 / CTX-01. Exercise fresh-boot hardware throws of custom exceptions with byte/int/reference/short first fields, exact/superclass catches and a following successful APDU; trace-budget state must not alter behavior. [R30](docs/archive/review_20261106.md#r30--p2-hardware-exception-diagnostics-assume-every-exceptions-first-field-is-a-short)

## Performance

- [ ] Before changing dispatch or branch checks, reprofile the XOR control
  and interpreter fetch/decode phases. See the
  [November measurements](docs/archive/jcvm-bytecode-performance-20261122.md)
  and [follow-up](docs/archive/review_VMperf_second_pass_20261122.md).

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
