# Project TODO

This tracker follows the acceptance criteria in `plan.md`. “Done” means the
behavior has evidence recorded in `progress.md`; partial results are called
out explicitly.

## Current prioritized backlog — 2026-10-20

The checklist and status notes below contain historical snapshots and can be
stale. This section is the current source of truth for open work. Missing
algorithms and JCAlgTest `NO_SUCH_ALGORITHM` results are expected unless CHOUPI
claims to support them; implementing every algorithm is not a goal.

### Priority 1 — close acceptance criteria AC1–AC5

- [x] **AC4: refresh the physical support-results comparison after OwnerPINX.**
  On 2026-10-20 the official physical `ALG_SUPPORT_EXTENDED` run completed
  4,525 rows: 15 `yes`, 4,510 `no`, and no other CHOUPI statuses. The
  comparator matched all row identities and the declared support list
  (15/15; no unexpected or missing entries). jCardSim's additional 1,454
  supported results are capability differences, not CHOUPI failures. Logs and
  CSV: `artifacts/logs/jcalgtest-alg-support-ownerpinx-20261020.log`,
  `artifacts/logs/monitor-alg-support-ownerpinx-20261020.log`,
  `artifacts/logs/jcalgtest-support-compare-ownerpinx-20261020.log`, and
  `artifacts/results/physical-alg-support-ownerpinx-20261020/`.
- [x] **AC5: prove the installed applet survives a real power cycle.**
  After a reported five-second disconnect/reconnect on 2026-10-20, the same
  physical Pico reader returned. `tools/test-pico-powerup` confirmed the
  JCAlgTest package and applet remained installed, SELECT and GETVERSION
  (`1.8.2_jc305`) returned `9000`, and the JCSystem info APDU returned `9000`.
  A fresh official `ALG_SUPPORT_EXTENDED` run completed all 4,525 rows and
  passed comparison against the jCardSim reference: 15 supported, 4,510
  unsupported, no other CHOUPI statuses, and all 15 declared algorithms
  matched. Evidence: `artifacts/logs/pico-powerup-persistence-20261020T132635Z.log`,
  `artifacts/logs/jcalgtest-alg-support-after-powercycle-20261020.log`,
  `artifacts/logs/monitor-alg-support-after-powercycle-20261020.log`,
  `artifacts/logs/jcalgtest-support-compare-after-powercycle-20261020.log`,
  and `artifacts/results/physical-alg-support-after-powercycle-20261020/`.
- [ ] **AC1: verify GPPro on Windows.** Linux PC/SC and GPPro were verified on
  2026-10-20: `gp --list` showed the Pico ISD, and the clean CAP install worked
  without a custom reader driver. Windows reader and GlobalPlatformPro
  compatibility remain unverified.
- [x] **AC2: cleanly install and select the supplied JCAlgTest 1.8.2 JC305
  CAP.** Removed the test applet/package, confirmed the registry was empty,
  installed `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` with GPPro 26.06.04,
  then confirmed package `LOADED`, applet `SELECTABLE`, SELECT `9000`,
  GETVERSION `1.8.2_jc305`, and JCSystem info `9000`. CAP SHA-256:
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  Transcripts: `artifacts/logs/gppro-clean-install-list-before-20261020.log`,
  `artifacts/logs/gppro-clean-install-delete-applet-20261020.log`,
  `artifacts/logs/gppro-clean-install-delete-package-20261020.log`,
  `artifacts/logs/gppro-clean-install-list-empty-20261020.log`,
  `artifacts/logs/gppro-clean-install-load-install-20261020.log`, and
  `artifacts/logs/pico-powerup-persistence-20261020T135329Z.log`. Package
  deletion requires deleting its registered applet instance first; see QKB.
- [ ] **AC1–AC5: run the plan's clean-flash integration sequence three times.**
  For each run, install the CAP, complete supported-algorithms mode, compare
  its CSV, power-cycle, and repeat. Record each run separately; `plan.md`
  defines three consecutive passes as the exit criterion. Run one is in
  progress: both architecture builds, the physical ARM CHOUPI cycle, clean
  reinstall, registry/SELECT/GETVERSION/JCSystem probe, support sweep, and
  jCardSim comparison passed. The required physical power removal and repeated
  post-cycle sweep are still pending; no complete three-pass run is counted
  yet. Evidence is recorded in `.agent/HANDOFF.md` and `progress.md`.

### Priority 2 — close correctness and reliability gaps

- [ ] **Make CAP installation and filesystem updates recover atomically.** The
  CAP slot header can be marked installed before the filesystem transaction's
  final `fs_sync()`. A sync failure can roll back filesystem snapshots without
  rolling back that header. Add failure-injection coverage and coordinate
  durable recovery. See `.agent/QKB.md`.
- [ ] **Complete GlobalPlatform lifecycle verification.** Exercise authenticated
  `DELETE` over APDU, then verify GET STATUS and reinstall behavior after
  restart. Parser, tombstone, cleanup, and host tests pass, but the complete
  DELETE APDU plus restart lifecycle is not covered end to end. Extend GET DATA
  / GET STATUS continuation cases required by the selected profile. CPLC
  (`9F7F`) returns `6A88` because no manufacturing-data source is available.
- [ ] **Broaden garbage-collector root and persistence-alias coverage.** The
  reachability collector passed the physical static workload (2,262 transfers;
  heap stabilized at 83 array slots / 28 instance slots; a pass reclaimed 50
  arrays). Add regressions for static references, native-held handles,
  aliasing, and lazy restoration in applet shapes beyond JCAlgTest. Preserve
  flash records when their RAM objects are reclaimed.
- [ ] **Characterize PC/SC discovery and reinsertion.** BOOTSEL insertion was
  recognized as `2e8a:000f`, firmware as `2e8a:0009`. Repeated remove/add
  timing and reader rediscovery after firmware reboot remain unmeasured.
  Capture bounded udev/PCSC logs and verify the same board serial reappears.

### Priority 3 — define and extend the VM profile

- [ ] **Complete deploy-folder API coverage on the Pico 2.** Runtime loading
  accepts arbitrary package AIDs and assigns IDs 56–63. Physical tests now
  install the Core, Security, Crypto, Service, and two Multi applets; Core,
  Security, Crypto, JavacardxExt, JavacardxSec, and Service clients ran. Core
  and Crypto expose API/runtime failures, Javacardx and JavacardxInt fail at
  INSTALL, Biometry fails at LOAD, and cleanup of populated applets returns
  `6581`. Next fix Core correctness, cleanup, then rerun stages with the fixed
  deploy runner. See `.agent/QKB.md` and the 2026-10-21 deploy result CSVs.
- [ ] **Fix Core API failures found by physical deploy coverage.** `Util.arrayCopy`
  and `arrayCopyNonAtomic` fail overlapping-range cases; `OwnerPIN` methods
  produce runtime exceptions/SW `6F00`; `Shareable` lookup with the test
  sub-interface returns the wrong result. Preserve the 3.0.5 API test artifacts
  under `artifacts/results/deploy/` and rerun Core after each fix.
- [ ] **Fix package deletion after applet tests.** GPPro DELETE returns `6581`
  for Core and Service packages after persistent objects have been created,
  while deleting packages with no installed instance succeeds. Determine which
  persistent-data cleanup or transaction step fails before relying on runner
  cleanup for repeated API stages.
- [ ] **Declare the supported Java Card profile and audit its API contract.**
  State the supported Java Card Classic version and APIs, then add applet-level
  behavior tests for each implemented API. Existing tests cover JCSystem,
  APDU, lifecycle, exceptions, and persistence but do not establish broad
  profile compatibility.
- [ ] **Implement multi-applet contexts and firewall semantics if the declared
  profile requires them.** The runtime has one applet context. General context
  switching, custom shareable-interface callbacks and ownership checks,
  cross-applet AID lookup, and logical channels are not implemented. The null
  shareable lookup only covers the no-server/default case; assigned channel is
  basic channel 0.
- [ ] **Make commit-capacity reporting consistent with transactions.**
  `beginTransaction` / commit / abort and rollback behavior have coverage, but
  `getMaxCommitCapacity()` and `getUnusedCommitCapacity()` return zero. Record
  zero as a deliberate capability or implement capacity accounting and tests.
- [ ] **Add extended APDUs only if required by the selected profile.** The
  transport rejects extended APDUs. `APDU.waitExtension()` enforces API state,
  but synchronous CCID processing cannot send actual T=1 WTX while a command
  is executing.

### Verified; do not reopen as current failures

- ARM and RISC-V repository builds pass; the RP2350 ARM image passes bounded
  runtime checks on hardware.
- JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` completed on Pico: 2,262 transfers,
  no panic or GC traversal error; expected totals were 1,772
  `NO_SUCH_ALGORITHM` and 95 `CANT_BE_MEASURED`. SHA-256 256-byte timings were
  3.10 ms/op (`doFinal`), 3.34 ms/op (`update`), and 6.11 ms/op (`reset`).
- SCP03, basic GlobalPlatform listing/loading/selection, SHA-256 execution,
  short-response chaining, applet registration/selection/reselection, tested
  JCSystem methods, transaction rollback, and VPCD process-restart persistence
  have working implementations and recorded tests.
- `JCSystem.requestObjectDeletion()` reports `SystemException.ILLEGAL_USE`
  because `isObjectDeletionSupported()` is false. This is an explicit
  unsupported capability, not an unfinished no-op.

## Historical checklist and evidence

The material below is retained as chronological context. Its open checkboxes
and hardware reports are snapshots from earlier sessions; use the current
prioritized backlog above for present status.

## Completed

- [x] Build and flash the CHOUPI image for RP2350 ARM through repository wrappers.
- [x] Provide USB CDC + CCID transport and bring up the Pico as a PC/SC reader.
- [x] Implement SCP03 authentication and enough GlobalPlatform card-manager
  behavior for GPPro listing, CAP LOAD, and registry queries.
- [x] Persist the supplied JCAlgTest v1.8.2 CAP blob across firmware reboot.
- [x] Resolve the persisted CAP through CHOUPI package lookup and return the
  applet AID/FCI on SELECT. This confirms registry/transport behavior only; it
  does not confirm that the applet constructor or `process()` has run.
- [x] Build the RP2350 ARM M33 target through the repository wrapper. ARM is
  the hardware target; the current root development instructions also require
  a wrapper RISC-V build for shared-source changes, but RISC-V hardware/runtime
  testing remains out of scope.
- [x] Correct `Util.arrayCompare()` so the first greater source byte returns
  `1` instead of incorrectly returning `-1`; nested API commit `6dceb7e`.
- [x] Limit the default target build to two parallel jobs to avoid saturating
  all VM cores; override with `PICO_BUILD_JOBS` when desired.
- [x] Implement `JCSystem.lookupAID()` null/range validation and matching for
  the active applet AID; VPCD cases 65–66 pass. Lookup across additional applet
  contexts remains open with the single-context runtime limitation.
- [x] Implement `Applet.register()` and `Applet.register(byte[],short,byte)`:
  registration is limited to the active install entry, only one successful
  registration is accepted, explicit AID lengths and bounds are checked, and
  the supplied AID must match the install instance AID. VPCD cases 67–70 pass
  for a matching AID; a mismatched AID reports
  `SystemException.ILLEGAL_AID` and the applet can complete fallback install.
- [x] Restore the registered applet instance AID independently of the CAP
  Applet component AID. GlobalPlatform INSTALL can assign distinct AIDs; reboot
  restore no longer rejects this valid metadata. A temporary VPCD CLI probe
  installed and selected an applet using a distinct instance AID and restored
  persistent objects successfully; the retained standard persistence test also
  passes. Physical Pico reboot validation remains open.
- [x] Match `JCSystem.isAppletActive(AID)` to the JC 3.0.5 registration and
  callback contract. It is false before registration; true during install after
  `Applet.register()`, during `select()`, and through `deselect()`/`process()`;
  false after deselection/reset. VPCD cases 76–77 and rejected-selection case 56
  pass.
- [x] Match `JCSystem.getAID()` registration semantics: return null before
  successful `Applet.register()`, then return the instance AID during install,
  selection, and after applet restore. Focused install and fresh-process restore
  VPCD checks pass; physical APDU validation remains open.
- [x] Keep an applet selected when its SELECT APDU returns `61xx`; dispatch the
  following GET RESPONSE to that applet. CCID and VPCD transport paths now
  recognize `9000` and `61xx` as successful selection outcomes. VPCD session
  regression verifies the selection state and GET RESPONSE dispatch.

- [x] Release temporary buffers after persistent byte/short/int field reads.
  getDataFromTag now returns owned storage, so successful reads and read or
  type-validation failures release the temporary buffer. The persistent-store
  regression counts new[]/delete[] on both success and injected fs_read()
  failure; all cases pass.

## Open persistence risks

- [ ] Diagnose intermittent Pico PC/SC/USB transport loss. The package and
  applet registry were confirmed and one attempt returned SELECT `9000` before
  GETVERSION failed with `SCARD_E_NOT_TRANSACTED`. After a fresh firmware cycle,
  the reader appeared in PC/SC but the bounded probe timed out before SELECT;
  serial reported `xfr=0`, so no command reached firmware. Physical APDUs and a
  true power-cycle persistence check remain unverified. See `.agent/QKB.md` and
  `artifacts/logs/physical-jcsystem-active-retry-20261019.log`.

- [ ] Make CAP installation metadata and VM filesystem state recover together.
  The CAP slot is marked installed before the filesystem transaction's final
  `fs_sync()`; a sync failure rolls back filesystem snapshots but not the CAP
  slot header. This is currently source-inspection evidence only. See
  `.agent/QKB.md`; add failure injection before choosing the recovery protocol.
- [x] Implement `Applet.selectingApplet()` and deliver a successful applet
  SELECT APDU to `Applet.process()`. Lifecycle probes verify the flag during
  `select()` and SELECT processing, its absence during `deselect()`, and reset,
  rejected-selection, and re-selection behavior. Unknown AID SELECT returns
  `6A82`, deselects the applet, and is not dispatched to `process()`; focused
  VPCD coverage passes. The earlier full lifecycle suite passed 75 checks.
  Physical validation remains open while the Pico is absent from PC/SC.

## Active: validate the applet process APDUs and client results

Current verified status (2026-10-17):

- `JC_Instance::getFields()` now follows the OS FFI contract: `fs_exists()` is
  nonzero when the record exists. A fresh JCAlgTest applet SELECT now succeeds;
  the earlier `== 0` interpretation was incorrect and caused a missing-field
  read.
- `./tools/build arm` and `./tools/build-pc` pass. The focused JCSystem/SHA
  VPCD probe passed 12/12 checks. Two full-VM JCAlgTest processes using one
  shared flash image passed 22/22 checks, including applet selection and SHA
  operations after the second process restored the saved applet.
- The static workload uses the JCAlgTest 256-byte, 50-call settings and returns
  `AA9000` in 36.939 s and 37.244 s on the PC emulator. These are not Pico
  performance measurements. Unsupported RSA-SHA1 reports reason `03`, as
  expected.
- The ARM-independent regression wrappers passed APDU, VPCD, GP registry /
  DELETE, install-status, transaction, RP2350 filesystem, array, and ROM-mask
  checks. `tools/test-jcarray` now uses `tools/env cmake` rather than invoking
  the compiler directly; its PC mbedTLS include setup and filesystem test stub
  are maintained under `tools/tests/jcarray/`.
- The replugged Pico enumerated as `2e8a:0009`. `./tools/build arm` passed,
  and the ARM image was flashed through `./tools/flash` while investigating
  this ARM hardware failure. Direct CCID APDUs then passed SELECT, GETVERSION
  (`1.8.2_jc305`), JCSystem info, SHA-256 support, SHA-256 `doFinal`, and the
  JCAlgTest 256-byte / 50-call static SHA workload (`AA9000`, 0.718038 s).
  The exact APDU transcript is saved in
  `artifacts/logs/jcalgtest-physical-resume-20261016.log`.
- The post-test serial capture reports `TEST:PASS choupi_runtime` and no
  `FAULT`. A follow-up change consumes the RP2350 scratch marker after copying
  it and prints `WATCHDOG_RECOVERY cause=other` once. An eight-second capture
  confirmed one marker, eight runtime passes, and no failure. Physical SELECT,
  GETVERSION, JCSystem, SHA support, and static SHA still pass; the post-fix
  static loop took 0.725005 s.
- An earlier session could not use the official Java client while PC/SC listed
  only Virtual PCD readers; the static APDU then reproduced the workload over
  raw CCID. Host-access PC/SC is now available, the physical probe passes, and
  the official static-suite result is recorded below.
- Pico `JCSystem.getAvailableMemory()` short and jc304 array forms now report
  linked Newlib and writable-overlay capacity. ARM and RISC-V builds passed;
  the ARM firmware flashed and the live physical JCSystem probe passed. It
  reported 47,777 persistent bytes and 325,352 transient bytes. Commit capacity
  remains zero while transactions are unsupported. The earlier Rust allocator
  link experiment was reverted; do not treat the unlinked Rust heap as the Pico
  VM heap.
- Physical USB tools now fail immediately in the restricted `NoNewPrivs: 1`
  context and print the required host-access instruction. Invoke flashing,
  serial, and PC/SC tools through host access from the outset.

Latest physical static-suite update (2026-10-17): the official v1.8.2 client
was run through the named Pico PC/SC reader. Five AES-128 CBC no-padding
`Cipher_doFinal` baselines passed (average 566.4 ms); the first 50-call measured
APDU caused reader loss and a board recovery. Temporary counters showed
`arrays=594`, `instances=192`, and no remaining interpreter frames before that
APDU. Cipher measurement APDUs accumulated about two array objects per repeat.
`Heap` has no reclamation path; `requestObjectDeletion()` is intentionally a
no-op while `isObjectDeletionSupported()` returns false, consistent with the
plan's unsupported capability. The failure mechanism is documented in
`.agent/QKB.md`; do not implement unsafe offset-list clearing. This run does
not prove AC3's supported-algorithms mode, which remains outstanding.

Latest supported-algorithm update (2026-10-17):
`InitializedMessageDigest.OneShot.open()` throws
`CryptoException.NO_SUCH_ALGORITHM` for unsupported factories. Retained
exception exhaustion is now fixed for both ROM and imported
`javacard.security` class views with separate VM-owned per-heap roots. The
focused VPCD 50-throw regression passes. `RandomData.OneShot.open()` now also
throws `CryptoException.NO_SUCH_ALGORITHM` instead of `ILLEGAL_USE` for this
unsupported factory; its six JCAlgTest rows changed from
`error(ILLEGAL_USE)` to `no`. The corrected physical official
`ALG_SUPPORT_EXTENDED` run completed all 4,525 result rows: 15 `yes`, 4,510
`no`, and no `error(...)` rows. The previous Java-static exception cache was
invalid because statics serialize through flash. Unsupported algorithms remain
expected; do not implement them merely to avoid `NO_SUCH_ALGORITHM`.

The initial cross-check against jCardSim and API factories is recorded in
`artifacts/results/jcalgtest-support-comparison-20261017.md`. It found two
false-positive `yes` rows because `OwnerPINBuilder.OWNER_PIN_X` and
`OWNER_PIN_X_WITH_PREDECREMENT` returned `null` and the JCAlgTest factory probe
did not check the returned reference. Both variants are now implemented and
the VPCD OwnerPINx behavior regression passes. The official physical support
sweep has not been rerun after this implementation; keep that comparison open
until a later hardware validation.

The reusable AC4 comparator now matches the archived Pico and jCardSim CSVs on
all 4,525 `(section, algorithm_name)` identities, checks the CHOUPI status
values against the reviewed 15-entry declaration in
`tools/jcalgtest-supported-v1.8.2.csv`, and reports simulator capability
differences separately. It passed with 15 declared/observed `yes` rows, 4,510
`no` rows, and no CHOUPI error statuses. The archived physical CSV predates the
OwnerPINX fix, so AC4 still needs a fresh physical run.

The separate physical `ALG_PERFORMANCE_STATIC` run remains unresolved. A
previous run directly observed cumulative array retention and allocation
failure. This rerun completed a 250-call DES CBC `Cipher_doFinal` measurement
(28.64 ms/op average), then the next DES `Cipher_init` request failed with
`SCARD_E_READER_UNAVAILABLE`; its bounded serial capture did not record an
allocation failure, and the physical reader was visible again after ACL
refresh. Keep the known array-retention root cause and this distinct reader
failure separate; do not report the static suite as passed. See `.agent/QKB.md`.

Next actions:

1. [x] Add golden applet assertions for both `getAvailableMemory` forms and
   commit-capacity queries. The JCSystem transaction VPCD probe now exercises
   both overloads for persistent, reset, and deselect memory and checks that
   commit capacities remain zero; it passed 13/13 on the desktop backend. The
   separate physical live-memory probe confirms Pico scalar/extended
   consistency and nonzero persistent/transient values.
2. [partial] Golden applet and applet callback checks now cover same-AID
   reselect, rejected SELECT cleanup, and reset callbacks. `Applet.reSelectingApplet()`
   reports true only during same-AID reselection; the full 69-check VPCD
   JCSystem/APDU suite passes. A fresh VPCD process restored committed instance
   state and persistent reference identity.
   Authenticated GP DELETE was exercised on the Pico: applet deletion left the
   package loaded, related-object package deletion removed both, and the exact
   JCAlgTest v1.8.2 CAP was reinstalled. After a firmware restart, GPPro still
   listed the applet as selectable and package as loaded; the bounded
   `tools/test-pico-powerup` check passed SELECT, GETVERSION, and JCSystem.
   The check now retries registry discovery and applet APDUs up to three times
   while refreshing USB ACLs. On 2026-10-19, USB remained enumerated but all
   three GPPro attempts saw only the two Virtual PCD readers, so no applet APDU
   was sent. Remaining: restore physical PC/SC enumeration, then repeat the
   check after true power-off/on and broader CAP restore validation.
   The explicit reinsertion retry at 2026-10-19 09:54 UTC again received USB
   ACL access but GPPro listed only Virtual PCD readers in all three attempts;
   `artifacts/logs/pico-powerup-persistence-20261019T095458Z.log` records it.
   A second explicit retry at 10:24 UTC also saw only the two Virtual PCD
   readers; `artifacts/logs/pico-powerup-persistence-20261019T102428Z.log`
   records that failure. True power-off/on persistence remains open.
   The GET STATUS builder also has host checks for the expected deleted views,
   while the shared DELETE target/transaction code has fake-backend success
   and rollback coverage. Unselected and SCP03-authenticated `0066` Card
   Recognition Data plus unavailable `9F7F` CPLC passed physical APDU probes.
   Persistent
   transaction tests cover injected write/commit rollback failures; filesystem
   overlay tests cover 1,000 randomized power-cut atomicity cases and restart
   recovery. The official physical static suite has not passed; PC/SC still
   stalls after SELECT.
3. [x] Finish the matching JCAlgTest v1.8.2 `ALG_SUPPORT_EXTENDED` mode on the
   named physical reader. The corrected run completed 4,525 rows with no
   transport or row errors. [x] Implement the two OwnerPINX builder variants
   and validate their behavior through VPCD. [x] Compare the archived result
   with the jCardSim CSV and declared implementation list; row identities and
   all CHOUPI statuses match. [ ] Rerun the physical support checks after the
   OwnerPINX fix. Treat absent algorithms and `NO_SUCH_ALGORITHM` as expected
   unless the implementation claims support.
4. Implement and verify the remaining applet lifecycle/API behavior, then
   complete AC1–AC5 and the clean-flash integration sequence three times.

Earlier startup-timeout and physical `F503` observations below are historical
checkpoints. The current physical ARM image passes runtime checks and the
official PC/SC Java client completes the supported-algorithm mode. RISC-V
remains out of scope for the current development objective.

The two-process VPCD transcript is
`artifacts/logs/vpcd-persistence-corrected-transcript-20261016.log`, with
emulator output in `artifacts/logs/vpcd-persistence-corrected-20261016.log`.

Prior ARM runtime result: `JC_Array::size()` was fixed to return logical
element counts by dividing transient/nonpersistent backing bytes by element
width. It stopped applet startup from reading past a persistent `Object[]`.
The earlier host JCAlgTest VPCD regression passed 11/11 and the ARM runtime
smoke reported `TEST:PASS choupi_runtime`; the static SHA APDU's 17.667 s was
measured on the PC interpreter, not the Pico. Those results do not verify the
current physical applet process path.

Host regression status update (2026-10-16): the archived 11/11 VPCD pass was
made by a PC emulator binary older than the latest source edits. After a fresh
`./tools/build-pc` pass, the bounded JCSystem applet runner timed out after 120
seconds before its VPCD socket connected. Stage markers locate the stall in
the initial host JCRE runtime, before CAP installation; no APDU was run. Treat
the old VPCD pass as historical until the freshly built host path is repeatable.
Investigate the startup path, especially `Starter.run()` and
`PlateformeGlobale.getInstance()`, then rerun the JCSystem and JCAlgTest host
regressions.

Host emulator guard (2026-10-16): `choupi/os` previously ignored a failed
`PTRACE_ATTACH` and blocked forever in `waitpid`. This can occur when the host
emulator is itself run under a tracer. It now kills/reaps the child and exits
with an explicit ptrace error. `./tools/build-pc` passed, and a bounded nested
`strace` check verified `EPERM` is reported and the child reaped. This does not
resolve the separate direct host VPCD startup timeout; that remains open.

JCSystem transaction calls now use the existing persistent-write snapshot
backend: begin/commit/abort update transaction depth, and the Java wrappers
throw `TransactionException` for nested begin, commit/abort outside a
transaction, or rollback failure. `Util.arrayCopy`, `arrayFill`, and `setShort`
now start and commit only when needed, and validate inputs before opening a
transaction. The snapshot rollback test passes 30 checks; ARM and RISC-V builds
pass. A Java Card transaction probe applet now verifies nested begin, commit
and abort reason codes, persistent rollback, and post-commit state over host
VPCD (10/10 assertions). Remaining transaction gaps: commit-capacity queries
still report zero, and physical transaction behavior remains unverified. On RP2350, `fs_sync` now defers persistence while
a JCSystem write transaction is active; commit flushes the two-slot,
CRC-protected snapshot, and abort flushes the restored state. The host overlay
test passes 1,000 randomized mutation cut-points across a two-record snapshot
and recovers the complete old state. The Rust OS filesystem's separate
single-record path has its own interrupted-write test; its multi-record
meta-block is not implemented.

SHA-256 `MessageDigest.doFinal("abc")` now returns the known-answer digest
`BA7816BF...15AD` in the host VPCD probe. The matching 256-byte/50-call static
doFinal APDU returns `AA9000` and takes 16.853 s on the PC interpreter. This
does not measure Pico throughput. A method-local `MessageDigest` streaming
probe (`update("abc")`, then empty `doFinal()`) returns the expected digest.
An earlier probe stored the implementation's `OneShot` object in an applet
field and returned the empty digest; that violates the `OneShot` class's
documented temporary-object lifetime. The factory currently returns that
implementation, so support for a retainable digest object remains unresolved;
do not treat the invalid field-retention probe as a general streaming failure.

Latest repeatable host validation (2026-10-16): `./tools/build arm` and
`./tools/build riscv` passed. The focused Java transaction probe passed 10/10
assertions. The two-pass PC VPCD applet test passed 22/22 checks across
two fresh emulator processes, including the JCSystem info APDU and SHA-256
prepare, `doFinal`, and 50-call static operation. Static operation time was
13.985 s and 13.487 s per APDU on the PC emulator; these values do not measure
Pico throughput. The physical reader selector still cannot run because
Java SmartcardIO sees no PC/SC readers in this environment. See the matching
2026-10-16 validation records in `progress.md`.

Current environment note (2026-10-15): the latest `./tools/cycle choupi`
rebuilt ARM, RISC-V, and CHOUPI successfully, flashed the ARM firmware, and
passed the finite 18-second runtime capture with 18 repeated
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` pairs. The detailed
record is `artifacts/logs/cycle-choupi-20261014T135618Z.log`. The corrected ARM firmware was flashed;
GlobalPlatformPro CAP LOAD and INSTALL returned `9000`, the registry lists
`JCAlgTest1` as `INITIALIZED`, and SELECT by AID returned `9000` with FCI. These
steps validate install and selection, but `Applet.process()` has not yet been
validated on the physical card. The PC VPCD integration now restores the
persisted applet and passes ATR, SELECT, JCSystem info, and the
supported-algorithm APDU in one bounded pass. The prior
`INVOKEVIRTUAL`/`StackUnderflowException` was caused by ROM-mask generation
encoding internal virtual method references with the declaration index rather
than the class virtual-table token. The updated generator resolves the
signature against the public/package method table; its emitted APDU tokens are
`1`, `3`, `5`, and `8` for `getBuffer`, `setOutgoing`, `setOutgoingLength`, and
`sendBytes`. The pass log is
`artifacts/logs/jcalgtest-internal-virtual-table-token-fix-20261015.log`.
This is host-emulator evidence only, not a board result. The ARM wrapper build
passes with the regenerated ROM mask. No RISC-V build was run, per the current
ARM-only scope, and no flash was attempted.
This is host-emulator evidence only, not a board result. The official JCAlgTest
v1.8.2 host JAR has
been downloaded to `build/algtest-client-1.8.2/`; its bundled JC305 CAP has the
same SHA-256 as the supplied CAP. `ALG_SUPPORT_EXTENDED` completed against the
bundled jCardSim and produced
`artifacts/results/jcardsim-jcalgtest-v1.8.2-alg-support-extended.csv` (4,595
lines). Treat this only as a reference: jCardSim itself raises
`NotImplementedException` on some algorithms and the client records those
errors. A Java `javax.smartcardio` probe compiles without extra modules, but
`pcsc_scan -r` still reports no reader after the latest flash; the user reports
that the authorized VM helper ran successfully. No APDU reached the board.
Always cap any PC/SC scan.
`tools/probe-jc-algtest` provides a finite-timeout SELECT/GETVERSION/JCSystem
APDU probe, but no APDU reached the board in this run. The ARM and RISC-V build
wrappers pass; only ARM firmware was flashed and tested. Windows reader
enumeration and applet process/power-cycle persistence remain unverified.

Current host update (2026-10-15): the release PC VPCD path now passes two fresh
JCAlgTest applet runs (`TEST:SUMMARY pass=10 fail=0`). The phase-instrumented
run measured 2.554 s connect, 0.001 s ATR, 0.231 s SELECT, 16.437 s for JCSystem
info, and 4.561 s for the supported-algorithm APDU. No physical token was used.
The official host client remains blocked here because the PC/SC service is
unavailable and the `vsmartcard-vpcd` driver is not installed. The next test
requires those host prerequisites; do not substitute the physical token.

Registration implementation status: the VM captures the receiver passed to
either imported JC305 `Applet.register` method and serializes that instance's
fields under a reserved persistent applet root after a successful install
method return. CAP metadata now also records the registered class index in a
version-2 header while continuing to read version-1 headers. The PC VPCD SELECT
path reconstructs the stored applet and runs its `select()` method; this
returned success in the latest host integration. Its APDU `process()` path is
now entered but stops on an applet field read. Installation
rollback and duplicate/invalid registration handling also remain unverified.

- [x] Parse the INSTALL [for install and make selectable] parameter field and
  pass its exact bytes in a Java byte array with offset zero and the matching
  byte length to `Applet.install(byte[], short, byte)`.
- [x] Verify the installation-parameter byte array and length in a valid VM
  execution, including the supplied two-byte `C9 00` field. The VPCD runner
  accepts `--install-params`; a PC-only VM-entry assertion verifies the Java
  local array reference, offset 0, signed `bLength` 2, array size, and both
  bytes before executing the method. A fresh JCAlgTest run then passed SELECT,
  GETVERSION, JCSystem, support, SHA-256 `doFinal`, and static SHA-256
  performance APDUs (`TEST:SUMMARY pass=11 fail=0`).
- [x] Resolve JC 3.0.5 imported class and method tokens against the embedded
  CHOUPI exports. Validate each mapping against CAP/export metadata; do not
  treat a GP INSTALL status as proof that Java installation completed. The
  corrected mapping was validated by the successful applet INSTALL and
  initialized registry record.
- [x] Use fresh install diagnostics to identify the `6A80` cause: the v1.8.2
  CAP calls JC 3.0.5 `Applet.register()` (external virtual method token 1), but
  the remap changed it to token 2, resolving `register(byte[], short, byte)`.
  The VM reported `StackUnderflowException` (id 1) at `invokevirtual` CP entry
  `0x179`, with one operand word and resolved method offset `0x305`; that method
  header has four arguments. The Applet token mapping now preserves token 1 and
  maps the later changed JC 3.0.5 tokens to the embedded API table.
- [x] Reflash and retry installation with the corrected Applet method mapping.
  ARM firmware flash passed; CAP INSTALL returned `9000`, and GPPro reports
  the applet as initialized. Current container cannot access PC/SC to continue
  process-level APDU testing.
- [x] Track calls to the imported JC305 `Applet.register` virtual methods during
  install and reject a successful VM return if no registration call occurred.
- [x] Store the registered applet class index beside the persistent AID using a
  CRC-protected CAP metadata version, while retaining version-1 CAP readability.
- [x] Implement `Applet.register()` and `register(byte[], short, byte)` so a
  successful Java install creates the persistent, selectable instance.
- [ ] Validate failed-install rollback and status words on hardware, then close
  power-loss atomicity.
  Synchronous rollback now snapshots persistent object/field writes, including
  indexed array and package-list updates, and the RP2350 overlay can erase
  transaction-created records. Failure stages now map to distinct status words
  (`6A80`, `6A82`, `6581`, `6985`, or `6F00`), with a focused host test passing.
  A host test now exercises the RP2350 overlay erase path, including collision
  probes and compressed child tags. Failure-path behavior still needs hardware
  validation; CAP metadata and filesystem state do not yet share a durable
  power-loss journal.

## Applet runtime and APDU lifecycle

- [x] Persist the registered applet object and its fields after Java install.
- [ ] Reconstruct the persisted applet object after reboot using its stored
  class index, then dispatch applet lifecycle methods. A `Context` restore
  helper now resolves the reserved root, verifies package/class identity, and
  eagerly decodes fields. CCID SELECT invokes it when version-2 metadata
  supplies the class index and retains the resulting context for the card
  session. Two fresh host VPCD emulator processes now restore the same saved
  applet and complete SELECT, GETVERSION, JCSystem info, supported-algorithm
  probes, SHA-256 doFinal, and repeated static SHA-256 calls. Power-cycle
  restoration on the RP2350 remains unverified.
- [ ] Dispatch SELECT, `select()`, `deselect()`, and `process()` through the
  interpreter for the selected applet. Ordinary `invokevirtual` now reads the
  receiver's concrete class and resolves its override through that class's CAP
  method table. The borrowed-context virtual method entry can seed receiver and
  argument locals; root short/reference/integer return values can now be
  captured without a synthetic caller frame. CCID SELECT now invokes
  `Applet.select()` and rejects a false result with `6999`; selecting another
  AID or powering off invokes `Applet.deselect()`. The APDU object/buffer is
  wired into `Applet.process()` dispatch and refreshed for each command. Host
  VPCD now restores the installed applet, and its `select()` returns true. The
  fresh applet integration pass confirms successful `process(APDU)` execution
  for JCSystem info, support probes, and SHA-256 operations. Separate callback
  tests for `deselect()`, rejected SELECT, and physical applet APDUs remain
  outstanding.
- [ ] Implement the remaining Java Card APDU buffer and incoming/outgoing
  command methods used by JCAlgTest, including response-length enforcement and
  `61xx` handling. Short cases 1, 2S, 3S, and 4S are parsed; case 2S/4S Le is
  exposed through `setOutgoing()` and `setOutgoingNoChaining()`, with zero Le
  interpreted as 256. Extended-length APDUs are rejected with `6700`.
  `getIncomingLength()` reads the current command's Lc through
  the native bridge, `getOffsetCdata()` returns the short-APDU data offset 5,
  and `receiveBytes()` returns 0 because CCID buffers the complete short APDU
  before dispatch. `getBuffer()` binds the Java-visible array to the same native
  array used to collect response bytes. The API reports T=1, IFSC 32, IFSD 254,
  and NAD 0 from the advertised ATR and CCID descriptor. The supplied applet
  predominantly calls `setIncomingAndReceive()` and `setOutgoingAndSend()`.
  Stateful `setOutgoing()`/`setOutgoingNoChaining()`, `setOutgoingLength()`,
  `sendBytes()`, `sendBytesLong()`, `setOutgoingAndSend()`, and
  `getCurrentState()` now use per-command state, append outgoing chunks, and
  enforce both the command Le and T=1 frame bound for no-chaining responses.
  The Java Card 3.0.5 `APDUUtil` CLA helpers now decode channels 0–19 and
  report command chaining, secure messaging, interindustry encoding, and CLA
  validity; the corresponding `APDU` methods read the current command's CLA.
  A standalone test passes 17 cases, and API CAP conversion plus the ARM build
  pass. The VPCD applet reaches its APDU entry and invokes the APDU path, but
  its first applet field access fails before a result can validate these
  helpers.
  `APDU.getCurrentAPDU()` now returns the same Java APDU object passed to
  `process()` and throws `SecurityException` during `select()`, verified in two
  VPCD applet cases. Physical validation remains blocked by missing Pico PC/SC
  enumeration. `APDU.getCurrentAPDUBuffer()` now returns the same buffer as
  `getBuffer()` during `process()` and throws `SecurityException` during
  `select()`, verified in two more VPCD cases. `APDU.waitExtension()` now
  rejects calls after `setOutgoingNoChaining()` with `ILLEGAL_USE` and accepts
  normal outgoing mode. The implementation cannot send a T=1 WTX while the
  VM synchronously processes a CCID command, so actual transport time-extension
  signaling remains unsupported.
  The bridge fragments data responses at
  short Le and serves `GET RESPONSE` using `61xx`. Parser, response-chain, and
  Context state tests pass; the Pico cycle also passed after flashing this
  implementation. An applet-level APDU exchange and PC/SC verification remain
  outstanding; extended APDUs are rejected.
- [ ] Complete the JCAlgTest v1.8.2-used native API surface, including the
  remaining transient array variants, `Util` helpers, `JCSystem` queries,
  `AID`, and
  Java exception mapping for unsupported crypto factories. `ArrayLogic` native
  copy/fill/compare/search now support the documented byte, short, and int
  primitive types (including big-endian byte encodings for short/int search);
  `JCSystem.getPreviousContextAID()` returns null because cross-applet context
  switching is unsupported. The buffer overload of
  `JCSystem.getAvailableMemory()` now distinguishes null, invalid-range, and
  invalid-type arguments in the native implementation, with focused tests and
  an ARM build passing. The Java Card wrapper now validates those arguments
  before the native call and throws Java `NullPointerException`,
  `ArrayIndexOutOfBoundsException`, or `SystemException.ILLEGAL_VALUE`, so
  those paths use ordinary Java exception dispatch. The ARM build regenerates
  the API CAP and ROM mask successfully; applet-level catch behavior remains
  physically unverified. Native C++ exception mapping for other methods is
  still incomplete. `JCSystem.getAID()` now creates a cached framework `AID`
  object only after successful registration or restoration; it returns null
  before registration. Focused VPCD install and fresh-process restore checks
  pass. Context byte retention is covered by the focused native test and ARM
  build. Physical `getAID()` validation and broader API coverage remain open.
  Transient primitive and object array factories now translate allocation failure
  into `SystemException.NO_TRANSIENT_SPACE`; the allocator backing constructor no
  longer terminates the VM on `std::bad_alloc`. Invalid event values report
  `SystemException.ILLEGAL_VALUE`. ARM build, VPCD cases 28–29, and a
  deterministic backing-allocation failure regression pass; physical low-memory
  validation remains open.
  `AID.equals(byte[],...)`
  and `partialEquals(byte[],...)` now return false for null arrays as required
  by the Java Card 3.0.5 contract; VPCD case 75 exercises null and valid full /
  prefix comparisons through the applet runtime. Physical validation remains
  open.
- [x] Implement `JCSystem.makeTransientObjectArray()` with Java-null default
  entries and a wildcard component type that accepts any reference. ARM build
  and focused factory tests pass; installed-applet use remains unverified.
- [ ] Verify that an installed applet instance and its persistent fields survive
  a power cycle, not only that its CAP blob survives. The new
  `./tools/test-vpcd-persistence` regression commits instance field `0x33` in
  one emulator process, then starts a fresh process from its saved flash image
  and verifies `0x33`, static reference identity, persistent array-child
  identity, object-field aliasing, and restored array type. The restore phase
  passed 7/7 checks; this models a process restart, not a physical power-off/on.
  Hardware persistence still needs a true power cycle and PC/SC access.

## Card manager and host integration

- [x] Make PC Java API and ROM-mask generation incremental, keyed to their
  inputs. A full regeneration took 1:01 for the Java API and 5:13 for Maven;
  after adding CMake output stamps, the follow-up PC build skipped both stages.
- [ ] Implement and verify GP DELETE and the remaining required GET STATUS / GET
  DATA behavior, including accurate package and applet lifecycle state.
  GET STATUS now handles ISD (`P1=80`), applications (`40`), executable load
  files (`20`), and load files with modules (`10`), with mandatory leading AID
  search criteria, wildcard/RID matching, and basic first/next request state.
  The ISD-only query correctly ignores its AID qualifier. Its bounded TLV
  encoders, search/response builder, and next-occurrence validation pass 44
  checks, and the ARM
  images build. GP DELETE now has authenticated `E4` dispatch, strict AID TLV
  parsing, persistent applet/package tombstones, instance/static record cleanup,
  and reserved CAP-slot erase for package deletion. The parser passes 11 checks;
  ARM and PC builds pass, and the rebuilt host VPCD applet regression passes
  11/11. The DELETE APDU itself, restart/GET STATUS after deletion, and physical
  hardware behavior remain unverified. GET DATA and complete continuation
  behavior also remain open.
- [x] Integrate the PC-build vpcd frame transport with power controls, ATR,
  applet SELECT/DESELECT, and APDU dispatch through the VM lifecycle bridge.
  The PC target builds, the frame codec passes 14 checks, and a new session
  state-machine test passes 14 checks. The bounded release VPCD applet path
  passes 11 checks on each of two fresh emulator processes, including SHA-256
  `doFinal` and the 50-call static APDU (`TEST:SUMMARY pass=22 fail=0`). The
  full official client is not yet validated against this transport; the
  current PC/SC service is unavailable and the VPCD reader driver is not
  installed. A later official `ALG_PERFORMANCE_STATIC` attempt disconnected at
  SHA-256 `doFinal`; after gating release filesystem trace output, the focused
  50-call SHA-256 VPCD APDU passed in 15.578 s. Simulator sub-millisecond SHA
  timings are below its timer resolution. Repeat the official test by exact
  reader name when PC/SC is available; physical throughput and digest KAT remain
  unverified.
- [ ] Add golden applet coverage for exceptions, transient arrays, statics,
  APDU exchange, install parameters, and persistence. In addition to VPCD
  session tests with fake VM callbacks, the installed JCAlgTest 1.8.2 applet
  now completes a focused real-VM host integration pass. Broader bytecode and
  persistent-state assertions still need dedicated golden tests.
- [ ] Run the matching JCAlgTest v1.8.2 client in supported-algorithms mode and
  archive its CSV and logs with hashes.
- [x] Generate a simulator reference using the official v1.8.2 client and its
  bundled jCardSim; the complete `ALG_SUPPORT_EXTENDED` CSV and run log are
  archived. Cross-comparison against a CHOUPI card result is still pending.
- [ ] Compare the CHOUPI supported-algorithms CSV against the jCardSim
  reference and CHOUPI's declared supported-algorithm list.
- [ ] Complete hardware integration: an earlier ARM image passed its finite
  runtime check, and GPPro `--debug --list` previously listed ISD
  `A000000151000000` in `OP_READY`. The later snapshot image currently emits
  repeated boot `6F00` failures; runtime exception diagnostics are now built
  into ARM but need a permitted flash/serial run to identify the cause. Current
  PC/SC access is unavailable. Windows enumeration, current-image
  install/select/process/delete, and physical power-cycle persistence remain
  unverified.
- [ ] Pass AC1–AC5 and the plan's clean-flash integration sequence three times.

## Later / optional

- [ ] Add RP2350 TRNG and SHA-256 support, then additional mbedTLS algorithms,
  each with known-answer tests (plan WP8).
- [ ] Complete deferred hardening and robustness work, including malformed CAP
  and APDU fuzzing and interrupted persistent-write recovery (plan WP9).
