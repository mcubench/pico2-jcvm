# Physical Pico 2 USB interface disappeared after successful flash (2026-10-26)

- **Symptom:** Pico 2 `2BED26ED20556535` accepted a verified CHOUPI image flash,
  but the following finite serial monitor could not find its USB serial device;
  subsequent USB/PCSC enumeration showed only `D92E743D5F640C0F`.
- **Cause:** Unknown. Do not attribute the disappearance to the new RSA
  implementation without further evidence.
- **Viable continuation:** Treat identified boards as a failover pool. Re-run
  the bounded USB/PCSC enumeration, choose another currently enumerated Pico,
  confirm its chip ID, and continue the same build/flash/test there. D92 passed
  the runtime monitor and all 45 RSA physical vectors after the same image was
  flashed. Retry 2BED only after it re-enumerates.
- **Related host detail:** The PCSC test client requires the exact reader name,
  including its `00 00` interface suffix; a shortened label fails before card
  installation and is not a firmware/card result.

## Guest VM loses all Pico USB devices after D92 flash (2026-10-26)

- **Symptom:** D92 (`D92E743D5F640C0F`) flashed and verified successfully, but
  `tools/monitor` found no USB serial device. Subsequent elevated `lsusb`
  showed no Pico USB devices; `pcsc_scan -r` listed only `Virtual PCD 00 00`
  and `Virtual PCD 00 01`; the authorized reader helper found 0 CHOUPI readers.
- **Cause:** Unknown; guest USB passthrough/enumeration is a likely possibility,
  but is not established. The firmware flash itself completed and verified.
- **Viable continuation:** Do not attribute this to firmware without evidence
  or repeatedly wait on this missing device. Continue offline; when devices
  reappear, select another enumerated board and confirm its chip ID before
  hardware testing.

# Project issue knowledge base

## Long RSA-3072 KeyPair generation exceeded default VPCD deadline (2026-10-27)

- **Symptom:** The asymmetric VPCD suite failed on its RSA-3072 generation
  APDU under the default 20-second response deadline; one measured RSA run took
  31.280 seconds. This was a test-harness timeout, not a crypto result.
- **Cause:** Mbed TLS prime generation time varies with random candidate count;
  3072-bit key generation can exceed the VPCD harness default.
- **Viable fix:** The asymmetric VPCD runner now uses a 180-second bounded
  APDU timeout, allowing slow large-key generation while retaining a finite
  bound. An isolated RSA-CRT-3072 call completed in 8.805 seconds. The complete
  suite passed 99/99 checks with both algorithms enabled.
- **Validation:** `PICO2_HOST_USB_ACCESS=1 ./tools/env python3
  tools/tests/test_asymm_crypto_vpcd.py` passed after the timeout adjustment.
  The timeout is test-only; firmware call limits were not changed.

## VPCD command-case framing for no-input APDUs (2026-10-27)

- **Symptom:** The new KeyAgreement vector returned `6700` before entering the
  applet method.
- **Cause:** The test encoded a six-byte command with both `Lc=0` and `Le=0`.
  A short case-2 APDU with no command data is five bytes: `CLA INS P1 P2 Le`.
  A later edge vector used P2 as an option, which also made it the Le byte and
  truncated the response.
- **Viable fix:** Encode no-input response APDUs with five bytes. When the test
  needs an option and a response, send it as one command-data byte in a case-4
  APDU (`CLA INS P1 P2 Lc data Le`).
- **Validation:** Correct framing made the six agreement known-answer vectors
  and DH/DHC missing-cofactor checks pass; the complete VPCD suite passed
  86/86 checks.

## Pico USB devices absent from the guest VM despite being plugged in (2026-10-26)

- **Symptom:** The user reported physical Pico 2 tokens connected. Elevated
  `lsusb` listed only the VirtualBox tablet and Linux root hubs; `pcsc_scan -r`
  listed only two virtual readers; the authorized `pico2-pcsc-reader` helper
  reported zero `2e8a:0009` CHOUPI devices.
- **Cause:** Unknown. The guest currently receives no Pico USB enumeration;
  device failure is not established.
- **Viable continuation:** Do not flash or attribute this to firmware. Continue
  offline, and retry bounded enumeration after the VM receives a Pico device.
  If several enumerate, confirm chip IDs and use another identified board if
  one becomes unavailable.
- **Validation:** Two elevated USB/PCSC enumeration checks during this turn
  returned the same result. No hardware operation was attempted.

## NativeKey ROM class identity mismatch — 2026-10-26

- **Symptom:** VPCD `asymm_crypto` APDU 11, the first RSA `KeyBuilder.buildKey`
  call, returned `6F00` with an uncaught `SecurityException` (reason 29).
- **Cause:** `NativeKey` was package-private, had no ROM export token, and the
  native factory allocated/validated it using `ROM_CLASS_INDEX`, which is a
  ROM token rather than the runtime class-component offset.
- **Viable fix:** Make the VM-owned API class exportable and resolve its class
  offset with `Export_Handler(Package(...)).getExportedClassOffset(...)` for
  both `Heap::addInstance` and runtime receiver validation. Apply the same
  resolver in RSA Cipher and Signature key checks.
- **Validation:** PC `asymm_crypto` build and elevated VPCD regression passed;
  all 26 checks succeeded, including all 5 RSA key-builder vectors and 6 RSA
  SHA-1 Signature vectors. ARM and RISC-V wrappers also passed. This fixes
  class identity resolution; it does not complete RSA Cipher's Java-provider
  migration.

## jcarray test target lacked generated headers and collector stubs — 2026-10-25

- **Symptom:** `./tools/test-jcarray` could not compile `jni.cpp` because the
  target omitted `build/javacard-support` from its include path. After fixing
  that, linking GC entry points retained instance-field visitor references
  that the test target did not link. The existing test also expected
  `SecurityException` for an invalid transient clear event.
- **Cause:** The target compiled shared JNI/heap source without the generated
  include directory and without test-only instance persistence/JCRE
  dependencies. The expected status was stale: `getTransientClearEvent`
  currently raises `SystemException`.
- **Viable fix:** Add the generated support include path, provide test-only
  stubs for the two unused `JC_Instance` accessors and applet-selection query,
  and expect `SystemException` for the invalid event. Keep the stubs scoped to
  this host test target.
- **Validation:** `./tools/test-jcarray` passed, including static/APDU Java
  buffer and native APDU outgoing-buffer mirror root retention, plus
  reclamation of seven unrooted arrays. The regression also traces a child array
  through a rooted wildcard reference array using `setReferenceEntry`. A
  top-level reference result remains rooted, then is released when replaced by
  a primitive result. A `CLEAR_ON_DESELECT` reference array remains intact on
  reset, is cleared on deselect, and its formerly referenced child is reclaimed
  by the next collection. An exported instance and array child remain rooted
  through collection; unexport clears the registry root and the next collection
  reclaims both.
- **Harness note:** The focused target needs test-only linker stubs for
  constant-pool/type-check and persistent-reference functions pulled in by the
  setter. The wildcard branch does not call these stubs; keep them scoped to
  this host test. For instance-edge coverage it creates a one-field
  `JC_Instance` fixture and writes the normal object/array field encodings;
  this checks Heap traversal but does not substitute for CAP-driven lifecycle
  validation.

## PC/SC stopped responding after bounded GPPro GET STATUS timeout — 2026-10-25

- **Symptom:** On Pico D92 (`D92E743D5F640C0F`), GPPro authenticated and
  completed GET STATUS for P1=80, P1=40, and P1=20, then
  stalled after sending `84 F2 10 02`. After the bounded Java process was
  terminated, a later `pcsc_scan -r` also timed out without printing readers.
- **Evidence:** The approved `pico2-pcsc-reader` helper reported access to two
  USB nodes, while `pcsc_scan` did not return. The VPCD module-list regression
  passes, and the physical CCID debug snapshot recorded the module GET STATUS
  APDU as its last XfrBlock; exact card response bytes were not captured.
- **Cause:** Unresolved. This does not prove a firmware limit of two readers,
  and the available output cannot distinguish a card response issue from a
  stuck pcscd/SmartcardIO transaction.
- **Viable fix:** None established. The helper restores ACLs after
  re-enumeration, but did not recover `pcsc_scan` in this occurrence. Keep
  physical validation open; continue on a different identified Pico if it is
  already accessible. Do not retry an unbounded GPPro command.
- **Validation:** `./tools/build-pc`, `./tools/build arm`,
  `./tools/build riscv`, registry test (50/50), and VPCD multipackage test
  (20/20) passed. D92 finite runtime cycle passed, but physical module GET
  STATUS did not.

## Pico runtime USB devices absent from PC/SC after clean firmware cycle — 2026-10-25

- **Symptom:** After D92 was reflashed with the clean static-buffer build and
  passed finite serial monitoring, the approved helper granted access to two
  USB nodes and `lsusb -d 2e8a:0009` showed two Pico JCVM devices, but
  `pcsc_scan -r` did not list any Pico reader before its 8-second timeout.
- **Cause:** Unknown. USB enumeration confirms both devices are connected; it
  does not identify why pcscd lacks a reader entry. No firmware reader-count
  limit is established.
- **Viable fix:** None established for this occurrence. The approved helper
  applied permissions but did not restore PC/SC enumeration. Continue on any
  other exact reader that becomes available; keep physical APDU testing open.
- **Validation:** Helper reported `count=2`; `lsusb` listed bus 001 devices 080
  and 086; line-buffered `pcsc_scan -r` timed out without Pico readers.
- **Latest reproduction:** After a fresh D92 GPPro timeout, the diagnostic
  firmware was built for ARM/RISC-V and ARM was flashed with a passing CHOUPI
  runtime cycle. GPPro then saw only the two Virtual PCD readers. The approved
  helper reported two USB nodes (`/dev/bus/usb/001/090` and `/dev/bus/usb/001/080`);
  `lsusb` confirmed both are Pico JCVM readers, but `pcsc_scan -r` timed out
  without listing them. The new firmware reports last response APDU, length,
  and status in serial diagnostics, but no fresh card command could be sent.

## Firmware upload preserves installed CAP state; first flashed BOOTSEL board lost runtime enumeration — 2026-10-25

- **Symptom:** After flashing, GPPro still listed previously installed JCAlgTest
  and JavacardxSec applets. A different BOOTSEL Pico (8E
  `8E464F4E2BBC0A19`) accepted and verified the image, then did not enumerate
  as runtime serial or PC/SC.
- **Cause:** Firmware upload preserves the separate persistent CAP/filesystem
  data, which was confirmed by GPPro. The 8E USB/runtime failure cause is not
  known.
- **Viable procedure:** Treat firmware image contents and persistent card
  registry as separate state. Use GPPro to delete each listed applet then its
  package when an empty card is required. For device failover, target an
  identified board with a working CCID reader; 2BED
  `2BED26ED20556535` accepted the same image and passed the finite runtime
  monitor after 8E did not enumerate.
- **Validation:** On 2BED, ARM/RISC-V builds passed, ARM flash verification
  reached 100%, runtime monitor passed, and final GPPro `--list` showed only
  ISD `A000000151000000`.

## GPPro reader state after Pico runtime re-enumeration — 2026-10-25

- **Symptom:** After the 2BED firmware cycle, `pcsc_scan -r` listed both Pico
  readers and ATRs, but a GPPro command could stall before or during reader
  selection. With multiple readers, a separate optional `--reader` argument
  also selected the first Pico instead of the requested 2BED reader.
- **Cause:** The reader/ACL state appeared stale after runtime re-enumeration;
  a root cause below the observed PC/SC state was not established.
- **Viable procedure:** Run the approved
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper after re-enumeration, then
  bind the optional argument as `--reader='exact reader name'`. Bound the GPPro
  command with `timeout` and inspect its `SCardConnect` trace to confirm board
  identity before any write.
- **Validation:** After the helper refresh, GPPro connected to exact reader
  `Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 01 00`; package DELETE and
  the final ISD-only listing completed. D92 still timed out and was not
  modified.

## Physical GPPro multi-package install: use `--install-only` for later applets — 2026-10-25

- **Symptom:** GPPro `--install CAP --applet B` returned `6A80` after applet A
  had already been installed from the same CAP.
- **Cause:** `--install` repeats package LOAD, so it cannot install a second
  applet from an already loaded package.
- **Viable fix:** Load and install the first instance with `--install CAP
  --applet A`; install subsequent instances from that package with
  `--install-only CAP --applet B`.
- **Validation:** On Pico 2 `2BED26ED20556535`, both Multi 3.0.5 instances
  listed and selected successfully. Logs are recorded in `.agent/HANDOFF.md`
  under the 2026-10-25 physical multi-package validation.

## Pico D92 PC/SC reader can enumerate while APDUs stall — 2026-10-25

- **Symptom:** `pcsc_scan -r` listed D92
  `D92E743D5F640C0F`, but GPPro `--list` timed out. Refreshing the authorized
  reader access helper did not restore APDU response.
- **Cause:** Not established. Runtime firmware passed its finite serial
  monitor, while card-manager APDU exchange did not complete.
- **Viable workaround:** Treat reader presence as insufficient proof of a
  responsive card. Use the identified-device failover pool; 2BED
  `2BED26ED20556535` responded to GPPro and the physical deploy runner.
- **Validation:** On 2BED, GPPro listed packages, installed Core and Multi
  upstream 3.0.5 CAPs, selected three applet instances, ran 291 Core API calls,
  and cleaned all test packages. D92 was left untouched by CAP writes.

## Host VPCD DELETE prefix erase requires privileged filesystem syscall — 2026-10-25

- **Symptom:** Authenticated raw SCP03 DELETE initially aborted the PC VPCD
  emulator with an assertion in the host flash backend:
  `locked() && privilege::is_privileged()`.
- **Cause:** Card-manager DELETE reached the filesystem prefix-erasure helper
  directly from VPCD dispatch, outside the trusted OS syscall context.
- **Viable fix:** Expose prefix erase as `FsErasePrefix` and have the host
  bridge invoke it through the trusted syscall dispatcher. The handler checks
  namespace write permission and delegates to `FileSystem::erase_prefix`.
- **Validation:** PC, ARM, and RISC-V builds pass. The authenticated raw VPCD
  multi-package regression passes 13/13, including applet DELETE, refused
  package DELETE with a live applet, cascade DELETE, and final empty registry.
  Current log: `artifacts/logs/host-vpcd-multipackage-apdu-20261025T050904Z.log`
  SHA-256 `8123442ca2a86d4d1cdc24d77a73d8b0312c3207c3404c58a4836d81ef0ef829`.
  Physical boards were not visible to this VM during authorized BOOTSEL checks.

## Raw VPCD multi-package APDU setup and dynamic SELECT routing — 2026-10-25

- **Symptom:** The first raw SCP03 test received `6999` for INITIALIZE UPDATE;
  after correcting setup, GET STATUS listed dynamically loaded instances but
  selecting them returned `6A82`.
- **Causes:** The test had not selected the default ISD, so the VPCD session
  correctly routed card-manager APDUs as applet traffic. The VPCD router also
  matched SELECT only against its one CLI-configured applet AID, despite the
  host package registry containing more installed AIDs.
- **Viable fix:** Select the default ISD before SCP03. Route every syntactically
  valid SELECT-by-AID through the installed-instance restore callback; return
  `6A82` when the registry has no matching applet. The CLI AID remains used
  only for its optional resume/install configuration.
- **Validation:** `./tools/test-vpcd-multipackage --timeout 180` passed 9/9:
  two upstream packages loaded, three instances listed and selectable, unknown
  AID rejected with `6A82`. `./tools/build-pc`, ARM and RISC-V wrappers passed.
  Physical boards were not visible to the VM during the follow-up enumeration.

## Concurrent ARM/RISC-V builds collided in the shared ROM-mask tree — 2026-10-25

- **Symptom:** Concurrent `./tools/build arm` and `./tools/build riscv`
  invocations both regenerated and consumed shared `build/choupi-pc` artifacts.
  One wrapper completed while the other failed at LTO with an internal
  compiler bus error (exit 2).
- **Cause:** The wrappers share the PC API/ROM-mask generation directory and
  support metadata; there was no cross-process build lock. The LTO bus error
  occurred only during this concurrent run, so resource/contention causality
  is likely but not independently proven.
- **Viable fix:** `tools/build` now holds an exclusive `flock` on
  `build/.tools-build.lock` through each complete architecture build. Keep the
  existing wrapper default at two compile jobs; concurrent requests now wait
  rather than racing generated files or multiplying compiler load.
- **Validation:** Launched ARM and RISC-V `symm_crypto` wrappers concurrently
  after the change. RISC-V acquired the lock first; ARM logged `waiting` until
  RISC-V exited, then acquired it. Both exited 0 and reported `BUILD:PASS`.
  No physical flash was needed for this host build-orchestration-only change.

## ROM class token differs from runtime class index — 2026-10-24

- **Symptom:** Native RSA key methods initially threw `SecurityException`
  (`6F00`) even though the receiver belonged to native package 17. The generated
  `ROM_CLASS_INDEX_*` value was 4, while the allocated object's runtime class
  index was 42.
- **Cause:** `ROM_CLASS_INDEX_*` is the local ROM/export class token, not the
  runtime class index. Resolve it through `Export_Handler::getExportedClassOffset`
  and apply `HTONS`, matching the imported-class resolution in
  `ConstantPool_Handler::getClassInformation`, before comparing instance IDs.
  Passing the token directly to `Heap::addInstance` also failed during class
  resolution; use the VM's normal CAP `new` path for ROM API objects.
- **Viable fix:** Resolve expected class identity from the package export table
  and let standard JCVM allocation construct API objects. Native methods can
  then validate package and resolved class IDs before touching fields.
- **Validation:** The asymmetric VPCD probe passed 20/20 checks after this fix;
  ARM and RISC-V builds passed. Physical validation was not run in this step.

## Pico storage durability depends on VM transaction state — 2026-10-24

- **Source finding:** `src/choupi_pico2.cpp:fs_sync()` returns success without
  flushing when `choupi_persistent_write_transaction_active()` is nonzero.
  `FlashMemory_Handler::commitPersistentWriteTransaction()` clears that VM
  flag and calls `fs_sync()` to make the overlay durable. Thus the lower
  record-store interface depends on higher-level VM state, and successful
  sync does not always mean persistence.
- **Existing protections:** Filesystem snapshots, CAP metadata mirrors, and
  the committed INSTALL recovery journal are present. Preserve them; this
  finding does not establish that every interruption corrupts state.
- **Open refactoring direction:** Make transaction/flush semantics explicit
  and give LOAD/INSTALL/DELETE durability one coordinating owner, with portable
  stores over a shared physical-flash backend. No implementation fix or new
  power-cut experiment was performed in this documentation step.
- **Analysis:** `FIRMWARE_JCVM_ARCHITECTURE.md`, findings L1/L5/L6.

## JC305 CardException was reported as ISOException through RMI — 2026-10-23

- **Symptom:** RMI calls throwing `CardException` and a `UserException`
  subclass returned ISOException type `0x23`, instead of their own exception
  types. Other Java Card framework exception reason fields also needed explicit
  propagation through the RMI wire response.
- **Cause:** The ISOException dispatch special-case treated embedded framework
  export token 18 as ISOException. In this ROM export, token 18 is
  `CardException`; `ISOException` is token 21. The RMI mapper also continued
  walking framework exceptions into `java.lang` superclasses without first
  recognizing the framework type and its inherited reason field.
- **Viable fix:** Match framework classes by embedded export token before
  walking to `java.lang`, return their protocol type, and read the 16-bit
  reason from instance field slot 0. Use token 21 in the ISOException dispatch
  special-case. Keep JC305 imported-token mapping distinct from embedded export
  token numbering.
- **Validation:** ARM and RISC-V builds passed. VPCD passed 35/35 class-form
  APDUs and 3/3 interface-form APDUs. The official SDK 2.2.1 proxy decoded all
  seven tested Java Card exceptions as their expected framework classes. On
  physical D92, all seven exact reason responses passed; cleanup removed the
  fixture and verified JCAlgTest remained installed. Logs:
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T085319Z.log`,
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T085436Z.log`,
  `artifacts/logs/rmi-array-physical-20261023T085737Z.log`.
- **Limit:** Other Java Card API exception types, unknown/custom framework
  subclasses, and malformed class hierarchies still need coverage.

## CryptoException reason was not preserved for Java RMI throws — 2026-10-23

- **Symptom:** `javacard.security.CryptoException.throwIt(0x1234)` travels as a
  Java exception object and cannot be handled by only mapping native
  `Exceptions::CryptoException` values.
- **Cause:** `CryptoException` is exported from the separate
  `javacard.security` package (AID `A0000000620102`), rather than the
  `javacard.framework` package. Its ROM-mask class view is at class offset 233.
- **Viable fix:** During the RMI class-hierarchy walk, match the security
  package's CryptoException class at offset 233, emit wire type `0x30`, and
  read its inherited 16-bit reason from field slot 0.
- **Validation:** ARM/RISC-V builds passed. VPCD returned `823012349000`;
  official SDK 2.2.1 decoded `javacard.security.CryptoException`; physical D92
  returned the same response and fixture cleanup plus JCAlgTest preservation
  passed.
- **Limit:** A custom CryptoException hierarchy with two applet-defined classes
  before the API parent is covered by an exact-response test. Other
  security-package exception types and longer inheritance chains remain
  untested.

## Fresh host VPCD card-manager state looked preinstalled — 2026-10-22

- **Symptom:** GPPro authenticated and listed a selectable JCAlgTest applet,
  but `--install` rejected the same CAP because its package AID was already
  present. An earlier launch also returned `Connection refused` when CHOUPI
  started before the virtual PC/SC endpoint was active.
- **Cause:** Supplying `--class-index` reconstructs a single installed applet
  for VPCD resume tests. The host CLI required that option on every VPCD start,
  leaving no fresh empty manager mode. It also returned configured CAP bytes
  through package lookup before GP LOAD committed the package.
- **Viable fix:** `--class-index` is optional; pass it only for resume fixtures.
  Gate per-package CAP lookup on `vpcd_package_loaded`, which becomes true on
  package storage commit. Run `pcsc_scan -r` before starting the CHOUPI VPCD
  client so the reader endpoint is active before it connects.
- **Validation:** GPPro 20.01.23 completed initial ISD-only GET STATUS, SCP03,
  JC305 CAP LOAD/INSTALL, applet/package listing, applet DELETE, package
  DELETE, and final ISD-only listing. Transcript:
  `artifacts/logs/gp-vpcd-card-manager-gppro-20261022.log`.
- **Limit:** The host registry still models one dynamic package/instance.
  For restart, use the package ID actually allocated by LOAD (56 for the first
  dynamic package in this run) plus the saved class index; the placeholder
  `--package-id 63` caused a false restore failure.

## Clearing the first persistent array field broke other aliases — 2026-10-22

- **Symptom:** After restoring a committed applet in a fresh VPCD process,
  clearing static reference A and collecting caused a remaining static alias B
  to restore as null. The pre-fix probe returned `00` for case 81.
- **Cause:** Persistent array serialization stored the array body under the
  first field tag that referenced it. A later alias pointed to that tag. Writing
  null to the first field replaced the array body with the null marker, losing
  the target record even though another alias remained.
- **Viable fix:** Store persistent array bodies under an identity tag derived
  from the first field tag plus reserved array index `0xffff` (array element
  indices stop at `0xfffe`); store an alias at each reference field. Route
  nested instance-array fields through the same reference serializer. The GC
  can then reclaim the RAM object while the stable body record remains
  available for lazy restoration.
- **Validation:** `./tools/build arm` passed. Elevated
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed install 3/3
  and restore/stress 33/33. Trace line 344 confirms one array reclaimed after
  clearing A; B then returned the expected byte after flash restoration. This
  verifies current-format writes across a VPCD process restart; physical
  power-cycle behavior and migration of legacy field-owned array records remain
  unverified.
- **Artifacts:** `artifacts/logs/vpcd-persistence-restore-20261022T061524Z.log`
  and `artifacts/logs/vpcd-persistence-restore-emulator-20261022T061524Z.log`.

## Security transient-key follow-up rows are unsupported-feature fallout — 2026-10-22

- **Symptom:** Current-source deploy Security cases 470 and 472 are decoded as
  `WRONG_RESULT` for `Key.isInitialized()` checks after transient AES setup.
- **Cause:** The paired setup cases fail with `UNSUPPORTED_PER_SPEC` because
  `KeyBuilder.buildKey` only creates persistent AES/DES keys; it rejects
  transient storage types. The later assertions therefore do not exercise
  clearing semantics on a successfully created transient key.
- **Viable resolution:** Classify the dependent rows as unavailable-feature
  fallout under the current project policy. Do not implement transient key
  storage or cryptographic algorithms solely to eliminate these decoder rows.
  If auditing `isInitialized()`, use a successfully constructed supported key
  and check the result/effect pair independently.
- **Evidence:** `artifacts/logs/security-full-vpcd-fixed-decoded.txt` cases
  470/472 and `AlgTest_JavaCard/deploy/results/Pico2_CHOUPI_current_security_20261021.csv`
  setup rows 471/473. SDK implementation:
  `choupi/javacard-sdk/javacard-api/src/javacard/security/KeyBuilder.java`.
- **Reconfirmed:** Physical current-profile Security run on Pico 2
  `2BED26ED20556535` on 2026-10-25 reproduced the same two dependent rows;
  the report's setup cases still show unsupported transient key types. Do not
  treat these as a new `isInitialized()` or transient-clear regression.
  Report: `artifacts/results/deploy/current-2BED-20261025/security.csv`.

## SDK JCRMI client SELECT with Le=7F — 2026-10-22

- **Symptom:** The bundled Oracle Java Card 2.2.1 `JavaCardRMIConnect` sends
  SELECT `00A4041009A000000151000001017F`; current-source VPCD returns `6D00`.
  The equivalent SELECT with final Le byte `00` returns the expected RMI FCI.
- **Cause:** `is_select_for_applet()` required a case-4 SELECT's trailing Le to
  equal `00`. The SDK's valid `Le=7F` therefore failed to set the VM's
  applet-selecting state; `RMIService` ignored the SELECT and the applet
  returned `6D00`.
- **Viable fix:** Accept the optional Le byte independently of its value after
  validating the SELECT header, AID length, and total command length. The same
  code now accepts the SDK's `Le=7F`.
- **Validation:** The unmodified SDK command
  `00A4041009A000000151000001017F` now returns the expected RMI interface FCI
  and `9000`. `tools/test-rmi-client-vpcd` also passes `Counter.add(short)`
  (result 3), the runtime exception subclass, and the checked exception
  subclass. The checked exception is surfaced as
  `UndeclaredThrowableException` because the interface only declares its
  concrete subclass. Log:
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261022T030617Z.log`.
- **Status:** Resolved for short case-4 SELECT and current-source VPCD. Physical
  SDK client interoperability remains unverified; no firmware was flashed.

## RMI exceptions escaped nested invocation boundary — 2026-10-22

- **Symptom:** An uncaught exception from a remote method entered the enclosing
  Java RMI service's exception handlers. VPCD returned `6F00` instead of a
  JCRMI exception response; runtime exception objects could be confused with
  ISOException class records while propagating through those frames.
- **Cause:** Nested `Interpretor` reused the caller's `Context` and the normal
  exception dispatcher searched below the nested invocation's entry frame.
- **Viable fix:** Give the remote invocation's `Bytecodes` dispatcher an
  exception boundary at its entry frame. It still searches handlers inside the
  invoked method, recognizes ISOException before reporting an uncaught
  exception, and returns native API exception metadata to the RMI encoder.
  `./tools/test-rmi-arrays-vpcd` now passes locally caught and unhandled
  exception cases with exact responses.

## JCSystem VPCD regression findings — 2026-10-22

- **Symptom:** The full transaction applet regression stopped at case 5:
  abort persistence expected field value `0x11`, got `0x22`. A separate
  selected run of case 27 left its APDUException sentinel at `0x7F` instead of
  the expected ILLEGAL_USE reason. Focused exception cases 19–22 and 24 passed.
- **Cause:** Not established. These tests use the normal interpreter path, not
  the RMI-specific exception boundary.
- **Use:** Keep both failures visible; rerun focused cases after investigating
  persistent abort restoration and repeated APDU receive state.

## Java Card RMI array signature and temporary allocation — 2026-10-22

- **Protocol:** CAP 2.2 type descriptor nibbles `0xA` through `0xD` encode
  boolean[], byte[], short[], and int[]. RMI array parameters are length plus
  elements, with `0xFF` reserved for null input; null array returns use the
  normal null reference encoding (`81 FFFF`). Java Card RMI forbids object
  parameters. See Java Card VM Spec 2.2 §6.8.1 and JCRE 3.1 §§8.1.1.1,
  8.3.4.2, and 8.3.5.1.
- **Implementation detail:** RMI arguments must be JCRE global arrays, not
  applet-owned arrays. The global heap uses stable compact handles, so freeing
  an element must leave its slot in place and later allocation must reuse the
  empty slot. Removing list entries would invalidate other global handles.
- **Validation:** `tools/test-rmi-arrays-vpcd` passes the current-source CAP
  fixture for byte[], short[], and boolean[] inputs/results, eight array
  parameters, null and empty arrays, and malformed encodings. Int[] remains
  unverified end to end.

## Duplicate deploy CAP after interrupted or reused area — 2026-10-22

- **Symptom:** GPPro upload reports that an API test package is already
  present, so the area is marked `UPLOAD_OTHER` and is not tested. This
  occurred for Security and JavacardxInt on two of the three readers.
- **Cause:** A previous deploy run left its test package installed on the card.
  The parallel runner performs area cleanup after the current area's test but
  cannot remove CAPs left by an earlier interrupted run.
- **Viable recovery:** Per `AlgTest_JavaCard/deploy/README.md` §2, uninstall
  only the matching deploy CAP using GPPro, pinned to the exact reader:
  `java -jar gp.jar -d --uninstall caps/AlgTestApiSecurity_305.cap -r
  "<exact reader name>"`. Then rerun the area. For multiple cards, do not omit
  the reader selector. The Security rerun completed and cleanup passed after
  this recovery. JavacardxInt was then correctly refused with `6A88` because an
  imported package is unavailable.
- **Artifacts:** `artifacts/results/deploy/parallel-20261022-034350/`.

## Persistent primitive GETFIELD skipped restored cache — 2026-10-22

- **Symptom:** Current-source RMI INVOKE of fixture `Counter.add(short)`
  returned `6F00`. The exception trace showed `SecurityException` from the
  dynamic `CounterImpl` method at `getfield_s`, with a valid field token 0 and
  one allocated instance field.
- **Cause:** `JC_Instance::getField_Byte`, `getField_Short`, and
  `getField_Int` read a field tag directly from flash for persistent objects.
  They bypassed `getFields()`, which restores and type-checks the persisted
  field cache and represents fields with no stored value as Java's default
  zero. `getField_Reference` already used this cache path.
- **Viable fix:** Route persistent primitive getters through `getFields()` and
  read the requested slot(s) from the cache. This also avoids a flash lookup
  on every primitive field access. Preserve integer's two-slot representation.
- **Validation:** ARM and RISC-V builds pass; PC build passes. Current-source
  VPCD CAP 2.2 fixture passes RMI SELECT, `Counter.add(short)` and
  `Counter.value()` with exact responses, plus unknown-method and unknown-object
  error responses; `TEST:SUMMARY pass=6 fail=0`. The test increments the
  persistent counter by 3 and reads back 3. No physical board was flashed for
  this fix.
- **Follow-up:** Add focused persistence coverage for default-valued and
  restored byte, short, and int fields, plus malformed stored type/length.

## Chained API areas in parallel workers may block during SmartcardIO — 2026-10-21

- **Symptom:** With three visible physical readers and all three independent
  `run-deploy-parallel --api 305` workers started, Pico 2 Crypto completed,
  while Pico 1 Security, Pico 2 Service, and Pico 3 Javacardx upload stopped
  making progress inside `subprocess.run(...).communicate()` for the official
  Java client / GPPro. Pico 3 Core completed normally. The coordinator was
  interrupted after approximately three minutes; no child process remained.
- **Evidence:** Incomplete logs and CSVs are preserved at
  `artifacts/results/deploy/parallel-20261021-213747/`. This is not a
  completed three-reader run. The earlier complete run at
  `parallel-20261021-211910/` remains valid evidence that the setup can work.
- **Likely investigation:** Check whether `pcsc_scan` reader indexing or
  simultaneous Java SmartcardIO sessions causes selection/transport waits;
  capture per-worker client stdout/stderr and elapsed stages. Do not assume
  these waits are VM applet failures. Retry each affected area on one exact
  reader sequentially after checking and cleaning card registry state.
- **Current safe recovery:** Refresh USB ACLs once with the approved
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` and verify `pcsc_scan -r` lists
  all boards. This restored Pico 3 visibility after its firmware cycle in the
  current session, but it has not been shown to resolve Java-client waits.
- **Sequential retest:** Removed only the orphaned Security/Service test
  packages. Security then completed alone on Pico 1 (901 calls, about two
  minutes) and Service completed alone on Pico 2 (124 calls, about 12 seconds);
  both runner cleanups passed. Their non-OK method results match the earlier
  full matrix. This makes concurrent PC/SC/USB scheduling a plausible cause
  of the waits, but one sequential pass does not establish causation.
- **Next diagnostic:** Try a controlled two-reader run of one short/known
  area on distinct readers. Capture timestamps per stage and do not start a
  third worker until the two-worker run completes. If it stalls, compare with
  sequential behavior and inspect pcscd/USB state before treating it as a card
  runtime defect.
- **Resolution/update:** Two-worker `Security,Service` completed with both
  exit codes 0. A later one-area-per-card three-worker run (`Crypto`, `Core`,
  `Service`) also completed all workers. Core/Multi, Crypto, and Service
  cleanup passed; the only failed stage was optional Ext INSTALL `6A88`, whose
  partial LOAD rollback passed. Final `pcsc_scan -r` listed all three Picos.
  Therefore concurrency itself is supported; avoid chaining many areas on one
  worker until the earlier wait is isolated. For stable parallel coverage,
  batch one area per physical reader and retain the timestamped per-reader
  logs.

## Current three-board PC/SC access blocked by stale installed helper — 2026-10-21

- **Current state:** USB now sees three CHOUPI CCID boards, but `pcsc_scan -r`
  still lists only the original reader. The installed no-argument root helper
  rejects all three as ambiguous (`Expected exactly one ...; found 3`) before
  setting ACLs.
- **Viable fix:** Repository source `tools/pico2-pcsc-reader-root` accepts one
  to three matching Pico nodes and applies all ACLs before restarting pcscd.
  The existing installer safely refreshes the installed root helper under the
  existing sudoers rule: run `sudo ./tools/install-pcsc-sudoers`, then
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`; verify all three with
  `pcsc_scan -r`.
- **Parallel test:** After the helper reports three granted nodes, launch
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305` using the
  elevated host-access execution path. Its default requires three physical
  PC/SC readers and refuses to start workers when any are missing.

## Multi-device BOOTSEL entry and serial-targeted flash — 2026-10-21

- **Symptom:** With several Pico 2 boards attached, picotool could not safely
  select an intended BOOTSEL target through the repository flash wrapper.
  `picotool reboot --usb --force --ser ...` also failed to locate a reset
  interface on the CHOUPI composite device.
- **Viable BOOTSEL entry:** Open the target's stable by-id CDC path at 1200
  baud (`stty -F /dev/serial/by-id/usb-Raspberry_Pi_Pico_*SERIAL*-if00 1200
  raw -echo`). This successfully put both added boards into BOOTSEL without
  writing flash. Always match the full unique board serial; never use a
  wildcard if multiple boards are attached.
- **Viable repository improvement:** `tools/flash`, `tools/monitor`, and
  `tools/cycle` now accept `--serial SERIAL`. Picotool selection arguments
  must come before positional arguments for the installed picotool version.
  `tools/cycle choupi --serial SERIAL` performs both architecture builds,
  flashes only the selected board, and validates bounded CHOUPI runtime output.
- **Observed limitation:** A third board flashed and passed the CHOUPI runtime
  check, but afterward re-enumerated briefly as `2e8a:000f` and disappeared
  from the VM. Current VM inventory sees only two application-mode CHOUPI
  boards. Reattach the third device in the VM before relying on it.
- **PC/SC follow-up:** The deployed root ACL helper is stale and rejects the
  multi-board count. Install the repository revision with
  `sudo ./tools/install-pcsc-sudoers`, then run the approved helper and check
  `pcsc_scan -r`; the updated helper grants pcscd ACLs to one through three
  matching device nodes.

## Reported BOOTSEL boards enumerate as application-mode RP2350s — 2026-10-21

- **Symptom:** The operator reported two additional Pico 2s in BOOTSEL, but
  live elevated host-access checks showed no BOOTSEL device. `lsusb -d 2e8a:`
  listed three `2e8a:0009` devices; `picotool info` identified all three as
  application-mode devices with USB serial connections. The existing BOOTSEL
  expectation is not evidence of guest USB passthrough.
- **Attempted recovery:** A serial-targeted
  `picotool reboot --usb --force --ser E0E14525E64AF6ED` returned
  `Unable to locate reset interface on the device`. Do not keep retrying the
  same command: this application USB interface does not expose the picotool
  reset interface, and no device contents were changed.
- **PC/SC access:** The installed root helper is stale and rejects three
  `2e8a:0009` devices. The repository's updated helper handles one to three;
  install it with `sudo ./tools/install-pcsc-sudoers`, then run the approved
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`. `pcsc_scan -r` currently sees
  only the original physical reader. A non-mutating parallel-runner dry run
  successfully routed that reader by exact name.

## PC/SC ACL helper rejects multiple CHOUPI readers — 2026-10-21

- **Symptom:** After rebooting the two added BOOTSEL devices into their
  existing applications, both exposed CHOUPI CCID interfaces. The approved
  `pico2-pcsc-reader` helper then stopped with “found 2” because it requires
  exactly one `2e8a:0009` device. `pcscd` has `user:pcscd:rw` on the original
  node only; the second node has only the interactive user's ACL. Restarting
  `pcscd` therefore leaves only one physical PC/SC reader.
- **Viable repository fix prepared:** `tools/pico2-pcsc-reader-root` now
  validates and applies the pcscd ACL to all one to three detected CHOUPI
  devices, then restarts pcscd once. `bash -n` passes. It is not installed
  under `/usr/local/sbin` yet; rerun `sudo ./tools/install-pcsc-sudoers` to
  deploy it under the already authorized no-argument sudoers rule, then run
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` and verify with `pcsc_scan -r`.
- **Board state:** USB serials `D92E743D5F640C0F` and `E0E14525E64AF6ED` expose
  CHOUPI CCID. Serial `5CFC5328F5BDE437` boots generic Pico firmware with no
  CCID interface and needs CHOUPI firmware before joining tests. No board was
  flashed during this step.

## Host VPCD package lookup must include ROM-mask packages — 2026-10-21

- **Symptom:** A JXExt method call returned `6F00` while resolving an imported
  catch type. `Import_Handler::getPackageIndex()` scanned only package IDs set
  in the mutable package bitmap, but PC VPCD stores built-in ROM-mask CAPs in a
  separate host image.
- **Viable host-side fix:** In `FlashMemory_Handler::isPackageExist()`, treat a
  nonempty `choupi_pc_rom_package_data(id)` result as a resident package before
  checking the mutable bitmap. This makes import lookup see the same PC ROM
  packages that `getCap()` can read. Core 3.0.5 VPCD install/SELECT still
  passes (`TEST:SUMMARY pass=11 fail=0`).
- **Related version rule:** Imported packages can link against a resident
  package with the same AID and major version when the imported minor version
  is no newer than the resident one. `getPackageIndex()` now applies that
  compatibility rule instead of requiring exact version equality. The Java
  Card VM specification describes package-version compatibility in §4.5.2:
  https://docs.oracle.com/en/java/javacard/3.1/jc-vm-spec/F12650_05.pdf
- **Remaining failure:** The JXExt CAP imports `javacardx.external` AID
  `A0000000620203` version 1.0, and the built ROM mask does not contain that
  AID. Its call still returns `6F00` after the host lookup fix. This is an
  unresolved CAP dependency/profile gap; validate imports during INSTALL and
  decide separately whether the API package belongs in the ROM profile. Do
  not report the JXExt APDU as fixed or hardware-verified.

## Dynamic applet virtual method token translated twice — 2026-10-21

- **Symptom:** A fresh physical deploy Core 3.0.5 run returned
  `EXC_RUNTIME` for inherited `Applet.select`, `deselect`, `register`,
  `selectingApplet`, and related probes, despite successful CAP installation.
- **Cause:** The receiver dispatch path remapped an external method token
  before looking in the dynamic CAP class table. When lookup reached the
  embedded framework superclass, it remapped the same token again.
- **Viable fix:** For a dynamic receiver retain the CAP's original token and
  remap only at the external superclass boundary. Keep direct embedded
  receiver remapping and Applet's public/protected bit normalization.
- **Physical validation:** Core+Multi+Ext completed 291 calls before and after
  the change. OK rose from 256 to 267, and the nine Applet method failures
  cleared. Both architectures built, the ARM UF2 passed BOOTSEL readback
  verification, and a bounded reboot log had seven runtime passes. Five Core
  failures remain in OwnerPIN and Shareable paths.

## Preserve CAP2 image while placing two JFS2 slots — 2026-10-21

- **Discovery:** The physical card backup has a valid, distinct CAP2 package
  at `0x1c0000`; the previous new JFS2 slot at `0x1c4000` would have
  overwritten part of its code. The startup guard stopped USB CDC servicing.
  A verified post-transfer image showed storage `0x100000..0x200000` remained
  byte-identical to the pre-flash backup.
- **Viable source fix:** CAP2's declared maximum is a 4 KiB header plus 28 KiB
  code, ending at `0x1c8000`. Place JFS2 slots at
  `0x188000..0x1b0000` and `0x1c8000..0x1f0000` (each `0x28000`).
  Preserve `0x1c0000..0x1c8000` and remove the CAP2 startup guard. The
  shared-flash test now covers migration and second-slot sync with simulated
  CAP2 bytes unchanged. CAP2 remains stored but is not interpreted by CAP3.
- **Caveat:** Old firmware that writes its 256 KiB CAP2 erase slot could erase
  the new JFS2 slot 1. Keep the full backup before any rollback.
- **Physical validation:** Revised ARM firmware was flashed with verification.
  A verified full-flash readback found new JFS2 generation 11748 with the
  exact old generation-11747 payload, while CAP3/old-JFS2 and CAP2 bytes were
  unchanged. Two bounded serial boots passed, and GPPro listed the same four
  loaded packages and selectable applets after the second boot. The migration
  print was missed by CDC attach timing; flash CRC/readback provides evidence.

## BOOTSEL picotool transfer disconnect after 100% — 2026-10-21

- **Symptom:** During `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi`, both
  architecture builds passed and `picotool load -f -x` reached 100%, then
  aborted with `picoboot::connection_error` before confirming reboot. The
  wrapper's bounded BOOTSEL and CDC recovery saw no Pico, and authorized
  `lsusb -d 2e8a:` returned no device. No serial result was captured.
- **Likely cause / viable recovery:** USB passthrough may have detached when
  the device changed modes; the host previously needed a manual detach and
  reattach. Reconnect the Pico in BOOTSEL and verify host visibility before
  another read or flash. Do not infer success from the 100% progress display;
  first compare the flash with the pre-flash backup and inspect a bounded
  runtime log. This recovery has not yet been confirmed for this attempt.
- **Data protection:** A full 2 MiB `picotool save -a -v` backup was verified
  before flashing at `artifacts/backups/flash-before-layout-migration-20261021.bin`
  (SHA-256 `408ba9dff6edf6924830d6ae9e9860a1aa7fef40fe6976ebe9a2ee736e9bc4b6`).
  Both old JFS2 snapshots and a distinct, valid CAP2 image were present.
- **Follow-up:** After manual reattachment, explicit `--bus 1 --address 17`
  `picotool info -a` identified the new ARM firmware. A second verified full
  flash image showed storage `0x100000..0x200000` was byte-identical to the
  pre-flash backup. The application enumerated as CCID, but the CAP2 guard
  stopped USB servicing before serial could be configured.

## CAP3 package slots overlap RP2350 filesystem snapshots — 2026-10-21

- **Symptom/risk:** Current source allocates CAP slots over the same flash
  offsets as both filesystem snapshots. CAP erase/program and filesystem sync
  can destroy each other's records; physical corruption has not been observed
  or reproduced in this review.
- **Cause:** `src/cap_store.cpp` uses eight `0x11000` slots starting at
  `0x100000`, ending at `0x188000`. `src/choupi_pico2.cpp` still uses snapshot
  slots `0x100000..0x140000` and `0x140000..0x180000`. Its non-overlap assert
  compares only with the old CAP boundary `0x1c0000`. Configured flash is
  2 MiB; the existing firmware reservation plus full CAP and FS capacities
  exceed it by `0x8000` even with perfect packing.
- **Status/solution:** Viable source fix implemented in `src/flash_layout.hpp`:
  CAP3 remains at `0x100000..0x188000`; JFS2 moves to two `0x3c000` slots at
  `0x188000..0x200000`. Startup migrates a valid old JFS2 snapshot from RAM
  to the new first slot without erasing the old snapshot. The fake-flash suite
  now links the real CAP3 store and checks all eight CAP slots, both snapshots,
  and migration power cuts. A missing/corrupt snapshot with CAP3 or a JFS2
  header fails closed. Historical CAP2 magic at `0x1c0000` also fails closed
  because that format is not automatically converted. See
  `docs/flash-layout-migration.md` before physical deployment.
- **Evidence:** Source inspection and address arithmetic in
  `review_20261021.md`; overlap is also present in commit `9c7b9ae`.

## Imported interface resolution can contaminate later applet interface lookups — 2026-10-21

- **Symptom:** Deploy Core 3.0.5 Shareable probe APDU `B083210000` returns
  `04 9000`. A diagnostic callback returning the selected applet reached
  `invokeinterface`, which raised `SecurityException` (internal enum 29).
- **Cause:** `Class_Handler::getImplementedInterfaceMethodOffset()` reused one
  `ConstantPool_Handler` while resolving an implementation's interface table.
  Resolving an external framework interface changes the resolver package, so
  a following internal applet interface can be resolved against package 0
  instead of its declaring CAP package.
- **Candidate fix:** Construct a fresh constant-pool resolver for each
  implemented interface, rooted at the concrete class package. This change is
  present in `choupi/src/jc_handlers/jc_class.cpp`; a host build passed, but
  the end-to-end Shareable APDU still returns `04 9000`. It is not yet a
  verified resolution.
- **Next diagnostic:** Trace the requested and table interface package/class
  identities after the fix, then trace callback return and `invokeinterface`
  dispatch to the precise exception. Reproduce on host VPCD first; physical
  hardware was not visible on 2026-10-21. Remember that the current host VPCD
  storage shim supports one package and one applet instance only.

## PC VPCD storage shim is single-package — 2026-10-21 (updated 2026-10-25)

- **Symptom:** Host VPCD tests cannot exercise two distinct uploaded CAP
  packages, even though the Pico card manager has eight package slots.
- **Cause:** `choupi/src/main_pc.cpp` owns one `vpcd_package_id`, one package
  AID, and one installed-instance record. Its package lookup and metadata
  callbacks reject other package IDs and do not maintain a package registry.
- **Resolution:** The PC backend now stores CAP packages by ID and AID, keeps
  multiple applet instances per package, and enumerates host packages and
  instances in GET STATUS pages. The restore callback takes the resolved
  package ID from `CapLoadSession`; do not use the CLI's initial package ID for
  a package allocated by LOAD.
- **Validation:** PC, ARM, and RISC-V builds pass. The JCAlgTest VPCD
  two-process restore test passes 30/30 checks. Multi-package GPPro validation
  is not yet complete: `VpcdPresenceProbe` reported that `Virtual PCD 00 00`
  is not registered with system PC/SC, before any CAP was uploaded. The host
  registry is process-local; multi-package process-restart reconstruction is
  still open.

## VPCD SmartcardIO reader absent; bound PC/SC probes — 2026-10-25

- **Symptom:** A GPPro integration run stopped before card setup because
  `VpcdPresenceProbe` could not find `Virtual PCD 00 00`. A subsequent
  unbounded `pcsc_scan -r` attempt emitted repeated `SCardEstablishContext:
  Access denied` / invalid-context messages until interrupted.
- **Cause:** This VM currently has no registered virtual PCD reader in system
  PC/SC; a raw VPCD socket test does not require that reader, but GPPro through
  SmartcardIO does.
- **Viable guidance:** Use the existing raw-socket VPCD harness for VM/APDU
  checks when PC/SC has no virtual reader. Before running SmartcardIO tools,
  require a bounded `VpcdPresenceProbe` to confirm the exact reader exists.
  Keep any direct PC/SC diagnostic bounded and redirect its output to a log.
- **Validation:** `PICO2_HOST_USB_ACCESS=1 ./tools/test-pc-vpcd-applet
  --passes 2 --log artifacts/logs/host-vpcd-multipackage-registry-20261025.log`
  passed; the GPPro reader probe failed before LOAD. The noisy scan was
  interrupted and is not evidence of card state.

## Util same-array overlapping copy — 2026-10-21

- **Symptom/cause:** Physical API coverage found that byte-array `Util.arrayCopy`
  and `arrayCopyNonAtomic` copied forward even when the destination overlapped
  the source at a higher offset. Destination writes overwrote bytes that had
  not yet been read.
- **Viable solution:** Use reverse copy order for the same-array case when
  `destOff > srcOff` and the ranges overlap; retain forward order otherwise.
  This gives the specified temporary-array behavior without allocating a
  second array.
- **Validation:** Flashed ARM firmware to the physical Pico 2, captured
  `TEST:PASS choupi_runtime`, then ran the deploy Core API 3.0.5 client through
  physical reader index 3. Both overlapping-range rows report `OK` with the
  expected returned offset and effect. Evidence:
  `artifacts/results/deploy/pico2-overlap-retest-20261021.csv`.

## JC305 OwnerPIN and PIN dispatch used embedded tokens — 2026-10-21

- **Symptom/cause:** The physical deploy Core API run reported OwnerPIN
  construction and methods as `EXC_RUNTIME`/SW `6F00`, with a wrong result for
  `getValidatedFlag`. The reference JC305 framework export assigns `OwnerPIN`
  class token 9 and orders its virtual methods as `equals`, `check`,
  `getTriesRemaining`, `getValidatedFlag`, `isValidated`, `reset`,
  `resetAndUnblock`, `setValidatedFlag`, `update`. The embedded method table
  uses a different order. The JC305 `PIN` interface also orders `check` before
  `getTriesRemaining`, opposite the embedded interface.
- **Viable solution:** Map JC305 OwnerPIN virtual tokens to the embedded
  OwnerPIN method table and map the PIN interface tokens independently. Keep
  OwnerPINx's separate map, since its own export order differs too.

## ROM-to-dynamic virtual override needs inverse CAP token mapping — 2026-10-21

- **Symptom:** The official Core client reported `OwnerPIN.getValidatedFlag()`
  false after a successful `check()`, although the related `isValidated()`
  result was true.
- **Cause:** Embedded ROM framework methods use embedded API virtual tokens,
  while a dynamic applet's override table uses the importing CAP's token
  ordering. Dispatch based on argument count can select the wrong override
  when two methods have the same arity. Trace showed the embedded declaration
  at offset 795 but dispatch selected dynamic offset 741 for method token 3.
- **Viable fix:** For public virtual calls from ROM to a dynamic receiver,
  resolve through `getMethodOffset(..., true)` and use that inverse-mapped
  token. After this change, the resolved offset was 795 and the focused
  OwnerPIN before/after-check probes returned `0`/`1` (`TEST:SUMMARY pass=11
  fail=0`). Host VPCD evidence only; ARM and physical validation remain open.
- **Caution:** `bc_invoke.cpp` still has temporary `INVOKEVIRTUAL_*`
  diagnostics and the PC host build is currently debug. Remove only those
  diagnostics, preserve the mapping fix, rebuild normally, and rerun focused
  regressions before relying on the fix. See `.agent/HANDOFF.md` for the
  official Core client cutoff at `APDUException.IO_ERROR`.

## Deploy Core IO_ERROR result was a client timeout, not a bridge defect — 2026-10-21

- **Correction:** The earlier report that the official PC/SC/VPCD client lost
  transport at `APDUException.IO_ERROR` was premature. A 900-second full Core
  run had only reached 183 calls before its external timeout.
- **Viable diagnosis:** Run the official client with an adequate timeout or
  filter by API class using `-Dapitest.classes=0x13` to isolate
  `APDUException`. The isolated official run completed all 14 constructor and
  `throwIt` rows, including `IO_ERROR` (returned reason `0x0004`), with
  `Tested calls; 14; not OK; 0`. The full Core run then completed in a 2400s
  bound; its summary reported 291 calls, 11 allowed unsupported, and zero
  wrong results.
- **CSV count audit:** The full CSV contains 291 detail rows: 272 `OK`, 11
  `UNSUPPORTED_PER_SPEC`, and 8 `APPLET_NOT_INSTALLED`, matching the client
  summary. Count both `javacard.` and `javacardx.` prefixes; matching only
  `javacard.` omits the single `ExtendedLength` row.
- **Scenario limit:** The host VPCD emulator hosts one package/applet at a
  time, so the full Core run's Multi and Ext scenario entries could not be
  selected. Independent VPCD smoke sessions for Multi AID A and B each passed
  SELECT (`9000`); they do not prove concurrent coexistence. Do not attribute
  the scenario failures to `APDUException` or the bridge.
- **Validation:** Rebuilt and flashed ARM firmware to the physical Pico 2;
  finite serial output reported `TEST:PASS choupi_runtime`. Deploy Core 3.0.5
  then passed all tested OwnerPIN constructor, update, check, state, and `PIN`
  interface rows. The complete direct Core run reported 268 OK, 15
  `UNSUPPORTED_PER_SPEC`, 3 `EXC_RUNTIME` (UserException probes), 4 Ext
  `APPLET_NOT_INSTALLED` rows (`6999`), and one remaining wrong Shareable
  result. Evidence:
  `artifacts/results/deploy/pico2-ownerpin-map-retest-20261021.csv`.

## CAP INSTALL and SELECT must resolve the package by the request/instance AID — 2026-10-21

- **Symptom/cause:** GPPro can power-cycle between LOAD and INSTALL. On power-up,
  `restoreInstalled()` changes the active loader package to an already installed
  applet's package. Comparing the next INSTALL package AID with that transient
  value caused Multi applet creation to fail `6A80`. SELECT then failed `6A82`
  because the old global CAP scan did not find applets in the dynamic package
  slots.
- **Viable solution:** Resolve every INSTALL package from the package AID in
  that command, then validate it against the stored CAP header. For SELECT,
  resolve the instance AID through the persisted package instance record and
  read that applet's install offset from the stored CAP Applet component.
- **Validation:** On the physical Pico 2, LOAD of `AlgTestApiMulti_305.cap`
  succeeded, then separate GPPro `--create` commands for applets A and B both
  returned `9000` across power-ons. `gp --list` showed both selectable AIDs;
  raw SELECT of each returned `9000`. The deploy Core client then selected its
  applet and completed 291 API calls. See
  `artifacts/results/deploy/pico2-core-client-20261021.csv`.

## Deploy API runner can report partial CAP install as success — 2026-10-21

- **Symptom/cause:** GlobalPlatformPro prints `CAP loaded` after a successful
  LOAD phase even if the following INSTALL command fails. The runner previously
  checked for `CAP loaded` before checking explicit `INSTALL failed`, so it
  reported UPLOAD OK and ran a client against an applet that was not installed.
- **Viable solution:** Classify explicit LOAD/INSTALL/DELETE failures before
  success markers. The runner's `--reader-index` must also match the SmartcardIO
  prompt: with two empty VPCD readers listed first, the physical Pico was index
  3 in this VM.
- **Validation:** A classifier check now returns `refused_install` for
  `CAP loaded` followed by `INSTALL ... failed: 0x6A82`. A real Javacardx CAP
  failure was recorded as UPLOAD_REFUSED_INSTALL after the change. Core,
  Security, and Crypto applet client runs use reader index 3.

## GP DELETE can return 6581 after API applets write persistent state — 2026-10-21

- **Symptom/cause:** Physical GPPro package deletion returned `6581` after the
  Core and Service API clients had run. Deletion succeeded for JavacardxExt,
  Javacardx, JavacardxInt, and JavacardxSec packages without persisted applet
  data. The failing persistent cleanup substep has not yet been isolated.
- **Workaround:** Remove a loaded package without an installed instance when
  possible; those package-only deletions reclaimed their CAP slots. Do not
  treat a failed cleanup record as success or reuse that test slot.
- **Next:** Inspect transaction snapshot/erase behavior with applet data, then
  rerun package deletion and the deploy runner's staged cleanup.

## GPPro package deletion requires removing its applet instance first — 2026-10-20

- **Symptom/cause:** On the physical CHOUPI card, GlobalPlatformPro package
  deletion for AID `4A43416C6754657374` returned `6985` while registered applet
  instance `4A43416C675465737431` still existed.
- **Viable solution:** Delete the applet instance AID first, then delete the
  package AID. On this firmware both commands returned `9000`; GPPro `--list`
  then showed only the ISD. Reinstall the supplied CAP with `--install`.
- **Validation:** The exact JCAlgTest 1.8.2 JC305 CAP reloaded successfully;
  LOAD blocks and final INSTALL returned `9000`. The applet was subsequently
  listed as `SELECTABLE`, and SELECT, GETVERSION, and JCSystem info all returned
  `9000`. See `artifacts/logs/gppro-clean-install-*-20261020.log` and
  `artifacts/logs/pico-powerup-persistence-20261020T135329Z.log`.

## Runtime-exception root failure test pulls full VM dependencies — 2026-10-20

- **Symptom/cause:** Calling `Heap::addRuntimeException()` from the small
  allocation-test target made the linker retain `Heap::addInstance()` and its
  `JC_Instance`, CAP metadata, and filesystem dependencies, which that target
  intentionally does not link.
- **Viable approach:** Keep direct allocator injection on the isolated
  `reserveStaticReferenceRoot()` API. Runtime exception-root ordering is
  compiled for both target architectures and should receive a dedicated
  integration or dependency-complete heap test if direct failure coverage is
  needed later; do not add test-only VM stubs to the small allocation target.
- **Validation:** After removing the un-linkable runtime-exception test attempt,
  `./tools/test-jcvmarray-allocation` passed all nine existing checks.

## Persistent static reference could commit before GC root registration — 2026-10-20

- **Symptom/cause:** `Static_Handler::setPersistentRef()` wrote a serialized
  reference before `Heap::setStaticReferenceRoot()` appended the in-memory GC
  root. If that vector growth failed, a persistent static could retain a
  transient object's handle without keeping the object reachable.
- **Viable solution:** Add a heap API that reserves root-vector capacity for a
  `(package, field)` pair, and call it before writing the persistent field.
  Existing roots need no allocation. After the write succeeds, setting the root
  uses the already-reserved slot.
- **Validation:** ARM/RISC-V builds and the persistent-transaction test passed.
  Two-process VPCD persistence passed install/commit 3/3 and fresh restore 7/7,
  including static reference identity and persistent array aliases. The
  allocation regression now injects failure into root-vector reserve, verifies
  no root is registered, then verifies retry and allocation-free repeat reserve.

## Exception object inserted before its GC root could be allocated — 2026-10-20

- **Symptom/cause:** `Heap::addRuntimeException()` inserted a new exception
  instance into the heap before appending its handle to the runtime-root vector.
  If vector growth then failed under memory pressure, the instance remained
  unrooted and the next exception attempt could allocate another one.
- **Viable solution:** Reserve one additional root-vector slot before creating
  the exception instance. The subsequent trivial root append then requires no
  allocation, while reserve failure occurs before the heap is mutated.
- **Validation:** ARM and RISC-V builds passed. The host VM build passed with
  existing Java Card converter, Maven/network, and packed-CAP warnings. The
  local VPCD integration attempt connected but did not complete applet restore;
  no APDU or exception-root behavior was verified. Allocation failure at the
  reserve boundary is not directly injected yet.

## Allocation-failure test optimized away an unused JCVMArray copy — 2026-10-19

- **Symptom/cause:** An injected `new[]` failure did not fire in the copy-
  constructor regression when the newly copied object was unused. The host
  compiler eliminated the unobservable copy and its replaceable allocation.
- **Viable solution:** Make the success path observe copied state (for example,
  size and an element) before asserting the allocation failure. This keeps the
  allocation semantically required and makes the global `operator new[]`
  injection deterministic.
- **Validation:** `./tools/test-jcvmarray-allocation` passes the copy-
  construction failure case when the copy's size and first value are read.

## JCSystem.isAppletActive must include registered install/select callbacks — 2026-10-19

- **Symptom:** The first lifecycle implementation returned false while the
  registered applet ran `install()` and `select()`, because the runtime marked
  the applet active only after `select()` returned true.
- **Contract:** The bundled JC 3.0.5 `JCSystem.java` Javadoc states that a
  successfully registered applet returns true even in `Applet.install()` and
  `Applet.select()`. Oracle's published Java Card API documents the same rule:
  <https://docs.oracle.com/en/java/javacard/3.2/jcapi/api_classic/javacard/framework/JCSystem.html
- **Viable solution:** During install, accept `isAppletInstallationActive()`
  only after registration. Set the context's active flag before entering
  `Applet.select()` and clear it if selection throws or returns false. Keep it
  set throughout `deselect()` and clear it after that callback completes.
- **Validation:** Focused VPCD cases 76 and 77 passed for install/select/
  deselect/process callbacks; rejected-selection case 56 passed. ARM/RISC-V
  builds and the physical CHOUPI runtime cycle passed.
- **Scope:** This runtime still supports one applet context and the basic
  logical channel only. Physical applet APDUs were not validated in this step.

## JCSystem.getAID exposed an unregistered instance during install — 2026-10-19

- **Symptom:** The native `getAID()` bridge created an AID object from the
  INSTALL instance AID before the applet had called `Applet.register()`.
- **Contract:** JC 3.0.5 `JCSystem.getAID()` returns null until the applet
  successfully registers. During installation the instance AID is already
  available to the card manager, so the native bridge must check registration
  state before constructing the Java AID object.
- **Viable solution:** Return a null reference unless the current context has
  successfully registered the applet or restored an installed applet instance.
  Then use the existing cached AID construction path.
- **Validation:** Focused VPCD install and fresh-process restore checks passed;
  the regression observes null before registration and a matching AID after
  registration and restore. ARM/RISC-V builds and physical CHOUPI runtime cycle
  passed.
- **Scope:** The runtime still has one applet context. Physical Java applet
  APDUs remain blocked by the host PC/SC transmit stall documented above.

## Pico reader disappears from PC/SC during second applet APDU — 2026-10-19

- **Observation:** After the authorized USB ACL helper ran, bounded
  `pcsc_scan -r` listed the Pico. GPPro listed the JCAlgTest package as LOADED
  and its applet as SELECTABLE. The physical probe read ATR `3B800181` and
  SELECT returned `9000`, but the following GETVERSION transmit failed with
  `SCARD_E_NOT_TRANSACTED`. A retry found no Pico reader in PC/SC. `lsusb`
  still showed `2e8a:0009`, and the finite serial monitor showed repeated
  `TEST:PASS choupi_runtime` with no `FAULT`.
- **Interpretation:** This is a post-SELECT PC/SC/CCID transport failure, not
  evidence that the package or applet registry is missing. Cause is unknown;
  the wrapper did not complete process/persistence APDUs, and no actual power
  cycle was performed.
- **Follow-up after a fresh firmware cycle:** The helper and bounded
  `pcsc_scan -r` listed the Pico, but `probe-jc-algtest --version-only` timed
  out (exit 124) after ATR and before SELECT response. An eight-second serial
  capture showed CCID `stage=2`, `rx_events=0`, `tasks=0`, `xfr=0` and repeated
  runtime `TEST:PASS`; this attempt delivered no XfrBlock to firmware. See
  `artifacts/logs/physical-jcsystem-active-retry-20261019.log`.
- **Recovery/status:** Re-running the authorized ACL helper made the Pico
  visible briefly, but did not prevent the next-APDU failure. Stop repeated
  applet retries; preserve the bounded probe log, USB enumeration, and serial
  evidence. If it recurs, have the operator restart `pcscd.service`, rerun the
  authorized helper, confirm the reader, and retry only the bounded probe.
  No durable fix is established.
- **Evidence:** `artifacts/logs/pico-powerup-persistence-20261019T183156Z.log` and
  `artifacts/logs/physical-jcsystem-active-retry-20261019.log`.

## CAP installation metadata can outlive a failed filesystem commit — 2026-10-19

- **Symptom/risk:** `CapLoadSession::installAndMakeSelectable()` writes the
  installed instance AID and class index into the separate CAP flash slot before
  `commitPersistentWriteTransaction()` performs its final `fs_sync()`. If that
  final sync fails, the VM transaction rolls its filesystem records back, but
  the CAP slot header remains marked installed.
- **Evidence:** In `src/cap_load.cpp`, the initial `fs_sync()` is followed by
  `choupi_store_cap_instance()` and then the transaction commit. The Pico
  implementation maps that call to `cap_store_mark_installed()` in
  `src/choupi_pico2.cpp`; the function erases and rewrites the CAP header
  sector. `FlashMemory_Handler::commitPersistentWriteTransaction()` in
  `choupi/src/jc_handlers/flashmemory.cpp` calls `fs_sync()` and rolls back its
  in-memory snapshots on failure. The CAP header is outside those snapshots.
- **Current mitigation:** If `commitPersistentWriteTransaction()` reports a
  final-sync failure after the CAP instance record was written, the install
  path now removes that instance record from the CAP header and syncs the
  metadata store. This prevents the reported returned-failure case from leaving
  a selectable registry entry after the filesystem snapshot rolls back.
- **Validation/status:** The persistent-transaction test injects a filesystem
  commit-sync failure and confirms its records roll back. The RP2350 overlay
  test now confirms a CAP instance record can be removed while retaining its
  package CAP. The full `CapLoadSession` path has not yet been run with an
  injected final-sync failure, and no real power cut has tested the boundary
  between the two stores.
- **Next:** Add fault injection around the full install path. A durable
  intent/reconciliation protocol is still needed to cover a power loss between
  filesystem and CAP metadata writes; two separate flash regions do not form
  one transaction.
- **Related validation limit:** The latest bounded `pcsc_scan -r` saw only
  `Virtual PCD 00 00` and `Virtual PCD 00 01`, so no physical APDU test ran.

## CAP Method component size was decoded in host byte order — 2026-10-19

- **Symptom:** Installing the enlarged JCSystem lifecycle probe failed at
  `INVOKESPECIAL` with `IndexOutOfBoundsException`. The CAP encoded a 4,102-byte
  Method component, but the host runtime exposed only 1,552 bytes; a valid
  method offset of 4,089 was rejected.
- **Cause:** `jc_cap_method_component::methods()` used the raw two-byte CAP size
  field without `NTOHS()`. It also failed to subtract the one-byte
  `handler_count` field when computing the method code length. The field is
  network byte order, and the component size includes `handler_count`, the
  exception-handler table, and method code.
- **Viable solution:** Convert the component size with `NTOHS(size)`, subtract
  `sizeof(handler_count)` and the complete exception-handler table, then expose
  the remaining method bytes. Do not base method bounds on the raw host-endian
  field.
- **Validation:** The 4,102-byte CAP installed and the full VPCD JCSystem/APDU
  lifecycle suite passed 75 checks (`elapsed_s=276.309`). ARM M33 build passed.
  The generated probe CAP is about 4.1 KiB in its Method component; smaller
  earlier CAPs did not expose the truncation.
- **Scope:** This affects every dynamically loaded CAP whose Method component
  crosses the mistaken byte-order bound. Malformed-component underflow checks
  remain part of the CAP validation/hardening backlog.

## USB-visible Pico omitted from PC/SC reader list — 2026-10-19

- **Observation:** `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` refreshed
  the authorized ACL for `/dev/bus/usb/001/004`, while GPPro listed only
  `Virtual PCD 00 00` and `Virtual PCD 00 01` through three registry retries.
  The probe exited before package or applet APDUs. See
  `artifacts/logs/pico-powerup-persistence-20261019T111724Z.log`.
- **Interpretation:** USB enumeration/ACL success does not establish that
  pcscd registered the Pico reader. This attempt does not indicate an applet
  persistence failure.
- **Recovery:** Still unresolved. Restart `pcscd.service` interactively, run
  the authorized `/usr/local/sbin/pico2-pcsc-reader` helper, then use a bounded
  `pcsc_scan -r` to confirm the named Pico reader before retrying.
- **Limit:** Do not run the applet persistence assertion until the Pico reader
  appears in PC/SC; the latest run did not reach card APDUs.

## Pico PC/SC lifecycle verified after CDC/BOOTSEL flash cycle — 2026-10-19

- **Observation:** After the `cycle choupi` path recovered through CDC 1200-baud
  BOOTSEL entry, PC/SC listed the Pico and consecutive GPPro / SmartcardIO
  exchanges completed. GPPro authenticated applet and related-object package
  DELETE, the GET STATUS listings reflected each result, CAP reinstall passed,
  and after the next firmware restart the package/app remained registered. The
  applet SELECT, GETVERSION, JCSystem, and SHA-256 probes then passed.
- **Reset limitation:** Direct `picotool reboot -f` reported
  `Unable to locate reset interface`. The repository `cycle` wrapper's CDC /
  BOOTSEL fallback reflashed the image and rebooted it successfully. This
  verifies restart persistence, not removal of USB power.
- **Recovery status:** This is one successful recovery sequence, not proof that
  the earlier post-SELECT PC/SC stall is permanently fixed. Reuse the bounded
  `tools/test-pico-powerup` probe after boot and record any recurrence. Do not
  run an unbounded scanner.
- **Evidence:** `artifacts/logs/cycle-choupi-20261019T065052Z.log`,
  `artifacts/logs/gp-delete-physical-20261019.log`, and
  `artifacts/logs/pico-powerup-persistence-20261019T065455Z.log`.

## ROMmask source edits skipped by build cache — 2026-10-24

- **Symptom:** Editing the ROMmask generator did not invalidate the cached
  PC-generated `flash` and `jni.hpp`; the firmware wrapper compared API source
  and support configuration hashes only.
- **Cause:** The generation manifest did not include ROMmask/capmap source
  fingerprints.
- **Fix:** `tools/rommask-source-hash` fingerprints generator inputs, excluding
  generated `target`, `build`, and `.git` trees. `tools/build-pc` records this
  in the generation manifest and `tools/build` compares it before deciding to
  reuse generated files. When changing ROMmask generation, verify the source
  fingerprint appears in `build/choupi-pc/rommask-source.sha256` and the
  generated header contains the expected new output.

## Native CryptoException dispatch and RSA NoPad range check — 2026-10-24

- **Symptom:** C++ native implementations could throw
  `Exceptions::CryptoException`, but the interpreter did not convert that
  into a Java Card `CryptoException` object. Native code therefore lacked a
  catchable exception path with a reason field.
- **Fix:** `Context` now carries a pending CryptoException reason, and
  `Bytecodes::dispatchJavaException` resolves the applet's imported Security
  package, gets the runtime-owned exception reference, stores the reason, and
  dispatches it through normal Java exception handlers. Native SHA, random,
  digest, cipher, MAC, and RSA methods now set the reason before throwing
  `Exceptions::CryptoException`.
- **Regression found by physical `/deploy` Crypto:** RSA `ALG_RSA_NOPAD`
  accepted an input equal to the modulus and returned zero. Mbed TLS performs
  modular reduction for `x >= n`; this is an illegal operation for Java Card
  raw RSA. Reject it before `mbedtls_mpi_exp_mod` with
  `CryptoException.ILLEGAL_USE` (reason 5).
- **Validation:** Both architecture builds passed. On Pico
  `2BED26ED20556535`, the original Crypto area had one `WRONG_RESULT` for the
  modulus-equal input. After range validation and reason mapping, the same
  466-call area had 174 `OK`, 286 `UNSUPPORTED_PER_SPEC`, 6 expected
  `EXC_CARDRUNTIME`, and zero `WRONG_RESULT`. Three invalid one-shot signature
  calls now match expected `CryptoException.ILLEGAL_USE` reason 5 instead of
  returning reason 1. Logs/results are recorded in root `progress.md`.

## Post-SELECT PC/SC timeout repeated after 2026-10-19 reflash

- **Observation:** ARM and RISC-V builds passed and the ARM image flashed from
  BOOTSEL. The bounded serial capture reported repeated runtime passes and no
  fault. The ACL helper found the new USB node and PC/SC listed the Pico;
  ATR and SELECT succeeded (`3B800181`, `9000`), but the probe timed out before
  GETVERSION (15-second wrapper exit 124).
- **Interpretation/action:** This repeats the host-side post-SELECT stall
  already described below. A visible reader and successful SELECT are not
  enough to begin a long test. Stop physical retries and continue on VPCD until
  consecutive APDUs work.
- **Evidence:** `artifacts/logs/cycle-choupi-20261019T035222Z.log`; the probe
  output was captured in the terminal, not archived.

## Post-SELECT PC/SC timeout repeated after successful reflash — 2026-10-18

- **Observation:** A fresh ARM build/flash cycle passed runtime self-checks.
  After refreshing USB access, Java listed both Virtual PCD readers and the
  Pico reader, ATR was `3B800181`, and SELECT returned `9000`. The bounded
  probe then timed out before GETVERSION (15-second wrapper exit 124).
- **Interpretation:** This reproduces the existing PC/SC transport symptom
  after reflash; firmware starts and handles SELECT. USB enumeration and ACL
  success do not ensure that the host sends a second APDU. No static workload
  was started.
- **Action:** Stop physical retries at this boundary and continue independent
  VM work through VPCD. Resume physical probing only after a host-side PC/SC
  transaction recovery establishes consecutive APDUs.
- **Evidence:** `artifacts/logs/cycle-choupi-20261018T194931Z.log`; the focused
  probe output was collected in the terminal and was not archived.

## PC/SC stops sending APDUs after SELECT — 2026-10-18

- **Symptom:** After `pcscd.service` restart, Java SmartcardIO enumerates the
  Pico and SELECT returns `9000`, but its next `SCardTransmit` fails with
  `SCARD_E_NOT_TRANSACTED` or times out. The Pico remains present as
  `2e8a:0009` and `pcscd.service` stays active.
- **Isolation:** A concurrent serial trace logged GetSlotStatus traffic and one
  XfrBlock for SELECT (`00A404...`, 16 bytes, `9000`). The CCID receive/task
  count reached 30, but `xfr=1`; no GETVERSION XfrBlock arrived. The runtime
  continued passing its periodic self-check. This places the observed failure
  between the host PC/SC API and CCID OUT endpoint after the first response; it
  does not establish a VM/APDU-handler failure.
- **Recovery status:** Restarting the daemon restored reader enumeration but
  did not make consecutive APDUs reliable. The authorized ACL helper and USB
  visibility alone are insufficient. Avoid unbounded `pcsc_scan`; it has
  previously saturated the VM. Find a host-side PC/SC recovery or inspect
  daemon/driver diagnostics before rerunning long JCAlgTest operations.
- **Evidence:** `artifacts/logs/monitor-pcsc-probe-concurrent-20261018.log` and
  `artifacts/logs/monitor-pcsc-after-restart-20261018.log`.

## Direct Pico-to-VPCD bridge requires exclusive USB interface access — 2026-10-18

- **Symptom:** `tools/bridge-pico-ccid-vpcd` first failed because port 35963
  refused a connection. After a bounded Java SmartcardIO presence query, the
  VPCD listeners on ports 35963/35964 appeared, but the bridge failed with
  `libusb claim interface 2 failed: -6`.
- **Cause/limit:** The system PC/SC daemon owns the Pico CCID USB interface
  while managing the physical reader. The direct libusb bridge cannot run
  concurrently against that interface. Do not start both paths at once.
- **Next:** To use the VPCD bridge, configure/restart the host PC/SC stack so
  it keeps Virtual PCD available but releases/excludes the Pico CCID interface;
  then run the bridge and target `Virtual PCD 00 00`. This host configuration
  change requires operator action and has not been made.

## PC/SC Virtual PCD was listed before its endpoint was ready — 2026-10-22

- **Symptom:** `pcsc_scan -r` listed `Virtual PCD 00 00`, but starting the PC
  emulator first failed to connect to `127.0.0.1:35963`. Starting the emulator
  after a presence query could also race the reader's card-present event.
- **Cause:** Reader enumeration alone does not activate the VPCD listener.
  Java SmartcardIO's bounded `waitForCardPresent()` query activates the driver;
  after the VM connects, a second query confirms the card is ready for client
  selection.
- **Viable fix:** Run `PICO2_HOST_USB_ACCESS=1
  ./tools/test-javacardxsec-vpcd` through the elevated host-access path. The
  wrapper builds PC, queries Virtual PCD 00 00 before VM startup, installs the
  JavacardxSec CAP in an isolated emulator image, waits for card presence, and
  requires all 84 official deploy calls to be `OK`. The 2026-10-22 run passed
  84/84. No physical reader or flash is involved.
- **Limit:** The wrapper covers the JC305 JavacardxSec API area only. Keep its
  readiness sequence for future PC/SC VPCD deploy tests; it does not prove
  physical hardware behavior.

## JCSystem shareable lookup requires a context-switch bridge — 2026-10-18

- **Cause:** `JCSystem.getAppletShareableInterfaceObject()` previously threw
  `NotYetImplemented`. The current runtime has one applet context and no
  applet registry or callback bridge for invoking a server applet's custom
  `getShareableInterfaceObject()` implementation.
- **Scoped behavior:** The native returns `null`, which is correct for absent
  server AIDs and for applets using `Applet`'s default implementation. Case 48
  verified null AID, unknown AID, and self lookup on the probe applet.
- **Limitation:** This does not implement custom shareable objects. Do not treat
  the null fallback as full Java Card applet firewall/shareable support; a
  callback requires explicit context ownership and switch/restore semantics.

Use this file as the recurring issue checklist. Record the observed symptom,
the reliable recovery, and any limits. Promote only reproducible fixes to the
viable-solution status; a single successful probe is evidence, not a universal
fix.

## JC305 SensitiveArrays invocation used the wrong embedded method token — 2026-10-18

- **Symptom:** A JC305 applet calling
  `SensitiveArrays.isIntegritySensitive(byte[])` received `6F00`; the emulator
  trace reported `exception=20 opcode=FE`. The native handler was present, but
  the call resolved to the wrong framework method.
- **Cause:** JC305 import tokens number the five `SensitiveArrays` static
  methods 0–4. The embedded framework class has a constructor at token 0 and
  numbers those methods 1–5. The framework remapper translated the class token
  but had no method-token mapping for `SensitiveArrays`.
- **Viable fix:** For the framework package and imported class token 22, map
  static method tokens `{0,1,2,3,4}` to `{1,2,3,4,5}`. Focused cases 42–47
  passed after this mapping; full VPCD JCSystem suite passed 50/50 in 138.900
  s. PC, ARM M33, and RISC-V builds passed.
- **Correction:** The earlier diagnosis that the embedded API did not export
  `SensitiveArrays` was incorrect. The class exists; the defect was its static
  method-token mapping.

## SensitiveArrays is intentionally unsupported when capability is false — 2026-10-18

- **Viable behavior:** `isIntegritySensitiveArraysSupported()` returns false;
  valid creation raises `SystemException.ILLEGAL_USE`; invalid type/memory
  raises `ILLEGAL_VALUE`; negative length raises `NegativeArraySizeException`;
  `assertIntegrity` and `clearArray` raise `ILLEGAL_VALUE` for non-sensitive
  objects. Null inputs retain `NullPointerException` behavior.
- **Validation:** Cases 42–47 cover the capability, query, creation, assertion,
  and clear behavior, including reason codes. Focused and full VPCD tests
  passed. No physical board was accessed.

## JCSystem.makeGlobalArray implementation — 2026-10-18

- **Viable fix:** `Heap` now owns a runtime-wide global array heap with tagged
  references. It creates transient `CLEAR_ON_RESET` arrays for type codes 1–5,
  resolves them from active applet heaps, and clears their contents on reset.
  Global array references are rejected from applet instance/static fields and
  array components. Invalid types raise `SystemException.ILLEGAL_VALUE` and
  negative lengths raise `NegativeArraySizeException`.
- **Validation:** Probe cases 36–41 cover byte-array access and transient
  classification, invalid type/length, reference-storage rejection, Object[]
  null entry, and applet-object store rejection. PC, ARM M33, and RISC-V builds
  passed. Focused cases passed; full VPCD suite passed 44/44 in 144.901 s.
  No physical device was accessed.
- **Limits:** Only one applet context is modeled, so cross-applet calls and
  reset clearing through a reference retained across contexts are not directly
  exercised. The VM has no shareable-object ownership model, so it rejects
  non-null applet-owned objects in global Object[]. Global handles reserve one
  bit in the 15-bit array offset, limiting a heap to 16,383 array handles.

### Initial audit (superseded by the implementation above)

- **Finding:** The JC305 API specifies that global arrays are accessible from
  any applet context and that references to them cannot be stored in applet
  static fields, instance fields, or array components. The current runtime has
  one `Heap` per `Context`; handles are heap-local and do not tag global arrays.
- **Decision:** Do not implement `makeGlobalArray()` by merely creating an
  ordinary `CLEAR_ON_RESET` transient array. That would appear to work only
  inside one context while violating cross-applet visibility, lifetime, and
  storage restrictions.
- **Required design:** Add a JCRE-owned global array store/handle class that
  remains resolvable from each active context, clear global arrays on reset,
  and reject storing global references in applet fields or reference arrays.
  Then map JCSystem's boolean, byte, short, int, and object type codes and
  validate invalid types with `SystemException.ILLEGAL_VALUE`.
- **Evidence/validation:** Inspected the API contract in
  `javacard/framework/JCSystem.java` and heap ownership/array-handle code in
  `Context`, `Heap`, and `JC_Array`. This was a read-only design audit; no code,
  build, or runtime test was performed.

## JCSystem.isAppletActive always threw NotYetImplemented — 2026-10-18

- **Cause:** The native method had no behavior despite this runtime having a
  registered/restored current applet and its AID in `Context`.
- **Viable fix:** For the current single applet context, read the supplied AID
  instance's encapsulated byte array and compare it with the current applet
  AID. Return false for null, malformed, unregistered, or nonmatching AIDs;
  return true only for the registered/restored applet that matches the active
  context.
- **Evidence:** Case 35 checks the current AID and a well-formed unknown AID.
  Focused VPCD passed; full suite passed 38/38 in 221.009 s. PC, ARM, and
  RISC-V builds passed. No physical device was used.
- **Scope:** This does not report applets active on other logical channels or
  model multiple applet contexts. The runtime currently exposes only the
  basic channel and selected context.

## JCSystem object-deletion request silently succeeded while unsupported — 2026-10-18

- **Cause:** `JCSystem.isObjectDeletionSupported()` correctly returned false,
  but `requestObjectDeletion()` was a no-op. The supplied Java Card API
  specifies `SystemException.ILLEGAL_USE` when deletion is unsupported.
- **Viable fix:** Carry the pending VM-raised `SystemException` reason in the
  active `Context`; consume and reset it during Java exception dispatch. The
  deletion request sets reason 6 (`ILLEGAL_USE`) and throws the mapped
  `SystemException`.
- **Evidence:** Case 34 caught the exception and checked reason 6. Focused
  VPCD passed; the full JCSystem VPCD suite passed 37/37 in 173.290 s. PC, ARM,
  and RISC-V builds passed. No physical device was used.
- **Test-harness note:** When a new APDU case is added, rebuild the generated
  probe CAP with `./tools/build-jcsystem-transaction-probe` before running the
  harness. Otherwise a stale CAP can execute unrelated bytecode and fail at
  `6F00`, making it appear the native path is broken.

## Restricted-context USB access — complete fail-fast coverage — 2026-10-17

- The first guard covered the usual `flash`, `monitor`, and physical JCAlgTest
  commands, but missed direct-libUSB `bridge-pico-ccid-vpcd` and
  `repair-jcalgtest-raw-ccid` entry points. `tools/cycle` also started both
  target builds before its nested `flash` wrapper rejected the restricted
  context, wasting build time before failing.
- Added one Python `NoNewPrivs` guard shared by both direct-libUSB tools and
  moved the existing shell guard to the start of `tools/cycle`, before builds.
  All known USB/PCSC entry points now have a fail-fast check: flash, monitor,
  cycle, physical APDU and memory probes, physical named-reader JCAlgTest,
  raw CCID bridge, and raw CCID repair.
- Validation in `NoNewPrivs: 1` context: all eight entry points exited with
  the explicit host-access diagnostic in 0.75–1.52 seconds; none reached
  libusb/PCSC, and `cycle` did not build. Shell syntax and Python compilation
  checks passed. Use host-access execution for USB work from the outset; these
  guards are a fail-fast backstop, not a retry mechanism.

### Recurrence prevention update

- The wrapper guards originally failed open if `/proc/self/status` could not
  be read or did not contain `NoNewPrivs`. Both shell and Python guards now
  fail closed unless they can verify `NoNewPrivs: 0` before USB/PCSC access.
- These checks cannot intercept direct commands that bypass repository
  wrappers. The operating rule is therefore to select the host-access execution
  path on the first USB/PCSC command; never issue a restricted-context probe
  first. In the current restricted context, all eight wrapper entry paths
  reject in under one second, and a simulated unreadable proc status also
  rejects before any USB call.
- **Viable prevention:** Use host access from the outset, with fail-closed
  wrapper guards as a second layer. If host access is unavailable, leave
  hardware untouched and continue offline work.

## USB passthrough and Pico visibility

- **Symptom:** VirtualBox/host UI says the Pico is attached, but guest `lsusb`
  reports `unable to initialize libusb: -99`, or the Pico does not appear.
- **Viable recovery:** In the VM UI, detach and reattach the Pico USB device.
  This resolved the earlier mismatch where the host showed it connected but
  the guest could not see it. Then verify the guest enumeration once with
  `lsusb -d 2e8a:0009` (running CHOUPI) or `lsusb -d 2e8a:000f` (BOOTSEL).
- **Limit:** USB bus/device numbers change on re-enumeration. Re-run the
  authorized PC/SC ACL helper after each new running-mode USB node.

## PC/SC permission after USB re-enumeration

- **Symptom:** `lsusb` sees `2e8a:0009`, while PC/SC lists only virtual readers
  or does not list the Pico.
- **Viable recovery:** The user authorized this exact no-prompt helper in
  `AGENTS.md`: `sudo -n /usr/local/sbin/pico2-pcsc-reader`. It applies access
  to the current USB device node and restarts/reloads the PC/SC setup. A prior
  invocation printed `PCSC:ACCESS_GRANTED node=/dev/bus/usb/001/023` and
  `pcsc_scan -r` then listed the physical CHOUPI reader. Invoke only through
  the permitted host-access path; do not use any other sudo command.
- **Limit:** This VM session sometimes has `no_new_privileges`, causing the
  helper to fail from the restricted shell. A host-authorized elevated
  execution path worked previously. If the helper reports success but no
  physical reader appears, diagnose the service/driver without an unbounded
  scanner.

## `pcsc_scan` runaway CPU / VM saturation

- **Symptom:** PCSC reader checks hang, saturate CPUs, or leave `pcsc_scan`
  running after a timed client command.
- **Viable recovery:** Avoid `pcsc_scan` in automation. A stale process
  (`pcsc_scan -r`, PID 33195) was observed at 99.2% CPU and was terminated
  explicitly; this removed a major VM load. Use the repository's bounded Java
  probe/client or direct bounded CCID helper instead. If an old scanner is
  suspected, inspect `ps` first and stop only the identified stale scanner.
- **Limit:** `timeout pcsc_scan` did not reliably terminate the process in
  prior runs, so adding `timeout` is not sufficient protection.

## Physical JCAlgTest static SHA-256 repeat failure

- **Symptom:** JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` passes applet select,
  GETVERSION, JCSystem info, SHA-256 support, and the first measured
  `MessageDigest.doFinal()`, then the PC/SC reader becomes unavailable during
  subsequent repeated SHA calls. Clean attempts measured roughly 0.72 s for
  the first APDU; the run did not complete the operation.
- **Status:** Resolved for the current firmware/source. The caller-scoped JC305
  interface-token fix below was built and flashed to Pico
  `D92E743D5F640C0F`. The official static client completed the SHA-256
  250-invocation `doFinal` and `update` measurements without reader loss or
  runtime error. Keep the historical watchdog marker inconclusive: it does not
  establish a hardware watchdog reset. The complete client catalog remains
  bounded by a 900-second timeout; unsupported algorithms are expected.
- **Evidence:** `artifacts/logs/jcalgtest-static-physical-clean-20261017.log`,
  `artifacts/logs/monitor-jcalgtest-static-reset-20261017.log`, and
  `artifacts/results/jcalgtest-static-physical-20261017/`.

## Host/sandbox PC/SC context split — 2026-10-17

- **Symptom:** Default sandbox commands report libusb `-99` and Java
  SmartcardIO sees no physical reader, while USB and the reader are visible
  through the host-access execution context.
- **Viable resolution:** Run USB/PCSC commands through the host-access path
  (`require_escalated` in this agent interface); this is an execution context
  change, not another `sudo` command. In that context,
  `./tools/probe-jc-algtest` found Virtual PCD 00/01 plus the physical Pico and
  passed SELECT, GETVERSION `1.8.2_jc305`, JCSystem info, SHA-256 prepare and
  doFinal (`AA9000`), and expected unsupported RSA-SHA1 (`1203`). After every
  device re-enumeration, first run the specifically authorized
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper to refresh the node ACL.
- **Confirmed setup:** The Pico appears as `2e8a:0009`; USB interface 2
  advertises class `Chip/SmartCard`, endpoints `03`/`83`, and the installed
  libccid table maps VID/PID `0x2E8A`/`0x0009` to `Pico 2 CHOUPI CCID`.
  pcscd is socket activated and its idle auto-exit is normal. Do not infer that
  PC/SC is broken from a sandbox-only empty reader list.
- **Limit:** A host-context `usbreset 001/057` succeeded but did not prevent
  the subsequent static test failure. `picotool reboot -F -u` reports no reset
  interface on this composite device; the bounded CDC 1200-baud fallback
  entered BOOTSEL, `picotool info -a` confirmed it, and
  `picotool reboot --application` returned to the current image without
  flashing.

## Fail fast before hardware commands in the restricted context — 2026-10-17

- **Cause:** This VM exposes two command contexts. In the default restricted
  context `/proc/self/status` reports `NoNewPrivs: 1`; USB/libusb and the
  physical PC/SC reader are inaccessible there. Repeating `picotool` or serial
  probes from that context only wastes time and returns misleading visibility
  errors. The host-access execution path reports `NoNewPrivs: 0` and reaches
  the same USB devices.
- **Viable prevention:** `tools/common.sh` now provides
  `require_host_usb_access`; the flash, serial monitor, physical APDU probe,
  and named-reader JCAlgTest wrappers call it before hardware access. A
  restricted-context invocation exits immediately with an instruction to use
  host access. The simulator mode remains allowed in the restricted context.
- **Validation:** The five restricted-context wrapper checks all exited in
  under one second with the expected message. The escalated path refreshed
  the ACL, flashed the ARM image, captured serial output, and ran the physical
  probes successfully. Always call hardware wrappers through the host-access
  execution path; do not first try the default context.

## JCSystem available-memory reporting — 2026-10-17

- **Implementation:** The Pico transient-memory provider reports Newlib
  `mallinfo().fordblks` plus the unused `_sbrk` range up to the linker heap
  limit. The RP2350 map places the main stack in Scratch Y, so this heap range
  does not overlap the main RAM heap. The persistent provider reports unused
  bytes in the 64 KiB writable overlay, returning zero if its layout or file
  index cannot accept more records. Scalar `getAvailableMemory` saturates at
  `0x7fff`; the jc304 two-short form returns the 32-bit byte count, high word
  first. The PC target keeps returning zero because it does not own the Pico
  persistent overlay or allocator.
- **Physical validation:** JCAlgTest 1.8.2 reports `getAvailableMemory` values
  through its JCSystem info APDU. The live JC305/JC304 probe passed on the Pico:
  persistent available `47,777` bytes and both transient types `325,352`
  bytes at the time of the query. The short persistent and deselect reports
  matched the saturated extended values. The applet's reset-RAM field adds
  benchmark-array sizes and is therefore not a raw scalar-memory result.
  `getMaxCommitCapacity()` and `getUnusedCommitCapacity()` remain zero while
  commit transactions are unsupported.
- **Build/runtime:** ARM and RISC-V wrappers passed. The ARM firmware flashed;
  an eight-second serial capture reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. The physical probe also passed SELECT,
  GETVERSION `1.8.2_jc305`, JCSystem info, repeated SHA-256, and expected
  unsupported RSA-SHA1 (`1203`). Artifacts are under `artifacts/logs/` with the
  `jcsystem-memory-accounting-continue-20261017` and
  `jcsystem-live-memory-continue-20261017` stems.

## Physical SHA-256 static repeat failure — 2026-10-17 update

- The official JCAlgTest 1.8.2 static client ran in the host-access context
  against exact reader `Pico 2 CHOUPI CCID [JCAlgTest CCID]
  (D92E743D5F640C0F) 00 00` (reader index 3). It passed applet SELECT,
  GETVERSION, and JCSystem info. Five zero-repeat baseline calls averaged
  `529.0`; the first measured 256-byte / 50-call SHA-256 doFinal APDU returned
  `AA9000` in `761 ms` (reported sample `260.00`). The next identical APDU
  failed with `SCARD_E_READER_UNAVAILABLE` and the official client exited 1.
- After refreshing the reader ACL, the isolated SHA probe returned
  `F105` (`CryptoException.ILLEGAL_USE`) following that static run. A full
  MCU reset through CDC-to-BOOTSEL and `picotool reboot --application`, with
  no flash write, restored the isolated probe (`AA9000`). This shows the
  failure depends on accumulated/runtime SHA state, but its precise cause is
  not yet known. Treat the reader error and subsequent `F105` as failures;
  `NO_SUCH_ALGORITHM` for other algorithms remains expected.
- A focused probe after a clean MCU reboot passed ten separate one-call SHA
  APDUs, one exact 256-byte/50-call static SHA APDU, and one more one-call SHA
  APDU (`AA9000` for all). This narrows the problem: one 50-call APDU followed
  by one call is healthy, while the official client fails during a further
  repeated measured call after its baseline sequence. The runtime/CCID path
  needs a closer comparison with the client's exact APDU order and repeat
  count. Probe evidence:
  `artifacts/logs/jcalgtest-sha-50call-followup-physical-20261017.log`
  (SHA-256 `afd4b7dab4483670cc5e44d39ac431047b6f15487e733c835f5296b71a2b183d`).
- Eight-second serial capture after the failed run showed seven
  `TEST:PASS choupi_runtime` samples and no `TEST:FAIL`/`FAULT`; CCID diagnostics
  showed repeated `GetSlotStatus` (`msg=65`) and no XfrBlock count. The capture
  contained one `WATCHDOG_RECOVERY cause=other` marker; it does not establish a
  watchdog reset. Evidence: `artifacts/logs/jcalgtest-static-physical-hostctx-20261017.log`
  (SHA-256 `f423bcbca20ebaf211c02fc393171938afaef65b632f00ccff44d9e1cf738b90`)
  and `artifacts/logs/monitor-jcalgtest-static-hostctx-20261017.log` (SHA-256
  `7fd345669b97c9ac62745f8d60e4ed5a8d4d8637eade79431cc2cd5bd2f69357`).
- No unbounded `pcsc_scan` process or active JCAlgTest process was found during
  the host-context checks. Continue to avoid `pcsc_scan` because of the
  previously observed runaway case.

## Intermittent reader recovery after MCU reboot — 2026-10-17

- After a second clean CDC/BOOTSEL/application reboot, the Pico enumerated as
  `2e8a:0009` at node `001/062` and the authorized ACL helper succeeded, but a
  host-context Java probe listed only the two virtual readers. The first
  helper call after this reboot was not sufficient; repeating it did not
  restore physical reader discovery.
- An attempted raw CCID endpoint transfer timed out (`libusb -7`) before an
  APDU was sent. The bounded serial monitor remained healthy
  (`TEST:PASS choupi_runtime`, no `FAULT`) and showed only CCID
  `GetSlotStatus` processing (`msg=65`, `xfr=0`). `pcscd --hotplug` could not
  signal the root-owned daemon (`Operation not permitted`).
- **Current status:** intermittent PC/SC/CCID availability after reset remains
  an open host/device interaction. A prior host-context probe did enumerate
  the physical reader and complete APDUs, so preserve the host-access context
  and helper workflow; use a VM USB detach/reattach if the physical reader
  remains absent. Do not run `pcsc_scan`.
- Evidence: `artifacts/logs/jcalgtest-sha-repeated-50-apdus-physical-20261017.log`,
  `artifacts/logs/jcalgtest-sha-rawccid-repeat-physical-20261017.log`, and
  `artifacts/logs/monitor-jcalgtest-ccid-timeout-20261017.log`.

## Static SHA repeat failure mechanism — 2026-10-17

- **Superseded by the resolution below:** At first, `F105` was attributed to a
  retained SHA input length. The native trace later showed `current=0` on all
  76 observed 256-byte appends, so this was not the root cause of the official
  reader loss. Earlier `F105` runs occurred after heap exhaustion/panic and
  are not evidence that normal SHA state cleanup overflowed.
- **Confirmed applet failure mechanism:** With the official v1.8.2 settings
  (256-byte input, 50 `doFinal()` calls), a repeated run can return `F105`
  while the MCU and CCID endpoint remain alive. The trace shows
  `CryptoException` reason `0005`; in this API that is
  `CryptoException.ILLEGAL_USE`. `Sha256Digest.doFinal()` throws that reason
  when `Sha256NativeBridge.update()` rejects appending input. The native
  bridge rejects it when the retained input length plus the new input exceeds
  its 2,046-byte limit; for a 256-byte input, the retained length was therefore
  above 1,790 bytes. This is an applet digest-state/overflow path, not a
  missing-algorithm result or a confirmed MCU reset.
- A focused physical probe now replays the official sequence: prepare the
  50-call settings once, send five zero-repeat baseline APDUs, then measure.
  In one run the first 50-call APDU returned `AA9000` and the next transmit
  failed with `SCARD_E_READER_UNAVAILABLE`. In a later run, all five baseline
  APDUs returned `AA9000`, then the first measured APDU returned `F105`. The
  concurrent serial capture records the exception and `xfr=20`, followed by
  successful `GetSlotStatus` handling and `TEST:PASS choupi_runtime`; thus the
  latest `F105` was delivered by the applet while the device remained alive.
- **Still open:** The evidence identifies stale/over-limit digest state as the
  direct cause of `F105`, but does not yet identify which reset/finalization
  path leaves that state uncleared. The earlier `SCARD_E_READER_UNAVAILABLE`
  may be a transport/client symptom of the same path, but that relationship is
  not proven. Keep the issue open until state cleanup is traced and both
  repeated APDUs complete successfully.
- **General lesson for future algorithms:** Test reusable crypto objects over
  multiple APDUs, after zero-repeat baseline calls, and after both successful
  and failed operations. Verify that native and Java state is reset/cleared on
  every completion and exception path. Distinguish applet status words from
  PC/SC reader errors; do not attribute either automatically to USB or the
  algorithm backend.
- Evidence: `artifacts/logs/jcalgtest-static-state-repro-20261017.log`
  (SHA-256 `dd713bfab8772a15d04661b9ebd244c8972cd3a2085d0b4f7d11ff08a0043ede`)
  and `artifacts/logs/monitor-alg-static-state-repro-20261017.log` (SHA-256
  `0c11622d7844e92db567de647d8b686321c2af87af92baa83e2041d8dedc6fa1`).
- A follow-up diagnostic sent `B034` again after `F105`, which allocated a
  new 2,048-byte transient array and stored the new digest reference, but the
  next measured APDU still returned `F105`. Reprepare alone does not recover
  the current failure. This shifts the next investigation toward how the VM
  reads/writes the digest object's array field and transient-array contents;
  it is not evidence that `reset()` alone is the only faulty path. Trace:
  `artifacts/logs/jcalgtest-static-reset-diagnostic-20261017.log` (SHA-256
  `462ac435d379a4615aa7b9ab719c9f9cdddc9aa78484977b750db0a205bd54dd`) and
  `artifacts/logs/monitor-alg-static-reset-diagnostic-20261017.log` (SHA-256
  `418169398bd3ccff8a300edb3d92021cb1614992d864142e2c33def19208d72d`).

## Resolution: static SHA repeat heap exhaustion — 2026-10-17

- **Root cause:** `JC_Instance::getField_Reference()` bypassed the persistent
  instance's already available `getFields()` cache and deserialized a new
  reference from flash on every `GETFIELD_A`. For the digest object's
  `input` field, each read rebuilt a 2,048-byte transient `JC_Array` wrapper;
  `Heap::addArray()` then copied and retained another wrapper in the applet
  heap. Repeated `doFinal()` calls therefore consumed heap in roughly 2,060-
  byte allocations until `malloc` failed. The second static APDU panicked
  partway through its 50 calls, and PC/SC reported the reader unavailable.
- **Diagnostic evidence:** Temporary native tracing showed `current=0` on all
  76 observed 256-byte appends. Measurement one completed; measurement two
  stopped after 26 further appends at `malloc 2060 failed to allocate memory`
  and `*** PANIC ***`. This disproves the earlier stale-length hypothesis.
  The instrumentation was removed before the final build/flash.
- **Fix:** Persistent reference reads now use `JC_Instance::getFields()`, which
  restores references once and returns the cached heap handle thereafter.
  This preserves reference identity and avoids per-call deserialization and
  heap growth.
- **Hardware validation:** ARM and RISC-V wrappers both built successfully;
  the non-instrumented ARM image was flashed. The physical focused probe
  passed five baseline calls, two consecutive official 256-byte/50-call SHA
  measurements, ten single-call repeats, another 50-call measurement, and a
  following single call (`AA9000` throughout). The official JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` client also completed with exit 0. SHA-256
  `doFinal()` measured 250 invocations at `2.01 ms/op` average; SHA-256 update
  measured `2.50 ms/op`, and reset measured `3.13 ms/op`. No
  `SCARD_E_READER_UNAVAILABLE`, `F105`, or panic occurred during these SHA
  checks.
- **General lesson for future algorithms:** A persistent object may hold a
  transient array reference. `GETFIELD_A` must return the same live heap
  object throughout the applet context; deserializing such a field on every
  read duplicates transient arrays and leaks heap. Reuse the field cache for
  reference identity and test repeated stateful operations, not just one call.
- **Separate remaining result:** The full official client recorded 34
  `NO_SUCH_ALGORITHM` results (expected) and 1,866 `CANT_BE_MEASURED` rows,
  many with `6F00`, across non-SHA operation combinations. They were not
  addressed by this focused heap/reference fix.
- Evidence: native trace `artifacts/logs/monitor-alg-static-state-length-20261017.log`
  (76 append records; includes allocation failure/panic), focused physical
  pass `artifacts/logs/jcalgtest-static-cache-fix-physical-20261017.log`,
  official client `artifacts/logs/jcalgtest-static-cache-fix-official-20261017.log`,
  and post-fix serial `artifacts/logs/monitor-arm-persistent-reference-cache-20261017.log`.

## Interpret JCAlgTest follow-on `6F00` after unsupported prepare — 2026-10-17

- In the physical JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` output, the first
  non-SHA `6F00` appears for `RandomData.ALG_TRNG`. The immediately preceding
  prepare APDU (`B033`, specification `3`) returned data byte `03` with
  `9000`, meaning `CryptoException.NO_SUCH_ALGORITHM`. The client nevertheless
  sent a measurement APDU (`B042`, 50 repeats), which returned `6F00` and was
  labeled `CANT_BE_MEASURED`.
- `RandomData.ALG_SECURE_RANDOM` (specification `2`) immediately before it
  prepared with `AA9000` and its 50-call measurement completed `AA9000`.
  This supports the implementation path for the supported algorithm. The
  `ALG_TRNG` follow-on `6F00` is not evidence of a supported-operation bug;
  the explicit prepare status already identifies the requested algorithm as
  unsupported. Do not implement TRNG merely to eliminate this follow-on row.
- General rule for future JCAlgTest triage: inspect the prepare status before
  treating a later measurement `6F00` as a VM failure. If prepare reports
  `NO_SUCH_ALGORITHM`, classify the measurement as a consequence of unsupported
  setup. Investigate only operations whose prepare returned success, while
  continuing to treat unsupported algorithms as expected.
- Evidence: `artifacts/results/jcalgtest-static-cache-fix-physical-20261017/ALGTEST_log_1792223036058.log`, lines around 900–1130; run transcript
  `artifacts/logs/jcalgtest-static-cache-fix-official-20261017.log`.


## Correction: first residual static `6F00` is supported `setSeed` — 2026-10-17

- A second pass through the raw result log corrected the previous
  `ALG_TRNG`-first interpretation. The **first** failed measurement is
  `RandomData.ALG_SECURE_RANDOM.setSeed()` (specification 2, method 2): prepare
  returned `AA9000`; five zero-repeat `B042` baselines returned `AA9000`; the
  first 50-repeat `B042` measurement returned `6F00`. The next `ALG_TRNG`
  prepare (`B033`) also returned `6F00`, after the supported-path failure.
  Therefore the following TRNG rows in this run are not valid evidence that
  TRNG alone returns `NO_SUCH_ALGORITHM`.
- The SDK implementation of `RandomData.setSeed()` validates the buffer range
  then performs no state change for `ALG_SECURE_RANDOM`. The cause of its
  failure under 50 repeats is not established. The earlier section
  “Interpret JCAlgTest follow-on `6F00` after unsupported prepare” is
  superseded for this run.
- No new hardware test was possible in the current execution context: `lsusb`
  returned `unable to initialize libusb: -99`; `picotool` failed libusb
  initialization; and the permitted helper exited because the container has
  `no new privileges`. The captured client result is still available for
  diagnosis.
- General lesson: determine the first failing operation in chronological
  order. Later prepare or measurement statuses may be consequences of a prior
  supported-path failure and must not be classified independently.
- Evidence: `artifacts/results/jcalgtest-static-cache-fix-physical-20261017/ALGTEST_log_1792223036058.log`, first failing sequence around lines 1030–1130; SDK implementation `choupi/javacard-sdk/javacard-api/src/javacard/security/RandomData.java`.

## Fix supported RandomData.setSeed static failure — 2026-10-17

- **Root cause:** The uploaded JCAlgTest CAP uses JC305 `RandomData` virtual
  method token `2` for `setSeed([BSS)V`; the embedded API's public method table
  puts `nextBytes([BSS)S` at token `2` and `setSeed([BSS)V` at token `3`. There
  was no RandomData token translation. The void setSeed call therefore resolved
  to short-returning nextBytes. Its `sreturn` left one word on the caller's
  operand stack; the next loop iteration eventually overflowed at
  `GETFIELD_A_THIS` (`6F00`, exception id 2). The same order difference exists
  for `RandomData.OneShot`.
- **Fix:** For the JC305 security package, remap virtual method tokens for
  RandomData class tokens 14 and 32 using `{0, 1, 3, 2, 4, 5}`. This maps
  JC305 `setSeed` to the embedded void method and JC305 `nextBytes` to the
  embedded short-returning method. The mapping is in
  `choupi/src/jc_handlers/jc_cp.cpp`.
- **Hardware validation:** Both `./tools/build arm` and `./tools/build riscv`
  passed. The clean ARM firmware was flashed to the physical Pico 2. The
  focused secure-random probe passed the exact 50-repeat workload (`AA9000`),
  and official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` exited 0. Its
  setSeed measurement passed all 250 invocations at `0.36 ms/op` average.
  Official output recorded 40 `NO_SUCH_ALGORITHM` results, which are expected.
- **Remaining static-run triage:** The completed run recorded 1,855
  `CANT_BE_MEASURED` rows, including 95 measurement/preparation APDUs with
  `6F00`. The first `6F00` after the setSeed fix occurs at DES
  `Cipher_doFinal()` prepare (`B031`, `ALG_DES_CBC_NOPAD`, key length 64), not
  in RandomData. This is a separate unresolved failure; inspect its prepare
  and method-dispatch path before changing any cipher support. Do not infer
  that every `CANT_BE_MEASURED` row is a VM defect.
- **General lesson:** Cross-version CAP compatibility includes virtual method
  token order as well as class-token order. A return-type mismatch can leave a
  seemingly unrelated stack word and only fail on a later iteration. Verify
  imported and embedded method descriptors/order for both void and value
  returns when porting SDK APIs.
- Evidence: `artifacts/logs/jcalgtest-secure-random-setseed-clean-50-20261017.log`
  (SHA-256 `2d73aab519f2050881bcb7d80ce82fbea3cf0937cdccf121654d27c17a090b69`),
  `artifacts/logs/jcalgtest-static-randomdata-token-map-20261017.log` (SHA-256
  `266df057ddd8df658cd339ea1b61ae5916a6b2396ad9ba8cb592a71b5d9fda20`), and
  `artifacts/results/jcalgtest-static-randomdata-token-map-20261017/ALGTEST_log_1792234644321.log`.

## Isolate first DES static prepare failure — 2026-10-17

- The first DES `ALG_PERFORMANCE_STATIC` failure is specifically the prepare
  APDU `B0 31` for `ALG_DES_CBC_NOPAD`, 64-bit DES key, `Cipher_doFinal`; its
  payload is `00 11 00 01 FF FF 00 03 00 40 00 07 01 00 FF FF 00 02 00 32 00 01`.
  It returns `6F00` after 550 ms. The immediately preceding secure-random
  `setSeed` measurement completed successfully. Later DES prepare variants
  also return `6F00`.
- JCAlgTest's `prepare_class_Cipher()` sequence is `prepare_Key()` (build two
  DES keys and set their values), `Cipher.getInstance(ALG_DES_CBC_NOPAD)`,
  `Cipher.init()`, then secure-random input fill. The applet's `m_trng` field
  is initialized once as `RandomData.ALG_SECURE_RANDOM`; the prior TRNG
  unsupported result concerns a separate RandomData test and does not itself
  explain this prepare failure. The archived serial capture ends before the
  DES APDU, so it contains no exception frame for this failure.
- Added `--des-cipher-prepare` to `tools/JCAlgTestApduProbe.java` to replay
  only this exact prepare request. The repository wrapper compiled the probe,
  but the run could not transmit: Java SmartcardIO reported that the Pico
  reader was absent. The approved ACL helper also exited because this
  container has the no-new-privileges flag. No new physical result was
  obtained; root cause remains unclassified.
- Next: in the host-access context, run the focused probe while capturing a
  bounded serial trace. Use the exception frame to distinguish key creation,
  DES `setKey/getKey`, Cipher construction/init, and random input fill before
  changing behavior. Do not infer a DES algorithm bug solely from `6F00`.
- Evidence: official transcript
  `artifacts/logs/jcalgtest-static-randomdata-token-map-20261017.log`, raw
  result `artifacts/results/jcalgtest-static-randomdata-token-map-20261017/ALGTEST_log_1792234644321.log`,
  serial capture `artifacts/logs/monitor-jcalgtest-static-randomdata-token-map-20261017.log`.

## Fix JC305 Cipher virtual-token mismatch in static workload — 2026-10-17

- **Root cause:** The first DES CBC no-padding `Cipher_doFinal` prepare failed
  because JCAlgTest 1.8.2 was compiled against the JC305
  `javacardx.crypto.Cipher` export, whose virtual tokens differ from the
  embedded CHOUPI export. JC305 uses token 3 for `init(Key, byte)`; embedded
  CHOUPI uses token 1. The unmodified token dispatched to `getAlgorithm()`
  (token 3 in CHOUPI), returning a byte where the caller expected `void`.
  This left an operand on the caller stack and led to a later invalid field
  access and `6F00`. The mismatch was visible in serial dispatch traces and
  confirmed by comparing both `.jca` public method tables.
- **Related receiver/interface resolution:** DES key setup also exposed that
  `invokeinterface` must resolve a receiver class with its owning package's
  import table and compare resolved interface identities across packages.
  JC305 `DESKey` interface tokens 4/5 (`getKey`/`setKey`) also reverse the
  embedded export order. Both mappings are scoped to the uploaded JC305 CAP.
- **Fix:** Added the `javacardx.crypto.Cipher` class-token-1 virtual map
  `{0,6,3,1,2,7,4,5}` (token 0 preserves inherited `Object.equals`). The
  physical trace now shows `method_raw=3 method_mapped=1`; the exact failing
  prepare APDU returns `AA9000`.
- **Validation:** ARM and RISC-V wrapper builds passed. ARM firmware flashed
  to the Pico 2; bounded serial output reported `TEST:PASS choupi_runtime`
  without `TEST:FAIL` or `FAULT`. The focused `--des-cipher-prepare` probe
  passed. Official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` exited 0 and
  completed. DES CBC no-padding `Cipher_doFinal` and `Cipher_init` measurements
  returned `AA9000`. Totals were 68 `NO_SUCH_ALGORITHM` and 1,824
  `CANT_BE_MEASURED`; these totals include unsupported or otherwise
  unmeasurable rows and do not imply 1,824 VM defects. The first later `6F00`
  is a distinct 3-key DES CBC ISO9797 M1 prepare; it remains unclassified and
  must be triaged by prepare status before changing support.
- **Reusable lesson:** Cross-version compatibility must compare each imported
  class's full virtual method table, including inherited tokens and return
  descriptors, against the embedded export. A `void`/value-return mismatch
  can corrupt caller stack state and surface as a later unrelated field error.
  For `invokeinterface`, resolve the receiver using its package context and
  compare canonical interface identity, not raw class-reference numbers.
- Evidence: exact probe `artifacts/logs/jcalgtest-des-cipher-token-map-fix-20261017.log`;
  official transcript `artifacts/logs/jcalgtest-static-des-cipher-token-map-fix-20261017.log`;
  raw log and CSV under `artifacts/results/jcalgtest-static-des-cipher-token-map-fix-20261017/`;
  bounded serial trace `artifacts/logs/monitor-arm-jcalgtest-static-des-cipher-token-map-fix-20261017.log`.

## JSR saved-PC storage and remaining static-suite OOM — 2026-10-17

- **Confirmed JSR correctness defect:** `Frame::old_pc_t` stored `pc_t&`, so
  saved return addresses aliased the live program counter. `Frame::savePC()`
  also appended a new list node on every `jsr` in a frame and never reused a
  slot after `ret`; its return index is only one byte. The fix stores `pc_t` by
  value, records the JSR fall-through address (`instruction_pc + 3`), reuses a
  slot after `ret`, and throws `StackOverflowException` if more than 256 slots
  are simultaneously required. `Frame::savePC()` and `Stack::savePC()` are no
  longer `noexcept` so this overflow is reported through VM exception handling.
- **Do not treat this as the complete ALG_PERFORMANCE_STATIC OOM fix:** after
  the JSR correction, the final physical run still panicked during the first
  50-call AES-128 CBC no-padding `Cipher_doFinal` measurement. Five baseline
  APDUs returned `AA9000` (average `654.6` ms); the measurement APDU then lost
  the reader. Serial ended with `malloc 44 failed to allocate memory` and
  `*** PANIC *** Out of memory`. Thus JSR saved-PC storage was a real VM defect,
  but it is not sufficient to explain the remaining memory exhaustion. The
  exact owner of the final 44-byte allocation has not been identified.
- **Temporary cipher-buffer experiment:** replacing the CBC vectors with
  fixed-size stack blocks removed the 256-byte allocation records, but an
  earlier image still exhausted memory at a 12-byte allocation. That
  experiment was reverted; do not infer that the vectors were the sole leak.
- **Validation:** ARM and RISC-V wrapper builds passed. The JSR-corrected ARM
  image flashed, bounded serial boot reported `TEST:PASS choupi_runtime`, and
  the focused physical DES prepare returned `AA9000`. The full official
  JCAlgTest 1.8.2 static suite did not complete; it reached the AES static
  measurement and panicked as described. `NO_SUCH_ALGORITHM` rows remain
  expected and were not treated as defects.
- **Next diagnosis:** instrument `Stack::push_Frame/pop_Frame` depth and
  `Heap::addInstance/addArray` counts around the repeated AES APDU to identify
  whether the 44-byte growth is a retained call frame, a Java object, or another
  allocation. Keep captures bounded; do not retry a run that panics without a
  newly flashed or reset image.
- Evidence: `artifacts/logs/build-arm-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/build-riscv-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/flash-arm-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/monitor-arm-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/jcalgtest-jsr-final-reader-probe-20261017.log`,
  `artifacts/logs/jcalgtest-static-jsr-pc-slot-reuse-final-20261017.log`, and
  `artifacts/logs/monitor-arm-jsr-pc-slot-reuse-static-final-20261017.log`.

## Physical ALG_PERFORMANCE_STATIC heap retention — 2026-10-17

- **Confirmed remaining OOM cause:** The RP2350 VM heap has no object
  reclamation. `Heap::addArray()` and `Heap::addInstance()` append owning
  `shared_ptr`s to index-addressed lists, and there is no removal/GC path.
  `Heap::clearTransientArrays()` clears transient array contents but does not
  remove array objects. The JNI implementation of
  `JCSystem.requestObjectDeletion()` is currently a no-op. This causes
  unreachable Java objects created by repeated JCAlgTest cipher calls to
  accumulate until the global allocator fails. The JSR saved-PC defect was
  independent and already fixed; this heap retention is the remaining static
  suite failure.
- **Physical evidence:** With temporary per-APDU counters on the named reader,
  the official 1.8.2 `ALG_PERFORMANCE_STATIC` run passed SELECT, GETVERSION,
  JCSystem info, SHA digest operations, and the five AES-128 CBC no-padding
  `Cipher_doFinal` baselines (average `566.4 ms`). The first 50-call measurement
  then returned `SCARD_E_READER_UNAVAILABLE`; the board recovered through its
  watchdog path. During cipher measurements, each 50-call operation added
  about 100 tracked arrays (roughly two per operation) and added instances as
  cipher/key objects were created. Before the failing AES measurement, the
  counters showed `arrays=594 instances=192 frames=0 frame_high=6`; the final
  APDU could not print its completion counters because allocation failed.
  SHA-256 `B041` repeat calls kept the tracked array count flat at 13, so the
  observed growth is in the cipher measurement path, not the SHA accelerator.
- **Stack ruled out:** After each completed APDU the interpreter frame count
  was zero, with maximum observed frame high-water `11`. The failure is not a
  retained interpreter call stack.
- **Safe-fix constraint:** Heap offsets are encoded directly in `jref_t` and
  instances/arrays can be retained by statics, object fields, persistent
  fields, APDU state, and active frames. Deleting list entries or resetting
  per APDU would invalidate live references. A correct fix needs a tracing
  collector or a carefully specified allocation lifetime/root scheme that
  scans every reference-bearing root and preserves/remaps indexed handles.
  Do not paper over this by clearing the heap or pooling arrays by size.
- **Validation and cleanup:** ARM and RISC-V wrapper builds passed with the
  temporary counters. The instrumented image booted and its reader probe
  passed. After evidence capture, the temporary counters were removed, both
  architectures rebuilt successfully, and the clean ARM image was flashed;
  bounded serial output reported `TEST:PASS choupi_runtime` with no
  `TEST:FAIL`/`FAULT`. The official static suite remains failed and must not be
  reported as passing. `NO_SUCH_ALGORITHM` remains expected.
- **Evidence:**
  `artifacts/logs/jcalgtest-static-memory-counters-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-memory-counters-static-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-memory-counters-post-failure-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-memory-counters-cleanup-20261017.log`;
  and build/flash records with the `jsr-memory-counters` stem. Key SHA-256
  digests: official transcript `53d3175ab15d4ee0ea141486714863653d48b312f95f93384b65c3b47e39ab83`;
  in-run serial `af644823a7965a6ed3b4eead17f56450de516a3357f5dd12892ad07fd443ab1f`;
  post-failure boot `cda099b9774a3a6858b98489aab67a3b7c6f612d2073a53dcfb289e63dbaf21a`;
  clean firmware boot `8d87aaf5bf04667a739e3acd8c307777f2b262380d8151a3a820027af0c12236`.
  Raw client output and partial CSV are also archived at
  `artifacts/results/jcalgtest-static-memory-counters-20261017/`:
  `ALGTEST_log_1792239359688.log` (`bf13f1222f87ead5262db72c58ddbd58b9dc79dd5329269af0395270499c8022`) and
  `Pico_2_CHOUPI_CCID_[JCAlgTest_CCID]_(D92E743D5F640C0F)_00_00____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED_1792239359688__3b_80_01_81.csv`
  (`9180b734a6f04cd5fa3118089d90eb08ec9717d15c1133cf9171f277f9e26cd6`).

## Supported sweep heap exhaustion — 2026-10-17

- **Observed:** The physical `ALG_SUPPORT_EXTENDED` sweep again stopped at
  `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384` after 1,636 completed CSV rows. The
  concurrent serial log contains `malloc 12 failed to allocate memory` at
  `artifacts/logs/monitor-arm-ccid-sweep-repro-20261017.log:62594`; the client
  then reported `SCARD_E_READER_UNAVAILABLE`. This corrects the earlier
  interpretation that this capture had no allocation failure. No CCID transfer
  error counter or firmware fault was logged before the allocation failure.
- **Cause:** `CryptoException.throwIt()` constructs a new Java exception for
  every unsupported/error result. `Heap::addInstance()` appends every instance
  to the heap's owning list, and the current heap lifecycle has no object
  reclamation path; `clearTransientArrays()` only clears arrays. The increasing
  Java exception allocations therefore remain retained across the sweep and
  explain the finite-heap failure. The 12-byte failed allocation and the exact
  allocation count were not independently instrumented, so do not claim each
  failed 12-byte block maps one-to-one to a `CryptoException`.
- **Rejected workaround:** A Java static cached exception does not provide a
  runtime-owned live object in this VM. Static reference reads/writes route
  through persistent flash serialization; a 50-throw VPCD probe observed a
  different object identity at throw 12. The source change and failing probe
  were reverted. Do not cache JCRE entry-point exceptions in ordinary Java
  static fields.
- **Status:** Root cause is identified; repair remains open. Implement a
  VM-managed per-context runtime exception object or a correct object
  reclamation mechanism, then run the 50-throw VPCD regression and repeat the
  physical supported sweep. Keep unsupported algorithms as expected results.
- **Evidence:** Partial official client output is in
  `artifacts/logs/jcalgtest-supported-arm-ccid-recovery-counters-20261017.log`
  and its results directory; serial allocation failure is in
  `artifacts/logs/monitor-arm-ccid-sweep-repro-20261017.log`. The rejected
  Java-static experiment's converter and VPCD logs are
  `artifacts/logs/build-pc-exception-reuse-v2-20261017.log`,
  `artifacts/logs/build-arm-exception-reuse-v2-20261017.log`, and
  `artifacts/logs/jcsystem-exception-reuse-vpcd-v2-20261017.log`.

## InitializedMessageDigest.OneShot unsupported factory — 2026-10-17

- **Symptom:** The official JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` sweep
  returned `6F00` at `javacard.security.InitializedMessageDigest.OneShot`.
  The VM diagnostic identified `NullPointerException` at `INVOKEVIRTUAL`;
  JCAlgTest calls `OneShot.open(...)` and then `close()` inside a `try` that
  catches only `CryptoException`.
- **Cause:** `InitializedMessageDigest.OneShot.open(byte)` was a TODO stub
  returning `null`. This turns an unsupported algorithm into a later NPE and
  corrupts the rest of the sweep's result stream.
- **Viable fix:** Unsupported factories must throw their documented
  `CryptoException.NO_SUCH_ALGORITHM`; never return `null` for a Java Card
  object factory. The stub now throws reason 3. A focused applet call verifies
  the returned reason, and the physical official CSV records
  `InitializedMessageDigest.OneShot / ALG_SHA_256;no` without `6F00`.
- **Remaining limit:** Two full physical supported-algorithm sweeps stopped at
  the same request, `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384`, after 1,637 CSV
  rows. The first ended with `SCARD_E_READER_UNAVAILABLE`; the second did too.
  A 240-second concurrent serial capture during the second run recorded normal
  CCID traffic through 3,556 RX events / 2,909 transfers and recurring
  `TEST:PASS choupi_runtime`, with no applet `6F00`, CCID transfer error, or
  reset. A later analysis of the serial tail found `malloc 12 failed to
  allocate memory`, so this run did contain an allocation failure. Afterward
  USB still enumerated as `2e8a:0009`, but PC/SC
  listed only VPCD readers even after the authorized ACL helper. Reflashing the
  same ARM image restored PC/SC publication; the bounded physical applet probe
  then passed. This narrows the failure to CCID/PCSC recovery after the long
  sweep. The later malloc failure and unbounded Java instance retention explain
  the applet's finite-heap failure; PC/SC reader publication still also fails
  after the APDU error. Do not claim the full sweep passed.
- **Evidence:**
  `artifacts/logs/jcalgtest-supported-arm-oneshot-fix-20261017.log`,
  `artifacts/logs/monitor-arm-oneshot-unsupported-after-official-20261017.log`,
  `artifacts/logs/jcalgtest-physical-after-supported-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-disconnect-repro-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-repro-20261017.log`,
  `artifacts/logs/flash-arm-supported-disconnect-recovery-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-recovery-20261017.log`,
  and `artifacts/logs/jcalgtest-physical-after-disconnect-reflash-20261017.log`.

## Runtime CryptoException roots and physical follow-up — 2026-10-17

- **Supported-sweep cause:** The runtime cache initially recognized only ROM
  class offset 234. `CryptoException` also resolves in the imported
  `javacard.security` package as class offset 233 under AID
  `A0000000620102`. That allocation bypassed the cache, so each throw created
  an instance retained by the append-only `Heap`. A 50-throw VPCD probe caught
  different refs (14 then 16) and failed its identity assertion even though
  the ROM view repeatedly returned ref 6. A dynamic export-token lookup was
  also unsafe because unrelated framework export tables can share AID
  `A0000000620101` and lack that token.
- **Viable fix:** Identify the pinned ROM and imported views by their
  package-AID/class-offset pairs, then maintain one VM-owned root per view in
  each applet `Heap`. Repeated throws within a view reuse the same object and
  run the constructor again to update the reason. The 50-throw VPCD test now
  passes, including reason checks and identity. All preceding transaction,
  SHA-256, JCSystem-memory, and expected-unsupported cases also pass.
- **Physical validation:** ARM and RISC-V builds passed. The ARM CHOUPI image
  flashed and its bounded cycle reported `TEST:PASS choupi_runtime`. The named
  physical JCAlgTest reader completed `ALG_SUPPORT_EXTENDED` with 4,520 result
  rows: 15 `yes`, 4,505 `no` (unsupported rows are expected). The run exited
  successfully. A concurrent four-minute serial capture had no `malloc ...
  failed`, `PANIC`, `FAULT`, or `TEST:FAIL`. This supersedes the earlier
  supported-sweep claim that the physical run remained incomplete.
- **Separate static-suite status:** A new physical `ALG_PERFORMANCE_STATIC`
  run reached a 250-call DES CBC `Cipher_doFinal` measurement (average
  `28.64 ms/op`) and then failed on the next
  `TYPE_DES LENGTH_DES3_2KEY ALG_DES_CBC_NOPAD Cipher_init()` request with
  `SCARD_E_READER_UNAVAILABLE`. Its bounded serial capture ended without
  `malloc <n> failed`, `PANIC`, `FAULT`, or `TEST:FAIL`; after the authorized
  ACL refresh, `pcsc_scan -r` again listed the Pico reader. This run does not
  establish that the prior static-suite heap issue is fixed: earlier evidence
  directly attributed its OOM to append-only cipher arrays (594 arrays and
  192 instances before failure). It adds a distinct reader-unavailable event
  without a confirmed firmware allocation failure. Static mode remains open.
- **Evidence:** `artifacts/logs/jcsystem-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/build-pc-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/build-arm-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/build-riscv-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/cycle-choupi-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/monitor-arm-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/jcalgtest-static-arm-crypto-class-view-roots-20261017.log`,
  and `artifacts/logs/monitor-arm-static-crypto-class-view-roots-20261017.log`.
- **Artifacts:** Supported CSV SHA-256
  `f90520551b76e8f90725d7df61fc46bf21a6c7e006b4a86949cc9911e5a5b951`;
  official transcript SHA-256
  `f17f7a8ce908a78a1919907859b493fdde5127d603781336d7506d5069513f59`;
  four-minute serial SHA-256
  `da8e6000bc3b27bdc341625beb673b05d4945f69e8db0e9a8ad1c62310e6123a`.

## RandomData.OneShot unsupported result and support-matrix audit — 2026-10-17

- **Cause:** `RandomData.OneShot.open()` was a TODO stub that threw
  `CryptoException.ILLEGAL_USE`. JCAlgTest's 1.8.2 support probe catches
  `CryptoException` and records its reason, so all six unsupported OneShot
  algorithm rows became `error(ILLEGAL_USE)` instead of `no`.
- **Viable fix:** An unimplemented factory must throw its documented
  `CryptoException.NO_SUCH_ALGORITHM`. The source now throws reason 3. A
  focused physical JCAlgTest VPCD APDU asserted response type `2B`, reason
  `03`, and `9000`; VPCD passed 11/11. ARM and PC builds passed. The official
  physical `ALG_SUPPORT_EXTENDED` sweep then completed all 4,525 rows with
  counts `15 yes / 4,510 no / 0 other`; all six OneShot rows are now `no`.
- **Physical health:** The named Pico reader remained available through the
  full run. Two bounded serial captures contained recurring
  `TEST:PASS choupi_runtime` and no `TEST:FAIL`, `FAULT`, `PANIC`, failed
  allocation, or `CHOUPI:CCID_ERRORS` record. ARM image flash and boot capture
  also passed. No RISC-V build or test was run.
- **AC4 audit finding:** The 15 physical `yes` rows all appear as `yes` in the
  jCardSim reference, but two are API false positives: `OwnerPINBuilder`
  returns `null` for `OWNER_PIN_X` and `OWNER_PIN_X_WITH_PREDECREMENT`, while
  JCAlgTest reports a factory as supported whenever it does not throw. These
  documented types need real implementations or a way to report absence
  without breaking the sweep; until resolved, do not mark the full
  source/results consistency check complete. Full matrix comparison and hashes
  are in `artifacts/results/jcalgtest-support-comparison-20261017.md`.
- **Separate limit:** `ALG_PERFORMANCE_STATIC` remains unresolved due the
  append-only heap and separate PC/SC loss from earlier runs. Do not infer that
  the supported-sweep fix resolves static-mode array retention.
- **Evidence:** `artifacts/logs/build-arm-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/build-pc-randomdata-oneshot-unsupported-v2-20261017.log`,
  `artifacts/logs/flash-arm-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/monitor-arm-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/pc-vpcd-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-randomdata-oneshot-fix-20261017.log`,
  and two 240-second physical serial captures.
- **Hashes:** Physical CSV
  `f9c74aaa7d5b3a3e7fc30897a4fa167bd7cb70cb8c79c5ad1e948d9e0aa401bf`;
  transcript
  `0cb527b6fbd0886febc71e008b4a0f408f9af01f1ce54e3671d00409c563aa57`;
  jCardSim CSV
  `67d9feebf9756da69db034bab05cddd9acb38d31aa980888fa7b5db22ef1757b`.

## OwnerPINx dispatch and inherited-field resolution — 2026-10-17

- **Cause 1 — internal `invokespecial`:** The rommask encoded a
  `SuperMethodRef` with the method-local ordinal (`OwnerPINxWithPredecrementImpl.update`
  appeared as token 0), while the VM resolves a class virtual/package method
  token. Token 0 selected `Object.equals` and interpreted the call with the
  wrong argument count. Internal super refs must use the declaring class's
  public or package method table token; package tokens carry the package bit.
  The VM resolves the ref through the immediate superclass, then checks the
  receiver in the caller's operand stack before entering the callee.
- **Cause 2 — inherited instance fields:** `InstanceFieldRef` tokens are local
  to their declaring class, but object storage is one flat field vector. The
  VM previously used the token directly, so subclass field token 0 aliased a
  base-class field. Add each superclass's declared instance-field count while
  resolving the declaration chain. The regression exposed this when OwnerPIN
  treated the subclass's transient predecrement marker as its PIN array.
- **Cause 3 — virtual reset after a successful PIN check:** `OwnerPIN.check()`
  set the validated flag and then called overridable `reset()`. The predecrement
  subclass override called `super.reset()`, clearing validation immediately.
  Successful checks now restore the try counter directly without virtual
  dispatch.
- **Viable validation:** The full VPCD JCSystem/crypto/OwnerPINx regression
  passed all 16 checks (`TEST:SUMMARY pass=16 fail=0 elapsed_s=59.295`). After
  removing temporary diagnostics, focused case 13 passed again with response
  `030302000100000202000201030103009000` and summary `pass=3 fail=0`.
  `./tools/build-pc` and the ARM M33 build both passed. The PC build emitted 49
  existing Rust javacard-os warnings and two packed-member alignment warnings;
  ARM emitted no warnings. No physical board was used.
- **Boundary:** This dispatch/layout fix does not resolve the separate
  `ALG_PERFORMANCE_STATIC` append-only heap retention issue documented above.
- **Evidence:** `artifacts/logs/jcsystem-ownerpinx-full-vpcd-20261017.log`,
  `artifacts/logs/jcsystem-ownerpinx-cleanup-vpcd-20261017.log`,
  `artifacts/logs/build-pc-ownerpinx-cleanup-20261017.log`, and
  `artifacts/logs/build-arm-ownerpinx-cleanup-20261017.log`.

## JCAlgTest VPCD startup delay before support queries — 2026-10-17

- **Observed:** The desktop VPCD integration can install and SELECT the
  JCAlgTest 1.8.2 applet, then fail to receive GETVERSION. Three bounded runs
  timed out on that first post-SELECT APDU at 20, 90, and 30 seconds. The 90-
  second run used the release VM; the 30-second diagnostic run used the debug
  VM. No OwnerPINX support APDU was sent, so this is not evidence against the
  OwnerPINX implementation.
- **Trace finding:** JCAlgTest defers initialization until the first process
  APDU. When its large transient arrays fit, `AlgPerformanceTest` construction
  also creates `JavaCardAES`, whose constructor allocates four 256-element
  lookup tables and generates the S-boxes in interpreted Java bytecode. The
  debug trace was still in that S-box generation loop at index 91 when stopped.
  The VM's normal release trace was not enabled for the 90-second timeout, so
  the exact no-debug instruction count remains unknown.
- **Current handling:** Keep the small OwnerPINx VPCD behavior regression as
  the reliable functional check. Do not use this desktop JCAlgTest path as an
  official support result until GETVERSION completes. The physical support
  sweep has not been rerun after OwnerPINx implementation. The build was
  returned to release mode after tracing.
- **Evidence:** `artifacts/logs/pc-vpcd-ownerpinx-support-20261017.log`,
  `artifacts/logs/pc-vpcd-ownerpinx-support-retry-20261017.log`,
  `artifacts/logs/pc-vpcd-ownerpinx-debug-20261017.log`,
  `artifacts/logs/build-pc-ownerpinx-support-debug-20261017.log`, and
  `artifacts/logs/build-pc-ownerpinx-support-release-20261017.log`.


## Host-access invocation latch — 2026-10-17

- **Recurrence:** `NoNewPrivs: 0` is not sufficient evidence that a tool was
  launched in the host-access execution context. A command can otherwise reach
  libusb from a restricted execution and spend tens of seconds failing.
- **Viable prevention:** Every repository USB/PCSC wrapper now also requires
  `PICO2_HOST_USB_ACCESS=1`, in addition to a readable `/proc/self/status`
  reporting `NoNewPrivs: 0`. Invoke that single command with
  `sandbox_permissions: "require_escalated"` and the marker. If the marker is
  forgotten, the wrapper exits before calling libusb, PC/SC, or starting a
  build. Direct commands such as `lsusb` must themselves be issued with
  `require_escalated`; never probe them in the default context.
- **Limit:** The marker is a caller-side latch, not an escalation mechanism.
  It cannot make direct commands safe or elevate a wrapper. No USB command was
  run while validating this change.

## Heap reclamation design constraints — 2026-10-17

- **Handle stability:** `jref_t` stores a 1-based 15-bit offset into either the
  `Heap::arrays` or `Heap::instances` list. Erasing or compacting list entries
  changes the meaning of every later live reference. Any collector must leave
  stable slots (for example, null tombstones plus a free list) and may reuse a
  slot only after proving that no live reference can reach it.
- **Collection boundary:** `Stack` stores untyped raw words for locals and
  operands. During interpretation, popped references can also be held only in
  native C++ locals. Collection inside bytecode/native calls cannot safely
  discover all roots. The interpreter returns with an empty frame list after a
  completed APDU, which is the earliest practical initial safepoint.
- **Roots still missing from a simple APDU-boundary sweep:** `Context` owns
  applet registration/restoration, AID, and APDU references, while `Heap` owns
  the VM CryptoException roots. Static references are now cached in the heap,
  but the cache has no enumeration API for a collector. Persistent object
  fields are cached after first access and can contain transient-array
  references; reference arrays also need element tracing.
- **Safe next design step:** first add canonical VM roots for loaded static
  references (keyed by package and static token), and explicit root enumeration
  for `Context`/`Heap`. Then add mark traversal through typed instance fields
  and reference-array elements, treating live persistent objects as roots or
  enumerating their serialized roots. Only after that should stable-slot sweep
  reuse be implemented and stress-tested across APDU boundaries. Do not clear
  the heap per APDU, compact lists, or pool arrays solely by size.
- **Validation status:** This is a source-architecture review only. No code,
  build, hardware, or performance validation was changed or run for this design
  note. Existing physical SHA measurements establish the one-time cached field
  fix, but do not validate a future garbage collector.
- **Resolved additional retention/identity path:** Before commit `425602b`,
  `bc_getstatic_a()` called `FlashMemory_Handler::getPersistentField_Reference()`
  on every access, reconstructing duplicate heap wrappers and breaking
  reference identity. It now uses a per-heap static root cache keyed by package
  and field; the cache is scoped to context lifetime. Its remaining collector
  gap is that roots cannot yet be enumerated (see the inventory below).

## Heap-root inventory after static-cache implementation — 2026-10-17

- **Direct context roots:** `Context` holds the AID object, registered and
  restored applet references, APDU buffer and APDU object references, and an
  outgoing buffer reference. These must be surfaced to a collector while the
  context is live. Some have `shared_ptr` mirrors; those C++ owners must not
  outlive reclamation of their corresponding heap slot.
- **Direct heap roots:** runtime CryptoException references and cached static
  reference entries. The static cache currently has only keyed get/set methods,
  so add read-only enumeration before using it as a collector root set.
- **Graph edges:** Instance fields store a compact `jref_t` plus a
  `FieldType`; the `FIELD_TYPE_OBJECT` and array variants identify references.
  Reference arrays store compact `jref_t` entries. Primitive fields and arrays
  must not be interpreted as references. Persistent instance fields load
  lazily, and persistent reference-array reads deserialize children on demand;
  traversal must account for heap growth while loading them and must not walk
  the containers with invalidated iterators.
- **Safepoint requirement:** `jref_t` has no generation counter. Stack locals
  and operands are untyped words, and bytecode/native routines keep references
  in C++ locals. Collect only after the APDU interpreter call and its native
  caller frames have returned, with no Java frames and no outstanding native
  references. At that point the root return-value cache must also be empty or
  explicitly included. Context-held APDU references remain roots if retained.
- **Slot requirement:** Heap references encode a 1-based offset into separate
  array and instance lists. Never erase list nodes or compact either list.
  A tombstone/free-list scheme can preserve offsets, but a slot may be reused
  only after every Java, context, cached-field, array-element, and native root
  has been accounted for. `getArray`/`getInstance` must reject tombstones before
  reuse is enabled.
- **Decision:** The next safe implementation step is an explicit root visitor
  for `Context` and `Heap`, followed by typed object/array edge enumeration.
  First run marking without reclaiming slots and compare the marked live set
  against heap contents across APDU boundaries. Add tombstones and reuse only
  after that trace is complete. No collector or runtime test was run in this
  source-inspection step.

## Canonical roots for serialized static references — 2026-10-17

- **Cause:** `GETSTATIC_A` decoded the serialized reference from flash on every
  read. Each decode appended a fresh array or instance to the heap. Repeated
  reads therefore returned different handles and retained duplicate wrappers.
- **Viable fix:** Route `GETSTATIC_A` through `Static_Handler::getPersistentRef`
  and cache each restored handle in the owning `Heap`, keyed by resolved
  package ID and static-field number. `PUTSTATIC_A` updates the root only after
  the persistent write succeeds. The heap lifetime matches the selected
  applet context; this VM destroys that context on deselect, and GP DELETE
  deselects before deleting package state.
- **Regression:** JCSystem transaction-probe case 14 reads the install-written
  static object twice after restore and checks reference identity. The focused
  VPCD case returned `019000`; the full VPCD suite passed 17/17 in 109.117 s.
  PC and ARM builds passed. No hardware or RISC-V build was used.
- **Limit:** This only canonicalizes Java static references; it is not a
  collector and does not make heap slots reusable. If persistent transaction
  rollback becomes supported for static references, the in-memory root must
  also be reconciled on abort.


## Root and typed-edge visitor API — 2026-10-18

- **Implemented:** `Heap::appendRootReferences()` exposes cached
  static-reference roots. `Context::appendRootReferences()`
  adds AID, registration/restoration, APDU, and outgoing-buffer handles, and
  resolves context-held APDU `shared_ptr` mirrors back to stable heap handles.
- **Implemented:** `Heap::appendChildReferences()` follows only typed
  reference fields and reference-array entries. Walking persistent fields or
  arrays can deserialize children and append heap entries, so callers must use
  a work queue and tolerate heap growth; do not iterate a presumed fixed-size
  snapshot as though loading were pure.
- **Not implemented:** No mark state, APDU-boundary invocation, slot tombstones,
  reclamation, or slot reuse exists yet. These visitor methods do not inspect
  Java stack words or native locals. Callers must establish a quiescent boundary
  and account for stack return values and other VM-owned roots before any future
  reclamation. In particular, VM-owned roots that are not in the static cache
  are not surfaced by `Heap::appendRootReferences()` yet.
- **Validation:** PC, ARM M33, and RISC-V builds passed. Full VPCD
  JCSystem/transaction regression passed 17/17 in 78.115 s. No hardware was
  used. The first ARM build found a range-loop copy warning in the new context
  visitor; it was corrected to a const-reference loop before the passing ARM
  build.

## Mark-only typed heap traversal — 2026-10-18

- **Implemented:** `Heap::traceReachableFrom()` performs a work-queue walk from
  caller-supplied references, keeps array and instance marks separate, and
  deduplicates handles before traversing typed fields and reference-array
  elements. It checks reference offsets against current slot counts and grows
  the mark vectors when lazy persistent reads append objects during traversal.
- **Failure reporting:** Invalid handles and caught Java Card `Exceptions`
  increment separate counters and make the result incomplete. Host allocation
  failures are not swallowed. The returned totals reflect the heap after the
  walk, including objects loaded during edge traversal.
- **Limit:** This only traces the supplied roots and does not mutate or reclaim
  heap slots. It is not called by runtime code or yet validated against an
  actual complete root set. `Context` root enumeration currently includes
  static/context handles, but runtime exception roots are not yet exposed by
  the committed heap visitor; Java stack words and native locals also remain
  outside the visitor API. Never use this result to authorize reclamation.
- **Validation:** PC, ARM M33, and RISC-V builds passed. The full VPCD
  JCSystem/transaction regression passed 17/17 in 70.499 s. No board was
  flashed. The PC build retains pre-existing packed-member warnings.
- **Decision:** Next expose every VM-owned root and define a safe post-APDU
  boundary, then exercise the tracer diagnostically across repeated APDUs.
  Keep slot destruction and reuse disabled until root coverage and mark
  accounting are demonstrated.

## Runtime CryptoException handles are heap roots — 2026-10-18

- **Implemented:** The heap keeps separate reusable `CryptoException`
  instances for its ROM and imported security-package class views. The
  `NEW` factory path routes only matching package/class allocations to those
  slots. `Heap::appendRootReferences()` now includes each non-null runtime
  exception handle alongside cached Java static references.
- **Reason:** Repeated `CryptoException.throwIt()` calls must not append one
  permanent instance per call, and a future heap walk must retain the shared
  runtime-owned instances even though Java statics are not their owner.
- **Validation:** PC, ARM M33, and RISC-V builds passed. VPCD JCSystem suite
  passed 17/17 in 71.327 s, including the 50-throw stable-identity/reason
  check. No hardware was flashed. The root visitor itself is not yet invoked
  by the runtime tracer.
- **Limit:** This closes only the two runtime exception roots. Stack reference
  return values, active Java words, and native locals still need coverage or a
  proven quiescent boundary. Do not reclaim slots based on current tracing.
- **Decision:** Determine how to represent a typed root return value and prove
  the APDU caller has returned before running diagnostic mark accounting.

## Preserve top-level reference return values as typed roots — 2026-10-18

- **Implemented:** `Stack` now distinguishes a reference result from primitive
  top-level results. `ARETURN` stores the compact handle with a reference tag;
  short/int return paths clear the tag, and a new root frame clears the stale
  result state. `Context::appendRootReferences()` adds the cached reference
  result when one is present.
- **Reason:** The prior root-return cache stored only a `uint32_t`, so a
  reference returned to native code could not be distinguished from a
  primitive or included in the root set.
- **Validation:** PC, ARM M33, and RISC-V builds passed. Full VPCD JCSystem
  regression passed 17/17 in 70.323 s. No hardware was flashed.
- **Limit:** This covers only a top-level returned reference. It does not
  scan local/operand words in active Java frames or C++ native locals. A
  collector still requires a proven quiescent caller boundary.
- **Decision:** Locate the exact APDU call-return boundary and verify that the
  Java stack is empty and native references are out of scope before invoking
  mark-only tracing.

## Opt-in post-APDU heap trace — 2026-10-18

- **Implemented:** PC builds accept `PICO2_HEAP_TRACE=1` to enumerate roots
  and run the mark-only tracer after `choupi_vm_process_applet()` exits its
  APDU `try` scope. The interpreter and temporary native references from that
  scope have been destroyed; the diagnostic also requires `Stack::empty()`.
  Default PC behavior and embedded ARM/RISC-V code do not run this trace.
- **Observed:** The VPCD JCSystem suite produced 15 post-APDU trace samples.
  All reported `complete=1`, zero invalid handles, and zero edge-load
  failures; none were skipped for active Java frames. The final sample had
  8 roots, 6/16 arrays reachable, and 4/12 instances reachable. Lazy
  persistent reads caused heap totals to grow during the walk, confirming
  that the work queue follows newly loaded objects.
- **Validation:** PC, ARM M33, and RISC-V builds passed. VPCD regression
  passed 17/17 in 73.382 s with tracing enabled. No hardware was used.
- **Limit:** A complete mark result in this applet run does not prove every
  possible native/global root is accounted for, and unreachable counts are
  not yet evidence safe to clear. No slot was reclaimed or reused. The trace
  hook is diagnostic and remains opt-in.
- **Decision:** Audit remaining persistent/runtime root paths, add a focused
  test for root-set completeness, and only then design stable-slot tombstones.

### Global arrays are outside applet-heap tracing

- **Cause:** `jref_t` marks global arrays with the array bit plus an offset
  bit, but `Heap::traceReachableFrom()` previously treated every array handle
  offset as an index into the local applet heap. A global handle could
  therefore falsely mark an unrelated local array, and child traversal could
  walk a global array as if it were owned by the applet heap.
- **Viable fix:** Ignore global-array references when enqueueing local heap
  marks and when enumerating children. Global arrays are owned by the runtime
  global heap and follow the separate reset lifecycle.
- **Validation:** PC, ARM, and RISC-V builds passed. VPCD JCSystem cases 36–41
  passed with two trace passes after each APDU: 12 traces complete, zero
  invalid references, and zero edge-load failures. One local array was
  unreachable in cases 40–41, as expected; no slots were mutated. The focused
  log is `artifacts/logs/heap-global-tracer-focused-vpcd-20261018.log`.
- **Limit:** This fixes a tracer classification error; it does not establish
  complete roots for all native/global holders and does not authorize heap
  reclamation. Reclamation remains disabled.

### Pin loaded objects with persistent flash tags

- **Root gap:** Static reference values are loaded into
  `static_reference_roots` only on first access. A persistent object or array
  already loaded through another reference could therefore be absent from
  that cache when a post-APDU trace runs.
- **Viable conservative fix:** Enumerate every loaded array and instance with
  a persistent flash tag as a root. This pins all loaded persistent objects
  (and, through typed edges, their children) even if the associated static
  field has not been read in the current context. It may retain unreachable
  persistent objects, which is acceptable while object deletion remains
  unsupported.
- **Validation:** PC, ARM, and RISC-V builds passed. VPCD cases 14–16 passed
  for restored static identity, persistent array-child identity, and aliased
  persistent object fields. Six repeated traces were complete with zero
  invalid references and zero edge-load failures; all loaded arrays and
  instances were reachable. Log:
  `artifacts/logs/heap-persistent-roots-focused-vpcd-20261018.log`.
- **Limit:** This improves conservative root coverage but does not by itself
  prove that every native handle is represented. No heap slots are reclaimed.

### Follow-up: reclaim unreachable persistent objects safely — 2026-10-20

- **Finding:** Pinning every loaded persistent-tagged object caused the
  physical `ALG_PERFORMANCE_STATIC` run to grow to 1,082 arrays and 314
  instances, then panic with out-of-memory near transfer 798. GC summaries
  showed zero static roots and almost every loaded object pinned.
- **Correction:** Persistent flash records survive independently of RAM
  objects, so GC now marks from current context/runtime roots and reachable
  fields only. If a previously unreferenced object is later accessed through
  a lazy static field restore, its tag can reconstruct it. Mark state is stored
  on the heap object itself so each collection does not allocate temporary
  mark bitmaps while the heap grows.
- **Validation:** VPCD persistence test passed 3/3. The official JCAlgTest
  1.8.2 physical static suite completed at 2,262 transfers with no panic,
  invalid references, traversal failures, or errors other than expected
  `NO_SUCH_ALGORITHM` (1,772) and `CANT_BE_MEASURED` (95). GC settled at
  83 array slots and 28 instance slots, with 33 arrays and 25 instances
  reachable; an early collection reclaimed 50 arrays. SHA-256 measurements
  were 3.10 ms/op for 256-byte `doFinal`, 3.34 ms/op for `update`, and
  6.11 ms/op for `reset`.
- **Status:** Viable for tested workload and persistence shapes. Continue
  checking static reference roots and object aliasing across more applets; the
  result does not prove collector root completeness for all Java Card apps.
- **Evidence:** `artifacts/logs/jcalgtest-static-gc-root-fix-20261020.log`,
  `artifacts/logs/monitor-static-gc-root-fix-20261020.log`, and
  `artifacts/results/physical-perf-static-gc-root-fix-20261020/`.

### Repeated-walk validation

- The PC diagnostic accepts a repeat count from `PICO2_HEAP_TRACE`, clamped
  to 1–4. With value `2`, the VPCD JCSystem suite passed 17/17 in 96.618 s
  and produced 15 pairs of consecutive walks. Every one of the 30 walks was
  complete, with zero invalid references and zero edge-load failures; no
  pair's array or instance totals grew between pass 1 and pass 2.
- This rules out tracer-induced duplicate deserialization for the persistent
  graph shapes exercised by this probe. It does not prove canonical identity
  for every persistent reference-array/alias graph or prove root completeness
  for all applets. Repeated tracing is diagnostic only and has measurable
  overhead; leave it disabled in normal operation.

## Persistent reference-array child identity — 2026-10-18

- A focused VPCD probe exposed a separate identity defect: a persistent
  `Object[]` containing the same `byte[]` twice restored two different child
  handles (`first == second` was false). Persistent object references already
  had alias handling, but array references serialized each occurrence under a
  distinct field tag.
- Persistent arrays now retain their canonical flash tag after a successful
  write. Repeated writes at another reference location store an array-alias
  record; reads resolve that record back to the canonical array tag. Heap
  lookup by persistent tag also reuses a previously loaded instance or array.
- Focused case 15 passed with `019000`; the full VPCD JCSystem suite passed
  18/18 in 81.386 s. The two-pass diagnostic emitted 18 paired trace results;
  every walk completed with zero invalid handles and zero edge-load failures.
  No hardware was flashed or accessed.
- This verifies repeated array children in the exercised persistent array
  shape. It does not cover cyclic object arrays, all persistent field layouts,
  or safe slot reclamation. Keep tombstones and reuse disabled pending broader
  root and alias validation.

## Persistent instance fields sharing an array — 2026-10-18

- Added a second VPCD probe where two fields of a persisted holder refer to one
  byte array. With the holder fields typed `byte[]`, case 16 verified both
  `first == second` and a write through the second field observed through the
  first. Focused case passed `019000`; full suite passed 19/19 in 85.020 s.
  All 34 repeated heap walks completed with zero invalid refs or edge failures.
- An initial version declared the fields as `Object` and cast them back to
  `byte[]`; that APDU failed with `ClassCastException` at `checkcast` opcode
  `0x94`. Replacing the field declarations with their actual `byte[]` type
  made the identity/mutation probe pass. The cast failure is separate from the
  alias result and remains untriaged; do not treat the typed-field result as a
  fix for general `checkcast` behavior. Evidence is in
  `artifacts/logs/jcsystem-persistent-object-array-alias-vpcd-20261018.log`.
- No embedded source changed, no board was accessed, and no reclamation was
  enabled. Next isolate the cast failure, then add root/alias graph shapes
  before considering tombstones.

## Array `checkcast` type-code mapping — 2026-10-18

- A focused cast from `Object` to `byte[]` failed with `ClassCastException`
  (`opcode=0x94`). `docheck()` directly cast the bytecode's `atype` to
  `jc_array_type`; the bytecode encodes array types as decimal `10`–`14`, while
  the internal enum uses `0x10`–`0x14`. These representations do not match.
- Replaced the cast with an explicit mapping for boolean, byte, short, optional
  int, and reference arrays. Unsupported/invalid codes raise
  `SecurityException`; valid but mismatched array types still produce a
  failed type check.
- Focused VPCD case 17 passed (`019000`), including after case 16 had mutated
  the shared array. PC, ARM, and RISC-V builds passed. Full VPCD JCSystem suite
  passed 20/20 in 114.250 s; all 36 repeated heap walks were complete with
  zero invalid refs and zero edge-load failures. No board was accessed.
- This validates a positive byte-array cast. Add a negative cast regression
  for mismatched array types before treating the full `checkcast` family as
  covered. The explicit type mapping is the viable fix for this mismatch.

### Correction — catch and reuse VM-generated ClassCastException

- The first negative-cast regression exposed a second defect: `checkcast`
  threw the C++ `Exceptions::ClassCastException` directly, which bypassed
  Java exception-table dispatch and returned `6F00` even inside a Java catch.
- The bytecode now resolves `java.lang.ClassCastException` from the applet's
  imports, obtains a per-heap JCRE-owned instance, and sends it through the
  normal `doThrow()` handler search. The heap visitor includes this exception
  root. Repeated mismatched casts reuse the same instance.
- Case 18 passed 50 mismatched casts in one APDU, checking both catch behavior
  and stable exception identity. Full VPCD suite passed 21/21 in 110.937 s;
  all 38 repeated heap walks completed with zero invalid refs and zero
  edge-load failures. Final sample: arrays 6/16, instances 6/14. PC, ARM, and
  RISC-V builds passed; no hardware was accessed.
- This establishes catchable/reusable ClassCastException for this path. Other
  VM-generated runtime exceptions still need review for consistent Java
  handler dispatch.

## VM-generated Java runtime exception dispatch — 2026-10-18

- **Symptom:** `arraylength` on a null reference raised internal
  `Exceptions::NullPointerException`; the interpreter caught it as a VM error
  and returned `6F00`, even though the CAP had a matching Java catch handler.
- **Cause:** Bytecode implementations use C++ `Exceptions` values for runtime
  faults, but the interpreter's per-instruction catch path called the VM-fatal
  status handler directly. Those internal exceptions never became Java object
  references and never entered `Bytecodes::doThrow()`'s exception-table search.
- **Viable fix:** At the per-instruction catch point, map supported VM
  exception values to class export tokens in the applet's `java.lang` import,
  obtain one rooted reusable exception instance per imported class in the
  applet heap, and send it through `doThrow()`. Resume interpretation when
  Java finds a handler. Leave security/unmapped exceptions on the existing
  fatal path. Current tested mappings are index bounds, array index bounds,
  negative array size, null, class cast, arithmetic, and generic runtime.
- **Related bounds defect:** `JC_Array` delegated upper-bound errors to the
  generic `JCVMArray` backing store, which raised `IndexOutOfBoundsException`.
  All typed array read/write entry points now validate against the Java array's
  logical length and raise `ArrayIndexOutOfBoundsException` before touching
  storage. Keep this check at the Java array boundary; generic list/array
  container checks retain their own exception semantics.
- **Evidence:** Focused VPCD cases 19–22 caught null, array-index, arithmetic,
  and negative-size exceptions (`019000` for all). Case 18's 50 mismatched
  casts still passed. Full suite passed 25/25 in 104.317 s; 23 pairs (46 total)
  of heap trace walks were complete with zero invalid refs and zero edge-load
  failures. PC, ARM, and RISC-V builds passed. No board was accessed.
- **Limit:** This is not exhaustive. `ArrayStoreException`, VM security
  violations, and other exception values not listed in the mapping remain
  outside Java dispatch and must be audited separately. Heap reclamation stays
  disabled.

### Correction — dispatch ArrayStoreException

- JC 3.0.5's checked-in `java/lang/javacard/lang.jca` exports
  `ArrayStoreException` at token 11. The dispatcher now maps internal
  `Exceptions::ArrayStoreException` to that class. A VPCD probe assigned the
  active applet instance into an `Exception[]` through an `Object[]` reference;
  Java caught the VM-raised mismatch and returned `019000`.
- PC, ARM, and RISC-V builds passed. Full VPCD suite passed 26/26 in 116.840 s;
  24 pairs (48 total) of heap walks were complete with zero invalid references
  or edge-load failures. No board was accessed.
- The initial probe used `new Object()` and the PC emulator terminated with an
  uncaught `jcvm::Exceptions` before reaching the array store. Allocation of
  `Exception[]` and same-type `Exception` stores work, and using the active
  applet instance reliably exercises the mismatched store. **Open issue:**
  isolate and fix the `new java.lang.Object()` path; current evidence does not
  identify the throwing source. Logs:
  `artifacts/logs/jcsystem-arraystore-exception-dispatch-vpcd-20261018.log` and
  `artifacts/logs/jcsystem-arraystore-object-allocation-probe-vpcd-20261018.log`.
- The earlier note that `ArrayStoreException` was unmapped is superseded by
  this correction. Security violations and other unmapped exception values
  remain outside Java dispatch.

### Correction — dispatch JCSystem SystemException

- `JCSystem.getAvailableMemory(byte)` forwards an invalid type into the native
  binding, which raises internal `Exceptions::SystemException`. The Java class
  is in the imported `javacard.framework` package, not `java.lang`; its JC 3.0.5
  class export token is 13. Exception lookup now matches a requested package
  AID and token, and the SystemException path sets `reason` field 0 to
  `ILLEGAL_VALUE` (1) before Java handler dispatch.
- Case 24 passed through VPCD: type 3 was caught as `SystemException`,
  `getReason()` equaled `SystemException.ILLEGAL_VALUE`, and response was
  `019000`. Full suite passed 27/27 in 98.571 s. All 50 heap walks were
  complete with zero invalid references and zero edge-load failures. PC, ARM,
  and RISC-V builds passed; no board was accessed.
- This covers the invalid-type native path only. Other framework exception
  values and reason codes still need explicit mappings and tests. The generic
  VM exception enum does not carry reasons, so only paths with a known reason
  should synthesize one.

## Root java.lang.Object allocation terminated the emulator — 2026-10-18

- **Symptom:** A valid applet APDU containing `new Object()` terminated the PC
  emulator with `terminate called after throwing an instance of
  'jcvm::Exceptions'`. Allocating `Exception[]` and constructing other
  non-root classes worked.
- **Cause:** `Class_Handler::getInstanceFieldsSize()` used a `do/while` loop
  that always resolved one superclass. For root `java.lang.Object`, whose
  superclass reference is absent, that lookup raised `SecurityException`.
  The exception escaped through `JC_Instance` constructors declared
  `noexcept`, which caused termination before the interpreter's Java/VM
  exception handling.
- **Viable fix:** Walk class sizes with an explicit root check before resolving
  the parent. Remove `noexcept` from `JC_Instance` constructors that perform
  CAP lookup or class field sizing, preserving normal error propagation for
  invalid CAP metadata. Case 25 (`new Object()`) now passes (`019000`).
- **Regression evidence:** PC, ARM, and RISC-V builds passed. Full VPCD suite
  passed 28/28 in 102.270 s; 26 pairs (52 total) of heap walks completed with
  zero invalid references and zero edge-load failures. No board was accessed.
- The prior ArrayStoreException diagnostic about `new Object()` is superseded
  by this fix. It had been a constructor-path failure before the mismatched
  array store, not a dispatch failure.

## VM-raised APDUException bypassed Java catch handlers — 2026-10-18

- **Cause:** Internal APDU state-machine errors were represented by
  `Exceptions::APDUException`, but the VM-to-Java exception dispatcher had no
  Java Card framework mapping for that enum value. A handler importing
  `javacard.framework.APDUException` therefore could not catch native APDU
  state violations.
- **Viable fix:** Resolve the framework APDUException through export token 12
  and initialize its reason field to `ILLEGAL_USE` for the current internal
  APDU state violations. The focused probe invokes
  `setIncomingAndReceive()` twice, returns immediately after the catch, and
  reads the captured reason on a subsequent APDU. It passed with reason 1.
- **Test-harness note:** A first probe called `setOutgoing()` and then used the
  normal final response path; its `6F00` was from trying to send after the APDU
  state had already failed. Returning from `process()` and checking the stored
  catch result on the next APDU isolates exception dispatch from response
  transmission state.
- **Evidence:** PC, ARM, and RISC-V builds passed. Focused VPCD cases 26–27
  passed. Full VPCD suite passed 30/30 in 159.731 s. No physical board was
  accessed.

## Invalid transient-array clear event raised the wrong exception — 2026-10-18

- **Cause:** `getTransientClearEvent()` returned the two supported lifecycle
  events but raised internal `SecurityException` for any other value. The API
  contract specifies `SystemException.ILLEGAL_VALUE`, which is catchable by an
  applet.
- **Viable fix:** Raise internal `SystemException`; the VM's existing mapping
  supplies the `ILLEGAL_VALUE` reason. Case 29 passes for event value 3.
- **Coverage:** Case 28 creates byte, boolean, short, and object transient
  arrays with reset/deselect events; writes and reads values; and verifies
  `JCSystem.isTransient()` reports the requested event. Focused and full VPCD
  tests passed; the full suite passed 32/32 in 113.436 s. PC, ARM, and RISC-V
  builds passed. No physical device was used.
- **Lifecycle cause:** VPCD deselection destroyed the selected applet context;
  restoration rebuilt transient arrays with zeroed contents for both event
  types. This incorrectly cleared `CLEAR_ON_RESET` arrays on deselection.
  `Heap::clearTransientArrays()` existed but had no runtime call sites.
- **Viable lifecycle fix:** Retain and reuse the same applet `Context` for a
  same-AID reselection. After invoking the applet's `deselect()` method, clear
  only `CLEAR_ON_DESELECT` arrays. On card reset/power-off, additionally clear
  `CLEAR_ON_RESET` arrays. VPCD selection no longer invokes the reset clear
  callback. Cases 30–33 verify reset-array retention across deselect, deselect
  clearing, and both events clearing after card reset.
- **Evidence:** Focused lifecycle tests passed; full VPCD suite passed 36/36 in
  114.875 s. PC, ARM, and RISC-V builds passed. No physical device was used.
- **Scope:** The runtime has one selected applet context; this fix reuses that
  context when the same AID is selected again. Cross-applet context caching
  and shareable-interface behavior remain outside this verified path.

## Heap reclamation must preserve Java Card handles — 2026-10-18

- **Cause/risk:** Java references encode offsets into heap tables. Erasing an
  unreferenced entry shifts later offsets and corrupts still-live references.
  Persistent static objects can also be lazily restored, so cached static
  roots alone do not cover every loaded persistent object.
- **Viable approach:** Trace typed object and array fields from context/runtime
  roots, then reclaim only after a complete walk. Leave empty table slots as
  tombstones so surviving handles stay stable; reuse tombstones on later
  allocation. Pin every loaded persistent-tagged object until deletion and
  persisted-root enumeration are implemented. Never sweep the separate global
  array heap as part of an applet heap walk.
- **Safety condition:** Run only with an empty Java frame stack, after native
  interpreter scopes have returned. If root enumeration or edge traversal
  fails, skip the entire sweep. Allocation failure during tracing must also
  leave heap slots untouched.
- **Evidence:** VPCD cases 49–54 each allocated 64 arrays or objects across
  repeated APDUs; the second and third batches reused slots. All 17 selected
  GC and root regressions passed, with complete traces, zero invalid refs, and
  zero edge failures. A 15-minute physical `ALG_PERFORMANCE_STATIC` attempt
  completed 1,297 APDU measurements before the wrapper timeout; all 1,297
  returned `6F00` in cipher measurement rows, with no allocation failure or
  reader disconnect in the client log. Bounded serial output showed
  `TEST:PASS choupi_runtime` and no GC-skip or panic marker.
- **Limitations:** Loaded persistent objects remain pinned, so this is not
  persistent-object deletion or full garbage collection. The static suite
  remains incomplete; cipher rows after the interface-token fix still require
  validation. Do not treat expected `NO_SUCH_ALGORITHM` responses as failures.

## JC305 DESKey interface token must be remapped — 2026-10-18

- **Symptom:** The exact JCAlgTest `B0 31` DES CBC no-padding prepare returned
  `6F00`; serial tracing showed `SecurityException` (29) through
  `invokeinterface` in the applet's key setup. Bypassing heap collection and
  reflashing produced the same exception, ruling out the new collector.
- **Cause:** JC305 imports `DESKey` as interface token 10, while the generated
  CHOUPI security export assigns `DESKey` interface token 12. The new
  cross-package interface resolver correctly stopped applying class-token
  mappings to interface references, but had no security-package interface map
  for this token. The receiver therefore failed the interface identity check.
- **Viable fix:** In the JC305 compatibility path, map security AID
  `A0000000620102`, interface token 10 to generated token 12. Keep interface
  token mapping separate from method-token mapping. The exact physical probe
  now returns `AA9000` with heap collection enabled.
- **Validation:** ARM, RISC-V, and PC builds passed; the physical CHOUPI cycle
  passed; PC/SC selected the Pico reader; physical SELECT, GETVERSION
  `1.8.2_jc305`, JCSystem info, and DES prepare all returned `9000`. The full
  static matrix still needs rerunning after this fix.
- **Regression guidance:** When adding imported interface resolution, compare
  interface token tables separately from class tokens and virtual/interface
  method tokens. A stale/unmapped identity can raise Java `SecurityException`
  before the applet's expected `CryptoException` handling runs.

## Bound serial configuration when Pico USB is re-enumerating — 2026-10-18

- **Symptom:** After flashing, `tools/monitor` found `/dev/ttyACM0` but hung
  indefinitely in `stty -F ...`. The retry loop looked finite, but each `stty`
  open could block while the VM/USB ACM node was stale. This stalled the whole
  physical cycle after a successful flash.
- **Viable fix:** Wrap each `stty` attempt in
  `timeout --foreground --kill-after=1s 1s`; keep the existing bounded retry
  count and finite `cat` capture. This makes both the per-attempt open and the
  overall monitor startup bounded.
- **Validation:** `bash -n tools/monitor` passed. The finite monitor captured
  CHOUPI runtime PASS, and the following `./tools/cycle choupi` reached
  `CYCLE:PASS` with the Pico reporting `TEST:SUMMARY pass=1 fail=0`.
- **Caution:** Run PC/SC reader checks only in the authorized host-access
  context and wrap `pcsc_scan -r` with a short timeout. Unrestricted scans in
  this VM can remain active and flood errors when pcscd has no usable device.

## Physical JCAlgTest static run — CryptoException catch failure — 2026-10-18

- After the JC305 `DESKey` interface-token fix, the official JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` client completed its full result matrix on the
  physical Pico 2. The reader remained present. Across 7,785 CSV rows, the
  client recorded 1,852 `CANT_BE_MEASURED` failures and 40
  `NO_SUCH_ALGORITHM` results. SHA-256 and DES CBC NOPAD both produced timing
  measurements; the expected unsupported-algorithm response remains valid.
- **Isolated failure:** DES CBC ISO9797 M1 prepare (`B0 31`, algorithm ID 2)
  returned `6F00` after an applet-side `ATHROW`. The `AlgPerformanceTest`
  source catches `CryptoException` in `prepare_class_Cipher()` and should
  return a one-byte algorithm-exception result with `9000`. The serial trace
  instead ended at `VPCD:APPLET_PROCESS status=6F00 exception=29 opcode=93`
  and showed exception frames through package 63 offsets 15671/15705. A PC/SC
  reset restored SELECT, then the single isolated prepare reproduced `6F00`.
- **Status:** This is a VM exception-dispatch/catch issue, not a request to
  implement the unsupported padding algorithm. Fix exception handling so the
  applet can report `NO_SUCH_ALGORITHM` normally; do not count the algorithm as
  supported. After the long run, a later SELECT also failed until card reset,
  so audit frame cleanup after uncaught VM exceptions as part of this issue.
- Evidence:
  `artifacts/results/physical-perf-static-des-interface-fix-20261018/ALGTEST_log_1792328303323.log`
  (SHA-256 `3d869a23c9350a02839d0e5f10a948cf110365ae7de17a3c11305904bf74c1e0`),
  its CSV (`f4abcb6507fced3a6ece90902f5b33bd6c24b07ba38210adc4b9c08f404904ba`),
  full-run serial trace
  `artifacts/logs/monitor-static-des-interface-fix-20261018.log`, and isolated
  trace `artifacts/logs/monitor-des-m1-reset-isolated-20261018.log`.

## Correction — ROM and imported exception class views — 2026-10-18

- The isolated DES M1 trace shows that the thrown `CryptoException` is the
  imported security view (package 2, class offset 233, reason 3). After the
  catch-class alias was added, the applet executed its catch and threw
  `ISOException(0x9000)`. That object was the ROM view at package 0, class
  offset 350; the JC305 exported `ISOException` view resolved to offset 316.
  The existing cast check returned false for this pair, so the APDU still
  ended in `6F00`.
- The current source treats the ROM and imported `CryptoException` records as
  the same class during catch matching, and recognizes ROM `ISOException`
  offset 350 as well as exported offset 316 in the terminal status-word path.
  This is a narrow alias correction; it adds no algorithm implementation.
- PC, ARM, and RISC-V build wrappers pass. Physical validation now confirms the
  exact DES CBC ISO9797 M1 prepare returns `03 9000`, followed by a successful
  SELECT in the same PC/SC session. The official static run also returned
  `03 9000`/`NO_SUCH_ALGORITHM` for M1 and continued into neighboring cipher
  tests; the exception-alias correction is viable for this path.
- Evidence: `artifacts/logs/monitor-des-m1-alias-fix-20261018.log`,
  `artifacts/logs/monitor-des-m1-iso-class-check-20261018.log`,
  `artifacts/logs/cycle-choupi-20261018T131939Z.log`, and
  `artifacts/logs/vpcd-des-m1-iso-alias-20261018.log`.
- Physical validation evidence after the correction:
  `artifacts/logs/physical-m1-exception-alias-20261018.log` (SHA-256
  `88966d8b0f6844b9b0b4dc323383af73ad6dbbc86af5f02e4cc9be869cdc557e`),
  `artifacts/logs/jcalgtest-static-exception-alias-20261018.log`, and
  `artifacts/logs/physical-des3-static-probe-20261018.log`.

## PC/SC reader disappears during physical static run — 2026-10-18

- **Symptom:** The official static client completed 382 CSV rows, including
  SHA-256 and the M1 `NO_SUCH_ALGORITHM` rows, then reported
  `SCARD_E_READER_UNAVAILABLE` while measuring 3-key DES CBC NOPAD and exited
  because its noninteractive stdin had no retry response. The Pico remained
  enumerated as USB `2e8a:0009`; a first ACL-helper run raced enumeration and
  found zero devices.
- **Recovery:** A second
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` found the re-enumerated node and
  restored the PC/SC reader. The finite serial monitor reported runtime
  `TEST:PASS`. A focused 3-key DES CBC NOPAD prepare and 50-call measurement
  then returned `AA9000`; a follow-up SELECT also succeeded.
- **Status:** Firmware remained responsive and the isolated failing workload
  passed. The static matrix is incomplete, so this does not establish full
  static-suite success. A later run with serial capture showed that the
  reader-unavailable result followed firmware heap exhaustion, so the apparent
  transport loss is a consequence of the VM panic rather than an independent
  PC/SC problem.
- **Regression guidance:** After `SCARD_E_READER_UNAVAILABLE`, inspect USB
  enumeration separately from PC/SC, rerun the authorized ACL helper once the
  USB node exists, confirm the reader with a short bounded scan, and validate
  card response with a focused APDU before restarting a long suite. Do not
  treat a client-side reader loss as an algorithm result.

## Interpreter frames were left active after APDU exceptions — 2026-10-18

- **Symptom and cause:** After an applet intentionally caught an unsupported
  algorithm `CryptoException` and used `ISOException.throwIt(0x9000)` to return
  the JCAlgTest result, `Interpretor::run()` returned the status word without
  unwinding its Java frames. `collect_heap_after_apdu()` then reported
  `HEAP_GC_SKIP reason=active_java_frames`. Repeated `NO_SUCH_ALGORITHM`
  requests accumulated objects and later ended in `malloc` failure and panic.
- **Viable fix:** Add `Stack::unwind()` to clear frames and any stale root return
  value on every exceptional/aborted interpreter exit, including the terminal
  ISO status path. Keep root return values on normal completion.
- **Validation:** PC, ARM, and RISC-V builds passed; the physical cycle passed.
  Fifty M1 prepare calls each returned `03 9000`, followed by a 50-call
  3-key-DES CBC NOPAD measurement returning `AA9000` and a successful SELECT.
  The concurrent serial capture had no `HEAP_GC_SKIP` or panic during this
  focused sequence.
- **Remaining issue:** A full static rerun reached 512 CSV rows and then
  panicked with `malloc 3244 failed to allocate memory` while the collector
  was tracing the AES CBC PKCS5 prepare. `SCARD_E_READER_UNAVAILABLE` was the
  client-visible consequence. The capture shows a 1,622-byte allocation
  immediately before the 3,244-byte request; inspect pending-worklist growth
  in `Heap::traceAndMark()` and reserve no more than the total stable handle
  count before claiming the full static run is fixed.
- Evidence: `artifacts/logs/physical-heap-unwind-stress-20261018.log`,
  `artifacts/logs/monitor-heap-unwind-stress-20261018.log`,
  `artifacts/logs/jcalgtest-static-heap-unwind-final-20261018.log`, and
  `artifacts/logs/monitor-static-heap-unwind-final-20261018.log`.

## Collector scratch vectors exhausted RP2350 heap — 2026-10-18

- **Symptom:** After exception frames were unwound, physical
  `ALG_PERFORMANCE_STATIC` still panicked around 512 rows. First the tracing
  worklist grew from 1,622 to 3,244 bytes and failed. Reserving the maximum
  local handle count avoided that growth pattern but requested a 1,642-byte
  contiguous block and failed around 508 rows. Removing the worklist and child
  vector advanced to 528 rows, then the temporary root vector requested 2,048
  bytes and failed.
- **Viable collector fix:** Stream root and child references directly into
  stable-slot mark bitmaps. Reuse each mark byte as `0` unreachable, `1` marked
  and pending, and `2` scanned; repeatedly scan the stable handle tables until
  no new references are marked. This removes the temporary root list, pending
  queue, and child-reference vector. The sweep remains gated on a complete
  trace, and stable handle slots are preserved.
- **Validation:** ARM, RISC-V, and PC builds passed. The physical CHOUPI cycle
  passed (`TEST:SUMMARY pass=1 fail=0`). The official static client reached
  544 CSV rows, beyond the previous OOM point, with no allocation-failure,
  panic, or GC-skip marker in serial output. The client then lost PC/SC access
  during an AES-192 APDU. A later focused trace showed SELECT reached the
  firmware, but GETVERSION was not received. A subsequent Java scan could not
  see the reader despite the authorized USB ACL helper finding the device.
  Another focused probe after the earlier interruption did pass SELECT,
  GETVERSION, JCSystem info, and DES prepare (`AA9000`). Thus the collector OOM
  is addressed through the observed threshold, but full static completion is
  still unverified; the remaining interruption appears in the host USB/PCSC
  path and needs a separate diagnosis.
- **Regression guidance:** During future long physical runs, capture serial
  output in parallel. On `SCARD_E_READER_UNAVAILABLE`, check whether the APDU
  appears in CHOUPI's CCID counters/logs before attributing it to the VM. Run a
  focused SELECT/GETVERSION after restoring access; do not count skipped or
  transport-failed rows as algorithm outcomes.
- Evidence: `artifacts/logs/cycle-choupi-gc-stream-roots-final-20261018.log`,
  `artifacts/logs/monitor-static-gc-stream-roots-20261018.log`,
  `artifacts/logs/jcalgtest-static-gc-stream-roots-20261018.log`,
  `artifacts/logs/monitor-gc-stream-roots-recovery-probe-20261018.log`, and
  `artifacts/logs/physical-gc-stream-roots-recovery-20261018.log`. The rejected
  allocation strategies are captured in
  `artifacts/logs/jcalgtest-static-worklist-bound-20261018.log`,
  `artifacts/logs/monitor-static-worklist-bound-20261018.log`,
  `artifacts/logs/jcalgtest-static-gc-bitmap-20261018.log`, and
  `artifacts/logs/monitor-static-gc-bitmap-20261018.log`.

## PC/SC daemon hangs while Pico USB device remains present — 2026-10-18

- **Symptom:** After a physical static run or focused APDU, Java may report
  `SCARD_E_READER_UNAVAILABLE`, and `TerminalFactory` may list only the two
  Virtual PCD readers. In the latest reproduction, USB still listed the Pico
  as `2e8a:0009`, `pcscd.service` was active, and the authorized
  `pico2-pcsc-reader` helper returned `PCSC:ACCESS_GRANTED`; nevertheless,
  `timeout 5s pcsc_scan -r` expired without printing a reader list.
- **Firmware evidence:** A concurrent serial capture recorded SELECT and
  `9000`, but no following GETVERSION APDU. A separate probe after an earlier
  interruption did complete SELECT, GETVERSION, JCSystem info, and DES prepare
  (`AA9000`). USB visibility alone does not prove PC/SC is responsive.
- **Recovery:** Restarting `pcscd.service` and running the authorized ACL
  helper has restored readers in earlier setup. The assistant cannot restart
  the service autonomously because repository `AGENTS.md` only permits sudo
  for `/usr/local/sbin/pico2-pcsc-reader`. Ask the operator to run
  `sudo systemctl restart pcscd.service`, then rerun the helper and verify with
  a bounded `pcsc_scan -r`.
- **Evidence:** `artifacts/logs/pcsc-host-state-gc-stream-roots-20261018.log`,
  `artifacts/logs/monitor-gc-stream-roots-recovery-probe-20261018.log`, and
  `artifacts/logs/physical-gc-stream-roots-recovery-final-20261018.log`.

## D92 PC/SC APDU path stalled after second-package load attempt — 2026-10-25

- **Symptom:** After flashing `symm_crypto` to Pico D92
  (`D92E743D5F640C0F`), PC/SC listed the reader and `pcsc_scan` reported card
  inserted with ATR `3B800181`, but bounded GPPro `--list`, raw `--apdu`, and
  Java SmartcardIO attempts timed out. The authorized helper refreshed USB
  ACLs and restarted `pcscd.service`; this did not restore APDU access.
- **Trigger/evidence:** D92 already had the explicitly installed upstream
  JCAlgTest package. A second, uniquely identified transaction-probe CAP
  completed LOAD (`F01234567890`) but GPPro INSTALL did not return within about
  one minute and was interrupted. After firmware recovery/reflash, serial
  monitoring continued to pass runtime checks. CCID diagnostics showed
  repeated `PC_to_RDR_GetSlotStatus` (`msg=65`) while PC/SC clients stalled;
  one capture also showed GET STATUS APDU `84F210020A`. The second CAP's final
  installed state is unknown; do not claim it was installed or removed.
- **Recovery status:** A second full ARM cycle passed flashing, verification,
  and finite runtime monitoring. It did not restore GPPro/SmartcardIO access.
  Only D92 appeared in PC/SC; `picotool info` found no BOOTSEL boards, so no
  visible physical failover target was available. Keep USB/PCSC operations
  bounded and inspect the CCID trace before retrying package writes. No robust
  solution for this instance is established yet.
- **Latest retest:** After the user reported tokens in BOOTSEL, the VM still
  exposed only runtime D92 (`2e8a:0009`). The repository cycle requested
  BOOTSEL by the unique CDC serial, flashed and verified the ARM image, then
  passed the 18-second `TEST:SUMMARY pass=1 fail=0` monitor. The ACL helper and
  `pcsc_scan` again listed D92 with inserted-card ATR `3B800181`, but a bounded
  read-only GPPro `--list` still timed out after 8 seconds. No package write was
  retried, and no second physical board was visible to the VM. The applet
  registry state therefore remains unknown.
- **Artifacts:**
  `artifacts/logs/cycle-choupi-20261025T025305Z.log` and
  `artifacts/logs/cycle-choupi-20261025T025904Z.log`,
  `artifacts/logs/cycle-choupi-20261025T032241Z.log`, and
  `artifacts/logs/pico-powerup-persistence-20261025T032441Z.log`.

## Host VPCD timed out while updating a 20 KB persistent byte array — 2026-10-25

- **Symptom:** A local `JCTransactionProbe` APDU created two persistent
  20,000-byte arrays, began a transaction, and attempted to modify each so the
  snapshots would exceed commit capacity. The VPCD client selected the applet,
  but the first persistent byte-array write did not return within the 90-second
  APDU timeout.
- **Cause:** Not established. The emulator log reaches the first
  `PERSIST_BYTE_WRITE` diagnostic and then stops; the timeout alone does not
  distinguish persistent-store latency/failure from JCVM execution failure.
- **Viable workaround:** Use the native persistent-transaction test to verify
  `BUFFER_FULL`, capacity preservation, and rollback, and use the VPCD applet
  for bounded small-field accounting. Do not retry large-array VPCD probes
  without adding scoped timing/phase diagnostics first.
- **Limit:** This leaves catchability of a real Java `BUFFER_FULL` during a
  large persistent-array update unverified. It does not invalidate the native
  overflow result or the VPCD commit-capacity getter checks.
- **Evidence:**
  `artifacts/logs/jcsystem-transaction-large-array-overflow-vpcd-20261025.log`.

## VM lost Pico USB passthrough during final transaction-capacity flash — 2026-10-25

- **Symptom:** The user reported Pico tokens in BOOTSEL. The final cycle targeted
  D92 `D92E743D5F640C0F`, but picotool could not find its reset interface. The
  1200-baud CDC fallback, three picotool reboot requests, and bounded BOOTSEL
  waits did not expose a board to the VM.
- **Observed host state:** Authorized `lsusb` listed only Linux root hubs;
  authorized `picotool info -a` reported no accessible RP-series devices in
  BOOTSEL. This is consistent with USB passthrough being absent; it does not
  establish a board hardware failure.
- **Viable next action:** Reattach/passthrough any BOOTSEL Pico to this VM, then
  use the identified chip ID or another enumerated board from the failover
  pool. Confirm identity and run the repository cycle only once a device is
  visible. The offline builds and VPCD checks can proceed without the board.
- **Result:** No final transaction-capacity image was flashed; card registry
  state is unknown. No package writes were attempted.
- **Evidence:** `artifacts/logs/cycle-choupi-20261025T034755Z.log`.

## Pico power-up check sees USB device but no PC/SC reader — 2026-10-19

- **Symptom:** `tools/test-pico-powerup` found USB VID/PID `2e8a:0009` and the
  authorized ACL helper granted `/dev/bus/usb/001/004`, but GPPro listed only
  `Virtual PCD 00 00` and `Virtual PCD 00 01`. A subsequent bounded
  `pcsc_scan -r` gave the same two readers. Earlier in this session, GPPro had
  briefly listed the Pico and confirmed the JCAlgTest package/applet, then the
  Java APDU probe lost the reader during transmit.
- **Mitigation:** The power-up check now retries registry discovery and the
  applet APDU probe separately, refreshing the ACL each time. Defaults are
  three attempts and 20 seconds per registry/APDU attempt; configure them with
  `PICO2_POWERUP_ATTEMPTS`, `PICO2_POWERUP_LIST_TIMEOUT_SECONDS`, and
  `PICO2_APDU_TIMEOUT_SECONDS`.
- **Result:** Retries ended in a clear bounded failure because `pcscd` did not
  enumerate the device. The physical persistence check remains inconclusive.
  The known recovery is to restart `pcscd.service` from an interactive shell,
  run the authorized ACL helper, and verify a Pico reader with a bounded scan.
  The assistant's sudo exception does not permit restarting the service.
- **Evidence:** `artifacts/logs/pico-powerup-persistence-20261019T084640Z.log`,
  `artifacts/logs/pico-powerup-persistence-20261019T085227Z.log`, and the
  retry after reinsertion at
  `artifacts/logs/pico-powerup-persistence-20261019T092025Z.log`. The last run
  again acquired USB ACL access but timed out on all three GPPro registry
  attempts; it did not reach the applet APDU phase.
- **Latest retry:** On reinsertion, `tools/test-pico-powerup` again acquired
  the USB ACL for `/dev/bus/usb/001/004`; `lsusb` showed `2e8a:0009`, while
  GPPro and `pcsc_scan -r` listed only the two Virtual PCD readers. The
  bounded three-attempt run ended before package or applet verification.
  Restart `pcscd.service` interactively, refresh the ACL, and confirm the Pico
  reader before retrying. Evidence:
  `artifacts/logs/pico-powerup-persistence-20261019T105115Z.log`.

### Reinserted-device retry and descriptor check — 2026-10-19

- **Observation:** The repeated `tools/test-pico-powerup` run granted access to
  `/dev/bus/usb/001/004` but GlobalPlatformPro still listed only the two
  Virtual PCD readers. `lsusb -v` confirmed the Pico advertises three
  interfaces, including interface 2 class `0x0B` (Chip/SmartCard), T=1, and
  CCID bulk endpoints `0x03`/`0x83`. The installed libccid `Info.plist` has
  entries for VID/PID `2E8A:0009`.
- **Host state:** `pcscd.socket` is active and listening while
  `pcscd.service` is inactive between client connections. This is normal for
  the configured socket-activated, auto-exit service; service inactivity alone
  is not the cause. A manual unprivileged foreground daemon could not bind the
  system socket or create its pid file, so that is not a recovery path.
  `lsusb -t` showed `Driver=[none]` on the CCID interface, as expected for the
  libusb-backed libccid driver.
- **Result:** The reader remained absent from PC/SC and a later bounded scan
  timed out without output. No applet APDU was sent; power-cycle persistence
  remains unverified. The exact latest run is
  `artifacts/logs/pico-powerup-persistence-20261019T121536Z.log`.
- **Next:** Resume after external PC/SC/device state changes. Avoid launching a
  second `pcscd` manually or using unbounded `pcsc_scan`; the service socket is
  already owned by systemd and the scanner has saturated VM CPUs in past runs.

## JC305 AID virtual-method token mismatch — 2026-10-19

- **Symptom:** `JCSystem.lookupAID()` reached `6F00` with a VM-raised
  `StackUnderflowException` at `invokevirtual`. The JC305 `AID` export assigns
  token 3 to `getBytes([BS)B`, while the embedded ROM-mask API assigns token 3
  to `partialEquals([BSB)Z`. The VM therefore read a four-word argument count
  for a three-word call.
- **Viable solution:** Map the six JC305 `javacard.framework.AID` virtual
  method tokens to the embedded AID method order as `{0, 4, 2, 1, 3, 5}`.
  Keep this mapping in `remap_imported_virtual_method_token()` alongside the
  other JC305-to-ROM API mappings.
- **Validation:** `./tools/build arm` passed. After refreshing the host
  emulator with `./tools/build-pc`, VPCD cases 65–66 passed (`TEST:SUMMARY
  pass=4 fail=0 elapsed_s=24.373`), covering a matching active AID, a miss,
  null, negative offset/length, and out-of-range input. The first VPCD attempt
  used a stale host emulator and still failed; rebuild it before validating a
  VM code change. The VPCD runner returned `Operation not permitted` in the
  restricted shell; rerunning that local test in host execution context passed.
- **Scope:** The Java API currently searches only the active applet AID because
  the VM models one applet context. Multi-applet registry lookup remains open.

## Unknown AID SELECT dispatched to active applet — 2026-10-19

- **Symptom:** With an applet selected, a well-formed SELECT-by-AID for an
  uninstalled AID could fall through to `Applet.process()` in both CCID and
  VPCD session handling. VPCD could therefore preserve a selected applet after
  a failed app selection, and CCID could misinterpret the SELECT as an applet
  command.
- **Viable solution:** Recognize valid short SELECT-by-AID APDUs before normal
  applet dispatch. On an unknown AID, deselect the active applet and return
  `6A82`; only the configured applet AID enters restore/select/process.
- **Validation:** VPCD session unit test passed 16 checks. Focused applet VPCD
  test passed 3 checks and verified the unknown SELECT returned `6A82`, then a
  regular applet APDU returned `6999`. PC release and ARM M33 builds passed.
- **Scope:** Covers short case-3 and case-4 SELECT-by-AID APDUs with AID length
  5–16 and any optional short `Le`. Extended-length APDUs are not modeled.

## Pico PC/SC lists the card but Java APDU transmit stalls — 2026-10-19

- **Symptom:** After flashing the CHOUPI image, the authorized USB ACL helper
  succeeded and bounded `pcsc_scan -r` listed the physical Pico. GlobalPlatformPro
  also listed the installed JCAlgTest package and selectable applet. The Java
  SmartcardIO probe connected and read ATR `3B800181`, then timed out while
  transmitting SELECT; no APDU response was received.
- **Firmware evidence:** A bounded serial capture immediately after the probe
  repeatedly reported `stage=2`, `rx_events=0`, `tasks=0`, and `xfr=0`, alongside
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The command did
  not reach the firmware CCID XfrBlock handler. This points to a host PC/SC or
  CCID transport issue before applet dispatch; it does not establish an applet
  execution failure.
- **Viable recovery:** The ACL helper and `pcsc_scan -r` restore discovery, but
  they do not fix this transmit stall. Stop after one bounded probe and inspect
  firmware CCID counters; do not count this as power-cycle persistence success.
  A working consecutive APDU path is still required before physical persistence
  validation can continue.
- **Evidence:** `artifacts/logs/pico-powerup-persistence-20261019T191503Z.log`
  (SHA-256 `14ecbe9209f62be8fd7b3f3327ef553e54d2d1fb52202bf808ad57086cfb5ed4`)
  and `artifacts/logs/monitor-apdu-probe-after-timeout-20261019.log` (SHA-256
  `3dc13d33a620ae7cc5dd1ccfa38029139d678e2682fbff2de4c0516ea2a8dad8`).


## Physical power-up check confirms registry but APDU transmit is not delivered — 2026-10-20

- **Symptom:** After a successful CHOUPI ARM flash, GlobalPlatformPro listed
  the installed JCAlgTest 1.8.2 package and selectable applet. The physical
  SmartcardIO probe listed both virtual readers and the Pico, read ATR
  `3B800181`, then failed SELECT with `SCARD_E_NOT_TRANSACTED`. On the next
  retry, the authorized USB ACL helper found no Pico device (`found 0`).
- **Firmware evidence:** The 18-second cycle capture passed
  `TEST:SUMMARY pass=1 fail=0`; after the failed transmit, a bounded 3-second
  serial capture again passed runtime self-tests and reported
  `rx_events=0 tasks=0 xfr=0`, so no physical APDU reached CCID firmware.
- **Result:** Package/applet registry visibility survived the flash/runtime
  cycle, but physical power-up persistence and applet execution are not
  validated because the host transmit did not reach the card.
- **Next:** Restore Pico visibility with the authorized ACL helper and verify
  PC/SC discovery, then retry one bounded APDU only when the physical reader is
  present. Inspect CCID counters before interpreting applet behavior.
- **Artifacts:** `artifacts/logs/cycle-choupi-20261020T041200Z.log` and
  `artifacts/logs/pico-powerup-persistence-20261020T041334Z.log`.
- **Repeat check:** After another BOOTSEL flash on 2026-10-20, the same
  `SCARD_E_NOT_TRANSACTED` occurred and the retry reported zero Pico devices.
  `lsusb -d 2e8a:0009` still found the board; the ACL helper restored its entry
  in `pcsc_scan -r`. A subsequent 3-second monitor again showed
  `rx_events=0 tasks=0 xfr=0`. USB descriptors show the CCID interface and bulk
  endpoints 0x03/0x83 at 64 bytes. `journalctl -u pcscd` exposed no entries to
  the session user, so host daemon transport diagnostics remain unavailable.
- **Repeat artifacts:** `artifacts/logs/cycle-choupi-20261020T041650Z.log` and
  `artifacts/logs/pico-powerup-persistence-20261020T041802Z.log`.
- **Correction from monitored command-history probe:** The zero counters above
  were captured before a later SELECT was sent. With diagnostic firmware,
  concurrent serial capture observed repeated `GetSlotStatus` (`0x65`) messages,
  then `IccPowerOn` (`0x62`) and SELECT XfrBlock. Firmware logged the JCAlgTest
  AID, restored class `0x0094`, and completed applet selection with `9000`.
  The Java probe still timed out before printing the SELECT response or
  GETVERSION. No unknown CCID commands or endpoint errors were reported. After
  the probe, PC/SC listed only virtual readers; the current bounded scan timed
  out with no output while USB still enumerated `2e8a:0009`.
- **Interpretation:** SELECT does reach and execute in firmware. The unresolved
  failure is at or after the CCID response path, and applet persistence cannot
  be concluded without a host-visible response and GETVERSION. The latest
  select/version probe and serial trace were inspected live, not archived;
  `artifacts/logs/cycle-choupi-20261020T055535Z.log` records the diagnostic
  firmware build/flash/runtime cycle.


## Transient array allocation failure terminated the VM — 2026-10-19

- **Symptom/cause:** `JCVMArray(uint16_t)` was `noexcept` while allocating its
  backing `new[]`. A failed transient array allocation therefore called
  `std::terminate` before `JCSystem.makeTransient*Array()` could map the error
  to its specified `SystemException.NO_TRANSIENT_SPACE`. The native transient
  factories also lacked an allocation-failure mapping.
- **Viable fix:** Allow the backing-array constructor to propagate
  `std::bad_alloc`; catch it in primitive and reference transient factories, set
  the pending system-exception reason to `NO_TRANSIENT_SPACE` (2), and throw the
  VM's `SystemException`. Invalid clear events set `ILLEGAL_VALUE` (1).
- **Validation:** ARM build passed. VPCD applet cases 28 and 29 passed (transient
  array behavior and invalid event reason). `./tools/test-jcvmarray-allocation`
  deterministically failed backing `new[]` and passed
  `allocation_failure_propagates`. The native factory's Java exception mapping
  is covered by inspection, not by forced allocator failure through the full VM;
  real-board low-memory APDU validation was not run.


## Persistent array restoration allocation used `noexcept` constructor — 2026-10-19

- **Symptom/cause:** The persistent `JC_Array` constructor allocates the tagged
  backing buffer with `new[]` in its initializer but was declared `noexcept`. A
  low-memory restore could terminate the firmware before `Context::restore` or
  the APDU boundary could handle the failure.
- **Viable fix:** Remove `noexcept` from both declaration and definition so the
  allocation failure propagates into existing restore/APDU catch boundaries.
- **Validation:** ARM build passed. `./tools/test-jcvmarray-allocation` injects
  `std::bad_alloc` into this persistent constructor's `new[]` initializer and
  passes `persistent_array_failure_propagates`, in addition to the direct
  `JCVMArray` allocation-failure case. Full firmware restore/APDU behavior under
  forced exhaustion remains untested.


## Persistent primitive reads leaked temporary buffers — 2026-10-19

- **Symptom/cause:** FlashMemory_Handler::getDataFromTag() allocated a raw
  buffer for every persistent field read. The byte, short, and int getters never
  freed it, including when validation or fs_read() failed. Repeated persistent
  field access therefore consumed the runtime heap.
- **Viable fix:** Return std::unique_ptr<uint8_t[]> from getDataFromTag() and
  pass .get() to fs_read(). Structured-binding callers retain ownership
  through validation and automatically release the data on every exit path.
- **Validation:** The persistent transaction test now counts array allocations
  and deletions. A valid byte read and an injected fs_read() failure each
  allocate and free exactly one temporary buffer. The test and ARM build pass.
  The two-process VPCD persistence test also passed install/commit 3/3 and fresh
  restore 7/7, including persistent array aliases and the committed applet field.


## Persistent JC_Instance construction could leak its temporary tag buffer — 2026-10-19

- **Symptom/cause:** The persistent JC_Instance constructor allocated an
  overflow buffer into a raw member before resolving class field metadata and
  allocating its fields array. If either later step threw, the incomplete object
  destructor did not run, leaking the overflow buffer.
- **Viable fix:** Hold the overflow tag and field array in local unique_ptrs.
  Transfer them into the object only after all potentially throwing setup
  succeeds.
- **Validation:** ARM build passed. The two-process VPCD persistence regression
  passed install/commit 3/3 and fresh restore 7/7. The constructor failure path
  was not directly allocation-injected.


## JCVMArray copy assignment lost its destination on allocation failure — 2026-10-19

- **Symptom/cause:** Mutable JCVMArray copy assignment released its current
  buffer before allocating and copying the replacement. If allocation failed,
  the assignment threw after setting the length but left the destination without
  backing storage.
- **Viable fix:** Build the replacement in a local unique_ptr, copy all elements,
  then release the old buffer and commit the new pointer/length. The copy
  constructor also stages its allocation so an element-copy exception cannot
  leak a partial buffer.
- **Validation:** The allocation test injects failure during assignment and
  verifies that the destination retains its old length and values. ARM build and
  the focused allocation regression pass.


## Physical APDU reaches applet, then stalls during post-APDU GC — 2026-10-20

- **Symptom:** PC/SC lists the Pico CCID reader and reads its ATR. A focused
  JCAlgTest SELECT reaches the firmware; applet lookup/restore/select and
  `Applet.process()` all return `9000`, but SmartcardIO reports
  `SCARD_E_NOT_TRANSACTED` before receiving a response.
- **Observed location:** Serial prints `CHOUPI:APDU_PROCESS stage=gc_begin`.
  Instrumentation found 7 array slots, 6 instance slots, and 13 pending roots;
  all 7 arrays and 6 instances scan in the first pass. A second pass starts,
  then the monitored capture ends before GC or CCID response completion.
- **Performance change attempted:** GC now iterates the stable heap lists
  alongside mark bytes rather than resolving each scanned slot with
  `std::list::at()`. Per-edge liveness is taken from empty-slot sentinels, and
  persistent array length is read once per scan instead of on each loop test.
  ARM/RISC-V builds and runtime cycle pass, but the physical probe still fails.
  This performance hypothesis is not confirmed as the root cause.
- **Next diagnosis:** Add per-instance field and per-reference-array progress
  to locate the second-pass stall. Check persistent array storage reads and
  traversal loop bounds. Do not classify the PC/SC failure as host-only: the
  firmware has not returned from collection to send the APDU response.
- **Artifacts:** `artifacts/logs/cycle-choupi-20261020T061302Z.log`,
  `061623Z.log`, `061949Z.log`, and `062341Z.log`. The concurrent APDU serial
  capture was inspected live but not saved.

- **Viable fix confirmed on hardware:** Persistent reference-array traversal can
  lazily restore objects, extending the heap slot tables after the mark vectors
  were initialized. Grow the matching mark vector before indexing a newly
  discovered handle and snapshot slot limits per pass so additions are visited
  on a subsequent pass. Refresh a sentinel slot if lazy restoration reuses a
  previously empty slot.
- **Validation:** The Pico SELECT, GETVERSION (`1.8.2_jc305`), and JCSystem info
  probe passed; serial showed heap growth from 7/6 to 27/17 array/instance
  slots, followed by GC completion and response submission. The bounded
  `tools/test-pico-powerup` passed registry plus SELECT/version/JCSystem checks.
  `SCARD_E_NOT_TRANSACTED` did not recur in these focused checks.
- **Follow-up:** Run official `ALG_PERFORMANCE_STATIC` to ensure repeated GC
  remains correct as the applet creates more transient objects.


## RP2350 BOOTSEL reinsertion detection — 2026-10-20

- **Observation:** After the reported reinsert in BOOTSEL, the host enumerated
  `2e8a:000f` (`Raspberry Pi RP2350 Boot`), `picotool info -a` returned
  `boot type: bootsel`, and Linux exposed `/dev/sdb` with serial
  `D92E743D5F640C0F` and DISKSEQ 106. The `RP2350` FAT volume was auto-mounted
  at `/run/media/claude/RP2350`; `INFO_UF2.TXT` identified UF2 Bootloader v1.1.
- **Correction:** RP2350 BOOTSEL reports VID/PID `2e8a:000f` in this setup, not
  `2e8a:0003`.
- **Limit:** This confirms recognition after the user's reported reinsertion,
  but no remove/add event monitor was running during the physical transition;
  no latency or repeated-cycle reliability measurement was made. PC/SC is not
  available in BOOTSEL mass-storage mode.
- **Evidence:** Read-only `lsusb`, `udevadm info`, `lsblk`, `findmnt`,
  `picotool info -a`, and BOOTSEL volume metadata query on 2026-10-20.

## Runtime CAP loader accepts only the configured JCAlgTest package AID — 2026-10-20

- **Symptom:** GPPro `--install` of a separately AID-scoped JCSystem probe
  failed at `INSTALL [for load]` with `6D00`; it did not proceed to LOAD.
- **Cause:** `CapLoadSession::beginInstallForLoad()` and
  `validate_cap_code()` compare the requested and embedded package AIDs against
  the firmware's configured `kPackageAid`. This firmware only supports the
  supplied JCAlgTest package for runtime CAP loading.
- **Effect:** The failure occurs before CAP blocks are sent, so the rejected
  attempt did not replace or modify the installed JCAlgTest package. A fresh
  `tools/test-pico-powerup` confirmed its registry, SELECT, GETVERSION, and
  JCSystem responses afterward.
- **Handling:** Do not use arbitrary package AIDs for temporary test CAPs on
  this firmware. A second applet test would require replacing the configured
  package under its supported AID and restoring the official CAP afterward;
  that replacement was not attempted. For AC5, first run a true power-off/on
  against the unchanged JCAlgTest CAP and check registry plus reproducible
  client behavior.
- **Evidence:** `artifacts/logs/gppro-install-jcsystem-persistence-probe-20261020.log`
  and `artifacts/logs/pico-powerup-persistence-20261020T131336Z.log`.

- **Reproduction with the deploy API suite:** The supplied `AlgTestApiCore`
  CAPs for API versions 305, 304, 303, 222, and 221 all failed at GPPro
  `INSTALL [for load]` with `6D00`. No CAP LOAD blocks were sent. GPPro
  `--list` afterward still showed only the ISD and the original JCAlgTest
  package/applet. This prevents the README's API-version detection and all
  subsequent API coverage areas on the current firmware.
- **Resolution required:** Generalize runtime CAP package acceptance and the
  associated storage/registry path so a deployment API test CAP can be loaded,
  selected, and removed without damaging the existing package. The README
  runner cannot work around this in its documented flow; it expects ordinary
  CAP package AIDs. Until that implementation exists, do not interpret this
  pre-LOAD `6D00` as a Java Card API test result.
- **Evidence:** `artifacts/logs/api-coverage-pico2-core-auto-20261020.log`,
  `artifacts/results/api-coverage-pico2-core-auto-20261020.csv`, and
  `artifacts/logs/api-coverage-gppro-registry-after-20261020.log`.
- **Resolution implemented (validation still pending):** Runtime CAP loading
  now uses eight package-indexed slots (IDs 56–63), accepts the embedded CAP
  package AID, and keeps each package's applet and instance AIDs plus class
  indexes. VM lookup and GET STATUS enumerate these dynamic slots; install can
  target an already loaded package; SELECT and DELETE restore the requested
  package/instance first. Package DELETE clears persistent state for every
  recorded applet instance. ARM/PC builds and focused registry, DELETE, and
  install-status regressions pass. Physical Core API CAP upload and multiple
  package install/select/delete have not yet been exercised end-to-end.

## Physical deploy Core lifecycle and cleanup results — 2026-10-21

- **Cause of Ext SELECT `6999`:** The Core lifecycle bridge invoked the
  JC305 `Applet.select()` token against the embedded framework table without
  translating the token at the dynamic-CAP-to-framework superclass boundary.
  `Class_Handler::doGetPublicMethodOffset()` now remaps imported public tokens
  when it follows an external superclass reference. This is separate from
  explicit external-method token remapping.
- **Viable fix / evidence:** ARM build passed and, after flashing, the physical
  Ext applet's full AID `4A43416C67546573744170657801` selected with `9000`;
  the previously failing call returned `6999`. The deploy Core applet's
  inherited `Applet.select()` and `deselect()` rows also returned OK. Preserve
  this superclass-boundary remap for future framework lifecycle methods.
- **OwnerPIN residual:** The deploy Core CAP still reports two
  `getValidatedFlag()` calls as `SW=6f00` and two `setValidatedFlag()` calls as
  `EXC_RUNTIME`, while constructors, update/check, validation status, and PIN
  interface methods pass. A narrowly added high-bit adjustment was tested on
  hardware and did not change those results; it was removed. The failing
  dispatch did not appear among the filtered OwnerPIN class-token traces, so
  capture the full external framework method-ref trace before changing token
  maps again.
- **Persistent applet deletion (historical issue, now fixed):** Earlier GPPro
  DELETE calls for Core, Security, Crypto, and Service returned `6581` after
  API tests. Current ARM firmware cleans up populated Core/Multi, Crypto, and
  Service packages successfully through the official deploy runner; see the
  2026-10-21 three-reader result in `TODO.md` and
  `artifacts/results/deploy/parallel-20261021-211910/`.
- **USB recovery:** After CDC BOOTSEL flash recovery, the Pico sometimes takes
  several seconds to return. `lsusb` initially showed no `2e8a:` device and
  the PC/SC helper reported zero; subsequent `lsusb` discovery followed by
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` restored access. On the latest
  attempt the VM did not enumerate the board at all, so stop physical calls
  there and continue offline until the device appears. Current clean ARM build
  is `build/arm/pico2_choupi.uf2`; it has not been flashed after temporary
  diagnostics were removed.

## DELETE cleanup rejects CAP class indexes above 255 — 2026-10-21

- **Cause found in source:** `clear_applet_persistent_state()` rejected a
  recorded 16-bit CAP class index above `UINT8_MAX` and returned failure,
  which GlobalPlatform DELETE maps to `6581`. The CHOUPI `path_applet_field`
  ABI encodes its class component as one byte, so persisted applet-field keys
  use the low byte of that class index. Cleanup now uses the same byte
  conversion as the runtime instead of rejecting the instance.
- **Validation:** ARM build passed. The persistent-transaction host regression
  covers erasing a field generated from class index `0x1234`; the GP DELETE
  protocol regression passes 26/26. Physical confirmation remains pending:
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` found no reader and gated
  `lsusb -d 2e8a:` showed no connected Pico.
- **Caveat:** This unblocks cleanup against the current one-byte field-tag
  format. The broader 16-bit class-index encoding and potential same-package
  key collisions still need design and compatibility coverage before claiming
  arbitrary class-index persistence is complete.
- **Next:** Reflash the clean ARM image when USB returns, retry DELETE for a
  populated Core CAP, confirm registry removal and reinstall, then continue the
  deploy README stages.

## 16-bit applet class indexes need distinct persistence tags — 2026-10-21

- **Cause:** The old `path_applet_field` ABI encoded only the low byte of the
  16-bit VM class index. Distinct classes such as 1 and 257 could therefore
  share persistent keys within one package. Removing the DELETE range check
  alone would have kept this collision.
- **Fix:** Class indexes through 255 retain the five-byte legacy root. Higher
  indexes use a six-byte root containing the full class index. Filesystem and
  `JC_Instance` tag compression recognize both forms. High-index reads fall
  back to the legacy key for migration, while DELETE removes both forms.
- **Validation:** `./tools/build-pc`, `./tools/build arm`,
  `./tools/test-persistent-transaction`, `./tools/test-rp2350-fs-overlay`,
  and `./tools/test-gp-delete` passed. The overlay regression proves class 1
  and class 257 tags remain distinct and tests a compressed child tag. No
  hardware test was possible: the PC/SC helper and gated `lsusb` found no Pico.
- **Physical retest:** After both architecture builds passed, all three Picos
  were flashed with the current ARM image. Core/Multi, Crypto, and Service
  cleanup all passed on hardware. JavacardxSec, Javacardx, JavacardxExt,
  JavacardxInt, and Biometry INSTALL refusals were rolled back; their imports
  are absent, so no applet instance was created.

- **Retest note:** The VM's authorized PC/SC helper currently reports no Pico.
  The host VPCD JCAlgTest SELECT succeeds but GETVERSION timed out at both 20 s
  and 60 s with the old test fixtures. Repeating with code concatenated from
  the exact provided 1.8.2 CAP and a fresh erased image still timed out at 30 s
  (VPCD connect 12.368 s, SELECT `9000`). This is unresolved host-emulator
  method execution, not physical evidence; no deploy CAP was uploaded.

## `instanceof` class resolution can fail on an invalid alternate interface token — 2026-10-21

- **Symptom:** The deploy Core `UserException.throwIt` checks returned
  `EXC_RUNTIME`. A focused VPCD probe reproduced the failure when the applet
  caught the exception as `Exception` and then evaluated
  `expected instanceof UserException`; the APDU returned `6F00`.
- **Cause:** `Bytecodes::docheck()` first resolved the JC305 imported type using
  its class-token map. It then speculatively resolved the same numeric token in
  the interface-token namespace. A byte can be a valid class token and an
  invalid interface token, so that second lookup threw `SecurityException`
  before the already valid class comparison could run.
- **Viable fix:** Catch only `SecurityException` from the speculative interface
  lookup and keep the successfully resolved class reference. Rethrow other VM
  exceptions. The class/interface alternative remains available when the
  interface lookup resolves to a real interface.
- **Evidence:** Release host VPCD case 78 passed (`catch (Exception)`,
  `instanceof UserException`, `getReason()==0x1234`). The actual
  `AlgTestApiCore_305.cap` loaded, installed, selected, and its matching
  `B0 83 17 02 01 00 00` API probe returned data `00000001` with `9000`.
  ARM build passed with image end `0x100c5c7c`. Physical confirmation remains
  pending because the Pico was not enumerated by the VM.
- **Future use:** When a class reference check fails unexpectedly on an imported
  JC305 API, inspect both class and interface token interpretations. Never let
  failure to resolve the alternate namespace invalidate the valid primary
  interpretation. Apply the same principle anywhere token namespaces overlap.

## JC305 Security Key interface method order mismatch — 2026-10-21

- **Symptom:** The deploy Security CAP's DES `KeyBuilder.buildKey` case returned
  `6F00` with `StackUnderflowException` (`opcode=8D`, operand depth 2) at the
  final result call. The trace showed `Key.getSize()` resolving to ROM
  `KeyImpl.clearKey()`, leaving a missing result argument.
- **Cause:** In the JC305 Security export, `Key` is class token 0 and interface
  method tokens are `clearKey=0`, `getSize=1`, `getType=2`,
  `isInitialized=3`. The embedded Java Card API orders these methods
  `isInitialized`, `clearKey`, `getType`, `getSize`. Treating imported tokens
  as embedded tokens therefore misdispatches `getSize` and
  `isInitialized`.
- **Viable fix:** In `remap_imported_interface_method_token`, map JC305
  Security `Key` class token 0 with `{1, 3, 2, 0}`. Scope by package AID and
  interface class token; the separate DESKey interface mapping remains
  independent.
- **Validation:** `./tools/build arm` and normal `./tools/build-pc` passed.
  The actual `AlgTestApiSecurity_305.cap` was installed in the host VPCD VM;
  APDU `B0 83 60 01 01 00 00` returned `00 00 40 01 90 00` (key size 64,
  uninitialized flag). A six-row exploratory APDU pass also got `9000` for
  every command, but responses `FC`/other payloads were not classified and
  should not be counted as supported results. No physical flash/test was run.
- **Follow-up validation (2026-10-21):** Correctly encoded deploy CAP row-index
  APDUs for persistent DES rows 0–2 and AES rows 9–11 returned respectively
  `00004001`, `00008001`, `0000C001`, `00008001`, `0000C001`, and `00010001`,
  each followed by `9000`. The values match the requested key sizes and the
  success flag. This resolves the earlier unclassified six-row exploratory
  result; the previous physical `SW_ERROR` rows predate this source fix. Log:
  `artifacts/logs/deploy-security-keybuilder-persistent-rows-vpcd-20261021.log`.
  Hardware confirmation remains pending.
- **Future use:** When API helper execution underflows after an interface call,
  compare both the imported interface method order and the embedded API order;
  class-token remaps do not translate interface method tokens.

## Official API client reader index includes empty virtual terminals — 2026-10-21

- **Symptom:** The physical Security API CAP uploaded successfully, but the
  client produced no call rows and waited until interrupted when launched with
  `--reader-index 1`.
- **Cause:** The Java client indexes all PC/SC terminals, including the two
  empty `Virtual PCD` readers. The attached Pico was the third terminal even
  though it was the only reader with a card.
- **Viable solution:** Pin GlobalPlatformPro to the full physical reader name
  with `--gp-args '-d -r "<Pico reader name>"'` and pass `--reader-index 3` to
  the official Java API client when both virtual terminals precede the Pico.
  Confirm terminal ordering with `pcsc_scan -r` before each run.
- **Validation:** The rerun against the named physical Pico completed all 901
  Security calls and removed `AlgTestApiSecurity_305.cap`; upload, test, and
  cleanup stages all reported `OK`. CSV:
  `artifacts/results/deploy/pico2-single-security-reader3-20261021.csv`.
- **Future use:** Do not infer the client index from “number of readers with a
  card”; the client may show terminals with no card. Explicitly select the
  physical name for GPPro and the matching Java terminal index.

## JavaCardX CAP static slots exceed the legacy tag width — 2026-10-21

- **Symptom:** Loading `AlgTestApiJavacardx_305.cap` reached INSTALL but
  returned `6A80` before applet initialization; the VM reported install stage
  5 with no Java exception. The CAP's StaticField component has an image size
  of 328 bytes, 164 reference slots, and 163 array initializers (159 byte and
  four short arrays).
- **Cause:** Static initializer index 128 maps to byte offset 256, but the
  static-field filename API accepted only an 8-bit offset and rejected later
  slots. Existing filesystem authorization already supports a two-byte static
  root key.
- **Viable fix:** Keep the historical three-byte root tag for offsets through
  255; use a four-byte root with a big-endian 16-bit offset beyond 255. Remove
  the installer rejection while retaining image bounds checks.
- **Validation:** Current-source host and ARM builds pass. The JavaCardX CAP
  installs and SELECTs through VPCD with `TEST:SUMMARY pass=11 fail=0`.
  Physical deployment remains pending.
- **Future use:** Inspect a CAP's static component size and initializer count;
  do not assume static offsets fit in one byte.

## BOOTSEL devices are not PC/SC readers — 2026-10-21

- **Observation:** USB enumeration found one application-mode Pico
  (`2e8a:0009`) and two RP2350 BOOTSEL devices (`2e8a:000f`). PC/SC lists only
  the application-mode device, alongside two empty Virtual PCD terminals.
- **Viable workflow:** `tools/run-deploy-parallel` discovers physical Pico
  reader names through Java SmartcardIO, computes each official client's
  one-based terminal index, pins GPPro to the same exact name, and writes
  separate result/log files. It requires three live PC/SC Pico readers by
  default, so it refuses to start while devices remain in BOOTSEL.
- **Validation:** The dry run mapped API areas to the one live Pico; a
  non-dry invocation stopped at “Found 1 ... need 3” before launching a card
  worker. USB and PC/SC queries used the elevated host access path.
- **Future use:** After all three Picos boot CHOUPI and appear in `pcsc_scan
  -r`, invoke `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305`
  with elevated host access. Do not count `2e8a:000f` devices as test readers.

## Failed scenario INSTALL can leave a partially loaded CAP — 2026-10-21

- **Symptom:** The Core deploy runner's optional Ext CAP failed INSTALL with
  `6A88`, but the package remained loaded. The shared ordinary-area upload path
  already rolled this back; `upload_scenarios()` had a separate install path
  and omitted the rollback.
- **Cause:** GPPro performs LOAD before INSTALL. If INSTALL refuses, the CAP
  package is still resident even though the applet instance was not created.
  Skipping the failed CAP during normal cleanup leaves a stale AID that can
  block later uploads.
- **Viable fix:** Use the same partial-install rollback helper for ordinary
  area CAPs and Core scenario CAPs. Emit a separate rollback stage so a failed
  cleanup remains visible.
- **Validation:** Removed the leftover `AlgTestApiExt_305.cap` from Pico 3 with
  GPPro. After the runner fix, a physical Core rerun again received `6A88`,
  recorded `UPLOAD_EXT_ROLLBACK OK`, completed its 291-call test, and a followup
  GPPro registry check showed only the ISD. No firmware change was involved.
- **Future use:** Every code path that invokes `gp.jar --install` must account
  for `CAP loaded` followed by INSTALL refusal; do not assume a failed install
  leaves no package state.

## Core deploy repeat reports unexpected `Applet.deselect()` value — 2026-10-21

- **Symptom:** Multiple Core 3.0.5 API runs on Pico 3 completed all 291 calls
  but sometimes reported one `WRONG_RESULT` for `Applet.deselect()`. A new
  three-reader run showed `select()` counter 239 and `deselect()` counter 98.
- **Cause detail:** Decompiling `algtestapi.JCAlgTestApiApplet` from the client
  jar shows the test does not inspect a VM return value for `void deselect()`.
  It increments `m_flags[1]` in the lifecycle callback, then returns that
  counter as a byte and checks `m_flags[1] == m_flags[0] - 1`, where slot 0 is
  incremented in `select()`. Thus the varying value is a callback-count
  mismatch, not interpreter root-return corruption. `m_flags` is a
  `CLEAR_ON_RESET` transient short array.
- **Viable fix (reset semantics, physically validated):** CCID `IccPowerOn` previously restored
  installed metadata but did not clear the selected context or
  `CLEAR_ON_RESET` arrays. It now invokes `choupi_vm_clear_selected_applet`,
  resets secure-channel state, and aborts incomplete CAP transfer state before
  restoring package metadata. USB bus reset also invokes the VM reset helper.
  ARM and RISC-V builds, VPCD integration, and the physical test pass. After
  flashing Pico 3 and refreshing its PC/SC ACL, two clean Core upload/test/
  cleanup runs each completed 291 calls and reported `select() returned=-19`
  and `deselect() returned=-19` with no `WRONG_RESULT`. This confirms the CCID
  reset-state omission caused the observed callback counter divergence.
- **Evidence:** `artifacts/results/deploy/pico3-core-resetfix-20261021.csv`
  (SHA-256 `e536e17f8785f79d327af4cfcb890d2de0fab1bbd34a1b482642e6943f7b060e`)
  and `artifacts/results/deploy/pico3-core-resetfix-repeat-20261021.csv`
  (SHA-256 `d6f1e16a7e7442a39358c19833a35334d9f4ae8fe61fe3df81f4115ffc337b1f`).

## Third Pico is USB-visible but intermittently absent from PC/SC — 2026-10-21

- **Symptom:** After the full three-reader run, `lsusb -d 2e8a:` still listed
  serials `D92E743D5F640C0F`, `E0E14525E64AF6ED`, and `5CFC5328F5BDE437`, but
  `pcsc_scan -r` and GPPro listed only the first two physical readers.
- **Checks:** The approved helper granted ACLs to all three nodes and restarted
  `pcscd.service`; the daemon was active. Pico 3's USB descriptor at bus 001,
  device 044 still advertised three interfaces, including CCID interface 2
  with endpoints `0x03`/`0x83`, and serial `5CFC5328F5BDE437`. Repeated scans
  still saw only two readers. Journal access exposed no pcscd entries.
- **Current use:** The full parallel run completed successfully before this
  visibility loss. Read-only persistence checks on Pico 3 could not confirm a
  package because that card had no JCAlgTest fixture installed; do not infer
  firmware data loss from the ISD-only listing. The JCAlgTest 1.8.2 CAP was
  installed on Pico 1 instead, and its bounded power-up registry/SELECT/
  GETVERSION/JCSystem probe passed.
- **Next:** Before physical tests assigned to Pico 3, confirm the exact reader
  has returned in `pcsc_scan -r`; USB presence and ACL grant alone are not
  sufficient. If it remains absent, record the `pcscd` driver error or replug
  the reader rather than retrying applet APDUs through a missing terminal.
# Stale dynamic package bitmap can hide a missing CAP — 2026-10-21

- **Symptom:** On a card carrying older install state, an optional CAP import
  could return INSTALL stage 9/`6F00`; the same CAP on a clean card correctly
  returned stage 10/`6A88`.
- **Cause:** `isPackageExist()` trusted the package bitmap for dynamic IDs
  56–63. Interrupted cleanup or older single-package storage can leave a bit
  set after the corresponding CAP slot is empty. Import resolution then tries
  to parse that empty slot and reports an internal exception instead of
  “referenced data not found.” This cause is inferred from the contrasting
  reader results and the lookup path.
- **Viable fix:** For dynamic IDs, require both the bitmap bit and a non-null,
  nonempty `choupi_cap_store_data_for_package()` result. The bitmap remains the
  enable/disable gate. Regression coverage checks a stale bit, a valid stored
  CAP, and a cleared bit. `./tools/test-persistent-transaction`,
  `./tools/test-install-status`, ARM build, host build, and host VPCD applet
  tests pass. Missing Ext dependency still returns stage 10/`6A88` on VPCD.
- **Hardware status:** Not flashed or physically verified because current
  repository instructions require both architecture builds before flashing,
  while the active user constraint is ARM-only. Retest on hardware after that
  gate is reconciled; do not claim the source fix is physically verified.
- **Repeat physical symptom:** On 2026-10-21, JavacardxExt INSTALL returned
  `6F00` twice on reader D92 after successful cleanup; the same area on the
  other two readers returned expected `6A88`. The runner rolled back both
  D92 partial uploads. This is consistent with stale dynamic package bitmap
  state on the already-flashed firmware, but is an inference: the source guard
  had not been flashed and the package registry was not directly inspected.

## CAP metadata update could destroy the only valid registry header — 2026-10-21

- **Symptom:** `write_metadata()` erased the package's only metadata sector
  before writing its replacement. A cut after erase could make an intact CAP
  appear absent, or lose its installed-applet registry.
- **Viable fix:** Keep the original slot header as the primary copy and add one
  mirrored header sector per dynamic CAP slot in the unused `0x1b0000–0x1b8000`
  flash gap. Version 4 headers carry a wrapping generation counter; metadata
  writes target the inactive copy, and boot chooses the newest valid CRC'd
  header. Version 3 primary headers remain readable and are upgraded on their
  next metadata mutation.
- **Validation:** `./tools/test-rp2350-fs-overlay` passes, including simulated
  cuts after erasing either alternating destination. Recovery retains the
  previous registry and CAP bytes. `./tools/build arm` and `./tools/build-pc`
  pass. The overlay-only glibc build now uses `mallinfo2()` where available,
  removing its deprecation warning; the OS Rust build still reports existing
  warnings. No RISC-V build or physical flash was run.
- **Remaining issue:** The CAP instance header and persistent filesystem
  snapshot are separate commit domains. A cut between publishing instance
  metadata and the final filesystem snapshot can still produce inconsistent
  installation state. A durable cross-store journal and full CAP-install
  fault injection are still required; this fix only makes each metadata
  update itself copy-on-write.

## CAP applet registration and filesystem install commit need reconciliation — 2026-10-21

- **Cause:** CAP applet instances live in the separate per-slot metadata
  header, while applet roots/fields live in the filesystem snapshot. A power
  cut between the two independent commits could leave only one side durable.
- **Current fix:** Install writes a CRC-protected intent record containing
  package, applet, and instance AIDs plus class index in the filesystem
  transaction with applet state. Only after that transaction commits does it
  publish the COW CAP instance record. It then removes the intent in a second
  transaction. On card power-on, `restoreInstalled()` validates and replays
  unfinished intents; replay detects a matching existing instance to avoid
  duplicate registration. Opaque record helpers participate in normal
  persistent-write snapshots.
- **Validation:** Persistent-transaction tests cover opaque record commit,
  rollback, failed sync, and cleanup. A fresh VPCD JCAlgTest 1.8.2 install
  reached `APPLET_INSTALLED`; the full current VPCD path has 15 checks. ARM and host builds
  pass. No physical flash was done.
- **Restart validation:** `--verify-install-process-restart` on the host VPCD
  runner skips registry publication once, leaves the committed journal, skips
  the first restore callback, closes the socket, and starts a fresh emulator
  process from the saved image. The new process reports
  `VPCD:INSTALL_JOURNAL_RECOVERED package=57`; SELECT and all 15 applet checks
  pass. The current runner reports 17 total checks including two pre-restart
  checks. Earlier logs printed a stale `pass=11` summary. The test discovers
  the actual allocated package ID from the install log because LOAD may choose
  a different dynamic slot than requested.
- **Cleanup-boundary validation:** `--verify-install-after-publication-restart`
  leaves the journal after successful registry publication, exits, then
  restarts from the saved image. Recovery accepts the matching existing
  registry entry, clears the journal, and passes SELECT plus all 15 checks.
- **Metadata-error validation:** `--verify-install-metadata-error-restart`
  makes the CAP instance store return an error after filesystem commit. The
  applet INSTALL reports stage 8 / `6581`; the saved image retains the journal.
  A fresh process replays it, SELECT returns `9000`, and all 15 checks pass.
  The emulator process itself exits 0 despite the internal INSTALL
  failure, so the regression asserts the explicit diagnostic marker and status
  word rather than inferring success from process status.
- **Remaining validation:** Add simulated reset/power cuts after filesystem
  commit, during registry publication, and during journal cleanup before
  calling install atomicity complete.

## VPCD test process must exit cleanly to save its image — 2026-10-21

- **Symptom:** A two-process restart test installed with a pending journal, but
  recovery in the next emulator process found no journal and SELECT returned
  `6F00`.
- **Cause:** The test harness closed its VPCD socket and immediately sent
  `SIGINT`. That could interrupt `starting_jcre()` before it finished saving
  the flash image. This was a test harness shutdown race, not evidence of lost
  card state during normal reset.
- **Viable fix:** After closing the VPCD client socket, wait up to five seconds
  for the emulator to leave its transport loop and exit naturally; only then
  fall back to the existing signal shutdown. The process-restart fault test
  subsequently logged journal recovery and passed SELECT plus the full
  15-check applet regression (older runner output called this 11).

## VPCD applet harness summary count was hard-coded — 2026-10-21

- **Symptom:** `tools/tests/pc_vpcd_applet_test.py` always printed
  `TEST:SUMMARY pass=11`, including `--select-only` and after adding more APDU
  assertions to the full regression.
- **Cause:** The summary used a historical constant instead of counting the
  selected test path. The full JCAlgTest path performs 15 checks; select-only
  performs 2 (ATR and SELECT); custom APDUs perform `2 + N`. Skipped-registry
  process restart adds two pre-restart checks; metadata-error restart does not.
- **Viable fix:** Derive the summary from mode and custom APDU count. Verified
  outputs: Security select-only `pass=2`, full JCAlgTest `pass=15`, and
  skipped-registry process restart `pass=17`. Older logs retain their printed
  `pass=11`; individual `TEST:PASS` lines are the evidence for those runs.

## Parallel deploy startup can race Java reader enumeration — 2026-10-21

- **Symptom:** The coordinator enumerated three Pico readers, but the third
  worker's second Java SmartcardIO lookup exited before producing its result
  CSV. Repeating the exact reader-name lookup after the other workers ended
  succeeded and returned index 5.
- **Cause:** Concurrent worker startup repeated the terminal enumeration
  independently. A transient SmartcardIO/PCSC enumeration failure is inferred;
  the worker log did not preserve the Java helper's stderr, so the exact native
  failure was not captured.
- **Viable fix:** Resolve `(index, exact reader name)` once in the coordinator.
  Pass both to the worker, skip its redundant enumeration, and still pin
  GPPro with the exact reader name. The subsequent physical three-reader
  Security/Crypto/Service run completed with exit 0 for all three workers,
  including upload, test, and cleanup stages.
- **Separate capability result:** `JavacardxSec` INSTALL returned `6A88` in the
  full sweep. Its CAP Import component lists
  `A0000000620001`, `A0000000620101`, `A000000062020901`, and
  `A0000000620205`. This does not establish a dependency on the separate
  `Security` test CAP. Check these API imports against the target profile before
  deciding whether to implement or classify the package as unavailable.


## Three-reader PC/SC access restored — 2026-10-21

- **Resolution verified:** After the user ran the updated installer/helper,
  elevated `pcsc_scan -r` listed all three Pico readers. The parallel deploy
  coordinator mapped Java SmartcardIO indices 3–5 to the exact Pico names. A
  three-worker Core/Crypto/Service run completed its tests and cleanup in about
  50 seconds. Optional Core Ext INSTALL returned `6A88`; partial-upload
  rollback passed. Results are in
  `artifacts/results/deploy/parallel-20261021-231607/`.


## API runner must reject partial output after abnormal client exit — 2026-10-21

- **Symptom:** An interrupted Java client wrote a partial API CSV, and the
  deploy runner reported `TEST OK` because it checked for CSV presence but
  ignored the child return code. A host Security comparison was stopped after
  551 calls and was therefore incomplete despite the misleading summary.
- **Fix:** `AlgTest_JavaCard/deploy/api_card_runner.py` now records
  `TEST_CLIENT_ERROR` and the nonzero return code even when a partial result
  CSV exists. It preserves those rows for diagnosis.
- **Validation:** A synthetic client returning 143 after writing one CSV row
  produced a failed TEST stage with `TEST_CLIENT_ERROR`; Python compilation
  and `git diff --check` passed.
- **Use:** Reject every interrupted/nonzero client run as incomplete; partial
  CSVs may diagnose the last APDU, but must not be reported as a passing test.
# Physical parallel deploy run — 2026-10-21

- **Observed:** The three-reader coordinator routed Java indices 3–5 to the
  exact Pico reader names. Read-only GPPro `--list` runs separately connected
  to each exact reader and showed the expected ISD; D92 had JCAlgTest while
  E0E and 5CFC had no applet packages.
- **Result:** Security completed 901 calls on D92 and cleaned up. Five
  Javacardx-family/Biometry CAPs returned INSTALL `6A88` and were all rolled
  back after their missing imported package was detected. The `Virtual PCD`
  reader text in GPPro's refusal diagnostic is not proof of misrouting; the
  exact-reader GPPro traces showed direct connections to the named physical
  Pico readers.
- **Viable practice:** For ambiguous GPPro diagnostics in parallel runs, verify
  routing with a read-only `java -jar <gp.jar> -d -r '<exact PC/SC name>'
  --list` and confirm the `SCardConnect` line names that Pico. The deploy
  runner's exact `-r` selector and captured SmartcardIO index remain required.

## API runner `--reader-name` did not set Java reader index — 2026-10-21

- **Symptom:** A run using the exact `--reader-name` uploaded the Core CAP
  successfully, but the official Java client printed the PC/SC reader list,
  waited at “Select index”, then exited with status 1. No API rows were
  produced.
- **Cause:** The runner appended `-r <reader name>` for GPPro but left
  `a.reader_index` unset before constructing the Java client's stdin.
- **Viable fix:** Resolve the exact reader name with
  `JCAlgTestReaderIndex` when parsing `--reader-name`, then use that index for
  the official client. `python3 -m py_compile` and physical Core/Crypto tests
  passed after the fix. Keep the index and exact name paired to avoid reader
  ordering assumptions.

## Canonical JC305 extension package AIDs did not match CHOUPI ROM AIDs — 2026-10-21

- **Symptom:** Deploy `AlgTestApiJavacardxSec_305` INSTALL failed `6A88`
  although the corresponding API classes exist in the embedded ROM mask.
- **Cause:** The deploy CAP imports canonical AIDs `A000000062020901`
  (`javacardx/apdu/util`) and `A0000000620205` (`javacardx/security`), while
  CHOUPI's generated local packages add a namespace component (`...0202080901`
  and `...020205`). Exact AID comparison therefore reported missing packages.
- **Viable fix:** Resolve only these two exact canonical-to-local AID aliases
  and compensate for their local static method token offset. Host VPCD now
  installs/selects the deploy CAP. The matching full API test was not run.
- **Follow-up:** `SensitiveResult` assertions/reset and initial AID, JCSystem,
  and Util result capture have since been implemented in current source. The
  slot is VM-wide: the API contract says a result survives context switches,
  including a call made inside a Shareable Interface Object. Per-Context
  storage would expose stale caller state after returning from the SIO.
- **Limit:** ARM and PC builds pass, but current-source API execution and
  same-context checks now pass via the official SDK RMI fixture: AID object,
  boolean, and short results; assertion mismatch; and reset/unassigned behavior.
  Explicit cross-context coverage and the complete 84-call API area are still
  pending. Remaining sensitive API producers are not implemented. Keep the area
  open until tested.

## Three-reader deploy batch and install rollback — 2026-10-21

- The approved PC/SC helper exposed three readers. `tools/run-deploy-parallel`
  enumerated exact Pico identities at Java SmartcardIO indices 3–5 and
  concurrently ran one API area per reader. Core/Crypto/Service finished in
  about 45 seconds. All successful CAP installs were removed; optional Ext
  refusal also rolled back. In the second batch, Javacardx/JavacardxInt refused
  with `6F00`; JavacardxExt/JavacardxSec/Biometry refused with `6A88`; each
  partial LOAD rollback passed.
- `6A88` signals unresolved imported package references; `6F00` is an untyped
  install-time failure requiring APDU/import trace before assigning a cause.
  The misleading `Virtual PCD` substring in GPPro diagnostics does not
  identify the routed card: use the enumerated exact reader name and pinned
  Java index from the coordinator output.
- Current-image API mismatches worth comparing with VPCD/current source:
  Core's one `Applet.deselect()` `WRONG_RESULT` and Service's 28
  `WRONG_RESULT` plus one each `SW_ERROR` and `EXC_RUNTIME`. Do not classify
  allowed unsupported algorithm rows as failures.
- Artifacts: `artifacts/results/deploy/parallel-20261021-234459/` and
  `parallel-20261021-234630/`; exact hashes are in `progress.md`.

## Host VPCD harness loopback bind denied in restricted execution — 2026-10-22

- **Symptom:** `./tools/test-pc-vpcd-applet` exits before emulator startup with `[Errno 1] Operation not permitted` while opening its loopback listener.
- **Cause:** The restricted command context denies the local socket bind used by the VPCD test harness. No USB/libusb operation or card APDU occurs before this failure.
- **Viable solution:** Re-run the host-only harness command through the approved elevated execution path when local loopback access is needed. The Service Dispatcher 33-case run then completed with all official results matching. Keep physical PC/SC commands on the required host USB path.

## JC305 MessageDigest constructor token differs from embedded ROM export — 2026-10-22

- **Symptom:** The official Security API case `MessageDigest()` called from a
  subclass returned `6F00` while executing `invokespecial`; the VM reported
  `StackUnderflowException`.
- **Cause:** The dynamic JC305 CAP references the protected no-argument
  constructor with imported class token 11 and method token 1. The embedded
  ROM export's corresponding receiver-only constructor is method token 2.
  Probing its static-method offsets showed tokens 0 and 1 each have `nargs=2`,
  while token 2 has `nargs=1`; preserving token 1 enters a mismatched method.
- **Viable fix:** In `remap_imported_constructor_method_token`, map only the
  exact Java Card Security package / MessageDigest class / imported token
  combination (AID `A0000000620102`, class 11, method 1) to token 2 for
  dynamic `invokespecial`. The isolated case and all 901 Security transport
  calls pass after the change. Keep constructor remapping separate from
  `invokestatic` mappings, which use a different token namespace.
- **Limit:** The full Security decoder still reports 13 runtime statuses and
  two wrong results elsewhere in the API area. Do not treat the 822
  `UNSUPPORTED_PER_SPEC` outcomes as defects.

## VPCD deploy applet selection uses area-specific AIDs — 2026-10-22

- **Symptom:** A current-source Service deploy test loaded its CAP but INSTALL
  failed at stage 4 with `6A82`, before any API APDUs ran.
- **Cause:** The test invocation supplied the Security applet AID
  (`4A43416C675465737441706953656331`) to the Service CAP. The Service CAP's
  `Applet.cap` encodes `4A43416C675465737441706953766331` (`JCAlgTestApiSvc1`).
- **Viable fix:** Read the target CAP's `Applet.cap` AID before VPCD installation
  and pass that exact AID. With the Service AID, current-source VPCD completed
  all 124 API cases: `SERVICE_API:SUMMARY total=124 pass=124 fail=0`.


## RMI export-capacity case includes existing applet references — 2026-10-22

- **Symptom:** The JC305 Service API's `CardRemoteObject.export()` capacity
  case returned `SystemException.NO_RESOURCE` on the 16th object.
- **Cause:** The test applet had already exported its initial remote object and
  other API-case objects in the same applet context; a 16-slot registry leaves
  fewer than 16 slots for the declared minimum-capacity case.
- **Viable fix:** Use a bounded table with headroom (32 entries in the current
  implementation). The full 124-case Service catalog then passes. Keep lookup
  restricted to exported objects; explicit SELECT-session retention still needs
  implementation and testing.

## JC305 RMI names come from CAP remote metadata, not Debug — corrected 2026-10-22

- **Symptom:** `RMIService` needs to return a remote reference descriptor, but
  the runtime CAP model exposes class/method descriptors without Java package,
  class, or interface names.
- **Correction/evidence:** The earlier Debug-component conclusion was wrong.
  The Java Card VM specification places `remote_interface_info` (including
  remote method hashes, hash modifier, implementation class name, and remote
  interface references) in the Class component for CAP format 2.2+. Debug is
  not required for those fields. The supplied `AlgTestApiService_305.cap`
  Header is format 2.1 (`minor=1`, `major=2`), so it cannot serve as a remote
  metadata fixture. Its packaged `.class` resources are not CAP load
  components.
- **Current implementation/evidence:** `jc_cap_class.hpp` now exposes a
  bounded signature-pool prefix and remote interface names. The metadata test
  uses exact Header/Class bytes emitted by the JCDK 3.0.5 converter for a
  remote-enabled CAP 2.2 package; it checks package/interface/class names,
  method hashes, signature offsets, virtual tokens, and remote interface refs.
  Pure encoders for both `remote_ref_with_class` and
  `remote_ref_with_interfaces` pass fixture known-answer tests, including
  reserved object-ID and output-capacity rejection. This validates component
  and descriptor layout only. The generated CAP is not yet loaded or executed
  by the VM, and no RMI client interoperability has been shown.
- **Viable next step:** Add the native metadata bridge and wire descriptor
  encoding into `RMIService` SELECT FCI handling; then load the generated
  remote-enabled CAP in current-source VPCD and verify with the official JCRMI
  client. Do not claim interoperability before SELECT/INVOKE is verified.

## Fresh-process VPCD install recovery needs the assigned package ID — 2026-10-22

- **Symptom:** The journal-clear sync-failure restart test initially returned
  `6F00` on SELECT after emulator restart, despite the injected failure
  occurring after registry publication.
- **Cause:** CAP LOAD selected package ID 56, while the test restarted VPCD
  with its requested/default ID 63. Recovery could not resolve the pending
  journal against the supplied CAP image.
- **Viable fix:** Include the actual package ID in the host fault-injection
  marker and pass that ID to the restarted process. The case then passed with
  SELECT and the JCAlgTest smoke sequence (`TEST:SUMMARY pass=17 fail=0`). Keep
  this requirement for future process-restart tests involving dynamic CAP IDs.

## Restored applets lose transient RMI exports — viable workaround 2026-10-22

- **Symptom:** The CAP 2.2 remote fixture installed and restored successfully,
  but its first SELECT FCI contained the null object ID `FFFF`.
- **Cause:** Installed applet fields restore from persistent storage into a new
  VM context; the per-context exported-object registry is transient and is not
  reconstructed by the object graph restore.
- **Viable fix:** `RMIService.beginSelectionSession()` now re-exports its initial
  object if it cannot retain it in the new selection session, then retains it.
  A current-source VPCD SELECT returns object ID `0001` and the expected class
  or interface descriptor. If unexport semantics are changed later, preserve
  the distinction between restoring a new VM context and an explicit
  `CardRemoteObject.unexport()` in a live context.

## RMI interface-form SELECT P2 rejected before applet dispatch — fixed 2026-10-22

- **Symptom:** SELECT P2 `0x10` returned `6999` before `RMIService` could emit
  the interface-form reference.
- **Cause:** VPCD and CCID applet-select matchers required P2 `0x00` exactly.
- **Viable fix:** Accept SELECT P2 values where `(P2 & 0xEF) == 0`; this keeps
  normal P2 `0x00` and allows the Java Card RMI interface form `0x10`. Exact
  class-form and interface-form FCI responses now pass the current-source VPCD
  fixture.

## VPCD local socket denied in restricted execution — 2026-10-22

- **Symptom:** `tools/test-rmi-arrays-vpcd` stopped with `TEST:FAIL [Errno 1]
  Operation not permitted` before VPCD startup.
- **Cause:** The local VPCD harness opens a loopback socket, which the default
  restricted execution context denied. This was not a USB or PC/SC access
  failure.
- **Viable recovery:** Run the same emulator-only test wrapper with elevated
  execution. It passed both class-form and interface-form RMI reference tests.
  Keep the usual host USB gating for any actual USB, PC/SC, or physical-token
  operation.

## JCDK 3.0.5 direct CAP output omits int RMI signatures — resolved 2026-10-23

- **Symptom:** Added `short sumInts(int[])` and `int[] echoInts(int[])` to the
  CAP 2.2 RMI fixture and invoked the JCDK converter with `-i`. Conversion
  succeeded and warned that the package requires int support, but the emitted
  `Class.cap` remote-method table omitted both methods. The CAP inspector still
  listed only the previous signatures, so there is no trustworthy APDU test for
  the VM's guarded int-array RMI path yet.
- **Cause:** Not established. The CLI supports `-i` for the optional Java Card
  int type; the bundled `pro.javacard.ant.JavaCard` task does not expose that
  converter option. A direct converter invocation enabled `-i` but did not
  produce the expected remote signatures.
- **Resolution:** Ask Converter 3.0.5 for JCA only, then assemble that JCA with
  `com.sun.javacard.jcasm.cap.Main` (`capgen`). The same JCA that produced a CAP
  without int remote records produces all 18 records through capgen, including
  `echoInts` hash `a336` signature `dd` and `sumInts` hash `bb6c` signature
  `d4`. The fixture builder asserts both entries and the total record count.

### Follow-up verification — 2026-10-22

- The fixture builder now invokes Converter 3.0.5 directly with `-i`, because
  the bundled Ant JavaCard task has no int-support option. The emitted CAP still
  omits `echoInts(int[])` and `sumInts(int[])` from its remote-method table, even
  after `sumInts` was changed to return `short` to isolate the array parameter.
- `./tools/test-rmi-arrays-vpcd` passed through the approved elevated path:
  23/23 class-form checks and 3/3 interface-form checks. The restricted run
  failed before VPCD startup with the known loopback `Operation not permitted`
  error. No USB or physical device access occurred.
- JCRE 3.1 allows single-dimensional arrays of supported primitive types,
  including `int[]`, when integer support is enabled. See the [Java Card VM
  Specification 3.1](https://docs.oracle.com/en/java/javacard/3.1/jc-vm-spec/F12650_05.pdf).
  A direct `-out CAP JCA` run confirms the generated JCA contains both methods
  as remote interface methods and marks their `CounterImpl` implementations
  `.remote`; the generated CAP Class component still has only 16 remote-method
  records, excluding those two int-array methods. The CAP inspector now scans
  every remote class entry. The VPCD fixture passes both int[] methods,
  including 32-bit value round-trip, empty array, null array, and primitive
  short result. Class-form coverage is 28/28 and interface-form coverage is
  3/3. The bounded physical test also passes on D92 after current ARM firmware
  was flashed: SELECT, `sumInts`, signed 32-bit `echoInts`, empty, and null
  arrays all returned expected responses. It removed its temporary package and
  verified JCAlgTest still selected. Logs are in
  `artifacts/logs/rmi-array-physical-20261023T041218Z.log` and the matching
  `cycle-choupi` / `pico-powerup-persistence` logs.

## RMI exported object retained through collection — validated 2026-10-22

- **Setup:** To test VM-owned remote references as GC roots, keep a returned
  child object in the active RMI session, issue a separate APDU that allocates
  256 short-lived 64-byte arrays, and enable host collection using
  `PICO2_HEAP_GC=1` on `tools/test-pc-vpcd-applet`.
- **Evidence:** `tools/test-rmi-arrays-vpcd` reports 23/23 class-form cases and
  3/3 interface-form checks. The emulator transcript records 256 arrays
  reclaimed (`arrays=5/261 reclaimed_arrays=256 complete=1`), then the child
  method invocation succeeds (`8100009000`).
- **Viable regression:** Keep the pressure APDU between child export and child
  invocation. This validates exported RMI root retention across the collector's
  post-APDU sweep; it does not establish persistent flash-record recovery under
  the same pressure.

## VPCD process-restart persistence stress can collect after restore — validated 2026-10-22

- **Setup:** `tools/test-vpcd-persistence` now restores the committed applet,
  enables `PICO2_HEAP_GC=1`, and runs four pressure rounds by default. Adjust
  rounds with `PICO2_VPCD_PERSISTENCE_GC_ROUNDS` (0–1000).
- **Validation:** Commit/install passed 3/3; fresh-process restore plus four
  rounds passed 31/31. Each round reclaimed 256 temporary arrays and 256
  instances with complete root walks, then verified static and persistent
  aliases/type behavior.
- **Limit:** The persistent graph remains rooted during this test. It does not
  prove reloading a flash-backed object after that object was reclaimed.


## GPPro card-manager lifecycle on PC VPCD — 2026-10-22

- **Symptom:** GPPro `--list` initially received `6999` for case-1
  `00 A4 04 00 00`. After adding card-manager routing, the emulator sometimes
  reported connection refused if launched before the VPCD reader was ready;
  a later restart test also listed a loaded package without its applet.
- **Cause:** The PC session did not recognize empty-AID ISD selection or route
  manager APDUs. Separately, GET STATUS used process-local instance metadata,
  which was not rebuilt from the explicit resume arguments.
- **Viable fix:** The VPCD session recognizes default-ISD selection and routes
  manager APDUs to the PC SCP03/GP implementation. Wait for the emulator's
  `VPCD:CONNECTED` marker before starting GPPro. On resume, the host registry
  reconstructs its supported single-instance record from `--applet-aid` and
  `--class-index`. GPPro 20.01.23 listing, package DELETE, CAP reinstall, and
  post-restart selectable-applet listing all pass over Virtual PCD. Regression
  is 21/21, PC and ARM builds pass. Physical CCID validation and multi-instance
  persistent registry restoration are still open.

## Host VPCD emulator interrupted with Ctrl-C — 2026-10-22

- **Symptom:** Sending SIGINT through the PTY while the PC emulator was serving
  VPCD produced a host-emulator panic at `src/arch/host/emulator.rs:602`:
  `Unknown ptrace signal received: 639`.
- **Cause:** Not established; the signal reaches the host-side simulated CPU
  signal handler rather than producing a clean shutdown.
- **Viable recovery:** Use the bounded VPCD harness cleanup, which sends SIGTERM
  to the emulator child and waits for it, instead of injecting Ctrl-C into the
  foreground emulated process. Check the emulator log and saved scratch image
  after shutdown. Avoid Ctrl-C as a shutdown method until signal forwarding is
  fixed.

## Host VPCD persistence test must resume the allocated package ID — 2026-10-22

- **Symptom:** `tools/test-vpcd-persistence` installed and committed the probe
  applet, but the fresh emulator returned `6F00` on SELECT. Restore logging
  showed it looked for instance tag `03003F00FF`, which was absent.
- **Cause:** CAP LOAD now allocates the first available dynamic package slot;
  this run chose package 56. The wrapper hard-coded package 63 for resume, so
  the restored applet root lookup used the wrong package ID.
- **Viable fix:** The PC VPCD install marker now includes the allocated
  `package-id`. `tools/test-vpcd-persistence` extracts it from the install
  emulator log and passes it to the fresh-process restore probe. The repaired
  wrapper passes commit 3/3, restore 7/7, and reports
  `VPCD:PERSISTENCE_PACKAGE_ID package=56`. Use the actual LOAD-assigned ID for
  future resume tests; do not assume the input/default package hint was used.

## Empty transaction abort invalidated unrelated persistent caches — resolved 2026-10-22

- **Symptom:** Current-source API 3.0.5 Core test completed 291 calls with
  `OK=208, EXC_RUNTIME=55, UNSUPPORTED_PER_SPEC=11, WRONG_RESULT=8,
  APPLET_NOT_INSTALLED=8, SW_ERROR=1`. Util/AID/PIN calls failed after the
  JCSystem transaction-abort check.
- **Cause:** That check aborts an empty transaction. The rollback code advanced
  the global cache generation even though there were no persistent snapshots
  to restore. Persistent instances then reloaded unnecessarily, losing live
  runtime state required by later API calls. A/B testing confirmed the stable
  persistent-array body-tag change was not causal.
- **Viable fix:** Advance `persistent_write_rollback_generation` only when the
  rollback snapshot list is nonempty. Empty aborts preserve caches; aborts that
  restore changed records still trigger persistent-instance reload. Apply the
  same generation update when `commitTransaction()` hits `fs_sync` failure and
  restores nonempty snapshots.
- **Validation:** ARM and PC builds pass. Current-source Core VPCD passes 272
  calls, reports 11 expected unsupported APIs and 8 unavailable optional
  scenario applets, and has zero runtime, wrong-result, or SW-error rows. The
  focused transaction probe passes abort-after-write/read-after-abort and
  commit/read-after-commit (7/7 checks). A new probe case 79 also aborts an
  empty transaction between transient-array setup and the deselect-lifetime
  check; cases 30/79/31 pass 5/5 assertions and preserve both clear events and
  values. `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` also passes
  install 3/3 and restart/GC stress 33/33, including four rounds of 256
  temporary arrays and instances.
- **Limit:** Host VPCD only. Physical current-source validation, RISC-V build,
  and Multi/Ext scenario coverage were not performed.
- **Evidence:** `artifacts/results/deploy-current-core-vpcd-20261022/` and
  `artifacts/logs/jcsystem-transaction-noop-rollback-generation-20261022.log`.

## Persistent abort cache stayed stale after flash rollback — 2026-10-22 (resolved)

- **Symptom:** Running the default full `tools/test-jcsystem-transaction-applet`
  suite returned `22 9000` for case 5 (“abort restores persistent applet
  field”), while the expected response was `11`. The wrapper stopped before
  the newly added GC pressure phase.
- **Cause:** The filesystem transaction layer restores the previous field-tag
  bytes, but the live `JC_Instance` field cache still contains the value written
  before abort. `getField_Byte()` then reads the stale cache, so the applet sees
  `0x22` even though the flash record was rolled back.
- **Initial evidence:** The focused GC-only run used a fresh installation and
  cases 14–17, followed by eight rounds of 256 transient arrays and 256
  objects. It passed 54/54 but did not exercise transaction rollback.
- **Reproduction:** Case 5 alone fails with the same `22 9000` response, so
  earlier calls are not required. The complete default run also fails at case 5
  after cases 1–4 pass. Do not classify this as caused by the GC stress change.
- **Viable fix:** `FlashMemory_Handler` now advances a rollback generation
  after restoring snapshots. A live `JC_Instance` compares the generation when
  fields are accessed and reloads its persistent fields from flash after a
  rollback. This retains the cached object/array identities during normal
  operation and refreshes them only after abort.
- **Validation:** PC build, ARM build, and RISC-V build passed. Case 5 passed
  within the ordered cases 1–7 (10/10 assertions); the full JCSystem VPCD
  probe passed 80/80. Native `./tools/test-persistent-transaction` passed all
  checks. Physical transaction behavior remains unverified.
- **Additional harness correction:** The full probe then exposed that case 74
  intentionally deselected the applet but the runner did not reselect before
  case 75. The runner now reselects after checking the expected `6999`; full
  suite passes. This was a test sequencing issue, not a VM defect.

## Nested persistent array graph fixture boundaries — 2026-10-22

- **Attempted fixture:** Add a persistent `Object[][]` root holding nested
  reference arrays. The JCDK 3.0.5 converter rejects both the field and
  allocation as unsupported multidimensional arrays; no CAP is produced.
- **Second observed failure:** A one-dimensional `Object[]` containing another
  `Object[]` compiled, but installation failed with
  `stage=5 sw=6A80 vm=6F00 exception=29 opcode=37`, which maps to
  `SecurityException` at `AASTORE`. The cause was not isolated; do not classify
  this nested-array write shape as supported.
- **Viable coverage shape:** Use a persistent holder object's `Object[]` field
  with two aliases to one byte array. This exercises a nested persistent object
  and array graph without multidimensional-array metadata or nested `AASTORE`.
  The process-restart VPCD test passes this graph and mutation visibility under
  four GC-pressure rounds (38/38 total restore assertions).

## Host VPCD idle timeout and external-install persistence — 2026-10-22

- **Symptom:** `--vpcd-idle-timeout` did not emit `VPCD:IDLE_TIMEOUT` or cleanly
  exit/save after the VPCD endpoint connected.
- **Cause/fix:** The timeout was parsed and handled by `run_vpcd_transport`, but
  the loop called `read_message()` without passing the remaining timeout. It
  therefore blocked forever. The protocol reader now accepts a poll deadline;
  the loop passes the remaining time to that call and measures idle from the
  last completed APDU response, so VPCD control-only frames and long APDU
  processing do not extend or prematurely consume the idle deadline.
- **Validation:** A 2-second no-client run now exits with
  `VPCD:IDLE_TIMEOUT seconds=2`, `VPCD:DISCONNECTED result=0`, and expands the
  saved image from 32,768 to 524,288 bytes. Full two-process VPCD test using
  native `--install`: GPPro listed the applet, SELECT/GETVERSION (`1.8.2_jc305`)
  and JCSystem returned `9000` before and after process restart; both processes
  exited cleanly and saved. Logs are under
  `artifacts/results/gp-vpcd-cli-install-restart-20261022/`. Final short timeout
  smoke log: `artifacts/logs/vpcd-idle-timeout-final-20261022.log` (SHA-256
  `acbe428d3679fd17439bd2bdc5139deaf9ef80877f0b866288297319600678fa`).
- **Correction:** The earlier external-install restart failure used package ID
  63, but GP CAP LOAD allocated package ID 56. Restarting the same saved image
  with `--package-id 56 --class-index 148` restored the persistent record
  (`CHOUPI:RESTORE_READ_OK`, tag `03003894FF`) and the JCAlgTest SELECT,
  GETVERSION, and JCSystem probe passed. This resolves the apparent external
  install persistence failure. The exact LOAD-assigned package ID must be used
  in the current CLI resume fixture; automatic registry discovery is still open.

## JC305 interface-token remapping corrupted embedded ROM API calls — 2026-10-22

- **Symptom:** AES-128 CBC-NOPAD `Cipher.init()` failed during the official
  performance test, even though key creation and setting succeeded. Earlier
  static SHA-256 runs also failed during repeated calls or lost the reader.
- **Cause:** `bc_invokeinterface()` translated every external interface method
  token as if the current bytecode came from a JC305 CAP. CHOUPI's own embedded
  ROM-mask API bytecode already uses its embedded export order, so the extra
  translation dispatched calls such as `Key.getSize()` to the wrong method.
- **Viable fix:** Apply imported interface-method remapping only when the
  caller package is dynamic and uses JC305 compatibility tokens. Keep the
  package-specific AESKey interface/method maps for uploaded JC305 CAPs.
- **Validation:** ARM and RISC-V builds passed. On physical Pico
  `D92E743D5F640C0F`, AES-128 CBC-NOPAD `Cipher_doFinal()` measured
  8.58 ms/op (250 invocations) and `Cipher.init()` 53.16 ms/op; all APDUs
  returned `AA9000`. Official SHA-256 measurements were 4.04 ms/op for
  `doFinal()` and 4.38 ms/op for `update()`, with 250 invocations each. The
  official static client ran 900 seconds and stopped during RSA variants after
  538 methods had started; the partial log contained no unexpected performance
  APDU status or VM exception. A focused AES static probe passed again after
  the timeout.
- **Evidence:** `artifacts/logs/jcalgtest-performance-static-after-aes-remap-20261022.log`,
  `artifacts/logs/jcalgtest-aes-cipher-static-physical-20261022.log`,
  `artifacts/logs/jcalgtest-sha256-static-physical-20261022.log`, and
  `artifacts/logs/jcalgtest-aes-cipher-static-post-full-run-physical-20261022.log`.

## Current-source Crypto API deploy outcomes and constructor mapping — 2026-10-22

- **Evidence:** The initial two-reader Crypto run at
  `artifacts/results/deploy/current-source-two-reader-20261022/pico2.csv`
  reported 18 `OK`, 444 `UNSUPPORTED_PER_SPEC`, two `SW_ERROR`, one
  `EXC_CARDRUNTIME`, and one `WRONG_RESULT`.
- **Viable fix:** JC305 imports `Cipher()` at constructor token 1, while the
  embedded ROM export places it at token 2. Extending the existing package and
  class scoped constructor remap fixed `Cipher()` on the freshly flashed D92
  board: the exact-reader Crypto rerun no longer reports that row. Do not
  broaden constructor remaps beyond verified package/class pairs.
- **Open rows:** On the updated D92 board, `Signature()` still returned `6F00`;
  AES CBC-NOPAD `Cipher.init(Key, mode, IV, offset, length)` surfaced
  `CryptoException` reason `0x0005` (`ILLEGAL_USE`); empty `Cipher.doFinal`
  returned zero where the test expected illegal use. The root causes and fixes
  are recorded in the follow-up entry below.
- **Updated result:** `artifacts/results/deploy/crypto-ctor-remap-d92-20261022.csv`
  completed all 466 calls and cleanup: 19 `OK`, 444 `UNSUPPORTED_PER_SPEC`,
  one `SW_ERROR`, one `EXC_CARDRUNTIME`, one `WRONG_RESULT`. SHA-256:
  `780f91b12d90f815dc6840a6af8857e859598ea2543a2f1a7a49b4acd1b0d2e`.
- **Related classification:** Security `Key.isInitialized()` wrong-result rows
  follow failed transient AES-key construction and remain unsupported-feature
  fallout, as documented above.
- **Reader note:** Three firmware-running boards were USB visible and passed
  serial checks, but SmartcardIO exposed two readers after ACL refresh, and
  later only one. Avoid `pcsc_scan`; use the runner's SmartcardIO discovery.

## Host USB helper rejected by restricted execution context — 2026-10-22

- **Symptom:** Running the authorized `sudo -n /usr/local/sbin/pico2-pcsc-reader`
  helper from the default restricted command context failed with
  `sudo: The "no new privileges" flag is set`.
- **Viable solution:** Invoke USB/PCSC work through the approved elevated host
  execution path from its first attempt. For repository wrappers include
  `PICO2_HOST_USB_ACCESS=1`; for the helper itself, request elevated execution.
  The helper then granted access to three USB nodes. Do not retry USB/PCSC from
  the restricted context.

## JC305 Signature virtual dispatch used the wrong embedded method — 2026-10-22

- **Symptom:** The JCAlgTest 1.8.2 Crypto probe created a subclass of imported `javacard.security.Signature`, overrode `getAlgorithm()` to return a marker, then got `6F00` when calling it through a Signature reference.
- **Cause:** JC305's exported Signature method tokens differ from CHOUPI's embedded API method order. The imported `getAlgorithm()` token resolved to a different embedded virtual method, despite constructor dispatch being corrected.
- **Viable fix:** For the JC305 Security package AID and imported class token 15, translate the full verified virtual-method token table before resolving both ROM implementations and dynamic applet overrides. Keep the remap scoped to this package/class pair.
- **Validation:** ARM and RISC-V builds passed; ARM was flashed to Pico `D92E743D5F640C0F`. A 20-second bounded serial monitor passed 20/20 runtime checks. The exact-reader Crypto area completed 466 calls and cleanup; the `Signature()` SW_ERROR disappeared. CSV: `artifacts/results/deploy/crypto-signature-vmap-d92-20261022.csv`.
- **Remaining:** This does not resolve or characterize the separate AES IV-init card-runtime exception or zero-length `doFinal` result. No general Signature algorithms were implemented here.

## JC305 CBC Cipher update must buffer partial blocks — 2026-10-22

- **Symptom:** The AES CBC-NOPAD IV known-answer test returned
  `CryptoException` reason `0x0005`. The earlier entry incorrectly called this
  `ILLEGAL_VALUE`; in this API `CryptoException.ILLEGAL_USE` is reason 5.
- **Cause:** The CAP supplied 32 bytes as a 20-byte `update()` followed by a
  12-byte `doFinal()`. `SoftwareCipher.update()` required every call to be
  block aligned and rejected the 20-byte chunk. Java Card permits partial input
  in `update()` and requires `doFinal()` to complete the buffered block.
- **Viable fix:** Buffer the trailing partial block, process complete blocks
  during `update()`, and chain CBC IV state across calls. Let `doFinal()` join
  the pending bytes with final input, reject a nonaligned final message, and
  reset the cipher state after success. Preserve the allocation-free path for
  already aligned input; allocating a temporary array for each aligned
  `doFinal()` caused the 50-call static measurement to return `6400`. The
  optimized aligned path passes the same measurement.
- **Related semantic fix:** JC305 Cipher API documentation requires
  `CryptoException.ILLEGAL_USE` when `doFinal()` receives no input and no data
  has been supplied with `update()`. The old implementation returned zero.
- **Validation:** Both architecture wrappers passed. ARM firmware was flashed
  and verified on D92 `D92E743D5F640C0F`; the bounded runtime monitor passed.
  The physical Crypto area completed 466 calls and cleanup with 22 `OK`, 444
  `UNSUPPORTED_PER_SPEC`, and zero unexpected rows. The AES 50-call static
  probe returned `AA9000` and `TEST:PASS`; final measurement APDU elapsed
  4,114 ms (the five zero-repeat baselines were 1,229–1,250 ms each).
- **Limit:** Full `ALG_PERFORMANCE_STATIC` was not rerun. PC/SC discovery
  fluctuated between D92 and 2BED; the authorized reader helper restored both
  readers before the final successful probes. Other cipher algorithms remain
  intentionally unsupported.
- **Artifacts:** Final Crypto CSV
  `artifacts/results/deploy/crypto-stream-final-d92-20261022.csv`, SHA-256
  `467e3c71f26c2a5a927c7e33e34766893899083e34668802d9df6198fd4f3683`;
  successful static transcript
  `artifacts/logs/aes-cipher-static-after-streaming-fix-retry-20261022.log`.

## Physical JC305 Security follow-up rows depend on unsupported transient AES keys — 2026-10-22

- **Observation:** Current-source Security run on D92 completed all 901 rows: 77 `OK`, 820 `UNSUPPORTED_PER_SPEC`, two skipped RSA-2048 keypair-generation cases, and two `WRONG_RESULT` `Key.isInitialized()` follow-ups.
- **Cause:** The paired `KeyBuilder.buildKey()` calls for `TYPE_AES_TRANSIENT_DESELECT` and `TYPE_AES_TRANSIENT_RESET` all returned `NO_SUCH_ALGORITHM` (CryptoException reason 3). The later initialization checks therefore had no successfully created transient key to inspect.
- **Classification:** Retain these as unsupported-feature fallout; this run provides no reproduction of transient-key clearing failure. Do not implement transient storage or additional crypto algorithms solely to reduce result counts.
- **Related current-source hardware coverage:** Core completed 291 rows on 2BED (272 OK, 15 expected unsupported, four Ext applet methods unavailable after optional INSTALL returned `6A88` and rollback passed). Crypto completed 466 rows on D92 (22 OK, 444 expected unsupported, zero unexpected statuses). All area cleanup stages completed except the optional Ext install refusal, whose rollback succeeded.
- **Artifacts:** `artifacts/results/deploy/current-source-sec-core-post-cipher-20261022/` and `artifacts/results/deploy/crypto-stream-final-d92-20261022.csv`.

- **In-place CBC decrypt follow-up:** When input and output are the same array,
  the native CBC routine can overwrite the last ciphertext block with plaintext.
  Save that input block before the native call and use it as the next IV. The
  final implementation was rebuilt for ARM/RISC-V, flashed to D92 and 2BED,
  and passed a final 466-call physical Crypto run plus the AES 50-call static
  probe. Final CSV SHA-256:
  `aeafc4fddf95e785c062db87528ba2b090ce1ef1a7605da64828aef81741d4df`.

## Transitive custom exception RMI mapping — 2026-10-23

- **Observation:** Custom runtime and checked exceptions, each with one
  applet-defined intermediate superclass, were returned over RMI as CAP 2.2
  exception responses.
- **Result:** Current-source VPCD retained the nearest API exception mapping:
  runtime encoded as `830900009000` (`RuntimeException`) and checked as
  `830500009000` (`Exception`). The existing superclass traversal handles
  this depth for both branches.
- **Validation:** `./tools/build-rmi-array-fixture` passed;
  `./tools/test-rmi-arrays-vpcd` passed 28/28 class-form and 3/3
  interface-form checks.
- **Limit:** Longer inheritance chains, malformed hierarchy metadata, and
  complete API reason-code coverage remain unverified. No hardware was used.

## Preserve SystemException reasons in Java Card RMI — 2026-10-23

- **Symptom:** An applet calling `SystemException.throwIt(0x1234)` returned
  RMI exception type `java.lang.RuntimeException` with reason zero instead of
  Java Card `SystemException` and its reason.
- **Cause:** The RMI exception walk only recognized `java.lang` exception
  classes. JC305 framework token 13 for `SystemException` maps to embedded
  token 22. Its inherited reason is instance field slot 0.
- **Viable fix:** Recognize the framework `SystemException` class by embedded
  token 22 before continuing to `RuntimeException`, then read its reason from
  field slot 0.
- **Validation:** ARM/RISC-V builds passed. VPCD passed 29/29 class-form and
  3/3 interface-form checks. Physical D92 returned exact `822512349000` after
  the CHOUPI image was flashed; fixture cleanup and JCAlgTest preservation
  passed.
- **Limit:** Reason extraction was added for SystemException; other Java Card
  API exception classes still need equivalent coverage. Use
  `./tools/cycle choupi --serial <serial>` to flash the VM; `cycle arm` flashes
  the smoke image.

## Service Dispatcher returns SW=6400 only on one physical token — 2026-10-23

- **Symptom:** API 3.0.5 Service tests return 12 `SW_ERROR`/`CARD_ERROR` results (`SW=6400`) on Pico serial `D92E743D5F640C0F`; all 12 are `javacard.framework.service.Dispatcher` calls (`addService`, `removeService`, `dispatch`, `process`).
- **Comparison:** Two other readers, serials `8E464F4E2BBC0A19` and `2BED26ED20556535`, each returned 124/124 `OK`. The D92 token reproduced 112 `OK` / 12 `SW_ERROR` both in a three-reader run and when run alone. CAP upload and cleanup passed each time, and all readers remained available.
- **Cause:** Not yet identified. The exact firmware image/build identity of the three boards was not compared during this test.
- **Viable solution:** None known. Do not classify as USB contention: the same D92 result reproduced in a solo run.
- **Next:** Compare firmware build/image identity across the three boards; inspect Service Dispatcher failure paths and reproduce against VPCD or a freshly flashed, verified image before changing runtime behavior.
- **Artifacts:** `artifacts/results/deploy/parallel-20261023-three-token-clean-1234/`, `artifacts/results/deploy/parallel-20261023-security-three-token-1240/`, `artifacts/results/deploy/parallel-20261023-service-three-token-1255/`, and `artifacts/results/deploy/service-token3-solo-20261023.csv`.

## Parallel deploy runner empty-area fallback — 2026-10-23

- **Symptom:** `tools/run-deploy-parallel --areas Security --max-devices 3` assigns the requested area to worker 1 and an empty list to workers 2 and 3. It still launches all workers; `api_card_runner.py --areas ""` interprets the empty argument as all API areas.
- **Impact:** A supposed one-area matched run also executes every area on empty-bucket readers. This was observed in the captured logs; it did not cause reader loss. The full-area split with non-empty buckets was unaffected.
- **Next:** Make the parallel wrapper skip empty workers or add an explicit replicate mode that repeats the requested area on each reader. Until fixed, don't use a subset whose area count is less than device count.

## Same-image reflash leaves the token 3 Service failures — 2026-10-23

- **Follow-up:** Rebuilt and flashed the same full CHOUPI ARM UF2 (SHA-256 `0945dd18ce32477b82c8f224a5f1c51f45cfe3b62a6a413809de629d354b938a`) onto all three physical tokens. Every exact-serial cycle verified the flash and returned `CYCLE:PASS`.
- **Result:** Full API 3.0.5 suite on each token still yields 124/124 Service OK on `8E464F4E2BBC0A19` and `2BED26ED20556535`, versus 113 OK / 11 `SW_ERROR` (`SW=6400`) on `D92E743D5F640C0F`. D92's same 11 Dispatcher failures persisted after reflash. This rules out different firmware image contents as the explanation; token-specific state or device-dependent execution remains possible.
- **Other areas:** Security results match all three (77 OK, 822 expected unsupported including RSA-2048 cases, 2 wrong results). Core 272 OK / 15 unsupported / 4 Ext-dependent unavailable; Crypto 22 OK / 444 expected unsupported; JavacardxSec 84/84 OK. All successful installs cleaned up and partial installs rolled back.
- **Next:** Inspect persisted Java Card/runtime state per device and trace the exact Service Dispatcher methods returning `6400`; compare with VPCD and a fresh/power-reset card state. Do not attribute the mismatch to firmware revision.
- **Artifacts:** `artifacts/results/deploy/full-20261023-three-reflashed-all-areas-1315/` and the three `artifacts/logs/cycle-choupi-20261023T110*.log` files.

## Unrestricted API 3.0.5 suite exposes matching 6F00 failures on two tokens — 2026-10-23

- **Observation:** After completely reflashing the two Pico 2 devices visible in the VM with the same current ARM UF2, full API 3.0.5 runs with `--exclude ''` produced identical Security and Crypto outcome counts on both. Security: 79 calls returned `SW=6F00` (plus 702 per-spec unsupported, 106 runtime exceptions, 6 wrong results, 4 card-runtime exceptions, 4 OK). Crypto: 192 calls returned `SW=6F00` (plus 256 unsupported, 7 runtime exceptions, 11 OK). The Security/Crypto tests completed and CAP cleanup succeeded.
- **Comparison:** This differs from earlier 3.0.5 runs that used the runner's default RSA-2048 exclusion and reported Security 77 OK / 822 unsupported / 2 wrong results and Crypto 22 OK / 444 unsupported. These two new runs were both on exact-reader-pinned, freshly reflashed boards `2BED26ED20556535` and `D92E743D5F640C0F`.
- **Cause:** Not identified. The unrestricted option may expose paths reached by RSA-2048 cases, but this has not been established. First inspect the 79/192 exact calls and their ordering around RSA-2048 operations; do not attribute the failures to device differences because the totals match on both boards.
- **Viable fix:** None known.
- **Artifacts:** `artifacts/results/deploy/full-305-2BED26ED20556535-20261023.csv` and `artifacts/results/deploy/full-305-D92E743D5F640C0F-20261023.csv`.

## JCAlgTest 1.8.2 cannot pass initialization in capability profiles — 2026-10-23

- **Observation:** On Pico 2 serial `2BED26ED20556535`, exact-reader SELECT returns `9000`; GETVERSION on the all-enabled image returns `F103`, and on the 0/15 minimal image returns `6F00`. The same supplied `AlgTest_v1.8.2_jc305.cap` was freshly installed on the token. Both images pass finite runtime monitoring, so this is an applet initialization issue rather than a firmware boot failure.
- **Impact:** Neither profile produced a valid `ALG_SUPPORT_EXTENDED` CSV. AES/SHA-256 profile differences remain unverified; the earlier partial minimal CSV contains errors and must not be compared to the expected profile CSV.
- **Investigation:** JC305 and embedded security class exports have different class token assignments. The JC305 class map was corrected by matching names. Imported static-field resolution was extended to all primitive/reference static bytecodes. ARM and RISC-V builds passed; a fresh all-enabled hardware probe still returns `F103` (CryptoException reason 3 / `NO_SUCH_ALGORITHM`), so this is not yet the complete root cause. Minimal remains `6F00`.
- **Viable fix:** None established. Do not infer the exact failing constructor/provider from the `F103` alone.
- **Next:** Isolate delayed-allocation constructor stages or test imported security constant/method resolution with a small diagnostic CAP; then rerun current and minimal reports on the same exact reader and compare generated CSVs only after successful GETVERSION.
- **Artifacts:** `artifacts/results/jcalgtest-capability-profile-20261023/physical-comparison.md` and its linked logs.


## Minimal JCAlgTest 6F00 caused by stale JC305 API class-token mappings — 2026-10-23

- **Symptom:** With the 0/15 optional-capability profile, JCAlgTest package SELECT returned `9000` but GETVERSION returned `6F00` after a Java `CryptoException.NO_SUCH_ALGORITHM`.
- **Cause:** Several JC305 import class tokens were mapped to unrelated embedded API classes. In particular JC305 `ISOException` token 7 mapped to embedded `UserException` token 21, causing a static method argument underflow. The uncaught ISOException converter also looked up token 21. CryptoException catch recognition used stale tokens 5/25 instead of embedded security export token 26. JC305 class-token assignments differ from the embedded ROM export; matching by numeric order is unsafe.
- **Viable solution:** Derive class-token mappings by matching the JC305 and embedded `.exp` class names. Map JC305 framework ISOException to embedded token 18 and CryptoException to security token 26. This changed minimal GETVERSION from uncaught `6F00` to the applet’s expected `F103` translation. Both architecture builds passed; real Pico D92 returned the corrected status.
- **Remaining issue:** `F103` is still an applet initialization failure, not a valid support report. `AlgStorageTest` eagerly requests AES and DES ciphers/keys in its constructor, even when those Java Card capabilities are intentionally disabled.
- **Next:** Make the JCAlgTest fixture’s eager allocations capability-tolerant (without advertising absent algorithms), then rerun complete 0/15 and 15/15 reports.
- **Artifacts:** Session hardware output; all-enabled monitor `artifacts/logs/minimal-exception-translation-monitor-20261023.log` (ignored).

## JCAlgTest base/current support profiles on Pico 2 — 2026-10-24

- **Observation:** On physical Pico D92 (`D92E743D5F640C0F`), the base profile
  completed the full JCAlgTest 1.8.2 extended support run with 4,525 result
  rows, 10 `yes`, 4,515 `no`, and zero other row statuses. Current completed
  the same run on the same token with 15 `yes`, 4,510 `no`, and zero other
  statuses. Both generated-support comparisons passed.
- **Cause/fix for the first seven-capability profile:** With OwnerPINBuilder
  variants disabled, `OwnerPINBuilderProvider` correctly throws the documented
  `SystemException.ILLEGAL_USE`. JCAlgTest's support probe catches only
  `CryptoException`, so it wrote exception strings instead of `no` for those
  three rows. The final base profile enables the three PIN providers so the
  official report is strictly yes/no. It remains five capabilities smaller
  than current: AES-192, AES-256, DES, DES3-2KEY, and SHA-256 are no.
- **Cause/fix for duplicate CAP LOAD:** D92 already held package AID
  `4A43416C6754657374` in persistent storage. LOAD block sequence and lengths
  were valid; `finishLoad()` rejected the duplicate package, but the CCID
  handler fell through to its generic `6D00` unsupported-instruction response.
  The E8 failure path now checks APDU data bounds and returns `6A80`. A physical
  duplicate LOAD returned `6A80`, and a follow-up GPPro LIST plus JCAlgTest
  SELECT/GETVERSION/JCSystem probe confirmed the existing package remained
  available.
- **Shared JCSystem behavior:** Before every row, JCAlgTest sends `B0 E2` to
  reset its cached objects. The applet calls `JCSystem.requestObjectDeletion()`
  without catching its exception. The VM reports deletion unsupported and
  correctly throws `SystemException.ILLEGAL_USE`; the uncaught applet exception
  maps to `6F00`. This occurred 4,525 times per profile and did not change the
  following support result. The profile comparator checks result rows, not
  those reset-control APDUs. Revisit if JCAlgTest's reset behavior must also
  return `9000`.
- **Viable solutions:** Keep the three PIN capabilities enabled in `base` for
  strict report validity; reject duplicate package LOAD with a data error; do
  not count the documented unsupported-object-deletion exception as an
  algorithm support result.
- **Artifacts:** `artifacts/results/jcalgtest-profile-comparison-20261024.md`,
  base/current result CSVs under `artifacts/results/jcalgtest-profile-*-20261024/`,
  `artifacts/logs/base10-monitor-20261024.log`, and `/tmp/duplicate-load-base10.log`.

## Selectable capability profile physical acceptance — 2026-10-24

- **Constraint:** Keep `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` byte-for-byte
  upstream. Its SHA-256 is
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  Do not create or use modified JCAlgTest CAP files to accommodate profiles.
- **Observed:** The original applet completes the current-profile support sweep
  (4,525 yes/no rows, 15 yes, 4,510 no). On no-AES, its eager startup requests
  disabled crypto and GETVERSION returns `F103`; minimal JCAlgTest is skipped
  per user instruction. Use the repository `/deploy` CAPs to compare those
  profiles instead.
- **Deploy Core:** Current and no-AES each returned 17 `EXC_RUNTIME` and 2
  `WRONG_RESULT` outcomes, all involving OwnerPINx behavior; minimal returned
  8 `EXC_SYSTEM reason=ILLEGAL_USE` for disabled OwnerPINx providers and no
  wrong results. Optional Ext CAP INSTALL returned `6A88` on all three; the
  runner rolled back, and four Ext calls were unavailable. Root cause of the
  current/no-AES OwnerPINx outcomes remains unresolved.
- **Storage limit:** After cumulative stateful `/deploy` installation and test
  runs, a physical memory query reported 23 persistent bytes free. Subsequent
  Security/Crypto CAP INSTALL attempts returned `6F00` and rollback succeeded.
  Stop further state-changing tests on this card. The stateful scenarios and
  unsupported object deletion can consume persistent storage; use a fresh card
  for another complete install matrix, and record per-run free-memory values.
- **Viable procedure:** Use unchanged upstream JCAlgTest only on profiles where
  it starts (currently verified: current). For no-AES/minimal, use the original
  `/deploy` CAP suite and classify installation failures separately from
  method results. Do not interpret storage-exhausted `6F00` as a profile
  implementation failure without a fresh-card reproduction.
- **Artifacts:**
  `artifacts/results/capability-profiles-acceptance-20261024/` and logs
  `artifacts/logs/deploy-current-Core-with-scenarios-20261024.log`,
  `artifacts/logs/deploy-no-aes-Core-20261024.log`,
  `artifacts/logs/deploy-minimal-Core-20261024.log`,
  `artifacts/logs/deploy-no-aes-Security-Crypto-20261024.log`,
  `artifacts/logs/deploy-minimal-Security-Crypto-20261024.log`, and
  `artifacts/logs/deploy-current-Security-Crypto-20261024.log`.

## Virtual PCD VPCD endpoint refused emulator connection — 2026-10-24

- **Observation:** `pcscd.service` was active and `ss -ltn` showed listeners on
  ports 35963 and 35964, but after applet installation the local CHOUPI PC
  emulator logged `ERROR: connecting to vpcd endpoint 127.0.0.1:35963:
  Connection refused`. SmartcardIO consequently reported `Virtual PCD 00 00`
  absent. The emulator's in-process TCP VPCD test path works separately.
- **Impact:** The deploy Crypto client could not run through the daemon-backed
  Virtual PCD reader. No physical Pico CCID device was enumerated either, so
  hardware testing is currently unavailable.
- **Cause:** Not identified. The listener shown by `ss` may not be accepting
  connections in the execution context; do not treat this as a symmetric
  crypto firmware result.
- **Viable fix:** None established. The repository's direct `tools/test-pc-vpcd-applet`
  path can validate CAP installation and APDUs without the PC/SC VPCD daemon.
  Retry physical tests once a Pico CCID reader is visible; investigate the
  daemon-backed bridge separately if it remains needed.
- **Artifacts:** `artifacts/results/symm-crypto-vpcd-crypto-20261024/`.
## Symmetric crypto profile has no physical test target visible — 2026-10-24

- **Observation:** After both RP2350 architecture builds passed, the approved
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper reported
  `Expected 1 to 3 Pico 2 CHOUPI CCID devices (2e8a:0009); found 0.`
- **Impact:** No physical flash, finite serial monitor, Crypto CAP known-answer
  test, or `/deploy` Crypto run was possible for `symm_crypto`. The PC emulator
  smoke is not a substitute for those checks.
- **Cause:** No Pico CCID reader was attached to the VM/visible to pcscd at the
  time of the check.
- **Viable solution:** Re-run the helper through the approved host-access path
  when a card is attached. Confirm an identified reader before flashing; do
  not flash a board in BOOTSEL without first identifying the expected target.
- **Artifacts:** `artifacts/logs/symm-crypto-vpcd-applet-final-20261024.log`.

## Symmetric crypto emulator vector coverage — 2026-10-24

- **Observation:** The in-process PC VPCD test exercised 16 AES/DES cipher
  variants with encrypt and decrypt, 13 MAC outputs, and valid plus modified
  signature verification. All 71 vectors matched independent Python
  `cryptography` results (`TEST:SUMMARY pass=73 fail=0`, including ATR/SELECT).
- **Impact:** The profile's cipher and MAC primitives now have broad PC VM
  known-answer coverage, including the exact-size padded-decrypt output path.
  This does not substitute for RP2350 timing/stack behavior or the requested
  physical `/deploy` run.
- **Viable solution:** Run `./tools/test-symm-crypto-vpcd` for deterministic
  offline vectors. On attached hardware, rebuild both architectures, flash
  ARM, then run physical KATs and the `/deploy` Crypto area.
- **Artifacts:** `tools/tests/symm_crypto_applet/SymmCryptoProbe.java`,
  `tools/tests/test_symm_crypto_vpcd.py`,
  `artifacts/logs/symm-crypto-kat-vpcd-20261024.log`.

## ISO9797-M1 cipher decryption retains zero padding — 2026-10-24

- **Symptom:** The `/deploy` API 3.0.5 Crypto area returned `WRONG_RESULT` for
  `ALG_DES_CBC_ISO9797_M1`, `ALG_DES_ECB_ISO9797_M1`,
  `ALG_AES_CBC_ISO9797_M1`, and `ALG_AES_ECB_ISO9797_M1` on physical 8E.
- **Cause:** `SymmetricCipherProvider` stripped trailing zero bytes on M1
  decrypt. Java Card `Cipher` preserves ISO9797-M1 padding bytes on decrypt;
  zero padding cannot encode the original input length. The emulator and
  physical probe had incorrectly expected the unpadded short message too.
- **Viable solution:** Do not unpad M1 decrypt output. Expect the complete
  block-aligned plaintext including zero padding, and allocate a full output
  buffer for M1. Keep unpadding for ISO9797-M2 and PKCS5.
- **Validation:** PC VPCD KAT passed 71/71. Physical Pico `2BED26ED20556535`
  passed 71/71 KATs. The official `/deploy` Crypto run passed all symmetric
  M1 cases: totals were 158 OK, 305 `UNSUPPORTED_PER_SPEC`, three Signature
  `SW=6F00`, and zero `WRONG_RESULT`.
- **Related harness issue:** A stale installed probe CAP still used the old
  3-byte M1 decrypt output array and produced four `6F00` responses after the
  firmware change. Rebuild and reinstall `symm-crypto-probe.cap` when its
  Java Card applet source changes; those errors were not firmware failures.
- **Artifacts:** `artifacts/logs/symm-crypto-kat-vpcd-20261024.log`,
  `artifacts/logs/symm-crypto-kat-pcsc-20261024T120000Z.log`, and
  `artifacts/results/symm-crypto-deploy-crypto-2BED-20261024.csv`.

## JC305 RSA key calls use shifted interface method tokens — 2026-10-24

- **Symptom:** JCAlgTest's RSA key builder succeeded, but calls such as
  `RSAPublicKey.setModulus` returned `6F00` or invoked a different method.
- **Cause:** JC305 security interface method tokens for RSA public/private and
  RSA CRT keys do not match the embedded ROM interface declaration order. The
  call dispatcher translated class tokens but did not remap these method
  tokens, so getters/setters were shifted.
- **Viable solution:** Apply package AID and imported class token scoped method
  maps for JC305 RSA public/private and CRT interfaces in
  `choupi/src/jc_handlers/jc_cp.cpp`. The VPCD key storage vectors and
  physical RSA cipher vectors then passed.
- **Validation:** VPCD asymmetric suite reported `TEST:SUMMARY pass=20 fail=0`.
  Pico 2 `2BED26ED20556535` passed 18/18 RSA cipher known-answer vectors.
  Reader `D92E743D5F640C0F` accepted runtime firmware but probe CAP INSTALL
  returned `6581` after LOAD; its existing JCAlgTest package was preserved.
- **Viable workflow:** Use the board with sufficient persistent CAP storage
  (`2BED26ED20556535`) for probe CAP hardware checks. Do not remove the
  JCAlgTest package from D92 to make room.
- **Artifacts:** `artifacts/logs/asymm-crypto-digest-vpcd-20261024.log`,
  `artifacts/logs/cycle-choupi-20261024T152252Z.log`,
  `artifacts/logs/cycle-choupi-20261024T152710Z.log`, and
  `build/symm-crypto-probe/asymm-physical-vectors.tsv`.

## Pico USB-visible but absent from PC/SC after asymmetric-profile flash — 2026-10-24

- **Symptom:** Asymmetric-profile image flashed and passed the finite monitor on
  Pico `2BED26ED20556535`. The subsequent upstream deploy coordinator found no
  physical PC/SC readers and did not begin applet installation. The approved
  ACL helper then granted access to two USB nodes (`001/123`, `001/004`), and
  `lsusb -d 2e8a:0009` listed two Pico 2 CHOUPI CCID devices. A bounded
  `pcsc_scan -r` timed out without output; the deploy coordinator still found
  no physical readers. `pcscd.service` reported active/running.
- **Cause:** Unknown. USB enumeration and ACL grant do not prove SmartcardIO or
  PC/SC reader enumeration; no physical APDU test could start in this state.
- **Viable solution:** None established for this occurrence. Preserve the
  device identities and logs; continue offline while PC/SC is unavailable. If
  multiple boards are attached, retest another exact reader once PC/SC exposes
  it. Do not infer a crypto failure from this reader-discovery failure.
- **Validation:** The successful flash/runtime evidence remains
  `artifacts/logs/cycle-choupi-20261024T163247Z.log`. The attempted deploy
  output was a no-reader error before worker start.

## RSA key source leaked into profiles that disable RSA — 2026-10-24

- **Symptom:** ARM `symm_crypto` builds attempted to compile
  `native_rsa_key.cpp`, which references ROM class and field metadata absent
  from that profile; compilation failed with undeclared RSA provider metadata.
- **Cause:** The ARM and PC CHOUPI CMake source globs included
  `native_rsa_key.cpp` unconditionally, before the profile-gated append logic.
- **Viable solution:** Exclude `native_rsa_key.cpp` from both globs, then append
  it only when an RSA key type or transient RSA key variant is enabled. Verify
  with one RSA-disabled and one RSA-enabled profile.
- **Validation:** ARM and RISC-V wrapper builds passed for `symm_crypto` and
  `asymm_crypto`; the symmetric ARM build no longer schedules RSA key code.
- **Artifacts:** `/tmp/build-arm-symm-final.log`,
  `/tmp/build-arm-asymm.log`, and `/tmp/build-riscv-asymm.log`.

## Stale persistent child tags block package deletion and reinstall — 2026-10-24

- **Symptom:** On Pico `D92E743D5F640C0F`, GPPro package DELETE returned
  `6581`. The overlay contained 4,775 records and 65,535/65,536 bytes; an
  earlier JCAlgTest install had failed while writing a static array.
- **Cause:** Older cleanup tombstoned parent tags before their compressed child
  records. A child still existed in the overlay, but ordinary `find_overlay`
  could no longer resolve it through its deleted parent. Reconstructing the
  child's full tag and passing it back through `fs_erase` therefore failed.
  Near capacity, deleting a small record first could also fail before a larger
  payload had freed space.
- **Viable solution:** Enumerate matching live records from their stored tag
  encodings, erase by overlay slot while preserving the encoded key bytes,
  process deeper tags before parents, and reclaim larger payloads before small
  records. Keep the regression tests for an orphan child and a nearly-full
  overlay. After applying this firmware, GPPro DELETE succeeded; `--list`
  showed only the ISD, and unchanged upstream JCAlgTest v1.8.2 LOAD/INSTALL
  returned `9000`.
- **Validation:** `./tools/test-rp2350-fs-overlay` ended with
  `TEST:SUMMARY fail=0`; physical post-install `--list` reported the applet as
  `SELECTABLE`. Commits: `f771800`, `986b277`.
- **Artifacts:** `artifacts/logs/gppro-empty-install-physical-20261024.log`,
  `artifacts/logs/monitor-empty-firmware-physical-20261024.log`.

## BOOTSEL ROM drops Pico runtime serial — 2026-10-24

- **Symptom:** `picotool --ser D92E743D5F640C0F` reports no accessible device
  after BOOTSEL enumeration (`2e8a:000f`); the ROM bootloader exposes no
  runtime USB serial and has no CDC port.
- **Cause:** Flashing code kept selecting by runtime serial after reset.
- **Viable solution:** Capture the device's stable Linux sysfs USB port path
  while it has the serial. After 1200-baud CDC reset, verify the same port and
  select BOOTSEL with picotool `--bus`/`--address`. For a board already in
  BOOTSEL, proceed only if exactly one BOOTSEL Pico is visible. USB operations
  still require the elevated host-access execution path.
- **Validation:** The updated flasher recovered this board, verified the
  `pico2_choupi.uf2` write, and the final finite monitor passed.
- **Commit:** `986b277`.

## JCSystem persistent array body hidden by a resurrected overlay tombstone — 2026-10-25

- **Symptom:** On physical Pico 2 `2BED26ED20556535`, a committed scalar
  transaction appeared to make a 32,767-element persistent array unreadable;
  its `length` access returned `6F00` (`IOException` during body lookup).
- **Cause:** The persistent overlay could contain a legacy parent-slot-encoded
  child tombstone and a newer live record for the same logical tag. When its
  parent tag was later recreated, the old child tombstone became resolvable
  again. `find_overlay()` returned the first matching record, allowing that
  stale tombstone to mask the newer live array body.
- **Viable solution:** New writes always use deterministic tag compression;
  do not encode child tags through a parent slot. Ignore deleted parents when
  choosing an encoding. For legacy duplicates, scan the complete hash probe
  cluster and choose the newest live matching record by append offset; use the
  newest tombstone only when no live record exists. Regression coverage builds
  a parent-encoded child tombstone, deletes its parent, writes a newer live
  compressed child, recreates the parent, and verifies the live bytes win.
- **Validation:** `./tools/test-rp2350-fs-overlay` passed with
  `TEST:SUMMARY fail=0`; ARM and RISC-V wrapper builds passed. Physical 2BED
  returned array length `7FFF` after scalar commit, after firmware cycle, and
  after CCID reset. The transaction probe ended `TEST:SUMMARY pass=9 fail=0`.
  A physical power-off/on was not performed.
- **Artifacts:** `artifacts/logs/jcsystem-transaction-physical-20261025T124715Z.log`;
  cycle `artifacts/logs/cycle-choupi-20261025T125039Z.log`.

## Firmware reflash does not make a used card empty — 2026-10-24

- **Symptom:** JCAlgTest remained selectable after uploading a new ARM UF2,
  appearing to be preinstalled in the firmware.
- **Cause:** The UF2 carries firmware/ROM API data, but its flash update does
  not wipe the separate persistent filesystem and CAP store. In this case the
  applet was deliberately installed earlier with GPPro and survived the
  subsequent firmware update.
- **Viable solution:** For a fresh emulator card, start with an empty flash
  image and verify GPPro `--list` reports only the ISD before explicitly
  running `gp --install <upstream-cap>`. For a used physical card, authenticate
  and delete applet AID `4A43416C675465737431` first, then package AID
  `4A43416C6754657374`; verify the registry is empty except for the ISD.
  Reflashing alone is not a card reset and must not be documented as one.
- **Validation:** `./tools/test-gppro-empty-install-vpcd` passed from a fresh
  zero-length flash file: only the ISD was listed before install, and the
  unchanged JCAlgTest CAP loaded and installed successfully through GPPro.
  Physical deletion is pending because the authorized host helper found zero
  visible Pico devices on 2026-10-24.
- **Artifacts:**
  `artifacts/results/gppro-empty-install-vpcd-20261024T212248Z/`.

## Multi-board BOOTSEL selection and clean-card verification — 2026-10-24

- **Symptom:** With several Pico 2 boards attached in BOOTSEL, the ROM no
  longer exposes the firmware USB serial, making order-based selection unsafe.
- **Viable solution:** Use the RP2350's stable 16-hex chip ID with
  `./tools/flash --chipid <ID> <image.uf2>` or
  `./tools/cycle choupi --profile <profile> --chipid <ID>`. The flasher matches
  each visible BOOTSEL device using pinned picotool's reported chip ID, then
  retains that device's USB port through re-enumeration. Never select among
  multiple BOOTSEL boards by bus enumeration order.
- **Validation:** `bash -n tools/flash tools/cycle` passed. The chip-ID path
  was exercised during a successful `symm_crypto` cycle on D92; its ARM and
  RISC-V builds passed and the bounded runtime monitor reported
  `TEST:SUMMARY pass=1 fail=0`. The 2BED board was not attributed a final
  runtime result because it disappeared from this VM after flash while a
  different reader remained visible.
- **Clean-card result:** On D92, GPPro deleted the explicitly installed
  JCAlgTest applet and package; the final `gp --list` showed only the ISD.
  The firmware image does not embed the applet, and reflashing preserves the
  separate persistent CAP store.
- **JCAlgTest result:** Official v1.8.2 `ALG_SUPPORT_EXTENDED` completed
  4,525 rows on D92 (73 yes, 4,452 no, zero other). The first comparison
  reported 46 expected identities against 73 raw yes rows because the client
  emits legacy Signature section names and cipher/signature factory aliases.
- **Viable solution:** Normalize the legacy Signature section and map known
  factory-method identities back to their canonical algorithm capability;
  retain the specifically configured CBC `getInstance` rows independently.
  The archived physical report then passes at 46 expected/actual identities,
  with zero unexpected or missing. Unsupported algorithms remain expected.
- **Validation:** `python3 tools/test-javacard-support.py` passes all 9 tests;
  archived D92 report comparison prints `COMPARE:PASS`.

## Pico leaves BOOTSEL but runtime CDC is not passed through — 2026-10-25

- **Symptom:** RP2350 `8E464F4E2BBC0A19` was visible in BOOTSEL and picotool
  wrote and verified the full ARM image, but after reboot
  `tools/monitor --serial 8E464F4E2BBC0A19` returned
  `ERROR: no USB serial device found`.
- **Cause:** Runtime serial enumeration was not passed back into this VM for
  that board. Successful UF2 verification alone does not prove runtime health
  or PC/SC visibility.
- **Viable solution:** Record the board as unavailable for runtime testing and
  immediately fail over to another identified Pico. D92
  `D92E743D5F640C0F` remained a PC/SC reader and passed the same finite
  `symm_crypto` cycle. When starting from runtime mode with `--chipid`, match
  its runtime USB serial to the same chip ID, then use that serial for CDC
  reset. Select boards already in BOOTSEL directly through picotool chip-ID
  lookup.
- **Validation:** D92 ARM/RISC-V builds passed, flash verification reached
  100%, the finite monitor passed, and GPPro showed only the ISD after the
  final reflash. Logs: `artifacts/logs/cycle-choupi-20261024T220417Z.log`
  (8E flash succeeded, monitor failed),
  `artifacts/logs/cycle-choupi-20261024T220800Z.log` (D92 pass), and
  `artifacts/logs/cycle-choupi-20261024T221145Z.log` (D92 chip-ID from runtime
  pass).

## `/deploy` runner resolves output paths from its own directory — 2026-10-25

- **Symptom:** Calling `deploy/run_api_test.sh` with a repository-relative
  `--out artifacts/...` failed before card access because the wrapper changes
  its working directory to `AlgTest_JavaCard/deploy`.
- **Viable solution:** Pass an absolute workspace path for `--out` and
  `--dump-dir`, or use a path rooted under `AlgTest_JavaCard/deploy`.
- **Validation:** Physical D92 Core, Security, and Crypto 3.0.5 runs then
  completed and each installed CAP was removed. JCSystem had 37 `OK` results
  and one expected `requestObjectDeletion()` `ILLEGAL_USE`; Security had 97 OK
  and 804 per-spec unsupported; Crypto had 161 OK and 305 per-spec unsupported.
  Results: `artifacts/results/deploy/symm_crypto-D92-20261025/`.
- **Artifacts:** `artifacts/results/jcalgtest-symm-crypto-D92-20261024/`;
  `artifacts/logs/cycle-choupi-20261024T213745Z.log`.


## JC305 `checkcast` class/interface token collision — 2026-10-25

- **Symptom:** In physical API 3.0.5 Core tests, OwnerPINx Builder values
  failed casts with `ClassCastException` (17 runtime errors and two wrong
  results). Ordinary OwnerPIN worked.
- **Cause:** A `checkcast` operand is a `Classref`, but in this imported-package
  mapping the same numeric token may identify a class export and an interface
  export. Resolving only the class namespace could succeed while selecting an
  unrelated class record, so code never consulted the valid interface mapping.
- **Viable fix:** In `Bytecodes::docheck`, resolve class and interface token
  namespaces independently. Check assignability against the ordinary resolved
  candidate first, then against the interface candidate if distinct and needed.
  Preserve exception behavior if no interface mapping exists. Do not globally
  reinterpret all class references as interfaces.
- **Validation:** PC/VPCD full JCSystem/OwnerPINx suite passed 84/84, including
  positive and negative casts. ARM and RISC-V `symm_crypto` builds passed; the
  ARM image was flashed and its bounded monitor passed. Physical D92 API 3.0.5
  Core completed all 291 calls: 272 OK, 15 `UNSUPPORTED_PER_SPEC`, four
  optional Ext `APPLET_NOT_INSTALLED`; all OwnerPINx rows passed. CAP cleanup
  passed and GPPro listed only ISD.
- **Evidence:** `artifacts/logs/jcsystem-ownerpinx-profile-fixed-full-vpcd-20261025.log`,
  `artifacts/logs/cycle-choupi-20261024T232303Z.log`, and
  `artifacts/results/deploy/symm_crypto-D92-ownerpinx-fixed-20261025.csv`.
  Source commit: `fa15bfa`.


## VPCD registry restore failure must stop card service — 2026-10-25

- **Symptom:** Corrupt or missing `.cap-registry` metadata produced a clear
  storage initialization error, but `starting_jcre()` still proceeded to open
  the VPCD transport. An invalid restored registry could therefore appear to
  start as a usable card.
- **Cause:** `choupi_pc_initialize_storage()` returned an error through the
  syscall, but the host startup callback did not retain that result across the
  JCRE startup call.
- **Viable fix:** Latch PC storage initialization failures in the host process
  and make `starting_jcre()` return before applet installation or VPCD service.
  Test the diagnostic and assert that no VPCD connection is established.
- **Validation:** The complete multipackage VPCD regression passes 20/20.
  Corrupted CRC and missing-sidecar cases both log the failure and never emit
  `VPCD:CONNECTED`; valid sidecar restore and applet cleanup still pass.
- **Test-harness note:** Reused test memory paths must remove their prior
  `.cap-registry` sidecars when starting from an empty image. Poll child status
  while awaiting VPCD accept so failed startup is reported immediately instead
  of waiting for the full listener timeout.


## Pico reader ACL refresh and GPPro timeout failover — 2026-10-25

- **Symptom:** USB enumeration and `pcsc_scan -r` listed D92 and 2BED, but
  GPPro `--list` timed out on both immediately after the device inventory
  changed. After refreshing the approved reader ACL helper, 2BED answered
  GPPro normally. D92 still timed out during its read-only listing and was not
  modified.
- **Viable recovery:** Run the authorized
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper through the host-access
  path after device re-enumeration, then fail over to another enumerated board
  if GPPro still stalls. Do not infer reader health from `lsusb` or
  `pcsc_scan` alone.
- **Validation:** The helper granted access to two USB nodes. GPPro 26.06.04
  listed 2BED as ISD-only after its current-profile firmware cycle. A bounded
  GPPro query to D92 did not complete; no writes were attempted there.
- **Evidence:**
  `artifacts/logs/cycle-choupi-20261025T073359Z.log` and
  `artifacts/logs/gppro-current-profile-2BED-20261025.log`. D92 timeout logs
  under `artifacts/logs/gppro-current-profile-D92-timeout-20261025*` are partial
  because output buffering hid the APDU progress on timeout.


## Capability-profile physical Core/Security/Crypto follow-up — 2026-10-25

- **OwnerPINx fix cross-profile result:** The `checkcast` namespace fix passed
  the upstream API 3.0.5 Core area on physical D92 under both `current` and
  `no_aes`. Each run completed 291 calls: 272 OK, 15 per-spec unsupported,
  and four optional Ext calls unavailable after INSTALL returned `6A88` and was
  rolled back. All nine OwnerPINx/OwnerPINxWithPredecrement report rows were OK
  in both profiles. The previous 17 runtime errors and two wrong results are
  gone in these profiles too.
- **Minimal Crypto:** After minimal firmware flash, the unchanged Crypto CAP
  uploaded and ran all 466 calls: 4 OK and 462 `UNSUPPORTED_PER_SPEC`, with no
  other statuses. Cleanup passed. The earlier minimal Crypto INSTALL `6F00`
  failure was not reproduced after the preceding test packages had been
  cleaned up; its original cause remains unconfirmed.
- **Current Security/Crypto:** Current Security completed 901 calls (77 OK,
  822 `UNSUPPORTED_PER_SPEC`, and two dependent transient AES-key
  `Key.isInitialized()` `WRONG_RESULT` rows; the key construction cases were
  unsupported). Current Crypto completed 466 calls (22 OK, 444
  `UNSUPPORTED_PER_SPEC`) without unexpected statuses. Both cleanups passed.
- **Card state:** After each profile batch, GPPro listed only ISD
  `A000000151000000`; no test CAP remained. D92 was left running `no_aes`.
- **Evidence:** `artifacts/results/deploy/current-D92-core-ownerpinx-fixed-20261025.csv`,
  `current-D92-security-20261025.csv`, `current-D92-crypto-20261025.csv`,
  `minimal-D92-crypto-20261025.csv`,
  `no-aes-D92-core-ownerpinx-fixed-20261025.csv`; cycle logs
  `artifacts/logs/cycle-choupi-20261024T235250Z.log` and
  `artifacts/logs/cycle-choupi-20261025T000109Z.log`.
## Physical JCSystem BUFFER_FULL probe can leave the runtime wedged — 2026-10-25

- **Symptom:** The first physical attempt to trigger Java-catchable
  `TransactionException.BUFFER_FULL` returned `6F00`. A PC/SC `disconnect(true)`
  and GPPro DELETE then returned `6581` (Memory failure), although the applet
  and package still appeared in GET STATUS.
- **Recovery:** Rebuild ARM and RISC-V with the intended `symm_crypto` profile,
  then use the gated `./tools/cycle choupi --profile symm_crypto --chipid
  2BED26ED20556535`. After refreshing PC/SC access, GPPro DELETE of the test
  applet and package succeeded. The same image hash was flashed; the full
  six-assertion physical probe then passed and its cleanup left only ISD.
- **Guidance:** A PC/SC reset may not reset the VM's transaction/flash runtime
  state after an uncaught exception. If a bounded APDU returns `6F00` and
  cleanup fails, capture serial diagnostics, stop repeating writes, perform a
  full known-good firmware reboot through the repository cycle wrapper, then
  verify and clean the exact test AID. Do not call the first failed attempt a
  passing capacity test.
- **Validation:** Physical success log
  `artifacts/logs/jcsystem-transaction-physical-20261025T095547Z.log`; failed
  attempt log `artifacts/logs/jcsystem-transaction-physical-20261025T094330Z.log`;
  recovery cycle log `artifacts/logs/cycle-choupi-20261025T095236Z.log`.

## JCSystem VPCD runner must clear its registry sidecar for a fresh image — 2026-10-25

- **Symptom:** Focused VPCD tests failed before connection with
  `VPCD CAP registry does not match saved memory image` after the test fixture
  CAP changed, because the runner truncated its memory image but retained the
  old `.cap-registry` sidecar.
- **Viable fix:** For non-resume runs, remove the sidecar whenever the harness
  initializes an empty memory image. The JCSystem harness now does this and
  writes default logs to unique UTC timestamped paths so it cannot overwrite a
  historical tracked log.
- **Validation:** Focused cases 5 and 83 passed `4/4`; the full
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-jcsystem-transaction-applet --timeout 120`
  run passed `TEST:SUMMARY pass=85 fail=0`.

## JCSystem applet SELECT can fail on a dangling persistent array alias — 2026-10-25

- **Symptom:** On physical 2BED (`2BED26ED20556535`), GPPro installs the
  transaction probe successfully, but the first SELECT returns `6F00`. Serial
  identifies applet instance root `03003800FF`; persistent field 5 exists as
  `03003800FF0505`, but restoring its serialized type throws `IOException`
  (`id=5`, restore stage 1). A detailed serial capture showed this field is an
  array alias to package-static tag `0238000505FFFF`, for which `fs_exists`
  was false and `fs_length` reported missing.
- **Interpretation:** The failing record is a dangling alias: the instance
  field survives while the package-static array body it names has been erased.
  Firmware reflashing preserves filesystem state, so reflashing alone cannot
  clean up this condition.
- **Rejected workaround:** Recreating the body through the restored array
  handle is not viable. `JC_Array::size()` reads the missing body header, so
  this attempt throws `IOException` and makes the otherwise successful GPPro
  install fail with `6A80`. The attempt was reverted.
- **Recovery:** The physical runner's cleanup trap successfully deletes the
  probe package after SELECT fails. A full bounded firmware cycle also
  restores runtime responsiveness; the source-matching image currently passes
  `TEST:SUMMARY pass=1 fail=0`.
- **Next fix:** Review package deletion's static-field prefix cleanup and
  persistent array identity/alias serialization. Ensure no instance alias is
  committed unless its target body remains valid, and test delete/reinstall
  without altering upstream CAP files.
- **Evidence:** `artifacts/logs/jcsystem-restore-current-serial.log`,
  `artifacts/logs/jcsystem-transaction-physical-20261025T111649Z.log` (failed
  fallback experiment; cleanup passed), and
  `artifacts/logs/cycle-choupi-20261025T112220Z.log`.

## Three LEDs active while PC/SC exposes only one Pico — 2026-10-25

- **Symptom:** The user reports all three physical Pico 2 devices show the
  firmware LED indication. The approved reader helper granted access to two USB
  nodes, but a bounded `pcsc_scan -r` listed only D92
  (`D92E743D5F640C0F`) and the two Virtual PCD readers.
- **Interpretation:** LED activity confirms user-observed firmware behavior but
  does not establish PC/SC enumeration or APDU access. The two USB nodes do not
  identify three active smart-card readers.
- **Viable solution:** None established for this occurrence. Continue source
  work and use any exact Pico reader that PC/SC exposes; retry other boards only
  after they appear as readers. Do not infer a firmware or applet failure from
  missing PC/SC enumeration alone.
- **Validation:** `sudo -n /usr/local/sbin/pico2-pcsc-reader` returned
  `PCSC:ACCESS_GRANTED count=2`; `timeout 8s pcsc_scan -r` exited 0 and listed
  Virtual PCD 00 00, Virtual PCD 00 01, and D92.
- **Follow-up:** The upstream API 3.0.5 Service deploy runner pinned to D92
  failed before test execution: GPPro INSTALL ended `UPLOAD_NO_CONNECT` after
  four attempts, with `SCARD_E_NOT_TRANSACTED` reported after roughly 201
  seconds. The CSV has only the failed UPLOAD stage and zero API calls. This
  confirms reader enumeration and ACL access are insufficient to establish a
  working APDU path; it does not indicate a two-device firmware limit.
- **Evidence:** `artifacts/results/deploy/parallel-20261025-142230/pico1.csv`
  and `pico1.log`.
- **USB/driver follow-up:** `lsusb -d 2e8a:0009` found two distinct serials,
  `D92E743D5F640C0F` and `2BED26ED20556535`. `lsusb -t` showed the D92 CCID
  interface as `Driver=usbfs` and the 2BED CCID interface as `Driver=[none]`.
  The approved helper granted access to both USB nodes and restarted pcscd,
  but a subsequent bounded PC/SC scan still listed only D92. The 2BED interface
  binding is the next host-side diagnostic; the evidence does not establish a
  maximum number of simultaneous readers.
- **Retry mitigation:** `tools/run-deploy-parallel` defaults to one GPPro
  attempt and zero pause (`--gp-retries`, `--gp-pause`, and `--gp-timeout` are
  configurable). The upstream runner sleeps after the final failed attempt,
  so leaving its default 20-second pause would delay fail reporting even with
  one attempt. The prior run spent about 201 seconds retrying the failed
  INSTALL four times and never reached API calls. This coordinator does not
  automatically reassign a failed area to another reader; start a targeted run
  on a different exact reader after confirming it is usable. This does not
  repair PC/SC or prove the failure is fixed.
# Physical GET STATUS stalls on exact-packet CCID response — fixed 2026-10-25

- **Observation:** D92 `D92E743D5F640C0F` and one other Pico USB node were visible
  after the authorized PC/SC helper. `lsusb -t` showed the CCID interface as
  `Driver=[none]`, expected for libusb-backed libccid. Java SmartcardIO reader
  listing intermittently returned D92 as reader 3; other bounded attempts
  timed out or listed only Virtual PCD readers. The approved helper reported
  two nodes (`/dev/bus/usb/001/096` and `/dev/bus/usb/001/080`) on the latest
  check.
- **Card-side evidence:** Before the latest diagnostic reflash, GPPro
  authenticated and received `9000` for ISD, package, and applet GET STATUS,
  then stalled after sending authenticated `84F21002`. Firmware telemetry
  recorded `84F210020A`, APDU response length 54, SW `9000`, and CCID transfer
  completion (`stage=7`, endpoint `03`); no CCID transfer error was printed.
  The 54-byte response makes the module entry exactly 52 data bytes plus SW.
- **Follow-up change:** Added a retained, serially printed snapshot of the
  latest XfrBlock response bytes (up to 64). Both architecture wrappers build;
  registry tests pass 50/50; full ARM CHOUPI cycle on D92 passes
  `TEST:SUMMARY pass=1 fail=0`. Post-flash GPPro attempts either timed out
  before connecting or PC/SC exposed only Virtual PCD readers, so the new raw
  response bytes have not yet been captured.
- **Current image:** The latest refined diagnostic build was cycle-flashed to
  D92 and passed the finite monitor at
  `artifacts/logs/cycle-choupi-20261025T182059Z.log`. A subsequent bounded Java
  reader listing timed out; no post-flash GPPro command reached the card.
- **Root cause:** The 54-byte APDU response plus the 10-byte CCID header was
  exactly 64 bytes, one full USB bulk-IN packet. TinyUSB's low-level endpoint
  transfer does not add class-specific framing; the CCID driver returned
  directly to listening without sending the terminating zero-length packet.
  The host therefore waited for more data even though the RP2350 endpoint
  transfer had completed. The response TLV was well-formed.
- **Viable solution:** After any nonempty CCID bulk-IN response whose total
  length is a multiple of the 64-byte endpoint packet size, submit a zero-byte
  IN transfer. Do not rearm bulk OUT until the ZLP completion callback. The
  implementation also clears pending-ZLP state on endpoint errors and reset.
- **Validation:** Both architecture wrappers passed. The ARM image was flashed
  and passed finite monitor on D92 `D92E743D5F640C0F`. GPPro then received the
  P1=10 response (52 data bytes plus `9000`), issued P1=20, and completed
  `--list` with both packages and the applet. Durable log:
  `artifacts/logs/gppro-get-status-zlp-D92-20261025.log`; cycle log:
  `artifacts/logs/cycle-choupi-20261025T191520Z.log`.
- **Follow-up:** Apply the same exact-multiple framing rule to any future bulk
  CCID response path. A bounded listing completed twice after flash; no other
  reader-count conclusion follows from this fix.

## Stale VPCD flash image prevents clean RMI client startup — 2026-10-25

- **Symptom:** `tools/test-rmi-client-vpcd` used the shared historical
  `build/algtest-v182-empty-flash.bin` image. It was only 81,920 bytes and had
  serialized dynamic-package state without the matching registry sidecar, so
  VPCD startup failed before the official SDK client connected.
- **Viable solution:** The RMI wrapper now writes a fresh raw 512 KiB all-`FF`
  flash image for each run under `build/rmi-client-vpcd/flash-<timestamp>.bin`.
  The PC VPCD applet wrapper also removes `<flash-copy>.cap-registry` before
  copying its fresh input image. A sidecar is bound to the flash checksum and
  must not be reused after the image changes.
- **Validation:** `./tools/test-rmi-client-vpcd` passed with exit status 0,
  `JCRMI_CLIENT:SUMMARY pass=18 fail=0`, and emulator `VPCD:DISCONNECTED
  result=0`. Client transcript:
  `artifacts/logs/rmi-client-official-vpcd-client-20261025T190043Z.log`.

## Reused PC VPCD flash copy retained stale registry sidecar — fixed 2026-10-25

- **Symptom:** Re-running `tools/test-pc-vpcd-applet` with the same
  `--flash-copy` path copied fresh bytes over the old image but kept the old
  `<flash-copy>.cap-registry`. The emulator rejected startup because the
  sidecar checksum described the previous image.
- **Viable solution:** The test wrapper now unlinks the sidecar immediately
  after copying the selected input image to its writable test image. Runtime
  resume sidecars are still preserved between emulator processes within one
  test invocation.
- **Validation:** First reuse attempt failed with
  `ERROR: VPCD CAP registry does not match saved memory image`. After the fix,
  reusing the identical scratch path passed ATR, SELECT, and an RSA/OAEP
  support APDU (`2603...9000`, `TEST:SUMMARY pass=3 fail=0`). That first APDU
  was later determined not to be the failing one; see the correction below.

## D92 JCAlgTest support sweep loses PC/SC transaction on MD5 RSA OAEP — 2026-10-25

- **Symptom:** After the physical GET STATUS ZLP fix, a fresh official
  `ALG_SUPPORT_EXTENDED` run on D92 completed 1,393 of 4,525 rows, then
  `SCardTransmit` failed with `SCARD_E_NOT_TRANSACTED` on command
  `B07526000302030900` (JCAlgTest class `0x26`, MD5, RSA, OAEP). The
  partial CSV is not an acceptance result.
- **Board evidence:** A subsequent finite serial capture showed runtime
  recovery marker `opcode=8C count=2964 stage=0 context=00020104 origin=0`
  and a fresh CCID event count, indicating the board had reset during the
  failed session. `cause=other` means the RP watchdog did not report itself as
  the reset cause; the trigger remains unknown. The serial output was observed
  directly during the bounded monitor, not archived as a standalone log.
- **Isolation:** An earlier VPCD replay used the wrong parameters and did not
  reproduce this command. A corrected replay of exact MD5 APDU
  `B07526000302030900` against current-source VPCD with fresh erased flash
  returned `2603...9000`, the expected `NO_SUCH_ALGORITHM`, with
  `TEST:SUMMARY pass=3 fail=0`. The supported algorithm result is expected; only the
  physical transaction loss is a defect to investigate.
- **Viable solution:** Not established. `tools/JCAlgTestApduProbe.java` now has
  `--unsupported-rsa-oaep-md5` for a short exact physical replay after PC/SC
  returns. Run it with the approved host-access path and capture serial at the
  same time. Preserve the unchanged upstream CAP.
- **Evidence:** `artifacts/logs/jcalgtest-after-zlp-D92-20261025.log`, partial
  CSV under `artifacts/results/jcalgtest-after-zlp-D92-20261025/`, and
  `artifacts/logs/pc-jcalgtest-rsa-oaep-md5-20261025.log`.

## Asymmetric signature profile currently adds a Java ROM-mask provider — unresolved architecture violation (2026-10-25)

- **Finding:** The pending `signature_rsa_sha_pkcs1` changes add
  `tools/java-support/RSASignatureProvider.java`; `tools/javacard-support.py`
  copies it into the generated `fr.gouv.ssi.nativeimpl` API sources and emits
  Java `SignatureProvider` factory branches. `tools/build-pc` compiles these
  sources into the ROM mask. Although the cryptographic routines in
  `choupi/src/native_rsa_signature.cpp` are C++, the adapter/provider bytecode
  still runs on the Pico. This violates the user's explicit instruction to
  implement Java Card API behavior through native C++ dispatch and not add Java
  provider/adapter implementations to firmware.
- **Disposition:** Do not build/package this asymmetric profile state or treat
  it as implementation evidence. Preserve the uncommitted files while replacing
  the Java provider/factory path with direct native dispatch. Audit the older
  RSA key/cipher provider classes under the same requirement; do not infer
  compliance merely because their crypto operations are native.
- **Baseline validation:** `./tools/build arm` passed with profile `current`,
  15 capabilities. This excludes RSA signature and does not validate the
  asymmetric profile. Source inspection confirmed the RSA signature module is
  conditionally included only when that profile capability is enabled.

## Native Java Card API dispatch can allocate API objects directly — design finding (2026-10-26)

- **Finding:** Native methods declared in the Java Card API are dispatched by
  the VM's `callJCNativeMethod` path. `Heap::addInstance(packageID,
  classIndex)` can allocate a class instance without executing a Java
  constructor or checking the abstract-class flag. The ROM-mask JNI generator
  emits `stack.push_Reference(ret)` for a native method with a `REFERENCE`
  return type. These are the core primitives needed to replace factory
  provider subclasses with API-declared native factory and operation methods.
- **Viable direction:** Put implementation state on the API object and have
  native `getInstance` validate the requested algorithm, then allocate the
  API-class ROM object. Keep algorithm selection and all key/cipher/signature
  behavior in C++. Apply the same rule to `Signature`, `KeyBuilder`/key APIs,
  `Cipher`, and symmetric MACs; no generated provider or adapter classes.
- **Limitations:** The generator mechanics are verified by source inspection,
  not a built fixture or runtime call. An end-to-end native factory with
  reference return has not been compiled or executed. Existing symmetric MAC
  behavior must move with the factory to avoid regressing the `symm_crypto`
  profile.
- **Test-suite caveat:** `python3 tools/test-javacard-support.py` passes 9/9,
  but current assertions require generation of RSA key/cipher and symmetric
  MAC Java provider classes. This pass is not evidence of compliance with the
  native-only requirement. Replace the provider-presence expectations after
  the direct API implementation exists.


## MessageDigest native buffer must be transient — fixed (2026-10-26)

- **Symptom:** A JCAlgTest SHA-256 call stalled during digest reset, producing repeated persistent byte-write diagnostics and timing out when the digest object was retained by the applet.
- **Cause:** The native implementation kept message bytes in an ordinary heap byte array referenced from a persistent `MessageDigest` object. Clearing the data on `doFinal` therefore routed each byte write through persistent storage. The first implementation also encoded input length in the tail of the SHA scratch buffer, coupling digest state to the SHA bridge.
- **Viable fix:** Allocate the digest input as a `JC_Array` with `ClearEvent::CLEAR_ON_RESET`, keep `inputLength` in a native-owned API instance field, and pass the length explicitly into the SHA implementation. Keep reset/doFinal clearing limited to transient bytes.
- **Validation:** `python3 tools/test-javacard-support.py` passed 9/9; `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` passed. A prior exact PC/VPCD run passed 15 checks (`TEST:SUMMARY pass=15 fail=0`), including SHA-256 prepare/final and static-performance calls. No physical flash was performed for this step.
- **Reuse:** Apply this rule to other persistent API objects that maintain ephemeral operation buffers: use transient arrays or native state with a reset lifecycle, never bytewise persistent writes for scratch state.


## Native method override manifests need full Java Card descriptors — fixed (2026-10-26)

- Symptom: A manifest keyed by package, class, method name, and static/instance status cannot distinguish overloaded API methods such as the two Signature.getInstance factories.
- Viable fix: Key each override using the JCA method descriptor (name(params)return) plus static/instance status. The ROM-mask parser consumes JCAClassMethodSignature.getFullName() and still rejects entries that do not match any API method.
- Validation: Native MessageDigest descriptors matched the generated JCA exports; PC/ARM/RISC-V builds passed and PC/VPCD regression passed 15/15.
- Use: Add one manifest record per overloaded Signature.getInstance descriptor when migrating that API.
# Physical SHA-256 validation after native MessageDigest dispatch (2026-10-26)

- **Finding:** On Pico 2 D92 (`D92E743D5F640C0F`), an initial isolated SHA-256 support probe returned `FF05` after a fresh JCAlgTest installation. After reflashing the clean ARM image (which rebooted the board), the same support probe passed and the official SHA-256 `doFinal` prepare/measurement also passed. No MessageDigest source correction was needed; the factory's class resolution, object allocation, and transient buffer allocation completed on-device. The initial `FF05` cause is not established.
- **Reproduction tools:** `tools/probe-jc-algtest --sha256-support-only` and `--sha256-dofinal-only` isolate these calls against the installed upstream JCAlgTest 1.8.2 CAP. Use the exact Pico reader name and host-access wrapper requirements.
- **Physical evidence:** The support probe returned data beginning `1500` with SW `9000`; the official JCAlgTest CSV recorded `ALG_SHA_256;yes`. The doFinal prepare and measurement each returned `AA9000`, followed by `TEST:PASS sha256_dofinal_only`. `./tools/cycle choupi --chipid D92E743D5F640C0F` passed with `TEST:SUMMARY pass=1 fail=0` at `artifacts/logs/cycle-choupi-20261026T044038Z.log`.
- **Extended run:** The official `ALG_SUPPORT_EXTENDED` run reached 1,897 rows and the SHA-256 yes row. The user intentionally unplugged the card; the resulting missing PC/SC transaction is an interrupted run, not evidence of a firmware fault. The partial CSV and client log are under `artifacts/results/jcalgtest-real-d92-msgdigest-20261026/` and must not be treated as a complete acceptance report.
- **Viable solution:** For the earlier `FF05`, reflashing and rebooting resulted in a passing repeat, but the causal mechanism is unknown. Preserve this as a useful recovery experiment, not a confirmed fix. Do not modify upstream CAP files.

# Symmetric Signature MAC path migrated to native API dispatch (2026-10-26)

- **Finding:** The previous `symm_crypto` generator emitted
  `SignatureProvider`, `SymmetricMacProvider`, and `MacNativeBridge` Java
  classes into the Java Card ROM mask. This was incompatible with the required
  native API boundary.
- **Viable fix:** Route both `Signature.getInstance` overloads and all
  `Signature` instance operations through exact-descriptor ROM-mask native
  overrides. Allocate the API `Signature` class directly in C++, keep MAC
  computation native, and use reset-cleared transient arrays for IV/message
  scratch. Revalidate key initialization when getting length or computing a
  MAC.
- **Validation:** PC, ARM, and RISC-V `symm_crypto` builds passed. The VPCD
  known-answer harness passed all 73 checks (32 cipher, 13 MAC-sign, 26
  MAC-verify APDUs, plus ATR/SELECT). Generator tests pass 9/9 and assert the
  symmetric Java provider/bridge classes are not generated.
- **Harness note:** `build/algtest-v182-empty-flash.bin` was an 80 KiB stale
  image with installed-package bits despite its filename. The VPCD runner
  refused to start without its missing registry sidecar. Use the clean
  512 KiB `build/algtest-v182-current-empty-flash.bin` as the source and a
  unique writable `--flash-copy` for this suite.
- **Limit:** The real token was intentionally unplugged, so this slice has no
  new physical test. RSA Signature and RSA key/cipher provider paths still
  need migration; the asymmetric profile is not yet native-only.


# PC emulator can remain stale after target builds (2026-10-26)

- **Symptom:** A new RSA signature boundary vector (signature representative
  equal to the modulus) returned `6F00` after rebuilding ARM and RISC-V.
- **Cause:** `./tools/build arm` and `./tools/build riscv` do not refresh an
  already-built `build/choupi-pc/choupi`; the VPCD harness was still executing
  an older PC binary. This mimicked a source regression.
- **Viable fix:** After any source changes under PC/VPCD test, run
  `./tools/build-pc --profile <profile>` before launching the VPCD test. Then
  rerun the test through the documented host-access execution path (loopback
  VPCD sockets are restricted in the default sandbox).
- **Validation:** Rebuilt with `./tools/build-pc --profile asymm_crypto`; the
  full VPCD test then passed 26/26, including equal-to-modulus rejection.


# CCID response buffer must fit full APDU response (2026-10-26)

- **Symptom:** On physical Pico 2, RSA SHA-1 signature responses (64 bytes for
  the RSA-512 test key) succeeded, while RSA SHA-2 with a 768-bit key returned
  `6F00` for signing/precomputed signing. Verification calls still completed.
- **Cause:** `src/ccid_device.cpp` allocated 96 bytes for the APDU response.
  The VM correctly produced a 96-byte RSA signature plus the two-byte status
  word, but `choupi/src/main.cpp` rejected the 98-byte result as larger than
  the caller capacity and mapped it to `6F00`. This was a CCID response-capacity
  issue, not a cryptographic algorithm failure.
- **Viable fix:** Keep the CCID receive limit independent; allocate response
  storage that covers the largest supported algorithm result, with the CCID
  header included in transmit storage. The current 1024-byte APDU response
  buffer supports RSA signatures through 4096 bits (512 bytes) and fits in the
  1034-byte CCID transmit buffer.
- **Validation:** ARM/RISC-V `asymm_crypto` builds passed. Physical Pico 2
  `2BED26ED20556535` flashed successfully and passed finite runtime checks; the
  physical RSA suite passed 24/24 vectors across SHA-1/224/256/384/512,
  including complete signatures returned over CCID. Log:
  `artifacts/logs/asymm-crypto-pcsc-20261026T181526Z.log`.

# JCAlgTest static workloads exceeded the applet-call time guard (2026-10-26)

- **Symptom:** Physical JCAlgTest AES CBC-NOPAD preparation and zero-repeat baselines returned `AA9000`, but the 50-call `Cipher.doFinal` performance request returned `6400` after about 6.36 seconds.
- **Cause:** `Interpretor::run` applied a five-second wall-clock limit to every applet method call, although its comment described INSTALL-only protection. Finite crypto performance workloads were incorrectly terminated.
- **Viable fix:** Preserve the five-second INSTALL-method limit and use a hard 15-second limit for normal applet calls. This keeps synchronous applet work bounded while permitting finite JCAlgTest static measurements.
- **Validation:** ARM and RISC-V wrappers passed; a full ARM image cycle passed on Pico 2 `2BED26ED20556535`. On a clean installation of the unchanged upstream JCAlgTest 1.8.2 CAP, AES CBC-NOPAD 50-call `doFinal` returned `AA9000` in 3.067 seconds; DES CBC-NOPAD preparation returned `AA9000`. Runtime serial checks passed.
- **Caveat:** An older token with existing applet state returned `FF01` for AES preparation after the timeout/reflash sequence. A clean install on the second board passed; whether the old token's persistent applet state caused its result is not proven. Use a clean CAP installation when reproducing this regression.

# RSA-PSS known-vector harness must distinguish explicit salt parameters (2026-10-26)

- **Symptom:** RSA-PSS generated signatures passed card-side self-verification, but an externally generated known signature returned false on the card.
- **Cause:** The test applet reserves operation-code bit `0x04` to select the parameterized `Signature.init` overload with an explicit two-byte salt length. A vector using that flag with a digest-length salt was accidentally verified with salt length zero.
- **Viable fix:** Use the default-init operation when the vector salt length equals the digest length; use the parameterized operation only for a non-default salt (including zero). For SHA-512's short-APDU precomputed-hash case, keep the explicit zero-salt operation and prehashed verify code.
- **Validation:** The current VPCD suite passed 59/59 checks. Physical Pico 2 `D92E743D5F640C0F` passed all 39 RSA SHA/PSS vectors, including default and explicit zero-salt verification; the test applet package was removed afterward. See `artifacts/logs/asymm-crypto-pcsc-20261026T185432Z.log`.

# JC305 ECPrivateKey interface methods need token remapping (2026-10-26)

- **Symptom:** A freshly built EC private key reported `isInitialized() == true`,
  while `getField()` correctly threw `UNINITIALIZED_KEY` before any setters ran.
- **Cause:** The JC305 export assigns `ECPrivateKey` class token 18 and orders
  inherited methods as `clearKey`, `getSize`, `getType`, `isInitialized`. The
  embedded ROM API orders them as `isInitialized`, `clearKey`, `getType`,
  `getSize`, and puts `copyDomainParametersFrom` before `setS/getS`. Without an
  explicit interface token map, `isInitialized()` dispatched to `getSize()`;
  its nonzero result made every fresh EC key appear initialized.
- **Viable fix:** Decode the exact upstream export (the kit provides
  `com.sun.javacard.converter.Exp2Text`) and map all 21 JC305 ECPrivateKey
  method tokens to their embedded interface positions in
  `remap_imported_interface_method_token`. Preserve this map when adding more
  EC API calls; check the external export's class/method token order rather
  than assuming it matches the ROM API.
- **Validation:** The independent VPCD suite passes 73/73 checks, including
  fresh/uninitialized state, field getters/setters, domain parameter copy,
  clear, and 256/521-bit persistent/transient key variants. PC, ARM, and
  RISC-V builds pass. Physical validation could not run because the VM
  currently sees only two virtual readers and no Pico USB device.

# JC305 ECPublicKey interface methods also need token remapping (2026-10-26)

- **Symptom:** The first ECDSA VPCD APDU returned `6F00` with an uncaught
  `NullPointerException`, while the EC private-key storage vectors passed.
- **Cause:** JC305 assigns `ECPublicKey` interface token 19 and declares
  `setW/getW` before `copyDomainParametersFrom`; the embedded ROM API puts
  `copyDomainParametersFrom` before `setW/getW`. Calls through the missing
  interface map resolved incorrectly and faulted during public-key setup.
- **Viable fix:** Apply the EC interface method-token map to imported class
  token 19 as well as token 18. Verify method order against the exact JC305
  export file at
  `choupi/javacard-sdk/javacard-api/sdks/jc305u1_kit/api_export_files/javacard/security/javacard/security.jca`.
- **Validation:** After adding the map, VPCD passes all 78 checks, including
  ECDSA sign/verify for SHA-1/224/256/384/512; both architecture wrappers
  build. Physical testing is pending because the host helper currently finds
  zero Pico readers.


## JC305 KeyPair getter token order and qualified constructor JNI name (2026-10-27)

- **Symptom:** Native EC KeyPair generation initially produced keys, but a
  sign/verify probe selected the public key for signing. The ROM-mask JNI
  generator also emitted an invalid C++ identifier for the qualified
  `KeyPair/<init>` method signature.
- **Cause:** The imported JC305 method-token mapping resolves `getPublic()` and
  `getPrivate()` to the opposite generated native selectors compared with the
  embedded API declaration. Separately, constructor normalization only handled
  the exact method name `<init>`, not a fully qualified name containing it.
- **Viable fix:** Keep Java Card API behavior stable by returning the key fields
  from the native selectors according to the verified imported CAP token mapping;
  test both returned key types through actual ECDSA sign/verify. In ROM-mask
  generation, normalize `<init>` anywhere in the qualified name to `init` for
  both declaration and dispatch generation.
- **Validation:** VPCD passed five EC KeyPair generation and ECDSA sign/verify
  vectors (P-192/224/256/384/521); PC, ARM, and RISC-V wrapper builds passed.


## EC subgroup order can exceed the field bit width (2026-10-27)

- **Symptom:** Native secp160r1 KeyPair generation returned CryptoException
  reason 5 (`ILLEGAL_USE`) after Mbed TLS generation.
- **Cause:** The subgroup order `n` for secp160r1 has 161 bits and serializes to
  21 bytes although the field size is 160 bits (20 bytes). The native key
  component buffer for `R` and the KeyPair serializer both assumed field-width
  storage, so writing `n` at 20 bytes failed.
- **Viable fix:** Allocate EC `R` storage with one byte beyond field width,
  serialize `R` at `(bitlen(n)+7)/8`, and allow the native ECDSA/agreement
  domain validation to read that encoding while still checking its exact value
  against the selected curve.
- **Validation:** VPCD generation and ECDSA sign/verify pass for both secp128r1
  and secp160r1, as well as P-192/224/256/384/521; the full VPCD suite passed
  93/93. ARM and RISC-V wrappers passed.

## Native RSA-CRT KeyPair keys were not usable by private Cipher/Signature (2026-10-27)

- **Symptom:** `KeyPair.ALG_RSA_CRT` generation and the five component getters
  passed, but native `Signature.init` and `Cipher.init` accepted only ordinary
  RSA private key types. The advertised CRT key support therefore did not
  provide working private operations.
- **Cause:** Java Card `RSAPrivateCrtKey` exposes P, Q, DP1, DQ1, and PQ, not a
  private exponent D. The native private-operation dispatch only read the
  ordinary RSA modulus/exponent slots, and generated CRT keys unnecessarily
  cached N in a private-only field.
- **Viable fix:** Recognize persistent and transient CRT key types. Compute
  private results with `m1 = x^DP1 mod P`, `m2 = x^DQ1 mod Q`, then
  `m = m2 + Q * (((m1 - m2) * PQ) mod P)`. Derive N from P×Q for CRT Cipher
  and Signature operations, and verify P×Q matches N before exponentiation.
  Cipher state packs the five components into its existing native scratch
  array; no Java API provider or new API field is needed.
- **Security audit correction (2026-10-27):** The bundled Mbed TLS
  `mbedtls_mpi_exp_mod()` selects its secret-exponent path. The exponentiation
  core processes the allocated exponent width and uses a constant-time table
  lookup. This establishes the core's constant-time handling of the exponent,
  not the complete RSA operation. CRT private operations now add a fresh
  224-bit exponent blinder: `DP' = DP + r(P-1)` and
  `DQ' = DQ + r(Q-1)`. They import the blinded exponents at `N`-width plus the
  fixed blinder width, so loop width does not follow private exponent values
  or factor encodings. On RP2350 the entropy callback uses Pico SDK `get_rand_32`;
  the pinned SDK enables its TRNG entropy source by default when RP2350 TRNG is
  available, mixed into its PRNG. This is exponent blinding only. The complete
  CRT operation remains unaudited; it does not blind the input or verify the
  private result with a public exponent, so CRT fault resistance remains
  unresolved. Hardware validation of the random/blinded path is still needed.
- **Validation:** VPCD passed 99/99 checks, including RSA-CRT 512- and 1024-bit
  sign/verify and public-encrypt/private-decrypt round trips, plus 3072-bit
  generation. PC, ARM, and RISC-V wrapper builds passed. The VM saw no physical
  Pico or BOOTSEL device, so hardware verification remains pending.

- **Hardening follow-up (2026-10-27):** Use independent random 224-bit
  exponent blinders for the P and Q exponentiation branches. A shared blinder
  could correlate the two branch traces; independent values avoid that direct
  correlation. This does not provide input blinding or a public-exponent
  result check, and is not a complete side-channel or fault-resistance audit.
  VPCD passes 99/99, and ARM/RISC-V wrappers pass; physical validation remains
  pending when a board is visible to the VM.

## CAP component framing accepted truncated or oversized lengths (2026-10-27)

- **Symptom:** The CAP parser read a component's tag and 16-bit length without
  first checking that the three-byte component header was present. It then
  advanced by the declared payload length without confirming the payload fit
  the supplied CAP buffer. Malformed or truncated CAP data could therefore
  cause out-of-bounds reads.
- **Viable fix:** Before each component view, require at least three bytes,
  decode the payload size from the two input bytes, and require that size to be
  no greater than the remaining bytes after the header. Reject null pointers
  when the supplied length is nonzero. Keep the empty null input case valid for
  existing callers.
- **Validation:** The ASan/UBSan test corpus accepted all 48 upstream deploy
  CAP component streams, rejected truncated and oversized component frames,
  and processed 100,000 randomized input buffers without sanitizer findings.
  The VPCD GPPro empty-card install/select integration also passed. This checks
  outer framing; variable-length payload structures still need deeper checks.

## CAP component payload tables need internal count bounds (2026-10-27)

- **Symptom:** A component's outer frame can fit the CAP while a variable-length
  table inside it declares more AIDs, handlers, array values, or exports than
  the component payload contains. Some later component accessors trust these
  counts.
- **Viable fix:** Before storing component views, validate Header package AID
  and name lengths; Directory custom entries; Applet and Import AID records;
  Constant Pool count; Method exception-handler extent; Static Field initializer
  records and values; Reference Location table extents; Export records; and
  minimum Descriptor fixed-prefix space. Use explicit byte decoding and
  remaining-length checks before every count-based advance.
- **Validation:** ASan/UBSan corpus accepts 48 supplied deploy CAPs, rejects
  targeted impossible counts and outer-frame truncations, and processes
  100,000 randomized byte strings. VPCD GPPro install/select passes. Class
  component and nested Descriptor record traversal remain unresolved, so this
  is partial structural validation rather than a complete CAP verifier.

## Compact Descriptor records require seven-byte field entries (2026-10-27)

- **Symptom:** Descriptor parsing checked only a fixed prefix, and the packed
  `field_descriptor_info` declared its access-flags member as two bytes. This
  shifted subsequent fields and method records from compact CAP offsets.
- **Cause:** A compact field descriptor contains a one-byte token, one-byte
  access flags, three-byte field reference, and two-byte type (seven bytes).
  Counted class/interface records, field descriptors, method descriptors, and
  trailing type-descriptor records were not walked before retaining the view.
- **Viable fix:** Decode Descriptor counts explicitly and bound records using
  compact sizes: nine-byte class prefix, two-byte interface reference,
  seven-byte field, and twelve-byte method. Then bound the constant-pool type
  offsets and each length-prefixed type descriptor. Keep `access_flags` one
  byte. Do not infer Class component offsets from Descriptor class references
  until the format/version-specific layout is independently verified.
- **Validation:** ASan/UBSan passes 4,147 checks across all 48 supplied deploy
  CAPs and 100,000 randomized buffers, including synthetic Descriptor records
  and impossible interface/field/method counts. ARM and RISC-V wrappers and
  VPCD GPPro LOAD/INSTALL/SELECT pass. No physical Pico enumerated, so hardware
  validation was not performed.

## CAP 2.1 Class records are variable-length and ordered by kind (2026-10-27)

- **Symptom:** The Class component payload had no nested-bound validation even
  though later VM accessors trust its class/interface counts and method tables.
- **Cause:** Format 2.1 stores all interface records first, distinguished by
  `ACC_INTERFACE` in the high nibble, followed by variable-length class
  records. A class includes public/package method tables and length-counted
  implemented-interface method maps. Treating the component as an opaque blob
  leaves later accessors vulnerable to forged counts.
- **Viable fix:** For CAP 2.1, walk leading interface records, then compact
  class records; bound each superinterface reference list, fixed class prefix,
  public/package method table, implemented-interface record, and method map
  against remaining component bytes. Keep CAP 2.2+ signature pools, remote
  metadata, and CAP 2.3 extended records out of this parser until their own
  format-specific traversal is implemented. Do not resolve class references
  from Descriptor into Class offsets based on inferred layout alone.
- **Validation:** ASan/UBSan passes 4,154 checks on all 48 supplied deploy CAPs
  plus 100,000 random buffers. Synthetic interface/class payloads and
  impossible method/interface counts are rejected. ARM and RISC-V wrappers and
  VPCD GPPro LOAD/INSTALL/list pass. No physical Pico is visible to the VM.
