# Handoff

## Current session state — 2026-10-11

- The user asked to continue implementation without the physical device for now;
  wait for an explicit return to board testing before running flash, monitor,
  PC/SC, or USB-device commands.
- Root `TODO.md` is the current acceptance-criteria tracker. The installed CAP
  blob and synthetic registry/SELECT path are not evidence that the Java
  constructor, `Applet.register`, or applet `process()` executed.
- The worktree contains preserved, uncommitted JC305 token-remapping and install
  diagnostics in `choupi/src/jc_bytecodes/bc_invoke.cpp`,
  `choupi/src/jc_handlers/jc_class.cpp`, `jc_cp.cpp`, `jc_cp.hpp`,
  `choupi/src/main.cpp`, and `src/choupi_pico2.cpp`. Do not overwrite or discard
  them. `CMakeLists.txt`, `SKILL.md`, and `choupi/src/jni.cpp` have separate
  pre-existing modifications and must remain out of unrelated commits.
- Last known candidate runtime builds passed for ARM and RISC-V after the
  Applet virtual-token high-bit adjustment. No board validation has occurred
  after that change.
- GP INSTALL most recently returned `6A80`. A proposed CP-tag mismatch was not
  established: the CAP begins with CP entry 1 tagged as an instance-field
  reference, but persisted watchdog scratch values are stale and the captured
  PC has not been mapped reliably to this CAP method stream.
- Next: validate the token mappings and method/CP interpretation from local CAP
  and export metadata, fix the install execution path, build both targets, and
  make a focused commit with a progress entry. Physical flash validation is
  pending the user's device.

## Objective

Implement `plan.md` for CHOUPI on Pico 2, following root `AGENTS.md`. The user
has connected the Pico 2 and requested real-board bring-up. Use JCAlgTest
v1.8.2 and preserve the provided source trees.

## Latest continuation

- The imported class offset byte-order fix is on the board and passed both target
  builds, flash, and finite runtime checks. A bounded v1.8.2 GP INSTALL still
  resets the card after the final `84E60C00`. New watchdog instrumentation
  reports stage 26 with packed context `00820301`: resolved package ID 0, raw
  package token `0x82` (external import index 2), class token 3, and interface
  flag 1. Thus the superclass walk is interpreting a framework interface as a
  class; the exact `new` operand/initial allocated class is still unknown.
  Logs: `artifacts/logs/gp-install-import-context-20261011.txt` and
  `artifacts/logs/cdc-import-context-20261011.txt`. Both builds pass and the
  normal serial runtime self-check passes. Trace commit: `1731f4f3da58751d7ffbb2fe8bfecdfcf260c6c1`. Next add a watchdog origin field for
  the allocated class package/index and inspect the `new` operand/field walk.

- Corrected imported exported-class offset byte order in
  `resolveClassref()`. Both ARM and RISC-V wrapper builds passed; the elevated
  flash wrapper reset through CDC 1200 baud, entered BOOTSEL after 2 seconds,
  flashed to 100%, and reported `FLASH:PASS`. The 8-second serial monitor
  passed repeated runtime checks. PC/SC and GlobalPlatformPro listing worked;
  the exact JCAlgTest v1.8.2 CAP LOAD completed (`9000`), but final INSTALL
  `84E60C00` again stalled and reset the device after 16.9 seconds. Captured
  watchdog marker `opcode=8F count=1 stage=17 context=00000003` shows imported
  class lookup now reaches Class component offset 3, beyond the previous bad
  `0x0300` index. No successful INSTALL status or applet SELECT was observed.
  Fix commit: `86b96a3c2aefb14dbf698257271fa0ae90c58f9b`.
  Logs: `artifacts/logs/gp-install-class-offset-endian-fix-20261011.txt`,
  `artifacts/logs/cdc-class-offset-install-capture-20261011.txt`, and
  `artifacts/logs/cdc-class-offset-endian-fix-20261011.txt`. Next add markers
  after `resolveClassref()` dynamic checks, through `classref2class()`, and
  around the superclass field-size return; rebuild both targets, flash, and
  retry once.

- Extended the `new` allocation breadcrumbs into `Heap::addInstance`, the
  `JC_Instance` field array setup, and imported superclass resolution. ARM and
  RISC-V builds passed; ARM UF2
  `353781d080f00aa25da77990dc4eb6868b69dfcb2e00ea8fbe83a82c44d07af9` flashed
  successfully and the 8-second serial monitor passed the normal runtime
  test. GPPro again accepted all CAP LOAD blocks and stalled on INSTALL; after
  recovery, the 5-second monitor had no watchdog marker from this attempt. The
  prior build's saved marker is still
  `pc=101C4EFA opcode=8F count=1 stage=6`, which places the stall inside
  `getInstanceFieldsSize` immediately before `classref2class` resolves the
  superclass. The new image reports resolver stages 11-17 and the imported
  class/token context if a marker survives the next reset. GPPro trace:
  `artifacts/logs/gp-install-superclass-resolution-20261011.txt`; CDC capture:
  `artifacts/logs/cdc-superclass-stage-recovery-20261011.txt`. The CAP remains
  uninstalled. Next resume with the board in application USB, run the bounded
  install once, and inspect the new stage/context; if the watchdog marker is
  again absent, capture serial immediately on its re-enumeration before
  changing/resetting the board.
- The latest diagnostic flash adds a persistent substage marker for `new`. Both
  ARM/RISC-V builds passed, the `f38acbf...` ARM UF2 flashed, and the 8-second
  CDC capture passed the normal runtime test. A bounded v1.8.2 GP INSTALL again
  loaded all CAP blocks (`9000`) then caused the 15-second hardware watchdog to
  reset the board. The reopened CDC capture reported
  `WATCHDOG_RECOVERY ... pc=101C4EFA opcode=8F count=1 stage=2`: the first
  `new` bytecode completed constant-pool class resolution and wedged inside
  `Heap::addInstance`. The board recovered in application mode, but INSTALL
  returned no status word and JCAlgTest remains uninstalled. Logs:
  `artifacts/logs/gp-install-substage-20261011.txt` and
  `artifacts/logs/cdc-install-stage2-recovery-20261011.txt`. Next instrument
  the `JC_Instance` constructor and superclass field-size walk inside
  `Heap::addInstance`; then rebuild both architectures, flash, and retry once
  with CDC capture active.
- Startup polling fix, commit `ca9bba37aeb458c848bcfd85b4e301a3351801e`, is
  now confirmed on hardware. The Pico appeared in RP2350 BOOTSEL (`2e8a:000f`);
  `./tools/env ./tools/flash build/arm/pico2_choupi.uf2` reached 100% and
  returned `FLASH:PASS`. After the application rebooted, USB enumerated as
  `2e8a:0009`, CDC became available, and an 18-second monitor captured repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` with no `FAULT` or
  `TEST:FAIL`. The authorized PC/SC helper restored USB access, `pcsc_scan -r`
  listed `Pico 2 CHOUPI CCID`, and GlobalPlatformPro `--list` connected over
  T=1 and reported the ISD `OP_READY` with no applet instances.
- Retried the exact supplied JCAlgTest v1.8.2 CAP. SCP03 authentication and all
  CAP LOAD blocks returned `9000`; the final authenticated `84E60C00` INSTALL
  then stalled. After about 17 seconds the host got
  `SCARD_E_READER_UNAVAILABLE`; `2e8a:0009`, `/dev/ttyACM0`, and the PC/SC
  reader all disappeared from the VM. Trace:
  `artifacts/logs/gp-install-retry-20261011.txt`, SHA-256
  `6287cb1c09e0855913307e521eddaa48a3cbe60baa2133c3033ef9ac28125f0b`.
  No `9000` response, GET STATUS entry, or SELECT confirmation was received;
  the applet remains uninstalled/unconfirmed. Next: restore BOOTSEL/application
  USB passthrough, then inspect the latest persistent watchdog breadcrumb and
  isolate the runtime operation reached by INSTALL before attempting another
  CAP install.
- Startup enumeration follow-up, commit `ca9bba37aeb458c848bcfd85b4e301a3351801e`:
  disabling the SDK IRQ worker also removed the only TinyUSB polling during
  the early `stdio_usb_connected()` wait. The CHOUPI startup now calls
  `tud_task()` during that wait before testing CDC. ARM and RISC-V wrapper
  builds pass. The candidate `build/arm/pico2_choupi.uf2` (SHA-256
  `e6b0802c22f6a07fe1a908782429c22027ed047666c67d7a331a00d5ffc78f8c`) is not
  yet flashed. The prior image is stuck in its pre-enumeration wait, so no USB
  node is visible; an escalated `tools/flash` call is waiting for BOOTSEL
  reattachment. Next flash this candidate, then capture CDC, confirm CCID stage
  7/PCSC reader discovery, and retry GP INSTALL. No applet install is confirmed.
- Newest fix, commit `a62514564543790ad4d0a5719c81b7d5638c1e1e`: CHOUPI now
  disables Pico SDK's TinyUSB IRQ worker and services `tud_task()` and deferred
  CCID commands serially in the main loop. ARM and RISC-V builds passed. The
  cycle flashed the image, but the monitor could not find a CDC node after USB
  re-enumeration. After the user reinserted the board in BOOTSEL, the repository
  flash wrapper reported 100% and `FLASH:PASS`; the board then disappeared from
  the VM USB bus again, so runtime and CCID behavior are not yet confirmed.
  Last `lsusb` showed only the VirtualBox tablet and root hubs. The preceding
  image showed CCID stage 6 after two requests, indicating responses were
  queued but not completing; the new single-context loop is intended to fix
  that endpoint race and the prior one-second servicing delay. Next: when Pico
  USB is visible in the VM, capture the finite serial log, confirm stage 7 and
  PC/SC reader discovery, then repeat the exact JCAlgTest v1.8.2 GP INSTALL.
  Do not report installation unless GPPro returns `9000` and GET STATUS/SELECT
  confirms the applet.
- Follow-up after the user reattached the Pico: it appeared in BOOTSEL as
  `2e8a:000f`; `picotool info -a` confirmed `boot type: bootsel`, and another
  repository-wrapper flash completed at 100%. Immediately after the requested
  application reboot, `./tools/monitor --seconds 18` found no CDC node and
  elevated `lsusb` showed only the VM's root hubs and tablet. `VBoxManage` is
  absent in this guest, so it cannot capture the application's new `2e8a:0009`
  identity itself. Next required host action is to attach `2e8a:0009` in the
  VirtualBox USB menu; leave the board running CHOUPI. Then continue monitor,
  PC/SC, and GP INSTALL checks.
- Latest install watchdog diagnostic build (`artifacts/logs/cycle-choupi-20261011T145623Z.log`) passed ARM/RISC-V builds, flashed to 100%, and passed the normal CHOUPI runtime test. It adds a five-second interpreter-loop deadline and watchdog scratch breadcrumbs for the last raw INSTALL opcode. GlobalPlatformPro v1.8.2 `--install` still fails after sending INSTALL: the host sees `SCARD_F_COMM_ERROR`, CDC output stops, and pcscd loses the reader although USB remains `2e8a:0009`. Scratch recovery did not print because the event may not have been a watchdog reboot or USB reattachment lost the logger. The cycle wrapper's three picotool attempts and CDC 1200-baud fallback also failed on the stalled image. Latest failed APDU trace is `artifacts/logs/gp-install-watchdog-scratch-20261011-apdu.txt`; latest captured CDC trace is `artifacts/logs/gp-install-watchdog-scratch-20261011.txt`.
- Immediate next action: recover/re-attach the Pico USB device, then query pcscd and capture a fresh finite CDC log. If the breadcrumb is present, map its XIP PC to the CAP/VM handler. Otherwise instrument the CCID callback stages in persistent watchdog scratch and determine why the APDU callback stops servicing USB. Do not report the applet installed; no INSTALL `9000` has been observed.
- The watchdog diagnostic implementation is recorded in `progress.md` under `wp7-install-watchdog-trace`; code commit is `d86489e` and the progress SHA follow-up is `6d708a8`.
- The `wp7-gp-install-method-execution` integration code is committed as `7315ae1` with its progress entry. It wires authenticated INSTALL into the CAP install-method interpreter, implements the zero-valued memory native required by JCAlgTest, and fixes CAP handler-table method-offset translation and method visibility checks. Both architecture builds and the latest CHOUPI cycle passed, but GPPro INSTALL still wedges the CCID path before replying. The unrelated `arrayFindGeneric` signature edit in `choupi/src/jni.cpp` and pre-existing root `CMakeLists.txt` / `SKILL.md` changes remain unstaged and preserved.
- The callback-safe image (`build/arm/pico2_choupi.uf2`, SHA-256 `418c69e4e4c11f82e0866cf98479e9c1e61244e298044a7c1110ee9950900504`) is now flashed. The cycle entered BOOTSEL with CDC 1200-baud reset after picotool reported no reset interface, flashed to 100%, and passed the finite CHOUPI runtime test. Log: `artifacts/logs/cycle-choupi-20261011T152212Z.log`, SHA-256 `4ab97132130d7c775e40dcd5c1a54a79b93542aa215d7d6a88caf7cd22ee4db5`.
- GlobalPlatformPro v1.8.2 authenticated with SCP03 and transferred every CAP LOAD block successfully (`9000`), then sent authenticated `84E60C00` INSTALL. The board did not answer within 60 seconds; GPPro timed out with status 124. With serial writes suppressed, the previous recursive CDC output hypothesis is not sufficient to explain the stall. The reader disappeared afterward although `2e8a:0009` remained enumerated; the authorized PC/SC helper restored ACCESS_GRANTED but `pcsc_scan` still found no reader. `picotool reboot -f -u`, CDC 1200-baud reset, and `usbreset 2e8a:0009` did not recover the firmware. GPPro trace: `artifacts/logs/gp-install-callback-safe-20261011.txt`, SHA-256 `c3c4dbf12611b67c7f8bcfa4a531dececac7d35babe810d01ca188ccf8e13746`.
- Follow-up inspection found that Pico SDK `stdio_usb.c` runs `tud_task()` from a low-priority IRQ, and the CCID `driver_xfer` called `process_message()` inline. Commit `21d7b94` now queues complete CCID messages from that callback and calls `process_message()` via `choupi_ccid_task()` in the main loop. ARM/RISC-V wrapper builds pass; candidate image SHA-256 is `f34f79fce442792fb099e00977521c73e3d03f7892d7f9483cb34e4038016fab`. The new image was not flashed: a full cycle attempt could not enter BOOTSEL after the 45-second wait, three picotool requests, and CDC 1200-baud fallback. Log: `artifacts/logs/cycle-choupi-20261011T153015Z.log`, SHA-256 `6c0412947bb6d39e26210f5ff214ca26bae5709bd3b9e7190978d244e56dde1f`.
- Immediate next action: detach/reattach the Pico USB device in VirtualBox or power-cycle it, then flash the queued-CCID build and repeat the bounded v1.8.2 INSTALL attempt. This architecture change remains unverified on hardware. Do not claim the applet installed; the last INSTALL did not return `9000` and GET STATUS did not confirm an instance. Latest hardware-validation record commit `53adb84`.

- Added composite USB CDC + CCID firmware to the ARM CHOUPI image. The board
  enumerates as `2e8a:0009` with CCID interface 2. After both architecture
  builds passed, `./tools/env ./tools/cycle choupi` flashed the image and the
  runtime emitted `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`.
  Direct libusb CCID IccPowerOn, XfrBlock, and IccPowerOff exchanges succeeded;
  details are in `artifacts/logs/ccid-probe-20261011.txt`. XfrBlock still
  returns a deliberate `6D00` placeholder. There is no GlobalPlatform or Java
  Card APDU dispatch and no runtime CAP install yet. The host has the
  GlobalPlatformPro JAR artifacts, but `pcscd`/`pcsc_scan` are not installed.
- Next: inspect the CHOUPI VM entry points for APDU dispatch and whether it can
  instantiate/select the ROM-masked AlgTest applet. Wire CCID XfrBlock through
  that path before attempting a real GPPro install; preserve the current static
  ROM-mask install as a working fallback.
- CCID transport step committed as `ffa3ddf`.
- APDU lifecycle audit is in `artifacts/logs/apdu-lifecycle-audit-20261011.txt`.
  The current boot path runs a single static starter, the interpreter's
  non-static entry is a TODO, `APDU.getBuffer()` returns null, and applet
  registration/selection is absent. GlobalPlatformPro `--list` exits with
  `SCARD_E_NO_SERVICE`; only libpcsclite is installed, not pcscd. Next work is
  WP3: implement APDU buffer + applet lifecycle dispatch and identify a
  non-root PC/SC daemon/driver route for host validation.
- The APDU lifecycle audit is committed as `fea69ed`.
- A user-local PC/SC daemon attempt is recorded in
  `artifacts/logs/pcsc-user-local-20261011.txt`. `pcscd`/`libccid` were
  downloaded and extracted under `/tmp`, but unprivileged pcscd cannot create
  `/run/pcscd` (`Permission denied`). No system packages were installed and no
  daemon is running. Continue firmware work; retry GPPro once an unprivileged
  or system PC/SC socket is available.
- The user-local PC/SC experiment is committed as `43059dc`.
- After the CCID VID/PID plist arrays were aligned, the next PC/SC scan still
  found no reader. Direct inspection showed the Pico absent from the VM USB bus
  entirely (only root hubs and VirtualBox tablet in `lsusb`; no 2e8a sysfs
  device; picotool saw no RP-series device). See
  `artifacts/logs/pcsc-usb-absent-20261011.txt`. Reattach the Pico in VirtualBox
  and verify `lsusb -d 2e8a:0009` before repeating `pcsc_scan -r`.
- This passthrough diagnosis is committed as `f9efb4a`.

- Improved `tools/flash` to wait up to 120 seconds for an RP-series USB device.
  VirtualBox had stale USB passthrough until the user toggled the device off and
  on; afterward sysfs showed `2e8a:000f` RP2350 BOOTSEL and picotool flashed the
  watchdog candidate to 100%. When the VM stalled, the watchdog reset into ROM
  BOOTSEL; `picotool info -a` confirmed boot type `bootsel`.
- Added a bounded trace of instructions after `putfield_a`, which showed opcode
  `0xAD` (`getfield_a_this`) requesting repeated 36,870-byte persistent-array
  buffers. Persistent non-transient arrays use flash for element I/O, so their
  backing storage now keeps only the tag metadata; transient arrays retain full
  buffers. The subsequent dual-build CHOUPI cycle passed and emitted
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. Latest log:
  `artifacts/logs/cycle-choupi-20261011T090019Z.log` (SHA-256
  `a093b8d548205e6a11dba34954509d6667fc9947cf14c18116f6f92a217f8cf7`); flashed
  UF2 SHA-256 `48f170eb971f3e9f33505eeb0d40934eced68ad8b46c6f80a31248ad8741e3cb`.
- The rebuilt JCAlgTest v1.8.2 CAP archive has a different outer ZIP hash, but
  all 10 Java Card `.cap` component files match the supplied archive byte for
  byte, including `Method.cap`. The current ARM UF2 embeds the ROM-mask image
  generated from those JCA components, and the Pico's CDC output repeatedly
  reports `TEST:PASS choupi_runtime`. This is a static ROM-mask installation,
  not a GlobalPlatform APDU install. The firmware still has no APDU/CCID
  transport or runtime CAP installer.

## Current state

- The root repository tracks only the plan and RP2350 references at the
  starting revision. `choupi/` and `AlgTest_JavaCard/` are user-provided,
  untracked source trees; preserve them and commit only files belonging to a
  completed step.
- The CHOUPI C++ VM source is present. The OS matches pinned revision
  `654f374067b2e7c1571fec1410adcd1e41a60c96`; the ROM mask matches
  `ac9f75253d476a8b2a838d459727df6e7a4a5107`. The Java Card API starts from
  pinned revision `d490faf358d4740def8eaf60017d5cc367f7383e` with local patch
  commit `d570ce3` aligning Framework, Security, and Crypto package export
  metadata with JCAlgTest v1.8.2 imports. They remain nested Git checkouts
  inside the user-provided untracked `choupi/` tree. The OS's
  STM32CubeF4 submodule is not initialized; it is specific to the old STM32
  target.
- The user selected JCAlgTest v1.8.2. `AlgTest_JavaCard/src305` identifies
  itself as v1.8.2 and the matching `AlgTest_v1.8.2_jc305.cap` is present.
  Read-only Java Card SDK CapDump inspection confirms CAP format 2.1, package
  AID `4A43416C6754657374`, applet AID `4A43416C675465737431`, four imports
  (`A0000000620001` v1.0; `A0000000620102`, `A0000000620101`, and
  `A0000000620201` v1.6), size 131,538 bytes, SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  Generated API JCA packages match these identifiers and versions. The v1.8.2
  `src305` Java sources also convert to JCA with those imports; the ROM-mask
  generator now succeeds with all 21 API/applet JCA packages after nested
  ROM-mask parser fix `af1091315fd7fd17baeae773ecf7553fd785ae37`. Its
  Makefile's removed Maven assembly goal was corrected in nested commit
  `f2a3f0e8eb945e3fa027f359be78844cf1306f42`, and
  `./tools/env make -C choupi/javacard-sdk/rommask` now passes. The resulting
  source-derived image is `build/choupi-rommask/flash-algtest.bin` (currently
  524,300 bytes, SHA-256
  `967a883ebf87955c74af84265218685414f82eaf584cab7e60651d719d11eda3`). The
  rebuilt CAP archive hash differs from the supplied archive hash, but a
  component-wise comparison confirms all 10 `.cap` entries match byte for
  byte. ROMMask adapts the package for CHOUPI storage; the current UF2 includes
  this ROM-mask image.
  Filtered CapDump output: `artifacts/logs/capdump-v182-20261010.txt`.
- The root Pico build now has a `choupi_vm_core` object target that cross
  compiles all 34 platform-independent CHOUPI C++ translation units for both
  RP2350 ISA builds. `tools/build` stages the generated source-derived JCA
  header and requires it before claiming a successful CHOUPI-core build.
  Successful wrapper builds still produce only the `pico2_trng` UF2; the VM
  core is not linked, no Rust OS implementation is part of the image, and no
  APDU/CAP installation route exists yet. The object build suppresses four
  legacy diagnostic classes from upstream sources (`ignored-qualifiers`,
  `address-of-packed-member`, `unused-parameter`, and `array-bounds`); this
  needs review before treating the build as warning-clean.
- A repository-local toolchain is installed under `.local-toolchain/`; use
  `./tools/env <command>` to select it. It includes CMake/Ninja, ARM and
  RISC-V compilers, stable Rust with the Cortex-M33 target, Pico SDK 2.3.1,
  pioasm, and picotool. See `.agent/TOOLCHAIN.md` for versions and hashes.
  Setup and the hardware report are committed as `db99639`.
- The board was initially identified in BOOTSEL mode. Read-only
  `picotool info -a` reported RP2350 A4, QFN60, 2048 KiB flash, and an
  existing RISC-V image named `pico2_btc_miner`. Before replacing it, saved a
  read-only 2 MiB backup at
  `artifacts/backups/flash-before-smoke-20261010.bin`, SHA-256
  `49a650c2093df2e2d528ec4ef9adce788db95133c7303b5f96d852d8b5ecc078`.
  Flash size remains a device report pending independent physical confirmation.
- Added the root Pico SDK smoke target and `tools/build`, `tools/cycle`, and
  `tools/monitor`. The final `./tools/env ./tools/cycle arm` passed: both ARM
  and RISC-V built with warnings as errors, picotool reported the expected UF2
  families, and the board emitted repeated `TEST:PASS random_stream_varies`
  and `TEST:SUMMARY pass=1 fail=0` records during the bounded 8-second serial
  capture. The board currently runs this ARM smoke firmware. This check only
  confirms changing output from `get_rand_32()`; it does not assess entropy or
  execute CHOUPI. Final log: `artifacts/logs/cycle-arm-20261010T053019Z.log`.
  Implementation commit: `9bd89df`.
- Maven 3.10.0 and Eclipse Temurin JDK 8u504 are installed under
  `.local-toolchain/`. The Java Card API Ant `build` passes with JDK 8. The
  Maven wrapper is visible through `./tools/env`. `tools/doctor` checks the
  pinned `capmap-free` submodule and JC 3.0.5u3 API kit explicitly. The CAP
  parser is restored from the user-provided GitHub mirror at its exact pinned
  commit; the nested `.gitmodules` URL now points to that mirror, and the
  submodule remote was synchronized. Maven's local repository is redirected
  inside `.local-toolchain/`; required artifacts have been resolved and the
  ROM-mask build passes.
- Diagnostics and continuity setup are committed as `1616b59`.
- Fixed `JCVMArray::fill()` to populate each element and removed the ignored
  return qualifier from `size()`. The focused strict host test passed with
  `TEST:PASS jcvmarray_fill`; committed as `0d293ae`.
- Implemented the `putstatic_a` path and nested persistent reference-array
  element tags in the VM persistence layer. Syntax checks passed using a
  temporary variadic `ffi.h` shim; this does not prove ABI compatibility or
  runtime persistence behavior. The step is committed as `ab1957b`; see the
  latest progress entry.
- Added guards for transient/non-reference persistent arrays and index bounds
  in the child-reference writer. Its syntax check and the JCVM array test both
  pass; runtime persistence has not been verified. Commit: `7e04f97`.
- Implemented Java-object `athrow` table dispatch across current and caller
  frames, saving each frame's opcode address and restoring package context as
  frames unwind. Syntax checks pass, but no runtime tests are possible yet;
  native `Exceptions` mapping and uncaught status-word handling are still open.
  Commit: `476a938`.
- Fixed the typed class/interface matcher to return `FALSE` on non-match; the
  expanded syntax check passes. CAP hierarchy runtime tests remain pending.
  Commit: `6f7f1bc`.
- Implemented the object-deletion capability fallback natives (`FALSE` and
  no-op). Syntax check passes; target builds and runtime native dispatch tests
  remain unavailable. Commit: `512337f`.
- Repaired persistent instance class/tag handling and reference-field get/set.
  Syntax checks pass, but `JC_Instance::getFields()` still cannot reconstruct
  a persistent object's typed field vector; this remains open for WP5. Commit:
  `a740f5e`.
- Fixed persistent instance class-index initialization and reference get/set
  behavior. Syntax passes; filesystem round-trip behavior is not validated.
  Commit: `a05147e`.
- Persistent instances now retain their original filesystem tag separately
  from their field storage, and `getFields()` lazily rebuilds typed field
  values, including split-word ints and reference/array values. Setters also
  keep cached field types and values aligned with serialized records. Syntax
  and the array regression test pass. No persistence round-trip test is
  possible without the filesystem implementation; missing child records still
  cannot be typed from the current class metadata API. Commit: `5263602`.
- The interpreter no longer spins forever after a VM-internal exception.
  `run()` returns `0x6F00` on an uncaught VM exception, including an `athrow`
  with no matching handler, and the C runtime entry returns that status. This
  still does not map VM exceptions to Java objects or send a status over APDU.
  Commit: `ff6a4b8`.
- The `JCSystem` transaction information natives report zero active depth and
  zero commit capacity, matching the runtime's lack of transaction support.
  AlgTest's system-information path queries maximum commit capacity. Mutating
  transaction calls remain unimplemented. Syntax and array regression checks
  pass; target builds remain unavailable. Commit: `53660ca`.
- Fixed `JCVMArray` ownership semantics for mutable copies and moves while
  preserving non-owning read-only CAP views. `JC_Array` now lets its backing
  buffer owner free storage exactly once, allocates the full tagged persistent
  array payload, reports transient element counts correctly, and recognizes
  `CLEAR_ON_RESET`. Focused copy/move and transient-size tests pass; the latter
  uses AddressSanitizer with leak detection disabled due an environment-level
  LeakSanitizer failure. Commit: `6fa7232`.
- Added an RAII active-context binding around interpreter execution, implemented
  `isTransient`, and implemented primitive transient boolean, byte, short, and
  int array factories. Invalid lengths and clear events map to the specified VM
  exceptions. Syntax and array tests pass; target builds remain unavailable.
  Object transient arrays still lack reference-type metadata in their native
  signature, and actual reset/deselect clearing awaits lifecycle hooks. Commit:
  `e56d14f`.
- Added `JC_Array::clearTransientData()` and `Heap::clearTransientArrays(event)`;
  the heap clears only transient arrays registered for the requested event. A
  focused ASan test verifies reset and deselect selection. The clear primitive
  is not yet called by lifecycle dispatch because that dispatch does not exist.
  Commit: `6d3630e`.
- Implemented native primitive array copy for `arrayCopyRepack` and its
  non-atomic counterpart. The path checks ranges and matching array types and
  snapshots input to preserve overlap semantics. Focused ASan tests cover an
  overlapping byte copy plus bounds/type errors. Reference copying remains
  unsupported; transaction atomicity is unavailable. Commit: `8e92ec1`.
- Implemented `arrayCompareGeneric` for byte arrays with range checks, signed
  Java byte comparison, and ternary results. The focused ASan test covers equality,
  signed-byte boundary ordering, and normal less-than ordering. SensitiveResult
  tracking is not implemented. Commit: `92ef957`. A follow-up checked the
  Java Card primitive ordering and corrected the comparison to signed bytes;
  that correction is committed as `e088119`.
- Implemented byte `arrayFillGeneric` and its non-atomic counterpart. The
  helper repeats bytes from the supplied pattern offset and returns the target
  end offset. A focused ASan test covers repetition; compatibility with the
  missing Java Card SDK binding has not been verified. Commit: `bb5d2b8`.
- Expanded the host native test to call the transient byte-array factory,
  `isTransient`, and invalid length/event paths using a live `Context`. The
  linked ASan test passes, closing the earlier native runtime-test gap for
  these paths. Java exception dispatch and actual card lifecycle events remain
  untested. Commit: `64fe919`.
- Expanded factory coverage to boolean, byte, short, and int arrays, including
  both valid clear events and `isTransient(null)`. All pass in the linked ASan
  host test. Object array creation remains unsupported. Commit: `bacd8fc`.
- Implemented `selectingApplet()` as `FALSE` in the current runtime because it
  has no applet selection or pending-select state. The linked ASan test passes;
  this must become stateful with lifecycle dispatch. Commit: `3446422`.
- Implemented `getAssignedChannel()` as channel 0 for the single-channel
  transport. The linked ASan test verifies it; logical channels are not
  supported. Commit: `657e589`.
- Implemented the SensitiveArrays integrity-array capability probe as
  `FALSE`, matching the absence of integrity storage and verification in this
  runtime. The linked ASan test now checks the capability result; operations
  that require integrity arrays remain unsupported. Commit: `32c88dd`.
- Implemented `isIntegritySensitive(Object)` as `FALSE` for non-null objects
  and `NullPointerException` for null, since this runtime never creates or
  tracks sensitive arrays. Focused host tests cover both cases; commit:
  `3ef6da7`.
- The documentation finalization commit `bff7c23` accidentally included 63
  previously untracked provided source files from stale index entries. Their
  worktree copies were hash-verified and retained; corrective commit `ae783a2`
  removes them from the index again. Keep the provided source trees untracked.
- The requested `.agent/SESSION_CONTINUITY.md` was absent and has now been
  created. Root `progress.md` will be append-only.

## Next action

Continue the RP2350 port and determine whether runtime GlobalPlatform
installation is required beyond the verified static ROM-mask deployment. The
current ARM UF2 has been flashed and CHOUPI is running on the Pico 2. All CAP
component payloads from the supplied v1.8.2 archive match the JCA-rebuilt
package byte for byte; ROMMask adapts storage by relocating static fields. CDC
is debug output only, so GPPro cannot install or select packages over this
firmware. Do not claim APDU/CCID functionality. Preserve `.local-toolchain/`,
`AlgTest_JavaCard/`, and the CHOUPI source tree.

## Latest validation

- Current board bring-up: `bash -n tools/common.sh tools/build tools/cycle
  tools/monitor`, ARM build, RISC-V build, and `./tools/env ./tools/cycle arm`
  all passed. The 8-second capture emitted repeated
  `TEST:PASS random_stream_varies` and `TEST:SUMMARY pass=1 fail=0`; log:
  `artifacts/logs/cycle-arm-20261010T053019Z.log`.
- Dependency restoration: OS and ROM-mask checkouts match pinned full commits;
  the API is at its pinned commit plus local patch `d570ce3`. The
  `capmap-free` submodule is at `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4`,
  and its nested URL points to the user-provided GitHub mirror. The doctor and
  ROM-mask Maven build pass. The source-derived ROM mask was generated, but
  CHOUPI firmware compilation and CAP installation remain unavailable.
- `bash -n tools/doctor`: passed. `./tools/env ./tools/doctor` reports all
  host tools, Pico SDK, API kit, and top-level sources present; expected exit
  status is 1 for missing `rommask/capmap-free/pom.xml`.
- Fresh `./tools/env ./tools/monitor --seconds 8` on the connected board
  returned 0 with 8 repetitions of `TEST:PASS random_stream_varies` and
  `TEST:SUMMARY pass=1 fail=0`; no `FAULT` or failure records appeared.
- JCAlgTest v1.8.2 CAP inspection: JC 3.0.5u3 CapDump returned 0. CAP format
  2.1, package/applet AIDs, four imports, size, and SHA-256 are recorded above;
  this exposed three API export mismatches. After patching API build settings,
  successful JCA output has exact matching identifiers and versions. Full
  ROM-mask import resolution remains unverified.
- Java Card API: `JAVA_HOME=$PWD/.local-toolchain/java8` with that JDK's `bin`
  prepended to `PATH`, then `./tools/env ant -f
  choupi/javacard-sdk/javacard-api/build.xml clean build`: `BUILD SUCCESSFUL`;
  Framework, Security, and Crypto generated JCA headers match the CAP imports.
- Latest VM syntax check: `TEST:PASS vm_core_syntax`; existing upstream
  warnings remain.
- Latest `./tools/test-jcvmarray`: `TEST:PASS jcvmarray_fill`.
- `./tools/build arm` and `./tools/build riscv`: wrapper absent; both calls
  returned status 127.
- Latest interpreter exception/status step: `./tools/check-vm-syntax` passed
  with `TEST:PASS vm_core_syntax`; `./tools/test-jcvmarray` passed.
- Latest array storage step: `./tools/test-jcvmarray` passed its fill and
  copy/move checks; `./tools/test-jcarray` passed `TEST:PASS
  jcarray_transient_size` under AddressSanitizer (LeakSanitizer disabled).
- Latest active-context/transient-native step: `./tools/check-vm-syntax`,
  `./tools/test-jcvmarray`, and `./tools/test-jcarray` passed; both target build
  wrapper calls returned 127 because `tools/build` is absent.
- Latest transient-clear primitive step: `./tools/test-jcarray` passed both
  `jcarray_transient_size` and `jcarray_clear_events`; syntax and JCVM array
  tests passed; both target build wrapper calls returned 127.
- Latest native array-copy step: focused ASan tests passed overlap and error
  cases; syntax and JCVM array tests passed; both target build calls returned
  127 because the wrapper is absent.
- Latest native array-compare step: focused ASan comparison checks, including
  signed byte ordering, passed; syntax checks passed; target build wrappers
  returned 127 because the wrapper is absent.
- Latest native array-fill step: focused ASan pattern repetition, syntax checks
  passed; ARM/RISC-V build wrappers returned 127 because the wrapper is absent.
- Latest transient-native runtime test: linked ASan test exercised factory,
  `isTransient`, and invalid argument paths; syntax checks passed. Target build
  wrappers were not rerun because only test files changed.
- Latest primitive-factory coverage: linked ASan test passed all four primitive
  array types and null handling; only test code changed.
- Latest selecting-applet fallback: linked ASan test and syntax check passed;
  ARM and RISC-V build wrappers remain absent.
- Latest basic-channel fallback: linked ASan test and syntax check passed;
  ARM and RISC-V build wrappers remain absent.
- Latest integrity-array capability fallback: linked ASan test passed and
  emitted `TEST:PASS jni_integrity_arrays_capability`; syntax check passed.
  ARM and RISC-V builds could not run because `tools/build` is absent.
- Latest integrity-sensitive query: linked ASan test passed and emitted
  `TEST:PASS jni_integrity_sensitive_query`; syntax check passed. ARM and
  RISC-V builds could not run because `tools/build` is absent.
- Firmware runtime validation is still pending for CHOUPI: the connected board
  runs the standalone smoke firmware and its bounded USB serial test passed.
- Latest physical cycle: `./tools/env ./tools/cycle arm` rebuilt ARM and RISC-V,
  flashed the ARM smoke image, and captured 8 seconds from `/dev/ttyACM0`.
  Output had eight passing `random_stream_varies` checks and eight passing
  summaries; no `TEST:FAIL` or `FAULT` appeared. Log:
  `artifacts/logs/cycle-arm-20261010T060906Z.log` (SHA-256
  `7bccbc03281e0f93c2e4068856903570b648bb44f51f452576162222072b7dd9`). The
  image is still only standalone smoke firmware; CHOUPI/JCAlgTest are not yet
  running on the board.
- Toolchain setup: `bash -n tools/env` passed. `./tools/env rustup target
  list --installed` includes `thumbv8m.main-none-eabihf`; CMake/Ninja, both
  compiler prefixes, and picotool version checks passed. `./tools/env
  ./tools/doctor` now passes after restoring `capmap-free`. Maven 3.10.0 is
  available through `./tools/env`; the API build passes with Temurin 8u504.
  CAP-parser Maven installation and ROM-mask packaging now pass. Root smoke
  wrappers pass for both ISAs. Board queries and
  cycles required reviewed escalation.
- Latest post-mirror board refresh: `./tools/env ./tools/build arm` and
  `./tools/env ./tools/build riscv` both returned 0 and validated their
  respective UF2 families. The 8-second `./tools/env ./tools/monitor
  --seconds 8` returned 0 from `/dev/serial/by-id/usb-Raspberry_Pi_Pico_...`,
  emitting eight `TEST:PASS random_stream_varies` and
  `TEST:SUMMARY pass=1 fail=0` pairs. Archived capture:
  `artifacts/logs/monitor-after-capmap-20261010.log` (SHA-256
  `9f4162fb209ebfb97a74d03517a55fe9d6e279b0354999ba7d7bbe7d6a76b5dd`). The
  board remains on the smoke image; this does not validate CHOUPI or CAP load.
- Previous deployment-readiness attempt found the parser missing and Bitbucket
  requested credentials; CHOUPI PC configuration also failed because Boost
  headers and `program_options` are absent. The user supplied a public GitHub
  mirror; `capmap-free` is now initialized at the exact pinned commit
  `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4`, and `./tools/env
  ./tools/doctor` passes. With reviewed network access, the capmap Maven install
  and ROM-mask generator package both pass.
  Existing CHOUPI supports host/STM32 only; its OS lacks an RP2350
  architecture, and the root Pico SDK target remains the smoke app. USB
  CCID/APDU and on-card CAP installation are absent, so no CHOUPI/CAP flash was
  attempted. See latest `progress.md` record.

## Latest bring-up state (2026-10-10)

- The root now links `pico2_choupi.uf2` for `rp2350-arm-s`. `tools/build` also
  cross-compiles the VM core for `rp2350-riscv`; `tools/cycle choupi` enforces
  both builds before flashing. ARM and RISC-V build gates pass.
- The CHOUPI image contains `build/choupi-rommask/flash-algtest.bin`, generated
  from JCA rebuilt from the v1.8.2 `src305` sources. It is not the exact user
  CAP. The image starts on the Pico 2 and emits CDC logs, but startup fails:
  latest serial log `artifacts/logs/cycle-choupi-20261010T084216Z.log` reports
  `PUTFIELD_A` object `0x0000`, value `0x0007`, `NullPointerException`, and
  `6F00`. The wrapper's `TEST:FAIL choupi_runtime` is expected and must not be
  treated as a pass.
- Source debugging in the user-provided, untracked `choupi/` tree has locally
  fixed field constant-pool token resolution, primitive `NEWARRAY` bytecode to
  internal array-type mapping, and new arrays remaining heap-backed until
  persisted. It also has temporary exception and field-reference traces. These
  edits are not in a root commit; preserve them and never stage the full
  provided source tree. Current lead: inspect starter/app initialization and
  why it dereferences element 0 of a new reference array before assignment.
- `gp`, `pcsc_scan`, and `opensc-tool` are absent. The installed TinyUSB SDK
  provides CDC but no CCID class, and the current firmware has no APDU/card
  manager. The exact supplied CAP
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` remains uninstalled (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`). Do
  not report the source-derived prelinked package as installation of this CAP.
- New root changes for the CHOUPI target and cycle wrapper plus the failed
  board experiment are awaiting the incremental root commit. Recent successful
  board access required reviewed USB escalation; the cycle log is archived.

## Latest startup/storage diagnostics (2026-10-10)

- Added bounded VM exception output and a one-line heartbeat per million
  bytecodes in `choupi/src/interpretor.cpp`. Both ARM and RISC-V builds pass.
- Hardware diagnostic sequence found the earlier `6F00` was an `IOException`
  during `putstatic_a`: the Pico overlay exhausted its 24 file slots while
  persisting CHOUPI startup state. Fixed tables at 48, 128, and 512 slots also
  exhausted. A dynamic `realloc` prototype ran out of memory. Current
  `src/choupi_pico2.cpp` uses 2,048 compact metadata records plus a static
  128 KiB packed byte arena to avoid per-file heap fragmentation.
- The current packed-arena UF2 builds for both architectures. Its board cycle
  did not capture BOOT, heartbeat, or TEST output during 18 seconds; a separate
  60-second monitor was also silent. The last `picotool reboot -u -f` returned
  success, but the host then lost USB enumeration and `usbreset` could not find
  the device. The current image is not hardware-validated. Latest cycle log:
  `artifacts/logs/cycle-choupi-20261010T094700Z.log`; UF2 SHA-256
  `8990a465f60f89b9baa724d1c05856605e77d7224e090f6f16b4a711f172c8b8`.
- The exact CAP remains `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`, SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`, and is
  still not installed. Firmware currently has CDC only, with no APDU/CCID or
  CAP loader. Do not call the embedded source-derived ROM mask an installation.
- Next action: restore Pico USB/BOOTSEL access, capture the sparse interpreter
  heartbeat on the packed-arena image, and continue diagnosing startup before
  adding an exact-CAP installation route. Preserve all pending worktree edits.

## Latest continuation update (2026-10-10)

- User put the Pico 2 into BOOTSEL. `picotool info -a` confirmed RP2350 A4, chip ID `0xd92e743d5f640c0f`, and `boot type: bootsel`.
- The 2,048-record overlay image flashed successfully and confirmed startup still exhausted the overlay at 2,048 files / 10,394 bytes. A runtime null-reference optimization did not relieve the file-count growth.
- Increased overlay metadata capacity to 4,096. Both `./tools/build arm` and `./tools/build riscv` pass. The latest hardware cycle flashed the image and failed at 4,096 files / 20,754 bytes, while writing a six-byte record at tag length 27. The VM reports exception id 5 at opcode `0x7F`, then status `6F00`. Log: `artifacts/logs/cycle-choupi-20261010T095555Z.log`, SHA-256 `5f0fe823120a2db9cfc7fd09598c2a17106942f82df07883a8c6688615fbd9e4`.
- Next action: trace recursive persistence at deeper tags, identify cyclic/shared object expansion, and fix the serialization path rather than keep multiplying overlay slots. Continue requiring both builds before flash. The exact JCAlgTest v1.8.2 CAP is still not installed; firmware has CDC only and lacks an APDU/CCID or CAP loader.
- Preserve all unrelated staged or untracked work. Commit the current storage experiment with this progress record, then continue diagnosis.

## Latest parent-compressed storage update (2026-10-10)

- The Pico filesystem shim now uses a 16,384-entry hash index and 256 KiB
  packed arena. Tags compress repeated field-index byte pairs; most nested
  records refer to the parent tag by a stable index slot. Record updates use
  the stored key encoding when accounting for replaced bytes.
- The required nested OS prototype update is committed as
  `choupi/os` commit `f994d5e` (`Expand persistent filesystem tag buffers`).
- Both `./tools/build arm` and `./tools/build riscv` pass for the current
  source. First hardware capture after parent compression passed previous
  arena/file limits but failed on `SecurityException id=29` while descending
  a 61-byte persistent path (`artifacts/logs/cycle-choupi-20261010T101555Z.log`).
- Root implementation/progress commit: `f2da673` (`Compress nested CHOUPI
  persistence keys`).
- Increasing tag buffers to 128 bytes passed builds, but hardware then
  reported `*** PANIC *** Out of memory` shortly after BOOT
  (`artifacts/logs/cycle-choupi-20261010T101744Z.log`). Instance tags were
  changed to compact repeated-pair encoding in a 68-byte inline buffer with
  overflow storage. Two later cycles returned no CHOUPI serial lines after
  `MONITOR:START` (`101940Z`, `102236Z`). The 8-second independent monitor was
  also silent. Read-only `picotool info -a` sees a USB serial connection but
  reports no accessible BOOTSEL device.
- User has been asked to reconnect the Pico 2 while holding BOOTSEL. Continue
  independent source and continuity work while awaiting that response; do not
  flash until both architecture builds pass. The exact CAP remains
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`)
  and is not installed. Firmware exposes CDC only; no APDU/CAP loader exists.
- Next action: after BOOTSEL re-enumerates, reflash the current compact-tag
  build and capture serial output; shrink the static overlay arena if startup
  still exhausts the available RAM. Preserve unrelated changes and append a
  progress record plus a new commit for each validated step.
- The four latest cycle logs are being archived under `artifacts/logs/`; their
  outcomes are `SecurityException` at tag length 61, `Out of memory`, and two
  silent captures. No passing CHOUPI runtime evidence exists yet.
- The storage implementation is in root commit `f2da673`; the four capture
  files and this progress record are in root commit `87c1bbe`.
- A follow-up guard now rejects odd-length candidates before compressed-pair
  comparisons and prevents uncompressed 128-byte keys from overflowing the
  record header. ARM/RISC-V builds pass. Hardware validation is pending a
  physical BOOTSEL reconnect; current RP2350 is only visible as USB serial.
- The guard is in root commit `9b73e3a`.

## Latest recursive-persistence attempt (2026-10-10)

- User confirmed the Pico 2 was reinserted in BOOTSEL. `picotool info -a`
  identified the RP2350 A4 and confirmed `boot type: bootsel`; the CHOUPI
  image flashed successfully. Serial then showed 16,384 overlay records in use
  at 171,176/262,144 bytes, followed by Java Card exception id 5 and `6F00`.
- The repeated reference `0x0094` at increasing tag depths indicates cyclic
  object serialization. Added a persistent-object alias record to break a
  reference back to an active/canonical object, and mapped alias reads back to
  the target's persistent tag. ARM and RISC-V builds pass.
- A first cycle with a full-tag recursion stack panicked `Out of memory`
  (`artifacts/logs/cycle-choupi-20261010T110210Z.log`). Reduced the tracker to
  pointers and moved the canonical compact tag into each instance. The next
  cycle did not capture runtime output; afterward `picotool info -a` and
  `lsusb` both failed to initialize libusb. Its log
  `artifacts/logs/cycle-choupi-20261010T110427Z.log` is byte-identical to an
  older silent capture, so it does not prove which firmware ran.
- Current UF2 SHA-256 is
  `a0682558fd222e51b7e93b434a46dea99a0887853ef05c73ee6378793e80fef8`.
  Source changes and a failed experiment record are in root commit `654219f`. Preserve
  unrelated changes in the tree.
- Follow-up: `JC_Instance::setPersistentTag` now also marks a newly serialized
  instance persistent so later field updates write through its canonical tag.
  Both target builds pass again; current UF2 SHA-256 is
  `9cc85d9051fb9f55d52682d4f772ea16316d144fb123686381415203bad85fc6`.
  This refinement is in root commit `06fdf87` and has not been run on hardware.
- Next action: reconnect the Pico while holding BOOTSEL, verify it with
  `picotool info -a`, then flash/capture again. If runtime starts, verify that
  aliases reduce startup expansion and that no `TEST:FAIL`, `FAULT`, or timeout
  occurs. The exact CAP remains
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`),
  uninstalled; the current CDC firmware has no APDU/CCID or CAP loader.

## JCAlgTest package compatibility check (2026-10-10)

- Exact requested CAP remains `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`,
  SHA-256 `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
- Its import component requests `java/lang` AID `A0000000620001` v1.0,
  `javacard/framework` AID `A0000000620101` v1.6,
  `javacard/security` AID `A0000000620102` v1.6, and `javacardx/crypto`
  AID `A0000000620201` v1.6. Generated JCA metadata under
  `build/choupi-algtest/rommask-input/` reports matching package IDs and
  versions. This verifies package-description compatibility only; it does not
  prove CHOUPI runtime method resolution or CAP installation support.
- `./tools/env picotool info -a` still fails with `ERROR: Failed to initialise
  libUSB`; no flash was attempted. Host inspection found no GlobalPlatformPro,
  GPShell, PC/SC service, or smart-card reader tools. The current CHOUPI image
  exposes CDC only and has no APDU/CCID transport or CAP loader, so the CAP is
  not installed.
- Resume after the board re-enumerates in BOOTSEL: pass both architecture
  builds, flash/capture CHOUPI and resolve startup failures, then implement or
  establish an APDU and CAP installation path. Do not claim the CAP installed
  until successful on-device installation is observed.

## Latest Pico 2 flash attempt (2026-10-10)

- `./tools/build arm` and `./tools/build riscv` both pass for the current
  source. The normal sandbox cannot initialize libUSB, but elevated read-only
  `picotool info -a` detected the RP2350 as a CDC device running firmware.
- The authorized repository command `./tools/cycle choupi` was run with USB
  device access and rebuilt both targets. `picotool load -f -x` returned zero
  and the cycle proceeded to the serial monitor, but its log has no flash
  progress/completion message. The silent capture therefore does not establish
  whether the UF2 was written or which image is running. Candidate ARM UF2
  SHA-256 is `9cc85d9051fb9f55d52682d4f772ea16316d144fb123686381415203bad85fc6`.
- The 18-second monitor emitted no serial bytes and the cycle exited nonzero
  (`artifacts/logs/cycle-choupi-20261010T111732Z.log`, SHA-256
  `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`). This
  is byte-identical to earlier silent captures. The post-cycle device query
  sees an RP2350 CDC interface but does not confirm BOOTSEL or the running
  image. Do not treat this as a passing runtime test or as evidence of a usable
  JCVM.
- Next investigate early startup/CDC silence (including whether firmware has
  reached the USB log path) before continuing the persistent-alias experiment.
  Both architecture builds must still pass before any additional flash. Once
  CHOUPI runs, it still needs APDU/CCID or another host protocol and a CAP
  loader; the exact JCAlgTest v1.8.2 CAP has not been installed.

## Reduced SRAM arena experiment (2026-10-10)

- Changed the Pico overlay capacity in `src/choupi_pico2.cpp` to 8,192 file
  records and 128 KiB of record bytes. Both architecture builds passed; the
  CHOUPI load command returned zero and the cycle proceeded to capture, but
  the log did not show flash progress/completion and the running image was not
  independently verified.
- Capture `artifacts/logs/cycle-choupi-20261010T112137Z.log` had no serial
  bytes; its SHA-256 is `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`.
  A separate 8-second pyserial capture with DTR asserted was also silent.
- The ARM ELF `.bss` is now 232,860 bytes (was 462,236); `.data` is 9,920
  bytes. An RP2350 CDC device enumerated afterward, but it still emitted no
  BOOT/runtime message and the running image was not verified. More heap did
  not resolve the silence. Candidate UF2 SHA-256 is
  `0a265f608a5c5ea44f90bcb46ced51a308b8d909967ffd8071dd89ef6f6d675d`.
- Next flash the known-good TRNG firmware with the cycle wrapper to confirm
  serial capture, then trace CHOUPI startup before further overlay tuning. Keep
  both architecture build gates before each flash. The exact v1.8.2 CAP is
  still not installed; CHOUPI has no APDU/CAP loading path.

## USB capture/BOOTSEL diagnostic (2026-10-10)

- `./tools/cycle arm` passed both architecture builds and ran the ARM TRNG
  flash command, but its 8-second capture had no `BOOT`/`TEST` output
  (`artifacts/logs/cycle-arm-20261010T112446Z.log`, SHA-256
  `62457db617d9013e2c91a67bf6cfb2a74b8600d7c50b952b091d8dd9fc4e7e13`). This
  is a second independent image with the same silent serial outcome; the image
  currently running is therefore uncertain.
- `picotool info -a` after the cycle finds only the RP2350 CDC interface, not
  BOOTSEL. `picotool reboot -f -u` returned “The device was asked to reboot
  into BOOTSEL mode,” but a follow-up query still found CDC. USB descriptor
  reads also reported `Resource temporarily unavailable` for device status.
- Next required hardware action: physically unplug the Pico 2, hold its
  BOOTSEL button while reconnecting USB, then verify `boot type: bootsel` with
  `./tools/env picotool info -a`. Only then rerun the CHOUPI cycle. Both build
  gates must pass before flashing. The exact CAP remains uninstalled.

## GlobalPlatformPro client prepared (2026-10-10)

- Built the official GlobalPlatformPro `v26.06.04` source (commit
  `5f3f4a2c7f6ae147da1351955deaeec9872eff47`) in the ignored local toolchain.
  Self-contained CLI: `.local-toolchain/globalplatformpro/tool/target/gp.jar`,
  SHA-256 `38b464deabb20f48ba4fa1379354e41e913f635a537c9ea5cd2b1e919e0f908c`.
  `--help` and `--version` both work with Java 21; Maven build succeeded with
  tests skipped.
- No PC/SC socket/service or card reader is present. Current Pico CHOUPI
  firmware provides CDC and does not implement CCID/APDU, so GPPro is ready
  but cannot connect to it. Do not claim CAP installation until there is an
  APDU path and on-device install confirmation.

## BOOTSEL flash confirms CHOUPI heap panic (2026-10-10)

- Physical BOOTSEL was confirmed with `picotool info -a` (RP2350 A4, 2 MB
  flash). The next `./tools/cycle choupi` passed ARM and RISC-V builds, flashed
  the ARM UF2 with visible 100% progress, rebooted it, and captured
  `BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi` followed by Pico SDK
  `*** PANIC *** / Out of memory`. Log:
  `artifacts/logs/cycle-choupi-20261010T115001Z.log` (SHA-256
  `e01b623fc94299571e1f72fde115acf6b0a7e3bbbe853f6ae9bc9954775c2903`).
- Source inspection traced the panic text to `pico_malloc`'s allocation guard.
  Added temporary serial stage markers around `fs_init()` and `runtime()` in
  `src/choupi_pico2.cpp`; ARM and RISC-V builds passed. The following cycle
  could not flash because the board had left BOOTSEL; its monitor repeated the
  previous panic and its log is byte-identical to an earlier silent capture
  (`9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`).
- Current stage-marker UF2 hash: `b49438a577524d767b9c859fa2a0de49475c2bce461f29bc97b5c11b04322c65`.
  `picotool reboot -f -u` also failed to produce a BOOTSEL device. The user
  was asked to hold BOOTSEL while reconnecting USB; verify boot mode before
  the next cycle. The stage-marker image is not yet flashed. After identifying
  the allocation phase, address SRAM pressure and repeat both build gates and
  the real board cycle. CAP installation is still blocked by missing APDU/CCID
  transport and loader support.
- Added Pico SDK allocator diagnostics to the ARM CHOUPI target: it now emits
  failed allocation sizes and allocations crossing `0x20070000`, without
  logging ordinary low-address allocations. ARM and RISC-V wrapper builds
  passed. Diagnostic UF2 SHA-256:
  `19532c092fa19bcc7dd1e43e8d5aa84450755e112a02225ec12b697836f3cc9b`.
  This build has not yet been flashed; BOOTSEL reconnect is still required.

## Heap allocation localized on the Pico 2 (2026-10-10)

- After the user reconnected the board in BOOTSEL, `./tools/cycle choupi`
  passed both ARM/RISC-V build gates, visibly flashed 100%, and captured the
  diagnostic image. Log: `artifacts/logs/cycle-choupi-20261010T133620Z.log`
  (SHA-256
  `9846ef5043fbd4b74f828a4a4da3260b538933db5455d9bf8027c56c7e51a9fb`).
- `fs_init()` completed; the failure occurs after `CHOUPI:STAGE runtime:start`.
  The Pico SDK reports a 36,870-byte allocation succeeding at
  `0x20068FB0..0x20071FB6`, followed by another 36,870-byte allocation failure
  and `*** PANIC *** / Out of memory`. The request call site is not yet known.
- Reduced the overlay byte arena from 128 KiB to 64 KiB (saving exactly
  65,536 bytes of static SRAM) to increase room for the second runtime
  allocation. Both architecture builds passed. New candidate UF2 SHA-256:
  `a590f7b12e9a9ebc66ffa202921e9878cc12b8457d667ed65f29aba97046c9f5`;
  `.bss` ends at `0x2002B56C`, down 65,536 bytes from `0x2003B56C`.
- This reduced-arena image has not yet been flashed. The board is running the
  heap-diagnostic image; verify BOOTSEL after physical reconnection, then run
  `./tools/cycle choupi`. Do not call the runtime a pass unless the cycle
  captures the expected TEST summary. CAP installation remains unimplemented.

## 64 KiB overlay hardware attempt (2026-10-10)

- The user reconnected the Pico in BOOTSEL; `picotool info -a` confirmed RP2350
  A4 and BOOTSEL. The 64 KiB arena candidate passed both architecture build
  gates, flashed with visible 100% progress, and was captured by the monitor.
- It still fails after `runtime:start`: `malloc 16390 2006EFE0->20072FE6`,
  then `malloc 36870 failed to allocate memory`, followed by
  `*** PANIC *** / Out of memory`. Log
  `artifacts/logs/cycle-choupi-20261010T212007Z.log` SHA-256
  `4ea1a3f80e2bb24e5665d4e3e3e178e76fa6cc858a3e20fd5d3d31acee11761c`.
- UF2 hash `a590f7b12e9a9ebc66ffa202921e9878cc12b8457d667ed65f29aba97046c9f5`.
  This cycle was a runtime failure, not a pass. Board is now running CHOUPI;
  re-enter BOOTSEL physically before a subsequent flash. Exact CAP remains
  uninstalled.
- Candidate follow-up: the 32 KiB overlay plus 8-byte index build passes both
  ISA wrappers and produces `build/arm/pico2_choupi.uf2` with SHA-256
  `46f2b453d33ada46fa1cda9e681f18648f53e00ac966b76bc9aa1faf49bda137`.
  `picotool info -a` sees a USB serial app but no BOOTSEL device; a software
  `picotool reboot -f -u` request did not yield BOOTSEL enumeration. This
  candidate has not been flashed. Next action: hold BOOTSEL while reconnecting,
  confirm BOOTSEL, then run `./tools/cycle choupi`. The runtime and CAP install
  remain incomplete.
- A later `./tools/cycle choupi` passed both build gates but produced no serial
  bytes; its log is byte-identical to a prior silent capture. A separate
  20-second capture with the reader attached before `picotool reboot -f -a`
  was also silent. No successful flash or runtime is confirmed. The Pico
  enumerates as USB serial, not BOOTSEL. Next action remains a physical
  BOOTSEL reconnect, verify with `picotool info -a`, and repeat the cycle.
- A direct `picotool reboot -F -u` also reported a BOOTSEL request, but
  subsequent `picotool info -a` still found only the USB serial app (exit 249).
  This software recovery attempt did not flash anything; physical BOOTSEL is
  required for the next try.
- The board was later confirmed in BOOTSEL. The 32 KiB candidate flashed at
  100%, passed both architecture builds, completed `fs_init()`, and no longer
  runs out of memory. Its runtime fails with `VM_EXCEPTION id=5 opcode=0x87`
  (`IOException` in `putfield_a`) at `0x10043C01`; that address maps into
  ROM-mask tag `0114`, data offset `0x1B1`. Log SHA-256 is
  `d2b068c660382e1b828004aea92132bd3f8f0cdccac0086a17369fd3c63d5d2f`.
  Next: instrument `write_at()` rejection causes, rebuild both architectures,
  then rerun `./tools/cycle choupi`. The user-provided v1.8.2 CAP is not yet
  installed; CAP transport/loader work remains after runtime startup.
- Added `CHOUPI:FS_WRITE_REJECT` diagnostics in `write_at()` for missing records,
  out-of-range writes, index exhaustion, or overlay capacity; both architecture
  builds pass. UF2 SHA-256:
  `1de6bce7ed146f0a4a709f3180650305c6d62eb2a902d0eb95d774dd49b88e13`.
  The cycle could not enter BOOTSEL from the running CHOUPI app and captured no
  output, so this image is not yet flashed. Physically hold BOOTSEL while
  reconnecting and verify before the next cycle.
- Added load/store phase exception diagnostics around persistent
  `setField_Reference()`; ARM and RISC-V builds pass. The diagnostic candidate
  UF2 hash is `18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`.
  The board still reports USB serial, not BOOTSEL; flash the candidate after a
  physical BOOTSEL reconnect. CAP installation remains pending.
- User asked to enter BOOTSEL without asking them. Tried `picotool reboot -f`
  and `-F`, direct `picotool load -F`, CDC 1200-baud reset, and direct libusb
  class/interface request to reset interface 2. None changed the live device
  from USB serial (same bus/device 1:116); serial read remained empty. Do not
  claim the diagnostic image was flashed. The current VM appears unresponsive
  after its IOException (inference); the new diagnostics are compiled but await
  a responsive reset path. Continue software analysis without asking the user.

## USB recovery update (2026-10-11)

- Retried `picotool reboot -F -u`; it returned success but the device remained
  in USB serial mode. `usbreset 001/116` then reported `No such device`; the
  Pico disappeared from `lsusb`, and its xHCI root port now reports
  `not attached`. `/dev/ttyACM0` is absent. The per-port xHCI sysfs
  `disable` control is not writable to this account. No flash occurred.
- Keep the built diagnostic UF2 ready (`18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`).
  Continue source analysis; do not claim CHOUPI or JCAlgTest hardware success.

## Persistent field default handling (2026-10-11)

- `JC_Instance::getFields()` now checks `fs_exists()` before trying to decode a
  persistent field type. Missing per-field records are initialized as
  `FIELD_TYPE_UNINITIALIZED` with zero value, matching the existing reference
  getter's null behavior and avoiding an `IOException` for an absent record.
- ARM and RISC-V wrapper builds pass. New ARM UF2 SHA-256:
  `2860767cb505cddcc6c6ec372eeceb4b7d5bd5d0e77026c909ae816294e3de03`.
  The firmware has not been flashed: Pico USB port 3 remains `not attached`.
  Runtime behavior is unverified. On the next enumeration, enter BOOTSEL if
  available, flash through `./tools/cycle choupi`, and inspect the bounded log
  for the persistence phase markers and `TEST:SUMMARY`.

## Latest Pico field-load attempt (2026-10-11)

- The default-field candidate above was flashed from BOOTSEL at visible 100%.
  Both architecture builds passed. Runtime advanced through two reference field
  stores in package 20/class 57 and then stopped producing output after
  `CHOUPI:PUTFIELD_A phase=load index=0 package=20 class=121`. The 18-second
  cycle failed with no `TEST:SUMMARY`; a separate eight-second monitor was
  silent. Log: `artifacts/logs/cycle-choupi-20261011T053229Z.log`, SHA-256
  `909b217b6fa27f343c27dcf7bb3ec4f5c2f2909ca36bf25c2a335ddfeedd2134`.
- `picotool reboot -F -u` again left the app in CDC mode. A USB bus reset then
  removed it from the host bus. No BOOTSEL or serial device is currently
  enumerated.
- A new capped trace in `JC_Instance::getFields()` marks entry/exit of
  `fs_exists`, decoded type, and value completion. ARM and RISC-V wrapper builds
  pass. Candidate UF2 SHA-256:
  `a2deabc677b6672431386a1b0aef3eb05cc6debf906b1f0bc478d19f2b86f543`.
  It has not been flashed; use it after BOOTSEL enumeration returns.
- That trace reached persistent class 121 with a reported field count of 8,912.
  Each of fields 0–63 was processed far enough to complete its existence query;
  no full vector completion marker existed yet. A follow-up candidate adds
  bounded superclass-chain diagnostics to `getInstanceFieldsSize()` for
  package 20/class 121. Both builds pass; its UF2 SHA-256 is
  `5fa53345dbcabadbcaec146dc6aa07bc5e195f61fe320f49d6308e4fa8bedde4` and is
  unflashed. The board is again in CDC mode after the prior cycle.

## USB BOOTSEL retry and class-token investigation (2026-10-11)

- Retried `picotool reboot -F -u` with direct USB access. Picotool reported
  “The device was asked to reboot into BOOTSEL mode,” but the follow-up still
  found only the CDC app at bus 1/address 120 and no BOOTSEL device. No flash
  was attempted; the hierarchy-trace UF2 remains pending.
- Source inspection confirms `getClassInformation()` stores the CAP class
  component byte offset, and `getClassFromClassIndex()` uses that offset
  directly. Its companion `getClassRefFromClassIndex()` instead iterates
  class/interface records by ordinal. This inconsistency can affect persistent
  object class references. Fixed that helper to encode the stored byte offset
  in CAP byte order; both wrapper builds pass. Updated ARM CHOUPI UF2 SHA-256:
  `274b8128c4259b167c52bc4f363e96d88348dd1f9540f655932232d4f4ed568f`.
  The change does not explain the 8,912 field count by itself, since superclass
  traversal uses direct class refs.
- Next: after BOOTSEL is available, flash the corrected image, then collect the
  superclass trace. Do not use repeated USB bus resets; prior host reset
  attempts made the Pico disappear. The exact JCAlgTest CAP is still not
  installed and no APDU/CAP loader exists in the current firmware.

## Forced CHOUPI cycle after class-offset fix (2026-10-11)

- `./tools/env ./tools/cycle choupi` passed both wrapper build gates and invoked
  `picotool load -f -x` on the corrected ARM CHOUPI UF2. Picotool asked the
  device to reboot into BOOTSEL and waited, but the cycle log has no
  `Loading into Flash` progress and no “device was rebooted to start the
  application” confirmation. Treat the image as not flashed.
- The 18-second serial capture was empty, and its log SHA-256
  `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c` matches
  an earlier silent capture. Follow-up enumeration still sees CDC at bus
  1/address 120, not BOOTSEL. A force-mode `picotool info -a -f` also waited
  without yielding image metadata.
- Next: keep the corrected UF2 ready and continue offline class/persistent
  field analysis. No additional USB bus reset; the current app does not
  successfully service the BOOTSEL transition.
- A follow-up `picotool reboot -f -a` returned success, but an eight-second
  monitor capture after the application reboot was silent. Retrying
  `picotool reboot -f -u` then returned success without changing enumeration;
  `picotool info -a` still found CDC at bus 1/address 120 and no BOOTSEL.
  Therefore the application reboot did not recover a software BOOTSEL path.
- The user clarified they had not physically changed the board and asked for a
  reflash. Re-ran `./tools/env ./tools/cycle choupi`: both architecture build
  gates passed, but force-load again waited for BOOTSEL and printed no flash
  progress. The 18-second capture was silent; log SHA-256 again matched the
  prior empty log (`9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`).
  `picotool info -a` still sees only CDC at bus 1/address 120. The updated
  image is not confirmed flashed.

## BOOTSEL replug and persistent instance corruption trace (2026-10-11)

- The user replugged the Pico. `picotool info -a` then confirmed BOOTSEL and
  identified the previous `pico2_choupi` image. `./tools/env ./tools/cycle
  choupi` passed both builds and visibly flashed the current ARM candidate to
  100%, then rebooted into CHOUPI.
- The bounded runtime log is
  `artifacts/logs/cycle-choupi-20261011T061811Z.log`, SHA-256
  `09be7d9368d2d988d4d953f45daa37f5832b9991d152de1b1de424ceb7266d40`. It
  reaches the persistent load for package 20/class 121. Three hierarchy
  calculations each report one class with 15 declared fields, but the loaded
  `JCVMArray` reports 8,912 fields. It completes existence checks for fields
  0–63 before the 18-second capture ends without `TEST:SUMMARY`.
- Source audit found `Heap::addInstance(JC_Instance instance)` copied the
  object into `shared_ptr` using the implicit shallow copy. `JC_Instance` owns
  raw `fields` and overflow-tag pointers and its destructor frees them, so the
  by-value temporary can invalidate the heap copy. A move-only ownership fix
  has been applied and both wrapper builds pass; it remains unflashed because
  the board returned to CDC after the cycle. Updated ARM UF2 SHA-256:
  `92ddc5aa9497f3020a302a253003f6074ebfcdb656eff9b7196beb406e7cfb98`.
- Next: when BOOTSEL is available, run `./tools/cycle choupi` for the move
  ownership fix and confirm the field count is 15 and runtime reaches its test
  summary. JCAlgTest v1.8.2 still has no APDU/CAP installation path.

## Persistent field load fix and continuation trace (2026-10-11)

- User made the Pico available in BOOTSEL. `./tools/env ./tools/cycle choupi`
  rebuilt ARM and RISC-V successfully, flashed the ARM image to 100%, and
  rebooted the application. The ownership fix changed package 20/class 121's
  field vector from the corrupted length 8,912 to the expected 15; all field
  loads (indexes 0–14) completed. The 18-second cycle still failed without
  `TEST:SUMMARY`. Log:
  `artifacts/logs/cycle-choupi-20261011T070346Z.log`.
- Added a diagnostic candidate around `JC_Instance::setField_Reference()` for
  class 121. Both ARM and RISC-V builds pass. UF2 SHA-256:
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.
- Tried `picotool reboot -F -u`; it returned success, but `picotool info -a`
  still saw CDC, no BOOTSEL. `picotool load -f -x` then returned without any
  `Loading into Flash` progress or reboot confirmation. A finite serial monitor
  was silent. Treat this diagnostic candidate as unflashed.
- Next: use the candidate after BOOTSEL reappears, then inspect the class-121
  reference and whether execution reaches array lookup or persistent write.
  No JCAlgTest v1.8.2 CAP installation has occurred; firmware still lacks an
  APDU transport and CAP installer.

## Connected Pico CDC retry (2026-10-11)

- The user said the device was present. Host enumeration confirmed the RP2350
  at bus 1/address 124 as USB serial, while `picotool info -a` reported no
  BOOTSEL device. Both generic and explicitly addressed `picotool reboot` requests
  returned success text but the device stayed in CDC mode. Application reboot
  and finite monitoring also produced no serial runtime output.
- Re-ran `./tools/env ./tools/cycle choupi`. ARM and RISC-V build gates passed.
  Picotool requested a BOOTSEL reboot, waited, and showed no flash progress;
  serial capture was silent and the runtime gate failed. Log:
  `artifacts/logs/cycle-choupi-20261011T071557Z.log` (SHA-256
  `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`).
- `./tools/env ./tools/doctor` reports all host tools, Pico SDK, CHOUPI sources,
  and capmap-free present. The diagnostic UF2 remains ready at
  `build/arm/pico2_choupi.uf2`, SHA-256
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.
- Next: flash after the Pico appears in actual BOOTSEL mode, then inspect the
  new `PUTFIELD_A` continuation markers. The attached CDC interface alone does
  not expose a working software route into BOOTSEL in the current firmware.

## Flash and run reference-field continuation trace (2026-10-11)

- After the cycle and direct `picotool load` requests had waited without flash
  progress, a later `picotool info -a` found the device in BOOTSEL mode. The
  device had transitioned asynchronously; the by-id CDC node had disappeared.
- `./tools/env ./tools/cycle choupi` then passed both builds, visibly flashed
  `build/arm/pico2_choupi.uf2` to 100%, and rebooted the application.
- The trace log is `artifacts/logs/cycle-choupi-20261011T074012Z.log`, SHA-256
  `997029462d5e282bd5726b40c2985070f093fa3ca07dd2aad40fb19bd08937eb`. It
  confirms class 121 field vector length 15, all field values loaded, array
  lookups succeeding, and persistent store for field 0 returning. The run then
  stops emitting runtime output before `TEST:SUMMARY`.
- Current next step: add a capped interpreter instruction trace to identify
  where execution stalls after `PUTFIELD_A phase=stored index=0`. The flashed
  candidate hash is
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.

## Robust flashing wrapper (2026-10-11)

- Added executable `tools/flash`; `tools/cycle` now calls it after both builds.
  The wrapper runs force-load first, but requires picotool's explicit 100%
  flash and application-reboot messages. If force-load only requests BOOTSEL,
  it polls for BOOTSEL for up to 45 seconds, then retries reset requests with
  bounded 20-second waits and one CDC 1200-baud fallback.
- Script syntax validation passed. A full CHOUPI cycle passed ARM and RISC-V
  builds but the connected firmware remained CDC. The helper correctly failed
  instead of claiming that flash succeeded. Log:
  `artifacts/logs/cycle-choupi-20261011T074828Z.log`, SHA-256
  `bf11511b61eb8b41c6675c41ad9f8985945d20757fbab9bf2c7ea82f252bc05b`.
- The ARM link map includes both `tud_cdc_line_coding_cb` and
  `usb_reset_interface_control_xfer_cb`, but the current CHOUPI runtime is
  stalled and does not service either reset route. The candidate diagnostic
  UF2 is `build/arm/pico2_choupi.uf2`, SHA-256
  `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`.
- Next: add an instruction-fed watchdog. On a watchdog timeout, the next startup
  should enter ROM BOOTSEL directly so future hung runs are recoverable by the
  host wrapper without a manual power cycle. The watchdog cannot rescue the
  currently installed image; it first needs one successful upload.

## Watchdog recovery candidate (2026-10-11)

- Added a 15-second RP2350 watchdog before VM startup. The interpreter refreshes
  it once per instruction via a Pico wrapper callback. If a handler blocks and
  the timeout resets the chip, startup checks `watchdog_enable_caused_reboot()`
  and calls `reset_usb_boot(0, 0)` before initializing USB CDC. This should make
  a future stalled VM enter BOOTSEL without depending on its USB task.
- ARM and RISC-V builds pass after routing the shared VM heartbeat through a
  board callback. The first direct hardware-header include failed because the
  shared object library lacks Pico SDK include paths; this was corrected.
- Candidate UF2 SHA-256:
  `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`.
  It remains unflashed: the current pre-watchdog firmware is present as CDC but
  does not service vendor reset or 1200-baud CDC reset requests. The robust
  `tools/flash` wrapper rejected the request-only force-load and exhausted its
  bounded recovery attempts. Latest log:
  `artifacts/logs/cycle-choupi-20261011T075539Z.log`.
- Next: flash this candidate once BOOTSEL is available. After a watchdog
  timeout, confirm the board reappears in BOOTSEL and that the host wrapper can
  flash it. Then resume the interpreter instruction trace.

## Final software recovery check (2026-10-11)

- Tried `picotool reboot -F -a`, waited 200 ms, then `picotool reboot -F -u`.
  Both commands printed their requested-mode success message, but an immediate
  `picotool info -a` still found the Pico in CDC mode at bus 1/address 126.
  The installed pre-watchdog runtime is not servicing its reset requests.
- The watchdog UF2 built for both architectures but remains unflashed. Its ARM
  image hash is
  `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`.
- Next: enter BOOTSEL once, run `./tools/env ./tools/cycle choupi`, and let the
  new watchdog provide autonomous BOOTSEL recovery if the VM stalls again.

## Latest continuation — CCID APDU exchange mode (2026-10-11)

- Changed the CCID class descriptor to `dwFeatures=0x00020042`: short APDU
  exchange plus automatic parameter configuration/negotiation. The old zero
  value declared character-level exchange while the firmware accepts complete
  APDUs through `XfrBlock`.
- Both architecture builds passed. The CHOUPI cycle flashed the updated ARM UF2
  and passed `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. The
  device's `lsusb -v` descriptor confirms short APDU mode and a 271-byte maximum
  CCID message. See `progress.md` and
  `artifacts/logs/cycle-choupi-20261011T101036Z.log`.
- Implementation and validation record committed as `4506ff7`; the final
  progress metadata follow-up records this SHA.
- PC/SC APDU transmission is still unverified. Flashing re-enumerated the Pico
  from USB node `001/016` to `001/018`; the ACL previously granted to `pcscd`
  did not persist, so pcsc_scan found no reader. The current libccid udev rule
  matches `ID_USB_INTERFACES=:020200:0a0000:0b0000:` and assigns group `pcscd`,
  but the live node still appears as `root:plugdev`; `udevadm test` confirms the
  expected group, so trigger the current device's rule and restart pcscd. Then
  test a raw APDU through GPPro. The current XfrBlock result remains placeholder `6D00`;
  applet dispatch, CAP install, and GlobalPlatform support are not implemented.

## PC/SC transport verified (2026-10-11)

- The user restored pcscd access after the Pico re-enumerated. `pcsc_scan -r`
  lists the Pico 2 CHOUPI reader. GlobalPlatformPro connected using T=1 with
  ATR `3B800181`, began a transaction, and sent `00A4040000`; the board returned
  `6D00` twice. The GPPro command exits 1 because this is the deliberately
  unimplemented XfrBlock placeholder response, not because PC/SC failed.
- This confirms USB CCID enumeration, activation, PC/SC transaction handling,
  and APDU-level XfrBlock request/response transport on the real board.
  Detailed output is in `artifacts/logs/pcsc-apdu-probe-20261011.txt` and the
  corresponding `progress.md` entry.
- Probe log and progress record committed as `eb6548f`; the metadata follow-up
  records this SHA.
- Next: find a minimal route from XfrBlock into the statically embedded
  JCAlgTest applet. Applet lifecycle and CAP loading remain unsupported; do not
  report the placeholder `6D00` as an applet response.

## GlobalPlatformPro list probe (2026-10-11)

- With PC/SC access restored, `gp --debug --list` connects to the Pico over
  T=1, receives ATR `3B800181`, and successfully transacts SELECT-by-empty-AID
  `00A4040000`. The board returns `6D00`, so GPPro exits 1 with
  `Could not SELECT default selected: 0x6D00 (Invalid INStruction)`.
- This verifies APDU transport but confirms AC1 is not yet met: no ISD select,
  GlobalPlatform card manager, applet registry, or SELECT dispatch exists.
  Trace: `artifacts/logs/gp-list-probe-20261011.txt`.
- Probe log and progress record committed as `e37ad51`; metadata follow-up
  records the SHA.
- Next: implement the minimal card manager/APDU lifecycle rather than treating
  the CCID layer as GlobalPlatform support.

## CAP applet AID lookup primitive (2026-10-11)

- Corrected `jc_cap_app::AID()` to read the CAP's one-byte AID length without
  applying a two-byte network-order conversion, which had included the first
  AID byte in the computed length.
- Added a bounded `jc_cap_applet_component::findByAID()` walker. It validates
  requested and stored AID lengths and ensures every variable-length applet
  record (AID plus install method offset) fits inside the component payload.
- `./tools/build arm` and `./tools/build riscv` pass. `git diff --check` passes.
  This is compile validation only: PC/SC service and libusb are unavailable in
  this VM session, and no hardware flash was attempted.
- No applet registry, SELECT, install execution, or APDU processing exists yet.
  Next integrate this lookup into the card manager and then build persistent
  interpreter contexts/lifecycle calls.

- The bounded CAP applet AID lookup implementation is committed as `52c7c7d`;
  a follow-up correction tracked its header as a regular file in `6f54e5e`.
  Both architecture builds passed for the implementation. The current session
  cannot access a PC/SC service or libusb device, so it has not flashed this
  source-only change.
- A direct read-only parse of the supplied v1.8.2 `Applet.cap` confirmed the
  component size is 14, with one applet AID `4A43416C675465737431` and install
  method offset `0x3DA3`; the variable-length traversal consumes the component
  exactly. Host hardware access in this session is unavailable (`lsusb` reports
  libusb initialization error `-99`, and the system bus/PCSC service is absent).

## Package-level AID resolution

- Added `FlashMemory_Handler::findAppletByAID()` to scan the persisted package
  bitmap and each installed CAP's Applet component, returning package ID and
  install-method offset. ARM and RISC-V builds both pass. The supplied
  JCAlgTest CAP's Applet component has been checked independently and matches
  the parser's size accounting.
- This lookup is not yet wired to CCID/SELECT, and it does not execute install
  or process methods. Implementing that requires persistent VM context/app
  instance state and a usable APDU buffer. `picotool info -a` currently fails
  at libusb initialization (`-99`), so no flash or hardware validation is
  possible in this session.
- The package-level registry lookup and its build record are committed as
  `fe7e12f`.

## Reflash after replug — 2026-10-11

- The updated ARM CHOUPI UF2 containing the AID parser and package-level
  registry lookup is now flashed. Both wrapper builds passed. Picotool force
  reset failed because it could not locate the reset interface; `tools/flash`
  then used CDC 1200 baud, entered BOOTSEL within 2 seconds, and loaded the UF2
  to 100%.
- The bounded hardware cycle passed with repeated `TEST:PASS choupi_runtime`
  and `TEST:SUMMARY pass=1 fail=0`, no `TEST:FAIL` or `FAULT`. Log:
  `artifacts/logs/cycle-choupi-20261011T111329Z.log`, SHA-256
  `354482ef6091f85a04b5192fea4a17574600f2523042b465a6410f72768b8009`.
  UF2 SHA-256: `ec437a81244d1729c4d6713825d50c79daec6252cb22041b009819cbca8b8a45`.
- Post-flash USB re-enumerated as RP2350 CDC bus 1/address 21. PC/SC has no
  active daemon/reader at present, so no post-flash APDU test was possible.
  Applet install/process lifecycle and APDU dispatch remain unimplemented;
  the registry lookup is not yet connected to SELECT.

## Faster flash recovery — 2026-10-11

- `tools/flash` now classifies `picotool load` failures. If picotool reports a
  missing reset interface, the wrapper goes directly to the known-working CDC
  1200-baud reset rather than waiting through 45+20 seconds of BOOTSEL polling.
  If libusb itself cannot initialize, it fails promptly instead of retrying
  inaccessible operations.
- Validated on the Pico 2: both builds passed, CDC fallback entered BOOTSEL in
  2 seconds, the UF2 flashed to 100%, and finite serial output passed with
  `TEST:SUMMARY pass=1 fail=0`. The failed reset-interface branch was exercised
  by the successful cycle. The libusb-init fast-fail branch was also verified
  without device access. Cycle log:
  `artifacts/logs/cycle-choupi-20261011T111923Z.log`, SHA-256
  `0eadba75a08908f07b30507e136d9546cdc5b1e5ad705de91898ed70857bc584`.
- The board still re-enumerates as CDC after flashing. PC/SC has no active
  daemon/reader, so APDU validation remains blocked; applet install/process and
  GlobalPlatform support are still not implemented.

## PC/SC after the latest flash

- Started `pcscd.service` without sudo; it is active. The Pico is present as
  `2e8a:0009` at bus 001/device 023, but `pcsc_scan -r` reports no reader.
  The previous per-node ACL did not follow re-enumeration. Current node:
  `/dev/bus/usb/001/023`. Once `pcscd` is granted read/write access to that
  node and restarted, rerun the PC/SC/GPPro probes. Do not use sudo from the
  autonomous cycle; the user previously supplied and ran the ACL command.

## Passwordless PC/SC recovery helper — 2026-10-11

- Added `tools/pico2-pcsc-reader-root`, which dynamically finds exactly one
  Pico 2 CHOUPI CCID (`2e8a:0009`), grants `pcscd` read/write access to its
  current USB node, and restarts `pcscd.service`. It rejects arguments and
  fails closed when the matching device is absent or ambiguous.
- Added `tools/install-pcsc-sudoers`. The user must run
  `sudo ./tools/install-pcsc-sudoers` once. It installs the helper as
  root-owned at `/usr/local/sbin/pico2-pcsc-reader` and creates a `visudo`
  checked NOPASSWD rule allowing the invoking user to run that helper with no
  arguments. No system file has been modified by this session.
- After setup, run `sudo -n /usr/local/sbin/pico2-pcsc-reader`, then
  `pcsc_scan -r`. Continue with GPPro/APDU probes after the reader appears.
- Shell syntax and standalone sudoers syntax validation passed. The setup is committed as `97c7eeb`.

## Installer visudo path correction — 2026-10-11

- The user's first run stopped before making system changes because the preflight
  looked for `/usr/bin/visudo`; this host provides `/usr/sbin/visudo`. Corrected
  the preflight path. `bash -n` and standalone sudoers parsing pass.
- Next: user reruns `sudo ./tools/install-pcsc-sudoers`; then invoke the helper
  and confirm `pcsc_scan -r` discovers the reader. Fix committed as `5421889`.

## GPPro after PC/SC recovery — 2026-10-11

- The user ran the installed recovery helper successfully; `pcsc_scan -r`
  finds `Pico 2 CHOUPI CCID`. An elevated GlobalPlatformPro 26.06.04 probe
  connected at T=1 (ATR `3B800181`) and sent default SELECT `00A4040000`;
  firmware replied `6D00`, and GPPro exited with invalid instruction. The
  trace is `artifacts/logs/gp-list-probe-20261011-reader-ready.txt`, SHA-256
  `14a11819fc90b44f3c4e3a9a9b3800e6904c9ca3472518718230a94dc2b54f21`.
- Transport and PC/SC are working; APDU dispatch remains a placeholder. Next
  implement default ISD SELECT (`9000`), build both targets, flash, and inspect
  GPPro's next command to guide incremental card-manager support. Probe record
  committed as `48a6656`.

## Default ISD selection and FCI — 2026-10-11

- The CCID driver now handles only `00 A4 04 00 00` as default ISD
  selection, returning minimal FCI for AID `A000000151000000` and `9000`.
  Other APDUs remain unsupported (`6D00`). Both architecture builds passed;
  the actual Pico 2 cycle flashed successfully and emitted
  `TEST:SUMMARY pass=1 fail=0` without `TEST:FAIL` or `FAULT`.
- GPPro 26.06.04 parsed the corrected FCI, detected the ISD AID, then sent
  SCP03 INITIALIZE UPDATE (`8050000008` + host challenge). That still returns
  `6D00`, so `gp --list` has advanced but does not pass. The first FCI attempt
  had an incorrect outer length (`6F0C`); corrected to `6F0A`.
- Latest cycle log: `artifacts/logs/cycle-choupi-20261011T114816Z.log`, SHA-256
  `b9afd73bf1c011b45c69944f3c0189d3f449cec2169823d1b8cd08555f9ad02d`.
  GPPro trace: `artifacts/logs/gp-list-probe-20261011-isd-fci-fixed.txt`,
  SHA-256 `da56ac7750d48a44cd75f5ce6949c7596bc26e98d4d38b6cccd9153fdc865cd7`.
- Next: implement SCP03 INITIALIZE UPDATE / EXTERNAL AUTHENTICATE using the
  default test keys, then implement GET STATUS and continue toward GP CAP load.
  Implementation and validation committed as `9e9e3ae`.

## Raw GP read-only probes — 2026-10-11

- GPPro raw APDU mode sent GET STATUS (`80F20000024F00`), GET DATA CPLC
  (`80CA9F7F00`), and GET DATA Card Data (`80CA006600`) without opening a
  secure channel. Each returned `6D00`; automatic discovery then SELECTed the
  default ISD and received its valid FCI and `9000`. No write command ran.
- Trace: `artifacts/logs/gp-raw-readonly-probes-20261011.txt`, SHA-256
  `8795b672b2dd211d2ccc18ac4f5e05579671b6f22f49adaa2d7752fbefb4df5b`. Next implement SCP03; then add tag-form GET STATUS
  and read-only GET DATA before any CAP loading. Probe record committed as `ab833a3`.

## SCP03 secure channel on hardware — 2026-10-11

- `src/scp03.cpp` derives SCP03 session keys and 64-bit cryptograms with
  AES-CMAC, validates EXTERNAL AUTHENTICATE, and authenticates subsequent
  commands. EXTERNAL AUTHENTICATE carries a trailing Le, so the firmware
  accepts both 21-byte case-3 and 22-byte case-4 forms.
- `gp --debug --list` exits 0 on the Pico: SCP03 EXTERNAL AUTHENTICATE returns
  `9000`; authenticated GET STATUS returns ISD `A000000151000000` in lifecycle
  `OP_READY`. GPPro's further registry probes return `6A88` for absent
  applets/domains.
- ARM and RISC-V wrapper builds passed. The real-board CHOUPI cycle passed
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. Logs:
  `artifacts/logs/cycle-choupi-20261011T120932Z.log` and
  `artifacts/logs/gp-list-probe-20261011-scp03-external-auth.txt`.
- Implementation and validation record committed as `ad08669`.
- CAP installation remains unsupported. Next run the authorized
  GlobalPlatformPro install attempt for the supplied JCAlgTest v1.8.2 CAP to
  identify the first GP LOAD/INSTALL APDU, then implement its sequence.

## JCAlgTest v1.8.2 install probe — 2026-10-11

- The exact supplied CAP hash is
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
- GlobalPlatformPro authenticated successfully, then sent secured
  `84 E6 02 00` (INSTALL [for load]) with the package AID and ISD AID. The
  board returned `6D00`; no LOAD block followed and the CAP was not installed.
  Probe log: `artifacts/logs/gp-install-probe-20261011.txt`.
- Probe and continuity record committed as `726d8e8`.
- Next: add strict parsing and state for INSTALL [for load], capture the LOAD
  command framing, and implement persistent CAP staging only after its size
  and flash space requirements are known. Existing embedded static ROM-mask
  package remains the boot-time runtime image.

## Runtime GP LOAD and applet SELECT — 2026-10-11

- The CCID path now accepts the exact JCAlgTest v1.8.2 GlobalPlatform LOAD
  sequence. It reassembles bounded C4 data blocks, checks component bounds and
  package/applet AIDs, stores package 20 in the 64 KiB runtime overlay, accepts
  INSTALL [for install and make selectable], and reports the instance in GET
  STATUS. Full short-APDU SCP03 MAC inputs are supported.
- ARM/RISC-V builds passed and the real board cycle emitted
  `TEST:SUMMARY pass=1 fail=0`. GPPro `--install` returned success; a later
  `--list` showed applet `4A43416C675465737431`, and SELECT-by-AID returned
  `6F0C840A4A43416C675465737431 9000`.
- Important limit: the Pico filesystem overlay is only RAM-backed. The CAP
  disappears after reboot/reflash. Java Card applet install-method execution
  and selected-applet APDU dispatch are also unfinished.
- The implementation and hardware validation are committed as `1107aaa`.
- Next: add a safe bounded nonvolatile CAP slot, ensure image/slot separation,
  and verify persistence across reboot. Then connect SELECT/process to the
  CHOUPI lifecycle. Hardware logs are recorded in `progress.md`.

## JCAlgTest CAP persistence across reboot — 2026-10-11

- Added a dedicated 256 KiB RP2350 flash slot at offset `0x1C0000`, with a
  CRC-protected header and payload. The firmware image ends at `0x1009FED4`,
  below this slot. The CAP is not exposed as the embedded CHOUPI filesystem's
  package record; that lookup collision caused a VM exception on the first
  persistence reboot attempt. After separating these paths, the CHOUPI runtime
  self-check passed again.
- CAP restoration now happens on CCID card power-on, after `fs_init()` has
  initialized the flash-backed CAP cache. GlobalPlatformPro confirmed the
  installed `4A43416C675465737431` instance survives a firmware reflash/reboot;
  registry listing reports it `INITIALIZED`, and SELECT returns its `6F/84`
  FCI with `9000`.
- Validation: both architecture builds passed; `./tools/cycle choupi` passed
  `TEST:SUMMARY pass=1 fail=0`; GPPro `--list` and SELECT-by-AID passed after
  reboot. Logs and hashes are in `progress.md` and
  `artifacts/logs/*persistent-cap-20261011*`.
- Commit: `a42a2a9a05ab65ad92edcb0729735d8cdac62bce`.
- Remaining: the applet's install method/lifecycle and selected applet APDU
  processing are not implemented. Next connect the persistent CAP to CHOUPI
  CAP loading, invoke installation/lifecycle methods, and route selected
  applet APDUs into the VM.

## Persisted CAP connected to CHOUPI package resolution — 2026-10-11

- `FlashMemory_Handler` now exposes the validated flash CAP as reserved package
  ID 63, returns it from `getCap(63)`, and checks its Applet component in
  `findAppletByAID`. This leaves static ROM package 20 unchanged. The CCID
  SELECT path requires that lookup to resolve the saved AID and install-method
  offset before returning FCI.
- First experiment scanned all enabled ROM packages before checking the dynamic
  CAP; it exceeded the USB servicing budget and the watchdog took the device
  to BOOTSEL. The resolver now checks the one dynamic CAP first and contains
  lookup exceptions at the C bridge. GPPro SELECT then returned the expected
  FCI / `9000` in 55 ms.
- ARM/RISC-V builds and the flashed CHOUPI runtime cycle passed. Artifacts and
  hashes are in the latest `wp7-vm-dynamic-cap-resolution` entry in
  `progress.md`. Implementation commit:
  `69616aea484ebf27307ec6fb758a71e99f1b682d`.
- Next: invoke the CAP's install method with the required applet constructor
  and install parameters; then preserve applet instance state and dispatch
  SELECT/process/deselect through the VM. No applet bytecode has run yet.


## Raw CAP method invocation entry — 2026-10-11

- Commit `3e49fc9c9cec1c38aeed431b2ba281ccb3556934` adds an interpreter
  constructor that starts a static method by its Method-component offset and
  seeds the `Applet.install(byte[], short, byte)` locals. Existing exported
  method startup remains unchanged. ARM/RISC-V builds passed and the real
  Pico cycle passed after USB access recovery (`TEST:SUMMARY pass=1 fail=0`).
- GPPro read-only SELECT-by-AID still returns `6F0C840A4A43416C675465737431 9000`
  after reflash (9 ms); see `artifacts/logs/gp-select-install-entry-20261011.txt`.
- This is only an invocation primitive. It has not been called yet. GP INSTALL
  discards its installation-parameter LV, JCAlgTest's constructor first calls
  unimplemented `JCSystem.getAvailableMemory` natives, and `Applet.register()`
  is a Java API no-op. Next parse/preserve install parameters and invoke the
  offset entry from INSTALL, then implement the first native call and truthful
  applet registration outcome.

## JC 3.0.5 imported class-token compatibility — 2026-10-11

- The JCAlgTest v1.8.2 CAP's imported API class tokens differ from CHOUPI's
  embedded export component. AID-based mappings were added for java/lang,
  javacard/framework, javacard/security, and javacardx/crypto and applied to
  class resolution plus external invokespecial class lookup.
- ARM/RISC-V builds passed. Real Pico 2 flash passed; the runtime self-test
  reported `TEST:SUMMARY pass=1 fail=0`. GPPro successfully LOADed the entire
  131,538-byte CAP. INSTALL now enters the Java install method, progressing
  beyond the prior superclass/interface wedge. The first attempt reached
  `IndexOutOfBoundsException` at `invokespecial` PC `0x101C4E1B`; after external
  constructor class-token translation it advanced to `NotYetImplemented` at
  `impdep1` PC `0x10047BF8`. The CAP is not installed.
- Logs: `artifacts/logs/gp-install-jc305-token-remap-20261011.txt` and
  `artifacts/logs/gp-install-jc305-method-token-remap-20261011.txt`.
- Implementation commit: `a71356d`.
- Next: capture the native index consumed by `impdep1`, map it to the generated
  native handler, implement the required constructor operation, then rebuild,
  flash, and repeat GPPro INSTALL. Keep USB recovery via the bounded
  `tools/flash` wrapper and user-authorized PC/SC helper.
