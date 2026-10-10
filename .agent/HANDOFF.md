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
  Generated API JCA packages now match these identifiers and versions;
  ROM-mask generation and complete import resolution remain unverified.
  Filtered CapDump output: `artifacts/logs/capdump-v182-20261010.txt`.
- A repository-local toolchain is installed under `.local-toolchain/`; use
  `./tools/env <command>` to select it. It includes CMake/Ninja, ARM and
  RISC-V compilers, stable Rust with the Cortex-M33 target, Pico SDK 2.3.1,
  pioasm, and picotool. See `.agent/TOOLCHAIN.md` for versions and hashes.
  Setup and the hardware report are committed as `db99639`.
- The board is visible in BOOTSEL mode. Read-only `picotool info -a` reported
  RP2350 A4, QFN60, 2048 KiB flash, and an existing RISC-V image named
  `pico2_btc_miner`. Before flashing, saved a read-only 2 MiB backup at
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
  pinned `capmap-free` submodule and JC 3.0.5u3 API kit explicitly; it reports
  the unavailable CAP parser. Maven's local repository is redirected inside
  `.local-toolchain/`; resolution of Maven Central currently fails because
  DNS is unavailable.
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

Find source for the exact pinned `capmap-free` commit or replace its parsing
role with a small compatible CAP reader after documenting the parser API used
by `rommask`. The Bitbucket repository returns 404/access denied and the
initial public mirror search found no copy. Maven Central DNS also fails in
this environment. The API export patch is built and its JCA metadata matches
all four CAP imports. Next restore CAP parsing and Maven artifacts, generate
the ROM mask, and attempt the first CHOUPI ARM configuration. Continue WP5
native and lifecycle work in parallel. The board currently emits the passing
standalone smoke output; it is not running CHOUPI or JCAlgTest. Preserve
`.local-toolchain/`, `AlgTest_JavaCard/`, and the CHOUPI source tree.

## Latest validation

- Current board bring-up: `bash -n tools/common.sh tools/build tools/cycle
  tools/monitor`, ARM build, RISC-V build, and `./tools/env ./tools/cycle arm`
  all passed. The 8-second capture emitted repeated
  `TEST:PASS random_stream_varies` and `TEST:SUMMARY pass=1 fail=0`; log:
  `artifacts/logs/cycle-arm-20261010T053019Z.log`.
- Dependency restoration: OS and ROM-mask checkouts match pinned full commits;
  the API is at its pinned commit plus local patch `d570ce3`. ROM-mask
  generation and CHOUPI compilation remain unvalidated because `capmap-free`
  is missing.
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
- Toolchain setup: `bash -n tools/env` passed. `./tools/env rustup target
  list --installed` includes `thumbv8m.main-none-eabihf`; CMake/Ninja, both
  compiler prefixes, and picotool version checks passed. `./tools/env
  ./tools/doctor` reports host tools and Pico SDK present, but
  exits 1 for the missing CAP parser. Maven 3.10.0 is available through
  `./tools/env`; the API build passes with Temurin 8u504. Root smoke wrappers
  pass for both ISAs. Board queries and cycles required reviewed escalation.
