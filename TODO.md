# Project TODO

Updated: 2026-11-08. This is the active backlog for `plan.md`.
Implementation and validation are separate: an implemented task stays open when
its acceptance gates remain unverified. Keep upstream CAPs unchanged and follow
`AGENTS.md` for native API boundaries, build gates, hardware access and commits.

## Current priorities

1. **Top priority: RESOURCE-01 — stop long-run resource exhaustion and
   investigate resets.** Complete safe object reclamation and verify bounded
   allocation during key reuse and JCAlgTest cache clearing. Coordinate
   PERSIST-01/02/03, STORE-01, API-01/03 and JC-RESET-01; see the acceptance
   task below. This takes precedence over the other backlog items.
2. Close **HOST-02 (P1)** acceptance: collect no-console Windows reconnect
   timings for the current and original images. Normal startup wait removal
   and the no-DTR diagnostic deadline are implemented and hardware-tested; the
   timing harness is ready for Windows execution. See the
   [diagnostic workflow](docs/usb-startup-diagnostics.md).
3. Continue **CTX-01/03** SIO lookup/proxy/firewall work and separate CAP
   firewall contexts from applet-instance ownership; retained per-instance
   contexts and a proxy registry are groundwork, not complete SIO semantics.
   R53 `JCSystem.isAppletActive(AID)` now checks the executing install context
   and active retained applet instances across AIDs; focused callback and SIO
   VPCD checks pass.
4. Finish **STORE-01**, **API-01/03** and **PERSIST-01/02/03** to distinguish
   live storage limits, reclaimable records, API results, and power-loss safety.
5. Close the current-image coverage and integration gates in **API-03**,
   **GP-01**, **AC-01/02**; do not infer them from runtime heartbeat output.
6. Implement and validate native support for the `ALG_HMAC_*` algorithms;
   acceptance must use the unchanged JCAlgTest 1.8.4 / jc305 fixture and
   matching client. See HMAC-01.
7. Diagnose the reported physical-reader disappearance during the max_crypto
   JCAlgTest Signature factory matrix without using real tokens. See
   JC-RESET-01.

Physical availability is volatile. On 2026-11-07 the helper granted access
to all three Pico USB nodes and `lsusb -d 2e8a:0009` saw three runtime CCID
devices. After physical GPPro/Cipher-lifecycle attempts, `pcscd.service` was
active but bounded `pcsc_scan -r` published no physical readers. Firmware-only
max_crypto images were flashed to identified boards D92 and 2BED; both passed
finite serial runtime checks, but neither is currently PC/SC-visible. Board
8E46 was not changed. Refresh USB and PC/SC immediately before hardware work;
do not treat USB enumeration alone as a PC/SC-available reader. See
`.agent/HANDOFF.md` and `.agent/QKB.md` for the current observation and history.

## How to use this backlog

Each ID owns one outcome; related tasks link rather than repeat its history.
Update status, remaining work and acceptance evidence here. Put detailed runs,
failures, measurements and commit records in [progress.md](progress.md), and
reusable diagnoses in [.agent/QKB.md](.agent/QKB.md).

The [original TODO](.agent/archive/2026-11-01-todo-pre-compaction/TODO.md) is
preserved byte-for-byte with a SHA-256 manifest. The [migration map](.agent/archive/2026-11-01-todo-pre-compaction/MIGRATION.md)
accounts for every original unchecked item, including historical entries.
Archive text is historical evidence, not current device state or priorities.

## Top-priority resource lifecycle investigation

### RESOURCE-01

- [ ] **Stop long-run NO_RESOURCE failures and establish the cause of resets.**
  Priority: highest, explicitly requested by the user on 2026-11-08.
  Implementation plan: [staged RESOURCE-01 plan](docs/resource-01-implementation-plan.md).
  Evidence: current JCSystem native dispatch reports object deletion unsupported
  and requestObjectDeletion() throws ILLEGAL_USE (reason 6), preventing the
  subsequent RSA-cache cleanup in the supplied JCAlgTest reset handler.
  The 2026-11-07 unchanged 1.8.4 VPCD support run returned F205 / NO_RESOURCE
  after 797,682 ms in RSA CRT 3072 probing, with persistent-reference
  serialization failing from filesystem OutOfFlash. Physical reset causality
  remains unproven; VPCD and RP2350 use different storage backends/capacities.
  RAM heap collection does not reclaim persistent body records; the production
  persistent reclaimer remains disabled pending PERSIST-01 safety acceptance.
- Remaining work:
  - Account separately for persistent live/orphan tags and bytes, filesystem
    obsolete versions/compaction, RAM backing, handles and native crypto state.
    Instrument the failing allocation, reset request and post-APDU boundaries.
  - Complete PERSIST-01 multi-owner, alias/root, transaction and physical
    interruption-safety gates before enabling normal persistent reclamation.
    Then implement context-scoped deferred JCSystem deletion requests and
    advertise support only when the required reclamation guarantees hold.
    Preserve the unchanged supplied CAP; do not replace deletion with a no-op.
  - Verify persistent versus transient allocation: durable applet/key state
    survives power loss; transient contents and crypto scratch stay in RAM.
    Audit key replacement, partial-construction failure, stale native roots,
    obsolete body tags, and resource release on every error path.
  - Verify same-key setters reuse bounded backing; clearKey clears material and
    initialized state while retaining reusable capacity. Audit RSA allocation
    of unused components and per-byte persistent writes for avoidable storage
    pressure and write amplification, preserving transaction semantics.
  - Diagnose resets independently using reset cause, watchdog state, last APDU
    and filesystem recovery evidence; distinguish firmware faults from USB/
    PCSC reader disappearance. Coordinate JC-RESET-01.
- Acceptance: repeated same-key reuse, new-key replacement plus deletion, and
  unchanged JCAlgTest 1.8.4 / jc305 reset/free-cache cycles reach a measured
  plateau in retained RAM, handles and persistent records. Demonstrate that
  deletion requested before clearing cache references reclaims the resulting
  unreachable persistent graph before the next process() call, preserves live
  aliases/other owners, and safely recovers from interrupted reclamation.
  Complete long-run physical validation without NO_RESOURCE, FAULT, unexpected
  reset or transport loss; record duration, iteration count and resource
  high-water marks. Follow both-architecture build and hardware gates.
- Ownership: this task coordinates the outcome; PERSIST-01/02/03, STORE-01,
  API-01/03 and JC-RESET-01 retain their existing implementation/safety scope.
  Implementation proceeds in independently validated stages; do not enable
  persistent reclamation before its existing safety gates pass.
- Progress (2026-11-08): Native `KeyBuilder` now allocates only the component
  arrays used by each RSA key form. A 3072-bit public/plain key omits five
  192-byte arrays (960 payload bytes); a CRT private key omits two 384-byte
  arrays (768 payload bytes). An RSA public+CRT pair therefore avoids 1,728
  component payload bytes and seven persistent array records, before record
  metadata. These are capacity-derived values, not measured flash deltas.
  VPCD exercises public, plain private and CRT private `clearKey()`, setters
  and getters; all pass. Full JCAlgTest plateau/resource acceptance remains
  open. A separate test-only RSA CRT KeyPair/signature probe returned generic
  `3F` during its sign/verify phase after key generation/component checks; its
  cause and relation to persistent resource pressure are unconfirmed.
- Progress (2026-11-08): Extended opt-in PC VPCD `PICO2_PERSISTENT_INVENTORY`
  output with active global persistent record counts and payload-byte totals;
  the applet-owner-only scan missed key graph records. A three-form retained
  RSA VPCD probe reported global active records/payload bytes of 6/34 at
  baseline, 17/264 after public, 27/483 after plain private and 43/800 after
  CRT private. These values include test/APDU scaffolding and are not isolated
  per-key costs or physical-flash measurements. ARM, RISC-V and PC max_crypto
  builds pass; the focused VPCD probe passes 5/5.

## Functionality review — 2026-11-03

### RV-20261105-01

- [x] **P1: Dynamic CAP Descriptor lookup used the bytecode offset instead of
  the method-header offset.** Evidence: reproduced while installing the
  unchanged deployed Core CAP (`6A80` / VM `6F00` SecurityException).
  Resolution: `Method_Handler::callMethod` now validates the method pointer and
  computes the Descriptor offset from the method header, including the
  handler-table and method-count prefix. The multipackage VPCD smoke passes
  LOAD for two distinct packages and INSTALL for three instances. ARM,
  RISC-V, and PC max_crypto builds pass. The complete 47-check
  pagination/persistence/DELETE/JCSystem transient VPCD suite passes. The
  fixture limits persistent-inventory diagnostics to one scan per owner ID
  and cascades deletion of the 18 stress instances after validating pagination;
  individual DELETE behavior remains separately covered. Physical board
  validation remains pending because none of the user-reported BOOTSEL boards
  enumerated inside the VM.

The detailed current-code review is [docs/review_20261103.md](docs/review_20261103.md).
These are concrete review follow-ups under the existing task owners.
Reproduced failures and source-confirmed paths remain open until corrected and
validated; passing unrelated helper tests does not close them. Review probes are
archived under `artifacts/reviews/review_20261103/`. No runtime fixes were made
during the review.

### RV-20261103-01

- [x] **P1: CCID length overflow defeats receive bounds.**
  Owner: HARD-01, HOST-01. Evidence: Source; [review R01](docs/review_20261103.md#r01--ccid-length-overflow-can-escape-the-receive-buffer-limit).
  Acceptance: compare `payload_length` to `max_total - header_size` before adding; validate minimum header/slot/state and exact frame length; never arm an endpoint beyond remaining capacity. Exercise all wrap values and packet fragmentation in a host shim of the production driver. No malformed USB command was sent to hardware.
  Progress: the production CCID path now uses checked framing and only arms
  Bulk OUT for the remaining buffer/frame capacity. Focused host test passes
  all ten wrapped lengths, maximum/oversize boundaries, header fragmentation,
  slot/state checks, exact-frame checks and receive-size bounds. ARM and
  RISC-V max_crypto builds pass. Physical flash verification and finite serial
  passed on failover board 2BED; its PC/SC reader was found and read-only GPPro
  APDU exchange passed. The separate 8E post-flash enumeration failure is
  recorded in QKB and does not block this acceptance.

### RV-20261103-02

- [x] **P1: Replacing an array overwrites older aliases.**
  Owner: PERSIST-01/02. Evidence: Reproduced; [review R02](docs/review_20261103.md#r02--persistent-array-replacement-overwrites-an-older-objects-identity).
  Acceptance: assign durable identities per object allocation/generation; make fields point to those identities. Preserve A until its last reference is gone. Test replacement, clearing, aliases read in either order, GC, transactions and fresh-heap restore.
  Progress: persistent array body tags now include a monotonically increasing durable identity under the original field prefix. Counter writes participate in transaction snapshots. Production persistence tests cover reassignment with an older alias, fresh-heap restore, GC, abort, and non-reuse after abort; `./tools/test-persistent-transaction` passes. ARM and RISC-V max_crypto builds pass. Physical ARM flash verification and finite serial passed on Pico 2 `2BED26ED20556535` (`TEST:SUMMARY pass=1 fail=0`). No CAP-level alias scenario was run on hardware.

### RV-20261103-03

- [x] **P1: Aborted static reference remains cached.**
  Owner: PERSIST-02/03. Evidence: Reproduced; [review R03](docs/review_20261103.md#r03--transaction-abort-does-not-restore-cached-static-references).
  Acceptance: restore or invalidate cached static references on successful abort and failed commit; preserve canonical object identity when reloading. Cover null, object and array substitutions, aliases and collection immediately after abort.
  Progress: static cache roots now carry `persistentWriteRollbackGeneration`; lookups and GC enumeration skip roots from an earlier generation. The persistence regression covers a null substitution followed by abort, an object substitution followed by failed commit, canonical array handle restoration, and GC retaining the restored root. `./tools/test-persistent-transaction` passes; ARM/RISC-V builds and physical 2BED flash/finite monitor pass.

### RV-20261103-04

- [x] **P1: DELETE rollback cannot undo raw erases.**
  Owner: PERSIST-03, STORE-01. Evidence: Source; [review R04](docs/review_20261103.md#r04--delete-prefix-erases-bypass-transaction-before-images).
  Acceptance: make namespace deletion participate in a rollback/recovery protocol at the actual storage boundary. Inject a failure after one real deletion and before publication; require all prior live values and registry entries to remain usable. A passing mock rollback is insufficient.
  Progress: transaction-aware prefix and package-state erase helpers snapshot every matching record before invoking the backend bulk erase. Delete call sites now use these helpers. The RP2350 overlay test links the production snapshot code and filesystem backend, fails bulk erase after a real overlay mutation, rolls back, reinitializes from committed flash, and verifies persistent values, package/APK instance registry metadata, lifecycle records and unrelated package data. Both target builds, PC build and physical 2BED startup pass.

### RV-20261103-05

- [x] **P1: Lifecycle publication can leave inconsistent stores.**
  Owner: PERSIST-03, GP-01. Evidence: Source; [review R05](docs/review_20261103.md#r05--delete-and-load-publication-lack-complete-cross-store-recovery).
  Acceptance: define and implement recovery for each publication point, including retry semantics and cleanup of incomplete LOADs. Fault-inject between stores and after each durable transition. Successful filesystem snapshots alone do not establish atomic package lifecycle.
  Progress: added idempotent startup/retry reconciliation for CAP-store and persistent-filesystem publication boundaries. Disabled CAP packages (incomplete LOAD or package DELETE) are removed; enabled bitmap entries without a CAP have their package namespace cleared and bit disabled; instance registry entries with a missing applet-root record are removed. Recovery runs before PC VPCD registry validation and before JCVM restore/load/install/DELETE operations, failing closed on errors. The RP2350 production overlay suite seeds each interrupted durable state, repeats recovery, and passes LOAD, DELETE, missing-CAP package cleanup and missing-root instance recovery. ARM, RISC-V and PC builds pass; physical 2BED CHOUPI flash and finite monitor pass (`TEST:SUMMARY pass=1 fail=0`).

### RV-20261103-06

- [x] **P1: Duplicate/full-registry INSTALL is rejected too late.**
  Owner: PERSIST-03, GP-01. Evidence: Source; [review R06](docs/review_20261103.md#r06--install-checks-registry-conflicts-after-committing-applet-state).
  Acceptance: preflight/reserve registry capacity and AID ownership before invoking INSTALL, then recheck publication invariants and recover idempotently. Test duplicates in the same/different package and a full instance table; rejected requests must leave existing state untouched and future installation usable.
  Progress: the shared CAP-store preflight now validates package presence, globally unique instance AID and package registry capacity before invoking the applet `INSTALL` method, then rechecks before journal publication; a failed recheck rolls back. PC and RP2350 stores enforce the same 40-instance limit, and recovery verifies journal owner identity. Duplicate/capacity conflict maps to `6A80`. Production overlay tests cover same-package and cross-package duplicate AIDs plus a full table. PC VPCD integration passes `TEST:SUMMARY pass=47 fail=0`, including unchanged registry and fresh retry. Physical Pico 2 `2BED26ED20556535` returned `6A80` for duplicate INSTALL, preserved one registry entry, installed a fresh instance, passed GET STATUS pagination, and removed the temporary package.

### RV-20261103-07

- [x] **P1: Missing automatic transaction abortion.**
  Owner: PERSIST-02/03, PROFILE-01. Evidence: Source; [review R07](docs/review_20261103.md#r07--applet-transactions-can-escape-lifecycle-and-reset-boundaries).
  Acceptance: add a lifecycle transaction guard that distinguishes runtime-internal INSTALL transactions from applet transactions. Validate normal return, uncaught exception, timeout, rejected selection, deselection and warm reset, followed by an unrelated applet command.
  Progress: runtime-owned CAP INSTALL transactions are now distinct from applet-owned transactions. JCSystem begin/commit/abort only act on the latter; applet transactions are aborted at process, select, deselect and card-reset boundaries. Normal process return with an open transaction maps to `6F00`, rejected selection remains `6999`, and deselection with an open transaction returns `6F00`. VPCD regressions pass normal return, uncaught transaction exception, deselect and rejected-select rollback plus fresh applet selection. Storage tests verify the reset abort helper does not consume a runtime INSTALL transaction. Interpreter errors, including timeout, use the same process-exit cleanup; no dedicated 120-second timeout injection or reset-interruption-during-bytecode test is available because the synchronous interpreter cannot receive a warm-reset request while executing an APDU.

### RV-20261103-08

- [x] **P1: Foreign retained references are traced in the wrong heap.**
  Owner: CTX-01/03, PERSIST-01. Evidence: Reproduced; [review R08](docs/review_20261103.md#r08--gc-drops-reference-owner-tags-in-volatile-object-arrays).
  Acceptance: carry owner provenance through graph edges and route foreign roots to the owning package, or disable this newly allowed retention until that is safe. Include owner slot reuse, package deletion and non-colliding handles.
  Progress: Applet-owned volatile wildcard `Object[]` arrays may retain ordinary foreign references with per-slot owner provenance. Root walks route each edge to its owning package heap; equal compact handles are not mistaken for local objects, overwrites release roots, and package deletion clears the edges before owner IDs can be reused. Tests cover compact-slot reuse after owner-edge clearing. Persistent writes of such arrays record null in the flash field while preserving the live in-memory field, so compact cross-package handles are never serialized; a Descriptor-free VPCD regression confirms the field restores as null after emulator process restart. Static/instance stores of foreign objects and persistent arrays remain fail-closed. Native root tests pass; Descriptor-free VPCD SIO passes 34/34; physical Pico 2 `D92E743D5F640C0F` returns `6B439000` for retention and `6A2E9000` after source roots are cleared and GC runs. Both ARM and RISC-V max_crypto builds and the final finite physical runtime cycle pass.

### RV-20261103-09

- [x] **P2: Object result assertions and roots confuse owners.**
  Owner: CTX-02. Evidence: Reproduced; [review R09](docs/review_20261103.md#r09--object-sensitive-results-have-no-owner-provenance).
  Resolution: `SensitiveResult` object values now retain package owner plus handle, assertions compare normalized full identity, and GC roots only package-local results in the owning heap. Native unit coverage and an interpreted SIO fixture prove equal compact handle `0002` across package IDs 57 and 58 is rejected. VPCD 32/32, PC/ARM/RISC-V builds, and Pico 2 `2BED26ED20556535` finite runtime validation passed; see `progress.md` and `.agent/QKB.md`.

### RV-20261103-10

- [x] **P1: Claimed finite execution budget is renewable.**
  Owner: HARD-01, API-03. Evidence: Source; [review R10](docs/review_20261103.md#r10--the-applet-timeout-is-renewable-through-native-and-nested-calls).
  Resolution: An RAII execution-budget scope establishes one immutable 600-second top-level deadline; nested interpreter entries share it. Native RNG callbacks update a separate progress timestamp and service watchdog/CCID work without extending the deadline, and stop RSA key generation when it expires. A fake-clock regression covers wraparound, progress, repeated nested/SIO scopes, deadline expiration, RAII unwind and a fresh following operation. The 600-second cap preserves the observed physical RSA-CRT-3072 workload (72.612 seconds on the current run; historical maximum 171.517 seconds). PC/ARM/RISC-V builds pass, VPCD asymmetric suite passes 99/99, and physical cycle plus RSA-CRT-3072 pass; see `progress.md` and `.agent/QKB.md`.

### RV-20261103-11

- [ ] **P1: Cipher key copies survive original transient-key clearing.**
  Owner: PROFILE-01, HARD-01. Evidence: Source; [review R11](docs/review_20261103.md#r11--cipher-copies-outlive-transient-key-clear-events).
  Acceptance: retain checked key identity and appropriate lifetime metadata, or use workspace that clears with the key. Test retained Cipher objects across deselect, reader reset and fresh restore for symmetric and RSA transient keys. Modification of a key after `init()` is separately unspecified; this finding concerns defined lifecycle clear events.
  Progress: AES and RSA private Cipher workspaces now allocate sensitive copies as transient arrays with the source key's clear event and validate a transient marker before use. The current `./tools/test-cipher-lifecycle-vpcd` run passes 31/31 across deselect, reader reset and fresh emulator-process restore; deselect returns `UNINITIALIZED_KEY` (`42`), while reset and fresh restore return `INVALID_INIT` (`44`). Physical runs on 2026-11-07 did not reach the Cipher cases: after GPPro installed the probe CAP, SELECT checks passed but open-channel card-manager GET DATA returned `6982` instead of `9000`. The physical probe now explicitly resets the card after GPPro installation to clear any prior SCP03 session before testing; this setup change compiled but could not be reverified because `pcsc_scan -r` no longer published readers. D92 and failover board 2BED were firmware-only reflashed and both passed finite serial checks; card storage was preserved. Keep RV-11 open for the corrected physical AES/RSA lifecycle run.

### RV-20261103-12

- [ ] **P1: Reset preserves deselect-cleared sensitive data.**
  Owner: CTX-03, PERSIST-02. Evidence: Reproduced; [review R12](docs/review_20261103.md#r12--reset-does-not-clear-clear_on_deselect-arrays).
  Acceptance: reset must clear both classes across active and inactive contexts without invoking applet `deselect()` as a reset callback. Test reset directly from the active applet, inactive package contexts and transient-key flags.
  Progress: the runtime reset callback clears both event classes in selected and inactive applet heaps. Fixed a VPCD regression where selected-applet tracking had reintroduced `deselect()` calls on POWER_OFF, POWER_RESET and disconnect; those boundaries now drop selection and invoke reset cleanup directly. Standalone VPCD session tests pass 29/29 and the same-CAP transient regression passes 6/6, including active reset, inactive writer-context clearing, and an unchanged persistent deselect counter. Physical validation remains pending: three USB CCID devices enumerate, but only the two VPCD readers currently appear in PC/SC.

### RV-20261103-13

- [x] **P2: Restore includes tag bytes in array length.**
  Owner: PERSIST-02, PROFILE-01. Evidence: Reproduced/source; [review R13](docs/review_20261103.md#r13--restored-transient-arrays-have-the-wrong-length-and-uninitialized-storage).
  Acceptance: represent logical length separately or calculate it correctly; initialize only the data region and preserve its tag metadata. Validate byte/short/int/reference arrays after fresh restore and after clear/reset, including the largest accepted sizes.
  Resolution: restored `JC_Array` objects keep their logical element count separately from serialized tag bytes, zero initialize backing storage, and reject tag-plus-data backing lengths above 65,535 bytes with `FullMemoryException`. Native tests cover byte/boolean/short/int/reference lengths, zero values, clear behavior, last valid elements and maximum backing sizes. The same-CAP VPCD test confirms transient byte arrays restore with logical length four and zero values. Extended the Cipher lifecycle test to repopulate restored AES and RSA transient keys after a fresh emulator-process restore; both setters now pass. The earlier reported RSA `ArrayIndexOutOfBoundsException` was not reproduced against the rebuilt current VM; its historical cause is unverified. `./tools/test-jcarray` passes all cases and the lifecycle VPCD suite passes 31/31. No physical run was performed.

### RV-20261103-14

- [x] **P2: Signature workspace is ordinary memory.**
  Owner: API-03, PROFILE-01. Evidence: Reproduced; [review R14](docs/review_20261103.md#r14--signature-workspace-allocation-selects-the-wrong-overload).
  Acceptance: construct transient arrays explicitly and make the API type-safe so the enum cannot silently bind as a component token. Verify both initial and expanded buffers.
  Progress: Signature IV, message, and recovery-hash workspaces plus RSA/ECDSA expansion buffers now use explicit transient-array allocation. A deleted overload rejects passing `ClearEvent` to the ambiguous legacy `addArray` API. The full asymmetric VPCD suite passes 99/99 phases (97 vectors), including ECDSA and RSA-SHA1 inputs longer than the initial 64-byte buffer; native heap tests verify initial and expanded allocations are transient and `CLEAR_ON_RESET`. PC, ARM and RISC-V max-profile builds pass. Physical validation remains pending.

### RV-20261103-15

- [x] **P2: Digest/cipher/signature intermediate state has wrong lifetime.**
  Owner: PROFILE-01, PERSIST-02. Evidence: Source; [review R15](docs/review_20261103.md#r15--crypto-session-metadata-is-not-reset-or-excluded-from-transactions).
  Acceptance: define native engine session state separately from persistent Java object identity. Test update → warm reset → final, update inside an aborted transaction, retained objects after cold restore and Signature recovery state. Fixing buffer allocation alone is insufficient.
  Progress: Native crypto operational counters now live in a lazy, nonpersistent sidecar on each `JC_Instance`; card reset drops it while object identity and persistent configuration remain. PC VPCD tests cover digest update/final after warm reset, a digest update aborted inside a transaction, cold process restore, and RSA ISO9796-2 message-recovery verification across an aborted transaction. The full asymmetric VPCD suite passes 107/107 phases; native heap tests and PC/ARM/RISC-V max-profile builds pass. Physical-token validation was not run.

### RV-20261103-16

- [x] **P2: Reused CBC object starts with wrong IV.**
  Owner: API-03, PROFILE-01. Evidence: Source; [review R16](docs/review_20261103.md#r16--cbc-dofinal-restores-the-supplied-iv-instead-of-zero).
  Acceptance: implement the selected API version's algorithm-specific post-final state. Compare the second AES/DES CBC result to a newly initialized zero-IV engine, then verify that an explicit new `init()` restores the supplied IV. Keep CTR policy separate.
  Progress: Successful AES and DES CBC `doFinal()` calls now clear the active IV to zero while leaving the saved initialization IV available. CTR counter restoration remains separate. VPCD vectors cover first use with a nonzero IV, reuse with a zero-IV reference, and explicit reinitialization with the original IV; the symmetric suite passes 99/99 phases. ARM and RISC-V max-profile builds pass. No physical-token run was performed.

### RV-20261103-17

- [x] **P2: Streaming digest is a small whole-message buffer.**
  Owner: PROFILE-01, API-01. Evidence: Source; [review R17](docs/review_20261103.md#r17--messagedigest-update-is-limited-to-2046-total-bytes).
  Acceptance: use native incremental digest contexts with correct reset/transaction lifetime, or explicitly narrow the declared compatibility contract until implemented. Test more than 2,046 cumulative bytes across APDUs and block boundaries, including empty final input and reuse after completion.
  Progress: Chose the contract-narrowing branch for this step: `docs/supported-profile.md` now states the 2,046-byte cumulative limit and `CryptoException.ILLEGAL_USE` behavior. Native incremental contexts remain unimplemented because transaction-abort-safe digest context rollback must be designed with the JCVM transaction lifecycle; the boundary and multi-APDU tests remain required before expanding the contract.

### RV-20261103-18

- [x] **P2: Legal `int[]` length silently becomes zero.**
  Owner: PROFILE-01, API-01. Evidence: Reproduced; [review R18](docs/review_20261103.md#r18--large-int-array-allocation-silently-wraps-its-byte-length).
  Acceptance: use checked wide arithmetic before allocation/serialization. Either support the logical length or reject it with the documented Java Card resource error before returning an object. Test boundaries around 16,383/16,384 ints and record/header limits for all primitive types.
  Progress: `JC_Array` now checks element-width multiplication and the persistent field-record header in 32-bit arithmetic before narrowing. Requests that cannot fit raise the Java Card resource exception (`SystemException.NO_RESOURCE`) in array factories and `newarray`/`anewarray`; persistent serialization uses a widened size and rejects any residual oversized record. Tests accept `int[16383]`, reject `int[16384]`, and check byte/short/int persistent record boundaries.

### RV-20261103-19

- [x] **P2: `MIN_VALUE / -1` has wrong result.**
  Owner: PROFILE-01. Evidence: Reproduced; [review R19](docs/review_20261103.md#r19--minimum-value-division-returns-zero).
  Acceptance: return the minimum dividend for division and add short/int edge vectors for sign combinations, remainder and division by zero. Test actual bytecode execution on both compiler targets after the change.
  Progress: `sdiv` and `idiv` now return the unchanged minimum dividend for division by `-1`. Added an isolated Java Card CAP fixture that exercises both bytecodes, sign combinations, `srem`/`irem`, and caught divide-by-zero exceptions. Its converted JCA was inspected to confirm it contains `sdiv`, `idiv`, `srem`, and `irem`; the VPCD run passed. ARM and RISC-V builds passed. Physical hardware was not tested.

### RV-20261103-20

- [x] **P2: Shifts, overflow and word assembly invoke UB.**
  Owner: HARD-01, PROFILE-01. Evidence: Sanitizer; [review R20](docs/review_20261103.md#r20--arithmetic-and-word-reconstruction-use-undefined-c-operations).
  Acceptance: implement wrapping operations and reconstruction in unsigned fixed-width types with explicit signed interpretation. Respect Java Card short-shift semantics, which use sign extension to 32 bits; do not blindly substitute a 16-bit logical shift.
  Progress: Arithmetic and word reconstruction now use unsigned fixed-width operations and explicit signed decoding. Short shifts sign-extend to 32 bits before shifting and narrow the result. The division CAP fixture covers wrapping int/short arithmetic, negation, `iinc`/`sinc`, shifts including negative distances, and minimum division; VPCD passes. PC, ARM, RISC-V and array regression builds/tests pass. No physical-token run was performed.

### RV-20261103-21

- [x] **P2: SIO loses exception class/reason.**
  Owner: CTX-01/02. Evidence: Source; [review R21](docs/review_20261103.md#r21--cross-context-sio-calls-replace-server-exceptions-with-securityexception).
  Acceptance: distinguish a transport/firewall denial from a Java exception crossing the SIO boundary. Implement owner-safe exception propagation and caller context restoration, including subclass identity and reason. Test at least two exception classes with distinct catch blocks and a successful follow-up call.
  Progress: The caller now receives a caller-heap copy of an uncaught server exception after verifying its owner. Concrete exception class and scalar fields are preserved; non-global server-private object references fail closed. Caller context restoration is checked. The cross-AID VPCD test passed 33/33 cases, including `CryptoException` reason `0x1234`, exact `ArrayIndexOutOfBoundsException` catch identity, and a subsequent successful SIO call. PC, ARM, and RISC-V `max_crypto` builds passed. Physical testing was not run.

### RV-20261103-22

- [x] **P2: Inherited interface membership is missed.**
  Owner: PROFILE-01, CTX-01. Evidence: Source; [review R22](docs/review_20261103.md#r22--class-to-interface-casts-do-not-search-superclasses).
  Acceptance: walk the class hierarchy with bounded cycle detection and correct package resolution; retain recursive superinterface handling. Validate same-package and imported superclass cases.
  Progress: Class-to-interface checks now walk up to 256 superclasses, resolving each link from its declaring package. Superinterface membership uses a bounded 64-node traversal with duplicate detection. A generated test-only CAP passes `instanceof` and `checkcast` for interfaces declared on same-package and imported superclasses and through a three-level interface chain. The full VPCD cross-AID suite passed 34/34. PC, ARM, and RISC-V builds passed. No physical device was tested.

### RV-20261103-23

- [x] **P2: Invalid Util arguments can succeed or partially write.**
  Owner: PROFILE-01. Evidence: Source; [review R23](docs/review_20261103.md#r23--non-atomic-util-methods-do-not-prevalidate-their-ranges).
  Acceptance: enforce complete validation in native API dispatch before mutation, with tests for negative/zero lengths, nulls, boundary offsets and invalid overlapping ranges. Verify exception type, destination contents and SensitiveResult after failure.
  Progress: Native `invokestatic` dispatch now intercepts `Util.arrayCopyNonAtomic` and `arrayFillNonAtomic`, resets SensitiveResult, validates null and full range arguments before writes, preserves overlapping-copy behavior, and assigns the result only on success. Focused CAP-to-native VPCD coverage passed for negative and positive overrun ranges, null source/destination with zero length, end-boundary zero-length operations, valid and invalid overlap, exact exceptions, unchanged destination data, and unassigned SensitiveResult after failure. Full cross-AID VPCD regression passed 34/34; PC, ARM and RISC-V max_crypto builds passed. No physical device was used. Non-atomic persistent-write journal semantics remain RV-24.

### RV-20261103-24

- [x] **P2: Non-atomic APIs consume commit capacity and roll back.**
  Owner: PROFILE-01, PERSIST-03. Evidence: Source; [review R24](docs/review_20261103.md#r24--non-atomic-persistent-updates-still-enter-the-transaction-journal).
  Acceptance: provide a native mutation path with the selected non-atomic semantics, including interaction with records already journaled by atomic writes. Test mixed atomic/non-atomic writes and abort on the same and different persistent arrays, including a record larger than commit capacity. Do not weaken ordinary transaction accounting globally.
  Progress: `Util.arrayCopyNonAtomic` and `arrayFillNonAtomic` now use a native byte-array mutation path which skips the transaction journal/capacity charge. When an existing record already has an atomic before-image, successful non-atomic updates patch that image to preserve the non-atomic baseline; a transaction-created record remains subject to rollback. Focused VPCD CAP tests including the existing capacity-overflow case passed 7/7. PC, ARM and RISC-V max_crypto builds passed. No physical token was used.

### RV-20261103-25

- [x] **P2: CCID double dispatch and explicit-ISD selection mismatch.**
  Owner: GP-01, HOST-01. Evidence: Source; [review R25](docs/review_20261103.md#r25--physical-card-management-and-applet-routing-have-different-contracts).
  Acceptance: define one card-session dispatch contract with exactly one route per command, explicit ISD identity and activation/slot checks. Test GP commands while an applet is selected, failed authentication, explicit/default ISD selection and APDUs while powered off. Cover the production CCID path, not just helper encoders.
  Progress: The production CCID dispatcher consumes one shared route decision for each XfrBlock. GET RESPONSE is manager-routed only for CLA 84 or when the card manager has a segmented response pending; otherwise CLA 00 GET RESPONSE goes to the selected applet. The router rejects nonzero slots and powered-off transfers, recognizes default and explicit ISD AIDs, and routes unauthenticated protected GP commands to 6982 without applet fallback. `tools/test-ccid-apdu-route` passes 15/15. ARM and RISC-V max_crypto builds pass. On Pico 2 D92E743D5F640C0F, the production PC/SC test passed default and explicit ISD selection while an applet was selected, public manager GET DATA while selected, unauthenticated protected GP rejection (`6982`), and subsequent applet dispatch; the full lifecycle suite passed 36/36 and removed its temporary package. Physical slot/off-state injection is unavailable through PC/SC; those router outcomes are covered by the route test. The pinned GPPro build must use `--reader=<exact name>`; an older deploy JAR with a separate `--reader` argument stalled before APDU dispatch. Implementation commit `10a3ab66`; progress/handoff record `c1861ebb`.

### RV-20261103-26

- [x] **P2: Lost final response can stall subsequent commands.**
  Owner: HOST-01. Evidence: Source; [review R26](docs/review_20261103.md#r26--failed-final-ccid-submission-can-leave-the-reader-unarmed).
  Acceptance: retain a pending final response for bounded retry, or transition to an explicit recoverable abort/reset state that safely rearms the endpoint. Test extension completion just after the wait bound, failed IN submission and subsequent slot-status/APDU recovery.
  Progress: Final CCID responses are retained while extensions complete; final IN submissions retry without blocking, with 1 ms spacing and an explicit host-retry abort after 32 submission failures. Bulk-OUT rearm rejection is also retried from the main task. Fixed asynchronous time-extension IN transfer failure: clear only the failed extension and preserve any final response queued behind it; keep Bulk-OUT disarmed until the final response completes. The shared recovery flow now passes 21/21, covering just-late extension completion, failed extension delivery, retry and subsequent slot-status/APDU dispatch after final response and bounded abort. ARM/RISC-V max_crypto builds pass. The final updated ARM firmware flashed to D92E743D5F640C0F and passed the finite monitor (`TEST:SUMMARY pass=1 fail=0`) and post-flash PC/SC lifecycle/routing test (`TEST:SUMMARY pass=36 fail=0`, cleanup pass). A transport failure was modeled through the exact production-shared recovery helper; hardware fault injection of a failed USB IN transfer was not performed.

### RV-20261103-27

- [x] **P2: Test compile drift plus wrong reselection/restore status.**
  Owner: PROFILE-01, GP-01. Evidence: Reproduced; [review R27](docs/review_20261103.md#r27--vpcd-session-tests-are-stale-and-mask-selection-regressions).
  Acceptance: restore the test's build, track the actual selected AID and retain meaningful restore failure status. Expand to two known applets, unknown AID, same-AID reselection, rejected `select()` and corrupted restore. Do not simply change assertions to the current wrong behavior. ,
  Progress: The stale session test constructor was restored; session state now tracks the selected AID, identifies same-AID reselection, and distinguishes missing applets (`6A82`) from restore failures (`6F00`). Tests cover two known AIDs, switching, same-AID selection, unknown AID, rejected `select()`, and corrupted restore. `./tools/test-vpcd-session` passes 29/29; ARM/RISC-V builds passed with the implementation. After rebuilding the max_crypto PC emulator, the production VPCD integration passed `TEST:SUMMARY pass=15 fail=0`: install/restore, applet SELECT (`9000`), GETVERSION `1.8.2_jc305`, JCSystem info, support probes, SHA-256 prepare/doFinal/static, AES/RSA/random/CRC16 and expected NO_SUCH_ALGORITHM. VPCD connected in 18.601 s; complete pass took 54.888 s. Earlier pre-connect timeout is superseded. No physical hardware was involved in this PC-only acceptance.

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

## Open architecture acceptance

### ARCH-08

- [x] **Measure and accept bounded native USB progress on hardware.**
  Status: accepted on 2026-11-02. Added post-operation telemetry for CCID
  native-progress duration, TinyUSB events processed, and time-extension
  submissions/completions. The physical test flooded standard USB device
  descriptor requests while Pico 2 `2BED26ED20556535` ran RSA-3072 generation
  and signature. The APDU returned `019000` in 118,055 ms; 4,336 control
  requests started during the 118,267-ms APDU interval and all succeeded. Five
  further requests passed after the APDU. The firmware measured 5,795 progress
  calls, 3,301 us total and 117 us maximum per call (about 0.57 us average);
  169 USB service passes used 1,006 us total and 94 us maximum, processed 86
  events total and at most two per pass (budget eight). All 85/85 CCID time
  extensions completed. No watchdog-recovery marker, `TEST:FAIL`, or `FAULT`
  appeared; the final response and temporary CAP cleanup passed.
  `./tools/test-ccid-progress-flow` passed 9/9. ARM and RISC-V wrapper builds
  passed before the physical run. The image ends at `0x100e7804`, leaving
  100,348 bytes in the firmware window.
  Remaining: none for this gate. The host flood recorded 5,608 successful
  requests total, zero failures, 7,999-us average and 331,254-us maximum
  transfer latency. Firmware progress timing is for the instrumented image.
  Evidence: `tools/test-ccid-progress-physical`,
  `artifacts/logs/ccid-progress-monitor-2BED26ED20556535-20261102T133921Z.log`,
  `artifacts/logs/asymm-crypto-pcsc-20261102T133921Z.log`, and cycle log
  `artifacts/logs/cycle-choupi-20261102T132946Z.log`.

## Priority 1 — storage, reclamation and recovery

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

### PERSIST-01

- [ ] **Finish acceptance before enabling normal persistent-object reclamation.**
  Status: owner-scoped root inventory, strict persistent-edge/alias decoding,
  completion reporting, and a bounded-batch reclaimer primitive are implemented.
  Static roots span installed packages; registered applet roots use stable owner
  IDs. Legacy records retain owner 0. Incomplete roots, malformed edges, cursor
  errors or transactions must block reclamation; provisional cursor results are
  discarded unless the final result is `Complete`.
  The primitive re-traces each batch and restarts its cursor after erasing;
  reachable tag subtrees and other owners are preserved. A test-only physical
  hook removed exactly one orphan body tag while preserving a rooted object.
  It is disabled by default and not wired into normal runtime collection.
  A fresh hook-enabled physical run on Pico 2 `2BED26ED20556535` now captured
  serial telemetry alongside the APDUs: after releasing the persistent array,
  `PersistentReclaimer` returned `Complete` (`result=0`) with exactly one
  unreachable owner tag erased (`candidates=1 erased=1`); subsequent root
  creation/read and package cleanup passed. Current-worktree reruns on
  2026-11-07 of `./tools/test-persistent-inventory` (22 cases) and
  `./tools/test-persistent-transaction` passed, including other-owner record
  preservation, incomplete-root fail-closed behavior, transaction refusal,
  and interruption/retry. The physical harness now requires
  matching erase telemetry, preventing a non-hook image from yielding a false
  reclaimer PASS. This closes the single-owner live erase check only.
  Remaining: physical multi-owner inventory and actual per-record power-loss /
  interrupted-reclaimer acceptance, including restart and retry. Retain stable
  Java Card handles and extend root audits if contexts/native owners change.
  The current-worktree `./tools/test-rp2350-fs-overlay` rerun passed on
  2026-11-07, including `persistent_reclamation_1000_power_cuts_recover_old_or_clean_state_and_retry`.
  This remains a host-model test, not a physical flash power-cut experiment.
  Rechecking after the JCSystem guard commit, persistent inventory passed all
  22 cases and both transaction/reclaimer and filesystem-overlay suites passed
  with `fail=0`. Physical multi-owner and power-loss gates remain open.
  A 2026-11-08 regression now injects erase failures after 0, 1, 7, 8, and 9
  successful deletions from a ten-tag owner set. Each new-Heap retry completed,
  and a neighboring owner's record survived every case. This covers both sides
  of the eight-tag batch boundary in the production reclaimer against the host
  filesystem stub; it is not flash power-cut or persistent-backend evidence.
  The RP2350 overlay power-cut harness now calls the production
  `PersistentReclaimer` for its staged deletion and retry. Across 1,000 COW
  snapshot cuts it reconstructs a fresh Heap from each recovered snapshot and
  checks a rooted `Object[]` plus its byte-array child and a neighboring owner's
  record, then retries any old-snapshot outcome through the reclaimer. The test
  uses the production inventory's ten serialized orphan-array tags.
  `./tools/test-rp2350-fs-overlay` passes with
  `TEST:METRIC persistent_reclamation_orphan_tags=10 cuts=1000` and
  `TEST:SUMMARY fail=0`. This remains host fake-flash evidence; it does not
  establish physical-flash or USB power-loss safety.
  Acceptance: incomplete reachability never authorizes deletion, all live roots
  and aliases survive, interrupted batches resume safely, and no other owner is
  affected. Do not enable persistent deletion before these gates pass.
  Dependencies: PERSIST-02/03. `JCSystem` object deletion stays unsupported.
  Evidence: original reclamation groundwork and primitive records; 1,000-cut
  fake-flash test invokes the VM reclaimer for ten staged erases, not physical
  power-loss behavior.

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
  Its expected applet version defaults to `1.8.2_jc305` and can be selected
  explicitly for other compatible upstream versions.
  Historical AC5 ordinary JCAlgTest power-cycle evidence is retained, but does
  not close these newer alias/reclaimer gates. Coordinate PERSIST-01 and AC-02.

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

## Priority 1 — current-image API acceptance

### HMAC-01

- [ ] **Complete `ALG_HMAC_*` acceptance in native crypto support.**
  Status: native HMAC-SHA1, SHA-256, SHA-384, SHA-512, MD5, and RIPEMD160
  signatures and HMAC key variants are implemented through the C++ VM/API
  dispatch path. All six RFC known-answer vectors pass in the CAP-only VPCD
  fixture, including split update, verify, tamper rejection, and key readback;
  a fresh current-worktree run on 2026-11-08 also passed the six HMAC algorithms
  in one-shot and split-update mode (12 vectors, 14/14 VPCD checks).
  Full JCAlgTest acceptance remains blocked by persistent-storage exhaustion
  during RSA CRT KeyBuilder probes; it is not a physical HMAC result.
  ARM and RISC-V max_crypto builds pass with 159/162 capabilities. The pinned
  JCAlgTest 1.8.4 CAP installs and runs on a clean VPCD card using GPPro. The
  first full support sweep exceeded the harness's 900-second timeout during
  RSA CRT key-size probes. A 3,600-second rerun completed the RSA CRT 3072-bit
  KeyBuilder APDU in 797,682 ms, then returned `SystemException_NO_RESOURCE`
  (`F205`). The emulator logged `PERSIST_EXCEPTION id=20` while serializing a
  persistent reference. The only reason-5 `NO_RESOURCE` mapping in that write
  path is filesystem status 1 (`OutOfFlash`), so the measured failure is
  persistent-storage exhaustion, not an unsupported algorithm or PC/SC
  transport error. `F206` is the separate expected status from optional
  `B0 E2` memory probes. The exact source of the long defragmentation time and
  accumulation of persistent records remain to be measured. Do not equate the
  PC VPCD FS layout (host OS filesystem with 512 KiB simulated flash) to the
  Pico backend (C++ copy-on-write overlay with a 96 KiB data arena); verify
  reclamation and capacity on the intended backend before claiming a fix.
  No physical HMAC test has run. Preserve the unchanged upstream CAP and keep
  all firmware implementation native; do not add Java providers/adapters or
  modify supplied CAPs. Acceptance: focused known-answer and lifecycle tests,
  complete pinned JCAlgTest 1.8.4 support comparison, both architecture builds,
  then physical validation when a reader is actually available.

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

### API-02

- [x] **Physically confirm the native Signature MAC IV correction.**
  Status: `runMac()` now uses the initialized Signature IV instead of always
  zero. Three deterministic vectors each for DES-MAC8-NOPAD, DES-MAC8-ISO9797-M2
  and AES-MAC-128-NOPAD pass; the symmetric VPCD harness passes 79 vectors.
  On 2026-11-02, flashed current ARM `max_crypto` UF2 to Pico 2
  `2BED26ED20556535`; the verified flash and finite monitor passed. The
  unchanged API 3.0.5 Crypto CAP then passed all three targeted Signature-IV
  rows as `OK` with expected output lengths (8, 16, 8 bytes) and removed the
  test CAP successfully. The other 463 rows were intentionally skipped.
  Remaining: include these rows in API-03's full coverage accounting; this
  focused run alone does not complete the full Crypto area.
  Evidence: `artifacts/results/Pico2_2BED_api305_signature-iv_maxcrypto_20261102.csv`
  and `artifacts/logs/cycle-choupi-20261102T123918Z.log`.

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

### EMU-01

- [x] **Complete full Crypto VPCD validation within a finite time window.**
  Status: the unchanged API 3.0.5 Crypto CAP completed all 466 calls against a
  fresh current-source max_crypto PC emulator over VPCD. From upload to cleanup
  it took 1:45:54 with a 14,400-second per-area timeout. Results: 304 `OK`, 161
  `UNSUPPORTED_PER_SPEC`, and one `EXC_CARDRUNTIME` (`0x0001`) for
  `ALG_RSA_SHA_512_PKCS1_PSS`. QKB documents why that row correctly rejects the
  upstream CAP's 1024-bit RSA-CRT key: its default 64-byte salt cannot fit the
  128-byte encoded message. There were zero `WRONG_RESULT`, transport errors,
  or incomplete calls. GPPro cleanup removed the Crypto CAP successfully.
  The earlier no-poller startup failed because the VPCD port refused the
  emulator; keeping `pcsc_scan` actively polling while the emulator and runner
  execute made the card present and the complete sweep pass.
  Remaining: none for EMU-01. Physical acceptance remains under API-03; this
  VPCD run does not replace it.
  Acceptance: fulfilled with fresh storage, exact reader pinning, bounded
  14,400-second test timeout, 466/466 results and successful cleanup. Partial
  runs remain incomplete. Never overlap runners on one reader. Coordinate
  physical retests with API-03.
  Evidence: `.agent/QKB.md` and `progress.md` record the exact result, elapsed
  time, CAP hash and CSV/log hashes.

## Priority 2 — applet contexts and profile behavior

### CTX-01

- [ ] **Complete cross-applet SIO lookup, proxy dispatch and firewall checks.**
  Status: selected plus up to 39 inactive contexts are retained by owner/full
  instance AID, preserve transient lifecycle across switches, and invalidate on
  successful DELETE. VPCD multipackage tests pass 47/47. Native lazy context
  resolution and previous-context AID reporting are implemented and tested.
  ROM `getAppletShareableInterfaceObject` dispatch now resolves an installed
  server, executes its Applet override in the retained context, and creates a
  separate client-heap placeholder with context-owned proxy metadata. The
  unchanged upstream probe passes null, unknown-AID and default-method cases.
  Proxy `invokeinterface` now checks the requested interface is marked
  Shareable, resolves the server method in its owning context, and forwards
  zero-argument calls. Same-context dispatch replaces the proxy receiver in
  place and uses the normal VM call frame, preserving all argument words; the
  unchanged upstream `AlgTestApiCore_305.cap` now completes its positive
  Shareable sub-interface call through VPCD, returning `1` as expected.
  Virtual/special calls and direct instance-field access
  on proxy handles are rejected; casts are limited to Shareable interfaces.
  Invocation also checks the actual server class against the requested
  interface before dispatch.
  Cross-context primitive and void returns are transferred by type. Null and
  JCRE-global references cross directly; server-local reference returns are
  wrapped in new client proxies only when the server class implements a
  Shareable interface. Same-context arguments stay in their owning heap;
  cross-context arguments are decoded from the interface CAP Descriptor
  signature when present, or from runtime word-kind tags when Descriptor is
  absent. Non-null private arguments cross as raw handles paired with their
  source-package owner tag; the receiver never treats the handle as local.
  Null and JCRE-global arguments retain their shared-heap behavior, and
  array-typed parameters require an array handle.
  Physical GPPro LOAD testing found `getDescriptor()==nullptr` for both
  pre-existing packages and fresh packages whose generated CAP archives include
  `Descriptor.cap`. This is confirmed GPPro behavior: pinned
  `GPSession.loadCapFile()` sends `CAPFile.getCode()`, and the local
  `capfile-26.02.22` implementation's default `_getCode(false)` excludes
  Descriptor (and Debug). The actual physical APDU stream was not captured.
  An initial receiver-only fallback for absent Descriptor data was superseded
  by the runtime type-tag path below. Present Descriptor signatures remain
  validated through the existing path; malformed Descriptor data never falls
  back to tags. The remaining physical acceptance gaps are listed below.
  Since standard GPPro LOAD omits Descriptor, the operand stack now maintains
  compact runtime word-kind tags (primitive, reference, int-low, int-high,
  return-address, unknown).
  Descriptor-free cross-context calls use these tags to construct typed
  initial locals, including ordered low/high int word pairing. A malformed or
  split int pair fails closed. Tags are maintained by typed
  pushes, local writes, frame-local initialization, `dup`, `swap`, and
  `set_Pushed_Element`; unknown values fail closed. Null and JCRE-global
  references may cross. Three-bit type-tag storage costs 96 bytes per 256-word
  Stack. A one-byte-per-word reference-owner sidecar follows references
  through generic and compact `aload`/`astore`, operand duplication and
  swapping, and typed nested-call locals; this adds 256 bytes per 256-word
  Stack. The receiver rejects foreign references on ordinary local reads and
  stack pops; array dereference bytecodes compare the retained owner to the
  active package and raise `SecurityException` on mismatch. A regression
  forces a private byte array through `astore_2`/`aload_2`, then verifies caught
  `SecurityException` on `arraylength`. VPCD passes 18/18 with Descriptor
  omitted and 18/18 with it present. The same three-package GPPro LOAD/INSTALL
  scenario passed on Pico 2 `8E464F4E2BBC0A19`, including private-array and
  global-Object[] denial and cleanup. This does not close the firewall: finish auditing every
  reference-consuming bytecode/native entry for owner enforcement, and test
  equal-handle collisions, storage/GC roots, stale owners and package deletion.
  Cross-context `areturn` now preserves owner provenance through the nested
  interpreter root result; references returned to their original package are
  restored as local references, while other-owner/global references remain
  opaque. The cross-AID fixture verifies byte-array identity and access after
  returning a client-owned array (18/18 with Descriptor absent and present).
  Opaque `ifnull`/`ifnonnull` and `if_acmp*` comparisons now retain provenance
  without resolving a foreign handle. Local markers normalize to the active
  package ID; equal compact handles from different owners compare unequal, and
  null/JCRE-global values share the global identity domain. VPCD exercises
  same-handle/null comparisons in the foreign service context (18/18 with and
  without Descriptor). Virtual calls and casts remain fail-closed for foreign
  private references; generated SIO tests verify foreign `checkcast` and
  `instanceof` raise catchable `SecurityException`. A two-array SIO probe once
  returned `6F00` with a suspicious second local, but the issue did not
  reproduce after rebuilding all fixture CAPs and using a distinct two-argument
  interface method; both Descriptor modes pass. Keep this path in regression
  coverage.
  `athrow` now checks reference provenance before resolving the object handle;
  otherwise a foreign caller exception could collide with a service-local
  compact handle and have its class read from the wrong heap. An owner mismatch
  dispatches a Java `SecurityException` through the normal catchable exception
  path. The cross-AID VPCD fixture verifies that a service catches this denial
  when asked to throw a caller-owned `RuntimeException`. Continue auditing
  every other reference-consuming bytecode/native entry and owner lifecycle.
  Persistent DELETE alias inventory now validates the complete deterministic
  foreign-static wrapper target encoding, matching the reclaimer inventory;
  malformed wrappers fail closed. Persistent-transaction and inventory unit
  suites pass. Physical DELETE/APDU validation remains open while PC/SC has no
  Pico readers.
  A follow-up audit found that reserved applet-owner IDs (`0xFE`, `0xFF`) were
  not rejected inside the deterministic target tag. The shared decoder now
  rejects them; persistent-transaction coverage confirms both DELETE alias
  scanning and reclamation inventory fail closed without candidates. ARM and
  RISC-V builds and a D92 finite runtime cycle pass. PC/SC still lists no Pico
  readers after the flash, so physical alias/APDU validation remains open.
  `Stack::get_Pushed_Element()` now remains potentially throwing when optional
  bounds checks are disabled, preserving catchable `SecurityException`; a
  compile-time no-checks contract test guards the exception specification.
  Dispatch now reads virtual/interface receivers through a typed accessor that
  rejects primitive and foreign-owner words before treating them as handles;
  the existing raw-word accessor remains for signature-checked argument
  marshalling. The registry test covers both rejection cases. Continue auditing
  every reference consumer and foreign-owner lifecycle path.
  Java Card RE 3.0.5 §6.2.8 permits ordinary foreign applet references to be
  retained in static fields, fields of an object owned by the active applet,
  and components of an accessible reference array. Only temporary JCRE entry
  point objects and global-array references are forbidden values for these
  stores. A cross-AID VPCD regression initially denied all three ordinary
  foreign-reference stores. Volatile applet-owned reference arrays now retain
  foreign references with per-slot owner provenance; root traversal routes
  them through the owning package. A retained-context lookup resolves the
  referenced object's class in its owner Heap, avoiding equal compact-handle
  aliases in the receiving package. JC 3.0.5 class/interface token metadata is
  generated from the API exports; a typed `Shareable[]` SIO regression exposed
  that constant-pool classrefs otherwise resolve interface tokens through the
  class map. Class/interface token overlaps are rejected by generation. The
  typed store passes with and without Descriptor data. Volatile ordinary
  instance fields now preserve foreign package provenance, route their edges
  into the owning package's GC roots, reject dereference in the receiving
  package, and clear edges before package-ID reuse. The focused regression
  confirms the foreign array survives GC; the complete cross-AID VPCD suite
  passes 35/35. Static foreign references and persistent foreign references
  remain fail-closed pending owner-aware roots and stable owner serialization.
  Static fields and persistent-array stores still need provenance and
  owner-aware GC/restoration support.
  Every later access must still apply firewall rules; never resolve a foreign
  compact handle in the receiving package's heap. See `.agent/QKB.md`.
  Global `Object[]` support is partially implemented for volatile JCRE global
  arrays: non-null applet references can be stored and read by their owning
  package, with per-slot package ownership, package-scoped GC roots, transient
  reset clearing, and package-delete cleanup. Case 41 verifies same-package
  store/read identity and returns `009000`; focused VPCD and physical Pico 2
  transaction probes passed. Cross-package `aaload` now returns the compact
  value with its original package owner tag. Ordinary reference consumption
  and array dereference reject that foreign owner before consulting the current
  package heap. A three-package regression stores the client instance in a
  global Object[], reads it in the service package, and verifies cross-package
  use is caught as `SecurityException` (`6B31`); VPCD and physical Pico 2 runs
  passed. This preserves opaque identity only while provenance stays attached
  to the VM stack/local path. Java Card specifies that global-array references
  cannot be stored in class fields, instance fields, or array components. The
  old fixture that cached one in a static field and later hit
  `RESTORE_READ_FAIL` (`tag=023904`) exercised invalid CAP behavior, not a
  missing persistent encoding. The current static, instance, and array-element
  guards raise `SecurityException`; the three-package regression verifies
  these denials while continuing to pass the global array as an SIO argument.
  This global-array retention rule is accepted; the separate foreign-reference
  lifecycle and GC-root audit remains open.
  The three-package SIO fixture also compares a client-owned applet reference
  read from a global `Object[]` with the service applet's local `this` before
  attempting virtual dispatch. This exercises cross-owner object identity
  separately from foreign-vs-null and same-foreign-reference checks.
  The VPCD integration fixture now proves the equal-handle case directly: the
  caller's foreign applet and service's local applet both use compact handle
  `0002`, with package owners 58 and 57, and compare unequal. A PC-only opt-in
  trace asserts that the integration test actually exercises this collision.
  A VPCD GP DELETE/reinstall regression now confirms that cached proxies for
  the deleted server are invalidated across client contexts: the old proxy
  fails closed after reinstall under the same AID, while a fresh lookup works.
  Remaining: additional stale-owner lifecycle cases and audit all paths that
  can consume or retain a foreign value. See `.agent/QKB.md`.
  No types are inferred from raw values. The
  descriptor-free VPCD fixture passes 18/18; the Descriptor-present regression
  also passes 18/18 through the existing signature path. Physical typed
  cross-AID execution for the private-array argument passed after flash and
  monitor. Typed local reads now check their
  stored kind and throw `SecurityException` on a mismatch, so caller-derived
  metadata is checked against the server bytecodes' actual local use. `astore`
  preserves a return-address tag for valid `jsr`/`ret` use and rejects other
  kinds. Typed operand pops now validate primitive, reference, ordered int, and
  return-address tags before consuming words, preventing malformed bytecode
  from relabeling a reference as a primitive. The VPCD multipackage suite
  passes 47/47 with these checks enabled.
  The local signature-to-typed-local conversion has focused tests for
  byte/short layout, int word order, null/non-null references, and argument
  count mismatch. A new test-only three-package fixture now passes a positive
  cross-AID VPCD call with byte/short arguments, a global AID argument, and a
  null array; it verifies `getPreviousContextAID()` and the server AID and
  returns `4567`. It also verifies denied lookup returns null, opaque
  applet-local references pass identity/null comparisons while array
  dereference and virtual dispatch fail closed, and a cross-context int
  argument and return round-trip produces `76543210`. A server-thrown SecurityException is
  caught by the client, and a subsequent SIO call succeeds. A proxy stored in
  an applet instance field survives the end-of-APDU GC and is reusable on the
  next APDU. Calling `Object.equals` directly on a proxy fails closed with
  `SecurityException`. A fresh lookup works after emulator process restart;
  an old persisted proxy loses its native routing metadata. The instance header
  now stores an internal Shareable-proxy marker; restoration recreates that
  marker, and the VM rejects marked placeholders absent from the runtime
  registry across interface/virtual/special calls, casts and instance-field
  operations instead of invoking them in the caller context. VPCD confirms the
  stale placeholder is fail-closed after process restart and VPCD card reset.
  Persistent object aliases now also preserve the internal proxy marker: an
  alias restored before its canonical target can no longer become an unmarked
  object and dispatch in the client context. The three-package VPCD fixture
  stores the same proxy in two fields and verifies that reading the alias first
  after restart fails closed. This establishes the documented fail-closed
  persistence boundary; routing metadata is not rehydrated. On
  physical 2BED `2BED26ED20556535`, full-index compaction now succeeds and
  GPPro INSTALL for the already-loaded client and service applets returns
  `9000`. The cross-AID probe selects the client, but its SIO APDU returns
  `6F00` with `SecurityException` at `invokeinterface` before flashing the
  current source. A clean fixture on physical Pico 2 `8E464F4E2BBC0A19` then
  reproduced a reset boundary bug: cross-AID SIO returned `45679000`, and a
  retained proxy still worked after PC/SC reset. `choupi_vm_clear_selected_applet()`
  cleared transient arrays but kept runtime proxy routes and server roots. It
  now clears those routes/roots across selected and inactive applet contexts;
  the persistent proxy placeholder stays marked and fails closed. After
  rebuilding and flashing that source, the clean physical fixture returned
  `45679000` for SIO and `6A0A9000` for the retained proxy after reset. Integer
  argument/return, exception unwind, denied lookup (`6A019000`), applet-local
  reference denial (`6A039000`), proxy method firewall (`6A099000`), and cleanup
  also passed. The earlier unknown-package failure is not current evidence.
  A fresh external-interface VPCD fixture passes a valid exported interface
  install and rejects a class/missing export (3/3).
  The unchanged same-applet
  call still passes on physical Pico 2 `8E464F4E2BBC0A19`. Never return raw
  server heap references to clients.
  Acceptance: physical positive cross-AID and reset-stale-proxy fixtures now
  pass on channel 0. Remaining: same-CAP/multi-package physical lifecycle,
  stable roots across GC and reset/delete, no cross-heap
  reference leaks; retain the tested fail-closed persistence boundary or add
  standards-compatible proxy rehydration; then unchanged upstream Multi CAP
  integration and physical checks.
  General cross-context calls and logical-channel policy remain unimplemented;
  basic channel 0 is the current assignment. No blanket firewall acceptance yet.
  The GP DELETE/stale-proxy defect is fixed: proxy cast checks now distinguish
  an unavailable/deleted server from an incompatible cast target. The former
  raises catchable `SecurityException`; the latter remains `ClassCastException`.
  VPCD passed 20/20 with Descriptor omitted and included. Physical Pico 2
  `8E464F4E2BBC0A19` passed cross-AID calls, GP DELETE, stale-proxy rejection
  (`6A0A9000`), fresh-lookup denial (`6A049000`), and fixture cleanup. See
  `.agent/QKB.md` and the 2026-11-03 progress record. Other CTX-01 lifecycle,
  GC-root, multi-package, and channel-policy acceptance remains open.
  Same-AID reinstall coverage is now included: the VPCD test reselects ISD,
  opens a fresh SCP03 channel, reinstalls the deleted service applet, confirms
  it is selectable, verifies the old client proxy remains blocked, and checks
  a fresh lookup. This caught and fixed proxy-route invalidation that previously
  only ran in the deleted server's own cached Context. PC, ARM and RISC-V builds
  pass; VPCD summary is 25/25. Physical same-AID reinstall remains unverified.
  Latest full three-package cross-AID regression passes 34/34 with Descriptor
  omitted (matching standard GPPro `getCode()` behavior) and with Descriptor
  included. Descriptor-free dynamic method bounds are inferred from verified
  callable entry offsets in the Applet, Class, internal static-method
  references, and Export components; the method range ends at the next known
  entry or Method-component end. Unknown, malformed, or unlisted method starts
  still fail closed. This resolves the prior VPCD service INSTALL `6A80`; it
  does not widen execution to the whole Method component. A physical
  Descriptor-free run has not yet been repeated on firmware containing this
  fallback: the earlier Pico 2 failure (`D92E743D5F640C0F`) predates the fix.
  A negative INSTALL fixture now redirects the Applet entry to a valid-header-
  looking byte inside the install method body; both Descriptor modes reject
  it with `6A80`. Preserve both Descriptor-present and Descriptor-absent
  acceptance cases. A second Descriptor-free negative fixture zeroes a Class
  method-table offset: LOAD succeeds and INSTALL rejects it with `6A80`.
  A zero internal `StaticMethodref` offset also LOADs but INSTALL rejects it
  with `6F00` at stage-9 package-import validation (no Java Card exception).
  A test-injected zero Export static-method offset is rejected at INSTALL with
  `6A80`. A generated CAP with its sole empty Export class record removed,
  Directory size updated, and Export `class_count=0` LOADs and INSTALLs
  successfully with Descriptor omitted and included (focused VPCD passes 1/1
  in each mode); the default cross-AID suite passes 34/34. Whole-entry
  Class-table omission now also LOADs and is rejected at INSTALL with `6F00`
  in both Descriptor modes. A generated CAP with an empty ConstantPool also
  LOADs and is rejected at INSTALL with `6F00` in both Descriptor modes;
  a separate valid client package installs successfully afterward in both
  modes. Whole-entry ConstantPool omission and post-failure card-manager
  recovery coverage are complete. Full physical GPPro revalidation remains
  open.
  The [Java
  Card 3.0.5 VM specification, §6.10 and §6.14](https://docs.oracle.com/cd/E59935_01/JCVMS.pdf)
  makes Descriptor loading optional and provides exact method offsets and
  bytecode counts there; the Method component alone does not carry exact body
  lengths. The current default VPCD script has a 30-second APDU deadline that
  is too short for this VM; use 90 seconds to connect and 120 seconds per APDU.
  The repeatable entry point is `PICO2_HOST_USB_ACCESS=1
  ./tools/test-sio-cross-aid-vpcd`; the latest default Descriptor-omitted run
  passed 34/34, and the Descriptor-included pass remains recorded separately.
  A new test-only SIO method passes a caller-owned byte array into native
  `Util.arrayCopyNonAtomic`. Generated JNI dispatch rejects the foreign owner
  before resolving the compact handle in the service heap; the applet catches
  `SecurityException` and returns `6A269000`. VPCD passes with Descriptor
  omitted and included, and physical Pico 2 `8E464F4E2BBC0A19` passes the same
  APDU and full fixture cleanup. This covers one native reference-consumer
  path, not the remaining native APIs or the full lifecycle audit.
  Evidence: current HANDOFF, `tools/test-rmi-registry`, context/proxy progress
  records. Preserve unrelated ongoing source changes while continuing this work.

  Update 2026-11-06: Typed foreign-reference-array storage now has a negative
  three-package SIO case. An incompatible foreign applet object passed through
  an `Object` interface parameter and stored into a runtime `Shareable[]` via
  an `Object[]` alias raises catchable `ArrayStoreException`; the compatible
  retained proxy case still succeeds. Full cross-AID VPCD passes 35/35 both
  with Descriptor omitted and included. This is VPCD only; physical validation
  remains pending.
  CTX lifecycle audit: applet-instance DELETE now checks volatile cross-package
  reference arrays before committing deletion. A retained reference to an
  object owned by the target instance returns `6985`; after the retaining applet
  clears the slot, DELETE returns `9000` and a sibling instance remains usable.
  The focused three-package VPCD regression and full `symm_crypto` cross-AID
  suite pass. Native coverage also confirms same-package sibling arrays and
  cached static roots to applet-owned state block DELETE until cleared; the
  guard inspects loaded sibling object fields and persistent reference arrays.
  Package-static objects tagged in namespace `0x02` do not block DELETE by
  allocation owner alone because that state survives deletion. Persistent
  aliases in that namespace which point into a target applet's `0x03` records
  now block DELETE. A cold-start VPCD case returns `6985` before either client
  applet is selected; clearing the alias through the same-package sibling
  permits DELETE `9000`, and the sibling remains usable. The inventory also
  follows aliases from unselected `0x03` sibling records. Keep CTX-01 open for a
  broader Java Card deletion semantics. A cold-start VPCD APDU test now stores
  the target reference in an unselected sibling's instance field, restarts the
  emulator, verifies DELETE returns `6985` before selecting either client
  applet, clears the field, then verifies DELETE returns `9000` and the sibling
  remains usable. A second cold-start case stores the target in a sibling-owned
  persistent `Object[]`; DELETE remains blocked until the array element itself
  is set to null, after which DELETE succeeds and the sibling remains usable.
  Interpretation: Oracle's Java Card 3.0.5 [removable-applet guidance](https://docs.oracle.com/javacard/3.0.5/prognotes/JCPCL.pdf)
  states that applet deletion fails when an object owned by the target applet is
  referenced by an object owned by another applet. The persisted array element
  remains such a cross-applet reference after its containing field is cleared;
  this test does not establish that the serialized array object has been
  reclaimed. Keep the fail-closed DELETE guard. Supporting reclamation of that
  object requires the separate PERSIST-01 object-deletion safety gates; do not
  ignore the alias to make DELETE succeed.

  Persistent-tag inventory now also treats foreign-reference wrappers in any
  package's static namespace as roots of their owner-applet target subtree. A
  cleared wrapper makes that target eligible for the existing unreachable-tag
  reclaimer; the reclaimer remains disabled in normal post-APDU collection
  until PERSIST-01 power-loss and multi-owner safety gates pass.

  Update 2026-11-08: ordinary foreign applet object references in package
  static fields now carry their owner through `getstatic_a`/`putstatic_a` and
  persistent storage. A wrapper record points to an owner-applet-scoped target
  record; GC roots are routed to the owning package, and persistent DELETE
  inventory follows the wrapper target. Static foreign arrays, global/private
  proxy values, and referenced objects that themselves contain foreign field
  edges remain fail-closed. The three-package VPCD suite passes 35/35,
  including static object identity after emulator restart, normal deletion of
  the service applet, and stale proxy rejection. A deletion scan bug found
  during this work resolved foreign static handles in the referencing heap;
  owner filtering now prevents equal compact handles from making the wrong
  applet look referenced. ARM and RISC-V current-profile builds pass. The full
  ARM firmware was flashed to Pico 2 `D92E743D5F640C0F`; its 18-second monitor
  repeatedly reported `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1
  fail=0`. PC/SC still reports no readers after access refresh, so no physical
  CAP installation or static-reference APDU was possible. Keep physical
  acceptance open, and audit clearing/replacing static foreign references for
  orphaned owner-scoped records before broadening this support.

  Update 2026-11-08: clearing the foreign static, instance, and array fields
  exposed a deletion false positive: an unrooted volatile array remained in
  the shared foreign-array registry because its owning retained context had
  not run GC. The DELETE preflight now traces and collects every selected and
  inactive applet context before checking live cross-applet edges; persistent
  alias inventory still runs and blocks real persistent references. A focused
  VPCD regression clears the edges and deletes the former owner applet with
  `9000`; the full max_crypto cross-AID suite passes 35/35. ARM/RISC-V builds
  pass and the current ARM image was flashed to D92 with a passing finite
  runtime monitor. PC/SC still publishes no readers, so physical APDU
  acceptance remains open.

  Static-reference lifecycle follow-up: replacing a foreign-static wrapper
  with a reference owned by a different applet leaves the old owner-scoped
  target and nested records unreachable; inventory offers exactly those old
  records as reclaim candidates while keeping the replacement target rooted.
  Clearing the replacement wrapper likewise exposes its target subtree.
  `./tools/test-persistent-transaction` covers both cases. This is inventory
  validation only: automatic persistent reclamation remains disabled, so the
  orphan records are not erased during the static assignment.

  Physical follow-up on 2026-11-08: all three boards (D92, 2BED, 8E46) were
  flashed with the current ARM image and passed finite runtime monitors.
  `tools/test-sio-cross-aid-physical` then passed its APDU assertions on each
  board, including SIO, owner firewall, SensitiveResult, GC, DELETE and
  same-AID reinstall. The physical probe's expected `6B43` was stale; the
  fixture and VPCD suite expect `6B40` when all four retention checks succeed,
  and the physical probe now matches. Cleanup exposed a real package-static
  dependency: remove the reinstalled service package before deleting the
  client applet, because the service static field may retain a client object.
  Cleanup order was corrected. On 8E46 the reader disappeared during cleanup
  with `SCARD_E_NOT_TRANSACTED` after the service package removal; D92 and 2BED
  readers also stopped publishing afterward. The physical suite's final cleanup
  and a clean registry are still unverified; retry when PC/SC readers return.

  Audit follow-up: proxy creation retains a server object root before allocating
  and registering the client proxy. If a later step fails, a newly inserted
  root may remain until reset or applet deletion. Implement rollback that only
  removes a root newly added by that attempt, and test proxy-table exhaustion
  plus allocation failure.

### CTX-02

- [ ] **Complete Java-level SensitiveResult coverage across SIO calls.**
  Status: same-context assertions and a native two-Context regression pass for
  boolean/short/object results, GC rooting, and reset rejection of stale state.
  The unchanged API has now also been exercised through interpreted three-
  package CAPs: the service applet's successful `Util.arrayCopyNonAtomic`
  result is asserted by the caller after the SIO context switch, and an invalid
  service copy clears the result so the caller's assertion throws
  `SecurityException`. The caller's object result from `JCSystem.getAID()` also
  survives a passive SIO call, and the service successfully asserts its
  `getAID()` and `getPreviousContextAID()` object results. The VPCD cross-AID
  suite passes 31/31 with these cases and the existing lifecycle/firewall
  regressions. This is emulator evidence, not physical acceptance.
  This exposed and fixed a runtime omission: native
  `MessageDigest.doFinal()` returned the correct SHA-256 length but did not set
  `SensitiveResult`. It now clears stale state at entry and records the output
  length only after digest and cleanup succeed. Success and invalid-range
  behavior both pass the 31/31 VPCD fixture suite; the SHA-256 output is checked
  against a known answer.
  Native `Cipher.update()` and `Cipher.doFinal()` now also clear stale state on
  entry and record the short output length on success. An interpreted AES CBC
  fixture asserts update's result in the service and finalization's result
  after SIO return; VPCD remains 31/31. The current device pool is empty, so
  physical validation is outstanding.
  Native `Signature.sign()` variants now publish their output length, while
  `verify()` variants publish their boolean result only after success. The
  interpreted fixture uses AES MAC sign/verify and asserts both true and false
  verification results across SIO. The VPCD suite passes 31/31, including this
  added false-result case. PC, ARM, and RISC-V builds passed for the preceding
  native implementation; physical validation is still outstanding.
  `JCSystem.makeGlobalArray()` now resets stale state and publishes its returned
  global array as an object result. A three-package interpreted fixture returns
  a global byte array from the service and the caller asserts its identity after
  SIO; the isolated VPCD case passes. The full cross-AID suite passes 34/34 on a
  rebuilt `symm_crypto` PC VM. Its AES-MAC Signature cases are not supported by
  `current`, which correctly returns `CryptoException.NO_SUCH_ALGORITHM`; use
  `symm_crypto` or `max_crypto` for this suite.
  Native `KeyBuilder.buildKey()` overloads now clear stale results at entry and
  publish the created key object only after construction succeeds. An
  interpreted SIO case asserts the successful key identity and verifies an
  invalid key type clears the previous object result. PIN and OwnerPINx
  `invokeinterface` results are now captured at the interpreted callee return:
  `getTriesRemaining`, `check`, `isValidated`, `getTryLimit`, and
  `decrementTriesRemaining` are asserted in the service and after SIO by the
  caller. Focused and full VPCD coverage pass; PC `symm_crypto`, ARM `current`,
  and RISC-V `current` builds pass. No physical device was used.
  Concrete `OwnerPIN` `invokevirtual` calls for `getTriesRemaining`, `check`,
  and `isValidated` are now also covered with interpreted service-side
  assertions and SIO return. Both focused and full VPCD runs pass.
  `RandomData.getAlgorithm()` and `nextBytes()` now also publish their short
  results on successful concrete-class `invokevirtual` return; both results
  are asserted by the caller after SIO. Focused RandomData VPCD and full 34/34
  cross-AID suite pass; PC `symm_crypto`, ARM `current`, and RISC-V `current`
  builds pass. No physical device was used.
  `SensitiveArrays.isIntegritySensitive()` and
  `isIntegritySensitiveArraysSupported()` now clear stale state and publish
  their correct `false` results from native code; the interpreted service and
  SIO caller assert both. Focused and full 34/34 VPCD coverage pass, as do PC,
  ARM and RISC-V builds. No physical device was used.
  The interpreted service also asserts the boolean SensitiveResult values from
  `AID.partialEquals()`, both `AID.equals()` overloads, and `AID.RIDEquals()`
  true/false outcomes; focused AID and full cross-AID VPCD coverage pass. This
  extends the existing `JCSystem.getAID()` and `getPreviousContextAID()`
  object-result checks. No runtime source change was needed for these API
  results. The currently supported SensitiveResult producers now have
  interpreted SIO coverage, including `buildKey()` and the atomic Util
  `arrayCopy()`, `arrayFill()` and `arrayCompare()` results. The atomic Util
  fixture also verifies an invalid copy clears prior state and the final
  comparison result is assertable by the caller after SIO. Another case proves
  the caller can assert the returned value, invoke `SensitiveResult.reset()`,
  then observe the result as unassigned.
  The current API has no supported Checksum provider (both factory branches
  return `NO_SUCH_ALGORITHM`) and no enabled AEAD provider; those results are
  not current producers and remain outside this coverage until separately
  implemented/enabled. The native heap regression confirms a
  SensitiveResult object is the sole VM root after ordinary frames are gone,
  and that reset permits collection. Interpreted SIO propagation/assertion is
  covered separately; the public Java Card API has no synchronous GC trigger
  to combine those phases in one CAP call. Broaden physical SensitiveResult
  acceptance beyond the JCSystem probe already run. The exact-reader physical
  probe now includes AID, atomic Util and explicit reset APDUs, but its latest
  attempt stopped before card traffic because PC/SC no longer registered any of the three
  still-enumerated Pico USB CCID devices; see `.agent/QKB.md`.
  Acceptance: correct result ownership/lifetime across cross-applet call/return,
  GC and reset; preserve tests distinguishing native simulation from applet calls.
  Evidence: original SensitiveResult task and `tools/test-rmi-registry`.

### CTX-03

- [ ] **Use one firewall context per CAP file while retaining per-applet ownership.**
  Status: package-level Context lookup is now wired by package ID. One Context
  retains sibling AID/lifecycle/root records; owner-specific Heap facades share
  handle tables and roots while Java objects preserve their allocating owner.
  SELECT and SIO lookup activate the target sibling state, and SIO restores the
  client state on return and exceptions. Instance deletion forgets only the
  deleted AID and traces the shared heap; package deletion drops the package
  Context. The same-CAP VPCD fixture now passes 6/6, including a sibling SIO
  call receiving the shared static array, default/non-default statics, writes,
  and retained-sibling visibility. The cross-AID SIO VPCD regression passes
  15/15, including context restoration after exceptions and fail-closed stale
  references. The three-package install/restart/delete/transient suite passes
  47/47. RMI exports, SIO proxy mappings, and server roots now live in each
  applet-instance state; deletion removes that applet's entries and invalidates
  sibling proxies targeting its AID. RMI registry tests prove sibling
  isolation, per-applet ID reuse, DELETE root removal, and proxy invalidation.
  A same-process VPCD delete/reinstall test now proves a deleted owner ID is
  skipped while a package-shared object allocated by that owner remains live:
  sibling owners 1 and 2 become 3 and 2 after deleting and reinstalling owner
  1, and the reader still sees the shared array. The PC backend and RP2350
  allocator both consult retained VM owner state. RMI registry test, filesystem
  overlay suite, same-CAP delete/reinstall VPCD (9/9), and multipackage VPCD
  (47/47) pass; PC, ARM and RISC-V max_crypto builds pass. The same-CAP
  fixture explicitly calls `getAppletShareableInterfaceObject` from the reader
  sibling and verifies the returned writer proxy sees the identical package
  static byte array; `tools/test-same-cap-siblings` passes 6/6. Latest ARM
  image ends at `0x100e7890`, using 948,368 of the 1 MiB firmware window and
  leaving 100,208 bytes. A physical same-CAP sibling check on Pico 2
  `8E464F4E2BBC0A19` passed: GPPro LOADed one CAP and CREATEd both applets,
  static defaults were `000012340056123456789000`, a writer update returned
  `000012350157123456799000`, and the sibling reader observed the update.
  Shareable array identity returned `019000`; cleanup removed both instances
  and the package. A second physical run also deleted and recreated the reader
  applet from the loaded CAP; its reinstalled instance read the writer-updated
  package static values (`000012350157123456799000`). Instance DELETE/recreate
  is physically covered. A third run deleted both applets and the whole
  package, LOADed the CAP again, CREATEd both applets, and confirmed initial
  static values and sibling SIO identity after package reinstall. A separate
  physical transient fixture passed on this board: switching to the sibling
  cleared `CLEAR_ON_DESELECT` (`00`) and retained `CLEAR_ON_RESET` (`6B`), then
  PC/SC reset cleared both (`0000`). The same physical results were reproduced
  on Pico 2 `2BED26ED20556535`: `5A6B9000`, `006B9000`, then `00009000`; cleanup
  passed on both cards. The VPCD transient suite passes 5/5. The unchanged RMI
  array fixture now also allocates 256 discarded 64-byte arrays per call. Four
  consecutive pressure APDUs, then the array and exception cases, passed
  physically on both Pico 2 `8E464F4E2BBC0A19` and `2BED26ED20556535`; cleanup
  passed. VPCD reports the first collection with 5/261 arrays reachable and 256
  reclaimed, and all four pressure calls pass. This validates repeated
  allocation/collection under APDU load; cold-power persistence remains open.
  The first probe omitted the explicit
  reader reselect used by the VPCD test; after aligning the APDU sequence,
  physical results matched VPCD.
  The complete same-CAP selection/instance-delete/package-delete/reinstall
  sequence was repeated on `2BED26ED20556535` with the same status words and
  values and successful cleanup. Its installed image hash was not captured, so
  this is a device-behavior comparison rather than a same-image comparison.
  Two repeated instance DELETE/reinstall cycles keep array-owning owner 1
  reserved while reclaiming object-free owner 3; full package DELETE/reinstall
  releases and reuses owner 1. Same-CAP transient tests show sibling selection
  clears `CLEAR_ON_DESELECT` while preserving `CLEAR_ON_RESET`, and card reset
  clears both; the same behavior has now passed on physical hardware. The owner
  lifecycle suite passes 9/9 and transient suite 5/5.
  A focused cross-AID regression additionally verifies that a selected caller
  can allocate CLEAR_ON_DESELECT arrays and that an SIO-invoked applet in a
  different, non-selected CAP context receives ILLEGAL_TRANSIENT for all five
  array types; CLEAR_ON_RESET allocation remains permitted. The selected
  caller/sibling-context VPCD check passes 2/2. The same-CAP sibling fixture
  now allocates CLEAR_ON_DESELECT from the SIO-invoked writer and passes 6/6,
  directly confirming the shared package context permits it. Physical
  confirmation remains part of API-03 acceptance. The full cross-AID VPCD
  suite passes 35/35 after the guard, including owner-tagged references,
  native array-copy enforcement, proxy deletion/reinstall, and reset behavior.
  The transient-allocation failure suite also passes in int-disabled and
  int-enabled builds, covering negative lengths, exact exception reasons,
  backing limits, and local/global handle exhaustion.
  A same-CAP restart VPCD test passes 7/7: both applet registrations restore,
  sibling static scalar/array state survives emulator restart, and SIO resolves
  the same restored array identity. The multi-package restart suite passes
  47/47.
  The native registry test retains a live array for every owner ID 1..255,
  then confirms reclamation after GC. Fake-flash CAP metadata tests allocate
  IDs 1..255 across packages, reject the next allocation, and reuse ID 1 after
  deleting its instance. PC/VPCD and RP2350 now share the same tested
  first-free owner-selection helper; tests cover a completely occupied 255-ID
  bitmap, lowest-free selection, retained-object exhaustion and reuse after
  GC. PC/VPCD delete/reinstall integration passes 9/9. Deserializing a full
  255-owner VCR2 registry has not been exercised. Review remaining GC/reset
  paths, then complete persistence and physical acceptance.
  Acceptance: an unchanged or test-only multi-applet CAP proves sibling
  instances share static/heap context and can access objects as specified,
  while separate CAPs remain isolated; AID reporting and per-instance DELETE
  preserve only the intended applet state. Include multi-package and restart
  regressions before physical testing.
  Dependencies: CTX-01, STORE-01, PERSIST-01/02. See `.agent/QKB.md`.

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

## Priority 2 — card manager, access and integration

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

### HOST-02

- [ ] **P1 / high priority: remove the serial-console wait from normal firmware;
  make startup capture an explicit diagnostic build option.**
  Status: implementation added. Normal ARM startup no longer waits for CDC/DTR
  or sleeps an extra 500 ms; it services USB once and enters filesystem/VM
  startup. `PICO2_DEBUG_SERIAL_WAIT_MS` defaults explicitly to zero, accepts
  bounded values through 120000, and services USB during diagnostic waits.
  Diagnostic images receive a duration-specific filename, and build
  provenance records profile, mode, setting, image and support-config hashes.
  Validation completed: ARM and RISC-V max_crypto normal/30000 ms diagnostic/
  normal builds passed in the same build directories; both CMake caches end at
  zero. Physical ARM runtime cycle and read-only GPPro SELECT/CPLC/Card Data
  checks passed on D92E743D5F640C0F. The normal Windows release UF2 is packaged
  with erased storage. The 30000 ms diagnostic image was flashed and its finite
  serial cycle passed (`TEST:SUMMARY pass=1 fail=0`), establishing DTR-triggered
  capture. The user reports the no-delay image became visible under Windows
  certutil almost instantly after replug. Exact timings, image hash used for
  that Windows run, and reader-list measurement method were not supplied.
  Remaining: run `tools/measure-windows-reconnect.ps1` on Windows for at least
  three cold reconnects on the current image and the original image, with all
  serial/COM clients closed. The script records the Windows PnP-arrival event,
  PC/SC reader-list latency, read-only ISD SELECT latency, image hash, reader,
  OS and driver state to CSV. Script execution and timing remain unverified on
  Windows; this Linux VM has no PowerShell runtime.
  Current rebuild: ARM and RISC-V max_crypto normal builds passed with
  `PICO2_DEBUG_SERIAL_WAIT_MS=0`. Fresh ARM UF2 SHA-256 is
  `fca53590c3631c8c69f529a8b4a861d63d621c85dab1dfcefb46963fed177654`;
  packaged empty-card UF2 SHA-256 is
  `2578fa07aa5d6dc92e1eaed947774964bdde40c3eb4f596f8841e387b9058a9c`
  (4,036,608 bytes). All three identified boards pass an eight-second serial
  runtime monitor on the current attached firmware. The package script filled
  flash storage at `0x10100000..0x101FFFFF` with `0xFF`; no token was flashed
  during this rebuild. A fresh 30,000 ms diagnostic ARM/RISC-V build passed;
  its image was flashed to D92, left without a serial client for more than 32
  seconds after the flash command completed, then reached the runtime loop
  when monitored (`TEST:SUMMARY pass=1 fail=0`). D92 was restored to the
  zero-wait empty-card image, and both architecture builds were returned to
  zero and passed again. Exact no-console Windows timing remains outstanding.
  Acceptance: build ARM/RISC-V in normal and diagnostic modes, then switch
  back to normal in the same build directories and verify the setting resets.
  Diagnostic mode exits on DTR or its deadline; normal mode reaches runtime
  without DTR or intentional console sleeps. Flash the identified ARM image
  only after both architecture builds pass; require finite serial PASS/no
  FAULT and successful PC/SC/APDU checks. Separately measure at least three
  Windows cold reconnects with **no serial terminal or monitor open**, plus
  record image hashes, reader-list method, timing, and remaining host overhead.
  A monitor-assisted cycle alone cannot close
  the no-console startup gate. Firmware should impose no 30-second delay;
  do not claim an exact Windows readiness bound before measurement.
  Agent instructions: [normal and diagnostic build workflow](docs/usb-startup-diagnostics.md).
  Related: HOST-01 (reader disappearance), AC-01 (Windows acceptance).
  Evidence: `src/choupi_pico2.cpp`, `tools/build`, CMake configuration, and
  `docs/usb-startup-diagnostics.md`. Do not close until validation gates and
  external Windows timing measurement are recorded.

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

## Deferred and conditional work

### REL-01

- [x] **P1: Complete REL-01 hardware/GPPro acceptance.** Source implementation:
  CPLC `OperatingSystemReleaseLevel` uses four hex digits encoded as major
  nibble, minor nibble, update byte; the explicitly requested 0.1.1 is `0101`.
  Ordinary builds must not change it. `tools/build` updates the four-byte
  `ICPersonalizationEquipmentID` on each firmware build: leading `00` means a
  clean tree, `FF` means dirty, followed by the first six hex digits of HEAD.
  The dirty marker prevents the ID from implying that an uncommitted image is
  exactly the named commit. The CPLC unit and hardware probe expectations are
  aligned. Two consecutive 2026-11-07 ARM max_crypto builds previously emitted
  `0xFFDCFCC7`; the 2026-11-07 ARM and RISC-V max_crypto wrapper builds passed,
  and ARM embedded source ID `FF1DC424` for that dirty tree. Physical board
  `D92E743D5F640C0F` was flashed with the firmware-only ARM image, passed the
  finite serial runtime check, and physical GPPro `--info -d` returned release
  `0101`, CPLC date `2026-11-07`, and equipment ID `FF1DC424` (`9000`). The
  latest refreshed empty-card external image was rebuilt on 2026-11-08 as
  `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`
  (4,079,104 bytes; SHA-256
  `d5837c6fc890c4298b18cbc028e2a8a5448cf3f7ccbdff01e8ee1153da7320dd`). It
  enables 159/162 capabilities, embeds release `0101` and build marker
  `FF735A76`, ends at `0x100F1D60` with 58,016 bytes remaining, and contains
  4,096 erased storage blocks. Its firmware payload UF2 is 1,930,240 bytes,
  SHA-256 `b2dac6fff57280c25e0b2bb118d2815f52adc0d8f64063e80e5a67bf4a69690c`.
  This refreshed image has not been flashed; the physical REL-01 proof above
  applies to the earlier firmware-only build.
  CPLC unit checks pass 25/25. GPPro `--info` on VPCD decodes release `0101` and the CPLC date
  `2026-11-07`; the direct GP GET DATA probe also passes. The latest helper
  granted USB nodes `001/108`, `001/109`, and `001/111`, all runtime `2e8a:0009`;
  `pcsc_scan -r` lists only VPCD and no Pico reader. Logs:
  `artifacts/logs/rel-01-monitor-D92-20261107.log` and
  `artifacts/logs/rel-01-gppro-physical-D92-20261107.log`. No applet or storage
  erase was performed.

### PROFILE-02

- [ ] **Add extended APDUs only if the selected profile requires them.**
  Status: extended frames are rejected with `6700`; APDU API state checks exist.
  General T=1 WTX and logical channels remain excluded. Native-operation CCID
  time-extension support exists and must not be confused with T=1 WTX.
  Acceptance if selected: explicit protocol/API requirements and focused tests,
  then build/physical validation. Otherwise retain documented exclusions; do not
  broaden scope merely to remove expected unsupported rows. See PROFILE-01.

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

## Completed baseline — preserve evidence, do not reopen as missing features

These are scoped completion records, not a blanket current-source acceptance.
Full original checklists, measurements, image hashes and exceptions remain in
[the archive](.agent/archive/2026-11-01-todo-pre-compaction/TODO.md) and progress.

| Completed area | Established baseline / remaining task |
| --- | --- |
| Initial APDU and VM review fixes | All eight items in the [2026-10-14 fix checklist](artifacts/reviews/fixes-20261014.md) were implemented; the dated [functionality reviews](artifacts/reviews/functionality-review-20261019.md) and [update](artifacts/reviews/functionality-review-20261021.md) remain historical evidence. Current acceptance belongs to the task IDs above. |
| ARCH-01–09 | Shared runtime/filesystem/diagnostic contracts, Pico storage extraction, loader encapsulation, and production progress tests; ARCH-08 physical gate was accepted on 2026-11-02 as recorded above. |
| Storage capacity and error mapping | Arena/snapshot improvements and package-state compaction implemented; live-limit/reclamation gates STORE-01, API-01, PERSIST-01. |
| Native API corrections | Key-interface/marker maps, EC batched writes, NOPAD directions, MessageRecovery reasons, digest continuation; preserve API-03 retest scope. |
| Core defects | OwnerPINx/reset/deselection and deleted-context invalidation fixed; latest full Core has only declared unsupported/unavailable rows. |
| Memory accounting | Physical allocator/report deltas agree; original “too large” concern not reproduced. No speculative accounting fix warranted. |
| CAP lifecycle / metadata | Dynamic packages, distinct instance AIDs, install journal, DELETE and import validation implemented; PERSIST-02/03 own remaining recovery proof. |
| Transactions / GC | Commit accounting and BUFFER_FULL rejection tested; typed roots, aliases, transient lifecycle and host restart stress pass; no general persistent-deletion acceptance. |
| RMI / SensitiveResult | Service catalog and tested primitive/array/exception/remote-return behavior pass; CTX-01/02 own general SIO and broader result coverage. |
| Profiles / generation | Single capability selection, generated support data, minimal/no-AES/current/symmetric/asymmetric/max selections, stable generated timestamps; not equivalent conformance. |
| Symmetric / P71 | 41/41 reference symmetric identities matched; physical KAT 71/71 and support comparison 46/46. AES-CTR is separately API-tested, absent from the supplied legacy P71 matrix. |
| Official reports | Symmetric 4,525 rows: 73 yes / 4,452 no; asymmetric: 196 / 4,329; max_crypto: 198 / 4,327. Complete reports contain zero other statuses. |
| Known report limitation | Two upstream 3072 KeyPair rows request 4096. Keep full-set mismatches explicit (asymmetric 143/145, max_crypto 145/147); never edit CAPs or enable 4096 to hide them. |
| Platform integration | Linux CCID/SCP03/GPPro and explicit unchanged CAP installation demonstrated; cold-power and Windows scopes remain AC-01/02. |
| Diagnostics / performance | Reader LED and measured static workloads recorded; later timed-out performance runs stay partial, not full-suite acceptance. |

Do not apply an old firmware hash or an older physical pass to a newly rebuilt
artifact. Generated capability manifests describe selection; measured test
results establish behavior. Keep validation limitations attached to both.

  Update 2026-11-06: Exception-handler dispatch now validates uploaded handler offsets against the current frame and decoded instruction starts. A generated catch-all handler targeting an INVOKESTATIC operand previously let the process APDU return `9000`; it now fails closed with `6F00`, and manager/app selection recovery passes VPCD 6/6. Out-of-range catch CP regression also passes 6/6. PC/ARM/RISC-V builds and the unchanged JCAlgTest positive VPCD regression pass; no physical hostile-CAP test was run.
  Update 2026-11-06: Active exception protected ranges now validate their one-based CAP `start_pc` conversion, decoded start boundary, Method-component bounds, and current-frame containment. The start-into-operand fixture fails closed with manager recovery (6/6); out-of-range catch CP and handler destination cases also pass 6/6. The unchanged JCAlgTest positive run passes 15/15 after correcting the one-based offset; ARM/RISC-V/PC builds pass. No physical hostile-CAP recovery test was run.
  Update 2026-11-06: A 32,767-byte active protected range extending beyond the Method image now has a VPCD malformed-CAP regression; it fails with `6F00` and confirms ISD/applet recovery (6/6). Source guard was already built on ARM, RISC-V and PC in the preceding exception-range step. Physical malformed-CAP behavior remains open.
  Update 2026-11-06: An active exception-table `start_pc=0xFFFF` now has a malformed-CAP VPCD case; dispatch fails with `6F00` and manager/app selection recovery passes (6/6). Together with the long-range overflow and operand-start cases, this covers both Method component bounds and instruction alignment. Physical hostile-CAP behavior remains open.
  Offline revalidation on 2026-11-07: two consecutive ARM `max_crypto` builds
  produced the same CPLC source ID `FFBB1B86` and unchanged firmware end;
  `gp_get_data_test` passes 25/25. The authorized USB helper granted three
  nodes, but bounded `pcsc_scan -r` still published no reader, so physical
  GPPro confirmation remains pending.
