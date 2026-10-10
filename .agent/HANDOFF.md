# Handoff

## Objective

Implement `plan.md` for CHOUPI on Pico 2, following root `AGENTS.md`. The user
has connected the Pico 2 and requested real-board bring-up. Use JCAlgTest
v1.8.2 and preserve the provided source trees.

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
  source-derived image is `build/choupi-rommask/flash-algtest.bin` (524,300
  bytes, SHA-256 `93a6678b3288e4ee92de2fc8df4e40b9f0bef2b297f1ceae3b761ce3a71e35e6`).
  The rebuilt CAP differs from the supplied CAP, and this pre-mask image has
  not been integrated into firmware or flashed.
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

Continue the RP2350 port and CAP installation path. The restored parser and
API source-derived ROM mask are ready, and the VM C++ sources now cross-compile
for both Pico 2 ISA targets. Next link an ARM CHOUPI firmware image with a
Pico-specific entry point and a documented single-context runtime/storage
shim. Then add an install path for the exact supplied CAP, initially over a
bounded CDC protocol if CCID is not ready. The supplied CAP differs from the
source-rebuilt CAP, so target the exact supplied file when implementing the
loader. The board currently runs the passing standalone smoke image; do not
claim CHOUPI or JCAlgTest is running until a built image is flashed and its
bounded serial/APDU output is inspected. Preserve `.local-toolchain/`,
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
