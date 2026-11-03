# Project TODO

Updated and compacted: 2026-11-02. This is the active backlog for `plan.md`.
Implementation and validation are separate: an implemented task stays open when
its acceptance gates remain unverified. Keep upstream CAPs unchanged and follow
`AGENTS.md` for native API boundaries, build gates, hardware access and commits.

## Current priorities

1. Continue **CTX-01/03** SIO lookup/proxy/firewall work and separate CAP
   firewall contexts from applet-instance ownership; retained per-instance
   contexts and a proxy registry are groundwork, not complete SIO semantics.
2. Finish **STORE-01**, **API-01/03** and **PERSIST-01/02/03** to distinguish
   live storage limits, reclaimable records, API results, and power-loss safety.
3. Close the current-image coverage and integration gates in **API-03**,
   **GP-01**, **AC-01/02**; do not infer them from runtime heartbeat output.

Physical availability is volatile. The latest elevated check on 2026-11-02
found two runtime CCID readers (`8E464F4E2BBC0A19` and `2BED26ED20556535`) and
no BOOTSEL devices, despite the user reporting three boards in BOOTSEL. Both
passed the read-only JCAlgTest registry/version/JCSystem probe at 16:03 UTC.
Their package inventories differ. Refresh identity and access before hardware
work; continue useful offline work otherwise.
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
  creation/read and package cleanup passed. The physical harness now requires
  matching erase telemetry, preventing a non-hook image from yielding a false
  reclaimer PASS. This closes the single-owner live erase check only.
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
  foreign-reference stores. The VM now preserves owner metadata in
  applet-owned transient wildcard `Object[]` arrays. Descriptor-free VPCD
  verifies identity on read, and Pico 2 `8E464F4E2BBC0A19` returned `6B439000`
  after the current ARM image was flash-verified; the full GPPro fixture
  cleanup passed. Persistent foreign references
  still fail closed because static/instance fields and persistent arrays need
  a stable cross-package identity encoding and owner-aware roots/restoration.
  Instance/static stores and typed foreign array stores remain unimplemented.
  Do not relax the `aastore` guard alone: the current typed-array assignability
  path resolves an instance handle through the active package Heap, which can
  alias an equal compact handle from that package. First add owner-aware lookup
  of the referenced package's retained Context/Heap, then test assignability
  across equal-handle collisions. Field and persistent-array stores also need
  to retain/serialize the package owner and add owner-aware GC roots.
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
  A new test-only SIO method passes a caller-owned byte array into native
  `Util.arrayCopyNonAtomic`. Generated JNI dispatch rejects the foreign owner
  before resolving the compact handle in the service heap; the applet catches
  `SecurityException` and returns `6A269000`. VPCD passes with Descriptor
  omitted and included, and physical Pico 2 `8E464F4E2BBC0A19` passes the same
  APDU and full fixture cleanup. This covers one native reference-consumer
  path, not the remaining native APIs or the full lifecycle audit.
  Evidence: current HANDOFF, `tools/test-rmi-registry`, context/proxy progress
  records. Preserve unrelated ongoing source changes while continuing this work.

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
  Remaining: exercise other result producers (PIN, random-data,
  checksum, AEAD and key APIs where enabled), verify GC rooting of object
  results after ordinary locals are dropped through interpreted SIO, and
  repeat selected coverage on physical hardware when available.
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
