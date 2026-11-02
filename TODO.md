# Project TODO

Updated and compacted: 2026-11-02. This is the active backlog for `plan.md`.
Implementation and validation are separate: an implemented task stays open when
its acceptance gates remain unverified. Keep upstream CAPs unchanged and follow
`AGENTS.md` for native API boundaries, build gates, hardware access and commits.

## Current priorities

1. Finish **ARCH-08** physical USB progress stress/timing acceptance when a
   board enumerates. Existing eight-event budgeting is not a measured time bound.
2. Continue **CTX-01/03** SIO lookup/proxy/firewall work and separate CAP
   firewall contexts from applet-instance ownership; retained per-instance
   contexts and a proxy registry are groundwork, not complete SIO semantics.
3. Finish **STORE-01**, **API-01/02** and **PERSIST-01/02/03** to distinguish
   live storage limits, reclaimable records, API results, and power-loss safety.
4. Close the current-image coverage and integration gates in **API-03**,
   **GP-01**, **AC-01/02**; do not infer them from runtime heartbeat output.

Physical availability is volatile. The latest authorized check on 2026-11-02
found two runtime CCID readers (`8E464F4E2BBC0A19` and `2BED26ED20556535`),
not BOOTSEL devices. Both pass the finite runtime monitor, read-only JCAlgTest
probe, and upstream Multi CAP GET STATUS test. Refresh identity and access
before hardware work; continue useful offline work otherwise.
See [.agent/HANDOFF.md](.agent/HANDOFF.md) for active continuation and last observations.

## How to use this backlog

Each ID owns one outcome; related tasks link rather than repeat its history.
Update status, remaining work and acceptance evidence here. Put detailed runs,
failures, measurements and commit records in [progress.md](progress.md), and
reusable diagnoses in [.agent/QKB.md](.agent/QKB.md).

The [original TODO](.agent/archive/2026-11-01-todo-pre-compaction/TODO.md) is
preserved byte-for-byte with a SHA-256 manifest. The [migration map](.agent/archive/2026-11-01-todo-pre-compaction/MIGRATION.md)
accounts for every original unchecked item, including historical entries.
Archive text is historical evidence, not current device state or priorities.

## Open architecture acceptance

### ARCH-08

- [ ] **Measure and accept bounded native USB progress on hardware.**
  Status: implemented, strict physical acceptance pending. The pinned TinyUSB
  patch adds `tud_task_budgeted(max_events)`; all JCVM service points use eight
  FIFO events per invocation and leave remaining events queued. PC/ARM/RISC-V
  builds and physical startup checks passed. ARCH-09 production flow tests pass
  9/9, but do not replace this gate.
  Remaining: sustain benign USB control traffic during a representative long
  RSA-3072 operation; measure per-invocation duration and eventual queue drain.
  Acceptance: no progress invocation monopolizes execution; watchdog, CCID
  time extensions, and final-response ordering remain correct. Record the bound
  and physical long-call result, or the concrete dependency limit if unmet.
  Evidence: [callback contract](docs/NATIVE_PROGRESS_CALLBACKS.md),
  `src/ccid_progress_flow.hpp`, `tools/test-ccid-progress-flow`, original ARCH-08.
  Earlier long-call completion was 49,184 ms; the new budgeted path's required
  sustained-traffic/timing gate is still open. No alternate backend work here.

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
  Remaining: physical multi-owner inventory and actual per-record power-loss /
  interrupted-reclaimer acceptance, including restart and retry. Retain stable
  Java Card handles and extend root audits if contexts/native owners change.
  Acceptance: incomplete reachability never authorizes deletion, all live roots
  and aliases survive, interrupted batches resume safely, and no other owner is
  affected. Do not enable persistent deletion before these gates pass.
  Dependencies: PERSIST-02/03. `JCSystem` object deletion stays unsupported.
  Evidence: original reclamation groundwork and primitive records; 1,000-cut
  fake-flash test covers ten staged erases, not physical VM-reclaimer power loss.

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
  was not identified by the probe.
  Remaining: cold-power restoration of instance identity/fields, nested aliases,
  static roots and multi-instance owner state; verify corrupted/incomplete
  records cannot authorize reclamation after restart.
  Acceptance: record actual disconnect/reconnect and exact image/board identity;
  verify data and isolation after boot, with negative recovery cases as relevant.
  `tools/test-pico-powerup` is read-only and explicitly reports
  `power_cycle=not_verified_by_script`; it cannot itself prove power removal.
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

### API-01

- [ ] **Diagnose cumulative Security/Crypto memory pressure and finish Security.**
  Status: the 2026-11-01 max_crypto Crypto run completed all 466 rows on 2BED:
  292 OK, 161 unsupported, nine NO_MEMORY, three WRONG_RESULT and one
  EXC_CARDRUNTIME; zero EXC_RUNTIME. Earlier 543 Security / 206 Crypto runtime
  outcomes and isolated null-receiver failures are historical baselines, not
  current-image results. Their cause was not established.
  Focused current RSA/RSA-CRT-512 generation passes; filtered KeyAgreement /
  Cipher returned 50 OK and 15 unsupported without NO_MEMORY, then timed out
  before AEAD. Security on 8E stopped at transient RSA-private-2048 allocation
  with SCARD_E_NOT_TRANSACTED; the reader vanished before cleanup.
  Remaining: capture the first failing full-sweep APDU and allocator/persistent
  state; reproduce cumulative pressure versus genuine live-data limits; complete
  Security and check cleanup/registry after interrupted attempts when accessible.
  Acceptance: complete current-image sweeps, account for every unexpected result
  and resource limit, and do not treat filtered timeouts as complete isolation.
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
  Optional Javacardx/Ext/Int/Biometry imports may remain unavailable for the
  declared profile; prior rejection is not evidence of implemented coverage.
  Remaining: maintain an area/profile/result matrix, retest changed native paths,
  and account explicitly for all unavailable scenarios and unsupported setup
  dependencies. Verify current-source Service/JavacardxSec results where affected.
  Acceptance: complete bounded runs and cleanup on identified images; require
  every ARM image to fit the 1 MiB firmware window enforced by `tools/build`;
  separate expected unsupported outcomes from wrong results, runtime errors and
  capacity failures. Do not count serial `choupi_runtime` heartbeat as API
  conformance.
  Dependencies: API-01/02, PROFILE-01, GP-01; EMU-01 handles host throughput.
  Evidence: current progress/QKB; original deploy and focused retest entries
  retain exact CSV/log paths, hashes, selected calls and historical baselines.

### EMU-01

- [ ] **Make full Crypto VPCD validation complete within an explicit time window.**
  Status: full Crypto timed out at 800 s after 190/466 rows (112 OK, 78 expected
  unsupported), before IV rows. This failed full-area validation is not a pass.
  An RSA-SHA-384 filter passed seven matching rows but timed out afterward,
  interrupting cleanup; a narrower PSS filter completed four OK with cleanup
  in 88 s. No RSA-SHA-384 defect was demonstrated.
  Remaining: diagnose setup/client/emulator throughput and cleanup, select a
  justified finite area deadline, and rerun the complete intended selection.
  The host MPU emulator also had a privileged-access failure when a memory
  fault interrupted an MPU region update: permission lookup tried the busy
  region lock and denied the access. Privileged accesses now bypass that lock;
  focused MPU tests and the 15-case SIO VPCD suite pass. Performance impact has
  not been measured.
  Acceptance: normal exit, all requested rows, correct statuses and CAP cleanup;
  partial CSVs retain partial classification even when all emitted rows pass.
  Use fresh VPCD storage for isolated sweeps; restart tests retain matched image
  and registry sidecar. Never overlap runners on one reader. Coordinate API-03.

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
  cross-context arguments are now decoded from the interface CAP Descriptor
  signature and copied by primitive type. Null and JCRE-global array/instance
  arguments may cross because their handles resolve through the shared global
  heap; array-typed parameters require an array handle, while arrays may also
  be passed where the declared type is `Object`. Applet-local references still
  fail closed pending ownership translation.
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
  `set_Pushed_Element`; unknown values and applet-private references fail
  closed. Null and JCRE-global references may cross. Three-bit tag storage
  costs 96 bytes per 256-word Stack. Java Card RE 3.0.5 permits parameters to
  cross an SIO call but still applies caller-context firewall checks to later
  object access (§§6.1.4, 6.2.4.3, 6.2.8). Do not copy private compact
  references between heaps: handles are heap-local and can alias a different
  server object. Applet-local reference arguments currently fail closed before
  entering the server. Keep that as the safe incomplete behavior until
  invocation arguments can carry an owner-tagged opaque reference without
  aliasing a server-local handle. The SIO parameter contract permits receiving
  parameters, but array operations remain subject to §6.2.8.2: only arrays
  owned in the active context or designated global arrays are accessible. Add
  a fixture proving private-array argument transport followed by caught
  `SecurityException` on server access, along with equal-handle collision,
  return, storage, GC and deletion tests. Never copy raw handles between
  independent heaps. See `.agent/QKB.md`.
  A separate firewall gap remains: Java Card RE 3.0.5 §6.2.8.2 permits
  ordinary applet references to be stored in global Object[] arrays, while the
  current implementation rejects all non-null entries (and transaction probe
  case 41 codifies that restriction). Do not relax it until global-array reads
  preserve the referenced object's owning context across independently
  allocated heap-local handles, with GC, deletion, persistence, reset, and
  stale-owner coverage. Update case 41 only with that implementation. Current
  case 41 reproducibly returns caught `SecurityException` (`019000`) in
  `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261102T042104Z.log`;
  the focused run took 85.593 s.
  No types are inferred from raw values. The
  descriptor-free VPCD fixture passes 15/15; the Descriptor-present regression
  also passes 14/14 through the existing signature path. Physical typed
  cross-AID execution remains unverified. Typed local reads now check their
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
  returns `4567`. It also verifies denied lookup returns null and applet-local
  array arguments fail closed (`6A03`), and a cross-context int argument and
  return round-trip produces `76543210`. A server-thrown SecurityException is
  caught by the client, and a subsequent SIO call succeeds. A proxy stored in
  an applet instance field survives the end-of-APDU GC and is reusable on the
  next APDU. Calling `Object.equals` directly on a proxy fails closed with
  `SecurityException`. A fresh lookup works after emulator process restart;
  an old persisted proxy loses its native routing metadata. The instance header
  now stores an internal Shareable-proxy marker; restoration recreates that
  marker, and the VM rejects marked placeholders absent from the runtime
  registry across interface/virtual/special calls, casts and instance-field
  operations instead of invoking them in the caller context. VPCD confirms the
  stale placeholder is fail-closed after process restart and card reset.
  Persistent object aliases now also preserve the internal proxy marker: an
  alias restored before its canonical target can no longer become an unmarked
  object and dispatch in the client context. The three-package VPCD fixture
  stores the same proxy in two fields and verifies that reading the alias first
  after restart fails closed. This establishes the documented fail-closed
  persistence boundary; routing metadata is not rehydrated. On
  physical 2BED `2BED26ED20556535`, full-index compaction now succeeds and
  GPPro INSTALL for the already-loaded client and service applets returns
  `9000`. The cross-AID probe selects the client, but its SIO APDU returns
  `6F00` with `SecurityException` at `invokeinterface`. The pre-existing
  package CAP identities are unknown, so clean physical cross-AID acceptance
  remains open; preserve these packages and test with distinct package AIDs.
  A fresh external-interface VPCD fixture passes a valid exported interface
  install and rejects a class/missing export (3/3).
  The unchanged same-applet
  call still passes on physical Pico 2 `8E464F4E2BBC0A19`. Never return raw
  server heap references to clients.
  Acceptance: physical positive cross-AID fixture; denied-call fixtures; correct
  context/AID restoration; stable roots across GC and reset/delete; no cross-heap
  reference leaks; retain the tested fail-closed persistence boundary or add
  standards-compatible proxy rehydration; then unchanged upstream Multi CAP
  integration and physical checks.
  General cross-context calls and logical-channel policy remain unimplemented;
  basic channel 0 is the current assignment. No blanket firewall acceptance yet.
  Evidence: current HANDOFF, `tools/test-rmi-registry`, context/proxy progress
  records. Preserve unrelated ongoing source changes while continuing this work.

### CTX-02

- [ ] **Complete Java-level SensitiveResult coverage across SIO calls.**
  Status: same-context assertions and a native two-Context regression pass for
  boolean/short/object results, GC rooting, and reset rejection of stale state.
  Remaining: Java-level SIO-sensitive results after CTX-01 plus producers beyond
  the tested JavacardxSec subset.
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
  (47/47) pass; PC, ARM and RISC-V max_crypto builds pass. Latest ARM image ends
  at `0x100e75f4`, using 947,700 of the 1 MiB firmware window and leaving
  100,876 bytes. Physical flash/runtime validation remains open: the latest
  approved host inventory exposed two runtime CCID readers and no BOOTSEL
  device. Further acceptance must cover same-CAP selection, instance/package
  deletion and reinstall, reset/transient behavior, and persistence on hardware.
  Two repeated instance DELETE/reinstall cycles keep array-owning owner 1
  reserved while reclaiming object-free owner 3; full package DELETE/reinstall
  releases and reuses owner 1. Same-CAP transient tests show sibling selection
  clears `CLEAR_ON_DESELECT` while preserving `CLEAR_ON_RESET`, and card reset
  clears both. The owner lifecycle suite passes 9/9 and transient suite 5/5.
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

### AC-01

- [ ] **Verify Windows PCSC/GPPro and both packaged profile workflows.**
  Status: Linux enumeration, GPPro install and official JCAlgTest reports have
  evidence; Windows remains unverified.
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
  Status: framed/count/type/Class/Directory CAP validation and APDU fuzzing exist.
  Latest CAP sanitizer run passes 4,208 checks / 48 supplied CAPs / 100,000 random
  inputs; short APDU fuzzing covered 250,000 inputs. Directory validation respects
  omitted GP LOAD components. Physical final-image parser validation is pending.
  Remaining: broader nested-table/linkage validation and fuzzing, interrupted
  persistent-write/reclaimer recovery, and private RSA input blinding / full CRT
  fault verification. Complete verification needs a matching public exponent;
  residue checks alone are insufficient. Preserve existing scratch zeroization.
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
| ARCH-01–07, ARCH-09 | Shared runtime/filesystem/diagnostic contracts, Pico storage extraction, loader encapsulation, production progress tests; ARCH-08 physical gate stays open. |
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
