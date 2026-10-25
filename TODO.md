# Project TODO

This tracker follows the acceptance criteria in `plan.md`. “Done” means the
behavior has evidence recorded in `progress.md`; partial results are called
out explicitly.

## Current prioritized backlog — 2026-10-20

The checklist and status notes below contain historical snapshots and can be
stale. This section is the current source of truth for open work. Missing
algorithms and JCAlgTest `NO_SUCH_ALGORITHM` results are expected unless CHOUPI
claims to support them; implementing every algorithm is not a goal.

Runtime representation (verified 2026-10-24): Java Card API `.java` sources
are compiled on the host to `.class`, converted to JCA, then converted by the
ROM-mask generator into CAP components. The Pico JCVM interprets Java Card CAP
bytecode; the host `.class` files are not embedded or executed on-device. New
cryptographic algorithms must still be implemented natively in C++ and
dispatched through the Java Card API. Do not add Java crypto providers or
adapters, and do not change upstream applet CAP files.

### Priority 1 — selectable Java Card firmware capabilities

Make `config/javacard-support.json` the only user-edited capability selection
for a build. Its 15 booleans must correspond one-to-one with the current
JCAlgTest v1.8.2 declared support rows: `OWNER_PIN`, `OWNER_PIN_X`,
`OWNER_PIN_X_WITH_PREDECREMENT`; AES-128/192/256 keys; DES, two-key 3DES, and
three-key 3DES keys; SHA-256; secure random; AES CBC-NOPAD; DES CBC-NOPAD; and
the AES/DES CBC-NOPAD `Cipher.getInstance(cipher,padding,externalAccess)` paths.
Validate JSON keys and boolean values, reject unknown or conflicting
capabilities, and require an enabled compatible key type for each selected
cipher.

- [x] Add the single JSON support file, schema/validation, and generated
  capability header. Default it to the current 15 declared yes results.
- [x] Move optional algorithm implementations and OwnerPINBuilder variants
  out of public API classes into separate provider sources. Keep public API
  signatures stable; generate factory dispatch and include only selected
  providers in the ROM mask so disabled bytecode is absent.
- [x] Extract AES CBC, DES CBC, SHA-256, and random-data native implementations
  from `choupi/src/jni.cpp` into separately selectable native modules. Use the
  same selection for PC and Pico. Keep the AES primitive required by SCP03 even
  when Java Card AES capabilities are disabled, and omit disabled modules and
  exclusive libraries.
- [x] Regenerate the ROM mask and JNI header whenever the JSON changes. Check
  that API exports, CAP imports, native tokens, and token mappings stay valid.
- [x] Generate the expected JCAlgTest support CSV from the same JSON. Update
  `tools/compare-jcalgtest-support.py` to accept an empty expected set and fail
  for unexpected yes, missing yes, or any result outside `yes`/`no`.
- [ ] Finish the `/deploy` physical acceptance matrix for all three capability
  profiles on identified Pico 2 `D92E743D5F640C0F`. Completed so far: current,
  no-AES, and minimal ARM/RISC-V wrappers passed; each ARM cycle passed finite
  monitoring; upstream JCAlgTest CAP (unchanged) completed the current-profile
  4,525-row report and matched 15 yes results; `/deploy` Core ran on all three,
  Security and Crypto on no-AES, and Security on minimal. Per user direction,
  minimal is not tested with JCAlgTest; the unchanged upstream CAP returns
  `F103` on no-AES startup because it eagerly requests disabled crypto. No
  modified JCAlgTest CAP is valid evidence. The missing Core/Security/Crypto
  area runs are now complete on all three profiles using unchanged 3.0.5 CAPs.
  Current Security: 901 calls (77 OK, 822 `UNSUPPORTED_PER_SPEC`, two
  transient-key follow-up `WRONG_RESULT` rows whose setup was unsupported);
  current Crypto: 466 calls (22 OK, 444 `UNSUPPORTED_PER_SPEC`). After the
  `checkcast` fix, current and no-AES Core each completed 291 calls (272 OK,
  15 `UNSUPPORTED_PER_SPEC`, four optional Ext `APPLET_NOT_INSTALLED`) with
  every OwnerPINx row OK. Minimal Crypto installed and completed 466 calls
  (4 OK, 462 `UNSUPPORTED_PER_SPEC`) with cleanup. Its earlier `6F00` INSTALL
  failure was not reproduced after Core/Security/Crypto cleanup. The no-AES
  JCAlgTest startup limitation and user's instruction to skip JCAlgTest on
  minimal remain. These are API results, independent of support comparison.
- [x] Compare the `symm_crypto` generated support manifest with the official
  JCAlgTest `ALG_SUPPORT_EXTENDED` CSV. A real D92 run completed all 4,525 rows
  with 73 `yes`, 4,452 `no`, and no other statuses. The CSV contains legacy
  `javacard.crypto.Signature` names and alternate `getInstance` factory rows;
  the comparator now canonicalizes these aliases to the corresponding selected
  capability identities while keeping the configured CBC `getInstance` flags
  independent. Comparison passes with 46 expected/actual capability identities,
  zero unexpected and zero missing. This is support metadata only; unsupported
  algorithms remain expected. A fresh physical D92 run on 2026-10-25 repeated
  the full 4,525-row `ALG_SUPPORT_EXTENDED` sweep after the latest flash and
  explicit unchanged-CAP installation: 73 `yes`, 4,452 `no`, and zero other
  CHOUPI statuses; the manifest comparison passed again (46/46). CSV and logs
  are recorded in `.agent/HANDOFF.md` and `progress.md`.
- [x] Ensure the current physical D92 card is empty before leaving the
  symmetric-profile test session: GPPro deleted applet AID
  `4A43416C675465737431` and package AID `4A43416C6754657374`; a subsequent
  `gp --list` showed only ISD `A000000151000000`. Firmware reflashing alone
  does not erase installed CAPs.
- [x] Rebuild and flash `symm_crypto` to physical D92 `D92E743D5F640C0F`;
  both ARM and RISC-V wrapper builds passed, UF2 verify reached 100%, runtime
  monitor passed, and a final GPPro list still showed only the ISD. A separate
  BOOTSEL board `8E464F4E2BBC0A19` accepted a verified image write but its
  runtime serial did not reappear in this VM, so it has no runtime pass.
  Continue on D92 or another identified reader rather than attributing results
  to 8E.
- [x] Prepare and validate a named empty-card `symm_crypto` image on Pico 2
  `2BED26ED20556535`. ARM and RISC-V wrapper builds passed; the verified ARM
  UF2 completed its 18-second monitor with repeated `TEST:PASS choupi_runtime`
  and `TEST:SUMMARY pass=1 fail=0`. GPPro initially listed persistent JCAlgTest
  and JavacardxSec packages from earlier explicit installs. After deleting
  those applets and packages, `--list` returned only ISD
  `A000000151000000`. This confirms firmware upload does not clear the card's
  persistent CAP store. UF2: `artifacts/firmware/pico2-jcvm-symm_crypto-empty-card.uf2`,
  1,676,800 bytes, SHA-256
  `44962baba1fa0e494bc1ed660566a3100396f6304f8dc2286536248967f55c22`.
  The first BOOTSEL target 8E accepted a verified write but disappeared before
  runtime serial/PCSC enumeration; 2BED was used as the verified failover.
- [x] Run the upstream `/deploy` Core, Security, and Crypto 3.0.5 areas on
  physical D92 with the `symm_crypto` firmware. Security completed 901 calls
  (97 OK, 804 `UNSUPPORTED_PER_SPEC`); Crypto completed 466 calls (161 OK,
  305 `UNSUPPORTED_PER_SPEC`, with the reader-risky RSA-2048 keygen excluded).
  Core completed 291 calls and cleanup passed. The JCSystem subset is 37 OK and
  one spec-permitted `requestObjectDeletion()` ILLEGAL_USE; there are no
  JCSystem wrong-result/runtime-error rows. Remaining Core findings are 17
  OwnerPINx `EXC_RUNTIME`, two OwnerPINx `WRONG_RESULT`, and four Ext-scenario
  `APPLET_NOT_INSTALLED` rows after its scenario CAP INSTALL returned 6A88.
  All `/deploy` CAPs were cleaned up; GPPro lists only the ISD. CSVs are in
  `artifacts/results/deploy/symm_crypto-D92-20261025/`.
- [x] Fix and physically verify the OwnerPINx Core regression. The root cause
  was ambiguous JC305 `checkcast` token resolution: a framework interface token
  could also resolve to an unrelated class export with the same byte value.
  Resolve class and interface export candidates independently and try the
  interface candidate when the class candidate does not match. VPCD regression
  passed 84/84, including positive and negative casts. Physical D92 Core now
  reports 272 OK, 15 `UNSUPPORTED_PER_SPEC`, four optional Ext
  `APPLET_NOT_INSTALLED`, and no runtime/wrong-result outcomes; all OwnerPINx
  rows pass. CAP cleanup succeeded and GPPro lists only ISD. Source commit:
  `fa15bfa`; evidence in `artifacts/results/deploy/symm_crypto-D92-ownerpinx-fixed-20261025.csv`.
- [x] Complete the remaining `/deploy` 3.0.5 area checks on `symm_crypto`:
  `JavacardxSec` passed 84/84; `Service` completed 124 calls (78 OK, 35
  `EXC_RUNTIME`, 9 `SW_ERROR`, 2 `WRONG_RESULT`). `JavacardxExt`, `JavacardxInt`,
  and `Biometry` each loaded the CAP but INSTALL returned `6A88`; the runner
  rolled each back. Per the deploy README, `JavacardxSec` substitutes for
  `Javacardx` on this 3.0.5 card without framework packages. The service
  failures are optional API work, not missing crypto algorithms.
- [x] Build and inspect all three capability profiles on physical Pico 2
  `D92E743D5F640C0F`: `current` (15 enabled), `no_aes` (10), and `minimal` (0).
  Build and finite-monitor results are recorded above; this checkbox covers
  compilation, flashing, monitoring, and image inspection only.
- [x] Inspect profile build outputs. Active ROM-mask sizes are 36,574 / 36,169 /
  32,491 bytes for current / no-AES / minimal; each stored ROM-mask container
  is 524,300 bytes. Firmware `.bin` sizes are 829,116 / 828,132 / 816,348
  bytes, and UF2 sizes are 1,658,880 / 1,656,832 / 1,633,280 bytes. Build logs
  show the AES-CBC module absent from no-AES and all five optional native modules
  absent from minimal, for both PC and Pico; SCP03's common AES primitive stays
  linked. Details and artifacts are recorded in `progress.md` and
  `.agent/HANDOFF.md`.

Assumptions: minimal disables every selectable capability, including optional
PIN variants, while core Java Card, card management, and public API entry points
remain. Preserve unrelated worktree changes. Validate and commit each logical
step with an appended `progress.md` entry and an updated `.agent/HANDOFF.md`.

### Priority 1 — close acceptance criteria AC1–AC5

- [x] Keep the physical card state empty until the requested GPPro install.
  JCAlgTest is not embedded in the ARM UF2 or ROM mask. A successful earlier
  `gp --install` wrote the CAP to persistent card storage, which survives a
  firmware upload; reflashing alone does not uninstall packages. The fresh
  blank-flash VPCD flow now proves `gp --list` shows only the ISD before
  `gp --install`, after which the unchanged applet becomes selectable. The
  symmetric-only Windows guide/archive now deletes only the known JCAlgTest
  applet/package, verifies no application or package remains, and then runs an
  explicit `gp --install`. On 2026-10-25, D92
  `D92E743D5F640C0F` was physically flashed with the 1,675,776-byte
  `symm_crypto` UF2, and GPPro verified only ISD before explicitly installing
  the unchanged upstream CAP. GPPro then listed the JCAlgTest package as
  `LOADED` and applet as `SELECTABLE`. This physical sequence proves that the
  firmware image does not preload the applet; reflashing preserves any CAPs
  previously written to persistent storage. Evidence:
  `artifacts/logs/gppro-empty-install-D92-20261025.log` and
  `artifacts/logs/cycle-choupi-20261025T013632Z.log`. The external Windows
  GPPro workflow remains unverified.

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

- [ ] **Make CAP installation and filesystem updates recover atomically.** CAP
  metadata uses alternating copy-on-write headers, and install now commits a
  CRC-protected intent record alongside applet persistent state before
  publishing the CAP instance registry. `restoreInstalled()` replays pending
  records idempotently and then clears them transactionally. Unit storage tests
  pass. A VPCD fault-injection test now skips registry publication, leaves the
  journal pending, exits the emulator, and verifies recovery plus SELECT in a
  fresh process. Another restart case makes the CAP instance store return an
  error after filesystem commit and verifies recovery. A third leaves the
  journal after successful publication and confirms idempotent recovery. A new
  VPCD case fails journal-clear transaction sync after publication, verifies
  rollback preserves the journal, then verifies fresh-process recovery and
  applet SELECT. This covers the host transaction rollback path; simulated
  RP2350 overlay testing now also cuts power at 1,000 randomized points while
  committing the install-journal clear; reboot recovers either the intact
  prior journal or the fully cleared state. A second fake-flash case injects
  1,000 cuts while creating the journal and applet-state records; reboot sees
  both complete or neither. A combined case persists the journal, injects
  1,000 cuts during CAP instance-header publication, and verifies recovery
  republishes the instance and clears the journal. These are simulated cuts;
  full physical power-cut testing remains open. See
  `.agent/QKB.md`.
- [x] **Verify authenticated GlobalPlatform DELETE and reinstall lifecycle.**
  GPPro 20.01.23 completed authenticated package DELETE, CAP reinstall, GET
  STATUS listing, and post-process-restart listing over host VPCD. The restarted
  scratch emulator reported both the package as loaded and its applet as
  selectable. Full persistent registry enumeration and physical CCID behavior
  remain open; see the card-manager section below.
- [ ] **Complete the selected GlobalPlatform GET DATA / GET STATUS profile.**
  GPPro `--info` confirms Card Recognition Data (`0066`) is returned correctly;
  CPLC (`9F7F`), Card Capabilities (`0067`), Key Info Template (`00E0`), and
  unknown data objects return `6A88`, which GPPro accepts as unavailable. There
  is no manufacturing data source for CPLC. Add required data objects only
  when backed by actual card data. Authenticated GET STATUS first/next pairs
  (`P2=02/03`) now pass for application, package, and module categories with
  one entry each. The host VPCD storage shim now indexes multiple packages and
  applet instances and assembles GET STATUS pages across the registry. An
  authenticated raw VPCD regression now loads the unchanged upstream Core and
  Multi CAPs, installs three instances across two packages, checks GET STATUS,
  rejects an unknown AID with `6A82`, and selects all three instances. The
  latest current-source regression also verifies instance DELETE, package
  DELETE refusal while instances remain, cascade deletion, and an empty final
  registry (`TEST:SUMMARY pass=13 fail=0`). It also saves and closes the first
  emulator process, then starts a fresh process from the saved VM image with
  two packages and three applet records. The explicit `--restore-applet` mode
  remains available; the follow-up `--resume` mode now discovers these records
  from its sidecar. GET STATUS and SELECT pass for all restored records; the
  complete lifecycle reports `TEST:SUMMARY pass=18 fail=0`.
  Host VPCD now writes an atomic checksummed CAP-registry sidecar alongside
  `--memory` on clean save. `--resume` validates the sidecar against the image
  checksum and package bitmap, then reconstructs CAP bytes and applet/class
  records automatically; the fresh-process regression passes all 18 checks.
  Corrupt/missing sidecars fail closed when dynamic packages are present; the
  regression now mutates the sidecar CRC and removes the sidecar from copied
  installed-package images, confirming both rejection paths.
  Host PC/SC GPPro
  multi-package testing remains unverified because this VM has no `Virtual
  PCD` reader registered with PC/SC; physical CCID multi-package testing did
  pass on 2BED as documented in the card-manager section below.
- [ ] **Broaden garbage-collector root and persistence-alias coverage.** The
  physical static workload passed 2,262 transfers (heap stabilized at 83 array
  slots / 28 instance slots; one pass reclaimed 50 arrays). The focused
  transaction-probe applet verifies static-reference identity, persistent
  array-child identity, object-field aliasing/mutation, and restored array type
  across fresh-process restoration (`./tools/test-vpcd-persistence`, 7/7).
  `./tools/test-rmi-registry` covers exported/native-held reference root
  enumeration. The transaction probe now runs eight VPCD rounds of 256
  short-lived arrays and 256 objects per batch, then checks static-root identity,
  persistent array/object aliases, and restored array type (54/54 focused
  checks). The converter-built RMI fixture now exports a child, allocates 256
  short-lived arrays in another APDU with `PICO2_HEAP_GC=1`, observes all 256
  reclaimed, and successfully invokes the child afterward. The process-restart
  persistence wrapper also enables GC and passes four 256-array/256-instance
  pressure rounds (31/31) while checking the restored persistent graph.
  Lazy restoration is now covered for a flash-backed byte array after its RAM
  object is reclaimed: the probe clears one of two static aliases, confirms GC
  reclaimed one array, then reads the other alias and verifies the restored
  value (`tools/test-vpcd-persistence`, 33/33 restore/stress checks). The
  persistent array body now has a stable tag separate from either reference
  field. Added a nested persistent graph through an object field containing an
  `Object[]` with two aliases to a byte array; process-restart restore and four
  GC-pressure rounds passed 38/38 checks. Physical power-cycle validation of
  these alias scenarios remains open.
- [x] **Expose all three Pico 2 devices through PC/SC.** The installed helper
  granted all three USB nodes, and `pcsc_scan -r` listed readers
  D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. A three-worker
  Core/Crypto/Service deploy run completed with cleanup on every card. This
  verifies current multi-reader visibility and use; repeated hotplug timing
  remains a separate characterization task.
- [x] **Validate CAP imports before applet execution.** INSTALL resolves each
  imported AID/version before applet execution and returns `6A88` for an absent
  dependency. A stale dynamic-package bitmap bit previously sent lookup into an
  empty CAP slot and produced `6F00`; `isPackageExist()` now also verifies that
  dynamic package IDs have a nonempty stored CAP. Unit tests cover stale bit,
  present CAP, and clear bit cases; install-status tests cover stage 10/`6A88`.
  VPCD still correctly rejects the deploy `JavacardxExt` CAP because
  `javacardx.external` (`A0000000620203`) is not implemented in the ROM profile.
  Physical validation of the changed firmware remains pending under the ARM-only
  build constraint; do not add that package implementation as part of import
  validation.

### Priority 3 — define and extend the VM profile

- [ ] **Complete JC305 Service framework behavior.** Current-source VPCD
  matches all 33 official `Dispatcher` cases, all 39 `BasicService` cases, and
  all 124 deploy Service API cases. `CardRemoteObject` now exports objects into
  a VM-owned, GC-rooted per-applet registry with stable 16-bit IDs, unexport,
  and 32 bounded slots; ARM and host builds pass. The 16-object catalog case
  initially exposed that an applet's existing reference consumes capacity, so
  the table keeps headroom above the specified minimum. The current catalog
  validates capacity/export/unexport calls; a focused host regression also
  verifies registry root enumeration and session retention/cleanup, but it does
  not force the collector to reclaim neighboring objects. `beginSelectionSession()`
  activates the staged INS and retains the initial object; it now restores the
  initial object's transient export registration after applet restore.
  `RMIService.processCommand()` recognizes applet SELECT and returns FCI with
  class-name or interface-name reference descriptors. Current-source VPCD
  tests load the converter-emitted CAP 2.2 fixture and match both exact SELECT
  FCI responses. SELECT P2 `0x10` is accepted by VPCD and CCID applet
  selection. Primitive INVOKE now parses the method hash and CAP 2.2 signature,
  decodes byte/boolean/short/int arguments, performs virtual dispatch, and
  encodes primitive results. The generated `Counter.add(short)` and
  `Counter.value()` fixture passes exact VPCD responses, as do unknown-method
  and unknown-object error cases. Primitive getter persistence uses the restored
  instance field cache. Array INVOKE now decodes boolean[], byte[], and
  short[] inputs by value into temporary global arrays and encodes array and
  null-array results by value. A converter-built CAP 2.2 fixture passes byte,
  short, and boolean cases, including eight array parameters in one method,
  empty/null arrays, truncated and oversized declared int[] payload rejection,
  and invalid boolean values. It also
  checks locally caught and uncaught exceptions and exact RMI API exception
  responses for bounds, null, `SystemException.ILLEGAL_VALUE`, and
  `ISOException(0x6A82)`. Uncaught exception dispatch is bounded at the nested
  invocation frame. Remote-interface return references now encode both class
  and interface forms, and the fixture successfully invokes the returned child
  object. Int[] remote INVOKE is implemented behind `JCVM_INT_SUPPORTED` and
  passes VPCD end-to-end coverage using the bundled capgen assembler on
  Converter-generated JCA. The fixture checks the assembled CAP for both int[]
  methods. Physical int[] INVOKE was verified on Pico 2 D92 after current ARM
  firmware was flashed; the temporary fixture was removed and JCAlgTest
  remained installed. Custom `RuntimeException` subclass responses
  now return CAP 2.2 tag `0x83`, using the nearest API exception type; the
  fixture verifies exact response `830900009000`. Runtime and checked custom
  exceptions now each pass through an additional applet-defined superclass and
  still map to `830900009000` and `830500009000`. `SystemException.throwIt()`
  with reason `0x1234` returns exact response `822512349000` on VPCD and
  physical D92. Reason capture also passes for CardException,
  CardRuntimeException, APDUException, PINException, TransactionException,
  and UserException on VPCD, through the official SDK proxy, and on physical
  D92. CryptoException reason `0x1234` also returns exact response
  `823012349000` on VPCD, the official SDK client, and physical D92; its class
  is resolved from the separate `javacard.security` package. A custom applet
  CryptoException subclass, with two applet-defined classes before the API
  parent, returns `833012349000` with its inherited reason on VPCD, the
  official SDK client, and physical D92. The proxy decodes
  each tested exception as its proper API class. The
  ISOException special-case export token was corrected from CardException's
  token 18 to ISOException's token 21. Broader hierarchy edge cases remain
  open. The bundled SDK 2.2.1 proxy selects the applet with its unmodified
  `Le=7F`, invokes primitive `add(short)`, and decodes runtime, checked, and
  framework exception responses through current-source VPCD. The checked
  subclass is surfaced as `UndeclaredThrowableException` because the remote
  method declares only the concrete subclass while the protocol reports its
  nearest exported parent.
  Object/reference arguments are prohibited by the Java Card RMI profile (JCRE
  3.1 §8.1.1.1); array parameters are passed by value and must be supported.
  The CAP `Class` format itself
  carries remote method hashes, class names, hash modifiers, and remote
  interface class references for remote classes (CAP format 2.2+); this is not
  Debug-component metadata. The parser now reads the CAP 2.2+ signature-pool
  prefix and remote interface
  names. Package-name, signature-pool, interface-name, class-name, method-hash,
  signature-offset, and remote-interface-reference parsing pass against exact
  Header/Class component bytes emitted by the JCDK 3.0.5 converter for a small
  remote-enabled CAP 2.2 package. Malformed signature-pool bounds are rejected.
  The converter-built fixture is loaded and exercised through current-source
  VPCD for SELECT, primitive INVOKE, primitive-array INVOKE, and protocol-error
  responses. The supplied Service CAP is format 2.1 and cannot validate the
  CAP 2.2 remote metadata path. External JCRMI interoperability passed the
  tested SDK 2.2.1 subset; broader client and RMI profile coverage remains
  open.
  Physical Service results are from existing firmware and do not validate
  current source.

- [x] **Run deploy API areas on three physical Pico readers concurrently.**
  Three-reader JC305 batches ran against existing board firmware. The latest
  full nine-area batch is `artifacts/results/deploy/parallel-20261022-053706/`:
  Security completed 901 calls (63 OK, 820 expected unsupported, 13 runtime
  exceptions, two wrong results, one status error, two excluded calls); Crypto
  completed 466 calls (11 OK, 434 expected unsupported, 16 runtime exceptions,
  three card-runtime exceptions, two status errors); Service completed 124
  calls (94 OK, 28 wrong results, one status error, one runtime exception);
  Core completed 291 calls (271 OK, 15 expected unsupported, four optional
  applet calls unavailable, one wrong result). Cleanup passed for completed
  areas. Optional Core Ext and Javacardx, JavacardxExt, JavacardxSec,
  JavacardxInt, and Biometry installs returned `6A88` for missing imports;
  each partial CAP was rolled back. Physical runs validate installed firmware,
  not current source. No firmware was flashed.

- [ ] **Complete deploy-folder API coverage on the Pico 2.** Runtime loading
  accepts arbitrary package AIDs and assigns IDs 56–63. The latest physical
  Core 3.0.5 run completed 291 calls (272 OK, 15 allowed unsupported, and
  four optional Ext methods unavailable because that CAP was refused with
  `6A88`). It recorded 38 JCSystem rows: 37 `OK`; `requestObjectDeletion()`
  returned profile-allowed `UNSUPPORTED_PER_SPEC`/`ILLEGAL_USE`. Current
  ARM and host builds pass. Fresh VPCD select-only smoke checks pass for
  Security, Crypto, JavacardxExt, JavacardxSec, Service, Biometry, and each
  Multi AID independently. Core's own-AID Shareable probe returns `0000`, and
  its Shareable tagging-interface probe returns `000101`, both with `9000`.
  These emulator checks do not model concurrent packages or applet instances.
  The current-source image was physically flashed and checked on 2026-10-22
  after ARM and RISC-V builds passed. Current-source Security (901 calls),
  Core (291 calls), and Crypto (466 calls) results are recorded below and in
  the newest `progress.md` entries. The optional Ext INSTALL refusal still
  leaves four Core Ext methods unavailable. Earlier physical package deletion failures (`6581`) and
  Javacardx/JavacardxInt/Biometry load/install limitations still need their
  respective follow-up. Focused host Security CAP testing fixed the JC305
  `Key` interface method order mismatch. Six persistent DES/AES
  `KeyBuilder.buildKey` cases now return `9000` with matching 64/128/192-bit
  DES and 128/192/256-bit AES sizes, and were physically confirmed in the
  current-source Security area (D92). Latest source also passes both
  architecture builds;
  temporary virtual-dispatch traces were removed. The official Core client
  completed its 291-call VPCD summary with 0 `WRONG_RESULT`; its CSV contains
  291 detail rows (272 OK, 11 allowed unsupported, 8 scenario entries
  `APPLET_NOT_INSTALLED`). Count both `javacard.` and `javacardx.` row prefixes.
  All 14 APDUException rows passed, including `IO_ERROR`; the earlier apparent
  IO_ERROR disconnect was the 900-second run timeout. Current host VPCD cannot
  install Multi and Ext scenario CAPs alongside Core, so those eight scenario
  calls remain uncovered by this run. Both Multi AIDs select independently in
  fresh VPCD sessions. Physical current-source Core coverage later passed 272
  calls, with four Ext methods unavailable after INSTALL returned `6A88`; the
  Multi and Core packages cleaned up successfully.
  A single-token physical Security 3.0.5 run completed all 901 calls and
  cleaned up its CAP: 822 allowed unsupported, 43 OK, 15 `SW_ERROR`, 13
  `EXC_RUNTIME`, and 8 `WRONG_RESULT`. This used the firmware already flashed
  on the board, so compare its failed Key/AES/DES/MessageDigest rows against
  current-source VPCD before changing code. The Java client needed reader index
  3 because two empty Virtual PCD terminals precede the Pico. CSV:
  `artifacts/results/deploy/pico2-single-security-reader3-20261021.csv`.
  The Crypto 3.0.5 stage also completed on the physical Pico: 466 calls, 444
  allowed unsupported, 14 OK, six `EXC_CARDRUNTIME`, and two `SW_ERROR`;
  upload and CAP cleanup succeeded. RSA-2048 key-pair generation was excluded
  per the deploy instructions. The `6F00` constructor rows and Cipher
  `ILLEGAL_VALUE` rows need comparison with current-source VPCD.
  Artifact: `artifacts/results/deploy/pico2-single-crypto-reader3-20261021.csv`.
  Current-source VPCD has now completed all 901 Security calls after fixing
  the JC305 `MessageDigest()` subclass-constructor token. Official decoding
  reports 64 `OK`, 822 spec-allowed unsupported outcomes, 13 runtime
  exceptions, and two result mismatches; no firmware was flashed. Keep
  `UNSUPPORTED_PER_SPEC` separate from correctness defects.
  A three-reader physical API sweep completed on 2026-10-21, distributing
  Security to D92, Crypto/Service to E0E, and Core plus optional areas to
  5CFC. Security completed 901 calls (820 allowed unsupported, 43 OK, 15
  SW_ERROR, 13 EXC_RUNTIME, 8 WRONG_RESULT, 2 skipped). Crypto completed 466
  calls (434 allowed unsupported, 11 OK, 16 EXC_RUNTIME, 3 EXC_CARDRUNTIME,
  2 SW_ERROR); Service completed 124 (94 OK, 28 WRONG_RESULT, 1 SW_ERROR,
  1 EXC_RUNTIME). Core completed 291 (272 OK, 15 allowed unsupported, four
  optional Ext scenario calls not run because that CAP's missing import was
  rejected with 6A88). JavacardxSec, Javacardx, JavacardxExt, JavacardxInt,
  and Biometry CAPs were refused with 6A88 for absent imported packages.
  Ordinary CAP upload failures were rolled back. The first Core-scenario
  runner left Ext's partial LOAD behind; that package was removed manually,
  then the scenario upload path was fixed and its rollback passed on Pico 3.
  An isolated Core repeat consistently reported `Applet.deselect()` as one
  WRONG_RESULT; investigate before marking Core coverage complete. These
  physical runs used the firmware already on each board, not a new build from
  this turn. Results: `artifacts/results/deploy/parallel-20261021-204343/`,
  `artifacts/results/deploy/pico3-core-scenario-rollback-20261021.csv`, and
  `artifacts/results/deploy/pico3-core-repeat-20261021.csv`. See `.agent/QKB.md`
  and the handoff for cleanup, reader mapping, and limits. A focused
  three-reader Core/Security/Crypto rerun completed all areas and cleanup.
  Core again showed one `Applet.deselect()` counter mismatch (239 selects vs.
  98 deselects). CCID `IccPowerOn` and USB bus reset now clear selected applet
  state and `CLEAR_ON_RESET` arrays. After an ARM flash to Pico 3, two clean
  Core reruns each completed 291 calls with `select()` and `deselect()` both
  returning `-19` and no wrong results. Results:
  `artifacts/results/deploy/pico3-core-resetfix-20261021.csv` and
  `artifacts/results/deploy/pico3-core-resetfix-repeat-20261021.csv`.
  On 2026-10-21, all remaining physical deploy areas were exercised across
  the three readers. Security completed 901 calls (820 allowed unsupported,
  63 OK, 13 runtime exceptions, two skipped, two wrong results, one SW_ERROR)
  and cleaned up. Javacardx, JavacardxSec, JavacardxInt, and Biometry returned
  `6A88` and rolled back. JavacardxExt returned `6F00` twice on D92 and `6A88`
  on the other cards, with rollback each time. The D92 response is consistent
  with stale dynamic package bitmap state on older firmware, but is unconfirmed.
  A fresh current-source host VPCD install and SELECT of `AlgTestApiSecurity_305`
  also passed (`9000`). These physical tests used already-installed firmware,
  not current source. Results:
  `artifacts/results/deploy/parallel-20261021-223749/`,
  `parallel-20261021-224015/`, and `parallel-20261021-224114/`.
- [x] **Allow CAP static initializers past offset 255.** The 3.0.5
  Javacardx CAP has 164 static slots; slots 128–163 use byte offsets above
  255. Added a 16-bit static tag path while retaining the legacy 8-bit key
  format for lower offsets. Current-source VPCD install and SELECT pass
  (`TEST:SUMMARY pass=2 fail=0` after correcting the select-only counter), and
  both host and ARM builds pass.
- [x] **Run deploy API areas concurrently on three Picos.** Added exact
  `--reader-name` resolution and `tools/run-deploy-parallel`, which maps each
  physical Pico to its own GPPro reader, Java index, result CSV, and log. It
  requires three active PC/SC Pico readers by default and excludes the known
  RSA-2048 reader-drop case. On 2026-10-21 it found all three named CHOUPI
  readers and completed parallel runs for all assigned areas. Per-card results
  and stage failures are recorded above. Repeatable three-way API execution is
  established. After the CCID reset fix was flashed to all three boards, the
  full API 3.0.5 suite completed again at
  `artifacts/results/deploy/parallel-20261021-211910/`. Core had no wrong
  results or runtime/status errors; the JCSystem rows were 37 OK and one
  spec-allowed unsupported. Security, Crypto, and Service API issues remain
  recorded in their CSVs; optional imported-package CAP failures rolled back.
  A later three-reader refresh completed Crypto (466 calls), Core (291), and
  Service (124) in parallel with cleanup. This installed image again showed
  one Core `Applet.deselect()` mismatch and 28 Service wrong results, one
  runtime exception, and one status error. A second batch attempted the other
  five areas: Javacardx/JavacardxInt returned `6F00` at INSTALL and
  JavacardxExt/JavacardxSec/Biometry returned `6A88`; every partial LOAD was
  rolled back. These are old-image physical results, not current-source
  validation. CSVs and hashes are in `progress.md` under
  `three-reader-deploy-api-current-firmware`.
- [x] **Verify physical coexistence of independently loaded packages and
  applets.** On the firmware already installed on reader
  `E0E14525E64AF6ED`, retained the 3.0.5 Core CAP while loading Crypto. GPPro
  listed both packages as `LOADED` and both applets as `SELECTABLE`; the
  official client then completed Core (291 calls) and Crypto (466 calls) with
  both resident. Both test packages were deleted afterward and GPPro showed
  only the ISD. This confirms coexistence on the installed image, not the
  current source build; current-source hardware validation remains subject to
  the ARM flash gate.
- [x] **Verify the current-source `JavacardxSec` profile.** Its CAP Import
  component lists `A000000062020901` and `A0000000620205` in addition to the
  core runtime/framework packages. Current source now recognizes these
  canonical AIDs as aliases for CHOUPI's local `javacardx/apdu/util` and
  `javacardx/security` packages, and maps the shifted static method tokens.
  The deploy CAP installs in host VPCD and the official 3.0.5 deploy client
  passes all 84 calls against current source using
  `./tools/test-javacardxsec-vpcd`. The suite covers APDUUtil and
  SensitiveResult. Earlier physical firmware returned `6A88`; no current-source
  image was flashed. This is not a dependency on the separate Security test
  CAP.
- [ ] **Complete general SensitiveResult coverage.** The result slot is VM-wide
  and the same-context assertions pass. Add explicit visibility coverage
  across a Shareable Interface Object context switch, then implement and test
  remaining sensitive result producers outside the JavacardxSec deploy suite.
- [x] **Fix remaining Core API failures found by physical deploy coverage.**
  Same-array overlapping-range behavior in `Util.arrayCopy` and
  `arrayCopyNonAtomic` and `OwnerPIN`/`PIN` dispatch pass on the physical Pico.
  The earlier physical Core run still has three `SW_ERROR` rows and two runtime
  exceptions; the current evidence identifies the runtime exceptions as
  Shareable calls. The host VPCD now passes both Shareable probes after fixing
  virtual-token translation for calls from the embedded ROM into dynamic applet
  overrides. OwnerPIN `getValidatedFlag` now also passes before and after a
  correct PIN check, and the official Core VPCD run reports zero wrong results.
  The fix has ARM-build and VPCD evidence only; it has not been flashed to
  hardware at that time. Two post-flash physical Core 3.0.5 runs on Pico 3
  each completed 291 calls with 272 OK, 15 allowed unsupported, and four
  optional Ext scenario calls not run; there were no `WRONG_RESULT`,
  `SW_ERROR`, or `EXC_*` rows. They confirm the reset-state fix and the
  Shareable/OwnerPIN dispatch changes. Preserve the artifacts under
  `artifacts/results/deploy/`. Optional Ext coverage still needs its imported
  packages, and logical channels remain unsupported by design.
- [x] **Fix package deletion after applet tests.** GPPro DELETE returned `6581`
  for Core and Service packages after persistent objects have been created,
  while deleting packages with no installed instance succeeds. Source review
  found applet cleanup rejected 16-bit CAP class indexes above 255. High class
  indexes now use two-byte persistent-field tags, can read legacy one-byte
  records, and DELETE clears both tag forms. Host persistence, overlay,
  DELETE, PC build, and ARM build checks pass. The latest three-reader physical
  run also cleaned up Core/Multi, Crypto, and Service CAPs successfully after
  their tests, confirming physical deletion on the current firmware.
- [ ] **Declare the supported Java Card profile and audit its API contract.**
  State the supported Java Card Classic version and APIs, then add applet-level
  behavior tests for each implemented API. Existing tests cover JCSystem,
  APDU, lifecycle, exceptions, and persistence but do not establish broad
  profile compatibility.
- [ ] **Implement multi-applet contexts and firewall semantics if the declared
  profile requires them.** The runtime has one applet context. Same-context
  callback dispatch for the Shareable API now passes focused host VPCD probes,
  but general context switching, cross-applet shareable lookup and ownership
  checks, cross-applet AID lookup, and logical channels are not implemented.
  Assigned channel is basic channel 0.
- [x] **Make commit-capacity reporting consistent with transactions.**
  `beginTransaction` / commit / abort now pass the full 80-check JCSystem VPCD
  suite. Abort advances a rollback generation so live persistent field caches
  reload the restored flash snapshots. The commit budget saturates at
  `0x7FFF`; each distinct persistent snapshot charges its old payload size,
  encoded tag, and three journal-length bytes. A full buffer throws a
  catchable `TransactionException.BUFFER_FULL` before mutating the rejected
  record. Native persistent-transaction tests pass, and the 84-check VPCD
  suite confirms the short and extended getters plus a persistent update
  reducing available capacity before abort. An earlier firmware runtime cycle
  passed, but the exact final source was not flashed: the latest targeted cycle
  could not enter BOOTSEL, and authorized host enumeration showed no Pico.
  Physical transaction and power-cycle behavior remain unverified. A
  large-array VPCD overflow attempt timed out before the APDU completed;
  Java-level catchability of `BUFFER_FULL` on an overflowing applet write is
  not yet end-to-end verified.
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
- A fresh D92 `symm_crypto` run on 2026-10-25 was bounded at 3,600 seconds and
  exited 124 at the configured timeout during DES MAC; do not call it a
  complete suite. Partial output has 3,127 CSV rows / 675 methods, 540
  `NO_SUCH_ALGORITHM`, 67 expected `CANT_BE_MEASURED`, 68 measured outcomes,
  and no unexpected SWs, `6F00`, Java exception, or runtime fault. The complete
  SHA-256 256-byte metrics were 3.48 ms/op (`doFinal`), 4.08 (`update`), and
  5.67 (`reset`). Secure random was 1.74 (`generateData`) and 2.79 (`setSeed`).
  The measured DES and AES `Cipher_init` timings scaled with key size: DES / 2-key
  3DES / 3-key 3DES at 39.0 / 47.8 / 56.0 ms/op, and AES-128/192/256 at
  59.4–60.9 / 68.2–68.8 / 76.7–77.5 ms/op. Follow-up SELECT, GETVERSION, and
  JCSystem info all returned `9000`. That pre-capacity firmware reported
  `getMaxCommitCapacity() = 0B`; the later host VPCD implementation now reports
  and accounts commit capacity, while physical APDU reconfirmation remains
  blocked by the D92 reader timeout.
  Artifacts are in `artifacts/logs/jcalgtest-symm-crypto-static-D92-20261025-final.log`,
  `artifacts/results/jcalgtest-symm-crypto-static-D92-20261025-final/`, and
  `artifacts/logs/jcalgtest-post-static-timeout-D92-20261025.log`.
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

- [x] Add a committed install journal to recover CAP registry publication and
  VM filesystem state together. VPCD process-restart cases cover skipped and
  failed instance publication plus journal-clear sync failure. RP2350 fake-flash
  tests inject 1,000 cuts during journal-plus-applet-state creation, 1,000
  during CAP header publication, and 1,000 during transactional journal clear;
  old/new valid states recover and the instance is published idempotently.
  Actual electrical power-cut validation remains
  open; see the current Priority 2 item above and `.agent/QKB.md`.
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
   max and unused commit capacity are `0x7FFF` outside a transaction and that
   a persistent byte-field update consumes snapshot/header capacity inside a
   transaction before abort. The full probe passes 84/84 on the desktop
   backend. The separate physical live-memory probe confirms Pico
   scalar/extended consistency and nonzero persistent/transient values.
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

JCSystem transaction calls use the existing persistent-write snapshot backend:
begin/commit/abort update transaction depth, and the Java wrappers throw
`TransactionException` for nested begin, commit/abort outside a transaction,
buffer-capacity exhaustion, or rollback failure. `Util.arrayCopy`,
`arrayFill`, and `setShort` start and commit only when needed, and validate
inputs before opening a transaction. Commit capacity saturates at `0x7FFF`,
charges each old record snapshot plus its tag and journal-length metadata, and
the native persistent-transaction tests pass. The full JCSystem VPCD probe
passes 84/84, including the active transaction capacity decrease and abort.
Physical transaction behavior remains unverified because the D92 PC/SC APDU
path times out after a successful flash/runtime cycle. On RP2350, `fs_sync` now defers persistence while
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
  performance APDUs (`TEST:SUMMARY pass=15 fail=0` with corrected counting).
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
- [x] Treat CCID `IccPowerOn` and USB bus reset as Java Card resets: drop
  selected state, clear `CLEAR_ON_RESET` transient arrays, reset secure-channel
  state, and abort any incomplete CAP transfer before restoring installed
  package metadata. ARM and RISC-V builds, host VPCD applet regression, and two
  post-flash physical Core runs pass.
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
- [x] Complete GlobalPlatform APDU routing in the PC VPCD session and validate
  official GPPro 20.01.23 `--list`, package `--delete`, CAP `--install`, and
  post-process-restart registry listing. The session recognizes case-1
  `00 A4 04 00 00` default-ISD SELECT; the PC callback implements SCP03,
  GET STATUS/GET DATA, LOAD/INSTALL, and DELETE. GPPro listed the ISD, package,
  and selectable applet; deleted the package, reinstalled the supplied JC305
  CAP, and listed the applet again. A fresh emulator process then reported the
  package and selectable applet from the same scratch flash image. VPCD session
  regression passes 21/21. This is host VPCD validation, not physical CCID
  validation.
- [ ] Complete remaining GET STATUS / GET DATA behavior, including accurate
  package and applet lifecycle state.
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
  11/11. GPPro authenticated package DELETE, CAP reinstall, GET STATUS listing,
  and process-restart listing now pass. Host VPCD resume supports repeatable
  `--restore-applet` records carrying each package ID, class index, applet AID,
  instance AID, and CAP component image. The fresh-process regression restores
  two packages and three instances, verifies GET STATUS, and successfully
  selects each applet (`18/18` full lifecycle checks). The legacy single-applet
  resume still uses `--applet-aid`, `--package-id`, `--class-index`, and
  `--cap-code`; omitting `--class-index` starts an empty registry. Automatic
  package/class discovery from persisted metadata remains open. GPPro 20.01.23
  fresh-state SCP03, JC305 LOAD/INSTALL, instance DELETE, package DELETE, and
  final ISD-only GET STATUS pass on VPCD. Persistent metadata is not yet
  enumerated from storage. The host VPCD registry now supports multiple
  packages and applet instances in one process. The raw authenticated SCP03
  regression loads upstream Core and Multi CAPs, installs three instances,
  verifies GET STATUS and SELECT routing, then checks instance DELETE, refusal
  to delete a package with live instances, cascade DELETE, and an empty final
  registry (`TEST:SUMMARY pass=13 fail=0`). Host deletion routes prefix erasure
  through the privileged filesystem syscall. A clean two-process PC VPCD
  restart now passes for both
  native `--install` and GPPro LOAD/INSTALL when resumed with the allocated
  package ID (56) and class index (148); GPPro listing plus SELECT, GETVERSION,
  and JCSystem all pass after restart. Automatic VPCD registry discovery is
  implemented; GET DATA coverage beyond GPPro's exercised commands and complete
  multi-entry continuation remain open. Physical CCID GPPro integration is
  now verified on Pico 2
  `2BED26ED20556535`: unchanged upstream Core and Multi API 3.0.5 CAPs loaded,
  three instances were listed and selected, and the Core runner completed 291
  calls (272 OK, 15 `UNSUPPORTED_PER_SPEC`, four optional Ext
  `APPLET_NOT_INSTALLED`). Cleanup removed both test packages and all their
  instances; final GPPro listing returned to the pre-test state (ISD,
  JCAlgTest, and JavacardxSec only). D92
  `D92E743D5F640C0F` enumerated as a reader but GPPro `--list` timed out;
  failover to 2BED succeeded. This does not validate multi-package
  reconstruction across a physical power cycle.
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

## Expanded and required cryptographic profiles

- [ ] Implement and validate the `asymm_crypto` selectable profile against
  `docs/NXP_JCOP4_J3R180_P71_ALGSUPPORT__3b_fa_18_00_ff_10_00_4a_54_61_78_43_6f_72_65_56_31_(provided_by_PetrS).csv`
  (SHA-256 `a335e371a3f4b1bed8c74aac4b3cbe353b623f37a778e94fd14a52d00ee23dcb`).
  Its asymmetric yes set includes RSA PKCS#1/NoPad/OAEP ciphers; RSA SHA-1/SHA-2
  PKCS#1, PSS and ISO9796 signatures; ECDSA SHA-1/SHA-224/256/384/512;
  RSA public/private/CRT keys through 4096-bit construction; EC prime-field
  private keys 128–521 bits; EC key agreement (DH, DHC, PACE-GM, plain and XY);
  and RSA/CRT and EC key generation (RSA through 3072-bit generation, EC
  128/160/192/224/256/384/521-bit generation). Add profile-gated native
  providers, exact expected-support CSV comparison, emulator known-answer
  tests, both architecture builds, image and ROM-mask size checks, then run the
  complete relevant `/deploy` tests on identified physical Pico 2 cards.
  Progress (2026-10-24): SHA-1/224/256/384/512, RSA public/private/CRT key
  storage for all requested sizes, and RSA PKCS#1/NoPad/OAEP ciphers have
  implementations. RSA key component storage now uses native C++ getters and
  setters; the VPCD probe passes 20 harness checks (10 digest, 5 key storage,
  3 cipher vectors plus ATR and SELECT). ARM and RISC-V builds pass. Physical
  RSA ciphers passed 18/18 vectors on Pico 2 `2BED26ED20556535`. Native
  `CryptoException` dispatch is now catchable with explicit reason codes across
  native crypto modules. A physical `/deploy` run found and fixed RSA NoPad
  accepting input equal to the modulus. The final repeated Crypto area had 174
  `OK`, 286 `UNSUPPORTED_PER_SPEC`, 6 expected `EXC_CARDRUNTIME`, and zero
  wrong-result rows; three invalid one-shot signature cases now report reason
  5 as expected. RSA key getters/setters now dispatch to native C++. The
  RSA cipher provider now contains its state fields, constructor, and native
  declarations; key validation, buffering, padding, and modular exponentiation
  dispatch to C++. Support-generator tests pass 8/8, the asymmetric VPCD probe
  passes 20/20, and ARM/RISC-V wrappers pass for both `symm_crypto` and
  `asymm_crypto`. Separate ARM UF2s and expected support CSVs are packaged for
  external Windows testing at
  `build/external-test/pico2-jcvm-windows-test.zip`; Windows PC/SC and standard
  client results remain pending. RSA signatures, EC keys/ECDSA, key agreement,
  key generation, exact full-report comparison, and complete physical deploy
  acceptance remain.
- [ ] Run the packaged Windows workflow in `docs/windows-asymm-symm-test.md`;
  archive each profile's JCAlgTest CSV and compare it with its generated
  expected-support CSV. The upstream CAP is included unchanged.
- [ ] User-requested symmetric-only package is available at
  `build/external-test/symm-crypto-windows-test.zip`; its Windows instructions
  are in `docs/symm-crypto-windows-test.md`. Windows enumeration/client results
  remain pending. The `asymm_crypto` implementation is paused at user request.
- [ ] Add RP2350 TRNG and SHA-256 support, then additional mbedTLS algorithms,
  each with known-answer tests (plan WP8).
- [ ] Complete deferred hardening and robustness work, including malformed CAP
  and APDU fuzzing and interrupted persistent-write recovery (plan WP9).
