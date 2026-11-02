# Issue knowledge base — compacted 2026-10-29

## Two physical JCVM readers pass read-only runtime checks — 2026-11-02

- **Inventory:** After the user reported three boards in BOOTSEL, the
  authorized helper exposed two runtime readers, not BOOTSEL devices:
  `8E464F4E2BBC0A19` and `2BED26ED20556535`. `pcsc_scan -r` listed both.
- **Validation:** Both identified cards passed `./tools/monitor --seconds 8`
  with `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The
  repository's read-only `tools/test-pico-powerup` check passed on each:
  JCAlgTest SELECT, GETVERSION `1.8.2_jc305`, and JCSystem info all returned
  `9000`. Logs: `artifacts/logs/pico-powerup-persistence-20261102T075746Z.log`
  (SHA-256 `dcc14c18763b93dd9ab325312deeb648f2d2e48a865c76d7e9782fb638e9b0b3`)
  and `artifacts/logs/pico-powerup-persistence-20261102T075821Z.log`
  (SHA-256 `639886744de9679f9ffaa63090492f449cbac3cc583f2268dc7f6b99453a8ba2`).
- **Difference:** Both returned the same JCAlgTest version and JCSystem data.
  Their installed package sets differ: the first lists JCAlgTest, A000...0201,
  and ApiSec; the second lists JCAlgTest, Api, and A000...0002/0003/0004.
  This is a card registry difference, not a difference in the tested calls.
- **Limits:** No BOOTSEL device was visible; no flash, reset/power-cycle,
  install, or mutation was performed. This does not validate persistence across
  a physical power cycle or the pending SIO behavior.
- **Viable procedure:** Refresh the helper before each USB operation, use the
  full serial-qualified reader name when multiple cards are present, and pass
  that same serial to the finite monitor. The no-serial monitor correctly
  refuses ambiguous multi-device selection.

## Reported BOOTSEL devices may enumerate as runtime JCVM CCIDs — 2026-11-02

- **Latest observation:** `PICO2_HOST_USB_ACCESS=1 sudo -n
  /usr/local/sbin/pico2-pcsc-reader && lsusb -d 2e8a:` granted two USB nodes;
  both were runtime `2e8a:0009 Pico2 Pico2 JCVM CCID` at bus 001 devices 003
  and 004. No BOOTSEL device appeared, despite the report of three inserted
  boards.
- **Viable inventory procedure:** Use the authorized helper first, followed
  by `lsusb -d 2e8a:` through elevated host access. Treat the returned USB
  node/device identities as current truth; a host or user report of BOOTSEL
  does not establish that the VM can flash those boards. Re-enumerate before
  each operation because bus/device addresses change after reconnects.
- **Action:** No flash was attempted. Keep the two visible runtime readers
  available as a failover pool, and refresh the inventory before physical
  testing.

## Firewall context is per CAP file, not per installed applet instance — 2026-11-02

- **Observed implementation:** `findAppletContext()` matches an inactive
  `Context` by owner ID and complete instance AID. On an absent match,
  `choupi_vm_find_or_restore_applet_context()` creates a new `Context` and
  `Heap` for that installed instance. Persistent owner IDs are likewise
  allocated per installed applet instance.
- **Specification:** Java Card RE 3.0.5 §6.1.2 requires a context for each CAP
  file and says applet instances within one CAP file share that context, with
  no firewall between them. Section 6.1.3 separately associates each object
  with an owning applet instance. The current runtime therefore conflates
  firewall context, object owner, and installed applet instance. This affects
  same-package object access, static state, SIO context switching, and DELETE.
  Source: [Oracle Java Card RE Specification 3.0.5, §§6.1.2–6.1.3](https://docs.oracle.com/cd/E59935_01/JCCRE.pdf).
- **Viable direction:** Introduce CAP/package-scoped execution heaps and
  firewall contexts, with separate per-instance AID/lifecycle/owner records.
  Same-CAP sibling calls should not cross a firewall, but object ownership and
  per-instance deletion must still be enforced where required. Treat this as
  prerequisite architecture for cross-context foreign-reference transport;
  do not relax the current fail-closed behavior before the separation exists.
- **Validation:** Read-only review of `context_registry.hpp`, the context
  retention/restoration path in `main.cpp`, owner allocation in `cap_load.cpp`,
  and the official 3.0.5 RE specification. No code, build, or hardware change
  was made in this review.

## Host MPU emulator must bypass the region lock for privileged accesses — 2026-11-02

- **Observed:** The PC SIO VPCD emulator exited before accepting a connection
  while running its first filesystem remote call. The fault was at emulated RAM
  offset `0x17e98`; diagnostics showed `privileged=true`, but both effective
  permission lookups returned `PROT_NONE` because the MPU region registry was
  busy.
- **Cause:** `uniform_page_protection()` and `allows_addr()` attempted a
  nonblocking lock before checking the privilege state. A privileged fault can
  interrupt a region update while that lock is held. The host MPU model must
  grant privileged RWX access without consulting the unprivileged region map.
- **Viable fix:** Check privilege first in both permission lookup paths and
  return RWX before touching the registry. Keep the registry fail-closed for
  unprivileged accesses. A focused regression deliberately holds the region
  lock, protects a RAM page, and verifies a privileged read/write succeeds.
- **Validation:** `RUST_TEST_THREADS=1 ./tools/env cargo test --manifest-path
  choupi/os/Cargo.toml --no-default-features --features host,big_ram --lib
  arch::host::mpu_ll::speculate_0` passed 11/11 with host ptrace permissions.
  `./tools/build-pc --profile current` passed. The cross-AID SIO VPCD suite
  passed 15/15 after this fix. The test log is
  `artifacts/logs/sio-cross-aid-vpcd-20261102T073510Z.log`; performance impact
  was not measured. Build output still has pre-existing Rust warnings.
- **Diagnostic aid:** Set `CHOUPI_LOG_HOST_MPU_PERMISSIONS=1` to log the
  permission result and matching region entries at a host MPU fault.

## Global Object[] must preserve foreign-reference ownership — 2026-11-02

- **Specification finding:** Java Card RE 3.0.5 §6.2.8.2 explicitly permits
  access to a global array from any context. Its `aastore` exclusions are
  references to temporary JCRE entry-point objects and global arrays; ordinary
  applet-owned references are not excluded. A later operation on a stored
  applet reference remains subject to normal ownership checks. Thus the current
  rule rejecting every non-null value stored in a global Object[] is stricter
  than the specified behavior. Source: [Oracle Java Card RE Specification
  3.0.5, §§6.2.2 and 6.2.8.2](https://docs.oracle.com/cd/E59935_01/JCCRE.pdf).
- **Current test mismatch:** transaction probe case 41 currently expects
  `global[0] = this` to throw `SecurityException`; that assertion should change
  only with an ownership-safe implementation, not as a standalone test edit.
- **Why direct relaxation is unsafe:** global arrays use shared 16-bit
  references, while each applet heap independently allocates the same compact
  handle range. Returning a foreign handle unchanged in another context could
  resolve to an unrelated local object. In addition, persistent references
  require GC roots, deletion protection, reset behavior, and stale-owner
  handling.
- **Viable implementation direction:** add an ownership-preserving reference
  indirection for global Object[] entries and other cross-context parameter
  paths. Reads in the owner context may resolve to the original handle; reads
  in another context must remain opaque and fail every unauthorized object
  access. Keep the existing restriction until the indirection, GC, deletion,
  persistence, and collision tests are complete.
- **Reproduction:** `./tools/test-jcsystem-transaction-applet --case 41`
  passed in 85.593 s because the applet caught `SecurityException` on
  `global[0] = this`; APDU response was `019000`. Log:
  `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261102T042104Z.log`
  (SHA-256 `8cf6cb16df35f096ee1fa3c3befcc3a99adb48ef5e13e307aab1c1f2f114e1e4`).
  This proves the implementation/test behavior but also confirms the spec gap.
  ARM/RISC-V builds and both Descriptor-mode SIO suites passed in the preceding
  worktree validation. No implementation change was made in this step.

## SIO reference parameters cross the call, not the firewall — 2026-11-02

- **Specification finding:** Java Card RE 3.0.5 §6.2.4.3 says an SIO method
  may access parameters passed by its client. This does not grant access to
  client-private objects: §§6.1.4 and 6.2.8 require object-access checks in
  the currently active context, and deny array, field, and ordinary method
  access when the referenced object belongs to another applet context. Global
  arrays are separately accessible; restrictions on storing global references
  remain in force. Source: [Oracle Java Card RE Specification 3.0.5,
  §§6.1.4, 6.2.2, 6.2.4.3 and 6.2.8](https://docs.oracle.com/cd/E59935_01/JCCRE.pdf).
- **Current implementation risk:** Each applet `Heap` allocates compact
  `jref_t` handles independently. Copying a client-private handle unchanged
  into a server frame could resolve to a different server-owned object with
  the same compact value. Rejecting it at the call boundary avoids that alias,
  but may be stricter than the SIO parameter contract: preserve the argument
  as an opaque reference and test the specified bytecode access behavior.
- **Viable implementation direction:** Introduce an explicit cross-context
  reference representation or invocation-scoped handle table that preserves
  the originating context and original handle. Do not let the server dereference
  a client-private object: §6.2.8.2 permits array operations only for arrays
  owned by an applet in the currently active context or designated global
  arrays. Verify that a private reference can be passed without aliasing and
  that attempted array/object access throws `SecurityException`. Define safe
  return and storage behavior, and test collisions between equal compact
  handles in two heaps before enabling transport. Until then, retain the
  existing call-boundary rejection as the safe incomplete behavior.
- **Validation:** Read-only review of official Oracle Java Card RE 3.0.5
  §§6.1.4, 6.2.4.3, and 6.2.8.2, plus the SIO marshaller, `jref_t`, and
  per-applet heap ownership model. The parameter statement does not override
  the array ownership rule. No source change, build, or test was performed in
  this research step.

## Typed operand pops must preserve SIO runtime kinds — 2026-11-02

- **Observed risk:** Runtime type tags protected cross-context SIO arguments at
  marshalling and server-local reads, but `pop_Short()`, `pop_Reference()`,
  `pop_Int()`, and `pop_ReturnAddress()` consumed raw words without checking
  their tags. Malformed CAP bytecode could pop a reference as a short and push
  it back as a primitive, defeating the metadata firewall.
- **Viable fix:** Typed pop methods validate the top category before changing
  TOS. Int pops additionally require ordered low/high words; mismatches throw
  `SecurityException`, and insufficient operands throw
  `StackUnderflowException`. Typed pops can now throw, so their prior
  conditional `noexcept` declarations were removed. Untyped `pop` remains
  available for the Java Card `pop`/`pop2` instructions.
- **Validation:** `./tools/test-rmi-registry` passes valid pops and rejects
  reference-as-short, primitive-as-reference, primitive-as-return-address,
  and reversed int words without consuming the value. `./tools/build-pc
  --profile current`, `./tools/build arm`, and `./tools/build riscv` pass. The
  SIO VPCD fixture passes 15/15 and VPCD multipackage lifecycle passes 47/47.
  Logs: `artifacts/logs/sio-cross-aid-vpcd-20261102T034712Z.log` (SHA-256
  `624cc196eb4417cf1e6948bc28e0d61aacdc231a5d396b9f3077ccd9e20bb786`) and
  `artifacts/logs/host-vpcd-multipackage-apdu-20261102T035136Z.log` (SHA-256
  `54a6fa69c91cc49c50db2abcfb2d0196d115018df3767224d678e1778f5f95d1`).
- **Hardware:** Final helper/USB refresh still finds only runtime CCID
  `2e8a:0009` at `/dev/bus/usb/001/055`; the three reported BOOTSEL boards are
  not guest-visible. No flash or physical check was attempted.

## Persistent object aliases must retain the Shareable proxy marker — 2026-11-02

- **Observed:** After process restart, a saved Shareable proxy returned through
  a persistent object alias could be reconstructed as an ordinary instance.
  Since its runtime route registry is intentionally empty after restart, the
  missing marker meant firewall checks could fail to recognize it as stale.
- **Cause:** The direct persistent-object restore path restored the internal
  `FIELD_TYPE_SHAREABLE_PROXY_OBJECT` marker, but
  `getPersistentField_ReferenceImpl()`'s `kPersistentObjectAlias` path rebuilt
  a `JC_Instance` without copying that marker from its canonical target header.
- **Viable fix:** In both alias cases (target already loaded or rebuilt from
  storage), check the target header and reapply
  `setShareableProxyPlaceholder()` for the reserved proxy field type. Keep
  Java-visible field queries normalized to ordinary `FIELD_TYPE_OBJECT`.
- **Regression:** The SIO fixture stores one proxy in two persistent client
  fields, then reads the alias field first after emulator restart. The stale
  proxy is rejected with a caught `SecurityException`. The complete VPCD SIO
  fixture passes 15/15, including this alias case;
  `./tools/test-rmi-registry`, PC, ARM, and RISC-V builds pass. Log:
  `artifacts/logs/sio-cross-aid-vpcd-20261102T032628Z.log` (SHA-256
  `d6925d8315d57309e4f63673f5b8f8ff1fd106c28494ed7c411a1508b4532d49`).
- **Persistence boundary:** Routing metadata is still process-local. After
  process restart, saved proxies intentionally fail closed; rehydration is not
  implemented. Do not remove this boundary without stable identity, lifecycle,
  transaction, and deletion coverage.
- **Hardware:** The user reports three boards in BOOTSEL; the authorized helper
  and elevated `lsusb -d 2e8a:` still show only one runtime `2e8a:0009` device
  at `/dev/bus/usb/001/055`. No BOOTSEL device was guest-visible and no flash
  was attempted. Retry after passthrough enumeration changes.

## Enforce descriptor-free SIO argument kinds at server local loads — 2026-11-02

- **Implementation:** The VM stack tracks each word as unknown, primitive,
  reference, int-low, int-high, or return-address. Cross-context
  `invokeinterface` uses these tags when GPPro omits Descriptor. Typed pushes,
  local writes, frame-local initialization, `dup`, `swap`, and stack element
  replacement maintain the tags. Three-bit storage costs 96 bytes for a
  256-word stack. Typed local reads compare the requested kind with the stored
  kind and throw `SecurityException` on mismatch. `astore` distinguishes
  references from return addresses so `ret` can enforce its expected kind.
- **Firewall:** Unknown values and applet-private object/array references are
  rejected. Null and JCRE-global references may cross. Int words must appear
  as an ordered low/high pair; isolated or reversed words are rejected. The
  callee's bytecode cannot load a marshalled word under a different primitive,
  reference, int, or return-address kind. No type is guessed from payload
  bits. Present
  Descriptor data continues through its existing signature validation path;
  malformed Descriptor data does not fall back to tags.
- **Validation:** `./tools/test-rmi-registry` passes kind-accept/reject cases,
  including invalid int-word order. The three-package VPCD suite passes 14/14
  with Descriptor omitted and 14/14 with Descriptor present. The broader
  multipackage VPCD test passes 47/47; transaction probe case 48 passes 3/3.
  Logs include `artifacts/logs/sio-cross-aid-vpcd-20261102T025926Z.log`,
  `artifacts/logs/sio-cross-aid-vpcd-20261102T030801Z.log`, and
  `artifacts/logs/host-vpcd-multipackage-apdu-20261102T030138Z.log`. PC, ARM,
  and RISC-V builds pass. No hardware validation was performed.
- **Physical state:** The user reports three devices in BOOTSEL. The latest
  authorized helper/USB refresh still sees one runtime `2e8a:0009` CCID and no
  BOOTSEL devices. No firmware was flashed. Refresh enumeration later; do not
  treat the report as guest-visible access.
- **Remaining:** Validate typed descriptor-free calls on physical hardware,
  then broaden stack-kind coverage if any CAP bytecode path can write an
  operand word without a typed Stack API. Continue CTX-01 firewall and
  persistent-proxy acceptance.

## GPPro-loaded CAP omits Descriptor data needed for typed cross-context SIO — 2026-11-02

- **Observed:** The runtime `JC_Cap` for the pre-existing SIO contract package
  and for freshly installed packages reports no Descriptor component. The fresh
  generated CAP ZIPs do contain `Descriptor.cap`. In the fresh two-package
  physical probe, both applets installed and the client selected, but its
  cross-AID call returned `6F00`; serial localized the failure to SIO signature
  lookup (`descriptor=0`, interface resolved, server method and argument count
  resolved). The captured CDC evidence is
  `artifacts/logs/sio-two-package-cdc-20261102T013345Z.log`; the probe result is
  `artifacts/logs/sio-two-package-probe-20261102T013345Z.log`.
- **Cause confirmed in the pinned GPPro loader:** `GPSession.loadCapFile()`
  passes `cap.getCode()` into LOAD block `C4`. In the local `capfile-26.02.22`
  library, `CAPFile.getCode()` invokes `_getCode(false)`, which explicitly
  skips both `Descriptor` and `Debug` components. Thus standard GPPro LOAD
  intentionally omits Descriptor; the physical APDU stream was not separately
  captured. Existing CAP loading permits omitted components while retaining
  original Directory sizes; see the CAP Directory note below.
- **Safe partial fix:** `invokeinterface` distinguishes unavailable Descriptor
  from malformed/present Descriptor data. If it is unavailable, a cross-context
  method may proceed only with one operand word (the receiver) and a target
  method header with exactly one argument slot. Explicit parameters still fail
  closed because their types and reference ownership cannot be established.
  Malformed present Descriptor data also fails closed. The focused registry
  test checks receiver-local construction and rejects extra words/null receivers;
  ARM and RISC-V current builds pass.
- **Next type-source requirement:** Since standard GPPro LOAD omits Descriptor,
  general cross-context typed arguments cannot depend on it. Investigate
  runtime operand/local type tracking or another on-card-verifiable source;
  never infer primitive/reference types from raw words. Keep explicit arguments
  fail-closed until that source is implemented and tested.
- **Limit:** This fallback has not yet been exercised by a positive physical
  receiver-only call. Existing physical fixtures were cleaned up without
  removing baseline card packages. At the latest check the VM exposed only one
  Pico, and it was runtime CCID (`2e8a:0009`), not BOOTSEL; the other two boards
  reported by the user were not guest-visible. Refresh access before hardware
  work. Keep CTX-01 open for omitted-Descriptor physical coverage, explicit
  argument behavior, and end-to-end firewall acceptance.

## Physical GP INSTALL stage 9 and parent-encoded compaction — 2026-11-02

- **Observed:** Repeating GPPro `--install-only --debug` for the loaded SIO
  client package returned INSTALL stage `09` and SW `6F00`. Concurrent finite
  CDC capture showed `CHOUPI:FS_WRITE_FAIL op=new tag_len=4 size=2
  files=8192/8192 free_slots=0 ... tag=02FF3D01` while writing lifecycle
  state. This is direct evidence of overlay-index exhaustion; the earlier
  stale/mismatched dependency hypothesis is not supported by this attempt.
- **Cause of initial non-fix:** `fs_write()` retried compaction, but the old
  compactor returned without work when any legacy parent-slot-encoded record
  existed. That prevented reuse of deleted slots on the card.
- **Fix:** Compaction now retains parent ancestors required by kept records and
  remaps encoded parent-slot references after rehash. `fs_write()` invokes this
  compactor on capacity pressure and resolves the target tag again after the
  rehash. If live records still leave insufficient capacity, it returns
  `FS_STATUS_OUT_OF_FLASH`.
- **Validation:** `./tools/test-rp2350-fs-overlay` passes with a full 8,192-slot
  table containing parent/child encoded records; a new write reclaims one
  tombstone, preserves and remaps the parent reference, and survives commit and
  restart. ARM and RISC-V current builds pass. The ARM image passed a physical
  cycle and finite serial monitor on `2BED26ED20556535`. On that card,
  GPPro INSTALL initially failed at 8,192 records, then after the remap fix
  serial reported `FS_COMPACT files=8192->696 bytes=82578->16749`; GPPro
  `--install-only` for both already-loaded client and service CAPs returned
  `9000`.
- **Limit:** A physical cross-AID invocation using these existing package AIDs
  still returned `6F00` with `SecurityException` at client `invokeinterface`.
  The packages predated this test and their code identity is unverified, so this
  is not a valid clean-fixture acceptance result. Preserve them; use distinct
  package AIDs or a clean identified board for the next physical SIO probe. The
  VM currently exposes one runtime CCID reader, not the three BOOTSEL devices
  reported by the user.

## Persisted Shareable proxy loses routing metadata after process restart — 2026-11-01

- **Observed:** The three-package VPCD fixture stores a Shareable proxy in a
  persistent client-applet field. It works across APDUs and normal heap GC, but
  after restarting the PC emulator process the old reference no longer has an
  entry in the runtime-only `Context::shareable_proxies` table. A fresh
  `getAppletShareableInterfaceObject` lookup still succeeds. Before the fix,
  invoking the old placeholder dispatched the service implementation in the
  client context (service marker `6F01`).
- **Cause:** The client heap persists the placeholder object reference, while
  proxy routing metadata (server AID and server reference) is only held in
  process memory and is not serialized or reconstructed.
- **Viable safety fix:** Persist a reserved internal instance-header type for
  Shareable proxy placeholders, decode it back into an internal
  `JC_Object` marker, and normalize it to ordinary `FIELD_TYPE_OBJECT` when
  Java Card field semantics inspect it. Bytecode paths reject marked
  placeholders missing from the runtime proxy registry across interface,
  virtual and special calls, casts, and instance-field operations. Active
  registered proxies remain eligible for guarded SIO dispatch. The VPCD
  fixture verifies fresh lookup after restart, caught `SecurityException` for
  stale proxy calls and virtual dispatch, and fail-closed behavior after card
  reset. Full VPCD test passes `14/14`; `tools/test-rmi-registry` passes.
- **Remaining limitation:** This prevents execution in the wrong applet
  context; it does not rehydrate persistent proxy references. Define whether
  such references should persist across process restart. If so, store routing
  metadata using a stable persistent proxy identity and test interrupted writes
  and deletion. Do not weaken the fail-closed check to make the old reference
  appear usable.
- **Physical status:** The user reports three boards in BOOTSEL, but the latest
  authorized reader helper sees one runtime device (node
  `/dev/bus/usb/001/041`, serial `2BED26ED20556535`); no other PC/SC reader is
  available to continue on.
- **Physical validation:** Current ARM image was flashed to this device using
  `./tools/cycle choupi --profile current`; picotool's reset-interface path
  failed, the wrapper recovered with CDC 1200-baud BOOTSEL, and the flash
  verified. Serial health passed (`TEST:SUMMARY pass=1 fail=0`). Existing
  contract/service fixture packages made the clean physical test abort safely.
  GPPro `--install` returned `6A80`, although GET STATUS then listed the client
  package. A subsequent `--install-only --debug` returned extended INSTALL
  stage `09`, VM status `0000`, and `6F00`; GET STATUS shows the package but
  SELECT gives `6A82`. Deleting its noninstalled applet AID returns “not
  present”; deleting the package returns `6581`. Preserve this test-created
  package and the pre-existing dependency packages. This physical install issue
  is separate from USB passthrough and remains unexplained. A read-only SELECT
  of the pre-existing service AID `A00000015100000301` also returns `6A82`;
  GPPro's APP rows here describe applets in loaded CAP packages and do not
  prove that applet instances were installed. Fresh VPCD external-link fixtures
  pass INSTALL for a valid exported interface (`9000`) and reject an exported
  class or missing export (`6F00`), all 3/3. This supports, but does not prove,
  a stale/mismatched pre-existing dependency package as the cause of the
  physical stage-9 result. Logs: `artifacts/logs/rmi-external-link-valid-interface-20261102T000421Z.log`
  (SHA-256 `aac5570d8c5cbc8caa812996f8d9eb6951c49e9a2487dcbe1a6ce76fa72965a2`),
  `artifacts/logs/rmi-external-link-exported-class-20261102T000459Z.log`
  (`eb47dd4fb5ac3e62e938d73b82ee4f2a677a34f6005ca448419a8b931bdf34b2`),
  and `artifacts/logs/rmi-external-link-missing-export-20261102T000533Z.log`
  (`3fae07042f5865e22119b808ca7fcd7fa7e3a1b8684e1719988d55748af18e52`).

## Cross-AID physical fixture install reaches internal stage 9 — 2026-11-01

- **Observed:** The test-only contract CAP package
  `A000000151000002` loads on Pico 2 `2BED26ED20556535` and appears in GPPro
  GET STATUS. The service package `A000000151000003` is also present, but
  installing its applet through `gp --create` returns an extended install
  response with failure stage `09`, VM status `0000`, exception/opcode/PC all
  zero, and final SW `6F00`. GET STATUS shows no selectable service applet.
  Deleting this test package previously returned `6581`; preserve the card's
  other packages and do not treat the cleanup failure as unrelated firmware
  corruption.
- **Earlier sequence:** Initial `gp --install service.cap` returned `6A80`
  during the final LOAD block. GPPro had printed success for the contract LOAD,
  but GET STATUS did not show that package. Explicitly reloading the contract
  made it resident; a subsequent service install progressed past import
  validation and failed with stage 9. The physical test script now verifies
  contract residency immediately after LOAD to catch this earlier condition.
- **Limit/root cause:** Stage 9 is the catch-all around static initialization,
  runtime applet installation, and install-journal publication. Zero VM status
  and zero VM-exception telemetry do not identify which C++ exception or phase
  failed. The same three-package CAP fixture passes on fresh PC VPCD storage
  (`45679000`, 4/4), so do not attribute stage 9 to CAP generation or claim the
  cross-AID VM path physically verified. Next add narrow install-phase
  diagnostics or repeat on a clean physical card; the current VM view exposes
  only 2BED as runtime `2e8a:0009`, despite the user reporting three devices in
  BOOTSEL.

## JC 3.0.5 converter int signatures require `-i` — 2026-11-01

- **Symptom:** The JC 3.0.5 converter rejected an `int` method in the
  test-only Shareable contract with `unsupported int type parameter`.
- **Cause and viable option:** This kit's converter defaults optional 32-bit
  integer support off. Put `-i` after `com.sun.javacard.converter.Main` (it is
  a converter argument, not a JVM argument) for each package that declares or
  implements the int method. CAP generation then succeeds; it emits the
  expected warning `This package requires int support.` Keep the option in
  generated test fixture builds when exercising VM int descriptors.
- **Validation:** Three fixture packages generated with zero errors; the
  positive VPCD cross-AID int argument/return probe passed, returning
  `765432109000`. Upstream CAPs remain untouched.

## Nested SIO exception unwinding — partial VPCD coverage — 2026-11-01

- **Validation:** A test service throws an uncaught Java Card
  `SecurityException`; the cross-context proxy converts the failed nested call
  into a caller-context `SecurityException`, which the client catches. A
  second SIO call then succeeds (`45679000`), proving the tested exception path
  restores usable client and service contexts.
- **Limit:** This does not establish preservation of exception object identity,
  ISOException status words, or arbitrary checked/runtime exception semantics.
  The proxy currently raises `SecurityException` for any non-`9000` nested
  status; review Java Card firewall rules and test other exception types before
  broadening the claim. Physical exception coverage is blocked by the current
  stage-9 applet-install failure.

## Client-stored Shareable proxy survives end-of-APDU heap GC — 2026-11-01

- A generated client applet stores a Shareable proxy in one of its own
  instance fields, returns from the APDU so normal heap collection runs, then
  invokes the stored proxy on the next APDU. Fresh VPCD install passes and the
  second call returns `45679000`; the service context and proxy metadata remain
  usable across the collection boundary.
- The same fixture's direct `Object.equals` invocation on the proxy is rejected
  with `SecurityException`, confirming that ordinary virtual methods cannot
  inspect the placeholder.
- This covers the active emulator context only. It does not verify context
  eviction, reset/process restart, persistent restoration of proxy metadata, or
  physical heap collection.

## CAP Directory sizes describe the original CAP when GP LOAD omits components — 2026-11-01

- **Symptom:** A new strict Directory check caused a VPCD multipackage INSTALL
  to return `6F00` when GP LOAD data was assembled from only selected CAP
  components.
- **Cause:** GP LOAD input can omit components from the original CAP while
  retaining the original Directory component, whose size table still records
  those omitted components. For example, the multipackage harness omits Export
  and Descriptor while loading the upstream Core package.
- **Viable validation rule:** After parsing all frames, require each present
  component's declared Directory size to equal its actual payload size. Do not
  require absent components to have a zero Directory entry. The implementation
  is in `choupi/src/jc_handlers/jc_cap.cpp`. The ASan/UBSan parser harness
  accepts all 48 supplied deploy CAPs and its GP LOAD subset fixture; the
  authenticated VPCD multipackage lifecycle passes 47/47. Keep this distinction
  when adding further cross-component CAP checks.
- **Physical follow-up:** ARM/RISC-V builds passed, but flash/monitor validation
  remains open because the authorized PC/SC helper reported zero guest-visible
  Pico readers on 2026-11-01.

## TinyUSB `tud_task()` could drain indefinitely under replenished USB traffic — 2026-11-01

- **Cause:** The pinned TinyUSB v0.18.0 API processes the device queue until it
  is empty. On the Pico OSAL, the `timeout_ms` argument does not bound this
  drain. A sustained setup/control event stream could therefore keep one
  cooperative JCVM service call busy for an unbounded number of queued events.
- **Viable event-count fix:** `tools/patches/tinyusb-0.18.0-event-budget.patch`
  adds `tud_task_budgeted(max_events)` to the exact pinned TinyUSB commit
  `86ad6e56c1700e85f1c5678607a762cfe3aa2f47`. It returns the processed-event
  count, never waits, and leaves unprocessed FIFO entries queued. The repository
  wrapper `tools/apply-tinyusb-event-budget` checks the commit and applies the
  patch idempotently; `tools/build` invokes it automatically. JCVM service
  points limit each call to eight events. PC, ARM, and RISC-V builds pass.
- **Limit:** This bounds the event count, not a measured worst-case duration.
  The runtime of one callback is not capped by the counter. A sustained USB
  control-request flood concurrent with the RSA-3072 APDU, extension/final
  response-order verification, and wall-time measurement remain outstanding.
  The physical cycle passed on `2BED26ED20556535` (`current`) and
  `8E464F4E2BBC0A19` (`max_crypto`), but later the 8E PC/SC reader disappeared.
  The temporary crypto test CAP install on 2BED returned `6A80`; cleanup
  confirmed the CAP was absent. A later helper refresh reported zero devices
  and `lsusb -d 2e8a:` found none. Resume hardware validation only after Pico
  devices enumerate again.

## picotool selection was ambiguous with multiple runtime Picos — 2026-11-01

- **Symptom:** `picotool load --force --ser <firmware USB serial>` returned
  `Command requires a single RP-series device to be targeted` when multiple
  RP-series runtime devices were attached. The wrapper had already resolved
  the intended board using its USB serial, but picotool's `--ser` refers to
  the bootrom selector and did not uniquely target the CDC serial.
- **Implemented remedy:** `tools/flash` now resolves the intended serial or
  chip ID through sysfs, then selects the current device for each picotool
  invocation by its live bus and address. `target_sysfs_path` is refreshed
  across USB re-enumeration, so BOOTSEL gets a newly resolved bus/address too.
  `bash -n tools/flash` passes. The modified multi-device selector has not yet
  been physically tested because the host lost all Pico USB enumeration during
  this session; verify it on the next multi-device flash.

## Signature MAC parameter IV was ignored — 2026-11-01

- **Symptom:** The full API 3.0.5 Crypto run reported `WRONG_RESULT` for
  `ALG_DES_MAC8_NOPAD` with IV, `ALG_AES_MAC_128_NOPAD` with IV, and
  `ALG_DES_MAC8_ISO9797_M2` with IV.
- **Cause:** `javacard_security_Signature_init(..., parameters, ...)` copied
  the provided block IV into the Signature instance, but `runMac()` discarded
  it and always supplied an all-zero vector to `computeSymmetricMac()`.
- **Viable resolution:** Read the block-sized IV from the Signature
  `INITIALVECTOR` field when computing a MAC. Regression coverage for all
  three algorithms uses nonzero IVs for sign and verify. The complete PC VPCD
  symmetric cipher/MAC suite passes 79 vectors; ARM and RISC-V `max_crypto`
  wrappers pass. Physical rerun of the API 3.0.5 rows remains required because
  the authorized device helper enumerated zero readers on 2026-11-01.
- The unchanged API 3.0.5 Crypto CAP and official client also passed all three
  IV calls over PCSC Virtual PCD (463 other calls skipped; CAP cleanup passed).
  A full Crypto VPCD run timed out at its 800-second client limit with 190
  partial rows and no completed API-area CSV rows; this is not a pass. Captured
  output is `artifacts/results/deploy/vpcd-iv-fix-20261101/crypto-iv-filtered.csv`
  (SHA-256 `b085ef9ed89517e8e6d9f467ea9bc63ad7e7a16963c5ef59e66515273be58006`)
  and partial client output is `crypto-full-client-partial.csv` (SHA-256
  `66637996f43c46ec4c4d254bb6adf899b4fb3ace920db9018e531fb9c2440b3e`).
- A separate API run selecting only `ALG_RSA_SHA_384_PKCS1` produced seven
  `OK` matching rows, then the client timed out; the outer bound interrupted
  cleanup. This argues against treating RSA-384 itself as the timeout cause.
  The full Crypto timeout remains unexplained and requires its own diagnostic;
  the deterministic and selected IV checks are independent.
- A narrower `ALG_RSA_SHA_384_PKCS1_PSS` run completed four selected API
  checks as `OK` and cleaned up successfully; the run took 88 seconds. The
  overall VPCD API client path has low throughput, and the 800-second Crypto
  bound may be insufficient for the complete area. The full-area timeout is
  not an algorithm support result.
- **Separate observation:** The full Crypto sweep's RSA SHA-512 PSS
  `EXC_CARDRUNTIME reason=0x0001` uses a 1024-bit key with a 64-byte salt. The
  PSS operation cannot fit that salt with SHA-512; separate known-answer
  analysis established this is an invalid test input. Do not conflate it with
  the IV fix.

## Dynamic overlay tombstones consumed the file index after CAP deletion — 2026-11-01

- `fs_erase_package_state()` marks matching overlay entries deleted, but the
  tombstones previously stayed in the fixed 8,192-slot hash table indefinitely.
  Repeated temporary CAP install/delete cycles therefore exhausted index slots
  even when the package-owned keys had already been removed.
- After successful package-state deletion, the Pico backend now compacts the
  overlay in place. It removes failed nonserialized reservations, tombstones
  for dynamic package IDs 56–63, and tombstones with no ROM backing; it retains
  live records and ROM-backed tombstones. It rebuilds the hash index and packs
  payload bytes while retaining the existing copy-on-write snapshot protocol.
  Parent-slot encoded records cause a safe no-op because their payload refers
  to slot numbers.
- `./tools/test-rp2350-fs-overlay` passes (`TEST:SUMMARY fail=0`). The expanded
  regression covers all eight dynamic package IDs; for each it writes 128
  applet-state records plus package-static and lifecycle tags, deletes and
  restarts, and verifies unrelated data remains. A separate case injects 64
  randomized power cuts during the compacted snapshot. The latest in-place
  implementation uses no extra 64 KiB scratch table; the ARM/RISC-V
  `max_crypto` builds pass. A full-index regression also fills all 8,192 slots,
  deletes package 56, then verifies restart leaves only an unrelated record.
- Physical validation is pending: the approved helper currently reports zero
  Pico 2 readers. The next hardware check should flash a currently enumerated
  board, capture the finite monitor, and repeat temporary CAP deletion cycles
  while checking `CHOUPI:FS_COMPACT` output. Full API-sweep follow-up is still
  required to distinguish cumulative tombstones from live-set storage needs.
- The independent PC VPCD `./tools/test-vpcd-persistence` workflow passed on
  2026-11-01 (install/commit 3/3; fresh-process restore and four GC-pressure
  rounds 38/38). This verifies applet data/heap behavior on the emulator, not
  the RP2350 overlay compactor or physical power-loss recovery. Logs:
  `artifacts/logs/vpcd-persistence-commit-20261101T090958Z.log` and
  `artifacts/logs/vpcd-persistence-restore-20261101T090958Z.log`.

## JCAlgTest NO_MEMORY traced to dynamic persistent-overlay exhaustion — 2026-11-01

- The complete unchanged API 3.0.5 Crypto area on max_crypto Pico 2
  `2BED26ED20556535` completed 466 calls: 292 `OK`, 161
  `UNSUPPORTED_PER_SPEC`, nine `NO_MEMORY`, three `WRONG_RESULT`, one
  `EXC_CARDRUNTIME`, and zero `EXC_RUNTIME`. Wrong results were three IV
  Signature cases; the runtime exception was RSA SHA-512 PSS.
- A newly installed Crypto CAP filtered to KeyAgreement and Cipher executed 65
  calls (25 KeyAgreement, 40 Cipher): 50 `OK`, 15 `UNSUPPORTED_PER_SPEC`,
  zero `NO_MEMORY`. It hit the 150-second client limit before the planned
  filtered run finished (AEADCipher was not reached); CAP cleanup did pass.
- The full Crypto serial capture recorded persistent `FS_WRITE_FAIL` events
  followed by `PERSIST_EXCEPTION id=20` and `VM_CAUGHT_EXCEPTION id=20`. At
  first failure there were 8,016 files and 94,573/98,304 bytes used. With a
  0x28000-byte snapshot slot and 4 KiB header, the v4 data ceiling at that
  count is `159,744 - 1,024 - 8*8,016 = 94,592` bytes; only 19 bytes remained.
  This explains why the VM maps the failed persistent write to `NO_RESOURCE`,
  reported by the API client as `NO_MEMORY`. Those original captures predate
  the diagnostic correction; current `FS_WRITE_FAIL` output includes the
  effective dynamic payload limit, fixed arena size, file count and free slots.
- A later exact-case filter recorded ten calls: eight `OK`, two `NO_MEMORY`.
  Serial showed `FS_WRITE_FAIL` with `files=8192`, the hard overlay record
  ceiling, at 89,801 and 90,150 bytes. The full-suite and exact-case evidence
  therefore confirms persistent overlay exhaustion, not unavailable crypto
  methods. In that filter, the DES-64 padded CBC case and `AEADCipher()` were
  the two failures; the other eight selected operations (including all original
  failing key-size and constructor cases except AEADCipher) passed. Temporary
  CAP cleanup passed, yet accumulated records remain.
- **Next:** investigate persistent object lifetime/reclamation and whether
  applet uninstall removes all package-owned records. Do not implement
  algorithms, delete user data, or reset the physical token to work around
  this storage condition.
- The parallel Security run on 8E stopped at transient RSA private 2048
  allocation with `SCARD_E_NOT_TRANSACTED`; that exact reader then vanished
  from PC/SC and serial enumeration. Preserve its temporary CAP until 8E is
  accessible again. 2BED remained available.
- Filtered CSV SHA-256:
  `2996f7e8d5b5cc3ead35620e952b7d28c97915216c1859aee392d5ac8596cd09`;
  serial SHA-256:
  `de0b789f19b046a7ef34d0f7318771c5b972a4ac4da0ef12f6bcf6144f2df77c`.
- Exact-case CSV SHA-256:
  `a7d30c2eb7045d5d386b582f4e1a545d48df2fc8a574f7e4a0c48ea69982dbe9`;
  serial SHA-256:
  `a28357fee22e19f4d7fac9d51fe8dd5b940377ffe74b482fbbd1308198cea6af`.

### Diagnostic follow-up

- `src/pico_filesystem.cpp` now reports current/max file count, free file slots,
  used/effective dynamic payload, and fixed arena size on `FS_WRITE_FAIL`.
  ARM/RISC-V builds passed; the updated max_crypto ARM image flashed and
  completed its finite physical monitor on 2BED. Image SHA-256:
  `52a6980806b8dcf4b418e5ff75b9e401f010cfe3a14f9206cf8679a50b1b2f13`.
- `./tools/test-rp2350-fs-overlay` passed (`TEST:SUMMARY fail=0`) and exercised
  the new output at `files=4894/8192 free_slots=3298
  bytes=98287/98304 arena=98304`. The physical failure branch was not hit
  after flashing. A later AEADCipher
  filter returned one constructor `OK` and seven `UNSUPPORTED_PER_SPEC`, but
  the client timed out at 60s after writing those rows; CAP cleanup succeeded.
  No current `FS_WRITE_FAIL` appeared in the bounded serial capture. Treat the
  timeout as a failed validation, not as an API pass.

## Focused API NPEs do not reproduce on the 2026-11-01 max_crypto image

- After a fresh ARM `max_crypto` build and verified physical cycle on Pico 2
  `2BED26ED20556535`, the unchanged API 3.0.5 RSA-512 `KeyPair.genKeyPair()`
  call passed (`OK`); the isolated Security CAP was removed successfully.
  The broader filter ran RSA-CRT-512 too (also `OK`) and reported DSA-512 as
  `UNSUPPORTED_PER_SPEC`.
- The filtered DES-MAC8 Crypto CAP completed without `EXC_RUNTIME`; seven of
  eight selected calls were `OK`. The IV-specific `Signature.init(...)` case
  remains `WRONG_RESULT` (`returned=0x0008`, effect unexpected). CAP cleanup
  passed.
- Bounded serial captures during these APDU tests show no
  `CHOUPI:NULL_RECEIVER`; runtime self-checks reported
  `TEST:SUMMARY pass=1 fail=0`. These are different scopes: the self-check is
  not the API result.
- **Current decision:** The 2026-10-31 NPE findings are historical and were
  not reproduced on the newly flashed image. No common cause or code fix has
  been established. Do not attribute the change to diagnostics alone. Run
  complete current-image Security and Crypto areas and compare exact source,
  ROM-mask, and image hashes before closing the broad regressions.
- **VPCD environment note:** PC/SC enumerated Virtual PCD 00 00/00 01, but the
  PC emulator still failed to connect to `127.0.0.1:35963` with
  `Connection refused`. The historical 81,920-byte empty flash image also had
  a package bit without its registry sidecar; use the 524,288-byte
  `algtest-v182-current-empty-flash.bin` for clean runs. This image alone did
  not restore the missing listener.
- Artifacts: `physical-null-receiver-security-rsa512-only-2BED-20261101.csv`
  SHA-256 `fb1bf4a6e962b6626ecfe136739bb29a7b3b7aefac7195d29c908ecc1964fd72`;
  `physical-null-receiver-security-2BED-20261101.csv` SHA-256
  `0652af1a2df34f93b7987ee6fd2736561c3ca34df0342cea266f40fbb89b959d`;
  `physical-null-receiver-crypto-2BED-20261101.csv` SHA-256
  `de8edc4d011a6df39e4d24b56651000cbad0dd96bed611072ffbe0c0648a28cd`;
  serial log SHA-256 `7b0bc7ff622e4f95ff6405fa5304b8e0128ad749cef060af50b5f4963c31ed1a`.

## Physical GPPro install and failed-install rollback verified — 2026-10-31

- On freshly flashed max_crypto firmware, the physical asymmetric probe CAP
  passed GPPro LOAD/INSTALL, SELECT, and its RSA-3072 API APDU on Pico 2
  `2BED26ED20556535`; cleanup removed the test applet and package and returned
  `PHYSICAL:CLEANUP_PASS`.
- A separate large JCSystem transaction CAP LOAD succeeded but INSTALL
  returned `6A80` on both visible boards. Its cleanup listed no package/app and
  returned `JC_SYSTEM_PHYSICAL:CLEANUP_PASS`, verifying failed INSTALL did not
  leave a registered package. On 2BED this is consistent with the known
  32,770-byte persistent-record request exceeding the 8,821-byte remaining
  commit budget. The 8E low-level cause was not captured; do not generalize
  the 2BED diagnosis to it.
- These runs validate physical GPPro install, APDU, successful delete, and
  failed-install cleanup without touching existing JCAlgTest or ApiSec
  packages. The large transaction fixture remains unsuitable for the current
  card storage state; prior 8E transaction/power-cycle run passed 9/9.
- Evidence logs: `artifacts/logs/asymm-crypto-pcsc-20261031T214638Z.log`;
  `artifacts/logs/jcsystem-transaction-physical-20261031T215221Z.log`;
  `artifacts/logs/jcsystem-transaction-physical-20261031T215407Z.log`.

## Full JCSystem transaction fixture exceeds current physical card capacity — 2026-10-31 retest

- The max-profile `JCTransactionProbe` GPPro install returned `6A80` on both
  visible physical readers, `2BED26ED20556535` and `8E464F4E2BBC0A19`. Cleanup
  confirmed package `A0000000620301` was not left installed. On 2BED, the
  established underlying cause remains the persistent store having only 8,821
  bytes free (`56,714/65,535` used) while the fixture creates a 32,770-byte
  persistent record. The 8E retest's low-level failure was not separately
  captured; its identical GPPro status alone does not prove identical capacity.
- Viable rule: don't retry this large fixture installation unchanged on a full
  card. Use a smaller fixture or an explicitly reviewed cleanup plan; do not
  delete user packages/state to make room. Prior 8E hardware evidence includes
  a 9/9 transaction/power-cycle run (2026-10-30).
- Despite the oversized-fixture limitation, 2BED startup and installed-state
  persistence passed after firmware reflash and explicit CCID slot reset. The
  `JCAlgTest` package/applet remained registered and SELECT, GETVERSION, and
  JCSystem-info APDUs returned `9000`; see `.agent/HANDOFF.md`.

## Physical long-native-operation callback validation — 2026-10-31

- Authorized host setup found two real CCID readers: Pico 2
  `8E464F4E2BBC0A19` and `2BED26ED20556535`. The latter was selected by its
  exact SmartcardIO name and reflashed with the current ARM `max_crypto`
  image (`build/arm/pico2_choupi.uf2`, SHA-256
  `de06d766469b459c299ce5831a9d3ad5a8139643cf4630e431fe019bd1724719`). ARM
  and RISC-V wrapper builds both passed before the flash.
- The physical `RSA 3072-bit generation and signature` probe passed on the
  freshly flashed image: final response `019000`, elapsed 49,184 ms, no
  transport error, and temporary package cleanup passed. The 15-second RP2350
  watchdog remained serviced during the long native operation. An 8-second
  serial capture afterward repeatedly reported `TEST:SUMMARY pass=1 fail=0`.
  This validates long-operation completion and the CCID extension path, but
  firmware currently exposes no direct count of emitted extension frames.
- Logs: `artifacts/logs/cycle-choupi-20261031T214423Z.log` (SHA-256
  `c398105410336334a0553b6c78ce211afe096f7d225dbaf5aa74f9b1ea08e954`),
  `artifacts/logs/asymm-crypto-pcsc-20261031T214638Z.log` (SHA-256
  `2f20190fdb89991d5aae79bf028ff2bcf4868c28a1e766998fb0fce27686bc81`), and
  `artifacts/logs/native-progress-max-crypto-2BED-20261031.log` (SHA-256
  `0a4aefbff99aab138690a993fa552e373496fccb902989241675619d634f88c1`).
- The cycle log reported one retained `WATCHDOG_RECOVERY ... cause=other`
  marker during the intentional CDC-to-BOOTSEL reset. The cycle completed and
  later runtime checks passed; this is not evidence that the long APDU caused
  a watchdog reset. The marker was consumed on startup. Re-investigate only if
  a watchdog-caused marker or runtime failure appears during normal operation.

## Physical API 3.0.5 NO_MEMORY is bounded by persistent overlay size — 2026-10-31

- On Pico 2 `2BED26ED20556535`, API 3.0.5 Security hit 94 `NO_MEMORY`
  responses. A bounded CDC capture identified the underlying storage failure:
  `FS_WRITE_FAIL op=new ... files=7246 bytes=77823/77824`. A temporary image
  enabling `PICO2_DEBUG_PERSISTENT_GC=1` produced the same 94 failures; these
  records were still reachable, so persistent reclamation is not the remedy.
- Viable fix implemented: snapshot format v4 stores occupancy in a 1 KiB
  bitmap and uses 8-byte index records instead of 10-byte slot-number records.
  This raises the data arena to 96 KiB within the existing 160 KiB COW slots
  while preserving the reported 2 MiB layout. The effective data limit shrinks
  with record count to keep the serialized snapshot inside its slot. V2/v3
  remain readable and migrate copy-on-write. Host power-cut tests and ARM/
  RISC-V builds pass; a physical runtime monitor passed after flashing v4.
  The first Security suite stopped mid-APDU when the reader disappeared, so
  the reduction in `NO_MEMORY` was initially unverified. Isolated physical
  reruns on the v4 image later completed both areas on 2BED: Security had zero
  `NO_MEMORY` across 901 calls, and Crypto had zero `NO_MEMORY` across 466
  calls. Both still showed many `EXC_RUNTIME` results (543 Security, 206
  Crypto), which are a separate open regression, not a passing API baseline.
  The complete results and physical trace hashes are recorded in
  `progress.md`. Do not use the unverified second flash half. The release
  reclaimer remains disabled.
- Release was restored and finite monitor passed. Debug Security CSV SHA-256:
  `0cb6789cb206ad1a5161dacd3d10c5cfff2e66eb6766eb6230c925baf015837d`.

## max_crypto v4 API tests report runtime exceptions and transient-key reader loss — 2026-10-31

- Isolated Security and Crypto runs on 2BED completed with no `NO_MEMORY`, but
  reported 543/901 and 206/466 `EXC_RUNTIME` outcomes. The first rows are RSA-512
  `KeyPair.genKeyPair()` (Security call 242) and `Signature.ALG_DES_MAC4_NOPAD`
  (Crypto call 153); subsequent classes also fail. A contemporaneous serial
  capture showed caught exception ID 28 (`NullPointerException` in the
  `Exceptions` enum). Runtime `TEST:SUMMARY` lines only validate the firmware
  self-check and do not validate these API calls.
- 8E lost its PC/SC session at transient RSA key allocation. Excluding the
  first 2048-bit RSA private transient case moved the first disconnect to
  RSA-CRT transient-reset 512-bit allocation. CDC serial remained healthy; the
  reader did not return after the authorized ACL helper. The temporary
  Security deploy CAP remains on 8E until its exact reader returns; do not
  install a duplicate there.
- A fresh PC/VPCD API Security run successfully LOADed/INSTALLed the CAP but
  did not finish within the 600-second bound and wrote no call rows. It is not
  comparative API evidence. The VPCD emulator's repeated missing-tag restore
  traces need separate interpretation before another long run.
- Viable next diagnostic: isolate the first runtime exception on 2BED with
  serial capture across the corresponding API call, then identify whether the
  exception originates in the unchanged CAP, native dispatch, or heap object
  lifecycle. Re-run both full areas after the fix.

### Focused reproductions on 2BED

- The unchanged API 3.0.5 Security CAP was uploaded, run with only
  `KeyPair.genKeyPair()` for `ALG_RSA, LENGTH_RSA_512` enabled, then cleaned up.
  It returned `EXC_RUNTIME` by itself; serial reported caught exception ID 28
  (`NullPointerException`) at CAP pc 6387, opcode `83`.
- The unchanged Crypto CAP was similarly filtered to `ALG_DES_MAC8_NOPAD`.
  Three factory/getter checks passed; five operation variants returned
  `EXC_RUNTIME`. Serial reported caught ID 28 at CAP PCs 8624, 9291, 9842, and
  10317. Upload and cleanup passed.
- Because each failure reproduces alone, later full-sweep state is not required
  to trigger it. The evidence is consistent with a null reference at a CAP to
  native API boundary, but does not yet prove whether the CAP argument, method
  token mapping, or native object state is responsible. Map each CAP PC to the
  exact invoke and add narrowly scoped dispatch diagnostics for method token
  and receiver reference before changing algorithm code. Full CSV/log hashes
  are in `progress.md`.

## USB-visible Pico CCID devices still fail PC/SC reader registration — 2026-10-31

- Rechecked Pico 2 `8E464F4E2BBC0A19` and `2BED26ED20556535`. The authorized
  helper reports both USB nodes accessible, and `lsusb -v` reads valid
  descriptors: interface class 11 (Chip/SmartCard), CCID 1.10, T=1, one slot,
  bulk endpoints `03/83`, and product name `Pico2 JCVM CCID`.
- The installed `ifd-ccid.bundle/Contents/Info.plist` has an exact aligned
  VID/PID/name entry at index 629: `0x2E8A`, `0x0009`, `Pico 2 CHOUPI CCID`.
  Thus neither a missing CCID interface nor a missing local libccid VID/PID
  mapping explains the failure.
- Despite that evidence, an elevated host-access scan
  `timeout --signal=TERM --kill-after=2s 8s stdbuf -oL -eL pcsc_scan -r`
  exited 124 after printing only the scanner header. The journal is unreadable
  to this user, and `/var/log` provided no PC/SC diagnostics. Root cause and
  any APDU-level behavior remain unverified. Do not change firmware CCID
  descriptors based on this evidence alone; obtain pcscd/IFD logs or restore a
  registered physical reader first.

Use this summary first, then the recent issue records or historical topic index.
Full original: [archived QKB](archive/2026-10-29-pre-compaction/QKB.md).
Archive text is immutable historical evidence; later fixes supersede earlier
"unresolved" statements. Current tasks/results: [HANDOFF.md](HANDOFF.md) and
opening prioritized backlog in `TODO.md`. Detailed work history: `progress.md`.
All original bytes, measurements, hashes, commands, and logs are preserved in
archives with SHA-256 manifests; logs referenced there may be local/Git-ignored.

## Persistent-object reclamation must fail closed — 2026-10-31

- The normal post-APDU collector only reclaims volatile heap slots. A new
  `PersistentReclaimer` primitive provides persistent tag deletion but is not
  called by runtime collection. It refuses active Java Card transactions,
  requires a complete read-only tag-cursor preflight, traces roots again for
  each fixed-size batch, and restarts enumeration after each batch. If an erase
  fails, earlier deletions are restricted to tags proven unreachable; retrying
  can finish the remaining candidates.
- `./tools/test-persistent-transaction` passes interrupted-batch retry,
  reachable-subtree/owner preservation, active-transaction refusal, incomplete
  root failure, and cursor-stall failure-closed cases. ARM and RISC-V max-profile
  wrappers pass. A compile-time test hook, disabled by default, was exercised
  on Pico 2 `2BED26ED20556535` using the temporary `PersistentReclaimerProbe`
  applet. Clearing a rooted persistent reference deleted exactly one orphan
  object-body tag; a separate rooted object remained readable, and probe
  install/cleanup passed. Serial evidence:
  `artifacts/logs/persistent-reclaimer-monitor-2BED-20261031-fixed.log`, SHA-256
  `b395ae8298ab06289794071a5b8011c93182fc0e47d1476b2200593b801bce86`; APDU
  run/cleanup:
  `artifacts/logs/persistent-reclaimer-physical-20261031T052908Z.log`, SHA-256
  `d161d673120e22172dcb6ae97f4363049ba0d435c38f72ac4e846d6b1bca936a`.
  This uncovered a real bug: a reachable object tag prefix incorrectly
  protected all deeper records. Reachability now covers the exact object tag
  and its direct two-byte field/element records only; deeper records require
  their own marked object. The regression and both architecture builds pass.
- Do not wire this primitive into the normal collector until the RP2350
  filesystem erase atomicity and recovery are validated on hardware with an
  actual USB power interruption. If a target power cycle interrupts a batch,
  confirm reboot preserves all roots and a subsequent collection safely
  finishes the remaining unreachable tags.
- The fake-flash RP2350 overlay suite now stages ten owner-tag erases and injects
  1,000 randomized cuts during the single COW snapshot commit. Every reboot
  restores either all ten old records or none; the live applet-root record is
  preserved in both states. When the old snapshot wins, retry removes all ten
  candidates. `./tools/test-rp2350-fs-overlay` ends with `TEST:SUMMARY fail=0`.
  This verifies overlay snapshot atomicity, not a physical Pico power loss or
  the end-to-end VM reclaimer invocation.

## VPCD first-reader scan can time out during PC/SC startup — 2026-10-31

- `tools/test-gppro-empty-install-vpcd` previously ran one 8-second
  `pcsc_scan -r` and aborted on timeout with an empty redirected log. After a
  direct line-buffered scan, both Virtual PCD readers appeared and GPPro
  LOAD/INSTALL succeeded. The physical access helper can grant Pico USB-node
  ACLs while PC/SC still has no physical reader; that is a separate condition.
- Viable script improvement: use line-buffered inventories, retain a log per
  attempt, and allow up to three bounded discovery attempts before failing.
  With the change, syntax validation and the full fresh-image GPPro test pass;
  GPPro lists the empty card, loads/installs the unchanged upstream JCAlgTest
  CAP, and lists the package/app as loaded/selectable. Retry exhaustion has
  not been fault-injected yet.
- Successful output is under
  `artifacts/results/gppro-empty-install-vpcd-20261031T021340Z/`.

## Synthetic CPLC dates lagged the requested build date — 2026-10-31

- The CPLC fields were hard-coded to YDDD `6302` (2026 day 302 / October 29),
  although the requested synthetic dates were “today” and the current date was
  October 31. The physical extended JCAlgTest metadata read exposed the stale
  values.
- Viable fix: update every synthetic date field in `src/gp_get_data.hpp` and
  keep both `tools/tests/gp_get_data/gp_get_data_test.cpp` and
  `tools/GPGetDataApduProbe.java` exact-byte fixtures synchronized. For this
  2026-10-31 build, YDDD is `6304`. `./tools/test-gp-get-data` passes 14/14;
  ARM and RISC-V wrapper builds pass; open and SCP03-authenticated physical
  GET DATA return the exact 42-byte CPLC value (45-byte TLV) and `9000` on Pico 2
  `2BED26ED20556535`.
- Physical APDU log:
  `artifacts/logs/physical-gp-get-data-cplc-2BED-20261031.log`, SHA-256
  `b4a4bb53964e98ee2c8541c00b7f4a0687ae9cedc1e8cd40e90e12b511708e42`.

## Standalone AES-CTR profile physical support comparison — 2026-10-31

- Rebuilt both wrapper targets for `symm_crypto_ctr`, flashed the full ARM
  `choupi` image to Pico 2 `2BED26ED20556535`, and passed the finite runtime
  monitor. After reboot, `pcsc_scan -r` initially listed only VPCD; the
  authorized `pico2-pcsc-reader` helper granted USB-node access and a retry
  registered the Pico reader.
- The unchanged JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` run completed with exit
  status 0: 4,525 identities, 75 yes, 4,450 no, zero other. jCardSim identities
  matched exactly. The generated `symm_crypto_ctr` manifest matched 48/48,
  with zero unexpected or missing positives. Both `ALG_AES_CTR` in the Cipher
  section and `CIPHER_AES_CTR PAD_NOPAD` reported yes.
- CSV SHA-256
  `4dd12ac4de12f0cc127b0af090d5fab32a4eb7ce8cb3d8bfd178b4ecb4ec1f98`; raw
  client log SHA-256
  `3b190ad422fe0752255f8b8397a5ab796199875845058df14be26f521480cf8d`.
  ARM image SHA-256
  `8e5e727543fe29d72fa9c6a55d835fef42cdb7d81c8a1e8a26a24e6cf5f0ec8c`.
- This supersedes the earlier max_crypto-only report note for the standalone
  profile. The eight NIST CTR physical KATs were previously run on the
  `max_crypto` superset; exact-profile KAT repetition remains optional unless
  CTR dispatch or profile linking changes.

## Physical AES-CTR vectors on max_crypto superset — 2026-10-31

- The current combined `max_crypto` ARM build includes the profile-gated CTR
  module. A custom CAP built from the existing symmetric API test applet was
  installed with GPPro on Pico 2 `2BED26ED20556535`.
- Viable validation: run the unchanged NIST SP 800-38A 128-bit CTR vectors for
  single-shot and 19-byte-split streaming encrypt/decrypt, using both
  `Cipher.getInstance(ALG_AES_CTR, false)` and the cipher/padding factory. All
  eight physical vectors passed. GPPro cleanup removed the temporary applet
  and package, and the post-cleanup list retained the JCAlgTest applet.
- This validates CTR on the max-profile superset only. The separate
  `symm_crypto_ctr` image and complete 4,525-row JCAlgTest run remain open.
- Logs and hashes: install `artifacts/logs/aes-ctr-pcsc-install-2BED-20261031.log`
  (`29a196c7c6dfff41dcf718c57ce4f36b565c26b9877a6149e17e77927ea43604`), KAT
  `artifacts/logs/aes-ctr-pcsc-kat-2BED-20261031.log`
  (`60ec42ff148b3a27a8b5ed4f6b9954169cf5739a0366dd485802098a83f25d70`),
  cleanup `artifacts/logs/aes-ctr-pcsc-cleanup-2BED-20261031.log`
  (`8287da14d42575ecdedc6c318d2396d3129f107c93808d8aa7d8fd5aca7fa785`), and
  post-cleanup list `artifacts/logs/aes-ctr-pcsc-post-cleanup-list-2BED-20261031.log`
  (`00df9b0b17d246684706058d183d7e4e5bc187b9bd973630b5c1b387befdeeb5`).

## Focused SHA-256 static probe assumed RSA-SHA1 was unsupported — 2026-10-31

- `tools/probe-jc-algtest` default path completed its SHA-256 static checks on
  the max-profile card, then exited nonzero because its trailing assertion
  hard-coded `RSA SHA-1 => NO_SUCH_ALGORITHM`. `max_crypto` intentionally
  supports RSA SHA-1, so the observed supported response was not a firmware
  defect.
- Viable fix: use `--sha256-static-only` to stop after the selected digest
  checks. This is host-side test-probe code; it does not add Java bytecode or
  crypto implementation to the card.
- On physical Pico 2 `2BED26ED20556535`, all SHA static prepare/baseline and
  measurement APDUs returned `AA9000`; the 50-call SHA-256 APDU also returned
  `AA9000`. Five no-call baselines averaged 1,680.2 ms, and two 50-call
  measurements averaged 1,979.5 ms, estimating about 5.99 ms per 256-byte
  `doFinal` after baseline subtraction. This is host elapsed time including
  transport/VM overhead, not a full official static report.
- Passing log `artifacts/logs/jcalgtest-sha256-static-only-2BED-20261031.log`
  SHA-256 `1f02365a2898bc045004d0ad626ea547dfe2b4e44875453bc3b4b8bb0204a15f`;
  original nonzero exit log SHA-256
  `a0e72d63b85cdf29382423751913e689f5097a19d3978640ef0cf3e3129c7170`.

## GET STATUS next occurrence must keep the original query — 2026-10-31

- The PC VPCD card manager already stored GET STATUS P1 and search bytes, but
  the Pico CCID handler only checked that a prior first/all query existed. The
  two paths could therefore differ when a caller changed category or AID
  criteria on `P2=03`.
- Viable fix: share `gp::get_status_query_matches()` and retain the previous
  P1 and bounded short-APDU search bytes in the CCID handler. Reject changed
  queries with `6985`; preserve the previous cursor so a matching continuation
  can still proceed. Unit coverage checks exact query, category mismatch, AID
  mismatch, and missing search data.
- Validation: GP registry unit suite passes 54/54; VPCD multipackage suite
  passes 39/39. On physical `2BED26ED20556535`, a single GPPro SCP session
  returned `9000`, `6985`, `6985`, then `9000` for initial, changed-category,
  changed-AID, and matching-continuation queries. The post-sequence 8-second
  CDC monitor passed. ARM/RISC-V builds and ARM flash/runtime cycle passed.
- Physical APDU log:
  `artifacts/logs/physical-get-status-continuation-2BED-20261031.log`,
  SHA-256 `1dc6e481c90e05eb7fb37be8d6544cce892525fc8263e74a5ee98005901e3435`.
  VPCD APDU/restart hashes are recorded in the matching progress entry.

## APDU context unit-test harness missing `fs_exists` declaration — 2026-10-31

- `./tools/test-short-apdu` initially failed to compile `heap.cpp`: the test's
  local `tools/tests/short_apdu/ffi.h` stub did not declare `fs_exists`, which
  the current heap tracer now calls. Adding the OS include directory did not
  help because the test-local FFI header intentionally shadows the OS header.
- Viable fix: declare `fs_exists(const uint8_t *, uint8_t)` in the local FFI
  stub and provide a `0`-returning implementation in `apdu_context_stubs.cpp`.
  The standalone test then passes 13 short-APDU parser cases and 13 APDU
  context cases. `./tools/test-apdu-parser-fuzz` also passes 250,000 inputs.
- The native build emits existing ignored-qualifier and packed-member
  alignment warnings from shared VM headers; this harness-only fix does not
  alter or silence them.

## JC array GC regression uses reserved global-handle bit — 2026-10-31

- After the `fs_exists` link issue was repaired, `./tools/test-jcarray` built
  but failed its invalid-child test. The fixture used instance offset `0x7ffe`,
  which now contains the marker bit for a runtime-wide global handle and is
  intentionally ignored by the applet-local heap collector.
- Viable fix: choose an out-of-range *local* offset below `0x4000` (`0x3ffe`)
  so the fixture tests invalid local handles rather than the valid global
  handle namespace. The filesystem stub also defines the current
  `JC_Instance::hasLoadedFields()` method for its nonpersistent fixture
  instances. `./tools/test-jcarray` then passes all 27 cases, including
  `gc_skips_reclamation_after_invalid_child_ref`.

## Full current-source JCSystem applet VPCD regression — 2026-10-31

- `PICO2_HOST_USB_ACCESS=1 ./tools/test-jcsystem-transaction-applet` passed
  the full API fixture: `TEST:SUMMARY pass=85 fail=0 elapsed_s=87.427`.
  It covered the 83 numbered cases plus install/restart checks and included
  transaction semantics, restore, short APDU state, exceptions, persistent
  aliases, `lookupAID()`, lifecycle callbacks, and reclamation pressure.
- This is PC VPCD integration evidence on current max-profile source, not a
  physical board test. The separate physical reader scan still found no PC/SC
  reader.
- Log: `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261031T015450Z.log`,
  SHA-256 `b9c1988f1b8934999bd7470a43c6492b6c2aa733c0c3f23a5a85975d1dd71c75`.

## Pico CDC startup available while PC/SC reader is absent — 2026-10-31

- Identified Pico 2 `2BED26ED20556535` is visible at
  `/dev/serial/by-id/usb-Pico2_Pico2_JCVM_CCID_2BED26ED20556535-if00`.
  `PICO2_HOST_USB_ACCESS=1 ./tools/monitor --serial 2BED26ED20556535
  --seconds 8` exited 0 and reported repeated `TEST:PASS choupi_runtime` /
  `TEST:SUMMARY pass=1 fail=0`, with no `FAULT`. The trace also showed an
  active CCID task loop (`rx_events=2761`, `tasks=1945`). No flash or APDU was
  performed.
- The authorized PC/SC helper grants USB access to two nodes, but bounded
  `pcsc_scan -r` still returns no readers. CDC startup evidence therefore
  establishes runtime liveness only; physical applet/APDU validation remains
  blocked on PC/SC reader registration.

## `JCSystem.lookupAID()` peer lookup and short-APDU length diagnostic — 2026-10-31

- The native implementation now scans installed package instance AIDs and
  returns a runtime-wide framework `AID` Entry Point Object for a peer.
  Global instance/array handles plus a dedicated persistent AID-reference
  record preserve object identity across two applet contexts, a simulated card
  reset, and a fresh VPCD process. The authenticated multipackage lifecycle
  regression passes 36/36. Physical validation remains unavailable: the
  approved access helper granted two nodes but bounded `pcsc_scan -r` timed
  out without a reader.
- Resolved: an APDU with 13 data bytes yielded `setIncomingAndReceive() == 13`
  but `getIncomingLength() == 1` because the imported JC 3.0.5 APDU virtual
  method token 15 mapped to embedded `isValidCLA()` (which returns true/1).
  `remap_imported_virtual_method_token()` now maps the JC305 APDU tail as
  `getIncomingLength` -> embedded token 16, `getOffsetCdata` -> 17, and
  `isValidCLA` -> 15. The generated CAP fixture checks a 13-byte command and
  verifies incoming length 13, data offset 5, and valid CLA; VPCD returns
  `0D0D05019000`. The full multipackage lifecycle passes 37/37.
  Physical PC/SC validation remains unavailable: the approved helper grants
  access to two USB nodes, but bounded `pcsc_scan -r` finds no readers.
- Latest rerun log: `artifacts/logs/host-vpcd-multipackage-apdu-20261031T020553Z.log`
  SHA-256 `dc1ed42e2bc73b05a9eb1df4697723ab60f672e1065071ff0faeb5b0eb1e3078`;
  restart log SHA-256
  `d810112ea751724d9dd3d54c317c545c1ee9d6e194f6a6b79ed4d9572901ea36`.
- Final passing logs: `artifacts/logs/host-vpcd-multipackage-apdu-20261031T011006Z.log`
  SHA-256 `be3729bfd76833d476a948eccdeab2771c2e795ac75c06cffeb1165114fd5637`;
  restart log `artifacts/logs/host-vpcd-multipackage-restart-20261031T011006Z.log`
  SHA-256 `b0ec9aa58f3f13e0d175b46d0d49a8804d50dc537cf7ed97ba8ccc0073718941`.
- Global EPO validation logs: `artifacts/logs/host-vpcd-multipackage-apdu-20261031T012842Z.log`
  SHA-256 `5de30452bdad8beefc12aa3d3aee6feb3226438a814d72a8068ac5d56e0efbee`;
  restart log `artifacts/logs/host-vpcd-multipackage-restart-20261031T012842Z.log`
  SHA-256 `8e67b6dd12d29e82bd432f149a58103b4988c8fd4821eb9414c0a2aee3e93da9`.

## CAP 2.2 external remote-interface target linking and VPCD registration — 2026-10-31

- Applet installation now walks CAP 2.2 class records and resolves external
  remote-interface references through the importing package's existing
  Import/Export resolver. The target must be an exported interface. CAP 2.1
  packages skip this CAP 2.2 signature-pool walk.
- `./tools/build arm --profile max_crypto` and
  `./tools/build riscv --profile max_crypto` passed. The parser suite remains
  4,205 checks / 100,000 fuzz inputs / 48 corpus CAPs. The official SDK
  2.2.1 VPCD client passed 18/18 after the final version guard. The official
  fixture has internal remote-interface refs; the separate generated
  `tools/test-rmi-external-link-vpcd` now verifies cross-package interface
  success, class-target rejection, and missing-export rejection end to end.
- GPPro JCAlgTest install validation did not complete. On reader
  `Virtual PCD 00 01` / port 35964, the presence probe reported present but
  GPPro returned `Unpowered card`. The test was moved to the repository's
  usual `Virtual PCD 00 00` / port 35963, with an eight-second bound around
  initial PC/SC discovery; that scan timed out with no output. After the
  approved `sudo -n /usr/local/sbin/pico2-pcsc-reader` refresh, `pcscd.service`
  was active but bounded `pcsc_scan -r` still timed out with no readers.
  This is a VPCD/PC-SC environment failure; no JCAlgTest CAP load or firmware
  flash was proven in these attempts.
- Final-source ARM image:
  `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, 2,077,184
  bytes, SHA-256
  `5e8a7cb2267312821b4622729d1f3e994ffc4e7f582c964ffdae85a4d491e53a`.
  RMI client log hash
  `d2fdfe4894bcb7bb052ef6419e6ad927c1ea505fce1325d12dc6db4974e0cf2f`;
  emulator log hash
  `90160ab231073098dbe0a0e227411b87a64862fd6398f4c5c2f3adda916d52df`.
  GPPro failure artifacts are under
  `artifacts/results/gppro-empty-install-vpcd-20261030T235658Z/` and
  `artifacts/results/gppro-empty-install-vpcd-20261031T000302Z/`.

## CAP 2.2 external remote-interface validation seam coverage — 2026-10-31

- The Class-component traversal called by INSTALL is factored into
  `jc_remote_interface_validation.hpp`. `./tools/test-rmi-cap-metadata`
  exercises that same traversal with an external reference accepted as an
  exported interface and rejected when the injected resolver reports a class
  or no export. This does not yet exercise real multi-package Import/Export
  lookup; add an installed-package fixture before claiming end-to-end coverage.
- `./tools/test-cap-parser` passes 4,205 checks, 100,000 fuzz inputs, and 48
  corpus CAPs. `./tools/build arm --profile max_crypto` and
  `./tools/build riscv --profile max_crypto` pass. No board was flashed.
- Follow-up: `tools/test-rmi-external-link-vpcd` builds two CAP packages and
  installs them through authenticated GP LOAD/INSTALL. Valid external
  interface => `9000`; exported class => `6F00`; out-of-range Export token =>
  `6F00`. The final case exposed unchecked Export-table indexing, now bounded
  before resolution. Test logs are listed in the 2026-10-31 handoff and
  progress record.

## Multipackage INSTALL regression after remote-link refactor — 2026-10-31

- `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` passed 25/25 after
  the shared CAP 2.2 Class traversal was extracted. Existing two-package,
  three-applet GP LOAD/INSTALL, SELECT, fresh-process restore, owner inventory,
  and delete/cascade-delete behavior passed. These CAPs do not exercise CAP
  2.2 external remote-interface targets.
- APDU log SHA-256:
  `3f4a508c2f4f4b3376e737d6172100962c39cf2308d1d19b8d80885980d3afe1`;
  restart log SHA-256:
  `70abcfd61bb9c04c5008edd6cc0a54fb4da9cbf58dc598c3b3278cb36c53a711`.

## Current-source persistence rerun and physical PC/SC visibility — 2026-10-31

- `./tools/test-rp2350-fs-overlay` passes with `TEST:SUMMARY fail=0`, including
  1,000 randomized filesystem transaction cuts, 1,000 install-journal clear
  cuts, and 1,000 cross-store header cuts. This is host fault-injection, not a
  physical loss-of-USB-power test.
- The restricted first attempt of `./tools/test-vpcd-persistence` failed with
  `EPERM` creating its local VPCD socket. Rerunning as
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` through elevated host
  access passed install/commit 3/3 and fresh-process restore/GC stress 38/38
  over four GC-pressure rounds. It remains PC VPCD evidence only.
- `lsusb -d 2e8a:` sees Pico2 JCVM CCID readers at USB nodes `001/015` and
  `001/017`. `sudo -n /usr/local/sbin/pico2-pcsc-reader` reports
  `ACCESS_GRANTED count=2`; `pcscd.service` is active. Despite this, bounded
  `timeout 8s pcsc_scan -r` exits 124 with no reader output, including after
  access refresh. No physical APDU, flash, or cold-power test was performed.

## CAP 2.2 external remote-interface Import indexes — 2026-10-30

- The parser now cross-checks each external remote-interface reference's
  seven-bit package token against the importing CAP's Import component after
  all component frames have been read. This preserves arbitrary CAP component
  order. Missing Import components and out-of-range tokens are rejected;
  valid imported indexes parse.
- `./tools/test-cap-parser` passes 4,205 checks, 100,000 fuzz inputs, and 48
  CAP corpus files. ARM and RISC-V `max_crypto` wrapper builds pass. The
  official SDK 2.2.1 JCRMI VPCD test passes 18/18.
- The refreshed external-test ARM image is
  `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, 2,075,648
  bytes, SHA-256
  `3f2bdf483acb086e465f51ece612e8b6487e7be4ae958d5158d8c1c896836acd`.
  No physical board was flashed. External package index validation is now
  covered, but verifying that each imported class token resolves to an
  exported interface remains open.
- Client log SHA-256
  `d2fdfe4894bcb7bb052ef6419e6ad927c1ea505fce1325d12dc6db4974e0cf2f`;
  emulator log SHA-256
  `daddde74f22aaa65a885c2527f0096d306eefc83ee2e3c7ebc4c925ef36d0231`;
  build log SHA-256
  `4b4192ccf60af000e8ff7aba39a8702963f2526a705a31b23d65ddbbf5d05129`.
  Logs use timestamp `20261030T233752Z`.

## CAP 2.2 Directory layout fixed; official JCRMI VPCD probe passes — 2026-10-30

- The JCDK 3.0.5 converter emits CAP 2.2 Directory payloads with twelve
  component-size entries as specified in Oracle JCVMS 3.0.5 §6.5. This
  validator had assumed eleven, so it interpreted `import_count` as
  `custom_count` and rejected the generated RMI helper CAP before VM applet
  installation. The check now selects twelve entries for CAP 2.2 and retains
  the eleven-entry CAP 2.1 layout found in the supplied deploy corpus. See the
  [Oracle JCVMS 3.0.5 Directory specification](https://docs.oracle.com/cd/E59935_01/JCVMS.pdf#page=82).
- Added an exact JCDK 3.0.5 Directory fixture, CAP 2.1/2.2 length checks, and
  version-aware custom-count mutation coverage. `./tools/test-cap-parser`
  passes 4,203 checks, 100,000 fuzz inputs, and 48 CAP corpus files. The exact
  generated RMI CAP also passes standalone validation (149 checks, one corpus
  CAP).
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-client-vpcd` now passes all 18
  official SDK 2.2.1 checks, including primitive/array methods, returned
  remote-interface invocation, `SensitiveResult`, and framework/custom
  exceptions. PC VPCD only; no physical reader was registered.
- ARM `max_crypto` wrapper build and RISC-V wrapper/core build pass. Refreshed
  external-test ARM image is 2,075,136 bytes, SHA-256
  `efb5fca5c96365e98a56a20d764807f9e35d1ecb841f0fe24cc8046ba7b0de36`.
  Client log SHA-256
  `d2fdfe4894bcb7bb052ef6419e6ad927c1ea505fce1325d12dc6db4974e0cf2f` and
  emulator log SHA-256
  `9082a1d893378597425d7f70e084c7b439fe289cf20dcf251e4245db646e8cdc` are in
  `artifacts/logs/` with timestamp `20261030T232628Z`.

## CAP 2.2 internal remote-interface references — 2026-10-30

- The Class-component validator now records internal interface-record starts in
  a bounded 15-bit bitmap during its structural pass, then validates remote
  interface references in a second pass. This accepts forward references,
  rejects an offset into a record and a reference to a class record, and leaves
  external references for package-context resolution.
- `./tools/test-cap-parser` passed 4,200 checks, 100,000 fuzz inputs, and 48
  corpus CAPs. ARM and RISC-V `max_crypto` wrapper builds passed. A fresh
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-gppro-empty-install-vpcd` passed; the
  unchanged JCAlgTest package is loaded and selectable. This is PC VPCD, not
  physical-card validation.
- Current external-test ARM image:
  `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, 2,075,136
  bytes, SHA-256
  `78cd6aa1e6d1d24e8d195087b527ea49e4f5fe101ca0dac14d1a19526cec8960`.
  GPPro emulator log SHA-256
  `74e05347c22b5e63b9b391c4dc767b0c58e1dae76dab711bf47e4fc684035e2c` at
  `artifacts/results/gppro-empty-install-vpcd-20261030T230701Z/emulator.log`.

## CAP 2.2 remote method tokens must resolve to public table entries — 2026-10-30

- Oracle JCVMS 3.0.5 §6.9.2.5 defines `virtual_method_token` as the remote
  method's virtual token in the class, and §6.9.2.3 describes the public
  virtual-method table token range. The validator now rejects a package-method
  token (high bit set) or a public token outside the class's public method
  table. See the [Oracle JCVMS 3.0.5 specification](https://docs.oracle.com/cd/E59935_01/JCVMS.pdf#page=106).
- Converter-emitted JCDK 3.0.5 fixture coverage tests an in-range method,
  package-token rejection, and one-past-table rejection. Signature offsets must
  also point to a descriptor start, and method hashes are checked ascending.
  `./tools/test-cap-parser` passed 4,194 checks, 100,000 deterministic fuzz
  inputs, and 48 corpus CAPs. `./tools/build arm --profile max_crypto` and
  `./tools/build riscv --profile max_crypto` passed.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-gppro-empty-install-vpcd` passed: the
  unchanged JCAlgTest CAP loaded and GPPro listed its applet selectable. This
  checks ordinary CAP install under the new parser; the separate JCRMI fixture
  helper still fails at INSTALL stage 9 / `6F00` before client connection.
- The refreshed empty-card max ARM UF2 is 2,074,112 bytes, SHA-256
  `8ef6492df92514d9a677f25d8d2bb1eff59c28160fe2609063b9b311d8ed2727`.
  Physical testing remains unavailable because `pcsc_scan -r` timed out.

## Current-source cryptographic VPCD vectors — 2026-10-30

- Reran all three API vector wrappers after refreshing the max-crypto ARM image.
  The symmetric test built profile `symm_crypto` and passed 75 checks (73
  crypto vectors: 32 cipher, 13 MAC-sign, 26 MAC-verify); the asymmetric test
  built `asymm_crypto` and passed 99 checks (97 crypto vectors); the CTR test
  built `symm_crypto_ctr` and passed 10 checks (eight CTR vectors). These are
  PC VPCD checks, not physical results. Individual wrappers choose focused
  profiles rather than one combined `max_crypto` PC build.
- Logs and SHA-256: `artifacts/logs/symm-crypto-kat-vpcd-20261026.log`
  (`b9dfdc35ed2fef5d5458fd534d29a8dfd62431f378d20968761b3fa5cbd1da0c`),
  `artifacts/logs/asymm-crypto-keypair-vpcd-20261027.log`
  (`15dbd589b9eb140e2d92c0c62c76a7e4648848d77a17b421fcf5d68d02c880f7`), and
  `artifacts/logs/aes-ctr-vpcd-20261030T225058Z.log`
  (`18054d1b1deec2da3eb7ca2932d51749842e81430c693ffdb631742660ccae36`). The
  first two scripts reuse dated log filenames; their modification times are
  2026-10-30 and hashes identify the current rerun contents.

## Official SDK JCRMI VPCD helper INSTALL failure — resolved 2026-10-30

- Earlier fresh runs failed with `stage=9 sw=6F00` before the SDK client
  connected. The failure was in the Directory component validator, not the
  VM INSTALL method: CAP 2.2 has twelve component-size entries. Treating the
  twelfth size as `import_count` and the actual import count as `custom_count`
  rejected a valid generated CAP before VM initialization.
- The validator now uses the 12-entry CAP 2.2 layout and retains the observed
  11-entry CAP 2.1 layout. Exact converter fixture coverage and the generated
  helper CAP now parse.
- `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-client-vpcd` passes 18/18 using the
  official SDK 2.2.1 client. The logs and hashes are in the later
  “CAP 2.2 Directory layout fixed” record. This supersedes the old failure
  evidence above; it is VPCD, not physical-card validation.

## Physical `/deploy` Service profile recheck and Java PC/SC library — 2026-10-30

- The 3.0.5 Service CAP produced an earlier asymmetric-profile report of 102
  `EXC_RUNTIME` results out of 124 calls on two boards. A controlled rerun on
  physical Pico 2 `8E464F4E2BBC0A19` with the normal `asymm_crypto` image
  passed all 124/124 calls, including CAP upload and cleanup. A diagnostic
  build also passed 124/124; its temporary bytecode field trace was removed.
  Do not treat the older 102-exception result as a confirmed firmware defect:
  it has not been reproduced under a correctly pinned Java SmartcardIO reader.
- The host-side cause found was a reader-index resolver omission in
  `AlgTest_JavaCard/deploy/api_card_runner.py`: `--pcsc-lib` was applied to the
  official client but not the helper that maps an exact reader name to the
  client's numeric index. With the system Java, the helper saw no readers
  until `-Dsun.security.smartcardio.library=/usr/lib/x86_64-linux-gnu/libpcsclite.so.1`
  was supplied; then physical 8E resolved to index 3 (two VPCD entries precede
  it). The resolver now receives the same property when `--pcsc-lib` is set.
- Validation without `JAVA_TOOL_OPTIONS`: `python3 -m py_compile
  AlgTest_JavaCard/deploy/api_card_runner.py` and `git diff --check` passed;
  the repository deploy runner with only `--pcsc-lib` then uploaded, tested,
  and removed the unchanged Service CAP on physical 8E: 124 calls, all `OK`.
  The concurrent serial capture showed physical APDUs and normal passing
  runtime summaries. The normal `asymm_crypto` firmware had passed ARM/RISC-V
  wrapper builds and a verified physical cycle before the test. No firmware
  source changed. Physical board 8E is running normal `asymm_crypto`; 2BED
  remains on normal `symm_crypto`.
- Reproduction artifacts: `artifacts/results/deploy/Service_normal_asymm_pcscopt_8E_20261030.csv`
  (SHA-256 `8b534206de57e52e762d1727ad67c24fa38af1277cc893272e471c03fe1fd39b`);
  client log `artifacts/logs/service-normal-asymm-pcscopt-client-8E-20261030.log`
  (SHA-256 `75dc2bfd266e77ce1c4be8588c04cce1211cba040959e59595d9a9d3d2b91a35`);
  serial log `artifacts/logs/service-normal-asymm-pcscopt-monitor-8E-20261030.log`
  (SHA-256 `ca7ee5e1e55755fbbe2d62e0433015c5bc0f7b9fde6d4f9bbb49e652ddb0617b`);
  image `build/arm/pico2_choupi.uf2` (SHA-256
  `b34630800ced9b7104bf47e937656cbe4639d26d4ddda0ba1e17166825bb80d3`).

## Physical JCAlgTest `symm_crypto` support report — 2026-10-30

- Board `2BED26ED20556535` was selected by exact PC/SC reader name and was
  already running the `symm_crypto` ARM image (`build/javacard-support/profile.txt`;
  latest recorded flash cycle `artifacts/logs/cycle-choupi-20261030T071532Z.log`).
  No firmware was flashed and no CAP was installed or deleted for this test.
- The first `ALG_SUPPORT_EXTENDED` attempt used the runner's default 900-second
  bound and ended with status 124 at 2,264 rows. This was a command timeout, not
  a reader or firmware failure. The viable workaround is to set
  `JCALGTEST_TIMEOUT_SECONDS=3600`; the fresh run completed in about 27 minutes.
- Full unchanged JCAlgTest 1.8.2 report: 4,525 normalized rows, 73 yes,
  4,452 no, zero other. It exactly matched the generated `symm_crypto` support
  declaration: 46 expected yes, 46 actual, zero unexpected, zero missing.
  jCardSim row identities all matched. Its reference distribution was 1,469
  yes, 2,935 no, and 121 other; that is a separate simulator profile and not
  the expected support declaration for this firmware.
- Comparator output:
  `artifacts/logs/compare-jcalgtest-symm-support-2BED-20261030.log`
  (SHA-256 `679cb172d3ffcefc671eb57d27debfbba723ef081d931a50989ce4dff06cf54b`).
  Full client transcript:
  `artifacts/logs/jcalgtest-symm-support-physical-2BED-20261030.log`
  (SHA-256 `8c3ec90f4aa3034ee7e029389c8b7fd90b1232553e9364235e48b5e83d4c86ec`).
  CSV SHA-256 `e84b29fb770d8795372ac5f042b1f007a4d6994a54b87fea8078f4f9fc5b921f`;
  JCAlgTest report log SHA-256
  `0fba635228ba7847db859120e3b39f6c22a1165c408f203cc44edb4a105a5f67`.
  Post-run monitor log SHA-256
  `5bab064d1d555950f5f0e2aa6701d5643c7f15adfb46b25982989dba5f6411d8`.
  The 8-second monitor passed repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`, with no `TEST:FAIL` or `FAULT`.
- The timed-out partial report remains in
  `artifacts/results/jcalgtest/symm_crypto_support_physical_2BED_20261030/`;
  use the `_long/` report for comparisons.

## Symmetric profile parity with the supplied JCOP4 P71 matrix — 2026-10-30

- Filtered the supplied JCAlgTest 1.7.9 reference to symmetric KeyBuilder
  AES/DES key types, AES/DES Cipher selectors and factory aliases, and AES/DES
  MAC algorithms. It has 41 positive identities. The generated `symm_crypto`
  profile selects exactly those 41 (zero missing, zero extra).
- Canonicalized the full physical JCAlgTest 1.8.2 report from Pico 2
  `2BED26ED20556535`; it reports the exact same 41 symmetric identities
  (zero missing, zero extra). The independent physical symmetric known-answer
  suite previously passed 71/71
  (`artifacts/logs/symm-crypto-kat-pcsc-20261027T195919Z.log`).
- The saved comparison is
  `artifacts/logs/compare-symm-jcop4-profile-and-physical-2BED-20261030.log`
  (SHA-256 `d4b05870cce57ec9ab9444c66b67102aa5553e108e3e11181cf87b4f020bb807`).
  The reference contains no `ALG_AES_CTR` row; its support remains unclaimed.
- Decision: No missing symmetric implementation was found against this
  matrix. Continue with the other open project work, including physical
  USB power-cycle recovery before any persistent-record deletion.

## Asymmetric profile parity with the supplied JCOP4 P71 matrix — 2026-10-30

- The cached `build/external-test/expected-support-asymm_crypto.csv` is stale
  (90 expected identities) and yields false missing-support output. Regenerate
  expected support from the current JSON before comparing:
  `python3 tools/javacard-support.py --config config/javacard-support.json
  --profile asymm_crypto --out-dir artifacts/results/profile-audits/asymm_crypto`.
- The current manifest has 145 entries and matches all 136 positive identities
  shared with the supplied JCAlgTest 1.7.9 matrix. Reproducible output:
  `artifacts/logs/compare-asymm-profile-jcop4-target-20261030.log` (SHA-256
  `98867eecdc3f0c978ee262ca8904e74557302ccc491963109eb88eca377093fd`). The
  generated manifest and CSV are in `artifacts/results/profile-audits/asymm_crypto/`.
- Full physical JCAlgTest 1.8.2 report from 8E has 4,525 matching rows, 196
  yes, 4,329 no, and zero other values. Against the generated 145-entry
  declaration, 143 canonical identities match; the two missing report
  identities are `ALG_RSA LENGTH_RSA_3072` and
  `ALG_RSA_CRT LENGTH_RSA_3072`. The unchanged upstream CAP sends length 4096
  for these queries. Separate physical RSA-3072 and RSA-CRT-3072 generation
  vectors pass; do not change the CAP or native support to mask its query.
- Full support comparison output:
  `artifacts/logs/compare-asymm-physical-support-8E-20261030.log` (SHA-256
  `6f5c08dfda64cc859a44f0e71e50a7f840e28a6152d118bb4753b83b85dc5606`). The
  comparator exits 1 for these two upstream-labeled rows; it has zero
  unexpected positives and zero non-yes/no results. Profile parity is
  complete; broader physical `/deploy` result corrections and RSA
  private-operation hardening remain separate.

## Persistent filesystem tag cursor — 2026-10-30

- The root Pico firmware does not link `choupi/os`; that Rust filesystem and
  its trusted cursor syscall serve the host emulator. Pico uses a separate
  persistent C++ overlay in `src/choupi_pico2.cpp`.
- Both backends now expose the same 134-byte cursor request ABI and 128-byte
  tag capacity. The Pico cursor enumerates active mutable-overlay records,
  filters by prefix, skips erased records, and rejects a nonzero offset.
  Immutable ROM defaults are intentionally excluded because they consume no
  persistent storage. Cursor state is invalid after filesystem mutation.
- `./tools/test-rp2350-fs-overlay` exercises the Pico cursor, including
  80-byte tags and tombstones; host Rust tests cover cursor paging and syscall
  validation. The ARM image was flashed to identified board
  `2BED26ED20556535`; the finite monitor passed `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. This cycle did **not** call the cursor from the
  VM, so physical cursor-call behavior remains unverified. Log:
  `artifacts/logs/cycle-choupi-20261030T010244Z.log`.
- Native read-only visitors now page applet-owned `0x03, owner` tags and
  package-static `0x02, package` tags. They validate cursor progress, returned
  tag length, and prefix, and support early stop without mutation. Focused host
  tests cover owner isolation, root/body-shaped records, a 128-byte tag, early
  stop, stalled cursor, and malformed output. They do not decode references or
  feed heap GC, and have not been called on physical hardware. ARM/RISC-V
  wrappers and PC build pass; latest startup-only flash log is
  `artifacts/logs/cycle-choupi-20261030T012137Z.log`.
- CAP reference-static slots are enumerated from `reference_count`, after
  checking `2 * reference_count <= image_size`; the visitor uses two-byte
  offsets and switches to the 16-bit static tag form beyond offset 255. This
  follows the official Java Card VM spec's Static Field Image layout: reference
  fields occupy the initial segment and each is two bytes
  ([Oracle Java Card VM Spec 3.1, Static Field Component](https://docs.oracle.com/en/java/javacard/3.1/jc-vm-spec/F12650_05.pdf)).
  Tests cover 129 references crossing the 8-bit offset boundary and invalid
  metadata. The visitor only returns candidate tags; it does not fetch values
  or establish non-null roots.
- Static root loading now has a strict path: a missing package static-reference
  tag is the default null, while an existing record that cannot be read or
  decoded propagates failure instead of becoming null. Strict decoding limits
  alias recursion to 32 hops and rejects array aliases resolving to null or a
  non-array. Normal VM getters retain their tolerant legacy behavior. The
  installed-CAP adapter obtains big-endian static metadata from that CAP and
  consults the heap's package/field cache before restoring roots.
- Registered applet instance roots are identified by applet namespace tags
  ending in `0xFF`; both 8-bit and 16-bit class-index tag forms are decoded.
  Restored objects are checked against package/class fields encoded in the
  root tag. The PC-only `PICO2_PERSISTENT_INVENTORY=1` post-APDU diagnostic now
  invokes the installed-CAP static adapter and applet-root adapter. On the
  unchanged JCSystem transaction CAP, cases 14–16 passed with both adapters
  returning `Complete`; each scan reported six non-null static roots and one
  registered applet root. VPCD log:
  `artifacts/logs/persistent-inventory-vpcd-fixed-20261030.log`.
  These checks exercise root restoration but do not yet walk object fields,
  reference-array bodies, or full alias graphs, and do not authorize persistent
  record deletion. The physical cycle at
  `artifacts/logs/cycle-choupi-20261030T021444Z.log` tested startup only; the
  PC-only diagnostic did not run on hardware.

### Static root adapter callback context corrupted its heap pointer — fixed

- The generic static-root walker intentionally supplies the same opaque context
  to both its reference reader and root visitor. The adapter had passed its
  private heap-reader context to the caller's visitor, which treated it as its
  own context. The first callback overwrote the stored heap pointer; the next
  static slot then faulted in cache lookup with SIGSEGV / unauthorized address
  `0x0`.
- The adapter now stores the caller's visitor and context alongside its heap
  pointer and uses a forwarding callback. A null visitor returns
  `InvalidArgument`. The previous fault reproduces only with the old wiring;
  after the fix, the unchanged VPCD JCSystem cases 14–16 pass (5/5 harness
  checks) and both adapters report `Complete` for all four post-APDU scans.
- The same opt-in diagnostic also calls the existing read-only
  `Heap::traceReachableFrom()` with selected-context roots plus static/applet
  roots. Cases 14–16 and nested-alias case 82 pass (6/6 harness checks). After
  setup, four scans report 11/11 arrays and 7/7 instances reachable, zero
  invalid refs, and zero edge-load failures. This walk still uses the ordinary
  tolerant child getters; the result is diagnostic and does not prove strict
  fail-closed edge validation or complete coverage of every context.
- `./tools/test-persistent-inventory`, PC/ARM/RISC-V builds, and the identified
  2BED ARM physical cycle all passed. The physical monitor emitted
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`; it did not invoke
  these adapters. No persistent records were deleted or changed.
- Logs: `artifacts/logs/persistent-inventory-static-vpcd-20261030e.log` (fault),
  `artifacts/logs/persistent-inventory-vpcd-fixed-20261030.log` (fixed run),
  `artifacts/logs/cycle-choupi-20261030T021444Z.log` (physical startup).

### Strict heap traversal of serialized persistent edges — 2026-10-30

- The opt-in read-only reachability tracer now treats serialized persistent
  object fields and persistent reference-array elements as strict edges. It
  restores them with `getPersistentField_ReferenceStrict`; malformed field
  types and tags too wide to append a two-byte field/element index cause the
  walk to fail instead of silently treating an edge as null. Missing object
  fields remain default values. Transient arrays and objects that have not yet
  acquired persistent tags continue to use their in-memory references.
- PC VPCD JCSystem cases 14–16 and nested-alias case 82 pass with strict edge
  traversal (`trace_complete=1`, arrays 11/11, instances 7/7, invalid refs=0,
  edge failures=0). ARM/RISC-V builds and two identified physical startup
  cycles passed. The physical JCSystem transaction probe on 8E passed 9/9,
  including rollback/commit, large array, catchable buffer-full, and card power
  cycle persistence checks. This is regression evidence; the PC-only inventory
  trace itself has not been run on physical hardware.
- The test package/applet AID collided with installed upstream JCAlgTest and
  prevented physical test installation. The repository-owned test fixture now
  uses package `A0000000620301` / applet `A000000062030101`; only that temporary
  fixture was installed/deleted. The upstream CAP was not changed.
- No persistent records were deleted. Remaining safety gaps: malformed edge
  and alias negative cases; roots from every installed package, applet,
  context, and native owner; physical invocation of inventory; and evidence
  that an incomplete mark cannot authorize reclamation.

### Malformed aliases fail closed in the actual heap collector — 2026-10-30

- The persistent-transaction host target now links production `heap.cpp`,
  `jc_array.cpp`, `jc_instance.cpp`, `jref_t.cpp`, and the actual
  `flashmemory.cpp` decoder. Its filesystem remains an in-memory fixture.
- Real `Heap::collectUnreachableFrom()` walks persistent object fields and
  reference-array elements through the production strict decoder. A truncated
  object alias in each edge kind, a truncated array alias, and a
  self-referential array-alias chain each produce `complete=false`, one
  edge-load failure, and zero reclaimed heap slots. The wrapper passes with
  `TEST:SUMMARY fail=0`.
- Fixture correction: the persistent reference arrays now have valid
  serialized `FIELD_TYPE_ARRAY_OBJECT` headers and length fields. Earlier test
  revisions assigned a persistent tag to an array but omitted its serialized
  header, so they could fail on parent length before reaching the malformed
  child. Re-run after adding the headers still passes all collector assertions;
  the self-reference case is evidence of fail-closed alias rejection, not a
  direct measurement of the recursion-depth guard.
- A follow-up valid-chain fixture found that strict reference restoration
  already followed array aliases recursively, but `getPersistentField_Type`
  inspected only one target. Since heap traversal queries the type first, a
  valid alias-to-alias chain failed before reaching the recursive reader. The
  type decoder now recursively resolves array aliases, accepts only array
  terminal types, and rejects cycles after the same 32-alias limit used by
  strict reference restoration. Host tests cover a two-link chain through the
  actual collector and a cycle through both collector and strict restore.
- After the fix, persistent-transaction tests pass; ARM and RISC-V wrappers
  pass; the updated ARM image was flashed/verified on 2BED; the finite runtime
  monitor passed; and physical JCAlgTest SELECT, GETVERSION, and JCSystem-info
  all returned `9000`. The existing VPCD persistence/restart suite also passes
  3 install/commit and 38 restore/GC checks. Image SHA-256
  `b4a26add8ef5bfc28b948633d2f4790caaf48aaaa7addcae8f236f57c1ab8de6`; cycle
  log SHA-256 `264a3e499c8c3f06f96176e20e5fc8899418fc4d8a28a03a67b0bf8adc1ac39d`;
  physical APDU log SHA-256
  `69af8d88d141cf607dbb43153801f3ecb9c62812cb274884ac2421d99baa6601`.
- This is host-only fail-closed behavior evidence. It does not emulate power
  loss/restart, prove persistent record cleanup or recovery, or authorize
  record deletion. Keep persistent reclamation disabled until restart recovery
  and stable-handle semantics are tested.
- A separate host case then drops the volatile heap, preserves the in-memory
  filesystem fixture, creates a new Heap, restores a persistent reference
  array whose element aliases a persistent byte array, and collects with that
  root. It observes two reachable arrays, one unrelated heap slot reclaimed,
  and unchanged serialized root/target records. This verifies heap
  reconstruction over retained fixture records only; it is not a process,
  emulator, filesystem, or physical power-cycle recovery test.
- The existing `./tools/test-vpcd-persistence` suite provides a full emulator
  process restart check using the unchanged JCSystem transaction CAP. It passed
  install/commit (3/3) and post-restart restore/GC (38/38), including restoration
  of a persistent array after clearing its static alias, nested alias identity,
  and four repeated GC-pressure rounds. The first default-sandbox attempt was
  denied opening the local VPCD socket (`Operation not permitted`); rerunning
  the same wrapper with `PICO2_HOST_USB_ACCESS=1` and elevated host execution
  passed. Logs and hashes:
  `vpcd-persistence-commit-20261030T044356Z.log`
  (`df4578db168deb7b1751623e486f3e9d696623eaadd3af56890d6ca916ee9192`) and
  `vpcd-persistence-restore-20261030T044356Z.log`
  (`68a9b4f9169ec699143095dec88f64c840043a16485f125c9849aed797fe8c75`).
  This establishes PC emulator process restart only; physical power-cycle and
  interrupted-write recovery remain open.

### Read-only physical persistence probe — 2026-10-30

- The read-only `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup
  'Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 00 00'` check passed on
  identified board 2BED. GPPro listed the JCAlgTest 1.8.2 applet/package;
  SELECT, GETVERSION (`1.8.2_jc305`), and JCSystem-info each returned `9000`.
  Log SHA-256:
  `bfcc4cc3fdf299ab3654d8be46e1b00926f5b2b795aa9b4e58551a160fa5d2ab`.
- This run did not initiate a reset or power cycle, so it only confirms the
  current card/app state and is not physical power-loss recovery evidence.
- A captured Java SmartcardIO `Card.disconnect(true)` reset test passed on the
  identified board: SELECT returned `9000` before reset; after PC/SC reset and
  reconnect, SELECT returned `9000`, GETVERSION returned
  `1.8.2_jc305`/`9000`, and JCSystem-info returned `9000`. Log:
  `artifacts/logs/pcsc-card-reset-2BED-20261030T0455Z.log`. This invokes CCID
  card power-off/on, not USB power removal or RP2350 cold reboot.
- Re-running `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` passed
  25/25. Both initial and post-process-restart inventory logs report
  `static_roots=9 applet_roots=3 trace_complete=1`, 14/14 arrays, 4/4
  instances, and zero invalid references or edge/root failures. Distinct owner
  IDs remained stable; deleting one instance preserved the others. Logs:
  `host-vpcd-multipackage-apdu-20261030T045329Z.log`
  (`f5d8ce2f8575477a63b16bacd50344916d40c37abd64833369f741143ba967cd`) and
  `host-vpcd-multipackage-restart-20261030T045329Z.log`
  (`dd132b4f47944c3f720b2d5997e0384e78aa5322d8d9b2e63d58ce820ec94c00`).

### Simulated RP2350 overlay interruption recovery — 2026-10-30

- `./tools/test-rp2350-fs-overlay` passed 75 checks. It includes 1,000
  randomized multi-record snapshot power cuts, 1,000 install-journal clear
  cuts, 1,000 cross-store registry/filesystem header cuts, and simulated reset
  restoring the last committed transaction state. The expected diagnostic
  `FAULT FS snapshot unavailable; preserve flash for recovery` is emitted by
  an explicit negative recovery fixture; the overall result remains
  `TEST:SUMMARY fail=0`.
- This is host-model storage-layer evidence. It does not exercise Pico 2 flash
  under actual power loss while heap GC is operating, nor prove interrupted
  persistent-object reclamation safe. Keep actual persistent-record deletion
  disabled pending physical power-cycle recovery.

### Device collector now scans selected-package and applet roots; `0xFF` body-tag false positive fixed — 2026-10-30

- The firmware collector now uses a boolean root enumerator. It restores roots
  from the active package CAP's static reference slots and scans the selected
  applet owner's registered root tags before marking. Any adapter error or
  exception increments `root_enumeration_failures`, makes the mark incomplete,
  and prevents the collector from freeing heap slots. This is limited to the
  selected applet/package; all installed package roots and other live contexts
  remain uncovered. No persistent filesystem record deletion is implemented.
- Physical 2BED `2BED26ED20556535` was flashed with ARM UF2 SHA-256
  `58001f50ed7501b772afceacc8a071c0d62f39901a4b894ff7a4efbdd4fbf5c5`;
  cycle `artifacts/logs/cycle-choupi-20261030T030140Z.log` passed the finite
  runtime monitor. The unchanged installed JCAlgTest 1.8.2 SELECT, GETVERSION,
  and JCSystem-info APDUs all returned `9000`. Serial shows each of their three
  `interpreter_end` / `gc_begin` / `gc_end` sequences, with no
  `CHOUPI:HEAP_GC_SKIP` or root failure. Logs:
  `artifacts/logs/persistent-inventory-device-apdu-2BED-20261030-retry.log`
  (SHA-256 `a835343133df15784e387a82072e5ff27545a89212a2b9f339671a8f56b3181e`)
  and `artifacts/logs/persistent-inventory-device-gc-monitor-2BED-20261030-retry.log`
  (SHA-256 `de7a2d8b883c08e947893a29ab104dc9f3561aaa7f91ce30b74d05a0c5ffae47`).
- Root scanning had classified every owner tag ending in `0xFF` as a registered
  applet root. A reference-array body tag can also end in `0xFF`; the 7-byte
  array element tag then caused `InvalidTag` and skipped collection. Root
  recognition now accepts only the exact 5-byte and 6-byte forms. The inventory
  unit test includes a 7-byte array-body tag ending in `0xFF` and passes.
- A repeat physical transaction-probe attempt could not install its test CAP
  on 2BED: the card reported `FS_WRITE_FAIL ... bytes=56714/65535` when setup
  attempted a 32,770-byte persistent record, and GPPro returned `6A80`. No
  existing package was removed. The 8E cycle verified its flash but the board
  did not re-enumerate; use an available board as failover and leave full
  transaction-probe retest open.

### Standalone RMI unit build needs the CHOUPI OS include path — 2026-10-30

- `heap.cpp` now includes `ffi.h` for strict persistent-edge reads. The
  `tools/test-rmi-registry` CMake target compiles `heap.cpp` outside the main
  wrapper and must add `choupi/os/src` to its include paths. Add the include
  directory without linking or probing USB; the wrapper then builds and reports
  `RMI_REGISTRY:PASS` again.

### Persistent collector must scan every installed package's static roots — 2026-10-30

- Static reference fields are package-owned. Scanning only the selected applet
  package can miss objects retained by an imported package's initialized
  statics. Enumerate IDs through `JCVM_MAX_PACKAGES` and gate each walk with
  `FlashMemory_Handler::isPackageExist`; this also filters stale dynamic slot
  bits. Fail closed if package presence or root enumeration throws/fails.
- A valid CAP can omit the Static Field component. Treat that as an empty root
  set, not malformed metadata; otherwise scanning every installed package
  would stop at ordinary CAPs with no static fields.
- The PC diagnostic and ARM collector now share this enumeration. On physical
  2BED, JCAlgTest SELECT, GETVERSION, and JCSystem-info all returned `9000`,
  and captured serial showed three `gc_begin`/`gc_end` pairs without
  `HEAP_GC_SKIP`. Log:
  `artifacts/logs/persistent-inventory-all-package-device-monitor-2BED-20261030.log`.
- This covers package static roots only. Other live applet/context/native
  roots remain unaccounted; do not enable persistent record deletion.

## Canonical operating fixes and unresolved boundaries

### Stable owner IDs distinguish same-class applet instances — implemented 2026-10-30

- Registered applet root tags encode owner/package/class/root suffix, but not
  instance AID. Assign each new installed instance a unique owner ID and
  persist the AID-to-owner mapping in unused metadata-sector tail bytes. CAP
  header version 5 validates this owner map and includes it in the existing
  mirrored metadata publication. Install journal `INS2` carries the owner;
  `INS1` recovery and CAP metadata versions 3/4 preserve the legacy owner 0.
- Install, restore, and DELETE cleanup now use the recorded owner. Multiple
  instances of one applet class are allowed when their instance AIDs differ;
  the package may still have only one package AID. Owner IDs are allocated
  from 1..255 globally across installed packages. Do not reuse an owner for
  live instances.
- PC VPCD registry sidecars now write VCR2 and preserve each owner ID across
  process restart. VCR1 is still accepted and restores its historic owner 0,
  allowing old snapshots to remain usable. The Rust filesystem allows the
  trusted RuntimeEnvironment/Installer contexts to access explicit logical
  applet-owner tags; ordinary applet contexts remain scoped to their own ID.
- Validation: overlay power-cut/metadata tests pass, including distinct owner
  mapping for two same-class instances, registry reinitialization, and removal
  of one mapping without changing the other. PC VPCD multi-package restart and
  delete suite passes 23/23; JCSystem transaction applet suite passes 85/85.
  ARM/RISC-V builds pass, and identified 2BED firmware flash/finite startup
  monitor pass. Physical install of multiple instances and their root restore
  has not been exercised. The collector still only enumerates active-applet
  roots, so complete all-live-owner coverage and reclamation remain blocked.

- **Host USB/PCSC access — viable workflow:** follow root `AGENTS.md` from the
  first attempt: elevated host execution and `PICO2_HOST_USB_ACCESS=1` on each
  repository USB wrapper. The permitted ACL helper can restore access after
  re-enumeration, but does not guarantee recovery from a stuck daemon or absent
  USB passthrough. If host access is unavailable, continue offline. Never infer
  a firmware defect from absent USB/PCSC enumeration alone.
- **Board selection — viable workflow:** re-enumerate exact reader strings;
  suffixes such as `00 00`/`01 00` change. Select by confirmed chip ID, not
  reader index. BOOTSEL is not a PC/SC reader. Flash supports targeted chip-ID
  selection from runtime or BOOTSEL. Use an identified available board as
  failover and retain per-board image/results; don't transfer pass attribution.
- **Clean card — viable workflow:** flash preserves persistent CAP storage.
  Verify GPPro listing and successful temporary-CAP cleanup. Avoid assumptions
  based on older empty-card or missing-reader reports.
- **GET STATUS exact USB packet stall — FIXED:** 54-byte APDU + 10-byte CCID
  header filled one 64-byte packet; the host awaited termination. Submit an IN
  zero-length packet for nonempty exact-multiple bulk responses, rearm OUT only
  after completion, and clear pending state on reset/error. Physical GPPro list
  passed on D92; `artifacts/logs/gppro-get-status-zlp-D92-20261025.log`.
  Earlier module-query investigations are historical, not an open blocker.
- **Long native RSA — FIXED transport/progress path:** native RNG progress
  refreshes interpreter inactivity, watchdog, and USB service; periodic CCID
  time extensions preserve the active receive buffer until final response.
  Physical runner bound is 600 s, asymmetric VPCD APDU bound 180 s. Generation
  time varies; compare actual workload before diagnosing an algorithm defect.
- **Stale emulator — viable fix:** rebuild PC explicitly after native source
  changes; ARM/RISC-V builds do not update its binary.
- **VPCD storage/startup — viable workflow:** fresh zero-length image and matching
  fresh sidecar for isolated sweeps, dummy package metadata where required,
  explicit presence probe, `pipefail` on probe pipelines, one runner per reader.
  Wait for cleanup/session completion. Resume tests instead retain matching
  flash, registry, and assigned package ID and require clean process exit.
  Restricted local-socket errors require the authorized host execution path;
  they are not crypto/USB test results. Do not change machine configuration.
- **VPCD framing — viable fix:** no-input response APDU is five-byte case 2;
  option data plus response uses case 4, not `Lc=0` plus a sixth byte.
  Deploy runner changes cwd; use absolute output paths and area-specific AIDs.
- **Evidence classification:** unsupported setup can invalidate downstream test
  expectations. Require complete runner output/normal exit for suite passes;
  partial CSVs, filtered rows, emulator passes, physical runtime boot, and actual
  physical API acceptance establish different things.
- **Native API identity — established fixes:** exported NativeKey class offset
  must be resolved through export metadata, not a ROM token. JC305 interface
  and virtual tokens differ from embedded ROM; map concrete/redeclared Key
  prefixes, key getters, EC methods, and descriptor-specific native overrides.
  Refine marker checks with key type and return false for null `instanceof`.
- **GC/persistence — established constraints:** retain static/APDU/native mirror,
  typed top-level reference results, exception, transient lifecycle, exported RMI,
  and instance/reference-array graph roots. Preserve Java Card handles and
  persistent identity; test-only host stubs do not prove CAP lifecycle or power
  recovery. Archive records retain individual regression and recovery details.
- **Persistent write exhaustion — fixed error mapping; reclamation remains open:**
  filesystem status 1 is `OutOfFlash`/capacity exhaustion, so Java Card writes
  now throw `SystemException.NO_RESOURCE` (reason `0x0005`) rather than
  `IOException`/`6F00`; other filesystem errors remain `IOException`. Physical
  2BED API 3.0.5 Security retest completed 901 calls: 396 `OK`, 504
  `UNSUPPORTED_PER_SPEC`, one `NO_MEMORY`, no `SW_ERROR` or wrong results, and
  successful CAP cleanup. Full-run RSA CRT 2048 `setP` now reports the
  resource exception. `OverlayFile` and `FilesystemRecord` offsets are
  `uint16_t`; cap the arena at 65,535 bytes. A 77,824-byte trial overflowed
  these offsets, causing D92 `tag-reconstruct` erase failure and DELETE `6581`.
  D92 was subsequently sent the corrected image and flash verification passed,
  but it did not return as a PC/SC reader; its runtime/recovery state is
  unconfirmed. Never increase the arena without a storage format/offset-width
  migration. Do not reclaim persistent objects until roots and stable handles
  are safely accounted for. Evidence: corrected ARM UF2 SHA-256
  `e99fe641f95d45a8775edf8996bade276103134dbfd6ba05855f0f10847856ea`, CSV
  `artifacts/results/deploy/asymm-storage-20261029/2BED-security-bounded.csv`
  SHA-256 `cdc05f4904d39fddfda86b5233b8a3a61ea112b2b31f013a3455a9e55c918ea1`.
- **RSA-512 JCAlgTest support APDU stalled on persistent byte writes — batching fix:**
  VPCD trace showed RSA-512 primes ready in 539 ms, then `putRsaComponent()`
  performed repeated per-byte persistent writes. Added one bulk persistent
  array write per key component, keeping the byte-wise path for transient
  arrays. On a fresh PC VPCD image, unchanged APDU
  `B07519000301020000` improved from a 240 s timeout to 16.419 s and returned
  `9000`; native probe APDU `801A000000` took 0.288 s. ARM/RISC-V builds passed
  and physical 2BED flash/startup passed. The exact physical JCAlgTest APDU was
  then run three times on `2BED26ED20556535`: 1.532, 1.707, and 1.650 s,
  each returned status 00 and SW 9000. The cycle logged one
  `WATCHDOG_RECOVERY marker=494E5354 cause=other`; the marker interpretation
  is resolved below. A complete official support report passed on 8E; 2BED's
  retry reached 4,484/4,525 comparable rows before the explicit 900-second
  runner bound, with all 196 yes results present. Logs are indexed in progress
  under `rsa512-persistent-component-batch` and
  `rsa512-physical-apdu-validation`.
- **Reinstall test CAP after changing firmware capability profiles:** On 8E,
  the previously installed JCAlgTest CAP still selected and returned its
  version after flashing `asymm_crypto`, but JCSystem info and algorithm
  support APDUs returned `FF01`. Deleting only JCAlgTest's applet/package and
  reinstalling the unchanged upstream CAP restored JCSystem info and RSA
  support to `9000`; the complete physical support report then passed. Use
  this recovery for stale installed test-app state after a ROM/profile change.
  Preserve unrelated card packages.
- **`WATCHDOG_RECOVERY cause=other` is retained reset context, not proof of a
  watchdog fault:** Runtime startup derives this label from
  `watchdog_enable_caused_reboot()`. The marker is stored in RP2350 watchdog
  scratch registers, survives non-watchdog reset/reflash, and is consumed once.
  The 8E cycle emitted `cause=other` with stale-looking trace fields, followed
  by repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`; no
  watchdog reset or runtime failure was observed. Do not treat this line alone
  as a failure. Revisit if `cause=watchdog`, repeated resets, or a failing
  runtime check occurs.
- **Native-only boundary:** old Java crypto-provider plans are superseded.
  Implement behavior in native C++ API dispatch; preserve upstream CAPs.
- **RSA support-label mismatch:** JCAlgTest 1.8.2's 3072 KeyPair rows send 4096.
  Manifest remains 143/145 despite passing true physical 3072 vectors. Do not
  alter firmware/CAP semantics to hide the two upstream mislabeled rows.
- **Full max_crypto report confirmation (2026-10-31):**
  `JCALGTEST_TIMEOUT_SECONDS=5400` allowed the unchanged physical client to
  complete on Pico 2 `2BED26ED20556535` (4,525 identities, 198 yes, 4,327 no,
  zero other). Run `tools/compare-jcalgtest-support.py` against jCardSim and the
  generated manifest. Expect its only declared-set failure to be RSA and
  RSA-CRT 3072 KeyPair: the upstream rows send 4096. With those two rows
  omitted only in a temporary expected file, comparison passes 145/145 with
  no unexpected support. Raw CSV/log and hashes are in `.agent/HANDOFF.md` and
  TODO. This is a viable 90-minute bound for the full extended client, not a
  reason to modify the upstream CAP.
- **Current status overrides:** physical `Applet.deselect()` discrepancy is
  resolved (`-19` both boards), and the repeated Core `6F00` cluster is fixed:
  successful GP DELETE now discards the selected applet context/heap, preventing
  a reinstall from reusing stale object fields after persistent array records
  are removed. Updated 2BED passed two consecutive complete Core runs. CPLC
  open/SCP03 hardware validation is complete. Latest board observations and
  evidence are in HANDOFF; old access failures are not current enumeration.

## Recent detailed issue records

The following retained records cover active investigations and recent fixes.
Their dates and validation scopes remain significant. Later follow-ups in
HANDOFF/TODO take precedence over an earlier statement that hardware was absent.

## Physical SCP03 GET STATUS with a requested short Le — 2026-10-31

- **Use the dedicated probe:** `PICO2_HOST_USB_ACCESS=1
  ./tools/test-get-status-le-physical '<exact reader name>'` requires host
  escalation and exercises PC/SC directly. It authenticates SCP03, sends
  GET STATUS with Le=1, expects empty `6310`, then sends matching P2=03 with
  Le=256 and expects the applet AID and `9000`.
- **Do not use GPPro `--secure-apdu` to validate small Le:** its SCP wrapper
  normalizes the outgoing Le to `00` (256), even if the supplied APDU requests
  Le=1. A debug trace established this. For the 2026-10-31 physical test on
  `2BED26ED20556535`, the direct probe passed 2/2; log
  `artifacts/logs/physical-get-status-le-20261031T061319Z.log` has SHA-256
  `bac3e5e627312edf9be024ffc12ee6761cd6abfb693da384debb099af42fb40f`.
- **Selection form:** CHOUPI's default ISD selection is `00 A4 04 00 00`;
  selecting its AID explicitly returned `6A82` in this probe. Preserve the
  supported empty-AID selection in this utility.
- **Physical multi-entry check:** Run
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-get-status-multipage-physical
  '<exact reader name>'`. It refuses pre-existing Multi AIDs, loads the
  unchanged deploy `AlgTestApiMulti_305.cap`, creates two instances, checks
  Le=40 pagination over JCAlgTest plus both instances, and removes only those
  temporary records. On `2BED26ED20556535` the first page held 32 bytes and
  returned `6310`; the continuation returned both AIDs and `9000`; cleanup
  passed. Log SHA-256:
  `4b3e27af31e2387c9b0f320329e1e6fe7d49180ed902a1550d34508bdac5a512`.
## OS trusted tag-cursor bridge and persistent-inventory groundwork — 2026-10-30

- **Persistent-storage GC groundwork:** The OS `FileSystem` owns an in-memory
  active-tag `HashSet`. It now has both a bounded-memory visitor and a trusted
  cursor syscall that returns one active tag per call, resumes by bucket and
  offset, filters by prefix, validates user pointers, and skips tags unreadable
  from the caller's context. Cursor stability requires the active index not to
  be mutated between pages. Tests cover paging, end-of-scan, and invalid prefix
  length. This bridge does not authorize deletion. Next, consume it in a native
  owner-scoped VM inventory that includes static roots, applet roots, object
  fields, array bodies, and aliases before any reclamation.
- **C++ cursor wrapper integration:** `FileSystem` construction calls `fs_init`,
  so a root-inventory caller must not instantiate it just to page tags. The
  `nextTag` wrapper is static and can be called without storage reinitialization.
  PC host build compiles the wrapper; it is not yet consumed by a VM root scan.
- **Validation:** Host functional suite passed 67, 0 failed, 2 ignored (generated
  `bindgen_test_layout*` tests skipped because they abort with `null reference
  produced`). ARM and RISC-V repository builds passed. Identified physical
  board 2BED `2BED26ED20556535` was flashed through the elevated host-access
  wrapper; finite CHOUPI runtime passed with `TEST:SUMMARY pass=1 fail=0`.
  Runtime log: `artifacts/logs/cycle-choupi-20261030T004223Z.log`.
- **Host OS test invocation:** `javacard-os` unit tests need host `PTRACE_ATTACH`;
  run the repository toolchain wrapper with elevated host execution. The
  generated `bindgen_test_layout*` tests currently abort with `null reference
  produced`; skip those when running functional OS tests. The remaining suite
  passed 66 tests with two ignored after updating one stale expected
  `assert_eq!` panic string to the current Rust format. This panic-text update
  only repairs test compatibility; it does not alter runtime behavior.

## RSA temporary byte vectors retained secret material after use — 2026-10-29

- **Symptom:** RSA native code copied private key components, decrypted
  plaintext, padding state, and encoded private-operation blocks into ordinary
  vectors, whose storage was released without an explicit wipe.
- **Cause:** The vectors had no cleanup policy, so normal return and Java Card
  exception unwinding both left cleanup to the general allocator.
- **Viable fix:** Attach a non-copyable scoped zeroization guard immediately
  after creating sensitive buffers. Use it for byte vectors, fixed byte
  arrays, and arrays of byte vectors; keep returned signature/ciphertext or
  decrypted result buffers alive until their caller has consumed them.
- **Validation:** PC, ARM, and RISC-V `asymm_crypto` builds pass, and the
  existing VPCD asymmetric suite passes 99/99. Physical validation was not
  performed because no attached Pico had been established in this step.
- **Limit:** This reduces residual heap copies but does not solve private RSA
  input blinding or CRT fault verification; those still need a public exponent
  association/design.
- **Follow-up:** The RSA-PSS helper's generated salt, hash input, MGF1
  scratch/masks, and decoded data block are also guarded and wiped. PC, ARM,
  RISC-V, and VPCD 99/99 validations pass (implementation commit `4ecd522`).
- **Follow-up:** RSA Signature entry points now wipe copied message/hash/signature
  inputs and outputs, RSA verification scratch, and ISO9796 recovered-message
  intermediates (commit `df2d2d9`). All target builds and VPCD 99/99 pass.
- **Follow-up:** The remaining RSA CRT factor copies and OAEP MGF1 digest
  scratch are guarded (commit `bedf23a`). Audit of the pinned SDK confirms
  `mbedtls_mpi_free()` wipes allocated limbs through
  `mbedtls_mpi_zeroize_and_free()`.


## Long synchronous RSA operations time out on hardware (2026-10-27)

- **Symptom:** Physical RSA-3072 key generation returned `6400` after about
  150 seconds despite a healthy device. A subsequent full test harness also
  terminated during RSA-CRT-3072 at its 180-second host timeout.
- **Cause:** The firmware applied a fixed 15-second interpreter wall-clock
  limit around the entire applet call. Long native RSA generation is
  synchronous; Mbed TLS repeatedly calls its RNG callback while searching
  candidates. CCID clients also need time-extension responses during that call.
- **Viable fix:** Refresh the interpreter's applet progress deadline from the
  native RNG callback, while feeding the watchdog and servicing USB. Send
  periodic CCID time extensions during native crypto and keep Bulk-OUT
  unarmed until the original APDU completes, avoiding reuse of the APDU receive
  buffer during execution. Raise the finite physical harness timeout to 600
  seconds to accommodate RP2350 RSA-CRT-3072 variance.
- **Validation:** On D92, isolated RSA-3072 passed in 137.724 s and isolated
  RSA-CRT-3072 passed in 171.517 s. The complete physical suite passed 79/79;
  its RSA-3072 and RSA-CRT-3072 generation cases took 138.521 s and 110.769 s.
  ARM/RISC-V wrapper builds, finite serial monitor, and PC/VPCD 99/99 vector
  suite passed. Logs are recorded in `.agent/HANDOFF.md`.
# Physical API 3.0.5 KeyPair and marker-interface failures (2026-10-28)

- **Symptom:** On physical combined `asymm_crypto` firmware, the unchanged
  Security 3.0.5 CAP got `6F00` from every `KeyPair(byte, short)` constructor
  probe (RSA, RSA-CRT, and EC_FP across tested sizes). Seven selected
  `genKeyPair` probes also got `6F00`. The same run's marker-interface checks
  for `SecretKey`, `PrivateKey`, and `PublicKey` returned native key type IDs
  rather than booleans. One null EC parameter-copy probe had an uncaught VM
  exception. Crypto had additional `6F00` and exception-reason mismatches.
- **Cause:** Not yet established. Direct physical RSA-3072 and RSA-CRT-3072
  vectors passed, so constructor dispatch/object setup needs isolation before
  attributing this to cryptographic arithmetic. Marker results suggest a
  native return-type or `invokeinterface` contract issue, but this is only a
  hypothesis pending trace/reproduction.
- **Viable fix:** Not established. Reproduce on current PC/VPCD with the
  unchanged deploy CAP and isolate one constructor first. Trace API native
  dispatch and interpreter return handling; preserve all upstream CAPs and
  implement behavior only in native VM code.
- **Validation:** Physical D92 API 3.0.5 Core/Security/Crypto sweep completed
  in 12 minutes, CSV
  `artifacts/results/deploy/asymm_crypto_D92_20261028/core_security_crypto_retry.csv`.
  Per-area counts and cleanup are in `.agent/HANDOFF.md`. No fix has been
  applied yet; do not claim these paths pass.


## JC305 NativeKey marker-interface checks must use the key type (2026-10-28)

- **Symptom:** Focused API 3.0.5 Security tests for `SecretKey`, `PrivateKey`,
  `PublicKey`, and concrete key marker interfaces returned `WRONG_RESULT` on
  the native VM despite the corresponding object types being correct.
- **Cause:** The ROM models key instances with one `NativeKey` class. That
  class declares a union of interfaces used by several key families, while
  Java Card assignability depends on the stored key type. Applying the ROM
  class metadata directly therefore reported AES/DES/RSA/EC interfaces too
  broadly. JC305 interface tokens also differ from the embedded API's token
  numbers, so matching must use the source interface token only for CAPs
  importing the JC305 security package AID.
- **Viable fix:** `Bytecodes::docheck()` now refines known JC305 security
  interface checks against the `NativeKey` type field. The token generator
  emits stable names for the JC305 source interface tokens. `instanceof(null)`
  now returns false and pushes its result as a byte boolean. Keep this logic
  in native VM code and do not modify API sources or CAPs.
- **Validation:** Final-source focused VPCD Security run completed 901 calls:
  11 supported marker checks `OK`, eight unsupported cases
  `UNSUPPORTED_PER_SPEC`, 882 intentionally filtered; upload and cleanup
  passed. Artifact:
  `artifacts/results/deploy/marker-interface-vpcd-20261028-type-aware10/`.
  PC, ARM, and RISC-V asymm_crypto builds passed. Physical D92 flash and finite
  monitor passed on the immediately prior source (before the null-byte stack
  correction); final source was not flashed because no Pico enumerated after
  the subsequent cycle's BOOTSEL request timed out.
- **VPCD setup note:** The host emulator invocation must include
  `--applet-aid`, `--package-id 63`, the contiguous dummy `--cap-code`, and
  `--rom-mask`. Start the empty card without `--install`, wait briefly, then
  run `VpcdPresenceProbe` against the matching reader (`Virtual PCD 00 01` is
  port `35964`) with `--require-present` before starting the deploy client.
  Omitting cap code or the presence probe led to `TEST_NO_APPLET`/probe timeout;
  those were harness setup failures, not VM results.


## VPCD API 3.0.5 reruns can encounter stale or duplicate Security CAP state (2026-10-28)

- **Symptom:** After a focused Security API run uploaded a CAP but the client
  timed out before cleanup, a later deploy run reported
  `UPLOAD_OTHER` with `Package with AID 4A43416C6754657374417069536563 is
  already present on card`. A `--test-only` retry then reported
  `TEST_NO_APPLET`. These occurred while the VPCD presence probe returned
  `present=true`.
- **Cause:** Not established. Possible stale package state, applet registry
  state, or selecting a different Virtual PCD reader than the active emulator;
  do not assume the VM rejected the tested method. GPPro inventory through a
  fresh emulator session listed only ISD, but a subsequent upload still
  reported the Security package present, so inventory and upload appear to
  address different state or reader contexts.
- **Viable fix:** None yet. Before a rerun, confirm reader-to-endpoint mapping,
  query package/app registry on the same reader, and uninstall/restore a clean
  image before retrying. Keep each emulator image and log in a separate
  artifact directory. The known-good marker validation setup uses
  `Virtual PCD 00 01` at `127.0.0.1:35964`, dummy package metadata/cap-code,
  and a successful presence probe.
- **Validation:** Focused `getK`/`copyDomainParametersFrom` attempt uploaded
  the Security CAP but timed out before producing call rows. A repeat hit the
  duplicate-package warning; test-only had no applet. `lsusb` showed no
  physical Pico at the time. Logs are under
  `artifacts/results/deploy/ec-key-guards-vpcd-20261028*/`. `pcsc_scan -r`
  showed two Virtual PCD readers; the allowed physical helper found zero Pico
  devices. A GPPro inventory against a fresh `Virtual PCD 00 01` session listed
  only ISD. A direct GPPro Security CAP install later printed `CAP loaded` then
  exited 1 before reporting INSTALL status. Do not infer successful install
  from the LOAD message alone.


## JC305 redeclared Key method prefixes need per-interface token remapping (2026-10-28)

- **Symptom:** API 3.0.5 Security `KeyPair(byte, short)` constructor calls
  returned `6F00`; tracing showed a later `IF_SCMPNE` stack underflow after
  `INVOKEINTERFACE`. The constructor had invoked `PublicKey.getSize()`.
- **Cause:** JC305's imported `PublicKey` (class token 3) redeclares the four
  inherited `Key` methods, whose method order differs from the embedded API.
  The JC305-to-embedded token translation `{1,3,2,0}` had only been applied to
  `Key` class token 0. The call therefore reached `NativeKey.clearKey()`
  instead of `getSize()S`, leaving a value on the operand stack. The same
  redeclared prefix exists on `PrivateKey`, `SecretKey`, DSAPrivateKey,
  DSAPublicKey, KoreanSEEDKey, DHPrivateKey, DHPublicKey, and RSA key
  interfaces already handled by specialized maps. Do not apply the prefix map
  to DSAKey, whose token 1 represents a DSA-specific operation.
- **Viable fix:** Apply the common four-method translation to imported JC305
  key interfaces that redeclare the `Key` prefix, before specialized full
  interface maps. Current implementation is in
  `choupi/src/jc_handlers/jc_cp.cpp`.
- **Validation:** Unchanged API 3.0.5 Security CAP RSA-512 constructor failed
  with `6F00` on current-source VPCD before the fix and passed afterward
  (one selected call; 900 excluded). PC, ARM, and RISC-V asymm_crypto builds
  passed. Identified physical D92 flash verification and finite startup monitor
  passed, but the constructor itself has not yet been tested physically.
  Commit: `a6bdb2f`.


## Full JC305 Security sweep loses PC/SC after RSA public 4096 KeyBuilder (2026-10-28)

- **Symptom:** With the corrected JC305 key-interface method mapping, focused
  physical D92 API 3.0.5 tests passed all 39 `KeyPair(byte, short)` constructor
  probes (27 supported OK, 12 `UNSUPPORTED_PER_SPEC`) and 14 `genKeyPair()`
  probes (9 supported RSA/RSA-CRT/EC_FP OK, five `UNSUPPORTED_PER_SPEC`). A
  subsequent full Security sweep aborted after 42 calls while testing
  `KeyBuilder.buildKey(TYPE_RSA_PUBLIC, LENGTH_RSA_4096)`: the first failed APDU
  surfaced `SCARD_E_NOT_TRANSACTED`; following calls reported a disconnected
  card and cleanup could not connect.
- **Cause:** Not established. This is a PC/SC transport loss, not a card status
  word or explicit unsupported response. The physical USB device and serial
  console remained present; a finite serial monitor afterward passed repeated
  runtime checks. PC/SC then listed only the two Virtual PCD readers.
- **Viable fix:** None established. After PC/SC reader enumeration recovers,
  isolate only the RSA public 4096 `KeyBuilder` probe, capture finite serial
  output immediately after failure, and distinguish a firmware reset/CCID state
  issue from host PC/SC loss. Do not relabel the 4096 result as expected
  unsupported without a card response.
- **Validation:** Focused physical CSVs are
  `AlgTest_JavaCard/deploy/results/keypair-constructor-physical-20261028/`
  (`all-keypair-constructors.csv` and `keypair-generation.csv`). Full-run CSV
  `security-full.csv`; post-failure D92 monitor passed.

## API 3.0.5 Security sweep exhausts persistent filesystem at RSA CRT setP (2026-10-29)

- **Symptom:** Two full physical Security runs on 2BED completed 901 calls but
  returned `6F00` from `RSAPrivateCrtKey.setP(TYPE_RSA_CRT_PRIVATE,
  LENGTH_RSA_2048)` at the same point. The exact call passed when isolated.
  A narrowed rerun including the preceding API test classes reproduced the
  same failure; the next `setQ`, `setDP1`, `setDQ1`, and `setPQ` rows passed.
- **Cause:** Serial trace proved persistent filesystem exhaustion, not RSA
  arithmetic failure. `fs_write` rejected a new 131-byte array record with
  5,929 records and 65,409/65,536 bytes occupied. The old `OutOfFlash` to
  `IOException` mapping escaped the CAP as `6F00`. **Resolution:** status 1 now
  maps to `SystemException.NO_RESOURCE` (reason 5), validated by the bounded
  physical rerun documented under “Persistent write exhaustion” above. The
  65,535-byte arena limit protects current `uint16_t` offsets; a 77,824-byte
  trial corrupted snapshot offsets and is rejected. Safe persisted-root
  enumeration/reclamation remains open. Do not free persistent records by
  clearing heap slots alone because Java Card references encode stable handles.
- **Validation:** The physical 3.0.5 Crypto run on asymm_crypto completed
  466 calls (302 `OK`, 163 `UNSUPPORTED_PER_SPEC`, one known SHA-512 PSS
  default-salt size exception) and all previous Crypto failures except that
  valid size error passed. Security ran twice (396 `OK`, 503/504 unsupported,
  two/one `6F00` respectively). The isolated `setP`/`setPQ` plus public RSA
  4096 KeyBuilder tests passed 3/3. The narrowed 629-call prefix reproduced
  one `setP` failure. Monitor log:
  `artifacts/logs/2BED-asymm-security-prefix-serial.log`; CSVs and hashes are
  recorded in `progress.md` under `physical-asymm-crypto-security-retest`.
- **Follow-up:** Audit object/root retention and safe persistent reclamation.
  Do not classify this as an RSA implementation defect, and do not increase
  flash partitions or alter configuration without a measured layout review.

## NativeKey.clearKey byte-by-byte persistence stalled EC API calls (2026-10-29)

- **Symptom:** The focused API 3.0.5 uninitialized `ECKey.getK()` request timed
  out over VPCD after 15 seconds. The VM trace reached native dispatch index 7
  (`NativeKey.clearKey`) while preparing the test and repeatedly wrote
  persistent key-array bytes.
- **Cause:** `clearKey()` zeroed each persistent RSA/EC component by calling
  `JC_Array::setByteEntry()` for every byte. Each byte incurred an individual
  filesystem syscall; the clear did not finish within the focused API test
  deadline.
- **Viable fix:** For persistent byte arrays, validate the stored byte-array
  record and write one zero-filled record through the existing
  `write_persistent_tag()` path so transaction snapshots are preserved. Keep
  the in-memory byte loop for transient arrays. `NativeKey.clearKey()` uses
  this for both RSA and EC components.
- **Validation:** The unchanged API 3.0.5 Security CAP completed 901 calls on
  VPCD after the change: uninitialized `ECKey.getK()` was `OK`, 900 unrelated
  calls were intentionally filtered, and GPPro upload/cleanup passed. PC,
  ARM, and RISC-V `asymm_crypto` builds passed. No physical Pico was connected;
  hardware validation remains open. Final report:
  `artifacts/results/deploy/ec-key-getk-final-20261029/`.

## Persistent key component writes stalled EC API setup (2026-10-29)

- **Symptom:** The focused API 3.0.5 `copyDomainParametersFrom(null)` test
  timed out before reaching the null-source method. Dispatch tracing showed it
  stalled earlier while the applet initialized key state through native
  `NativeKey.setA` (dispatch index 29). Persistent EC component setters and
  domain copies wrote each array byte with an individual filesystem syscall.
- **Cause:** Per-byte persistent array updates made key setup/copy proportional
  to repeated filesystem syscall cost. The earlier clear-only bulk optimization
  did not cover setter writes.
- **Viable fix:** Build a full padded byte vector in memory, validate and update
  the existing persistent byte-array record with one transaction-aware
  `write_persistent_tag()` call, then update the component length. Use the same
  path for persistent RSA/EC setters and EC domain copying; retain per-byte
  updates for transient arrays. This change also centralizes bulk clearing.
- **Validation:** Unchanged API 3.0.5 Security CAP VPCD runs passed for
  uninitialized `ECKey.getField()` and `copyDomainParametersFrom(null)` (901
  calls each, selected row `OK`, 900 unrelated rows filtered, upload and
  cleanup passed). PC, ARM, and RISC-V `asymm_crypto` builds passed. No
  physical Pico was connected. Results:
  `artifacts/results/deploy/ec-key-getfield-final-20261029/` and
  `artifacts/results/deploy/ec-key-copy-domain-null-batched-20261029/`.

## VPCD endpoint can stall after a focused API timeout (2026-10-29)

- **Symptom:** After a 40-second focused EC API test timeout on `Virtual PCD 00
  01` (endpoint `127.0.0.1:35964`), the next run on the same reader timed out
  in GPPro upload after 50 seconds even though the reader was enumerated.
- **Cause:** Not established. Reader enumeration alone did not establish that
  the VPCD transport was transacting. A fresh presence probe succeeded on the
  other paired endpoint and tests then passed there.
- **Viable workaround:** Start a fresh VPCD emulator, run
  `pcsc_scan -r`, require `VpcdPresenceProbe` to report `present=true`, and
  after repeated `00 01` failures switch to its paired reader/endpoint
  `Virtual PCD 00 00` / `127.0.0.1:35963`. Do not interpret a GPPro upload
  timeout as a VM API result.
- **Validation:** On `00 00`/35963, unchanged API 3.0.5 Security CAP tests
  passed for valid P-256 domain copy, mismatched source size, and uninitialized
  source. Artifacts:
  `artifacts/results/deploy/ec-key-copy-domain-p256-reader00-20261029/` and
  `artifacts/results/deploy/ec-key-copy-domain-errors-reader00-20261029/`.

## Physical Cipher factory 6F00 not reproduced in VPCD (2026-10-29)

- **Symptom:** A prior physical API 3.0.5 Crypto report had a `6F00` for the
  `javacardx.crypto.Cipher` `Cipher()` scenario.
- **Cause:** Not established. This may be card-state or transport-specific;
  current evidence does not isolate it to native `Cipher.getInstance` code.
- **Viable next step:** Do not change native implementation based only on that
  physical row. Rerun the unchanged Crypto CAP on an identified physical Pico
  and compare its exact CAP, profile image, reader, and row against the VPCD
  result before attributing the failure.
- **Validation:** Focused unchanged API 3.0.5 Crypto CAP completed 466 VPCD
  calls: selected `Cipher()` returned object token `86` as expected, 465 other
  calls were filtered, and GPPro cleanup passed. Result:
  `artifacts/results/deploy/cipher-factory-vpcd-20261029/`.
  A second fresh-image run at 07:10 local repeated the same one-row pass and
  cleanup. Neither run establishes why the physical 6F00 occurred.

## Both VPCD endpoints timed out during CAP upload (2026-10-29)

- **Symptom:** After an earlier focused API timeout, a presence probe on the
  paired VPCD endpoint reported the reader present, but subsequent GPPro CAP
  uploads timed out on both `Virtual PCD 00 00` (`127.0.0.1:35963`) and
  `Virtual PCD 00 01` (`127.0.0.1:35964`). The native test applet was never
  installed and the targeted native methods were not invoked.
- **Cause:** Unknown. Reader enumeration and a successful presence probe did
  not establish that a later PC/SC transaction could complete.
- **Viable solution:** None confirmed for this two-endpoint failure. The prior
  workaround (fresh emulator, presence probe, switch paired endpoint) remains
  useful for a single stalled endpoint, but do not repeat an upload timeout
  and interpret it as a runtime API result. Refresh/restart the emulator
  session through its normal test harness, verify transport, then retry once.
- **Validation:** Both endpoint upload timeouts are recorded in
  `artifacts/results/deploy/signature-initial-digest-invalid-trace-20261029/`
  and `signature-initial-digest-invalid-trace-reader01-20261029/`. No physical
  Pico reader was enumerated.

## VPCD endpoint refused the emulator after its service had idled (2026-10-29)

- **Symptom:** GPPro could not connect although the Virtual PCD readers were
  listed by PC/SC. The emulator logged `ERROR: connecting to vpcd endpoint
  127.0.0.1:35964: Connection refused` and exited.
- **Cause:** The PC/SC plug-and-play service had gone idle between emulator
  runs. Reader enumeration alone did not leave the VPCD TCP backend ready. A
  background emulator command also exited with its launching shell, and two
  concurrent API clients contended for the same virtual card during diagnosis.
- **Viable solution:** Through the elevated host-access path, run
  `PICO2_HOST_USB_ACCESS=1 ./tools/env pcsc_scan -r` before each new emulator
  session. Keep the emulator attached to a persistent command session (do not
  background it from a short-lived shell), require
  `VpcdPresenceProbe ... --require-present`, and run only one API client at a
  time. Use `--test-only` for subsequent isolated cases while the CAP remains
  installed.
- **Validation:** GPPro listed the installed Crypto CAP after this sequence.
  The unchanged API 3.0.5 Crypto CAP then passed the selected RSA/SHA-256
  `Signature.setInitialDigest()` known-signature case over VPCD (465 checks
  skipped). Evidence:
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/crypto-sha256-testonly.csv`.

## Signature initial-digest metadata exceeded the native IV storage (2026-10-29)

- **Symptom:** Valid RSA and ECDSA `Signature.setInitialDigest()` calls
  returned `EXC_RUNTIME`, while invalid length/block tests correctly returned
  `ILLEGAL_VALUE`.
- **Cause:** The native Signature factory allocates a 16-byte IV array, but the
  initial-digest metadata encoding required 18 bytes (128-bit byte count plus
  a redundant 16-bit state length). `storeInitialDigestState()` therefore
  raised `ArrayStoreException` for every valid state.
- **Viable fix:** Keep the full 128-bit SHA-384/512 byte count in the existing
  16-byte array and derive the state length from the digest algorithm. The
  state itself remains in `recoveryHash`; do not enlarge the IV array because
  MAC algorithms depend on its existing 16-byte size.
- **Validation:** The full unchanged upstream `setInitialDigest` group passed
  on VPCD 00 00: four RSA/ECDSA SHA-1/SHA-256 valid vectors and three invalid
  length/block cases, in a 466-call Crypto run (10 OK, eight expected
  unsupported, 448 filtered). SHA-224/384/512 and physical validation remain
  open.
- Paired-endpoint result: an isolated ECDSA/SHA-256 run timed out on 00 01,
  while isolated ECDSA/SHA-1 and SHA-256 round trips both passed on
  `Virtual PCD 00 00` / `127.0.0.1:35963` (465 calls skipped each). For these
  expensive Crypto test vectors, switch to 00 00 after a 00 01 timeout rather
  than treating the first timeout as a VM failure. Artifacts are
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/crypto-ecdsa256-reader00.csv`
  and `crypto-ecdsa-sha1-reader00.csv`.
- The earlier physical `signPreComputedHash()` ECDSA/SHA-1 and SHA-256
  `ILLEGAL_USE` results are not reproduced by the unchanged Crypto CAP on
  VPCD: both selected sign/verify round-trip cases pass on 00 00. Use a
  200-second API client deadline for those regex-filtered calls because the
  runner emits/filters preceding Crypto rows before reaching them. Results:
  `crypto-ecdsa-precomputed-sha256-testonly-reader00-retry.csv` and
  `crypto-ecdsa-precomputed-sha1-reader00.csv` in the signature test artifact
  directory.

## RSA precomputed-hash length error used the wrong reason code (2026-10-29)

- **Symptom:** The upstream API 3.0.5 Crypto CAP reported
  `CryptoException.ILLEGAL_VALUE` (`0x0001`) for a malformed precomputed hash,
  while the API contract requires `ILLEGAL_USE` (`0x0005`).
- **Cause:** Native RSA `signPreComputedHash()` and
  `verifyPreComputedHash()` classified a hash-length/digest-length mismatch as
  `ILLEGAL_VALUE`.
- **Viable fix:** Throw `CryptoException.ILLEGAL_USE` from both native RSA
  precomputed-hash entry points when the hash length differs from the selected
  message digest size.
- **Validation:** Unchanged upstream Crypto CAP VPCD test passed the malformed
  hash row with expected reason `0x0005`; 465 other calls were skipped. PC,
  ARM, and RISC-V `asymm_crypto` builds passed. No physical validation was run.
  Artifact:
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/crypto-rsa-precomputed-invalid-reader00-fixed.csv`.

## SignatureMessageRecovery beginVerify mode error (2026-10-29)

- **Symptom:** The API 3.0.5 Crypto CAP's `beginVerify after init for sign`
  case did not receive the specified `CryptoException.ILLEGAL_USE`. The earlier
  physical run recorded reason `0x0004` (`INVALID_INIT`); before the fix, the
  current PC JCVM/VPCD case returned `6F00`.
- **Cause:** `beginVerify()` used the shared `requireInitialized(...,
  MODE_VERIFY)` guard. That guard reports `INVALID_INIT` for either an
  uninitialized object or a mode mismatch, but the Java Card
  `SignatureMessageRecovery.beginVerify()` contract specifically requires
  `ILLEGAL_USE` when initialized for signing.
- **Viable fix:** First call the no-mode initialization check, then report
  `ILLEGAL_USE` for a non-verify mode in `javacard_security_Signature_beginVerify`.
  This preserves `INVALID_INIT` when the object was never initialized.
- **Validation:** Unchanged upstream API 3.0.5 Crypto CAP on current PC JCVM/VPCD
  passed the selected row (`OK`; 465 other rows filtered). CAP upload and
  cleanup passed. PC, ARM, and RISC-V `asymm_crypto` builds passed. No physical
  device was available. Source commit: `be0ea69`.
- **Other diagnostic:** RSA/SHA-512 PSS on the same API CAP returned
  `CryptoException.ILLEGAL_VALUE` in VPCD. Native PSS initialization checks
  whether the key can fit the SHA-512 digest and default digest-length salt;
  insufficient key length is a plausible explanation, but the test key size
  and exact failing subcall were not established. Do not change this case
  without isolating those details.
- **Artifacts:**
  `artifacts/results/deploy/crypto-current-vpcd-20261029/mr-beginverify-wrong-mode-fixed.csv`,
  `rsa-sha512-pss.csv`, and `rsa-iso9796-params.csv`.

## SignatureMessageRecovery parameterized init must return ILLEGAL_USE (2026-10-29)

- **Symptom:** The API 3.0.5 Crypto CAP case `ALG_RSA_SHA_ISO9796_MR init with
  parameters (illegal use)` returned `CryptoException.ILLEGAL_VALUE` in VPCD.
- **Cause:** The overload `Signature.init(Key, mode, parameters, offset,
  length)` sent all RSA algorithms through the PSS parameter handler, which
  returned `ILLEGAL_VALUE` because message-recovery algorithms do not accept
  this overload. The Java Card `SignatureMessageRecovery` contract explicitly
  requires `ILLEGAL_USE` for parameterized init.
- **Viable fix:** In the native parameterized-init dispatch, detect a
  message-recovery Signature before the generic RSA/PSS handling and throw
  `CryptoException.ILLEGAL_USE`.
- **Validation:** Unchanged API 3.0.5 Crypto CAP on current PC JCVM/VPCD passed
  the selected row (`OK`; 465 other rows filtered); upload and cleanup passed.
  PC, ARM, and RISC-V `asymm_crypto` wrapper builds passed. No physical device
  was available. Source commit: `2ae55a4`.
- **Reference:** [Oracle Java Card `SignatureMessageRecovery`](https://docs.oracle.com/en/java/javacard/3.2/jcapi/api_classic/javacard/security/SignatureMessageRecovery.html).
- **Artifacts:**
  `artifacts/results/deploy/crypto-current-vpcd-20261029/mr-init-parameters-fixed.csv`.

## KeyAgreement secp192r1 VPCD failures were not repeatable (2026-10-29)

- **Symptom:** The dated physical API 3.0.5 report had `6F00` for the
  `KeyAgreement()` factory, two secp192r1 DH/DHC known-secret operations, and
  initialization with an uninitialized EC private key.
- **Investigation:** On current PC JCVM/VPCD, the factory returned `OK`, the
  uninitialized-key case returned the expected `UNINITIALIZED_KEY` (`0x0002`),
  and ECDSA/SHA-1 with secp192r1 passed. DH and DHC each returned `6F00` once;
  the runtime diagnostic identified exception ID 5 (`IOException`). A
  trace-enabled repeat passed both rows (`returned=0x0014` each). Temporary
  PC-only diagnostics around persistent byte-array reads reported no read
  failure on the passing repeat and were removed afterward.
- **Cause:** Not established. No native code change is justified by one
  non-repeatable `IOException` followed by a passing run. Consider a transient
  PC emulator/persistent-storage interaction; this is only a hypothesis.
- **Viable fix:** None confirmed. Preserve the exact-image/profile and reader
  identity when retesting on physical hardware; capture VM diagnostics if the
  failure recurs.
- **Validation:** Clean-source PC, ARM, and RISC-V `asymm_crypto` builds pass.
  VPCD result files:
  `ecdsa-sha1-secp192-known.csv`, `ec-svdp-p192-known.csv`,
  `ec-svdp-p192-traced.csv`, `keyagreement-factory.csv`, and
  `keyagreement-uninitialized.csv` under
  `artifacts/results/deploy/crypto-current-vpcd-20261029/`.

## VPCD presence probe and overlapping API runner attempts (2026-10-29)

- **Symptom:** A VPCD presence probe printed `present=false`, but a shell
  pipeline ending in `tee` still returned success. A later attempt overlapped
  the still-running runner against the same PC/SC reader, producing duplicate
  package warnings, `SCARD_W_RESET_CARD`, disconnects, and failed cleanup.
- **Cause:** The probe's nonzero status was masked by the successful `tee`
  command because the shell did not enable `pipefail`; the exec session had not
  yet completed when a second runner started. These overlapping-run artifacts
  are invalid behavioral evidence.
- **Viable procedure:** Use `set -euo pipefail` for probe pipelines, require
  `present=true` before the runner, use a fresh emulator flash image for every
  attempt, and wait until the runner exec session exits before reusing the
  reader. The clean follow-up ran both secp192r1 DH/DHC known-secret cases
  successfully (2 `OK`, return `0x0014`) and cleaned up the CAP.
- **Artifacts:** `artifacts/results/deploy/crypto-dh-vpcd-20261029/`; clean
  output is in `retry2/`.

## JCAlgTest host VPCD applet path passes after restore (2026-10-29)

- **Observation:** Current-source `tools/test-pc-vpcd-applet --passes 2` passed
  twice through the unchanged JCAlgTest v1.8.2 CAP, including GETVERSION,
  JCSystem, support probes, SHA-256 doFinal and static performance, and the
  expected unsupported one-shot factory result. Each pass reported 15/15; the
  second emulator process resumed the installed applet from the saved image.
- **Resolution:** The older TODO statement that the VPCD applet failed on its
  first applet field access is stale for current source. The host integration
  path now validates applet stateful API calls and process restart. It does not
  establish physical Pico persistence or Windows PC/SC behavior.
- **Timing/artifact:** First pass 58.449 s, second process 18.333 s, initial
  VPCD connect 23.570 s. Emulator log:
  `artifacts/logs/host-vpcd-jcalgtest-20261029-two-pass.log`.

## Full API 3.0.5 Crypto VPCD sweep is too slow for a normal session (2026-10-29)

- **Observation:** The unchanged unfiltered Crypto CAP processed 190 of 466
  calls in about 17 minutes on PC JCVM/VPCD. The processed rows were 112 `OK`
  and 78 `UNSUPPORTED_PER_SPEC`, with no failed call. The latest processed
  algorithm was `ALG_RSA_SHA_256_PKCS1`.
- **Action and cleanup:** Stopped the Java client after the observed pace
  implied several hours for all calls. The wrapper reported `CLEANUP OK` and
  removed `AlgTestApiCrypto_305.cap`; the emulator and `pcsc_scan` keepalive
  were stopped. Do not count this as full acceptance.
- **Viable approach:** Use filtered VPCD runs for focused rows and start from a
  fresh zero-length flash image per sweep. Only schedule the full unfiltered
  CAP run in a window long enough to complete all 466 calls and cleanup.
- **Artifacts:** `artifacts/results/deploy/crypto-current-vpcd-20261029/`
  (`full-crypto-current.csv`, `full-crypto-emulator.log`); raw partial client
  log at `/tmp/apitest_card_5x39_p6b/APITEST_log_1793245348258.log`.

## Local asymmetric VPCD test needs an unrestricted local socket (2026-10-29)

- **Symptom:** `./tools/env python3 tools/tests/test_asymm_crypto_vpcd.py`
  failed immediately in the restricted execution context with
  `TEST:FAIL [Errno 1] Operation not permitted`.
- **Viable workaround:** Run this local PC/VPCD test through the approved
  elevated execution path so its loopback emulator socket can be created. The
  test did not access USB or PC/SC.
- **Validation:** Elevated run passed `TEST:SUMMARY pass=99 fail=0`, including
  50 RSA Signature vectors and SHA-512 PSS precomputed sign/verify using an
  explicit zero-salt parameter. This confirms that valid native SHA-512 PSS
  path. It does not explain the separate API 3.0.5 CAP `ILLEGAL_VALUE`, whose
  generated key size/default salt constraints remain unverified.
- **Artifact:** `artifacts/logs/asymm-crypto-keypair-vpcd-20261027.log`.

## API Crypto SHA-512 PSS row uses an undersized 1024-bit test key (2026-10-29)

- **Symptom:** The API 3.0.5 Crypto CAP's
  `ALG_RSA_SHA_512_PKCS1_PSS` sign/verify sequence returned
  `CryptoException.ILLEGAL_VALUE` (`0x0001`) on current PC JCVM/VPCD.
- **Cause:** Inspection of the unchanged host client bytecode shows the row's
  case 46 maps to `K_RSA`, and `selectKeys(K_RSA)` uses
  `ApiTestCryptoKeys.rsaCrt()`, which builds a 1024-bit RSA-CRT key. Default
  PSS salt length equals the SHA-512 digest size (64 bytes). PSS needs at least
  `hLen + sLen + 2 = 130` encoded bytes, but the key provides 128. Rejecting
  this key/parameter combination with `ILLEGAL_VALUE` is correct.
- **Viable resolution:** None needed in firmware. Keep SHA-512 PSS enabled and
  retain the key-size validation. Do not modify the upstream CAP; its fixture
  cannot exercise this default-salt operation. Use explicit shorter salt or a
  larger RSA key for an affirmative operation test.
- **Validation:** Existing native PC/VPCD asymmetric suite passed 99/99,
  including SHA-512 PSS precomputed sign/verify with explicit zero salt. No
  physical device was available.
- **Evidence:** `javap -c -p` inspection of
  `AlgTest_JavaCard/deploy/client/AlgTestJClient.jar`, class
  `algtestapi.ApiTestCryptoSignature` and `algtestapi.ApiTestCryptoKeys`.

## Reuse of VPCD storage caused intermittent Crypto IOException results (2026-10-29)

- **Symptom:** After multiple Crypto CAP upload/test/cleanup cycles on one PC
  emulator flash image, uninitialized-key Signature and GCM test rows
  intermittently returned `6F00`; VPCD diagnostics reported exception ID 5
  (`IOException`). KeyAgreement DH/DHC secp192r1 rows also failed once with
  `IOException` on the reused image.
- **Cause:** The failing rows were not native algorithm failures: the same
  tests passed after starting the emulator with a fresh zero-length flash image.
  The persistent PC emulator state was contaminated or stale; the exact stored
  record involved was not identified. This does not establish a physical card
  storage defect.
- **Viable workaround:** Start a fresh PC JCVM emulator with a new zero-length
  flash image for each isolated Crypto sweep. Keep `pcsc_scan` active while
  starting VPCD, then require `VpcdPresenceProbe --require-present` before the
  client. Avoid repeating many CAP lifecycles against one emulator image.
- **Validation:** On fresh VPCD, Signature, SignatureMessageRecovery, and Cipher
  uninitialized-key rows returned expected `UNINITIALIZED_KEY`; the GCM row
  returned `NO_SUCH_ALGORITHM` (`UNSUPPORTED_PER_SPEC`), and `AEADCipher()`
  returned normally. DES 64/192 and AES 192/256 cipher known-answer rows all
  passed (five selected rows including DES CBC ISO9797-M2). Clean PC build
  passed after removing temporary diagnostics. No physical device was tested.
- **Artifacts:** `cipher-keylengths.csv`, `cipher-aead-errors.csv`,
  `cipher-aead-errors-repeat.csv`, `signature-aead-persistent-trace.csv`, and
  `signature-aead-persistent-trace.log` in
  `artifacts/results/deploy/crypto-current-vpcd-20261029/`.

## Focused API Core rows on VPCD (2026-10-29)

- **Observation:** The unchanged API 3.0.5 Core CAP contains 291 calls; an
  unfiltered run did not finish before the 300-second limit. A fresh-image
  focused rerun completed in under a minute when all but `Applet.deselect()`
  were filtered. The selected row returned `OK` (`effect=as expected`,
  diagnostic `returned=1`); this does not reproduce the physical 2026-10-28
  `WRONG_RESULT` (`-15`) and does not replace physical validation.
- **Viable filter:** `--exclude '^(?!.*deselect).*$'` leaves method descriptors
  containing `deselect` and marks other calls `SKIPPED_BY_USER`. Avoid matching
  an assumed exact delimiter spacing: a stricter negative lookahead filtered
  the target too and silently left all 291 rows skipped.
- **Artifacts:** `artifacts/results/deploy/core-deselect-vpcd-20261029/`.

## JCAlgTest RSA-512 support probe unexpectedly slow on VPCD (2026-10-29)

- **Symptom:** The official JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` run on
  `asymm_crypto` reported 133.756 seconds for the `ALG_RSA LENGTH_RSA_512`
  key-pair-generation APDU. The run was interrupted before the next key size;
  its CSV is partial. The following client-side `IllegalArgumentException`
  (`apdu must be at least 2 bytes long`) resulted from that deliberate VPCD
  shutdown while the next APDU was in flight and is not a VM response.
- **Counter-evidence:** The independent native VPCD suite ran the same public
  Java Card `KeyPair.genKeyPair()` path and passed all 99/99 checks. Its
  measured RSA/RSA-CRT generation APDU times were 1.068/1.149 seconds at 512,
  1.334/3.469 seconds at 1024, and 47.488/23.334 seconds at 3072 bits.
  Therefore RSA-512 generation itself is not established as the source of the
  133.756-second result. The discrepancy may lie in the JCAlgTest workload or
  execution path; do not change RSA based only on this observation.
- **Next diagnostic:** Compare the precise JCAlgTest request and APDU timing
  against the native probe on the same fresh emulator image/profile. Capture
  the official result without interrupting an in-flight request, then run the
  exact APDU against the same applet with timing. Do not use the partial CSV as
  a profile support comparison.
- **Artifacts:** Partial official attempt in
  `artifacts/results/jcalgtest-vpcd-asymm-current-pcsc-20261029/`; complete
  native probe timing in
  `artifacts/logs/asymm-crypto-keypair-vpcd-timing-20261029.log`.

## PC VPCD does not exercise the Pico CCID GET DATA dispatcher (2026-10-29)

- **Symptom:** Running the SmartcardIO `GPGetDataApduProbe` against a plain
  `choupi --vpcd` PC emulator returned `6999` for `80 CA 00 66`. The emulator
  VPCD endpoint implements CAP/runtime APDUs but does not pass this request
  through `src/ccid_device.cpp`, where card-manager GET DATA is dispatched.
- **Viable test split:** Use `./tools/test-gp-get-data` for the shared
  CPLC/Card Recognition TLV encoder and response status. Use
  `tools/GPGetDataApduProbe.java` only with actual CCID firmware for open and
  SCP03-authenticated GET DATA checks. The separate PC VPCD GPPro install test
  remains valid for CAP LOAD/INSTALL; it does not cover GET DATA.
- **Validation:** The existing VPCD GPPro install flow passed after the
  unsuccessful GET DATA attempt was removed from it. No physical CCID reader
  was tested in this step.

## Pico reader disappears or is omitted from PC/SC after flash/reconnect (2026-10-29)

- **Symptom:** After a Pico passed an APDU probe, its PC/SC reader disappeared
  before a second run. A second Pico flashed successfully and passed finite
  serial startup, but the immediate `pcsc_scan -r` listed only VPCD readers.
- **Viable recovery:** Keep testing on another identified physical Pico rather
  than waiting for the first one. Refresh USB access with the permitted
  `PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader`, then run
  `pcsc_scan -r` again and target the exact reader name. The helper restored
  both physical readers; the second board then passed open and
  SCP03-authenticated CPLC queries. Do not infer a firmware APDU failure from
  a reader omitted by PC/SC.
- **Confirmed after a fresh firmware cycle (2026-10-31):** Immediately after
  flashing Pico 2 `2BED26ED20556535`, GPPro saw only the VPCD readers. Running
  the approved helper and `pcsc_scan -r` made `Pico 2 CHOUPI CCID [JCVM CCID]
  (2BED26ED20556535) 00 00` available; a bounded GPPro `--list` then passed.
  Apply this refresh before diagnosing a post-cycle GPPro reader-selection
  error.
- **Artifacts:** `artifacts/logs/cycle-choupi-20261029T143004Z.log`,
  `artifacts/logs/cycle-choupi-20261029T143255Z.log`, and
  `artifacts/logs/gp-cplc-physical-D92E743D5F640C0F-20261029.log`.

## Physical Core 6F00 results differ between identical-image Pico boards (2026-10-29)

- **Observation:** The current ARM firmware image was flashed to 2BED and D92
  and each passed the finite `choupi_runtime` monitor. The unchanged API 3.0.5
  Core CAP was run with `--no-scenarios` and exact-reader pinning. D92 completed
  291 calls with 272 `OK`, 11 `UNSUPPORTED_PER_SPEC`, and eight expected
  unavailable Multi-scenario rows; no `SW_ERROR`. 2BED completed the same 291
  calls twice with 198 `OK`, 74 `SW_ERROR` (`6F00`), 11
  `UNSUPPORTED_PER_SPEC`, and the same eight scenario rows. CAP cleanup passed
  on each run; GPPro could still list the ISD and persistent packages.
- **Scope:** 2BED's errors cluster in JCSystem (6), Util (19), AID (14), APDU
  (1), OwnerPIN (13), PIN (12), OwnerPINx (6), and OwnerPINxWithPredecrement
  (3). `Applet.deselect()` itself returned the expected `-19` on both boards.
  2BED had the JCAlgTest package/applet registered; D92 had that package plus
  an `F01234567890` package. The differing registry contents are a variable,
  not an established cause. No firmware change is justified yet.
- **Next diagnostic:** Establish equivalent clean persistent applet/package
  state, then rerun the exact Core command and capture per-APDU VM fault/exception
  diagnostics on 2BED. Compare dynamic package IDs and imported API dispatch
  mappings with D92. Do not classify this as transport loss: the test completed
  and received `6F00` responses.
- **Artifacts:**
  `artifacts/results/deploy/core-physical-retest-20261029/core-no-scenarios.csv`,
  `2BED-core-no-scenarios-retry.csv`, `D92-core-no-scenarios.csv`, and
  `pico1.csv` (the first parallel attempt stopped at the optional Ext CAP
  install refusal, then rolled it back).

### Follow-up: persistent applet context survived GP DELETE

- **Diagnostic:** Reproducing the failing 2BED pattern on D92 with serial
  capture showed 74 `IOException` failures at `arraylength`. Every failed read
  was a persistent array body under the Core applet's instance root; the Pico
  filesystem read returned its missing-record status. A heap-cache probe did
  not find direct cache lookups for these child tags.
- **Cause:** GP DELETE cleared the applet's persistent records and deselected
  it, but retained `selected_applet_context` and its heap. A subsequent
  reinstall with the same package/AID could reuse the already-restored object
  graph whose fields referenced the deleted array bodies.
- **Viable fix:** After a successful GP DELETE, call
  `choupi_vm_forget_selected_applet()` to clear transient lifecycle state and
  destroy the selected context/heap. Keep ordinary deselect and reset behavior
  unchanged so an installed applet still preserves its context across normal
  reselection/reset handling.
- **Validation:** `./tools/build arm` and `./tools/build riscv` passed. The
  fixed image (`6a3915f6611dca76a728b6085fda6af6671a16ca1e59ba1fef674e9ed3a4b17a`)
  was flashed to 2BED and passed finite startup monitoring. Two consecutive
  unchanged API 3.0.5 Core runs each completed 291 calls with 272 `OK`, 11
  `UNSUPPORTED_PER_SPEC`, eight expected unavailable scenarios, and zero
  `SW_ERROR`; GP DELETE/cleanup passed both times. The overlay regression suite
  passed with `TEST:SUMMARY fail=0`. D92's reflash recovery timed out; no D92
  result is attributed to the fix.
- **Artifacts:** `artifacts/results/deploy/core-physical-retest-20261029/`
  (`2BED-candidate-fix-pass1.csv`, `2BED-candidate-fix-pass2.csv`) and
  `artifacts/logs/cycle-choupi-20261029T154706Z.log`.

## Default-profile build failures in signature digest support (2026-10-29)

- **Symptom/cause:** The `current` profile's ARM build failed compiling
  `Signature.setInitialDigest()` when no RSA/ECDSA signature algorithms were
  selected, because its validation called helper functions compiled only for
  those optional providers. After guarding those calls, ARM linked with
  undefined `mbedtls_sha256_free`: the firmware CMake source condition omitted
  `sha256.c` when SHA-256 was enabled by itself.
- **Viable fix:** Compile the algorithm check only when its family is selected,
  and include SHA-256's Mbed TLS source in the firmware whenever
  `JC_SUPPORT_SHA256` is enabled. An unused local must be marked
  `[[maybe_unused]]` for the profile with both signature families disabled.
- **Validation:** `./tools/build arm` and `./tools/build riscv` passed on the
  `current` profile after both changes. The RISC-V wrapper builds its
  documented VM-core smoke image only.

## Historical issue index

Each link opens the byte-for-byte original at the recorded line. Titles retain
original terminology/status; check current summary and later fixes before reuse.

- [Physical Pico 2 USB interface disappeared after successful flash (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L1)
- [PC/SC reader suffix changes when a second Pico is present — 2026-10-27](archive/2026-10-29-pre-compaction/QKB.md#L17)
- [Asymmetric physical probe vector assertion stale after suite expansion — 2026-10-27](archive/2026-10-29-pre-compaction/QKB.md#L35)
- [Guest VM loses all Pico USB devices after D92 flash (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L49)
- [Project issue knowledge base](archive/2026-10-29-pre-compaction/QKB.md#L62)
- [RSA NOPAD reverse key/mode pair rejected as `ILLEGAL_VALUE` (2026-10-29)](archive/2026-10-29-pre-compaction/QKB.md#L64)
- [Intermittent VPCD ECDSA SHA-224/512 `6F00` during combined API filtering (2026-10-29)](archive/2026-10-29-pre-compaction/QKB.md#L85)
- [JCAlgTest 1.8.2 KeyPair RSA-3072 rows send a 4096-bit length (2026-10-28)](archive/2026-10-29-pre-compaction/QKB.md#L104)
- [Long RSA-3072 KeyPair generation exceeded default VPCD deadline (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L130)
- [VPCD command-case framing for no-input APDUs (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L145)
- [Pico USB devices absent from the guest VM despite being plugged in (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L160)
- [NativeKey ROM class identity mismatch — 2026-10-26](archive/2026-10-29-pre-compaction/QKB.md#L175)
- [jcarray test target lacked generated headers and collector stubs — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L192)
- [PC/SC stopped responding after bounded GPPro GET STATUS timeout — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L225)
- [Pico runtime USB devices absent from PC/SC after clean firmware cycle — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L247)
- [Firmware upload preserves installed CAP state; first flashed BOOTSEL board lost runtime enumeration — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L269)
- [GPPro reader state after Pico runtime re-enumeration — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L288)
- [Physical GPPro multi-package install: use `--install-only` for later applets — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L306)
- [Pico D92 PC/SC reader can enumerate while APDUs stall — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L319)
- [Host VPCD DELETE prefix erase requires privileged filesystem syscall — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L333)
- [Raw VPCD multi-package APDU setup and dynamic SELECT routing — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L350)
- [Concurrent ARM/RISC-V builds collided in the shared ROM-mask tree — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L368)
- [ROM class token differs from runtime class index — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L387)
- [Pico storage durability depends on VM transaction state — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L405)
- [JC305 CardException was reported as ISOException through RMI — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L422)
- [CryptoException reason was not preserved for Java RMI throws — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L449)
- [Fresh host VPCD card-manager state looked preinstalled — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L469)
- [Clearing the first persistent array field broke other aliases — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L492)
- [Security transient-key follow-up rows are unsupported-feature fallout — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L517)
- [SDK JCRMI client SELECT with Le=7F — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L540)
- [RMI exceptions escaped nested invocation boundary — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L563)
- [JCSystem VPCD regression findings — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L578)
- [Java Card RMI array signature and temporary allocation — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L589)
- [Duplicate deploy CAP after interrupted or reused area — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L606)
- [Persistent primitive GETFIELD skipped restored cache — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L623)
- [Chained API areas in parallel workers may block during SmartcardIO — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L646)
- [Current three-board PC/SC access blocked by stale installed helper — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L688)
- [Multi-device BOOTSEL entry and serial-targeted flash — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L705)
- [Reported BOOTSEL boards enumerate as application-mode RP2350s — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L731)
- [PC/SC ACL helper rejects multiple CHOUPI readers — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L750)
- [Host VPCD package lookup must include ROM-mask packages — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L769)
- [Dynamic applet virtual method token translated twice — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L793)
- [Preserve CAP2 image while placing two JFS2 slots — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L810)
- [BOOTSEL picotool transfer disconnect after 100% — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L832)
- [CAP3 package slots overlap RP2350 filesystem snapshots — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L855)
- [Imported interface resolution can contaminate later applet interface lookups — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L879)
- [PC VPCD storage shim is single-package — 2026-10-21 (updated 2026-10-25)](archive/2026-10-29-pre-compaction/QKB.md#L900)
- [VPCD SmartcardIO reader absent; bound PC/SC probes — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L919)
- [Util same-array overlapping copy — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L937)
- [JC305 OwnerPIN and PIN dispatch used embedded tokens — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L953)
- [ROM-to-dynamic virtual override needs inverse CAP token mapping — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L967)
- [Deploy Core IO_ERROR result was a client timeout, not a bridge defect — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L988)
- [CAP INSTALL and SELECT must resolve the package by the request/instance AID — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L1018)
- [Deploy API runner can report partial CAP install as success — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L1037)
- [GP DELETE can return 6581 after API applets write persistent state — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L1052)
- [GPPro package deletion requires removing its applet instance first — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1064)
- [Runtime-exception root failure test pulls full VM dependencies — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1078)
- [Persistent static reference could commit before GC root registration — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1092)
- [Exception object inserted before its GC root could be allocated — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L1108)
- [Allocation-failure test optimized away an unused JCVMArray copy — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1123)
- [JCSystem.isAppletActive must include registered install/select callbacks — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1135)
- [JCSystem.getAID exposed an unregistered instance during install — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1154)
- [Pico reader disappears from PC/SC during second applet APDU — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1172)
- [CAP installation metadata can outlive a failed filesystem commit — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1200)
- [CAP Method component size was decoded in host byte order — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1232)
- [USB-visible Pico omitted from PC/SC reader list — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1255)
- [Pico PC/SC lifecycle verified after CDC/BOOTSEL flash cycle — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L1271)
- [ROMmask source edits skipped by build cache — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L1291)
- [Native CryptoException dispatch and RSA NoPad range check — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L1305)
- [Post-SELECT PC/SC timeout repeated after 2026-10-19 reflash](archive/2026-10-29-pre-compaction/QKB.md#L1330)
- [Post-SELECT PC/SC timeout repeated after successful reflash — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1344)
- [PC/SC stops sending APDUs after SELECT — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1360)
- [Direct Pico-to-VPCD bridge requires exclusive USB interface access — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1380)
- [PC/SC Virtual PCD was listed before its endpoint was ready — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L1394)
- [JCSystem shareable lookup requires a context-switch bridge — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1413)
- [JC305 SensitiveArrays invocation used the wrong embedded method token — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1431)
- [SensitiveArrays is intentionally unsupported when capability is false — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1449)
- [JCSystem.makeGlobalArray implementation — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1460)
- [JCSystem.isAppletActive always threw NotYetImplemented — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1499)
- [JCSystem object-deletion request silently succeeded while unsupported — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L1515)
- [Restricted-context USB access — complete fail-fast coverage — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1532)
- [USB passthrough and Pico visibility](archive/2026-10-29-pre-compaction/QKB.md#L1565)
- [PC/SC permission after USB re-enumeration](archive/2026-10-29-pre-compaction/QKB.md#L1576)
- [`pcsc_scan` runaway CPU / VM saturation](archive/2026-10-29-pre-compaction/QKB.md#L1592)
- [Physical JCAlgTest static SHA-256 repeat failure](archive/2026-10-29-pre-compaction/QKB.md#L1604)
- [Host/sandbox PC/SC context split — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1622)
- [Fail fast before hardware commands in the restricted context — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1647)
- [JCSystem available-memory reporting — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1666)
- [Physical SHA-256 static repeat failure — 2026-10-17 update](archive/2026-10-29-pre-compaction/QKB.md#L1693)
- [Intermittent reader recovery after MCU reboot — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1730)
- [Static SHA repeat failure mechanism — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1751)
- [Resolution: static SHA repeat heap exhaustion — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1803)
- [Interpret JCAlgTest follow-on `6F00` after unsupported prepare — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1847)
- [Correction: first residual static `6F00` is supported `setSeed` — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1870)
- [Fix supported RandomData.setSeed static failure — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1895)
- [Isolate first DES static prepare failure — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1934)
- [Fix JC305 Cipher virtual-token mismatch in static workload — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L1964)
- [JSR saved-PC storage and remaining static-suite OOM — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2005)
- [Physical ALG_PERFORMANCE_STATIC heap retention — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2046)
- [Supported sweep heap exhaustion — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2103)
- [InitializedMessageDigest.OneShot unsupported factory — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2139)
- [Runtime CryptoException roots and physical follow-up — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2179)
- [RandomData.OneShot unsupported result and support-matrix audit — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2230)
- [OwnerPINx dispatch and inherited-field resolution — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2272)
- [JCAlgTest VPCD startup delay before support queries — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2307)
- [Host-access invocation latch — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2334)
- [Heap reclamation design constraints — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2350)
- [Heap-root inventory after static-cache implementation — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2386)
- [Canonical roots for serialized static references — 2026-10-17](archive/2026-10-29-pre-compaction/QKB.md#L2422)
- [Root and typed-edge visitor API — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2443)
- [Mark-only typed heap traversal — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2466)
- [Runtime CryptoException handles are heap roots — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2491)
- [Preserve top-level reference return values as typed roots — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2511)
- [Opt-in post-APDU heap trace — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2530)
- [Persistent reference-array child identity — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2632)
- [Persistent instance fields sharing an array — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2652)
- [Array `checkcast` type-code mapping — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2670)
- [VM-generated Java runtime exception dispatch — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2706)
- [Root java.lang.Object allocation terminated the emulator — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2778)
- [VM-raised APDUException bypassed Java catch handlers — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2801)
- [Invalid transient-array clear event raised the wrong exception — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2822)
- [Heap reclamation must preserve Java Card handles — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2851)
- [JC305 DESKey interface token must be remapped — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2880)
- [Bound serial configuration when Pico USB is re-enumerating — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2904)
- [Physical JCAlgTest static run — CryptoException catch failure — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2921)
- [Correction — ROM and imported exception class views — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2949)
- [PC/SC reader disappears during physical static run — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L2977)
- [Interpreter frames were left active after APDU exceptions — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L3002)
- [Collector scratch vectors exhausted RP2350 heap — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L3030)
- [PC/SC daemon hangs while Pico USB device remains present — 2026-10-18](archive/2026-10-29-pre-compaction/QKB.md#L3073)
- [D92 PC/SC APDU path stalled after second-package load attempt — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L3095)
- [Host VPCD timed out while updating a 20 KB persistent byte array — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L3130)
- [VM lost Pico USB passthrough during final transaction-capacity flash — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L3150)
- [Pico power-up check sees USB device but no PC/SC reader — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3168)
- [JC305 AID virtual-method token mismatch — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3223)
- [Unknown AID SELECT dispatched to active applet — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3244)
- [Pico PC/SC lists the card but Java APDU transmit stalls — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3260)
- [Physical power-up check confirms registry but APDU transmit is not delivered — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3284)
- [Transient array allocation failure terminated the VM — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3329)
- [Persistent array restoration allocation used `noexcept` constructor — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3348)
- [Persistent primitive reads leaked temporary buffers — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3363)
- [Persistent JC_Instance construction could leak its temporary tag buffer — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3379)
- [JCVMArray copy assignment lost its destination on allocation failure — 2026-10-19](archive/2026-10-29-pre-compaction/QKB.md#L3393)
- [Physical APDU reaches applet, then stalls during post-APDU GC — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3408)
- [RP2350 BOOTSEL reinsertion detection — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3447)
- [Runtime CAP loader accepts only the configured JCAlgTest package AID — 2026-10-20](archive/2026-10-29-pre-compaction/QKB.md#L3463)
- [Physical deploy Core lifecycle and cleanup results — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3509)
- [DELETE cleanup rejects CAP class indexes above 255 — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3545)
- [16-bit applet class indexes need distinct persistence tags — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3566)
- [`instanceof` class resolution can fail on an invalid alternate interface token — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3594)
- [JC305 Security Key interface method order mismatch — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3620)
- [Official API client reader index includes empty virtual terminals — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3654)
- [JavaCardX CAP static slots exceed the legacy tag width — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3674)
- [BOOTSEL devices are not PC/SC readers — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3694)
- [Failed scenario INSTALL can leave a partially loaded CAP — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3711)
- [Core deploy repeat reports unexpected `Applet.deselect()` value — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3732)
- [Third Pico is USB-visible but intermittently absent from PC/SC — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3759)
- [Stale dynamic package bitmap can hide a missing CAP — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3779)
- [CAP metadata update could destroy the only valid registry header — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3807)
- [CAP applet registration and filesystem install commit need reconciliation — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3831)
- [VPCD test process must exit cleanly to save its image — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3872)
- [VPCD applet harness summary count was hard-coded — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3887)
- [Parallel deploy startup can race Java reader enumeration — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3901)
- [Three-reader PC/SC access restored — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3924)
- [API runner must reject partial output after abnormal client exit — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3935)
- [Physical parallel deploy run — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3949)
- [API runner `--reader-name` did not set Java reader index — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3966)
- [Canonical JC305 extension package AIDs did not match CHOUPI ROM AIDs — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L3980)
- [Three-reader deploy batch and install rollback — 2026-10-21](archive/2026-10-29-pre-compaction/QKB.md#L4003)
- [Host VPCD harness loopback bind denied in restricted execution — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4024)
- [JC305 MessageDigest constructor token differs from embedded ROM export — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4030)
- [VPCD deploy applet selection uses area-specific AIDs — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4050)
- [RMI export-capacity case includes existing applet references — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4062)
- [JC305 RMI names come from CAP remote metadata, not Debug — corrected 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4074)
- [Fresh-process VPCD install recovery needs the assigned package ID — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4102)
- [Restored applets lose transient RMI exports — viable workaround 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4115)
- [RMI interface-form SELECT P2 rejected before applet dispatch — fixed 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4129)
- [VPCD local socket denied in restricted execution — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4139)
- [JCDK 3.0.5 direct CAP output omits int RMI signatures — resolved 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4151)
- [RMI exported object retained through collection — validated 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4196)
- [VPCD process-restart persistence stress can collect after restore — validated 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4211)
- [GPPro card-manager lifecycle on PC VPCD — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4224)
- [Host VPCD emulator interrupted with Ctrl-C — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4242)
- [Host VPCD persistence test must resume the allocated package ID — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4255)
- [Empty transaction abort invalidated unrelated persistent caches — resolved 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4270)
- [Persistent abort cache stayed stale after flash rollback — 2026-10-22 (resolved)](archive/2026-10-29-pre-compaction/QKB.md#L4301)
- [Nested persistent array graph fixture boundaries — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4331)
- [Host VPCD idle timeout and external-install persistence — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4347)
- [JC305 interface-token remapping corrupted embedded ROM API calls — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4374)
- [Current-source Crypto API deploy outcomes and constructor mapping — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4400)
- [Host USB helper rejected by restricted execution context — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4427)
- [JC305 Signature virtual dispatch used the wrong embedded method — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4438)
- [JC305 CBC Cipher update must buffer partial blocks — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4446)
- [Physical JC305 Security follow-up rows depend on unsupported transient AES keys — 2026-10-22](archive/2026-10-29-pre-compaction/QKB.md#L4481)
- [Transitive custom exception RMI mapping — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4497)
- [Preserve SystemException reasons in Java Card RMI — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4512)
- [Service Dispatcher returns SW=6400 only on one physical token — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4532)
- [Parallel deploy runner empty-area fallback — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4541)
- [Same-image reflash leaves the token 3 Service failures — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4547)
- [Unrestricted API 3.0.5 suite exposes matching 6F00 failures on two tokens — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4555)
- [JCAlgTest 1.8.2 cannot pass initialization in capability profiles — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4563)
- [Minimal JCAlgTest 6F00 caused by stale JC305 API class-token mappings — 2026-10-23](archive/2026-10-29-pre-compaction/QKB.md#L4573)
- [JCAlgTest base/current support profiles on Pico 2 — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4582)
- [Selectable capability profile physical acceptance — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4620)
- [Virtual PCD VPCD endpoint refused emulator connection — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4657)
- [Symmetric crypto profile has no physical test target visible — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4675)
- [Symmetric crypto emulator vector coverage — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4690)
- [ISO9797-M1 cipher decryption retains zero padding — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4707)
- [JC305 RSA key calls use shifted interface method tokens — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4731)
- [Pico USB-visible but absent from PC/SC after asymmetric-profile flash — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4755)
- [RSA key source leaked into profiles that disable RSA — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4774)
- [Stale persistent child tags block package deletion and reinstall — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4789)
- [BOOTSEL ROM drops Pico runtime serial — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4813)
- [JCSystem persistent array body hidden by a resurrected overlay tombstone — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4828)
- [Firmware reflash does not make a used card empty — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4853)
- [Multi-board BOOTSEL selection and clean-card verification — 2026-10-24](archive/2026-10-29-pre-compaction/QKB.md#L4875)
- [Pico leaves BOOTSEL but runtime CDC is not passed through — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4907)
- [`/deploy` runner resolves output paths from its own directory — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4931)
- [JC305 `checkcast` class/interface token collision — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4947)
- [VPCD registry restore failure must stop card service — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4973)
- [Pico reader ACL refresh and GPPro timeout failover — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L4994)
- [Capability-profile physical Core/Security/Crypto follow-up — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5016)
- [Physical JCSystem BUFFER_FULL probe can leave the runtime wedged — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5043)
- [JCSystem VPCD runner must clear its registry sidecar for a fresh image — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5065)
- [JCSystem applet SELECT can fail on a dangling persistent array alias — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5079)
- [Three LEDs active while PC/SC exposes only one Pico — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5109)
- [Physical GET STATUS stalls on exact-packet CCID response — fixed 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5149)
- [Stale VPCD flash image prevents clean RMI client startup — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5194)
- [Reused PC VPCD flash copy retained stale registry sidecar — fixed 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5210)
- [D92 JCAlgTest support sweep loses PC/SC transaction on MD5 RSA OAEP — 2026-10-25](archive/2026-10-29-pre-compaction/QKB.md#L5226)
- [Asymmetric signature profile currently adds a Java ROM-mask provider — unresolved architecture violation (2026-10-25)](archive/2026-10-29-pre-compaction/QKB.md#L5253)
- [Native Java Card API dispatch can allocate API objects directly — design finding (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5274)
- [MessageDigest native buffer must be transient — fixed (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5300)
- [Native method override manifests need full Java Card descriptors — fixed (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5309)
- [Physical SHA-256 validation after native MessageDigest dispatch (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5315)
- [Symmetric Signature MAC path migrated to native API dispatch (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5323)
- [PC emulator can remain stale after target builds (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5349)
- [CCID response buffer must fit full APDU response (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5364)
- [JCAlgTest static workloads exceeded the applet-call time guard (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5385)
- [RSA-PSS known-vector harness must distinguish explicit salt parameters (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5393)
- [JC305 ECPrivateKey interface methods need token remapping (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5400)
- [JC305 ECPublicKey interface methods also need token remapping (2026-10-26)](archive/2026-10-29-pre-compaction/QKB.md#L5422)
- [JC305 KeyPair getter token order and qualified constructor JNI name (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5440)
- [EC subgroup order can exceed the field bit width (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5459)
- [Native RSA-CRT KeyPair keys were not usable by private Cipher/Signature (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5475)
- [CAP component framing accepted truncated or oversized lengths (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5518)
- [CAP component payload tables need internal count bounds (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5536)
- [Compact Descriptor records require seven-byte field entries (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5554)
- [CAP 2.1 Class records are variable-length and ordered by kind (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5575)
- [Descriptor `this_class_ref` must point to a Class record start (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5596)
- [CAP Descriptor type nibble grammar and padding (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5614)
- [CAP Descriptor type-offset base compatibility (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5636)
- [VPCD emulator can connect while PC/SC still reports no card (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5658)
- [ROM-mask CAP payloads are prelinked and fail standard Descriptor validation (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5675)
- [VPCD PC/SC service startup race (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5682)
- [VPCD install-recovery tests must resume from persisted registry (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5689)
- [RSA-CRT result verification needs a matching public exponent (2026-10-27)](archive/2026-10-29-pre-compaction/QKB.md#L5696)

### Enumerate registered roots for all installed applet instances — 2026-10-30

- The firmware/PC inventory path now walks the package instance registry,
  resolves each instance's stable owner ID, and visits all registered root tags
  in that owner namespace. Missing package metadata, instance records, owner
  mappings, or root walks fail the inventory closed. This replaces the stale
  active-applet-only limitation; applet instances share the active heap used
  by the inventory tracer.
- PC VPCD multipackage coverage passed with three installed applet roots both
  before and after emulator restart (`applet_roots=3`, `trace_complete=1`,
  zero invalid references/edge failures). Logs:
  `artifacts/logs/host-vpcd-multipackage-apdu-20261030T040017Z.log`
  (SHA-256 `9f397c883b02706990a84e532fc1ffe02a706ab89eba1debb1b7d4412e8016ab`)
  and `artifacts/logs/host-vpcd-multipackage-restart-20261030T040017Z.log`
  (SHA-256 `177355201566029f929b9fbe945c325799573a10a57ba66d0aeef3d1d5416995`).
  Suite summary: `TEST:SUMMARY pass=25 fail=0`. Physical multi-instance
  installation/root restore has not been tested. Other live contexts and
  native-held roots remain outside this enumeration; persistent record deletion
  remains disabled.

### Physical SHA-256 doFinal lost its persistent digest instance at APDU GC — fixed 2026-10-30

- On Pico 2 `2BED26ED20556535`, the unchanged installed JCAlgTest 1.8.2 applet
  previously selected normally; GETVERSION and JCSystem-info returned `9000`,
  SHA-256 setup returned `AA9000`, but doFinal returned `FF01`. Serial tracing
  showed the factory returned digest handle `0036`, the next APDU used receiver
  `0036`, then the collector reclaimed that persistent instance at APDU end.
- Cause: strict persistent-field tracing reads the serialized flash snapshot.
  A persistent object's loaded in-memory field cache can contain a newer
  reference than that snapshot; the collector was ignoring this live edge.
  The `FF01` was therefore consistent with a stale/dangling digest handle on
  the following APDU. This was not a SHA-256 arithmetic failure.
- `Heap::visitChildReferences` now first keeps strict serialized validation,
  then visits cached child handles only when the persistent field cache is
  loaded and current for the active rollback generation. A focused
  `persistent_transaction` regression proves the cached child survives and
  unrelated garbage is reclaimed. VPCD persistence passes install/commit 3/3
  and restore/GC 38/38 after process restart. On the normal symm_crypto ARM
  image, physical board 2BED was flash-verified, its finite monitor passed 18
  repeated runtime summaries, and the unchanged JCAlgTest SHA-256 prepare and
  doFinal APDUs both returned `AA9000` in three consecutive probes.
- Evidence: ARM image SHA-256
  `18a7a230adf2e1719c26e1f79ccd48f60e93ae108c4b989ade4a2a89d943604e`;
  cycle log `artifacts/logs/cycle-choupi-20261030T105252Z.log`; physical APDU
  log `artifacts/logs/sha256-dofinal-loaded-cache-2BED-20261030.log`; VPCD
  logs `artifacts/logs/vpcd-persistence-commit-20261030T105520Z.log` and
  `artifacts/logs/vpcd-persistence-restore-20261030T105520Z.log`. A recurring
  `WATCHDOG_RECOVERY ... cause=other` startup marker remains separate; all
  finite runtime checks passed.

### Installed-instance root enumeration must match the registry — fixed 2026-10-30

- A complete cursor scan could previously return zero registered roots for an
  installed applet instance and still mark the walk complete. The collector
  now validates applet and instance AID lengths, resolves the instance owner,
  and requires exactly one root whose owner, package ID, and class index match
  the package-instance registry entry. Missing, duplicate, or mismatched roots
  fail the root walk, so `collectUnreachableFrom` skips reclaiming heap slots.
- The multipackage VPCD suite passed 25/25 before and after process restart;
  all three installed roots were discovered with a complete trace and no invalid
  references or edge failures. ARM, RISC-V, and PC builds passed. On physical
  2BED `2BED26ED20556535`, a full ARM image cycle passed finite startup checks;
  after refreshing the PC/SC ACL helper, JCAlgTest SELECT, GETVERSION, and
  JCSystem-info each returned `9000`. No persistent filesystem records were
  deleted. Negative registry/root-corruption injection is still untested.

### JCArray host collector harness needs the OS FFI declarations and strict-reader stubs — fixed 2026-10-30

- `heap.cpp` now includes the Rust OS `ffi.h` and references strict persistent
  field/tag helpers. The standalone JCArray harness was picking the older
  `tools/tests/short_apdu/ffi.h` first and its filesystem stub lacked these
  declarations/definitions. Put `choupi/os/src` first in the target include path
  and extend its filesystem fixture with the new FFI and strict-reader stubs.
- `./tools/test-jcarray` now passes the existing heap lifecycle/GC suite plus
  two fail-closed checks: a root enumerator that returns false after yielding a
  partial set, and a reachable reference array with an invalid child handle.
  Both mark summaries incomplete and reclaim zero heap handles. This does not
  exercise malformed serialized fields/aliases or reboot recovery.

### Strict persistent child-read failure must stop heap reclamation — covered 2026-10-30

- The JCArray host collector fixture can mark an array persistent and inject a
  `SecurityException` from `getPersistentField_ReferenceStrict` while scanning
  its child slots. `./tools/test-jcarray` confirms the trace reports one
  `edge_load_failure`, returns incomplete, and reclaims zero handles, including
  an unrelated unrooted array. This verifies the collector's safety gate for a
  persistent read exception, but does not fuzz malformed record bytes or alias
  cycles and does not validate post-reboot recovery.

### Persistent array-alias headers reject truncated encodings — covered 2026-10-30

- The FlashMemory transaction fixture now exercises the actual
  `getPersistentField_Type` decoder against stored alias records. It rejects a
  one-byte array-alias header and a declared target-tag length that does not
  match the record payload, and rejects a correctly framed alias whose target
  is a scalar record. The full `./tools/test-persistent-transaction` suite
  passes. The fixture now implements `fs_read_inplace` and minimal unused
  Context hooks required when retaining the actual decoder section in the host
  link.
- These checks cover malformed array-alias framing only. They do not validate
  malformed object-alias chains through `getPersistentField_ReferenceStrict`,
  collector behavior on an alias-corrupt record, or recovery after restart.

### Physical static workload disconnect is not yet explained — updated 2026-10-30

- On normal `symm_crypto` firmware, two official JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` runs on 8E ended during the measured DES CBC PKCS5
  `Cipher_init()` sequence with `SCARD_E_NOT_TRANSACTED`, then
  `SCARD_E_READER_UNAVAILABLE`. The second run's reader later disappeared from
  PC/SC even though USB enumeration still showed devices; the ACL helper
  restored a reader earlier in the investigation. Preserve both reports at
  `artifacts/results/jcalgtest/performance-static-post-gc-fix-8E-20261030/`.
- A GC-diagnostic image on separate physical 2BED passed that same DES CBC
  PKCS5 `Cipher_init()` case, then DES ECB and many 3DES cases. It was still
  responding normally when the 900-second outer timeout ended during 3DES CBC
  NOPAD `Cipher_init()`; this is partial coverage only. Debug serial output
  reports passing runtime summaries. GC snapshots stabilized at 148 live / 198
  allocated arrays and 36 live / 39 allocated instances, with zero invalid
  refs, edge failures, or root failures. This makes deterministic GC/reference
  corruption at the earlier failure location less likely, but does not
  distinguish a timing-sensitive firmware issue from USB/PCSC transport loss.
- Diagnostic logs and partial report:
  `artifacts/logs/heap-gc-static-debug-2BED-20261030.log`,
  `artifacts/logs/heap-gc-static-debug-2BED-continued-20261030.log`, and
  `artifacts/results/jcalgtest/performance-static-gc-debug-2BED-20261030/`.
  Standard firmware was restored and passed both build wrappers, flash
  verification, an 18-second monitor, and PC/SC discovery (2BED).
- Next probe: rerun the exact normal image on one identified board with
  concurrent serial capture and host USB event capture, or isolate the failing
  method using client filters if available. Do not mark the official static
  suite as passing until its complete report succeeds.
- Follow-up on normal image, separate identified Pico 2 `2BED26ED20556535`:
  the official run continued for the full 3,600-second wrapper limit and
  completed DES/AES Cipher coverage (126 DES and 108 AES method entries), then
  timed out during RSA/signature cases. It produced 4,214 CSV lines / 937
  method entries and contains no `SCARD_E_*`. Concurrent 2.5 MB serial capture
  has 3,575 passing summaries and no `TEST:FAIL`, `FAULT`, or
  `WATCHDOG_RECOVERY`; concurrent udev capture has no USB add/remove event.
  PC/SC still listed 2BED after the run. This does not prove the entire suite
  passes but shows the DES CBC PKCS5 initialization failure did not reproduce
  on 2BED after about an hour of normal-image stress. The original disconnect
  remains board/state-specific or intermittent; root cause unproven.
- Separate observation: this run logged 72 `FF01` responses in DES MAC
  `Signature_*` static measurements, which JCAlgTest records as
  `CANT_BE_MEASURED`. These were applet execution errors, not reader loss; do
  not conflate them with the missing-algorithm `NO_SUCH_ALGORITHM` outcomes.
  Investigate only if these MAC signatures are intended as supported in the
  selected profile or the upstream suite expects a clean unsupported result.
- Evidence:
  `artifacts/results/jcalgtest/performance-static-normal-full-2BED-20261030/`
  (client log SHA-256
  `5b054c044d7c5a7c511906bc1498100473886aa9ce3cb6a69f310fef013c2e45`, CSV
  SHA-256 `488708254d7189dc4bdde7b3c31eea04489537a5dfaff044a56bfb8115570e46`);
  serial log SHA-256
  `0cc602228990f9919375449a9162b4aa541e37db44a4ed870322c29c69b18780`; empty
  udev log SHA-256
  `a75697edb29c2c13f19e67b1fcaa15b36e16e39fd08f9ed40f33e7f029ae5b5a`.

- **DES-MAC static errors isolated in VPCD — 2026-10-30:** Replayed the
  unchanged upstream JCAlgTest 1.8.2 `Signature` static workload with DES MAC4
  ISO9797 M1, 256-byte input, and 50 sign calls. The initial emulator run
  timed out. The applet stores its `Signature` instance in a persistent field;
  the generic completion path cleared unused message/recovery arrays and reset
  an IV that the MAC backend never mutates. Those operations caused persistent
  array reads/writes on every sign. A specialized MAC completion path now
  clears only buffered `update()` bytes, and does no buffer work when none are
  pending; it also leaves the unchanged IV alone. The same upstream prep and
  measurement APDUs now return `AA9000`; measurement elapsed 47.425 s and
  prepare elapsed 24.686 s. The diagnostic log SHA-256 is
  `4be4132e3443325f44eb9ae55ba71c6f4af14e6a8ae006d8b169a475c24682cb` at
  `artifacts/logs/jcalgtest-desmac-256byte-50repeat-optimized2-vpcd-20261030.log`.
  The complete symmetric VPCD set passes 73 vectors, including repeated sign
  and buffered-update-plus-direct-input checks. Viable fix: retain separate
  MAC completion cleanup and avoid reading or writing persistent engine arrays
  unless buffered data actually exists. This establishes host VPCD behavior
  only; repeat the focused test on a physical Pico and rerun the relevant
  official static Signature section before closing the physical issue. Do not
  conflate this applet execution issue with the intermittent reader disconnect
  described above.

- Final follow-up validation: the latest version avoids persistent IV reads by
  supplying the zero IV locally (the MAC implementation does not mutate it).
  PC build plus ARM and RISC-V max-profile wrapper builds pass. The VPCD suite
  passes 75 APDU checks / 73 crypto vectors through the approved host-network
  path; restricted execution fails with `Operation not permitted` while
  opening localhost sockets. Updated external ARM UF2 SHA-256:
  `e013ff4f87331dc99980d9fe9edf10b0a1579838fa5b3feefb166beb93632340`.
  No physical Pico was flashed, so physical MAC/static validation remains open.

### Post-APDU collector has one live interpreter context — audited 2026-10-30

- `Interpretor` currently pushes one `Context` in each constructor; the hardware
  APDU entry point supplies the selected external context. Collection is called
  after interpreter execution, not while Java frames or native C++ call-local
  references are active. Its callback invokes `Context::visitRootReferences`
  and `Heap::visitRootReferences`, which visit remote-object roots, applet and
  APDU lifecycle references, APDU buffer mirrors, sensitive results, stack
  reference returns, runtime exception roots, and cached static references.
- The missing root inventory was therefore per-installed-applet roots, now
  enumerated for every registry instance, rather than several simultaneous
  interpreter contexts. `./tools/test-rmi-registry` passes
  `lifecycle_roots=6`, `sensitive_result_cross_context=1`, and session cleanup.
  Revisit this audit if the interpreter gains concurrent contexts or another
  persistent native owner. No persistent records are deleted by this change.

### Stale external profile package — refreshed 2026-10-30

- The old ignored `build/external-test/pico2-jcvm-windows-test.zip` and loose
  asymmetric expected CSV were stale. Viable refresh: run
  `./tools/package-asymm-symm-windows-test`. The packager verifies the pinned
  upstream CAP hash, runs ARM and RISC-V wrappers for both profiles, copies
  each generated ARM UF2 and expected CSV before changing profile, and writes
  per-file SHA-256s into the archive. It validates ZIP integrity and all
  checksums. Latest ZIP SHA-256:
  `26a1d6e6d34033c44a22915c8a41c644917de85fd04c5d6d3598606889708be3`.
- The generated package contains 46 `symm_crypto` and 145 `asymm_crypto`
  declared positives. Use the packaged expected CSVs; the loose ignored
  `build/external-test/expected-support-asymm_crypto.csv` remains stale.
  Windows has not yet run the included instructions. The README's comparator
  command must include the required second positional jCardSim reference CSV.
- The asymmetric physical comparison is known to be 143/145 because the
  unchanged upstream JCAlgTest CAP labels RSA/RSA-CRT 3072 key-pair probes as
  3072 but sends 4096; independent physical 3072 generation vectors pass.
  Preserve this caveat; do not alter the CAP or expected manifest to hide it.

### Physical API 3.0.5 RSA NOPAD rows pass — 2026-10-29

- The unchanged Crypto CAP completed all 466 calls on Pico 2
  `2BED26ED20556535` with `asymm_crypto`. All four `ALG_RSA_NOPAD` rows
  returned `OK`, including the parameterized illegal-value case; upload and
  cleanup stages both passed. The area totals were 302 `OK`, 163
  `UNSUPPORTED_PER_SPEC`, and one unrelated `EXC_CARDRUNTIME`.
- Evidence CSV:
  `artifacts/results/deploy/crypto-physical-retest-20261029/2BED-asymm-crypto.csv`
  (SHA-256 `28400be72b530b81d264dbad3dbd11f860ba3af6c5756c06dac792ee8e4d05b1`).
- The prior “physical retest pending” notes were stale. This does not make the
  broader deploy matrix clean; track its remaining findings separately.

### Current-image physical RSA NOPAD and secp192r1 KeyAgreement focus — 2026-10-30

- Ran the unchanged API 3.0.5 Crypto CAP on physical Pico 2
  `8E464F4E2BBC0A19`, using its already-installed `asymm_crypto` image and the
  exact reader name. The focused filter selected six calls: four RSA NOPAD
  cases and known-secret secp192r1 `ALG_EC_SVDP_DH` / `ALG_EC_SVDP_DHC`.
- All six calls returned `OK`; the DH/DHC cases each returned `0x0014`. Of 466
  total API calls, 460 were intentionally filtered. CAP upload and cleanup both
  passed. No image was flashed or source changed during this run.
- CSV:
  `artifacts/results/deploy/crypto-focused-current-8E-20261030.csv` (SHA-256
  `321cd73f1664deaac455729b2806b426185733a36fb3d3bc46009fd7263ecdbb`).
- This closes the current-image physical check for these six rows only. It
  does not establish a full Crypto area pass or close the remaining deploy
  regression sweep.

### Full API 3.0.5 Crypto physical runs and 8E PC/SC loss — 2026-10-30

- Full Crypto on 8E (`asymm_crypto`) uploaded successfully but aborted after
  233/466 calls. It recorded 145 `OK`, 82 `UNSUPPORTED_PER_SPEC`, one
  `EXC_CARDRUNTIME` (RSA SHA-512 PSS, reason `0x0001`), then five `CARD_ERROR`
  rows beginning with the RSA-512 SHA-1 known-signature call
  (`SCARD_E_NOT_TRANSACTED`) and subsequent “Card has been disconnected.”
  Cleanup failed because the exact 8E reader was no longer found. CSV SHA-256:
  `f55cb277860dc648441a6ab2e58bc1167693db29b880d98f260951420f634a4f`.
- The serial device and USB descriptors for 8E remained present after the
  abort. Its finite eight-second monitor returned eight `TEST:PASS` /
  `TEST:SUMMARY pass=1 fail=0` records and no `FAULT`, but `pcsc_scan -r`
  continued to show only physical board 2BED. The USB helper saw both devices.
  Root cause is not established; do not classify the five transport fallout
  rows as five independent VM failures. The 8E Crypto CAP may remain installed;
  check before the next install when PC/SC returns.
- Failover full Crypto on 2BED (`symm_crypto`) completed all 466 calls with
  138 `OK`, 304 `UNSUPPORTED_PER_SPEC`, and 24 `NO_MEMORY` /
  `SystemException.NO_RESOURCE`. Upload and cleanup passed. The resource errors
  affect multiple MAC/signature and Cipher paths; the exact allocator/store is
  not established. This is consistent with known storage pressure, but does
  not prove persistent GC is the cause. CSV SHA-256:
  `8343b601775a8fc3e8dbe126509901b137b5903464b8372d9c39d927bcbb6510`.
- A read-only post-run check on 2BED confirmed the JCAlgTest applet/package,
  GETVERSION `1.8.2_jc305`, and JCSystem-info all returned `9000`. Log:
  `artifacts/logs/pico-powerup-persistence-20261030T173744Z.log` (SHA-256
  `ba3f8aeac6c1d13d1726309b0123fbcadb6c89d6ab515822ca919e658e5f3d72`). This
  check does not measure power-cycle recovery.
- After the successful full-run cleanup, a focused repeat of only the DES-64
  and AES-192 known-answer Cipher rows again returned `NO_MEMORY` /
  `SystemException.NO_RESOURCE` (2/2); upload and cleanup passed. These errors
  reproduce outside the full sweep sequence. Firmware maps filesystem
  `OutOfFlash` status 1 to `NO_RESOURCE`, but this observation alone does not
  identify which capacity limit was reached. Focused CSV SHA-256:
  `ad53a69876f892724a8be1727fbb48c93d2ac4b726f4435bce034ead36565c8a`.

- JCSystem-info before and after this focus showed a small but internally
  consistent current persistent-memory reading: P1=01 after the full run had
  legacy short value `0x177A` (6,010 bytes); a subsequent clean P1=00
  version-only probe reported `0x1751` (5,969 bytes) in both short and extended
  fields. The 41-byte difference spans the isolated CAP install/test/cleanup
  and an extra prepare APDU; it is not attributed to any one operation. The
  positive free-byte value does not explain the exact write that returns
  `OutOfFlash`.
- Probe procedure: to stop after JCSystem-info, invoke
  `tools/probe-jc-algtest --reader <exact-reader> --jcsystem-info-p1 00
  --version-only`. Omitting `--version-only` continues into the SHA-256 static
  test sequence; one such attempt sent `OFFICIAL_SHA256_STATIC_PREPARE`
  (`AA9000`) and then ended with `SCARD_E_PROTO_MISMATCH`. The following
  version-only probe succeeded with SELECT, GETVERSION, and JCSystem-info all
  `9000`.
- A concurrent serial capture during the same two-row repeat showed the
  concrete failure: `CHOUPI:FS_WRITE_FAIL op=new tag_len=14 size=4 files=6263
  bytes=65533/65535`, followed by `PERSIST_EXCEPTION` and a caught Java Card
  `SystemException.NO_RESOURCE`. Another new 4-byte record failed at the same
  65,533-byte usage. After CAP cleanup, a safe version-only memory probe again
  reported 5,969 bytes free. The immediate cause is the active 64 KiB overlay
  limit while the CAP and its test objects are resident. Whether persistent
  GC, a larger compatible overlay format, or smaller test-time writes is the
  appropriate remedy remains open; do not enable record deletion before the
  interrupted-update/power-loss gates pass.
- Serial-traced runner CSV:
  `artifacts/results/deploy/crypto-memory-trace-2BED-20261030.csv` (SHA-256
  `ee2853a2e35f339677c9693f8e37440f70c02c5b25008918878ebfd97c8a842c`).

### Read-only power-up check does not perform a power cycle — clarified 2026-10-30

- `tools/test-pico-powerup` checks GPPro package/applet listing, applet SELECT,
  GETVERSION, and JCSystem-info. It does not reset the card or remove USB power.
  Its completion marker is now
  `PERSISTENCE:PASS registry_applet_and_jcsystem_verified power_cycle=not_verified_by_script`.
  A separate physical reboot/cold-power procedure is required before treating
  a result as power-cycle recovery evidence.
- The available physical reader was confirmed as Pico 2
  `2BED26ED20556535` on USB bus 001 device 116 / root-hub port 2. `lsusb -t`
  exposed only the root hub, `uhubctl` was not installed, and sysfs exposed
  `power/control` autosuspend only; no controllable VBUS port was identified.
  Do not substitute USB authorization detach or PC/SC reset for power loss.
- Latest read-only check passed SELECT, GETVERSION `1.8.2_jc305`, and
  JCSystem-info (`9000`). The finite serial monitor passed seven runtime
  summaries with no `TEST:FAIL` or `FAULT`. Logs:
  `artifacts/logs/pico-powerup-persistence-20261030T145210Z.log` (SHA-256
  `4708926c9e2209409d15a6217f8043a58e473be8cff3fc9a5378fa5b6b7c4840`) and
  `artifacts/logs/persistent-inventory-latest-monitor-2BED-20261030.log` (SHA-256
  `f7a73be8232621a94faa183aabd21559748a7d829c781e0462b0e4465c06561d`).

### RP2350 overlay capacity v3 resolves physical API Crypto exhaustion — 2026-10-30

- The 65,535-byte overlay cap was not required by the fixed snapshot slot. The
  index uses 8,192 ten-byte records and each 160 KiB slot reserves 4 KiB for
  its header, leaving exactly 77,824 bytes for the byte arena. Snapshot format
  v3 stores offset bit 16 in otherwise-unused `OverlayFile.flags` bit 6; v2
  snapshots remain readable and the next write publishes a v3 generation.
  File lengths stay 16-bit, so a single record still cannot exceed 65,535
  bytes; use multiple records when testing offsets beyond 64 KiB.
- `./tools/test-rp2350-fs-overlay` passes v2 migration, high-offset restore,
  compaction, restart, and seven v3 interrupted-snapshot cut points
  (`TEST:SUMMARY fail=0`). Each cut preserves the >64 KiB record and recovers
  either the old snapshot or a complete new generation. Both
  `./tools/build arm` and `./tools/build riscv` pass. Current `symm_crypto`
  cycle flashed/verified ARM UF2 on Pico 2 `2BED26ED20556535`; its 18-second
  monitor had 18 `TEST:SUMMARY pass=1 fail=0` records and one known stale-field
  watchdog marker (`cause=other`). UF2 SHA-256:
  `a4790d06c5691ac5f222bc51fb47e207268e7b805bd749958940de82d130f977`.
- On that same physical board, the unchanged API 3.0.5 Crypto CAP passed its
  previously failing DES-64 and AES-192 known-answer calls (both `OK`); the
  focused filter also selected DES CBC M2, which passed. Upload and cleanup
  succeeded. Full Crypto then completed all 466 calls with 161 `OK`, 305
  `UNSUPPORTED_PER_SPEC`, zero other statuses, and successful cleanup. This
  supersedes the earlier 2BED full-run 24 `NO_RESOURCE` result for the new
  image; it does not claim persistent GC/reclamation is implemented.
- Concurrent serial capture of the full run contained 15,582 lines and no
  `FS_WRITE_FAIL`, `PERSIST_EXCEPTION`, `TEST:FAIL`, or `FAULT`. Its 900-second
  monitor was intentionally terminated after the client completed. The
  post-run read-only GP registry check timed out three times; subsequent
  `pcsc_scan -r` listed only the two VPCD readers. Therefore APDU-level
  post-run registry verification and physical power-cycle recovery were not
  confirmed, even though API CAP cleanup reported success.
- Evidence: focused CSV SHA-256
  `69bf72a96047fbd8a8bf380b48face54559d28477fa4a5368e757943e2f5fca4`; full
  Crypto CSV SHA-256
  `3cb62be1b5cd8539fcddbd450a900f1f83236fa375eb0bf20cec79c9ca2efa0e`;
  serial capture SHA-256
  `5561e9a86014df6dbaf00066b7df9c6ac190b7be10836ec911c0c26a9dd80df4`;
  cycle log SHA-256
  `dfe46433fb498099a60450d88811d4e60f323e10453e7a1941b03104f6a4f023`.
- For a repeat on this host, use the exact reader name with
  `--pcsc-lib /usr/lib/x86_64-linux-gnu/libpcsclite.so.1`; the authorized
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper may need to be rerun after
  firmware reboot. Do not treat SmartcardIO's failure to resolve a stale reader
  name as a VM failure; refresh `pcsc_scan -r` first.

### Pico USB enumeration without PC/SC reader registration — 2026-10-30

- After the full Crypto run and successful CAP cleanup, `lsusb -d 2e8a:0009`
  still found two normal-mode CHOUPI devices. Their USB descriptors include
  the CCID interface and identify `8E464F4E2BBC0A19` (bus 001 device 015) and
  `2BED26ED20556535` (bus 001 device 017). The authorized reader helper granted
  access to both nodes, but `pcsc_scan -r` listed only `Virtual PCD 00 00` and
  `Virtual PCD 00 01`.
- Eight-second serial monitors on each board produced eight
  `TEST:SUMMARY pass=1 fail=0` records and no `TEST:FAIL` or `FAULT`. The 2BED
  last CCID debug counters showed `xfr=1200`, `tasks=1945`; thus its firmware
  continued running after the deploy client/cleanup, even though PC/SC no
  longer registered it. This is a host reader-registration/transport issue
  until proven otherwise; do not classify it as a firmware crash or keep
  reflashing solely because PC/SC enumeration is missing.
- `pcscd.socket` is active/listening, while `pcscd.service` was inactive after
  auto-exit. That lifecycle is normal for the socket-activated service and is
  not by itself the cause. Journal access did not expose daemon messages. No
  root cause or remediation was established; physical PC/SC testing cannot
  continue until a board reader is registered.
- Follow-up diagnostic: both Pico devices still enumerate and their interface
  descriptors show CDC ACM plus CCID interface 2. The installed
  `/etc/libccid_Info.plist` has an exact `0x2E8A:0x0009` entry named `Pico 2
  CHOUPI CCID`; the actual USB product string is `Pico2 JCVM CCID`. The
  authorized helper grants `pcscd` read/write ACLs and restarts the service.
  `pcscd` is running when queried, yet PC/SC still lists only VPCD. Current
  user cannot read system journal/syslog diagnostics, so this rules out a
  missing VID/PID mapping but not CCID descriptor/IFD initialization failure.
  Do not change firmware or driver files without daemon-side error evidence.
- Recheck at 2026-10-30 21:09 UTC: `lsusb` again found serials
  `8E464F4E2BBC0A19` and `2BED26ED20556535`, both with a CCID 1.10 descriptor,
  T=1 protocol and interface 2. `PICO2_HOST_USB_ACCESS=1 ./tools/doctor` passed
  all build prerequisites. The bounded `timeout 8s pcsc_scan -r` still listed
  only VPCD. `pcscd.service` had exited successfully after its normal
  socket-activated auto-exit; journal entries were unavailable to this user.
  USB enumeration alone still does not permit physical APDU or power-cycle
  tests.
- Logs: `artifacts/logs/usb-visible-no-pcsc-8E-20261030.log` (SHA-256
  `b6b08a7ce585cd995c182926a629b494042f01a9d451d0aaea94dd156afb3a82`) and
  `artifacts/logs/usb-visible-no-pcsc-2BED-20261030.log` (SHA-256
  `0d1dfcbe80fb5f6e2f99d25198ee18a3279b1531bfac9d8ade5f2e3cb54b02ca`).

### VPCD localhost sockets need host networking access in this VM — 2026-10-30

- The non-physical VPCD applet harness failed in the restricted execution
  context with `TEST:FAIL [Errno 1] Operation not permitted` when it attempted
  to open its localhost socket. Re-running the already-built VPCD test through
  the approved elevated execution path succeeded. This is a sandbox socket
  permission failure, not a Pico USB/PCSC problem. For VPCD-only harnesses that
  create localhost listeners, use host networking access on the test command;
  do not route them through the physical USB helper.
- At the same check, the authorized USB access helper granted two device nodes,
  but `pcsc_scan -r` returned only `Virtual PCD 00 00` and `Virtual PCD 00 01`.
  No physical smart-card validation was available.

### AES-CTR native stream state — 2026-10-30

- Java Card 3.0.5 `Cipher.ALG_AES_CTR` is a stream mode and `update()` must
  emit all input bytes. The native provider uses Mbed TLS CTR with the mutable
  16-byte counter in `Cipher.iv`, cached keystream block in the otherwise
  unused `Cipher.modulus` array, and byte offset in `modulusLength`. These
  existing fields avoid changing the ROM class layout. `doFinal()` resets the
  counter/stream state to the initialization values. The separately selectable
  `symm_crypto_ctr` profile enables the `ALG_AES_CTR` path and the paired
  `CIPHER_AES_CTR PAD_NOPAD` path; the existing profiles keep their previous
  output sets. VPCD passes the NIST SP 800-38A AES-128 CTR vector through both
  factory paths, in one-shot mode and split after 19 bytes for both encryption
  and decryption (8/8). Its implementation is compiled from the separate
  `choupi/src/native_aes_ctr.cpp` module only when either the single-algorithm
  or paired-factory AES-CTR capability is selected. Physical Pico validation
  remains open; AES-CTR is not
  present in the supplied JCOP4 P71 1.7.9 matrix and must not be counted as
  matrix parity.

### Minimal-profile native module selection and compile guards — resolved 2026-10-30

- The ARM target had its own `CHOUPI_VM_SOURCES` list, separate from the PC
  build. It did not exclude optional `native_aes_ctr.cpp` from the source glob,
  so minimal-profile builds still compiled that module. The root CMake list now
  excludes AES-CTR and explicitly appends it only when either CTR capability is
  selected, matching the PC build.
- Marked `native_signature.cpp::readBigEndianWord()` maybe-unused because no
  digest consumer exists in the all-disabled profile. Both
  `./tools/build arm --profile minimal` and
  `./tools/build riscv --profile minimal` pass, and both generated
  `build.ninja` files omit `native_aes_ctr.cpp.o`.
- Regression validation: both max-profile wrapper builds pass and include the
  CTR object; `python3 tools/test-javacard-support.py` passes 14/14. Builds
  still emit existing packed-member alignment warnings from CAP structure
  headers; this fix adds no new compiler warning.

### PC VPCD emulator connect refusal during reader startup — mitigated 2026-10-30

- `choupi` previously made a single TCP connect attempt to the configured
  localhost VPCD endpoint. A transient `ECONNREFUSED` occurred even though
  `pcsc_scan -r` had enumerated the virtual reader; the socket-activated
  listener became available moments later. This stopped GPPro testing before
  CAP traffic.
- `connect_vpcd()` now retries refused connections for up to 31 attempts at
  100 ms intervals (about three seconds maximum), while immediately returning
  on success or non-refused errors. Each failed socket is closed. This is
  emulator-only code; target firmware is unchanged.
- Validation: `./tools/build-pc --profile current` passed. The approved-host
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-gppro-empty-install-vpcd` then passed
  from an empty image: GPPro LOAD/INSTALL and post-install list confirmed
  package `4A43416C6754657374` and selectable applet
  `4A43416C675465737431` on Virtual PCD 00 01. The test's emulator log
  recorded `VPCD:CONNECTED endpoint=127.0.0.1:35964`. This run did not force a
  delayed-listener condition, so the retry branch itself was not directly
  fault-injected.
- Artifacts: `artifacts/results/gppro-empty-install-vpcd-20261030T205954Z/`.

### CAP 2.3 Class layout must not be parsed as 2.1/2.2 — resolved 2026-10-30

- The parser previously accepted every CAP version's Class component after its
  2.1/2.2 branches. CAP 2.3 adds a Virtual Method Mapping Table (VMMT) to the
  Class component; this VM does not consume that table. Silently accepting the
  new layout could skip metadata needed for virtual dispatch.
- Dynamic CAP validation now reads the Header version before walking other
  components, independent of component order, accepts compact formats 2.1 and
  2.2, and rejects other versions. The Class validator fails closed unless it
  has a 2.1 or 2.2 parser. No Java Card 3.0.5 behavior is restricted: the
  supplied API/deploy CAP corpus uses 2.1. To add compact CAP 2.3 support,
  implement VMMT dispatch and validate it with official-format fixtures first.
- Evidence: the official [Java Card VM Specification 3.2](https://docs.oracle.com/en/java/javacard/3.2/jc-vm-spec/F74158_02.pdf)
  describes the format 2.3 extended Class component; the [Java Card 3.1 release notes](https://docs.oracle.com/en/java/javacard/3.1/specnotes/index.html)
  state that format 2.3 adds VMMT.
- Validation: `./tools/test-cap-parser` passes 4,186 checks, all 48 supplied
  deploy CAPs, and 100,000 deterministic fuzz inputs. The test covers 2.3 and
  unknown-major rejection, and parses 2.2 when Class precedes Header. ARM
  `max_crypto`, RISC-V `max_crypto`, and PC `max_crypto` wrapper builds pass.
  Through the approved host path, GPPro's unchanged 1.8.2 CAP installed from a
  clean VPCD image, and the multi-package restart/delete suite passed 25/25.
  This does not claim physical validation; PC/SC still lists only VPCD.
- Logs: `artifacts/results/gppro-empty-install-vpcd-20261030T212130Z/`
  (emulator log SHA-256
  `d9b30bb92667cd529983716a05d12e03d2b61bfe32a6233e954355ec5b81ca74`);
  multi-package APDU log SHA-256
  `ccd7500268390ec94f585af60d84dd076750ed9aa3396f0d083873a5b6abffb1`,
  restart log SHA-256
  `b18ec590036087e809b3c370c02d6168d0a5af7dcd4438524badfb770ae417a9`.

### Profile README compared the base report to itself — fixed 2026-10-30

- `readme.md` previously supplied the base profile report as both inputs to
  `compare-jcalgtest-support.py`, so the example never compared against
  jCardSim and did not validate the current-profile report. It also pointed at
  an `expected.csv` path without showing how to preserve that profile's
  generated support set.
- The instructions now copy `build/javacard-support/jcalgtest-supported.csv`
  after each profile build, compare both results against the same checked-in
  jCardSim CSV, and compare the two expected-support files separately.
- Validation: both profile support CSVs generate successfully to `/tmp`. The
  comparator passes for the archived current report (15 declared, 15 actual).
  The archived base report from 2026-10-24 fails on three `OwnerPINBuilder`
  rows with `SystemException.ILLEGAL_USE`; that indicates a fresh base-profile
  report is needed, not a problem in the corrected command.

### Persistence regression after latest native changes — 2026-10-30

- `./tools/test-persistent-inventory`, `./tools/test-persistent-transaction`,
  and `./tools/test-rp2350-fs-overlay` passed; the overlay runner ended with
  `TEST:SUMMARY fail=0`, including injected snapshot, install-journal, and
  cross-store recovery cuts.
- Rebuilt the PC emulator and ran `./tools/test-vpcd-persistence` through the
  approved host-network path. Install/commit passed 3/3. A fresh process
  restored committed state and passed 38/38 checks, including four GC-pressure
  rounds and nested persistent aliases. This is VPCD evidence, not physical
  loss-of-power validation.
- Immediately afterward, the approved PC/SC helper granted access to two USB
  nodes, but `pcsc_scan -r` listed only the two Virtual PCD readers. The USB
  nodes were not identified as Pico readers; no physical card operation was
  attempted. Keep physical power-cycle recovery and persistent-record deletion
  open.
- Logs: `artifacts/logs/vpcd-persistence-commit-20261030T203932Z.log`
  (SHA-256
  `bf1f7a43bc4ce0d0cf07804c11b31e7b1277c09256c14abdb51b41649e5ce451`) and
  `artifacts/logs/vpcd-persistence-restore-20261030T203932Z.log` (SHA-256
  `59309a770239352bfa223f6e8bcd0bc6b307e4ea772c5079b3cc2bd599387bbc`).

### Fresh `base` profile JCAlgTest report clears historical OwnerPIN errors — 2026-10-30

- Rebuilt the current PC JCVM/ROM mask with `./tools/build-pc --profile base`;
  support generation selected 10 of 150 capabilities and ROM-mask generation
  passed.
- On a new blank VPCD image, explicitly installed the unchanged upstream
  `AlgTest_v1.8.2_jc305.cap` with GPPro, confirmed the package was `LOADED` and
  applet `SELECTABLE`, then ran official JCAlgTest 1.8.2
  `ALG_SUPPORT_EXTENDED` through `Virtual PCD 00 01`.
- The comparator matched all 4,525 normalized rows against jCardSim. CHOUPI
  reported 10 yes, 4,515 no, zero other; generated support comparison was
  10/10 with no missing or unexpected capabilities. `OwnerPINBuilder` results
  were `OWNER_PIN=yes`, `OWNER_PIN_X=yes`, and
  `OWNER_PIN_X_WITH_PREDECREMENT=yes`. The three
  `SystemException.ILLEGAL_USE` results in the archived 2026-10-24 report did
  not reproduce on current source.
- Use both the emulator bootstrap CAP code and `--applet-aid` when starting the
  VPCD emulator so PC/SC sees a present card; that bootstrap does not install
  the applet. Run GPPro `--install` with the unchanged CAP before starting the
  JCAlgTest client. Starting VPCD without the bootstrap left the reader absent;
  running the client before explicit install returned `6A82` on SELECT and
  produced no valid report.
- This is PC VPCD evidence, not physical-card validation. CSV SHA-256
  `a76dd25089b50e0269388c057098eafdbb434b5a96bbcdeb4b868b75b9537fac`; client
  log SHA-256
  `9c63926e0acdade95dfe32e4b479f9cd917011434736fc4d44349d344d5cd444`;
  expected-support CSV SHA-256
  `0b5d8ac1bd273d09ebe771267adee4334294b43c3616ce7617e8b7bd7d05ade7`.
  Files are under
  `artifacts/results/jcalgtest-base-vpcd-current-20261030-rerun3/`.

### Fresh base-profile multipackage VPCD lifecycle — 2026-10-30

- After `./tools/build-pc --profile base`,
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` passed all 25 checks.
  The run installed two packages and three applet instances, validated package
  and module GET STATUS, selected all three AIDs, checked per-owner reachability,
  restarted from the saved image/registry, then verified instance deletion,
  package-delete refusal while live instances remained, cascade deletion, and
  an empty final registry.
- The test exercised the current PC build, including the pre-existing dirty
  `src/cap_load.cpp`; no source files were staged or committed by this run.
  VPCD/process-restart evidence is not physical USB power-cycle evidence.
- APDU log SHA-256
  `67fc5ac0901290ac0db8120eed1078784dbbb115999879a1e711b6a0b7c257cf`;
  restart log SHA-256
  `62c64049dac1a210102de0176f2f98044a3a6208ec71e0f56c571c1b512b9748`;
  malformed-sidecar logs:
  `1258fee290847031d1bab8628a03ead541db187e4be544f8bef65951d817ccdf`
  (corrupt) and
  `758ad126b3e90ba79e92d99021dca3739b553bc130f600a4ed559ea3bdba50fd`
  (missing). Paths are printed by the test under `artifacts/logs/` with suffix
  `20261030T220126Z`.

### PC/SC publishes no VPCD readers despite active pcscd — 2026-10-31

- **Symptom:** The bounded GET DATA VPCD integration preflight found no
  `Virtual PCD 00 00` reader in three consecutive `pcsc_scan -r` runs. The
  authorized `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper granted access
  to two USB nodes and restarted pcscd, but the subsequent bounded scan still
  timed out without reader output. `systemctl status` confirmed both
  `pcscd.socket` and `pcscd.service` active; no `vpcd.service` or
  `vpcd.socket` unit exists. This is host PC/SC reader publication, not a JCVM
  APDU result.
- **Workaround:** None established. Do not claim GET DATA APDU integration
  until a VPCD reader is registered. The wrapper
  `tools/test-gp-get-data-vpcd` captures bounded reader scans and logs the
  emulator/probe output once VPCD is available.
- **Artifacts:** `artifacts/results/gp-get-data-vpcd-20261031T085055Z/`.
## OS and Pico oversized `fs_read` buffers — 2026-10-31

- The OS filesystem copies `min(buffer_length, record_length)` bytes and leaves
  the rest of a larger destination untouched. Pico previously rejected a
  buffer larger than the record, creating a backend mismatch.
- Viable fix: Pico `fs_read` now copies only the available prefix and returns
  success, matching the OS API contract. The overlay regression checks both
  copied bytes and an unchanged sentinel tail; the OS syscall regression
  checks the same behavior. Focused overlay and OS syscall tests, PC/ARM/RISC-V
  max_crypto builds, and the identified Pico 2 ARM cycle passed. Keep this
  behavior and avoid interpreting the untouched destination tail as record
  data; call `fs_length` first when the full record size is required.

## OS filesystem tag length extension — 2026-10-31

- The OS record header stores tag length in five bits and reserved code 30;
  legacy writers capped tags at 29 bytes while Pico allowed 128. The C syscall
  bridge also used a 33-byte length-prefixed tag scratch area, despite its
  cursor output already allowing 128.
- Viable fix: reserve header code 30 for an extra one-byte tag length, valid
  only for lengths 30–128. Legacy 1–29-byte records keep their byte layout.
  Parsing validates the extended length and rejects zero, reserved code 31,
  lengths below 30, or lengths above 128. The syscall bridge now carries 129
  bytes and rejects public wrapper tags outside 1–128 before marshaling;
  applet/static ownership checks accept structurally valid 128-byte tags.
  Tests cover restart, replacement, erase, 29/30/128/129 boundaries, malformed
  encoded lengths, and 128-byte syscall I/O. PC/ARM/RISC-V max_crypto wrapper
  builds pass. Keep old record compatibility; do not reinterpret any old valid
  header as an extended tag because code 30 was previously rejected.

## Filesystem error-code parity — 2026-10-31

- Pico previously collapsed missing tags, invalid lengths, out-of-range typed
  access, overlay capacity failures, and snapshot I/O failures into status `1`.
  This made cross-backend diagnostics ambiguous and hid an offset multiplication
  overflow in Pico typed reads/writes.
- Viable fix: use the OS FFI's established codes in Pico: `1` OutOfFlash, `2`
  NoSuchTag, `3` InvalidLengthForTag, `0x81` locked flash, `0x82` out-of-bounds,
  and `0x83` unknown flash/snapshot I/O. Preserve separate `fs_exists`,
  `fs_next_tag`, and `fs_init` contracts. Check typed offset multiplication
  before scaling. OS `edit_at` must validate bounds before removing the record
  from its index; otherwise an invalid update can panic or lose it.
- Verification: Pico overlay fault/capacity tests, Rust OS filesystem and
  syscall tests, and PC/ARM/RISC-V max_crypto builds passed. Keep this code table
  synchronized if either backend adds an error class.

## PC emulator checkpoint durability contract — 2026-10-31

- PC `fs_sync()` is a no-op status success after synchronous in-process OS
  filesystem mutation. It does not write the emulated flash image to disk.
- `--save` checkpoints only when normal control reaches the save block (after
  VPCD disconnect or idle timeout included). The memory image is truncated and
  rewritten first; `<memory>.cap-registry` is then written to a temporary file
  and renamed. Neither file is explicitly fsync'd, and the pair is not atomic.
- The registry CRC binds dynamic CAP metadata to the full memory image.
  `--resume` rejects a stale/corrupt/missing registry if dynamic package bits
  exist. No dynamic packages means a missing sidecar is acceptable.
- Viable usage rule: treat successful orderly `--save` completion as a process
  checkpoint only. Do not treat it as host power-loss durability. To recover a
  mismatch, restore the matching image/sidecar pair or reconstruct dynamic CAP
  registry state with `--restore-applet`.
- Root source: `choupi/src/main_pc.cpp`; contract: `docs/FILESYSTEM_ABI.md`.
  Atomic image replacement plus explicit durability syncing is not implemented.

### Resolved: applet-root inventory mixed separate heaps after DELETE

- **Reproduction:** With the VPCD test-only JCSystem transaction CAP,
  transient-state checks passed, but SELECT of a surviving applet after
  deleting the transaction package closed the session. The emulator had
  completed the APDU with `9000`; the PC persistent-inventory diagnostic then
  panicked while reading dynamic package `0x39` (`PC_FS:MISSING_DYNAMIC_CAP` /
  former tag `0139`).
- **Root cause:** Root enumeration collected every installed applet's
  persistent root reference and interpreted all of them in one `Context` heap.
  Applet-local references are offsets in separate heaps, so this treated an
  unrelated applet's local handle as a package-57 object. The embedded GC root
  walk had the same cross-context aggregation.
- **Viable fix:** Enumerate only the current context's own applet root and its
  already cached heap roots. Do not import all package static roots into each
  applet-local heap. Missing dynamic CAP IDs now fail directly rather than
  falling through to the ROM tag namespace. The VPCD regression passes
  post-delete selection and cleanup (47/47). ARM and RISC-V wrappers and the
  PC build pass. Pico 2 `8E464F4E2BBC0A19` was flashed with the ARM image; its
  finite monitor passed. The physical transaction-probe applet preserved its
  transient result across an applet switch, then its package was deleted and
  the surviving Core applet selected successfully (`9000`). GPPro confirmed
  that the temporary package was removed. Physical log:
  `artifacts/logs/physical-delete-reselect-8E464F4E2BBC0A19-20261101.log`.
- **Limit:** Package-wide static sharing across applet contexts is still a
  separate Java Card semantics issue; keep broader firewall/shareable and
  static-state behavior tracked independently.

## Cross-applet Shareable Interface Objects require context-owned proxies — 2026-11-01

- **Observed limitation:** `NativeImplementation.getAppletShareableInterfaceObject`
  currently returns null unless the requested server AID equals the current
  client AID, then obtains only the current applet instance. The default
  `Applet.getShareableInterfaceObject` also returns null unless an applet CAP
  overrides it. The native `invokeinterface` path resolves its receiver from
  the active context's heap; it does not switch to another applet's context.
- **Implication:** Removing the AID equality guard alone would not implement
  SIO semantics. Applet-local references are heap-relative, so a server object
  cannot safely be returned as a raw client-context reference. Cross-applet
  lookup and calls need an explicit proxy/handle mapping, server-context
  invocation, reference-argument and return-value firewall checks, and a return
  context switch. `Interpretor` already supports nested execution against an
  external `Context` and restores the previous active context via RAII; this is
  a possible execution primitive, not a complete SIO solution.
- **Dispatch constraint:** `NativeImplementation.getAppletShareableInterfaceObject`
  is not declared native in the ROM API JCA. It compiles to interpreted Java
  Card bytecode, and the `JCSystem` ROM method invokes it with `invokestatic`.
  Adding a same-named C++ function cannot override that bytecode call. Keep API
  sources and CAPs unchanged; the native implementation must intercept the
  resolved VM invocation before the same-AID guard executes.
- **Validation status:** Current-source VPCD multipackage regression passes
  47/47, but its existing Shareable probes cover absent servers/default-null
  behavior, not a successful cross-AID server call. Keep the broad context and
  firewall TODO open until a positive cross-applet fixture exercises lookup,
  interface dispatch, allowed object arguments, rejected private references,
  exceptions, and context restoration.

### Previous-context AID is now tracked for applet context switches — 2026-11-01

- `Context::setActiveContext()` now copies the active caller applet AID into a
  newly entered context. Re-entering the same context preserves the last
  previous AID; entry from JCRE (`nullptr`) clears it. Native
  `JCSystem.getPreviousContextAID()` returns null when there is no previous
  applet, otherwise creates/uses the JCRE-owned global AID object and exposes
  it through `SensitiveResult`.
- The native RMI/context regression checks client-to-server and return switches,
  JCRE entry, and same-context re-entry. PC, ARM, and RISC-V builds pass.
- This does not implement `getAppletShareableInterfaceObject`, cross-context
  object dispatch, or firewall checks. Hardware validation remains pending:
  the authorized reader helper and elevated `lsusb -d 2e8a:` both found zero
  devices on 2026-11-01.

### Resolve inactive applet contexts without changing selection — 2026-11-01

- Added `choupi/src/context_registry.hpp` for lookup by applet owner and full
  instance AID, and `choupi_vm_find_or_restore_applet_context()` for resolving
  a dynamic installed instance to its retained context or restoring its
  persistent root into an inactive slot. The operation never swaps/replaces
  the currently selected context; malformed AIDs, missing instances, restore
  failures, and a full inactive-context table return null.
- The RMI/context regression covers selected and inactive AID matches, wrong
  owner, and malformed AID. PC, ARM, and RISC-V builds pass; elevated VPCD
  multipackage lifecycle passes 47/47 after the change.
- The resolver was infrastructure only; it is now called by native dispatch
  for `JCSystem.getAppletShareableInterfaceObject`. Positive cross-applet
  service invocation and physical behavior are still untested.

### Native Shareable lookup callback token and same-context return — 2026-11-01

- **Symptom:** The upstream transaction probe's null/unknown/default Shareable
  lookup case returned `6F00` after adding native interception.
- **Cause:** A ROM-origin callback into a dynamic applet must resolve the
  embedded `Applet.getShareableInterfaceObject` token and then reverse-map it
  into the applet CAP's imported override token. Passing the imported token
  directly to the dynamic-class resolver produced `SecurityException`. Also,
  when client and server are the same `Context`, a nested `Interpretor` returns
  its reference on the existing caller operand stack; it does not populate the
  root-result slot used when running an inactive server context.
- **Viable fix:** Resolve the Applet callback with the embedded ROM token and
  request reverse mapping for the dynamic receiver. For same-context nested
  calls, pop the callback result from the current operand stack; for a separate
  server context, use its root reference result. `tools/test-jcsystem-transaction-applet
  --case 48` passes 3/3 on the unchanged CAP after both fixes. Preserve this
  distinction in other nested cross-context calls.
- **Limit:** Only null, unknown-AID and default-null paths are tested. Positive
  cross-AID lookup/invocation and firewall argument/result checks remain open;
  the authorized physical-reader helper reports zero devices.

### Fail-closed Shareable proxy dispatch — 2026-11-01

- Proxy references are ordinary local handles only until `invokeinterface`.
  Resolve their server applet by stored full AID, verify the requested
  interface has `ACC_SHAREABLE`, and resolve the method against the server
  object's concrete class before entering the server context. Do not call
  `Heap::getInstance()` on the client placeholder to resolve methods.
- `Stack::ReturnKind` distinguishes void, short, int, and reference results
  across nested interpretation. The nested server call returns primitive and
  void values safely; arguments are currently accepted only when the receiver
  is the sole argument. Nonnull server-local reference returns are rejected
  until converted into client-owned proxies. Null reference returns are safe.
- A proxy must not be usable as an ordinary instance. `invokevirtual`,
  `invokespecial`, direct instance field bytecodes, and array casts are denied;
  `checkcast`/`instanceof` accept only interfaces marked Shareable. This keeps
  the placeholder's implementation class and fields private.
- **Validation:** PC/ARM/RISC-V builds pass. The registry unit test covers
  typed root/method return bookkeeping and proxy metadata. The unchanged
  transaction CAP case 48 passes 3/3, covering null/unknown/default lookup
  behavior only. No positive two-applet CAP fixture is available, so forwarding
  has not been exercised end to end or on physical hardware.
- **Next:** Add a test fixture for a positive cross-AID lookup and zero-argument
  primitive/void interface calls, then test argument/result reference
  translation and exception/context restoration before calling CTX-01 complete.

### Shareable proxy casts must inspect the server class — 2026-11-01

- A proxy's client placeholder uses the server implementation class for
  storage. Checking only that a `checkcast`/`instanceof` target has the
  Shareable flag would make `instanceof` incorrectly succeed for any Shareable
  interface, even when the server class does not implement it.
- **Viable fix:** Resolve the proxy's server context and object, require the
  target interface's `ACC_SHAREABLE`, and apply `Class_Handler::docheckcast`
  to the actual server class/interface pair. `checkcast` and `instanceof` now
  share this verification. Ordinary classes, non-Shareable interfaces and
  array types remain denied for proxy placeholders.
- Existing VPCD multipackage regression passes 47/47, but it contains no proxy
  calls. Full PC/ARM/RISC-V builds pass. A positive Shareable fixture is still
  needed to validate the cast behavior at runtime.

### Translate cross-context Shareable reference returns — 2026-11-01

- Never push a server applet's local `jref_t` directly onto the client
  context's operand stack: its offset refers to a different heap.
- Null and JCRE-global references (`isGlobalArray()` or
  `isGlobalInstance()`) are runtime-wide handles and may cross unchanged.
  Server-local references must be rooted in the server context and represented
  by a proxy allocated/registered in the client context. Reject arrays and
  classes without a Shareable interface.
- **Current implementation:** `invokeinterface` follows these rules for
  reference results. It still rejects all arguments; no positive CAP fixture
  tests the behavior yet. Existing host VPCD multipackage tests pass 47/47 but
  do not exercise Shareable calls. The authorized PC/SC helper currently sees
  zero Pico readers, so physical validation is pending.

### Same-context Shareable proxy call entered with stack underflow — fixed 2026-11-01

- **Symptom:** The positive Shareable test in the unchanged upstream
  `AlgTestApiCore_305.cap` returned `EXC_RUNTIME` on its
  `getAppletShareableInterfaceObject` → sub-interface `ping()` path.
- **Cause:** Proxy dispatch pops the client-side proxy receiver before creating
  the nested interpreter call. Constructing that call with initial references
  selects the interpreter mode that expects arguments on the caller operand
  stack. The caller's receiver had already been consumed, so entry raised
  `Exceptions::StackUnderflowException` (enum value 1) before executing the
  server method.
- **Viable fix:** Enter nested Shareable methods with an explicit typed
  `InterpretorInitialLocal` receiver. Before dispatch, validate the concrete
  server class against the exact requested interface, as well as requiring
  `ACC_SHAREABLE` on the interface. Preserve fail-closed behavior for calls
  with unimplemented argument translation.
- **Validation:** Fresh VPCD run of the three selected Shareable calls from
  unchanged `AlgTestApiCore_305.cap` passed; the sub-interface call returned
  `1` as expected. `tools/test-rmi-registry`, PC build, ARM build and RISC-V
  build passed. Artifact:
  `artifacts/results/shareable-vpcd-verified-20261101T183526Z/`.
- **Limit:** Same-applet/same-context path only. Cross-AID routing, argument
  conversion, exception propagation, GC/reset lifetime and physical validation
  remain open. Latest approved reader enumeration found zero Pico readers.
- **Correction:** The prior “positive CAP fixture missing” assessment was
  wrong; inspect API CAP entries under `APPLET-INF/classes` and deploy CAPs
  before concluding that no upstream fixture exists. Do not modify those CAPs.

### Same-context Shareable methods should use the existing operand frame — 2026-11-01

- **Finding:** Explicit typed-local nested entry fixed the receiver-only call,
  but same-context proxy methods already have their correctly typed arguments
  on the caller operand stack and need no cross-context translation.
- **Viable approach:** Replace the proxy receiver word at its `invokeinterface`
  stack position with the retained server object reference, then use ordinary
  `Method_Handler::callVirtualMethod` with caller-stack arguments enabled. Keep
  exact server-class/interface checking before this substitution. This
  preserves primitive widths and in-context references, and avoids spinning a
  second interpreter frame just to make a same-context call.
- **Validation:** Stack unit assertion `stack_receiver_replace=1`; unchanged
  upstream Core 3.0.5 CAP passes the three focused Shareable calls with
  `returned=1` and successful cleanup. PC/ARM/RISC-V builds pass.
- **Limit:** Cross-context SIO still rejects arguments, and remains untested
  end to end. Do not copy raw local references or assume a same-context stack
  replacement is valid when server/client contexts differ.

### Cross-context Shareable primitive arguments — 2026-11-01

- CAP `invokeinterface` records the argument word count, while the CAP
  Descriptor component records the method's logical parameter types. Use the
  requested interface's Descriptor method entry to recover those types; do not
  infer types from raw stack words, and do not resolve the proxy placeholder in
  the client heap.
- **Current implementation:** Proxy dispatch scans variable-length class
  descriptor records to locate the requested Shareable interface's method
  signature, copies boolean/byte/short/int arguments into typed interpreter
  locals, and then removes the original caller words. Null references may
  cross; non-null object/array references are rejected until firewall ownership
  translation exists. Unsupported/malformed signatures fail closed.
- This code builds for PC, ARM and RISC-V. The existing upstream positive SIO
  CAP tests only a same-applet call, so they do not execute the cross-context
  argument branch. `tools/test-rmi-registry` directly tests the local
  signature-to-local conversion for byte/short order, int high/low word order,
  null versus non-null reference handling, and argument-count mismatch. This
  does not test Descriptor lookup or a cross-context VM call. Do not claim
  argument forwarding works on hardware until a positive cross-AID fixture
  exercises it. Keep all upstream CAP files unchanged.
- The approved reader helper and `lsusb -d 2e8a:0009` currently find no Pico in
  the VM, although PC/SC exposes two VPCD readers. Retry hardware only after
  guest USB enumeration returns.
- An attempted full multipackage VPCD regression passed its package, restart,
  lookup and delete phases but timed out while reinstalling the transaction
  probe (`INSTALL a000000062030101`). Root cause is not established; retain its
  full logs before attributing it to firmware.

### Shareable cross-context argument reference boundary — 2026-11-01

- Null references and JCRE-global array/instance handles are now copied into
  the server call's typed locals. These handles use the shared global heap and
  do not identify storage in either applet's private heap. Non-null handles
  array-typed descriptor parameters require an array handle; a global array
  may also satisfy an `Object` parameter because arrays are Java objects.
- Applet-local object/array references remain rejected. Do not pass client
  local references or proxy placeholders across contexts until explicit
  ownership translation and the firewall policy are implemented.
- `tools/test-rmi-registry` covers null, JCRE-global instance/array, and
  applet-local rejection. This is a marshaller unit test, not an end-to-end
  cross-AID SIO test.
- The user's physical devices were reported connected, but this VM currently
  enumerates no `2e8a:` USB devices; `pcsc_scan -r` only showed VPCD. Recheck
  guest USB passthrough before attempting a physical run.

### Physical Shareable Core regression restored USB path — 2026-11-01

- Earlier `2e8a:` inventories were stale: all three boards later enumerated as
  RP2350 BOOTSEL (`2e8a:000f`). Avoid querying multiple bootloaders with
  concurrent picotool processes; that caused connection errors. Sequential
  `picotool info --device --bus 1 --address N` returned stable chip IDs.
- Board `8E464F4E2BBC0A19` was targeted explicitly. `tools/cycle choupi`
  flashed `build/arm/pico2_choupi.uf2`, and finite serial output passed
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. The authorized
  PC/SC helper then exposed it as reader
  `Pico 2 CHOUPI CCID [JCVM CCID] (8E464F4E2BBC0A19) 00 00`.
- Unchanged upstream `AlgTestApiCore_305.cap` uploaded, completed all 291 Core
  calls (272 `OK`, 11 `UNSUPPORTED_PER_SPEC`, 8 `APPLET_NOT_INSTALLED` from
  skipped Multi/Ext scenarios), and cleaned up successfully. Three Shareable
  rows were `OK`; the positive sub-interface invocation returned `1`.
- This is physical same-applet SIO coverage only; it does not test cross-AID
  descriptor-based arguments. Logs and SHA-256 digests are in current
  `.agent/HANDOFF.md`.
