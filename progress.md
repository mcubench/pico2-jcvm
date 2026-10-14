## 2026-10-09 — Development environment diagnostics

- Step/experiment: `wp1-host-diagnostics`
- Change: Added `tools/doctor` to report host tools, Pico SDK configuration, and required CHOUPI dependency checkouts. Added session continuity instructions and a handoff record because the required continuity file was absent.
- Validation: `bash -n tools/doctor` passed. `./tools/doctor` returned 1 as expected and identified missing `cmake`, `cargo`, `rustc`, `PICO_SDK_PATH`, and the OS/API/rommask dependency sources; it did not attempt hardware access.
- Result: The checkout inspection found the C++ VM and AlgTest source, but its OS/API/rommask dependencies are empty; GitHub access and the local embedded build toolchain are unavailable.
- Decision: Preserve supplied source trees; proceed with VM-only changes where safe, then populate pinned dependencies and establish the host baseline before hardware port validation.
- Commit: `1616b59`
- Artifacts/logs: `.agent/HANDOFF.md`; no hardware log

## 2026-10-09 — Fix JCVM array fill

- Step/experiment: `wp5-jcvmarray-fill`
- Change: Corrected `JCVMArray::fill()` to write each element by index, and added a focused host test for a populated array and a zero-length array.
- Validation: `bash -n tools/test-jcvmarray` passed. The first strict compile exposed a pre-existing ignored-qualifier warning on `size()`; after removing it, `./tools/test-jcvmarray` passed with `TEST:PASS jcvmarray_fill` under `-Wall -Wextra -Werror -pedantic`.
- Result: All four elements received `0xBEEF`; zero-length fill completed without touching storage.
- Decision: Retain the fix. Full ARM/RISC-V builds remain unavailable because the SDK, Rust OS and build tools are missing; no hardware run was attempted.
- Commit: `0d293ae`
- Artifacts/logs: Test emits `TEST:PASS jcvmarray_fill` on success.

## 2026-10-09 — Persist static reference fields

- Step/experiment: `wp5-putstatic-reference`
- Change: Implemented `putstatic_a` resolution for internal and imported fields, then persisted object/array values with the existing serializers. Added null-reference markers and fixed indexed tags for nested persistent reference-array elements, including bounds checks.
- Validation: `bash -n tools/check-vm-syntax` passed; `./tools/check-vm-syntax` parsed `bc_static.cpp`, `jc_static.cpp`, and `flashmemory.cpp` and emitted `TEST:PASS vm_static_reference_syntax`. The script uses temporary variadic FFI declarations because the Rust-generated `ffi.h` is missing, so this does not validate ABI compatibility. `./tools/test-jcvmarray` also passed with `TEST:PASS jcvmarray_fill`.
- Result: C++ syntax checks passed. Existing warnings in upstream declarations/serialization code remain; full CMake/ARM/RISC-V builds and persistence runtime checks could not run without the absent dependencies and toolchain.
- Decision: Retain this implementation step and continue the remaining exception, APDU, and lifecycle work after restoring the required source dependencies. Hardware validation was intentionally skipped.
- Commit: `ab1957b`
- Artifacts/logs: `/tmp/vm-syntax.log` (temporary syntax-check output; no hardware log)

## 2026-10-09 — Validate persistent reference-array writes

- Step/experiment: `wp5-persistent-array-reference-guard`
- Change: Added transient-array, element-type, and index checks to the persistent reference-array setter before it writes the indexed child tag.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_static_reference_syntax`; `./tools/test-jcvmarray` passed with `TEST:PASS jcvmarray_fill`.
- Result: The setter and the related static persistence translation units pass syntax checking. The syntax shim does not validate the generated Rust FFI ABI, and persistent storage behavior remains unverified at runtime.
- Decision: Keep the bounds/type guard; continue to the remaining WP5 runtime gaps. Hardware and full target builds remain unavailable.
- Commit: `7e04f97`
- Artifacts/logs: `/tmp/vm-syntax.log`; no hardware log

## 2026-10-09 — Implement object-deletion fallback natives

- Step/experiment: `wp5-object-deletion-natives`
- Change: `isObjectDeletionSupported()` now returns `FALSE`, and `requestObjectDeletion()` is a no-op as specified for a platform without object-deletion support.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`, including `jni.cpp`. The syntax check uses a temporary FFI shim and does not prove generated ABI linkage.
- Result: Both native stubs now provide the defined unsupported-platform behavior instead of raising `NotYetImplemented`. No target build or runtime APDU test was possible with current prerequisites.
- Decision: Retain and continue with other WP5 native methods. Hardware validation remains intentionally deferred.
- Commit: `512337f`
- Artifacts/logs: `/tmp/vm-syntax.log`; no hardware log

## 2026-10-09 — Define false class-cast results

- Step/experiment: `wp5-exception-type-match`
- Change: Made `Class_Handler::docheckcast()` return `FALSE` when a class does not implement the requested interface, instead of falling through without a result. This makes typed exception-handler matching deterministic on non-matches.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`, including `jc_class.cpp`; no compiler errors or missing-return diagnostic were emitted.
- Result: The exception matcher translation unit parses with a defined non-match path. The syntax check still uses a temporary FFI shim and does not exercise CAP hierarchy data.
- Decision: Retain this fix as part of WP5 exception dispatch. Runtime type-hierarchy tests and full architecture builds remain pending dependencies.
- Commit: `6f7f1bc`
- Artifacts/logs: `/tmp/vm-syntax.log`; no hardware log

## 2026-10-09 — Repair persistent instance reference access

- Step/experiment: `wp5-persistent-instance-reference`
- Change: Initialized the persistent object's class index from the constructor argument, made instance tag reconstruction start at field zero, returned loaded reference-field values, and routed persistent reference writes (including null) through the shared serializer.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`, including `jc_instance.cpp`; the prior uninitialized-index and self-initialization warnings disappeared. `./tools/test-jcvmarray` remains passing.
- Result: Persistent instance tags and reference access have defined code paths. `JC_Instance::getFields()` still cannot deserialize a complete persistent field vector, so reserializing loaded objects remains incomplete; runtime persistence checks and full builds are unavailable.
- Decision: Retain these fixes and continue implementing persistent-object field reconstruction after the dependency/API model is available. Hardware validation remains deferred.
- Commit: `a740f5e`
- Artifacts/logs: `/tmp/vm-syntax.log`; no hardware log

## 2026-10-09 — Dispatch Java athrow handlers

- Step/experiment: `wp5-athrow-dispatch`
- Change: Frames now retain the active opcode address, and `athrow` searches CAP exception-handler records in the current frame and callers. Matching covers catch-all entries and resolved class hierarchy checks; unwinding restores the package context and clears the handler operand stack before pushing the exception object.
- Validation: `bash -n tools/check-vm-syntax` and `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`. The syntax wrapper uses temporary variadic FFI declarations, so it does not validate the Rust/C ABI. `./tools/test-jcvmarray` passed with `TEST:PASS jcvmarray_fill`.
- Result: Touched VM translation units parse successfully. Existing upstream warnings remain. Native `Exceptions` enum-to-Java-object mapping, uncaught-exception status words, full target builds, and runtime handler tests remain incomplete.
- Decision: Retain this first Java-object exception dispatch path; continue WP5 with native exception mapping and lifecycle/APDU integration. Device validation is intentionally deferred.
- Commit: `476a938`
- Artifacts/logs: `/tmp/vm-syntax.log`; no hardware log

## 2026-10-09 — Fix persistent instance field reference access

- Step/experiment: `wp5-persistent-instance-reference-access`
- Change: Corrected the persistent-instance constructor to retain the supplied class index, returned values from persistent reference reads, and routed persistent reference writes through the null-safe serializer. This closes two defects found while repairing persistent tag reconstruction.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`, including `jc_instance.cpp`; the previous constructor self-initialization and unused-parameter warnings are gone.
- Result: Persistent class identity and reference reads/writes now use defined code paths. The C++ syntax test does not exercise filesystem behavior; persistent `getFields()` deserialization remains incomplete.
- Decision: Retain the fixes and continue with other VM runtime gaps. Full target builds and hardware validation are still unavailable.
- Commit: `a05147e`
- Artifacts/logs: `/tmp/vm-syntax.log`; no hardware log

## 2026-10-09 — Load persistent instance fields

- Step/experiment: `wp5-persistent-instance-fields`
- Change: Separated persistent object tags from the in-memory field vector and implemented lazy reconstruction of stored byte, boolean, short, int, reference, and array fields. Field setters now keep the cached type/value representation in sync, including null references and the two-word int layout.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`; `./tools/test-jcvmarray` passed with `TEST:PASS jcvmarray_fill`. `./tools/build arm` and `./tools/build riscv` could not run because the instructed `tools/build` wrapper is absent (status 127). Hardware validation was intentionally skipped; no device was requested.
- Result: VM translation units pass syntax checking, with pre-existing upstream warnings. Filesystem round-trip behavior is not measured because the filesystem submodule and runtime are absent. Missing child records cannot be typed using the current class metadata API.
- Decision: Retain the lazy field reconstruction and proceed to the remaining exception/APDU lifecycle gaps. Full target builds and hardware validation remain blocked by the missing build wrapper, toolchain, SDK, and OS sources.
- Commit: `5263602`
- Artifacts/logs: `/tmp/pico2-vm-syntax.log`; no hardware log

## 2026-10-09 — Return status for uncaught VM exceptions

- Step/experiment: `wp5-uncaught-exception-status`
- Change: Replaced the interpreter's infinite uncaught-exception loop with status word `0x6F00`. `Interpretor::run()` returns its status, `runtime()` propagates it, and an `athrow` that exhausts all frames is distinguished from normal method completion.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`; `./tools/test-jcvmarray` passed with `TEST:PASS jcvmarray_fill`. Both instructed builds were attempted but `./tools/build` is absent (status 127 for ARM and RISC-V). Hardware validation was intentionally skipped.
- Result: Uncaught C++ VM exceptions and unmatched Java `athrow` paths now return `0x6F00` from the runtime entry instead of hanging. Runtime/APDU delivery is not yet connected, and VM exceptions are not yet materialized as Java objects. No runtime measurement was possible.
- Decision: Retain the failure status path and continue WP5 with Java exception mapping and APDU integration.
- Commit: `ff6a4b8`
- Artifacts/logs: `/tmp/pico2-vm-syntax.log`; no hardware log

## 2026-10-09 — Report zero transaction capacity

- Step/experiment: `wp5-transaction-capacity-reporting`
- Change: Implemented the JCSystem transaction information natives to report zero transaction depth, zero unused commit capacity, and zero maximum commit capacity. This lets AlgTest's system-information query report the runtime's actual lack of transactional persistence.
- Validation: `./tools/check-vm-syntax` passed with `TEST:PASS vm_core_syntax`; `./tools/test-jcvmarray` passed with `TEST:PASS jcvmarray_fill`. ARM and RISC-V build attempts returned 127 because `tools/build` is absent. Hardware validation was intentionally skipped.
- Result: Capability queries return zero without raising `NotYetImplemented`. Mutating transaction calls remain unimplemented; no runtime/APDU measurement was possible.
- Decision: Retain the honest capability values; transaction operations still require atomic persistence support.
- Commit: `53660ca`
- Artifacts/logs: `/tmp/pico2-vm-syntax.log`; no hardware log

## 2026-10-09 — Fix Java Card array storage ownership

- Step/experiment: `wp5-transient-array-storage`
- Change: Added safe deep-copy/move ownership to mutable `JCVMArray` instances while retaining non-owning `const` CAP views. `JC_Array` now frees its backing storage once, allocates the full persistent tagged payload, returns correct transient element counts, and names the reset clear event `CLEAR_ON_RESET`.
- Validation: `./tools/test-jcvmarray` passed `jcvmarray_fill` and `jcvmarray_copy_move`. `./tools/test-jcarray` passed `TEST:PASS jcarray_transient_size` under AddressSanitizer with LeakSanitizer disabled because LeakSanitizer fails in this environment. `./tools/check-vm-syntax` passed. ARM and RISC-V build attempts returned 127 because `tools/build` is absent; hardware validation was intentionally skipped.
- Result: Mutable copies no longer alias backing storage, moved-from arrays become empty, read-only CAP views remain views, transient byte/short arrays report their requested element counts, and sanitizer checks found no double-free or out-of-bounds access in the focused transient-size case.
- Decision: Retain the ownership and sizing fixes; next connect native array creation to the active execution context and implement actual clear-event lifecycle behavior.
- Commit: `6fa7232`
- Artifacts/logs: no archived logs; test wrappers emitted the PASS records above

## 2026-10-09 — Bind transient array natives to the active context

- Step/experiment: `wp5-active-context-transient-arrays`
- Change: Added an RAII active-context scope around interpreter execution and used it to implement `isTransient` plus transient boolean, byte, short, and int array creation. Invalid clear events raise `SecurityException`; negative lengths raise `NegativeArraySizeException`; null `isTransient` input raises `NullPointerException`. Added context implementation to the VM syntax check.
- Validation: `./tools/check-vm-syntax`, `./tools/test-jcvmarray`, and `./tools/test-jcarray` passed. The syntax check still reports existing upstream warnings. `./tools/build arm` and `./tools/build riscv` both returned 127 because `tools/build` is absent. Hardware validation was intentionally skipped.
- Result: Context-free native methods can resolve the current applet heap during interpreter execution, and primitive transient-array factories construct arrays with their requested type and clear event. No active-context/native runtime test could be linked in the missing OS/build environment. Transient object arrays remain unsupported because the native signature does not supply a reference type; lifecycle clearing awaits select/deselect/reset hooks.
- Decision: Keep the context scope bounded to interpreter runs and continue with native utility/API coverage; integrate clearing when lifecycle dispatch exists.
- Commit: `e56d14f`
- Artifacts/logs: test wrappers emitted the PASS records above; no hardware log

## 2026-10-09 — Clear transient arrays by lifecycle event

- Step/experiment: `wp5-transient-array-clear-events`
- Change: Added `JC_Array::clearTransientData()` and `Heap::clearTransientArrays(event)`. The heap clears only transient arrays whose clear event matches reset or deselect. Expanded the focused ASan test to verify reset leaves deselect arrays intact and deselect clears its arrays.
- Validation: `./tools/test-jcarray` passed `jcarray_transient_size` and `jcarray_clear_events` under AddressSanitizer with leak detection disabled. `./tools/check-vm-syntax` and `./tools/test-jcvmarray` passed. ARM and RISC-V builds could not start because `tools/build` is absent (status 127). Hardware validation was intentionally skipped.
- Result: The heap now exposes the operation needed for reset/deselect lifecycle handlers and its event selection is covered by a host test. There are no lifecycle dispatch hooks yet, so automatic clearing during card events is not established.
- Decision: Keep the tested heap clearing primitive; connect it when select/deselect/reset dispatch is implemented, and continue filling the `Util` natives.
- Commit: `6d3630e`
- Artifacts/logs: no archived logs; focused tests emitted the PASS records above

## 2026-10-09 — Implement primitive native array copying

- Step/experiment: `wp5-native-array-copy`
- Change: Implemented overlapping-range-safe `arrayCopyRepack` and its non-atomic counterpart for same-type primitive arrays, with negative/range checks and incompatible-type errors. The copy snapshots source values before writing, so overlapping copies behave correctly. Reference-array copying remains unimplemented because it requires firewall class compatibility handling.
- Validation: `./tools/test-jcarray` passed transient sizing, clear-event selection, overlapping native copy, and invalid-copy error checks under AddressSanitizer with leak detection disabled. `./tools/check-vm-syntax` and `./tools/test-jcvmarray` passed. ARM and RISC-V build wrappers are absent (both commands returned 127); hardware validation was intentionally skipped.
- Result: Primitive array copying now works through the active applet heap, including self-overlap. Persistent array atomicity is not guaranteed because transactions are not implemented.
- Decision: Retain the primitive copy implementation and continue with the compare/fill utilities used by Java Card `Util`.
- Commit: `8e92ec1`
- Artifacts/logs: no archived logs; focused tests emitted the PASS records above

## 2026-10-09 — Implement native byte array comparison

- Step/experiment: `wp5-native-array-compare`
- Change: Implemented `arrayCompareGeneric` for byte arrays with null/range checking and Java Card ternary comparison results. Bytes compare as unsigned values; incompatible types raise `SecurityException`.
- Validation: `./tools/test-jcarray` passed native comparison checks, including ordering `0x01` before `0xFF`, alongside transient clear and copy cases. `./tools/check-vm-syntax` and `./tools/test-jcvmarray` passed. Both target build commands returned 127 because `tools/build` is absent. Hardware validation was intentionally skipped.
- Result: Java Card byte-array compare semantics now have a tested native path. Comparison does not include Java Card `SensitiveResult` tracking.
- Decision: Continue with the pattern-based fill helper and then resolve the remaining AlgTest native/API dependencies.
- Commit: `92ef957`
- Artifacts/logs: no archived logs; focused tests emitted the PASS records above

## 2026-10-09 — Correct byte array comparison ordering

- Step/experiment: `wp5-native-array-compare-signed-order`
- Change: Changed `arrayCompareGeneric` to compare Java Card byte values as signed `jbyte_t` values. Kept the required -1/0/1 result and added a boundary assertion for comparing `0x01` with `0xFF` (-1).
- Validation: `./tools/test-jcarray` and `./tools/check-vm-syntax` passed. The syntax check reports existing upstream warnings. ARM and RISC-V build wrappers are absent (both returned 127); hardware validation was intentionally skipped.
- Result: The implementation follows Java Card's byte primitive ordering; Oracle's `Util.arrayCompare` API specifies the ternary result but does not explicitly state signedness, so the signed interpretation follows Java's signed `byte` type.
- Decision: Keep signed comparison and continue with the remaining utility natives.
- Commit: `e088119`
- Artifacts/logs: no archived logs; focused test emitted `TEST:PASS jni_array_compare`

## 2026-10-09 — Implement native pattern array fill

- Step/experiment: `wp5-native-array-fill-pattern`
- Change: Implemented `arrayFillGeneric` and its non-atomic counterpart for byte arrays. They validate target/pattern offsets, repeat the pattern bytes from `patternOffset`, and return the destination end offset.
- Validation: `./tools/test-jcarray` passed a three-byte fill from a two-byte pattern, plus prior clear/copy/compare checks, under AddressSanitizer with leak detection disabled. `./tools/check-vm-syntax` passed with existing upstream warnings. Both target build commands returned 127 because `tools/build` is absent. Hardware validation was intentionally skipped.
- Result: The native fill helper now handles pattern repetition and bounds errors. Binding and semantics against the missing Java Card SDK implementation are not verified; persistent writes are not transactional.
- Decision: Continue mapping the remaining native methods against the shipped AlgTest calls and available VM APIs.
- Commit: `bb5d2b8`
- Artifacts/logs: no archived logs; focused test emitted `TEST:PASS jni_array_fill_pattern`

## 2026-10-09 — Exercise transient natives with an active context

- Step/experiment: `wp5-transient-native-runtime-test`
- Change: Extended the focused native test to call the transient byte-array factory, query transient state, and verify invalid lengths and clear events raise their VM exception codes. The test now uses the active `Context` and its heap directly.
- Validation: `./tools/test-jcarray` passed all transient, clear, copy, compare, fill, and factory checks under AddressSanitizer with leak detection disabled. `./tools/check-vm-syntax` and `./tools/test-jcvmarray` passed. Target builds were not rerun because this step changed tests only.
- Result: The active-context and primitive transient-factory paths now have a linked host runtime test, beyond translation-unit syntax coverage. This does not test Java-level exception dispatch or card lifecycle integration.
- Decision: Keep this host native regression harness and continue examining the remaining JCSystem behavior required by AlgTest.
- Commit: `64fe919`
- Artifacts/logs: no archived logs; test emitted the PASS records above

## 2026-10-09 — Cover all primitive transient factories

- Step/experiment: `wp5-transient-native-factory-coverage`
- Change: Extended the linked native test to exercise boolean, byte, short, and int transient factories with both clear events, and verify `isTransient(null)` raises `NullPointerException`.
- Validation: `./tools/test-jcarray` passed the full linked ASan suite, including factory type/length metadata and null handling. Target builds and VM syntax checks were not rerun because only test code changed.
- Result: Every implemented primitive transient factory and the null query path now have runtime-level host coverage. Object arrays and Java exception dispatch remain outside this step.
- Decision: Continue with native system-memory reporting and the residual AlgTest API surface.
- Commit: `bacd8fc`
- Artifacts/logs: no archived logs; test emitted the PASS records above

## 2026-10-09 — Report no pending applet selection

- Step/experiment: `wp5-selecting-applet-static-entry`
- Change: Implemented `selectingApplet()` to return `FALSE` while the VM has no applet selection lifecycle or pending-select state.
- Validation: `./tools/test-jcarray` passed its linked ASan suite, including the selecting-applet fallback. `./tools/check-vm-syntax` passed with existing upstream warnings. ARM and RISC-V build wrappers are absent (both returned 127); hardware validation was intentionally skipped.
- Result: Calls no longer raise `NotYetImplemented` in the current static-entry execution model. The result must become stateful when applet selection is added.
- Decision: Keep the conservative fallback and continue with remaining required natives and lifecycle integration.
- Commit: `3446422`
- Artifacts/logs: no archived logs; test emitted `TEST:PASS jni_selecting_applet_fallback`

## 2026-10-09 — Use the basic logical channel fallback

- Step/experiment: `wp5-basic-channel-fallback`
- Change: Implemented `getAssignedChannel()` to return logical channel 0, the basic channel used by the current single-channel transport.
- Validation: `./tools/test-jcarray` passed its linked ASan suite, including the channel fallback. `./tools/check-vm-syntax` passed with existing upstream warnings. ARM and RISC-V build wrappers are absent (both returned 127); hardware validation was intentionally skipped.
- Result: Basic-channel calls no longer raise `NotYetImplemented`. Additional logical channels remain unsupported.
- Decision: Keep channel 0 as the only supported channel and continue with remaining runtime integration gaps.
- Commit: `657e589`
- Artifacts/logs: no archived logs; test emitted `TEST:PASS jni_basic_channel_fallback`

## 2026-10-09 — Restore user source files to untracked state

- Step/experiment: `preserve-provided-source-index`
- Change: The handoff finalization commit `bff7c23` unintentionally included 63 pre-existing untracked CHOUPI source files due to stale staged entries. Removed only those paths from the index while retaining their working-tree contents; no source files were rewritten.
- Validation: Compared Git blob hashes for all 63 paths against `bff7c23`; all 63 matched with zero mismatches. The current implementation files and progress records remain committed separately.
- Result: The supplied source files are preserved in the worktree and removed from the repository commit; the accidental commit remains in history followed by this corrective commit.
- Decision: Continue with the user source trees untracked and only stage files explicitly belonging to implementation steps.
- Commit: `ae783a2`
- Artifacts/logs: no archived logs; 63 content hashes matched

## 2026-10-09 — Report integrity arrays as unsupported

- Step/experiment: `wp5-integrity-array-capability`
- Change: Implemented `isIntegritySensitiveArraysSupported()` to return `FALSE` because this runtime has no integrity-array storage or verification mechanism. Added a linked host assertion for the capability result.
- Validation: `./tools/test-jcarray` passed, including `TEST:PASS jni_integrity_arrays_capability`. `./tools/check-vm-syntax` passed with existing warnings in upstream headers. `./tools/build arm` and `./tools/build riscv` both returned 127 because `tools/build` is absent. Hardware validation was intentionally skipped.
- Result: Capability probing now reports the runtime's actual support level without throwing `NotYetImplemented`; integrity-sensitive array operations remain unavailable.
- Decision: Keep integrity-array construction and operations unsupported until the backing verification feature is designed. Continue mapping remaining native and lifecycle gaps.
- Commit: `32c88dd`
- Artifacts/logs: no archived logs

## 2026-10-09 — Query integrity sensitivity conservatively

- Step/experiment: `wp5-integrity-sensitive-query`
- Change: Implemented `isIntegritySensitive(Object)` to throw `NullPointerException` for null and return `FALSE` for non-null objects because the VM does not create or track integrity-sensitive arrays. Added linked host checks for both cases.
- Validation: `./tools/test-jcarray` passed, including `TEST:PASS jni_integrity_sensitive_query`. `./tools/check-vm-syntax` passed with existing upstream header warnings. `./tools/build arm` and `./tools/build riscv` both returned 127 because `tools/build` is absent. Hardware validation was intentionally skipped.
- Result: The native query now follows Java Card's null and ordinary-object behavior. Sensitive-result side-channel tracking remains unavailable.
- Decision: Keep array creation and integrity assertion operations unsupported. Continue remaining VM-native and lifecycle work.
- Commit: `3ef6da7`
- Artifacts/logs: no archived logs

## 2026-10-09 — Install repository-local RP2350 toolchains

- Step/experiment: `wp1-local-toolchain-and-board-identification`
- Change: Installed tools only under ignored `.local-toolchain/`: CMake 4.2.3, Ninja 1.13.2, ARM GCC 14.2.1 with Newlib/C++ runtime, Raspberry Pi RISC-V GCC 16.1.0, Rust stable 1.99.0 with `thumbv8m.main-none-eabihf`, Pico SDK 2.3.1 and pinned submodules, pioasm, and picotool 2.3.1. Added `tools/env` to select the isolated environment and `.agent/TOOLCHAIN.md` with setup details. Updated the plan to use user-selected JCAlgTest v1.8.2 and the board-reported 2 MiB flash size.
- Validation: `bash -n tools/env` passed. Through `tools/env`, the Rust target list included `thumbv8m.main-none-eabihf`; CMake/Ninja/Rust and both compiler target prefixes reported the installed versions/targets; `picotool version` reported v2.3.1. `./tools/env ./tools/doctor` found host tools and Pico SDK but returned 1 for missing CHOUPI OS/API/ROM-mask source trees. Both build wrapper calls returned 127 because `tools/build` is absent. Read-only `picotool info -a` succeeded under reviewed escalation and reported RP2350 A4, QFN60, 2048 KiB, BOOTSEL; no firmware was written. The flash capacity has not been independently measured.
- Result: Compilation and upload binaries are present locally (5.9 GiB extracted), but this checkout still lacks build wrappers and the documented `pico2_trng` target; no firmware build is possible yet. The user-selected v1.8.2 CAP and sources are preserved.
- Decision: Keep all tools inside the repository, use the reported 2 MiB as a working build configuration pending physical confirmation, and implement the missing build project/wrappers before flashing.
- Commit: `db99639`
- Artifacts/logs: `.agent/TOOLCHAIN.md`; local SDK commit `079c6f39023649b154152db30f1d781e884879bc`; release archive checksums in `.agent/TOOLCHAIN.md`; no serial log or firmware artifact

## 2026-10-10 — Bring up RP2350 USB serial smoke cycle

- Step/experiment: `wp1-rp2350-board-smoke`
- Change: Added a Pico SDK `pico2_trng` smoke target and repository wrappers for ARM/RISC-V builds, guarded flashing, and bounded USB CDC capture. Configured the reported 2 MiB flash size and repaired the relocated ARM Newlib include/library paths in the local build wrapper.
- Validation: `bash -n tools/common.sh tools/build tools/cycle tools/monitor` passed. `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` passed with `-Wall -Wextra -Werror`; picotool verified the `rp2350-arm-s` and `rp2350-riscv` UF2 image families. Final `./tools/env ./tools/cycle arm` returned 0 after rebuilding both architectures, flashing ARM, and capturing 8 seconds of serial output. Earlier cycle attempts exposed a one-shot output timing issue, CRLF log matching, and a transient serial-node permission race; the firmware now repeats its result and the wrapper normalizes CRLF and retries the node.
- Result: The connected RP2350 A4 QFN60 booted the ARM image and emitted repeated `TEST:PASS random_stream_varies` and `TEST:SUMMARY pass=1 fail=0` records. This is only a small variation check for `get_rand_32()`, not an entropy-quality test or CHOUPI validation. Saved the prior full 2,097,152-byte flash image before replacement; SHA-256 `49a650c2093df2e2d528ec4ef9adce788db95133c7303b5f96d852d8b5ecc078`. The reported flash capacity is not independently measured.
- Decision: Hardware communication, ARM/RISC-V smoke builds, flashing, and finite serial capture are established. Continue with CHOUPI integration; its OS/API/ROM-mask submodules are still empty, and JCAlgTest v1.8.2 has not been run on the board.
- Commit: `9bd89df`
- Artifacts/logs: `artifacts/logs/cycle-arm-20261010T053019Z.log`; `artifacts/backups/flash-before-smoke-20261010.bin`

## 2026-10-10 — Restore pinned CHOUPI dependencies

- Step/experiment: `wp1-restore-source-dependencies`
- Change: Cloned the declared CHOUPI OS, Java Card API, and ROM-mask mirrors into their previously empty directories and checked out the exact plan revisions: `654f374067b2e7c1571fec1410adcd1e41a60c96`, `d490faf358d4740def8eaf60017d5cc367f7383e`, and `ac9f75253d476a8b2a838d459727df6e7a4a5107`. Kept all checkouts within the pre-existing user-provided, untracked `choupi/` tree.
- Validation: Confirmed each nested Git HEAD equals its pinned full SHA and its worktree is clean. `./tools/env ./tools/doctor` passed all required host/Pico SDK and CHOUPI dependency path checks. Maven remains unavailable, so ROM-mask generation and full CHOUPI compilation were not run.
- Result: The dependencies blocking CHOUPI configuration are now present at the expected revisions. The OS still declares an uninitialized STM32CubeF4 submodule, which is tied to its legacy STM32 target.
- Decision: Continue with Maven setup and the first CHOUPI ARM integration build; keep the root smoke firmware unchanged until an integrated image passes both required builds.
- Commit: `654f374067b2e7c1571fec1410adcd1e41a60c96`, `d490faf358d4740def8eaf60017d5cc367f7383e`, `ac9f75253d476a8b2a838d459727df6e7a4a5107` (pinned source checkout commits; source remains in the provided untracked tree)
- Artifacts/logs: no build artifacts; dependency repositories are nested under `choupi/os`, `choupi/javacard-sdk/javacard-api`, and `choupi/javacard-sdk/rommask`

## 2026-10-10 — Complete local Java build prerequisites

- Step/experiment: `wp1-local-java-card-build-tools`
- Change: Added repository-local Maven 3.10.0 and Temurin JDK 8u504 details to toolchain documentation, routed Maven's artifact cache inside `.local-toolchain/m2`, and extended `tools/doctor` to check the pinned CAP parser and JC 3.0.5u3 kit.
- Validation: `bash -n tools/env tools/common.sh tools/doctor` and `git diff --check` passed. `./tools/env mvn --version` reported Maven 3.10.0 on host Java 21. With `JAVA_HOME=$PWD/.local-toolchain/java8` and its `bin` prepended to `PATH`, `./tools/env ant -f choupi/javacard-sdk/javacard-api/build.xml build` completed `BUILD SUCCESSFUL` (upstream converter warnings about prior minor exports and int support were non-fatal). `./tools/env ./tools/doctor` reports all host tools, SDK paths, top-level CHOUPI sources, and the JC 3.0.5u3 kit, then exits 1 for missing `rommask/capmap-free/pom.xml`. A ROM-mask Maven build attempt was blocked resolving Maven Central because DNS/network access is unavailable; its plugin/dependency build therefore remains unvalidated. No firmware was flashed in this step.
- Result: The local Java Card API build works and Maven no longer targets a cache outside the repository. ROM-mask generation still cannot proceed without the pinned CAP parser and network resolution for Maven artifacts.
- Decision: Continue by locating the exact `capmap-free` source or implementing a compatible local CAP parser, then build the ROM mask and begin the CHOUPI ARM integration build.
- Commit: `6100b96`
- Artifacts/logs: Maven and JDK archives under ignored `.local-toolchain/`; no archived build logs

## 2026-10-10 — Verify connected board and JCAlgTest CAP metadata

- Step/experiment: `wp1-hardware-monitor-and-algtest-cap-inspection`
- Change: Refreshed the real-board smoke status and recorded metadata from the user-selected JCAlgTest v1.8.2 CAP using the Java Card 3.0.5u3 CapDump utility; no source CAP or firmware was modified.
- Validation: `./tools/env ./tools/monitor --seconds 8` returned 0 from the connected Pico 2 and emitted 8 `TEST:PASS random_stream_varies` plus `TEST:SUMMARY pass=1 fail=0` pairs, with no `FAULT` or failure record. CapDump returned 0 and reported format 2.1, package AID `4A43416C6754657374`, applet AID `4A43416C675465737431`, imports `A0000000620001` v1.0 and `A0000000620102`, `A0000000620101`, `A0000000620201` v1.6. CAP size is 131,538 bytes; SHA-256 is `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`. ROM-mask import compatibility remains unverified because `capmap-free` and Maven Central resolution are unavailable.
- Result: The connected board remains responsive and runs the passing standalone smoke image. The supplied applet CAP is compatible with CHOUPI's stated CAP 2.1 parser format, but the import versions still need checking against generated exports.
- Decision: Continue toward the CHOUPI Cortex-M33 build; source for the pinned CAP parser or a compatible replacement remains the current prerequisite.
- Commit: `c922222`
- Artifacts/logs: `artifacts/logs/capdump-v182-20261010.txt` (SHA-256 `66f3d639dd3ea3beae3d4592132bac5f130666d47dc3fce5e0955c9a7355c35c`); bounded monitor output was observed in the command session and was not archived separately

## 2026-10-10 — Align Java Card exports with JCAlgTest imports

- Step/experiment: `wp5-jc305-import-export-alignment`
- Change: Updated both the EXP and JCA generation settings in the pinned Java Card API build so Framework exports as AID `A0000000620101` v1.6, Security as `A0000000620102` v1.6, and Crypto as `A0000000620201` v1.6. These now match the supplied JCAlgTest v1.8.2 CAP; Java Lang already matched at `A0000000620001` v1.0.
- Validation: `env JAVA_HOME=$PWD/.local-toolchain/java8 PATH=$PWD/.local-toolchain/java8/bin:$PATH ./tools/env ant -f choupi/javacard-sdk/javacard-api/build.xml clean build` completed `BUILD SUCCESSFUL` with converter warnings only (missing previous-minor exports and int-support notices). Inspected all four generated `.jca` headers under the ignored API build tree and confirmed exact AIDs and versions. Nested `git diff --check` passed. No Pico firmware was changed or flashed.
- Result: Corrected the three mismatched package exports which would have prevented ROM-mask import resolution for the selected CAP. Full ROM-mask validation remains blocked by the absent `capmap-free` submodule and unavailable Maven Central DNS.
- Decision: Keep the local API patch on top of pinned base `d490faf358d4740def8eaf60017d5cc367f7383e`; next restore CAP parser/dependencies and generate the ROM mask.
- Commit: `d570ce3` (nested `javacard-api` checkout)
- Artifacts/logs: generated `.jca` outputs are ignored under `choupi/javacard-sdk/javacard-api/build/`; no archived build log

## 2026-10-10 — Refresh board and architecture build status

- Step/experiment: `wp1-refresh-board-and-build-status`
- Change: Corrected the handoff to distinguish the board's initial BOOTSEL discovery from its current smoke firmware; refreshed both ISA builds after the local toolchain wrapper changes.
- Validation: `./tools/env ./tools/build arm` returned 0 and picotool validated the `rp2350-arm-s` UF2. `./tools/env ./tools/build riscv` returned 0 and picotool validated the `rp2350-riscv` UF2. No flash operation was needed; the Pico 2 remains on the ARM smoke firmware that passed the bounded serial monitor.
- Result: Both compilation/upload image families still build correctly through the repository wrappers, and the current board state is recorded accurately.
- Decision: Continue toward a CHOUPI image after resolving the CAP parser and Maven dependency blockers; do not replace the passing smoke firmware before an integrated image passes both build gates.
- Commit: `12dc94b`
- Artifacts/logs: Existing validated UF2s under ignored `build/arm/` and `build/riscv/`; board smoke log remains `artifacts/logs/cycle-arm-20261010T053019Z.log`

## 2026-10-10 — Flash ARM smoke image to connected Pico 2

- Step/experiment: `wp1-flash-connected-pico2-arm`
- Change: Rebuilt both target images and flashed the ARM smoke firmware to the connected RP2350 Pico 2 using the repository cycle wrapper.
- Validation: `./tools/env ./tools/cycle arm` returned 0. ARM and RISC-V builds passed, picotool recognized `rp2350-arm-s`, and the wrapper flashed then captured 8 seconds from `/dev/ttyACM0`. Serial output contained eight `TEST:PASS random_stream_varies` and `TEST:SUMMARY pass=1 fail=0` pairs, with no `TEST:FAIL` or `FAULT`.
- Result: The physical board booted the ARM image and passed its bounded random-stream variation smoke check. This does not validate entropy quality, CHOUPI, or JCAlgTest.
- Decision: Keep the board on the passing smoke firmware while CHOUPI ROM-mask generation/build remains blocked on the missing `capmap-free` source and Maven dependency resolution.
- Commit: `4ebfe1b`
- Artifacts/logs: `artifacts/logs/cycle-arm-20261010T060906Z.log` (SHA-256 `7bccbc03281e0f93c2e4068856903570b648bb44f51f452576162222072b7dd9`)

## 2026-10-10 — Assess CHOUPI and JCAlgTest deployment prerequisites

- Step/experiment: `wp1-choupi-cap-deployment-readiness`
- Change: Checked the pinned ROM-mask parser, CHOUPI build configuration, board target, and selected v1.8.2 CAP before attempting deployment.
- Validation: `./tools/env ./tools/doctor` returned 1 because `rommask/capmap-free/pom.xml` is missing. Fetching the exact `capmap-free` submodule commit `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4` failed because Bitbucket requested credentials. `./tools/env cmake -S choupi -B build/choupi-pc -DCHOUPI_TARGET_PC=ON -DCHOUPI_OS_DEBUG=ON -DCHOUPI_JCVM_DEBUG=OFF` failed at configuration because Boost headers and `program_options` are absent. Source inspection confirmed CHOUPI's existing targets are host and STM32; the OS has host and STM32 architectures only. The Pico build wrapper still builds only `pico2_trng` smoke firmware. No CHOUPI image was built, no CAP installation path exists, and no board flash was attempted.
- Result: The requested CHOUPI/CAP deployment is not currently buildable or installable. The board remains on the previously validated standalone ARM smoke firmware.
- Decision: Restore/replace the inaccessible CAP parser, establish a Pico 2 CHOUPI port, and implement APDU/CCID lifecycle and CAP loading before uploading the CAP. The PC build also needs Boost or a dependency-free CLI.
- Commit: `a5de012`
- Artifacts/logs: no new archived logs; diagnostics are available in the command session

## 2026-10-10 — Restore pinned capmap-free from GitHub mirror

- Step/experiment: `wp1-restore-capmap-free-mirror`
- Change: Initialized the ROM-mask `capmap-free` submodule using the user-provided GitHub mirror `https://github.com/pancanaka/capmap-free`.
- Validation: The mirror checkout is exactly the ROM-mask gitlink commit `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4` with a clean submodule worktree. `./tools/env ./tools/doctor` returned 0 and reports all required host, SDK, CHOUPI, API, ROM-mask, CAP-parser, and Java Card kit paths present. `./tools/env mvn -f choupi/javacard-sdk/rommask/capmap-free/pom.xml install -DskipTests` failed because Maven Central DNS resolution is unavailable and `maven-assembly-plugin` metadata cannot be fetched.
- Result: The missing parser source is restored at the exact pinned revision. Its Java artifacts are not yet installed in the repository-local Maven cache, so ROM-mask generation remains unvalidated.
- Decision: Continue resolving Maven build dependencies, then generate and inspect the ROM mask with the selected JCAlgTest v1.8.2 CAP and current API JCA exports.
- Commit: `3cc059a`
- Artifacts/logs: source at `choupi/javacard-sdk/rommask/capmap-free` (nested submodule commit `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4`); no build log archived

## 2026-10-10 — Generate source-derived JCAlgTest ROM mask

- Step/experiment: `wp5-jcalgtest-rommask-generation`
- Change: Built and packaged capmap-free and the ROM-mask generator, converted the supplied v1.8.2 `src305` applet sources to JCA with the JC 3.0.5u3 converter, and fixed two JCA parser incompatibilities exposed by this applet: label indexes were truncated to signed bytes and uppercase `TRUE`/`FALSE` field names tokenized as boolean literals.
- Validation: `./tools/env mvn -f choupi/javacard-sdk/rommask/capmap-free/pom.xml install -DskipTests` passed. `./tools/env mvn -f choupi/javacard-sdk/rommask/pom.xml package assembly:single -DskipTests` passed with existing unchecked-conversion warnings; Maven tests were skipped. The Ant JCAlgTest source build passed, and the Java Card converter reported zero errors and zero warnings for JCA conversion. ROM-mask generation then parsed all 21 JCA packages, resolved imports, and returned 0, writing 56,776 bytes of content into a 524,300-byte padded image. Generated native header size is 22,135 bytes and reports 34 native methods. The JCA imports match all four selected CAP imports. `git diff --check` passed; no firmware build or board flash was attempted.
- Result: A ROM-mask image containing the API and source-derived JCAlgTest applet is generated. The source-rebuilt CAP is 131,545 bytes with SHA-256 `daccd8838322995f02a5200d9ebd61a4bf237d293031875424813acd7a7df68b`, which differs from the supplied CAP (131,538 bytes; SHA-256 `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`). Thus this validates source-based pre-masking, not loading the exact supplied CAP.
- Decision: Continue the RP2350 CHOUPI integration and card-install path; the generated ROM mask is not itself board firmware, and the supplied CAP has not been installed on the board.
- Commit: `af1091315fd7fd17baeae773ecf7553fd785ae37` (nested ROM-mask repository); progress record in `bff0a5d`
- Artifacts/logs: `build/choupi-rommask/flash-algtest.bin` (SHA-256 `93a6678b3288e4ee92de2fc8df4e40b9f0bef2b297f1ceae3b761ce3a71e35e6`); `build/choupi-rommask/jni-algtest.hpp` (SHA-256 `935e124965e192b871a0782276845c02736220e496f428d46cfddbd0170e35a1`); `build/choupi-algtest/classes/algtest/javacard/algtest.jca` (SHA-256 `a7915e412d1fb80d389bfa722b5cb70df859db1243230b0a2a9751938868bdd8`)

## 2026-10-10 — Repair ROM-mask project build wrapper

- Step/experiment: `wp1-rommask-maven-wrapper`
- Change: Updated the ROM-mask Makefile from the obsolete Maven `assembly:assembly` goal to `package assembly:single`.
- Validation: `./tools/env make -C choupi/javacard-sdk/rommask` returned 0. It installed all four capmap modules and packaged `rommask-1.0-jar-with-dependencies.jar`. Maven reports no ROM-mask tests to run; dependency unchecked-conversion warnings remain.
- Result: The documented ROM-mask project build works with the installed Maven version and local cache.
- Decision: Use this `make` path in later ROM-mask integration builds; device upload remains blocked on the separate RP2350 CHOUPI and card-manager work.
- Commit: `f2a3f0e8eb945e3fa027f359be78844cf1306f42` (nested ROM-mask repository); progress record in `b4485e9`
- Artifacts/logs: rebuilt nested artifact `choupi/javacard-sdk/rommask/target/rommask-1.0-jar-with-dependencies.jar`; no separate log archived

## 2026-10-10 — Persist capmap-free GitHub mirror URL

- Step/experiment: `wp1-persist-capmap-free-mirror`
- Change: Changed the ROM-mask nested submodule URL from inaccessible Bitbucket to the user-provided GitHub mirror so a fresh checkout can fetch the parser source.
- Validation: Ran `git submodule sync capmap-free`; verified the submodule remote is `https://github.com/pancanaka/capmap-free.git` and its checkout remains exact pinned commit `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4`. `./tools/env ./tools/doctor` returned 0. `./tools/env make -C choupi/javacard-sdk/rommask` passed in the preceding step. No firmware was flashed.
- Result: Both source and checkout metadata use the public GitHub mirror; a fresh submodule initialization can fetch the parser without Bitbucket credentials.
- Decision: Continue RP2350 CHOUPI target and CAP installation work. The current source-derived image is not firmware and does not install the supplied CAP.
- Commit: `2302f926528a9f9001124d8f4b5cf6cd98e1de5c` (nested ROM-mask repository); root record pending
- Artifacts/logs: Submodule source `choupi/javacard-sdk/rommask/capmap-free` at `c94b09a128eaa7c4d20d53d9c24e4c12f810caf4`; no new log

## 2026-10-10 — Refresh Pico 2 baseline after parser restoration

- Step/experiment: `wp1-post-capmap-board-refresh`
- Change: Rebuilt both repository smoke images and captured the connected Pico 2's current serial output after restoring the CAP parser dependencies; no firmware source changed.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` both returned 0 and picotool validated `rp2350-arm-s` and `rp2350-riscv` UF2s. `./tools/env ./tools/monitor --seconds 8` returned 0 and emitted eight `TEST:PASS random_stream_varies` plus `TEST:SUMMARY pass=1 fail=0` pairs, with no failure or fault records.
- Result: Both baseline architectures still build, and the board remains responsive on the previously flashed ARM smoke image. These checks do not build or run CHOUPI and do not install the supplied CAP.
- Decision: Preserve the smoke firmware until an integrated CHOUPI target and both build gates are ready; continue RP2350 runtime and transport implementation.
- Commit: `2a7a2017bd818d0f54e9d8d84cbfada430d4a825`
- Artifacts/logs: `artifacts/logs/monitor-after-capmap-20261010.log` (SHA-256 `9f4162fb209ebfb97a74d03517a55fe9d6e279b0354999ba7d7bbe7d6a76b5dd`)

## 2026-10-10 — Cross-compile CHOUPI VM core for RP2350

- Step/experiment: `wp1-rp2350-choupi-core-cross-compile`
- Change: Added a Pico SDK `choupi_vm_core` object target that compiles the platform-independent CHOUPI C++ sources, enables C++ exceptions, stages the generated source-derived `jni.hpp`, and makes the build wrapper fail if that header is missing.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` returned 0; both wrappers validated their RP2350 UF2 family. The core compiled all 34 C++ translation units for each target. A small set of upstream warning classes is suppressed for this compatibility build (`ignored-qualifiers`, `address-of-packed-member`, `unused-parameter`, `array-bounds`); warnings are therefore not claimed clean. `git diff --check` passed. No CHOUPI image was linked or flashed, and no CAP was installed.
- Result: CHOUPI VM C++ sources are cross-compilable by the installed RP2350 ARM and RISC-V toolchains. The produced UF2 remains the smoke application; the Rust OS, filesystem shim, startup, and APDU/card-manager path remain unintegrated.
- Decision: Link an ARM-only minimal CHOUPI firmware path next, then implement a CDC CAP loader or CCID card manager for the exact supplied v1.8.2 CAP.
- Commit: `601d099`
- Artifacts/logs: `artifacts/logs/choupi-core-arm-build.log` (SHA-256 `325a749aa6d71fb0217f94d3cfa60caa3d49ef4250f40db5f9f64834325323ba`); `artifacts/logs/choupi-core-riscv-build.log` (SHA-256 `646933b36ea464ea5df3f7d73cbeb48f74ca8b35c9b93210ddd085403aec59ae`)

## 2026-10-10 — Link CHOUPI Pico 2 bring-up target

- Step/experiment: `wp1-pico2-choupi-target-bringup`
- Change: Added an ARM Pico SDK CHOUPI image with the generated ROM-mask package image, USB CDC status output, a bounded RAM filesystem overlay, and a dedicated `cycle choupi` gate. The wrapper builds ARM and RISC-V before any flash. The source-derived ROM-mask image currently embeds the rebuilt v1.8.2 applet package.
- Validation: `./tools/env ./tools/cycle choupi` built both RP2350 architectures and picotool validated both UF2 families. It flashed the ARM CHOUPI image and captured 8 seconds from the connected board. The VM entered startup but returned `6F00`; the wrapper correctly returned failure because the log contains `TEST:FAIL choupi_runtime`. The trace reports `PUTFIELD_A` with object reference `0x0000`, value `0x0007`, followed by `NullPointerException` at PC `0x1004459F`. A preceding USB-only attempt failed before flash with `Failed to initialise libUSB`; the escalated hardware cycle succeeded in flashing and capture. `./tools/env ./tools/doctor` passed.
- Result: The RP2350 firmware target path is now present, links, flashes, and starts the CHOUPI interpreter. Runtime startup is not yet passing. The latest UF2 is 0x9C7E4 bytes of flash image end offset (picotool end address `0x1009C7E4`), SHA-256 `19436167e42047114c3a7628b406643ba87bbf45af3936d07e9fe8f09188c008`.
- Decision: Diagnose the invalid null receiver before claiming CHOUPI runtime readiness. No GlobalPlatformPro executable, PC/SC scanner, or TinyUSB CCID class is available on the host/SDK; the connected image exposes CDC only. The exact supplied CAP remains uninstalled. Its SHA-256 is `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`; the currently embedded source-rebuilt ROM package image differs from that CAP.
- Commit: `9694ac7`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T084216Z.log` (SHA-256 `26979714b539ccd026752e6ccfbb63d5d55ad5d2222d47aee94af765fa6670b7`); `build/arm/pico2_choupi.uf2`; supplied CAP `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`

## 2026-10-10 — Diagnose CHOUPI startup persistence capacity

- Step/experiment: `wp1-choupi-pico2-startup-storage-diagnostics`
- Change: Added bounded uncaught-exception and sparse interpreter progress records. Replaced the fixed per-file payload table in the Pico CDC filesystem shim with 2,048 metadata entries and a packed 128 KiB overlay arena; in-place writes update the arena and replacements compact existing records.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed, including UF2 family checks. `./tools/cycle choupi` failed its hardware gate. Earlier bounded captures identified `IOException` at `putstatic_a` because the original table filled at 24 entries; larger fixed tables filled at 48 and 128 entries, then 512. A dynamic allocation prototype emitted `*** PANIC *** Out of memory`. The packed arena build was flashed, but its 18-second cycle and a separate 60-second serial capture contained no CHOUPI runtime output. A later read-only USB/BOOTSEL recovery attempt left the RP2350 unresponsive to picotool; host enumeration then disappeared. The map places `__bss_end__` at `0x2003945c` (BSS begins at `0x200027d0`). `git -c core.whitespace=cr-at-eol diff --check` passed.
- Result: The original overlay file-count limit is confirmed as a cause of the 6F00 failure. The packed arena removes the observed small-table ceiling, but hardware startup is not validated and board/host USB recovery is unresolved. No `TEST:PASS` was observed; this is a failed hardware validation. The supplied JCAlgTest CAP remains uninstalled.
- Decision: Recover the board's USB/BOOTSEL connection, then capture the sparse VM heartbeat and diagnose any remaining startup or arena limit before implementing CAP installation transport.
- Commit: `0d1e516`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T094700Z.log` (SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`); `build/arm/pico2_choupi.uf2` (SHA-256 `8990a465f60f89b9baa724d1c05856605e77d7224e090f6f16b4a711f172c8b8`); ROM mask `build/choupi-rommask/flash-algtest.bin` remains source-derived, not the exact supplied CAP.

## 2026-10-10 — Expand startup overlay and skip absent null references

- Step/experiment: `wp1-choupi-overlay-4096`
- Change: Avoided creating overlay records for null references whose tags do not exist in ROM or overlay storage, while preserving explicit null writes for existing tags. Increased the packed overlay file index from 2,048 to 4,096 entries after the previous hardware capture reached the old ceiling.
- Validation: `./tools/build arm` and `./tools/build riscv` passed with their RP2350 UF2 checks. `./tools/cycle choupi` flashed the Pico 2 and captured the 18-second serial window, then returned failure because startup still hit the overlay limit.
- Result: The same startup workload reached 4,096 records and 20,754/131,072 bytes, then failed writing a 6-byte record at tag length 27 (`0214020101000000…`). Skipping absent null references did not change the dominant record growth. Runtime returned exception 5 at opcode `0x7F`, status `6F00`; no `TEST:PASS` was observed.
- Decision: Trace nested object/array persistence at deep tags to find why startup expands thousands of records before increasing capacity further. The supplied CAP remains uninstalled.
- Commit: `f9add8b`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T095555Z.log` (SHA-256 `5f0fe823120a2db9cfc7fd09598c2a17106942f82df07883a8c6688615fbd9e4`)

## 2026-10-10 — Compress nested CHOUPI persistence keys

- Step/experiment: `wp1-choupi-parent-compressed-overlay`
- Change: Replaced full tag copies in the Pico overlay index with compact hashes and packed records; compresses repeated field-index byte pairs and references nested paths through stable parent slots. Expanded overlay capacity to 16,384 entries and 256 KiB, raised tag buffers to 128 bytes, and stores per-instance persistent tags compactly to limit RAM growth. Updated record replacement accounting to use the original encoded key size.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Hardware cycles flashed CHOUPI. The first parent-compressed capture progressed beyond prior arena and tag limits but ended in `SecurityException id=29` at a nested persistent tag length of 61 (`artifacts/logs/cycle-choupi-20261010T101555Z.log`). Expanding tag paths to 128 built, but the next cycle emitted `*** PANIC *** Out of memory` shortly after BOOT (`artifacts/logs/cycle-choupi-20261010T101744Z.log`). The follow-up compact per-instance tag cache built for both architectures; two later cycle attempts emitted no serial data after `MONITOR:START` and failed the runtime gate (`101940Z` and `102236Z`). No `TEST:PASS` was observed.
- Result: Parent-key compression avoids the prior full arena and 64-byte tag guard, but CHOUPI startup still fails. After the silent captures picotool reported the RP2350 only as a USB serial device and could not read BOOTSEL metadata; a physical BOOTSEL reconnect is requested. Latest compact UF2 SHA-256 is `2a16a875055301c323888b7c20d5b0230fcd612ffd48e2f74fd05db0aeea04fe`.
- Decision: Re-establish stable BOOTSEL/serial access, capture the compact-cache image’s actual startup result, then measure and reduce the overlay arena if RAM remains the limiting factor. Continue only after both build gates pass before each flash. The exact JCAlgTest v1.8.2 CAP remains uninstalled.
- Commit: root `f2da673`; nested OS `f994d5e`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T101555Z.log` (SHA-256 `bd2241808eb99b562619366e36fdbef5030649495c7276922a8b985a44ed9f62`); `artifacts/logs/cycle-choupi-20261010T101744Z.log` (SHA-256 `b2f9490d81aacadc9c7458bd39197b3724d1f775550231efce5a6c85d6c230d7`); `artifacts/logs/cycle-choupi-20261010T101940Z.log` and `102236Z.log` (both SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`); `build/arm/pico2_choupi.uf2`

## 2026-10-10 — Archive parent-compression board captures

- Step/experiment: `wp1-choupi-parent-compression-capture-archive`
- Change: Archived the four finite CHOUPI cycle logs used to evaluate parent-key compression and compact tag storage.
- Validation: Recomputed SHA-256 for all four logs and confirmed the expected records: 61-byte path `SecurityException`; 128-byte buffer `Out of memory`; then two silent 18-second monitor captures. Both architecture build gates passed for the compact-tag image before its cycle.
- Result: The failed and silent hardware outcomes are preserved for follow-up. No successful CHOUPI startup or exact-CAP installation is claimed.
- Decision: Resume hardware diagnosis when the Pico 2 is back in BOOTSEL; next capture must distinguish USB enumeration from firmware startup and runtime output.
- Commit: root `87c1bbe`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T101555Z.log` (`bd2241808eb99b562619366e36fdbef5030649495c7276922a8b985a44ed9f62`); `artifacts/logs/cycle-choupi-20261010T101744Z.log` (`b2f9490d81aacadc9c7458bd39197b3724d1f775550231efce5a6c85d6c230d7`); `artifacts/logs/cycle-choupi-20261010T101940Z.log` and `102236Z.log` (`9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`)

## 2026-10-10 — Guard compressed overlay tag comparisons

- Step/experiment: `wp1-choupi-overlay-tag-guard`
- Change: Added parity and minimum-length checks before decoding compressed field-index pairs, and reject uncompressed 128-byte keys that cannot fit the packed record header.
- Validation: `./tools/build arm` and `./tools/build riscv` passed, including UF2 family checks. Hardware validation was skipped because `picotool info -a` reports no accessible BOOTSEL device and the last monitor capture had no CHOUPI serial output.
- Result: The overlay tag comparison cannot read a dangling pair from an odd-length hash collision candidate. Runtime behavior remains unverified.
- Decision: Keep both architecture build gates; resume the hardware cycle after physical BOOTSEL reconnect.
- Commit: root `9b73e3a`
- Artifacts/logs: none; current UF2 `build/arm/pico2_choupi.uf2`

## 2026-10-10 — Add aliases for recursive persistent object references

- Step/experiment: `wp1-choupi-persistent-cycle-alias`
- Change: Detect persistent object references that recur during serialization and store a compact alias to the object's canonical persistence tag. Alias reads reconstruct a persistent instance from that canonical record. The in-progress object tracker stores pointers only to reduce SRAM overhead.
- Validation: `./tools/build arm` and `./tools/build riscv` passed with UF2 family checks. The first hardware cycle emitted `*** PANIC *** Out of memory` after adding a full tag per recursion frame. After reducing tracker storage, a second cycle could not capture serial output; `./tools/env picotool info -a` and `lsusb` both reported libusb initialization failure, so runtime validation is incomplete. `git -c core.whitespace=cr-at-eol diff --check` passed.
- Result: Compile-time compatibility is confirmed. No passing CHOUPI runtime result is established. The latest built image SHA-256 is `a0682558fd222e51b7e93b434a46dea99a0887853ef05c73ee6378793e80fef8`.
- Decision: Reconnect the Pico 2 in BOOTSEL, verify enumeration, then rerun the hardware cycle and inspect alias behavior. The exact v1.8.2 CAP remains uninstalled; the firmware still exposes CDC without an APDU/CAP loader.
- Commit: root `654219f`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T110210Z.log` (SHA-256 `e6d1bd36833d7925933168533ad3ca86d17504970da2656b148030d81c514dd8`, OOM); `artifacts/logs/cycle-choupi-20261010T110427Z.log` (SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`, no output)

## 2026-10-10 — Mark serialized instances persistent in memory

- Step/experiment: `wp1-choupi-persistent-instance-mode`
- Change: When assigning a canonical persistence tag to a newly serialized instance, also set the VM object's persistent flag so subsequent field writes use the stored tag.
- Validation: `./tools/build arm` and `./tools/build riscv` passed with UF2 family checks. Hardware validation was skipped because the previous cycle left the Pico unenumerated; `picotool info -a` and `lsusb` report libusb initialization failure.
- Result: Both target builds include the updated object mode. No runtime result was captured. UF2 SHA-256: `9cc85d9051fb9f55d52682d4f772ea16316d144fb123686381415203bad85fc6`.
- Decision: Reconnect the Pico in BOOTSEL and rerun the CHOUPI cycle before accepting the alias implementation. The exact JCAlgTest v1.8.2 CAP remains uninstalled.
- Commit: root `06fdf87`
- Artifacts/logs: current image `build/arm/pico2_choupi.uf2`; no new hardware log

## 2026-10-10 — Check JCAlgTest CAP package dependencies

- Step/experiment: `wp1-jcalgtest-cap-import-compatibility`
- Change: Inspected the exact `AlgTest_v1.8.2_jc305.cap` header, applet metadata, import component, and the generated CHOUPI ROM package metadata to determine whether its linked package versions are represented.
- Validation: Recomputed the CAP SHA-256 and compared each import AID/version against generated JCA package metadata. CAP imports are `java/lang` AID `A0000000620001` v1.0, `javacard/framework` AID `A0000000620101` v1.6, `javacard/security` AID `A0000000620102` v1.6, and `javacardx/crypto` AID `A0000000620201` v1.6; generated package JCA metadata has matching AIDs and versions. Hardware check `./tools/env picotool info -a` failed with `ERROR: Failed to initialise libUSB`; no flash or installation was attempted. Host inspection found no GlobalPlatformPro, GPShell, PC/SC service, or smart-card reader tooling, and CHOUPI firmware still exposes CDC without APDU/CCID or a CAP loader.
- Result: Dependency metadata is version-compatible at the JCA package-description level, but this does not establish that CHOUPI resolves all imported methods at runtime or can install the CAP. Exact CAP SHA-256: `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`. The board remains unavailable and CAP remains uninstalled.
- Decision: Restore USB BOOTSEL enumeration, validate CHOUPI startup, then add or select an APDU/CAP installation path before claiming installation.
- Commit: root `b1d6852`
- Artifacts/logs: `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`; generated package metadata under `build/choupi-algtest/rommask-input/`; no new hardware log

## 2026-10-10 — Flash current CHOUPI image on Pico 2

- Step/experiment: `wp1-choupi-current-image-hardware-cycle`
- Change: Ran the repository CHOUPI cycle with USB device access after the normal sandbox could not initialize libUSB. `picotool load -f` was asked to reboot the RP2350 from CDC into BOOTSEL; the cycle command returned success from the load stage and proceeded to monitor.
- Validation: The cycle rebuilt and passed the ARM and RISC-V image gates. Its 18-second CDC monitor capture failed the runtime-output gate. The log contains the reboot request and monitor start but no flash progress/completion message, CHOUPI output, `TEST:PASS`, or `TEST:SUMMARY`. A follow-up read-only query saw an RP2350 USB serial interface, but did not establish BOOTSEL mode or which image was executing. Candidate UF2 SHA-256: `9cc85d9051fb9f55d52682d4f772ea16316d144fb123686381415203bad85fc6`.
- Result: The load command returned zero, but the capture SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c` matches earlier silent captures and does not prove the image was written or that CHOUPI started. Exact JCAlgTest v1.8.2 CAP remains uninstalled because no working APDU/CCID transport or CAP loader exists.
- Decision: Diagnose why the post-flash CDC capture is silent before claiming a working CHOUPI runtime; then add an installation transport/loader and install the exact CAP.
- Commit: root `8c9b584`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T111732Z.log`; `build/arm/pico2_choupi.uf2`

## 2026-10-10 — Reduce static CHOUPI persistence arena

- Step/experiment: `wp1-choupi-arena-sram-reduction`
- Change: Reduced the static Pico overlay index from 16,384 to 8,192 entries and the byte arena from 256 KiB to 128 KiB to give the Rust VM more dynamic heap.
- Validation: `./tools/cycle choupi` rebuilt and passed ARM and RISC-V gates; the load command returned zero and the cycle captured 18 seconds. The capture had no output. A separate 8-second capture with pyserial DTR asserted was also silent. The ARM ELF `.bss` fell from 462,236 to 232,860 bytes; `.data` remains 9,920 bytes. The device later appeared as RP2350 CDC, but its running image was not verified.
- Result: This cycle does not establish whether the reduced image booted; no `BOOT` marker or runtime status was captured. The monitor log SHA-256 is `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`, identical to previous silent captures. Increasing available heap by about 224 KiB did not restore serial output.
- Decision: Keep this as a diagnostic baseline only; isolate the CDC capture path with the small TRNG firmware, then trace CHOUPI startup before further persistence changes.
- Commit: root `5f3a72b`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T112137Z.log`; current ARM image `build/arm/pico2_choupi.uf2` (SHA-256 `0a265f608a5c5ea44f90bcb46ced51a308b8d909967ffd8071dd89ef6f6d675d`)

## 2026-10-10 — Isolate Pico USB capture with TRNG image

- Step/experiment: `wp1-pico-trng-usb-capture-diagnostic`
- Change: Ran the authorized ARM TRNG cycle to determine whether silent CHOUPI output was caused by its runtime or by the board/CDC capture path.
- Validation: The cycle rebuilt both architectures successfully and its ARM flash command returned without error, but the 8-second serial capture contained no `BOOT` or `TEST` lines and the cycle failed. The log notes a blank USB serial number while waiting for reboot. Read-only `picotool info -a` afterward still found only the RP2350 CDC interface, not BOOTSEL; explicit `picotool reboot -f -u` reported that it requested BOOTSEL, but a follow-up query still found CDC.
- Result: The known-good TRNG capture path also failed, so the silence cannot currently be attributed to the CHOUPI VM. The host can enumerate the USB descriptors, but device status requests intermittently return `Resource temporarily unavailable`; the image currently executing on the Pico is not verified.
- Decision: Reconnect the Pico physically while holding BOOTSEL, confirm `boot type: bootsel`, then reflash and capture. Do not attempt another software-only flash until bootloader mode is verified.
- Commit: root `d7c3dd0`
- Artifacts/logs: `artifacts/logs/cycle-arm-20261010T112446Z.log` (SHA-256 `62457db617d9013e2c91a67bf6cfb2a74b8600d7c50b952b091d8dd9fc4e7e13`)

## 2026-10-10 — Prepare GlobalPlatformPro installation client

- Step/experiment: `wp1-globalplatformpro-cap-client`
- Change: Cloned official GlobalPlatformPro tag `v26.06.04` (source commit `5f3f4a2c7f6ae147da1351955deaeec9872eff47`) into the ignored repository toolchain and built the self-contained CLI JAR with the repository Maven/Java environment.
- Validation: `./tools/env mvn -f .local-toolchain/globalplatformpro/pom.xml -DskipTests package` completed with `BUILD SUCCESS`; Maven skipped test execution. `java -jar .../gp.jar --help` lists `--install`, and `--version` reports GlobalPlatformPro 26.06.04 on Java 21.0.12.1. JAR SHA-256: `38b464deabb20f48ba4fa1379354e41e913f635a537c9ea5cd2b1e919e0f908c`.
- Result: A ready CLI exists at `.local-toolchain/globalplatformpro/tool/target/gp.jar`. The host has no PC/SC socket or reader, and the Pico exposes CDC, so GPPro cannot yet send APDUs to this device. Exact CAP remains uninstalled.
- Decision: Keep the CLI ready; continue with CHOUPI startup and establish an APDU transport/reader before installation.
- Commit: root `d9ac44d`
- Artifacts/logs: `.local-toolchain/globalplatformpro/tool/target/gp.jar` (ignored toolchain artifact; SHA-256 above)

## 2026-10-10 — Clarify Pico flash evidence

- Step/experiment: `wp1-flash-evidence-wording-correction`
- Change: Corrected the handoff and earlier progress records to distinguish a successful `picotool` command exit from independently verified UF2 flash completion.
- Validation: Reviewed the three latest silent cycle logs; each lacks the flash progress/completion output present in older confirmed loads. `git diff --check` passed.
- Result: Hardware notes now state that the running image is uncertain until BOOTSEL is confirmed and a load is evidenced.
- Decision: Physically re-enter BOOTSEL and rerun the requested image cycle before reporting flash success.
- Commit: root `4eae0e1`
- Artifacts/logs: corrected evidence references `artifacts/logs/cycle-choupi-20261010T111732Z.log`, `112137Z.log`, and `cycle-arm-20261010T112446Z.log`

## 2026-10-10 — Archive current Pico cycle captures

- Step/experiment: `wp1-current-pico-cycle-log-archive`
- Change: Added the three newly captured CHOUPI and TRNG cycle logs to version control for reproducible hardware history.
- Validation: Recomputed each SHA-256. The two silent CHOUPI logs are byte-identical at `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`; the silent TRNG log is `62457db617d9013e2c91a67bf6cfb2a74b8600d7c50b952b091d8dd9fc4e7e13`.
- Result: All current failed captures are preserved without adding unrelated pre-existing logs.
- Decision: Resume hardware validation after a physical BOOTSEL reconnect and verify flash completion before interpreting serial output.
- Commit: root `99978ab`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T111732Z.log`, `artifacts/logs/cycle-choupi-20261010T112137Z.log`, `artifacts/logs/cycle-arm-20261010T112446Z.log`

## 2026-10-10 — Localize CHOUPI startup allocation panic

- Step/experiment: `wp1-choupi-startup-allocation-phase-diagnostic`
- Change: Added serial stage markers immediately before and after `fs_init()` and around the CHOUPI VM entry point so the next successful flash identifies which startup phase triggers the Pico heap panic.
- Validation: `./tools/cycle choupi` passed ARM and RISC-V build gates. Its flash attempt could not re-enter BOOTSEL and so did not write the marker image; the 18-second capture repeated an earlier panic log. `./tools/env picotool reboot -f -u` returned a BOOTSEL request but `picotool info -a` found no BOOTSEL device. `git diff --check` has pre-existing CRLF whitespace reports in `choupi/src/jc_types/jc_array.cpp` and `choupi/src/jni.cpp`; no diagnostics were reported for this step's source or markdown changes.
- Result: The first verified BOOTSEL cycle wrote the CHOUPI image (visible 100% flash progress) and captured `BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi` followed by `*** PANIC ***` / `Out of memory`. Pico SDK source maps that text to `pico_malloc`'s allocation guard. The stage-marker image was built (UF2 SHA-256 `b49438a577524d767b9c859fa2a0de49475c2bce461f29bc97b5c11b04322c65`) but remains unflashed; allocation phase and requested size are not yet known.
- Decision: Reconnect while holding BOOTSEL, verify `boot type: bootsel`, then flash the marker image and identify the failing phase. CAP remains uninstalled.
- Commit: `ffa3ddf`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T115001Z.log` (SHA-256 `e01b623fc94299571e1f72fde115acf6b0a7e3bbbe853f6ae9bc9954775c2903`); `artifacts/logs/cycle-choupi-20261010T115247Z.log` (identical to prior silent capture, SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`)

## 2026-10-10 — Add Pico heap high-water diagnostics

- Step/experiment: `wp1-choupi-pico-heap-highwater-diagnostic`
- Change: Enabled `PICO_DEBUG_MALLOC` on the Pico SDK allocator used by the ARM CHOUPI target with a `0x20070000` low-water threshold, so failed requests and allocations near the stack limit are reported without logging normal low-address allocations.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed after correcting the SDK interface-target compile-definition scope. `./tools/env ninja -C build/arm -t commands` confirmed the allocator compile command carries `PICO_DEBUG_MALLOC=1` and the threshold. No hardware flash was attempted because the Pico had left BOOTSEL; the stage-marker image is also awaiting the user-requested physical BOOTSEL reconnect. UF2 SHA-256: `19532c092fa19bcc7dd1e43e8d5aa84450755e112a02225ec12b697836f3cc9b`.
- Result: A diagnostic image is ready, but the failing allocation and startup phase remain unmeasured until this image is flashed and captured.
- Decision: Verify the board in BOOTSEL after the pending physical reconnect, then flash and inspect finite serial output.
- Commit: `fea69ed`
- Artifacts/logs: `build/arm/pico2_choupi.uf2` SHA-256 `19532c092fa19bcc7dd1e43e8d5aa84450755e112a02225ec12b697836f3cc9b`; build logs not archived

## 2026-10-10 — Capture CHOUPI heap exhaustion on Pico 2

- Step/experiment: `wp1-choupi-pico-heap-panic-localization`
- Change: Flashed the heap high-water and runtime-stage diagnostic image on the physically reconnected Pico 2 and captured its finite startup output.
- Validation: `./tools/cycle choupi` passed ARM and RISC-V build gates, showed 100% ARM UF2 flash progress, and recorded an 18-second board capture. `fs_init()` reported done; `runtime()` started; the cycle correctly failed its runtime acceptance gate on Pico SDK `*** PANIC *** / Out of memory`.
- Result: One 36,870-byte allocation succeeded at `0x20068FB0..0x20071FB6`; the next 36,870-byte allocation failed. Exact allocating call site remains unknown. Log SHA-256: `9846ef5043fbd4b74f828a4a4da3260b538933db5455d9bf8027c56c7e51a9fb`.
- Decision: Free more static SRAM, then repeat both build gates and the real board cycle. Runtime and CAP installation are not passing yet.
- Commit: `43059dc`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T133620Z.log`

## 2026-10-10 — Test 64 KiB CHOUPI overlay arena on Pico 2

- Step/experiment: `wp1-choupi-overlay-arena-64k-hardware`
- Change: Reduced the overlay byte arena from 128 KiB to 64 KiB to free 65,536 bytes of SRAM for runtime allocations.
- Validation: `./tools/cycle choupi` passed ARM and RISC-V build gates, flashed the RP2350 ARM UF2 with visible 100% progress, and captured 18 seconds of serial output. The runtime acceptance gate failed on `*** PANIC *** / Out of memory`.
- Result: After `runtime:start`, a 16,390-byte allocation succeeded at `0x2006EFE0..0x20072FE6`, but a subsequent 36,870-byte allocation failed. The reduced arena alone did not let the VM start. Log SHA-256: `4ea1a3f80e2bb24e5665d4e3e3e178e76fa6cc858a3e20fd5d3d31acee11761c`; UF2 SHA-256: `a590f7b12e9a9ebc66ffa202921e9878cc12b8457d667ed65f29aba97046c9f5`.
- Decision: Free further static SRAM while retaining a smaller overlay, rebuild both architectures, and repeat the real board cycle. The Pico must be physically returned to BOOTSEL. CAP remains uninstalled.
- Commit: `f9efb4a`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T212007Z.log`

## 2026-10-10 — Reduce CHOUPI overlay SRAM footprint

- Step/experiment: `wp1-choupi-overlay-arena-32k-compact-index`
- Change: Reduced the overlay byte arena to 32 KiB and compacted each overlay index entry from 12 to 8 bytes by using 16-bit offsets and sizes; the arena bound ensures both fields fit while retaining up to 8,192 indexed files.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The built ARM CHOUPI UF2 SHA-256 is `46f2b453d33ada46fa1cda9e681f18648f53e00ac966b76bc9aa1faf49bda137`. No runtime test has been recorded yet: USB shows the Pico in serial application mode, and `picotool reboot -f -u` did not re-enumerate it in BOOTSEL.
- Result: Both variants compile; static SRAM usage is reduced by about 96 KiB relative to the previous 64 KiB arena/index configuration. The VM runtime behavior and capacity impact have not been measured.
- Decision: Flash and inspect a finite serial capture after the Pico is confirmed in BOOTSEL. Do not treat this candidate as a runtime pass.
- Commit: `21d7b94`
- Artifacts/logs: `build/arm/pico2_choupi.uf2` SHA-256 `46f2b453d33ada46fa1cda9e681f18648f53e00ac966b76bc9aa1faf49bda137`

## 2026-10-10 — Diagnose silent CHOUPI upload attempt

- Step/experiment: `wp1-choupi-32k-serial-capture`
- Change: Attempted the repository-gated CHOUPI flash cycle on the connected Pico 2, then reset once with the serial reader already attached and preserved both bounded captures.
- Validation: `./tools/cycle choupi` rebuilt ARM and RISC-V successfully, attempted `picotool load -f -x`, and failed the runtime output gate because no serial bytes arrived. `picotool reboot -f -a` also completed, but the 20-second pre-attached serial capture was silent. `picotool info -a` continues to identify the device as running USB serial firmware, not BOOTSEL.
- Result: The candidate was not confirmed running; neither capture included BOOT or TEST output. No runtime or flash success is claimed. The cycle log is byte-identical to a previous silent capture (SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`); the pre-attached reset capture SHA-256 is `15125998cb93fe0190829ac3a6ffec0bfe84b365bc903605d437a42f39c98c65`.
- Decision: Have the board physically enter BOOTSEL, confirm it with `picotool info -a`, then retry the cycle and check for visible flash progress and startup output.
- Commit: `afc793333d8b13aaf790a0d7f80891684a5a2830`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T212553Z.log`; `artifacts/logs/serial-reset-capture-20261010.log`

## 2026-10-10 — Verify software BOOTSEL recovery path

- Step/experiment: `wp1-choupi-force-bootsel-recovery`
- Change: Retried the documented `picotool reboot -F -u` transition with direct USB access after the silent upload attempt.
- Validation: The command returned success and reported that the device was asked to reboot into BOOTSEL; the following `picotool info -a` still found only USB serial mode and exited 249.
- Result: Software recovery does not leave this board accessible in BOOTSEL. No firmware write occurred during this experiment.
- Decision: Physically hold BOOTSEL while reconnecting, then confirm `boot type: bootsel` before flashing.
- Commit: `d474832fe8217b714344a3a238a08d0c93a28771`
- Artifacts/logs: none

## 2026-10-11 — Correct applet header file mode

- Step/experiment: `wp5-cap-applet-aid-index-file-mode`
- Change: Corrected the new C++ header's Git mode from executable to regular source-file mode; the imported CHOUPI checkout carried broad filesystem permissions.
- Validation: Rechecked the file mode and ran `git diff --check`. The ARM and RISC-V builds from the immediately preceding code commit passed; source contents are unchanged.
- Result: The applet metadata header is tracked as a regular source file.
- Decision: Continue implementation from the committed parser primitive.
- Commit: `4cec0a400d364398bab6d9028c311f9f982e59b7`
- Artifacts/logs: none

## 2026-10-10 — Boot CHOUPI after overlay SRAM reduction

- Step/experiment: `wp1-choupi-overlay-32k-hardware`
- Change: Flashed the committed 32 KiB overlay arena and compact 8-byte index on the RP2350 Pico 2 after confirming BOOTSEL.
- Validation: `./tools/cycle choupi` passed ARM and RISC-V build gates, visibly flashed 100%, and captured the 18-second serial stream. The cycle failed the runtime acceptance gate on an uncaught VM exception.
- Result: `fs_init()` completed and the former heap exhaustion is gone. A 36,870-byte allocation succeeded at `0x2006C000..0x20075006`, then CHOUPI reported `VM_EXCEPTION id=5 opcode=0x87 pc=0x10043C01` (IOException from `putfield_a`). The image-relative PC maps into ROM-mask record tag `0114`, at data offset `0x1B1`. Log SHA-256: `d2b068c660382e1b828004aea92132bd3f8f0cdccac0086a17369fd3c63d5d2f`.
- Decision: Keep the SRAM reduction; instrument filesystem write rejection causes and retry the hardware cycle to identify why persistence throws IOException.
- Commit: `3fb12c5`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T213245Z.log`

## 2026-10-10 — Add filesystem write rejection diagnostics

- Step/experiment: `wp1-choupi-fs-write-rejection-diagnostic`
- Change: Added explicit reasons and requested/existing ranges when `write_at()` rejects a missing file or out-of-range write; also report full-index and overlay-capacity rejection. Narrowed the new file offset assignment to the 16-bit overlay representation.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` passed both build gates but could not transition the running CHOUPI image to BOOTSEL; it recorded no flash progress or serial output and failed the runtime gate. Diagnostic UF2 SHA-256: `1de6bce7ed146f0a4a709f3180650305c6d62eb2a902d0eb95d774dd49b88e13`.
- Result: The diagnostic image has not been flashed. The current app remains running and software force-reboot did not expose BOOTSEL.
- Decision: Re-enter BOOTSEL with the physical button, verify, then repeat `./tools/cycle choupi` to capture the write rejection.
- Commit: `06ad89f`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T213559Z.log` (silent; SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`)

## 2026-10-10 — Trace persistent reference field failure phase

- Step/experiment: `wp1-choupi-putfield-persistence-phase`
- Change: Added phase markers and exception IDs around `getFields()` and recursive persistence in `JC_Instance::setField_Reference()` to determine whether the IOException occurs while loading or storing an instance.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The Pico is running the previous image in USB serial mode, and `picotool info -a` confirms it is not in BOOTSEL; the new image has not been flashed.
- Result: Candidate ARM CHOUPI UF2 SHA-256 is `18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`. Persistence failure phase is not yet measured.
- Decision: Physically enter BOOTSEL, flash this candidate, and capture whether the failure occurs at load or store and any filesystem rejection reason.
- Commit: pending
- Artifacts/logs: `build/arm/pico2_choupi.uf2` SHA-256 `18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`

## 2026-10-10 — Attempt software-only BOOTSEL recovery

- Step/experiment: `wp1-choupi-usb-reset-recovery`
- Change: Tried all available USB software reset paths against the running Pico app: `picotool reboot -f/-F -u`, `picotool load -F`, CDC 1200 baud, and direct libusb control transfers to the enumerated reset interface (including a claimed interface and class/interface request).
- Validation: Each tool path reported a BOOTSEL request or returned from the transfer, but `picotool info -a` still reported only CDC serial mode (exit 249). The USB device remained at bus 1, device 116. Keeping `/dev/ttyACM0` open during reset yielded zero bytes and did not change the device mode. `lsusb -t` shows CDC interfaces 0/1 and reset interface 2.
- Result: No software-only path reached BOOTSEL; the diagnostic image remains unflashed. The running VM emitted no further serial output after its IOException trace, consistent with firmware no longer servicing the reset path (inference from host observations).
- Decision: Keep the latest diagnostic candidate ready. Do not claim a reset or flash. Continue software-only source analysis; hardware flash remains unavailable until the device responds to a supported reset.
- Commit: pending
- Artifacts/logs: none; command results recorded above
## 2026-10-11 — Retry software BOOTSEL recovery

- Step/experiment: `wp1-pico-usb-port-recovery`
- Change: Retried `picotool reboot -F -u`, used `usbreset` on the Pico's USB bus/device, and inspected the xHCI root port for a host-side recovery control. Updated the handoff with the current USB state.
- Validation: `picotool reboot -F -u` returned 0 but left the RP2350 in CDC mode. `usbreset 001/116` reported `No such device`; the Pico then disappeared from `lsusb`. The root port reports `not attached`; `/dev/ttyACM0` is absent. Toggling the per-port sysfs `disable` control was denied by host permissions. No flash was attempted.
- Result: No software path brought the board into BOOTSEL. Device is currently absent from the host USB bus; no hardware runtime output was captured.
- Decision: Continue software analysis with the existing built diagnostic firmware. Upload and hardware verification remain pending until the RP2350 reappears.
- Commit: `256edf8e6ca33ed9584877d7ca8bdd842c01cbe4`
- Artifacts/logs: none; UF2 awaiting upload at `build/arm/pico2_choupi.uf2` (SHA-256 `18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`)

## 2026-10-11 — Treat missing persistent instance fields as defaults

- Step/experiment: `wp1-choupi-missing-persistent-field-defaults`
- Change: `JC_Instance::getFields()` now detects absent field tags before type decoding and initializes those entries to `FIELD_TYPE_UNINITIALIZED` with value zero. This follows the existing missing-reference behavior and avoids propagating an IOException for a never-written child record.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` both passed. `git -c core.whitespace=cr-at-eol diff --check -- choupi/src/jc_types/jc_instance.cpp` passed. Hardware execution was not attempted because xHCI port 3 reports `not attached` and no Pico device is enumerated.
- Result: Both target variants compile. The missing-record path is addressed in source; runtime behavior and whether this resolves the observed `putfield_a` IOException remain unverified.
- Decision: Keep the candidate ready and flash it with `./tools/cycle choupi` when USB/BOOTSEL enumeration returns. JCAlgTest v1.8.2 remains uninstalled.
- Commit: `73f4890ebf8758f25b4fe066f30f3a0edcfb3479`
- Artifacts/logs: ARM UF2 `build/arm/pico2_choupi.uf2`, SHA-256 `2860767cb505cddcc6c6ec372eeceb4b7d5bd5d0e77026c909ae816294e3de03`; last board trace remains `artifacts/logs/cycle-choupi-20261010T213245Z.log`

## 2026-10-11 — Flash persistent-field default fix on Pico 2

- Step/experiment: `wp1-choupi-missing-field-hardware-cycle`
- Change: Ran the repository-gated CHOUPI cycle with the missing persistent field default fix.
- Validation: Both ARM and RISC-V builds passed. The RP2350 entered BOOTSEL, the ARM UF2 visibly reached 100% flash progress, and the cycle captured 18 seconds. It failed the runtime gate: the trace passed `putfield_a` stores for package 20/class 57 fields 0 and 1, allocated 36,870 bytes at `0x2006C000..0x20075006`, then began loading package 20/class 121 field 0. No exception or `TEST:SUMMARY` followed. A separate 8-second monitor returned 0 with no serial bytes. `picotool reboot -F -u` did not reach BOOTSEL; the subsequent USB reset left the host with no Pico device.
- Result: The original immediate IOException is no longer observed, but startup now stalls during persistent field loading for class 121. The default-field fix is not sufficient to pass runtime.
- Decision: Add bounded field-load phase markers, build both architectures, and retain the trace candidate for the next BOOTSEL window. JCAlgTest v1.8.2 remains uninstalled.
- Commit: `d376210073bdcee34e63f352f6a08343a39197a9`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T053229Z.log`, SHA-256 `909b217b6fa27f343c27dcf7bb3ec4f5c2f2909ca36bf25c2a335ddfeedd2134`; prior flashed UF2 SHA-256 `2860767cb505cddcc6c6ec372eeceb4b7d5bd5d0e77026c909ae816294e3de03`

## 2026-10-11 — Trace persistent field reconstruction phases

- Step/experiment: `wp1-choupi-field-load-phase-trace`
- Change: Added a capped trace around persistent field reconstruction. For the first 64 decoded entries it reports `fs_exists` start/completion, field type, and value completion, with package, class, index, and field count.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` both passed. Whitespace validation with `git -c core.whitespace=cr-at-eol diff --check -- choupi/src/jc_types/jc_instance.cpp` passed. No hardware flash was attempted because the host no longer enumerates the Pico after the USB reset.
- Result: The trace candidate is ready to distinguish a filesystem syscall stall from type/value reconstruction; it is not hardware validated.
- Decision: Preserve the candidate for the next BOOTSEL window. Continue toward a runtime pass before attempting the still-missing exact JCAlgTest CAP installation.
- Commit: `0f417104cb79f12f6c3f4c6ea157296373d3872f`
- Artifacts/logs: `build/arm/pico2_choupi.uf2`, SHA-256 `a2deabc677b6672431386a1b0aef3eb05cc6debf906b1f0bc478d19f2b86f543`; no board log for this unflashed build

## 2026-10-11 — Trace persistent class field sizing

- Step/experiment: `wp1-choupi-class-field-count-trace`
- Change: Added bounded diagnostics to `Class_Handler::getInstanceFieldsSize()` for package 20/class 121, logging each superclass step, declared instance size, raw superclass reference, object-class status, and accumulated field count.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` both passed. No flash was attempted because the RP2350 currently enumerates only as the USB serial application and does not enter BOOTSEL on the software request.
- Result: The prior field-load trace measured 8,912 fields for package 20/class 121; this candidate should expose the hierarchy calculation that produced that count. Hardware output is pending.
- Decision: Flash only after BOOTSEL returns, then check whether superclass traversal cycles or accumulates implausible class sizes.
- Commit: `df98bc5d5e54206035d92239b07cb344107f2223`
- Artifacts/logs: `build/arm/pico2_choupi.uf2`, SHA-256 `5fa53345dbcabadbcaec146dc6aa07bc5e195f61fe320f49d6308e4fa8bedde4`; previous trace `artifacts/logs/cycle-choupi-20261011T053802Z.log`

## 2026-10-11 — Retry BOOTSEL and audit class token conversions

- Step/experiment: `wp1-pico-bootsel-class-token-audit`
- Change: Retried software BOOTSEL recovery and inspected class-index conversion paths. Updated `.agent/HANDOFF.md` with the result and the pending hierarchy trace.
- Validation: Direct USB `picotool reboot -F -u` returned “The device was asked to reboot into BOOTSEL mode”; follow-up `picotool info -a` still found only the CDC app at bus 1/address 120. Source inspection confirmed `getClassInformation()` returns the CAP byte offset and `getClassFromClassIndex()` dereferences that offset; `getClassRefFromClassIndex()` instead iterates by ordinal. No build or flash was needed for this inspection.
- Result: The firmware remains unflashed; the board is not in BOOTSEL. The index conversion mismatch warrants follow-up but is not yet tied to the 8,912-field count.
- Decision: Continue tracing the class-token semantics and attempt the already-built superclass trace when BOOTSEL is available. Avoid further bus resets.
- Commit: `6ae21f4` (record commit; metadata follow-up is `ff59debdba66dd7fdbfa7579546bca355681f8bc`)
- Artifacts/logs: no new log; pending ARM UF2 SHA-256 `5fa53345dbcabadbcaec146dc6aa07bc5e195f61fe320f49d6308e4fa8bedde4`

## 2026-10-11 — Preserve CAP class byte offsets in instance references

- Step/experiment: `wp1-choupi-class-offset-ref-conversion`
- Change: Corrected `getClassRefFromClassIndex()` to encode the instance's CAP Class component byte offset directly in CAP byte order. Removed the incorrect ordinal scan through variable-length class/interface records.
- Validation: `./tools/env ./tools/build arm` passed and emitted the CHOUPI ARM UF2; `./tools/env ./tools/build riscv` passed its wrapper build. `git -c core.whitespace=cr-at-eol diff --check -- choupi/src/jc_handlers/jc_cp.cpp` passed. No hardware flash was attempted because `picotool info -a` still reports CDC and no BOOTSEL device.
- Result: Class references reconstructed from persistent instance class offsets now round-trip through `resolveClassref()` consistently. Hardware behavior is not yet verified.
- Decision: Keep both source fixes in the next board image and flash after BOOTSEL becomes accessible; collect the pending superclass trace to address the 8,912-field allocation.
- Commit: `0a90a18` (record commit; metadata follow-up `668c1a8`)
- Artifacts/logs: ARM UF2 `build/arm/pico2_choupi.uf2`, SHA-256 `274b8128c4259b167c52bc4f363e96d88348dd1f9540f655932232d4f4ed568f`

## 2026-10-11 — Retry force-loaded CHOUPI cycle after class offset fix

- Step/experiment: `wp1-choupi-class-offset-force-load-cycle`
- Change: Ran the repository cycle with the corrected class-offset conversion and existing hierarchy trace.
- Validation: `./tools/env ./tools/cycle choupi` passed ARM and RISC-V build gates. `picotool load -f -x` requested a BOOTSEL reboot but emitted no flash progress or application-reboot confirmation; the cycle's 18-second serial capture was empty and failed the CHOUPI runtime output gate. Subsequent `picotool info -a` reported no BOOTSEL device and only the CDC app at bus 1/address 120. Force-mode info likewise yielded no installed-image metadata.
- Result: The candidate image is not confirmed flashed. The empty log is byte-identical to the prior silent capture, SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`.
- Decision: Keep the corrected image ready and continue offline analysis; avoid further host bus resets.
- Commit: `2d7fead` (record commit; metadata follow-up `b27cc89`)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T054755Z.log`; candidate UF2 SHA-256 `274b8128c4259b167c52bc4f363e96d88348dd1f9540f655932232d4f4ed568f`

## 2026-10-11 — Retry BOOTSEL after application reboot

- Step/experiment: `wp1-pico-app-reboot-bootsel-recovery`
- Change: Forced a normal application reboot, captured serial output for eight seconds, then retried the software BOOTSEL request. Updated the handoff with the recovery result.
- Validation: `picotool reboot -f -a` returned success; `./tools/env ./tools/monitor --seconds 8` returned zero with no serial output. `picotool reboot -f -u` also returned success, but `picotool info -a` exited 249 and found only the CDC app at bus 1/address 120. No flash was attempted.
- Result: Resetting the application does not restore software entry to BOOTSEL; the installed image remains unverified and the runtime emits no output.
- Decision: Preserve the built UF2 and continue offline analysis. The failed BOOTSEL transition now persists across both direct and cycle force requests and an application reboot.
- Commit: `6a584c4` (this record; metadata follow-up follows)
- Artifacts/logs: no monitor file was produced; latest cycle log remains `artifacts/logs/cycle-choupi-20261011T054755Z.log`

## 2026-10-11 — Retry CHOUPI reflash with board left connected

- Step/experiment: `wp1-choupi-force-load-retry`
- Change: Re-ran the repository-gated CHOUPI cycle at the user's request, with the Pico left connected as-is.
- Validation: `./tools/env ./tools/cycle choupi` passed ARM and RISC-V build gates. `picotool load -f -x` waited for BOOTSEL and emitted no flash progress; the 18-second capture was silent and failed the runtime gate. Follow-up `picotool info -a` exited 249 and still found only the CDC app at bus 1/address 120.
- Result: Reflash is not confirmed. The new log's SHA-256 is `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`, identical to the previous silent cycle capture.
- Decision: Keep the current ARM candidate ready. The host's force-load route still cannot transition the running image into BOOTSEL; no JCAlgTest CAP installation occurred.
- Commit: `9982fae` (this record; metadata follow-up follows)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T060944Z.log`; ARM UF2 SHA-256 `274b8128c4259b167c52bc4f363e96d88348dd1f9540f655932232d4f4ed568f`

## 2026-10-11 — Flash class trace and localize persistent field corruption

- Step/experiment: `wp1-choupi-class-field-runtime-trace`
- Change: Reflashed the prepared hierarchy and field-load trace after the user replugged the Pico into BOOTSEL.
- Validation: `./tools/env ./tools/cycle choupi` passed ARM and RISC-V builds, visibly flashed the ARM image to 100%, rebooted it, and captured 18 seconds of serial output. Runtime gate failed with no `TEST:SUMMARY`.
- Result: For package 20/class 121, three hierarchy calculations each report depth 1 and 15 declared fields, while `JC_Instance::getFields()` reports vector length 8,912. The trace completes existence checks for indexes 0–63 before capture ends. Log SHA-256: `09be7d9368d2d988d4d953f45daa37f5832b9991d152de1b1de424ceb7266d40`.
- Decision: Investigate object ownership between persistent instance construction and heap insertion; this discrepancy indicates the superclass calculation is not producing 8,912.
- Commit: `c383486` (this record; metadata follow-up follows)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T061811Z.log`; flashed ARM UF2 SHA-256 `274b8128c4259b167c52bc4f363e96d88348dd1f9540f655932232d4f4ed568f`

## 2026-10-11 — Transfer persistent instances into the heap safely

- Step/experiment: `wp1-choupi-persistent-instance-move-ownership`
- Change: Made `JC_Instance` move-only, transferring its owned field vector and overflow tag buffer while nulling the moved-from pointers. Changed `Heap::addInstance` to accept and move an rvalue instead of making a shallow copy.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` both passed. No hardware cycle was run for this change because `picotool info -a` reports the Pico in CDC mode, not BOOTSEL.
- Result: The ownership bug matches the hardware trace where class sizing returned 15 but the persistent instance vector later reported 8,912. The fix is compile-validated but not yet hardware-verified.
- Decision: Flash this candidate during the next BOOTSEL window and confirm the field vector remains 15 entries and the runtime reaches `TEST:SUMMARY`.
- Commit: `45ace5d` (code and record; metadata follow-up `bfb8802`)
- Artifacts/logs: ARM UF2 `build/arm/pico2_choupi.uf2`, SHA-256 `92ddc5aa9497f3020a302a253003f6074ebfcdb656eff9b7196beb406e7cfb98`; diagnosis log `artifacts/logs/cycle-choupi-20261011T061811Z.log`

## 2026-10-11 — Trace persistent reference-field update after load

- Step/experiment: `wp1-choupi-persistent-reference-update-trace`
- Change: Added bounded diagnostics around class 121's `putfield_a` continuation, logging the reference kind and marking array lookup and persistent-store completion.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` passed. `picotool reboot -F -u` reported that it requested BOOTSEL, but `picotool info -a` found only CDC. `picotool load -f -x build/arm/pico2_choupi.uf2` returned without flash progress or application-reboot confirmation; `./tools/env ./tools/monitor --seconds 8` returned silently. Hardware validation is pending.
- Result: The previous flashed image completed loading all 15 fields but emitted no later runtime marker. This diagnostic candidate is not confirmed flashed.
- Decision: Reuse this candidate at the next BOOTSEL window; inspect whether it stops before field access, while looking up an array, or while writing persistent storage.
- Commit: `360085f` (metadata follow-up)
- Artifacts/logs: `build/arm/pico2_choupi.uf2`, SHA-256 `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`; prior hardware trace `artifacts/logs/cycle-choupi-20261011T070346Z.log`

## 2026-10-11 — Retry CHOUPI cycle with Pico attached over CDC

- Step/experiment: `wp1-choupi-live-cdc-bootsel-retry`
- Change: Re-ran the repository-gated CHOUPI cycle against the enumerated Pico and tried targeted and untargeted picotool BOOTSEL reboot requests.
- Validation: `./tools/env ./tools/cycle choupi` passed ARM and RISC-V build gates, then picotool waited for a BOOTSEL device without reporting flash progress. The 18-second serial capture was silent and the cycle exited 1 with `CHOUPI runtime output did not pass`. `./tools/env ./tools/doctor` passed host and source checks. `picotool info -a` consistently identifies a CDC serial RP2350 at bus 1/address 124 and no BOOTSEL device.
- Result: The board is connected, but the current firmware does not complete picotool's software BOOTSEL transition; the diagnostic UF2 remains unflashed.
- Decision: Keep the candidate ready. A hardware BOOTSEL entry is needed to install it; do not treat USB enumeration as proof of bootloader mode.
- Commit: `28427a5` (metadata follow-up)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T071557Z.log`, SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`; candidate UF2 SHA-256 `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`

## 2026-10-11 — Flash reference-field continuation trace

- Step/experiment: `wp1-choupi-reference-field-continuation-cycle`
- Change: Waited for the connected Pico to finish its delayed transition to BOOTSEL, then ran the gated CHOUPI cycle with the class-121 reference update trace.
- Validation: ARM and RISC-V builds passed. Picotool visibly flashed the ARM UF2 to 100% and rebooted the application. The 18-second serial capture showed all 15 persistent fields loaded, the field reference resolved to array type 17, and `setPersistentField_Reference()` returned. The cycle still exited 1 without `TEST:SUMMARY`.
- Result: The suspected stall is after the class-121 field load and persistent write; `PUTFIELD_A phase=stored index=0` is the last runtime marker.
- Decision: Instrument the interpreter instruction loop to locate the next bytecode that fails to complete.
- Commit: `6baf0f9` (metadata follow-up)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T074012Z.log`, SHA-256 `997029462d5e282bd5726b40c2985070f093fa3ca07dd2aad40fb19bd08937eb`; flashed UF2 SHA-256 `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`

## 2026-10-11 — Add bounded BOOTSEL and flash confirmation retries

- Step/experiment: `wp1-pico-flash-retry-and-confirm`
- Change: Added `tools/flash`, which tries the existing force-load route, polls for actual BOOTSEL after delayed USB transitions, retries picotool reset requests, and tries a bounded CDC 1200-baud reset fallback. Flash success now requires both a 100% progress marker and application-reboot confirmation; `tools/cycle` uses the helper and retains the existing 18-second CHOUPI capture.
- Validation: `bash -n tools/flash` and `bash -n tools/cycle` passed. `./tools/env ./tools/cycle choupi` passed ARM and RISC-V builds; the new wrapper rejected the force-load's request-only result, polled BOOTSEL, retried, and failed closed when the device remained CDC. The cycle exited 1 rather than reporting an unverified flash. Inspection of the ARM map found both the CDC line-coding and vendor-reset callbacks linked. No watchdog firmware change has been built or flashed yet.
- Result: The wrapper no longer treats picotool's successful reset request as proof of a flash. Current app USB reset callbacks are present but the stalled runtime does not enter BOOTSEL under software requests.
- Decision: Keep the host retry bounded. Add a hardware watchdog recovery path so a stalled VM can boot into BOOTSEL after the next successful flash.
- Commit: `dfd27a5` (metadata follow-up)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T074828Z.log`, SHA-256 `bf11511b61eb8b41c6675c41ad9f8985945d20757fbab9bf2c7ea82f252bc05b`; current ARM image SHA-256 `cf0c219a391e54c40dbc251591ae89f7a49adc0c515903adbcefa79b6d903ba5`

## 2026-10-11 — Add watchdog-to-BOOTSEL recovery

- Step/experiment: `wp1-choupi-watchdog-bootsel-recovery`
- Change: Enabled a 15-second RP2350 watchdog before VM startup, refresh it once per interpreter instruction, and check `watchdog_enable_caused_reboot()` at startup. A watchdog timeout now enters ROM BOOTSEL directly on the next boot. The status loop refreshes the watchdog while reporting results.
- Validation: An initial build failed because the shared VM target lacks Pico SDK hardware include paths; replaced the direct SDK call with a board-provided watchdog callback. `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` then passed. The full cycle also passed both build gates, but the already-installed pre-watchdog image remained CDC despite force-load, delayed polling, repeated picotool requests, and the CDC reset fallback. The candidate is not flashed or hardware-verified.
- Result: Watchdog code compiles for both configured builds. Hardware watchdog recovery remains unproven until this candidate is installed once.
- Decision: Use the watchdog candidate on the next successful BOOTSEL window; the current installed image has no watchdog recovery and stays unresponsive to software reset requests.
- Commit: `810f529` (metadata follow-up)
- Artifacts/logs: candidate ARM UF2 `build/arm/pico2_choupi.uf2`, SHA-256 `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`; failed gated cycle `artifacts/logs/cycle-choupi-20261011T075539Z.log`, SHA-256 `bf11511b61eb8b41c6675c41ad9f8985945d20757fbab9bf2c7ea82f252bc05b`

## 2026-10-11 — Try rapid app reset followed by BOOTSEL

- Step/experiment: `wp1-pico-rapid-reset-bootsel`
- Change: Requested an application reset and then a BOOTSEL reset 200 ms later, targeting the currently attached Pico.
- Validation: Both `picotool reboot -F -a` and `picotool reboot -F -u` returned success text, but the following `picotool info -a` still found only USB serial at bus 1/address 126 and no BOOTSEL device. No flash was attempted.
- Result: Chaining software reset requests does not recover the currently stalled image.
- Decision: The watchdog candidate remains the recovery route for subsequent hangs; one initial BOOTSEL entry is still needed to install it.
- Commit: `e86c53d` (metadata follow-up)
- Artifacts/logs: no new cycle log; candidate UF2 remains `build/arm/pico2_choupi.uf2`, SHA-256 `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`

## 2026-10-11 — Wait for Pico USB re-enumeration before flashing

- Step/experiment: `wp1-pico-flash-hotplug-wait`
- Change: Updated `tools/flash` to detect RP-series USB devices in sysfs and wait up to 120 seconds for re-enumeration before invoking picotool. This avoids spending the force-load timeout on a Pico that is absent from the host USB bus and lets a transient passthrough disconnect recover within the bounded wait.
- Validation: `bash -n tools/flash` and scoped `git diff --check -- tools/flash .agent/HANDOFF.md progress.md` passed. A 60-second initial USB observation found no RP-series device. Then `./tools/env ./tools/cycle choupi` passed ARM and RISC-V builds and exercised the hot-plug wait for its full 120 seconds, but no device appeared; the cycle exited 1 before picotool or serial monitoring. Hardware flashing was not attempted because the board is not enumerated.
- Result: The hot-plug wait runs as intended and reports a reconnect-specific failure at timeout. Current watchdog UF2 remains unflashed; no hardware validation was possible in this step.
- Decision: Keep the wait bounded and fail with a reconnect-specific message if USB does not return; rerun the gated CHOUPI cycle once the RP-series device enumerates.
- Commit: `5f6842b` (metadata follow-up records this SHA)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T082732Z.log`, SHA-256 `fe2431031ef112a6a2d8c0294c29c9bcf8b380b2e50976af86beccde606d204f`; watchdog UF2 `build/arm/pico2_choupi.uf2`, SHA-256 `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`

## 2026-10-11 — Retry flash after BOOTSEL was reported visible

- Step/experiment: `wp1-pico-bootsel-visible-flash-retry`
- Change: Re-ran the requested real-board CHOUPI cycle and checked USB topology, mass-storage mounts, and picotool enumeration when the flasher did not find the Pico.
- Validation: `./tools/env ./tools/cycle choupi` passed ARM and RISC-V builds, then its 120-second RP USB wait expired with status 1. `lsusb` showed only the two root hubs and VirtualBox USB Tablet; no RP-series VID was present in sysfs, no RPI-RP2 volume was mounted, and both `picotool info -a` and `picotool reboot -F -u` reported no accessible RP-series device. No flash or serial capture occurred.
- Result: The BOOTSEL device is not enumerated to the shell running the flasher, so picotool cannot access it despite the user's report that it is visible in the VM. Watchdog candidate remains unflashed.
- Decision: Retry immediately once the RP-series device is visible in this process's USB enumeration; keep the dual-build gate before flashing.
- Commit: `21812de` (metadata follow-up records this SHA)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T084337Z.log`, SHA-256 `fe2431031ef112a6a2d8c0294c29c9bcf8b380b2e50976af86beccde606d204f`; candidate UF2 SHA-256 `607a0804a414115eea58f912a2ad0dafafa20cc319ddfca0ed84189fed5dec6d`

## 2026-10-11 — Recover watchdog BOOTSEL and pass CHOUPI runtime

- Step/experiment: `wp1-pico-watchdog-trace-and-array-memory-fix`
- Change: Flashed the watchdog candidate after the user refreshed VirtualBox USB passthrough. Confirmed watchdog recovery to ROM BOOTSEL after the stalled runtime. Added a 24-instruction trace after `putfield_a`, which identified repeated 36,870-byte allocations while reading persistent arrays. Changed non-transient persistent arrays to allocate only tag metadata in RAM; their contents continue to use flash-backed element access. Transient arrays still allocate their full data buffer.
- Validation: The first watchdog cycle passed both architecture builds and flashed the image at 100%; the VM stalled and watchdog recovery was confirmed by `picotool info -a` reporting `boot type: bootsel`. After the trace and array-storage change, `./tools/env ./tools/cycle choupi` passed ARM and RISC-V builds, flashed the updated image to 100%, and captured `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` with no `FAULT` or timeout.
- Result: Watchdog recovery is hardware-verified. The memory fix allowed bytecode `0xAD` reads to continue past the prior second 36,870-byte allocation failure, and the full CHOUPI runtime test now passes on Pico 2. Current firmware is running the passing test loop.
- Decision: Preserve the watchdog and bounded instruction trace while continuing to build an installation path for the supplied JCAlgTest v1.8.2 CAP. The current CDC firmware still lacks APDU/CCID and CAP installation support.
- Commit: `13c8b4e` (metadata follow-up records this SHA)
- Artifacts/logs: watchdog recovery run `artifacts/logs/cycle-choupi-20261011T085422Z.log` (SHA-256 `13126143227f832e1abd2b7160a63d5c91745cbb3727cdb0c0c1dafcba8a4d1f`); passing cycle `artifacts/logs/cycle-choupi-20261011T090019Z.log` (SHA-256 `a093b8d548205e6a11dba34954509d6667fc9947cf14c18116f6f92a217f8cf7`); flashed UF2 SHA-256 `48f170eb971f3e9f33505eeb0d40934eced68ad8b46c6f80a31248ad8741e3cb`

## 2026-10-11 — Verify supplied JCAlgTest CAP components on Pico 2

- Step/experiment: `wp5-v182-cap-component-identity`
- Change: Compared every `.cap` component in the supplied JCAlgTest v1.8.2 archive with the ROM-mask build's rebuilt CAP archive and checked that the current ARM UF2 contains the current ROM-mask image. All 10 component files, including the 19,181-byte `Method.cap`, match byte for byte. The ROM mask statically installs the package and applies CHOUPI's storage transformation.
- Validation: The supplied archive SHA-256 is `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`; the rebuilt archive SHA-256 is `daccd8838322995f02a5200d9ebd61a4bf237d293031875424813acd7a7df68b` because its outer ZIP metadata differs. Component comparison found 10/10 identical payloads. UF2 inspection confirmed the current `build/choupi-rommask/flash-algtest.bin` bytes are embedded in `build/arm/pico2_choupi.uf2`. A five-second live CDC capture from `/dev/serial/by-id/usb-Raspberry_Pi_Pico_D92E743D5F640C0F-if00` returned status 0 and contained five `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` pairs, with no `FAULT` or timeout. No new dual-architecture build was needed for this inspection-only step; no GlobalPlatform APDU install was performed.
- Result: The package components supplied in `AlgTest_v1.8.2_jc305.cap` are the same bytes used to build the applet package embedded in the running CHOUPI ROM mask. The different archive-level hash alone does not indicate different CAP components. Firmware still exposes CDC debug only, so runtime GP install/select is not available.
- Decision: Treat the static ROM-mask package as the completed alternative installation path for this board image. Keep APDU/CCID and GP installation as a separate future capability if runtime package management is needed.
- Commit: `d747a27` (metadata follow-up records this SHA)
- Artifacts/logs: `artifacts/logs/cap-component-compare-20261011.txt` (SHA-256 `f2106b97ad08d4505ce427fed9cf8daafd02453b1a22f66987022d92de2ec370`); `artifacts/logs/monitor-choupi-20261011T091500Z.log` (SHA-256 `02a4976d2be4c2e6f8df39c71db1451f94490a7a837434cb922342777d042ffb`); current ROM mask `build/choupi-rommask/flash-algtest.bin` SHA-256 `967a883ebf87955c74af84265218685414f82eaf584cab7e60651d719d11eda3`; current UF2 SHA-256 `48f170eb971f3e9f33505eeb0d40934eced68ad8b46c6f80a31248ad8741e3cb`

## 2026-10-11 — Add and verify USB CCID transport

- Step/experiment: `wp5-pico-ccid-transport`
- Change: Added a composite CDC + CCID USB configuration and a TinyUSB CCID class driver for slot power, status, and XfrBlock messages. The APDU response remains a deliberate `6D00` placeholder until the VM has an APDU dispatcher. Retained the SDK CDC descriptor for the separate smoke target.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` passed before flash. `./tools/env ./tools/cycle choupi` flashed the ARM UF2 to 100%; the finite serial capture emitted `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`, with no `FAULT` or timeout. Elevated `lsusb -v -d 2e8a:0009` showed CDC interfaces 0-1 and CCID interface 2 with bulk endpoints 0x03/0x83. A direct libusb hardware exchange passed IccPowerOn (ATR `3b800181`), XfrBlock (response `6d00`), and IccPowerOff. The host has `libpcsclite1` but no `pcscd`/`pcsc_scan`; GlobalPlatformPro JAR artifacts are present.
- Result: The board now exposes a working, USB-visible CCID transport alongside CDC diagnostics. The transport is hardware-verified, but it does not yet provide Java Card or GlobalPlatform behavior, and no CAP install was attempted through CCID.
- Decision: Next implement an APDU dispatch route from CCID XfrBlock into CHOUPI and identify the VM's supported applet command lifecycle; then use an available PC/SC daemon or the direct USB probe for GPPro validation.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T093403Z.log` (SHA-256 `23c41afe5f954df2edf1b01eb26a5b90482a1288437d4f9dc30a1ffeb72702c9`); `artifacts/logs/ccid-probe-20261011.txt`; ARM UF2 SHA-256 `4f675fadf4913cf33d430494236c8853a6978a177873f9c5abe09b4a074eb995`

## 2026-10-11 — Audit APDU and applet lifecycle entry points

- Step/experiment: `wp3-apdu-lifecycle-entry-audit`
- Change: Traced the boot-time ROM-mask starter through the CHOUPI interpreter and Java Card API. Recorded the minimal missing path and checked the bundled GlobalPlatformPro host against PC/SC.
- Validation: Source inspection confirms firmware invokes one fixed static method at boot; non-static interpreter entry, `Applet.register()`, and `APDU.getBuffer()` are explicit TODOs, and no selected-applet registry exists. `./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar --list` exits with `SCARD_E_NO_SERVICE (0x8010001d)` because `pcscd` is absent. Inspection-only step; no build or flash was needed.
- Result: The existing static AlgTest ROM-mask runner cannot receive host APDUs, and GlobalPlatformPro cannot yet access a PC/SC resource manager. A CCID USB interface alone is insufficient for either GP package management or applet execution.
- Decision: Start WP3 by adding an APDU buffer and explicit applet selection/dispatch path, then provide a user-space PC/SC service/driver route before validating GPPro.
- Commit: pending
- Artifacts/logs: `artifacts/logs/apdu-lifecycle-audit-20261011.txt`

## 2026-10-11 — Check for a user-local PC/SC service

- Step/experiment: `wp3-pcsc-user-local-daemon`
- Change: Downloaded `pcscd` 2.4.1-1 and `libccid` 1.7.1-1 to `/tmp`, extracted them without system installation, and attempted to start the PC/SC daemon with the extracted CCID driver.
- Validation: The daemon exited immediately with `cannot create /run/pcscd: Permission denied`. No daemon remains running, no system package was installed, and the Pico was not flashed during this experiment.
- Result: Direct libusb CCID probing works, but this account cannot start the system PC/SC daemon because its runtime socket directory is root-owned/absent. The repository rule forbids using sudo in an autonomous cycle.
- Decision: Keep the hardware transport verified via libusb; resume PC/SC/GPPro after the host exposes its normal PC/SC service or an unprivileged socket path is available. Continue firmware work on applet lifecycle and APDU dispatch.
- Commit: pending
- Artifacts/logs: `artifacts/logs/pcsc-user-local-20261011.txt`; downloaded/extracted packages remain only under `/tmp/pico2-pcsc-root`.

## 2026-10-11 — Diagnose empty PC/SC scan after driver match

- Step/experiment: `wp3-pcsc-empty-reader-usb-state`
- Change: Checked PC/SC service state, USB enumeration, sysfs, and picotool after the aligned CCID driver match was added.
- Validation: pcscd socket and service were active, but `pcsc_scan -r` reported no reader. VM `lsusb` listed only root hubs and the VirtualBox USB Tablet; sysfs had no VID `2e8a`, and `picotool info -a` found no RP-series device. Inspection only; no build or flash was performed.
- Result: The reader was absent from the VM USB bus at the time of the scan, so libccid and pcscd had no device to enumerate.
- Decision: Reattach the Pico through VirtualBox; verify `lsusb -d 2e8a:0009`, then rerun `pcsc_scan -r`.
- Commit: pending
- Artifacts/logs: `artifacts/logs/pcsc-usb-absent-20261011.txt`

## 2026-10-11 — Correct CCID APDU exchange declaration

- Step/experiment: `wp5-ccid-short-apdu-feature-declaration`
- Change: Changed the CCID descriptor `dwFeatures` from `0x00000000` (character-level exchange) to `0x00020042` (short APDU exchange with automatic parameter configuration/negotiation). The firmware's `XfrBlock` path consumes and returns complete APDUs, so the previous declaration disagreed with its transport behavior.
- Validation: ARM and RISC-V wrapper builds passed. `./tools/env ./tools/cycle choupi` passed both build gates, entered BOOTSEL with the existing CDC 1200-baud fallback after picotool could not find a reset interface, flashed the ARM UF2 to 100%, and captured `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` with no `FAULT`. `lsusb -v -d 2e8a:0009` on the re-enumerated board reports `dwFeatures 00020042`, `Short APDU level exchange`, and `dwMaxCCIDMsgLen 271`. PC/SC APDU transmission was not verified: after flashing, the USB node changed from `001/016` to `001/018`, and pcscd had no reader because the prior per-node ACL did not carry over; a later scan also found its socket inactive.
- Result: The board is running the updated firmware and the CCID descriptor now matches the raw APDU-level XfrBlock implementation. The GPPro/PCSC transaction path remains unverified until pcscd can access the current USB device node. The firmware still returns `6D00` for XfrBlock and does not implement applet dispatch or GP installation.
- Decision: Restore pcscd access for the current/re-enumerated USB node, confirm `pcsc_scan -r` sees the reader, then send a harmless raw APDU through GPPro and confirm the returned `6D00` before implementing APDU dispatch.
- Commit: `4506ff7`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T101036Z.log` (SHA-256 `b2493c96ac884b487e7685014a1e9864d643b69198ee2b5a4e0f84f752d1092b`); flashed ARM UF2 SHA-256 `d6116030ec2fcd82cccbc8be61f123b19ee18616ed8596725cfc98e3e817e12e`

## 2026-10-11 — Verify APDU exchange through PC/SC

- Step/experiment: `wp5-pcsc-ccid-apdu-transport-validation`
- Change: No firmware change. After the host restored access to the re-enumerated USB node, verified the current Pico reader through pcscd and sent a SELECT-by-empty-AID APDU using GlobalPlatformPro.
- Validation: `pcsc_scan -r` listed `Pico 2 CHOUPI CCID [JCAlgTest CCID] (D92E743D5F640C0F)`. GlobalPlatformPro connected using T=1 and ATR `3B800181`, began a transaction, transmitted `00A4040000`, and received `6D00` twice in 47 ms and 17 ms. The command exited 1 because GlobalPlatformPro then reported `Could not SELECT default selected: 0x6D00`; this is the firmware's known placeholder response, not a transport error. No rebuild or flash was needed.
- Result: CCID enumeration, card activation, PC/SC transaction, and complete APDU request/response transport are verified end to end on hardware. Java Card applet dispatch and valid SELECT behavior remain absent.
- Decision: Trace the CHOUPI runtime entry and implement the narrowest APDU-to-applet dispatch path needed to select and invoke the statically embedded JCAlgTest applet; keep GlobalPlatform CAP loading separate until lifecycle support exists.
- Commit: `eb6548f`
- Artifacts/logs: `artifacts/logs/pcsc-apdu-probe-20261011.txt` (SHA-256 `615ae514e783cbc2f99e9ed6b22b84e66eeee1d9341824e60ca0fa7b68e618ec`)

## 2026-10-11 — Probe GlobalPlatformPro card listing

- Step/experiment: `wp5-gp-list-over-pcsc`
- Change: No firmware change. Ran the original `gp --list` acceptance probe with PC/SC access restored and saved its bounded APDU trace.
- Validation: GlobalPlatformPro connected to the Pico over T=1 with ATR `3B800181`, began/ended a PC/SC transaction, sent SELECT-by-empty-AID `00A4040000`, and received `6D00` in 15 ms. It exited 1 with `Could not SELECT default selected: 0x6D00 (Invalid INStruction)`. The transport path is working; GP SELECT/card-manager behavior is not implemented. No build or flash was needed.
- Result: AC1 is not met. The existing CCID returns its deliberate unsupported-instruction placeholder, so `gp --list` cannot select an ISD or list registry contents.
- Decision: Implement card-manager SELECT/status behavior and Java Card applet lifecycle before retrying `gp --list`; do not claim the card is GlobalPlatform-compatible from CCID/PCSC transport alone.
- Commit: `e37ad51`
- Artifacts/logs: `artifacts/logs/gp-list-probe-20261011.txt` (SHA-256 `fb8f6f2fe5db66583677148c20699d483a4381f686724f669b7afb3b1537e086`)

## 2026-10-11 — Record CAP lookup commit metadata

- Step/experiment: `wp5-cap-applet-aid-index-commit-metadata`
- Change: Recorded the implementation and source-file-mode commits for the bounded applet AID lookup step.
- Validation: `git ls-files -s choupi/src/jc_cap/jc_cap_applet.hpp` reports regular-file mode `100644`; both architecture builds passed for the implementation.
- Result: The implementation is committed as `52c7c7d`; the regular-file mode correction is committed as `6f54e5e`.
- Decision: Continue with firmware lifecycle integration.
- Commit: pending
- Artifacts/logs: none

## 2026-10-11 — Add bounded CAP applet AID lookup

- Step/experiment: `wp5-cap-applet-aid-index`
- Change: Corrected `jc_cap_app::AID()` to use its one-byte AID length directly and added a bounded lookup that walks variable-length Applet component records, validates each 5–16 byte AID and record boundary, and compares the requested AID.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed. `git diff --check` passed. Hardware/APDU validation was skipped because the current VM session reports no usable libusb context and no PC/SC service; no flash was attempted.
- Result: The VM has a safe CAP metadata primitive for locating applet entries by AID and reading the matching install method offset. No applet is selected or executed yet.
- Decision: Next wire the lookup into a persistent applet registry/lifecycle path; do not treat this parser primitive as SELECT or install support.
- Commit: pending
- Artifacts/logs: none

## 2026-10-11 — Record CAP lookup commit metadata

- Step/experiment: `wp5-cap-applet-aid-index-commit-metadata`
- Change: Recorded the implementation and source-file-mode commits for the bounded applet AID lookup step.
- Validation: `git ls-files -s choupi/src/jc_cap/jc_cap_applet.hpp` reports regular-file mode `100644`; both architecture builds passed for the implementation.
- Result: The implementation is committed as `52c7c7d`; the regular-file mode correction is committed as `6f54e5e`.
- Decision: Continue with firmware lifecycle integration.
- Commit: pending
- Artifacts/logs: none

## 2026-10-11 — Confirm v1.8.2 Applet component record layout

- Step/experiment: `wp5-cap-applet-aid-index-artifact-check`
- Change: No source change. Parsed the supplied CAP Applet component with a bounded Python inspection to verify the lookup's size accounting and variable-length record layout.
- Validation: `AlgTest_v1.8.2_jc305.cap` contains Applet.cap component size 14, count 1, AID `4A43416C675465737431`, and big-endian install offset `0x3DA3` (15779); record traversal consumes the component exactly with zero trailing bytes.
- Result: The new lookup's component-size convention matches the supplied JCAlgTest v1.8.2 Applet component.
- Decision: Continue into applet lifecycle dispatch. Host hardware access is unavailable in this VM session (`lsusb` reports libusb initialization error -99; system bus/PCSC unavailable), so defer flashing until USB access returns.
- Commit: pending
- Artifacts/logs: supplied `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (source artifact; no new log)

## 2026-10-11 — Resolve applet AID across installed packages

- Step/experiment: `wp5-applet-registry-aid-lookup`
- Change: Added `FlashMemory_Handler::findAppletByAID()`. It walks the persistent package bitmap, parses each installed package CAP, skips packages without Applet components, and returns the matching package ID and install-method offset.
- Validation: `./tools/build arm` and `./tools/build riscv` passed; both generated their expected RP2350 UF2s. The supplied CAP layout was independently checked in the preceding step. `git diff --check` passed. Flashing was skipped because `picotool info -a` cannot initialize libusb (`-99`) in this VM session.
- Result: Static ROM-mask applets can now be resolved through one registry API using their AID. No install method is invoked, no applet instance is retained, and no APDU is dispatched yet.
- Decision: Next add interpreter entry/lifetime support for applet install and virtual process calls, with APDU data retained in context; then connect this registry query to a SELECT path.
- Commit: pending
- Artifacts/logs: none

## 2026-10-11 — Record package AID registry commit

- Step/experiment: `wp5-applet-registry-aid-lookup-commit-metadata`
- Change: Recorded the package-level AID lookup commit and its build result.
- Validation: ARM/RISC-V wrapper builds passed before the change was committed.
- Result: Implementation and required progress/handoff are committed as `fe7e12f`.
- Decision: Continue toward interpreter-backed applet install and APDU processing.
- Commit: pending
- Artifacts/logs: none

## 2026-10-11 — Reflash CHOUPI after Pico 2 replug

- Step/experiment: `wp5-pico2-reflash-aid-registry`
- Change: No source change during this hardware cycle. Rebuilt and flashed the ARM CHOUPI image containing the CAP AID parser and package registry lookup.
- Validation: `./tools/env ./tools/cycle choupi` passed ARM and RISC-V builds. The sandboxed attempt could not initialize libusb; with user-authorized device access, picotool saw the CDC device, but force-load could not locate its reset interface. The wrapper's CDC 1200-baud fallback entered BOOTSEL after 2 seconds, and normal UF2 load reached 100%. The bounded capture completed with repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`; no `TEST:FAIL` or `FAULT` was present. Post-flash picotool saw the re-enumerated RP2350 CDC device at bus 1/address 21.
- Result: The board is running the updated CHOUPI image. The flashing recovery sequence worked end-to-end through its CDC fallback. PC/SC currently reports no reader; no APDU or GPPro probe was made after this flash.
- Decision: Restore the PC/SC daemon/reader for the re-enumerated node, then continue with interpreter-backed applet lifecycle and APDU dispatch. The static registry lookup is not yet used for SELECT or install.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T111329Z.log` (SHA-256 `354482ef6091f85a04b5192fea4a17574600f2523042b465a6410f72768b8009`); flashed ARM UF2 SHA-256 `ec437a81244d1729c4d6713825d50c79daec6252cb22041b009819cbca8b8a45`

## 2026-10-11 — Short-circuit known Pico reset failures

- Step/experiment: `wp5-flash-recovery-error-classification`
- Change: Updated `tools/flash` to classify picotool failures. A missing reset interface now triggers the known-working CDC 1200-baud BOOTSEL fallback immediately; a libusb initialization failure exits without polling or issuing reset attempts that cannot work in the current host context.
- Validation: `bash -n tools/flash` and `git diff --check -- tools/flash` passed. Both architecture wrapper builds passed. The escalated hardware cycle reached the missing-reset-interface branch, used CDC fallback, entered BOOTSEL in 2 seconds, flashed to 100%, and passed repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. A sandboxed direct flash attempt produced the expected fast failure on `Failed to initialise libUSB`, without retries. No `TEST:FAIL` or `FAULT` appeared.
- Result: The real-board recovery path avoids 65 seconds of known-useless polling for this CCID composite, and the no-libusb context now fails immediately. The updated CHOUPI image remains on the board.
- Decision: Keep the wrapper's generic delayed BOOTSEL polling for other failure classes; restore PC/SC service/access before the next APDU probe.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T111923Z.log` (SHA-256 `0eadba75a08908f07b30507e136d9546cdc5b1e5ad705de91898ed70857bc584`); UF2 SHA-256 `ec437a81244d1729c4d6713825d50c79daec6252cb22041b009819cbca8b8a45`

## 2026-10-11 — Restart PC/SC after flash re-enumeration

- Step/experiment: `wp5-pcsc-postflash-reader-recovery`
- Change: Started the already-installed `pcscd.service` without sudo after the successful reflash and checked reader discovery.
- Validation: `systemctl is-active pcscd.service` reports `active`; `lsusb -d 2e8a:0009` sees the Pico 2 CHOUPI CCID at bus 001/device 023; `timeout 8s pcsc_scan -r` reports `No reader found`. The current device node is `/dev/bus/usb/001/023`; the prior node ACL did not follow re-enumeration. No source/build/flash change in this step.
- Result: USB device is present and firmware runtime passes, but pcscd cannot enumerate it under the current USB-node permissions. No GPPro/APDU probe was run.
- Decision: Grant user `pcscd` read/write access to `/dev/bus/usb/001/023`, restart `pcscd.service`, and rerun `pcsc_scan -r`; then continue with GPPro/APDU validation.
- Commit: pending
- Artifacts/logs: none

## 2026-10-11 — Stage narrow passwordless PC/SC recovery helper

- Step/experiment: `wp5-pcsc-nopasswd-helper`
- Change: Added a root-installed helper that dynamically identifies exactly one Pico 2 CHOUPI CCID (`2e8a:0009`), grants the `pcscd` account read/write access to its current USB node, and restarts `pcscd.service`. Added a one-time installer that installs the helper root-owned and creates a `visudo`-validated sudoers rule for the invoking user, limited to that command with no arguments.
- Validation: `bash -n tools/pico2-pcsc-reader-root tools/install-pcsc-sudoers` passed. `visudo -cf` accepted the generated exact command rule. No sudo command was run and no file outside the repository was modified. Hardware/PCSC recheck is pending user execution of the installer.
- Result: The repo now contains a narrow setup that can remove the repeated sudo prompt for this recovery action. The system sudoers file and root helper remain uninstalled until the user runs the command below.
- Decision: User runs `sudo ./tools/install-pcsc-sudoers` once, then `sudo -n /usr/local/sbin/pico2-pcsc-reader`; recheck with `pcsc_scan -r` and continue APDU/GPPro validation.
- Commit: `97c7eeb`
- Artifacts/logs: none

## 2026-10-11 — Correct visudo preflight path

- Step/experiment: `wp5-pcsc-installer-visudo-path`
- Change: Corrected the installer preflight to check `/usr/sbin/visudo`, matching this host's installed path and the path already used for sudoers validation.
- Validation: `bash -n` passed for both helper scripts; `/usr/sbin/visudo` is executable and parsed the generated no-argument command rule successfully. The user's first installer attempt exited at preflight before copying the helper or changing sudoers.
- Result: The installer can proceed past the missing-command check on this host. Full installation and PC/SC recheck await the user's rerun.
- Decision: Rerun `sudo ./tools/install-pcsc-sudoers`, then run the installed helper and check `pcsc_scan -r`.
- Commit: `5421889`
- Artifacts/logs: none

## 2026-10-11 — Repeat GPPro probe after PC/SC recovery

- Step/experiment: `wp6-gppro-list-reader-recovered`
- Change: No firmware change. Re-ran GlobalPlatformPro 26.06.04 against the reader after the user-installed helper restored `pcscd` access.
- Validation: The elevated host-side read-only probe connected to `Pico 2 CHOUPI CCID`, negotiated T=1 with ATR `3B800181`, and sent `00A4040000`. The card returned `6D00`; GPPro exited with `Could not SELECT default selected: 0x6D00 (Invalid INStruction)`. `pcsc_scan -r` had already confirmed the reader. No CAP install or write APDU was sent.
- Result: USB, CCID, PC/SC and APDU transport work. The remaining immediate failure is firmware's placeholder APDU response; the default-selected ISD path and GlobalPlatform commands are not implemented.
- Decision: Implement explicit default-ISD SELECT handling, rebuild both architectures, flash CHOUPI, and use GPPro's next trace to drive the command handler.
- Commit: `48a6656`
- Artifacts/logs: `artifacts/logs/gp-list-probe-20261011-reader-ready.txt` (SHA-256 `14a11819fc90b44f3c4e3a9a9b3800e6904c9ca3472518718230a94dc2b54f21`)

## 2026-10-11 — Return default ISD FCI for SELECT

- Step/experiment: `wp6-default-isd-select-fci`
- Change: The CCID APDU handler now recognizes the exact short SELECT-by-empty-AID command, identifies the default ISD as `A000000151000000`, and returns its minimal `6F/84` FCI plus `9000`. Other APDUs still return `6D00`; this does not yet implement a card manager or applet selection.
- Validation: `./tools/env ./tools/cycle choupi` passed ARM and RISC-V builds, flashed the ARM image, and captured repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`, with no `TEST:FAIL` or `FAULT`. GPPro parsed the corrected FCI and identified the ISD. It then sent SCP03 `INITIALIZE UPDATE` (`8050000008` plus an 8-byte host challenge), which still receives `6D00`; `gp --list` therefore remains unsuccessful. An earlier FCI iteration exposed an outer TLV length error (`6F0C`); it was corrected to `6F0A` before the passing cycle and final probe.
- Result: Default ISD selection now proceeds through FCI discovery over real PC/SC/CCID. The next blocker is secure-channel initialization and then GET STATUS/card-manager responses.
- Decision: Implement and validate SCP03 `INITIALIZE UPDATE`/`EXTERNAL AUTHENTICATE` on the default test keys, then continue GPPro listing and CAP loading.
- Commit: `9e9e3ae`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T114816Z.log` (SHA-256 `b9afd73bf1c011b45c69944f3c0189d3f449cec2169823d1b8cd08555f9ad02d`); `artifacts/logs/gp-list-probe-20261011-isd-fci-fixed.txt` (SHA-256 `da56ac7750d48a44cd75f5ce6949c7596bc26e98d4d38b6cccd9153fdc865cd7`); malformed-FCI trace `artifacts/logs/gp-list-probe-20261011-isd-fci.txt`

## 2026-10-11 — Probe unwrapped GP status and data commands

- Step/experiment: `wp6-raw-gp-readonly-probes`
- Change: No firmware change. Sent read-only GET STATUS (`80F20000024F00`), GET DATA CPLC (`80CA9F7F00`), and GET DATA Card Data (`80CA006600`) APDUs through GPPro's raw APDU path.
- Validation: All three commands reached the real board over PC/SC/T=1 and returned `6D00`; GPPro's automatic discovery then selected the default ISD and received the valid FCI with `9000`. No write APDU or CAP operation was sent.
- Result: The CCID path correctly carries short APDUs, but GET STATUS, GET DATA, and secure-channel commands are all still unsupported. GPPro's normal `--list` cannot proceed until SCP03 works because that mode authenticates first.
- Decision: Implement SCP03 first, then add tag-form GET STATUS and read-only GET DATA responses before attempting any LOAD or INSTALL command.
- Commit: `ab833a3`
- Artifacts/logs: `artifacts/logs/gp-raw-readonly-probes-20261011.txt` (SHA-256 `8795b672b2dd211d2ccc18ac4f5e05579671b6f22f49adaa2d7752fbefb4df5b`)

## 2026-10-11 — Authenticate SCP03 and report ISD status

- Step/experiment: `wp6-scp03-authenticate-and-list`
- Change: Added AES-CMAC SCP03 session derivation and command authentication to the CCID APDU path. The session KDF encodes 128-bit output for session keys and 64-bit output for card/host cryptograms. EXTERNAL AUTHENTICATE accepts the short case-4 form including Le, validates the host cryptogram and C-MAC, and enables authenticated tag-form GET STATUS for the ISD.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` flashed the real Pico 2 and passed its bounded serial checks (`TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, no `TEST:FAIL` or `FAULT`). GlobalPlatformPro 26.06.04 `--debug --list` completed the SCP03 exchange: it accepted the card cryptogram, EXTERNAL AUTHENTICATE returned `9000`, and GET STATUS returned the ISD entry with lifecycle `OP_READY`. The subsequent registry probes returned `6A88` for absent applets/domains; `gp --list` exited 0.
- Result: Default SCP03 authentication with test key `404142434445464748494A4B4C4D4E4F` and ISD discovery now work against the physical board. The v1.8.2 CAP is not installed yet; GP LOAD/INSTALL APDUs remain unsupported.
- Decision: Continue with a GPPro install attempt to identify the first unsupported CAP-management command, then implement the smallest required sequence.
- Commit: `ad08669`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T120932Z.log` (SHA-256 `b81dff57dafdea77f6b55412eb4c72b297f2a458a7002bcab28ce8d28fc81839`); `artifacts/logs/gp-list-probe-20261011-scp03-external-auth.txt` (SHA-256 `daf3e55a9f829a1fbe581e944e960ecb2c58607e2d7b6c493b08d2a6141b9eef`)

## 2026-10-11 — Probe JCAlgTest v1.8.2 GP installation

- Step/experiment: `wp7-gp-install-first-command`
- Change: No firmware change. Attempted GlobalPlatformPro installation of the exact supplied `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` (SHA-256 `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`).
- Validation: SCP03 authentication and GPPro's read-only registry probes succeeded. The first load command was `84 E6 02 00 1E ...` (INSTALL [for load]); firmware returned `6D00`, and GPPro exited 1. No LOAD block or successful install command followed, so the CAP was not written or installed.
- Result: Card discovery and secure messaging are functional. GP LOAD/INSTALL command dispatch is the next blocker.
- Decision: Implement bounded parsing/state for INSTALL [for load], then capture GPPro's LOAD block framing and decide how the board can persist the supplied CAP before accepting further install commands.
- Commit: `726d8e8`
- Artifacts/logs: `artifacts/logs/gp-install-probe-20261011.txt` (SHA-256 `a12680db8ddcf252b83f7e584f95183f48b12f1ef59d814f132fa9efcbb82d0f`)

## 2026-10-11 — Transfer and select JCAlgTest v1.8.2

- Step/experiment: `wp7-gp-cap-load-and-select`
- Change: Added bounded GlobalPlatform LOAD block sequencing and C4 TLV reassembly for the supplied JCAlgTest package, validates its CAP component boundaries/package AID/applet AID, stores the raw CAP code in the CHOUPI runtime overlay, accepts INSTALL [for install and make selectable], reports the installed instance in GET STATUS, and returns FCI for SELECT-by-instance-AID. Increased the RAM overlay from 32 KiB to 64 KiB and sized SCP03 command-MAC workspace for the full 247-byte short-APDU payload.
- Validation: ARM and RISC-V wrapper builds passed. `./tools/cycle choupi` flashed the Pico 2 and emitted `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`, no `TEST:FAIL` or `FAULT`. GlobalPlatformPro 26.06.04 `--install` loaded the exact supplied CAP (hash `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`): all LOAD blocks and final INSTALL returned `9000`. A subsequent `--apdu 00A404000A4A43416C675465737431` returned FCI `6F0C840A4A43416C675465737431 9000`; GPPro `--list` had already confirmed the instance as `INITIALIZED` under the JCAlgTest package.
- Result: Runtime GlobalPlatform CAP transfer, registration, and AID selection work on hardware. The Pico port's overlay is RAM-only and resets on reboot, so this install is not persistent yet; applet APDU processing is also not implemented.
- Decision: Add a dedicated, bounded nonvolatile CAP slot or another persistence backend, preserving the existing ROM image and validating that the dynamic package survives reboot before calling installation complete. Then wire selected-applet APDUs into the CHOUPI lifecycle.
- Commit: `1107aaa`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T122830Z.log` (SHA-256 `d14597b132051f6d730b1ab952d397a39cae3f53c86bb6e899c8d41258d2ca81`); `artifacts/logs/gp-install-cap-select-enabled-20261011.txt` (SHA-256 `624fc4f958fee15d14a1b418da77e9959dc0421ccab90d0bbcd6b9ed5928e97e`); `artifacts/logs/gp-list-after-cap-select-enabled-20261011.txt` (SHA-256 `aba4339bfb43aac4c105fb8bb57974bc03fed9fd18f000ea6ac9262274eee80d`); `artifacts/logs/gp-select-algtest-probe-20261011-enabled.txt` (SHA-256 `c5d723732fac9ec030bea764f4446edc7680894535f7fff0668b1c6271d2b97b`)

## 2026-10-11 — Persist JCAlgTest CAP across firmware reboot

- Step/experiment: `wp7-persistent-cap-slot`
- Change: Added a bounded 256 KiB flash slot at offset `0x1C0000` for the JCAlgTest CAP payload and CRC-protected install metadata. The code image ends at `0x1009FED4`, below the slot. CAP restoration now runs when the CCID card is powered on, after `fs_init()` has validated flash. The persisted package is kept separate from CHOUPI's embedded filesystem lookup so the existing boot runtime continues to use its own package image.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` flashed the real Pico 2 and passed with `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`, with no `TEST:FAIL`, `FAULT`, or timeout. GlobalPlatformPro installed the supplied v1.8.2 CAP successfully (final INSTALL `9000`). After two firmware reboots, `gp --debug --list` showed `4A43416C675465737431 (INITIALIZED)` under package `4A43416C6754657374`; SELECT-by-AID returned `6F0C840A4A43416C675465737431 9000`.
- Result: The exact 26,743-byte CAP payload and applet instance metadata survive a firmware update/reboot and are restored into the CCID registry. An initial reboot trial exposed a filesystem lookup collision that caused `VM_EXCEPTION id=7`; separating persistent CAP storage from the embedded runtime lookup fixed it, and the subsequent hardware cycle passed. Applet install-method execution and applet APDU processing remain unimplemented.
- Decision: Persistence and AID selection are now verified. Next implement actual applet installation/lifecycle execution and APDU dispatch through the CHOUPI VM before describing JCAlgTest as executable.
- Commit: `a42a2a9a05ab65ad92edcb0729735d8cdac62bce`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T123949Z.log` (SHA-256 `2d1b24d01b181a568091a8dde4d73b36fe2242533fb2ccdbcd42b20665b2274e`); `artifacts/logs/gp-install-persistent-cap-20261011.txt` (SHA-256 `b1f53baf3f6dff3a76254887f5d1715c5c7f292767cd98a97cdbdd1946304b88`); `artifacts/logs/gp-list-persistent-cap-20261011.txt` (SHA-256 `8d0488571acaa39bc0acf779adc130e180d56787b13ab8d171ac84210a47ac92`); `artifacts/logs/gp-select-persistent-cap-20261011.txt` (SHA-256 `6e022dca655b14d3d649feef486b6326405414c5954e74f7f7d2a2999df2bb21`)

## 2026-10-11 — Resolve persisted CAP through the CHOUPI VM

- Step/experiment: `wp7-vm-dynamic-cap-resolution`
- Change: Exposed the flash-backed JCAlgTest CAP to `FlashMemory_Handler` as reserved package ID 63, added `getCap(63)`, and taught the VM's existing `findAppletByAID` path to read the installed CAP's applet component and install-method offset. Applet SELECT now requires this VM lookup to resolve the stored AID before returning its FCI. The resolver checks the dynamic CAP first so it does not scan unrelated ROM packages inside the USB service path; exceptions from malformed package lookup are contained at the C bridge.
- Validation: ARM and RISC-V wrapper builds passed. `./tools/cycle choupi` flashed the Pico 2 and passed `TEST:SUMMARY pass=1 fail=0`, with no `TEST:FAIL`, `FAULT`, or timeout. GPPro's real SELECT-by-AID returned `6F0C840A4A43416C675465737431 9000` in 55 ms, which now requires CHOUPI's CAP parser to find the stored AID and install offset. An earlier resolver ordering timed out at 16.7 seconds and triggered watchdog recovery; moving the dynamic lookup ahead of ROM enumeration fixed the APDU path.
- Result: The persistent CAP is now connected to CHOUPI's package parser and AID resolver, while static package 20 remains unchanged. The install method is not invoked yet, and APDUs are still not dispatched to applet code.
- Decision: Next implement a bounded VM entry path for the CAP install method and then route applet APDUs through a persistent selected-applet context.
- Commit: `69616aea484ebf27307ec6fb758a71e99f1b682d`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T134038Z.log` (SHA-256 `eb7da84d5f0a1250269fab46c1f0c3b8ac2df3c15b0659b5fc518a511da32a03`); `artifacts/logs/gp-select-vm-cap-resolver-20261011.txt` (SHA-256 `ee917c37eb53abfa4564c9ecb820c448d271e49bae12fcb7f34f657ef650af0f`)


## 2026-10-11 — Add raw CAP method invocation entry

- Step/experiment: `wp7-raw-cap-method-entry`
- Change: Added an interpreter constructor for invoking a static method directly from its CAP Method-component byte offset. It seeds the three Java Card method locals for `Applet.install(byte[], short, byte)` (array reference, offset, length), while leaving existing exported-method startup unchanged.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real-board `./tools/cycle choupi` passed after USB access was restored: the ARM image flashed to 100%, repeated `TEST:PASS choupi_runtime`, and `TEST:SUMMARY pass=1 fail=0`; no `TEST:FAIL`, `FAULT`, or timeout occurred. GlobalPlatformPro read-only SELECT-by-AID returned the persisted JCAlgTest FCI and `9000` in 9 ms; default ISD selection also returned `9000`.
- Result: The VM now has the entry primitive needed to call the CAP's install method at its parser-provided offset. This step did not invoke that method or alter applet lifecycle state; `JCSystem.getAvailableMemory` still throws `NotYetImplemented`, and `Applet.register()` remains a no-op.
- Decision: Preserve and parse GP INSTALL parameters, then connect the method entry to install and implement the first native behavior needed by the supplied applet.
- Commit: `3e49fc9c9cec1c38aeed431b2ba281ccb3556934`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T134532Z.log` (SHA-256 `ecd917b5a265987b417bcfccf33365104e95e255fefed8d93d8aaeda03415884`); `artifacts/logs/gp-select-install-entry-20261011.txt` (SHA-256 `c8120829b3c7941b2bd2e48adb7db34ff8f7ff57b7fe0c9617eabed9046d74ab`)

## 2026-10-11 — Bound and trace stalled CAP INSTALL execution

- Step/experiment: `wp7-install-watchdog-trace`
- Change: Added a five-second wall-clock budget to the raw install-method interpreter entry, with opcode/PC diagnostics; exposed the Pico timer through a platform bridge. Added watchdog scratch breadcrumbs for the last INSTALL opcode and repeatable recovery reporting, and kept CHOUPI running after a watchdog restart so the CCID/CDC device can re-enumerate without immediately entering BOOTSEL.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` flashed the real Pico 2 to 100% and passed `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` in `artifacts/logs/cycle-choupi-20261011T145623Z.log`. GlobalPlatformPro reloaded the exact v1.8.2 CAP (hash `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`), but INSTALL did not return; the subsequent transmit failed with `SCARD_F_COMM_ERROR`. CDC capture stopped producing output during the APDU. USB still enumerated as `2e8a:0009`, while pcscd had no usable reader. A later cycle retry could not reset this stalled device through picotool or CDC 1200-baud fallback.
- Result: The watchdog and cycle work for normal runtime startup, but they have not contained or diagnosed the blocked INSTALL callback. The trace path is built and flashed; it still needs a successful watchdog-reset capture or other device reattachment to identify the last opcode.
- Decision: Once the USB device is reattached or reset, collect the saved watchdog breadcrumb and continue isolating the blocking VM/native operation. Do not mark JCAlgTest installed: GP INSTALL has not returned `9000`.
- Commit: `d86489e`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T145623Z.log` (SHA-256 `3d100ceb51d258af55cef828aca9c0cdeeec31ff20a3bd6ae209f5dd7bc910ab`); `artifacts/logs/gp-install-watchdog-scratch-20261011-apdu.txt` (SHA-256 `1dce95fd4a3a37238e9343ab6056a32e08436a8c2d261c64956afa2144b02198`); `artifacts/logs/gp-install-watchdog-scratch-20261011.txt` (SHA-256 `2db80438b43563cd14175a572822f6a631fcda0d0a8fb689a33bc7c572fcf838`)

## 2026-10-11 — Invoke the JCAlgTest CAP INSTALL method

- Step/experiment: `wp7-gp-install-method-execution`
- Change: Connected authenticated GP INSTALL to bounded CAP parsing, persisted CAP resolution, and the CAP-declared `Applet.install(byte[], short, byte)` method entry. Added install instance metadata only after the method returns `9000`, reported failed INSTALL diagnostics over CCID, returned zero for unmeasured `JCSystem.getAvailableMemory` capacities, and corrected CAP method offsets for the exception-handler table. Fixed reversed public/package method visibility checks that blocked v1.8.2 constructor dispatch.
- Validation: `./tools/build arm` and `./tools/build riscv` passed, and the real-board cycle at `artifacts/logs/cycle-choupi-20261011T145623Z.log` passed `TEST:SUMMARY pass=1 fail=0`. GPPro loaded the supplied 26,743-byte v1.8.2 CAP successfully, but its authenticated INSTALL APDU did not return; attempts ended in `SCARD_E_READER_UNAVAILABLE` or `SCARD_F_COMM_ERROR`. No successful applet instance registration has been verified.
- Result: CAP parsing, method-offset resolution, and the request path are connected. The runtime stalls during INSTALL before returning `9000`; applet installation is incomplete.
- Decision: Resume after USB/PCSC recovery, capture callback/VM progress, and fix the operation that stalls before retrying INSTALL. Do not claim the JCAlgTest applet is installed.
- Commit: `7315ae1`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T145623Z.log` (SHA-256 `3d100ceb51d258af55cef828aca9c0cdeeec31ff20a3bd6ae209f5dd7bc910ab`); `artifacts/logs/gp-install-watchdog-scratch-20261011-apdu.txt` (SHA-256 `1dce95fd4a3a37238e9343ab6056a32e08436a8c2d261c64956afa2144b02198`)

## 2026-10-11 — Keep USB serial output out of INSTALL callbacks

- Step/experiment: `wp7-no-serial-in-install-callback`
- Change: Removed `printf`/`fflush` diagnostics from the CCID INSTALL, CAP loader, and raw install-method call path. Suppressed interpreter trace/progress/exception serial output for raw CAP-method execution while preserving its structured status and diagnostic response fields. This avoids depending on USB CDC servicing recursively from the TinyUSB CCID transfer callback.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` was attempted after both builds, but the active USB device did not enter BOOTSEL: picotool reported no reset interface and the CDC 1200-baud fallback timed out. Therefore the edited image was not flashed and this runtime hypothesis remains unverified on hardware.
- Result: The candidate deadlock source is removed from the next build. The currently attached Pico still runs the prior image and its CCID/CDC paths remain unresponsive after the failed INSTALL.
- Decision: Reattach/reset the Pico USB device, flash this image, then retry JCAlgTest v1.8.2 INSTALL with bounded PC/SC/CDC logs. If it still stalls, use the structured INSTALL diagnostic response and watchdog scratch trace to isolate the VM/native operation.
- Commit: `005271c`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T151333Z.log` (SHA-256 `eb2a3afb5acc37989db615a51dafa3ca610836c50ed8fe049611c52953e72d2b`); candidate UF2 `build/arm/pico2_choupi.uf2` (SHA-256 `418c69e4e4c11f82e0866cf98479e9c1e61244e298044a7c1110ee9950900504`)

## 2026-10-11 — Flash and retry callback-safe JCAlgTest INSTALL

- Step/experiment: `wp7-callback-safe-install-hardware-retry`
- Change: Flashed the previously unflashed callback-safe CHOUPI image and retried the exact supplied JCAlgTest v1.8.2 CAP installation after restoring PC/SC access with the user-approved helper.
- Validation: `./tools/build arm` and `./tools/build riscv` passed; `./tools/cycle choupi` entered BOOTSEL through its CDC 1200-baud fallback, flashed to 100%, and passed `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. GlobalPlatformPro authenticated, all CAP LOAD blocks returned `9000`, then the final authenticated `84E60C00` INSTALL did not return within 60 seconds (timeout status 124). The board continued enumerating as `2e8a:0009`, but the reader disappeared and serial capture returned no diagnostics. The PC/SC recovery helper, picotool reboot, CDC reset, and USB bus reset did not restore it.
- Result: The no-serial-write change is now validated on hardware and does not resolve the INSTALL stall. No successful applet installation or registry confirmation occurred.
- Decision: Continue after a board USB detach/reattach or power reset; add targeted persistent CCID/VM stage breadcrumbs to distinguish a blocked bytecode/native call from an in-callback storage or USB deadlock. Keep the applet status unconfirmed until INSTALL returns `9000` and GET STATUS/SELECT verify it.
- Commit: `005271c` (firmware under test); hardware-validation record: `53adb84`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T152212Z.log` (SHA-256 `4ab97132130d7c775e40dcd5c1a54a79b93542aa215d7d6a88caf7cd22ee4db5`); `artifacts/logs/gp-install-callback-safe-20261011.txt` (SHA-256 `c3c4dbf12611b67c7f8bcfa4a531dececac7d35babe810d01ca188ccf8e13746`); candidate/flashed UF2 SHA-256 `418c69e4e4c11f82e0866cf98479e9c1e61244e298044a7c1110ee9950900504`

## 2026-10-11 — Defer CCID APDU processing out of TinyUSB IRQ

- Step/experiment: `wp7-deferred-ccid-apdu-processing`
- Change: The Pico SDK services TinyUSB from a low-priority worker IRQ, while the CCID OUT transfer callback previously performed the complete APDU synchronously, including Java VM INSTALL. The callback now queues a complete CCID message; `choupi_ccid_task()` processes it from CHOUPI's main loop and sends its response there. The OUT endpoint remains paused until the response transfer completes, preserving command order.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed. The real-board `./tools/cycle choupi` was attempted after both builds; it could not reset the stalled board. Picotool saw no BOOTSEL/reset interface, the initial BOOTSEL wait timed out after 45 seconds, three reboot requests failed, and CDC 1200-baud reset did not re-enumerate BOOTSEL. No hardware result for this change is available; the candidate was not flashed.
- Result: Long VM/install work no longer runs in the TinyUSB USB worker IRQ in the candidate firmware. This is a concrete concurrency fix based on the Pico SDK call path, but hardware impact remains unverified.
- Decision: Restore USB passthrough or power-cycle the board, flash this image, confirm the runtime self-test, then retry JCAlgTest v1.8.2 INSTALL. Keep the install status unconfirmed unless GPPro receives `9000` and GET STATUS/SELECT confirm it.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T153015Z.log` (SHA-256 `6c0412947bb6d39e26210f5ff214ca26bae5709bd3b9e7190978d244e56dde1f`); candidate UF2 `build/arm/pico2_choupi.uf2` (SHA-256 `f34f79fce442792fb099e00977521c73e3d03f7892d7f9483cb34e4038016fab`)

## 2026-10-11 — Serialize CCID endpoint operations with TinyUSB polling

- Step/experiment: `wp7-ccid-single-context`
- Change: Disabled Pico SDK's background TinyUSB IRQ task for the CHOUPI image. The main loop now polls `tud_task()` continuously and handles deferred CCID messages in the same context, and prints bounded status output once per second. This follows evidence that two CCID requests were processed and responses queued (debug stage 6), but no IN-completion callback ran while endpoint operations crossed the IRQ/main-loop contexts.
- Validation: `./tools/env ./tools/cycle choupi` built ARM and RISC-V successfully and flashed the image to 100%, but exited 1 because its finite monitor found no USB serial node after re-enumeration. After the user reinserted BOOTSEL, `./tools/env ./tools/flash build/arm/pico2_choupi.uf2` detected BOOTSEL and completed with `FLASH:PASS`; a subsequent `./tools/env ./tools/monitor --seconds 18` also exited 1 because the Pico was no longer enumerated in the VM. `pcsc_scan -r` did not discover a reader before the reflash. No install APDU was attempted in this step.
- Result: The dual-build and flash gates pass; runtime self-test and CCID endpoint completion are unverified because USB passthrough dropped after the BOOTSEL-to-application reboot. JCAlgTest installation remains unconfirmed.
- Decision: Resume finite CDC capture and PC/SC/CCID validation when the Pico is visible in the VM, then retry GPPro INSTALL. Require INSTALL `9000` plus GET STATUS/SELECT confirmation before reporting success.
- Commit: `a62514564543790ad4d0a5719c81b7d5638c1e1e`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T161525Z.log` (SHA-256 `caaa3246aa39d510563430e4c1a8c7fee6550dffa76205a2a338899fc8885ad4`); flashed `build/arm/pico2_choupi.uf2` (SHA-256 `5f33bf01f01f462eaf731623696aa8b2e36a475b036760ef47fd48e15a43674f`)

## 2026-10-11 — Reflash after BOOTSEL reattachment

- Step/experiment: `wp7-application-pid-passthrough`
- Change: Retried the CHOUPI ARM image after the user reattached the board. `picotool info -a` identified the device as RP2350 BOOTSEL (`2e8a:000f`); the repository flash wrapper loaded the UF2 to 100% and reported `FLASH:PASS`.
- Validation: `./tools/env ./tools/monitor --seconds 18` exited 1 with `ERROR: no USB serial device found` after the firmware reboot. Elevated `lsusb` then listed only the VM root hubs and VirtualBox tablet; no RP2350 USB device or serial node was available. `VBoxManage` is not installed in the guest, so application-mode USB passthrough cannot be attached from here.
- Result: The application image is flashed, but runtime output, CCID, PC/SC, and JCAlgTest INSTALL remain unverified. No GP INSTALL command was sent.
- Decision: Attach the re-enumerated application device (`2e8a:0009`) in the VirtualBox USB menu while leaving the Pico running CHOUPI, then resume the serial and PC/SC checks.
- Commit: `a62514564543790ad4d0a5719c81b7d5638c1e1e` (implementation; retry evidence appended in follow-up record commit)
- Artifacts/logs: flashed `build/arm/pico2_choupi.uf2` (SHA-256 `5f33bf01f01f462eaf731623696aa8b2e36a475b036760ef47fd48e15a43674f`); no persistent serial artifact because the application USB device was not captured by the VM.

## 2026-10-11 — Poll USB during early CHOUPI startup

- Step/experiment: `wp7-tinyusb-startup-poll`
- Change: Added explicit `tud_task()` polling to the pre-runtime CDC wait loop. With the IRQ worker disabled for single-context CCID processing, the earlier startup loop only checked `stdio_usb_connected()`; when disconnected, stdio output did not poll TinyUSB, so the application could remain unenumerated indefinitely.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` both passed. The current `./tools/env ./tools/flash build/arm/pico2_choupi.uf2` is waiting for RP-series USB re-enumeration because the prior image is not servicing USB during startup. Hardware flash and serial validation of this correction are pending BOOTSEL reattachment; not run yet.
- Result: The enumeration deadlock is addressed in the candidate image; no runtime or CCID result is claimed. The JCAlgTest CAP is not confirmed installed.
- Decision: Flash this candidate when BOOTSEL is visible, then run the finite monitor, refresh PC/SC permissions, verify the reader and CCID response completion, and repeat the JCAlgTest v1.8.2 INSTALL.
- Commit: `ca9bba37aeb458c848bcfd85b4e301a3351801e`
- Artifacts/logs: candidate `build/arm/pico2_choupi.uf2` (SHA-256 `e6b0802c22f6a07fe1a908782429c22027ed047666c67d7a331a00d5ffc78f8c`); build output not archived.

## 2026-10-11 — Validate USB startup and retry JCAlgTest installation

- Step/experiment: `wp7-startup-usb-validation-install-retry`
- Change: Flashed the startup-polling CHOUPI ARM image after the user reattached the Pico in BOOTSEL; restored PC/SC access and retried the exact supplied JCAlgTest v1.8.2 CAP.
- Validation: ARM and RISC-V wrapper builds had passed for this image. `./tools/env ./tools/flash build/arm/pico2_choupi.uf2` wrote 100% and returned `FLASH:PASS`. `./tools/env ./tools/monitor --seconds 18` captured repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0` with no `TEST:FAIL` or `FAULT`. The authorized PC/SC helper reported `PCSC:ACCESS_GRANTED`; `pcsc_scan -r` found `Pico 2 CHOUPI CCID`. GlobalPlatformPro `--list` connected over T=1 and returned the ISD in `OP_READY`, with no applet instance. GPPro authenticated using SCP03 and all CAP LOAD blocks returned `9000`; final authenticated INSTALL `84E60C00` did not return. After about 17 seconds, GPPro reported `SCARD_E_READER_UNAVAILABLE` and the Pico disappeared from guest USB. A subsequent `lsusb` showed no Pico and `pcsc_scan -r` showed no reader.
- Result: CHOUPI startup, serial, and PC/SC reader discovery now work on the real board. The CAP load is accepted, but the install method still wedges the device before INSTALL `9000`; JCAlgTest v1.8.2 is not installed or confirmed.
- Decision: Recover BOOTSEL/application USB passthrough and inspect the persisted install watchdog breadcrumb or instrument the operation that stalls before retrying INSTALL. Do not report the applet installed without a successful INSTALL and GET STATUS/SELECT confirmation.
- Commit: pending
- Artifacts/logs: `artifacts/logs/gp-install-retry-20261011.txt` (SHA-256 `6287cb1c09e0855913307e521eddaa48a3cbe60baa2133c3033ef9ac28125f0b`); flashed UF2 `build/arm/pico2_choupi.uf2` (SHA-256 `e6b0802c22f6a07fe1a908782429c22027ed047666c67d7a331a00d5ffc78f8c`)

## 2026-10-11 — Pinpoint the first blocking INSTALL operation

- Step/experiment: `wp7-install-new-substage-trace`
- Change: Added watchdog-persistent stages around the Java Card `new` bytecode's constant-pool resolution and object allocation, and included the stage in the post-watchdog CDC report.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` passed. The repository flash wrapper entered BOOTSEL through its CDC 1200-baud recovery and flashed the ARM image to 100% (`FLASH:PASS`). `./tools/env ./tools/monitor --seconds 8` passed repeatedly with `TEST:SUMMARY pass=1 fail=0`. The authorized PC/SC helper restored access; GPPro authenticated and loaded every v1.8.2 CAP block (`9000`), but final INSTALL `84E60C00` caused `SCARD_E_READER_UNAVAILABLE` after about 17 seconds. CDC recovery reported `pc=101C4EFA opcode=8F count=1 stage=2`; stage 2 means constant-pool resolution completed and the stall occurred inside `Heap::addInstance` before returning to `bc_new`.
- Result: The watchdog recovery and serial reconnect can now retain a specific VM substage across the reset. The first blocking bytecode is `new`; the object allocation path is the failing operation. No INSTALL status `9000` or applet registry confirmation was received.
- Decision: Instrument `Heap::addInstance`, the `JC_Instance` constructor, and the superclass field-size walk to identify the exact blocking suboperation before another bounded install retry.
- Commit: pending
- Artifacts/logs: `artifacts/logs/gp-install-substage-20261011.txt` (SHA-256 `80e4a7b35042ecaa970834a9df197f32b568435398c8eb6b23f9312a6fc1ca53`); `artifacts/logs/cdc-install-stage2-recovery-20261011.txt` (SHA-256 `739eb43c42d4925539a55abdf620a5cbcd8c5798e6d3cca0bd0aeafb1fe859e6`); flashed UF2 `build/arm/pico2_choupi.uf2` (SHA-256 `f38acbf07f0720ade74385d25f3f694e48596bd761e3aa150f2b925063c11cf3`)

## 2026-10-11 — Trace the blocked superclass lookup

- Step/experiment: `wp7-install-superclass-resolution-trace`
- Change: Added persistent markers around heap insertion, JC_Instance field-count/allocation, superclass traversal, imported package lookup, and class-table lookup. The recovery report now includes the current class-reference/import context.
- Validation: `./tools/env ./tools/build arm` and `./tools/env ./tools/build riscv` passed. The repository flash wrapper entered BOOTSEL through CDC 1200-baud recovery and flashed the ARM UF2 to 100% (`FLASH:PASS`). After delayed USB serial re-enumeration, the 8-second monitor repeatedly reported `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The authorized helper and `pcsc_scan -r` found the reader. GPPro authenticated and all CAP LOAD blocks returned `9000`; final `84E60C00` INSTALL again caused `SCARD_E_READER_UNAVAILABLE` after about 17 seconds. The subsequent recovery monitor showed the runtime self-test but did not report a watchdog marker. The previous instrumented image's persistent result was `pc=101C4EFA opcode=8F count=1 stage=6`, immediately before superclass `classref2class` resolution.
- Result: Heap allocation is narrowed to superclass lookup, but the finer import/class-table stage was not recovered from this install attempt. The applet is not installed; no INSTALL `9000`, GET STATUS entry, or SELECT confirmation occurred.
- Decision: Preserve this diagnostic build on the Pico. Retry once with a serial capture ready before INSTALL and keep the host monitor attached across the expected USB reset; inspect resolver stage/context, then fix the failing lookup.
- Commit: pending
- Artifacts/logs: `artifacts/logs/gp-install-superclass-resolution-20261011.txt` (SHA-256 `d5e669e6042dc5625fea56e1a4b32a7f17a568c52a8a6e444027140530f3d19c`); `artifacts/logs/cdc-superclass-stage-recovery-20261011.txt` (SHA-256 `2e06bd348650e34e5ee701977428ce6760aad05200c33f4a7403cbf705b06eab`); prior stage 6 capture `artifacts/logs/cdc-instance-stage-recovery-20261011.txt` (SHA-256 `c8a9e99819358f55af70ed3b050d66bc67709f5efa3b43dd170dead87e6b96e4`); flashed UF2 `build/arm/pico2_choupi.uf2` (SHA-256 `353781d080f00aa25da77990dc4eb6868b69dfcb2e00ea8fbe83a82c44d07af9`)

## 2026-10-11 — Normalize imported class offsets before Class lookup

- Step/experiment: `wp7-install-class-offset-endian-fix`
- Change: Applied `HTONS()` to the class-component offset returned by the Export component before indexing the imported package's Class component. On the little-endian RP2350, the prior code used a CAP byte-order value of `0x0300` as an offset instead of `0x0003`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The elevated repository flash wrapper used its CDC 1200-baud reset fallback, detected BOOTSEL after 2 seconds, wrote 100%, rebooted, and returned `FLASH:PASS`. The 8-second serial monitor reported repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. PC/SC found the reader and GPPro `--list` returned the ISD `OP_READY`. The exact v1.8.2 CAP LOAD completed with `9000`; final INSTALL `84E60C00` again wedged the card and returned `SCARD_E_READER_UNAVAILABLE` after 16.9 seconds. Reconnected CDC captured `WATCHDOG_RECOVERY ... opcode=8F count=1 stage=17 context=00000003`, proving class-table resolution advanced past the prior invalid `0x0300` offset. No INSTALL `9000`, GET STATUS instance, or SELECT confirmation was received; the CAP is not confirmed installed.
- Result: Imported class export tokens now resolve to correctly byte-swapped Class component offsets. A later operation in `resolveClassref` / superclass field sizing still blocks during Java Card `new`.
- Decision: Add persistent markers immediately after resolver checks and through `classref2class` / `getInstanceFieldsSize`, then rebuild both architectures, flash, and retry once.
- Commit: `86b96a3c2aefb14dbf698257271fa0ae90c58f9b`
- Artifacts/logs: candidate UF2 `build/arm/pico2_choupi.uf2` SHA-256 `5709ce1b628672e6e172c3bdd6d93a45a66b4dd75095bb3cdbb8c4885bbf3126`; runtime capture `artifacts/logs/cdc-class-offset-endian-fix-20261011.txt` SHA-256 `43a49c9f7d953d7447c67ffe6e4e691a2e31e66da55206ecc20c5ba9af42f6a4`; install trace `artifacts/logs/gp-install-class-offset-endian-fix-20261011.txt` SHA-256 `b74b66418190e6cb53667b7eefe75ac31b4eb6dd3b6ef8ba637a9557b9f8cd72`; recovery trace `artifacts/logs/cdc-class-offset-install-capture-20261011.txt` SHA-256 `1eaa418cea43121058decd0cc79080c9f9cef01f77696e43d91c4f20bdc0949b`.

## 2026-10-11 — Trace the imported interface misclassification

- Step/experiment: `wp7-install-classref-interface-trace`
- Change: Added watchdog stages through `resolveClassref()` and `classref2class()` and packed the source external package/class tokens plus resolved package ID and interface flag into the recovery context.
- Validation: ARM/RISC-V wrapper builds passed. The latest diagnostic UF2 flashed with `FLASH:PASS`; the 8-second serial monitor reported repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. PC/SC reader discovery succeeded after stopping an orphaned `pcsc_scan -r` process. GPPro authenticated and loaded every CAP block with `9000`, then final INSTALL `84E60C00` reset the card without a response. Reopened CDC captured `opcode=8F count=1 stage=26 context=00820301`. Context decodes to resolved package ID 0, source package token `0x82` (external bit set, import index 2), class token 3, and `is_interface=1`. No INSTALL `9000` or GET STATUS/SELECT confirmation was received.
- Result: The VM is interpreting a Java Card interface reference (framework export token 3) as a superclass during the first `new`. This is downstream of successful export-offset endian conversion. The exact initial object class/package being sized is not yet preserved in the watchdog context.
- Decision: Add a separate watchdog origin field containing the allocated class package/index, then trace the `new` operand and instance-field walk to identify why an interface reaches `classref2class()`.
- Commit: `1731f4f3da58751d7ffbb2fe8bfecdfcf260c6c1`
- Artifacts/logs: UF2 `build/arm/pico2_choupi.uf2` SHA-256 `9d07425d968219ca21cded897617091fe716d11896f4dc63d24f428c001dafd9`; runtime `artifacts/logs/cdc-classref-raw-context-20261011.txt` SHA-256 `c52635d6721f2745dee73246f41eff9a4226df965cacf514e37fee51d105c00b`; GP trace `artifacts/logs/gp-install-import-context-20261011.txt` SHA-256 `6fd7a6df24fbdb094d4fd3e7d71308683e4351a93afb48677706bc7c4da5ec26`; recovery `artifacts/logs/cdc-import-context-20261011.txt` SHA-256 `87ec93eb174d07434af7bd3b82524303e31082543516c96fc546f165158725e4`.

## 2026-10-11 — Translate JC 3.0.5 imported class tokens

- Step/experiment: `wp7-jc305-imported-class-token-remap`
- Change: Added AID-based JC305-to-CHOUPI embedded class token mappings for `java/lang`, `javacard/framework`, `javacard/security`, and `javacardx/crypto`; applied them during external class resolution and imported static constructor lookup. Retained watchdog origin/context diagnostics to identify class-reference failures.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real-board ARM image flashed with `FLASH:PASS`; the finite serial monitor reported `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. PC/SC discovery succeeded. GPPro loaded all 131,538 CAP bytes, with every LOAD block returning `9000`. First install execution reached `IndexOutOfBoundsException` at `invokespecial` PC `0x101C4E1B`; mapping the external constructor class advanced execution to `NotYetImplemented` at `impdep1` PC `0x10047BF8`. GP INSTALL returned `6A80`; applet installation is not confirmed. The 8-second monitor command initially hit USB re-enumeration delay, then a repeated finite capture passed.
- Result: Imported class ABI mismatch is corrected far enough for the CAP's install method and constructor/native calls to execute. Remaining failure is inside a native call; the consumed native index has not yet been captured.
- Decision: Capture the `impdep1` native method index, implement the exact required operation, and retry installation.
- Commit: `a71356d`
- Artifacts/logs: `artifacts/logs/gp-install-jc305-token-remap-20261011.txt` (SHA-256 `6e4420cb27c2daf62c8f22cd08568ad71606fc3698ef62c0f132f63970eb6dcd`); `artifacts/logs/gp-install-jc305-method-token-remap-20261011.txt` (SHA-256 `195b05ec5d805c9f7bd34591311b4e13d5a892123e3a02e791261a1c4705802e`); flashed UF2 SHA-256 `877d322f5ebc4f03f8f3cee7112b11a202c82400cb4ba359ffc55796bb592022`.

## 2026-10-11 — Add acceptance-criteria TODO tracker

- Step/experiment: `project-todo-tracker`
- Change: Added root `TODO.md` to track remaining plan work against the stated
  acceptance criteria, and updated `.agent/HANDOFF.md` with current offline-only
  scope and the preserved in-progress JC305 compatibility changes.
- Validation: Compared tracker items with `plan.md`, existing progress records,
  and the current worktree; confirmed it distinguishes CAP persistence and
  registry/SELECT responses from actual applet installation and execution.
  Builds and hardware checks were intentionally skipped because this is a
  documentation-only step and the user requested offline implementation.
- Result: Remaining installation, applet lifecycle, APDU, host integration,
  simulator comparison, and final acceptance work are now visible in one root
  checklist.
- Decision: Continue with offline diagnosis of the JC305 install-method path.
- Commit: `a21f7af8af70284240c49a1f030d4662620ef682`
- Artifacts/logs: `TODO.md`, `.agent/HANDOFF.md`

## 2026-10-11 — Resolve JC305 method references and seed INSTALL arguments

- Step/experiment: `wp7-jc305-method-abi-and-install-args`
- Change: Added AID-based static and virtual method-token translation for the
  imported JC305 `JCSystem`, `Object`, and `Applet` references, preserving the
  Applet access-flag behavior required by CHOUPI's exported method tables.
  Captured the native-method index consumed by `impdep1`. Separately parsed GP
  INSTALL parameters, passed their bytes into the install entry, allocated a
  Java byte array, and seeded `install(byte[], short, byte)` locals. Setup
  exceptions now return a status instead of escaping the `noexcept` entry.
- Validation: `./tools/build arm` and `./tools/build riscv` passed with no
  compiler warnings or errors. The prior GPPro trace shows the supplied request
  has install parameters `C9 00`; no board or PC/SC checks were run because the
  user requested offline implementation.
- Result: Both target variants compile with JC305 method-token translations,
  native-call diagnostics, and the GP parameter field wired into Java install.
  Runtime method resolution and successful applet registration remain
  unverified.
- Decision: Continue offline verification of JC305 tokens against API exports;
  perform the install runtime check when the Pico is available again.
- Commit: `ef90fea8677801c93c49db5c973afcb7eb360e13`
- Artifacts/logs: `artifacts/logs/gp-install-jc305-native-index-20261011.txt`

## 2026-10-11 — Require Applet.register during installation

- Step/experiment: `wp7-install-register-call-tracking`
- Change: Added a per-context marker for calls to the JC305 imported Applet
  `register()` overloads. The install bridge returns the marker to the GP
  loader, which now refuses to persist an instance AID when Java install
  returns without invoking registration.
- Validation: `./tools/build arm` and `./tools/build riscv` passed with no
  compiler errors or warnings. Hardware and PC/SC validation were intentionally
  skipped while continuing offline implementation.
- Result: A successful interpreter return alone can no longer create a
  selectable GP registry entry; a recognized Applet registration method call
  is also required. The JC305 method mapping and actual instance persistence are
  not yet runtime-verified.
- Decision: Continue implementing the registered applet instance and lifecycle
  bridge, then verify it on the board when physical testing resumes.
- Commit: `36473c12e145b1b5f5e53c8fe53286f784c9f204`
- Artifacts/logs: none (offline wrapper builds only)

## 2026-10-11 — Bring up the PC build and retry the hardware cycle

- Step/experiment: `wp4-pc-build-bootstrap-and-physical-retry`
- Change: Added `tools/build-pc`, removed the PC executable's unused Boost
  parser dependency, updated the Rust OS build commands for current Cargo, and
  configured the Java Card API build to run under the bundled Java 8 compiler
  while leaving Maven on the system JDK. Updated
  the handoff and TODO with the renewed physical-testing scope and current host
  limitations.
- Validation: `./tools/build arm`, `./tools/build riscv`, Java Card API CAP
  conversion, ROM-mask generation, and compilation of `src/main_pc.cpp` passed.
  `./tools/build-pc` configured and generated the ROM mask, then failed because
  Cargo could not resolve `ipc-channel` from crates.io in this network-isolated
  environment. `./tools/cycle choupi` rebuilt both architectures, but flashing
  stopped before writing because `picotool` reported `Failed to initialise
  libUSB`; `/dev/bus/usb` is absent and `lsusb` reports the same libusb error.
- Result: PC setup no longer requires Boost and JavaCard ROM artifacts can be
  generated with the available Java 8 compiler. No hardware flash occurred.
  Full PC linking and runtime validation remain blocked by the missing Cargo
  dependency; hardware validation remains blocked by USB isolation.
- Decision: Continue implementation from the plan. Retry PC build when Cargo
  dependencies are available and retry board flashing when this environment can
  access the VM's USB device.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T191905Z.log` (SHA-256
  `3f6e644789143bebc2394f7b756f452d417d52bc7d23132a9428f042297f5cb3`); ARM
  UF2 `build/arm/pico2_choupi.uf2` (SHA-256
  `8dd273698c99943ff37462962b76a57197e44b2671436521e39a57ded4bdd38e`)

## 2026-10-11 — Persist the applet object on registration

- Step/experiment: `wp7-register-persistent-applet-object`
- Change: The VM now records the object reference received by either imported
  JC305 `Applet.register` overload. After the install method returns `9000`, the
  bridge serializes the instance and its persistent fields through the existing
  flash object serializer under reserved applet field tag `0xFF`. A persistence
  failure now makes GP INSTALL fail instead of reporting registration success.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed without
  warnings or errors. Hardware and PC runtime checks were skipped: the
  container cannot access `/dev/bus/usb`, and the PC build still lacks the
  uncached Cargo `ipc-channel` dependency.
- Result: Both architecture variants compile the object-reference handoff and
  persistent serializer path. The implementation currently reserves applet
  owner ID 0 and does not roll back the object if the later card-manager AID
  write fails; neither the filesystem permission path nor reboot reload has
  been exercised.
- Decision: Next add a persistent instance index and cleanup/reload semantics,
  then connect the object to SELECT and APDU lifecycle dispatch.
- Commit: pending
- Artifacts/logs: ARM UF2 `build/arm/pico2_choupi.uf2` (SHA-256
  `003a2a2d6fc7c936740dab4199c99bb01987c3c1d7ece7322006c2badb720cc6`); no
  serial log because flashing was blocked before device access.

## 2026-10-11 — Persist registered applet class identity

- Step/experiment: `wp7-persist-applet-class-index`
- Change: Passed the registered object's class index from the VM install bridge
  into the CAP slot metadata. Header version 2 stores that index with an
  additional CRC; version-1 headers remain readable. The install writer rejects
  the `0xFFFF` invalid-index sentinel. Updated TODO and handoff to distinguish
  saved class identity from the still-missing reboot reconstruction/dispatch.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Two
  `./tools/cycle choupi` attempts passed both build gates and flashed the ARM
  CHOUPI image, but each bounded cycle timed out looking for serial within the
  monitor's current 10-second enumeration window. A finite `./tools/monitor
  --seconds 8` then passed on the same board with repeated
  `TEST:SUMMARY pass=1 fail=0`. The board was detected as
  `2e8a:0009 Raspberry Pi Pico 2 CHOUPI CCID`. No JCAlgTest installation was
  attempted in this step.
- Result: The registered class index is now persisted alongside the AID and
  object state, but runtime class lookup/reconstruction is not implemented or
  verified. The flash succeeded; the runtime self-check passed. The full cycle
  command remains flaky because serial enumeration takes longer than its
  current wait.
- Decision: Keep the physical board in service. Extend serial-enumeration
  tolerance, then resume applet reconstruction and GP INSTALL/SELECT/APDU tests.
- Commit: `78a0c37`
- Artifacts/logs: `artifacts/logs/monitor-choupi-class-index-20261011.txt`
  (SHA-256 `999f4652dea75934aa24ee1fa587049b982e30a2c63a8bc26701ca63d3b0a8ab`);
  failed full-cycle captures `artifacts/logs/cycle-choupi-20261011T194021Z.log`
  (SHA-256 `ce1d0a9ed3b293223a0b8814942fec0bc12a59b82cb86b622947a6f8cb5d0a85`)
  and `artifacts/logs/cycle-choupi-20261011T194129Z.log` (SHA-256
  `72da27a28c1bdc2e0e3a3b446e7efaf6fb4e277182d229d80b6cdfc2ebcb37a4`); ARM
  UF2 `build/arm/pico2_choupi.uf2` (SHA-256
  `afd2194987c9f06476e7e7098124e09b7ceadab12d1b22862f1f68268f64e4c4`).

## 2026-10-11 — Make the physical cycle tolerate USB CDC delay

- Step/experiment: `wp4-robust-post-flash-serial-capture`
- Change: Increased the bounded USB serial enumeration wait in `tools/monitor`
  from 10 to 30 seconds. Updated `tools/cycle` so CHOUPI runtime health is
  determined by the repeated `TEST:PASS`, `TEST:SUMMARY`, and fault markers;
  the one-time BOOT banner is no longer required because CDC may attach after
  it has already been printed.
- Validation: `./tools/cycle choupi` passed end to end. ARM and RISC-V
  wrappers passed, picotool recovered from a missing reset interface using
  CDC 1200-baud BOOTSEL, flashed the CHOUPI UF2, and the monitor attached after
  the delayed CDC enumeration. The 18-second capture had repeated
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`; no failure,
  fault, or timeout occurred. The preceding run captured the same passing
  runtime output but exited nonzero solely because the old BOOT-banner check
  was not satisfied. A read-only GPPro `--list` probe currently returns
  `SCARD_E_NO_SERVICE`; no card operation was attempted.
- Result: The complete hardware cycle now survives the observed device reset
  and delayed CDC enumeration behavior and reports pass on actual runtime
  output. GP INSTALL remains pending until the host PC/SC daemon is available.
- Decision: Keep the device in runtime mode, restore PC/SC access without
  violating the no-sudo cycle constraint, then resume GPPro applet lifecycle
  verification.
- Commit: `dde096e`
- Artifacts/logs: passing cycle
  `artifacts/logs/cycle-choupi-20261011T194451Z.log` (SHA-256
  `8e2efc766e495bffebfcc15722c8e5bfeaaab727ecdb91033df346d3c13c53b5`);
  preceding monitor-check cycle
  `artifacts/logs/cycle-choupi-20261011T194325Z.log` (SHA-256
  `9364f8279d25adbffd9db1582312d2b6747451cae294e825767c11a6c10e7c0f`);
  UF2 SHA-256 `afd2194987c9f06476e7e7098124e09b7ceadab12d1b22862f1f68268f64e4c4`.

## 2026-10-11 — Add persistent applet root reconstruction helper

- Step/experiment: `wp7-restore-persistent-applet-root`
- Change: Added a `Context` helper that resolves the reserved persistent
  applet-instance tag, decodes it through CHOUPI's existing persistent-object
  reader, verifies package and class identity, and eagerly loads its fields.
  It records the reconstructed object reference for subsequent context use.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real
  `./tools/cycle choupi` passed both build gates, recovered BOOTSEL through
  CDC, flashed the firmware, and captured repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` during its
  18-second monitor window. This cycle did not invoke the new restoration
  helper. GPPro read-only listing is blocked by the host PC/SC driver state:
  pcscd is running, but its `pcscd` account lacks access to the current USB
  node, so no reader is enumerated. No privileged helper was invoked.
- Result: The VM now has a bounded, validated way to reconstruct an applet
  object graph when given its stored class index. Runtime SELECT, retaining the
  object for applet lifecycle calls, and physical restoration/install checks
  remain incomplete.
- Decision: Wire this helper into a per-applet CCID context and SELECT path;
  restore PC/SC reader access through the user-authorized setup path when
  allowed by the active repository safety constraints.
- Commit: `845e86f`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T195102Z.log` (SHA-256
  `420c05f8ea0ae1ae7689f31aa57a8e87ec4d611ca59a9cf5a00e20592d956595`); ARM
  UF2 SHA-256 `48c6a1a4871365ba6964e9b141c43eefda53abf61e2aa97507adfe66da17b9ac`.

## 2026-10-11 — Restore the applet root during AID SELECT

- Step/experiment: `wp7-restore-applet-on-select`
- Change: Added a C bridge that creates a fresh applet `Context`, restores the
  persistent root by package/class index, verifies the decoded field graph,
  and returns a failure status on inconsistency. The CCID AID SELECT path now
  invokes this bridge when version-2 CAP metadata contains the class index;
  older version-1 headers retain their previous FCI behavior. Failed restore
  returns `6F00`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real
  `./tools/cycle choupi` passed both builds, flash, and 18-second serial
  capture with repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. The cycle does not send AID SELECT, so the new
  restore-on-SELECT branch was not exercised. GlobalPlatformPro currently sees
  no available readers: pcscd is active, but its service account has no access
  to the Pico USB node. No applet APDU was sent.
- Result: Version-2 applet AID SELECT now performs object reconstruction and
  integrity checks before returning FCI. It still discards the temporary
  context/reference and does not invoke `select()` or `process()`.
- Decision: Retain the restored context across APDUs and implement Java Card
  lifecycle/APDU dispatch; retry GPPro install and SELECT once PC/SC can see
  the reader.
- Commit: `6370685`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T195521Z.log` (SHA-256
  `06de173c6f70be838090a032ccf11ad92c43e2a89e1225d631087b161c1b79a0`); ARM
  UF2 SHA-256 `2211ce2bcab208aa8abfaa884ec71853f685a14934c508d2721a4d07691a7f53`.

## 2026-10-11 — Retain the selected applet context

- Step/experiment: `wp7-retain-selected-applet-context`
- Change: Successful v2 restore now stores its reconstructed `Context` in a
  session-owned firmware pointer instead of destroying its heap after FCI
  creation. Any SELECT APDU clears the previous context before routing the new
  AID, and a failed restore clears the candidate context.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real
  `./tools/cycle choupi` passed both builds, recovered BOOTSEL through CDC,
  flashed the ARM image, and captured repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` over 18 seconds.
  No applet SELECT APDU was sent, so context retention was build- and
  boot-validated only. PC/SC still enumerates no reader because the daemon
  account cannot open the current Pico node.
- Result: A successfully reconstructed applet heap can now live across the
  CCID selection boundary and is cleared on AID changes. There is still no
  interpreter entry for applet `select()`, `process()`, or `deselect()`.
- Decision: Add a per-APDU interpreter entry that consumes the retained
  receiver/context, then validate it with the GPPro path when the reader is
  available.
- Commit: `d1fc0a7`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T195927Z.log` (SHA-256
  `dee9effae6d9a883e0837a526c74e693ef27c849814bb41ba69e85ea9f75b76e`); ARM
  UF2 SHA-256 `a5f9b3a301d1919a4f5ba15ffc2df54183d3a0f46dee092fdcc9bd0c286d5896`.

## 2026-10-12 — Add borrowed-context virtual method entry

- Step/experiment: `wp7-borrowed-context-virtual-entry`
- Change: Added an interpreter constructor that borrows a retained `Context`,
  starts a virtual method at a Method-component offset, and seeds checked
  receiver/reference locals. Added a firmware bridge that invokes the entry on
  the selected applet context. The borrowed context is not copied because its
  stack frames refer to storage owned by that context.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The first
  sandboxed `./tools/cycle choupi` stopped at picotool/libusb initialization
  (`status=250`); retry with USB access passed both wrapper build gates, CDC
  BOOTSEL recovery and flash, then captured 18 seconds of board output. The
  board repeatedly reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`, with no `TEST:FAIL` or `FAULT`. No Java applet
  lifecycle method or SELECT APDU was invoked; this is not lifecycle validation.
- Result: The retained applet context can now be supplied to a virtual-method
  interpreter entry with explicit initial reference locals. Method resolution,
  APDU object/buffer initialization, and lifecycle dispatch remain incomplete.
- Decision: Resolve the applet's exported lifecycle methods from its CAP and
  wire SELECT/process dispatch with the APDU state needed by those methods.
- Commit: `24febcf`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T033945Z.log` (SHA-256
  `c6a964c2ddeef80cf2e7d790633feff3aafd1cf623bdbcf68cf5f9509c090ec7`); ARM
  UF2 SHA-256
  `e588e7c926aaf022e632c052a1a18129f6e9d6b572c4a12c65483cbd036073e8`.

## 2026-10-12 — Implement byte-array search native

- Step/experiment: `wp5-array-find-byte-native`
- Change: Implemented the shipped JC 3.0.5 `arrayFindGeneric` native for byte
  arrays. It validates null references, offsets, and array component types,
  then returns the first matching absolute index at or after `offset`, or
  `-1`. The API's other primitive-array variants are not implemented.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The
  `./tools/cycle choupi` wrapper passed both build gates, recovered BOOTSEL
  through CDC 1200-baud reset, flashed the ARM image, and captured 18 seconds
  of serial output with repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. The native was not invoked by the generic
  runtime smoke check; no GP INSTALL APDU was sent.
- Result: Byte-oriented native array search no longer throws
  `NotYetImplemented`; non-byte primitive arrays remain unsupported.
- Decision: Inspect the JCAlgTest 1.8.2 constructor/install call sequence and
  continue implementing only the native/API behavior that blocks real install
  and APDU lifecycle execution.
- Commit: `0223ba7`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T034427Z.log` (SHA-256
  `4a3ba30db076432c0a0da7cb19f23b6726b248f57682defbc4fd278bf720bfa9`); ARM
  UF2 SHA-256
  `8f3099f3779eddd46e8e84fa3ff20415ac2aad13b4709cc01c9d9c233155a1dd`.

## 2026-10-12 — Recognize JC305 applet registration tokens

- Step/experiment: `wp7-jc305-applet-register-token-detection`
- Change: Corrected the install-time `Applet.register()` observer to recognize
  imported JC305 virtual method tokens 1 and 2, matching the v1.8.2 CAP
  ConstantPool entries and JC305u3 `framework.exp` (`register()` and
  `register(byte[], short, byte)`).
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real
  Pico cycle passed both build gates, recovered BOOTSEL with CDC 1200-baud
  reset after picotool could not locate the reset interface, flashed the ARM
  image to 100%, and captured repeated `TEST:PASS choupi_runtime` /
  `TEST:SUMMARY pass=1 fail=0` with no `TEST:FAIL` or `FAULT`. A bounded
  elevated `pcsc_scan -r` timed out with exit 124 and no reader output, so the
  GP INSTALL/APDU path could not be retried. No warnings were reported by the
  wrapper builds.
- Result: The observer now matches the actual JC305 register virtual tokens;
  this does not establish that virtual method lookup succeeds or that
  registration ran. The last captured INSTALL diagnostic is still a
  `SecurityException` at `invokevirtual` PC `0x101C4EF4`, CP operand `0x0179`
  (`Applet.register()`); that dispatch failure remains unresolved.
- Decision: Restore PC/SC reader availability and retry INSTALL. If the same
  exception remains, instrument external class/method resolution around this
  exact Applet register reference and inspect the returned method-table slot.
- Commit: `2c2728e`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T040413Z.log` (SHA-256
  `db0c06428e3c0061346ed543fea5e3c46c7aac03808432cc00170b152d067233`); ARM
  UF2 SHA-256
  `3236a6a98ba47b92ad25afa7d22ce308b37630e5a4437ce01e92f17613fc2d1f`.

## 2026-10-12 — Return INSTALL resolver diagnostics

- Step/experiment: `wp7-install-resolver-stage-response`
- Change: INSTALL failure responses now include the last class-resolution stage
  and packed context. Added checkpoints after imported class resolution and
  before/after public method-table lookup. The stage/context is reset at each
  install attempt and returned with the existing proprietary diagnostic bytes;
  the final two bytes remain `6A80` on failure.
- Validation: `./tools/build arm` and `./tools/build riscv` passed without
  warnings. The real Pico cycle passed both build gates, entered BOOTSEL via
  CDC reset after picotool missed its reset interface, flashed the ARM image to
  100%, and passed the 18-second finite runtime capture with repeated
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. PC/SC was
  unavailable, so the new failure fields and GP INSTALL behavior were not
  exercised.
- Result: The next INSTALL response can distinguish a class resolution failure
  from an invalid public method-table slot. Current installed firmware contains
  this diagnostic build; it does not establish applet installation.
- Decision: Restore PC/SC reader access and retry the exact JCAlgTest v1.8.2
  INSTALL. Use the returned stage/context to correct the Applet register
  dispatch path.
- Commit: `81ca2a5`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T041446Z.log` (SHA-256
  `e198912f9c31a8bead2e7b12c60797881a7e1ba63b9aea4918092b1952d367fd`); ARM
  UF2 SHA-256
  `902c5c1c6453d4f3b9ceedaa00cb2c5cb1067adda60b73c81dac5aae38a3196a`.

## 2026-10-12 — Capture virtual-method flag resolution

- Step/experiment: `wp7-install-virtual-method-flag-stage`
- Change: Added stage 30 immediately before `getPublicMethodOffset` checks
  whether the imported virtual-method token is public. Its context contains
  the raw class reference and method token, so the next INSTALL failure can
  show whether `Applet.register()` is rejected before class/method resolution.
- Validation: `./tools/build arm` and `./tools/build riscv` passed without
  warnings. `./tools/cycle choupi` passed with elevated host USB access: CDC
  reset entered BOOTSEL, the ARM image flashed to 100%, and the finite serial
  capture repeatedly reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. A normal-context cycle could not initialize
  libUSB. A bounded PC/SC restart/scan did not find a reader: `pcscd` could not
  bind `/run/pcscd/pcscd.comm` (`Permission denied` / `Address already in use`),
  and `pcsc_scan -r` reported `No reader found`.
- Result: Firmware with stage 30 is on the Pico 2 and its runtime self-check
  passes. The new marker and the JCAlgTest INSTALL path remain unexercised
  because the PC/SC service is unavailable; no applet installation is claimed.
- Decision: Restore the system PC/SC service/reader, then retry the exact
  JCAlgTest v1.8.2 INSTALL and decode response bytes 24-28 (stage/context).
- Commit: `1f41a70`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T055405Z.log` (SHA-256
  `c497422821ef84fca6313e87e6719653b944fa0b0dd7e9d63adc24b9785265bb`); ARM
  UF2 SHA-256 `7e82e6de6ac4211cdc431cf4f301220042a033e8ba980554e02963a8121be352`.

## 2026-10-12 — Decode CAP method-table offsets

- Step/experiment: `wp7-cap-method-table-endianness`
- Change: Convert public and package virtual-method table entries from CAP
  big-endian byte order with `NTOHS` before using the method-component offset.
- Validation: `./tools/build arm` and `./tools/build riscv` passed without
  warnings. The real Pico cycle passed BOOTSEL recovery, ARM flashing to 100%,
  and the finite serial check with repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. Before this correction, GPPro v1.8.2 INSTALL
  returned failure payload stage `0x1D` (29), context `0x00003302`; the table
  bytes decode to offset `0x0233` on this little-endian target. After reflash,
  the GPPro retry listed no readers, so it sent no INSTALL APDU; USB
  re-enumeration changed the node after its ACL was granted.
- Result: The byte-order defect is corrected and the firmware passes its board
  runtime check. Applet installation after the correction is not yet verified.
- Decision: Reapply the existing reader ACL helper after USB re-enumeration,
  confirm `pcsc_scan -r` lists the reader, then retry the exact GPPro INSTALL.
- Commit: `f8643e2`
- Artifacts/logs: `artifacts/logs/gp-install-stage30-20261012.txt` (SHA-256
  `a3fc895e031e90c70203933ef48467a5d6d82112651bc3a0fc9763f61c43c197`);
  `artifacts/logs/gp-install-method-table-endian-20261012.txt` (SHA-256
  `1a29a6772360ac5ba8a5a4d9e38b0f78cd6bfea4167b09d9c36d0618a17823ab`);
  `artifacts/logs/cycle-choupi-20261012T061610Z.log` (SHA-256
  `e0e35fcf78ea1567d58267929434ba922f1738974ec2bbae1cd5584b6b463c98`);
  ARM UF2 SHA-256
  `811504dc6897acca45cbc8b7ce4b6bcc3b847c1f55cc100e8304f978a7ce306e`.

## 2026-10-12 — Resolve virtual calls through receiver class

- Step/experiment: `wp7-virtual-dispatch-receiver-class`
- Change: Resolve `invokevirtual` against the receiver instance's concrete
  package and class after normalizing imported JC method tokens. Read the
  declared method argument count before dispatch so the receiver can be found
  safely on the operand stack; reject null receivers and insufficient operands
  before changing frames.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real
  `./tools/cycle choupi` passed both build gates, reset through CDC into
  BOOTSEL, flashed the ARM image to 100%, and captured repeated
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0` over 18 seconds.
  `git -c core.whitespace=cr-at-eol diff --check` passed. The runtime test does
  not execute the new virtual override resolution because no applet has been
  installed and PC/SC still reports no reader.
- Result: Subclass overrides can now be selected from the actual receiver's
  CAP virtual-method table; applet lifecycle entry points are still not wired
  to CCID APDU dispatch, and this path remains unverified on a Java applet.
- Decision: Continue implementing the SELECT/process bridge and Java Card APDU
  object/buffer behavior; retest this method-resolution path immediately after
  PC/SC reader access is restored.
- Commit: `fa54b76` (the progress reference is committed in the follow-up record)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T072742Z.log` (SHA-256
  `f8bdd6a2cb5d6326e1bdfe0343ea89157b122b07647e68d690b102295e28f088`); ARM
  UF2 SHA-256 `7a34c0f0941dbc0818c9fa7984a1b6c63d58ee50cb89c73d4c7984b50a4ed04e`.

## 2026-10-12 — Capture top-level Java method returns

- Step/experiment: `wp7-root-method-return-capture`
- Change: Record reference, short, and int return values when a Java method
  returns from the root interpreter frame. Nested method returns still push
  their values onto the caller frame. Reset the captured value when a new root
  frame starts. This lets the borrowed-context applet bridge consume the
  boolean result from `Applet.select()` without inventing a Java caller frame.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The real
  `./tools/cycle choupi` passed both build gates, recovered BOOTSEL via CDC
  reset, flashed the ARM image to 100%, and captured repeated
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0` over 18 seconds.
  `git -c core.whitespace=cr-at-eol diff --check` passed. The runtime check
  does not exercise a Java top-level return, so this behavior is not yet
  directly validated.
- Result: Root interpreter calls can expose their return value while preserving
  nested return semantics; applet lifecycle calls remain unwired and PC/SC has
  no reader.
- Decision: Add the lifecycle method resolver/bridge and consume the captured
  `select()` result, then validate it with the v1.8.2 applet once the reader is
  restored.
- Commit: `0061646` (the progress reference is committed in the follow-up record)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T073435Z.log` (SHA-256
  `22437f072e2f01ca1480d9d05ac84fa695f2e732af29dff12731ded061a93acc`); ARM
  UF2 SHA-256 `7c180dd9eba1af4e40172c2a9df655b99de7a45f621e424986a3c9a75324bc57`.

## 2026-10-12 — Dispatch applet SELECT and deselect lifecycle

- Step/experiment: `wp7-applet-select-deselect-dispatch`
- Change: Resolve lifecycle method offsets from the restored instance's concrete
  class. Invoke `Applet.select()` before returning FCI and return `6999` if it
  declines selection. Invoke `Applet.deselect()` before another SELECT and on
  CCID power-off, then clear the selected context. Pass the resolved method
  package into the borrowed-context interpreter entry.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The physical
  `./tools/cycle choupi` passed both build gates, CDC BOOTSEL recovery, ARM flash
  to 100%, and repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1
  fail=0` markers during its 18-second capture. Elevated USB inspection found
  the Pico at `001:011`; `pcsc_scan -r` reported no reader, so no applet SELECT
  or deselect APDU was sent. `git -c core.whitespace=cr-at-eol diff --check`
  passed.
- Result: SELECT/deselect lifecycle entry points are now connected to the
  interpreter, but runtime behavior on an installed applet remains unverified.
  `process(APDU)` and APDU buffer/response handling remain incomplete.
- Decision: Add APDU buffer delivery and process-method dispatch next; retry
  hardware installation and lifecycle verification after reader access returns.
- Commit: `256986e` (the progress reference is committed in the follow-up record)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261012T073952Z.log` (SHA-256
  `58fc542a20f41bb715ede6e522ef7acdddd44b1c7751c7909c612bac14090983`); ARM
  UF2 SHA-256 `6c599c684698e868d80664585d38eaf0a26252acf2579907516d6d8b1ffbc07b`.

## 2026-10-12 — Recheck PC build and reader access

- Step/experiment: `wp7-host-build-and-reader-recheck`
- Change: Updated the project tracker and handoff with current PC build and
  reader-access evidence so the next session can resume from the actual state.
- Validation: `./tools/build-pc` completed Java Card API compilation and CAP
  conversion with zero converter errors, then failed before CHOUPI C++ linking
  because Cargo could not resolve `index.crates.io` to fetch `ipc-channel`
  0.13.0. Elevated USB inspection found Pico `2e8a:0009` at `001:003`; the
  PC/SC service is active, but `pcsc_scan -r` reports no reader. The node ACL
  currently grants `claude` access and omits `pcscd`.
- Result: The endianness-fix firmware remains on the physical board, but no
  post-fix GP INSTALL APDU has been sent. The PC build's network dependency and
  the refreshed `pcscd` ACL are the current external blockers.
- Decision: Resume exact v1.8.2 GP INSTALL once the installed ACL helper has
  been run for the current USB node; continue ARM/RISC-V validated work while
  crates.io is unavailable.
- Commit: `fbcb35f` (this record is completed by the follow-up metadata commit)
- Artifacts/logs: `/tmp/pico2-build-pc.log` (not archived in the repository);
  prior firmware runtime log `artifacts/logs/cycle-choupi-20261012T061610Z.log`.

## 2026-10-13 — Review current applet and APDU integration

- Step/experiment: `code-review-20261013`
- Change: Recorded eight concrete implementation defects in the current lifecycle, APDU, generated JNI, and virtual-resolution paths; implementation source was not edited.
- Validation: Static inspection of source, generated headers, and supplied v1.8.2 CAP; host-only `./tools/build arm` failed at link time with missing `setAPDUOutgoing(std::shared_ptr<JC_Array>, short, short)`.
- Result: Current firmware cannot link; further defects would prevent correct selection and repeated APDU exchange after that link issue is fixed. No token access, flash, runtime measurement, or hardware validation was performed.
- Decision: Resolve the review findings before resuming applet integration. The project implementation goal remains paused; this was a separately requested review.
- Commit: `recorded in the following review commit reference`
- Artifacts/logs: `artifacts/reviews/code-review-20261013.md`, `artifacts/logs/review-arm-build-20261013.log`

## 2026-10-13 — Review commit reference

- Step/experiment: `code-review-20261013-reference`
- Change: Recorded the immutable review commit reference.
- Validation: Review report and host-only build log were committed successfully.
- Result: Review commit is `d8afcd4`; implementation files retain their existing worktree state.
- Decision: Review complete; implementation goal remains paused.
- Commit: `d8afcd4`
- Artifacts/logs: `artifacts/reviews/code-review-20261013.md`, `artifacts/logs/review-arm-build-20261013.log`

## 2026-10-14 — Prepare implementation fix checklist

- Step/experiment: `review-fix-checklist`
- Change: Added root FIXES.md with eight prioritized findings, affected files, proposed correction requirements, validation cases, and remaining project scope for another agent.
- Validation: Checked the checklist against the committed 2026-10-13 review and its host-only link-failure evidence. No builds, tests, token operations, or implementation edits were performed.
- Result: Documentation handoff is available; every proposed fix remains unchecked.
- Decision: Other agent should revalidate the worktree and implement the listed corrections in the documented order.
- Commit: `recorded in the following documentation commit reference`
- Artifacts/logs: `FIXES.md`, `artifacts/reviews/code-review-20261013.md`

## 2026-10-14 — Fix checklist commit reference

- Step/experiment: `review-fix-checklist-reference`
- Change: Recorded the fix-checklist documentation commit.
- Validation: Commit contains only FIXES.md and its progress record.
- Result: Checklist commit is `df4edca`; proposed fixes remain unimplemented.
- Decision: Hand off implementation to the other agent as requested.
- Commit: `df4edca`
- Artifacts/logs: `FIXES.md`

## 2026-10-14 — Implement reviewed APDU and lifecycle fixes

- Step/experiment: `implement-review-fixes-20261014`
- Change: Corrected the generated JNI ABI and registered the Java-visible APDU array so subsequent commands refresh and use the same object. The Java API changes are committed in its nested checkout as `534c1b1`. The build now regenerates the ROM image and JNI header together, fingerprints API sources, and rejects stale/mismatched artifacts. Corrected JCAlgTest lifecycle tokens, framework APDU class resolution and endianness, public/package superclass table traversal, failed SELECT cleanup, and APDU status-byte response handling. Documented the implementation state in `FIXES.md`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed after the changes. `./tools/cycle choupi` rebuilt both targets, detected the RP2350 in sysfs, then stopped before flashing because `picotool` reported `Failed to initialise libUSB`; no firmware write or serial capture occurred. `./tools/doctor` reports its host environment variables/tools missing because it uses the ambient PATH, while repository build wrappers pass. `git -c core.whitespace=cr-at-eol diff --check` passed before the final include-only cleanup; both builds are repeated after it.
- Result: ARM CHOUPI firmware links with the regenerated native API. No physical runtime behavior was validated. USB access from this execution context remains blocked despite sysfs showing device `2e8a:0009`.
- Decision: Source fixes are complete; retry the bounded CHOUPI flash/runtime capture when libUSB access works, then exercise SELECT and repeated APDU commands on an installed applet.
- Commit: `55c26ce` (implementation; this reference is recorded in the follow-up entry)
- Artifacts/logs: `artifacts/logs/fixes-build-arm-20261014.log`, `artifacts/logs/fixes-build-riscv-20261014.log`, `artifacts/logs/fixes-cycle-choupi-20261014.log`; ROM image SHA-256 `7683c4a90d57702cf313e1048a5f0e7478f18f2a69ae99a4d078205aed57d9d9`, JNI header SHA-256 `dca8995ec8a8fadf16ce46e99cc869bf6e0748ad5dc90f7c841dd38701825586`, ARM CHOUPI UF2 SHA-256 `6bf2fbf36f860dd6952c02a1ea91c90a54c661ece40e5796a4310f00ef797844`.


## 2026-10-14 — Implementation commit reference

- Step/experiment: `implement-review-fixes-20261014-reference`
- Change: Recorded the implementation commit SHA after committing the fixes and validation artifacts.
- Validation: Confirmed the root implementation commit and nested Java Card API commit are present; unrelated pre-existing worktree changes remain unstaged.
- Result: Root implementation commit is `55c26ce`; nested Java API buffer commit is `534c1b1`.
- Decision: Resume with physical CHOUPI flashing and applet APDU validation when USB/libUSB access is available.
- Commit: `55c26ce`
- Artifacts/logs: `FIXES.md`, `artifacts/logs/fixes-build-arm-20261014.log`, `artifacts/logs/fixes-build-riscv-20261014.log`, `artifacts/logs/fixes-cycle-choupi-20261014.log`

## 2026-10-14 — Flash CHOUPI and verify runtime on Pico 2

- Step/experiment: `physical-cycle-choupi-20261014`
- Change: Flashed the current ARM CHOUPI UF2 to the attached RP2350 while it was in BOOTSEL and captured the bounded serial runtime log.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` passed both build gates, confirmed BOOTSEL, wrote the image to 100%, rebooted the card, and captured 18 seconds of serial output. Runtime reported repeated `CHOUPI:STATUS sw=9000`, `TEST:PASS choupi_runtime`, and `TEST:SUMMARY pass=1 fail=0`; no `TEST:FAIL` or `FAULT` appeared. `pcsc_scan -r` could not establish a PC/SC context because the service is unavailable in this execution context.
- Result: The fixed ARM firmware is physically flashed and the board's built-in CHOUPI runtime test passes. Applet install and SELECT/APDU behavior remain unverified.
- Decision: Proceed with the exact JCAlgTest v1.8.2 installation when PC/SC access is available; do not infer applet installation from the firmware self-test.
- Commit: `eff90e5` (the following reference entry records this SHA)
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T070921Z.log` (SHA-256 `af02d162b8d631ac0a82da12f5e8e8cbbaa1a9a859d9bd3bdd727b7866ddb100`); UF2 SHA-256 `6bf2fbf36f860dd6952c02a1ea91c90a54c661ece40e5796a4310f00ef797844`.

## 2026-10-14 — CHOUPI hardware-validation commit reference

- Step/experiment: `physical-cycle-choupi-20261014-reference`
- Change: Recorded the immutable commit for the successful hardware cycle and its handoff update.
- Validation: Confirmed commit `eff90e5` contains only the cycle log, progress record, and session handoff.
- Result: Successful Pico 2 CHOUPI runtime validation is committed as `eff90e5`.
- Decision: Continue with JCAlgTest applet installation after PC/SC service access is restored.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T070921Z.log`.

## 2026-10-14 — Recheck JCAlgTest install transport after flash

- Step/experiment: `gp-install-transport-recheck-20261014`
- Change: Retried the authorized GlobalPlatformPro installation of the supplied JCAlgTest v1.8.2 CAP after the successful firmware cycle.
- Validation: Ran `timeout --signal=TERM --kill-after=2s 20s ./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar --debug --install AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`. GlobalPlatformPro reported CAP SHA-256 `38b464deabb20f48ba4fa1379354e41e913f635a537c9ea5cd2b1e919e0f908c` and exited with `SCARD_E_NO_SERVICE` (`SCardEstablishContext` response `0x8010001d`).
- Result: No INSTALL APDU reached the card; applet behavior remains unverified. This is a host PC/SC service failure, not an applet status-word result.
- Decision: Restore `pcscd.service` and rerun the Pico reader ACL helper for the current USB node before retrying the exact CAP install.
- Commit: `30f57f8` (the following reference entry records this SHA)
- Artifacts/logs: none; bounded command output is preserved in the session transcript.

## 2026-10-14 — PC/SC blocker commit reference

- Step/experiment: `gp-install-transport-recheck-20261014-reference`
- Change: Recorded the immutable commit containing the applet-install transport result and handoff update.
- Validation: Confirmed commit `30f57f8` contains only progress and handoff records.
- Result: The PC/SC service blocker is recorded as `30f57f8`.
- Decision: Continue with independent host-side build work while PC/SC is restored.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: none.

## 2026-10-14 — Verify offline CHOUPI host-build dependencies

- Step/experiment: `host-vm-offline-dependency-check-20261014`
- Change: Checked whether the host VM executable can be compiled from local dependencies while PC/SC is unavailable.
- Validation: `./tools/env cmake --build build/choupi-pc --target choupi -j2` completed the Java Card API compile and CAP conversions (zero converter errors) and built the capmap-free Maven modules, but Cargo retried crates.io because `index.crates.io` did not resolve. The explicit offline retry `CARGO_NET_OFFLINE=true MAVEN_ARGS=-o ./tools/env cmake --build build/choupi-pc --target choupi -j2` failed because `ipc-channel` is absent from the local Cargo registry. Both host build attempts exited nonzero before C++ linking.
- Result: Java source/CAP generation is available; host VM linking cannot proceed without the uncached Rust crate. The same offline build also reported an existing GNU make jobserver FIFO at `/tmp/GMfifo5`; no files outside the repository were changed.
- Decision: Keep hardware firmware builds and applet transport validation separate from the host VM build; retry host linking when the crate is available locally or Cargo network resolution is restored.
- Commit: `dea8f52` (the following reference entry records this SHA)
- Artifacts/logs: none; command output is preserved in the session transcript. Existing generated outputs are under `build/choupi-pc/`.

## 2026-10-14 — Offline host-build diagnostic commit reference

- Step/experiment: `host-vm-offline-dependency-check-20261014-reference`
- Change: Recorded the commit containing updated host-build and PC/SC status.
- Validation: Confirmed commit `dea8f52` includes only the dependency diagnostic, tracker, and handoff records.
- Result: Offline host-build dependency evidence is committed as `dea8f52`.
- Decision: Resume host VM linking after `ipc-channel` becomes available; resume applet installation after PC/SC is active.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: none.

## 2026-10-14 — Implement short APDU receive metadata

- Step/experiment: `short-apdu-receive-metadata-20261014`
- Change: Implemented `APDU.getIncomingLength()` through the native command-length bridge, returned `ISO7816.OFFSET_CDATA` for short APDUs, and documented `receiveBytes()` returning zero because the CCID path buffers the complete command before applet dispatch. Nested Java Card API commit: `5ef3f90356c437ab658fb79ced1abde59ba3df9a`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` passed both build gates, recovered BOOTSEL through CDC 1200-baud reset, flashed the ARM image, and captured 18 seconds of serial output with repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`; no `TEST:FAIL` or `FAULT` appeared. Bounded GPPro install was attempted both in the normal context and elevated context. The first reported `SCARD_E_NO_SERVICE`; the elevated attempt reported no available readers and was stopped at the 25-second bound. No INSTALL APDU reached the token.
- Result: Short-APDU receive metadata now matches the current fully buffered CCID path. Java-level behavior remains unverified because GPPro cannot see a reader in this execution context, despite the user's report that it is visible.
- Decision: Continue completing the Java Card APDU interface and retry applet installation when this execution context can enumerate the PC/SC reader.
- Commit: root documentation/log commit recorded in the following reference entry; API source commit `5ef3f90356c437ab658fb79ced1abde59ba3df9a`.
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T073306Z.log` SHA-256 `914f437056f50088dd27f2b066aa051d97728d8fc76b241d8e4e2c7f3bd746ff`; `build/arm/pico2_choupi.uf2` SHA-256 `c3958f3c3e9e0a35dfd42be552d4fc1183958d763afb63266cbc6a3f691b5680`.

## 2026-10-14 — Short APDU validation commit reference

- Step/experiment: `short-apdu-receive-metadata-20261014-reference`
- Change: Recorded the root commit that contains the short-APDU API validation, tracker/handoff updates, and bounded cycle log.
- Validation: Confirmed root commit `49f54ba76d5f6a907b015dce616833e33b2fc7a8` contains only `progress.md`, `TODO.md`, `.agent/HANDOFF.md`, and the cycle log; Java Card API source is separately committed as `5ef3f90356c437ab658fb79ced1abde59ba3df9a`.
- Result: Source and hardware evidence for this logical step are both committed.
- Decision: Continue with remaining APDU behavior and retry PC/SC installation when reader enumeration is available to GPPro.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T073306Z.log`.

## 2026-10-14 — Report APDU transport parameters

- Step/experiment: `apdu-transport-parameters-20261014`
- Change: Implemented `APDU.getInBlockSize()`, `getOutBlockSize()`, `getProtocol()`, and `getNAD()` for the current CCID configuration. They report T=1 default IFSC 32, descriptor maximum IFSD 254, protocol T=1, and unused NAD 0. Nested Java Card API commit: `6713938e9d7941aff9edb33a2ceecad72d355835`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` rebuilt both, flashed ARM through CDC BOOTSEL recovery, and completed with `CYCLE:PASS`; its serial capture contains repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`, with no `TEST:FAIL` or `FAULT`.
- Result: APDU transport metadata now matches the T=1 ATR and CCID descriptor; it has not yet been observed from the Java applet because GPPro cannot enumerate the reader in this execution context.
- Decision: Continue the v1.8.2 API coverage and retry the real GPPro install when the host reader is visible here.
- Commit: root documentation/log commit recorded in the following reference entry; API source commit `6713938e9d7941aff9edb33a2ceecad72d355835`.
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T075042Z.log` SHA-256 `9ecb112077c3df2eab59df9b53471d93699880cb882fc996c4a5354eea05853e`; `build/arm/pico2_choupi.uf2` SHA-256 `8900ed53725f5a69a5001138fa9f6f04a90fcdcffbef474a42960b70fdf315de`.

## 2026-10-14 — APDU transport validation commit reference

- Step/experiment: `apdu-transport-parameters-20261014-reference`
- Change: Recorded the root documentation/evidence commit for the APDU transport metadata change.
- Validation: Confirmed root commit `880153f79d9cb2cc2081b90a19defff18ae093f5` contains `progress.md`, `TODO.md`, `.agent/HANDOFF.md`, and the physical cycle log. The Java Card API source is committed separately as `6713938e9d7941aff9edb33a2ceecad72d355835`.
- Result: APDU source and physical validation evidence are committed.
- Decision: Proceed to the bounded monitor timeout hardening discovered during this cycle.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T075042Z.log`.

## 2026-10-14 — Bound serial monitor shutdown

- Step/experiment: `serial-monitor-timeout-watchdog-20261014`
- Change: Changed `tools/monitor` to send TERM to the serial `cat` process at the requested capture deadline and escalate to KILL after 2 seconds. Only timeout status 124 is treated as a successful finite capture; other command failures still propagate.
- Validation: `./tools/monitor --seconds 2` exited with status 0 on the flashed Pico 2 and captured repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. This is a tooling-only change; the immediately preceding APDU source step already passed both architecture builds and `./tools/cycle choupi`, so firmware builds/flash were not repeated for the monitor change.
- Result: The finite monitor now has a hard shutdown fallback and preserves non-timeout errors.
- Decision: Keep this bounded capture behavior for future board cycles. GPPro's latest install retry after reflashing timed out after 15 seconds with no reader or APDU output; installation remains unverified.
- Commit: root implementation/evidence commit recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/monitor-timeout-watchdog-20261014.txt` SHA-256 `f76159c0af3797b520f45557a25d929575b6b9de280486006f3d711a18932c07`.

## 2026-10-14 — Serial monitor hardening commit reference

- Step/experiment: `serial-monitor-timeout-watchdog-20261014-reference`
- Change: Recorded the root implementation/evidence commit for bounded serial-monitor shutdown.
- Validation: Confirmed root commit `dc73b1a5bd8575e1a9ebf718ab34d1012e84f0ad` contains `tools/monitor`, the 2-second capture log, `progress.md`, `TODO.md`, and `.agent/HANDOFF.md`.
- Result: Monitor timeout hardening and its physical serial evidence are committed.
- Decision: Continue implementing the remaining JCAlgTest v1.8.2 API surface while retaining bounded board capture and flash recovery.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/monitor-timeout-watchdog-20261014.txt`.

## 2026-10-14 — Bind APDU Java buffer to native response buffer

- Step/experiment: `apdu-java-buffer-identity-20261014`
- Change: `APDU.getBuffer()` caches the array returned by `NativeImplementation.getAPDUBuffer()` and passes that same Java array to the native context. Nested Java Card API commit: `0278fc9730e8915469ff35933c12549c39807831`.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed. `./tools/cycle choupi` passed both build gates but saw no RP-series device in the flash wrapper's sysfs polling window; `lsusb -d 2e8a:0009` and `lsusb -d 2e8a:000f` reported `unable to initialize libusb: -99`. The cycle was interrupted after over five minutes without device visibility, before firmware write. No serial or applet runtime output was captured.
- Result: The API and both firmware targets build. Applet response-buffer identity still needs an applet-level APDU test.
- Decision: Continue with stateful APDU output methods and short case-4 Le/response handling; retry physical validation when USB passthrough is visible to this VM.
- Commit: recorded in the following reference entry
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T081438Z.log` SHA-256 `94e0fb59a7ae1510e906299f616bac3ad500ccac68c42f405f477979f2d16caf`; `build/arm/pico2_choupi.uf2` SHA-256 `f59f4eec665d35ab678e652062ce6b4d8a135ea94a7aa643c52bdc21b0490b9c`.

## 2026-10-14 — APDU buffer validation record commit reference

- Step/experiment: `apdu-java-buffer-identity-20261014-reference`
- Change: Recorded the root documentation/evidence commit for the Java-visible APDU buffer identity change.
- Validation: Confirmed root commit `281aa75` contains the TODO and handoff updates, this validation record, and the bounded cycle log. Nested API source is committed separately as `0278fc9730e8915469ff35933c12549c39807831`.
- Result: Source and root build/USB evidence are tied to immutable commits.
- Decision: Continue implementation with APDU output state and then the unresolved Java Card API/native surface.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T081438Z.log`.

## 2026-10-14 — Implement generic primitive ArrayLogic operations

- Step/experiment: `arraylogic-primitive-types-20261014`
- Change: Extended `arrayFillGeneric` and its non-atomic variant to repeat one typed primitive value for byte, short, and int arrays; extended `arrayCompareGeneric` to those types with signed Java ordering; and extended `arrayFindGeneric` to decode byte, big-endian short, and big-endian int search values. Added regression cases to the existing ASan array-native harness.
- Validation: `./tools/test-jcarray` exited 0 with `TEST:PASS jni_array_find`, `TEST:PASS jni_array_fill_primitive_types`, and all existing array/transient checks. `./tools/build arm` and `./tools/build riscv` both passed. A post-change `./tools/cycle choupi` rebuilt both architectures but the Pico did not appear in USB; the wrapper remained in its re-enumeration wait and was stopped before any flash write. No serial output was captured. The focused test emits pre-existing packed-member and ignored-qualifier warnings from upstream headers.
- Result: Generic fill, compare, and search now cover the documented byte/short/int primitive array types in host regression coverage. Hardware behavior remains unverified.
- Decision: Continue filling the JCAlgTest API/native gaps and host integration coverage; retry physical cycle after the VM can see the Pico.
- Commit: recorded in the following reference entry
- Artifacts/logs: `artifacts/logs/jcarray-generic-20261014.log` SHA-256 `499791e60622ccaeaa24d5423f426b3aee1f34779d48703ddabe40301316c6cf`; `artifacts/logs/cycle-choupi-20261014T082552Z.log` SHA-256 `94e0fb59a7ae1510e906299f616bac3ad500ccac68c42f405f477979f2d16caf`; ARM UF2 SHA-256 `652fa4b42ad9a83c589367d1385a3663d9fce4c36275032232827013613f4f01`.

## 2026-10-14 — Generic ArrayLogic implementation commit reference

- Step/experiment: `arraylogic-primitive-types-20261014-reference`
- Change: Recorded the root source/test/evidence commit for generic byte/short/int array operations.
- Validation: Confirmed root commit `b64ff65` includes `choupi/src/jni.cpp`, the expanded ASan test, both logs, and tracker updates.
- Result: Implementation and evidence are committed together.
- Decision: Continue with the remaining JCAlgTest runtime and API gaps.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/jcarray-generic-20261014.log`.

## 2026-10-14 — Restore stable host VM build

- Step/experiment: `host-vm-stable-rust-and-cap-store-20261014`
- Change: Modernized the CHOUPI host Rust crate's obsolete feature gates, naked-function syntax, and inline assembly for the current stable compiler (nested `choupi/os` commit `dd074eb`). Added the PC build's explicit empty dynamic-CAP store bridge; only RP2350 hardware provides the reserved-flash CAP store.
- Validation: `CARGO_NET_OFFLINE=true MAVEN_ARGS=-o ./tools/env cmake --build build/choupi-pc --target choupi -j2` passed and linked the complete host VM. `build/choupi-pc/choupi --help` exited 0. `./tools/build arm` and `./tools/build riscv` both passed. `./tools/cycle choupi` passed both build gates but could not access USB (`libusb: -99`); no flash write or serial capture occurred.
- Result: The host VM can now be built and launched with the repository's current stable Rust toolchain. The build still emits legacy Rust/Maven warnings and two packed CAP metadata warnings. The PC target reports an empty dynamic CAP store; applet installation remains an RP2350-only path.
- Decision: Use the host VM as a build/runtime fixture for further API and APDU work. Retry physical installation after this VM can access the board and PC/SC reader.
- Commit: `54c41b8` (root tracker/evidence commit; nested Rust source commit `dd074eb`)
- Artifacts/logs: `artifacts/logs/host-build-after-capstore-bridge-20261014.log` SHA-256 `8631f51c835eea768b45b24c25744ce0c78784946e569bc0dc243ac05e759754`; ARM `artifacts/logs/arm-after-host-modernize-20261014.log` SHA-256 `deda16985d8ee0d23130e58ca46f100cbce0206bffa4328d9e5130072f6ec22e`; RISC-V `artifacts/logs/riscv-after-host-modernize-20261014.log` SHA-256 `3d71f0ea9ef4d6a17afdf2417f315f26ff5d802750163a165a583f3f63a8eb4f`; cycle `artifacts/logs/cycle-host-bridge-20261014.log` SHA-256 `72fea9335f221c1c430c1936e74fc597634f2e7f733e434ee9c38a808dae4416`; CLI smoke `artifacts/logs/host-vm-help-20261014.log`.

## 2026-10-14 — Host VM build commit reference

- Step/experiment: `host-vm-stable-rust-and-cap-store-20261014-reference`
- Change: Recorded the immutable root commit for the host build restoration; stable Rust changes are in nested commit `dd074eb`.
- Validation: Confirmed root commit `54c41b8` contains the PC CAP-store bridge, host/architecture build logs, CLI smoke log, `TODO.md`, `progress.md`, and `.agent/HANDOFF.md`.
- Result: Host build implementation and evidence are committed. The board cycle did not flash because this VM could not initialize USB.
- Decision: Continue with APDU state and JCAlgTest v1.8.2 API coverage; retain the host executable for fixture work.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/host-build-after-capstore-bridge-20261014.log`.

## 2026-10-14 — Parse short APDU cases and expose Le

- Step/experiment: `short-apdu-cases-and-le-20261014`
- Change: Added a bounds-checked parser for short APDU cases 1/2S/3S/4S. The applet bridge now distinguishes case-2 Le from case-3/4 Lc, preserves P3, rejects extended/malformed APDUs with `6700`, and records Le in `Context`. Java `APDU.setOutgoing()` and `setOutgoingNoChaining()` return that value; zero is interpreted as 256. Nested Java Card API commit: `86345d2`.
- Validation: `./tools/test-short-apdu` passed nine cases. The full PC VM build passed after regenerating Java API CAPs and the ROM mask. `./tools/build arm` and `./tools/build riscv` both passed. A bounded `./tools/cycle choupi` passed both build gates but the flash wrapper could not access USB (`libusb: -99`) and timed out; no firmware write or serial capture occurred.
- Result: Short APDU Lc/Le parsing is covered by a host unit test and the Le value reaches Java. Response-length enforcement, stateful `sendBytes*`, extended APDUs, and `61xx` chaining remain incomplete; no installed-applet APDU test has run.
- Decision: Continue with APDU outgoing state and response handling, then verify case-2/case-4 behavior through the PC fixture and physical board when USB and PC/SC are available.
- Commit: `d2dddd8` (root parser/native bridge; nested API source commit `86345d2`)
- Artifacts/logs: `artifacts/logs/short-apdu-tests-20261014.log` SHA-256 `f8f3316b2b0db669eff0d4fb26295d90bec2cff240187ee0f03e121d8c3dfc8d`; PC build `artifacts/logs/host-apdu-le-build-20261014.log` SHA-256 `e41fb21e782fb2eacf3e7d0f54678d57ee88c4ff8d251c15f3b7a5538c4dcde0`; ARM `artifacts/logs/arm-apdu-le-build-20261014.log` SHA-256 `909e69655d3be456748ae12546204097995417a657edc2ccea436d082214612f`; RISC-V `artifacts/logs/riscv-apdu-le-build-20261014.log` SHA-256 `b56cbbb6bc31d8e7857259a6f8acb43d062c95f1df89980dc9dcbf9aa509319c`; cycle `artifacts/logs/cycle-apdu-le-20261014.log` SHA-256 `55e3bfbc574ad53d926c98b529359156e89b146cde8bc9d867f0aee1452cf448`.

## 2026-10-14 — Short APDU parser commit reference

- Step/experiment: `short-apdu-cases-and-le-20261014-reference`
- Change: Recorded the immutable root commit for short APDU parsing and Le propagation; nested Java Card API commit is `86345d2`.
- Validation: Confirmed root commit `d2dddd8` includes the parser, context/native bridge, focused test, trackers, and build/cycle evidence.
- Result: Source and validation are committed. Physical flash remains blocked by USB passthrough visibility.
- Decision: Continue with response state and chaining semantics.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/short-apdu-tests-20261014.log`.

## 2026-10-14 — Add T=1 short response chaining

- Step/experiment: `short-apdu-response-chaining-20261014`
- Change: Added a bounded response-chain buffer (1022 bytes). When an applet returns more data than short Le, the bridge sends the first Le bytes with `61xx`, stores the remaining bytes, and serves them through case-2 `GET RESPONSE`; a new command, SELECT, or deselect clears the pending chain. Invalid GET RESPONSE P1/P2 returns `6A86`.
- Validation: `./tools/test-short-apdu` passed all 13 parser and response-chunk cases, including multi-chunk 600-byte data (`6100`, then `6158`, then `9000`). PC VM, ARM, and RISC-V builds passed. The bounded `./tools/cycle choupi` rebuilt both architectures but USB remained unavailable (`libusb: -99`); timeout occurred before flash or serial capture.
- Result: The core response splitting and continuation counter are unit-tested. Integration with applet APDU processing remains unverified; stateful `setOutgoingLength()`/`sendBytes*` are still missing.
- Decision: Continue with APDU send state and a host applet exchange fixture. Retry physical validation when USB passthrough is visible.
- Commit: `aba9440`
- Artifacts/logs: `artifacts/logs/short-apdu-chaining-20261014.log` SHA-256 `70c97607c39483d0edc941e44e4243c6ad24e57234eab5eefa21a6dd4cfa6d54`; PC build `artifacts/logs/host-apdu-chaining-20261014.log` SHA-256 `8b357f11a29f291f7ef03ec731cfa0b9975fe8aa1cb30413dfe208cbcecbb8c5`; ARM `artifacts/logs/arm-apdu-chaining-20261014.log` SHA-256 `5087f9b6529acca7dac2b4557e144b14de5a8836a4360a6e0bb65b1eaa816396`; RISC-V `artifacts/logs/riscv-apdu-chaining-20261014.log` SHA-256 `27d2453b9f281b7d4c8489bfd37cd71bab31c12d7c036f23ed6ec4c65f7e4a7d`; cycle `artifacts/logs/cycle-apdu-chaining-20261014.log` SHA-256 `2d2d62c15b5ade56f614436b61f786eb06441235b1e1089c54c48b776cd52b32`.

## 2026-10-14 — APDU response-chain commit reference

- Step/experiment: `short-apdu-response-chaining-20261014-reference`
- Change: Recorded the immutable root commit for bounded `61xx` and GET RESPONSE handling.
- Validation: Confirmed root commit `aba9440` includes the response buffer, `GET RESPONSE` processing, unit coverage, trackers, and build/cycle logs.
- Result: Implementation and validation record are committed; physical execution remains unverified because USB was unavailable.
- Decision: Continue with Java APDU outgoing state and host applet exchange fixtures.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/short-apdu-chaining-20261014.log`.

## 2026-10-14 — Implement APDU output state and chunk accumulation

- Step/experiment: `apdu-output-state-20261014`
- Change: Added per-command APDU transfer state to `Context`, including expected Le, declared output length, outgoing mode, and an accumulated 1022-byte response buffer. The Java APDU API now implements `setOutgoing()`, `setOutgoingNoChaining()`, `setOutgoingLength()`, `sendBytes()`, `sendBytesLong()`, `setOutgoingAndSend()`, `setIncomingAndReceive()`, and `getCurrentState()` using the native state bridge. Nested API commit: `5aef71e`.
- Validation: `./tools/test-short-apdu` passes parser, multi-chunk response, and Context-state cases (including illegal transitions, output accumulation, missing Le, and the 252-byte no-chaining bound). PC VM, ARM, and RISC-V builds passed. Elevated `./tools/cycle choupi` recovered BOOTSEL via CDC after picotool lacked a reset interface, flashed the ARM image, and captured repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`; no `TEST:FAIL` or `FAULT` appeared. The firmware runtime smoke check does not execute an installed applet. `pcsc_scan -r` reported `SCardEstablishContext: Service not available`, so no applet APDU was sent.
- Result: The APDU output state machine and response chunk storage compile and pass unit-level tests; real-board firmware boots successfully. Applet-level case-2/case-4 commands, response chaining, and Java exception mapping remain unverified. Extended APDUs remain unsupported.
- Decision: Next build a host applet exchange fixture or regain PC/SC, then exercise GET RESPONSE and continue the remaining JCAlgTest API gaps.
- Commit: `c4fa546` (root native/context/tests; nested Java Card API commit `5aef71e`)
- Artifacts/logs: unit tests `artifacts/logs/short-apdu-state-20261014.log` SHA-256 `a721816d1700d2e6c48c41cf50cc776e2417c32453bf469261a6a2d922336286`; PC build `artifacts/logs/host-apdu-state-20261014.log` SHA-256 `49b2cc55cada2d22f71ec79c20fa3947aa094f888fe01a7b8e75db8e682bfddf`; ARM `artifacts/logs/arm-apdu-state-20261014.log` SHA-256 `421ed022d97c52db4a5320d10f4fd3893d4819765c8f4fa078a80f8ed4ec7e96`; RISC-V `artifacts/logs/riscv-apdu-state-20261014.log` SHA-256 `95c7b2401f65fc5a28161892dd176d232b5bdd96db638842c86142dcdbcbe466`; physical cycle `artifacts/logs/cycle-apdu-state-20261014.log` SHA-256 `383330020642e57906a34b28115163f04a1a3c40de31ecaeb5fa209611a3088d`; UF2 SHA-256 `033ed8c9e1868434a0c801c47a0b6d1e03f652b62656f243521d8626118d97e1`.

## 2026-10-14 — APDU output-state commit reference

- Step/experiment: `apdu-output-state-20261014-reference`
- Change: Recorded the immutable root commit for APDU outgoing state and the nested API source commit.
- Validation: Confirmed root commit `c4fa546` contains Context/native state handling, host state tests, tracker updates, and PC/ARM/RISC-V/physical logs; nested API commit is `5aef71e`.
- Result: The implementation and real-board runtime smoke evidence are committed. Applet APDU behavior remains unverified.
- Decision: Continue with a host applet exchange fixture or available PC/SC path, then address more JCAlgTest API gaps.
- Commit: `c4fa546` (root Context/native/tests; nested API commit `5aef71e`)
- Artifacts/logs: `artifacts/logs/cycle-apdu-state-20261014.log`.

## 2026-10-14 — APDU output-state validation commit reference

- Step/experiment: `apdu-output-state-20261014-validation-reference`
- Change: Recorded the APDU output-state source and validation commit identifiers.
- Validation: Confirmed root commit `c4fa546` contains the native/context state implementation, unit tests, project trackers, and physical cycle logs; nested Java Card API implementation is commit `5aef71e`.
- Result: APDU state code compiles and firmware runtime passes on hardware; applet command behavior still requires end-to-end verification.
- Decision: Continue with host fixture/reader access and remaining runtime API work.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/cycle-apdu-state-20261014.log`.

## 2026-10-14 — Make doctor use the repository toolchain

- Step/experiment: `doctor-wrapper-toolchain-20261014`
- Change: Updated `tools/doctor` to resolve compiler/build commands and `PICO_SDK_PATH` through `tools/env`, while retaining PATH fallbacks for host tools.
- Validation: `./tools/doctor` exits 0 and reports all required local tools, the SDK import file, and CHOUPI sources present. It now reports `cmake`, Cargo, Rust, Maven, and picotool from `.local-toolchain` instead of incorrectly marking them missing.
- Result: The diagnostic agrees with the repository wrappers that were already building both architectures and the PC VM.
- Decision: Use the doctor output to distinguish missing local prerequisites from USB/PCSC availability.
- Commit: `300ee1b`
- Artifacts/logs: `artifacts/logs/doctor-wrapper-env-20261014.log` SHA-256 `daa8356548514a6469b72ab53611b48dac1ff510877b338ad534ce951d5ba91b`.

## 2026-10-14 — Doctor toolchain commit reference

- Step/experiment: `doctor-wrapper-toolchain-20261014-reference`
- Change: Recorded the immutable commit for the `tools/doctor` wrapper-environment fix.
- Validation: Confirmed root commit `300ee1b` contains `tools/doctor`, its successful diagnostic log, and the progress/handoff records.
- Result: The doctor now reports the same local toolchain that repository build wrappers use.
- Decision: Continue toward the golden applet APDU fixture and remaining JCAlgTest support.
- Commit: `recorded in the following reference commit`
- Artifacts/logs: `artifacts/logs/doctor-wrapper-env-20261014.log`.

## 2026-10-14 — Fix Java Card Util array comparison ordering

- Step/experiment: `util-array-compare-order-20261014`
- Change: Fixed `Util.arrayCompare()` so the first source byte greater than the corresponding destination byte returns `1`; the old final comparison repeated `<` and incorrectly returned `-1`. Nested Java Card API commit: `6dceb7e`.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed after regenerating the ROM mask. GlobalPlatformPro `--debug --list` connected to the now-visible Pico 2 over T=1 and listed ISD `A000000151000000` as `OP_READY`; package/applet list queries returned `6A88` because no package or applet is installed. The build logs contain pre-existing Java Card converter/source-level warnings; neither architecture build failed.
- Result: The Java API comparison now has the documented greater-than result. PC/SC and basic ISD listing work on this Linux host. Applet installation, runtime APDU dispatch, Windows enumeration, and the newly built firmware's hardware behavior remain unverified.
- Decision: Flash and capture the current ARM image now that both architecture builds pass, then retry JCAlgTest installation and investigate any returned status word from fresh traces.
- Commit: root documentation/evidence commit recorded in the following reference entry.
- Artifacts/logs: ARM `artifacts/logs/arm-util-array-compare-20261014.log` SHA-256 `16ddb401d4ddb2511f957407fd03ab717547cae58bb42eab10c795546a57146a`; RISC-V `artifacts/logs/riscv-util-array-compare-20261014.log` SHA-256 `f09a02c4542f463837c5e1745834cfd3e3e780f159f73a335f69a2dfc854ea65`; GPPro list `artifacts/logs/gp-list-current-20261014.log` SHA-256 `da115a4a917ad1a00b819f69bfc96b39cac2288e033528d9db9ea1defb10d0fb`.

## 2026-10-14 — Util comparison commit reference

- Step/experiment: `util-array-compare-order-20261014-reference`
- Change: Recorded the immutable root evidence commit for the Java Card `Util.arrayCompare()` ordering correction; the nested API source commit is `6dceb7e`.
- Validation: Confirmed root commit `b03142b` contains the updated trackers and ARM, RISC-V, and successful PC/SC list logs.
- Result: Root validation records and the source-level commit are linked for continuation.
- Decision: Flash the newly built ARM image and continue with JCAlgTest install validation.
- Commit: `b03142b`
- Artifacts/logs: `artifacts/logs/gp-list-current-20261014.log`.

## 2026-10-14 — Bound ARM build CPU use and check PC/SC APDU access

- Step/experiment: `arm-build-cpu-bound-smartcardio-probe-20261014`
- Change: `tools/build` now defaults CMake/Ninja to two parallel jobs, with an explicit positive-integer override through `PICO_BUILD_JOBS`. Updated `TODO.md` to record current install/SELECT evidence, ARM-only scope, and the process-level validation blocker.
- Validation: `./tools/build arm` passed and produced picotool metadata for both `pico2_trng.uf2` and `pico2_choupi.uf2`; the wrapper invoked `cmake --build ... --parallel 2`. Compiled a Java 8-compatible `javax.smartcardio` probe with `./tools/env javac`; execution found no readers because PC/SC is unavailable in this sandbox. The authorized sudo helper was blocked by the container's `no new privileges` setting. RISC-V was not built or tested.
- Result: Host USB/BOOTSEL polling is bounded and sleeps between checks; the idle firmware loop is on the Pico and cannot explain VM host CPU saturation. Ninja previously defaulted to all available VM cores; this is now limited to two. An unbounded `pcsc_scan -r` while pcscd is unavailable produced repeated error output, so scans must be run under `timeout`.
- Decision: Resume the applet `process()` APDU test when PC/SC becomes accessible to the execution environment; continue remaining plan work with ARM-only builds.
- Commit: recorded in a following reference entry.
- Artifacts/logs: `artifacts/logs/arm-build-limited-jobs-20261014.log` (SHA-256 `1d73bc85bc2b3dd9200eb452cb2b0b594508712dfb8696acde91191121a7260f`); `artifacts/logs/pico-apdu-probe-smartcardio-20261014.log` (SHA-256 `379f07f0162dd593fcc9f688b32aab69504ae37e87950d952caf2ca1d66a5f58`).

## 2026-10-14 — ARM build CPU bound commit reference

- Step/experiment: `arm-build-cpu-bound-smartcardio-probe-20261014-reference`
- Change: Recorded the immutable root commit for the two-job default ARM build limit and its validation records.
- Validation: Confirmed commit `678bd02` contains the `tools/build` limit, TODO/handoff updates, progress entry, ARM build log, and bounded Java Smart Card I/O probe log.
- Result: The ARM-only build resource fix is committed; RISC-V was not built or tested in this step.
- Decision: Continue with the remaining JCAlgTest applet `process()` and AC3/AC4 validation work.
- Commit: `678bd02`
- Artifacts/logs: `artifacts/logs/arm-build-limited-jobs-20261014.log`; `artifacts/logs/pico-apdu-probe-smartcardio-20261014.log`.

## 2026-10-14 — Produce JCAlgTest v1.8.2 jCardSim reference

- Step/experiment: `jcalgtest-v182-jcardsim-reference-20261014`
- Change: Retrieved the official v1.8.2 distribution into `build/downloads/`, extracted its fat host client under `build/algtest-client-1.8.2/`, and archived the completed extended supported-algorithms simulator CSV.
- Validation: Client `--help` exits 0 and exposes `-simulator`, `-op ALG_SUPPORT_EXTENDED`, and `-selftest`. `-simulator -op ALG_SUPPORT_BASIC ... -fresh` and `-simulator -op ALG_SUPPORT_EXTENDED ... -fresh` both completed with exit 0; the extended CSV has 4,595 lines. The distribution's JC305 CAP SHA-256 matches the supplied CAP exactly (`b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`). RISC-V and the physical board were not tested in this step.
- Result: The official client/JCardSim can provide a reference report. jCardSim logs `NotImplementedException` for some algorithms; the client records these as algorithm errors, so the simulator is a comparison input rather than a perfect oracle. The all-mode `-selftest` also starts CPU-heavy performance operations and hit its 180-second timeout after producing partial outputs; it is not counted as passed.
- Decision: Compare future CHOUPI `ALG_SUPPORT_EXTENDED` output to this reference and the VM's declared factory support; process-level CHOUPI output still awaits PC/SC access.
- Commit: recorded in a following reference entry.
- Artifacts/logs: `artifacts/results/jcardsim-jcalgtest-v1.8.2-alg-support-extended.csv` (SHA-256 `67d9feebf9756da69db034bab05cddd9acb38d31aa980888fa7b5db22ef1757b`); `artifacts/logs/jcalgtest-jcardsim-extended-20261014.log` (SHA-256 `9aef8813e7d4eb4430dff6eae28d349a8a47fd20d44b4771cb560985db6c830c`); client JAR SHA-256 `43d1d9c1cf92b8f5720c87ec0815e89ae5c713ec254713dcb24ffa431f4e6e90`.

## 2026-10-14 — Add bounded JCAlgTest applet APDU probe

- Step/experiment: `jc-algtest-apdu-probe-20261014`
- Change: Added `tools/probe-jc-algtest` and its Java Smart Card I/O source. The repository tool compiles the dependency-free probe with `tools/env`, has a configurable 30-second default timeout, selects `JCAlgTest1`, checks its exact `1.8.2_jc305` version response, and sends JCSystem info.
- Validation: `./tools/probe-jc-algtest` compiled successfully. The bounded runtime probe exited with `PC/SC reader Pico 2 CHOUPI CCID not found`; no card APDU was transmitted. `bash -n tools/probe-jc-algtest` and scoped `git diff --check` passed. No architecture build was needed for this host-only step; no RISC-V build/test was run.
- Result: No Python PySCard package or separate Java smartcard module needs installation; the host JDK supplies `javax.smartcardio`. A live, accessible PC/SC daemon/reader is required to continue hardware APDU validation.
- Decision: Run this probe after PC/SC and USB access return, then run the official v1.8.2 client on the board.
- Commit: recorded in a following reference entry.
- Artifacts/logs: `artifacts/logs/jc-algtest-apdu-probe-current-20261014.log` records the current no-reader condition.

## 2026-10-14 — JCAlgTest simulator reference commit reference

- Step/experiment: `jcalgtest-v182-jcardsim-reference-20261014-reference`
- Change: Recorded the immutable root commit for the official v1.8.2 client/JCardSim reference run and CSV archive.
- Validation: Confirmed commit `c33d1b6` contains the 4,595-line simulator CSV, focused extended-mode log, client help log, and updated tracker/handoff.
- Result: Simulator comparison input is version-matched to the supplied v1.8.2 JC305 CAP; physical CHOUPI comparison remains pending.
- Decision: Continue with process APDU validation and AC3/AC4.
- Commit: `c33d1b6`
- Artifacts/logs: `artifacts/results/jcardsim-jcalgtest-v1.8.2-alg-support-extended.csv`.

## 2026-10-14 — Bounded APDU probe commit reference

- Step/experiment: `jc-algtest-apdu-probe-20261014-reference`
- Change: Recorded the immutable root commit for the PC/SC APDU probe and its no-reader environment result.
- Validation: Confirmed commit `432eee3` contains the Java probe, executable wrapper, scoped TODO/handoff updates, and compile/runtime log.
- Result: The probe is ready to validate GETVERSION and JCSystem info once PC/SC exposes the Pico; current no-reader result does not represent an applet failure.
- Decision: Resume hardware APDU work when this execution environment can access the PC/SC reader.
- Commit: `432eee3`
- Artifacts/logs: `artifacts/logs/jc-algtest-apdu-probe-current-20261014.log` (SHA-256 `32f712cdd1346069e332db1ddaf71863947558414b34496b8e5e537f0716ffea`).

## 2026-10-14 — Implement transient Object[] factory

- Step/experiment: `jc-system-transient-object-array-20261014`
- Change: Implemented `JCSystem.makeTransientObjectArray()` with negative-length and clear-event validation, transient reference storage, zero-initialized Java-null entries, and a wildcard component marker. Updated firewall checks so the native-created `Object[]` accepts any reference without attempting to resolve a nonexistent CAP constant-pool component class. Added the factory to the focused native array test.
- Validation: `./tools/test-jcarray` passed with `TEST:PASS jni_transient_object_array` and all existing array/transient checks. `./tools/build arm` passed and generated the RP2350 ARM CHOUPI UF2. The focused host test emitted existing packed-member and ignored-qualifier warnings. No flash or physical APDU test was run; the updated image's applet behavior remains unverified. No RISC-V compile or test was run.
- Result: The previously unimplemented JCSystem Object[] transient factory now returns an array with the requested length and clear event. Applet-level assignment under the Java firewall still needs runtime validation.
- Decision: Keep this API implementation and continue the remaining JCAlgTest native methods; resume APDU validation when PC/SC is accessible to this execution environment.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `/tmp/pico2-jcarray-object-array.log` (SHA-256 `8f6b160efeb6bc7aa4ab962af6a7b2efb696d08f400a673e0f5375f103f51db3`); ARM UF2 SHA-256 `6a50c2ad5630a06951ecfcfca0491d57a9e161334b2eefd443a78932a1429b5e`.

## 2026-10-14 — Transient Object[] commit reference

- Step/experiment: `jc-system-transient-object-array-20261014-reference`
- Change: Recorded the immutable implementation commit for the `JCSystem.makeTransientObjectArray()` support and its focused validation.
- Validation: Confirmed commit `9942d57` contains the native factory, Object[] wildcard handling, the focused test, and the TODO/handoff updates. The ARM build and `./tools/test-jcarray` passed before commit.
- Result: The transient Object[] implementation is committed as `9942d57`; physical applet behavior remains unverified.
- Decision: Continue with the remaining JCAlgTest native API surface and retry PC/SC integration when this execution environment can reach the host service.
- Commit: `cc13ec4`
- Artifacts/logs: `/tmp/pico2-jcarray-object-array.log`; ARM UF2 `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Retry PC/SC access after VM helper run

- Step/experiment: `pcsc-access-retry-20261014`
- Change: Retried the bounded JCAlgTest APDU probe after the user reported the VM PC/SC helper ran successfully.
- Validation: Normal `./tools/probe-jc-algtest` exited 1 because reader `Pico 2 CHOUPI CCID` was not found. Retrying with elevated device access reached Java Smart Card I/O but returned `SCARD_E_NO_READERS_AVAILABLE`. `systemctl is-active pcscd.service` could not connect to the system bus; `lsusb -d 2e8a:0009` returned `libusb: -99`. No APDU was sent. No RISC-V build or test was run.
- Result: Host PC/SC setup may be active in the user's VM, but the isolated agent process cannot see its system bus, USB device, or reader.
- Decision: Continue implementation and ARM-only validation locally; retry APDU testing when the reader is exposed to this execution environment.
- Commit: recorded in the following reference entry.
- Artifacts/logs: no repository log; probe and access errors were captured in the tool output for this turn.

## 2026-10-14 — PC/SC access retry commit reference

- Step/experiment: `pcsc-access-retry-20261014-reference`
- Change: Recorded the immutable commit for the PC/SC visibility retry and updated execution-environment handoff.
- Validation: Confirmed commit `99b0eaf` contains the bounded probe result, TODO update, and handoff evidence. The elevated probe returned `SCARD_E_NO_READERS_AVAILABLE`; no APDU was sent.
- Result: The reader access limitation is recorded without treating a successful VM helper run as proof of agent-container access.
- Decision: Continue with implementation that can be built and tested locally.
- Commit: `99b0eaf`
- Artifacts/logs: no repository log; exact output is summarized in `progress.md` above.

## 2026-10-14 — Define previous-context AID fallback

- Step/experiment: `jc-system-previous-context-aid-fallback-20261014`
- Change: Implemented `JCSystem.getPreviousContextAID()` as Java null because the current runtime has no cross-applet/shareable-interface context switch from which to obtain a previous applet AID. Added a focused native assertion.
- Validation: `./tools/test-jcarray` passed with `TEST:PASS jni_previous_context_aid_fallback` and all existing checks. `./tools/build arm` passed and generated the RP2350 ARM CHOUPI UF2. No flash or physical APDU test was run; no RISC-V build or test was run.
- Result: The unsupported cross-applet query now returns the documented null fallback instead of throwing `NotYetImplemented`.
- Decision: Retain this limited behavior and continue the remaining JCAlgTest Java Card APIs; applet-level runtime validation remains pending.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `/tmp/pico2-previous-context-aid.log` (SHA-256 `4a68dfe0966deaee6ba12de8a08a83bef4730f3c01d7b9f53f7b0d82c7610e02`); ARM UF2 SHA-256 `1b5231c74e3b7c3a38ce63a20b696aa0a6e16749ffadd4862ecb7f38fb3d5738`.

## 2026-10-14 — Previous-context AID commit reference

- Step/experiment: `jc-system-previous-context-aid-fallback-20261014-reference`
- Change: Recorded the implementation commit for the no-shareable-context AID fallback.
- Validation: Confirmed commit `ca0520a` contains the native behavior, focused assertion, ARM build evidence, and TODO/handoff updates.
- Result: The fallback is committed as `ca0520a`; Java lifecycle behavior on an installed applet remains unverified.
- Decision: Continue with the remaining platform API and lifecycle work.
- Commit: `ca0520a`
- Artifacts/logs: `/tmp/pico2-previous-context-aid.log`; ARM UF2 `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Previous-context AID commit hash reference

- Step/experiment: `jc-system-previous-context-aid-fallback-20261014-hash-reference`
- Change: Recorded the immutable reference for the `getPreviousContextAID()` fallback implementation.
- Validation: Confirmed commit `ca0520a` contains the native null result, focused test, ARM build record, and TODO/handoff update.
- Result: The API fallback and its evidence are linked to commit `ca0520a`.
- Decision: Continue the remaining API and applet lifecycle work.
- Commit: `ca0520a`
- Artifacts/logs: `/tmp/pico2-previous-context-aid.log`.

## 2026-10-14 — Enforce Le for no-chaining APDU responses

- Step/experiment: `apdu-no-chaining-le-bound-20261014`
- Change: Tightened `Context::setAPDUOutgoingLength()` so `setOutgoingNoChaining()` rejects a response length above either the command's Le or the T=1 maximum payload of IFSD minus two status bytes (252 bytes). Expanded the APDU context tests for both limits.
- Validation: `./tools/test-short-apdu` passed parser, GET RESPONSE chaining, and APDU context cases, including `no_chaining_respects_le` and `no_chaining_t1_frame_bound`. `./tools/build arm` passed and generated the RP2350 ARM CHOUPI UF2. No flash or physical APDU test was run; no RISC-V compile or test was run.
- Result: No-chaining output can no longer be accepted above Le and later incorrectly converted into a `61xx` response chain.
- Decision: Keep the validation and continue remaining applet/runtime integration work; installed-applet behavior still awaits PC/SC access.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `/tmp/pico2-apdu-nochaining.log` (SHA-256 `5df9fc50c8120e1abbef839dea5d0f5fbeef393c06b6773af97154159d6a645a`); ARM UF2 SHA-256 `81fd209c5245c0c4f7977f561f0f3926575beb9168dff54113b380aad5163f03`.

## 2026-10-14 — No-chaining APDU commit reference

- Step/experiment: `apdu-no-chaining-le-bound-20261014-reference`
- Change: Recorded the implementation commit for no-chaining response limits.
- Validation: Confirmed commit `7448bec` contains the Le/frame bounds, focused APDU tests, and project tracker updates. `./tools/test-short-apdu` and `./tools/build arm` passed.
- Result: No-chaining response validation is committed as `7448bec`; physical applet behavior remains unverified.
- Decision: Continue with remaining APDU and JCAlgTest runtime work.
- Commit: `7448bec`
- Artifacts/logs: `/tmp/pico2-apdu-nochaining.log`; ARM UF2 `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — No-chaining APDU implementation hash reference

- Step/experiment: `apdu-no-chaining-le-bound-20261014-hash-reference`
- Change: Recorded the immutable commit containing the Le and T=1 no-chaining response bounds.
- Validation: Confirmed commit `7448bec` contains the `Context` bound, expanded APDU context tests, and TODO/handoff updates; the test wrapper and ARM build passed.
- Result: The APDU no-chaining fix is linked to commit `7448bec`; board-level APDU behavior remains pending.
- Decision: Continue implementing and validating the remaining applet transport behavior.
- Commit: `7448bec`
- Artifacts/logs: `/tmp/pico2-apdu-nochaining.log`; ARM UF2 `build/arm/pico2_choupi.uf2`.
